#include <cuexis/shader/shader_cache.hpp>

#include <cuexis/core/error.hpp>
#include <cuexis_internal/sha256.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <exception>
#include <fstream>
#include <new>
#include <span>
#include <string>
#include <utility>

namespace cuexis::shader {
namespace {

constexpr std::size_t envelopeByteCount = 24;
constexpr std::size_t identityByteCount = 32;
constexpr std::uint32_t maxCountedBytes = 1048576;
constexpr std::uint32_t maxProfileBytes = 64;
constexpr std::uint32_t maxToolNameBytes = 32;
constexpr std::uint32_t maxToolVersionBytes = 64;
constexpr std::uint32_t maxCacheKeywords = 4;
constexpr std::uint32_t maxCacheTargets = 8;
constexpr std::uint32_t maxCacheTools = 8;

#ifndef CUEXIS_SHADERC_VERSION
#define CUEXIS_SHADERC_VERSION "2026.2"
#endif
#ifndef CUEXIS_GLSLANG_VERSION
#define CUEXIS_GLSLANG_VERSION "16.4.0"
#endif
#ifndef CUEXIS_SPIRV_TOOLS_VERSION
#define CUEXIS_SPIRV_TOOLS_VERSION "1.4.350.1"
#endif
#ifndef CUEXIS_SPIRV_CROSS_VERSION
#define CUEXIS_SPIRV_CROSS_VERSION "1.4.350.1"
#endif

[[nodiscard]] auto cacheError(std::string_view code, std::string message,
                              std::string_view field = {}) -> core::Error {
    auto error = core::Error{std::string{code}, std::move(message)};
    if (!field.empty()) {
        error.withContext("byte_offset", std::string{field});
    }
    return error;
}

void appendU32(std::vector<std::byte>& out, std::uint32_t value) {
    out.push_back(static_cast<std::byte>(value & 0xFFu));
    out.push_back(static_cast<std::byte>((value >> 8) & 0xFFu));
    out.push_back(static_cast<std::byte>((value >> 16) & 0xFFu));
    out.push_back(static_cast<std::byte>((value >> 24) & 0xFFu));
}

void appendU64(std::vector<std::byte>& out, std::uint64_t value) {
    for (int shift = 0; shift < 64; shift += 8) {
        out.push_back(static_cast<std::byte>((value >> shift) & 0xFFu));
    }
}

void appendBytes(std::vector<std::byte>& out, std::span<const std::byte> bytes) {
    out.insert(out.end(), bytes.begin(), bytes.end());
}

void appendBytes(std::vector<std::byte>& out, std::string_view text) {
    const auto* bytes = reinterpret_cast<const std::byte*>(text.data());
    out.insert(out.end(), bytes, bytes + text.size());
}

void appendCounted(std::vector<std::byte>& out, std::string_view text) {
    appendU32(out, static_cast<std::uint32_t>(text.size()));
    appendBytes(out, text);
}

void appendCounted(std::vector<std::byte>& out, std::span<const std::byte> bytes) {
    appendU32(out, static_cast<std::uint32_t>(bytes.size()));
    appendBytes(out, bytes);
}

void hashU32(core::detail::Sha256& hash, std::uint32_t value) noexcept {
    const std::byte bytes[4] = {
        static_cast<std::byte>(value & 0xFFu),
        static_cast<std::byte>((value >> 8) & 0xFFu),
        static_cast<std::byte>((value >> 16) & 0xFFu),
        static_cast<std::byte>((value >> 24) & 0xFFu),
    };
    hash.update(bytes);
}

void hashPrefixed(core::detail::Sha256& hash, std::span<const std::byte> bytes) noexcept {
    hashU32(hash, static_cast<std::uint32_t>(bytes.size()));
    if (!bytes.empty()) {
        hash.update(bytes);
    }
}

void hashPrefixed(core::detail::Sha256& hash, std::string_view text) noexcept {
    hashU32(hash, static_cast<std::uint32_t>(text.size()));
    if (text.empty()) {
        return;
    }
    hash.update(std::as_bytes(std::span<const char>{text.data(), text.size()}));
}

[[nodiscard]] auto sortedUnique(std::span<const std::string_view> values)
    -> std::vector<std::string> {
    std::vector<std::string> items;
    items.reserve(values.size());
    for (const auto value : values) {
        items.emplace_back(value);
    }
    std::sort(items.begin(), items.end());
    items.erase(std::unique(items.begin(), items.end()), items.end());
    return items;
}

[[nodiscard]] auto sortedTools(std::span<const ShaderToolVersion> tools)
    -> std::vector<ShaderToolVersion> {
    std::vector<ShaderToolVersion> items{tools.begin(), tools.end()};
    std::sort(items.begin(), items.end(),
              [](const ShaderToolVersion& left, const ShaderToolVersion& right) {
                  if (left.name == right.name) {
                      return left.version < right.version;
                  }
                  return left.name < right.name;
              });
    return items;
}

struct ByteReader final {
    std::span<const std::byte> bytes;
    std::size_t offset{};

    [[nodiscard]] auto remaining() const noexcept -> std::size_t {
        return bytes.size() - offset;
    }

    [[nodiscard]] auto fail(std::string message) const -> core::Error {
        return cacheError(diagnosticCacheKeyInvalid, std::move(message), std::to_string(offset));
    }

    [[nodiscard]] auto readU32() -> core::Result<std::uint32_t> {
        if (remaining() < 4) {
            return core::unexpected(fail("CXSCCH01 field is truncated"));
        }
        const auto value = static_cast<std::uint32_t>(bytes[offset]) |
                           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8U) |
                           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16U) |
                           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24U);
        offset += 4;
        return value;
    }

    [[nodiscard]] auto readU64() -> core::Result<std::uint64_t> {
        if (remaining() < 8) {
            return core::unexpected(fail("CXSCCH01 field is truncated"));
        }
        std::uint64_t value = 0;
        for (int index = 0; index < 8; ++index) {
            value |= static_cast<std::uint64_t>(bytes[offset + static_cast<std::size_t>(index)])
                     << (index * 8);
        }
        offset += 8;
        return value;
    }

    [[nodiscard]] auto readBytes(std::uint32_t count) -> core::Result<std::vector<std::byte>> {
        if (count > maxCountedBytes || remaining() < count) {
            return core::unexpected(fail("CXSCCH01 counted field is truncated or too large"));
        }
        std::vector<std::byte> out(count);
        if (count > 0) {
            std::memcpy(out.data(), bytes.data() + offset, count);
        }
        offset += count;
        return out;
    }

    [[nodiscard]] auto readCountedBytes(std::uint32_t maxBytes)
        -> core::Result<std::vector<std::byte>> {
        const auto count = readU32();
        if (!count) {
            return core::unexpected(std::move(count.error()));
        }
        if (*count > maxBytes) {
            return core::unexpected(fail("CXSCCH01 counted field exceeds the v1 limit"));
        }
        return readBytes(*count);
    }

    [[nodiscard]] auto readCountedString(std::uint32_t maxBytes) -> core::Result<std::string> {
        auto bytesValue = readCountedBytes(maxBytes);
        if (!bytesValue) {
            return core::unexpected(std::move(bytesValue.error()));
        }
        return std::string{reinterpret_cast<const char*>(bytesValue->data()), bytesValue->size()};
    }

    [[nodiscard]] auto readIdentity() -> core::Result<std::array<std::uint8_t, 32>> {
        auto bytesValue = readBytes(identityByteCount);
        if (!bytesValue) {
            return core::unexpected(std::move(bytesValue.error()));
        }
        std::array<std::uint8_t, 32> identity{};
        std::memcpy(identity.data(), bytesValue->data(), identityByteCount);
        return identity;
    }
};

} // namespace

auto defaultTargetProfiles() -> std::array<std::string_view, 3> {
    return {targetProfileGlsl330V1, targetProfileGlslEs300V1, targetProfileSpirvV1};
}

auto currentToolVersions() -> std::vector<ShaderToolVersion> {
    std::vector<ShaderToolVersion> tools{
        {std::string{toolGlslang}, CUEXIS_GLSLANG_VERSION},
        {std::string{toolShaderc}, CUEXIS_SHADERC_VERSION},
        {std::string{toolSpirvCross}, CUEXIS_SPIRV_CROSS_VERSION},
        {std::string{toolSpirvTools}, CUEXIS_SPIRV_TOOLS_VERSION},
    };
    return sortedTools(tools);
}

auto encodeCacheKey(const ShaderCacheKeyInput& input) -> std::array<std::uint8_t, 32> {
    const auto targets = input.targetProfiles.empty() ? sortedUnique(defaultTargetProfiles())
                                                      : sortedUnique(input.targetProfiles);
    const auto keywords = sortedUnique(input.selectedKeywords);
    const auto tools = input.tools.empty() ? currentToolVersions() : sortedTools(input.tools);
    const auto importer =
        input.importerProfile.empty() ? importerProfileShaderV1 : input.importerProfile;
    const auto vertexEntry =
        input.vertexEntry.empty() ? std::string_view{"main"} : input.vertexEntry;
    const auto fragmentEntry =
        input.fragmentEntry.empty() ? std::string_view{"main"} : input.fragmentEntry;

    core::detail::Sha256 hash;
    std::array<char, cacheDomain.size() + 1> domain{};
    std::memcpy(domain.data(), cacheDomain.data(), cacheDomain.size());
    hashPrefixed(hash, std::string_view{domain.data(), domain.size()});
    hashPrefixed(hash, std::as_bytes(std::span{input.sourceIdentity}));
    hashPrefixed(hash, importer);
    hashU32(hash, static_cast<std::uint32_t>(targets.size()));
    for (const auto& target : targets) {
        hashPrefixed(hash, target);
    }
    hashU32(hash, static_cast<std::uint32_t>(keywords.size()));
    for (const auto& keyword : keywords) {
        hashPrefixed(hash, keyword);
    }
    hashPrefixed(hash, vertexEntry);
    hashPrefixed(hash, fragmentEntry);
    hashU32(hash, static_cast<std::uint32_t>(tools.size()));
    for (const auto& tool : tools) {
        hashPrefixed(hash, tool.name);
        hashPrefixed(hash, tool.version);
    }
    return hash.finish();
}

auto cacheFileName(const std::array<std::uint8_t, 32>& key) -> std::string {
    return core::detail::sha256Hex(key) + std::string{cacheFileExtension};
}

auto hashStandaloneSourceIdentity(std::string_view vertexSource, std::string_view fragmentSource,
                                  std::string_view vertexEntry, std::string_view fragmentEntry,
                                  std::span<const std::string_view> selectedKeywords)
    -> std::array<std::uint8_t, 32> {
    const auto keywords = sortedUnique(selectedKeywords);
    core::detail::Sha256 hash;
    hashPrefixed(hash, vertexSource);
    hashPrefixed(hash, fragmentSource);
    hashPrefixed(hash, vertexEntry);
    hashPrefixed(hash, fragmentEntry);
    hashU32(hash, static_cast<std::uint32_t>(keywords.size()));
    for (const auto& keyword : keywords) {
        hashPrefixed(hash, keyword);
    }
    return hash.finish();
}

auto encodeCache(const ShaderCacheRecord& record) -> core::Result<std::vector<std::byte>> {
    try {
        auto tools = record.tools.empty() ? currentToolVersions() : sortedTools(record.tools);
        auto targets = record.targetProfiles;
        if (targets.empty()) {
            for (const auto profile : defaultTargetProfiles()) {
                targets.emplace_back(profile);
            }
        }
        std::sort(targets.begin(), targets.end());
        targets.erase(std::unique(targets.begin(), targets.end()), targets.end());
        auto keywords = record.selectedKeywords;
        std::sort(keywords.begin(), keywords.end());
        keywords.erase(std::unique(keywords.begin(), keywords.end()), keywords.end());

        const auto importer = record.importerProfile.empty() ? std::string{importerProfileShaderV1}
                                                             : record.importerProfile;
        const auto vertexEntry =
            record.vertexEntry.empty() ? std::string{"main"} : record.vertexEntry;
        const auto fragmentEntry =
            record.fragmentEntry.empty() ? std::string{"main"} : record.fragmentEntry;

        std::vector<std::string_view> targetViews;
        targetViews.reserve(targets.size());
        for (const auto& target : targets) {
            targetViews.emplace_back(target);
        }
        std::vector<std::string_view> keywordViews;
        keywordViews.reserve(keywords.size());
        for (const auto& keyword : keywords) {
            keywordViews.emplace_back(keyword);
        }

        const ShaderCacheKeyInput keyInput{
            .sourceIdentity = record.sourceIdentity,
            .importerProfile = importer,
            .targetProfiles = targetViews,
            .selectedKeywords = keywordViews,
            .vertexEntry = vertexEntry,
            .fragmentEntry = fragmentEntry,
            .tools = tools,
        };
        const auto key = encodeCacheKey(keyInput);
        const auto reflection = encodeCanonicalReflection(record.artifact.reflection);

        std::vector<std::byte> body;
        appendBytes(body, std::as_bytes(std::span{record.sourceIdentity}));
        appendCounted(body, importer);
        appendU32(body, static_cast<std::uint32_t>(targets.size()));
        for (const auto& target : targets) {
            appendCounted(body, target);
        }
        appendU32(body, static_cast<std::uint32_t>(keywords.size()));
        for (const auto& keyword : keywords) {
            appendCounted(body, keyword);
        }
        appendCounted(body, vertexEntry);
        appendCounted(body, fragmentEntry);
        appendU32(body, static_cast<std::uint32_t>(tools.size()));
        for (const auto& tool : tools) {
            appendCounted(body, tool.name);
            appendCounted(body, tool.version);
        }
        appendCounted(body, std::span<const std::byte>{record.artifact.vertexSpirv});
        appendCounted(body, std::span<const std::byte>{record.artifact.fragmentSpirv});
        appendCounted(body, record.artifact.vertexGlsl330);
        appendCounted(body, record.artifact.fragmentGlsl330);
        appendCounted(body, record.artifact.vertexGlslEs300);
        appendCounted(body, record.artifact.fragmentGlslEs300);
        appendCounted(body, std::span<const std::byte>{reflection});
        appendBytes(body, std::as_bytes(std::span{key}));

        std::vector<std::byte> bytes;
        appendBytes(bytes, cacheMagic);
        appendU32(bytes, cacheVersionV1);
        appendU32(bytes, 0);
        appendU64(bytes, static_cast<std::uint64_t>(envelopeByteCount + body.size()));
        appendBytes(bytes, std::span<const std::byte>{body});
        return bytes;
    } catch (const std::bad_alloc&) {
        return core::unexpected(
            cacheError(diagnosticLimitExceeded, "CXSCCH01 encode could not allocate"));
    }
}

auto decodeCache(std::span<const std::byte> bytes) -> core::Result<ShaderCacheRecord> {
    try {
        if (bytes.size() < envelopeByteCount) {
            return core::unexpected(
                cacheError(diagnosticCacheKeyInvalid, "CXSCCH01 envelope is truncated", "0"));
        }
        ByteReader reader{.bytes = bytes};
        if (std::string_view{reinterpret_cast<const char*>(bytes.data()), cacheMagic.size()} !=
            cacheMagic) {
            return core::unexpected(
                cacheError(diagnosticCacheKeyInvalid, "CXSCCH01 magic is invalid", "0"));
        }
        reader.offset = cacheMagic.size();
        const auto version = reader.readU32();
        const auto reserved = reader.readU32();
        const auto total = reader.readU64();
        if (!version || !reserved || !total) {
            return core::unexpected(
                cacheError(diagnosticCacheKeyInvalid, "CXSCCH01 envelope is truncated", "8"));
        }
        if (*version != cacheVersionV1) {
            return core::unexpected(cacheError(diagnosticCacheKeyInvalid,
                                               "CXSCCH01 cache version is unsupported", "8"));
        }
        if (*reserved != 0) {
            return core::unexpected(
                cacheError(diagnosticCacheKeyInvalid, "CXSCCH01 reserved field is non-zero", "12"));
        }
        if (*total != bytes.size()) {
            return core::unexpected(cacheError(diagnosticCacheKeyInvalid,
                                               "CXSCCH01 total byte count does not match", "16"));
        }

        ShaderCacheRecord record;
        auto identity = reader.readIdentity();
        if (!identity) {
            return core::unexpected(std::move(identity.error()));
        }
        record.sourceIdentity = *identity;
        auto importer = reader.readCountedString(maxProfileBytes);
        if (!importer) {
            return core::unexpected(std::move(importer.error()));
        }
        record.importerProfile = std::move(*importer);

        const auto targetCount = reader.readU32();
        if (!targetCount) {
            return core::unexpected(std::move(targetCount.error()));
        }
        if (*targetCount > maxCacheTargets) {
            return core::unexpected(
                reader.fail("CXSCCH01 target profile count exceeds the v1 limit"));
        }
        record.targetProfiles.reserve(*targetCount);
        for (std::uint32_t index = 0; index < *targetCount; ++index) {
            auto target = reader.readCountedString(maxProfileBytes);
            if (!target) {
                return core::unexpected(std::move(target.error()));
            }
            record.targetProfiles.push_back(std::move(*target));
        }

        const auto keywordCount = reader.readU32();
        if (!keywordCount) {
            return core::unexpected(std::move(keywordCount.error()));
        }
        if (*keywordCount > maxCacheKeywords) {
            return core::unexpected(reader.fail("CXSCCH01 keyword count exceeds the v1 limit"));
        }
        record.selectedKeywords.reserve(*keywordCount);
        for (std::uint32_t index = 0; index < *keywordCount; ++index) {
            auto keyword = reader.readCountedString(32);
            if (!keyword) {
                return core::unexpected(std::move(keyword.error()));
            }
            record.selectedKeywords.push_back(std::move(*keyword));
        }

        auto vertexEntry = reader.readCountedString(64);
        auto fragmentEntry = reader.readCountedString(64);
        if (!vertexEntry) {
            return core::unexpected(std::move(vertexEntry.error()));
        }
        if (!fragmentEntry) {
            return core::unexpected(std::move(fragmentEntry.error()));
        }
        record.vertexEntry = std::move(*vertexEntry);
        record.fragmentEntry = std::move(*fragmentEntry);

        const auto toolCount = reader.readU32();
        if (!toolCount) {
            return core::unexpected(std::move(toolCount.error()));
        }
        if (*toolCount > maxCacheTools) {
            return core::unexpected(reader.fail("CXSCCH01 tool count exceeds the v1 limit"));
        }
        record.tools.reserve(*toolCount);
        for (std::uint32_t index = 0; index < *toolCount; ++index) {
            auto name = reader.readCountedString(maxToolNameBytes);
            auto versionText = reader.readCountedString(maxToolVersionBytes);
            if (!name) {
                return core::unexpected(std::move(name.error()));
            }
            if (!versionText) {
                return core::unexpected(std::move(versionText.error()));
            }
            record.tools.push_back(ShaderToolVersion{std::move(*name), std::move(*versionText)});
        }

        auto vertexSpirv = reader.readCountedBytes(maxCountedBytes);
        auto fragmentSpirv = reader.readCountedBytes(maxCountedBytes);
        auto vertexGlsl330 = reader.readCountedString(maxCountedBytes);
        auto fragmentGlsl330 = reader.readCountedString(maxCountedBytes);
        auto vertexGlslEs300 = reader.readCountedString(maxCountedBytes);
        auto fragmentGlslEs300 = reader.readCountedString(maxCountedBytes);
        auto reflectionBytes = reader.readCountedBytes(maxCountedBytes);
        if (!vertexSpirv || !fragmentSpirv || !vertexGlsl330 || !fragmentGlsl330 ||
            !vertexGlslEs300 || !fragmentGlslEs300 || !reflectionBytes) {
            return core::unexpected(cacheError(diagnosticCacheKeyInvalid,
                                               "CXSCCH01 artifact payload is truncated",
                                               std::to_string(reader.offset)));
        }
        auto reflection = decodeCanonicalReflection(*reflectionBytes);
        if (!reflection) {
            return core::unexpected(cacheError(diagnosticCacheKeyInvalid,
                                               "CXSCCH01 reflection blob is invalid",
                                               std::to_string(reader.offset))
                                        .withCause(std::move(reflection.error())));
        }
        auto key = reader.readIdentity();
        if (!key) {
            return core::unexpected(std::move(key.error()));
        }
        if (reader.remaining() != 0) {
            return core::unexpected(reader.fail("CXSCCH01 has trailing bytes"));
        }

        record.artifact.vertexSpirv = std::move(*vertexSpirv);
        record.artifact.fragmentSpirv = std::move(*fragmentSpirv);
        record.artifact.vertexGlsl330 = std::move(*vertexGlsl330);
        record.artifact.fragmentGlsl330 = std::move(*fragmentGlsl330);
        record.artifact.vertexGlslEs300 = std::move(*vertexGlslEs300);
        record.artifact.fragmentGlslEs300 = std::move(*fragmentGlslEs300);
        record.artifact.reflection = std::move(*reflection);
        record.key = *key;

        std::vector<std::string_view> targetViews;
        targetViews.reserve(record.targetProfiles.size());
        for (const auto& target : record.targetProfiles) {
            targetViews.emplace_back(target);
        }
        std::vector<std::string_view> keywordViews;
        keywordViews.reserve(record.selectedKeywords.size());
        for (const auto& keyword : record.selectedKeywords) {
            keywordViews.emplace_back(keyword);
        }
        const ShaderCacheKeyInput keyInput{
            .sourceIdentity = record.sourceIdentity,
            .importerProfile = record.importerProfile,
            .targetProfiles = targetViews,
            .selectedKeywords = keywordViews,
            .vertexEntry = record.vertexEntry,
            .fragmentEntry = record.fragmentEntry,
            .tools = record.tools,
        };
        if (encodeCacheKey(keyInput) != record.key) {
            return core::unexpected(cacheError(
                diagnosticCacheKeyInvalid, "CXSCCH01 stored key does not match payload fields"));
        }
        return record;
    } catch (const std::bad_alloc&) {
        return core::unexpected(
            cacheError(diagnosticLimitExceeded, "CXSCCH01 decode could not allocate"));
    }
}

auto adoptCacheRecord(const ShaderCacheRecord& record) -> core::Result<ShaderCacheRecord> {
    if (record.tools != currentToolVersions()) {
        return core::unexpected(
            cacheError(diagnosticCacheToolMismatch,
                       "CXSCCH01 tool versions do not match the current importer"));
    }
    return record;
}

auto makeShaderDiagnostics() -> core::Diagnostics {
    return core::Diagnostics{
        maxDiagnostics,
        core::Diagnostic{core::DiagnosticSeverity::Error, std::string{diagnosticLimitExceeded},
                         "Shader diagnostics reached the configured limit", "$"}
            .withContext("max_diagnostics", std::to_string(maxDiagnostics))};
}

ShaderCacheStore::ShaderCacheStore(std::filesystem::path directory)
    : directory_(std::move(directory)) {}

auto ShaderCacheStore::directory() const noexcept -> const std::filesystem::path& {
    return directory_;
}

auto ShaderCacheStore::pathForKey(const std::array<std::uint8_t, 32>& key) const
    -> std::filesystem::path {
    return directory_ / cacheFileName(key);
}

auto ShaderCacheStore::load(const ShaderCacheKeyInput& input) const
    -> core::Result<ShaderCacheRecord> {
    const auto key = encodeCacheKey(input);
    const auto path = pathForKey(key);
    std::error_code status;
    if (!std::filesystem::is_regular_file(path, status)) {
        return core::unexpected(cacheError(diagnosticCacheMissing, "Shader cache file is absent")
                                    .withContext("path", path.generic_string()));
    }
    std::ifstream inputFile{path, std::ios::binary};
    if (!inputFile) {
        return core::unexpected(
            cacheError(diagnosticCacheMissing, "Shader cache file could not be opened")
                .withContext("path", path.generic_string()));
    }
    std::vector<std::byte> bytes;
    inputFile.seekg(0, std::ios::end);
    const auto size = static_cast<std::size_t>(inputFile.tellg());
    inputFile.seekg(0, std::ios::beg);
    bytes.resize(size);
    if (size > 0) {
        inputFile.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
        if (!inputFile) {
            return core::unexpected(
                cacheError(diagnosticCacheKeyInvalid, "Shader cache file could not be read")
                    .withContext("path", path.generic_string()));
        }
    }
    auto decoded = decodeCache(bytes);
    if (!decoded) {
        return decoded;
    }
    if (decoded->key != key) {
        return core::unexpected(
            cacheError(diagnosticCacheKeyInvalid, "Shader cache file key does not match lookup"));
    }
    return adoptCacheRecord(*decoded);
}

auto ShaderCacheStore::store(ShaderCacheRecord record) const
    -> core::Result<std::filesystem::path> {
    std::error_code status;
    std::filesystem::create_directories(directory_, status);
    if (status) {
        return core::unexpected(
            cacheError(diagnosticCacheKeyInvalid, "Shader cache directory could not be created")
                .withContext("path", directory_.generic_string()));
    }
    auto encoded = encodeCache(record);
    if (!encoded) {
        return core::unexpected(std::move(encoded.error()));
    }
    const auto decoded = decodeCache(*encoded);
    if (!decoded) {
        return core::unexpected(std::move(decoded.error()));
    }
    const auto path = pathForKey(decoded->key);
    auto temporary = path;
    temporary += ".tmp";
    {
        std::ofstream output{temporary, std::ios::binary | std::ios::trunc};
        if (!output) {
            return core::unexpected(
                cacheError(diagnosticCacheKeyInvalid, "Shader cache file could not be written")
                    .withContext("path", temporary.generic_string()));
        }
        output.write(reinterpret_cast<const char*>(encoded->data()),
                     static_cast<std::streamsize>(encoded->size()));
        if (!output) {
            return core::unexpected(
                cacheError(diagnosticCacheKeyInvalid, "Shader cache file write failed")
                    .withContext("path", temporary.generic_string()));
        }
    }
    std::filesystem::remove(path, status);
    std::filesystem::rename(temporary, path, status);
    if (status) {
        std::filesystem::remove(temporary, status);
        return core::unexpected(
            cacheError(diagnosticCacheKeyInvalid, "Shader cache file could not be committed")
                .withContext("path", path.generic_string()));
    }
    return path;
}

} // namespace cuexis::shader

#include "cxc_test_support.hpp"

#include "cxc_hash_internal.hpp"
#include "zip32_envelope_internal.hpp"

#include <algorithm>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace cuexis::cxc::test {
namespace {

constexpr std::uint32_t localHeaderSignature = 0x04034B50U;
constexpr std::uint32_t centralHeaderSignature = 0x02014B50U;
constexpr std::uint32_t eocdSignature = 0x06054B50U;
constexpr std::size_t localHeaderBytes = 30;
constexpr std::size_t centralHeaderBytes = 46;
constexpr std::size_t eocdBytes = 22;

[[nodiscard]] auto rootProject(std::string_view chartPath) -> std::string {
    std::string result = R"({
  "format": "cuexis.project",
  "version": 1,
  "projectId": "019f0000-0000-7abc-8def-000000000c30",
  "assetRoots": [
    {
      "id": "main",
      "path": "assets"
    }
  ],
  "entry": {
    "chart": {
      "root": "main",
      "path": ")";
    result.append(chartPath);
    result += R"("
    }
  },
  "extensions": {}
}
)";
    return result;
}

[[nodiscard]] auto emptyAssetIndex() -> std::string {
    return R"({
  "format": "cuexis.asset-index",
  "version": 1,
  "assets": [],
  "extensions": {}
}
)";
}

[[nodiscard]] auto unusedAssetIndex() -> std::string {
    return R"({
  "format": "cuexis.asset-index",
  "version": 1,
  "assets": [
    {
      "id": "texture.unused",
      "type": "texture",
      "source": "textures/unused.bin",
      "dependencies": []
    }
  ],
  "extensions": {}
}
)";
}

[[nodiscard]] auto manifestText(const std::vector<CxcWriteEntry>& entries) -> std::string {
    std::ostringstream output;
    output << "{\n  \"entries\": [\n";
    for (std::size_t index = 0; index < entries.size(); ++index) {
        const auto& entry = entries[index];
        output << "    {\n"
               << "      \"byteCount\": " << entry.bytes.size() << ",\n"
               << "      \"path\": \"" << entry.path << "\",\n"
               << "      \"sha256\": \"" << detail::sha256Hex(entry.bytes) << "\"\n"
               << "    }" << (index + 1U == entries.size() ? "\n" : ",\n");
    }
    output << "  ],\n"
           << "  \"extensions\": {},\n"
           << "  \"format\": \"cuexis.cxc\",\n"
           << "  \"project\": \"cuexis.project.json\",\n"
           << "  \"requiredExtensions\": [],\n"
           << "  \"version\": 1\n"
           << "}\n";
    return output.str();
}

void requireRange(std::span<const std::byte> bytes, std::size_t offset, std::size_t count) {
    if (offset > bytes.size() || count > bytes.size() - offset) {
        throw std::runtime_error{"ZIP test helper accessed an invalid byte range"};
    }
}

} // namespace

auto sourceRoot() -> std::filesystem::path {
    return std::filesystem::path{CUEXIS_SOURCE_DIR};
}

auto binaryFixtureRoot() -> std::filesystem::path {
    return sourceRoot() / "tests" / "fixtures" / "chart_format_update" / "binary";
}

auto goldenFixtureRoot() -> std::filesystem::path {
    return sourceRoot() / "tests" / "fixtures" / "chart_format_update" / "golden";
}

auto readText(const std::filesystem::path& path) -> std::string {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw std::runtime_error{"Could not open text fixture: " + path.string()};
    }
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

auto readBytes(const std::filesystem::path& path) -> std::vector<std::byte> {
    std::ifstream input{path, std::ios::binary | std::ios::ate};
    if (!input) {
        throw std::runtime_error{"Could not open binary fixture: " + path.string()};
    }
    const auto size = input.tellg();
    if (size < 0) {
        throw std::runtime_error{"Could not size binary fixture: " + path.string()};
    }
    std::vector<std::byte> result(static_cast<std::size_t>(size));
    input.seekg(0);
    if (!result.empty()) {
        input.read(reinterpret_cast<char*>(result.data()), size);
    }
    if (!input) {
        throw std::runtime_error{"Could not read binary fixture: " + path.string()};
    }
    return result;
}

void writeBytes(const std::filesystem::path& path, std::span<const std::byte> bytes) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    if (!output) {
        throw std::runtime_error{"Could not create binary fixture: " + path.string()};
    }
    if (!bytes.empty()) {
        output.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
    }
    if (!output) {
        throw std::runtime_error{"Could not write binary fixture: " + path.string()};
    }
}

auto bytesFromText(std::string_view text) -> std::vector<std::byte> {
    std::vector<std::byte> result;
    result.reserve(text.size());
    for (const char character : text) {
        result.push_back(static_cast<std::byte>(static_cast<unsigned char>(character)));
    }
    return result;
}

auto textFromBytes(std::span<const std::byte> bytes) -> std::string {
    std::string result(bytes.size(), '\0');
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        result[index] = static_cast<char>(std::to_integer<unsigned char>(bytes[index]));
    }
    return result;
}

auto textEntry(std::string path, std::string text) -> CxcWriteEntry {
    return CxcWriteEntry{std::move(path), bytesFromText(text)};
}

auto binaryEntry(std::string path, std::string_view bytes) -> CxcWriteEntry {
    return textEntry(std::move(path), std::string{bytes});
}

auto makeV1Request() -> CxcWriteRequest {
    const auto root = sourceRoot() / "assets" / "projects" / "stage1b_project";
    CxcWriteRequest request;
    request.entries = {
        textEntry("cuexis.project.json", readText(root / "cuexis.project.json")),
        textEntry("assets/cuexis.asset-index.json",
                  readText(root / "assets" / "cuexis.asset-index.json")),
        textEntry("assets/charts/stage1b_example.cuexis.chart.json",
                  readText(root / "assets" / "charts" / "stage1b_example.cuexis.chart.json")),
        CxcWriteEntry{"assets/materials/basic.material.bin",
                      readBytes(root / "assets" / "materials" / "basic.material.bin")},
        CxcWriteEntry{"assets/meshes/note.mesh.bin",
                      readBytes(root / "assets" / "meshes" / "note.mesh.bin")},
        CxcWriteEntry{"assets/textures/white.texture.bin",
                      readBytes(root / "assets" / "textures" / "white.texture.bin")},
    };
    return request;
}

auto makeV2Request() -> CxcWriteRequest {
    const auto root = sourceRoot() / "assets" / "projects" / "stage1d_project";
    CxcWriteRequest request;
    request.entries = {
        textEntry("cuexis.project.json", readText(root / "cuexis.project.json")),
        textEntry("assets/cuexis.asset-index.json",
                  readText(root / "assets" / "cuexis.asset-index.json")),
        textEntry("assets/charts/stage1d_example.cuexis.chart.json",
                  readText(root / "assets" / "charts" / "stage1d_example.cuexis.chart.json")),
        CxcWriteEntry{"assets/audio/main.wav", readBytes(root / "assets" / "audio" / "main.wav")},
    };
    return request;
}

auto makeV3Request() -> CxcWriteRequest {
    CxcWriteRequest request;
    request.entries = {
        textEntry("cuexis.project.json", rootProject("charts/main.cuexis.chart.json")),
        textEntry("assets/cuexis.asset-index.json", emptyAssetIndex()),
        textEntry("assets/charts/main.cuexis.chart.json",
                  readText(sourceRoot() / "tests" / "fixtures" /
                           "stage2_migration_v3.golden.cuexis.chart.json")),
    };
    return request;
}

auto makeV4CxtRequest() -> CxcWriteRequest {
    const auto fixtureRoot = sourceRoot() / "tests" / "fixtures" / "chart_format_update" / "valid";
    const auto sourceRootPath =
        sourceRoot() / "tests" / "fixtures" / "chart_format_update" / "source_project";
    CxcWriteRequest request;
    request.entries = {
        textEntry("cuexis.project.json", readText(sourceRootPath / "cuexis.project.json")),
        textEntry("assets/cuexis.asset-index.json",
                  readText(sourceRootPath / "assets" / "cuexis.asset-index.json")),
        textEntry("assets/charts/main.cuexis.chart.json",
                  readText(fixtureRoot / "chart_v4_cxt_template_binding.json")),
        CxcWriteEntry{"assets/textures/unused.bin",
                      readBytes(sourceRootPath / "assets" / "textures" / "unused.bin")},
        textEntry("templates/move-y.cxt", readText(fixtureRoot / "templates" / "move-y.cxt")),
    };
    return request;
}

auto staticV4Chart() -> std::string {
    return readText(sourceRoot() / "tests" / "fixtures" / "chart_format_update" / "valid" /
                    "chart_v4_static_migration.json");
}

auto writePackage(CxcWriteRequest request, const CxcPackageLimits& limits)
    -> std::vector<std::byte> {
    auto written = CxcWriter::write(std::move(request), limits);
    if (!written.hasValue()) {
        throw std::runtime_error{"CXC Writer rejected a valid test package:\n" +
                                 diagnosticsText(written.diagnostics)};
    }
    return std::move(*written.bytes);
}

auto writeUncheckedPackage(std::vector<CxcWriteEntry> entries, const CxcPackageLimits& limits)
    -> std::vector<std::byte> {
    std::ranges::sort(entries, {}, &CxcWriteEntry::path);
    std::vector<CxcWriteEntry> archive;
    archive.reserve(entries.size() + 1U);
    archive.push_back(textEntry("cuexis.cxc.json", manifestText(entries)));
    for (auto& entry : entries) {
        archive.push_back(std::move(entry));
    }
    return writeRawZip(std::move(archive), limits);
}

auto writeRawZip(std::vector<CxcWriteEntry> entries, const CxcPackageLimits& limits)
    -> std::vector<std::byte> {
    std::vector<std::pair<std::string, std::vector<std::byte>>> rawEntries;
    rawEntries.reserve(entries.size());
    for (auto& entry : entries) {
        rawEntries.emplace_back(std::move(entry.path), std::move(entry.bytes));
    }
    auto written = detail::writeCanonicalZip32(rawEntries, limits);
    if (!written) {
        throw std::runtime_error{"Raw ZIP writer failed: " + std::string{written.error().code()} +
                                 ": " + std::string{written.error().message()}};
    }
    return std::move(*written);
}

auto parseZip(std::span<const std::byte> bytes) -> ZipLayout {
    if (bytes.size() < eocdBytes) {
        throw std::runtime_error{"ZIP test helper received a truncated archive"};
    }
    ZipLayout result;
    result.eocdOffset = bytes.size() - eocdBytes;
    if (readU32(bytes, result.eocdOffset) != eocdSignature) {
        throw std::runtime_error{"ZIP test helper could not find the EOCD"};
    }
    const auto entryCount = readU16(bytes, result.eocdOffset + 10U);
    result.centralSize = readU32(bytes, result.eocdOffset + 12U);
    result.centralOffset = readU32(bytes, result.eocdOffset + 16U);
    auto position = result.centralOffset;
    result.entries.reserve(entryCount);
    for (std::size_t index = 0; index < entryCount; ++index) {
        requireRange(bytes, position, centralHeaderBytes);
        if (readU32(bytes, position) != centralHeaderSignature) {
            throw std::runtime_error{"ZIP test helper found an invalid central header"};
        }
        const auto filenameLength = readU16(bytes, position + 28U);
        const auto extraLength = readU16(bytes, position + 30U);
        const auto commentLength = readU16(bytes, position + 32U);
        const auto byteCount = readU32(bytes, position + 24U);
        const auto localOffset = readU32(bytes, position + 42U);
        requireRange(bytes, position + centralHeaderBytes, filenameLength);
        const auto path =
            textFromBytes(bytes.subspan(position + centralHeaderBytes, filenameLength));
        requireRange(bytes, localOffset, localHeaderBytes);
        if (readU32(bytes, localOffset) != localHeaderSignature) {
            throw std::runtime_error{"ZIP test helper found an invalid local header"};
        }
        const auto localFilenameLength = readU16(bytes, localOffset + 26U);
        const auto localExtraLength = readU16(bytes, localOffset + 28U);
        const auto dataOffset =
            localOffset + localHeaderBytes + localFilenameLength + localExtraLength;
        requireRange(bytes, dataOffset, byteCount);
        result.entries.push_back(
            ZipEntryLocation{path, localOffset, dataOffset, byteCount, position});
        position += centralHeaderBytes + filenameLength + extraLength + commentLength;
    }
    return result;
}

auto findEntry(const ZipLayout& layout, std::string_view path) -> const ZipEntryLocation& {
    const auto found = std::ranges::find(layout.entries, path, &ZipEntryLocation::path);
    if (found == layout.entries.end()) {
        throw std::runtime_error{"ZIP test helper could not find entry: " + std::string{path}};
    }
    return *found;
}

auto readU16(std::span<const std::byte> bytes, std::size_t offset) -> std::uint16_t {
    requireRange(bytes, offset, 2U);
    return static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[offset])) |
           static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[offset + 1U]) << 8U);
}

auto readU32(std::span<const std::byte> bytes, std::size_t offset) -> std::uint32_t {
    requireRange(bytes, offset, 4U);
    return static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset])) |
           (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 1U])) << 8U) |
           (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 2U])) << 16U) |
           (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 3U])) << 24U);
}

void writeU16(std::vector<std::byte>& bytes, std::size_t offset, std::uint16_t value) {
    requireRange(bytes, offset, 2U);
    bytes[offset] = static_cast<std::byte>(value & 0xFFU);
    bytes[offset + 1U] = static_cast<std::byte>((value >> 8U) & 0xFFU);
}

void writeU32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value) {
    requireRange(bytes, offset, 4U);
    bytes[offset] = static_cast<std::byte>(value & 0xFFU);
    bytes[offset + 1U] = static_cast<std::byte>((value >> 8U) & 0xFFU);
    bytes[offset + 2U] = static_cast<std::byte>((value >> 16U) & 0xFFU);
    bytes[offset + 3U] = static_cast<std::byte>((value >> 24U) & 0xFFU);
}

auto crc32(std::span<const std::byte> bytes) noexcept -> std::uint32_t {
    std::uint32_t result = 0xFFFFFFFFU;
    for (const auto value : bytes) {
        result ^= std::to_integer<std::uint8_t>(value);
        for (std::size_t bit = 0; bit < 8U; ++bit) {
            const auto mask = static_cast<std::uint32_t>(0U - (result & 1U));
            result = (result >> 1U) ^ (0xEDB88320U & mask);
        }
    }
    return result ^ 0xFFFFFFFFU;
}

void refreshEntryCrc(std::vector<std::byte>& bytes, std::string_view path) {
    const auto layout = parseZip(bytes);
    const auto& entry = findEntry(layout, path);
    const auto checksum =
        crc32(std::span<const std::byte>{bytes.data() + entry.dataOffset, entry.byteCount});
    writeU32(bytes, entry.localHeaderOffset + 14U, checksum);
    writeU32(bytes, entry.centralHeaderOffset + 16U, checksum);
}

void replaceEntryData(std::vector<std::byte>& bytes, std::string_view path,
                      std::span<const std::byte> replacement) {
    const auto layout = parseZip(bytes);
    const auto& entry = findEntry(layout, path);
    if (replacement.size() != entry.byteCount) {
        throw std::runtime_error{"ZIP test helper replacement changed entry size"};
    }
    std::copy(replacement.begin(), replacement.end(), bytes.begin() + entry.dataOffset);
    refreshEntryCrc(bytes, path);
}

void replaceEntryText(std::vector<std::byte>& bytes, std::string_view path, std::string_view from,
                      std::string_view to) {
    if (from.size() != to.size()) {
        throw std::runtime_error{"ZIP test helper text replacement changed entry size"};
    }
    const auto layout = parseZip(bytes);
    const auto& entry = findEntry(layout, path);
    auto text =
        textFromBytes(std::span<const std::byte>{bytes.data() + entry.dataOffset, entry.byteCount});
    const auto position = text.find(from);
    if (position == std::string::npos) {
        throw std::runtime_error{"ZIP test helper could not find replacement text"};
    }
    text.replace(position, from.size(), to);
    replaceEntryData(bytes, path, bytesFromText(text));
}

void replaceEntryPath(std::vector<std::byte>& bytes, std::string_view oldPath,
                      std::string_view newPath) {
    if (oldPath.size() != newPath.size()) {
        throw std::runtime_error{"ZIP test helper path replacement changed filename size"};
    }
    const auto layout = parseZip(bytes);
    const auto& entry = findEntry(layout, oldPath);
    const auto replacement = bytesFromText(newPath);
    std::copy(replacement.begin(), replacement.end(),
              bytes.begin() + entry.localHeaderOffset + localHeaderBytes);
    std::copy(replacement.begin(), replacement.end(),
              bytes.begin() + entry.centralHeaderOffset + centralHeaderBytes);
}

auto hasDiagnostic(const core::Diagnostics& diagnostics, std::string_view code) -> bool {
    return std::ranges::any_of(diagnostics.items(), [code](const core::Diagnostic& diagnostic) {
        return diagnostic.code() == code;
    });
}

auto diagnosticsText(const core::Diagnostics& diagnostics) -> std::string {
    std::ostringstream output;
    for (const auto& diagnostic : diagnostics.items()) {
        output << diagnostic.code() << " at " << diagnostic.fieldPath() << ": "
               << diagnostic.message() << '\n';
    }
    return output.str();
}

} // namespace cuexis::cxc::test

#include "asset_publish_internal.hpp"

#include "publish_fs_internal.hpp"

#include <cuexis/core/diagnostic.hpp>
#include <cuexis_internal/sha256.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace cuexis::tools {

using detail::publishError;
using detail::publishInvalidCode;
using detail::publishIoCode;
using detail::publishStagingCode;
using detail::publishValidationCode;

// --- small helpers -----------------------------------------------------------------------------

[[nodiscard]] auto diagnosticsMessage(const core::Diagnostics& diagnostics) -> std::string {
    std::string out;
    for (const auto& item : diagnostics.items()) {
        if (!out.empty()) {
            out += "; ";
        }
        out += std::string{item.code()};
        if (!item.fieldPath().empty()) {
            out += " at ";
            out += item.fieldPath();
        }
        if (!item.message().empty()) {
            out += ": ";
            out += item.message();
        }
    }
    if (out.empty()) {
        out = "no diagnostic was produced";
    }
    return out;
}

// std::filesystem::path construction from a string_view is not portable across toolchains, so every
// constant name goes through one conversion point.
[[nodiscard]] auto pathOf(std::string_view name) -> fs::path {
    return fs::path{std::string{name}};
}

[[nodiscard]] auto isPortableToken(std::string_view value, std::size_t maxBytes) noexcept -> bool {
    if (value.empty() || value.size() > maxBytes) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](char character) {
        return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') ||
               (character >= '0' && character <= '9') || character == '.' || character == '_' ||
               character == '-';
    });
}

[[nodiscard]] auto foldAscii(std::string_view value) -> std::string {
    std::string out;
    out.reserve(value.size());
    for (const char character : value) {
        out.push_back(character >= 'A' && character <= 'Z'
                          ? static_cast<char>(character - 'A' + 'a')
                          : character);
    }
    return out;
}

// The total payload of a generation closure. It is the sum of the entry byte counts, not the number
// of entries: `GenerationPublishResult::closureBytes` names bytes, and a republish must report the
// same value as the publish that created the generation.
[[nodiscard]] auto closureByteTotal(const std::vector<ListedEntry>& closure) -> std::uint64_t {
    std::uint64_t total = 0;
    for (const auto& entry : closure) {
        total += entry.byteCount;
    }
    return total;
}

[[nodiscard]] auto listEntries(const std::vector<PublishEntry>& entries)
    -> core::Result<std::vector<ListedEntry>> {
    if (entries.size() > maxEntries) {
        return core::unexpected(publishError(publishInvalidCode, "Publication batch is too large")
                                    .withContext("entries", std::to_string(entries.size())));
    }
    std::vector<ListedEntry> listed;
    listed.reserve(entries.size());
    std::vector<std::string> seen;
    seen.reserve(entries.size());
    for (const auto& entry : entries) {
        if (!detail::isPortableRelativePath(entry.path)) {
            return core::unexpected(
                publishError(publishInvalidCode, "Publication entry path is not portable")
                    .withContext("path", entry.path));
        }
        if (entry.bytes.empty()) {
            return core::unexpected(publishError(publishInvalidCode, "Publication entry is empty")
                                        .withContext("path", entry.path));
        }
        const auto folded = foldAscii(entry.path);
        if (std::find(seen.begin(), seen.end(), folded) != seen.end()) {
            return core::unexpected(
                publishError(publishInvalidCode, "Publication entry path is duplicated")
                    .withContext("path", entry.path));
        }
        seen.push_back(folded);
        listed.push_back(ListedEntry{entry.path, static_cast<std::uint64_t>(entry.bytes.size()),
                                     detail::sha256Hex(entry.bytes)});
    }
    std::sort(listed.begin(), listed.end(), [](const ListedEntry& left, const ListedEntry& right) {
        return left.path < right.path;
    });
    return listed;
}

[[nodiscard]] auto generationIdentityOf(std::string_view label,
                                        const std::vector<ListedEntry>& closure,
                                        const std::vector<ListedEntry>& provenance) -> std::string {
    core::detail::Sha256 hash;
    const auto update = [&hash](std::string_view text) {
        const auto bytes = std::as_bytes(std::span<const char>{text.data(), text.size()});
        std::array<std::byte, 8> length{};
        const auto size = static_cast<std::uint64_t>(bytes.size());
        for (std::size_t index = 0; index < length.size(); ++index) {
            length[index] = static_cast<std::byte>((size >> (8U * index)) & 0xFFU);
        }
        hash.update(length);
        hash.update(bytes);
    };
    update(generationDomain);
    update(label);
    const auto updateList = [&update](std::string_view tag,
                                      const std::vector<ListedEntry>& entries) {
        update(tag);
        update(std::to_string(entries.size()));
        for (const auto& entry : entries) {
            update(entry.path);
            update(std::to_string(entry.byteCount));
            update(entry.sha256);
        }
    };
    updateList("closure", closure);
    updateList("provenance", provenance);
    return core::detail::sha256Hex(hash.finish());
}

// --- generation marker -------------------------------------------------------------------------

[[nodiscard]] auto encodeMarker(const MarkerDocument& marker) -> std::string {
    std::string out;
    out += std::string{generationMarkerFormat};
    out += ' ';
    out += std::to_string(generationMarkerVersion);
    out += '\n';
    out += "generationId ";
    out += marker.label;
    out += '\n';
    out += "identity ";
    out += marker.identity;
    out += '\n';
    const auto appendList = [&out](std::string_view tag, const std::vector<ListedEntry>& entries) {
        out += tag;
        out += ' ';
        out += std::to_string(entries.size());
        out += '\n';
        for (const auto& entry : entries) {
            out += entry.sha256;
            out += ' ';
            out += std::to_string(entry.byteCount);
            out += ' ';
            out += entry.path;
            out += '\n';
        }
    };
    appendList("closure", marker.closure);
    appendList("provenance", marker.provenance);
    return out;
}

class LineReader final {
  public:
    explicit LineReader(std::string_view text) noexcept : text_(text) {}

    // Every line must be terminated: a truncated marker is never accepted.
    [[nodiscard]] auto next() noexcept -> std::optional<std::string_view> {
        if (offset_ >= text_.size()) {
            return std::nullopt;
        }
        const auto end = text_.find('\n', offset_);
        if (end == std::string_view::npos) {
            offset_ = text_.size();
            return std::nullopt;
        }
        const auto line = text_.substr(offset_, end - offset_);
        offset_ = end + 1;
        return line;
    }

    [[nodiscard]] auto finished() const noexcept -> bool {
        return offset_ >= text_.size();
    }

  private:
    std::string_view text_;
    std::size_t offset_{};
};

[[nodiscard]] auto splitFields(std::string_view line, std::size_t expected)
    -> std::optional<std::vector<std::string_view>> {
    std::vector<std::string_view> fields;
    std::size_t offset = 0;
    while (true) {
        const auto separator = line.find(' ', offset);
        if (separator == std::string_view::npos) {
            fields.push_back(line.substr(offset));
            break;
        }
        fields.push_back(line.substr(offset, separator - offset));
        offset = separator + 1;
    }
    if (fields.size() != expected) {
        return std::nullopt;
    }
    for (const auto& field : fields) {
        if (field.empty()) {
            return std::nullopt;
        }
    }
    return fields;
}

[[nodiscard]] auto parseCount(std::string_view text) -> std::optional<std::size_t> {
    if (text.empty() || text.size() > 10) {
        return std::nullopt;
    }
    std::size_t value = 0;
    for (const char character : text) {
        if (character < '0' || character > '9') {
            return std::nullopt;
        }
        value = value * 10U + static_cast<std::size_t>(character - '0');
    }
    return value;
}

[[nodiscard]] auto decodeMarker(std::string_view text) -> core::Result<MarkerDocument> {
    const auto invalid = [](std::string message) {
        return publishError(publishValidationCode, std::move(message));
    };
    LineReader reader{text};
    auto header = reader.next();
    if (!header.has_value() || *header != std::string{generationMarkerFormat} + " " +
                                              std::to_string(generationMarkerVersion)) {
        return core::unexpected(invalid("Generation marker has an unknown header"));
    }
    MarkerDocument marker;
    auto labelLine = reader.next();
    if (!labelLine.has_value()) {
        return core::unexpected(invalid("Generation marker is truncated"));
    }
    auto labelFields = splitFields(*labelLine, 2);
    if (!labelFields.has_value() || (*labelFields)[0] != "generationId" ||
        !isPortableToken((*labelFields)[1], maxGenerationIdBytes)) {
        return core::unexpected(invalid("Generation marker has a malformed generationId"));
    }
    marker.label = std::string{(*labelFields)[1]};
    auto identityLine = reader.next();
    if (!identityLine.has_value()) {
        return core::unexpected(invalid("Generation marker is truncated"));
    }
    auto identityFields = splitFields(*identityLine, 2);
    if (!identityFields.has_value() || (*identityFields)[0] != "identity" ||
        !detail::isLowerHex((*identityFields)[1], 64)) {
        return core::unexpected(invalid("Generation marker has a malformed identity"));
    }
    marker.identity = std::string{(*identityFields)[1]};

    const auto readList = [&](std::string_view tag,
                              std::vector<ListedEntry>& entries) -> core::Result<void> {
        auto countLine = reader.next();
        if (!countLine.has_value()) {
            return core::unexpected(invalid("Generation marker is truncated"));
        }
        auto countFields = splitFields(*countLine, 2);
        if (!countFields.has_value() || (*countFields)[0] != tag) {
            return core::unexpected(
                invalid("Generation marker is missing the " + std::string{tag} + " section"));
        }
        const auto count = parseCount((*countFields)[1]);
        if (!count.has_value() || *count > maxEntries) {
            return core::unexpected(invalid("Generation marker has a malformed entry count"));
        }
        entries.reserve(*count);
        std::string previous;
        for (std::size_t index = 0; index < *count; ++index) {
            auto entryLine = reader.next();
            if (!entryLine.has_value()) {
                return core::unexpected(invalid("Generation marker is truncated"));
            }
            auto fields = splitFields(*entryLine, 3);
            if (!fields.has_value() || !detail::isLowerHex((*fields)[0], 64)) {
                return core::unexpected(invalid("Generation marker has a malformed entry"));
            }
            const auto byteCount = parseCount((*fields)[1]);
            if (!byteCount.has_value() || *byteCount == 0) {
                return core::unexpected(invalid("Generation marker has a malformed entry size"));
            }
            if (!detail::isPortableRelativePath((*fields)[2])) {
                return core::unexpected(invalid("Generation marker has a non-portable entry path"));
            }
            ListedEntry entry{std::string{(*fields)[2]}, static_cast<std::uint64_t>(*byteCount),
                              std::string{(*fields)[0]}};
            if (!previous.empty() && entry.path <= previous) {
                return core::unexpected(
                    invalid("Generation marker entries are not strictly ordered"));
            }
            previous = entry.path;
            entries.push_back(std::move(entry));
        }
        return {};
    };

    auto closureResult = readList("closure", marker.closure);
    if (!closureResult) {
        return core::unexpected(std::move(closureResult.error()));
    }
    auto provenanceResult = readList("provenance", marker.provenance);
    if (!provenanceResult) {
        return core::unexpected(std::move(provenanceResult.error()));
    }
    if (!reader.finished()) {
        return core::unexpected(invalid("Generation marker has trailing content"));
    }
    return marker;
}

// Re-hashes every listed file, so a published generation is never trusted on its marker alone.
[[nodiscard]] auto verifyGenerationTree(const fs::path& generationPath,
                                        const MarkerDocument& marker) -> core::Result<void> {
    const auto verifyList = [&](std::string_view tag, const std::vector<ListedEntry>& entries,
                                const fs::path& base) -> core::Result<void> {
        for (const auto& entry : entries) {
            const auto file = base / fs::path{entry.path};
            auto bytes = detail::readFileBytes(file, static_cast<std::size_t>(entry.byteCount));
            if (!bytes) {
                return core::unexpected(
                    publishError(publishValidationCode,
                                 "Published generation file is missing or unreadable")
                        .withContext("section", std::string{tag})
                        .withContext("path", entry.path));
            }
            if (bytes->size() != entry.byteCount) {
                return core::unexpected(
                    publishError(publishValidationCode, "Published generation file size differs")
                        .withContext("path", entry.path)
                        .withContext("expected", std::to_string(entry.byteCount))
                        .withContext("actual", std::to_string(bytes->size())));
            }
            const auto digest = detail::sha256Hex(*bytes);
            if (digest != entry.sha256) {
                return core::unexpected(publishError(publishValidationCode,
                                                     "Published generation file identity differs")
                                            .withContext("path", entry.path)
                                            .withContext("expected", entry.sha256)
                                            .withContext("actual", digest));
            }
        }
        return {};
    };
    auto closureResult = verifyList("closure", marker.closure, generationPath);
    if (!closureResult) {
        return core::unexpected(std::move(closureResult.error()));
    }
    return verifyList("provenance", marker.provenance,
                      generationPath / pathOf(provenanceDirectoryName));
}

[[nodiscard]] auto readMarker(const fs::path& generationPath) -> core::Result<MarkerDocument> {
    auto bytes = detail::readFileBytes(generationPath / pathOf(generationMarkerName), 1U << 20U);
    if (!bytes) {
        return core::unexpected(
            publishError(publishValidationCode, "Published generation marker is missing")
                .withContext("path", generationPath.generic_string()));
    }
    const std::string_view text{reinterpret_cast<const char*>(bytes->data()), bytes->size()};
    return decodeMarker(text);
}

[[nodiscard]] auto markersEqual(const MarkerDocument& left, const MarkerDocument& right) noexcept
    -> bool {
    const auto equalList = [](const std::vector<ListedEntry>& a,
                              const std::vector<ListedEntry>& b) {
        if (a.size() != b.size()) {
            return false;
        }
        for (std::size_t index = 0; index < a.size(); ++index) {
            if (a[index].path != b[index].path || a[index].byteCount != b[index].byteCount ||
                a[index].sha256 != b[index].sha256) {
                return false;
            }
        }
        return true;
    };
    return left.label == right.label && left.identity == right.identity &&
           equalList(left.closure, right.closure) && equalList(left.provenance, right.provenance);
}

// --- shared publication steps ------------------------------------------------------------------

[[nodiscard]] auto writeEntryTree(const fs::path& base, const std::vector<PublishEntry>& entries,
                                  const fs::path& relativeBase) -> core::Result<void> {
    for (const auto& entry : entries) {
        const auto file = base / relativeBase / fs::path{entry.path};
        std::error_code status;
        fs::create_directories(file.parent_path(), status);
        if (status) {
            return core::unexpected(
                publishError(publishStagingCode, "Publication staging directory failed")
                    .withContext("path", file.parent_path().generic_string()));
        }
        auto written = detail::writeFileExclusive(file, entry.bytes);
        if (!written) {
            return core::unexpected(std::move(written.error()));
        }
    }
    return {};
}

auto readAdoptedGeneration(const fs::path& root) -> core::Result<std::string> {
    auto bytes = detail::readFileBytes(root / pathOf(adoptedGenerationName), 4096);
    if (!bytes) {
        return core::unexpected(
            publishError(publishValidationCode, "No generation has been adopted")
                .withContext("path", root.generic_string()));
    }
    const std::string_view text{reinterpret_cast<const char*>(bytes->data()), bytes->size()};
    LineReader reader{text};
    auto header = reader.next();
    if (!header.has_value() || *header != std::string{adoptedGenerationFormat} + " " +
                                              std::to_string(adoptedGenerationVersion)) {
        return core::unexpected(
            publishError(publishValidationCode, "Adoption record has an unknown header"));
    }
    auto identity = reader.next();
    if (!identity.has_value() || !detail::isLowerHex(*identity, 64) || !reader.finished()) {
        return core::unexpected(
            publishError(publishValidationCode, "Adoption record is malformed"));
    }
    return std::string{*identity};
}

} // namespace cuexis::tools

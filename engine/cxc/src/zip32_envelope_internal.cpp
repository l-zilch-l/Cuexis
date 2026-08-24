#include "zip32_envelope_internal.hpp"

#include "cxc_hash_internal.hpp"
#include "cxc_path_internal.hpp"

#include <cuexis/core/error.hpp>

#include <minizip-ng/mz.h>
#include <minizip-ng/mz_strm.h>
#include <minizip-ng/mz_zip.h>
#include <minizip-ng/mz_zip_rw.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cuexis::cxc::detail {
namespace {

constexpr std::uint32_t localHeaderSignature = 0x04034B50U;
constexpr std::uint32_t centralHeaderSignature = 0x02014B50U;
constexpr std::uint32_t endOfCentralDirectorySignature = 0x06054B50U;
constexpr std::uint32_t zip64EndOfCentralDirectorySignature = 0x06064B50U;
constexpr std::uint32_t zip64EndOfCentralDirectoryLocatorSignature = 0x07064B50U;
constexpr std::uint32_t zip64Sentinel32 = 0xFFFFFFFFU;
constexpr std::uint16_t zip64Sentinel16 = 0xFFFFU;
constexpr std::uint16_t zip64ExtraFieldId = 0x0001U;
constexpr std::size_t localHeaderBytes = 30;
constexpr std::size_t centralHeaderBytes = 46;
constexpr std::size_t endOfCentralDirectoryBytes = 22;

struct CentralEntry final {
    Zip32Entry entry;
    std::uint16_t versionMadeBy{};
    std::uint16_t versionNeeded{};
    std::uint16_t flags{};
    std::uint16_t method{};
    std::uint16_t modifiedTime{};
    std::uint16_t modifiedDate{};
    std::uint16_t internalAttributes{};
    std::uint32_t externalAttributes{};
};

[[nodiscard]] auto makeDiagnostics(const CxcPackageLimits& limits) -> core::Diagnostics {
    return core::Diagnostics{limits.maxDiagnostics,
                             core::Diagnostic{core::DiagnosticSeverity::Error,
                                              "cxc.budget.exceeded",
                                              "CXC diagnostics reached the configured limit", "$"}};
}

void addError(core::Diagnostics& diagnostics, std::string code, std::string message,
              std::string fieldPath, std::string_view entryPath = {}) {
    auto diagnostic = core::Diagnostic{core::DiagnosticSeverity::Error, std::move(code),
                                       std::move(message), std::move(fieldPath)};
    if (!entryPath.empty()) {
        diagnostic.withContext("path", std::string{entryPath});
    }
    static_cast<void>(diagnostics.add(std::move(diagnostic)));
}

template <typename Value>
[[nodiscard]] auto checkedAdd(Value left, Value right, Value& result) noexcept -> bool {
    if (right > std::numeric_limits<Value>::max() - left) {
        return false;
    }
    result = static_cast<Value>(left + right);
    return true;
}

[[nodiscard]] auto hasRange(std::span<const std::byte> bytes, std::size_t offset,
                            std::size_t count) noexcept -> bool {
    return offset <= bytes.size() && count <= bytes.size() - offset;
}

[[nodiscard]] auto readU16(std::span<const std::byte> bytes, std::size_t offset,
                           std::uint16_t& result) noexcept -> bool {
    if (!hasRange(bytes, offset, 2)) {
        return false;
    }
    result = static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[offset])) |
             static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[offset + 1]) << 8U);
    return true;
}

[[nodiscard]] auto readU32(std::span<const std::byte> bytes, std::size_t offset,
                           std::uint32_t& result) noexcept -> bool {
    if (!hasRange(bytes, offset, 4)) {
        return false;
    }
    result = static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset])) |
             (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 1])) << 8U) |
             (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 2])) << 16U) |
             (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 3])) << 24U);
    return true;
}

[[nodiscard]] auto textAt(std::span<const std::byte> bytes, std::size_t offset, std::size_t count)
    -> std::string {
    std::string result;
    result.resize(count);
    for (std::size_t index = 0; index < count; ++index) {
        result[index] = static_cast<char>(std::to_integer<unsigned char>(bytes[offset + index]));
    }
    return result;
}

[[nodiscard]] auto hasZip64ExtraField(std::span<const std::byte> bytes, std::size_t offset,
                                      std::uint16_t length) noexcept -> bool {
    std::uint16_t headerId = 0;
    return length >= 2 && readU16(bytes, offset, headerId) && headerId == zip64ExtraFieldId;
}

[[nodiscard]] auto isRegularFileAttributes(std::uint16_t versionMadeBy,
                                           std::uint32_t externalAttributes) noexcept -> bool {
    const auto hostSystem = static_cast<std::uint8_t>(versionMadeBy >> 8U);
    if (hostSystem == 0) {
        return (externalAttributes & 0x18U) == 0;
    }
    if (hostSystem == 3) {
        const auto unixMode = static_cast<std::uint16_t>(externalAttributes >> 16U);
        const auto fileType = static_cast<std::uint16_t>(unixMode & 0170000U);
        return fileType == 0 || fileType == 0100000U;
    }
    return false;
}

[[nodiscard]] auto validateFeatureFields(core::Diagnostics& diagnostics,
                                         const CentralEntry& candidate, std::string_view fieldPath)
    -> bool {
    if (candidate.flags != 0) {
        addError(diagnostics, "cxc.archive.feature_unsupported",
                 "CXC entries must not use ZIP flags", std::string{fieldPath},
                 candidate.entry.metadata.path);
        return false;
    }
    if (candidate.method != 0) {
        addError(diagnostics, "cxc.archive.feature_unsupported",
                 "CXC entries must use the Stored compression method", std::string{fieldPath},
                 candidate.entry.metadata.path);
        return false;
    }
    if (candidate.versionNeeded > 20) {
        addError(diagnostics, "cxc.archive.feature_unsupported",
                 "CXC entry requires an unsupported ZIP feature version", std::string{fieldPath},
                 candidate.entry.metadata.path);
        return false;
    }
    if (!isRegularFileAttributes(candidate.versionMadeBy, candidate.externalAttributes)) {
        addError(diagnostics, "cxc.archive.feature_unsupported",
                 "CXC entries must be regular files", std::string{fieldPath},
                 candidate.entry.metadata.path);
        return false;
    }
    return true;
}

void appendU16(std::vector<std::byte>& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::byte>(value & 0xFFU));
    bytes.push_back(static_cast<std::byte>((value >> 8U) & 0xFFU));
}

void appendU32(std::vector<std::byte>& bytes, std::uint32_t value) {
    bytes.push_back(static_cast<std::byte>(value & 0xFFU));
    bytes.push_back(static_cast<std::byte>((value >> 8U) & 0xFFU));
    bytes.push_back(static_cast<std::byte>((value >> 16U) & 0xFFU));
    bytes.push_back(static_cast<std::byte>((value >> 24U) & 0xFFU));
}

void appendText(std::vector<std::byte>& bytes, std::string_view text) {
    for (const char character : text) {
        bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(character)));
    }
}

[[nodiscard]] auto minizipFailure() -> core::Result<void> {
    return core::unexpected(
        core::Error{"cxc.archive.invalid", "CXC archive failed independent ZIP validation"});
}

} // namespace

auto validateZip32Envelope(std::span<const std::byte> bytes, const CxcPackageLimits& limits)
    -> Zip32EnvelopeResult {
    auto diagnostics = makeDiagnostics(limits);
    if (limits.maxDiagnostics == 0 || limits.maxPackageBytes == 0 || limits.maxEntryBytes == 0 ||
        limits.maxEntries == 0 || limits.maxPathBytes == 0 || limits.maxPathDepth == 0 ||
        limits.maxDependencyDepth == 0) {
        addError(diagnostics, "cxc.budget.exceeded",
                 "CXC package limits must all be greater than zero", "$/limits");
        diagnostics.sortDeterministically();
        return {{}, std::move(diagnostics)};
    }
    if (bytes.size() > limits.maxPackageBytes) {
        addError(diagnostics, "cxc.budget.exceeded", "CXC package exceeds the byte limit",
                 "$/archive");
        diagnostics.sortDeterministically();
        return {{}, std::move(diagnostics)};
    }
    if (bytes.size() < endOfCentralDirectoryBytes) {
        addError(diagnostics, "cxc.archive.invalid", "CXC archive is truncated", "$/archive");
        diagnostics.sortDeterministically();
        return {{}, std::move(diagnostics)};
    }

    const auto eocdOffset = bytes.size() - endOfCentralDirectoryBytes;
    std::uint32_t signature = 0;
    std::uint16_t diskNumber = 0;
    std::uint16_t centralDiskNumber = 0;
    std::uint16_t diskEntryCount = 0;
    std::uint16_t totalEntryCount = 0;
    std::uint32_t centralSize32 = 0;
    std::uint32_t centralOffset32 = 0;
    std::uint16_t archiveCommentLength = 0;
    if (!readU32(bytes, eocdOffset, signature) || signature != endOfCentralDirectorySignature ||
        !readU16(bytes, eocdOffset + 4, diskNumber) ||
        !readU16(bytes, eocdOffset + 6, centralDiskNumber) ||
        !readU16(bytes, eocdOffset + 8, diskEntryCount) ||
        !readU16(bytes, eocdOffset + 10, totalEntryCount) ||
        !readU32(bytes, eocdOffset + 12, centralSize32) ||
        !readU32(bytes, eocdOffset + 16, centralOffset32) ||
        !readU16(bytes, eocdOffset + 20, archiveCommentLength)) {
        addError(diagnostics, "cxc.archive.invalid", "CXC end record is invalid", "$/archive");
        diagnostics.sortDeterministically();
        return {{}, std::move(diagnostics)};
    }
    if (diskNumber != 0 || centralDiskNumber != 0 || diskEntryCount != totalEntryCount) {
        addError(diagnostics, "cxc.archive.feature_unsupported",
                 "CXC archives must use a single disk", "$/archive/end");
        diagnostics.sortDeterministically();
        return {{}, std::move(diagnostics)};
    }
    if (totalEntryCount == zip64Sentinel16 || centralSize32 == zip64Sentinel32 ||
        centralOffset32 == zip64Sentinel32) {
        addError(diagnostics, "cxc.archive.feature_unsupported", "CXC does not support ZIP64",
                 "$/archive/end");
        diagnostics.sortDeterministically();
        return {{}, std::move(diagnostics)};
    }
    if (archiveCommentLength != 0) {
        addError(diagnostics, "cxc.archive.feature_unsupported",
                 "CXC archives must not contain comments", "$/archive/end");
        diagnostics.sortDeterministically();
        return {{}, std::move(diagnostics)};
    }
    if (totalEntryCount == 0 || totalEntryCount > limits.maxEntries) {
        addError(diagnostics, "cxc.budget.exceeded", "CXC archive entry count is invalid",
                 "$/archive/end");
        diagnostics.sortDeterministically();
        return {{}, std::move(diagnostics)};
    }

    const auto centralOffset = static_cast<std::size_t>(centralOffset32);
    const auto centralSize = static_cast<std::size_t>(centralSize32);
    std::size_t centralEnd = 0;
    if (!checkedAdd(centralOffset, centralSize, centralEnd) ||
        !hasRange(bytes, centralOffset, centralSize)) {
        addError(diagnostics, "cxc.archive.invalid",
                 "CXC central directory span does not match the end record", "$/archive/end");
        diagnostics.sortDeterministically();
        return {{}, std::move(diagnostics)};
    }
    if (centralEnd < eocdOffset) {
        std::uint32_t interveningSignature = 0;
        if (readU32(bytes, centralEnd, interveningSignature) &&
            (interveningSignature == zip64EndOfCentralDirectorySignature ||
             interveningSignature == zip64EndOfCentralDirectoryLocatorSignature)) {
            addError(diagnostics, "cxc.archive.feature_unsupported",
                     "CXC does not support ZIP64 end records", "$/archive/end");
            diagnostics.sortDeterministically();
            return {{}, std::move(diagnostics)};
        }
    }
    if (centralEnd != eocdOffset) {
        addError(diagnostics, "cxc.archive.invalid",
                 "CXC central directory span does not match the end record", "$/archive/end");
        diagnostics.sortDeterministically();
        return {{}, std::move(diagnostics)};
    }

    std::vector<CentralEntry> centralEntries;
    centralEntries.reserve(totalEntryCount);
    std::set<std::string, std::less<>> foldedPaths;
    auto position = centralOffset;
    for (std::size_t index = 0; index < totalEntryCount; ++index) {
        const auto fieldPath = "$/archive/central/" + std::to_string(index);
        if (!hasRange(bytes, position, centralHeaderBytes) ||
            !readU32(bytes, position, signature) || signature != centralHeaderSignature) {
            addError(diagnostics, "cxc.archive.invalid", "CXC central header is invalid",
                     fieldPath);
            diagnostics.sortDeterministically();
            return {{}, std::move(diagnostics)};
        }

        CentralEntry candidate;
        std::uint32_t compressedSize = 0;
        std::uint32_t uncompressedSize = 0;
        std::uint16_t filenameLength = 0;
        std::uint16_t extraLength = 0;
        std::uint16_t commentLength = 0;
        std::uint16_t diskStart = 0;
        std::uint32_t localOffset = 0;
        if (!readU16(bytes, position + 4, candidate.versionMadeBy) ||
            !readU16(bytes, position + 6, candidate.versionNeeded) ||
            !readU16(bytes, position + 8, candidate.flags) ||
            !readU16(bytes, position + 10, candidate.method) ||
            !readU16(bytes, position + 12, candidate.modifiedTime) ||
            !readU16(bytes, position + 14, candidate.modifiedDate) ||
            !readU32(bytes, position + 16, candidate.entry.metadata.crc32) ||
            !readU32(bytes, position + 20, compressedSize) ||
            !readU32(bytes, position + 24, uncompressedSize) ||
            !readU16(bytes, position + 28, filenameLength) ||
            !readU16(bytes, position + 30, extraLength) ||
            !readU16(bytes, position + 32, commentLength) ||
            !readU16(bytes, position + 34, diskStart) ||
            !readU16(bytes, position + 36, candidate.internalAttributes) ||
            !readU32(bytes, position + 38, candidate.externalAttributes) ||
            !readU32(bytes, position + 42, localOffset)) {
            addError(diagnostics, "cxc.archive.invalid", "CXC central header is truncated",
                     fieldPath);
            diagnostics.sortDeterministically();
            return {{}, std::move(diagnostics)};
        }
        if (compressedSize == zip64Sentinel32 || uncompressedSize == zip64Sentinel32 ||
            localOffset == zip64Sentinel32) {
            addError(diagnostics, "cxc.archive.feature_unsupported", "CXC does not support ZIP64",
                     fieldPath);
            diagnostics.sortDeterministically();
            return {{}, std::move(diagnostics)};
        }
        const auto centralExtraOffset = position + centralHeaderBytes + filenameLength;
        if (hasZip64ExtraField(bytes, centralExtraOffset, extraLength)) {
            addError(diagnostics, "cxc.archive.feature_unsupported",
                     "CXC does not support ZIP64 extra fields", fieldPath);
            diagnostics.sortDeterministically();
            return {{}, std::move(diagnostics)};
        }
        if (extraLength != 0 || commentLength != 0 || diskStart != 0) {
            addError(diagnostics, "cxc.archive.feature_unsupported",
                     "CXC entries must not use extra fields, comments, or multiple disks",
                     fieldPath);
            diagnostics.sortDeterministically();
            return {{}, std::move(diagnostics)};
        }
        if (compressedSize != uncompressedSize) {
            addError(diagnostics, "cxc.archive.invalid", "CXC Stored entry sizes do not match",
                     fieldPath);
            diagnostics.sortDeterministically();
            return {{}, std::move(diagnostics)};
        }
        if (uncompressedSize > limits.maxEntryBytes) {
            addError(diagnostics, "cxc.budget.exceeded", "CXC entry exceeds the byte limit",
                     fieldPath);
            diagnostics.sortDeterministically();
            return {{}, std::move(diagnostics)};
        }

        std::size_t recordBytes = centralHeaderBytes;
        if (!checkedAdd(recordBytes, static_cast<std::size_t>(filenameLength), recordBytes) ||
            !hasRange(bytes, position, recordBytes) || position + recordBytes > centralEnd) {
            addError(diagnostics, "cxc.archive.invalid", "CXC central entry is truncated",
                     fieldPath);
            diagnostics.sortDeterministically();
            return {{}, std::move(diagnostics)};
        }
        candidate.entry.metadata.path =
            textAt(bytes, position + centralHeaderBytes, filenameLength);
        candidate.entry.metadata.byteCount = uncompressedSize;
        candidate.entry.localHeaderOffset = localOffset;
        if (!isPortablePath(candidate.entry.metadata.path, limits.maxPathBytes,
                            limits.maxPathDepth) ||
            candidate.entry.metadata.path.ends_with('/')) {
            addError(diagnostics, "cxc.entry.path_invalid", "CXC entry path is not portable",
                     fieldPath, candidate.entry.metadata.path);
            diagnostics.sortDeterministically();
            return {{}, std::move(diagnostics)};
        }
        if (!insertUniqueArchivePath(foldedPaths, candidate.entry.metadata.path)) {
            addError(diagnostics, "cxc.entry.duplicate",
                     "CXC entry path is duplicated, conflicts by ASCII case, or overlaps a path "
                     "prefix",
                     fieldPath, candidate.entry.metadata.path);
            diagnostics.sortDeterministically();
            return {{}, std::move(diagnostics)};
        }
        if (!validateFeatureFields(diagnostics, candidate, fieldPath)) {
            diagnostics.sortDeterministically();
            return {{}, std::move(diagnostics)};
        }
        centralEntries.push_back(std::move(candidate));
        position += recordBytes;
    }
    if (position != centralEnd) {
        addError(diagnostics, "cxc.archive.invalid",
                 "CXC central directory contains unparsed bytes", "$/archive/central");
        diagnostics.sortDeterministically();
        return {{}, std::move(diagnostics)};
    }

    struct LocalRange final {
        std::size_t begin{};
        std::size_t end{};
        std::size_t centralIndex{};
    };
    std::vector<LocalRange> ranges;
    ranges.reserve(centralEntries.size());
    for (std::size_t index = 0; index < centralEntries.size(); ++index) {
        auto& candidate = centralEntries[index];
        const auto fieldPath = "$/archive/local/" + std::to_string(index);
        const auto localOffset = candidate.entry.localHeaderOffset;
        if (!hasRange(bytes, localOffset, localHeaderBytes) ||
            !readU32(bytes, localOffset, signature) || signature != localHeaderSignature) {
            addError(diagnostics, "cxc.archive.invalid", "CXC local header is invalid", fieldPath,
                     candidate.entry.metadata.path);
            diagnostics.sortDeterministically();
            return {{}, std::move(diagnostics)};
        }

        std::uint16_t versionNeeded = 0;
        std::uint16_t flags = 0;
        std::uint16_t method = 0;
        std::uint16_t modifiedTime = 0;
        std::uint16_t modifiedDate = 0;
        std::uint32_t entryCrc = 0;
        std::uint32_t compressedSize = 0;
        std::uint32_t uncompressedSize = 0;
        std::uint16_t filenameLength = 0;
        std::uint16_t extraLength = 0;
        if (!readU16(bytes, localOffset + 4, versionNeeded) ||
            !readU16(bytes, localOffset + 6, flags) || !readU16(bytes, localOffset + 8, method) ||
            !readU16(bytes, localOffset + 10, modifiedTime) ||
            !readU16(bytes, localOffset + 12, modifiedDate) ||
            !readU32(bytes, localOffset + 14, entryCrc) ||
            !readU32(bytes, localOffset + 18, compressedSize) ||
            !readU32(bytes, localOffset + 22, uncompressedSize) ||
            !readU16(bytes, localOffset + 26, filenameLength) ||
            !readU16(bytes, localOffset + 28, extraLength)) {
            addError(diagnostics, "cxc.archive.invalid", "CXC local header is truncated", fieldPath,
                     candidate.entry.metadata.path);
            diagnostics.sortDeterministically();
            return {{}, std::move(diagnostics)};
        }
        if (compressedSize == zip64Sentinel32 || uncompressedSize == zip64Sentinel32) {
            addError(diagnostics, "cxc.archive.feature_unsupported", "CXC does not support ZIP64",
                     fieldPath, candidate.entry.metadata.path);
            diagnostics.sortDeterministically();
            return {{}, std::move(diagnostics)};
        }
        const auto localExtraOffset = localOffset + localHeaderBytes + filenameLength;
        if (hasZip64ExtraField(bytes, localExtraOffset, extraLength)) {
            addError(diagnostics, "cxc.archive.feature_unsupported",
                     "CXC does not support ZIP64 extra fields", fieldPath,
                     candidate.entry.metadata.path);
            diagnostics.sortDeterministically();
            return {{}, std::move(diagnostics)};
        }
        if (extraLength != 0) {
            addError(diagnostics, "cxc.archive.feature_unsupported",
                     "CXC local entries must not use extra fields", fieldPath,
                     candidate.entry.metadata.path);
            diagnostics.sortDeterministically();
            return {{}, std::move(diagnostics)};
        }
        std::size_t dataOffset = localOffset;
        if (!checkedAdd(dataOffset, localHeaderBytes, dataOffset) ||
            !checkedAdd(dataOffset, static_cast<std::size_t>(filenameLength), dataOffset) ||
            !hasRange(bytes, localOffset, dataOffset - localOffset)) {
            addError(diagnostics, "cxc.archive.invalid", "CXC local entry is truncated", fieldPath,
                     candidate.entry.metadata.path);
            diagnostics.sortDeterministically();
            return {{}, std::move(diagnostics)};
        }
        const auto localPath = textAt(bytes, localOffset + localHeaderBytes, filenameLength);
        if (versionNeeded != candidate.versionNeeded || flags != candidate.flags ||
            method != candidate.method || modifiedTime != candidate.modifiedTime ||
            modifiedDate != candidate.modifiedDate || entryCrc != candidate.entry.metadata.crc32 ||
            compressedSize != candidate.entry.metadata.byteCount ||
            uncompressedSize != candidate.entry.metadata.byteCount ||
            localPath != candidate.entry.metadata.path) {
            addError(diagnostics, "cxc.archive.invalid",
                     "CXC local and central entry metadata do not match", fieldPath,
                     candidate.entry.metadata.path);
            diagnostics.sortDeterministically();
            return {{}, std::move(diagnostics)};
        }
        std::size_t dataEnd = dataOffset;
        if (!checkedAdd(dataEnd, static_cast<std::size_t>(candidate.entry.metadata.byteCount),
                        dataEnd) ||
            dataEnd > centralOffset || !hasRange(bytes, dataOffset, dataEnd - dataOffset)) {
            addError(diagnostics, "cxc.archive.invalid", "CXC entry data range is invalid",
                     fieldPath, candidate.entry.metadata.path);
            diagnostics.sortDeterministically();
            return {{}, std::move(diagnostics)};
        }
        candidate.entry.dataOffset = dataOffset;
        const auto data = bytes.subspan(dataOffset, dataEnd - dataOffset);
        if (crc32(data) != candidate.entry.metadata.crc32) {
            addError(diagnostics, "cxc.archive.invalid", "CXC entry CRC32 does not match",
                     fieldPath, candidate.entry.metadata.path);
            diagnostics.sortDeterministically();
            return {{}, std::move(diagnostics)};
        }
        candidate.entry.metadata.sha256 = sha256Hex(data);
        ranges.push_back(LocalRange{localOffset, dataEnd, index});
    }

    std::ranges::sort(ranges, {}, &LocalRange::begin);
    std::size_t expectedOffset = 0;
    for (const auto& range : ranges) {
        if (range.begin != expectedOffset || range.end < range.begin) {
            addError(diagnostics, "cxc.archive.invalid",
                     range.begin < expectedOffset ? "CXC entry ranges overlap"
                                                  : "CXC archive contains unlisted padding",
                     "$/archive/local", centralEntries[range.centralIndex].entry.metadata.path);
            diagnostics.sortDeterministically();
            return {{}, std::move(diagnostics)};
        }
        expectedOffset = range.end;
    }
    if (expectedOffset != centralOffset) {
        addError(diagnostics, "cxc.archive.invalid",
                 "CXC archive contains bytes outside entry ranges", "$/archive/local");
        diagnostics.sortDeterministically();
        return {{}, std::move(diagnostics)};
    }

    std::vector<Zip32Entry> entries;
    entries.reserve(centralEntries.size());
    for (auto& candidate : centralEntries) {
        entries.push_back(std::move(candidate.entry));
    }
    diagnostics.sortDeterministically();
    return {std::move(entries), std::move(diagnostics)};
}

auto writeCanonicalZip32(std::span<const std::pair<std::string, std::vector<std::byte>>> entries,
                         const CxcPackageLimits& limits) -> core::Result<std::vector<std::byte>> {
    if (entries.empty() || entries.size() > limits.maxEntries ||
        entries.size() >= zip64Sentinel16) {
        return core::unexpected(
            core::Error{"cxc.budget.exceeded", "CXC archive entry count is invalid"});
    }

    std::set<std::string, std::less<>> foldedPaths;
    std::size_t totalBytes = endOfCentralDirectoryBytes;
    for (const auto& [path, data] : entries) {
        if (!isPortablePath(path, limits.maxPathBytes, limits.maxPathDepth) ||
            path.ends_with('/')) {
            return core::unexpected(
                core::Error{"cxc.entry.path_invalid", "CXC entry path is not portable"}.withContext(
                    "path", path));
        }
        if (path.size() > std::numeric_limits<std::uint16_t>::max()) {
            return core::unexpected(
                core::Error{"cxc.budget.exceeded", "CXC entry path exceeds the ZIP32 field limit"}
                    .withContext("path", path));
        }
        if (!insertUniqueArchivePath(foldedPaths, path)) {
            return core::unexpected(
                core::Error{"cxc.entry.duplicate",
                            "CXC entry path is duplicated, conflicts by ASCII case, or overlaps a "
                            "path prefix"}
                    .withContext("path", path));
        }
        if (data.size() > limits.maxEntryBytes ||
            data.size() >= static_cast<std::size_t>(zip64Sentinel32)) {
            return core::unexpected(
                core::Error{"cxc.budget.exceeded", "CXC entry exceeds the byte limit"}.withContext(
                    "path", path));
        }
        std::size_t localSize = localHeaderBytes;
        std::size_t centralSize = centralHeaderBytes;
        if (!checkedAdd(localSize, path.size(), localSize) ||
            !checkedAdd(localSize, data.size(), localSize) ||
            !checkedAdd(centralSize, path.size(), centralSize) ||
            !checkedAdd(totalBytes, localSize, totalBytes) ||
            !checkedAdd(totalBytes, centralSize, totalBytes)) {
            return core::unexpected(
                core::Error{"cxc.budget.exceeded", "CXC archive size overflowed"});
        }
    }
    if (totalBytes > limits.maxPackageBytes || totalBytes >= zip64Sentinel32) {
        return core::unexpected(
            core::Error{"cxc.budget.exceeded", "CXC archive exceeds the byte limit"});
    }

    struct WrittenEntry final {
        std::string_view path;
        std::span<const std::byte> data;
        std::uint32_t crc{};
        std::uint32_t localOffset{};
    };
    std::vector<WrittenEntry> writtenEntries;
    writtenEntries.reserve(entries.size());
    std::vector<std::byte> result;
    result.reserve(totalBytes);
    for (const auto& [path, data] : entries) {
        const auto offset = static_cast<std::uint32_t>(result.size());
        const auto entryCrc = crc32(data);
        appendU32(result, localHeaderSignature);
        appendU16(result, 10);
        appendU16(result, 0);
        appendU16(result, 0);
        appendU16(result, 0);
        appendU16(result, 0x0021U);
        appendU32(result, entryCrc);
        appendU32(result, static_cast<std::uint32_t>(data.size()));
        appendU32(result, static_cast<std::uint32_t>(data.size()));
        appendU16(result, static_cast<std::uint16_t>(path.size()));
        appendU16(result, 0);
        appendText(result, path);
        result.insert(result.end(), data.begin(), data.end());
        writtenEntries.push_back(WrittenEntry{path, data, entryCrc, offset});
    }

    const auto centralOffset = static_cast<std::uint32_t>(result.size());
    for (const auto& entry : writtenEntries) {
        appendU32(result, centralHeaderSignature);
        appendU16(result, 0x000AU);
        appendU16(result, 10);
        appendU16(result, 0);
        appendU16(result, 0);
        appendU16(result, 0);
        appendU16(result, 0x0021U);
        appendU32(result, entry.crc);
        appendU32(result, static_cast<std::uint32_t>(entry.data.size()));
        appendU32(result, static_cast<std::uint32_t>(entry.data.size()));
        appendU16(result, static_cast<std::uint16_t>(entry.path.size()));
        appendU16(result, 0);
        appendU16(result, 0);
        appendU16(result, 0);
        appendU16(result, 0);
        appendU32(result, 0);
        appendU32(result, entry.localOffset);
        appendText(result, entry.path);
    }
    const auto centralSize = static_cast<std::uint32_t>(result.size() - centralOffset);
    appendU32(result, endOfCentralDirectorySignature);
    appendU16(result, 0);
    appendU16(result, 0);
    appendU16(result, static_cast<std::uint16_t>(writtenEntries.size()));
    appendU16(result, static_cast<std::uint16_t>(writtenEntries.size()));
    appendU32(result, centralSize);
    appendU32(result, centralOffset);
    appendU16(result, 0);

    if (result.size() != totalBytes) {
        return core::unexpected(
            core::Error{"cxc.archive.invalid", "CXC canonical writer size accounting failed"});
    }
    return result;
}

auto verifyWithMinizip(std::span<const std::byte> bytes, std::span<const Zip32Entry> expected)
    -> core::Result<void> {
    if (bytes.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
        return minizipFailure();
    }
    auto* reader = mz_zip_reader_create();
    if (reader == nullptr) {
        return minizipFailure();
    }
    const auto cleanup = [&reader]() {
        if (reader != nullptr) {
            static_cast<void>(mz_zip_reader_close(reader));
            mz_zip_reader_delete(&reader);
        }
    };
    if (mz_zip_reader_open_buffer(reader, reinterpret_cast<const std::uint8_t*>(bytes.data()),
                                  static_cast<std::int32_t>(bytes.size()), 0) != MZ_OK) {
        cleanup();
        return minizipFailure();
    }

    auto status = mz_zip_reader_goto_first_entry(reader);
    for (std::size_t index = 0; index < expected.size(); ++index) {
        if (status != MZ_OK) {
            cleanup();
            return minizipFailure();
        }
        mz_zip_file* info = nullptr;
        if (mz_zip_reader_entry_get_info(reader, &info) != MZ_OK || info == nullptr ||
            info->filename == nullptr || expected[index].metadata.path != info->filename ||
            info->compression_method != 0 || info->flag != 0 ||
            info->compressed_size !=
                static_cast<std::int64_t>(expected[index].metadata.byteCount) ||
            info->uncompressed_size !=
                static_cast<std::int64_t>(expected[index].metadata.byteCount) ||
            info->crc != expected[index].metadata.crc32 ||
            mz_zip_reader_entry_open(reader) != MZ_OK) {
            cleanup();
            return minizipFailure();
        }

        std::vector<std::byte> extracted(static_cast<std::size_t>(info->uncompressed_size));
        std::size_t offset = 0;
        while (offset < extracted.size()) {
            const auto count =
                std::min(extracted.size() - offset,
                         static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()));
            const auto read = mz_zip_reader_entry_read(reader, extracted.data() + offset,
                                                       static_cast<std::int32_t>(count));
            if (read <= 0) {
                cleanup();
                return minizipFailure();
            }
            offset += static_cast<std::size_t>(read);
        }
        const auto source =
            bytes.subspan(expected[index].dataOffset,
                          static_cast<std::size_t>(expected[index].metadata.byteCount));
        if (mz_zip_reader_entry_close(reader) != MZ_OK || !std::ranges::equal(extracted, source)) {
            cleanup();
            return minizipFailure();
        }
        status = mz_zip_reader_goto_next_entry(reader);
    }
    if (status != MZ_END_OF_LIST) {
        cleanup();
        return minizipFailure();
    }
    cleanup();
    return {};
}

} // namespace cuexis::cxc::detail

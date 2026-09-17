#include <cuexis/chart/packed_chart_io.hpp>

#include <cuexis/chart/packed_chart_primitives.hpp>
#include <cuexis/core/error.hpp>

#include "packed_limits_internal.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <fstream>
#include <limits>
#include <map>
#include <span>
#include <string>
#include <system_error>
#include <utility>

#if defined(_WIN32)
#include <Windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace cuexis::chart {
namespace {

namespace fs = std::filesystem;
using packed::ByteReader;

[[nodiscard]] auto error(std::string code, std::string message) -> core::Error {
    return core::Error{std::move(code), std::move(message)};
}

[[nodiscard]] auto temporarySibling(const fs::path& target, std::uint64_t attempt) -> fs::path {
    static std::atomic<std::uint64_t> sequence{0};
    const auto ticks =
        static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto token = ticks ^ sequence.fetch_add(1, std::memory_order_relaxed) ^ attempt;
    return target.parent_path() /
           (target.filename().native() + fs::path{".tmp." + std::to_string(token)}.native());
}

[[nodiscard]] auto checkedRange(std::size_t offset, std::size_t length, std::size_t total) -> bool {
    return offset <= total && length <= total - offset;
}

[[nodiscard]] auto parseSizing(std::span<const std::byte> bytes,
                               const packed::PackedChartStatistics& statistics)
    -> core::Result<PackedChartSizing> {
    if (bytes.size() < 96U) {
        return core::unexpected(error("packed.io.truncated_header", "Packed header is truncated"));
    }
    ByteReader header{bytes};
    auto magic = header.readBytes(8);
    if (!magic) {
        return core::unexpected(std::move(magic.error()));
    }
    constexpr std::array<std::byte, 8> expected{std::byte{'C'}, std::byte{'X'}, std::byte{'P'},
                                                std::byte{'K'}, std::byte{'5'}, std::byte{0},
                                                std::byte{0},   std::byte{0}};
    if (!std::equal(expected.begin(), expected.end(), magic->begin())) {
        return core::unexpected(error("packed.io.invalid_magic", "Packed magic is invalid"));
    }
    auto version = header.readU16();
    auto headerBytes = header.readU16();
    auto flags = header.readU32();
    auto totalBytes = header.readU32();
    auto directoryOffset = header.readU32();
    auto directoryCount = header.readU32();
    auto directoryBytes = header.readU32();
    if (!version || !headerBytes || !flags || !totalBytes || !directoryOffset || !directoryCount ||
        !directoryBytes) {
        return core::unexpected(error("packed.io.truncated_header", "Packed header is truncated"));
    }
    if (*version != 1U || *headerBytes != 96U || *directoryOffset != 96U ||
        *totalBytes != bytes.size()) {
        return core::unexpected(
            error("packed.io.header_mismatch", "Packed header values mismatch"));
    }
    const auto directoryBytesExpected = static_cast<std::uint64_t>(*directoryCount) * 32U;
    if (directoryBytesExpected > std::numeric_limits<std::uint32_t>::max() ||
        *directoryBytes != static_cast<std::uint32_t>(directoryBytesExpected) ||
        !checkedRange(*directoryOffset, *directoryBytes, bytes.size())) {
        return core::unexpected(error("packed.io.directory_bounds", "Packed directory is invalid"));
    }

    PackedChartSizing sizing{.statistics = statistics,
                             .headerBytes = *headerBytes,
                             .directoryBytes = *directoryBytes,
                             .sections = {}};
    sizing.sections.reserve(*directoryCount);
    ByteReader directory{bytes.subspan(*directoryOffset, *directoryBytes)};
    std::size_t previousEnd = *directoryOffset + *directoryBytes;
    for (std::uint32_t index = 0; index < *directoryCount; ++index) {
        auto typeBytes = directory.readBytes(4);
        auto codec = directory.readU8();
        auto sectionFlags = directory.readU8();
        auto reserved = directory.readU16();
        auto offset = directory.readU32();
        auto encoded = directory.readU32();
        auto decoded = directory.readU32();
        auto records = directory.readU32();
        auto crc = directory.readU32();
        auto reserved2 = directory.readU32();
        if (!typeBytes || !codec || !sectionFlags || !reserved || !offset || !encoded || !decoded ||
            !records || !crc || !reserved2 || *codec != 0U || *reserved != 0U || *reserved2 != 0U ||
            *sectionFlags > 1U || !checkedRange(*offset, *encoded, bytes.size()) ||
            *offset != previousEnd) {
            return core::unexpected(
                error("packed.io.directory_invalid", "Packed directory entry is invalid"));
        }
        std::string type;
        type.reserve(4);
        for (const auto byte : *typeBytes) {
            const auto value = std::to_integer<unsigned char>(byte);
            if (value < 0x20U || value > 0x7eU) {
                return core::unexpected(
                    error("packed.io.section_type", "Packed section type is invalid"));
            }
            type.push_back(static_cast<char>(value));
        }
        sizing.sections.push_back(
            PackedSectionSize{std::move(type), *offset, *encoded, *decoded, *records});
        previousEnd = static_cast<std::size_t>(*offset) + *encoded;
    }
    if (previousEnd != bytes.size()) {
        return core::unexpected(
            error("packed.io.trailing_bytes", "Packed file has trailing bytes"));
    }
    return sizing;
}

[[nodiscard]] auto writeTemporary(const fs::path& path, std::span<const std::byte> bytes)
    -> core::Result<void> {
#if defined(_WIN32)
    const HANDLE handle = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                      FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return core::unexpected(
            error("packed.io.temp_create_failed", "Packed temporary file could not be created"));
    }
    bool succeeded = true;
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const auto remaining = std::min<std::size_t>(bytes.size() - offset, 0x7fffffffU);
        DWORD written = 0;
        if (WriteFile(handle, bytes.data() + offset, static_cast<DWORD>(remaining), &written,
                      nullptr) == 0 ||
            written == 0) {
            succeeded = false;
            break;
        }
        offset += written;
    }
    if (succeeded && FlushFileBuffers(handle) == 0) {
        succeeded = false;
    }
    const bool closed = CloseHandle(handle) != 0;
    if (!succeeded || !closed) {
        return core::unexpected(
            error("packed.io.temp_write_failed", "Packed temporary file write failed"));
    }
#else
    const int descriptor = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (descriptor < 0) {
        return core::unexpected(
            error("packed.io.temp_create_failed", "Packed temporary file could not be created"));
    }
    bool succeeded = true;
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const auto written = ::write(descriptor, bytes.data() + offset, bytes.size() - offset);
        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written <= 0) {
            succeeded = false;
            break;
        }
        offset += static_cast<std::size_t>(written);
    }
    if (succeeded && ::fsync(descriptor) != 0) {
        succeeded = false;
    }
    const bool closed = ::close(descriptor) == 0;
    if (!succeeded || !closed) {
        return core::unexpected(
            error("packed.io.temp_write_failed", "Packed temporary file write failed"));
    }
#endif
    return {};
}

[[nodiscard]] auto replaceAtomically(const fs::path& source, const fs::path& target)
    -> core::Result<void> {
#if defined(_WIN32)
    if (MoveFileExW(source.c_str(), target.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == 0) {
        return core::unexpected(
            error("packed.io.replace_failed", "Packed atomic replacement failed"));
    }
#else
    std::error_code renameError;
    fs::rename(source, target, renameError);
    if (renameError) {
        return core::unexpected(
            error("packed.io.replace_failed", "Packed atomic replacement failed"));
    }
#endif
    return {};
}

} // namespace

auto PackedChartWriter::size(const CanonicalSemanticChart& chart,
                             packed::PackedChartProfile profile, PackedChartLimits limits)
    -> core::Result<PackedChartSizing> {
    // packed::encode owns the profile, budget and file-envelope gates, so the bridge never
    // produces bytes that already violate a budget it was given.
    auto bytes = packed::encode(chart, profile, limits);
    if (!bytes) {
        return core::unexpected(std::move(bytes.error()));
    }
    auto statistics = packed::inspect(*bytes, limits);
    if (!statistics) {
        return core::unexpected(std::move(statistics.error()));
    }
    return parseSizing(*bytes, *statistics);
}

auto PackedChartWriter::writeAtomic(const CanonicalSemanticChart& chart, const fs::path& target,
                                    PackedChartWriteOptions options)
    -> core::Result<PackedChartSizing> {
    auto bytes = packed::encode(chart, options.profile, options.limits);
    if (!bytes) {
        return core::unexpected(std::move(bytes.error()));
    }
    auto statistics = packed::inspect(*bytes, options.limits);
    if (!statistics) {
        return core::unexpected(std::move(statistics.error()));
    }
    auto sizing = parseSizing(*bytes, *statistics);
    if (!sizing) {
        return core::unexpected(std::move(sizing.error()));
    }
    std::error_code statusError;
    const auto parent =
        target.has_parent_path() ? target.parent_path() : fs::current_path(statusError);
    if (statusError || !fs::is_directory(parent, statusError)) {
        return core::unexpected(
            error("packed.io.parent_missing", "Packed output parent directory is missing"));
    }
    fs::path temporary;
    core::Result<void> written = core::unexpected(
        error("packed.io.temp_create_failed", "Packed temporary file creation failed"));
    for (std::uint64_t attempt = 0; attempt < 128U; ++attempt) {
        temporary = temporarySibling(target, attempt);
        written = writeTemporary(temporary, *bytes);
        if (written) {
            break;
        }
        std::error_code ignored;
        fs::remove(temporary, ignored);
    }
    if (!written) {
        return core::unexpected(std::move(written.error()));
    }
    auto cleanup = [&temporary]() {
        std::error_code ignored;
        fs::remove(temporary, ignored);
    };
    auto replaced = replaceAtomically(temporary, target);
    if (!replaced) {
        cleanup();
        return core::unexpected(std::move(replaced.error()));
    }
    return *sizing;
}

auto PackedChartReader::decode(std::span<const std::byte> bytes, PackedChartLimits limits)
    -> core::Result<CanonicalSemanticChart> {
    // The file budget is resolved with the frozen Foundation ceilings, so a caller cannot read an
    // artifact past 16 MiB by passing a larger limit.
    if (bytes.size() > packed::limits_detail::effectiveLimits(limits).maxPackedFileBytes) {
        return core::unexpected(
            error("packed.budget.file_bytes", "Packed chart exceeds the file byte budget"));
    }
    // The file bridge deliberately owns no second section registry or structural decoder: it
    // delegates to packed::decode so inspect(), packed::decode and PackedChartReader::decode
    // cannot disagree about the registered subset or report a misleading diagnostic.
    return packed::decode(bytes, limits);
}

auto PackedChartReader::read(const fs::path& source, PackedChartLimits limits)
    -> core::Result<CanonicalSemanticChart> {
    std::error_code statusError;
    const auto size = fs::file_size(source, statusError);
    if (statusError) {
        return core::unexpected(
            error("packed.io.open_failed", "Packed input file could not be stat'ed"));
    }
    if (size > packed::limits_detail::effectiveLimits(limits).maxPackedFileBytes) {
        return core::unexpected(
            error("packed.budget.file_bytes", "Packed chart exceeds the file byte budget"));
    }
    std::ifstream input(source, std::ios::binary);
    if (!input) {
        return core::unexpected(
            error("packed.io.open_failed", "Packed input file could not be opened"));
    }
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    if (size != 0U) {
        input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
    }
    if (!input && !input.eof()) {
        return core::unexpected(
            error("packed.io.read_failed", "Packed input file could not be read"));
    }
    return decode(bytes, limits);
}

} // namespace cuexis::chart

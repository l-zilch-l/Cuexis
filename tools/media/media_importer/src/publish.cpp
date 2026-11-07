#include "publish.hpp"

#include <cuexis/media_import/media_import.hpp>
#include <cuexis_internal/sha256.hpp>

#include <algorithm>
#include <array>
#include <fstream>
#include <system_error>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

namespace cuexis::media_importer {
namespace {

[[nodiscard]] auto ioError(std::string code, std::string message) -> core::Error {
    return core::Error{std::move(code), std::move(message)};
}

[[nodiscard]] auto toHex(const std::array<std::uint8_t, 32>& digest) -> std::string {
    constexpr std::string_view digits = "0123456789abcdef";
    std::string out;
    out.reserve(64);
    for (const auto value : digest) {
        out.push_back(digits[value >> 4U]);
        out.push_back(digits[value & 0x0FU]);
    }
    return out;
}

} // namespace

auto currentProcessId() -> long {
#if defined(_WIN32)
    return static_cast<long>(::_getpid());
#else
    return static_cast<long>(::getpid());
#endif
}

auto readFileBounded(const std::filesystem::path& path, std::uint64_t limit)
    -> core::Result<std::vector<std::byte>> {
    std::error_code statusError;
    const auto size = std::filesystem::file_size(path, statusError);
    if (statusError) {
        return core::unexpected(
            ioError("media.io.input_unavailable", "Input file could not be inspected"));
    }
    if (size > limit) {
        return core::unexpected(
            core::Error{"media.source.limit", "Encoded source exceeds the source budget"}
                .withContext("limit", std::to_string(limit))
                .withContext("actual", std::to_string(size)));
    }
    std::ifstream stream{path, std::ios::binary};
    if (!stream) {
        return core::unexpected(
            ioError("media.io.input_unavailable", "Input file could not be opened"));
    }
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    if (size > 0) {
        stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
        if (stream.gcount() != static_cast<std::streamsize>(size)) {
            return core::unexpected(
                ioError("media.io.input_truncated", "Input file shrank while it was being read"));
        }
    }
    return bytes;
}

auto publishImmutable(const std::filesystem::path& target, std::span<const std::byte> bytes)
    -> core::Result<void> {
    std::error_code existsError;
    const bool exists = std::filesystem::exists(target, existsError);
    if (existsError) {
        return core::unexpected(
            ioError("media.publish.unavailable", "Publication target could not be inspected"));
    }
    if (exists) {
        auto existing = readFileBounded(target, 1024ULL * 1024ULL * 1024ULL);
        if (!existing) {
            return core::unexpected(
                ioError("media.publish.unavailable", "Existing publication could not be read"));
        }
        if (existing->size() == bytes.size() &&
            std::equal(existing->begin(), existing->end(), bytes.begin())) {
            return {}; // identical content is already published
        }
        return core::unexpected(
            ioError("media.publish.immutable_conflict",
                    "Publication target already holds different bytes and is immutable"));
    }

    auto directory = target.parent_path();
    if (directory.empty()) {
        directory = std::filesystem::current_path();
    }
    std::error_code directoryError;
    std::filesystem::create_directories(directory, directoryError);
    if (directoryError) {
        return core::unexpected(
            ioError("media.publish.unavailable", "Publication directory could not be created"));
    }

    const auto temporary =
        directory / (target.filename().string() + ".tmp" + std::to_string(currentProcessId()));
    {
        std::ofstream stream{temporary, std::ios::binary | std::ios::trunc};
        if (!stream) {
            return core::unexpected(ioError("media.publish.unavailable",
                                            "Temporary publication file could not be created"));
        }
        if (!bytes.empty()) {
            stream.write(reinterpret_cast<const char*>(bytes.data()),
                         static_cast<std::streamsize>(bytes.size()));
        }
        stream.flush();
        if (!stream) {
            std::error_code removeError;
            std::filesystem::remove(temporary, removeError);
            return core::unexpected(ioError("media.publish.unavailable",
                                            "Temporary publication file could not be written"));
        }
    }

    std::error_code renameError;
    std::filesystem::rename(temporary, target, renameError);
    if (renameError) {
        std::error_code removeError;
        std::filesystem::remove(temporary, removeError);
        return core::unexpected(ioError("media.publish.unavailable",
                                        "Temporary publication file could not be renamed"));
    }
    return {};
}

auto hashFile(const std::filesystem::path& path) -> core::Result<std::string> {
    std::ifstream stream{path, std::ios::binary};
    if (!stream) {
        return core::unexpected(ioError("media.io.input_unavailable",
                                        "Published file could not be opened for hashing"));
    }
    cuexis::core::detail::Sha256 digest;
    std::array<char, 64U * 1024U> buffer{};
    while (stream) {
        stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto read = stream.gcount();
        if (read > 0) {
            digest.update(std::span<const std::byte>{
                reinterpret_cast<const std::byte*>(buffer.data()), static_cast<std::size_t>(read)});
        }
    }
    if (!stream.eof()) {
        return core::unexpected(
            ioError("media.io.input_unavailable", "Published file could not be read for hashing"));
    }
    return toHex(digest.finish());
}

auto promoteTemporary(const std::filesystem::path& temporary, const std::filesystem::path& target,
                      std::string_view identity) -> core::Result<void> {
    std::error_code existsError;
    const bool exists = std::filesystem::exists(target, existsError);
    if (existsError) {
        return core::unexpected(
            ioError("media.publish.unavailable", "Publication target could not be inspected"));
    }
    if (exists) {
        // The target name is the content hash of the temporary file. An existing target is only a
        // no-op when it still hashes to that name; anything else is an immutable conflict.
        auto existing = hashFile(target);
        if (!existing) {
            return core::unexpected(
                ioError("media.publish.unavailable", "Existing publication could not be read"));
        }
        if (*existing == identity) {
            std::error_code removeError;
            std::filesystem::remove(temporary, removeError);
            return {};
        }
        std::error_code removeError;
        std::filesystem::remove(temporary, removeError);
        return core::unexpected(
            ioError("media.publish.immutable_conflict",
                    "Publication target already holds different bytes and is immutable"));
    }
    std::error_code renameError;
    std::filesystem::rename(temporary, target, renameError);
    if (renameError) {
        return core::unexpected(ioError("media.publish.unavailable",
                                        "Temporary publication file could not be renamed"));
    }
    return {};
}

} // namespace cuexis::media_importer

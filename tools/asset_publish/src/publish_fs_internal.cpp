#include "publish_fs_internal.hpp"

#include <cuexis_internal/sha256.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#include <Windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace cuexis::tools::detail {
namespace {

namespace fs = std::filesystem;

[[nodiscard]] auto platformFailure(std::string_view code, std::string message, int platformError)
    -> core::Error {
    return publishError(code, std::move(message))
        .withContext("platform_error", std::to_string(platformError));
}

[[nodiscard]] auto readEnvValue(std::string_view name) -> std::string {
    const std::string key{name};
    // _dupenv_s is an MSVC CRT extension: MinGW has no such symbol, so it must not be selected by
    // _WIN32.
#if defined(_MSC_VER)
    char* value = nullptr;
    std::size_t size = 0;
    if (::_dupenv_s(&value, &size, key.c_str()) != 0 || value == nullptr) {
        return {};
    }
    std::string out{value};
    std::free(value);
    return out;
#else
    const char* value = std::getenv(key.c_str());
    return value == nullptr ? std::string{} : std::string{value};
#endif
}

} // namespace

auto publishError(std::string_view code, std::string message) -> core::Error {
    return core::Error{std::string{code}, std::move(message)};
}

#if defined(_WIN32)
auto writeFileExclusive(const fs::path& file, std::span<const std::byte> bytes)
    -> core::Result<void> {
    const HANDLE handle = CreateFileW(file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                      FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return core::unexpected(platformFailure(publishIoCode, "Publication file creation failed",
                                                static_cast<int>(GetLastError()))
                                    .withContext("path", file.generic_string()));
    }

    bool succeeded = true;
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const auto remaining =
            std::min<std::size_t>(bytes.size() - offset, static_cast<std::size_t>(0x7FFFFFFFU));
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
    const int platformError = succeeded ? 0 : static_cast<int>(GetLastError());
    const bool closed = CloseHandle(handle) != 0;
    if (!succeeded || !closed) {
        return core::unexpected(
            platformFailure(publishIoCode, "Publication file write failed", platformError)
                .withContext("path", file.generic_string()));
    }
    return {};
}

auto replaceAtomically(const fs::path& source, const fs::path& target) -> core::Result<void> {
    if (MoveFileExW(source.c_str(), target.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == 0) {
        return core::unexpected(platformFailure(publishReplaceCode, "Atomic replacement failed",
                                                static_cast<int>(GetLastError()))
                                    .withContext("source", source.generic_string())
                                    .withContext("target", target.generic_string()));
    }
    return {};
}

auto syncDirectory(const fs::path& directory) -> core::Result<void> {
    // MoveFileEx with WRITE_THROUGH already flushes the directory entry on Windows.
    (void)directory;
    return {};
}
#else
auto writeFileExclusive(const fs::path& file, std::span<const std::byte> bytes)
    -> core::Result<void> {
    const int descriptor = ::open(file.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (descriptor < 0) {
        return core::unexpected(
            platformFailure(publishIoCode, "Publication file creation failed", errno)
                .withContext("path", file.generic_string()));
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
    const int platformError = succeeded ? 0 : errno;
    const bool closed = ::close(descriptor) == 0;
    if (!succeeded || !closed) {
        return core::unexpected(
            platformFailure(publishIoCode, "Publication file write failed", platformError)
                .withContext("path", file.generic_string()));
    }
    return {};
}

auto replaceAtomically(const fs::path& source, const fs::path& target) -> core::Result<void> {
    if (::rename(source.c_str(), target.c_str()) != 0) {
        return core::unexpected(
            platformFailure(publishReplaceCode, "Atomic replacement failed", errno)
                .withContext("source", source.generic_string())
                .withContext("target", target.generic_string()));
    }
    return {};
}

auto syncDirectory(const fs::path& directory) -> core::Result<void> {
    const int descriptor = ::open(directory.c_str(), O_RDONLY);
    if (descriptor < 0) {
        return core::unexpected(
            platformFailure(publishIoCode, "Publication directory could not be opened", errno)
                .withContext("path", directory.generic_string()));
    }
    const int result = ::fsync(descriptor);
    const int platformError = result == 0 ? 0 : errno;
    ::close(descriptor);
    if (result != 0) {
        return core::unexpected(platformFailure(publishIoCode,
                                                "Publication directory could not be flushed",
                                                platformError)
                                    .withContext("path", directory.generic_string()));
    }
    return {};
}
#endif

auto readFileBytes(const fs::path& file, std::size_t maxBytes)
    -> core::Result<std::vector<std::byte>> {
    std::error_code status;
    const auto size = fs::file_size(file, status);
    if (status) {
        return core::unexpected(publishError(publishIoCode, "Publication file is not readable")
                                    .withContext("path", file.generic_string()));
    }
    if (size > maxBytes) {
        return core::unexpected(publishError(publishIoCode, "Publication file exceeds the budget")
                                    .withContext("path", file.generic_string())
                                    .withContext("size_bytes", std::to_string(size)));
    }
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    std::FILE* stream = nullptr;
#if defined(_WIN32)
    if (::_wfopen_s(&stream, file.c_str(), L"rb") != 0) {
        stream = nullptr;
    }
#else
    stream = std::fopen(file.c_str(), "rb");
#endif
    if (stream == nullptr) {
        return core::unexpected(publishError(publishIoCode, "Publication file could not be opened")
                                    .withContext("path", file.generic_string()));
    }
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const auto read = std::fread(bytes.data() + offset, 1, bytes.size() - offset, stream);
        if (read == 0) {
            break;
        }
        offset += read;
    }
    std::fclose(stream);
    if (offset != bytes.size()) {
        return core::unexpected(publishError(publishIoCode, "Publication file read was short")
                                    .withContext("path", file.generic_string()));
    }
    return bytes;
}

auto isRegularFile(const fs::path& path) noexcept -> bool {
    std::error_code status;
    return fs::is_regular_file(path, status) && !status;
}

auto isDirectory(const fs::path& path) noexcept -> bool {
    std::error_code status;
    return fs::is_directory(path, status) && !status;
}

void removeTreeQuiet(const fs::path& path) noexcept {
    std::error_code status;
    fs::remove_all(path, status);
}

auto uniqueSibling(const fs::path& target, std::string_view role) -> fs::path {
    static std::atomic<std::uint64_t> sequence{0};
    const auto ticks =
        static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto token = ticks ^ sequence.fetch_add(1, std::memory_order_relaxed);
    std::string infix{"."};
    infix.append(role);
    infix.append(".tmp.");
    infix.append(std::to_string(token));
    // The extension is preserved, because a loader that validates the suffix (for example
    // CxcPackageLoader::loadFile, which requires ".cxc") must accept the temporary file.
    fs::path name = target.stem();
    name += fs::path{infix};
    name += target.extension();
    return target.parent_path() / name;
}

auto isPortableRelativePath(std::string_view path) noexcept -> bool {
    if (path.empty() || path.size() > publishMaxPathBytes) {
        return false;
    }
    if (path.front() == '/' || path.back() == '/') {
        return false;
    }
    const auto isAlphaNumeric = [](char character) {
        return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') ||
               (character >= '0' && character <= '9');
    };
    if (!isAlphaNumeric(path.front())) {
        return false;
    }
    std::size_t segmentStart = 0;
    for (std::size_t index = 0; index < path.size(); ++index) {
        const char character = path[index];
        if (character == '/') {
            const std::string_view segment = path.substr(segmentStart, index - segmentStart);
            if (segment.empty() || segment == "." || segment == "..") {
                return false;
            }
            segmentStart = index + 1;
            continue;
        }
        // A portable entry path is a relative path of non-empty segments built from the portable
        // character set. A space is not allowed, because a path that needs quoting is not portable
        // through every host shell. '%' is allowed so a percent-encoded AssetId name can be
        // published unchanged.
        const bool allowed = isAlphaNumeric(character) || character == '.' || character == '_' ||
                             character == '-' || character == '%';
        if (!allowed) {
            return false;
        }
    }
    const std::string_view last = path.substr(segmentStart);
    if (last.empty() || last == "." || last == "..") {
        return false;
    }
    return true;
}

auto isLowerHex(std::string_view value, std::size_t length) noexcept -> bool {
    if (value.size() != length) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](char character) {
        return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f');
    });
}

auto toHex(std::span<const std::byte> bytes) -> std::string {
    constexpr std::string_view digits = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (const std::byte value : bytes) {
        const auto byte = std::to_integer<std::uint8_t>(value);
        out.push_back(digits[byte >> 4]);
        out.push_back(digits[byte & 0x0FU]);
    }
    return out;
}

auto sha256Hex(std::span<const std::byte> bytes) -> std::string {
    return cuexis::core::detail::sha256Hex(bytes);
}

auto testFailureEnabled(std::string_view name) noexcept -> bool {
#if defined(CUEXIS_ASSET_PUBLISH_TESTING)
    const auto value = readEnvValue(name);
    return !value.empty() && value[0] == '1';
#else
    (void)name;
    return false;
#endif
}

auto testFailureTargetMatches(std::string_view name, std::string_view fileName) noexcept -> bool {
#if defined(CUEXIS_ASSET_PUBLISH_TESTING)
    const auto value = readEnvValue(name);
    return !value.empty() && std::string_view{value} == fileName;
#else
    (void)name;
    (void)fileName;
    return false;
#endif
}

} // namespace cuexis::tools::detail

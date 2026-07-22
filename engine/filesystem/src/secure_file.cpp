#include <cuexis/filesystem/secure_file.hpp>

#include <cuexis/core/error.hpp>

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <Windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace cuexis::filesystem {
namespace {

[[nodiscard]] auto fileError(const std::string& code, std::string message,
                             const std::filesystem::path& path) -> core::Error {
    return core::Error{code, std::move(message)}.withContext("file", path.filename().string());
}

#if defined(_WIN32)

class UniqueHandle final {
  public:
    explicit UniqueHandle(HANDLE handle = INVALID_HANDLE_VALUE) noexcept : handle_(handle) {}
    ~UniqueHandle() {
        if (handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_);
        }
    }

    UniqueHandle(const UniqueHandle&) = delete;
    auto operator=(const UniqueHandle&) -> UniqueHandle& = delete;

    [[nodiscard]] auto get() const noexcept -> HANDLE {
        return handle_;
    }

  private:
    HANDLE handle_;
};

[[nodiscard]] auto finalPath(HANDLE handle) -> std::optional<std::filesystem::path> {
    constexpr DWORD flags = FILE_NAME_NORMALIZED | VOLUME_NAME_DOS;
    const DWORD required = GetFinalPathNameByHandleW(handle, nullptr, 0, flags);
    if (required == 0) {
        return std::nullopt;
    }
    std::wstring buffer(static_cast<std::size_t>(required) + 1U, L'\0');
    const DWORD written =
        GetFinalPathNameByHandleW(handle, buffer.data(), static_cast<DWORD>(buffer.size()), flags);
    if (written == 0 || written >= buffer.size()) {
        return std::nullopt;
    }
    buffer.resize(written);
    if (buffer.starts_with(L"\\\\?\\UNC\\")) {
        buffer = L"\\\\" + buffer.substr(8);
    } else if (buffer.starts_with(L"\\\\?\\")) {
        buffer.erase(0, 4);
    }
    return std::filesystem::path{std::move(buffer)}.lexically_normal();
}

[[nodiscard]] auto componentEqual(const std::filesystem::path& left,
                                  const std::filesystem::path& right) noexcept -> bool {
    const auto& leftText = left.native();
    const auto& rightText = right.native();
    if (leftText.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        rightText.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return false;
    }
    return CompareStringOrdinal(leftText.data(), static_cast<int>(leftText.size()),
                                rightText.data(), static_cast<int>(rightText.size()),
                                TRUE) == CSTR_EQUAL;
}

[[nodiscard]] auto containsPath(const std::filesystem::path& root,
                                const std::filesystem::path& path) noexcept -> bool {
    auto rootPart = root.begin();
    auto pathPart = path.begin();
    for (; rootPart != root.end(); ++rootPart, ++pathPart) {
        if (pathPart == path.end() || !componentEqual(*rootPart, *pathPart)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] auto sameIdentity(const BY_HANDLE_FILE_INFORMATION& left,
                                const BY_HANDLE_FILE_INFORMATION& right) noexcept -> bool {
    return left.dwVolumeSerialNumber == right.dwVolumeSerialNumber &&
           left.nFileIndexHigh == right.nFileIndexHigh && left.nFileIndexLow == right.nFileIndexLow;
}

[[nodiscard]] auto fileSize(const BY_HANDLE_FILE_INFORMATION& information) noexcept
    -> std::uint64_t {
    ULARGE_INTEGER size{};
    size.HighPart = information.nFileSizeHigh;
    size.LowPart = information.nFileSizeLow;
    return size.QuadPart;
}

[[nodiscard]] auto readPlatformFile(const std::filesystem::path& path,
                                    const ReadFileOptions& options) -> core::Result<FileContents> {
    const UniqueHandle root{
        CreateFileW(options.root.c_str(), FILE_READ_ATTRIBUTES | FILE_LIST_DIRECTORY,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                    FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr)};
    if (root.get() == INVALID_HANDLE_VALUE) {
        return core::unexpected(fileError(options.errors.rootUnavailable,
                                          "Trusted file root could not be opened", path));
    }

    BY_HANDLE_FILE_INFORMATION rootInformation{};
    const auto resolvedRoot = finalPath(root.get());
    if (!GetFileInformationByHandle(root.get(), &rootInformation) || !resolvedRoot.has_value() ||
        (rootInformation.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 ||
        (rootInformation.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        return core::unexpected(fileError(options.errors.rootUnavailable,
                                          "Trusted file root is not a stable directory", path));
    }

    const UniqueHandle file{
        CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                    FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_OPEN_REPARSE_POINT, nullptr)};
    if (file.get() == INVALID_HANDLE_VALUE) {
        return core::unexpected(
            fileError(options.errors.openFailed, "Requested file could not be opened", path));
    }

    BY_HANDLE_FILE_INFORMATION before{};
    if (!GetFileInformationByHandle(file.get(), &before)) {
        return core::unexpected(
            fileError(options.errors.openFailed, "Requested file could not be inspected", path));
    }
    if ((before.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ||
        (before.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        return core::unexpected(fileError(
            options.errors.notRegular, "Requested file is not a regular non-reparse file", path));
    }

    const auto resolvedPath = finalPath(file.get());
    if (!resolvedPath.has_value()) {
        return core::unexpected(fileError(options.errors.openFailed,
                                          "Requested file path could not be resolved", path));
    }
    if (!containsPath(*resolvedRoot, *resolvedPath)) {
        return core::unexpected(fileError(
            options.errors.outsideRoot, "Requested file resolved outside its trusted root", path));
    }

    const auto size = fileSize(before);
    if (size > options.maxBytes || size > std::numeric_limits<std::size_t>::max()) {
        return core::unexpected(fileError(options.errors.tooLarge,
                                          "Requested file exceeds the configured byte limit", path)
                                    .withContext("size_bytes", std::to_string(size))
                                    .withContext("limit_bytes", std::to_string(options.maxBytes)));
    }

    FileContents contents;
    contents.bytes.resize(static_cast<std::size_t>(size));
    std::size_t offset = 0;
    while (offset < contents.bytes.size()) {
        constexpr std::size_t chunkSize = 1024U * 1024U;
        const auto request =
            static_cast<DWORD>(std::min(chunkSize, contents.bytes.size() - offset));
        DWORD count = 0;
        if (!ReadFile(file.get(), contents.bytes.data() + offset, request, &count, nullptr) ||
            count == 0) {
            return core::unexpected(fileError(options.errors.readFailed,
                                              "Requested file could not be read completely", path));
        }
        offset += count;
    }

    std::byte extra{};
    DWORD extraCount = 0;
    if (!ReadFile(file.get(), &extra, 1, &extraCount, nullptr)) {
        return core::unexpected(fileError(options.errors.readFailed,
                                          "Requested file read could not be finalized", path));
    }

    BY_HANDLE_FILE_INFORMATION after{};
    if (extraCount != 0 || !GetFileInformationByHandle(file.get(), &after) ||
        !sameIdentity(before, after) || fileSize(before) != fileSize(after) ||
        CompareFileTime(&before.ftLastWriteTime, &after.ftLastWriteTime) != 0) {
        return core::unexpected(fileError(options.errors.changedDuringRead,
                                          "Requested file changed while it was being read", path));
    }

    contents.resolvedPath = *resolvedPath;
    contents.resolvedRoot = *resolvedRoot;
    return contents;
}

#else

class UniqueFd final {
  public:
    explicit UniqueFd(int fd = -1) noexcept : fd_(fd) {}
    ~UniqueFd() {
        if (fd_ >= 0) {
            close(fd_);
        }
    }

    UniqueFd(const UniqueFd&) = delete;
    auto operator=(const UniqueFd&) -> UniqueFd& = delete;
    UniqueFd(UniqueFd&& other) noexcept : fd_(std::exchange(other.fd_, -1)) {}
    auto operator=(UniqueFd&& other) noexcept -> UniqueFd& {
        if (this != &other) {
            if (fd_ >= 0) {
                close(fd_);
            }
            fd_ = std::exchange(other.fd_, -1);
        }
        return *this;
    }

    [[nodiscard]] auto get() const noexcept -> int {
        return fd_;
    }

  private:
    int fd_;
};

[[nodiscard]] auto sameModifiedTime(const struct stat& left, const struct stat& right) noexcept
    -> bool {
#if defined(__APPLE__)
    return left.st_mtimespec.tv_sec == right.st_mtimespec.tv_sec &&
           left.st_mtimespec.tv_nsec == right.st_mtimespec.tv_nsec;
#else
    return left.st_mtim.tv_sec == right.st_mtim.tv_sec &&
           left.st_mtim.tv_nsec == right.st_mtim.tv_nsec;
#endif
}

[[nodiscard]] auto readPlatformFile(const std::filesystem::path& path,
                                    const ReadFileOptions& options) -> core::Result<FileContents> {
    std::error_code pathError;
    const auto absoluteRoot = std::filesystem::absolute(options.root, pathError).lexically_normal();
    const auto absolutePath = std::filesystem::absolute(path, pathError).lexically_normal();
    if (pathError) {
        return core::unexpected(fileError(options.errors.openFailed,
                                          "Requested file path could not be resolved", path));
    }
    const auto relative = absolutePath.lexically_relative(absoluteRoot);
    if (relative.empty() || relative.is_absolute()) {
        return core::unexpected(fileError(options.errors.outsideRoot,
                                          "Requested file is outside its trusted root", path));
    }

    std::vector<std::string> components;
    for (const auto& component : relative) {
        if (component == ".") {
            continue;
        }
        if (component == "..") {
            return core::unexpected(fileError(options.errors.outsideRoot,
                                              "Requested file is outside its trusted root", path));
        }
        components.push_back(component.string());
    }
    if (components.empty()) {
        return core::unexpected(
            fileError(options.errors.notRegular, "Requested file is not a regular file", path));
    }

    UniqueFd directory{open(absoluteRoot.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW)};
    if (directory.get() < 0) {
        return core::unexpected(fileError(options.errors.rootUnavailable,
                                          "Trusted file root could not be opened", path));
    }
    for (std::size_t index = 0; index + 1 < components.size(); ++index) {
        UniqueFd next{openat(directory.get(), components[index].c_str(),
                             O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW)};
        if (next.get() < 0) {
            return core::unexpected(fileError(options.errors.outsideRoot,
                                              "Requested file path contains an unsafe directory",
                                              path));
        }
        directory = std::move(next);
    }

    UniqueFd file{
        openat(directory.get(), components.back().c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW)};
    if (file.get() < 0) {
        return core::unexpected(
            fileError(options.errors.openFailed, "Requested file could not be opened", path));
    }
    struct stat before{};
    if (fstat(file.get(), &before) != 0 || !S_ISREG(before.st_mode)) {
        return core::unexpected(
            fileError(options.errors.notRegular, "Requested file is not a regular file", path));
    }
    if (before.st_size < 0 || static_cast<std::uintmax_t>(before.st_size) > options.maxBytes ||
        static_cast<std::uintmax_t>(before.st_size) > std::numeric_limits<std::size_t>::max()) {
        return core::unexpected(fileError(options.errors.tooLarge,
                                          "Requested file exceeds the configured byte limit", path)
                                    .withContext("size_bytes", std::to_string(before.st_size))
                                    .withContext("limit_bytes", std::to_string(options.maxBytes)));
    }

    FileContents contents;
    contents.bytes.resize(static_cast<std::size_t>(before.st_size));
    std::size_t offset = 0;
    while (offset < contents.bytes.size()) {
        const auto count = pread(file.get(), contents.bytes.data() + offset,
                                 contents.bytes.size() - offset, static_cast<off_t>(offset));
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            return core::unexpected(fileError(options.errors.readFailed,
                                              "Requested file could not be read completely", path));
        }
        offset += static_cast<std::size_t>(count);
    }

    std::byte extra{};
    ssize_t extraCount = -1;
    do {
        extraCount = pread(file.get(), &extra, 1, static_cast<off_t>(contents.bytes.size()));
    } while (extraCount < 0 && errno == EINTR);

    struct stat after{};
    if (extraCount != 0 || fstat(file.get(), &after) != 0 || before.st_dev != after.st_dev ||
        before.st_ino != after.st_ino || before.st_size != after.st_size ||
        !sameModifiedTime(before, after)) {
        return core::unexpected(fileError(options.errors.changedDuringRead,
                                          "Requested file changed while it was being read", path));
    }

    contents.resolvedRoot = absoluteRoot;
    contents.resolvedPath = absoluteRoot / relative;
    return contents;
}

#endif

} // namespace

auto readBoundedFile(const std::filesystem::path& path, const ReadFileOptions& options)
    -> core::Result<FileContents> {
    if (path.empty() || options.root.empty()) {
        return core::unexpected(fileError(
            options.errors.openFailed, "Requested file and trusted root must be specified", path));
    }
    if (options.maxBytes == 0) {
        return core::unexpected(core::Error{"filesystem.file.invalid_limit",
                                            "Bounded file byte limit must be non-zero"});
    }
    return readPlatformFile(path, options);
}

auto readBoundedTextFile(const std::filesystem::path& path, const ReadFileOptions& options)
    -> core::Result<TextFileContents> {
    auto contents = readBoundedFile(path, options);
    if (!contents) {
        return core::unexpected(std::move(contents.error()));
    }
    TextFileContents text;
    text.text.resize(contents->bytes.size());
    if (!text.text.empty()) {
        std::memcpy(text.text.data(), contents->bytes.data(), contents->bytes.size());
    }
    text.resolvedPath = std::move(contents->resolvedPath);
    text.resolvedRoot = std::move(contents->resolvedRoot);
    return text;
}

} // namespace cuexis::filesystem

#include "asset_publish_internal.hpp"

#include "publish_fs_internal.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <Windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace cuexis::tools {

using detail::publishBusyCode;
using detail::publishError;
using detail::publishInvalidCode;
using detail::publishIoCode;
using detail::publishStagingCode;

// --- publication lock --------------------------------------------------------------------------

// The lock is an OS-level exclusive lock on an open file handle rather than the existence of a lock
// file: the kernel releases it when the holder exits, so an interrupted writer cannot leave a stale
// lock that blocks every later writer, and a live second writer is rejected instead of queueing.
#if defined(_WIN32)
auto PublicationLock::acquire(const fs::path& target) -> core::Result<PublicationLock> {
    const HANDLE handle = CreateFileW(target.c_str(), GENERIC_READ | GENERIC_WRITE,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
                                      FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return core::unexpected(
            publishError(publishIoCode, "Publication lock file could not be opened")
                .withContext("path", target.generic_string())
                .withContext("platform_error", std::to_string(GetLastError())));
    }
    OVERLAPPED overlapped{};
    if (LockFileEx(handle, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0,
                   &overlapped) == 0) {
        const auto platformError = static_cast<int>(GetLastError());
        CloseHandle(handle);
        if (platformError == ERROR_LOCK_VIOLATION || platformError == ERROR_IO_PENDING) {
            return core::unexpected(
                publishError(publishBusyCode, "Another writer holds the publication lock")
                    .withContext("path", target.generic_string()));
        }
        return core::unexpected(
            publishError(publishIoCode, "Publication lock could not be acquired")
                .withContext("path", target.generic_string())
                .withContext("platform_error", std::to_string(platformError)));
    }
    PublicationLock lock;
    lock.path_ = target;
    lock.handles_.push_back(handle);
    return lock;
}

void PublicationLock::release() noexcept {
    for (void* entry : handles_) {
        if (entry == nullptr) {
            continue;
        }
        const HANDLE handle = static_cast<HANDLE>(entry);
        OVERLAPPED overlapped{};
        UnlockFileEx(handle, 0, 1, 0, &overlapped);
        CloseHandle(handle);
    }
    handles_.clear();
}
#else
auto PublicationLock::acquire(const fs::path& target) -> core::Result<PublicationLock> {
    const int descriptor = ::open(target.c_str(), O_RDWR | O_CREAT, 0600);
    if (descriptor < 0) {
        return core::unexpected(
            publishError(publishIoCode, "Publication lock file could not be opened")
                .withContext("path", target.generic_string())
                .withContext("platform_error", std::to_string(errno)));
    }
    if (::flock(descriptor, LOCK_EX | LOCK_NB) != 0) {
        const int platformError = errno;
        ::close(descriptor);
        if (platformError == EWOULDBLOCK || platformError == EAGAIN) {
            return core::unexpected(
                publishError(publishBusyCode, "Another writer holds the publication lock")
                    .withContext("path", target.generic_string()));
        }
        return core::unexpected(
            publishError(publishIoCode, "Publication lock could not be acquired")
                .withContext("path", target.generic_string())
                .withContext("platform_error", std::to_string(platformError)));
    }
    PublicationLock lock;
    lock.path_ = target;
    lock.handles_.push_back(reinterpret_cast<void*>(static_cast<std::intptr_t>(descriptor)));
    return lock;
}

void PublicationLock::release() noexcept {
    for (void* entry : handles_) {
        if (entry == nullptr) {
            continue;
        }
        const int descriptor = static_cast<int>(reinterpret_cast<std::intptr_t>(entry));
        ::flock(descriptor, LOCK_UN);
        ::close(descriptor);
    }
    handles_.clear();
}
#endif

auto PublicationLock::acquireAll(std::vector<fs::path> targets) -> core::Result<PublicationLock> {
    std::sort(targets.begin(), targets.end());
    targets.erase(std::unique(targets.begin(), targets.end()), targets.end());
    PublicationLock lock;
    for (const auto& target : targets) {
        auto one = acquire(target);
        if (!one) {
            // `lock` is destroyed here, which releases every handle already taken: a partial
            // acquisition must not leave a lock behind.
            return core::unexpected(std::move(one.error()));
        }
        if (lock.path_.empty()) {
            lock.path_ = one->path_;
        }
        lock.handles_.insert(lock.handles_.end(), one->handles_.begin(), one->handles_.end());
        one->handles_.clear();
    }
    return lock;
}

PublicationLock::PublicationLock(PublicationLock&& other) noexcept
    : path_(std::move(other.path_)), handles_(std::move(other.handles_)) {
    other.handles_.clear();
}

auto PublicationLock::operator=(PublicationLock&& other) noexcept -> PublicationLock& {
    if (this != &other) {
        release();
        path_ = std::move(other.path_);
        handles_ = std::move(other.handles_);
        other.handles_.clear();
    }
    return *this;
}

PublicationLock::~PublicationLock() {
    release();
}

auto PublicationLock::path() const noexcept -> const std::filesystem::path& {
    return path_;
}

auto recoverPublicationStaging(const fs::path& root) -> core::Result<std::size_t> {
    if (root.empty()) {
        return core::unexpected(publishError(publishInvalidCode, "Publication root is empty"));
    }
    std::error_code status;
    if (!fs::is_directory(root, status) || status) {
        return core::unexpected(
            publishError(publishInvalidCode, "Publication root must be an existing directory")
                .withContext("path", root.generic_string()));
    }
    std::size_t removed = 0;
    const auto removeMatchingTemporaries = [&removed](const fs::path& directory) {
        std::error_code scanStatus;
        std::vector<fs::path> candidates;
        for (const auto& entry : fs::directory_iterator{directory, scanStatus}) {
            if (scanStatus) {
                break;
            }
            const auto name = entry.path().filename().generic_string();
            if (name.find(std::string{"."} + std::string{publicationRole} +
                          std::string{temporaryInfix}) != std::string::npos) {
                candidates.push_back(entry.path());
            }
        }
        for (const auto& candidate : candidates) {
            std::error_code removeStatus;
            fs::remove_all(candidate, removeStatus);
            if (!removeStatus) {
                ++removed;
            }
        }
    };
    removeMatchingTemporaries(root);
    // A backup is not a temporary file. Earlier revisions published by moving the target aside
    // first; if such a revision crashed before its second rename, the backup holds the last valid
    // package and deleting it would destroy the only surviving copy. Restore it over a missing
    // target, and leave a target that already exists untouched.
    const auto restoreMatchingBackups = [&removed](const fs::path& directory) {
        std::error_code scanStatus;
        std::vector<fs::path> candidates;
        for (const auto& entry : fs::directory_iterator{directory, scanStatus}) {
            if (scanStatus) {
                break;
            }
            const auto name = entry.path().filename().generic_string();
            if (name.find(std::string{"."} + std::string{publicationBackupRole} +
                          std::string{temporaryInfix}) != std::string::npos) {
                candidates.push_back(entry.path());
            }
        }
        for (const auto& candidate : candidates) {
            const auto restored = detail::backupTargetOf(candidate, publicationBackupRole);
            std::error_code existsStatus;
            const bool targetExists = fs::exists(restored, existsStatus) && !existsStatus;
            if (!targetExists) {
                auto moved = detail::replaceAtomically(candidate, restored);
                if (!moved) {
                    continue;
                }
            }
            std::error_code removeStatus;
            fs::remove_all(candidate, removeStatus);
            if (!removeStatus) {
                ++removed;
            }
        }
    };
    restoreMatchingBackups(root);
    for (const auto& directory :
         {root / pathOf(generationStagingDirectory), root / pathOf(generationPublishedDirectory)}) {
        if (fs::is_directory(directory, status) && !status) {
            restoreMatchingBackups(directory);
        }
    }
    for (const auto& directory :
         {root / pathOf(generationStagingDirectory), root / pathOf(generationPublishedDirectory)}) {
        if (fs::is_directory(directory, status) && !status) {
            removeMatchingTemporaries(directory);
        }
    }
    const auto staging = root / pathOf(generationStagingDirectory);
    if (fs::is_directory(staging, status) && !status) {
        for (const auto& entry : fs::directory_iterator{staging, status}) {
            if (status) {
                break;
            }
            std::error_code removeStatus;
            fs::remove_all(entry.path(), removeStatus);
            if (!removeStatus) {
                ++removed;
            }
        }
    }
    return removed;
}

auto publishGeneration(const GenerationPublishRequest& request)
    -> core::Result<GenerationPublishResult> {
    return publishGenerationLocked(request, true);
}

auto adoptGeneration(const fs::path& root, std::string_view generationId) -> core::Result<void> {
    return adoptGenerationLocked(root, generationId, true);
}

auto publishAndAdoptGeneration(const GenerationPublishRequest& request)
    -> core::Result<GenerationPublishResult> {
    bool commitVisible = false;
    try {
        if (request.root.empty())
            return core::unexpected(publishError(publishInvalidCode, "Generation root is empty"));
        std::error_code status;
        fs::create_directories(request.root, status);
        if (status || !detail::isDirectory(request.root))
            return core::unexpected(
                publishError(publishIoCode, "Generation root could not be created"));
        auto lock = PublicationLock::acquire(request.root / pathOf(publicationLockName));
        if (!lock)
            return core::unexpected(std::move(lock.error()));
        auto published = publishGenerationLocked(request, false);
        if (!published)
            return core::unexpected(std::move(published.error()));
        auto adopted = adoptGenerationLocked(request.root, published->generationIdentity, false,
                                             &commitVisible);
        if (!adopted)
            return core::unexpected(std::move(adopted.error()));
        return std::move(*published);
    } catch (...) {
        return core::unexpected(
            publishError(publishIoCode, "Generation transaction could not produce an owning result")
                .withContext("commitVisible", commitVisible ? "true" : "false"));
    }
}

} // namespace cuexis::tools

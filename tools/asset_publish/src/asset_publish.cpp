#include <cuexis/tools/asset_publish.hpp>

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

#if defined(_WIN32)
#include <Windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace cuexis::tools {
namespace {

namespace fs = std::filesystem;

using detail::publishBusyCode;
using detail::publishConflictCode;
using detail::publishError;
using detail::publishInvalidCode;
using detail::publishIoCode;
using detail::publishReplaceCode;
using detail::publishStagingCode;
using detail::publishValidationCode;

inline constexpr std::string_view generationDomain = "cuexis.generation.closure.v1";
inline constexpr std::string_view generationStagingDirectory = "staging";
inline constexpr std::string_view generationPublishedDirectory = "generations";
inline constexpr std::string_view publicationRole = "cuexis-publish";
inline constexpr std::string_view publicationBackupRole = "cuexis-backup";
inline constexpr std::string_view temporaryInfix = ".tmp.";
inline constexpr std::size_t maxGenerationIdBytes = 64;
inline constexpr std::size_t maxEntries = 65534;

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

struct ListedEntry final {
    std::string path;
    std::uint64_t byteCount{};
    std::string sha256;
};

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

struct MarkerDocument final {
    std::string label;
    std::string identity;
    std::vector<ListedEntry> closure;
    std::vector<ListedEntry> provenance;
};

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

} // namespace

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
    lock.handle_ = handle;
    return lock;
}

void PublicationLock::release() noexcept {
    if (handle_ == nullptr) {
        return;
    }
    const HANDLE handle = static_cast<HANDLE>(handle_);
    OVERLAPPED overlapped{};
    UnlockFileEx(handle, 0, 1, 0, &overlapped);
    CloseHandle(handle);
    handle_ = nullptr;
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
    lock.handle_ = reinterpret_cast<void*>(static_cast<std::intptr_t>(descriptor));
    return lock;
}

void PublicationLock::release() noexcept {
    if (handle_ == nullptr) {
        return;
    }
    const int descriptor = static_cast<int>(reinterpret_cast<std::intptr_t>(handle_));
    ::flock(descriptor, LOCK_UN);
    ::close(descriptor);
    handle_ = nullptr;
}
#endif

PublicationLock::PublicationLock(PublicationLock&& other) noexcept
    : path_(std::move(other.path_)), handle_(other.handle_) {
    other.handle_ = nullptr;
}

auto PublicationLock::operator=(PublicationLock&& other) noexcept -> PublicationLock& {
    if (this != &other) {
        release();
        path_ = std::move(other.path_);
        handle_ = other.handle_;
        other.handle_ = nullptr;
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
                          std::string{temporaryInfix}) != std::string::npos ||
                name.find(std::string{"."} + std::string{publicationBackupRole} +
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
    if (request.root.empty()) {
        return core::unexpected(publishError(publishInvalidCode, "Generation root is empty"));
    }
    if (!isPortableToken(request.generationId, maxGenerationIdBytes)) {
        return core::unexpected(
            publishError(publishInvalidCode, "Generation id must be a short portable token")
                .withContext("generation_id", request.generationId));
    }
    if (request.entries.empty()) {
        return core::unexpected(
            publishError(publishInvalidCode, "Generation closure must not be empty"));
    }
    auto closure = listEntries(request.entries);
    if (!closure) {
        return core::unexpected(std::move(closure.error()));
    }
    auto provenance = listEntries(request.provenanceRecords);
    if (!provenance) {
        return core::unexpected(std::move(provenance.error()));
    }

    std::error_code status;
    fs::create_directories(request.root, status);
    if (status || !detail::isDirectory(request.root)) {
        return core::unexpected(publishError(publishIoCode, "Generation root could not be created")
                                    .withContext("path", request.root.generic_string()));
    }

    auto lock = PublicationLock::acquire(request.root / pathOf(publicationLockName));
    if (!lock) {
        return core::unexpected(std::move(lock.error()));
    }

    MarkerDocument marker;
    marker.label = request.generationId;
    marker.identity = generationIdentityOf(marker.label, *closure, *provenance);
    marker.closure = *closure;
    marker.provenance = *provenance;

    const auto publishedRoot = request.root / pathOf(generationPublishedDirectory);
    const auto generationPath = publishedRoot / fs::path{marker.identity};
    if (fs::exists(generationPath, status) && !status) {
        // Content-addressed and immutable: an identical republish is a verified no-op, and a
        // different tree under the same identity is refused instead of overwritten.
        auto existing = readMarker(generationPath);
        if (!existing) {
            return core::unexpected(std::move(existing.error()));
        }
        if (!markersEqual(*existing, marker)) {
            return core::unexpected(
                publishError(publishConflictCode,
                             "Published generation does not match the requested content")
                    .withContext("path", generationPath.generic_string()));
        }
        auto verified = verifyGenerationTree(generationPath, *existing);
        if (!verified) {
            return core::unexpected(std::move(verified.error()));
        }
        return GenerationPublishResult{generationPath, marker.identity, 0, marker.closure.size(),
                                       marker.provenance.size()};
    }

    const auto stagingPath =
        request.root / pathOf(generationStagingDirectory) / fs::path{marker.identity};
    detail::removeTreeQuiet(stagingPath);
    fs::create_directories(stagingPath, status);
    if (status) {
        return core::unexpected(
            publishError(publishStagingCode, "Generation staging directory could not be created")
                .withContext("path", stagingPath.generic_string()));
    }
    const auto abandon = [&stagingPath]() { detail::removeTreeQuiet(stagingPath); };

    auto closureWritten = writeEntryTree(stagingPath, request.entries, fs::path{});
    if (!closureWritten) {
        abandon();
        return core::unexpected(std::move(closureWritten.error()));
    }
    auto provenanceWritten =
        writeEntryTree(stagingPath, request.provenanceRecords, pathOf(provenanceDirectoryName));
    if (!provenanceWritten) {
        abandon();
        return core::unexpected(std::move(provenanceWritten.error()));
    }
    const auto markerText = encodeMarker(marker);
    const auto markerBytes =
        std::as_bytes(std::span<const char>{markerText.data(), markerText.size()});
    auto markerWritten =
        detail::writeFileExclusive(stagingPath / pathOf(generationMarkerName), markerBytes);
    if (!markerWritten) {
        abandon();
        return core::unexpected(std::move(markerWritten.error()));
    }

    if (detail::testFailureEnabled("CUEXIS_ASSET_PUBLISH_FAIL_AFTER_STAGING")) {
        abandon();
        return core::unexpected(
            publishError(publishStagingCode, "Injected failure after staging (test hook)"));
    }

    // The staged tree is re-read and re-hashed before it becomes visible.
    auto stagedMarker = readMarker(stagingPath);
    if (!stagedMarker) {
        abandon();
        return core::unexpected(std::move(stagedMarker.error()));
    }
    auto stagedVerified = verifyGenerationTree(stagingPath, *stagedMarker);
    if (!stagedVerified) {
        abandon();
        return core::unexpected(std::move(stagedVerified.error()));
    }
    auto synced = detail::syncDirectory(stagingPath);
    if (!synced) {
        abandon();
        return core::unexpected(std::move(synced.error()));
    }

    if (detail::testFailureEnabled("CUEXIS_ASSET_PUBLISH_FAIL_BEFORE_PUBLISH")) {
        abandon();
        return core::unexpected(
            publishError(publishReplaceCode, "Injected failure before publish (test hook)"));
    }

    fs::create_directories(publishedRoot, status);
    if (status) {
        abandon();
        return core::unexpected(
            publishError(publishStagingCode, "Generation directory could not be created")
                .withContext("path", publishedRoot.generic_string()));
    }
    // The single visible switch: one rename publishes the whole generation.
    auto published = detail::replaceAtomically(stagingPath, generationPath);
    if (!published) {
        abandon();
        auto existing = readMarker(generationPath);
        if (existing) {
            auto verified = verifyGenerationTree(generationPath, *existing);
            if (verified && markersEqual(*existing, marker)) {
                return GenerationPublishResult{generationPath, marker.identity, 0,
                                               marker.closure.size(), marker.provenance.size()};
            }
        }
        return core::unexpected(std::move(published.error()));
    }
    auto publishedSynced = detail::syncDirectory(publishedRoot);
    if (!publishedSynced) {
        return core::unexpected(std::move(publishedSynced.error()));
    }

    std::uint64_t closureBytes = 0;
    for (const auto& entry : marker.closure) {
        closureBytes += entry.byteCount;
    }
    return GenerationPublishResult{generationPath, marker.identity, closureBytes,
                                   marker.closure.size(), marker.provenance.size()};
}

auto adoptGeneration(const fs::path& root, std::string_view generationId) -> core::Result<void> {
    if (root.empty() || !detail::isLowerHex(generationId, 64)) {
        return core::unexpected(
            publishError(publishInvalidCode, "Adoption requires a root and a generation identity"));
    }
    const auto generationPath =
        root / pathOf(generationPublishedDirectory) / fs::path{std::string{generationId}};
    auto marker = readMarker(generationPath);
    if (!marker) {
        return core::unexpected(std::move(marker.error()));
    }
    if (marker->identity != generationId) {
        return core::unexpected(
            publishError(publishValidationCode, "Generation marker identity differs from its path")
                .withContext("path", generationPath.generic_string()));
    }
    auto verified = verifyGenerationTree(generationPath, *marker);
    if (!verified) {
        return core::unexpected(std::move(verified.error()));
    }
    std::string text{adoptedGenerationFormat};
    text += ' ';
    text += std::to_string(adoptedGenerationVersion);
    text += '\n';
    text += std::string{generationId};
    text += '\n';
    const auto bytes = std::as_bytes(std::span<const char>{text.data(), text.size()});

    auto lock = PublicationLock::acquire(root / pathOf(publicationLockName));
    if (!lock) {
        return core::unexpected(std::move(lock.error()));
    }
    const auto target = root / pathOf(adoptedGenerationName);
    const auto temporary = detail::uniqueSibling(target, publicationRole);
    auto written = detail::writeFileExclusive(temporary, bytes);
    if (!written) {
        detail::removeTreeQuiet(temporary);
        return core::unexpected(std::move(written.error()));
    }
    auto replaced = detail::replaceAtomically(temporary, target);
    if (!replaced) {
        detail::removeTreeQuiet(temporary);
        return core::unexpected(std::move(replaced.error()));
    }
    return detail::syncDirectory(root);
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

// --- CXC package publication -------------------------------------------------------------------

namespace {

struct BuiltPackage final {
    std::vector<std::byte> bytes;
    std::string identity;
};

[[nodiscard]] auto buildPackage(const std::vector<cxc::CxcWriteEntry>& entries,
                                const std::string& extensionsJson,
                                const std::vector<cxc::CxcRequiredExtension>& requiredExtensions,
                                const cxc::CxcPackageLimits& limits) -> core::Result<BuiltPackage> {
    cxc::CxcWriteRequest request;
    request.entries = entries;
    request.extensionsJson = extensionsJson;
    request.requiredExtensions = requiredExtensions;
    auto written = cxc::CxcWriter::write(std::move(request), limits);
    if (!written.hasValue() || !written.bytes.has_value()) {
        return core::unexpected(
            publishError(publishValidationCode, "CXC writer rejected the package: " +
                                                    diagnosticsMessage(written.diagnostics)));
    }
    auto loaded = cxc::CxcPackageLoader::loadMemory(
        std::span<const std::byte>{written.bytes->data(), written.bytes->size()}, limits);
    if (!loaded.package.has_value()) {
        return core::unexpected(
            publishError(publishValidationCode, "CXC package self-validation failed: " +
                                                    diagnosticsMessage(loaded.diagnostics)));
    }
    return BuiltPackage{std::move(*written.bytes), loaded.package->identity().hex()};
}

// Writes one package to a same-directory temporary file, re-validates the bytes on disk through the
// production loader, and only then replaces the target. The previous package is restored if the
// final replacement fails.
[[nodiscard]] auto commitPackage(const fs::path& target, const BuiltPackage& built,
                                 const cxc::CxcPackageLimits& limits, bool& replacedExisting)
    -> core::Result<void> {
    const auto parent = target.parent_path();
    if (!detail::isDirectory(parent)) {
        return core::unexpected(
            publishError(publishInvalidCode, "Package target directory does not exist")
                .withContext("path", parent.generic_string()));
    }
    const auto temporary = detail::uniqueSibling(target, publicationRole);
    detail::removeTreeQuiet(temporary);
    auto written = detail::writeFileExclusive(temporary, built.bytes);
    if (!written) {
        detail::removeTreeQuiet(temporary);
        return core::unexpected(std::move(written.error()));
    }
    auto reloaded = cxc::CxcPackageLoader::loadFile(temporary, limits);
    if (!reloaded.package.has_value()) {
        detail::removeTreeQuiet(temporary);
        return core::unexpected(
            publishError(publishValidationCode, "Published package bytes failed validation: " +
                                                    diagnosticsMessage(reloaded.diagnostics)));
    }
    if (reloaded.package->identity().hex() != built.identity) {
        detail::removeTreeQuiet(temporary);
        return core::unexpected(
            publishError(publishValidationCode, "Published package identity changed on disk")
                .withContext("path", target.generic_string()));
    }

    if (detail::testFailureEnabled("CUEXIS_ASSET_PUBLISH_FAIL_BEFORE_REPLACE") ||
        detail::testFailureTargetMatches("CUEXIS_ASSET_PUBLISH_FAIL_REPLACE_TARGET",
                                         target.filename().string())) {
        detail::removeTreeQuiet(temporary);
        return core::unexpected(
            publishError(publishReplaceCode, "Injected failure before replace (test hook)"));
    }

    fs::path backup;
    std::error_code status;
    if (fs::exists(target, status) && !status) {
        backup = detail::uniqueSibling(target, publicationBackupRole);
        auto moved = detail::replaceAtomically(target, backup);
        if (!moved) {
            detail::removeTreeQuiet(temporary);
            return core::unexpected(std::move(moved.error()));
        }
        replacedExisting = true;
    }
    auto committed = detail::replaceAtomically(temporary, target);
    if (!committed) {
        detail::removeTreeQuiet(temporary);
        if (!backup.empty()) {
            auto restored = detail::replaceAtomically(backup, target);
            if (!restored) {
                return core::unexpected(
                    publishError(publishReplaceCode,
                                 "Package replacement failed and the previous package could not be "
                                 "restored")
                        .withContext("backup", backup.generic_string())
                        .withContext("target", target.generic_string()));
            }
        }
        return core::unexpected(std::move(committed.error()));
    }
    if (!backup.empty()) {
        detail::removeTreeQuiet(backup);
    }
    auto synced = detail::syncDirectory(parent);
    if (!synced) {
        return core::unexpected(std::move(synced.error()));
    }
    return {};
}

} // namespace

auto publishPackage(const PackagePublishRequest& request) -> core::Result<PackagePublishResult> {
    if (request.target.empty() || request.entries.empty()) {
        return core::unexpected(
            publishError(publishInvalidCode, "Package publication requires a target and entries"));
    }
    if (!detail::isDirectory(request.target.parent_path())) {
        return core::unexpected(
            publishError(publishInvalidCode, "Package target directory does not exist")
                .withContext("path", request.target.parent_path().generic_string()));
    }
    auto built = buildPackage(request.entries, request.extensionsJson, request.requiredExtensions,
                              request.limits);
    if (!built) {
        return core::unexpected(std::move(built.error()));
    }
    auto lock =
        PublicationLock::acquire(request.target.parent_path() / pathOf(publicationLockName));
    if (!lock) {
        return core::unexpected(std::move(lock.error()));
    }
    bool replacedExisting = false;
    auto committed = commitPackage(request.target, *built, request.limits, replacedExisting);
    if (!committed) {
        return core::unexpected(std::move(committed.error()));
    }
    return PackagePublishResult{request.target, built->identity,
                                static_cast<std::uint64_t>(built->bytes.size()), replacedExisting};
}

auto publishPackagePair(const PackagePairRequest& request) -> core::Result<PackagePairResult> {
    if (request.v4Target.empty() || request.candidateTarget.empty() || request.entries.empty()) {
        return core::unexpected(publishError(
            publishInvalidCode, "Package pair publication requires two targets and entries"));
    }
    if (request.v4Target == request.candidateTarget) {
        return core::unexpected(
            publishError(publishInvalidCode, "Package pair targets must differ"));
    }
    if (!detail::isDirectory(request.v4Target.parent_path())) {
        return core::unexpected(
            publishError(publishInvalidCode, "Package target directory does not exist")
                .withContext("path", request.v4Target.parent_path().generic_string()));
    }
    // One batch, two packages: the candidate closure is the shared closure plus the entries its own
    // extension declares, and both packages are built and validated before either target is
    // touched.
    auto v4 = buildPackage(request.entries, "{}", request.requiredExtensions, request.limits);
    if (!v4) {
        return core::unexpected(std::move(v4.error()));
    }
    std::vector<cxc::CxcWriteEntry> candidateEntries = request.entries;
    candidateEntries.insert(candidateEntries.end(), request.candidateEntries.begin(),
                            request.candidateEntries.end());
    auto candidate = buildPackage(candidateEntries, request.candidateExtensionsJson,
                                  request.requiredExtensions, request.limits);
    if (!candidate) {
        return core::unexpected(std::move(candidate.error()));
    }
    auto lock =
        PublicationLock::acquire(request.v4Target.parent_path() / pathOf(publicationLockName));
    if (!lock) {
        return core::unexpected(std::move(lock.error()));
    }

    bool v4Replaced = false;
    auto v4Committed = commitPackage(request.v4Target, *v4, request.limits, v4Replaced);
    if (!v4Committed) {
        return core::unexpected(std::move(v4Committed.error()));
    }
    bool candidateReplaced = false;
    auto candidateCommitted =
        commitPackage(request.candidateTarget, *candidate, request.limits, candidateReplaced);
    if (!candidateCommitted) {
        // The pair is transactional: a failed second package must not leave a half-updated pair.
        detail::removeTreeQuiet(request.v4Target);
        return core::unexpected(
            publishError(publishReplaceCode,
                         "Candidate package publication failed; the v4 package was rolled back")
                .withCause(std::move(candidateCommitted.error())));
    }
    PackagePairResult result;
    result.v4 = PackagePublishResult{request.v4Target, v4->identity,
                                     static_cast<std::uint64_t>(v4->bytes.size()), v4Replaced};
    result.candidate = PackagePublishResult{request.candidateTarget, candidate->identity,
                                            static_cast<std::uint64_t>(candidate->bytes.size()),
                                            candidateReplaced};
    return result;
}

} // namespace cuexis::tools

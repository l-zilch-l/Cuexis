#include <cuexis/content/content_provider.hpp>

#include <cuexis/core/error.hpp>
#include <cuexis/filesystem/secure_file.hpp>

#include <algorithm>
#include <cctype>
#include <exception>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <system_error>
#include <utility>

namespace cuexis::content {
namespace {

[[nodiscard]] auto invalidRequest(const ContentRequest& request) -> core::Result<void> {
    if (request.rootId.empty() || request.source.empty()) {
        return core::unexpected(
            core::Error{"content.request.invalid", "Content root and source must not be empty"});
    }
    if (request.maxBytes == 0) {
        return core::unexpected(
            core::Error{"content.request.invalid_limit", "Content byte limit must be non-zero"});
    }
    const std::filesystem::path sourcePath{request.source};
    if (sourcePath.is_absolute() || request.source.starts_with('/') ||
        request.source.find('\\') != std::string_view::npos ||
        request.source.find(':') != std::string_view::npos) {
        return core::unexpected(core::Error{"content.request.source_invalid",
                                            "Content source must be a portable relative path"});
    }
    for (const auto& component : sourcePath) {
        if (component.empty() || component == "." || component == "..") {
            return core::unexpected(core::Error{"content.request.source_invalid",
                                                "Content source contains an unsafe path segment"});
        }
    }
    return {};
}

[[nodiscard]] auto contentKey(std::string_view rootId, std::string_view source) -> std::string {
    std::string key;
    key.reserve(rootId.size() + source.size() + 1U);
    key.append(rootId);
    key.push_back('\0');
    key.append(source);
    return key;
}

[[nodiscard]] auto normalizedPathKey(const std::filesystem::path& path) -> std::string {
    auto key = path.lexically_normal().generic_string();
#if defined(_WIN32)
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
#endif
    return key;
}

[[nodiscard]] auto isPathPrefix(const std::filesystem::path& prefix,
                                const std::filesystem::path& path) -> bool {
    const auto prefixKey = normalizedPathKey(prefix);
    const auto pathKey = normalizedPathKey(path);
    if (pathKey.size() < prefixKey.size() || !pathKey.starts_with(prefixKey)) {
        return false;
    }
    return pathKey.size() == prefixKey.size() || prefixKey.ends_with('/') ||
           pathKey[prefixKey.size()] == '/';
}

[[nodiscard]] auto contentRevision(std::span<const std::byte> bytes) noexcept -> std::uint64_t {
    std::uint64_t hash = 1469598103934665603ULL;
    for (const auto value : bytes) {
        hash ^= static_cast<std::uint64_t>(std::to_integer<unsigned char>(value));
        hash *= 1099511628211ULL;
    }
    return hash == 0 ? 1 : hash;
}

[[nodiscard]] auto checkedBlob(ContentBlob blob, const ContentRequest& request,
                               std::string_view providerName) -> core::Result<ContentBlob> {
    if (blob.bytes.size() > request.maxBytes) {
        return core::unexpected(
            core::Error{"content.provider.too_large", "Content provider exceeded the byte limit"}
                .withContext("provider", std::string{providerName})
                .withContext("size_bytes", std::to_string(blob.bytes.size()))
                .withContext("limit_bytes", std::to_string(request.maxBytes)));
    }
    return blob;
}

} // namespace

struct FilesystemContentProvider::Root final {
    std::string id;
    std::filesystem::path canonicalPath;
};

FilesystemContentProvider::FilesystemContentProvider(std::vector<Root> roots) noexcept
    : roots_(std::move(roots)) {}

FilesystemContentProvider::~FilesystemContentProvider() = default;

auto FilesystemContentProvider::create(std::vector<FilesystemContentRoot> roots)
    -> core::Result<std::shared_ptr<FilesystemContentProvider>> {
    if (roots.empty()) {
        return core::unexpected(core::Error{"content.filesystem.roots_empty",
                                            "Filesystem provider requires at least one root"});
    }

    std::vector<Root> checkedRoots;
    checkedRoots.reserve(roots.size());
    std::set<std::string, std::less<>> ids;
    for (auto& root : roots) {
        if (root.id.empty() || !ids.insert(root.id).second) {
            return core::unexpected(core::Error{"content.filesystem.root_id_invalid",
                                                "Filesystem content root ID is empty or duplicated"}
                                        .withContext("rootId", root.id));
        }
        std::error_code error;
        auto canonical = std::filesystem::weakly_canonical(root.path, error);
        if (error || canonical.empty() || !std::filesystem::is_directory(canonical, error) ||
            error) {
            return core::unexpected(core::Error{"content.filesystem.root_unavailable",
                                                "Filesystem content root is unavailable"}
                                        .withContext("rootId", root.id));
        }
        for (const auto& existing : checkedRoots) {
            if (isPathPrefix(existing.canonicalPath, canonical) ||
                isPathPrefix(canonical, existing.canonicalPath)) {
                return core::unexpected(core::Error{"content.filesystem.root_overlap",
                                                    "Filesystem content roots overlap"}
                                            .withContext("rootId", root.id));
            }
        }
        checkedRoots.push_back(Root{std::move(root.id), std::move(canonical)});
    }
    return std::shared_ptr<FilesystemContentProvider>{
        new FilesystemContentProvider{std::move(checkedRoots)}};
}

auto FilesystemContentProvider::readBlob(const ContentRequest& request)
    -> core::Result<ContentBlob> {
    if (auto valid = invalidRequest(request); !valid) {
        return core::unexpected(std::move(valid.error()));
    }
    const auto root = std::find_if(roots_.begin(), roots_.end(), [&](const Root& candidate) {
        return candidate.id == request.rootId;
    });
    if (root == roots_.end()) {
        return core::unexpected(core::Error{"content.filesystem.root_not_found",
                                            "Filesystem content root was not registered"}
                                    .withContext("rootId", std::string{request.rootId}));
    }

    auto contents = filesystem::readBoundedFile(
        root->canonicalPath / std::filesystem::path{request.source},
        {.root = root->canonicalPath,
         .maxBytes = request.maxBytes,
         .errors = {.rootUnavailable = "content.filesystem.root_unavailable",
                    .rootChanged = "content.filesystem.root_changed",
                    .openFailed = "content.filesystem.open_failed",
                    .outsideRoot = "content.filesystem.outside_root",
                    .notRegular = "content.filesystem.not_regular",
                    .tooLarge = "content.filesystem.too_large",
                    .readFailed = "content.filesystem.read_failed",
                    .changedDuringRead = "content.filesystem.changed_during_read"}});
    if (!contents) {
        auto error = std::move(contents.error());
        error.withContext("rootId", std::string{request.rootId})
            .withContext("source", std::string{request.source});
        return core::unexpected(std::move(error));
    }
    ContentBlob result;
    result.bytes = std::move(contents->bytes);
    result.revision = contentRevision(result.span());
    return result;
}

struct MemoryContentProvider::Data final {
    std::map<std::string, MemoryContentEntry, std::less<>> entries;
};

MemoryContentProvider::MemoryContentProvider(std::shared_ptr<const Data> data) noexcept
    : data_(std::move(data)) {}

MemoryContentProvider::~MemoryContentProvider() = default;

auto MemoryContentProvider::create(std::vector<MemoryContentEntry> entries)
    -> core::Result<std::shared_ptr<MemoryContentProvider>> {
    auto data = std::make_shared<Data>();
    for (auto& entry : entries) {
        const ContentRequest request{.rootId = entry.rootId, .source = entry.source};
        if (auto valid = invalidRequest(request); !valid) {
            return core::unexpected(std::move(valid.error()));
        }
        if (entry.revision == 0) {
            return core::unexpected(core::Error{"content.memory.revision_invalid",
                                                "Memory content revision must be non-zero"});
        }
        const auto key = contentKey(entry.rootId, entry.source);
        if (!data->entries.emplace(key, std::move(entry)).second) {
            return core::unexpected(core::Error{"content.memory.source_duplicate",
                                                "Memory content source is duplicated"});
        }
    }
    return std::shared_ptr<MemoryContentProvider>{new MemoryContentProvider{std::move(data)}};
}

auto MemoryContentProvider::readBlob(const ContentRequest& request) -> core::Result<ContentBlob> {
    if (auto valid = invalidRequest(request); !valid) {
        return core::unexpected(std::move(valid.error()));
    }
    const auto found = data_->entries.find(contentKey(request.rootId, request.source));
    if (found == data_->entries.end()) {
        return core::unexpected(core::Error{"content.memory.source_not_found",
                                            "Memory content source was not registered"}
                                    .withContext("rootId", std::string{request.rootId})
                                    .withContext("source", std::string{request.source}));
    }
    return checkedBlob(
        ContentBlob{.bytes = found->second.bytes, .revision = found->second.revision}, request,
        "memory");
}

HostContentProvider::HostContentProvider(HostContentCallback callback) noexcept
    : callback_(std::move(callback)) {}

auto HostContentProvider::create(HostContentCallback callback)
    -> core::Result<std::shared_ptr<HostContentProvider>> {
    if (!callback) {
        return core::unexpected(
            core::Error{"content.host.callback_empty", "Host content callback must be provided"});
    }
    return std::shared_ptr<HostContentProvider>{new HostContentProvider{std::move(callback)}};
}

auto HostContentProvider::readBlob(const ContentRequest& request) -> core::Result<ContentBlob> {
    if (auto valid = invalidRequest(request); !valid) {
        return core::unexpected(std::move(valid.error()));
    }

    thread_local std::vector<const HostContentProvider*> activeProviders;
    if (std::find(activeProviders.begin(), activeProviders.end(), this) != activeProviders.end()) {
        return core::unexpected(core::Error{"content.host.reentrant",
                                            "Host content callback re-entered the same provider"});
    }
    activeProviders.push_back(this);
    struct ActiveCall final {
        std::vector<const HostContentProvider*>& providers;
        ~ActiveCall() {
            providers.pop_back();
        }
    } activeCall{activeProviders};

    try {
        auto result = callback_(request);
        if (!result) {
            return core::unexpected(core::Error{"content.host.callback_failed",
                                                "Host content callback reported a failure"}
                                        .withCause(std::move(result.error())));
        }
        return checkedBlob(std::move(*result), request, "host");
    } catch (const std::exception& exception) {
        return core::unexpected(core::Error{"content.host.callback_exception",
                                            "Host content callback threw an exception"}
                                    .withContext("exception", exception.what()));
    } catch (...) {
        return core::unexpected(core::Error{"content.host.callback_exception",
                                            "Host content callback threw an exception"});
    }
}

} // namespace cuexis::content

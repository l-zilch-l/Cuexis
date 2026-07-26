#pragma once

//  ResourceManager / ResourceLease / ResourceScope — 资源生命周期管理
//  ResourceManager: 持有 AssetDatabase，管理同步 CPU blob 加载和类型化槽位
//  ResourceLease<T>: RAII 强引用（move-only），保证资源在持有期间存活
//  ResourceScope: 批量持有 Lease，对直接和传递依赖去重
//  ResourcePolicy: Required（缺失失败）、Fallback（错误占位资源）、Optional（跳过）
//  contentRevision: 热重载扩展点；当前无文件监听或正式热重载 API

#include <cuexis/assets/asset_database.hpp>
#include <cuexis/assets/resource_handle.hpp>
#include <cuexis/content/content_provider.hpp>
#include <cuexis/core/diagnostic.hpp>
#include <cuexis/core/error.hpp>
#include <cuexis/core/result.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace cuexis::assets {

enum class ResourceState {
    Unloaded,
    Loading,
    Ready,
    Failed,
    Reloading,
};

enum class ResourcePolicy {
    Required,
    Fallback,
    Optional,
};

using ResourceReferencePolicy = ResourcePolicy;

template <typename Tag> struct CpuResource final {
    AssetId id;
    std::shared_ptr<const AssetBlob> blob;

    [[nodiscard]] std::span<const std::byte> bytes() const noexcept {
        return blob ? blob->span() : std::span<const std::byte>{};
    }
};

using MeshResource = CpuResource<MeshTag>;
using MaterialResource = CpuResource<MaterialTag>;
using TextureResource = CpuResource<TextureTag>;

namespace detail {
struct ResourceLeaseControl;
struct ResourceManagerState;
} // namespace detail

template <typename Tag> class ResourceLease final {
  public:
    using Handle = ResourceHandle<Tag>;
    using Resource = CpuResource<Tag>;

    ResourceLease() = default;
    ~ResourceLease() = default;

    ResourceLease(const ResourceLease&) = delete;
    auto operator=(const ResourceLease&) -> ResourceLease& = delete;
    ResourceLease(ResourceLease&&) noexcept = default;
    auto operator=(ResourceLease&&) noexcept -> ResourceLease& = default;

    [[nodiscard]] bool valid() const noexcept {
        return handle_.valid() && resource_ != nullptr && control_ != nullptr;
    }

    explicit operator bool() const noexcept {
        return valid();
    }

    [[nodiscard]] Handle handle() const noexcept {
        return handle_;
    }

    [[nodiscard]] const Resource* get() const noexcept {
        return resource_.get();
    }

    [[nodiscard]] const Resource& resource() const noexcept {
        return *resource_;
    }

    [[nodiscard]] const Resource* operator->() const noexcept {
        return get();
    }

    [[nodiscard]] const Resource& operator*() const noexcept {
        return resource();
    }

    void reset() noexcept {
        resource_.reset();
        control_.reset();
        handle_ = {};
    }

  private:
    friend class ResourceManager;
    friend struct detail::ResourceManagerState;

    ResourceLease(Handle handle, std::shared_ptr<const Resource> resource,
                  std::shared_ptr<detail::ResourceLeaseControl> control) noexcept
        : handle_(handle), resource_(std::move(resource)), control_(std::move(control)) {}

    Handle handle_{};
    std::shared_ptr<const Resource> resource_;
    std::shared_ptr<detail::ResourceLeaseControl> control_;
};

using MeshLease = ResourceLease<MeshTag>;
using MaterialLease = ResourceLease<MaterialTag>;
using TextureLease = ResourceLease<TextureTag>;

template <typename Lease> struct ResourceLoadResult final {
    std::optional<Lease> lease;
    core::Diagnostics diagnostics;

    [[nodiscard]] bool hasValue() const noexcept {
        return lease.has_value() && !diagnostics.hasErrors();
    }

    [[nodiscard]] bool succeeded() const noexcept {
        return !diagnostics.hasErrors();
    }
};

template <typename Handle> struct ResourceRequestResult final {
    std::optional<Handle> handle;
    core::Diagnostics diagnostics;

    [[nodiscard]] bool hasValue() const noexcept {
        return handle.has_value() && handle->valid() && !diagnostics.hasErrors();
    }

    // Optional requests can succeed without producing a handle.
    [[nodiscard]] bool succeeded() const noexcept {
        return !diagnostics.hasErrors();
    }
};

struct ResourceManagerLimits final {
    std::size_t maxBlobBytes{64U * 1024U * 1024U};
    std::size_t maxDependencyDepth{64};
    std::size_t maxDiagnostics{1024};
};

struct ResourceManagerMetrics final {
    std::size_t slots{};
    std::size_t ready{};
    std::size_t failed{};
    std::size_t strongReferences{};
    std::size_t loadedBytes{};
};

class ResourceScope;

class ResourceManager final {
  public:
    explicit ResourceManager(AssetDatabase database, ResourceManagerLimits limits = {});
    ResourceManager(AssetDatabase database, std::shared_ptr<content::IContentProvider> provider,
                    ResourceManagerLimits limits = {});
    ~ResourceManager();

    ResourceManager(const ResourceManager&) = delete;
    auto operator=(const ResourceManager&) -> ResourceManager& = delete;
    ResourceManager(ResourceManager&&) noexcept = delete;
    auto operator=(ResourceManager&&) noexcept -> ResourceManager& = delete;

    // Direct single-resource acquisition.  ResourceScope should be used when a
    // complete dependency closure is required.
    [[nodiscard]] auto loadMesh(const AssetId& id) -> core::Result<MeshLease>;
    [[nodiscard]] auto loadMaterial(const AssetId& id) -> core::Result<MaterialLease>;
    [[nodiscard]] auto loadTexture(const AssetId& id) -> core::Result<TextureLease>;

    [[nodiscard]] auto requestMesh(const AssetId& id, ResourcePolicy policy)
        -> ResourceLoadResult<MeshLease>;
    [[nodiscard]] auto requestMaterial(const AssetId& id, ResourcePolicy policy)
        -> ResourceLoadResult<MaterialLease>;
    [[nodiscard]] auto requestTexture(const AssetId& id, ResourcePolicy policy)
        -> ResourceLoadResult<TextureLease>;

    [[nodiscard]] auto get(MeshHandle handle) const -> core::Result<const MeshResource*>;
    [[nodiscard]] auto get(MaterialHandle handle) const -> core::Result<const MaterialResource*>;
    [[nodiscard]] auto get(TextureHandle handle) const -> core::Result<const TextureResource*>;

    [[nodiscard]] auto state(MeshHandle handle) const -> core::Result<ResourceState>;
    [[nodiscard]] auto state(MaterialHandle handle) const -> core::Result<ResourceState>;
    [[nodiscard]] auto state(TextureHandle handle) const -> core::Result<ResourceState>;

    [[nodiscard]] auto contentRevision(MeshHandle handle) const -> core::Result<std::uint64_t>;
    [[nodiscard]] auto contentRevision(MaterialHandle handle) const -> core::Result<std::uint64_t>;
    [[nodiscard]] auto contentRevision(TextureHandle handle) const -> core::Result<std::uint64_t>;

    [[nodiscard]] std::uint64_t managerToken() const noexcept;
    [[nodiscard]] ResourceManagerMetrics metrics() const noexcept;
    [[nodiscard]] const AssetDatabase& database() const noexcept;

    // Releases an unleased slot immediately and advances its generation.
    [[nodiscard]] auto unload(MeshHandle handle) -> core::Result<void>;
    [[nodiscard]] auto unload(MaterialHandle handle) -> core::Result<void>;
    [[nodiscard]] auto unload(TextureHandle handle) -> core::Result<void>;
    void unloadUnused() noexcept;

    [[nodiscard]] ResourceScope createScope();

  private:
    friend class ResourceScope;

    std::shared_ptr<detail::ResourceManagerState> state_;
};

class ResourceScope final {
  public:
    explicit ResourceScope(ResourceManager& manager) noexcept;
    ~ResourceScope() = default;

    ResourceScope(const ResourceScope&) = delete;
    auto operator=(const ResourceScope&) -> ResourceScope& = delete;
    ResourceScope(ResourceScope&& other) noexcept;
    auto operator=(ResourceScope&& other) noexcept -> ResourceScope&;

    // Each request is transactional with respect to this Scope.  A required
    // failure removes leases added by that request while preserving earlier
    // successful requests.
    [[nodiscard]] auto requestMesh(const AssetId& id,
                                   ResourcePolicy policy = ResourcePolicy::Required)
        -> ResourceRequestResult<MeshHandle>;
    [[nodiscard]] auto requestMaterial(const AssetId& id,
                                       ResourcePolicy policy = ResourcePolicy::Required)
        -> ResourceRequestResult<MaterialHandle>;
    [[nodiscard]] auto requestTexture(const AssetId& id,
                                      ResourcePolicy policy = ResourcePolicy::Required)
        -> ResourceRequestResult<TextureHandle>;

    // Naming aliases for call sites that describe the operation as acquisition.
    [[nodiscard]] auto acquireMesh(const AssetId& id,
                                   ResourcePolicy policy = ResourcePolicy::Required)
        -> ResourceRequestResult<MeshHandle> {
        return requestMesh(id, policy);
    }
    [[nodiscard]] auto acquireMaterial(const AssetId& id,
                                       ResourcePolicy policy = ResourcePolicy::Required)
        -> ResourceRequestResult<MaterialHandle> {
        return requestMaterial(id, policy);
    }
    [[nodiscard]] auto acquireTexture(const AssetId& id,
                                      ResourcePolicy policy = ResourcePolicy::Required)
        -> ResourceRequestResult<TextureHandle> {
        return requestTexture(id, policy);
    }

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] bool contains(AssetType type, const AssetId& id) const noexcept;
    [[nodiscard]] bool contains(MeshHandle handle) const noexcept;
    [[nodiscard]] bool contains(MaterialHandle handle) const noexcept;
    [[nodiscard]] bool contains(TextureHandle handle) const noexcept;
    [[nodiscard]] std::uint64_t managerToken() const noexcept;
    void clear() noexcept;

  private:
    struct EntryKey final {
        AssetType type{AssetType::Mesh};
        AssetId id;
    };

    struct EntryKeyView final {
        AssetType type{AssetType::Mesh};
        std::string_view id;
    };

    struct EntryKeyLess final {
        using is_transparent = void;

        [[nodiscard]] bool operator()(const EntryKey& left, const EntryKey& right) const noexcept;
        [[nodiscard]] bool operator()(const EntryKey& left, EntryKeyView right) const noexcept;
        [[nodiscard]] bool operator()(EntryKeyView left, const EntryKey& right) const noexcept;
    };

    enum class EntryResolution {
        Loaded,
        Fallback,
    };

    struct Entry final {
        AssetType type{AssetType::Mesh};
        AssetId requestedId;
        EntryResolution resolution{EntryResolution::Loaded};
        ResourcePolicy acquisitionPolicy{ResourcePolicy::Required};
        std::optional<core::Error> failureCause;
        std::variant<MeshLease, MaterialLease, TextureLease> lease;
    };

    struct UntypedRequestResult final {
        std::optional<std::variant<MeshHandle, MaterialHandle, TextureHandle>> handle;
        core::Diagnostics diagnostics;
    };

    [[nodiscard]] auto request(AssetType type, const AssetId& id, ResourcePolicy policy)
        -> UntypedRequestResult;
    void rollbackTo(std::size_t size) noexcept;

    std::weak_ptr<detail::ResourceManagerState> state_;
    std::vector<Entry> entries_;
    std::map<EntryKey, std::size_t, EntryKeyLess> entryIndex_;
};

} // namespace cuexis::assets

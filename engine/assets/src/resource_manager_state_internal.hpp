#pragma once

// ResourceManager state shared by the manager facade and the scope unit. The state owns the
// typed pools, the lease accounting and the diagnostics budget, so both units need its
// definition while only one needs its construction.

#include "resource_manager_resource_detail.hpp"

#include <cuexis/assets/resource_manager.hpp>
#include <cuexis/core/error.hpp>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace cuexis::assets {
namespace detail {

struct ResourceLeaseControl final {
    std::weak_ptr<ResourceManagerState> state;
    AssetType type{AssetType::Mesh};
    std::uint32_t index{};
    std::uint32_t generation{};

    ~ResourceLeaseControl();
};

struct ResourceManagerState final : std::enable_shared_from_this<ResourceManagerState> {
    explicit ResourceManagerState(AssetDatabase assetDatabase,
                                  std::shared_ptr<content::IContentProvider> contentProvider,
                                  ResourceManagerLimits resourceLimits)
        : database(std::move(assetDatabase)), provider(std::move(contentProvider)),
          limits(resourceLimits), token(allocateToken()), ownerThread(std::this_thread::get_id()) {
        initializeFallback<MeshTag>();
        initializeFallback<MaterialTag>();
        initializeFallback<TextureTag>();
        initializeFallback<AudioSourceTag>();
        initializeFallback<ShaderTag>();
    }

    static auto allocateToken() noexcept -> std::uint64_t {
        auto token = nextManagerToken.fetch_add(1, std::memory_order_relaxed);
        if (token == 0) {
            token = nextManagerToken.fetch_add(1, std::memory_order_relaxed);
        }
        return token;
    }

    [[nodiscard]] bool isOwnerThread() const noexcept {
        return ownerThread == std::this_thread::get_id();
    }

    template <typename Tag> auto pool() -> Pool<Tag>& {
        if constexpr (std::is_same_v<Tag, MeshTag>) {
            return meshes;
        } else if constexpr (std::is_same_v<Tag, MaterialTag>) {
            return materials;
        } else if constexpr (std::is_same_v<Tag, TextureTag>) {
            return textures;
        } else if constexpr (std::is_same_v<Tag, ShaderTag>) {
            return shaders;
        } else {
            static_assert(std::is_same_v<Tag, AudioSourceTag>);
            return audioSources;
        }
    }

    template <typename Tag> auto pool() const -> const Pool<Tag>& {
        if constexpr (std::is_same_v<Tag, MeshTag>) {
            return meshes;
        } else if constexpr (std::is_same_v<Tag, MaterialTag>) {
            return materials;
        } else if constexpr (std::is_same_v<Tag, TextureTag>) {
            return textures;
        } else if constexpr (std::is_same_v<Tag, ShaderTag>) {
            return shaders;
        } else {
            static_assert(std::is_same_v<Tag, AudioSourceTag>);
            return audioSources;
        }
    }

    template <typename Tag> void initializeFallback() {
        auto& typedPool = pool<Tag>();
        const auto id = AssetId{std::string{ResourceTraits<Tag>::fallbackId}};
        auto resource = std::make_shared<CpuResource<Tag>>(CpuResource<Tag>{
            id,
            makeBlob(ResourceTraits<Tag>::fallbackPayload,
                     "builtin/" + std::string{assetTypeName(ResourceTraits<Tag>::type)}),
        });
        typedPool.slots.push_back(Slot<Tag>{
            .id = id,
            .state = ResourceState::Ready,
            .generation = 1,
            .contentRevision = 1,
            .strongReferences = 0,
            .resource = std::move(resource),
            .pinned = true,
            .retired = false,
        });
        typedPool.byId.emplace(id.value, 0);
        typedPool.fallbackIndex = 0;
    }

    template <typename Tag> auto makeLease(std::uint32_t index) -> ResourceLease<Tag> {
        auto& slot = pool<Tag>().slots[index];
        ++slot.strongReferences;
        auto control = std::make_shared<ResourceLeaseControl>();
        control->state = weak_from_this();
        control->type = ResourceTraits<Tag>::type;
        control->index = index;
        control->generation = slot.generation;
        return ResourceLease<Tag>{ResourceHandle<Tag>{index, slot.generation, token}, slot.resource,
                                  std::move(control)};
    }

    template <typename Tag> auto load(const AssetId& id) -> core::Result<ResourceLease<Tag>> {
        if (!isOwnerThread()) {
            return core::unexpected(core::Error{"assets.resource.not_owner_thread",
                                                "ResourceManager belongs to another thread"});
        }
        const auto* record = database.find(id);
        if (record == nullptr) {
            return core::unexpected(
                core::Error{"assets.asset.not_found", "AssetId does not exist in AssetDatabase"}
                    .withContext("assetId", id.value));
        }
        if (record->type != ResourceTraits<Tag>::type) {
            return core::unexpected(
                core::Error{"assets.resource.type_mismatch",
                            "Asset type does not match the requested resource type"}
                    .withContext("assetId", id.value)
                    .withContext("expected", std::string{assetTypeName(ResourceTraits<Tag>::type)})
                    .withContext("actual", std::string{assetTypeName(record->type)}));
        }

        std::uint32_t index{};
        {
            std::scoped_lock lock{mutex};
            auto& typedPool = pool<Tag>();
            const auto existing = typedPool.byId.find(id.value);
            if (existing != typedPool.byId.end()) {
                index = existing->second;
                auto& slot = typedPool.slots[index];
                if (slot.state == ResourceState::Ready && slot.resource != nullptr) {
                    return makeLease<Tag>(index);
                }
                slot.state = ResourceState::Loading;
            } else {
                if (typedPool.slots.size() >= ResourceHandle<Tag>::invalidIndex) {
                    return core::unexpected(core::Error{"assets.resource.slot_limit",
                                                        "Resource slot index space is exhausted"});
                }
                index = static_cast<std::uint32_t>(typedPool.slots.size());
                typedPool.slots.push_back(Slot<Tag>{
                    .id = id,
                    .state = ResourceState::Loading,
                    .generation = 1,
                    .contentRevision = 0,
                    .strongReferences = 0,
                    .resource = {},
                    .pinned = false,
                    .retired = false,
                });
                typedPool.byId.emplace(id.value, index);
            }
        }

        if (!provider) {
            std::scoped_lock lock{mutex};
            auto& slot = pool<Tag>().slots[index];
            slot.state = ResourceState::Failed;
            slot.resource.reset();
            return core::unexpected(core::Error{"assets.resource.provider_missing",
                                                "ResourceManager has no content provider"}
                                        .withContext("assetId", id.value));
        }

        const auto rootId = database.rootIdOf(id);
        auto providerBlob = [&]() -> core::Result<content::ContentBlob> {
            try {
                return provider->readBlob(
                    {.rootId = rootId, .source = record->source, .maxBytes = limits.maxBlobBytes});
            } catch (const std::exception& exception) {
                return core::unexpected(core::Error{"assets.resource.provider_exception",
                                                    "Content provider threw an exception"}
                                            .withContext("exception", exception.what()));
            } catch (...) {
                return core::unexpected(core::Error{"assets.resource.provider_exception",
                                                    "Content provider threw an exception"});
            }
        }();
        if (!providerBlob || providerBlob->bytes.size() > limits.maxBlobBytes) {
            std::scoped_lock lock{mutex};
            auto& slot = pool<Tag>().slots[index];
            slot.state = ResourceState::Failed;
            slot.resource.reset();
            auto cause = providerBlob
                             ? core::Error{"assets.resource.provider_too_large",
                                           "Content provider exceeded the resource byte limit"}
                             : std::move(providerBlob.error());
            return core::unexpected(
                core::Error{"assets.resource.load_failed", "CPU resource blob could not be loaded"}
                    .withContext("assetId", id.value)
                    .withContext("rootId", std::string{rootId})
                    .withContext("source", record->source)
                    .withCause(std::move(cause)));
        }

        auto sharedBlob = std::make_shared<AssetBlob>();
        sharedBlob->rootId = std::string{rootId};
        sharedBlob->source = record->source;
        sharedBlob->providerRevision = providerBlob->revision;
        sharedBlob->bytes = std::move(providerBlob->bytes);
        auto resource = std::make_shared<CpuResource<Tag>>(CpuResource<Tag>{id, sharedBlob});
        std::scoped_lock lock{mutex};
        auto& slot = pool<Tag>().slots[index];
        slot.resource = std::move(resource);
        slot.state = ResourceState::Ready;
        ++slot.contentRevision;
        if (slot.contentRevision == 0) {
            slot.contentRevision = 1;
        }
        return makeLease<Tag>(index);
    }

    template <typename Tag> auto fallback() -> core::Result<ResourceLease<Tag>> {
        if (!isOwnerThread()) {
            return core::unexpected(core::Error{"assets.resource.not_owner_thread",
                                                "ResourceManager belongs to another thread"});
        }
        std::scoped_lock lock{mutex};
        return makeLease<Tag>(pool<Tag>().fallbackIndex);
    }

    template <typename Tag>
    auto get(ResourceHandle<Tag> handle) const -> core::Result<const CpuResource<Tag>*> {
        if (!isOwnerThread()) {
            return core::unexpected(core::Error{"assets.resource.not_owner_thread",
                                                "ResourceManager belongs to another thread"});
        }
        std::scoped_lock lock{mutex};
        auto slot = checkedSlot<Tag>(handle);
        if (!slot) {
            return core::unexpected(std::move(slot.error()));
        }
        if ((*slot)->state != ResourceState::Ready || (*slot)->resource == nullptr) {
            return core::unexpected(core::Error{
                "assets.resource.not_ready", "Resource handle does not refer to a ready resource"});
        }
        return (*slot)->resource.get();
    }

    template <typename Tag>
    auto stateOf(ResourceHandle<Tag> handle) const -> core::Result<ResourceState> {
        if (!isOwnerThread()) {
            return core::unexpected(core::Error{"assets.resource.not_owner_thread",
                                                "ResourceManager belongs to another thread"});
        }
        std::scoped_lock lock{mutex};
        auto slot = checkedSlot<Tag>(handle);
        if (!slot) {
            return core::unexpected(std::move(slot.error()));
        }
        return (*slot)->state;
    }

    template <typename Tag>
    auto revisionOf(ResourceHandle<Tag> handle) const -> core::Result<std::uint64_t> {
        if (!isOwnerThread()) {
            return core::unexpected(core::Error{"assets.resource.not_owner_thread",
                                                "ResourceManager belongs to another thread"});
        }
        std::scoped_lock lock{mutex};
        auto slot = checkedSlot<Tag>(handle);
        if (!slot) {
            return core::unexpected(std::move(slot.error()));
        }
        return (*slot)->contentRevision;
    }

    template <typename Tag> auto unload(ResourceHandle<Tag> handle) -> core::Result<void> {
        if (!isOwnerThread()) {
            return core::unexpected(core::Error{"assets.resource.not_owner_thread",
                                                "ResourceManager belongs to another thread"});
        }
        std::scoped_lock lock{mutex};
        auto slotResult = checkedSlot<Tag>(handle);
        if (!slotResult) {
            return core::unexpected(std::move(slotResult.error()));
        }
        auto& slot = **slotResult;
        if (slot.pinned) {
            return core::unexpected(core::Error{"assets.resource.fallback_pinned",
                                                "Built-in fallback resources cannot be unloaded"});
        }
        if (slot.strongReferences != 0) {
            return core::unexpected(
                core::Error{"assets.resource.in_use", "Resource still has strong references"}
                    .withContext("assetId", slot.id.value));
        }
        releaseSlot<Tag>(handle.index, slot);
        return {};
    }

    template <typename Tag>
    auto checkedSlot(ResourceHandle<Tag> handle) const -> core::Result<const Slot<Tag>*> {
        if (!handle.valid()) {
            return core::unexpected(core::Error{"assets.resource.invalid_handle",
                                                "Resource handle is structurally invalid"});
        }
        if (handle.managerToken == 0 || handle.managerToken != token) {
            return core::unexpected(core::Error{"assets.resource.manager_mismatch",
                                                "Resource handle belongs to another manager"});
        }
        const auto& slots = pool<Tag>().slots;
        if (handle.index >= slots.size()) {
            return core::unexpected(core::Error{"assets.resource.invalid_index",
                                                "Resource handle index is out of range"});
        }
        const auto& slot = slots[handle.index];
        if (slot.retired || handle.generation != slot.generation) {
            return core::unexpected(
                core::Error{"assets.resource.stale_handle", "Resource handle generation is stale"});
        }
        return &slot;
    }

    template <typename Tag>
    auto checkedSlot(ResourceHandle<Tag> handle) -> core::Result<Slot<Tag>*> {
        auto result = std::as_const(*this).checkedSlot<Tag>(handle);
        if (!result) {
            return core::unexpected(std::move(result.error()));
        }
        return const_cast<Slot<Tag>*>(*result);
    }

    template <typename Tag> void releaseSlot(std::uint32_t index, Slot<Tag>& slot) noexcept {
        slot.resource.reset();
        slot.state = ResourceState::Unloaded;
        if (slot.generation == std::numeric_limits<std::uint32_t>::max()) {
            slot.retired = true;
            pool<Tag>().byId.erase(slot.id.value);
            return;
        }
        ++slot.generation;
        (void)index;
    }

    template <typename Tag>
    void releaseLease(std::uint32_t index, std::uint32_t generation) noexcept {
        std::scoped_lock lock{mutex};
        auto& slots = pool<Tag>().slots;
        if (index >= slots.size()) {
            return;
        }
        auto& slot = slots[index];
        if (slot.generation != generation || slot.strongReferences == 0) {
            return;
        }
        --slot.strongReferences;
        if (slot.strongReferences == 0 && !slot.pinned) {
            releaseSlot<Tag>(index, slot);
        }
    }

    void releaseLease(AssetType type, std::uint32_t index, std::uint32_t generation) noexcept {
        switch (type) {
        case AssetType::Mesh:
            releaseLease<MeshTag>(index, generation);
            break;
        case AssetType::Material:
            releaseLease<MaterialTag>(index, generation);
            break;
        case AssetType::Texture:
            releaseLease<TextureTag>(index, generation);
            break;
        case AssetType::Audio:
            releaseLease<AudioSourceTag>(index, generation);
            break;
        case AssetType::Shader:
            releaseLease<ShaderTag>(index, generation);
            break;
        }
    }

    template <typename Tag> void unloadUnused() noexcept {
        auto& typedPool = pool<Tag>();
        for (std::uint32_t index = 0; index < typedPool.slots.size(); ++index) {
            auto& slot = typedPool.slots[index];
            if (!slot.pinned && !slot.retired && slot.strongReferences == 0 &&
                slot.state != ResourceState::Unloaded) {
                releaseSlot<Tag>(index, slot);
            }
        }
    }

    AssetDatabase database;
    std::shared_ptr<content::IContentProvider> provider;
    ResourceManagerLimits limits;
    std::uint64_t token{};
    std::thread::id ownerThread;
    mutable std::mutex mutex;
    Pool<MeshTag> meshes;
    Pool<MaterialTag> materials;
    Pool<TextureTag> textures;
    Pool<AudioSourceTag> audioSources;
    Pool<ShaderTag> shaders;
};

inline ResourceLeaseControl::~ResourceLeaseControl() {

    if (const auto locked = state.lock()) {
        locked->releaseLease(type, index, generation);
    }
}

inline void appendErrorDiagnostic(core::Diagnostics& diagnostics, const core::Error& error,
                                  core::DiagnosticSeverity severity, std::string code,
                                  std::string message, const AssetId& id) {
    auto diagnostic = core::Diagnostic{severity, std::move(code), std::move(message), "$"};
    diagnostic.withContext("assetId", id.value).withContext("cause", std::string{error.code()});
    for (const auto& context : error.context()) {
        diagnostic.withContext(context.key, context.value);
    }
    diagnostics.add(std::move(diagnostic));
}

template <typename Tag>
auto handleFromEntry(const std::variant<MeshLease, MaterialLease, TextureLease, AudioSourceLease,
                                        ShaderLease>& lease) -> ResourceHandle<Tag> {
    return std::get<ResourceLease<Tag>>(lease).handle();
}

inline auto handleFromEntry(AssetType type,
                            const std::variant<MeshLease, MaterialLease, TextureLease,
                                               AudioSourceLease, ShaderLease>& lease)
    -> std::variant<MeshHandle, MaterialHandle, TextureHandle, AudioSourceHandle, ShaderHandle> {
    switch (type) {
    case AssetType::Mesh:
        return handleFromEntry<MeshTag>(lease);
    case AssetType::Material:
        return handleFromEntry<MaterialTag>(lease);
    case AssetType::Texture:
        return handleFromEntry<TextureTag>(lease);
    case AssetType::Audio:
        return handleFromEntry<AudioSourceTag>(lease);
    case AssetType::Shader:
        return handleFromEntry<ShaderTag>(lease);
    }
    return MeshHandle{};
}

} // namespace detail
} // namespace cuexis::assets

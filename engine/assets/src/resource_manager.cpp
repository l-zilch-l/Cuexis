// ResourceManager facade: typed load, request and unload.

#include <cuexis/assets/resource_manager.hpp>

#include "resource_manager_state_internal.hpp"

#include <cuexis/core/error.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace cuexis::assets {

using detail::appendErrorDiagnostic;
using detail::handleFromEntry;
using detail::resource_detail::makeDiagnostics;
template <typename Tag>
auto requestDirect(const std::shared_ptr<detail::ResourceManagerState>& state, const AssetId& id,
                   ResourcePolicy policy) -> ResourceLoadResult<ResourceLease<Tag>> {
    auto diagnostics = makeDiagnostics(state->limits.maxDiagnostics);
    auto loaded = state->load<Tag>(id);
    if (loaded) {
        return {std::optional<ResourceLease<Tag>>{std::move(*loaded)}, std::move(diagnostics)};
    }

    switch (policy) {
    case ResourcePolicy::Required:
        appendErrorDiagnostic(diagnostics, loaded.error(), core::DiagnosticSeverity::Error,
                              "assets.resource.required_failed",
                              "Required resource could not be loaded", id);
        break;
    case ResourcePolicy::Fallback: {
        auto fallback = state->fallback<Tag>();
        if (!fallback) {
            appendErrorDiagnostic(diagnostics, fallback.error(), core::DiagnosticSeverity::Error,
                                  "assets.resource.fallback_failed",
                                  "Built-in fallback resource could not be acquired", id);
            break;
        }
        appendErrorDiagnostic(diagnostics, loaded.error(), core::DiagnosticSeverity::Warning,
                              "assets.resource.fallback_used",
                              "Built-in fallback resource was used", id);
        diagnostics.sortDeterministically();
        return {std::optional<ResourceLease<Tag>>{std::move(*fallback)}, std::move(diagnostics)};
    }
    case ResourcePolicy::Optional:
        appendErrorDiagnostic(diagnostics, loaded.error(), core::DiagnosticSeverity::Warning,
                              "assets.resource.optional_skipped", "Optional resource was skipped",
                              id);
        break;
    }
    diagnostics.sortDeterministically();
    return {std::nullopt, std::move(diagnostics)};
}

ResourceManager::ResourceManager(AssetDatabase database, ResourceManagerLimits limits)
    : ResourceManager(database, database.defaultContentProvider(), limits) {}

ResourceManager::ResourceManager(AssetDatabase database,
                                 std::shared_ptr<content::IContentProvider> provider,
                                 ResourceManagerLimits limits)
    : state_(std::make_shared<detail::ResourceManagerState>(std::move(database),
                                                            std::move(provider), limits)) {}

ResourceManager::~ResourceManager() = default;

auto ResourceManager::loadMesh(const AssetId& id) -> core::Result<MeshLease> {
    return state_->load<MeshTag>(id);
}

auto ResourceManager::loadMaterial(const AssetId& id) -> core::Result<MaterialLease> {
    return state_->load<MaterialTag>(id);
}

auto ResourceManager::loadTexture(const AssetId& id) -> core::Result<TextureLease> {
    return state_->load<TextureTag>(id);
}

auto ResourceManager::loadAudioSource(const AssetId& id) -> core::Result<AudioSourceLease> {
    return state_->load<AudioSourceTag>(id);
}

auto ResourceManager::loadShader(const AssetId& id) -> core::Result<ShaderLease> {
    return state_->load<ShaderTag>(id);
}

auto ResourceManager::requestMesh(const AssetId& id, ResourcePolicy policy)
    -> ResourceLoadResult<MeshLease> {
    return requestDirect<MeshTag>(state_, id, policy);
}

auto ResourceManager::requestMaterial(const AssetId& id, ResourcePolicy policy)
    -> ResourceLoadResult<MaterialLease> {
    return requestDirect<MaterialTag>(state_, id, policy);
}

auto ResourceManager::requestTexture(const AssetId& id, ResourcePolicy policy)
    -> ResourceLoadResult<TextureLease> {
    return requestDirect<TextureTag>(state_, id, policy);
}

auto ResourceManager::requestAudioSource(const AssetId& id, ResourcePolicy policy)
    -> ResourceLoadResult<AudioSourceLease> {
    return requestDirect<AudioSourceTag>(state_, id, policy);
}

auto ResourceManager::requestShader(const AssetId& id, ResourcePolicy policy)
    -> ResourceLoadResult<ShaderLease> {
    return requestDirect<ShaderTag>(state_, id, policy);
}

auto ResourceManager::get(MeshHandle handle) const -> core::Result<const MeshResource*> {
    return state_->get<MeshTag>(handle);
}

auto ResourceManager::get(MaterialHandle handle) const -> core::Result<const MaterialResource*> {
    return state_->get<MaterialTag>(handle);
}

auto ResourceManager::get(TextureHandle handle) const -> core::Result<const TextureResource*> {
    return state_->get<TextureTag>(handle);
}

auto ResourceManager::get(AudioSourceHandle handle) const
    -> core::Result<const AudioSourceResource*> {
    return state_->get<AudioSourceTag>(handle);
}

auto ResourceManager::get(ShaderHandle handle) const -> core::Result<const ShaderResource*> {
    return state_->get<ShaderTag>(handle);
}

auto ResourceManager::state(MeshHandle handle) const -> core::Result<ResourceState> {
    return state_->stateOf<MeshTag>(handle);
}

auto ResourceManager::state(MaterialHandle handle) const -> core::Result<ResourceState> {
    return state_->stateOf<MaterialTag>(handle);
}

auto ResourceManager::state(TextureHandle handle) const -> core::Result<ResourceState> {
    return state_->stateOf<TextureTag>(handle);
}

auto ResourceManager::state(AudioSourceHandle handle) const -> core::Result<ResourceState> {
    return state_->stateOf<AudioSourceTag>(handle);
}

auto ResourceManager::state(ShaderHandle handle) const -> core::Result<ResourceState> {
    return state_->stateOf<ShaderTag>(handle);
}

auto ResourceManager::contentRevision(MeshHandle handle) const -> core::Result<std::uint64_t> {
    return state_->revisionOf<MeshTag>(handle);
}

auto ResourceManager::contentRevision(MaterialHandle handle) const -> core::Result<std::uint64_t> {
    return state_->revisionOf<MaterialTag>(handle);
}

auto ResourceManager::contentRevision(TextureHandle handle) const -> core::Result<std::uint64_t> {
    return state_->revisionOf<TextureTag>(handle);
}

auto ResourceManager::contentRevision(AudioSourceHandle handle) const
    -> core::Result<std::uint64_t> {
    return state_->revisionOf<AudioSourceTag>(handle);
}

auto ResourceManager::contentRevision(ShaderHandle handle) const -> core::Result<std::uint64_t> {
    return state_->revisionOf<ShaderTag>(handle);
}

std::uint64_t ResourceManager::managerToken() const noexcept {
    return state_ ? state_->token : 0;
}

ResourceManagerMetrics ResourceManager::metrics() const noexcept {
    ResourceManagerMetrics result;
    if (!state_) {
        return result;
    }
    std::scoped_lock lock{state_->mutex};
    const auto accumulate = [&result](const auto& pool) {
        for (const auto& slot : pool.slots) {
            if (slot.pinned || slot.retired) {
                continue;
            }
            ++result.slots;
            result.strongReferences += slot.strongReferences;
            if (slot.state == ResourceState::Ready && slot.resource != nullptr) {
                ++result.ready;
                result.loadedBytes += slot.resource->bytes().size();
            } else if (slot.state == ResourceState::Failed) {
                ++result.failed;
            }
        }
    };
    accumulate(state_->meshes);
    accumulate(state_->materials);
    accumulate(state_->textures);
    accumulate(state_->audioSources);
    accumulate(state_->shaders);
    return result;
}

const AssetDatabase& ResourceManager::database() const noexcept {
    static const AssetDatabase emptyDatabase;
    return state_ ? state_->database : emptyDatabase;
}

auto ResourceManager::unload(MeshHandle handle) -> core::Result<void> {
    return state_->unload<MeshTag>(handle);
}

auto ResourceManager::unload(MaterialHandle handle) -> core::Result<void> {
    return state_->unload<MaterialTag>(handle);
}

auto ResourceManager::unload(TextureHandle handle) -> core::Result<void> {
    return state_->unload<TextureTag>(handle);
}

auto ResourceManager::unload(AudioSourceHandle handle) -> core::Result<void> {
    return state_->unload<AudioSourceTag>(handle);
}

auto ResourceManager::unload(ShaderHandle handle) -> core::Result<void> {
    return state_->unload<ShaderTag>(handle);
}

void ResourceManager::unloadUnused() noexcept {
    if (!state_) {
        return;
    }
    std::scoped_lock lock{state_->mutex};
    state_->unloadUnused<MeshTag>();
    state_->unloadUnused<MaterialTag>();
    state_->unloadUnused<TextureTag>();
    state_->unloadUnused<AudioSourceTag>();
    state_->unloadUnused<ShaderTag>();
}

ResourceScope ResourceManager::createScope() {
    return ResourceScope{*this};
}
} // namespace cuexis::assets

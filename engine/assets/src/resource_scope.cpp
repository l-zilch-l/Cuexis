// ResourceScope: dependency deduplication and transaction rollback.

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
bool ResourceScope::EntryKeyLess::operator()(const EntryKey& left,
                                             const EntryKey& right) const noexcept {
    if (left.type != right.type) {
        return left.type < right.type;
    }
    return left.id.value < right.id.value;
}

bool ResourceScope::EntryKeyLess::operator()(const EntryKey& left,
                                             EntryKeyView right) const noexcept {
    if (left.type != right.type) {
        return left.type < right.type;
    }
    return left.id.value < right.id;
}

bool ResourceScope::EntryKeyLess::operator()(EntryKeyView left,
                                             const EntryKey& right) const noexcept {
    if (left.type != right.type) {
        return left.type < right.type;
    }
    return left.id < right.id.value;
}

struct ResourceScope::Storage final {
    std::vector<Entry> entries;
    std::map<EntryKey, std::size_t, EntryKeyLess> index;
};

ResourceScope::ResourceScope(ResourceManager& manager)
    : state_(manager.state_), storage_(std::make_unique<Storage>()) {}
ResourceScope::~ResourceScope() = default;

ResourceScope::ResourceScope(ResourceScope&& other) noexcept
    : state_(std::move(other.state_)), storage_(std::move(other.storage_)) {
    other.state_.reset();
}

auto ResourceScope::operator=(ResourceScope&& other) noexcept -> ResourceScope& {
    if (this != &other) {
        storage_ = std::move(other.storage_);
        state_ = std::move(other.state_);
        other.state_.reset();
    }
    return *this;
}

auto ResourceScope::request(AssetType type, const AssetId& id, ResourcePolicy policy)
    -> UntypedRequestResult {
    const auto state = state_.lock();
    auto diagnostics = makeDiagnostics(state ? state->limits.maxDiagnostics : 1);
    if (!state) {
        diagnostics.add(core::Diagnostic{core::DiagnosticSeverity::Error,
                                         "assets.resource.manager_expired",
                                         "ResourceScope manager no longer exists", "$"});
        return {std::nullopt, std::move(diagnostics)};
    }
    if (!state->isOwnerThread()) {
        diagnostics.add(core::Diagnostic{core::DiagnosticSeverity::Error,
                                         "assets.resource.not_owner_thread",
                                         "ResourceScope belongs to another thread", "$"});
        return {std::nullopt, std::move(diagnostics)};
    }

    const auto existing = storage_->index.find(EntryKeyView{type, id.value});
    if (existing != storage_->index.end()) {
        const auto& entry = storage_->entries[existing->second];
        if (entry.resolution == EntryResolution::Loaded) {
            return {handleFromEntry(entry.type, entry.lease), std::move(diagnostics)};
        }

        const auto cause = entry.failureCause.value_or(core::Error{
            "assets.resource.cached_fallback", "A previous request resolved to a fallback"});
        switch (policy) {
        case ResourcePolicy::Required:
            appendErrorDiagnostic(diagnostics, cause, core::DiagnosticSeverity::Error,
                                  "assets.resource.required_failed",
                                  "Required resource cannot reuse a cached fallback", id);
            diagnostics.sortDeterministically();
            return {std::nullopt, std::move(diagnostics)};
        case ResourcePolicy::Fallback:
            appendErrorDiagnostic(diagnostics, cause, core::DiagnosticSeverity::Warning,
                                  "assets.resource.fallback_used",
                                  "Built-in fallback resource was used", id);
            diagnostics.sortDeterministically();
            return {handleFromEntry(entry.type, entry.lease), std::move(diagnostics)};
        case ResourcePolicy::Optional:
            appendErrorDiagnostic(diagnostics, cause, core::DiagnosticSeverity::Warning,
                                  "assets.resource.optional_skipped",
                                  "Optional resource was skipped instead of reusing a fallback",
                                  id);
            diagnostics.sortDeterministically();
            return {std::nullopt, std::move(diagnostics)};
        }
    }

    const auto originalSize = storage_->entries.size();
    const auto addEntry = [&](Entry entry) {
        const auto keyView = EntryKeyView{entry.type, entry.requestedId.value};
        if (storage_->index.contains(keyView)) {
            return false;
        }
        auto key = EntryKey{entry.type, entry.requestedId};
        const auto index = storage_->entries.size();
        storage_->entries.push_back(std::move(entry));
        storage_->index.emplace(std::move(key), index);
        return true;
    };
    std::vector<std::pair<AssetType, AssetId>> visiting;
    const auto acquire = [&](auto&& self, AssetType requestedType, const AssetId& requestedId,
                             std::size_t depth)
        -> core::Result<std::variant<MeshHandle, MaterialHandle, TextureHandle, AudioSourceHandle,
                                     ShaderHandle>> {
        const auto held = storage_->index.find(EntryKeyView{requestedType, requestedId.value});
        if (held != storage_->index.end()) {
            const auto& entry = storage_->entries[held->second];
            if (entry.resolution == EntryResolution::Loaded) {
                return handleFromEntry(entry.type, entry.lease);
            }
            auto error = core::Error{
                "assets.resource.cached_fallback_not_required",
                "A required dependency cannot reuse a fallback cached by an earlier request"};
            error.withContext("assetId", requestedId.value);
            if (entry.failureCause) {
                error.withCause(*entry.failureCause);
            }
            return core::unexpected(std::move(error));
        }
        if (depth > state->limits.maxDependencyDepth) {
            return core::unexpected(core::Error{"assets.resource.dependency_depth",
                                                "Resource dependency depth limit was exceeded"}
                                        .withContext("assetId", requestedId.value));
        }
        if (std::find(visiting.begin(), visiting.end(), std::pair{requestedType, requestedId}) !=
            visiting.end()) {
            std::string cycle;
            for (const auto& [cycleType, cycleId] : visiting) {
                if (!cycle.empty()) {
                    cycle += " -> ";
                }
                cycle += std::string{assetTypeName(cycleType)} + ":" + cycleId.value;
            }
            cycle += " -> " + std::string{assetTypeName(requestedType)} + ":" + requestedId.value;
            return core::unexpected(core::Error{"assets.resource.dependency_cycle",
                                                "Resource dependency graph contains a cycle"}
                                        .withContext("cycle", std::move(cycle)));
        }

        const auto* record = state->database.find(requestedId);
        if (record == nullptr) {
            return core::unexpected(
                core::Error{"assets.asset.not_found", "AssetId does not exist in AssetDatabase"}
                    .withContext("assetId", requestedId.value));
        }
        if (record->type != requestedType) {
            return core::unexpected(
                core::Error{"assets.resource.type_mismatch",
                            "Asset type does not match the requested resource type"}
                    .withContext("assetId", requestedId.value)
                    .withContext("expected", std::string{assetTypeName(requestedType)})
                    .withContext("actual", std::string{assetTypeName(record->type)}));
        }

        visiting.emplace_back(requestedType, requestedId);
        for (const auto& dependency : record->dependencies) {
            const auto* dependencyRecord = state->database.find(dependency);
            if (dependencyRecord == nullptr) {
                visiting.pop_back();
                return core::unexpected(core::Error{"assets.resource.dependency_missing",
                                                    "Resource dependency does not exist"}
                                            .withContext("assetId", requestedId.value)
                                            .withContext("dependency", dependency.value));
            }
            auto dependencyHandle = self(self, dependencyRecord->type, dependency, depth + 1);
            if (!dependencyHandle) {
                visiting.pop_back();
                return core::unexpected(core::Error{"assets.resource.dependency_failed",
                                                    "Resource dependency could not be loaded"}
                                            .withContext("assetId", requestedId.value)
                                            .withContext("dependency", dependency.value)
                                            .withCause(std::move(dependencyHandle.error())));
            }
        }
        visiting.pop_back();

        switch (requestedType) {
        case AssetType::Mesh: {
            auto lease = state->load<MeshTag>(requestedId);
            if (!lease) {
                return core::unexpected(std::move(lease.error()));
            }
            const auto handle = lease->handle();
            if (!addEntry(Entry{.type = requestedType,
                                .requestedId = requestedId,
                                .resolution = EntryResolution::Loaded,
                                .acquisitionPolicy = ResourcePolicy::Required,
                                .failureCause = std::nullopt,
                                .lease = std::move(*lease)})) {
                return core::unexpected(
                    core::Error{"assets.resource.scope_index_conflict",
                                "ResourceScope already contains the requested resource key"});
            }
            return handle;
        }
        case AssetType::Material: {
            auto lease = state->load<MaterialTag>(requestedId);
            if (!lease) {
                return core::unexpected(std::move(lease.error()));
            }
            const auto handle = lease->handle();
            if (!addEntry(Entry{.type = requestedType,
                                .requestedId = requestedId,
                                .resolution = EntryResolution::Loaded,
                                .acquisitionPolicy = ResourcePolicy::Required,
                                .failureCause = std::nullopt,
                                .lease = std::move(*lease)})) {
                return core::unexpected(
                    core::Error{"assets.resource.scope_index_conflict",
                                "ResourceScope already contains the requested resource key"});
            }
            return handle;
        }
        case AssetType::Texture: {
            auto lease = state->load<TextureTag>(requestedId);
            if (!lease) {
                return core::unexpected(std::move(lease.error()));
            }
            const auto handle = lease->handle();
            if (!addEntry(Entry{.type = requestedType,
                                .requestedId = requestedId,
                                .resolution = EntryResolution::Loaded,
                                .acquisitionPolicy = ResourcePolicy::Required,
                                .failureCause = std::nullopt,
                                .lease = std::move(*lease)})) {
                return core::unexpected(
                    core::Error{"assets.resource.scope_index_conflict",
                                "ResourceScope already contains the requested resource key"});
            }
            return handle;
        }
        case AssetType::Audio: {
            auto lease = state->load<AudioSourceTag>(requestedId);
            if (!lease) {
                return core::unexpected(std::move(lease.error()));
            }
            const auto handle = lease->handle();
            if (!addEntry(Entry{.type = requestedType,
                                .requestedId = requestedId,
                                .resolution = EntryResolution::Loaded,
                                .acquisitionPolicy = ResourcePolicy::Required,
                                .failureCause = std::nullopt,
                                .lease = std::move(*lease)})) {
                return core::unexpected(
                    core::Error{"assets.resource.scope_index_conflict",
                                "ResourceScope already contains the requested resource key"});
            }
            return handle;
        }
        case AssetType::Shader: {
            auto lease = state->load<ShaderTag>(requestedId);
            if (!lease) {
                return core::unexpected(std::move(lease.error()));
            }
            const auto handle = lease->handle();
            if (!addEntry(Entry{.type = requestedType,
                                .requestedId = requestedId,
                                .resolution = EntryResolution::Loaded,
                                .acquisitionPolicy = ResourcePolicy::Required,
                                .failureCause = std::nullopt,
                                .lease = std::move(*lease)})) {
                return core::unexpected(
                    core::Error{"assets.resource.scope_index_conflict",
                                "ResourceScope already contains the requested resource key"});
            }
            return handle;
        }
        }
        return core::unexpected(
            core::Error{"assets.resource.type_unknown", "Resource type is unknown"});
    };

    auto loaded = acquire(acquire, type, id, 1);
    if (loaded) {
        return {std::move(*loaded), std::move(diagnostics)};
    }

    rollbackTo(originalSize);
    switch (policy) {
    case ResourcePolicy::Required:
        appendErrorDiagnostic(diagnostics, loaded.error(), core::DiagnosticSeverity::Error,
                              "assets.resource.required_failed",
                              "Required resource dependency closure could not be loaded", id);
        break;
    case ResourcePolicy::Optional:
        appendErrorDiagnostic(diagnostics, loaded.error(), core::DiagnosticSeverity::Warning,
                              "assets.resource.optional_skipped",
                              "Optional resource dependency closure was skipped", id);
        break;
    case ResourcePolicy::Fallback:
        switch (type) {
        case AssetType::Mesh: {
            auto fallback = state->fallback<MeshTag>();
            if (fallback) {
                const auto handle = fallback->handle();
                if (!addEntry(Entry{.type = type,
                                    .requestedId = id,
                                    .resolution = EntryResolution::Fallback,
                                    .acquisitionPolicy = ResourcePolicy::Fallback,
                                    .failureCause = loaded.error(),
                                    .lease = std::move(*fallback)})) {
                    appendErrorDiagnostic(diagnostics, loaded.error(),
                                          core::DiagnosticSeverity::Error,
                                          "assets.resource.scope_index_conflict",
                                          "ResourceScope fallback key conflicts with an entry", id);
                    break;
                }
                appendErrorDiagnostic(
                    diagnostics, loaded.error(), core::DiagnosticSeverity::Warning,
                    "assets.resource.fallback_used", "Built-in fallback resource was used", id);
                diagnostics.sortDeterministically();
                return {handle, std::move(diagnostics)};
            }
            appendErrorDiagnostic(diagnostics, fallback.error(), core::DiagnosticSeverity::Error,
                                  "assets.resource.fallback_failed",
                                  "Built-in fallback resource could not be acquired", id);
            break;
        }
        case AssetType::Material: {
            auto fallback = state->fallback<MaterialTag>();
            if (fallback) {
                const auto handle = fallback->handle();
                if (!addEntry(Entry{.type = type,
                                    .requestedId = id,
                                    .resolution = EntryResolution::Fallback,
                                    .acquisitionPolicy = ResourcePolicy::Fallback,
                                    .failureCause = loaded.error(),
                                    .lease = std::move(*fallback)})) {
                    appendErrorDiagnostic(diagnostics, loaded.error(),
                                          core::DiagnosticSeverity::Error,
                                          "assets.resource.scope_index_conflict",
                                          "ResourceScope fallback key conflicts with an entry", id);
                    break;
                }
                appendErrorDiagnostic(
                    diagnostics, loaded.error(), core::DiagnosticSeverity::Warning,
                    "assets.resource.fallback_used", "Built-in fallback resource was used", id);
                diagnostics.sortDeterministically();
                return {handle, std::move(diagnostics)};
            }
            appendErrorDiagnostic(diagnostics, fallback.error(), core::DiagnosticSeverity::Error,
                                  "assets.resource.fallback_failed",
                                  "Built-in fallback resource could not be acquired", id);
            break;
        }
        case AssetType::Texture: {
            auto fallback = state->fallback<TextureTag>();
            if (fallback) {
                const auto handle = fallback->handle();
                if (!addEntry(Entry{.type = type,
                                    .requestedId = id,
                                    .resolution = EntryResolution::Fallback,
                                    .acquisitionPolicy = ResourcePolicy::Fallback,
                                    .failureCause = loaded.error(),
                                    .lease = std::move(*fallback)})) {
                    appendErrorDiagnostic(diagnostics, loaded.error(),
                                          core::DiagnosticSeverity::Error,
                                          "assets.resource.scope_index_conflict",
                                          "ResourceScope fallback key conflicts with an entry", id);
                    break;
                }
                appendErrorDiagnostic(
                    diagnostics, loaded.error(), core::DiagnosticSeverity::Warning,
                    "assets.resource.fallback_used", "Built-in fallback resource was used", id);
                diagnostics.sortDeterministically();
                return {handle, std::move(diagnostics)};
            }
            appendErrorDiagnostic(diagnostics, fallback.error(), core::DiagnosticSeverity::Error,
                                  "assets.resource.fallback_failed",
                                  "Built-in fallback resource could not be acquired", id);
            break;
        }
        case AssetType::Audio: {
            auto fallback = state->fallback<AudioSourceTag>();
            if (fallback) {
                const auto handle = fallback->handle();
                if (!addEntry(Entry{.type = type,
                                    .requestedId = id,
                                    .resolution = EntryResolution::Fallback,
                                    .acquisitionPolicy = ResourcePolicy::Fallback,
                                    .failureCause = loaded.error(),
                                    .lease = std::move(*fallback)})) {
                    appendErrorDiagnostic(diagnostics, loaded.error(),
                                          core::DiagnosticSeverity::Error,
                                          "assets.resource.scope_index_conflict",
                                          "ResourceScope fallback key conflicts with an entry", id);
                    break;
                }
                appendErrorDiagnostic(
                    diagnostics, loaded.error(), core::DiagnosticSeverity::Warning,
                    "assets.resource.fallback_used", "Built-in fallback resource was used", id);
                diagnostics.sortDeterministically();
                return {handle, std::move(diagnostics)};
            }
            appendErrorDiagnostic(diagnostics, fallback.error(), core::DiagnosticSeverity::Error,
                                  "assets.resource.fallback_failed",
                                  "Built-in fallback resource could not be acquired", id);
            break;
        }
        case AssetType::Shader: {
            auto fallback = state->fallback<ShaderTag>();
            if (fallback) {
                const auto handle = fallback->handle();
                if (!addEntry(Entry{.type = type,
                                    .requestedId = id,
                                    .resolution = EntryResolution::Fallback,
                                    .acquisitionPolicy = ResourcePolicy::Fallback,
                                    .failureCause = loaded.error(),
                                    .lease = std::move(*fallback)})) {
                    appendErrorDiagnostic(diagnostics, loaded.error(),
                                          core::DiagnosticSeverity::Error,
                                          "assets.resource.scope_index_conflict",
                                          "ResourceScope fallback key conflicts with an entry", id);
                    break;
                }
                appendErrorDiagnostic(
                    diagnostics, loaded.error(), core::DiagnosticSeverity::Warning,
                    "assets.resource.fallback_used", "Built-in fallback resource was used", id);
                diagnostics.sortDeterministically();
                return {handle, std::move(diagnostics)};
            }
            appendErrorDiagnostic(diagnostics, fallback.error(), core::DiagnosticSeverity::Error,
                                  "assets.resource.fallback_failed",
                                  "Built-in fallback resource could not be acquired", id);
            break;
        }
        }
        break;
    }
    diagnostics.sortDeterministically();
    return {std::nullopt, std::move(diagnostics)};
}

auto ResourceScope::requestMesh(const AssetId& id, ResourcePolicy policy)
    -> ResourceRequestResult<MeshHandle> {
    auto result = request(AssetType::Mesh, id, policy);
    std::optional<MeshHandle> handle;
    if (result.handle && std::holds_alternative<MeshHandle>(*result.handle)) {
        handle = std::get<MeshHandle>(*result.handle);
    }
    return {std::move(handle), std::move(result.diagnostics)};
}

auto ResourceScope::requestMaterial(const AssetId& id, ResourcePolicy policy)
    -> ResourceRequestResult<MaterialHandle> {
    auto result = request(AssetType::Material, id, policy);
    std::optional<MaterialHandle> handle;
    if (result.handle && std::holds_alternative<MaterialHandle>(*result.handle)) {
        handle = std::get<MaterialHandle>(*result.handle);
    }
    return {std::move(handle), std::move(result.diagnostics)};
}

auto ResourceScope::requestTexture(const AssetId& id, ResourcePolicy policy)
    -> ResourceRequestResult<TextureHandle> {
    auto result = request(AssetType::Texture, id, policy);
    std::optional<TextureHandle> handle;
    if (result.handle && std::holds_alternative<TextureHandle>(*result.handle)) {
        handle = std::get<TextureHandle>(*result.handle);
    }
    return {std::move(handle), std::move(result.diagnostics)};
}

auto ResourceScope::requestAudioSource(const AssetId& id, ResourcePolicy policy)
    -> ResourceRequestResult<AudioSourceHandle> {
    auto result = request(AssetType::Audio, id, policy);
    std::optional<AudioSourceHandle> handle;
    if (result.handle && std::holds_alternative<AudioSourceHandle>(*result.handle)) {
        handle = std::get<AudioSourceHandle>(*result.handle);
    }
    return {std::move(handle), std::move(result.diagnostics)};
}

auto ResourceScope::requestShader(const AssetId& id, ResourcePolicy policy)
    -> ResourceRequestResult<ShaderHandle> {
    auto result = request(AssetType::Shader, id, policy);
    std::optional<ShaderHandle> handle;
    if (result.handle && std::holds_alternative<ShaderHandle>(*result.handle)) {
        handle = std::get<ShaderHandle>(*result.handle);
    }
    return {std::move(handle), std::move(result.diagnostics)};
}

void ResourceScope::rollbackTo(std::size_t size) noexcept {
    if (!storage_ || size >= storage_->entries.size()) {
        return;
    }
    for (std::size_t index = size; index < storage_->entries.size(); ++index) {
        const auto& entry = storage_->entries[index];
        const auto found = storage_->index.find(EntryKeyView{entry.type, entry.requestedId.value});
        if (found != storage_->index.end() && found->second == index) {
            storage_->index.erase(found);
        }
    }
    storage_->entries.resize(size);
}

std::size_t ResourceScope::size() const noexcept {
    return storage_ ? storage_->entries.size() : 0;
}

bool ResourceScope::empty() const noexcept {
    return !storage_ || storage_->entries.empty();
}

bool ResourceScope::contains(AssetType type, const AssetId& id) const noexcept {
    return storage_ && storage_->index.contains(EntryKeyView{type, id.value});
}

bool ResourceScope::contains(MeshHandle handle) const noexcept {
    return storage_ &&
           std::any_of(storage_->entries.begin(), storage_->entries.end(), [&](const Entry& entry) {
               return std::holds_alternative<MeshLease>(entry.lease) &&
                      std::get<MeshLease>(entry.lease).handle() == handle;
           });
}

bool ResourceScope::contains(MaterialHandle handle) const noexcept {
    return storage_ &&
           std::any_of(storage_->entries.begin(), storage_->entries.end(), [&](const Entry& entry) {
               return std::holds_alternative<MaterialLease>(entry.lease) &&
                      std::get<MaterialLease>(entry.lease).handle() == handle;
           });
}

bool ResourceScope::contains(TextureHandle handle) const noexcept {
    return storage_ &&
           std::any_of(storage_->entries.begin(), storage_->entries.end(), [&](const Entry& entry) {
               return std::holds_alternative<TextureLease>(entry.lease) &&
                      std::get<TextureLease>(entry.lease).handle() == handle;
           });
}

bool ResourceScope::contains(AudioSourceHandle handle) const noexcept {
    return storage_ &&
           std::any_of(storage_->entries.begin(), storage_->entries.end(), [&](const Entry& entry) {
               return std::holds_alternative<AudioSourceLease>(entry.lease) &&
                      std::get<AudioSourceLease>(entry.lease).handle() == handle;
           });
}

bool ResourceScope::contains(ShaderHandle handle) const noexcept {
    return storage_ &&
           std::any_of(storage_->entries.begin(), storage_->entries.end(), [&](const Entry& entry) {
               return std::holds_alternative<ShaderLease>(entry.lease) &&
                      std::get<ShaderLease>(entry.lease).handle() == handle;
           });
}

std::uint64_t ResourceScope::managerToken() const noexcept {
    const auto state = state_.lock();
    return state ? state->token : 0;
}

void ResourceScope::clear() noexcept {
    if (!storage_)
        return;
    storage_->index.clear();
    storage_->entries.clear();
}
} // namespace cuexis::assets

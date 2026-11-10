#include <cuexis/runtime/runtime_session.hpp>
#include <cuexis/runtime/transaction_access.hpp>
#include <cuexis_internal/allocation_observer.hpp>
#include <cuexis_internal/presentation_metrics.hpp>

#include "runtime_session_units_internal.hpp"

#include <cuexis/animation/animation_program.hpp>
#include <cuexis/animation/animation_system.hpp>
#include <cuexis/behavior/behavior_program.hpp>
#include <cuexis/behavior/behavior_system.hpp>
#include <cuexis/chart/animation_program_input.hpp>
#include <cuexis/chart/rational_beat.hpp>
#include <cuexis/core/diagnostic.hpp>
#include <cuexis/core/error.hpp>
#include <cuexis/render/camera_component.hpp>
#include <cuexis/render/renderable_component.hpp>
#include <cuexis/world/property.hpp>
#include <cuexis/world/resolver_access.hpp>
#include <cuexis/world/transform_system.hpp>

#include <entt/entity/registry.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <exception>
#include <memory>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace cuexis::runtime {

auto RuntimeSession::acquireOverride(world::OverrideKind kind, std::string ownerId,
                                     std::int64_t priority, std::uint16_t propertyMask,
                                     world::OverrideLifetime lifetime,
                                     std::span<const PropertyOverrideWrite> writes)
    -> core::Result<world::OverrideTokenId> {
    if (!threadChecker_.isCurrent()) {
        return core::unexpected(core::Error{"runtime.session.not_owner_thread",
                                            "RuntimeSession belongs to another thread"});
    }
    if (callbackActive_) {
        return core::unexpected(core::Error{"runtime.session.callback_reentrant",
                                            "RuntimeSession World callback must not be reentrant"});
    }
    if (!world_ || !evaluation_) {
        return core::unexpected(
            core::Error{"runtime.session.empty", "RuntimeSession has no committed World"});
    }
    if (kind == world::OverrideKind::StudioPreview &&
        sessionKind_ != RuntimeSessionKind::StudioPreview) {
        return core::unexpected(
            core::Error{"runtime.override.preview_session_required",
                        "StudioPreviewOverride is only valid on a Studio preview RuntimeSession"});
    }
    if (writes.empty()) {
        return core::unexpected(core::Error{"runtime.override.empty",
                                            "Override tokens require at least one property write"});
    }

    auto mutation = internal::presentation::Mutation::reserve(presentationClock_);
    if (!mutation)
        return core::unexpected(std::move(mutation.error()));
    world::OverrideToken token;
    token.id.value = evaluation_->nextOverrideId++;
    token.kind = kind;
    token.ownerId = std::move(ownerId);
    token.priority = priority;
    token.propertyMask = propertyMask;
    token.lifetime = lifetime;
    token.writes.reserve(writes.size());
    for (const auto& write : writes) {
        const auto entity = objects_.find(write.objectId);
        if (!entity.has_value() || *entity == entt::null) {
            return core::unexpected(core::Error{"runtime.override.object_missing",
                                                "Override target object was not found"}
                                        .withContext("object_id", write.objectId.value));
        }
        if ((propertyMask & world::propertyBit(write.property)) == 0U) {
            return core::unexpected(core::Error{"world.property.override_mask",
                                                "Override write is outside the token property mask"}
                                        .withContext("object_id", write.objectId.value)
                                        .withContext("owner", token.ownerId));
        }
        token.writes.push_back(world::OverrideWrite{
            .entity = *entity, .property = write.property, .value = write.value});
    }

    auto& store = kind == world::OverrideKind::StudioPreview ? evaluation_->previewOverrides
                                                             : evaluation_->hostOverrides;
    store.push_back(std::move(token));
    publishPresentation(*mutation, internal::presentation::Source::Mutation);
    return store.back().id;
}

auto RuntimeSession::releaseOverride(world::OverrideTokenId id) -> core::Result<void> {
    if (!threadChecker_.isCurrent()) {
        return core::unexpected(core::Error{"runtime.session.not_owner_thread",
                                            "RuntimeSession belongs to another thread"});
    }
    if (callbackActive_) {
        return core::unexpected(core::Error{"runtime.session.callback_reentrant",
                                            "RuntimeSession World callback must not be reentrant"});
    }
    if (!evaluation_) {
        return core::unexpected(
            core::Error{"runtime.session.empty", "RuntimeSession has no committed World"});
    }
    auto mutation = internal::presentation::Mutation::reserve(presentationClock_);
    if (!mutation)
        return core::unexpected(std::move(mutation.error()));
    auto eraseFrom = [&](std::vector<world::OverrideToken>& tokens) {
        const auto found =
            std::find_if(tokens.begin(), tokens.end(),
                         [id](const world::OverrideToken& token) { return token.id == id; });
        if (found == tokens.end()) {
            return false;
        }
        tokens.erase(found);
        return true;
    };
    if (eraseFrom(evaluation_->hostOverrides) || eraseFrom(evaluation_->previewOverrides)) {
        publishPresentation(*mutation, internal::presentation::Source::Mutation);
        return {};
    }
    return core::unexpected(
        core::Error{"runtime.override.token_missing", "Override token was not found"}.withContext(
            "token", std::to_string(id.value)));
}

auto RuntimeSession::applyBaseProperty(const chart::ChartObjectId& objectId,
                                       world::PropertyId property, world::PropertyValue value)
    -> core::Result<void> try {
    if (!threadChecker_.isCurrent()) {
        return core::unexpected(core::Error{"runtime.session.not_owner_thread",
                                            "RuntimeSession belongs to another thread"});
    }
    if (callbackActive_) {
        return core::unexpected(core::Error{"runtime.session.callback_reentrant",
                                            "RuntimeSession World callback must not be reentrant"});
    }
    if (!world_ || !evaluation_) {
        return core::unexpected(
            core::Error{"runtime.session.empty", "RuntimeSession has no committed World"});
    }
    auto mutation = internal::presentation::Mutation::reserve(presentationClock_);
    if (!mutation)
        return core::unexpected(std::move(mutation.error()));
    auto candidate = std::make_unique<RuntimeEvaluationState>(*evaluation_);
    const auto entity = objects_.find(objectId);
    if (!entity.has_value() || *entity == entt::null) {
        return core::unexpected(
            core::Error{"runtime.property.object_missing", "Base property target was not found"}
                .withContext("object_id", objectId.value));
    }
    auto applied = candidate->resolver.applyBaseProperty(*entity, property, value);
    if (!applied) {
        return applied;
    }
    for (auto& binding : candidate->program.bindings) {
        if (binding.entity != *entity) {
            continue;
        }
        for (auto& baseline : binding.baselines) {
            if (baseline.property == property) {
                baseline.value = value;
            }
        }
    }
    if (property == world::PropertyId::CameraFovY) {
        if (const auto* fov = std::get_if<double>(&value)) {
            for (auto& camera : candidate->cameras) {
                if (camera.entity == *entity) {
                    camera.baselineFovY = *fov;
                }
            }
        }
    }
    for (auto& appearance : candidate->appearances) {
        if (appearance.entity != *entity) {
            continue;
        }
        switch (property) {
        case world::PropertyId::RenderVisible:
            if (const auto* visible = std::get_if<bool>(&value)) {
                appearance.baseline.visible = *visible;
            }
            break;
        case world::PropertyId::RenderMaterial:
            if (const auto* material = std::get_if<std::string>(&value)) {
                appearance.baseline.materialAssetId = *material;
            }
            break;
        case world::PropertyId::MaterialOpacity:
            if (const auto* opacity = std::get_if<double>(&value)) {
                appearance.baseline.opacity = *opacity;
            }
            break;
        case world::PropertyId::MaterialTint:
            if (const auto* tint = std::get_if<core::Vec3>(&value)) {
                appearance.baseline.tint = *tint;
            }
            break;
        default:
            break;
        }
    }
    std::vector<RuntimeDebugRecord> records;
    bool truncated{};
    if (lastFrame_) {
        auto updated = updatePrepared(*world_, *candidate, chartRuntime_->timingMap, *lastFrame_,
                                      true, &records, &truncated);
        if (!updated)
            return updated;
    }
    evaluation_.swap(candidate);
    if (lastFrame_) {
        debugRecords_.swap(records);
        debugTruncated_ = truncated;
    }
    publishPresentation(*mutation, internal::presentation::Source::Mutation);
    return {};
} catch (...) {
    if (transformUpdate_)
        transformUpdate_->cancel();
    return core::unexpected(
        core::Error{"runtime.session.update_exception", "Base property preparation failed"});
}

auto RuntimeSession::applyBaseProperty(const BasePropertyCommand& command) -> core::Result<void> {
    return applyBaseProperty(command.objectId, command.property, command.value);
}

auto RuntimeSession::baseRevision() const noexcept -> std::uint64_t {
    threadChecker_.assertCurrent();
    return evaluation_ ? evaluation_->resolver.baseRevision() : 0;
}

auto RuntimeSession::reload(chart::ChartRuntime replacement) -> RuntimeSessionReloadResult {
    const RuntimeFrame target = lastFrame_.value_or(RuntimeFrame{});
    return reload(std::move(replacement), target, ReloadPolicy::KeepChartTime);
}

auto RuntimeSession::reload(chart::ChartRuntime replacement, const RuntimeFrame& targetFrame,
                            ReloadPolicy policy) -> RuntimeSessionReloadResult try {
    RuntimeSessionReloadResult result;
    if (!threadChecker_.isCurrent()) {
        addSessionError(result.diagnostics, "runtime.session.not_owner_thread",
                        "RuntimeSession reload must run on its owner thread");
        return result;
    }
    if (!world_ || !evaluation_) {
        addSessionError(result.diagnostics, "runtime.session.empty",
                        "Cannot reload an empty RuntimeSession");
        return result;
    }

    RuntimeFrame reloadFrame = targetFrame;
    reloadFrame.simulationDeltaTimeMs = 0.0;
    if (policy == ReloadPolicy::RestartAtZero) {
        reloadFrame.chartTimeMs = 0.0;
    }
    auto frameValidation = validateFrame(reloadFrame, std::nullopt);
    if (!frameValidation) {
        addSessionError(result.diagnostics, frameValidation.error());
        return result;
    }

    if (callbackActive_) {
        addSessionError(result.diagnostics, "runtime.session.callback_reentrant",
                        "Runtime callback reentry");
        return result;
    }
    auto mutation = internal::presentation::Mutation::reserve(presentationClock_, true);
    if (!mutation) {
        addSessionError(result.diagnostics, mutation.error());
        return result;
    }
    auto preparedResult =
        evaluation_->animation.has_value()
            ? prepare(std::move(replacement), animation::AnimationProgram{*evaluation_->animation})
            : prepare(std::move(replacement));
    result.diagnostics = std::move(preparedResult.diagnostics);
    if (!preparedResult.hasValue()) {
        result.diagnostics.sortDeterministically();
        return result;
    }

    // Evaluate the candidate directly; the active World is never exchanged during prepare.
    auto sampled =
        updatePrepared(*preparedResult.prepared->world_, *preparedResult.prepared->evaluation_,
                       preparedResult.prepared->chartRuntime_.timingMap, reloadFrame);
    if (!sampled) {
        addSessionError(result.diagnostics, sampled.error());
        result.diagnostics.sortDeterministically();
        return result;
    }

    std::vector<RuntimeDebugRecord> candidateDebugRecords;
    bool candidateDebugTruncated = false;
    if (debugOptions_.enabled) {
        const auto beatSample = preparedResult.prepared->chartRuntime_.timingMap.sampleChartTimeMs(
            reloadFrame.chartTimeMs);
        if (!beatSample) {
            addSessionError(result.diagnostics, beatSample.error());
            result.diagnostics.sortDeterministically();
            return result;
        }
        auto captured =
            captureDebug(*preparedResult.prepared->evaluation_, preparedResult.prepared->objects_,
                         beatSample->beat, candidateDebugRecords, candidateDebugTruncated);
        if (!captured) {
            addSessionError(result.diagnostics, captured.error());
            result.diagnostics.sortDeterministically();
            return result;
        }
    }
    replaceWith(std::move(*preparedResult.prepared));
    lastFrame_ = reloadFrame;
    if (debugOptions_.enabled) {
        debugRecords_ = std::move(candidateDebugRecords);
        debugTruncated_ = candidateDebugTruncated;
    } else {
        debugRecords_.clear();
        debugTruncated_ = false;
    }
    publishPresentation(*mutation, internal::presentation::Source::Restore, reloadFrame);
    result.reloaded = true;
    return result;
} catch (...) {
    if (transformUpdate_)
        transformUpdate_->cancel();
    RuntimeSessionReloadResult result;
    addSessionError(result.diagnostics, "runtime.session.update_exception",
                    "Reload preparation failed");
    return result;
}

auto RuntimeSession::unload() -> core::Result<void> {
    if (!threadChecker_.isCurrent()) {
        return core::unexpected(core::Error{"runtime.session.not_owner_thread",
                                            "RuntimeSession unload must run on its owner thread"});
    }
    if (callbackActive_)
        return core::unexpected(
            core::Error{"runtime.session.callback_reentrant", "Runtime callback reentry"});
    if (!world_)
        return {};
    auto mutation = internal::presentation::Mutation::reserve(presentationClock_);
    if (!mutation)
        return core::unexpected(std::move(mutation.error()));
    evaluation_.reset();
    world_.reset();
    objects_.clear();
    chartRuntime_.reset();
    activeDiagnostics_.clear();
    resourceScope_.reset();
    lastFrame_.reset();
    debugRecords_.clear();
    debugTruncated_ = false;
    presentationCamera_.reset();
    publishPresentation(*mutation, internal::presentation::Source::Mutation);
    return {};
}

auto RuntimeSession::empty() const noexcept -> bool {
    threadChecker_.assertCurrent();
    return !world_;
}

auto RuntimeSession::objectCount() const noexcept -> std::size_t {
    threadChecker_.assertCurrent();
    return objects_.size();
}

auto RuntimeSession::resourceCount() const noexcept -> std::size_t {
    threadChecker_.assertCurrent();
    return resourceScope_.has_value() ? resourceScope_->size() : 0;
}

auto RuntimeSession::activeDiagnostics() const noexcept -> const core::Diagnostics& {
    threadChecker_.assertCurrent();
    return activeDiagnostics_;
}

auto RuntimeSession::findEntity(const chart::ChartObjectId& objectId) const
    -> core::Result<std::optional<entt::entity>> {
    if (!threadChecker_.isCurrent()) {
        return core::unexpected(core::Error{"runtime.session.not_owner_thread",
                                            "RuntimeSession belongs to another thread"});
    }
    if (!world_) {
        return core::unexpected(
            core::Error{"runtime.session.empty", "RuntimeSession has no committed World"});
    }
    return objects_.find(objectId);
}

void RuntimeSession::replaceWith(PreparedRuntimeSession&& prepared) noexcept {
    auto previousRuntime = std::move(chartRuntime_);
    auto previousObjects = std::move(objects_);
    auto previousDiagnostics = std::move(activeDiagnostics_);
    auto previousEvaluation = std::move(evaluation_);
    auto previousScope = std::move(resourceScope_);
    auto previousWorld = std::move(world_);

    chartRuntime_.emplace(std::move(prepared.chartRuntime_));
    objects_ = std::move(prepared.objects_);
    activeDiagnostics_ = std::move(prepared.diagnostics_);
    evaluation_ = std::move(prepared.evaluation_);
    resourceScope_ = std::move(prepared.resourceScope_);
    world_ = std::move(prepared.world_);
    presentationCamera_ = prepared.presentationCamera_;

    previousEvaluation.reset();
    previousWorld.reset();
    previousScope.reset();
    previousObjects.clear();
    previousDiagnostics.clear();
    previousRuntime.reset();
}

auto RuntimeSession::updateGameplay(const RuntimeFrame& frame,
                                    std::span<const PropertyOverrideWrite> writes,
                                    bool tickLifetimes) -> core::Result<void> {
    internal::pd::Timer measurement{internal::pd::Stage::runtime};
    measurement.items(writes.size());
    if (!threadChecker_.isCurrent())
        return core::unexpected(
            core::Error{"runtime.session.not_owner_thread", "Wrong owner thread"});
    if (!evaluation_ || !world_)
        return core::unexpected(core::Error{"runtime.session.empty", "Runtime inactive"});
    if (callbackActive_)
        return core::unexpected(
            core::Error{"runtime.session.callback_reentrant", "Runtime callback reentry"});
    auto valid = validateFrame(frame, lastFrame_);
    if (!valid)
        return valid;
    auto mutation =
        internal::presentation::Mutation::reserve(presentationClock_, false, tickLifetimes);
    if (!mutation)
        return core::unexpected(std::move(mutation.error()));
    std::unique_ptr<RuntimeEvaluationState> candidate;
    try {
        {
            internal::pd::Timer copy{internal::pd::Stage::runtimeCopy};
            copy.items(1);
            copy.logicalCopy(sizeof(RuntimeEvaluationState));
            candidate = std::make_unique<RuntimeEvaluationState>(*evaluation_);
        }
        candidate->resolver.beginFrame();
        candidate->gameplayWrites.clear();
        candidate->gameplayWrites.reserve(writes.size());
        for (const auto& write : writes) {
            if (write.property != world::PropertyId::RenderVisible ||
                !std::holds_alternative<bool>(write.value))
                return core::unexpected(core::Error{"world.property.invalid_value",
                                                    "Gameplay requires typed render.visible"});
            auto entity = findEntity(write.objectId);
            if (!entity)
                return core::unexpected(std::move(entity.error()));
            if (!*entity)
                return core::unexpected(
                    core::Error{"presentation-target-missing", "Gameplay target missing"});
            candidate->gameplayWrites.push_back(
                {**entity, write.property, std::get<bool>(write.value)});
        }
        std::vector<RuntimeDebugRecord> records;
        bool truncated{};
        auto updated = updatePrepared(*world_, *candidate, chartRuntime_->timingMap, frame,
                                      tickLifetimes, &records, &truncated);
        if (!updated) {
            return updated;
        }
        evaluation_.swap(candidate);
        internal::pd::memory::structure("current.runtime", 1, 1);
        debugRecords_.swap(records);
        debugTruncated_ = truncated;
        lastFrame_ = frame;
        publishPresentation(*mutation,
                            tickLifetimes ? internal::presentation::Source::RuntimeUpdate
                                          : internal::presentation::Source::Restore,
                            frame);
        return {};
    } catch (...) {
        if (transformUpdate_)
            transformUpdate_->cancel();
        return core::unexpected(core::Error{"runtime.session.update_exception",
                                            "Gameplay projection allocation failed"});
    }
}

void RuntimeSession::publishPresentation(internal::presentation::Mutation& mutation,
                                         internal::presentation::Source source,
                                         std::optional<RuntimeFrame> frame) noexcept {
    mutation.publish();
    presentationSource_ = source;
    presentationFrame_ = frame;
}

} // namespace cuexis::runtime

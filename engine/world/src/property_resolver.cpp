#include <cuexis/world/property.hpp>
#include <cuexis_internal/allocation_observer.hpp>

#include <cuexis/core/error.hpp>
#include <cuexis/world/resolver_access.hpp>
#include <cuexis/world/world.hpp>

#include <entt/entity/registry.hpp>

#include <algorithm>
#include <cmath>
#include <exception>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "property_helpers.hpp"

namespace cuexis::world {

using namespace property_detail;

auto PropertyResolver::findEntry(entt::entity entity) noexcept -> Entry* {
    const auto entry = std::lower_bound(
        entries_.begin(), entries_.end(), entity, [](const Entry& candidate, entt::entity target) {
            return entityValue(candidate.entity) < entityValue(target);
        });
    if (entry == entries_.end() || entry->entity != entity) {
        return nullptr;
    }
    return &*entry;
}

auto PropertyResolver::findEntry(entt::entity entity) const noexcept -> const Entry* {
    const auto entry = std::lower_bound(
        entries_.begin(), entries_.end(), entity, [](const Entry& candidate, entt::entity target) {
            return entityValue(candidate.entity) < entityValue(target);
        });
    if (entry == entries_.end() || entry->entity != entity) {
        return nullptr;
    }
    return &*entry;
}

auto PropertyResolver::ensureEntry(entt::entity entity) -> Entry& {
    auto* existing = findEntry(entity);
    if (existing != nullptr) {
        return *existing;
    }
    Entry entry;
    entry.entity = entity;
    const auto insert = std::lower_bound(
        entries_.begin(), entries_.end(), entity, [](const Entry& candidate, entt::entity target) {
            return entityValue(candidate.entity) < entityValue(target);
        });
    const auto insertedIndex = static_cast<std::size_t>(insert - entries_.begin());
    auto& inserted = *entries_.insert(insert, std::move(entry));
    // Membership follows each Entry; all saved indices must follow sorted insertion as well.
    for (auto* indices : {&touchedEntries_, &committedEntries_, &thisCommit_}) {
        for (auto& index : *indices) {
            if (index >= insertedIndex)
                ++index;
        }
    }
    touchedEntries_.reserve(entries_.size());
    committedEntries_.reserve(entries_.size());
    thisCommit_.reserve(entries_.size());
    return inserted;
}

auto PropertyResolver::capture(const World& world) -> core::Result<PropertyResolver> {
    return world.withRegistry([](const entt::registry& registry) -> core::Result<PropertyResolver> {
        PropertyResolver resolver;
        const auto view = registry.view<const TransformComponent>();
        for (const entt::entity entity : view) {
            const auto& transform = view.get<const TransformComponent>(entity);
            Entry entry;
            entry.entity = entity;
            entry.hasTransform = true;
            entry.transformBaseline = transform;
            entry.transformCandidate = transform;
            entry.transformPrevious = transform;
            for (const auto property :
                 {PropertyId::TransformPositionX, PropertyId::TransformPositionY,
                  PropertyId::TransformPositionZ, PropertyId::TransformRotation,
                  PropertyId::TransformScale}) {
                auto& state = entry.properties[propertyIndex(property)];
                state.present = true;
                state.baseline = transformFromBaseline(transform, property);
                state.candidate = state.baseline;
                state.source = PropertyLayer::Initial;
            }
            resolver.entries_.push_back(std::move(entry));
        }
        std::sort(resolver.entries_.begin(), resolver.entries_.end(),
                  [](const Entry& left, const Entry& right) {
                      return entityValue(left.entity) < entityValue(right.entity);
                  });
        resolver.touchedEntries_.reserve(resolver.entries_.size());
        resolver.committedEntries_.reserve(resolver.entries_.size());
        resolver.thisCommit_.reserve(resolver.entries_.size());
        return resolver;
    });
}

auto PropertyResolver::registerBaseline(entt::entity entity, PropertyId property,
                                        PropertyValue value) -> core::Result<void> {
    if (entity == entt::null) {
        return core::unexpected(
            core::Error{"world.property.null_entity", "Property writes require a valid entity"});
    }
    auto& entry = ensureEntry(entity);
    auto& state = entry.properties[propertyIndex(property)];
    state.present = true;
    state.baseline = std::move(value);
    state.candidate = state.baseline;
    state.source = PropertyLayer::Initial;
    return {};
}

auto PropertyResolver::applyBaseProperty(entt::entity entity, PropertyId property,
                                         PropertyValue value) -> core::Result<void> {
    if (entity == entt::null) {
        return core::unexpected(
            core::Error{"world.property.null_entity", "Property writes require a valid entity"});
    }
    auto* entry = findEntry(entity);
    if (entry == nullptr) {
        return core::unexpected(entityError("world.property.baseline_missing",
                                            "Base property target has no resolver entry", entity));
    }
    auto& state = entry->properties[propertyIndex(property)];
    if (!state.present) {
        return core::unexpected(entityError("world.property.baseline_missing",
                                            "Base property target has no captured baseline",
                                            entity));
    }
    auto parsed = parsedPropertyValue(entity, entry->hasTransform, state.present, property,
                                      writeValue(value));
    if (!parsed) {
        return core::unexpected(std::move(parsed.error()));
    }
    state.baseline = std::move(*parsed);
    state.candidate = state.baseline;
    state.source = PropertyLayer::Initial;
    if (isTransformProperty(property) && entry->hasTransform) {
        assignTransform(entry->transformBaseline, property, state.baseline);
        entry->transformCandidate = entry->transformBaseline;
    }
    const auto entryIndex = static_cast<std::size_t>(entry - entries_.data());
    markTouched(entryIndex);
    if (std::find(committedEntries_.begin(), committedEntries_.end(), entryIndex) ==
        committedEntries_.end()) {
        committedEntries_.push_back(entryIndex);
    }
    ++baseRevision_;
    return {};
}

void PropertyResolver::reserveStringCapacity(entt::entity entity, PropertyId property,
                                             std::size_t bytes) {
    auto* entry = findEntry(entity);
    if (entry == nullptr) {
        return;
    }
    auto ensureReserved = [bytes](PropertyValue& value) {
        if (auto* text = std::get_if<std::string>(&value)) {
            if (text->capacity() < bytes) {
                text->reserve(bytes);
            }
            return;
        }
        std::string text;
        text.reserve(bytes);
        value = std::move(text);
    };
    auto& state = entry->properties[propertyIndex(property)];
    ensureReserved(state.baseline);
    ensureReserved(state.candidate);
    ensureReserved(state.behavior);
    ensureReserved(state.animation);
    ensureReserved(state.host);
    ensureReserved(state.preview);
}

void PropertyResolver::resetEntry(Entry& entry) noexcept {
    entry.touched = false;
    entry.transformCandidate = entry.transformBaseline;
    entry.transformSeenMask = 0;
    for (auto& state : entry.properties) {
        if (!state.present) {
            continue;
        }
        copyStoredValue(state.candidate, state.baseline);
        resetStoredValue(state.behavior);
        resetStoredValue(state.animation);
        resetStoredValue(state.gameplay);
        resetStoredValue(state.host);
        resetStoredValue(state.preview);
        state.source = PropertyLayer::Initial;
        state.seenLayers = 0;
        state.conflict = false;
    }
}

void PropertyResolver::beginFrame() {
    prepared_ = false;
    committed_ = false;
    conflicts_.clear();
    for (const auto index : committedEntries_) {
        resetEntry(entries_[index]);
    }
    for (const auto index : touchedEntries_) {
        resetEntry(entries_[index]);
    }
    touchedEntries_.clear();
}

auto PropertyResolver::validateAndStore(Entry& entry, PropertyId property,
                                        const PropertyWriteValue& value, PropertyLayer layer)
    -> core::Result<void> {
    auto& state = entry.properties[propertyIndex(property)];
    auto stored = storeParsedPropertyValue(state.candidate, entry.entity, entry.hasTransform,
                                           state.present, property, value);
    if (!stored) {
        return core::unexpected(std::move(stored.error()));
    }
    if (isTransformProperty(property) && entry.hasTransform) {
        assignTransform(entry.transformCandidate, property, state.candidate);
    }
    state.present = true;
    state.source = layer;
    switch (layer) {
    case PropertyLayer::Behavior:
        copyStoredValue(state.behavior, state.candidate);
        break;
    case PropertyLayer::Animation:
        copyStoredValue(state.animation, state.candidate);
        break;
    case PropertyLayer::GameplayOverride:
        copyStoredValue(state.gameplay, state.candidate);
        break;
    case PropertyLayer::HostOverride:
        copyStoredValue(state.host, state.candidate);
        break;
    case PropertyLayer::StudioPreviewOverride:
        copyStoredValue(state.preview, state.candidate);
        break;
    case PropertyLayer::Initial:
        break;
    }
    return {};
}

auto PropertyResolver::applyWrite(const PropertyWrite& write, PropertyLayer layer,
                                  bool duplicateIsError) -> core::Result<void> {
    if (write.entity == entt::null) {
        return core::unexpected(
            core::Error{"world.property.null_entity", "Property writes require a valid entity"});
    }
    auto* entry = findEntry(write.entity);
    if (entry == nullptr) {
        if (isTransformProperty(write.property)) {
            return core::unexpected(entityError("world.property.transform_missing",
                                                "Transform property target has no baseline",
                                                write.entity));
        }
        if (isCameraProperty(write.property)) {
            return core::unexpected(core::Error{"runtime.camera.binding_missing",
                                                "camera.fovY target has no CameraComponent"});
        }
        if (isAppearanceProperty(write.property)) {
            return core::unexpected(core::Error{"runtime.appearance.binding_missing",
                                                "Appearance target has no Renderable component"});
        }
        return core::unexpected(entityError("world.property.baseline_missing",
                                            "Property target has no baseline", write.entity));
    }

    auto& state = entry->properties[propertyIndex(write.property)];
    const auto mask = layerBit(layer);
    if ((state.seenLayers & mask) != 0U) {
        if (duplicateIsError) {
            if (isCameraProperty(write.property)) {
                return core::unexpected(core::Error{"runtime.camera.write_conflict",
                                                    "camera.fovY was written more than once"});
            }
            return core::unexpected(entityError("world.property.write_conflict",
                                                "A property was written more than once",
                                                write.entity));
        }
        return {};
    }

    const auto entryIndex = static_cast<std::size_t>(entry - entries_.data());
    markTouched(entryIndex);
    state.seenLayers = static_cast<std::uint8_t>(state.seenLayers | mask);
    if (isTransformProperty(write.property)) {
        entry->transformSeenMask =
            static_cast<std::uint8_t>(entry->transformSeenMask | transformBit(write.property));
    }
    return validateAndStore(*entry, write.property, write.value, layer);
}

void PropertyResolver::markTouched(std::size_t entryIndex) {
    auto& entry = entries_[entryIndex];
    if (!entry.touched) {
        touchedEntries_.push_back(entryIndex);
        entry.touched = true;
    }
}

void PropertyResolver::collectCommitEntries() {
    thisCommit_.clear();
    thisCommit_.reserve(entries_.size());
    thisCommit_.insert(thisCommit_.end(), committedEntries_.begin(), committedEntries_.end());
    thisCommit_.insert(thisCommit_.end(), touchedEntries_.begin(), touchedEntries_.end());
    std::sort(thisCommit_.begin(), thisCommit_.end());
    thisCommit_.erase(std::unique(thisCommit_.begin(), thisCommit_.end()), thisCommit_.end());
}

auto PropertyResolver::applyLayer(std::span<const PropertyWrite> writes, PropertyLayer layer,
                                  bool duplicateIsError) -> core::Result<void> {
    for (const auto& write : writes) {
        auto applied = applyWrite(write, layer, duplicateIsError);
        if (!applied) {
            return applied;
        }
    }
    return {};
}

auto PropertyResolver::applyOverrides(std::span<const OverrideToken> tokens, PropertyLayer layer)
    -> core::Result<void> {
    struct Candidate final {
        const OverrideToken* token{};
        const OverrideWrite* write{};
    };

    struct Key final {
        entt::entity entity{entt::null};
        PropertyId property{};

        auto operator<=>(const Key&) const = default;
    };

    std::vector<Candidate> candidates;
    candidates.reserve(tokens.size());
    for (const auto& token : tokens) {
        for (const auto& write : token.writes) {
            if ((token.propertyMask & propertyBit(write.property)) == 0U) {
                return core::unexpected(
                    entityError("world.property.override_mask",
                                "Override write is outside the token property mask", write.entity)
                        .withContext("owner", token.ownerId));
            }
            candidates.push_back(Candidate{.token = &token, .write = &write});
        }
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& left, const Candidate& right) {
                  if (entityValue(left.write->entity) != entityValue(right.write->entity)) {
                      return entityValue(left.write->entity) < entityValue(right.write->entity);
                  }
                  if (left.write->property != right.write->property) {
                      return left.write->property < right.write->property;
                  }
                  if (left.token->priority != right.token->priority) {
                      return left.token->priority > right.token->priority;
                  }
                  return left.token->id.value < right.token->id.value;
              });

    std::size_t index = 0;
    while (index < candidates.size()) {
        const auto key = Key{candidates[index].write->entity, candidates[index].write->property};
        std::size_t end = index + 1;
        while (end < candidates.size() && candidates[end].write->entity == key.entity &&
               candidates[end].write->property == key.property) {
            ++end;
        }
        const auto maxPriority = candidates[index].token->priority;
        std::size_t winners = 0;
        for (std::size_t cursor = index; cursor < end; ++cursor) {
            if (candidates[cursor].token->priority == maxPriority) {
                ++winners;
            }
        }
        if (winners > 1) {
            auto* entry = findEntry(key.entity);
            if (entry != nullptr) {
                entry->properties[propertyIndex(key.property)].conflict = true;
            }
            conflicts_.push_back(PropertyConflict{
                .entity = key.entity,
                .property = key.property,
                .layer = layer,
                .priority = maxPriority,
            });
            index = end;
            continue;
        }

        const auto& winner = *candidates[index].write;
        auto applied = applyWrite(PropertyWrite{.entity = winner.entity,
                                                .property = winner.property,
                                                .value = writeValue(winner.value)},
                                  layer, true);
        if (!applied) {
            return applied;
        }
        index = end;
    }
    return {};
}

auto PropertyResolver::prepare(std::span<const PropertyWrite> writes) -> core::Result<void> {
    beginFrame();
    auto applied = applyLayer(writes, PropertyLayer::Behavior, true);
    if (!applied) {
        return applied;
    }
    return finalize();
}

auto PropertyResolver::finalize() -> core::Result<void> {
    for (const auto index : touchedEntries_) {
        const auto& entry = entries_[index];
        if (!entry.hasTransform || entry.transformSeenMask == 0U) {
            continue;
        }
        const auto matrix = core::composeTransform(entry.transformCandidate.position,
                                                   entry.transformCandidate.rotation,
                                                   entry.transformCandidate.scale);
        if (!matrix || !core::isFinite(*matrix)) {
            return core::unexpected(entityError(
                "world.property.transform_invalid",
                "Resolved Transform did not produce a finite local matrix", entry.entity));
        }
    }
    prepared_ = true;
    return {};
}

auto detail::ResolverAccess::resolvedAppearance(const PropertyResolver& resolver,
                                                entt::entity entity) noexcept
    -> std::array<const PropertyValue*, 4> {
    const auto* entry = resolver.findEntry(entity);
    if (!entry)
        return {};
    const auto value = [entry](PropertyId property) -> const PropertyValue* {
        const auto& state = entry->properties[propertyIndex(property)];
        return state.present ? &state.candidate : nullptr;
    };
    return {value(PropertyId::RenderVisible), value(PropertyId::RenderMaterial),
            value(PropertyId::MaterialOpacity), value(PropertyId::MaterialTint)};
}

auto detail::ResolverAccess::prepareTransforms(PropertyResolver& resolver,
                                               std::vector<TransformWrite>& writes)
    -> core::Result<void> {
    if (!resolver.prepared_)
        return core::unexpected(core::Error{"world.property.not_prepared",
                                            "Property resolver commit requires prepare"});
    resolver.collectCommitEntries();
    using internal::pd::memory::structure;
    structure("index.resolver", resolver.entries_.size(), resolver.entries_.capacity());
    structure("scratch.resolver-touched", resolver.touchedEntries_.size(),
              resolver.touchedEntries_.capacity());
    structure("current.resolver-committed", resolver.committedEntries_.size(),
              resolver.committedEntries_.capacity());
    structure("candidate.resolver-commit", resolver.thisCommit_.size(),
              resolver.thisCommit_.capacity());
    structure("scratch.resolver-conflicts", resolver.conflicts_.size(),
              resolver.conflicts_.capacity());
    writes.clear();
    writes.reserve(resolver.entries_.size());
    for (auto index : resolver.thisCommit_) {
        const auto& entry = resolver.entries_[index];
        if (entry.hasTransform)
            writes.push_back({entry.entity, entry.transformCandidate});
    }
    return {};
}

void detail::ResolverAccess::installed(PropertyResolver& resolver) noexcept {
    resolver.committedEntries_.swap(resolver.thisCommit_);
    resolver.committed_ = false;
}

auto PropertyResolver::commit(World& world) -> core::Result<void> {
    if (!prepared_) {
        return core::unexpected(core::Error{"world.property.not_prepared",
                                            "Property resolver commit requires prepare"});
    }
    collectCommitEntries();
    committedEntries_.reserve(thisCommit_.size());
    auto result = world.withRegistry([&](entt::registry& registry) -> core::Result<void> {
        for (const auto index : thisCommit_) {
            const auto& entry = entries_[index];
            if (!entry.hasTransform) {
                continue;
            }
            if (!registry.valid(entry.entity) ||
                !registry.all_of<TransformComponent>(entry.entity)) {
                return core::unexpected(entityError(
                    "world.property.transform_missing",
                    "A captured Transform baseline is no longer available", entry.entity));
            }
        }
        for (const auto index : thisCommit_) {
            auto& entry = entries_[index];
            if (!entry.hasTransform) {
                continue;
            }
            entry.transformPrevious = registry.get<TransformComponent>(entry.entity);
            registry.replace<TransformComponent>(entry.entity, entry.transformCandidate);
        }
        return {};
    });
    if (result) {
        committedEntries_ = thisCommit_;
        committed_ = true;
    }
    return result;
}

void PropertyResolver::rollback(World& world) noexcept {
    if (!committed_) {
        return;
    }
    const auto rolledBack = world.withRegistry([&](entt::registry& registry) {
        for (const auto index : thisCommit_) {
            const auto& entry = entries_[index];
            if (!entry.hasTransform) {
                continue;
            }
            if (registry.valid(entry.entity) && registry.all_of<TransformComponent>(entry.entity)) {
                registry.replace<TransformComponent>(entry.entity, entry.transformPrevious);
            }
        }
    });
    if (!rolledBack) {
        std::terminate();
    }
    committed_ = false;
}

auto PropertyResolver::resolvedValuePtr(entt::entity entity, PropertyId property) const noexcept
    -> const PropertyValue* {
    const auto* entry = findEntry(entity);
    if (entry == nullptr) {
        return nullptr;
    }
    const auto& state = entry->properties[propertyIndex(property)];
    if (!state.present) {
        return nullptr;
    }
    return &state.candidate;
}

auto PropertyResolver::resolvedValue(entt::entity entity, PropertyId property) const noexcept
    -> std::optional<PropertyValue> {
    const auto* entry = findEntry(entity);
    if (entry == nullptr) {
        return std::nullopt;
    }
    if (entry->hasTransform) {
        switch (property) {
        case PropertyId::TransformPositionX:
            return PropertyValue{static_cast<double>(entry->transformCandidate.position.x)};
        case PropertyId::TransformPositionY:
            return PropertyValue{static_cast<double>(entry->transformCandidate.position.y)};
        case PropertyId::TransformPositionZ:
            return PropertyValue{static_cast<double>(entry->transformCandidate.position.z)};
        case PropertyId::TransformRotation:
            return PropertyValue{entry->transformCandidate.rotation};
        case PropertyId::TransformScale:
            return PropertyValue{entry->transformCandidate.scale};
        default:
            break;
        }
    }
    const auto* value = resolvedValuePtr(entity, property);
    if (value == nullptr) {
        return std::nullopt;
    }
    return *value;
}

auto PropertyResolver::baselineValue(entt::entity entity, PropertyId property) const noexcept
    -> std::optional<PropertyValue> {
    const auto* entry = findEntry(entity);
    if (entry == nullptr) {
        return std::nullopt;
    }
    const auto& state = entry->properties[propertyIndex(property)];
    if (!state.present) {
        return std::nullopt;
    }
    return state.baseline;
}

auto PropertyResolver::layerValue(entt::entity entity, PropertyId property,
                                  PropertyLayer layer) const noexcept
    -> std::optional<PropertyValue> {
    const auto* entry = findEntry(entity);
    if (entry == nullptr) {
        return std::nullopt;
    }
    const auto& state = entry->properties[propertyIndex(property)];
    if (!state.present) {
        return std::nullopt;
    }
    switch (layer) {
    case PropertyLayer::Initial:
        return state.baseline;
    case PropertyLayer::Behavior:
        return (state.seenLayers & layerBit(PropertyLayer::Behavior)) != 0U
                   ? std::optional<PropertyValue>{state.behavior}
                   : std::nullopt;
    case PropertyLayer::Animation:
        return (state.seenLayers & layerBit(PropertyLayer::Animation)) != 0U
                   ? std::optional<PropertyValue>{state.animation}
                   : std::nullopt;
    case PropertyLayer::HostOverride:
        return (state.seenLayers & layerBit(PropertyLayer::HostOverride)) != 0U
                   ? std::optional<PropertyValue>{state.host}
                   : std::nullopt;
    case PropertyLayer::GameplayOverride:
        return (state.seenLayers & layerBit(PropertyLayer::GameplayOverride)) != 0U
                   ? std::optional<PropertyValue>{state.gameplay}
                   : std::nullopt;
    case PropertyLayer::StudioPreviewOverride:
        return (state.seenLayers & layerBit(PropertyLayer::StudioPreviewOverride)) != 0U
                   ? std::optional<PropertyValue>{state.preview}
                   : std::nullopt;
    }
    return std::nullopt;
}

auto PropertyResolver::sourceLayer(entt::entity entity, PropertyId property) const noexcept
    -> std::optional<PropertyLayer> {
    const auto* entry = findEntry(entity);
    if (entry == nullptr) {
        return std::nullopt;
    }
    const auto& state = entry->properties[propertyIndex(property)];
    if (!state.present) {
        return std::nullopt;
    }
    return state.source;
}

auto PropertyResolver::hadConflict(entt::entity entity, PropertyId property) const noexcept
    -> bool {
    const auto* entry = findEntry(entity);
    if (entry == nullptr) {
        return false;
    }
    return entry->properties[propertyIndex(property)].conflict;
}

auto PropertyResolver::conflicts() const noexcept -> std::span<const PropertyConflict> {
    return conflicts_;
}

auto PropertyResolver::baselineCount() const noexcept -> std::size_t {
    return entries_.size();
}

auto PropertyResolver::baseRevision() const noexcept -> std::uint64_t {
    return baseRevision_;
}

} // namespace cuexis::world

// TransformSystem uses a private prepare/apply plan. Hierarchy evaluation remains shared.
#include <algorithm>
#include <cuexis/core/error.hpp>
#include <cuexis/world/transform_system.hpp>
#include <cuexis/world/transform_update.hpp>
#include <cuexis_internal/allocation_observer.hpp>
#include <exception>
#include <unordered_map>
namespace cuexis::world {
namespace {

using EntityValue = std::underlying_type_t<entt::entity>;

[[nodiscard]] auto entityValue(entt::entity entity) noexcept -> EntityValue {
    return entt::to_integral(entity);
}

[[nodiscard]] auto entityText(entt::entity entity) -> std::string {
    return std::to_string(static_cast<std::uint64_t>(entityValue(entity)));
}

[[nodiscard]] auto sameTransform(const TransformComponent& left,
                                 const TransformComponent& right) noexcept -> bool {
    return left.position == right.position && left.rotation == right.rotation &&
           left.scale == right.scale;
}

} // namespace

void detail::TransformUpdate::cancel() noexcept {
    if (owner_) {
        owner_->callbackActive_ = false;
        owner_ = nullptr;
    }
    prepared_ = false;
    activeStorage_ = nullptr;
    storage_.reset();
    cache_.clear();
    order_.clear();
    localWrites_.clear();
    matrixWrites_.clear();
    stale_.clear();
}
void detail::TransformUpdate::finish() noexcept {
    cancel();
}
auto detail::TransformUpdate::prepare(World& world, std::span<const TransformWrite> writes,
                                      bool legacySignals) -> core::Result<void> {
    cancel();
    if (!world.threadChecker_.isCurrent() || world.callbackActive_)
        return core::unexpected(core::Error{"world.callback.reentrant",
                                            "World registry callback must not be reentrant"});
    owner_ = &world;
    world.callbackActive_ = true;
    legacySignals_ = legacySignals;
    try {
        auto result = prepareLocked(writes);
#if defined(CUEXIS_ALLOCATION_LIFETIME_OBSERVER)
        using internal::pd::memory::structure;
        structure("world.transform-cache", world.transformCache_.size(),
                  world.transformCache_.capacity());
        structure("world.transform-order", world.transformOrder_.size(),
                  world.transformOrder_.capacity());
        structure("world.transform-components",
                  world.registry_.storage<TransformComponent>().size(),
                  world.registry_.storage<TransformComponent>().capacity());
        structure("world.derived-components",
                  world.registry_.storage<WorldTransformComponent>().size(),
                  world.registry_.storage<WorldTransformComponent>().capacity());
        structure("scratch.transform-cache", cache_.size(), cache_.capacity());
        structure("scratch.transform-order", order_.size(), order_.capacity());
        structure("scratch.local-matrices", localScratch_.size(), localScratch_.capacity());
        structure("scratch.world-matrices", scratch_.size(), scratch_.capacity());
        structure("scratch.local-dirty", localDirty_.size(), localDirty_.capacity());
        structure("scratch.dirty", dirty_.size(), dirty_.capacity());
        structure("scratch.local-writes", localWrites_.size(), localWrites_.capacity());
        structure("scratch.matrix-writes", matrixWrites_.size(), matrixWrites_.capacity());
        structure("scratch.stale", stale_.size(), stale_.capacity());
        if (storage_)
            structure("candidate.derived-components", storage_->size(), storage_->capacity());
#endif
        if (!result)
            cancel();
        else
            prepared_ = true;
        return result;
    } catch (...) {
        cancel();
        throw;
    }
}
auto detail::TransformUpdate::prepareLocked(std::span<const TransformWrite> writes)
    -> core::Result<void> {
    auto& world = *owner_;
    auto& registry = world.registry_;
    localWrites_.clear();
    matrixWrites_.clear();
    stale_.clear();
    const auto transformView = registry.view<TransformComponent>();
    localWrites_.reserve(transformView.size());
    matrixWrites_.reserve(transformView.size());
    localScratch_.resize(transformView.size());
    scratch_.resize(transformView.size());
    localDirty_.assign(transformView.size(), false);
    dirty_.assign(transformView.size(), false);
    if (!std::is_sorted(writes.begin(), writes.end(), [](const auto& a, const auto& b) {
            return entityValue(a.entity) < entityValue(b.entity);
        }))
        return core::unexpected(
            core::Error{"world.property.transform_missing", "Transform writes must be ordered"});
    for (const auto& write : writes) {
        auto* slot = registry.valid(write.entity)
                         ? registry.try_get<TransformComponent>(write.entity)
                         : nullptr;
        if (!slot)
            return core::unexpected(
                core::Error{"world.property.transform_missing",
                            "A captured Transform baseline is no longer available"}
                    .withContext("entity", entityText(write.entity)));
        localWrites_.push_back({slot, write.value});
    }
    const auto effectiveTransform = [&](entt::entity entity) -> const TransformComponent& {
        auto found =
            std::lower_bound(writes.begin(), writes.end(), entity, [](const auto& a, auto b) {
                return entityValue(a.entity) < entityValue(b);
            });
        return found != writes.end() && found->entity == entity
                   ? found->value
                   : registry.get<TransformComponent>(entity);
    };
    for (const auto entity : registry.view<HierarchyComponent>())
        if (!registry.all_of<TransformComponent>(entity))
            return core::unexpected(
                core::Error{"world.transform.hierarchy_without_transform",
                            "Hierarchy entities must also have a local transform"}
                    .withContext("entity", entityText(entity)));
    bool valid = world.transformCacheValid_ && world.transformCache_.size() == transformView.size();
    if (valid)
        for (const auto& entry : world.transformCache_) {
            const auto* h = registry.valid(entry.entity)
                                ? registry.try_get<HierarchyComponent>(entry.entity)
                                : nullptr;
            if (!registry.valid(entry.entity) ||
                !registry.all_of<TransformComponent>(entry.entity) ||
                (h ? h->parent : entt::null) != entry.parent) {
                valid = false;
                break;
            }
        }
    coldCache_ = !valid;
    if (coldCache_) {
        auto& candidateCache = cache_;
        candidateCache.clear();
        candidateCache.reserve(transformView.size());
        for (const entt::entity entity : transformView) {
            const auto* hierarchy = registry.try_get<HierarchyComponent>(entity);
            const auto parent = hierarchy == nullptr ? entt::null : hierarchy->parent;
            const auto& transform = effectiveTransform(entity);
            auto local =
                core::composeTransform(transform.position, transform.rotation, transform.scale);
            if (!local) {
                return core::unexpected(core::Error{"world.transform.invalid_local_transform",
                                                    "A local transform could not be composed"}
                                            .withContext("entity", entityText(entity))
                                            .withCause(local.error()));
            }
            candidateCache.push_back(World::TransformCacheEntry{
                .entity = entity,
                .parent = parent,
                .parentIndex = std::nullopt,
                .local = transform,
                .localMatrix = *local,
                .world = {},
                .children = {},
            });
        }
        std::sort(candidateCache.begin(), candidateCache.end(),
                  [](const auto& left, const auto& right) {
                      return entityValue(left.entity) < entityValue(right.entity);
                  });

        std::unordered_map<EntityValue, std::size_t> indices;
        indices.reserve(candidateCache.size());
        for (std::size_t index = 0; index < candidateCache.size(); ++index) {
            indices.emplace(entityValue(candidateCache[index].entity), index);
        }

        for (std::size_t index = 0; index < candidateCache.size(); ++index) {
            auto& entry = candidateCache[index];
            if (entry.parent == entt::null) {
                continue;
            }
            if (!registry.valid(entry.parent)) {
                return core::unexpected(
                    core::Error{"world.transform.invalid_parent",
                                "A hierarchy parent is not a valid World entity"}
                        .withContext("entity", entityText(entry.entity))
                        .withContext("parent", entityText(entry.parent)));
            }
            const auto parent = indices.find(entityValue(entry.parent));
            if (parent == indices.end()) {
                return core::unexpected(
                    core::Error{"world.transform.parent_without_transform",
                                "A hierarchy parent must have a local transform"}
                        .withContext("entity", entityText(entry.entity))
                        .withContext("parent", entityText(entry.parent)));
            }
            if (parent->second == index) {
                return core::unexpected(core::Error{"world.transform.hierarchy_cycle",
                                                    "An entity cannot be its own hierarchy parent"}
                                            .withContext("entity", entityText(entry.entity)));
            }
            entry.parentIndex = parent->second;
            candidateCache[parent->second].children.push_back(index);
        }

        auto& candidateOrder = order_;
        candidateOrder.clear();
        candidateOrder.reserve(candidateCache.size());
        for (std::size_t index = 0; index < candidateCache.size(); ++index) {
            if (!candidateCache[index].parentIndex.has_value()) {
                candidateOrder.push_back(index);
            }
        }
        for (std::size_t cursor = 0; cursor < candidateOrder.size(); ++cursor) {
            const auto index = candidateOrder[cursor];
            for (const auto childIndex : candidateCache[index].children) {
                candidateOrder.push_back(childIndex);
            }
        }
        if (candidateOrder.size() != candidateCache.size()) {
            auto error = core::Error{"world.transform.hierarchy_cycle",
                                     "The transform hierarchy contains a cycle"};
            for (std::size_t index = 0; index < candidateCache.size(); ++index) {
                if (std::find(candidateOrder.begin(), candidateOrder.end(), index) ==
                    candidateOrder.end()) {
                    error.withContext("entity", entityText(candidateCache[index].entity));
                    break;
                }
            }
            return core::unexpected(std::move(error));
        }

        for (const auto index : candidateOrder) {
            auto& entry = candidateCache[index];
            entry.world =
                entry.parentIndex.has_value()
                    ? core::multiply(candidateCache[*entry.parentIndex].world, entry.localMatrix)
                    : entry.localMatrix;
            if (!core::isFinite(entry.world)) {
                return core::unexpected(core::Error{"world.transform.invalid_world_transform",
                                                    "A world transform is not finite"}
                                            .withContext("entity", entityText(entry.entity)));
            }
        }

        for (std::size_t i = 0; i < cache_.size(); ++i) {
            const auto& entry = cache_[i];
            matrixWrites_.push_back({entry.entity,
                                     registry.try_get<WorldTransformComponent>(entry.entity),
                                     entry.world,
                                     i,
                                     {},
                                     {}});
        }
    } else {
        const auto count = world.transformCache_.size();
        for (std::size_t i = 0; i < count; ++i) {
            const auto& entry = world.transformCache_[i];
            const auto& local = effectiveTransform(entry.entity);
            if (!sameTransform(local, entry.local)) {
                auto matrix = core::composeTransform(local.position, local.rotation, local.scale);
                if (!matrix)
                    return core::unexpected(core::Error{"world.transform.invalid_local_transform",
                                                        "A local transform could not be composed"}
                                                .withContext("entity", entityText(entry.entity))
                                                .withCause(matrix.error()));
                localScratch_[i] = *matrix;
                localDirty_[i] = dirty_[i] = true;
            } else if (!registry.all_of<WorldTransformComponent>(entry.entity))
                dirty_[i] = true;
        }
        for (auto i : world.transformOrder_) {
            const auto& entry = world.transformCache_[i];
            if (entry.parentIndex && dirty_[*entry.parentIndex])
                dirty_[i] = true;
            if (!dirty_[i])
                continue;
            const auto& local = localDirty_[i] ? localScratch_[i] : entry.localMatrix;
            if (entry.parentIndex) {
                auto parent = *entry.parentIndex;
                scratch_[i] = core::multiply(
                    dirty_[parent] ? scratch_[parent] : world.transformCache_[parent].world, local);
            } else
                scratch_[i] = local;
            if (!core::isFinite(scratch_[i]))
                return core::unexpected(core::Error{"world.transform.invalid_world_transform",
                                                    "A world transform is not finite"}
                                            .withContext("entity", entityText(entry.entity)));
            matrixWrites_.push_back(
                {entry.entity, registry.try_get<WorldTransformComponent>(entry.entity), scratch_[i],
                 i, localDirty_[i] ? std::optional{effectiveTransform(entry.entity)} : std::nullopt,
                 local});
        }
    }
    auto& pool = registry.storage<WorldTransformComponent>();
    activeStorage_ = &static_cast<entt::storage<WorldTransformComponent>&>(pool);
    for (auto entity : registry.view<WorldTransformComponent>())
        if (!registry.all_of<TransformComponent>(entity))
            stale_.push_back(entity);
    storageReplacement_ = !stale_.empty() ||
                          std::ranges::any_of(matrixWrites_, [](const auto& w) { return !w.slot; });
    if (storageReplacement_ && !legacySignals_) {
        if (!pool.on_construct().empty() || !pool.on_destroy().empty())
            return core::unexpected(core::Error{"world.callback.exception",
                                                "Transactional storage membership has observers"});
        storage_.emplace();
        storage_->reserve(transformView.size());
        const auto& entries = coldCache_ ? cache_ : world.transformCache_;
        for (std::size_t i = 0; i < entries.size(); ++i) {
            const auto& entry = entries[i];
            storage_->emplace(entry.entity, coldCache_ || !dirty_[i] ? entry.world : scratch_[i]);
        }
    }
    return {};
}
void detail::TransformUpdate::installCache() noexcept {
    auto& world = *owner_;
    if (coldCache_) {
        world.transformCache_.swap(cache_);
        world.transformOrder_.swap(order_);
        world.transformCacheValid_ = true;
    } else
        for (const auto& write : matrixWrites_) {
            auto& entry = world.transformCache_[write.cacheIndex];
            if (write.local) {
                entry.local = *write.local;
                entry.localMatrix = write.localMatrix;
            }
            entry.world = write.value;
        }
}
void detail::TransformUpdate::apply() noexcept {
    if (!owner_ || !prepared_ || legacySignals_)
        std::terminate();
    for (const auto& write : localWrites_)
        *write.slot = write.value;
    if (storageReplacement_) {
        // Swap only basic_storage payload, preserving registry owner and signal connections.
        static_assert(noexcept(activeStorage_->swap(*storage_)));
        activeStorage_->swap(*storage_);
    } else
        for (const auto& write : matrixWrites_)
            write.slot->matrix = write.value;
    installCache();
    prepared_ = false;
}
auto detail::TransformUpdate::applyLegacy() -> core::Result<void> {
    if (!owner_ || !prepared_ || !legacySignals_)
        return core::unexpected(
            core::Error{"world.property.not_prepared", "Legacy transform plan unavailable"});
    try {
        auto& registry = owner_->registry_;
        for (auto entity : stale_)
            registry.remove<WorldTransformComponent>(entity);
        for (const auto& write : matrixWrites_)
            registry.emplace_or_replace<WorldTransformComponent>(write.entity, write.value);
        installCache();
        finish();
        return {};
    } catch (const std::bad_alloc&) {
        cancel();
        throw;
    } catch (const std::exception& e) {
        cancel();
        return core::unexpected(
            core::Error{"world.callback.exception", "World registry callback raised an exception"}
                .withContext("exception", e.what()));
    } catch (...) {
        cancel();
        return core::unexpected(
            core::Error{"world.callback.exception", "World registry callback raised an exception"});
    }
}
auto updateWorldTransforms(World& world) -> core::Result<void> {
    if (!world.threadChecker_.isCurrent() || world.callbackActive_)
        return core::unexpected(core::Error{"world.callback.reentrant",
                                            "World registry callback must not be reentrant"});
    if (!world.transformUpdateScratch_)
        world.transformUpdateScratch_ = std::make_unique<detail::TransformUpdate>();
    auto& update = *world.transformUpdateScratch_;
    auto prepared = update.prepare(world, {}, true);
    if (!prepared)
        return prepared;
    return update.applyLegacy();
}
} // namespace cuexis::world

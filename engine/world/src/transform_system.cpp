//  TransformSystem implementation - updates world matrices in parent-first order.
//  The hierarchy topology is cached; unchanged frames only scan local values.

#include <cuexis/world/transform_system.hpp>

#include <cuexis/core/error.hpp>
#include <cuexis/core/math.hpp>
#include <cuexis/world/components.hpp>
#include <cuexis/world/world.hpp>

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

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

auto updateWorldTransforms(World& world) -> core::Result<void> {
    return world.withRegistry([&world](entt::registry& registry) -> core::Result<void> {
        const auto hierarchyView = registry.view<HierarchyComponent>();
        for (const entt::entity entity : hierarchyView) {
            if (!registry.all_of<TransformComponent>(entity)) {
                return core::unexpected(
                    core::Error{"world.transform.hierarchy_without_transform",
                                "Hierarchy entities must also have a local transform"}
                        .withContext("entity", entityText(entity)));
            }
        }

        const auto transformView = registry.view<TransformComponent>();
        bool cacheValid =
            world.transformCacheValid_ && world.transformCache_.size() == transformView.size();
        if (cacheValid) {
            for (const auto& entry : world.transformCache_) {
                if (!registry.valid(entry.entity) ||
                    !registry.all_of<TransformComponent>(entry.entity)) {
                    cacheValid = false;
                    break;
                }
                const auto* hierarchy = registry.try_get<HierarchyComponent>(entry.entity);
                const auto parent = hierarchy == nullptr ? entt::null : hierarchy->parent;
                if (parent != entry.parent) {
                    cacheValid = false;
                    break;
                }
            }
        }

        if (!cacheValid) {
            std::vector<World::TransformCacheEntry> candidateCache;
            candidateCache.reserve(transformView.size());
            for (const entt::entity entity : transformView) {
                const auto* hierarchy = registry.try_get<HierarchyComponent>(entity);
                const auto parent = hierarchy == nullptr ? entt::null : hierarchy->parent;
                const auto& transform = registry.get<TransformComponent>(entity);
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
                    .local = transform,
                    .localMatrix = *local,
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
                    return core::unexpected(
                        core::Error{"world.transform.hierarchy_cycle",
                                    "An entity cannot be its own hierarchy parent"}
                            .withContext("entity", entityText(entry.entity)));
                }
                entry.parentIndex = parent->second;
                candidateCache[parent->second].children.push_back(index);
            }

            std::vector<std::size_t> candidateOrder;
            candidateOrder.reserve(candidateCache.size());
            for (std::size_t index = 0; index < candidateCache.size(); ++index) {
                if (!candidateCache[index].parentIndex.has_value()) {
                    candidateOrder.push_back(index);
                }
            }
            for (std::size_t cursor = 0; cursor < candidateOrder.size(); ++cursor) {
                const auto index = candidateOrder[cursor];
                candidateOrder.insert(candidateOrder.end(), candidateCache[index].children.begin(),
                                      candidateCache[index].children.end());
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
                entry.world = entry.parentIndex.has_value()
                                  ? core::multiply(candidateCache[*entry.parentIndex].world,
                                                   entry.localMatrix)
                                  : entry.localMatrix;
                if (!core::isFinite(entry.world)) {
                    return core::unexpected(core::Error{"world.transform.invalid_world_transform",
                                                        "A world transform is not finite"}
                                                .withContext("entity", entityText(entry.entity)));
                }
            }

            std::vector<entt::entity> staleWorldTransforms;
            const auto worldTransformView = registry.view<WorldTransformComponent>();
            for (const entt::entity entity : worldTransformView) {
                if (!registry.all_of<TransformComponent>(entity)) {
                    staleWorldTransforms.push_back(entity);
                }
            }
            for (const entt::entity entity : staleWorldTransforms) {
                registry.remove<WorldTransformComponent>(entity);
            }

            world.transformCache_ = std::move(candidateCache);
            world.transformOrder_ = std::move(candidateOrder);
            world.transformLocalScratch_.resize(world.transformCache_.size());
            world.transformScratch_.resize(world.transformCache_.size());
            world.transformLocalDirty_.resize(world.transformCache_.size());
            world.transformDirty_.resize(world.transformCache_.size());
            world.transformCacheValid_ = true;
            for (const auto& entry : world.transformCache_) {
                registry.emplace_or_replace<WorldTransformComponent>(entry.entity, entry.world);
            }
            return {};
        }

        std::fill(world.transformLocalDirty_.begin(), world.transformLocalDirty_.end(), false);
        std::fill(world.transformDirty_.begin(), world.transformDirty_.end(), false);
        bool anyDirty = false;
        for (std::size_t index = 0; index < world.transformCache_.size(); ++index) {
            const auto& entry = world.transformCache_[index];
            const auto& transform = registry.get<TransformComponent>(entry.entity);
            if (!sameTransform(transform, entry.local)) {
                auto local =
                    core::composeTransform(transform.position, transform.rotation, transform.scale);
                if (!local) {
                    return core::unexpected(core::Error{"world.transform.invalid_local_transform",
                                                        "A local transform could not be composed"}
                                                .withContext("entity", entityText(entry.entity))
                                                .withCause(local.error()));
                }
                world.transformLocalScratch_[index] = *local;
                world.transformLocalDirty_[index] = true;
                world.transformDirty_[index] = true;
                anyDirty = true;
            } else if (!registry.all_of<WorldTransformComponent>(entry.entity)) {
                world.transformDirty_[index] = true;
                anyDirty = true;
            }
        }
        if (!anyDirty) {
            return {};
        }

        for (const auto index : world.transformOrder_) {
            const auto& entry = world.transformCache_[index];
            if (entry.parentIndex.has_value() && world.transformDirty_[*entry.parentIndex]) {
                world.transformDirty_[index] = true;
            }
            if (!world.transformDirty_[index]) {
                continue;
            }
            const auto& local = world.transformLocalDirty_[index]
                                    ? world.transformLocalScratch_[index]
                                    : entry.localMatrix;
            if (entry.parentIndex.has_value()) {
                const auto parentIndex = *entry.parentIndex;
                const auto& parentWorld = world.transformDirty_[parentIndex]
                                              ? world.transformScratch_[parentIndex]
                                              : world.transformCache_[parentIndex].world;
                world.transformScratch_[index] = core::multiply(parentWorld, local);
            } else {
                world.transformScratch_[index] = local;
            }
            if (!core::isFinite(world.transformScratch_[index])) {
                return core::unexpected(core::Error{"world.transform.invalid_world_transform",
                                                    "A world transform is not finite"}
                                            .withContext("entity", entityText(entry.entity)));
            }
        }

        for (const auto index : world.transformOrder_) {
            if (!world.transformDirty_[index]) {
                continue;
            }
            auto& entry = world.transformCache_[index];
            if (world.transformLocalDirty_[index]) {
                entry.local = registry.get<TransformComponent>(entry.entity);
                entry.localMatrix = world.transformLocalScratch_[index];
            }
            entry.world = world.transformScratch_[index];
            registry.emplace_or_replace<WorldTransformComponent>(entry.entity, entry.world);
        }
        return {};
    });
}

} // namespace cuexis::world

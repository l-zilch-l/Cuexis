//  TransformSystem 实现 — 按父级优先顺序自顶向下更新世界矩阵
//  检查循环引用和无效父引用；使用脏标记避免无变化层级重复计算
//  Local = Translation * Rotation * Scale; World = ParentWorld * Local

#include <cuexis/world/transform_system.hpp>

#include <cuexis/core/error.hpp>
#include <cuexis/core/math.hpp>
#include <cuexis/world/components.hpp>
#include <cuexis/world/world.hpp>

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>

#include <algorithm>
#include <cstdint>
#include <deque>
#include <optional>
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

struct TransformNode final {
    entt::entity entity{entt::null};
    core::Mat4 local{};
    core::Mat4 world{};
    std::optional<std::size_t> parentIndex;
    std::vector<std::size_t> children;
    bool resolved{};
};

} // namespace

auto updateWorldTransforms(World& world) -> core::Result<void> {
    return world.withRegistry([](entt::registry& registry) -> core::Result<void> {
        const auto hierarchyView = registry.view<HierarchyComponent>();
        for (const entt::entity entity : hierarchyView) {
            if (!registry.all_of<TransformComponent>(entity)) {
                return core::unexpected(
                    core::Error{"world.transform.hierarchy_without_transform",
                                "Hierarchy entities must also have a local transform"}
                        .withContext("entity", entityText(entity)));
            }
        }

        std::vector<entt::entity> entities;
        const auto transformView = registry.view<TransformComponent>();
        for (const entt::entity entity : transformView) {
            entities.push_back(entity);
        }
        std::sort(entities.begin(), entities.end(), [](entt::entity left, entt::entity right) {
            return entityValue(left) < entityValue(right);
        });

        std::vector<TransformNode> nodes;
        nodes.reserve(entities.size());
        std::unordered_map<EntityValue, std::size_t> nodeIndex;
        nodeIndex.reserve(entities.size());

        for (const entt::entity entity : entities) {
            const auto& transform = registry.get<TransformComponent>(entity);
            auto local =
                core::composeTransform(transform.position, transform.rotation, transform.scale);
            if (!local) {
                return core::unexpected(core::Error{"world.transform.invalid_local_transform",
                                                    "A local transform could not be composed"}
                                            .withContext("entity", entityText(entity))
                                            .withCause(local.error()));
            }

            const std::size_t index = nodes.size();
            nodeIndex.emplace(entityValue(entity), index);
            nodes.push_back(TransformNode{.entity = entity, .local = *local});
        }

        for (std::size_t index = 0; index < nodes.size(); ++index) {
            const entt::entity entity = nodes[index].entity;
            const auto* hierarchy = registry.try_get<HierarchyComponent>(entity);
            if (hierarchy == nullptr || hierarchy->parent == entt::null) {
                continue;
            }
            if (!registry.valid(hierarchy->parent)) {
                return core::unexpected(
                    core::Error{"world.transform.invalid_parent",
                                "A hierarchy parent is not a valid World entity"}
                        .withContext("entity", entityText(entity))
                        .withContext("parent", entityText(hierarchy->parent)));
            }

            const auto parent = nodeIndex.find(entityValue(hierarchy->parent));
            if (parent == nodeIndex.end()) {
                return core::unexpected(
                    core::Error{"world.transform.parent_without_transform",
                                "A hierarchy parent must have a local transform"}
                        .withContext("entity", entityText(entity))
                        .withContext("parent", entityText(hierarchy->parent)));
            }
            if (parent->second == index) {
                return core::unexpected(core::Error{"world.transform.hierarchy_cycle",
                                                    "An entity cannot be its own hierarchy parent"}
                                            .withContext("entity", entityText(entity)));
            }

            nodes[index].parentIndex = parent->second;
            nodes[parent->second].children.push_back(index);
        }

        std::deque<std::size_t> ready;
        for (std::size_t index = 0; index < nodes.size(); ++index) {
            if (!nodes[index].parentIndex.has_value()) {
                ready.push_back(index);
            }
        }

        std::size_t processed = 0;
        while (!ready.empty()) {
            const std::size_t index = ready.front();
            ready.pop_front();

            auto& node = nodes[index];
            node.world = node.parentIndex.has_value()
                             ? core::multiply(nodes[*node.parentIndex].world, node.local)
                             : node.local;
            node.resolved = true;
            ++processed;

            for (const std::size_t child : node.children) {
                ready.push_back(child);
            }
        }

        if (processed != nodes.size()) {
            const auto cycle = std::find_if(nodes.begin(), nodes.end(),
                                            [](const auto& node) { return !node.resolved; });
            auto error = core::Error{"world.transform.hierarchy_cycle",
                                     "The transform hierarchy contains a cycle"};
            if (cycle != nodes.end()) {
                error.withContext("entity", entityText(cycle->entity));
            }
            return core::unexpected(std::move(error));
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
        for (const auto& node : nodes) {
            registry.emplace_or_replace<WorldTransformComponent>(node.entity, node.world);
        }

        return {};
    });
}

} // namespace cuexis::world

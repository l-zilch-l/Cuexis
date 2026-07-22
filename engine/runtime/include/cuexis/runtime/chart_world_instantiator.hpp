#pragma once

//  ChartWorldInstantiator — 将 ChartRuntime 实例化到 EnTT World
//  validate(): 无副作用结构验证（排序、parent 引用、Behavior 引用）
//  instantiate(): 创建 Entity、设置 Component、建立层级和对象映射
//  ObjectEntityMap: ChartObjectId 到 entt::entity 的会话期映射
//  ResolvedRenderableResources: 已解析的类型化 Handle（阶段 1B）

#include <cuexis/assets/resource_handle.hpp>
#include <cuexis/chart/chart_runtime.hpp>
#include <cuexis/core/diagnostic.hpp>
#include <cuexis/world/world.hpp>

#include <entt/entity/entity.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace cuexis::runtime {

inline constexpr std::size_t runtimeDiagnosticLimit = 1024;

struct ObjectEntityEntry final {
    chart::ChartObjectId objectId;
    entt::entity entity{entt::null};
};

class ObjectEntityMap final {
  public:
    [[nodiscard]] auto find(const chart::ChartObjectId& objectId) const noexcept
        -> std::optional<entt::entity>;
    [[nodiscard]] auto size() const noexcept -> std::size_t;
    [[nodiscard]] auto entries() const noexcept -> const std::vector<ObjectEntityEntry>&;

    void clear() noexcept;

  private:
    friend class ChartWorldInstantiator;
    friend class RuntimeSession;

    std::vector<ObjectEntityEntry> entries_;
};

struct ChartWorldInstantiation final {
    std::unique_ptr<world::World> world;
    ObjectEntityMap objects;
};

struct ChartWorldInstantiationResult final {
    ChartWorldInstantiationResult()
        : diagnostics(runtimeDiagnosticLimit,
                      core::Diagnostic{core::DiagnosticSeverity::Error,
                                       "runtime.chart.diagnostic_limit",
                                       "Runtime Chart diagnostic limit was reached", "/objects"}) {}

    std::optional<ChartWorldInstantiation> value;
    core::Diagnostics diagnostics;

    [[nodiscard]] auto hasValue() const noexcept -> bool {
        return value.has_value() && !diagnostics.hasErrors();
    }
};

struct ResolvedRenderableResources final {
    assets::MeshHandle mesh;
    assets::MaterialHandle material;

    friend bool operator==(const ResolvedRenderableResources&,
                           const ResolvedRenderableResources&) = default;
};

class ChartWorldInstantiator final {
  public:
    // Performs side-effect-free Runtime structure validation before resource I/O.
    [[nodiscard]] static auto validate(const chart::ChartRuntime& runtime) -> core::Diagnostics;

    // The calling thread becomes the owner of the returned World.
    [[nodiscard]] static auto instantiate(const chart::ChartRuntime& runtime)
        -> ChartWorldInstantiationResult;

    // A non-empty binding span must contain exactly one entry per Runtime object.
    [[nodiscard]] static auto
    instantiate(const chart::ChartRuntime& runtime,
                std::span<const std::optional<ResolvedRenderableResources>> renderableResources,
                std::uint64_t expectedManagerToken) -> ChartWorldInstantiationResult;
};

} // namespace cuexis::runtime

#pragma once

//  RuntimeSession — 一次播放或 Studio 预览的事务式运行时实例
//  拥有 World、ChartRuntime、对象映射和 ResourceScope
//  事务式 prepare/commit：先无副作用结构验证 → 临时 Scope 获取资源 → 实例化 World → commit 发布
//  reload: 完整 Replacement，失败保留旧 World/Scope/诊断
//  销毁顺序固定为 World → ResourceScope，确保 Component 清理时资源 Lease 仍有效
//  PreparedSession 绑定 Session token + Manager token，拒绝跨 owner 提交

#include <cuexis/assets/resource_manager.hpp>
#include <cuexis/chart/chart_runtime.hpp>
#include <cuexis/core/diagnostic.hpp>
#include <cuexis/core/result.hpp>
#include <cuexis/core/thread_checker.hpp>
#include <cuexis/runtime/chart_world_instantiator.hpp>
#include <cuexis/runtime/runtime_frame.hpp>
#include <cuexis/world/world.hpp>

#include <entt/entity/entity.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace cuexis::runtime {

class RuntimeSession;
class RuntimeEvaluationState;

class PreparedRuntimeSession final {
  public:
    // Prepared data remains bound to the RuntimeSession owner thread that created its World.
    PreparedRuntimeSession(const PreparedRuntimeSession&) = delete;
    ~PreparedRuntimeSession();
    auto operator=(const PreparedRuntimeSession&) -> PreparedRuntimeSession& = delete;
    PreparedRuntimeSession(PreparedRuntimeSession&& other) noexcept;
    auto operator=(PreparedRuntimeSession&& other) noexcept -> PreparedRuntimeSession&;

  private:
    friend class RuntimeSession;

    PreparedRuntimeSession(const RuntimeSession& owner, std::uint64_t ownerToken,
                           std::uint64_t managerToken, chart::ChartRuntime chartRuntime,
                           ObjectEntityMap objects, core::Diagnostics diagnostics,
                           std::unique_ptr<RuntimeEvaluationState> evaluation,
                           std::optional<assets::ResourceScope> resourceScope,
                           std::unique_ptr<world::World> world) noexcept;

    const RuntimeSession* owner_{};
    std::uint64_t ownerToken_{};
    std::uint64_t managerToken_{};
    chart::ChartRuntime chartRuntime_;
    ObjectEntityMap objects_;
    core::Diagnostics diagnostics_;
    std::unique_ptr<RuntimeEvaluationState> evaluation_;
    // Declared before World so member destruction releases World first.
    std::optional<assets::ResourceScope> resourceScope_;
    std::unique_ptr<world::World> world_;
};

struct PreparedRuntimeSessionResult final {
    PreparedRuntimeSessionResult()
        : diagnostics(runtimeDiagnosticLimit,
                      core::Diagnostic{core::DiagnosticSeverity::Error,
                                       "runtime.session.diagnostic_limit",
                                       "RuntimeSession diagnostic limit was reached"}) {}

    std::optional<PreparedRuntimeSession> prepared;
    core::Diagnostics diagnostics;

    [[nodiscard]] auto hasValue() const noexcept -> bool {
        return prepared.has_value() && !diagnostics.hasErrors();
    }
};

struct RuntimeSessionReloadResult final {
    RuntimeSessionReloadResult()
        : diagnostics(runtimeDiagnosticLimit,
                      core::Diagnostic{core::DiagnosticSeverity::Error,
                                       "runtime.session.diagnostic_limit",
                                       "RuntimeSession diagnostic limit was reached"}) {}

    bool reloaded{};
    core::Diagnostics diagnostics;
};

class RuntimeSession final {
  public:
    RuntimeSession() noexcept;
    explicit RuntimeSession(assets::ResourceManager& resourceManager) noexcept;
    ~RuntimeSession();

    RuntimeSession(const RuntimeSession&) = delete;
    auto operator=(const RuntimeSession&) -> RuntimeSession& = delete;
    RuntimeSession(RuntimeSession&&) = delete;
    auto operator=(RuntimeSession&&) -> RuntimeSession& = delete;

    [[nodiscard]] auto prepare(chart::ChartRuntime chartRuntime) const
        -> PreparedRuntimeSessionResult;
    [[nodiscard]] auto commit(PreparedRuntimeSession&& prepared) -> core::Result<void>;
    [[nodiscard]] auto reload(chart::ChartRuntime replacement) -> RuntimeSessionReloadResult;
    [[nodiscard]] auto reload(chart::ChartRuntime replacement, const RuntimeFrame& targetFrame,
                              ReloadPolicy policy) -> RuntimeSessionReloadResult;
    [[nodiscard]] auto update(const RuntimeFrame& frame) -> core::Result<void>;
    [[nodiscard]] auto unload() -> core::Result<void>;

    [[nodiscard]] auto empty() const noexcept -> bool;
    [[nodiscard]] auto objectCount() const noexcept -> std::size_t;
    [[nodiscard]] auto resourceCount() const noexcept -> std::size_t;
    [[nodiscard]] auto activeDiagnostics() const noexcept -> const core::Diagnostics&;
    [[nodiscard]] auto findEntity(const chart::ChartObjectId& objectId) const
        -> core::Result<std::optional<entt::entity>>;

    template <typename Callback>
    [[nodiscard]] auto withWorld(Callback&& callback)
        -> core::Result<std::invoke_result_t<Callback&&, world::World&>> {
        using ReturnType = std::invoke_result_t<Callback&&, world::World&>;
        static_assert(!std::is_reference_v<ReturnType>,
                      "RuntimeSession World callbacks must not return references");
        static_assert(!std::is_pointer_v<std::remove_cv_t<ReturnType>>,
                      "RuntimeSession World callbacks must not return pointers");

        if (!threadChecker_.isCurrent()) {
            return core::unexpected(core::Error{"runtime.session.not_owner_thread",
                                                "RuntimeSession belongs to another thread"});
        }
        if (!world_) {
            return core::unexpected(
                core::Error{"runtime.session.empty", "RuntimeSession has no committed World"});
        }

        if constexpr (std::is_void_v<ReturnType>) {
            std::invoke(std::forward<Callback>(callback), *world_);
            return {};
        } else {
            return std::invoke(std::forward<Callback>(callback), *world_);
        }
    }

    template <typename Callback>
    [[nodiscard]] auto withWorld(Callback&& callback) const
        -> core::Result<std::invoke_result_t<Callback&&, const world::World&>> {
        using ReturnType = std::invoke_result_t<Callback&&, const world::World&>;
        static_assert(!std::is_reference_v<ReturnType>,
                      "RuntimeSession World callbacks must not return references");
        static_assert(!std::is_pointer_v<std::remove_cv_t<ReturnType>>,
                      "RuntimeSession World callbacks must not return pointers");

        if (!threadChecker_.isCurrent()) {
            return core::unexpected(core::Error{"runtime.session.not_owner_thread",
                                                "RuntimeSession belongs to another thread"});
        }
        if (!world_) {
            return core::unexpected(
                core::Error{"runtime.session.empty", "RuntimeSession has no committed World"});
        }

        if constexpr (std::is_void_v<ReturnType>) {
            std::invoke(std::forward<Callback>(callback), *world_);
            return {};
        } else {
            return std::invoke(std::forward<Callback>(callback), *world_);
        }
    }

  private:
    void replaceWith(PreparedRuntimeSession&& prepared) noexcept;
    [[nodiscard]] auto updatePrepared(RuntimeEvaluationState& state, const RuntimeFrame& frame)
        -> core::Result<void>;

    std::optional<chart::ChartRuntime> chartRuntime_;
    ObjectEntityMap objects_;
    assets::ResourceManager* resourceManager_{};
    std::uint64_t sessionToken_{};
    std::uint64_t managerToken_{};
    core::Diagnostics activeDiagnostics_;
    std::unique_ptr<RuntimeEvaluationState> evaluation_;
    // Declared before World so member destruction releases World first.
    std::optional<assets::ResourceScope> resourceScope_;
    std::unique_ptr<world::World> world_;
    core::ThreadChecker threadChecker_{};
    std::optional<RuntimeFrame> lastFrame_;
};

} // namespace cuexis::runtime

#pragma once

//  RuntimeSession - transactional runtime instance for one playback or Studio preview
//  Owns the World, the ChartRuntime, the object map and the ResourceScope
//  Transactional prepare/commit: side-effect-free structural validation first, then acquire
//  resources through a temporary Scope, then instantiate the World, then publish on commit
//  reload: full replacement; on failure the old World/Scope/diagnostics are retained
//  Destruction order is fixed as World then ResourceScope, so resource Leases remain valid
//  while components are being cleaned up
//  PreparedSession binds a session token plus a manager token and rejects cross-owner commits

#include <cuexis/assets/resource_manager.hpp>
#include <cuexis/chart/chart_runtime.hpp>
#include <cuexis/core/diagnostic.hpp>
#include <cuexis/core/result.hpp>
#include <cuexis/core/thread_checker.hpp>
#include <cuexis/runtime/chart_world_instantiator.hpp>
#include <cuexis/runtime/runtime_frame.hpp>
#include <cuexis/world/property.hpp>
#include <cuexis/world/world.hpp>

#include <entt/entity/entity.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace cuexis::animation {
class AnimationProgram;
}

namespace cuexis::runtime {

enum class RuntimeSessionKind : std::uint8_t {
    Playback,
    StudioPreview,
};

inline constexpr std::size_t maxRuntimeDebugRecords = 65536;

struct RuntimeDebugOptions final {
    bool enabled{};
    std::size_t capacity{};
};

struct RuntimeDebugAnimationLayer final {
    std::string identity;
    std::int64_t priority{};
    double weight{};
    std::vector<std::string> mask;
    world::PropertyValue value{};
};

struct RuntimeDebugRecord final {
    chart::ChartObjectId objectId;
    world::PropertyId property{};
    world::PropertyValue initialValue{};
    std::optional<std::size_t> eventIndex;
    double normalizedProgress{};
    world::PropertyValue behaviorValue{};
    world::PropertyValue animationValue{};
    world::PropertyValue hostOverrideValue{};
    world::PropertyValue previewOverrideValue{};
    world::PropertyValue finalValue{};
    std::optional<world::PropertyLayer> sourceLayer{};
    bool conflict{};
    std::vector<RuntimeDebugAnimationLayer> animationLayers;
};

struct PropertyOverrideWrite final {
    chart::ChartObjectId objectId;
    world::PropertyId property{};
    world::PropertyValue value{};
};

struct BasePropertyCommand final {
    chart::ChartObjectId objectId;
    world::PropertyId property{};
    world::PropertyValue value{};
};

struct RuntimeDebugSnapshot final {
    std::vector<RuntimeDebugRecord> records;
    bool truncated{};
};

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
  private:
    class CallbackScope final {
      public:
        explicit CallbackScope(bool& active) noexcept : active_(active) {
            active_ = true;
        }
        ~CallbackScope() noexcept {
            active_ = false;
        }

        CallbackScope(const CallbackScope&) = delete;
        CallbackScope& operator=(const CallbackScope&) = delete;

      private:
        bool& active_;
    };

  public:
    RuntimeSession() noexcept;
    explicit RuntimeSession(RuntimeSessionKind kind) noexcept;
    explicit RuntimeSession(assets::ResourceManager& resourceManager) noexcept;
    RuntimeSession(RuntimeSessionKind kind, assets::ResourceManager& resourceManager) noexcept;
    ~RuntimeSession();

    RuntimeSession(const RuntimeSession&) = delete;
    auto operator=(const RuntimeSession&) -> RuntimeSession& = delete;
    RuntimeSession(RuntimeSession&&) = delete;
    auto operator=(RuntimeSession&&) -> RuntimeSession& = delete;

    [[nodiscard]] auto prepare(chart::ChartRuntime chartRuntime) const
        -> PreparedRuntimeSessionResult;
    [[nodiscard]] auto prepare(chart::ChartRuntime chartRuntime,
                               animation::AnimationProgram&& animation) const
        -> PreparedRuntimeSessionResult;
    [[nodiscard]] auto commit(PreparedRuntimeSession&& prepared) -> core::Result<void>;
    [[nodiscard]] auto reload(chart::ChartRuntime replacement) -> RuntimeSessionReloadResult;
    [[nodiscard]] auto reload(chart::ChartRuntime replacement, const RuntimeFrame& targetFrame,
                              ReloadPolicy policy) -> RuntimeSessionReloadResult;
    [[nodiscard]] auto update(const RuntimeFrame& frame) -> core::Result<void>;
    [[nodiscard]] auto unload() -> core::Result<void>;
    [[nodiscard]] auto configureDebug(RuntimeDebugOptions options) -> core::Result<void>;
    [[nodiscard]] auto debugSnapshot() const -> core::Result<RuntimeDebugSnapshot>;
    [[nodiscard]] auto acquireOverride(world::OverrideKind kind, std::string ownerId,
                                       std::int64_t priority, std::uint16_t propertyMask,
                                       world::OverrideLifetime lifetime,
                                       std::span<const PropertyOverrideWrite> writes)
        -> core::Result<world::OverrideTokenId>;
    [[nodiscard]] auto releaseOverride(world::OverrideTokenId id) -> core::Result<void>;
    [[nodiscard]] auto applyBaseProperty(const chart::ChartObjectId& objectId,
                                         world::PropertyId property, world::PropertyValue value)
        -> core::Result<void>;
    [[nodiscard]] auto applyBaseProperty(const BasePropertyCommand& command) -> core::Result<void>;
    [[nodiscard]] auto baseRevision() const noexcept -> std::uint64_t;

    [[nodiscard]] auto kind() const noexcept -> RuntimeSessionKind {
        return sessionKind_;
    }
    [[nodiscard]] auto empty() const noexcept -> bool;
    [[nodiscard]] auto objectCount() const noexcept -> std::size_t;
    [[nodiscard]] auto resourceCount() const noexcept -> std::size_t;
    [[nodiscard]] auto activeDiagnostics() const noexcept -> const core::Diagnostics&;
    [[nodiscard]] auto findEntity(const chart::ChartObjectId& objectId) const
        -> core::Result<std::optional<entt::entity>>;

    template <typename Callback>
    [[nodiscard]] auto withWorld(Callback&& callback)
        -> core::GuardedInvokeResult<Callback&&, world::World&> {
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
        if (callbackActive_) {
            return core::unexpected(
                core::Error{"runtime.session.callback_reentrant",
                            "RuntimeSession World callback must not be reentrant"});
        }

        // The callback runs synchronously on the owner thread and is never retained. It must not
        // re-enter this RuntimeSession or retain World references beyond the call.
        CallbackScope callbackScope{callbackActive_};
        return core::invokeGuarded("runtime.session.callback_exception",
                                   "RuntimeSession World callback raised an exception",
                                   std::forward<Callback>(callback), *world_);
    }

    template <typename Callback>
    [[nodiscard]] auto withWorld(Callback&& callback) const
        -> core::GuardedInvokeResult<Callback&&, const world::World&> {
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
        if (callbackActive_) {
            return core::unexpected(
                core::Error{"runtime.session.callback_reentrant",
                            "RuntimeSession World callback must not be reentrant"});
        }

        CallbackScope callbackScope{callbackActive_};
        return core::invokeGuarded("runtime.session.callback_exception",
                                   "RuntimeSession World callback raised an exception",
                                   std::forward<Callback>(callback), *world_);
    }

  private:
    void replaceWith(PreparedRuntimeSession&& prepared) noexcept;
    [[nodiscard]] auto updatePrepared(RuntimeEvaluationState& state,
                                      const chart::TimingMap& timingMap, const RuntimeFrame& frame)
        -> core::Result<void>;
    void captureDebug(const RuntimeEvaluationState& state, double beat);

    RuntimeSessionKind sessionKind_{RuntimeSessionKind::Playback};
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
    mutable bool callbackActive_{false};
    core::ThreadChecker threadChecker_{};
    std::optional<RuntimeFrame> lastFrame_;
    RuntimeDebugOptions debugOptions_;
    std::vector<RuntimeDebugRecord> debugRecords_;
    bool debugTruncated_{};
};

} // namespace cuexis::runtime

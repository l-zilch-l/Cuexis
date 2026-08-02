#pragma once

//  World - thread-safe wrapper around the EnTT registry
//  Accessed through the withRegistry() callback; returning internal registry pointers or
//  references is forbidden
//  ThreadChecker ensures every operation runs on the owning thread (asserted in Debug builds)
//  World does not depend on the SDL, OpenGL, Chart or Asset modules

#include <functional>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include <entt/entity/registry.hpp>

#include <cuexis/core/result.hpp>
#include <cuexis/core/thread_checker.hpp>
#include <cuexis/world/components.hpp>

namespace cuexis::world {

class World final {
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
    World() = default;
    ~World();

    World(const World&) = delete;
    World& operator=(const World&) = delete;
    World(World&&) = delete;
    World& operator=(World&&) = delete;

    // The callback runs synchronously on the owner thread and is never retained. It must not
    // re-enter this World or retain Registry references beyond the call.
    template <typename Callback>
    [[nodiscard]] auto withRegistry(Callback&& callback)
        -> core::GuardedInvokeResult<Callback&&, entt::registry&> {
        threadChecker_.assertCurrent();

        using ReturnType = std::invoke_result_t<Callback&&, entt::registry&>;
        static_assert(!std::is_reference_v<ReturnType>,
                      "World registry callbacks must not return references");
        static_assert(!std::is_pointer_v<std::remove_cv_t<ReturnType>>,
                      "World registry callbacks must not return pointers");

        if (callbackActive_) {
            return core::unexpected(core::Error{"world.callback.reentrant",
                                                "World registry callback must not be reentrant"});
        }
        CallbackScope callbackScope{callbackActive_};
        return core::invokeGuarded("world.callback.exception",
                                   "World registry callback raised an exception",
                                   std::forward<Callback>(callback), registry_);
    }

    template <typename Callback>
    [[nodiscard]] auto withRegistry(Callback&& callback) const
        -> core::GuardedInvokeResult<Callback&&, const entt::registry&> {
        threadChecker_.assertCurrent();

        using ReturnType = std::invoke_result_t<Callback&&, const entt::registry&>;
        static_assert(!std::is_reference_v<ReturnType>,
                      "World registry callbacks must not return references");
        static_assert(!std::is_pointer_v<std::remove_cv_t<ReturnType>>,
                      "World registry callbacks must not return pointers");

        if (callbackActive_) {
            return core::unexpected(core::Error{"world.callback.reentrant",
                                                "World registry callback must not be reentrant"});
        }
        CallbackScope callbackScope{callbackActive_};
        return core::invokeGuarded("world.callback.exception",
                                   "World registry callback raised an exception",
                                   std::forward<Callback>(callback), registry_);
    }

  private:
    struct TransformCacheEntry final {
        entt::entity entity{entt::null};
        entt::entity parent{entt::null};
        std::optional<std::size_t> parentIndex;
        TransformComponent local{};
        core::Mat4 localMatrix{};
        core::Mat4 world{};
        std::vector<std::size_t> children;
    };

    friend auto updateWorldTransforms(World& world) -> core::Result<void>;

    entt::registry registry_{};
    std::vector<TransformCacheEntry> transformCache_;
    std::vector<std::size_t> transformOrder_;
    std::vector<core::Mat4> transformLocalScratch_;
    std::vector<core::Mat4> transformScratch_;
    std::vector<bool> transformLocalDirty_;
    std::vector<bool> transformDirty_;
    bool transformCacheValid_{false};
    mutable bool callbackActive_{false};
    core::ThreadChecker threadChecker_{};
};

} // namespace cuexis::world

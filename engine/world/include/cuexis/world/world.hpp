#pragma once

//  World — EnTT Registry 的线程安全封装
//  通过 withRegistry() 回调访问，禁止返回 Registry 内部指针或引用
//  ThreadChecker 确保所有操作在所属线程执行（Debug 构建断言）
//  World 不依赖 SDL、OpenGL、Chart 或 Asset 模块

#include <functional>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include <entt/entity/registry.hpp>

#include <cuexis/core/thread_checker.hpp>
#include <cuexis/world/components.hpp>

namespace cuexis::world {

class World final {
  public:
    World() = default;
    ~World();

    World(const World&) = delete;
    World& operator=(const World&) = delete;
    World(World&&) = delete;
    World& operator=(World&&) = delete;

    template <typename Callback> decltype(auto) withRegistry(Callback&& callback) {
        threadChecker_.assertCurrent();

        using ReturnType = std::invoke_result_t<Callback&&, entt::registry&>;
        static_assert(!std::is_reference_v<ReturnType>,
                      "World registry callbacks must not return references");
        static_assert(!std::is_pointer_v<std::remove_cv_t<ReturnType>>,
                      "World registry callbacks must not return pointers");

        return std::invoke(std::forward<Callback>(callback), registry_);
    }

    template <typename Callback> decltype(auto) withRegistry(Callback&& callback) const {
        threadChecker_.assertCurrent();

        using ReturnType = std::invoke_result_t<Callback&&, const entt::registry&>;
        static_assert(!std::is_reference_v<ReturnType>,
                      "World registry callbacks must not return references");
        static_assert(!std::is_pointer_v<std::remove_cv_t<ReturnType>>,
                      "World registry callbacks must not return pointers");

        return std::invoke(std::forward<Callback>(callback), registry_);
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
    core::ThreadChecker threadChecker_{};
};

} // namespace cuexis::world

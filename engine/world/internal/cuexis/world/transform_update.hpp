#pragma once

// Noninstalled owner-thread write plan. A prepared plan excludes public registry callbacks.
#include <cuexis/world/world.hpp>
#include <entt/entity/storage.hpp>
#include <span>

namespace cuexis::world::detail {
struct TransformWrite final {
    entt::entity entity;
    TransformComponent value;
};

class TransformUpdate final {
  public:
    TransformUpdate() = default;
    TransformUpdate(const TransformUpdate&) = delete;
    auto operator=(const TransformUpdate&) -> TransformUpdate& = delete;
    ~TransformUpdate() {
        cancel();
    }

    auto prepare(World&, std::span<const TransformWrite> = {}, bool legacySignals = false)
        -> core::Result<void>;
    // Called only after all Runtime preparation, in the same uninterrupted owner operation.
    void apply() noexcept;
    void finish() noexcept;
    void cancel() noexcept;
    // Standalone TransformSystem retains its existing signal-publishing behavior. Runtime
    // uses apply(), which cannot invoke those callbacks or perform component insertion.
    auto applyLegacy() -> core::Result<void>;

    template <typename Component> auto resolve(entt::entity entity) noexcept -> Component* {
        return owner_ && owner_->registry_.valid(entity)
                   ? owner_->registry_.try_get<Component>(entity)
                   : nullptr;
    }

  private:
    auto prepareLocked(std::span<const TransformWrite>) -> core::Result<void>;
    void installCache() noexcept;
    struct LocalWrite final {
        TransformComponent* slot;
        TransformComponent value;
    };
    struct MatrixWrite final {
        entt::entity entity;
        WorldTransformComponent* slot;
        core::Mat4 value;
        std::size_t cacheIndex;
        std::optional<TransformComponent> local;
        core::Mat4 localMatrix;
    };
    World* owner_{};
    entt::storage<WorldTransformComponent>* activeStorage_{};
    bool coldCache_{}, storageReplacement_{}, prepared_{}, legacySignals_{};
    std::vector<World::TransformCacheEntry> cache_;
    std::vector<std::size_t> order_;
    std::vector<core::Mat4> localScratch_, scratch_;
    std::vector<bool> localDirty_, dirty_;
    std::vector<LocalWrite> localWrites_;
    std::vector<MatrixWrite> matrixWrites_;
    std::vector<entt::entity> stale_;
    std::optional<entt::storage<WorldTransformComponent>> storage_;
};
} // namespace cuexis::world::detail

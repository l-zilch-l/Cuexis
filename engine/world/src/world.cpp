//  World 实现 — EnTT Registry 线程安全封装
//  析构时断言当前线程为创建线程（Debug 构建），确保 EnTT 在正确线程清理

#include <cuexis/world/transform_update.hpp>
#include <cuexis/world/world.hpp>
#include <cuexis_internal/semantic_memory.hpp>

namespace cuexis::world {

World::World() {
    internal::pd::memory::semanticPhase(this, internal::pd::memory::SemanticPhase::constructing);
    // The empty derived pool is part of a World from birth. Transactions never insert a
    // visible placeholder or allocate a registry pool after installation starts.
    try {
        const internal::pd::memory::SemanticOwnerScope memoryOwner{this};
        static_cast<void>(registry_.storage<WorldTransformComponent>());
    } catch (...) {
        internal::pd::memory::semanticPhase(this, internal::pd::memory::SemanticPhase::retiring);
        throw;
    }
}

World::~World() {
    threadChecker_.assertCurrent();
    // Members release after this body. The observer retires the root only at its last free.
    internal::pd::memory::semanticPhase(this, internal::pd::memory::SemanticPhase::retiring);
}

} // namespace cuexis::world

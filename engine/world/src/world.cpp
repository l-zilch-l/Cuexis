//  World 实现 — EnTT Registry 线程安全封装
//  析构时断言当前线程为创建线程（Debug 构建），确保 EnTT 在正确线程清理

#include <cuexis/world/transform_update.hpp>
#include <cuexis/world/world.hpp>

namespace cuexis::world {

World::World() {
    // The empty derived pool is part of a World from birth. Transactions never insert a
    // visible placeholder or allocate a registry pool after installation starts.
    static_cast<void>(registry_.storage<WorldTransformComponent>());
}

World::~World() {
    threadChecker_.assertCurrent();
}

} // namespace cuexis::world

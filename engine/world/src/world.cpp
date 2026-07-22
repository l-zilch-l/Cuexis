//  World 实现 — EnTT Registry 线程安全封装
//  析构时断言当前线程为创建线程（Debug 构建），确保 EnTT 在正确线程清理

#include <cuexis/world/world.hpp>

namespace cuexis::world {

World::~World() {
    threadChecker_.assertCurrent();
}

} // namespace cuexis::world

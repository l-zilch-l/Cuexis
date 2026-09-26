#pragma once

//  Cuexis Player 组合根。准备、设备、窗口和帧循环分在相邻源文件中。
//  run() 持有对象寿命，并按关闭 renderer、音频、clip、PlaybackSession 的顺序退出。

#include <cuexis/core/result.hpp>

namespace cuexis::player {

class PlayerLogger;

[[nodiscard]] auto run(int argumentCount, char** arguments, PlayerLogger& logger)
    -> core::Result<void>;

} // namespace cuexis::player

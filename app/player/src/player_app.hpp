#pragma once

//  Cuexis Player 应用入口 — 解析命令行参数，预检 Project/Asset/Chart，再初始化 SDL/OpenGL
//  run() 只通过 PlaybackSession 驱动更新与帧提取，然后把 Snapshot 适配到渲染后端
//  支持 --smoke-test（渲染三帧后退出）、--chart（谱面文件路径，与 --project
//  互斥）、--project（项目路径）

#include <cuexis/core/result.hpp>

namespace cuexis::player {

[[nodiscard]] auto run(int argumentCount, char** arguments) -> core::Result<void>;

} // namespace cuexis::player

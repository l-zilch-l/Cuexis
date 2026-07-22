#pragma once

//  log — spdlog 封装，提供结构化 category、severity 和稳定 error code 的日志
//  实时线程（Audio/Render）只更新预分配计数器或投递轻量事件，由非实时线程格式化
//  异步日志不得进入音频实时路径中

#include <cuexis/core/result.hpp>

#include <string_view>

namespace cuexis::core::log {

[[nodiscard]] Result<void> init(std::string_view applicationName = "Cuexis") noexcept;
void shutdown() noexcept;

void info(std::string_view category, std::string_view message) noexcept;
void warn(std::string_view category, std::string_view message) noexcept;
void error(std::string_view category, std::string_view message) noexcept;

} // namespace cuexis::core::log

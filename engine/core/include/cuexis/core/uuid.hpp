#pragma once

//  UUID 工具函数 — 置于 Core 使格式前端无需依赖 Chart 模块
//  Chart 方案的 UUIDv7 原生创建、UUIDv5 确定性导入均由 Core 提供基础校验和生成

#include <cuexis/core/result.hpp>

#include <string>
#include <string_view>

namespace cuexis::core {

[[nodiscard]] auto isUuidV7(std::string_view text) noexcept -> bool;
[[nodiscard]] auto isUuidV5(std::string_view text) noexcept -> bool;

// RFC 4122 UUIDv5。SHA-1 仅用于标准化命名标识，不用于安全用途
[[nodiscard]] auto uuidV5(std::string_view namespaceUuid, std::string_view name)
    -> Result<std::string>;

} // namespace cuexis::core

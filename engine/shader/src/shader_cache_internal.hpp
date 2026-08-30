#pragma once

#include <cuexis/shader/shader_cache.hpp>

namespace cuexis::shader::detail {

[[nodiscard]] auto validateCacheKeyInput(const ShaderCacheKeyInput& input) -> core::Result<void>;

[[nodiscard]] auto validateCacheRecord(const ShaderCacheRecord& record) -> core::Result<void>;

[[nodiscard]] auto validateCacheRequest(const ShaderCacheKeyInput& key,
                                        const ShaderCompileRequest& request) -> core::Result<void>;

} // namespace cuexis::shader::detail

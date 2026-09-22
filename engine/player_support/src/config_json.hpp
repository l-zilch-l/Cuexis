#pragma once

#include <cuexis/core/error.hpp>
#include <cuexis/core/result.hpp>
#include <cuexis/json/parse.hpp>
#include <cuexis/json/schema.hpp>
#include <cuexis/json/value.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace cuexis::player_support::detail {

inline constexpr json::ParseLimits configParseLimits{64U * 1024U, 8U, 8U * 1024U};

[[nodiscard]] auto readTextFile(const std::filesystem::path& path) -> core::Result<std::string>;

[[nodiscard]] auto loadSchema(const std::filesystem::path& schemaPath) -> core::Result<json::Value>;

[[nodiscard]] auto readInteger(const json::Value& value) noexcept -> std::optional<std::int64_t>;

[[nodiscard]] auto objectField(const json::Value::Object& object, std::string_view key) noexcept
    -> const json::Value*;

} // namespace cuexis::player_support::detail

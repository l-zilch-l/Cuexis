#pragma once

#include <cuexis/core/result.hpp>

#include <cstddef>
#include <filesystem>
#include <span>

namespace cuexis::chart::packed::file_detail {
[[nodiscard]] auto writeAtomic(std::span<const std::byte> bytes,
                               const std::filesystem::path& target) -> core::Result<void>;
}

#pragma once

#include <cstddef>
#include <optional>
#include <set>
#include <string>
#include <string_view>

namespace cuexis::cxc::detail {

[[nodiscard]] auto foldAscii(std::string_view value) -> std::string;
[[nodiscard]] auto insertUniqueArchivePath(std::set<std::string, std::less<>>& foldedPaths,
                                           std::string_view path) -> bool;
[[nodiscard]] auto isPortablePath(std::string_view path, std::size_t maxBytes = 4096,
                                  std::size_t maxDepth = 64) -> bool;
[[nodiscard]] auto joinPortablePath(std::string_view base, std::string_view relative,
                                    std::size_t maxBytes = 4096, std::size_t maxDepth = 64)
    -> std::optional<std::string>;

} // namespace cuexis::cxc::detail

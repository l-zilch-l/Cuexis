#pragma once

//  AssetId - serializable, stable resource identifier
//  Owned by AssetDatabase; callers request resources by this ID and must never treat
//  a file path as an AssetId

#include <compare>
#include <string>

namespace cuexis::assets {

struct AssetId final {
    std::string value;

    [[nodiscard]] bool empty() const noexcept {
        return value.empty();
    }

    auto operator<=>(const AssetId&) const = default;
};

} // namespace cuexis::assets

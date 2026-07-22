#pragma once

//  AssetId — 可序列化的稳定资源标识符
//  由 AssetDatabase 管理，业务层使用此 ID 请求资源；不得把文件路径当作 AssetId

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

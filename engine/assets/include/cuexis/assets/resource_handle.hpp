#pragma once

//  ResourceHandle<Tag> — 类型化弱句柄，不拥有资源
//  由 index + generation + managerToken 标识：
//    index: 槽位索引（槽位复用后 generation 递增防止 ABA 问题）
//    managerToken: 进程内 ResourceManager 身份，防止跨 Manager 别名
//  Handle 不序列化到文档/缓存；Component 只保存 Handle，不保存 Lease 或裸指针

#include <compare>
#include <cstdint>
#include <limits>

namespace cuexis::assets {

template <typename Tag> struct ResourceHandle final {
    static constexpr std::uint32_t invalidIndex = std::numeric_limits<std::uint32_t>::max();

    std::uint32_t index{invalidIndex};
    std::uint32_t generation{};
    // Process-local identity of the ResourceManager that owns this handle.  It is
    // intentionally not serialized and is checked by ResourceManager::get().
    std::uint64_t managerToken{};

    [[nodiscard]] constexpr bool valid() const noexcept {
        return index != invalidIndex && generation != 0;
    }

    [[nodiscard]] constexpr bool belongsTo(std::uint64_t token) const noexcept {
        return valid() && managerToken != 0 && managerToken == token;
    }

    auto operator<=>(const ResourceHandle&) const = default;
};

struct MeshTag final {};
struct MaterialTag final {};
struct TextureTag final {};

using MeshHandle = ResourceHandle<MeshTag>;
using MaterialHandle = ResourceHandle<MaterialTag>;
using TextureHandle = ResourceHandle<TextureTag>;

} // namespace cuexis::assets

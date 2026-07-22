#pragma once

//  IContentProvider — 内容源抽象接口
//  宿主编译提供字节来源（文件系统、VFS、归档或内存）
//  SDK 内部根据已校验的 AssetId 和逻辑来源请求有界字节，不自行打开任意路径
//  阶段 1B 的 AssetDatabase 默认使用内建 Filesystem 回退路径，阶段 1E 完成注入式改造

#include <cuexis/core/result.hpp>

#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::playback {

struct BlobLimits final {
    std::size_t maxBytes{64 * 1024 * 1024};
};

struct ContentBlob final {
    std::vector<std::byte> bytes;

    [[nodiscard]] auto span() const noexcept -> std::span<const std::byte> {
        return std::span{bytes.data(), bytes.size()};
    }
};

class IContentProvider {
  public:
    virtual ~IContentProvider() = default;

    IContentProvider(const IContentProvider&) = delete;
    auto operator=(const IContentProvider&) -> IContentProvider& = delete;
    IContentProvider(IContentProvider&&) = delete;
    auto operator=(IContentProvider&&) -> IContentProvider& = delete;

    [[nodiscard]] virtual auto readBlob(std::string_view source, const BlobLimits& limits = {})
        -> core::Result<ContentBlob> = 0;

  protected:
    IContentProvider() = default;
};

} // namespace cuexis::playback
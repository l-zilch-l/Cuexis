#include "cxc_hash_internal.hpp"

#include <minizip-ng/mz_crypt.h>

#include <algorithm>
#include <limits>

namespace cuexis::cxc::detail {
namespace {

template <typename Callback>
[[nodiscard]] auto forChunks(std::span<const std::byte> bytes, Callback&& callback) noexcept
    -> bool {
    constexpr auto maxChunk = static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max());
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const auto count = std::min(bytes.size() - offset, maxChunk);
        if (!callback(bytes.data() + offset, static_cast<std::int32_t>(count))) {
            return false;
        }
        offset += count;
    }
    return true;
}

} // namespace

auto crc32(std::span<const std::byte> bytes) noexcept -> std::uint32_t {
    std::uint32_t result = 0;
    static_cast<void>(forChunks(bytes, [&](const std::byte* data, std::int32_t count) {
        result = mz_crypt_crc32_update(result, reinterpret_cast<const std::uint8_t*>(data), count);
        return true;
    }));
    return result;
}

} // namespace cuexis::cxc::detail

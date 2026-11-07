#pragma once

// Isolated Stage A diagnostic support; never included by a production target.
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>

namespace admission_audit {
inline auto add(std::uint64_t left, std::uint64_t right) -> std::uint64_t {
    if (right > UINT64_MAX - left)
        throw std::overflow_error{"audit counter overflow"};
    return left + right;
}
inline auto multiply(std::uint64_t left, std::uint64_t right) -> std::uint64_t {
    if (left && right > UINT64_MAX / left)
        throw std::overflow_error{"audit product overflow"};
    return left * right;
}
inline auto leb(std::uint64_t value) -> std::uint64_t {
    std::uint64_t size = 1;
    while (value >= 128) {
        value >>= 7;
        ++size;
    }
    return size;
}
struct Expansion final {
    std::uint64_t calls{}, bytes{}, maximum{};
};
struct Counters final {
    std::map<std::string, Expansion> expansions;
    std::uint64_t projectionCalls{}, projectionBytes{}, maximumProjection{};
    void expanded(std::string_view kind, std::uint64_t bytes) {
        auto& row = expansions[std::string{kind}];
        row.calls = add(row.calls, 1);
        row.bytes = add(row.bytes, bytes);
        row.maximum = std::max(row.maximum, bytes);
    }
    void projection(std::string_view key) {
        if (!key.starts_with("cxgp2:"))
            return;
        projectionCalls = add(projectionCalls, 1);
        projectionBytes = add(projectionBytes, key.size());
        maximumProjection = std::max(maximumProjection, std::uint64_t{key.size()});
    }
};
} // namespace admission_audit

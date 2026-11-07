#pragma once

// Aliases, budget helpers, UTF-8 checks and the shared emission mode.

#include <cuexis/gameplay_packed/gameplay_capsule.hpp>

#include "claim_key_internal.hpp"
#include "hash_frames_internal.hpp"
#include "packed_file_internal.hpp"
#include "packed_gameplay_internal.hpp"
#include "packed_identity_internal.hpp"
#include "packed_limits_internal.hpp"
#include "semantic_stream_internal.hpp"

#include <cuexis/chart/packed_chart_primitives.hpp>
#include <cuexis/core/error.hpp>
#include <cuexis/filesystem/secure_file.hpp>
#include <cuexis/json/parse.hpp>
#include <cuexis_internal/pmr_buffer.hpp>
#include <cuexis_internal/presentation_metrics.hpp>
#include <cuexis_internal/sha256.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <exception>
#include <limits>
#include <map>
#include <set>
#include <tuple>
#include <type_traits>

namespace cuexis::gameplay_packed {
namespace capsule_detail {

using namespace judgement;
using chart::packed::ByteReader;
using chart::packed::ByteWriter;
using chart::packed::crc32;
namespace gp = chart::packed::gameplay_detail;
using Bytes = std::vector<std::byte>;
using Ref = gp::Reference;
using detail::ClaimRow;
using detail::RequirementIndices;
constexpr std::string_view kResolver = "grace.resolver.s2.v1";
constexpr std::string_view kClose = "local.close.t4.v1";
inline auto executionRevision(std::uint32_t revision) -> bool {
    return revision == 3 || revision == 4;
}

struct Failure final {
    core::Error error;
};
[[noreturn]] inline void fail(std::string_view code, std::string_view message,
                              std::string_view path = "gameplay") {
    throw Failure{core::Error{std::string{code}, std::string{message}}.withContext(
        "fieldPath", std::string{path})};
}
inline void require(bool condition, std::string_view path, std::string_view message) {
    if (!condition) {
        fail("packed.requirements.invalid", message, path);
    }
}
template <typename T> auto checked(core::Result<T> result) -> T {
    if (!result) {
        throw Failure{std::move(result.error())};
    }
    return std::move(*result);
}
inline void checked(core::Result<void> result) {
    if (!result) {
        throw Failure{std::move(result.error())};
    }
}
inline auto u32(std::size_t value) -> std::uint32_t {
    if (value > std::numeric_limits<std::uint32_t>::max()) {
        fail("packed.field.wire_range", "counter is outside the u32 domain");
    }
    return static_cast<std::uint32_t>(value);
}
inline void bound(std::size_t value, std::size_t limit, std::string_view code) {
    if (value > limit) {
        fail(code, "accepted Packed budget exceeded");
    }
}
inline auto utf8(std::string_view text) -> bool {
    std::uint32_t value = 0, minimum = 0;
    unsigned remaining = 0;
    for (const unsigned char byte : text) {
        if (remaining == 0) {
            if (byte < 0x80) {
                continue;
            }
            if (byte >= 0xc2 && byte <= 0xdf) {
                value = byte & 0x1fU;
                minimum = 0x80;
                remaining = 1;
            } else if (byte >= 0xe0 && byte <= 0xef) {
                value = byte & 0x0fU;
                minimum = 0x800;
                remaining = 2;
            } else if (byte >= 0xf0 && byte <= 0xf4) {
                value = byte & 7U;
                minimum = 0x10000;
                remaining = 3;
            } else {
                return false;
            }
        } else {
            if ((byte & 0xc0U) != 0x80U) {
                return false;
            }
            value = (value << 6U) | (byte & 0x3fU);
            if (--remaining == 0 &&
                (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))) {
                return false;
            }
        }
    }
    return remaining == 0;
}
inline auto textLess(std::string_view left, std::string_view right) -> bool {
    return std::lexicographical_compare(
        left.begin(), left.end(), right.begin(), right.end(), [](char l, char r) {
            return static_cast<unsigned char>(l) < static_cast<unsigned char>(r);
        });
}

// Shared visitors collect dictionaries, emit compact wire, or emit the fixed-width preimage.
// Sizing traverses the same fields without allocating output.
enum class Mode { collect, wire, hash };
template <typename A, typename M> auto storesProjection(const A& a, const M& m) -> bool {
    if constexpr (A::reading)
        return m.candidateRevision != 4;
    else
        return m.candidateRevision != 4 || a.mode == Mode::hash;
}

} // namespace capsule_detail
} // namespace cuexis::gameplay_packed

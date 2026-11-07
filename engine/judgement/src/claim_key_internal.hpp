#pragma once

#include "structural_key_codec_internal.hpp"

#include <limits>
#include <stdexcept>

namespace cuexis::judgement::detail {

// Capsule kind 11/subdomain 0: fixed-width, length-framed namespace and emission identity.
inline auto structuralClaimKey(std::string_view explicitNamespace,
                               const RequirementIdentity& identity) -> std::string {
    struct Adapter final {
        std::string bytes;
        void write(std::span<const std::byte> data) {
            if (!data.empty())
                bytes.append(reinterpret_cast<const char*>(data.data()), data.size());
        }
        auto checkedCount(std::size_t value) const -> std::uint32_t {
            if (value > std::numeric_limits<std::uint32_t>::max())
                throw std::length_error{"claim key length exceeds u32"};
            return static_cast<std::uint32_t>(value);
        }
        void validateText(std::string_view) const noexcept {}
    } adapter;
    StructuralKeyEncoder encoder{adapter};
    claimKeyFields(encoder, explicitNamespace, identity);
    return formatStructuralProjectionKey(std::as_bytes(std::span{adapter.bytes}), [] {
        throw std::length_error{"claim key hex length overflow"};
    });
}

} // namespace cuexis::judgement::detail

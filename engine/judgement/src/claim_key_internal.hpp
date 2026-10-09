#pragma once

#include <cuexis/judgement/gameplay_graph.hpp>

#include <limits>
#include <stdexcept>

namespace cuexis::judgement::detail {

// Capsule kind 11/subdomain 0: fixed-width, length-framed namespace and emission identity.
inline auto structuralClaimKey(std::string_view explicitNamespace,
                               const RequirementIdentity& identity) -> std::string {
    std::string bytes(1, '\0');
    const auto number = [&bytes](std::uint64_t value, unsigned width) {
        for (unsigned i = 0; i < width; ++i) {
            bytes.push_back(static_cast<char>((value >> (i * 8U)) & 255U));
        }
    };
    const auto count = [&number](std::size_t value) {
        if (value > std::numeric_limits<std::uint32_t>::max()) {
            throw std::length_error{"claim key length exceeds u32"};
        }
        number(value, 4);
    };
    const auto text = [&bytes, &count](std::string_view value) {
        count(value.size());
        bytes.append(value);
    };
    text(explicitNamespace);
    text(identity.chartEntryId);
    text(identity.invocationId);
    text(identity.moduleId);
    text(identity.exportId);
    count(identity.emissionPath.size());
    for (const auto& step : identity.emissionPath) {
        text(step.nodeId);
        number(step.repeatIndex, 8);
    }
    text(identity.requirementLocalId);
    if (bytes.size() > (std::string{}.max_size() - 6) / 2) {
        throw std::length_error{"claim key hex length overflow"};
    }
    constexpr char digits[] = "0123456789abcdef";
    std::string key{"cxgp2:"};
    key.reserve(6 + bytes.size() * 2);
    for (const unsigned char value : bytes) {
        key.push_back(digits[value >> 4U]);
        key.push_back(digits[value & 15U]);
    }
    return key;
}

} // namespace cuexis::judgement::detail

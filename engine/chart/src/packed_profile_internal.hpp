#pragma once

// Foundation revision 1 registered-subset ("profile") rules for the candidate Packed Chart.
//
// The same validator gates the typed Writer (packed::encode) and the canonical Reader
// (packed::decode), so an out-of-profile model can neither be published nor accepted, and both
// sides report the same packed.profile.* diagnostics. The Spec 10.1 preimage keeps its own
// structural preconditions (identity uniqueness, parent graph, finite floats, Beat domains); a
// profile rule is never enforced only by the preimage.

#include <cuexis/chart/canonical_semantic_chart.hpp>
#include <cuexis/core/result.hpp>

#include <cstdint>
#include <string_view>

namespace cuexis::chart::packed::profile_detail {

// Spec 7.6 registered values.
inline constexpr std::string_view registeredFeatureId{"cuexis.gameplay.candidate.lanes4"};
inline constexpr std::uint32_t registeredFeatureVersion{1};
inline constexpr std::string_view registeredJudgementDomainId{"candidate.lanes4"};
inline constexpr std::string_view registeredActionId{"press"};
inline constexpr std::uint32_t registeredMaxLane{3};

// Rejects every model shape outside the Spec 7.6 Foundation demo profile with a stable
// packed.profile.* error. The gate is evaluated before any artifact bytes or semantic identity
// exist, and after the Reader has rebuilt the typed model but before it compares the identity.
[[nodiscard]] auto validateFoundationProfile(const CanonicalSemanticChart& chart)
    -> core::Result<void>;

} // namespace cuexis::chart::packed::profile_detail

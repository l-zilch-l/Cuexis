#pragma once
#include <cuexis/chart/candidate_lowering.hpp>
#include <string_view>

namespace cuexis::chart {
// Explicit source-json candidate. Does not change the registered static Packed profile.
inline constexpr std::string_view behaviorCandidateProfile = "candidate.behavior-v2-v1";
[[nodiscard]] auto compileBehaviorCandidate(std::string_view source, const ChartLimits& limits = {})
    -> core::Result<CandidateRuntimeArtifact>;
} // namespace cuexis::chart

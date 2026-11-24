#pragma once

// Candidate-only prepare hook entries. The ordinary source table does not include this header, so
// its closure never sees a candidate hook declaration; the experimental source table includes it
// and provides exactly one definition for each entry below.
//
// This header is build-only, not installed and carries no SDK stability promise.
#include "playback_session_prepare_internal.hpp"

#include <optional>
#include <vector>

namespace cuexis::playback {
namespace prepare_detail {

[[nodiscard]] auto candidateResourceRequirements(const chart::CanonicalResourceClosure& closure,
                                                 core::Diagnostics& diagnostics)
    -> std::optional<std::vector<chart::ChartResourceRequirement>>;

[[nodiscard]] auto prepareCandidateStage(const cxc::CandidateChart& candidate, bool gameplaySource,
                                         const PlaybackPrepareOptions& options, bool replacement,
                                         const chart::ChartLimits& limits,
                                         PrepareArtifact& artifact, core::Diagnostics& diagnostics)
    -> core::Result<chart::ChartRuntime>;

} // namespace prepare_detail
} // namespace cuexis::playback

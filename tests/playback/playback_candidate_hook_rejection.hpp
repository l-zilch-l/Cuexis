#pragma once

// Test-only declaration of the ordinary-build prepare-hook rejection. The ordinary Playback
// closure no longer sees or defines a candidate prepare hook; this stub exists only in the
// Playback test executable so the isolation test can call the same-signature rejection directly.
#include "playback_session_prepare_internal.hpp"

namespace cuexis::playback {
namespace prepare_detail {

[[nodiscard]] auto prepareCandidateStage(const cxc::CandidateChart& candidate, bool gameplaySource,
                                         const PlaybackPrepareOptions& options, bool replacement,
                                         const chart::ChartLimits& limits,
                                         PrepareArtifact& artifact, core::Diagnostics& diagnostics)
    -> core::Result<chart::ChartRuntime>;

} // namespace prepare_detail
} // namespace cuexis::playback

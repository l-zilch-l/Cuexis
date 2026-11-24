// Ordinary-build prepare-hook rejection used only by the Playback isolation test. The ordinary
// Playback library no longer compiles or declares any candidate hook; this test-support definition
// keeps the same signature and rejection so the source-table isolation stays observable without
// putting a candidate hook back into the ordinary closure.

#include "playback_candidate_hook_rejection.hpp"

namespace cuexis::playback {
namespace prepare_detail {

[[nodiscard]] auto prepareCandidateStage(const cxc::CandidateChart& candidate, bool gameplaySource,
                                         const PlaybackPrepareOptions& options, bool replacement,
                                         const chart::ChartLimits& limits,
                                         PrepareArtifact& artifact, core::Diagnostics& diagnostics)
    -> core::Result<chart::ChartRuntime> {
    static_cast<void>(candidate);
    static_cast<void>(gameplaySource);
    static_cast<void>(options);
    static_cast<void>(replacement);
    static_cast<void>(limits);
    static_cast<void>(artifact);
    static_cast<void>(diagnostics);
    return core::unexpected(core::Error{"playback.candidate.disabled",
                                        "Chart v5 candidate Playback is disabled in this build"});
}

} // namespace prepare_detail
} // namespace cuexis::playback

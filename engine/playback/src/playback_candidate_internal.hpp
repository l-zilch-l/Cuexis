#pragma once

// Test and internal access to candidate metadata retained by a prepared or active session.
// This header is not installed and does not expose candidate types through the public SDK.
// It belongs to the experimental source table only: the ordinary Playback closure never needs
// the candidate lowering header or the metadata members it reads.

#include <cuexis/playback/playback_session.hpp>
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
#include <cuexis/chart/candidate_lowering.hpp>
#endif

namespace cuexis::playback::detail {

#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
struct CandidateMetadataAccess final {
    [[nodiscard]] static auto prepared(const PreparedPlayback& playback)
        -> const chart::CandidateRuntimeMetadata*;

    [[nodiscard]] static auto active(const PlaybackSession& session)
        -> const chart::CandidateRuntimeMetadata*;
};
#endif

} // namespace cuexis::playback::detail

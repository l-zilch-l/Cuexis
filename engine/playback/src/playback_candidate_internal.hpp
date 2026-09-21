#pragma once

// Test and internal access to candidate metadata retained by a prepared or active session.
// This header is not installed and does not expose candidate types through the public SDK.

#include <cuexis/chart/candidate_lowering.hpp>
#include <cuexis/playback/playback_session.hpp>

namespace cuexis::playback::detail {

struct CandidateMetadataAccess final {
    [[nodiscard]] static auto prepared(const PreparedPlayback& playback)
        -> const chart::CandidateRuntimeMetadata*;

    [[nodiscard]] static auto active(const PlaybackSession& session)
        -> const chart::CandidateRuntimeMetadata*;
};

} // namespace cuexis::playback::detail

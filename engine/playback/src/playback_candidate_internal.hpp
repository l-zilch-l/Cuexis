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
// Candidate-only fields that unload must clear. This mirrors their presence; it does not model
// their lifetime or control semantics.
struct CandidateSessionState final {
    bool hostFence{};
    bool acceptedHostControl{};
    bool confirmedHostContext{};
    bool executionMapping{};
    bool tailTransition{};
};

// Internal test seam: exposes the candidate fields so the unload regression can seed and then
// observe exactly the fields cleared by unload. Not part of the candidate SDK surface.
struct CandidateSessionSlots final {
    std::optional<ExecutionFence>& hostFence;
    std::optional<ControlRequest>& acceptedHostControl;
    std::optional<HostContext>& confirmedHostContext;
    std::optional<ClockMappingConfiguration>& executionMapping;
    std::optional<TailClockTransition>& tailTransition;
    bool& hostCommitAuthorized;
};

struct CandidateMetadataAccess final {
    [[nodiscard]] static auto prepared(const PreparedPlayback& playback)
        -> const chart::CandidateRuntimeMetadata*;

    [[nodiscard]] static auto active(const PlaybackSession& session)
        -> const chart::CandidateRuntimeMetadata*;

    [[nodiscard]] static auto candidateState(const PlaybackSession& session)
        -> CandidateSessionState;

    [[nodiscard]] static auto candidateSlots(PlaybackSession& session) -> CandidateSessionSlots;

    [[nodiscard]] static auto preparedOwnerDereferences(const PreparedPlayback& playback) noexcept
        -> std::uint64_t;

    // Internal test seam for the F509 lifetime guard: expiring the lease models a session that
    // is already gone without dereferencing it.
    static void expirePreparedOwnerLease(PreparedPlayback& playback) noexcept;
};
#endif

} // namespace cuexis::playback::detail

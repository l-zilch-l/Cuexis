#pragma once

// Shared declarations for the session worker units: the owner state and its member
// interface stay here so the dispatch table and the frame path do not repeat them.

#include "session_worker.hpp"

#include <algorithm>
#include <chrono>
#include <cuexis_internal/execution_diagnostics.hpp>
#include <limits>
#include <type_traits>

namespace cuexis::playback::detail {

#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
namespace rpa = cuexis::internal::execution_diagnostics::rpa;
inline auto fenceIdentity(const ExecutionFence& fence) -> rpa::Identity {
    rpa::Identity identity{};
    identity.valid = true;
    identity.scope = fence.scope();
    identity.generation = fence.generation();
    identity.candidateId = fence.candidateId();
    identity.controlSequence = fence.controlSequence();
    return identity;
}
inline auto proofIdentity(const ControlReceipt& proof) -> rpa::Identity {
    rpa::Identity identity{};
    identity.valid = true;
    identity.scope = proof.request.context.sessionScope;
    identity.generation = proof.request.target.expectedGeneration;
    identity.candidateId = proof.request.target.candidateId;
    identity.controlSequence = proof.request.context.controlSequence;
    identity.operationId = proof.request.operationId;
    identity.decisionOrdinal = proof.decisionOrdinal;
    identity.kind = static_cast<std::uint8_t>(proof.request.kind);
    identity.status = static_cast<std::uint8_t>(proof.status);
    identity.clockSegment = proof.request.context.clockSegment;
    identity.runEpoch = proof.request.context.runEpoch;
    identity.resourceGeneration = proof.request.context.resourceGeneration;
    identity.rendererGeneration = proof.request.context.rendererGeneration;
    identity.contentIdentity = proof.request.context.contentIdentity.sha256;
    return identity;
}
#endif

namespace worker_detail {

inline auto workerError(std::string_view code, std::string_view message) -> core::Error {
    return {"host.rpa." + std::string{code}, std::string{message}};
}
template <typename T> auto reply(core::Result<T>&& result) -> core::Result<SessionReply> {
    if (!result)
        return core::unexpected(std::move(result.error()));
    if constexpr (std::is_void_v<T>)
        return SessionReply{std::monostate{}};
    else
        return SessionReply{std::move(*result)};
}
inline auto monotonicNow() -> std::int64_t {
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

} // namespace worker_detail

struct CoherentAdvanceRecord final {
    SessionCoherentAdvance request;
};

struct SessionOwner final {
    PlaybackSession session;
    std::optional<PreparedPlayback> prepared;
    std::optional<PresentationCandidateInput> candidate;
    std::optional<PresentationCandidateInput> activeCandidate;
    std::optional<OwnerStepLedger> software;
    std::optional<FrameObservationLedger> frames;
    std::optional<HostContext> frameContext;
    std::uint64_t projectionScope{}, lastFrameId{};
    std::optional<std::int64_t> lastPresentationTick;
    std::optional<SessionRealtimeStart> realtime;
    std::vector<std::optional<CoherentAdvanceRecord>> coherent;
    std::optional<GameplayExecutionContext> coherentIdentity;
    SessionWorkerConfiguration& configuration;
    std::shared_ptr<FrameDelivery> delivery;
    std::optional<PresentationSourceInfo> currentSource;
    std::vector<FeedbackId> currentMembership;
    std::uint64_t membershipScope{};
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    rpa::Fence rpaFence{};
    rpa::OwnerCloseObservation rpaClose{};
    rpa::SeekLedger rpaSeek{};
#endif
    enum class FramePath { unselected, legacy, requested };
    FramePath framePath{FramePath::unselected};

    SessionOwner(SessionWorkerConfiguration& config, std::shared_ptr<FrameDelivery> frameDelivery);
    [[nodiscard]] auto bindFrames(const HostContext& context, std::uint64_t scope)
        -> core::Result<void>;
    [[nodiscard]] auto updatePresentation(const SessionUpdatePresentation& job)
        -> core::Result<void>;
    [[nodiscard]] auto refreshMembership(const PresentationSourceInfo& source, std::uint64_t scope)
        -> core::Result<void>;
    [[nodiscard]] auto layout() -> core::Result<FrameDeliveryLayout>;
    [[nodiscard]] auto requestFrame(const FrameRequest& request) -> core::Result<DeliveryInfo>;
    void reclaimCoherent(const AdvanceRetentionProgress& progress) noexcept;
    [[nodiscard]] auto ledger(std::uint64_t scope) -> core::Result<void>;
    [[nodiscard]] auto fence(const ControlReceipt& proof
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
                             ,
                             rpa::Fence* destination = nullptr
#endif
                             ) -> core::Result<CommitFence>;
    [[nodiscard]] auto simpleStep(const StepRequest& request, bool retired = false)
        -> core::Result<StepReceipt>;
    [[nodiscard]] auto dispatch(SessionCommand&& command) -> core::Result<SessionReply>;
    [[nodiscard]] auto now() -> std::int64_t;
};

} // namespace cuexis::playback::detail

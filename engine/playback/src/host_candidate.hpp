#pragma once

#include "host_protocol.hpp"

namespace cuexis::playback::detail {
struct FeedbackObservation;
struct AudioCandidateInput final {
    std::string assetId;
    double timingOffsetMs{};
    std::uint64_t contentRevision{};
    std::vector<std::byte> encoded;
};
struct PresentationCandidateInput final {
    HostContext context;
    std::uint64_t mutationGeneration{}, candidateId{}, rendererInstance{};
    PresentationCandidateToken token;
    PresentationCapabilities capabilities;
    PresentationResourceManifest manifest;
    EffectivePresentationSettings settings;
    std::vector<PortableResourcePtr> resources;
    std::optional<AudioCandidateInput> audio;
    std::uint64_t playbackOwnerToken{};
};
struct CommitFence final {
    std::uint64_t scope{}, generation{}, candidateId{}, controlSequence{};
    friend auto operator==(CommitFence, CommitFence) -> bool = default;
};
// Prepared stays on its owner; only CPU-owning data crosses this non-installed bridge.
struct CUEXIS_PLAYBACK_API HostCoordinationAccess final {
    [[nodiscard]] static auto
    materialize(PreparedPlayback&, const PresentationCapabilities&, const PresentationRequest&,
                std::uint64_t rendererInstance, std::uint64_t rendererGeneration,
                std::uint64_t controlSequence) -> core::Result<PresentationCandidateInput>;
    [[nodiscard]] static auto enterControlFence(PlaybackSession&, const ControlReceipt&)
        -> core::Result<CommitFence>;
    [[nodiscard]] static auto currentFence(const PlaybackSession&) -> core::Result<CommitFence>;
    [[nodiscard]] static auto enterFence(PlaybackSession&, const PresentationCandidateInput&)
        -> core::Result<CommitFence>;
    [[nodiscard]] static auto commit(PlaybackSession&, PreparedPlayback&&, CommitFence,
                                     const ControlReceipt&) -> core::Result<void>;
    [[nodiscard]] static auto commitOnce(PlaybackSession&, OwnerStepLedger&, const StepRequest&,
                                         PreparedPlayback&&, CommitFence, const ControlReceipt&)
        -> core::Result<StepReceipt>;
    [[nodiscard]] static auto retireSoftware(PlaybackSession&, OwnerStepLedger&, const StepRequest&,
                                             const ControlReceipt&) -> core::Result<StepReceipt>;
    [[nodiscard]] static auto completeClose(PlaybackSession&, CommitFence, const ControlReceipt&)
        -> core::Result<void>;
    [[nodiscard]] static auto acceptControl(PlaybackSession&, HostCoordinator&,
                                            const ControlRequest&) -> core::Result<ControlReceipt>;
    [[nodiscard]] static auto acceptControlAtOwner(PlaybackSession&, const ControlRequest&)
        -> core::Result<std::uint64_t>;
    [[nodiscard]] static auto leaveFence(PlaybackSession&, CommitFence, const ControlReceipt&)
        -> core::Result<void>;
    [[nodiscard]] static auto generation(const PlaybackSession&) -> core::Result<std::uint64_t>;
    [[nodiscard]] static auto sessionScope(const PlaybackSession&) -> core::Result<std::uint64_t>;
    [[nodiscard]] static auto validateFrameContext(const PlaybackSession&, const HostContext&)
        -> core::Result<void>;
    [[nodiscard]] static auto validateControlBinding(const PlaybackSession&, const StepRequest&)
        -> core::Result<void>;
    [[nodiscard]] static auto currentRuntimeFrame(const PlaybackSession&)
        -> core::Result<RuntimeFrame>;
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    [[nodiscard]] static auto mapRealtimeClock(PlaybackSession&, const ClockObservation&,
                                               std::int64_t now) -> core::Result<ClockTicks>;
    [[nodiscard]] static auto revokeRealtimePermission(PlaybackSession&) -> core::Result<void>;
    [[nodiscard]] static auto archiveCut(const PlaybackSession&, const GameplayReplay&,
                                         GameplayTick) -> core::Result<GameplayReplayCut>;
    [[nodiscard]] static auto reconciliationOrdinal(const PlaybackSession&)
        -> core::Result<std::uint64_t>;
    [[nodiscard]] static auto feedbackMembership(const PlaybackSession&, std::uint64_t scope)
        -> core::Result<std::vector<FeedbackObservation>>;
    [[nodiscard]] static auto samplePresentation(PlaybackSession&, GameplayPresentationTick,
                                                 const RuntimeFrame&) -> core::Result<void>;
    [[nodiscard]] static auto
    applySeek(PlaybackSession&, OwnerStepLedger&, const StepRequest&, const ControlReceipt&,
              const GameplayReplay&, GameplayTick, const GameplayReplayCut&,
              std::span<const GameplayCheckpoint>, GameplayPresentationTick, const RuntimeFrame&,
              SchedulingProfile, ClockMappingConfiguration, const std::string&)
        -> core::Result<StepReceipt>;
    [[nodiscard]] static auto resumeIdentity(const PlaybackSession&, const GameplayReconciliation&)
        -> core::Result<PreparedSemanticIdentity>;
    [[nodiscard]] static auto applyPause(PlaybackSession&, OwnerStepLedger&, const StepRequest&,
                                         std::int64_t now = 0) -> core::Result<StepReceipt>;
    [[nodiscard]] static auto applyResume(PlaybackSession&, OwnerStepLedger&, const StepRequest&,
                                          const GameplayReconciliation&, GameplayExecutionContext,
                                          const ClockObservation&, std::int64_t now,
                                          const StepReceipt* routeReceipt)
        -> core::Result<StepReceipt>;
#endif
};
} // namespace cuexis::playback::detail

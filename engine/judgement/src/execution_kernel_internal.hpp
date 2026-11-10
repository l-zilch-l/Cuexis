#pragma once

// Shared declarations for the execution kernel units. The mutable run state, the immutable
// preparation, and the private ExecutionKernel::Impl declaration live here so that no unit
// repeats them.

#include "diagnostic_policy.hpp"
#include "execution_kernel.hpp"
#include "execution_log.hpp"
#include "execution_testing.hpp"
#include "identity_factory.hpp"
#include <cuexis/judgement/recovery_codec.hpp>
#include <cuexis_internal/execution_diagnostics.hpp>

#include <algorithm>
#include <bit>
#include <map>
#include <set>
#include <stdexcept>
#include <tuple>

namespace cuexis::judgement::detail {

namespace kernel_detail {

inline auto error(std::string_view suffix, std::string_view message, bool faulted = false)
    -> core::Error {
    return core::Error{"judgement." + std::string{suffix}, std::string{message}}
        .withContext("category", suffix == "execution.profile_incomplete"
                                     ? "identity_closure_incomplete"
                                     : "invalid_relation")
        .withContext("severity", "error")
        .withContext("faulted", faulted ? "true" : "false");
}
inline auto sameTimebase(const TimebaseProfile& a, const TimebaseProfile& b) -> bool {
    return a.profileId == b.profileId && a.unitToken == b.unitToken && a.tickScale == b.tickScale &&
           a.originBeat == b.originBeat && a.initialTempo == b.initialTempo &&
           a.tempoSections == b.tempoSections && a.stopSections == b.stopSections;
}
inline auto keyOf(const NormalizedObservation& v) -> CanonicalObservationKey {
    return {v.observationTick,
            std::string{v.domainToken},
            std::string{v.sourceClass.token()},
            std::string{v.channel.token()},
            v.action,
            v.amount ? std::optional{v.amount->value.canonicalInteger} : std::nullopt};
}
inline auto sameChannel(const ContactHandle& contact, const CanonicalObservationKey& key) -> bool {
    return contact.domainToken == key.domainToken && contact.sourceClass == key.sourceClass &&
           contact.channelToken == key.channelToken;
}
inline auto matches(const AtomBinding& binding, const CanonicalObservationKey& key) -> bool {
    return binding.domainToken == key.domainToken && binding.sourceClass == key.sourceClass &&
           binding.channelToken == key.channelToken && binding.action == key.action &&
           (!binding.amountRange || (key.amount && *key.amount >= binding.amountRange->minimum &&
                                     *key.amount <= binding.amountRange->maximum));
}
inline auto observer(const RequirementRecord& r) -> bool {
    return !r.resourceClaims.empty() &&
           r.resourceClaims.front().intent == ResourceClaimIntent::observe;
}
inline auto rootPhase(const RequirementRecord& r) -> PhaseKind {
    return r.phases.front().kind == PhaseKind::tap ? PhaseKind::tap : PhaseKind::head;
}

#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
namespace rpaObservation = cuexis::internal::execution_diagnostics::rpa;
using SeekWork = rpaObservation::SeekWork;
using SeekPhase = rpaObservation::SeekPhase;
#endif

} // namespace kernel_detail

struct MatcherState final {
    std::vector<std::size_t> states;
    bool epsilonAttempted{false};
};

struct RuntimeState final : ProjectionStorage {
    RuntimeState(KernelProjection projection, SharedSequence<MatcherState> matchers)
        : ProjectionStorage(std::move(projection)), matchers(std::move(matchers)) {}
    SharedSequence<MatcherState> matchers;
    std::vector<HeadContactOwnership> coverageHistory;
    std::vector<std::size_t> activeRequirements;
    std::vector<Tick> signalVisibility;
    std::size_t timerCursor{0};
    std::uint64_t nextCommitId{0};
    bool idsExhausted{false};
};

struct FactDescriptor final {
    OriginId origin;
    std::optional<RequirementIdentity> requirement;
    std::optional<PhaseKind> phase;
    FactKind kind;
    Outcome outcome;
    std::optional<TickDelta> error;
    std::optional<CanonicalObservationKey> evidence;
};

struct ExecutionPreparation final {
    std::shared_ptr<const RuntimeState> initial;
    std::shared_ptr<const RecoveryInputs> inputs;
    std::vector<std::size_t> phaseOffsets, timerRequirements;
};

struct NativeRecoveryPoint final {
    std::shared_ptr<const RuntimeState> state;
    std::shared_ptr<const ExecutionLogChunk> prefix;
    std::shared_ptr<const ReplayData> recording;
    std::uint64_t completeRecords{};
};

struct NativeRecoveryIndex final {
    // One anchor after the first eligible record, then at existing chunk boundaries.
    // This optional cache never rejects content or changes record cadence. Empty pending
    // is a closed subset; other states retain explicit checkpoint/origin recovery.
    static constexpr std::size_t maxAnchors = 64;
    std::vector<std::shared_ptr<const NativeRecoveryPoint>> points;
};

struct ExecutionKernel::Impl final {
    SessionConfiguration configuration;
    PreparedGameplay prepared;
    SessionIngressState ingress;
    bool historyAvailable{true};
    std::vector<AdmittedObservation> pending;
    std::shared_ptr<const RuntimeState> active;
    std::optional<KernelTestControls> testControls;
    std::shared_ptr<RuntimeState> faultReserve;
    std::shared_ptr<const RecoveryInputs> recoveryInputs;
    std::shared_ptr<const ReplayData> recording;
    std::shared_ptr<const ExecutionLogChunk> logTail;
    std::shared_ptr<ExecutionLogChunk> reservedLog;
    std::shared_ptr<const ReplayData> seekSource;
    bool seekSourceSealed{false};
    core::Error failureResult;
    core::Error foldFailureResult;
    // Derived from one immutable PreparedGameplay; never serialized or globally cached.
    std::shared_ptr<const ExecutionPreparation> preparation;
    std::vector<std::shared_ptr<const NativeRecoveryPoint>> nativePoints;
    std::shared_ptr<const NativeRecoveryIndex> seekNativeSource;
    std::shared_ptr<CanonicalReplayIdentityCache> seekCanonicalCache;
    bool nativeAnchorEligible{true};
    std::uint64_t appliedRecords{};
    std::uint64_t nativeBaseRecords{};

    Impl(SessionConfiguration c, PreparedGameplay p, std::shared_ptr<const RuntimeState> state,
         std::shared_ptr<const ExecutionPreparation> cached = {});
    auto recordingCandidate() -> std::shared_ptr<ReplayData>;
    void publishRecording(std::shared_ptr<ReplayData> value) noexcept;
    void releaseSeekSource() noexcept;
    auto reserveFault(const RuntimeState& value) const -> std::shared_ptr<RuntimeState>;
    auto graph() const -> const CanonicalGameplayGraph&;
    void failIfRequested(Tick tick, KernelFailurePoint point) const;
    auto phaseIndex(std::size_t i, PhaseKind kind) const -> std::size_t;
    auto phase(const RuntimeState& d, std::size_t i, PhaseKind kind) const -> KernelPhaseState;
    void setPhase(RuntimeState& d, std::size_t i, PhaseKind kind, KernelPhaseState value) const;
    auto pendingPhase(RuntimeState& d, std::size_t i, PhaseKind kind) const -> bool;
    auto inWindow(const RequirementRecord& r, PhaseKind kind, Tick tick) const -> bool;
    auto target(const RequirementRecord& r, PhaseKind kind) const -> Tick;
    void outcome(RuntimeState& d, std::vector<FactDescriptor>& facts, std::size_t i, PhaseKind kind,
                 Outcome outcome, const OriginId& origin,
                 std::optional<CanonicalObservationKey> evidence = {},
                 bool observedError = false) const;
    void endOwnership(RuntimeState& d, std::size_t i) const;
    void failPending(RuntimeState& d, std::vector<FactDescriptor>& facts, std::size_t i,
                     const OriginId& origin) const;
    auto available(RuntimeState& d, std::size_t i, const std::optional<ContactHandle>& contact,
                   bool acquiring = true) const -> bool;
    void success(RuntimeState& d, std::vector<FactDescriptor>& facts, std::size_t i,
                 const OriginId& origin, Tick tick, std::optional<CanonicalObservationKey> evidence,
                 std::optional<ContactHandle> contact) const;
    void timer(RuntimeState& d, std::vector<FactDescriptor>& facts, const PreparedTimerKey& timer,
               std::size_t i, std::span<const AdmittedObservation> inputs) const;
    void input(RuntimeState& d, std::vector<FactDescriptor>& facts,
               const AdmittedObservation& input) const;
    void interruptRpa(RuntimeState& d, std::vector<FactDescriptor>& facts,
                      const CanonicalObservationKey& start, const OriginId& origin) const;
    void deferredRpa(RuntimeState& d, std::vector<FactDescriptor>& facts,
                     const AdmittedObservation& input) const;
    void validatePhaseLayout(const RuntimeState& d) const;
    void validateDraft(const RuntimeState& d, bool phaseLayout = true) const;
    auto idleDeadlineEnd(const RuntimeState& d, Tick tick, std::size_t& end) const -> bool;
    static void sealIdleDeadline(RuntimeState& d, Tick tick, std::size_t cursor) noexcept;
    void fold(RuntimeState& d, Tick tick) const;
    void ledger(RuntimeState& d, std::vector<FactDescriptor>& facts, Tick tick) const;
};

} // namespace cuexis::judgement::detail

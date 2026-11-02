#include "execution_host.hpp"

#include "../../../engine/playback/src/replay_identity.hpp"
#include "execution_host_units_internal.hpp"

#include <algorithm>
#include <cuexis_internal/execution_diagnostics.hpp>
#include <cuexis_internal/presentation_metrics.hpp>
#include <limits>
#include <thread>

namespace cuexis::player {

using namespace playback;
using namespace playback::detail;

#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
namespace rpa = cuexis::internal::execution_diagnostics::rpa;
#endif

auto ExecutionHost::audioPublication() const -> core::Result<AudioPublication> {
    if (!owner_.isCurrent() || !audio_)
        return core::unexpected(error("device_pending"));
    return audio_->publication();
}
auto ExecutionHost::snapshotGameplay() -> core::Result<GameplaySnapshot> try {
    checkOwner();
    return value<GameplaySnapshot>(session(SessionSnapshot{}));
} catch (const Failure& failure) {
    return core::unexpected(failure.error);
} catch (...) {
    return core::unexpected(error("capacity_exceeded"));
}
auto ExecutionHost::replayCut(const GameplayReplay& archive, GameplayTick horizon)
    -> core::Result<GameplayReplayCut> try {
    checkOwner();
    return value<GameplayReplayCut>(session(SessionArchiveCut{archive, horizon}));
} catch (const Failure& failure) {
    return core::unexpected(failure.error);
} catch (...) {
    return core::unexpected(error("capacity_exceeded"));
}
auto ExecutionHost::context() const noexcept -> const HostContext& {
    return context_;
}
auto ExecutionHost::state() const noexcept -> HostState {
    return closed_                                    ? HostState::closed
           : closePhase_ != ExecutionClosePhase::idle ? HostState::closing
           : suspended_                               ? HostState::suspended
           : host_                                    ? host_->state()
                                                      : HostState::controlPreparing;
}
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
void ExecutionHost::rpaMark(rpa::CloseStage stage) noexcept {
    rpaStage_ = stage;
    rpa::mark(rpaClose_, stage);
}
void ExecutionHost::rpaCause(rpa::Reason reason, const core::Error& failure) noexcept {
    rpa::firstCause(rpaClose_, reason, rpaStage_, failure.code());
}
void ExecutionHost::rpaSyncIdentity() noexcept {
    rpaClose_.identity.valid = true;
    rpaClose_.identity.operationId = control_.operationId;
    rpaClose_.identity.generation = control_.target.expectedGeneration;
    rpaClose_.identity.candidateId = control_.target.candidateId;
    rpaClose_.identity.scope = context_.sessionScope;
    rpaClose_.identity.controlSequence = context_.controlSequence;
    rpaClose_.identity.kind = static_cast<std::uint8_t>(control_.kind);
    rpaClose_.identity.clockSegment = context_.clockSegment;
    rpaClose_.identity.runEpoch = context_.runEpoch;
    rpaClose_.identity.resourceGeneration = context_.resourceGeneration;
    rpaClose_.identity.rendererGeneration = context_.rendererGeneration;
    rpaClose_.identity.contentIdentity = context_.contentIdentity.sha256;
}
void ExecutionHost::rpaCaptureFence(rpa::CloseObservationPart part) noexcept {
    // Diagnostic replies have their own IDs and destinations. They must never occupy the
    // production in-flight slot or be consumed as a later Close step's typed reply.
    if (sessionJob_)
        return;
    const auto harvest = [&]() -> bool {
        auto result = take(session_->poll(*rpaObservationJob_));
        if (!result)
            return false;
        take(session_->acknowledge(*rpaObservationJob_));
        rpaObservationJob_.reset();
        const auto* observation =
            *result ? std::get_if<rpa::OwnerCloseObservation>(&**result) : nullptr;
        if (!observation) {
            if (rpaObservationPart_ == rpa::CloseObservationPart::closeStep)
                rpaClose_.fence.closeState = rpa::CloseStepState::observationMissing;
            return true;
        }
        switch (rpaObservationPart_) {
        case rpa::CloseObservationPart::cleanup:
            rpaClose_.cleanupFence = observation->cleanup;
            break;
        case rpa::CloseObservationPart::closeRetire:
            rpaClose_.closeRetireFence = observation->closeRetire;
            break;
        case rpa::CloseObservationPart::closeStep:
            rpaClose_.fence = observation->closeStep;
            break;
        }
        return true;
    };
    try {
        if (rpaObservationJob_ && !harvest())
            return;
        if (closeNow() >= closeDeadline_)
            return;
        rpaObservationPart_ = part;
        rpaObservationJob_ = take(session_->post(SessionRpaObservation{}, false));
        // A diagnostic-only wait cap; production budget decisions use closeNow/closeDeadline.
        const auto observationLimit =
            std::chrono::steady_clock::now() + std::chrono::milliseconds{5};
        while (!harvest()) {
            if (closeNow() >= closeDeadline_ ||
                std::chrono::steady_clock::now() >= observationLimit)
                return;
            session_->waitForReply(*rpaObservationJob_, std::chrono::milliseconds{1});
        }
    } catch (...) {
        if (part == rpa::CloseObservationPart::closeStep)
            rpaClose_.fence.closeState = rpa::CloseStepState::observationMissing;
    }
    rpaSeekFetch();
}
void ExecutionHost::rpaSeekFetch() noexcept {
    // One control round trip outside every seek phase; its cost is recorded as perturbation.
    const auto started = rpa::nowUs();
    try {
        if (closePhase_ != ExecutionClosePhase::idle) {
            if (sessionJob_ || closeNow() >= closeDeadline_)
                return;
            if (!rpaSeekJob_)
                rpaSeekJob_ = take(session_->post(SessionRpaSeekObservation{}, false));
            const auto limit = std::chrono::steady_clock::now() + std::chrono::milliseconds{5};
            for (;;) {
                auto result = take(session_->poll(*rpaSeekJob_));
                if (result) {
                    take(session_->acknowledge(*rpaSeekJob_));
                    rpaSeekJob_.reset();
                    if (*result) {
                        if (const auto* ledger = std::get_if<rpa::SeekLedger>(&**result))
                            rpa::mergeSeek(rpaSeek_, *ledger);
                    }
                    break;
                }
                if (closeNow() >= closeDeadline_ || std::chrono::steady_clock::now() >= limit)
                    break;
                session_->waitForReply(*rpaSeekJob_, std::chrono::milliseconds{1});
            }
            rpa::seekFetch(rpaSeek_, rpa::nowUs() - started);
            return;
        }
        auto reply = session(SessionRpaSeekObservation{}, false);
        if (const auto* ledger = std::get_if<rpa::SeekLedger>(&reply))
            rpa::mergeSeek(rpaSeek_, *ledger);
    } catch (...) {
    }
    rpa::seekFetch(rpaSeek_, rpa::nowUs() - started);
}
#endif

} // namespace cuexis::player

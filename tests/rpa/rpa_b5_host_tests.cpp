#include "../../app/player/src/execution_host.hpp"
#include "../../engine/core/internal/cuexis_internal/sha256.hpp"
#include "../../engine/playback/src/gameplay_internal.hpp"
#include "../../engine/playback/src/replay_identity.hpp"
#include "../audio/fake_audio_transport.hpp"
#include "../support/environment.hpp"
#include "execution_fixture.hpp"
#include <atomic>
#include <bit>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <cuexis/judgement/recovery_codec.hpp>
#include <filesystem>
#include <future>
#include <iostream>
#include <new>
#include <span>

using namespace cuexis;
using namespace playback;
using namespace playback::detail;
TEST_CASE("Finite audio endpoints bound extrapolation before explicit EOF tail", "[rpa][b5]") {
    auto clock = RealtimeClock::create({500000, 20000, 64, "fixture", true},
                                       {{0, 1}, {0, 1}, {0, 1}, {1, 1}}, "fixture", 1);
    REQUIRE(clock);
    ClockObservation sample{"fixture",
                            1,
                            1,
                            100,
                            500100,
                            0,
                            ClockFramePosition{9, 1000000},
                            ClockFramePosition{9, 1000000},
                            {},
                            ClockPlaybackState::Playing,
                            ClockQuality::Estimated,
                            false,
                            false,
                            false};
    sample.maximumPresented = ClockFramePosition{10, 1000000};
    auto ticks = clock->sample(sample, 200);
    REQUIRE(ticks);
    CHECK(ticks->horizon == 10);
    sample.publicationSequence = 2;
    sample.presented = ClockFramePosition{10, 1000000};
    sample.source = sample.presented;
    sample.playback = ClockPlaybackState::EndOfStream;
    auto tail = clock->startTail(sample, 201, 2, {{1, 1}, 15, 0, "fixture", true}, 10, 1);
    REQUIRE(tail);
    CHECK(tail->terminalHorizon == 16);
    auto invalid = RealtimeClock::create({500000, 20000, 64, "fixture", true},
                                         {{0, 1}, {0, 1}, {0, 1}, {1, 1}}, "fixture", 1);
    REQUIRE(invalid);
    sample.playback = ClockPlaybackState::Playing;
    sample.maximumPresented = ClockFramePosition{9, 1000000};
    REQUIRE_FALSE(invalid->sample(sample, 201));
}
namespace {
class Device final : public OwnedAudioDevice {
  public:
    explicit Device(audio::AudioClipStore& store) : transport_(store) {}
    auto transport() noexcept -> audio::IAudioTransport& override {
        return transport_;
    }
    auto isolateCallbacks() -> core::Result<bool> override {
        return transport_.snapshot().source.state != audio::PlaybackState::Playing;
    }
    auto recheckDevice() -> core::Result<void> override {
        return {};
    }

  private:
    test_support::FakeAudioTransport transport_;
};
auto options() -> player::ExecutionHostOptions {
    player::ExecutionHostOptions result;
    result.session = {64, 16, 128, 32, 16, 64, true, {}};
    result.audio.sessionScope = 1;
    result.audio.timeDomain = 1;
    result.audio.testOnly = true;
    result.audio.capacity = 64;
    result.audio.controlReserve = 16;
    result.audio.clipLimits = {4, 1024 * 1024, 4 * 1024 * 1024};
    result.hostCapacity = 16;
    result.captureCapacity = 64;
    result.captureReserve = 16;
    result.replyDeadline = std::chrono::seconds{5};
    result.viewport = {640, 480};
    return result;
}
auto ports(std::atomic<unsigned>& pumps) -> player::ExecutionHostPorts {
    return {[](audio::AudioClipStore& store) -> core::Result<std::unique_ptr<OwnedAudioDevice>> {
                return std::unique_ptr<OwnedAudioDevice>{std::make_unique<Device>(store)};
            },
            [](std::span<const std::byte>) {
                return audio::AudioClip::create(48000, 1, std::vector<float>(96000));
            },
            [&]() -> core::Result<void> {
                ++pumps;
                return {};
            }};
}
} // namespace
TEST_CASE("R1 real worker delivery failure preserves the first Close error",
          "[rpa][r1][host][first-error]") {
    enum class FailureOrder { deliveryOnly, deliveryThenExport, cutThenDelivery };
    const auto order = GENERATE(FailureOrder::deliveryOnly, FailureOrder::deliveryThenExport,
                                FailureOrder::cutThenDelivery);
    presentation_renderer::TestPresentationRenderer renderer;
    std::atomic<unsigned> pumps{};
    std::atomic<bool> armed{}, injected{};
    auto settings = options();
    const ClockMappingConfiguration mapping{{0, 1}, {0, 1}, {0, 1}, {1, 1000000}};
    settings.realtime =
        SessionRealtimeStart{{1000000, 10, 128, "", true}, mapping, {}, "r1.first-error"};
    settings.session.beforeDispatch = [&](std::uint64_t, const SessionCommand& command) {
        if (armed && std::holds_alternative<SessionCaptureDelivery>(command) &&
            !injected.exchange(true))
            throw std::bad_alloc{};
    };
    auto fixture = std::make_unique<test_support::SessionTestContent>();
    test_support::rpa(*fixture, mapping);
    bool interrupted{};
    unsigned accepted{}, finished{};
    auto wired = ports(pumps);
    wired.beforeClosePhase = [&](player::ExecutionClosePhase phase) -> core::Result<void> {
        using Phase = player::ExecutionClosePhase;
        if (phase == Phase::postAcceptance)
            ++accepted;
        if (phase == Phase::finish)
            ++finished;
        if (phase == Phase::flush)
            armed = true;
        if (!interrupted && order == FailureOrder::cutThenDelivery && phase == Phase::cut) {
            interrupted = true;
            return core::unexpected(core::Error{"fixture.cut_failure", "Earlier cut failure"});
        }
        if (!interrupted && order == FailureOrder::deliveryThenExport &&
            phase == Phase::exportEvidence) {
            interrupted = true;
            return core::unexpected(core::Error{"fixture.export_failure", "Later export failure"});
        }
        return {};
    };
    auto host = player::ExecutionHost::create(renderer, settings, std::move(wired));
    REQUIRE(host);
    REQUIRE((*host)->load(test_support::presentation(), fixture->content()));
    const auto original = (*host)->context();
    ClockObservation clock{};
    clock.monotonicDomain = "r1.first-error";
    clock.segment = original.clockSegment;
    clock.publicationSequence = 1;
    clock.sampleMonotonicUs = std::chrono::duration_cast<std::chrono::microseconds>(
                                  std::chrono::steady_clock::now().time_since_epoch())
                                  .count();
    clock.validUntilMonotonicUs = clock.sampleMonotonicUs + 1000000;
    clock.playback = ClockPlaybackState::Playing;
    clock.quality = ClockQuality::Exact;
    clock.source = ClockFramePosition{1, 1};
    clock.presented = clock.source;
    REQUIRE((*host)->advance(clock));
    const auto first = (*host)->close();
    if (order == FailureOrder::deliveryOnly) {
        REQUIRE(first);
    } else {
        REQUIRE_FALSE(first);
        CHECK(first.error().code() == (order == FailureOrder::cutThenDelivery
                                           ? "fixture.cut_failure"
                                           : "fixture.export_failure"));
        CHECK((*host)->state() == HostState::closing);
        REQUIRE((*host)->closeFirstError());
        CHECK((*host)->closeFirstError()->code() == (order == FailureOrder::cutThenDelivery
                                                         ? "fixture.cut_failure"
                                                         : "host.rpa.capacity_exceeded"));
        REQUIRE((*host)->close());
    }
    REQUIRE(injected);
    CHECK((*host)->state() == HostState::closed);
    CHECK((*host)->closeOwnersExited());
    CHECK((*host)->context().controlSequence == original.controlSequence + 1);
    CHECK(accepted == 1);
    CHECK(finished == 1);
    REQUIRE((*host)->closeDeliveryError());
    CHECK((*host)->closeDeliveryError()->code() == "host.rpa.capacity_exceeded");
    REQUIRE((*host)->closeFirstError());
    CHECK((*host)->closeFirstError()->code() == (order == FailureOrder::cutThenDelivery
                                                     ? "fixture.cut_failure"
                                                     : (*host)->closeDeliveryError()->code()));
}

TEST_CASE("B5 realtime main host cuts after inspection and drives Pause Resume Seek Close",
          "[b5][host][realtime]") {
    presentation_renderer::TestPresentationRenderer renderer;
    std::atomic<unsigned> pumps{};
    auto settings = options();
    const auto canonical = GENERATE(false, true);
    settings.seekIdentity = canonical ? player::SeekIdentityPolicy::canonicalContent
                                      : player::SeekIdentityPolicy::owningSource;
    ClockMappingConfiguration mapping{{100000, 1}, {2, 1}, {9, 1}, {1, 1}};
    settings.realtime = SessionRealtimeStart{{1000000, 10, 128, "", true}, mapping, {}, "b5.host"};
    test_support::SessionTestContent fixture;
    test_support::rpa(fixture, mapping);
    auto wired = ports(pumps);
    player::ExecutionHost* live{};
    bool captureDuringInspect{};
    unsigned captures{};
    std::int64_t sourceH{1000};
    auto sourceAnchor = std::chrono::steady_clock::now();
    const auto sourceElapsed = [&] {
        return std::chrono::duration_cast<std::chrono::microseconds>(
                   std::chrono::steady_clock::now() - sourceAnchor)
            .count();
    };
    wired.pumpMain = [&]() -> core::Result<void> {
        ++pumps;
        if (live && captureDuringInspect) {
            auto input = test_support::input();
            input.action = GameplayInputAction::Release;
            input.sequence = captures + 1;
            if (auto q = live->pausedCut())
                input.observationTick = {*q};
            else {
                sourceH += 1000;
                input.observationTick = {sourceH + sourceElapsed() - 2};
            }
            auto captured = live->capture(std::move(input));
            if (!captured)
                return captured;
            ++captures;
        }
        return {};
    };
    const auto capturePump = wired.pumpMain;
    unsigned capturesAtClockRead{};
    auto host = player::ExecutionHost::create(renderer, settings, std::move(wired));
    REQUIRE(host);
    live = host->get();
    auto loaded = live->load(test_support::presentation(), fixture.content());
    INFO((loaded ? "" : std::string{loaded.error().code()}));
    REQUIRE(loaded);
    std::uint64_t publication{};
    player::ExecutionHost::ClockSource source =
        [&](const HostContext& c) -> core::Result<ClockObservation> {
        // This callback is after inspection and before the control cut. A completed owner
        // reply need not enter the wait/pump loop, so inject here independently of scheduling.
        if (captureDuringInspect) {
            if (auto pumped = capturePump(); !pumped)
                return core::unexpected(std::move(pumped.error()));
            ++capturesAtClockRead;
        }
        const auto now = std::chrono::duration_cast<std::chrono::microseconds>(
                             std::chrono::steady_clock::now().time_since_epoch())
                             .count();
        ClockObservation o{};
        o.monotonicDomain = "b5.host";
        o.segment = c.clockSegment;
        o.publicationSequence = ++publication;
        o.sampleMonotonicUs = now;
        o.validUntilMonotonicUs = now + 1000000;
        o.presented = ClockFramePosition{sourceH + sourceElapsed() + 100000, 1000000};
        o.playback = ClockPlaybackState::Playing;
        o.quality = ClockQuality::Exact;
        return o;
    };
    captureDuringInspect = true;
    auto paused = live->pause(source);
    INFO((paused
              ? ""
              : std::string{paused.error().code()} + ": " + std::string{paused.error().message()}));
    REQUIRE(paused);
    REQUIRE(live->state() == HostState::paused);
    REQUIRE(live->pausedCut());
    CHECK(captures > 0);
    CHECK(capturesAtClockRead == 1);
    CHECK(*live->pausedCut() >= sourceH - 2);
    captureDuringInspect = false;
    sourceH = *live->pausedCut() + 10000;
    auto resumed = live->resume(source);
    INFO((resumed ? "" : std::string{resumed.error().code()}));
    REQUIRE(resumed);
    CHECK(live->state() == HostState::running);
    auto pausedAgain = live->pause(source);
    INFO((pausedAgain ? "" : std::string{pausedAgain.error().code()}));
    REQUIRE(pausedAgain);
    auto archive = live->archive();
    REQUIRE(archive);
    auto cut = live->replayCut(*archive, {4});
    REQUIRE(cut);
    sourceH = 4;
    sourceAnchor = std::chrono::steady_clock::now();
    auto sought = live->seek(*archive, *cut, {4}, source);
    INFO((sought
              ? ""
              : std::string{sought.error().code()} + ": " + std::string{sought.error().message()}));
    REQUIRE(sought);
    CHECK(live->state() == HostState::running);
    auto frame = live->sample(13, {0.013, 0, live->context().clockSegment}, false);
    INFO((frame ? "" : std::string{frame.error().code()}));
    REQUIRE(frame);
    REQUIRE(live->present(*frame));
    auto closed = live->close();
    INFO((closed
              ? ""
              : std::string{closed.error().code()} + ": " + std::string{closed.error().message()}));
    REQUIRE(closed);
    CHECK(live->state() == HostState::closed);
    REQUIRE(live->closeEvidence());
    CHECK(live->closeEvidence()->pending().empty());
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    namespace rpa = cuexis::internal::execution_diagnostics::rpa;
    const auto& closedObservation = live->rpaObservation();
    CHECK(closedObservation.captureTerminal == 2);
    CHECK(closedObservation.captureClosed);
    CHECK(closedObservation.softwareTerminal == 2);
    CHECK(closedObservation.softwareClosed);
    CHECK(closedObservation.identity.status == static_cast<std::uint8_t>(ControlStatus::complete));
    CHECK(closedObservation.unackedDeliveries == closedObservation.evidenceDeliveries);
    // This Close is called once, with no pending job and no failure. The Seek completed and
    // released its own fence, so Close takes a fresh normal control fence rather than reusing an
    // existing one. The existing-fence branch is separately mapped on the cleanup-permit path.
    CHECK(closedObservation.attempts == 1);
    CHECK(closedObservation.reentries == 0);
    CHECK(closedObservation.firstDecisionOrdinal > 0);
    CHECK(closedObservation.pendingJobKind == 0);
    CHECK(closedObservation.pendingSessionJobAtEntry == 0);
    CHECK(closedObservation.pendingAudioJobAtEntry == 0);
    CHECK(closedObservation.firstCause == rpa::Reason::none);
    CHECK(closedObservation.closeRetireFence.path == rpa::FencePath::normalControl);
    CHECK(closedObservation.closeRetireFence.applicability == rpa::FenceApplicability::applicable);
    CHECK(closedObservation.closeRetireFence.attempted);
    CHECK_FALSE(closedObservation.closeRetireFence.existingFence);
    CHECK(closedObservation.closeRetireFence.succeeded);
    CHECK(closedObservation.closedFlag);
    const auto phaseCount = [&](rpa::SeekPhase phase) {
        return live->rpaSeekObservation().phases[static_cast<std::size_t>(phase)].count;
    };
    const auto& seekLedger = live->rpaSeekObservation();
    CHECK(seekLedger.seeks >= 1);
    CHECK(phaseCount(rpa::SeekPhase::s0HostIdentity) >= 1);
    CHECK(phaseCount(rpa::SeekPhase::s1HostProtocol) >= 2);
    // S2 is measured inside Playback through the owner thread's seek sink; S3 wraps the
    // candidate validation and records the work the kernel actually performs.
    CHECK(phaseCount(rpa::SeekPhase::s2CandidatePrep) >= 1);
    CHECK(phaseCount(rpa::SeekPhase::s3Validation) >= 1);
    CHECK(phaseCount(rpa::SeekPhase::s4InternalExecution) >= 1);
    CHECK(phaseCount(rpa::SeekPhase::s5PublishSwap) >= 1);
    CHECK(phaseCount(rpa::SeekPhase::s6ClockPresent) >= 1);
    const auto& work = seekLedger.work;
    const auto& identityWork = work[static_cast<std::size_t>(rpa::SeekPhase::s0HostIdentity)];
    if (canonical)
        CHECK(identityWork.archiveBytes > 0);
    else
        CHECK(identityWork.archiveBytes == 0);
    CHECK(identityWork.encodeCalls == (canonical ? 1 : 0));
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s0HostIdentity)].sealedIdentityKnown == 1);
    CHECK(identityWork.sealedIdentityPresent == (canonical ? 1 : 0));
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s1HostProtocol)].operationId > 0);
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s1HostProtocol)].participants >= 3);
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s2CandidatePrep)].validCandidates == 1);
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s2CandidatePrep)].entities >= 1);
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s2CandidatePrep)].requirements >= 1);
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s2CandidatePrep)].preparedHandles == 1);
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s2CandidatePrep)].configCopies == 1);
    // Canonical encoding and the Seek candidate both reuse their immutable preparation.
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s2CandidatePrep)].kernelPrepareCalls == 0);
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s2CandidatePrep)].derivedIndexBuilds == 0);
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s3Validation)].actualRecords >= 1);
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s3Validation)].batchInputs >= 1);
    // This fixture's cut contains admission batches but no recorded result rows, so the archive
    // has nothing for sameKernelResult to compare; zero is the measured value, not lost evidence.
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s3Validation)].sameKernelCalls == 0);
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s3Validation)].nestedResultElements >= 1);
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s4InternalExecution)].fullTicks >= 1);
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s4InternalExecution)].matcherSteps >= 1);
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s4InternalExecution)].runtimePeakElements >=
          1);
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s4InternalExecution)].recordPeakElements >=
          1);
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s5PublishSwap)].objects >= 2);
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s5PublishSwap)].releases >= 1);
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s5PublishSwap)].oldOwnersRetained == 1);
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s6ClockPresent)].clockValid == 1);
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s6ClockPresent)].targetGeneration > 0);
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s6ClockPresent)].targetPresentationTick >
          0);
    CHECK(work[static_cast<std::size_t>(rpa::SeekPhase::s6ClockPresent)].endToEndUs > 0);
    CHECK(seekLedger.envelopeTotalUs > 0);
    CHECK(seekLedger.fetchCount >= 2);
    CHECK(seekLedger.timerCostNs > 0);
    CHECK(seekLedger.unaccountedMaxUs < seekLedger.envelopeMaxUs);
    CHECK(seekLedger.seeksWithoutPhases == 0);
#endif
}

TEST_CASE("B5 headless EOF tail preserves owners through Pause Resume and terminal execution",
          "[b5][host][tail]") {
    presentation_renderer::TestPresentationRenderer renderer;
    std::atomic<unsigned> pumps{};
    auto settings = options();
    ClockMappingConfiguration mapping{{0, 1}, {0, 1}, {0, 1}, {1, 1}};
    settings.realtime = SessionRealtimeStart{{1000000, 10, 128, "", true}, mapping, {}, "b5.tail"};
    test_support::SessionTestContent fixture;
    test_support::rpa(fixture, mapping);
    auto host = player::ExecutionHost::create(renderer, settings, ports(pumps));
    REQUIRE(host);
    auto content = GameplayContent::fromPacked(fixture.packed(), fixture.config);
    REQUIRE(content);
    REQUIRE((*host)->load(test_support::presentation(), *content));
    const auto before = (*host)->context();
    const auto now = std::chrono::duration_cast<std::chrono::microseconds>(
                         std::chrono::steady_clock::now().time_since_epoch())
                         .count();
    ClockObservation eof{};
    eof.monotonicDomain = "b5.tail";
    eof.segment = before.clockSegment;
    eof.publicationSequence = 1;
    eof.sampleMonotonicUs = now;
    eof.validUntilMonotonicUs = now + 1000000;
    eof.source = ClockFramePosition{0, 1000000};
    eof.presented = eof.source;
    eof.playback = ClockPlaybackState::EndOfStream;
    eof.quality = ClockQuality::Exact;
    auto tail = (*host)->beginTail(eof, {{1, 1}, 100000, 0, "explicit CPU fixture", true});
    INFO((tail ? ""
               : std::string{tail.error().code()} + ": " + std::string{tail.error().message()}));
    REQUIRE(tail);
    CHECK((*host)->context().sessionScope == before.sessionScope);
    CHECK((*host)->context().runEpoch == before.runEpoch);
    CHECK((*host)->context().clockSegment == before.clockSegment + 1);
    auto advanced = (*host)->advance(eof);
    INFO((advanced ? "" : std::string{advanced.error().code()}));
    REQUIRE(advanced);
    player::ExecutionHost::ClockSource source =
        [&](const HostContext&) -> core::Result<ClockObservation> { return eof; };
    auto paused = (*host)->pause(source);
    INFO((paused
              ? ""
              : std::string{paused.error().code()} + ": " + std::string{paused.error().message()}));
    REQUIRE(paused);
    const auto frozen = (*host)->inputTicks(eof);
    REQUIRE(frozen);
    std::this_thread::sleep_for(std::chrono::milliseconds{20});
    REQUIRE((*host)->inputTicks(eof)->horizon == frozen->horizon);
    auto resumed = (*host)->resume(source);
    INFO((resumed ? ""
                  : std::string{resumed.error().code()} + ": " +
                        std::string{resumed.error().message()}));
    REQUIRE(resumed);
    bool complete{};
    GameplayResult terminalResult;
    const auto limit = std::chrono::steady_clock::now() + std::chrono::seconds{3};
    while (!complete && std::chrono::steady_clock::now() < limit) {
        const auto requestStart = std::chrono::steady_clock::now();
        auto result = (*host)->advance(eof);
        if (!result && result.error().code() == "host.rpa.control_pending")
            continue;
        INFO((result ? ""
                     : std::string{result.error().code()} + ": " +
                           std::string{result.error().message()}));
        REQUIRE(result);
        const auto requestUs = std::chrono::duration_cast<std::chrono::microseconds>(
                                   std::chrono::steady_clock::now() - requestStart)
                                   .count();
        auto lag = (*host)->lag(eof);
        REQUIRE(lag);
        std::cout << "{\"event\":\"b5.cpu.owner_sample\",\"test_only\":true,\"fixture\":\"single_"
                     "requirement_tail\",\"request_us\":"
                  << requestUs << ",\"clock_h\":" << lag->clockHorizon.value
                  << ",\"h_lag\":" << (lag->horizonLag ? std::to_string(*lag->horizonLag) : "null")
                  << ",\"frontier_lag\":"
                  << (lag->frontierLag ? std::to_string(*lag->frontierLag) : "null")
                  << ",\"sealed_cursor\":" << lag->sealedFactCursor
                  << ",\"folded_cursor\":" << lag->foldedFactCursor << "}\n";
        complete = result->disposition == GameplayAdvanceDisposition::Executed &&
                   result->request.horizon.value == (*host)->tail()->terminalHorizon;
        if (complete)
            terminalResult = result->result;
    }
    REQUIRE(complete);
    auto replay = (*host)->archive();
    REQUIRE(replay);
    auto evaluated = (*host)->evaluateReplay(*replay);
    REQUIRE(evaluated);
    REQUIRE(evaluated->evidenceValid);
    auto equal = terminalResult.sameResult(evaluated->result);
    REQUIRE(equal);
    CHECK(*equal);
    auto frame = (*host)->sample(
        tail->terminalHorizon,
        {static_cast<double>(tail->terminalHorizon) / 1000.0, 0, (*host)->context().clockSegment},
        false);
    REQUIRE(frame);
    REQUIRE((*host)->present(*frame));
    const auto closeStart = std::chrono::steady_clock::now();
    REQUIRE((*host)->close());
    std::cout << "{\"event\":\"b5.cpu.close\",\"test_only\":true,\"close_join_us\":"
              << std::chrono::duration_cast<std::chrono::microseconds>(
                     std::chrono::steady_clock::now() - closeStart)
                     .count()
              << ",\"session_capacity\":" << settings.session.capacity
              << ",\"session_control_reserve\":" << settings.session.controlReserve
              << ",\"capture_capacity\":" << settings.captureCapacity << "}\n";
    REQUIRE(replay->toBytes());
    CHECK_FALSE(frame->snapshotView().objects.empty());
}
TEST_CASE("B5 main host drives actual owners and pumps while awaiting directed Close",
          "[b5][host]") {
    presentation_renderer::TestPresentationRenderer renderer;
    std::atomic<unsigned> pumps{};
    auto host = player::ExecutionHost::create(renderer, options(), ports(pumps));
    REQUIRE(host);
    auto source = PlaybackSource::fromFilesystemProject(std::filesystem::path{CUEXIS_SOURCE_DIR} /
                                                        "assets/projects/stage3_project");
    REQUIRE(source);
    auto loaded = (*host)->load(std::move(*source));
    INFO((loaded
              ? ""
              : std::string{loaded.error().code()} + ": " + std::string{loaded.error().message()}));
    REQUIRE(loaded);
    REQUIRE((*host)->resize({320, 200}));
    REQUIRE_FALSE((*host)->resize({0, 200}));
    REQUIRE((*host)->state() == HostState::running);
    auto frame = (*host)->sample(1000, {1, 1, 1}, false);
    INFO((frame ? ""
                : std::string{frame.error().code()} + ": " + std::string{frame.error().message()}));
    REQUIRE(frame);
    auto receipt = (*host)->present(*frame);
    REQUIRE(receipt);
    REQUIRE(receipt->status == PresentStatus::succeeded);
    auto closed = (*host)->close();
    INFO((closed
              ? ""
              : std::string{closed.error().code()} + ": " + std::string{closed.error().message()}));
    REQUIRE(closed);
    REQUIRE((*host)->state() == HostState::closed);
    CHECK(pumps.load() > 0);
    CHECK((*host)->pumpCount() == pumps.load());
    // External owning snapshots do not depend on either joined worker.
    CHECK_FALSE(frame->snapshotView().objects.empty());
    REQUIRE((*host)->close());
}

#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
TEST_CASE("R0 close observation keeps first cause and bounded slots", "[rpa][r0][observation]") {
    namespace rpa = cuexis::internal::execution_diagnostics::rpa;
    rpa::Close close{};
    rpa::firstCause(close, rpa::Reason::deliveryFailed, rpa::CloseStage::closeFlush,
                    "host.rpa.delivery");
    rpa::firstCause(close, rpa::Reason::operationFailed, rpa::CloseStage::finish,
                    "host.rpa.capacity");
    CHECK(close.errors == 2);
    CHECK(close.firstCause == rpa::Reason::deliveryFailed);
    CHECK(close.firstCauseStage == rpa::CloseStage::closeFlush);
    CHECK(std::string{close.firstErrorCode} == "host.rpa.delivery");
    for (int index = 0; index < 40; ++index)
        rpa::mark(close, rpa::CloseStage::closeEnter);
    CHECK(close.markCount == rpa::closeMarkCapacity);
    CHECK(close.marksLost == 8);
    CHECK(close.truncated);
    for (int index = 0; index < 10; ++index)
        rpa::addWait(close, rpa::CloseStage::exitWait, index, index == 9);
    CHECK(close.waitCount == rpa::closeWaitCapacity);
    CHECK(close.waitsLost == 2);
    CHECK(close.firstOverBudget == rpa::CloseStage::exitWait);

    rpa::SampleClaim claim{};
    CHECK(rpa::classifySample(claim) == rpa::SampleReject::identityMismatch);
    claim.identityMatched = true;
    CHECK(rpa::classifySample(claim) == rpa::SampleReject::noFormalWindow);
    claim.formalWindow = true;
    CHECK(rpa::classifySample(claim) == rpa::SampleReject::none);
    claim.instrumentedBuild = true;
    CHECK(rpa::classifySample(claim) == rpa::SampleReject::instrumentedBuild);
}

TEST_CASE("R0 close observation records a normal complete Close path",
          "[rpa][r0][observation][b5]") {
    namespace rpa = cuexis::internal::execution_diagnostics::rpa;
    presentation_renderer::TestPresentationRenderer renderer;
    std::atomic<unsigned> pumps{};
    auto host = player::ExecutionHost::create(renderer, options(), ports(pumps));
    REQUIRE(host);
    auto source = PlaybackSource::fromFilesystemProject(std::filesystem::path{CUEXIS_SOURCE_DIR} /
                                                        "assets/projects/stage3_project");
    REQUIRE(source);
    REQUIRE((*host)->load(std::move(*source)));
    REQUIRE((*host)->resize({320, 200}));
    auto frame = (*host)->sample(1000, {1, 1, 1}, false);
    REQUIRE(frame);
    REQUIRE((*host)->present(*frame));
    const auto context = (*host)->context();
    auto closed = (*host)->close();
    INFO((closed
              ? ""
              : std::string{closed.error().code()} + ": " + std::string{closed.error().message()}));
    REQUIRE(closed);
    const auto& observed = (*host)->rpaObservation();
    CHECK(observed.requestedUs > 0);
    CHECK(observed.deadlineUs >= observed.requestedUs);
    CHECK(observed.identity.valid);
    CHECK(observed.identity.operationId != 0);
    CHECK(observed.identity.decisionOrdinal != 0);
    CHECK(observed.identity.scope == context.sessionScope);
    CHECK(observed.identity.clockSegment == context.clockSegment);
    CHECK(observed.identity.runEpoch == context.runEpoch);
    CHECK(observed.identity.resourceGeneration == context.resourceGeneration);
    CHECK(observed.identity.rendererGeneration == context.rendererGeneration);
    CHECK(observed.identity.contentIdentity == context.contentIdentity.sha256);
    CHECK(observed.identity.status == static_cast<std::uint8_t>(ControlStatus::complete));
    // This fixture loads a presentation-only project, so no gameplay capture route exists and the
    // capture terminal stages are not applicable rather than missing evidence.
    CHECK(observed.closeTick == 0);
    CHECK(observed.captureTerminal == 0);
    CHECK_FALSE(observed.captureClosed);
    CHECK(observed.softwareTerminal == 2);
    CHECK(observed.softwareRetired);
    CHECK(observed.softwareClosed);
    CHECK(observed.evidenceDeliveries == 0);
    CHECK(observed.unackedDeliveries == observed.evidenceDeliveries);
    CHECK(observed.pumpSamples > 0);
    CHECK(observed.pumpClosed > 0);
    CHECK(observed.errors == 0);
    CHECK(observed.receiptStatus == static_cast<std::uint8_t>(ControlStatus::complete));
    CHECK(observed.finishCalled);
    CHECK(observed.confirmCalled);
    CHECK(observed.shutdownRequested);
    CHECK(observed.workerExited);
    CHECK(observed.workerJoined);
    CHECK(observed.closedFlag);
    CHECK(observed.facadeAfter == static_cast<std::uint8_t>(HostState::closed));
    CHECK(observed.firstCause == rpa::Reason::none);
    CHECK(observed.firstOverBudget == rpa::CloseStage::none);
    CHECK(observed.markCount > 10);
    CHECK(observed.markCount <= rpa::closeMarkCapacity);
    CHECK(observed.marks[0].stage == rpa::CloseStage::closeEnter);
    CHECK(observed.marks[observed.markCount - 1].stage == rpa::CloseStage::closed);
    CHECK(observed.closeRetireFence.permitAttempted);
    CHECK(observed.closeRetireFence.permitDirected);
    // A complete Close still takes the normal software fence; the F1 permit bypass is reserved for
    // suspended/failed cleanup. No existing fence was present, so only the existing-fence branch is
    // not applicable; the normal fence itself is applicable and succeeded.
    CHECK(observed.closeRetireFence.path == rpa::FencePath::normalControl);
    CHECK(observed.closeRetireFence.applicability == rpa::FenceApplicability::applicable);
    CHECK_FALSE(observed.closeRetireFence.existingFence);
    CHECK(observed.closeRetireFence.attempted);
    CHECK(observed.closeRetireFence.succeeded);
    CHECK(observed.closeRetireFence.retireCalled);
    CHECK(observed.closeRetireFence.retireApplied);
    CHECK(observed.closeRetireFence.referencesReleased);
    CHECK(observed.fence.closeAttempted);
    CHECK(observed.fence.closeApplied);
    CHECK(observed.fence.closeState == rpa::CloseStepState::applied);
    CHECK(observed.fence.identity.operationId == observed.identity.operationId);
    CHECK_FALSE(observed.fence.retireCalled);
    CHECK_FALSE(observed.cleanupFence.identity.valid);
}

TEST_CASE("R0 seek ledger keeps envelope, nesting and merge accounting",
          "[rpa][r0][observation][seek]") {
    namespace rpa = cuexis::internal::execution_diagnostics::rpa;
    const auto index = [](rpa::SeekPhase phase) { return static_cast<std::size_t>(phase); };
    rpa::SeekLedger ledger{};
    ledger.timerCostNs = rpa::calibrateTimer();
    CHECK(ledger.timerCostNs < 1000000);
    // An envelope with no root phase cannot be attributed and must be visible as such.
    {
        rpa::SeekEnvelope envelope{&ledger};
        (void)envelope;
    }
    CHECK(ledger.seeks == 1);
    CHECK(ledger.seeksWithoutPhases == 1);
    CHECK(ledger.seekRootCount == 0);
    CHECK(ledger.unaccountedTotalUs == ledger.envelopeTotalUs);
    // A nested phase is attributed to its own slot but never counted as a root sample.
    {
        rpa::SeekEnvelope envelope{&ledger};
        rpa::SeekScope outer{&ledger, rpa::SeekPhase::s3Validation};
        {
            rpa::SeekScope inner{&ledger, rpa::SeekPhase::s1HostProtocol};
        }
    }
    CHECK(ledger.depth == 0);
    CHECK(ledger.seeks == 2);
    CHECK(ledger.seeksWithoutPhases == 1);
    CHECK(ledger.phases[index(rpa::SeekPhase::s3Validation)].count == 1);
    CHECK(ledger.phases[index(rpa::SeekPhase::s3Validation)].rootCount == 1);
    CHECK(ledger.phases[index(rpa::SeekPhase::s1HostProtocol)].count == 1);
    CHECK(ledger.phases[index(rpa::SeekPhase::s1HostProtocol)].rootCount == 0);
    CHECK(ledger.seekRootCount == 1);
    CHECK(ledger.seekRootUs <= ledger.envelopeTotalUs);
    CHECK(ledger.unaccountedTotalUs >= 0);
    // Worker samples merge into the host ledger without inventing envelopes.
    rpa::SeekLedger worker{};
    worker.phases[index(rpa::SeekPhase::s4InternalExecution)].count = 2;
    worker.phases[index(rpa::SeekPhase::s4InternalExecution)].totalUs = 30;
    worker.phases[index(rpa::SeekPhase::s4InternalExecution)].maxUs = 20;
    worker.seeksWithoutPhases = 1;
    worker.timerCostNs = 7;
    const auto beforeCount = ledger.phases[index(rpa::SeekPhase::s4InternalExecution)].count;
    const auto beforeTotal = ledger.phases[index(rpa::SeekPhase::s4InternalExecution)].totalUs;
    const auto beforeFetch = ledger.fetchCount;
    rpa::mergeSeek(ledger, worker);
    CHECK(ledger.phases[index(rpa::SeekPhase::s4InternalExecution)].count == beforeCount + 2);
    CHECK(ledger.phases[index(rpa::SeekPhase::s4InternalExecution)].totalUs >= beforeTotal + 30);
    CHECK(ledger.phases[index(rpa::SeekPhase::s4InternalExecution)].maxUs >= 20);
    CHECK(ledger.seeksWithoutPhases == 2);
    CHECK(ledger.timerCostNs == 7);
    CHECK(ledger.seeks == 2);
    rpa::seekFetch(ledger, 5);
    CHECK(ledger.fetchCount == beforeFetch + 1);
    CHECK(ledger.fetchTotalUs >= 5);

    // Playback-side phases reach the owner ledger through the thread-scoped sink only.
    rpa::SeekLedger bound{};
    {
        rpa::SeekEnvelope envelope{&bound};
        rpa::SeekSinkScope bind{&bound};
        rpa::SeekScope outer{rpa::SeekPhase::s2CandidatePrep};
        rpa::SeekScope inner{rpa::SeekPhase::s3Validation};
        inner.finish();
        inner.finish(); // Idempotent: a finished scope never records twice.
    }
    CHECK(bound.seeks == 1);
    CHECK(bound.seeksWithoutPhases == 0);
    CHECK(bound.phases[index(rpa::SeekPhase::s2CandidatePrep)].count == 1);
    CHECK(bound.phases[index(rpa::SeekPhase::s2CandidatePrep)].rootCount == 1);
    CHECK(bound.phases[index(rpa::SeekPhase::s3Validation)].count == 1);
    CHECK(bound.phases[index(rpa::SeekPhase::s3Validation)].rootCount == 0);
    CHECK(bound.depth == 0);
    CHECK(rpa::seekSink == nullptr);

    // A recorded existing fence stays distinguishable from "not checked" on the permit path.
    rpa::Fence permit{};
    permit.path = rpa::FencePath::cleanupPermit;
    permit.applicability = rpa::FenceApplicability::notApplicable;
    rpa::Identity stale{};
    stale.scope = 7;
    stale.generation = 3;
    {
        rpa::FenceScope scope{&permit};
        rpa::noteExisting(stale);
    }
    CHECK(permit.attempted == false);
    CHECK(permit.applicability == rpa::FenceApplicability::notApplicable);
    CHECK(permit.existingFence);
    CHECK(permit.existingIdentity.scope == 7);
    CHECK(permit.existingIdentity.generation == 3);

    // The default record never claims a skipped fence was "not applicable"; only the owner sets
    // either verdict after deciding the path. An existing-fence reuse keeps the old identity and
    // still marks the normal fence as applicable even though it was not attempted.
    rpa::Fence unrecorded{};
    CHECK(unrecorded.applicability == rpa::FenceApplicability::missingEvidence);
    rpa::Fence reused{};
    reused.path = rpa::FencePath::existingFence;
    reused.applicability = rpa::FenceApplicability::applicable;
    {
        rpa::FenceScope scope{&reused};
        rpa::noteExisting(stale);
    }
    CHECK_FALSE(reused.attempted);
    CHECK(reused.applicability == rpa::FenceApplicability::applicable);
    CHECK(reused.existingFence);
    CHECK(reused.existingIdentity.scope == 7);
    CHECK(reused.existingIdentity.generation == 3);
}
#endif

TEST_CASE("B5 tail reply interruption is queried before main closes the new clock scope",
          "[b5][host][fault]") {
    presentation_renderer::TestPresentationRenderer renderer;
    std::atomic<unsigned> pumps{};
    std::atomic<bool> blockOwner{};
    std::promise<void> release;
    auto released = release.get_future().share();
    auto settings = options();
    settings.session.monotonicNow = [&] {
        if (blockOwner.load())
            released.wait();
        return std::chrono::duration_cast<std::chrono::microseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
    };
    const ClockMappingConfiguration mapping{{0, 1}, {0, 1}, {0, 1}, {1, 1}};
    settings.realtime = SessionRealtimeStart{{1000000, 10, 128, "", true}, mapping, {}, "b5.reply"};
    auto wired = ports(pumps);
    bool interrupt{};
    wired.pumpMain = [&]() -> core::Result<void> {
        ++pumps;
        if (interrupt) {
            interrupt = false;
            release.set_value();
            return core::unexpected(
                core::Error{"probe.pump_interrupted", "Injected main wait interruption"});
        }
        return {};
    };
    test_support::SessionTestContent fixture;
    test_support::rpa(fixture, mapping);
    auto content = fixture.content();
    auto host = player::ExecutionHost::create(renderer, settings, std::move(wired));
    REQUIRE(host);
    REQUIRE((*host)->load(test_support::presentation(), content));
    const auto original = (*host)->context();
    ClockObservation eof{};
    eof.monotonicDomain = "b5.reply";
    eof.segment = original.clockSegment;
    eof.publicationSequence = 1;
    eof.sampleMonotonicUs = std::chrono::duration_cast<std::chrono::microseconds>(
                                std::chrono::steady_clock::now().time_since_epoch())
                                .count();
    eof.validUntilMonotonicUs = eof.sampleMonotonicUs + 1000000;
    eof.source = ClockFramePosition{0, 1000000};
    eof.presented = eof.source;
    eof.quality = ClockQuality::Exact;
    eof.playback = ClockPlaybackState::EndOfStream;
    blockOwner.store(true);
    interrupt = true;
    auto tail = (*host)->beginTail(eof, {{1, 1}, 100000, 0, "explicit reply fixture", true});
    REQUIRE_FALSE(tail);
    CHECK(tail.error().code() == "probe.pump_interrupted");
    auto closed = (*host)->close();
    INFO((closed
              ? ""
              : std::string{closed.error().code()} + ": " + std::string{closed.error().message()}));
    REQUIRE(closed);
    CHECK((*host)->context().clockSegment == original.clockSegment + 1);
    CHECK((*host)->state() == HostState::closed);
}
TEST_CASE("B5 deadline retains original tail job until pumped Close completes",
          "[b5][host][fault][deadline]") {
    presentation_renderer::TestPresentationRenderer renderer;
    std::atomic<unsigned> pumps{}, blockedCalls{};
    std::atomic<bool> blockOwner{};
    std::promise<void> release;
    auto released = release.get_future().share();
    auto settings = options();
    settings.replyDeadline = std::chrono::milliseconds{100};
    settings.session.monotonicNow = [&] {
        if (blockOwner.load()) {
            ++blockedCalls;
            // A failed assertion must not leave the owner join blocked indefinitely.
            released.wait();
        }
        return std::chrono::duration_cast<std::chrono::microseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
    };
    const ClockMappingConfiguration mapping{{0, 1}, {0, 1}, {0, 1}, {1, 1}};
    settings.realtime =
        SessionRealtimeStart{{1000000, 10, 128, "", true}, mapping, {}, "b5.deadline"};
    bool didRelease{}, releaseOnPump{};
    auto wired = ports(pumps);
    wired.pumpMain = [&]() -> core::Result<void> {
        ++pumps;
        if (releaseOnPump && !didRelease) {
            didRelease = true;
            release.set_value();
        }
        return {};
    };
    test_support::SessionTestContent fixture;
    test_support::rpa(fixture, mapping);
    auto host = player::ExecutionHost::create(renderer, settings, std::move(wired));
    REQUIRE(host);
    struct ReleaseOnExit {
        std::promise<void>& promise;
        bool& done;
        ~ReleaseOnExit() {
            if (!done) {
                done = true;
                promise.set_value();
            }
        }
    } releaseOnExit{release, didRelease};
    REQUIRE((*host)->load(test_support::presentation(), fixture.content()));
    const auto original = (*host)->context();
    ClockObservation eof{};
    eof.monotonicDomain = "b5.deadline";
    eof.segment = original.clockSegment;
    eof.publicationSequence = 1;
    eof.sampleMonotonicUs = std::chrono::duration_cast<std::chrono::microseconds>(
                                std::chrono::steady_clock::now().time_since_epoch())
                                .count();
    eof.validUntilMonotonicUs = eof.sampleMonotonicUs + 1000000;
    eof.source = eof.presented = ClockFramePosition{0, 1000000};
    eof.quality = ClockQuality::Exact;
    eof.playback = ClockPlaybackState::EndOfStream;
    blockOwner.store(true);
    const auto beforeWait = std::chrono::steady_clock::now();
    auto tail = (*host)->beginTail(eof, {{1, 1}, 100000, 0, "explicit deadline fixture", true});
    REQUIRE_FALSE(tail);
    CHECK(tail.error().code() == "host.rpa.control_pending");
    CHECK(std::chrono::steady_clock::now() - beforeWait >= settings.replyDeadline);
    REQUIRE(blockedCalls.load() > 0);
    const auto beforeClose = pumps.load();
    auto pending = (*host)->close();
    REQUIRE_FALSE(pending);
    CHECK(pending.error().code() == "host.rpa.control_pending");
    CHECK((*host)->state() != HostState::closed);
    CHECK(pumps.load() > beforeClose);
    releaseOnPump = true;
    auto closed = (*host)->close();
    // An expired original transport deadline is not restarted by a new owner call. Continue
    // nonblocking cleanup under the same Close; this guard is a test deadlock limit, not a budget.
    const auto drainLimit = std::chrono::steady_clock::now() + std::chrono::seconds{5};
    while (!closed && (*host)->state() == HostState::closing &&
           std::chrono::steady_clock::now() < drainLimit) {
        (*host)->waitFor(std::chrono::milliseconds{1});
        closed = (*host)->close();
    }
    INFO((closed
              ? ""
              : std::string{closed.error().code()} + ": " + std::string{closed.error().message()}));
    REQUIRE(closed);
    CHECK(didRelease);
    // No second Tail request: cleanup collected its original reply exactly once.
    CHECK((*host)->context().clockSegment == original.clockSegment + 1);
    CHECK((*host)->state() == HostState::closed);
    REQUIRE((*host)->close());
}
TEST_CASE("B5 host rejects unaccepted budgets and closes CPU preparation failure", "[b5][host]") {
    presentation_renderer::TestPresentationRenderer renderer;
    std::atomic<unsigned> pumps{};
    auto unaccepted = options();
    unaccepted.session.testOnly = false;
    REQUIRE_FALSE(player::ExecutionHost::create(renderer, unaccepted, ports(pumps)));
    auto host = player::ExecutionHost::create(renderer, options(), ports(pumps));
    REQUIRE(host);
    auto loaded = (*host)->load(PlaybackSource{});
    REQUIRE_FALSE(loaded);
    REQUIRE((*host)->state() == HostState::suspended);
    REQUIRE((*host)->close());
    CHECK((*host)->state() == HostState::closed);
}

TEST_CASE("R1 C6 failed accepted Load cleans its own identity before independent Close",
          "[rpa][r1][c6][host]") {
    presentation_renderer::TestPresentationRenderer renderer;
    std::atomic<unsigned> pumps{};
    auto host = player::ExecutionHost::create(renderer, options(), ports(pumps));
    REQUIRE(host);
    // Fail renderer prepare after D2 Load acceptance and software prepare, before the software
    // fence. This is a deterministic test renderer fault, not evidence about a real device.
    renderer.markDeviceLost();
    auto loaded = (*host)->load(test_support::presentation());
    REQUIRE_FALSE(loaded);
    const std::string originalError{loaded.error().code()};
    CHECK(originalError == "host.rpa.context_stale");
    CHECK((*host)->state() == HostState::suspended);
    auto closed = (*host)->close();
    INFO((closed ? "" : std::string{closed.error().code()}));
    REQUIRE(closed);
    CHECK((*host)->state() == HostState::closed);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    namespace obs = cuexis::internal::execution_diagnostics::rpa;
    const auto& seen = (*host)->rpaObservation();
    CHECK(seen.cleanupFence.path == obs::FencePath::cleanupPermit);
    CHECK(seen.cleanupFence.permitAcceptedControl);
    CHECK(seen.cleanupFence.retireApplied);
    CHECK(seen.cleanupFence.referencesReleased);
    CHECK_FALSE(seen.cleanupFence.existingFence);
    CHECK(seen.cleanupFence.identity.kind == static_cast<std::uint8_t>(ControlKind::load));
    CHECK(seen.cleanupFence.identity.operationId == 1);
    CHECK(seen.closeRetireFence.identity.operationId == 2);
    CHECK(seen.fence.identity.operationId == 2);
    CHECK(seen.cleanupFence.identity.decisionOrdinal < seen.fence.identity.decisionOrdinal);
    CHECK(seen.attempts == 1);
    CHECK(seen.reentries == 0);
    CHECK(seen.fence.closeState == obs::CloseStepState::applied);
    CHECK(seen.workerExited);
    CHECK(seen.workerJoined);
    CHECK(seen.closedFlag);
#endif
    CHECK(loaded.error().code() == originalError);
    REQUIRE((*host)->close());
}

TEST_CASE("B5 full capture queue preserves control cut space and owning fault evidence",
          "[b5][host][fault]") {
    presentation_renderer::TestPresentationRenderer renderer;
    std::atomic<unsigned> pumps{};
    auto settings = options();
    settings.captureCapacity = 4;
    settings.captureReserve = 1;
    const ClockMappingConfiguration mapping{{0, 1}, {0, 1}, {0, 1}, {1, 1}};
    settings.realtime = SessionRealtimeStart{{1000000, 10, 128, "", true}, mapping, {}, "b5.full"};
    test_support::SessionTestContent fixture;
    test_support::rpa(fixture, mapping);
    auto host = player::ExecutionHost::create(renderer, settings, ports(pumps));
    REQUIRE(host);
    REQUIRE((*host)->load(test_support::presentation(), fixture.content()));
    for (std::int64_t tick = 0; tick < 3; ++tick) {
        auto event = test_support::input();
        event.observationTick = {tick};
        event.sequence = static_cast<std::uint64_t>(tick + 1);
        REQUIRE((*host)->capture(event));
    }
    auto event = test_support::input();
    event.observationTick = {3};
    event.sequence = 4;
    REQUIRE_FALSE((*host)->capture(event));
    CHECK((*host)->state() == HostState::suspended);
    REQUIRE((*host)->close());
    REQUIRE((*host)->closeEvidence());
    CHECK((*host)->closeEvidence()->pending().size() == 4);
    REQUIRE((*host)->closeEvidence()->tentativeTickMembers().size() == 1);
    CHECK((*host)->closeEvidence()->tentativeTickMembers()[0].input.sequence == 4);
    CHECK((*host)->state() == HostState::closed);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    // The capture queue fault suspends the host before Close. Cleanup queries the suspended
    // coordinator but must not suspend it a second time; the directed-cleanup permit checks run,
    // yet the completed Close still enters the normal software fence rather than the permit
    // bypass. Four unacked deliveries become evidence, so none may remain unaccounted.
    namespace rpa = cuexis::internal::execution_diagnostics::rpa;
    const auto& fullClose = (*host)->rpaObservation();
    CHECK(fullClose.attempts == 1);
    CHECK(fullClose.reentries == 0);
    CHECK(fullClose.firstCause == rpa::Reason::none);
    CHECK(fullClose.closeRetireFence.path == rpa::FencePath::normalControl);
    CHECK(fullClose.closeRetireFence.permitAttempted);
    CHECK(fullClose.closeRetireFence.permitAcceptedControl);
    CHECK(fullClose.closeRetireFence.permitDirected);
    CHECK_FALSE(fullClose.closeRetireFence.existingFence);
    CHECK(fullClose.closeRetireFence.succeeded);
    // cleanup() queries the coordinator, not the host facade. Depending on whether the failed
    // capture reached the coordinator's suspend, the original control is either complete or
    // suspended; only a non-terminal status may be suspended.
    const auto queryStatus = fullClose.cleanupQueryStatus;
    CHECK((queryStatus == static_cast<std::uint8_t>(ControlStatus::complete) ||
           queryStatus == static_cast<std::uint8_t>(ControlStatus::suspended)));
    CHECK(fullClose.cleanupSuspendCalled ==
          (queryStatus != static_cast<std::uint8_t>(ControlStatus::complete)));
    CHECK(fullClose.cleanupWaitSamples >= 1);
    CHECK(fullClose.unackedDeliveries == fullClose.evidenceDeliveries);
    CHECK(fullClose.captureTerminal == 2);
    CHECK(fullClose.closedFlag);
#endif
}

TEST_CASE("R0 close flush fault records delivery cause and still completes",
          "[rpa][r0][observation][b5]") {
    presentation_renderer::TestPresentationRenderer renderer;
    std::atomic<unsigned> pumps{};
    auto settings = options();
    const ClockMappingConfiguration mapping{{0, 1}, {0, 1}, {0, 1}, {1, 1}};
    settings.realtime = SessionRealtimeStart{{1000000, 10, 128, "", true}, mapping, {}, "b5.flush"};
    test_support::SessionTestContent fixture;
    test_support::rpa(fixture, mapping);
    auto host = player::ExecutionHost::create(renderer, settings, ports(pumps));
    REQUIRE(host);
    REQUIRE((*host)->load(test_support::presentation(), fixture.content()));
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    namespace rpa = cuexis::internal::execution_diagnostics::rpa;
    rpa::testFlushFaultArmed.store(true);
#endif
    auto closed = (*host)->close();
    INFO((closed
              ? ""
              : std::string{closed.error().code()} + ": " + std::string{closed.error().message()}));
    REQUIRE(closed);
    CHECK((*host)->state() == HostState::closed);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    const auto& observed = (*host)->rpaObservation();
    CHECK(observed.firstCause == rpa::Reason::deliveryFailed);
    CHECK(observed.firstCauseStage == rpa::CloseStage::closeFlush);
    CHECK(std::string{observed.firstErrorCode} == "host.rpa.capacity_exceeded");
    CHECK(observed.coordinatorBeforeSuspend == static_cast<std::uint8_t>(HostState::closing));
    // HostCoordinator::suspend() marks a Close receipt suspended but keeps HostState::closing,
    // because only a non-Close control can return the coordinator to HostState::suspended. The
    // delivery failure is still recorded as the first cause and the Close still completes.
    CHECK(observed.coordinatorAfterSuspend == static_cast<std::uint8_t>(HostState::closing));
    CHECK(observed.attempts == 1);
    CHECK(observed.captureTerminal == 2);
    CHECK(observed.softwareTerminal == 2);
    CHECK(observed.errors == 1);
    CHECK(observed.unackedDeliveries == observed.evidenceDeliveries);
    CHECK(observed.closedFlag);
#else
    CHECK(true);
#endif
}

TEST_CASE("R1 close export failure preserves its cause while the original Close completes",
          "[rpa][r1][observation][b5]") {
    presentation_renderer::TestPresentationRenderer renderer;
    std::atomic<unsigned> pumps{};
    auto settings = options();
    const ClockMappingConfiguration mapping{{0, 1}, {0, 1}, {0, 1}, {1, 1}};
    settings.realtime =
        SessionRealtimeStart{{1000000, 10, 128, "", true}, mapping, {}, "b5.export"};
    test_support::SessionTestContent fixture;
    test_support::rpa(fixture, mapping);
    auto host = player::ExecutionHost::create(renderer, settings, ports(pumps));
    REQUIRE(host);
    REQUIRE((*host)->load(test_support::presentation(), fixture.content()));
#if !defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    // The fault hook is compiled out in the OFF build, so this case only checks that the same
    // public Close path still completes without observation code.
    CHECK((*host)->close());
#else
    namespace rpa = cuexis::internal::execution_diagnostics::rpa;
    rpa::testExportFaultArmed.store(true);
    auto first = (*host)->close();
    REQUIRE_FALSE(first);
    CHECK(first.error().code() == "host.rpa.capacity_exceeded");
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    CHECK((*host)->rpaObservation().firstCause == rpa::Reason::operationFailed);
    CHECK((*host)->rpaObservation().firstCauseStage == rpa::CloseStage::exportEvidence);
    CHECK((*host)->rpaObservation().attempts == 1);
#endif
    auto second = (*host)->close();
    INFO((second
              ? ""
              : std::string{second.error().code()} + ": " + std::string{second.error().message()}));
    REQUIRE(second);
    CHECK((*host)->state() == HostState::closed);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    const auto& observed = (*host)->rpaObservation();
    CHECK(observed.attempts == 2);
    CHECK(observed.reentries == 1);
    CHECK(observed.firstCause == rpa::Reason::operationFailed);
    CHECK(observed.firstCauseStage == rpa::CloseStage::exportEvidence);
    CHECK(std::string{observed.firstErrorCode} == "host.rpa.capacity_exceeded");
    CHECK(observed.closedFlag);
#endif
#endif
}

#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
TEST_CASE("R1 Close resumes its original export phase after an allocation rejection",
          "[rpa][r1][continuation][host]") {
    namespace obs = cuexis::internal::execution_diagnostics::rpa;
    presentation_renderer::TestPresentationRenderer renderer;
    std::atomic<unsigned> pumps{};
    auto settings = options();
    const ClockMappingConfiguration mapping{{0, 1}, {0, 1}, {0, 1}, {1, 1}};
    settings.realtime =
        SessionRealtimeStart{{1000000, 10, 128, "", true}, mapping, {}, "r1.export"};
    auto fixture = std::make_unique<test_support::SessionTestContent>();
    test_support::rpa(*fixture, mapping);
    auto host = player::ExecutionHost::create(renderer, settings, ports(pumps));
    REQUIRE(host);
    REQUIRE((*host)->load(test_support::presentation(), fixture->content()));
    obs::testExportFaultArmed.store(true);
    const auto first = (*host)->close();
    REQUIRE_FALSE(first);
    CHECK((*host)->state() == HostState::closing);
    const auto original = (*host)->rpaObservation().identity;
    const auto second = (*host)->close();
    INFO((second ? "" : std::string{second.error().code()}));
    REQUIRE(second);
    CHECK((*host)->state() == HostState::closed);
    CHECK((*host)->rpaObservation().identity.operationId == original.operationId);
    CHECK((*host)->rpaObservation().firstDecisionOrdinal == original.decisionOrdinal);
    CHECK(std::string{(*host)->rpaObservation().firstErrorCode} == first.error().code());
}
#endif

TEST_CASE("R0 public operation transcript is identical with observation on and off",
          "[rpa][r0][equivalence]") {
    // This is a public-operation declaration trace, not a byte trace of every internal table.
    // The same test binary body runs against an observation-ON and an observation-OFF build; the
    // emitted digest must match. The fixed-rate clock makes the 32 advance horizons independent of
    // wall time, so the trace covers load/admission context, control admission, canonical Replay
    // bytes, ReplayEvaluation, FrameSnapshot and errors under one repeatable schedule.
    presentation_renderer::TestPresentationRenderer renderer;
    std::atomic<unsigned> pumps{};
    auto settings = options();
    const ClockMappingConfiguration mapping{{0, 1}, {0, 1}, {0, 1}, {1, 1000000}};
    settings.realtime =
        SessionRealtimeStart{{1000000, 10, 128, "", true}, mapping, {}, "b5.equivalence"};
    test_support::SessionTestContent fixture;
    test_support::rpa(fixture, mapping);
    auto host = player::ExecutionHost::create(renderer, settings, ports(pumps));
    REQUIRE(host);

    cuexis::core::detail::Sha256 sha;
    const auto addText = [&](std::string_view text) {
        sha.update(std::as_bytes(std::span{text.data(), text.size()}));
    };
    const auto addBytes = [&](std::span<const std::byte> bytes) { sha.update(bytes); };
    const auto addU64 = [&](std::uint64_t value) {
        std::array<std::byte, 8> bytes{};
        for (unsigned index = 0; index < 8; ++index)
            bytes[index] = static_cast<std::byte>((value >> (index * 8)) & 0xFFU);
        sha.update(bytes);
    };
    const auto addFloat = [&](float value) { addU64(std::bit_cast<std::uint32_t>(value)); };
    const auto addDouble = [&](double value) { addU64(std::bit_cast<std::uint64_t>(value)); };
    const auto addResource = [&](const std::optional<PresentationResourceRef>& resource) {
        addU64(resource.has_value());
        if (!resource)
            return;
        addU64(static_cast<std::uint64_t>(resource->type));
        addText(resource->assetId);
        addBytes(std::as_bytes(std::span{resource->identity.sha256}));
    };

    auto early = (*host)->resize({0, 200});
    const std::string earlyCode{early ? std::string_view{"ok"} : early.error().code()};
    addText(earlyCode);
    std::cout << "R0EQ.resize=" << earlyCode << "\n";

    auto loaded = (*host)->load(test_support::presentation(), fixture.content());
    REQUIRE(loaded);
    const auto context = (*host)->context();
    // sessionScope is a process-global allocation ordinal, not a semantic declaration. Hash only
    // the fact that a scope exists so the transcript is independent of suite order.
    addU64(context.sessionScope != 0);
    addU64(context.runEpoch);
    addU64(context.clockSegment);
    addU64(static_cast<std::uint64_t>((*host)->state()));
    std::cout << "R0EQ.load=" << context.sessionScope << "," << context.runEpoch << ","
              << context.clockSegment << "," << static_cast<unsigned>((*host)->state()) << "\n";

    // A capture would start its own control. The tail transcript starts from the load admission
    // boundary, so no capture is issued before beginTail().
    std::cout << "R0EQ.capture=skipped\n";

    ClockObservation clock{};
    clock.monotonicDomain = "b5.equivalence";
    clock.segment = context.clockSegment;
    clock.publicationSequence = 1;
    clock.sampleMonotonicUs = std::chrono::duration_cast<std::chrono::microseconds>(
                                  std::chrono::steady_clock::now().time_since_epoch())
                                  .count();
    clock.validUntilMonotonicUs = clock.sampleMonotonicUs + 1000000;
    clock.playback = ClockPlaybackState::Playing;
    clock.quality = ClockQuality::Exact;
    std::uint64_t executed{};
    std::uint64_t failed{};
    std::int64_t lastHorizon{};
    for (std::int64_t tick = 1; tick <= 32; ++tick) {
        clock.publicationSequence = static_cast<std::uint64_t>(tick);
        clock.source = ClockFramePosition{tick, 1};
        clock.presented = clock.source;
        auto advanced = (*host)->advance(clock);
        if (!advanced) {
            addText(advanced.error().code());
            ++failed;
            continue;
        }
        ++executed;
        lastHorizon = advanced->request.horizon.value;
        addU64(static_cast<std::uint64_t>(advanced->disposition));
        addU64(advanced->request.horizon.value);
    }
    addU64(executed);
    addU64(failed);
    std::cout << "R0EQ.advance=" << executed << "," << failed << "," << lastHorizon << "\n";

    auto replay = (*host)->archive();
    REQUIRE(replay);
    auto cut = replay->cutAt(GameplayTick{lastHorizon});
    REQUIRE(cut);
    addU64(cut->completeRecords);
    std::cout << "R0EQ.records=" << cut->completeRecords << "\n";
    auto replayBytes = replay->toBytes();
    REQUIRE(replayBytes);
    addBytes(*replayBytes);
    const auto digestBytes = [](std::span<const std::byte> bytes) {
        const auto digest = cuexis::core::detail::sha256(bytes);
        static constexpr char hex[] = "0123456789abcdef";
        std::string text;
        text.reserve(64);
        for (const auto byte : digest) {
            const auto value = static_cast<unsigned>(byte);
            text.push_back(hex[value >> 4U]);
            text.push_back(hex[value & 0x0FU]);
        }
        return text;
    };
    std::cout << "R0EQ.replay=" << digestBytes(*replayBytes) << ",bytes=" << replayBytes->size()
              << "\n";

    auto evaluated = (*host)->evaluateReplay(*replay);
    REQUIRE(evaluated);
    REQUIRE(evaluated->evidenceValid);
    addU64(evaluated->evidenceValid);
    auto evalScore = evaluated->result.score();
    REQUIRE(evalScore);
    addU64(static_cast<std::uint64_t>(evalScore->score));
    addU64(evalScore->misses);
    auto evalFacts = evaluated->result.publishableFactCount();
    REQUIRE(evalFacts);
    addU64(*evalFacts);
    std::cout << "R0EQ.evaluation=" << evalScore->score << "," << evalScore->misses << ","
              << *evalFacts << "\n";

    auto frame = (*host)->sample(
        lastHorizon, {static_cast<double>(lastHorizon) / 1000.0, 0, context.clockSegment}, false);
    REQUIRE(frame);
    const auto& snapshot = frame->snapshotView();
    addU64(snapshot.objects.size());
    for (const auto& object : snapshot.objects) {
        addText(object.id);
        addU64(object.hasTransform);
        addU64(object.visible);
        for (const auto value : object.worldMatrix)
            addFloat(value);
        addText(object.materialAssetId);
        addResource(object.mesh);
        addResource(object.material);
        addDouble(object.materialOpacity);
        for (const auto value : object.materialTint)
            addFloat(value);
    }
    addU64(snapshot.camera.active);
    for (const auto value : snapshot.camera.viewMatrix)
        addFloat(value);
    for (const auto value : snapshot.camera.projectionMatrix)
        addFloat(value);
    addDouble(snapshot.camera.fovY);
    addDouble(snapshot.camera.nearPlane);
    addDouble(snapshot.camera.farPlane);
    addDouble(snapshot.camera.pitch);
    addDouble(snapshot.camera.yaw);
    addDouble(snapshot.camera.roll);
    addFloat(snapshot.clearRed);
    addFloat(snapshot.clearGreen);
    addFloat(snapshot.clearBlue);
    addFloat(snapshot.clearAlpha);
    addU64(snapshot.viewportWidth);
    addU64(snapshot.viewportHeight);
    std::cout << "R0EQ.snapshot=" << snapshot.objects.size() << "," << snapshot.viewportWidth << "x"
              << snapshot.viewportHeight << "\n";

    auto presented = (*host)->present(*frame);
    REQUIRE(presented);
    addU64(static_cast<std::uint64_t>(presented->status));
    std::cout << "R0EQ.present=" << static_cast<unsigned>(presented->status) << "\n";

    auto closed = (*host)->close();
    INFO((closed
              ? ""
              : std::string{closed.error().code()} + ": " + std::string{closed.error().message()}));
    REQUIRE(closed);
    addU64(static_cast<std::uint64_t>((*host)->state()));
    auto afterClose = (*host)->capture(test_support::input());
    const std::string afterCode{afterClose ? std::string_view{"ok"} : afterClose.error().code()};
    addText(afterCode);
    auto secondClose = (*host)->close();
    addU64(secondClose.has_value());
    std::cout << "R0EQ.close=" << static_cast<unsigned>((*host)->state()) << "," << afterCode << ","
              << (secondClose ? 1 : 0) << "\n";

    const auto digest = sha.finish();
    static constexpr char hex[] = "0123456789abcdef";
    std::string text;
    text.reserve(digest.size() * 2);
    for (const auto byte : digest) {
        const auto value = static_cast<unsigned>(byte);
        text.push_back(hex[value >> 4U]);
        text.push_back(hex[value & 0x0FU]);
    }
    std::cout << "R0EQ.digest=" << text << "\n";
    // Golden transcript for the observation-ON/OFF pair. A mismatch means the two builds no
    // longer declare the same public operation sequence.
    CHECK(text == "b4608fbfd7a89706d5c982fa8e57132e4dbb12abde7fa019cc762bbf6c1a7bf2");
}

TEST_CASE("R1 Close interruption continues each original acceptance step confirmation and exit",
          "[rpa][r1][continuation][phases]") {
    using Phase = player::ExecutionClosePhase;
    const auto stop =
        GENERATE(Phase::postAcceptance, Phase::awaitAcceptance, Phase::acceptReplyReady,
                 Phase::softwareRetire, Phase::softwareClose, Phase::confirm, Phase::exitWait);
    CAPTURE(static_cast<unsigned>(stop));
    presentation_renderer::TestPresentationRenderer renderer;
    std::atomic<unsigned> pumps{}, accepts{}, retires{}, closes{}, confirms{};
    auto settings = options();
    settings.session.beforeDispatch = [&](std::uint64_t, const SessionCommand& command) {
        if (const auto* value = std::get_if<SessionAcceptControl>(&command);
            value && value->request.kind == ControlKind::close)
            ++accepts;
        if (const auto* value = std::get_if<SessionSoftwareRetire>(&command);
            value && value->proof.request.kind == ControlKind::close)
            ++retires;
        if (std::holds_alternative<SessionSoftwareClose>(command))
            ++closes;
        if (const auto* value = std::get_if<SessionConfirmControl>(&command);
            value && value->proof.request.kind == ControlKind::close)
            ++confirms;
    };
    auto wired = ports(pumps);
    player::ExecutionHost* live{};
    bool interrupted{};
    std::array<unsigned, 32> successful{};
    wired.beforeClosePhase = [&](Phase phase) -> core::Result<void> {
        if (live)
            CHECK(live->state() == HostState::closing);
        if (!interrupted && phase == stop) {
            interrupted = true;
            return core::unexpected(core::Error{"probe.close_pause", "Controlled Close boundary"});
        }
        ++successful[static_cast<unsigned>(phase)];
        return {};
    };
    auto made = player::ExecutionHost::create(renderer, settings, std::move(wired));
    REQUIRE(made);
    live = made->get();
    REQUIRE(live->load(test_support::presentation()));
    const auto original = live->context();
    const auto first = live->close();
    REQUIRE_FALSE(first);
    REQUIRE(interrupted);
    CHECK(first.error().code() == "probe.close_pause");
    CHECK(live->state() == HostState::closing);
    CHECK(live->closePhase() == (stop == Phase::acceptReplyReady ? Phase::awaitAcceptance : stop));
    const auto second = live->close();
    INFO((second ? "" : std::string{second.error().code()}));
    REQUIRE(second);
    CHECK(live->state() == HostState::closed);
    CHECK(live->closePhase() == Phase::joined);
    CHECK(live->context().controlSequence == original.controlSequence + 1);
    CHECK(accepts == 1);
    CHECK(retires == 1);
    CHECK(closes == 1);
    CHECK(confirms == 1);
    for (auto once :
         {Phase::prepareControl, Phase::postAcceptance, Phase::cut, Phase::softwareRetire,
          Phase::softwareClose, Phase::finish, Phase::confirm, Phase::shutdown})
        CHECK(successful[static_cast<unsigned>(once)] == 1);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    CHECK(live->rpaObservation().firstDecisionOrdinal != 0);
    CHECK(live->rpaObservation().identity.operationId == 2);
    CHECK(std::string{live->rpaObservation().firstErrorCode} == "probe.close_pause");
    CHECK(live->rpaObservation().workerExited);
    CHECK(live->rpaObservation().workerJoined);
#endif
    REQUIRE(live->close());
    CHECK(confirms == 1);
}

TEST_CASE("R1 Close consumes the original archive or inspection reply exactly once",
          "[rpa][r1][continuation][old-job]") {
    const bool archive = GENERATE(false, true);
    const bool ownerFailure = GENERATE(false, true);
    presentation_renderer::TestPresentationRenderer renderer;
    std::atomic<unsigned> pumps{}, oldDispatches{}, oldAcks{}, accepts{};
    std::atomic<bool> armed{}, entered{};
    std::atomic<std::uint64_t> oldId{};
    std::promise<void> release;
    auto released = release.get_future().share();
    bool didRelease{};
    const auto unblock = [&] {
        if (!didRelease) {
            didRelease = true;
            release.set_value();
        }
    };
    auto settings = options();
    const ClockMappingConfiguration mapping{{0, 1}, {0, 1}, {0, 1}, {1, 1}};
    settings.realtime = SessionRealtimeStart{{1000000, 10, 128, "", true}, mapping, {}, "r1.old"};
    settings.session.beforeDispatch = [&](std::uint64_t id, const SessionCommand& command) {
        if (const auto* control = std::get_if<SessionAcceptControl>(&command);
            control && control->request.kind == ControlKind::close)
            ++accepts;
        const bool selected = archive ? std::holds_alternative<SessionArchive>(command)
                                      : std::holds_alternative<SessionInspect>(command);
        if (selected && armed.exchange(false)) {
            oldId = id;
            ++oldDispatches;
            entered = true;
            released.wait();
            if (ownerFailure)
                throw std::bad_alloc{};
        }
    };
    settings.session.replyAcknowledged = [&](std::uint64_t id) {
        if (id == oldId && oldId != 0)
            ++oldAcks;
    };
    auto wired = ports(pumps);
    wired.pumpMain = [&]() -> core::Result<void> {
        ++pumps;
        if (entered && !didRelease)
            return core::unexpected(
                core::Error{"probe.old_job", "Original typed reply remains in flight"});
        return {};
    };
    wired.beforeClosePhase = [&](player::ExecutionClosePhase phase) -> core::Result<void> {
        if (phase == player::ExecutionClosePhase::cleanup)
            unblock();
        return {};
    };
    auto fixture = std::make_unique<test_support::SessionTestContent>();
    test_support::rpa(*fixture, mapping);
    auto made = player::ExecutionHost::create(renderer, settings, std::move(wired));
    REQUIRE(made);
    struct ReleaseGuard {
        std::function<void()> release;
        ~ReleaseGuard() {
            release();
        }
    } guard{unblock};
    REQUIRE((*made)->load(test_support::presentation(), fixture->content()));
    const auto original = (*made)->context();
    armed = true;
    if (archive) {
        const auto result = (*made)->archive();
        REQUIRE_FALSE(result);
        CHECK(result.error().code() == "probe.old_job");
    } else {
        const auto result = (*made)->pause(player::ExecutionHost::ClockSource{
            [](const HostContext&) -> core::Result<ClockObservation> {
                return core::unexpected(
                    core::Error{"probe.unused", "Inspection must finish first"});
            }});
        REQUIRE_FALSE(result);
        CHECK(result.error().code() == "probe.old_job");
    }
    CHECK(entered);
    CHECK(oldAcks == 0);
    REQUIRE((*made)->close());
    CHECK(oldDispatches == 1);
    CHECK(oldAcks == 1);
    CHECK(accepts == 1);
    CHECK((*made)->context().controlSequence == original.controlSequence + 1);
    CHECK((*made)->state() == HostState::closed);
    if (ownerFailure) {
        REQUIRE((*made)->closeFirstError());
        CHECK((*made)->closeFirstError()->code() == "host.rpa.capacity_exceeded");
    }
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    CHECK_FALSE((*made)->rpaObservation().cleanupFence.identity.valid);
    CHECK((*made)->rpaObservation().identity.operationId == 2);
#endif
    REQUIRE((*made)->archive());
}

TEST_CASE("R1 Close clock starts at zero and includes join at the fixed 1000 ms boundary",
          "[rpa][r1][budget][clock]") {
    const auto elapsed = GENERATE(std::int64_t{1000000}, std::int64_t{1000001});
    presentation_renderer::TestPresentationRenderer renderer;
    std::atomic<unsigned> pumps{};
    std::int64_t logicalUs{};
    auto wired = ports(pumps);
    wired.closeNow = [&] {
        return std::chrono::steady_clock::time_point{std::chrono::microseconds{logicalUs}};
    };
    player::ExecutionHost* live{};
    wired.beforeClosePhase = [&](player::ExecutionClosePhase phase) -> core::Result<void> {
        if (phase == player::ExecutionClosePhase::exitWait) {
            // Establish actual exit independently of the fake elapsed time. This is a deadlock
            // guard for the test transport, not the Close latency measurement.
            const auto guard = std::chrono::steady_clock::now() + std::chrono::seconds{5};
            while (!live->closeOwnersExited() && std::chrono::steady_clock::now() < guard)
                live->waitFor(std::chrono::milliseconds{1});
            REQUIRE(live->closeOwnersExited());
            CHECK(live->state() == HostState::closing);
            logicalUs = elapsed;
        }
        return {};
    };
    auto made = player::ExecutionHost::create(renderer, options(), std::move(wired));
    REQUIRE(made);
    live = made->get();
    REQUIRE(live->load(test_support::presentation()));
    const auto closed = live->close();
    CHECK(bool(closed) == (elapsed == 1000000));
    CHECK(live->state() == HostState::closed);
    CHECK(live->closeBudgetExceeded() == (elapsed > 1000000));
    CHECK(live->closeFirstError().has_value() == (elapsed > 1000000));
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    CHECK(live->rpaObservation().requestedUs == 0);
    CHECK(live->rpaObservation().deadlineUs == 1000000);
    CHECK(live->rpaObservation().attempts == 1);
    CHECK(live->rpaObservation().workerJoined);
#endif
    REQUIRE(live->close());
    CHECK(live->closeBudgetExceeded() == (elapsed > 1000000));
}

TEST_CASE("R1 expired total or original transport deadline retains a pumpable Close and captures",
          "[rpa][r1][budget][continuation][clock]") {
    const bool totalBudget = GENERATE(false, true);
    presentation_renderer::TestPresentationRenderer renderer;
    std::atomic<unsigned> pumps{}, accepts{}, retires{}, confirms{};
    std::atomic<bool> entered{};
    std::int64_t logicalUs{};
    bool shifted{}, released{};
    std::promise<void> release;
    auto ready = release.get_future().share();
    auto settings = options();
    settings.replyDeadline = totalBudget ? std::chrono::seconds{5} : std::chrono::milliseconds{100};
    settings.session.beforeDispatch = [&](std::uint64_t, const SessionCommand& command) {
        if (const auto* value = std::get_if<SessionAcceptControl>(&command);
            value && value->request.kind == ControlKind::close) {
            ++accepts;
            entered = true;
            ready.wait();
        }
        if (std::holds_alternative<SessionSoftwareRetire>(command))
            ++retires;
        if (const auto* value = std::get_if<SessionConfirmControl>(&command);
            value && value->proof.request.kind == ControlKind::close)
            ++confirms;
    };
    auto wired = ports(pumps);
    wired.closeNow = [&] {
        return std::chrono::steady_clock::time_point{std::chrono::microseconds{logicalUs}};
    };
    wired.beforeClosePhase = [&](player::ExecutionClosePhase phase) -> core::Result<void> {
        if (!shifted && phase == player::ExecutionClosePhase::awaitAcceptance) {
            const auto guard = std::chrono::steady_clock::now() + std::chrono::seconds{5};
            while (!entered && std::chrono::steady_clock::now() < guard)
                std::this_thread::yield();
            REQUIRE(entered);
            logicalUs = totalBudget ? 1000001 : 100001;
            shifted = true;
        }
        return {};
    };
    auto made = player::ExecutionHost::create(renderer, settings, std::move(wired));
    REQUIRE(made);
    struct ReleaseGuard {
        std::promise<void>& promise;
        bool& released;
        ~ReleaseGuard() {
            if (!released) {
                released = true;
                promise.set_value();
            }
        }
    } releaseGuard{release, released};
    REQUIRE((*made)->load(test_support::presentation()));
    const auto first = (*made)->close();
    REQUIRE_FALSE(first);
    CHECK(first.error().code() == "host.rpa.control_pending");
    CHECK((*made)->state() == HostState::closing);
    CHECK((*made)->closePhase() == player::ExecutionClosePhase::awaitAcceptance);
    CHECK((*made)->closeBudgetExceeded() == totalBudget);
    const auto pumped = pumps.load();
    CHECK_FALSE((*made)->close());
    CHECK(pumps > pumped); // Expired calls still pump events without restarting a wait.
    CHECK(accepts == 1);
    released = true;
    release.set_value();
    const auto guard = std::chrono::steady_clock::now() + std::chrono::seconds{5};
    while ((*made)->state() != HostState::closed && std::chrono::steady_clock::now() < guard) {
        const auto result = (*made)->close();
        if (!result && (*made)->state() != HostState::closed)
            (*made)->waitFor(std::chrono::milliseconds{1});
    }
    REQUIRE((*made)->state() == HostState::closed);
    CHECK((*made)->closeOwnersExited());
    CHECK((*made)->closeBudgetExceeded() == totalBudget);
    REQUIRE((*made)->closeFirstError());
    CHECK((*made)->closeFirstError()->code() == first.error().code());
    CHECK(accepts == 1);
    CHECK(retires == 1);
    CHECK(confirms == 1);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    CHECK((*made)->rpaObservation().requestedUs == 0);
    CHECK((*made)->rpaObservation().deadlineUs == 1000000);
    CHECK(std::string{(*made)->rpaObservation().firstErrorCode} == first.error().code());
#endif
}

TEST_CASE("R1 application lifetime drains while callback captures survive exception unwinding",
          "[rpa][r1][lifetime][clock]") {
    struct Unwind {};
    std::atomic<bool> alive{}, entered{};
    std::atomic<unsigned> accessesAfterDestruction{}, pumps{}, accepts{}, confirms{};
    std::promise<void> release;
    auto ready = release.get_future().share();
    bool unwinding{}, released{}, shifted{};
    std::int64_t logicalUs{};
    std::string originalError;
    std::unique_ptr<player::ExecutionHost> host;
    presentation_renderer::TestPresentationRenderer renderer;
    try {
        struct Captures {
            std::atomic<bool>& alive;
            explicit Captures(std::atomic<bool>& value) : alive(value) {
                alive = true;
            }
            ~Captures() {
                alive = false;
            }
        } captures{alive};
        auto settings = options();
        settings.session.beforeDispatch = [&](std::uint64_t, const SessionCommand& command) {
            if (!alive)
                ++accessesAfterDestruction;
            if (const auto* value = std::get_if<SessionAcceptControl>(&command);
                value && value->request.kind == ControlKind::close) {
                ++accepts;
                entered = true;
                ready.wait();
            }
            if (const auto* value = std::get_if<SessionConfirmControl>(&command);
                value && value->proof.request.kind == ControlKind::close)
                ++confirms;
        };
        auto wired = ports(pumps);
        wired.closeNow = [&] {
            return std::chrono::steady_clock::time_point{std::chrono::microseconds{logicalUs}};
        };
        wired.pumpMain = [&]() -> core::Result<void> {
            if (!alive)
                ++accessesAfterDestruction;
            ++pumps;
            if (unwinding && !released) {
                released = true;
                release.set_value();
            }
            return {};
        };
        wired.beforeClosePhase = [&](player::ExecutionClosePhase phase) -> core::Result<void> {
            if (!shifted && phase == player::ExecutionClosePhase::awaitAcceptance) {
                const auto guard = std::chrono::steady_clock::now() + std::chrono::seconds{5};
                while (!entered && std::chrono::steady_clock::now() < guard)
                    std::this_thread::yield();
                REQUIRE(entered);
                logicalUs = 1000001;
                shifted = true;
            }
            return {};
        };
        auto made = player::ExecutionHost::create(renderer, settings, std::move(wired));
        REQUIRE(made);
        host = std::move(*made);
        // This is the same guard used by runExecutionPlayer, declared after its captures.
        player::ExecutionHostLifetime lifetime{host};
        struct ArmUnwind {
            bool& value;
            ~ArmUnwind() {
                value = true;
            }
        } arm{unwinding};
        REQUIRE(host->load(test_support::presentation()));
        const auto first = host->close();
        REQUIRE_FALSE(first);
        originalError = first.error().code();
        CHECK(host->state() == HostState::closing);
        CHECK(host->closeBudgetExceeded());
        CHECK(alive);
        throw Unwind{};
    } catch (const Unwind&) {
    }
    CHECK(originalError == "host.rpa.control_pending");
    CHECK_FALSE(host);
    CHECK_FALSE(alive);
    CHECK(released);
    CHECK(accessesAfterDestruction == 0);
    CHECK(accepts == 1);
    CHECK(confirms == 1);
}

#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
TEST_CASE("R1 unavailable Close observation is distinct from a rejected Close step",
          "[rpa][r1][observation][missing]") {
    namespace obs = cuexis::internal::execution_diagnostics::rpa;
    presentation_renderer::TestPresentationRenderer renderer;
    std::atomic<unsigned> pumps{};
    std::atomic<bool> closeSeen{}, injected{};
    auto settings = options();
    settings.session.beforeDispatch = [&](std::uint64_t, const SessionCommand& command) {
        if (std::holds_alternative<SessionSoftwareClose>(command))
            closeSeen = true;
        if (closeSeen && std::holds_alternative<SessionRpaObservation>(command) &&
            !injected.exchange(true))
            throw std::bad_alloc{};
    };
    auto made = player::ExecutionHost::create(renderer, settings, ports(pumps));
    REQUIRE(made);
    REQUIRE((*made)->load(test_support::presentation()));
    REQUIRE((*made)->close());
    CHECK(injected);
    const auto& seen = (*made)->rpaObservation();
    CHECK(seen.fence.closeState == obs::CloseStepState::observationMissing);
    CHECK(seen.fence.closeAttempted);
    CHECK(seen.fence.closeApplied); // The reliable production receipt is still known.
    CHECK(seen.closeRetireFence.retireApplied);
    CHECK(seen.closedFlag);
    CHECK_FALSE((*made)->closeFirstError());
}
#endif

TEST_CASE("R1 a dispatched Close step remains pending until its original typed reply arrives",
          "[rpa][r1][observation][inflight]") {
    presentation_renderer::TestPresentationRenderer renderer;
    std::atomic<unsigned> pumps{}, dispatched{}, acknowledged{};
    std::atomic<bool> entered{};
    std::atomic<std::uint64_t> originalId{};
    std::promise<void> release;
    auto ready = release.get_future().share();
    bool interrupted{}, released{};
    auto settings = options();
    settings.session.beforeDispatch = [&](std::uint64_t id, const SessionCommand& command) {
        if (std::holds_alternative<SessionSoftwareClose>(command)) {
            originalId = id;
            ++dispatched;
            entered = true;
            ready.wait();
        }
    };
    settings.session.replyAcknowledged = [&](std::uint64_t id) {
        if (id == originalId && originalId != 0)
            ++acknowledged;
    };
    auto wired = ports(pumps);
    wired.pumpMain = [&]() -> core::Result<void> {
        ++pumps;
        if (entered && !interrupted) {
            interrupted = true;
            return core::unexpected(core::Error{"probe.close_step", "In-flight Close step"});
        }
        return {};
    };
    auto made = player::ExecutionHost::create(renderer, settings, std::move(wired));
    REQUIRE(made);
    struct ReleaseGuard {
        std::promise<void>& release;
        bool& released;
        ~ReleaseGuard() {
            if (!released) {
                released = true;
                release.set_value();
            }
        }
    } guard{release, released};
    REQUIRE((*made)->load(test_support::presentation()));
    const auto first = (*made)->close();
    REQUIRE_FALSE(first);
    CHECK((*made)->state() == HostState::closing);
    CHECK(dispatched == 1);
    CHECK(acknowledged == 0);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    namespace obs = cuexis::internal::execution_diagnostics::rpa;
    CHECK((*made)->rpaObservation().fence.closeState == obs::CloseStepState::resultPending);
    CHECK_FALSE((*made)->rpaObservation().fence.closeAttempted);
#endif
    released = true;
    release.set_value();
    REQUIRE((*made)->close());
    CHECK(dispatched == 1);
    CHECK(acknowledged == 1);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    CHECK((*made)->rpaObservation().fence.closeState == obs::CloseStepState::applied);
    CHECK((*made)->rpaObservation().fence.closeApplied);
#endif
}

TEST_CASE("R1 an exception at a Close boundary retains the original cause and phase",
          "[rpa][r1][exception][continuation]") {
    presentation_renderer::TestPresentationRenderer renderer;
    std::atomic<unsigned> pumps{}, accepts{}, retires{};
    auto settings = options();
    settings.session.beforeDispatch = [&](std::uint64_t, const SessionCommand& command) {
        if (const auto* value = std::get_if<SessionAcceptControl>(&command);
            value && value->request.kind == ControlKind::close)
            ++accepts;
        if (std::holds_alternative<SessionSoftwareRetire>(command))
            ++retires;
    };
    bool injected{};
    auto wired = ports(pumps);
    wired.beforeClosePhase = [&](player::ExecutionClosePhase phase) -> core::Result<void> {
        if (phase == player::ExecutionClosePhase::exportEvidence && !injected) {
            injected = true;
            throw std::bad_alloc{};
        }
        return {};
    };
    auto made = player::ExecutionHost::create(renderer, settings, std::move(wired));
    REQUIRE(made);
    REQUIRE((*made)->load(test_support::presentation()));
    const auto first = (*made)->close();
    REQUIRE_FALSE(first);
    CHECK(first.error().code() == "host.rpa.capacity_exceeded");
    CHECK((*made)->state() == HostState::closing);
    CHECK((*made)->closePhase() == player::ExecutionClosePhase::exportEvidence);
    REQUIRE((*made)->close());
    REQUIRE((*made)->closeFirstError());
    CHECK((*made)->closeFirstError()->code() == first.error().code());
    CHECK(accepts == 1);
    CHECK(retires == 1);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    CHECK(std::string{(*made)->rpaObservation().firstErrorCode} == first.error().code());
#endif
}

TEST_CASE("B5 declared 256 requirement boundary completes simultaneous timers and Replay",
          "[b5][host][measurement]") {
    const auto requirementBound = GENERATE(std::size_t{1}, std::size_t{256});
    auto fixture = std::make_unique<test_support::SessionTestContent>();
    const ClockMappingConfiguration mapping{{0, 1}, {0, 1}, {0, 1}, {1, 1}};
    test_support::rpa(*fixture, mapping);
    auto& document = fixture->request.assembly.sources[0].document;
    const auto prototype = document.requirements[0];
    document.requirements.clear();
    std::erase_if(document.declarations, [](const auto& declaration) {
        return declaration.kind == judgement::DeclarationKind::requirement;
    });
    document.relations[0].members.clear();
    fixture->owners.clear();
    fixture->request.assembly.contentProfileLimits.maxRequirements =
        judgement::MeasuredParameter<std::uint64_t>::measured(requirementBound);
    for (std::size_t i = 0; i < requirementBound; ++i) {
        auto requirement = prototype;
        requirement.resourceClaims[0].claimPolicy.competition =
            judgement::ClaimPolicyDeclaration::CompetitionKey{-1, static_cast<std::uint64_t>(i)};
        requirement.stableId.declarationOrdinal = static_cast<std::uint32_t>(100 + i);
        requirement.identity.requirementLocalId = "boundary." + std::to_string(i);
        document.declarations.push_back(judgement::testing::testDeclaration(
            judgement::DeclarationKind::requirement, requirement.identity.requirementLocalId,
            requirement.stableId.declarationOrdinal));
        document.relations[0].members.push_back(requirement.stableId);
        fixture->owners.push_back({requirement.identity, fixture->chart.entities[0].identity});
        document.requirements.push_back(std::move(requirement));
    }
    const auto prepareStart = std::chrono::steady_clock::now();
    auto packed = fixture->packed();
    auto content = GameplayContent::fromPacked(packed, fixture->config);
    REQUIRE(content);
    const auto prepareUs = std::chrono::duration_cast<std::chrono::microseconds>(
                               std::chrono::steady_clock::now() - prepareStart)
                               .count();
    presentation_renderer::TestPresentationRenderer renderer;
    std::atomic<unsigned> pumps{};
    auto settings = options();
    settings.realtime =
        SessionRealtimeStart{{1000000, 10, 1024, "", true}, mapping, {}, "b5.boundary"};
    auto host = player::ExecutionHost::create(renderer, settings, ports(pumps));
    REQUIRE(host);
    REQUIRE((*host)->load(test_support::presentation(), *content));
    ClockObservation eof{};
    eof.monotonicDomain = "b5.boundary";
    eof.segment = (*host)->context().clockSegment;
    eof.publicationSequence = 1;
    eof.sampleMonotonicUs = std::chrono::duration_cast<std::chrono::microseconds>(
                                std::chrono::steady_clock::now().time_since_epoch())
                                .count();
    eof.validUntilMonotonicUs = eof.sampleMonotonicUs + 1000000;
    eof.source = ClockFramePosition{0, 1000000};
    eof.presented = eof.source;
    eof.quality = ClockQuality::Exact;
    eof.playback = ClockPlaybackState::EndOfStream;
    auto tail =
        (*host)->beginTail(eof, {{1, 1}, 100000, 0, "declared 256 requirement boundary", true});
    REQUIRE(tail);
    GameplayAdvanceDecision final{};
    const auto limit = std::chrono::steady_clock::now() + std::chrono::seconds{3};
    do {
        auto decision = (*host)->advance(eof);
        REQUIRE(decision);
        final = std::move(*decision);
    } while ((final.disposition != GameplayAdvanceDisposition::Executed ||
              final.request.horizon.value != tail->terminalHorizon) &&
             std::chrono::steady_clock::now() < limit);
    REQUIRE(final.disposition == GameplayAdvanceDisposition::Executed);
    REQUIRE(final.request.horizon.value == tail->terminalHorizon);
    auto score = final.result.score();
    REQUIRE(score);
    CHECK(score->misses == requirementBound);
    CHECK(score->score == -static_cast<std::int64_t>(requirementBound));
    auto replay = (*host)->archive();
    REQUIRE(replay);
    auto evaluation = (*host)->evaluateReplay(*replay);
    REQUIRE(evaluation);
    REQUIRE(evaluation->evidenceValid);
    REQUIRE(final.result.sameResult(evaluation->result).value());
    const auto sampleStart = std::chrono::steady_clock::now();
    auto frame = (*host)->sample(
        tail->terminalHorizon,
        {static_cast<double>(tail->terminalHorizon) / 1000.0, 0, (*host)->context().clockSegment},
        false);
    REQUIRE(frame);
    REQUIRE((*host)->present(*frame));
    const auto sampleUs = std::chrono::duration_cast<std::chrono::microseconds>(
                              std::chrono::steady_clock::now() - sampleStart)
                              .count();
    REQUIRE((*host)->close());
    std::cout << "{\"event\":\"b5.cpu.content_boundary\",\"test_only\":true,\"requirements\":"
              << requirementBound << ",\"prepare_us\":" << prepareUs
              << ",\"packed_bytes\":" << packed.size() << ",\"sample_present_us\":" << sampleUs
              << ",\"misses\":" << score->misses << "}\n";
}

TEST_CASE("B5 main closes after audio factory failure before Load acceptance",
          "[b5][host][fault]") {
    presentation_renderer::TestPresentationRenderer renderer;
    std::atomic<unsigned> pumps{};
    auto wired = ports(pumps);
    wired.audioDevice =
        [](audio::AudioClipStore&) -> core::Result<std::unique_ptr<OwnedAudioDevice>> {
        return core::unexpected(core::Error{"probe.factory_failure", "Injected startup failure"});
    };
    auto settings = options();
    settings.sourceMode = PlaybackMode::HostClock;
    auto host = player::ExecutionHost::create(renderer, settings, std::move(wired));
    REQUIRE(host);
    auto source = PlaybackSource::fromFilesystemProject(std::filesystem::path{CUEXIS_SOURCE_DIR} /
                                                        "assets/projects/stage1d_project");
    REQUIRE(source);
    auto loaded = (*host)->load(std::move(*source));
    INFO((loaded
              ? ""
              : std::string{loaded.error().code()} + ": " + std::string{loaded.error().message()}));
    REQUIRE_FALSE(loaded);
    REQUIRE((*host)->hasAudio());
    auto publication = (*host)->audioPublication();
    REQUIRE(publication);
    REQUIRE(publication->fault == AudioPublication::Fault::device);
    auto closed = (*host)->close();
    INFO((closed
              ? ""
              : std::string{closed.error().code()} + ": " + std::string{closed.error().message()}));
    REQUIRE(closed);
    CHECK((*host)->state() == HostState::closed);
}

TEST_CASE("R3 CPU host Seek measures first and repeated requests through target present",
          "[.][b5][redesign-host-perf]") {
    if (test_support::environmentVariable("CUEXIS_REDESIGN_BARRIER") == "1") {
        static const bool admitted = [] {
            std::string line;
            return static_cast<bool>(std::getline(std::cin, line)) && line == "go";
        }();
        REQUIRE(admitted);
    }
    const auto policy = test_support::environmentVariable("CUEXIS_SEEK_IDENTITY");
    REQUIRE((policy.empty() || policy == "canonical" || policy == "source"));
    const auto recordSetting = test_support::environmentVariable("CUEXIS_SEEK_RECORDS");
    REQUIRE((recordSetting.empty() || recordSetting == "33" || recordSetting == "129"));
    const unsigned records = recordSetting == "129" ? 129 : 33;
    const std::int64_t horizon = 1103 + records - 1;
    const auto requirementSetting = test_support::environmentVariable("CUEXIS_SEEK_REQUIREMENTS");
    REQUIRE((requirementSetting.empty() || requirementSetting == "1000" ||
             requirementSetting == "10000" || requirementSetting == "40000"));
    const auto count = requirementSetting.empty()
                           ? GENERATE(std::size_t{64}, std::size_t{256}, std::size_t{2048})
                           : (requirementSetting == "1000"    ? std::size_t{1000}
                              : requirementSetting == "10000" ? std::size_t{10000}
                                                              : std::size_t{40000});
    const auto contentPath = test_support::environmentVariable("CUEXIS_SEEK_CONTENT");
    REQUIRE((contentPath.empty() || contentPath == "packed" || contentPath == "typed"));
    REQUIRE((contentPath != "typed" || !requirementSetting.empty()));
    const auto setupStart = std::chrono::steady_clock::now();
    std::cout << "{\"event\":\"r3.redesign.setup_start\",\"requirements\":" << count
              << ",\"records\":" << records << "}" << std::endl;
    auto fixture = std::make_unique<test_support::SessionTestContent>();
    const ClockMappingConfiguration mapping{{0, 1}, {0, 1}, {0, 1}, {1, 1}};
    test_support::rpa(*fixture, mapping);
    auto& document = fixture->request.assembly.sources[0].document;
    const auto prototype = document.requirements[0];
    document.requirements.clear();
    std::erase_if(document.declarations,
                  [](const auto& d) { return d.kind == judgement::DeclarationKind::requirement; });
    document.relations[0].members.clear();
    fixture->owners.clear();
    fixture->request.assembly.contentProfileLimits.maxRequirements =
        judgement::MeasuredParameter<std::uint64_t>::measured(count);
    for (std::size_t i = 0; i < count; ++i) {
        auto requirement = prototype;
        requirement.stableId.declarationOrdinal = static_cast<std::uint32_t>(100 + i);
        requirement.identity.requirementLocalId = "redesign.host." + std::to_string(i);
        requirement.resourceClaims[0].claimPolicy.competition =
            judgement::ClaimPolicyDeclaration::CompetitionKey{-1, static_cast<std::uint64_t>(i)};
        document.declarations.push_back(judgement::testing::testDeclaration(
            judgement::DeclarationKind::requirement, requirement.identity.requirementLocalId,
            requirement.stableId.declarationOrdinal));
        document.relations[0].members.push_back(requirement.stableId);
        fixture->owners.push_back({requirement.identity, fixture->chart.entities[0].identity});
        document.requirements.push_back(std::move(requirement));
    }
    presentation_renderer::TestPresentationRenderer renderer;
    std::atomic<unsigned> pumps{};
    auto settings = options();
    settings.seekIdentity = policy == "canonical" ? player::SeekIdentityPolicy::canonicalContent
                                                  : player::SeekIdentityPolicy::owningSource;
    settings.realtime =
        SessionRealtimeStart{{1000000, 10, 128, "", true}, mapping, {}, "redesign.host"};
    auto host = player::ExecutionHost::create(renderer, settings, ports(pumps));
    REQUIRE(host);
    const auto content = [&]() -> GameplayContent {
        if (requirementSetting.empty())
            return fixture->content();
        // The existing revision 4 path streams the semantic preimage separately from
        // the unchanged 16 MiB physical Packed envelope used by large fixtures.
        const auto prepared = judgement::prepareResolvedGameplay(fixture->request);
        INFO((prepared ? "" : std::string{prepared.error().message()}));
        REQUIRE(prepared);
        REQUIRE(prepared->assembled().graph.requirements.size() == count);
        if (contentPath == "typed") {
            // Test-only typed fixture: retain the same validated configuration and
            // pattern/measure definitions, replacing the owning prepared graph.
            test_support::SessionTestContent prototypeFixture;
            test_support::rpa(prototypeFixture, mapping);
            const auto prototypeContent = prototypeFixture.content();
            const auto* basis = GameplayAccess::content(prototypeContent);
            REQUIRE(basis);
            auto capsule = basis->capsule;
            capsule.chart = fixture->chart;
            capsule.gameplay = *prepared;
            capsule.owners = fixture->owners;
            capsule.verifiedSemanticDigest.reset();
            return GameplayAccess::content(std::move(capsule), basis->configuration,
                                           basis->bindings, basis->enabledCapabilities,
                                           basis->publicConfiguration);
        }
        const auto packed =
            gameplay_packed::encode({fixture->chart,
                                     *prepared,
                                     fixture->owners,
                                     {"normalization.one", "coordinator.policy.greedy_v1"},
                                     {},
                                     {},
                                     4});
        INFO((packed ? "" : std::string{packed.error().message()}));
        REQUIRE(packed);
        const auto decoded = GameplayContent::fromPacked(*packed, fixture->config);
        INFO((decoded ? "" : std::string{decoded.error().message()}));
        REQUIRE(decoded);
        return *decoded;
    }();
    const auto loaded = (*host)->load(test_support::presentation(), content);
    INFO((loaded
              ? ""
              : std::string{loaded.error().code()} + ": " + std::string{loaded.error().message()}));
    REQUIRE(loaded);
    const auto setupUs = std::chrono::duration_cast<std::chrono::microseconds>(
                             std::chrono::steady_clock::now() - setupStart)
                             .count();
    std::cout << "{\"event\":\"r3.redesign.setup_done\",\"requirements\":" << count
              << ",\"records\":" << records << ",\"setup_us\":" << setupUs << "}" << std::endl;
    std::int64_t target = 1103;
    std::uint64_t sequence{};
    player::ExecutionHost::ClockSource clock =
        [&](const HostContext& context) -> core::Result<ClockObservation> {
        ClockObservation observation{};
        observation.monotonicDomain = "redesign.host";
        observation.segment = context.clockSegment;
        observation.publicationSequence = ++sequence;
        observation.sampleMonotonicUs = std::chrono::duration_cast<std::chrono::microseconds>(
                                            std::chrono::steady_clock::now().time_since_epoch())
                                            .count();
        observation.validUntilMonotonicUs = observation.sampleMonotonicUs + 1000000;
        observation.presented = ClockFramePosition{target, 1000000};
        observation.source = observation.presented;
        observation.maximumPresented = observation.presented;
        observation.playback = ClockPlaybackState::Playing;
        observation.quality = ClockQuality::Exact;
        return observation;
    };
    for (unsigned record = 0; record < records; ++record) {
        target = 1103 + record;
        const auto observation = clock((*host)->context());
        REQUIRE(observation);
        const auto decision = (*host)->advance(*observation);
        INFO((decision ? "" : std::string{decision.error().code()}));
        REQUIRE(decision);
        REQUIRE(decision->disposition == GameplayAdvanceDisposition::Executed);
    }
    const auto archiveStart = std::chrono::steady_clock::now();
    const auto archive = (*host)->archive();
    const auto archiveUs = std::chrono::duration_cast<std::chrono::microseconds>(
                               std::chrono::steady_clock::now() - archiveStart)
                               .count();
    REQUIRE(archive);
    const auto cutStart = std::chrono::steady_clock::now();
    const auto cut = archive->cutAt({horizon});
    REQUIRE(cut);
    const auto cutUs = std::chrono::duration_cast<std::chrono::microseconds>(
                           std::chrono::steady_clock::now() - cutStart)
                           .count();
    const auto expected = (*host)->queryGameplay();
    REQUIRE(expected);
    const auto expectedScore = expected->score();
    REQUIRE(expectedScore);
    CHECK(expectedScore->misses == count);
    for (unsigned attempt = 0; attempt < 3; ++attempt) {
        target = horizon;
        REQUIRE((*host)->pause(clock));
        const auto started = std::chrono::steady_clock::now();
        auto sought = (*host)->seek(*archive, *cut, {horizon}, clock);
        const auto seekUs = std::chrono::duration_cast<std::chrono::microseconds>(
                                std::chrono::steady_clock::now() - started)
                                .count();
        INFO((sought ? ""
                     : std::string{sought.error().code()} + ": " +
                           std::string{sought.error().message()}));
        REQUIRE(sought);
        auto frame = (*host)->sample(
            horizon, {static_cast<double>(horizon) / 1000.0, 0, (*host)->context().clockSegment},
            false);
        REQUIRE(frame);
        const auto presented = (*host)->present(*frame);
        REQUIRE(presented);
        CHECK(presented->status == PresentStatus::succeeded);
        const auto totalUs = std::chrono::duration_cast<std::chrono::microseconds>(
                                 std::chrono::steady_clock::now() - started)
                                 .count();
        const auto actual = (*host)->queryGameplay();
        REQUIRE(actual);
        CHECK(actual->sameResult(*expected).value());
        std::cout << "{\"event\":\"r3.redesign.host\",\"requirements\":" << count
                  << ",\"attempt\":" << attempt << ",\"seek_us\":" << seekUs
                  << ",\"request_to_present_us\":" << totalUs << ",\"archive_us\":" << archiveUs
                  << ",\"capsule_revision\":"
                  << (contentPath == "typed" ? 0 : (requirementSetting.empty() ? 3 : 4))
                  << ",\"content_path\":\"" << (contentPath == "typed" ? "typed" : "packed") << "\""
                  << ",\"setup_us\":" << setupUs << ",\"cut_us\":" << cutUs
                  << ",\"records\":" << records << ",\"identity\":\""
                  << (policy == "canonical" ? "canonical" : "source") << "\""
                  << ",\"renderer\":\"cpu-test\",\"audio\":false,\"formal\":false}\n";
    }
    REQUIRE((*host)->close());
}
#include "r3_seek_matrix_tests.hpp"

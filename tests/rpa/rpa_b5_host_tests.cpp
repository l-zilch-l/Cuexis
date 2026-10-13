#include "../../app/player/src/execution_host.hpp"
#include "../audio/fake_audio_transport.hpp"
#include "execution_fixture.hpp"
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <future>
#include <iostream>

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
TEST_CASE("B5 realtime main host cuts after inspection and drives Pause Resume Seek Close",
          "[b5][host][realtime]") {
    presentation_renderer::TestPresentationRenderer renderer;
    std::atomic<unsigned> pumps{};
    auto settings = options();
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
    auto host = player::ExecutionHost::create(renderer, settings, std::move(wired));
    REQUIRE(host);
    live = host->get();
    auto loaded = live->load(test_support::presentation(), fixture.content());
    INFO((loaded ? "" : std::string{loaded.error().code()}));
    REQUIRE(loaded);
    std::uint64_t publication{};
    player::ExecutionHost::ClockSource source =
        [&](const HostContext& c) -> core::Result<ClockObservation> {
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
    CHECK_FALSE(frame->snapshot.objects.empty());
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
    CHECK_FALSE(frame->snapshot.objects.empty());
    REQUIRE((*host)->close());
}

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
}

TEST_CASE("B5 declared 256 requirement boundary completes simultaneous timers and Replay",
          "[b5][host][measurement]") {
    constexpr std::size_t requirementBound = 256;
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

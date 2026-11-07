#include "rpa_b5_session_close_fixture.hpp"
#include "rpa_b5_session_fixture.hpp"
#include "rpa_b5_session_paused_fixture.hpp"

// Worker threading, delivery rejection, prepared transfer and paused scenarios.

TEST_CASE("B5 Session owner has separate thread and preserves control reply capacity",
          "[rpa][b5][worker]") {
    auto config = configuration();
    config.capacity = 4;
    config.controlReserve = 1;
    auto worker = SessionWorker::create(config);
    REQUIRE(worker);
    std::vector<std::uint64_t> ids;
    for (int i = 0; i < 3; ++i) {
        auto id = (*worker)->post(SessionInspect{});
        REQUIRE(id);
        ids.push_back(*id);
    }
    REQUIRE_FALSE((*worker)->post(SessionInspect{}));
    auto control = (*worker)->post(SessionInspect{}, true);
    REQUIRE(control);
    ids.push_back(*control);
    for (const auto id : ids) {
        auto result = await(**worker, id);
        REQUIRE(result);
        CHECK(std::get<SessionInspection>(*result).ownerThread != std::this_thread::get_id());
    }
    shutdown(**worker);
    config.testOnly = false;
    REQUIRE_FALSE(SessionWorker::create(config));
}
TEST_CASE("B5 worker safely rejects invalid delivery after local construction",
          "[rpa][b5][worker]") {
    auto config = configuration();
    config.delivery.consumers = 0;
    auto rejected = SessionWorker::create(config);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().code() == "host.rpa.context_stale");
}
TEST_CASE("B5 Prepared and software commit stay on worker with owning frame transfer",
          "[rpa][b5][worker]") {
    auto worker = SessionWorker::create(configuration());
    REQUIRE(worker);
    auto loaded = load(**worker, presentation());
    SessionSample sample;
    sample.context = loaded.input.context;
    sample.projectionScope = 1;
    sample.frameId = 1;
    sample.runtimeFrame = {0, 0, 0};
    sample.viewport = {640, 480};
    auto frame = std::get<FrameEnvelope>(send(**worker, sample));
    CHECK(frame.context == loaded.input.context);
    CHECK_FALSE(frame.resources.empty());
    auto stale = sample;
    ++stale.context.rendererGeneration;
    auto id = (*worker)->post(stale);
    REQUIRE(id);
    auto rejected = await(**worker, *id);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().code() == "host.rpa.context_stale");
    close(**worker, loaded.host, loaded.input.context, 2, 1);
    CHECK_FALSE(frame.resources.empty());
    shutdown(**worker);
}

TEST_CASE("B5 Load ordinals continue into realtime while Waiting leaves owner input loop free",
          "[rpa][b5][worker]") {
    // Each owner scenario has a separate frame: large typed control payloads do not
    // accumulate across Catch sections in an unoptimized Windows debug stack.
    auto scene = pausedScenario();
    SECTION("Paused owner can close directly") {
        closePausedScenario(*scene);
    }
    SECTION("Actual owning Replay Seek restarts execution ordinals in a new epoch") {
        const auto sourceBinding = GENERATE(false, true);
        seekPausedScenario(*scene, sourceBinding);
    }
    SECTION("Actual R2 consumes owning final trace once on worker") {
        resumePausedScenario(*scene);
    }
    shutdown(*scene->worker);
}

TEST_CASE("B5 queued execution uses owner clock instead of stale submission time",
          "[rpa][b5][worker][clock]") {
    SessionTestContent fixture;
    const ClockMappingConfiguration mapping{{0, 1}, {0, 1}, {0, 1}, {1, 1}};
    rpa(fixture, mapping);
    auto config = configuration();
    config.monotonicNow = [] { return std::int64_t{1001}; };
    auto worker = SessionWorker::create(config);
    REQUIRE(worker);
    auto loaded = load(**worker, presentation(), fixture.content());
    const GameplayExecutionContext execution{loaded.input.context.sessionScope, 1, 1};
    send(**worker,
         SessionRealtimeStart{{1000, 100, 64, "", true}, mapping, execution, "test.clock"});
    const ClockObservation clock{"test.clock",
                                 1,
                                 1,
                                 0,
                                 1000,
                                 0,
                                 ClockFramePosition{20, 1000000},
                                 ClockFramePosition{20, 1000000},
                                 {},
                                 ClockPlaybackState::Playing,
                                 ClockQuality::Exact,
                                 false,
                                 false,
                                 false};
    auto posted = (*worker)->post(SessionAdvance{{execution, 1, 1, {20}}, clock}, false);
    REQUIRE(posted);
    auto rejected = await(**worker, *posted);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().code() == "host.rpa.clock_invalid");
    auto queried = (*worker)->post(SessionQueryAdvance{1}, true);
    REQUIRE(queried);
    CHECK_FALSE(await(**worker, *queried));
    // Rejected before admission: command 1 remains available to the directed Close.
    close(**worker, loaded.host, loaded.input.context, 2, 1);
    shutdown(**worker);
}

TEST_CASE("B5 suspended Close directs its own cleanup without a normal fence",
          "[rpa][b5][worker][close]") {
    auto worker = SessionWorker::create(configuration());
    REQUIRE(worker);
    auto loaded = load(**worker, presentation());
    ControlRequest close;
    close.context = loaded.input.context;
    ++close.context.controlSequence;
    close.kind = ControlKind::close;
    close.operationId = 2;
    close.commandOrdinal = 1;
    close.participants = {false, true, false, false};
    close.target.context = close.context;
    close.target.commandOrdinal = 1;
    close.target.participantGenerations[1] =
        std::get<SessionInspection>(send(**worker, SessionInspect{})).generation;
    const auto decision = std::get<std::uint64_t>(send(**worker, SessionAcceptControl{close}));
    REQUIRE(loaded.host.accept(close, decision));
    // Delivery failure suspends the Close while the protocol deliberately stays closing.
    REQUIRE(loaded.host.suspend(2));
    CHECK(loaded.host.query(2)->status == ControlStatus::suspended);
    StepRequest retire{{close.context.sessionScope, 2, Participant::software, StepPhase::retire, 0},
                       close.target};
    auto retired =
        std::get<StepReceipt>(send(**worker, SessionSoftwareRetire{retire, *loaded.host.query(2)}));
    CHECK(retired.status == StepStatus::applied);
    CHECK(retired.effects.referencesReleased);
    REQUIRE(loaded.host.observe(retired));
    retire.key.phase = StepPhase::close;
    auto closed =
        std::get<StepReceipt>(send(**worker, SessionSoftwareClose{retire, *loaded.host.query(2)}));
    REQUIRE(loaded.host.observe(closed));
    REQUIRE(loaded.host.finish());
    CHECK(loaded.host.query(2)->status == ControlStatus::complete);
    send(**worker, SessionConfirmControl{*loaded.host.query(2)});
    CHECK(std::get<SessionInspection>(send(**worker, SessionInspect{})).state ==
          SessionState::Empty);
    shutdown(**worker);
}

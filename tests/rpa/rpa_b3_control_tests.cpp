#include "rpa_b3_fixture.hpp"

// Control, fold and route failure cases.

using namespace cuexis::judgement;
using namespace cuexis::judgement::testing;
using namespace cuexis::rpa::testing;

TEST_CASE("B3 rev3 replay wire rejects rev1 rev2 and retains all fields", "[rpa][b3]") {
    Fixture3 f;
    auto s = f.session();
    REQUIRE(s.submitCanonical({occurrence(900, 0, InputAction::press)}));
    REQUIRE(s.advance(Tick{903}));
    auto a = s.archive();
    REQUIRE(a);
    auto bytes = encodeReplay(*a);
    REQUIRE(bytes);
    auto decoded = decodeReplay(*bytes, {f.configuration(), f.prepared()});
    REQUIRE(decoded);
    CHECK(sameReplayHistory(*a, *decoded));
    for (const auto old : {1, 2}) {
        auto changed = *bytes;
        changed[8] = static_cast<std::byte>(old);
        auto result = decodeReplay(changed, {f.configuration(), f.prepared()});
        REQUIRE_FALSE(result);
        CHECK(result.error().code() == "codec.version_unsupported");
    }
    for (std::size_t cut = 0; cut < bytes->size(); ++cut)
        CHECK_FALSE(decodeReplay(std::span{*bytes}.first(cut), {f.configuration(), f.prepared()}));
}
TEST_CASE("B3 A1 final route failure terminates continuation and waiting trace is distinct",
          "[rpa][b3]") {
    Fixture3 f;
    auto s = f.session();
    auto d = scheduler(s, scope, 32);
    REQUIRE(d.acceptControl(scope, 0));
    REQUIRE(d.deliver({scope, 0, EndRunEpoch{Tick{900}, 1}}));
    REQUIRE(d.pause());
    const RpaControlRecord resume{RpaControlKind::resume, Tick{900}, 0, 1, 1, {}};
    auto pending = d.resume(resume, {1, 2, 2}, true);
    REQUIRE_FALSE(pending);
    CHECK(pending.error().code() == "host.rpa.control_pending");
    const auto before = view(s);
    auto failed = d.resume({RpaControlKind::resume, Tick{900}, 0, 1, 0, {}}, {1, 2, 2}, true, true);
    REQUIRE_FALSE(failed);
    CHECK(failed.error().code() == "host.rpa.resume_aborted");
    CHECK(sameKernelResult(before, view(s)));
    auto again = d.resume({RpaControlKind::resume, Tick{900}, 1, 1, 0, {}}, {1, 2, 3}, true);
    REQUIRE_FALSE(again);
    CHECK(again.error().code() == "host.rpa.resume_aborted");
    CHECK(d.suspended());
}

TEST_CASE("B3 T2 complete kernel matches independent source scanning oracle", "[rpa][b3][oracle]") {
    Fixture3 f;
    auto s = f.session();
    std::vector<CanonicalInput> inputs{
        occurrence(900, 0, InputAction::press), occurrence(900, 1, InputAction::release),
        occurrence(900, 2, InputAction::press), occurrence(900, 0, InputAction::press, "lane.two")};
    std::sort(inputs.begin(), inputs.end(),
              [](const auto& a, const auto& b) { return a.key < b.key; });
    std::vector<AdmittedObservation> expectedInputs;
    for (const auto& input : inputs)
        expectedInputs.push_back({input.key, Tick{900}, false, {}, {}});
    auto expected = scanReference(f.prepared(), f.configuration(), expectedInputs, Tick{903});
    expected.kernelWorkTick = Tick{900};
    expected.sealedFactCursor = static_cast<std::uint64_t>(expected.facts.size());
    expected.effectiveFactSemanticRevision = "fact.semantic.rpa.v1";
    expected.rpa.emplace();
    for (const auto& input : inputs)
        expected.rpa->accepted.push_back(input.key);
    expected.rpa->foldBaseTick = Tick{900};
    REQUIRE(s.submitCanonical(inputs));
    REQUIRE(s.advance(Tick{903}));
    CHECK(sameKernelResult(expected, view(s)));
}
TEST_CASE("B3 control termination closes the last batch without fictitious later observation",
          "[rpa][b3]") {
    Fixture3 f;
    auto s = f.session();
    auto d = scheduler(s, scope, 32);
    REQUIRE(d.acceptControl(scope, 0));
    auto last = batch(0, 900, {occurrence(900, 0, InputAction::press)});
    auto& closed = std::get<ClosedCaptureBatch>(last.payload);
    closed.observedTick = Tick{900};
    closed.epochTerminated = true;
    closed.pauseEpoch = 1;
    REQUIRE(d.deliver(last));
    CHECK_FALSE(d.verifiedExclusive());
    REQUIRE(d.deliver({scope, 1, EndRunEpoch{Tick{900}, 1}}));
    REQUIRE(d.pause());
    const auto p = view(s);
    CHECK(p.rpa->paused);
    CHECK(phase(p, PhaseKind::head) == KernelPhaseState::Hit);
    CHECK(p.lastAdvanceHorizon == Tick{903});
}
TEST_CASE("B3 fragment gaps cannot authorize a prefix and fragments admit a single batch",
          "[rpa][b3]") {
    Fixture3 f;
    auto s = f.session();
    auto d = scheduler(s, scope, 32);
    REQUIRE(d.request({scope, 1, 0, Tick{903}}));
    auto first = batch(0, 900, {occurrence(900, 0, InputAction::press)});
    auto second = batch(1, 900, {occurrence(900, 1, InputAction::release)});
    auto& a = std::get<ClosedCaptureBatch>(first.payload);
    auto& b = std::get<ClosedCaptureBatch>(second.payload);
    a.fragmentCount = b.fragmentCount = 2;
    a.totalMembers = b.totalMembers = 2;
    b.batchId = a.batchId;
    b.fragmentIndex = 1;
    REQUIRE(d.deliver(second));
    CHECK(d.admissions().empty());
    REQUIRE(d.deliver(first));
    REQUIRE(d.admissions().size() == 1);
    REQUIRE(d.deliver({scope, 2, CapturePrefix{Tick{901}}}));
    auto r = d.poll(1);
    REQUIRE(r);
    CHECK(r->disposition == AdvanceDisposition::executed);
    REQUIRE(r->result);
    CHECK(r->result->receipts.size() == 2);
}
TEST_CASE("B3 same H with different control cuts yields different reachable snapshots",
          "[rpa][b3]") {
    Fixture3 f;
    auto s = f.session();
    REQUIRE(s.submitCanonical({occurrence(900, 0, InputAction::press)}));
    REQUIRE(s.advance(Tick{903}));
    const auto beforeCut = s.archive();
    REQUIRE(beforeCut);
    REQUIRE(s.controlRpa({RpaControlKind::pause, Tick{900}, 0, 1, 0, {}}));
    REQUIRE(s.controlRpa({RpaControlKind::resume,
                          Tick{900},
                          0,
                          1,
                          1,
                          {{0, occurrence(901, 0, InputAction::release).key}}}));
    const auto archive = s.archive();
    REQUIRE(archive);
    const auto after = view(s);
    auto a = f.session();
    REQUIRE(a.seek(*archive, Tick{903}, ReplayCut{2, {}}, {}));
    const auto earlier = view(a);
    auto b = f.session();
    REQUIRE(b.seek(*archive, Tick{903}, ReplayCut{4, {}}, {}));
    CHECK(sameKernelResult(after, view(b)));
    CHECK_FALSE(sameKernelResult(earlier, after));
    CHECK(earlier.lastAdvanceHorizon == after.lastAdvanceHorizon);
    auto checkpoint = JudgementSession::checkpoint(*archive, Tick{903});
    REQUIRE(checkpoint);
    CHECK(checkpoint->archive.data().identity == archive->data().identity);
}
TEST_CASE("B3 future press release association interrupts only its actual paused contact",
          "[rpa][b3]") {
    Fixture3 f;
    auto s = f.session();
    REQUIRE(s.advance(Tick{902}));
    REQUIRE(s.submitCanonical({occurrence(898, 0, InputAction::press)}));
    REQUIRE(s.controlRpa({RpaControlKind::pause, Tick{899}, 0, 1, 0, {}}));
    REQUIRE(s.controlRpa({RpaControlKind::resume,
                          Tick{899},
                          0,
                          1,
                          2,
                          {{0, occurrence(901, 0, InputAction::release).key},
                           {1, occurrence(902, 0, InputAction::press).key}}}));
    REQUIRE(s.advance(Tick{904}));
    const auto p = view(s);
    CHECK(phase(p, PhaseKind::head) == KernelPhaseState::Hit);
    CHECK(phase(p, PhaseKind::body) == KernelPhaseState::Miss);
    CHECK(p.contacts.empty());
    CHECK(p.rpa->deferred.empty());
    REQUIRE(p.rpa->quarantine.size() == 1);
    auto archive = s.archive();
    REQUIRE(archive);
    auto e = JudgementSession::evaluateReplay(*archive);
    REQUIRE(e);
    CHECK(sameKernelResult(p, *e->result));
}
TEST_CASE("B3 occurrence golden wire includes signed Tick and maximum source position",
          "[rpa][b3][wire]") {
    CanonicalObservationKey key{ObservationTick{Tick{-1}}, "d", "s",       "c",
                                InputAction::press,        {},  UINT64_MAX};
    cuexis::judgement::detail::wire::Writer writer;
    writer.revision = 3;
    writer(key);
    constexpr std::string_view golden = "ffffffffffffffff010000000000000064010000000000000073010000"
                                        "000000000063000001ffffffffffffffff";
    std::string hex;
    constexpr char digits[] = "0123456789abcdef";
    for (const auto value : writer.bytes) {
        const auto b = std::to_integer<unsigned>(value);
        hex.push_back(digits[b >> 4]);
        hex.push_back(digits[b & 15]);
    }
    CHECK(hex == golden);
}

TEST_CASE("B3 same q control suffix recomputes Fold and emits one hook at q plus one",
          "[rpa][b3][fold]") {
    Fixture3 f;
    f.rules = controlRules(true);
    auto s = f.session();
    REQUIRE(s.submitCanonical({occurrence(900, 0, InputAction::press)}));
    REQUIRE(s.advance(Tick{903}));
    REQUIRE(s.controlRpa({RpaControlKind::pause, Tick{900}, 0, 1, 0, {}}));
    REQUIRE(s.controlRpa({RpaControlKind::resume,
                          Tick{900},
                          0,
                          1,
                          1,
                          {{0, occurrence(901, 0, InputAction::release).key}}}));
    auto p = view(s);
    REQUIRE(p.fold);
    CHECK(p.fold->score == 0);
    CHECK(p.fold->factCursor == p.sealedFactCursor);
    REQUIRE(p.fold->produced.size() == 1);
    CHECK(p.fold->produced[0].visibleTick == Tick{901});
    auto a = s.archive();
    REQUIRE(a);
    auto e = JudgementSession::evaluateReplay(*a);
    REQUIRE(e);
    CHECK(sameKernelResult(p, *e->result));
}
TEST_CASE("B3 resume allocation rejection preserves paused kernel and Fold failure preserves seal",
          "[rpa][b3][fault]") {
    for (const auto sealed : {false, true}) {
        Fixture3 f;
        f.rules = controlRules();
        auto s = f.session();
        auto d = scheduler(s, scope, 32);
        REQUIRE(s.submitCanonical({occurrence(900, 0, InputAction::press)}));
        REQUIRE(d.acceptControl(scope, 0));
        REQUIRE(d.deliver({scope, 0, EndRunEpoch{Tick{900}, 1}}));
        REQUIRE(d.pause());
        const auto before = view(s);
        detail::KernelTestControls controls{};
        controls.failedTick = Tick{900};
        controls.failurePoint = sealed ? detail::KernelFailurePoint::beforeFold
                                       : detail::KernelFailurePoint::beforeSeal;
        REQUIRE(detail::KernelTestAccess::inject(s, controls));
        auto failed = d.resume({RpaControlKind::resume,
                                Tick{900},
                                0,
                                1,
                                1,
                                {{0, occurrence(901, 0, InputAction::release).key}}},
                               {1, 2, 2}, true);
        REQUIRE_FALSE(failed);
        const auto p = view(s);
        if (!sealed) {
            CHECK(sameKernelResult(before, p));
            CHECK(d.suspended());
        } else {
            CHECK(p.state == KernelSessionState::Faulted);
            CHECK(p.faultStage == FaultStage::fold);
            CHECK(p.sealedFactCursor == 3);
            REQUIRE(p.fold);
            CHECK(p.fold->factCursor == 1);
            CHECK_FALSE(s.snapshot());
            auto dto = detail::KernelTestAccess::captureDTO(s);
            REQUIRE(dto);
            auto snapshot =
                detail::KernelTestAccess::snapshotValue(*dto, {f.configuration(), f.prepared()});
            auto restored = JudgementSession::recover(snapshot, {f.configuration(), f.prepared()});
            REQUIRE(restored);
            CHECK(sameKernelResult(p, view(*restored)));
            auto archive = s.archive();
            REQUIRE(archive);
            auto evaluation = detail::ExecutionKernel::evaluate(*archive, &controls);
            REQUIRE(evaluation);
            CHECK(sameKernelResult(p, *evaluation->result));
        }
    }
}
TEST_CASE("B3 clock invalid cancels wait and epoch Tick overflow cannot fabricate drain",
          "[rpa][b3]") {
    Fixture3 f;
    auto s = f.session();
    auto d = scheduler(s, scope, 32);
    REQUIRE_FALSE(d.request({scope, 0, 0, Tick{INT64_MIN}}));
    CHECK_FALSE(d.poll(0));
    REQUIRE(d.request({scope, 1, 0, Tick{903}}));
    REQUIRE(d.clockValid(false));
    REQUIRE(d.deliver({scope, 0, CapturePrefix{Tick{1000}}}));
    auto r = d.poll(1);
    REQUIRE(r);
    CHECK(r->disposition == AdvanceDisposition::clockInvalidNotExecuted);
    CHECK_FALSE(view(s).lastAdvanceHorizon);
    auto other = f.session();
    auto overflow = scheduler(other, scope, 32);
    REQUIRE(overflow.acceptControl(scope, 0));
    REQUIRE(overflow.deliver({scope, 0, EndRunEpoch{Tick{INT64_MAX}, 1}}));
    REQUIRE_FALSE(overflow.pause());
    CHECK_FALSE(view(other).lastAdvanceHorizon);
}
TEST_CASE("B3 real monotonic clock measures H and frontier lag separately",
          "[rpa][b3][measurement]") {
    using namespace cuexis::playback;
    Fixture3 f;
    auto s = f.session();
    auto d = scheduler(s, scope, 128);
    auto bridge = RealtimeClock::create({1000000, 100, 128, "", true},
                                        {{0, 1}, {0, 1}, {0, 1}, {1, 1}}, "steady.fixture", 1);
    REQUIRE(bridge);
    const auto origin = std::chrono::steady_clock::now();
    auto now = [&] {
        return std::chrono::duration_cast<std::chrono::microseconds>(
                   std::chrono::steady_clock::now() - origin)
            .count();
    };
    ClockObservation observation{"steady.fixture",
                                 1,
                                 1,
                                 0,
                                 1000000,
                                 0,
                                 ClockFramePosition{0, 1000000},
                                 ClockFramePosition{0, 1000000},
                                 {},
                                 ClockPlaybackState::Playing,
                                 ClockQuality::Exact,
                                 false,
                                 false,
                                 false};
    std::uint64_t publication = 0;
    auto sampleClock = [&]() {
        const auto stamp = now();
        observation.sampleMonotonicUs = stamp;
        observation.validUntilMonotonicUs = stamp + 1000000;
        observation.source = ClockFramePosition{stamp, 1000000};
        observation.presented = observation.source;
        observation.publicationSequence = publication++;
        return bridge->sample(observation, stamp);
    };
    std::vector<std::int64_t> horizonLag, frontierLag;
    for (std::uint64_t i = 0; i < 32; ++i) {
        const auto sampled = sampleClock();
        REQUIRE(sampled);
        REQUIRE(d.deliver({scope, i, CapturePrefix{Tick{sampled->horizon + 1}}}));
        auto result = d.request({scope, i, i, Tick{sampled->horizon}});
        REQUIRE(result);
        REQUIRE(result->disposition == AdvanceDisposition::executed);
        const auto measured = sampleClock();
        REQUIRE(measured);
        const auto p = view(s);
        REQUIRE(p.lastAdvanceHorizon);
        REQUIRE(p.processedFrontier);
        horizonLag.push_back(measured->horizon - p.lastAdvanceHorizon->value());
        frontierLag.push_back(measured->horizon - 3 - p.processedFrontier->value());
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    auto statistics = [](auto values) {
        std::sort(values.begin(), values.end());
        return std::vector<std::int64_t>{values[15], values[30], values[31], values.back()};
    };
    const auto h = statistics(horizonLag), frontier = statistics(frontierLag);
    const auto json = std::string{"{\"testOnly\":true,\"samples\":32,\"unit\":\"us\",\"clock\":"
                                  "\"std::chrono::steady_clock\",\"H_lag_p50_p95_p99_max\":["} +
                      std::to_string(h[0]) + "," + std::to_string(h[1]) + "," +
                      std::to_string(h[2]) + "," + std::to_string(h[3]) +
                      "],\"frontier_lag_p50_p95_p99_max\":[" + std::to_string(frontier[0]) + "," +
                      std::to_string(frontier[1]) + "," + std::to_string(frontier[2]) + "," +
                      std::to_string(frontier[3]) + "]}";
    std::cout << json << '\n';
    const auto measurementDestination =
        cuexis::test_support::environmentVariable("CUEXIS_B3_MEASUREMENT_FILE");
    if (!measurementDestination.empty()) {
        std::ofstream output{measurementDestination};
        REQUIRE(output);
        output << json << '\n';
        REQUIRE(output);
    }
}

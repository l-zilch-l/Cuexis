#include "rpa_b5_host_fixture.hpp"

// Public transcript, close interruption and deadline cases.

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

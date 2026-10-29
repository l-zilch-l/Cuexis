#pragma once

TEST_CASE("R3 Host Seek verifies origin state and target frame across cuts and hints",
          "[rpa][b5][seek-matrix]") {
    const auto records = GENERATE(33U, 129U);
    // Include partial targets inside the first long record, before any native anchor.
    const auto cutKind = GENERATE(0U, 1U, 2U, 3U, 4U);
    const bool externalSource = GENERATE(false, true);
    const auto hintKind = GENERATE(0U, 1U, 2U); // none, checkpoint, uncertified copy
    test_support::SessionTestContent fixture;
    const ClockMappingConfiguration mapping{{0, 1}, {0, 1}, {0, 1}, {1, 1}};
    test_support::rpa(fixture, mapping);
    const auto content = fixture.content();
    presentation_renderer::TestPresentationRenderer renderer;
    std::atomic<unsigned> pumps{};
    auto settings = options();
    settings.seekIdentity = player::SeekIdentityPolicy::owningSource;
    settings.realtime =
        SessionRealtimeStart{{1000000, 10, 128, "", true}, mapping, {}, "r3.matrix"};
    auto created = player::ExecutionHost::create(renderer, settings, ports(pumps));
    REQUIRE(created);
    auto host = std::move(*created);
    REQUIRE(host->load(test_support::presentation(), content));
    std::int64_t target = 1103;
    std::uint64_t sequence{};
    player::ExecutionHost::ClockSource clock =
        [&](const HostContext& context) -> core::Result<ClockObservation> {
        ClockObservation result{};
        result.monotonicDomain = "r3.matrix";
        result.segment = context.clockSegment;
        result.publicationSequence = ++sequence;
        result.sampleMonotonicUs = std::chrono::duration_cast<std::chrono::microseconds>(
                                       std::chrono::steady_clock::now().time_since_epoch())
                                       .count();
        result.validUntilMonotonicUs = result.sampleMonotonicUs + 1000000;
        result.presented = ClockFramePosition{target, 1000000};
        result.source = result.maximumPresented = result.presented;
        result.playback = ClockPlaybackState::Playing;
        result.quality = ClockQuality::Exact;
        return result;
    };
    for (unsigned i = 0; i < records; ++i) {
        target = 1103 + 2 * i;
        auto observation = clock(host->context());
        REQUIRE(observation);
        REQUIRE(host->advance(*observation));
    }
    auto original = host->archive();
    REQUIRE(original);
    auto encoded = original->toBytes();
    REQUIRE(encoded);
    auto decoded =
        GameplayReplay::fromBytes(*encoded, content, {encoded->size(), 10000, 10000000, true});
    REQUIRE(decoded);
    const auto archive = externalSource ? *decoded : *original;
    const std::int64_t horizon = cutKind == 1   ? target
                                 : cutKind == 3 ? 902
                                 : cutKind == 4 ? 5
                                                : 1103 + 2 * (records / 2) + (cutKind == 2);

    // A distinct external owner has no native checkpoint index: the oracle takes origin replay.
    const auto* basis = GameplayAccess::content(content);
    const auto* external = GameplayAccess::replay(*decoded);
    REQUIRE(basis);
    REQUIRE(external);
    auto reference = judgement::JudgementSession::create();
    REQUIRE(reference);
    REQUIRE(reference->configure(basis->configuration));
    REQUIRE(reference->prepare(basis->capsule.gameplay));
    const auto oracleCut = judgement::replayCutAt(external->archive, judgement::Tick{horizon});
    auto oracle = reference->forkForSeek(external->archive, judgement::Tick{horizon}, oracleCut);
    REQUIRE(oracle);
    auto expected = oracle->snapshot();
    REQUIRE(expected);
    auto expectedBytes = judgement::encodeSnapshot(*expected);
    REQUIRE(expectedBytes);
    std::vector<GameplayCheckpoint> checkpoints;
    std::uint64_t expectedRestoredRecords{};
    if (hintKind) {
        const auto* source = GameplayAccess::replay(archive);
        REQUIRE(source);
        auto checkpoint =
            judgement::JudgementSession::checkpoint(source->archive, judgement::Tick{1103});
        REQUIRE(checkpoint);
        if (hintKind == 1 && !externalSource && checkpoint->horizon <= judgement::Tick{horizon} &&
            checkpoint->cut.completeRecords <= oracleCut.completeRecords)
            expectedRestoredRecords = checkpoint->cut.completeRecords;
        if (hintKind == 2) {
            // Rebuilding the public carrier deliberately omits its private certificate.
            checkpoint = judgement::ReplayCheckpoint{checkpoint->archive, checkpoint->cut,
                                                     checkpoint->horizon, checkpoint->state};
        }
        checkpoints.push_back(GameplayAccess::checkpoint(std::move(*checkpoint)));
    }
    REQUIRE(host->pause(clock));
    target = horizon;
    const auto requestStart = std::chrono::steady_clock::now();
    auto cut = host->replayCut(archive, {horizon});
    REQUIRE(cut);
    CHECK(cut->partialHorizon.has_value() == (cutKind >= 2));
    auto sought = host->seek(archive, *cut, {horizon}, clock, checkpoints);
    INFO((sought
              ? ""
              : std::string{sought.error().code()} + ": " + std::string{sought.error().message()}));
    REQUIRE(sought);
    const auto ready = std::chrono::steady_clock::now();
    CHECK(host->state() == HostState::running);
    auto frame = host->sample(
        horizon, {static_cast<double>(horizon) / 1000.0, 0, host->context().clockSegment}, false);
    REQUIRE(frame);
    CHECK(frame->presentationTick == std::optional<std::int64_t>{horizon});
    CHECK(frame->context == host->context());
    auto receipt = host->present(*frame);
    REQUIRE(receipt);
    const auto presented = std::chrono::steady_clock::now();
    CHECK(receipt->status == PresentStatus::succeeded);
    CHECK(receipt->context == frame->context);
    CHECK(receipt->frameId == frame->frameId);
    CHECK(receipt->projectionScope == frame->projectionScope);
    auto actual = host->snapshotGameplay();
    REQUIRE(actual);
    auto actualBytes = actual->toBytes();
    REQUIRE(actualBytes);
    CHECK(*actualBytes == *expectedBytes);
    std::cout
        << "{\"event\":\"r3.seek.matrix\",\"records\":" << records << ",\"cut_kind\":" << cutKind
        << ",\"external\":" << externalSource << ",\"hint_kind\":" << hintKind
        << ",\"request_to_ready_us\":"
        << std::chrono::duration_cast<std::chrono::microseconds>(ready - requestStart).count()
        << ",\"request_to_present_us\":"
        << std::chrono::duration_cast<std::chrono::microseconds>(presented - requestStart).count()
        << ",\"renderer\":\"cpu-test\",\"formal\":false}\n";
    REQUIRE(host->close());
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    if (hintKind || externalSource) {
        namespace rpa = cuexis::internal::execution_diagnostics::rpa;
        const auto& work =
            host->rpaSeekObservation().work[static_cast<std::size_t>(rpa::SeekPhase::s3Validation)];
        CHECK(work.actualRecords == cut->completeRecords - expectedRestoredRecords);
    }
#else
    static_cast<void>(expectedRestoredRecords);
#endif
}

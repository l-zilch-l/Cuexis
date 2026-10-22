// Included by pd5_matrix_probe.cpp so the exact typed presentation generator is shared.
#if defined(_WIN32)
#include "pd5_capacity_r4_support.hpp"
TEST_CASE("PD5 full content Snapshot Seek Replay boundary", "[.][pd][pd5][capacity]") {
    using namespace cuexis;
    using namespace playback;
    using namespace playback::detail;
    const auto scale = matrixCount("CUEXIS_PD5_ENTITIES", 40000);
    REQUIRE(scale <= 40000);
    const auto reuse = test_support::environmentVariable("CUEXIS_PD5_REUSE");
    REQUIRE((reuse == "low" || reuse == "high"));
    const auto revision = test_support::environmentVariable("CUEXIS_PD5_CAPSULE_REVISION");
    REQUIRE((revision.empty() || revision == "3" || revision == "4"));
    const bool r4 = revision == "4";
    pd5_capacity::Trace trace{r4};
    if (r4)
        trace.run("identity", [&] { pd5_capacity::identity(scale, reuse); });
    trace.begin("fixture-construction");
    auto fixture = std::make_unique<test_support::SessionTestContent>();
    const ClockMappingConfiguration mapping{{0, 1}, {0, 1}, {0, 1}, {1, 1}};
    test_support::rpa(*fixture, mapping);
    auto& document = fixture->request.assembly.sources[0].document;
    const auto prototype = document.requirements[0];
    const auto entityPrototype = fixture->chart.entities[0];
    document.requirements.clear();
    fixture->chart.entities.clear();
    fixture->owners.clear();
    std::erase_if(document.declarations,
                  [](const auto& d) { return d.kind == judgement::DeclarationKind::requirement; });
    document.relations[0].members.clear();
    fixture->request.assembly.contentProfileLimits.maxRequirements =
        judgement::MeasuredParameter<std::uint64_t>::measured(scale);
    for (unsigned i = 0; i < scale; ++i) {
        auto requirement = prototype;
        requirement.stableId.declarationOrdinal = 100 + i;
        requirement.identity.requirementLocalId = "capacity." + std::to_string(i);
        requirement.resourceClaims[0].claimPolicy.competition =
            judgement::ClaimPolicyDeclaration::CompetitionKey{-1, i};
        if (reuse == "low")
            requirement.timing->end = judgement::Tick{requirement.timing->end.value() + i};
        document.declarations.push_back(judgement::testing::testDeclaration(
            judgement::DeclarationKind::requirement, requirement.identity.requirementLocalId,
            requirement.stableId.declarationOrdinal));
        document.relations[0].members.push_back(requirement.stableId);
        auto entity = entityPrototype;
        std::ostringstream suffix;
        suffix << "019b0000-0000-7abc-8def-" << std::hex << std::setw(12) << std::setfill('0')
               << i + 2;
        entity.identity = chart::ExplicitEntityIdentity{chart::ChartObjectId{suffix.str()}};
        fixture->owners.push_back({requirement.identity, entity.identity});
        fixture->chart.entities.push_back(std::move(entity));
        document.requirements.push_back(std::move(requirement));
    }
    trace.succeeded("fixture-construction");
    trace.run("before-load-memory", [&] { internal::pd::memory::checkpoint("before-load"); });
    auto packed = trace.run("encode", [&] {
        return r4 ? pd5_capacity::encodeR4(*fixture, trace, scale, reuse) : fixture->packed();
    });
    const auto content = trace.run("GameplayContent::fromPacked", [&] {
        return trace.take(GameplayContent::fromPacked(packed, fixture->config));
    });
    presentation_renderer::TestPresentationRenderer renderer;
    // This profile has exactly scale objects and at most one feedback member per requirement.
    // Derive its finite storage declaration from that workload, including two portable resources.
    // A reduced plumbing check must not reserve the 40k profile's membership capacity.
    const auto frameBound = trace.run("frame-budget", [&] {
        const auto bound = trace.take(frameByteBound(scale, 2, scale));
        REQUIRE(bound <= SIZE_MAX / 2);
        return bound;
    });
    player::ExecutionHostOptions options;
    options.session = {128, 32, 4096, 128, 4096, 4096, true, matrixNow};
    options.session.delivery = {8, 2, frameBound * 2, 32, 64};
    options.session.observation = {8U * 1024U * 1024U, 128, 8, scale, 1000000, 5000000};
    options.session.maxFrameMembers = scale;
    options.session.receiptCapacity = 128;
    options.audio.testOnly = true;
    options.hostCapacity = 16;
    options.captureCapacity = 64;
    options.captureReserve = 16;
    options.replyDeadline = std::chrono::seconds{30};
    options.viewport = {1280, 720};
    options.realtime =
        SessionRealtimeStart{{1000000, 10, 65536, "", true}, mapping, {}, "pd5.capacity"};
    auto host = trace.run("host-create", [&] {
        return trace.take(player::ExecutionHost::create(renderer, options, {}));
    });
    trace.run("host-load", [&] { trace.take(host->load(sparseSource(scale), content)); });
    std::int64_t tick = 100000;
    std::uint64_t sequence{};
    auto clock = [&](const HostContext& context) -> core::Result<ClockObservation> {
        ClockObservation o{};
        o.monotonicDomain = "pd5.capacity";
        o.segment = context.clockSegment;
        o.publicationSequence = ++sequence;
        o.sampleMonotonicUs = matrixNow();
        o.validUntilMonotonicUs = o.sampleMonotonicUs + 1000000;
        o.source = o.presented = ClockFramePosition{tick, 1000000};
        o.maximumPresented = o.presented;
        o.quality = ClockQuality::Exact;
        o.playback = ClockPlaybackState::Playing;
        return o;
    };
    const auto decision = trace.run("host-advance", [&] {
        const auto decision = trace.take(host->advance(trace.take(clock(host->context()))));
        REQUIRE(decision.disposition == GameplayAdvanceDisposition::Executed);
        return decision;
    });
    const auto expected = trace.run("host-query", [&] {
        auto result = trace.take(host->queryGameplay());
        REQUIRE(trace.take(result.score()).misses == scale);
        return result;
    });
    auto snapshot = trace.run("snapshot", [&] { return trace.take(host->snapshotGameplay()); });
    auto bytes = trace.run("snapshot-encode", [&] { return trace.take(snapshot.toBytes()); });
    const auto decodedSnapshot = trace.run("snapshot-decode", [&] {
        const auto decodedSnapshot = trace.take(
            GameplaySnapshot::fromBytes(bytes, content, {268435456, 100000, 20000000, true}));
        REQUIRE(decodedSnapshot.valid());
        return decodedSnapshot;
    });
    const auto replay = trace.run("archive", [&] { return trace.take(host->archive()); });
    const auto evaluated = trace.run("evaluateReplay-sameResult", [&] {
        const auto evaluated = trace.take(host->evaluateReplay(replay));
        REQUIRE(evaluated.evidenceValid);
        REQUIRE(trace.take(expected.sameResult(evaluated.result)));
        return evaluated;
    });
    trace.run("pause", [&] { trace.take(host->pause(clock)); });
    const auto cut =
        trace.run("replayCut", [&] { return trace.take(host->replayCut(replay, {tick})); });
    const auto started = matrixNow();
    trace.run("seek", [&] { trace.take(host->seek(replay, cut, {tick}, clock)); });
    const auto confirmed = matrixNow();
    auto frame = trace.run("sample", [&] {
        return trace.take(host->sample(
            tick, {static_cast<double>(tick) / 1000., 0, host->context().clockSegment}, false));
    });
    trace.run("present", [&] {
        REQUIRE(trace.take(host->present(frame)).status == PresentStatus::succeeded);
    });
    const auto end = matrixNow();
    trace.run("seek-sameResult",
              [&] { REQUIRE(trace.take(expected.sameResult(trace.take(host->queryGameplay())))); });
    trace.run("active-memory", [&] { internal::pd::memory::checkpoint("active"); });
    frame = {};
    trace.run("close", [&] {
        trace.take(host->close());
        host.reset();
    });
    trace.run("after-close-memory", [&] { internal::pd::memory::checkpoint("after-close"); });
    std::cout << "PD5_CAPACITY {\"schema\":\"pd5.capacity.v1\",\"reuse\":\"" << reuse
              << "\",\"entities\":" << scale << ",\"requirements\":" << scale
              << ",\"packed_bytes\":" << packed.size() << ",\"snapshot_bytes\":" << bytes.size()
              << ",\"snapshot_valid\":" << (snapshot.valid() ? "true" : "false")
              << ",\"decoded_snapshot_valid\":" << (decodedSnapshot.valid() ? "true" : "false")
              << ",\"advance_executed\":"
              << (decision.disposition == GameplayAdvanceDisposition::Executed ? "true" : "false")
              << ",\"replay_evidence_valid\":" << (evaluated.evidenceValid ? "true" : "false")
              << ",\"frame_byte_bound\":" << frameBound
              << ",\"global_payloads\":2,\"global_payload_bytes\":" << frameBound * 2
              << ",\"replay_equal\":true,\"seek_start_us\":" << started
              << ",\"seek_confirmed_us\":" << confirmed << ",\"seek_present_us\":" << end
              << ",\"seek_us\":" << end - started
              << ",\"closed\":true,\"formal_acceptance\":false}\n";
}
#endif

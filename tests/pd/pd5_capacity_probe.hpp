// Included by pd5_matrix_probe.cpp so the exact typed presentation generator is shared.
#if defined(_WIN32)
#include "pd5_capacity_r4_support.hpp"
TEST_CASE("PD5 full content Snapshot Seek Replay boundary", "[.][pd][pd5][capacity]") {
    using namespace cuexis;
    using namespace playback;
    using namespace playback::detail;
    namespace memory = internal::pd::memory;
    const auto scale = matrixCount("CUEXIS_PD5_ENTITIES", 40000);
    REQUIRE(scale <= 40000);
    // The P8 profile measures entity-scaled state at 40k entries. The packed writer is bounded by
    // the frozen Foundation string budget, so the content rows may be a smaller explicit subset;
    // entity count keeps coming from the source chart below.
    const auto requirements = std::min<unsigned>(scale, matrixCount("CUEXIS_PD5_REQUIREMENTS", scale));
    const auto reuse = test_support::environmentVariable("CUEXIS_PD5_REUSE");
    REQUIRE((reuse == "low" || reuse == "high"));
    const auto revision = test_support::environmentVariable("CUEXIS_PD5_CAPSULE_REVISION");
    REQUIRE((revision.empty() || revision == "3" || revision == "4"));
    const bool r4 = revision == "4";
    const bool diagnosticOnly =
        test_support::environmentVariable("CUEXIS_PD5_ADVANCE_DIAGNOSTIC_ONLY") == "1";
    if (diagnosticOnly) {
        REQUIRE(r4);
        REQUIRE(scale <= 2048);
#if !defined(CUEXIS_EXECUTION_DIAGNOSTICS_TEST_ONLY)
        FAIL("Advance diagnostics require the explicit test-only build");
#endif
    }
    pd5_capacity::Trace trace{r4};
    if (r4)
        trace.run("identity", [&] { pd5_capacity::identity(scale, requirements, reuse); });
    trace.begin("fixture-construction");
    std::unique_ptr<test_support::SessionTestContent> fixture;
    const ClockMappingConfiguration mapping{{0, 1}, {0, 1}, {0, 1}, {1, 1}};
    {
        const memory::TagScope tag{memory::ownerProbe, memory::domainFixture};
        fixture = std::make_unique<test_support::SessionTestContent>();
        test_support::rpa(*fixture, mapping);
        auto& document = fixture->request.assembly.sources[0].document;
        const auto prototype = document.requirements[0];
        const auto entityPrototype = fixture->chart.entities[0];
        document.requirements.clear();
        fixture->chart.entities.clear();
        fixture->owners.clear();
        std::erase_if(document.declarations, [](const auto& d) {
            return d.kind == judgement::DeclarationKind::requirement;
        });
        document.relations[0].members.clear();
        fixture->request.assembly.contentProfileLimits.maxRequirements =
            judgement::MeasuredParameter<std::uint64_t>::measured(requirements);
        for (unsigned i = 0; i < requirements; ++i) {
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
    }
    trace.succeeded("fixture-construction");
    trace.run("before-load-memory", [&] { memory::checkpoint("before-load"); });
    auto packed = trace.run("encode", [&] {
        const memory::TagScope tag{memory::ownerProbe, memory::domainFixture};
        return r4 ? pd5_capacity::encodeR4(*fixture, trace, scale, reuse) : fixture->packed();
    });
    auto content = trace.run("GameplayContent::fromPacked", [&] {
        const memory::TagScope tag{memory::ownerProbe, memory::domainDecode};
        return trace.take(GameplayContent::fromPacked(packed, fixture->config));
    });
    presentation_renderer::TestPresentationRenderer renderer;
    // This profile has exactly scale objects and at most one feedback member per content row.
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
        const memory::TagScope tag{memory::ownerProbe, memory::domainStatic};
        return trace.take(player::ExecutionHost::create(renderer, options, {}));
    });
    trace.run("host-load", [&] {
        const memory::TagScope tag{memory::ownerProbe, memory::domainLoad};
        trace.take(host->load(sparseSource(scale), content));
    });
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
    auto decision = trace.run("host-advance", [&] {
        const memory::TagScope tag{memory::ownerProbe, memory::domainAdvance};
        const auto decision = trace.take(host->advance(trace.take(clock(host->context()))));
        REQUIRE(decision.disposition == GameplayAdvanceDisposition::Executed);
        return decision;
    });
    if (diagnosticOnly) {
        trace.run("diagnostic-close", [&] {
            trace.take(host->close());
            host.reset();
        });
        std::cout << "PD5_ADVANCE_DIAGNOSTIC {\"entities\":" << scale
                  << ",\"advance_executed\":true,\"closed\":true,\"formal_measurement\":false}"
                  << std::endl;
        return;
    }
    auto expected = trace.run("host-query", [&] {
        const memory::TagScope tag{memory::ownerProbe, memory::domainSnapshot};
        auto result = trace.take(host->queryGameplay());
        REQUIRE(trace.take(result.score()).misses == requirements);
        return result;
    });
    auto snapshot = trace.run("snapshot", [&] {
        const memory::TagScope tag{memory::ownerProbe, memory::domainSnapshot};
        return trace.take(host->snapshotGameplay());
    });
    auto bytes = trace.run("snapshot-encode", [&] {
        const memory::TagScope tag{memory::ownerProbe, memory::domainSnapshot};
        return trace.take(snapshot.toBytes());
    });
    // TEST ONLY: the saved 40k low R4 Snapshot charges 63,782,206 elements, including
    // header identity bytes. Declare a finite 64 Mi-element workload bound for R4;
    // revision 3 retains its original declaration. This does not accept production budgets.
    const GameplayCodecBudget snapshotBudget{268435456, 100000,
                                             r4 ? 64U * 1024U * 1024U : 20000000U, true};
    auto decodedSnapshot = trace.run("snapshot-decode", [&] {
        const memory::TagScope tag{memory::ownerProbe, memory::domainDecode};
        if (r4)
            std::cout << "PD5_C_SNAPSHOT_DECODE {\"snapshot_bytes\":" << bytes.size()
                      << ",\"max_bytes\":" << snapshotBudget.maxBytes
                      << ",\"max_records\":" << snapshotBudget.maxRecords
                      << ",\"max_elements\":" << snapshotBudget.maxElements
                      << ",\"test_only\":" << (snapshotBudget.testOnly ? "true" : "false") << '}'
                      << std::endl;
        const auto decodedSnapshot =
            trace.take(GameplaySnapshot::fromBytes(bytes, content, snapshotBudget));
        REQUIRE(decodedSnapshot.valid());
        return decodedSnapshot;
    });
    auto replay = trace.run("archive", [&] {
        const memory::TagScope tag{memory::ownerProbe, memory::domainJournal};
        return trace.take(host->archive());
    });
    auto evaluated = trace.run("evaluateReplay-sameResult", [&] {
        const memory::TagScope tag{memory::ownerProbe, memory::domainReplay};
        const auto evaluated = trace.take(host->evaluateReplay(replay));
        REQUIRE(evaluated.evidenceValid);
        REQUIRE(trace.take(expected.sameResult(evaluated.result)));
        return evaluated;
    });
    trace.run("pause", [&] { trace.take(host->pause(clock)); });
    // Diagnostic-only transient localisation: one checkpoint after each transaction call in the
    // active tail, so a sticky peak that exceeds the following steady read is attributable to the
    // call between the two checkpoints. Checkpoints allocate nothing and never change the payload.
    trace.run("after-pause-memory", [&] { memory::checkpoint("after-pause"); });
    auto cut = trace.run("replayCut", [&] {
        const memory::TagScope tag{memory::ownerProbe, memory::domainReplay};
        return trace.take(host->replayCut(replay, {tick}));
    });
    trace.run("after-replay-cut-memory", [&] { memory::checkpoint("after-replay-cut"); });
    const auto started = matrixNow();
    trace.run("seek", [&] {
        const memory::TagScope tag{memory::ownerProbe, memory::domainSnapshot};
        trace.take(host->seek(replay, cut, {tick}, clock));
    });
    const auto confirmed = matrixNow();
    trace.run("after-seek-memory", [&] { memory::checkpoint("after-seek"); });
    auto frame = trace.run("sample", [&] {
        const memory::TagScope tag{memory::ownerProbe, memory::domainSnapshot};
        return trace.take(host->sample(
            tick, {static_cast<double>(tick) / 1000., 0, host->context().clockSegment}, false));
    });
    trace.run("after-sample-memory", [&] { memory::checkpoint("after-sample"); });
    trace.run("present", [&] {
        const memory::TagScope tag{memory::ownerProbe, memory::domainReady};
        REQUIRE(trace.take(host->present(frame)).status == PresentStatus::succeeded);
    });
    trace.run("after-present-memory", [&] { memory::checkpoint("after-present"); });
    const auto end = matrixNow();
    trace.run("seek-sameResult",
              [&] { REQUIRE(trace.take(expected.sameResult(trace.take(host->queryGameplay())))); });
    trace.run("after-seek-same-result-memory",
              [&] { memory::checkpoint("after-seek-same-result"); });
    trace.run("active-memory", [&] { memory::checkpoint("active"); });
    frame = {};
    trace.run("close", [&] {
        trace.take(host->close());
        host.reset();
    });
    trace.run("after-close-memory", [&] { memory::checkpoint("after-close"); });
    std::cout << "PD5_CAPACITY {\"schema\":\"pd5.capacity.v1\",\"reuse\":\"" << reuse
              << "\",\"entities\":" << scale << ",\"requirements\":" << requirements
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
    // Diagnostic-only release ladder. Each owner is destroyed before its checkpoint, so the
    // next line reads exactly the bytes that owner retained. This attributes the process
    // envelope to owners; it accepts no per-domain budget and is never acceptance input.
    // Vector owners are swapped with a temporary: `owner = {}` keeps the vector capacity and
    // would leave the buffer counted against the harness instead of against its owner.
    decodedSnapshot = {};
    memory::checkpoint("after-decoded-snapshot-release");
    decltype(bytes){}.swap(bytes);
    memory::checkpoint("after-snapshot-bytes-release");
    snapshot = {};
    memory::checkpoint("after-snapshot-dto-release");
    evaluated = {};
    memory::checkpoint("after-evaluated-release");
    expected = {};
    memory::checkpoint("after-expected-release");
    decision = {};
    memory::checkpoint("after-decision-release");
    replay = {};
    cut = {};
    memory::checkpoint("after-replay-release");
    content = {};
    memory::checkpoint("after-content-release");
    decltype(packed){}.swap(packed);
    memory::checkpoint("after-packed-release");
    fixture.reset();
    memory::checkpoint("after-fixture-release");
}
#endif

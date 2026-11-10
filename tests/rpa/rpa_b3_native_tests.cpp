#include "rpa_b3_fixture.hpp"

// Native anchors, identity and archive ownership cases.

using namespace cuexis::judgement;
using namespace cuexis::judgement::testing;
using namespace cuexis::rpa::testing;

TEST_CASE("R3 fixed ordinary allocator recovery cost", "[.r3-cost]") {
    Fixture3 f;
    auto& document = f.assembly.sources[0].document;
    const auto prototype = document.requirements[0];
    document.requirements.clear();
    std::erase_if(document.declarations,
                  [](const auto& d) { return d.kind == DeclarationKind::requirement; });
    document.relations[0].members.clear();
    constexpr unsigned count = 64;
    f.assembly.contentProfileLimits.maxRequirements =
        MeasuredParameter<std::uint64_t>::measured(count);
    for (unsigned i = 0; i < count; ++i) {
        auto r = prototype;
        r.stableId.declarationOrdinal = 100 + i;
        r.identity.requirementLocalId = "r3.cost." + std::to_string(i);
        r.resourceClaims[0].claimPolicy.competition = ClaimPolicyDeclaration::CompetitionKey{-1, i};
        document.declarations.push_back(
            testDeclaration(DeclarationKind::requirement, r.identity.requirementLocalId, 100 + i));
        document.relations[0].members.push_back(r.stableId);
        document.requirements.push_back(std::move(r));
    }
    const RecoveryInputs inputs{f.configuration(), f.prepared()};
    auto live = f.session();
    REQUIRE(live.submitCanonical({occurrence(900, 0, InputAction::press)}));
    REQUIRE(live.advance(Tick{903}));
    const auto saved = live.snapshot();
    REQUIRE(saved);
    auto restored = JudgementSession::recover(*saved, inputs);
    REQUIRE(restored);
    CHECK(sameSnapshot(saved->state(), restored->snapshot()->state()));
    for (unsigned i = 0; i < 64; ++i) {
        auto warm = JudgementSession::recover(*saved, inputs);
        REQUIRE(warm);
    }
    constexpr unsigned repetitions = 2048;
    for (unsigned block = 0; block < 8; ++block) {
        bool success = true;
        const auto started = std::chrono::steady_clock::now();
        for (unsigned i = 0; i < repetitions; ++i) {
            auto recovered = JudgementSession::recover(*saved, inputs);
            success = success && recovered.has_value();
        }
        const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now() - started)
                            .count();
        REQUIRE(success);
        std::cout << "R3_RECOVERY_COST requirements=" << count << " block=" << block
                  << " repetitions=" << repetitions << " elapsed_ns=" << ns << '\n';
    }
}

TEST_CASE("R3 redesign fixed scaling diagnostic compares origin and checkpoint forks",
          "[.][rpa][redesign-perf]") {
    if (cuexis::test_support::environmentVariable("CUEXIS_REDESIGN_BARRIER") == "1") {
        static const bool admitted = [] {
            std::string line;
            return static_cast<bool>(std::getline(std::cin, line)) && line == "go";
        }();
        REQUIRE(admitted);
    }
    const auto count = GENERATE(std::size_t{64}, std::size_t{256}, std::size_t{2048});
    Fixture3 f;
    auto& document = f.assembly.sources[0].document;
    const auto prototype = document.requirements[0];
    document.requirements.clear();
    std::erase_if(document.declarations,
                  [](const auto& d) { return d.kind == DeclarationKind::requirement; });
    document.relations[0].members.clear();
    f.assembly.contentProfileLimits.maxRequirements =
        MeasuredParameter<std::uint64_t>::measured(count);
    for (std::size_t i = 0; i < count; ++i) {
        auto requirement = prototype;
        requirement.stableId.declarationOrdinal = static_cast<std::uint32_t>(100 + i);
        requirement.identity.requirementLocalId = "redesign." + std::to_string(i);
        requirement.resourceClaims[0].claimPolicy.competition =
            ClaimPolicyDeclaration::CompetitionKey{-1, static_cast<std::uint64_t>(i)};
        document.declarations.push_back(testDeclaration(DeclarationKind::requirement,
                                                        requirement.identity.requirementLocalId,
                                                        requirement.stableId.declarationOrdinal));
        document.relations[0].members.push_back(requirement.stableId);
        document.requirements.push_back(std::move(requirement));
    }
    const auto ns = [](auto start) {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
                   std::chrono::steady_clock::now() - start)
            .count();
    };
    auto source = f.session();
    REQUIRE(source.advance(Tick{1103}));
    for (std::int64_t i = 1; i <= 32; ++i)
        REQUIRE(source.advance(Tick{1103 + i}));
    const auto archiveStart = std::chrono::steady_clock::now();
    const auto archive = source.archive();
    const auto archiveNs = ns(archiveStart);
    REQUIRE(archive);
    const auto start = std::chrono::steady_clock::now();
    const auto checkpoint = JudgementSession::checkpoint(*archive, Tick{1119});
    const auto checkpointNs = ns(start);
    REQUIRE(checkpoint);
    const auto expected = source.snapshot();
    REQUIRE(expected);
    const auto cut = replayCutAt(*archive, Tick{1135});
    for (unsigned pair = 0; pair < 3; ++pair) {
        for (const bool hinted : {false, true}) {
            auto started = std::chrono::steady_clock::now();
            auto forked = source.forkForSeek(*archive, Tick{1135}, cut,
                                             hinted ? std::span{&*checkpoint, 1}
                                                    : std::span<const ReplayCheckpoint>{});
            const auto forkNs = ns(started);
            REQUIRE(forked);
            CHECK(detail::KernelTestAccess::certifiedPrefixRecords(*forked) == (hinted ? 17 : 0));
            started = std::chrono::steady_clock::now();
            auto saved = forked->snapshot();
            const auto snapshotNs = ns(started);
            REQUIRE(saved);
            CHECK(sameSnapshot(saved->state(), expected->state()));
            CHECK(forked->query()->kernelView().runScope != source.query()->kernelView().runScope);
            saved = cuexis::core::unexpected(cuexis::core::Error{"test.release", "release"});
            started = std::chrono::steady_clock::now();
            forked = cuexis::core::unexpected(cuexis::core::Error{"test.release", "release"});
            const auto releaseNs = ns(started);
            std::cout << "{\"event\":\"r3.redesign\",\"requirements\":" << count
                      << ",\"pair\":" << pair << ",\"hinted\":" << hinted
                      << ",\"fork_ns\":" << forkNs << ",\"snapshot_ns\":" << snapshotNs
                      << ",\"release_ns\":" << releaseNs << ",\"checkpoint_ns\":" << checkpointNs
                      << ",\"archive_ns\":" << archiveNs << ",\"formal\":false}\n";
        }
    }
}

TEST_CASE("R3 native chunk anchors preserve exact prefixes partial cuts and source ownership",
          "[rpa][redesign][native][recovery]") {
    Fixture3 f;
    auto source = f.session();
    REQUIRE(source.advance(Tick{1103}));
    for (std::int64_t i = 1; i <= 96; ++i)
        REQUIRE(source.advance(Tick{1103 + i}));
    const auto archive = source.archive();
    REQUIRE(archive);
    const auto identity = archive->canonicalIdentity();
    REQUIRE(identity);
    const auto wire = encodeReplay(*archive);
    REQUIRE(wire);
    CHECK(*identity == cuexis::core::detail::sha256(*wire));
    const ReplayArchive external{archive->anchor(),
                                 std::make_shared<const RecoveryInputs>(archive->dependencies())};
    const auto externalIdentity = external.canonicalIdentity();
    REQUIRE(externalIdentity);
    CHECK(*externalIdentity == *identity);
    for (const auto horizon : {1103, 1119, 1134, 1135, 1167, 1199}) {
        INFO(horizon);
        const auto cut = replayCutAt(*archive, Tick{horizon});
        auto origin = source.forkForSeek(external, Tick{horizon}, cut);
        auto native = source.forkForSeek(*archive, Tick{horizon}, cut);
        REQUIRE(origin);
        REQUIRE(native);
        CHECK(detail::KernelTestAccess::nativePrefixRecords(*origin) == 0);
        CHECK(detail::KernelTestAccess::nativePrefixRecords(*native) >= 1);
        CHECK(sameSnapshot(origin->snapshot()->state(), native->snapshot()->state()));
        CHECK(*encodeSnapshot(*origin->snapshot()) == *encodeSnapshot(*native->snapshot()));
        CHECK(native->query()->kernelView().runScope != source.query()->kernelView().runScope);
        REQUIRE(native->submitCanonical({occurrence(1250, 0, InputAction::press)}));
        REQUIRE(native->advance(Tick{1300}));
        const auto branch = native->archive();
        REQUIRE(branch);
        CHECK(branch->anchor() != archive->anchor());
        const auto branchIdentity = branch->canonicalIdentity();
        REQUIRE(branchIdentity);
        CHECK(*branchIdentity != *identity);
        auto evaluated = JudgementSession::evaluateReplay(*branch);
        REQUIRE(evaluated);
        CHECK(sameKernelResult(*evaluated->result, native->query()->kernelView()));
    }
    const ReplayCut partial{32, Tick{1134}};
    auto direct = source.forkForSeek(external, Tick{1134}, partial);
    auto native = source.forkForSeek(*archive, Tick{1134}, partial);
    REQUIRE(direct);
    REQUIRE(native);
    CHECK(detail::KernelTestAccess::nativePrefixRecords(*native) == 32);
    const auto saved = native->snapshot();
    REQUIRE(saved);
    CHECK(sameSnapshot(saved->state(), direct->snapshot()->state()));
    REQUIRE(source.reset());
    REQUIRE(native->reset());
    auto recovered = JudgementSession::recover(*saved, archive->dependencies());
    REQUIRE(recovered);
    CHECK(sameSnapshot(saved->state(), recovered->snapshot()->state()));
    CHECK(*encodeReplay(*archive) == *wire);
}

TEST_CASE("R3 external altered suffix never inherits native source trust",
          "[rpa][redesign][native][rejection]") {
    Fixture3 f;
    auto source = f.session();
    REQUIRE(source.advance(Tick{1103}));
    for (std::int64_t i = 1; i <= 33; ++i)
        REQUIRE(source.advance(Tick{1103 + i}));
    const auto archive = source.archive();
    REQUIRE(archive);
    const auto before = source.snapshot();
    REQUIRE(before);
    auto data = archive->data();
    auto changed = std::make_shared<KernelProjection>(*data.records[32].result);
    changed->facts.update(0, [](auto& fact) { std::get<PhaseOutcomeFact>(fact).commitId += 1; });
    data.records[32].result = changed;
    ReplayArchive external{std::make_shared<const ReplayData>(std::move(data)),
                           std::make_shared<const RecoveryInputs>(archive->dependencies())};
    CHECK_FALSE(source.forkForSeek(external, Tick{1136}, replayCutAt(external, Tick{1136})));
    CHECK(sameSnapshot(before->state(), source.snapshot()->state()));
    CHECK(archive->data().records[32].result->facts != changed->facts);
    CHECK(source.forkForSeek(*archive, Tick{1136}, replayCutAt(*archive, Tick{1136})));
}

TEST_CASE("R3 native branch drops source suffix and external identity never gains cache trust",
          "[rpa][redesign][native][lifetime][identity]") {
    Fixture3 f;
    auto source = f.session();
    REQUIRE(source.advance(Tick{1103}));
    for (int i = 1; i <= 64; ++i)
        REQUIRE(source.advance(Tick{1103 + i}));
    auto archive = source.archive();
    REQUIRE(archive);
    const auto firstIdentity = archive->canonicalIdentity();
    REQUIRE(firstIdentity);
    auto mutableData = std::make_shared<ReplayData>(archive->data());
    ReplayArchive external{mutableData,
                           std::make_shared<const RecoveryInputs>(archive->dependencies())};
    auto externalBefore = external.canonicalIdentity();
    REQUIRE(externalBefore);
    CHECK(*externalBefore == *firstIdentity);
    // A publicly constructed mutable source must re-encode even after its previous hash
    // was valid. The sealed source's bytes and cache remain unaffected.
    mutableData->records.back().horizon = Tick{1168};
    const auto externalAfter = external.canonicalIdentity();
    REQUIRE(externalAfter);
    CHECK(*externalAfter != *externalBefore);
    const auto sealedAfter = archive->canonicalIdentity();
    REQUIRE(sealedAfter);
    CHECK(*sealedAfter == *firstIdentity);
    std::weak_ptr<const KernelProjection> future = archive->data().records[63].result;
    auto branch = source.forkForSeek(*archive, Tick{1104}, replayCutAt(*archive, Tick{1104}));
    REQUIRE(branch);
    REQUIRE(branch->advance(Tick{1105}));
    mutableData->records.clear();
    archive = cuexis::core::unexpected(cuexis::core::Error{"test.release", "release"});
    REQUIRE(source.reset());
    CHECK(future.expired());
    const auto branched = branch->archive();
    REQUIRE(branched);
    auto evaluated = JudgementSession::evaluateReplay(*branched);
    REQUIRE(evaluated);
    CHECK(sameKernelResult(*evaluated->result, branch->query()->kernelView()));
}

TEST_CASE("R3 canonical wire fingerprint matches the preserved implementation",
          "[.][rpa][redesign-wire]") {
    const auto legacy = GENERATE(false, true);
    Fixture3 f;
    if (legacy) {
        f.assembly.executionProfile = "gameplay.execution.t4-k4.v1";
        f.assembly.identityDeclarations.engine.executionProfileToken = f.assembly.executionProfile;
        f.assembly.identityDeclarations.engine.lateAlgorithmToken = "late.window.logical.v1";
        f.assembly.identityDeclarations.engine.factSemanticRevision =
            "fact.semantic.phase-local.v1";
    }
    auto configuration = f.configuration();
    if (legacy) {
        configuration.lateAlgorithmToken = "late.window.logical.v1";
        configuration.factSemanticRevision = "fact.semantic.phase-local.v1";
    }
    auto session = JudgementSession::create();
    REQUIRE(session);
    REQUIRE(session->configure(configuration));
    REQUIRE(session->prepare(f.prepared()));
    auto press = occurrence(900, 0, InputAction::press);
    auto release = occurrence(1000, 1, InputAction::release);
    if (legacy) {
        press.key.sourceLocalPosition.reset();
        release.key.sourceLocalPosition.reset();
    }
    REQUIRE(session->submitCanonical({press, release}));
    REQUIRE(session->advance(Tick{903}));
    if (!legacy) {
        REQUIRE(session->controlRpa({RpaControlKind::pause, Tick{900}, 0, 1, 0, {}}));
        REQUIRE(session->controlRpa({RpaControlKind::resume, Tick{900}, 0, 1, 0, {}}));
    }
    for (int i = 0; i < 5; ++i)
        REQUIRE(session->advance(Tick{1103 + i}));
    const auto archive = session->archive();
    const auto snapshot = session->snapshot();
    REQUIRE(archive);
    REQUIRE(snapshot);
    const auto replayBytes = encodeReplay(*archive);
    const auto snapshotBytes = encodeSnapshot(*snapshot);
    REQUIRE(replayBytes);
    REQUIRE(snapshotBytes);
    std::cout << "R3_WIRE legacy=" << legacy
              << " replay=" << cuexis::core::detail::sha256Hex(*replayBytes)
              << " replay_bytes=" << replayBytes->size()
              << " snapshot=" << cuexis::core::detail::sha256Hex(*snapshotBytes)
              << " snapshot_bytes=" << snapshotBytes->size() << '\n';
}

TEST_CASE("R3 native anchors preserve pending input and pause resume suffixes",
          "[rpa][redesign][native][controls]") {
    Fixture3 f;
    f.rules = controlRules(true);
    auto source = f.session();
    REQUIRE(source.submitCanonical(
        {occurrence(900, 0, InputAction::press), occurrence(1000, 1, InputAction::release)}));
    REQUIRE(source.advance(Tick{903}));
    const auto pending = source.archive();
    REQUIRE(pending);
    auto pendingBranch = source.forkForSeek(*pending, Tick{903}, replayCutAt(*pending, Tick{903}));
    REQUIRE(pendingBranch);
    CHECK(detail::KernelTestAccess::nativePrefixRecords(*pendingBranch) == 0);
    REQUIRE(source.advance(Tick{1003}));
    for (int i = 1; i <= 36; ++i)
        REQUIRE(source.advance(Tick{1003 + i}));
    REQUIRE(source.controlRpa({RpaControlKind::pause, Tick{1036}, 0, 1, 0, {}}));
    REQUIRE(source.controlRpa({RpaControlKind::resume, Tick{1036}, 0, 1, 0, {}}));
    REQUIRE(source.advance(Tick{1103}));
    const auto archive = source.archive();
    REQUIRE(archive);
    const ReplayArchive external{archive->anchor(),
                                 std::make_shared<const RecoveryInputs>(archive->dependencies())};
    for (const auto horizon : {903, 999, 1003, 1035, 1103}) {
        INFO(horizon);
        const auto cut = replayCutAt(*archive, Tick{horizon});
        auto reference = source.forkForSeek(external, Tick{horizon}, cut);
        auto branch = source.forkForSeek(*archive, Tick{horizon}, cut);
        REQUIRE(reference);
        REQUIRE(branch);
        CHECK(sameSnapshot(reference->snapshot()->state(), branch->snapshot()->state()));
        if (horizon == 1103)
            CHECK(detail::KernelTestAccess::nativePrefixRecords(*branch) >= 32);
        REQUIRE(reference->advance(Tick{1203}));
        REQUIRE(branch->advance(Tick{1203}));
        CHECK(*encodeSnapshot(*reference->snapshot()) == *encodeSnapshot(*branch->snapshot()));
    }
}

TEST_CASE("R3 archive copies share a concurrent first canonical identity",
          "[rpa][redesign][identity][concurrency]") {
    Fixture3 f;
    auto source = f.session();
    REQUIRE(source.advance(Tick{1103}));
    const auto archive = source.archive();
    REQUIRE(archive);
    const auto wire = encodeReplay(*archive);
    REQUIRE(wire);
    const auto expected = cuexis::core::detail::sha256(*wire);
    std::array<std::optional<std::array<std::uint8_t, 32>>, 4> actual;
    {
        std::vector<std::jthread> workers;
        for (std::size_t i = 0; i < actual.size(); ++i) {
            workers.emplace_back([copy = *archive, &actual, i] {
                const auto identity = copy.canonicalIdentity();
                if (identity)
                    actual[i] = *identity;
            });
        }
    }
    for (const auto& identity : actual) {
        REQUIRE(identity);
        CHECK(*identity == expected);
    }
}

#include "gameplay_test_fixture.hpp"

#include <cuexis/judgement/gameplay_prepare.hpp>

#include <algorithm>
#include <limits>

namespace {
using namespace cuexis::judgement;
using namespace cuexis::judgement::testing;

auto requestFor(const Fixture& fixture) -> GameplayPrepareRequest {
    auto assembly = fixture.request();
    auto& requirement = assembly.sources.front().document.requirements.front();
    requirement.phases.push_back({PhaseKind::tail, 3});
    requirement.patternArmRefs = {"atom.one"};
    requirement.timing = RequirementRecord::Timing{
        Tick{100},
        {{Tick{10}, Tick{20}, {PhaseKind::head, 1}}, {Tick{100}, Tick{103}, {PhaseKind::tail, 3}}},
        TimeInterval{Tick{20}, Tick{100}}};
    requirement.resourceClaims.front().claimPolicy.competition =
        ClaimPolicyDeclaration::CompetitionKey{-1, 7};
    return {std::move(assembly),
            {{testRequirementStableId(),
              GraceResolutionInputs{makeDuration(1, 1), 0, 10, {}, {}, {}, TickSpan{3}}}},
            {},
            {}};
}
} // namespace

TEST_CASE("S7A-3 immutable prepare integrates compiler timing and resource plan",
          "[judgement][s7a-3][prepare]") {
    Fixture fixture;
    const auto request = requestFor(fixture);
    const auto prepared = prepareGameplay(request);
    REQUIRE(prepared);
    CHECK(prepared->requirements().size() == 1);
    CHECK(prepared->requirements()[0].deadline == Tick{103});
    CHECK(prepared->resources()[0].claims.occupyingCandidateCount() == 1);
    CHECK(prepared->resources()[0].requirementIndices == std::vector<std::size_t>{0});
    CHECK(prepared->admitsSuccess(0, Tick{10}));
    CHECK_FALSE(prepared->admitsSuccess(0, Tick{20}));
    CHECK(prepared->admitsSuccess(0, Tick{102}));
    CHECK_FALSE(prepared->admitsSuccess(0, Tick{103}));
    CHECK_FALSE(prepared->admitsSuccess(1, Tick{10}));
    CHECK(prepared->assembled().graph.requirements[0].timing->body->end == Tick{100});
}

TEST_CASE("S7A-3 failed prepare preserves the entire active product",
          "[judgement][s7a-3][prepare][atomic]") {
    Fixture fixture;
    auto good = requestFor(fixture);
    PreparedGameplayPublication publication;
    REQUIRE(prepareInto(publication, good));
    const auto* active = publication.active();
    const auto identity = active->assembled().prepared;
    auto bad = good;
    SECTION("missing exact value") {
        bad.graceInputs[0].inputs.candidate.reset();
    }
    SECTION("negative before rounding") {
        bad.graceInputs[0].inputs.candidate.reset();
        bad.graceInputs[0].inputs.chartDuration = makeDuration(-1, 2);
        bad.assembly.sources[0].document.requirements[0].grace.allowChartGrace = true;
    }
    SECTION("missing binding") {
        bad.graceInputs.clear();
    }
    SECTION("duplicate binding") {
        bad.graceInputs.push_back(bad.graceInputs[0]);
    }
    SECTION("dangling binding") {
        bad.graceInputs[0].requirement.sourceDocumentId = "absent";
    }
    SECTION("missing timing") {
        bad.assembly.sources[0].document.requirements[0].timing.reset();
    }
    SECTION("deadline overflow") {
        auto& timing = *bad.assembly.sources[0].document.requirements[0].timing;
        timing.end = Tick{std::numeric_limits<std::int64_t>::max()};
        timing.body->end = timing.end;
    }
    SECTION("window past D") {
        bad.assembly.sources[0].document.requirements[0].timing->successWindows[1].end = Tick{104};
    }
    SECTION("empty window") {
        auto& window = bad.assembly.sources[0].document.requirements[0].timing->successWindows[0];
        window.end = window.start;
    }
    SECTION("overlapping windows") {
        auto& windows = bad.assembly.sources[0].document.requirements[0].timing->successWindows;
        windows.push_back({Tick{19}, Tick{21}, {PhaseKind::head, 1}});
    }
    SECTION("undeclared phase ordinal") {
        bad.assembly.sources[0]
            .document.requirements[0]
            .timing->successWindows[0]
            .phase.declarationOrdinal = 999;
    }
    SECTION("body extension") {
        bad.assembly.sources[0].document.requirements[0].timing->body->end = Tick{103};
    }
    SECTION("missing explicit arms") {
        bad.assembly.sources[0].document.requirements[0].patternArmRefs.clear();
    }
    SECTION("atom outside arms") {
        bad.assembly.sources[0].document.requirements[0].patternArmRefs = {"atom.other"};
    }
    SECTION("missing competition") {
        bad.assembly.sources[0]
            .document.requirements[0]
            .resourceClaims[0]
            .claimPolicy.competition.reset();
    }
    SECTION("compiled budget") {
        bad.patternBudget.maxCompiledBytes = MeasuredParameter<std::uint64_t>::measured(0);
    }
    CHECK_FALSE(prepareInto(publication, bad));
    CHECK(publication.active() == active);
    CHECK(publication.active()->assembled().prepared == identity);
}

TEST_CASE("S7A-3 incomplete arm or deadline containment is a stable prepare rejection",
          "[judgement][s7a-3][prepare][containment]") {
    Fixture fixture;
    auto good = requestFor(fixture);
    PreparedGameplayPublication publication;
    REQUIRE(prepareInto(publication, good));
    const auto* active = publication.active();
    const auto identity = active->assembled().prepared;

    auto incomplete = good;
    auto& requirement = incomplete.assembly.sources[0].document.requirements[0];
    requirement.maxArmElements.reset();
    const auto rejected = prepareInto(publication, incomplete);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().code() == "judgement.s7a3.declaration.structurally_incomplete");
    CHECK(publication.active() == active);
    CHECK(publication.active()->assembled().prepared == identity);
}

TEST_CASE("S7A-3 T4 no-tail deadline is end even with nonzero grace",
          "[judgement][s7a-3][prepare][t4]") {
    Fixture fixture;
    auto request = requestFor(fixture);
    auto& requirement = request.assembly.sources[0].document.requirements[0];
    requirement.phases.pop_back();
    auto& last = requirement.timing->successWindows[1];
    last.phase = {PhaseKind::head, 1};
    last.start = Tick{90};
    last.end = Tick{100};
    const auto prepared = prepareGameplay(request);
    REQUIRE(prepared);
    CHECK(prepared->requirements()[0].deadline == Tick{100});
    CHECK(prepared->admitsSuccess(0, Tick{99}));
    CHECK_FALSE(prepared->admitsSuccess(0, Tick{100}));
    last.end = Tick{101};
    CHECK_FALSE(prepareGameplay(request));

    // No-tail does not perform irrelevant end+grace arithmetic.
    last.end = Tick{std::numeric_limits<std::int64_t>::max()};
    requirement.timing->end = last.end;
    requirement.timing->body->end = last.end;
    REQUIRE(prepareGameplay(request));
}

TEST_CASE("S7A-3 overlapping windows on different phases retain independent meaning",
          "[judgement][s7a-3][prepare][t4]") {
    Fixture fixture;
    auto request = requestFor(fixture);
    request.assembly.sources[0].document.requirements[0].timing->successWindows[1].start =
        Tick{100};
    REQUIRE(prepareGameplay(request));
}

TEST_CASE("S7A-3 resolved grace source changes content but not judgement identity",
          "[judgement][s7a-3][prepare][identity]") {
    Fixture fixture;
    auto request = requestFor(fixture);
    const auto explicitValue = prepareGameplay(request);
    REQUIRE(explicitValue);
    auto& requirement = request.assembly.sources[0].document.requirements[0];
    requirement.grace = {GraceResolutionPolicy::defaultDeclaration, {}, false};
    request.graceInputs[0].inputs.candidate.reset();
    request.graceInputs[0].inputs.defaultDuration = makeDuration(3, 1);
    const auto defaultValue = prepareGameplay(request);
    REQUIRE(defaultValue);
    CHECK(explicitValue->assembled().chart == defaultValue->assembled().chart);
    CHECK(explicitValue->assembled().prepared == defaultValue->assembled().prepared);
    CHECK_FALSE(explicitValue->assembled().content == defaultValue->assembled().content);
    requirement.grace = {GraceResolutionPolicy::inheritedDeclaration, "literal.three", false};
    request.graceInputs[0].inputs.defaultDuration.reset();
    request.namedGraceDurations = {{"literal.three", makeDuration(3, 1)}};
    const auto inheritedValue = prepareGameplay(request);
    REQUIRE(inheritedValue);
    CHECK(explicitValue->assembled().prepared == inheritedValue->assembled().prepared);
    CHECK_FALSE(defaultValue->assembled().content == inheritedValue->assembled().content);
}

TEST_CASE("S7A-3 prepared storage outlives all input bindings",
          "[judgement][s7a-3][prepare][ownership]") {
    const auto make = [] {
        Fixture fixture;
        return prepareGameplay(requestFor(fixture));
    };
    const auto prepared = make();
    REQUIRE(prepared);
    CHECK(prepared->assembled().graph.timebase->profileId == "engine.tick.us.v1");
    CHECK(prepared->assembled().graph.timebase->unitToken == "us");
    CHECK(makeChartIdentity(prepared->assembled().graph) == prepared->assembled().chart);
    const auto copy = *prepared;
    CHECK(copy.admitsSuccess(0, Tick{102}));
}

TEST_CASE("S7A-3 inheritance is one named literal and zero grace does not extend body",
          "[judgement][s7a-3][prepare][grace]") {
    Fixture fixture;
    auto request = requestFor(fixture);
    auto& requirement = request.assembly.sources[0].document.requirements[0];
    requirement.grace = {GraceResolutionPolicy::inheritedDeclaration, "literal.one", false};
    request.graceInputs[0].inputs.candidate.reset();
    request.namedGraceDurations = {{"literal.one", makeDuration(5, 2)}};
    requirement.timing->body->end = Tick{99};
    requirement.timing->successWindows[1].start = Tick{99};
    requirement.timing->successWindows[1].end = Tick{102};
    const auto inherited = prepareGameplay(request);
    REQUIRE(inherited);
    CHECK(inherited->requirements()[0].deadline == Tick{102});
    request.namedGraceDurations[0].duration = makeDuration(0, 1);
    requirement.timing->successWindows[1].start = Tick{99};
    requirement.timing->successWindows[1].end = Tick{100};
    requirement.timing->body->end = Tick{99};
    const auto zero = prepareGameplay(request);
    REQUIRE(zero);
    CHECK(zero->requirements()[0].deadline == Tick{100});
    CHECK_FALSE(zero->admitsSuccess(0, Tick{100}));
    request.namedGraceDurations[0].declarationId = "wrong.literal";
    CHECK_FALSE(prepareGameplay(request));
}

TEST_CASE("S7A-3 competition keys have no identity fallback",
          "[judgement][s7a-3][prepare][resource]") {
    ResourceClaimResolutionInputs input{
        1,
        {},
        {{ResourceClaimIntent::claim, "policy", "z", GraceOverrideMode::none,
          ClaimPolicyDeclaration::CompetitionKey{-5, 2}},
         {ResourceClaimIntent::consume, "policy", "a", GraceOverrideMode::none,
          ClaimPolicyDeclaration::CompetitionKey{1, 0}},
         {ResourceClaimIntent::observe, "policy", "", GraceOverrideMode::none, {}}},
        PreparedGrace{TickSpan{0}}};
    const auto plan = resolveResourceClaims(input);
    REQUIRE(plan);
    CHECK(plan->occupyingCandidateCount() == 2);
    CHECK(plan->candidates()[1].claimKeyToken == "z");
    CHECK(plan->candidates()[2].claimKeyToken == "a");
    std::reverse(input.candidates.begin(), input.candidates.end());
    const auto permuted = resolveResourceClaims(input);
    REQUIRE(permuted);
    CHECK(plan->candidates() == permuted->candidates());
    input.candidates[1].competition = input.candidates[2].competition;
    CHECK_FALSE(resolveResourceClaims(input));
}

TEST_CASE("S7A-3 frozen artifacts share prepare without rerunning the grace resolver",
          "[judgement][s7a-3][prepare][resolved]") {
    Fixture fixture;
    auto author = requestFor(fixture);
    author.assembly.sources[0].document.requirements[0].grace = {
        GraceResolutionPolicy::inheritedDeclaration, "literal.three", false};
    author.graceInputs[0].inputs.candidate.reset();
    author.namedGraceDurations = {{"literal.three", makeDuration(3, 1)}};
    const auto source = prepareGameplay(author);
    REQUIRE(source);
    ResolvedGameplayPrepareRequest frozen{author.assembly, author.patternBudget};
    frozen.assembly.entryKind = EntryKind::packedChart;
    const auto artifact = prepareResolvedGameplay(frozen);
    REQUIRE(artifact);
    CHECK(semanticDiff(source->assembled().graph, artifact->assembled().graph).empty());
    CHECK(source->assembled().prepared == artifact->assembled().prepared);
    CHECK(source->assembled().content == artifact->assembled().content);
    CHECK(artifact->requirements()[0].deadline == Tick{103});

    PreparedGameplayPublication publication;
    REQUIRE(prepareResolvedInto(publication, frozen));
    const auto* active = publication.active();
    auto& requirement = frozen.assembly.sources[0].document.requirements[0];
    SECTION("negative frozen ticks") {
        requirement.preparedGrace = PreparedGrace{TickSpan{-1}};
    }
    SECTION("unknown source tag") {
        requirement.grace.policy = static_cast<GraceResolutionPolicy>(255);
    }
    SECTION("missing inherited id") {
        requirement.grace.inheritedFromDeclarationId.clear();
    }
    SECTION("window phase") {
        requirement.timing->successWindows[0].phase.declarationOrdinal = 77;
    }
    SECTION("body interval") {
        requirement.timing->body->end = Tick{103};
    }
    SECTION("deadline overflow") {
        requirement.timing->end = Tick{std::numeric_limits<std::int64_t>::max()};
        requirement.timing->body->end = requirement.timing->end;
    }
    SECTION("containment incomplete") {
        requirement.maxArmElements.reset();
    }
    SECTION("compiler limit") {
        frozen.patternBudget.maxCompiledBytes = MeasuredParameter<std::uint64_t>::measured(0);
    }
    SECTION("capacity") {
        frozen.assembly.sources[0].document.resources[0].declaredCapacity = 2;
    }
    SECTION("competition pair") {
        requirement.resourceClaims[0].claimPolicy.competition.reset();
    }
    CHECK_FALSE(prepareResolvedInto(publication, frozen));
    CHECK(publication.active() == active);
    CHECK(publication.active()->assembled().prepared == source->assembled().prepared);
}

TEST_CASE("S2 grace source failures are atomic and never invent values",
          "[judgement][s7a-3][prepare][grace][hostile]") {
    Fixture fixture;
    auto good = requestFor(fixture);
    PreparedGameplayPublication publication;
    REQUIRE(prepareInto(publication, good));
    const auto* active = publication.active();
    const auto identity = active->assembled().prepared;
    auto bad = good;
    auto& r = bad.assembly.sources[0].document.requirements[0];
    auto& inputs = bad.graceInputs[0].inputs;
    SECTION("negative canonical") {
        inputs.candidate = TickSpan{-1};
    }
    SECTION("outside declared range") {
        inputs.candidate = TickSpan{11};
    }
    SECTION("reversed range") {
        inputs.minimumCanonical = 9;
        inputs.maximumCanonical = 8;
    }
    SECTION("zero quantization unit") {
        inputs.unitInTicks = makeDuration(0, 1);
    }
    SECTION("negative quantization unit") {
        inputs.unitInTicks = makeDuration(-1, 1);
    }
    SECTION("chart and canonical sources conflict") {
        inputs.chartDuration = makeDuration(3, 1);
    }
    SECTION("chart override forbidden") {
        inputs.candidate.reset();
        inputs.chartDuration = makeDuration(3, 1);
        r.grace.allowChartGrace = false;
    }
    SECTION("missing default") {
        r.grace = {GraceResolutionPolicy::defaultDeclaration, {}, false};
        inputs.candidate.reset();
        inputs.defaultDuration.reset();
    }
    SECTION("inheritance is not implicit name lookup") {
        r.grace = {GraceResolutionPolicy::inheritedDeclaration, "missing.literal", false};
        inputs.candidate.reset();
        bad.namedGraceDurations.clear();
    }
    SECTION("duplicate named source") {
        r.grace = {GraceResolutionPolicy::inheritedDeclaration, "literal", false};
        inputs.candidate.reset();
        bad.namedGraceDurations = {{"literal", makeDuration(3, 1)},
                                   {"literal", makeDuration(4, 1)}};
    }
    SECTION("negative duration rejected before rounding") {
        inputs.candidate.reset();
        inputs.chartDuration = makeDuration(-1, 3);
        r.grace.allowChartGrace = true;
    }
    SECTION("rational quantization overflow") {
        inputs.candidate.reset();
        inputs.chartDuration = makeDuration(INT64_MAX, 1);
        inputs.unitInTicks = makeDuration(1, INT64_MAX);
        r.grace.allowChartGrace = true;
    }
    auto rejected = prepareInto(publication, bad);
    REQUIRE_FALSE(rejected);
    CHECK_FALSE(rejected.error().code().empty());
    CHECK(publication.active() == active);
    CHECK(publication.active()->assembled().prepared == identity);
}

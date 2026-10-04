#include "gameplay_test_fixture.hpp"
#include "packed_fixture_support.hpp"
#include "packed_gameplay_internal.hpp"

#include <cuexis/gameplay_packed/gameplay_capsule.hpp>
#include <cuexis/chart/packed_chart_io.hpp>
#include <cuexis_internal/sha256.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>

namespace {
using namespace cuexis;
using namespace judgement;
using namespace judgement::testing;
namespace capsule = gameplay_packed;
namespace packed = chart::packed;
namespace support = chart::packed::test;
using Bytes = std::vector<std::byte>;

struct CapsuleFixture final {
    Fixture base;
    ResolvedGameplayPrepareRequest request;
    chart::CanonicalSemanticChart chart;
    std::vector<capsule::RequirementOwner> owners;
    capsule::CapsuleProfiles profiles{"normalization.one", "coordination.policy.one"};

    CapsuleFixture() : request{base.request(), {}} {
        auto& r = request.assembly.sources[0].document.requirements[0];
        r.localClosePolicyToken = "local.close.t4.v1";
        r.phases.push_back({PhaseKind::tail, 3});
        r.patternArmRefs = {"atom.one"};
        r.timing = RequirementRecord::Timing{
            Tick{100}, {{Tick{10}, Tick{20}, {PhaseKind::head, 1}},
                        {Tick{100}, Tick{103}, {PhaseKind::tail, 3}}},
            TimeInterval{Tick{20}, Tick{100}}};
        r.resourceClaims[0].claimPolicy.competition = ClaimPolicyDeclaration::CompetitionKey{-1, 7};
        request.assembly.sources[0].document.relations[0].policy.claimKeyToken.clear();
        chart.chartId.value = "019b0000-0000-7abc-8def-000000000001";
        chart.features = {{std::string{kFeatureId}, 1}, {std::string{kRulesetFeatureId}, 1}};
        chart.mainMusic = chart::AssetId{"music.one"};
        chart.resourceClosure.resources = {{*chart.mainMusic, chart::CanonicalResourceUseKind::MainMusic},
            {chart::AssetId{"mesh.one"}, chart::CanonicalResourceUseKind::RenderableMesh},
            {chart::AssetId{"material.one"}, chart::CanonicalResourceUseKind::RenderableMaterial}};
        std::sort(chart.resourceClosure.resources.begin(), chart.resourceClosure.resources.end());
        chart::CanonicalEntity entity;
        entity.identity = chart::ExplicitEntityIdentity{chart::ChartObjectId{"019b0000-0000-7abc-8def-000000000002"}};
        entity.components = {chart::CanonicalTransform{}, chart::CanonicalRenderable{
            chart::AssetId{"mesh.one"}, chart::AssetId{"material.one"}, 37}};
        chart.entities.push_back(entity);
        auto visual = entity;
        visual.identity = chart::ExplicitEntityIdentity{chart::ChartObjectId{"019b0000-0000-7abc-8def-000000000003"}};
        visual.parent = entity.identity;
        chart.entities.push_back(visual);
        owners.push_back({r.identity, entity.identity});
    }
    auto prepare() const -> PreparedGameplay {
        auto result = prepareResolvedGameplay(request);
        INFO((result ? "" : std::string{result.error().code()} + ": " +
                                std::string{result.error().message()}));
        REQUIRE(result); return *result;
    }
    auto context() const -> capsule::DecodeContext {
        return {request.assembly.capabilityContext, request.assembly.contentProfileLimits,
                request.patternBudget, request.assembly.identityDeclarations,
                profiles.coordinatorPolicyToken};
    }
    auto encode(const PreparedGameplay& prepared) const -> Bytes {
        const auto result = capsule::encode({chart, prepared, owners, profiles});
        INFO((result ? "" : std::string{result.error().code()} + ": " +
                                std::string{result.error().message()}));
        REQUIRE(result); return *result;
    }
};

auto u32(const Bytes& bytes, std::size_t at) -> std::uint32_t {
    packed::ByteReader r{std::span{bytes}.subspan(at, 4)};
    const auto value = r.readU32(); REQUIRE(value); return *value;
}
void setU32(Bytes& bytes, std::size_t at, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i) { bytes[at + i] = static_cast<std::byte>((value >> (i * 8)) & 255); }
}
auto directory(const Bytes& bytes, std::string_view code) -> std::size_t {
    for (std::uint32_t i = 0; i < u32(bytes, 24); ++i) {
        const auto at = 96 + static_cast<std::size_t>(i) * 32;
        if (std::string_view{reinterpret_cast<const char*>(bytes.data() + at), 4} == code) { return at; }
    }
    FAIL("section missing");
}
void refreshHeader(Bytes& bytes) { setU32(bytes, 92, packed::crc32(std::span{bytes}.first(92))); }
void refreshSection(Bytes& bytes, std::size_t dir) {
    setU32(bytes, dir + 24, packed::crc32(std::span{bytes}.subspan(u32(bytes, dir + 8), u32(bytes, dir + 12))));
    refreshHeader(bytes);
}
auto rewrite(Bytes bytes, bool reverse, std::string inspection = {}) -> Bytes {
    struct Section { Bytes directory; Bytes payload; };
    std::vector<Section> sections;
    for (std::uint32_t i = 0; i < u32(bytes, 24); ++i) {
        const auto at = 96 + static_cast<std::size_t>(i) * 32;
        const auto offset = u32(bytes, at + 8), length = u32(bytes, at + 12);
        sections.push_back({Bytes{bytes.begin() + at, bytes.begin() + at + 32},
            Bytes{bytes.begin() + offset, bytes.begin() + offset + length}});
    }
    if (reverse) { std::reverse(sections.begin(), sections.end()); }
    if (!inspection.empty()) {
        Bytes row(32), payload{std::byte{0x42}};
        for (std::size_t i = 0; i < 4; ++i) { row[i] = static_cast<std::byte>(inspection[i]); }
        row[5] = std::byte{1}; setU32(row, 12, 1); setU32(row, 16, 1);
        setU32(row, 24, packed::crc32(payload)); sections.push_back({std::move(row), std::move(payload)});
    }
    Bytes result{bytes.begin(), bytes.begin() + 96};
    const auto count = static_cast<std::uint32_t>(sections.size());
    auto offset = 96U + count * 32;
    for (auto& s : sections) {
        setU32(s.directory, 8, offset); offset += static_cast<std::uint32_t>(s.payload.size());
        result.insert(result.end(), s.directory.begin(), s.directory.end());
    }
    for (const auto& s : sections) { result.insert(result.end(), s.payload.begin(), s.payload.end()); }
    setU32(result, 16, static_cast<std::uint32_t>(result.size())); setU32(result, 24, count);
    setU32(result, 28, count * 32);
    setU32(result, 76, static_cast<std::uint32_t>(result.size()) - 96 - count * 32);
    refreshHeader(result); return result;
}
auto digestText(std::span<const std::byte> bytes) -> std::string {
    const auto digest = core::detail::sha256(bytes);
    constexpr char hex[] = "0123456789abcdef";
    std::string result;
    for (auto b : digest) { result += hex[b >> 4]; result += hex[b & 15]; }
    return result;
}
struct Workspace final {
    std::filesystem::path path;
    Workspace() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (unsigned i = 0; i < 100; ++i) {
            auto candidate = std::filesystem::temp_directory_path() /
                ("cuexis-capsule-" + std::to_string(stamp) + "-" + std::to_string(i));
            if (std::filesystem::create_directory(candidate)) {
                path = std::move(candidate); return;
            }
        }
        FAIL("cannot create an exclusive test workspace");
    }
    ~Workspace() {
        std::error_code error;
        if (!path.empty()) { std::filesystem::remove_all(path, error); }
    }
};
auto readBytes(const std::filesystem::path& path) -> Bytes {
    std::ifstream stream{path, std::ios::binary}; REQUIRE(stream);
    Bytes bytes;
    char value{};
    while (stream.get(value)) { bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(value))); }
    return bytes;
}
} // namespace

TEST_CASE("Capsule complete typed chart round trip uses shared prepare", "[capsule][roundtrip]") {
    CapsuleFixture f;
    const auto prepared = f.prepare(), copy = prepared;
    const auto bytes = f.encode(prepared);
    const auto decoded = capsule::decode(bytes, f.context());
    INFO((decoded ? "" : std::string{decoded.error().code()} + ": " +
                            std::string{decoded.error().message()}));
    REQUIRE(decoded);
    CHECK(judgement::semanticDiff(prepared.assembled().graph, decoded->gameplay.assembled().graph).empty());
    CHECK(decoded->gameplay.assembled().prepared == copy.assembled().prepared);
    CHECK(decoded->gameplay.assembled().content == copy.assembled().content);
    CHECK(decoded->gameplay.requirements()[0].deadline == Tick{103});
    CHECK(decoded->gameplay.admitsSuccess(0, Tick{102}));
    CHECK_FALSE(decoded->gameplay.admitsSuccess(0, Tick{103}));
    CHECK(decoded->chart.entities == f.chart.entities);
    CHECK(decoded->chart.resourceClosure == f.chart.resourceClosure);
    CHECK(decoded->owners[0].entity == f.owners[0].entity);
    CHECK(decoded->gameplay.assembled().contentProfileVerdict == ContentProfileVerdict::notEnforcedPendingBounds);
    CHECK(capsule::encode({decoded->chart, decoded->gameplay, decoded->owners, decoded->profiles,
                          decoded->patterns, decoded->measures}).value() == bytes);
    CHECK_FALSE(packed::decode(bytes));
}

TEST_CASE("Capsule source provenance does not change frozen semantic identity", "[capsule][identity]") {
    CapsuleFixture f;
    const auto original = f.encode(f.prepare());
    for (auto policy : {GraceResolutionPolicy::inheritedDeclaration, GraceResolutionPolicy::defaultDeclaration}) {
        auto& grace = f.request.assembly.sources[0].document.requirements[0].grace;
        grace.policy = policy;
        grace.inheritedFromDeclarationId = policy == GraceResolutionPolicy::inheritedDeclaration ? "literal.three" : "";
        const auto bytes = f.encode(f.prepare());
        CHECK(bytes != original);
        CHECK(std::equal(bytes.begin() + 32, bytes.begin() + 64, original.begin() + 32));
        const auto decoded = capsule::decode(bytes, f.context());
        REQUIRE(decoded); CHECK(decoded->gameplay.assembled().graph.requirements[0].grace == grace);
    }
}

TEST_CASE("Capsule Reader accepts section permutation and bounded inspection", "[capsule][layout]") {
    CapsuleFixture f;
    const auto bytes = f.encode(f.prepare());
    for (const auto& altered : {rewrite(bytes, true), rewrite(bytes, false, "DBG0"), rewrite(bytes, true, "ZZZ0")}) {
        const auto decoded = capsule::decode(altered, f.context());
        INFO((decoded ? "" : std::string{decoded.error().code()} + ": " +
                                std::string{decoded.error().message()}));
        REQUIRE(decoded);
        CHECK(capsule::encode({decoded->chart, decoded->gameplay, decoded->owners, decoded->profiles,
                             decoded->patterns, decoded->measures}).value() == bytes);
    }
}

TEST_CASE("Capsule hostile framing fails before atomic publication", "[capsule][hostile][atomic]") {
    CapsuleFixture f;
    const auto good = f.encode(f.prepare());
    std::optional<capsule::PreparedCapsule> active;
    REQUIRE(capsule::decodeInto(active, good, f.context()));
    const auto identity = active->gameplay.assembled().prepared;
    auto bytes = good;
    SECTION("revision") { setU32(bytes, 88, 1); refreshHeader(bytes); }
    SECTION("header count") { setU32(bytes, 68, 2); refreshHeader(bytes); }
    SECTION("digest") { bytes[32] ^= std::byte{1}; refreshHeader(bytes); }
    SECTION("CRC") { bytes.back() ^= std::byte{1}; }
    SECTION("missing Gameplay section") { const auto at = directory(bytes, "GPH0"); bytes[at] = std::byte{'Z'}; }
    SECTION("table flags") { const auto at = directory(bytes, "GPH0"); bytes[u32(bytes, at + 8) + 3] = std::byte{1}; refreshSection(bytes, at); }
    SECTION("table revision") { const auto at = directory(bytes, "GPH0"); bytes[u32(bytes, at + 8)] = std::byte{2}; refreshSection(bytes, at); }
    SECTION("table codec") { const auto at = directory(bytes, "GPD0"); bytes[u32(bytes, at + 8) + 2] = std::byte{2}; refreshSection(bytes, at); }
    SECTION("table rowCount") { const auto at = directory(bytes, "GPR0"); setU32(bytes, u32(bytes, at + 8) + 4, 2); refreshSection(bytes, at); }
    SECTION("table payloadBytes") { const auto at = directory(bytes, "GRC0"); setU32(bytes, u32(bytes, at + 8) + 8, 0); refreshSection(bytes, at); }
    SECTION("entity ordinal") { const auto at = directory(bytes, "GPR0"); bytes[u32(bytes, at + 8) + 16] = std::byte{99}; refreshSection(bytes, at); }
    SECTION("nonshort varint") { const auto at = directory(bytes, "GPH0"); const auto p = u32(bytes, at + 8) + 16; bytes[p] = std::byte{0x82}; bytes[p + 1] = std::byte{0}; refreshSection(bytes, at); }
    SECTION("unknown ref kind") { const auto at = directory(bytes, "REF0"); bytes[u32(bytes, at + 8)] = std::byte{14}; refreshSection(bytes, at); }
    SECTION("truncated") { bytes.pop_back(); }
    SECTION("trailing") { bytes.push_back(std::byte{0}); }
    const auto result = capsule::decodeInto(active, bytes, f.context());
    CHECK_FALSE(result); CHECK(active->gameplay.assembled().prepared == identity);
}

TEST_CASE("Capsule execution context and accepted budgets cannot be bypassed", "[capsule][context][budget]") {
    CapsuleFixture f; const auto bytes = f.encode(f.prepare());
    auto context = f.context();
    SECTION("normalization") { context.identities.session.normalizationProfileToken = "other"; }
    SECTION("coordinator") { context.coordinatorPolicyToken = "other"; }
    SECTION("disabled capability") { context.capabilities.enabledCapabilityIds.clear(); }
    SECTION("unknown capability") { context.capabilities.recognisedCapabilityIds.clear(); }
    SECTION("content budget") { context.contentLimits.maxRequirements = MeasuredParameter<std::uint64_t>::measured(0); }
    SECTION("compiled budget") { context.patternBudget.maxCompiledBytes = MeasuredParameter<std::uint64_t>::measured(0); }
    CHECK_FALSE(capsule::decode(bytes, context));
}

TEST_CASE("Capsule instance claims share a namespace but not a structural key",
          "[capsule][resource][instances]") {
    CapsuleFixture f;
    auto& doc = f.request.assembly.sources[0].document;
    auto second = doc.requirements[0];
    second.stableId.declarationOrdinal = 5;
    second.identity.requirementLocalId = "requirement.two";
    second.identity.emissionPath[0].repeatIndex = 1;
    second.resourceClaims[0].intent = ResourceClaimIntent::consume;
    second.resourceClaims[0].claimPolicy.competition =
        ClaimPolicyDeclaration::CompetitionKey{-1, 3};
    doc.declarations.push_back(
        testDeclaration(DeclarationKind::requirement, "requirement.two", 5));
    doc.relations[0].members.push_back(second.stableId);
    f.owners.push_back({second.identity, f.chart.entities[0].identity});
    doc.requirements.push_back(second);
    const auto prepared = f.prepare();
    REQUIRE(prepared.resources()[0].claims.candidates().size() == 2);
    CHECK(prepared.resources()[0].requirementIndices == std::vector<std::size_t>{1, 0});
    CHECK(prepared.resources()[0].claims.candidates()[0].claimKeyToken !=
          prepared.resources()[0].claims.candidates()[1].claimKeyToken);
    const auto bytes = f.encode(prepared);
    const auto decoded = capsule::decode(bytes, f.context());
    REQUIRE(decoded);
    CHECK(decoded->gameplay.resources()[0].claims.candidates() ==
          prepared.resources()[0].claims.candidates());
    CHECK(decoded->gameplay.assembled().graph.requirements[1].resourceClaims[0].intent ==
          ResourceClaimIntent::consume);
    std::reverse(doc.requirements.begin(), doc.requirements.end());
    std::reverse(doc.declarations.begin(), doc.declarations.end());
    std::reverse(doc.relations[0].members.begin(), doc.relations[0].members.end());
    std::reverse(f.chart.entities.begin(), f.chart.entities.end());
    std::reverse(f.owners.begin(), f.owners.end());
    CHECK(f.encode(f.prepare()) == bytes);
}

TEST_CASE("Capsule empty requirement graph preserves independent definitions",
          "[capsule][roundtrip][empty]") {
    CapsuleFixture f;
    auto& doc = f.request.assembly.sources[0].document;
    doc.requirements.clear();
    doc.relations.clear();
    doc.declarations.erase(std::remove_if(doc.declarations.begin(), doc.declarations.end(),
        [](const auto& d) { return d.kind == DeclarationKind::requirement; }), doc.declarations.end());
    f.owners.clear();
    f.chart.features = {{std::string{kRulesetFeatureId}, 1}};
    const auto prepared = f.prepare();
    const auto bytes = f.encode(prepared);
    const auto decoded = capsule::decode(bytes, f.context());
    REQUIRE(decoded);
    CHECK(decoded->gameplay.requirements().empty());
    CHECK(decoded->gameplay.resources().size() == 1);
    CHECK(decoded->gameplay.assembled().graph.judgementDomains.size() == 1);
    CHECK(decoded->patterns.empty());
    CHECK(decoded->measures.empty());
    CHECK_FALSE(decoded->gameplay.admitsSuccess(0, Tick{0}));
    CHECK(capsule::encode({decoded->chart, decoded->gameplay, decoded->owners,
                           decoded->profiles}).value() == bytes);
}

TEST_CASE("Capsule Tap and resource intent remain explicit", "[capsule][roundtrip][tap][intent]") {
    CapsuleFixture f;
    auto& r = f.request.assembly.sources[0].document.requirements[0];
    r.phases = {{PhaseKind::tap, 7}};
    r.measure.components = {{PhaseKind::tap, "tap", {"perfect", "good"}}};
    r.timing = RequirementRecord::Timing{Tick{100}, {{Tick{90}, Tick{100}, {PhaseKind::tap, 7}}}, {}};
    auto& claim = r.resourceClaims[0];
    SECTION("observe") {
        claim.intent = ResourceClaimIntent::observe;
        claim.claimPolicy.claimKeyToken.clear();
        claim.claimPolicy.competition.reset();
    }
    SECTION("consume") { claim.intent = ResourceClaimIntent::consume; }
    SECTION("claim") { claim.intent = ResourceClaimIntent::claim; }
    const auto prepared = f.prepare();
    const auto bytes = f.encode(prepared);
    const auto decoded = capsule::decode(bytes, f.context());
    REQUIRE(decoded);
    CHECK(decoded->gameplay.assembled().graph.requirements[0].resourceClaims == r.resourceClaims);
    CHECK(decoded->gameplay.requirements()[0].deadline == Tick{100});
    CHECK(decoded->gameplay.admitsSuccess(0, Tick{99}));
    CHECK_FALSE(decoded->gameplay.admitsSuccess(0, Tick{100}));
    CHECK(decoded->gameplay.assembled().graph.requirements[0].measure == r.measure);
    CHECK(decoded->gameplay.resources()[0].claims.occupyingCandidateCount() ==
          (claim.intent == ResourceClaimIntent::observe ? 0 : 1));
}

TEST_CASE("Capsule exact timebase domain actions and ordered grades round trip",
          "[capsule][roundtrip][typed]") {
    CapsuleFixture f;
    f.base.timebase.originBeat = makeBeat(-1, 3);
    f.base.timebase.initialTempo = makeDuration(1000000, 3);
    f.base.timebase.tempoSections = {{makeBeat(4, 3), makeDuration(2000000, 7)}};
    f.base.timebase.stopSections = {{makeBeat(2, 1), makeBeat(3, 1), makeDuration(4000000, 9)}};
    f.base.latePolicy.policy = LateEventPolicy::queueNextTick;
    auto& doc = f.request.assembly.sources[0].document;
    auto& r = doc.requirements[0];
    r.requiredActions.push_back(RequiredActionRef::fromToken("action.secondary").value());
    r.measure.components = {{PhaseKind::head, "hold_head", {"perfect", "good"}},
        {PhaseKind::body, "hold_body", {"steady", "rough"}},
        {PhaseKind::tail, "hold_tail", {"clean", "late"}}};
    doc.judgementDomains[0].axes.push_back({"axis.vertical", -1000, 1000});
    const auto prepared = f.prepare();
    const auto decoded = capsule::decode(f.encode(prepared), f.context());
    REQUIRE(decoded);
    CHECK(semanticDiff(prepared.assembled().graph, decoded->gameplay.assembled().graph).empty());
    CHECK(decoded->gameplay.assembled().prepared == prepared.assembled().prepared);
    CHECK(decoded->gameplay.assembled().graph.timebase->stopSections == f.base.timebase.stopSections);
}

TEST_CASE("Capsule named and unused definitions survive canonical dictionary reuse",
          "[capsule][roundtrip][definitions]") {
    CapsuleFixture f;
    auto& doc = f.request.assembly.sources[0].document;
    auto& r = doc.requirements[0];
    r.pattern.patternId = "pattern.named";
    auto pattern = r.pattern;
    auto unused = pattern;
    unused.patternId = "pattern.unused";
    unused.root = {PatternPrimitive::skip, {}, {}, {}, {}};
    doc.declarations.push_back(testDeclaration(DeclarationKind::patternDefinition, pattern.patternId, 6));
    doc.declarations.push_back(testDeclaration(DeclarationKind::measureDefinition, "measure.named", 7));
    doc.declarations.push_back(testDeclaration(DeclarationKind::patternDefinition, unused.patternId, 8));
    const auto d = std::find_if(doc.declarations.begin(), doc.declarations.end(),
        [](const auto& v) { return v.kind == DeclarationKind::requirement; });
    d->references = {{ReferenceScope::sameDocument, "", 6}, {ReferenceScope::sameDocument, "", 7}};
    const std::vector patterns{pattern, unused, unused};
    const std::vector measures{capsule::MeasureDefinition{std::string{"measure.named"}, r.measure}};
    const auto prepared = f.prepare();
    const auto bytes = capsule::encode({f.chart, prepared, f.owners, f.profiles, patterns, measures});
    REQUIRE(bytes);
    const auto decoded = capsule::decode(*bytes, f.context());
    REQUIRE(decoded);
    CHECK(decoded->patterns.size() == 2);
    CHECK(decoded->measures.size() == 1);
    CHECK(decoded->measures[0].id == std::optional<std::string>{"measure.named"});
    CHECK(capsule::encode({decoded->chart, decoded->gameplay, decoded->owners, decoded->profiles,
                          decoded->patterns, decoded->measures}).value() == *bytes);
}

TEST_CASE("Capsule file and memory bridge replace only validated artifacts",
          "[capsule][file][atomic]") {
    CapsuleFixture f;
    const auto prepared = f.prepare();
    const auto bytes = f.encode(prepared);
    Workspace workspace;
    const auto target = workspace.path / "chart.packed";
    REQUIRE(capsule::writeAtomic({f.chart, prepared, f.owners, f.profiles}, target));
    CHECK(readBytes(target) == bytes);
    const auto file = capsule::read(target, f.context());
    REQUIRE(file);
    CHECK(file->gameplay.assembled().prepared == prepared.assembled().prepared);
    auto invalid = f.chart;
    invalid.features.clear();
    CHECK_FALSE(capsule::writeAtomic({invalid, prepared, f.owners, f.profiles}, target));
    CHECK(readBytes(target) == bytes);
    auto limits = chart::PackedChartLimits{};
    limits.maxPackedFileBytes = 1;
    CHECK_FALSE(capsule::writeAtomic({f.chart, prepared, f.owners, f.profiles}, target, limits));
    CHECK(readBytes(target) == bytes);
    const auto occupied = workspace.path / "occupied";
    REQUIRE(std::filesystem::create_directory(occupied));
    CHECK_FALSE(capsule::writeAtomic({f.chart, prepared, f.owners, f.profiles}, occupied));
    CHECK(std::filesystem::is_directory(occupied));
    CHECK(std::distance(std::filesystem::directory_iterator{workspace.path},
                        std::filesystem::directory_iterator{}) == 2);
}

TEST_CASE("Capsule unused definitions cannot bypass the declared closure",
          "[capsule][definitions][closure][hostile]") {
    CapsuleFixture f;
    auto pattern = f.request.assembly.sources[0].document.requirements[0].pattern;
    pattern.root = {PatternPrimitive::skip, {}, {}, {}, {}};
    capsule::MeasureDefinition measure{std::nullopt, {}};
    std::vector<PatternDeclaration> patterns;
    std::vector<capsule::MeasureDefinition> measures;
    SECTION("unused Pattern feature") {
        pattern.required.features.push_back({"feature.undeclared"});
        patterns.push_back(pattern);
    }
    SECTION("unused Pattern capability") {
        pattern.required.capabilities.push_back({"capability.undeclared", "revision.one"});
        patterns.push_back(pattern);
    }
    SECTION("unused Measure feature") {
        measure.declaration.required.features.push_back({"feature.undeclared"});
        measures.push_back(measure);
    }
    SECTION("unused Measure capability") {
        measure.declaration.required.capabilities.push_back({"capability.undeclared", ""});
        measures.push_back(measure);
    }
    const auto prepared = f.prepare();
    CHECK_FALSE(capsule::encode({f.chart, prepared, f.owners, f.profiles, patterns, measures}));
}

TEST_CASE("Capsule canonical byte and structural preimage goldens",
          "[capsule][golden]") {
    CapsuleFixture f;
    const auto prepared = f.prepare();
    const auto bytes = f.encode(prepared);
    const auto preimage = capsule::semanticPreimage({f.chart, prepared, f.owners, f.profiles});
    REQUIRE(preimage);
    CHECK(bytes.size() == 3264);
    CHECK(digestText(bytes) == "471e5cefc7cd2b1376163ad87cf1b1d7557f624252fe2cef707b7af1a9774e7c");
    CHECK(preimage->size() == 4528);
    CHECK(digestText(*preimage) == "82765a403f7fe3c0b820aae1e3ca699793fe206223278d7a2c33606121b7c9ff");
    const auto digest = core::detail::sha256(*preimage);
    CHECK(std::equal(digest.begin(), digest.end(), bytes.begin() + 32,
        [](auto l, auto r) { return l == std::to_integer<std::uint8_t>(r); }));
}

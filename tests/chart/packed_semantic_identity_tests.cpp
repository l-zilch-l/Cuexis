// R1 contract tests for the Foundation Packed semantic identity (Spec 10.1/10.2/10.3).
//
// The golden digests below are copied from docs/formats/PACKED_CHART_FORMAT.md 10.2, so these
// cases validate this implementation against an independent, frozen source instead of comparing
// two values produced by the same code.

#include <cuexis/chart/packed_chart_io.hpp>
#include <cuexis/chart/packed_chart_tables.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace {

using cuexis::chart::CanonicalEntity;
using cuexis::chart::CanonicalFeature;
using cuexis::chart::CanonicalRequirement;
using cuexis::chart::CanonicalSemanticChart;
using cuexis::chart::ChartId;
using cuexis::chart::ChartObjectId;
using cuexis::chart::ExplicitEntityIdentity;
using cuexis::chart::GeneratedEntityIdentity;
using cuexis::chart::LaneConstraint;
using cuexis::chart::RationalBeat;
using cuexis::chart::SemanticIdentityStep;

// Spec 10.2 golden inputs.
constexpr std::string_view goldenChartId{"019b0000-0000-7abc-8def-000000000001"};
constexpr std::string_view goldenEntityId{"019b0000-0000-7abc-8def-000000000010"};
constexpr std::string_view goldenFeatureId{"cuexis.gameplay.candidate.lanes4"};
constexpr std::string_view goldenEmptyDigest{
    "9372e8f76da7234fd6f3d31c835bc24a98b4c65c53bf4e9ca37fbe672507d178"};
constexpr std::string_view goldenOneTapDigest{
    "9b1714dc3087fddd2a73ad2d947c1f9854e17f5964135085a232edb31003a091"};
constexpr std::size_t goldenEmptyBytes{165U};
constexpr std::size_t goldenOneTapBytes{312U};

auto emptyChart() -> CanonicalSemanticChart {
    CanonicalSemanticChart chart;
    chart.chartId = ChartId{std::string{goldenChartId}};
    chart.defaultCamera = cuexis::chart::CameraData{};
    return chart;
}

auto oneTapChart() -> CanonicalSemanticChart {
    auto chart = emptyChart();
    chart.features.push_back(CanonicalFeature{std::string{goldenFeatureId}, 1});
    CanonicalRequirement requirement;
    requirement.localId = "hit";
    requirement.judgementDomain = {"judgement-domain", "candidate.lanes4"};
    requirement.requiredAction = {"action", "press"};
    requirement.constraints.emplace_back(LaneConstraint{2});
    CanonicalEntity entity;
    entity.identity = ExplicitEntityIdentity{ChartObjectId{std::string{goldenEntityId}}};
    entity.requirements.push_back(std::move(requirement));
    chart.entities.push_back(std::move(entity));
    return chart;
}

[[nodiscard]] auto digestHex(const CanonicalSemanticChart& chart) -> std::string {
    const auto identity = cuexis::chart::packed::semanticIdentity(chart);
    REQUIRE(identity);
    return cuexis::chart::packed::semanticIdentityHex(*identity);
}

[[nodiscard]] auto readU32(std::span<const std::byte> bytes, std::size_t offset) -> std::uint32_t {
    std::uint32_t value = 0;
    for (std::size_t index = 0; index < 4U; ++index) {
        value |= static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + index]))
                 << (8U * index);
    }
    return value;
}

void writeU32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value) {
    for (std::size_t index = 0; index < 4U; ++index) {
        bytes[offset + index] = static_cast<std::byte>((value >> (8U * index)) & 0xffU);
    }
}

// Directory entry index for a section code, or -1 when it is absent.
[[nodiscard]] auto findSectionIndex(std::span<const std::byte> bytes, std::string_view code)
    -> std::size_t {
    const auto count = readU32(bytes, 24U);
    for (std::uint32_t index = 0; index < count; ++index) {
        const auto base = 96U + 32U * static_cast<std::size_t>(index);
        const std::string_view name{reinterpret_cast<const char*>(bytes.data() + base), 4U};
        if (name == code) {
            return index;
        }
    }
    return static_cast<std::size_t>(-1);
}

[[nodiscard]] auto sectionOffset(std::span<const std::byte> bytes, std::size_t index)
    -> std::size_t {
    return readU32(bytes, 96U + 32U * index + 8U);
}

[[nodiscard]] auto sectionSize(std::span<const std::byte> bytes, std::size_t index) -> std::size_t {
    return readU32(bytes, 96U + 32U * index + 12U);
}

// Recomputes both the directory CRC of one section and the header CRC, so a byte-level edit
// keeps the artifact structurally valid and isolates the semantic identity check.
void refreshCrcs(std::vector<std::byte>& bytes, std::size_t sectionIndex) {
    const auto base = 96U + 32U * sectionIndex;
    const auto offset = sectionOffset(bytes, sectionIndex);
    const auto size = sectionSize(bytes, sectionIndex);
    writeU32(bytes, base + 24U,
             cuexis::chart::packed::crc32(std::span<const std::byte>{bytes.data() + offset, size}));
    writeU32(bytes, 92U,
             cuexis::chart::packed::crc32(std::span<const std::byte>{bytes.data(), 92U}));
}

} // namespace

TEST_CASE("R1 semantic preimage matches the frozen empty golden", "[chart][packed][identity][r1]") {
    const auto preimage = cuexis::chart::packed::semanticPreimage(emptyChart());
    REQUIRE(preimage);
    CHECK(preimage->size() == goldenEmptyBytes);
    CHECK(digestHex(emptyChart()) == goldenEmptyDigest);
}

TEST_CASE("R1 semantic preimage matches the frozen one-tap-lane2 golden",
          "[chart][packed][identity][r1]") {
    const auto preimage = cuexis::chart::packed::semanticPreimage(oneTapChart());
    REQUIRE(preimage);
    CHECK(preimage->size() == goldenOneTapBytes);
    CHECK(digestHex(oneTapChart()) == goldenOneTapDigest);
}

TEST_CASE("R1 writer publishes the computed semantic identity in the header",
          "[chart][packed][identity][r1]") {
    const auto encoded = cuexis::chart::packed::encode(oneTapChart());
    REQUIRE(encoded);
    auto header = std::array<std::uint8_t, 32>{};
    for (std::size_t index = 0; index < header.size(); ++index) {
        header[index] = std::to_integer<std::uint8_t>((*encoded)[32U + index]);
    }
    CHECK(cuexis::chart::packed::semanticIdentityHex(header) == goldenOneTapDigest);
    CHECK(
        std::any_of(header.begin(), header.end(), [](std::uint8_t value) { return value != 0U; }));

    // decode verifies the same digest and therefore publishes the identical semantic chart.
    const auto decoded = cuexis::chart::PackedChartReader::decode(*encoded);
    REQUIRE(decoded);
    CHECK(digestHex(*decoded) == goldenOneTapDigest);
}

TEST_CASE("R1 reader rejects semantic bytes that no longer match the header digest",
          "[chart][packed][identity][r1]") {
    const auto encoded = cuexis::chart::packed::encode(oneTapChart());
    REQUIRE(encoded);
    auto tampered = *encoded;
    const auto metaIndex = findSectionIndex(tampered, "META");
    REQUIRE(metaIndex != static_cast<std::size_t>(-1));
    // Flip the last hex digit of the META chartId UUID (semantic step 2 of Spec 10.1).
    const auto chartIdOffset = sectionOffset(tampered, metaIndex) + 15U;
    tampered[chartIdOffset] = std::byte{
        static_cast<unsigned char>(std::to_integer<std::uint8_t>(tampered[chartIdOffset]) ^ 0x01U)};
    refreshCrcs(tampered, metaIndex);

    // The artifact is still structurally valid and the section/header CRCs agree, so only the
    // semantic identity comparison can reject it.
    CHECK(cuexis::chart::packed::inspect(tampered));
    const auto decoded = cuexis::chart::PackedChartReader::decode(tampered);
    REQUIRE_FALSE(decoded);
    CHECK(decoded.error().code() == "packed.identity.mismatch");
}

TEST_CASE("R1 reader rejects a rewritten header digest even after a header CRC refresh",
          "[chart][packed][identity][r1]") {
    const auto encoded = cuexis::chart::packed::encode(oneTapChart());
    REQUIRE(encoded);
    auto tampered = *encoded;
    std::fill(tampered.begin() + 32, tampered.begin() + 64, std::byte{0});
    writeU32(tampered, 92U,
             cuexis::chart::packed::crc32(std::span<const std::byte>{tampered.data(), 92U}));

    CHECK(cuexis::chart::packed::inspect(tampered));
    const auto decoded = cuexis::chart::packed::decode(tampered);
    REQUIRE_FALSE(decoded);
    CHECK(decoded.error().code() == "packed.identity.mismatch");
}

TEST_CASE("R1 semantic identity ignores input ordering that the format does not order",
          "[chart][packed][identity][r1]") {
    auto ordered = oneTapChart();
    CanonicalEntity second;
    second.identity = ExplicitEntityIdentity{ChartObjectId{"019b0000-0000-7abc-8def-000000000002"}};
    ordered.entities.push_back(second);
    auto reversed = ordered;
    std::reverse(reversed.entities.begin(), reversed.entities.end());

    const auto first = cuexis::chart::packed::semanticIdentity(ordered);
    const auto secondDigest = cuexis::chart::packed::semanticIdentity(reversed);
    REQUIRE(first);
    REQUIRE(secondDigest);
    CHECK(*first == *secondDigest);

    // Canonical output does not depend on the input order either.
    const auto firstBytes = cuexis::chart::packed::encode(ordered);
    const auto secondBytes = cuexis::chart::packed::encode(reversed);
    REQUIRE(firstBytes);
    REQUIRE(secondBytes);
    CHECK(*firstBytes == *secondBytes);
}

TEST_CASE("R1 semantic identity changes when semantic content changes",
          "[chart][packed][identity][r1]") {
    const auto baseline = digestHex(oneTapChart());

    SECTION("lane constraint") {
        auto chart = oneTapChart();
        chart.entities.front().requirements.front().constraints.front() = LaneConstraint{3};
        CHECK(digestHex(chart) != baseline);
    }
    SECTION("requirement beat") {
        auto chart = oneTapChart();
        chart.entities.front().requirements.front().interval.startBeat =
            *RationalBeat::create(1, 4);
        CHECK(digestHex(chart) != baseline);
    }
    SECTION("feature version") {
        auto chart = oneTapChart();
        chart.features.front().version = 2;
        CHECK(digestHex(chart) != baseline);
    }
    SECTION("entity identity") {
        auto chart = oneTapChart();
        chart.entities.front().identity =
            ExplicitEntityIdentity{ChartObjectId{"019b0000-0000-7abc-8def-000000000011"}};
        CHECK(digestHex(chart) != baseline);
    }
    SECTION("default camera position") {
        auto chart = oneTapChart();
        chart.defaultCamera.defaultTransform = cuexis::chart::TransformData{};
        chart.defaultCamera.defaultTransform->position.x = 1.0F;
        CHECK(digestHex(chart) != baseline);
    }
    SECTION("main music") {
        auto chart = oneTapChart();
        chart.mainMusic = cuexis::chart::AssetId{"audio.main"};
        // R4/D9: the declared closure must be the closure derived from the asset references.
        chart.resourceClosure.resources.push_back(cuexis::chart::CanonicalResourceUse{
            *chart.mainMusic, cuexis::chart::CanonicalResourceUseKind::MainMusic});
        CHECK(digestHex(chart) != baseline);
    }
    SECTION("requirement action") {
        auto chart = oneTapChart();
        chart.entities.front().requirements.front().requiredAction = {"action", "release"};
        CHECK(digestHex(chart) != baseline);
    }
}

TEST_CASE("R1 hash preconditions fail with stable errors instead of throwing",
          "[chart][packed][identity][r1]") {
    SECTION("duplicate entity identity") {
        auto chart = oneTapChart();
        chart.entities.push_back(chart.entities.front());
        const auto identity = cuexis::chart::packed::semanticIdentity(chart);
        REQUIRE_FALSE(identity);
        CHECK(identity.error().code() == "packed.identity.duplicate_identity");
        CHECK_FALSE(cuexis::chart::packed::encode(chart));
    }
    SECTION("dangling parent") {
        auto chart = oneTapChart();
        chart.entities.front().parent =
            ExplicitEntityIdentity{ChartObjectId{"019b0000-0000-7abc-8def-0000000000ff"}};
        const auto identity = cuexis::chart::packed::semanticIdentity(chart);
        REQUIRE_FALSE(identity);
        CHECK(identity.error().code() == "packed.identity.parent_missing");
        CHECK_FALSE(cuexis::chart::packed::encode(chart));
    }
    SECTION("parent cycle") {
        auto chart = oneTapChart();
        CanonicalEntity second;
        second.identity =
            ExplicitEntityIdentity{ChartObjectId{"019b0000-0000-7abc-8def-000000000002"}};
        chart.entities.front().parent = second.identity;
        second.parent = chart.entities.front().identity;
        chart.entities.push_back(std::move(second));
        const auto identity = cuexis::chart::packed::semanticIdentity(chart);
        REQUIRE_FALSE(identity);
        CHECK(identity.error().code() == "packed.identity.parent_cycle");
        CHECK_FALSE(cuexis::chart::packed::encode(chart));
    }
    SECTION("duplicate component of the same kind") {
        auto chart = oneTapChart();
        chart.entities.front().components.emplace_back(cuexis::chart::CanonicalTransform{});
        chart.entities.front().components.emplace_back(cuexis::chart::CanonicalTransform{});
        const auto identity = cuexis::chart::packed::semanticIdentity(chart);
        REQUIRE_FALSE(identity);
        CHECK(identity.error().code() == "packed.identity.component_duplicate");
        CHECK_FALSE(cuexis::chart::packed::encode(chart));
    }
    SECTION("duplicate feature id") {
        auto chart = oneTapChart();
        chart.features.push_back(CanonicalFeature{std::string{goldenFeatureId}, 2});
        const auto identity = cuexis::chart::packed::semanticIdentity(chart);
        REQUIRE_FALSE(identity);
        CHECK(identity.error().code() == "packed.identity.feature_duplicate");
    }
    SECTION("duplicate requirement localId") {
        auto chart = oneTapChart();
        chart.entities.front().requirements.push_back(chart.entities.front().requirements.front());
        const auto identity = cuexis::chart::packed::semanticIdentity(chart);
        REQUIRE_FALSE(identity);
        CHECK(identity.error().code() == "packed.identity.requirement_duplicate");
    }
    SECTION("missing lane constraint") {
        auto chart = oneTapChart();
        chart.entities.front().requirements.front().constraints.clear();
        const auto identity = cuexis::chart::packed::semanticIdentity(chart);
        REQUIRE_FALSE(identity);
        // R2 unified the profile diagnostics: the preimage half of a Spec 7.6 rule reports the
        // same packed.profile.* code as packed::encode and packed::decode.
        CHECK(identity.error().code() == "packed.profile.constraints");
    }
    SECTION("non-empty effect set") {
        auto chart = oneTapChart();
        chart.entities.front().requirements.front().effects.push_back(
            cuexis::chart::TypedReference{"effect", "flash"});
        const auto identity = cuexis::chart::packed::semanticIdentity(chart);
        REQUIRE_FALSE(identity);
        CHECK(identity.error().code() == "packed.profile.effects");
    }
    SECTION("non-finite float") {
        auto chart = oneTapChart();
        chart.defaultCamera.fovY = std::numeric_limits<double>::infinity();
        const auto identity = cuexis::chart::packed::semanticIdentity(chart);
        REQUIRE_FALSE(identity);
        CHECK(identity.error().code() == "packed.identity.non_finite");
    }
    SECTION("non-canonical chart id") {
        auto chart = oneTapChart();
        chart.chartId = ChartId{"not-a-uuid"};
        const auto identity = cuexis::chart::packed::semanticIdentity(chart);
        REQUIRE_FALSE(identity);
        CHECK(identity.error().code() == "packed.identity.invalid_uuid");
    }
    SECTION("generated identity with a foreign scope chartId") {
        auto chart = oneTapChart();
        GeneratedEntityIdentity generated;
        generated.chartId = ChartId{"019b0000-0000-7abc-8def-0000000000aa"};
        generated.bindingId = "binding0";
        generated.moduleId = "module0";
        generated.exportId = "emit";
        generated.path.push_back(SemanticIdentityStep{"step", 0});
        CanonicalEntity entity;
        entity.identity = std::move(generated);
        chart.entities.push_back(std::move(entity));
        const auto identity = cuexis::chart::packed::semanticIdentity(chart);
        REQUIRE_FALSE(identity);
        CHECK(identity.error().code() == "packed.identity.scope_chart");
    }
    SECTION("generated identity whose last step is indexed") {
        auto chart = oneTapChart();
        GeneratedEntityIdentity generated;
        generated.chartId = chart.chartId;
        generated.bindingId = "binding0";
        generated.moduleId = "module0";
        generated.exportId = "emit";
        generated.path.push_back(SemanticIdentityStep{"step", 2});
        CanonicalEntity entity;
        entity.identity = std::move(generated);
        chart.entities.push_back(std::move(entity));
        const auto identity = cuexis::chart::packed::semanticIdentity(chart);
        REQUIRE_FALSE(identity);
        CHECK(identity.error().code() == "packed.identity.generated_path");
    }
    SECTION("-0 is canonicalized instead of rejected") {
        auto chart = oneTapChart();
        chart.defaultCamera.pitch = -0.0;
        CHECK(digestHex(chart) == digestHex(oneTapChart()));
    }
}

TEST_CASE("R1 generated identities round trip through the Spec 6.5 scope and path tables",
          "[chart][packed][identity][r1]") {
    auto chart = oneTapChart();
    GeneratedEntityIdentity first;
    first.chartId = chart.chartId;
    first.bindingId = "binding0";
    first.moduleId = "module0";
    first.exportId = "emit";
    first.path.push_back(SemanticIdentityStep{"group", 2});
    first.path.push_back(SemanticIdentityStep{"emit", 0});

    // Same scope and same path shape, different repeat index: the two identities share one
    // scope record and one path record and differ only in the per-identity iteration index.
    GeneratedEntityIdentity second;
    second.chartId = chart.chartId;
    second.bindingId = "binding0";
    second.moduleId = "module0";
    second.exportId = "emit";
    second.path.push_back(SemanticIdentityStep{"group", 3});
    second.path.push_back(SemanticIdentityStep{"emit", 0});

    CanonicalEntity firstEntity;
    firstEntity.identity = std::move(first);
    chart.entities.push_back(std::move(firstEntity));

    CanonicalEntity secondEntity;
    secondEntity.identity = std::move(second);
    secondEntity.parent = chart.entities.front().identity;
    chart.entities.push_back(std::move(secondEntity));

    const auto encoded = cuexis::chart::packed::encode(chart);
    REQUIRE(encoded);
    const auto identityIndex = findSectionIndex(*encoded, "IDN0");
    REQUIRE(identityIndex != static_cast<std::size_t>(-1));
    const auto payload = sectionOffset(*encoded, identityIndex);
    // Spec 6.5 starts IDN0 with u32 scopeCount; the pre-hardening writer always emitted zero.
    CHECK(readU32(*encoded, payload) == 1U);

    const auto decoded = cuexis::chart::packed::decode(*encoded);
    REQUIRE(decoded);
    REQUIRE(decoded->entities.size() == 3U);
    const auto& restored = std::get<GeneratedEntityIdentity>(decoded->entities[1].identity);
    CHECK(restored.chartId.value == chart.chartId.value);
    CHECK(restored.bindingId == "binding0");
    CHECK(restored.moduleId == "module0");
    CHECK(restored.exportId == "emit");
    REQUIRE(restored.path.size() == 2U);
    CHECK(restored.path[0].nodeId == "group");
    CHECK(restored.path[0].iterationIndexPlusOne == 2U);
    CHECK(restored.path[1].nodeId == "emit");
    CHECK(restored.path[1].iterationIndexPlusOne == 0U);
    CHECK(std::get<GeneratedEntityIdentity>(decoded->entities[2].identity)
              .path.front()
              .iterationIndexPlusOne == 3U);

    // The parent indirection still resolves to the explicit entity through the new layout.
    REQUIRE(decoded->entities[2].parent);
    CHECK(std::holds_alternative<ExplicitEntityIdentity>(*decoded->entities[2].parent));

    // Canonical determinism and identity stability across the decode.
    const auto reencoded = cuexis::chart::packed::encode(*decoded);
    REQUIRE(reencoded);
    CHECK(*reencoded == *encoded);
    CHECK(digestHex(*decoded) == digestHex(chart));
}

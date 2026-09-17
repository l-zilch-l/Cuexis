// R4 end-to-end tests: encode -> decode -> full semantic comparison -> re-encode.
//
// The plan rejects count-only and size()-only evidence, so every case here compares the complete
// typed model field by field (chart identity, dictionary-visible features, timing, default camera,
// resource closure and every entity value) and then proves the artifact is canonical by
// re-encoding it byte for byte.
//
// The fixtures are small and explainable on purpose: they supplement the 40,000 entity capacity
// profile, they do not replace it and they do not claim that an arbitrary chart fits 16 MiB.

#include "packed_fixture_support.hpp"

#include <cuexis/chart/cxt_v2_loader.hpp>
#include <cuexis/chart/packed_chart_io.hpp>
#include <cuexis/chart/packed_chart_tables.hpp>
#include <cuexis/core/result.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace {

using cuexis::chart::AssetId;
using cuexis::chart::CameraData;
using cuexis::chart::CanonicalCamera;
using cuexis::chart::CanonicalEntity;
using cuexis::chart::CanonicalFeature;
using cuexis::chart::CanonicalRenderable;
using cuexis::chart::CanonicalRequirement;
using cuexis::chart::CanonicalResourceUse;
using cuexis::chart::CanonicalResourceUseKind;
using cuexis::chart::CanonicalSemanticChart;
using cuexis::chart::CanonicalTransform;
using cuexis::chart::ChartId;
using cuexis::chart::ChartObjectId;
using cuexis::chart::ChartTiming;
using cuexis::chart::CxtV2Invocation;
using cuexis::chart::CxtV2Loader;
using cuexis::chart::ExplicitEntityIdentity;
using cuexis::chart::LaneConstraint;
using cuexis::chart::RationalBeat;
using cuexis::chart::TempoEvent;
using cuexis::chart::TimingStop;
using cuexis::chart::TransformData;
using cuexis::chart::TypedReference;
using cuexis::chart::packed::test::findSection;
using cuexis::chart::packed::test::readU32;
using cuexis::chart::packed::test::readU8;
using cuexis::chart::packed::test::replaceSection;

constexpr std::string_view registeredFeature{"cuexis.gameplay.candidate.lanes4"};
constexpr std::string_view chartUuid{"019b0000-0000-7abc-8def-000000000001"};

[[nodiscard]] auto beat(std::int64_t numerator, std::int64_t denominator) -> RationalBeat {
    const auto value = RationalBeat::create(numerator, denominator);
    REQUIRE(value);
    return *value;
}

// ---------------------------------------------------------------------------------------------
// Complete semantic comparison of the candidate typed model. ChartTiming, CameraData,
// TransformData, TempoEvent and TimingStop have no comparison operator, so every field is
// compared explicitly instead of relying on a defaulted operator.
// ---------------------------------------------------------------------------------------------

[[nodiscard]] auto sameCamera(const CameraData& left, const CameraData& right) -> bool {
    if (left.type != right.type || left.fovY != right.fovY || left.nearPlane != right.nearPlane ||
        left.farPlane != right.farPlane || left.pitch != right.pitch || left.yaw != right.yaw ||
        left.roll != right.roll) {
        return false;
    }
    if (left.defaultTransform.has_value() != right.defaultTransform.has_value()) {
        return false;
    }
    if (left.defaultTransform) {
        const auto& a = *left.defaultTransform;
        const auto& b = *right.defaultTransform;
        return a.position == b.position && a.rotation == b.rotation && a.scale == b.scale;
    }
    return true;
}

[[nodiscard]] auto sameTiming(const ChartTiming& left, const ChartTiming& right) -> bool {
    if (left.offsetMs != right.offsetMs || left.defaultBpm != right.defaultBpm ||
        left.tempoEvents.size() != right.tempoEvents.size() ||
        left.stops.size() != right.stops.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.tempoEvents.size(); ++index) {
        const auto& a = left.tempoEvents[index];
        const auto& b = right.tempoEvents[index];
        if (a.startBeat != b.startBeat || a.durationBeats != b.durationBeats ||
            a.startBpm != b.startBpm || a.endBpm != b.endBpm || a.startSlope != b.startSlope ||
            a.endSlope != b.endSlope) {
            return false;
        }
    }
    for (std::size_t index = 0; index < left.stops.size(); ++index) {
        if (left.stops[index].beat != right.stops[index].beat ||
            left.stops[index].durationMs != right.stops[index].durationMs) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] auto sameSemantics(const CanonicalSemanticChart& left,
                                 const CanonicalSemanticChart& right) -> bool {
    if (left.chartId.value != right.chartId.value) {
        return false;
    }
    if (left.mainMusic.has_value() != right.mainMusic.has_value()) {
        return false;
    }
    if (left.mainMusic && left.mainMusic->value != right.mainMusic->value) {
        return false;
    }
    if (!(left.features == right.features) || !sameTiming(left.timing, right.timing) ||
        !sameCamera(left.defaultCamera, right.defaultCamera) ||
        !(left.resourceClosure == right.resourceClosure)) {
        return false;
    }
    // CanonicalEntity, CanonicalRequirement, the component variants and the identity variants all
    // have defaulted comparison operators.
    return left.entities == right.entities;
}

// ---------------------------------------------------------------------------------------------
// Fixtures
// ---------------------------------------------------------------------------------------------

[[nodiscard]] auto uuidText(std::string_view lastTwelve) -> std::string {
    return std::string{chartUuid}.substr(0, 24) + std::string{lastTwelve};
}

// Builds a canonical explicit object UUID: the fixture chart prefix plus a 12-hex-digit suffix.
// Short suffixes are zero-padded, so fixtures can use readable identifiers such as "0010".
[[nodiscard]] auto explicitEntity(std::string_view suffix) -> CanonicalEntity {
    std::string twelve(12, '0');
    const auto length = std::min<std::size_t>(suffix.size(), twelve.size());
    twelve.replace(twelve.size() - length, length, suffix.substr(suffix.size() - length));
    CanonicalEntity entity;
    entity.identity = ExplicitEntityIdentity{ChartObjectId{uuidText(twelve)}};
    return entity;
}

[[nodiscard]] auto tapRequirement(std::uint32_t lane, RationalBeat startBeat)
    -> CanonicalRequirement {
    CanonicalRequirement requirement;
    requirement.localId = "hit";
    requirement.kind = cuexis::chart::CanonicalRequirementKind::Tap;
    requirement.interval.kind = cuexis::chart::CanonicalIntervalKind::Point;
    requirement.interval.startBeat = startBeat;
    requirement.judgementDomain = TypedReference{"judgement-domain", "candidate.lanes4"};
    requirement.requiredAction = TypedReference{"action", "press"};
    requirement.constraints.emplace_back(LaneConstraint{lane});
    return requirement;
}

[[nodiscard]] auto baseChart() -> CanonicalSemanticChart {
    CanonicalSemanticChart chart;
    chart.chartId = ChartId{std::string{chartUuid}};
    chart.defaultCamera = CameraData{};
    chart.features.push_back(CanonicalFeature{std::string{registeredFeature}, 1});
    return chart;
}

[[nodiscard]] auto tapChart() -> CanonicalSemanticChart {
    auto chart = baseChart();
    auto entity = explicitEntity("0010");
    entity.requirements.push_back(tapRequirement(2U, RationalBeat::zero()));
    chart.entities.push_back(std::move(entity));
    return chart;
}

// Three entities sharing the Transform+Renderable+Camera mask. Entity A and entity C carry the
// same complete values, entity B differs in every defaultable component, so the archetype default
// is A and B must be described entirely by its delta rows.
[[nodiscard]] auto componentChart() -> CanonicalSemanticChart {
    auto chart = baseChart();
    const auto makeTransform = [](float x, float y, float z) {
        CanonicalTransform transform;
        transform.position = {x, y, z};
        return transform;
    };
    const auto makeRenderable = [](std::string mesh, std::string material, std::uint8_t alpha) {
        return CanonicalRenderable{AssetId{std::move(mesh)}, AssetId{std::move(material)}, alpha};
    };

    auto first = explicitEntity("0010");
    first.components.emplace_back(makeTransform(1.0F, 2.0F, 3.0F));
    first.components.emplace_back(makeRenderable("mesh.note", "material.note", 255U));
    first.components.emplace_back(CanonicalCamera{"perspective", 60.0, 0.1, 1000.0});

    auto second = explicitEntity("0020");
    second.components.emplace_back(makeTransform(-4.5F, 0.0F, 8.25F));
    second.components.emplace_back(makeRenderable("mesh.other", "material.other", 128U));
    second.components.emplace_back(CanonicalCamera{"perspective", 45.5, 0.25, 250.75});

    auto third = explicitEntity("0030");
    third.components.emplace_back(makeTransform(1.0F, 2.0F, 3.0F));
    third.components.emplace_back(makeRenderable("mesh.note", "material.note", 255U));
    third.components.emplace_back(CanonicalCamera{"perspective", 60.0, 0.1, 1000.0});

    chart.entities.push_back(std::move(first));
    chart.entities.push_back(std::move(second));
    chart.entities.push_back(std::move(third));
    // R4/D9: the declared closure is the closure derived from these asset references.
    chart.resourceClosure.resources.push_back(CanonicalResourceUse{
        AssetId{"material.note"}, CanonicalResourceUseKind::RenderableMaterial});
    chart.resourceClosure.resources.push_back(CanonicalResourceUse{
        AssetId{"material.other"}, CanonicalResourceUseKind::RenderableMaterial});
    chart.resourceClosure.resources.push_back(
        CanonicalResourceUse{AssetId{"mesh.note"}, CanonicalResourceUseKind::RenderableMesh});
    chart.resourceClosure.resources.push_back(
        CanonicalResourceUse{AssetId{"mesh.other"}, CanonicalResourceUseKind::RenderableMesh});
    std::sort(chart.resourceClosure.resources.begin(), chart.resourceClosure.resources.end());
    return chart;
}

// Two entities with the same Transform mask and the given positions.
[[nodiscard]] auto transformChart(std::vector<float> positionsX) -> CanonicalSemanticChart {
    auto chart = baseChart();
    std::uint32_t ordinal = 0x10U;
    for (const auto x : positionsX) {
        auto entity = explicitEntity("0000000000" + std::to_string(ordinal));
        CanonicalTransform transform;
        transform.position = {x, 0.0F, 0.0F};
        entity.components.emplace_back(transform);
        chart.entities.push_back(std::move(entity));
        ordinal += 0x10U;
    }
    return chart;
}

[[nodiscard]] auto encodedBytes(const CanonicalSemanticChart& chart) -> std::vector<std::byte> {
    const auto encoded = cuexis::chart::packed::encode(chart);
    if (!encoded) {
        FAIL("packed::encode failed with " << encoded.error().code());
    }
    return *encoded;
}

[[nodiscard]] auto decodedChart(const std::vector<std::byte>& bytes) -> CanonicalSemanticChart {
    const auto decoded = cuexis::chart::packed::decode(bytes);
    if (!decoded) {
        FAIL("packed::decode failed with " << decoded.error().code());
    }
    return *decoded;
}

[[nodiscard]] auto identityHex(const CanonicalSemanticChart& chart) -> std::string {
    const auto identity = cuexis::chart::packed::semanticIdentity(chart);
    REQUIRE(identity);
    return cuexis::chart::packed::semanticIdentityHex(*identity);
}

[[nodiscard]] auto encodeCode(const CanonicalSemanticChart& chart) -> std::string {
    const auto encoded = cuexis::chart::packed::encode(chart);
    return encoded ? std::string{"accepted"} : std::string{encoded.error().code()};
}

[[nodiscard]] auto sectionPayload(const std::vector<std::byte>& bytes,
                                  const cuexis::chart::packed::test::SectionRef& section)
    -> std::span<const std::byte> {
    return std::span<const std::byte>{bytes.data() + section.offset, section.size};
}

[[nodiscard]] auto readF32(std::span<const std::byte> bytes, std::size_t offset) -> float {
    return std::bit_cast<float>(readU32(bytes, offset));
}

// ---------------------------------------------------------------------------------------------
// CXT ladder input (Spec CXT v2 fixture already used by the expansion tests).
// ---------------------------------------------------------------------------------------------

[[nodiscard]] auto readTextFile(const std::filesystem::path& path) -> std::string {
    std::ifstream input{path, std::ios::binary};
    REQUIRE(input);
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

[[nodiscard]] auto ladderSource(std::string_view name) -> std::string {
    return readTextFile(std::filesystem::path{CUEXIS_SOURCE_DIR} / "tests" / "fixtures" /
                        "chart_format_foundation" / "valid" / name);
}

[[nodiscard]] auto ladderInvocation() -> CxtV2Invocation {
    CxtV2Invocation invocation;
    invocation.chartId = ChartId{std::string{chartUuid}};
    invocation.bindingId = "intro-stair";
    invocation.moduleId = "pattern.stair";
    invocation.exportId = "stair";
    invocation.startBeat = beat(16, 1);
    return invocation;
}

// The CXT v2 loader produces entities only; the caller composes the capability declaration. R4
// records this as a handoff gap (the source format has no feature field yet), so the E2E test
// performs that composition explicitly instead of hiding it.
[[nodiscard]] auto expandedLadder(std::string_view name) -> CanonicalSemanticChart {
    const auto result = CxtV2Loader::expand(ladderSource(name), ladderInvocation());
    REQUIRE(result.chart.has_value());
    CHECK(result.counts.entityCount == result.chart->entities.size());
    CHECK(result.counts.entityCount == 16U);
    auto chart = *result.chart;
    chart.features.push_back(CanonicalFeature{std::string{registeredFeature}, 1});
    return chart;
}

[[nodiscard]] auto lowReuseChart(std::size_t count) -> CanonicalSemanticChart {
    auto chart = baseChart();
    constexpr char digits[] = "0123456789abcdef";
    chart.entities.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        auto value = static_cast<std::uint32_t>(index + 0x100U);
        std::string suffix(12, '0');
        for (std::size_t position = suffix.size(); position > 0; --position) {
            suffix[position - 1] = digits[value & 0x0fU];
            value >>= 4U;
        }
        auto entity = explicitEntity(suffix);
        entity.requirements.push_back(tapRequirement(static_cast<std::uint32_t>(index % 4U),
                                                     beat(static_cast<std::int64_t>(index), 1)));
        chart.entities.push_back(std::move(entity));
    }
    return chart;
}
} // namespace

TEST_CASE("R4 a component chart round-trips encode, decode, semantics and re-encode",
          "[chart][packed][roundtrip][r4]") {
    const auto chart = componentChart();
    const auto artifact = encodedBytes(chart);

    const auto statistics = cuexis::chart::packed::inspect(artifact);
    REQUIRE(statistics);
    CHECK(statistics->entityCount == 3U);
    for (const auto code : {"ARCH", "ENT0", "IDN0", "TRN0", "REN0", "CAM0"}) {
        REQUIRE(findSection(artifact, code).has_value());
    }

    const auto decoded = decodedChart(artifact);
    CHECK(sameSemantics(decoded, chart));
    CHECK(decoded.resourceClosure.resources.size() == 4U);

    // Canonical artifact: re-encoding the decoded model reproduces the same bytes and the same
    // semantic identity.
    const auto reencoded = encodedBytes(decoded);
    CHECK(reencoded == artifact);
    CHECK(identityHex(decoded) == identityHex(chart));
}

TEST_CASE("R4 single-field camera differences survive the CAM0 delta",
          "[chart][packed][roundtrip][r4]") {
    // Two entities share `first` and one carries `second`, so the archetype default is decided by
    // frequency rather than by the byte tie-break and the delta rows are easy to read.
    const auto makeCameraChart = [](const CanonicalCamera& first, const CanonicalCamera& second) {
        auto chart = baseChart();
        for (const auto& [suffix, camera] :
             {std::pair<std::string_view, const CanonicalCamera&>{"0010", first},
              {"0020", second},
              {"0030", first}}) {
            auto entity = explicitEntity(suffix);
            entity.components.emplace_back(camera);
            chart.entities.push_back(std::move(entity));
        }
        return chart;
    };

    SECTION("near plane only") {
        const auto chart = makeCameraChart(CanonicalCamera{"perspective", 60.0, 0.1, 1000.0},
                                           CanonicalCamera{"perspective", 60.0, 0.25, 1000.0});
        const auto artifact = encodedBytes(chart);
        const auto decoded = decodedChart(artifact);
        CHECK(sameSemantics(decoded, chart));
        const auto camera = std::get<CanonicalCamera>(decoded.entities[1].components.front());
        CHECK(camera.nearPlane == 0.25);
        CHECK(camera.farPlane == 1000.0);

        // Spec 7.3: near is field index 2, so the second CAM0 row carries mask bit 2 only.
        const auto cam = findSection(artifact, "CAM0");
        REQUIRE(cam);
        const auto payload = sectionPayload(artifact, *cam);
        CHECK(readU8(payload, 1U) == 0U); // first row: no change
        CHECK(readU8(payload, 3U) == 4U); // second row: near only
    }
    SECTION("far plane only") {
        const auto chart = makeCameraChart(CanonicalCamera{"perspective", 60.0, 0.1, 1000.0},
                                           CanonicalCamera{"perspective", 60.0, 0.1, 250.5});
        const auto decoded = decodedChart(encodedBytes(chart));
        CHECK(sameSemantics(decoded, chart));
        const auto camera = std::get<CanonicalCamera>(decoded.entities[1].components.front());
        CHECK(camera.farPlane == 250.5);
    }
    SECTION("fov only") {
        const auto chart = makeCameraChart(CanonicalCamera{"perspective", 60.0, 0.1, 1000.0},
                                           CanonicalCamera{"perspective", 30.5, 0.1, 1000.0});
        const auto decoded = decodedChart(encodedBytes(chart));
        CHECK(sameSemantics(decoded, chart));
        const auto camera = std::get<CanonicalCamera>(decoded.entities[1].components.front());
        CHECK(camera.fovY == 30.5);
    }
    SECTION("all three fields at once") {
        const auto chart = makeCameraChart(CanonicalCamera{"perspective", 60.0, 0.1, 1000.0},
                                           CanonicalCamera{"perspective", 30.5, 0.25, 250.5});
        const auto artifact = encodedBytes(chart);
        const auto decoded = decodedChart(artifact);
        CHECK(sameSemantics(decoded, chart));
        const auto cam = findSection(artifact, "CAM0");
        REQUIRE(cam);
        CHECK(readU8(sectionPayload(artifact, *cam), 3U) == 0x0eU);
    }
}

TEST_CASE("R4 archetype defaults use the most frequent value and ignore the model order",
          "[chart][packed][roundtrip][r4]") {
    // ARCH row layout: mask u64, defaultsByteCount u32, then Transform position x at +12.
    const auto archetypeX = [](const std::vector<std::byte>& artifact) {
        const auto arch = findSection(artifact, "ARCH");
        REQUIRE(arch);
        return readF32(sectionPayload(artifact, *arch), 12U);
    };

    SECTION("the most frequent value wins over the last value in the model") {
        auto ascending = transformChart({5.0F, 5.0F, 7.0F});
        auto reordered = ascending;
        std::reverse(reordered.entities.begin(), reordered.entities.end());
        CHECK(archetypeX(encodedBytes(ascending)) == 5.0F);
        CHECK(archetypeX(encodedBytes(reordered)) == 5.0F);
        // A last-value-wins rule would make the canonical bytes depend on the model order.
        CHECK(encodedBytes(ascending) == encodedBytes(reordered));
    }
    SECTION("a tie is broken by the ascending canonical field bytes") {
        // 1.0F is 0x3f800000 and -1.0F is 0xbf800000, so the tie-break is byte order rather than
        // numeric order.
        auto first = transformChart({1.0F, -1.0F});
        auto reordered = first;
        std::reverse(reordered.entities.begin(), reordered.entities.end());
        CHECK(archetypeX(encodedBytes(first)) == 1.0F);
        CHECK(archetypeX(encodedBytes(reordered)) == 1.0F);
        CHECK(encodedBytes(first) == encodedBytes(reordered));
    }
    SECTION("both orders decode to the same complete semantics") {
        auto ascending = transformChart({5.0F, 5.0F, 7.0F});
        auto reversed = ascending;
        std::reverse(reversed.entities.begin(), reversed.entities.end());
        const auto decoded = decodedChart(encodedBytes(reversed));
        // The artifact orders entities by canonical identity bytes, so the decoded model matches
        // the canonically ordered source rather than the reversed model order.
        CHECK(sameSemantics(decoded, decodedChart(encodedBytes(ascending))));
        CHECK(identityHex(decoded) == identityHex(reversed));
    }
}

TEST_CASE("R4 TIME rows are canonical ascending and out-of-order rows are refused",
          "[chart][packed][roundtrip][r4]") {
    const auto makeTimingChart = [](bool reversed) {
        auto chart = baseChart();
        auto entity = explicitEntity("0010");
        entity.requirements.push_back(tapRequirement(1U, RationalBeat::zero()));
        chart.entities.push_back(std::move(entity));
        const auto first = TempoEvent{beat(1, 1), beat(1, 1), 120.0, 140.0, 0.0, 0.5};
        const auto second = TempoEvent{beat(2, 1), beat(1, 1), 140.0, 90.0, 0.5, 0.0};
        chart.timing.tempoEvents = reversed ? std::vector<TempoEvent>{second, first}
                                            : std::vector<TempoEvent>{first, second};
        const auto lowStop = TimingStop{beat(1, 1), 50.0};
        const auto highStop = TimingStop{beat(3, 1), 25.0};
        chart.timing.stops = reversed ? std::vector<TimingStop>{highStop, lowStop}
                                      : std::vector<TimingStop>{lowStop, highStop};
        return chart;
    };

    SECTION("the writer sorts both tables and the reader restores them") {
        auto ascending = makeTimingChart(false);
        auto shuffled = makeTimingChart(true);
        CHECK(encodedBytes(ascending) == encodedBytes(shuffled));

        const auto decoded = decodedChart(encodedBytes(shuffled));
        REQUIRE(decoded.timing.tempoEvents.size() == 2U);
        CHECK(decoded.timing.tempoEvents[0].startBeat == beat(1, 1));
        CHECK(decoded.timing.tempoEvents[1].startBeat == beat(2, 1));
        REQUIRE(decoded.timing.stops.size() == 2U);
        CHECK(decoded.timing.stops[0].beat == beat(1, 1));
        CHECK(decoded.timing.stops[1].beat == beat(3, 1));
        CHECK(sameSemantics(decoded, ascending));
    }
    SECTION("a hand-patched descending TIME table is refused before the identity comparison") {
        const auto base = encodedBytes(makeTimingChart(false));
        const auto time = findSection(base, "TIME");
        REQUIRE(time);

        cuexis::chart::packed::ByteWriter payload;
        payload.writeU64(std::bit_cast<std::uint64_t>(0.0));   // offsetMs
        payload.writeU64(std::bit_cast<std::uint64_t>(120.0)); // defaultBpm
        payload.writeU32(2U);                                  // tempoCount
        payload.writeU32(1U);                                  // stopCount
        payload.writeU8(0);
        payload.writeU8(0);
        payload.writeU8(0);
        for (const auto value : {beat(2, 1), beat(1, 1)}) {
            REQUIRE(cuexis::chart::packed::writeRationalBeatAtom(payload, value));
            REQUIRE(cuexis::chart::packed::writeRationalBeatAtom(payload, beat(1, 1)));
            for (const auto scalar : {120.0, 120.0, 0.0, 0.0}) {
                payload.writeU64(std::bit_cast<std::uint64_t>(scalar));
            }
        }
        REQUIRE(cuexis::chart::packed::writeRationalBeatAtom(payload, beat(1, 1)));
        payload.writeU64(std::bit_cast<std::uint64_t>(50.0));

        const auto patched = replaceSection(base, time->index, std::move(payload).takeBytes());
        const auto decoded = cuexis::chart::packed::decode(patched);
        REQUIRE_FALSE(decoded);
        CHECK(decoded.error().code() == "packed.time.order");
        CHECK(decoded.error().code() != "packed.identity.mismatch");
    }
}

TEST_CASE("R4 the resource closure is derived from the asset references",
          "[chart][packed][roundtrip][r4]") {
    SECTION("a declared closure matching the derivation round-trips and is restored") {
        const auto chart = componentChart();
        const auto decoded = decodedChart(encodedBytes(chart));
        REQUIRE(decoded.resourceClosure.resources.size() == chart.resourceClosure.resources.size());
        CHECK(decoded.resourceClosure == chart.resourceClosure);
        CHECK(decoded.resourceClosure.assetIds() == chart.resourceClosure.assetIds());
    }
    SECTION("an empty closure with an asset reference is refused") {
        auto chart = componentChart();
        chart.resourceClosure.resources.clear();
        CHECK(encodeCode(chart) == "packed.identity.closure");
    }
    SECTION("an extra unused resource is refused") {
        auto chart = componentChart();
        chart.resourceClosure.resources.push_back(
            CanonicalResourceUse{AssetId{"mesh.unused"}, CanonicalResourceUseKind::RenderableMesh});
        CHECK(encodeCode(chart) == "packed.identity.closure");
    }
    SECTION("a wrong use kind is refused") {
        auto chart = componentChart();
        std::replace_if(
            chart.resourceClosure.resources.begin(), chart.resourceClosure.resources.end(),
            [](const auto& use) { return use.assetId.value == "mesh.note"; },
            CanonicalResourceUse{AssetId{"mesh.note"},
                                 CanonicalResourceUseKind::RenderableMaterial});
        CHECK(encodeCode(chart) == "packed.identity.closure");
    }
    SECTION("main music is part of the derived closure") {
        auto chart = baseChart();
        auto entity = explicitEntity("0010");
        entity.requirements.push_back(tapRequirement(0U, RationalBeat::zero()));
        chart.entities.push_back(std::move(entity));
        chart.mainMusic = AssetId{"audio.main"};
        chart.resourceClosure.resources.push_back(
            CanonicalResourceUse{*chart.mainMusic, CanonicalResourceUseKind::MainMusic});
        const auto decoded = decodedChart(encodedBytes(chart));
        REQUIRE(decoded.resourceClosure.resources.size() == 1U);
        CHECK(decoded.resourceClosure.resources.front().assetId.value == "audio.main");
        CHECK(decoded.resourceClosure.resources.front().use == CanonicalResourceUseKind::MainMusic);
    }
}

TEST_CASE("R4 the registered default camera carries a position only",
          "[chart][packed][roundtrip][r4]") {
    SECTION("a position-only default camera round-trips") {
        auto chart = tapChart();
        chart.defaultCamera.defaultTransform = TransformData{};
        chart.defaultCamera.defaultTransform->position = {4.5F, -2.0F, 12.25F};
        const auto decoded = decodedChart(encodedBytes(chart));
        REQUIRE(decoded.defaultCamera.defaultTransform.has_value());
        CHECK(decoded.defaultCamera.defaultTransform->position.x == 4.5F);
        CHECK(decoded.defaultCamera.defaultTransform->position.y == -2.0F);
        CHECK(decoded.defaultCamera.defaultTransform->position.z == 12.25F);
        CHECK(sameSemantics(decoded, chart));
    }
    SECTION("a rotated default camera is refused instead of silently flattened") {
        auto chart = tapChart();
        chart.defaultCamera.defaultTransform = TransformData{};
        chart.defaultCamera.defaultTransform->rotation = {0.0F, 0.0F, 0.7071068F, 0.7071068F};
        CHECK(encodeCode(chart) == "packed.identity.camera_transform");
    }
    SECTION("a scaled default camera is refused instead of silently flattened") {
        auto chart = tapChart();
        chart.defaultCamera.defaultTransform = TransformData{};
        chart.defaultCamera.defaultTransform->scale = {2.0F, 2.0F, 2.0F};
        CHECK(encodeCode(chart) == "packed.identity.camera_transform");
    }
}

TEST_CASE("R4 the 40000 entity profile round-trips with complete semantics",
          "[chart][packed][roundtrip][r4][capacity]") {
    const auto chart = lowReuseChart(40000U);
    const auto artifact = encodedBytes(chart);
    CHECK(artifact.size() <= 16U * 1024U * 1024U);

    const auto decoded = decodedChart(artifact);
    CHECK(sameSemantics(decoded, chart));
    REQUIRE(decoded.entities.size() == 40000U);
    CHECK(std::get<ExplicitEntityIdentity>(decoded.entities[39999].identity)
              .objectId.value.ends_with("9d3f"));
    REQUIRE(decoded.entities[39999].requirements.size() == 1U);
    CHECK(decoded.entities[39999].requirements.front().interval.startBeat == beat(39999, 1));
    CHECK(std::get<LaneConstraint>(decoded.entities[39999].requirements.front().constraints.front())
              .lane == 3U);

    // Canonical: the decoded model re-encodes to exactly the same artifact and identity.
    const auto reencoded = encodedBytes(decoded);
    CHECK(reencoded == artifact);
    CHECK(identityHex(decoded) == identityHex(chart));
}

TEST_CASE("R4 the CXT stair ladder expands and round-trips through Packed",
          "[chart][packed][roundtrip][r4][cxt]") {
    const auto chart = expandedLadder("pattern_stair.cxt");

    // Source semantics: 16 generated entities, the second one carries lane 2 at beat 33/2.
    REQUIRE(chart.entities.size() == 16U);
    const auto& second = chart.entities[2];
    const auto& identity = std::get<cuexis::chart::GeneratedEntityIdentity>(second.identity);
    CHECK(identity.bindingId == "intro-stair");
    CHECK(identity.moduleId == "pattern.stair");
    CHECK(identity.exportId == "stair");
    REQUIRE(identity.path.size() == 2U);
    CHECK(identity.path[0] == cuexis::chart::SemanticIdentityStep{"groups", 1U});
    CHECK(identity.path[1] == cuexis::chart::SemanticIdentityStep{"n2", 0U});
    CHECK(std::get<CanonicalTransform>(second.components.front()).position.x == 2.0F);
    CHECK(second.requirements.front().interval.startBeat == beat(33, 2));
    CHECK(std::get<LaneConstraint>(second.requirements.front().constraints.front()).lane == 2U);
    CHECK(chart.resourceClosure.resources.empty());

    const auto artifact = encodedBytes(chart);
    const auto decoded = decodedChart(artifact);
    CHECK(sameSemantics(decoded, chart));
    CHECK(sameTiming(decoded.timing, chart.timing));
    CHECK(decoded.resourceClosure.resources.empty());
    CHECK(identityHex(decoded) == identityHex(chart));
    CHECK(encodedBytes(decoded) == artifact);

    // The reordered source fixture expands to the same semantics, so it must produce the same
    // canonical artifact.
    const auto reordered = expandedLadder("pattern_stair_reordered.cxt");
    CHECK(sameSemantics(reordered, chart));
    CHECK(encodedBytes(reordered) == artifact);
}

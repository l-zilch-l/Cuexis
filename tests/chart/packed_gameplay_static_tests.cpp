#include "packed_fixture_support.hpp"
#include "packed_gameplay_internal.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <limits>

namespace {
using namespace cuexis::chart;
using namespace cuexis::chart::packed;
namespace gp = cuexis::chart::packed::gameplay_detail;
namespace support = cuexis::chart::packed::test;

auto fixture() -> CanonicalSemanticChart {
    CanonicalSemanticChart chart;
    chart.chartId.value = "019b0000-0000-7abc-8def-000000000001";
    chart.mainMusic = AssetId{"asset.music"};
    chart.features = {{"test.feature.z", 2}, {"test.feature.a", 1}};
    chart.timing.offsetMs = 23.5;
    chart.timing.defaultBpm = 125;
    chart.timing.tempoEvents = {
        {RationalBeat::zero(), *RationalBeat::create(3, 2), 125, 130, 1.5, 0.5}};
    chart.timing.stops = {{*RationalBeat::create(5, 2), 17.25}};
    chart.defaultCamera.pitch = 4.5;
    chart.defaultCamera.yaw = 9;
    chart.defaultCamera.roll = -1;
    chart.defaultCamera.defaultTransform = TransformData{{1, 2, 3}, {0, 0, 0, 1}, {1, 1, 1}};
    CanonicalEntity first;
    first.identity = ExplicitEntityIdentity{ChartObjectId{"019b0000-0000-7abc-8def-000000000003"}};
    first.components = {CanonicalTransform{{3, 2, 1}, {0, 0, 0, 1}, {1, 2, 3}},
                        CanonicalRenderable{AssetId{"asset.mesh"}, AssetId{"asset.material"}, 27},
                        CanonicalCamera{"perspective", 77, 0.3, 4321}};
    auto second = first;
    second.identity = ExplicitEntityIdentity{ChartObjectId{"019b0000-0000-7abc-8def-000000000002"}};
    second.components = {CanonicalTransform{{8, 2, 1}, {0, 0, 0, 1}, {1, 2, 3}},
                         CanonicalRenderable{AssetId{"asset.mesh"}, AssetId{"asset.material"}, 31},
                         CanonicalCamera{"perspective", 78, 0.7, 4322}};
    first.parent = second.identity;
    chart.entities = {first, second};
    chart.resourceClosure.resources = {
        {*chart.mainMusic, CanonicalResourceUseKind::MainMusic},
        {AssetId{"asset.mesh"}, CanonicalResourceUseKind::RenderableMesh},
        {AssetId{"asset.material"}, CanonicalResourceUseKind::RenderableMaterial}};
    return chart;
}

auto envelope(gp::StaticTables tables, std::uint32_t requirements = 3, bool reverse = false)
    -> std::vector<std::byte> {
    // These four opaque payloads only exercise the static layer. They are not valid executable
    // Gameplay records, and this helper never claims a complete Capsule validation.
    for (const auto& [code, count] : std::array<std::pair<std::array<char, 4>, std::uint32_t>, 4>{
             {{{'G', 'P', 'H', '0'}, 1},
              {{'G', 'P', 'R', '0'}, requirements},
              {{'G', 'P', 'D', '0'}, 0},
              {{'G', 'R', 'C', '0'}, 0}}}) {
        ByteWriter local;
        local.writeU16(1);
        local.writeU8(1);
        local.writeU8(0);
        local.writeU32(count);
        local.writeU64(0);
        tables.sections.push_back({code, std::move(local).takeBytes(), count});
    }
    std::sort(tables.sections.begin(), tables.sections.end(),
              [](const auto& left, const auto& right) { return left.code < right.code; });
    if (reverse) {
        std::reverse(tables.sections.begin(), tables.sections.end());
    }
    std::uint32_t decoded = 0;
    for (const auto& section : tables.sections) {
        decoded += static_cast<std::uint32_t>(section.bytes.size());
    }
    const auto directoryBytes = static_cast<std::uint32_t>(32 * tables.sections.size());
    const auto total = 96U + directoryBytes + decoded;
    ByteWriter file;
    constexpr std::array magic{std::byte{'C'}, std::byte{'X'}, std::byte{'P'}, std::byte{'K'},
                               std::byte{'5'}, std::byte{0},   std::byte{0},   std::byte{0}};
    file.writeBytes(magic);
    file.writeU16(1);
    file.writeU16(96);
    file.writeU32(1);
    file.writeU32(total);
    file.writeU32(96);
    file.writeU32(static_cast<std::uint32_t>(tables.sections.size()));
    file.writeU32(directoryBytes);
    file.writeBytes(std::array<std::byte, 32>{});
    file.writeU32(static_cast<std::uint32_t>(tables.entityIdentities.size()));
    file.writeU32(requirements);
    file.writeU32(0);
    file.writeU32(decoded);
    file.writeU32(static_cast<std::uint32_t>(tables.strings.size()));
    file.writeU32(static_cast<std::uint32_t>(tables.references.size()));
    file.writeU32(2);
    file.writeU32(crc32(file.bytes()));
    auto offset = 96U + directoryBytes;
    for (const auto& section : tables.sections) {
        const auto size = static_cast<std::uint32_t>(section.bytes.size());
        file.writeBytes(std::as_bytes(std::span{section.code}));
        file.writeU8(0);
        file.writeU8(0);
        file.writeU16(0);
        file.writeU32(offset);
        file.writeU32(size);
        file.writeU32(size);
        file.writeU32(section.records);
        file.writeU32(crc32(section.bytes));
        file.writeU32(0);
        offset += size;
    }
    for (const auto& section : tables.sections) {
        file.writeBytes(section.bytes);
    }
    return std::move(file).takeBytes();
}
} // namespace

TEST_CASE("Gameplay static Packed tables preserve real entities and shared dictionaries",
          "[packed][gameplay][static]") {
    const auto chart = fixture();
    const std::array owners{chart.entities.front().identity};
    const std::array<std::string, 2> extraStrings{"gameplay.token", ""};
    const std::array<gp::Reference, 3> extraRefs{
        {{10, "resource.one"}, {5, "action.one"}, {1, "asset.mesh"}}};
    const auto tables = gp::encodeStatic(chart, owners, extraStrings, extraRefs);
    REQUIRE(tables);
    CHECK(tables->entityIdentities[0] == chart.entities[1].identity);
    CHECK(tables->entityIdentities[1] == chart.entities[0].identity);
    CHECK(tables->entityMasks == std::vector<std::uint64_t>{19, 23});
    CHECK(tables->strings.front().empty());
    CHECK(std::count(tables->references.begin(), tables->references.end(),
                     gp::Reference{1, "asset.mesh"}) == 1);
    const auto bytes = envelope(*tables);
    REQUIRE(gp::inspect(bytes));
    const auto result = gp::decodeStatic(bytes);
    REQUIRE(result);
    CHECK(result->chart.chartId == chart.chartId);
    CHECK(result->chart.mainMusic == chart.mainMusic);
    CHECK(result->chart.features ==
          std::vector<CanonicalFeature>{{"test.feature.a", 1}, {"test.feature.z", 2}});
    CHECK(result->chart.entities[0] == chart.entities[1]);
    CHECK(result->chart.entities[1] == chart.entities[0]);
    CHECK(result->chart.entities[1].requirements.empty());
    CHECK(result->entityMasks == tables->entityMasks);
    CHECK(result->references == tables->references);
    CHECK(result->strings == tables->strings);
    CHECK(result->chart.timing.offsetMs == chart.timing.offsetMs);
    CHECK(result->chart.timing.defaultBpm == chart.timing.defaultBpm);
    const auto& tempo = result->chart.timing.tempoEvents[0];
    const auto& originalTempo = chart.timing.tempoEvents[0];
    CHECK(tempo.startBeat == originalTempo.startBeat);
    CHECK(tempo.durationBeats == originalTempo.durationBeats);
    CHECK(tempo.startBpm == originalTempo.startBpm);
    CHECK(tempo.endBpm == originalTempo.endBpm);
    CHECK(tempo.startSlope == originalTempo.startSlope);
    CHECK(tempo.endSlope == originalTempo.endSlope);
    CHECK(result->chart.timing.stops[0].beat == chart.timing.stops[0].beat);
    CHECK(result->chart.timing.stops[0].durationMs == chart.timing.stops[0].durationMs);
    const auto& camera = result->chart.defaultCamera;
    CHECK(camera.type == chart.defaultCamera.type);
    CHECK(camera.fovY == chart.defaultCamera.fovY);
    CHECK(camera.nearPlane == chart.defaultCamera.nearPlane);
    CHECK(camera.farPlane == chart.defaultCamera.farPlane);
    CHECK(camera.pitch == chart.defaultCamera.pitch);
    CHECK(camera.yaw == chart.defaultCamera.yaw);
    CHECK(camera.roll == chart.defaultCamera.roll);
    CHECK(camera.defaultTransform->position == chart.defaultCamera.defaultTransform->position);
    CHECK(camera.defaultTransform->rotation == chart.defaultCamera.defaultTransform->rotation);
    CHECK(camera.defaultTransform->scale == chart.defaultCamera.defaultTransform->scale);
    auto closure = chart.resourceClosure.resources;
    std::sort(closure.begin(), closure.end());
    CHECK(result->chart.resourceClosure.resources == closure);
}

TEST_CASE("Gameplay static Packed paths are explicit and do not certify a full Capsule",
          "[packed][gameplay][static][boundary]") {
    auto chart = fixture();
    auto tables = gp::encodeStatic(chart, {}, {}, {});
    REQUIRE(tables);
    const auto bytes = envelope(*tables);
    REQUIRE(gp::decodeStatic(bytes));
    const auto rejected = decode(bytes);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().code() == "packed.header.unsupported_revision");
    REQUIRE_FALSE(inspect(bytes));

    chart.features.clear();
    const auto legacy = encode(chart);
    REQUIRE(legacy);
    REQUIRE(decode(*legacy));
    const auto wrongRevision = gp::decodeStatic(*legacy);
    REQUIRE_FALSE(wrongRevision);
    CHECK(wrongRevision.error().code() == "packed.header.unsupported_revision");
    auto altered = bytes;
    support::writeU8(altered, 32, 127);
    support::refreshHeaderCrc(altered);
    // Static decode cannot validate the full digest without the Gameplay graph.
    REQUIRE(gp::decodeStatic(altered));
}

TEST_CASE("Gameplay static Packed sections support physical permutations and inspection",
          "[packed][gameplay][static][determinism]") {
    auto chart = fixture();
    const std::array owners{chart.entities[0].identity};
    const std::array<std::string, 2> seed{"z.token", "a.token"};
    const std::array<gp::Reference, 2> refs{{{10, "resource.z"}, {10, "resource.a"}}};
    const auto first = gp::encodeStatic(chart, owners, seed, refs);
    REQUIRE(first);
    std::reverse(chart.entities.begin(), chart.entities.end());
    std::reverse(chart.features.begin(), chart.features.end());
    auto seedReordered = seed;
    auto refsReordered = refs;
    std::reverse(seedReordered.begin(), seedReordered.end());
    std::reverse(refsReordered.begin(), refsReordered.end());
    const auto second = gp::encodeStatic(chart, owners, seedReordered, refsReordered);
    REQUIRE(second);
    CHECK(envelope(*first) == envelope(*second));
    const auto reversed = gp::decodeStatic(envelope(*second, 3, true));
    REQUIRE(reversed);
    CHECK(reversed->entityMasks == first->entityMasks);
    const auto bytes = envelope(*first);
    const auto inspected = support::appendSection(bytes, "XDBG", 1, 0, {});
    REQUIRE(gp::inspect(inspected));
    REQUIRE(gp::decodeStatic(inspected));
    const auto hidden = support::appendSection(bytes, "GPR1", 0, 0, {});
    REQUIRE_FALSE(gp::inspect(hidden));
}

TEST_CASE("Gameplay static Packed rejects inconsistent envelopes before typed consumption",
          "[packed][gameplay][static][negative]") {
    const auto chart = fixture();
    const std::array owners{chart.entities[0].identity};
    const auto tables = gp::encodeStatic(chart, owners, {}, {});
    REQUIRE(tables);
    auto bytes = envelope(*tables);
    SECTION("missing mandatory Gameplay section") {
        support::renameSection(bytes, *support::findSection(bytes, "GPD0"), "XDBG");
        support::writeU8(bytes, 96 + 32 * support::findSection(bytes, "XDBG")->index + 5, 1);
    }
    SECTION("Gameplay section hidden as inspection") {
        support::writeU8(bytes, 96 + 32 * support::findSection(bytes, "GPR0")->index + 5, 1);
    }
    SECTION("false Header requirement count") {
        support::writeU32(bytes, 68, 4);
    }
    SECTION("missing component stream") {
        support::renameSection(bytes, *support::findSection(bytes, "TRN0"), "XDBG");
        support::writeU8(bytes, 96 + 32 * support::findSection(bytes, "XDBG")->index + 5, 1);
    }
    SECTION("wrong stream count") {
        support::writeU32(bytes, 96 + 32 * support::findSection(bytes, "CAM0")->index + 20, 1);
    }
    SECTION("wrong static table count") {
        support::writeU32(bytes, 96 + 32 * support::findSection(bytes, "IDN0")->index + 20, 7);
    }
    SECTION("mixed legacy requirements") {
        support::writeU32(bytes, 96 + 32 * support::findSection(bytes, "REQ0")->index + 20, 3);
    }
    SECTION("malformed UTF-8") {
        const auto section = *support::findSection(bytes, "STR0");
        auto strings = support::readStrings(
            std::span<const std::byte>{bytes}.subspan(section.offset, section.size));
        strings.back() = "\xff";
        bytes = support::replaceSection(bytes, section.index, support::buildStrings(strings));
    }
    SECTION("unknown reference kind") {
        const auto section = *support::findSection(bytes, "REF0");
        support::writeU8(bytes, section.offset, 14);
        support::refreshSectionCrc(bytes, section);
    }
    SECTION("corrupt section CRC") {
        const auto section = *support::findSection(bytes, "META");
        support::writeU8(bytes, section.offset, 99);
    }
    support::refreshHeaderCrc(bytes);
    REQUIRE_FALSE(gp::decodeStatic(bytes));
}

TEST_CASE("Gameplay static preimage separates legacy domain and owner presence",
          "[packed][gameplay][static][identity]") {
    auto chart = fixture();
    chart.features.clear();
    const std::array owner{chart.entities[0].identity};
    const auto first = gp::staticPreimage(chart, owner);
    const auto noOwner = gp::staticPreimage(chart, {});
    const auto legacy = semanticPreimage(chart);
    REQUIRE(first);
    REQUIRE(noOwner);
    REQUIRE(legacy);
    CHECK(*first != *noOwner);
    CHECK(*noOwner != *legacy);
    const std::string domain{"cuexis.chart.semantic.v5.gameplay.1"};
    CHECK(std::equal(domain.begin(), domain.end(), first->begin(),
                     [](char left, std::byte right) { return std::byte(left) == right; }));
    CHECK((*first)[domain.size()] == std::byte{0});
    std::reverse(chart.entities.begin(), chart.entities.end());
    CHECK(*gp::staticPreimage(chart, owner) == *first);
    chart.entities[0].components.push_back(CanonicalTransform{});
    REQUIRE_FALSE(gp::staticPreimage(chart, owner));
}

TEST_CASE("Gameplay static Writer rejects fabricated ownership and legacy payloads",
          "[packed][gameplay][static][negative]") {
    auto chart = fixture();
    std::vector<CanonicalEntityIdentity> owners;
    std::vector<std::string> seed;
    std::vector<gp::Reference> refs;
    PackedChartLimits limits;
    SECTION("missing owner") {
        owners.push_back(
            ExplicitEntityIdentity{ChartObjectId{"019b0000-0000-7abc-8def-000000000099"}});
    }
    SECTION("duplicate owner") {
        owners = {chart.entities[0].identity, chart.entities[0].identity};
    }
    SECTION("legacy Requirement") {
        chart.entities[0].requirements.push_back(CanonicalRequirement{});
    }
    SECTION("invalid UTF-8") {
        seed = {"\xc0\xaf"};
    }
    SECTION("unknown reference kind") {
        refs = {{14, "unknown"}};
    }
    SECTION("entity budget") {
        limits.maxPackedEntities = 1;
    }
    SECTION("reference budget") {
        limits.maxPackedReferences = 1;
    }
    SECTION("string budget") {
        limits.maxPackedStrings = 1;
    }
    SECTION("section budget") {
        limits.maxPackedSectionBytes = 1;
    }
    SECTION("decoded budget") {
        limits.maxPackedDecodedBytes = 1;
    }
    SECTION("file budget") {
        limits.maxPackedFileBytes = 1;
    }
    SECTION("parent cycle") {
        chart.entities[1].parent = chart.entities[0].identity;
    }
    SECTION("closure disagreement") {
        chart.resourceClosure.resources.clear();
    }
    SECTION("nonfinite static field") {
        chart.timing.offsetMs = std::numeric_limits<double>::infinity();
    }
    REQUIRE_FALSE(gp::encodeStatic(chart, owners, seed, refs, limits));
}

TEST_CASE("Gameplay static empty chart remains a complete Foundation envelope",
          "[packed][gameplay][static]") {
    CanonicalSemanticChart chart;
    chart.chartId.value = "019b0000-0000-7abc-8def-000000000001";
    const auto tables = gp::encodeStatic(chart, {}, {}, {});
    REQUIRE(tables);
    CHECK(tables->sections.size() == 9);
    const auto bytes = envelope(*tables, 0);
    REQUIRE(gp::inspect(bytes));
    const auto decoded = gp::decodeStatic(bytes);
    REQUIRE(decoded);
    CHECK(decoded->chart.entities.empty());
    CHECK(decoded->entityMasks.empty());
}

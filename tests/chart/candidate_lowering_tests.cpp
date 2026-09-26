#include <cuexis/chart/candidate_lowering.hpp>
#include <cuexis/chart/packed_chart_tables.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>

namespace {

using cuexis::chart::CanonicalEntity;
using cuexis::chart::CanonicalFeature;
using cuexis::chart::CanonicalRenderable;
using cuexis::chart::CanonicalRequirement;
using cuexis::chart::CanonicalResourceUse;
using cuexis::chart::CanonicalResourceUseKind;
using cuexis::chart::CanonicalSemanticChart;
using cuexis::chart::ChartId;
using cuexis::chart::ChartObjectId;
using cuexis::chart::ExplicitEntityIdentity;
using cuexis::chart::GeneratedEntityIdentity;
using cuexis::chart::LaneConstraint;
using cuexis::chart::SemanticIdentityStep;

[[nodiscard]] auto hexValue(char character) -> int {
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'a' && character <= 'f') {
        return character - 'a' + 10;
    }
    return -1;
}

[[nodiscard]] auto parseHex32(std::string_view hex) -> std::array<std::uint8_t, 32> {
    REQUIRE(hex.size() == 64);
    std::array<std::uint8_t, 32> bytes{};
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        const auto high = hexValue(hex[index * 2]);
        const auto low = hexValue(hex[index * 2 + 1]);
        REQUIRE(high >= 0);
        REQUIRE(low >= 0);
        bytes[index] = static_cast<std::uint8_t>((high << 4) | low);
    }
    return bytes;
}

[[nodiscard]] auto tapRequirement(std::string localId, std::uint32_t lane) -> CanonicalRequirement {
    CanonicalRequirement requirement;
    requirement.localId = std::move(localId);
    requirement.judgementDomain = {"judgement-domain", "candidate.lanes4"};
    requirement.requiredAction = {"action", "press"};
    requirement.constraints.emplace_back(LaneConstraint{lane});
    return requirement;
}

[[nodiscard]] auto explicitEntity(std::string id) -> CanonicalEntity {
    CanonicalEntity entity;
    entity.identity = ExplicitEntityIdentity{ChartObjectId{std::move(id)}};
    return entity;
}

[[nodiscard]] auto tapChart() -> CanonicalSemanticChart {
    CanonicalSemanticChart chart;
    chart.chartId = ChartId{"019b0000-0000-7abc-8def-000000000001"};
    chart.features.push_back(CanonicalFeature{"cuexis.gameplay.candidate.lanes4", 1});
    auto entity = explicitEntity("019b0000-0000-7abc-8def-000000000010");
    entity.requirements.push_back(tapRequirement("hit", 2));
    chart.entities.push_back(std::move(entity));
    return chart;
}

[[nodiscard]] auto hasCode(const cuexis::core::Diagnostics& diagnostics, std::string_view code)
    -> bool {
    for (const auto& item : diagnostics.items()) {
        if (item.code() == code) {
            return true;
        }
    }
    return false;
}

} // namespace

TEST_CASE("Candidate lowering keeps explicit identity, requirements and opacity",
          "[chart][candidate][lowering]") {
    auto chart = tapChart();
    chart.resourceClosure.resources.push_back(CanonicalResourceUse{
        cuexis::chart::AssetId{"mesh.note"}, CanonicalResourceUseKind::RenderableMesh});
    chart.resourceClosure.resources.push_back(CanonicalResourceUse{
        cuexis::chart::AssetId{"mat.note"}, CanonicalResourceUseKind::RenderableMaterial});
    chart.entities[0].components.push_back(CanonicalRenderable{
        cuexis::chart::AssetId{"mesh.note"}, cuexis::chart::AssetId{"mat.note"}, 128});

    const auto lowered = cuexis::chart::lowerCandidateRuntime(chart);
    REQUIRE(lowered.hasValue());
    REQUIRE(lowered.artifact->runtime.objects.size() == 1);
    CHECK(lowered.artifact->runtime.objects[0].id.value == "019b0000-0000-7abc-8def-000000000010");
    CHECK(lowered.artifact->runtime.objects[0].renderableOpacity == Catch::Approx(128.0 / 255.0));
    REQUIRE(lowered.artifact->metadata.objects.size() == 1);
    const auto& metadata = lowered.artifact->metadata.objects[0];
    CHECK(metadata.runtimeIndex == 0);
    CHECK(metadata.executionId == lowered.artifact->runtime.objects[0].id.value);
    REQUIRE(std::holds_alternative<ExplicitEntityIdentity>(metadata.identity));
    REQUIRE(metadata.requirements.size() == 1);
    CHECK(metadata.requirements[0].localId == "hit");
    CHECK(std::get<LaneConstraint>(metadata.requirements[0].constraints[0]).lane == 2);
    CHECK(metadata.renderableOpacity == Catch::Approx(128.0 / 255.0));
    CHECK(lowered.artifact->metadata.resourceClosure.resources.size() == 2);
}

TEST_CASE("Generated candidate execution IDs match the Stage 6 golden",
          "[chart][candidate][identity]") {
    CanonicalSemanticChart chart;
    chart.chartId = ChartId{"019b0000-0000-7abc-8def-000000000001"};
    GeneratedEntityIdentity generated;
    generated.chartId = chart.chartId;
    generated.bindingId = "binding-main";
    generated.moduleId = "module-stairs";
    generated.exportId = "emit-note";
    generated.path.push_back(SemanticIdentityStep{"group", 2});
    generated.path.push_back(SemanticIdentityStep{"note", 0});
    CanonicalEntity entity;
    entity.identity = std::move(generated);
    chart.entities.push_back(std::move(entity));

    const auto lowered = cuexis::chart::lowerCandidateRuntime(chart);
    REQUIRE(lowered.hasValue());
    CHECK(lowered.artifact->runtime.objects[0].id.value ==
          "v5g1:75dafa6d8fa0e5f47590d81ce9809c5ea0c183009cc3835defcb22fe4b501e57");
    CHECK(lowered.artifact->metadata.objects[0].executionId ==
          lowered.artifact->runtime.objects[0].id.value);
    CHECK(std::holds_alternative<GeneratedEntityIdentity>(
        lowered.artifact->metadata.objects[0].identity));
}

TEST_CASE("Candidate lowering rejects duplicate identities, missing parents and cycles",
          "[chart][candidate][lowering]") {
    SECTION("duplicate identity publishes nothing") {
        auto chart = tapChart();
        chart.entities.push_back(chart.entities.front());
        const auto lowered = cuexis::chart::lowerCandidateRuntime(chart);
        CHECK_FALSE(lowered.hasValue());
        CHECK_FALSE(lowered.artifact.has_value());
        CHECK(hasCode(lowered.diagnostics, "packed.identity.duplicate_identity"));
    }
    SECTION("missing parent publishes nothing") {
        auto chart = tapChart();
        chart.entities[0].parent =
            ExplicitEntityIdentity{ChartObjectId{"019b0000-0000-7abc-8def-000000000099"}};
        const auto lowered = cuexis::chart::lowerCandidateRuntime(chart);
        CHECK_FALSE(lowered.hasValue());
        CHECK_FALSE(lowered.artifact.has_value());
        CHECK(hasCode(lowered.diagnostics, "packed.identity.parent_missing"));
    }
    SECTION("parent cycle publishes nothing") {
        CanonicalSemanticChart chart;
        chart.chartId = ChartId{"019b0000-0000-7abc-8def-000000000001"};
        auto first = explicitEntity("019b0000-0000-7abc-8def-0000000000aa");
        auto second = explicitEntity("019b0000-0000-7abc-8def-0000000000bb");
        first.parent =
            ExplicitEntityIdentity{ChartObjectId{"019b0000-0000-7abc-8def-0000000000bb"}};
        second.parent =
            ExplicitEntityIdentity{ChartObjectId{"019b0000-0000-7abc-8def-0000000000aa"}};
        chart.entities.push_back(std::move(first));
        chart.entities.push_back(std::move(second));
        const auto lowered = cuexis::chart::lowerCandidateRuntime(chart);
        CHECK_FALSE(lowered.hasValue());
        CHECK_FALSE(lowered.artifact.has_value());
        CHECK(hasCode(lowered.diagnostics, "packed.identity.parent_cycle"));
    }
}

TEST_CASE("Candidate lowering remaps parents into execution-id order",
          "[chart][candidate][lowering]") {
    CanonicalSemanticChart chart;
    chart.chartId = ChartId{"019b0000-0000-7abc-8def-000000000001"};
    auto child = explicitEntity("019b0000-0000-7abc-8def-0000000000c0");
    child.parent = ExplicitEntityIdentity{ChartObjectId{"019b0000-0000-7abc-8def-0000000000b0"}};
    chart.entities.push_back(std::move(child));
    chart.entities.push_back(explicitEntity("019b0000-0000-7abc-8def-0000000000b0"));

    const auto lowered = cuexis::chart::lowerCandidateRuntime(chart);
    REQUIRE(lowered.hasValue());
    REQUIRE(lowered.artifact->runtime.objects.size() == 2);
    CHECK(lowered.artifact->runtime.objects[0].id.value == "019b0000-0000-7abc-8def-0000000000b0");
    CHECK(lowered.artifact->runtime.objects[1].id.value == "019b0000-0000-7abc-8def-0000000000c0");
    CHECK_FALSE(lowered.artifact->runtime.objects[0].parentIndex.has_value());
    REQUIRE(lowered.artifact->runtime.objects[1].parentIndex.has_value());
    CHECK(*lowered.artifact->runtime.objects[1].parentIndex == 0);
    CHECK(lowered.artifact->metadata.objects[1].runtimeIndex == 1);
    REQUIRE(lowered.artifact->metadata.objects[1].parent.has_value());
}

TEST_CASE("Candidate prepared identity matches the Stage 6 golden and rejects duplicates",
          "[chart][candidate][identity]") {
    const auto semantic =
        parseHex32("9b1714dc3087fddd2a73ad2d947c1f9854e17f5964135085a232edb31003a091");
    const auto audio = parseHex32(std::string(64, '1'));
    const auto material = parseHex32(std::string(64, '2'));
    const auto mesh = parseHex32(std::string(64, '3'));
    std::vector<cuexis::chart::PreparedResourceIdentityComponent> resources{
        {cuexis::chart::AssetId{"mesh.note"}, {mesh}},
        {cuexis::chart::AssetId{"audio.main"}, {audio}},
        {cuexis::chart::AssetId{"mat.note"}, {material}},
    };

    const auto assembled =
        cuexis::chart::assembleCandidatePreparedSemanticIdentity(semantic, resources);
    REQUIRE(assembled.has_value());
    CHECK(assembled->sha256 ==
          parseHex32("63451a64d86865dca648c88c12addf1762160e23e8e678ce65447d039f556999"));

    resources[0].identity.sha256[0] ^= 0x01;
    const auto changed =
        cuexis::chart::assembleCandidatePreparedSemanticIdentity(semantic, resources);
    REQUIRE(changed.has_value());
    CHECK(changed->sha256 != assembled->sha256);

    resources.push_back(resources.front());
    const auto duplicate =
        cuexis::chart::assembleCandidatePreparedSemanticIdentity(semantic, resources);
    CHECK_FALSE(duplicate.has_value());
    CHECK(duplicate.error().code() == "candidate.identity.resource_duplicate");
}

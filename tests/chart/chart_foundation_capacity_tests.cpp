#include <cuexis/chart/packed_chart_io.hpp>
#include <cuexis/chart/packed_chart_tables.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>

namespace {

using cuexis::chart::CanonicalEntity;
using cuexis::chart::CanonicalRequirement;
using cuexis::chart::CanonicalSemanticChart;
using cuexis::chart::ChartObjectId;
using cuexis::chart::ExplicitEntityIdentity;
using cuexis::chart::LaneConstraint;
using cuexis::chart::RationalBeat;
using cuexis::chart::TypedReference;

[[nodiscard]] auto hex12(std::uint32_t value) -> std::string {
    static constexpr char digits[] = "0123456789abcdef";
    std::string result(12, '0');
    for (std::size_t index = result.size(); index > 0; --index) {
        result[index - 1] = digits[value & 0x0fU];
        value >>= 4U;
    }
    return result;
}

[[nodiscard]] auto makeLowReuseChart(std::size_t count) -> CanonicalSemanticChart {
    CanonicalSemanticChart chart;
    chart.chartId = cuexis::chart::ChartId{"019b0000-0000-7abc-8def-000000000001"};
    chart.features.push_back(
        cuexis::chart::CanonicalFeature{"cuexis.gameplay.candidate.lanes4", 1});
    chart.entities.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        CanonicalEntity entity;
        entity.identity = ExplicitEntityIdentity{ChartObjectId{
            "019b0000-0000-7abc-8def-" + hex12(static_cast<std::uint32_t>(index + 0x100U))}};
        const auto beat = RationalBeat::create(static_cast<std::int64_t>(index), 1);
        CanonicalRequirement requirement;
        requirement.localId = "hit";
        requirement.interval.startBeat = beat ? *beat : RationalBeat::zero();
        requirement.judgementDomain = TypedReference{"judgement-domain", "candidate.lanes4"};
        requirement.requiredAction = TypedReference{"action", "press"};
        requirement.constraints.emplace_back(
            LaneConstraint{static_cast<std::uint32_t>(index % 4U)});
        entity.requirements.push_back(std::move(requirement));
        chart.entities.push_back(std::move(entity));
    }
    return chart;
}

} // namespace

TEST_CASE("Foundation low-reuse profile measures 40000 independent entities",
          "[chart][packed][capacity][f8]") {
    const auto chart = makeLowReuseChart(40000U);
    const auto limits = cuexis::chart::PackedChartLimits{};
    const auto sizing = cuexis::chart::PackedChartWriter::size(chart, {}, limits);
    REQUIRE(sizing);
    REQUIRE(sizing->statistics.entityCount == 40000U);
    REQUIRE(sizing->statistics.requirementCount == 40000U);
    REQUIRE(sizing->statistics.packedBytes <= 16U * 1024U * 1024U);

    std::cout << "foundation.capacity.low-reuse-v1 entities=" << sizing->statistics.entityCount
              << " requirements=" << sizing->statistics.requirementCount
              << " packedBytes=" << sizing->statistics.packedBytes
              << " decodedBytes=" << sizing->statistics.decodedBytes << '\n';
    CHECK_FALSE(sizing->sections.empty());
    CHECK(std::any_of(sizing->sections.begin(), sizing->sections.end(), [](const auto& section) {
        return section.type == "IDN0" && section.recordCount == 40000U;
    }));
}

TEST_CASE("Foundation Packed encoder rejects more than 40000 entities before publication",
          "[chart][packed][capacity][security][f8]") {
    const auto chart = makeLowReuseChart(40001U);
    const auto encoded = cuexis::chart::packed::encode(chart);
    REQUIRE_FALSE(encoded);
    CHECK(encoded.error().code() == "packed.budget.entities");
}

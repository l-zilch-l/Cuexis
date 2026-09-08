#include <cuexis/chart/canonical_semantic_chart.hpp>

#include <catch2/catch_test_macros.hpp>

namespace cuexis::chart {

TEST_CASE("Canonical semantic chart models concrete entities and parent identity",
          "[chart][foundation][f1]") {
    CanonicalSemanticChart chart;
    chart.chartId.value = "chart-foundation";

    CanonicalEntity parent;
    parent.identity = ExplicitEntityIdentity{ChartObjectId{"parent"}};

    CanonicalEntity child;
    child.identity = ExplicitEntityIdentity{ChartObjectId{"child"}};
    child.parent = parent.identity;
    child.components.push_back(CanonicalTransform{});

    chart.entities = {parent, child};

    REQUIRE(chart.entities.size() == 2);
    CHECK(isExplicitIdentity(chart.entities.front().identity));
    REQUIRE(chart.entities[1].parent.has_value());
    CHECK(std::get<ExplicitEntityIdentity>(*chart.entities[1].parent).objectId.value == "parent");
    CHECK(std::holds_alternative<CanonicalTransform>(chart.entities[1].components.front()));
}

TEST_CASE("Generated identity keeps the typed expansion path independent of entity handles",
          "[chart][foundation][f1]") {
    GeneratedEntityIdentity generated;
    generated.chartId.value = "chart";
    generated.bindingId = "intro";
    generated.moduleId = "pattern.stair";
    generated.exportId = "stair";
    generated.path = {
        SemanticIdentityStep{"groups", 1},
        SemanticIdentityStep{"n2", 0},
    };

    CanonicalEntity entity;
    entity.identity = generated;

    REQUIRE(isGeneratedIdentity(entity.identity));
    const auto& identity = std::get<GeneratedEntityIdentity>(entity.identity);
    CHECK(identity.path.size() == 2);
    CHECK(identity.path[0].iterationIndexPlusOne == 1);
    CHECK(identity.path[1].iterationIndexPlusOne == 0);
}

TEST_CASE("Canonical requirements retain interval, typed references, constraints and closure",
          "[chart][foundation][f1]") {
    CanonicalRequirement requirement;
    requirement.localId = "hit";
    requirement.kind = CanonicalRequirementKind::Tap;
    requirement.interval.kind = CanonicalIntervalKind::Point;
    requirement.interval.startBeat = RationalBeat::one();
    requirement.judgementDomain = TypedReference{"judgement-domain", "candidate.lanes4"};
    requirement.requiredAction = TypedReference{"action", "press"};
    requirement.constraints.emplace_back(LaneConstraint{2});

    CanonicalEntity entity;
    entity.identity = ExplicitEntityIdentity{ChartObjectId{"note"}};
    entity.requirements.push_back(requirement);
    entity.requirements.front().effects.clear();

    CanonicalResourceClosure closure;
    closure.resources.push_back(
        CanonicalResourceUse{AssetId{"mesh.note"}, CanonicalResourceUseKind::RenderableMesh});
    closure.resources.push_back(CanonicalResourceUse{AssetId{"material.note"},
                                                     CanonicalResourceUseKind::RenderableMaterial});

    REQUIRE(entity.requirements.size() == 1);
    CHECK(entity.requirements.front().judgementDomain.id == "candidate.lanes4");
    CHECK(std::get<LaneConstraint>(entity.requirements.front().constraints.front()).lane == 2);
    CHECK(entity.requirements.front().effects.empty());
    CHECK(entity.componentMask() == (std::uint64_t{1} << 2U));
    CHECK(closure.assetIds() ==
          std::vector<AssetId>{AssetId{"mesh.note"}, AssetId{"material.note"}});
}

TEST_CASE("Canonical semantic chart stores only typed semantic values", "[chart][foundation][f1]") {
    CanonicalSemanticChart left;
    left.chartId.value = "chart";
    CanonicalSemanticChart right = left;
    right.entities.push_back(
        CanonicalEntity{ExplicitEntityIdentity{ChartObjectId{"note"}}, std::nullopt, {}, {}});

    CHECK(left.chartId.value == right.chartId.value);
    CHECK(right.entities.size() == 1);
}

} // namespace cuexis::chart

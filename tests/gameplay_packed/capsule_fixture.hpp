#pragma once
#include "../judgement/execution_test_fixture.hpp"
#include "gameplay_test_fixture.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cuexis/gameplay_packed/gameplay_capsule.hpp>

namespace cuexis::gameplay_packed::testing {
using namespace judgement;
using namespace judgement::testing;
namespace capsule = cuexis::gameplay_packed;
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
        r.timing = RequirementRecord::Timing{Tick{100},
                                             {{Tick{10}, Tick{20}, {PhaseKind::head, 1}},
                                              {Tick{100}, Tick{103}, {PhaseKind::tail, 3}}},
                                             TimeInterval{Tick{20}, Tick{100}}};
        r.resourceClaims[0].claimPolicy.competition = ClaimPolicyDeclaration::CompetitionKey{-1, 7};
        request.assembly.sources[0].document.relations[0].policy.claimKeyToken.clear();
        chart.chartId.value = "019b0000-0000-7abc-8def-000000000001";
        chart.features = {{std::string{kFeatureId}, 1}, {std::string{kRulesetFeatureId}, 1}};
        chart.mainMusic = chart::AssetId{"music.one"};
        chart.resourceClosure.resources = {
            {*chart.mainMusic, chart::CanonicalResourceUseKind::MainMusic},
            {chart::AssetId{"mesh.one"}, chart::CanonicalResourceUseKind::RenderableMesh},
            {chart::AssetId{"material.one"}, chart::CanonicalResourceUseKind::RenderableMaterial}};
        std::sort(chart.resourceClosure.resources.begin(), chart.resourceClosure.resources.end());
        chart::CanonicalEntity entity;
        entity.identity = chart::ExplicitEntityIdentity{
            chart::ChartObjectId{"019b0000-0000-7abc-8def-000000000002"}};
        entity.components = {chart::CanonicalTransform{},
                             chart::CanonicalRenderable{chart::AssetId{"mesh.one"},
                                                        chart::AssetId{"material.one"}, 37}};
        chart.entities.push_back(entity);
        auto visual = entity;
        visual.identity = chart::ExplicitEntityIdentity{
            chart::ChartObjectId{"019b0000-0000-7abc-8def-000000000003"}};
        visual.parent = entity.identity;
        chart.entities.push_back(visual);
        owners.push_back({r.identity, entity.identity});
    }
    auto prepare() const -> PreparedGameplay {
        auto result = prepareResolvedGameplay(request);
        INFO((result ? ""
                     : std::string{result.error().code()} + ": " +
                           std::string{result.error().message()}));
        REQUIRE(result);
        return *result;
    }
    auto context() const -> capsule::DecodeContext {
        return {request.assembly.capabilityContext, request.assembly.contentProfileLimits,
                request.patternBudget, request.assembly.identityDeclarations,
                profiles.coordinatorPolicyToken};
    }
    auto encode(const PreparedGameplay& prepared) const -> Bytes {
        const auto result = capsule::encode({chart, prepared, owners, profiles});
        INFO((result ? ""
                     : std::string{result.error().code()} + ": " +
                           std::string{result.error().message()}));
        REQUIRE(result);
        return *result;
    }
};

} // namespace cuexis::gameplay_packed::testing

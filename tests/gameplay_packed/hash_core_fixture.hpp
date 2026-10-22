#pragma once
#include "capsule_fixture.hpp"
#include "semantic_stream_internal.hpp"

namespace cuexis::gameplay_packed::testing {
// Test-owned canonical input, constructed before the allocation observation window.
// This fixture is not an encode-request-to-hash adapter in the implementation.
struct HashCoreFixture final {
    CapsuleFixture source;
    std::optional<PreparedCapsule> decoded;
    judgement::CanonicalGameplayGraph graph;
    std::vector<chart::CanonicalEntityIdentity> entities, owners;
    std::vector<detail::RequirementIndices> indices;
    std::vector<detail::ClaimRow> claims;
    std::vector<std::uint32_t> relationResources;
    std::uint32_t revision;
    explicit HashCoreFixture(std::uint32_t value = 3) : revision{value} {
        if (revision == 3) {
            source.base.latePolicy = judgement::testing::executionLate();
            judgement::testing::executionFields(source.request.assembly);
            source.profiles = {source.request.assembly.normalizationProfileToken,
                               source.request.assembly.coordinatorPolicyToken};
        }
        const auto prepared = source.prepare();
        const auto bytes =
            encode({source.chart, prepared, source.owners, source.profiles, {}, {}, revision});
        REQUIRE(bytes);
        auto context = source.context();
        context.candidateRevision = revision;
        auto result = decode(*bytes, context);
        REQUIRE(result);
        decoded.emplace(std::move(*result));
        graph = decoded->gameplay.assembled().graph;
        REQUIRE(graph.requirements.size() == 1);
        REQUIRE(decoded->patterns.size() == 1);
        REQUIRE(decoded->measures.size() == 1);
        for (const auto& entity : decoded->chart.entities)
            entities.push_back(entity.identity);
        for (const auto& owner : decoded->owners)
            owners.push_back(owner.entity);
        indices.push_back({0, 0, 0, 0, 0, {0}, {0}});
        claims.push_back({0, 0, graph.requirements[0].resourceClaims[0]});
        relationResources.assign(graph.relations.size(), 0);
    }
    auto input() const -> detail::SemanticInput {
        return {.chart = decoded->chart,
                .graph = graph,
                .entities = entities,
                .owners = owners,
                .indices = indices,
                .patterns = decoded->patterns,
                .measures = decoded->measures,
                .claims = claims,
                .profiles = decoded->profiles,
                .timebase = *graph.timebase,
                .latePolicy = *graph.latePolicy,
                .profileId = graph.timebase->profileId,
                .unit = graph.timebase->unitToken,
                .candidateRevision = revision,
                .relationResources = relationResources};
    }
    auto legacyBytes() const -> std::vector<std::byte> {
        const auto result =
            semanticPreimage({decoded->chart, decoded->gameplay, decoded->owners, decoded->profiles,
                              decoded->patterns, decoded->measures, revision});
        REQUIRE(result);
        return *result;
    }
};
} // namespace cuexis::gameplay_packed::testing

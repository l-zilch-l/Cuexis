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
    explicit HashCoreFixture(std::uint32_t value = 3, std::size_t count = 1, bool mixed = false)
        : revision{value} {
        if (revision == 3) {
            source.base.latePolicy = judgement::testing::executionLate();
            judgement::testing::executionFields(source.request.assembly);
            source.profiles = {source.request.assembly.normalizationProfileToken,
                               source.request.assembly.coordinatorPolicyToken};
        }
        if (count != 1 || mixed) {
            auto& doc = source.request.assembly.sources[0].document;
            const auto prototype = doc.requirements[0];
            doc.requirements.clear();
            source.owners.clear();
            std::erase_if(doc.declarations,
                          [](const auto& d) { return d.kind == DeclarationKind::requirement; });
            doc.relations[0].members.clear();
            for (std::size_t i = 0; i < count; ++i) {
                auto row = prototype;
                row.stableId.declarationOrdinal = static_cast<std::uint32_t>(4 + i);
                row.identity.requirementLocalId = std::string(i % 5 + 1, 'r') + std::to_string(i);
                row.identity.emissionPath[0].repeatIndex = i;
                auto& claim = row.resourceClaims[0];
                claim.claimPolicy.competition = ClaimPolicyDeclaration::CompetitionKey{-1, i};
                if (mixed && i % 3 == 1)
                    claim.intent = ResourceClaimIntent::consume;
                if (mixed && i % 3 == 2) {
                    claim.intent = ResourceClaimIntent::observe;
                    row.phases = {{PhaseKind::tap, 1}};
                    row.requiresReleaseTailSemantics = false;
                    row.timing = RequirementRecord::Timing{
                        Tick{940}, {{Tick{850}, Tick{920}, {PhaseKind::tap, 1}}}, {}, {}};
                    if (revision == 3) {
                        row.timing->phaseTargets = {{PhaseKind::tap, Tick{900}}};
                        row.atomBindings.resize(1);
                    }
                    row.measure.components = {{PhaseKind::tap, "tap", {}}};
                    row.preparedGrace = PreparedGrace{TickSpan{0}};
                    claim.claimPolicy.claimKeyToken.clear();
                    claim.claimPolicy.competition.reset();
                }
                doc.declarations.push_back(testDeclaration(DeclarationKind::requirement,
                                                           row.identity.requirementLocalId,
                                                           row.stableId.declarationOrdinal));
                doc.relations[0].members.push_back(row.stableId);
                source.owners.push_back({row.identity, source.chart.entities[0].identity});
                doc.requirements.push_back(std::move(row));
            }
            if (count == 0)
                doc.relations.clear();
        }
        const auto prepared = source.prepare();
        if (count == 0) {
            source.chart.features.clear();
            for (const auto& feature : prepared.assembled().graph.derivedFeatures.features)
                source.chart.features.push_back({feature.featureId, 1});
        }
        const auto bytes =
            encode({source.chart, prepared, source.owners, source.profiles, {}, {}, revision});
        INFO((bytes ? ""
                    : std::string{bytes.error().code()} + ": " +
                          std::string{bytes.error().message()}));
        REQUIRE(bytes);
        auto context = source.context();
        context.candidateRevision = revision;
        auto result = decode(*bytes, context);
        REQUIRE(result);
        decoded.emplace(std::move(*result));
        graph = decoded->gameplay.assembled().graph;
        REQUIRE(graph.requirements.size() == count);
        REQUIRE(decoded->patterns.size() == (count ? 1 : 0));
        REQUIRE(decoded->measures.size() == (count ? (mixed && count > 2 ? 2 : 1) : 0));
        for (const auto& entity : decoded->chart.entities)
            entities.push_back(entity.identity);
        for (const auto& owner : decoded->owners)
            if (std::find(owners.begin(), owners.end(), owner.entity) == owners.end())
                owners.push_back(owner.entity);
        for (std::size_t i = 0; i < count; ++i) {
            const auto index = static_cast<std::uint32_t>(i);
            const auto measure = std::find_if(
                decoded->measures.begin(), decoded->measures.end(),
                [&](const auto& m) { return m.declaration == graph.requirements[i].measure; });
            REQUIRE(measure != decoded->measures.end());
            indices.push_back({0,
                               0,
                               static_cast<std::uint32_t>(measure - decoded->measures.begin()),
                               0,
                               0,
                               {index},
                               {0}});
            claims.push_back({index, 0, graph.requirements[i].resourceClaims[0]});
        }
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

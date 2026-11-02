#pragma once

// Requirement, resource, relation and claim rows plus the table skeleton.

#include "gameplay_capsule_model_internal.hpp"

namespace cuexis::gameplay_packed {
namespace capsule_detail {

template <typename A> constexpr auto defersGraphLinks() -> bool {
    if constexpr (requires { A::deferLinks; })
        return A::deferLinks;
    return false;
}
template <typename A, typename V, typename M> void relationRow(A& a, V& v, M& m) {
    require(v.kind == RelationKind::exclusive, "relations.kind", "unsupported relation kind");
    constant(a, 1);
    std::uint32_t resource{};
    if constexpr (!A::reading) {
        if constexpr (std::is_same_v<std::remove_cv_t<M>, detail::SemanticInput>) {
            const auto ordinal = static_cast<std::size_t>(&v - m.graph.relations.data());
            resource = m.relationResources.empty() ? a.operation->relationResources[ordinal]
                                                   : m.relationResources[ordinal];
            require(resource < m.graph.resources.size(), "relations.resource", "dangling resource");
        } else {
            const auto it =
                std::find_if(m.graph.resources.begin(), m.graph.resources.end(),
                             [&v](const auto& row) { return row.ref == v.resourceRef; });
            require(it != m.graph.resources.end(), "relations.resource", "dangling resource");
            resource = u32(static_cast<std::size_t>(it - m.graph.resources.begin()));
        }
    }
    index(a, resource, m.graph.resources, [](auto& out, auto& r) { out.value(r.ref.resourceId); });
    if constexpr (A::reading) {
        if constexpr (defersGraphLinks<A>()) {
            m.deferredRelationResources.push_back(resource);
        } else {
            require(resource < m.graph.resources.size(), "relations.resource",
                    "resource index out of range");
            v.resourceRef = m.graph.resources[resource].ref;
        }
    }
    require(v.policy.claimKeyToken.empty() && !v.policy.competition, "relations.policy",
            "relation cannot carry requirement-side namespace or competition");
    a.fields(v.members, v.policy.policyToken, v.declaredCapacity, v.required);
}
template <typename A, typename V, typename M> void claimRow(A& a, V& row, M& m) {
    std::optional<HashCoreWork::EmissionScope> emissionScope;
    if constexpr (std::is_same_v<A, Output>)
        if (a.operation)
            emissionScope.emplace(*a.operation, row.requirement);
    auto& v = row.declaration;
    index(a, row.requirement, m.graph.requirements,
          [](auto& out, auto& r) { out.value(r.identity); });
    index(a, row.resource, m.graph.resources,
          [](auto& out, auto& r) { out.value(r.ref.resourceId); });
    if constexpr (!defersGraphLinks<A>()) {
        require(row.requirement < m.graph.requirements.size() &&
                    row.resource < m.graph.resources.size(),
                "claims", "dangling claim owner or resource");
        if constexpr (A::reading)
            v.resourceRef = m.graph.resources[row.resource].ref;
    }
    enumeration(a, v.intent);
    a.value(v.claimPolicy.policyToken);
    std::optional<std::conditional_t<A::reading, std::string, std::string_view>> ns;
    if constexpr (!A::reading) {
        if (!v.claimPolicy.claimKeyToken.empty()) {
            ns = v.claimPolicy.claimKeyToken;
        }
    }
    a.value(ns);
    bool hasKey = v.intent != ResourceClaimIntent::observe;
    if (storesProjection(a, m))
        a.value(hasKey);
    require(hasKey == (v.intent != ResourceClaimIntent::observe), "claims.claimKey",
            "claim-key presence disagrees with intent");
    require(ns.has_value() == hasKey && (!ns || !ns->empty()), "claims.namespace",
            "namespace presence disagrees with intent");
    if (hasKey && storesProjection(a, m)) {
        if constexpr (std::is_same_v<A, Output>) {
            if (a.mode == Mode::hash) {
                projectedClaim(a, *ns, m.graph.requirements[row.requirement].identity);
            } else {
                const auto expected = judgement::detail::structuralClaimKey(
                    *ns, m.graph.requirements[row.requirement].identity);
                a.ref(11, expected);
            }
        } else if constexpr (defersGraphLinks<A>()) {
            std::string key;
            a.ref(11, key);
            row.deferredClaimKey = std::move(key);
        } else {
            const auto expected = judgement::detail::structuralClaimKey(
                *ns, m.graph.requirements[row.requirement].identity);
            auto actual = expected;
            a.ref(11, actual);
            require(actual == expected, "claims.claimKey",
                    "claim key does not match namespace and E");
        }
    }
    a.value(v.claimPolicy.competition);
    require(v.claimPolicy.competition.has_value() == hasKey, "claims.competition",
            "competition presence disagrees with intent");
    std::optional<std::uint32_t> slot;
    if constexpr (!A::reading) {
        if (hasKey) {
            slot = 0;
        }
    }
    a.value(slot);
    require(slot.has_value() == hasKey && (!slot || *slot == 0), "claims.slot",
            "capacity-one claim must use slot zero");
    constant(a, 0);
    require(v.graceOverride.mode == GraceOverrideMode::none &&
                v.graceOverride.overrideToken.empty(),
            "claims.graceOverride", "unsupported grace override");
    if constexpr (A::reading) {
        v.claimPolicy.claimKeyToken = ns.value_or("");
    }
}
template <typename A, typename V>
void visitFields(A& a, V& v, std::type_identity<RequirementRecord::SuccessWindow>) {
    a.fields(v.phase, v.start, v.end);
}
template <typename A, typename V>
void visitFields(A& a, V& v, std::type_identity<RequirementRecord::Timing>) {
    a.fields(v.end, v.successWindows, v.body);
}
template <typename A, typename V> void executionFields(A& a, V& v) {
    require(v.timing.has_value(), "requirements.timing", "revision 3 requires timing");
    a.fields(v.timing->phaseTargets, v.atomBindings, v.independentCompetition);
}
template <typename A, typename V, typename I, typename M>
void requirementRow(A& a, V& v, I& ix, M& m) {
    if constexpr (A::reading) {
        a.value(ix.entity);
    } else if (a.mode == Mode::hash) {
        require(ix.entity < m.entities.size(), "requirements.entity", "dangling entity");
        if constexpr (std::is_same_v<A, Output>) {
            if (a.operation) {
                a.operation->visitEdge(a);
                if (a.operation->counting) {
                    a.operation->combine(a, a.operation->group(CostGroup::entity)[ix.entity].cost);
                } else {
                    const auto size = checked(chart::packed::identity_detail::canonicalIdentitySize(
                        m.entities[ix.entity]));
                    a.count(size);
                    checked(chart::packed::identity_detail::streamCanonicalIdentity(
                        m.entities[ix.entity], {&a, [](void* context, auto data) {
                                                    static_cast<Output*>(context)->raw(data);
                                                }}));
                }
            } else {
                if (a.cost)
                    ++a.cost->visits;
                const auto size = checked(
                    chart::packed::identity_detail::canonicalIdentitySize(m.entities[ix.entity]));
                a.count(size);
                checked(chart::packed::identity_detail::streamCanonicalIdentity(
                    m.entities[ix.entity], {&a, [](void* context, auto data) {
                                                static_cast<Output*>(context)->raw(data);
                                            }}));
            }
        } else {
            const auto bytes = checked(
                chart::packed::identity_detail::canonicalIdentityBytes(m.entities[ix.entity]));
            a.count(bytes.size());
            a.raw(bytes);
        }
    } else {
        a.value(ix.entity);
    }
    a.fields(v.stableId, v.identity);
    if (storesProjection(a, m)) {
        projected(a, 12, 0, {}, &v.identity);
        projected(a, 13, 1, {}, &v.identity);
    }
    if constexpr (A::reading) {
        const auto n = a.count();
        for (std::size_t i = 0; i < n; ++i) {
            std::string token;
            a.ref(5, token);
            v.requiredActions.push_back(checked(RequiredActionRef::fromToken(std::move(token))));
        }
        std::string token;
        a.value(token);
        v.domainBinding = checked(DomainBindingRef::fromToken(std::move(token)));
    } else {
        a.count(v.requiredActions.size());
        for (const auto& action : v.requiredActions) {
            a.ref(5, action.token());
        }
        a.text(v.domainBinding.token());
    }
    index(a, ix.domain, m.graph.judgementDomains, [](auto& out, auto& d) { domainRow(out, d); });
    a.fields(v.phases, v.requiresReleaseTailSemantics);
    index(a, ix.pattern, m.patterns, [](auto& out, auto& p) { patternRow(out, p); });
    measured(a, v.maxArmElements);
    measured(a, v.maxDeadlineElements);
    a.value(v.patternArmRefs);
    index(a, ix.measure, m.measures, [](auto& out, auto& w) { measureRow(out, w); });
    if constexpr (A::reading) {
        a.value(ix.claims);
    } else {
        a.count(ix.claims.size());
        for (auto& i : ix.claims) {
            index(a, i, m.claims, [&m](auto& out, auto& c) { claimRow(out, c, m); });
        }
    }
    a.fields(v.preparedGrace, v.localClosePolicyToken);
    index(a, ix.solver, m.graph.solverProfiles, [](auto& out, auto& s) { solverRow(out, s); });
    if constexpr (A::reading) {
        a.value(ix.bindings);
    } else {
        a.count(ix.bindings.size());
        for (auto& i : ix.bindings) {
            index(a, i, m.graph.factBindings, [](auto& out, auto& b) { bindingRow(out, b); });
        }
    }
    a.fields(v.required, v.timing);
    if constexpr (!A::reading) {
        if (a.mode == Mode::hash) {
            if (executionRevision(m.candidateRevision)) {
                executionFields(a, v);
            }
            return;
        }
    }
    enumeration(a, v.grace.policy);
    std::optional<std::string> inherited;
    if constexpr (!A::reading) {
        if (!v.grace.inheritedFromDeclarationId.empty()) {
            inherited = v.grace.inheritedFromDeclarationId;
        }
    }
    a.fields(inherited, v.grace.allowChartGrace);
    require(inherited.has_value() ==
                    (v.grace.policy == GraceResolutionPolicy::inheritedDeclaration) &&
                (!inherited || !inherited->empty()),
            "requirements.graceProvenance", "invalid inherited provenance");
    if constexpr (A::reading) {
        v.grace.inheritedFromDeclarationId = inherited.value_or("");
    }
    if (executionRevision(m.candidateRevision)) {
        executionFields(a, v);
    }
}
template <typename A, typename M> void globalRow(A& a, M& m) {
    auto& g = m.graph;
    a.fields(g.gameplayVersion, g.graphRevision, g.rulesetRef);
    require(g.gameplayVersion == 2, "GPH0.gameplayVersion", "unsupported gameplay version");
    projected(a, 13, 8, {g.rulesetRef});
    a.value(m.profiles.normalizationProfileToken);
    projected(a, 13, 4, {m.profiles.normalizationProfileToken});
    a.value(m.profiles.coordinatorPolicyToken);
    projected(a, 13, 6, {m.profiles.coordinatorPolicyToken});
    if constexpr (A::reading) {
        a.value(m.resolver);
    } else if (a.mode != Mode::hash) {
        a.value(m.resolver);
    }
    a.value(m.close);
    // Projection precedes the timebase it identifies; decode defers the equality check until the ID
    // exists.
    if constexpr (A::reading) {
        std::string key;
        a.ref(13, key);
        timebase(a, m.timebase, m.profileId, m.unit);
        require(key == projection(2, {m.profileId}), "GPH0.timebaseProjection",
                "wrong timebase projection");
    } else {
        projected(a, 13, 2, {m.profileId});
        timebase(a, m.timebase, m.profileId, m.unit);
    }
    projected(a, 13, 3, {m.profileId});
    late(a, m.latePolicy);
    a.fields(g.declaredFeatures.features, g.declaredCapabilities.capabilities,
             g.closureContributions.features, g.closureContributions.capabilities,
             g.derivedFeatures.features, g.derivedCapabilities.capabilities,
             g.resourceClosure.resources);
    if constexpr (A::reading) {
        a.fields(g.sourceClosure.sourceDocumentIds, g.sourceClosure.compilerProfileToken);
        for (auto& n : m.counts) {
            a.value(n);
        }
    } else if (a.mode != Mode::hash) {
        a.fields(g.sourceClosure.sourceDocumentIds, g.sourceClosure.compilerProfileToken);
        for (auto& n : m.counts) {
            a.value(n);
        }
    }
    if (executionRevision(m.candidateRevision)) {
        a.value(g.executionProfile);
    }
}
template <typename A, typename Rows, typename F> void table(A& a, Rows& rows, F visitRow) {
    if constexpr (A::reading) {
        const auto n = a.count();
        rows.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            typename Rows::value_type row{};
            visitRow(a, row);
            rows.push_back(std::move(row));
        }
    } else {
        a.count(rows.size());
        for (std::size_t i = 0; i < rows.size(); ++i) {
            if constexpr (std::is_same_v<A, Output>) {
                if (a.operation && a.operation->counting) {
                    using T = std::remove_cvref_t<decltype(rows[i])>;
                    const auto cost = a.operation->resolve(
                        costGroup<T>(), i, [&](Output& child) { visitRow(child, rows[i]); });
                    a.operation->combine(a, cost);
                    continue;
                }
            }
            visitRow(a, rows[i]);
        }
    }
}
template <typename A, typename M> void definitions(A& a, M& m) {
    table(a, m.patterns, [](auto& out, auto& p) { patternRow(out, p); });
    table(a, m.measures, [](auto& out, auto& w) { measureRow(out, w); });
    table(a, m.graph.judgementDomains, [](auto& out, auto& h) { domainRow(out, h); });
    table(a, m.graph.mergedNamespace.declarations, [](auto& out, auto& k) { mergedRow(out, k); });
}
template <typename A, typename M> void coordination(A& a, M& m) {
    table(a, m.graph.resources, [](auto& out, auto& t) { resourceRow(out, t); });
    table(a, m.graph.relations, [&m](auto& out, auto& f) { relationRow(out, f, m); });
    table(a, m.graph.solverProfiles, [](auto& out, auto& y) { solverRow(out, y); });
    table(a, m.graph.factBindings, [](auto& out, auto& b) { bindingRow(out, b); });
    table(a, m.claims, [&m](auto& out, auto& x) { claimRow(out, x, m); });
}
template <typename A, typename T> void visit(A& a, T& v) {
    visitFields(a, v, std::type_identity<std::remove_cv_t<T>>{});
}

} // namespace capsule_detail
} // namespace cuexis::gameplay_packed

#include "gameplay_capsule_operations_internal.hpp"

// Capsule model construction and the finite fidelity closure.

namespace cuexis::gameplay_packed {
namespace capsule_detail {

void validate(Model& m, chart::PackedChartLimits limits,
              const PatternCompileBudget& patternBudget) {
    auto& g = m.graph;
    m.bind();
    require(m.resolver == kResolver && m.close == kClose, "GPH0.policy",
            "unsupported resolver or close algorithm");
    require(!m.profiles.normalizationProfileToken.empty() &&
                !m.profiles.coordinatorPolicyToken.empty(),
            "GPH0.profiles", "explicit normalization and coordinator profiles are required");
    bound(g.requirements.size(), limits.maxPackedRequirements, "packed.budget.requirements");
    require(m.indices.size() == g.requirements.size(), "GPR0", "requirement-index count mismatch");
    ordered(
        g.requirements, [](const auto& l, const auto& r) { return l.identity < r.identity; },
        "GPR0");
    ordered(m.patterns, patternLess, "GPD0.patterns");
    ordered(m.measures, measureLess, "GPD0.measures");
    ordered(
        g.resources, [](const auto& l, const auto& r) { return l.ref < r.ref; }, "GRC0.resources");
    ordered(
        g.relations,
        [](const auto& l, const auto& r) {
            return std::tie(l.resourceRef, l.kind, l.members, l.policy.policyToken) <
                   std::tie(r.resourceRef, r.kind, r.members, r.policy.policyToken);
        },
        "GRC0.relations");
    ordered(
        g.solverProfiles,
        [](const auto& l, const auto& r) {
            return std::tie(l.solverId, l.revision) < std::tie(r.solverId, r.revision);
        },
        "GRC0.solverProfiles");
    ordered(g.factBindings, "GRC0.factBindings");
    ordered(
        g.judgementDomains,
        [](const auto& l, const auto& r) { return textLess(l.domainId, r.domainId); },
        "GPD0.domains");
    ordered(
        g.mergedNamespace.declarations,
        [](const auto& l, const auto& r) { return l.stableId < r.stableId; },
        "GPD0.mergedDeclarations");
    ordered(g.sourceClosure.sourceDocumentIds, "GPH0.sourceClosure");
    refsValid({g.declaredFeatures.features, g.declaredCapabilities.capabilities});
    refsValid({g.derivedFeatures.features, g.derivedCapabilities.capabilities});
    refsValid({g.closureContributions.features, g.closureContributions.capabilities});
    ordered(g.resourceClosure.resources, "GPH0.resourceClosure");
    require(g.derivedFeatures == deriveFeatureClosure(g) &&
                g.derivedCapabilities == deriveCapabilityClosure(g) &&
                g.resourceClosure == deriveResourceClosure(g),
            "GPH0.closure", "derived closure does not match records");
    std::vector<std::string> features;
    for (const auto& f : m.chart.features) {
        features.push_back(f.id);
    }
    std::sort(features.begin(), features.end());
    std::vector<std::string> derived;
    for (const auto& f : g.derivedFeatures.features) {
        derived.push_back(f.featureId);
    }
    require(features == derived, "META.requiredFeatures",
            "META and Gameplay feature closures differ");
    // The model owns every name for this entire validation call. This local view index
    // also retains kind, so later definition checks need not scan the declaration table.
    std::map<std::string_view, DeclarationKind, std::less<>> names;
    std::set<std::string> patternNames, measureNames;
    const auto definitionNeeds = [&g](const RequiredRefs& required) {
        refsValid(required);
        for (const auto& feature : required.features) {
            require(std::binary_search(g.declaredFeatures.features.begin(),
                                       g.declaredFeatures.features.end(), feature) &&
                        std::binary_search(g.derivedFeatures.features.begin(),
                                           g.derivedFeatures.features.end(), feature),
                    "GPD0.required.features",
                    "definition feature is missing from the prepared closure");
        }
        for (const auto& capability : required.capabilities) {
            require(std::binary_search(g.declaredCapabilities.capabilities.begin(),
                                       g.declaredCapabilities.capabilities.end(), capability) &&
                        std::binary_search(g.derivedCapabilities.capabilities.begin(),
                                           g.derivedCapabilities.capabilities.end(), capability),
                    "GPD0.required.capabilities",
                    "definition capability is missing from the prepared closure");
        }
    };
    for (const auto& p : m.patterns) {
        definitionNeeds(p.required);
        if (!p.patternId.empty()) {
            require(patternNames.insert(p.patternId).second, "patterns.id",
                    "conflicting named Pattern");
        }
        checked(compilePattern(p, patternBudget));
    }
    for (const auto& w : m.measures) {
        definitionNeeds(w.declaration.required);
        ordered(
            w.declaration.components,
            [](const auto& l, const auto& r) {
                return std::tie(l.phase, l.categoryToken) < std::tie(r.phase, r.categoryToken);
            },
            "measures.components");
        if (w.id) {
            require(!w.id->empty() && measureNames.insert(*w.id).second, "measures.id",
                    "conflicting named Measure");
        }
        MeasurePhaseContext context{.declaredPhases = {},
                                    .requiresReleaseTailSemantics = false,
                                    .fieldPathPrefix = "GPD0.measures"};
        for (const auto& c : w.declaration.components) {
            context.declaredPhases.push_back({c.phase, 0});
        }
        checked(compileMeasure(w.declaration, context));
    }
    for (const auto& d : g.judgementDomains) {
        refsValid(d.required);
        ordered(
            d.axes, [](const auto& l, const auto& r) { return textLess(l.axisToken, r.axisToken); },
            "domains.axes");
        checked(validateJudgementDomain(d));
    }
    // Stable IDs need not have the same order as Requirement identities. Retain
    // all matches so malformed duplicate IDs keep the original existence check.
    std::multimap<StableDeclarationId, const RequirementRecord*> requirementsByDeclaration;
    for (const auto& r : g.requirements)
        requirementsByDeclaration.emplace(r.stableId, &r);
    // These typed tables have passed their ordering checks in this call.
    const auto containsTypedName = [](const auto& rows, std::string_view name, const auto& key,
                                      const auto& less) {
        if (rows.size() < 2)
            return std::any_of(rows.begin(), rows.end(),
                               [&](const auto& row) { return key(row) == name; });
        const auto found = std::lower_bound(
            rows.begin(), rows.end(), name,
            [&](const auto& row, std::string_view wanted) { return less(key(row), wanted); });
        return found != rows.end() && key(*found) == name;
    };
    for (const auto& d : g.mergedNamespace.declarations) {
        require(!d.name.empty() && names.emplace(d.name, d.kind).second, "mergedDeclarations.name",
                "conflicting declaration name");
        ordered(d.references, "mergedDeclarations.references");
        ordered(d.requiredFeatureIds, "mergedDeclarations.features");
        ordered(d.requiredCapabilityIds, "mergedDeclarations.capabilities");
        for (const auto& ref : d.references) {
            require(findDeclaration(ref, m) != nullptr, "mergedDeclarations.references",
                    "dangling declaration");
        }
        bool exists = false;
        switch (d.kind) {
        case DeclarationKind::requirement: {
            const auto [first, last] = requirementsByDeclaration.equal_range(d.stableId);
            exists = std::any_of(first, last, [&d, &g](const auto& entry) {
                return !g.executionProfile.empty() ||
                       entry.second->identity.requirementLocalId == d.name;
            });
            break;
        }
        case DeclarationKind::patternDefinition:
            exists = patternNames.contains(d.name);
            break;
        case DeclarationKind::measureDefinition:
            exists = measureNames.contains(d.name);
            break;
        case DeclarationKind::resourceRecord:
            exists = containsTypedName(
                g.resources, d.name,
                [](const auto& r) -> std::string_view { return r.ref.resourceId; },
                [](std::string_view l, std::string_view r) { return l < r; });
            break;
        case DeclarationKind::judgementDomain:
            exists = containsTypedName(
                g.judgementDomains, d.name,
                [](const auto& h) -> std::string_view { return h.domainId; }, textLess);
            break;
        case DeclarationKind::solverProfile:
            exists = containsTypedName(
                g.solverProfiles, d.name,
                [](const auto& s) -> std::string_view { return s.solverId; },
                [](std::string_view l, std::string_view r) { return l < r; });
            break;
        }
        require(exists, "mergedDeclarations.kind", "declaration does not locate its typed record");
    }
    for (const auto& p : patternNames) {
        const auto found = names.find(p);
        require(found != names.end() && found->second == DeclarationKind::patternDefinition,
                "patterns.id", "named Pattern is not declared");
    }
    for (const auto& w : measureNames) {
        const auto found = names.find(w);
        require(found != names.end() && found->second == DeclarationKind::measureDefinition,
                "measures.id", "named Measure is not declared");
    }
    ordered(
        m.claims,
        [&m](const auto& l, const auto& r) {
            return std::tie(m.graph.resources[l.resource].ref,
                            m.graph.requirements[l.requirement].identity) <
                   std::tie(m.graph.resources[r.resource].ref,
                            m.graph.requirements[r.requirement].identity);
        },
        "GRC0.claims");
    std::vector<unsigned> usedClaims(m.claims.size()), usedOwners(m.entities.size());
    for (std::size_t i = 0; i < g.requirements.size(); ++i) {
        auto& r = g.requirements[i];
        const auto& ix = m.indices[i];
        require(ix.entity < m.entities.size() && ix.domain < g.judgementDomains.size() &&
                    ix.pattern < m.patterns.size() && ix.measure < m.measures.size() &&
                    ix.solver < g.solverProfiles.size(),
                "GPR0.indices", "table index out of range");
        ++usedOwners[ix.entity];
        require(r.unsupportedForms.empty() && r.localClosePolicyToken == m.close,
                "GPR0.localClosePolicy", "unsupported Requirement representation or policy");
        refsValid(r.required);
        ordered(r.phases, "requirements.phases");
        ordered(r.patternArmRefs, "requirements.patternArmRefs");
        if (executionRevision(m.candidateRevision)) {
            ordered(
                r.atomBindings,
                [](const auto& l, const auto& rr) { return textLess(l.atomRef, rr.atomRef); },
                "requirements.atomBindings");
            require(r.timing.has_value(), "requirements.timing", "execution timing absent");
            ordered(
                r.timing->phaseTargets,
                [](const auto& l, const auto& rr) { return l.phase < rr.phase; },
                "requirements.phaseTargets");
        }
        ordered(
            r.requiredActions,
            [](const auto& l, const auto& rr) { return textLess(l.token(), rr.token()); },
            "requirements.requiredActions");
        ordered(r.factBindingRefs, "requirements.factBindings");
        if (r.timing) {
            ordered(
                r.timing->successWindows,
                [](const auto& l, const auto& rr) {
                    return std::tie(l.phase, l.start, l.end) < std::tie(rr.phase, rr.start, rr.end);
                },
                "requirements.timing.successWindows");
        }
        require(r.pattern == m.patterns[ix.pattern] &&
                    r.measure == m.measures[ix.measure].declaration &&
                    namedMeasure(r, m) == m.measures[ix.measure].id &&
                    r.judgementDomainId == g.judgementDomains[ix.domain].domainId &&
                    r.solverProfileRef == g.solverProfiles[ix.solver].solverId,
                "GPR0.indices", "referenced definition differs from Requirement");
        require(ix.claims.size() == r.resourceClaims.size() && ix.claims.size() <= 1 &&
                    ix.bindings.size() == r.factBindingRefs.size(),
                "GPR0.claims", "reference count mismatch or fanout exceeds one");
        for (std::size_t j = 0; j < ix.claims.size(); ++j) {
            const auto c = ix.claims[j];
            require(c < m.claims.size() && m.claims[c].requirement == i &&
                        m.claims[c].declaration == r.resourceClaims[j],
                    "GPR0.claims", "claim is not owned by Requirement");
            ++usedClaims[c];
        }
        for (std::size_t j = 0; j < ix.bindings.size(); ++j) {
            require(ix.bindings[j] < g.factBindings.size() &&
                        g.factBindings[ix.bindings[j]].bindingId == r.factBindingRefs[j],
                    "GPR0.factBindings", "binding index mismatch");
        }
    }
    require(std::all_of(usedClaims.begin(), usedClaims.end(), [](auto n) { return n == 1; }),
            "GRC0.claims", "orphaned or multiply-owned claim");
    if (m.candidateRevision == 4) {
        // n <= the frozen u32 requirement budget; these u64 products cannot overflow.
        const auto n = static_cast<std::uint64_t>(g.requirements.size());
        const auto occupying = static_cast<std::uint64_t>(
            std::count_if(m.claims.begin(), m.claims.end(), [](const auto& claim) {
                return claim.declaration.intent != ResourceClaimIntent::observe;
            }));
        require(2 * n + occupying <= 3 * n, "GRC0.logicalProjections",
                "Derived role fanout exceeds the subset");
    }
    for (const auto& t : g.resources) {
        refsValid(t.required);
    }
    for (const auto& f : g.relations) {
        refsValid(f.required);
        ordered(f.members, "relations.members");
    }
    for (const auto& s : g.solverProfiles) {
        refsValid(s.required);
    }
}
auto makeModel(const EncodeRequest& request, chart::PackedChartLimits limits) -> Model {
    Model m;
    m.chart = request.chart;
    m.graph = request.gameplay.assembled().graph;
    m.candidateRevision = request.candidateRevision;
    require(m.candidateRevision == 2 || executionRevision(m.candidateRevision), "candidateRevision",
            "unsupported Writer revision");
    if (m.candidateRevision == 2) {
        require(m.graph.executionProfile.empty() && m.graph.normalizationProfileToken.empty() &&
                    m.graph.coordinatorPolicyToken.empty(),
                "executionProfile", "revision 2 Writer cannot discard execution fields");
        for (const auto& r : m.graph.requirements) {
            require(r.atomBindings.empty() && !r.independentCompetition &&
                        (!r.timing || r.timing->phaseTargets.empty()),
                    "requirements", "revision 2 Writer cannot discard execution fields");
        }
    } else {
        require((m.graph.executionProfile == "gameplay.execution.t4-k4.v1" ||
                 m.graph.executionProfile == "gameplay.execution.t4-k4.rpa.v1") &&
                    m.graph.normalizationProfileToken ==
                        request.profiles.normalizationProfileToken &&
                    m.graph.coordinatorPolicyToken == request.profiles.coordinatorPolicyToken,
                "executionProfile", "revision 3 graph and Writer profiles must agree");
    }
    require(m.graph.timebase && m.graph.latePolicy, "GPH0.timebase", "missing prepared timebase");
    m.timebase = *m.graph.timebase;
    m.latePolicy = *m.graph.latePolicy;
    m.profileId = std::string{m.timebase.profileId};
    m.unit = std::string{m.timebase.unitToken};
    m.profiles = request.profiles;
    m.bind();
    for (const auto& entity : m.chart.entities) {
        m.entities.push_back(entity.identity);
    }
    std::sort(m.entities.begin(), m.entities.end(), [](const auto& l, const auto& r) {
        return chart::packed::identity_detail::ByteKeyLess{}(
            checked(chart::packed::identity_detail::canonicalIdentityBytes(l)),
            checked(chart::packed::identity_detail::canonicalIdentityBytes(r)));
    });
    require(request.owners.size() == m.graph.requirements.size(), "GPR0.entity",
            "explicit ownership must cover every Requirement exactly once");
    std::map<RequirementIdentity, const RequirementOwner*> ownersByRequirement;
    for (const auto& owner : request.owners) {
        require(ownersByRequirement.try_emplace(owner.requirement, &owner).second, "GPR0.entity",
                "duplicate ownership");
        const auto found = std::lower_bound(
            m.graph.requirements.begin(), m.graph.requirements.end(), owner.requirement,
            [](const auto& row, const auto& key) { return row.identity < key; });
        require(found != m.graph.requirements.end() && found->identity == owner.requirement,
                "GPR0.entity", "ownership names no Requirement");
    }
    m.patterns.assign(request.additionalPatterns.begin(), request.additionalPatterns.end());
    m.measures.assign(request.additionalMeasures.begin(), request.additionalMeasures.end());
    for (const auto& r : m.graph.requirements) {
        m.patterns.push_back(r.pattern);
        const auto id = namedMeasure(r, m);
        if (id) {
            require(std::any_of(m.measures.begin(), m.measures.end(),
                                [&id, &r](const auto& w) {
                                    return w.id == id && w.declaration == r.measure;
                                }),
                    "requirements.measure", "named Measure definition must be explicitly supplied");
        } else {
            m.measures.push_back({std::nullopt, r.measure});
        }
    }
    std::sort(m.patterns.begin(), m.patterns.end(), patternLess);
    m.patterns.erase(std::unique(m.patterns.begin(), m.patterns.end()), m.patterns.end());
    std::sort(m.measures.begin(), m.measures.end(), measureLess);
    m.measures.erase(std::unique(m.measures.begin(), m.measures.end()), m.measures.end());
    for (std::size_t i = 0; i < m.graph.requirements.size(); ++i) {
        const auto& r = m.graph.requirements[i];
        const auto foundOwner = ownersByRequirement.find(r.identity);
        require(foundOwner != ownersByRequirement.end(), "GPR0.entity", "missing ownership");
        const auto* owner = foundOwner->second;
        RequirementIndices ix;
        ix.entity = findIndex(
            m.entities, [&owner](const auto& e) { return e == owner->entity; },
            "requirements.entity");
        if (std::find(m.owners.begin(), m.owners.end(), owner->entity) == m.owners.end()) {
            m.owners.push_back(owner->entity);
        }
        ix.pattern = findSortedIndex(m.patterns, r.pattern, patternLess, "requirements.pattern");
        const auto measureId = namedMeasure(r, m);
        ix.measure = findIndex(
            m.measures,
            [&r, &measureId](const auto& w) {
                return w.id == measureId && w.declaration == r.measure;
            },
            "requirements.measure");
        ix.domain = findIndex(
            m.graph.judgementDomains,
            [&r](const auto& d) { return d.domainId == r.judgementDomainId; },
            "requirements.domain");
        ix.solver = findIndex(
            m.graph.solverProfiles,
            [&r](const auto& s) { return s.solverId == r.solverProfileRef; },
            "requirements.solver");
        for (const auto& c : r.resourceClaims) {
            const auto resource = findIndex(
                m.graph.resources, [&c](const auto& t) { return t.ref == c.resourceRef; },
                "claims.resource");
            m.claims.push_back({u32(i), resource, c});
        }
        for (const auto& b : r.factBindingRefs) {
            ix.bindings.push_back(findIndex(
                m.graph.factBindings, [&b](const auto& s) { return s.bindingId == b; },
                "requirements.factBinding"));
        }
        m.indices.push_back(std::move(ix));
    }
    std::sort(m.claims.begin(), m.claims.end(), [&m](const auto& l, const auto& r) {
        return std::tie(m.graph.resources[l.resource].ref,
                        m.graph.requirements[l.requirement].identity) <
               std::tie(m.graph.resources[r.resource].ref,
                        m.graph.requirements[r.requirement].identity);
    });
    for (std::size_t i = 0; i < m.claims.size(); ++i) {
        m.indices[m.claims[i].requirement].claims.push_back(u32(i));
    }
    validate(m, limits);
    return m;
}

} // namespace capsule_detail
} // namespace cuexis::gameplay_packed

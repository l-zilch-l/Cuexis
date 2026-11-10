// Gameplay assembler assembly: the identity factory and the atomic assembly entry points.

#include "gameplay_assembler_internal.hpp"

namespace cuexis::judgement {

using namespace assembler_detail;

namespace {
void writePreparedPrefix(IdentityByteWriter& writer,
                         const PreparedIdentityDeclarations& declarations) {
    writer.beginComponent("engine");
    writer.writeText(declarations.engine.judgementSemanticRevision);
    writer.writeText(declarations.engine.factSemanticRevision);
    writer.writeText(declarations.engine.fixedPointTableId);
    writer.writeText(declarations.engine.coordinationPhaseOrderToken);
    if (declarations.engine.executionProfileToken || declarations.engine.lateAlgorithmToken) {
        writer.writeText("execution.engine.v1");
        writer.writeBool(declarations.engine.executionProfileToken.has_value());
        if (declarations.engine.executionProfileToken) {
            writer.writeText(*declarations.engine.executionProfileToken);
        }
        writer.writeBool(declarations.engine.lateAlgorithmToken.has_value());
        if (declarations.engine.lateAlgorithmToken) {
            writer.writeText(*declarations.engine.lateAlgorithmToken);
        }
    }
    //  The snapshot state schema revision has no field here on purpose: a normalized snapshot
    //  detail must not be able to change the semantic judgement identity.
    writer.beginComponent("ruleset");
    writer.writeText(declarations.ruleset.interfaceProjectionToken);
    writer.writeCount(declarations.ruleset.moduleOrder.size());
    for (const auto& module : declarations.ruleset.moduleOrder) {
        writer.writeText(module);
    }
    writer.writeText(declarations.ruleset.buildHash);
    writer.beginComponent("chart");
}

template <typename Writer>
void writePreparedSession(Writer& writer, const PreparedIdentityDeclarations& declarations) {
    writer.beginComponent("session");
    //  The discrete-input normalization profile and the default grace source belong to the session
    //  component, where Spec 5.2 puts them.
    writer.writeText(declarations.session.loadoutToken);
    writer.writeText(declarations.session.defaultGraceSourceToken);
    writer.writeText(declarations.session.normalizationProfileToken);
    writer.writeText(declarations.session.judgementConfigToken);
}

void writeExecutionSession(IdentityByteWriter& writer, const InputMappingProfile& mapping,
                           const LatePolicyParameters& late, std::string_view calibration) {
    writer.writeText("cuexis.judgement.identity.session.execution.v1");
    writer.writeText(mapping.profileId);
    writer.writeText(mapping.profileVersion);
    writer.writeText(mapping.sourceClass.token());
    auto domains = mapping.domains;
    std::sort(domains.begin(), domains.end(),
              [](const auto& a, const auto& b) { return a.domainToken < b.domainToken; });
    writer.writeCount(domains.size());
    for (const auto& d : domains) {
        writer.writeText(d.domainToken);
        writer.writeSigned(d.amount.scale.numerator());
        writer.writeSigned(d.amount.scale.denominator());
        writer.writeSigned(d.amount.minimum);
        writer.writeSigned(d.amount.maximum);
        writer.writeText(d.amount.boundaryPolicy == AmountBoundaryPolicy::inclusive ? "inclusive"
                                                                                    : "exclusive");
    }
    for (const auto& p : {late.finalizationWatermark, late.maxQueueHop, late.windowCloseThreshold,
                          late.windowOpenThreshold}) {
        writer.writeBool(p.isMeasured());
        if (p.isMeasured()) {
            writer.writeSigned(p.measuredValue()->value());
        }
    }
    writer.writeBool(late.policy.has_value());
    if (late.policy) {
        writer.writeText(*late.policy == LateEventPolicy::rejectLate ? "reject_late"
                                                                     : "queue_next_tick");
    }
    writer.writeText(calibration);
}

auto chartProjection(const ChartIdentity& chart) -> std::optional<std::span<const std::byte>> {
    IdentityByteWriter header{"cuexis.judgement.identity.chart.v1"};
    header.beginComponent("chart");
    const auto prefix = header.take();
    const auto& bytes = chart.canonicalBytes().bytes();
    if (bytes.size() < prefix.size() || !std::equal(prefix.begin(), prefix.end(), bytes.begin()))
        return std::nullopt;
    return std::span<const std::byte>{bytes}.subspan(prefix.size());
}

auto preparedFromProjection(std::span<const std::byte> projection,
                            const PreparedIdentityDeclarations& declarations,
                            std::span<const std::byte> execution = {}) -> std::vector<std::byte> {
    IdentityByteWriter writer{"cuexis.judgement.identity.prepared.v1"};
    writePreparedPrefix(writer, declarations);
    IdentitySizeWriter suffix;
    writePreparedSession(suffix, declarations);
    // Reserve the tail before the large append to avoid a second full-buffer allocation.
    if (execution.size() > std::numeric_limits<std::size_t>::max() - suffix.size() ||
        projection.size() >
            std::numeric_limits<std::size_t>::max() - suffix.size() - execution.size())
        throw std::length_error{"Identity size overflow"};
    writer.reserveAdditional(projection.size() + suffix.size() + execution.size());
    writer.append(projection);
    writePreparedSession(writer, declarations);
    writer.append(execution);
    return writer.take();
}
} // namespace

auto makePreparedIdentity(const CanonicalGameplayGraph& graph,
                          const PreparedIdentityDeclarations& declarations) -> PreparedIdentity {
    IdentityByteWriter writer{"cuexis.judgement.identity.prepared.v1"};
    writePreparedPrefix(writer, declarations);
    writeChartProjection(writer, graph);
    writePreparedSession(writer, declarations);
    return PreparedIdentity{CanonicalIdentityBytes{writer.take()}};
}

auto makeRuntimePreparedIdentity(const CanonicalGameplayGraph& graph,
                                 const PreparedIdentityDeclarations& declarations,
                                 const InputMappingProfile& mapping,
                                 const LatePolicyParameters& late, std::string_view calibration)
    -> PreparedIdentity {
    IdentityByteWriter writer{"cuexis.judgement.identity.prepared.v1"};
    writePreparedPrefix(writer, declarations);
    writeChartProjection(writer, graph);
    writePreparedSession(writer, declarations);
    writeExecutionSession(writer, mapping, late, calibration);
    return PreparedIdentity{CanonicalIdentityBytes{writer.take()}};
}

auto detail::IdentityFactory::assemble(const CanonicalGameplayGraph& graph,
                                       const PreparedIdentityDeclarations& declarations)
    -> std::tuple<ChartIdentity, ContentIdentity, PreparedIdentity> {
    auto chart = makeChartIdentity(graph);
    const auto projection = chartProjection(chart);
    if (!projection)
        return {std::move(chart), makeContentIdentity(graph),
                makePreparedIdentity(graph, declarations)};
    IdentityByteWriter content{"cuexis.judgement.identity.content.v1"};
    content.beginComponent("chart");
    IdentitySizeWriter suffix;
    suffix.beginComponent("provenance");
    writeContentProvenance(suffix, graph);
    if (projection->size() > std::numeric_limits<std::size_t>::max() - suffix.size())
        throw std::length_error{"Identity size overflow"};
    content.reserveAdditional(projection->size() + suffix.size());
    content.append(*projection);
    content.beginComponent("provenance");
    writeContentProvenance(content, graph);
    auto contentIdentity = ContentIdentity{CanonicalIdentityBytes{content.take()}};
    auto preparedIdentity =
        PreparedIdentity{CanonicalIdentityBytes{preparedFromProjection(*projection, declarations)}};
    return {std::move(chart), std::move(contentIdentity), std::move(preparedIdentity)};
}

auto detail::IdentityFactory::runtime(const PreparedGameplay& prepared,
                                      const PreparedIdentityDeclarations& declarations,
                                      const InputMappingProfile& mapping,
                                      const LatePolicyParameters& late,
                                      std::string_view calibration) -> PreparedIdentity {
    const auto projection = chartProjection(prepared.assembled().chart);
    if (!projection)
        return makeRuntimePreparedIdentity(prepared.assembled().graph, declarations, mapping, late,
                                           calibration);
    IdentityByteWriter suffix;
    writeExecutionSession(suffix, mapping, late, calibration);
    const auto execution = suffix.take();
    return PreparedIdentity{
        CanonicalIdentityBytes{preparedFromProjection(*projection, declarations, execution)}};
}

//  Counts every content-profile dimension of one canonical graph. The pattern dimensions are
//  measured by an iterative walk: a recursive counter would need a compiled-in depth bound to keep
//  the host stack safe, and no such bound is frozen, so this function is not `noexcept` -- the walk
//  allocates its own traversal stack.
auto countContentProfile(const CanonicalGameplayGraph& graph) -> ContentProfileCounts {
    ContentProfileCounts counts{};
    counts.requirements = graph.requirements.size();
    counts.resources = graph.resources.size();
    counts.solverProfiles = graph.solverProfiles.size();
    counts.factBindings = graph.factBindings.size();
    counts.judgementDomains = graph.judgementDomains.size();
    counts.mergedDeclarations = graph.mergedNamespace.declarations.size();
    counts.declaredCapabilities = graph.declaredCapabilities.capabilities.size();
    counts.derivedCapabilities = graph.derivedCapabilities.capabilities.size();
    counts.declaredFeatures = graph.declaredFeatures.features.size();
    counts.derivedFeatures = graph.derivedFeatures.features.size();
    counts.diagnosticMapEntries = graph.diagnosticMap.entries.size();
    for (const auto& relation : graph.relations) {
        if (relation.kind == RelationKind::exclusive) {
            ++counts.exclusiveRelations;
        }
        counts.relationMembers += relation.members.size();
    }
    for (const auto& requirement : graph.requirements) {
        //  Every requirement record is one emission of the CXT expansion. A Chart v5 inline
        //  declaration is a single emission, so the count is the same measurement in both carriers.
        ++counts.emissions;
        counts.measureComponents += requirement.measure.components.size();
        counts.phaseDeclarations += requirement.phases.size();
        counts.requiredFeatureRefs += requirement.required.features.size() +
                                      requirement.pattern.required.features.size() +
                                      requirement.measure.required.features.size();
        counts.requiredCapabilityRefs += requirement.required.capabilities.size() +
                                         requirement.pattern.required.capabilities.size() +
                                         requirement.measure.required.capabilities.size();
        counts.patternNodes += countPatternNodes(requirement.pattern.root);
    }
    for (const auto& resource : graph.resources) {
        counts.requiredFeatureRefs += resource.required.features.size();
        counts.requiredCapabilityRefs += resource.required.capabilities.size();
    }
    for (const auto& relation : graph.relations) {
        counts.requiredFeatureRefs += relation.required.features.size();
        counts.requiredCapabilityRefs += relation.required.capabilities.size();
    }
    for (const auto& profile : graph.solverProfiles) {
        counts.requiredFeatureRefs += profile.required.features.size();
        counts.requiredCapabilityRefs += profile.required.capabilities.size();
    }
    for (const auto& domain : graph.judgementDomains) {
        counts.requiredFeatureRefs += domain.required.features.size();
        counts.requiredCapabilityRefs += domain.required.capabilities.size();
    }
    return counts;
}

auto checkContentProfile(const ContentProfileCounts& counts, const ContentProfileLimits& limits)
    -> core::Result<ContentProfileVerdict> {
    struct Row final {
        const MeasuredParameter<std::uint64_t>* limit;
        std::uint64_t count;
        std::string_view path;
    };
    //  A pending bound is reported as not enforced. BUDGET 3.2 keeps 0 as a literal upper bound, so
    //  a zero can never stand for "no limit" and this table never substitutes one.
    const Row rows[] = {
        Row{&limits.maxRequirements, counts.requirements, "maxRequirements"},
        Row{&limits.maxResources, counts.resources, "maxResources"},
        Row{&limits.maxExclusiveRelations, counts.exclusiveRelations, "maxExclusiveRelations"},
        Row{&limits.maxRelationMembers, counts.relationMembers, "maxRelationMembers"},
        Row{&limits.maxMergedDeclarations, counts.mergedDeclarations, "maxMergedDeclarations"},
        Row{&limits.maxPatternNodes, counts.patternNodes, "maxPatternNodes"},
        Row{&limits.maxMeasureComponents, counts.measureComponents, "maxMeasureComponents"},
        Row{&limits.maxFactBindings, counts.factBindings, "maxFactBindings"},
        Row{&limits.maxDerivedCapabilities, counts.derivedCapabilities, "maxDerivedCapabilities"},
        Row{&limits.maxDiagnosticMapEntries, counts.diagnosticMapEntries,
            "maxDiagnosticMapEntries"},
    };
    bool pending = false;
    for (const Row& row : rows) {
        const std::uint64_t* accepted = row.limit->measuredValue();
        if (accepted == nullptr) {
            pending = true;
            continue;
        }
        if (row.count > *accepted) {
            return core::unexpected(valueError(
                codes::kContentProfileExceededCode, codes::kBudgetSection,
                std::string{codes::kContentProfilePath} + "." + std::string{row.path},
                "a content-profile count is above the upper bound that was accepted for it"));
        }
    }
    return pending ? ContentProfileVerdict::notEnforcedPendingBounds
                   : ContentProfileVerdict::withinDeclaredBounds;
}

auto assembleGameplay(const AssemblyRequest& request) -> core::Result<AssembledGameplay> {
#if defined(CUEXIS_PD_METRICS_TEST_ONLY)
    std::optional<internal::pd::Timer> measurement;
    measurement.emplace(internal::pd::Stage::gameplayAssemblyEntryCheck);
#endif
    const auto entryChecked = validateEntry(request);
    if (!entryChecked.has_value()) {
        return core::unexpected(entryChecked.error());
    }
#if defined(CUEXIS_PD_METRICS_TEST_ONLY)
    measurement.reset();
    measurement.emplace(internal::pd::Stage::gameplayAssemblyNamespaceMerge);
#endif
    auto merged = mergeNamespace(request);
    if (!merged.has_value()) {
        return core::unexpected(merged.error());
    }

#if defined(CUEXIS_PD_METRICS_TEST_ONLY)
    measurement.reset();
    measurement.emplace(internal::pd::Stage::gameplayAssemblyGraphBuild);
#endif
    CanonicalGameplayGraph graph{};
    graph.gameplayVersion = 2U;
    graph.graphRevision = request.graphRevision;
    graph.executionProfile = request.executionProfile;
    graph.normalizationProfileToken = request.normalizationProfileToken;
    graph.coordinatorPolicyToken = request.coordinatorPolicyToken;
    graph.timebase = request.timebase;
    graph.latePolicy = request.latePolicy;
    graph.rulesetRef = request.rulesetRef;
    graph.declaredCapabilities = request.declaredCapabilities;
    graph.declaredFeatures = request.declaredFeatures;
    graph.closureContributions = request.closureContributions;
    // The copy and stabilization retain stable-ID order, so the index positions stay valid.
    graph.mergedNamespace = merged->value;
    graph.sourceClosure.sourceDocumentIds.reserve(request.sources.size());
    for (const auto& source : request.sources) {
        graph.sourceClosure.sourceDocumentIds.push_back(source.document.sourceDocumentId);
        const GameplaySourceDocument& document = source.document;
        graph.requirements.insert(graph.requirements.end(), document.requirements.begin(),
                                  document.requirements.end());
        graph.resources.insert(graph.resources.end(), document.resources.begin(),
                               document.resources.end());
        graph.relations.insert(graph.relations.end(), document.relations.begin(),
                               document.relations.end());
        graph.solverProfiles.insert(graph.solverProfiles.end(), document.solverProfiles.begin(),
                                    document.solverProfiles.end());
        graph.factBindings.insert(graph.factBindings.end(), document.factBindings.begin(),
                                  document.factBindings.end());
        graph.judgementDomains.insert(graph.judgementDomains.end(),
                                      document.judgementDomains.begin(),
                                      document.judgementDomains.end());
    }
    graph.sourceClosure.compilerProfileToken = request.compilerProfileToken;
    graph.diagnosticMap = buildDiagnosticMap(request);

#if defined(CUEXIS_PD_METRICS_TEST_ONLY)
    measurement.reset();
    measurement.emplace(internal::pd::Stage::gameplayAssemblyStabilize);
#endif
    stabilizeGraph(graph);

#if defined(CUEXIS_PD_METRICS_TEST_ONLY)
    measurement.reset();
    measurement.emplace(internal::pd::Stage::gameplayAssemblyDeclarations);
#endif
    const auto declarationsChecked = validateContentDeclarations(graph, merged->byName);
    if (!declarationsChecked.has_value()) {
        return core::unexpected(declarationsChecked.error());
    }

#if defined(CUEXIS_PD_METRICS_TEST_ONLY)
    measurement.reset();
    measurement.emplace(internal::pd::Stage::gameplayAssemblyClosures);
#endif
    graph.derivedCapabilities = deriveCapabilityClosure(graph);
    graph.derivedFeatures = deriveFeatureClosure(graph);
    graph.resourceClosure = deriveResourceClosure(graph);

    const auto closuresChecked = validateClosures(graph, request);
    if (!closuresChecked.has_value()) {
        return core::unexpected(closuresChecked.error());
    }

#if defined(CUEXIS_PD_METRICS_TEST_ONLY)
    measurement.reset();
    measurement.emplace(internal::pd::Stage::gameplayAssemblyBudget);
#endif
    const ContentProfileCounts counts = countContentProfile(graph);
    const auto verdict = checkContentProfile(counts, request.contentProfileLimits);
    if (!verdict.has_value()) {
        return core::unexpected(verdict.error());
    }

    //  The identities are computed while the graph is still here, and the value is assembled from
    //  its parts in one aggregate initialization so that no partially built result can exist.
#if defined(CUEXIS_PD_METRICS_TEST_ONLY)
    measurement.reset();
    measurement.emplace(internal::pd::Stage::gameplayAssemblyIdentities);
#endif
    const auto [chart, content, prepared] =
        detail::IdentityFactory::assemble(graph, request.identityDeclarations);
#if defined(CUEXIS_PD_METRICS_TEST_ONLY)
    measurement.reset();
#endif
    return AssembledGameplay{.graph = std::move(graph),
                             .chart = chart,
                             .content = content,
                             .prepared = prepared,
                             .contentProfile = counts,
                             .contentProfileVerdict = verdict.value()};
}

auto assembleInto(GameplayPublication& publication, const AssemblyRequest& request)
    -> core::Result<void> {
    //  The value is built first and assigned afterwards, so a failed assembly leaves an already
    //  published value exactly as it was: no half-built graph, no partially replaced identities.
    auto assembled = assembleGameplay(request);
    if (!assembled.has_value()) {
        return core::unexpected(assembled.error());
    }
    publication.active_ = std::move(assembled.value());
    return {};
}

auto partitionReferences(const CanonicalGameplayGraph& graph) -> ReferenceClosurePartition {
    ReferenceClosurePartition partition;
    if (graph.timebase != nullptr) {
        partition.judgementRef0.push_back(std::string{"timebase:"} +
                                          std::string{graph.timebase->profileId});
    }
    for (const auto& requirement : graph.requirements) {
        partition.judgementRef0.push_back(std::string{"requirement:"} +
                                          requirement.identity.requirementLocalId);
        partition.judgementRef0.push_back(std::string{"coordination-policy:"} +
                                          requirement.localClosePolicyToken);
        partition.judgementRef0.push_back(std::string{"solver-profile:"} +
                                          requirement.solverProfileRef);
        //  The satisfaction and emission paths are judgement-necessary: a changed emission path is
        //  a different requirement identity and a different Fact attribution (Spec 3.8.6 rule 2).
        for (const auto& step : requirement.identity.emissionPath) {
            partition.judgementRef0.push_back(std::string{"emission:"} + step.nodeId);
        }
        for (const auto& binding : requirement.factBindingRefs) {
            partition.judgementRef0.push_back(std::string{"fact-binding-existence:"} + binding);
        }
        for (const auto& claim : requirement.resourceClaims) {
            partition.judgementRef0.push_back(std::string{"resource-claim:"} +
                                              claim.resourceRef.resourceId);
            partition.judgementRef0.push_back(std::string{"claim-key:"} +
                                              claim.claimPolicy.claimKeyToken);
        }
    }
    for (const auto& domain : graph.judgementDomains) {
        //  Judgement geometry is judgement content (P1-01), so its domain belongs to the judgement
        //  closure and never to the presentation contribution.
        partition.judgementRef0.push_back(std::string{"judgement-domain:"} + domain.domainId);
    }
    //  The presentation contribution is not part of the canonical graph, so this partition has no
    //  manifest entry to report: the graph carries the judgement-side existence of a fact binding
    //  and never its detail (Spec 3.8.6 rule 3).
    for (const auto& entry : graph.diagnosticMap.entries) {
        partition.diagnosticMap.push_back(entry.declarationId + "@" + entry.fieldPath);
    }
    sortUnique(partition.judgementRef0);
    sortUnique(partition.presentationManifest);
    sortUnique(partition.diagnosticMap);
    return partition;
}

//  ---------------------------------------------------------------------------------------------
//  The compiled Pattern (plan S7A-3 item 2)
//  ---------------------------------------------------------------------------------------------
//
//  What is compiled, and what is deliberately not:
//
//    * every Stage 7A primitive of ABI domain 3: atom, sequence, choice, bounded repeat, skip,
//      instant and complement;
//    * a `bounded repeat` as prepare-time finite static structure (Spec 3.8.4). The declared bounds
//      are static content, the expansion is complete at compile time, and both the number of
//      primitive instances of that complete expansion and the `maximum x child` upper bound of one
//      branch are computed with checked arithmetic. No runtime counter, no accumulator and no
//      dynamically generated requirement takes part: those forms are not representable in the
//      declaration model at all, and their declarations (`runtimeRepeatCounter`,
//      `dynamicRequirementGeneration`) are still rejected by `mapUnsupported`. A copy does not have
//      to consume an element: the declared bound is finite on its own, an operand that accepts the
//      empty word is expanded like any other, and the copy order is the increasing copy index with
//      the accepted set the union over the admissible copy counts;
//    * `skip` and `instant` as two distinct declarations of the same language. They are interned as
//      two subpatterns and never normalize into each other, so the difference between them is
//      identity and not language;
//    * a compositional `complement`, which accepts exactly the prefixes none of its operands
//      reaches, so it consumes a prefix and composes with what follows it. The end-anchored reading
//      is the special case in which the rest matches the empty word, not the rule;
//    * interning as the representation: structurally identical subpatterns share one compiled
//    state.
//      The size of that representation is reported as the interned subpattern count, and it is
//      explicitly NOT the state count of Spec 8.2, which counts the determinised and minimised
//      automaton and is measured and reported separately;
//    * the fixed `leftmost-first` policy of ABI domain 3. It is observable, not merely asserted:
//      `leftmostAcceptingArm` reports the arm the matcher selected, and arm order is declared
//      operand order.
//
//  The arm / deadline containment gate is consumed by the assembly validator below. A requirement
//  with a non-empty Pattern must carry measured capacities for both dimensions, and a gate
//  incompleteness result is converted into an atomic prepare rejection rather than a diagnostic
//  overrun.

} // namespace cuexis::judgement

// Gameplay assembler validation: requirements, resources, relations, closures and identity.

#include "gameplay_assembler_internal.hpp"

namespace cuexis::judgement {

using namespace assembler_detail;

[[nodiscard]] auto validateRequirement(const CanonicalGameplayGraph& graph,
                                       const RequirementRecord& requirement, std::size_t index,
                                       std::span<const std::size_t> declarationNames)
    -> core::Result<void> {
    const std::string path =
        std::string{codes::kRequirementsPath} + "[" + std::to_string(index) + "]";
    //  P2-05: action, required action and domain are three independently declared fields, so each
    //  of them has to be declared. None of them is derived from another, and an empty one is a
    //  missing declaration rather than a value the assembler may infer.
    if (requirement.requiredActions.empty()) {
        return core::unexpected(
            declarationInvalidError(codes::kDeclarationIncompleteCode, codes::kGraphSection,
                                    path + "." + std::string{codes::kRequiredActionsPath},
                                    "a requirement must declare at least one required action"));
    }
    if (requirement.domainBinding.token().empty()) {
        return core::unexpected(
            declarationInvalidError(codes::kDeclarationIncompleteCode, codes::kGraphSection,
                                    path + "." + std::string{codes::kDomainBindingPath},
                                    "a requirement must declare the domain it is scoped to"));
    }
    if (requirement.judgementDomainId.empty() ||
        findDomain(graph, requirement.judgementDomainId) == nullptr) {
        return core::unexpected(declarationInvalidError(
            codes::kReferenceDanglingCode, codes::kGraphSection,
            path + "." + std::string{codes::kJudgementDomainRefPath},
            "a requirement references a judgement domain that the graph does not declare"));
    }
    if (requirement.phases.empty()) {
        return core::unexpected(declarationInvalidError(
            codes::kDeclarationIncompleteCode, codes::kGraphSection,
            path + "." + std::string{codes::kPhasesPath},
            "a requirement must declare its phases; the phase registry is not a source of an "
            "implicit one"));
    }
    for (std::size_t phase = 1; phase < requirement.phases.size(); ++phase) {
        if (requirement.phases[phase].kind == requirement.phases[phase - 1].kind) {
            return core::unexpected(
                declarationInvalidError(codes::kDeclarationDuplicateCode, codes::kGraphSection,
                                        path + "." + std::string{codes::kPhasesPath},
                                        "a requirement may declare each phase once"));
        }
    }
    //  The measure is validated by the compiled entry point itself, so the assembly path and a
    //  standalone compile of the same declaration cannot drift apart: one implementation decides
    //  the phase-derived category, the per-component uniqueness and the explicit Release / tail.
    //  The requirement-side phase surface is passed in, and the prefix keeps the diagnostic naming
    //  the requirement the measure belongs to.
    const auto measureChecked = compileMeasure(
        requirement.measure, MeasurePhaseContext{.declaredPhases = requirement.phases,
                                                 .requiresReleaseTailSemantics =
                                                     requirement.requiresReleaseTailSemantics,
                                                 .fieldPathPrefix = path});
    if (!measureChecked.has_value()) {
        return core::unexpected(measureChecked.error());
    }
    const auto patternShape = measurePatternTree(requirement.pattern.root);
    if (!patternShape.has_value()) {
        return core::unexpected(patternShape.error());
    }
    const auto compiledPattern = compilePattern(requirement.pattern);
    if (!compiledPattern.has_value()) {
        return core::unexpected(compiledPattern.error());
    }
    PatternArmBound containmentBound;
    containmentBound.declaredActionRefs = requirement.patternArmRefs;
    if (requirement.maxArmElements.has_value()) {
        containmentBound.maxArmElements = requirement.maxArmElements.value();
    }
    if (requirement.maxDeadlineElements.has_value()) {
        containmentBound.maxDeadlineElements = requirement.maxDeadlineElements.value();
    }
    const auto containmentChecked =
        checkPatternContainment(compiledPattern.value(), containmentBound);
    if (!containmentChecked.has_value()) {
        return core::unexpected(containmentChecked.error());
    }
    if (containmentChecked.value() == ContainmentStatus::gateIncomplete &&
        !compiledPattern->atomRefs().empty()) {
        return core::unexpected(declarationInvalidError(
            codes::kDeclarationIncompleteCode, codes::kGraphSection,
            path + "." + std::string{codes::kPatternPath},
            "the Pattern containment gate is incomplete and the requirement cannot be published"));
    }
    if (!requirement.pattern.patternId.empty() &&
        findDeclarationByName(graph.mergedNamespace, declarationNames,
                              DeclarationKind::patternDefinition,
                              requirement.pattern.patternId) == nullptr) {
        return core::unexpected(declarationInvalidError(
            codes::kReferenceDanglingCode, codes::kGraphSection,
            path + "." + std::string{codes::kPatternPath},
            "a requirement references a pattern declaration that the merged namespace does not "
            "declare"));
    }
    for (std::size_t claim = 0; claim < requirement.resourceClaims.size(); ++claim) {
        const ResourceClaimDeclaration& declared = requirement.resourceClaims[claim];
        if (findResource(graph, declared.resourceRef.resourceId) == nullptr) {
            return core::unexpected(declarationInvalidError(
                codes::kResourceReferenceDanglingCode, codes::kGraphSection,
                path + "." + std::string{codes::kResourceClaimPath},
                "a resource claim references a resource the graph does not declare"));
        }
        if (declared.claimPolicy.policyToken.empty()) {
            return core::unexpected(
                declarationInvalidError(codes::kDeclarationIncompleteCode, codes::kGraphSection,
                                        path + "." + std::string{codes::kClaimPolicyPath},
                                        "a resource claim must declare its claim policy"));
        }
        if (claim > 0 && requirement.resourceClaims[claim - 1].resourceRef.resourceId ==
                             declared.resourceRef.resourceId) {
            return core::unexpected(
                declarationInvalidError(codes::kDeclarationDuplicateCode, codes::kGraphSection,
                                        path + "." + std::string{codes::kResourceClaimPath},
                                        "a requirement may claim one resource once"));
        }
    }
    if (requirement.grace.policy == GraceResolutionPolicy::inheritedDeclaration) {
        if (requirement.grace.inheritedFromDeclarationId.empty()) {
            return core::unexpected(declarationInvalidError(
                codes::kDeclarationIncompleteCode, codes::kGraphSection,
                path + "." + std::string{codes::kGracePath},
                "an inherited grace must name the declaration it inherits from"));
        }
    } else if (!requirement.grace.inheritedFromDeclarationId.empty()) {
        return core::unexpected(declarationInvalidError(
            codes::kDeclarationIncompleteCode, codes::kGraphSection,
            path + "." + std::string{codes::kGracePath},
            "only an inherited grace names a declaration it inherits from"));
    }
    if (requirement.solverProfileRef.empty()) {
        return core::unexpected(declarationInvalidError(
            codes::kSolverProfileMissingCode, codes::kGraphSection,
            path + "." + std::string{codes::kSolverProfileRefPath},
            "a requirement must declare the solver profile its coordination uses"));
    }
    if (findSolverProfile(graph, requirement.solverProfileRef) == nullptr) {
        return core::unexpected(declarationInvalidError(
            codes::kSolverProfileMissingCode, codes::kGraphSection,
            path + "." + std::string{codes::kSolverProfileRefPath},
            "a requirement references a solver profile the graph does not declare"));
    }
    for (std::size_t binding = 0; binding < requirement.factBindingRefs.size(); ++binding) {
        if (findFactBinding(graph, requirement.factBindingRefs[binding]) == nullptr) {
            return core::unexpected(declarationInvalidError(
                codes::kReferenceDanglingCode, codes::kGraphSection,
                path + "." + std::string{codes::kFactBindingsPath},
                "a requirement references a fact binding the graph does not declare"));
        }
        if (binding > 0 &&
            requirement.factBindingRefs[binding - 1] == requirement.factBindingRefs[binding]) {
            return core::unexpected(
                declarationInvalidError(codes::kDeclarationDuplicateCode, codes::kGraphSection,
                                        path + "." + std::string{codes::kFactBindingsPath},
                                        "a requirement may reference a fact binding once"));
        }
    }
    if (!requirement.unsupportedForms.empty()) {
        const UnsupportedContentDeclaration& form = requirement.unsupportedForms.front();
        const UnsupportedMapping mapping = mapUnsupported(form.kind);
        return core::unexpected(
            capabilityRejection(mapping.code, mapping.category, codes::kGraphSection,
                                path + "." + std::string{codes::kUnsupportedFormPath} + "." +
                                    std::string{form.declaredToken},
                                "this requirement declares a content form Stage 7A rejects",
                                mapping.capabilityId, mapping.remediation));
    }
    return {};
}

[[nodiscard]] auto validateResource(const ResourceRecord& resource, std::size_t index)
    -> core::Result<void> {
    const std::string path = std::string{codes::kResourcesPath} + "[" + std::to_string(index) + "]";
    if (resource.ref.resourceId.empty()) {
        return core::unexpected(declarationInvalidError(
            codes::kDeclarationIncompleteCode, codes::kGraphSection, codes::kResourceRefPath,
            "a resource record must declare its identity"));
    }
    if (resource.slotToken.empty()) {
        return core::unexpected(declarationInvalidError(
            codes::kDeclarationIncompleteCode, codes::kGraphSection, codes::kSlotPath,
            "a resource record must declare its slot identity; the engine assigns it and a host "
            "does not"));
    }
    if (resource.decisionPolicyRef.empty()) {
        return core::unexpected(declarationInvalidError(
            codes::kDeclarationIncompleteCode, codes::kGraphSection, codes::kDecisionPolicyPath,
            "a resource record must declare its resource decision policy"));
    }
    //  Spec 3.10: the Stage 7A resource subset is exactly one exclusive resource with
    //  `capacity = 1`. A declared capacity of any other value is a stable rejection, not an
    //  unspellable declaration.
    if (resource.declaredCapacity != 1U) {
        return core::unexpected(capabilityRejection(
            codes::kResourceCapacityUnsupportedCode, codes::kCapabilityDisabledCategory,
            codes::kGraphSection, path + "." + std::string{codes::kCapacityPath},
            "Stage 7A admits only an exclusive resource with capacity 1",
            codes::kResourceCapacityCapabilityId, codes::kLaterBatchRemediation));
    }
    //  Spec 3.10: `grace = 0` is the only legal Stage 7A value for the resource-state grace, and a
    //  nonzero one belongs to the handoff / gap family this batch rejects. This is the Gameplay I
    //  resource-state grace and not the prepare-time preparedGrace of a requirement.
    if (resource.declaredGapGrace.value() != 0) {
        return core::unexpected(capabilityRejection(
            codes::kResourceHandoffUnsupportedCode, codes::kCapabilityDisabledCategory,
            codes::kGraphSection, path + "." + std::string{codes::kGapGracePath},
            "Stage 7A admits only a zero resource-state grace: a nonzero grace belongs to the "
            "handoff family",
            codes::kResourceHandoffCapabilityId, codes::kLaterBatchRemediation));
    }
    if (!resource.unsupportedForms.empty()) {
        const UnsupportedContentDeclaration& form = resource.unsupportedForms.front();
        const UnsupportedMapping mapping = mapUnsupported(form.kind);
        return core::unexpected(
            capabilityRejection(mapping.code, mapping.category, codes::kGraphSection,
                                path + "." + std::string{codes::kUnsupportedFormPath} + "." +
                                    std::string{form.declaredToken},
                                "this resource declares a content form Stage 7A rejects",
                                mapping.capabilityId, mapping.remediation));
    }
    return {};
}

[[nodiscard]] auto validateRelation(const CanonicalGameplayGraph& graph,
                                    const RelationDeclaration& relation, std::size_t index)
    -> core::Result<void> {
    const std::string path = std::string{codes::kRelationsPath} + "[" + std::to_string(index) + "]";
    if (relation.kind != RelationKind::exclusive) {
        return core::unexpected(capabilityRejection(
            codes::kCoordinationRelationUnsupportedCode, codes::kCapabilityDisabledCategory,
            codes::kGraphSection, path, "Stage 7A admits only the exclusive relation kind",
            codes::kCoordinationRelationCapabilityId, codes::kLaterBatchRemediation));
    }
    if (relation.declaredCapacity != 1U) {
        return core::unexpected(capabilityRejection(
            codes::kResourceCapacityUnsupportedCode, codes::kCapabilityDisabledCategory,
            codes::kGraphSection, path + "." + std::string{codes::kCapacityPath},
            "Stage 7A admits only a relation with capacity 1", codes::kResourceCapacityCapabilityId,
            codes::kLaterBatchRemediation));
    }
    if (findResource(graph, relation.resourceRef.resourceId) == nullptr) {
        return core::unexpected(
            declarationInvalidError(codes::kResourceReferenceDanglingCode, codes::kGraphSection,
                                    path + "." + std::string{codes::kResourceRefPath},
                                    "a relation references a resource the graph does not declare"));
    }
    if (relation.policy.policyToken.empty()) {
        return core::unexpected(
            declarationInvalidError(codes::kDeclarationIncompleteCode, codes::kGraphSection,
                                    path + "." + std::string{codes::kClaimPolicyPath},
                                    "a relation must declare its claim policy"));
    }
    if (relation.members.empty()) {
        return core::unexpected(declarationInvalidError(
            codes::kDeclarationIncompleteCode, codes::kGraphSection,
            path + "." + std::string{codes::kReferencesPath},
            "an exclusive relation must declare the requirements that share the resource"));
    }
    for (std::size_t member = 0; member < relation.members.size(); ++member) {
        if (findDeclarationById(graph.mergedNamespace, relation.members[member]) == nullptr) {
            return core::unexpected(declarationInvalidError(
                codes::kReferenceDanglingCode, codes::kGraphSection,
                path + "." + std::string{codes::kReferencesPath},
                "a relation member is not declared in the merged namespace"));
        }
        if (member > 0 && relation.members[member - 1] == relation.members[member]) {
            return core::unexpected(
                declarationInvalidError(codes::kDeclarationDuplicateCode, codes::kGraphSection,
                                        path + "." + std::string{codes::kReferencesPath},
                                        "a relation may declare a member once"));
        }
    }
    return {};
}

[[nodiscard]] auto validateContentDeclarations(const CanonicalGameplayGraph& graph,
                                               std::span<const std::size_t> declarationNames)
    -> core::Result<void> {
    for (std::size_t index = 0; index < graph.judgementDomains.size(); ++index) {
        const auto checked = validateJudgementDomain(graph.judgementDomains[index]);
        if (!checked.has_value()) {
            return core::unexpected(checked.error());
        }
    }
    for (std::size_t index = 0; index < graph.solverProfiles.size(); ++index) {
        const SolverProfileDeclaration& profile = graph.solverProfiles[index];
        if (profile.solverId.empty() || profile.revision.empty() ||
            profile.algorithmToken.empty()) {
            return core::unexpected(declarationInvalidError(
                codes::kDeclarationIncompleteCode, codes::kGraphSection,
                std::string{codes::kSolverProfilesPath} + "[" + std::to_string(index) + "]",
                "a solver profile must declare its identity, its revision and its algorithm"));
        }
    }
    for (std::size_t index = 0; index < graph.factBindings.size(); ++index) {
        if (graph.factBindings[index].bindingId.empty()) {
            return core::unexpected(declarationInvalidError(
                codes::kDeclarationIncompleteCode, codes::kGraphSection,
                std::string{codes::kFactBindingsPath} + "[" + std::to_string(index) + "]",
                "a fact binding reference must declare its identity"));
        }
        if (index > 0 &&
            graph.factBindings[index - 1].bindingId == graph.factBindings[index].bindingId) {
            return core::unexpected(declarationInvalidError(
                codes::kDeclarationDuplicateCode, codes::kGraphSection, codes::kFactBindingsPath,
                "a fact binding may be declared once"));
        }
    }
    //  Every content record has to be declared in the merged namespace: the namespace is the one
    //  authority for names, so a record that is not declared there could be referenced by a name no
    //  declaration owns.
    for (std::size_t index = 0; index < graph.requirements.size(); ++index) {
        const RequirementRecord& requirement = graph.requirements[index];
        const MergedDeclaration* declared =
            findDeclarationById(graph.mergedNamespace, requirement.stableId);
        if (declared == nullptr || declared->kind != DeclarationKind::requirement) {
            return core::unexpected(declarationInvalidError(
                codes::kDeclarationIncompleteCode, codes::kMergeSection,
                std::string{codes::kRequirementsPath} + "[" + std::to_string(index) + "]",
                "a requirement record must be declared as a requirement in the merged namespace"));
        }
        if (graph.executionProfile.empty() &&
            declared->name != requirement.identity.requirementLocalId) {
            return core::unexpected(declarationInvalidError(
                codes::kDeclarationIncompleteCode, codes::kMergeSection,
                std::string{codes::kRequirementsPath} + "[" + std::to_string(index) + "]",
                "the name a requirement is declared under must be its canonical local identity"));
        }
    }
    for (std::size_t index = 0; index < graph.resources.size(); ++index) {
        if (findDeclarationByName(graph.mergedNamespace, declarationNames,
                                  DeclarationKind::resourceRecord,
                                  graph.resources[index].ref.resourceId) == nullptr) {
            return core::unexpected(declarationInvalidError(
                codes::kDeclarationIncompleteCode, codes::kMergeSection,
                std::string{codes::kResourcesPath} + "[" + std::to_string(index) + "]",
                "a resource record must be declared as a resource in the merged namespace"));
        }
    }
    for (std::size_t index = 0; index < graph.judgementDomains.size(); ++index) {
        if (findDeclarationByName(graph.mergedNamespace, declarationNames,
                                  DeclarationKind::judgementDomain,
                                  graph.judgementDomains[index].domainId) == nullptr) {
            return core::unexpected(declarationInvalidError(
                codes::kDeclarationIncompleteCode, codes::kMergeSection,
                std::string{codes::kJudgementDomainsPath} + "[" + std::to_string(index) + "]",
                "a judgement domain must be declared as a judgement domain in the merged "
                "namespace"));
        }
    }
    for (std::size_t index = 0; index < graph.solverProfiles.size(); ++index) {
        if (findDeclarationByName(graph.mergedNamespace, declarationNames,
                                  DeclarationKind::solverProfile,
                                  graph.solverProfiles[index].solverId) == nullptr) {
            return core::unexpected(declarationInvalidError(
                codes::kDeclarationIncompleteCode, codes::kMergeSection,
                std::string{codes::kSolverProfilesPath} + "[" + std::to_string(index) + "]",
                "a solver profile must be declared as a solver profile in the merged namespace"));
        }
    }
    //  Two prepared requirements carrying the same canonical identity are an identity collision:
    //  the six-tuple is what addresses a requirement in the judgement, so it may not repeat.
    for (std::size_t index = 1; index < graph.requirements.size(); ++index) {
        if (graph.requirements[index].identity == graph.requirements[index - 1].identity) {
            return core::unexpected(declarationInvalidError(
                codes::kIdentityCollisionCode, codes::kGraphSection, codes::kRequirementsPath,
                "two requirements carry the same canonical identity"));
        }
    }
    for (std::size_t index = 0; index < graph.requirements.size(); ++index) {
        const auto checked =
            validateRequirement(graph, graph.requirements[index], index, declarationNames);
        if (!checked.has_value()) {
            return core::unexpected(checked.error());
        }
    }
    for (std::size_t index = 0; index < graph.resources.size(); ++index) {
        const auto checked = validateResource(graph.resources[index], index);
        if (!checked.has_value()) {
            return core::unexpected(checked.error());
        }
    }
    for (std::size_t index = 0; index < graph.relations.size(); ++index) {
        const auto checked = validateRelation(graph, graph.relations[index], index);
        if (!checked.has_value()) {
            return core::unexpected(checked.error());
        }
    }
    return {};
}

//  ---------------------------------------------------------------------------------------------
//  Closure validation (Spec 6.1, 6.2, 7.2)
//  ---------------------------------------------------------------------------------------------

[[nodiscard]] auto validateClosures(const CanonicalGameplayGraph& graph,
                                    const AssemblyRequest& request) -> core::Result<void> {
    for (const auto& capability : graph.derivedCapabilities.capabilities) {
        const bool recognised = std::find(request.capabilityContext.recognisedCapabilityIds.begin(),
                                          request.capabilityContext.recognisedCapabilityIds.end(),
                                          capability.capabilityId) !=
                                request.capabilityContext.recognisedCapabilityIds.end();
        if (!recognised) {
            return core::unexpected(rejection(DiagnosticTokens{
                .code = codes::kCapabilityUnknownCode,
                .category = codes::kUnknownCapabilityCategory,
                .summary = "the derived closure needs a capability id the compiling toolchain does "
                           "not recognise",
                .section = codes::kClosureSection,
                .path = std::string{codes::kDerivedCapabilitiesPath} + "." +
                        std::string{codes::kCapabilityIdPath},
                .capabilityId = capability.capabilityId,
                .remediation = codes::kLaterBatchRemediation}));
        }
    }
    for (const auto& capability : graph.derivedCapabilities.capabilities) {
        const bool enabled = std::find(request.capabilityContext.enabledCapabilityIds.begin(),
                                       request.capabilityContext.enabledCapabilityIds.end(),
                                       capability.capabilityId) !=
                             request.capabilityContext.enabledCapabilityIds.end();
        if (!enabled) {
            return core::unexpected(rejection(DiagnosticTokens{
                .code = codes::kCapabilityDisabledCode,
                .category = codes::kCapabilityDisabledCategory,
                .summary = "the derived closure needs a capability this compile recognises but did "
                           "not enable",
                .section = codes::kClosureSection,
                .path = std::string{codes::kDerivedCapabilitiesPath} + "." +
                        std::string{codes::kCapabilityIdPath},
                .capabilityId = capability.capabilityId,
                .remediation = codes::kLaterBatchRemediation}));
        }
    }

    //  Spec 6.2 rule 1: the source declares the minimum it needs, and a derived need the source did
    //  not declare is a stable failure. Comparing against the content-side derivation only keeps
    //  the ruleset and presentation contributions, which those subsystems own, out of the
    //  comparison; the published closure still contains them.
    //
    //  The disposition is deliberately three-way and the three parts differ: the contributions are
    //  excluded from this completeness comparison, retained in the semantic diff (P1-15: they are
    //  graph fields), and retained in the chart / content-artifact identities (they flow into the
    //  derived closure the chart projection carries, and the content provenance writes them).
    //  Dropping them from a field the identity covers would make two different content values share
    //  one identity, and letting them into this comparison would demand a source-side declaration
    //  for something the source does not own.
    const DerivedCapabilityClosure contentCapabilities =
        detail::deriveContentCapabilityClosure(graph);
    const FeatureClosure contentFeatures = detail::deriveContentFeatureClosure(graph);
    for (const auto& capability : contentCapabilities.capabilities) {
        if (!containsCapability(graph.declaredCapabilities.capabilities, capability)) {
            return core::unexpected(closureIncompleteError(
                std::string{codes::kDeclaredCapabilitiesPath} + "." +
                    std::string{codes::kCapabilityIdPath},
                "the content needs a capability it did not declare; the writer never fills a "
                "missing declaration in"));
        }
    }
    for (const auto& feature : contentFeatures.features) {
        if (!containsFeature(graph.declaredFeatures.features, feature)) {
            return core::unexpected(closureIncompleteError(
                std::string{codes::kDeclaredFeaturesPath} + "." +
                    std::string{codes::kFeatureIdPath},
                "the content needs a feature it did not declare; the writer never fills a missing "
                "declaration in"));
        }
    }
    return {};
}

//  ---------------------------------------------------------------------------------------------
//  Assembly
//  ---------------------------------------------------------------------------------------------

[[nodiscard]] auto buildDiagnosticMap(const AssemblyRequest& request) -> DiagnosticMap {
    DiagnosticMap map;
    //  The carrier form and its provenance are recorded and then excluded from every identity and
    //  from the semantic diff, which is what makes a file carrier and a memory carrier holding the
    //  same typed content assemble into the same graph.
    map.form = request.sources.front().form;
    map.carrierProvenance = request.sources.front().provenanceToken;
    for (const auto& source : request.sources) {
        for (const auto& declaration : source.document.declarations) {
            map.entries.push_back(DiagnosticMap::Entry{
                .declarationId = declaration.stableId.sourceDocumentId + "#" +
                                 std::to_string(declaration.stableId.declarationOrdinal),
                .sourceDocumentId = declaration.stableId.sourceDocumentId,
                .fieldPath = declaration.localName});
        }
        for (const auto& requirement : source.document.requirements) {
            map.entries.push_back(DiagnosticMap::Entry{
                .declarationId = requirement.stableId.sourceDocumentId + "#" +
                                 std::to_string(requirement.stableId.declarationOrdinal),
                .sourceDocumentId = requirement.stableId.sourceDocumentId,
                .fieldPath = requirement.identity.requirementLocalId});
        }
    }
    return map;
}

//  ---------------------------------------------------------------------------------------------
//  Public entry points
//  ---------------------------------------------------------------------------------------------

auto CanonicalIdentityBytes::toHex() const -> std::string {
    constexpr std::string_view digits{"0123456789abcdef"};
    std::string text;
    text.reserve(bytes().size() * 2U);
    for (const std::byte value : bytes()) {
        const auto raw = static_cast<unsigned>(value);
        text.push_back(digits[(raw >> 4U) & 0x0FU]);
        text.push_back(digits[raw & 0x0FU]);
    }
    return text;
}

auto sharesJudgementIdentity(const PreparedIdentity& left, const PreparedIdentity& right) noexcept
    -> bool {
    return left.canonicalBytes() == right.canonicalBytes();
}

auto makeChartIdentity(const CanonicalGameplayGraph& graph) -> ChartIdentity {
    IdentityByteWriter writer{"cuexis.judgement.identity.chart.v1"};
    writer.beginComponent("chart");
    writeChartProjection(writer, graph);
    return ChartIdentity{CanonicalIdentityBytes{writer.take()}};
}

auto makeContentIdentity(const CanonicalGameplayGraph& graph) -> ContentIdentity {
    IdentityByteWriter writer{"cuexis.judgement.identity.content.v1"};
    writer.beginComponent("chart");
    writeChartProjection(writer, graph);
    writer.beginComponent("provenance");
    writeContentProvenance(writer, graph);
    return ContentIdentity{CanonicalIdentityBytes{writer.take()}};
}

} // namespace cuexis::judgement

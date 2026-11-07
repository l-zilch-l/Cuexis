// Gameplay assembler identity bytes: the projection encoders.

#include "gameplay_assembler_internal.hpp"

namespace cuexis::judgement {

using namespace assembler_detail;

void writeRequiredRefs(IdentityByteWriter& writer, const RequiredRefs& required) {
    writeFeatureRefs(writer, required.features);
    writeCapabilityRefs(writer, required.capabilities);
}

void writeEmissionPath(IdentityByteWriter& writer, const std::vector<EmissionPathStep>& path) {
    writer.writeCount(path.size());
    for (const auto& step : path) {
        writer.writeText(step.nodeId);
        writer.writeUnsigned(step.repeatIndex);
    }
}

void writeRequirementIdentity(IdentityByteWriter& writer, const RequirementIdentity& identity) {
    writer.writeText(identity.chartEntryId);
    writer.writeText(identity.invocationId);
    writer.writeText(identity.moduleId);
    writer.writeText(identity.exportId);
    writeEmissionPath(writer, identity.emissionPath);
    writer.writeText(identity.requirementLocalId);
}

void writeRequirementCapacity(IdentityByteWriter& writer,
                              const std::optional<MeasuredParameter<std::uint64_t>>& capacity) {
    const RequirementCapacityKey key = requirementCapacityKey(capacity);
    if (key.state == RequirementCapacityState::measured) {
        writer.writeUnsigned(key.value);
    }
}

void writePatternNode(IdentityByteWriter& writer, const PatternNodeDeclaration& root) {
    //  Iterative pre-order over an explicit LIFO stack: a node writes its own fields and its
    //  operand count, and its operands are pushed in reverse so that they are written in declared
    //  order. The byte stream is exactly the stream the recursive form produced, and a deeply
    //  nested declaration is projected with the same host stack as a flat one. The projection is
    //  part of identity, so it may not depend on the host stack depth.
    std::vector<const PatternNodeDeclaration*> stack{&root};
    while (!stack.empty()) {
        const PatternNodeDeclaration& node = *stack.back();
        stack.pop_back();
        writer.writeText(patternPrimitiveToken(node.primitive));
        writer.writeText(node.atomRef);
        writer.writeBool(node.repeatBounds.has_value());
        if (node.repeatBounds.has_value()) {
            writer.writeUnsigned(node.repeatBounds->minimum);
            writer.writeUnsigned(node.repeatBounds->maximum);
        }
        writer.writeBool(node.unsupportedForm.has_value());
        if (node.unsupportedForm.has_value()) {
            writer.writeText(unsupportedKindToken(node.unsupportedForm->kind));
            writer.writeText(node.unsupportedForm->declaredToken);
        }
        writer.writeCount(node.operands.size());
        for (auto operand = node.operands.rbegin(); operand != node.operands.rend(); ++operand) {
            stack.push_back(&*operand);
        }
    }
}

void writeUnsupportedForms(IdentityByteWriter& writer,
                           const std::vector<UnsupportedContentDeclaration>& forms) {
    writer.writeCount(forms.size());
    for (const auto& form : forms) {
        writer.writeText(unsupportedKindToken(form.kind));
        writer.writeText(form.declaredToken);
    }
}

void writeTimebaseProjection(IdentityByteWriter& writer, const TimebaseProfile& profile) {
    writer.writeText(profile.profileId);
    writer.writeText(profile.unitToken);
    writer.writeSigned(profile.tickScale.numerator());
    writer.writeSigned(profile.tickScale.denominator());
    writer.writeSigned(profile.originBeat.numerator());
    writer.writeSigned(profile.originBeat.denominator());
    writer.writeBool(profile.initialTempo.has_value());
    if (profile.initialTempo.has_value()) {
        writer.writeSigned(profile.initialTempo->numerator());
        writer.writeSigned(profile.initialTempo->denominator());
    }
    writer.writeCount(profile.tempoSections.size());
    for (const auto& section : profile.tempoSections) {
        writer.writeSigned(section.startBeat.numerator());
        writer.writeSigned(section.startBeat.denominator());
        writer.writeSigned(section.durationPerBeat.numerator());
        writer.writeSigned(section.durationPerBeat.denominator());
    }
    writer.writeCount(profile.stopSections.size());
    for (const auto& section : profile.stopSections) {
        writer.writeSigned(section.startBeat.numerator());
        writer.writeSigned(section.startBeat.denominator());
        writer.writeSigned(section.endBeat.numerator());
        writer.writeSigned(section.endBeat.denominator());
        writer.writeSigned(section.duration.numerator());
        writer.writeSigned(section.duration.denominator());
    }
}

void writeLatePolicyProjection(IdentityByteWriter& writer, const LatePolicyParameters& parameters) {
    const auto writeParameter = [&writer](const MeasuredParameter<TickSpan>& parameter) {
        const TickSpan* value = parameter.measuredValue();
        writer.writeBool(value != nullptr);
        if (value != nullptr) {
            writer.writeSigned(value->value());
        }
    };
    writeParameter(parameters.finalizationWatermark);
    writeParameter(parameters.maxQueueHop);
    writeParameter(parameters.windowCloseThreshold);
    writeParameter(parameters.windowOpenThreshold);
    writer.writeBool(parameters.policy.has_value());
    if (parameters.policy.has_value()) {
        writer.writeText(lateEventPolicyToken(*parameters.policy));
    }
}

//  The chart / judgement projection. It is written first by both the chart identity and the content
//  identity, and the content identity only appends provenance after it, so equal content bytes
//  imply equal chart bytes.
void writeChartProjection(IdentityByteWriter& writer, const CanonicalGameplayGraph& graph) {
    writer.writeUnsigned(graph.gameplayVersion);
    writer.writeUnsigned(graph.graphRevision);
    writer.writeText(graph.rulesetRef);
    writer.writeBool(graph.timebase != nullptr);
    if (graph.timebase != nullptr) {
        writeTimebaseProjection(writer, *graph.timebase);
    }
    writer.writeBool(graph.latePolicy != nullptr);
    if (graph.latePolicy != nullptr) {
        writeLatePolicyProjection(writer, *graph.latePolicy);
    }
    writer.writeCount(graph.requirements.size());
    for (const auto& requirement : graph.requirements) {
        writeRequirementIdentity(writer, requirement.identity);
        writer.writeCount(requirement.requiredActions.size());
        for (const auto& action : requirement.requiredActions) {
            writer.writeText(action.token());
        }
        writer.writeText(requirement.domainBinding.token());
        writer.writeText(requirement.judgementDomainId);
        writer.writeCount(requirement.phases.size());
        for (const auto& phase : requirement.phases) {
            writer.writeText(phaseKindToken(phase.kind));
            writer.writeUnsigned(phase.declarationOrdinal);
        }
        writer.writeBool(requirement.requiresReleaseTailSemantics);
        writer.writeCount(requirement.patternArmRefs.size());
        for (const auto& arm : requirement.patternArmRefs) {
            writer.writeText(arm);
        }
        writer.writeBool(requirement.timing.has_value());
        if (requirement.timing.has_value()) {
            writer.writeSigned(requirement.timing->end.value());
            writer.writeCount(requirement.timing->successWindows.size());
            for (const auto& window : requirement.timing->successWindows) {
                writer.writeText(phaseKindToken(window.phase.kind));
                writer.writeUnsigned(window.phase.declarationOrdinal);
                writer.writeSigned(window.start.value());
                writer.writeSigned(window.end.value());
            }
            writer.writeBool(requirement.timing->body.has_value());
            if (requirement.timing->body.has_value()) {
                writer.writeSigned(requirement.timing->body->start.value());
                writer.writeSigned(requirement.timing->body->end.value());
            }
        }
        writeRequirementCapacity(writer, requirement.maxArmElements);
        writeRequirementCapacity(writer, requirement.maxDeadlineElements);
        writer.writeText(matchPolicyToken(requirement.pattern.matchPolicy));
        writePatternNode(writer, requirement.pattern.root);
        writeRequiredRefs(writer, requirement.pattern.required);
        writer.writeCount(requirement.measure.components.size());
        for (const auto& component : requirement.measure.components) {
            writer.writeText(phaseKindToken(component.phase));
            writer.writeText(component.categoryToken);
            //  The declared grade tokens are judgement content of the component, so they are part
            //  of the chart projection and of the content identity derived from it. Whether a
            //  component declared a grade table at all is therefore also covered: an absent table
            //  and a declared one never produce the same bytes.
            writer.writeCount(component.declaredGradeTokens.size());
            for (const auto& grade : component.declaredGradeTokens) {
                writer.writeText(grade);
            }
            if (component.gradeTable) {
                writer.writeText("measure.signed-interval.v1");
                writer.writeCount(component.gradeTable->size());
                for (const auto& row : *component.gradeTable) {
                    writer.writeText(std::to_string(row.minimum));
                    writer.writeText(std::to_string(row.maximum));
                    writer.writeText(row.grade);
                }
            }
        }
        writeRequiredRefs(writer, requirement.measure.required);
        writer.writeCount(requirement.resourceClaims.size());
        for (const auto& claim : requirement.resourceClaims) {
            writer.writeText(claim.resourceRef.resourceId);
            writer.writeText(claimIntentToken(claim.intent));
            writer.writeText(claim.claimPolicy.policyToken);
            writer.writeText(claim.claimPolicy.claimKeyToken);
            writer.writeBool(claim.claimPolicy.competition.has_value());
            if (claim.claimPolicy.competition.has_value()) {
                writer.writeSigned(claim.claimPolicy.competition->priority);
                writer.writeUnsigned(claim.claimPolicy.competition->tieRank);
            }
            writer.writeText(graceOverrideToken(claim.graceOverride.mode));
            writer.writeText(claim.graceOverride.overrideToken);
        }
        //  The resolved grace value and its policy are judgement content. The declaration the value
        //  was inherited from is provenance and is written by the content projection only.
        writer.writeSigned(requirement.preparedGrace.span().value());
        writer.writeCount(requirement.factBindingRefs.size());
        for (const auto& binding : requirement.factBindingRefs) {
            writer.writeText(binding);
        }
        writer.writeText(requirement.solverProfileRef);
        writer.writeText(requirement.localClosePolicyToken);
        writeRequiredRefs(writer, requirement.required);
        writeUnsupportedForms(writer, requirement.unsupportedForms);
        if (!graph.executionProfile.empty() || !requirement.atomBindings.empty() ||
            requirement.independentCompetition ||
            (requirement.timing && !requirement.timing->phaseTargets.empty())) {
            writer.writeText("execution.requirement.v1");
            writer.writeCount(requirement.timing ? requirement.timing->phaseTargets.size() : 0);
            if (requirement.timing) {
                for (const auto& target : requirement.timing->phaseTargets) {
                    writer.writeText(phaseKindToken(target.phase));
                    writer.writeSigned(target.chartTick.value());
                }
            }
            writer.writeCount(requirement.atomBindings.size());
            for (const auto& binding : requirement.atomBindings) {
                writer.writeText(binding.atomRef);
                writer.writeText(binding.domainToken);
                writer.writeText(binding.sourceClass);
                writer.writeText(binding.channelToken);
                writer.writeUnsigned(static_cast<unsigned>(binding.action));
                writer.writeBool(binding.amountRange.has_value());
                if (binding.amountRange) {
                    writer.writeSigned(binding.amountRange->minimum);
                    writer.writeSigned(binding.amountRange->maximum);
                }
                writer.writeBool(binding.tailOnly);
            }
            writer.writeBool(requirement.independentCompetition.has_value());
            if (requirement.independentCompetition) {
                writer.writeSigned(requirement.independentCompetition->priority);
                writer.writeUnsigned(requirement.independentCompetition->tieRank);
            }
        }
    }
    writer.writeCount(graph.resources.size());
    for (const auto& resource : graph.resources) {
        writer.writeText(resource.ref.resourceId);
        writer.writeUnsigned(resource.declaredCapacity);
        writer.writeText(resource.slotToken);
        writer.writeText(resource.decisionPolicyRef);
        writer.writeBool(resource.terminalAfterTermination);
        writer.writeSigned(resource.declaredGapGrace.value());
        writeUnsupportedForms(writer, resource.unsupportedForms);
        writeRequiredRefs(writer, resource.required);
    }
    writer.writeCount(graph.relations.size());
    for (const auto& relation : graph.relations) {
        writer.writeText(relationKindToken(relation.kind));
        writer.writeText(relation.resourceRef.resourceId);
        writeStableIds(writer, relation.members);
        writer.writeText(relation.policy.policyToken);
        writer.writeText(relation.policy.claimKeyToken);
        writer.writeBool(relation.policy.competition.has_value());
        if (relation.policy.competition.has_value()) {
            writer.writeSigned(relation.policy.competition->priority);
            writer.writeUnsigned(relation.policy.competition->tieRank);
        }
        writer.writeUnsigned(relation.declaredCapacity);
        writeRequiredRefs(writer, relation.required);
    }
    writer.writeCount(graph.solverProfiles.size());
    for (const auto& profile : graph.solverProfiles) {
        writer.writeText(profile.solverId);
        writer.writeText(profile.revision);
        writer.writeText(profile.algorithmToken);
        writer.writeCount(profile.objective.size());
        for (const auto& objective : profile.objective) {
            writer.writeText(objective);
        }
        writer.writeCount(profile.tieBreak.size());
        for (const auto& tieBreak : profile.tieBreak) {
            writer.writeText(tieBreak);
        }
        writer.writeBool(profile.rejectIfNonUnique);
        writeRequiredRefs(writer, profile.required);
    }
    writer.writeCount(graph.factBindings.size());
    for (const auto& binding : graph.factBindings) {
        writer.writeText(binding.bindingId);
    }
    writer.writeCount(graph.judgementDomains.size());
    for (const auto& domain : graph.judgementDomains) {
        writer.writeText(domain.domainId);
        writer.writeText(domain.coordinateSystemToken);
        writer.writeCount(domain.axes.size());
        for (const auto& axis : domain.axes) {
            writer.writeText(axis.axisToken);
            writer.writeSigned(axis.minimum);
            writer.writeSigned(axis.maximum);
        }
        writer.writeText(frameResolutionToken(domain.frameResolution));
        writer.writeText(domain.dynamicFrameProviderToken);
        writeRequiredRefs(writer, domain.required);
    }
    //  The declared set and the derived closure are two separately queryable things and both take
    //  part in the judgement projection (Spec 5.2 `capabilities[]` and plan P1-15).
    writeCapabilityRefs(writer, graph.declaredCapabilities.capabilities);
    writeCapabilityRefs(writer, graph.derivedCapabilities.capabilities);
    if (!graph.executionProfile.empty() || !graph.normalizationProfileToken.empty() ||
        !graph.coordinatorPolicyToken.empty()) {
        writer.writeText("execution.graph.v1");
        writer.writeText(graph.executionProfile);
        writer.writeText(graph.normalizationProfileToken);
        writer.writeText(graph.coordinatorPolicyToken);
    }
}

//  The provenance the content projection adds. None of it is judgement content: it is authoring
//  identity, toolchain identity and the source of a resolved value, which Spec 5.2 attributes to
//  the content projection and to the diagnostic context.

//  ---------------------------------------------------------------------------------------------
//  Small helpers
//  ---------------------------------------------------------------------------------------------

} // namespace cuexis::judgement

#include "gameplay_graph_render_internal.hpp"

#include "gameplay_closure.hpp"
#include "source_codes.hpp"

#include <cuexis/judgement/diagnostic.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cuexis::judgement {

//  ---------------------------------------------------------------------------------------------
//  The field-level diff recorder
//  ---------------------------------------------------------------------------------------------

class DifferenceRecorder final {
  public:
    void add(std::string path, std::string left, std::string right) {
        differences_.push_back(GraphFieldDifference{
            .path = std::move(path), .left = std::move(left), .right = std::move(right)});
    }

    [[nodiscard]] auto take() -> std::vector<GraphFieldDifference> {
        return std::move(differences_);
    }

  private:
    std::vector<GraphFieldDifference> differences_;
};

template <typename Value, typename Render>
void compareValue(DifferenceRecorder& recorder, std::string path, const Value& left,
                  const Value& right, Render render) {
    if (!(left == right)) {
        recorder.add(std::move(path), render(left), render(right));
    }
}

//  A stable key for a resource record.
[[nodiscard]] auto resourceKey(const ResourceRecord& record) -> std::string {
    return record.ref.resourceId;
}

[[nodiscard]] auto relationKey(const RelationDeclaration& relation) -> std::string {
    return relation.resourceRef.resourceId + "#" +
           std::to_string(static_cast<unsigned>(relation.kind));
}

[[nodiscard]] auto solverKey(const SolverProfileDeclaration& profile) -> std::string {
    return profile.solverId + "@" + profile.revision;
}

[[nodiscard]] auto domainKey(const JudgementDomainRecord& domain) -> std::string {
    return domain.domainId;
}

[[nodiscard]] auto capabilityKey(const CapabilityRef& ref) -> std::string {
    return ref.capabilityId + "@" + ref.revision;
}

[[nodiscard]] auto featureKey(const FeatureRef& ref) -> std::string {
    return ref.featureId;
}

// Keep the first row for a key, matching the original linear lookup even for
// noncanonical inputs. Keys are materialized once, never inside a candidate scan.
template <typename Value, typename Key>
auto indexRows(const std::vector<Value>& rows, Key key)
    -> std::map<std::string, const Value*, std::less<>> {
    std::map<std::string, const Value*, std::less<>> index;
    for (const auto& value : rows)
        index.try_emplace(key(value), &value);
    return index;
}

//  Compares two canonically ordered tables keyed by a stable identity. A key that only one side has
//  is reported as a presence difference, so a missing or extra table row can never be hidden by a
//  size-only comparison.
template <typename Value, typename Key, typename Render>
void compareKeyedTable(DifferenceRecorder& recorder, std::string path,
                       const std::vector<Value>& left, const std::vector<Value>& right, Key key,
                       Render render) {
    if (left.size() != right.size()) {
        recorder.add(path + ".size", std::to_string(left.size()), std::to_string(right.size()));
    }
    const auto rightIndex = indexRows(right, key);
    std::set<std::string, std::less<>> leftKeys;
    for (const auto& value : left) {
        const std::string valueKey = key(value);
        leftKeys.insert(valueKey);
        const auto found = rightIndex.find(valueKey);
        if (found == rightIndex.end()) {
            recorder.add(path + "[" + valueKey + "]", render(value), "<absent>");
            continue;
        }
        if (!(value == *found->second)) {
            recorder.add(path + "[" + valueKey + "]", render(value), render(*found->second));
        }
    }
    for (const auto& value : right) {
        const std::string valueKey = key(value);
        if (!leftKeys.contains(valueKey)) {
            recorder.add(path + "[" + valueKey + "]", "<absent>", render(value));
        }
    }
}

[[nodiscard]] auto renderRequiredRefs(const RequiredRefs& required) -> std::string {
    return "features=" + renderFeatureRefs(required.features) +
           ";capabilities=" + renderCapabilityRefs(required.capabilities);
}

[[nodiscard]] auto renderPattern(const PatternDeclaration& pattern) -> std::string {
    return "patternId=" + pattern.patternId + ";root=" + renderPatternNode(pattern.root) +
           ";required=" + renderRequiredRefs(pattern.required);
}

[[nodiscard]] auto renderMeasure(const MeasureSpecDeclaration& measure) -> std::string {
    std::string rendered{"components="};
    for (std::size_t index = 0; index < measure.components.size(); ++index) {
        if (index != 0) {
            rendered.push_back(',');
        }
        rendered.append(renderPhaseKind(measure.components[index].phase));
        rendered.push_back(':');
        rendered.append(measure.components[index].categoryToken);
        //  The declared grade tokens are part of the component, so the field-by-field diff has to
        //  see them: two components that differ only in their declared grade table are not the same
        //  component and must not compare equal.
        rendered.append(":grades=");
        rendered.append(renderTokens(measure.components[index].declaredGradeTokens));
        if (measure.components[index].gradeTable) {
            rendered.append(":intervals=");
            for (const auto& row : *measure.components[index].gradeTable) {
                rendered.append(std::to_string(row.minimum) + ":" + std::to_string(row.maximum) +
                                ":" + row.grade + ";");
            }
        }
    }
    rendered.append(";required=");
    rendered.append(renderRequiredRefs(measure.required));
    return rendered;
}

[[nodiscard]] auto renderPhases(const std::vector<PhaseDeclaration>& phases) -> std::string {
    std::vector<std::string> tokens;
    tokens.reserve(phases.size());
    for (const auto& phase : phases) {
        tokens.push_back(renderPhaseKind(phase.kind) + "@" +
                         std::to_string(phase.declarationOrdinal));
    }
    return renderTokens(tokens);
}

[[nodiscard]] auto renderGrace(const GraceDeclaration& grace) -> std::string {
    return renderGracePolicy(grace.policy) + ";inheritedFrom=" + grace.inheritedFromDeclarationId +
           ";allowChartGrace=" + renderBool(grace.allowChartGrace);
}

[[nodiscard]] auto renderPreparedGrace(const PreparedGrace& grace) -> std::string {
    return std::to_string(grace.span().value());
}

[[nodiscard]] auto renderClaims(const std::vector<ResourceClaimDeclaration>& claims)
    -> std::string {
    std::string rendered;
    for (const auto& claim : claims) {
        if (!rendered.empty()) {
            rendered.push_back('|');
        }
        rendered.append(claim.resourceRef.resourceId);
        rendered.append(":intent=");
        rendered.append(std::to_string(static_cast<unsigned>(claim.intent)));
        rendered.append(":policy=");
        rendered.append(claim.claimPolicy.policyToken);
        rendered.append(":claimKey=");
        rendered.append(claim.claimPolicy.claimKeyToken);
        if (claim.claimPolicy.competition.has_value()) {
            rendered.append(":competition=");
            rendered.append(std::to_string(claim.claimPolicy.competition->priority));
            rendered.push_back(',');
            rendered.append(std::to_string(claim.claimPolicy.competition->tieRank));
        }
        rendered.append(":graceOverride=");
        rendered.append(std::to_string(static_cast<unsigned>(claim.graceOverride.mode)));
    }
    return rendered;
}

[[nodiscard]] auto renderUnsupported(const std::vector<UnsupportedContentDeclaration>& forms)
    -> std::string {
    std::vector<std::string> tokens;
    tokens.reserve(forms.size());
    for (const auto& form : forms) {
        tokens.push_back(form.declaredToken + "@" +
                         std::to_string(static_cast<unsigned>(form.kind)));
    }
    return renderTokens(tokens);
}

//  Compares one requirement record field by field, under a path that is keyed by its canonical
//  identity so that the diff text itself does not depend on the position of the record.
void compareRequirement(DifferenceRecorder& recorder, const std::string& path,
                        const RequirementRecord& left, const RequirementRecord& right) {
    compareValue(recorder, path + ".stableId", left.stableId, right.stableId,
                 [](const auto& value) {
                     return value.sourceDocumentId + "#" + std::to_string(value.declarationOrdinal);
                 });
    compareValue(recorder, path + ".identity", left.identity, right.identity,
                 renderRequirementIdentity);
    compareValue(recorder, path + ".requiredActions", left.requiredActions, right.requiredActions,
                 [](const std::vector<RequiredActionRef>& actions) {
                     std::vector<std::string> tokens;
                     tokens.reserve(actions.size());
                     for (const auto& action : actions) {
                         tokens.push_back(action.token());
                     }
                     return renderTokens(tokens);
                 });
    compareValue(recorder, path + ".domainBinding", left.domainBinding, right.domainBinding,
                 [](const DomainBindingRef& binding) { return binding.token(); });
    compareValue(recorder, path + ".judgementDomainId", left.judgementDomainId,
                 right.judgementDomainId, renderText);
    compareValue(recorder, path + ".phases", left.phases, right.phases, renderPhases);
    compareValue(recorder, path + ".requiresReleaseTailSemantics",
                 left.requiresReleaseTailSemantics, right.requiresReleaseTailSemantics, renderBool);
    compareValue(recorder, path + ".pattern", left.pattern, right.pattern, renderPattern);
    compareValue(recorder, path + ".patternArmRefs", left.patternArmRefs, right.patternArmRefs,
                 renderTokens);
    compareValue(recorder, path + ".atomBindings", left.atomBindings, right.atomBindings,
                 [](const auto& rows) {
                     std::string text;
                     for (const auto& row : rows) {
                         text += row.atomRef + ":" + row.domainToken + ":" + row.sourceClass + ":" +
                                 row.channelToken + ":" +
                                 std::to_string(static_cast<unsigned>(row.action)) + ":" +
                                 renderBool(row.tailOnly);
                         if (row.amountRange) {
                             text += ":" + std::to_string(row.amountRange->minimum) + ":" +
                                     std::to_string(row.amountRange->maximum);
                         }
                         text += ";";
                     }
                     return text;
                 });
    compareValue(recorder, path + ".independentCompetition", left.independentCompetition,
                 right.independentCompetition, [](const auto& pair) {
                     return pair ? std::to_string(pair->priority) + "," +
                                       std::to_string(pair->tieRank)
                                 : std::string{"absent"};
                 });
    compareValue(recorder, path + ".timing", left.timing, right.timing, [](const auto& timing) {
        if (!timing.has_value()) {
            return std::string{"absent"};
        }
        std::string result = "end=" + std::to_string(timing->end.value());
        const auto append = [&result](const TimeInterval& window) {
            result += ";[" + std::to_string(window.start.value()) + "," +
                      std::to_string(window.end.value()) + ")";
        };
        for (const auto& window : timing->successWindows) {
            result += ";phase=" + std::to_string(static_cast<unsigned>(window.phase.kind)) + ":" +
                      std::to_string(window.phase.declarationOrdinal);
            append(TimeInterval{window.start, window.end});
        }
        result += ";body=";
        if (timing->body.has_value()) {
            append(*timing->body);
        } else {
            result += "absent";
        }
        for (const auto& target : timing->phaseTargets) {
            result += ";target=" + std::to_string(static_cast<unsigned>(target.phase)) + ":" +
                      std::to_string(target.chartTick.value());
        }
        return result;
    });
    if (compareRequirementCapacity(left.maxArmElements, right.maxArmElements) !=
        std::strong_ordering::equal) {
        recorder.add(path + ".maxArmElements", renderRequirementCapacity(left.maxArmElements),
                     renderRequirementCapacity(right.maxArmElements));
    }
    if (compareRequirementCapacity(left.maxDeadlineElements, right.maxDeadlineElements) !=
        std::strong_ordering::equal) {
        recorder.add(path + ".maxDeadlineElements",
                     renderRequirementCapacity(left.maxDeadlineElements),
                     renderRequirementCapacity(right.maxDeadlineElements));
    }
    compareValue(recorder, path + ".measure", left.measure, right.measure, renderMeasure);
    compareValue(recorder, path + ".resourceClaims", left.resourceClaims, right.resourceClaims,
                 renderClaims);
    compareValue(recorder, path + ".grace", left.grace, right.grace, renderGrace);
    compareValue(recorder, path + ".preparedGrace", left.preparedGrace, right.preparedGrace,
                 renderPreparedGrace);
    compareValue(recorder, path + ".factBindingRefs", left.factBindingRefs, right.factBindingRefs,
                 renderTokens);
    compareValue(recorder, path + ".solverProfileRef", left.solverProfileRef,
                 right.solverProfileRef, renderText);
    compareValue(recorder, path + ".localClosePolicyToken", left.localClosePolicyToken,
                 right.localClosePolicyToken, renderText);
    compareValue(recorder, path + ".required", left.required, right.required, renderRequiredRefs);
    compareValue(recorder, path + ".unsupportedForms", left.unsupportedForms,
                 right.unsupportedForms, renderUnsupported);
}

[[nodiscard]] auto renderResource(const ResourceRecord& record) -> std::string {
    return "capacity=" + std::to_string(record.declaredCapacity) + ";slot=" + record.slotToken +
           ";decisionPolicy=" + record.decisionPolicyRef +
           ";terminal=" + renderBool(record.terminalAfterTermination) +
           ";gapGrace=" + std::to_string(record.declaredGapGrace.value()) +
           ";unsupported=" + renderUnsupported(record.unsupportedForms) +
           ";required=" + renderRequiredRefs(record.required);
}

[[nodiscard]] auto renderRelation(const RelationDeclaration& relation) -> std::string {
    std::string competition = "absent";
    if (relation.policy.competition) {
        competition = std::to_string(relation.policy.competition->priority) + "," +
                      std::to_string(relation.policy.competition->tieRank);
    }
    return "kind=" + std::to_string(static_cast<unsigned>(relation.kind)) +
           ";resource=" + relation.resourceRef.resourceId +
           ";members=" + renderStableIds(relation.members) +
           ";policy=" + relation.policy.policyToken + "@" + relation.policy.claimKeyToken +
           ";competition=" + competition +
           ";capacity=" + std::to_string(relation.declaredCapacity) +
           ";required=" + renderRequiredRefs(relation.required);
}

[[nodiscard]] auto renderSolver(const SolverProfileDeclaration& profile) -> std::string {
    return "algorithm=" + profile.algorithmToken + ";objective=" + renderTokens(profile.objective) +
           ";tieBreak=" + renderTokens(profile.tieBreak) +
           ";rejectIfNonUnique=" + renderBool(profile.rejectIfNonUnique) +
           ";required=" + renderRequiredRefs(profile.required);
}

[[nodiscard]] auto renderDomain(const JudgementDomainRecord& domain) -> std::string {
    std::string rendered{"coordinateSystem=" + domain.coordinateSystemToken + ";axes="};
    for (std::size_t index = 0; index < domain.axes.size(); ++index) {
        if (index != 0) {
            rendered.push_back(',');
        }
        rendered.append(domain.axes[index].axisToken);
        rendered.push_back(':');
        rendered.append(std::to_string(domain.axes[index].minimum));
        rendered.push_back('.');
        rendered.append(std::to_string(domain.axes[index].maximum));
    }
    rendered.append(";frame=");
    rendered.append(std::to_string(static_cast<unsigned>(domain.frameResolution)));
    rendered.append(";provider=");
    rendered.append(domain.dynamicFrameProviderToken);
    rendered.append(";required=");
    rendered.append(renderRequiredRefs(domain.required));
    return rendered;
}

[[nodiscard]] auto renderMergedDeclaration(const MergedDeclaration& declaration) -> std::string {
    return "kind=" + std::to_string(static_cast<unsigned>(declaration.kind)) +
           ";name=" + declaration.name + ";references=" + renderStableIds(declaration.references) +
           ";features=" + renderTokens(declaration.requiredFeatureIds) +
           ";capabilities=" + renderTokens(declaration.requiredCapabilityIds);
}

//  Compares the requirement table field by field. The table is paired by the canonical six-tuple,
//  so the diff text is keyed by a stable identity and not by the position of a record; a
//  requirement only one graph has is reported as a presence difference and never as a silently
//  skipped row.
void compareRequirementTable(DifferenceRecorder& recorder, std::string path,
                             const std::vector<RequirementRecord>& left,
                             const std::vector<RequirementRecord>& right) {
    if (left.size() != right.size()) {
        recorder.add(path + ".size", std::to_string(left.size()), std::to_string(right.size()));
    }
    const auto keyOf = [](const RequirementRecord& value) {
        return renderRequirementIdentity(value.identity);
    };
    const auto rightIndex = indexRows(right, keyOf);
    std::set<std::string, std::less<>> leftKeys;
    for (const auto& requirement : left) {
        const std::string key = renderRequirementIdentity(requirement.identity);
        leftKeys.insert(key);
        const auto found = rightIndex.find(key);
        if (found == rightIndex.end()) {
            recorder.add(path + "[" + key + "]", renderRequirementIdentity(requirement.identity),
                         "<absent>");
            continue;
        }
        compareRequirement(recorder, path + "[" + key + "]", requirement, *found->second);
    }
    for (const auto& requirement : right) {
        const std::string key = renderRequirementIdentity(requirement.identity);
        if (!leftKeys.contains(key)) {
            recorder.add(path + "[" + key + "]", "<absent>",
                         renderRequirementIdentity(requirement.identity));
        }
    }
}

auto requirementCapacityKey(const std::optional<MeasuredParameter<std::uint64_t>>& value) noexcept
    -> RequirementCapacityKey {
    if (!value.has_value()) {
        return RequirementCapacityKey{.state = RequirementCapacityState::absent, .value = 0U};
    }
    const std::uint64_t* measured = value->measuredValue();
    if (measured == nullptr) {
        return RequirementCapacityKey{.state = RequirementCapacityState::pending, .value = 0U};
    }
    return RequirementCapacityKey{.state = RequirementCapacityState::measured, .value = *measured};
}

auto compareRequirementCapacity(
    const std::optional<MeasuredParameter<std::uint64_t>>& left,
    const std::optional<MeasuredParameter<std::uint64_t>>& right) noexcept -> std::strong_ordering {
    const RequirementCapacityKey leftKey = requirementCapacityKey(left);
    const RequirementCapacityKey rightKey = requirementCapacityKey(right);
    if (leftKey.state != rightKey.state) {
        return static_cast<unsigned>(leftKey.state) < static_cast<unsigned>(rightKey.state)
                   ? std::strong_ordering::less
                   : std::strong_ordering::greater;
    }
    if (leftKey.state != RequirementCapacityState::measured) {
        return std::strong_ordering::equal;
    }
    return leftKey.value <=> rightKey.value;
}

auto semanticDiff(const CanonicalGameplayGraph& left, const CanonicalGameplayGraph& right)
    -> std::vector<GraphFieldDifference> {
    DifferenceRecorder recorder;

    compareValue(recorder, std::string{codes::kGameplayVersionPath}, left.gameplayVersion,
                 right.gameplayVersion, renderInteger<std::uint32_t>);
    compareValue(recorder, std::string{codes::kGraphRevisionPath}, left.graphRevision,
                 right.graphRevision, renderInteger<std::uint64_t>);
    compareValue(recorder, std::string{codes::kRulesetRefPath}, left.rulesetRef, right.rulesetRef,
                 renderText);
    compareValue(recorder, "executionProfile", left.executionProfile, right.executionProfile,
                 renderText);
    compareValue(recorder, "normalizationProfileToken", left.normalizationProfileToken,
                 right.normalizationProfileToken, renderText);
    compareValue(recorder, "coordinatorPolicyToken", left.coordinatorPolicyToken,
                 right.coordinatorPolicyToken, renderText);

    if (left.timebase == nullptr || right.timebase == nullptr) {
        if (left.timebase != right.timebase) {
            recorder.add(std::string{codes::kTimebaseRefPath},
                         left.timebase == nullptr ? "<absent>" : renderTimebase(*left.timebase),
                         right.timebase == nullptr ? "<absent>" : renderTimebase(*right.timebase));
        }
    } else if (!sameTimebase(*left.timebase, *right.timebase)) {
        recorder.add(std::string{codes::kTimebaseRefPath}, renderTimebase(*left.timebase),
                     renderTimebase(*right.timebase));
    }

    if (left.latePolicy == nullptr || right.latePolicy == nullptr) {
        if (left.latePolicy != right.latePolicy) {
            recorder.add(
                std::string{codes::kLatePolicyReferencePath},
                left.latePolicy == nullptr ? "<absent>" : renderLatePolicy(*left.latePolicy),
                right.latePolicy == nullptr ? "<absent>" : renderLatePolicy(*right.latePolicy));
        }
    } else if (!(*left.latePolicy == *right.latePolicy)) {
        recorder.add(std::string{codes::kLatePolicyReferencePath},
                     renderLatePolicy(*left.latePolicy), renderLatePolicy(*right.latePolicy));
    }

    compareRequirementTable(recorder, std::string{codes::kRequirementsPath}, left.requirements,
                            right.requirements);
    compareKeyedTable(recorder, std::string{codes::kResourcesPath}, left.resources, right.resources,
                      resourceKey, renderResource);
    compareKeyedTable(recorder, std::string{codes::kRelationsPath}, left.relations, right.relations,
                      relationKey, renderRelation);
    compareKeyedTable(recorder, std::string{codes::kSolverProfilesPath}, left.solverProfiles,
                      right.solverProfiles, solverKey, renderSolver);
    compareKeyedTable(recorder, std::string{codes::kJudgementDomainsPath}, left.judgementDomains,
                      right.judgementDomains, domainKey, renderDomain);
    compareKeyedTable(
        recorder, std::string{codes::kFactBindingsPath}, left.factBindings, right.factBindings,
        [](const FactBindingRef& ref) { return ref.bindingId; },
        [](const FactBindingRef& ref) { return ref.bindingId; });
    compareKeyedTable(recorder, std::string{codes::kDeclaredCapabilitiesPath},
                      left.declaredCapabilities.capabilities,
                      right.declaredCapabilities.capabilities, capabilityKey, capabilityKey);
    compareKeyedTable(recorder, std::string{codes::kDerivedCapabilitiesPath},
                      left.derivedCapabilities.capabilities, right.derivedCapabilities.capabilities,
                      capabilityKey, capabilityKey);
    compareKeyedTable(recorder, std::string{codes::kClosureContributionsPath},
                      left.closureContributions.capabilities,
                      right.closureContributions.capabilities, capabilityKey, capabilityKey);
    compareKeyedTable(recorder, std::string{codes::kDeclaredFeaturesPath},
                      left.declaredFeatures.features, right.declaredFeatures.features, featureKey,
                      featureKey);
    compareKeyedTable(recorder, std::string{codes::kDerivedFeaturesPath},
                      left.derivedFeatures.features, right.derivedFeatures.features, featureKey,
                      featureKey);
    compareKeyedTable(recorder, std::string{codes::kClosureContributionFeaturesPath},
                      left.closureContributions.features, right.closureContributions.features,
                      featureKey, featureKey);
    compareKeyedTable(
        recorder, std::string{codes::kResourceClosurePath}, left.resourceClosure.resources,
        right.resourceClosure.resources, [](const ResourceRef& ref) { return ref.resourceId; },
        [](const ResourceRef& ref) { return ref.resourceId; });
    compareKeyedTable(
        recorder, std::string{codes::kMergedNamespacePath}, left.mergedNamespace.declarations,
        right.mergedNamespace.declarations,
        [](const MergedDeclaration& declaration) {
            return declaration.stableId.sourceDocumentId + "#" +
                   std::to_string(declaration.stableId.declarationOrdinal);
        },
        renderMergedDeclaration);

    //  Deliberately not compared, and the omission is the contract (Spec 2.3, Spec 3.8.6 rule 3):
    //  the source closure and the diagnostic map. They carry toolchain, carrier and editor
    //  provenance, so comparing them would make two semantically equal graphs look different.

    return recorder.take();
}

auto equivalent(const CanonicalGameplayGraph& left, const CanonicalGameplayGraph& right) -> bool {
    return semanticDiff(left, right).empty();
}

} // namespace cuexis::judgement

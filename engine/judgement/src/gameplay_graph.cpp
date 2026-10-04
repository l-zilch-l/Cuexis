//  Judgement typed kernel - S7A-3 (first half) Canonical Gameplay Graph implementation.
//
//  This file is the graph half of S7A-3: the reference attribution of Spec 3.8.6, the declaration
//  namespace identity of P1-02, the static typed judgement domain of P1-01, the checked judgement
//  geometry of the ABI "units and ranges" rule, the closure derivations of Spec 6.1 and the
//  field-by-field semantic diff of Spec 3.5 / P1-15.
//
//  Four rules shape every function below.
//
//    1. The graph is a function of declared content. Every table order is derived from a stable
//       identity, never from an array position, so permuting an input cannot change the graph.
//
//    2. Nothing is inferred. A missing component is a stable rejection, never a default, and the
//       closure derivation only repeats tokens the content declared.
//
//    3. Geometry is exact or refused. The arithmetic below is integer-exact, reports the
//       budget_exceeded category when a declared value left the domain its declaration requires,
//       and clamps, saturates and truncates nowhere.
//
//    4. Equivalence is a field list, not a hash. semanticDiff names every differing field and skips
//       the diagnostic map and every physical order by construction.

#include <cuexis/judgement/gameplay_graph.hpp>

#include "source_codes.hpp"

#include <cuexis/judgement/diagnostic.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cuexis::judgement {

PatternNodeDeclaration::PatternNodeDeclaration(
    PatternPrimitive kind, std::vector<PatternNodeDeclaration> children, std::string atom,
    std::optional<RepeatBoundsDeclaration> bounds,
    std::optional<UnsupportedContentDeclaration> unsupported)
    : primitive(kind), operands(std::move(children)), atomRef(std::move(atom)),
      repeatBounds(std::move(bounds)), unsupportedForm(std::move(unsupported)) {}

PatternNodeDeclaration::PatternNodeDeclaration(const PatternNodeDeclaration& other) {
    struct Pair {
        PatternNodeDeclaration* destination;
        const PatternNodeDeclaration* source;
    };
    // Copy into an owning temporary so an allocation failure also drains iteratively.
    PatternNodeDeclaration copy;
    std::vector<Pair> pending{{&copy, &other}};
    while (!pending.empty()) {
        const auto [destination, source] = pending.back();
        pending.pop_back();
        destination->primitive = source->primitive;
        destination->atomRef = source->atomRef;
        destination->repeatBounds = source->repeatBounds;
        destination->unsupportedForm = source->unsupportedForm;
        destination->operands.resize(source->operands.size());
        for (std::size_t i = 0; i < source->operands.size(); ++i) {
            pending.push_back({&destination->operands[i], &source->operands[i]});
        }
    }
    *this = std::move(copy);
}

auto PatternNodeDeclaration::operator=(const PatternNodeDeclaration& other)
    -> PatternNodeDeclaration& {
    if (this != &other) {
        PatternNodeDeclaration copy{other};
        *this = std::move(copy);
    }
    return *this;
}

auto PatternNodeDeclaration::operator=(PatternNodeDeclaration&& other) noexcept
    -> PatternNodeDeclaration& {
    if (this != &other) {
        primitive = other.primitive;
        atomRef = std::move(other.atomRef);
        repeatBounds = std::move(other.repeatBounds);
        unsupportedForm = std::move(other.unsupportedForm);
        // Swapping leaves the old tree with an owning temporary for iterative destruction.
        PatternNodeDeclaration old;
        old.operands.swap(operands);
        operands.swap(other.operands);
    }
    return *this;
}

PatternNodeDeclaration::~PatternNodeDeclaration() noexcept {
    // Thread parent links through live nodes. No allocations and no depth-dependent call stack.
    auto* node = this;
    while (true) {
        if (!node->operands.empty()) {
            auto* child = &node->operands.back();
            child->destructionParent_ = node;
            node = child;
        } else if (node != this) {
            auto* parent = node->destructionParent_;
            parent->operands.pop_back();
            node = parent;
        } else {
            break;
        }
    }
}

auto operator==(const PatternNodeDeclaration& left, const PatternNodeDeclaration& right) -> bool {
    using Pair = std::pair<const PatternNodeDeclaration*, const PatternNodeDeclaration*>;
    std::vector<Pair> pending{{&left, &right}};
    while (!pending.empty()) {
        const auto [l, r] = pending.back();
        pending.pop_back();
        if (l->primitive != r->primitive || l->atomRef != r->atomRef ||
            l->repeatBounds != r->repeatBounds || l->unsupportedForm != r->unsupportedForm ||
            l->operands.size() != r->operands.size()) {
            return false;
        }
        for (std::size_t i = 0; i < l->operands.size(); ++i) {
            pending.emplace_back(&l->operands[i], &r->operands[i]);
        }
    }
    return true;
}

auto operator==(const PatternDeclaration& left, const PatternDeclaration& right) -> bool {
    return left.patternId == right.patternId && left.matchPolicy == right.matchPolicy &&
           left.root == right.root && left.required == right.required;
}

auto operator==(const RequirementRecord& left, const RequirementRecord& right) -> bool {
    return left.stableId == right.stableId && left.identity == right.identity &&
           left.requiredActions == right.requiredActions &&
           left.domainBinding == right.domainBinding &&
           left.judgementDomainId == right.judgementDomainId && left.phases == right.phases &&
           left.requiresReleaseTailSemantics == right.requiresReleaseTailSemantics &&
           left.pattern == right.pattern && left.maxArmElements == right.maxArmElements &&
           left.maxDeadlineElements == right.maxDeadlineElements && left.measure == right.measure &&
           left.resourceClaims == right.resourceClaims && left.grace == right.grace &&
           left.preparedGrace == right.preparedGrace && left.timing == right.timing &&
           left.patternArmRefs == right.patternArmRefs &&
           left.factBindingRefs == right.factBindingRefs &&
           left.solverProfileRef == right.solverProfileRef &&
           left.localClosePolicyToken == right.localClosePolicyToken &&
           left.required == right.required && left.unsupportedForms == right.unsupportedForms;
}

namespace {

constexpr std::int64_t kInt64Min = std::numeric_limits<std::int64_t>::min();
constexpr std::int64_t kInt64Max = std::numeric_limits<std::int64_t>::max();

//  ---------------------------------------------------------------------------------------------
//  Diagnostics
//  ---------------------------------------------------------------------------------------------

struct DiagnosticTokens final {
    std::string_view code;
    std::string_view category;
    std::string_view summary;
    std::string_view section;
    std::string_view path;
    std::string_view capabilityId = codes::kAbsent;
    std::string_view remediation = codes::kAbsent;
};

[[nodiscard]] auto rejection(const DiagnosticTokens& tokens) -> core::Error {
    const Diagnostic diagnostic{
        .code = tokens.code,
        .category = tokens.category,
        .severity = codes::kErrorSeverity,
        //  None of these rejections mutates session state, so none of them faults the session.
        .faulted = false,
        .summary = tokens.summary,
        .context =
            DiagnosticContext{
                .fieldPath = DiagnosticFieldPath{.section = tokens.section, .path = tokens.path},
                .requirement =
                    DiagnosticRequirementRef{.kind = codes::kAbsent, .identity = codes::kAbsent},
                .identity = DiagnosticIdentityComponent{.component = codes::kAbsent,
                                                        .token = codes::kAbsent},
                .budget = nullptr,
                .capabilityId = tokens.capabilityId,
                .remediation = tokens.remediation,
                .rawTime = codes::kAbsent,
            },
    };
    return toError(diagnostic);
}

//  A structurally incomplete declaration: a component its own contract requires is not declared
//  (Spec 9.3, first class).
[[nodiscard]] auto declarationInvalidError(std::string_view code, std::string_view section,
                                           std::string_view path, std::string_view summary)
    -> core::Error {
    return rejection(DiagnosticTokens{.code = code,
                                      .category = codes::kInvalidRelationCategory,
                                      .summary = summary,
                                      .section = section,
                                      .path = path});
}

//  A declared value that left the domain its declaration requires (Spec 9.3, second class).
[[nodiscard]] auto valueError(std::string_view code, std::string_view section,
                              std::string_view path, std::string_view summary) -> core::Error {
    return rejection(DiagnosticTokens{.code = code,
                                      .category = codes::kBudgetExceededCategory,
                                      .summary = summary,
                                      .section = section,
                                      .path = path});
}

//  A stable capability rejection. Spec 7.2 requires an R-01 to R-11 rejection to name the
//  capability and the replacement path, so both are passed explicitly and neither has a default.
[[nodiscard]] auto capabilityRejection(std::string_view code, std::string_view section,
                                       std::string_view path, std::string_view summary,
                                       std::string_view capabilityId, std::string_view remediation)
    -> core::Error {
    return rejection(DiagnosticTokens{.code = code,
                                      .category = codes::kCapabilityDisabledCategory,
                                      .summary = summary,
                                      .section = section,
                                      .path = path,
                                      .capabilityId = capabilityId,
                                      .remediation = remediation});
}

//  ---------------------------------------------------------------------------------------------
//  Rendering, for the field-level diff
//  ---------------------------------------------------------------------------------------------

[[nodiscard]] auto renderText(std::string_view text) -> std::string {
    return std::string{text};
}

[[nodiscard]] auto
renderRequirementCapacity(const std::optional<MeasuredParameter<std::uint64_t>>& value)
    -> std::string {
    const RequirementCapacityKey key = requirementCapacityKey(value);
    switch (key.state) {
    case RequirementCapacityState::absent:
        return "absent";
    case RequirementCapacityState::pending:
        return "pending";
    case RequirementCapacityState::measured:
        return "measured(" + std::to_string(key.value) + ")";
    }
    return "?";
}

[[nodiscard]] auto renderBool(bool value) -> std::string {
    return value ? std::string{"true"} : std::string{"false"};
}

template <typename Integer> [[nodiscard]] auto renderInteger(Integer value) -> std::string {
    return std::to_string(value);
}

//  A deterministic rendering of a token list. The separator is a single character that cannot occur
//  in a stable token, so a rendering of "a,b" can never be confused with one of "a" and "b".
[[nodiscard]] auto renderTokens(const std::vector<std::string>& tokens) -> std::string {
    std::string rendered;
    for (const auto& token : tokens) {
        if (!rendered.empty()) {
            rendered.push_back(',');
        }
        rendered.append(token);
    }
    return rendered;
}

[[nodiscard]] auto renderFeatureRefs(const std::vector<FeatureRef>& refs) -> std::string {
    std::vector<std::string> tokens;
    tokens.reserve(refs.size());
    for (const auto& ref : refs) {
        tokens.push_back(ref.featureId);
    }
    return renderTokens(tokens);
}

[[nodiscard]] auto renderCapabilityRefs(const std::vector<CapabilityRef>& refs) -> std::string {
    std::vector<std::string> tokens;
    tokens.reserve(refs.size());
    for (const auto& ref : refs) {
        tokens.push_back(ref.capabilityId + "@" + ref.revision);
    }
    return renderTokens(tokens);
}

[[maybe_unused, nodiscard]] auto renderResourceRefs(const std::vector<ResourceRef>& refs)
    -> std::string {
    std::vector<std::string> tokens;
    tokens.reserve(refs.size());
    for (const auto& ref : refs) {
        tokens.push_back(ref.resourceId);
    }
    return renderTokens(tokens);
}

[[nodiscard]] auto renderStableIds(const std::vector<StableDeclarationId>& ids) -> std::string {
    std::vector<std::string> tokens;
    tokens.reserve(ids.size());
    for (const auto& id : ids) {
        tokens.push_back(id.sourceDocumentId + "#" + std::to_string(id.declarationOrdinal));
    }
    return renderTokens(tokens);
}

[[nodiscard]] auto renderEmissionPath(const std::vector<EmissionPathStep>& path) -> std::string {
    std::vector<std::string> tokens;
    tokens.reserve(path.size());
    for (const auto& step : path) {
        tokens.push_back(step.nodeId + "#" + std::to_string(step.repeatIndex));
    }
    return renderTokens(tokens);
}

//  The canonical rendering of the six-tuple. It is the diff key of a requirement and never an
//  interchange identity: nothing outside this file reads it.
[[nodiscard]] auto renderRequirementIdentity(const RequirementIdentity& identity) -> std::string {
    return identity.chartEntryId + "/" + identity.invocationId + "/" + identity.moduleId + "/" +
           identity.exportId + "/" + renderEmissionPath(identity.emissionPath) + "/" +
           identity.requirementLocalId;
}

[[nodiscard]] auto renderPhaseKind(PhaseKind kind) -> std::string {
    switch (kind) {
    case PhaseKind::tap:
        return "tap";
    case PhaseKind::head:
        return "head";
    case PhaseKind::body:
        return "body";
    case PhaseKind::tail:
        return "tail";
    }
    return "?";
}

[[nodiscard]] auto renderGracePolicy(GraceResolutionPolicy policy) -> std::string {
    switch (policy) {
    case GraceResolutionPolicy::explicitDeclaration:
        return "explicit";
    case GraceResolutionPolicy::inheritedDeclaration:
        return "inherited";
    case GraceResolutionPolicy::defaultDeclaration:
        return "default";
    }
    return "?";
}

[[nodiscard]] auto renderPatternPrimitive(PatternPrimitive primitive) -> std::string {
    switch (primitive) {
    case PatternPrimitive::atom:
        return "atom";
    case PatternPrimitive::sequence:
        return "sequence";
    case PatternPrimitive::choice:
        return "choice";
    case PatternPrimitive::boundedRepeat:
        return "boundedRepeat";
    case PatternPrimitive::skip:
        return "skip";
    case PatternPrimitive::instant:
        return "instant";
    case PatternPrimitive::complement:
        return "complement";
    }
    return "?";
}

//  The declaration tree of a pattern, rendered deterministically. The compiled predicate comparison
//  is the second half; this rendering is the declaration-level evidence.
//
//  The rendering is an iterative pre-order over an explicit stack, so a deeply nested declaration
//  is rendered with the same host stack as a flat one and the rendering never becomes the depth
//  bound the declaration model deliberately does not have. The text is the text a recursive
//  pre-order produced: a node renders its primitive, its atom, its operands in declared order, then
//  its bounds and its unsupported form.
[[nodiscard]] auto renderPatternNode(const PatternNodeDeclaration& root) -> std::string {
    struct Frame final {
        const PatternNodeDeclaration* node{nullptr};
        std::size_t next{0};
    };
    std::string rendered;
    std::vector<Frame> stack;
    stack.push_back(Frame{.node = &root});
    while (!stack.empty()) {
        const PatternNodeDeclaration* node = stack.back().node;
        const std::size_t next = stack.back().next;
        if (next == 0U) {
            rendered.append(renderPatternPrimitive(node->primitive));
            rendered.push_back('(');
            if (!node->atomRef.empty()) {
                rendered.append("atom=");
                rendered.append(node->atomRef);
                rendered.push_back(';');
            }
            if (!node->operands.empty()) {
                rendered.push_back('[');
            }
        }
        if (next < node->operands.size()) {
            if (next != 0U) {
                rendered.push_back(',');
            }
            stack.back().next = next + 1U;
            stack.push_back(Frame{.node = &node->operands[next]});
            continue;
        }
        if (!node->operands.empty()) {
            rendered.push_back(']');
        }
        if (node->repeatBounds.has_value()) {
            rendered.append("bounds=");
            rendered.append(std::to_string(node->repeatBounds->minimum));
            rendered.push_back('.');
            rendered.append(std::to_string(node->repeatBounds->maximum));
        }
        if (node->unsupportedForm.has_value()) {
            rendered.append("unsupported=");
            rendered.append(node->unsupportedForm->declaredToken);
        }
        rendered.push_back(')');
        stack.pop_back();
    }
    return rendered;
}

//  ---------------------------------------------------------------------------------------------
//  Typed comparison of the two borrowed declarations
//  ---------------------------------------------------------------------------------------------

//  TimebaseProfile deliberately has no operator==, and this file does not add one, because the type
//  belongs to S7A-2. The semantic diff compares it member by member instead, which keeps the
//  comparison inside this batch.
[[nodiscard]] auto sameTimebase(const TimebaseProfile& left, const TimebaseProfile& right) -> bool {
    return left.profileId == right.profileId && left.unitToken == right.unitToken &&
           left.tickScale == right.tickScale && left.originBeat == right.originBeat &&
           left.initialTempo == right.initialTempo && left.tempoSections == right.tempoSections &&
           left.stopSections == right.stopSections;
}

[[nodiscard]] auto renderTimebase(const TimebaseProfile& profile) -> std::string {
    std::string rendered{profile.profileId};
    rendered.append("/");
    rendered.append(profile.unitToken);
    rendered.append("/scale=");
    rendered.append(std::to_string(profile.tickScale.numerator()));
    rendered.push_back('/');
    rendered.append(std::to_string(profile.tickScale.denominator()));
    rendered.append("/origin=");
    rendered.append(std::to_string(profile.originBeat.numerator()));
    rendered.push_back('/');
    rendered.append(std::to_string(profile.originBeat.denominator()));
    rendered.append("/tempo=");
    rendered.append(profile.initialTempo.has_value()
                        ? std::to_string(profile.initialTempo->numerator()) + "/" +
                              std::to_string(profile.initialTempo->denominator())
                        : std::string{"absent"});
    rendered.append("/tempoSections=");
    rendered.append(std::to_string(profile.tempoSections.size()));
    rendered.append("/stopSections=");
    rendered.append(std::to_string(profile.stopSections.size()));
    return rendered;
}

[[nodiscard]] auto renderLatePolicy(const LatePolicyParameters& parameters) -> std::string {
    const auto render = [](const MeasuredParameter<TickSpan>& parameter) -> std::string {
        const TickSpan* value = parameter.measuredValue();
        return value == nullptr ? std::string{"pending"} : std::to_string(value->value());
    };
    std::string rendered{"finalizationWatermark="};
    rendered.append(render(parameters.finalizationWatermark));
    rendered.append(",maxQueueHop=");
    rendered.append(render(parameters.maxQueueHop));
    rendered.append(",windowCloseThreshold=");
    rendered.append(render(parameters.windowCloseThreshold));
    rendered.append(",windowOpenThreshold=");
    rendered.append(render(parameters.windowOpenThreshold));
    rendered.append(",policy=");
    rendered.append(parameters.policy.has_value()
                        ? std::to_string(static_cast<unsigned>(*parameters.policy))
                        : std::string{"absent"});
    return rendered;
}

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
    for (const auto& value : left) {
        const std::string valueKey = key(value);
        const auto found = std::find_if(right.begin(), right.end(), [&](const Value& candidate) {
            return key(candidate) == valueKey;
        });
        if (found == right.end()) {
            recorder.add(path + "[" + valueKey + "]", render(value), "<absent>");
            continue;
        }
        if (!(value == *found)) {
            recorder.add(path + "[" + valueKey + "]", render(value), render(*found));
        }
    }
    for (const auto& value : right) {
        const std::string valueKey = key(value);
        const auto found = std::find_if(left.begin(), left.end(), [&](const Value& candidate) {
            return key(candidate) == valueKey;
        });
        if (found == left.end()) {
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
    for (const auto& requirement : left) {
        const std::string key = renderRequirementIdentity(requirement.identity);
        const auto found =
            std::find_if(right.begin(), right.end(), [&](const RequirementRecord& candidate) {
                return renderRequirementIdentity(candidate.identity) == key;
            });
        if (found == right.end()) {
            recorder.add(path + "[" + key + "]", renderRequirementIdentity(requirement.identity),
                         "<absent>");
            continue;
        }
        compareRequirement(recorder, path + "[" + key + "]", requirement, *found);
    }
    for (const auto& requirement : right) {
        const std::string key = renderRequirementIdentity(requirement.identity);
        const auto found =
            std::find_if(left.begin(), left.end(), [&](const RequirementRecord& candidate) {
                return renderRequirementIdentity(candidate.identity) == key;
            });
        if (found == left.end()) {
            recorder.add(path + "[" + key + "]", "<absent>",
                         renderRequirementIdentity(requirement.identity));
        }
    }
}

} // namespace

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

//  ---------------------------------------------------------------------------------------------
//  Reference attribution (Spec 3.8.6)
//  ---------------------------------------------------------------------------------------------

auto declareReference(ReferenceKind kind, ReferenceClosure requested)
    -> core::Result<ReferenceClosure> {
    const ReferenceClosure owned = closureOf(kind);
    if (owned != requested) {
        return core::unexpected(declarationInvalidError(
            codes::kDeclarationIncompleteCode, codes::kGraphSection, codes::kReferencesPath,
            "a gameplay reference belongs to exactly one closure class and cannot be registered in "
            "another"));
    }
    return owned;
}

//  ---------------------------------------------------------------------------------------------
//  Fact category spelling (Spec 3.20 rule 3)
//  ---------------------------------------------------------------------------------------------

auto factCategoryToken(FactCategory category) noexcept -> std::string_view {
    switch (category) {
    case FactCategory::tap:
        return "tap";
    case FactCategory::holdHead:
        return "hold_head";
    case FactCategory::holdBody:
        return "hold_body";
    case FactCategory::holdTail:
        return "hold_tail";
    }
    return "?";
}

//  ---------------------------------------------------------------------------------------------
//  Declaration-scoped names (P2-05)
//  ---------------------------------------------------------------------------------------------

auto RequiredActionRef::fromToken(std::string token) -> core::Result<RequiredActionRef> {
    if (token.empty()) {
        return core::unexpected(declarationInvalidError(
            codes::kDeclarationIncompleteCode, codes::kGraphSection, codes::kRequiredActionsPath,
            "a required action must be declared: an empty token is not a default action"));
    }
    return RequiredActionRef{std::move(token)};
}

auto DomainBindingRef::fromToken(std::string token) -> core::Result<DomainBindingRef> {
    if (token.empty()) {
        return core::unexpected(declarationInvalidError(
            codes::kDeclarationIncompleteCode, codes::kGraphSection, codes::kDomainBindingPath,
            "a domain binding must be declared: an empty token is not a default domain"));
    }
    return DomainBindingRef{std::move(token)};
}

//  ---------------------------------------------------------------------------------------------
//  Judgement domain (P1-01)
//  ---------------------------------------------------------------------------------------------

auto validateJudgementDomain(const JudgementDomainRecord& domain) -> core::Result<void> {
    if (domain.domainId.empty()) {
        return core::unexpected(declarationInvalidError(
            codes::kDeclarationIncompleteCode, codes::kDomainSection, codes::kDomainIdPath,
            "a judgement domain must declare its identity"));
    }
    if (domain.coordinateSystemToken.empty()) {
        return core::unexpected(declarationInvalidError(
            codes::kDeclarationIncompleteCode, codes::kDomainSection, codes::kCoordinateSystemPath,
            "a judgement domain must declare its coordinate system; there is no inherited one"));
    }
    if (domain.axes.empty()) {
        return core::unexpected(declarationInvalidError(
            codes::kDomainRangeUndeclaredCode, codes::kDomainSection, codes::kAxesPath,
            "a judgement domain must declare the range of its axes; an empty axis table would be "
            "an "
            "implicit unbounded range"));
    }
    for (std::size_t index = 0; index < domain.axes.size(); ++index) {
        const JudgementAxisRange& axis = domain.axes[index];
        if (axis.axisToken.empty()) {
            return core::unexpected(declarationInvalidError(
                codes::kDeclarationIncompleteCode, codes::kDomainSection, codes::kAxisTokenPath,
                "a judgement axis must be named: the coordinate system is declared inside the "
                "domain, so its axes cannot be positional"));
        }
        for (std::size_t other = index + 1; other < domain.axes.size(); ++other) {
            if (domain.axes[other].axisToken == axis.axisToken) {
                return core::unexpected(declarationInvalidError(
                    codes::kDeclarationDuplicateCode, codes::kDomainSection, codes::kAxesPath,
                    "a judgement axis may be declared once: two ranges for one axis would be a "
                    "container-order decision"));
            }
        }
        if (axis.minimum > axis.maximum) {
            return core::unexpected(valueError(
                codes::kDomainRangeReversedCode, codes::kDomainSection, codes::kAxesPath,
                "a declared judgement axis range is reversed: its maximum precedes its minimum"));
        }
    }
    if (domain.frameResolution == FrameResolution::dynamicRuntimeFrame) {
        return core::unexpected(capabilityRejection(
            codes::kDynamicFrameUnsupportedCode, codes::kDomainSection, codes::kFrameResolutionPath,
            "a judgement domain must declare its frame statically inside the domain: a dynamically "
            "resolved frame is registered as a later-batch candidate and is rejected here",
            codes::kDynamicFrameCapabilityId, codes::kLaterBatchRemediation));
    }
    return {};
}

//  ---------------------------------------------------------------------------------------------
//  Checked judgement geometry (ABI units and ranges)
//  ---------------------------------------------------------------------------------------------

auto geometryAdd(std::int64_t left, std::int64_t right) -> core::Result<std::int64_t> {
    if (right > 0 && left > kInt64Max - right) {
        return core::unexpected(valueError(codes::kDomainGeometryOverflowCode,
                                           codes::kDomainSection, codes::kAxesPath,
                                           "a judgement geometry sum is not representable in the "
                                           "signed 64-bit domain"));
    }
    if (right < 0 && left < kInt64Min - right) {
        return core::unexpected(valueError(codes::kDomainGeometryOverflowCode,
                                           codes::kDomainSection, codes::kAxesPath,
                                           "a judgement geometry sum is not representable in the "
                                           "signed 64-bit domain"));
    }
    return static_cast<std::int64_t>(left + right);
}

auto geometryMultiply(std::int64_t left, std::int64_t right) -> core::Result<std::int64_t> {
    if (left == 0 || right == 0) {
        return std::int64_t{0};
    }
    const bool negative = (left < 0) != (right < 0);
    const std::uint64_t limit = negative ? static_cast<std::uint64_t>(kInt64Max) + 1U
                                         : static_cast<std::uint64_t>(kInt64Max);
    const std::uint64_t leftMagnitude =
        left < 0 ? (~static_cast<std::uint64_t>(left) + 1U) : static_cast<std::uint64_t>(left);
    const std::uint64_t rightMagnitude =
        right < 0 ? (~static_cast<std::uint64_t>(right) + 1U) : static_cast<std::uint64_t>(right);
    if (leftMagnitude > limit / rightMagnitude) {
        return core::unexpected(
            valueError(codes::kDomainGeometryOverflowCode, codes::kDomainSection, codes::kAxesPath,
                       "a judgement geometry product is not representable in the "
                       "signed 64-bit domain"));
    }
    const std::uint64_t product = leftMagnitude * rightMagnitude;
    if (!negative) {
        return static_cast<std::int64_t>(product);
    }
    if (product == static_cast<std::uint64_t>(kInt64Max) + 1U) {
        return kInt64Min;
    }
    return -static_cast<std::int64_t>(product);
}

auto geometrySquare(std::int64_t value) -> core::Result<std::int64_t> {
    return geometryMultiply(value, value);
}

auto axisExtent(const JudgementAxisRange& axis) -> core::Result<std::int64_t> {
    if (axis.minimum > axis.maximum) {
        return core::unexpected(
            valueError(codes::kDomainRangeReversedCode, codes::kDomainSection, codes::kAxesPath,
                       "a declared judgement axis range is reversed: its maximum "
                       "precedes its minimum"));
    }
    //  With minimum <= maximum the unsigned difference is the exact extent, so it can be compared
    //  against the representable limit without any signed intermediate that could overflow.
    const std::uint64_t extent =
        static_cast<std::uint64_t>(axis.maximum) - static_cast<std::uint64_t>(axis.minimum);
    if (extent > static_cast<std::uint64_t>(kInt64Max)) {
        return core::unexpected(
            valueError(codes::kDomainGeometryOverflowCode, codes::kDomainSection, codes::kAxesPath,
                       "a declared judgement axis extent is not representable in "
                       "the signed 64-bit domain"));
    }
    return static_cast<std::int64_t>(extent);
}

auto narrowToAxis(const JudgementAxisRange& axis, std::int64_t value)
    -> core::Result<std::int64_t> {
    //  The axis has to be a usable declaration before a value can be narrowed into it, so an extent
    //  that cannot be represented is rejected here as well.
    const auto extent = axisExtent(axis);
    if (!extent.has_value()) {
        return core::unexpected(extent.error());
    }
    if (value < axis.minimum || value > axis.maximum) {
        return core::unexpected(valueError(codes::kDomainNarrowingRejectedCode,
                                           codes::kDomainSection, codes::kAxesPath,
                                           "a canonical value lies outside the range its judgement "
                                           "axis declared; it is refused rather than clamped"));
    }
    return value;
}

//  ---------------------------------------------------------------------------------------------
//  Canonical table order
//  ---------------------------------------------------------------------------------------------

auto canonicalCompare(const RequirementRecord& left, const RequirementRecord& right) noexcept
    -> std::strong_ordering {
    if (const auto order = left.identity <=> right.identity; order != 0) {
        return order;
    }
    return left.stableId <=> right.stableId;
}

auto canonicalCompare(const ResourceRecord& left, const ResourceRecord& right) noexcept
    -> std::strong_ordering {
    if (const auto order = left.ref <=> right.ref; order != 0) {
        return order;
    }
    return left.slotToken <=> right.slotToken;
}

auto canonicalCompare(const RelationDeclaration& left, const RelationDeclaration& right) noexcept
    -> std::strong_ordering {
    if (const auto order = left.resourceRef <=> right.resourceRef; order != 0) {
        return order;
    }
    if (left.kind != right.kind) {
        return static_cast<unsigned>(left.kind) < static_cast<unsigned>(right.kind)
                   ? std::strong_ordering::less
                   : std::strong_ordering::greater;
    }
    if (const auto order = left.members <=> right.members; order != 0) {
        return order;
    }
    return left.policy.policyToken <=> right.policy.policyToken;
}

auto canonicalCompare(const SolverProfileDeclaration& left,
                      const SolverProfileDeclaration& right) noexcept -> std::strong_ordering {
    if (const auto order = left.solverId <=> right.solverId; order != 0) {
        return order;
    }
    return left.revision <=> right.revision;
}

auto canonicalCompare(const JudgementDomainRecord& left,
                      const JudgementDomainRecord& right) noexcept -> std::strong_ordering {
    if (const auto order = left.domainId <=> right.domainId; order != 0) {
        return order;
    }
    return left.coordinateSystemToken <=> right.coordinateSystemToken;
}

auto canonicalCompare(const MergedDeclaration& left, const MergedDeclaration& right) noexcept
    -> std::strong_ordering {
    if (const auto order = left.stableId <=> right.stableId; order != 0) {
        return order;
    }
    return left.name <=> right.name;
}

//  ---------------------------------------------------------------------------------------------
//  Closure derivation (Spec 6.1)
//  ---------------------------------------------------------------------------------------------

namespace {

void collectCapabilities(const RequiredRefs& required, std::vector<CapabilityRef>& into) {
    into.insert(into.end(), required.capabilities.begin(), required.capabilities.end());
}

void collectFeatures(const RequiredRefs& required, std::vector<FeatureRef>& into) {
    into.insert(into.end(), required.features.begin(), required.features.end());
}

void sortUniqueCapabilities(std::vector<CapabilityRef>& refs) {
    std::sort(refs.begin(), refs.end());
    refs.erase(std::unique(refs.begin(), refs.end()), refs.end());
}

void sortUniqueFeatures(std::vector<FeatureRef>& refs) {
    std::sort(refs.begin(), refs.end());
    refs.erase(std::unique(refs.begin(), refs.end()), refs.end());
}

void sortUniqueResources(std::vector<ResourceRef>& refs) {
    std::sort(refs.begin(), refs.end());
    refs.erase(std::unique(refs.begin(), refs.end()), refs.end());
}

} // namespace

auto deriveCapabilityClosure(const CanonicalGameplayGraph& graph) -> DerivedCapabilityClosure {
    std::vector<CapabilityRef> refs;
    //  The declared set of the source side is deliberately not an input: the declared set and the
    //  derived closure are two separately queryable things and no operation merges them (ABI domain
    //  8). Only the needs stated by ruleset, presentation and content take part.
    refs.insert(refs.end(), graph.closureContributions.capabilities.begin(),
                graph.closureContributions.capabilities.end());
    for (const auto& requirement : graph.requirements) {
        collectCapabilities(requirement.required, refs);
        collectCapabilities(requirement.pattern.required, refs);
        collectCapabilities(requirement.measure.required, refs);
    }
    for (const auto& resource : graph.resources) {
        collectCapabilities(resource.required, refs);
    }
    for (const auto& relation : graph.relations) {
        collectCapabilities(relation.required, refs);
    }
    for (const auto& profile : graph.solverProfiles) {
        collectCapabilities(profile.required, refs);
    }
    for (const auto& domain : graph.judgementDomains) {
        collectCapabilities(domain.required, refs);
    }
    for (const auto& declaration : graph.mergedNamespace.declarations) {
        for (const auto& capabilityId : declaration.requiredCapabilityIds) {
            refs.push_back(CapabilityRef{.capabilityId = capabilityId, .revision = {}});
        }
    }
    sortUniqueCapabilities(refs);
    return DerivedCapabilityClosure{.capabilities = std::move(refs)};
}

auto deriveFeatureClosure(const CanonicalGameplayGraph& graph) -> FeatureClosure {
    std::vector<FeatureRef> refs;
    refs.insert(refs.end(), graph.closureContributions.features.begin(),
                graph.closureContributions.features.end());
    for (const auto& requirement : graph.requirements) {
        collectFeatures(requirement.required, refs);
        collectFeatures(requirement.pattern.required, refs);
        collectFeatures(requirement.measure.required, refs);
    }
    for (const auto& resource : graph.resources) {
        collectFeatures(resource.required, refs);
    }
    for (const auto& relation : graph.relations) {
        collectFeatures(relation.required, refs);
    }
    for (const auto& profile : graph.solverProfiles) {
        collectFeatures(profile.required, refs);
    }
    for (const auto& domain : graph.judgementDomains) {
        collectFeatures(domain.required, refs);
    }
    for (const auto& declaration : graph.mergedNamespace.declarations) {
        for (const auto& featureId : declaration.requiredFeatureIds) {
            refs.push_back(FeatureRef{.featureId = featureId});
        }
    }
    sortUniqueFeatures(refs);
    return FeatureClosure{.features = std::move(refs)};
}

auto deriveResourceClosure(const CanonicalGameplayGraph& graph) -> ResourceClosure {
    std::vector<ResourceRef> refs;
    for (const auto& requirement : graph.requirements) {
        for (const auto& claim : requirement.resourceClaims) {
            refs.push_back(claim.resourceRef);
        }
    }
    for (const auto& relation : graph.relations) {
        refs.push_back(relation.resourceRef);
    }
    sortUniqueResources(refs);
    return ResourceClosure{.resources = std::move(refs)};
}

//  ---------------------------------------------------------------------------------------------
//  Field-by-field semantic diff (Spec 3.5, P1-15)
//  ---------------------------------------------------------------------------------------------

auto semanticDiff(const CanonicalGameplayGraph& left, const CanonicalGameplayGraph& right)
    -> std::vector<GraphFieldDifference> {
    DifferenceRecorder recorder;

    compareValue(recorder, std::string{codes::kGameplayVersionPath}, left.gameplayVersion,
                 right.gameplayVersion, renderInteger<std::uint32_t>);
    compareValue(recorder, std::string{codes::kGraphRevisionPath}, left.graphRevision,
                 right.graphRevision, renderInteger<std::uint64_t>);
    compareValue(recorder, std::string{codes::kRulesetRefPath}, left.rulesetRef, right.rulesetRef,
                 renderText);

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

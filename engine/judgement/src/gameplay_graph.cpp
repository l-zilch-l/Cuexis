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
           left.atomBindings == right.atomBindings &&
           left.independentCompetition == right.independentCompetition &&
           left.patternArmRefs == right.patternArmRefs &&
           left.factBindingRefs == right.factBindingRefs &&
           left.solverProfileRef == right.solverProfileRef &&
           left.localClosePolicyToken == right.localClosePolicyToken &&
           left.required == right.required && left.unsupportedForms == right.unsupportedForms;
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

auto collectCapabilityClosure(const CanonicalGameplayGraph& graph,
                              const ClosureContributions& contributions)
    -> DerivedCapabilityClosure {
    std::vector<CapabilityRef> refs;
    //  The declared set of the source side is deliberately not an input: the declared set and the
    //  derived closure are two separately queryable things and no operation merges them (ABI domain
    //  8). Only the needs stated by ruleset, presentation and content take part.
    refs.insert(refs.end(), contributions.capabilities.begin(), contributions.capabilities.end());
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

auto collectFeatureClosure(const CanonicalGameplayGraph& graph,
                           const ClosureContributions& contributions) -> FeatureClosure {
    std::vector<FeatureRef> refs;
    refs.insert(refs.end(), contributions.features.begin(), contributions.features.end());
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

} // namespace

auto deriveCapabilityClosure(const CanonicalGameplayGraph& graph) -> DerivedCapabilityClosure {
    return collectCapabilityClosure(graph, graph.closureContributions);
}

auto deriveFeatureClosure(const CanonicalGameplayGraph& graph) -> FeatureClosure {
    return collectFeatureClosure(graph, graph.closureContributions);
}

auto detail::deriveContentCapabilityClosure(const CanonicalGameplayGraph& graph)
    -> DerivedCapabilityClosure {
    return collectCapabilityClosure(graph, {});
}

auto detail::deriveContentFeatureClosure(const CanonicalGameplayGraph& graph) -> FeatureClosure {
    return collectFeatureClosure(graph, {});
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

} // namespace cuexis::judgement

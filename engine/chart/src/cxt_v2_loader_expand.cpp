#include <cuexis/chart/cxt_v2_loader.hpp>

#include "cxt_v2_loader_units_internal.hpp"

#include <cuexis/core/diagnostic.hpp>
#include <cuexis/core/error.hpp>
#include <cuexis/core/result.hpp>
#include <cuexis/json/parse.hpp>
#include <cuexis/json/value.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cuexis::chart {

[[nodiscard]] auto asInteger(const CxtV2Value& value) -> std::optional<std::int64_t> {
    if (const auto* result = std::get_if<std::int64_t>(&value))
        return *result;
    return std::nullopt;
}

[[nodiscard]] auto asBeat(const CxtV2Value& value) -> std::optional<RationalBeat> {
    if (const auto* result = std::get_if<RationalBeat>(&value)) {
        return *result;
    }
    if (const auto* integerValue = std::get_if<std::int64_t>(&value)) {
        auto beat = RationalBeat::create(*integerValue, 1);
        if (!beat) {
            return std::nullopt;
        }
        return *beat;
    }
    return std::nullopt;
}

[[nodiscard]] auto valueMatches(const CxtV2Value& value, ValueKind kind) noexcept -> bool {
    return (kind == ValueKind::Integer && std::holds_alternative<std::int64_t>(value)) ||
           (kind == ValueKind::Beat && std::holds_alternative<RationalBeat>(value));
}

[[nodiscard]] auto evalSource(const Source& source, const Bindings& parameters,
                              const Bindings& slots, const Bindings& indices,
                              core::Diagnostics& diagnostics) -> std::optional<CxtV2Value> {
    if (source.kind == SourceKind::Literal)
        return source.literal;
    const Bindings* scope = source.kind == SourceKind::Parameter ? &parameters
                            : source.kind == SourceKind::Slot    ? &slots
                                                                 : &indices;
    if (source.kind == SourceKind::Parameter || source.kind == SourceKind::Slot ||
        source.kind == SourceKind::Index) {
        const auto found = scope->find(source.id);
        if (found == scope->end()) {
            addCxtError(diagnostics, "cxt.v2.reference_missing",
                        "Value source reference is unbound", source.id);
            return std::nullopt;
        }
        return found->second;
    }
    if (!source.input || !source.scale || !source.offset)
        return std::nullopt;
    auto input = evalSource(**source.input, parameters, slots, indices, diagnostics);
    auto scale = evalSource(**source.scale, parameters, slots, indices, diagnostics);
    auto offset = evalSource(**source.offset, parameters, slots, indices, diagnostics);
    if (!input || !scale || !offset)
        return std::nullopt;
    if (const auto i = asInteger(*input)) {
        const auto s = asInteger(*scale);
        const auto o = asInteger(*offset);
        if (s && o) {
            auto product = checkedMultiply(*i, *s);
            if (!product) {
                addCxtError(diagnostics, std::string{product.error().code()},
                            std::string{product.error().message()}, "$");
                return std::nullopt;
            }
            auto sum = checkedAdd(*product, *o);
            if (!sum) {
                addCxtError(diagnostics, std::string{sum.error().code()},
                            std::string{sum.error().message()}, "$");
                return std::nullopt;
            }
            return *sum;
        }
    }
    const auto i = asBeat(*input);
    const auto s = asBeat(*scale);
    const auto o = asBeat(*offset);
    if (!i || !s || !o) {
        addCxtError(diagnostics, "cxt.v2.affine_type_mismatch",
                    "Affine source types are incompatible", "$");
        return std::nullopt;
    }
    auto product = multiplyRationalBeats(*i, *s);
    if (!product) {
        addCxtError(diagnostics, std::string{product.error().code()},
                    std::string{product.error().message()}, "$");
        return std::nullopt;
    }
    auto result = addRationalBeats(*product, *o);
    if (!result) {
        addCxtError(diagnostics, std::string{result.error().code()},
                    std::string{result.error().message()}, "$");
        return std::nullopt;
    }
    return *result;
}

[[nodiscard]] auto countFor(const Source& source, const Bindings& parameters,
                            core::Diagnostics& diagnostics) -> std::optional<std::size_t> {
    Bindings empty;
    auto value = evalSource(source, parameters, empty, empty, diagnostics);
    if (!value)
        return std::nullopt;
    const auto count = asInteger(*value);
    if (!count || *count < 0) {
        addCxtError(diagnostics, "cxt.v2.repeat_count_invalid",
                    "Repeat count must be a non-negative integer", "$");
        return std::nullopt;
    }
    return static_cast<std::size_t>(*count);
}

[[nodiscard]] auto valueMatches(const CxtV2Value& value, const Slot& slot) -> bool {
    if (slot.kind == ValueKind::Integer) {
        return std::holds_alternative<std::int64_t>(value) &&
               inDeclaredRange(value, slot.minimum, slot.maximum);
    }
    return std::holds_alternative<RationalBeat>(value) &&
           inDeclaredRange(value, slot.minimum, slot.maximum);
}

[[nodiscard]] auto identityPathLess(const std::vector<SemanticIdentityStep>& left,
                                    const std::vector<SemanticIdentityStep>& right) noexcept
    -> bool {
    const auto count = std::min(left.size(), right.size());
    for (std::size_t index = 0; index < count; ++index) {
        if (left[index].nodeId != right[index].nodeId)
            return left[index].nodeId < right[index].nodeId;
        if (left[index].iterationIndexPlusOne != right[index].iterationIndexPlusOne)
            return left[index].iterationIndexPlusOne < right[index].iterationIndexPlusOne;
    }
    return left.size() < right.size();
}

void expandNodes(const std::vector<Node>& nodes, const Module& module, const Pattern& pattern,
                 const CxtV2Invocation& invocation, const Bindings& parameters, Bindings& indices,
                 std::vector<SemanticIdentityStep>& path, ExpansionState& state,
                 core::Diagnostics& diagnostics, const ChartLimits& limits, std::size_t depth) {
    if (depth > limits.maxCxtV2RepeatDepth) {
        addCxtError(diagnostics, "cxt.v2.budget.repeat_depth",
                    "Repeat depth exceeds configured budget", pattern.id);
        return;
    }
    std::vector<const Node*> ordered;
    ordered.reserve(nodes.size());
    for (const auto& node : nodes)
        ordered.push_back(&node);
    std::ranges::sort(ordered, {}, [](const Node* node) { return node->nodeId; });
    for (const auto* node : ordered) {
        if (node->op == Node::Op::Repeat) {
            auto count = countFor(node->count, parameters, diagnostics);
            if (!count)
                continue;
            if (*count > limits.maxCxtV2ExpansionEntities || *count > 100000000U) {
                addCxtError(diagnostics, "cxt.v2.budget.expansion",
                            "Repeat expansion exceeds configured budget", node->nodeId);
                continue;
            }
            for (std::size_t i = 0; i < *count; ++i) {
                indices[node->indexId] = static_cast<std::int64_t>(i);
                path.push_back(
                    SemanticIdentityStep{node->nodeId, static_cast<std::uint32_t>(i + 1U)});
                expandNodes(node->body, module, pattern, invocation, parameters, indices, path,
                            state, diagnostics, limits, depth + 1U);
                path.pop_back();
            }
            indices.erase(node->indexId);
            continue;
        }
        const auto prototypeIt = module.prototypes.find(node->prototype);
        if (prototypeIt == module.prototypes.end()) {
            addCxtError(diagnostics, "cxt.v2.prototype_missing",
                        "Emit references a missing prototype", node->prototype);
            continue;
        }
        const auto& prototype = prototypeIt->second;
        Bindings slots;
        for (const auto& [slotId, slot] : prototype.slots) {
            const auto binding = node->bindings.find(slotId);
            if (binding != node->bindings.end()) {
                auto value = evalSource(binding->second, parameters, slots, indices, diagnostics);
                if (value) {
                    if (valueMatches(*value, slot))
                        slots.emplace(slotId, *value);
                    else
                        addCxtError(diagnostics, "cxt.v2.slot_type_invalid",
                                    "Slot binding type or range does not match declaration",
                                    slotId);
                }
            } else if (slot.defaultValue)
                slots.emplace(slotId, *slot.defaultValue);
            else if (slot.required)
                addCxtError(diagnostics, "cxt.v2.slot_missing", "Required slot is not bound",
                            slotId);
        }
        for (const auto& [slotId, unused] : node->bindings)
            if (prototype.slots.find(slotId) == prototype.slots.end()) {
                static_cast<void>(unused);
                addCxtError(diagnostics, "cxt.v2.slot_unknown", "Emit binds an unknown slot",
                            slotId);
            }
        const auto scopePath = path;
        path.push_back(SemanticIdentityStep{node->nodeId, 0U});
        CanonicalEntity entity;
        entity.identity = GeneratedEntityIdentity{invocation.chartId, invocation.bindingId,
                                                  module.id, invocation.exportId, path};
        if (node->parentKind == Node::ParentKind::Root)
            entity.parent = std::nullopt;
        else if (node->parentKind == Node::ParentKind::Invocation)
            entity.parent = invocation.parent;
        if (prototype.hasTransform) {
            CanonicalTransform transform;
            if (prototype.positionX) {
                if (auto value =
                        evalSource(*prototype.positionX, parameters, slots, indices, diagnostics)) {
                    if (auto x = asInteger(*value)) {
                        transform.position.x = static_cast<float>(*x);
                    } else {
                        addCxtError(diagnostics, "cxt.v2.affine_type_mismatch",
                                    "Transform position[0] must evaluate to an integer",
                                    prototype.id);
                    }
                }
            }
            entity.components.emplace_back(transform);
        }
        for (const auto& requirement : prototype.requirements) {
            auto beat = evalSource(requirement.beat, parameters, slots, indices, diagnostics);
            auto lane = evalSource(requirement.lane, parameters, slots, indices, diagnostics);
            if (!beat || !lane)
                continue;
            auto localBeat = asBeat(*beat);
            auto laneValue = asInteger(*lane);
            if (!localBeat) {
                addCxtError(diagnostics, "cxt.v2.affine_type_mismatch",
                            "Requirement startBeat must evaluate to a beat", requirement.id);
                continue;
            }
            if (!laneValue || *laneValue < 0 || *laneValue > 3) {
                addCxtError(diagnostics, "cxt.v2.lane_out_of_range",
                            "lane must be in candidate.lanes4 range 0..3", requirement.id);
                continue;
            }
            auto absolute = addRationalBeats(*localBeat, invocation.startBeat);
            if (!absolute) {
                addCxtError(diagnostics, std::string{absolute.error().code()},
                            std::string{absolute.error().message()}, requirement.id);
                continue;
            }
            CanonicalRequirement req;
            req.localId = requirement.id;
            req.kind = CanonicalRequirementKind::Tap;
            req.interval.startBeat = *absolute;
            req.judgementDomain = {"judgement-domain", "candidate.lanes4"};
            req.requiredAction = {"action", "press"};
            req.constraints.emplace_back(LaneConstraint{static_cast<std::uint32_t>(*laneValue)});
            entity.requirements.push_back(std::move(req));
            ++state.requirementCount;
        }
        if (state.requirementCount > limits.maxCxtV2ExpansionRequirements) {
            addCxtError(diagnostics, "cxt.v2.budget.requirements",
                        "Expanded requirement count exceeds configured budget", node->nodeId);
        }
        const auto entityIndex = state.chart.entities.size();
        state.chart.entities.push_back(std::move(entity));
        if (node->parentKind == Node::ParentKind::Emission)
            state.pendingParents.push_back(
                {entityIndex, node->parentKind, node->parentNodeId, scopePath});
        path.pop_back();
    }
}

} // namespace cuexis::chart

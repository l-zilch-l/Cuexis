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

auto CxtV2Loader::expand(std::string_view jsonText, const CxtV2Invocation& invocation,
                         const ChartLimits& limits) -> CxtV2ExpansionResult {
    CxtV2ExpansionResult result;
    core::Diagnostics diagnostics{limits.maxDiagnostics,
                                  core::Diagnostic{core::DiagnosticSeverity::Error,
                                                   "cxt.v2.diagnostics.limit",
                                                   "CXT v2 diagnostic limit reached"}};
    const auto inputLimit = std::min(limits.maxInputBytes, limits.maxAnimationTemplateBytes);
    auto parsed = json::parse(
        jsonText, json::ParseLimits{inputLimit, limits.maxNestingDepth, limits.maxStringBytes});
    if (!parsed) {
        addCxtError(diagnostics, std::string{parsed.error().code()},
                    std::string{parsed.error().message()}, "$");
        result.diagnostics = std::move(diagnostics);
        return result;
    }
    auto module = parseModule(*parsed, diagnostics, limits);
    if (!module) {
        result.diagnostics = std::move(diagnostics);
        return result;
    }
    if (module->id != invocation.moduleId) {
        addCxtError(diagnostics, "cxt.v2.module_mismatch",
                    "Invocation module does not match moduleId", "$/moduleId");
    }
    Bindings parameters;
    for (const auto& [id, parameter] : module->parameters)
        parameters.emplace(id, parameter.defaultValue);
    std::set<std::string> supplied;
    for (const auto& binding : invocation.parameters) {
        const auto parameter = module->parameters.find(binding.id);
        if (parameter == module->parameters.end() || !supplied.insert(binding.id).second) {
            addCxtError(diagnostics, "cxt.v2.parameter_invalid",
                        "Unknown or duplicate parameter binding", binding.id);
            continue;
        }
        parameters[binding.id] = binding.value;
    }
    if (invocation.exportId.empty() || invocation.exportId != module->exportId)
        addCxtError(diagnostics, "cxt.v2.export_mismatch",
                    "Invocation export does not match module export", "$/exports/0/id");
    for (const auto& binding : invocation.parameters) {
        const auto parameter = module->parameters.find(binding.id);
        if (parameter != module->parameters.end()) {
            if (!valueMatches(binding.value, parameter->second.kind)) {
                addCxtError(diagnostics, "cxt.v2.parameter_type_invalid",
                            "Parameter binding type does not match declaration", binding.id);
                continue;
            }
            if (!inDeclaredRange(binding.value, parameter->second.minimum,
                                 parameter->second.maximum)) {
                addCxtError(diagnostics, "cxt.v2.parameter_range_invalid",
                            "Parameter binding is outside the declared range", binding.id);
            }
        }
    }
    Pattern syntheticPattern;
    const Pattern* pattern = nullptr;
    if (module->moduleKind == "pattern") {
        if (!invocation.slotBindings.empty())
            addCxtError(diagnostics, "cxt.v2.slot_bindings_invalid",
                        "Pattern invocations must not provide slot bindings", "$/slotBindings");
        const auto patternIt = module->patterns.find(module->exportId);
        if (patternIt == module->patterns.end()) {
            addCxtError(diagnostics, "cxt.v2.exports_invalid", "Pattern export is missing",
                        "$/exports/0/id");
        } else {
            pattern = &patternIt->second;
        }
    } else if (module->moduleKind == "prototype") {
        if (module->prototypes.find(module->exportId) == module->prototypes.end()) {
            addCxtError(diagnostics, "cxt.v2.exports_invalid", "Prototype export is missing",
                        "$/exports/0/id");
        } else {
            Node direct;
            direct.op = Node::Op::Emit;
            direct.nodeId = "__direct__";
            direct.prototype = module->exportId;
            direct.parentKind =
                invocation.parent ? Node::ParentKind::Invocation : Node::ParentKind::Root;
            std::set<std::string> suppliedSlots;
            for (const auto& binding : invocation.slotBindings) {
                const auto slotIt = module->prototypes.at(module->exportId).slots.find(binding.id);
                if (slotIt == module->prototypes.at(module->exportId).slots.end() ||
                    !suppliedSlots.insert(binding.id).second) {
                    addCxtError(diagnostics, "cxt.v2.slot_invalid",
                                "Unknown or duplicate prototype slot binding", binding.id);
                    continue;
                }
                if (!valueMatches(binding.value, slotIt->second)) {
                    addCxtError(diagnostics, "cxt.v2.slot_type_invalid",
                                "Slot binding type does not match declaration", binding.id);
                    continue;
                }
                direct.bindings.emplace(
                    binding.id,
                    Source{
                        SourceKind::Literal, slotIt->second.kind, binding.value, {}, {}, {}, {}});
            }
            syntheticPattern.id = "__direct__";
            syntheticPattern.nodes.push_back(std::move(direct));
            pattern = &syntheticPattern;
        }
    } else {
        addCxtError(diagnostics, "cxt.v2.module_kind_unsupported",
                    "Foundation does not expand animation modules", "$/moduleKind");
    }
    if (pattern == nullptr) {
        result.diagnostics = std::move(diagnostics);
        return result;
    }
    // Preflight uses checked products so Repeat count does not allocate semantic entities.
    struct Estimate final {
        std::size_t entities{};
        std::size_t requirements{};
    };
    std::function<Estimate(const std::vector<Node>&, std::size_t)> estimate =
        [&](const std::vector<Node>& nodes, std::size_t depth) -> Estimate {
        Estimate total;
        if (depth > limits.maxCxtV2RepeatDepth) {
            addCxtError(diagnostics, "cxt.v2.budget.repeat_depth",
                        "Repeat depth exceeds configured budget", "$/patterns");
            return total;
        }
        for (const auto& node : nodes) {
            if (node.op == Node::Op::Emit) {
                const auto prototypeIt = module->prototypes.find(node.prototype);
                if (prototypeIt == module->prototypes.end()) {
                    addCxtError(diagnostics, "cxt.v2.prototype_missing",
                                "Emit references a missing prototype", node.prototype);
                    continue;
                }
                if (total.entities == std::numeric_limits<std::size_t>::max() ||
                    total.requirements > std::numeric_limits<std::size_t>::max() -
                                             prototypeIt->second.requirements.size()) {
                    addCxtError(diagnostics, "cxt.v2.budget.expansion",
                                "Expanded entity count exceeds configured budget", "$/patterns");
                    return total;
                }
                total.entities += 1U;
                total.requirements += prototypeIt->second.requirements.size();
                continue;
            }
            auto count = countFor(node.count, parameters, diagnostics);
            if (!count) {
                continue;
            }
            const auto body = estimate(node.body, depth + 1U);
            if (*count != 0 &&
                (body.entities > std::numeric_limits<std::size_t>::max() / *count ||
                 body.requirements > std::numeric_limits<std::size_t>::max() / *count)) {
                addCxtError(diagnostics, "cxt.v2.budget.expansion",
                            "Expanded entity count exceeds configured budget", node.nodeId);
                return total;
            }
            const auto scaledEntities = body.entities * *count;
            const auto scaledRequirements = body.requirements * *count;
            if (total.entities > std::numeric_limits<std::size_t>::max() - scaledEntities ||
                total.requirements > std::numeric_limits<std::size_t>::max() - scaledRequirements) {
                addCxtError(diagnostics, "cxt.v2.budget.expansion",
                            "Expanded entity count exceeds configured budget", node.nodeId);
                return total;
            }
            total.entities += scaledEntities;
            total.requirements += scaledRequirements;
        }
        return total;
    };
    const auto estimated = estimate(pattern->nodes, 0U);
    result.counts.entityCount = estimated.entities;
    result.counts.requirementCount = estimated.requirements;
    if (estimated.entities > limits.maxCxtV2ExpansionEntities) {
        addCxtError(diagnostics, "cxt.v2.budget.expansion",
                    "Expanded entity count exceeds configured budget", "$/patterns");
    }
    if (estimated.requirements > limits.maxCxtV2ExpansionRequirements) {
        addCxtError(diagnostics, "cxt.v2.budget.requirements",
                    "Expanded requirement count exceeds configured budget", "$/patterns");
    }
    diagnostics.sortDeterministically();
    if (diagnostics.hasErrors()) {
        result.diagnostics = std::move(diagnostics);
        return result;
    }
    ExpansionState state;
    state.chart.chartId = invocation.chartId;
    Bindings indices;
    std::vector<SemanticIdentityStep> path;
    expandNodes(pattern->nodes, *module, *pattern, invocation, parameters, indices, path, state,
                diagnostics, limits, 0U);

    // Resolve sibling emission parents after all entities exist, allowing forward references.
    for (const auto& pending : state.pendingParents) {
        std::vector<SemanticIdentityStep> targetPath = pending.scopePath;
        targetPath.push_back(SemanticIdentityStep{pending.nodeId, 0U});
        std::optional<CanonicalEntityIdentity> target;
        for (const auto& entity : state.chart.entities) {
            const auto* generated = std::get_if<GeneratedEntityIdentity>(&entity.identity);
            if (generated != nullptr && generated->path == targetPath) {
                target = entity.identity;
                break;
            }
        }
        if (!target) {
            addCxtError(diagnostics, "cxt.v2.parent_missing",
                        "Emission parent does not resolve in the same scope", pending.nodeId);
        } else if (pending.entityIndex < state.chart.entities.size()) {
            state.chart.entities[pending.entityIndex].parent = *target;
        }
    }
    std::vector<std::size_t> parentIndices(state.chart.entities.size(),
                                           std::numeric_limits<std::size_t>::max());
    for (std::size_t i = 0; i < state.chart.entities.size(); ++i) {
        const auto& parent = state.chart.entities[i].parent;
        if (!parent)
            continue;
        for (std::size_t j = 0; j < state.chart.entities.size(); ++j) {
            if (state.chart.entities[j].identity == *parent) {
                parentIndices[i] = j;
                break;
            }
        }
    }
    std::vector<std::uint8_t> visit(state.chart.entities.size(), 0U);
    std::function<void(std::size_t)> checkParent = [&](std::size_t index) {
        if (visit[index] == 1U) {
            addCxtError(diagnostics, "cxt.v2.parent_cycle",
                        "Generated parent graph contains a cycle", "$/patterns");
            return;
        }
        if (visit[index] == 2U)
            return;
        visit[index] = 1U;
        if (parentIndices[index] != std::numeric_limits<std::size_t>::max())
            checkParent(parentIndices[index]);
        visit[index] = 2U;
    };
    for (std::size_t i = 0; i < visit.size(); ++i)
        checkParent(i);
    std::ranges::sort(
        state.chart.entities, [](const CanonicalEntity& left, const CanonicalEntity& right) {
            return identityPathLess(std::get<GeneratedEntityIdentity>(left.identity).path,
                                    std::get<GeneratedEntityIdentity>(right.identity).path);
        });
    result.counts.entityCount = state.chart.entities.size();
    result.counts.requirementCount = state.requirementCount;
    diagnostics.sortDeterministically();
    if (!diagnostics.hasErrors())
        result.chart = std::move(state.chart);
    result.diagnostics = std::move(diagnostics);
    return result;
}

} // namespace cuexis::chart

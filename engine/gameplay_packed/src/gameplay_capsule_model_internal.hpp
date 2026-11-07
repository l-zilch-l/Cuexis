#pragma once

// The capsule model and the pattern/measure/domain row visitors.

#include "gameplay_capsule_visit_internal.hpp"

namespace cuexis::gameplay_packed {
namespace capsule_detail {

struct Model final {
    chart::CanonicalSemanticChart chart;
    CanonicalGameplayGraph graph{};
    std::vector<chart::CanonicalEntityIdentity> entities, owners;
    std::vector<RequirementIndices> indices;
    std::vector<PatternDeclaration> patterns;
    std::vector<MeasureDefinition> measures;
    std::vector<ClaimRow> claims;
    std::vector<std::uint32_t> deferredRelationResources;
    CapsuleProfiles profiles;
    std::uint32_t candidateRevision{2};
    std::string resolver{kResolver}, close{kClose}, profileId, unit;
    TimebaseProfile timebase{
        {}, {}, checked(RationalDuration::create(1, 1)), checked(RationalBeat::create(0, 1)), {},
        {}, {}};
    LatePolicyParameters latePolicy{};
    std::array<std::uint32_t, 11> counts{};
    void bind() {
        timebase.profileId = profileId;
        timebase.unitToken = unit;
        graph.timebase = &timebase;
        graph.latePolicy = &latePolicy;
        if (executionRevision(candidateRevision)) {
            graph.normalizationProfileToken = profiles.normalizationProfileToken;
            graph.coordinatorPolicyToken = profiles.coordinatorPolicyToken;
        }
    }
};

template <typename A, typename V> void optionalRef(A& a, std::uint8_t kind, V& v) {
    bool present = v.has_value();
    a.value(present);
    if constexpr (!A::reading) {
        if (present) {
            a.ref(kind, *v);
            require(!v->empty(), "definitions.id", "present definition ID must not be empty");
        }
    } else if (present) {
        auto token = v.value_or("");
        a.ref(kind, token);
        require(!token.empty(), "definitions.id", "present definition ID must not be empty");
        v = std::move(token);
    } else
        v.reset();
}

template <typename Node>
auto nodes(Node& root, std::pmr::memory_resource* resource) -> core::detail::PmrBuffer<Node*> {
    core::detail::PmrBuffer<Node*> result{resource}, pending{resource};
    pending.push_back(&root);
    while (!pending.empty()) {
        auto* node = pending.back();
        pending.resize(pending.size() - 1);
        result.push_back(node);
        for (auto i = node->operands.rbegin(); i != node->operands.rend(); ++i) {
            pending.push_back(&*i);
        }
    }
    return result;
}
template <typename A, typename V> void nodeFields(A& a, V& v, std::uint32_t& children) {
    enumeration(a, v.primitive);
    a.value(children);
    require(!v.unsupportedForm, "patterns.nodes", "unsupported Pattern form");
    switch (v.primitive) {
    case PatternPrimitive::atom:
        require(children == 0 && !v.repeatBounds, "patterns.atom", "atom must be a leaf");
        a.value(v.atomRef);
        require(!v.atomRef.empty(), "patterns.atom", "atom reference is missing");
        break;
    case PatternPrimitive::sequence:
    case PatternPrimitive::choice:
        require(children > 0 && v.atomRef.empty() && !v.repeatBounds, "patterns.operands",
                "sequence and choice require nonempty operands");
        break;
    case PatternPrimitive::boundedRepeat: {
        require(children == 1 && v.atomRef.empty(), "patterns.repeat", "repeat needs one operand");
        if constexpr (A::reading) {
            v.repeatBounds = RepeatBoundsDeclaration{};
        }
        require(v.repeatBounds.has_value(), "patterns.repeat", "repeat bounds are missing");
        a.fields(v.repeatBounds->minimum, v.repeatBounds->maximum);
        require(v.repeatBounds->minimum <= v.repeatBounds->maximum, "patterns.repeat",
                "repeat bounds are reversed");
        break;
    }
    case PatternPrimitive::complement:
        require(children == 1 && v.atomRef.empty() && !v.repeatBounds, "patterns.complement",
                "complement needs one operand");
        break;
    case PatternPrimitive::skip:
    case PatternPrimitive::instant:
        require(children == 0 && v.atomRef.empty() && !v.repeatBounds, "patterns.leaf",
                "empty-language primitive must be a leaf");
        break;
    }
}
template <typename Emit>
auto walkPattern(HashCoreWork& work, const PatternNodeDeclaration& root, Emit emit) -> std::size_t {
    require(work.frames.empty(), "patterns.nodes", "Reentrant Pattern traversal");
    work.frames.push({&root, 0});
    std::size_t count = 0;
    while (!work.frames.empty()) {
        auto& frame = work.frames.top();
        if (frame.cursor == 0) {
            emit(*frame.node);
            ++count;
            frame.cursor = 1;
        }
        if (frame.cursor <= frame.node->operands.size()) {
            const auto* child = &frame.node->operands[static_cast<std::size_t>(frame.cursor++ - 1)];
            work.frames.push({child, 0});
        } else
            work.frames.pop();
    }
    return count;
}

template <typename A, typename V> void patternRow(A& a, V& v) {
    std::optional<std::conditional_t<A::reading, std::string, std::string_view>> id;
    if constexpr (!A::reading) {
        if (!v.patternId.empty()) {
            id = v.patternId;
        }
    }
    optionalRef(a, 8, id);
    if constexpr (A::reading) {
        v.patternId = id.value_or("");
    }
    require(v.matchPolicy == MatchPolicy::leftmostFirst, "patterns.matchPolicy",
            "unsupported match policy");
    constant(a, 1);
    a.value(v.required);
    if constexpr (A::reading) {
        const auto n = a.count();
        require(n > 0, "patterns.nodes", "root node is missing");
        // An explicit stack consumes preorder without recursive calls or pointers invalidated by
        // growth.
        struct Parent {
            PatternNodeDeclaration* node;
            std::uint32_t remaining;
        };
        std::vector<Parent> stack;
        std::uint64_t pending = 1;
        for (std::size_t i = 0; i < n; ++i) {
            require(pending > 0, "patterns.nodes", "disconnected Pattern node");
            PatternNodeDeclaration* node = &v.root;
            if (i > 0) {
                while (!stack.empty() && stack.back().remaining == 0) {
                    stack.pop_back();
                }
                require(!stack.empty(), "patterns.nodes", "disconnected Pattern node");
                auto& parent = stack.back();
                parent.node->operands.emplace_back();
                node = &parent.node->operands.back();
                --parent.remaining;
            }
            std::uint32_t childCount{};
            nodeFields(a, *node, childCount);
            --pending;
            require(childCount <= n - i - 1 && pending <= n - i - 1 - childCount, "patterns.nodes",
                    "child count exceeds available nodes");
            pending += childCount;
            if (childCount > 0) {
                node->operands.reserve(childCount);
                stack.push_back({node, childCount});
            }
        }
        require(pending == 0, "patterns.nodes", "incomplete Pattern tree");
    } else {
        if constexpr (std::is_same_v<A, Output>) {
            if (a.operation) {
                const auto count = walkPattern(*a.operation, v.root, [](const auto&) {});
                a.count(count);
                (void)walkPattern(*a.operation, v.root, [&](const auto& node) {
                    auto children = u32(node.operands.size());
                    nodeFields(a, node, children);
                    if (a.operation->counting)
                        ++a.operation->observation.patternCountNodes;
                    else
                        ++a.operation->observation.patternHashNodes;
                });
                return;
            }
        }
        auto flat = nodes(v.root, std::pmr::get_default_resource());
        a.count(flat.size());
        for (auto* node : flat) {
            auto n = u32(node->operands.size());
            nodeFields(a, *node, n);
        }
    }
}
template <typename A, typename V>
void visitFields(A& a, V& v, std::type_identity<MeasureComponentDeclaration>) {
    enumeration(a, v.phase);
    a.fields(v.categoryToken, v.declaredGradeTokens);
}
template <typename A, typename V> void measureRow(A& a, V& v) {
    optionalRef(a, 9, v.id);
    a.fields(v.declaration.components, v.declaration.required);
}
template <typename A, typename V>
void visitFields(A& a, V& v, std::type_identity<JudgementAxisRange>) {
    a.fields(v.axisToken, v.minimum, v.maximum);
}
template <typename A, typename V> void domainRow(A& a, V& v) {
    a.ref(4, v.domainId);
    a.fields(v.coordinateSystemToken, v.axes);
    constant(a, 1);
    require(v.frameResolution == FrameResolution::staticDeclaration &&
                v.dynamicFrameProviderToken.empty(),
            "domains.frame", "only static judgement geometry is accepted");
    a.fields(v.dynamicFrameProviderToken, v.required);
}
template <typename A, typename V> void mergedRow(A& a, V& v) {
    a.value(v.stableId);
    enumeration(a, v.kind);
    a.fields(v.name, v.references);
    if constexpr (A::reading) {
        RequiredRefs refs;
        a.value(refs);
        for (const auto& row : refs.features)
            v.requiredFeatureIds.push_back(row.featureId);
        for (const auto& row : refs.capabilities) {
            require(row.revision.empty(), "mergedDeclarations.required",
                    "merged declaration cannot carry an unrepresented revision");
            v.requiredCapabilityIds.push_back(row.capabilityId);
        }
    } else {
        a.count(v.requiredFeatureIds.size());
        for (const auto& token : v.requiredFeatureIds)
            a.ref(7, token);
        a.count(v.requiredCapabilityIds.size());
        for (const auto& token : v.requiredCapabilityIds)
            capabilityFields(a, token, {});
    }
}

template <typename A, typename V> void resourceRow(A& a, V& v) {
    require(v.unsupportedForms.empty(), "resources", "unsupported resource form");
    a.fields(v.ref, v.declaredCapacity, v.slotToken, v.decisionPolicyRef,
             v.terminalAfterTermination, v.declaredGapGrace, v.required);
}
template <typename A, typename V> void solverRow(A& a, V& v) {
    a.fields(v.solverId, v.revision, v.algorithmToken, v.objective, v.tieBreak, v.rejectIfNonUnique,
             v.required);
    projected(a, 13, 5, {v.solverId, v.revision});
}
template <typename A, typename V> void bindingRow(A& a, V& v) {
    a.value(v.bindingId);
    projected(a, 13, 7, {v.bindingId});
}
template <typename A, typename I, typename Rows, typename F>
void index(A& a, I& i, Rows& table, F row) {
    if constexpr (A::reading) {
        a.value(i);
    } else if (a.mode == Mode::hash) {
        require(i < table.size(), "gameplay.index", "table index is out of range");
        if constexpr (std::is_same_v<A, Output>) {
            if (a.operation) {
                a.operation->visitEdge(a);
                using T = std::remove_cvref_t<decltype(table[i])>;
                if (a.operation->counting) {
                    if constexpr (!std::is_same_v<T, RequirementRecord> &&
                                  !std::is_same_v<T, ResourceRecord>) {
                        const auto cost = a.operation->resolve(
                            costGroup<T>(), i, [&](Output& child) { row(child, table[i]); });
                        a.operation->combine(a, cost);
                        return;
                    }
                }
            } else if (a.cost)
                ++a.cost->visits;
        }
        row(a, table[i]);
    } else {
        a.value(i);
    }
}

} // namespace capsule_detail
} // namespace cuexis::gameplay_packed

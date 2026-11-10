#pragma once

// Ordering, reference closure and index lookups shared by the closures.

#include "gameplay_capsule_model_internal.hpp"

namespace cuexis::gameplay_packed {
namespace capsule_detail {

inline auto refsKey(const RequiredRefs& v) {
    return std::tie(v.features, v.capabilities);
}
inline auto patternLess(const PatternDeclaration& l, const PatternDeclaration& r) -> bool {
    if (l.patternId != r.patternId) {
        return textLess(l.patternId, r.patternId);
    }
    if (refsKey(l.required) != refsKey(r.required)) {
        return refsKey(l.required) < refsKey(r.required);
    }
    std::vector<const PatternNodeDeclaration*> left{&l.root}, right{&r.root};
    while (!left.empty() && !right.empty()) {
        const auto* a = left.back();
        left.pop_back();
        const auto* b = right.back();
        right.pop_back();
        const auto ak = std::tuple{a->primitive, a->operands.size(), a->atomRef, a->repeatBounds};
        const auto bk = std::tuple{b->primitive, b->operands.size(), b->atomRef, b->repeatBounds};
        if (ak != bk) {
            return ak < bk;
        }
        for (auto i = a->operands.rbegin(); i != a->operands.rend(); ++i) {
            left.push_back(&*i);
        }
        for (auto i = b->operands.rbegin(); i != b->operands.rend(); ++i) {
            right.push_back(&*i);
        }
    }
    return left.empty() && !right.empty();
}
inline auto measureLess(const MeasureDefinition& l, const MeasureDefinition& r) -> bool {
    if (l.id != r.id) {
        return l.id < r.id;
    }
    if (l.declaration.components != r.declaration.components) {
        return l.declaration.components < r.declaration.components;
    }
    return refsKey(l.declaration.required) < refsKey(r.declaration.required);
}
template <typename T, typename F>
void ordered(const std::vector<T>& rows, F less, std::string_view path) {
    for (std::size_t i = 1; i < rows.size(); ++i) {
        require(less(rows[i - 1], rows[i]), path, "set must be strictly sorted and unique");
    }
}
template <typename T> void ordered(const std::vector<T>& rows, std::string_view path) {
    ordered(rows, std::less<T>{}, path);
}
inline void refsValid(const RequiredRefs& v) {
    ordered(v.features, "required.features");
    ordered(v.capabilities, "required.capabilities");
    for (const auto& r : v.features) {
        require(!r.featureId.empty(), "required.features", "empty feature ID");
    }
    for (const auto& r : v.capabilities) {
        require(!r.capabilityId.empty(), "required.capabilities", "empty capability ID");
    }
}
inline void counts(Model& m, std::size_t referenceCount) {
    const auto& g = m.graph;
    m.counts = {u32(g.requirements.size()),
                u32(m.patterns.size()),
                u32(m.measures.size()),
                u32(g.judgementDomains.size()),
                u32(g.mergedNamespace.declarations.size()),
                u32(g.resources.size()),
                u32(g.relations.size()),
                u32(g.solverProfiles.size()),
                u32(g.factBindings.size()),
                u32(m.claims.size()),
                u32(referenceCount)};
}
template <typename T, typename F>
auto findIndex(const std::vector<T>& rows, F predicate, std::string_view path) -> std::uint32_t {
    const auto it = std::find_if(rows.begin(), rows.end(), predicate);
    require(it != rows.end(), path, "dangling table reference");
    return u32(static_cast<std::size_t>(it - rows.begin()));
}
template <typename T, typename Less>
auto findSortedIndex(const std::vector<T>& rows, const T& wanted, Less less, std::string_view path)
    -> std::uint32_t {
    // The single-definition path needs no ordering comparisons or traversal scratch.
    if (rows.size() < 2)
        return findIndex(rows, [&wanted](const auto& row) { return row == wanted; }, path);
    const auto [first, last] = std::equal_range(rows.begin(), rows.end(), wanted, less);
    // Ordering can omit a field. Retain the first exact match within its equivalent range.
    const auto found = std::find(first, last, wanted);
    require(found != last, path, "dangling table reference");
    return u32(static_cast<std::size_t>(found - rows.begin()));
}
// Prepared Writer graphs are canonical; the Reader verifies declaration order
// before these lookups. Never search an unvalidated physical input table here.
inline auto findDeclaration(const StableDeclarationId& id, const Model& m)
    -> const MergedDeclaration* {
    const auto& rows = m.graph.mergedNamespace.declarations;
    const auto found =
        std::lower_bound(rows.begin(), rows.end(), id,
                         [](const auto& row, const auto& key) { return row.stableId < key; });
    return found != rows.end() && found->stableId == id ? &*found : nullptr;
}
inline auto namedMeasure(const RequirementRecord& r, const Model& m) -> std::optional<std::string> {
    const auto* declaration = findDeclaration(r.stableId, m);
    require(declaration != nullptr, "requirements.stableId", "missing declaration");
    std::optional<std::string> result;
    for (const auto& ref : declaration->references) {
        const auto* d = findDeclaration(ref, m);
        require(d != nullptr, "mergedDeclarations.references", "dangling declaration");
        if (d->kind == DeclarationKind::measureDefinition) {
            require(!result, "requirements.measure", "multiple named Measure references");
            result = d->name;
        }
    }
    return result;
}

} // namespace capsule_detail
} // namespace cuexis::gameplay_packed

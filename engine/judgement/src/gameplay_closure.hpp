#pragma once

#include <cuexis/judgement/gameplay_graph.hpp>

namespace cuexis::judgement::detail {

// Content completeness excludes subsystem contributions without copying the owning graph.
[[nodiscard]] auto deriveContentCapabilityClosure(const CanonicalGameplayGraph& graph)
    -> DerivedCapabilityClosure;
[[nodiscard]] auto deriveContentFeatureClosure(const CanonicalGameplayGraph& graph)
    -> FeatureClosure;

} // namespace cuexis::judgement::detail

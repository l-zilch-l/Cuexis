#pragma once

#include <cuexis/judgement/kernel_types.hpp>

namespace cuexis::judgement::testing {
// S1 rescans source obligations. It never reads PreparedGameplay::timers or the production cursor,
// contact reducer, resource checks, candidate comparator, phase reducer, or Ledger builder.
auto scanReference(const PreparedGameplay&, const SessionConfiguration&,
                   const std::vector<AdmittedObservation>&, Tick horizon) -> KernelProjection;
// L2 uses its own checked integer table and no production routing/comparison helper.
auto referenceRoute(const LatePolicyParameters&, Tick observation, std::optional<Tick> horizon)
    -> std::optional<Tick>;
} // namespace cuexis::judgement::testing

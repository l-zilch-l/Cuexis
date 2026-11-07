#pragma once

#include <cuexis/judgement/gameplay_prepare.hpp>
#include <tuple>

namespace cuexis::judgement::detail {
// Reuses only the chart projection owned by the same immutable prepared content.
// Public graph-based generators remain available as independent byte references.
struct IdentityFactory final {
    static auto assemble(const CanonicalGameplayGraph&, const PreparedIdentityDeclarations&)
        -> std::tuple<ChartIdentity, ContentIdentity, PreparedIdentity>;
    static auto runtime(const PreparedGameplay&, const PreparedIdentityDeclarations&,
                        const InputMappingProfile&, const LatePolicyParameters&, std::string_view)
        -> PreparedIdentity;
};
} // namespace cuexis::judgement::detail

#pragma once

#include <cuexis/chart/chart_v4_resolver.hpp>

#include <span>

namespace cuexis::chart {

struct PreparedResourceIdentityComponent final {
    AssetId assetId;
    CanonicalContentIdentity identity;
};

[[nodiscard]] auto emptyParameterIdentity() -> CanonicalContentIdentity;

[[nodiscard]] auto canonicalBytesIdentity(std::string_view bytes) -> CanonicalContentIdentity;

[[nodiscard]] auto audioContentIdentity(std::span<const std::byte> bytes)
    -> CanonicalContentIdentity;

[[nodiscard]] auto assemblePreparedSemanticIdentity(
    const CanonicalContentIdentity& chartIdentity,
    std::span<const CxtIdentityComponent> cxtIdentities,
    std::span<const PreparedResourceIdentityComponent> resourceIdentities,
    const CanonicalContentIdentity& parameterIdentity) -> CanonicalContentIdentity;

} // namespace cuexis::chart
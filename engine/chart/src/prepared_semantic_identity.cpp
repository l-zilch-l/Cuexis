#include <cuexis/chart/prepared_semantic_identity.hpp>

#include "sha256_internal.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace cuexis::chart {
namespace {

void writeU32(detail::Sha256& hash, std::uint32_t value) noexcept {
    std::array<std::byte, 4> bytes{};
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        bytes[index] = static_cast<std::byte>(value >> (index * 8U));
    }
    hash.update(bytes);
}

void writeIdentity(detail::Sha256& hash, const CanonicalContentIdentity& identity) noexcept {
    hash.update(std::as_bytes(std::span{identity.sha256}));
}

void writeCountedName(detail::Sha256& hash, std::string_view name) {
    writeU32(hash, static_cast<std::uint32_t>(name.size()));
    hash.update(std::as_bytes(std::span{name.data(), name.size()}));
}

template <typename Item, typename Key> void sortByKey(std::vector<Item>& items, Key key) {
    std::ranges::sort(items, {}, key);
}

} // namespace

auto emptyParameterIdentity() -> CanonicalContentIdentity {
    detail::Sha256 hash;
    static constexpr char domain[] = "cuexis.parameter-set.v1";
    hash.update(std::as_bytes(std::span{domain, sizeof(domain)}));
    return CanonicalContentIdentity{hash.finish()};
}

auto canonicalBytesIdentity(std::string_view bytes) -> CanonicalContentIdentity {
    return CanonicalContentIdentity{detail::sha256(bytes)};
}

auto audioContentIdentity(std::span<const std::byte> bytes) -> CanonicalContentIdentity {
    detail::Sha256 hash;
    static constexpr char domain[] = "cuexis.prepared-audio.v1";
    hash.update(std::as_bytes(std::span{domain, sizeof(domain)}));
    hash.update(bytes);
    return CanonicalContentIdentity{hash.finish()};
}

auto assemblePreparedSemanticIdentity(
    const CanonicalContentIdentity& chartIdentity,
    std::span<const CxtIdentityComponent> cxtIdentities,
    std::span<const PreparedResourceIdentityComponent> resourceIdentities,
    const CanonicalContentIdentity& parameterIdentity) -> CanonicalContentIdentity {
    auto sortedCxt = std::vector<CxtIdentityComponent>{cxtIdentities.begin(), cxtIdentities.end()};
    sortByKey(sortedCxt, &CxtIdentityComponent::importId);
    auto sortedResources = std::vector<PreparedResourceIdentityComponent>{
        resourceIdentities.begin(), resourceIdentities.end()};
    sortByKey(sortedResources,
              [](const PreparedResourceIdentityComponent& item) { return item.assetId.value; });

    detail::Sha256 hash;
    static constexpr char domain[] = "cuexis.prepared-semantic.v1";
    hash.update(std::as_bytes(std::span{domain, sizeof(domain)}));
    writeIdentity(hash, chartIdentity);
    writeU32(hash, static_cast<std::uint32_t>(sortedCxt.size()));
    for (const auto& item : sortedCxt) {
        writeCountedName(hash, item.importId);
        writeIdentity(hash, item.identity);
    }
    writeU32(hash, static_cast<std::uint32_t>(sortedResources.size()));
    for (const auto& item : sortedResources) {
        writeCountedName(hash, item.assetId.value);
        writeIdentity(hash, item.identity);
    }
    writeIdentity(hash, parameterIdentity);
    return CanonicalContentIdentity{hash.finish()};
}

} // namespace cuexis::chart
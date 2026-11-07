#pragma once

#include <cuexis/judgement/input_boundary.hpp>

#include <memory>
#include <span>
#include <string>

namespace cuexis::judgement {

namespace detail {
struct IngressJournal final {
    std::vector<std::unique_ptr<const OwnedIngressSubject>> owners;
    std::vector<NormalizedObservationEntry> entries;
    std::optional<ObservationTick> latestTick;
    std::uint64_t nextId;
    bool exhausted;
};
// All storage allocation precedes publication. Entries borrow stable heap nodes in this journal.
auto prepareIngressBatch(const SessionIngressState&, const InputMappingProfile&,
                         std::span<const ClockedIngress>, bool resolveDomains = true,
                         std::span<const std::optional<std::int64_t>> canonicalAmounts = {})
    -> core::Result<IngressJournal>;
auto reserveIngressBatch(SessionIngressState&, const IngressJournal&) -> core::Result<void>;
void commitIngressBatch(SessionIngressState&, IngressJournal&&) noexcept;
} // namespace detail
} // namespace cuexis::judgement

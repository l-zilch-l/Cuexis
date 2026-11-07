#include "diagnostic_policy.hpp"
#include "publication_lifetime.hpp"

namespace cuexis::judgement::detail {
PublicationRead::PublicationRead(std::weak_ptr<PublicationLifetime> lifetime,
                                 const KernelProjection* projection, PublicationMetadata metadata)
    : lifetime_(std::move(lifetime)), projection_(projection), metadata_(std::move(metadata)) {}
PublicationRead::PublicationRead(PublicationRead&&) noexcept = default;
PublicationRead::~PublicationRead() {
    if (const auto lifetime = lifetime_.lock();
        lifetime && lifetime->owner.isCurrent() && lifetime->epoch == metadata_.readEpoch)
        lifetime->reading = false;
}
auto PublicationRead::validate() const -> core::Result<void> {
    const auto lifetime = lifetime_.lock();
    if (!lifetime)
        return core::unexpected(publicationError("session lifetime ended"));
    // Check the immutable thread identity before touching owner-only state or borrowed data.
    if (!lifetime->owner.isCurrent())
        return core::unexpected(publicationError("wrong owner thread"));
    if (lifetime->mutating || !lifetime->reading || lifetime->epoch != metadata_.readEpoch)
        return core::unexpected(publicationError("stale or reentrant publication read"));
    return {};
}
auto PublicationRead::metadata() const
    -> core::Result<std::reference_wrapper<const PublicationMetadata>> {
    if (auto valid = validate(); !valid)
        return core::unexpected(std::move(valid.error()));
    return std::cref(metadata_);
}
auto PublicationRead::fact(std::uint64_t ordinal) const -> core::Result<PublicationFact> {
    if (auto valid = validate(); !valid)
        return core::unexpected(std::move(valid.error()));
    if (ordinal < metadata_.fromCursor || ordinal >= metadata_.publishableCursor)
        return core::unexpected(publicationError("Fact ordinal outside requested prefix"));
    const auto& value = projection_->facts[static_cast<std::size_t>(ordinal)];
    if (const auto* phase = std::get_if<PhaseOutcomeFact>(&value))
        return PublicationFact{FactKind::phaseOutcome, phase->factId,  &phase->requirement,
                               phase->phase,           phase->outcome, phase->error};
    const auto& receipt = std::get<ReceiptFact>(value);
    return PublicationFact{receipt.kind, receipt.factId, nullptr, {}, {}, {}};
}
auto classifyPublicDiagnostic(std::string_view code) noexcept -> bool {
    return categoryProjectedDiagnostic(code);
}
} // namespace cuexis::judgement::detail

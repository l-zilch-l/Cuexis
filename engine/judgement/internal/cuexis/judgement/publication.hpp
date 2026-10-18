#pragma once

// Noninstalled, owner-thread publication bridge. Accessors validate the read epoch.
// Borrowed references must not escape this operation or survive any session mutation.
#include <cuexis/judgement/judgement_session.hpp>

#include <functional>

namespace cuexis::judgement::detail {
struct PublicationLifetime;
struct PublicationMetadata final {
    const PreparedIdentity& identity;
    std::shared_ptr<const std::uint8_t> runScope;
    std::uint64_t readEpoch, fromCursor, sealedCursor, publishableCursor;
    std::optional<Tick> processedFrontier, kernelWorkTick, foldWorkTick;
    std::uint64_t controlCursor;
    bool hasFold, mayProject, realtime;
    std::optional<FaultStage> faultStage;
    const core::Error* faultDiagnostic;
};
struct PublicationFact final {
    FactKind kind;
    FactId factId;
    // Present only for phaseOutcome. This reference has the read's lifetime.
    const RequirementIdentity* requirement;
    std::optional<PhaseKind> phase;
    std::optional<Outcome> outcome;
    std::optional<TickDelta> error;
};

class PublicationRead final {
  public:
    PublicationRead(PublicationRead&&) noexcept;
    auto operator=(PublicationRead&&) noexcept -> PublicationRead& = delete;
    PublicationRead(const PublicationRead&) = delete;
    ~PublicationRead();
    auto metadata() const -> core::Result<std::reference_wrapper<const PublicationMetadata>>;
    auto fact(std::uint64_t ordinal) const -> core::Result<PublicationFact>;

  private:
    friend struct PublicationAccess;
    PublicationRead(std::weak_ptr<PublicationLifetime>, const KernelProjection*,
                    PublicationMetadata);
    auto validate() const -> core::Result<void>;
    std::weak_ptr<PublicationLifetime> lifetime_;
    const KernelProjection* projection_;
    PublicationMetadata metadata_;
};

struct PublicationAccess final {
    // Scope acquisition is explicit. A caller rebuilding a new run must explicitly choose 0.
    static auto scope(const JudgementSession&) -> core::Result<std::shared_ptr<const std::uint8_t>>;
    static auto readSince(const JudgementSession&, const std::shared_ptr<const std::uint8_t>&,
                          std::uint64_t fromCursor) -> core::Result<PublicationRead>;
};

// Delegates to Judgement's single diagnostic policy; no private code table escapes.
auto classifyPublicDiagnostic(std::string_view code) noexcept -> bool;
} // namespace cuexis::judgement::detail

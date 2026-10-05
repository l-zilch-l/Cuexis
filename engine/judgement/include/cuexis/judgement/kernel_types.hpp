#pragma once

#include <cuexis/judgement/gameplay_prepare.hpp>

#include <variant>

namespace cuexis::judgement {

// Owning configuration carriers create borrowed legacy views only at the point of use.
class OwnedInputMappingProfile final {
  public:
    explicit OwnedInputMappingProfile(const InputMappingProfile& value);
    [[nodiscard]] auto view() const -> InputMappingProfile;

  private:
    std::string id_, version_, source_;
    std::vector<std::pair<std::string, AmountSpec>> domains_;
};

class OwnedTimebaseProfile final {
  public:
    explicit OwnedTimebaseProfile(const TimebaseProfile& value);
    [[nodiscard]] auto view() const -> TimebaseProfile;

  private:
    std::string id_, unit_;
    RationalDuration scale_;
    RationalBeat origin_;
    std::optional<RationalDuration> tempo_;
    std::vector<TempoSection> tempos_;
    std::vector<StopSection> stops_;
};

struct SessionConfiguration final {
    OwnedInputMappingProfile inputMapping;
    OwnedTimebaseProfile timebase;
    LatePolicyParameters latePolicy;
    PreparedIdentityDeclarations identityDeclarations;
    std::string calibrationIdentityToken;
    std::string executionProfileToken;
    std::string lateAlgorithmToken;
    std::string factSemanticRevision;
};

struct CanonicalObservationKey final {
    ObservationTick observationTick;
    std::string domainToken, sourceClass, channelToken;
    InputAction action;
    std::optional<std::int64_t> amount;
    friend auto operator==(const CanonicalObservationKey&, const CanonicalObservationKey&)
        -> bool = default;
    friend auto operator<=>(const CanonicalObservationKey&,
                            const CanonicalObservationKey&) = default;
};

struct AdmittedObservation final {
    CanonicalObservationKey observationKey;
    Tick dispatchTick;
    bool wasForwarded;
    std::optional<Tick> admissionFrontier, admissionHorizon;
};
struct InputReceiptPending final {
    CanonicalObservationKey observationKey;
    Tick dispatchTick;
    bool wasForwarded;
    std::optional<Tick> admissionFrontier, admissionHorizon;
    friend auto operator==(const InputReceiptPending&, const InputReceiptPending&)
        -> bool = default;
};

struct ObservationOrigin final {
    Tick dispatchTick;
    CanonicalObservationKey observationKey;
    friend auto operator==(const ObservationOrigin&, const ObservationOrigin&) -> bool = default;
    friend auto operator<=>(const ObservationOrigin&, const ObservationOrigin&) = default;
};
struct TimerOrigin final {
    Tick tick;
    RequirementIdentity requirement;
    friend auto operator==(const TimerOrigin&, const TimerOrigin&) -> bool = default;
    friend auto operator<=>(const TimerOrigin&, const TimerOrigin&) = default;
};
struct CoordinationOrigin final {
    Tick dispatchTick;
    CanonicalObservationKey observationKey;
    friend auto operator==(const CoordinationOrigin&, const CoordinationOrigin&) -> bool = default;
    friend auto operator<=>(const CoordinationOrigin&, const CoordinationOrigin&) = default;
};
using OriginId = std::variant<ObservationOrigin, TimerOrigin, CoordinationOrigin>;
using OriginKey = OriginId;

struct CandidateKey final {
    RequirementIdentity requirement;
    PhaseKind phase;
    OriginKey origin;
    std::optional<std::string> resourceId;
    std::optional<ResourceClaimIntent> intent;
    friend auto operator==(const CandidateKey&, const CandidateKey&) -> bool = default;
};
struct ContactHandle final {
    std::string domainToken, sourceClass, channelToken;
    CanonicalObservationKey startPressKey;
    friend auto operator==(const ContactHandle&, const ContactHandle&) -> bool = default;
};
struct SlotIdentity final {
    std::string resourceId, slotToken;
    std::uint64_t ordinal;
    friend auto operator==(const SlotIdentity&, const SlotIdentity&) -> bool = default;
};
struct LeaseIdentity final {
    SlotIdentity slot;
    RequirementIdentity requirement;
    CandidateKey candidate;
    friend auto operator==(const LeaseIdentity&, const LeaseIdentity&) -> bool = default;
};
struct HeadContactOwnership final {
    RequirementIdentity requirement;
    ContactHandle contact;
    Tick acquiredTick;
    std::optional<LeaseIdentity> lease;
    friend auto operator==(const HeadContactOwnership&, const HeadContactOwnership&)
        -> bool = default;
};
struct Free final {
    friend auto operator==(Free, Free) -> bool = default;
};
struct Terminal final {
    friend auto operator==(Terminal, Terminal) -> bool = default;
};
struct Held final {
    RequirementIdentity requirement;
    LeaseIdentity lease;
    ContactHandle contact;
    friend auto operator==(const Held&, const Held&) -> bool = default;
};
using ResourceState = std::variant<Free, Held, Terminal>;
struct ResourceProjection final {
    std::string resourceId;
    ResourceState state;
    friend auto operator==(const ResourceProjection&, const ResourceProjection&) -> bool = default;
};

enum class KernelSessionState : std::uint8_t { Created, Configured, Prepared, Faulted };
enum class KernelPhaseState : std::uint8_t { Dormant, Pending, Hit, Miss, Observed, Expired };
struct PhaseProjection final {
    RequirementIdentity requirement;
    PhaseKind phase;
    KernelPhaseState state;
    friend auto operator==(const PhaseProjection&, const PhaseProjection&) -> bool = default;
};
struct FactId final {
    std::uint64_t commitId, localOrdinal;
    friend auto operator==(const FactId&, const FactId&) -> bool = default;
};
enum class FactKind : std::uint8_t { phaseOutcome, stray, consumeEmpty };
struct LogicalCanonicalOrdinal final {
    OriginId originScope;
    std::uint64_t originOrdinal, localOrdinal;
    std::uint8_t phaseRank;
    FactKind factKind;
    friend auto operator==(const LogicalCanonicalOrdinal&, const LogicalCanonicalOrdinal&)
        -> bool = default;
};
struct PhaseOutcomeFact final {
    RequirementIdentity requirement;
    PhaseKind phase;
    Outcome outcome;
    FactCategory category;
    std::optional<TickDelta> error;
    std::optional<CanonicalObservationKey> evidence;
    OriginId originId;
    std::uint64_t commitId;
    FactId factId;
    Tick commitTick;
    LogicalCanonicalOrdinal canonicalOrdinal;
    friend auto operator==(const PhaseOutcomeFact&, const PhaseOutcomeFact&) -> bool = default;
};
struct ReceiptFact final {
    FactKind kind;
    CanonicalObservationKey observationKey;
    OriginId originId;
    std::uint64_t commitId;
    FactId factId;
    Tick commitTick;
    LogicalCanonicalOrdinal canonicalOrdinal;
    friend auto operator==(const ReceiptFact&, const ReceiptFact&) -> bool = default;
};
using KernelFact = std::variant<PhaseOutcomeFact, ReceiptFact>;
struct ObservationRecord final {
    RequirementIdentity requirement;
    OriginKey originKey;
    Tick commitTick;
    std::optional<CanonicalObservationKey> observationKey;
    bool progressed, accepted;
    friend auto operator==(const ObservationRecord&, const ObservationRecord&) -> bool = default;
};
struct InputReceipt final {
    CanonicalObservationKey observationKey;
    std::uint64_t consideredCount;
    std::optional<RequirementIdentity> consumedBy;
    Tick dispatchTick;
    friend auto operator==(const InputReceipt&, const InputReceipt&) -> bool = default;
};
struct KernelProjection final {
    std::vector<PhaseProjection> phases;
    std::vector<ResourceProjection> resources;
    std::vector<ContactHandle> contacts;
    std::vector<HeadContactOwnership> ownership;
    std::vector<ObservationRecord> observers;
    std::vector<InputReceipt> receipts;
    std::vector<KernelFact> facts;
    std::optional<Tick> processedFrontier, lastAdvanceHorizon;
    KernelSessionState state;
    std::optional<core::Error> faultDiagnostic;
    PreparedIdentity judgementIdentity;
    // Opaque per-prepare anchor; excluded from deterministic identities.
    std::shared_ptr<const std::uint8_t> runScope;
    std::optional<Tick> failedTick, requestedHorizon;
};

} // namespace cuexis::judgement

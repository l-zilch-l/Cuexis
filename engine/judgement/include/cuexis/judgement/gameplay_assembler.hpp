#pragma once

//  Judgement typed kernel - S7A-3 (first half) offline typed assembler, closures and identity
//  bytes.
//
//  Authority: plan S7A-3 items 1-6, Gameplay V2 Spec sections 2.1-2.5, 3.2-3.6, 3.8, 5.2, 5.3, 5.5,
//  6.1-6.5 and 9.3, Gameplay V2 ABI domains 7 and 8, the proposal documents
//  `FORMAT_ENTRY_AND_IDENTITY.md` and `BUDGET_AND_EVIDENCE_PLAN.md`, and the rulings P1-01, P1-02,
//  P1-15, P2-05 and P2-07.
//
//  Frozen by this batch (first half of S7A-3):
//
//    * the typed Chart / CXT / candidate input form and the single offline assembler entry point.
//      `gameplay.version = 2` is the only V2 semantic entry; an older revision is either migrated
//      offline by a separate tool or stably rejected at the earliest decidable entry, and it is
//      never interpreted as V2 (Spec 2.1, 2.2; the offline migrator is S7A-8.2 and is not here);
//    * the physical entry rule of ABI domain 8: `playback = true` carries an explicit entry kind
//    and
//      an `author-source` entry must be explicitly non-playback. A `packed-chart` entry and a
//      `gameplay-graph` entry that carry the same typed content produce the same graph, because
//      there is exactly one assembling path;
//    * the P1-02 merge: stable id = source document identity + declaration ordinal, a duplicate
//    name
//      is rejected instead of merged, a cross-document reference must be explicit, and the merged
//      namespace is sorted by stable id. The result is therefore invariant under a permutation of
//      the input;
//    * closure derivation (feature / capability / resource) with the Spec 6.2 rule that a declared
//      set smaller than the derived one is a stable failure and that the writer never fills a
//      missing feature in;
//    * the canonical identity bytes of chart / content / prepared identity (Spec 5.5). They are one
//      deterministic internal algorithm, and they are explicitly not a public ABI, not a Packed
//      encoding and not a frozen artifact contract;
//    * atomic failure: a refused assembly publishes nothing and leaves an already published value
//      exactly as it was (Spec 9.3);
//    * the content profile *count* (BUDGET 3.1 / 3.2 requires counting and reporting capability
//      before any threshold is accepted) with every threshold supplied as a measured-or-pending
//      parameter, so that "no accepted limit yet" is a reported state and never a zero.
//
//  Implemented in this batch (second half of S7A-3, first part): the compiled Pattern predicate of
//  plan S7A-3 item 2 and the compiled Measure grading of plan S7A-3 item 3, both declared at the
//  end of this header with the budget entry points they consume.
//
//  Deliberately not implemented here: the first Packed write of the REF0 physical fields and the
//  accepted content-profile upper bounds of S7A-9. The prepare products below are implemented, but
//  physical Packed consumption remains blocked until its revision, section and field encoding are
//  closed by the P1-W1 wire decision.

#include <cuexis/core/result.hpp>
#include <cuexis/judgement/gameplay_graph.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "gameplay_assembler_compiled.hpp"
#include "gameplay_assembler_identity.hpp"
#include "gameplay_assembler_publication.hpp"

namespace cuexis::judgement {

//  ---------------------------------------------------------------------------------------------
//  Third part of S7A-3: prepared grace and resource plan
//  ---------------------------------------------------------------------------------------------

//  The declared inputs of the `preparedGrace` resolution (plan S7A-3 item 4). G2-S2 keeps the
//  source choice explicit: a chart value may override only when the declaration allows it, while
//  an inherited value is resolved at most one named hop and a frozen default is a separate source.
//  The legacy `candidate` field remains accepted as an already canonical TickSpan so existing typed
//  callers can migrate without changing the value domain; new callers should use the exact
//  RationalDuration fields.
struct GraceResolutionInputs final {
    //  The exact size of one canonical grace unit on the tick domain's declared scale.
    RationalDuration unitInTicks;
    //  The declared representable range of the grace value, in canonical units.
    std::int64_t minimumCanonical;
    std::int64_t maximumCanonical;
    //  Exact chart-supplied duration. It is considered only when allowChartGrace is true.
    std::optional<RationalDuration> chartDuration;
    //  Exact value supplied by the one declaration named by inheritedFromDeclarationId.
    std::optional<RationalDuration> inheritedDuration;
    //  Exact default frozen before prepare. It is not an implicit zero.
    std::optional<RationalDuration> defaultDuration;
    //  Compatibility input for callers that already hold a canonical unit count. It has no
    //  implicit fallback semantics: source policy and allowChartGrace still decide whether it is
    //  usable.
    std::optional<TickSpan> candidate;
};

//  Resolves the final prepared grace value of one requirement, read-only after prepare.
[[nodiscard]] auto resolvePreparedGrace(const GraceDeclaration& declaration,
                                        const GraceResolutionInputs& inputs)
    -> core::Result<PreparedGrace>;

//  The declared inputs of the resource claim / ownership resolution for the single `capacity = 1`
//  exclusive resource subset of Stage 7A.
struct ResourceClaimResolutionInputs final {
    struct Candidate final {
        ResourceClaimIntent intent;
        std::string policyToken;
        std::string claimKeyToken;
        GraceOverrideMode graceOverrideMode{GraceOverrideMode::none};
        std::optional<ClaimPolicyDeclaration::CompetitionKey> competition;

        friend auto operator==(const Candidate&, const Candidate&) noexcept -> bool = default;
    };

    std::uint64_t declaredCapacity;
    std::vector<ResourceClaimIntent> intents;
    //  Preferred R2 input. Every occupying candidate must carry an explicit policy and stable key;
    //  observe candidates are retained in the plan but never own a slot.
    std::vector<Candidate> candidates;
    PreparedGrace preparedGrace;
};

//  The resolved claim / ownership of one resource.
class ResourceClaimResolution final {
  public:
    ResourceClaimResolution(const ResourceClaimResolution&) noexcept = default;
    ResourceClaimResolution(ResourceClaimResolution&&) noexcept = default;
    auto operator=(const ResourceClaimResolution&) noexcept -> ResourceClaimResolution& = default;
    auto operator=(ResourceClaimResolution&&) noexcept -> ResourceClaimResolution& = default;
    ~ResourceClaimResolution() noexcept = default;

    [[nodiscard]] auto candidateCount() const noexcept -> std::size_t;
    [[nodiscard]] auto occupyingCandidateCount() const noexcept -> std::size_t;
    [[nodiscard]] auto hasObserveOnlyCandidates() const noexcept -> bool;
    [[nodiscard]] auto candidates() const -> std::vector<ResourceClaimResolutionInputs::Candidate>;

  private:
    friend auto resolveResourceClaims(const ResourceClaimResolutionInputs& inputs)
        -> core::Result<ResourceClaimResolution>;

    explicit ResourceClaimResolution(
        std::shared_ptr<const detail::ResourceClaimResolutionStorage> storage) noexcept;

    std::shared_ptr<const detail::ResourceClaimResolutionStorage> storage_;
};

//  Resolves the immutable prepared claim plan for the single `capacity = 1` exclusive resource.
//  Runtime owner / lease / contact allocation remains outside this prepare product.
[[nodiscard]] auto resolveResourceClaims(const ResourceClaimResolutionInputs& inputs)
    -> core::Result<ResourceClaimResolution>;

} // namespace cuexis::judgement

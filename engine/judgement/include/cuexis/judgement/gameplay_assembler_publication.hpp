#pragma once

//  Judgement typed kernel - content profile, assembly request and reference partition.

#include "gameplay_assembler_identity.hpp"

#include <cuexis/core/result.hpp>
#include <cuexis/judgement/gameplay_graph.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace cuexis::judgement {

//  ---------------------------------------------------------------------------------------------
//  Content profile counting (BUDGET_AND_EVIDENCE_PLAN 3.1 / 3.2)
//  ---------------------------------------------------------------------------------------------

//  The count of every content-profile dimension of one prepared graph.
//
//  Counting is implemented by this batch because the budget plan requires counting and reporting
//  capability before any threshold is accepted, and because a reported count is what a later
//  measurement is compared against. No threshold, no upper bound and no research-slice measurement
//  appears in this type: a count is a measurement of the content, never a limit on it.
struct ContentProfileCounts final {
    std::uint64_t requirements;
    std::uint64_t resources;
    std::uint64_t exclusiveRelations;
    std::uint64_t relationMembers;
    std::uint64_t solverProfiles;
    std::uint64_t factBindings;
    std::uint64_t judgementDomains;
    std::uint64_t emissions;
    std::uint64_t mergedDeclarations;
    std::uint64_t declaredCapabilities;
    std::uint64_t derivedCapabilities;
    std::uint64_t declaredFeatures;
    std::uint64_t derivedFeatures;
    std::uint64_t patternNodes;
    std::uint64_t measureComponents;
    std::uint64_t phaseDeclarations;
    std::uint64_t requiredFeatureRefs;
    std::uint64_t requiredCapabilityRefs;
    std::uint64_t diagnosticMapEntries;

    friend auto operator==(const ContentProfileCounts&, const ContentProfileCounts&) noexcept
        -> bool = default;
};

[[nodiscard]] auto countContentProfile(const CanonicalGameplayGraph& graph) -> ContentProfileCounts;

//  The accepted upper bounds of the content profile. Every bound is a measured-or-pending
//  parameter, which is the S7A-2 carrier for exactly this situation: a bound no measurement has
//  accepted stays pending and is reported as not enforced, instead of being spelled as a zero limit
//  (BUDGET 3.2 keeps 0 as a literal upper bound, so 0 can never mean "no limit").
struct ContentProfileLimits final {
    MeasuredParameter<std::uint64_t> maxRequirements;
    MeasuredParameter<std::uint64_t> maxResources;
    MeasuredParameter<std::uint64_t> maxExclusiveRelations;
    MeasuredParameter<std::uint64_t> maxRelationMembers;
    MeasuredParameter<std::uint64_t> maxMergedDeclarations;
    MeasuredParameter<std::uint64_t> maxPatternNodes;
    MeasuredParameter<std::uint64_t> maxMeasureComponents;
    MeasuredParameter<std::uint64_t> maxFactBindings;
    MeasuredParameter<std::uint64_t> maxDerivedCapabilities;
    MeasuredParameter<std::uint64_t> maxDiagnosticMapEntries;
};

//  The outcome of one content-profile check.
enum class ContentProfileVerdict : std::uint8_t {
    //  Every bound was measured and every count is within it.
    withinDeclaredBounds,
    //  At least one bound is still pending measurement, so that dimension was counted and reported
    //  but not enforced. This is the explicit form of "thresholds are not accepted yet": it is a
    //  verdict rather than a silent success, and it is never spelled as 0 or as "unlimited".
    notEnforcedPendingBounds,
};

//  Compares counts against the accepted bounds. A count above a measured bound is a stable
//  rejection with the budget_exceeded category, which is the prepare atomic-failure path for a
//  budget overrun; a pending bound is reported as `notEnforcedPendingBounds` and never enforced
//  with an invented number.
[[nodiscard]] auto checkContentProfile(const ContentProfileCounts& counts,
                                       const ContentProfileLimits& limits)
    -> core::Result<ContentProfileVerdict>;

//  ---------------------------------------------------------------------------------------------
//  The offline assembler
//  ---------------------------------------------------------------------------------------------

//  One assembly request: the physical entry, the typed sources, the graph-level declarations and
//  the compile context.
//
//  Everything that is a property of the *graph* rather than of a document lives here, so that
//  combining several sources cannot make the result depend on which source came first. The two
//  typed declarations are borrowed through pointers: Spec 3.2 calls the graph member `timebaseRef`,
//  the typed model stays owned by <cuexis/judgement/timebase.hpp>, and a null pointer is a stable
//  rejection rather than a missing binding the assembler would have to invent.
struct AssemblyRequest final {
    EntryKind entryKind;
    //  ABI domain 8: `playback = true` carries an explicit entry kind, and an `author-source` entry
    //  must be explicitly non-playback.
    bool playback;
    std::vector<GameplaySource> sources;
    //  The semantic graph revision the caller declares (Spec 5.2 `graphRevision`).
    std::uint64_t graphRevision;
    //  The declared ruleset binding and Interface projection token (Spec 5.2 `rulesetRef`).
    std::string rulesetRef;
    //  The declared minimum capability and feature sets of the source side (Spec 6.1 first arrow).
    DeclaredCapabilitySet declaredCapabilities;
    FeatureClosure declaredFeatures;
    //  The capability and feature needs of the ruleset binding and of the presentation closure.
    ClosureContributions closureContributions;
    //  The compiler / packer profile token of the source closure. Declared by the toolchain.
    std::string compilerProfileToken;
    //  The typed timebase binding and the late-policy parameters the graph is validated against
    //  before anything is assembled. Both are required.
    const TimebaseProfile* timebase;
    const LatePolicyParameters* latePolicy;
    //  The capability context of this compile.
    CompileCapabilityContext capabilityContext;
    //  The accepted content-profile bounds. Every field may still be pending measurement.
    ContentProfileLimits contentProfileLimits;
    //  The declared engine / ruleset / session components of the prepared identity.
    PreparedIdentityDeclarations identityDeclarations;
    //  Execution and compile profile declarations are explicit owning graph fields. Empty values
    //  preserve the revision-2 static prepare path; executable revision 3 requires all three.
    std::string executionProfile;
    std::string normalizationProfileToken;
    std::string coordinatorPolicyToken;
};

//  One successfully assembled result.
//
//  It carries the graph, the three identities and the counted content profile. The graph borrows
//  the two typed declarations of the request (`timebaseRef` is a reference): the caller keeps them
//  alive for as long as it uses the result, and everything else here is owned.
struct AssembledGameplay final {
    CanonicalGameplayGraph graph;
    ChartIdentity chart;
    ContentIdentity content;
    PreparedIdentity prepared;
    ContentProfileCounts contentProfile;
    //  Whether the accepted bounds were actually enforced. A graph assembled while some bound is
    //  still pending measurement is reported as `notEnforcedPendingBounds` instead of being
    //  presented as fully bounded.
    ContentProfileVerdict contentProfileVerdict;
};

//  The single offline assembler entry point.
//
//  The order of the checks is the order of the failures a caller has to be able to rely on:
//
//    1. the entry rule of ABI domain 8 (an explicitly non-playback `author-source` entry);
//    2. the version gates: `chartVersion = 5` and `gameplayVersion = 2` per source document, with
//    an
//       older revision stably rejected rather than interpreted as V2;
//    3. the typed timebase binding and late policy, through the S7A-2 `validatePrepare` gate;
//    4. the P1-02 merge of the declaration namespace: stable ids, duplicate names, explicit
//       cross-document references and dangling references;
//    5. every declaration's own structure, including the judgement domain record of P1-01 and the
//       content forms Stage 7A rejects;
//    6. the closures: derivation, dangling resource / solver / fact-binding references, unknown and
//       disabled capabilities, and the Spec 6.2 rule that a declared set smaller than the derived
//       one is a stable failure;
//    7. identity collisions;
//    8. the identity bytes and the content profile count.
//
//  Failure is atomic: a refused assembly returns an error and publishes nothing. It never
//  saturates, never clamps, never fills in a missing declaration and never falls back to an older
//  semantics.
[[nodiscard]] auto assembleGameplay(const AssemblyRequest& request)
    -> core::Result<AssembledGameplay>;

//  The atomic publication boundary of one assembly.
//
//  A host that already holds an active prepared graph keeps it across a failed assembly: the only
//  writer is `assembleInto`, and it assigns after `assembleGameplay` has succeeded, so a failed
//  attempt cannot leave a half-built value behind. The type exists so that the rule is a property
//  of the code rather than of a comment.
class GameplayPublication final {
  public:
    GameplayPublication() noexcept = default;
    ~GameplayPublication() noexcept = default;
    GameplayPublication(GameplayPublication&&) noexcept = default;
    auto operator=(GameplayPublication&&) noexcept -> GameplayPublication& = default;
    GameplayPublication(const GameplayPublication&) = delete;
    auto operator=(const GameplayPublication&) -> GameplayPublication& = delete;

    [[nodiscard]] auto hasActive() const noexcept -> bool {
        return active_.has_value();
    }
    //  Non-owning view of the active value; null when nothing has been published.
    [[nodiscard]] auto active() const noexcept -> const AssembledGameplay* {
        return active_.has_value() ? &active_.value() : nullptr;
    }

  private:
    friend auto assembleInto(GameplayPublication& publication, const AssemblyRequest& request)
        -> core::Result<void>;

    std::optional<AssembledGameplay> active_;
};

//  Assembles and publishes in one step. On failure the publication is untouched.
[[nodiscard]] auto assembleInto(GameplayPublication& publication, const AssemblyRequest& request)
    -> core::Result<void>;

//  ---------------------------------------------------------------------------------------------
//  Reference closure partition (P1-14 / Spec 3.8.6)
//  ---------------------------------------------------------------------------------------------

//  The semantic partition of the graph's references into the three classes of Spec 3.8.6, each in
//  canonical order. It is the input the first Packed write of REF0 consumes.
//
//  Only the semantic partition is produced. The physical fields, the numbering and the encoding of
//  REF0 that the first Packed write needs are explicitly not closed (Spec 3.8.6 landing note), so
//  this batch produces the partition and no byte image: a function that emitted bytes for an
//  unclosed encoding would be exactly the frozen representation the Spec keeps open.
struct ReferenceClosurePartition final {
    std::vector<std::string> judgementRef0;
    std::vector<std::string> presentationManifest;
    std::vector<std::string> diagnosticMap;

    friend auto operator==(const ReferenceClosurePartition&,
                           const ReferenceClosurePartition&) noexcept -> bool = default;
};

[[nodiscard]] auto partitionReferences(const CanonicalGameplayGraph& graph)
    -> ReferenceClosurePartition;

} // namespace cuexis::judgement

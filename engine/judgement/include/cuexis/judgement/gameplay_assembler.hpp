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

namespace cuexis::judgement {

//  ---------------------------------------------------------------------------------------------
//  Typed source input
//  ---------------------------------------------------------------------------------------------

//  One declaration as it appears inside one source document, before the merge (P1-02).
//
//  The declaration ordinal travels with the declaration instead of being the position of this
//  object in the array the assembler is handed. That is the whole reason the merge is invariant
//  under a permutation of the input: a permuted array carries the same ordinals and therefore
//  produces the same stable ids.
struct LocalDeclaration final {
    StableDeclarationId stableId;
    DeclarationKind kind;
    //  The name the declaration gives itself inside its own document. Names are global in the
    //  merged namespace, so two declarations with the same name are rejected rather than merged.
    std::string localName;
    std::vector<DeclarationRef> references;
    RequiredRefs required;

    friend auto operator==(const LocalDeclaration&, const LocalDeclaration&) noexcept
        -> bool = default;
};

//  One typed gameplay source document.
//
//  A document owns its declarations; the assembler owns the graph it publishes. Both version fields
//  are gates: `chartVersion` must be 5 (the only accepted outer version) and `gameplayVersion` must
//  be 2 (the only accepted V2 semantic version). Neither gate guesses a version from the presence
//  of a field, which Spec 2.2 item 2 forbids.
struct GameplaySourceDocument final {
    //  Stable identity of this source document. It is one half of every stable declaration id.
    std::string sourceDocumentId;
    std::uint32_t chartVersion;
    std::uint32_t gameplayVersion;
    std::vector<LocalDeclaration> declarations;
    std::vector<RequirementRecord> requirements;
    std::vector<ResourceRecord> resources;
    std::vector<RelationDeclaration> relations;
    std::vector<SolverProfileDeclaration> solverProfiles;
    std::vector<FactBindingRef> factBindings;
    std::vector<JudgementDomainRecord> judgementDomains;
};

//  One typed source together with the form it was carried in.
//
//  `form` and `provenanceToken` are provenance: they reach the diagnostic map and never a semantic
//  field or an identity. Two carriers that hold the same typed document therefore assemble into the
//  same graph and the same chart / content / prepared identity, which is the "typed file / memory
//  source consistency" the plan requires.
struct GameplaySource final {
    SourceForm form;
    //  A file path or a memory buffer identity. Diagnostic context only.
    std::string provenanceToken;
    GameplaySourceDocument document;
};

//  The physical entry an assembly came from (ABI domain 8 `EntryKind`). The candidate names are the
//  ABI's; their final spelling is unresolved item 3.
enum class EntryKind : std::uint8_t {
    packedChart,
    gameplayGraph,
    authorSource,
};

//  The capability context of one assembly.
//
//  The capability registry entry field set is unfrozen (CM-X01, first consumer S7A-7), so this
//  batch does not invent a registry. What it consumes is the caller-supplied context: which
//  capability ids the compiling toolchain recognises, and which of those this compile enabled. A
//  derived capability that no context recognises is an unknown capability; one that is recognised
//  but not enabled is a disabled capability; neither is silently added to the closure.
struct CompileCapabilityContext final {
    std::vector<std::string> recognisedCapabilityIds;
    std::vector<std::string> enabledCapabilityIds;
};

//  ---------------------------------------------------------------------------------------------
//  Canonical identity bytes (Spec 5.5)
//  ---------------------------------------------------------------------------------------------

//  The declared engine-side judgement semantics that take part in the *engine* component of the
//  judgement identity (ABI domain 7, Spec 5.2).
//
//  Every member is a token the engine build or the caller declares. This batch invents no revision
//  number, and the fact semantic revision is present while the snapshot state schema revision is
//  deliberately absent: round 5 keeps `factSemanticRevision` inside the engine identity and keeps
//  `stateSchemaRevision` out of it, and the cleanest way to obey that is to have no member that
//  could carry it.
struct EngineIdentityDeclaration final {
    //  The engine judgement-semantics revision token.
    std::string judgementSemanticRevision;
    //  The Fact semantic revision. A change to the Fact ordering key or to the originKind priority
    //  order has to raise it, and it takes part in the engine identity.
    std::string factSemanticRevision;
    //  The fixed-point table actually referenced by the judgement.
    std::string fixedPointTableId;
    //  The canonical coordination phase order semantics the engine applies. The eight-phase order
    //  enters the engine component and never the chart or content projection.
    std::string coordinationPhaseOrderToken;
    std::optional<std::string> executionProfileToken;
    std::optional<std::string> lateAlgorithmToken;

    friend auto operator==(const EngineIdentityDeclaration&, const EngineIdentityDeclaration&)
        -> bool = default;
};

//  The declared ruleset-side identity projection (ABI domain 7 `RulesetIdentity`).
struct RulesetIdentityDeclaration final {
    std::string interfaceProjectionToken;
    //  The module / fold order. It is an ordered semantic list, which is why it is not sorted.
    std::vector<std::string> moduleOrder;
    std::string buildHash;
    friend auto operator==(const RulesetIdentityDeclaration&, const RulesetIdentityDeclaration&)
        -> bool = default;
};

//  The declared session-side identity projection (ABI domain 7 `SessionIdentity`).
//
//  The discrete-input normalization profile is here rather than in the engine component because
//  Spec 5.2 makes the InputMapping profile a session identity component (CM-T09); the ABI's engine
//  list names a normalization profile as well, and this batch keeps one location instead of two.
struct SessionIdentityDeclaration final {
    std::string loadoutToken;
    std::string defaultGraceSourceToken;
    std::string normalizationProfileToken;
    std::string judgementConfigToken;
    friend auto operator==(const SessionIdentityDeclaration&, const SessionIdentityDeclaration&)
        -> bool = default;
};

//  The three declared components the assembler cannot derive from the graph.
struct PreparedIdentityDeclarations final {
    EngineIdentityDeclaration engine;
    RulesetIdentityDeclaration ruleset;
    SessionIdentityDeclaration session;
    friend auto operator==(const PreparedIdentityDeclarations&, const PreparedIdentityDeclarations&)
        -> bool = default;
};

//  The three identity generators, declared first so that the identity carriers below can name them
//  as the only producers of their bytes.
class CanonicalIdentityBytes;
class ChartIdentity;
class ContentIdentity;
class PreparedIdentity;

[[nodiscard]] auto makeRuntimePreparedIdentity(const CanonicalGameplayGraph& graph,
                                               const PreparedIdentityDeclarations& declarations,
                                               const InputMappingProfile& mapping,
                                               const LatePolicyParameters& late,
                                               std::string_view calibration) -> PreparedIdentity;

[[nodiscard]] auto makeChartIdentity(const CanonicalGameplayGraph& graph) -> ChartIdentity;
[[nodiscard]] auto makeContentIdentity(const CanonicalGameplayGraph& graph) -> ContentIdentity;
[[nodiscard]] auto makePreparedIdentity(const CanonicalGameplayGraph& graph,
                                        const PreparedIdentityDeclarations& declarations)
    -> PreparedIdentity;

//  The opaque canonical byte sequence of one identity (Spec 5.5 item 1).
//
//  The bytes are produced by exactly one algorithm, in one translation unit, with no host-dependent
//  step: integers are emitted big-endian byte by byte, tokens are length-prefixed, enumerators are
//  spelled by stable tokens rather than by their ordinal value, and every table is written in
//  canonical order. Two builds on two toolchains therefore produce the same bytes for the same
//  declared content.
//
//  What this is not: it is not the public ABI representation of an identity (ABI domain 7 exposes
//  an opaque value plus an equality test only), it is not the Packed encoding, and it is not a
//  frozen wire contract. Spec 5.5 item 4 keeps the *encoding* open until S7A-3 consumes it, so no
//  golden hash is committed and no interchangeability verdict may rest on these bytes alone.
class CanonicalIdentityBytes final {
  public:
    [[nodiscard]] auto bytes() const noexcept -> const std::vector<std::byte>& {
        return bytes_;
    }
    [[nodiscard]] auto empty() const noexcept -> bool {
        return bytes_.empty();
    }
    //  A stable hexadecimal rendering, for diagnostics and for test evidence. It is a rendering of
    //  the bytes and never a replacement for the field-by-field semantic diff of Spec 3.5.
    [[nodiscard]] auto toHex() const -> std::string;

    friend auto operator==(const CanonicalIdentityBytes&, const CanonicalIdentityBytes&) noexcept
        -> bool = default;

  private:
    friend auto makeRuntimePreparedIdentity(const CanonicalGameplayGraph&,
                                            const PreparedIdentityDeclarations&,
                                            const InputMappingProfile&, const LatePolicyParameters&,
                                            std::string_view) -> PreparedIdentity;
    friend auto makeChartIdentity(const CanonicalGameplayGraph& graph) -> ChartIdentity;
    friend auto makeContentIdentity(const CanonicalGameplayGraph& graph) -> ContentIdentity;
    friend auto makePreparedIdentity(const CanonicalGameplayGraph& graph,
                                     const PreparedIdentityDeclarations& declarations)
        -> PreparedIdentity;

    explicit CanonicalIdentityBytes(std::vector<std::byte> bytes) noexcept
        : bytes_(std::move(bytes)) {}

    std::vector<std::byte> bytes_;
};

//  The *chart* component of the judgement identity (ABI domain 7 `ChartIdentity`): the compiled
//  Requirement / Pattern / Measure declarations, the anchor quantization, the prepared grace, the
//  coordination judgement projection and the solver profile.
//
//  The explicit / inherited *source* of the prepared grace is deliberately not part of it: Spec 5.2
//  and plan S7A-3 item 5 keep that source in the content projection and in the diagnostic context,
//  so two requirements whose final grace and policy are the same share a chart component even when
//  one declared the value and the other inherited it.
class ChartIdentity final {
  public:
    [[nodiscard]] auto canonicalBytes() const noexcept -> const CanonicalIdentityBytes& {
        return bytes_;
    }

    friend auto operator==(const ChartIdentity&, const ChartIdentity&) noexcept -> bool = default;

  private:
    friend auto makeChartIdentity(const CanonicalGameplayGraph& graph) -> ChartIdentity;

    explicit ChartIdentity(CanonicalIdentityBytes bytes) noexcept : bytes_(std::move(bytes)) {}

    CanonicalIdentityBytes bytes_;
};

//  The *content* projection: the source closure, the whole semantic graph content and the resolved
//  grace provenance (P2-07 item 3).
//
//  It is strictly larger than the chart component, so equal content bytes imply an equal chart
//  component, while the converse does not hold: an inherited grace value and an explicitly declared
//  one with the same final value share the chart component and differ here.
class ContentIdentity final {
  public:
    [[nodiscard]] auto canonicalBytes() const noexcept -> const CanonicalIdentityBytes& {
        return bytes_;
    }

    friend auto operator==(const ContentIdentity&, const ContentIdentity&) noexcept
        -> bool = default;

  private:
    friend auto makeContentIdentity(const CanonicalGameplayGraph& graph) -> ContentIdentity;

    explicit ContentIdentity(CanonicalIdentityBytes bytes) noexcept : bytes_(std::move(bytes)) {}

    CanonicalIdentityBytes bytes_;
};

//  The *prepared judgement identity*: the four components of ABI domain 7, that is engine, ruleset,
//  chart and session. Two prepared identities are the same exactly when their canonical bytes are
//  equal, and nothing outside the fixed participant list can enter them.
class PreparedIdentity final {
  public:
    [[nodiscard]] auto canonicalBytes() const noexcept -> const CanonicalIdentityBytes& {
        return bytes_;
    }

    friend auto operator==(const PreparedIdentity&, const PreparedIdentity&) noexcept
        -> bool = default;

  private:
    friend auto makeRuntimePreparedIdentity(const CanonicalGameplayGraph&,
                                            const PreparedIdentityDeclarations&,
                                            const InputMappingProfile&, const LatePolicyParameters&,
                                            std::string_view) -> PreparedIdentity;
    friend auto makePreparedIdentity(const CanonicalGameplayGraph& graph,
                                     const PreparedIdentityDeclarations& declarations)
        -> PreparedIdentity;

    explicit PreparedIdentity(CanonicalIdentityBytes bytes) noexcept : bytes_(std::move(bytes)) {}

    CanonicalIdentityBytes bytes_;
};

//  True exactly when the two prepared identities are the same judgement identity, which is what
//  "the same final grace and policy share a judgement identity" means in code.
[[nodiscard]] auto sharesJudgementIdentity(const PreparedIdentity& left,
                                           const PreparedIdentity& right) noexcept -> bool;

[[nodiscard]] auto makeChartIdentity(const CanonicalGameplayGraph& graph) -> ChartIdentity;

[[nodiscard]] auto makeContentIdentity(const CanonicalGameplayGraph& graph) -> ContentIdentity;

[[nodiscard]] auto makePreparedIdentity(const CanonicalGameplayGraph& graph,
                                        const PreparedIdentityDeclarations& declarations)
    -> PreparedIdentity;

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

//  ---------------------------------------------------------------------------------------------
//  Second half of S7A-3: compiled products, prepared grace and resource plan
//  ---------------------------------------------------------------------------------------------
//
//  Plan S7A-3 items 2 and 3 are implemented here: `compilePattern` produces the read-only compiled
//  predicate of one Pattern declaration (all seven Stage 7A primitives, a prepare-time fully
//  expanded `bounded repeat`, the `leftmost-first` match policy, and the termination / state count
//  / expansion count / time / memory budget checks), and `compileMeasure` produces the read-only
//  per-component grading product of one Measure declaration (multiple phase / category components,
//  Hold head / body, the explicitly declared Release / tail, and the optional grade table).
//
//  Three rules shape the compiled products:
//
//    1. Nothing is frozen that the batch is not authorised to freeze. The compiled carriers are
//       immutable handles: no field width, no state number, no transition-table encoding, no
//       accounting constant and no budget number is part of this interface. The counts and the
//       budget parameters are exposed, the representation behind them is not;
//    2. no limit is invented. Every bound is a `MeasuredParameter`: a bound no measurement has
//       accepted stays pending, is reported as such, and is never enforced with a substituted
//       number. In particular there is no compiled-in declaration-depth constant: the depth is one
//       budgeted, measurable dimension like the expansion count and the state count, so an
//       unmeasured depth never refuses a deeply nested but legal pattern;
//    3. a check that is not implemented is refused, not faked. The rest of the second half -- the
//       `preparedGrace` resolution, the single `capacity = 1` exclusive resource claim / ownership
//       resolution, the first Packed write of the REF0 physical fields and the Packed physical
//       encoding -- keeps its stable rejection and fabricates no product.

namespace detail {

//  Storage anchors of the compiled products. The representation of a compiled Pattern, a compiled
//  Measure and a resolved resource claim is an implementation detail of this module, so this
//  boundary promises no member, no width and no encoding.
class CompiledPatternStorage;
class CompiledMeasureStorage;
class ResourceClaimResolutionStorage;

} // namespace detail

//  The compiled budget of one Pattern (plan S7A-3 item 2: "state count, expansion count and time /
//  memory budget"). Every dimension is a measured-or-pending parameter, so the compile never
//  refuses legal content on behalf of a threshold no measurement has accepted, and a measured
//  threshold is enforced exactly.
//
//  Time and memory are budgeted as deterministic work counters rather than as a wall clock or a
//  host allocator reading: a wall-clock threshold would make prepare depend on the machine, and the
//  canonical graph of one content has to be the same everywhere.
struct PatternCompileBudget final {
    //  The declaration depth of the Pattern tree. A budgeted, measurable dimension; there is no
    //  compiled-in depth constant.
    MeasuredParameter<std::uint64_t> maxDeclarationDepth;
    //  The number of primitive instances of the complete prepare-time expansion (Spec 3.8.4
    //  requires the complete expansion, so it is what bounds the compiled representation). A
    //  bounded repeat expands over every admissible copy count, so this number is the sum over
    //  those copy counts and not the largest single branch, which is reported separately. A count
    //  with no representable value is a PROVEN LOWER BOUND of at least 2^64, because the arithmetic
    //  that produces it is exact and checked, so against an accepted limit of this dimension it is
    //  a real overrun and is refused as such; it is never compared against a substituted number,
    //  and without an accepted limit nothing is compared and nothing is refused. A count whose true
    //  value is not a lower bound of the dimension -- a bounded repeat of zero copies, whose count
    //  is exactly zero whatever its operand measures -- never takes that branch: the comparison is
    //  made on the count of the complete expansion, not on an intermediate one.
    MeasuredParameter<std::uint64_t> maxExpansionCount;
    //  The number of states of the determinised and minimised automaton of the pattern, which is
    //  what Spec 8.2 counts. It is neither the size of the declaration tree, nor the number of
    //  distinct subpatterns: the interned subpattern count is reported as its own measurement. This
    //  batch measures the count from the compiled representation, under its own working-set bound
    //  and only when a measured dimension asks for it.
    //
    //  The accepted limit of this dimension is enforced on a measured count above it, and on a
    //  PROVEN LOWER BOUND of the count above it. The lower bound is content and is derived without
    //  the construction: a language whose longest acceptable trace has length `L` has a minimal
    //  automaton with at least `L + 1` states, because the walk of a longest accepted trace visits
    //  one state per element and cannot revisit one. When `L` itself has no representable value but
    //  is finite, the count is then provably at least 2^64 and is above every representable
    //  accepted limit. The refusal is the `budget_exceeded` of this dimension; no substituted
    //  number is ever compared, and no comparison is made without an accepted bound.
    //
    //  INCOMPLETE GATE. The dimension is still NOT enforced in general and this batch does not
    //  claim otherwise: when the construction does not complete, the absence is reported through
    //  `CompiledPattern::stateCount()`, and unless the proven lower bound above is itself over the
    //  accepted bound nothing about the content is decided -- no claim is made that the content
    //  stays inside `maxStateCount`. The gate stays incomplete and is reported as such rather than
    //  being closed with an internal construction bound, which is a limit of the measurement and
    //  never a content threshold.
    MeasuredParameter<std::uint64_t> maxStateCount;
    //  The deterministic work count of the compile itself: the time dimension. The count only
    //  grows, so its accepted bound is applied as a RELATIVE early stop while the compile runs: the
    //  moment the count is known to be above an accepted bound the compile stops with that
    //  dimension's `budget_exceeded`, and with no accepted bound nothing is compared and nothing is
    //  refused. A count with no representable value is a proven lower bound of at least 2^64, since
    //  the accumulation that produces it is exact, so it is above every accepted bound as well.
    MeasuredParameter<std::uint64_t> maxEvaluationSteps;
    //  The deterministic size of the compiled representation in this module's own accounting units:
    //  the memory dimension. It is an internal accounting of an internal representation, not a wire
    //  size and not an encoding. Its accepted bound is applied exactly like `maxEvaluationSteps`.
    MeasuredParameter<std::uint64_t> maxCompiledBytes;
};

//  The declared arm / deadline surface a compiled Pattern has to be contained in (plan S7A-3 item
//  2: "the containment relation between the Pattern and the arm / deadline").
//
//  The Pattern declaration carries no timing field of its own, so the containment is checked
//  against what the requirement declares: the actions its arms may name, and the declared element
//  capacity of its armed window and of its deadline. Both capacities are measured-or-pending, so an
//  unmeasured window never refuses a legal pattern and an accepted one is enforced exactly.
//
//  The assembly path consumes this gate only after it has a requirement-level capacity declaration.
//  A pending or otherwise unavailable measurement is reported as `gateIncomplete`, not as a
//  content-overrun diagnostic.
struct PatternArmBound final {
    //  The action references the requirement declares. Empty means the requirement declares no arm
    //  set to be contained in, and the atom membership check is not applicable.
    std::vector<std::string> declaredActionRefs;
    //  The maximum number of elements the requirement's armed window admits. A pattern that accepts
    //  a trace longer than this does not fit, so the comparison uses the longest acceptable length.
    MeasuredParameter<std::uint64_t> maxArmElements;
    //  The maximum number of elements the declared deadline can cover. Same comparison, on the
    //  deadline side.
    MeasuredParameter<std::uint64_t> maxDeadlineElements;
};

//  The result of the Pattern-to-arm/deadline containment gate. `gateIncomplete` is a successful
//  return carrying a non-diagnostic state: the gate could not prove containment from the available
//  measurements, so an assembly consumer must reject the product conservatively.
enum class ContainmentStatus : std::uint8_t {
    contained,
    gateIncomplete,
};

//  The read-only compiled predicate of one Pattern declaration.
//
//  The compiled value is an immutable handle over the expanded and interned pattern, plus the
//  counts the budget check measured. `matches` is the reference predicate of this batch: it decides
//  whether one observed atom trace is matched by the whole pattern, and `leftmostAcceptingArm`
//  exposes the leftmost-first decision itself instead of only its boolean consequence.
//
//  The language of the primitives is fixed here, because two of them need their rule written down
//  rather than left to a reader:
//
//    * `skip` and `instant` denote the same language, the empty word, and no trace can tell them
//      apart: they are observably different in identity only, as two distinct declaration kinds
//      that never normalize into each other, and the compiled product keeps them as distinct
//      interned subpatterns. Nothing may treat them as different languages;
//    * `complement` accepts exactly the prefixes that none of its operands reaches, so it consumes
//    a
//      prefix rather than the whole remainder and composes with what follows it in a `sequence`. In
//      particular `sequence(complement(X), Y)` accepts `w` when some split `w = p . s` has `p` not
//      reached by any operand and `s` accepted by `Y`. The end-anchored reading, in which a
//      complement swallows the entire remaining trace, is the special case in which everything
//      after it matches the empty word, and it is not the rule;
//    * a `bounded repeat` expands over every admissible copy count, and its copy order is the
//      increasing copy index, so the accepted set is the union over `k` in `[minimum, maximum]` of
//      `k` copies of the operand. A copy does not have to consume an element: an operand that
//      accepts the empty word is compiled and matched like any other, its fixed point ends the
//      expansion, and the declared finite bound does not depend on a copy consuming anything.
// Internal executable language table. Undefined transitions use optional indices; the explicit
// alphabet excludes the reference evaluator's unnamed-symbol class.
struct PatternExecutionProgram final {
    std::vector<std::string> atomRefs;
    std::size_t start;
    std::vector<std::uint8_t> accepting;
    std::vector<std::optional<std::size_t>> transitions;
    std::vector<std::uint8_t> live;
};

class CompiledPattern final {
  public:
    CompiledPattern(const CompiledPattern&) noexcept = default;
    CompiledPattern(CompiledPattern&&) noexcept = default;
    auto operator=(const CompiledPattern&) noexcept -> CompiledPattern& = default;
    auto operator=(CompiledPattern&&) noexcept -> CompiledPattern& = default;
    ~CompiledPattern() noexcept = default;

    //  Non-deterministic matching is fixed to `leftmost-first` (ABI domain 3), so the compiled
    //  predicate reports that single policy and no second one.
    [[nodiscard]] auto matchPolicy() const noexcept -> MatchPolicy;

    //  The measured dimensions of this compile. A count is a measurement of the content, never a
    //  limit on it.
    [[nodiscard]] auto declarationDepth() const noexcept -> std::uint64_t;
    //  The number of primitive instances of the complete prepare-time expansion, that is, the sum
    //  over every admissible copy count of a bounded repeat (Spec 3.8.4). Absent exactly when that
    //  sum has no representable value, which is a measurement gap: it is neither a refusal nor a
    //  statement that the language is unbounded. A budget dimension a measurement accepted is still
    //  exceeded by such a count, because every representable accepted limit is smaller than it.
    [[nodiscard]] auto fullyExpandedCount() const noexcept -> std::optional<std::uint64_t>;
    //  The largest number of primitive instances one admissible expansion branch contains, which is
    //  the `maximum x child` upper bound of one repeat copy set. It is the coarse companion of
    //  `fullyExpandedCount` and the number the representability check of the declared bounds uses;
    //  it is not an upper bound of the complete expansion, which the sum above bounds. Absent
    //  exactly when the count has no representable value. This count has no budget dimension of its
    //  own, so its absence is a measurement gap and never a refusal.
    [[nodiscard]] auto largestBranchExpansion() const noexcept -> std::optional<std::uint64_t>;
    //  The number of distinct interned subpatterns. It is the size of the compiled representation
    //  after interning and is explicitly NOT the state count of Spec 8.2, which counts the
    //  determinised and minimised automaton; the two numbers answer different questions and both
    //  are reported.
    [[nodiscard]] auto internedSubpatternCount() const noexcept -> std::uint64_t;
    //  The number of states of the determinised and minimised automaton of the pattern (Spec 8.2).
    //  It is measured from the compiled representation when it is asked for, so a compile that is
    //  never asked for it does not pay for it, and it is a function of the content alone. Absent
    //  when this batch's construction did not complete inside its own working-set bound, which is a
    //  measurement gap and neither a content limit nor a refusal by itself: the budget check
    //  refuses this dimension only on a count, or on a proven lower bound of it, that is above an
    //  accepted bound.
    [[nodiscard]] auto stateCount() const -> std::optional<std::uint64_t>;
    //  The deterministic work count of the compile: the time dimension. Absent exactly when the
    //  count has no representable value, which is a measurement gap. Against a `maxEvaluationSteps`
    //  a measurement accepted the count is compared while it grows (the relative early stop of
    //  `PatternCompileBudget`), and every representable accepted bound is smaller than a count that
    //  has no representable value.
    [[nodiscard]] auto evaluationSteps() const noexcept -> std::optional<std::uint64_t>;
    //  The accounting size of the compiled representation in this module's own units. It covers the
    //  compiled product only: it is a compile-time budget, and it says nothing about the reference
    //  evaluator's runtime working set, which `referenceEvaluatorWorkingBytes` reports separately.
    //  Absent exactly when the count has no representable value; the `maxCompiledBytes` dimension
    //  treats that absence the same way `maxEvaluationSteps` does.
    [[nodiscard]] auto compiledBytes() const noexcept -> std::optional<std::uint64_t>;

    //  The deterministic accounting size of the reference evaluator's working set for one trace of
    //  `traceLength` observed atoms, in the same accounting units as `compiledBytes`. The reference
    //  evaluator of this batch allocates one position set per compiled subpattern per trace
    //  position, so this number grows with the product of the two; it is reported because the
    //  compile budget deliberately does not cover it and no caller may read `compiledBytes` as a
    //  promise about the memory a match needs. Absent when the product has no representable value.
    //  It is a measurement of this module's reference evaluator, not a limit and not a promise
    //  about a host allocator.
    [[nodiscard]] auto referenceEvaluatorWorkingBytes(std::size_t traceLength) const noexcept
        -> std::optional<std::uint64_t>;

    //  The shortest and the longest atom trace the pattern accepts. Both are absent when the
    //  measurement has no representable value; the longest is absent as well when no finite bound
    //  exists (a `complement` consumes the remaining trace). Both absences are the same statement
    //  for a reader -- "no finite bound is available from this measurement" -- and both are exactly
    //  what a deadline containment check has to know about.
    [[nodiscard]] auto minimumTraceLength() const noexcept -> std::optional<std::uint64_t>;
    [[nodiscard]] auto maximumTraceLength() const noexcept -> std::optional<std::uint64_t>;

    //  Every atom reference the pattern names, sorted and deduplicated. This is the arm set the
    //  containment check reads.
    [[nodiscard]] auto atomRefs() const -> std::vector<std::string>;
    [[nodiscard]] auto executionProgram() const -> core::Result<PatternExecutionProgram>;

    //  The reference predicate: `trace` is the observed atom reference sequence, and the pattern
    //  matches when it accepts the whole trace. The decision is total and terminating for every
    //  declared repeat bound, including the largest representable one, because the reachable
    //  positions of a repeat converge.
    [[nodiscard]] auto matches(const std::vector<std::string>& trace) const -> bool;

    //  The arm of the root `choice` the leftmost-first matcher selects: the lowest operand index
    //  that accepts the trace. Absent when the root is not a choice, or when no arm accepts. Arm
    //  order is the declared operand order, which is the observable form of the fixed policy.
    [[nodiscard]] auto leftmostAcceptingArm(const std::vector<std::string>& trace) const
        -> std::optional<std::size_t>;

  private:
    friend auto compilePattern(const PatternDeclaration& pattern,
                               const PatternCompileBudget& budget) -> core::Result<CompiledPattern>;

    explicit CompiledPattern(
        std::shared_ptr<const detail::CompiledPatternStorage> storage) noexcept;

    std::shared_ptr<const detail::CompiledPatternStorage> storage_;
};

//  Compiles one Pattern declaration with a budget in which every bound is still pending, which is
//  the state the batch is in before S7A-9 accepts a number: the intrinsic checks (termination,
//  static finiteness of a bounded repeat, representability of the expansion count and the rejection
//  of every excluded content form) all run, and no numeric threshold is applied.
[[nodiscard]] auto compilePattern(const PatternDeclaration& pattern)
    -> core::Result<CompiledPattern>;

//  Compiles one Pattern declaration and enforces the budget dimensions that were measured.
[[nodiscard]] auto compilePattern(const PatternDeclaration& pattern,
                                  const PatternCompileBudget& budget)
    -> core::Result<CompiledPattern>;

//  Checks that a compiled Pattern is contained in the declared arm / deadline surface. A rejection
//  is a stable diagnostic only for an atom outside the declared arm set or a measured capacity that
//  the Pattern provably exceeds. Missing/pending measurements and an unavailable finite maximum
//  return `gateIncomplete`.
[[nodiscard]] auto checkPatternContainment(const CompiledPattern& pattern,
                                           const PatternArmBound& bound)
    -> core::Result<ContainmentStatus>;

//  The compiled product of one measure component: the frozen category derived from the declared
//  phase, the presence state of the grade table, and the declared grade tokens themselves. The
//  tokens are carried, not evaluated: no grade value is computed anywhere in this batch.
struct CompiledMeasureComponent final {
    PhaseKind phase;
    //  Derived from `phase` (Spec 3.20 rule 3), never copied from a caller-assigned token.
    FactCategory category;
    //  Whether the component declared a grade table. Presence only; it is not a computed grade.
    GradePresence grade;
    //  The declared grade tokens as declared, opaque: they are content, so they take part in chart
    //  / content identity and in the semantic diff, but the scale, the tolerance and the
    //  aggregation rule are CM-S10 / P1-07 and are neither interpreted nor defaulted nor evaluated
    //  here. Empty exactly when `grade` is `absent`.
    std::vector<std::string> declaredGradeTokens;

    friend auto operator==(const CompiledMeasureComponent&,
                           const CompiledMeasureComponent&) noexcept -> bool = default;
};

//  The requirement-side phase surface a Measure declaration is contained in (plan S7A-3 item 3).
struct MeasurePhaseContext final {
    //  The phases the requirement declares. A measure component that quantifies a phase the
    //  requirement did not declare has no phase to belong to.
    std::vector<PhaseDeclaration> declaredPhases;
    //  Whether the content's semantics require Release / tail. When they do and no tail phase is
    //  declared, the measure is refused: the batch never infers a tail from an implicit legacy
    //  profile, and it never generates a second requirement for one.
    bool requiresReleaseTailSemantics;
    //  The field-path prefix the rejection reports, so the assembly path keeps naming the
    //  requirement the measure belongs to. Empty reports the measure on its own.
    std::string fieldPathPrefix;
};

//  The read-only compiled measure product of one Measure declaration.
//
//  Stage 7A covers Tap and Hold head / body plus the explicitly declared Release / tail, derives
//  each component's `FactCategory` from its phase, and carries the grade table each component
//  declared. It does NOT compute grades: this batch stores the declared grade tokens as opaque
//  content, they take part in chart / content identity and in the semantic diff, and the grade
//  scale, the tolerance and the aggregation rule stay CM-S10 / P1-07. Grade evaluation is not
//  implemented here, and nothing in this type may be read as a computed grade.
//
//  A component whose content declared no grade table stays `absent`: the product has no default
//  table, no implicit upgrade and no second result dimension. `Outcome` is the frozen two-value set
//  of `kStage7AOutcomes`.
class CompiledMeasure final {
  public:
    CompiledMeasure(const CompiledMeasure&) noexcept = default;
    CompiledMeasure(CompiledMeasure&&) noexcept = default;
    auto operator=(const CompiledMeasure&) noexcept -> CompiledMeasure& = default;
    auto operator=(CompiledMeasure&&) noexcept -> CompiledMeasure& = default;
    ~CompiledMeasure() noexcept = default;

    [[nodiscard]] auto componentCount() const noexcept -> std::uint64_t;

    //  The compiled components in canonical (phase, category) order. The declaration order of the
    //  components carries no semantics, so the product reports the normalized order.
    [[nodiscard]] auto components() const -> std::vector<CompiledMeasureComponent>;

    //  The category of one component, or absent when the measure has no component for that phase.
    [[nodiscard]] auto gradeOf(PhaseKind phase) const -> std::optional<CompiledMeasureComponent>;

  private:
    friend auto compileMeasure(const MeasureSpecDeclaration& measure,
                               const MeasurePhaseContext& context) -> core::Result<CompiledMeasure>;

    explicit CompiledMeasure(
        std::shared_ptr<const detail::CompiledMeasureStorage> storage) noexcept;

    std::shared_ptr<const detail::CompiledMeasureStorage> storage_;
};

//  Compiles one Measure declaration without a requirement-side phase context: the intrinsic checks
//  (the phase-derived category, the per-component grading and the uniqueness of a component) all
//  run, and the Release / tail requirement is the one the declaration itself carries.
[[nodiscard]] auto compileMeasure(const MeasureSpecDeclaration& measure)
    -> core::Result<CompiledMeasure>;

//  Compiles one Measure declaration against the phase surface of the requirement that owns it.
[[nodiscard]] auto compileMeasure(const MeasureSpecDeclaration& measure,
                                  const MeasurePhaseContext& context)
    -> core::Result<CompiledMeasure>;

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

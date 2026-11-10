#pragma once

//  Judgement typed kernel - source documents, prepared identity declarations and identity bytes.

#include <cuexis/core/result.hpp>
#include <cuexis/judgement/gameplay_graph.hpp>

#include <cstdint>
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
namespace detail {
struct IdentityFactory;
}

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
        static const std::vector<std::byte> empty;
        return bytes_ ? *bytes_ : empty;
    }
    [[nodiscard]] auto empty() const noexcept -> bool {
        return bytes().empty();
    }
    //  A stable hexadecimal rendering, for diagnostics and for test evidence. It is a rendering of
    //  the bytes and never a replacement for the field-by-field semantic diff of Spec 3.5.
    [[nodiscard]] auto toHex() const -> std::string;

    friend auto operator==(const CanonicalIdentityBytes& a,
                           const CanonicalIdentityBytes& b) noexcept -> bool {
        return a.bytes_ == b.bytes_ || a.bytes() == b.bytes();
    }

  private:
    friend struct detail::IdentityFactory;
    friend auto makeRuntimePreparedIdentity(const CanonicalGameplayGraph&,
                                            const PreparedIdentityDeclarations&,
                                            const InputMappingProfile&, const LatePolicyParameters&,
                                            std::string_view) -> PreparedIdentity;
    friend auto makeChartIdentity(const CanonicalGameplayGraph& graph) -> ChartIdentity;
    friend auto makeContentIdentity(const CanonicalGameplayGraph& graph) -> ContentIdentity;
    friend auto makePreparedIdentity(const CanonicalGameplayGraph& graph,
                                     const PreparedIdentityDeclarations& declarations)
        -> PreparedIdentity;

    explicit CanonicalIdentityBytes(std::vector<std::byte> bytes)
        : bytes_(std::make_shared<const std::vector<std::byte>>(std::move(bytes))) {}

    std::shared_ptr<const std::vector<std::byte>> bytes_;
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
    friend struct detail::IdentityFactory;
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
    friend struct detail::IdentityFactory;
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

} // namespace cuexis::judgement

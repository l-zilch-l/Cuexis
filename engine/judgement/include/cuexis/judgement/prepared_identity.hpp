#pragma once

//  Judgement runtime - the declared identity components of one prepared judgement.
//
//  These four value declarations are the only identity input the running kernel and an independent
//  runtime consumer need. They carry the caller-declared tokens exactly as declared: the three
//  components the assembler cannot derive from the graph (engine, ruleset, session) and their
//  by-value aggregate. The generated identity bytes are produced elsewhere; this header contains no
//  generator, no graph and no compile context, so a consumer that only links the judgement runtime
//  can construct `CompiledGameplayView::identities` itself.

#include <optional>
#include <string>
#include <vector>

namespace cuexis::judgement {

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

} // namespace cuexis::judgement

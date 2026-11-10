#pragma once

//  Canonical gameplay graph - reference closures, declaration namespace and derived closures.

#include <cuexis/core/result.hpp>
#include <cuexis/judgement/input_boundary.hpp>
#include <cuexis/judgement/timebase.hpp>

#include <array>
#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::judgement {

//  ---------------------------------------------------------------------------------------------
//  Reference attribution (P1-14 / Spec 3.8.6, P2-07)
//  ---------------------------------------------------------------------------------------------

//  The three classes a gameplay reference can belong to (Spec 3.8.6 rule 1). A reference belongs to
//  exactly one of them; it never crosses a class and is never registered twice.
//
//  P2-07 unifies the overlapping names of the identity table into three separately named things.
//  The mapping used by this batch is explicit and one-directional:
//
//    REF0 / judgement closure        <->  the judgement projection that the chart component and the
//                                         `content-artifact identity` carriers consume;
//    manifest / presentation closure <->  the presentation contribution, which never enters a
//                                         judgement identity (Spec 3.8.6 rule 3);
//    diagnostic / source map         <->  `DiagnosticMap`, which enters no identity at all.
enum class ReferenceClosure : std::uint8_t {
    judgementRef0,
    presentationManifest,
    diagnosticMap,
};

//  The declared kind of one gameplay reference. The kind is what decides the closure, and the
//  decision is made by prepare and never by the host, the presentation layer or a session (Spec
//  3.8.6 rule 5). The enumeration lists exactly the rows of the Spec 3.8.6 attribution matrix.
enum class ReferenceKind : std::uint8_t {
    //  Judgement-necessary references: a missing or changed one changes a Fact, a Score, a Combo,
    //  the statistics, the Fact order, an identity or a rejection.
    requirement,
    pattern,
    measure,
    resource,
    claimKey,
    emission,
    requirementIdentityProjection,
    timebaseProjection,
    latePolicyProjection,
    normalizationProfileProjection,
    solverProfileProjection,
    coordinationPolicyProjection,
    //  FactBinding judgement-side existence only. The binding detail stays presentation-side, which
    //  is exactly the "existence and judgement-domain decision only" cell of the matrix.
    factBindingExistence,
    //  Pure presentation references: a missing one still allows headless judgement.
    factBindingDetail,
    presentationResource,
    presentationOverride,
    //  Diagnosis, editors and migration only.
    sourceMapLocation,
};

//  The frozen attribution of one reference kind (Spec 3.8.6 matrix). It is a total function: every
//  kind has exactly one class, so a caller cannot pick a class for a kind.
[[nodiscard]] constexpr auto closureOf(ReferenceKind kind) noexcept -> ReferenceClosure {
    switch (kind) {
    case ReferenceKind::requirement:
    case ReferenceKind::pattern:
    case ReferenceKind::measure:
    case ReferenceKind::resource:
    case ReferenceKind::claimKey:
    case ReferenceKind::emission:
    case ReferenceKind::requirementIdentityProjection:
    case ReferenceKind::timebaseProjection:
    case ReferenceKind::latePolicyProjection:
    case ReferenceKind::normalizationProfileProjection:
    case ReferenceKind::solverProfileProjection:
    case ReferenceKind::coordinationPolicyProjection:
    case ReferenceKind::factBindingExistence:
        return ReferenceClosure::judgementRef0;
    case ReferenceKind::factBindingDetail:
    case ReferenceKind::presentationResource:
    case ReferenceKind::presentationOverride:
        return ReferenceClosure::presentationManifest;
    case ReferenceKind::sourceMapLocation:
        return ReferenceClosure::diagnosticMap;
    }
    return ReferenceClosure::diagnosticMap;
}

//  A request to register a reference in a named closure. Spec 3.8.6 rule 6 rejects a reference
//  whose attribution cannot be decided uniquely or that would cross classes, and the way to make
//  that a property of the code is to make the request the caller-supplied part: a request that
//  names the class the kind already owns succeeds and returns that class, and any other request is
//  a stable rejection rather than a re-registration.
[[nodiscard]] auto declareReference(ReferenceKind kind, ReferenceClosure requested)
    -> core::Result<ReferenceClosure>;

//  ---------------------------------------------------------------------------------------------
//  Declaration namespace identity (P1-02)
//  ---------------------------------------------------------------------------------------------

//  The stable identity of one declaration (P1-02): the identity of the source document it was
//  declared in, plus its declaration ordinal inside that document.
//
//  Both halves travel with the declaration. The ordinal is never a position in the array the
//  assembler was handed, so permuting the input array cannot renumber anything, and the canonical
//  order below is the stable id order rather than the input order.
struct StableDeclarationId final {
    std::string sourceDocumentId;
    std::uint32_t declarationOrdinal;

    friend auto operator==(const StableDeclarationId&, const StableDeclarationId&) noexcept
        -> bool = default;
    friend auto operator<=>(const StableDeclarationId&, const StableDeclarationId&) noexcept
        -> std::strong_ordering = default;
};

//  What a named declaration declares. The merge treats every kind alike: a name is unique in the
//  merged namespace whatever it names.
enum class DeclarationKind : std::uint8_t {
    requirement,
    patternDefinition,
    measureDefinition,
    resourceRecord,
    judgementDomain,
    solverProfile,
};

//  A reference from one declaration to another.
//
//  `sameDocument` resolves inside the referring declaration's own source document. An
//  `explicitCrossDocument` reference names the other document as well; P1-02 requires a
//  cross-invocation reference to be explicit, so a cross-document reference that leaves the
//  document identity empty is a stable rejection and is never resolved by looking the name up
//  somewhere else.
enum class ReferenceScope : std::uint8_t {
    sameDocument,
    explicitCrossDocument,
};

struct DeclarationRef final {
    ReferenceScope scope;
    //  Required when `scope` is `explicitCrossDocument`; absent (empty) otherwise.
    std::string sourceDocumentId;
    std::uint32_t declarationOrdinal;

    friend auto operator==(const DeclarationRef&, const DeclarationRef&) noexcept -> bool = default;
    friend auto operator<=>(const DeclarationRef&, const DeclarationRef&) noexcept
        -> std::strong_ordering = default;
};

//  One declaration of the merged global namespace. It owns its name and its resolved references, so
//  the merged namespace is a value that outlives the request that produced it.
struct MergedDeclaration final {
    StableDeclarationId stableId;
    DeclarationKind kind;
    //  The global name. Two declarations with the same name are rejected, never merged (P1-02), so
    //  a name identifies at most one declaration here.
    std::string name;
    //  Resolved references, sorted by the referenced stable id. Every entry exists in the merged
    //  namespace: a dangling reference is an atomic prepare failure.
    std::vector<StableDeclarationId> references;
    //  The declared needs of this declaration (Spec 6.1 derivation input).
    std::vector<std::string> requiredFeatureIds;
    std::vector<std::string> requiredCapabilityIds;

    friend auto operator==(const MergedDeclaration&, const MergedDeclaration&) noexcept
        -> bool = default;
};

//  The merged global namespace (P1-02), sorted by stable id. The sort is what makes the merge
//  invariant under a permutation of the input: the result is a function of the stable ids alone.
struct MergedNamespace final {
    std::vector<MergedDeclaration> declarations;

    friend auto operator==(const MergedNamespace&, const MergedNamespace&) noexcept
        -> bool = default;
};

//  ---------------------------------------------------------------------------------------------
//  Capability and feature references
//  ---------------------------------------------------------------------------------------------

//  A capability reference declared by content (ABI domain 3 `CapabilityRef`). It is an opaque
//  stable id plus the revision the declaration named. The capability registry entry field set is
//  unfrozen (CM-X01, first consumer S7A-7), so this batch carries nothing but the two tokens and
//  claims no ordering, no range and no monotonicity rule beyond exact token equality.
//
//  An empty revision is the documented absence marker "this declaration named no revision". It is
//  carried as declared and is never defaulted to a value this batch would have to invent.
struct CapabilityRef final {
    std::string capabilityId;
    std::string revision;

    friend auto operator==(const CapabilityRef&, const CapabilityRef&) noexcept -> bool = default;
    friend auto operator<=>(const CapabilityRef&, const CapabilityRef&) noexcept
        -> std::strong_ordering = default;
};

//  A capability of the physical carrier (Packed META `requiredFeatures`). Unfrozen and opaque: this
//  batch invents no feature registry and no feature semantics.
struct FeatureRef final {
    std::string featureId;

    friend auto operator==(const FeatureRef&, const FeatureRef&) noexcept -> bool = default;
    friend auto operator<=>(const FeatureRef&, const FeatureRef&) noexcept
        -> std::strong_ordering = default;
};

//  What one declaration states it needs. The assembler is the only thing that derives a closure
//  from these declarations, and it never adds a token the content did not state (Spec 6.2 rule 1:
//  the compiler must not silently fill the gap).
struct RequiredRefs final {
    std::vector<FeatureRef> features;
    std::vector<CapabilityRef> capabilities;

    friend auto operator==(const RequiredRefs&, const RequiredRefs&) noexcept -> bool = default;
};

//  The declared closure tables of a prepared graph. They are separate values from the derived
//  closures below and there is deliberately no operation that merges them (ABI domain 8).
struct DeclaredCapabilitySet final {
    std::vector<CapabilityRef> capabilities;

    friend auto operator==(const DeclaredCapabilitySet&, const DeclaredCapabilitySet&) noexcept
        -> bool = default;
};

struct FeatureClosure final {
    std::vector<FeatureRef> features;

    friend auto operator==(const FeatureClosure&, const FeatureClosure&) noexcept -> bool = default;
};

//  The complete closure the offline assembler derived from the canonical graph, the ruleset binding
//  and the presentation closure contribution (Spec 6.1). Sorted and deduplicated, so the closure is
//  a set: the order a caller declared its needs in cannot reach it.
struct DerivedCapabilityClosure final {
    std::vector<CapabilityRef> capabilities;

    friend auto operator==(const DerivedCapabilityClosure&,
                           const DerivedCapabilityClosure&) noexcept -> bool = default;
};

struct ResourceRef final {
    std::string resourceId;

    friend auto operator==(const ResourceRef&, const ResourceRef&) noexcept -> bool = default;
    friend auto operator<=>(const ResourceRef&, const ResourceRef&) noexcept
        -> std::strong_ordering = default;
};

//  Every resource the graph references, sorted by resource id. A reference that does not resolve to
//  a `ResourceRecord` is a dangling reference and is rejected, so every closure entry resolves.
struct ResourceClosure final {
    std::vector<ResourceRef> resources;

    friend auto operator==(const ResourceClosure&, const ResourceClosure&) noexcept
        -> bool = default;
};

//  The capability needs of the ruleset binding and of the presentation closure, as declarations.
//
//  Not every source of the Spec 6.1 derivation chain is chart content: the ruleset binding and the
//  presentation closure contribute needs of their own. Only their *needs* travel here, never their
//  content (no material, no shader, no animation, no audio), so carrying them cannot put a
//  presentation field into the judgement closure.
struct ClosureContributions final {
    std::vector<FeatureRef> features;
    std::vector<CapabilityRef> capabilities;

    friend auto operator==(const ClosureContributions&, const ClosureContributions&) noexcept
        -> bool = default;
};

} // namespace cuexis::judgement

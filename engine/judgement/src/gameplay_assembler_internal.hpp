#pragma once

// Shapes and declarations shared by the gameplay assembler units: the diagnostic token shape,
// the rejection mapping, the identity byte writer and every helper another unit calls.

//  Judgement typed kernel - S7A-3 (first half) offline typed assembler implementation.
//
//  This file is the assembly half of S7A-3: the entry and version gates, the P1-02 merge of the
//  declaration namespace, the declaration validation of the Spec 3.8.5 exclusion list, the closure
//  derivation and the Spec 6.2 completeness rule, the canonical identity bytes of Spec 5.5, the
//  content-profile count, and the atomic publication boundary.
//
//  Five rules shape every function below.
//
//    1. There is one assembling path. A packed-chart entry and a gameplay-graph entry that carry
//    the
//       same typed content differ in nothing but the diagnostics they carry, because nothing here
//       branches on the entry kind except the ABI domain 8 playback rule.
//
//    2. Order is never semantics. Every table is stabilized into canonical order before anything
//       reads it, and the merge keys declarations by (source document identity, declaration
//       ordinal) rather than by their position in the array it was handed.
//
//    3. Nothing is filled in. A capability or feature the content needs but did not declare is a
//       stable failure with the identity_closure_incomplete category, never an addition.
//
//    4. Failure is atomic. assembleGameplay returns a value or an error, and assembleInto assigns
//       only after that value exists, so a refused assembly cannot leave a half-published graph.
//
//    5. Prepare products are immutable and explicit. Unresolved source values, illegal resource
//       states and missing stable claim keys are rejected rather than filled with defaults.

#include <cuexis/judgement/gameplay_assembler.hpp>

#include "gameplay_closure.hpp"
#include "identity_factory.hpp"
#include "pattern_dfa.hpp"
#include "source_codes.hpp"

#include <cuexis/judgement/diagnostic.hpp>

#include <cuexis_internal/presentation_metrics.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace cuexis::judgement {

struct DiagnosticTokens final {
    std::string_view code;
    std::string_view category;
    std::string_view summary;
    std::string_view section;
    std::string_view path;
    std::string_view capabilityId = codes::kAbsent;
    std::string_view remediation = codes::kAbsent;
    std::string_view identityComponent = codes::kAbsent;
    std::string_view identityToken = codes::kAbsent;
};

struct UnsupportedMapping final {
    std::string_view code;
    std::string_view category;
    std::string_view capabilityId;
    std::string_view remediation;
};

class IdentityByteWriter final {
  public:
    IdentityByteWriter() = default;
    explicit IdentityByteWriter(std::string_view signature) {
        writeText(signature);
    }

    void beginComponent(std::string_view name) {
        writeText(name);
    }

    void writeBool(bool value) {
        bytes_.push_back(value ? std::byte{0x01} : std::byte{0x00});
    }

    void writeUnsigned(std::uint64_t value) {
        for (int shift = 56; shift >= 0; shift -= 8) {
            bytes_.push_back(std::byte{static_cast<unsigned char>((value >> shift) & 0xFFU)});
        }
    }

    void writeCount(std::size_t count) {
        writeUnsigned(static_cast<std::uint64_t>(count));
    }

    void writeSigned(std::int64_t value) {
        //  Zigzag: a negative value is mapped to an odd number and a non-negative one to an even
        //  number, so the signed domain has exactly one representation per value without relying on
        //  the width of a signed right shift.
        const std::uint64_t magnitude = value < 0 ? (~static_cast<std::uint64_t>(value) + 1U)
                                                  : static_cast<std::uint64_t>(value);
        writeUnsigned(value < 0 ? (magnitude << 1U) - 1U : (magnitude << 1U));
    }

    void writeText(std::string_view text) {
        writeCount(text.size());
        for (const char character : text) {
            bytes_.push_back(std::byte{static_cast<unsigned char>(character)});
        }
    }

    [[nodiscard]] auto take() -> std::vector<std::byte> {
        return std::move(bytes_);
    }

    void reserveAdditional(std::size_t additional) {
        if (additional > bytes_.max_size() - bytes_.size())
            throw std::length_error{"Identity bytes exceed vector capacity"};
        bytes_.reserve(bytes_.size() + additional);
    }

    void append(std::span<const std::byte> bytes) {
        bytes_.insert(bytes_.end(), bytes.begin(), bytes.end());
    }

  private:
    std::vector<std::byte> bytes_;
};

// Counts metadata without serializing a second large projection or provenance buffer.
class IdentitySizeWriter final {
  public:
    void beginComponent(std::string_view name) {
        writeText(name);
    }
    void writeBool(bool) {
        add(1);
    }
    void writeUnsigned(std::uint64_t) {
        add(8);
    }
    void writeCount(std::size_t) {
        add(8);
    }
    void writeText(std::string_view text) {
        add(8);
        add(text.size());
    }
    auto size() const noexcept -> std::size_t {
        return size_;
    }

  private:
    void add(std::size_t amount) {
        if (amount > std::numeric_limits<std::size_t>::max() - size_)
            throw std::length_error{"Identity size overflow"};
        size_ += amount;
    }
    std::size_t size_{};
};

template <typename Writer>
void writeFeatureRefs(Writer& writer, const std::vector<FeatureRef>& refs) {
    writer.writeCount(refs.size());
    for (const auto& ref : refs) {
        writer.writeText(ref.featureId);
    }
}

template <typename Writer>
void writeCapabilityRefs(Writer& writer, const std::vector<CapabilityRef>& refs) {
    writer.writeCount(refs.size());
    for (const auto& ref : refs) {
        writer.writeText(ref.capabilityId);
        writer.writeText(ref.revision);
    }
}

[[maybe_unused]] inline void writeResourceRefs(IdentityByteWriter& writer,
                                               const std::vector<ResourceRef>& refs) {
    writer.writeCount(refs.size());
    for (const auto& ref : refs) {
        writer.writeText(ref.resourceId);
    }
}

template <typename Writer>
void writeStableIds(Writer& writer, const std::vector<StableDeclarationId>& ids) {
    writer.writeCount(ids.size());
    for (const auto& id : ids) {
        writer.writeText(id.sourceDocumentId);
        writer.writeUnsigned(id.declarationOrdinal);
    }
}

[[nodiscard]] inline auto sameCapability(const CapabilityRef& left, const CapabilityRef& right)
    -> bool {
    return left.capabilityId == right.capabilityId && left.revision == right.revision;
}

[[nodiscard]] inline auto containsCapability(const std::vector<CapabilityRef>& refs,
                                             const CapabilityRef& wanted) -> bool {
    return std::any_of(refs.begin(), refs.end(), [&](const CapabilityRef& candidate) {
        return sameCapability(candidate, wanted);
    });
}

[[nodiscard]] inline auto containsFeature(const std::vector<FeatureRef>& refs,
                                          const FeatureRef& wanted) -> bool {
    return std::any_of(refs.begin(), refs.end(), [&](const FeatureRef& candidate) {
        return candidate.featureId == wanted.featureId;
    });
}

template <typename Value> void sortUnique(std::vector<Value>& values) {
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
}

struct MergedNamespaceBuild final {
    MergedNamespace value;
    std::vector<std::size_t> byName;
};

struct PatternDeclarationShape final {
    std::uint64_t nodeCount{0};
    std::uint64_t declarationDepth{0};
};

namespace assembler_detail {

[[nodiscard]] auto rejection(const DiagnosticTokens& tokens) -> core::Error;
[[nodiscard]] auto declarationInvalidError(std::string_view code, std::string_view section,
                                           std::string_view path, std::string_view summary)
    -> core::Error;
[[nodiscard]] auto valueError(std::string_view code, std::string_view section,
                              std::string_view path, std::string_view summary) -> core::Error;
[[nodiscard]] auto versionError(std::string_view code, std::string_view path,
                                std::string_view summary) -> core::Error;
[[nodiscard]] auto capabilityRejection(std::string_view code, std::string_view category,
                                       std::string_view section, std::string_view path,
                                       std::string_view summary, std::string_view capabilityId,
                                       std::string_view remediation) -> core::Error;
[[nodiscard]] auto closureIncompleteError(std::string_view path, std::string_view summary)
    -> core::Error;
[[nodiscard]] auto declarationKindToken(DeclarationKind kind) -> std::string_view;
[[nodiscard]] auto patternPrimitiveToken(PatternPrimitive primitive) -> std::string_view;
[[nodiscard]] auto matchPolicyToken(MatchPolicy policy) -> std::string_view;
[[nodiscard]] auto phaseKindToken(PhaseKind kind) -> std::string_view;
[[nodiscard]] auto claimIntentToken(ResourceClaimIntent intent) -> std::string_view;
[[nodiscard]] auto graceOverrideToken(GraceOverrideMode mode) -> std::string_view;
[[nodiscard]] auto gracePolicyToken(GraceResolutionPolicy policy) -> std::string_view;
[[nodiscard]] auto frameResolutionToken(FrameResolution resolution) -> std::string_view;
[[nodiscard]] auto relationKindToken(RelationKind kind) -> std::string_view;
[[nodiscard]] auto unsupportedKindToken(UnsupportedContentKind kind) -> std::string_view;
[[nodiscard]] auto lateEventPolicyToken(LateEventPolicy policy) -> std::string_view;
[[nodiscard]] auto mapUnsupported(UnsupportedContentKind kind) -> UnsupportedMapping;

} // namespace assembler_detail

using namespace assembler_detail;

template <typename Writer>
void writeContentProvenance(Writer& writer, const CanonicalGameplayGraph& graph) {
    writer.writeCount(graph.sourceClosure.sourceDocumentIds.size());
    for (const auto& documentId : graph.sourceClosure.sourceDocumentIds) {
        writer.writeText(documentId);
    }
    writer.writeText(graph.sourceClosure.compilerProfileToken);
    writer.writeCount(graph.mergedNamespace.declarations.size());
    for (const auto& declaration : graph.mergedNamespace.declarations) {
        writer.writeText(declaration.stableId.sourceDocumentId);
        writer.writeUnsigned(declaration.stableId.declarationOrdinal);
        writer.writeText(declarationKindToken(declaration.kind));
        writer.writeText(declaration.name);
        writeStableIds(writer, declaration.references);
        writer.writeCount(declaration.requiredFeatureIds.size());
        for (const auto& featureId : declaration.requiredFeatureIds) {
            writer.writeText(featureId);
        }
        writer.writeCount(declaration.requiredCapabilityIds.size());
        for (const auto& capabilityId : declaration.requiredCapabilityIds) {
            writer.writeText(capabilityId);
        }
    }
    writeFeatureRefs(writer, graph.declaredFeatures.features);
    writeFeatureRefs(writer, graph.derivedFeatures.features);
    writeFeatureRefs(writer, graph.closureContributions.features);
    writeCapabilityRefs(writer, graph.closureContributions.capabilities);
    writer.writeCount(graph.resourceClosure.resources.size());
    for (const auto& ref : graph.resourceClosure.resources) {
        writer.writeText(ref.resourceId);
    }
    writer.writeCount(graph.requirements.size());
    for (const auto& requirement : graph.requirements) {
        writer.writeText(requirement.stableId.sourceDocumentId);
        writer.writeUnsigned(requirement.stableId.declarationOrdinal);
        writer.writeText(requirement.pattern.patternId);
        writer.writeText(gracePolicyToken(requirement.grace.policy));
        writer.writeText(requirement.grace.inheritedFromDeclarationId);
        writer.writeBool(requirement.grace.allowChartGrace);
    }
}

void writeChartProjection(IdentityByteWriter& writer, const CanonicalGameplayGraph& graph);
[[nodiscard]] auto countPatternNodes(const PatternNodeDeclaration& root) -> std::uint64_t;
[[nodiscard]] auto findResource(const CanonicalGameplayGraph& graph, std::string_view resourceId)
    -> const ResourceRecord*;
[[nodiscard]] auto findDomain(const CanonicalGameplayGraph& graph, std::string_view domainId)
    -> const JudgementDomainRecord*;
[[nodiscard]] auto findSolverProfile(const CanonicalGameplayGraph& graph, std::string_view solverId)
    -> const SolverProfileDeclaration*;
[[nodiscard]] auto findDeclarationById(const MergedNamespace& merged, const StableDeclarationId& id)
    -> const MergedDeclaration*;
[[nodiscard]] auto findDeclarationByName(const MergedNamespace& merged,
                                         std::span<const std::size_t> byName, DeclarationKind kind,
                                         std::string_view name) -> const MergedDeclaration*;
[[nodiscard]] auto findFactBinding(const CanonicalGameplayGraph& graph, std::string_view bindingId)
    -> const FactBindingRef*;
[[nodiscard]] auto validateEntry(const AssemblyRequest& request) -> core::Result<void>;
[[nodiscard]] auto mergeNamespace(const AssemblyRequest& request)
    -> core::Result<MergedNamespaceBuild>;
void stabilizeGraph(CanonicalGameplayGraph& graph);
[[nodiscard]] auto measurePatternTree(const PatternNodeDeclaration& root)
    -> core::Result<PatternDeclarationShape>;
[[nodiscard]] auto validateContentDeclarations(const CanonicalGameplayGraph& graph,
                                               std::span<const std::size_t> declarationNames)
    -> core::Result<void>;
[[nodiscard]] auto validateClosures(const CanonicalGameplayGraph& graph,
                                    const AssemblyRequest& request) -> core::Result<void>;
[[nodiscard]] auto buildDiagnosticMap(const AssemblyRequest& request) -> DiagnosticMap;
auto makeChartIdentity(const CanonicalGameplayGraph& graph) -> ChartIdentity;
auto makeContentIdentity(const CanonicalGameplayGraph& graph) -> ContentIdentity;
auto compilePattern(const PatternDeclaration& pattern) -> core::Result<CompiledPattern>;
auto compilePattern(const PatternDeclaration& pattern, const PatternCompileBudget& budget)
    -> core::Result<CompiledPattern>;
auto checkPatternContainment(const CompiledPattern& pattern, const PatternArmBound& bound)
    -> core::Result<ContainmentStatus>;
auto compileMeasure(const MeasureSpecDeclaration& measure) -> core::Result<CompiledMeasure>;
auto compileMeasure(const MeasureSpecDeclaration& measure, const MeasurePhaseContext& context)
    -> core::Result<CompiledMeasure>;

} // namespace cuexis::judgement

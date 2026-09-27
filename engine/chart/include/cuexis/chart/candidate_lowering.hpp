#pragma once

// Internal candidate Chart v5 lowering contract. This header is intentionally not installed.
// It preserves typed candidate metadata beside the existing v4-compatible ChartRuntime.

#include <cuexis/chart/canonical_semantic_chart.hpp>
#include <cuexis/chart/chart_runtime.hpp>
#include <cuexis/chart/limits.hpp>
#include <cuexis/chart/packed_chart_tables.hpp>
#include <cuexis/chart/prepared_semantic_identity.hpp>
#include <cuexis/core/diagnostic.hpp>
#include <cuexis/core/result.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace cuexis::chart {

// Metadata for one lowered candidate entity. The vector containing these records is ordered
// exactly like CandidateRuntimeArtifact::runtime.objects. The runtimeIndex is repeated so a
// consumer can validate the alignment before using the metadata.
struct CandidateRuntimeObjectMetadata final {
    std::size_t runtimeIndex{};
    CanonicalEntityIdentity identity;
    std::optional<CanonicalEntityIdentity> parent;
    std::string executionId;
    std::vector<CanonicalRequirement> requirements;
    // Present only when the entity has a Renderable component. This is the single conversion
    // from the candidate u8 alpha to the runtime opacity domain [0, 1].
    std::optional<double> renderableOpacity;
};

// Candidate-only typed metadata retained beside the existing ChartRuntime. It deliberately
// carries Requirement values as internal chart data; it is not a judgement or scoring API.
struct CandidateRuntimeMetadata final {
    std::uint32_t flags{1};
    std::uint32_t candidateRevision{1};
    std::string compilerProfile{"candidate.static-tap-lanes4-v1"};
    packed::PackedSemanticIdentity semanticIdentity{};
    CanonicalResourceClosure resourceClosure;
    std::vector<CandidateRuntimeObjectMetadata> objects;
};

// Playback consumes the existing ChartRuntime for timing/world instantiation and this immutable
// carrier for candidate identity, Requirement and resource-closure data. No JSON, CXT or Packed
// bytes are exposed here, and no source document is reparsed during execution.
struct CandidateRuntimeArtifact final {
    ChartRuntime runtime;
    CandidateRuntimeMetadata metadata;
};

struct CandidateRuntimeArtifactResult final {
    std::optional<CandidateRuntimeArtifact> artifact;
    core::Diagnostics diagnostics;

    [[nodiscard]] auto hasValue() const noexcept -> bool {
        return artifact.has_value() && !diagnostics.hasErrors();
    }
};

// Validates the registered Foundation candidate subset and lowers one already-decoded typed
// semantic chart. Generated execution IDs use the v5g1 domain over Packed section 6.5 identity
// bytes. Existing v4 ChartRuntime compilation and identity behavior are unchanged.
[[nodiscard]] auto lowerCandidateRuntime(const CanonicalSemanticChart& chart,
                                         const ChartLimits& limits = {})
    -> CandidateRuntimeArtifactResult;

// Domain cuexis.prepared-semantic.v5.candidate.1. Duplicate asset IDs are rejected.
// Paths, devices, windows, clock mode and gain are not inputs.
[[nodiscard]] auto assembleCandidatePreparedSemanticIdentity(
    const packed::PackedSemanticIdentity& semanticIdentity,
    std::span<const PreparedResourceIdentityComponent> resourceIdentities)
    -> core::Result<CanonicalContentIdentity>;

} // namespace cuexis::chart

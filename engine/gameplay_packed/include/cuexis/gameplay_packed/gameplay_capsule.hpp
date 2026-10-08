#pragma once

#include <cuexis/chart/canonical_semantic_chart.hpp>
#include <cuexis/chart/limits.hpp>
#include <cuexis/judgement/gameplay_prepare.hpp>

#include <filesystem>
#include <span>

namespace cuexis::gameplay_packed {

struct DecodeContext final {
    judgement::CompileCapabilityContext capabilities;
    judgement::ContentProfileLimits contentLimits;
    judgement::PatternCompileBudget patternBudget;
    judgement::PreparedIdentityDeclarations identities;
    std::string coordinatorPolicyToken;
    std::uint32_t candidateRevision{2};
};

struct RequirementOwner final {
    judgement::RequirementIdentity requirement;
    chart::CanonicalEntityIdentity entity;
};

struct MeasureDefinition final {
    std::optional<std::string> id;
    judgement::MeasureSpecDeclaration declaration;
    friend auto operator==(const MeasureDefinition&, const MeasureDefinition&) -> bool = default;
};

struct CapsuleProfiles final {
    std::string normalizationProfileToken;
    std::string coordinatorPolicyToken;
};

// A complete chart and explicit ownership are mandatory. The adapter never invents entities.
// Additional definitions preserve unused source declarations.
struct EncodeRequest final {
    const chart::CanonicalSemanticChart& chart;
    const judgement::PreparedGameplay& gameplay;
    std::span<const RequirementOwner> owners;
    CapsuleProfiles profiles;
    std::span<const judgement::PatternDeclaration> additionalPatterns{};
    std::span<const MeasureDefinition> additionalMeasures{};
    std::uint32_t candidateRevision{2};
};

struct PreparedCapsule final {
    chart::CanonicalSemanticChart chart;
    judgement::PreparedGameplay gameplay;
    std::vector<RequirementOwner> owners;
    CapsuleProfiles profiles;
    std::vector<judgement::PatternDeclaration> patterns;
    std::vector<MeasureDefinition> measures;
    std::uint32_t candidateRevision{2};
};

// Explicit candidate-only bounds. No Graph production defaults are selected.
struct GraphWriterLimits final {
    std::size_t maxBytes;
    std::size_t maxStringBytes;
    std::size_t maxRowAtoms;
    bool testOnly;
};

struct GraphReaderLimits final {
    std::size_t maxBytes, maxDepth, maxStringBytes, maxValues, maxContainerElements;
    std::size_t maxRowAtoms, maxRowDecodedStringBytes;
    bool testOnly;
};

[[nodiscard]] auto decodeGraph(std::string_view text, const DecodeContext& context,
                               GraphReaderLimits graphLimits, chart::PackedChartLimits limits = {})
    -> core::Result<PreparedCapsule>;

[[nodiscard]] auto encodeGraph(const EncodeRequest& request, GraphWriterLimits graphLimits,
                               chart::PackedChartLimits limits = {}) -> core::Result<std::string>;

[[nodiscard]] auto encode(const EncodeRequest& request, chart::PackedChartLimits limits = {})
    -> core::Result<std::vector<std::byte>>;

[[nodiscard]] auto decode(std::span<const std::byte> bytes, const DecodeContext& context,
                          chart::PackedChartLimits limits = {}) -> core::Result<PreparedCapsule>;

[[nodiscard]] auto read(const std::filesystem::path& source, const DecodeContext& context,
                        chart::PackedChartLimits limits = {}) -> core::Result<PreparedCapsule>;

[[nodiscard]] auto writeAtomic(const EncodeRequest& request, const std::filesystem::path& target,
                               chart::PackedChartLimits limits = {}) -> core::Result<void>;

[[nodiscard]] auto decodeInto(std::optional<PreparedCapsule>& active,
                              std::span<const std::byte> bytes, const DecodeContext& context,
                              chart::PackedChartLimits limits = {}) -> core::Result<void>;

[[nodiscard]] auto semanticPreimage(const EncodeRequest& request,
                                    chart::PackedChartLimits limits = {})
    -> core::Result<std::vector<std::byte>>;

} // namespace cuexis::gameplay_packed

#pragma once

// Internal Stage 6 candidate bridge. This header is a build-time implementation detail and is
// intentionally not part of the installed public Playback SDK.

#include <cuexis/chart/canonical_semantic_chart.hpp>
#include <cuexis/chart/packed_chart_tables.hpp>
#include <cuexis/core/diagnostic.hpp>
#include <cuexis/cxc/cxc_package.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::cxc {

struct CandidateChartEntry final {
    std::string path;
    std::string kind;
    std::string encoding;
    bool playback{};
    std::optional<std::string> sourcePath;
    std::optional<std::string> sourceSemanticIdentity;
    std::optional<std::string> compiledSemanticIdentity;
    std::optional<std::string> artifactIdentity;
    std::optional<std::string> compilerProfile;
    std::optional<std::uint64_t> expandedEntityCount;
    std::optional<std::uint64_t> expandedRequirementCount;
};

struct CandidateChartEntryResult final {
    std::optional<CandidateChartEntry> entry;
    core::Diagnostics diagnostics;

    [[nodiscard]] auto hasValue() const noexcept -> bool {
        return entry.has_value() && !diagnostics.hasErrors();
    }
};

struct CandidateChart final {
    CandidateChartEntry entry;
    std::vector<std::byte> bytes;
    chart::CanonicalSemanticChart semantic;
    chart::packed::PackedSemanticIdentity semanticIdentity{};
    std::string artifactIdentity;
};

struct CandidateChartResult final {
    std::optional<CandidateChart> candidate;
    core::Diagnostics diagnostics;

    [[nodiscard]] auto hasValue() const noexcept -> bool {
        return candidate.has_value() && !diagnostics.hasErrors();
    }
};

// Parses and selects exactly one explicit candidate entry from a canonical extension object.
// The input is metadata only; no archive or Packed bytes are read here.
[[nodiscard]] auto parseCandidateChartEntryExtension(std::string_view extensionsJson,
                                                     std::string_view requestedPath)
    -> CandidateChartEntryResult;

// Validates one already-selected entry. The semantic result owns a copy of the exact entry bytes
// and contains the single decoded typed chart used by Playback prepare.
[[nodiscard]] auto validateCandidateChartBytes(const CandidateChartEntry& entry,
                                               std::span<const std::byte> bytes)
    -> CandidateChartResult;

// Package bridge used by CXC file/memory Playback factories. The package itself has already
// validated manifest closure, byte counts and archive SHA-256 values.
[[nodiscard]] auto validateCandidateChartEntry(const CxcPackage& package,
                                               std::string_view requestedPath)
    -> CandidateChartResult;

// Package-wide candidate extension check shared by developer tools. An absent extension is
// success. Playback does not call this during prepare; explicit entry selection owns that decode.
[[nodiscard]] auto validateCandidateChartExtension(const CxcPackage& package) -> core::Diagnostics;

} // namespace cuexis::cxc

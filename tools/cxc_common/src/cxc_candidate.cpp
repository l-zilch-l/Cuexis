#include <cuexis/tools/cxc_candidate.hpp>

#include <cuexis/chart/packed_chart_tables.hpp>
#include <cuexis/json/parse.hpp>
#include <cuexis/json/reader.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ranges>
#include <set>
#include <string>
#include <string_view>

namespace cuexis::tools {
namespace {

void addError(core::Diagnostics& diagnostics, std::string code, std::string message,
              std::string path) {
    static_cast<void>(diagnostics.add(core::Diagnostic{
        core::DiagnosticSeverity::Error, std::move(code), std::move(message), std::move(path)}));
}

[[nodiscard]] auto isSha256(std::string_view value) noexcept -> bool {
    return value.size() == 64 && std::all_of(value.begin(), value.end(), [](const char character) {
               return (character >= '0' && character <= '9') ||
                      (character >= 'a' && character <= 'f');
           });
}

[[nodiscard]] auto findEntry(const cxc::CxcPackage& package, std::string_view path)
    -> const cxc::CxcArchiveEntry* {
    const auto entries = package.entries();
    const auto found = std::ranges::find(entries, path, &cxc::CxcArchiveEntry::path);
    return found == entries.end() ? nullptr : &*found;
}

[[nodiscard]] auto requiredString(const json::Reader& reader, std::string_view field)
    -> std::optional<std::string> {
    const auto child = reader.requiredField(field);
    if (!child) {
        return std::nullopt;
    }
    const auto value = child->readString();
    return value ? std::optional{std::string{*value}} : std::nullopt;
}

} // namespace

auto validateCandidateChartExtension(const cxc::CxcPackage& package) -> core::Diagnostics {
    core::Diagnostics diagnostics;
    const auto& manifest = package.manifest();
    if (manifest.canonicalExtensionsJson.empty() || manifest.canonicalExtensionsJson == "{}") {
        return diagnostics;
    }

    const auto parsed = json::parse(manifest.canonicalExtensionsJson,
                                    json::ParseLimits{1024U * 1024U, 32U, 1024U * 1024U});
    if (!parsed) {
        addError(diagnostics, "cxc.candidate.extension_invalid",
                 "CXC candidate extension JSON could not be parsed", "$/extensions");
        return diagnostics;
    }
    json::Reader root{*parsed, diagnostics, "$/extensions"};
    if (root.readObject() == nullptr) {
        return diagnostics;
    }
    const auto candidateReader = root.optionalField("cuexis.chart-entry.v1");
    if (!candidateReader) {
        return diagnostics;
    }
    if (candidateReader->readObject() == nullptr) {
        return diagnostics;
    }
    candidateReader->rejectUnknownFields(std::array<std::string_view, 1>{"entries"});
    const auto entriesReader = candidateReader->requiredField("entries");
    if (!entriesReader) {
        return diagnostics;
    }
    const auto* entries = entriesReader->readArray();
    if (entries == nullptr) {
        return diagnostics;
    }
    if (entries->empty()) {
        addError(diagnostics, "cxc.candidate.entry_missing",
                 "Chart candidate extension must declare at least one entry",
                 std::string{entriesReader->fieldPath()});
        return diagnostics;
    }

    bool hasPlayback = false;
    std::set<std::string, std::less<>> declaredPaths;
    for (std::size_t index = 0; index < entries->size(); ++index) {
        const auto item = entriesReader->element(index);
        if (!item || item->readObject() == nullptr) {
            continue;
        }
        constexpr std::array fields{
            std::string_view{"path"},
            std::string_view{"kind"},
            std::string_view{"encoding"},
            std::string_view{"playback"},
            std::string_view{"sourcePath"},
            std::string_view{"sourceSemanticIdentity"},
            std::string_view{"compiledSemanticIdentity"},
            std::string_view{"artifactIdentity"},
            std::string_view{"compilerProfile"},
            std::string_view{"expandedEntityCount"},
            std::string_view{"expandedRequirementCount"},
        };
        item->rejectUnknownFields(fields);

        const auto path = requiredString(*item, "path");
        const auto kind = requiredString(*item, "kind");
        const auto encoding = requiredString(*item, "encoding");
        const auto playbackReader = item->requiredField("playback");
        const auto playback = playbackReader ? playbackReader->readBoolean() : std::nullopt;
        if (!path || !kind || !encoding || !playback) {
            continue;
        }
        const auto field = std::string{item->fieldPath()};
        if (!declaredPaths.emplace(*path).second) {
            addError(diagnostics, "cxc.candidate.entry_duplicate",
                     "Candidate extension declares an entry path more than once", field + "/path");
        }
        if (*kind != "chart" || *encoding != "packed-chart") {
            addError(diagnostics, "cxc.candidate.entry_unsupported",
                     "Chart candidate entries must use kind=chart and encoding=packed-chart",
                     field);
            continue;
        }
        const auto* archiveEntry = findEntry(package, *path);
        if (archiveEntry == nullptr) {
            addError(diagnostics, "cxc.candidate.entry_missing",
                     "Candidate entry path is not present in the archive", field + "/path");
            continue;
        }
        const auto bytes = package.entryBytes(*path);
        if (!bytes) {
            addError(diagnostics, "cxc.candidate.entry_missing",
                     "Candidate entry bytes are unavailable", field + "/path");
            continue;
        }
        if (bytes->size() > 16U * 1024U * 1024U) {
            addError(diagnostics, "cxc.budget.exceeded",
                     "Packed Chart candidate exceeds the 16 MiB entry limit", field + "/path");
        }
        const auto artifactIdentity = requiredString(*item, "artifactIdentity");
        if (!artifactIdentity || !isSha256(*artifactIdentity)) {
            addError(diagnostics, "cxc.candidate.identity_invalid",
                     "artifactIdentity must be lowercase SHA-256", field + "/artifactIdentity");
        } else if (*artifactIdentity != archiveEntry->sha256) {
            addError(diagnostics, "cxc.candidate.artifact_identity_mismatch",
                     "artifactIdentity does not match exact archive entry bytes",
                     field + "/artifactIdentity");
        }
        const auto compiledIdentity = requiredString(*item, "compiledSemanticIdentity");
        if (!compiledIdentity || !isSha256(*compiledIdentity)) {
            addError(diagnostics, "cxc.candidate.identity_invalid",
                     "compiledSemanticIdentity must be lowercase SHA-256",
                     field + "/compiledSemanticIdentity");
        }
        if (const auto sourcePath = item->optionalField("sourcePath")) {
            const auto source = sourcePath->readString();
            if (!source) {
                continue;
            }
            if (findEntry(package, *source) == nullptr) {
                addError(diagnostics, "cxc.candidate.source_missing",
                         "Candidate sourcePath is not present in the archive",
                         field + "/sourcePath");
            }
            if (const auto sourceIdentity = item->optionalField("sourceSemanticIdentity")) {
                const auto identity = sourceIdentity->readString();
                if (!identity || !isSha256(*identity)) {
                    addError(diagnostics, "cxc.candidate.identity_invalid",
                             "sourceSemanticIdentity must be lowercase SHA-256",
                             field + "/sourceSemanticIdentity");
                }
            }
        }
        const auto profile = requiredString(*item, "compilerProfile");
        if (!profile || *profile != "candidate.static-tap-lanes4-v1") {
            addError(diagnostics, "cxc.candidate.profile_unsupported",
                     "Candidate compilerProfile is not registered for Foundation revision 1",
                     field + "/compilerProfile");
        }
        const auto entityReader = item->requiredField("expandedEntityCount");
        const auto requirementReader = item->requiredField("expandedRequirementCount");
        std::optional<std::uint64_t> entityCount;
        if (entityReader) {
            entityCount = entityReader->readUInt64();
        }
        std::optional<std::uint64_t> requirementCount;
        if (requirementReader) {
            requirementCount = requirementReader->readUInt64();
        }
        if (!entityCount || *entityCount == 0 || *entityCount > 40000U) {
            addError(diagnostics, "cxc.candidate.count_invalid",
                     "expandedEntityCount must be in the Foundation range 1..40000",
                     field + "/expandedEntityCount");
        }
        if (!requirementCount || *requirementCount > 40000U) {
            addError(diagnostics, "cxc.candidate.count_invalid",
                     "expandedRequirementCount must be in the Foundation range 0..40000",
                     field + "/expandedRequirementCount");
        }

        if (*playback) {
            hasPlayback = true;
            const auto inspection = chart::packed::inspect(
                *bytes, chart::PackedChartLimits{.maxPackedFileBytes = 16U * 1024U * 1024U});
            if (!inspection) {
                addError(diagnostics, "cxc.candidate.packed_invalid",
                         "Playback candidate bytes are not a valid Foundation Packed Chart",
                         field + "/path");
            } else {
                const auto decoded = chart::packed::decode(
                    *bytes, chart::PackedChartLimits{.maxPackedFileBytes = 16U * 1024U * 1024U});
                if (!decoded) {
                    addError(diagnostics, "cxc.candidate.packed_invalid",
                             "Playback candidate bytes fail Foundation semantic validation",
                             field + "/path");
                } else if (const auto identity = chart::packed::semanticIdentity(*decoded)) {
                    // The declared compiled semantic identity must equal the identity recomputed
                    // from the decoded Packed chart, not merely look like a SHA-256.
                    if (compiledIdentity &&
                        *compiledIdentity != chart::packed::semanticIdentityHex(*identity)) {
                        addError(diagnostics, "cxc.candidate.compiled_identity_mismatch",
                                 "compiledSemanticIdentity does not match the decoded Packed "
                                 "semantic identity",
                                 field + "/compiledSemanticIdentity");
                    }
                } else {
                    addError(diagnostics, "cxc.candidate.packed_invalid",
                             "Playback candidate bytes do not yield a canonical semantic identity",
                             field + "/path");
                }
                if (entityCount && inspection->entityCount != *entityCount) {
                    addError(diagnostics, "cxc.candidate.count_mismatch",
                             "Packed entity count does not match candidate metadata",
                             field + "/expandedEntityCount");
                }
                if (requirementCount && inspection->requirementCount != *requirementCount) {
                    addError(diagnostics, "cxc.candidate.count_mismatch",
                             "Packed requirement count does not match candidate metadata",
                             field + "/expandedRequirementCount");
                }
            }
        }
    }
    if (!hasPlayback) {
        addError(diagnostics, "cxc.candidate.playback_missing",
                 "Chart candidate extension must declare a playback Packed entry",
                 "$/extensions/cuexis.chart-entry.v1/entries");
    }
    diagnostics.sortDeterministically();
    return diagnostics;
}

} // namespace cuexis::tools

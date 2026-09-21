#include <cuexis/cxc/cxc_candidate.hpp>

#include <cuexis/chart/packed_chart_tables.hpp>
#include <cuexis/json/parse.hpp>
#include <cuexis/json/reader.hpp>

#include "cxc_hash_internal.hpp"
#include "cxc_path_internal.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <ranges>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cuexis::cxc {
namespace {

constexpr std::string_view candidateExtension{"cuexis.chart-entry.v1"};
constexpr std::string_view candidateProfile{"candidate.static-tap-lanes4-v1"};
constexpr std::size_t maxExtensionEntries = 256U;
constexpr std::size_t maxPackedBytes = 16U * 1024U * 1024U;
constexpr std::size_t maxCandidateCount = 40000U;

void addError(core::Diagnostics& diagnostics, std::string code, std::string message,
              std::string path) {
    static_cast<void>(diagnostics.add(core::Diagnostic{
        core::DiagnosticSeverity::Error, std::move(code), std::move(message), std::move(path)}));
}

[[nodiscard]] auto isSha256(std::string_view value) noexcept -> bool {
    return value.size() == 64U && std::ranges::all_of(value, [](const char character) {
               return (character >= '0' && character <= '9') ||
                      (character >= 'a' && character <= 'f');
           });
}

[[nodiscard]] auto readString(const json::Reader& reader, std::string_view field)
    -> std::optional<std::string> {
    const auto child = reader.optionalField(field);
    if (!child) {
        return std::nullopt;
    }
    const auto value = child->readString();
    return value ? std::optional<std::string>{std::string{*value}} : std::nullopt;
}

[[nodiscard]] auto readRequiredString(const json::Reader& reader, std::string_view field,
                                      core::Diagnostics&) -> std::optional<std::string> {
    const auto child = reader.requiredField(field);
    if (!child) {
        return std::nullopt;
    }
    const auto value = child->readString();
    if (!value) {
        return std::nullopt;
    }
    return std::string{*value};
}

void appendExtensionDiagnostics(core::Diagnostics& destination, const core::Diagnostics& source) {
    for (const auto& item : source.items()) {
        auto diagnostic =
            core::Diagnostic{core::DiagnosticSeverity::Error, "cxc.chart_entry.extension_invalid",
                             "Chart entry extension is invalid", std::string{item.fieldPath()}};
        for (const auto& context : item.context()) {
            diagnostic.withContext(context.key, context.value);
        }
        static_cast<void>(destination.add(std::move(diagnostic)));
    }
}

[[nodiscard]] auto hasPathConflict(const std::vector<std::string>& paths, std::string_view folded)
    -> bool {
    for (const auto& previous : paths) {
        if (folded == previous || folded.starts_with(std::string{previous} + "/") ||
            previous.starts_with(std::string{folded} + "/")) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] auto packedErrorCode(std::string_view code) -> std::string {
    if (code.starts_with("packed.budget.")) {
        return "cxc.candidate.budget_exceeded";
    }
    if (code == "packed.header.unsupported_revision" || code == "packed.header.invalid") {
        return "cxc.candidate.revision_unsupported";
    }
    if (code.starts_with("packed.profile.")) {
        return "cxc.candidate.profile_unsupported";
    }
    return "cxc.candidate.packed_invalid";
}

void appendPackedError(core::Diagnostics& diagnostics, const core::Error& error, std::string path) {
    addError(diagnostics, packedErrorCode(error.code()), "Packed candidate validation failed",
             std::move(path));
}

[[nodiscard]] auto countRequirements(const chart::CanonicalSemanticChart& chart) -> std::size_t {
    std::size_t result = 0;
    for (const auto& entity : chart.entities) {
        result += entity.requirements.size();
    }
    return result;
}

[[nodiscard]] auto findArchiveEntry(const CxcPackage& package, std::string_view path)
    -> const CxcArchiveEntry* {
    const auto entries = package.entries();
    const auto found = std::ranges::find(entries, path, &CxcArchiveEntry::path);
    return found == entries.end() ? nullptr : &*found;
}

} // namespace

auto parseCandidateChartEntryExtension(std::string_view extensionsJson,
                                       std::string_view requestedPath)
    -> CandidateChartEntryResult {
    CandidateChartEntryResult result;
    if (requestedPath.empty() || !detail::isPortablePath(requestedPath)) {
        addError(result.diagnostics, "cxc.chart_entry.path_invalid",
                 "Requested chart entry path is not portable", "$/requestedPath");
        return result;
    }
    if (extensionsJson.empty() || extensionsJson == "{}") {
        addError(result.diagnostics, "cxc.chart_entry.extension_missing",
                 "Registered chart entry extension is missing", "$/extensions");
        return result;
    }

    const auto parsed =
        json::parse(extensionsJson, json::ParseLimits{1024U * 1024U, 64U, 1024U * 1024U});
    if (!parsed) {
        addError(result.diagnostics, "cxc.chart_entry.extension_invalid",
                 "Chart entry extension JSON could not be parsed", "$/extensions");
        return result;
    }

    core::Diagnostics readerDiagnostics;
    json::Reader root{*parsed, readerDiagnostics, "$/extensions"};
    if (root.readObject() == nullptr) {
        appendExtensionDiagnostics(result.diagnostics, readerDiagnostics);
        return result;
    }
    const auto extension = root.optionalField(candidateExtension);
    if (!extension) {
        addError(result.diagnostics, "cxc.chart_entry.extension_missing",
                 "Registered chart entry extension is missing", "$/extensions");
        return result;
    }
    if (extension->readObject() == nullptr) {
        appendExtensionDiagnostics(result.diagnostics, readerDiagnostics);
        return result;
    }
    extension->rejectUnknownFields(std::array<std::string_view, 1>{"entries"});
    const auto entriesReader = extension->requiredField("entries");
    if (!entriesReader) {
        appendExtensionDiagnostics(result.diagnostics, readerDiagnostics);
        return result;
    }
    const auto* entries = entriesReader->readArray();
    if (entries == nullptr) {
        appendExtensionDiagnostics(result.diagnostics, readerDiagnostics);
        return result;
    }
    if (entries->empty() || entries->size() > maxExtensionEntries) {
        addError(result.diagnostics, "cxc.chart_entry.extension_invalid",
                 "Chart entry extension entry count is outside the supported range",
                 std::string{entriesReader->fieldPath()});
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

    std::set<std::string, std::less<>> exactPaths;
    std::set<std::string, std::less<>> foldedPaths;
    std::vector<std::string> validFoldedPaths;
    std::optional<std::string> previousPath;
    std::optional<CandidateChartEntry> selected;
    bool selectedPathWasDeclared = false;

    for (std::size_t index = 0; index < entries->size(); ++index) {
        const auto item = entriesReader->element(index);
        if (!item || item->readObject() == nullptr) {
            continue;
        }
        item->rejectUnknownFields(fields);
        const auto fieldPath = std::string{item->fieldPath()};
        const auto path = readRequiredString(*item, "path", readerDiagnostics);
        const auto kind = readRequiredString(*item, "kind", readerDiagnostics);
        const auto encoding = readRequiredString(*item, "encoding", readerDiagnostics);
        const auto playbackReader = item->requiredField("playback");
        const auto playback = playbackReader ? playbackReader->readBoolean() : std::nullopt;
        if (!path || !kind || !encoding || !playback) {
            continue;
        }

        if (!detail::isPortablePath(*path)) {
            addError(result.diagnostics, "cxc.chart_entry.path_invalid",
                     "Chart entry path is not portable", fieldPath + "/path");
        } else {
            const auto folded = detail::foldAscii(*path);
            const bool exactDuplicate = !exactPaths.emplace(*path).second;
            const bool foldedDuplicate = !foldedPaths.emplace(folded).second;
            if (exactDuplicate) {
                addError(result.diagnostics, "cxc.chart_entry.duplicate_path",
                         "Chart entry path is duplicated", fieldPath + "/path");
            } else if (foldedDuplicate) {
                addError(result.diagnostics, "cxc.chart_entry.casefold_conflict",
                         "Chart entry path conflicts by ASCII case folding", fieldPath + "/path");
            } else if (hasPathConflict(validFoldedPaths, folded)) {
                addError(result.diagnostics, "cxc.chart_entry.duplicate_path",
                         "Chart entry path conflicts with a file or descendant path",
                         fieldPath + "/path");
            }
            validFoldedPaths.push_back(folded);
            if (previousPath && *path <= *previousPath) {
                addError(result.diagnostics, "cxc.chart_entry.order_invalid",
                         "Chart entry paths must be sorted by portable path bytes",
                         fieldPath + "/path");
            }
            previousPath = *path;
        }

        if (*kind != "chart" || (*encoding != "source-json" && *encoding != "source-cxt" &&
                                 *encoding != "packed-chart")) {
            addError(result.diagnostics, "cxc.chart_entry.entry_invalid",
                     "Chart entry kind or encoding is unsupported", fieldPath);
        }

        const auto sourcePath = readString(*item, "sourcePath");
        if (sourcePath && !detail::isPortablePath(*sourcePath)) {
            addError(result.diagnostics, "cxc.chart_entry.path_invalid",
                     "Chart entry sourcePath is not portable", fieldPath + "/sourcePath");
        }
        const auto sourceIdentity = readString(*item, "sourceSemanticIdentity");
        if (sourceIdentity && !isSha256(*sourceIdentity)) {
            addError(result.diagnostics, "cxc.chart_entry.identity_invalid",
                     "sourceSemanticIdentity must be lowercase SHA-256",
                     fieldPath + "/sourceSemanticIdentity");
        }
        const auto compiledIdentity = readString(*item, "compiledSemanticIdentity");
        if (compiledIdentity && !isSha256(*compiledIdentity)) {
            addError(result.diagnostics, "cxc.chart_entry.identity_invalid",
                     "compiledSemanticIdentity must be lowercase SHA-256",
                     fieldPath + "/compiledSemanticIdentity");
        }
        const auto artifactIdentity = readString(*item, "artifactIdentity");
        if (artifactIdentity && !isSha256(*artifactIdentity)) {
            addError(result.diagnostics, "cxc.chart_entry.identity_invalid",
                     "artifactIdentity must be lowercase SHA-256", fieldPath + "/artifactIdentity");
        }
        const auto compilerProfile = readString(*item, "compilerProfile");
        if (*playback && (!compiledIdentity || !artifactIdentity || !compilerProfile)) {
            addError(result.diagnostics, "cxc.chart_entry.extension_invalid",
                     "Playback chart entries require candidate identity and profile metadata",
                     fieldPath);
        }
        const auto entityReader = item->optionalField("expandedEntityCount");
        const auto requirementReader = item->optionalField("expandedRequirementCount");
        const auto entityCount = entityReader ? entityReader->readUInt64() : std::nullopt;
        const auto requirementCount =
            requirementReader ? requirementReader->readUInt64() : std::nullopt;
        if (*playback && (!entityCount || !requirementCount || *entityCount > maxCandidateCount ||
                          *requirementCount > maxCandidateCount)) {
            addError(result.diagnostics, "cxc.chart_entry.extension_invalid",
                     "Playback chart entry counts are outside the candidate range", fieldPath);
        }

        if (*path == requestedPath) {
            selectedPathWasDeclared = true;
            if (*playback) {
                if (!selected) {
                    selected = CandidateChartEntry{
                        *path,           *kind,          *encoding,        *playback,
                        sourcePath,      sourceIdentity, compiledIdentity, artifactIdentity,
                        compilerProfile, entityCount,    requirementCount};
                }
            } else {
                addError(result.diagnostics, "cxc.chart_entry.playback_not_declared",
                         "Requested chart entry is not declared for playback", fieldPath);
            }
        }
    }

    appendExtensionDiagnostics(result.diagnostics, readerDiagnostics);
    if (!selectedPathWasDeclared) {
        addError(result.diagnostics, "cxc.chart_entry.entry_missing",
                 "Requested chart entry path is not declared", "$/extensions");
    }
    if (!selected && selectedPathWasDeclared) {
        addError(result.diagnostics, "cxc.chart_entry.playback_not_declared",
                 "Requested chart entry has no playback=true record", "$/extensions");
    }
    result.diagnostics.sortDeterministically();
    if (!result.diagnostics.hasErrors() && selected) {
        result.entry = std::move(selected);
    }
    return result;
}

auto validateCandidateChartBytes(const CandidateChartEntry& entry, std::span<const std::byte> bytes)
    -> CandidateChartResult {
    CandidateChartResult result;
    const auto path = std::string{"$/extensions/cuexis.chart-entry.v1/entries"};
    if (!entry.playback || entry.kind != "chart" || entry.encoding != "packed-chart") {
        addError(result.diagnostics, "cxc.chart_entry.entry_invalid",
                 "Selected entry is not a playback Packed Chart candidate", path);
        return result;
    }
    if (!entry.compilerProfile || *entry.compilerProfile != candidateProfile) {
        addError(result.diagnostics, "cxc.candidate.profile_unsupported",
                 "Candidate compiler profile is not registered", path + "/compilerProfile");
    }
    if (!entry.artifactIdentity || !isSha256(*entry.artifactIdentity)) {
        addError(result.diagnostics, "cxc.chart_entry.identity_invalid",
                 "artifactIdentity must be lowercase SHA-256", path + "/artifactIdentity");
    } else if (detail::sha256Hex(bytes) != *entry.artifactIdentity) {
        addError(result.diagnostics, "cxc.candidate.artifact_identity_mismatch",
                 "artifactIdentity does not match exact entry bytes", path + "/artifactIdentity");
    }
    if (!entry.compiledSemanticIdentity || !isSha256(*entry.compiledSemanticIdentity)) {
        addError(result.diagnostics, "cxc.chart_entry.identity_invalid",
                 "compiledSemanticIdentity must be lowercase SHA-256",
                 path + "/compiledSemanticIdentity");
    }
    if (!entry.expandedEntityCount || !entry.expandedRequirementCount ||
        *entry.expandedEntityCount > maxCandidateCount ||
        *entry.expandedRequirementCount > maxCandidateCount) {
        addError(result.diagnostics, "cxc.candidate.budget_exceeded",
                 "Candidate metadata counts are outside the supported range", path);
    }
    if (bytes.size() > maxPackedBytes) {
        addError(result.diagnostics, "cxc.candidate.budget_exceeded",
                 "Packed candidate exceeds the 16 MiB file limit", path + "/path");
    }
    if (result.diagnostics.hasErrors()) {
        return result;
    }

    // This is deliberately the only Packed decoder call in the bridge. It performs the fixed
    // header, flags/revision, CRC, profile, semantic identity and typed model validation gates.
    const auto decoded = chart::packed::decode(bytes);
    if (!decoded) {
        appendPackedError(result.diagnostics, decoded.error(), path + "/path");
        result.diagnostics.sortDeterministically();
        return result;
    }
    const auto identity = chart::packed::semanticIdentity(*decoded);
    if (!identity) {
        addError(result.diagnostics, "cxc.candidate.packed_invalid",
                 "Decoded candidate did not yield a semantic identity", path + "/path");
        return result;
    }
    const auto identityHex = chart::packed::semanticIdentityHex(*identity);
    if (identityHex != *entry.compiledSemanticIdentity) {
        addError(result.diagnostics, "cxc.candidate.compiled_identity_mismatch",
                 "compiledSemanticIdentity does not match the decoded semantic identity",
                 path + "/compiledSemanticIdentity");
    }
    const auto requirementCount = countRequirements(*decoded);
    if (decoded->entities.size() != *entry.expandedEntityCount) {
        addError(result.diagnostics, "cxc.candidate.count_mismatch",
                 "Packed entity count does not match candidate metadata",
                 path + "/expandedEntityCount");
    }
    if (requirementCount != *entry.expandedRequirementCount) {
        addError(result.diagnostics, "cxc.candidate.count_mismatch",
                 "Packed requirement count does not match candidate metadata",
                 path + "/expandedRequirementCount");
    }
    result.diagnostics.sortDeterministically();
    if (result.diagnostics.hasErrors()) {
        return result;
    }

    CandidateChart candidate;
    candidate.entry = entry;
    candidate.bytes.assign(bytes.begin(), bytes.end());
    candidate.semantic = std::move(*decoded);
    candidate.semanticIdentity = *identity;
    candidate.artifactIdentity = detail::sha256Hex(bytes);
    result.candidate = std::move(candidate);
    return result;
}

auto validateCandidateChartEntry(const CxcPackage& package, std::string_view requestedPath)
    -> CandidateChartResult {
    CandidateChartResult result;
    const auto selected = parseCandidateChartEntryExtension(
        package.manifest().canonicalExtensionsJson, requestedPath);
    result.diagnostics.append(std::move(selected.diagnostics));
    if (!selected.entry || result.diagnostics.hasErrors()) {
        return result;
    }
    const auto bytes = package.entryBytes(selected.entry->path);
    if (!bytes) {
        addError(result.diagnostics, "cxc.chart_entry.entry_missing",
                 "Selected chart entry is not present in the CXC package", "$/archive");
        return result;
    }
    auto validated = validateCandidateChartBytes(*selected.entry, *bytes);
    result.diagnostics.append(std::move(validated.diagnostics));
    if (!result.diagnostics.hasErrors() && validated.candidate) {
        result.candidate = std::move(validated.candidate);
    }
    result.diagnostics.sortDeterministically();
    return result;
}

auto validateCandidateChartExtension(const CxcPackage& package) -> core::Diagnostics {
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
    const auto candidateReader = root.optionalField(candidateExtension);
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

        const auto path = readRequiredString(*item, "path", diagnostics);
        const auto kind = readRequiredString(*item, "kind", diagnostics);
        const auto encoding = readRequiredString(*item, "encoding", diagnostics);
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
        const auto* archiveEntry = findArchiveEntry(package, *path);
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
        if (bytes->size() > maxPackedBytes) {
            addError(diagnostics, "cxc.budget.exceeded",
                     "Packed Chart candidate exceeds the 16 MiB entry limit", field + "/path");
        }
        const auto artifactIdentity = readRequiredString(*item, "artifactIdentity", diagnostics);
        if (!artifactIdentity || !isSha256(*artifactIdentity)) {
            addError(diagnostics, "cxc.candidate.identity_invalid",
                     "artifactIdentity must be lowercase SHA-256", field + "/artifactIdentity");
        } else if (*artifactIdentity != archiveEntry->sha256) {
            addError(diagnostics, "cxc.candidate.artifact_identity_mismatch",
                     "artifactIdentity does not match exact archive entry bytes",
                     field + "/artifactIdentity");
        }
        const auto compiledIdentity =
            readRequiredString(*item, "compiledSemanticIdentity", diagnostics);
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
            if (findArchiveEntry(package, *source) == nullptr) {
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
        const auto profile = readRequiredString(*item, "compilerProfile", diagnostics);
        if (!profile || *profile != candidateProfile) {
            addError(diagnostics, "cxc.candidate.profile_unsupported",
                     "Candidate compilerProfile is not registered for Foundation revision 1",
                     field + "/compilerProfile");
        }
        const auto entityReader = item->requiredField("expandedEntityCount");
        const auto requirementReader = item->requiredField("expandedRequirementCount");
        const auto entityCount = entityReader ? entityReader->readUInt64() : std::nullopt;
        const auto requirementCount =
            requirementReader ? requirementReader->readUInt64() : std::nullopt;
        if (!entityCount || *entityCount == 0 || *entityCount > maxCandidateCount) {
            addError(diagnostics, "cxc.candidate.count_invalid",
                     "expandedEntityCount must be in the Foundation range 1..40000",
                     field + "/expandedEntityCount");
        }
        if (!requirementCount || *requirementCount > maxCandidateCount) {
            addError(diagnostics, "cxc.candidate.count_invalid",
                     "expandedRequirementCount must be in the Foundation range 0..40000",
                     field + "/expandedRequirementCount");
        }

        if (*playback) {
            hasPlayback = true;
            const auto decoded = chart::packed::decode(
                *bytes, chart::PackedChartLimits{.maxPackedFileBytes = maxPackedBytes});
            if (!decoded) {
                addError(diagnostics, "cxc.candidate.packed_invalid",
                         "Playback candidate bytes fail Foundation semantic validation",
                         field + "/path");
            } else if (const auto identity = chart::packed::semanticIdentity(*decoded)) {
                if (compiledIdentity &&
                    *compiledIdentity != chart::packed::semanticIdentityHex(*identity)) {
                    addError(diagnostics, "cxc.candidate.compiled_identity_mismatch",
                             "compiledSemanticIdentity does not match the decoded Packed semantic "
                             "identity",
                             field + "/compiledSemanticIdentity");
                }
                if (entityCount && decoded->entities.size() != *entityCount) {
                    addError(diagnostics, "cxc.candidate.count_mismatch",
                             "Packed entity count does not match candidate metadata",
                             field + "/expandedEntityCount");
                }
                if (requirementCount && countRequirements(*decoded) != *requirementCount) {
                    addError(diagnostics, "cxc.candidate.count_mismatch",
                             "Packed requirement count does not match candidate metadata",
                             field + "/expandedRequirementCount");
                }
            } else {
                addError(diagnostics, "cxc.candidate.packed_invalid",
                         "Playback candidate bytes do not yield a canonical semantic identity",
                         field + "/path");
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

} // namespace cuexis::cxc

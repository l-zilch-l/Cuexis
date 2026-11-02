#include <cuexis/chart/chart_runtime.hpp>
#include <cuexis/chart/uuid.hpp>
#include <cuexis/core/diagnostic.hpp>
#include <cuexis/core/error.hpp>
#include <cuexis/core/math.hpp>
#include <cuexis/json/parse.hpp>
#include <cuexis/json/reader.hpp>

#include "canonical_chart_loader_internal.hpp"
#include "canonical_chart_loader_units_internal.hpp"
#include "canonical_chart_template_internal.hpp"
#include "diagnostic_limit.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cuexis::chart {

auto detail::loadCanonicalValue(const json::Value& parsed, const ChartLimits& limits)
    -> ChartDocumentResult {
    auto diagnostics = detail::makeDiagnostics(limits);
    if (detail::rejectInvalidDiagnosticLimit(diagnostics, limits)) {
        return ChartDocumentResult{std::nullopt, std::move(diagnostics)};
    }
    json::Reader root{parsed, diagnostics};
    if (root.readObject() == nullptr) {
        diagnostics.sortDeterministically();
        return ChartDocumentResult{std::nullopt, std::move(diagnostics)};
    }
    const auto versionReader = root.requiredField("version");
    const auto formatVersion =
        versionReader ? readVersion(*versionReader, diagnostics) : std::nullopt;
    constexpr std::array knownFieldsV1{
        std::string_view{"format"},    std::string_view{"version"},
        std::string_view{"chartId"},   std::string_view{"metadata"},
        std::string_view{"timing"},    std::string_view{"camera"},
        std::string_view{"templates"}, std::string_view{"behaviors"},
        std::string_view{"objects"},   std::string_view{"requiredExtensions"},
        std::string_view{"extensions"}};
    constexpr std::array knownFieldsV2{
        std::string_view{"format"},     std::string_view{"version"},
        std::string_view{"chartId"},    std::string_view{"metadata"},
        std::string_view{"timing"},     std::string_view{"camera"},
        std::string_view{"templates"},  std::string_view{"behaviors"},
        std::string_view{"objects"},    std::string_view{"requiredExtensions"},
        std::string_view{"extensions"}, std::string_view{"audio"}};
    if (formatVersion && (*formatVersion == 2 || *formatVersion == 3)) {
        root.rejectUnknownFields(knownFieldsV2);
    } else {
        root.rejectUnknownFields(knownFieldsV1);
    }

    const auto formatReader = root.requiredField("format");
    const auto chartIdReader = root.requiredField("chartId");
    const auto metadataReader = root.requiredField("metadata");
    const auto timingReader = root.requiredField("timing");
    const auto cameraReader = root.optionalField("camera");
    const auto templatesReader = root.requiredField("templates");
    const auto behaviorsReader = root.requiredField("behaviors");
    const auto objectsReader = root.requiredField("objects");
    const auto requiredExtensionsReader = root.requiredField("requiredExtensions");
    const auto extensionsReader = root.requiredField("extensions");
    const auto audioReader = formatVersion && (*formatVersion == 2 || *formatVersion == 3)
                                 ? root.optionalField("audio")
                                 : std::nullopt;

    std::optional<ChartId> chartId;
    if (formatReader) {
        const auto format = formatReader->readString();
        if (format && *format != "cuexis.chart") {
            addError(diagnostics, "chart.format.invalid", "Expected canonical cuexis.chart format",
                     std::string{formatReader->fieldPath()});
        }
    }
    if (chartIdReader) {
        const auto id = readIdentifier(*chartIdReader, limits, diagnostics, "Chart ID");
        if (id && isUuidV7(*id)) {
            chartId = ChartId{*id};
        } else if (id) {
            addError(diagnostics, "chart.uuid.invalid_v7", "Chart ID must be a UUIDv7",
                     std::string{chartIdReader->fieldPath()});
        }
    }

    ChartMetadata metadata;
    if (metadataReader && metadataReader->readObject() != nullptr) {
        if (metadataReader->value().object()->size() > limits.maxMetadataMembers) {
            addError(diagnostics, "chart.limit.metadata",
                     "Metadata member count exceeds configured limit",
                     std::string{metadataReader->fieldPath()});
        }
        metadata.data =
            opaqueJson(metadataReader->value(), diagnostics, metadataReader->fieldPath());
    }
    const auto timing = timingReader && formatVersion
                            ? parseTiming(*timingReader, *formatVersion, limits, diagnostics)
                            : std::nullopt;
    const auto audio = audioReader ? readChartAudio(*audioReader, limits, diagnostics)
                                   : std::optional<ChartAudioData>{};
    const auto camera = cameraReader ? readCamera(*cameraReader, diagnostics) : CameraData{};
    const detail::TemplateParserCallbacks templateCallbacks{.readIdentifier = readIdentifier,
                                                            .readReference = readReference,
                                                            .readNullableName = readNullableName,
                                                            .opaqueJson = opaqueJson,
                                                            .parseComponents = parseComponents};
    auto [templates, expandedTemplates] =
        templatesReader
            ? detail::parseTemplates(*templatesReader, limits, diagnostics, templateCallbacks)
            : std::pair<std::vector<ChartTemplate>, std::map<std::string, json::Value>>{};
    auto behaviors =
        behaviorsReader && formatVersion
            ? (*formatVersion == 3 ? parseEventBehaviors(*behaviorsReader, limits, diagnostics)
                                   : parseBehaviors(*behaviorsReader, limits, diagnostics))
            : std::vector<ChartBehavior>{};
    auto objects = objectsReader
                       ? parseObjects(*objectsReader, expandedTemplates, limits, diagnostics)
                       : std::vector<ChartObject>{};

    if (requiredExtensionsReader) {
        if (const auto* required = requiredExtensionsReader->readArray()) {
            if (required->size() > limits.maxExtensions) {
                addError(diagnostics, "chart.limit.required_extensions",
                         "Required extension count exceeds configured limit",
                         std::string{requiredExtensionsReader->fieldPath()});
            }
            for (std::size_t index = 0; index < required->size(); ++index) {
                if (diagnostics.limitReached()) {
                    break;
                }
                const auto itemReader = requiredExtensionsReader->element(index);
                if (!itemReader || itemReader->readObject() == nullptr) {
                    continue;
                }
                constexpr std::array requiredExtensionFields{std::string_view{"id"},
                                                             std::string_view{"version"}};
                itemReader->rejectUnknownFields(requiredExtensionFields);
                const auto idReader = itemReader->requiredField("id");
                const auto extensionVersionReader = itemReader->requiredField("version");
                if (!idReader || !extensionVersionReader) {
                    continue;
                }
                const auto extensionId =
                    readIdentifier(*idReader, limits, diagnostics, "Required extension ID");
                const auto extensionVersion = extensionVersionReader->readInt64();
                bool valid = extensionId.has_value() && isExtensionId(*extensionId);
                if (extensionId && !valid) {
                    addError(diagnostics, "chart.extension.id_invalid",
                             "Required extension ID contains unsupported characters",
                             std::string{idReader->fieldPath()});
                }
                if (extensionVersion && *extensionVersion < 1) {
                    addError(diagnostics, "chart.extension.version_invalid",
                             "Required extension version must be positive",
                             std::string{extensionVersionReader->fieldPath()});
                    valid = false;
                } else if (!extensionVersion) {
                    valid = false;
                }
                if (!valid) {
                    continue;
                }
                addError(diagnostics, "chart.extension.required_unsupported",
                         "Required extension has no registered handler",
                         json::appendIndexPath(requiredExtensionsReader->fieldPath(), index));
            }
        }
    }

    OpaqueJson extensions;
    if (extensionsReader && extensionsReader->readObject() != nullptr) {
        if (extensionsReader->value().object()->size() > limits.maxExtensions) {
            addError(diagnostics, "chart.limit.extensions",
                     "Extension count exceeds configured limit",
                     std::string{extensionsReader->fieldPath()});
        }
        extensions =
            opaqueJson(extensionsReader->value(), diagnostics, extensionsReader->fieldPath());
        for (const auto& [extensionId, ignored] : *extensionsReader->value().object()) {
            if (diagnostics.limitReached()) {
                break;
            }
            static_cast<void>(ignored);
            addWarning(diagnostics, "chart.extension.optional_unknown",
                       "Unknown optional extension is preserved without runtime behavior",
                       json::appendFieldPath(extensionsReader->fieldPath(), extensionId));
        }
    }

    if (!chartId || !timing || !formatVersion || diagnostics.hasErrors()) {
        diagnostics.sortDeterministically();
        return ChartDocumentResult{std::nullopt, std::move(diagnostics)};
    }

    ChartDocument document{*chartId,
                           std::move(metadata),
                           *timing,
                           camera,
                           std::move(templates),
                           std::move(behaviors),
                           std::move(objects),
                           std::move(extensions),
                           *formatVersion,
                           audio};
    auto semantic = ChartCompiler::compile(document, limits);
    diagnostics.append(std::move(semantic.diagnostics));
    diagnostics.sortDeterministically();
    if (diagnostics.hasErrors()) {
        return ChartDocumentResult{std::nullopt, std::move(diagnostics)};
    }
    return ChartDocumentResult{std::move(document), std::move(diagnostics)};
}

auto CanonicalChartLoader::load(std::string_view jsonText, const ChartLimits& limits)
    -> ChartDocumentResult {
    auto diagnostics = detail::makeDiagnostics(limits);
    if (detail::rejectInvalidDiagnosticLimit(diagnostics, limits)) {
        return ChartDocumentResult{std::nullopt, std::move(diagnostics)};
    }
    auto parsed =
        json::parse(jsonText, json::ParseLimits{limits.maxInputBytes, limits.maxNestingDepth,
                                                limits.maxStringBytes});
    if (!parsed) {
        addError(diagnostics, parsed.error(), "$");
        diagnostics.sortDeterministically();
        return ChartDocumentResult{std::nullopt, std::move(diagnostics)};
    }

    return detail::loadCanonicalValue(*parsed, limits);
}

} // namespace cuexis::chart

#include <cuexis/chart/chart_v4_loader.hpp>

#include <cuexis/chart/canonical_chart_loader.hpp>
#include <cuexis/chart/detail/chart_dispatch_internal.hpp>
#include <cuexis/chart/uuid.hpp>
#include <cuexis/json/parse.hpp>
#include <cuexis/json/reader.hpp>
#include <cuexis_internal/prepare_trace.hpp>

#include "canonical_chart_loader_internal.hpp"
#include "chart_v4_animation_reader_internal.hpp"
#include "chart_v4_common_reader_internal.hpp"
#include "chart_v4_loader_helpers_internal.hpp"
#include "chart_v4_loader_internal.hpp"
#include "chart_writer_internal.hpp"
#include "diagnostic_limit.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cuexis::chart {
namespace {

void addParseError(core::Diagnostics& diagnostics, const core::Error& error) {
    auto diagnostic = core::Diagnostic{core::DiagnosticSeverity::Error, std::string{error.code()},
                                       std::string{error.message()}, "$"};
    for (const auto& context : error.context()) {
        diagnostic.withContext(context.key, context.value);
    }
    diagnostics.add(std::move(diagnostic));
}
} // namespace

auto ChartV4Loader::isV4(std::string_view jsonText, const ChartLimits& limits) -> bool {
    auto parsed =
        json::parse(jsonText, json::ParseLimits{limits.maxInputBytes, limits.maxNestingDepth,
                                                limits.maxStringBytes});
    if (!parsed) {
        return false;
    }
    const auto* version = parsed->find("version");
    if (version == nullptr) {
        return false;
    }
    if (const auto* signedValue = version->signedInteger()) {
        return *signedValue == 4;
    }
    if (const auto* unsignedValue = version->unsignedInteger()) {
        return *unsignedValue == 4;
    }
    return false;
}

auto detail::loadV4Value(const json::Value& parsed, const ChartLimits& limits)
    -> ChartV4SourceResult {
    auto diagnostics = detail::makeDiagnostics(limits);
    if (detail::rejectInvalidDiagnosticLimit(diagnostics, limits)) {
        return ChartV4SourceResult{std::nullopt, std::move(diagnostics)};
    }

    json::Reader root{parsed, diagnostics};
    constexpr std::array fields{std::string_view{"format"},
                                std::string_view{"version"},
                                std::string_view{"chartId"},
                                std::string_view{"metadata"},
                                std::string_view{"timing"},
                                std::string_view{"camera"},
                                std::string_view{"audio"},
                                std::string_view{"parameters"},
                                std::string_view{"templates"},
                                std::string_view{"behaviors"},
                                std::string_view{"animationTemplateImports"},
                                std::string_view{"animationClips"},
                                std::string_view{"objects"},
                                std::string_view{"requiredExtensions"},
                                std::string_view{"extensions"}};
    if (root.readObject() == nullptr) {
        diagnostics.sortDeterministically();
        return ChartV4SourceResult{std::nullopt, std::move(diagnostics)};
    }
    root.rejectUnknownFields(fields);
    const auto formatReader = root.requiredField("format");
    const auto versionReader = root.requiredField("version");
    const auto chartIdReader = root.requiredField("chartId");
    const auto parametersReader = root.requiredField("parameters");
    const auto importsReader = root.requiredField("animationTemplateImports");
    const auto clipsReader = root.requiredField("animationClips");
    const auto requiredExtensionsReader = root.requiredField("requiredExtensions");
    const auto extensionsReader = root.requiredField("extensions");
    static_cast<void>(root.requiredField("metadata"));
    static_cast<void>(root.requiredField("timing"));
    static_cast<void>(root.requiredField("templates"));
    static_cast<void>(root.requiredField("behaviors"));
    static_cast<void>(root.requiredField("objects"));

    const auto format = formatReader ? formatReader->readString() : std::nullopt;
    auto version = std::optional<std::int64_t>{};
    if (versionReader) {
        version = versionReader->readInt64();
    }
    const auto chartId = chartIdReader ? chartIdReader->readString() : std::nullopt;
    if (format && *format != "cuexis.chart") {
        detail::addV4Error(diagnostics, "chart.format.unsupported", "Chart format is unsupported",
                           std::string{formatReader->fieldPath()});
    }
    if (version && *version != 4) {
        detail::addV4Error(diagnostics, "chart.version.unsupported", "Chart version is unsupported",
                           std::string{versionReader->fieldPath()});
    }
    if (chartId && !isUuidV7(*chartId)) {
        detail::addV4Error(diagnostics, "chart.chart_id.invalid",
                           "Chart ID must be a lowercase UUIDv7",
                           std::string{chartIdReader->fieldPath()});
    }

    auto parameters = parametersReader
                          ? loader_internal::readParameters(*parametersReader, limits, diagnostics)
                          : std::vector<ChartParameterDeclaration>{};
    auto imports = importsReader ? loader_internal::readImports(*importsReader, limits, diagnostics)
                                 : std::vector<AnimationTemplateImport>{};
    std::vector<AnimationClip> clips;
    std::set<std::string, std::less<>> clipIds;
    if (clipsReader) {
        if (const auto* items = clipsReader->readArray()) {
            if (items->size() > limits.maxAnimationClips) {
                detail::addV4Error(diagnostics, "chart.animation.generated_limit",
                                   "Animation clip count exceeds the configured limit",
                                   std::string{clipsReader->fieldPath()});
            }
            for (std::size_t index = 0; index < items->size(); ++index) {
                const auto item = clipsReader->element(index);
                if (!item) {
                    continue;
                }
                auto clip = detail::readAnimationClip(*item, limits, diagnostics, true);
                if (!clip) {
                    continue;
                }
                if (!clipIds.emplace(clip->id).second) {
                    detail::addV4Error(diagnostics, "chart.animation.clip_invalid",
                                       "Animation clip ID is duplicated", clip->fieldPath + "/id");
                }
                clips.push_back(std::move(*clip));
            }
        }
    }
    std::ranges::sort(clips, {}, &AnimationClip::id);

    std::vector<ParameterUse> parameterUses;
    auto animators = loader_internal::readAnimators(root, limits, diagnostics, parameterUses);
    const auto projection =
        loader_internal::makeLegacyProjection(parsed, limits, diagnostics, parameterUses);
    loader_internal::validateParameterUses(parameters, parameterUses, diagnostics);
    loader_internal::validateAnimationReferences(imports, clips, animators, diagnostics);

    auto requiredExtensions =
        requiredExtensionsReader
            ? detail::readRequiredExtensions(*requiredExtensionsReader, limits, diagnostics)
            : std::vector<RequiredExtension>{};
    OpaqueJson extensions;
    if (extensionsReader) {
        if (auto serialized = json::serialize(extensionsReader->value())) {
            extensions.canonicalText = std::move(*serialized);
        } else {
            addParseError(diagnostics, serialized.error());
        }
    }
    OpaqueJson canonicalSource;
    if (auto serialized = json::serialize(parsed)) {
        canonicalSource.canonicalText = std::move(*serialized);
    } else {
        addParseError(diagnostics, serialized.error());
    }

    diagnostics.sortDeterministically();
    if (diagnostics.hasErrors() || !format || *format != "cuexis.chart" || !version ||
        *version != 4 || !chartId || !isUuidV7(*chartId) || !projection) {
        return ChartV4SourceResult{std::nullopt, std::move(diagnostics)};
    }
    return ChartV4SourceResult{
        ChartV4SourceDocument{
            ChartId{std::string{*chartId}}, *projection, std::move(canonicalSource),
            std::move(parameters), std::move(parameterUses), std::move(imports), std::move(clips),
            std::move(animators), std::move(requiredExtensions), std::move(extensions)},
        std::move(diagnostics)};
}

auto ChartV4Loader::load(std::string_view jsonText, const ChartLimits& limits)
    -> ChartV4SourceResult {
    auto diagnostics = detail::makeDiagnostics(limits);
    if (detail::rejectInvalidDiagnosticLimit(diagnostics, limits)) {
        return ChartV4SourceResult{std::nullopt, std::move(diagnostics)};
    }
    auto parsed =
        json::parse(jsonText, json::ParseLimits{limits.maxInputBytes, limits.maxNestingDepth,
                                                limits.maxStringBytes});
    if (!parsed) {
        addParseError(diagnostics, parsed.error());
        diagnostics.sortDeterministically();
        return ChartV4SourceResult{std::nullopt, std::move(diagnostics)};
    }
    return detail::loadV4Value(*parsed, limits);
}

auto detail::loadChartForPlayback(std::string_view jsonText, const ChartLimits& limits)
    -> ChartDispatchResult {
    auto diagnostics = detail::makeDiagnostics(limits);
    if (detail::rejectInvalidDiagnosticLimit(diagnostics, limits)) {
        return ChartDispatchResult{false, std::nullopt, std::nullopt,
                                   {},    std::nullopt, std::move(diagnostics)};
    }
    auto parsed = [&] {
        const internal::PrepareTraceScope trace{"json_dom_parse"};
        return json::parse(jsonText, json::ParseLimits{limits.maxInputBytes, limits.maxNestingDepth,
                                                       limits.maxStringBytes});
    }();
    if (!parsed) {
        addParseError(diagnostics, parsed.error());
        diagnostics.sortDeterministically();
        return ChartDispatchResult{false, std::nullopt, std::nullopt,
                                   {},    std::nullopt, std::move(diagnostics)};
    }

    bool isV4 = false;
    if (const auto* version = parsed->find("version"); version != nullptr) {
        if (const auto* signedValue = version->signedInteger()) {
            isV4 = *signedValue == 4;
        } else if (const auto* unsignedValue = version->unsignedInteger()) {
            isV4 = *unsignedValue == 4;
        }
    }
    if (isV4) {
        auto input = std::make_shared<ParsedChartInput>(ParsedChartInput{std::move(*parsed)});
        auto loaded = [&] {
            const internal::PrepareTraceScope trace{"chart_typed_read"};
            return detail::loadV4Value(input->value, limits);
        }();
        return ChartDispatchResult{true,
                                   std::nullopt,
                                   std::move(loaded.document),
                                   std::move(input),
                                   std::nullopt,
                                   std::move(loaded.diagnostics)};
    }

    auto loaded = [&] {
        const internal::PrepareTraceScope trace{"chart_typed_read"};
        return detail::loadCanonicalValue(*parsed, limits);
    }();
    std::optional<std::string> canonicalText;
    if (loaded.hasValue()) {
        // The typed reader only reads the tree, so this parse can also produce the canonical
        // identity bytes. Producing them here is what lets a legacy prepare parse once instead of
        // re-parsing the same document for its identity.
        {
            const internal::PrepareTraceScope trace{"chart_canonical_write"};
            if (auto text = detail::writeCanonicalValue(std::move(*parsed), limits)) {
                canonicalText = std::move(*text);
            }
        }
    }
    return ChartDispatchResult{false, std::move(loaded.document), std::nullopt,
                               {},    std::move(canonicalText),   std::move(loaded.diagnostics)};
}

auto detail::loadV4IfPresent(std::string_view jsonText, const ChartLimits& limits)
    -> std::optional<ChartV4SourceDocument> {
    // The version alone decides whether this text is a v4 source at all, and the callers that ask
    // are the ones that already hold the text for the real parse. Building the value tree only to
    // read one member would make them pay twice for the whole document, so the top-level version is
    // scanned first and a non-v4 document costs only the prefix in front of that member.
    const auto version = json::scanTopLevelInteger(
        jsonText, "version",
        json::ParseLimits{limits.maxInputBytes, limits.maxNestingDepth, limits.maxStringBytes});
    if (!version.has_value() || *version != 4) {
        return std::nullopt;
    }
    auto parsed =
        json::parse(jsonText, json::ParseLimits{limits.maxInputBytes, limits.maxNestingDepth,
                                                limits.maxStringBytes});
    if (!parsed) {
        return std::nullopt;
    }
    auto loaded = detail::loadV4Value(*parsed, limits);
    if (!loaded.hasValue()) {
        return std::nullopt;
    }
    return std::move(loaded.document);
}

} // namespace cuexis::chart

#include <cuexis/chart/animation_template_loader.hpp>

#include <cuexis/core/error.hpp>
#include <cuexis/json/parse.hpp>
#include <cuexis/json/reader.hpp>

#include "chart_v4_reader_internal.hpp"
#include "diagnostic_limit.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

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

void rejectUnknownTopLevelFields(const json::Reader& reader, core::Diagnostics& diagnostics) {
    constexpr std::array fields{std::string_view{"format"},
                                std::string_view{"version"},
                                std::string_view{"templateId"},
                                std::string_view{"metadata"},
                                std::string_view{"application"},
                                std::string_view{"clip"},
                                std::string_view{"requiredExtensions"},
                                std::string_view{"extensions"}};
    const auto* object = reader.value().object();
    if (object == nullptr) {
        return;
    }
    for (const auto& [name, value] : *object) {
        static_cast<void>(value);
        if (std::ranges::find(fields, name) == fields.end()) {
            detail::addV4Error(diagnostics, "cxt.template.invalid",
                               "Animation template contains an unknown core field",
                               json::appendFieldPath(reader.fieldPath(), name));
        }
    }
}

[[nodiscard]] auto readMetadata(const json::Reader& reader, const ChartLimits& limits,
                                core::Diagnostics& diagnostics) -> std::optional<std::string> {
    constexpr std::array fields{std::string_view{"name"}};
    if (reader.readObject() == nullptr) {
        return std::nullopt;
    }
    reader.rejectUnknownFields(fields);
    const auto nameReader = reader.optionalField("name");
    if (!nameReader) {
        return std::nullopt;
    }
    const auto name = nameReader->readString();
    if (!name) {
        return std::nullopt;
    }
    if (name->size() > limits.maxAnimationTemplateNameBytes) {
        detail::addV4Error(diagnostics, "cxt.template.invalid",
                           "Animation template name exceeds the configured byte limit",
                           std::string{nameReader->fieldPath()});
        return std::nullopt;
    }
    return std::string{*name};
}

[[nodiscard]] auto readApplication(const json::Reader& reader, core::Diagnostics& diagnostics)
    -> std::optional<AnimationTemplateApplication> {
    constexpr std::array fields{std::string_view{"coordinateSpace"}, std::string_view{"blendMode"},
                                std::string_view{"iterations"}, std::string_view{"fillMode"}};
    if (reader.readObject() == nullptr) {
        return std::nullopt;
    }
    reader.rejectUnknownFields(fields);
    const auto coordinateReader = reader.requiredField("coordinateSpace");
    const auto blendReader = reader.requiredField("blendMode");
    const auto iterationsReader = reader.requiredField("iterations");
    const auto fillReader = reader.requiredField("fillMode");
    auto coordinate = std::optional<std::string_view>{};
    auto blend = std::optional<AnimationBlendMode>{};
    auto iterations = std::optional<AnimationIterations>{};
    auto fill = std::optional<AnimationFillMode>{};
    if (coordinateReader) {
        coordinate = coordinateReader->readString();
    }
    if (blendReader) {
        blend = detail::readBlendMode(*blendReader, diagnostics);
    }
    if (iterationsReader) {
        iterations = detail::readIterations(*iterationsReader, diagnostics);
    }
    if (fillReader) {
        fill = detail::readFillMode(*fillReader, diagnostics);
    }
    if (coordinate && *coordinate != "local") {
        detail::addV4Error(diagnostics, "cxt.template.invalid",
                           "Animation template coordinateSpace must be local",
                           std::string{coordinateReader->fieldPath()});
    }
    if (!coordinate || *coordinate != "local" || !blend || !iterations || !fill) {
        return std::nullopt;
    }
    const auto application = AnimationTemplateApplication{*blend, *iterations, *fill};
    if (application.iterations.infinite && application.fillMode != AnimationFillMode::None) {
        detail::addV4Error(diagnostics, "cxt.template.invalid",
                           "Infinite animation template must use fillMode none",
                           std::string{fillReader->fieldPath()});
    }
    return application;
}

void validateApplicationClip(const AnimationTemplateApplication& application,
                             const AnimationClip& clip, core::Diagnostics& diagnostics) {
    if (application.blendMode != AnimationBlendMode::Additive) {
        return;
    }
    if (!clip.stepTracks.empty()) {
        detail::addV4Error(diagnostics, "chart.animation.additive_unsupported",
                           "Additive animation template cannot contain step tracks",
                           clip.fieldPath + "/stepTracks");
    }
    for (const auto& track : clip.tracks) {
        if (track.property == AnimationProperty::MaterialOpacity ||
            track.property == AnimationProperty::MaterialTint) {
            detail::addV4Error(diagnostics, "chart.animation.additive_unsupported",
                               "Additive animation template targets an unsupported property",
                               track.fieldPath + "/property");
        }
        if (track.property != AnimationProperty::TransformScale) {
            continue;
        }
        for (const auto& segment : track.segments) {
            const auto* start = std::get_if<core::Vec3>(&segment.startValue);
            const auto* end = std::get_if<core::Vec3>(&segment.endValue);
            if (start == nullptr || end == nullptr || start->x <= 0.0F || start->y <= 0.0F ||
                start->z <= 0.0F || end->x <= 0.0F || end->y <= 0.0F || end->z <= 0.0F) {
                detail::addV4Error(diagnostics, "chart.animation.additive_unsupported",
                                   "Additive transform.scale requires positive factors",
                                   segment.fieldPath);
            }
        }
    }
}

} // namespace

auto AnimationTemplateLoader::load(std::string_view jsonText, const ChartLimits& limits)
    -> AnimationTemplateResult {
    auto diagnostics = detail::makeDiagnostics(limits);
    if (detail::rejectInvalidDiagnosticLimit(diagnostics, limits)) {
        return AnimationTemplateResult{std::nullopt, std::move(diagnostics)};
    }
    const auto inputLimit = std::min(limits.maxInputBytes, limits.maxAnimationTemplateBytes);
    auto parsed = json::parse(
        jsonText, json::ParseLimits{inputLimit, limits.maxNestingDepth, limits.maxStringBytes});
    if (!parsed) {
        addParseError(diagnostics, parsed.error());
        diagnostics.sortDeterministically();
        return AnimationTemplateResult{std::nullopt, std::move(diagnostics)};
    }

    json::Reader root{*parsed, diagnostics};
    if (root.readObject() == nullptr) {
        diagnostics.sortDeterministically();
        return AnimationTemplateResult{std::nullopt, std::move(diagnostics)};
    }
    rejectUnknownTopLevelFields(root, diagnostics);
    const auto formatReader = root.requiredField("format");
    const auto versionReader = root.requiredField("version");
    const auto templateIdReader = root.requiredField("templateId");
    const auto metadataReader = root.requiredField("metadata");
    const auto applicationReader = root.requiredField("application");
    const auto clipReader = root.requiredField("clip");
    const auto requiredExtensionsReader = root.requiredField("requiredExtensions");
    const auto extensionsReader = root.requiredField("extensions");

    const auto format = formatReader ? formatReader->readString() : std::nullopt;
    auto version = std::optional<std::int64_t>{};
    if (versionReader) {
        version = versionReader->readInt64();
    }
    const auto templateId =
        templateIdReader
            ? detail::readPortableStableId(*templateIdReader, limits, diagnostics, "Template ID")
            : std::nullopt;
    if (format && *format != "cuexis.animation-template") {
        detail::addV4Error(diagnostics, "cxt.format.unsupported",
                           "Animation template format is unsupported",
                           std::string{formatReader->fieldPath()});
    }
    if (version && *version != 1) {
        detail::addV4Error(diagnostics, "cxt.version.unsupported",
                           "Animation template version is unsupported",
                           std::string{versionReader->fieldPath()});
    }

    const auto name =
        metadataReader ? readMetadata(*metadataReader, limits, diagnostics) : std::nullopt;
    const auto application =
        applicationReader ? readApplication(*applicationReader, diagnostics) : std::nullopt;
    auto clip = clipReader ? detail::readAnimationClip(*clipReader, limits, diagnostics, false,
                                                       templateId.value_or(std::string{}))
                           : std::nullopt;
    if (application && clip) {
        validateApplicationClip(*application, *clip, diagnostics);
    }
    auto requiredExtensions =
        requiredExtensionsReader
            ? detail::readRequiredExtensions(*requiredExtensionsReader, limits, diagnostics)
            : std::vector<RequiredExtension>{};

    OpaqueJson extensions;
    if (extensionsReader) {
        const auto* object = extensionsReader->readObject();
        if (object != nullptr && object->size() > limits.maxExtensions) {
            detail::addV4Error(diagnostics, "cxt.budget.exceeded",
                               "Animation template extensions exceed the member limit",
                               std::string{extensionsReader->fieldPath()});
        }
        if (auto serialized = json::serialize(extensionsReader->value())) {
            extensions.canonicalText = std::move(*serialized);
        } else {
            addParseError(diagnostics, serialized.error());
        }
    }
    OpaqueJson canonicalSource;
    if (auto serialized = json::serialize(*parsed)) {
        canonicalSource.canonicalText = std::move(*serialized);
    } else {
        addParseError(diagnostics, serialized.error());
    }

    diagnostics.sortDeterministically();
    if (diagnostics.hasErrors() || !format || *format != "cuexis.animation-template" || !version ||
        *version != 1 || !templateId || !metadataReader || !application || !clip ||
        !requiredExtensionsReader || !extensionsReader) {
        return AnimationTemplateResult{std::nullopt, std::move(diagnostics)};
    }
    return AnimationTemplateResult{
        AnimationTemplateDocument{*templateId, name, *application, std::move(*clip),
                                  std::move(requiredExtensions), std::move(extensions),
                                  std::move(canonicalSource)},
        std::move(diagnostics)};
}

} // namespace cuexis::chart

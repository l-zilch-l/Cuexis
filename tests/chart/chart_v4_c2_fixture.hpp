#pragma once

// Shared Chart v4 C2 fixture for the contract case units.

#include <cuexis/chart/canonical_chart_loader.hpp>
#include <cuexis/chart/chart_loader.hpp>
#include <cuexis/chart/chart_v4_loader.hpp>
#include <cuexis/chart/chart_v4_resolver.hpp>
#include <cuexis/chart/chart_writer.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace chart_c2_fixture {

struct ChartParts final {
    std::string timing{R"({"offsetMs":0,"defaultBpm":120,"tempoEvents":[],"stops":[]})"};
    std::optional<std::string> audio;
    std::string parameters{"[]"};
    std::string templates{"[]"};
    std::string behaviors{"[]"};
    std::string imports{"[]"};
    std::string clips{"[]"};
    std::string objects{"[]"};
    std::string requiredExtensions{"[]"};
    std::string extensions{"{}"};
};

inline [[nodiscard]] auto makeChart(const ChartParts& parts) -> std::string {
    auto result = std::string{R"({
      "format":"cuexis.chart","version":4,
      "chartId":"019f0000-0000-7abc-8def-0000000004c2",
      "metadata":{"title":"CFU-C2 contract"},
      "timing":)"};
    result += parts.timing;
    result += R"(,
      "camera":{"type":"perspective","fovY":60,"near":0.1,"far":1000})";
    if (parts.audio) {
        result += ",\n      \"audio\":" + *parts.audio;
    }
    result += ",\n      \"parameters\":" + parts.parameters;
    result += ",\n      \"templates\":" + parts.templates;
    result += ",\n      \"behaviors\":" + parts.behaviors;
    result += ",\n      \"animationTemplateImports\":" + parts.imports;
    result += ",\n      \"animationClips\":" + parts.clips;
    result += ",\n      \"objects\":" + parts.objects;
    result += ",\n      \"requiredExtensions\":" + parts.requiredExtensions;
    result += ",\n      \"extensions\":" + parts.extensions + "\n    }";
    return result;
}

inline [[nodiscard]] auto makeCxt(std::string_view templateId, std::string_view clip,
                                  std::string_view blendMode = "override",
                                  std::string_view requiredExtensions = "[]") -> std::string {
    auto result = std::string{R"({
      "format":"cuexis.animation-template","version":1,"templateId":")"};
    result += templateId;
    result += R"(","metadata":{"name":"contract"},
      "application":{"coordinateSpace":"local","blendMode":")";
    result += blendMode;
    result += R"(","iterations":1,"fillMode":"none"},
      "clip":)";
    result += clip;
    result += ",\n      \"requiredExtensions\":";
    result += requiredExtensions;
    result += ",\n      \"extensions\":{}\n    }";
    return result;
}

inline [[nodiscard]] auto loadSource(std::string_view text)
    -> cuexis::chart::ChartV4SourceDocument {
    auto result = cuexis::chart::ChartV4Loader::load(text);
    if (!result.hasValue()) {
        throw std::runtime_error{"Inline CFU-C2 Chart did not load"};
    }
    return std::move(*result.document);
}

inline [[nodiscard]] auto hasDiagnostic(const cuexis::chart::ChartV4ResolveResult& result,
                                        std::string_view code) -> bool {
    return std::ranges::any_of(result.diagnostics.items(),
                               [code](const auto& item) { return item.code() == code; });
}

inline [[nodiscard]] auto hasDiagnostic(const cuexis::chart::ChartV4SourceResult& result,
                                        std::string_view code) -> bool {
    return std::ranges::any_of(result.diagnostics.items(),
                               [code](const auto& item) { return item.code() == code; });
}

inline [[nodiscard]] auto findDiagnostic(const cuexis::chart::ChartV4SourceResult& result,
                                         std::string_view code) -> const cuexis::core::Diagnostic* {
    const auto item =
        std::ranges::find_if(result.diagnostics.items(),
                             [code](const auto& diagnostic) { return diagnostic.code() == code; });
    return item != result.diagnostics.items().end() ? &*item : nullptr;
}

inline [[nodiscard]] auto findDiagnostic(const cuexis::chart::ChartV4ResolveResult& result,
                                         std::string_view code, std::string_view recordKind = {})
    -> const cuexis::core::Diagnostic* {
    const auto item = std::ranges::find_if(result.diagnostics.items(), [&](const auto& diagnostic) {
        if (diagnostic.code() != code) {
            return false;
        }
        if (recordKind.empty()) {
            return true;
        }
        return std::ranges::any_of(diagnostic.context(), [&](const auto& context) {
            return context.key == "record_kind" && context.value == recordKind;
        });
    });
    return item != result.diagnostics.items().end() ? &*item : nullptr;
}

inline [[nodiscard]] auto contextValue(const cuexis::core::Diagnostic& diagnostic,
                                       std::string_view key) -> std::optional<std::string_view> {
    const auto item = std::ranges::find_if(
        diagnostic.context(), [key](const auto& context) { return context.key == key; });
    return item != diagnostic.context().end() ? std::optional<std::string_view>{item->value}
                                              : std::nullopt;
}

inline [[nodiscard]] auto
continuousClip(std::string_view id, std::string_view property, std::string_view startValue,
               std::string_view endValue,
               std::string_view duration = R"({"numerator":1,"denominator":1})") -> std::string {
    auto result = std::string{"{\"id\":\""};
    result += id;
    result += "\",\"version\":1,\"durationBeats\":";
    result += duration;
    result += ",\"tracks\":[{\"property\":\"";
    result += property;
    result += R"(","segments":[{"startBeat":{"numerator":0,"denominator":1},"durationBeats":)";
    result += duration;
    result += ",\"startValue\":";
    result += startValue;
    result += ",\"endValue\":";
    result += endValue;
    result += R"(,"startSlope":0,"endSlope":0}]}],"stepTracks":[]})";
    return result;
}

inline [[nodiscard]] auto cxtContinuousClip(std::string_view property, std::string_view startValue,
                                            std::string_view endValue) -> std::string {
    auto clip = continuousClip("ignored", property, startValue, endValue);
    const auto id = clip.find("\"id\":\"ignored\",");
    if (id == std::string::npos) {
        throw std::runtime_error{"Could not strip CXT clip ID"};
    }
    clip.erase(id, std::string_view{"\"id\":\"ignored\","}.size());
    return clip;
}

inline [[nodiscard]] auto stepMaterialClip(std::string_view id, std::string_view assetId,
                                           bool includeId = true) -> std::string {
    auto result = std::string{"{"};
    if (includeId) {
        result += "\"id\":\"";
        result += id;
        result += "\",";
    }
    result += R"("version":1,"durationBeats":{"numerator":1,"denominator":1},
      "tracks":[],"stepTracks":[{"property":"render.material","steps":[{
        "beat":{"numerator":0,"denominator":1},"value":{"domain":"asset","id":")";
    result += assetId;
    result += R"("}}]}]})";
    return result;
}

inline [[nodiscard]] auto transformComponent() -> std::string_view {
    return R"("cuexis.transform":{"version":1,"position":[0,0,0],
      "rotation":[0,0,0,1],"scale":[1,1,1]})";
}

inline [[nodiscard]] auto findResource(const cuexis::chart::ChartV4ResolvedArtifact& artifact,
                                       std::string_view id)
    -> const cuexis::chart::ChartResourceRequirement* {
    const auto item = std::ranges::find_if(artifact.resourceRequirements, [id](const auto& value) {
        return value.assetId.value == id;
    });
    return item != artifact.resourceRequirements.end() ? &*item : nullptr;
}

} // namespace chart_c2_fixture

using namespace chart_c2_fixture;

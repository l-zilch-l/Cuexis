#include <cuexis/chart/animation_template_loader.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

[[nodiscard]] auto readFile(const std::filesystem::path& path) -> std::string {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw std::runtime_error{"Could not open CXT fixture: " + path.string()};
    }
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

[[nodiscard]] auto hasDiagnostic(const cuexis::chart::AnimationTemplateResult& result,
                                 std::string_view code) -> bool {
    for (const auto& diagnostic : result.diagnostics.items()) {
        if (diagnostic.code() == code) {
            return true;
        }
    }
    return false;
}

} // namespace

TEST_CASE("CXT Reader accepts the promoted animation template", "[chart][cxt][cfu-c1]") {
    const auto path = std::filesystem::path{CUEXIS_SOURCE_DIR} / "tests" / "fixtures" /
                      "chart_format_update" / "valid" / "templates" / "move-y.cxt";
    const auto result = cuexis::chart::AnimationTemplateLoader::load(readFile(path));
    REQUIRE(result.hasValue());
    CHECK(result.document->templateId == "motion.move-y");
    CHECK(result.document->application.blendMode == cuexis::chart::AnimationBlendMode::Additive);
    CHECK(result.document->clip.tracks.size() == 1);
}

TEST_CASE("CXT Reader rejects runtime script fields", "[chart][cxt][cfu-c1]") {
    const auto path = std::filesystem::path{CUEXIS_SOURCE_DIR} / "tests" / "fixtures" /
                      "chart_format_update" / "invalid" / "move_y_runtime_script.cxt";
    const auto result = cuexis::chart::AnimationTemplateLoader::load(readFile(path));
    CHECK_FALSE(result.hasValue());
    CHECK(hasDiagnostic(result, "cxt.template.invalid"));
}

TEST_CASE("CXT Reader accepts the valid template paired with a mismatched Chart import",
          "[chart][cxt][cfu-c1]") {
    const auto path = std::filesystem::path{CUEXIS_SOURCE_DIR} / "tests" / "fixtures" /
                      "chart_format_update" / "invalid" / "templates" / "move-y.cxt";
    const auto result = cuexis::chart::AnimationTemplateLoader::load(readFile(path));
    REQUIRE(result.hasValue());
    CHECK(result.document->templateId == "motion.move-y");
}

TEST_CASE("CXT infinite iterations do not emit a numeric type mismatch", "[chart][cxt][cfu-c1]") {
    constexpr std::string_view document = R"({
      "format":"cuexis.animation-template","version":1,"templateId":"motion.hold",
      "metadata":{},
      "application":{"coordinateSpace":"local","blendMode":"override",
                     "iterations":"infinite","fillMode":"none"},
      "clip":{"version":1,"durationBeats":{"numerator":1,"denominator":1},
              "tracks":[{"property":"transform.position.y","segments":[{
                "startBeat":{"numerator":0,"denominator":1},
                "durationBeats":{"numerator":1,"denominator":1},
                "startValue":0,"endValue":1,"startSlope":0,"endSlope":0}]}],
              "stepTracks":[]},
      "requiredExtensions":[],"extensions":{}
    })";
    const auto result = cuexis::chart::AnimationTemplateLoader::load(document);
    REQUIRE(result.hasValue());
    CHECK_FALSE(hasDiagnostic(result, "json.type.mismatch"));
}

TEST_CASE("CXT Reader rejects duplicate segment starts after a zero-duration segment",
          "[chart][cxt][cfu-c1]") {
    constexpr std::string_view document = R"({
      "format":"cuexis.animation-template","version":1,"templateId":"motion.conflict",
      "metadata":{},
      "application":{"coordinateSpace":"local","blendMode":"override",
                     "iterations":1,"fillMode":"none"},
      "clip":{"version":1,"durationBeats":{"numerator":1,"denominator":1},
              "tracks":[{"property":"transform.position.y","segments":[{
                "startBeat":{"numerator":0,"denominator":1},
                "durationBeats":{"numerator":0,"denominator":1},
                "startValue":0,"endValue":0,"startSlope":0,"endSlope":0},{
                "startBeat":{"numerator":0,"denominator":1},
                "durationBeats":{"numerator":1,"denominator":1},
                "startValue":0,"endValue":1,"startSlope":0,"endSlope":0}]}],
              "stepTracks":[]},
      "requiredExtensions":[],"extensions":{}
    })";
    const auto result = cuexis::chart::AnimationTemplateLoader::load(document);
    CHECK_FALSE(result.hasValue());
    CHECK(hasDiagnostic(result, "chart.animation.track_conflict"));
}

TEST_CASE("CXT Reader rejects an empty step array", "[chart][cxt][cfu-c1]") {
    constexpr std::string_view document = R"({
      "format":"cuexis.animation-template","version":1,"templateId":"visibility.empty",
      "metadata":{},
      "application":{"coordinateSpace":"local","blendMode":"override",
                     "iterations":1,"fillMode":"none"},
      "clip":{"version":1,"durationBeats":{"numerator":1,"denominator":1},
              "tracks":[],
              "stepTracks":[{"property":"render.visible","steps":[]}]},
      "requiredExtensions":[],"extensions":{}
    })";
    const auto result = cuexis::chart::AnimationTemplateLoader::load(document);
    CHECK_FALSE(result.hasValue());
    CHECK(hasDiagnostic(result, "chart.animation.clip_invalid"));
}

TEST_CASE("CXT Reader applies positive scale only to additive templates", "[chart][cxt][cfu-c2]") {
    constexpr std::string_view document = R"({
      "format":"cuexis.animation-template","version":1,"templateId":"motion.scale",
      "metadata":{},
      "application":{"coordinateSpace":"local","blendMode":"additive",
        "iterations":1,"fillMode":"none"},
      "clip":{"version":1,"durationBeats":{"numerator":1,"denominator":1},
        "tracks":[{"property":"transform.scale","segments":[{
          "startBeat":{"numerator":0,"denominator":1},
          "durationBeats":{"numerator":1,"denominator":1},
          "startValue":[1,1,1],"endValue":[0,1,1],"startSlope":0,"endSlope":0
        }]}],"stepTracks":[]},
      "requiredExtensions":[],"extensions":{}
    })";
    const auto result = cuexis::chart::AnimationTemplateLoader::load(document);
    CHECK_FALSE(result.hasValue());
    CHECK(hasDiagnostic(result, "chart.animation.additive_unsupported"));
}

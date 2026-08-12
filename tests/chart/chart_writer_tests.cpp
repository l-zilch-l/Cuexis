#include <cuexis/chart/animation_template_loader.hpp>
#include <cuexis/chart/chart_v4_loader.hpp>
#include <cuexis/chart/chart_writer.hpp>

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
        throw std::runtime_error{"Could not open Writer fixture: " + path.string()};
    }
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

[[nodiscard]] auto fixture(std::string_view relative) -> std::filesystem::path {
    return std::filesystem::path{CUEXIS_SOURCE_DIR} / "tests" / "fixtures" / "chart_format_update" /
           relative;
}

} // namespace

TEST_CASE("Chart v4 canonical Writer round-trips and emits one LF", "[chart][v4][writer][cfu-c2]") {
    const auto loaded = cuexis::chart::ChartV4Loader::load(
        readFile(fixture("valid/chart_v4_cxt_template_binding.json")));
    REQUIRE(loaded.hasValue());

    const auto first = cuexis::chart::ChartWriter::writeV4(*loaded.document);
    REQUIRE(first.has_value());
    REQUIRE_FALSE(first->empty());
    CHECK(first->back() == '\n');
    const bool hasSingleLfEnding = first->size() < 2 || (*first)[first->size() - 2] != '\r';
    CHECK(hasSingleLfEnding);

    const auto reloaded = cuexis::chart::ChartV4Loader::load(*first);
    REQUIRE(reloaded.hasValue());
    const auto second = cuexis::chart::ChartWriter::writeV4(*reloaded.document);
    REQUIRE(second.has_value());
    CHECK(*second == *first);
}

TEST_CASE("Chart v4 canonical Writer sorts arrays and reduces Rational values",
          "[chart][v4][writer][cfu-c2]") {
    constexpr std::string_view source = R"({
      "objects":[],"extensions":{},"requiredExtensions":[],"behaviors":[],
      "animationClips":[],"animationTemplateImports":[],"templates":[],
      "parameters":[
        {"constraints":{},"default":1,"type":"number","id":"z.value"},
        {"constraints":{},"default":{"denominator":2,"numerator":2},
         "type":"rational","id":"a.value"}],
      "timing":{"stops":[],"tempoEvents":[],"defaultBpm":120,"offsetMs":-0.0},
      "metadata":{},"chartId":"019f0000-0000-7abc-8def-0000000004f0",
      "version":4,"format":"cuexis.chart"})";
    const auto loaded = cuexis::chart::ChartV4Loader::load(source);
    REQUIRE(loaded.hasValue());
    const auto written = cuexis::chart::ChartWriter::writeV4(*loaded.document);
    REQUIRE(written.has_value());
    CHECK(written->find("\"id\": \"a.value\"") < written->find("\"id\": \"z.value\""));
    CHECK(written->find("\"denominator\": 1") != std::string::npos);
    CHECK(written->find("-0.0") == std::string::npos);
}

TEST_CASE("CXT canonical Writer round-trips deterministically", "[chart][cxt][writer][cfu-c2]") {
    const auto loaded = cuexis::chart::AnimationTemplateLoader::load(
        readFile(fixture("valid/templates/move-y.cxt")));
    REQUIRE(loaded.hasValue());
    const auto first = cuexis::chart::ChartWriter::writeAnimationTemplate(*loaded.document);
    REQUIRE(first.has_value());
    CHECK(first->back() == '\n');

    const auto reloaded = cuexis::chart::AnimationTemplateLoader::load(*first);
    REQUIRE(reloaded.hasValue());
    const auto second = cuexis::chart::ChartWriter::writeAnimationTemplate(*reloaded.document);
    REQUIRE(second.has_value());
    CHECK(*second == *first);
}

TEST_CASE("Chart and CXT canonical Writers match committed byte goldens",
          "[chart][writer][golden][cfu-c2]") {
    const auto chart = cuexis::chart::ChartV4Loader::load(
        readFile(fixture("valid/chart_v4_static_migration.json")));
    REQUIRE(chart.hasValue());
    const auto chartBytes = cuexis::chart::ChartWriter::writeV4(*chart.document);
    REQUIRE(chartBytes.has_value());
    CHECK(*chartBytes == readFile(fixture("golden/chart_v4_static_migration.canonical.json")));

    const auto cxt = cuexis::chart::AnimationTemplateLoader::load(
        readFile(fixture("valid/templates/move-y.cxt")));
    REQUIRE(cxt.hasValue());
    const auto cxtBytes = cuexis::chart::ChartWriter::writeAnimationTemplate(*cxt.document);
    REQUIRE(cxtBytes.has_value());
    CHECK(*cxtBytes == readFile(fixture("golden/move-y.canonical.cxt")));
}

TEST_CASE("Canonical Writer does not reinterpret Rational-like opaque extension data",
          "[chart][v4][writer][extension][cfu-c2]") {
    constexpr std::string_view source = R"({
      "format":"cuexis.chart","version":4,
      "chartId":"019f0000-0000-7abc-8def-0000000004f8",
      "metadata":{},
      "timing":{"offsetMs":0,"defaultBpm":120,"tempoEvents":[],"stops":[]},
      "parameters":[{"id":"ratio","type":"rational",
        "default":{"numerator":2,"denominator":4},"constraints":{}}],
      "templates":[],"behaviors":[],"animationTemplateImports":[],
      "animationClips":[],"objects":[],"requiredExtensions":[],
      "extensions":{"org.example":{"rationalLike":{"numerator":2,"denominator":4}}}
    })";
    const auto loaded = cuexis::chart::ChartV4Loader::load(source);
    REQUIRE(loaded.hasValue());
    const auto written = cuexis::chart::ChartWriter::writeV4(*loaded.document);
    REQUIRE(written.has_value());
    CHECK(written->find("\"denominator\": 4") != std::string::npos);
    CHECK(written->find("\"numerator\": 2") != std::string::npos);
    CHECK(written->find("\"denominator\": 2") != std::string::npos);
    CHECK(written->find("\"numerator\": 1") != std::string::npos);
}

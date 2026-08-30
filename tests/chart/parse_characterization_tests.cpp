#include <cuexis/chart/animation_template_loader.hpp>
#include <cuexis/chart/canonical_chart_loader.hpp>
#include <cuexis/chart/chart_loader.hpp>
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
        throw std::runtime_error{"Could not open characterization fixture: " + path.string()};
    }
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

[[nodiscard]] auto readSource(std::string_view relativePath) -> std::string {
    return readFile(std::filesystem::path{CUEXIS_SOURCE_DIR} / relativePath);
}

[[nodiscard]] auto functionBody(const std::string& source, std::string_view functionName)
    -> std::string_view {
    const auto functionStart = source.find(functionName);
    if (functionStart == std::string::npos) {
        return {};
    }
    const auto openBrace = source.find('{', functionStart);
    if (openBrace == std::string::npos) {
        return {};
    }
    std::size_t depth = 0;
    for (std::size_t index = openBrace; index < source.size(); ++index) {
        if (source[index] == '{') {
            ++depth;
        } else if (source[index] == '}' && --depth == 0) {
            return std::string_view{source}.substr(openBrace + 1U, index - openBrace - 1U);
        }
    }
    return {};
}

[[nodiscard]] auto countOccurrences(std::string_view text, std::string_view needle) -> std::size_t {
    std::size_t count = 0;
    for (std::size_t offset = text.find(needle); offset != std::string_view::npos;
         offset = text.find(needle, offset + needle.size())) {
        ++count;
    }
    return count;
}

[[nodiscard]] auto diagnosticsFingerprint(const cuexis::core::Diagnostics& diagnostics)
    -> std::string {
    std::ostringstream output;
    for (const auto& diagnostic : diagnostics.items()) {
        output << static_cast<int>(diagnostic.severity()) << '|' << diagnostic.code() << '|'
               << diagnostic.fieldPath() << '|' << diagnostic.message();
        for (const auto& context : diagnostic.context()) {
            output << '|' << context.key << '=' << context.value;
        }
        output << '\n';
    }
    return output.str();
}

constexpr std::string_view invalidCanonicalChart = R"json({
  "format":"cuexis.chart",
  "version":"one",
  "chartId":"not-a-uuid",
  "metadata":{},
  "timing":{"offsetMs":0,"defaultBpm":120,"bpmChanges":[],"stops":[]},
  "templates":[],"behaviors":[],"objects":[],
  "requiredExtensions":[],"extensions":{},"futureCoreField":true
})json";

} // namespace

TEST_CASE("ChartLoader routing keeps the current two-phase parse baseline",
          "[chart][parse][characterization][count]") {
    const auto source = readSource("engine/chart/src/chart_loader.cpp");
    const auto body = functionBody(source, "ChartLoader::load");
    REQUIRE_FALSE(body.empty());

    // The first parse peeks at format; canonical routing reparses the original text today.
    CHECK(countOccurrences(body, "json::parse(") == 1U);
    CHECK(body.find("CanonicalChartLoader::load(jsonText, limits)") != std::string_view::npos);
}

TEST_CASE("Chart v4 and CXC preparation retain the current parse layering baseline",
          "[chart][cxc][parse][characterization][count]") {
    const auto loaderSource = readSource("engine/chart/src/chart_v4_loader.cpp");
    const auto loaderBody = functionBody(loaderSource, "ChartV4Loader::load");
    const auto projectionBody = functionBody(loaderSource, "makeLegacyProjection");
    REQUIRE_FALSE(loaderBody.empty());
    REQUIRE_FALSE(projectionBody.empty());
    CHECK(countOccurrences(loaderBody, "json::parse(") == 1U);
    CHECK(projectionBody.find("detail::loadCanonicalValue(std::move(source), limits)") !=
          std::string_view::npos);
    CHECK(projectionBody.find("json::serialize(source)") == std::string_view::npos);

    const auto canonicalSource = readSource("engine/chart/src/canonical_chart_loader.cpp");
    const auto canonicalBody = functionBody(canonicalSource, "detail::loadCanonicalValue");
    REQUIRE_FALSE(canonicalBody.empty());
    CHECK(countOccurrences(canonicalBody, "json::parse(") == 0U);

    const auto resolverSource = readSource("engine/chart/src/chart_v4_resolver.cpp");
    const auto resolverBody = functionBody(resolverSource, "ChartV4Resolver::resolve");
    const auto concreteBody = functionBody(resolverSource, "makeConcreteChart");
    REQUIRE_FALSE(resolverBody.empty());
    REQUIRE_FALSE(concreteBody.empty());
    CHECK(countOccurrences(resolverBody, "json::parse(") == 1U);
    CHECK(concreteBody.find("CanonicalChartLoader::load(*serialized, limits)") !=
          std::string_view::npos);

    const auto writerSource = readSource("engine/chart/src/chart_writer.cpp");
    const auto writerBody = functionBody(writerSource, "ChartWriter::writeV4");
    REQUIRE_FALSE(writerBody.empty());
    CHECK(countOccurrences(writerBody, "json::parse(") == 1U);

    const auto cxcSource = readSource("engine/cxc/src/cxc_package.cpp");
    const auto peekBody = functionBody(cxcSource, "isV4Chart");
    REQUIRE_FALSE(peekBody.empty());
    CHECK(countOccurrences(peekBody, "json::parse(") == 1U);
    CHECK(cxcSource.find("isV4Chart(chartText, chartLimits)") != std::string::npos);
    CHECK(cxcSource.find("ChartV4Loader::load(chartText, chartLimits)") != std::string::npos);
}

TEST_CASE("ChartLoader diagnostics are stable across routed and direct entry points",
          "[chart][parse][characterization][diagnostics]") {
    const auto routed = cuexis::chart::ChartLoader::load(invalidCanonicalChart);
    const auto direct = cuexis::chart::CanonicalChartLoader::load(invalidCanonicalChart);
    const auto repeated = cuexis::chart::ChartLoader::load(invalidCanonicalChart);
    REQUIRE_FALSE(routed.hasValue());
    REQUIRE_FALSE(direct.hasValue());
    REQUIRE_FALSE(repeated.hasValue());

    const auto directFingerprint = diagnosticsFingerprint(direct.diagnostics);
    CHECK(diagnosticsFingerprint(routed.diagnostics) == directFingerprint);
    CHECK(diagnosticsFingerprint(repeated.diagnostics) == directFingerprint);
    REQUIRE(direct.diagnostics.size() >= 2U);
    CHECK(direct.diagnostics.items().front().fieldPath() == "$/chartId");
    CHECK(direct.diagnostics.items().back().fieldPath() == "$/version");
}

TEST_CASE("Chart v4 and CXT canonical source bytes remain writer-parity baselines",
          "[chart][cxt][parse][characterization][canonical]") {
    const auto chartText =
        readSource("tests/fixtures/chart_format_update/valid/chart_v4_static_migration.json");
    const auto chart = cuexis::chart::ChartV4Loader::load(chartText);
    REQUIRE(chart.hasValue());
    REQUIRE(chart.document.has_value());
    const auto chartBytes = cuexis::chart::ChartWriter::writeV4(*chart.document);
    REQUIRE(chartBytes.has_value());
    const auto chartCanonicalized = cuexis::chart::ChartWriter::writeCanonicalJson(
        chart.document->canonicalSource.canonicalText);
    REQUIRE(chartCanonicalized.has_value());
    CHECK(*chartBytes == *chartCanonicalized);

    auto spacedChart = std::string{"\n  "} + chartText + "\n";
    const auto spaced = cuexis::chart::ChartV4Loader::load(spacedChart);
    REQUIRE(spaced.hasValue());
    REQUIRE(spaced.document.has_value());
    CHECK(spaced.document->canonicalSource.canonicalText ==
          chart.document->canonicalSource.canonicalText);

    const auto cxtText =
        readSource("tests/fixtures/chart_format_update/valid/templates/move-y.cxt");
    const auto cxt = cuexis::chart::AnimationTemplateLoader::load(cxtText);
    REQUIRE(cxt.hasValue());
    REQUIRE(cxt.document.has_value());
    const auto cxtBytes = cuexis::chart::ChartWriter::writeAnimationTemplate(*cxt.document);
    REQUIRE(cxtBytes.has_value());
    const auto cxtCanonicalized =
        cuexis::chart::ChartWriter::writeCanonicalJson(cxt.document->canonicalSource.canonicalText);
    REQUIRE(cxtCanonicalized.has_value());
    CHECK(*cxtBytes == *cxtCanonicalized);
}

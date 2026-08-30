#include <cuexis/chart/chart_loader.hpp>
#include <cuexis/chart/chart_v4_loader.hpp>
#include <cuexis/cxc/cxc_package.hpp>

#include "cxc_test_support.hpp"

#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

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

[[nodiscard]] auto findEntry(cuexis::cxc::CxcWriteRequest& request, std::string_view path)
    -> cuexis::cxc::CxcWriteEntry& {
    for (auto& entry : request.entries) {
        if (entry.path == path) {
            return entry;
        }
    }
    throw std::runtime_error{"CXC characterization entry is missing"};
}

constexpr std::string_view invalidChart = R"json({
  "format":"cuexis.chart",
  "version":"one",
  "chartId":"not-a-uuid",
  "metadata":{},
  "timing":{"offsetMs":0,"defaultBpm":120,"bpmChanges":[],"stops":[]},
  "templates":[],"behaviors":[],"objects":[],
  "requiredExtensions":[],"extensions":{},"futureCoreField":true
})json";

} // namespace

TEST_CASE("CXC project documents preserve source bytes while typed readers expose canonical bytes",
          "[cxc][chart][parse][characterization][canonical]") {
    const auto bytes = cuexis::cxc::test::writePackage(cuexis::cxc::test::makeV4CxtRequest());
    const auto loaded = cuexis::cxc::CxcPackageLoader::loadMemory(bytes);
    INFO(cuexis::cxc::test::diagnosticsText(loaded.diagnostics));
    REQUIRE(loaded.hasValue());
    REQUIRE(loaded.package->projectDocuments().size() == 2U);

    const auto& chartDocument = loaded.package->projectDocuments().front();
    REQUIRE(chartDocument.path == "assets/charts/main.cuexis.chart.json");
    const auto chart = cuexis::chart::ChartV4Loader::load(chartDocument.utf8Text);
    REQUIRE(chart.hasValue());
    REQUIRE(chart.document.has_value());
    CHECK(chart.document->canonicalSource.canonicalText != chartDocument.utf8Text);

    const auto canonicalReload =
        cuexis::chart::ChartV4Loader::load(chart.document->canonicalSource.canonicalText);
    REQUIRE(canonicalReload.hasValue());
    REQUIRE(canonicalReload.document.has_value());
    CHECK(canonicalReload.document->canonicalSource.canonicalText ==
          chart.document->canonicalSource.canonicalText);
}

TEST_CASE("CXC invalid Chart diagnostics retain stable code path and ordering",
          "[cxc][chart][parse][characterization][diagnostics]") {
    auto request = cuexis::cxc::test::makeV4StaticRequest();
    auto& chartEntry = findEntry(request, "assets/charts/main.cuexis.chart.json");
    chartEntry.bytes = cuexis::cxc::test::bytesFromText(invalidChart);
    const auto bytes = cuexis::cxc::test::writeUncheckedPackage(std::move(request.entries));

    const auto first = cuexis::cxc::CxcPackageLoader::loadMemory(bytes);
    const auto second = cuexis::cxc::CxcPackageLoader::loadMemory(bytes);
    INFO(cuexis::cxc::test::diagnosticsText(first.diagnostics));
    REQUIRE_FALSE(first.hasValue());
    REQUIRE_FALSE(second.hasValue());
    CHECK(diagnosticsFingerprint(first.diagnostics) == diagnosticsFingerprint(second.diagnostics));
    REQUIRE(first.diagnostics.size() >= 2U);
    CHECK(cuexis::cxc::test::hasDiagnostic(first.diagnostics, "json.type.mismatch"));
    CHECK(cuexis::cxc::test::hasDiagnostic(first.diagnostics, "chart.uuid.invalid_v7"));
    CHECK(cuexis::cxc::test::hasDiagnostic(first.diagnostics, "cxc.project.invalid"));
    REQUIRE(first.diagnostics.items().size() == 4U);
    CHECK(first.diagnostics.items()[0].code() == "chart.uuid.invalid_v7");
    CHECK(first.diagnostics.items()[0].fieldPath() == "$/chartId");
    CHECK(first.diagnostics.items()[1].code() == "json.field.unknown");
    CHECK(first.diagnostics.items()[1].fieldPath() == "$/futureCoreField");
    CHECK(first.diagnostics.items()[2].code() == "cxc.project.invalid");
    CHECK(first.diagnostics.items()[2].fieldPath() == "$/project/entry/chart");
    CHECK(first.diagnostics.items()[3].code() == "json.type.mismatch");
    CHECK(first.diagnostics.items()[3].fieldPath() == "$/version");
}

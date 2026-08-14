#include <cuexis/content/content_provider.hpp>
#include <cuexis/core/error.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/playback/playback_source.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

[[nodiscard]] auto readText(const std::filesystem::path& path) -> std::string {
    std::ifstream stream{path, std::ios::binary};
    REQUIRE(stream.good());
    return {std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
}

[[nodiscard]] auto readBytes(const std::filesystem::path& path) -> std::vector<std::byte> {
    const auto text = readText(path);
    std::vector<std::byte> bytes;
    bytes.reserve(text.size());
    for (const unsigned char value : text) {
        bytes.push_back(static_cast<std::byte>(value));
    }
    return bytes;
}

[[nodiscard]] auto unusedProvider() -> std::shared_ptr<cuexis::content::IContentProvider> {
    auto provider = cuexis::content::HostContentProvider::create(
        [](const cuexis::content::ContentRequest&)
            -> cuexis::core::Result<cuexis::content::ContentBlob> {
            return cuexis::core::unexpected(cuexis::core::Error{
                "test.content.unexpected", "The source test provider must not be read"});
        });
    REQUIRE(provider.has_value());
    return *provider;
}

[[nodiscard]] auto staticV3Chart() -> std::string {
    return readText(std::filesystem::path{CUEXIS_SOURCE_DIR} / "tests" / "fixtures" /
                    "chart_format_update" / "valid" / "chart_v3_static_migration.json");
}

} // namespace

TEST_CASE("TypedPlaybackProjectSource owns documents and selects the exact entry path",
          "[playback][source][cfu-e1]") {
    cuexis::playback::TypedPlaybackProjectSource project{
        .sourceId = "typed-project-documents",
        .entryChartPath = "charts/main.cuexis.chart.json",
        .projectDocuments = {{.path = "templates/unused.cxt", .utf8Text = "{}"},
                             {.path = "charts/main.cuexis.chart.json",
                              .utf8Text = staticV3Chart()}},
        .assets = {},
    };

    auto source =
        cuexis::playback::PlaybackSource::fromTypedProjectSource(project, unusedProvider());
    REQUIRE(source.has_value());

    project.entryChartPath = "changed.cuexis.chart.json";
    project.projectDocuments.front().utf8Text = "not the source bytes";
    project.projectDocuments.back().utf8Text = "{}";

    cuexis::playback::PlaybackSession session;
    REQUIRE(
        session.load(std::move(*source), cuexis::playback::PlaybackMode::ChartClock).has_value());
    const auto info = session.contentInfo();
    REQUIRE(info.has_value());
    CHECK(info->chartFormatVersion == 3U);
}

TEST_CASE("TypedPlaybackProjectSource rejects invalid document tables",
          "[playback][source][cfu-e1]") {
    const auto provider = unusedProvider();

    SECTION("entry path must match exactly") {
        auto source = cuexis::playback::PlaybackSource::fromTypedProjectSource(
            {.sourceId = "missing-entry",
             .entryChartPath = "charts/main.cuexis.chart.json",
             .projectDocuments = {{.path = "Charts/main.cuexis.chart.json",
                                   .utf8Text = staticV3Chart()}},
             .assets = {}},
            provider);
        REQUIRE_FALSE(source.has_value());
        CHECK(source.error().code() == "playback.source.entry_document_missing");
    }

    SECTION("case-folded aliases conflict") {
        auto source = cuexis::playback::PlaybackSource::fromTypedProjectSource(
            {.sourceId = "case-conflict",
             .entryChartPath = "charts/main.cuexis.chart.json",
             .projectDocuments = {{.path = "charts/main.cuexis.chart.json",
                                   .utf8Text = staticV3Chart()},
                                  {.path = "Charts/MAIN.cuexis.chart.json", .utf8Text = "{}"}},
             .assets = {}},
            provider);
        REQUIRE_FALSE(source.has_value());
        CHECK(source.error().code() == "playback.source.document_path_conflict");
    }

    SECTION("parent traversal is not portable") {
        auto source = cuexis::playback::PlaybackSource::fromTypedProjectSource(
            {.sourceId = "parent-path",
             .entryChartPath = "../main.cuexis.chart.json",
             .projectDocuments = {{.path = "../main.cuexis.chart.json",
                                   .utf8Text = staticV3Chart()}},
             .assets = {}},
            provider);
        REQUIRE_FALSE(source.has_value());
        CHECK(source.error().code() == "playback.source.entry_path_invalid");
    }

    SECTION("document bytes must be valid UTF-8") {
        auto invalidUtf8 = staticV3Chart();
        invalidUtf8.push_back(static_cast<char>(0xC3));
        auto source = cuexis::playback::PlaybackSource::fromTypedProjectSource(
            {.sourceId = "invalid-utf8",
             .entryChartPath = "charts/main.cuexis.chart.json",
             .projectDocuments = {{.path = "charts/main.cuexis.chart.json",
                                   .utf8Text = std::move(invalidUtf8)}},
             .assets = {}},
            provider);
        REQUIRE_FALSE(source.has_value());
        CHECK(source.error().code() == "playback.source.document_utf8_invalid");
    }
}

TEST_CASE("PlaybackSource accepts owning CXC file and memory factories",
          "[playback][source][cxc][cfu-e1]") {
    const auto package = std::filesystem::path{CUEXIS_SOURCE_DIR} / "tests" / "fixtures" /
                         "chart_format_update" / "golden" / "cxc_v1_v4_cxt.cxc";

    auto fileSource = cuexis::playback::PlaybackSource::fromCxcFile(package);
    REQUIRE(fileSource.has_value());

    auto packageBytes = readBytes(package);
    auto memorySource = cuexis::playback::PlaybackSource::fromCxcMemory(std::move(packageBytes));
    REQUIRE(memorySource.has_value());

    auto invalid = cuexis::playback::PlaybackSource::fromCxcMemory({std::byte{0x43}});
    REQUIRE_FALSE(invalid.has_value());
    CHECK(invalid.error().code() == "playback.source.cxc_invalid");
}

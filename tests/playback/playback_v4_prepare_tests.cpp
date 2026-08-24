#include <cuexis/content/content_provider.hpp>
#include <cuexis/core/error.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/playback/playback_source.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace {

static_assert(!std::is_same_v<cuexis::playback::ChartParameterNumber,
                              cuexis::playback::ChartParameterWeight>);
static_assert(std::variant_size_v<cuexis::playback::ChartParameterValue> == 3);

[[nodiscard]] auto fixture(std::string_view relative) -> std::filesystem::path {
    return std::filesystem::path{CUEXIS_SOURCE_DIR} / "tests" / "fixtures" / "chart_format_update" /
           relative;
}

[[nodiscard]] auto readText(const std::filesystem::path& path) -> std::string {
    std::ifstream stream{path, std::ios::binary};
    if (!stream) {
        throw std::runtime_error{"Could not read CFU-E2 fixture: " + path.string()};
    }
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
                "test.content.unexpected", "The CFU-E2 provider must not be read"});
        });
    if (!provider) {
        throw std::runtime_error{"Could not create CFU-E2 content provider"};
    }
    return *provider;
}

[[nodiscard]] auto typedCxtSource() -> cuexis::core::Result<cuexis::playback::PlaybackSource> {
    return cuexis::playback::PlaybackSource::fromTypedProjectSource(
        {.sourceId = "cfu-e2-cxt",
         .entryChartPath = "charts/main.cuexis.chart.json",
         .projectDocuments = {{.path = "charts/main.cuexis.chart.json",
                               .utf8Text =
                                   readText(fixture("valid/chart_v4_cxt_template_binding.json"))},
                              {.path = "templates/move-y.cxt",
                               .utf8Text = readText(fixture("valid/templates/move-y.cxt"))}},
         .assets = {}},
        unusedProvider());
}

[[nodiscard]] auto hasDiagnostic(const cuexis::core::Diagnostics& diagnostics,
                                 std::string_view code) -> bool {
    return std::ranges::any_of(diagnostics.items(),
                               [code](const auto& item) { return item.code() == code; });
}

[[nodiscard]] auto diagnosticCapability(const cuexis::core::Diagnostics& diagnostics,
                                        std::string_view capability) -> bool {
    return std::ranges::any_of(diagnostics.items(), [&](const auto& item) {
        return item.code() == "playback.capability.unsupported" &&
               std::ranges::any_of(item.context(), [&](const auto& context) {
                   return context.key == "capability" && context.value == capability;
               });
    });
}

[[nodiscard]] auto overrideOptions(double x, double scaleY, double fov)
    -> cuexis::playback::PlaybackPrepareOptions {
    return {
        .parameters = {
            .values = {
                {.id = "layout.x", .value = cuexis::playback::ChartParameterNumber{x}},
                {.id = "layout.scale-y", .value = cuexis::playback::ChartParameterNumber{scaleY}},
                {.id = "camera.fov", .value = cuexis::playback::ChartParameterNumber{fov}}}}};
}

} // namespace

TEST_CASE("Playback resolves empty and parameterized Chart v4 into the existing Runtime",
          "[playback][v4][parameters][cfu-e2]") {
    const auto staticChart = readText(fixture("valid/chart_v4_static_migration.json"));
    cuexis::playback::PlaybackSession staticSession;
    auto staticPrepared =
        staticSession.prepareLoad(staticChart, cuexis::playback::PlaybackMode::ChartClock,
                                  cuexis::playback::PlaybackPrepareOptions{});
    REQUIRE(staticPrepared.has_value());
    REQUIRE(staticPrepared->contentInfo() != nullptr);
    CHECK(staticPrepared->contentInfo()->chartFormatVersion == 4U);
    REQUIRE(staticSession.commit(std::move(*staticPrepared)).has_value());

    const auto parameterized = readText(fixture("valid/chart_v4_parameterized_transform.json"));
    auto defaultSource = cuexis::playback::PlaybackSource::fromChartText(parameterized);
    REQUIRE(defaultSource.has_value());
    cuexis::playback::PlaybackSession defaultSession;
    REQUIRE(
        defaultSession.load(std::move(*defaultSource), cuexis::playback::PlaybackMode::ChartClock)
            .has_value());
    REQUIRE(defaultSession.update({.chartTimeMs = 0.0}).has_value());
    auto defaultFrame = defaultSession.extractFrame({.width = 640, .height = 480});
    REQUIRE(defaultFrame.has_value());
    REQUIRE(defaultFrame->objects.size() == 1);
    CHECK(defaultFrame->objects[0].worldMatrix[12] == Catch::Approx(2.0F));
    CHECK(defaultFrame->objects[0].worldMatrix[5] == Catch::Approx(1.0F));

    auto options = overrideOptions(-4.0, 2.0, 75.0);
    auto overrideSource = cuexis::playback::PlaybackSource::fromChartText(parameterized);
    REQUIRE(overrideSource.has_value());
    cuexis::playback::PlaybackSession overrideSession;
    auto prepared = overrideSession.prepareLoad(
        std::move(*overrideSource), cuexis::playback::PlaybackMode::ChartClock, options);
    REQUIRE(prepared.has_value());
    std::get<cuexis::playback::ChartParameterNumber>(options.parameters.values[0].value).value =
        99.0;
    REQUIRE(overrideSession.commit(std::move(*prepared)).has_value());
    REQUIRE(overrideSession.update({.chartTimeMs = 0.0}).has_value());
    auto frame = overrideSession.extractFrame({.width = 640, .height = 480});
    REQUIRE(frame.has_value());
    REQUIRE(frame->objects.size() == 1);
    CHECK(frame->objects[0].worldMatrix[12] == Catch::Approx(-4.0F));
    CHECK(frame->objects[0].worldMatrix[5] == Catch::Approx(2.0F));
}

TEST_CASE("Playback preserves parameter tags and reports resolver failures before capability",
          "[playback][v4][parameters][failure][cfu-e2]") {
    const auto parameterized = readText(fixture("valid/chart_v4_parameterized_transform.json"));

    SECTION("unknown") {
        cuexis::playback::PlaybackSession session;
        const cuexis::playback::PlaybackPrepareOptions options{
            .parameters = {.values = {{.id = "unknown",
                                       .value = cuexis::playback::ChartParameterNumber{1.0}}}}};
        CHECK_FALSE(
            session.prepareLoad(parameterized, cuexis::playback::PlaybackMode::ChartClock, options)
                .has_value());
        REQUIRE(session.lastOperationDiagnostics().has_value());
        CHECK(hasDiagnostic(*session.lastOperationDiagnostics(), "chart.parameter.unknown"));
    }

    SECTION("duplicate") {
        cuexis::playback::PlaybackSession session;
        const cuexis::playback::PlaybackPrepareOptions options{
            .parameters = {
                .values = {
                    {.id = "layout.x", .value = cuexis::playback::ChartParameterNumber{1.0}},
                    {.id = "layout.x", .value = cuexis::playback::ChartParameterNumber{2.0}}}}};
        CHECK_FALSE(
            session.prepareLoad(parameterized, cuexis::playback::PlaybackMode::ChartClock, options)
                .has_value());
        REQUIRE(session.lastOperationDiagnostics().has_value());
        CHECK(hasDiagnostic(*session.lastOperationDiagnostics(), "chart.parameter.duplicate"));
    }

    SECTION("type tag") {
        cuexis::playback::PlaybackSession session;
        const cuexis::playback::PlaybackPrepareOptions options{
            .parameters = {.values = {{.id = "layout.x",
                                       .value = cuexis::playback::ChartParameterWeight{0.5}}}}};
        CHECK_FALSE(
            session.prepareLoad(parameterized, cuexis::playback::PlaybackMode::ChartClock, options)
                .has_value());
        REQUIRE(session.lastOperationDiagnostics().has_value());
        CHECK(hasDiagnostic(*session.lastOperationDiagnostics(), "chart.parameter.type_mismatch"));
    }

    SECTION("number range") {
        cuexis::playback::PlaybackSession session;
        const cuexis::playback::PlaybackPrepareOptions options{
            .parameters = {.values = {{.id = "camera.fov",
                                       .value = cuexis::playback::ChartParameterNumber{180.0}}}}};
        CHECK_FALSE(
            session.prepareLoad(parameterized, cuexis::playback::PlaybackMode::ChartClock, options)
                .has_value());
        REQUIRE(session.lastOperationDiagnostics().has_value());
        CHECK(hasDiagnostic(*session.lastOperationDiagnostics(), "chart.parameter.out_of_range"));
    }

    SECTION("non-finite number") {
        cuexis::playback::PlaybackSession session;
        const cuexis::playback::PlaybackPrepareOptions options{
            .parameters = {.values = {{.id = "layout.x",
                                       .value = cuexis::playback::ChartParameterNumber{
                                           std::numeric_limits<double>::infinity()}}}}};
        CHECK_FALSE(
            session.prepareLoad(parameterized, cuexis::playback::PlaybackMode::ChartClock, options)
                .has_value());
        REQUIRE(session.lastOperationDiagnostics().has_value());
        CHECK(hasDiagnostic(*session.lastOperationDiagnostics(), "chart.parameter.out_of_range"));
    }

    SECTION("missing") {
        auto missing = parameterized;
        const auto defaultValue = missing.find("\"default\": 2.0,");
        REQUIRE(defaultValue != std::string::npos);
        missing.erase(defaultValue, std::string_view{"\"default\": 2.0,"}.size());
        cuexis::playback::PlaybackSession session;
        CHECK_FALSE(
            session.prepareLoad(missing, cuexis::playback::PlaybackMode::ChartClock).has_value());
        REQUIRE(session.lastOperationDiagnostics().has_value());
        CHECK(hasDiagnostic(*session.lastOperationDiagnostics(), "chart.parameter.missing"));
    }

    SECTION("legacy Chart rejects parameter input") {
        const auto legacy = readText(fixture("valid/chart_v3_static_migration.json"));
        cuexis::playback::PlaybackSession session;
        const cuexis::playback::PlaybackPrepareOptions options{
            .parameters = {.values = {{.id = "layout.x",
                                       .value = cuexis::playback::ChartParameterNumber{1.0}}}}};
        CHECK_FALSE(session.prepareLoad(legacy, cuexis::playback::PlaybackMode::ChartClock, options)
                        .has_value());
        REQUIRE(session.lastOperationDiagnostics().has_value());
        CHECK(hasDiagnostic(*session.lastOperationDiagnostics(), "chart.parameter.unknown"));
    }
}

TEST_CASE("Playback converts rational and weight parameters without collapsing their tags",
          "[playback][v4][parameters][cxt][cfu-e2]") {
    SECTION("valid tagged inputs reach animation capability preflight") {
        auto source = typedCxtSource();
        REQUIRE(source.has_value());
        cuexis::playback::PlaybackSession session;
        const cuexis::playback::PlaybackPrepareOptions options{
            .parameters = {.values = {{.id = "motion.duration-scale",
                                       .value = cuexis::playback::ChartParameterRational{2, 2}},
                                      {.id = "motion.weight",
                                       .value = cuexis::playback::ChartParameterWeight{0.5}}}}};
        const auto result = session.prepareLoad(
            std::move(*source), cuexis::playback::PlaybackMode::ChartClock, options);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code() == "playback.capability.preflight_failed");
        REQUIRE(session.lastOperationDiagnostics().has_value());
        CHECK_FALSE(
            hasDiagnostic(*session.lastOperationDiagnostics(), "chart.parameter.type_mismatch"));
        CHECK(
            diagnosticCapability(*session.lastOperationDiagnostics(), "cuexis.animation.clip.v1"));
    }

    SECTION("rational denominator") {
        auto source = typedCxtSource();
        REQUIRE(source.has_value());
        cuexis::playback::PlaybackSession session;
        const cuexis::playback::PlaybackPrepareOptions options{
            .parameters = {.values = {{.id = "motion.duration-scale",
                                       .value = cuexis::playback::ChartParameterRational{1, 0}}}}};
        CHECK_FALSE(session
                        .prepareLoad(std::move(*source), cuexis::playback::PlaybackMode::ChartClock,
                                     options)
                        .has_value());
        REQUIRE(session.lastOperationDiagnostics().has_value());
        CHECK(hasDiagnostic(*session.lastOperationDiagnostics(), "chart.parameter.out_of_range"));
    }

    SECTION("weight range") {
        auto source = typedCxtSource();
        REQUIRE(source.has_value());
        cuexis::playback::PlaybackSession session;
        const cuexis::playback::PlaybackPrepareOptions options{
            .parameters = {.values = {{.id = "motion.weight",
                                       .value = cuexis::playback::ChartParameterWeight{1.5}}}}};
        CHECK_FALSE(session
                        .prepareLoad(std::move(*source), cuexis::playback::PlaybackMode::ChartClock,
                                     options)
                        .has_value());
        REQUIRE(session.lastOperationDiagnostics().has_value());
        CHECK(hasDiagnostic(*session.lastOperationDiagnostics(), "chart.parameter.out_of_range"));
    }
}

TEST_CASE("Playback validates Chart v4 before stable Stage 4 capability rejection",
          "[playback][v4][capability][cfu-e2]") {
    const auto staticChart = readText(fixture("valid/chart_v4_static_migration.json"));
    cuexis::playback::PlaybackSession noCapabilities{
        cuexis::playback::PlaybackCapabilitySet{.version = 1, .ids = {}}};
    auto staticRejected =
        noCapabilities.prepareLoad(staticChart, cuexis::playback::PlaybackMode::ChartClock);
    REQUIRE_FALSE(staticRejected.has_value());
    REQUIRE(noCapabilities.lastOperationDiagnostics().has_value());
    CHECK(diagnosticCapability(*noCapabilities.lastOperationDiagnostics(), "cuexis.chart.v4"));

    const auto animation = readText(fixture("valid/chart_v4_animation.json"));
    cuexis::playback::PlaybackSession animationSession;
    auto animationRejected =
        animationSession.prepareLoad(animation, cuexis::playback::PlaybackMode::ChartClock);
    REQUIRE_FALSE(animationRejected.has_value());
    CHECK(animationRejected.error().code() == "playback.capability.preflight_failed");
    REQUIRE(animationSession.lastOperationDiagnostics().has_value());
    CHECK(diagnosticCapability(*animationSession.lastOperationDiagnostics(),
                               "cuexis.animation.clip.v1"));
    CHECK(diagnosticCapability(*animationSession.lastOperationDiagnostics(),
                               "cuexis.animation.layers.v1"));

    auto malformed = animation;
    const auto nearPlane = malformed.find("\"near\": 0.1");
    REQUIRE(nearPlane != std::string::npos);
    malformed.replace(nearPlane, std::string_view{"\"near\": 0.1"}.size(), "\"near\": 0.0");
    cuexis::playback::PlaybackSession malformedSession;
    auto malformedRejected =
        malformedSession.prepareLoad(malformed, cuexis::playback::PlaybackMode::ChartClock);
    REQUIRE_FALSE(malformedRejected.has_value());
    CHECK(malformedRejected.error().code() == "playback.chart.load_failed");
    REQUIRE(malformedSession.lastOperationDiagnostics().has_value());
    CHECK_FALSE(hasDiagnostic(*malformedSession.lastOperationDiagnostics(),
                              "playback.capability.unsupported"));
}

TEST_CASE("Playback resolves owning CXT sources before capability preflight",
          "[playback][v4][cxt][cxc][filesystem][cfu-e2]") {
    SECTION("typed") {
        auto source = typedCxtSource();
        REQUIRE(source.has_value());
        cuexis::playback::PlaybackSession session;
        CHECK_FALSE(
            session.prepareLoad(std::move(*source), cuexis::playback::PlaybackMode::ChartClock)
                .has_value());
        REQUIRE(session.lastOperationDiagnostics().has_value());
        CHECK_FALSE(hasDiagnostic(*session.lastOperationDiagnostics(), "cxt.import.missing"));
        CHECK(diagnosticCapability(*session.lastOperationDiagnostics(),
                                   "cuexis.animation.layers.v1"));
    }

    SECTION("filesystem") {
        auto source =
            cuexis::playback::PlaybackSource::fromFilesystemProject(fixture("source_project"));
        REQUIRE(source.has_value());
        cuexis::playback::PlaybackSession session;
        CHECK_FALSE(
            session.prepareLoad(std::move(*source), cuexis::playback::PlaybackMode::ChartClock)
                .has_value());
        REQUIRE(session.lastOperationDiagnostics().has_value());
        CHECK_FALSE(hasDiagnostic(*session.lastOperationDiagnostics(), "cxt.import.missing"));
        CHECK(
            diagnosticCapability(*session.lastOperationDiagnostics(), "cuexis.animation.clip.v1"));
    }

    SECTION("CXC source capability") {
        const auto package = fixture("golden/cxc_v1_v4_cxt.cxc");
        auto source = cuexis::playback::PlaybackSource::fromCxcMemory(readBytes(package));
        REQUIRE(source.has_value());
        cuexis::playback::PlaybackSession session{cuexis::playback::PlaybackCapabilitySet{
            .version = 1,
            .ids = {std::string{cuexis::playback::capabilityChartV4},
                    std::string{cuexis::playback::capabilitySourceCxtV1}}}};
        CHECK_FALSE(
            session.prepareLoad(std::move(*source), cuexis::playback::PlaybackMode::ChartClock)
                .has_value());
        REQUIRE(session.lastOperationDiagnostics().has_value());
        CHECK(diagnosticCapability(*session.lastOperationDiagnostics(),
                                   cuexis::playback::capabilitySourceCxcV1));
    }
}

TEST_CASE("Playback v4 failed reload preserves active content and options reload commits",
          "[playback][v4][reload][rollback][cfu-e2]") {
    const auto parameterized = readText(fixture("valid/chart_v4_parameterized_transform.json"));
    auto initialSource = cuexis::playback::PlaybackSource::fromChartText(parameterized);
    REQUIRE(initialSource.has_value());
    cuexis::playback::PlaybackSession session;
    REQUIRE(session
                .load(std::move(*initialSource), cuexis::playback::PlaybackMode::ChartClock,
                      overrideOptions(3.0, 1.0, 60.0))
                .has_value());
    REQUIRE(session.update({.chartTimeMs = 0.0}).has_value());
    const auto before = session.extractFrame({.width = 640, .height = 480});
    const auto infoBefore = session.contentInfo();
    REQUIRE(before.has_value());
    REQUIRE(infoBefore.has_value());

    const auto animation = readText(fixture("valid/chart_v4_animation.json"));
    CHECK_FALSE(session
                    .reload(animation, {.chartTimeMs = 0.0},
                            cuexis::playback::ReloadPolicy::KeepChartTime,
                            cuexis::playback::PlaybackPrepareOptions{})
                    .has_value());
    const auto afterFailure = session.extractFrame({.width = 640, .height = 480});
    const auto infoAfterFailure = session.contentInfo();
    REQUIRE(afterFailure.has_value());
    REQUIRE(infoAfterFailure.has_value());
    CHECK(afterFailure->objects[0].worldMatrix[12] ==
          Catch::Approx(before->objects[0].worldMatrix[12]));
    CHECK(infoAfterFailure->chartId == infoBefore->chartId);

    auto replacement = cuexis::playback::PlaybackSource::fromChartText(parameterized);
    REQUIRE(replacement.has_value());
    REQUIRE(session
                .reload(std::move(*replacement), {.chartTimeMs = 0.0},
                        cuexis::playback::ReloadPolicy::KeepChartTime,
                        overrideOptions(7.0, 1.0, 60.0))
                .has_value());
    const auto afterSuccess = session.extractFrame({.width = 640, .height = 480});
    REQUIRE(afterSuccess.has_value());
    CHECK(afterSuccess->objects[0].worldMatrix[12] == Catch::Approx(7.0F));
}

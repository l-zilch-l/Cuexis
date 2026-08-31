#include <cuexis/content/content_provider.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/playback/playback_source.hpp>

#include "parse_probe_internal.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace {

using cuexis::playback::FrameSnapshot;
using cuexis::playback::PlaybackAssetType;
using cuexis::playback::PlaybackContentInfo;
using cuexis::playback::PlaybackMode;
using cuexis::playback::PlaybackSession;
using cuexis::playback::PlaybackSource;
using cuexis::playback::ReloadPolicy;
using cuexis::playback::SessionState;

constexpr std::string_view simpleChart = R"json(
{
  "format":"cuexis.chart","version":1,
  "chartId":"019b0000-0000-7abc-8def-000000000201","metadata":{},
  "timing":{"offsetMs":0,"defaultBpm":120,"bpmChanges":[],"stops":[]},
  "camera":{"type":"perspective","fovY":60,"near":0.1,"far":1000,
            "pitch":0,"yaw":0,"roll":0,"defaultTransform":{"position":[0,0,-10]}},
  "templates":[],
  "behaviors":[{
    "id":"move","type":"behavior.transform.keyframe","version":1,
    "tracks":[
      {"property":"transform.position.x","keys":[
        {"beat":{"numerator":0,"denominator":1},"value":0},
        {"beat":{"numerator":1,"denominator":1},"value":10,"easing":"linear"}]},
      {"property":"camera.fovY","keys":[
        {"beat":{"numerator":0,"denominator":1},"value":60},
        {"beat":{"numerator":1,"denominator":1},"value":90,"easing":"linear"}]}
    ]
  }],
  "objects":[{
    "id":"019b0000-0000-7abc-8def-000000000210","name":"camera","parent":null,
    "components":{
      "cuexis.transform":{"version":1,"position":[0,0,-10],
                          "rotation":[0,0,0,1],"scale":[1,1,1]},
      "cuexis.camera":{"version":1,"type":"perspective","fovY":60,
                       "near":0.1,"far":1000},
      "cuexis.behavior":{"version":1,"behavior":{"domain":"behavior","id":"move"}}
    },"extensions":{}
  }],
  "requiredExtensions":[],"extensions":{}
}
)json";

constexpr std::string_view v4LegacyRenderableChart = R"json(
{
  "format":"cuexis.chart","version":4,
  "chartId":"019f0000-0000-7abc-8def-000000000451","metadata":{},
  "timing":{"offsetMs":0,"defaultBpm":120,"tempoEvents":[],"stops":[]},
  "camera":{"type":"perspective","fovY":60,"near":0.1,"far":1000},
  "parameters":[],"templates":[],"behaviors":[],
  "animationTemplateImports":[],"animationClips":[],
  "objects":[{
    "id":"019f0000-0000-7abc-8def-000000000461","name":"legacy_renderable",
    "parent":null,
    "components":{
      "cuexis.transform":{"version":1,"position":[0,0,0],
                           "rotation":[0,0,0,1],"scale":[1,1,1]},
      "cuexis.renderable":{"version":1,
                            "mesh":{"domain":"asset","id":"mesh.legacy"},
                            "material":{"domain":"asset","id":"material.legacy"}}
    },
    "extensions":{}
  }],
  "requiredExtensions":[],"extensions":{}
}
)json";

struct ActiveBaseline final {
    SessionState state{};
    PlaybackContentInfo content;
    cuexis::playback::PreparedSemanticIdentity identity;
    FrameSnapshot frame;
};

[[nodiscard]] auto sourceRoot() -> std::filesystem::path {
    return std::filesystem::path{CUEXIS_SOURCE_DIR};
}

[[nodiscard]] auto readFile(const std::filesystem::path& path) -> std::string {
    std::ifstream input{path, std::ios::binary};
    REQUIRE(input.good());
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

[[nodiscard]] auto readBytes(const std::filesystem::path& path) -> std::vector<std::byte> {
    const auto text = readFile(path);
    std::vector<std::byte> bytes;
    bytes.reserve(text.size());
    for (const unsigned char value : text) {
        bytes.push_back(static_cast<std::byte>(value));
    }
    return bytes;
}

[[nodiscard]] auto hasDiagnostic(const cuexis::core::Diagnostics& diagnostics,
                                 std::string_view code) -> bool {
    return std::ranges::any_of(diagnostics.items(),
                               [code](const auto& item) { return item.code() == code; });
}

void writeU32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value) {
    REQUIRE(offset + 4 <= bytes.size());
    for (std::size_t index = 0; index < 4; ++index) {
        bytes[offset + index] = static_cast<std::byte>((value >> (index * 8U)) & 0xFFU);
    }
}

[[nodiscard]] auto captureBaseline(PlaybackSession& session) -> ActiveBaseline {
    const auto state = session.state();
    const auto content = session.contentInfo();
    const auto identity = session.semanticIdentity();
    const auto frame = session.extractFrame({.width = 640, .height = 480});
    REQUIRE(state.has_value());
    REQUIRE(content.has_value());
    REQUIRE(identity.has_value());
    REQUIRE(frame.has_value());
    return {*state, *content, *identity, *frame};
}

void checkRollback(PlaybackSession& session, const ActiveBaseline& baseline) {
    const auto state = session.state();
    const auto content = session.contentInfo();
    const auto identity = session.semanticIdentity();
    const auto frame = session.extractFrame({.width = 640, .height = 480});
    REQUIRE(state.has_value());
    REQUIRE(content.has_value());
    REQUIRE(identity.has_value());
    REQUIRE(frame.has_value());
    CHECK(*state == baseline.state);
    CHECK(content->chartId == baseline.content.chartId);
    CHECK(content->mode == baseline.content.mode);
    CHECK(*identity == baseline.identity);
    REQUIRE(frame->objects.size() == baseline.frame.objects.size());
    for (std::size_t index = 0; index < frame->objects.size(); ++index) {
        CHECK(frame->objects[index].id == baseline.frame.objects[index].id);
        CHECK(frame->objects[index].worldMatrix[12] ==
              baseline.frame.objects[index].worldMatrix[12]);
        CHECK(frame->objects[index].materialAssetId ==
              baseline.frame.objects[index].materialAssetId);
    }
    CHECK(frame->camera.fovY == baseline.frame.camera.fovY);
}

[[nodiscard]] auto stage1dSource(std::shared_ptr<cuexis::content::IContentProvider> provider)
    -> cuexis::core::Result<PlaybackSource> {
    const auto root = sourceRoot() / "assets" / "projects" / "stage1d_project";
    return PlaybackSource::fromTypedProject(
        {.sourceId = "pb03-audio-provider-failure",
         .chartJson = readFile(root / "assets" / "charts" / "stage1d_example.cuexis.chart.json"),
         .assets = {{.id = "audio.main",
                     .type = PlaybackAssetType::Audio,
                     .rootId = "main",
                     .logicalSource = "audio/main.wav"}}},
        std::move(provider));
}

[[nodiscard]] auto stage3Source(std::vector<std::byte> meshBytes)
    -> cuexis::core::Result<PlaybackSource> {
    const auto root = sourceRoot() / "assets" / "projects" / "stage3_project" / "assets";
    std::vector<cuexis::content::MemoryContentEntry> entries;
    entries.push_back({.rootId = "main",
                       .source = "meshes/triangle.mesh.bin",
                       .bytes = std::move(meshBytes),
                       .revision = 7});
    entries.push_back({.rootId = "main",
                       .source = "materials/opaque.material.bin",
                       .bytes = readBytes(root / "materials" / "opaque.material.bin"),
                       .revision = 7});
    entries.push_back({.rootId = "main",
                       .source = "materials/blend.material.bin",
                       .bytes = readBytes(root / "materials" / "blend.material.bin"),
                       .revision = 7});
    entries.push_back({.rootId = "main",
                       .source = "textures/checker.texture.bin",
                       .bytes = readBytes(root / "textures" / "checker.texture.bin"),
                       .revision = 7});
    auto provider = cuexis::content::MemoryContentProvider::create(std::move(entries));
    if (!provider) {
        return cuexis::core::unexpected(std::move(provider.error()));
    }
    return PlaybackSource::fromTypedProject(
        {.sourceId = "pb03-presentation-failure",
         .chartJson = readFile(root / "charts" / "stage3_example.cuexis.chart.json"),
         .assets = {{.id = "texture.checker",
                     .type = PlaybackAssetType::Texture,
                     .rootId = "main",
                     .logicalSource = "textures/checker.texture.bin"},
                    {.id = "material.blend",
                     .type = PlaybackAssetType::Material,
                     .rootId = "main",
                     .logicalSource = "materials/blend.material.bin",
                     .dependencies = {"texture.checker"}},
                    {.id = "mesh.triangle",
                     .type = PlaybackAssetType::Mesh,
                     .rootId = "main",
                     .logicalSource = "meshes/triangle.mesh.bin"},
                    {.id = "material.opaque",
                     .type = PlaybackAssetType::Material,
                     .rootId = "main",
                     .logicalSource = "materials/opaque.material.bin"}}},
        std::move(*provider));
}

[[nodiscard]] auto
stage3SourceWithProvider(std::shared_ptr<cuexis::content::IContentProvider> provider)
    -> cuexis::core::Result<PlaybackSource> {
    const auto root = sourceRoot() / "assets" / "projects" / "stage3_project" / "assets";
    return PlaybackSource::fromTypedProject(
        {.sourceId = "pb03-provider-failure",
         .chartJson = readFile(root / "charts" / "stage3_example.cuexis.chart.json"),
         .assets = {{.id = "texture.checker",
                     .type = PlaybackAssetType::Texture,
                     .rootId = "main",
                     .logicalSource = "textures/checker.texture.bin"},
                    {.id = "material.blend",
                     .type = PlaybackAssetType::Material,
                     .rootId = "main",
                     .logicalSource = "materials/blend.material.bin",
                     .dependencies = {"texture.checker"}},
                    {.id = "mesh.triangle",
                     .type = PlaybackAssetType::Mesh,
                     .rootId = "main",
                     .logicalSource = "meshes/triangle.mesh.bin"},
                    {.id = "material.opaque",
                     .type = PlaybackAssetType::Material,
                     .rootId = "main",
                     .logicalSource = "materials/opaque.material.bin"}}},
        std::move(provider));
}

[[nodiscard]] auto v4LegacyPresentationSource() -> cuexis::core::Result<PlaybackSource> {
    const auto root = sourceRoot() / "tests" / "fixtures" / "stage1b_project" / "assets";
    auto provider = cuexis::content::MemoryContentProvider::create(
        {{.rootId = "main",
          .source = "meshes/note.mesh.bin",
          .bytes = readBytes(root / "meshes" / "note.mesh.bin")},
         {.rootId = "main",
          .source = "materials/basic.material.bin",
          .bytes = readBytes(root / "materials" / "basic.material.bin")}});
    if (!provider) {
        return cuexis::core::unexpected(std::move(provider.error()));
    }
    return PlaybackSource::fromTypedProject(
        {.sourceId = "pb01-legacy-presentation",
         .chartJson = std::string{v4LegacyRenderableChart},
         .assets = {{.id = "mesh.legacy",
                     .type = PlaybackAssetType::Mesh,
                     .rootId = "main",
                     .logicalSource = "meshes/note.mesh.bin"},
                    {.id = "material.legacy",
                     .type = PlaybackAssetType::Material,
                     .rootId = "main",
                     .logicalSource = "materials/basic.material.bin"}}},
        std::move(*provider));
}

void checkDiagnosticOrder(const cuexis::core::Diagnostics& diagnostics) {
    for (std::size_t index = 1; index < diagnostics.items().size(); ++index) {
        const auto& previous = diagnostics.items()[index - 1];
        const auto& current = diagnostics.items()[index];
        const auto previousKey =
            std::tuple{previous.fieldPath(), static_cast<int>(previous.severity()), previous.code(),
                       previous.message()};
        const auto currentKey =
            std::tuple{current.fieldPath(), static_cast<int>(current.severity()), current.code(),
                       current.message()};
        CHECK(previousKey <= currentKey);
    }
}

} // namespace

TEST_CASE("PB-03 source and chart failures retain active candidate and diagnostics",
          "[playback][prepare][characterization][pb-03]") {
    PlaybackSession session;
    REQUIRE(session.loadChart(simpleChart).has_value());
    REQUIRE(session.update({.chartTimeMs = 250.0}).has_value());
    const auto baseline = captureBaseline(session);

    SECTION("empty source records a source diagnostic") {
        PlaybackSource empty;
        const auto failed = session.prepareReload(std::move(empty), {.chartTimeMs = 250.0},
                                                  ReloadPolicy::KeepChartTime);
        REQUIRE_FALSE(failed.has_value());
        CHECK(failed.error().code() == "playback.source.invalid");
        const auto diagnostics = session.lastOperationDiagnostics();
        REQUIRE(diagnostics.has_value());
        REQUIRE(diagnostics->size() == 1);
        CHECK(diagnostics->items().front().code() == "playback.source.invalid");
        checkRollback(session, baseline);
    }

    SECTION("runtime compile failure records cause and path") {
        auto invalid = std::string{simpleChart};
        const auto marker = std::string_view{"\"domain\":\"behavior\",\"id\":\"move\""};
        const auto position = invalid.find(marker);
        REQUIRE(position != std::string::npos);
        invalid.replace(position, marker.size(), "\"domain\":\"behavior\",\"id\":\"missing\"");

        const auto failed =
            session.prepareReload(invalid, {.chartTimeMs = 250.0}, ReloadPolicy::KeepChartTime);
        REQUIRE_FALSE(failed.has_value());
        CHECK(failed.error().code() == "playback.chart.reload_load_failed");
        const auto diagnostics = session.lastOperationDiagnostics();
        REQUIRE(diagnostics.has_value());
        REQUIRE(hasDiagnostic(*diagnostics, "chart.reference.behavior_missing"));
        checkDiagnosticOrder(*diagnostics);
        checkRollback(session, baseline);
    }

    SECTION("preparing a load while active records the lifecycle failure") {
        const auto failed = session.prepareLoad(simpleChart, PlaybackMode::ChartClock);
        REQUIRE_FALSE(failed.has_value());
        CHECK(failed.error().code() == "playback.session.not_empty");
        const auto diagnostics = session.lastOperationDiagnostics();
        REQUIRE(diagnostics.has_value());
        REQUIRE(diagnostics->size() == 1);
        CHECK(diagnostics->items().front().code() == "playback.session.not_empty");
        checkDiagnosticOrder(*diagnostics);
        checkRollback(session, baseline);
    }
}

TEST_CASE("PB-03 prepare boundary failures publish one operation diagnostic",
          "[playback][prepare][characterization][pb-03]") {
    SECTION("reload before the first load records not-ready") {
        PlaybackSession session;
        const auto failed =
            session.prepareReload(simpleChart, {.chartTimeMs = 0.0}, ReloadPolicy::KeepChartTime);
        REQUIRE_FALSE(failed.has_value());
        CHECK(failed.error().code() == "playback.session.not_ready");
        const auto diagnostics = session.lastOperationDiagnostics();
        REQUIRE(diagnostics.has_value());
        REQUIRE(diagnostics->size() == 1);
        CHECK(diagnostics->items().front().code() == "playback.session.not_ready");
        checkDiagnosticOrder(*diagnostics);
        REQUIRE(session.state().has_value());
        CHECK(*session.state() == SessionState::Empty);
    }

    SECTION("mode and content mismatch records the stable error") {
        auto provider = cuexis::content::HostContentProvider::create(
            [](const cuexis::content::ContentRequest&)
                -> cuexis::core::Result<cuexis::content::ContentBlob> {
                return cuexis::content::ContentBlob{};
            });
        REQUIRE(provider.has_value());
        auto source = stage1dSource(std::move(*provider));
        REQUIRE(source.has_value());

        PlaybackSession session;
        const auto failed = session.prepareLoad(std::move(*source), PlaybackMode::ChartClock);
        REQUIRE_FALSE(failed.has_value());
        CHECK(failed.error().code() == "playback.mode.content_mismatch");
        const auto diagnostics = session.lastOperationDiagnostics();
        REQUIRE(diagnostics.has_value());
        REQUIRE(diagnostics->size() == 1);
        CHECK(diagnostics->items().front().code() == "playback.mode.content_mismatch");
        checkDiagnosticOrder(*diagnostics);
        REQUIRE(session.state().has_value());
        CHECK(*session.state() == SessionState::Empty);
    }
}

TEST_CASE("PB-03 nested prepare reentry is isolated from the outer operation",
          "[playback][prepare][characterization][pb-03][reentry]") {
    const auto projectRoot = sourceRoot() / "assets" / "projects" / "stage1d_project";
    const auto chartText =
        readFile(projectRoot / "assets" / "charts" / "stage1d_example.cuexis.chart.json");
    const auto audioBytes = readBytes(projectRoot / "assets" / "audio" / "main.wav");

    PlaybackSession session;
    auto initialProvider = cuexis::content::MemoryContentProvider::create(
        {{.rootId = "main", .source = "audio/main.wav", .bytes = audioBytes, .revision = 1}});
    REQUIRE(initialProvider.has_value());
    auto initial = stage1dSource(std::move(*initialProvider));
    REQUIRE(initial.has_value());
    REQUIRE(session.load(std::move(*initial), PlaybackMode::HostClock).has_value());
    std::vector<std::string> nestedErrors;
    auto provider = cuexis::content::HostContentProvider::create(
        [&](const cuexis::content::ContentRequest&)
            -> cuexis::core::Result<cuexis::content::ContentBlob> {
            const auto nested = session.prepareLoad(simpleChart, PlaybackMode::ChartClock);
            CHECK_FALSE(nested.has_value());
            if (!nested) {
                nestedErrors.emplace_back(nested.error().code());
            }
            return cuexis::content::ContentBlob{.bytes = audioBytes, .revision = 2};
        });
    REQUIRE(provider.has_value());
    auto replacement = cuexis::playback::PlaybackSource::fromTypedProject(
        {.sourceId = "pb03-prepare-reentry",
         .chartJson = chartText,
         .assets = {{.id = "audio.main",
                     .type = PlaybackAssetType::Audio,
                     .rootId = "main",
                     .logicalSource = "audio/main.wav"}}},
        std::move(*provider));
    REQUIRE(replacement.has_value());

    const auto prepared = session.prepareReload(std::move(*replacement), {.chartTimeMs = 0.0},
                                                ReloadPolicy::KeepChartTime);
    REQUIRE(prepared.has_value());
    REQUIRE_FALSE(nestedErrors.empty());
    for (const auto& error : nestedErrors) {
        CHECK(error == "playback.session.reentrant");
    }
    const auto diagnostics = session.lastOperationDiagnostics();
    REQUIRE(diagnostics.has_value());
    CHECK(diagnostics->empty());
}

TEST_CASE("PB-03 successful prepare replaces stale diagnostics and preserves warnings",
          "[playback][prepare][characterization][pb-03]") {
    constexpr std::string_view chartWithWarning = R"json(
{
  "format":"cuexis.chart","version":1,
  "chartId":"019b0000-0000-7abc-8def-000000000299","metadata":{},
  "timing":{"offsetMs":0,"defaultBpm":120,"bpmChanges":[],"stops":[]},
  "templates":[],"behaviors":[],"objects":[],"requiredExtensions":[],
  "extensions":{"org.example.optional":{"version":1,"data":{"x":1}}}
}
)json";

    PlaybackSession session;
    const auto failed =
        session.prepareReload(simpleChart, {.chartTimeMs = 0.0}, ReloadPolicy::KeepChartTime);
    REQUIRE_FALSE(failed.has_value());
    CHECK(failed.error().code() == "playback.session.not_ready");
    REQUIRE(session.lastOperationDiagnostics().has_value());
    REQUIRE_FALSE(session.lastOperationDiagnostics()->empty());

    const auto prepared = session.prepareLoad(chartWithWarning, PlaybackMode::ChartClock);
    REQUIRE(prepared.has_value());
    const auto diagnostics = session.lastOperationDiagnostics();
    REQUIRE(diagnostics.has_value());
    REQUIRE(diagnostics->size() == 1);
    CHECK(diagnostics->items().front().code() == "chart.extension.optional_unknown");
    checkDiagnosticOrder(*diagnostics);
}

TEST_CASE("PB-03 provider and presentation failures roll back without stale active data",
          "[playback][prepare][characterization][pb-03]") {
    PlaybackSession session;
    REQUIRE(session.loadChart(simpleChart).has_value());
    REQUIRE(session.update({.chartTimeMs = 125.0}).has_value());
    const auto baseline = captureBaseline(session);

    SECTION("provider failure preserves active session") {
        auto provider = cuexis::content::HostContentProvider::create(
            [](const cuexis::content::ContentRequest&)
                -> cuexis::core::Result<cuexis::content::ContentBlob> {
                return cuexis::core::unexpected(cuexis::core::Error{
                    "test.provider.failed", "Characterization provider failure"});
            });
        REQUIRE(provider.has_value());
        auto source = stage3SourceWithProvider(std::move(*provider));
        REQUIRE(source.has_value());

        const auto failed = session.prepareReload(std::move(*source), {.chartTimeMs = 125.0},
                                                  ReloadPolicy::KeepChartTime);
        REQUIRE_FALSE(failed.has_value());
        CHECK(failed.error().code() == "playback.session.prepare_failed");
        const auto diagnostics = session.lastOperationDiagnostics();
        REQUIRE(diagnostics.has_value());
        REQUIRE(hasDiagnostic(*diagnostics, "assets.resource.required_failed"));
        checkDiagnosticOrder(*diagnostics);
        checkRollback(session, baseline);
    }

    SECTION("provider exception preserves active session") {
        auto provider = cuexis::content::HostContentProvider::create(
            [](const cuexis::content::ContentRequest&)
                -> cuexis::core::Result<cuexis::content::ContentBlob> {
                throw std::runtime_error{"Characterization provider exception"};
            });
        REQUIRE(provider.has_value());
        auto source = stage3SourceWithProvider(std::move(*provider));
        REQUIRE(source.has_value());

        const auto failed = session.prepareReload(std::move(*source), {.chartTimeMs = 125.0},
                                                  ReloadPolicy::KeepChartTime);
        REQUIRE_FALSE(failed.has_value());
        CHECK(failed.error().code() == "playback.session.prepare_failed");
        const auto diagnostics = session.lastOperationDiagnostics();
        REQUIRE(diagnostics.has_value());
        REQUIRE(hasDiagnostic(*diagnostics, "assets.resource.required_failed"));
        checkDiagnosticOrder(*diagnostics);
        checkRollback(session, baseline);
    }

    SECTION("portable presentation parse failure preserves active session") {
        const auto root = sourceRoot() / "assets" / "projects" / "stage3_project" / "assets";
        auto bytes = readBytes(root / "meshes" / "triangle.mesh.bin");
        writeU32(bytes, 100, 3);
        auto source = stage3Source(std::move(bytes));
        REQUIRE(source.has_value());

        const auto failed = session.prepareReload(std::move(*source), {.chartTimeMs = 125.0},
                                                  ReloadPolicy::KeepChartTime);
        REQUIRE_FALSE(failed.has_value());
        CHECK(failed.error().code() == "playback.presentation.mesh.index_out_of_range");
        const auto diagnostics = session.lastOperationDiagnostics();
        REQUIRE(diagnostics.has_value());
        REQUIRE(hasDiagnostic(*diagnostics, "playback.presentation.mesh.index_out_of_range"));
        checkDiagnosticOrder(*diagnostics);
        checkRollback(session, baseline);
    }
}

TEST_CASE("PB-03 capability preflight and identity failures are deterministic",
          "[playback][prepare][characterization][pb-03]") {
    SECTION("capability preflight fails before provider reads") {
        const auto stage2Chart =
            readFile(sourceRoot() / "assets" / "charts" / "stage2_example.cuexis.chart.json");
        PlaybackSession session{cuexis::playback::PlaybackCapabilitySet{.version = 1, .ids = {}}};
        REQUIRE(session.loadChart(simpleChart).has_value());
        REQUIRE(session.update({.chartTimeMs = 0.0}).has_value());
        const auto baseline = captureBaseline(session);

        const auto failed =
            session.prepareReload(stage2Chart, {.chartTimeMs = 0.0}, ReloadPolicy::KeepChartTime);
        REQUIRE_FALSE(failed.has_value());
        CHECK(failed.error().code() == "playback.capability.preflight_failed");
        const auto diagnostics = session.lastOperationDiagnostics();
        REQUIRE(diagnostics.has_value());
        REQUIRE_FALSE(diagnostics->empty());
        CHECK(hasDiagnostic(*diagnostics, "playback.capability.unsupported"));
        checkDiagnosticOrder(*diagnostics);
        checkRollback(session, baseline);
    }
}

TEST_CASE("PB-01 Chart v4 legacy renderables are rejected before identity assembly",
          "[playback][prepare][pb-01]") {
    PlaybackSession session;
    REQUIRE(session.loadChart(simpleChart).has_value());
    REQUIRE(session.update({.chartTimeMs = 125.0}).has_value());
    const auto baseline = captureBaseline(session);
    const auto activeDiagnostics = session.diagnostics();
    REQUIRE(activeDiagnostics.has_value());
    CHECK(activeDiagnostics->empty());

    auto source = v4LegacyPresentationSource();
    REQUIRE(source.has_value());
    const auto failed = session.prepareReload(std::move(*source), {.chartTimeMs = 125.0},
                                              ReloadPolicy::KeepChartTime);
    REQUIRE_FALSE(failed.has_value());
    CHECK(failed.error().code() == "playback.chart.v4.requires_portable_presentation");
    CHECK(failed.error().message() ==
          "Chart v4 renderable requires a portable CXPRES01 presentation payload");
    REQUIRE(failed.error().context().size() == 4);
    CHECK(failed.error().context()[0].key == "asset_id");
    CHECK(failed.error().context()[0].value == "material.legacy");
    CHECK(failed.error().context()[1].key == "resource_type");
    CHECK(failed.error().context()[1].value == "unlit_material");
    CHECK(failed.error().context()[2].key == "field_path");
    CHECK(failed.error().context()[2].value == "$/resources/material.legacy");
    CHECK(failed.error().context()[3].key == "object_id");
    CHECK(failed.error().context()[3].value == "019f0000-0000-7abc-8def-000000000461");

    const auto diagnostics = session.lastOperationDiagnostics();
    REQUIRE(diagnostics.has_value());
    REQUIRE(diagnostics->size() == 1);
    CHECK(diagnostics->items()[0].code() == "playback.chart.v4.requires_portable_presentation");
    CHECK(diagnostics->items()[0].message() ==
          "Chart v4 renderable requires a portable CXPRES01 presentation payload");
    CHECK(diagnostics->items()[0].fieldPath() == "$/resources/material.legacy");
    REQUIRE(diagnostics->items()[0].context().size() == 4);
    CHECK(diagnostics->items()[0].context()[0].key == "asset_id");
    CHECK(diagnostics->items()[0].context()[0].value == "material.legacy");
    CHECK(diagnostics->items()[0].context()[1].key == "resource_type");
    CHECK(diagnostics->items()[0].context()[1].value == "unlit_material");
    CHECK(diagnostics->items()[0].context()[2].key == "field_path");
    CHECK(diagnostics->items()[0].context()[2].value == "$/resources/material.legacy");
    CHECK(diagnostics->items()[0].context()[3].key == "object_id");
    CHECK(diagnostics->items()[0].context()[3].value == "019f0000-0000-7abc-8def-000000000461");
    checkDiagnosticOrder(*diagnostics);

    const auto activeAfterFailure = session.diagnostics();
    REQUIRE(activeAfterFailure.has_value());
    CHECK(activeAfterFailure->empty());
    checkRollback(session, baseline);
}

TEST_CASE("PB-03 stale commit failure records operation diagnostics and keeps active frame",
          "[playback][prepare][commit][characterization][pb-03]") {
    PlaybackSession session;
    REQUIRE(session.loadChart(simpleChart).has_value());
    REQUIRE(session.update({.chartTimeMs = 100.0}).has_value());
    auto prepared =
        session.prepareReload(simpleChart, {.chartTimeMs = 200.0}, ReloadPolicy::KeepChartTime);
    REQUIRE(prepared.has_value());

    REQUIRE(session.update({.chartTimeMs = 300.0}).has_value());
    const auto beforeCommit = captureBaseline(session);
    const auto failed = session.commit(std::move(*prepared));
    REQUIRE_FALSE(failed.has_value());
    CHECK(failed.error().code() == "playback.prepared.stale");
    const auto diagnostics = session.lastOperationDiagnostics();
    REQUIRE(diagnostics.has_value());
    REQUIRE(diagnostics->size() == 1);
    CHECK(diagnostics->items().front().code() == "playback.prepared.stale");
    checkRollback(session, beforeCommit);
}

TEST_CASE("PB-03 wrong-session commit failure records operation diagnostics",
          "[playback][prepare][commit][characterization][pb-03]") {
    PlaybackSession sourceSession;
    auto prepared = sourceSession.prepareLoad(simpleChart, PlaybackMode::ChartClock);
    REQUIRE(prepared.has_value());

    PlaybackSession targetSession;
    const auto failed = targetSession.commit(std::move(*prepared));
    REQUIRE_FALSE(failed.has_value());
    CHECK(failed.error().code() == "playback.prepared.wrong_session");

    const auto diagnostics = targetSession.lastOperationDiagnostics();
    REQUIRE(diagnostics.has_value());
    REQUIRE(diagnostics->size() == 1);
    CHECK(diagnostics->items().front().code() == "playback.prepared.wrong_session");
    REQUIRE(targetSession.state().has_value());
    CHECK(*targetSession.state() == SessionState::Empty);
}

TEST_CASE("Playback v4 prepare reuses the initial Chart parse",
          "[playback][prepare][parse][characterization][count]") {
    const auto chart = readFile(sourceRoot() / "tests" / "fixtures" / "chart_format_update" /
                                "valid" / "chart_v4_static_migration.json");
    cuexis::json::detail::ScopedParseCounter counter;
    PlaybackSession session;
    const auto prepared = session.prepareLoad(chart, PlaybackMode::ChartClock);
    REQUIRE(prepared.has_value());
    CHECK(counter.count() == 1U);
}

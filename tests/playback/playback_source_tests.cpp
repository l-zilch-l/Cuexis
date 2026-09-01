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
#include <string_view>
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

[[nodiscard]] auto typedSource(std::string sourceId, std::string entryPath,
                               std::vector<cuexis::playback::PlaybackProjectDocument> documents)
    -> cuexis::core::Result<cuexis::playback::PlaybackSource> {
    return cuexis::playback::PlaybackSource::fromTypedProjectSource(
        {.sourceId = std::move(sourceId),
         .entryChartPath = std::move(entryPath),
         .projectDocuments = std::move(documents),
         .assets = {}},
        unusedProvider());
}

[[nodiscard]] auto document(std::string path, std::string text = "{}")
    -> cuexis::playback::PlaybackProjectDocument {
    return {.path = std::move(path), .utf8Text = std::move(text)};
}

[[nodiscard]] auto errorContext(const cuexis::core::Error& error, std::string_view key)
    -> std::string_view {
    for (const auto& context : error.context()) {
        if (context.key == key) {
            return context.value;
        }
    }
    return {};
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

TEST_CASE("TypedPlaybackProjectSource validates portable source IDs and document paths",
          "[playback][source][validation][branch-coverage]") {
    SECTION("portable source IDs preserve accepted punctuation after the first character") {
        const std::string_view validIds[] = {"", "a", "A9", "source.id_1-2/part"};
        for (const auto id : validIds) {
            CAPTURE(id);
            auto source = typedSource(std::string{id}, "charts/main.cuexis.chart.json",
                                      {document("charts/main.cuexis.chart.json", staticV3Chart())});
            REQUIRE(source.has_value());
        }
    }

    SECTION("source IDs reject invalid first and later bytes plus the length boundary") {
        const std::string_view invalidIds[] = {"-source", "_source",      ".source",
                                               "/source", "source space", "source\\backslash"};
        for (const auto id : invalidIds) {
            CAPTURE(id);
            auto source = typedSource(std::string{id}, "charts/main.cuexis.chart.json",
                                      {document("charts/main.cuexis.chart.json", staticV3Chart())});
            REQUIRE_FALSE(source.has_value());
            CHECK(source.error().code() == "playback.source.id_invalid");
        }

        std::string tooLong(257U, 'a');
        auto source = typedSource(std::move(tooLong), "charts/main.cuexis.chart.json",
                                  {document("charts/main.cuexis.chart.json", staticV3Chart())});
        REQUIRE_FALSE(source.has_value());
        CHECK(source.error().code() == "playback.source.id_invalid");
    }

    SECTION("paths reject every nonportable segment form") {
        const std::vector<std::string> invalidPaths = {"",
                                                       "/absolute.cuexis.chart.json",
                                                       "trailing/",
                                                       "double//separator",
                                                       ".",
                                                       "./chart.cuexis.chart.json",
                                                       "../chart.cuexis.chart.json",
                                                       "folder/.",
                                                       "folder/..",
                                                       "folder/name.",
                                                       "folder/name ",
                                                       "CON/chart.cuexis.chart.json",
                                                       "folder/has space.cxt",
                                                       "folder/has\\backslash.cxt"};
        for (const auto& path : invalidPaths) {
            CAPTURE(path);
            auto source = typedSource("portable-source", "charts/main.cuexis.chart.json",
                                      {document(path, staticV3Chart())});
            REQUIRE_FALSE(source.has_value());
            CHECK(source.error().code() == "playback.source.document_path_invalid");
        }

        std::string deepPath;
        for (int index = 0; index != 65; ++index) {
            if (!deepPath.empty()) {
                deepPath.push_back('/');
            }
            deepPath.push_back('a');
        }
        auto tooDeep = typedSource("portable-source", "charts/main.cuexis.chart.json",
                                   {document(std::move(deepPath), staticV3Chart())});
        REQUIRE_FALSE(tooDeep.has_value());
        CHECK(tooDeep.error().code() == "playback.source.document_path_invalid");
    }

    SECTION("document paths reject both ancestor and descendant conflicts") {
        const auto chart = staticV3Chart();
        auto ancestor = typedSource(
            "portable-source", "charts/main.cuexis.chart.json",
            {document("charts", "{}"), document("charts/main.cuexis.chart.json", chart)});
        REQUIRE_FALSE(ancestor.has_value());
        CHECK(ancestor.error().code() == "playback.source.document_path_conflict");

        auto descendant = typedSource(
            "portable-source", "charts",
            {document("charts", chart), document("charts/main.cuexis.chart.json", "{}")});
        REQUIRE_FALSE(descendant.has_value());
        CHECK(descendant.error().code() == "playback.source.document_path_conflict");
    }

    SECTION("entry lookup sorts documents and rejects a missing or empty chart") {
        auto sorted = typedSource("portable-source", "charts/main.cuexis.chart.json",
                                  {document("templates/unused.cxt", "{}"),
                                   document("charts/main.cuexis.chart.json", staticV3Chart())});
        REQUIRE(sorted.has_value());

        auto missing = typedSource("portable-source", "charts/main.cuexis.chart.json",
                                   {document("templates/unused.cxt", "{}")});
        REQUIRE_FALSE(missing.has_value());
        CHECK(missing.error().code() == "playback.source.entry_document_missing");

        auto empty = typedSource("portable-source", "charts/main.cuexis.chart.json",
                                 {document("charts/main.cuexis.chart.json", "")});
        REQUIRE_FALSE(empty.has_value());
        CHECK(empty.error().code() == "playback.source.chart_empty");
    }
}

TEST_CASE("TypedPlaybackProjectSource accepts UTF-8 boundaries and rejects malformed sequences",
          "[playback][source][utf8][branch-coverage]") {
    const auto entryPath = std::string{"charts/main.cuexis.chart.json"};
    const auto validPrefix = staticV3Chart();

    SECTION("valid two, three, and four byte boundaries") {
        const std::string_view validSuffixes[] = {
            "\xC2\x80",         "\xDF\xBF",         "\xE0\xA0\x80",     "\xE1\x80\x80",
            "\xEC\xBF\xBF",     "\xED\x9F\xBF",     "\xEE\x80\x80",     "\xEF\xBF\xBF",
            "\xF0\x90\x80\x80", "\xF1\x80\x80\x80", "\xF3\xBF\xBF\xBF", "\xF4\x8F\xBF\xBF"};
        for (const auto suffix : validSuffixes) {
            CAPTURE(suffix);
            auto text = validPrefix;
            text.append(suffix);
            auto source =
                typedSource("utf8-source", entryPath, {document(entryPath, std::move(text))});
            REQUIRE(source.has_value());
        }
    }

    SECTION("malformed leading, continuation, range, and truncation sequences") {
        const std::string_view invalidSuffixes[] = {"\x80",
                                                    "\xC0\x80",
                                                    "\xC1\x80",
                                                    "\xC2",
                                                    "\xC2\x7F",
                                                    "\xE0",
                                                    "\xE0\x9F\x80",
                                                    "\xE0\xA0",
                                                    "\xE1\x80",
                                                    "\xE1\x80\x7F",
                                                    "\xED\xA0\x80",
                                                    "\xED\x80",
                                                    "\xF0",
                                                    "\xF0\x8F\x80\x80",
                                                    "\xF0\x90\x80",
                                                    "\xF1\x80\x80",
                                                    "\xF4\x90\x80\x80",
                                                    "\xF4\x80\x80",
                                                    "\xF5\x80\x80\x80"};
        for (const auto suffix : invalidSuffixes) {
            auto text = validPrefix;
            text.append(suffix);
            auto source =
                typedSource("utf8-source", entryPath, {document(entryPath, std::move(text))});
            REQUIRE_FALSE(source.has_value());
            CHECK(source.error().code() == "playback.source.document_utf8_invalid");
        }
    }
}

TEST_CASE("Playback source factories preserve typed asset routing and provider boundaries",
          "[playback][source][assets][branch-coverage]") {
    SECTION("legacy chart factory rejects empty input before constructing a document") {
        const auto empty = cuexis::playback::PlaybackSource::fromChartText("");
        REQUIRE_FALSE(empty.has_value());
        CHECK(empty.error().code() == "playback.source.chart_empty");

        auto typed = cuexis::playback::PlaybackSource::fromTypedProject(
            {.sourceId = "legacy", .chartJson = "", .assets = {}}, unusedProvider());
        REQUIRE_FALSE(typed.has_value());
        CHECK(typed.error().code() == "playback.source.chart_empty");
    }

    SECTION("typed source requires a provider and routes every public asset type") {
        const auto chartPath = std::string{"charts/main.cuexis.chart.json"};
        const auto chart = staticV3Chart();
        auto missingProvider = cuexis::playback::PlaybackSource::fromTypedProjectSource(
            {.sourceId = "missing-provider",
             .entryChartPath = chartPath,
             .projectDocuments = {document(chartPath, chart)},
             .assets = {}},
            nullptr);
        REQUIRE_FALSE(missingProvider.has_value());
        CHECK(missingProvider.error().code() == "playback.source.provider_missing");

        using cuexis::playback::PlaybackAssetDescriptor;
        using cuexis::playback::PlaybackAssetType;
        const std::vector<PlaybackAssetDescriptor> assets = {
            {.id = "mesh",
             .type = PlaybackAssetType::Mesh,
             .rootId = "main",
             .logicalSource = "mesh.bin"},
            {.id = "material",
             .type = PlaybackAssetType::Material,
             .rootId = "main",
             .logicalSource = "material.bin"},
            {.id = "texture",
             .type = PlaybackAssetType::Texture,
             .rootId = "main",
             .logicalSource = "texture.bin"},
            {.id = "audio",
             .type = PlaybackAssetType::Audio,
             .rootId = "main",
             .logicalSource = "audio.bin"},
            {.id = "shader",
             .type = PlaybackAssetType::Shader,
             .rootId = "main",
             .logicalSource = "shader.bin"},
        };
        auto source = cuexis::playback::PlaybackSource::fromTypedProjectSource(
            {.sourceId = "typed-assets",
             .entryChartPath = chartPath,
             .projectDocuments = {document(chartPath, chart)},
             .assets = assets},
            unusedProvider());
        REQUIRE(source.has_value());

        auto emptyRoot = assets;
        emptyRoot.front().rootId.clear();
        auto rejected = cuexis::playback::PlaybackSource::fromTypedProjectSource(
            {.sourceId = "missing-root",
             .entryChartPath = chartPath,
             .projectDocuments = {document(chartPath, chart)},
             .assets = std::move(emptyRoot)},
            unusedProvider());
        REQUIRE_FALSE(rejected.has_value());
        CHECK(rejected.error().code() == "playback.source.root_missing");
    }

    SECTION("asset dependencies are retained through source construction") {
        using cuexis::playback::PlaybackAssetDescriptor;
        using cuexis::playback::PlaybackAssetType;
        const auto chartPath = std::string{"charts/main.cuexis.chart.json"};
        auto source = cuexis::playback::PlaybackSource::fromTypedProjectSource(
            {.sourceId = "dependency-source",
             .entryChartPath = chartPath,
             .projectDocuments = {document(chartPath, staticV3Chart())},
             .assets = {{.id = "material",
                         .type = PlaybackAssetType::Material,
                         .rootId = "main",
                         .logicalSource = "material.bin",
                         .dependencies = {"texture"}},
                        {.id = "texture",
                         .type = PlaybackAssetType::Texture,
                         .rootId = "main",
                         .logicalSource = "texture.bin"}}},
            unusedProvider());
        REQUIRE(source.has_value());

        cuexis::playback::PlaybackSession session;
        REQUIRE(session.load(std::move(*source), cuexis::playback::PlaybackMode::ChartClock)
                    .has_value());
        CHECK(session.contentInfo()->chartFormatVersion == 3U);
    }
}

TEST_CASE("Typed Playback sources retain asset-index validation and dependency diagnostics",
          "[playback][source][assets][diagnostics][branch-coverage]") {
    using cuexis::playback::PlaybackAssetDescriptor;
    using cuexis::playback::PlaybackAssetType;

    const auto chartPath = std::string{"charts/main.cuexis.chart.json"};
    const auto makeSource = [&](std::vector<PlaybackAssetDescriptor> assets) {
        return cuexis::playback::PlaybackSource::fromTypedProjectSource(
            {.sourceId = "asset-validation",
             .entryChartPath = chartPath,
             .projectDocuments = {document(chartPath, staticV3Chart())},
             .assets = std::move(assets)},
            unusedProvider());
    };

    SECTION("duplicate asset IDs and logical sources are rejected before a source is created") {
        auto duplicateId = makeSource({
            {.id = "mesh.duplicate",
             .type = PlaybackAssetType::Mesh,
             .rootId = "main",
             .logicalSource = "meshes/one.bin"},
            {.id = "mesh.duplicate",
             .type = PlaybackAssetType::Mesh,
             .rootId = "main",
             .logicalSource = "meshes/two.bin"},
        });
        REQUIRE_FALSE(duplicateId.has_value());
        CHECK(duplicateId.error().code() == "assets.database.asset_id_duplicate");

        auto duplicateSource = makeSource({
            {.id = "mesh.one",
             .type = PlaybackAssetType::Mesh,
             .rootId = "main",
             .logicalSource = "meshes/shared.bin"},
            {.id = "mesh.two",
             .type = PlaybackAssetType::Mesh,
             .rootId = "main",
             .logicalSource = "meshes/shared.bin"},
        });
        REQUIRE_FALSE(duplicateSource.has_value());
        CHECK(duplicateSource.error().code() == "assets.database.source_duplicate");
    }

    SECTION("invalid paths and dependency contracts retain their original diagnostics") {
        auto invalidPath = makeSource({
            {.id = "mesh.invalid-path",
             .type = PlaybackAssetType::Mesh,
             .rootId = "main",
             .logicalSource = "../outside.bin"},
        });
        REQUIRE_FALSE(invalidPath.has_value());
        CHECK(invalidPath.error().code() == "assets.database.source_path_invalid");

        auto missingDependency = makeSource({
            {.id = "material.missing-dependency",
             .type = PlaybackAssetType::Material,
             .rootId = "main",
             .logicalSource = "materials/missing.bin",
             .dependencies = {"texture.missing"}},
        });
        REQUIRE_FALSE(missingDependency.has_value());
        CHECK(missingDependency.error().code() == "assets.database.dependency_missing");

        auto audioDependency = makeSource({
            {.id = "audio.with-dependency",
             .type = PlaybackAssetType::Audio,
             .rootId = "main",
             .logicalSource = "audio/clip.bin",
             .dependencies = {"texture.used"}},
            {.id = "texture.used",
             .type = PlaybackAssetType::Texture,
             .rootId = "main",
             .logicalSource = "textures/used.bin"},
        });
        REQUIRE_FALSE(audioDependency.has_value());
        CHECK(audioDependency.error().code() == "assets.database.audio_dependencies_not_empty");
    }

    SECTION("dependency cycles are translated at the Playback presentation boundary") {
        auto cyclic = makeSource({
            {.id = "material.first",
             .type = PlaybackAssetType::Material,
             .rootId = "main",
             .logicalSource = "materials/first.bin",
             .dependencies = {"texture.second"}},
            {.id = "texture.second",
             .type = PlaybackAssetType::Texture,
             .rootId = "main",
             .logicalSource = "textures/second.bin",
             .dependencies = {"material.first"}},
        });
        REQUIRE_FALSE(cyclic.has_value());
        CHECK(cyclic.error().code() == "playback.presentation.dependency.cycle");
        REQUIRE(cyclic.error().cause() != nullptr);
        CHECK(cyclic.error().cause()->code() == "assets.database.dependency_cycle");
    }
}

TEST_CASE("Filesystem and CXC source factories preserve loader diagnostics",
          "[playback][source][loader][diagnostics][branch-coverage]") {
    SECTION("filesystem project failures preserve the first project diagnostic") {
        const auto missing = cuexis::playback::PlaybackSource::fromFilesystemProject(
            std::filesystem::path{CUEXIS_SOURCE_DIR} / "tests" / "fixtures" / "does-not-exist");
        REQUIRE_FALSE(missing.has_value());
        CHECK(missing.error().code() == "playback.source.project_invalid");
        CHECK_FALSE(errorContext(missing.error(), "diagnostic_code").empty());
        CHECK_FALSE(errorContext(missing.error(), "field_path").empty());
    }

    SECTION("file and memory CXC loaders preserve their diagnostic envelope") {
        const auto missingFile = cuexis::playback::PlaybackSource::fromCxcFile(
            std::filesystem::path{CUEXIS_SOURCE_DIR} / "tests" / "fixtures" / "does-not-exist.cxc");
        REQUIRE_FALSE(missingFile.has_value());
        CHECK(missingFile.error().code() == "playback.source.cxc_invalid");
        CHECK_FALSE(errorContext(missingFile.error(), "diagnostic_code").empty());
        CHECK_FALSE(errorContext(missingFile.error(), "field_path").empty());

        for (const auto& bytes : std::vector<std::vector<std::byte>>{
                 {}, {std::byte{0x50}}, {std::byte{0x50}, std::byte{0x4B}}}) {
            auto malformed = cuexis::playback::PlaybackSource::fromCxcMemory(bytes);
            REQUIRE_FALSE(malformed.has_value());
            CHECK(malformed.error().code() == "playback.source.cxc_invalid");
            CHECK_FALSE(errorContext(malformed.error(), "diagnostic_code").empty());
        }
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

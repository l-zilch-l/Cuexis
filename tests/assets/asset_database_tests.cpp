#include <cuexis/assets/asset_database.hpp>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>

namespace {

class TemporaryAssetRoot final {
  public:
    TemporaryAssetRoot() {
        static std::atomic<unsigned int> next{1};
        path_ = std::filesystem::temp_directory_path() /
                ("cuexis-assets-tests-" + std::to_string(next.fetch_add(1)));
        std::filesystem::create_directories(path_);
    }

    ~TemporaryAssetRoot() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    TemporaryAssetRoot(const TemporaryAssetRoot&) = delete;
    auto operator=(const TemporaryAssetRoot&) -> TemporaryAssetRoot& = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept {
        return path_;
    }

    void write(std::string_view relativePath, std::string_view bytes) const {
        const auto target = path_ / std::filesystem::path{relativePath};
        std::filesystem::create_directories(target.parent_path());
        std::ofstream stream{target, std::ios::binary};
        stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }

  private:
    std::filesystem::path path_;
};

auto inputFor(const TemporaryAssetRoot& root, std::vector<cuexis::assets::AssetRecord> records)
    -> cuexis::assets::AssetDatabaseInput {
    return {.roots = {{.root = {.id = "main", .path = root.path()},
                       .index = {.assets = std::move(records)}}}};
}

auto hasDiagnostic(const cuexis::core::Diagnostics& diagnostics, std::string_view code) -> bool {
    for (const auto& diagnostic : diagnostics.items()) {
        if (diagnostic.code() == code) {
            return true;
        }
    }
    return false;
}

} // namespace

TEST_CASE("AssetDatabase builds a deterministic typed index and reads bounded blobs",
          "[assets][database]") {
    TemporaryAssetRoot root;
    root.write("meshes/note.bin", "mesh-data");
    root.write("materials/basic.bin", "material-data");

    const auto built = cuexis::assets::AssetDatabase::build(
        inputFor(root, {
                           {.id = {"mesh/note"},
                            .type = cuexis::assets::AssetType::Mesh,
                            .source = "meshes/note.bin",
                            .dependencies = {{"material.basic"}}},
                           {.id = {"material.basic"},
                            .type = cuexis::assets::AssetType::Material,
                            .source = "materials/basic.bin"},
                       }));

    REQUIRE(built.hasValue());
    CHECK(built.database->size() == 2);
    CHECK(built.database->rootCount() == 1);
    REQUIRE(built.database->find("mesh/note") != nullptr);
    CHECK(built.database->find("mesh/note")->type == cuexis::assets::AssetType::Mesh);
    CHECK(built.database->rootIdOf({"mesh/note"}) == "main");

    const auto ids = built.database->ids();
    REQUIRE(ids.size() == 2);
    CHECK(ids[0].value == "material.basic");
    CHECK(ids[1].value == "mesh/note");

    const auto blob = built.database->readBlob({"mesh/note"});
    REQUIRE(blob.has_value());
    CHECK(blob->bytes.size() == 9);
    CHECK(blob->rootId == "main");
    CHECK(blob->source == "meshes/note.bin");
}

TEST_CASE("AssetDatabase rejects duplicate identities and physical sources",
          "[assets][database][validation]") {
    TemporaryAssetRoot root;
    root.write("shared.bin", "data");

    SECTION("duplicate AssetId") {
        const auto built = cuexis::assets::AssetDatabase::build(
            inputFor(root, {
                               {.id = {"mesh.same"},
                                .type = cuexis::assets::AssetType::Mesh,
                                .source = "shared.bin"},
                               {.id = {"mesh.same"},
                                .type = cuexis::assets::AssetType::Mesh,
                                .source = "shared.bin"},
                           }));
        CHECK_FALSE(built.hasValue());
        CHECK(hasDiagnostic(built.diagnostics, "assets.database.asset_id_duplicate"));
    }

    SECTION("duplicate physical source") {
        const auto built = cuexis::assets::AssetDatabase::build(
            inputFor(root, {
                               {.id = {"mesh.first"},
                                .type = cuexis::assets::AssetType::Mesh,
                                .source = "shared.bin"},
                               {.id = {"mesh.second"},
                                .type = cuexis::assets::AssetType::Mesh,
                                .source = "shared.bin"},
                           }));
        CHECK_FALSE(built.hasValue());
        CHECK(hasDiagnostic(built.diagnostics, "assets.database.source_duplicate"));
    }
}

TEST_CASE("AssetDatabase rejects unsupported typed input values",
          "[assets][database][validation]") {
    TemporaryAssetRoot root;
    root.write("asset.bin", "data");
    const auto built = cuexis::assets::AssetDatabase::build(
        inputFor(root, {{.id = {"mesh.invalid"},
                         .type = static_cast<cuexis::assets::AssetType>(255),
                         .source = "asset.bin"}}));

    CHECK_FALSE(built.hasValue());
    CHECK(hasDiagnostic(built.diagnostics, "assets.database.asset_type_invalid"));
}

TEST_CASE("AssetDatabase routes v2 Audio and enforces the audio dependency boundary",
          "[assets][database][audio]") {
    TemporaryAssetRoot root;
    root.write("audio/main.wav", "wav");
    root.write("mesh.bin", "mesh");

    auto acceptedInput = inputFor(root, {{.id = {"audio.main"},
                                          .type = cuexis::assets::AssetType::Audio,
                                          .source = "audio/main.wav"}});
    acceptedInput.roots[0].index.version = 2;
    const auto accepted = cuexis::assets::AssetDatabase::build(acceptedInput);
    REQUIRE(accepted.hasValue());
    CHECK(accepted.database->typeOf({"audio.main"}) == cuexis::assets::AssetType::Audio);

    auto v1Input = acceptedInput;
    v1Input.roots[0].index.version = 1;
    const auto v1 = cuexis::assets::AssetDatabase::build(v1Input);
    REQUIRE_FALSE(v1.hasValue());
    CHECK(hasDiagnostic(v1.diagnostics, "assets.database.asset_type_invalid"));

    auto invalidInput = inputFor(root, {{.id = {"audio.main"},
                                         .type = cuexis::assets::AssetType::Audio,
                                         .source = "audio/main.wav"},
                                        {.id = {"mesh.note"},
                                         .type = cuexis::assets::AssetType::Mesh,
                                         .source = "mesh.bin",
                                         .dependencies = {{"audio.main"}}}});
    invalidInput.roots[0].index.version = 2;
    const auto invalid = cuexis::assets::AssetDatabase::build(invalidInput);
    REQUIRE_FALSE(invalid.hasValue());
    CHECK(hasDiagnostic(invalid.diagnostics, "assets.database.audio_dependency_forbidden"));
}

TEST_CASE("AssetDatabase rejects unsafe sources and overlapping roots",
          "[assets][database][security]") {
    TemporaryAssetRoot root;
    root.write("valid.bin", "data");

    SECTION("parent traversal") {
        const auto built = cuexis::assets::AssetDatabase::build(
            inputFor(root, {{.id = {"mesh.invalid"},
                             .type = cuexis::assets::AssetType::Mesh,
                             .source = "../outside.bin"}}));
        CHECK_FALSE(built.hasValue());
        CHECK(hasDiagnostic(built.diagnostics, "assets.database.source_path_invalid"));
    }

    SECTION("backslash") {
        const auto built = cuexis::assets::AssetDatabase::build(
            inputFor(root, {{.id = {"mesh.invalid"},
                             .type = cuexis::assets::AssetType::Mesh,
                             .source = "meshes\\outside.bin"}}));
        CHECK_FALSE(built.hasValue());
        CHECK(hasDiagnostic(built.diagnostics, "assets.database.source_path_invalid"));
    }

    SECTION("overlap") {
        std::filesystem::create_directories(root.path() / "nested");
        cuexis::assets::AssetDatabaseInput input{
            .roots =
                {
                    {.root = {.id = "main", .path = root.path()}, .index = {}},
                    {.root = {.id = "nested", .path = root.path() / "nested"}, .index = {}},
                },
        };
        const auto built = cuexis::assets::AssetDatabase::build(input);
        CHECK_FALSE(built.hasValue());
        CHECK(hasDiagnostic(built.diagnostics, "assets.database.root_overlap"));
    }
}

TEST_CASE("AssetDatabase rejects missing dependencies, cycles, and excessive depth",
          "[assets][database][dependencies]") {
    TemporaryAssetRoot root;
    root.write("a.bin", "a");
    root.write("b.bin", "b");
    root.write("c.bin", "c");

    SECTION("missing dependency") {
        const auto built = cuexis::assets::AssetDatabase::build(
            inputFor(root, {{.id = {"mesh.a"},
                             .type = cuexis::assets::AssetType::Mesh,
                             .source = "a.bin",
                             .dependencies = {{"texture.missing"}}}}));
        CHECK_FALSE(built.hasValue());
        CHECK(hasDiagnostic(built.diagnostics, "assets.database.dependency_missing"));
    }

    SECTION("cycle") {
        const auto built = cuexis::assets::AssetDatabase::build(
            inputFor(root, {
                               {.id = {"mesh.a"},
                                .type = cuexis::assets::AssetType::Mesh,
                                .source = "a.bin",
                                .dependencies = {{"material.b"}}},
                               {.id = {"material.b"},
                                .type = cuexis::assets::AssetType::Material,
                                .source = "b.bin",
                                .dependencies = {{"mesh.a"}}},
                           }));
        CHECK_FALSE(built.hasValue());
        CHECK(hasDiagnostic(built.diagnostics, "assets.database.dependency_cycle"));
    }

    SECTION("depth") {
        auto input = inputFor(root, {
                                        {.id = {"mesh.a"},
                                         .type = cuexis::assets::AssetType::Mesh,
                                         .source = "a.bin",
                                         .dependencies = {{"material.b"}}},
                                        {.id = {"material.b"},
                                         .type = cuexis::assets::AssetType::Material,
                                         .source = "b.bin",
                                         .dependencies = {{"texture.c"}}},
                                        {.id = {"texture.c"},
                                         .type = cuexis::assets::AssetType::Texture,
                                         .source = "c.bin"},
                                    });
        auto limits = cuexis::assets::AssetDatabaseLimits{};
        limits.maxDependencyDepth = 2;
        const auto built = cuexis::assets::AssetDatabase::build(input, limits);
        CHECK_FALSE(built.hasValue());
        CHECK(hasDiagnostic(built.diagnostics, "assets.database.dependency_depth"));
    }
}

TEST_CASE("AssetDatabase enforces read-time blob limits and containment rechecks",
          "[assets][database][blob]") {
    TemporaryAssetRoot root;
    root.write("mesh.bin", "0123456789");
    auto database = cuexis::assets::AssetDatabase::create(inputFor(
        root,
        {{.id = {"mesh.note"}, .type = cuexis::assets::AssetType::Mesh, .source = "mesh.bin"}}));
    REQUIRE(database.has_value());

    const auto tooSmall = database->readBlob({"mesh.note"}, {.maxBytes = 4});
    REQUIRE_FALSE(tooSmall.has_value());
    CHECK(tooSmall.error().code() == "assets.blob.too_large");

    std::filesystem::remove(root.path() / "mesh.bin");
    const auto removed = database->readBlob({"mesh.note"});
    REQUIRE_FALSE(removed.has_value());
    CHECK(removed.error().code() == "assets.blob.source_unavailable");
}

TEST_CASE("AssetDatabase stops immediately when the root budget is exceeded",
          "[assets][database][limits]") {
    cuexis::assets::AssetDatabaseInput input;
    input.roots = {
        {.root = {.id = "one", .path = "missing-one"}, .index = {}},
        {.root = {.id = "two", .path = "missing-two"}, .index = {}},
    };
    auto limits = cuexis::assets::AssetDatabaseLimits{};
    limits.maxRoots = 1;

    const auto built = cuexis::assets::AssetDatabase::build(input, limits);
    CHECK_FALSE(built.hasValue());
    REQUIRE(built.diagnostics.size() == 1);
    CHECK(hasDiagnostic(built.diagnostics, "assets.database.root_limit"));
}

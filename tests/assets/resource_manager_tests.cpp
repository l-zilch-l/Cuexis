#include <cuexis/assets/resource_manager.hpp>
#include <cuexis/content/content_provider.hpp>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <future>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

static_assert(!std::is_copy_constructible_v<cuexis::assets::MeshLease>);
static_assert(std::is_nothrow_move_constructible_v<cuexis::assets::MeshLease>);
static_assert(!std::is_copy_constructible_v<cuexis::assets::ResourceScope>);

namespace {

class ResourceFixture final {
  public:
    ResourceFixture() {
        static std::atomic<unsigned int> next{1};
        // Catch discovery runs cases in separate processes; their counters all start at one.
        // Claim a directory exclusively instead of sharing an existing fixture directory.
        do {
            root_ = std::filesystem::temp_directory_path() /
                    ("cuexis-resource-tests-" + std::to_string(next.fetch_add(1)));
        } while (!std::filesystem::create_directory(root_));
    }

    ~ResourceFixture() {
        std::error_code error;
        std::filesystem::remove_all(root_, error);
    }

    ResourceFixture(const ResourceFixture&) = delete;
    auto operator=(const ResourceFixture&) -> ResourceFixture& = delete;

    void write(std::string_view path, std::string_view bytes) const {
        const auto target = root_ / std::filesystem::path{path};
        std::filesystem::create_directories(target.parent_path());
        std::ofstream stream{target, std::ios::binary};
        stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }

    void remove(std::string_view path) const {
        std::filesystem::remove(root_ / std::filesystem::path{path});
    }

    [[nodiscard]] auto root() const noexcept -> const std::filesystem::path& {
        return root_;
    }

    [[nodiscard]] auto database(std::vector<cuexis::assets::AssetRecord> records,
                                std::uint32_t version = 1) const -> cuexis::assets::AssetDatabase {
        auto database = cuexis::assets::AssetDatabase::create({
            .roots = {{.root = {.id = "main", .path = root_},
                       .index = {.version = version, .assets = std::move(records)}}},
        });
        if (!database) {
            throw std::runtime_error{std::string{database.error().message()}};
        }
        return std::move(*database);
    }

  private:
    std::filesystem::path root_;
};

auto hasDiagnostic(const cuexis::core::Diagnostics& diagnostics, std::string_view code) -> bool {
    for (const auto& diagnostic : diagnostics.items()) {
        if (diagnostic.code() == code) {
            return true;
        }
    }
    return false;
}

auto basicDatabase(ResourceFixture& fixture) -> cuexis::assets::AssetDatabase {
    fixture.write("mesh.bin", "mesh");
    fixture.write("material.bin", "material");
    fixture.write("texture.bin", "texture");
    return fixture.database({
        {.id = {"mesh.note"}, .type = cuexis::assets::AssetType::Mesh, .source = "mesh.bin"},
        {.id = {"material.basic"},
         .type = cuexis::assets::AssetType::Material,
         .source = "material.bin"},
        {.id = {"texture.white"},
         .type = cuexis::assets::AssetType::Texture,
         .source = "texture.bin"},
    });
}

auto byteVector(std::string_view text) -> std::vector<std::byte> {
    std::vector<std::byte> result;
    result.reserve(text.size());
    for (const unsigned char character : text) {
        result.push_back(static_cast<std::byte>(character));
    }
    return result;
}

} // namespace

TEST_CASE("ResourceManager reads logical assets only through an injected provider",
          "[assets][resource][content]") {
    auto database = cuexis::assets::AssetDatabase::create({
        .roots = {{.root = {.id = "memory"},
                   .index = {.assets = {{.id = {"mesh.memory"},
                                         .type = cuexis::assets::AssetType::Mesh,
                                         .source = "mesh.bin"}}}}},
        .sourceMode = cuexis::assets::AssetSourceMode::Logical,
    });
    REQUIRE(database.has_value());
    CHECK(database->defaultContentProvider() == nullptr);

    auto provider = cuexis::content::MemoryContentProvider::create({{
        .rootId = "memory",
        .source = "mesh.bin",
        .bytes = byteVector("memory-mesh"),
        .revision = 42,
    }});
    REQUIRE(provider.has_value());

    cuexis::assets::ResourceManager manager{std::move(*database), *provider};
    auto lease = manager.loadMesh({"mesh.memory"});
    REQUIRE(lease.has_value());
    CHECK(lease->get()->bytes().size() == 11);
    REQUIRE(lease->get()->blob != nullptr);
    CHECK(lease->get()->blob->providerRevision == 42);
}

TEST_CASE("ResourceManager loads typed CPU blobs and rejects cross-manager handles",
          "[assets][resource]") {
    ResourceFixture fixture;
    auto database = basicDatabase(fixture);
    cuexis::assets::ResourceManager first{database};
    cuexis::assets::ResourceManager second{database};

    auto lease = first.loadMesh({"mesh.note"});
    REQUIRE(lease.has_value());
    const auto handle = lease->handle();
    CHECK(handle.belongsTo(first.managerToken()));
    CHECK_FALSE(handle.belongsTo(second.managerToken()));
    CHECK(lease->get()->id.value == "mesh.note");
    CHECK(lease->get()->bytes().size() == 4);

    const auto resource = first.get(handle);
    REQUIRE(resource.has_value());
    CHECK((*resource)->id.value == "mesh.note");
    REQUIRE(first.state(handle).has_value());
    CHECK(*first.state(handle) == cuexis::assets::ResourceState::Ready);
    REQUIRE(first.contentRevision(handle).has_value());
    CHECK(*first.contentRevision(handle) == 1);

    const auto foreign = second.get(handle);
    REQUIRE_FALSE(foreign.has_value());
    CHECK(foreign.error().code() == "assets.resource.manager_mismatch");

    const auto metrics = first.metrics();
    CHECK(metrics.ready == 1);
    CHECK(metrics.strongReferences == 1);
    CHECK(metrics.loadedBytes == 4);
}

TEST_CASE("ResourceManager loads AudioSource through its distinct typed handle",
          "[assets][resource][audio]") {
    ResourceFixture fixture;
    fixture.write("main.wav", "encoded-wav");
    auto database = fixture.database(
        {{.id = {"audio.main"}, .type = cuexis::assets::AssetType::Audio, .source = "main.wav"}},
        2);
    cuexis::assets::ResourceManager manager{std::move(database)};

    auto lease = manager.loadAudioSource({"audio.main"});
    REQUIRE(lease.has_value());
    CHECK(lease->get()->id.value == "audio.main");
    CHECK(lease->get()->bytes().size() == 11);
    CHECK(manager.get(lease->handle()).has_value());

    auto scope = manager.createScope();
    const auto requested = scope.requestAudioSource({"audio.main"});
    REQUIRE(requested.hasValue());
    CHECK(scope.contains(cuexis::assets::AssetType::Audio, {"audio.main"}));
    CHECK(scope.contains(*requested.handle));
}

TEST_CASE("ResourceManager loads Shader through its distinct typed handle",
          "[assets][resource][shader][s5-c]") {
    ResourceFixture fixture;
    fixture.write("sprite.shader.bin", "glsl-source");
    auto database = fixture.database({{.id = {"shader.sprite"},
                                       .type = cuexis::assets::AssetType::Shader,
                                       .source = "sprite.shader.bin"}},
                                     3);
    cuexis::assets::ResourceManager manager{std::move(database)};

    auto lease = manager.loadShader({"shader.sprite"});
    REQUIRE(lease.has_value());
    CHECK(lease->get()->id.value == "shader.sprite");
    CHECK(lease->get()->bytes().size() == 11);
    CHECK(manager.get(lease->handle()).has_value());

    auto scope = manager.createScope();
    const auto requested = scope.requestShader({"shader.sprite"});
    REQUIRE(requested.hasValue());
    CHECK(scope.contains(cuexis::assets::AssetType::Shader, {"shader.sprite"}));
    CHECK(scope.contains(*requested.handle));
}

TEST_CASE("Lease release invalidates old handles and advances generation on reuse",
          "[assets][resource][lifetime]") {
    ResourceFixture fixture;
    cuexis::assets::ResourceManager manager{basicDatabase(fixture)};

    auto first = manager.loadMesh({"mesh.note"});
    REQUIRE(first.has_value());
    const auto oldHandle = first->handle();

    auto second = manager.loadMesh({"mesh.note"});
    REQUIRE(second.has_value());
    CHECK(second->handle() == oldHandle);
    CHECK(manager.metrics().strongReferences == 2);

    first->reset();
    CHECK(manager.get(oldHandle).has_value());
    second->reset();

    const auto stale = manager.get(oldHandle);
    REQUIRE_FALSE(stale.has_value());
    CHECK(stale.error().code() == "assets.resource.stale_handle");
    CHECK(manager.metrics().ready == 0);

    auto replacement = manager.loadMesh({"mesh.note"});
    REQUIRE(replacement.has_value());
    CHECK(replacement->handle().index == oldHandle.index);
    CHECK(replacement->handle().generation == oldHandle.generation + 1);
    REQUIRE(manager.contentRevision(replacement->handle()).has_value());
    CHECK(*manager.contentRevision(replacement->handle()) == 2);
}

TEST_CASE("Resource policies produce deterministic required fallback and optional outcomes",
          "[assets][resource][policy]") {
    ResourceFixture fixture;
    cuexis::assets::ResourceManager manager{basicDatabase(fixture)};

    const auto required =
        manager.requestMesh({"mesh.missing"}, cuexis::assets::ResourcePolicy::Required);
    CHECK_FALSE(required.hasValue());
    CHECK_FALSE(required.succeeded());
    CHECK(hasDiagnostic(required.diagnostics, "assets.resource.required_failed"));

    auto fallback = manager.requestMesh({"mesh.missing"}, cuexis::assets::ResourcePolicy::Fallback);
    REQUIRE(fallback.hasValue());
    CHECK(fallback.succeeded());
    CHECK(hasDiagnostic(fallback.diagnostics, "assets.resource.fallback_used"));
    CHECK(fallback.lease->get()->id.value == "cuexis.builtin.fallback.mesh");

    const auto optional =
        manager.requestMesh({"mesh.missing"}, cuexis::assets::ResourcePolicy::Optional);
    CHECK_FALSE(optional.hasValue());
    CHECK(optional.succeeded());
    CHECK(hasDiagnostic(optional.diagnostics, "assets.resource.optional_skipped"));

    const auto wrongType =
        manager.requestMesh({"texture.white"}, cuexis::assets::ResourcePolicy::Required);
    CHECK_FALSE(wrongType.hasValue());
    CHECK(hasDiagnostic(wrongType.diagnostics, "assets.resource.required_failed"));
}

TEST_CASE("ResourceScope owns a deduplicated transitive dependency closure",
          "[assets][resource][scope]") {
    ResourceFixture fixture;
    fixture.write("mesh.bin", "mesh");
    fixture.write("material-a.bin", "material-a");
    fixture.write("material-b.bin", "material-b");
    fixture.write("texture.bin", "texture");
    auto database = fixture.database({
        {.id = {"mesh.note"},
         .type = cuexis::assets::AssetType::Mesh,
         .source = "mesh.bin",
         .dependencies = {{"material.a"}, {"material.b"}}},
        {.id = {"material.a"},
         .type = cuexis::assets::AssetType::Material,
         .source = "material-a.bin",
         .dependencies = {{"texture.white"}}},
        {.id = {"material.b"},
         .type = cuexis::assets::AssetType::Material,
         .source = "material-b.bin",
         .dependencies = {{"texture.white"}}},
        {.id = {"texture.white"},
         .type = cuexis::assets::AssetType::Texture,
         .source = "texture.bin"},
    });
    cuexis::assets::ResourceManager manager{std::move(database)};
    auto scope = manager.createScope();

    const auto requested = scope.requestMesh({"mesh.note"});
    REQUIRE(requested.hasValue());
    CHECK(scope.size() == 4);
    CHECK(scope.contains(cuexis::assets::AssetType::Mesh, {"mesh.note"}));
    CHECK(scope.contains(cuexis::assets::AssetType::Material, {"material.a"}));
    CHECK(scope.contains(cuexis::assets::AssetType::Material, {"material.b"}));
    CHECK(scope.contains(cuexis::assets::AssetType::Texture, {"texture.white"}));
    CHECK(scope.contains(*requested.handle));
    CHECK(manager.metrics().ready == 4);
    CHECK(manager.metrics().strongReferences == 4);

    const auto repeated = scope.requestMesh({"mesh.note"});
    REQUIRE(repeated.hasValue());
    CHECK(*repeated.handle == *requested.handle);
    CHECK(scope.size() == 4);
    CHECK(manager.metrics().strongReferences == 4);

    const auto oldHandle = *requested.handle;
    scope.clear();
    CHECK(scope.empty());
    CHECK(manager.metrics().ready == 0);
    const auto stale = manager.get(oldHandle);
    REQUIRE_FALSE(stale.has_value());
    CHECK(stale.error().code() == "assets.resource.stale_handle");
}

TEST_CASE("ResourceScope rolls back partial dependency loads before policy handling",
          "[assets][resource][scope][rollback]") {
    ResourceFixture fixture;
    fixture.write("mesh.bin", "mesh");
    fixture.write("texture.bin", "texture");
    auto database = fixture.database({
        {.id = {"mesh.note"},
         .type = cuexis::assets::AssetType::Mesh,
         .source = "mesh.bin",
         .dependencies = {{"texture.white"}}},
        {.id = {"texture.white"},
         .type = cuexis::assets::AssetType::Texture,
         .source = "texture.bin"},
    });
    cuexis::assets::ResourceManager manager{std::move(database)};
    fixture.remove("mesh.bin");

    auto scope = manager.createScope();
    const auto required = scope.requestMesh({"mesh.note"});
    CHECK_FALSE(required.hasValue());
    CHECK(scope.empty());
    CHECK(manager.metrics().ready == 0);

    const auto fallback =
        scope.requestMesh({"mesh.note"}, cuexis::assets::ResourcePolicy::Fallback);
    REQUIRE(fallback.hasValue());
    CHECK(hasDiagnostic(fallback.diagnostics, "assets.resource.fallback_used"));
    CHECK(scope.size() == 1);
    const auto resource = manager.get(*fallback.handle);
    REQUIRE(resource.has_value());
    CHECK((*resource)->id.value == "cuexis.builtin.fallback.mesh");
}

TEST_CASE("ResourceScope cached fallbacks preserve each later request policy",
          "[assets][resource][scope][policy]") {
    ResourceFixture fixture;
    cuexis::assets::ResourceManager manager{basicDatabase(fixture)};
    auto scope = manager.createScope();

    const auto fallback =
        scope.requestMesh({"mesh.missing"}, cuexis::assets::ResourcePolicy::Fallback);
    REQUIRE(fallback.hasValue());
    REQUIRE(hasDiagnostic(fallback.diagnostics, "assets.resource.fallback_used"));
    CHECK(scope.size() == 1);

    const auto repeatedFallback =
        scope.requestMesh({"mesh.missing"}, cuexis::assets::ResourcePolicy::Fallback);
    REQUIRE(repeatedFallback.hasValue());
    CHECK(*repeatedFallback.handle == *fallback.handle);
    CHECK(hasDiagnostic(repeatedFallback.diagnostics, "assets.resource.fallback_used"));
    CHECK(scope.size() == 1);

    const auto required =
        scope.requestMesh({"mesh.missing"}, cuexis::assets::ResourcePolicy::Required);
    CHECK_FALSE(required.hasValue());
    CHECK_FALSE(required.succeeded());
    CHECK(hasDiagnostic(required.diagnostics, "assets.resource.required_failed"));
    CHECK(scope.size() == 1);

    const auto optional =
        scope.requestMesh({"mesh.missing"}, cuexis::assets::ResourcePolicy::Optional);
    CHECK_FALSE(optional.hasValue());
    CHECK(optional.succeeded());
    CHECK(hasDiagnostic(optional.diagnostics, "assets.resource.optional_skipped"));
    CHECK(scope.size() == 1);
}

TEST_CASE("ResourceScope never reuses a cached fallback as a required dependency",
          "[assets][resource][scope][policy]") {
    ResourceFixture fixture;
    fixture.write("material.bin", "material");
    fixture.write("texture.bin", "texture");
    auto database = fixture.database({
        {.id = {"material.basic"},
         .type = cuexis::assets::AssetType::Material,
         .source = "material.bin",
         .dependencies = {{"texture.white"}}},
        {.id = {"texture.white"},
         .type = cuexis::assets::AssetType::Texture,
         .source = "texture.bin"},
    });
    cuexis::assets::ResourceManager manager{std::move(database)};
    fixture.remove("texture.bin");
    auto scope = manager.createScope();

    const auto fallback =
        scope.requestTexture({"texture.white"}, cuexis::assets::ResourcePolicy::Fallback);
    REQUIRE(fallback.hasValue());
    CHECK(scope.size() == 1);

    const auto material = scope.requestMaterial({"material.basic"});
    CHECK_FALSE(material.hasValue());
    CHECK(hasDiagnostic(material.diagnostics, "assets.resource.required_failed"));
    CHECK(scope.size() == 1);
    CHECK(scope.contains(cuexis::assets::AssetType::Texture, {"texture.white"}));
    CHECK_FALSE(scope.contains(cuexis::assets::AssetType::Material, {"material.basic"}));
}

TEST_CASE("ResourceScope key index stays consistent across rollback move and clear",
          "[assets][resource][scope][index]") {
    ResourceFixture fixture;
    fixture.write("good.bin", "good");
    fixture.write("failing.bin", "failing");
    fixture.write("dependency.bin", "dependency");
    auto database = fixture.database({
        {.id = {"mesh.good"}, .type = cuexis::assets::AssetType::Mesh, .source = "good.bin"},
        {.id = {"mesh.failing"},
         .type = cuexis::assets::AssetType::Mesh,
         .source = "failing.bin",
         .dependencies = {{"texture.dependency"}}},
        {.id = {"texture.dependency"},
         .type = cuexis::assets::AssetType::Texture,
         .source = "dependency.bin"},
    });
    cuexis::assets::ResourceManager manager{std::move(database)};
    fixture.remove("failing.bin");

    auto scope = manager.createScope();
    const auto good = scope.requestMesh({"mesh.good"});
    REQUIRE(good.hasValue());
    const auto goodHandle = *good.handle;

    const auto failed = scope.requestMesh({"mesh.failing"});
    CHECK_FALSE(failed.hasValue());
    CHECK(scope.size() == 1);
    CHECK(scope.contains(cuexis::assets::AssetType::Mesh, {"mesh.good"}));
    CHECK_FALSE(scope.contains(cuexis::assets::AssetType::Texture, {"texture.dependency"}));

    cuexis::assets::ResourceScope moved{std::move(scope)};
    CHECK(scope.empty());
    CHECK_FALSE(scope.contains(cuexis::assets::AssetType::Mesh, {"mesh.good"}));
    CHECK(moved.contains(cuexis::assets::AssetType::Mesh, {"mesh.good"}));
    const auto repeated = moved.requestMesh({"mesh.good"});
    REQUIRE(repeated.hasValue());
    CHECK(*repeated.handle == goodHandle);
    CHECK(moved.size() == 1);

    auto assigned = manager.createScope();
    const auto dependency = assigned.requestTexture({"texture.dependency"});
    REQUIRE(dependency.hasValue());
    const auto dependencyHandle = *dependency.handle;
    assigned = std::move(moved);
    CHECK(moved.empty());
    CHECK(assigned.contains(cuexis::assets::AssetType::Mesh, {"mesh.good"}));
    CHECK_FALSE(assigned.contains(cuexis::assets::AssetType::Texture, {"texture.dependency"}));
    CHECK_FALSE(manager.get(dependencyHandle).has_value());

    assigned.clear();
    CHECK(assigned.empty());
    CHECK_FALSE(assigned.contains(cuexis::assets::AssetType::Mesh, {"mesh.good"}));

    const auto reloaded = assigned.requestMesh({"mesh.good"});
    REQUIRE(reloaded.hasValue());
    CHECK(reloaded.handle->index == goodHandle.index);
    CHECK(reloaded.handle->generation == goodHandle.generation + 1);
}

TEST_CASE("ResourceManager rejects owner-thread operations from a worker",
          "[assets][resource][thread]") {
    ResourceFixture fixture;
    cuexis::assets::ResourceManager manager{basicDatabase(fixture)};

    auto worker =
        std::async(std::launch::async, [&manager] { return manager.loadMesh({"mesh.note"}); });
    auto result = worker.get();
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == "assets.resource.not_owner_thread");
}

TEST_CASE("ResourceScope loads all five kinds through the shared typed path",
          "[assets][resource][scope][types]") {
    using namespace cuexis::assets;
    ResourceFixture fixture;
    fixture.write("mesh.bin", "mesh");
    fixture.write("material.bin", "material");
    fixture.write("texture.bin", "texture");
    fixture.write("audio.bin", "audio");
    fixture.write("shader.bin", "shader");
    auto database = fixture.database(
        {
            {.id = {"mesh.quads"},
             .type = AssetType::Mesh,
             .source = "mesh.bin",
             .dependencies = {{"material.solid"}, {"texture.white"}}},
            {.id = {"material.solid"},
             .type = AssetType::Material,
             .source = "material.bin",
             .dependencies = {{"texture.white"}, {"shader.lit"}}},
            {.id = {"texture.white"}, .type = AssetType::Texture, .source = "texture.bin"},
            {.id = {"shader.lit"}, .type = AssetType::Shader, .source = "shader.bin"},
            {.id = {"audio.beep"}, .type = AssetType::Audio, .source = "audio.bin"},
        },
        3);
    ResourceManager manager{std::move(database)};
    auto scope = manager.createScope();

    // One mixed closure owns five entries: the shared texture is held once for both consumers.
    const auto mesh = scope.requestMesh({"mesh.quads"});
    REQUIRE(mesh.hasValue());
    CHECK(scope.size() == 4);
    const auto audio = scope.requestAudioSource({"audio.beep"});
    REQUIRE(audio.hasValue());
    CHECK(scope.size() == 5);
    CHECK(scope.contains(AssetType::Mesh, {"mesh.quads"}));
    CHECK(scope.contains(AssetType::Material, {"material.solid"}));
    CHECK(scope.contains(AssetType::Texture, {"texture.white"}));
    CHECK(scope.contains(AssetType::Shader, {"shader.lit"}));
    CHECK(scope.contains(AssetType::Audio, {"audio.beep"}));
    CHECK(scope.contains(*audio.handle));
    CHECK(manager.metrics().ready == 5);
    CHECK(manager.metrics().strongReferences == 5);

    // A repeated request for a shared dependency keeps the same lease and adds no holding.
    const auto texture = scope.requestTexture({"texture.white"});
    REQUIRE(texture.hasValue());
    const auto repeatedTexture = scope.requestTexture({"texture.white"});
    REQUIRE(repeatedTexture.hasValue());
    CHECK(*repeatedTexture.handle == *texture.handle);
    CHECK(scope.size() == 5);
    CHECK(manager.metrics().strongReferences == 5);

    // Each kind reports the fallback built-in for a missing id, which is only reachable through the
    // shared fallback path.
    const auto meshFallback = scope.requestMesh({"mesh.absent"}, ResourcePolicy::Fallback);
    REQUIRE(meshFallback.hasValue());
    const auto materialFallback =
        scope.requestMaterial({"material.absent"}, ResourcePolicy::Fallback);
    REQUIRE(materialFallback.hasValue());
    const auto textureFallback = scope.requestTexture({"texture.absent"}, ResourcePolicy::Fallback);
    REQUIRE(textureFallback.hasValue());
    const auto audioFallback = scope.requestAudioSource({"audio.absent"}, ResourcePolicy::Fallback);
    REQUIRE(audioFallback.hasValue());
    const auto shaderFallback = scope.requestShader({"shader.absent"}, ResourcePolicy::Fallback);
    REQUIRE(shaderFallback.hasValue());
    CHECK(scope.size() == 10);
    for (const auto* code : {"assets.resource.fallback_used"}) {
        CHECK(hasDiagnostic(meshFallback.diagnostics, code));
        CHECK(hasDiagnostic(materialFallback.diagnostics, code));
        CHECK(hasDiagnostic(textureFallback.diagnostics, code));
        CHECK(hasDiagnostic(audioFallback.diagnostics, code));
        CHECK(hasDiagnostic(shaderFallback.diagnostics, code));
    }
    const auto meshResource = manager.get(*meshFallback.handle);
    REQUIRE(meshResource.has_value());
    CHECK((*meshResource)->id.value == "cuexis.builtin.fallback.mesh");
    const auto materialResource = manager.get(*materialFallback.handle);
    REQUIRE(materialResource.has_value());
    CHECK((*materialResource)->id.value == "cuexis.builtin.fallback.material");
    const auto textureResource = manager.get(*textureFallback.handle);
    REQUIRE(textureResource.has_value());
    CHECK((*textureResource)->id.value == "cuexis.builtin.fallback.texture");
    const auto audioResource = manager.get(*audioFallback.handle);
    REQUIRE(audioResource.has_value());
    CHECK((*audioResource)->id.value == "cuexis.builtin.fallback.audio");
    const auto shaderResource = manager.get(*shaderFallback.handle);
    REQUIRE(shaderResource.has_value());
    CHECK((*shaderResource)->id.value == "cuexis.builtin.fallback.shader");

    // Required and Optional never register; the fallback entries above stay untouched.
    const auto requiredMesh = scope.requestMesh({"mesh.absent"});
    CHECK_FALSE(requiredMesh.hasValue());
    CHECK(hasDiagnostic(requiredMesh.diagnostics, "assets.resource.required_failed"));
    const auto optionalAudio = scope.requestAudioSource({"audio.absent"}, ResourcePolicy::Optional);
    CHECK_FALSE(optionalAudio.hasValue());
    CHECK(optionalAudio.succeeded());
    CHECK(hasDiagnostic(optionalAudio.diagnostics, "assets.resource.optional_skipped"));
    CHECK(scope.size() == 10);

    // Releasing the scope retires every kind and invalidates the old handles.
    const auto meshHandle = *mesh.handle;
    scope.clear();
    CHECK(scope.empty());
    CHECK(manager.metrics().ready == 0);
    const auto stale = manager.get(meshHandle);
    REQUIRE_FALSE(stale.has_value());
    CHECK(stale.error().code() == "assets.resource.stale_handle");
}

TEST_CASE("ResourceScope rejects depth and type mismatches and fallback covers the refusal",
          "[assets][resource][scope][rejection]") {
    using namespace cuexis::assets;
    ResourceFixture fixture;
    fixture.write("a.bin", "a");
    fixture.write("b.bin", "b");
    fixture.write("c.bin", "c");
    fixture.write("d.bin", "d");
    // The AssetDatabase itself rejects a dependency cycle before a manager exists, so the scope's
    // own cycle guard is defensive and cannot be reached from a valid database.
    const auto cyclic = AssetDatabase::create({
        .roots = {{.root = {.id = "main", .path = fixture.root()},
                   .index = {.version = 3,
                             .assets = {{.id = {"mesh.cycle-a"},
                                         .type = AssetType::Mesh,
                                         .source = "a.bin",
                                         .dependencies = {{"material.cycle-b"}}},
                                        {.id = {"material.cycle-b"},
                                         .type = AssetType::Material,
                                         .source = "b.bin",
                                         .dependencies = {{"mesh.cycle-a"}}}}}}},
    });
    REQUIRE_FALSE(cyclic.has_value());
    CHECK(cyclic.error().code() == "assets.database.dependency_cycle");

    auto database = fixture.database(
        {
            {.id = {"mesh.deep"},
             .type = AssetType::Mesh,
             .source = "a.bin",
             .dependencies = {{"mesh.mid"}}},
            {.id = {"mesh.mid"},
             .type = AssetType::Mesh,
             .source = "b.bin",
             .dependencies = {{"mesh.leaf"}}},
            {.id = {"mesh.leaf"}, .type = AssetType::Mesh, .source = "c.bin"},
            {.id = {"texture.wrong"}, .type = AssetType::Texture, .source = "d.bin"},
        },
        3);
    ResourceManagerLimits limits;
    limits.maxDependencyDepth = 2;
    ResourceManager manager{std::move(database), limits};
    auto scope = manager.createScope();

    const auto depth = scope.requestMesh({"mesh.deep"});
    CHECK_FALSE(depth.hasValue());
    CHECK(hasDiagnostic(depth.diagnostics, "assets.resource.required_failed"));
    CHECK(scope.empty());

    // A texture id requested as a mesh is a type mismatch, not a load failure of that texture.
    const auto mismatch = scope.requestMesh({"texture.wrong"});
    CHECK_FALSE(mismatch.hasValue());
    CHECK(hasDiagnostic(mismatch.diagnostics, "assets.resource.required_failed"));
    CHECK(scope.empty());

    // The same reject stays clean for the fallback policy: only the fallback entry appears.
    const auto fallback = scope.requestMesh({"mesh.deep"}, ResourcePolicy::Fallback);
    REQUIRE(fallback.hasValue());
    CHECK(hasDiagnostic(fallback.diagnostics, "assets.resource.fallback_used"));
    CHECK(scope.size() == 1);
    CHECK(scope.contains(AssetType::Mesh, {"mesh.deep"}));
}
#include "open_gl_presentation_internal.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

struct Fixture final {
    cuexis::playback::PresentationResourceRef meshReference;
    cuexis::playback::PresentationResourceRef materialReference;
    cuexis::playback::PresentationResourceManifest manifest;
    std::vector<cuexis::playback::PortableResourcePtr> resources;
    std::shared_ptr<cuexis::playback::PortableResource> meshResource;
    cuexis::playback::FrameSnapshot snapshot;
};

void makeIdentity(float (&matrix)[16]) {
    matrix[0] = 1.0F;
    matrix[5] = 1.0F;
    matrix[10] = 1.0F;
    matrix[15] = 1.0F;
}

auto makeFixture() -> Fixture {
    Fixture fixture;
    fixture.meshReference.type = cuexis::playback::PresentationResourceType::Mesh;
    fixture.meshReference.assetId = "mesh.bounds";
    fixture.materialReference.type = cuexis::playback::PresentationResourceType::UnlitMaterial;
    fixture.materialReference.assetId = "material.bounds";

    fixture.meshResource = std::make_shared<cuexis::playback::PortableResource>();
    fixture.meshResource->reference = fixture.meshReference;
    auto& meshValue = fixture.meshResource->value.emplace<cuexis::playback::PortableMesh>();
    meshValue.positions = {-1.0F, -1.0F, -1.0F, 1.0F, -1.0F, -1.0F, 0.0F, 1.0F, 0.0F};
    meshValue.indices = {0, 1, 2};
    meshValue.boundsMin[0] = -1.0F;
    meshValue.boundsMin[1] = -1.0F;
    meshValue.boundsMin[2] = -1.0F;
    meshValue.boundsMax[0] = 1.0F;
    meshValue.boundsMax[1] = 1.0F;
    meshValue.boundsMax[2] = 1.0F;

    auto material = std::make_shared<cuexis::playback::PortableResource>();
    material->reference = fixture.materialReference;
    material->value.emplace<cuexis::playback::PortableUnlitMaterial>();

    fixture.manifest.entries.push_back({fixture.meshReference, 0, 0, {}});
    fixture.manifest.entries.push_back({fixture.materialReference, 0, 0, {}});

    // Keep the matching mesh after an unrelated resource to retain a multi-resource cache fixture.
    fixture.resources.push_back(std::move(material));
    fixture.resources.push_back(fixture.meshResource);

    fixture.snapshot.camera.active = true;
    makeIdentity(fixture.snapshot.camera.viewMatrix);
    makeIdentity(fixture.snapshot.camera.projectionMatrix);
    cuexis::playback::FrameSnapshot::ObjectSnapshot object;
    object.id = "bounds-object";
    object.hasTransform = true;
    object.visible = true;
    makeIdentity(object.worldMatrix);
    object.materialAssetId = fixture.materialReference.assetId;
    object.mesh = fixture.meshReference;
    object.material = fixture.materialReference;
    fixture.snapshot.objects.push_back(std::move(object));
    return fixture;
}

auto findMesh(Fixture& fixture) -> cuexis::playback::PortableMesh& {
    REQUIRE(fixture.meshResource != nullptr);
    return std::get<cuexis::playback::PortableMesh>(fixture.meshResource->value);
}

auto hasContext(const cuexis::core::Error& error, std::string_view key, std::string_view value)
    -> bool {
    for (const auto& context : error.context()) {
        if (context.key == key && context.value == value) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] auto renderImplementationSource() -> std::string {
    const auto path = std::filesystem::path{CUEXIS_SOURCE_DIR} / "engine" / "render_opengl" /
                      "src" / "open_gl_presentation.cpp";
    std::ifstream input{path, std::ios::binary};
    REQUIRE(input.good());
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

[[nodiscard]] auto playerImplementationSource() -> std::string {
    const auto path =
        std::filesystem::path{CUEXIS_SOURCE_DIR} / "app" / "player" / "src" / "player_app.cpp";
    std::ifstream input{path, std::ios::binary};
    REQUIRE(input.good());
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

[[nodiscard]] auto functionRegion(const std::string& source, std::string_view functionName)
    -> std::optional<std::string_view> {
    const auto functionStart = source.find(functionName);
    if (functionStart == std::string::npos) {
        return std::nullopt;
    }
    const auto openBrace = source.find('{', functionStart);
    if (openBrace == std::string::npos) {
        return std::nullopt;
    }
    std::size_t depth = 0;
    for (std::size_t index = openBrace; index < source.size(); ++index) {
        if (source[index] == '{') {
            ++depth;
        } else if (source[index] == '}' && --depth == 0) {
            return std::string_view{source}.substr(openBrace + 1, index - openBrace - 1);
        }
    }
    return std::nullopt;
}

[[nodiscard]] auto functionSignature(const std::string& source, std::string_view functionName)
    -> std::optional<std::string_view> {
    const auto functionStart = source.find(functionName);
    if (functionStart == std::string::npos) {
        return std::nullopt;
    }
    const auto openBrace = source.find('{', functionStart);
    if (openBrace == std::string::npos) {
        return std::nullopt;
    }
    return std::string_view{source}.substr(functionStart, openBrace - functionStart);
}

} // namespace

TEST_CASE("OpenGL draw bounds use a prepare-time cache and avoid frame resource scans",
          "[render][opengl][bounds][characterization]") {
    auto fixture = makeFixture();
    cuexis::render_opengl::detail::BoundsProbeStats stats;
    auto summary = cuexis::render_opengl::detail::probeBuildDraws(
        fixture.snapshot, fixture.manifest, fixture.resources, &stats);
    REQUIRE(summary.has_value());
    REQUIRE(summary->opaque.size() == 1);
    CHECK(summary->opaque.front().depthMeters == Catch::Approx(0.0));
    CHECK(stats.meshResourceLookupComparisons == 0);
    CHECK(stats.meshBoundsParses == 1);

    stats = {};
    auto second = cuexis::render_opengl::detail::probeBuildDraws(fixture.snapshot, fixture.manifest,
                                                                 fixture.resources, &stats);
    REQUIRE(second.has_value());
    CHECK(stats.meshResourceLookupComparisons == 0);
    CHECK(stats.meshBoundsParses == 1);
}

TEST_CASE("OpenGL draw bounds report a missing mesh resource", "[render][opengl][bounds]") {
    auto fixture = makeFixture();
    fixture.resources.erase(fixture.resources.begin() + 1);

    auto result = cuexis::render_opengl::detail::probeBuildDraws(fixture.snapshot, fixture.manifest,
                                                                 fixture.resources);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == "playback.presentation.frame.resource_mismatch");
}

TEST_CASE("OpenGL draw bounds reject non-finite Portable Mesh bounds", "[render][opengl][bounds]") {
    auto fixture = makeFixture();
    auto& mesh = findMesh(fixture);
    mesh.boundsMin[0] = std::numeric_limits<float>::infinity();

    auto result = cuexis::render_opengl::detail::probeBuildDraws(fixture.snapshot, fixture.manifest,
                                                                 fixture.resources);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == "playback.presentation.frame.non_finite");
    CHECK(hasContext(result.error(), "field", "mesh_bounds"));
}

TEST_CASE("OpenGL draw preparation rejects late frame errors without a partial summary",
          "[render][opengl][frame][rollback]") {
    auto fixture = makeFixture();
    const auto baseline = cuexis::render_opengl::detail::probeBuildDraws(
        fixture.snapshot, fixture.manifest, fixture.resources);
    REQUIRE(baseline.has_value());
    REQUIRE(baseline->opaque.size() == 1);

    auto lateObject = fixture.snapshot.objects.front();
    lateObject.id = "late-invalid-object";
    lateObject.worldMatrix[0] = std::numeric_limits<float>::infinity();
    fixture.snapshot.objects.push_back(std::move(lateObject));

    const auto rejected = cuexis::render_opengl::detail::probeBuildDraws(
        fixture.snapshot, fixture.manifest, fixture.resources);
    REQUIRE_FALSE(rejected.has_value());
    CHECK(rejected.error().code() == "playback.presentation.frame.non_finite");
    CHECK(hasContext(rejected.error(), "object_id", "late-invalid-object"));

    // The successful summary is a caller-owned value; a later failed build cannot mutate it.
    CHECK(baseline->opaque.size() == 1);
    CHECK(baseline->opaque.front().objectId == "bounds-object");
}

TEST_CASE("OpenGL draw bounds tolerate an empty mesh and preserve its zero center",
          "[render][opengl][bounds]") {
    auto fixture = makeFixture();
    auto& mesh = findMesh(fixture);
    mesh.positions.clear();
    mesh.indices.clear();
    mesh.boundsMin[0] = 0.0F;
    mesh.boundsMin[1] = 0.0F;
    mesh.boundsMin[2] = 0.0F;
    mesh.boundsMax[0] = 0.0F;
    mesh.boundsMax[1] = 0.0F;
    mesh.boundsMax[2] = 0.0F;

    auto summary = cuexis::render_opengl::detail::probeBuildDraws(
        fixture.snapshot, fixture.manifest, fixture.resources);
    REQUIRE(summary.has_value());
    REQUIRE(summary->opaque.size() == 1);
    CHECK(summary->opaque.front().depthMeters == Catch::Approx(0.0));
}

TEST_CASE("Parameterized OpenGL numeric uniforms are queried during prepare, not hot draw",
          "[render][opengl][uniform][characterization]") {
    const auto source = renderImplementationSource();
    const auto numericSignature = functionSignature(source, "setNumericUniform");
    const auto numericBody = functionRegion(source, "setNumericUniform");
    const auto prepareBody = functionRegion(source, "uploadParameterizedProgram");
    const auto drawBody = functionRegion(source, "drawParameterizedCommands");
    REQUIRE(numericSignature.has_value());
    REQUIRE(numericBody.has_value());
    REQUIRE(prepareBody.has_value());
    REQUIRE(drawBody.has_value());

    // RT-27 requires a prepared GLint location. Numeric lookups belong to the prepare path and
    // the hot draw helper receives only the cached location plus the typed value.
    CHECK(numericSignature->find("GLint") != std::string_view::npos);
    CHECK(numericSignature->find("GLuint program") == std::string_view::npos);
    CHECK(numericBody->find("glGetUniformLocation") == std::string_view::npos);
    CHECK(prepareBody->find("glGetUniformLocation") != std::string_view::npos);
    CHECK(drawBody->find("glGetUniformLocation") == std::string_view::npos);
    CHECK(drawBody->find("numericUniformLocations") != std::string_view::npos);
    CHECK(drawBody->find("numericUniforms.begin()") == std::string_view::npos);
}

TEST_CASE("Parameterized OpenGL optional uniform locations remain skippable",
          "[render][opengl][uniform]") {
    const auto source = renderImplementationSource();
    const auto numericBody = functionRegion(source, "setNumericUniform");
    const auto prepareBody = functionRegion(source, "uploadParameterizedProgram");
    REQUIRE(numericBody.has_value());
    REQUIRE(prepareBody.has_value());

    // A location below zero is the OpenGL contract for an optimized-out optional uniform. Both
    // numeric and texture bindings must skip writes rather than fail the draw.
    CHECK(numericBody->find("location < 0") != std::string_view::npos);
    CHECK(numericBody->find("return;") != std::string_view::npos);
    CHECK(prepareBody->find("mapped.location >= 0") != std::string_view::npos);
}

TEST_CASE("OpenGL summary characterization preserves command order and digest",
          "[render][opengl][summary][characterization]") {
    auto fixture = makeFixture();
    const auto first = cuexis::render_opengl::detail::probeBuildDraws(
        fixture.snapshot, fixture.manifest, fixture.resources);
    REQUIRE(first.has_value());

    const auto second = cuexis::render_opengl::detail::probeBuildDraws(
        fixture.snapshot, fixture.manifest, fixture.resources);
    REQUIRE(second.has_value());

    REQUIRE(first->opaque.size() == second->opaque.size());
    REQUIRE(first->transparent.size() == second->transparent.size());
    CHECK(first->digest == second->digest);
    REQUIRE(first->opaque.size() == 1);
    CHECK(first->opaque.front().objectId == second->opaque.front().objectId);
    CHECK(first->opaque.front().worldMatrix == second->opaque.front().worldMatrix);
    CHECK(first->opaque.front().mesh == second->opaque.front().mesh);
    CHECK(first->opaque.front().material == second->opaque.front().material);
    CHECK(first->opaque.front().effectiveColor == second->opaque.front().effectiveColor);
    CHECK(first->opaque.front().pass == second->opaque.front().pass);
    CHECK(first->opaque.front().depthMeters == second->opaque.front().depthMeters);
    CHECK(first->opaque.front().transparentDepthKey == second->opaque.front().transparentDepthKey);
    CHECK(first->debugCommandCount == second->debugCommandCount);
}

TEST_CASE("OpenGL summary omission skips command copies and digest work",
          "[render][opengl][summary][characterization]") {
    const auto source = renderImplementationSource();
    const auto body = functionRegion(source, "renderPresentationFrame");
    REQUIRE(body.has_value());

    CHECK(body->find("const bool needSummary = summary != nullptr") != std::string_view::npos);
    CHECK(body->find("if (needSummary)") != std::string_view::npos);
    CHECK(body->find("buildDraws(snapshot, *presentation_->active, preparedSummary") !=
          std::string_view::npos);
    CHECK(body->find("*summary = std::move(preparedSummary)") != std::string_view::npos);
}

TEST_CASE("OpenGL frame scratch vectors persist in backend state",
          "[render][opengl][scratch][characterization]") {
    const auto source = renderImplementationSource();
    const auto body = functionRegion(source, "renderPresentationFrame");
    REQUIRE(body.has_value());

    CHECK(body->find("state.opaqueScratch.clear()") != std::string_view::npos);
    CHECK(body->find("state.transparentScratch.clear()") != std::string_view::npos);
    CHECK(body->find("state.debugVerticesScratch.clear()") != std::string_view::npos);
    CHECK(body->find("std::vector<PreparedDraw> opaque;") == std::string_view::npos);
    CHECK(body->find("std::vector<PreparedDraw> transparent;") == std::string_view::npos);
    CHECK(body->find("std::vector<DebugVertex> debugVertices;") == std::string_view::npos);
}

TEST_CASE("Player frame scene is persistent and cleared per frame",
          "[player][scene][allocation][characterization]") {
    const auto source = playerImplementationSource();
    const auto loopBody = functionRegion(source, "run");
    REQUIRE(loopBody.has_value());

    const auto sceneDeclaration = loopBody->find("render::RenderScene scene;");
    const auto appendCall = loopBody->find("appendSnapshotAxes(snapshot, scene)");
    const auto loopStart = loopBody->find("while (!quitRequested)");
    REQUIRE(sceneDeclaration != std::string_view::npos);
    REQUIRE(appendCall != std::string_view::npos);
    REQUIRE(loopStart != std::string_view::npos);
    CHECK(sceneDeclaration < loopStart);
    CHECK(loopBody->find("scene.clear()") != std::string_view::npos);
}

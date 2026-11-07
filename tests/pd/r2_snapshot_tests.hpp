#pragma once

#include <algorithm>
#include <tuple>

namespace r2_snapshot_tests {
using namespace cuexis;
inline void same(const playback::FrameSnapshot& a, const playback::FrameSnapshot& b) {
    CHECK(std::tie(a.clearRed, a.clearGreen, a.clearBlue, a.clearAlpha, a.viewportWidth,
                   a.viewportHeight) == std::tie(b.clearRed, b.clearGreen, b.clearBlue,
                                                 b.clearAlpha, b.viewportWidth, b.viewportHeight));
    CHECK(std::tie(a.camera.active, a.camera.fovY, a.camera.nearPlane, a.camera.farPlane,
                   a.camera.pitch, a.camera.yaw, a.camera.roll) ==
          std::tie(b.camera.active, b.camera.fovY, b.camera.nearPlane, b.camera.farPlane,
                   b.camera.pitch, b.camera.yaw, b.camera.roll));
    CHECK(std::equal(std::begin(a.camera.viewMatrix), std::end(a.camera.viewMatrix),
                     std::begin(b.camera.viewMatrix)));
    CHECK(std::equal(std::begin(a.camera.projectionMatrix), std::end(a.camera.projectionMatrix),
                     std::begin(b.camera.projectionMatrix)));
    REQUIRE(a.objects.size() == b.objects.size());
    for (std::size_t i = 0; i < a.objects.size(); ++i) {
        CAPTURE(i);
        const auto& x = a.objects[i];
        const auto& y = b.objects[i];
        CHECK(std::tie(x.id, x.hasTransform, x.visible, x.materialAssetId, x.mesh, x.material,
                       x.materialOpacity) == std::tie(y.id, y.hasTransform, y.visible,
                                                      y.materialAssetId, y.mesh, y.material,
                                                      y.materialOpacity));
        CHECK(std::equal(std::begin(x.worldMatrix), std::end(x.worldMatrix),
                         std::begin(y.worldMatrix)));
        CHECK(std::equal(std::begin(x.materialTint), std::end(x.materialTint),
                         std::begin(y.materialTint)));
    }
}
inline void pollute(playback::FrameSnapshot& value, std::size_t count) {
    value.objects.resize(count + 2);
    playback::PresentationResourceRef ref{
        playback::PresentationResourceType::Shader, std::string(300, 'x'), {}};
    ref.identity.sha256.fill(255);
    for (auto& object : value.objects) {
        object.id = "caller pollution";
        std::fill(std::begin(object.worldMatrix), std::end(object.worldMatrix), -99.0F);
        object.hasTransform = false;
        object.visible = false;
        object.materialAssetId = std::string(300, 'y');
        object.mesh = ref;
        object.material = ref;
        object.materialOpacity = -8;
        std::fill(std::begin(object.materialTint), std::end(object.materialTint), -9.0F);
    }
    value.camera.active = true;
    value.camera.fovY = value.camera.nearPlane = value.camera.farPlane = -10;
    value.camera.pitch = value.camera.yaw = value.camera.roll = -11;
    std::fill(std::begin(value.camera.viewMatrix), std::end(value.camera.viewMatrix), -12.0F);
    std::fill(std::begin(value.camera.projectionMatrix), std::end(value.camera.projectionMatrix),
              -13.0F);
    value.clearRed = value.clearGreen = value.clearBlue = value.clearAlpha = -14;
    value.viewportWidth = value.viewportHeight = 999;
}
} // namespace r2_snapshot_tests

TEST_CASE("R2 public extraction restores every polluted field across DTOs sessions and reload",
          "[pd][presentation-state][r2]") {
    using namespace cuexis;
    playback::PlaybackSession first, second;
    REQUIRE(first.loadChart(chartJson));
    REQUIRE(second.loadChart(chartJson));
    const auto old = first.extractFrame({640, 480});
    REQUIRE(old);
    playback::FrameSnapshot left, right;
    for (auto* session : {&first, &second}) {
        for (auto* dto : {&left, &right}) {
            r2_snapshot_tests::pollute(*dto, 1);
            REQUIRE(session->extractFrame({800, 600}, *dto));
            const auto fresh = session->extractFrame({800, 600});
            REQUIRE(fresh);
            r2_snapshot_tests::same(*dto, *fresh);
        }
    }
    REQUIRE(first.reload(chartJson, {20, 0, 1}, playback::ReloadPolicy::KeepChartTime));
    r2_snapshot_tests::pollute(left, 1);
    REQUIRE(first.extractFrame({640, 480}, left));
    r2_snapshot_tests::same(left, *first.extractFrame({640, 480}));
    REQUIRE(first.unload());
    REQUIRE(second.unload());
    CHECK(old->objects[0].worldMatrix[12] == 2);
    REQUIRE(first.loadChart(chartJson));
    REQUIRE(first.extractFrame({640, 480}, right));
    r2_snapshot_tests::same(*old, right);
}

#if defined(CUEXIS_PLAYBACK_STATIC_DEFINE)
#include "r2_legacy_materializer.hpp"
#include <cuexis/chart/chart_runtime.hpp>
#include <cuexis/render/camera_component.hpp>
#include <cuexis/render/renderable_component.hpp>
#include <cuexis/runtime/runtime_session.hpp>
#include <filesystem>

namespace r2_snapshot_tests {
using Layout = playback::detail::SnapshotLayout;
inline auto database() -> assets::AssetDatabase {
    assets::AssetDatabaseInput input;
    input.roots.push_back({.root = {.id = "main",
                                    .path = std::filesystem::path{CUEXIS_SOURCE_DIR} /
                                            "tests/fixtures/stage1b_project"},
                           .index = {.format = "cuexis.asset-index",
                                     .version = 1,
                                     .assets = {{.id = {"material.basic"},
                                                 .type = assets::AssetType::Material,
                                                 .source = "assets/materials/basic.material.bin",
                                                 .dependencies = {{"texture.white"}}},
                                                {.id = {"mesh.note"},
                                                 .type = assets::AssetType::Mesh,
                                                 .source = "assets/meshes/note.mesh.bin"},
                                                {.id = {"texture.white"},
                                                 .type = assets::AssetType::Texture,
                                                 .source = "assets/textures/white.texture.bin"}}}});
    auto result = assets::AssetDatabase::create(input);
    REQUIRE(result);
    return std::move(*result);
}
struct Fixture {
    assets::ResourceManager resources{database()};
    runtime::RuntimeSession session{resources};
    Layout layout;
    static auto idFor(std::size_t index) -> std::string {
        const auto number = std::to_string(index);
        return "object." + std::string(3 - number.size(), '0') + number;
    }
    explicit Fixture(std::size_t count) {
        chart::ChartDocument doc;
        doc.chartId = {"r2.snapshot"};
        doc.timing.defaultBpm = 120;
        for (std::size_t i = 0; i < count; ++i) {
            chart::ObjectComponents c;
            c.transform = chart::TransformData{.position = {static_cast<float>(i), 2, 3}};
            c.renderable =
                chart::RenderableData{.mesh = {"mesh.note"}, .material = {"material.basic"}};
            if (!i)
                c.camera = chart::CameraComponentData{.fovY = 70};
            const auto id = idFor(i);
            doc.objects.push_back({.id = {id}, .components = c});
            playback::PresentationResourceRef mesh{
                playback::PresentationResourceType::Mesh, "mesh.note", {}};
            playback::PresentationResourceRef material{
                playback::PresentationResourceType::UnlitMaterial, "material.basic", {}};
            mesh.identity.sha256[0] = 17;
            material.identity.sha256[0] = 29;
            layout.entities.push_back({.id = {id},
                                       .runtimeIndex = i,
                                       .mesh = mesh,
                                       .materials = {{"material.basic", material}},
                                       .maximumMeshAssetIdSize = 9,
                                       .maximumMaterialAssetIdSize = 14});
        }
        auto compiled = chart::ChartCompiler::compile(doc);
        REQUIRE(compiled.hasValue());
        auto prepared = session.prepare(*compiled.runtime);
        REQUIRE(prepared.hasValue());
        REQUIRE(session.commit(std::move(*prepared.prepared)));
        REQUIRE(session.update({0, 0, 0}));
        bind();
    }
    void bind() {
        const auto state = *runtime::detail::PresentationAccess::state(session);
        layout.runtimeOwner = state.stamp.owner;
        layout.runtimeLayout = state.stamp.layout;
    }
    template <typename Fn> void mutate(std::size_t index, Fn&& fn) {
        auto entity = session.findEntity({idFor(index)});
        REQUIRE(entity);
        REQUIRE(*entity);
        REQUIRE(session.withWorld([&](world::World& w) {
            return w.withRegistry([&](entt::registry& r) { fn(r, **entity); });
        }));
    }
    auto compare(playback::FrameSnapshot& candidate, playback::FrameSnapshot& reference)
        -> core::Result<void> {
        return runtime::detail::PresentationAccess::read(
            session, runtime::detail::PresentationAccess::state(session)->stamp,
            [&](const runtime::detail::PresentationRead& view) -> core::Result<void> {
                auto old = playback::detail::r2_legacy::legacyMaterializeSnapshot(
                    view, layout, {800, 600}, reference);
                auto result =
                    playback::detail::materializeSnapshot(view, layout, {800, 600}, candidate);
                CHECK(result.has_value() == old.has_value());
                if (!result && !old)
                    sameError(result.error(), old.error());
                same(candidate, reference);
                return result;
            });
    }
    static void sameError(const core::Error& a, const core::Error& b) {
        CHECK(a.code() == b.code());
        CHECK(a.message() == b.message());
        REQUIRE(a.context().size() == b.context().size());
        for (std::size_t i = 0; i < a.context().size(); ++i) {
            CHECK(a.context()[i].key == b.context()[i].key);
            CHECK(a.context()[i].value == b.context()[i].value);
        }
        REQUIRE((a.cause() == nullptr) == (b.cause() == nullptr));
        if (a.cause())
            sameError(*a.cause(), *b.cause());
    }
};
} // namespace r2_snapshot_tests

TEST_CASE(
    "R2 materializer matches frozen old fields defaults identities empty full and tail blocks",
    "[pd][presentation-state][r2]") {
    using namespace r2_snapshot_tests;
    for (const std::size_t count : {0, 1, 31, 32, 33, 65}) {
        CAPTURE(count);
        Fixture f{count};
        playback::FrameSnapshot candidate, reference;
        pollute(candidate, count);
        reference = candidate;
        REQUIRE(f.compare(candidate, reference));
        if (count) {
            f.mutate(count - 1, [](entt::registry& r, entt::entity e) {
                r.remove<world::WorldTransformComponent>(e);
                r.remove<render::AppearanceComponent>(e);
                r.remove<render::RenderableComponent>(e);
            });
            f.layout.entities.back().mesh.reset();
            REQUIRE(f.compare(candidate, reference));
            CHECK_FALSE(candidate.objects.back().hasTransform);
            CHECK_FALSE(candidate.objects.back().mesh);
            CHECK_FALSE(candidate.objects.back().material);
        }
    }
}

TEST_CASE("R2 original order includes noncontiguous indices and material override fallback",
          "[pd][presentation-state][r2]") {
    using namespace r2_snapshot_tests;
    Fixture f{33};
    std::reverse(f.layout.entities.begin(), f.layout.entities.end());
    playback::FrameSnapshot candidate, reference;
    REQUIRE(f.compare(candidate, reference));
    auto alternate = f.layout.entities[1].materials.begin()->second;
    alternate.assetId = "material.alternate";
    alternate.identity.sha256[0] = 77;
    f.layout.entities[1].materials.emplace(alternate.assetId, alternate);
    f.mutate(31, [](entt::registry& r, entt::entity e) {
        auto& appearance = r.get<render::AppearanceComponent>(e);
        appearance.materialAssetId = "material.alternate";
        appearance.visible = false;
        appearance.opacity = 0.25;
        appearance.tint = {0.1F, 0.2F, 0.3F};
    });
    REQUIRE(f.compare(candidate, reference));
    CHECK(candidate.objects[1].material == alternate);
    // A singleton whose sole key no longer matches must take the old failing lookup path.
    f.layout.entities[1].materials.erase("material.basic");
    f.mutate(31, [](entt::registry& r, entt::entity e) {
        r.get<render::AppearanceComponent>(e).materialAssetId = "material.basic";
    });
    auto failed = f.compare(candidate, reference);
    REQUIRE_FALSE(failed);
    CHECK(failed.error().code() == "playback.presentation.frame.resource_mismatch");
}

TEST_CASE("R2 preserves first errors contexts and partial writes inside and across blocks",
          "[pd][presentation-state][r2]") {
    using namespace r2_snapshot_tests;
    for (std::size_t first : {0, 30, 31, 32}) {
        for (int error = 0; error < 5; ++error) {
            CAPTURE(first, error);
            Fixture f{65};
            f.layout.entities[first + 1].runtimeIndex = std::numeric_limits<std::size_t>::max();
            if (error == 0) {
                f.mutate(first, [](entt::registry& r, entt::entity e) {
                    r.get<render::AppearanceComponent>(e).materialAssetId = "missing";
                });
            } else if (error == 1) {
                f.mutate(first, [](entt::registry& r, entt::entity e) {
                    r.get<render::AppearanceComponent>(e).materialAssetId = std::string(257, 'z');
                });
            } else if (error == 2) {
                f.mutate(first, [](entt::registry& r, entt::entity e) {
                    r.remove<render::AppearanceComponent>(e);
                });
            } else if (error == 3) {
                f.layout.entities[first].id.value = "wrong layout ID";
            } else {
                f.layout.entities[first].runtimeIndex = std::numeric_limits<std::size_t>::max();
            }
            playback::FrameSnapshot candidate, reference;
            pollute(candidate, 65);
            reference = candidate;
            auto failed = f.compare(candidate, reference);
            REQUIRE_FALSE(failed);
            CHECK(failed.error().code() == (error < 3
                                                ? "playback.presentation.frame.resource_mismatch"
                                                : "runtime.presentation.layout_stale"));
            CHECK(candidate.objects.size() == 67); // no successful final shrink on failure
        }
    }
}

TEST_CASE("R2 camera failures and stale state preserve legacy materialization order",
          "[pd][presentation-state][r2]") {
    using namespace r2_snapshot_tests;
    Fixture f{33};
    playback::FrameSnapshot candidate, reference;
    REQUIRE(f.compare(candidate, reference));
    f.mutate(0, [](entt::registry& r, entt::entity e) {
        r.get<world::WorldTransformComponent>(e).matrix = core::makeScale({0, 1, 1});
    });
    auto singular = f.compare(candidate, reference);
    REQUIRE_FALSE(singular);
    CHECK(singular.error().code() == "playback.snapshot.camera_not_invertible");
    f.mutate(0, [](entt::registry& r, entt::entity e) { r.remove<render::CameraComponent>(e); });
    auto missing = f.compare(candidate, reference);
    REQUIRE_FALSE(missing);
    CHECK(missing.error().code() == "playback.snapshot.camera_missing");
    ++f.layout.runtimeLayout;
    auto stale = f.compare(candidate, reference);
    REQUIRE_FALSE(stale);
    CHECK(stale.error().code() == "runtime.presentation.layout_stale");
}
#endif

#include "transaction_allocations.hpp"
#include "transaction_state.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cuexis/render/camera_component.hpp>
#include <cuexis/render/renderable_component.hpp>
#include <cuexis/runtime/transaction_access.hpp>
#include <filesystem>

using namespace cuexis;
namespace {
auto database() -> assets::AssetDatabase {
    assets::AssetDatabaseInput input;
    input.roots.push_back({.root = {.id = "main",
                                    .path = std::filesystem::path{CUEXIS_TEST_SOURCE_DIR} /
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
    auto db = assets::AssetDatabase::create(input);
    REQUIRE(db);
    return std::move(*db);
}
auto chartData(std::size_t extraObjects = 0) -> chart::ChartRuntime {
    chart::ObjectComponents parent, child;
    parent.transform = chart::TransformData{};
    parent.camera = chart::CameraComponentData{.fovY = 60};
    parent.behavior = chart::BehaviorReferenceData{{"move"}};
    child.transform = chart::TransformData{.position = {2, 0, 0}};
    child.renderable = chart::RenderableData{.mesh = {"mesh.note"}, .material = {"material.basic"}};
    const auto zero = *chart::RationalBeat::create(0, 1), one = *chart::RationalBeat::create(1, 1);
    chart::ChartDocument doc;
    doc.chartId = {"pd3a.transaction"};
    doc.timing.defaultBpm = 120;
    chart::ChartBehavior behavior;
    behavior.id = {"move"};
    behavior.type = "behavior.transform.keyframe";
    behavior.version = 1;
    chart::BehaviorTrack position{
        .property = chart::BehaviorProperty::TransformPositionX,
        .keys = {{.beat = zero, .value = 0.0}, {.beat = one, .value = 10.0}}};
    chart::BehaviorTrack fov{.property = chart::BehaviorProperty::CameraFovY,
                             .keys = {{.beat = zero, .value = 60.0}, {.beat = one, .value = 90.0}}};
    behavior.tracks = chart::BehaviorTracks{{position, fov}};
    doc.behaviors.push_back(std::move(behavior));
    doc.objects = {{.id = {"child"}, .parent = chart::ChartObjectId{"parent"}, .components = child},
                   {.id = {"parent"}, .components = parent}};
    for (std::size_t i = 0; i < extraObjects; ++i) {
        chart::ObjectComponents components;
        components.transform = chart::TransformData{};
        doc.objects.push_back({.id = {"extra." + std::to_string(i)}, .components = components});
    }
    auto result = chart::ChartCompiler::compile(doc);
    REQUIRE(result.hasValue());
    return std::move(*result.runtime);
}
struct Fixture {
    assets::ResourceManager resources{database()};
    runtime::RuntimeSession session{resources};
    entt::entity parent{}, child{};
    explicit Fixture(bool cold = false, bool warmed = true) {
        auto prepared = session.prepare(chartData());
        REQUIRE(prepared.hasValue());
        REQUIRE(session.commit(std::move(*prepared.prepared)));
        parent = **session.findEntity({"parent"});
        child = **session.findEntity({"child"});
        REQUIRE(session.configureDebug({true, 32}));
        if (warmed)
            REQUIRE(session.update({0, 0, 0}));
        const std::array writes{
            runtime::PropertyOverrideWrite{{"parent"}, world::PropertyId::TransformPositionY, 3.0}};
        REQUIRE(session.acquireOverride(
            world::OverrideKind::Host, "host", 0,
            world::propertyBit(world::PropertyId::TransformPositionY),
            {.kind = world::OverrideLifetimeKind::RemainingFrames, .remainingFrames = 2}, writes));
        if (cold)
            REQUIRE(session.withWorld([&](world::World& w) {
                return w.withRegistry([&](entt::registry& r) {
                    r.remove<world::WorldTransformComponent>(child);
                    r.remove<world::HierarchyComponent>(child);
                });
            }));
    }
    auto observed() {
        auto physical = std::as_const(session).withWorld([&](const world::World& w) {
            return w.withRegistry([&](const entt::registry& r) {
                std::vector<double> values;
                std::vector<std::string> text;
                for (auto e : {parent, child}) {
                    const auto& local = r.get<world::TransformComponent>(e);
                    values.insert(values.end(),
                                  {local.position.x, local.position.y, local.scale.x});
                    auto* matrix = r.try_get<world::WorldTransformComponent>(e);
                    values.push_back(matrix != nullptr);
                    if (matrix)
                        for (auto v : matrix->matrix.values)
                            values.push_back(v);
                    auto* camera = r.try_get<render::CameraComponent>(e);
                    values.push_back(camera != nullptr);
                    if (camera)
                        values.push_back(camera->fovY);
                    if (auto* a = r.try_get<render::AppearanceComponent>(e)) {
                        values.insert(values.end(), {double(a->visible), a->opacity, a->tint.x,
                                                     a->tint.y, a->tint.z});
                        text.push_back(a->materialAssetId);
                    }
                }
                return std::tuple{values, text, world::detail::WorldTestAccess::cache(w)};
            });
        });
        REQUIRE(physical);
        const auto metadata = runtime::detail::RuntimeTestAccess::state(session);
        std::vector<double> frame;
        if (metadata.frame)
            frame = {metadata.frame->chartTimeMs, metadata.frame->simulationDeltaTimeMs};
        auto debug = session.debugSnapshot();
        REQUIRE(debug);
        for (const auto& record : debug->records)
            if (const auto* value = std::get_if<double>(&record.finalValue))
                frame.push_back(*value);
        return std::tuple{*physical, frame,
                          metadata.frame ? metadata.frame->timeDiscontinuityId : UINT64_MAX,
                          metadata.baseRevision, metadata.tokenState};
    }
    auto update(bool gameplay) -> core::Result<void> {
        if (gameplay) {
            const std::array writes{
                runtime::PropertyOverrideWrite{{"child"}, world::PropertyId::RenderVisible, false}};
            return session.updateGameplay({500, 500, 0}, writes);
        }
        return session.update({500, 500, 0});
    }
};
} // namespace

TEST_CASE("PD3a Runtime preparation allocation failures preserve matrices cache frame debug and "
          "lifetimes",
          "[pd][transaction][runtime][failure]") {
    for (bool warmed : {false, true})
        for (bool gameplay : {false, true})
            for (bool cold : {false, true}) {
                std::size_t allocations{};
                {
                    Fixture f(cold, warmed);
                    pd_allocations::begin();
                    auto result = f.update(gameplay);
                    allocations = pd_allocations::end();
                    REQUIRE(result);
                }
                REQUIRE(allocations > 0);
                for (std::size_t index = 1; index <= allocations; ++index) {
                    CAPTURE(warmed, gameplay, cold, index, allocations);
                    Fixture f(cold, warmed);
                    const auto before = f.observed();
                    pd_allocations::begin(index);
                    auto rejected = f.update(gameplay);
                    pd_allocations::end();
                    CHECK(pd_allocations::failed);
                    REQUIRE_FALSE(rejected);
                    CHECK(f.observed() == before);
                    REQUIRE(f.update(gameplay));
                    Fixture control(cold, warmed);
                    REQUIRE(control.update(gameplay));
                    CHECK(f.observed() == control.observed());
                }
            }
}

TEST_CASE("PD3a Runtime rejects a missing install slot after preparing cold topology",
          "[pd][transaction][runtime][slot]") {
    for (bool gameplay : {false, true}) {
        Fixture f(true);
        REQUIRE(f.session.withWorld([&](world::World& w) {
            return w.withRegistry(
                [&](entt::registry& r) { r.remove<render::CameraComponent>(f.parent); });
        }));
        auto before = f.observed();
        auto rejected = f.update(gameplay);
        REQUIRE_FALSE(rejected);
        CHECK(rejected.error().code() == "runtime.camera.baseline_missing");
        CHECK(f.observed() == before);
        REQUIRE(f.session.withWorld([&](world::World& w) {
            return w.withRegistry(
                [&](entt::registry& r) { r.emplace<render::CameraComponent>(f.parent); });
        }));
        REQUIRE(f.update(gameplay));
    }
}

TEST_CASE("PD3a Runtime base-property failure leaves the baseline revision uncommitted",
          "[pd][transaction][runtime][base]") {
    Fixture f;
    REQUIRE(f.session.withWorld([&](world::World& w) {
        return w.withRegistry([&](entt::registry& r) {
            r.get<world::TransformComponent>(f.child).scale = {1e30F, 1e30F, 1e30F};
        });
    }));
    const auto before = f.observed();
    CHECK_FALSE(f.session.applyBaseProperty({"parent"}, world::PropertyId::TransformScale,
                                            core::Vec3{1e30F, 1e30F, 1e30F}));
    CHECK(f.observed() == before);
}

TEST_CASE("PD3a reload candidate allocation failures preserve the active World and permit retry",
          "[pd][transaction][runtime][reload]") {
    std::size_t allocations{};
    {
        Fixture f;
        auto replacement = f.session.prepare(chartData(8));
        REQUIRE(replacement.hasValue());
        pd_allocations::begin();
        auto result = runtime::detail::RuntimeTestAccess::sampleReload(
            f.session, *replacement.prepared, {0, 0, 0});
        allocations = pd_allocations::end();
        REQUIRE(result);
    }
    REQUIRE(allocations > 0);
    for (std::size_t index = 1; index <= allocations; ++index) {
        CAPTURE(index, allocations);
        Fixture f;
        const auto before = f.observed();
        auto replacement = f.session.prepare(chartData(8));
        REQUIRE(replacement.hasValue());
        bool rejected{};
        pd_allocations::begin(index);
        try {
            auto sampled = runtime::detail::RuntimeTestAccess::sampleReload(
                f.session, *replacement.prepared, {0, 0, 0});
            rejected = !sampled;
        } catch (const std::bad_alloc&) {
            rejected = true;
        }
        pd_allocations::end();
        CHECK(pd_allocations::failed);
        REQUIRE(rejected);
        CHECK(f.observed() == before);
        CHECK(f.session.objectCount() == 2);
        CHECK(**f.session.findEntity({"parent"}) == f.parent);
        REQUIRE(runtime::detail::RuntimeTestAccess::sampleReload(f.session, *replacement.prepared,
                                                                 {0, 0, 0}));
        REQUIRE(f.session.reload(chartData(8)).reloaded);
        CHECK(f.session.objectCount() == 10);
    }
}

TEST_CASE("PD3a public resource-backed reload contains preparation allocation failures",
          "[pd][transaction][runtime][reload][resources]") {
    std::size_t allocations{};
    {
        Fixture f;
        auto replacement = chartData(8);
        pd_allocations::begin();
        auto result = f.session.reload(std::move(replacement));
        allocations = pd_allocations::end();
        REQUIRE(result.reloaded);
    }
    REQUIRE(allocations > 0);
    for (std::size_t index = 1; index <= allocations; ++index) {
        CAPTURE(index, allocations);
        Fixture f;
        const auto before = f.observed();
        const auto resourcesBefore = f.session.resourceCount();
        auto replacement = chartData(8);
        pd_allocations::begin(index);
        auto rejected = f.session.reload(std::move(replacement));
        pd_allocations::end();
        CHECK(pd_allocations::failed);
        REQUIRE_FALSE(rejected.reloaded);
        CHECK(rejected.diagnostics.hasErrors());
        CHECK(f.observed() == before);
        CHECK(f.session.objectCount() == 2);
        CHECK(f.session.resourceCount() == resourcesBefore);
        CHECK(**f.session.findEntity({"parent"}) == f.parent);
        CHECK(**f.session.findEntity({"child"}) == f.child);
        REQUIRE(f.session.reload(chartData(8)).reloaded);
        CHECK(f.session.objectCount() == 10);
    }
}

TEST_CASE("PD3a ResourceScope moves transfer populated storage without allocating",
          "[pd][transaction][runtime][scope]") {
    assets::ResourceManager manager{database()};
    auto source = manager.createScope();
    auto resource = source.requestMesh({"mesh.note"}, assets::ResourcePolicy::Fallback);
    REQUIRE(resource.hasValue());
    const auto count = source.size();
    std::optional<assets::ResourceScope> moved;
    auto destination = manager.createScope();
    pd_allocations::begin();
    moved.emplace(std::move(source));
    destination = std::move(*moved);
    const auto allocations = pd_allocations::end();
    CHECK(allocations == 0);
    CHECK(source.empty());
    CHECK(moved->empty());
    CHECK_FALSE(source.contains(*resource.handle));
    CHECK_FALSE(moved->contains(*resource.handle));
    CHECK(destination.size() == count);
    CHECK(destination.contains(*resource.handle));
    source.clear();
    moved->clear();
}
#include "presentation_read_tests.hpp"

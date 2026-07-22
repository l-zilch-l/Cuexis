#include <cuexis/runtime/runtime_session.hpp>

#include <cuexis/assets/asset_database.hpp>
#include <cuexis/assets/resource_manager.hpp>
#include <cuexis/behavior/behavior_component.hpp>
#include <cuexis/chart/chart_runtime.hpp>
#include <cuexis/chart/timing_map.hpp>
#include <cuexis/core/math.hpp>
#include <cuexis/gameplay/tags.hpp>
#include <cuexis/render/renderable_component.hpp>
#include <cuexis/world/components.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <future>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

auto timingMap() -> cuexis::chart::TimingMap {
    auto timing = cuexis::chart::TimingMap::create(120.0, 0.0);
    if (!timing) {
        throw std::logic_error{"The test TimingMap fixture is invalid"};
    }
    return std::move(timing).value();
}

auto zeroBeat() -> cuexis::chart::RationalBeat {
    auto beat = cuexis::chart::RationalBeat::create(0, 1);
    if (!beat) {
        throw std::logic_error{"The test RationalBeat fixture is invalid"};
    }
    return std::move(beat).value();
}

auto hierarchyRuntime() -> cuexis::chart::ChartRuntime {
    cuexis::chart::ObjectComponents rootComponents;
    rootComponents.transform = cuexis::chart::TransformData{
        .position = {1.0F, 0.0F, 0.0F},
        .rotation = {},
        .scale = {1.0F, 1.0F, 1.0F},
    };
    rootComponents.element = true;

    cuexis::chart::ObjectComponents childComponents;
    childComponents.transform = cuexis::chart::TransformData{
        .position = {0.0F, 2.0F, 0.0F},
        .rotation = {},
        .scale = {1.0F, 1.0F, 1.0F},
    };
    childComponents.behavior = cuexis::chart::BehaviorReferenceData{.behavior = {"behavior.move"}};
    childComponents.note = cuexis::chart::NoteData{.beat = zeroBeat()};

    return cuexis::chart::ChartRuntime{
        .chartId = {"chart.hierarchy"},
        .timingMap = timingMap(),
        .behaviors = {{.id = {"behavior.move"}, .type = "behavior.transform", .version = 1}},
        .objects =
            {
                {.id = {"object.child"},
                 .parentIndex = 1,
                 .components = std::move(childComponents)},
                {.id = {"object.root"},
                 .parentIndex = std::nullopt,
                 .components = std::move(rootComponents)},
            },
    };
}

auto singleObjectRuntime(std::string_view id) -> cuexis::chart::ChartRuntime {
    cuexis::chart::ObjectComponents components;
    components.transform = cuexis::chart::TransformData{};

    return cuexis::chart::ChartRuntime{
        .chartId = {"chart.single"},
        .timingMap = timingMap(),
        .behaviors = {},
        .objects = {{.id = {std::string{id}},
                     .parentIndex = std::nullopt,
                     .components = std::move(components)}},
    };
}

auto unsupportedRenderableRuntime() -> cuexis::chart::ChartRuntime {
    cuexis::chart::ObjectComponents components;
    components.transform = cuexis::chart::TransformData{};
    components.renderable = cuexis::chart::RenderableData{
        .mesh = {"mesh.external"},
        .material = {"material.external"},
    };

    return cuexis::chart::ChartRuntime{
        .chartId = {"chart.renderable"},
        .timingMap = timingMap(),
        .behaviors = {},
        .objects = {{.id = {"object.renderable"},
                     .parentIndex = std::nullopt,
                     .components = std::move(components)}},
    };
}

auto supportedRenderableRuntime() -> cuexis::chart::ChartRuntime {
    auto runtime = unsupportedRenderableRuntime();
    runtime.objects[0].components.renderable = cuexis::chart::RenderableData{
        .mesh = {"mesh.note"},
        .material = {"material.basic"},
    };
    return runtime;
}

auto replacementRenderableRuntime() -> cuexis::chart::ChartRuntime {
    auto runtime = unsupportedRenderableRuntime();
    runtime.objects[0].components.renderable = cuexis::chart::RenderableData{
        .mesh = {"mesh.replacement"},
        .material = {"material.replacement"},
    };
    return runtime;
}

auto manyUnsupportedRenderableRuntime(std::size_t count) -> cuexis::chart::ChartRuntime {
    cuexis::chart::ChartRuntime runtime{
        .chartId = {"chart.many_renderables"},
        .timingMap = timingMap(),
        .behaviors = {},
        .objects = {},
    };
    runtime.objects.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        cuexis::chart::ObjectComponents components;
        components.renderable = cuexis::chart::RenderableData{
            .mesh = {"mesh.missing"},
            .material = {"material.missing"},
        };
        runtime.objects.push_back(cuexis::chart::RuntimeObject{
            .id = {"object." + std::to_string(100000 + index)},
            .parentIndex = std::nullopt,
            .components = std::move(components),
        });
    }
    return runtime;
}

auto resourceDatabase() -> cuexis::assets::AssetDatabase {
    const auto root =
        std::filesystem::path{CUEXIS_TEST_SOURCE_DIR} / "tests" / "fixtures" / "stage1b_project";
    cuexis::assets::AssetDatabaseInput input;
    input.roots.push_back(cuexis::assets::AssetRootIndex{
        .root = {.id = "main", .path = root},
        .index =
            {
                .format = "cuexis.asset-index",
                .version = 1,
                .assets =
                    {
                        {.id = {"material.basic"},
                         .type = cuexis::assets::AssetType::Material,
                         .source = "assets/materials/basic.material.bin",
                         .dependencies = {{"texture.white"}}},
                        {.id = {"material.replacement"},
                         .type = cuexis::assets::AssetType::Material,
                         .source = "cuexis.project.json",
                         .dependencies = {{"texture.replacement"}}},
                        {.id = {"mesh.note"},
                         .type = cuexis::assets::AssetType::Mesh,
                         .source = "assets/meshes/note.mesh.bin",
                         .dependencies = {}},
                        {.id = {"mesh.replacement"},
                         .type = cuexis::assets::AssetType::Mesh,
                         .source = "assets/cuexis.asset-index.json",
                         .dependencies = {}},
                        {.id = {"texture.replacement"},
                         .type = cuexis::assets::AssetType::Texture,
                         .source = "assets/charts/stage1b_example.cuexis.chart.json",
                         .dependencies = {}},
                        {.id = {"texture.white"},
                         .type = cuexis::assets::AssetType::Texture,
                         .source = "assets/textures/white.texture.bin",
                         .dependencies = {}},
                    },
            },
    });

    auto database = cuexis::assets::AssetDatabase::create(input);
    if (!database) {
        throw std::logic_error{"The resource database fixture is invalid"};
    }
    return std::move(database).value();
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

TEST_CASE("RuntimeSession commits a prepared hierarchy with stable component references",
          "[runtime][session]") {
    cuexis::runtime::RuntimeSession session;
    auto prepared = session.prepare(hierarchyRuntime());
    REQUIRE(prepared.hasValue());
    REQUIRE(session.commit(std::move(*prepared.prepared)).has_value());

    CHECK_FALSE(session.empty());
    CHECK(session.objectCount() == 2);

    const auto rootResult = session.findEntity({"object.root"});
    const auto childResult = session.findEntity({"object.child"});
    REQUIRE(rootResult.has_value());
    REQUIRE(childResult.has_value());
    REQUIRE(rootResult->has_value());
    REQUIRE(childResult->has_value());
    const entt::entity root = **rootResult;
    const entt::entity child = **childResult;

    const auto valid = session.withWorld([&](const cuexis::world::World& world) {
        return world.withRegistry([&](const entt::registry& registry) {
            const auto childPosition = cuexis::core::transformPoint(
                registry.get<cuexis::world::WorldTransformComponent>(child).matrix, {});
            return registry.get<cuexis::world::HierarchyComponent>(child).parent == root &&
                   registry.get<cuexis::behavior::BehaviorComponent>(child).behavior.value == 0 &&
                   registry.all_of<cuexis::gameplay::NoteTag>(child) &&
                   registry.all_of<cuexis::gameplay::ElementTag>(root) &&
                   cuexis::core::nearlyEqual(childPosition, {1.0F, 2.0F, 0.0F});
        });
    });
    REQUIRE(valid.has_value());
    CHECK(*valid);
}

TEST_CASE("RuntimeSession rejects external renderables without publishing a World",
          "[runtime][session][rollback]") {
    cuexis::runtime::RuntimeSession session;
    const auto prepared = session.prepare(unsupportedRenderableRuntime());

    REQUIRE_FALSE(prepared.hasValue());
    CHECK(hasDiagnostic(prepared.diagnostics, "runtime.chart.renderable_resources_unsupported"));
    CHECK(session.empty());
    CHECK(session.objectCount() == 0);
}

TEST_CASE("RuntimeSession commits renderables with a deduplicated dependency Scope",
          "[runtime][session][assets]") {
    cuexis::assets::ResourceManager resources{resourceDatabase()};
    cuexis::runtime::RuntimeSession session{resources};

    auto prepared = session.prepare(supportedRenderableRuntime());
    REQUIRE(prepared.hasValue());
    REQUIRE(session.commit(std::move(*prepared.prepared)).has_value());
    CHECK(session.resourceCount() == 3);

    const auto entityResult = session.findEntity({"object.renderable"});
    REQUIRE(entityResult.has_value());
    REQUIRE(entityResult->has_value());

    const auto handles = session.withWorld([&](const cuexis::world::World& world) {
        return world.withRegistry([&](const entt::registry& registry) {
            return registry.get<cuexis::render::RenderableComponent>(**entityResult);
        });
    });
    REQUIRE(handles.has_value());
    REQUIRE(resources.get(handles->mesh).has_value());
    REQUIRE(resources.get(handles->material).has_value());

    const auto metrics = resources.metrics();
    CHECK(metrics.ready == 3);
    CHECK(metrics.strongReferences == 3);

    REQUIRE(session.unload().has_value());
    CHECK(session.resourceCount() == 0);
    CHECK(resources.metrics().strongReferences == 0);
}

TEST_CASE("RuntimeSession retains active fallback diagnostics across a failed reload",
          "[runtime][session][assets][reload]") {
    cuexis::assets::ResourceManager resources{resourceDatabase()};
    cuexis::runtime::RuntimeSession session{resources};

    auto prepared = session.prepare(unsupportedRenderableRuntime());
    REQUIRE(prepared.hasValue());
    REQUIRE(prepared.diagnostics.hasWarnings());
    REQUIRE(session.commit(std::move(*prepared.prepared)).has_value());
    const auto activeWarningCount =
        session.activeDiagnostics().count(cuexis::core::DiagnosticSeverity::Warning);
    REQUIRE(activeWarningCount > 0);

    auto invalid = supportedRenderableRuntime();
    invalid.objects[0].parentIndex = 4;
    const auto reload = session.reload(std::move(invalid));
    REQUIRE_FALSE(reload.reloaded);
    CHECK(session.activeDiagnostics().count(cuexis::core::DiagnosticSeverity::Warning) ==
          activeWarningCount);

    REQUIRE(session.unload().has_value());
    CHECK(session.activeDiagnostics().empty());
}

TEST_CASE("RuntimeSession validates structure before loading resources",
          "[runtime][session][assets][rollback]") {
    cuexis::assets::ResourceManager resources{resourceDatabase()};
    cuexis::runtime::RuntimeSession session{resources};
    auto invalid = supportedRenderableRuntime();
    invalid.objects[0].parentIndex = 4;

    const auto prepared = session.prepare(std::move(invalid));
    REQUIRE_FALSE(prepared.hasValue());
    CHECK(hasDiagnostic(prepared.diagnostics, "runtime.chart.invalid_parent_index"));
    CHECK(resources.metrics().ready == 0);
    CHECK(resources.metrics().strongReferences == 0);
}

TEST_CASE("Prepared RuntimeSession cannot be committed by another owner",
          "[runtime][session][ownership]") {
    cuexis::runtime::RuntimeSession first;
    cuexis::runtime::RuntimeSession second;
    auto prepared = first.prepare(singleObjectRuntime("object.owner"));
    REQUIRE(prepared.hasValue());

    const auto committed = second.commit(std::move(*prepared.prepared));
    REQUIRE_FALSE(committed.has_value());
    CHECK(committed.error().code() == "runtime.session.prepared_owner_mismatch");
    CHECK(first.empty());
    CHECK(second.empty());
}

TEST_CASE("Prepared RuntimeSession rejects a same-address replacement owner",
          "[runtime][session][ownership]") {
    std::optional<cuexis::runtime::RuntimeSession> storage;
    storage.emplace();
    auto prepared = storage->prepare(singleObjectRuntime("object.old_owner"));
    REQUIRE(prepared.hasValue());

    storage.reset();
    storage.emplace();
    const auto committed = storage->commit(std::move(*prepared.prepared));
    REQUIRE_FALSE(committed.has_value());
    CHECK(committed.error().code() == "runtime.session.prepared_owner_mismatch");
}

TEST_CASE("ChartWorldInstantiator rejects bindings from mixed ResourceManagers",
          "[runtime][assets][ownership]") {
    cuexis::assets::ResourceManager first{resourceDatabase()};
    cuexis::assets::ResourceManager second{resourceDatabase()};
    auto mesh = first.loadMesh({"mesh.note"});
    auto material = second.loadMaterial({"material.basic"});
    REQUIRE(mesh.has_value());
    REQUIRE(material.has_value());

    std::vector<std::optional<cuexis::runtime::ResolvedRenderableResources>> bindings(1);
    bindings[0] = cuexis::runtime::ResolvedRenderableResources{
        .mesh = mesh->handle(),
        .material = material->handle(),
    };
    const auto instantiated = cuexis::runtime::ChartWorldInstantiator::instantiate(
        supportedRenderableRuntime(), bindings, first.managerToken());

    REQUIRE_FALSE(instantiated.hasValue());
    CHECK(hasDiagnostic(instantiated.diagnostics, "runtime.chart.renderable_handle_invalid"));
}

TEST_CASE("Runtime renderable diagnostics stay within the fixed budget",
          "[runtime][diagnostics][limits]") {
    cuexis::runtime::RuntimeSession session;
    const auto prepared = session.prepare(manyUnsupportedRenderableRuntime(1100));

    REQUIRE_FALSE(prepared.hasValue());
    CHECK(prepared.diagnostics.size() == cuexis::runtime::runtimeDiagnosticLimit);
    CHECK(hasDiagnostic(prepared.diagnostics, "runtime.chart.diagnostic_limit"));
}

TEST_CASE("Runtime resource preparation stops when its diagnostic budget is exhausted",
          "[runtime][diagnostics][limits][assets]") {
    cuexis::assets::ResourceManager resources{resourceDatabase()};
    cuexis::runtime::RuntimeSession session{resources};
    const auto prepared = session.prepare(manyUnsupportedRenderableRuntime(1100));

    REQUIRE_FALSE(prepared.hasValue());
    CHECK(prepared.diagnostics.size() == cuexis::runtime::runtimeDiagnosticLimit);
    CHECK(hasDiagnostic(prepared.diagnostics, "runtime.session.diagnostic_limit"));
    CHECK(resources.metrics().strongReferences == 0);
}

TEST_CASE("RuntimeSession reload failure preserves the active World and mapping",
          "[runtime][session][reload]") {
    cuexis::runtime::RuntimeSession session;
    auto prepared = session.prepare(singleObjectRuntime("object.original"));
    REQUIRE(prepared.hasValue());
    REQUIRE(session.commit(std::move(*prepared.prepared)).has_value());

    const auto originalBefore = session.findEntity({"object.original"});
    REQUIRE(originalBefore.has_value());
    REQUIRE(originalBefore->has_value());

    const auto reload = session.reload(unsupportedRenderableRuntime());
    REQUIRE_FALSE(reload.reloaded);
    CHECK(hasDiagnostic(reload.diagnostics, "runtime.chart.renderable_resources_unsupported"));
    CHECK(session.objectCount() == 1);

    const auto originalAfter = session.findEntity({"object.original"});
    REQUIRE(originalAfter.has_value());
    REQUIRE(originalAfter->has_value());
    CHECK(*originalBefore == *originalAfter);
}

TEST_CASE("RuntimeSession reload publishes a complete replacement", "[runtime][session][reload]") {
    cuexis::runtime::RuntimeSession session;
    auto prepared = session.prepare(singleObjectRuntime("object.original"));
    REQUIRE(prepared.hasValue());
    REQUIRE(session.commit(std::move(*prepared.prepared)).has_value());

    const auto reload = session.reload(singleObjectRuntime("object.replacement"));
    REQUIRE(reload.reloaded);
    CHECK_FALSE(reload.diagnostics.hasErrors());
    CHECK(session.objectCount() == 1);

    const auto oldEntity = session.findEntity({"object.original"});
    const auto newEntity = session.findEntity({"object.replacement"});
    REQUIRE(oldEntity.has_value());
    REQUIRE(newEntity.has_value());
    CHECK_FALSE(oldEntity->has_value());
    CHECK(newEntity->has_value());
}

TEST_CASE("Resource reload releases the old Scope and publishes usable replacement resources",
          "[runtime][session][assets][reload]") {
    cuexis::assets::ResourceManager resources{resourceDatabase()};
    cuexis::runtime::RuntimeSession session{resources};
    auto prepared = session.prepare(supportedRenderableRuntime());
    REQUIRE(prepared.hasValue());
    REQUIRE(session.commit(std::move(*prepared.prepared)).has_value());

    const auto entity = session.findEntity({"object.renderable"});
    REQUIRE(entity.has_value());
    REQUIRE(entity->has_value());
    const auto oldHandles = session.withWorld([&](const cuexis::world::World& world) {
        return world.withRegistry([&](const entt::registry& registry) {
            return registry.get<cuexis::render::RenderableComponent>(**entity);
        });
    });
    REQUIRE(oldHandles.has_value());
    CHECK(resources.metrics().strongReferences == 3);

    const auto reload = session.reload(replacementRenderableRuntime());
    REQUIRE(reload.reloaded);
    CHECK_FALSE(reload.diagnostics.hasErrors());
    CHECK(session.resourceCount() == 3);

    const auto replacementEntity = session.findEntity({"object.renderable"});
    REQUIRE(replacementEntity.has_value());
    REQUIRE(replacementEntity->has_value());
    const auto replacementHandles = session.withWorld([&](const cuexis::world::World& world) {
        return world.withRegistry([&](const entt::registry& registry) {
            return registry.get<cuexis::render::RenderableComponent>(**replacementEntity);
        });
    });
    REQUIRE(replacementHandles.has_value());

    const auto metrics = resources.metrics();
    CHECK(metrics.ready == 3);
    CHECK(metrics.strongReferences == 3);

    const auto oldMesh = resources.get(oldHandles->mesh);
    const auto oldMaterial = resources.get(oldHandles->material);
    REQUIRE_FALSE(oldMesh.has_value());
    REQUIRE_FALSE(oldMaterial.has_value());
    CHECK(oldMesh.error().code() == "assets.resource.stale_handle");
    CHECK(oldMaterial.error().code() == "assets.resource.stale_handle");

    const auto replacementMesh = resources.get(replacementHandles->mesh);
    const auto replacementMaterial = resources.get(replacementHandles->material);
    REQUIRE(replacementMesh.has_value());
    REQUIRE(replacementMaterial.has_value());
    CHECK((*replacementMesh)->id.value == "mesh.replacement");
    CHECK((*replacementMaterial)->id.value == "material.replacement");
}

TEST_CASE("RuntimeSession unload destroys World before clearing the session",
          "[runtime][session]") {
    cuexis::runtime::RuntimeSession session;
    auto prepared = session.prepare(singleObjectRuntime("object.only"));
    REQUIRE(prepared.hasValue());
    REQUIRE(session.commit(std::move(*prepared.prepared)).has_value());

    REQUIRE(session.unload().has_value());
    CHECK(session.empty());
    CHECK(session.objectCount() == 0);

    const auto entity = session.findEntity({"object.only"});
    REQUIRE_FALSE(entity.has_value());
    CHECK(entity.error().code() == "runtime.session.empty");
}

TEST_CASE("RuntimeSession rejects invalid parent indices without publishing",
          "[runtime][session][rollback]") {
    auto invalid = singleObjectRuntime("object.invalid_parent");
    invalid.objects[0].parentIndex = 4;

    cuexis::runtime::RuntimeSession session;
    const auto prepared = session.prepare(std::move(invalid));
    REQUIRE_FALSE(prepared.hasValue());
    CHECK(hasDiagnostic(prepared.diagnostics, "runtime.chart.invalid_parent_index"));
    CHECK(session.empty());
}

TEST_CASE("RuntimeSession rejects non-deterministic Runtime object ordering",
          "[runtime][determinism]") {
    auto unsorted = hierarchyRuntime();
    std::swap(unsorted.objects[0], unsorted.objects[1]);
    unsorted.objects[0].parentIndex = std::nullopt;
    unsorted.objects[1].parentIndex = 0;

    cuexis::runtime::RuntimeSession session;
    const auto prepared = session.prepare(std::move(unsorted));
    REQUIRE_FALSE(prepared.hasValue());
    CHECK(hasDiagnostic(prepared.diagnostics, "runtime.chart.objects_not_strictly_sorted"));
    CHECK(session.empty());
}

TEST_CASE("RuntimeSession preparation rejects a worker thread", "[runtime][thread]") {
    cuexis::runtime::RuntimeSession session;
    auto worker = std::async(std::launch::async,
                             [&session] { return session.prepare(singleObjectRuntime("object")); });
    const auto prepared = worker.get();

    REQUIRE_FALSE(prepared.hasValue());
    CHECK(hasDiagnostic(prepared.diagnostics, "runtime.session.not_owner_thread"));
    CHECK(session.empty());
}

TEST_CASE("RuntimeSession instantiation is deterministic across separate Worlds",
          "[runtime][determinism]") {
    cuexis::runtime::RuntimeSession first;
    cuexis::runtime::RuntimeSession second;
    auto firstPrepared = first.prepare(hierarchyRuntime());
    auto secondPrepared = second.prepare(hierarchyRuntime());
    REQUIRE(firstPrepared.hasValue());
    REQUIRE(secondPrepared.hasValue());
    REQUIRE(first.commit(std::move(*firstPrepared.prepared)).has_value());
    REQUIRE(second.commit(std::move(*secondPrepared.prepared)).has_value());

    const auto firstChild = first.findEntity({"object.child"});
    const auto secondChild = second.findEntity({"object.child"});
    REQUIRE(firstChild.has_value());
    REQUIRE(secondChild.has_value());
    REQUIRE(firstChild->has_value());
    REQUIRE(secondChild->has_value());

    const auto firstMatrix = first.withWorld([&](const cuexis::world::World& world) {
        return world.withRegistry([&](const entt::registry& registry) {
            return registry.get<cuexis::world::WorldTransformComponent>(**firstChild).matrix;
        });
    });
    const auto secondMatrix = second.withWorld([&](const cuexis::world::World& world) {
        return world.withRegistry([&](const entt::registry& registry) {
            return registry.get<cuexis::world::WorldTransformComponent>(**secondChild).matrix;
        });
    });
    REQUIRE(firstMatrix.has_value());
    REQUIRE(secondMatrix.has_value());
    CHECK(cuexis::core::nearlyEqual(*firstMatrix, *secondMatrix));
}

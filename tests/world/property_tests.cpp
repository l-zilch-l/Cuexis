#include <cuexis/world/property.hpp>
#include <cuexis/world/resolver_access.hpp>
#include <cuexis/world/world.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <entt/entity/registry.hpp>

#include <array>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace {

using cuexis::core::Quat;
using cuexis::core::Vec3;
using cuexis::world::PropertyId;
using cuexis::world::PropertyWrite;
using cuexis::world::TransformComponent;
using cuexis::world::TransformPropertyResolver;
using cuexis::world::World;

template <typename WorldType, typename Callback>
auto registryValue(WorldType& world, Callback&& callback) {
    auto result = world.withRegistry(std::forward<Callback>(callback));
    REQUIRE(result.has_value());
    return std::move(*result);
}

template <typename Callback> void registryAction(World& world, Callback&& callback) {
    REQUIRE(world.withRegistry(std::forward<Callback>(callback)).has_value());
}

TEST_CASE("Transform resolver rebuilds every frame from the captured baseline",
          "[world][property][determinism]") {
    World world;
    entt::entity entity{entt::null};
    registryAction(world, [&](entt::registry& registry) {
        entity = registry.create();
        registry.emplace<TransformComponent>(entity, Vec3{1.0F, 2.0F, 3.0F},
                                             Quat{0.0F, 0.0F, 0.0F, 1.0F}, Vec3{1.0F, 1.0F, 1.0F});
    });

    auto resolverResult = TransformPropertyResolver::capture(world);
    REQUIRE(resolverResult.has_value());
    auto resolver = std::move(*resolverResult);

    cuexis::world::PropertyWriteBuffer writes;
    REQUIRE(writes.push(PropertyWrite{entity, PropertyId::TransformPositionX, 9.0}).has_value());
    REQUIRE(resolver.prepare(writes.writes()).has_value());
    REQUIRE(resolver.commit(world).has_value());
    auto first = registryValue(world, [&](const entt::registry& registry) {
        return registry.get<TransformComponent>(entity).position;
    });
    CHECK(first.x == Catch::Approx(9.0F));

    writes.clear();
    REQUIRE(writes.push(PropertyWrite{entity, PropertyId::TransformPositionY, 8.0}).has_value());
    REQUIRE(resolver.prepare(writes.writes()).has_value());
    REQUIRE(resolver.commit(world).has_value());
    auto second = registryValue(world, [&](const entt::registry& registry) {
        return registry.get<TransformComponent>(entity).position;
    });
    CHECK(second.x == Catch::Approx(1.0F));
    CHECK(second.y == Catch::Approx(8.0F));
    CHECK(second.z == Catch::Approx(3.0F));
}

TEST_CASE("Transform resolver validates all writes before commit", "[world][property][rollback]") {
    World world;
    entt::entity entity{entt::null};
    registryAction(world, [&](entt::registry& registry) {
        entity = registry.create();
        registry.emplace<TransformComponent>(entity);
    });
    auto resolverResult = TransformPropertyResolver::capture(world);
    REQUIRE(resolverResult.has_value());
    auto resolver = std::move(*resolverResult);
    cuexis::world::PropertyWriteBuffer writes;
    REQUIRE(writes.push(PropertyWrite{entity, PropertyId::TransformPositionX, 4.0}).has_value());
    REQUIRE(writes
                .push(PropertyWrite{entity, PropertyId::TransformRotation,
                                    Quat{0.0F, 0.0F, 0.0F, 2.0F}})
                .has_value());
    auto result = resolver.prepare(writes.writes());
    REQUIRE_FALSE(result.has_value());
    const auto transform = registryValue(world, [&](const entt::registry& registry) {
        return registry.get<TransformComponent>(entity);
    });
    CHECK(transform.position == Vec3{});
    CHECK(transform.rotation == Quat{});
}

TEST_CASE("Animation write budget multiply is overflow-safe", "[world][property][limits][s4-g]") {
    CHECK(cuexis::world::requiredAnimationWrites(0) == 0);
    CHECK(cuexis::world::requiredAnimationWrites(1) == cuexis::world::propertyCount);
    CHECK(cuexis::world::requiredAnimationWrites(60000) ==
          cuexis::world::maxPropertyWritesPerFrame);
    CHECK(cuexis::world::animationWriteBudgetFits(60000, cuexis::world::maxPropertyWritesPerFrame));
    CHECK_FALSE(
        cuexis::world::animationWriteBudgetFits(60001, cuexis::world::maxPropertyWritesPerFrame));
    const auto overflowCount =
        (std::numeric_limits<std::size_t>::max() / cuexis::world::propertyCount) + 1;
    CHECK_FALSE(cuexis::world::requiredAnimationWrites(overflowCount).has_value());
    CHECK_FALSE(cuexis::world::animationWriteBudgetFits(overflowCount,
                                                        cuexis::world::maxPropertyWritesPerFrame));
}

TEST_CASE("PropertyWriteBuffer enforces its configured budget", "[world][property][limits]") {
    cuexis::world::PropertyWriteBuffer writes{1};
    REQUIRE(writes.push(PropertyWrite{entt::entity{1}, PropertyId::TransformPositionX, 1.0})
                .has_value());
    const auto result =
        writes.push(PropertyWrite{entt::entity{1}, PropertyId::TransformPositionY, 2.0});
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == "world.property.write_limit");
}

TEST_CASE("Transform resolver rollback only restores entities written in the current frame",
          "[world][property][sparse]") {
    World world;
    entt::entity first{entt::null};
    entt::entity second{entt::null};
    registryAction(world, [&](entt::registry& registry) {
        first = registry.create();
        second = registry.create();
        registry.emplace<TransformComponent>(first);
        registry.emplace<TransformComponent>(second);
    });
    auto captured = TransformPropertyResolver::capture(world);
    REQUIRE(captured.has_value());
    auto resolver = std::move(*captured);

    const PropertyWrite firstWrite{first, PropertyId::TransformPositionX, 3.0};
    const auto firstPrepared = resolver.prepare(std::span{&firstWrite, 1});
    if (!firstPrepared) {
        UNSCOPED_INFO(firstPrepared.error().code());
    }
    REQUIRE(firstPrepared.has_value());
    REQUIRE(resolver.commit(world).has_value());

    const PropertyWrite secondWrite{second, PropertyId::TransformPositionY, 4.0};
    REQUIRE(resolver.prepare(std::span{&secondWrite, 1}).has_value());
    REQUIRE(resolver.commit(world).has_value());
    resolver.rollback(world);

    const auto positions = registryValue(world, [&](const entt::registry& registry) {
        return std::pair{registry.get<TransformComponent>(first).position,
                         registry.get<TransformComponent>(second).position};
    });
    CHECK(positions.first.x == Catch::Approx(3.0F));
    CHECK(positions.second == Vec3{});
}

TEST_CASE("Property resolver applies layers in Initial Behavior Animation Override order",
          "[world][property][s4-d]") {
    World world;
    entt::entity entity{entt::null};
    registryAction(world, [&](entt::registry& registry) {
        entity = registry.create();
        registry.emplace<TransformComponent>(entity, Vec3{1.0F, 0.0F, 0.0F},
                                             Quat{0.0F, 0.0F, 0.0F, 1.0F}, Vec3{1.0F, 1.0F, 1.0F});
    });
    auto captured = cuexis::world::PropertyResolver::capture(world);
    REQUIRE(captured.has_value());
    auto resolver = std::move(*captured);

    const PropertyWrite behavior{entity, PropertyId::TransformPositionX, 2.0};
    const PropertyWrite animation{entity, PropertyId::TransformPositionX, 3.0};
    cuexis::world::OverrideToken host{
        .id = {.value = 1},
        .kind = cuexis::world::OverrideKind::Host,
        .ownerId = "host",
        .priority = 1,
        .propertyMask = cuexis::world::propertyBit(PropertyId::TransformPositionX),
        .lifetime = {},
        .writes = {{.entity = entity, .property = PropertyId::TransformPositionX, .value = 4.0}},
    };
    cuexis::world::OverrideToken preview{
        .id = {.value = 2},
        .kind = cuexis::world::OverrideKind::StudioPreview,
        .ownerId = "studio",
        .priority = 1,
        .propertyMask = cuexis::world::propertyBit(PropertyId::TransformPositionX),
        .lifetime = {},
        .writes = {{.entity = entity, .property = PropertyId::TransformPositionX, .value = 5.0}},
    };

    resolver.beginFrame();
    REQUIRE(
        resolver.applyLayer(std::span{&behavior, 1}, cuexis::world::PropertyLayer::Behavior, true)
            .has_value());
    REQUIRE(
        resolver.applyLayer(std::span{&animation, 1}, cuexis::world::PropertyLayer::Animation, true)
            .has_value());
    REQUIRE(resolver.applyOverrides(std::span{&host, 1}, cuexis::world::PropertyLayer::HostOverride)
                .has_value());
    REQUIRE(resolver
                .applyOverrides(std::span{&preview, 1},
                                cuexis::world::PropertyLayer::StudioPreviewOverride)
                .has_value());
    REQUIRE(resolver.finalize().has_value());
    REQUIRE(resolver.commit(world).has_value());

    CHECK(std::get<double>(*resolver.baselineValue(entity, PropertyId::TransformPositionX)) ==
          Catch::Approx(1.0));
    CHECK(std::get<double>(*resolver.layerValue(entity, PropertyId::TransformPositionX,
                                                cuexis::world::PropertyLayer::Behavior)) ==
          Catch::Approx(2.0));
    CHECK(std::get<double>(*resolver.layerValue(entity, PropertyId::TransformPositionX,
                                                cuexis::world::PropertyLayer::Animation)) ==
          Catch::Approx(3.0));
    CHECK(std::get<double>(*resolver.layerValue(entity, PropertyId::TransformPositionX,
                                                cuexis::world::PropertyLayer::HostOverride)) ==
          Catch::Approx(4.0));
    CHECK(std::get<double>(*resolver.resolvedValue(entity, PropertyId::TransformPositionX)) ==
          Catch::Approx(5.0));
    CHECK(resolver.sourceLayer(entity, PropertyId::TransformPositionX) ==
          cuexis::world::PropertyLayer::StudioPreviewOverride);
}

TEST_CASE("Same-priority override writes discard the property and keep the lower layer",
          "[world][property][override][s4-d]") {
    World world;
    entt::entity entity{entt::null};
    registryAction(world, [&](entt::registry& registry) {
        entity = registry.create();
        registry.emplace<TransformComponent>(entity, Vec3{1.0F, 0.0F, 0.0F});
    });
    auto captured = cuexis::world::PropertyResolver::capture(world);
    REQUIRE(captured.has_value());
    auto resolver = std::move(*captured);

    const PropertyWrite behavior{entity, PropertyId::TransformPositionX, 2.0};
    cuexis::world::OverrideToken left{
        .id = {.value = 2},
        .kind = cuexis::world::OverrideKind::Host,
        .ownerId = "left",
        .priority = 7,
        .propertyMask = cuexis::world::propertyBit(PropertyId::TransformPositionX),
        .lifetime = {},
        .writes = {{.entity = entity, .property = PropertyId::TransformPositionX, .value = 8.0}},
    };
    cuexis::world::OverrideToken right{
        .id = {.value = 1},
        .kind = cuexis::world::OverrideKind::Host,
        .ownerId = "right",
        .priority = 7,
        .propertyMask = cuexis::world::propertyBit(PropertyId::TransformPositionX),
        .lifetime = {},
        .writes = {{.entity = entity, .property = PropertyId::TransformPositionX, .value = 9.0}},
    };

    resolver.beginFrame();
    REQUIRE(
        resolver.applyLayer(std::span{&behavior, 1}, cuexis::world::PropertyLayer::Behavior, true)
            .has_value());
    REQUIRE(
        resolver.applyOverrides(std::array{left, right}, cuexis::world::PropertyLayer::HostOverride)
            .has_value());
    REQUIRE(resolver.finalize().has_value());
    CHECK(resolver.hadConflict(entity, PropertyId::TransformPositionX));
    REQUIRE(resolver.conflicts().size() == 1);
    CHECK(std::get<double>(*resolver.resolvedValue(entity, PropertyId::TransformPositionX)) ==
          Catch::Approx(2.0));
    CHECK(resolver.sourceLayer(entity, PropertyId::TransformPositionX) ==
          cuexis::world::PropertyLayer::Behavior);
}

TEST_CASE("Property resolver registers camera and appearance baselines without render headers",
          "[world][property][appearance][s4-d]") {
    World world;
    entt::entity entity{entt::null};
    registryAction(world, [&](entt::registry& registry) { entity = registry.create(); });
    cuexis::world::PropertyResolver resolver;
    REQUIRE(resolver.registerBaseline(entity, PropertyId::CameraFovY, 60.0).has_value());
    REQUIRE(resolver.registerBaseline(entity, PropertyId::RenderVisible, true).has_value());
    REQUIRE(resolver.registerBaseline(entity, PropertyId::RenderMaterial, std::string{"mat.a"})
                .has_value());
    REQUIRE(resolver.registerBaseline(entity, PropertyId::MaterialOpacity, 1.0).has_value());
    REQUIRE(resolver.registerBaseline(entity, PropertyId::MaterialTint, Vec3{1.0F, 1.0F, 1.0F})
                .has_value());

    const PropertyWrite fov{entity, PropertyId::CameraFovY, 75.0};
    const PropertyWrite visible{entity, PropertyId::RenderVisible, false};
    resolver.beginFrame();
    REQUIRE(
        resolver.applyLayer(std::array{fov, visible}, cuexis::world::PropertyLayer::Behavior, true)
            .has_value());
    REQUIRE(resolver.finalize().has_value());
    CHECK(std::get<double>(*resolver.resolvedValue(entity, PropertyId::CameraFovY)) ==
          Catch::Approx(75.0));
    CHECK(std::get<bool>(*resolver.resolvedValue(entity, PropertyId::RenderVisible)) == false);
    CHECK(std::get<std::string>(*resolver.resolvedValue(entity, PropertyId::RenderMaterial)) ==
          "mat.a");
}

TEST_CASE("Appearance reads preserve sparse baselines and layer fallback",
          "[world][property][appearance][r3]") {
    using cuexis::world::PropertyLayer;
    using cuexis::world::detail::ResolverAccess;
    const auto entity = static_cast<entt::entity>(37);
    const auto camera = static_cast<entt::entity>(2);
    cuexis::world::PropertyResolver resolver;
    REQUIRE(resolver.registerBaseline(camera, PropertyId::CameraFovY, 60.0));
    REQUIRE(resolver.registerBaseline(entity, PropertyId::RenderVisible, true));
    REQUIRE(resolver.registerBaseline(entity, PropertyId::RenderMaterial, std::string{"initial"}));
    REQUIRE(resolver.registerBaseline(entity, PropertyId::MaterialOpacity, 1.0));
    REQUIRE(resolver.registerBaseline(entity, PropertyId::MaterialTint, Vec3{1, 1, 1}));
    const auto checkRead = [&](entt::entity target) {
        const auto values = ResolverAccess::resolvedAppearance(resolver, target);
        constexpr std::array properties{PropertyId::RenderVisible, PropertyId::RenderMaterial,
                                        PropertyId::MaterialOpacity, PropertyId::MaterialTint};
        for (std::size_t i = 0; i < properties.size(); ++i)
            CHECK(values[i] == resolver.resolvedValuePtr(target, properties[i]));
        return values;
    };
    for (const auto target :
         {camera, static_cast<entt::entity>(entt::null), static_cast<entt::entity>(38)})
        for (const auto* value : checkRead(target))
            CHECK(value == nullptr);
    const auto sparse = static_cast<entt::entity>(13);
    REQUIRE(resolver.registerBaseline(sparse, PropertyId::RenderMaterial, true));
    REQUIRE(resolver.registerBaseline(sparse, PropertyId::MaterialOpacity, 0.5));
    const auto sparseValues = checkRead(sparse);
    CHECK(sparseValues[0] == nullptr);
    CHECK(std::get<bool>(*sparseValues[1]));
    CHECK(std::get<double>(*sparseValues[2]) == 0.5);
    CHECK(sparseValues[3] == nullptr);
    CHECK(std::get<bool>(*checkRead(entity)[0]));
    const std::string material(256, 'm');
    const std::array writes{
        PropertyWrite{entity, PropertyId::RenderVisible, false},
        PropertyWrite{entity, PropertyId::RenderMaterial, std::string_view{material}},
        PropertyWrite{entity, PropertyId::MaterialOpacity, 0.25},
        PropertyWrite{entity, PropertyId::MaterialTint, Vec3{0, 0.5F, 1}}};
    resolver.beginFrame();
    REQUIRE(resolver.applyLayer(writes, PropertyLayer::Behavior, true));
    REQUIRE(resolver.finalize());
    auto values = checkRead(entity);
    CHECK_FALSE(std::get<bool>(*values[0]));
    CHECK(std::get<std::string>(*values[1]) == material);
    CHECK(std::get<double>(*values[2]) == 0.25);
    CHECK(std::get<Vec3>(*values[3]) == Vec3{0, 0.5F, 1});
    resolver.beginFrame();
    values = checkRead(entity);
    CHECK(std::get<bool>(*values[0]));
    CHECK(std::get<std::string>(*values[1]) == "initial");
    CHECK(std::get<double>(*values[2]) == 1.0);
    CHECK(std::get<Vec3>(*values[3]) == Vec3{1, 1, 1});
    const PropertyWrite invalid{entity, PropertyId::MaterialOpacity, 2.0};
    const auto rejected =
        resolver.applyLayer(std::span{&invalid, 1}, PropertyLayer::Behavior, true);
    REQUIRE_FALSE(rejected);
    CHECK(std::get<double>(*checkRead(entity)[2]) == 1.0);
}

TEST_CASE("Touched membership survives resolver copies and aborted frames",
          "[world][property][appearance][r3]") {
    using cuexis::world::PropertyLayer;
    using cuexis::world::detail::ResolverAccess;
    cuexis::world::PropertyResolver resolver;
    constexpr std::array entities{entt::entity{2}, entt::entity{37}, entt::entity{101}};
    for (const auto entity : entities) {
        REQUIRE(resolver.registerBaseline(entity, PropertyId::RenderVisible, true));
        REQUIRE(resolver.registerBaseline(entity, PropertyId::MaterialOpacity, 1.0));
    }
    // First-touch order differs from entity order; multiple properties and layers reuse entries.
    const std::array writes{PropertyWrite{entities[2], PropertyId::RenderVisible, false},
                            PropertyWrite{entities[0], PropertyId::MaterialOpacity, 0.25},
                            PropertyWrite{entities[2], PropertyId::MaterialOpacity, 0.5},
                            PropertyWrite{entities[1], PropertyId::RenderVisible, false}};
    resolver.beginFrame();
    REQUIRE(resolver.applyLayer(writes, PropertyLayer::Behavior, true));
    const PropertyWrite higher{entities[2], PropertyId::MaterialOpacity, 0.75};
    REQUIRE(resolver.applyLayer(std::span{&higher, 1}, PropertyLayer::Animation, true));
    REQUIRE(resolver.finalize());
    auto copy = resolver;
    auto moved = std::move(copy);
    std::vector<cuexis::world::detail::TransformWrite> transforms;
    REQUIRE(ResolverAccess::prepareTransforms(resolver, transforms));
    ResolverAccess::installed(resolver);
    for (auto* current : {&resolver, &moved}) {
        // Both committed and only-prepared copies must reset their own touched entries.
        current->beginFrame();
        for (const auto entity : entities) {
            CHECK(std::get<bool>(*current->resolvedValue(entity, PropertyId::RenderVisible)));
            CHECK(std::get<double>(*current->resolvedValue(entity, PropertyId::MaterialOpacity)) ==
                  1.0);
        }
        REQUIRE(current->applyBaseProperty(entities[0], PropertyId::MaterialOpacity, 0.8));
        REQUIRE(current->applyLayer(writes, PropertyLayer::Behavior, true));
        const PropertyWrite invalid{entities[1], PropertyId::MaterialOpacity, 2.0};
        REQUIRE_FALSE(current->applyLayer(std::span{&invalid, 1}, PropertyLayer::Animation, true));
        current->beginFrame();
        REQUIRE(current->applyLayer(writes, PropertyLayer::Behavior, true));
        REQUIRE(current->finalize());
        REQUIRE(ResolverAccess::prepareTransforms(*current, transforms));
        ResolverAccess::installed(*current);
        current->beginFrame();
        CHECK(std::get<double>(*current->resolvedValue(entities[0], PropertyId::MaterialOpacity)) ==
              0.8);
        CHECK(std::get<bool>(*current->resolvedValue(entities[2], PropertyId::RenderVisible)));
    }
}

TEST_CASE("Late baseline insertion preserves touched indices across subsequent frames",
          "[world][property][appearance][r3]") {
    using cuexis::world::PropertyLayer;
    using cuexis::world::detail::ResolverAccess;
    for (const bool installed : {false, true}) {
        cuexis::world::PropertyResolver resolver;
        const entt::entity high{101}, low{2};
        REQUIRE(resolver.registerBaseline(high, PropertyId::MaterialOpacity, 1.0));
        resolver.beginFrame();
        const PropertyWrite first{high, PropertyId::MaterialOpacity, 0.5};
        REQUIRE(resolver.applyLayer(std::span{&first, 1}, PropertyLayer::Behavior, true));
        REQUIRE(resolver.finalize());
        std::vector<cuexis::world::detail::TransformWrite> transforms;
        REQUIRE(ResolverAccess::prepareTransforms(resolver, transforms));
        if (installed)
            ResolverAccess::installed(resolver);
        REQUIRE(resolver.registerBaseline(low, PropertyId::MaterialOpacity, 1.0));
        resolver.beginFrame();
        const PropertyWrite second{high, PropertyId::MaterialOpacity, 0.25};
        REQUIRE(resolver.applyLayer(std::span{&second, 1}, PropertyLayer::Animation, true));
        REQUIRE(resolver.finalize());
        REQUIRE(ResolverAccess::prepareTransforms(resolver, transforms));
        ResolverAccess::installed(resolver);
        resolver.beginFrame();
        CHECK(std::get<double>(*resolver.resolvedValue(high, PropertyId::MaterialOpacity)) == 1.0);
    }
}

TEST_CASE("Late baseline insertion keeps saved transform rollback indices",
          "[world][property][rollback][r3]") {
    World world;
    entt::entity low{}, high{};
    registryAction(world, [&](entt::registry& registry) {
        low = registry.create();
        high = registry.create();
        registry.emplace<TransformComponent>(high, Vec3{1, 2, 3}, Quat{}, Vec3{1, 1, 1});
    });
    auto captured = TransformPropertyResolver::capture(world);
    REQUIRE(captured);
    auto resolver = std::move(*captured);
    const PropertyWrite write{high, PropertyId::TransformPositionX, 9.0};
    REQUIRE(resolver.prepare(std::span{&write, 1}));
    REQUIRE(resolver.commit(world));
    CHECK(registryValue(world, [&](const entt::registry& registry) {
              return registry.get<TransformComponent>(high).position.x;
          }) == 9.0F);
    // Insertion happens after commit, so rollback must consume the shifted saved index directly.
    REQUIRE(resolver.registerBaseline(low, PropertyId::RenderVisible, true));
    resolver.rollback(world);
    CHECK(registryValue(world, [&](const entt::registry& registry) {
              return registry.get<TransformComponent>(high).position;
          }) == Vec3{1, 2, 3});
    const PropertyWrite later{high, PropertyId::TransformPositionY, 8.0};
    REQUIRE(resolver.prepare(std::span{&later, 1}));
    REQUIRE(resolver.commit(world));
    CHECK(registryValue(world, [&](const entt::registry& registry) {
              return registry.get<TransformComponent>(high).position;
          }) == Vec3{1, 8, 3});
}

TEST_CASE("Base property command replaces the captured baseline and increments revision",
          "[world][property][base][s4-d]") {
    World world;
    entt::entity entity{entt::null};
    registryAction(world, [&](entt::registry& registry) {
        entity = registry.create();
        registry.emplace<TransformComponent>(entity, Vec3{1.0F, 2.0F, 3.0F});
    });
    auto captured = cuexis::world::PropertyResolver::capture(world);
    REQUIRE(captured.has_value());
    auto resolver = std::move(*captured);
    CHECK(resolver.baseRevision() == 0);
    REQUIRE(resolver.applyBaseProperty(entity, PropertyId::TransformPositionX, 4.0).has_value());
    CHECK(resolver.baseRevision() == 1);
    CHECK(std::get<double>(*resolver.baselineValue(entity, PropertyId::TransformPositionX)) ==
          Catch::Approx(4.0));

    resolver.beginFrame();
    REQUIRE(resolver.finalize().has_value());
    REQUIRE(resolver.commit(world).has_value());
    const auto position = registryValue(world, [&](const entt::registry& registry) {
        return registry.get<TransformComponent>(entity).position;
    });
    CHECK(position.x == Catch::Approx(4.0F));
    CHECK(position.y == Catch::Approx(2.0F));
}

TEST_CASE("Base property command rejects invalid values without changing revision",
          "[world][property][base][s4-d]") {
    World world;
    entt::entity entity{entt::null};
    registryAction(world, [&](entt::registry& registry) {
        entity = registry.create();
        registry.emplace<TransformComponent>(entity, Vec3{1.0F, 2.0F, 3.0F});
    });
    auto captured = cuexis::world::PropertyResolver::capture(world);
    REQUIRE(captured.has_value());
    auto resolver = std::move(*captured);
    const auto failed = resolver.applyBaseProperty(entity, PropertyId::TransformPositionX,
                                                   std::numeric_limits<double>::quiet_NaN());
    REQUIRE_FALSE(failed.has_value());
    CHECK(failed.error().code() == "world.property.value_invalid");
    CHECK(resolver.baseRevision() == 0);
    CHECK(std::get<double>(*resolver.baselineValue(entity, PropertyId::TransformPositionX)) ==
          Catch::Approx(1.0));

    const auto missing = resolver.applyBaseProperty(entity, PropertyId::CameraFovY, 75.0);
    REQUIRE_FALSE(missing.has_value());
    CHECK(missing.error().code() == "world.property.baseline_missing");
    CHECK(resolver.baseRevision() == 0);
}

TEST_CASE("PropertyWriteBuffer owns material string views after the source is destroyed",
          "[world][property][strings][s4-d]") {
    cuexis::world::PropertyWriteBuffer writes;
    {
        const std::string material{"mat.owned"};
        REQUIRE(writes
                    .push(PropertyWrite{entt::entity{1}, PropertyId::RenderMaterial,
                                        std::string_view{material}})
                    .has_value());
    }
    const auto* view = std::get_if<std::string_view>(&writes.writes().front().value);
    REQUIRE(view != nullptr);
    CHECK(*view == "mat.owned");
}

} // namespace

#include <cuexis/chart/chart_loader.hpp>
#include <cuexis/chart/chart_migrator.hpp>
#include <cuexis/chart/chart_runtime.hpp>
#include <cuexis/render/camera_component.hpp>
#include <cuexis/runtime/runtime_session.hpp>
#include <cuexis/world/components.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

[[nodiscard]] auto readFile(const std::filesystem::path& path) -> std::string {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw std::runtime_error{"Could not read migration fixture: " + path.string()};
    }
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

[[nodiscard]] auto prepareSession(const cuexis::chart::ChartDocument& document)
    -> std::unique_ptr<cuexis::runtime::RuntimeSession> {
    auto compiled = cuexis::chart::ChartCompiler::compile(document);
    REQUIRE(compiled.hasValue());
    auto session = std::make_unique<cuexis::runtime::RuntimeSession>();
    auto prepared = session->prepare(std::move(*compiled.runtime));
    REQUIRE(prepared.hasValue());
    REQUIRE(session->commit(std::move(*prepared.prepared)).has_value());
    return session;
}

struct ObservedState final {
    cuexis::world::TransformComponent transform;
    std::optional<cuexis::render::CameraComponent> camera;
};

[[nodiscard]] auto observe(cuexis::runtime::RuntimeSession& session, std::string_view id)
    -> ObservedState {
    const auto entity = session.findEntity({std::string{id}});
    REQUIRE(entity.has_value());
    REQUIRE(entity->has_value());
    const auto state = session.withWorld([&](const cuexis::world::World& world) {
        return world.withRegistry([&](const entt::registry& registry) {
            const auto* camera = registry.try_get<cuexis::render::CameraComponent>(**entity);
            return ObservedState{
                .transform = registry.get<cuexis::world::TransformComponent>(**entity),
                .camera = camera == nullptr ? std::nullopt : std::optional{*camera},
            };
        });
    });
    REQUIRE(state.has_value());
    return *state;
}

void checkNear(const ObservedState& actual, const ObservedState& expected) {
    constexpr double tolerance = 1e-6;
    CHECK(actual.transform.position.x ==
          Catch::Approx(expected.transform.position.x).margin(tolerance));
    CHECK(actual.transform.position.y ==
          Catch::Approx(expected.transform.position.y).margin(tolerance));
    CHECK(actual.transform.position.z ==
          Catch::Approx(expected.transform.position.z).margin(tolerance));
    CHECK(actual.transform.scale.x == Catch::Approx(expected.transform.scale.x).margin(tolerance));
    CHECK(actual.transform.scale.y == Catch::Approx(expected.transform.scale.y).margin(tolerance));
    CHECK(actual.transform.scale.z == Catch::Approx(expected.transform.scale.z).margin(tolerance));
    CHECK(actual.transform.rotation.x ==
          Catch::Approx(expected.transform.rotation.x).margin(tolerance));
    CHECK(actual.transform.rotation.y ==
          Catch::Approx(expected.transform.rotation.y).margin(tolerance));
    CHECK(actual.transform.rotation.z ==
          Catch::Approx(expected.transform.rotation.z).margin(tolerance));
    CHECK(actual.transform.rotation.w ==
          Catch::Approx(expected.transform.rotation.w).margin(tolerance));
    REQUIRE(actual.camera.has_value() == expected.camera.has_value());
    if (actual.camera) {
        CHECK(actual.camera->fovY == Catch::Approx(expected.camera->fovY).margin(tolerance));
    }
}

} // namespace

TEST_CASE("Migrated v3 runtime matches v1 scalar Vec3 Quaternion and FOV sampling",
          "[runtime][stage2][migration][equivalence]") {
    const auto sourcePath = std::filesystem::path{CUEXIS_TEST_SOURCE_DIR} / "tests" / "fixtures" /
                            "stage2_migration_v1.cuexis.chart.json";
    const auto sourceJson = readFile(sourcePath);
    const auto source = cuexis::chart::ChartLoader::load(sourceJson);
    const auto migrated = cuexis::chart::ChartMigrator::migrateToV3(sourceJson);
    REQUIRE(source.hasValue());
    REQUIRE(migrated.hasValue());

    auto originalSession = prepareSession(*source.document);
    auto migratedSession = prepareSession(migrated.artifact->document);
    constexpr std::array sampleTimesMs{-500.0, 0.0,   125.0,  250.0, 375.0,
                                       500.0,  750.0, 1000.0, 2000.0};
    constexpr std::array objectIds{
        std::string_view{"019c0000-0000-7abc-8def-000000000210"},
        std::string_view{"019c0000-0000-7abc-8def-000000000211"},
        std::string_view{"019c0000-0000-7abc-8def-000000000212"},
    };

    std::uint64_t discontinuity = 1;
    for (const double chartTimeMs : sampleTimesMs) {
        REQUIRE(originalSession
                    ->update({.chartTimeMs = chartTimeMs, .timeDiscontinuityId = discontinuity})
                    .has_value());
        REQUIRE(migratedSession
                    ->update({.chartTimeMs = chartTimeMs, .timeDiscontinuityId = discontinuity})
                    .has_value());
        ++discontinuity;
        for (const auto objectId : objectIds) {
            checkNear(observe(*migratedSession, objectId), observe(*originalSession, objectId));
        }
    }
}

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cuexis/chart/behavior_candidate.hpp>
#include <cuexis/json/parse.hpp>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace {
auto source() -> cuexis::json::Value {
    std::ifstream stream{std::filesystem::path{CUEXIS_SOURCE_DIR} /
                         "tests/fixtures/chart_format_update/behavior_candidate.v5.json"};
    REQUIRE(stream.good());
    auto parsed = cuexis::json::parse(
        std::string{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}},
        {16U * 1024U * 1024U, 64, 8192});
    REQUIRE(parsed);
    return std::move(*parsed);
}
auto compile(const cuexis::json::Value& value, const cuexis::chart::ChartLimits& limits = {}) {
    auto text = cuexis::json::serialize(value);
    REQUIRE(text);
    return cuexis::chart::compileBehaviorCandidate(*text, limits);
}
} // namespace

TEST_CASE("V5 Behavior candidate retains identity and executable event timing",
          "[chart][behavior-candidate]") {
    auto value = source();
    auto result = compile(value);
    REQUIRE(result);
    CHECK(result->runtime.version == 5);
    REQUIRE(result->runtime.behaviors.size() == 1);
    CHECK(result->runtime.behaviors[0].version == 2);
    CHECK(result->runtime.behaviors[0].eventTracks[0].events[0].endBeat == 2);
    CHECK(result->metadata.objects[0].executionId == "019f0000-0000-7abc-8def-000000000411");
    auto repeated = compile(value);
    REQUIRE(repeated);
    CHECK(result->metadata.semanticIdentity == repeated->metadata.semanticIdentity);
    auto& event = (*value.find("behaviors")->array())[0].find("events")->array()->at(0);
    *event.find("endValue") = cuexis::json::Value{3.0};
    auto changed = compile(value);
    REQUIRE(changed);
    CHECK(result->metadata.semanticIdentity != changed->metadata.semanticIdentity);
    cuexis::chart::ChartLimits limits;
    limits.maxObjects = 0;
    CHECK_FALSE(compile(value, limits));
}

TEST_CASE("V5 Behavior alpha uses integers and rejects unsupported source forms",
          "[chart][behavior-candidate]") {
    auto value = source();
    auto& components = *value.find("objects")->array()->at(0).find("components")->object();
    auto renderable = cuexis::json::parse(
        R"({"version":1,"mesh":{"domain":"asset","id":"mesh"},"material":{"domain":"asset","id":"mat"},"alpha":128})",
        {8192, 64, 8192});
    REQUIRE(renderable);
    components.emplace("cuexis.renderable", std::move(*renderable));
    auto& events = *value.find("behaviors")->array()->at(0).find("events")->array();
    auto alpha = events[0];
    *alpha.find("property") = cuexis::json::Value{"render.alpha"};
    *alpha.find("startValue") = cuexis::json::Value{std::uint64_t{0}};
    *alpha.find("endValue") = cuexis::json::Value{std::uint64_t{255}};
    events.push_back(std::move(alpha));
    auto result = compile(value);
    REQUIRE(result);
    CHECK(result->runtime.objects[0].renderableOpacity == Catch::Approx(128.0 / 255.0));
    CHECK(result->metadata.resourceClosure.resources.size() == 2);
    SECTION("Fractional object alpha") {
        *components.at("cuexis.renderable").find("alpha") = cuexis::json::Value{0.5};
        CHECK_FALSE(compile(value));
    }
    SECTION("Fractional event alpha") {
        *events[1].find("endValue") = cuexis::json::Value{254.5};
        CHECK_FALSE(compile(value));
    }
    SECTION("Old Behavior version") {
        *value.find("behaviors")->array()->at(0).find("version") =
            cuexis::json::Value{std::uint64_t{1}};
        CHECK_FALSE(compile(value));
    }
    SECTION("Malformed renderable") {
        components.at("cuexis.renderable") = cuexis::json::Value{true};
        CHECK_FALSE(compile(value));
    }
    SECTION("Unknown event property") {
        *events[1].find("property") = cuexis::json::Value{"render.unregistered"};
        CHECK_FALSE(compile(value));
    }
    SECTION("Animator is not silently ignored") {
        components.emplace("cuexis.animator", cuexis::json::Value{true});
        CHECK_FALSE(compile(value));
    }
}

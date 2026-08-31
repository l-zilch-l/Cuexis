#include <cuexis/chart/chart_v4_loader.hpp>
#include <cuexis/chart/limits.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

[[nodiscard]] auto readFile(const std::filesystem::path& path) -> std::string {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw std::runtime_error{"Could not open Chart v4 fixture: " + path.string()};
    }
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

[[nodiscard]] auto hasDiagnostic(const cuexis::chart::ChartV4SourceResult& result,
                                 std::string_view code) -> bool {
    for (const auto& diagnostic : result.diagnostics.items()) {
        if (diagnostic.code() == code) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] auto findDiagnostic(const cuexis::chart::ChartV4SourceResult& result,
                                  std::string_view code, std::string_view fieldPath)
    -> const cuexis::core::Diagnostic* {
    for (const auto& diagnostic : result.diagnostics.items()) {
        if (diagnostic.code() == code && diagnostic.fieldPath() == fieldPath) {
            return &diagnostic;
        }
    }
    return nullptr;
}

[[nodiscard]] auto repeatedEmptyObjects(std::size_t count) -> std::string {
    std::string result{"["};
    for (std::size_t index = 0; index < count; ++index) {
        if (index != 0) {
            result.push_back(',');
        }
        result += "{}";
    }
    result.push_back(']');
    return result;
}

[[nodiscard]] auto makeAnimatorBudgetDocument(std::string_view groupsJson) -> std::string {
    auto result = std::string{R"({
      "format":"cuexis.chart","version":4,
      "chartId":"019f0000-0000-7abc-8def-0000000004b0","metadata":{},
      "timing":{"offsetMs":0,"defaultBpm":120,"tempoEvents":[],"stops":[]},
      "parameters":[],"templates":[],"behaviors":[],
      "animationTemplateImports":[],"animationClips":[],
      "objects":[{
        "id":"019f0000-0000-7abc-8def-0000000004b1","parent":null,
        "components":{
          "cuexis.transform":{"version":1,"position":[0,0,0],
            "rotation":[0,0,0,1],"scale":[1,1,1]},
          "cuexis.animator":{"version":1,"templateBindings":[],"layers":[{
            "layerId":"layer.limit","priority":0,"weight":1,
            "propertyMask":{"properties":["transform.scale"],"prefixes":[]},
            "blendGroups":)"};
    result += groupsJson;
    result += R"(}]}
        },
        "extensions":{}}],
      "requiredExtensions":[],"extensions":{}
    })";
    return result;
}

[[nodiscard]] auto makeLayerMaskPropertyDocument(std::string_view property) -> std::string {
    const auto groups = std::string{R"([{
      "groupId":"group.mask","mode":"override","weight":1,"instances":[]}])"};
    auto document = makeAnimatorBudgetDocument(groups);
    const auto original = std::string{R"("properties":["transform.scale"])"};
    const auto replacement = std::string{R"("properties":[")"} + std::string{property} + R"("])";
    const auto position = document.find(original);
    REQUIRE(position != std::string::npos);
    document.replace(position, original.size(), replacement);
    return document;
}

[[nodiscard]] auto makeInstanceMaskPropertyDocument(std::string_view property) -> std::string {
    auto groups = std::string{R"([{
      "groupId":"group.mask","mode":"override","weight":1,"instances":[{
        "instanceId":"instance.mask",
        "clip":{"domain":"animation","id":"animation.mask"},
        "startBeat":{"numerator":0,"denominator":1},"iterations":1,
        "fillMode":"none","weight":1,
        "propertyMask":{"properties":[")"};
    groups += property;
    groups += R"("],"prefixes":[]}}]}])";
    return makeAnimatorBudgetDocument(groups);
}

[[nodiscard]] auto makeMinimalV4Document(std::string_view parameters,
                                         std::string_view imports) -> std::string {
    std::string result = R"({
      "format":"cuexis.chart","version":4,
      "chartId":"019f0000-0000-7abc-8def-0000000004c0","metadata":{},
      "timing":{"offsetMs":0,"defaultBpm":120,"tempoEvents":[],"stops":[]},
      "parameters":)";
    result += parameters;
    result += R"(,"templates":[],"behaviors":[],
      "animationTemplateImports":)";
    result += imports;
    result += R"(,"animationClips":[],"objects":[],
      "requiredExtensions":[],"extensions":{}})";
    return result;
}

void requireInvalidMaskProperty(const cuexis::chart::ChartV4SourceResult& result,
                                std::string_view fieldPath) {
    CHECK_FALSE(result.hasValue());
    const auto* diagnostic = findDiagnostic(result, "chart.animation.mask_conflict", fieldPath);
    REQUIRE(diagnostic != nullptr);
    CHECK(diagnostic->message().find("Property mask property") != std::string_view::npos);
    CHECK(diagnostic->message().find("bounded") != std::string_view::npos);
}

} // namespace

TEST_CASE("Chart v4 Reader accepts promoted source fixtures", "[chart][v4][cfu-c1]") {
    const auto root = std::filesystem::path{CUEXIS_SOURCE_DIR} / "tests" / "fixtures" /
                      "chart_format_update" / "valid";
    for (const auto* name : {"chart_v4_animation.json", "chart_v4_cxt_template_binding.json",
                             "chart_v4_parameterized_transform.json",
                             "chart_v4_static_migration.json", "chart_v4_template_animator.json"}) {
        const auto result = cuexis::chart::ChartV4Loader::load(readFile(root / name));
        INFO(name);
        REQUIRE(result.hasValue());
        CHECK(result.document->legacyProjection.version == 4);
        CHECK_FALSE(result.document->canonicalSource.canonicalText.empty());
    }
}

TEST_CASE("Chart v4 Reader rejects promoted local semantic violations", "[chart][v4][cfu-c1]") {
    const auto root = std::filesystem::path{CUEXIS_SOURCE_DIR} / "tests" / "fixtures" /
                      "chart_format_update" / "invalid";
    const auto requireCode = [&](const char* name, std::string_view code) {
        const auto result = cuexis::chart::ChartV4Loader::load(readFile(root / name));
        INFO(name);
        CHECK_FALSE(result.hasValue());
        CHECK(hasDiagnostic(result, code));
    };

    requireCode("chart_v4_animator_deep_patch.json", "chart.patch.path_unsupported");
    requireCode("chart_v4_cxt_parameter_type.json", "chart.parameter.type_mismatch");
    requireCode("chart_v4_mask_conflict.json", "chart.animation.mask_conflict");
    requireCode("chart_v4_mask_overlap.json", "chart.animation.mask_conflict");
    requireCode("chart_v4_parameter_asset_use.json", "chart.parameter.use_not_allowed");
}

TEST_CASE("Chart v4 Reader retains effective-property rules for C2 resolution",
          "[chart][v4][cfu-c1]") {
    const auto root = std::filesystem::path{CUEXIS_SOURCE_DIR} / "tests" / "fixtures" /
                      "chart_format_update" / "invalid";
    for (const auto* name :
         {"chart_v4_additive_material.json", "chart_v4_discrete_partial_weight.json"}) {
        const auto result = cuexis::chart::ChartV4Loader::load(readFile(root / name));
        INFO(name);
        REQUIRE(result.hasValue());
    }
}

TEST_CASE("Chart v4 Reader retains imports that require project lookup in C2",
          "[chart][v4][cfu-c1]") {
    const auto root = std::filesystem::path{CUEXIS_SOURCE_DIR} / "tests" / "fixtures" /
                      "chart_format_update" / "invalid";
    for (const auto* name : {"chart_v4_cxt_missing_import.json", "chart_v4_cxt_id_mismatch.json"}) {
        const auto result = cuexis::chart::ChartV4Loader::load(readFile(root / name));
        INFO(name);
        REQUIRE(result.hasValue());
        REQUIRE(result.document->animationTemplateImports.size() == 1);
    }
}

TEST_CASE("Chart v4 Reader retains the source path of a parameter reference in a patch",
          "[chart][v4][cfu-c1]") {
    constexpr std::string_view document = R"({
      "format":"cuexis.chart","version":4,
      "chartId":"019f0000-0000-7abc-8def-0000000004a0","metadata":{},
      "timing":{"offsetMs":0,"defaultBpm":120,"tempoEvents":[],"stops":[]},
      "parameters":[{"id":"layout.x","type":"number","default":0,"constraints":{}}],
      "templates":[{
        "id":"019f0000-0000-7abc-8def-0000000004a1","extends":null,
        "prototype":{"components":{"cuexis.transform":{"version":1,
          "position":[0,0,0],"rotation":[0,0,0,1],"scale":[1,1,1]}}},
        "extensions":{}}],
      "behaviors":[],"animationTemplateImports":[],"animationClips":[],
      "objects":[{
        "id":"019f0000-0000-7abc-8def-0000000004a2","parent":null,
        "template":{"domain":"template","id":"019f0000-0000-7abc-8def-0000000004a1"},
        "overrides":[{"op":"replace","path":"/components/cuexis.transform/position",
          "value":[{"parameter":{"domain":"chart-parameter","id":"layout.x"}},0,0]}],
        "extensions":{}}],
      "requiredExtensions":[],"extensions":{}
    })";
    const auto result = cuexis::chart::ChartV4Loader::load(document);
    REQUIRE(result.hasValue());
    REQUIRE(result.document->parameterUses.size() == 1);
    CHECK(result.document->parameterUses.front().fieldPath == "$/objects/0/overrides/0/value/0");
}

TEST_CASE("Chart v4 Reader reports one original path for a deep Animator patch",
          "[chart][v4][cfu-c1]") {
    const auto path = std::filesystem::path{CUEXIS_SOURCE_DIR} / "tests" / "fixtures" /
                      "chart_format_update" / "invalid" / "chart_v4_animator_deep_patch.json";
    const auto result = cuexis::chart::ChartV4Loader::load(readFile(path));
    CHECK_FALSE(result.hasValue());

    std::size_t count = 0;
    for (const auto& diagnostic : result.diagnostics.items()) {
        if (diagnostic.code() != "chart.patch.path_unsupported") {
            continue;
        }
        ++count;
        CHECK(diagnostic.fieldPath() == "$/objects/0/overrides/0/path");
    }
    CHECK(count == 1);
}

TEST_CASE("Chart v4 Reader enforces nested Animator array budgets", "[chart][v4][cfu-c1]") {
    SECTION("blend groups") {
        const auto result = cuexis::chart::ChartV4Loader::load(
            makeAnimatorBudgetDocument(repeatedEmptyObjects(65)));
        CHECK_FALSE(result.hasValue());
        CHECK(hasDiagnostic(result, "chart.animation.generated_limit"));
    }

    SECTION("clip instances") {
        auto groups = std::string{R"([{
          "groupId":"group.limit","mode":"override","weight":1,"instances":)"};
        groups += repeatedEmptyObjects(257);
        groups += "}]";
        const auto result = cuexis::chart::ChartV4Loader::load(makeAnimatorBudgetDocument(groups));
        CHECK_FALSE(result.hasValue());
        CHECK(hasDiagnostic(result, "chart.animation.generated_limit"));
    }
}

TEST_CASE("Chart v4 Reader rejects masks without known effective properties",
          "[chart][v4][cfu-c1]") {
    const auto loadMask = [](std::string_view mask) {
        auto groups = std::string{R"([{
          "groupId":"group.mask","mode":"override","weight":1,"instances":[]}])"};
        auto document = makeAnimatorBudgetDocument(groups);
        const auto original =
            std::string{R"("propertyMask":{"properties":["transform.scale"],"prefixes":[]})"};
        const auto replacement = std::string{"\"propertyMask\":"} + std::string{mask};
        const auto position = document.find(original);
        REQUIRE(position != std::string::npos);
        document.replace(position, original.size(), replacement);
        return cuexis::chart::ChartV4Loader::load(document);
    };

    for (const auto* mask : {R"({"properties":[],"prefixes":[]})",
                             R"({"properties":["unknown.property"],"prefixes":[]})",
                             R"({"properties":[],"prefixes":["unknown."]})"}) {
        const auto result = loadMask(mask);
        INFO(mask);
        CHECK_FALSE(result.hasValue());
        CHECK(hasDiagnostic(result, "chart.animation.mask_conflict"));
    }
}

TEST_CASE("Chart v4 Reader diagnoses empty and overlong mask properties",
          "[chart][v4][mask-property][ch-01]") {
    constexpr std::string_view layerPath =
        "$/objects/0/components/cuexis.animator/layers/0/propertyMask/properties/0";
    constexpr std::string_view instancePath =
        "$/objects/0/components/cuexis.animator/layers/0/blendGroups/0/instances/0/"
        "propertyMask/properties/0";
    const auto overlongProperty = std::string(257, 'x');

    SECTION("empty Layer property") {
        requireInvalidMaskProperty(
            cuexis::chart::ChartV4Loader::load(makeLayerMaskPropertyDocument("")), layerPath);
    }

    SECTION("overlong Layer property") {
        requireInvalidMaskProperty(
            cuexis::chart::ChartV4Loader::load(makeLayerMaskPropertyDocument(overlongProperty)),
            layerPath);
    }

    SECTION("empty Instance property") {
        requireInvalidMaskProperty(
            cuexis::chart::ChartV4Loader::load(makeInstanceMaskPropertyDocument("")), instancePath);
    }

    SECTION("overlong Instance property") {
        requireInvalidMaskProperty(
            cuexis::chart::ChartV4Loader::load(makeInstanceMaskPropertyDocument(overlongProperty)),
            instancePath);
    }
}

TEST_CASE("Chart v4 Reader fixes routing, import, and parameter failure diagnostics",
          "[chart][v4][diagnostics][branch-coverage]") {
    SECTION("root type and unsupported version") {
        const auto root = cuexis::chart::ChartV4Loader::load("[]");
        REQUIRE_FALSE(root.hasValue());
        REQUIRE_FALSE(root.diagnostics.empty());
        CHECK(root.diagnostics.items().front().code() == "json.type.mismatch");

        const auto version = cuexis::chart::ChartV4Loader::load(
            makeMinimalV4Document("[]", "[]").replace(
                makeMinimalV4Document("[]", "[]").find("\"version\":4"), 11,
                "\"version\":3"));
        REQUIRE_FALSE(version.hasValue());
        CHECK(hasDiagnostic(version, "chart.version.unsupported"));
    }

    SECTION("parameter exact limit, duplicate, and malformed type") {
        constexpr std::string_view parameter =
            R"({"id":"value.x","type":"number","default":0,"constraints":{}})";
        const auto document = makeMinimalV4Document(
            std::string{"["} + std::string{parameter} + "]", "[]");
        auto limits = cuexis::chart::ChartLimits{};
        limits.maxChartParameters = 1;
        const auto exact = cuexis::chart::ChartV4Loader::load(document, limits);
        REQUIRE(exact.hasValue());

        const auto over = cuexis::chart::ChartV4Loader::load(
            makeMinimalV4Document(std::string{"["} + std::string{parameter} + "," +
                                      std::string{parameter} + "]",
                                  "[]"),
            limits);
        REQUIRE_FALSE(over.hasValue());
        CHECK(hasDiagnostic(over, "chart.parameter.out_of_range"));
        CHECK(hasDiagnostic(over, "chart.parameter.duplicate"));

        const auto malformed = cuexis::chart::ChartV4Loader::load(makeMinimalV4Document(
            R"([{"id":"value.x","type":"future","constraints":{}}])", "[]"));
        REQUIRE_FALSE(malformed.hasValue());
        CHECK(hasDiagnostic(malformed, "chart.parameter.type_mismatch"));
    }

    SECTION("imports require portable lowercase CXT paths and unique records") {
        const auto invalidPath = cuexis::chart::ChartV4Loader::load(makeMinimalV4Document(
            "[]", R"([{"id":"motion.x","source":"Templates/MOVE.CXT"}])"));
        REQUIRE_FALSE(invalidPath.hasValue());
        CHECK(hasDiagnostic(invalidPath, "cxt.template.invalid"));

        const auto duplicate = cuexis::chart::ChartV4Loader::load(makeMinimalV4Document(
            "[]", R"([{"id":"motion.x","source":"templates/move.cxt"},
                         {"id":"motion.x","source":"templates/other.cxt"}])"));
        REQUIRE_FALSE(duplicate.hasValue());
        CHECK(hasDiagnostic(duplicate, "cxt.import.duplicate"));
    }
}

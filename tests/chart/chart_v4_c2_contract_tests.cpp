#include <cuexis/chart/chart_v4_loader.hpp>
#include <cuexis/chart/chart_v4_resolver.hpp>
#include <cuexis/chart/chart_writer.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace {

struct ChartParts final {
    std::string timing{R"({"offsetMs":0,"defaultBpm":120,"tempoEvents":[],"stops":[]})"};
    std::optional<std::string> audio;
    std::string parameters{"[]"};
    std::string templates{"[]"};
    std::string behaviors{"[]"};
    std::string imports{"[]"};
    std::string clips{"[]"};
    std::string objects{"[]"};
    std::string requiredExtensions{"[]"};
    std::string extensions{"{}"};
};

[[nodiscard]] auto makeChart(const ChartParts& parts) -> std::string {
    auto result = std::string{R"({
      "format":"cuexis.chart","version":4,
      "chartId":"019f0000-0000-7abc-8def-0000000004c2",
      "metadata":{"title":"CFU-C2 contract"},
      "timing":)"};
    result += parts.timing;
    result += R"(,
      "camera":{"type":"perspective","fovY":60,"near":0.1,"far":1000})";
    if (parts.audio) {
        result += ",\n      \"audio\":" + *parts.audio;
    }
    result += ",\n      \"parameters\":" + parts.parameters;
    result += ",\n      \"templates\":" + parts.templates;
    result += ",\n      \"behaviors\":" + parts.behaviors;
    result += ",\n      \"animationTemplateImports\":" + parts.imports;
    result += ",\n      \"animationClips\":" + parts.clips;
    result += ",\n      \"objects\":" + parts.objects;
    result += ",\n      \"requiredExtensions\":" + parts.requiredExtensions;
    result += ",\n      \"extensions\":" + parts.extensions + "\n    }";
    return result;
}

[[nodiscard]] auto makeCxt(std::string_view templateId, std::string_view clip,
                           std::string_view blendMode = "override",
                           std::string_view requiredExtensions = "[]") -> std::string {
    auto result = std::string{R"({
      "format":"cuexis.animation-template","version":1,"templateId":")"};
    result += templateId;
    result += R"(","metadata":{"name":"contract"},
      "application":{"coordinateSpace":"local","blendMode":")";
    result += blendMode;
    result += R"(","iterations":1,"fillMode":"none"},
      "clip":)";
    result += clip;
    result += ",\n      \"requiredExtensions\":";
    result += requiredExtensions;
    result += ",\n      \"extensions\":{}\n    }";
    return result;
}

[[nodiscard]] auto loadSource(std::string_view text) -> cuexis::chart::ChartV4SourceDocument {
    auto result = cuexis::chart::ChartV4Loader::load(text);
    if (!result.hasValue()) {
        throw std::runtime_error{"Inline CFU-C2 Chart did not load"};
    }
    return std::move(*result.document);
}

[[nodiscard]] auto hasDiagnostic(const cuexis::chart::ChartV4ResolveResult& result,
                                 std::string_view code) -> bool {
    return std::ranges::any_of(result.diagnostics.items(),
                               [code](const auto& item) { return item.code() == code; });
}

[[nodiscard]] auto hasDiagnostic(const cuexis::chart::ChartV4SourceResult& result,
                                 std::string_view code) -> bool {
    return std::ranges::any_of(result.diagnostics.items(),
                               [code](const auto& item) { return item.code() == code; });
}

[[nodiscard]] auto findDiagnostic(const cuexis::chart::ChartV4ResolveResult& result,
                                  std::string_view code, std::string_view recordKind = {})
    -> const cuexis::core::Diagnostic* {
    const auto item = std::ranges::find_if(result.diagnostics.items(), [&](const auto& diagnostic) {
        if (diagnostic.code() != code) {
            return false;
        }
        if (recordKind.empty()) {
            return true;
        }
        return std::ranges::any_of(diagnostic.context(), [&](const auto& context) {
            return context.key == "record_kind" && context.value == recordKind;
        });
    });
    return item != result.diagnostics.items().end() ? &*item : nullptr;
}

[[nodiscard]] auto contextValue(const cuexis::core::Diagnostic& diagnostic, std::string_view key)
    -> std::optional<std::string_view> {
    const auto item = std::ranges::find_if(
        diagnostic.context(), [key](const auto& context) { return context.key == key; });
    return item != diagnostic.context().end() ? std::optional<std::string_view>{item->value}
                                              : std::nullopt;
}

[[nodiscard]] auto continuousClip(std::string_view id, std::string_view property,
                                  std::string_view startValue, std::string_view endValue,
                                  std::string_view duration = R"({"numerator":1,"denominator":1})")
    -> std::string {
    auto result = std::string{"{\"id\":\""};
    result += id;
    result += "\",\"version\":1,\"durationBeats\":";
    result += duration;
    result += ",\"tracks\":[{\"property\":\"";
    result += property;
    result += R"(","segments":[{"startBeat":{"numerator":0,"denominator":1},"durationBeats":)";
    result += duration;
    result += ",\"startValue\":";
    result += startValue;
    result += ",\"endValue\":";
    result += endValue;
    result += R"(,"startSlope":0,"endSlope":0}]}],"stepTracks":[]})";
    return result;
}

[[nodiscard]] auto cxtContinuousClip(std::string_view property, std::string_view startValue,
                                     std::string_view endValue) -> std::string {
    auto clip = continuousClip("ignored", property, startValue, endValue);
    const auto id = clip.find("\"id\":\"ignored\",");
    if (id == std::string::npos) {
        throw std::runtime_error{"Could not strip CXT clip ID"};
    }
    clip.erase(id, std::string_view{"\"id\":\"ignored\","}.size());
    return clip;
}

[[nodiscard]] auto stepMaterialClip(std::string_view id, std::string_view assetId,
                                    bool includeId = true) -> std::string {
    auto result = std::string{"{"};
    if (includeId) {
        result += "\"id\":\"";
        result += id;
        result += "\",";
    }
    result += R"("version":1,"durationBeats":{"numerator":1,"denominator":1},
      "tracks":[],"stepTracks":[{"property":"render.material","steps":[{
        "beat":{"numerator":0,"denominator":1},"value":{"domain":"asset","id":")";
    result += assetId;
    result += R"("}}]}]})";
    return result;
}

[[nodiscard]] auto transformComponent() -> std::string_view {
    return R"("cuexis.transform":{"version":1,"position":[0,0,0],
      "rotation":[0,0,0,1],"scale":[1,1,1]})";
}

[[nodiscard]] auto findResource(const cuexis::chart::ChartV4ResolvedArtifact& artifact,
                                std::string_view id)
    -> const cuexis::chart::ChartResourceRequirement* {
    const auto item = std::ranges::find_if(artifact.resourceRequirements, [id](const auto& value) {
        return value.assetId.value == id;
    });
    return item != artifact.resourceRequirements.end() ? &*item : nullptr;
}

} // namespace

TEST_CASE("Parameter identity matches the frozen binary encoding vector",
          "[chart][v4][resolver][identity][cfu-c2]") {
    ChartParts parts;
    parts.parameters = R"([
      {"id":"omega.weight","type":"weight","default":0.5,"constraints":{}},
      {"id":"beta.rational","type":"rational",
       "default":{"numerator":-6,"denominator":4},"constraints":{}},
      {"id":"alpha.number","type":"number","default":-0.0,"constraints":{}}
    ])";
    const auto result = cuexis::chart::ChartV4Resolver::resolve(loadSource(makeChart(parts)));
    REQUIRE(result.hasValue());
    CHECK(result.artifact->parameterIdentity.hex() ==
          "79f913beaa5a246f0bf0b7747fe43d4920b6516f2bb6ff7aedada8dd69a4eb5e");
    REQUIRE(result.artifact->document.parameters.size() == 3);
    CHECK(result.artifact->document.parameters[0].id == "alpha.number");
    CHECK(result.artifact->document.parameters[1].id == "beta.rational");
    CHECK(result.artifact->document.parameters[2].id == "omega.weight");
}

TEST_CASE("Resolved discrete Layer weight is validated after parameter freezing",
          "[chart][v4][resolver][animation][parameters][cfu-c2]") {
    ChartParts parts;
    parts.parameters = R"([
      {"id":"layer.weight","type":"weight","default":0.5,"constraints":{}}
    ])";
    parts.clips = R"([{
      "id":"animation.visible","version":1,
      "durationBeats":{"numerator":1,"denominator":1},"tracks":[],
      "stepTracks":[{"property":"render.visible","steps":[{
        "beat":{"numerator":0,"denominator":1},"value":true}]}]
    }])";
    parts.objects = R"([{
      "id":"019f0000-0000-7abc-8def-0000000004d1","parent":null,
      "components":{
        "cuexis.renderable":{"version":1,
          "mesh":{"domain":"asset","id":"mesh.visible"},
          "material":{"domain":"asset","id":"material.visible"}},
        "cuexis.animator":{"version":1,"templateBindings":[],"layers":[{
          "layerId":"layer.visible","priority":0,
          "weight":{"parameter":{"domain":"chart-parameter","id":"layer.weight"}},
          "propertyMask":{"properties":["render.visible"],"prefixes":[]},
          "blendGroups":[{"groupId":"group.visible","mode":"override","weight":1,
            "instances":[{"instanceId":"instance.visible",
              "clip":{"domain":"animation","id":"animation.visible"},
              "startBeat":{"numerator":0,"denominator":1},"iterations":1,
              "fillMode":"none","weight":1,
              "propertyMask":{"properties":["render.visible"],"prefixes":[]}}]}]
        }]}
      },"extensions":{}
    }])";
    const auto result = cuexis::chart::ChartV4Resolver::resolve(loadSource(makeChart(parts)));
    CHECK_FALSE(result.hasValue());
    CHECK(hasDiagnostic(result, "chart.animation.discrete_weight_unsupported"));
}

TEST_CASE("Generated and explicit Layers share priority-mask conflict validation",
          "[chart][v4][resolver][lowering][mask][cfu-c2]") {
    ChartParts parts;
    parts.imports = R"([{"id":"motion.x","source":"templates/motion-x.cxt"}])";
    parts.objects = std::string{R"([{
      "id":"019f0000-0000-7abc-8def-0000000004d2","parent":null,"components":{
        )"} + std::string{transformComponent()} +
                    R"(,
        "cuexis.animator":{"version":1,"templateBindings":[{
          "bindingId":"binding.x","template":{"domain":"animation-template","id":"motion.x"},
          "startBeat":{"numerator":0,"denominator":1},
          "durationScale":{"numerator":1,"denominator":1},"weight":1,"priority":10
        }],"layers":[{
          "layerId":"layer.explicit","priority":10,"weight":1,
          "propertyMask":{"properties":["transform.position.x"],"prefixes":[]},
          "blendGroups":[]
        }]}
      },"extensions":{}
    }])";
    const auto cxt = makeCxt("motion.x", cxtContinuousClip("transform.position.x", "0", "1"));
    const std::array documents{cuexis::chart::ProjectDocument{"templates/motion-x.cxt", cxt}};
    const auto result =
        cuexis::chart::ChartV4Resolver::resolve(loadSource(makeChart(parts)), {}, documents);
    CHECK_FALSE(result.hasValue());
    CHECK(hasDiagnostic(result, "chart.animation.mask_conflict"));
    const auto* diagnostic = findDiagnostic(result, "chart.animation.mask_conflict", "layer");
    REQUIRE(diagnostic != nullptr);
    CHECK(diagnostic->fieldPath() == "$/objects/0/components/cuexis.animator/templateBindings/0");
    CHECK(contextValue(*diagnostic, "object_id") ==
          std::optional<std::string_view>{"019f0000-0000-7abc-8def-0000000004d2"});
    CHECK(contextValue(*diagnostic, "binding_id") == std::optional<std::string_view>{"binding.x"});
    CHECK(contextValue(*diagnostic, "template_id") == std::optional<std::string_view>{"motion.x"});
}

TEST_CASE("Resolver rejects overlapping BlendGroup effective properties",
          "[chart][v4][resolver][animation][mask][cfu-c2]") {
    ChartParts parts;
    parts.clips = "[" +
                  continuousClip("animation.scale", "transform.scale", "[1,1,1]", "[1.2,1.2,1.2]") +
                  "]";
    parts.objects = std::string{R"([{
      "id":"019f0000-0000-7abc-8def-0000000004d3","parent":null,"components":{
        )"} + std::string{transformComponent()} +
                    R"(,
        "cuexis.animator":{"version":1,"templateBindings":[],"layers":[{
          "layerId":"layer.scale","priority":0,"weight":1,
          "propertyMask":{"properties":["transform.scale"],"prefixes":[]},
          "blendGroups":[
            {"groupId":"group.a","mode":"override","weight":1,"instances":[{
              "instanceId":"instance.a","clip":{"domain":"animation","id":"animation.scale"},
              "startBeat":{"numerator":0,"denominator":1},"iterations":1,"fillMode":"none",
              "weight":1,"propertyMask":{"properties":["transform.scale"],"prefixes":[]}}]},
            {"groupId":"group.b","mode":"override","weight":1,"instances":[{
              "instanceId":"instance.b","clip":{"domain":"animation","id":"animation.scale"},
              "startBeat":{"numerator":0,"denominator":1},"iterations":1,"fillMode":"none",
              "weight":1,"propertyMask":{"properties":["transform.scale"],"prefixes":[]}}]}
          ]
        }]}
      },"extensions":{}
    }])";
    const auto result = cuexis::chart::ChartV4Resolver::resolve(loadSource(makeChart(parts)));
    CHECK_FALSE(result.hasValue());
    CHECK(hasDiagnostic(result, "chart.animation.track_conflict"));
}

TEST_CASE("Resolver rejects an Instance mask outside its Layer mask",
          "[chart][v4][resolver][animation][mask][cfu-c2]") {
    ChartParts parts;
    parts.clips = "[" + continuousClip("animation.opacity", "material.opacity", "1", "0.5") + "]";
    parts.objects = R"([{
      "id":"019f0000-0000-7abc-8def-0000000004d4","parent":null,"components":{
        "cuexis.renderable":{"version":1,
          "mesh":{"domain":"asset","id":"mesh.mask"},
          "material":{"domain":"asset","id":"material.mask"}},
        "cuexis.animator":{"version":1,"templateBindings":[],"layers":[{
          "layerId":"layer.scale","priority":0,"weight":1,
          "propertyMask":{"properties":["transform.scale"],"prefixes":[]},
          "blendGroups":[{"groupId":"group.opacity","mode":"override","weight":1,
            "instances":[{"instanceId":"instance.opacity",
              "clip":{"domain":"animation","id":"animation.opacity"},
              "startBeat":{"numerator":0,"denominator":1},"iterations":1,
              "fillMode":"none","weight":1,
              "propertyMask":{"properties":["material.opacity"],"prefixes":[]}}]}]
        }]}
      },"extensions":{}
    }])";
    const auto result = cuexis::chart::ChartV4Resolver::resolve(loadSource(makeChart(parts)));
    CHECK_FALSE(result.hasValue());
    CHECK(hasDiagnostic(result, "chart.animation.mask_conflict"));
}

TEST_CASE("Resolver rejects non-positive additive scale factors",
          "[chart][v4][resolver][animation][additive][cfu-c2]") {
    ChartParts parts;
    parts.clips =
        "[" + continuousClip("animation.scale", "transform.scale", "[1,1,1]", "[0,1,1]") + "]";
    parts.objects = std::string{R"([{
      "id":"019f0000-0000-7abc-8def-0000000004d5","parent":null,"components":{
        )"} + std::string{transformComponent()} +
                    R"(,
        "cuexis.animator":{"version":1,"templateBindings":[],"layers":[{
          "layerId":"layer.scale","priority":0,"weight":1,
          "propertyMask":{"properties":["transform.scale"],"prefixes":[]},
          "blendGroups":[{"groupId":"group.scale","mode":"additive","weight":1,
            "instances":[{"instanceId":"instance.scale",
              "clip":{"domain":"animation","id":"animation.scale"},
              "startBeat":{"numerator":0,"denominator":1},"iterations":1,
              "fillMode":"none","weight":1,
              "propertyMask":{"properties":["transform.scale"],"prefixes":[]}}]}]
        }]}
      },"extensions":{}
    }])";
    const auto result = cuexis::chart::ChartV4Resolver::resolve(loadSource(makeChart(parts)));
    CHECK_FALSE(result.hasValue());
    CHECK(hasDiagnostic(result, "chart.animation.additive_unsupported"));
}

TEST_CASE("Animator whole-component add remove and replace patches are lowered",
          "[chart][v4][resolver][template][patch][cfu-c2]") {
    ChartParts parts;
    parts.templates = R"([
      {"id":"019f0000-0000-7abc-8def-0000000004e0","extends":null,
       "prototype":{"components":{"cuexis.transform":{"version":1,"position":[0,0,0],
         "rotation":[0,0,0,1],"scale":[1,1,1]}}},"extensions":{}},
      {"id":"019f0000-0000-7abc-8def-0000000004e1",
       "extends":{"domain":"template","id":"019f0000-0000-7abc-8def-0000000004e0"},
       "patch":[{"op":"add","path":"/components/cuexis.animator","value":{
         "version":1,"templateBindings":[],"layers":[{"layerId":"layer.add","priority":1,
           "weight":1,"propertyMask":{"properties":["transform.scale"],"prefixes":[]},
           "blendGroups":[]}]}}],"extensions":{}},
      {"id":"019f0000-0000-7abc-8def-0000000004e2","extends":null,
       "prototype":{"components":{"cuexis.transform":{"version":1,"position":[0,0,0],
         "rotation":[0,0,0,1],"scale":[1,1,1]},"cuexis.animator":{
           "version":1,"templateBindings":[],"layers":[{"layerId":"layer.old","priority":1,
             "weight":1,"propertyMask":{"properties":["transform.scale"],"prefixes":[]},
             "blendGroups":[]}]}}},"extensions":{}},
      {"id":"019f0000-0000-7abc-8def-0000000004e3",
       "extends":{"domain":"template","id":"019f0000-0000-7abc-8def-0000000004e2"},
       "patch":[{"op":"remove","path":"/components/cuexis.animator"}],"extensions":{}},
      {"id":"019f0000-0000-7abc-8def-0000000004e4",
       "extends":{"domain":"template","id":"019f0000-0000-7abc-8def-0000000004e2"},
       "patch":[{"op":"replace","path":"/components/cuexis.animator","value":{
         "version":1,"templateBindings":[],"layers":[{"layerId":"layer.new","priority":2,
           "weight":1,"propertyMask":{"properties":["transform.scale"],"prefixes":[]},
           "blendGroups":[]}]}}],"extensions":{}}
    ])";
    parts.objects = R"([
      {"id":"019f0000-0000-7abc-8def-0000000004e5","parent":null,
       "template":{"domain":"template","id":"019f0000-0000-7abc-8def-0000000004e1"},
       "overrides":[],"extensions":{}},
      {"id":"019f0000-0000-7abc-8def-0000000004e6","parent":null,
       "template":{"domain":"template","id":"019f0000-0000-7abc-8def-0000000004e3"},
       "overrides":[],"extensions":{}},
      {"id":"019f0000-0000-7abc-8def-0000000004e7","parent":null,
       "template":{"domain":"template","id":"019f0000-0000-7abc-8def-0000000004e4"},
       "overrides":[],"extensions":{}}
    ])";
    const auto result = cuexis::chart::ChartV4Resolver::resolve(loadSource(makeChart(parts)));
    REQUIRE(result.hasValue());
    REQUIRE(result.artifact->animationProgram.objects.size() == 2);
    CHECK(result.artifact->animationProgram.objects[0].objectId.value ==
          "019f0000-0000-7abc-8def-0000000004e5");
    CHECK(std::get<std::string>(result.artifact->animationProgram.objects[0].layers[0].identity) ==
          "layer.add");
    CHECK(result.artifact->animationProgram.objects[1].objectId.value ==
          "019f0000-0000-7abc-8def-0000000004e7");
    CHECK(std::get<std::string>(result.artifact->animationProgram.objects[1].layers[0].identity) ==
          "layer.new");
}

TEST_CASE("Chart and CXT required extensions use explicit supported versions",
          "[chart][v4][resolver][extension][cfu-c2]") {
    SECTION("Chart extension") {
        ChartParts parts;
        parts.requiredExtensions = R"([{"id":"org.example.chart","version":2}])";
        const auto source = loadSource(makeChart(parts));
        const auto rejected = cuexis::chart::ChartV4Resolver::resolve(source);
        CHECK_FALSE(rejected.hasValue());
        CHECK(hasDiagnostic(rejected, "chart.extension.required_unsupported"));

        const std::array supported{cuexis::chart::RequiredExtension{"org.example.chart", 2}};
        CHECK(cuexis::chart::ChartV4Resolver::resolve(source, {}, {}, supported).hasValue());
    }

    SECTION("CXT extension") {
        ChartParts parts;
        parts.imports = R"([{"id":"motion.ext","source":"templates/ext.cxt"}])";
        const auto cxt = makeCxt("motion.ext", cxtContinuousClip("transform.position.x", "0", "1"),
                                 "override", R"([{"id":"org.example.cxt","version":3}])");
        const std::array documents{cuexis::chart::ProjectDocument{"templates/ext.cxt", cxt}};
        const auto source = loadSource(makeChart(parts));
        const auto rejected = cuexis::chart::ChartV4Resolver::resolve(source, {}, documents);
        CHECK_FALSE(rejected.hasValue());
        CHECK(hasDiagnostic(rejected, "chart.extension.required_unsupported"));
        const auto* diagnostic = findDiagnostic(rejected, "chart.extension.required_unsupported");
        REQUIRE(diagnostic != nullptr);
        CHECK(diagnostic->fieldPath() == "$/requiredExtensions");
        CHECK(contextValue(*diagnostic, "source") ==
              std::optional<std::string_view>{"templates/ext.cxt"});
        CHECK(contextValue(*diagnostic, "template_id") ==
              std::optional<std::string_view>{"motion.ext"});
        CHECK(contextValue(*diagnostic, "import_id") ==
              std::optional<std::string_view>{"motion.ext"});

        const std::array supported{cuexis::chart::RequiredExtension{"org.example.cxt", 3}};
        CHECK(cuexis::chart::ChartV4Resolver::resolve(source, {}, documents, supported).hasValue());
    }
}

TEST_CASE("Chart v4 imports require the lowercase .cxt extension",
          "[chart][v4][reader][cxt][path][cfu-c2]") {
    for (const auto* source : {"templates/motion.CXT", "templates/motion.json"}) {
        ChartParts parts;
        parts.imports = std::string{"[{\"id\":\"motion.path\",\"source\":\""} + source + "\"}]";
        const auto result = cuexis::chart::ChartV4Loader::load(makeChart(parts));
        INFO(source);
        CHECK_FALSE(result.hasValue());
        CHECK(hasDiagnostic(result, "cxt.template.invalid"));
    }
}

TEST_CASE("Project document paths reject exact and ASCII case-fold duplicates",
          "[chart][v4][resolver][cxt][path][cfu-c2]") {
    ChartParts parts;
    parts.imports = R"([{"id":"motion.path","source":"templates/path.cxt"}])";
    const auto source = loadSource(makeChart(parts));
    const auto cxt = makeCxt("motion.path", cxtContinuousClip("transform.position.x", "0", "1"));

    SECTION("exact duplicate") {
        const std::array documents{
            cuexis::chart::ProjectDocument{"templates/path.cxt", cxt},
            cuexis::chart::ProjectDocument{"templates/path.cxt", cxt},
        };
        const auto result = cuexis::chart::ChartV4Resolver::resolve(source, {}, documents);
        CHECK_FALSE(result.hasValue());
        CHECK(hasDiagnostic(result, "cxt.import.duplicate"));
    }

    SECTION("case-fold duplicate") {
        const std::array documents{
            cuexis::chart::ProjectDocument{"templates/path.cxt", cxt},
            cuexis::chart::ProjectDocument{"Templates/PATH.cxt", cxt},
        };
        const auto result = cuexis::chart::ChartV4Resolver::resolve(source, {}, documents);
        CHECK_FALSE(result.hasValue());
        CHECK(hasDiagnostic(result, "cxt.import.duplicate"));
    }
}

TEST_CASE("CXT import rejects an unsupported document version",
          "[chart][v4][resolver][cxt][version][cfu-c2]") {
    ChartParts parts;
    parts.imports = R"([{"id":"motion.version","source":"templates/version.cxt"}])";
    auto cxt = makeCxt("motion.version", cxtContinuousClip("transform.position.x", "0", "1"));
    const auto version = cxt.find("\"version\":1");
    REQUIRE(version != std::string::npos);
    cxt.replace(version, std::string_view{"\"version\":1"}.size(), "\"version\":2");
    const std::array documents{cuexis::chart::ProjectDocument{"templates/version.cxt", cxt}};
    const auto result =
        cuexis::chart::ChartV4Resolver::resolve(loadSource(makeChart(parts)), {}, documents);
    CHECK_FALSE(result.hasValue());
    CHECK(hasDiagnostic(result, "cxt.version.unsupported"));
    const auto* diagnostic = findDiagnostic(result, "cxt.version.unsupported");
    REQUIRE(diagnostic != nullptr);
    CHECK(contextValue(*diagnostic, "source") ==
          std::optional<std::string_view>{"templates/version.cxt"});
    CHECK(contextValue(*diagnostic, "template_id") ==
          std::optional<std::string_view>{"motion.version"});
    CHECK(contextValue(*diagnostic, "import_id") ==
          std::optional<std::string_view>{"motion.version"});
}

TEST_CASE("Component validation uses the mask-filtered effective property set",
          "[chart][v4][resolver][component][mask][cfu-c2]") {
    ChartParts parts;
    parts.clips = "[" +
                  continuousClip("animation.scale", "transform.scale", "[1,1,1]", "[1.2,1.2,1.2]") +
                  "," + continuousClip("animation.opacity", "material.opacity", "1", "0.5") + "]";
    parts.objects = std::string{R"([{
      "id":"019f0000-0000-7abc-8def-0000000004f4","parent":null,"components":{
        )"} + std::string{transformComponent()} +
                    R"(,
        "cuexis.animator":{"version":1,"templateBindings":[],"layers":[{
          "layerId":"layer.scale","priority":0,"weight":1,
          "propertyMask":{"properties":["transform.scale"],"prefixes":[]},
          "blendGroups":[{"groupId":"group.scale","mode":"override","weight":1,
            "instances":[
              {"instanceId":"instance.scale",
               "clip":{"domain":"animation","id":"animation.scale"},
               "startBeat":{"numerator":0,"denominator":1},"iterations":1,
               "fillMode":"none","weight":1,
               "propertyMask":{"properties":["transform.scale"],"prefixes":[]}},
              {"instanceId":"instance.opacity",
               "clip":{"domain":"animation","id":"animation.opacity"},
               "startBeat":{"numerator":0,"denominator":1},"iterations":1,
               "fillMode":"none","weight":1,
               "propertyMask":{"properties":["transform.scale"],"prefixes":[]}}
            ]}]
        }]}
      },"extensions":{}
    }])";
    CHECK(cuexis::chart::ChartV4Resolver::resolve(loadSource(makeChart(parts))).hasValue());

    parts.objects = R"([{
      "id":"019f0000-0000-7abc-8def-0000000004f5","parent":null,"components":{
        "cuexis.element":{"version":1},
        "cuexis.animator":{"version":1,"templateBindings":[],"layers":[{
          "layerId":"layer.scale","priority":0,"weight":1,
          "propertyMask":{"properties":["transform.scale"],"prefixes":[]},
          "blendGroups":[{"groupId":"group.scale","mode":"override","weight":1,
            "instances":[{"instanceId":"instance.scale",
              "clip":{"domain":"animation","id":"animation.scale"},
              "startBeat":{"numerator":0,"denominator":1},"iterations":1,
              "fillMode":"none","weight":1,
              "propertyMask":{"properties":["transform.scale"],"prefixes":[]}}]}]
        }]}
      },"extensions":{}
    }])";
    const auto missing = cuexis::chart::ChartV4Resolver::resolve(loadSource(makeChart(parts)));
    CHECK_FALSE(missing.hasValue());
    CHECK(hasDiagnostic(missing, "chart.animation.reference_missing"));
}

TEST_CASE("Capability requirements include all source animation declarations",
          "[chart][v4][resolver][capability][cfu-c2]") {
    const auto resolveCapabilities = [](ChartParts parts) {
        const auto result = cuexis::chart::ChartV4Resolver::resolve(loadSource(makeChart(parts)));
        if (!result.hasValue()) {
            throw std::runtime_error{"Capability test Chart did not resolve"};
        }
        return result.artifact->capabilityRequirements;
    };

    CHECK(resolveCapabilities(ChartParts{}) == std::vector<std::string>{"cuexis.chart.v4"});

    ChartParts inert;
    inert.objects = R"([{
      "id":"019f0000-0000-7abc-8def-0000000004f6","parent":null,"components":{
        "cuexis.element":{"version":1},
        "cuexis.animator":{"version":1,"templateBindings":[],"layers":[]}
      },"extensions":{}
    }])";
    CHECK(resolveCapabilities(std::move(inert)) == std::vector<std::string>{"cuexis.chart.v4"});

    ChartParts localClip;
    localClip.clips =
        "[" + continuousClip("animation.unbound", "transform.position.x", "0", "1") + "]";
    CHECK(resolveCapabilities(std::move(localClip)) ==
          std::vector<std::string>{"cuexis.animation.clip.v1", "cuexis.animation.layers.v1",
                                   "cuexis.chart.v4"});

    ChartParts sourceAnimator;
    sourceAnimator.templates = R"([{
      "id":"019f0000-0000-7abc-8def-0000000004f7","extends":null,
      "prototype":{"components":{
        "cuexis.element":{"version":1},
        "cuexis.animator":{"version":1,"templateBindings":[],"layers":[{
          "layerId":"layer.unused","priority":0,"weight":1,
          "propertyMask":{"properties":["transform.scale"],"prefixes":[]},
          "blendGroups":[]
        }]}
      }},"extensions":{}
    }])";
    CHECK(resolveCapabilities(std::move(sourceAnimator)) ==
          std::vector<std::string>{"cuexis.animation.clip.v1", "cuexis.animation.layers.v1",
                                   "cuexis.chart.v4"});
}

TEST_CASE("Resource closure includes every declared candidate independent of binding",
          "[chart][v4][resolver][resources][cfu-c2]") {
    ChartParts parts;
    parts.audio = R"({"version":1,"mainMusic":{"domain":"asset","id":"asset.audio"}})";
    parts.behaviors = R"([{
      "id":"behavior.resources","type":"behavior.event","version":1,"events":[],
      "stepEvents":[{"property":"render.material","beat":{"numerator":0,"denominator":1},
        "value":{"domain":"asset","id":"asset.behavior"}}]
    }])";
    parts.imports = R"([
      {"id":"motion.resource-b","source":"templates/resource-b.cxt"},
      {"id":"motion.resource-a","source":"templates/resource-a.cxt"}
    ])";
    parts.clips = "[" + stepMaterialClip("animation.resource", "asset.chart-animation") + "]";
    parts.objects = R"([{
      "id":"019f0000-0000-7abc-8def-0000000004f0","parent":null,"components":{
        "cuexis.renderable":{"version":1,
          "mesh":{"domain":"asset","id":"asset.mesh"},
          "material":{"domain":"asset","id":"asset.material"}}
      },"extensions":{}
    }])";
    const auto cxtA = makeCxt("motion.resource-a", stepMaterialClip("", "asset.cxt-a", false));
    const auto cxtB = makeCxt("motion.resource-b", stepMaterialClip("", "asset.cxt-b", false));
    const std::array documents{
        cuexis::chart::ProjectDocument{"templates/resource-b.cxt", cxtB},
        cuexis::chart::ProjectDocument{"templates/resource-a.cxt", cxtA},
    };
    const auto result =
        cuexis::chart::ChartV4Resolver::resolve(loadSource(makeChart(parts)), {}, documents);
    REQUIRE(result.hasValue());
    CHECK(result.artifact->cxtIdentities[0].importId == "motion.resource-a");
    CHECK(result.artifact->cxtIdentities[1].importId == "motion.resource-b");

    const auto requireUse = [&](std::string_view id, cuexis::chart::ChartResourceUse use) {
        const auto* resource = findResource(*result.artifact, id);
        REQUIRE(resource != nullptr);
        CHECK(resource->uses == std::vector<cuexis::chart::ChartResourceUse>{use});
    };
    requireUse("asset.audio", cuexis::chart::ChartResourceUse::MainMusic);
    requireUse("asset.behavior", cuexis::chart::ChartResourceUse::BehaviorMaterial);
    requireUse("asset.chart-animation", cuexis::chart::ChartResourceUse::AnimationMaterial);
    requireUse("asset.cxt-a", cuexis::chart::ChartResourceUse::AnimationMaterial);
    requireUse("asset.cxt-b", cuexis::chart::ChartResourceUse::AnimationMaterial);
    requireUse("asset.material", cuexis::chart::ChartResourceUse::RenderableMaterial);
    requireUse("asset.mesh", cuexis::chart::ChartResourceUse::RenderableMesh);
}

TEST_CASE("Aggregate and generated animation budgets honor exact boundaries",
          "[chart][v4][resolver][budget][cfu-c2]") {
    const auto cxtClip = cxtContinuousClip("transform.position.x", "0", "1");
    const auto cxt = makeCxt("motion.budget", cxtClip);
    const std::array documents{cuexis::chart::ProjectDocument{"templates/budget.cxt", cxt}};

    SECTION("unbound imports participate in track and segment totals") {
        ChartParts parts;
        parts.imports = R"([{"id":"motion.budget","source":"templates/budget.cxt"}])";
        parts.clips =
            "[" + continuousClip("animation.budget", "transform.position.y", "0", "1") + "]";
        const auto source = loadSource(makeChart(parts));

        cuexis::chart::ChartLimits exact;
        exact.maxAnimationTracks = 2;
        exact.maxAnimationSegmentsAndSteps = 2;
        CHECK(cuexis::chart::ChartV4Resolver::resolve(source, {}, documents, {}, exact).hasValue());

        auto tooFewTracks = exact;
        tooFewTracks.maxAnimationTracks = 1;
        const auto trackFailure =
            cuexis::chart::ChartV4Resolver::resolve(source, {}, documents, {}, tooFewTracks);
        CHECK_FALSE(trackFailure.hasValue());
        CHECK(hasDiagnostic(trackFailure, "chart.animation.generated_limit"));

        auto tooFewSegments = exact;
        tooFewSegments.maxAnimationSegmentsAndSteps = 1;
        const auto segmentFailure =
            cuexis::chart::ChartV4Resolver::resolve(source, {}, documents, {}, tooFewSegments);
        CHECK_FALSE(segmentFailure.hasValue());
        CHECK(hasDiagnostic(segmentFailure, "chart.animation.generated_limit"));
    }

    SECTION("one Binding requires exactly four generated records") {
        ChartParts parts;
        parts.imports = R"([{"id":"motion.budget","source":"templates/budget.cxt"}])";
        parts.objects = std::string{R"([{
          "id":"019f0000-0000-7abc-8def-0000000004f1","parent":null,"components":{
            )"} + std::string{transformComponent()} +
                        R"(,
            "cuexis.animator":{"version":1,"templateBindings":[{
              "bindingId":"binding.budget",
              "template":{"domain":"animation-template","id":"motion.budget"},
              "startBeat":{"numerator":0,"denominator":1},
              "durationScale":{"numerator":1,"denominator":1},"weight":1,"priority":0
            }],"layers":[]}
          },"extensions":{}
        }])";
        const auto source = loadSource(makeChart(parts));

        cuexis::chart::ChartLimits exact;
        exact.maxGeneratedAnimationRecords = 4;
        CHECK(cuexis::chart::ChartV4Resolver::resolve(source, {}, documents, {}, exact).hasValue());

        auto tooFew = exact;
        tooFew.maxGeneratedAnimationRecords = 3;
        const auto failure =
            cuexis::chart::ChartV4Resolver::resolve(source, {}, documents, {}, tooFew);
        CHECK_FALSE(failure.hasValue());
        CHECK(hasDiagnostic(failure, "chart.animation.generated_limit"));
    }

    SECTION("prepared-content totals count imported and generated Binding Clips") {
        ChartParts parts;
        parts.imports = R"([{"id":"motion.budget","source":"templates/budget.cxt"}])";
        parts.objects = std::string{R"([{
          "id":"019f0000-0000-7abc-8def-0000000004f1","parent":null,"components":{
            )"} + std::string{transformComponent()} +
                        R"(,
            "cuexis.animator":{"version":1,"templateBindings":[{
              "bindingId":"binding.budget",
              "template":{"domain":"animation-template","id":"motion.budget"},
              "startBeat":{"numerator":0,"denominator":1},
              "durationScale":{"numerator":1,"denominator":1},"weight":1,"priority":0
            }],"layers":[]}
          },"extensions":{}
        }])";
        const auto source = loadSource(makeChart(parts));

        cuexis::chart::ChartLimits exact;
        exact.maxAnimationTracks = 2;
        exact.maxAnimationSegmentsAndSteps = 2;
        CHECK(cuexis::chart::ChartV4Resolver::resolve(source, {}, documents, {}, exact).hasValue());

        auto limits = exact;
        limits.maxAnimationTracks = 1;
        const auto failure =
            cuexis::chart::ChartV4Resolver::resolve(source, {}, documents, {}, limits);
        CHECK_FALSE(failure.hasValue());
        const auto* diagnostic = findDiagnostic(failure, "chart.animation.generated_limit", "clip");
        REQUIRE(diagnostic != nullptr);
        CHECK(diagnostic->fieldPath() ==
              "$/objects/0/components/cuexis.animator/templateBindings/0");
        CHECK(contextValue(*diagnostic, "binding_id") ==
              std::optional<std::string_view>{"binding.budget"});
        CHECK(contextValue(*diagnostic, "template_id") ==
              std::optional<std::string_view>{"motion.budget"});
    }
}

TEST_CASE("Resolver revalidates the concrete v3 projection after parameter freezing",
          "[chart][v4][resolver][compiler][cfu-c2]") {
    ChartParts parts;
    parts.parameters = R"([{"id":"camera.fov","type":"number","default":60,"constraints":{}}])";
    auto chart = makeChart(parts);
    constexpr std::string_view literal{"\"fovY\":60"};
    const auto position = chart.find(literal);
    REQUIRE(position != std::string::npos);
    chart.replace(position, literal.size(),
                  R"("fovY":{"parameter":{"domain":"chart-parameter","id":"camera.fov"}})");
    const auto source = loadSource(chart);
    const std::array parameters{cuexis::chart::ChartParameterInput{
        "camera.fov", cuexis::chart::ChartParameterType::Number, 180.0}};
    const auto result = cuexis::chart::ChartV4Resolver::resolve(source, parameters);
    CHECK_FALSE(result.hasValue());
    CHECK(hasDiagnostic(result, "chart.camera.invalid_fov"));
}

TEST_CASE("Array permutations and equivalent Rational inputs resolve identically",
          "[chart][v4][resolver][writer][determinism][cfu-c2]") {
    ChartParts first;
    first.parameters = R"([
      {"id":"z.rational","type":"rational",
       "default":{"numerator":2,"denominator":2},"constraints":{}},
      {"id":"a.number","type":"number","default":-0.0,"constraints":{}}
    ])";
    first.timing = R"({"offsetMs":-0.0,"defaultBpm":120,"tempoEvents":[],"stops":[
      {"beat":{"numerator":4,"denominator":2},"durationMs":100},
      {"beat":{"numerator":1,"denominator":1},"durationMs":50}
    ]})";
    first.clips = "[" +
                  continuousClip("animation.z", "transform.position.z", "0", "1",
                                 R"({"numerator":2,"denominator":2})") +
                  "," + continuousClip("animation.a", "transform.position.x", "0", "1") + "]";
    first.objects = R"([
      {"id":"019f0000-0000-7abc-8def-0000000004f3","parent":null,
       "components":{"cuexis.element":{"version":1}},"extensions":{}},
      {"id":"019f0000-0000-7abc-8def-0000000004f2","parent":null,
       "components":{"cuexis.element":{"version":1}},"extensions":{}}
    ])";

    ChartParts second;
    second.parameters = R"([
      {"constraints":{},"default":0.0,"type":"number","id":"a.number"},
      {"constraints":{},"default":{"denominator":4,"numerator":4},
       "type":"rational","id":"z.rational"}
    ])";
    second.timing = R"({"stops":[
      {"durationMs":50,"beat":{"denominator":2,"numerator":2}},
      {"durationMs":100,"beat":{"denominator":1,"numerator":2}}
    ],"tempoEvents":[],"defaultBpm":120,"offsetMs":0.0})";
    second.clips = "[" +
                   continuousClip("animation.a", "transform.position.x", "0", "1",
                                  R"({"denominator":2,"numerator":2})") +
                   "," +
                   continuousClip("animation.z", "transform.position.z", "0", "1",
                                  R"({"denominator":4,"numerator":4})") +
                   "]";
    second.objects = R"([
      {"extensions":{},"components":{"cuexis.element":{"version":1}},"parent":null,
       "id":"019f0000-0000-7abc-8def-0000000004f2"},
      {"extensions":{},"components":{"cuexis.element":{"version":1}},"parent":null,
       "id":"019f0000-0000-7abc-8def-0000000004f3"}
    ])";

    const auto firstSource = loadSource(makeChart(first));
    const auto secondSource = loadSource(makeChart(second));
    const auto firstBytes = cuexis::chart::ChartWriter::writeV4(firstSource);
    const auto secondBytes = cuexis::chart::ChartWriter::writeV4(secondSource);
    REQUIRE(firstBytes.has_value());
    REQUIRE(secondBytes.has_value());
    CHECK(*firstBytes == *secondBytes);

    const auto firstResult = cuexis::chart::ChartV4Resolver::resolve(firstSource);
    const auto secondResult = cuexis::chart::ChartV4Resolver::resolve(secondSource);
    REQUIRE(firstResult.hasValue());
    REQUIRE(secondResult.hasValue());
    CHECK(firstResult.artifact->chartIdentity == secondResult.artifact->chartIdentity);
    CHECK(firstResult.artifact->parameterIdentity == secondResult.artifact->parameterIdentity);
    CHECK(firstResult.artifact->document.chart.objects[0].id.value ==
          secondResult.artifact->document.chart.objects[0].id.value);
    CHECK(firstResult.artifact->document.chart.objects[1].id.value ==
          secondResult.artifact->document.chart.objects[1].id.value);
    REQUIRE(firstResult.artifact->animationProgram.clips.size() == 2);
    REQUIRE(secondResult.artifact->animationProgram.clips.size() == 2);
    for (std::size_t index = 0; index < 2; ++index) {
        CHECK(firstResult.artifact->animationProgram.clips[index].identity ==
              secondResult.artifact->animationProgram.clips[index].identity);
        CHECK(firstResult.artifact->animationProgram.clips[index].clip.durationBeats ==
              secondResult.artifact->animationProgram.clips[index].clip.durationBeats);
    }
}

TEST_CASE("Canonical Writer breaks equal primary keys by normalized compact JSON",
          "[chart][v4][writer][determinism][cfu-c2]") {
    ChartParts first;
    first.timing = R"({"offsetMs":0,"defaultBpm":120,"tempoEvents":[],"stops":[
      {"beat":{"numerator":1,"denominator":1},"durationMs":100},
      {"beat":{"numerator":2,"denominator":2},"durationMs":50}
    ]})";
    ChartParts second = first;
    second.timing = R"({"offsetMs":0,"defaultBpm":120,"tempoEvents":[],"stops":[
      {"beat":{"numerator":2,"denominator":2},"durationMs":50},
      {"beat":{"numerator":1,"denominator":1},"durationMs":100}
    ]})";

    auto firstSource = loadSource(makeChart(ChartParts{}));
    auto secondSource = firstSource;
    firstSource.canonicalSource.canonicalText = makeChart(first);
    secondSource.canonicalSource.canonicalText = makeChart(second);
    const auto firstBytes = cuexis::chart::ChartWriter::writeV4(firstSource);
    const auto secondBytes = cuexis::chart::ChartWriter::writeV4(secondSource);
    REQUIRE(firstBytes.has_value());
    REQUIRE(secondBytes.has_value());
    CHECK(*firstBytes == *secondBytes);
}

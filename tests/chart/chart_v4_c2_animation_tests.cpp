#include "chart_v4_c2_fixture.hpp"

// Animation values, layers, resolver property and patch cases.

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

TEST_CASE("Chart v4 animation clips parse each continuous value type and reject invalid values",
          "[chart][v4][reader][animation][value][branch-coverage]") {
    ChartParts parts;
    parts.clips = R"([{
      "id":"animation.values","version":1,
      "durationBeats":{"numerator":1,"denominator":1},
      "tracks":[
        {"property":"transform.rotation","segments":[{
          "startBeat":{"numerator":0,"denominator":1},
          "durationBeats":{"numerator":1,"denominator":1},
          "startValue":[0,0,0,1],"endValue":[0,0,0,1],"startSlope":0,"endSlope":0
        }]},
        {"property":"transform.scale","segments":[{
          "startBeat":{"numerator":0,"denominator":1},
          "durationBeats":{"numerator":1,"denominator":1},
          "startValue":[1,1,1],"endValue":[1.5,1.5,1.5],"startSlope":0,"endSlope":0
        }]},
        {"property":"material.opacity","segments":[{
          "startBeat":{"numerator":0,"denominator":1},
          "durationBeats":{"numerator":1,"denominator":1},
          "startValue":1,"endValue":0.5,"startSlope":0,"endSlope":0
        }]},
        {"property":"material.tint","segments":[{
          "startBeat":{"numerator":0,"denominator":1},
          "durationBeats":{"numerator":1,"denominator":1},
          "startValue":[1,1,1],"endValue":[0.5,0.25,0.75],"startSlope":0,"endSlope":0
        }]}
      ],"stepTracks":[]
    }])";

    const auto accepted = cuexis::chart::ChartV4Loader::load(makeChart(parts));
    REQUIRE(accepted.hasValue());
    REQUIRE(accepted.document.has_value());
    const auto& tracks = accepted.document->animationClips.front().tracks;
    REQUIRE(tracks.size() == 4U);
    const auto hasProperty = [&](cuexis::chart::AnimationProperty property) {
        return std::ranges::any_of(
            tracks, [property](const auto& track) { return track.property == property; });
    };
    CHECK(hasProperty(cuexis::chart::AnimationProperty::TransformRotation));
    CHECK(hasProperty(cuexis::chart::AnimationProperty::TransformScale));
    CHECK(hasProperty(cuexis::chart::AnimationProperty::MaterialOpacity));
    CHECK(hasProperty(cuexis::chart::AnimationProperty::MaterialTint));

    auto invalid = makeChart(parts);
    const auto rotation = invalid.find("\"endValue\":[0,0,0,1]");
    REQUIRE(rotation != std::string::npos);
    invalid.replace(rotation, std::string_view{"\"endValue\":[0,0,0,1]"}.size(),
                    "\"endValue\":[0,0,0,2]");
    const auto slopes = invalid.find("\"startSlope\":0,\"endSlope\":0");
    REQUIRE(slopes != std::string::npos);
    invalid.replace(slopes, std::string_view{"\"startSlope\":0,\"endSlope\":0"}.size(),
                    "\"startSlope\":2,\"endSlope\":2");
    const auto opacity = invalid.find("\"endValue\":0.5");
    REQUIRE(opacity != std::string::npos);
    invalid.replace(opacity, std::string_view{"\"endValue\":0.5"}.size(), "\"endValue\":1.5");

    const auto rejected = cuexis::chart::ChartV4Loader::load(invalid);
    CHECK_FALSE(rejected.hasValue());
    CHECK(hasDiagnostic(rejected, "chart.transform.rotation_not_normalized"));
    CHECK(hasDiagnostic(rejected, "chart.animation.clip_invalid"));
    CHECK_FALSE(rejected.document.has_value());
}

TEST_CASE("Chart v4 animator records preserve blend fill iteration and mask boundaries",
          "[chart][v4][reader][animation][options][branch-coverage]") {
    const auto loadAnimator = [](std::string_view mode, std::string_view fillMode,
                                 std::string_view iterations, std::string_view mask) {
        ChartParts parts;
        parts.clips =
            "[" + continuousClip("animation.position", "transform.position.x", "0", "1") + "]";
        parts.objects = std::string{R"([{
          "id":"019f0000-0000-7abc-8def-0000000004f9","parent":null,"components":{
            )"} + std::string{transformComponent()} +
                        R"(,
            "cuexis.animator":{"version":1,"templateBindings":[],"layers":[{
              "layerId":"layer.options","priority":0,"weight":1,"propertyMask":)" +
                        std::string{mask} + R"(,"blendGroups":[{
                "groupId":"group.options","mode":")" +
                        std::string{mode} +
                        R"(","weight":1,"instances":[{
                  "instanceId":"instance.options",
                  "clip":{"domain":"animation","id":"animation.position"},
                  "startBeat":{"numerator":0,"denominator":1},"iterations":)" +
                        std::string{iterations} + R"(,"fillMode":")" + std::string{fillMode} +
                        R"(","weight":1,"propertyMask":)" + std::string{mask} +
                        R"(}]}]}]}
          },"extensions":{}}
        ])";
        return cuexis::chart::ChartV4Loader::load(makeChart(parts));
    };

    constexpr std::string_view positionMask =
        R"({"properties":["transform.position.x"],"prefixes":[]})";
    CHECK(loadAnimator("additive", "hold", "1", positionMask).hasValue());
    CHECK(loadAnimator("override", "none", R"("infinite")", positionMask).hasValue());

    const auto duplicateProperty = loadAnimator(
        "override", "none", "1",
        R"({"properties":["transform.position.x","transform.position.x"],"prefixes":[]})");
    CHECK_FALSE(duplicateProperty.hasValue());
    CHECK(hasDiagnostic(duplicateProperty, "chart.animation.mask_conflict"));

    const auto overlappingPrefix =
        loadAnimator("override", "none", "1",
                     R"({"properties":[],"prefixes":["transform.","transform.position."]})");
    CHECK_FALSE(overlappingPrefix.hasValue());
    CHECK(hasDiagnostic(overlappingPrefix, "chart.animation.mask_conflict"));

    const auto invalidIterations = loadAnimator("override", "none", "0", positionMask);
    CHECK_FALSE(invalidIterations.hasValue());
    CHECK(hasDiagnostic(invalidIterations, "chart.animation.clip_invalid"));
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

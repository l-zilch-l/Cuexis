#include "chart_v4_c2_fixture.hpp"

// Capability, closure, budget, writer and diagnostics cases.

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

TEST_CASE("Chart reader entry points keep empty and minimal input boundaries stable",
          "[chart][cohesion][characterization][empty-minimal]") {
    const auto chartLoaderEmpty = cuexis::chart::ChartLoader::load("");
    const auto canonicalLoaderEmpty = cuexis::chart::CanonicalChartLoader::load("");
    for (const auto* result : {&chartLoaderEmpty, &canonicalLoaderEmpty}) {
        REQUIRE_FALSE(result->hasValue());
        REQUIRE(result->diagnostics.size() == 1);
        CHECK(result->diagnostics.items().front().code() == "json.parse.syntax_error");
        CHECK(result->diagnostics.items().front().fieldPath() == "$");
    }
    const auto v4Empty = cuexis::chart::ChartV4Loader::load("");
    REQUIRE_FALSE(v4Empty.hasValue());
    REQUIRE(v4Empty.diagnostics.size() == 1);
    CHECK(v4Empty.diagnostics.items().front().code() == "json.parse.syntax_error");
    CHECK(v4Empty.diagnostics.items().front().fieldPath() == "$");

    const auto v4 = cuexis::chart::ChartV4Loader::load(makeChart(ChartParts{}));
    REQUIRE(v4.hasValue());
    REQUIRE(v4.document.has_value());
    CHECK(v4.document->legacyProjection.version == 4);
    CHECK(v4.document->legacyProjection.objects.empty());
    const auto secondV4 = cuexis::chart::ChartV4Loader::load(makeChart(ChartParts{}));
    REQUIRE(secondV4.hasValue());
    CHECK(v4.document->canonicalSource.canonicalText ==
          secondV4.document->canonicalSource.canonicalText);
}

TEST_CASE("ChartLoader routing preserves canonical document and diagnostics parity",
          "[chart][cohesion][characterization][routing]") {
    constexpr std::string_view canonical = R"json({
      "format":"cuexis.chart","version":1,
      "chartId":"019b0000-0000-7abc-8def-000000000001","metadata":{},
      "timing":{"offsetMs":0,"defaultBpm":120,"bpmChanges":[],"stops":[]},
      "templates":[],"behaviors":[],"objects":[],
      "requiredExtensions":[],"extensions":{}
    })json";

    const auto routed = cuexis::chart::ChartLoader::load(canonical);
    const auto direct = cuexis::chart::CanonicalChartLoader::load(canonical);
    REQUIRE(routed.hasValue());
    REQUIRE(direct.hasValue());
    REQUIRE(routed.document.has_value());
    REQUIRE(direct.document.has_value());
    CHECK(routed.diagnostics.size() == direct.diagnostics.size());
    CHECK(routed.document->chartId == direct.document->chartId);
    CHECK(routed.document->version == direct.document->version);
    CHECK(routed.document->objects.size() == direct.document->objects.size());
}

TEST_CASE("Chart v4 reader sorts imports while retaining source paths",
          "[chart][cohesion][characterization][diagnostics][order]") {
    ChartParts parts;
    parts.imports = R"([
      {"id":"motion.z","source":"templates/z.cxt"},
      {"id":"motion.a","source":"templates/a.cxt"}
    ])";
    const auto result = cuexis::chart::ChartV4Loader::load(makeChart(parts));
    REQUIRE(result.hasValue());
    REQUIRE(result.document->animationTemplateImports.size() == 2);
    CHECK(result.document->animationTemplateImports[0].id == "motion.a");
    CHECK(result.document->animationTemplateImports[0].source == "templates/a.cxt");
    CHECK(result.document->animationTemplateImports[0].fieldPath == "$/animationTemplateImports/1");
    CHECK(result.document->animationTemplateImports[1].id == "motion.z");
    CHECK(result.document->animationTemplateImports[1].fieldPath == "$/animationTemplateImports/0");
}

TEST_CASE("Chart v4 reader reports portable project path violations at source paths",
          "[chart][cohesion][characterization][cxt][path]") {
    ChartParts parts;
    parts.imports = R"([{"id":"motion.path","source":"templates/../motion.cxt"}])";
    const auto result = cuexis::chart::ChartV4Loader::load(makeChart(parts));
    REQUIRE_FALSE(result.hasValue());
    const auto* diagnostic = findDiagnostic(result, "cxt.template.invalid");
    REQUIRE(diagnostic != nullptr);
    CHECK(diagnostic->fieldPath() == "$/animationTemplateImports/0/source");
}

TEST_CASE("Chart v4 resolver reports invalid project document paths before lookup",
          "[chart][cohesion][characterization][cxt][project-path]") {
    ChartParts parts;
    parts.imports = R"([{"id":"motion.path","source":"templates/path.cxt"}])";
    const auto source = loadSource(makeChart(parts));
    const auto cxt = makeCxt("motion.path", cxtContinuousClip("transform.position.x", "0", "1"));
    const std::array documents{cuexis::chart::ProjectDocument{"templates/../path.cxt", cxt}};

    const auto result = cuexis::chart::ChartV4Resolver::resolve(source, {}, documents);
    REQUIRE_FALSE(result.hasValue());
    const auto* diagnostic = findDiagnostic(result, "cxt.template.invalid");
    REQUIRE(diagnostic != nullptr);
    CHECK(diagnostic->fieldPath() == "$/projectDocuments/0/path");
}

TEST_CASE("Chart v4 diagnostics retain deterministic path ordering",
          "[chart][cohesion][characterization][diagnostics][order]") {
    auto text = makeChart(ChartParts{});
    const auto format = text.find("\"format\":\"cuexis.chart\"");
    REQUIRE(format != std::string::npos);
    text.replace(format, std::string_view{"\"format\":\"cuexis.chart\""}.size(),
                 "\"format\":\"cuexis.chart.future\"");
    const auto version = text.find("\"version\":4");
    REQUIRE(version != std::string::npos);
    text.replace(version, std::string_view{"\"version\":4"}.size(), "\"version\":3");
    const auto chartId = text.find("019f0000-0000-7abc-8def-0000000004c2");
    REQUIRE(chartId != std::string::npos);
    text.replace(chartId, std::string_view{"019f0000-0000-7abc-8def-0000000004c2"}.size(),
                 "not-a-uuid");

    const auto result = cuexis::chart::ChartV4Loader::load(text);
    REQUIRE_FALSE(result.hasValue());
    REQUIRE(result.diagnostics.size() == 5);
    CHECK(result.diagnostics.items()[0].fieldPath() == "$/chartId");
    CHECK(result.diagnostics.items()[0].code() == "chart.chart_id.invalid");
    CHECK(result.diagnostics.items()[1].fieldPath() == "$/chartId");
    CHECK(result.diagnostics.items()[1].code() == "chart.uuid.invalid_v7");
    CHECK(result.diagnostics.items()[2].fieldPath() == "$/format");
    CHECK(result.diagnostics.items()[2].code() == "chart.format.invalid");
    CHECK(result.diagnostics.items()[3].fieldPath() == "$/format");
    CHECK(result.diagnostics.items()[3].code() == "chart.format.unsupported");
    CHECK(result.diagnostics.items()[4].fieldPath() == "$/version");
    CHECK(result.diagnostics.items()[4].code() == "chart.version.unsupported");
}

TEST_CASE("Resolver canonicalizes V4 behavior records and retains their resource closure",
          "[chart][v4][resolver][behavior][determinism][branch-coverage]") {
    ChartParts parts;
    parts.behaviors = R"([
      {
        "id":"behavior.z","type":"behavior.event","version":1,
        "events":[
          {"property":"material.tint","startBeat":{"numerator":2,"denominator":1},
           "durationBeats":{"numerator":1,"denominator":1},
           "startValue":[1,1,1],"endValue":[0,0,0],"startSlope":1,"endSlope":1},
          {"property":"material.opacity","startBeat":{"numerator":0,"denominator":1},
           "durationBeats":{"numerator":1,"denominator":1},
           "startValue":1,"endValue":0.5,"startSlope":1,"endSlope":1}
        ],
        "stepEvents":[
          {"property":"render.visible","beat":{"numerator":2,"denominator":1},
           "value":true},
          {"property":"render.material","beat":{"numerator":1,"denominator":1},
           "value":{"domain":"asset","id":"asset.behavior"}}
        ]
      },
      {
        "id":"behavior.a","type":"behavior.event","version":1,
        "events":[
          {"property":"material.opacity","startBeat":{"numerator":3,"denominator":1},
           "durationBeats":{"numerator":1,"denominator":1},
           "startValue":1,"endValue":1,"startSlope":0,"endSlope":0}
        ],"stepEvents":[]
      }
    ])";

    const auto result = cuexis::chart::ChartV4Resolver::resolve(loadSource(makeChart(parts)));
    REQUIRE(result.hasValue());
    const auto& chart = result.artifact->document.chart;
    REQUIRE(chart.behaviors.size() == 2U);
    CHECK(chart.behaviors[0].id.value == "behavior.a");
    const auto& behavior = chart.behaviors[1];
    REQUIRE(behavior.events.size() == 2U);
    CHECK(behavior.events[0].property == cuexis::chart::BehaviorProperty::MaterialOpacity);
    CHECK(behavior.events[1].property == cuexis::chart::BehaviorProperty::MaterialTint);
    REQUIRE(behavior.stepEvents.size() == 2U);
    CHECK(behavior.stepEvents[0].property == cuexis::chart::BehaviorStepProperty::RenderVisible);
    CHECK(behavior.stepEvents[1].property == cuexis::chart::BehaviorStepProperty::RenderMaterial);

    const auto* resource = findResource(*result.artifact, "asset.behavior");
    REQUIRE(resource != nullptr);
    CHECK(resource->uses == std::vector<cuexis::chart::ChartResourceUse>{
                                cuexis::chart::ChartResourceUse::BehaviorMaterial});
}

TEST_CASE("Resolver expands prefix property masks for concrete animation validation",
          "[chart][v4][resolver][animation][mask][branch-coverage]") {
    ChartParts parts;
    parts.clips = "[" + continuousClip("animation.prefix", "transform.position.x", "0", "1") + "]";
    parts.objects = std::string{R"([
      {"id":"019f0000-0000-7abc-8def-0000000004f8","parent":null,"components":{
        )"} + std::string{transformComponent()} +
                    R"(,
        "cuexis.animator":{"version":1,"templateBindings":[],"layers":[{
          "layerId":"layer.prefix","priority":0,"weight":1,
          "propertyMask":{"properties":[],"prefixes":["transform."]},
          "blendGroups":[{
            "groupId":"group.prefix","mode":"override","weight":1,"instances":[{
              "instanceId":"instance.prefix",
              "clip":{"domain":"animation","id":"animation.prefix"},
              "startBeat":{"numerator":0,"denominator":1},"iterations":1,
              "fillMode":"none","weight":1,
              "propertyMask":{"properties":[],"prefixes":["transform."]}
            }]
          }]
        }]}
      },"extensions":{}}
    ])";

    const auto result = cuexis::chart::ChartV4Resolver::resolve(loadSource(makeChart(parts)));
    REQUIRE(result.hasValue());
    REQUIRE(result.artifact->animationProgram.objects.size() == 1U);
    const auto& layer = result.artifact->animationProgram.objects.front().layers.front();
    CHECK(layer.propertyMask.prefixes == std::vector<std::string>{"transform."});
    CHECK(result.artifact->capabilityRequirements ==
          std::vector<std::string>{"cuexis.animation.clip.v1", "cuexis.animation.layers.v1",
                                   "cuexis.chart.v4"});
}

TEST_CASE("Chart v4 canonical source and resolved identity are whitespace invariant",
          "[chart][cohesion][characterization][identity]") {
    const auto firstText = makeChart(ChartParts{});
    auto secondText = firstText;
    secondText.insert(0, "  \n");
    secondText.append("\n");

    const auto first = cuexis::chart::ChartV4Loader::load(firstText);
    const auto second = cuexis::chart::ChartV4Loader::load(secondText);
    REQUIRE(first.hasValue());
    REQUIRE(second.hasValue());
    CHECK(first.document->canonicalSource.canonicalText ==
          second.document->canonicalSource.canonicalText);

    const auto firstResolved = cuexis::chart::ChartV4Resolver::resolve(*first.document);
    const auto secondResolved = cuexis::chart::ChartV4Resolver::resolve(*second.document);
    REQUIRE(firstResolved.hasValue());
    REQUIRE(secondResolved.hasValue());
    CHECK(firstResolved.artifact->chartIdentity == secondResolved.artifact->chartIdentity);
}

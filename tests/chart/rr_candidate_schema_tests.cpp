#include <cuexis/chart/chart_v5_document.hpp>
#include <cuexis/chart/limits.hpp>
#include <cuexis/json/parse.hpp>
#include <cuexis/json/schema.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using cuexis::chart::BeatPhaseKind;
using cuexis::chart::ChartLimits;
using cuexis::chart::ChartV5Loader;
using cuexis::chart::ChartV5SourceDocument;
using cuexis::chart::ChartV5SourceResult;

[[nodiscard]] auto readFile(const std::filesystem::path& path) -> std::string {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw std::runtime_error{"Could not open artifact: " + path.string()};
    }
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

[[nodiscard]] auto parseArtifact(const std::filesystem::path& path) -> cuexis::json::Value {
    auto parsed = cuexis::json::parse(
        readFile(path), cuexis::json::ParseLimits{16U * 1024U * 1024U, 96, 1024U * 1024U});
    REQUIRE(parsed.has_value());
    return std::move(*parsed);
}

[[nodiscard]] auto authorSchema() -> const cuexis::json::Value& {
    static const auto schema =
        parseArtifact(std::filesystem::path{CUEXIS_SOURCE_DIR} / "schemas" / "candidate" /
                      "cuexis.gameplay-author.beat-us.v3.schema.json");
    return schema;
}

[[nodiscard]] auto chartSchema() -> const cuexis::json::Value& {
    static const auto schema = parseArtifact(std::filesystem::path{CUEXIS_SOURCE_DIR} / "schemas" /
                                             "candidate" / "cuexis.chart.beat-us.v5.schema.json");
    return schema;
}

void expectSchemaValid(const cuexis::json::Value& schema, std::string_view instanceText) {
    auto instance = cuexis::json::parse(
        instanceText, cuexis::json::ParseLimits{16U * 1024U * 1024U, 96, 1024U * 1024U});
    REQUIRE(instance.has_value());
    cuexis::core::Diagnostics diagnostics;
    const auto result = cuexis::json::validateAgainstSchema(*instance, schema, diagnostics);
    REQUIRE(result.has_value());
    std::string report;
    for (const auto& diagnostic : diagnostics.items()) {
        report += std::string{diagnostic.code()} + " at " + std::string{diagnostic.fieldPath()} +
                  ": " + std::string{diagnostic.message()} + "\n";
    }
    INFO(instanceText);
    INFO(report);
    CHECK_FALSE(diagnostics.hasErrors());
}

void expectSchemaInvalid(const cuexis::json::Value& schema, std::string_view instanceText) {
    INFO(instanceText);
    auto instance = cuexis::json::parse(
        instanceText, cuexis::json::ParseLimits{16U * 1024U * 1024U, 96, 1024U * 1024U});
    REQUIRE(instance.has_value());
    cuexis::core::Diagnostics diagnostics;
    const auto result = cuexis::json::validateAgainstSchema(*instance, schema, diagnostics);
    REQUIRE(result.has_value());
    CHECK(diagnostics.hasErrors());
}

[[nodiscard]] auto replaceOnce(std::string text, std::string_view from, std::string_view to)
    -> std::string {
    const auto position = text.find(from);
    REQUIRE(position != std::string::npos);
    text.replace(position, from.size(), to);
    return text;
}

//  GP-01 author payload with one declared tap/1 phase. `windowOrdinal` changes only the
//  success window's author declaration ordinal; both values are shape-legal at this stage.
[[nodiscard]] auto gameplayPayload(int windowOrdinal = 1) -> std::string {
    std::string payload = R"({
  "version": 3,
  "authorProfile": "gameplay.author.beat-us.v1",
  "sourceDocumentId": "doc.chart",
  "common": {
    "chartEntryId": "chart.entry.one",
    "graphRevision": "3",
    "normalizationProfileToken": "gameplay.normalization.v1",
    "coordinatorPolicy": "coordinator.policy.greedy_v1",
    "executionProfile": "gameplay.execution.t4-k4.v1",
    "latePolicy": {
      "mode": "queue_next_tick",
      "finalizationWatermark": {"state": "measured", "value": "8"},
      "maxQueueHop": {"state": "measured", "value": "4"},
      "windowCloseThreshold": {"state": "measured", "value": "3"},
      "windowOpenThreshold": {"state": "measured", "value": "2"}
    },
    "gradeTables": [
      {"id": "main", "exact": {"startMs": -10, "endMs": 15},
       "intervals": [
         {"grade": "Good", "startMs": 10, "endMs": 50},
         {"grade": "Great", "startMs": -10, "endMs": 10},
         {"grade": "Good", "startMs": -50, "endMs": -10}
       ]}
    ],
    "noteTypes": [
      {"id": "tap", "gradeTables": [{"phase": "tap", "gradeTable": "main"}]}
    ],
    "scoreMode": {
      "id": "standard", "total": 100, "mj": 90, "mc": 10, "onExhausted": "reset",
      "comboRewards": [{"from": 1, "rewardAtFrom": 1, "slope": 1}],
      "rules": [
        {"phase": "tap", "outcome": "hit", "grade": "Good", "qualifies": true,
         "judgePoints": 1, "combo": "increment"},
        {"phase": "tap", "outcome": "miss", "grade": null, "qualifies": false,
         "judgePoints": 0, "combo": "keep"},
        {"phase": "body", "outcome": "miss", "grade": "Great", "qualifies": true,
         "judgePoints": 1, "combo": "keep"},
        {"phase": "tail", "outcome": "miss", "grade": "Good", "qualifies": true,
         "judgePoints": 1, "combo": "keep"}
      ]
    },
    "graceInputs": [], "declarations": [], "resources": [], "relations": [],
    "solverProfiles": [], "factBindings": [], "judgementDomains": [], "declaredFeatures": [],
    "declaredCapabilities": [], "namedGraceDurations": [], "patternDefinitions": [],
    "measureDefinitions": []
  },
  "requirements": [
    {
      "stableId": {"sourceDocumentId": "doc.chart", "declarationOrdinal": 0},
      "identity": {
        "chartEntryId": "chart.entry.one", "invocationId": "invocation.one",
        "moduleId": "__chart__", "exportId": "__object__",
        "emissionPath": [{"nodeId": "__direct__", "repeatIndex": "0"}],
        "requirementLocalId": "req.one"
      },
      "requiredActions": [], "domainBinding": "domain.one",
      "judgementDomainId": "domain.one",
      "phases": [{"kind": "tap", "declarationOrdinal": 1}],
      "requiresReleaseTailSemantics": false,
      "pattern": {
        "patternId": "pattern.one", "matchPolicy": "leftmost-first",
        "root": {"primitive": "atom", "operands": [], "atomRef": "atom.one"},
        "requiredRefs": {"features": [], "capabilities": []}
      },
      "patternArmRefs": [],
      "maxArmElements": {"state": "measured", "value": "16"},
      "maxDeadlineElements": {"state": "measured", "value": "16"},
      "measure": {
        "components": [{"phase": "tap", "categoryToken": "tap", "gradeTokens": []}],
        "requiredRefs": {"features": [], "capabilities": []}
      },
      "resourceClaims": [],
      "grace": {"policy": "default", "allowChartGrace": false},
      "timing": {
        "endBeat": {"numerator": 21, "denominator": 10},
        "phaseTargets": [{"phase": "tap", "beat": {"numerator": 2, "denominator": 1}}],
        "successWindows": [
          {"phase": {"kind": "tap", "declarationOrdinal": @WINDOW_ORDINAL@},
           "startMs": -50, "endMs": 50}
        ]
      },
      "atomBindings": [], "solverProfileRef": "solver.one",
      "localClosePolicyToken": "close.one", "factBindingRefs": [],
      "requiredRefs": {"features": [], "capabilities": []},
      "noteType": "tap",
      "gradeTables": [{"phase": "tap", "gradeTable": "main"}],
      "scoring": []
    }
  ],
  "rankAssignment": {"mode": "explicit", "rows": []}
})";
    return replaceOnce(std::move(payload), "@WINDOW_ORDINAL@", std::to_string(windowOrdinal));
}

[[nodiscard]] auto chartText(
    std::string_view payload, std::string_view objects = "[]", std::string_view templates = "[]",
    std::string_view behaviors = "[]", std::string_view imports = "[]",
    std::string_view clips = "[]", std::string_view framings = "", std::string_view stops = "[]",
    std::string_view extensions = R"([{"id":"cuexis.gameplay.v3","version":3}])") -> std::string {
    std::string text = R"({
  "format": "cuexis.chart",
  "version": 5,
  "chartId": "019c0000-0000-7abc-8def-000000000003",
  "metadata": {},
  "timing": {"offsetMs": 0, "defaultBpm": 120, "tempoEvents": [], "stops": )";
    text += std::string{stops};
    text += R"(},
  "parameters": [],
  "templates": )";
    text += std::string{templates};
    text += R"(,
  "behaviors": )";
    text += std::string{behaviors};
    text += R"(,
  "animationTemplateImports": )";
    text += std::string{imports};
    text += R"(,
  "animationClips": )";
    text += std::string{clips};
    text += R"(,
  "objects": )";
    text += std::string{objects};
    text += R"(,
  "gameplay": )";
    text += std::string{payload};
    text += R"(,
  "requiredExtensions": )";
    text += std::string{extensions};
    text += R"(,
  "extensions": {})";
    if (!framings.empty()) {
        text += R"(,
  "framings": )";
        text += std::string{framings};
    }
    text += "\n}\n";
    return text;
}

[[nodiscard]] auto accepted(std::string_view text) -> ChartV5SourceDocument {
    const auto result = ChartV5Loader::load(text, ChartLimits{});
    std::string report;
    for (const auto& diagnostic : result.diagnostics.items()) {
        report += std::string{diagnostic.code()} + " at " + std::string{diagnostic.fieldPath()} +
                  ": " + std::string{diagnostic.message()} + "\n";
    }
    INFO(report);
    REQUIRE(result.hasValue());
    return *result.document;
}

[[nodiscard]] auto rejected(std::string_view text) -> cuexis::core::Diagnostics {
    auto result = ChartV5Loader::load(text, ChartLimits{});
    std::string report;
    for (const auto& diagnostic : result.diagnostics.items()) {
        report += std::string{diagnostic.code()} + " at " + std::string{diagnostic.fieldPath()} +
                  ": " + std::string{diagnostic.message()} + "\n";
    }
    CHECK_FALSE(result.hasValue());
    CHECK(result.diagnostics.hasErrors());
    return std::move(result.diagnostics);
}

[[nodiscard]] auto hasCode(const cuexis::core::Diagnostics& diagnostics, std::string_view code)
    -> bool {
    return std::ranges::any_of(diagnostics.items(),
                               [&](const auto& item) { return item.code() == code; });
}

//  A source-value negative may be caught by the candidate schema (`json.schema.validation_failed`)
//  or by the typed source pass (`judgement.execution.relation_invalid` /
//  `chart.v5.source_shape_invalid`). Both are source rejections; the test only requires that the
//  document is not accepted.
[[nodiscard]] auto isRejected(const cuexis::core::Diagnostics& diagnostics) -> bool {
    return hasCode(diagnostics, "judgement.execution.relation_invalid") ||
           hasCode(diagnostics, "json.schema.validation_failed") ||
           hasCode(diagnostics, "chart.v5.source_shape_invalid");
}

constexpr auto kClipTransformOnly = R"([
  {"id": "clip.move", "version": 2, "durationBeats": {"numerator": 2, "denominator": 1},
   "tracks": [
     {"property": "transform.position.x", "segments": [
       {"startBeat": {"numerator": 0, "denominator": 1},
        "durationBeats": {"numerator": 1, "denominator": 1},
        "startValue": 0, "endValue": 1, "startSlope": 1, "endSlope": 1}
     ]}
   ],
   "stepTracks": [
     {"property": "render.visible", "steps": [
       {"beat": {"numerator": 0, "denominator": 1}, "value": true}
     ]}
   ]}
])";

constexpr auto kClipAlpha = R"([
  {"id": "clip.alpha", "version": 2, "durationBeats": {"numerator": 1, "denominator": 1},
   "tracks": [
     {"property": "render.alpha", "segments": [
       {"startBeat": {"numerator": 0, "denominator": 1},
        "durationBeats": {"numerator": 1, "denominator": 1},
        "startValue": 0, "endValue": 255, "startSlope": 0, "endSlope": 0}
     ]}
   ], "stepTracks": []}
])";

constexpr auto kBehaviorEvent2 = R"([
  {"id": "behavior.alpha", "type": "behavior.event", "version": 2,
   "events": [
     {"property": "render.alpha",
      "startBeat": {"numerator": 0, "denominator": 1},
      "durationBeats": {"numerator": 1, "denominator": 1},
      "startValue": 128, "endValue": 255, "startSlope": 0, "endSlope": 0}
   ],
   "stepEvents": []}
])";

constexpr auto kImports = R"([{"id": "import.one", "source": "templates/move-y.cxt"}])";

constexpr auto kObjects = R"([
  {"id": "019c0000-0000-5abc-8def-000000000001", "parent": null, "extensions": {},
   "components": {
     "cuexis.renderable": {"version": 1,
       "mesh": {"domain": "asset", "id": "mesh.note"},
       "material": {"domain": "asset", "id": "material.note"},
       "alpha": 128}
   }}
])";

constexpr auto kTemplatesAnimator = R"([
  {"id": "019c0000-0000-5abc-8def-000000000002", "extends": null, "extensions": {},
   "prototype": {"components": {
     "cuexis.animator": {
       "version": 1,
       "templateBindings": [
         {"bindingId": "bind.one",
          "template": {"domain": "animation-template", "id": "import.one"},
          "startBeat": {"numerator": 0, "denominator": 1},
          "durationScale": {"numerator": 1, "denominator": 1}, "weight": 1.0, "priority": 0}
       ],
       "layers": [
         {"layerId": "layer.one", "priority": 0, "weight": 1.0,
          "propertyMask": {"properties": ["render.alpha"], "prefixes": ["transform."]},
          "blendGroups": [
            {"groupId": "group.one", "mode": "override", "weight": 1.0,
             "instances": [
               {"instanceId": "inst.one",
                "clip": {"domain": "animation", "id": "clip.move"},
                "startBeat": {"numerator": 0, "denominator": 1},
                "iterations": 1, "fillMode": "none", "weight": 1.0,
                "propertyMask": {"properties": ["render.alpha"], "prefixes": []}}
             ]}
          ]}
       ]
     }
   }}}
])";

} // namespace

TEST_CASE("rr_candidate_schema", "[chart][candidate][v5]") {

    SECTION("the authored v3 schema accepts GP-01 and rejects the deltas") {
        expectSchemaValid(authorSchema(), gameplayPayload());
        expectSchemaInvalid(authorSchema(), replaceOnce(gameplayPayload(), R"("scoreMode": {)",
                                                        R"("scoreModeX": {)"));

        auto missingExact = replaceOnce(gameplayPayload(),
                                        R"({"id": "main", "exact": {"startMs": -10, "endMs": 15},)",
                                        R"({"id": "main",)");
        expectSchemaInvalid(authorSchema(), missingExact);

        auto disabledFalse = replaceOnce(gameplayPayload(), R"("scoring": [])",
                                         R"("scoring": [{"phase": "tap", "disabled": false}])");
        expectSchemaInvalid(authorSchema(), disabledFalse);
        expectSchemaInvalid(authorSchema(), replaceOnce(gameplayPayload(), R"("judgePoints": 1,)",
                                                        R"("judgePoints": 1.5,)"));

        auto oldScore = replaceOnce(
            gameplayPayload(), R"("gradeTables": [{"phase": "tap", "gradeTable": "main"}])",
            R"("gradeTables": [{"phase": "tap", "gradeTable": "main"}], "score": 1)");
        expectSchemaInvalid(authorSchema(), oldScore);
        expectSchemaInvalid(authorSchema(),
                            replaceOnce(gameplayPayload(), R"("graphRevision": "3")",
                                        R"("graphRevision": "3", "rulesetRef": "ruleset.old")"));
    }

    SECTION("the chart v5 schema accepts the base shell and rejects old presentation syntax") {
        const auto payload = gameplayPayload();
        expectSchemaValid(chartSchema(), chartText(payload));

        expectSchemaInvalid(chartSchema(),
                            replaceOnce(chartText(payload, kObjects, kTemplatesAnimator,
                                                  kBehaviorEvent2, kImports, kClipTransformOnly),
                                        R"("version": 2, "durationBeats")",
                                        R"("version": 1, "durationBeats")"));
        expectSchemaInvalid(chartSchema(),
                            replaceOnce(chartText(payload, "[]", "[]", kBehaviorEvent2),
                                        R"("version": 2,)", R"("version": 1,)"));
        expectSchemaInvalid(chartSchema(),
                            replaceOnce(chartText(payload, "[]", "[]", kBehaviorEvent2),
                                        R"("type": "behavior.event")",
                                        R"("type": "behavior.transform.keyframe")"));
        expectSchemaInvalid(chartSchema(),
                            replaceOnce(chartText(payload, kObjects, "[]", "[]", "[]", kClipAlpha),
                                        R"("property": "render.alpha")",
                                        R"("property": "material.opacity")"));
        for (const auto* alpha : {"255.0", "-1", "256"}) {
            expectSchemaInvalid(
                chartSchema(),
                replaceOnce(chartText(payload, kObjects, "[]", "[]", "[]", kClipAlpha),
                            R"("startValue": 0, "endValue": 255, "startSlope": 0, "endSlope": 0)",
                            std::string{R"("startValue": 0, "endValue": )"} + alpha +
                                R"(, "startSlope": 0, "endSlope": 0)"));
        }
        expectSchemaInvalid(chartSchema(),
                            chartText(payload, "[]", "[]", "[]", "[]", "[]", "",
                                      R"([{"id": "capability.test", "version": 1}])"));
        expectSchemaInvalid(
            chartSchema(), replaceOnce(chartText(payload), R"("version": 3,)", R"("version": 2,)"));
    }

    SECTION("typed parse keeps the window phase ordinal and normalizes the table") {
        const auto document = accepted(chartText(gameplayPayload(2)));
        const auto& requirement = document.gameplay.requirements.front();
        REQUIRE(requirement.timing.successWindows.size() == 1);
        CHECK(requirement.timing.successWindows.front().phase.kind == BeatPhaseKind::tap);
        CHECK(requirement.timing.successWindows.front().phase.declarationOrdinal == 2);
        CHECK(requirement.phases.front().declarationOrdinal == 1);
        CHECK(requirement.timing.endBeat.numerator == 21);
        CHECK(requirement.timing.endBeat.denominator == 10);
        CHECK(requirement.timing.phaseTargets.front().beat.numerator == 2);

        const auto& table = document.gameplay.common.gradeTables.front();
        REQUIRE(table.intervals.size() == 3);
        CHECK(table.intervals[0].startMs == -50.0);
        CHECK(table.intervals[1].startMs == -10.0);
        CHECK(table.intervals[2].startMs == 10.0);
        CHECK(table.exactStartMs == -10.0);
        CHECK(table.exactEndMs == 15.0);

        const auto& mode = *document.gameplay.common.scoreMode;
        CHECK(mode.comboRewards.front().from == 1);
        REQUIRE(mode.rules.size() == 4);
        CHECK(mode.rules.front().phase == BeatPhaseKind::tap);
        CHECK(mode.rules.front().hit);
        CHECK(mode.rules.back().phase == BeatPhaseKind::tail);
        CHECK(mode.rules.back().grade.has_value());
    }

    SECTION("table source values reject the geometry and key negatives") {
        const auto text = chartText(gameplayPayload());
        for (const auto& mutation :
             {replaceOnce(text, R"("intervals": [
         {"grade": "Good", "startMs": 10, "endMs": 50},
         {"grade": "Great", "startMs": -10, "endMs": 10},
         {"grade": "Good", "startMs": -50, "endMs": -10}
       ]})",
                          R"("intervals": []})"),
              replaceOnce(text, R"("startMs": -50, "endMs": -10)",
                          R"("startMs": -10, "endMs": -10)"),
              replaceOnce(text, R"("startMs": 10, "endMs": 50)", R"("startMs": 5, "endMs": 50)"),
              replaceOnce(text, R"("exact": {"startMs": -10, "endMs": 15})",
                          R"("exact": {"startMs": 1, "endMs": 15})"),
              replaceOnce(text, R"("exact": {"startMs": -10, "endMs": 15})",
                          R"("exact": {"startMs": -10, "endMs": 0})"),
              replaceOnce(text, R"("gradeTables": [
      {"id": "main",)",
                          R"("gradeTables": [
      {"id": "main", "exact": {"startMs": -10, "endMs": 15},
       "intervals": [{"grade": "Good", "startMs": -50, "endMs": -10}]},
      {"id": "main",)")}) {
            const auto diagnostics = rejected(mutation);
            CHECK(isRejected(diagnostics));
        }
        //  The same table declared in another order is accepted and normalized.
        CHECK(accepted(text).gameplay.common.gradeTables.front().intervals.size() == 3);
    }

    SECTION("selection and uniqueness negatives stay source rejections") {
        const auto text = chartText(gameplayPayload());
        for (const auto& mutation :
             {replaceOnce(text, R"({"phase": "tap", "gradeTable": "main"})",
                          R"({"phase": "tap", "gradeTable": "missing"})"),
              replaceOnce(text, R"("gradeTables": [{"phase": "tap", "gradeTable": "main"}])",
                          R"("gradeTables": [{"phase": "tap", "gradeTable": "main"},
                                             {"phase": "tap", "gradeTable": "main"}])"),
              replaceOnce(text, R"("noteTypes": [
      {"id": "tap", "gradeTables": [{"phase": "tap", "gradeTable": "main"}]}
    ])",
                          R"("noteTypes": [
      {"id": "tap", "gradeTables": [{"phase": "tap", "gradeTable": "main"}]},
      {"id": "tap", "gradeTables": []}
    ])")}) {
            CHECK(isRejected(rejected(mutation)));
        }
        //  A NoteType phase selection that the current instance does not use is legal.
        CHECK(accepted(replaceOnce(text, R"({"phase": "tap", "gradeTable": "main"}]})",
                                   R"({"phase": "tap", "gradeTable": "main"},
                                      {"phase": "body", "gradeTable": "main"}]})"))
                  .gameplay.common.noteTypes.front()
                  .gradeTables.size() == 2);
    }

    SECTION("score mode source values reject the reward, key and qualification negatives") {
        const auto text = chartText(gameplayPayload());
        for (const auto& mutation :
             {replaceOnce(text, R"("onExhausted": "reset")", R"("onExhausted": "increment")"),
              replaceOnce(text, R"("comboRewards": [{"from": 1, "rewardAtFrom": 1, "slope": 1}])",
                          R"("comboRewards": [])"),
              replaceOnce(text, R"("comboRewards": [{"from": 1, "rewardAtFrom": 1, "slope": 1}])",
                          R"("comboRewards": [{"from": 2, "rewardAtFrom": 1, "slope": 1}])"),
              replaceOnce(text, R"("comboRewards": [{"from": 1, "rewardAtFrom": 1, "slope": 1}])",
                          R"("comboRewards": [{"from": 1, "rewardAtFrom": 1, "slope": 0},
                                              {"from": 1, "rewardAtFrom": 1, "slope": 1}])"),
              replaceOnce(text, R"("comboRewards": [{"from": 1, "rewardAtFrom": 1, "slope": 1}])",
                          R"("comboRewards": [{"from": 1, "rewardAtFrom": 5, "slope": 0},
                                              {"from": 2, "rewardAtFrom": 4, "slope": 0}])"),
              replaceOnce(text, R"("qualifies": false,
         "judgePoints": 0, "combo": "keep")",
                          R"("qualifies": false,
         "judgePoints": 3, "combo": "keep")"),
              replaceOnce(text, R"("qualifies": false,
         "judgePoints": 0, "combo": "keep")",
                          R"("qualifies": false,
         "judgePoints": 0, "combo": "increment")"),
              replaceOnce(
                  text,
                  R"({"phase": "tail", "outcome": "miss", "grade": "Good", "qualifies": true,)",
                  R"({"phase": "tap", "outcome": "miss", "grade": null, "qualifies": true,)")}) {
            CHECK(isRejected(rejected(mutation)));
        }
        //  The zero-reward segment and the zero-point keep row are legal on their own.
        auto zeroReward =
            replaceOnce(text, R"("comboRewards": [{"from": 1, "rewardAtFrom": 1, "slope": 1}])",
                        R"("comboRewards": [{"from": 1, "rewardAtFrom": 0, "slope": 0}])");
        CHECK(accepted(zeroReward).gameplay.common.scoreMode->comboRewards.front().slope == 0);
    }

    SECTION("framings keep the author pair and normalize the projection") {
        const auto document = accepted(chartText(
            gameplayPayload(), "[]", "[]", "[]", "[]", "[]",
            R"([{"width": 42, "height": 18}, {"width": 9, "height": 16, "verticalScale": 2}])"));
        REQUIRE(document.framings.size() == 2);
        CHECK(document.framings[0].width == 42);
        CHECK(document.framings[0].height == 18);
        CHECK_FALSE(document.framings[0].verticalScale.has_value());
        CHECK(document.framings[1].verticalScale == 2.0);
        const auto& normalized = document.normalizedFramings;
        REQUIRE(normalized.size() == 2);
        CHECK(normalized[0].width == 7);
        CHECK(normalized[0].height == 3);
        CHECK(normalized[0].verticalScale == 1.0);
        CHECK(normalized[1].width == 9);
        CHECK(normalized[1].height == 16);

        for (const auto* invalid :
             {R"([])", R"([{"width": 0, "height": 18}])",
              R"([{"width": 2147483648, "height": 18}])",
              R"([{"width": 42, "height": 18, "verticalScale": 0.124}])",
              R"([{"width": 42, "height": 18, "verticalScale": 8.001}])",
              R"([{"width": 42, "height": 18, "centerX": 0}])",
              R"([{"width": 42, "height": 18}, {"width": 21, "height": 9}])"}) {
            const auto diagnostics =
                rejected(chartText(gameplayPayload(), "[]", "[]", "[]", "[]", "[]", invalid));
            CHECK((hasCode(diagnostics, "chart.framing.invalid") ||
                   hasCode(diagnostics, "chart.framing.duplicate_aspect") ||
                   hasCode(diagnostics, "json.schema.validation_failed")));
        }
    }

    SECTION("local presentation shapes accept Clip2, event2, alpha and mask") {
        const auto document = accepted(chartText(gameplayPayload(), kObjects, kTemplatesAnimator,
                                                 kBehaviorEvent2, kImports, kClipTransformOnly));
        REQUIRE(document.animationClips.size() == 1);
        CHECK(document.animationClips.front().version == 2);
        REQUIRE(document.behaviors.size() == 1);
        CHECK(document.behaviors.front().version == 2);
        REQUIRE(document.animatorBindings.size() == 1);
        CHECK(document.animatorBindings.front().templateId == "import.one");
        REQUIRE(document.animatorMasks.size() == 2);
        CHECK(document.animatorMasks.front().properties ==
              std::vector<std::string>{"render.alpha"});
    }

    SECTION("alpha tokens and the v5 mask whitelist reject old syntax") {
        const auto clips = std::string{kClipAlpha};
        for (const auto* alpha : {"255.0", "-1", "256"}) {
            const auto mutated = replaceOnce(
                clips, R"("startValue": 0, "endValue": 255, "startSlope": 0, "endSlope": 0)",
                std::string{R"("startValue": 0, "endValue": )"} + alpha +
                    R"(, "startSlope": 0, "endSlope": 0)");
            const auto diagnostics =
                rejected(chartText(gameplayPayload(), "[]", "[]", "[]", "[]", mutated));
            CHECK((hasCode(diagnostics, "json.schema.validation_failed") ||
                   hasCode(diagnostics, "judgement.execution.relation_invalid")));
        }
        const auto mask =
            replaceOnce(std::string{kTemplatesAnimator}, R"("properties": ["render.alpha"])",
                        R"("properties": ["material.opacity"])");
        CHECK(
            hasCode(rejected(chartText(gameplayPayload(), "[]", mask, "[]", kImports, kClipAlpha)),
                    "judgement.execution.relation_invalid"));
        //  A binding whose template import does not exist is a local-shape rejection.
        const auto missingBinding = replaceOnce(std::string{kTemplatesAnimator},
                                                R"("id": "import.one")", R"("id": "missing.one")");
        CHECK(isRejected(rejected(
            chartText(gameplayPayload(), "[]", missingBinding, "[]", kImports, kClipAlpha))));
        //  A clip instance must refer to a local AnimationClip.
        const auto missingClip = replaceOnce(std::string{kTemplatesAnimator},
                                             R"("id": "clip.move")", R"("id": "clip.missing")");
        CHECK(isRejected(
            rejected(chartText(gameplayPayload(), "[]", missingClip, "[]", kImports, kClipAlpha))));
        //  The Animator component itself keeps component version 1.
        CHECK(hasCode(rejected(chartText(gameplayPayload(), "[]",
                                         replaceOnce(std::string{kTemplatesAnimator},
                                                     R"("version": 1,)", R"("version": 2,)"),
                                         "[]", kImports, kClipAlpha)),
                      "json.schema.validation_failed"));
    }

    SECTION("Stop shape and the old v5/v2 path") {
        for (
            const auto* stop :
            {R"([{"beat": {"numerator": 1, "denominator": 1}, "durationMs": 20, "endBeat": {"numerator": 2, "denominator": 1}}])",
             R"([{"beat": {"numerator": 1, "denominator": 1}, "durationMs": 0}])"}) {
            const auto diagnostics =
                rejected(chartText(gameplayPayload(), "[]", "[]", "[]", "[]", "[]", "", stop));
            CHECK((hasCode(diagnostics, "json.schema.validation_failed") ||
                   hasCode(diagnostics, "judgement.execution.relation_invalid") ||
                   hasCode(diagnostics, "chart.v5.source_shape_invalid")));
        }
        auto v2 = replaceOnce(gameplayPayload(), R"("version": 3,)", R"("version": 2,)");
        const auto diagnostics = rejected(chartText(v2));
        CHECK(hasCode(diagnostics, "format.gameplay_version_unsupported"));
        CHECK(ChartV5Loader::isV5(chartText(gameplayPayload()), ChartLimits{}));
        CHECK_FALSE(ChartV5Loader::isV5(std::string{"{\"format\":\"cuexis.chart\",\"version\":4}"},
                                        ChartLimits{}));
    }
}

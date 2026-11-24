#include <cuexis/chart/chart_v5_document.hpp>

#include <cuexis/chart/uuid.hpp>
#include <cuexis/json/parse.hpp>
#include <cuexis/json/reader.hpp>
#include <cuexis/json/schema.hpp>

#include "chart_v5_schema.hpp"
#include "diagnostic_limit.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cuexis::chart {
namespace {

using json::Value;

constexpr std::string_view kRelationInvalid = "judgement.execution.relation_invalid";
constexpr std::string_view kSourceShape = "chart.v5.source_shape_invalid";

constexpr auto phaseOrder(BeatPhaseKind kind) -> int {
    switch (kind) {
    case BeatPhaseKind::tap:
        return 0;
    case BeatPhaseKind::head:
        return 1;
    case BeatPhaseKind::body:
        return 2;
    case BeatPhaseKind::tail:
        return 3;
    }
    return 4;
}

void addParseError(core::Diagnostics& diagnostics, const core::Error& error) {
    auto diagnostic = core::Diagnostic{core::DiagnosticSeverity::Error, std::string{error.code()},
                                       std::string{error.message()}, "$"};
    for (const auto& context : error.context()) {
        diagnostic.withContext(context.key, context.value);
    }
    static_cast<void>(diagnostics.add(std::move(diagnostic)));
}

void addError(core::Diagnostics& diagnostics, std::string_view code, std::string message,
              std::string path) {
    static_cast<void>(diagnostics.add(core::Diagnostic{
        core::DiagnosticSeverity::Error, std::string{code}, std::move(message), std::move(path)}));
}

//  Author-source relation failures reuse the registered judgement relation code, with the
//  same category / severity / faulted context the old author reader reports.
void addRelationError(core::Diagnostics& diagnostics, std::string message, std::string path) {
    auto diagnostic =
        core::Diagnostic{core::DiagnosticSeverity::Error, std::string{kRelationInvalid},
                         std::move(message), std::move(path)};
    diagnostic.withContext("category", "invalid_relation")
        .withContext("severity", "error")
        .withContext("faulted", "false");
    static_cast<void>(diagnostics.add(std::move(diagnostic)));
}

void addShapeError(core::Diagnostics& diagnostics, std::string message, std::string path) {
    addError(diagnostics, kSourceShape, std::move(message), std::move(path));
}

auto member(const Value& object, std::string_view name) -> const Value* {
    return object.find(name);
}

auto asObject(const Value* value) -> const Value::Object* {
    return value != nullptr ? value->object() : nullptr;
}

auto asArray(const Value* value) -> const Value::Array* {
    return value != nullptr ? value->array() : nullptr;
}

auto asString(const Value* value, std::string& out) -> bool {
    if (value == nullptr) {
        return false;
    }
    const auto* text = value->string();
    if (text == nullptr) {
        return false;
    }
    out = *text;
    return true;
}

auto exactI64(const Value* value, std::int64_t& out) -> bool {
    if (value == nullptr) {
        return false;
    }
    if (const auto* signedValue = value->signedInteger()) {
        out = *signedValue;
        return true;
    }
    if (const auto* unsignedValue = value->unsignedInteger();
        unsignedValue != nullptr &&
        *unsignedValue <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        out = static_cast<std::int64_t>(*unsignedValue);
        return true;
    }
    return false;
}

auto exactU64(const Value* value, std::uint64_t& out) -> bool {
    if (value == nullptr) {
        return false;
    }
    if (const auto* unsignedValue = value->unsignedInteger()) {
        out = *unsignedValue;
        return true;
    }
    if (const auto* signedValue = value->signedInteger();
        signedValue != nullptr && *signedValue >= 0) {
        out = static_cast<std::uint64_t>(*signedValue);
        return true;
    }
    return false;
}

auto exactU32(const Value* value, std::uint32_t& out) -> bool {
    std::uint64_t wide{};
    if (!exactU64(value, wide) || wide > std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }
    out = static_cast<std::uint32_t>(wide);
    return true;
}

//  Author milliseconds are finite binary64 JSON numbers. Exact integer tokens are also
//  accepted here because every integer converts exactly to this value domain.
auto asFiniteDouble(const Value* value, double& out) -> bool {
    if (value == nullptr) {
        return false;
    }
    if (const auto* number = value->number()) {
        if (!std::isfinite(*number)) {
            return false;
        }
        out = *number;
        return true;
    }
    if (const auto* signedValue = value->signedInteger()) {
        out = static_cast<double>(*signedValue);
        return std::isfinite(out);
    }
    if (const auto* unsignedValue = value->unsignedInteger()) {
        out = static_cast<double>(*unsignedValue);
        return std::isfinite(out);
    }
    return false;
}

auto asBool(const Value* value, bool& out) -> bool {
    if (value == nullptr || value->boolean() == nullptr) {
        return false;
    }
    out = *value->boolean();
    return true;
}

auto readPhaseKind(const Value* value, BeatPhaseKind& out) -> bool {
    std::string text;
    if (!asString(value, text)) {
        return false;
    }
    if (text == "tap") {
        out = BeatPhaseKind::tap;
    } else if (text == "head") {
        out = BeatPhaseKind::head;
    } else if (text == "body") {
        out = BeatPhaseKind::body;
    } else if (text == "tail") {
        out = BeatPhaseKind::tail;
    } else {
        return false;
    }
    return true;
}

auto readBeat(const Value* value, BeatSource& out) -> bool {
    const auto* object = asObject(value);
    if (object == nullptr) {
        return false;
    }
    std::int64_t denominator{};
    if (!exactI64(member(*value, "numerator"), out.numerator) ||
        !exactI64(member(*value, "denominator"), denominator) || denominator <= 0) {
        return false;
    }
    out.denominator = denominator;
    return true;
}

auto toRationalBeat(const BeatSource& beat) -> core::Result<RationalBeat> {
    return RationalBeat::create(beat.numerator, beat.denominator);
}

auto readRationalBeat(const Value* value, RationalBeat& out) -> bool {
    const auto* object = asObject(value);
    if (object == nullptr) {
        return false;
    }
    std::int64_t numerator{}, denominator{};
    if (!exactI64(member(*value, "numerator"), numerator) ||
        !exactI64(member(*value, "denominator"), denominator) || denominator <= 0) {
        return false;
    }
    auto created = RationalBeat::create(numerator, denominator);
    if (!created) {
        return false;
    }
    out = *created;
    return true;
}

//  ------------------------------------------------------------------------------------------
//  Framings
//  ------------------------------------------------------------------------------------------

void readFramings(const Value& root, ChartV5SourceDocument& document,
                  core::Diagnostics& diagnostics) {
    const auto* items = asArray(member(root, "framings"));
    if (items == nullptr) {
        return;
    }
    std::set<std::pair<std::uint32_t, std::uint32_t>> aspects;
    for (std::size_t index = 0; index < items->size(); ++index) {
        const auto& item = (*items)[index];
        const auto itemPath = json::appendIndexPath("$/framings", index);
        std::uint32_t width{}, height{};
        if (!exactU32(member(item, "width"), width) || width == 0 || width > 2147483647U ||
            !exactU32(member(item, "height"), height) || height == 0 || height > 2147483647U) {
            addError(diagnostics, "chart.framing.invalid",
                     "Framing width and height must be exact integers in [1, 2147483647]",
                     itemPath);
            continue;
        }
        ChartFramingSource framing{width, height, std::nullopt};
        double verticalScale = 1.0;
        if (const auto* scale = member(item, "verticalScale"); scale != nullptr) {
            if (!asFiniteDouble(scale, verticalScale) || verticalScale < 0.125 ||
                verticalScale > 8.0) {
                addError(diagnostics, "chart.framing.invalid",
                         "Framing verticalScale must be in [0.125, 8]",
                         itemPath + "/verticalScale");
                continue;
            }
            framing.verticalScale = verticalScale;
        }
        const auto divisor = std::gcd(width, height);
        const auto reducedWidth = width / divisor;
        const auto reducedHeight = height / divisor;
        if (!aspects.emplace(reducedWidth, reducedHeight).second) {
            addError(diagnostics, "chart.framing.duplicate_aspect",
                     "Framing aspect ratio is duplicated after reduction", itemPath);
        }
        document.framings.push_back(framing);
        document.normalizedFramings.push_back(
            NormalizedFramingSource{reducedWidth, reducedHeight, verticalScale});
    }
}

//  ------------------------------------------------------------------------------------------
//  Chart-level timing (v4 shape unchanged)
//  ------------------------------------------------------------------------------------------

void readChartTiming(const Value& root, ChartV5SourceDocument& document,
                     core::Diagnostics& diagnostics) {
    const auto* timing = member(root, "timing");
    const auto* object = asObject(timing);
    if (object == nullptr) {
        addShapeError(diagnostics, "Chart v5 requires a timing object", "$/timing");
        return;
    }
    if (!asFiniteDouble(member(*timing, "offsetMs"), document.timing.offsetMs)) {
        addShapeError(diagnostics, "timing.offsetMs must be a finite number", "$/timing/offsetMs");
    }
    if (!asFiniteDouble(member(*timing, "defaultBpm"), document.timing.defaultBpm)) {
        addShapeError(diagnostics, "timing.defaultBpm must be a finite number",
                      "$/timing/defaultBpm");
    }
    if (const auto* tempoEvents = asArray(member(*timing, "tempoEvents")); tempoEvents != nullptr) {
        for (std::size_t index = 0; index < tempoEvents->size(); ++index) {
            const auto& item = (*tempoEvents)[index];
            const auto itemPath = json::appendIndexPath("$/timing/tempoEvents", index);
            ChartTempoEventSource tempo;
            if (!readRationalBeat(member(item, "startBeat"), tempo.startBeat) ||
                !readRationalBeat(member(item, "durationBeats"), tempo.durationBeats) ||
                !asFiniteDouble(member(item, "startBpm"), tempo.startBpm) ||
                !asFiniteDouble(member(item, "endBpm"), tempo.endBpm) ||
                !asFiniteDouble(member(item, "startSlope"), tempo.startSlope) ||
                !asFiniteDouble(member(item, "endSlope"), tempo.endSlope)) {
                addShapeError(diagnostics, "Tempo event source shape is invalid", itemPath);
                continue;
            }
            document.timing.tempoEvents.push_back(tempo);
        }
    }
    if (const auto* stops = asArray(member(*timing, "stops")); stops != nullptr) {
        for (std::size_t index = 0; index < stops->size(); ++index) {
            const auto& item = (*stops)[index];
            const auto itemPath = json::appendIndexPath("$/timing/stops", index);
            if (member(item, "endBeat") != nullptr) {
                addRelationError(diagnostics,
                                 "A v3 Stop accepts only beat and durationMs, not endBeat",
                                 itemPath + "/endBeat");
                continue;
            }
            ChartStopSource stop;
            if (!readRationalBeat(member(item, "beat"), stop.beat) ||
                !asFiniteDouble(member(item, "durationMs"), stop.durationMs) ||
                !(stop.durationMs > 0.0)) {
                addShapeError(diagnostics,
                              "A v3 Stop requires a rational beat and a finite positive durationMs",
                              itemPath);
                continue;
            }
            document.timing.stops.push_back(stop);
        }
    }
}

//  ------------------------------------------------------------------------------------------
//  Gameplay v3 author payload
//  ------------------------------------------------------------------------------------------

void readGradeTables(const Value* common, GameplayCommonSource& source,
                     core::Diagnostics& diagnostics) {
    const auto* tables = asArray(member(*common, "gradeTables"));
    if (tables == nullptr) {
        return;
    }
    std::set<std::string, std::less<>> ids;
    for (std::size_t index = 0; index < tables->size(); ++index) {
        const auto& item = (*tables)[index];
        const auto itemPath = json::appendIndexPath("$/gameplay/common/gradeTables", index);
        GradeTableSource table;
        if (!asString(member(item, "id"), table.id)) {
            addShapeError(diagnostics, "Grade table requires a string id", itemPath + "/id");
            continue;
        }
        if (!ids.emplace(table.id).second) {
            addRelationError(diagnostics, "Grade table id is duplicated", itemPath + "/id");
        }
        const auto* exact = member(item, "exact");
        if (exact == nullptr || !asFiniteDouble(member(*exact, "startMs"), table.exactStartMs) ||
            !asFiniteDouble(member(*exact, "endMs"), table.exactEndMs)) {
            addShapeError(diagnostics, "Grade table requires finite exact endpoints",
                          itemPath + "/exact");
            continue;
        }
        if (!(table.exactStartMs <= 0.0) || !(table.exactEndMs > 0.0) ||
            !(table.exactStartMs < table.exactEndMs)) {
            addRelationError(diagnostics,
                             "The exact band must satisfy startMs <= 0 < endMs and be non-empty",
                             itemPath + "/exact");
        }
        const auto* intervals = asArray(member(item, "intervals"));
        if (intervals == nullptr || intervals->empty()) {
            addRelationError(diagnostics, "A grade table requires at least one grade interval",
                             itemPath + "/intervals");
            continue;
        }
        bool intervalsValid = true;
        for (std::size_t intervalIndex = 0; intervalIndex < intervals->size(); ++intervalIndex) {
            const auto& interval = (*intervals)[intervalIndex];
            const auto intervalPath = json::appendIndexPath(itemPath + "/intervals", intervalIndex);
            GradeIntervalSource parsed;
            if (!asString(member(interval, "grade"), parsed.grade) ||
                !asFiniteDouble(member(interval, "startMs"), parsed.startMs) ||
                !asFiniteDouble(member(interval, "endMs"), parsed.endMs)) {
                addShapeError(diagnostics, "Grade interval source shape is invalid", intervalPath);
                intervalsValid = false;
                continue;
            }
            if (!(parsed.startMs < parsed.endMs)) {
                addRelationError(diagnostics,
                                 "A grade interval requires startMs < endMs (raw endpoints)",
                                 intervalPath);
                intervalsValid = false;
                continue;
            }
            table.intervals.push_back(std::move(parsed));
        }
        std::ranges::sort(table.intervals, {}, &GradeIntervalSource::startMs);
        for (std::size_t intervalIndex = 1; intervalIndex < table.intervals.size();
             ++intervalIndex) {
            if (table.intervals[intervalIndex - 1].endMs !=
                table.intervals[intervalIndex].startMs) {
                addRelationError(diagnostics,
                                 "Sorted grade intervals must be adjacent (endMs == startMs)",
                                 json::appendIndexPath(itemPath + "/intervals", intervalIndex));
                intervalsValid = false;
            }
        }
        if (intervalsValid) {
            source.gradeTables.push_back(std::move(table));
        }
    }
    std::ranges::sort(source.gradeTables, {}, &GradeTableSource::id);
}

void readNoteTypes(const Value* common, GameplayCommonSource& source,
                   core::Diagnostics& diagnostics) {
    const auto* noteTypes = asArray(member(*common, "noteTypes"));
    if (noteTypes == nullptr) {
        return;
    }
    std::set<std::string, std::less<>> ids;
    std::set<std::string, std::less<>> tableIds;
    for (const auto& table : source.gradeTables) {
        tableIds.emplace(table.id);
    }
    for (std::size_t index = 0; index < noteTypes->size(); ++index) {
        const auto& item = (*noteTypes)[index];
        const auto itemPath = json::appendIndexPath("$/gameplay/common/noteTypes", index);
        NoteTypeSource noteType;
        if (!asString(member(item, "id"), noteType.id)) {
            addShapeError(diagnostics, "NoteType requires a string id", itemPath + "/id");
            continue;
        }
        if (!ids.emplace(noteType.id).second) {
            addRelationError(diagnostics, "NoteType id is duplicated", itemPath + "/id");
        }
        const auto* selections = asArray(member(item, "gradeTables"));
        if (selections == nullptr) {
            addShapeError(diagnostics, "NoteType requires a gradeTables array", itemPath);
            continue;
        }
        std::set<BeatPhaseKind> phases;
        for (std::size_t selectionIndex = 0; selectionIndex < selections->size();
             ++selectionIndex) {
            const auto& selection = (*selections)[selectionIndex];
            const auto selectionPath =
                json::appendIndexPath(itemPath + "/gradeTables", selectionIndex);
            GradeSelectionSource parsed;
            if (!readPhaseKind(member(selection, "phase"), parsed.phase) ||
                !asString(member(selection, "gradeTable"), parsed.gradeTable)) {
                addShapeError(diagnostics, "Grade selection source shape is invalid",
                              selectionPath);
                continue;
            }
            if (!phases.emplace(parsed.phase).second) {
                addRelationError(diagnostics, "A NoteType may select one table per phase",
                                 selectionPath + "/phase");
            }
            if (!tableIds.contains(parsed.gradeTable)) {
                addRelationError(diagnostics, "Grade selection refers to an unknown grade table",
                                 selectionPath + "/gradeTable");
            }
            parsed.fieldPath = selectionPath;
            noteType.gradeTables.push_back(std::move(parsed));
        }
        std::ranges::sort(noteType.gradeTables,
                          [](const GradeSelectionSource& left, const GradeSelectionSource& right) {
                              return phaseOrder(left.phase) < phaseOrder(right.phase);
                          });
        noteType.fieldPath = itemPath;
        source.noteTypes.push_back(std::move(noteType));
    }
    std::ranges::sort(source.noteTypes, {}, &NoteTypeSource::id);
}

auto checkedProduct(std::uint64_t left, std::uint64_t right, std::uint64_t& out) -> bool {
    if (left != 0 && right > std::numeric_limits<std::uint64_t>::max() / left) {
        return false;
    }
    out = left * right;
    return true;
}

auto readScoreMode(const Value* common, GameplayCommonSource& source,
                   core::Diagnostics& diagnostics) {
    const auto* scoreMode = member(*common, "scoreMode");
    if (scoreMode == nullptr) {
        return;
    }
    constexpr std::string_view kScorePath = "$/gameplay/common/scoreMode";
    ScoreModeSource mode;
    if (!asString(member(*scoreMode, "id"), mode.id) ||
        !exactU32(member(*scoreMode, "total"), mode.total) ||
        !exactU32(member(*scoreMode, "mj"), mode.mj) ||
        !exactU32(member(*scoreMode, "mc"), mode.mc)) {
        addShapeError(diagnostics, "ScoreMode source shape is invalid", std::string{kScorePath});
        return;
    }
    std::string onExhausted;
    if (!asString(member(*scoreMode, "onExhausted"), onExhausted) ||
        (onExhausted != "keep" && onExhausted != "reset")) {
        addRelationError(diagnostics, "ScoreMode onExhausted must be keep or reset",
                         std::string{kScorePath} + "/onExhausted");
    }
    mode.resetOnExhausted = onExhausted == "reset";
    const auto* rewards = asArray(member(*scoreMode, "comboRewards"));
    if (rewards != nullptr) {
        for (std::size_t index = 0; index < rewards->size(); ++index) {
            const auto& reward = (*rewards)[index];
            const auto rewardPath =
                json::appendIndexPath(std::string{kScorePath} + "/comboRewards", index);
            ComboRewardSegmentSource segment;
            if (!exactU32(member(reward, "from"), segment.from) ||
                !exactU64(member(reward, "rewardAtFrom"), segment.rewardAtFrom) ||
                !exactU32(member(reward, "slope"), segment.slope)) {
                addShapeError(diagnostics, "Combo reward segment source shape is invalid",
                              rewardPath);
                continue;
            }
            mode.comboRewards.push_back(segment);
        }
    }
    const auto* rules = asArray(member(*scoreMode, "rules"));
    if (rules != nullptr) {
        for (std::size_t index = 0; index < rules->size(); ++index) {
            const auto& rule = (*rules)[index];
            const auto rulePath = json::appendIndexPath(std::string{kScorePath} + "/rules", index);
            ScoreRuleRowSource parsed;
            std::string outcome, combo, grade;
            const auto* gradeValue = member(rule, "grade");
            if (!readPhaseKind(member(rule, "phase"), parsed.phase) ||
                !asString(member(rule, "outcome"), outcome) ||
                (outcome != "hit" && outcome != "miss") ||
                !asBool(member(rule, "qualifies"), parsed.qualifies) ||
                !exactU64(member(rule, "judgePoints"), parsed.judgePoints) ||
                !asString(member(rule, "combo"), combo) || gradeValue == nullptr) {
                addShapeError(diagnostics, "Score rule source shape is invalid", rulePath);
                continue;
            }
            if (gradeValue->string() != nullptr) {
                parsed.grade = *gradeValue->string();
            } else if (gradeValue->object() != nullptr || gradeValue->array() != nullptr ||
                       gradeValue->boolean() != nullptr || gradeValue->number() != nullptr ||
                       gradeValue->signedInteger() != nullptr ||
                       gradeValue->unsignedInteger() != nullptr) {
                addShapeError(diagnostics, "Score rule grade must be an id or null", rulePath);
                continue;
            }
            if (combo == "keep") {
                parsed.combo = ScoreComboAction::keep;
            } else if (combo == "increment") {
                parsed.combo = ScoreComboAction::increment;
            } else if (combo == "reset") {
                parsed.combo = ScoreComboAction::reset;
            } else {
                addRelationError(diagnostics, "Score rule combo must be keep, increment or reset",
                                 rulePath + "/combo");
                continue;
            }
            parsed.hit = outcome == "hit";
            if (!parsed.qualifies &&
                (parsed.judgePoints != 0 || parsed.combo != ScoreComboAction::keep)) {
                addRelationError(diagnostics,
                                 "A non-qualifying score rule must carry zero points and keep",
                                 rulePath);
            }
            parsed.fieldPath = rulePath;
            mode.rules.push_back(std::move(parsed));
        }
    }

    //  Reward segments: first `from` is 1, `from` strictly increases, and the junction
    //  between two segments never decreases. The last-point check uses the same checked
    //  integer arithmetic the runtime score helper uses.
    std::optional<std::uint64_t> previousLast;
    std::optional<std::uint32_t> previousFrom;
    for (std::size_t index = 0; index < mode.comboRewards.size(); ++index) {
        const auto& segment = mode.comboRewards[index];
        const auto rewardPath =
            json::appendIndexPath(std::string{kScorePath} + "/comboRewards", index);
        if (index == 0) {
            if (segment.from != 1) {
                addRelationError(diagnostics, "The first combo reward segment must start at 1",
                                 rewardPath + "/from");
            }
        } else if (previousFrom.has_value() && segment.from <= *previousFrom) {
            addRelationError(diagnostics, "Combo reward segment starts must be strictly increasing",
                             rewardPath + "/from");
        }
        if (previousLast.has_value() && segment.rewardAtFrom < *previousLast) {
            addRelationError(diagnostics, "Combo reward segments must not decrease at the boundary",
                             rewardPath + "/rewardAtFrom");
        }
        if (index + 1 < mode.comboRewards.size()) {
            const auto& next = mode.comboRewards[index + 1];
            std::uint64_t growth{};
            std::uint64_t lastPoint{};
            if (next.from <= segment.from ||
                !checkedProduct(static_cast<std::uint64_t>(next.from - 1 - segment.from),
                                segment.slope, growth) ||
                segment.rewardAtFrom > std::numeric_limits<std::uint64_t>::max() - growth) {
                addRelationError(diagnostics,
                                 "Combo reward segment arithmetic is not representable",
                                 rewardPath);
                previousLast.reset();
            } else {
                lastPoint = segment.rewardAtFrom + growth;
                previousLast = lastPoint;
            }
        } else {
            previousLast.reset();
        }
        previousFrom = segment.from;
    }
    if (mode.comboRewards.empty()) {
        addRelationError(diagnostics, "ScoreMode requires at least one combo reward segment",
                         std::string{kScorePath} + "/comboRewards");
    }

    //  Rule keys are unique and normalized into contract order.
    for (std::size_t left = 0; left < mode.rules.size(); ++left) {
        for (std::size_t right = left + 1; right < mode.rules.size(); ++right) {
            if (mode.rules[left].phase == mode.rules[right].phase &&
                mode.rules[left].hit == mode.rules[right].hit &&
                mode.rules[left].grade == mode.rules[right].grade) {
                addRelationError(diagnostics,
                                 "Two score rules carry the same (phase, outcome, grade) key",
                                 mode.rules[right].fieldPath);
            }
        }
    }
    std::ranges::sort(mode.rules,
                      [](const ScoreRuleRowSource& left, const ScoreRuleRowSource& right) {
                          if (left.phase != right.phase) {
                              return phaseOrder(left.phase) < phaseOrder(right.phase);
                          }
                          if (left.hit != right.hit) {
                              return left.hit;
                          }
                          return left.grade < right.grade;
                      });
    mode.fieldPath = std::string{kScorePath};
    source.scoreMode = std::move(mode);
}

void readRequirementTiming(const Value& requirement, BeatRequirementSource& out,
                           core::Diagnostics& diagnostics) {
    const auto* timing = member(requirement, "timing");
    const auto timingPath = json::appendFieldPath(out.fieldPath, "timing");
    if (asObject(timing) == nullptr) {
        addShapeError(diagnostics, "Requirement timing requires an object", timingPath);
        return;
    }
    if (!readBeat(member(*timing, "endBeat"), out.timing.endBeat)) {
        addShapeError(diagnostics, "Requirement endBeat is invalid", timingPath + "/endBeat");
    } else {
        auto rational = toRationalBeat(out.timing.endBeat);
        if (!rational || rational->numerator() <= 0) {
            addRelationError(diagnostics, "Requirement endBeat must be representable and positive",
                             timingPath + "/endBeat");
        }
    }
    if (const auto* targets = asArray(member(*timing, "phaseTargets")); targets != nullptr) {
        std::set<BeatPhaseKind> kinds;
        for (std::size_t index = 0; index < targets->size(); ++index) {
            const auto& target = (*targets)[index];
            const auto targetPath = json::appendIndexPath(timingPath + "/phaseTargets", index);
            BeatPhaseTargetSource parsed;
            if (!readPhaseKind(member(target, "phase"), parsed.phase) ||
                !readBeat(member(target, "beat"), parsed.beat)) {
                addShapeError(diagnostics, "Phase target source shape is invalid", targetPath);
                continue;
            }
            if (!kinds.emplace(parsed.phase).second) {
                addRelationError(diagnostics, "A requirement may declare one target per phase",
                                 targetPath + "/phase");
            }
            parsed.fieldPath = targetPath;
            out.timing.phaseTargets.push_back(std::move(parsed));
        }
    }
    if (const auto* windows = asArray(member(*timing, "successWindows")); windows != nullptr) {
        for (std::size_t index = 0; index < windows->size(); ++index) {
            const auto& window = (*windows)[index];
            const auto windowPath = json::appendIndexPath(timingPath + "/successWindows", index);
            BeatWindowSource parsed;
            const auto* phase = member(window, "phase");
            if (phase == nullptr || !readPhaseKind(member(*phase, "kind"), parsed.phase.kind) ||
                !exactU32(member(*phase, "declarationOrdinal"), parsed.phase.declarationOrdinal)) {
                addShapeError(diagnostics, "Success window phase shape is invalid", windowPath);
                continue;
            }
            if (!asFiniteDouble(member(window, "startMs"), parsed.startMs) ||
                !asFiniteDouble(member(window, "endMs"), parsed.endMs) ||
                !(parsed.startMs < parsed.endMs)) {
                addRelationError(diagnostics, "A success window requires finite startMs < endMs",
                                 windowPath);
                continue;
            }
            parsed.fieldPath = windowPath;
            out.timing.successWindows.push_back(std::move(parsed));
        }
    }
    if (const auto* body = member(*timing, "body"); body != nullptr) {
        BeatIntervalSource interval;
        if (!readBeat(member(*body, "startBeat"), interval.startBeat) ||
            !readBeat(member(*body, "endBeat"), interval.endBeat)) {
            addShapeError(diagnostics, "Body interval source shape is invalid",
                          timingPath + "/body");
        } else {
            auto start = toRationalBeat(interval.startBeat);
            auto end = toRationalBeat(interval.endBeat);
            if (!start || !end || !(*start < *end)) {
                addRelationError(diagnostics, "Body interval requires startBeat < endBeat",
                                 timingPath + "/body");
            }
            out.timing.body = interval;
        }
    }
    out.timing.fieldPath = timingPath;
}

void readRequirementSelections(const Value& requirement, BeatRequirementSource& out,
                               const GameplayCommonSource& common, core::Diagnostics& diagnostics) {
    std::set<std::string, std::less<>> tableIds;
    std::set<std::string, std::less<>> noteTypeIds;
    for (const auto& table : common.gradeTables) {
        tableIds.emplace(table.id);
    }
    for (const auto& noteType : common.noteTypes) {
        noteTypeIds.emplace(noteType.id);
    }
    if (const auto* noteType = member(requirement, "noteType"); noteType != nullptr) {
        std::string id;
        if (!asString(noteType, id)) {
            addShapeError(diagnostics, "Requirement noteType must be a string",
                          out.fieldPath + "/noteType");
        } else {
            if (!noteTypeIds.contains(id)) {
                addRelationError(diagnostics, "Requirement refers to an unknown NoteType",
                                 out.fieldPath + "/noteType");
            }
            out.noteType = std::move(id);
        }
    }
    if (const auto* selections = asArray(member(requirement, "gradeTables"));
        selections != nullptr) {
        std::set<BeatPhaseKind> phases;
        for (std::size_t index = 0; index < selections->size(); ++index) {
            const auto& selection = (*selections)[index];
            const auto selectionPath = json::appendIndexPath(out.fieldPath + "/gradeTables", index);
            GradeSelectionSource parsed;
            if (!readPhaseKind(member(selection, "phase"), parsed.phase) ||
                !asString(member(selection, "gradeTable"), parsed.gradeTable)) {
                addShapeError(diagnostics, "Grade selection source shape is invalid",
                              selectionPath);
                continue;
            }
            if (!phases.emplace(parsed.phase).second) {
                addRelationError(diagnostics, "A requirement may select one table per phase",
                                 selectionPath + "/phase");
            }
            if (!tableIds.contains(parsed.gradeTable)) {
                addRelationError(diagnostics, "Grade selection refers to an unknown grade table",
                                 selectionPath + "/gradeTable");
            }
            parsed.fieldPath = selectionPath;
            out.gradeTables.push_back(std::move(parsed));
        }
    }
    if (const auto* selections = asArray(member(requirement, "scoring")); selections != nullptr) {
        std::set<BeatPhaseKind> phases;
        for (std::size_t index = 0; index < selections->size(); ++index) {
            const auto& selection = (*selections)[index];
            const auto selectionPath = json::appendIndexPath(out.fieldPath + "/scoring", index);
            ScoreSelectionSource parsed;
            if (!readPhaseKind(member(selection, "phase"), parsed.phase)) {
                addShapeError(diagnostics, "Score selection source shape is invalid",
                              selectionPath);
                continue;
            }
            if (!phases.emplace(parsed.phase).second) {
                addRelationError(diagnostics,
                                 "A requirement may declare one score selection per phase",
                                 selectionPath + "/phase");
            }
            if (const auto* unit = member(selection, "unit"); unit != nullptr) {
                std::string id;
                if (!asString(unit, id)) {
                    addShapeError(diagnostics, "Score selection unit must be a string",
                                  selectionPath + "/unit");
                } else {
                    parsed.unit = std::move(id);
                }
            } else {
                bool disabled = false;
                if (!asBool(member(selection, "disabled"), disabled) || !disabled) {
                    addRelationError(diagnostics,
                                     "A score selection requires a unit or disabled:true",
                                     selectionPath);
                }
            }
            parsed.fieldPath = selectionPath;
            out.scoring.push_back(std::move(parsed));
        }
    }
    std::ranges::sort(out.gradeTables,
                      [](const GradeSelectionSource& left, const GradeSelectionSource& right) {
                          return phaseOrder(left.phase) < phaseOrder(right.phase);
                      });
    std::ranges::sort(out.scoring,
                      [](const ScoreSelectionSource& left, const ScoreSelectionSource& right) {
                          return phaseOrder(left.phase) < phaseOrder(right.phase);
                      });
}

void readGameplay(const Value& root, ChartV5SourceDocument& document,
                  core::Diagnostics& diagnostics) {
    const auto& gameplay = *member(root, "gameplay");
    const auto gameplayPath = std::string{"$/gameplay"};
    auto& source = document.gameplay;
    static_cast<void>(asString(member(gameplay, "sourceDocumentId"), source.sourceDocumentId));
    const auto* common = member(gameplay, "common");
    if (asObject(common) == nullptr) {
        addShapeError(diagnostics, "Gameplay common requires an object", gameplayPath + "/common");
        return;
    }
    static_cast<void>(asString(member(*common, "chartEntryId"), source.common.chartEntryId));
    readGradeTables(common, source.common, diagnostics);
    readNoteTypes(common, source.common, diagnostics);
    readScoreMode(common, source.common, diagnostics);

    const auto* requirements = asArray(member(gameplay, "requirements"));
    if (requirements == nullptr) {
        addShapeError(diagnostics, "Gameplay requirements requires an array",
                      gameplayPath + "/requirements");
        return;
    }
    for (std::size_t index = 0; index < requirements->size(); ++index) {
        const auto& requirement = (*requirements)[index];
        BeatRequirementSource parsed;
        parsed.fieldPath = json::appendIndexPath(gameplayPath + "/requirements", index);
        const auto* stableId = member(requirement, "stableId");
        if (const auto* identity = member(requirement, "identity"); identity != nullptr) {
            const auto* object = asObject(identity);
            if (object != nullptr) {
                static_cast<void>(
                    asString(member(*identity, "chartEntryId"), parsed.identity.chartEntryId));
                static_cast<void>(
                    asString(member(*identity, "invocationId"), parsed.identity.invocationId));
                static_cast<void>(
                    asString(member(*identity, "moduleId"), parsed.identity.moduleId));
                static_cast<void>(
                    asString(member(*identity, "exportId"), parsed.identity.exportId));
                static_cast<void>(asString(member(*identity, "requirementLocalId"),
                                           parsed.identity.requirementLocalId));
            }
        }
        if (stableId != nullptr) {
            if (!asString(member(*stableId, "sourceDocumentId"),
                          parsed.identity.stableIdSourceDocumentId) ||
                !exactU32(member(*stableId, "declarationOrdinal"),
                          parsed.identity.stableIdDeclarationOrdinal)) {
                addShapeError(diagnostics, "Requirement stableId source shape is invalid",
                              parsed.fieldPath + "/stableId");
            }
        }
        if (const auto* phases = asArray(member(requirement, "phases")); phases != nullptr) {
            std::set<std::pair<int, std::uint32_t>> keys;
            for (std::size_t phaseIndex = 0; phaseIndex < phases->size(); ++phaseIndex) {
                const auto& phase = (*phases)[phaseIndex];
                const auto phasePath =
                    json::appendIndexPath(parsed.fieldPath + "/phases", phaseIndex);
                BeatPhaseSource value;
                if (!readPhaseKind(member(phase, "kind"), value.kind) ||
                    !exactU32(member(phase, "declarationOrdinal"), value.declarationOrdinal)) {
                    addShapeError(diagnostics, "Declared phase source shape is invalid", phasePath);
                    continue;
                }
                if (!keys.emplace(phaseOrder(value.kind), value.declarationOrdinal).second) {
                    addRelationError(diagnostics, "Declared phase key is duplicated", phasePath);
                }
                parsed.phases.push_back(value);
            }
        }
        const auto* measure = member(requirement, "measure");
        if (const auto* components =
                measure != nullptr ? asArray(member(*measure, "components")) : nullptr;
            components != nullptr) {
            for (std::size_t componentIndex = 0; componentIndex < components->size();
                 ++componentIndex) {
                const auto& component = (*components)[componentIndex];
                BeatMeasureComponentSource value;
                if (!readPhaseKind(member(component, "phase"), value.phase) ||
                    !asString(member(component, "categoryToken"), value.categoryToken)) {
                    addShapeError(diagnostics, "Measure component source shape is invalid",
                                  json::appendIndexPath(parsed.fieldPath + "/measure/components",
                                                        componentIndex));
                    continue;
                }
                parsed.measureComponents.push_back(std::move(value));
            }
        }
        readRequirementTiming(requirement, parsed, diagnostics);
        readRequirementSelections(requirement, parsed, source.common, diagnostics);

        //  Every phase target names a declared phase kind. A success window keeps both
        //  fields of its declared phase without re-keying by kind; whether that (kind,
        //  ordinal) pair is a legal relation is T-12's compile-stage check, so this source
        //  parser must preserve an author ordinal instead of rejecting or rebuilding it.
        std::set<int> declaredKinds;
        for (const auto& phase : parsed.phases) {
            declaredKinds.emplace(phaseOrder(phase.kind));
        }
        std::set<int> targetKinds;
        for (const auto& target : parsed.timing.phaseTargets) {
            targetKinds.emplace(phaseOrder(target.phase));
            if (!declaredKinds.contains(phaseOrder(target.phase))) {
                addRelationError(diagnostics, "A phase target refers to an undeclared phase kind",
                                 target.fieldPath + "/phase");
            }
        }
        for (const auto& phase : parsed.phases) {
            if (!targetKinds.contains(phaseOrder(phase.kind))) {
                addRelationError(diagnostics,
                                 "A declared phase kind requires exactly one phase target",
                                 parsed.fieldPath + "/timing/phaseTargets");
            }
        }
        source.requirements.push_back(std::move(parsed));
    }
}

//  ------------------------------------------------------------------------------------------
//  Local presentation shapes (Clip v2 / Behavior Event v2 / render.alpha / mask / bindings)
//  ------------------------------------------------------------------------------------------

auto alphaEndpointValid(const Value* value) -> bool {
    std::int64_t endpoint{};
    return exactI64(value, endpoint) && endpoint >= 0 && endpoint <= 255;
}

constexpr std::array kMaskableProperties{
    std::string_view{"transform.position.x"}, std::string_view{"transform.position.y"},
    std::string_view{"transform.position.z"}, std::string_view{"transform.rotation"},
    std::string_view{"transform.scale"},      std::string_view{"render.alpha"},
    std::string_view{"render.visible"},       std::string_view{"render.material"},
    std::string_view{"camera.fovY"}};

auto maskTokenValid(std::string_view token) -> bool {
    return std::ranges::find(kMaskableProperties, token) != kMaskableProperties.end();
}

auto maskPrefixValid(std::string_view prefix) -> bool {
    if (prefix.empty() || prefix.back() != '.') {
        return false;
    }
    return std::ranges::any_of(kMaskableProperties, [&](std::string_view property) {
        return property.starts_with(prefix);
    });
}

void readMask(const Value& mask, std::vector<std::string>& properties,
              std::vector<std::string>& prefixes, core::Diagnostics& diagnostics,
              const std::string& path) {
    if (const auto* items = asArray(member(mask, "properties")); items != nullptr) {
        for (std::size_t index = 0; index < items->size(); ++index) {
            std::string token;
            if (!asString(&(*items)[index], token)) {
                addShapeError(diagnostics, "Mask property must be a string",
                              json::appendIndexPath(path + "/properties", index));
                continue;
            }
            if (!maskTokenValid(token)) {
                addRelationError(diagnostics,
                                 "Mask property is not part of the v5 author property whitelist",
                                 json::appendIndexPath(path + "/properties", index));
                continue;
            }
            properties.push_back(std::move(token));
        }
    }
    if (const auto* items = asArray(member(mask, "prefixes")); items != nullptr) {
        for (std::size_t index = 0; index < items->size(); ++index) {
            std::string token;
            if (!asString(&(*items)[index], token)) {
                addShapeError(diagnostics, "Mask prefix must be a string",
                              json::appendIndexPath(path + "/prefixes", index));
                continue;
            }
            if (!maskPrefixValid(token)) {
                addRelationError(diagnostics, "Mask prefix does not match a v5 author property",
                                 json::appendIndexPath(path + "/prefixes", index));
                continue;
            }
            prefixes.push_back(std::move(token));
        }
    }
    std::ranges::sort(properties);
    std::ranges::sort(prefixes);
}

void readAnimator(const Value& component, const std::string& ownerId,
                  std::set<std::string, std::less<>>& bindingIds,
                  std::set<std::string, std::less<>>& layerIds,
                  std::set<std::string, std::less<>>& importIds,
                  std::set<std::string, std::less<>>& clipIds, ChartV5SourceDocument& document,
                  core::Diagnostics& diagnostics, const std::string& path) {
    const auto* version = member(component, "version");
    std::int64_t versionValue{};
    if (version == nullptr || !exactI64(version, versionValue) || versionValue != 1) {
        addRelationError(diagnostics,
                         "The Animator component keeps component version 1 on the v5 path", path);
        return;
    }
    if (const auto* bindings = asArray(member(component, "templateBindings"));
        bindings != nullptr) {
        for (std::size_t index = 0; index < bindings->size(); ++index) {
            const auto& binding = (*bindings)[index];
            const auto bindingPath = json::appendIndexPath(path + "/templateBindings", index);
            std::string bindingId, templateId;
            if (!asString(member(binding, "bindingId"), bindingId)) {
                addShapeError(diagnostics, "Template binding id is missing", bindingPath);
                continue;
            }
            if (!bindingIds.emplace(bindingId).second) {
                addRelationError(diagnostics, "Template binding id is duplicated",
                                 bindingPath + "/bindingId");
            }
            const auto* reference = member(binding, "template");
            std::string domain;
            if (asObject(reference) == nullptr || !asString(member(*reference, "domain"), domain) ||
                domain != "animation-template" || !asString(member(*reference, "id"), templateId)) {
                addShapeError(diagnostics, "Template binding reference shape is invalid",
                              bindingPath + "/template");
                continue;
            }
            if (!importIds.contains(templateId)) {
                addRelationError(diagnostics,
                                 "Template binding refers to an unknown animation template import",
                                 bindingPath + "/template");
            }
            document.animatorBindings.push_back(
                AnimatorBindingSummary{ownerId, bindingId, templateId});
        }
    }
    if (const auto* layers = asArray(member(component, "layers")); layers != nullptr) {
        for (std::size_t index = 0; index < layers->size(); ++index) {
            const auto& layer = (*layers)[index];
            const auto layerPath = json::appendIndexPath(path + "/layers", index);
            std::string layerId;
            if (!asString(member(layer, "layerId"), layerId)) {
                addShapeError(diagnostics, "Animation layer id is missing", layerPath);
                continue;
            }
            if (!layerIds.emplace(layerId).second) {
                addRelationError(diagnostics, "Animation layer id is duplicated",
                                 layerPath + "/layerId");
            }
            AnimatorMaskSummary mask{ownerId, {}, {}};
            if (const auto* maskValue = member(layer, "propertyMask"); maskValue != nullptr) {
                readMask(*maskValue, mask.properties, mask.prefixes, diagnostics,
                         layerPath + "/propertyMask");
                document.animatorMasks.push_back(std::move(mask));
            }
            if (const auto* groups = asArray(member(layer, "blendGroups")); groups != nullptr) {
                for (std::size_t groupIndex = 0; groupIndex < groups->size(); ++groupIndex) {
                    const auto& group = (*groups)[groupIndex];
                    const auto groupPath =
                        json::appendIndexPath(layerPath + "/blendGroups", groupIndex);
                    const auto* instances = asArray(member(group, "instances"));
                    if (instances == nullptr) {
                        continue;
                    }
                    for (std::size_t instanceIndex = 0; instanceIndex < instances->size();
                         ++instanceIndex) {
                        const auto& instance = (*instances)[instanceIndex];
                        const auto instancePath =
                            json::appendIndexPath(groupPath + "/instances", instanceIndex);
                        const auto* reference = member(instance, "clip");
                        std::string domain, clipId;
                        if (asObject(reference) != nullptr &&
                            asString(member(*reference, "domain"), domain) &&
                            domain == "animation" && asString(member(*reference, "id"), clipId)) {
                            if (!clipIds.contains(clipId)) {
                                addRelationError(
                                    diagnostics,
                                    "Clip instance refers to an unknown local AnimationClip",
                                    instancePath + "/clip");
                            }
                        }
                        if (const auto* maskValue = member(instance, "propertyMask");
                            maskValue != nullptr) {
                            AnimatorMaskSummary instanceMask{ownerId, {}, {}};
                            readMask(*maskValue, instanceMask.properties, instanceMask.prefixes,
                                     diagnostics, instancePath + "/propertyMask");
                            document.animatorMasks.push_back(std::move(instanceMask));
                        }
                    }
                }
            }
        }
    }
}

void readPresentation(const Value& root, ChartV5SourceDocument& document,
                      core::Diagnostics& diagnostics) {
    std::set<std::string, std::less<>> importIds;
    if (const auto* imports = asArray(member(root, "animationTemplateImports"));
        imports != nullptr) {
        for (std::size_t index = 0; index < imports->size(); ++index) {
            const auto& item = (*imports)[index];
            const auto itemPath = json::appendIndexPath("$/animationTemplateImports", index);
            std::string id;
            if (!asString(member(item, "id"), id)) {
                addShapeError(diagnostics, "Animation template import requires an id", itemPath);
                continue;
            }
            if (!importIds.emplace(id).second) {
                addRelationError(diagnostics, "Animation template import id is duplicated",
                                 itemPath + "/id");
                continue;
            }
            document.animationTemplateImports.push_back(id);
        }
        std::ranges::sort(document.animationTemplateImports);
    }

    std::set<std::string, std::less<>> clipIds;
    if (const auto* clips = asArray(member(root, "animationClips")); clips != nullptr) {
        for (std::size_t index = 0; index < clips->size(); ++index) {
            const auto& clip = (*clips)[index];
            const auto clipPath = json::appendIndexPath("$/animationClips", index);
            std::string id;
            std::int64_t version{};
            if (!asString(member(clip, "id"), id) || !exactI64(member(clip, "version"), version)) {
                addShapeError(diagnostics, "Animation clip requires an id and version", clipPath);
                continue;
            }
            if (version != 2) {
                addRelationError(diagnostics,
                                 "Chart v5 local AnimationClips require clip version 2", clipPath);
                continue;
            }
            if (!clipIds.emplace(id).second) {
                addRelationError(diagnostics, "Animation clip id is duplicated", clipPath + "/id");
            }
            document.animationClips.push_back(AnimationClipSummary{id, 2});
            if (const auto* tracks = asArray(member(clip, "tracks")); tracks != nullptr) {
                for (std::size_t trackIndex = 0; trackIndex < tracks->size(); ++trackIndex) {
                    const auto& track = (*tracks)[trackIndex];
                    const auto trackPath = json::appendIndexPath(clipPath + "/tracks", trackIndex);
                    std::string property;
                    if (!asString(member(track, "property"), property)) {
                        addShapeError(diagnostics, "Animation track property is missing",
                                      trackPath);
                        continue;
                    }
                    if (property != "render.alpha") {
                        continue;
                    }
                    const auto* segments = asArray(member(track, "segments"));
                    if (segments == nullptr) {
                        continue;
                    }
                    for (std::size_t segmentIndex = 0; segmentIndex < segments->size();
                         ++segmentIndex) {
                        const auto& segment = (*segments)[segmentIndex];
                        const auto segmentPath =
                            json::appendIndexPath(trackPath + "/segments", segmentIndex);
                        if (!alphaEndpointValid(member(segment, "startValue")) ||
                            !alphaEndpointValid(member(segment, "endValue"))) {
                            addRelationError(
                                diagnostics,
                                "render.alpha endpoints must be exact integers in [0,255]",
                                segmentPath);
                        }
                    }
                }
            }
        }
    }

    if (const auto* behaviors = asArray(member(root, "behaviors")); behaviors != nullptr) {
        std::set<std::string, std::less<>> behaviorIds;
        for (std::size_t index = 0; index < behaviors->size(); ++index) {
            const auto& behavior = (*behaviors)[index];
            const auto behaviorPath = json::appendIndexPath("$/behaviors", index);
            std::string id, type;
            std::int64_t version{};
            if (!asString(member(behavior, "id"), id) ||
                !asString(member(behavior, "type"), type) ||
                !exactI64(member(behavior, "version"), version)) {
                addShapeError(diagnostics, "Behavior source shape is invalid", behaviorPath);
                continue;
            }
            if (type != "behavior.event" || version != 2) {
                addRelationError(diagnostics, "Chart v5 requires behavior.event version 2",
                                 behaviorPath);
                continue;
            }
            if (!behaviorIds.emplace(id).second) {
                addRelationError(diagnostics, "Behavior id is duplicated", behaviorPath + "/id");
            }
            document.behaviors.push_back(BehaviorSummary{id, 2});
            if (const auto* events = asArray(member(behavior, "events")); events != nullptr) {
                for (std::size_t eventIndex = 0; eventIndex < events->size(); ++eventIndex) {
                    const auto& event = (*events)[eventIndex];
                    const auto eventPath =
                        json::appendIndexPath(behaviorPath + "/events", eventIndex);
                    std::string property;
                    if (!asString(member(event, "property"), property) ||
                        property != "render.alpha") {
                        continue;
                    }
                    if (!alphaEndpointValid(member(event, "startValue")) ||
                        !alphaEndpointValid(member(event, "endValue"))) {
                        addRelationError(diagnostics,
                                         "render.alpha event endpoints must be exact integers in "
                                         "[0,255]",
                                         eventPath);
                    }
                }
            }
        }
    }

    const auto readOwners = [&](std::string_view field, bool templateOwner) {
        const auto* owners = asArray(member(root, field));
        if (owners == nullptr) {
            return;
        }
        for (std::size_t index = 0; index < owners->size(); ++index) {
            const auto& owner = (*owners)[index];
            const auto ownerPath = json::appendIndexPath("$/" + std::string{field}, index);
            std::string ownerId;
            if (!asString(member(owner, "id"), ownerId)) {
                continue;
            }
            const Value* components = nullptr;
            std::string componentsPath = ownerPath + "/components";
            if (templateOwner) {
                if (const auto* prototype = member(owner, "prototype"); prototype != nullptr) {
                    components = member(*prototype, "components");
                    componentsPath = ownerPath + "/prototype/components";
                }
            } else {
                components = member(owner, "components");
            }
            if (asObject(components) == nullptr) {
                continue;
            }
            if (const auto* animator = member(*components, "cuexis.animator");
                animator != nullptr) {
                std::set<std::string, std::less<>> bindingIds, layerIds;
                readAnimator(*animator, ownerId, bindingIds, layerIds, importIds, clipIds, document,
                             diagnostics, componentsPath + "/cuexis.animator");
            }
            //  The renderable alpha static field must be an exact integer token. The
            //  schema bounds the value; the exact-token rule is a source rule.
            if (const auto* renderable = member(*components, "cuexis.renderable");
                renderable != nullptr) {
                if (const auto* alpha = member(*renderable, "alpha"); alpha != nullptr) {
                    std::int64_t value{};
                    if (!exactI64(alpha, value) || value < 0 || value > 255) {
                        addRelationError(
                            diagnostics,
                            "Static renderable alpha must be an exact integer in [0,255]",
                            componentsPath + "/cuexis.renderable/alpha");
                    }
                }
            }
        }
    };
    readOwners("templates", true);
    readOwners("objects", false);
}

} // namespace

auto ChartV5Loader::isV5(std::string_view jsonText, const ChartLimits& limits) -> bool {
    auto parsed =
        json::parse(jsonText, json::ParseLimits{limits.maxInputBytes, limits.maxNestingDepth,
                                                limits.maxStringBytes});
    if (!parsed) {
        return false;
    }
    const auto* version = parsed->find("version");
    std::int64_t value{};
    return exactI64(version, value) && value == 5;
}

auto ChartV5Loader::load(std::string_view jsonText, const ChartLimits& limits)
    -> ChartV5SourceResult {
    auto diagnostics = detail::makeDiagnostics(limits);
    if (detail::rejectInvalidDiagnosticLimit(diagnostics, limits)) {
        return ChartV5SourceResult{std::nullopt, std::move(diagnostics)};
    }
    auto parsed =
        json::parse(jsonText, json::ParseLimits{limits.maxInputBytes, limits.maxNestingDepth,
                                                limits.maxStringBytes});
    if (!parsed) {
        addParseError(diagnostics, parsed.error());
        diagnostics.sortDeterministically();
        return ChartV5SourceResult{std::nullopt, std::move(diagnostics)};
    }

    std::string format;
    std::int64_t version{};
    if (!asString(parsed->find("format"), format) || format != "cuexis.chart") {
        addError(diagnostics, "chart.format.unsupported", "Chart format is unsupported",
                 "$/format");
    }
    if (!exactI64(parsed->find("version"), version) || version != 5) {
        addError(diagnostics, "chart.version.unsupported",
                 "Chart v5 candidate loader requires version 5", "$/version");
        diagnostics.sortDeterministically();
        return ChartV5SourceResult{std::nullopt, std::move(diagnostics)};
    }

    //  The gameplay version/profile gate runs before schema validation so an unenabled
    //  gameplay version reports its dedicated format code, not a generic schema error.
    const auto* gameplay = parsed->find("gameplay");
    std::int64_t gameplayVersion{};
    std::string authorProfile;
    if (gameplay == nullptr || !exactI64(gameplay->find("version"), gameplayVersion) ||
        gameplayVersion != 3 || !asString(gameplay->find("authorProfile"), authorProfile) ||
        authorProfile != "gameplay.author.beat-us.v1") {
        addError(diagnostics, "format.gameplay_version_unsupported",
                 "Chart v5 requires the gameplay v3 author payload with the beat-us profile",
                 "$/gameplay");
        diagnostics.sortDeterministically();
        return ChartV5SourceResult{std::nullopt, std::move(diagnostics)};
    }

    auto schema =
        json::parse(detail::kChartV5Schema,
                    json::ParseLimits{detail::kChartV5Schema.size(), detail::kChartV5Schema.size(),
                                      detail::kChartV5Schema.size()});
    if (!schema) {
        addError(diagnostics, "chart.v5.schema_unavailable",
                 "The embedded Chart v5 candidate schema could not be parsed", "$");
    } else if (auto validated = json::validateAgainstSchema(*parsed, *schema, diagnostics);
               !validated) {
        addParseError(diagnostics, validated.error());
    }
    if (diagnostics.hasErrors()) {
        diagnostics.sortDeterministically();
        return ChartV5SourceResult{std::nullopt, std::move(diagnostics)};
    }

    ChartV5SourceDocument document;
    if (!asString(parsed->find("chartId"), document.chartId) || !isUuidV7(document.chartId)) {
        addError(diagnostics, "chart.chart_id.invalid", "Chart ID must be a lowercase UUIDv7",
                 "$/chartId");
    }
    readChartTiming(*parsed, document, diagnostics);
    readGameplay(*parsed, document, diagnostics);
    readFramings(*parsed, document, diagnostics);
    readPresentation(*parsed, document, diagnostics);
    if (auto serialized = json::serialize(*parsed)) {
        document.canonicalSource.canonicalText = std::move(*serialized);
    } else {
        addParseError(diagnostics, serialized.error());
    }
    diagnostics.sortDeterministically();
    if (diagnostics.hasErrors()) {
        return ChartV5SourceResult{std::nullopt, std::move(diagnostics)};
    }
    return ChartV5SourceResult{std::move(document), std::move(diagnostics)};
}

} // namespace cuexis::chart

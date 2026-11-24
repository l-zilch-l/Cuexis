#pragma once

//  Chart v5 candidate source document (base shell).
//
//  Build-only typed view of the explicit v5 author path. The v4 loader, the canonical
//  Chart reader and every existing Playback/Player entry keep their current acceptance
//  sets; nothing selects this header or `ChartV5Loader` implicitly.
//
//  T-11 owns the base shell: root format dispatch, the gameplay v3 author payload
//  (including Stop/grade/scoring source values and the success-window phase ordinal),
//  the author framings and the local presentation shapes whose versions v5 changes
//  (Clip v2, Behavior Event v2, integer `render.alpha`, Animator component version 1).
//  CXT document resolution, `presentationBindings` and the animation program stay with
//  T-13; no external CXT document is read here.

#include <cuexis/chart/chart_document.hpp>
#include <cuexis/chart/limits.hpp>
#include <cuexis/chart/rational_beat.hpp>
#include <cuexis/core/diagnostic.hpp>
#include <cuexis/json/value.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cuexis::chart {

//  The four Author phase kinds, in the order the contract lists them.
enum class BeatPhaseKind : std::uint8_t { tap, head, body, tail };

//  A declared phase instance: the kind plus the author declaration ordinal. The pair is
//  kept together so a success window never loses the ordinal it was declared with.
struct BeatPhaseSource final {
    BeatPhaseKind kind{BeatPhaseKind::tap};
    std::uint32_t declarationOrdinal{};
    [[nodiscard]] auto operator==(const BeatPhaseSource&) const -> bool = default;
};

//  A source Beat. The author may leave it unreduced; reduction belongs to the compiler.
struct BeatSource final {
    std::int64_t numerator{};
    std::int64_t denominator{1};
};

//  `successWindows[]`: endpoints are relative milliseconds around the phase target.
struct BeatWindowSource final {
    BeatPhaseSource phase;
    double startMs{};
    double endMs{};
    std::string fieldPath;
};

//  `phaseTargets[]`: the phase kind only, as the contract keeps this array kind-keyed.
struct BeatPhaseTargetSource final {
    BeatPhaseKind phase{BeatPhaseKind::tap};
    BeatSource beat;
    std::string fieldPath;
};

struct BeatIntervalSource final {
    BeatSource startBeat;
    BeatSource endBeat;
};

//  The v3 Timing delta. Stop declarations stay in the Chart-level `timing.stops`, the
//  only timing authority on both old and v3 paths.
struct BeatTimingSource final {
    BeatSource endBeat;
    std::vector<BeatPhaseTargetSource> phaseTargets;
    std::vector<BeatWindowSource> successWindows;
    std::optional<BeatIntervalSource> body;
    std::string fieldPath;
};

struct GradeIntervalSource final {
    std::string grade;
    double startMs{};
    double endMs{};
};

//  One grade band table. Intervals are stored in contract order (ascending start), and
//  the exact band keeps its author endpoints independently of the interval geometry.
struct GradeTableSource final {
    std::string id;
    double exactStartMs{};
    double exactEndMs{};
    std::vector<GradeIntervalSource> intervals;
    std::string fieldPath;
};

//  `{phase, gradeTable}`; one selection per phase inside one owner.
struct GradeSelectionSource final {
    BeatPhaseKind phase{BeatPhaseKind::tap};
    std::string gradeTable;
    std::string fieldPath;
};

//  `{phase, unit}` or `{phase, disabled:true}`. A missing unit means disabled.
struct ScoreSelectionSource final {
    BeatPhaseKind phase{BeatPhaseKind::tap};
    std::optional<std::string> unit;
    std::string fieldPath;
};

struct NoteTypeSource final {
    std::string id;
    std::vector<GradeSelectionSource> gradeTables;
    std::string fieldPath;
};

struct ComboRewardSegmentSource final {
    std::uint32_t from{};
    std::uint64_t rewardAtFrom{};
    std::uint32_t slope{};
};

enum class ScoreComboAction : std::uint8_t { keep, increment, reset };

struct ScoreRuleRowSource final {
    BeatPhaseKind phase{BeatPhaseKind::tap};
    bool hit{};
    std::optional<std::string> grade;
    bool qualifies{};
    std::uint64_t judgePoints{};
    ScoreComboAction combo{ScoreComboAction::keep};
    std::string fieldPath;
};

//  A cross-chart scoring mode. The reward table and every rule key are kept in contract
//  order; validation rejects duplicate keys before any sorting can hide them.
struct ScoreModeSource final {
    std::string id;
    std::uint32_t total{};
    std::uint32_t mj{};
    std::uint32_t mc{};
    bool resetOnExhausted{};
    std::vector<ComboRewardSegmentSource> comboRewards;
    std::vector<ScoreRuleRowSource> rules;
    std::string fieldPath;
};

struct RequirementIdentitySource final {
    std::string chartEntryId, invocationId, moduleId, exportId, requirementLocalId;
    std::string stableIdSourceDocumentId;
    std::uint32_t stableIdDeclarationOrdinal{};
};

struct BeatMeasureComponentSource final {
    BeatPhaseKind phase{BeatPhaseKind::tap};
    std::string categoryToken;
};

//  One v3 Requirement source row. The retained v4-shaped fields are validated by the
//  candidate schema; the typed value carries the fields the timing/gameplay compiler
//  consumes plus every selection declaration.
struct BeatRequirementSource final {
    RequirementIdentitySource identity;
    std::vector<BeatPhaseSource> phases;
    std::vector<BeatMeasureComponentSource> measureComponents;
    BeatTimingSource timing;
    std::optional<std::string> noteType;
    std::vector<GradeSelectionSource> gradeTables;
    std::vector<ScoreSelectionSource> scoring;
    std::string fieldPath;
};

//  The Chart-level `timing` block. The v4 shape is unchanged; the v3 path still reads
//  the one timing authority here. `stops[]` accepts only `beat` and a finite positive
//  `durationMs`; an `endBeat` member is not part of the shape.
struct ChartTempoEventSource final {
    RationalBeat startBeat{RationalBeat::zero()};
    RationalBeat durationBeats{RationalBeat::zero()};
    double startBpm{}, endBpm{}, startSlope{}, endSlope{};
};

struct ChartStopSource final {
    RationalBeat beat{RationalBeat::zero()};
    double durationMs{};
};

struct ChartTimingSource final {
    double offsetMs{};
    double defaultBpm{};
    std::vector<ChartTempoEventSource> tempoEvents;
    std::vector<ChartStopSource> stops;
};

struct GameplayCommonSource final {
    std::string chartEntryId;
    std::vector<GradeTableSource> gradeTables;
    std::vector<NoteTypeSource> noteTypes;
    std::optional<ScoreModeSource> scoreMode;
};

struct GameplayAuthorSource final {
    std::string sourceDocumentId;
    GameplayCommonSource common;
    std::vector<BeatRequirementSource> requirements;
};

//  Author framing: the exact author pair and whether `verticalScale` was written.
struct ChartFramingSource final {
    std::uint32_t width{};
    std::uint32_t height{};
    std::optional<double> verticalScale;
};

//  Prepared framing: the GCD-reduced pair and the default-filled scale. The source keeps
//  the author pair; this projection is what the later prepared/commit step consumes.
struct NormalizedFramingSource final {
    std::uint32_t width{};
    std::uint32_t height{};
    double verticalScale{1.0};
};

struct AnimationClipSummary final {
    std::string id;
    std::uint32_t version{2};
};

struct BehaviorSummary final {
    std::string id;
    std::uint32_t version{2};
};

struct AnimatorBindingSummary final {
    std::string ownerId;
    std::string bindingId;
    std::string templateId;
};

struct AnimatorMaskSummary final {
    std::string ownerId;
    std::vector<std::string> properties;
    std::vector<std::string> prefixes;
};

struct ChartV5SourceDocument final {
    std::string chartId;
    ChartTimingSource timing;
    GameplayAuthorSource gameplay;
    std::vector<ChartFramingSource> framings;
    std::vector<NormalizedFramingSource> normalizedFramings;
    std::vector<std::string> animationTemplateImports;
    std::vector<AnimationClipSummary> animationClips;
    std::vector<BehaviorSummary> behaviors;
    std::vector<AnimatorBindingSummary> animatorBindings;
    std::vector<AnimatorMaskSummary> animatorMasks;
    OpaqueJson canonicalSource;
};

struct ChartV5SourceResult final {
    std::optional<ChartV5SourceDocument> document;
    core::Diagnostics diagnostics;

    [[nodiscard]] auto hasValue() const noexcept -> bool {
        return document.has_value() && !diagnostics.hasErrors();
    }
};

//  Explicit v5 candidate entry. The version alone decides whether this loader is
//  interested; callers are expected to hold the text for the real parse.
class ChartV5Loader final {
  public:
    [[nodiscard]] static auto isV5(std::string_view jsonText, const ChartLimits& limits) -> bool;
    [[nodiscard]] static auto load(std::string_view jsonText, const ChartLimits& limits)
        -> ChartV5SourceResult;
};

} // namespace cuexis::chart

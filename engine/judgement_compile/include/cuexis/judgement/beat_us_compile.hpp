#pragma once

//  Beat-us author compiler.
//
//  Build-only compile-side entry. It consumes the merged v3 author declarations, the exact beat
//  keys the caller already derived, the shared timing map and the compile context, and produces the
//  unterminated owning `BeatGameplayProduct`: runtime records, canonical score, exact beat keys and
//  (later) canonical graph rows. It does not compute an identity, does not build a wire encoding
//  and does not call the old graph publication or hasher.
//
//  The only timing authority is the caller's `TimingMap`. Every integer comparison is made against
//  the saved anchors; no interpolation fallback is used for a requirement boundary.

#include <cuexis/core/result.hpp>
#include <cuexis/judgement/compiled_gameplay_context.hpp>
#include <cuexis/judgement/compiled_gameplay_rows.hpp>
#include <cuexis/judgement/gameplay_assembler.hpp>
#include <cuexis/timing/timing_map.hpp>

#include <optional>
#include <span>
#include <string>
#include <vector>

namespace cuexis::judgement {

//  One authored success window. The phase carries the full declaration (kind and ordinal) so the
//  compiler can validate the exact declared phase before it looks up the kind-keyed target.
struct BeatWindow final {
    PhaseDeclaration phase;
    double startMs{};
    double endMs{};
};

//  One instance-level grade table selection.
struct BeatGradeSelection final {
    PhaseKind phase;
    std::string gradeTable;
};

//  One instance-level scoring-unit selection. An absent unit means "disabled".
struct BeatScoreSelection final {
    PhaseKind phase;
    std::optional<std::string> unit;
};

//  One authored requirement: the exact source beats of its timing plus every selection.
struct BeatRequirementSource final {
    RequirementIdentity identity;
    CompiledBeatKeys beats;
    std::vector<BeatWindow> successWindows;
    std::optional<std::string> noteType;
    std::vector<BeatGradeSelection> gradeTables;
    std::vector<BeatScoreSelection> scoring;
};

struct BeatGradeInterval final {
    std::string grade;
    double startMs{};
    double endMs{};
};

struct BeatGradeTable final {
    std::string id;
    double exactStartMs{};
    double exactEndMs{};
    std::vector<BeatGradeInterval> intervals;
};

struct BeatNoteType final {
    std::string id;
    std::vector<BeatGradeSelection> gradeTables;
};

//  One named Measure definition body. `id` is absent for the inline/anonymous path, which does not
//  take part in the named matching.
struct BeatMeasureDefinition final {
    std::optional<std::string> id;
    MeasureSpecDeclaration declaration;
};

//  The synchronous borrowed input view. Every span must stay valid until the call returns.
struct BeatGameplaySourceView final {
    timing::TimingSourceView timing;
    std::span<const GameplaySource> sources;
    std::span<const PatternDeclaration> patternDefinitions;
    std::span<const BeatMeasureDefinition> measureDefinitions;
    std::span<const BeatRequirementSource> requirements;
    std::span<const BeatGradeTable> gradeTables;
    std::span<const BeatNoteType> noteTypes;
    const ScoreMode& scoreMode;
    const LatePolicyParameters& latePolicy;
    const DeclaredCapabilitySet& declaredCapabilities;
    const FeatureClosure& declaredFeatures;
    const ClosureContributions& closureContributions;
    std::string_view executionProfile;
    std::string_view normalizationProfileToken;
    std::string_view coordinatorPolicyToken;
};

//  The compile-side budgets and capability context.
struct GameplayCompileContext final {
    const CompileCapabilityContext& capabilities;
    const ContentProfileLimits& contentLimits;
    const PatternCompileBudget& patternBudget;
    const runtime::PrepareGameplayLimits& prepareLimits;
};

//  Compiles one chart's gameplay source into the unterminated owning product.
[[nodiscard]] auto compileBeatGameplay(const BeatGameplaySourceView& view,
                                       const timing::TimingMap& map,
                                       const GameplayCompileContext& context)
    -> core::Result<BeatGameplayProduct>;

} // namespace cuexis::judgement

#pragma once

//  Judgement typed kernel - compiled pattern and measure products.

#include <cuexis/core/result.hpp>
#include <cuexis/judgement/gameplay_graph.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace cuexis::judgement {

//  ---------------------------------------------------------------------------------------------
//  Second half of S7A-3: compiled products, prepared grace and resource plan
//  ---------------------------------------------------------------------------------------------
//
//  Plan S7A-3 items 2 and 3 are implemented here: `compilePattern` produces the read-only compiled
//  predicate of one Pattern declaration (all seven Stage 7A primitives, a prepare-time fully
//  expanded `bounded repeat`, the `leftmost-first` match policy, and the termination / state count
//  / expansion count / time / memory budget checks), and `compileMeasure` produces the read-only
//  per-component grading product of one Measure declaration (multiple phase / category components,
//  Hold head / body, the explicitly declared Release / tail, and the optional grade table).
//
//  Three rules shape the compiled products:
//
//    1. Nothing is frozen that the batch is not authorised to freeze. The compiled carriers are
//       immutable handles: no field width, no state number, no transition-table encoding, no
//       accounting constant and no budget number is part of this interface. The counts and the
//       budget parameters are exposed, the representation behind them is not;
//    2. no limit is invented. Every bound is a `MeasuredParameter`: a bound no measurement has
//       accepted stays pending, is reported as such, and is never enforced with a substituted
//       number. In particular there is no compiled-in declaration-depth constant: the depth is one
//       budgeted, measurable dimension like the expansion count and the state count, so an
//       unmeasured depth never refuses a deeply nested but legal pattern;
//    3. a check that is not implemented is refused, not faked. The rest of the second half -- the
//       `preparedGrace` resolution, the single `capacity = 1` exclusive resource claim / ownership
//       resolution, the first Packed write of the REF0 physical fields and the Packed physical
//       encoding -- keeps its stable rejection and fabricates no product.

namespace detail {

//  Storage anchors of the compiled products. The representation of a compiled Pattern, a compiled
//  Measure and a resolved resource claim is an implementation detail of this module, so this
//  boundary promises no member, no width and no encoding.
class CompiledPatternStorage;
class CompiledMeasureStorage;
class ResourceClaimResolutionStorage;

} // namespace detail

//  The compiled budget of one Pattern (plan S7A-3 item 2: "state count, expansion count and time /
//  memory budget"). Every dimension is a measured-or-pending parameter, so the compile never
//  refuses legal content on behalf of a threshold no measurement has accepted, and a measured
//  threshold is enforced exactly.
//
//  Time and memory are budgeted as deterministic work counters rather than as a wall clock or a
//  host allocator reading: a wall-clock threshold would make prepare depend on the machine, and the
//  canonical graph of one content has to be the same everywhere.
struct PatternCompileBudget final {
    //  The declaration depth of the Pattern tree. A budgeted, measurable dimension; there is no
    //  compiled-in depth constant.
    MeasuredParameter<std::uint64_t> maxDeclarationDepth;
    //  The number of primitive instances of the complete prepare-time expansion (Spec 3.8.4
    //  requires the complete expansion, so it is what bounds the compiled representation). A
    //  bounded repeat expands over every admissible copy count, so this number is the sum over
    //  those copy counts and not the largest single branch, which is reported separately. A count
    //  with no representable value is a PROVEN LOWER BOUND of at least 2^64, because the arithmetic
    //  that produces it is exact and checked, so against an accepted limit of this dimension it is
    //  a real overrun and is refused as such; it is never compared against a substituted number,
    //  and without an accepted limit nothing is compared and nothing is refused. A count whose true
    //  value is not a lower bound of the dimension -- a bounded repeat of zero copies, whose count
    //  is exactly zero whatever its operand measures -- never takes that branch: the comparison is
    //  made on the count of the complete expansion, not on an intermediate one.
    MeasuredParameter<std::uint64_t> maxExpansionCount;
    //  The number of states of the determinised and minimised automaton of the pattern, which is
    //  what Spec 8.2 counts. It is neither the size of the declaration tree, nor the number of
    //  distinct subpatterns: the interned subpattern count is reported as its own measurement. This
    //  batch measures the count from the compiled representation, under its own working-set bound
    //  and only when a measured dimension asks for it.
    //
    //  The accepted limit of this dimension is enforced on a measured count above it, and on a
    //  PROVEN LOWER BOUND of the count above it. The lower bound is content and is derived without
    //  the construction: a language whose longest acceptable trace has length `L` has a minimal
    //  automaton with at least `L + 1` states, because the walk of a longest accepted trace visits
    //  one state per element and cannot revisit one. When `L` itself has no representable value but
    //  is finite, the count is then provably at least 2^64 and is above every representable
    //  accepted limit. The refusal is the `budget_exceeded` of this dimension; no substituted
    //  number is ever compared, and no comparison is made without an accepted bound.
    //
    //  INCOMPLETE GATE. The dimension is still NOT enforced in general and this batch does not
    //  claim otherwise: when the construction does not complete, the absence is reported through
    //  `CompiledPattern::stateCount()`, and unless the proven lower bound above is itself over the
    //  accepted bound nothing about the content is decided -- no claim is made that the content
    //  stays inside `maxStateCount`. The gate stays incomplete and is reported as such rather than
    //  being closed with an internal construction bound, which is a limit of the measurement and
    //  never a content threshold.
    MeasuredParameter<std::uint64_t> maxStateCount;
    //  The deterministic work count of the compile itself: the time dimension. The count only
    //  grows, so its accepted bound is applied as a RELATIVE early stop while the compile runs: the
    //  moment the count is known to be above an accepted bound the compile stops with that
    //  dimension's `budget_exceeded`, and with no accepted bound nothing is compared and nothing is
    //  refused. A count with no representable value is a proven lower bound of at least 2^64, since
    //  the accumulation that produces it is exact, so it is above every accepted bound as well.
    MeasuredParameter<std::uint64_t> maxEvaluationSteps;
    //  The deterministic size of the compiled representation in this module's own accounting units:
    //  the memory dimension. It is an internal accounting of an internal representation, not a wire
    //  size and not an encoding. Its accepted bound is applied exactly like `maxEvaluationSteps`.
    MeasuredParameter<std::uint64_t> maxCompiledBytes;
};

//  The declared arm / deadline surface a compiled Pattern has to be contained in (plan S7A-3 item
//  2: "the containment relation between the Pattern and the arm / deadline").
//
//  The Pattern declaration carries no timing field of its own, so the containment is checked
//  against what the requirement declares: the actions its arms may name, and the declared element
//  capacity of its armed window and of its deadline. Both capacities are measured-or-pending, so an
//  unmeasured window never refuses a legal pattern and an accepted one is enforced exactly.
//
//  The assembly path consumes this gate only after it has a requirement-level capacity declaration.
//  A pending or otherwise unavailable measurement is reported as `gateIncomplete`, not as a
//  content-overrun diagnostic.
struct PatternArmBound final {
    //  The action references the requirement declares. Empty means the requirement declares no arm
    //  set to be contained in, and the atom membership check is not applicable.
    std::vector<std::string> declaredActionRefs;
    //  The maximum number of elements the requirement's armed window admits. A pattern that accepts
    //  a trace longer than this does not fit, so the comparison uses the longest acceptable length.
    MeasuredParameter<std::uint64_t> maxArmElements;
    //  The maximum number of elements the declared deadline can cover. Same comparison, on the
    //  deadline side.
    MeasuredParameter<std::uint64_t> maxDeadlineElements;
};

//  The result of the Pattern-to-arm/deadline containment gate. `gateIncomplete` is a successful
//  return carrying a non-diagnostic state: the gate could not prove containment from the available
//  measurements, so an assembly consumer must reject the product conservatively.
enum class ContainmentStatus : std::uint8_t {
    contained,
    gateIncomplete,
};

//  The read-only compiled predicate of one Pattern declaration.
//
//  The compiled value is an immutable handle over the expanded and interned pattern, plus the
//  counts the budget check measured. `matches` is the reference predicate of this batch: it decides
//  whether one observed atom trace is matched by the whole pattern, and `leftmostAcceptingArm`
//  exposes the leftmost-first decision itself instead of only its boolean consequence.
//
//  The language of the primitives is fixed here, because two of them need their rule written down
//  rather than left to a reader:
//
//    * `skip` and `instant` denote the same language, the empty word, and no trace can tell them
//      apart: they are observably different in identity only, as two distinct declaration kinds
//      that never normalize into each other, and the compiled product keeps them as distinct
//      interned subpatterns. Nothing may treat them as different languages;
//    * `complement` accepts exactly the prefixes that none of its operands reaches, so it consumes
//    a
//      prefix rather than the whole remainder and composes with what follows it in a `sequence`. In
//      particular `sequence(complement(X), Y)` accepts `w` when some split `w = p . s` has `p` not
//      reached by any operand and `s` accepted by `Y`. The end-anchored reading, in which a
//      complement swallows the entire remaining trace, is the special case in which everything
//      after it matches the empty word, and it is not the rule;
//    * a `bounded repeat` expands over every admissible copy count, and its copy order is the
//      increasing copy index, so the accepted set is the union over `k` in `[minimum, maximum]` of
//      `k` copies of the operand. A copy does not have to consume an element: an operand that
//      accepts the empty word is compiled and matched like any other, its fixed point ends the
//      expansion, and the declared finite bound does not depend on a copy consuming anything.
// Internal executable language table. Undefined transitions use optional indices; the explicit
// alphabet excludes the reference evaluator's unnamed-symbol class.
struct PatternExecutionProgram final {
    std::vector<std::string> atomRefs;
    std::size_t start;
    std::vector<std::uint8_t> accepting;
    std::vector<std::optional<std::size_t>> transitions;
    std::vector<std::uint8_t> live;
};

class CompiledPattern final {
  public:
    CompiledPattern(const CompiledPattern&) noexcept = default;
    CompiledPattern(CompiledPattern&&) noexcept = default;
    auto operator=(const CompiledPattern&) noexcept -> CompiledPattern& = default;
    auto operator=(CompiledPattern&&) noexcept -> CompiledPattern& = default;
    ~CompiledPattern() noexcept = default;

    //  Non-deterministic matching is fixed to `leftmost-first` (ABI domain 3), so the compiled
    //  predicate reports that single policy and no second one.
    [[nodiscard]] auto matchPolicy() const noexcept -> MatchPolicy;

    //  The measured dimensions of this compile. A count is a measurement of the content, never a
    //  limit on it.
    [[nodiscard]] auto declarationDepth() const noexcept -> std::uint64_t;
    //  The number of primitive instances of the complete prepare-time expansion, that is, the sum
    //  over every admissible copy count of a bounded repeat (Spec 3.8.4). Absent exactly when that
    //  sum has no representable value, which is a measurement gap: it is neither a refusal nor a
    //  statement that the language is unbounded. A budget dimension a measurement accepted is still
    //  exceeded by such a count, because every representable accepted limit is smaller than it.
    [[nodiscard]] auto fullyExpandedCount() const noexcept -> std::optional<std::uint64_t>;
    //  The largest number of primitive instances one admissible expansion branch contains, which is
    //  the `maximum x child` upper bound of one repeat copy set. It is the coarse companion of
    //  `fullyExpandedCount` and the number the representability check of the declared bounds uses;
    //  it is not an upper bound of the complete expansion, which the sum above bounds. Absent
    //  exactly when the count has no representable value. This count has no budget dimension of its
    //  own, so its absence is a measurement gap and never a refusal.
    [[nodiscard]] auto largestBranchExpansion() const noexcept -> std::optional<std::uint64_t>;
    //  The number of distinct interned subpatterns. It is the size of the compiled representation
    //  after interning and is explicitly NOT the state count of Spec 8.2, which counts the
    //  determinised and minimised automaton; the two numbers answer different questions and both
    //  are reported.
    [[nodiscard]] auto internedSubpatternCount() const noexcept -> std::uint64_t;
    //  The number of states of the determinised and minimised automaton of the pattern (Spec 8.2).
    //  It is measured from the compiled representation when it is asked for, so a compile that is
    //  never asked for it does not pay for it, and it is a function of the content alone. Absent
    //  when this batch's construction did not complete inside its own working-set bound, which is a
    //  measurement gap and neither a content limit nor a refusal by itself: the budget check
    //  refuses this dimension only on a count, or on a proven lower bound of it, that is above an
    //  accepted bound.
    [[nodiscard]] auto stateCount() const -> std::optional<std::uint64_t>;
    //  The deterministic work count of the compile: the time dimension. Absent exactly when the
    //  count has no representable value, which is a measurement gap. Against a `maxEvaluationSteps`
    //  a measurement accepted the count is compared while it grows (the relative early stop of
    //  `PatternCompileBudget`), and every representable accepted bound is smaller than a count that
    //  has no representable value.
    [[nodiscard]] auto evaluationSteps() const noexcept -> std::optional<std::uint64_t>;
    //  The accounting size of the compiled representation in this module's own units. It covers the
    //  compiled product only: it is a compile-time budget, and it says nothing about the reference
    //  evaluator's runtime working set, which `referenceEvaluatorWorkingBytes` reports separately.
    //  Absent exactly when the count has no representable value; the `maxCompiledBytes` dimension
    //  treats that absence the same way `maxEvaluationSteps` does.
    [[nodiscard]] auto compiledBytes() const noexcept -> std::optional<std::uint64_t>;

    //  The deterministic accounting size of the reference evaluator's working set for one trace of
    //  `traceLength` observed atoms, in the same accounting units as `compiledBytes`. The reference
    //  evaluator of this batch allocates one position set per compiled subpattern per trace
    //  position, so this number grows with the product of the two; it is reported because the
    //  compile budget deliberately does not cover it and no caller may read `compiledBytes` as a
    //  promise about the memory a match needs. Absent when the product has no representable value.
    //  It is a measurement of this module's reference evaluator, not a limit and not a promise
    //  about a host allocator.
    [[nodiscard]] auto referenceEvaluatorWorkingBytes(std::size_t traceLength) const noexcept
        -> std::optional<std::uint64_t>;

    //  The shortest and the longest atom trace the pattern accepts. Both are absent when the
    //  measurement has no representable value; the longest is absent as well when no finite bound
    //  exists (a `complement` consumes the remaining trace). Both absences are the same statement
    //  for a reader -- "no finite bound is available from this measurement" -- and both are exactly
    //  what a deadline containment check has to know about.
    [[nodiscard]] auto minimumTraceLength() const noexcept -> std::optional<std::uint64_t>;
    [[nodiscard]] auto maximumTraceLength() const noexcept -> std::optional<std::uint64_t>;

    //  Every atom reference the pattern names, sorted and deduplicated. This is the arm set the
    //  containment check reads.
    [[nodiscard]] auto atomRefs() const -> std::vector<std::string>;
    [[nodiscard]] auto executionProgram() const -> core::Result<PatternExecutionProgram>;

    //  The reference predicate: `trace` is the observed atom reference sequence, and the pattern
    //  matches when it accepts the whole trace. The decision is total and terminating for every
    //  declared repeat bound, including the largest representable one, because the reachable
    //  positions of a repeat converge.
    [[nodiscard]] auto matches(const std::vector<std::string>& trace) const -> bool;

    //  The arm of the root `choice` the leftmost-first matcher selects: the lowest operand index
    //  that accepts the trace. Absent when the root is not a choice, or when no arm accepts. Arm
    //  order is the declared operand order, which is the observable form of the fixed policy.
    [[nodiscard]] auto leftmostAcceptingArm(const std::vector<std::string>& trace) const
        -> std::optional<std::size_t>;

  private:
    friend auto compilePattern(const PatternDeclaration& pattern,
                               const PatternCompileBudget& budget) -> core::Result<CompiledPattern>;

    explicit CompiledPattern(
        std::shared_ptr<const detail::CompiledPatternStorage> storage) noexcept;

    std::shared_ptr<const detail::CompiledPatternStorage> storage_;
};

//  Compiles one Pattern declaration with a budget in which every bound is still pending, which is
//  the state the batch is in before S7A-9 accepts a number: the intrinsic checks (termination,
//  static finiteness of a bounded repeat, representability of the expansion count and the rejection
//  of every excluded content form) all run, and no numeric threshold is applied.
[[nodiscard]] auto compilePattern(const PatternDeclaration& pattern)
    -> core::Result<CompiledPattern>;

//  Compiles one Pattern declaration and enforces the budget dimensions that were measured.
[[nodiscard]] auto compilePattern(const PatternDeclaration& pattern,
                                  const PatternCompileBudget& budget)
    -> core::Result<CompiledPattern>;

//  Checks that a compiled Pattern is contained in the declared arm / deadline surface. A rejection
//  is a stable diagnostic only for an atom outside the declared arm set or a measured capacity that
//  the Pattern provably exceeds. Missing/pending measurements and an unavailable finite maximum
//  return `gateIncomplete`.
[[nodiscard]] auto checkPatternContainment(const CompiledPattern& pattern,
                                           const PatternArmBound& bound)
    -> core::Result<ContainmentStatus>;

//  The compiled product of one measure component: the frozen category derived from the declared
//  phase, the presence state of the grade table, and the declared grade tokens themselves. The
//  tokens are carried, not evaluated: no grade value is computed anywhere in this batch.
struct CompiledMeasureComponent final {
    PhaseKind phase;
    //  Derived from `phase` (Spec 3.20 rule 3), never copied from a caller-assigned token.
    FactCategory category;
    //  Whether the component declared a grade table. Presence only; it is not a computed grade.
    GradePresence grade;
    //  The declared grade tokens as declared, opaque: they are content, so they take part in chart
    //  / content identity and in the semantic diff, but the scale, the tolerance and the
    //  aggregation rule are CM-S10 / P1-07 and are neither interpreted nor defaulted nor evaluated
    //  here. Empty exactly when `grade` is `absent`.
    std::vector<std::string> declaredGradeTokens;

    friend auto operator==(const CompiledMeasureComponent&,
                           const CompiledMeasureComponent&) noexcept -> bool = default;
};

//  The requirement-side phase surface a Measure declaration is contained in (plan S7A-3 item 3).
struct MeasurePhaseContext final {
    //  The phases the requirement declares. A measure component that quantifies a phase the
    //  requirement did not declare has no phase to belong to.
    std::vector<PhaseDeclaration> declaredPhases;
    //  Whether the content's semantics require Release / tail. When they do and no tail phase is
    //  declared, the measure is refused: the batch never infers a tail from an implicit legacy
    //  profile, and it never generates a second requirement for one.
    bool requiresReleaseTailSemantics;
    //  The field-path prefix the rejection reports, so the assembly path keeps naming the
    //  requirement the measure belongs to. Empty reports the measure on its own.
    std::string fieldPathPrefix;
};

//  The read-only compiled measure product of one Measure declaration.
//
//  Stage 7A covers Tap and Hold head / body plus the explicitly declared Release / tail, derives
//  each component's `FactCategory` from its phase, and carries the grade table each component
//  declared. It does NOT compute grades: this batch stores the declared grade tokens as opaque
//  content, they take part in chart / content identity and in the semantic diff, and the grade
//  scale, the tolerance and the aggregation rule stay CM-S10 / P1-07. Grade evaluation is not
//  implemented here, and nothing in this type may be read as a computed grade.
//
//  A component whose content declared no grade table stays `absent`: the product has no default
//  table, no implicit upgrade and no second result dimension. `Outcome` is the frozen two-value set
//  of `kStage7AOutcomes`.
class CompiledMeasure final {
  public:
    CompiledMeasure(const CompiledMeasure&) noexcept = default;
    CompiledMeasure(CompiledMeasure&&) noexcept = default;
    auto operator=(const CompiledMeasure&) noexcept -> CompiledMeasure& = default;
    auto operator=(CompiledMeasure&&) noexcept -> CompiledMeasure& = default;
    ~CompiledMeasure() noexcept = default;

    [[nodiscard]] auto componentCount() const noexcept -> std::uint64_t;

    //  The compiled components in canonical (phase, category) order. The declaration order of the
    //  components carries no semantics, so the product reports the normalized order.
    [[nodiscard]] auto components() const -> std::vector<CompiledMeasureComponent>;

    //  The category of one component, or absent when the measure has no component for that phase.
    [[nodiscard]] auto gradeOf(PhaseKind phase) const -> std::optional<CompiledMeasureComponent>;

  private:
    friend auto compileMeasure(const MeasureSpecDeclaration& measure,
                               const MeasurePhaseContext& context) -> core::Result<CompiledMeasure>;

    explicit CompiledMeasure(
        std::shared_ptr<const detail::CompiledMeasureStorage> storage) noexcept;

    std::shared_ptr<const detail::CompiledMeasureStorage> storage_;
};

//  Compiles one Measure declaration without a requirement-side phase context: the intrinsic checks
//  (the phase-derived category, the per-component grading and the uniqueness of a component) all
//  run, and the Release / tail requirement is the one the declaration itself carries.
[[nodiscard]] auto compileMeasure(const MeasureSpecDeclaration& measure)
    -> core::Result<CompiledMeasure>;

//  Compiles one Measure declaration against the phase surface of the requirement that owns it.
[[nodiscard]] auto compileMeasure(const MeasureSpecDeclaration& measure,
                                  const MeasurePhaseContext& context)
    -> core::Result<CompiledMeasure>;

} // namespace cuexis::judgement

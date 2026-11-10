// Gameplay assembler compile: the compiled-pattern construction.

#include "gameplay_assembler_compile_internal.hpp"
#include "gameplay_assembler_internal.hpp"

namespace cuexis::judgement {

using namespace assembler_detail;

struct CompiledMetrics final {
    std::uint64_t largestBranch{0};
    std::uint64_t fullyExpanded{0};
    std::uint64_t minimumLength{0};
    std::uint64_t maximumLength{0};
    //  These counts have no representable value at all.
    bool largestBranchGap{false};
    bool fullyExpandedGap{false};
    bool minimumLengthGap{false};
    //  The longest acceptable length is FINITE and its exact value has no `uint64` representation.
    //  The arithmetic that produced it is exact and monotone, so the true value is at least 2^64.
    //  This is the one count of this struct that is read as a PROVEN LOWER BOUND of another count
    //  rather than as a count of its own: a language whose longest acceptable trace has length `L`
    //  has a minimal automaton with at least `L + 1` states (the walk of such a trace visits one
    //  state per element and cannot revisit one, because a revisited state would pump a longer
    //  accepted trace), so `L + 1 >= 2^64` proves the Spec 8.2 state count is above every
    //  representable bound. The flag is only ever set while the longest length is finite: an
    //  infinite language has no longest trace and proves no such bound.
    bool maximumLengthGap{false};
    //  No finite upper bound of the acceptable length is available for this subpattern because the
    //  declaration itself has none (a `complement` consumes the remaining trace). It is NOT a
    //  measurement gap and never a refusal: "no finite bound is available" is not "the count is
    //  above a bound", and an infinite language does not force one automaton state per length.
    bool maximumLengthUnbounded{false};
    //  No finite upper bound of the acceptable length is available to report: either the
    //  declaration itself has none or the count has no representable value. Both are the same
    //  absence for a reader of the measurement, so both are reported through this one flag.
    bool maximumLengthUnavailable{false};
};

//  The key of one interned state: two subpatterns with the same key are the same state. This is the
//  minimisation the compiled state count is defined on.
struct CompiledStateKey final {
    detail::CompiledStateKind kind{detail::CompiledStateKind::skip};
    std::string atomRef;
    std::uint64_t minimum{0};
    std::uint64_t maximum{0};
    std::vector<std::size_t> operands;
};

[[nodiscard]] auto stateKeyLess(const CompiledStateKey& left, const CompiledStateKey& right)
    -> bool {
    return std::make_tuple(static_cast<std::uint8_t>(left.kind), left.atomRef, left.minimum,
                           left.maximum, left.operands) <
           std::make_tuple(static_cast<std::uint8_t>(right.kind), right.atomRef, right.minimum,
                           right.maximum, right.operands);
}

//  Builds the interned compiled pattern of one declaration.
//
//  Both the validating measurement and this build are iterative. A recursive walker would need a
//  compiled-in depth bound to keep the host stack safe, and such a constant would be exactly the
//  hidden content limit this batch refuses to freeze: the depth is a budgeted, measurable dimension
//  of `PatternCompileBudget` instead, so an unmeasured depth refuses nothing.
[[nodiscard]] auto buildCompiledPattern(const PatternDeclaration& pattern,
                                        const PatternCompileBudget& budget)
    -> core::Result<detail::CompiledPatternStorage> {
    const auto shape = measurePatternTree(pattern.root);
    if (!shape.has_value()) {
        return core::unexpected(shape.error());
    }
    detail::CompiledPatternStorage storage;
    storage.declarationDepth = shape->declarationDepth;
    //  The deterministic work count of the compile: one unit per declaration node the validating
    //  walk visited, plus one per build step below. A wall clock would make the result depend on
    //  the host, and the canonical graph of one content has to be the same everywhere.
    //
    //  The count only grows, so its budget dimension is compared RELATIVE to the accepted bound
    //  while the walk proceeds: the moment the count is known to be above a bound a measurement
    //  accepted the compile stops, and with no accepted bound there is nothing to refuse.
    //  `stepsGap` records a count with no representable value.
    std::uint64_t steps = shape->nodeCount;
    bool stepsGap = false;

    std::map<CompiledStateKey, std::size_t, decltype(&stateKeyLess)> interned{&stateKeyLess};

    struct BuildFrame final {
        const PatternNodeDeclaration* node{nullptr};
        std::size_t next{0};
        std::vector<std::size_t> children;
        std::vector<CompiledMetrics> childMetrics;
    };
    std::vector<BuildFrame> stack;
    stack.push_back(
        BuildFrame{.node = &pattern.root, .next = 0, .children = {}, .childMetrics = {}});
    std::optional<std::size_t> rootState;
    CompiledMetrics rootMetrics;

    while (!stack.empty()) {
        steps = addChecked(steps, 1U, stepsGap);
        if (const auto stopped = earlyStopAgainst(steps, stepsGap, budget.maxEvaluationSteps,
                                                  codes::kPatternMaxEvaluationStepsPath);
            !stopped.has_value()) {
            return core::unexpected(stopped.error());
        }
        if (stack.back().next < stack.back().node->operands.size()) {
            const PatternNodeDeclaration* child = &stack.back().node->operands[stack.back().next];
            ++stack.back().next;
            stack.push_back(
                BuildFrame{.node = child, .next = 0, .children = {}, .childMetrics = {}});
            continue;
        }
        const BuildFrame frame = std::move(stack.back());
        stack.pop_back();
        const PatternNodeDeclaration& node = *frame.node;

        CompiledMetrics metrics;
        bool largestGap = false;
        bool expansionGap = false;
        bool minimumGap = false;
        bool maximumGap = false;
        switch (node.primitive) {
        case PatternPrimitive::atom:
            metrics = CompiledMetrics{
                .largestBranch = 1U, .fullyExpanded = 1U, .minimumLength = 1U, .maximumLength = 1U};
            break;
        case PatternPrimitive::skip:
        case PatternPrimitive::instant:
            //  Both accept without consuming and both ARE one primitive instance. They stay
            //  distinct declarations even though their language is the same empty word: the
            //  distinction is identity, not language.
            metrics = CompiledMetrics{.largestBranch = 1U, .fullyExpanded = 1U};
            break;
        case PatternPrimitive::sequence: {
            std::uint64_t largestBranch = 1U;
            std::uint64_t fullyExpanded = 1U;
            std::uint64_t minimum = 0U;
            std::uint64_t maximum = 0U;
            bool maximumUnbounded = false;
            for (const CompiledMetrics& child : frame.childMetrics) {
                largestBranch = addCounts(largestBranch, largestGap, child.largestBranch,
                                          child.largestBranchGap, largestGap);
                fullyExpanded = addCounts(fullyExpanded, expansionGap, child.fullyExpanded,
                                          child.fullyExpandedGap, expansionGap);
                minimum = addCounts(minimum, minimumGap, child.minimumLength,
                                    child.minimumLengthGap, minimumGap);
                maximum = addCounts(maximum, maximumGap, child.maximumLength,
                                    child.maximumLengthGap, maximumGap);
                maximumUnbounded = maximumUnbounded || child.maximumLengthUnbounded;
            }
            //  An operand with no finite longest length makes the parent's longest length infinite
            //  as well, so the parent's count is neither a value nor a proven lower bound of the
            //  state count: the gap is dropped rather than carried.
            if (maximumUnbounded) {
                maximumGap = false;
            }
            metrics = CompiledMetrics{.largestBranch = largestBranch,
                                      .fullyExpanded = fullyExpanded,
                                      .minimumLength = minimum,
                                      .maximumLength = maximum,
                                      .largestBranchGap = largestGap,
                                      .fullyExpandedGap = expansionGap,
                                      .minimumLengthGap = minimumGap,
                                      .maximumLengthGap = maximumGap,
                                      .maximumLengthUnbounded = maximumUnbounded,
                                      .maximumLengthUnavailable = maximumUnbounded || maximumGap};
            break;
        }
        case PatternPrimitive::choice: {
            std::uint64_t largestBranch = 1U;
            std::uint64_t fullyExpanded = 1U;
            std::uint64_t minimum = std::numeric_limits<std::uint64_t>::max();
            std::uint64_t maximum = 0U;
            bool maximumUnbounded = false;
            for (const CompiledMetrics& child : frame.childMetrics) {
                largestBranch = std::max(largestBranch, child.largestBranch);
                largestGap = largestGap || child.largestBranchGap;
                fullyExpanded = addCounts(fullyExpanded, expansionGap, child.fullyExpanded,
                                          child.fullyExpandedGap, expansionGap);
                //  The shortest acceptable length of a choice is the minimum over its arms, so an
                //  arm whose own minimum has no representable value is skipped numerically and only
                //  turns the parent measurement into a gap, never into a smaller number.
                if (!child.minimumLengthGap) {
                    minimum = std::min(minimum, child.minimumLength);
                }
                minimumGap = minimumGap || child.minimumLengthGap;
                maximum = std::max(maximum, child.maximumLength);
                maximumGap = maximumGap || child.maximumLengthGap;
                maximumUnbounded = maximumUnbounded || child.maximumLengthUnbounded;
            }
            //  One arm with no finite longest length makes the choice's longest length infinite, so
            //  the parent's count proves no state-count lower bound either.
            if (maximumUnbounded) {
                maximumGap = false;
            }
            metrics = CompiledMetrics{.largestBranch = largestBranch,
                                      .fullyExpanded = fullyExpanded,
                                      .minimumLength = minimum,
                                      .maximumLength = maximum,
                                      .largestBranchGap = largestGap,
                                      .fullyExpandedGap = expansionGap,
                                      .minimumLengthGap = minimumGap,
                                      .maximumLengthGap = maximumGap,
                                      .maximumLengthUnbounded = maximumUnbounded,
                                      .maximumLengthUnavailable = maximumUnbounded || maximumGap};
            break;
        }
        case PatternPrimitive::complement: {
            std::uint64_t largestBranch = 1U;
            std::uint64_t fullyExpanded = 1U;
            for (const CompiledMetrics& child : frame.childMetrics) {
                largestBranch = addCounts(largestBranch, largestGap, child.largestBranch,
                                          child.largestBranchGap, largestGap);
                fullyExpanded = addCounts(fullyExpanded, expansionGap, child.fullyExpanded,
                                          child.fullyExpandedGap, expansionGap);
            }
            //  A complement accepts exactly the prefixes that none of its operands reaches, so it
            //  consumes a prefix and not the whole remainder, and it has no finite maximum length.
            //  That absence is "no finite bound is available" and not a gap: no state-count lower
            //  bound follows from it.
            metrics = CompiledMetrics{.largestBranch = largestBranch,
                                      .fullyExpanded = fullyExpanded,
                                      .largestBranchGap = largestGap,
                                      .fullyExpandedGap = expansionGap,
                                      .maximumLengthUnbounded = true,
                                      .maximumLengthUnavailable = true};
            break;
        }
        case PatternPrimitive::boundedRepeat: {
            const CompiledMetrics& child = frame.childMetrics.front();
            //  The complete expansion of a repeat materialises every admissible copy count, so its
            //  instance count is the sum over `k` in `[minimum, maximum]` of `k` copies of the
            //  operand, and the largest single branch is `maximum` copies of it. Both are checked
            //  arithmetic. Nothing here requires a copy to consume an element: the declared bound
            //  is finite on its own, an operand that accepts the empty word is expanded like any
            //  other, and the copy order is the increasing copy index with the accepted set the
            //  union over the admissible counts.
            const std::uint64_t copies = copyCountChecked(node.repeatBounds->minimum,
                                                          node.repeatBounds->maximum, expansionGap);
            const std::uint64_t fullyExpanded = multiplyCounts(
                copies, expansionGap, child.fullyExpanded, child.fullyExpandedGap, expansionGap);
            const std::uint64_t largestBranch =
                multiplyCounts(node.repeatBounds->maximum, false, child.largestBranch,
                               child.largestBranchGap, largestGap);
            const std::uint64_t minimum =
                multiplyCounts(node.repeatBounds->minimum, false, child.minimumLength,
                               child.minimumLengthGap, minimumGap);
            const std::uint64_t maximum =
                multiplyCounts(node.repeatBounds->maximum, false, child.maximumLength,
                               child.maximumLengthGap, maximumGap);
            //  A repeat of an operand with no finite longest length has none either. The zero-copy
            //  rule of `multiplyCounts` already keeps `repeat(X, 0, 0)` at an exact longest length
            //  of zero whatever `X` is, so only a positive copy count can make the parent
            //  unavailable here.
            const bool maximumUnbounded =
                child.maximumLengthUnbounded && node.repeatBounds->maximum > 0U;
            if (maximumUnbounded) {
                maximumGap = false;
            }
            metrics = CompiledMetrics{.largestBranch = largestBranch,
                                      .fullyExpanded = fullyExpanded,
                                      .minimumLength = minimum,
                                      .maximumLength = maximum,
                                      .largestBranchGap = largestGap,
                                      .fullyExpandedGap = expansionGap,
                                      .minimumLengthGap = minimumGap,
                                      .maximumLengthGap = maximumGap,
                                      .maximumLengthUnbounded = maximumUnbounded,
                                      .maximumLengthUnavailable =
                                          child.maximumLengthUnavailable || maximumGap};
            break;
        }
        }

        CompiledStateKey key;
        switch (node.primitive) {
        case PatternPrimitive::atom:
            key = CompiledStateKey{detail::CompiledStateKind::atom, node.atomRef, 0U, 0U, {}};
            break;
        case PatternPrimitive::skip:
            key = CompiledStateKey{detail::CompiledStateKind::skip, {}, 0U, 0U, {}};
            break;
        case PatternPrimitive::instant:
            key = CompiledStateKey{detail::CompiledStateKind::instant, {}, 0U, 0U, {}};
            break;
        case PatternPrimitive::sequence:
            key = CompiledStateKey{detail::CompiledStateKind::sequence, {}, 0U, 0U, frame.children};
            break;
        case PatternPrimitive::choice:
            key = CompiledStateKey{detail::CompiledStateKind::choice, {}, 0U, 0U, frame.children};
            break;
        case PatternPrimitive::complement:
            key =
                CompiledStateKey{detail::CompiledStateKind::complement, {}, 0U, 0U, frame.children};
            break;
        case PatternPrimitive::boundedRepeat:
            key = CompiledStateKey{detail::CompiledStateKind::repeat,
                                   {},
                                   node.repeatBounds->minimum,
                                   node.repeatBounds->maximum,
                                   frame.children};
            break;
        }
        std::size_t state = 0;
        const auto existing = interned.find(key);
        if (existing != interned.end()) {
            state = existing->second;
        } else {
            state = storage.states.size();
            storage.states.push_back(detail::CompiledState{key.kind, key.atomRef, key.minimum,
                                                           key.maximum, key.operands});
            interned.emplace(key, state);
        }
        if (stack.empty()) {
            rootState = state;
            rootMetrics = metrics;
        } else {
            stack.back().children.push_back(state);
            stack.back().childMetrics.push_back(metrics);
        }
    }

    storage.root = *rootState;
    storage.fullyExpandedCount = rootMetrics.fullyExpandedGap
                                     ? std::nullopt
                                     : std::optional<std::uint64_t>{rootMetrics.fullyExpanded};
    storage.largestBranchExpansion = rootMetrics.largestBranchGap
                                         ? std::nullopt
                                         : std::optional<std::uint64_t>{rootMetrics.largestBranch};
    storage.internedSubpatternCount = static_cast<std::uint64_t>(storage.states.size());
    storage.minimumTraceLength = rootMetrics.minimumLengthGap
                                     ? std::nullopt
                                     : std::optional<std::uint64_t>{rootMetrics.minimumLength};
    storage.maximumTraceLength = rootMetrics.maximumLengthUnavailable
                                     ? std::nullopt
                                     : std::optional<std::uint64_t>{rootMetrics.maximumLength};
    storage.maximumTraceLengthGap = rootMetrics.maximumLengthGap;
    storage.evaluationSteps = stepsGap ? std::nullopt : std::optional<std::uint64_t>{steps};

    for (const detail::CompiledState& state : storage.states) {
        if (!state.atomRef.empty()) {
            storage.atomRefs.push_back(state.atomRef);
        }
    }
    sortUnique(storage.atomRefs);

    bool bytesGap = false;
    std::uint64_t bytes = 0U;
    for (const detail::CompiledState& state : storage.states) {
        bytes = addChecked(bytes, kStateAccountingBytes, bytesGap);
        bytes = addChecked(bytes, state.atomRef.size(), bytesGap);
        bytes = addChecked(
            bytes, multiplyChecked(state.operands.size(), kOperandAccountingBytes, bytesGap),
            bytesGap);
        //  Relative early stop: the accounting is compared with the accepted bound of its dimension
        //  as it is accumulated, so content that is already over the bound stops the compile here
        //  instead of being accounted to the end. Without an accepted bound nothing is compared and
        //  nothing is refused.
        if (const auto stopped = earlyStopAgainst(bytes, bytesGap, budget.maxCompiledBytes,
                                                  codes::kPatternMaxCompiledBytesPath);
            !stopped.has_value()) {
            return core::unexpected(stopped.error());
        }
    }
    storage.compiledBytes = bytesGap ? std::nullopt : std::optional<std::uint64_t>{bytes};
    return storage;
}

//  The measurement of the Spec 8.2 state count of one compiled pattern: the count itself when the
//  construction completed, plus the lower bound the DECLARATION proves whether or not it did.
//
//  The lower bound is derived from the content and never from the construction. A language whose
//  longest acceptable trace has length `L` has a minimal automaton with at least `L + 1` states:
//  the walk of an accepted trace of that length visits one state per element, and it cannot revisit
//  a state, because a revisited state would pump the accepted trace into a longer accepted one,
//  contradicting that `L` is the longest. The bound is therefore available exactly when a finite
//  longest acceptable length is measured, and it is `L + 1` that has no `uint64` representation
//  when `L` is the largest representable value: the count is then PROVABLY at least 2^64, which is
//  above every representable accepted bound.
} // namespace cuexis::judgement

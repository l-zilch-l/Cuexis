// Gameplay assembler pattern: the compile entry points and the CompiledPattern accessors.

#include "gameplay_assembler_compile_internal.hpp"
#include "gameplay_assembler_internal.hpp"

namespace cuexis::judgement {

using namespace assembler_detail;

auto compilePattern(const PatternDeclaration& pattern) -> core::Result<CompiledPattern> {
    //  No budget supplied means no accepted bound: every dimension stays pending and none of them
    //  is enforced, which is the state of this batch before S7A-9 accepts a number. The intrinsic
    //  checks -- termination, the static finiteness of a bounded repeat, the exclusion list -- all
    //  run, and a count that has no representable value is reported as an absent measurement
    //  instead of refusing the declaration.
    return compilePattern(pattern, PatternCompileBudget{});
}

auto compilePattern(const PatternDeclaration& pattern, const PatternCompileBudget& budget)
    -> core::Result<CompiledPattern> {
    auto built = buildCompiledPattern(pattern, budget);
    if (!built.has_value()) {
        return core::unexpected(built.error());
    }
    auto storage = std::make_shared<detail::CompiledPatternStorage>(std::move(*built));
    const auto enforced = enforcePatternBudget(*storage, budget);
    if (!enforced.has_value()) {
        return core::unexpected(enforced.error());
    }
    return CompiledPattern{std::move(storage)};
}

CompiledPattern::CompiledPattern(
    std::shared_ptr<const detail::CompiledPatternStorage> storage) noexcept
    : storage_(std::move(storage)) {}

auto CompiledPattern::matchPolicy() const noexcept -> MatchPolicy {
    //  ABI domain 3 fixes non-deterministic matching to `leftmost-first`, and the declaration model
    //  carries exactly that one policy, so no second policy is representable here.
    return MatchPolicy::leftmostFirst;
}

auto CompiledPattern::declarationDepth() const noexcept -> std::uint64_t {
    return storage_->declarationDepth;
}

auto CompiledPattern::fullyExpandedCount() const noexcept -> std::optional<std::uint64_t> {
    return storage_->fullyExpandedCount;
}

auto CompiledPattern::largestBranchExpansion() const noexcept -> std::optional<std::uint64_t> {
    return storage_->largestBranchExpansion;
}

auto CompiledPattern::internedSubpatternCount() const noexcept -> std::uint64_t {
    //  The interned subpattern count is the size of the compiled representation. It is deliberately
    //  not the state count of Spec 8.2, which counts the determinised and minimised automaton and
    //  is measured separately by `stateCount`.
    return storage_->internedSubpatternCount;
}

auto CompiledPattern::stateCount() const -> std::optional<std::uint64_t> {
    //  Measured from the compiled representation on demand, so a compile that never asks for the
    //  Spec 8.2 state count does not pay for it. The result is a function of the content alone and
    //  is deterministic; only a construction that does not complete inside its own working-set
    //  bound reports no count. The absence is a measurement gap of this module and never a refusal
    //  by itself: the budget check refuses this dimension only on a count or on a proven lower
    //  bound of it that is above the accepted bound.
    return measurePatternStateCount(*storage_).value;
}

auto CompiledPattern::executionProgram() const -> core::Result<PatternExecutionProgram> try {
    std::vector<detail::DfaNode> nodes;
    nodes.reserve(storage_->states.size());
    for (const auto& state : storage_->states) {
        detail::DfaNode node;
        switch (state.kind) {
        case detail::CompiledStateKind::atom:
            node.kind = detail::DfaNodeKind::atom;
            node.atomSymbol =
                static_cast<std::size_t>(std::lower_bound(storage_->atomRefs.begin(),
                                                          storage_->atomRefs.end(), state.atomRef) -
                                         storage_->atomRefs.begin());
            break;
        case detail::CompiledStateKind::sequence:
            node.kind = detail::DfaNodeKind::sequence;
            break;
        case detail::CompiledStateKind::choice:
            node.kind = detail::DfaNodeKind::choice;
            break;
        case detail::CompiledStateKind::repeat:
            node.kind = detail::DfaNodeKind::repeat;
            node.minimum = state.minimum;
            node.maximum = state.maximum;
            break;
        case detail::CompiledStateKind::complement:
            node.kind = detail::DfaNodeKind::complement;
            break;
        case detail::CompiledStateKind::skip:
        case detail::CompiledStateKind::instant:
            node.kind = detail::DfaNodeKind::epsilon;
            break;
        }
        node.operands = state.operands;
        nodes.push_back(std::move(node));
    }
    // Execution construction is subject to allocation/representability, never to the state-count
    // measurement's working-set cutoffs or an unaccepted production threshold.
    auto built =
        detail::buildMinimalPatternDfa(nodes, storage_->root, storage_->atomRefs.size() + 1, false);
    if (!built.available) {
        return core::unexpected(core::Error{"judgement.execution.relation_invalid",
                                            "executable language table could not be constructed"}
                                    .withContext("category", "invalid_relation")
                                    .withContext("severity", "error")
                                    .withContext("faulted", "false")
                                    .withContext("field.path", "requirements.pattern"));
    }
    auto& dfa = built.dfa;
    PatternExecutionProgram program{storage_->atomRefs,
                                    dfa.start,
                                    dfa.accepting,
                                    {},
                                    std::vector<std::uint8_t>(dfa.stateCount(), 0)};
    if (!program.atomRefs.empty() &&
        dfa.stateCount() > program.transitions.max_size() / program.atomRefs.size()) {
        return core::unexpected(core::Error{"judgement.execution.relation_invalid",
                                            "executable transition table size overflow"}
                                    .withContext("category", "invalid_relation")
                                    .withContext("severity", "error")
                                    .withContext("faulted", "false")
                                    .withContext("field.path", "requirements.pattern"));
    }
    program.transitions.reserve(dfa.stateCount() * program.atomRefs.size());
    for (std::size_t s = 0; s < dfa.stateCount(); ++s) {
        for (std::size_t a = 0; a < program.atomRefs.size(); ++a) {
            auto target = dfa.step(s, a);
            program.transitions.push_back(target == detail::kNoTransition
                                              ? std::nullopt
                                              : std::optional<std::size_t>{target});
        }
    }
    program.live = program.accepting;
    bool changed = true;
    while (changed) {
        changed = false;
        for (std::size_t s = 0; s < program.live.size(); ++s) {
            if (program.live[s]) {
                continue;
            }
            for (std::size_t a = 0; a < program.atomRefs.size(); ++a) {
                const auto target = program.transitions[s * program.atomRefs.size() + a];
                if (target && program.live[*target]) {
                    program.live[s] = 1;
                    changed = true;
                    break;
                }
            }
        }
    }
    std::vector<std::uint8_t> reachable(program.live.size(), 0);
    std::vector<std::size_t> queue{program.start};
    reachable[program.start] = 1;
    for (std::size_t i = 0; i < queue.size(); ++i) {
        for (std::size_t a = 0; a < program.atomRefs.size(); ++a) {
            auto next = program.transitions[queue[i] * program.atomRefs.size() + a];
            if (next && !reachable[*next]) {
                reachable[*next] = 1;
                queue.push_back(*next);
            }
        }
    }
    for (std::size_t i = 0; i < program.live.size(); ++i) {
        program.live[i] &= reachable[i];
    }
    return program;
} catch (const std::exception&) {
    return core::unexpected(
        core::Error{"judgement.execution.relation_invalid", "executable Pattern allocation failed"}
            .withContext("category", "invalid_relation")
            .withContext("severity", "error")
            .withContext("faulted", "false")
            .withContext("field.path", "requirements.pattern"));
}

auto CompiledPattern::evaluationSteps() const noexcept -> std::optional<std::uint64_t> {
    return storage_->evaluationSteps;
}

auto CompiledPattern::compiledBytes() const noexcept -> std::optional<std::uint64_t> {
    return storage_->compiledBytes;
}

auto CompiledPattern::referenceEvaluatorWorkingBytes(std::size_t traceLength) const noexcept
    -> std::optional<std::uint64_t> {
    //  The reference evaluator stores one position set of `traceLength + 1` entries for every
    //  compiled subpattern and every trace position, so its working set is `internedSubpatternCount
    //  x (traceLength + 1) x (traceLength + 1)` accounting bytes. This is the part the compile
    //  budget deliberately does not cover: `compiledBytes` is a compile-time accounting of the
    //  compiled product, and no caller may read it as a promise about the memory a match needs.
    //  Every step is checked, so a trace length whose working set has no representable size is
    //  reported as absent rather than as a wrapped number.
    const auto width = checkedAdd(static_cast<std::uint64_t>(traceLength), 1U);
    if (!width.has_value()) {
        return std::nullopt;
    }
    const auto positions = checkedMultiply(storage_->internedSubpatternCount, *width);
    if (!positions.has_value()) {
        return std::nullopt;
    }
    return checkedMultiply(*positions, *width);
}

auto CompiledPattern::minimumTraceLength() const noexcept -> std::optional<std::uint64_t> {
    return storage_->minimumTraceLength;
}

auto CompiledPattern::maximumTraceLength() const noexcept -> std::optional<std::uint64_t> {
    return storage_->maximumTraceLength;
}

auto CompiledPattern::atomRefs() const -> std::vector<std::string> {
    return storage_->atomRefs;
}

auto CompiledPattern::matches(const std::vector<std::string>& trace) const -> bool {
    const PatternTraceEvaluator evaluator{*storage_, trace};
    return evaluator.accepts(storage_->root, 0U);
}

auto CompiledPattern::leftmostAcceptingArm(const std::vector<std::string>& trace) const
    -> std::optional<std::size_t> {
    const detail::CompiledState& root = storage_->states[storage_->root];
    if (root.kind != detail::CompiledStateKind::choice) {
        return std::nullopt;
    }
    const PatternTraceEvaluator evaluator{*storage_, trace};
    for (std::size_t arm = 0; arm < root.operands.size(); ++arm) {
        if (evaluator.accepts(root.operands[arm], 0U)) {
            return arm;
        }
    }
    return std::nullopt;
}

auto checkPatternContainment(const CompiledPattern& pattern, const PatternArmBound& bound)
    -> core::Result<ContainmentStatus> {
    const std::vector<std::string> atoms = pattern.atomRefs();
    if (!bound.declaredActionRefs.empty()) {
        std::vector<std::string> declared = bound.declaredActionRefs;
        sortUnique(declared);
        for (const std::string& atom : atoms) {
            if (!std::binary_search(declared.begin(), declared.end(), atom)) {
                return core::unexpected(declarationInvalidError(
                    codes::kPatternAtomOutsideDeclaredArmsCode, codes::kGraphSection,
                    codes::kPatternDeclaredActionRefsPath,
                    "the pattern matches an atom the requirement does not declare among its arms"));
            }
        }
    }
    //  Containment covers EVERY acceptable length, not only the shortest one. The declared capacity
    //  is an interval from zero, and the acceptable lengths are a set, so the two ends of the
    //  acceptable length range decide it: the shortest match has to fit and so has the longest.
    //  When no finite longest match exists, the pattern cannot be shown to be contained at all.
    //  That is a gate-completeness gap, not proof that the declared capacity was exceeded.
    const std::optional<std::uint64_t> shortest = pattern.minimumTraceLength();
    const std::optional<std::uint64_t> longest = pattern.maximumTraceLength();
    const auto capacityStatus =
        [&shortest,
         &longest](const MeasuredParameter<std::uint64_t>& capacity) -> std::optional<bool> {
        if (capacity.measuredValue() == nullptr || !shortest.has_value() || !longest.has_value()) {
            return std::nullopt;
        }
        return *shortest <= *capacity.measuredValue() && *longest <= *capacity.measuredValue();
    };
    const auto armFits = capacityStatus(bound.maxArmElements);
    const auto deadlineFits = capacityStatus(bound.maxDeadlineElements);
    if (!atoms.empty() && (!armFits.has_value() || !deadlineFits.has_value())) {
        return ContainmentStatus::gateIncomplete;
    }
    if (armFits.has_value() && !armFits.value()) {
        //  The pattern has an acceptable length that does not fit the armed window, so it is not
        //  contained in the declared arm capacity.
        return core::unexpected(valueError(
            codes::kPatternArmBoundExceededCode, codes::kGraphSection,
            std::string{codes::kPatternPath} + "." + std::string{codes::kPatternMaxArmElementsPath},
            "the pattern has an acceptable length that exceeds the declared armed window"));
    }
    if (deadlineFits.has_value() && !deadlineFits.value()) {
        //  Same comparison on the deadline side, and again over both ends of the acceptable length
        //  range: neither end is rounded down to fit.
        return core::unexpected(
            valueError(codes::kPatternArmBoundExceededCode, codes::kGraphSection,
                       std::string{codes::kPatternPath} + "." +
                           std::string{codes::kPatternMaxDeadlineElementsPath},
                       "the pattern has an acceptable length that exceeds the declared deadline "
                       "capacity"));
    }
    return ContainmentStatus::contained;
}

auto compileMeasure(const MeasureSpecDeclaration& measure) -> core::Result<CompiledMeasure> {
    //  Without a requirement-side phase context there is no declared phase surface to be contained
    //  in and no Release / tail requirement to check: the intrinsic checks (the phase-derived
    //  category, the per-component uniqueness and the optional grade table) are the ones that run.
    return compileMeasure(measure, MeasurePhaseContext{});
}

auto compileMeasure(const MeasureSpecDeclaration& measure, const MeasurePhaseContext& context)
    -> core::Result<CompiledMeasure> {
    auto built = buildCompiledMeasure(measure, context);
    if (!built.has_value()) {
        return core::unexpected(built.error());
    }
    return CompiledMeasure{std::make_shared<detail::CompiledMeasureStorage>(std::move(*built))};
}

CompiledMeasure::CompiledMeasure(
    std::shared_ptr<const detail::CompiledMeasureStorage> storage) noexcept
    : storage_(std::move(storage)) {}

auto CompiledMeasure::componentCount() const noexcept -> std::uint64_t {
    return static_cast<std::uint64_t>(storage_->components.size());
}

auto CompiledMeasure::components() const -> std::vector<CompiledMeasureComponent> {
    return storage_->components;
}

auto CompiledMeasure::gradeOf(PhaseKind phase) const -> std::optional<CompiledMeasureComponent> {
    const auto found = std::find_if(
        storage_->components.begin(), storage_->components.end(),
        [&](const CompiledMeasureComponent& component) { return component.phase == phase; });
    if (found == storage_->components.end()) {
        return std::nullopt;
    }
    return *found;
}

//  ---------------------------------------------------------------------------------------------
//  Third part of S7A-3: prepared grace and resource plan
//  ---------------------------------------------------------------------------------------------

} // namespace cuexis::judgement

#pragma once

//  S7A-3 second half (first part): the automaton layer behind the compiled Pattern's determinised
//  and minimised state count.
//
//  Spec 8.2 counts a Pattern's states after determinisation and minimisation and never by NFA size,
//  so the compiled product needs a real construction to report that number. This header owns that
//  construction and nothing else: it is internal to the judgement module, it is not part of any
//  interface, and the compiled Pattern keeps its own interned representation.
//
//  The number it produces is a MEASUREMENT, not a limit. The construction is therefore bounded by
//  its own working set and reports "unavailable" instead of refusing a declaration, and no caller
//  may turn that bound into a content threshold. The state budget is enforced on the number this
//  header produced, or on a PROVEN LOWER BOUND of that number that the declaration itself yields
//  (the host of this construction derives it from the longest acceptable trace length, which is
//  content): an available count above an accepted bound is an overrun, a proven lower bound above
//  an accepted bound is an overrun, and an unavailable count below or without such a bound is only
//  a measurement gap. "Unavailable" from this header alone therefore never decides anything.
//
//  The alphabet is finite by construction. Every atom reference the pattern names is one symbol,
//  and one extra symbol stands for every reference the pattern does not name: an atom matches its
//  own reference and nothing else, so all unnamed references behave identically and a single extra
//  symbol class is exact rather than an approximation.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace cuexis::judgement::detail {

[[nodiscard]] inline auto checkedDfaTableEntries(std::size_t states, std::size_t symbols)
    -> std::size_t {
    const auto maximum = std::vector<std::size_t>{}.max_size();
    if (symbols != 0 && states > maximum / symbols) {
        throw std::length_error{"DFA table size is not representable"};
    }
    return states * symbols;
}
[[nodiscard]] inline auto checkedDfaStateSum(std::size_t left, std::size_t right) -> std::size_t {
    const auto maximum = std::vector<std::uint8_t>{}.max_size();
    if (left > maximum || right > maximum - left) {
        throw std::length_error{"DFA state count is not representable"};
    }
    return left + right;
}

//  The "no such state" sentinel of a partial automaton. An undefined transition rejects.
inline constexpr std::size_t kNoTransition = std::numeric_limits<std::size_t>::max();

//  The working-set bound of one measurement: `kPatternDfaConstructionStates` bounds how many
//  automaton states one construction may materialise, and `kPatternDfaConstructionWork` bounds how
//  many construction steps it may take. They exist so that a measurement can never exhaust memory
//  or time, and they are NOT limits on any content: a declaration that needs a larger construction
//  is not refused on their behalf, its state count is simply reported as unavailable, and nothing
//  else about the compile changes. Whether the state budget is exceeded is decided by the count or
//  by a proven lower bound of it, never by these bounds.
inline constexpr std::uint64_t kPatternDfaConstructionStates = 1U << 16;
inline constexpr std::uint64_t kPatternDfaConstructionWork = 1U << 24;

//  The pair space of a repeat, `(child states + 1) x (maximum + 1)` pairs, or nothing when it has
//  no representable value or does not fit the working-set bound of one construction. Knowing it
//  before the exploration starts keeps a repeat whose pair space cannot fit from being explored up
//  to the bound, and it is a limit of the measurement and never of the declared bound.
[[nodiscard]] inline auto patternRepeatPairSpace(std::size_t childStates, std::uint64_t maximum)
    -> std::optional<std::uint64_t> {
    if (maximum >= kPatternDfaConstructionStates ||
        childStates > static_cast<std::size_t>(kPatternDfaConstructionStates)) {
        return std::nullopt;
    }
    const auto pairs = static_cast<std::uint64_t>(childStates + 1U) * (maximum + 1U);
    if (pairs > kPatternDfaConstructionStates) {
        return std::nullopt;
    }
    return pairs;
}

//  The shared counter of one `buildMinimalPatternDfa` call. It is charged per constructed state and
//  per construction step; once it is exceeded the caller abandons the measurement.
class ConstructionBudget final {
  public:
    explicit ConstructionBudget(bool measurement = true) : measurement_(measurement) {}
    [[nodiscard]] auto isMeasurement() const noexcept -> bool {
        return measurement_;
    }
    [[nodiscard]] auto ok() const noexcept -> bool {
        return !exceeded_;
    }

    void chargeState() noexcept {
        if (states_ == std::numeric_limits<std::uint64_t>::max()) {
            exceeded_ = true;
            return;
        }
        ++states_;
        if (measurement_ && states_ > kPatternDfaConstructionStates) {
            exceeded_ = true;
        }
    }

    void chargeWork(std::uint64_t units = 1U) noexcept {
        if (!measurement_) {
            return;
        }
        if (units > std::numeric_limits<std::uint64_t>::max() - work_) {
            exceeded_ = true;
            return;
        }
        work_ += units;
        if (measurement_ && work_ > kPatternDfaConstructionWork) {
            exceeded_ = true;
        }
    }

    //  Gives up the whole measurement at once, for a construction whose size is known before it
    //  starts to be explored. It is the same outcome as reaching the bound the hard way: the state
    //  count is reported as unavailable, and no declaration is refused on behalf of this bound. The
    //  state budget is still decided on what the content proves, which this call does not touch.
    void abandon() noexcept {
        states_ = kPatternDfaConstructionStates + 1U;
        exceeded_ = true;
    }

    [[nodiscard]] auto states() const noexcept -> std::uint64_t {
        return states_;
    }

  private:
    bool measurement_;
    std::uint64_t states_{0};
    std::uint64_t work_{0};
    bool exceeded_{false};
};

//  A deterministic PARTIAL automaton: an undefined transition is a rejection, and the implicit dead
//  state behind it is not one of the states. The state count of this type is the number of states a
//  trace can occupy, which is the counting object of Spec 8.2 and not an NFA size.
struct PartialDfa final {
    std::size_t symbolCount{0};
    std::size_t start{0};
    std::vector<std::uint8_t> accepting;
    //  `state * symbolCount + symbol`, or `kNoTransition`.
    std::vector<std::size_t> transitions;

    [[nodiscard]] auto stateCount() const noexcept -> std::size_t {
        return accepting.size();
    }

    [[nodiscard]] auto step(std::size_t state, std::size_t symbol) const noexcept -> std::size_t {
        return transitions[state * symbolCount + symbol];
    }

    [[nodiscard]] auto accepts(const std::vector<std::size_t>& word) const noexcept -> bool {
        std::size_t state = start;
        for (const std::size_t symbol : word) {
            state = step(state, symbol);
            if (state == kNoTransition) {
                return false;
            }
        }
        return accepting[state] != 0U;
    }
};

//  A deterministic automaton, as this header hands it to its caller: `available == false` means the
//  construction did not complete inside its own bound, which is a measurement gap and never a
//  statement about the declaration. It says nothing about whether the state budget was exceeded:
//  that is decided by a caller on an available count or on a proven lower bound of the count, and
//  this flag distinguishes neither of the reasons a construction can stop.
struct PatternDfaResult final {
    bool available{false};
    PartialDfa dfa{};
};

//  The language of the empty word only. Stage 7A `skip` and `instant` both compile to it.
[[nodiscard]] inline auto epsilonDfa(std::size_t symbolCount) -> PartialDfa {
    PartialDfa dfa;
    dfa.symbolCount = symbolCount;
    dfa.start = 0U;
    dfa.accepting.assign(1U, 1U);
    dfa.transitions.assign(symbolCount, kNoTransition);
    return dfa;
}

//  The language of the single one-symbol word `symbol`.
[[nodiscard]] inline auto atomDfa(std::size_t symbol, std::size_t symbolCount) -> PartialDfa {
    PartialDfa dfa;
    dfa.symbolCount = symbolCount;
    dfa.start = 0U;
    dfa.accepting = {0U, 1U};
    dfa.transitions.assign(checkedDfaTableEntries(2U, symbolCount), kNoTransition);
    dfa.transitions[symbol] = 1U;
    return dfa;
}

//  A view over a MaterialisedNfa as the determinisation consumes it.
class MaterialisedNfa final {
  public:
    std::size_t symbolCount{0};
    std::size_t start{0};
    std::vector<std::uint8_t> accepting;
    std::vector<std::size_t> transitions;
    std::vector<std::vector<std::size_t>> epsilon;

    //  Appends one child automaton and returns the offset its states start at. The child's
    //  transition targets are relative to the child, so they are shifted by the offset here: a copy
    //  that kept them would send a later child's transition into an earlier child's state.
    [[nodiscard]] auto append(const PartialDfa& child) -> std::size_t {
        const std::size_t offset = accepting.size();
        epsilon.resize(checkedDfaStateSum(offset, child.accepting.size()));
        accepting.insert(accepting.end(), child.accepting.begin(), child.accepting.end());
        const std::size_t first = transitions.size();
        transitions.insert(transitions.end(), child.transitions.begin(), child.transitions.end());
        for (std::size_t entry = first; entry < transitions.size(); ++entry) {
            if (transitions[entry] != kNoTransition) {
                transitions[entry] += offset;
            }
        }
        return offset;
    }

    [[nodiscard]] auto viewStart() const noexcept -> std::size_t {
        return start;
    }
    [[nodiscard]] auto viewAccepting(std::size_t id) const noexcept -> bool {
        return accepting[id] != 0U;
    }
    void viewEpsilonSuccessors(std::size_t id, std::vector<std::size_t>& out) const {
        out.insert(out.end(), epsilon[id].begin(), epsilon[id].end());
    }
    [[nodiscard]] auto viewSymbolSuccessor(std::size_t id, std::size_t symbol) const noexcept
        -> std::size_t {
        return transitions[id * symbolCount + symbol];
    }
    [[nodiscard]] auto viewIdCount() const noexcept -> std::size_t {
        return accepting.size();
    }
    [[nodiscard]] auto viewSymbolCount() const noexcept -> std::size_t {
        return symbolCount;
    }
};

//  Nondeterministic presentation of `child` repeated between `minimum` and `maximum` times.
//
//  A state is a pair (child state, number of copies completed so far), plus one dedicated start
//  state that exists so that "zero copies" is expressible: the start state accepts the empty word
//  exactly when `minimum` is zero, which a pair state cannot say because a pair state can also be
//  reached after consuming symbols. Finishing a copy may either stay inside the same copy or start
//  the next one, which is the nondeterminism the determinisation resolves. The pair space is
//  generated lazily, so a bound of the largest representable value costs the construction bound
//  rather than that many states.
class RepeatNfa final {
  public:
    RepeatNfa(const PartialDfa& child, std::uint64_t minimum, std::uint64_t maximum,
              ConstructionBudget& budget)
        : child_(child), minimum_(minimum), maximum_(maximum), budget_(&budget) {
        //  The pair space of this repeat is `(child states + 1) x (maximum + 1)` pairs, and it is
        //  known before anything is explored. When it does not fit the working-set bound the
        //  measurement is abandoned immediately instead of being explored up to the bound: the
        //  bound is a limit of this module's measurement, and the declared maximum stays legal
        //  content either way.
        if (budget.isMeasurement() &&
            !patternRepeatPairSpace(child.accepting.size(), maximum).has_value()) {
            budget.abandon();
        }
    }

    [[nodiscard]] auto viewStart() const -> std::size_t {
        return kStartId;
    }

    [[nodiscard]] auto viewAccepting(std::size_t id) const -> bool {
        if (id == kStartId) {
            //  Zero copies, which is admissible exactly when the lower bound admits it.
            return minimum_ == 0U;
        }
        const auto [state, copies] = decode(id);
        return child_.accepting[state] != 0U && copies + 1U >= minimum_ && copies + 1U <= maximum_;
    }

    void viewEpsilonSuccessors(std::size_t id, std::vector<std::size_t>& out) const {
        if (id == kStartId) {
            out.push_back(idOf(child_.start, 0U));
            return;
        }
        const auto [state, copies] = decode(id);
        if (child_.accepting[state] != 0U && copies < maximum_) {
            out.push_back(idOf(child_.start, copies + 1U));
        }
    }

    [[nodiscard]] auto viewSymbolSuccessor(std::size_t id, std::size_t symbol) const
        -> std::size_t {
        if (id == kStartId) {
            return kNoTransition;
        }
        const auto [state, copies] = decode(id);
        const std::size_t target = child_.step(state, symbol);
        if (target == kNoTransition) {
            return kNoTransition;
        }
        return idOf(target, copies);
    }

    [[nodiscard]] auto viewIdCount() const noexcept -> std::size_t {
        return byId_.size() + 1U;
    }
    [[nodiscard]] auto viewSymbolCount() const noexcept -> std::size_t {
        return child_.symbolCount;
    }

  private:
    static constexpr std::size_t kStartId = 0U;

    [[nodiscard]] auto idOf(std::size_t state, std::uint64_t copies) const -> std::size_t {
        const auto key = std::pair{state, copies};
        const auto found = ids_.find(key);
        if (found != ids_.end()) {
            return found->second;
        }
        if (budget_->isMeasurement() &&
            byId_.size() + 1U >= static_cast<std::size_t>(kPatternDfaConstructionStates)) {
            //  The measurement is over; the caller abandons it. Returning an existing id keeps this
            //  function total so that no partially built subset can be interpreted as a real one.
            budget_->chargeState();
            return byId_.empty() ? kStartId : kStartId + 1U;
        }
        const std::size_t id = kStartId + byId_.size() + 1U;
        ids_.emplace(key, id);
        byId_.push_back(key);
        budget_->chargeState();
        return id;
    }

    [[nodiscard]] auto decode(std::size_t id) const -> std::pair<std::size_t, std::uint64_t> {
        if (id > kStartId && id - kStartId - 1U < byId_.size()) {
            return byId_[id - kStartId - 1U];
        }
        return {child_.start, 0U};
    }

    const PartialDfa& child_;
    std::uint64_t minimum_{0};
    std::uint64_t maximum_{0};
    ConstructionBudget* budget_{nullptr};
    //  The pair space, allocated lazily, in id order. `mutable` because the view interface is
    //  const.
    mutable std::map<std::pair<std::size_t, std::uint64_t>, std::size_t> ids_;
    mutable std::vector<std::pair<std::size_t, std::uint64_t>> byId_;
};

//  The epsilon closure of `seeds`, sorted and duplicate free.
template <class View>
void epsilonClosure(const View& view, const std::vector<std::size_t>& seeds,
                    std::vector<std::size_t>& out, ConstructionBudget& budget) {
    std::vector<std::uint8_t> marked;
    std::vector<std::size_t> pending;
    const auto mark = [&marked](std::size_t id) {
        if (id >= marked.size()) {
            marked.resize(id + 1U, 0U);
        }
        if (marked[id] != 0U) {
            return false;
        }
        marked[id] = 1U;
        return true;
    };
    for (const std::size_t seed : seeds) {
        if (mark(seed)) {
            pending.push_back(seed);
        }
    }
    std::vector<std::size_t> successors;
    while (!pending.empty()) {
        const std::size_t id = pending.back();
        pending.pop_back();
        out.push_back(id);
        budget.chargeWork();
        if (!budget.ok()) {
            return;
        }
        successors.clear();
        view.viewEpsilonSuccessors(id, successors);
        for (const std::size_t successor : successors) {
            if (mark(successor)) {
                pending.push_back(successor);
            }
        }
    }
    std::sort(out.begin(), out.end());
}

//  The subset construction of a lazily presented nondeterministic automaton. The subsets are keyed
//  in an ordered map, so one input always produces the same automaton.
template <class View>
[[nodiscard]] auto determinise(const View& view, ConstructionBudget& budget)
    -> std::optional<PartialDfa> {
    PartialDfa dfa;
    dfa.symbolCount = view.viewSymbolCount();
    std::map<std::vector<std::size_t>, std::size_t> index;
    std::vector<std::vector<std::size_t>> subsets;

    std::vector<std::size_t> startSubset;
    epsilonClosure(view, {view.viewStart()}, startSubset, budget);
    if (!budget.ok()) {
        return std::nullopt;
    }
    index.emplace(startSubset, 0U);
    subsets.push_back(std::move(startSubset));
    dfa.start = 0U;
    budget.chargeState();

    std::vector<std::size_t> moved;
    std::vector<std::size_t> closure;
    for (std::size_t cursor = 0; cursor < subsets.size(); ++cursor) {
        const std::vector<std::size_t> subset = subsets[cursor];
        bool accepting = false;
        for (const std::size_t id : subset) {
            accepting = accepting || view.viewAccepting(id);
        }
        dfa.accepting.push_back(accepting ? 1U : 0U);
        dfa.transitions.resize(
            checkedDfaTableEntries(checkedDfaStateSum(cursor, 1U), dfa.symbolCount), kNoTransition);

        for (std::size_t symbol = 0; symbol < dfa.symbolCount; ++symbol) {
            moved.clear();
            for (const std::size_t id : subset) {
                const std::size_t target = view.viewSymbolSuccessor(id, symbol);
                if (target == kNoTransition) {
                    continue;
                }
                if (std::find(moved.begin(), moved.end(), target) == moved.end()) {
                    moved.push_back(target);
                }
            }
            if (moved.empty()) {
                continue;
            }
            closure.clear();
            epsilonClosure(view, moved, closure, budget);
            if (!budget.ok()) {
                return std::nullopt;
            }
            if (closure.empty()) {
                continue;
            }
            const auto [found, inserted] = index.try_emplace(closure, subsets.size());
            if (inserted) {
                subsets.push_back(closure);
                budget.chargeState();
                if (!budget.ok()) {
                    return std::nullopt;
                }
            }
            dfa.transitions[cursor * dfa.symbolCount + symbol] = found->second;
        }
        if (!budget.ok()) {
            return std::nullopt;
        }
    }
    return dfa;
}

//  Keeps only the states that are reachable from the start and can still reach an accepting state.
//  Every state of the result is live, so an undefined transition and a transition into the implicit
//  dead state are the same thing afterwards, which is what the merging below relies on.
//
//  The empty language keeps one state, because a partial automaton still has its start state.
[[nodiscard]] inline auto trim(const PartialDfa& source, ConstructionBudget& budget)
    -> std::optional<PartialDfa> {
    const std::size_t symbols = source.symbolCount;
    const std::size_t states = source.accepting.size();

    std::vector<std::uint8_t> reachable(states, 0U);
    std::vector<std::size_t> order;
    std::vector<std::size_t> pending{source.start};
    reachable[source.start] = 1U;
    while (!pending.empty()) {
        const std::size_t state = pending.back();
        pending.pop_back();
        order.push_back(state);
        for (std::size_t symbol = 0; symbol < symbols; ++symbol) {
            const std::size_t target = source.transitions[state * symbols + symbol];
            if (target != kNoTransition && reachable[target] == 0U) {
                reachable[target] = 1U;
                pending.push_back(target);
            }
        }
        budget.chargeWork();
        if (!budget.ok()) {
            return std::nullopt;
        }
    }

    std::vector<std::vector<std::size_t>> predecessors(states);
    for (std::size_t state = 0; state < states; ++state) {
        for (std::size_t symbol = 0; symbol < symbols; ++symbol) {
            const std::size_t target = source.transitions[state * symbols + symbol];
            if (target != kNoTransition) {
                predecessors[target].push_back(state);
            }
        }
    }
    std::vector<std::uint8_t> live(states, 0U);
    pending.clear();
    for (std::size_t state = 0; state < states; ++state) {
        if (source.accepting[state] != 0U) {
            live[state] = 1U;
            pending.push_back(state);
        }
    }
    while (!pending.empty()) {
        const std::size_t state = pending.back();
        pending.pop_back();
        for (const std::size_t predecessor : predecessors[state]) {
            if (live[predecessor] == 0U) {
                live[predecessor] = 1U;
                pending.push_back(predecessor);
            }
        }
    }

    std::vector<std::size_t> remap(states, kNoTransition);
    std::size_t kept = 0U;
    for (const std::size_t state : order) {
        if (live[state] != 0U) {
            remap[state] = kept;
            ++kept;
        }
    }
    PartialDfa result;
    result.symbolCount = symbols;
    if (kept == 0U) {
        result.start = 0U;
        result.accepting.assign(1U, 0U);
        result.transitions.assign(symbols, kNoTransition);
        return result;
    }
    result.accepting.assign(kept, 0U);
    result.transitions.assign(checkedDfaTableEntries(kept, symbols), kNoTransition);
    for (const std::size_t state : order) {
        const std::size_t index = remap[state];
        if (index == kNoTransition) {
            continue;
        }
        result.accepting[index] = source.accepting[state];
        for (std::size_t symbol = 0; symbol < symbols; ++symbol) {
            const std::size_t target = source.transitions[state * symbols + symbol];
            if (target != kNoTransition && remap[target] != kNoTransition) {
                result.transitions[index * symbols + symbol] = remap[target];
            }
        }
    }
    result.start = remap[source.start];
    return result;
}

//  Merges the equivalent states of a trimmed automaton, that is, it computes the minimal partial
//  automaton of the same language.
//
//  An acyclic automaton is merged bottom-up: the key of a state is its accepting flag plus the
//  classes of its targets, and every target is classed before its predecessors are, so one reverse
//  topological pass is exact and linear. Only an automaton that contains a cycle needs the
//  completion with a dead state and the partition refinement below.
[[nodiscard]] inline auto mergeEquivalent(const PartialDfa& source, ConstructionBudget& budget)
    -> std::optional<PartialDfa> {
    const std::size_t symbols = source.symbolCount;
    const std::size_t states = source.accepting.size();

    //  Kahn's algorithm over the defined transitions: a topological order exists exactly when the
    //  automaton is acyclic.
    std::vector<std::size_t> indegree(states, 0U);
    for (std::size_t state = 0; state < states; ++state) {
        for (std::size_t symbol = 0; symbol < symbols; ++symbol) {
            const std::size_t target = source.transitions[state * symbols + symbol];
            if (target != kNoTransition) {
                ++indegree[target];
            }
        }
    }
    std::vector<std::size_t> pending;
    for (std::size_t state = 0; state < states; ++state) {
        if (indegree[state] == 0U) {
            pending.push_back(state);
        }
    }
    std::vector<std::size_t> order;
    order.reserve(states);
    while (!pending.empty()) {
        const std::size_t state = pending.back();
        pending.pop_back();
        order.push_back(state);
        for (std::size_t symbol = 0; symbol < symbols; ++symbol) {
            const std::size_t target = source.transitions[state * symbols + symbol];
            if (target != kNoTransition && --indegree[target] == 0U) {
                pending.push_back(target);
            }
        }
        budget.chargeWork();
        if (!budget.ok()) {
            return std::nullopt;
        }
    }
    if (order.size() == states) {
        std::map<std::vector<std::size_t>, std::size_t> byKey;
        std::vector<std::size_t> classOf(states, kNoTransition);
        std::vector<std::size_t> key;
        for (auto iterator = order.rbegin(); iterator != order.rend(); ++iterator) {
            const std::size_t state = *iterator;
            key.clear();
            key.reserve(symbols + 1U);
            key.push_back(source.accepting[state]);
            for (std::size_t symbol = 0; symbol < symbols; ++symbol) {
                const std::size_t target = source.transitions[state * symbols + symbol];
                key.push_back(target == kNoTransition ? kNoTransition : classOf[target]);
            }
            const auto [found, inserted] = byKey.try_emplace(key, byKey.size());
            classOf[state] = found->second;
            budget.chargeWork(symbols + 1U);
            if (!budget.ok()) {
                return std::nullopt;
            }
        }
        //  Classes are renumbered by their smallest member state so that the result does not depend
        //  on the order in which the merging happened to discover them.
        std::vector<std::size_t> newIndex(byKey.size(), kNoTransition);
        std::size_t classCount = 0U;
        std::vector<std::size_t> representative(byKey.size(), kNoTransition);
        for (std::size_t state = 0; state < states; ++state) {
            const std::size_t classId = classOf[state];
            if (newIndex[classId] == kNoTransition) {
                newIndex[classId] = classCount;
                ++classCount;
            }
            if (representative[classId] == kNoTransition) {
                representative[classId] = state;
            }
        }
        PartialDfa result;
        result.symbolCount = symbols;
        result.accepting.assign(classCount, 0U);
        result.transitions.assign(checkedDfaTableEntries(classCount, symbols), kNoTransition);
        for (std::size_t classId = 0; classId < byKey.size(); ++classId) {
            if (newIndex[classId] == kNoTransition) {
                continue;
            }
            const std::size_t state = representative[classId];
            result.accepting[newIndex[classId]] = source.accepting[state];
            for (std::size_t symbol = 0; symbol < symbols; ++symbol) {
                const std::size_t target = source.transitions[state * symbols + symbol];
                if (target != kNoTransition) {
                    result.transitions[newIndex[classId] * symbols + symbol] =
                        newIndex[classOf[target]];
                }
            }
        }
        result.start = newIndex[classOf[source.start]];
        return result;
    }

    //  A cyclic automaton. Completing it with a dead state makes the transition function total,
    //  which is what the refinement needs.
    std::vector<std::uint8_t> accepting = source.accepting;
    std::vector<std::size_t> transitions = source.transitions;
    std::size_t total = states;
    bool needsSink = false;
    for (std::size_t state = 0; state < states && !needsSink; ++state) {
        for (std::size_t symbol = 0; symbol < symbols; ++symbol) {
            if (transitions[state * symbols + symbol] == kNoTransition) {
                needsSink = true;
                break;
            }
        }
    }
    if (needsSink) {
        const std::size_t sink = states;
        total = checkedDfaStateSum(states, 1U);
        accepting.resize(total, 0U);
        transitions.resize(checkedDfaTableEntries(total, symbols), kNoTransition);
        for (std::size_t state = 0; state < states; ++state) {
            for (std::size_t symbol = 0; symbol < symbols; ++symbol) {
                if (transitions[state * symbols + symbol] == kNoTransition) {
                    transitions[state * symbols + symbol] = sink;
                }
            }
        }
        for (std::size_t symbol = 0; symbol < symbols; ++symbol) {
            transitions[sink * symbols + symbol] = sink;
        }
        budget.chargeState();
    }

    //  Moore refinement: split the accepting / non-accepting partition by the classes of the
    //  transition targets until the partition stops growing. The signatures are compared through a
    //  sort of a flat key buffer rather than through per-state allocations.
    const std::size_t width = symbols + 1U;
    std::vector<std::size_t> keys(total * width, 0U);
    std::vector<std::size_t> orderByKey(total, 0U);
    std::vector<std::size_t> next(total, 0U);
    std::vector<std::size_t> classes(total, 0U);
    for (std::size_t state = 0; state < total; ++state) {
        classes[state] = accepting[state] != 0U ? 1U : 0U;
    }
    std::size_t classCount =
        (std::find(classes.begin(), classes.end(), 0U) != classes.end()) ? 2U : 1U;
    while (true) {
        for (std::size_t state = 0; state < total; ++state) {
            keys[state * width] = classes[state];
            for (std::size_t symbol = 0; symbol < symbols; ++symbol) {
                keys[state * width + symbol + 1U] = classes[transitions[state * symbols + symbol]];
            }
            orderByKey[state] = state;
        }
        std::sort(orderByKey.begin(), orderByKey.end(),
                  [&keys, width](std::size_t left, std::size_t right) {
                      return std::lexicographical_compare(
                          keys.begin() + static_cast<std::ptrdiff_t>(left * width),
                          keys.begin() + static_cast<std::ptrdiff_t>((left + 1U) * width),
                          keys.begin() + static_cast<std::ptrdiff_t>(right * width),
                          keys.begin() + static_cast<std::ptrdiff_t>((right + 1U) * width));
                  });
        std::size_t nextCount = 0U;
        for (std::size_t index = 0; index < total; ++index) {
            if (index == 0U) {
                ++nextCount;
            } else {
                const std::size_t previous = orderByKey[index - 1U];
                const std::size_t current = orderByKey[index];
                bool same = true;
                for (std::size_t entry = 0; entry < width; ++entry) {
                    if (keys[previous * width + entry] != keys[current * width + entry]) {
                        same = false;
                        break;
                    }
                }
                if (!same) {
                    ++nextCount;
                }
            }
            next[orderByKey[index]] = nextCount - 1U;
        }
        budget.chargeWork(total);
        if (!budget.ok()) {
            return std::nullopt;
        }
        classes = next;
        if (nextCount == classCount) {
            classCount = nextCount;
            break;
        }
        classCount = nextCount;
    }

    //  A class is live when it contains a state that can reach an accepting state. The dead class
    //  is the class of the added dead state, and it is the only one that is not live.
    std::vector<std::vector<std::size_t>> predecessors(total);
    for (std::size_t state = 0; state < total; ++state) {
        for (std::size_t symbol = 0; symbol < symbols; ++symbol) {
            predecessors[transitions[state * symbols + symbol]].push_back(state);
        }
    }
    std::vector<std::uint8_t> live(total, 0U);
    std::vector<std::size_t> worklist;
    for (std::size_t state = 0; state < total; ++state) {
        if (accepting[state] != 0U) {
            live[state] = 1U;
            worklist.push_back(state);
        }
    }
    while (!worklist.empty()) {
        const std::size_t state = worklist.back();
        worklist.pop_back();
        for (const std::size_t predecessor : predecessors[state]) {
            if (live[predecessor] == 0U) {
                live[predecessor] = 1U;
                worklist.push_back(predecessor);
            }
        }
    }

    std::vector<std::size_t> newIndex(classCount, kNoTransition);
    std::size_t liveClasses = 0U;
    std::vector<std::size_t> representative(classCount, kNoTransition);
    for (std::size_t state = 0; state < total; ++state) {
        const std::size_t classId = classes[state];
        if (live[state] == 0U) {
            continue;
        }
        if (newIndex[classId] == kNoTransition) {
            newIndex[classId] = liveClasses;
            ++liveClasses;
        }
        if (representative[classId] == kNoTransition) {
            representative[classId] = state;
        }
    }
    PartialDfa result;
    result.symbolCount = symbols;
    if (liveClasses == 0U) {
        result.start = 0U;
        result.accepting.assign(1U, 0U);
        result.transitions.assign(symbols, kNoTransition);
        return result;
    }
    result.accepting.assign(liveClasses, 0U);
    result.transitions.assign(checkedDfaTableEntries(liveClasses, symbols), kNoTransition);
    //  Class ids and result state numbers are different numberings, so a result state number is
    //  translated back into its class before the representative state is read.
    std::vector<std::size_t> classOfState(liveClasses, kNoTransition);
    for (std::size_t classId = 0; classId < classCount; ++classId) {
        if (newIndex[classId] != kNoTransition) {
            classOfState[newIndex[classId]] = classId;
        }
    }
    for (std::size_t index = 0; index < liveClasses; ++index) {
        const std::size_t state = representative[classOfState[index]];
        result.accepting[index] = accepting[state];
        for (std::size_t symbol = 0; symbol < symbols; ++symbol) {
            const std::size_t target = transitions[state * symbols + symbol];
            if (live[target] == 0U) {
                continue;
            }
            result.transitions[index * symbols + symbol] = newIndex[classes[target]];
        }
    }
    result.start = newIndex[classes[source.start]];
    return result;
}

//  The minimal partial automaton of the language of `source`. `refine == false` stops after the
//  trimming, which keeps the language of every intermediate node of a declaration exactly and
//  leaves the merging to the single refinement of the root.
[[nodiscard]] inline auto minimise(const PartialDfa& source, ConstructionBudget& budget,
                                   bool refine = true) -> std::optional<PartialDfa> {
    const auto trimmed = trim(source, budget);
    if (!trimmed.has_value()) {
        return std::nullopt;
    }
    if (!refine) {
        return trimmed;
    }
    return mergeEquivalent(*trimmed, budget);
}

//  The union of the children, as a deterministic partial automaton.
[[nodiscard]] inline auto unionOf(const std::vector<const PartialDfa*>& children,
                                  std::size_t symbolCount, ConstructionBudget& budget)
    -> std::optional<PartialDfa> {
    MaterialisedNfa nfa;
    nfa.symbolCount = symbolCount;
    nfa.start = 0U;
    nfa.epsilon.resize(1U);
    for (const PartialDfa* child : children) {
        const std::size_t offset = nfa.append(*child);
        //  The union only needs one epsilon edge from the new start state to each child start.
        nfa.epsilon[0].push_back(offset + child->start);
    }
    const auto determinised = determinise(nfa, budget);
    if (!determinised.has_value()) {
        return std::nullopt;
    }
    return minimise(*determinised, budget, false);
}

//  The concatenation of the children, in order, as a deterministic partial automaton. The empty
//  child list is the language of the empty word.
[[nodiscard]] inline auto concatenationOf(const std::vector<const PartialDfa*>& children,
                                          std::size_t symbolCount, ConstructionBudget& budget)
    -> std::optional<PartialDfa> {
    if (children.empty()) {
        return epsilonDfa(symbolCount);
    }
    MaterialisedNfa nfa;
    nfa.symbolCount = symbolCount;
    std::vector<std::size_t> offsets;
    offsets.reserve(children.size());
    for (const PartialDfa* child : children) {
        offsets.push_back(nfa.append(*child));
        //  The concatenation accepts only through the last child, so every earlier child's
        //  accepting flags are cleared and its accepting states get an epsilon edge into the next
        //  child.
        for (std::size_t state = 0; state < child->accepting.size(); ++state) {
            nfa.accepting[offsets.back() + state] = 0U;
        }
    }
    for (std::size_t index = 0; index + 1U < children.size(); ++index) {
        const PartialDfa& child = *children[index];
        const std::size_t nextStart = offsets[index + 1U] + children[index + 1U]->start;
        for (std::size_t state = 0; state < child.accepting.size(); ++state) {
            if (child.accepting[state] != 0U) {
                nfa.epsilon[offsets[index] + state].push_back(nextStart);
            }
        }
    }
    const PartialDfa& last = *children.back();
    for (std::size_t state = 0; state < last.accepting.size(); ++state) {
        nfa.accepting[offsets.back() + state] = last.accepting[state];
    }
    nfa.start = offsets.front() + children.front()->start;
    const auto determinised = determinise(nfa, budget);
    if (!determinised.has_value()) {
        return std::nullopt;
    }
    return minimise(*determinised, budget, false);
}

//  The complement of the union of the children within the whole symbol alphabet. Complementing
//  needs a total function, so the automaton is completed with a dead state first and the accepting
//  flags are flipped afterwards, which is exactly what turns "the dead state rejects" into "every
//  word from here is accepted by the complement".
[[nodiscard]] inline auto complementOf(const std::vector<const PartialDfa*>& children,
                                       std::size_t symbolCount, ConstructionBudget& budget)
    -> std::optional<PartialDfa> {
    const auto united = unionOf(children, symbolCount, budget);
    if (!united.has_value()) {
        return std::nullopt;
    }
    PartialDfa total = *united;
    const std::size_t states = total.accepting.size();
    std::size_t sink = kNoTransition;
    for (std::size_t state = 0; state < states && sink == kNoTransition; ++state) {
        for (std::size_t symbol = 0; symbol < symbolCount; ++symbol) {
            if (total.transitions[state * symbolCount + symbol] == kNoTransition) {
                sink = states;
                break;
            }
        }
    }
    if (sink != kNoTransition) {
        total.accepting.resize(checkedDfaStateSum(states, 1U), 0U);
        total.transitions.resize(
            checkedDfaTableEntries(checkedDfaStateSum(states, 1U), symbolCount), kNoTransition);
        for (std::size_t state = 0; state < states; ++state) {
            for (std::size_t symbol = 0; symbol < symbolCount; ++symbol) {
                if (total.transitions[state * symbolCount + symbol] == kNoTransition) {
                    total.transitions[state * symbolCount + symbol] = sink;
                }
            }
        }
        for (std::size_t symbol = 0; symbol < symbolCount; ++symbol) {
            total.transitions[sink * symbolCount + symbol] = sink;
        }
        budget.chargeState();
    }
    for (std::uint8_t& flag : total.accepting) {
        flag = flag != 0U ? 0U : 1U;
    }
    return minimise(total, budget, false);
}

//  The language of `source` is exactly the empty word: its start state accepts and no accepting
//  state is reachable from it through at least one symbol. A construction that materialised the
//  copy index cannot decide the count of a repeat of such a child (its copy chain never
//  terminates), but the language itself is decided here, in one pass over the states.
[[nodiscard]] inline auto isExactEmptyWordLanguage(const PartialDfa& source) -> bool {
    if (source.accepting.empty() || source.start >= source.accepting.size() ||
        source.accepting[source.start] == 0U) {
        return false;
    }
    std::vector<std::uint8_t> reached(source.accepting.size(), 0U);
    std::vector<std::size_t> pending;
    for (std::size_t symbol = 0; symbol < source.symbolCount; ++symbol) {
        const std::size_t target = source.step(source.start, symbol);
        if (target != kNoTransition && reached[target] == 0U) {
            reached[target] = 1U;
            pending.push_back(target);
        }
    }
    while (!pending.empty()) {
        const std::size_t state = pending.back();
        pending.pop_back();
        if (source.accepting[state] != 0U) {
            return false;
        }
        for (std::size_t symbol = 0; symbol < source.symbolCount; ++symbol) {
            const std::size_t target = source.step(state, symbol);
            if (target != kNoTransition && reached[target] == 0U) {
                reached[target] = 1U;
                pending.push_back(target);
            }
        }
    }
    return true;
}

//  The union over k in [minimum, maximum] of k copies of `child`.
//
//  The copy index is part of the state, so a nullable child is handled exactly like any other
//  child: the finite bound never depends on a copy consuming an element, only on the copy count.
[[nodiscard]] inline auto repeatOf(const PartialDfa& child, std::uint64_t minimum,
                                   std::uint64_t maximum, ConstructionBudget& budget)
    -> std::optional<PartialDfa> {
    if (minimum > maximum) {
        return PartialDfa{.symbolCount = child.symbolCount,
                          .start = 0U,
                          .accepting = {0U},
                          .transitions =
                              std::vector<std::size_t>(child.symbolCount, kNoTransition)};
    }
    if (isExactEmptyWordLanguage(child)) {
        //  Every admissible copy count contributes the same one-word language, so the union is that
        //  word whatever the declared bounds are, and the minimised automaton is the single start
        //  state. The declared bounds remain content and identity; they do not change the language,
        //  and they must not turn a count this construction can state exactly into a gap either.
        return epsilonDfa(child.symbolCount);
    }
    const RepeatNfa view{child, minimum, maximum, budget};
    const auto determinised = determinise(view, budget);
    if (!determinised.has_value()) {
        return std::nullopt;
    }
    return minimise(*determinised, budget, false);
}

//  The kind of one node of the description this header consumes.
enum class DfaNodeKind : std::uint8_t {
    atom,
    sequence,
    choice,
    repeat,
    epsilon,
    complement,
};

//  One node of a pattern description, in a form the compiled Pattern can hand over without exposing
//  its own representation. Operands are indexes into the same table and are always smaller than the
//  node that holds them, so the table is processed bottom-up.
struct DfaNode final {
    DfaNodeKind kind{DfaNodeKind::epsilon};
    std::size_t atomSymbol{0};
    std::uint64_t minimum{0};
    std::uint64_t maximum{0};
    std::vector<std::size_t> operands;
};

//  The minimised, determinised state count of `nodes[rootIndex]` over `symbolCount` symbols.
[[nodiscard]] inline auto buildMinimalPatternDfa(const std::vector<DfaNode>& nodes,
                                                 std::size_t rootIndex, std::size_t symbolCount,
                                                 bool measurement = true) -> PatternDfaResult {
    //  A node's automaton is needed only by the nodes that reference it, so it is released as soon
    //  as its last parent has been built. Without that, a deep declaration would cost the sum of
    //  every intermediate automaton instead of the live set.
    std::vector<std::size_t> uses(nodes.size(), 0U);
    for (const DfaNode& node : nodes) {
        for (const std::size_t operand : node.operands) {
            if (operand < uses.size()) {
                ++uses[operand];
            }
        }
    }

    std::vector<PartialDfa> built(nodes.size());
    for (std::size_t index = 0; index < nodes.size(); ++index) {
        const DfaNode& node = nodes[index];
        ConstructionBudget budget{measurement};
        std::vector<const PartialDfa*> operands;
        operands.reserve(node.operands.size());
        for (const std::size_t operand : node.operands) {
            if (operand >= index) {
                return PatternDfaResult{};
            }
            operands.push_back(&built[operand]);
        }
        switch (node.kind) {
        case DfaNodeKind::atom:
            built[index] = atomDfa(node.atomSymbol, symbolCount);
            break;
        case DfaNodeKind::epsilon:
            built[index] = epsilonDfa(symbolCount);
            break;
        case DfaNodeKind::choice: {
            const auto result = unionOf(operands, symbolCount, budget);
            if (!result.has_value()) {
                return PatternDfaResult{};
            }
            built[index] = *result;
            break;
        }
        case DfaNodeKind::sequence: {
            const auto result = concatenationOf(operands, symbolCount, budget);
            if (!result.has_value()) {
                return PatternDfaResult{};
            }
            built[index] = *result;
            break;
        }
        case DfaNodeKind::complement: {
            const auto result = complementOf(operands, symbolCount, budget);
            if (!result.has_value()) {
                return PatternDfaResult{};
            }
            built[index] = *result;
            break;
        }
        case DfaNodeKind::repeat: {
            if (operands.size() != 1U) {
                return PatternDfaResult{};
            }
            const auto result = repeatOf(*operands.front(), node.minimum, node.maximum, budget);
            if (!result.has_value()) {
                return PatternDfaResult{};
            }
            built[index] = *result;
            break;
        }
        }
        if (index == rootIndex) {
            //  The intermediates were only trimmed, so the one refinement that produces the
            //  minimised automaton of Spec 8.2 happens here, once.
            const auto refined = minimise(built[index], budget, true);
            if (!refined.has_value()) {
                return PatternDfaResult{};
            }
            PatternDfaResult result;
            result.available = true;
            result.dfa = *refined;
            return result;
        }
        for (const std::size_t operand : node.operands) {
            if (operand < uses.size() && uses[operand] > 0U) {
                --uses[operand];
                if (uses[operand] == 0U) {
                    built[operand] = PartialDfa{};
                }
            }
        }
    }
    return PatternDfaResult{};
}

} // namespace cuexis::judgement::detail

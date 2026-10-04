//  S7A-3 second half (first part) compiled Pattern tests: plan S7A-3 item 2.
//
//  The cases pin down the properties the compiled predicate has to have, and the ones it must not
//  invent:
//
//    1. every Stage 7A primitive of ABI domain 3 is covered (atom, sequence, choice, bounded
//       repeat, skip, instant, complement), and each one decides the language its declaration
//       names. Two of them need their rule written down: `skip` and `instant` denote the same
//       language and differ in identity only, and a `complement` accepts the prefixes none of its
//       operands reaches, so it composes with what follows it in a `sequence` instead of swallowing
//       the whole remainder;
//    2. a bounded repeat is prepare-time finite static structure (Spec 3.8.4): the declared bounds
//       are static, the expansion is complete at compile time, the copy order is the increasing
//       copy index, an operand that accepts the empty trace is expanded instead of refused, a
//       declared bound of the largest representable value still terminates, and the excluded
//       runtime-counter and bounded-relation-instance forms stay stably rejected;
//    3. the category/primitive set of Spec 3.8.5 that Stage 7A must not consume (`sameContact`, a
//       continuous trajectory, a cross-Requirement relation) stays stably rejected with its own
//       capability id and replacement path;
//    4. non-deterministic matching is fixed to `leftmost-first`, and the choice is observable
//       instead of only asserted: arm order is declared operand order and the lowest accepting arm
//       wins;
//    5. three counts are measured and they are different measurements: the number of distinct
//       interned subpatterns, the number of primitive instances of the complete expansion, and the
//       number of states of the determinised and minimised automaton that Spec 8.2 counts. No one
//       of them is a limit, and none of them is reported under another one's name;
//    6. the budget is controllable and measurable: a pending dimension refuses nothing (there is no
//       compiled-in depth constant, and the depth is one budget entry among several), a measured
//       dimension is enforced exactly at its boundary, a count or a PROVEN LOWER BOUND of a count
//       above an ACCEPTED bound is `budget_exceeded` and never a disguised non-termination, and a
//       dimension with no accepted bound -- or a count that is not a proven lower bound, or a
//       construction that did not complete and proves no such bound -- refuses nothing at all;
//    7. the pattern / arm / deadline containment check reports an undeclared atom as an undeclared
//       relation and an oversize pattern as a declared bound that was exceeded, over ALL acceptable
//       lengths and not only the shortest one. The gate itself is incomplete: it is not wired into
//       assembly and this suite does not claim that it is;
//    8. the reference evaluator's working set is a measurement of its own and not part of the
//    compile
//       budget, and its size is checked arithmetic: an unrepresentable size is absent and never a
//       wrapped number.

#include "gameplay_test_fixture.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cuexis/core/error.hpp>
#include <cuexis/judgement/gameplay_assembler.hpp>
#include <cuexis/judgement/gameplay_graph.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

namespace judgement = cuexis::judgement;
namespace testing = cuexis::judgement::testing;

using judgement::CompiledPattern;
using judgement::MatchPolicy;
using judgement::MeasuredParameter;
using judgement::PatternArmBound;
using judgement::PatternCompileBudget;
using judgement::PatternDeclaration;
using judgement::PatternNodeDeclaration;
using judgement::PatternPrimitive;
using judgement::RepeatBoundsDeclaration;
using judgement::UnsupportedContentDeclaration;
using judgement::UnsupportedContentKind;

using testing::contextValue;

//  Asserts the three parts of a stable rejection this suite reads.
void expectRejection(const cuexis::core::Error& error, std::string_view code,
                     std::string_view category, std::string_view path) {
    CHECK(error.code() == code);
    CHECK(contextValue(error, "category") == category);
    CHECK(contextValue(error, "severity") == "error");
    CHECK(contextValue(error, "faulted") == "false");
    CHECK(contextValue(error, "field.path") == path);
}

//  --- declaration builders -----------------------------------------------------------------------

[[nodiscard]] auto atomNode(std::string atomRef) -> PatternNodeDeclaration {
    return PatternNodeDeclaration{PatternPrimitive::atom, {}, std::move(atomRef), {}, {}};
}

[[nodiscard]] auto composeNode(PatternPrimitive primitive,
                               std::vector<PatternNodeDeclaration> operands)
    -> PatternNodeDeclaration {
    return PatternNodeDeclaration{primitive, std::move(operands), {}, {}, {}};
}

[[nodiscard]] auto repeatNode(PatternNodeDeclaration operand, std::uint64_t minimum,
                              std::uint64_t maximum) -> PatternNodeDeclaration {
    return PatternNodeDeclaration{
        PatternPrimitive::boundedRepeat, {std::move(operand)}, {},
        RepeatBoundsDeclaration{.minimum = minimum, .maximum = maximum}, {}};
}

[[nodiscard]] auto declarationOf(PatternNodeDeclaration root) -> PatternDeclaration {
    return PatternDeclaration{.patternId = {},
                              .matchPolicy = MatchPolicy::leftmostFirst,
                              .root = std::move(root),
                              .required = judgement::RequiredRefs{}};
}

[[nodiscard]] auto trace(std::initializer_list<std::string_view> refs) -> std::vector<std::string> {
    std::vector<std::string> result;
    result.reserve(refs.size());
    for (const std::string_view ref : refs) {
        result.emplace_back(ref);
    }
    return result;
}

//  A budget in which `depth` is the only measured dimension, so a rejection can be attributed to
//  it.
[[nodiscard]] auto depthBudget(std::uint64_t depth) -> PatternCompileBudget {
    PatternCompileBudget budget;
    budget.maxDeclarationDepth = MeasuredParameter<std::uint64_t>::measured(depth);
    return budget;
}

//  A `sequence` chain of `depth` nested nodes over one atom, built iteratively. The declaration
//  type owns its operands recursively, so the chain is kept well inside the host stack of the test
//  process; the property under test is that the *compiler* imposes no depth bound of its own.
[[nodiscard]] auto nestedChain(std::size_t depth) -> PatternNodeDeclaration {
    PatternNodeDeclaration chain = atomNode("atom.one");
    for (std::size_t level = 1; level < depth; ++level) {
        chain = composeNode(PatternPrimitive::sequence, {std::move(chain), atomNode("atom.one")});
    }
    return chain;
}

} // namespace

TEST_CASE("S7A-3 compiled pattern covers every Stage 7A primitive", "[judgement][s7a-3][pattern]") {
    SECTION("atom") {
        const auto compiled = judgement::compilePattern(declarationOf(atomNode("atom.one")));
        REQUIRE(compiled.has_value());
        CHECK(compiled->matchPolicy() == MatchPolicy::leftmostFirst);
        CHECK(compiled->matches(trace({"atom.one"})));
        CHECK_FALSE(compiled->matches(trace({"atom.two"})));
        CHECK_FALSE(compiled->matches(trace({})));
        CHECK_FALSE(compiled->matches(trace({"atom.one", "atom.one"})));
        CHECK(compiled->minimumTraceLength() == 1U);
        CHECK(compiled->maximumTraceLength() == std::optional<std::uint64_t>{1U});
        CHECK(compiled->atomRefs() == std::vector<std::string>{"atom.one"});
    }

    SECTION("sequence") {
        const auto compiled = judgement::compilePattern(declarationOf(
            composeNode(PatternPrimitive::sequence, {atomNode("atom.one"), atomNode("atom.two")})));
        REQUIRE(compiled.has_value());
        CHECK(compiled->matches(trace({"atom.one", "atom.two"})));
        CHECK_FALSE(compiled->matches(trace({"atom.one"})));
        CHECK_FALSE(compiled->matches(trace({"atom.two", "atom.one"})));
        CHECK_FALSE(compiled->matches(trace({"atom.one", "atom.two", "atom.one"})));
        CHECK(compiled->minimumTraceLength() == 2U);
        CHECK(compiled->maximumTraceLength() == std::optional<std::uint64_t>{2U});
        CHECK(compiled->atomRefs() == std::vector<std::string>({"atom.one", "atom.two"}));
    }

    SECTION("choice") {
        const auto compiled = judgement::compilePattern(declarationOf(
            composeNode(PatternPrimitive::choice, {atomNode("atom.one"), atomNode("atom.two")})));
        REQUIRE(compiled.has_value());
        CHECK(compiled->matches(trace({"atom.one"})));
        CHECK(compiled->matches(trace({"atom.two"})));
        CHECK_FALSE(compiled->matches(trace({})));
        CHECK_FALSE(compiled->matches(trace({"atom.one", "atom.two"})));
        CHECK(compiled->minimumTraceLength() == 1U);
        CHECK(compiled->maximumTraceLength() == std::optional<std::uint64_t>{1U});
    }

    SECTION("skip and instant accept without consuming") {
        for (const PatternPrimitive primitive :
             {PatternPrimitive::skip, PatternPrimitive::instant}) {
            const auto compiled =
                judgement::compilePattern(declarationOf(composeNode(primitive, {})));
            REQUIRE(compiled.has_value());
            CHECK(compiled->matches(trace({})));
            CHECK_FALSE(compiled->matches(trace({"atom.one"})));
            CHECK(compiled->minimumTraceLength() == 0U);
            CHECK(compiled->maximumTraceLength() == std::optional<std::uint64_t>{0U});
        }
        //  The difference between the two primitives is identity and not language. Two declarations
        //  of the same empty word stay two distinct interned subpatterns, and the same word is the
        //  single state of the minimised automaton that Spec 8.2 counts.
        const auto skipOnly =
            judgement::compilePattern(declarationOf(composeNode(PatternPrimitive::skip, {})));
        const auto instantOnly =
            judgement::compilePattern(declarationOf(composeNode(PatternPrimitive::instant, {})));
        REQUIRE(skipOnly.has_value());
        REQUIRE(instantOnly.has_value());
        CHECK(skipOnly->internedSubpatternCount() == 1U);
        CHECK(instantOnly->internedSubpatternCount() == 1U);
        CHECK(skipOnly->stateCount() == std::optional<std::uint64_t>{1U});
        CHECK(instantOnly->stateCount() == std::optional<std::uint64_t>{1U});
        //  Neither primitive normalizes into the other, so a sequence of both keeps three
        //  subpatterns while the language stays the empty word.
        const auto both = judgement::compilePattern(declarationOf(
            composeNode(PatternPrimitive::sequence, {composeNode(PatternPrimitive::skip, {}),
                                                     composeNode(PatternPrimitive::instant, {})})));
        REQUIRE(both.has_value());
        CHECK(both->internedSubpatternCount() == 3U);
        CHECK(both->stateCount() == std::optional<std::uint64_t>{1U});
        CHECK(both->matches(trace({})));
        CHECK_FALSE(both->matches(trace({"atom.one"})));
    }

    SECTION("complement accepts exactly the prefixes its operand does not reach") {
        const auto compiled = judgement::compilePattern(
            declarationOf(composeNode(PatternPrimitive::complement, {atomNode("atom.one")})));
        REQUIRE(compiled.has_value());
        CHECK(compiled->matches(trace({})));
        CHECK(compiled->matches(trace({"atom.two"})));
        CHECK(compiled->matches(trace({"atom.two", "atom.one"})));
        CHECK_FALSE(compiled->matches(trace({"atom.one"})));
        //  A complement consumes a prefix whose length the declaration does not bound, so this
        //  measurement has no finite maximum length to report. The absence is reported and never
        //  replaced by a number, and it means "no finite bound is available" rather than "proven
        //  unbounded".
        CHECK_FALSE(compiled->maximumTraceLength().has_value());
    }

    SECTION("a complement consumes a prefix and composes with the next element") {
        //  `complement(atom.one)` accepts every prefix except the single element `atom.one`, so the
        //  sequence below accepts a trace whose LAST element is `atom.two` and whose prefix is not
        //  `atom.one`. The second case is the one that separates this rule from the end-anchored
        //  reading: the split leaves a non-empty remainder for `atom.two`.
        const auto compiled = judgement::compilePattern(declarationOf(
            composeNode(PatternPrimitive::sequence,
                        {composeNode(PatternPrimitive::complement, {atomNode("atom.one")}),
                         atomNode("atom.two")})));
        REQUIRE(compiled.has_value());
        CHECK(compiled->matches(trace({"atom.two"})));
        CHECK(compiled->matches(trace({"atom.two", "atom.two"})));
        CHECK(compiled->matches(trace({"atom.three", "atom.two"})));
        CHECK_FALSE(compiled->matches(trace({"atom.one", "atom.two"})));
        CHECK_FALSE(compiled->matches(trace({"atom.one", "atom.one"})));
        CHECK_FALSE(compiled->matches(trace({})));
    }
}

TEST_CASE("S7A-3 bounded repeat is prepare-time finite static structure",
          "[judgement][s7a-3][pattern][repeat]") {
    SECTION("the declared bounds decide the accepted lengths") {
        const auto compiled =
            judgement::compilePattern(declarationOf(repeatNode(atomNode("atom.one"), 2U, 4U)));
        REQUIRE(compiled.has_value());
        CHECK_FALSE(compiled->matches(trace({})));
        CHECK_FALSE(compiled->matches(trace({"atom.one"})));
        CHECK(compiled->matches(trace({"atom.one", "atom.one"})));
        CHECK(compiled->matches(trace({"atom.one", "atom.one", "atom.one"})));
        CHECK(compiled->matches(trace({"atom.one", "atom.one", "atom.one", "atom.one"})));
        CHECK_FALSE(
            compiled->matches(trace({"atom.one", "atom.one", "atom.one", "atom.one", "atom.one"})));
        CHECK(compiled->minimumTraceLength() == 2U);
        CHECK(compiled->maximumTraceLength() == std::optional<std::uint64_t>{4U});
        //  Three different measurements, none of them a limit. The complete prepare-time expansion
        //  materialises every admissible copy count, so it is 2 + 3 + 4 = 9 primitive instances;
        //  the largest single branch is `maximum x child` = 4; the interned representation holds
        //  the repeat and the atom; and the minimised automaton of `a` repeated two to four times
        //  has one state per accepted length boundary.
        CHECK(compiled->fullyExpandedCount() == std::optional<std::uint64_t>{9U});
        CHECK(compiled->largestBranchExpansion() == 4U);
        CHECK(compiled->internedSubpatternCount() == 2U);
        CHECK(compiled->stateCount() == std::optional<std::uint64_t>{5U});
    }

    SECTION("an exact repeat and a zero repeat") {
        const auto exact =
            judgement::compilePattern(declarationOf(repeatNode(atomNode("atom.one"), 3U, 3U)));
        REQUIRE(exact.has_value());
        CHECK(exact->matches(trace({"atom.one", "atom.one", "atom.one"})));
        CHECK_FALSE(exact->matches(trace({"atom.one", "atom.one"})));
        CHECK(exact->fullyExpandedCount() == std::optional<std::uint64_t>{3U});
        CHECK(exact->largestBranchExpansion() == 3U);
        CHECK(exact->stateCount() == std::optional<std::uint64_t>{4U});

        const auto none =
            judgement::compilePattern(declarationOf(repeatNode(atomNode("atom.one"), 0U, 0U)));
        REQUIRE(none.has_value());
        CHECK(none->matches(trace({})));
        CHECK_FALSE(none->matches(trace({"atom.one"})));
        //  Zero copies is one admissible branch with zero primitive instances of the operand, and
        //  the complete expansion is that same single branch.
        CHECK(none->fullyExpandedCount() == std::optional<std::uint64_t>{0U});
        CHECK(none->largestBranchExpansion() == 0U);
        CHECK(none->stateCount() == std::optional<std::uint64_t>{1U});
    }

    SECTION("a repeat of a sequence") {
        const auto compiled = judgement::compilePattern(declarationOf(repeatNode(
            composeNode(PatternPrimitive::sequence, {atomNode("atom.one"), atomNode("atom.two")}),
            2U, 3U)));
        REQUIRE(compiled.has_value());
        CHECK(compiled->matches(trace({"atom.one", "atom.two", "atom.one", "atom.two"})));
        CHECK_FALSE(compiled->matches(trace({"atom.one", "atom.two"})));
        CHECK(compiled->matches(
            trace({"atom.one", "atom.two", "atom.one", "atom.two", "atom.one", "atom.two"})));
        CHECK_FALSE(compiled->matches(trace({"atom.one", "atom.two", "atom.one", "atom.two",
                                             "atom.one", "atom.two", "atom.one", "atom.two"})));
        CHECK(compiled->minimumTraceLength() == 4U);
        CHECK(compiled->maximumTraceLength() == std::optional<std::uint64_t>{6U});
        //  The operand holds three primitive instances, so the complete expansion over the two,
        //  three and four copy branches is 2 x 3 + 3 x 3 = 15 and the largest branch is 3 x 3 = 9.
        CHECK(compiled->fullyExpandedCount() == std::optional<std::uint64_t>{15U});
        CHECK(compiled->largestBranchExpansion() == 9U);
        CHECK(compiled->stateCount() == std::optional<std::uint64_t>{7U});
    }

    SECTION("the largest representable bound still terminates") {
        const auto compiled = judgement::compilePattern(declarationOf(
            repeatNode(atomNode("atom.one"), 1U, std::numeric_limits<std::uint64_t>::max())));
        REQUIRE(compiled.has_value());
        CHECK_FALSE(compiled->matches(trace({})));
        CHECK(compiled->matches(trace({"atom.one"})));
        CHECK(compiled->matches(trace({"atom.one", "atom.one", "atom.one"})));
        //  No runtime counter and no accumulator take part: the declared bound is static content
        //  and the expansion is decided by the trace positions alone.
        CHECK(compiled->largestBranchExpansion() == std::numeric_limits<std::uint64_t>::max());
        CHECK(compiled->compiledBytes() > 0U);
        //  The complete expansion over every admissible copy count has no representable value,
        //  which is a measurement gap: it is reported as absent and is not a refusal of the
        //  declaration.
        CHECK_FALSE(compiled->fullyExpandedCount().has_value());
        //  The pair space of this repeat does not fit this batch's construction bound either, so
        //  the Spec 8.2 count is a measurement gap rather than a guess. The gap is decided from the
        //  declared bound before any copy is materialised, it is reported as an absent count, and
        //  it refuses nothing. The count itself is `UINT64_MAX + 1` (see the measurement test
        //  above), which has no `uint64_t` representation.
        CHECK_FALSE(compiled->stateCount().has_value());
    }

    SECTION("an operand that can match the empty trace is expanded, not refused") {
        //  A copy does not have to consume an element. The declared bound is finite on its own, the
        //  copy order is the increasing copy index, and the accepted set is the union over the
        //  admissible copy counts, so a nullable operand is ordinary content. The case terminating
        //  at all is the termination evidence: the position set reaches a fixed point and the
        //  expansion ends there instead of walking the declared bound.
        const auto bare = judgement::compilePattern(
            declarationOf(repeatNode(composeNode(PatternPrimitive::skip, {}), 1U, 4U)));
        REQUIRE(bare.has_value());
        CHECK(bare->matches(trace({})));
        CHECK_FALSE(bare->matches(trace({"atom.one"})));
        CHECK(bare->minimumTraceLength() == 0U);
        CHECK(bare->maximumTraceLength() == std::optional<std::uint64_t>{0U});
        //  One instance per copy over the four admissible copy counts.
        CHECK(bare->fullyExpandedCount() == std::optional<std::uint64_t>{10U});
        CHECK(bare->largestBranchExpansion() == 4U);
        CHECK(bare->stateCount() == std::optional<std::uint64_t>{1U});

        const auto optionalArm = judgement::compilePattern(declarationOf(
            repeatNode(composeNode(PatternPrimitive::choice,
                                   {atomNode("atom.one"), composeNode(PatternPrimitive::skip, {})}),
                       1U, 2U)));
        REQUIRE(optionalArm.has_value());
        CHECK(optionalArm->matches(trace({})));
        CHECK(optionalArm->matches(trace({"atom.one"})));
        CHECK(optionalArm->matches(trace({"atom.one", "atom.one"})));
        CHECK_FALSE(optionalArm->matches(trace({"atom.one", "atom.one", "atom.one"})));
        CHECK(optionalArm->stateCount() == std::optional<std::uint64_t>{3U});

        //  Equivalence evidence: the same language written without a repeat. Every word of length
        //  zero to three over the two symbols is decided identically, and the two products agree on
        //  the minimised state count as well, which they cannot do by accident here.
        const auto explicitForm = judgement::compilePattern(declarationOf(
            composeNode(PatternPrimitive::choice,
                        {composeNode(PatternPrimitive::skip, {}), atomNode("atom.one"),
                         composeNode(PatternPrimitive::sequence,
                                     {atomNode("atom.one"), atomNode("atom.one")})})));
        REQUIRE(explicitForm.has_value());
        const std::vector<std::string> symbols{"atom.one", "atom.two"};
        std::vector<std::vector<std::string>> words{{}};
        std::vector<std::vector<std::string>> frontier{{}};
        for (std::size_t length = 0; length < 3U; ++length) {
            std::vector<std::vector<std::string>> extendedWords;
            for (const std::vector<std::string>& word : frontier) {
                for (const std::string& symbol : symbols) {
                    std::vector<std::string> extended = word;
                    extended.push_back(symbol);
                    words.push_back(extended);
                    extendedWords.push_back(std::move(extended));
                }
            }
            frontier = std::move(extendedWords);
        }
        for (const std::vector<std::string>& word : words) {
            CHECK(optionalArm->matches(word) == explicitForm->matches(word));
        }
        CHECK(optionalArm->stateCount() == explicitForm->stateCount());
    }

    SECTION("a runtime counter and an unexpanded relation instance are still stably rejected") {
        PatternNodeDeclaration counter = repeatNode(atomNode("atom.one"), 1U, 3U);
        counter.unsupportedForm =
            UnsupportedContentDeclaration{.kind = UnsupportedContentKind::runtimeRepeatCounter,
                                          .declaredToken = "declared.token"};
        const auto counterChecked = judgement::compilePattern(declarationOf(counter));
        REQUIRE_FALSE(counterChecked.has_value());
        expectRejection(counterChecked.error(), "capability.permanently_unsupported",
                        "capability_disabled", "pattern.root.unsupportedForm");

        PatternNodeDeclaration dynamic = atomNode("atom.one");
        dynamic.unsupportedForm = UnsupportedContentDeclaration{
            .kind = UnsupportedContentKind::dynamicRequirementGeneration,
            .declaredToken = "declared.token"};
        const auto dynamicChecked = judgement::compilePattern(declarationOf(dynamic));
        REQUIRE_FALSE(dynamicChecked.has_value());
        expectRejection(dynamicChecked.error(), "capability.permanently_unsupported",
                        "capability_disabled", "pattern.root.unsupportedForm");

        //  The one form that really is a non-terminating source keeps that category, so the
        //  category is never borrowed for anything else.
        PatternNodeDeclaration unexpanded =
            repeatNode(atomNode("atom.one"), 0U, std::numeric_limits<std::uint64_t>::max());
        unexpanded.unsupportedForm =
            UnsupportedContentDeclaration{.kind = UnsupportedContentKind::boundedRelationInstance,
                                          .declaredToken = "declared.token"};
        const auto unexpandedChecked = judgement::compilePattern(declarationOf(unexpanded));
        REQUIRE_FALSE(unexpandedChecked.has_value());
        expectRejection(unexpandedChecked.error(), "judgement.s7a3.content.unsupported_form",
                        "non_terminating_source", "pattern.root.unsupportedForm");
    }
}

TEST_CASE("S7A-3 pattern relation forms stay stably rejected", "[judgement][s7a-3][pattern]") {
    const auto rejectedWith = [](UnsupportedContentKind kind, std::string_view code,
                                 std::string_view capabilityId, std::string_view remediation) {
        PatternNodeDeclaration node = atomNode("atom.one");
        node.unsupportedForm =
            UnsupportedContentDeclaration{.kind = kind, .declaredToken = "declared.token"};
        const auto compiled = judgement::compilePattern(declarationOf(node));
        REQUIRE_FALSE(compiled.has_value());
        expectRejection(compiled.error(), code, "capability_disabled",
                        "pattern.root.unsupportedForm");
        CHECK(contextValue(compiled.error(), "capabilityId") == capabilityId);
        CHECK(contextValue(compiled.error(), "remediation") == remediation);
    };

    //  R-11: `sameContact` and a cross-Requirement relation hidden inside a pattern.
    rejectedWith(UnsupportedContentKind::sameContact, "pattern.relation_unsupported",
                 "pattern.relation.v1", "7B+");
    rejectedWith(UnsupportedContentKind::crossRequirementRelation, "pattern.relation_unsupported",
                 "pattern.relation.v1", "7B+");
    //  R-05: a continuous trajectory as the frozen rejection with its own capability id.
    rejectedWith(UnsupportedContentKind::continuousTrajectory, "input.continuous_unsupported",
                 "input.trajectory.v1", "S7B-1");
    rejectedWith(UnsupportedContentKind::contactFollowingSlider, "input.continuous_unsupported",
                 "input.trajectory.v1", "S7B-1");
}

TEST_CASE("S7A-3 compiled pattern is leftmost-first", "[judgement][s7a-3][pattern][match]") {
    SECTION("the lowest accepting arm wins") {
        //  Both arms accept the whole trace, so the decision is the policy and nothing else.
        const auto first = judgement::compilePattern(declarationOf(composeNode(
            PatternPrimitive::choice,
            {atomNode("atom.one"),
             composeNode(PatternPrimitive::sequence,
                         {atomNode("atom.one"), composeNode(PatternPrimitive::skip, {})})})));
        REQUIRE(first.has_value());
        CHECK(first->matches(trace({"atom.one"})));
        CHECK(first->leftmostAcceptingArm(trace({"atom.one"})) == std::optional<std::size_t>{0U});

        const auto second = judgement::compilePattern(declarationOf(
            composeNode(PatternPrimitive::choice,
                        {atomNode("atom.two"), atomNode("atom.one"), atomNode("atom.one")})));
        REQUIRE(second.has_value());
        //  Arm 0 does not accept, so the lowest accepting arm is arm 1 and not the last one.
        CHECK(second->leftmostAcceptingArm(trace({"atom.one"})) == std::optional<std::size_t>{1U});
        CHECK_FALSE(second->leftmostAcceptingArm(trace({"atom.three"})).has_value());
    }

    SECTION("a later arm never wins over an accepting earlier arm") {
        const auto compiled = judgement::compilePattern(declarationOf(composeNode(
            PatternPrimitive::choice,
            {composeNode(PatternPrimitive::sequence, {atomNode("atom.one"), atomNode("atom.two")}),
             composeNode(PatternPrimitive::sequence,
                         {atomNode("atom.one"), composeNode(PatternPrimitive::skip, {})})})));
        REQUIRE(compiled.has_value());
        //  The earlier, longer arm accepts, so it is selected.
        CHECK(compiled->leftmostAcceptingArm(trace({"atom.one", "atom.two"})) ==
              std::optional<std::size_t>{0U});
        //  It does not accept the shorter trace, so the later arm is selected; order and not
        //  preference decides, and the matcher never prefers a later arm.
        CHECK(compiled->leftmostAcceptingArm(trace({"atom.one"})) ==
              std::optional<std::size_t>{1U});
    }

    SECTION("the arm report is defined for a root choice only") {
        const auto compiled = judgement::compilePattern(declarationOf(atomNode("atom.one")));
        REQUIRE(compiled.has_value());
        CHECK_FALSE(compiled->leftmostAcceptingArm(trace({"atom.one"})).has_value());
    }
}

TEST_CASE("S7A-3 the interned count and the Spec 8.2 state count are different measurements",
          "[judgement][s7a-3][pattern]") {
    //  Structurally identical subpatterns share one interned state, so the interned count follows
    //  the number of distinct subpatterns and not the size of the declaration tree. It is
    //  explicitly not the state count of Spec 8.2, which counts the determinised and minimised
    //  automaton: for a sequence of four identical atoms the interned count is 2 while the
    //  automaton has one state per prefix of the accepted word.
    const auto repeated = judgement::compilePattern(declarationOf(
        composeNode(PatternPrimitive::sequence, {atomNode("atom.one"), atomNode("atom.one"),
                                                 atomNode("atom.one"), atomNode("atom.one")})));
    REQUIRE(repeated.has_value());
    CHECK(repeated->internedSubpatternCount() == 2U);
    CHECK(repeated->stateCount() == std::optional<std::uint64_t>{5U});
    CHECK(repeated->fullyExpandedCount() == std::optional<std::uint64_t>{5U});
    CHECK(repeated->largestBranchExpansion() == 5U);
    CHECK(repeated->declarationDepth() == 2U);
    CHECK(repeated->matches(trace({"atom.one", "atom.one", "atom.one", "atom.one"})));

    const auto distinct = judgement::compilePattern(declarationOf(
        composeNode(PatternPrimitive::sequence, {atomNode("atom.one"), atomNode("atom.two")})));
    REQUIRE(distinct.has_value());
    CHECK(distinct->internedSubpatternCount() == 3U);
    CHECK(distinct->stateCount() == std::optional<std::uint64_t>{3U});

    //  The minimisation is a real one and not the structural deduplication: four atoms and two
    //  atoms give the same interned count of 2 only for the same atom, while the automata differ,
    //  and two structurally different declarations of the same language agree on the minimised
    //  count.
    const auto oneAtom = judgement::compilePattern(declarationOf(atomNode("atom.one")));
    REQUIRE(oneAtom.has_value());
    CHECK(oneAtom->internedSubpatternCount() == 1U);
    CHECK(oneAtom->stateCount() == std::optional<std::uint64_t>{2U});
}

//  --- independent measurement of the minimal state count
//  ------------------------------------------
//
//  Counts the distinct NON-EMPTY residual languages of `pattern` over every word of length up to
//  `window`, each residual characterised by the compiled predicate's own decision on every
//  continuation of length up to `window`. This is the Myhill-Nerode definition stated
//  operationally, and it is read through the compiled predicate rather than through the
//  construction under test, so it is independent evidence for the counts this suite asserts.
//
//  It is exact only while `window` is wide enough to tell the live residuals apart: past that the
//  truncation saturates and the number under-reports. The empty residual is the implicit dead state
//  of a partial automaton and is not counted, which is the same counting object `stateCount` uses.
[[nodiscard]] auto distinctNonEmptyResiduals(const CompiledPattern& pattern, std::size_t window)
    -> std::size_t {
    const std::vector<std::string> symbols{"atom.one", "atom.two"};
    std::vector<std::vector<std::string>> words{{}};
    std::vector<std::vector<std::string>> frontier{{}};
    for (std::size_t step = 0; step < window; ++step) {
        std::vector<std::vector<std::string>> next;
        for (const std::vector<std::string>& word : frontier) {
            for (const std::string& symbol : symbols) {
                std::vector<std::string> extended = word;
                extended.push_back(symbol);
                words.push_back(extended);
                next.push_back(std::move(extended));
            }
        }
        frontier = std::move(next);
    }
    std::vector<std::string> signatures;
    for (const std::vector<std::string>& word : words) {
        std::string signature;
        for (const std::vector<std::string>& continuation : words) {
            std::vector<std::string> combined = word;
            combined.insert(combined.end(), continuation.begin(), continuation.end());
            signature.push_back(pattern.matches(combined) ? '1' : '0');
        }
        if (signature.find('1') == std::string::npos) {
            continue;
        }
        if (std::find(signatures.begin(), signatures.end(), signature) == signatures.end()) {
            signatures.push_back(std::move(signature));
        }
    }
    return signatures.size();
}

TEST_CASE("S7A-3 a repeat of the empty word has one state and is not a gap",
          "[judgement][s7a-3][pattern][budget]") {
    //  `repeat(skip, 1, max)`: `skip` denotes the empty word, so every admissible copy count
    //  contributes the same one-word language and the repeat denotes the empty word as well. Its
    //  minimised automaton has the single start state, and that is what the measurement must
    //  report. An internal construction bound of this module may not turn a count it can state
    //  exactly into a gap, and it may never turn it into a refusal. The declared bounds stay
    //  content and stay in identity; they do not change the language.
    const PatternDeclaration declaration = declarationOf(repeatNode(
        composeNode(PatternPrimitive::skip, {}), 1U, std::numeric_limits<std::uint64_t>::max()));
    const auto compiled = judgement::compilePattern(declaration);
    REQUIRE(compiled.has_value());
    CHECK(compiled->stateCount() == std::optional<std::uint64_t>{1U});
    //  Independent evidence: exactly one non-empty residual language, the empty word itself.
    CHECK(distinctNonEmptyResiduals(*compiled, 3U) == 1U);
    CHECK(compiled->matches(trace({})));
    CHECK_FALSE(compiled->matches(trace({"atom.one"})));

    SECTION("a measured state dimension of one is satisfied, not refused") {
        PatternCompileBudget one;
        one.maxStateCount = MeasuredParameter<std::uint64_t>::measured(1U);
        const auto accepted = judgement::compilePattern(declaration, one);
        REQUIRE(accepted.has_value());
        CHECK(accepted->stateCount() == std::optional<std::uint64_t>{1U});
    }

    SECTION("the same language written with the other empty-word primitive agrees") {
        //  `skip` and `instant` are distinct declarations with the same language, so the measured
        //  count has to agree while the interned identity does not.
        const auto asInstant = judgement::compilePattern(
            declarationOf(repeatNode(composeNode(PatternPrimitive::instant, {}), 1U,
                                     std::numeric_limits<std::uint64_t>::max())));
        REQUIRE(asInstant.has_value());
        CHECK(asInstant->stateCount() == std::optional<std::uint64_t>{1U});
        CHECK(asInstant->internedSubpatternCount() == compiled->internedSubpatternCount());
    }
}

TEST_CASE("S7A-3 the state count of a bounded repeat grows with its declared bound",
          "[judgement][s7a-3][pattern][budget]") {
    //  MEASURED, not assumed. `repeat(atom.one, 1, M)` denotes `a^(1..M)`, whose minimal partial
    //  automaton holds the start state plus one state per admissible copy count, because
    //  `L(a^j) = a^(0..M-j)` is a different residual for every `j`. The module's own construction
    //  reports `M + 1` states for M = 1..4, and the independent residual measurement over the
    //  compiled predicate reports the same numbers. Measurement method: `stateCount()` for the
    //  construction side, `distinctNonEmptyResiduals(..., 4)` for the independent side.
    for (const std::uint64_t maximum : {1ULL, 2ULL, 3ULL, 4ULL}) {
        const auto compiled =
            judgement::compilePattern(declarationOf(repeatNode(atomNode("atom.one"), 1U, maximum)));
        REQUIRE(compiled.has_value());
        CHECK(compiled->stateCount() == std::optional<std::uint64_t>{maximum + 1U});
        CHECK(distinctNonEmptyResiduals(*compiled, 4U) == maximum + 1U);
    }
    //  Beyond M = 4 the four-symbol residual window saturates (five residuals is all it can tell
    //  apart), while the construction keeps counting: the independent number is therefore quoted
    //  inside its window only, and it under-reports rather than over-reports.
    const auto wide =
        judgement::compilePattern(declarationOf(repeatNode(atomNode("atom.one"), 1U, 8U)));
    REQUIRE(wide.has_value());
    CHECK(wide->stateCount() == std::optional<std::uint64_t>{9U});
    CHECK(distinctNonEmptyResiduals(*wide, 4U) == 2U);

    //  The conclusion this suite registers: for `maximum = UINT64_MAX` the count is
    //  `UINT64_MAX + 1`, which has no `uint64_t` representation, so the module reports a
    //  measurement gap. The count is NOT 2 and NOT any other small constant. A gap is not a refusal
    //  by itself, but it is not "nothing left to compare" either: this declaration measures a
    //  FINITE longest acceptable length of `UINT64_MAX`, and the state count of a language whose
    //  longest acceptable trace has length `L` is at least `L + 1` -- the walk of a longest
    //  accepted trace visits `L + 1` distinct states, because a revisited state would pump a longer
    //  accepted trace. `L + 1` is exactly the count above, so the absent measurement carries a
    //  proven lower bound of `2^64`, which is above every representable accepted bound. The
    //  consequence is locked down by `a state-count measurement gap refuses only a proven overrun`.
    const auto unbounded = judgement::compilePattern(declarationOf(
        repeatNode(atomNode("atom.one"), 1U, std::numeric_limits<std::uint64_t>::max())));
    REQUIRE(unbounded.has_value());
    CHECK_FALSE(unbounded->stateCount().has_value());
    CHECK(unbounded->maximumTraceLength() ==
          std::optional<std::uint64_t>{std::numeric_limits<std::uint64_t>::max()});
    CHECK(unbounded->largestBranchExpansion() == std::numeric_limits<std::uint64_t>::max());
}

TEST_CASE("S7A-3 a state-count measurement gap refuses only a proven overrun",
          "[judgement][s7a-3][pattern][budget]") {
    //  `repeat(atom.one, 1, max)` denotes `atom.one` repeated between one and the largest
    //  representable number of times. Its Spec 8.2 count is not representable in 64 bits at all (a
    //  finite bound admits one state per copy count, so `maximum + 1` states), and this batch's
    //  bounded-repeat construction materialises the copy index, so the pair space does not fit the
    //  working-set bound of one measurement either. The count is therefore absent: a MEASUREMENT
    //  GAP, and the module's own construction bound is a bound of its measurement and never of the
    //  content.
    //
    //  What the gap does NOT mean is "nothing can be decided". The declaration measures a FINITE
    //  longest acceptable length `L = UINT64_MAX`, and the state count of a language whose longest
    //  acceptable trace has length `L` is at least `L + 1`: the walk of a longest accepted trace
    //  visits one state per element and cannot revisit a state, because a revisited state would
    //  pump a longer accepted trace. The absent count therefore carries a PROVEN LOWER BOUND of
    //  `2^64`, and against an ACCEPTED state bound that bound is really exceeded for any `uint64`
    //  limit. The refusal uses the same `budget_exceeded` category and the same
    //  `pattern.maxStateCount` path every measured overrun of this dimension uses; no new code,
    //  category or path is introduced.
    //
    //  Time line of this criterion (both corrections are in force at once):
    //    * round 1 mapped "count has no representable value" onto an unconditional refusal, which
    //      round 2 rejected -- an exact-arithmetic overflow of an internal counter is not a content
    //      threshold;
    //    * round 2 then accepted a gap with an accepted bound as "nothing to compare", which round
    //    3
    //      rejected -- `2^64` is a proven lower bound and IS above the accepted limit.
    //  Nothing here compares a substituted number, and nothing refuses on the module's construction
    //  bound alone.
    const PatternDeclaration declaration = declarationOf(
        repeatNode(atomNode("atom.one"), 1U, std::numeric_limits<std::uint64_t>::max()));
    const auto compiled = judgement::compilePattern(declaration);
    REQUIRE(compiled.has_value());
    CHECK_FALSE(compiled->stateCount().has_value());
    CHECK(compiled->maximumTraceLength() ==
          std::optional<std::uint64_t>{std::numeric_limits<std::uint64_t>::max()});

    SECTION("no accepted bound: the count is a measurement gap and nothing is refused") {
        const auto accepted = judgement::compilePattern(declaration, PatternCompileBudget{});
        REQUIRE(accepted.has_value());
        CHECK_FALSE(accepted->stateCount().has_value());
    }

    SECTION("a measured state bound plus a proven lower bound: a real overrun is refused") {
        //  上一轮按缺口放行，经 Codex 第 3 轮复审驳回（同一 thread
        //  `01a0fe91-416c-7f31-a48a-d1890a0e68d1`，verdict `reject`，confidence 0.93）：2^64 是
        //  经证明的下界，对已接受上限 1 确实超限，因此这里的期望由"接受"改为"拒绝"。
        PatternCompileBudget budget;
        budget.maxStateCount = MeasuredParameter<std::uint64_t>::measured(1U);
        const auto refused = judgement::compilePattern(declaration, budget);
        REQUIRE_FALSE(refused.has_value());
        expectRejection(refused.error(), "judgement.s7a3.pattern.budget_exceeded",
                        "budget_exceeded", "pattern.maxStateCount");
        CHECK(contextValue(refused.error(), "category") != "non_terminating_source");
    }

    SECTION("the widest representable accepted bound is exceeded as well") {
        //  No accepted `uint64` bound can hold this content: `2^64 > UINT64_MAX`. The widest bound
        //  a caller can accept is therefore refused exactly like the narrowest one.
        PatternCompileBudget widest;
        widest.maxStateCount =
            MeasuredParameter<std::uint64_t>::measured(std::numeric_limits<std::uint64_t>::max());
        const auto refused = judgement::compilePattern(declaration, widest);
        REQUIRE_FALSE(refused.has_value());
        expectRejection(refused.error(), "judgement.s7a3.pattern.budget_exceeded",
                        "budget_exceeded", "pattern.maxStateCount");
    }

    SECTION("a construction that does not complete is not a refusal by itself") {
        //  The same kind of construction gap on a declaration whose proven lower bound an accepted
        //  bound can still admit: `repeat(atom.one, 1, 100000)` measures a longest acceptable
        //  length of 100000, so the proven lower bound of the state count is 100001, while its pair
        //  space does not fit this batch's construction bound either, so the count itself stays
        //  absent. The construction bound alone refuses nothing: an accepted bound at or above the
        //  proven lower bound accepts and leaves the dimension unenforced (INCOMPLETE GATE), and
        //  one below it is a proven overrun.
        const PatternDeclaration mid = declarationOf(repeatNode(atomNode("atom.one"), 1U, 100000U));
        const auto midCompiled = judgement::compilePattern(mid);
        REQUIRE(midCompiled.has_value());
        CHECK_FALSE(midCompiled->stateCount().has_value());
        CHECK(midCompiled->maximumTraceLength() == std::optional<std::uint64_t>{100000U});

        PatternCompileBudget atBound;
        atBound.maxStateCount = MeasuredParameter<std::uint64_t>::measured(100001U);
        const auto accepted = judgement::compilePattern(mid, atBound);
        REQUIRE(accepted.has_value());
        CHECK_FALSE(accepted->stateCount().has_value());

        PatternCompileBudget belowBound;
        belowBound.maxStateCount = MeasuredParameter<std::uint64_t>::measured(100000U);
        const auto refused = judgement::compilePattern(mid, belowBound);
        REQUIRE_FALSE(refused.has_value());
        expectRejection(refused.error(), "judgement.s7a3.pattern.budget_exceeded",
                        "budget_exceeded", "pattern.maxStateCount");
    }

    SECTION("a measured count above the accepted bound is still a budget overrun") {
        //  The other side of the same dimension: a count this batch DID measure is compared with
        //  the accepted bound and refused when it is above it. Neither the round-2 nor the round-3
        //  correction deleted that comparison.
        const auto measurable =
            judgement::compilePattern(declarationOf(atomNode("atom.one")), PatternCompileBudget{});
        REQUIRE(measurable.has_value());
        REQUIRE(measurable->stateCount() == std::optional<std::uint64_t>{2U});
        PatternCompileBudget narrow;
        narrow.maxStateCount = MeasuredParameter<std::uint64_t>::measured(1U);
        const auto refused = judgement::compilePattern(declarationOf(atomNode("atom.one")), narrow);
        REQUIRE_FALSE(refused.has_value());
        expectRejection(refused.error(), "judgement.s7a3.pattern.budget_exceeded",
                        "budget_exceeded", "pattern.maxStateCount");
        CHECK(contextValue(refused.error(), "category") != "non_terminating_source");
    }
}

TEST_CASE("S7A-3 the state-count lower bound is the longest acceptable trace plus one",
          "[judgement][s7a-3][pattern][budget]") {
    //  The lower bound the state dimension is enforced with is derived from the CONTENT and not
    //  from the construction: for a language whose longest acceptable trace has length `L`, the
    //  walk of a longest accepted trace visits `L + 1` states of the minimal automaton and cannot
    //  revisit one, so the count is at least `L + 1`. This case checks the two sides against each
    //  other wherever both numbers exist, so the bound is not a guess about the implementation.
    const std::vector<PatternDeclaration> measured{
        declarationOf(atomNode("atom.one")),
        declarationOf(
            composeNode(PatternPrimitive::sequence, {atomNode("atom.one"), atomNode("atom.two")})),
        declarationOf(
            composeNode(PatternPrimitive::choice, {atomNode("atom.one"), atomNode("atom.two")})),
        declarationOf(repeatNode(atomNode("atom.one"), 1U, 4U)),
        //  `repeat(skip, 1, max)` accepts the empty word only: its longest acceptable length is 0,
        //  so its lower bound is 1, and the measured count is that same 1. A bound derived from the
        //  declared copy count instead of from the accepted length would wrongly claim 2^64 here.
        declarationOf(repeatNode(composeNode(PatternPrimitive::skip, {}), 1U,
                                 std::numeric_limits<std::uint64_t>::max())),
        declarationOf(nestedChain(16U)),
    };
    for (const PatternDeclaration& pattern : measured) {
        const auto compiled = judgement::compilePattern(pattern);
        REQUIRE(compiled.has_value());
        REQUIRE(compiled->stateCount().has_value());
        REQUIRE(compiled->maximumTraceLength().has_value());
        CHECK(*compiled->stateCount() >= *compiled->maximumTraceLength() + 1U);
    }

    //  A FINITE longest acceptable length that has no `uint64` representation is the other half of
    //  the same bound: the count is then at least `2^64`. The nested maximal repeats below accept
    //  traces as long as `UINT64_MAX x UINT64_MAX` atoms, which is a finite length, so the longest
    //  length is a gap of its own measurement (and not "no finite bound is available"), and the
    //  proven lower bound of the state count has no representable value either.
    const PatternDeclaration nested = declarationOf(
        repeatNode(repeatNode(atomNode("atom.one"), 1U, std::numeric_limits<std::uint64_t>::max()),
                   1U, std::numeric_limits<std::uint64_t>::max()));
    const auto nestedCompiled = judgement::compilePattern(nested);
    REQUIRE(nestedCompiled.has_value());
    CHECK_FALSE(nestedCompiled->stateCount().has_value());
    CHECK_FALSE(nestedCompiled->maximumTraceLength().has_value());
    CHECK(nestedCompiled->minimumTraceLength() == std::optional<std::uint64_t>{1U});

    PatternCompileBudget budget;
    budget.maxStateCount = MeasuredParameter<std::uint64_t>::measured(1U);
    const auto refused = judgement::compilePattern(nested, budget);
    REQUIRE_FALSE(refused.has_value());
    expectRejection(refused.error(), "judgement.s7a3.pattern.budget_exceeded", "budget_exceeded",
                    "pattern.maxStateCount");
    CHECK(contextValue(refused.error(), "category") != "non_terminating_source");
}

TEST_CASE("S7A-3 the reference evaluator working set is measured and checked",
          "[judgement][s7a-3][pattern][budget]") {
    const auto compiled = judgement::compilePattern(declarationOf(atomNode("atom.one")));
    REQUIRE(compiled.has_value());
    //  One interned subpattern and one position set of `traceLength + 1` entries per trace
    //  position, each entry one accounting byte: the working set is the product of the two widths.
    CHECK(compiled->referenceEvaluatorWorkingBytes(0U) == std::optional<std::uint64_t>{1U});
    CHECK(compiled->referenceEvaluatorWorkingBytes(4U) == std::optional<std::uint64_t>{25U});
    CHECK(compiled->referenceEvaluatorWorkingBytes(100U) ==
          std::optional<std::uint64_t>{101U * 101U});
    //  A trace length whose working set has no representable size is reported as absent, never as a
    //  wrapped number, and the absence is not a refusal of anything.
    CHECK_FALSE(compiled->referenceEvaluatorWorkingBytes(std::size_t{1} << 40U).has_value());
    CHECK_FALSE(compiled->referenceEvaluatorWorkingBytes(std::numeric_limits<std::size_t>::max())
                    .has_value());
    //  The compile accounting and the runtime working set are different measurements, so the
    //  compile budget is never read as a promise about the memory a match needs.
    CHECK(compiled->compiledBytes() > 0U);
    CHECK(std::optional<std::uint64_t>{compiled->compiledBytes()} !=
          compiled->referenceEvaluatorWorkingBytes(4U));
}

TEST_CASE("S7A-3 compiled pattern depth is a budget entry and not a constant",
          "[judgement][s7a-3][pattern][budget]") {
    constexpr std::size_t kDeepChainDepth = 1024U;
    const PatternDeclaration deep = declarationOf(nestedChain(kDeepChainDepth));

    SECTION("a pending depth budget refuses nothing, well beyond the removed constant") {
        //  No compiled-in depth constant exists any more. The declaration below nests far deeper
        //  than the former 512 guard and is compiled, measured and reported.
        const auto compiled = judgement::compilePattern(deep, PatternCompileBudget{});
        REQUIRE(compiled.has_value());
        CHECK(compiled->declarationDepth() == kDeepChainDepth);
        //  One interned state for the shared innermost atom and one per distinct chain, so the
        //  interned count follows the distinct subpatterns while the complete expansion counts
        //  every primitive instance of the chain.
        CHECK(compiled->internedSubpatternCount() == 1U + (kDeepChainDepth - 1U));
        CHECK(compiled->fullyExpandedCount() ==
              std::optional<std::uint64_t>{2U * kDeepChainDepth - 1U});
        CHECK(compiled->largestBranchExpansion() == 2U * kDeepChainDepth - 1U);
    }

    SECTION("a measured depth budget is enforced exactly at its boundary") {
        const auto accepted = judgement::compilePattern(deep, depthBudget(kDeepChainDepth));
        CHECK(accepted.has_value());
        const auto refused = judgement::compilePattern(deep, depthBudget(kDeepChainDepth - 1U));
        REQUIRE_FALSE(refused.has_value());
        expectRejection(refused.error(), "judgement.s7a3.pattern.budget_exceeded",
                        "budget_exceeded", "pattern.maxDeclarationDepth");
        //  Nothing about this rejection is non-termination: the source terminates, it only exceeds
        //  a budget a measurement accepted.
        CHECK(contextValue(refused.error(), "category") != "non_terminating_source");
    }

    SECTION("the Spec 8.2 state count of a deep chain is measured too") {
        //  A shallower chain keeps the measurement cheap while still pinning that the count is the
        //  minimised automaton's: one state per prefix of the accepted word, plus none for the atom
        //  that is shared.
        constexpr std::size_t kMeasuredDepth = 16U;
        const auto compiled = judgement::compilePattern(declarationOf(nestedChain(kMeasuredDepth)));
        REQUIRE(compiled.has_value());
        CHECK(compiled->stateCount() == std::optional<std::uint64_t>{kMeasuredDepth + 1U});
    }
}

TEST_CASE("S7A-3 compiled pattern budget dimensions", "[judgement][s7a-3][pattern][budget]") {
    const PatternDeclaration declaration = declarationOf(
        composeNode(PatternPrimitive::sequence, {atomNode("atom.one"), atomNode("atom.two")}));
    const auto baseline = judgement::compilePattern(declaration);
    REQUIRE(baseline.has_value());
    CHECK(baseline->declarationDepth() == 2U);
    CHECK(baseline->fullyExpandedCount() == std::optional<std::uint64_t>{3U});
    CHECK(baseline->largestBranchExpansion() == 3U);
    CHECK(baseline->internedSubpatternCount() == 3U);
    CHECK(baseline->stateCount() == std::optional<std::uint64_t>{3U});
    CHECK(baseline->evaluationSteps() > 0U);
    CHECK(baseline->compiledBytes() > 0U);

    SECTION("every measured dimension is enforced at its value") {
        const auto enforce = [&](const PatternCompileBudget& budget, std::string_view path) {
            const auto compiled = judgement::compilePattern(declaration, budget);
            REQUIRE_FALSE(compiled.has_value());
            expectRejection(compiled.error(), "judgement.s7a3.pattern.budget_exceeded",
                            "budget_exceeded", path);
        };

        PatternCompileBudget depth;
        depth.maxDeclarationDepth = MeasuredParameter<std::uint64_t>::measured(1U);
        enforce(depth, "pattern.maxDeclarationDepth");

        PatternCompileBudget expansion;
        expansion.maxExpansionCount = MeasuredParameter<std::uint64_t>::measured(2U);
        enforce(expansion, "pattern.maxExpansionCount");

        PatternCompileBudget states;
        states.maxStateCount = MeasuredParameter<std::uint64_t>::measured(2U);
        enforce(states, "pattern.maxStateCount");

        PatternCompileBudget steps;
        steps.maxEvaluationSteps = MeasuredParameter<std::uint64_t>::measured(1U);
        enforce(steps, "pattern.maxEvaluationSteps");

        PatternCompileBudget bytes;
        bytes.maxCompiledBytes = MeasuredParameter<std::uint64_t>::measured(16U);
        enforce(bytes, "pattern.maxCompiledBytes");
    }

    SECTION("a budget exactly at the measured value accepts") {
        PatternCompileBudget exact;
        exact.maxDeclarationDepth = MeasuredParameter<std::uint64_t>::measured(2U);
        exact.maxExpansionCount = MeasuredParameter<std::uint64_t>::measured(3U);
        exact.maxStateCount = MeasuredParameter<std::uint64_t>::measured(3U);
        REQUIRE(baseline->evaluationSteps().has_value());
        REQUIRE(baseline->compiledBytes().has_value());
        exact.maxEvaluationSteps =
            MeasuredParameter<std::uint64_t>::measured(*baseline->evaluationSteps());
        exact.maxCompiledBytes =
            MeasuredParameter<std::uint64_t>::measured(*baseline->compiledBytes());
        const auto compiled = judgement::compilePattern(declaration, exact);
        REQUIRE(compiled.has_value());
        CHECK(compiled->stateCount() == std::optional<std::uint64_t>{3U});
    }

    SECTION("a pending dimension is reported and never enforced with a substituted number") {
        //  One measured dimension that is wide, and four pending ones: the compile succeeds and the
        //  measurement is the compiled product's own count.
        PatternCompileBudget oneMeasured = PatternCompileBudget{};
        oneMeasured.maxStateCount = MeasuredParameter<std::uint64_t>::measured(1000U);
        const auto compiled = judgement::compilePattern(declaration, oneMeasured);
        REQUIRE(compiled.has_value());
        CHECK(compiled->fullyExpandedCount() == baseline->fullyExpandedCount());
        CHECK(compiled->evaluationSteps() == baseline->evaluationSteps());
    }
}

TEST_CASE("S7A-3 a count with no representable value is a gap, an overrun, or neither",
          "[judgement][s7a-3][pattern][budget]") {
    //  `repeat(repeat(atom.one, 1, max), 1, max)` nests two maximal repeats. Its complete expansion
    //  count has no `uint64_t` representation, and the count is a PROVEN LOWER BOUND: the
    //  arithmetic is exact checked arithmetic over non-negative counts, so an overflow means the
    //  true count is at least 2^64, which is greater than every representable bound.
    const PatternDeclaration declaration = declarationOf(
        repeatNode(repeatNode(atomNode("atom.one"), 1U, std::numeric_limits<std::uint64_t>::max()),
                   1U, std::numeric_limits<std::uint64_t>::max()));

    SECTION("no accepted bound: the count is a measurement gap and nothing is refused") {
        //  This replaces the previous round's unconditional rejection
        //  (`judgement.s7a3.pattern.expansion_not_representable`). Nothing is refused on behalf of
        //  a count leaving the `uint64` range; the counts are reported as absent measurements.
        const auto compiled = judgement::compilePattern(declaration);
        REQUIRE(compiled.has_value());
        CHECK_FALSE(compiled->fullyExpandedCount().has_value());
        CHECK_FALSE(compiled->largestBranchExpansion().has_value());
        //  The SHORTEST match stays measured: one copy of each repeat is one atom. Only the ends of
        //  the range that really have no representable value are absent.
        CHECK(compiled->minimumTraceLength() == std::optional<std::uint64_t>{1U});
        CHECK_FALSE(compiled->maximumTraceLength().has_value());
        CHECK(compiled->internedSubpatternCount() == 3U);
        //  The language itself is still decided: the reference predicate accepts a run of atoms.
        CHECK(compiled->matches(trace({"atom.one", "atom.one", "atom.one"})));
        CHECK_FALSE(compiled->matches(trace({})));
    }

    SECTION("an accepted bound plus a proven lower bound: a real overrun is refused") {
        //  The count is at least 2^64 and the accepted bound is 1, so the bound IS exceeded. The
        //  rejection is `budget_exceeded` for the dimension an accepted measurement bound, and it
        //  is never a disguised non-termination.
        PatternCompileBudget narrow;
        narrow.maxExpansionCount = MeasuredParameter<std::uint64_t>::measured(1U);
        const auto refused = judgement::compilePattern(declaration, narrow);
        REQUIRE_FALSE(refused.has_value());
        expectRejection(refused.error(), "judgement.s7a3.pattern.budget_exceeded",
                        "budget_exceeded", "pattern.maxExpansionCount");
        CHECK(contextValue(refused.error(), "category") != "non_terminating_source");
    }

    SECTION("an accepted bound is not exceeded by a count that only looks large") {
        //  A count is only a proven lower bound when the arithmetic that produced it is exact. A
        //  repeat that declares zero copies contains no copy of its operand at all, so the
        //  operand's unrepresentable count cannot reach the product: the count of the whole
        //  declaration is exactly 0, and even the narrowest accepted bound accepts it. Propagating
        //  the operand's gap through the zero factor would turn a count of zero into an apparent
        //  overrun, which is the false refusal this batch must not produce.
        const auto compiled = judgement::compilePattern(
            declarationOf(repeatNode(
                repeatNode(atomNode("atom.one"), 1U, std::numeric_limits<std::uint64_t>::max()), 0U,
                0U)),
            PatternCompileBudget{});
        REQUIRE(compiled.has_value());
        CHECK(compiled->fullyExpandedCount() == std::optional<std::uint64_t>{0U});
        CHECK(compiled->largestBranchExpansion() == std::optional<std::uint64_t>{0U});
        CHECK(compiled->minimumTraceLength() == std::optional<std::uint64_t>{0U});
        CHECK(compiled->maximumTraceLength() == std::optional<std::uint64_t>{0U});
        CHECK(compiled->matches(trace({})));
        CHECK_FALSE(compiled->matches(trace({"atom.one"})));

        //  A zero bound is a literal upper bound (BUDGET 3.2: 0 never means "no limit"), and this
        //  declaration is exactly at it: the complete expansion is zero primitive instances.
        PatternCompileBudget zeroBound;
        zeroBound.maxExpansionCount = MeasuredParameter<std::uint64_t>::measured(0U);
        CHECK(judgement::compilePattern(
                  declarationOf(repeatNode(repeatNode(atomNode("atom.one"), 1U,
                                                      std::numeric_limits<std::uint64_t>::max()),
                                           0U, 0U)),
                  zeroBound)
                  .has_value());

        //  The state dimension reads the same declaration the same way. Its proven lower bound is
        //  the longest acceptable length plus one, and a repeat of ZERO copies has a longest
        //  acceptable length of exactly 0 whatever its operand is: the operand's unrepresentable
        //  longest length is multiplied away instead of being carried into an apparent state-count
        //  overrun. The lower bound is therefore 1, and an accepted state bound of 1 accepts --
        //  with no claim that the count itself was measured, which is the incomplete state-budget
        //  gate.
        PatternCompileBudget stateBound;
        stateBound.maxStateCount = MeasuredParameter<std::uint64_t>::measured(1U);
        const auto zeroCopies = judgement::compilePattern(
            declarationOf(repeatNode(
                repeatNode(atomNode("atom.one"), 1U, std::numeric_limits<std::uint64_t>::max()), 0U,
                0U)),
            stateBound);
        REQUIRE(zeroCopies.has_value());
        CHECK(zeroCopies->maximumTraceLength() == std::optional<std::uint64_t>{0U});
    }
}

TEST_CASE("S7A-3 compiled pattern containment in the arm and deadline surface",
          "[judgement][s7a-3][pattern][containment]") {
    const PatternDeclaration declaration = declarationOf(
        composeNode(PatternPrimitive::sequence, {atomNode("atom.one"), atomNode("atom.two")}));
    const auto compiled = judgement::compilePattern(declaration);
    REQUIRE(compiled.has_value());

    SECTION("an empty declared arm set makes the membership check not applicable") {
        CHECK(judgement::checkPatternContainment(*compiled, PatternArmBound{}).has_value());
    }

    SECTION("an atom outside the declared arms is an undeclared relation") {
        PatternArmBound bound;
        bound.declaredActionRefs = {"atom.one"};
        const auto checked = judgement::checkPatternContainment(*compiled, bound);
        REQUIRE_FALSE(checked.has_value());
        expectRejection(checked.error(), "judgement.s7a3.pattern.atom_outside_declared_arms",
                        "invalid_relation", "declaredActionRefs");

        bound.declaredActionRefs = {"atom.two", "atom.one"};
        CHECK(judgement::checkPatternContainment(*compiled, bound).has_value());
    }

    SECTION("a pattern that cannot fit the declared capacities is not contained") {
        PatternArmBound arm;
        arm.maxArmElements = MeasuredParameter<std::uint64_t>::measured(1U);
        arm.maxDeadlineElements = MeasuredParameter<std::uint64_t>::measured(2U);
        const auto armChecked = judgement::checkPatternContainment(*compiled, arm);
        REQUIRE_FALSE(armChecked.has_value());
        expectRejection(armChecked.error(), "judgement.s7a3.pattern.arm_bound_exceeded",
                        "budget_exceeded", "pattern.maxArmElements");

        PatternArmBound wideArm;
        wideArm.maxArmElements = MeasuredParameter<std::uint64_t>::measured(2U);
        CHECK(judgement::checkPatternContainment(*compiled, wideArm).has_value());

        PatternArmBound deadline;
        deadline.maxArmElements = MeasuredParameter<std::uint64_t>::measured(2U);
        deadline.maxDeadlineElements = MeasuredParameter<std::uint64_t>::measured(1U);
        const auto deadlineChecked = judgement::checkPatternContainment(*compiled, deadline);
        REQUIRE_FALSE(deadlineChecked.has_value());
        expectRejection(deadlineChecked.error(), "judgement.s7a3.pattern.arm_bound_exceeded",
                        "budget_exceeded", "pattern.maxDeadlineElements");

        PatternArmBound wideDeadline;
        wideDeadline.maxDeadlineElements = MeasuredParameter<std::uint64_t>::measured(2U);
        CHECK(judgement::checkPatternContainment(*compiled, wideDeadline).has_value());
    }

    SECTION("every acceptable length has to fit, not only the shortest one") {
        //  The counterexample: the pattern accepts one or three elements, so its shortest match
        //  fits a capacity of two and its longest one does not. A check that reads only the
        //  shortest match would accept it, which is exactly the fault this case locks out.
        const auto wide = judgement::compilePattern(declarationOf(composeNode(
            PatternPrimitive::choice,
            {atomNode("atom.one"),
             composeNode(PatternPrimitive::sequence,
                         {atomNode("atom.one"), atomNode("atom.one"), atomNode("atom.one")})})));
        REQUIRE(wide.has_value());
        CHECK(wide->minimumTraceLength() == 1U);
        CHECK(wide->maximumTraceLength() == std::optional<std::uint64_t>{3U});

        PatternArmBound arm;
        arm.maxArmElements = MeasuredParameter<std::uint64_t>::measured(2U);
        arm.maxDeadlineElements = MeasuredParameter<std::uint64_t>::measured(3U);
        const auto armChecked = judgement::checkPatternContainment(*wide, arm);
        REQUIRE_FALSE(armChecked.has_value());
        expectRejection(armChecked.error(), "judgement.s7a3.pattern.arm_bound_exceeded",
                        "budget_exceeded", "pattern.maxArmElements");

        PatternArmBound deadline;
        deadline.maxArmElements = MeasuredParameter<std::uint64_t>::measured(3U);
        deadline.maxDeadlineElements = MeasuredParameter<std::uint64_t>::measured(2U);
        const auto deadlineChecked = judgement::checkPatternContainment(*wide, deadline);
        REQUIRE_FALSE(deadlineChecked.has_value());
        expectRejection(deadlineChecked.error(), "judgement.s7a3.pattern.arm_bound_exceeded",
                        "budget_exceeded", "pattern.maxDeadlineElements");

        PatternArmBound fitting;
        fitting.maxArmElements = MeasuredParameter<std::uint64_t>::measured(3U);
        fitting.maxDeadlineElements = MeasuredParameter<std::uint64_t>::measured(3U);
        CHECK(judgement::checkPatternContainment(*wide, fitting).has_value());
    }

    SECTION("a pattern with no finite longest match is not contained in a finite arm or deadline") {
        const auto unbounded = judgement::compilePattern(
            declarationOf(composeNode(PatternPrimitive::complement, {atomNode("atom.one")})));
        REQUIRE(unbounded.has_value());
        PatternArmBound deadline;
        deadline.maxArmElements =
            MeasuredParameter<std::uint64_t>::measured(std::numeric_limits<std::uint64_t>::max());
        deadline.maxDeadlineElements =
            MeasuredParameter<std::uint64_t>::measured(std::numeric_limits<std::uint64_t>::max());
        const auto checked = judgement::checkPatternContainment(*unbounded, deadline);
        REQUIRE(checked.has_value());
        CHECK(checked.value() == judgement::ContainmentStatus::gateIncomplete);

        //  The same holds on the arm side: no finite arm capacity contains a pattern with no finite
        //  longest match, and the check refuses instead of reading only the shortest match.
        PatternArmBound arm;
        arm.maxArmElements =
            MeasuredParameter<std::uint64_t>::measured(std::numeric_limits<std::uint64_t>::max());
        arm.maxDeadlineElements =
            MeasuredParameter<std::uint64_t>::measured(std::numeric_limits<std::uint64_t>::max());
        const auto armChecked = judgement::checkPatternContainment(*unbounded, arm);
        REQUIRE(armChecked.has_value());
        CHECK(armChecked.value() == judgement::ContainmentStatus::gateIncomplete);
    }

    SECTION("a pending capacity is not enforced") {
        PatternArmBound pending;
        pending.maxArmElements = MeasuredParameter<std::uint64_t>::pendingMeasurement();
        pending.maxDeadlineElements = MeasuredParameter<std::uint64_t>::pendingMeasurement();
        const auto checked = judgement::checkPatternContainment(*compiled, pending);
        REQUIRE(checked.has_value());
        CHECK(checked.value() == judgement::ContainmentStatus::gateIncomplete);
    }
}

TEST_CASE("S7A-3 compiled pattern declaration faults are stable rejections",
          "[judgement][s7a-3][pattern]") {
    SECTION("an incomplete atom") {
        PatternNodeDeclaration node = atomNode("");
        const auto compiled = judgement::compilePattern(declarationOf(node));
        REQUIRE_FALSE(compiled.has_value());
        expectRejection(compiled.error(), "judgement.s7a3.declaration.structurally_incomplete",
                        "invalid_relation", "atomRef");
    }

    SECTION("a bounded repeat without bounds") {
        PatternNodeDeclaration node =
            composeNode(PatternPrimitive::boundedRepeat, {atomNode("atom.one")});
        const auto compiled = judgement::compilePattern(declarationOf(node));
        REQUIRE_FALSE(compiled.has_value());
        expectRejection(compiled.error(), "judgement.s7a3.declaration.structurally_incomplete",
                        "invalid_relation", "repeatBounds");
    }

    SECTION("a reversed repeat range") {
        PatternNodeDeclaration node = repeatNode(atomNode("atom.one"), 4U, 2U);
        const auto compiled = judgement::compilePattern(declarationOf(node));
        REQUIRE_FALSE(compiled.has_value());
        expectRejection(compiled.error(), "judgement.s7a3.domain.range_reversed", "budget_exceeded",
                        "repeatBounds");
    }
}

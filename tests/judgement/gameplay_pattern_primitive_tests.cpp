#include "gameplay_pattern_compile_fixture.hpp"

// Primitive coverage, bounded repeat, relation forms and state count cases.

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
        expectRejection(unexpandedChecked.error(), "judgement.content.unsupported_form",
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

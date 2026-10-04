//  S7A-3 second half (first part) compiled Measure tests: plan S7A-3 item 3.
//
//  The cases pin down what Stage 7A fixes about the measure layer and what it refuses to invent:
//
//    1. several phase / category components, with the grading content carried per component. The
//       compiled measure stores the declared grade tokens as opaque content: this batch carries and
//       publishes what the declaration said, and it does not evaluate a grade;
//    2. the category is *derived* from the phase (Spec 3.20 rule 3) and a caller-assigned category
//       that contradicts its phase is refused instead of accepted as a second name for one fact;
//    3. Hold head / body plus the explicitly declared Release / tail. A tail is an optional phase
//    of
//       the same requirement: it is never inferred from an implicit legacy profile, and no second
//       requirement is generated for it;
//    4. `Outcome` is exactly the frozen two-value set, and a phase-local outcome does not add a
//       second result dimension;
//    5. the grade table is optional: a component whose content declared no table stays `absent` and
//       is never upgraded to a default, because the grade scale and the aggregation rule are CM-S10
//       / P1-07 and no default of them has been accepted. Absence is carried as absence and is
//       never turned into an evaluated grade or a substituted token;
//    6. failure is atomic: an assembly refused by the measure layer publishes nothing and leaves an
//       already published value exactly as it was.

#include "gameplay_test_fixture.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cuexis/core/error.hpp>
#include <cuexis/judgement/gameplay_assembler.hpp>
#include <cuexis/judgement/gameplay_graph.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

namespace judgement = cuexis::judgement;
namespace testing = cuexis::judgement::testing;

using judgement::CompiledMeasure;
using judgement::CompiledMeasureComponent;
using judgement::FactCategory;
using judgement::GameplayPublication;
using judgement::GradePresence;
using judgement::MeasureComponentDeclaration;
using judgement::MeasurePhaseContext;
using judgement::MeasureSpecDeclaration;
using judgement::Outcome;
using judgement::PhaseDeclaration;
using judgement::PhaseKind;

using testing::contextValue;
using testing::Fixture;

//  Asserts the three parts of a stable rejection this suite reads.
void expectRejection(const cuexis::core::Error& error, std::string_view code,
                     std::string_view category, std::string_view path) {
    CHECK(error.code() == code);
    CHECK(contextValue(error, "category") == category);
    CHECK(contextValue(error, "severity") == "error");
    CHECK(contextValue(error, "faulted") == "false");
    CHECK(contextValue(error, "field.path") == path);
}

[[nodiscard]] auto component(PhaseKind phase, std::string categoryToken,
                             std::vector<std::string> gradeTokens = {})
    -> MeasureComponentDeclaration {
    return MeasureComponentDeclaration{.phase = phase,
                                       .categoryToken = std::move(categoryToken),
                                       .declaredGradeTokens = std::move(gradeTokens)};
}

[[nodiscard]] auto declarationOf(std::vector<MeasureComponentDeclaration> components)
    -> MeasureSpecDeclaration {
    return MeasureSpecDeclaration{.components = std::move(components),
                                  .required = judgement::RequiredRefs{}};
}

[[nodiscard]] auto phasesOf(std::initializer_list<PhaseKind> kinds)
    -> std::vector<PhaseDeclaration> {
    std::vector<PhaseDeclaration> phases;
    std::uint32_t ordinal = 1U;
    for (const PhaseKind kind : kinds) {
        phases.push_back(PhaseDeclaration{.kind = kind, .declarationOrdinal = ordinal});
        ++ordinal;
    }
    return phases;
}

} // namespace

TEST_CASE("S7A-3 compiled measure covers several phase and category components",
          "[judgement][s7a-3][measure]") {
    const auto compiled = judgement::compileMeasure(declarationOf({
        component(PhaseKind::head, "hold_head", {"grade.head.one", "grade.head.two"}),
        component(PhaseKind::body, "hold_body"),
        component(PhaseKind::tail, "hold_tail", {"grade.tail.one"}),
    }));
    REQUIRE(compiled.has_value());
    CHECK(compiled->componentCount() == 3U);

    //  The components are reported in canonical (phase, category) order, so the declaration order
    //  of the table carries no semantics.
    const std::vector<CompiledMeasureComponent> components = compiled->components();
    REQUIRE(components.size() == 3U);
    CHECK(components[0].phase == PhaseKind::head);
    CHECK(components[0].category == FactCategory::holdHead);
    CHECK(components[1].phase == PhaseKind::body);
    CHECK(components[1].category == FactCategory::holdBody);
    CHECK(components[2].phase == PhaseKind::tail);
    CHECK(components[2].category == FactCategory::holdTail);

    //  The grading content travels per component: the declared table is present exactly where the
    //  content declared one, and it is not copied from a sibling component. The tokens are carried
    //  as declared content; nothing here evaluates a grade from them.
    CHECK(components[0].grade == GradePresence::declared);
    CHECK(components[0].declaredGradeTokens ==
          std::vector<std::string>({"grade.head.one", "grade.head.two"}));
    CHECK(components[1].grade == GradePresence::absent);
    CHECK(components[1].declaredGradeTokens.empty());
    CHECK(components[2].grade == GradePresence::declared);
    CHECK(components[2].declaredGradeTokens == std::vector<std::string>({"grade.tail.one"}));

    const auto body = compiled->gradeOf(PhaseKind::body);
    REQUIRE(body.has_value());
    CHECK(body->grade == GradePresence::absent);
    CHECK_FALSE(compiled->gradeOf(PhaseKind::tap).has_value());
}

TEST_CASE("S7A-3 fact category is derived from the phase", "[judgement][s7a-3][measure]") {
    //  The one-to-one map of Spec 3.20 rule 3, with the frozen spelling of each category.
    CHECK(judgement::factCategoryOfPhase(PhaseKind::tap) == FactCategory::tap);
    CHECK(judgement::factCategoryOfPhase(PhaseKind::head) == FactCategory::holdHead);
    CHECK(judgement::factCategoryOfPhase(PhaseKind::body) == FactCategory::holdBody);
    CHECK(judgement::factCategoryOfPhase(PhaseKind::tail) == FactCategory::holdTail);
    CHECK(judgement::factCategoryToken(FactCategory::tap) == "tap");
    CHECK(judgement::factCategoryToken(FactCategory::holdHead) == "hold_head");
    CHECK(judgement::factCategoryToken(FactCategory::holdBody) == "hold_body");
    CHECK(judgement::factCategoryToken(FactCategory::holdTail) == "hold_tail");
    CHECK(judgement::kStage7AFactCategories.size() == 4U);

    //  The four categories are distinct, so the derivation is one to one.
    for (std::size_t left = 0; left < judgement::kStage7AFactCategories.size(); ++left) {
        for (std::size_t right = left + 1U; right < judgement::kStage7AFactCategories.size();
             ++right) {
            CHECK(judgement::kStage7AFactCategories[left] !=
                  judgement::kStage7AFactCategories[right]);
        }
    }

    //  An empty declared token is the "not assigned" state: the category is derived from the phase.
    const auto derived = judgement::compileMeasure(
        declarationOf({component(PhaseKind::tap, ""), component(PhaseKind::head, "")}));
    REQUIRE(derived.has_value());
    const auto tap = derived->gradeOf(PhaseKind::tap);
    const auto head = derived->gradeOf(PhaseKind::head);
    REQUIRE(tap.has_value());
    REQUIRE(head.has_value());
    CHECK(tap->category == FactCategory::tap);
    CHECK(head->category == FactCategory::holdHead);

    //  A caller-assigned category that contradicts its phase is refused, never accepted as a second
    //  name for the same fact.
    const auto assigned =
        judgement::compileMeasure(declarationOf({component(PhaseKind::head, "category.one")}));
    REQUIRE_FALSE(assigned.has_value());
    expectRejection(assigned.error(), "judgement.s7a3.measure.category_not_derived_from_phase",
                    "invalid_relation", "measure.categoryToken");
}

TEST_CASE("S7A-3 outcome is the frozen two-value set", "[judgement][s7a-3][measure]") {
    CHECK(judgement::kStage7AOutcomes.size() == 2U);
    CHECK(judgement::kStage7AOutcomes[0] == Outcome::hit);
    CHECK(judgement::kStage7AOutcomes[1] == Outcome::miss);
    CHECK(Outcome::hit != Outcome::miss);
}

TEST_CASE("S7A-3 Release tail is an explicit phase of the same requirement",
          "[judgement][s7a-3][measure][tail]") {
    const MeasurePhaseContext headBodyContext{.declaredPhases =
                                                  phasesOf({PhaseKind::head, PhaseKind::body}),
                                              .requiresReleaseTailSemantics = false,
                                              .fieldPathPrefix = "requirements[0]"};
    const MeasureSpecDeclaration headBody = declarationOf(
        {component(PhaseKind::head, "hold_head"), component(PhaseKind::body, "hold_body")});

    SECTION("content that needs no tail is compiled without one") {
        const auto compiled = judgement::compileMeasure(headBody, headBodyContext);
        REQUIRE(compiled.has_value());
        CHECK(compiled->componentCount() == 2U);
        CHECK_FALSE(compiled->gradeOf(PhaseKind::tail).has_value());
    }

    SECTION("content whose semantics require a tail but declares none is refused") {
        MeasurePhaseContext context = headBodyContext;
        context.requiresReleaseTailSemantics = true;
        const auto compiled = judgement::compileMeasure(headBody, context);
        REQUIRE_FALSE(compiled.has_value());
        expectRejection(compiled.error(), "judgement.s7a3.requirement.release_tail_implicit",
                        "invalid_relation", "requirements[0].phases");
    }

    SECTION("the declared tail phase is what makes the same content acceptable") {
        MeasurePhaseContext context = headBodyContext;
        context.requiresReleaseTailSemantics = true;
        context.declaredPhases = phasesOf({PhaseKind::head, PhaseKind::body, PhaseKind::tail});
        const auto compiled = judgement::compileMeasure(
            declarationOf({component(PhaseKind::head, "hold_head"),
                           component(PhaseKind::body, "hold_body"),
                           component(PhaseKind::tail, "hold_tail", {"grade.tail.one"})}),
            context);
        REQUIRE(compiled.has_value());
        //  The tail is a component of this measure, not a second requirement and not an inferred
        //  phase: exactly the declared components are reported.
        CHECK(compiled->componentCount() == 3U);
        const auto tail = compiled->gradeOf(PhaseKind::tail);
        REQUIRE(tail.has_value());
        CHECK(tail->category == FactCategory::holdTail);
        CHECK(tail->grade == GradePresence::declared);
    }
}

TEST_CASE("S7A-3 measure component must belong to a declared phase",
          "[judgement][s7a-3][measure]") {
    const MeasurePhaseContext context{.declaredPhases = phasesOf({PhaseKind::head}),
                                      .requiresReleaseTailSemantics = false,
                                      .fieldPathPrefix = "requirements[0]"};
    const auto compiled = judgement::compileMeasure(
        declarationOf({component(PhaseKind::body, "hold_body")}), context);
    REQUIRE_FALSE(compiled.has_value());
    expectRejection(compiled.error(), "judgement.s7a3.declaration.structurally_incomplete",
                    "invalid_relation", "requirements[0].measure");

    //  The same declaration is complete on its own, which is what the context-free entry point
    //  checks: without a requirement-side phase surface there is nothing to be contained in.
    CHECK(judgement::compileMeasure(declarationOf({component(PhaseKind::body, "hold_body")}))
              .has_value());
}

TEST_CASE("S7A-3 measure component table has no duplicates", "[judgement][s7a-3][measure]") {
    const auto compiled = judgement::compileMeasure(declarationOf(
        {component(PhaseKind::head, "hold_head"), component(PhaseKind::head, "hold_head")}));
    REQUIRE_FALSE(compiled.has_value());
    expectRejection(compiled.error(), "judgement.s7a3.declaration.duplicate_entry",
                    "invalid_relation", "measure");

    //  The check reads the canonical order, so a shuffled duplicate is the same fault.
    const auto shuffled = judgement::compileMeasure(declarationOf(
        {component(PhaseKind::body, "hold_body"), component(PhaseKind::head, "hold_head"),
         component(PhaseKind::body, "hold_body")}));
    REQUIRE_FALSE(shuffled.has_value());
    expectRejection(shuffled.error(), "judgement.s7a3.declaration.duplicate_entry",
                    "invalid_relation", "measure");
}

TEST_CASE("S7A-3 an absent grade table is absent and is never upgraded",
          "[judgement][s7a-3][measure][grade]") {
    const auto compiled = judgement::compileMeasure(
        declarationOf({component(PhaseKind::head, "hold_head"), component(PhaseKind::body, "")}));
    REQUIRE(compiled.has_value());
    const auto head = compiled->gradeOf(PhaseKind::head);
    const auto body = compiled->gradeOf(PhaseKind::body);
    REQUIRE(head.has_value());
    REQUIRE(body.has_value());
    //  No default grade token, no implicit upgrade and no schema-wide fallback: the absence is the
    //  value.
    CHECK(head->grade == GradePresence::absent);
    CHECK(head->declaredGradeTokens.empty());
    CHECK(body->grade == GradePresence::absent);
    CHECK(body->declaredGradeTokens.empty());
}

TEST_CASE("S7A-3 measure failure is atomic", "[judgement][s7a-3][measure][atomic]") {
    const Fixture fixture;
    GameplayPublication publication;
    CHECK_FALSE(publication.hasActive());

    const auto accepted = judgement::assembleInto(publication, fixture.request());
    REQUIRE(accepted.has_value());
    REQUIRE(publication.hasActive());
    const std::string chartHex = publication.active()->chart.canonicalBytes().toHex();
    const std::string contentHex = publication.active()->content.canonicalBytes().toHex();

    //  A measure component whose declared category contradicts its phase is refused by the compiled
    //  measure inside the assembly path, and the refusal publishes nothing.
    judgement::AssemblyRequest refused = fixture.request();
    refused.sources.front().document.requirements.front().measure.components.front().categoryToken =
        "category.one";
    const auto rejected = judgement::assembleInto(publication, refused);
    REQUIRE_FALSE(rejected.has_value());
    expectRejection(rejected.error(), "judgement.s7a3.measure.category_not_derived_from_phase",
                    "invalid_relation", "requirements[0].measure.categoryToken");
    REQUIRE(publication.hasActive());
    CHECK(publication.active()->chart.canonicalBytes().toHex() == chartHex);
    CHECK(publication.active()->content.canonicalBytes().toHex() == contentHex);

    //  A declared grade table is measure content, so it is part of the published content identity:
    //  changing it changes the identity and nothing else.
    judgement::AssemblyRequest graded = fixture.request();
    graded.sources.front()
        .document.requirements.front()
        .measure.components.front()
        .declaredGradeTokens = {"grade.body.one"};
    CHECK(judgement::assembleInto(publication, graded).has_value());
    CHECK(publication.active()->chart.canonicalBytes().toHex() != chartHex);
    CHECK(publication.active()->content.canonicalBytes().toHex() != contentHex);
}

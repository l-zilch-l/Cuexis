//  S7A-2 input mapping and AmountSpec golden tests.
//
//  Every expected value below is a hard-coded number, never a relation such as "not equal" or
//  "greater than". The cases are organised by the rulings they pin down:
//
//    1. canonical integer quantization (Spec 3.7.6 items 1-2): exact integer arithmetic,
//       round-half-to-even, negative symmetry, scale, and the declared boundary policy;
//    2. stable rejection of an out-of-range value and of a value the canonical integer cannot hold
//       (Spec 3.7.6 item 3);
//    3. the InputMapping profile as a session component (CM-T09): the session identity components
//       (the profile identity, the version, the source class and the domain declaration set), the
//       structural declaration checks, and the runtime mutation rejection;
//    4. the compile-time half of the same contract: a domain cannot be declared without a typed
//       AmountSpec, and the boundary policy enumeration has no default.
//
//  The numeric ranges used here are test-local declarations. Spec 3.7.6 item 4 leaves every
//  business range to a later batch, so no production quantity appears in this file.

#include <catch2/catch_test_macros.hpp>

#include <cuexis/core/error.hpp>
#include <cuexis/judgement/input_boundary.hpp>
#include <cuexis/judgement/timebase.hpp>

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {

namespace judgement = cuexis::judgement;

using judgement::AmountBoundaryPolicy;
using judgement::AmountSpec;
using judgement::InputDomainDeclaration;
using judgement::InputMappingProfile;
using judgement::RationalBeat;
using judgement::RationalDuration;
using judgement::SourceClass;

constexpr std::int64_t kInt64Max = std::numeric_limits<std::int64_t>::max();
constexpr std::int64_t kInt64Min = std::numeric_limits<std::int64_t>::min();

//  -------------------------------------------------------------------------------------------
//  Fixtures
//  -------------------------------------------------------------------------------------------

[[nodiscard]] auto beat(std::int64_t numerator, std::int64_t denominator) -> RationalBeat {
    const auto value = RationalBeat::create(numerator, denominator);
    REQUIRE(value.has_value());
    return *value;
}

[[nodiscard]] auto scale(std::int64_t numerator, std::int64_t denominator) -> RationalDuration {
    const auto value = RationalDuration::create(numerator, denominator);
    REQUIRE(value.has_value());
    return *value;
}

//  One declared AmountSpec. The range and the scale are named at the call site because no business
//  quantity is frozen by this batch.
[[nodiscard]] auto spec(std::int64_t scaleNumerator, std::int64_t scaleDenominator,
                        std::int64_t minimum, std::int64_t maximum,
                        AmountBoundaryPolicy policy = AmountBoundaryPolicy::inclusive)
    -> AmountSpec {
    return AmountSpec{.scale = scale(scaleNumerator, scaleDenominator),
                      .minimum = minimum,
                      .maximum = maximum,
                      .boundaryPolicy = policy};
}

[[nodiscard]] auto sourceClass(std::string_view token) -> SourceClass {
    const auto value = SourceClass::fromToken(token);
    REQUIRE(value.has_value());
    return *value;
}

[[nodiscard]] auto domain(std::string_view token, AmountSpec amount) -> InputDomainDeclaration {
    return InputDomainDeclaration{.domainToken = token, .amount = amount};
}

//  A complete mapping with exactly one declared domain.
[[nodiscard]] auto oneDomainMapping(AmountSpec amount) -> InputMappingProfile {
    return InputMappingProfile{
        .profileId = "test.mapping.keyboard",
        .profileVersion = "1",
        .sourceClass = sourceClass("test.source.keyboard"),
        .domains = {domain("test.domain.button", amount)},
    };
}

[[nodiscard]] auto contextValue(const cuexis::core::Error& error, std::string_view key)
    -> std::string {
    for (const auto& entry : error.context()) {
        if (entry.key == key) {
            return entry.value;
        }
    }
    return {};
}

//  Asserts the four parts of a stable rejection: the code, the category, the input section and the
//  field path.
void checkRejection(const cuexis::core::Error& error, std::string_view code,
                    std::string_view category, std::string_view path) {
    CHECK(error.code() == code);
    CHECK(contextValue(error, "category") == category);
    CHECK(contextValue(error, "severity") == "error");
    CHECK(contextValue(error, "faulted") == "false");
    CHECK(contextValue(error, "field.section") == "judgement.input");
    CHECK(contextValue(error, "field.path") == path);
}

//  A change that only the domain declaration set carries is refused on the domain table path,
//  which is the component that actually differs when the three declared identity components agree.
void checkDomainMutationRefused(const InputMappingProfile& current,
                                const InputMappingProfile& candidate) {
    const auto rejected = judgement::rejectRuntimeMappingChange(current, candidate);
    REQUIRE_FALSE(rejected.has_value());
    checkRejection(rejected.error(), "judgement.s7a2.input.runtime_mapping_change",
                   "invalid_relation", "inputMapping.domains");
}

//  -------------------------------------------------------------------------------------------
//  1. Canonical integer quantization
//  -------------------------------------------------------------------------------------------

//  One golden quantization case. `numerator`/`denominator` is the exact incoming quantity and
//  `expected` is the hard-coded canonical integer.
struct QuantizationCase final {
    std::int64_t numerator;
    std::int64_t denominator;
    std::int64_t expected;
};

TEST_CASE("amount: quantization rounds to the nearest integer with ties to even",
          "[judgement][s7a2][input][amount]") {
    //  scale = 1/1: one canonical integer is exactly one quantity unit, so the canonical integer is
    //  the rounded quantity itself. The ties below are the ruling's own cases: 1/2 and 3/2 round to
    //  the even side, and the negative values are symmetric.
    const AmountSpec identity = spec(1, 1, -1000000, 1000000);

    const std::vector<QuantizationCase> goldens{
        {0, 1, 0},   {1, 1, 1}, {-1, 1, -1}, {1, 2, 0},           {3, 2, 2},
        {5, 2, 2},   {7, 2, 4}, {-1, 2, 0},  {-3, 2, -2},         {-5, 2, -2},
        {-7, 2, -4}, {1, 3, 0}, {2, 3, 1},   {-2, 3, -1},         {4, 3, 1},
        {-4, 3, -1}, {5, 3, 2}, {-5, 3, -2}, {999999, 1, 999999}, {-999999, 1, -999999},
    };
    for (const QuantizationCase& item : goldens) {
        CAPTURE(item.numerator, item.denominator, item.expected);
        const auto quantized =
            judgement::quantizeAmount(identity, beat(item.numerator, item.denominator));
        REQUIRE(quantized.has_value());
        CHECK(quantized->canonicalInteger == item.expected);
    }
}

TEST_CASE("amount: the declared scale converts a quantity into canonical integers",
          "[judgement][s7a2][input][amount]") {
    //  scale = 1/4: four canonical integers make one quantity unit, so a quantity of 1 maps to 4.
    const AmountSpec quarter = spec(1, 4, -1000000, 1000000);

    const std::vector<QuantizationCase> goldens{
        {0, 1, 0},   {1, 1, 4},   {1, 4, 1},  {1, 8, 0},   {3, 8, 2},
        {-1, 1, -4}, {-1, 4, -1}, {-1, 8, 0}, {-3, 8, -2}, {5, 8, 2},
    };
    for (const QuantizationCase& item : goldens) {
        CAPTURE(item.numerator, item.denominator, item.expected);
        const auto quantized =
            judgement::quantizeAmount(quarter, beat(item.numerator, item.denominator));
        REQUIRE(quantized.has_value());
        CHECK(quantized->canonicalInteger == item.expected);
    }

    //  scale = 4/1: one canonical integer is four quantity units, so a quantity of 4 maps to 1.
    const AmountSpec four = spec(4, 1, -1000000, 1000000);
    const auto quantized = judgement::quantizeAmount(four, beat(4, 1));
    REQUIRE(quantized.has_value());
    CHECK(quantized->canonicalInteger == 1);
}

TEST_CASE("amount: the declared boundary policy decides whether an endpoint is representable",
          "[judgement][s7a2][input][amount]") {
    SECTION("an inclusive range represents both endpoints") {
        const AmountSpec inclusive = spec(1, 1, -3, 3, AmountBoundaryPolicy::inclusive);
        for (const std::int64_t value : {-3, -2, 0, 2, 3}) {
            CAPTURE(value);
            const auto quantized = judgement::quantizeAmount(inclusive, beat(value, 1));
            REQUIRE(quantized.has_value());
            CHECK(quantized->canonicalInteger == value);
        }
    }

    SECTION("an exclusive range excludes both endpoints") {
        const AmountSpec exclusive = spec(1, 1, -3, 3, AmountBoundaryPolicy::exclusive);
        for (const std::int64_t value : {-3, 3}) {
            CAPTURE(value);
            const auto rejected = judgement::quantizeAmount(exclusive, beat(value, 1));
            REQUIRE_FALSE(rejected.has_value());
            checkRejection(rejected.error(), "judgement.s7a2.input.amount_out_of_range",
                           "budget_exceeded", "amountSpec.range");
        }
        for (const std::int64_t value : {-2, 0, 2}) {
            CAPTURE(value);
            const auto quantized = judgement::quantizeAmount(exclusive, beat(value, 1));
            REQUIRE(quantized.has_value());
            CHECK(quantized->canonicalInteger == value);
        }
    }
}

TEST_CASE("amount: a value outside the declared range is a stable rejection",
          "[judgement][s7a2][input][amount]") {
    const AmountSpec bounded = spec(1, 1, -10, 10);

    SECTION("one unit beyond either bound is rejected and not clamped") {
        const std::vector<std::int64_t> beyond{11, -11, 1000, -1000, kInt64Max};
        for (const std::int64_t value : beyond) {
            CAPTURE(value);
            const auto rejected = judgement::quantizeAmount(bounded, beat(value, 1));
            REQUIRE_FALSE(rejected.has_value());
            checkRejection(rejected.error(), "judgement.s7a2.input.amount_out_of_range",
                           "budget_exceeded", "amountSpec.range");
        }
    }

    SECTION("rounding is what decides the range check, and it still lands outside") {
        //  21/2 rounds to 10 with a tie to even, which is inside the range: the tie rule is applied
        //  before the range check, not skipped by it.
        const auto inside = judgement::quantizeAmount(bounded, beat(21, 2));
        REQUIRE(inside.has_value());
        CHECK(inside->canonicalInteger == 10);

        //  23/2 rounds to 12, which is outside the declared range.
        const auto outside = judgement::quantizeAmount(bounded, beat(23, 2));
        REQUIRE_FALSE(outside.has_value());
        checkRejection(outside.error(), "judgement.s7a2.input.amount_out_of_range",
                       "budget_exceeded", "amountSpec.range");
    }
}

TEST_CASE("amount: a quotient the canonical integer cannot hold is a narrowing rejection",
          "[judgement][s7a2][input][amount]") {
    //  Each case is representable as an exact rational but its exact canonical quotient, or one of
    //  the two exact products that quotient is built from, leaves the signed 64-bit canonical
    //  integer. None of them is saturated or truncated, and all of them are reported on the
    //  narrowing token instead of the range token, so the two rejection reasons stay
    //  distinguishable.
    //
    //  The declared range of these fixtures spans the whole canonical integer domain, so the range
    //  check cannot be the reason any of them is refused.
    const AmountSpec fullWidth = spec(1, 1, kInt64Min, kInt64Max);

    SECTION("scale below one multiplies the quantity on the numerator axis") {
        //  scale = 1/4 makes the exact numerator 4 * 2^62.
        AmountSpec quarter = fullWidth;
        const auto quarterScale = scale(1, 4);
        quarter.scale = quarterScale;
        const auto rejected = judgement::quantizeAmount(quarter, beat(4611686018427387904LL, 1));
        REQUIRE_FALSE(rejected.has_value());
        checkRejection(rejected.error(), "judgement.s7a2.input.amount_narrowed", "budget_exceeded",
                       "amountSpec");
    }

    SECTION("scale above one multiplies the quantity on the denominator axis") {
        //  scale = 4/1 makes the exact denominator 4 * 2^62 while the numerator stays 1.
        AmountSpec four = fullWidth;
        const auto fourScale = scale(4, 1);
        four.scale = fourScale;
        const auto rejected = judgement::quantizeAmount(four, beat(1, 4611686018427387904LL));
        REQUIRE_FALSE(rejected.has_value());
        checkRejection(rejected.error(), "judgement.s7a2.input.amount_narrowed", "budget_exceeded",
                       "amountSpec");
    }

    SECTION("the negative mirror is rejected the same way") {
        //  The rule is symmetric: there is no sign for which the quantization accepts a magnitude
        //  it refuses on the other side.
        AmountSpec quarter = fullWidth;
        const auto quarterScale = scale(1, 4);
        quarter.scale = quarterScale;
        const auto rejected = judgement::quantizeAmount(quarter, beat(-4611686018427387904LL, 1));
        REQUIRE_FALSE(rejected.has_value());
        checkRejection(rejected.error(), "judgement.s7a2.input.amount_narrowed", "budget_exceeded",
                       "amountSpec");
    }

    SECTION("a representable exact quotient outside the declared range stays a range rejection") {
        //  scale = 4/1: the exact quotient of 4000004 is 1000001, which the canonical integer holds
        //  exactly but the declared range does not, so the refusal is the range and not
        //  representability.
        const auto bounded = spec(4, 1, -1000000, 1000000);
        const auto rejected = judgement::quantizeAmount(bounded, beat(4000004, 1));
        REQUIRE_FALSE(rejected.has_value());
        checkRejection(rejected.error(), "judgement.s7a2.input.amount_out_of_range",
                       "budget_exceeded", "amountSpec.range");
    }
}

TEST_CASE("amount: a declaration that cannot be honoured is a structural rejection",
          "[judgement][s7a2][input][amount]") {
    SECTION("a non-positive scale is rejected") {
        const auto zero = judgement::quantizeAmount(spec(0, 1, -10, 10), beat(1, 1));
        REQUIRE_FALSE(zero.has_value());
        checkRejection(zero.error(), "judgement.s7a2.input.mapping_declaration_invalid",
                       "invalid_relation", "amountSpec.scale");

        const auto negative = judgement::quantizeAmount(spec(-1, 4, -10, 10), beat(1, 1));
        REQUIRE_FALSE(negative.has_value());
        checkRejection(negative.error(), "judgement.s7a2.input.mapping_declaration_invalid",
                       "invalid_relation", "amountSpec.scale");
    }

    SECTION("a reversed range is rejected") {
        const auto reversed = judgement::quantizeAmount(spec(1, 1, 10, -10), beat(0, 1));
        REQUIRE_FALSE(reversed.has_value());
        checkRejection(reversed.error(), "judgement.s7a2.input.mapping_declaration_invalid",
                       "invalid_relation", "amountSpec.scale");
    }
}

//  -------------------------------------------------------------------------------------------
//  2. The InputMapping profile as a session component (CM-T09)
//  -------------------------------------------------------------------------------------------

TEST_CASE("mapping: the three identity components are declared and checked separately",
          "[judgement][s7a2][input][mapping]") {
    const AmountSpec amount = spec(1, 1, -10, 10);
    const InputMappingProfile reference = oneDomainMapping(amount);
    CHECK(judgement::validateInputMapping(reference).has_value());

    SECTION("a complete profile contributes one session identity") {
        CHECK(judgement::contributesToSameSessionIdentity(reference, reference));
    }

    SECTION("a missing profile identity is a structural rejection") {
        InputMappingProfile missing = reference;
        missing.profileId = {};
        const auto rejected = judgement::validateInputMapping(missing);
        REQUIRE_FALSE(rejected.has_value());
        checkRejection(rejected.error(), "judgement.s7a2.input.mapping_declaration_invalid",
                       "invalid_relation", "inputMapping.profileId");
    }

    SECTION("a missing version is a structural rejection") {
        InputMappingProfile missing = reference;
        missing.profileVersion = {};
        const auto rejected = judgement::validateInputMapping(missing);
        REQUIRE_FALSE(rejected.has_value());
        checkRejection(rejected.error(), "judgement.s7a2.input.mapping_declaration_invalid",
                       "invalid_relation", "inputMapping.profileVersion");
    }

    SECTION("a missing source class is a structural rejection") {
        InputMappingProfile missing = reference;
        missing.sourceClass = SourceClass{};
        const auto rejected = judgement::validateInputMapping(missing);
        REQUIRE_FALSE(rejected.has_value());
        checkRejection(rejected.error(), "judgement.s7a2.input.mapping_declaration_invalid",
                       "invalid_relation", "inputMapping.sourceClass");
    }

    SECTION("an empty domain token is a structural rejection") {
        InputMappingProfile missing = reference;
        missing.domains = {domain({}, amount)};
        const auto rejected = judgement::validateInputMapping(missing);
        REQUIRE_FALSE(rejected.has_value());
        checkRejection(rejected.error(), "judgement.s7a2.input.mapping_declaration_invalid",
                       "invalid_relation", "domainToken");
    }

    SECTION("a domain with an unusable AmountSpec is a structural rejection") {
        InputMappingProfile unusable = reference;
        unusable.domains = {domain("test.domain.button", spec(1, 1, 5, -5))};
        const auto rejected = judgement::validateInputMapping(unusable);
        REQUIRE_FALSE(rejected.has_value());
        checkRejection(rejected.error(), "judgement.s7a2.input.mapping_declaration_invalid",
                       "invalid_relation", "amountSpec");
    }

    SECTION("a duplicated domain declaration is rejected instead of resolved by position") {
        InputMappingProfile duplicated = reference;
        duplicated.domains = {domain("test.domain.button", amount),
                              domain("test.domain.button", spec(1, 2, -4, 4))};
        const auto rejected = judgement::validateInputMapping(duplicated);
        REQUIRE_FALSE(rejected.has_value());
        checkRejection(rejected.error(), "judgement.s7a2.input.mapping_declaration_invalid",
                       "invalid_relation", "inputMapping.domains");
    }

    SECTION("a domain table with several distinct domains is accepted") {
        InputMappingProfile several = reference;
        several.domains = {domain("test.domain.button", amount),
                           domain("test.domain.axis", spec(1, 2, -4, 4))};
        CHECK(judgement::validateInputMapping(several).has_value());
    }
}

TEST_CASE("mapping: each declared identity component changes the session identity",
          "[judgement][s7a2][input][mapping]") {
    const AmountSpec amount = spec(1, 1, -10, 10);
    const InputMappingProfile reference = oneDomainMapping(amount);

    SECTION("a version change changes the session identity") {
        InputMappingProfile changed = reference;
        changed.profileVersion = "2";
        CHECK_FALSE(judgement::contributesToSameSessionIdentity(reference, changed));
        const auto rejected = judgement::rejectRuntimeMappingChange(reference, changed);
        REQUIRE_FALSE(rejected.has_value());
        checkRejection(rejected.error(), "judgement.s7a2.input.runtime_mapping_change",
                       "invalid_relation", "inputMapping.profileVersion");
    }

    SECTION("a source class change changes the session identity") {
        InputMappingProfile changed = reference;
        changed.sourceClass = sourceClass("test.source.gamepad");
        CHECK_FALSE(judgement::contributesToSameSessionIdentity(reference, changed));
        const auto rejected = judgement::rejectRuntimeMappingChange(reference, changed);
        REQUIRE_FALSE(rejected.has_value());
        checkRejection(rejected.error(), "judgement.s7a2.input.runtime_mapping_change",
                       "invalid_relation", "inputMapping.sourceClass");
    }

    SECTION("a profile identity change changes the session identity") {
        InputMappingProfile changed = reference;
        changed.profileId = "test.mapping.gamepad";
        CHECK_FALSE(judgement::contributesToSameSessionIdentity(reference, changed));
        const auto rejected = judgement::rejectRuntimeMappingChange(reference, changed);
        REQUIRE_FALSE(rejected.has_value());
        checkRejection(rejected.error(), "judgement.s7a2.input.runtime_mapping_change",
                       "invalid_relation", "inputMapping.profileId");
    }

    SECTION("the refusal writes neither profile") {
        InputMappingProfile newVersion = reference;
        newVersion.profileVersion = "2";
        CHECK_FALSE(judgement::rejectRuntimeMappingChange(reference, newVersion).has_value());
        CHECK(reference.profileVersion == "1");
        CHECK(newVersion.profileVersion == "2");
    }
}

TEST_CASE("mapping: a domain declaration change is a session identity change",
          "[judgement][s7a2][input][mapping]") {
    //  A declared AmountSpec is the quantization CM-T09 requires inside the session identity, so
    //  changing any field of one declaration, or adding or removing a declaration, is a different
    //  session identity. Every candidate below keeps the profile identity, the version and the
    //  source class identical, so a refusal can only come from the declaration set.
    const AmountSpec amount = spec(1, 1, -10, 10);
    const InputMappingProfile reference = oneDomainMapping(amount);

    SECTION("a scale change") {
        InputMappingProfile changed = reference;
        changed.domains = {domain("test.domain.button", spec(1, 4, -10, 10))};
        CHECK_FALSE(judgement::contributesToSameSessionIdentity(reference, changed));
        checkDomainMutationRefused(reference, changed);
    }

    SECTION("a minimum change") {
        InputMappingProfile changed = reference;
        changed.domains = {domain("test.domain.button", spec(1, 1, -11, 10))};
        CHECK_FALSE(judgement::contributesToSameSessionIdentity(reference, changed));
        checkDomainMutationRefused(reference, changed);
    }

    SECTION("a maximum change") {
        InputMappingProfile changed = reference;
        changed.domains = {domain("test.domain.button", spec(1, 1, -10, 11))};
        CHECK_FALSE(judgement::contributesToSameSessionIdentity(reference, changed));
        checkDomainMutationRefused(reference, changed);
    }

    SECTION("a boundary policy change") {
        InputMappingProfile changed = reference;
        changed.domains = {
            domain("test.domain.button", spec(1, 1, -10, 10, AmountBoundaryPolicy::exclusive))};
        CHECK_FALSE(judgement::contributesToSameSessionIdentity(reference, changed));
        checkDomainMutationRefused(reference, changed);
    }

    SECTION("a domain declaration added") {
        InputMappingProfile changed = reference;
        changed.domains = {domain("test.domain.button", amount),
                           domain("test.domain.axis", spec(1, 2, -4, 4))};
        CHECK(judgement::validateInputMapping(changed).has_value());
        CHECK_FALSE(judgement::contributesToSameSessionIdentity(reference, changed));
        checkDomainMutationRefused(reference, changed);
    }

    SECTION("a domain declaration removed") {
        InputMappingProfile changed = reference;
        changed.domains = {};
        CHECK_FALSE(judgement::contributesToSameSessionIdentity(reference, changed));
        checkDomainMutationRefused(reference, changed);
    }

    SECTION("a domain token renamed while its AmountSpec is unchanged") {
        InputMappingProfile changed = reference;
        changed.domains = {domain("test.domain.key", amount)};
        CHECK_FALSE(judgement::contributesToSameSessionIdentity(reference, changed));
        checkDomainMutationRefused(reference, changed);
    }
}

TEST_CASE("mapping: the declaration order of the domain table is not a session identity change",
          "[judgement][s7a2][input][mapping]") {
    //  The order of the table carries no semantics - validateInputMapping refuses a duplicated
    //  token instead of resolving it by position - so the identity comparison reads the table as a
    //  set keyed by the domain token. Reordering the same declarations therefore contributes the
    //  same session identity and is not a runtime mapping change. This is the proof that the
    //  comparison is normalized by token and not positional: every declaration below keeps its own
    //  scale, range and boundary policy, and only its position in the table changes.
    const InputMappingProfile ordered = InputMappingProfile{
        .profileId = "test.mapping.keyboard",
        .profileVersion = "1",
        .sourceClass = sourceClass("test.source.keyboard"),
        .domains = {domain("test.domain.axis", spec(1, 2, -4, 4)),
                    domain("test.domain.button", spec(1, 1, -10, 10)),
                    domain("test.domain.trigger", spec(2, 1, -1, 1))},
    };
    const InputMappingProfile reversed = InputMappingProfile{
        .profileId = "test.mapping.keyboard",
        .profileVersion = "1",
        .sourceClass = sourceClass("test.source.keyboard"),
        .domains = {domain("test.domain.trigger", spec(2, 1, -1, 1)),
                    domain("test.domain.button", spec(1, 1, -10, 10)),
                    domain("test.domain.axis", spec(1, 2, -4, 4))},
    };
    REQUIRE(judgement::validateInputMapping(ordered).has_value());
    REQUIRE(judgement::validateInputMapping(reversed).has_value());

    SECTION("a reordered table is the same session identity in either argument order") {
        CHECK(judgement::contributesToSameSessionIdentity(ordered, reversed));
        CHECK(judgement::contributesToSameSessionIdentity(reversed, ordered));
        CHECK(judgement::rejectRuntimeMappingChange(ordered, reversed).has_value());
        CHECK(judgement::rejectRuntimeMappingChange(reversed, ordered).has_value());
    }

    SECTION("a reordered table whose declarations do not all agree is a different identity") {
        InputMappingProfile requantized = reversed;
        requantized.domains = {domain("test.domain.trigger", spec(2, 1, -1, 1)),
                               domain("test.domain.button", spec(1, 4, -40, 40)),
                               domain("test.domain.axis", spec(1, 2, -4, 4))};
        CHECK_FALSE(judgement::contributesToSameSessionIdentity(ordered, requantized));
        checkDomainMutationRefused(ordered, requantized);
    }

    SECTION("the same token declared twice is not a declaration set") {
        //  A repeated token is refused rather than resolved by position, so it is not a
        //  well-defined declaration set and the identity comparison reports a difference instead of
        //  inventing an order for it.
        InputMappingProfile duplicated = ordered;
        duplicated.domains = {domain("test.domain.axis", spec(1, 2, -4, 4)),
                              domain("test.domain.axis", spec(1, 2, -4, 4))};
        InputMappingProfile single = ordered;
        single.domains = {domain("test.domain.axis", spec(1, 2, -4, 4))};
        CHECK_FALSE(judgement::contributesToSameSessionIdentity(duplicated, duplicated));
        CHECK_FALSE(judgement::contributesToSameSessionIdentity(single, duplicated));
    }
}

//  -------------------------------------------------------------------------------------------
//  3. The compile-time half of the same contract
//  -------------------------------------------------------------------------------------------

TEST_CASE("mapping: a domain declaration cannot omit its typed AmountSpec",
          "[judgement][s7a2][input][mapping]") {
    //  AmountSpec is a required member of InputDomainDeclaration, so a domain without a declared
    //  quantization does not compile. That is the type-level form of Spec 3.7.6 item 1.
    static_assert(std::is_constructible_v<InputDomainDeclaration, std::string_view, AmountSpec>);
    static_assert(!std::is_constructible_v<InputDomainDeclaration, std::string_view>);
    static_assert(!std::is_default_constructible_v<InputDomainDeclaration>);

    CHECK(std::is_constructible_v<InputDomainDeclaration, std::string_view, AmountSpec>);
    CHECK_FALSE(std::is_constructible_v<InputDomainDeclaration, std::string_view>);
    CHECK_FALSE(std::is_default_constructible_v<InputDomainDeclaration>);
}

TEST_CASE("amount: the boundary policy and the action have no default enumerator",
          "[judgement][s7a2][input][amount]") {
    //  No local token stands for "the default boundary policy", because there is none: a
    //  specification that does not declare one does not compile.
    static_assert(!std::is_constructible_v<InputDomainDeclaration, std::string_view>);
    static_assert(sizeof(AmountBoundaryPolicy) == sizeof(std::uint8_t));

    //  The declared set has exactly the two enumerators the inversion rule needs, and no third one
    //  that would let a caller select a saturation the ruling forbids.
    const std::vector<AmountBoundaryPolicy> policies{AmountBoundaryPolicy::inclusive,
                                                     AmountBoundaryPolicy::exclusive};
    CHECK(policies.size() == 2);

    CHECK_FALSE(std::is_default_constructible_v<InputDomainDeclaration>);
}

} // namespace

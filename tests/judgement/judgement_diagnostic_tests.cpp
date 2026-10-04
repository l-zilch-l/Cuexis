//  S7A-1 diagnostic carrier tests.
//
//  The carrier must be able to carry a code, a category, a severity, a faulted flag, a field path,
//  requirement and identity components, a capability id, a remediation and a budget context while
//  freezing no value. These tests therefore supply their own tokens: if the header had frozen a
//  code table or an enumeration set, a caller-supplied token could not travel through it.
//
//  Every literal in this file is test-owned. No production diagnostic value is asserted, and no
//  test token is a production token.

#include <catch2/catch_test_macros.hpp>

#include <cuexis/judgement/diagnostic.hpp>

#include <string>
#include <string_view>

namespace {

namespace judgement = cuexis::judgement;

class TestBudgetContext final : public judgement::BudgetContext {
  public:
    [[nodiscard]] auto budgetDomain() const noexcept -> std::string_view override {
        return "test.budget.domain";
    }

    [[nodiscard]] auto budgetSubject() const noexcept -> std::string_view override {
        return "test.budget.subject";
    }
};

//  Reads one context entry, or an empty token when the entry was not attached.
[[nodiscard]] auto contextValue(const cuexis::core::Error& error, std::string_view key)
    -> std::string_view {
    for (const auto& entry : error.context()) {
        if (entry.key == key) {
            return entry.value;
        }
    }
    return {};
}

//  A carrier with every component supplied by the caller.
[[nodiscard]] auto fullDiagnostic(const judgement::BudgetContext* budget) -> judgement::Diagnostic {
    return judgement::Diagnostic{
        .code = "test.diagnostic.code",
        .category = "test.category",
        .severity = "test.severity",
        .faulted = true,
        .summary = "test summary text",
        .context =
            judgement::DiagnosticContext{
                .fieldPath =
                    judgement::DiagnosticFieldPath{.section = "test.section", .path = "test.path"},
                .requirement = judgement::DiagnosticRequirementRef{.kind = "test.kind",
                                                                   .identity = "test.identity"},
                .identity = judgement::DiagnosticIdentityComponent{.component = "test.component",
                                                                   .token = "test.token"},
                .budget = budget,
                .capabilityId = "test.capability",
                .remediation = "test.remediation",
            },
    };
}

//  A carrier whose context components are all absent.
[[nodiscard]] auto bareDiagnostic() -> judgement::Diagnostic {
    return judgement::Diagnostic{
        .code = "test.bare.code",
        .category = {},
        .severity = {},
        .faulted = false,
        .summary = "test bare summary",
        .context =
            judgement::DiagnosticContext{
                .fieldPath = judgement::DiagnosticFieldPath{.section = {}, .path = {}},
                .requirement = judgement::DiagnosticRequirementRef{.kind = {}, .identity = {}},
                .identity = judgement::DiagnosticIdentityComponent{.component = {}, .token = {}},
                .budget = nullptr,
                .capabilityId = {},
                .remediation = {},
            },
    };
}

} // namespace

TEST_CASE("the diagnostic code and summary travel through the error channel",
          "[judgement][s7a-1][diagnostic]") {
    const auto error = judgement::toError(fullDiagnostic(nullptr));

    CHECK(error.code() == "test.diagnostic.code");
    CHECK(error.message() == "test summary text");
}

TEST_CASE("category severity and faulted travel as context", "[judgement][s7a-1][diagnostic]") {
    const auto error = judgement::toError(fullDiagnostic(nullptr));
    const auto bareError = judgement::toError(bareDiagnostic());

    CHECK(contextValue(error, "category") == "test.category");
    CHECK(contextValue(error, "severity") == "test.severity");
    CHECK(contextValue(error, "faulted") == "true");
    CHECK(contextValue(bareError, "faulted") == "false");
}

TEST_CASE("field path requirement identity capability and remediation travel as context",
          "[judgement][s7a-1][diagnostic]") {
    const auto error = judgement::toError(fullDiagnostic(nullptr));

    CHECK(contextValue(error, "field.section") == "test.section");
    CHECK(contextValue(error, "field.path") == "test.path");
    CHECK(contextValue(error, "requirement.kind") == "test.kind");
    CHECK(contextValue(error, "requirement.identity") == "test.identity");
    CHECK(contextValue(error, "identity.component") == "test.component");
    CHECK(contextValue(error, "identity.token") == "test.token");
    CHECK(contextValue(error, "capabilityId") == "test.capability");
    CHECK(contextValue(error, "remediation") == "test.remediation");
}

TEST_CASE("an absent context component is not attached", "[judgement][s7a-1][diagnostic]") {
    const auto error = judgement::toError(bareDiagnostic());

    CHECK(contextValue(error, "category").empty());
    CHECK(contextValue(error, "severity").empty());
    CHECK(contextValue(error, "field.section").empty());
    CHECK(contextValue(error, "field.path").empty());
    CHECK(contextValue(error, "requirement.kind").empty());
    CHECK(contextValue(error, "requirement.identity").empty());
    CHECK(contextValue(error, "identity.component").empty());
    CHECK(contextValue(error, "identity.token").empty());
    CHECK(contextValue(error, "capabilityId").empty());
    CHECK(contextValue(error, "remediation").empty());
    CHECK(contextValue(error, "budget.domain").empty());
    CHECK(contextValue(error, "budget.subject").empty());
}

TEST_CASE("the budget context is read through its interface and carries no number",
          "[judgement][s7a-1][diagnostic]") {
    const TestBudgetContext budget;
    const auto error = judgement::toError(fullDiagnostic(&budget));

    CHECK(contextValue(error, "budget.domain") == "test.budget.domain");
    CHECK(contextValue(error, "budget.subject") == "test.budget.subject");
}

TEST_CASE("the carrier holds caller supplied tokens instead of a frozen set",
          "[judgement][s7a-1][diagnostic]") {
    auto first = fullDiagnostic(nullptr);
    auto second = first;
    second.code = "test.diagnostic.other";
    second.category = "test.category.other";

    const auto firstError = judgement::toError(first);
    const auto secondError = judgement::toError(second);

    //  Two carriers that differ only in caller-supplied tokens stay distinguishable, which is only
    //  possible because no value set is fixed in the header.
    CHECK(firstError.code() != secondError.code());
    CHECK(contextValue(firstError, "category") != contextValue(secondError, "category"));
}

TEST_CASE("the error owns its tokens and outlives the diagnostic",
          "[judgement][s7a-1][diagnostic]") {
    const cuexis::core::Error error = [] {
        const auto diagnostic = fullDiagnostic(nullptr);
        return judgement::toError(diagnostic);
    }();

    //  The diagnostic above is gone; the error still owns every token it was given.
    CHECK(error.code() == "test.diagnostic.code");
    CHECK(contextValue(error, "capabilityId") == "test.capability");
    CHECK(contextValue(error, "field.path") == "test.path");
}

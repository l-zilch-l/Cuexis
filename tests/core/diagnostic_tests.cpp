#include <cuexis/core/diagnostic.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Diagnostics preserve details and report severities", "[core][diagnostic]") {
    cuexis::core::Diagnostics diagnostics;
    diagnostics.add(cuexis::core::Diagnostic{cuexis::core::DiagnosticSeverity::Warning,
                                             "chart.field.deprecated", "Field is deprecated",
                                             "$/objects/0/legacy"}
                        .withContext("replacement", "components"));
    diagnostics.add(cuexis::core::Diagnostic{cuexis::core::DiagnosticSeverity::Error,
                                             "chart.field.missing", "Required field is missing",
                                             "$/objects/0/id"});

    REQUIRE(diagnostics.size() == 2);
    REQUIRE(diagnostics.hasWarnings());
    REQUIRE(diagnostics.hasErrors());
    REQUIRE(diagnostics.count(cuexis::core::DiagnosticSeverity::Info) == 0);
    REQUIRE(diagnostics.items()[0].context().size() == 1);
    REQUIRE(diagnostics.items()[0].context()[0].value == "components");
}

TEST_CASE("Diagnostics sort by stable machine-readable fields", "[core][diagnostic]") {
    cuexis::core::Diagnostics diagnostics;
    diagnostics.add(cuexis::core::Diagnostic{cuexis::core::DiagnosticSeverity::Warning, "chart.z",
                                             "Later", "$/objects/2"});
    diagnostics.add(cuexis::core::Diagnostic{cuexis::core::DiagnosticSeverity::Error, "chart.b",
                                             "Second", "$/objects/1"});
    diagnostics.add(cuexis::core::Diagnostic{cuexis::core::DiagnosticSeverity::Error, "chart.a",
                                             "First", "$/objects/1"});

    diagnostics.sortDeterministically();

    REQUIRE(diagnostics.items()[0].code() == "chart.a");
    REQUIRE(diagnostics.items()[1].code() == "chart.b");
    REQUIRE(diagnostics.items()[2].code() == "chart.z");
}

TEST_CASE("Diagnostics append transfers all entries", "[core][diagnostic]") {
    cuexis::core::Diagnostics first;
    first.add(
        cuexis::core::Diagnostic{cuexis::core::DiagnosticSeverity::Info, "core.first", "First"});

    cuexis::core::Diagnostics second;
    second.add(
        cuexis::core::Diagnostic{cuexis::core::DiagnosticSeverity::Info, "core.second", "Second"});
    second.add(
        cuexis::core::Diagnostic{cuexis::core::DiagnosticSeverity::Error, "core.third", "Third"});

    first.append(std::move(second));

    REQUIRE(first.size() == 3);
    REQUIRE(first.items()[2].code() == "core.third");
}

TEST_CASE("Bounded Diagnostics append one caller-defined limit diagnostic", "[core][diagnostic]") {
    cuexis::core::Diagnostics diagnostics{
        2, cuexis::core::Diagnostic{cuexis::core::DiagnosticSeverity::Error,
                                    "chart.diagnostics.limit_exceeded",
                                    "Chart diagnostic limit was reached", "$"}
               .withContext("max_diagnostics", "2")};

    REQUIRE(diagnostics.add(cuexis::core::Diagnostic{cuexis::core::DiagnosticSeverity::Warning,
                                                     "chart.first", "First"}));
    REQUIRE(diagnostics.add(cuexis::core::Diagnostic{cuexis::core::DiagnosticSeverity::Warning,
                                                     "chart.second", "Second"}));
    REQUIRE_FALSE(diagnostics.limitReached());
    REQUIRE(diagnostics.size() == 2);

    REQUIRE_FALSE(diagnostics.add(cuexis::core::Diagnostic{
        cuexis::core::DiagnosticSeverity::Warning, "chart.third", "Third"}));
    REQUIRE(diagnostics.limitReached());
    REQUIRE(diagnostics.size() == 2);
    REQUIRE(diagnostics.items()[0].code() == "chart.first");
    REQUIRE(diagnostics.items()[1].code() == "chart.diagnostics.limit_exceeded");
    REQUIRE(diagnostics.count(cuexis::core::DiagnosticSeverity::Error) == 1);

    REQUIRE_FALSE(diagnostics.add(cuexis::core::Diagnostic{
        cuexis::core::DiagnosticSeverity::Warning, "chart.ignored", "Ignored"}));
    REQUIRE(diagnostics.size() == 2);
    REQUIRE(diagnostics.count(cuexis::core::DiagnosticSeverity::Error) == 1);

    diagnostics.clear();
    REQUIRE_FALSE(diagnostics.limitReached());
    REQUIRE(diagnostics.empty());
    REQUIRE(diagnostics.add(cuexis::core::Diagnostic{cuexis::core::DiagnosticSeverity::Info,
                                                     "chart.after_clear", "After clear"}));
}

TEST_CASE("Zero-capacity Diagnostics normalize to one total item", "[core][diagnostic]") {
    cuexis::core::Diagnostics diagnostics{
        0, cuexis::core::Diagnostic{cuexis::core::DiagnosticSeverity::Error,
                                    "chart.diagnostics.limit_exceeded", "Limit reached", "$"}};

    REQUIRE(diagnostics.add(
        cuexis::core::Diagnostic{cuexis::core::DiagnosticSeverity::Error, "chart.first", "First"}));
    REQUIRE_FALSE(diagnostics.limitReached());
    REQUIRE(diagnostics.size() == 1);

    REQUIRE_FALSE(diagnostics.add(cuexis::core::Diagnostic{cuexis::core::DiagnosticSeverity::Error,
                                                           "chart.overflow", "Overflow"}));
    REQUIRE(diagnostics.limitReached());
    REQUIRE(diagnostics.size() == 1);
    REQUIRE(diagnostics.items()[0].code() == "chart.diagnostics.limit_exceeded");
}

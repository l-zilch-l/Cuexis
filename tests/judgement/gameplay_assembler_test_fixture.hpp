#pragma once

// Shared assembler fixture for the S7A-3 case units.

#include "gameplay_test_fixture.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cuexis/core/error.hpp>
#include <cuexis/judgement/gameplay_assembler.hpp>
#include <cuexis/judgement/gameplay_graph.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace assembler_fixture {

namespace judgement = cuexis::judgement;
namespace testing = cuexis::judgement::testing;

using judgement::AssembledGameplay;
using judgement::AssemblyRequest;
using judgement::ContentProfileCounts;
using judgement::ContentProfileLimits;
using judgement::ContentProfileVerdict;
using judgement::EntryKind;
using judgement::GameplayPublication;
using judgement::GameplaySource;
using judgement::SourceForm;
using judgement::UnsupportedContentDeclaration;
using judgement::UnsupportedContentKind;

using testing::contextValue;
using testing::Fixture;

//  Asserts the three parts of a stable rejection this suite reads: the code, the category and the
//  field path. The section is checked separately where it matters.
inline void expectRejection(const cuexis::core::Error& error, std::string_view code,
                            std::string_view category, std::string_view path) {
    CHECK(error.code() == code);
    CHECK(contextValue(error, "category") == category);
    CHECK(contextValue(error, "severity") == "error");
    CHECK(contextValue(error, "faulted") == "false");
    CHECK(contextValue(error, "field.path") == path);
}

inline void expectSection(const cuexis::core::Error& error, std::string_view section) {
    CHECK(contextValue(error, "field.section") == section);
}

//  Adds a second requirement to the fixture document, together with the declaration that names it.
inline void addSecondRequirement(AssemblyRequest& request) {
    auto& document = request.sources.front().document;
    judgement::RequirementRecord second = testing::makeTestRequirement();
    second.stableId.declarationOrdinal = 5U;
    second.identity.requirementLocalId = "requirement.two";
    second.identity.chartEntryId = "chart.entry.two";
    document.requirements.push_back(second);
    document.declarations.push_back(
        testing::testDeclaration(judgement::DeclarationKind::requirement, "requirement.two", 5U));
}

} // namespace assembler_fixture

using namespace assembler_fixture;

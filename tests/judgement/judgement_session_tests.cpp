//  S7A-1 judgement session lifecycle tests.
//
//  These tests need no GPU, no window, no audio device and no chart fixture: an S7A-1 session owns
//  no judgement state, so the empty-session, prepare-failure and destruction paths are all
//  headless.
//
//  No test asserts a diagnostic code literal. The code table belongs to round 6 (CM-D04) and is
//  not frozen; a literal here would freeze it inside the repository. Stability is tested as
//  equality across repeated calls and across sessions, and distinguishability is tested by
//  comparing two rejections with each other.
//
//  A *category* is different: Spec 9.2 freezes exactly nine category names and forbids a tenth, so
//  the category a rejection emits is asserted here. A category name that is not one of the nine is
//  a defect of the same kind as an unregistered code, and no such name may be emitted.

#include <catch2/catch_test_macros.hpp>

#include <cuexis/judgement/judgement_session.hpp>

#include <string>
#include <string_view>
#include <utility>

namespace {

namespace judgement = cuexis::judgement;

[[nodiscard]] auto contextValue(const cuexis::core::Error& error, std::string_view key)
    -> std::string {
    for (const auto& entry : error.context()) {
        if (entry.key == key) {
            return entry.value;
        }
    }
    return {};
}

//  The nine frozen category names of Spec 9.2. Nothing else is a legal category.
constexpr std::string_view kFrozenCategories[]{
    "unknown_capability",  "capability_disabled",    "budget_exceeded",
    "ambiguous_migration", "non_terminating_source", "non_unique_solution",
    "invalid_relation",    "late_policy_incomplete", "identity_closure_incomplete"};

[[nodiscard]] auto isFrozenCategory(std::string_view category) -> bool {
    for (const std::string_view frozen : kFrozenCategories) {
        if (category == frozen) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] auto requiredSession() -> judgement::JudgementSession {
    auto created = judgement::JudgementSession::create();
    REQUIRE(created.has_value());
    return std::move(created).value();
}

} // namespace

TEST_CASE("create yields an empty session without a device", "[judgement][s7a-1][lifecycle]") {
    auto created = judgement::JudgementSession::create();
    REQUIRE(created.has_value());

    judgement::JudgementSession session = std::move(created).value();
    CHECK_FALSE(session.hasPreparedState());
}

TEST_CASE("a refused prepare leaves no half-prepared session", "[judgement][s7a-1][prepare]") {
    auto session = requiredSession();
    REQUIRE_FALSE(session.hasPreparedState());

    const auto before = session.query();
    REQUIRE_FALSE(before.has_value());

    const auto refused = session.prepare();
    REQUIRE_FALSE(refused.has_value());
    CHECK_FALSE(refused.error().code().empty());

    //  The refused prepare published nothing: the prepared phase is still absent, and the verbs
    //  that require it reject with exactly the code they used before the attempt.
    CHECK_FALSE(session.hasPreparedState());
    const auto after = session.query();
    REQUIRE_FALSE(after.has_value());
    CHECK(before.error().code() == after.error().code());
}

TEST_CASE("lifecycle and unfrozen rejections stay distinguishable",
          "[judgement][s7a-1][rejection]") {
    auto session = requiredSession();

    const auto configure = session.configure();
    const auto prepare = session.prepare();
    REQUIRE_FALSE(configure.has_value());
    REQUIRE_FALSE(prepare.has_value());

    //  Two different reason classes must not collapse into one code. No literal is asserted, so
    //  the code table stays unfrozen.
    CHECK(configure.error().code() != prepare.error().code());
    CHECK(configure.error().message() != prepare.error().message());

    //  The categories, in contrast, are frozen. The unconfigured session runs a verb out of the
    //  ABI lifecycle order, which is an ordering fault and goes through the existing
    //  `invalid_relation` atomic-failure path (Spec 3.7.3 rules 4 and 5, Spec 7.2). The semantics
    //  this batch has not frozen are a capability of the compile that is not enabled, so they
    //  report `capability_disabled`. Both are asserted as members of the frozen nine, and neither
    //  the bare `capability` nor the previously emitted `lifecycle` may reappear.
    const std::string configureCategory = contextValue(configure.error(), "category");
    const std::string prepareCategory = contextValue(prepare.error(), "category");
    CHECK(configureCategory == "capability_disabled");
    CHECK(prepareCategory == "invalid_relation");
    CHECK(isFrozenCategory(configureCategory));
    CHECK(isFrozenCategory(prepareCategory));
    CHECK(configureCategory != "capability");
    CHECK(prepareCategory != "lifecycle");
    CHECK(contextValue(configure.error(), "severity") == "error");
    CHECK(contextValue(prepare.error(), "faulted") == "false");
}

TEST_CASE("a rejection is stable across repeated calls and across sessions",
          "[judgement][s7a-1][rejection]") {
    auto first = requiredSession();
    auto second = requiredSession();

    const auto firstAttempt = first.prepare();
    const auto repeatedAttempt = first.prepare();
    const auto otherSessionAttempt = second.prepare();

    REQUIRE_FALSE(firstAttempt.has_value());
    REQUIRE_FALSE(repeatedAttempt.has_value());
    REQUIRE_FALSE(otherSessionAttempt.has_value());
    CHECK(firstAttempt.error().code() == repeatedAttempt.error().code());
    CHECK(firstAttempt.error().code() == otherSessionAttempt.error().code());
    CHECK(firstAttempt.error().message() == otherSessionAttempt.error().message());
}

TEST_CASE("query snapshot seek reject before prepare and reset returns Created",
          "[judgement][s7a-1][rejection]") {
    auto session = requiredSession();

    const auto query = session.query();
    const auto snapshot = session.snapshot();
    const auto seek = session.seek();
    const auto reset = session.reset();

    REQUIRE_FALSE(query.has_value());
    REQUIRE_FALSE(snapshot.has_value());
    REQUIRE_FALSE(seek.has_value());
    REQUIRE(reset.has_value());

    //  One reason class for one missing lifecycle phase.
    CHECK(query.error().code() == snapshot.error().code());
    CHECK(query.error().code() == seek.error().code());
    CHECK_FALSE(session.hasPreparedState());
    CHECK_FALSE(query.error().code().empty());
}

TEST_CASE("submit and advance reject stably before prepare", "[judgement][s7a-1][rejection]") {
    auto session = requiredSession();

    const auto submit = session.submit();
    const auto advance = session.advance();

    REQUIRE_FALSE(submit.has_value());
    REQUIRE_FALSE(advance.has_value());
    CHECK(submit.error().code() == advance.error().code());
    CHECK_FALSE(session.hasPreparedState());
}

TEST_CASE("a session is destructible in every reachable phase", "[judgement][s7a-1][lifetime]") {
    {
        //  Created and destroyed while empty.
        auto created = judgement::JudgementSession::create();
        REQUIRE(created.has_value());
    }
    {
        //  Destroyed after a configuration attempt was refused.
        auto session = requiredSession();
        CHECK_FALSE(session.configure().has_value());
    }
    {
        //  Destroyed after a prepare attempt was refused.
        auto session = requiredSession();
        CHECK_FALSE(session.prepare().has_value());
    }
    {
        //  Destroyed after every prepared-dependent verb was refused.
        auto session = requiredSession();
        CHECK_FALSE(session.submit().has_value());
        CHECK_FALSE(session.advance().has_value());
        CHECK_FALSE(session.query().has_value());
        CHECK_FALSE(session.snapshot().has_value());
        CHECK_FALSE(session.seek().has_value());
        CHECK(session.reset().has_value());
    }
    {
        //  Destroyed after ownership moved to another session value.
        auto session = requiredSession();
        auto moved = std::move(session);
        CHECK_FALSE(moved.hasPreparedState());
    }
}

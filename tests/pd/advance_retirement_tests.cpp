#include "../judgement/execution_test_fixture.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cuexis/judgement/execution_scheduler.hpp>
#include <limits>
using namespace cuexis::judgement;
using namespace cuexis::judgement::testing;
namespace {
struct Fixture3 {
    Fixture base;
    AssemblyRequest assembly;
    std::optional<RulesetDeclaration> rules;
    Fixture3() : assembly(base.request()) {
        base.latePolicy = executionLate();
        executionFields(assembly);
        assembly.executionProfile = "gameplay.execution.t4-k4.rpa.v1";
        assembly.identityDeclarations.engine.executionProfileToken = assembly.executionProfile;
        assembly.identityDeclarations.engine.lateAlgorithmToken = "late.window.batch.v1";
        assembly.identityDeclarations.engine.factSemanticRevision = "fact.semantic.rpa.v1";
    }
    auto prepared() -> PreparedGameplay {
        auto p = prepareResolvedGameplay({assembly, {}});
        INFO((p ? "" : std::string{p.error().message()}));
        REQUIRE(p);
        return *p;
    }
    auto configuration() -> SessionConfiguration {
        InputMappingProfile mapping{
            "mapping",
            "v1",
            *SourceClass::fromToken("keyboard"),
            {{"domain.binding.one",
              {makeDuration(1, 1), -100, 100, AmountBoundaryPolicy::inclusive}}}};
        auto result = SessionConfiguration{OwnedInputMappingProfile{mapping},
                                           OwnedTimebaseProfile{base.timebase},
                                           base.latePolicy,
                                           assembly.identityDeclarations,
                                           "calibration.fixture",
                                           assembly.executionProfile,
                                           "late.window.batch.v1",
                                           "fact.semantic.rpa.v1"};
        if (rules) {
            auto preparedRules = prepareRuleset(*rules);
            REQUIRE(preparedRules);
            result.ruleset = *preparedRules;
        }
        return result;
    }
    auto session() -> JudgementSession {
        auto s = JudgementSession::create();
        REQUIRE(s);
        REQUIRE(s->configure(configuration()));
        REQUIRE(s->prepare(prepared()));
        return std::move(*s);
    }
};
constexpr RpaContext context{1, 1, 1};
auto scheduler(JudgementSession& session, std::size_t capacity) -> ExecutionScheduler {
    auto value = ExecutionScheduler::create(session, context, TickSpan{3}, capacity);
    REQUIRE(value);
    return std::move(*value);
}
void retired(ExecutionScheduler& scheduler, std::uint64_t id) {
    const auto result = scheduler.poll(id);
    REQUIRE_FALSE(result);
    CHECK(result.error().code() == "host.rpa.request_retired");
}
} // namespace
TEST_CASE("PD scheduler reclaims retired identities beyond fixed capacity", "[pd][retirement]") {
    Fixture3 fixture;
    auto session = fixture.session();
    auto owner = scheduler(session, 2);
    REQUIRE(owner.deliver({context, 0, CapturePrefix{Tick{10000}}}));
    for (std::uint64_t command = 0; command < 64; ++command) {
        const auto id = 7 + command * 2;
        const AdvanceRequest request{context, id, command, Tick{903}};
        const auto result = owner.request(request);
        INFO(command);
        REQUIRE(result);
        REQUIRE(result->disposition == AdvanceDisposition::executed);
        const auto duplicate = owner.request(request);
        REQUIRE(duplicate);
        CHECK(duplicate->decisionOrdinal == result->decisionOrdinal);
        REQUIRE(owner.retire(id));
    }
    retired(owner, 7);
    const auto replay = owner.request({context, 7, 0, Tick{903}});
    REQUIRE_FALSE(replay);
    CHECK(replay.error().code() == "host.rpa.request_retired");
    CHECK_FALSE(owner.suspended());
}
TEST_CASE("PD scheduler retirement retains holes and rejects old IDs without execution",
          "[pd][retirement]") {
    Fixture3 fixture;
    auto session = fixture.session();
    auto owner = scheduler(session, 3);
    REQUIRE(owner.deliver({context, 0, CapturePrefix{Tick{10000}}}));
    REQUIRE(owner.request({context, 7, 0, Tick{903}}));
    const auto second = owner.request({context, 11, 1, Tick{904}});
    REQUIRE(second);
    REQUIRE(owner.retire(11));
    REQUIRE(owner.retire(11));
    CHECK_FALSE(owner.retentionProgress().reclaimedThrough);
    const auto hole = owner.poll(11);
    REQUIRE(hole);
    CHECK(hole->disposition == AdvanceDisposition::retired);
    CHECK_FALSE(hole->result);
    const auto older = owner.request({context, 9, 2, Tick{905}});
    REQUIRE_FALSE(older);
    CHECK(older.error().code() == "host.rpa.context_stale");
    REQUIRE(owner.poll(7)->result);
    REQUIRE(owner.retire(7));
    REQUIRE(owner.retire(7));
    const auto progress = owner.retentionProgress();
    CHECK(progress.identityContext == context);
    CHECK(progress.reclaimedThrough == 11);
    CHECK(progress.lastAcceptedId == 11);
    retired(owner, 7);
    retired(owner, 11);
    const auto before = owner.lastDecisionOrdinal();
    const auto old = owner.request({context, 7, 99, Tick{999}});
    REQUIRE_FALSE(old);
    CHECK(old.error().code() == "host.rpa.request_retired");
    CHECK(owner.lastDecisionOrdinal() == before);
    const auto next = owner.request({context, 19, 2, Tick{905}});
    REQUIRE(next);
    CHECK(next->disposition == AdvanceDisposition::executed);
    // A new request records both admission and execution; rejected old IDs record neither.
    CHECK(next->decisionOrdinal == *before + 2);
    REQUIRE(owner.retire(19));
    REQUIRE(owner.retire(7));
}
TEST_CASE("PD scheduler pending identities survive failed retirement and payload conflicts",
          "[pd][retirement]") {
    Fixture3 fixture;
    auto session = fixture.session();
    auto owner = scheduler(session, 2);
    const AdvanceRequest request{context, 7, 0, Tick{903}};
    REQUIRE(owner.request(request)->disposition == AdvanceDisposition::waiting);
    REQUIRE_FALSE(owner.retire(7));
    REQUIRE(owner.request(request)->disposition == AdvanceDisposition::waiting);
    SECTION("completion remains available until explicit retirement") {
        REQUIRE(owner.deliver({context, 0, CapturePrefix{Tick{10000}}}));
        REQUIRE(owner.poll(7)->result);
        REQUIRE(owner.retire(7));
        retired(owner, 7);
    }
    SECTION("conflicting unretired payload still suspends execution") {
        auto conflict = request;
        conflict.horizon = Tick{904};
        const auto rejected = owner.request(conflict);
        REQUIRE_FALSE(rejected);
        CHECK(rejected.error().code() == "host.rpa.message_conflict");
        CHECK(owner.suspended());
    }
}
TEST_CASE("PD scheduler retirement handles maximum request ID without wrapping",
          "[pd][retirement]") {
    Fixture3 fixture;
    auto session = fixture.session();
    auto owner = scheduler(session, 1);
    REQUIRE(owner.deliver({context, 0, CapturePrefix{Tick{10000}}}));
    constexpr auto id = std::numeric_limits<std::uint64_t>::max();
    REQUIRE(owner.request({context, id, 0, Tick{903}}));
    REQUIRE(owner.retire(id));
    retired(owner, id);
    REQUIRE_FALSE(owner.request({context, 0, 1, Tick{904}}));
}

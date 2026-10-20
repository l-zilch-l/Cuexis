#include "../rpa/execution_fixture.hpp"
#include "gameplay_internal.hpp"
#include "presentation_projection.hpp"

using namespace cuexis;
using namespace cuexis::playback;
using namespace cuexis::playback::detail;
namespace {
struct AdapterFixture {
    test_support::SessionTestContent source;
    GameplayContent content{source.content()};
    judgement::JudgementSession kernel{std::move(*judgement::JudgementSession::create())};
    const judgement::RequirementIdentity& identity{
        source.request.assembly.sources[0].document.requirements[0].identity};
    AdapterFixture() {
        const auto* c = GameplayAccess::content(content);
        REQUIRE(kernel.configure(c->configuration));
        REQUIRE(kernel.prepare(c->capsule.gameplay));
    }
    auto binding(std::string id = "one", std::string target = "target") const
        -> GameplayFactBinding {
        GameplayRequirementRef ref{identity.chartEntryId,       identity.invocationId,
                                   identity.moduleId,           identity.exportId,
                                   identity.requirementLocalId, {}};
        for (const auto& step : identity.emissionPath)
            ref.emissionPath.push_back({step.nodeId, step.repeatIndex});
        return {std::move(id),
                std::move(ref),
                GameplayPhase::Tap,
                GameplayOutcome::Hit,
                GameplayTimingClass::Any,
                std::move(target),
                false,
                {0},
                {}};
    }
    auto fact(std::uint64_t ordinal = 0) const -> judgement::detail::PublicationFact {
        return {judgement::FactKind::phaseOutcome,
                {1, ordinal},
                &identity,
                judgement::PhaseKind::tap,
                judgement::Outcome::hit,
                judgement::TickDelta{0}};
    }
    auto project(
        const PreparedProjection& prepared, const ProjectionState& current, std::int64_t tick,
        std::vector<judgement::detail::PublicationFact> facts = {},
        PresentationProjectionAdapter::HasTarget target =
            [](std::size_t, std::string_view) -> core::Result<bool> { return true; })
        -> core::Result<ProjectionCandidate> {
        auto scope = judgement::detail::PublicationAccess::scope(kernel);
        REQUIRE(scope);
        auto read = judgement::detail::PublicationAccess::readSince(kernel, *scope, 0);
        REQUIRE(read);
        auto metadata = read->metadata();
        REQUIRE(metadata);
        auto m = metadata->get();
        m.fromCursor = current.cursor;
        m.publishableCursor = current.cursor + facts.size();
        m.sealedCursor = m.publishableCursor;
        return PresentationProjectionAdapter::project(
            prepared, current, m, {tick},
            [&](std::uint64_t ordinal) -> core::Result<judgement::detail::PublicationFact> {
                return facts.at(static_cast<std::size_t>(ordinal - current.cursor));
            },
            target);
    }
};
auto hasDiagnostic(const core::Diagnostics& diagnostics, std::string_view code) -> bool {
    return std::ranges::any_of(diagnostics.items(),
                               [&](const auto& d) { return d.code() == code; });
}
} // namespace

TEST_CASE("PD projection uses full prepared source predicates in Fact order",
          "[pd][projection][index]") {
    AdapterFixture f;
    std::vector<GameplayFactBinding> bindings{f.binding()};
    for (int field = 0; field < 8; ++field) {
        auto other = f.binding("other" + std::to_string(field), "wrong");
        switch (field) {
        case 0:
            other.source.chartEntryId += "x";
            break;
        case 1:
            other.source.invocationId += "x";
            break;
        case 2:
            other.source.moduleId += "x";
            break;
        case 3:
            other.source.exportId += "x";
            break;
        case 4:
            other.source.requirementLocalId += "x";
            break;
        case 5:
            other.source.emissionPath.push_back({"extra", 0});
            break;
        case 6:
            other.phase = GameplayPhase::Head;
            break;
        case 7:
            other.outcome = GameplayOutcome::Miss;
            break;
        }
        bindings.push_back(other);
    }
    auto exact = f.binding("exact", "exact");
    exact.timing = GameplayTimingClass::Exact;
    auto early = f.binding("early", "early");
    early.timing = GameplayTimingClass::Early;
    auto late = f.binding("late", "late");
    late.timing = GameplayTimingClass::Late;
    bindings.insert(bindings.end(), {exact, early, late});
    auto p = PresentationProjectionAdapter::prepare(
        bindings, {{"target", 4}, {"wrong", 5}, {"exact", 6}, {"early", 7}, {"late", 8}});
    REQUIRE(p);
    auto stray = f.fact();
    stray.kind = judgement::FactKind::stray;
    auto empty = stray;
    empty.kind = judgement::FactKind::consumeEmpty;
    std::size_t lookups = 0;
    auto c = f.project(*p, {}, 0, {stray, f.fact(1), empty},
                       [&](std::size_t index, std::string_view) -> core::Result<bool> {
                           CHECK((index == 4 || index == 6));
                           ++lookups;
                           return true;
                       });
    REQUIRE(c);
    CHECK(c->state.cursor == 3);
    REQUIRE(c->state.active.size() == 2);
    CHECK(c->state.active.front().ordinal == 1);
    CHECK(c->writes.size() == 2);
    CHECK(lookups == 2);
    for (auto [error, target] : {std::pair{-1, "early"}, std::pair{1, "late"}}) {
        auto fact = f.fact();
        fact.error = judgement::TickDelta{error};
        auto timed = f.project(*p, {}, 0, {fact});
        REQUIRE(timed);
        CHECK(timed->writes.size() == 2);
        CHECK(timed->writes.contains(target));
    }
    auto absent = f.fact();
    absent.error.reset();
    auto miss = f.project(*p, {}, 0, {absent});
    REQUIRE(miss);
    CHECK(miss->writes.size() == 1);
}

TEST_CASE("PD projection retains alias windows and expires without new Facts",
          "[pd][projection][lifetime]") {
    AdapterFixture f;
    auto one = f.binding();
    one.start = {10};
    one.end = GameplayPresentationTick{20};
    auto two = one;
    two.start = {30};
    two.end.reset();
    auto p = PresentationProjectionAdapter::prepare(std::array{two, one, one}, {{"target", 0}});
    REQUIRE(p);
    REQUIRE(p->rows.size() == 2);
    auto c = f.project(*p, {}, 0, {f.fact()});
    REQUIRE(c);
    CHECK(c->state.future.size() == 2);
    CHECK(c->writes.empty());
    auto active = f.project(*p, c->state, 10);
    REQUIRE(active);
    CHECK(active->state.cursor == 1);
    CHECK(active->writes.size() == 1);
    CHECK(active->state.future.size() == 1);
    auto expired = f.project(*p, active->state, 20);
    REQUIRE(expired);
    CHECK(expired->state.active.empty());
    CHECK(expired->state.seenMembers.front());
    auto later = f.project(*p, expired->state, 30);
    REQUIRE(later);
    CHECK(later->state.future.empty());
    CHECK(later->writes.size() == 1);
    CHECK_FALSE(f.project(*p, later->state, 29));
    auto missing =
        f.project(*p, later->state, 31, {},
                  [](std::size_t, std::string_view) -> core::Result<bool> { return false; });
    REQUIRE(missing);
    CHECK(missing->writes.empty());
    CHECK(hasDiagnostic(missing->diagnostics, "presentation-target-missing"));
}

TEST_CASE("PD projection separates group memory from expiry and stable rejection",
          "[pd][projection][group]") {
    for (auto aggregation :
         {GameplayAggregation::Any, GameplayAggregation::All, GameplayAggregation::GroupCommit}) {
        AdapterFixture f;
        auto one = f.binding("one");
        auto two = f.binding("two", "missing");
        one.groupMembers = two.groupMembers = {"two", "one"};
        one.aggregation = two.aggregation = aggregation;
        one.end = GameplayPresentationTick{10};
        two.phase = GameplayPhase::Head;
        auto future = one;
        future.start = {20};
        future.end.reset();
        auto p =
            PresentationProjectionAdapter::prepare(std::array{future, two, one}, {{"target", 0}});
        REQUIRE(p);
        auto first = f.project(*p, {}, 0, {f.fact()});
        REQUIRE(first);
        CHECK(first->writes.size() == (aggregation == GameplayAggregation::Any ? 1 : 0));
        auto head = f.fact(1);
        head.phase = judgement::PhaseKind::head;
        auto completed = f.project(*p, first->state, 1, {head});
        REQUIRE(completed);
        CHECK(completed->writes.size() == 1);
        CHECK(hasDiagnostic(completed->diagnostics, "presentation-target-missing"));
        CHECK(hasDiagnostic(completed->diagnostics, "partial-group") ==
              (aggregation == GameplayAggregation::GroupCommit));
        auto later = f.project(*p, completed->state, 20);
        REQUIRE(later);
        CHECK(later->state.seenMembers[0]);
        CHECK(later->state.seenMembers[1]);
        CHECK(later->writes.size() == (aggregation == GameplayAggregation::GroupCommit ? 0 : 1));
        // A fresh scope discards rejection/seen state and can rebuild from the same prefix.
        auto rebuilt = f.project(*p, {}, 20, {f.fact(), head});
        REQUIRE(rebuilt);
        CHECK(rebuilt->writes.size() == 1);
        // All targets absent is missing, never a fictitious partial commit.
        auto absent = PresentationProjectionAdapter::prepare(std::array{one, two}, {});
        REQUIRE(absent);
        auto allMissing = f.project(*absent, {}, 1, {f.fact(), head});
        REQUIRE(allMissing);
        CHECK(allMissing->writes.empty());
        CHECK_FALSE(hasDiagnostic(allMissing->diagnostics, "partial-group"));
        // The first member expires before the last source arrives. It still completes the group.
        auto lateSource = f.project(*p, first->state, 20, {head});
        REQUIRE(lateSource);
        CHECK(lateSource->writes.size() == 1);
        CHECK(lateSource->state.matchedGroupMembers.front() == 2);
    }
}

TEST_CASE("PD projection rejects unsupported records and contradictory writes atomically",
          "[pd][projection][failure]") {
    AdapterFixture f;
    auto row = f.binding();
    auto p = PresentationProjectionAdapter::prepare(std::array{row}, {{"target", 0}});
    REQUIRE(p);
    for (int field = 0; field < 3; ++field) {
        auto unsupported = f.fact();
        if (field == 0)
            unsupported.kind = static_cast<judgement::FactKind>(255);
        if (field == 1)
            unsupported.phase = static_cast<judgement::PhaseKind>(255);
        if (field == 2)
            unsupported.outcome = static_cast<judgement::Outcome>(255);
        ProjectionState current;
        CHECK_FALSE(f.project(*p, current, 0, {f.fact(), unsupported}));
        CHECK(current.cursor == 0);
        CHECK_FALSE(current.scope);
        CHECK(current.active.empty());
        CHECK(current.future.empty());
        CHECK(current.seenMembers.empty());
        auto retry = f.project(*p, current, 0, {f.fact()});
        REQUIRE(retry);
        CHECK(retry->state.cursor == 1);
    }
    for (int field = 0; field < 4; ++field) {
        auto unsupported = row;
        if (field == 0)
            unsupported.phase = static_cast<GameplayPhase>(255);
        if (field == 1)
            unsupported.outcome = static_cast<GameplayOutcome>(255);
        if (field == 2)
            unsupported.timing = static_cast<GameplayTimingClass>(255);
        if (field == 3)
            unsupported.aggregation = static_cast<GameplayAggregation>(255);
        CHECK_FALSE(PresentationProjectionAdapter::prepare(std::array{unsupported}, {}));
    }
    row.visible = true;
    auto conflicting =
        PresentationProjectionAdapter::prepare(std::array{row, f.binding()}, {{"target", 0}});
    REQUIRE(conflicting);
    CHECK_FALSE(f.project(*conflicting, {}, 0, {f.fact()}));
    auto invalidated =
        f.project(*p, {}, 0, {f.fact()}, [](std::size_t, std::string_view) -> core::Result<bool> {
            return core::unexpected(core::Error{"test.target", "rejected"});
        });
    REQUIRE_FALSE(invalidated);
    CHECK(invalidated.error().code() == "test.target");
}

TEST_CASE("PD projection refuses stale scope and prefix without reading or installing",
          "[pd][projection][scope]") {
    AdapterFixture f;
    auto p = PresentationProjectionAdapter::prepare(std::array{f.binding()}, {{"target", 0}});
    REQUIRE(p);
    auto first = f.project(*p, {}, 0, {f.fact()});
    REQUIRE(first);
    auto wrongScope = first->state;
    wrongScope.scope = std::make_shared<const std::uint8_t>(0);
    CHECK_FALSE(f.project(*p, wrongScope, 1));
    CHECK(wrongScope.cursor == 1);
    CHECK(wrongScope.active.front().published);
    auto scope = judgement::detail::PublicationAccess::scope(f.kernel);
    REQUIRE(scope);
    auto read = judgement::detail::PublicationAccess::readSince(f.kernel, *scope, 0);
    REQUIRE(read);
    auto metadata = read->metadata();
    REQUIRE(metadata);
    auto m = metadata->get();
    for (int fault = 0; fault < 3; ++fault) {
        auto bad = m;
        bad.fromCursor = first->state.cursor;
        bad.publishableCursor = first->state.cursor;
        if (fault == 0)
            bad.fromCursor = 0;
        if (fault == 1)
            bad.publishableCursor = 0;
        if (fault == 2)
            bad.mayProject = false;
        bool invoked = false;
        auto rejected = PresentationProjectionAdapter::project(
            *p, first->state, bad, {1},
            [&](std::uint64_t) -> core::Result<judgement::detail::PublicationFact> {
                invoked = true;
                return f.fact();
            },
            [](std::size_t, std::string_view) -> core::Result<bool> { return true; });
        CHECK_FALSE(rejected);
        CHECK_FALSE(invoked);
        CHECK(first->state.cursor == 1);
        CHECK(first->state.active.front().published);
    }
}

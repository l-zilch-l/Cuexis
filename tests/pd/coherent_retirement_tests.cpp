#include "../../app/execution_host/session_worker.hpp"
#include "../rpa/execution_fixture.hpp"
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cuexis/presentation_renderer/presentation_renderer.hpp>
using namespace cuexis;
using namespace cuexis::playback;
using namespace cuexis::playback::detail;
using namespace cuexis::test_support;
namespace {
template <class Session>
concept PublicAdvanceRetention = requires(const Session& session, GameplayExecutionContext& context,
                                          std::optional<std::uint64_t>& value) {
    session.executionAdvanceRetention(context, value, value);
};
static_assert(!PublicAdvanceRetention<PlaybackSession>);
template <class Job> auto invoke(SessionWorker& worker, Job job) -> core::Result<SessionReply> {
    auto message = std::make_unique<SessionCommand>(std::move(job));
    const auto id = worker.post(std::move(*message), true);
    REQUIRE(id);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
    for (;;) {
        auto result = worker.poll(*id);
        REQUIRE(result);
        if (*result) {
            auto value = std::move(**result);
            REQUIRE(worker.acknowledge(*id));
            return value;
        }
        REQUIRE(std::chrono::steady_clock::now() < deadline);
        worker.waitForReply(*id, std::chrono::milliseconds{1});
    }
}
TEST_CASE("PD SDK Seek transfers the accepted ID floor without retaining old results",
          "[pd][retirement][seek]") {
    SessionTestContent fixture;
    const ClockMappingConfiguration mapping{{0, 1}, {0, 1}, {0, 1}, {1, 1}};
    rpa(fixture, mapping);
    PlaybackSession session;
    fixture.load(session);
    const GameplayExecutionContext first{1, 1, 1};
    const SchedulingProfile profile{1000, 100, 2, "", true};
    REQUIRE(session.beginRealtimeGameplay(profile, mapping, first, "pd.seek"));
    ClockObservation clock{};
    clock.monotonicDomain = "pd.seek";
    clock.segment = clock.publicationSequence = 1;
    clock.validUntilMonotonicUs = 1000;
    clock.source = clock.presented = ClockFramePosition{20, 1000000};
    clock.playback = ClockPlaybackState::Playing;
    clock.quality = ClockQuality::Exact;
    REQUIRE(session.deliverGameplayCapture({first, 0, GameplayCapturePrefix{{20}}}, clock, 0));
    // Keep a terminal result unacknowledged: successful Seek ends the old execution instance.
    const auto original = session.requestGameplayAdvance({first, 7, 0, {20}}, clock, 0);
    REQUIRE(original);
    REQUIRE(original->disposition == GameplayAdvanceDisposition::Executed);
    const auto before = HostCoordinationAccess::advanceRetention(session);
    REQUIRE(before);
    CHECK(before->lastAcceptedId == 7);
    CHECK_FALSE(before->reclaimedThrough);
    REQUIRE_FALSE(session.seekGameplay(GameplayReplay{}, {20}, {20}, {0.02, 0, 1}));
    REQUIRE(session.queryGameplayAdvance(7));
    const auto archive = session.archiveGameplay();
    REQUIRE(archive);
    REQUIRE(session.seekGameplay(*archive, {20}, {20}, {0.02, 0, 1}));
    const GameplayExecutionContext next{1, 2, 2};
    REQUIRE(session.beginRealtimeGameplay(profile, mapping, next, "pd.seek"));
    const auto after = HostCoordinationAccess::advanceRetention(session);
    REQUIRE(after);
    CHECK(after->identityContext == next);
    CHECK(after->lastAcceptedId == 7);
    CHECK(after->reclaimedThrough == 7);
    clock.segment = 2;
    const auto old = session.requestGameplayAdvance({next, 7, 0, {20}}, clock, 2000);
    REQUIRE_FALSE(old);
    CHECK(old.error().code() == "host.rpa.request_retired");
    REQUIRE(session.retireGameplayAdvance(7));
    REQUIRE(session.retireGameplayAdvance(7));
    REQUIRE(session.deliverGameplayCapture({next, 0, GameplayCapturePrefix{{20}}}, clock, 0));
    for (std::uint64_t i = 0; i < 8; ++i) {
        const auto result = session.requestGameplayAdvance({next, 8 + i, i, {20}}, clock, 0);
        REQUIRE(result);
        REQUIRE(result->disposition == GameplayAdvanceDisposition::Executed);
        REQUIRE(session.retireGameplayAdvance(8 + i));
    }
}
template <class Job> auto send(SessionWorker& worker, Job job) -> SessionReply {
    auto value = invoke(worker, std::move(job));
    INFO((value ? ""
                : std::string{value.error().code()} + ": " + std::string{value.error().message()}));
    REQUIRE(value);
    return std::move(*value);
}
struct Loaded final {
    PresentationCandidateInput input;
    HostCoordinator host;
};
auto load(SessionWorker& worker, PlaybackSource source,
          std::optional<GameplayContent> gameplay = {}) -> Loaded {
    presentation_renderer::TestPresentationRenderer renderer;
    SessionPrepareSource prepare;
    prepare.source = std::move(source);
    prepare.capabilities = renderer.capabilities();
    prepare.rendererInstance = renderer.identity().instance;
    prepare.rendererGeneration = renderer.identity().generation;
    prepare.controlSequence = 1;
    prepare.gameplay = std::move(gameplay);
    auto input = std::get<PresentationCandidateInput>(send(worker, std::move(prepare)));
    auto initial = input.context;
    initial.controlSequence = 0;
    initial.resourceGeneration = 0;
    initial.contentIdentity = {};
    auto host = HostCoordinator::create(initial, 16);
    REQUIRE(host);
    ControlRequest load;
    load.context = initial;
    load.context.controlSequence = 1;
    load.operationId = 1;
    load.kind = ControlKind::load;
    load.participants = {false, true, false, false};
    load.target.context = load.context;
    load.target.nextContext = input.context;
    load.target.candidateId = input.candidateId;
    load.target.participantGenerations[1] = input.mutationGeneration;
    auto decision = std::get<std::uint64_t>(send(worker, SessionAcceptControl{load}));
    CHECK(std::get<std::uint64_t>(send(worker, SessionAcceptControl{load})) == decision);
    REQUIRE(host->accept(load, decision));
    StepRequest request{{initial.sessionScope, 1, Participant::software, StepPhase::prepare, 0},
                        load.target};
    auto prepared = std::get<StepReceipt>(send(worker, SessionSoftwarePrepare{request}));
    REQUIRE(host->observe(prepared));
    request.key.phase = StepPhase::fence;
    auto fenced =
        std::get<StepReceipt>(send(worker, SessionSoftwareFence{request, *host->query(1)}));
    REQUIRE(host->observe(fenced));
    request.key.phase = StepPhase::softwareCommit;
    auto committed =
        std::get<StepReceipt>(send(worker, SessionSoftwareCommit{request, *host->query(1)}));
    REQUIRE(committed.status == StepStatus::applied);
    REQUIRE(host->observe(committed));
    REQUIRE(host->markSoftwareApplied());
    REQUIRE(host->finish());
    send(worker, SessionConfirmControl{*host->query(1)});
    return {std::move(input), std::move(*host)};
}
} // namespace
TEST_CASE("PD coherent owner recycles only scheduler-reclaimed identities",
          "[pd][retirement][worker]") {
    SessionTestContent fixture;
    const ClockMappingConfiguration mapping{{0, 1}, {0, 1}, {0, 1}, {1, 1}};
    rpa(fixture, mapping);
    std::atomic<std::int64_t> now{};
    SessionWorkerConfiguration configuration{32, 8,  64,   16,
                                             16, 64, true, [&] { return now.load(); }};
    auto created = SessionWorker::create(configuration);
    REQUIRE(created);
    auto& worker = **created;
    const auto loaded = load(worker, presentation(), fixture.content());
    const GameplayExecutionContext execution{loaded.input.context.sessionScope, 1, 1};
    send(worker, SessionRealtimeStart{{1000, 100, 2, "", true}, mapping, execution, "pd.retire"});
    ClockObservation clock{};
    clock.monotonicDomain = "pd.retire";
    clock.segment = clock.publicationSequence = 1;
    clock.validUntilMonotonicUs = 1000;
    clock.source = clock.presented = ClockFramePosition{20, 1000000};
    clock.playback = ClockPlaybackState::Playing;
    clock.quality = ClockQuality::Exact;
    const auto prefix = SessionCaptureDelivery{{execution, 0, GameplayCapturePrefix{{20}}}, clock};
    SECTION("more than capacity retirements and expired old retransmission") {
        send(worker, prefix);
        for (std::uint64_t id = 1; id <= 64; ++id) {
            const auto request = SessionCoherentAdvance{execution, id, id, clock};
            const auto result = std::get<GameplayAdvanceDecision>(send(worker, request));
            REQUIRE(result.disposition == GameplayAdvanceDisposition::Executed);
            const auto duplicate = std::get<GameplayAdvanceDecision>(send(worker, request));
            CHECK(duplicate.decisionOrdinal == result.decisionOrdinal);
            send(worker, SessionRetireAdvance{id});
            send(worker, SessionRetireAdvance{id});
            const auto query = invoke(worker, SessionQueryAdvance{id});
            REQUIRE_FALSE(query);
            CHECK(query.error().code() == "host.rpa.request_retired");
            now = 1001;
            const auto old = invoke(worker, request);
            REQUIRE_FALSE(old);
            CHECK(old.error().code() == "host.rpa.request_retired");
            const auto direct = invoke(worker, SessionAdvance{{execution, id, id, {20}}, clock});
            REQUIRE_FALSE(direct);
            CHECK(direct.error().code() == "host.rpa.request_retired");
            now = 0;
        }
    }
    SECTION("failed retirement keeps pending payload and result") {
        const auto request = SessionCoherentAdvance{execution, 7, 1, clock};
        REQUIRE(std::get<GameplayAdvanceDecision>(send(worker, request)).disposition ==
                GameplayAdvanceDisposition::Waiting);
        REQUIRE_FALSE(invoke(worker, SessionRetireAdvance{7}));
        REQUIRE(std::get<GameplayAdvanceDecision>(send(worker, request)).disposition ==
                GameplayAdvanceDisposition::Waiting);
        send(worker, prefix);
        REQUIRE(
            std::get<GameplayAdvanceDecision>(send(worker, SessionQueryAdvance{7})).disposition ==
            GameplayAdvanceDisposition::Executed);
        send(worker, SessionRetireAdvance{7});
        REQUIRE(std::get<GameplayAdvanceDecision>(
                    send(worker, SessionCoherentAdvance{execution, 11, 2, clock}))
                    .disposition == GameplayAdvanceDisposition::Executed);
    }
    SECTION("out of order retirement retains hole payload until prefix closes") {
        send(worker, prefix);
        send(worker, SessionCoherentAdvance{execution, 7, 1, clock});
        const auto request = SessionCoherentAdvance{execution, 11, 2, clock};
        send(worker, request);
        send(worker, SessionRetireAdvance{11});
        REQUIRE(std::get<GameplayAdvanceDecision>(send(worker, request)).disposition ==
                GameplayAdvanceDisposition::Retired);
        const auto blocked = invoke(worker, SessionCoherentAdvance{execution, 19, 3, clock});
        REQUIRE_FALSE(blocked);
        CHECK(blocked.error().code() == "host.rpa.capacity_exceeded");
        send(worker, SessionRetireAdvance{7});
        REQUIRE_FALSE(invoke(worker, request));
        REQUIRE(std::get<GameplayAdvanceDecision>(
                    send(worker, SessionCoherentAdvance{execution, 19, 3, clock}))
                    .disposition == GameplayAdvanceDisposition::Executed);
    }
    SECTION("unacknowledged coherent payload conflicts remain rejected") {
        send(worker, prefix);
        auto request = SessionCoherentAdvance{execution, 7, 1, clock};
        send(worker, request);
        ++request.clock.publicationSequence;
        const auto conflict = invoke(worker, request);
        REQUIRE_FALSE(conflict);
        CHECK(conflict.error().code() == "host.rpa.message_conflict");
    }
}

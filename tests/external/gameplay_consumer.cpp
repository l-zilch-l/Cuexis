// Independent installed-tree consumer. No internal execution headers or test framework.
#include <cuexis/playback/content_provider.hpp>
#include <cuexis/playback/gameplay_candidate.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/playback/playback_source.hpp>

#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

#ifndef CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE
#error The Gameplay surface requires an explicit experimental SDK flavor.
#endif

namespace {
using namespace cuexis::playback;
template <typename T> auto take(cuexis::core::Result<T> result) -> T {
    if (!result)
        throw std::runtime_error{std::string{result.error().code()} + ": " +
                                 std::string{result.error().message()}};
    return std::move(*result);
}
void take(cuexis::core::Result<void> result) {
    if (!result)
        throw std::runtime_error{std::string{result.error().code()} + ": " +
                                 std::string{result.error().message()}};
}
void expect(bool value, const char* message) {
    if (!value)
        throw std::runtime_error{message};
}
void expectSameError(const cuexis::core::Error& actual, const cuexis::core::Error& expected) {
    const auto* a = &actual;
    const auto* b = &expected;
    while (a && b) {
        expect(a->code() == b->code() && a->message() == b->message() &&
                   a->context().size() == b->context().size(),
               "Installed complete error differs");
        for (std::size_t i = 0; i < a->context().size(); ++i)
            expect(a->context()[i].key == b->context()[i].key &&
                       a->context()[i].value == b->context()[i].value,
                   "Installed ordered error context differs");
        a = a->cause();
        b = b->cause();
    }
    expect(!a && !b, "Installed error cause chain differs");
}
auto fixtureConfiguration() -> GameplayConfiguration {
    GameplayConfiguration c;
    c.engine = {"judgement.t4-k4.v1",          "fact.semantic.phase-local.v1",
                "fixed-point.none.v1",         "coordination.six-eight.t4-k4.v1",
                "gameplay.execution.t4-k4.v1", "late.window.logical.v1"};
    c.rulesetInterfaceProjection = "interface.projection.one";
    c.rulesetBuild = "ruleset.build.hash.one";
    c.rulesetModuleOrder = {"module.one"};
    c.session = {"loadout.one", "grace.source.one", "normalization.one", "judgement.config.one"};
    c.enabledCapabilities = {"cuexis.gameplay.t4-k4.v1"};
    c.mappingId = "mapping";
    c.mappingVersion = "v1";
    c.sourceClass = "keyboard";
    c.calibration = "calibration.fixture";
    c.domains = {{"domain.binding.one", {1, 1}, -100, 100, true}};
    c.loadout = "loadout.fixture";
    c.actualRulesetInterface = "cuexis.ruleset.finite";
    c.actualRulesetRevision = "1";
    c.actualRulesetBuild = "cuexis.finite-fold.1";
    c.modules = {{"score", "1", "cuexis.finite-fold.1"},
                 {"combo", "1", "cuexis.finite-fold.1"},
                 {"statistics", "1", "cuexis.finite-fold.1"}};
    c.programPolicy = GameplayProgramPolicy::Locked;
    c.outcomeScope = GameplayOutcomeScope::Declared;
    c.arithmetic = GameplayArithmetic::Checked;
    c.hooks = {};
    c.externalPackage = false;
    c.life = false;
    c.initialScore = 0;
    c.minimumScore = std::numeric_limits<std::int64_t>::min();
    c.maximumScore = std::numeric_limits<std::int64_t>::max();
    c.initialCombo = 0;
    c.scoreRules = {{GameplayPhase::Tap, GameplayOutcome::Hit, {}, 2, true},
                    {GameplayPhase::Tap, GameplayOutcome::Miss, {}, -1, false}};
    c.bindings = {};
    c.testOnly = true;
    return c;
}
} // namespace
int main(int argc, char** argv) try {
    if (argc != 2)
        throw std::runtime_error{"Expected the compiled test fixture path"};
    std::ifstream file{argv[1], std::ios::binary | std::ios::ate};
    const auto size = file.tellg();
    if (!file || size < 0 || size > 16 * 1024 * 1024)
        throw std::runtime_error{"Invalid bounded fixture"};
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!file)
        throw std::runtime_error{"Fixture read failed"};
    const auto configuration = fixtureConfiguration();
    const auto content = take(GameplayContent::fromPacked(bytes, configuration));
    auto invalidConfiguration = configuration;
    invalidConfiguration.mappingId.clear();
    const auto invalidContent = GameplayContent::fromPacked(bytes, invalidConfiguration);
    expect(!invalidContent && invalidContent.error().code() == "playback.gameplay.invalid",
           "Installed internal diagnostic projection differs");
    bool sourceFound = false;
    for (const auto& field : invalidContent.error().context())
        if (field.key == "sourceCode" &&
            field.value == "judgement.input.mapping_declaration_invalid")
            sourceFound = true;
    expect(sourceFound, "Installed sourceCode did not use the formal internal reason");

    auto provider = take(MemoryContentProvider::create({}));
    auto source = take(PlaybackSource::fromGameplayPacked(
        "installed-fixture", "compiled/main.packed", bytes, configuration, {}, provider,
        GameplayPrepareIntent::GameplayOnly));
    PlaybackSession session{{.version = 1, .ids = {}}};
    auto prepared = take(session.prepareLoad(std::move(source), PlaybackMode::ChartClock));
    take(session.commit(std::move(prepared)));
    expect(take(take(session.queryGameplay()).factCount()) == 0, "Unexpected initial Fact");
    const GameplayInput event{
        {5}, 1, GameplayInputAction::Press, "lane.one", "domain.binding.one", "keyboard", {}};
    const auto initialResult = take(session.queryGameplay());
    const GameplayInput conflicting[] = {
        event,
        {{5}, 2, GameplayInputAction::Release, "lane.one", "domain.binding.one", "keyboard", {}}};
    const auto rejected = session.submitGameplay(conflicting);
    expect(!rejected && rejected.error().code() == "judgement.input.same_tick_collision",
           "Installed collision reason differs");
    expect(take(initialResult.sameResult(take(session.queryGameplay()))),
           "Rejected batch changed the full result");
    take(session.submitGameplay(std::span{&event, 1}));
    const auto pending = take(session.snapshotGameplay());
    take(session.advanceGameplay({5}, {5}, {5, 0, 0}));
    expect(take(take(session.queryGameplay()).factCount()) == 0, "Exact H bypassed F(H)");
    take(session.advanceGameplay({20}, {20}, {20, 0, 0}));
    const auto golden = take(session.queryGameplay());
    const auto score = take(golden.score());
    expect(score.score == 2 && score.combo == 1 && score.hits == 1 && score.misses == 0,
           "Manual Hit golden differs");
    auto replay = take(session.archiveGameplay());
    const auto evaluation = take(session.evaluateGameplayReplay(replay));
    expect(evaluation.evidenceValid && take(golden.sameResult(evaluation.result)),
           "Complete Replay differs");
    const GameplayCodecBudget budget{1024 * 1024, 100, 100000, true};
    auto oldReplayBytes = take(replay.toBytes());
    auto oldSnapshotBytes = take(pending.toBytes());
    expect(oldReplayBytes[8] == std::byte{2} && oldSnapshotBytes[8] == std::byte{2},
           "Installed Writer did not emit revision2");
    oldReplayBytes[8] = std::byte{1}; // artificial valid body with old envelope version
    oldSnapshotBytes[8] = std::byte{1};
    auto oldReplay = GameplayReplay::fromBytes(oldReplayBytes, content, budget);
    auto oldSnapshot = GameplaySnapshot::fromBytes(oldSnapshotBytes, content, budget);
    expect(!oldReplay && oldReplay.error().code() == "codec.version_unsupported",
           "Installed Replay Reader accepted revision1");
    expect(!oldSnapshot && oldSnapshot.error().code() == "codec.version_unsupported",
           "Installed Snapshot Reader accepted revision1");
    expect(take(golden.sameResult(take(session.queryGameplay()))),
           "Failed Reader modified session result");
    auto archive = take(GameplayReplay::fromBytes(take(replay.toBytes()), content, budget));
    auto snapshot = take(GameplaySnapshot::fromBytes(take(pending.toBytes()), content, budget));
    const auto checkpoint = take(session.checkpointGameplay(archive, {20}));
    take(session.seekGameplay(archive, {20}, take(archive.cutAt({20})), std::span{&checkpoint, 1},
                              {20}, {20, 0, 1}));
    expect(take(golden.sameResult(take(session.queryGameplay()))), "Certified Seek differs");
    take(session.restoreGameplay(snapshot, {0}, {0, 0, 2}));
    const auto restoredResult = take(session.queryGameplay());
    const auto unavailableHistory = session.archiveGameplay();
    expect(!unavailableHistory &&
               unavailableHistory.error().code() == "judgement.session.operation_state_invalid",
           "Restored owning state fabricated a complete Replay journal");
    expect(unavailableHistory.error().message() ==
               "the judgement session is not in the lifecycle phase this verb requires",
           "Historical lifecycle message changed");
    const auto& operationContext = unavailableHistory.error().context();
    expect(operationContext.size() == 5 && operationContext[0].key == "category" &&
               operationContext[0].value == "invalid_relation" &&
               operationContext[1].key == "severity" && operationContext[1].value == "error" &&
               operationContext[2].key == "faulted" && operationContext[2].value == "false" &&
               operationContext[3].key == "field.section" &&
               operationContext[3].value == "session" && operationContext[4].key == "field.path" &&
               operationContext[4].value == "query" && !unavailableHistory.error().cause(),
           "Restored archive rejection lost complete metadata");
    expect(take(restoredResult.sameResult(take(session.queryGameplay()))),
           "Unavailable journal query changed restored owning state");

    take(session.advanceGameplay({20}, {20}, {20, 0, 2}));
    expect(take(golden.sameResult(take(session.queryGameplay()))),
           "Pending Snapshot reconstruction differs");
    take(session.controlGameplay(GameplayControl::Pause));
    expect(!session.submitGameplay(std::span{&event, 1}), "Paused submit accepted");
    take(session.controlGameplay(GameplayControl::Resume));
    take(session.controlGameplay(GameplayControl::Stop));
    take(session.controlGameplay(GameplayControl::Resume));
    take(session.advanceGameplay({20}, {20}, {20, 0, 2}));
    expect(take(take(session.queryGameplay()).score()).misses == 1,
           "Explicit zero-input Gameplay lost Miss");
    take(session.controlGameplay(GameplayControl::Reset));
    take(session.unload());
    expect(take(golden.score()).score == 2 && replay.valid() && snapshot.valid() &&
               checkpoint.valid(),
           "Owning values expired with session");
    expect(!session.extractFrame({640, 480}), "GameplayOnly unexpectedly created a World");
    auto faultConfiguration = configuration;
    faultConfiguration.maximumScore = 1;
    auto faultProvider = take(MemoryContentProvider::create({}));
    auto faultSource = take(PlaybackSource::fromGameplayPacked(
        "installed-fault-fixture", "compiled/main.packed", bytes, faultConfiguration, {},
        faultProvider, GameplayPrepareIntent::GameplayOnly));
    PlaybackSession faultSession;
    take(faultSession.load(std::move(faultSource), PlaybackMode::ChartClock));
    take(faultSession.submitGameplay(std::span{&event, 1}));
    const auto failedAdvance = faultSession.advanceGameplay({20}, {20}, {20, 0, 0});
    expect(!failedAdvance && failedAdvance.error().code() == "ruleset.transaction_failed",
           "Installed real Fold failure differs");
    const auto faultReceipt = take(faultSession.gameplayAdvanceReceipt());
    expect(faultReceipt.error.has_value(), "Installed fault receipt lost error");
    expectSameError(*faultReceipt.error, failedAdvance.error());
    expect(faultReceipt.admitted && faultReceipt.faultStage == GameplayFaultStage::Fold &&
               !faultReceipt.publicationFailureStage && !faultReceipt.runtimeUpdated &&
               faultReceipt.sealedFacts == 1 && faultReceipt.publishableFacts == 0,
           "Installed fault receipt lost execution phase or sealed prefix");
    const auto faultResult = take(faultSession.queryGameplay());
    auto faultArchive = take(faultSession.archiveGameplay());
    const auto faultEvaluation = take(faultSession.evaluateGameplayReplay(faultArchive));
    expect(faultEvaluation.evidenceValid && take(faultResult.sameResult(faultEvaluation.result)),
           "Installed full fault Replay differs");
    expectSameError(*take(faultSession.gameplayAdvanceReceipt()).error, failedAdvance.error());
    std::cout << "Installed GameplayOnly: prepare/submit/F(H)/query/full "
                 "Replay/W2/Snapshot/certified Seek/pause/Stop/reset/lifetime passed\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 2;
}

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
    auto archive = take(GameplayReplay::fromBytes(take(replay.toBytes()), content, budget));
    auto snapshot = take(GameplaySnapshot::fromBytes(take(pending.toBytes()), content, budget));
    const auto checkpoint = take(session.checkpointGameplay(archive, {20}));
    take(session.seekGameplay(archive, {20}, take(archive.cutAt({20})), std::span{&checkpoint, 1},
                              {20}, {20, 0, 1}));
    expect(take(golden.sameResult(take(session.queryGameplay()))), "Certified Seek differs");
    take(session.restoreGameplay(snapshot, {0}, {0, 0, 2}));
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
    std::cout << "Installed GameplayOnly: prepare/submit/F(H)/query/full "
                 "Replay/W2/Snapshot/certified Seek/pause/Stop/reset/lifetime passed\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 2;
}

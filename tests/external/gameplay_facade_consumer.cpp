// Independent installed-tree consumer of the experimental Playback Gameplay facade. It builds two
// independent legacy experimental sessions from the same inputs: one drives the old members and the
// installed reconciliation-identity free function, the other drives only the experimental facade
// entries. Both must reach the same identity, the same Paused/Resume transition and the same
// Running state; a tampered reconciliation must be rejected while the session stays Paused.
#include <cuexis/playback/content_provider.hpp>
#include <cuexis/playback/execution_control.hpp>
#include <cuexis/playback/experimental/gameplay.hpp>
#include <cuexis/playback/experimental/gameplay_realtime.hpp>
#include <cuexis/playback/experimental/gameplay_replay.hpp>
#include <cuexis/playback/gameplay_candidate.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/playback/playback_source.hpp>
#include <cuexis/playback/realtime_clock.hpp>

#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <vector>

#if !defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
#error The experimental Playback facade requires the candidate SDK flavor.
#endif

namespace {

using namespace cuexis::playback;

// F310 identity and layout checks against the installed declarations: the facade keeps the same
// return types and enum values, and the shared identity value keeps its 32-byte layout.
static_assert(sizeof(PreparedSemanticIdentity) == 32);
static_assert(static_cast<int>(SessionState::Empty) == 0);
static_assert(static_cast<int>(SessionState::Ready) == 1);
static_assert(static_cast<int>(SessionState::Running) == 2);
static_assert(static_cast<int>(SessionState::Failed) == 3);
static_assert(std::is_same_v<decltype(&experimental::archiveGameplay),
                             cuexis::core::Result<GameplayReplay> (*)(const PlaybackSession&)>);
static_assert(
    std::is_same_v<decltype(&experimental::prepareGameplayLoad),
                   cuexis::core::Result<PreparedPlayback> (*)(
                       PlaybackSession&, PlaybackSource&&, PlaybackMode, const GameplayContent&)>);
static_assert(
    std::is_same_v<decltype(&experimental::resumeRealtimeGameplay),
                   cuexis::core::Result<void> (*)(
                       PlaybackSession&, const GameplayReconciliation&, GameplayExecutionContext,
                       const ClockObservation&, std::int64_t, bool, std::optional<std::int64_t>)>);
static_assert(std::is_same_v<
              decltype(&experimental::computeGameplayReconciliationIdentity),
              cuexis::core::Result<PreparedSemanticIdentity> (*)(const GameplayReconciliation&)>);

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
auto readBounded(const char* path) -> std::vector<std::byte> {
    std::ifstream file{path, std::ios::binary | std::ios::ate};
    const auto size = file.tellg();
    if (!file || size < 0 || size > 16 * 1024 * 1024)
        throw std::runtime_error{"Invalid bounded fixture"};
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!file)
        throw std::runtime_error{"Fixture read failed"};
    return bytes;
}
auto readText(const char* path) -> std::string {
    std::ifstream file{path};
    if (!file)
        throw std::runtime_error{"Configuration fixture missing"};
    return std::string{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
}
auto realtimeConfiguration(const GameplayConfiguration& decoded,
                           const ClockMappingConfiguration& mapping) -> GameplayConfiguration {
    auto configuration = decoded;
    configuration.calibration = take(RealtimeClock::calibrationIdentity(mapping));
    configuration.engine[1] = "fact.semantic.rpa.v1";
    configuration.engine[4] = "gameplay.execution.t4-k4.rpa.v1";
    configuration.engine[5] = "late.window.batch.v1";
    configuration.enabledCapabilities = {"cuexis.gameplay.rpa.v1"};
    return configuration;
}
auto realtimeClock() -> ClockObservation {
    return ClockObservation{"facade.clock",
                            1,
                            1,
                            0,
                            1000,
                            0,
                            ClockFramePosition{20, 1000000},
                            ClockFramePosition{20, 1000000},
                            {},
                            ClockPlaybackState::Playing,
                            ClockQuality::Exact,
                            false,
                            false,
                            false};
}

struct Fixture final {
    std::vector<std::byte> realtime;
    GameplayConfiguration configuration;
    ClockMappingConfiguration mapping{{0, 1}, {0, 1}, {0, 1}, {1, 1}};
    GameplayInput event{{5},        1, GameplayInputAction::Press, "lane.one", "domain.binding.one",
                        "keyboard", {}};
    GameplayExecutionContext legacyScope{7, 1, 1};
    GameplayExecutionContext facadeScope{7, 1, 1};
    ClockObservation clock{realtimeClock()};
    GameplayReconciliation reconciliation{{21}, 0, 1, 0, {}};
    GameplayExecutionContext resumed{7, 2, 2};
};

// Drives one independent legacy experimental session to the paused state using the old members.
void pauseLegacySession(const Fixture& f, PlaybackSession& session) {
    auto provider = take(MemoryContentProvider::create({}));
    auto source = take(PlaybackSource::fromGameplayPacked("facade-legacy", "compiled/main.packed",
                                                          f.realtime, f.configuration, {}, provider,
                                                          GameplayPrepareIntent::GameplayOnly));
    take(session.load(std::move(source), PlaybackMode::ChartClock));
    take(session.beginRealtimeGameplay({1000, 100, 64, "", true}, f.mapping, f.legacyScope,
                                       "facade.clock"));
    take(session.requestGameplayAdvance({f.legacyScope, 1, 0, {20}}, f.clock, 0));
    take(session.deliverGameplayCapture(
        {f.legacyScope, 0, GameplayClosedBatch{{5}, {6}, 0, 0, 1, 1, {{f.event, 0}}}}, f.clock, 0));
    take(session.deliverGameplayCapture({f.legacyScope, 1, GameplayCapturePrefix{{21}}}, f.clock,
                                        1));
    expect(take(session.queryGameplayAdvance(1)).disposition ==
               GameplayAdvanceDisposition::Executed,
           "Legacy verified prefix did not execute");
    take(session.acceptGameplayControl(f.legacyScope, 1));
    take(session.deliverGameplayCapture({f.legacyScope, 2, GameplayEpochEnd{{21}, 1}}, f.clock, 1));
    take(session.pauseRealtimeGameplay());
    expect(take(session.gameplayState()) == GameplayState::Paused, "Legacy session did not pause");
}

// Drives the matching independent session through the experimental facade only.
void pauseFacadeSession(const Fixture& f, PlaybackSession& session) {
    auto provider = take(MemoryContentProvider::create({}));
    auto source = take(experimental::fromGameplayPacked("facade-new", "compiled/main.packed",
                                                        f.realtime, f.configuration, {}, provider,
                                                        GameplayPrepareIntent::GameplayOnly));
    take(session.load(std::move(source), PlaybackMode::ChartClock));
    take(experimental::beginRealtimeGameplay(session, {1000, 100, 64, "", true}, f.mapping,
                                             f.facadeScope, "facade.clock"));
    take(experimental::requestGameplayAdvance(session, {f.facadeScope, 1, 0, {20}}, f.clock, 0));
    take(experimental::deliverGameplayCapture(
        session, {f.facadeScope, 0, GameplayClosedBatch{{5}, {6}, 0, 0, 1, 1, {{f.event, 0}}}},
        f.clock, 0));
    take(experimental::deliverGameplayCapture(
        session, {f.facadeScope, 1, GameplayCapturePrefix{{21}}}, f.clock, 1));
    expect(take(experimental::queryGameplayAdvance(session, 1)).disposition ==
               GameplayAdvanceDisposition::Executed,
           "Facade verified prefix did not execute");
    take(experimental::acceptGameplayControl(session, f.facadeScope, 1));
    take(experimental::deliverGameplayCapture(
        session, {f.facadeScope, 2, GameplayEpochEnd{{21}, 1}}, f.clock, 1));
    take(experimental::pauseRealtimeGameplay(session));
    expect(take(experimental::gameplayState(session)) == GameplayState::Paused,
           "Facade session did not pause");
}
} // namespace

int main(int argc, char** argv) try {
    if (argc != 4)
        throw std::runtime_error{
            "Expected the compiled fixture, the realtime fixture and the configuration fixture"};
    Fixture fixture;
    fixture.realtime = readBounded(argv[2]);
    const auto decoded = take(decodeGameplayConfiguration(
        readText(argv[3]), {4 * 1024 * 1024, 128, 1024 * 1024, 1024 * 1024, 1024 * 1024, true}));
    fixture.configuration = realtimeConfiguration(decoded, fixture.mapping);

    // Legacy path: old members plus the old installed identity function.
    PlaybackSession legacy;
    pauseLegacySession(fixture, legacy);
    const auto legacyIdentity = take(computeGameplayReconciliationIdentity(fixture.reconciliation));
    auto legacyClock = fixture.clock;
    legacyClock.segment = 2;
    legacyClock.presented = ClockFramePosition{22, 1000000};
    legacyClock.source = legacyClock.presented;
    take(legacy.resumeRealtimeGameplay(fixture.reconciliation, fixture.resumed, legacyClock, 1,
                                       true));
    expect(take(legacy.gameplayState()) == GameplayState::Running,
           "Legacy Resume did not reach Running");
    const auto legacyResult = take(legacy.queryGameplay());

    // Facade path: the same flow through the experimental entries only.
    PlaybackSession facade;
    pauseFacadeSession(fixture, facade);
    const auto facadeIdentity =
        take(experimental::computeGameplayReconciliationIdentity(fixture.reconciliation));
    expect(legacyIdentity == facadeIdentity, "Old and facade reconciliation identities differ");
    auto facadeClock = legacyClock;
    auto conflict = fixture.reconciliation;
    ++conflict.pauseEpoch;
    const auto conflictIdentity =
        take(experimental::computeGameplayReconciliationIdentity(conflict));
    expect(!(conflictIdentity == facadeIdentity),
           "Tampered reconciliation identity did not change");
    const auto rejected = experimental::resumeRealtimeGameplay(facade, conflict, fixture.resumed,
                                                               facadeClock, 1, true);
    expect(!rejected && rejected.error().code() == "host.rpa.context_stale",
           "Facade Resume accepted a tampered reconciliation");
    expect(take(experimental::gameplayState(facade)) == GameplayState::Paused,
           "Rejected Resume left the facade session unpaused");
    take(experimental::resumeRealtimeGameplay(facade, fixture.reconciliation, fixture.resumed,
                                              facadeClock, 1, true));
    expect(take(experimental::gameplayState(facade)) == GameplayState::Running,
           "Facade Resume did not reach Running");
    const auto facadeResult = take(experimental::queryGameplay(facade));
    expect(take(legacyResult.sameResult(facadeResult)),
           "Legacy and facade sessions produced different results");
    const auto facadeArchive = take(experimental::archiveGameplay(facade));
    expect(take(take(experimental::evaluateGameplayReplay(facade, facadeArchive))
                    .result.sameResult(facadeResult)),
           "Facade archive and evaluation differ");
    take(facade.unload());
    std::cout << "Experimental Playback facade Resume parity passed\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 2;
}

#include "host_realtime.hpp"
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
#include <charconv>
#include <cuexis/playback/playback_session.hpp>
#include <limits>
#include <stdexcept>
#include <type_traits>
namespace cuexis_reference_host {
namespace {
using namespace cuexis::playback;
template <class T> auto take(cuexis::core::Result<T> result) -> T {
    if (!result)
        throw std::runtime_error{std::string{result.error().code()}};
    if constexpr (!std::is_void_v<T>)
        return std::move(*result);
}
} // namespace
auto runRealtimeHost(const HostOptions& options, HostReport& report) -> int try {
    using namespace cuexis::playback;
    if (!options.gameplay || !options.candidateEntry || !options.executionTailDeadline ||
        options.gameplay->configuration.engine[4] != "gameplay.execution.t4-k4.rpa.v1" ||
        !options.gameplay->configuration.testOnly)
        throw std::runtime_error{"host.rpa.profile_incomplete"};
    const auto& gameplay = *options.gameplay;
    std::int64_t deadline{};
    const auto& text = *options.executionTailDeadline;
    auto parsed = std::from_chars(text.data(), text.data() + text.size(), deadline);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || deadline < 0)
        throw std::runtime_error{"host.rpa.profile_incomplete"};
    const GameplayGraphDecodeBudget graphBudget{
        4U * 1024U * 1024U, 64, 16384, 1000000, 100000, 100000, 4U * 1024U * 1024U};
    auto source =
        options.packageFile
            ? PlaybackSource::fromCxcFileGameplayEntry(
                  *options.packageFile, *options.candidateEntry, gameplay.configuration,
                  gameplay.budget, graphBudget, GameplayPrepareIntent::GameplayOnly)
            : PlaybackSource::fromFilesystemGameplayEntry(
                  options.contentDirectory, *options.candidateEntry, gameplay.configuration,
                  gameplay.budget, graphBudget, GameplayPrepareIntent::GameplayOnly);
    PlaybackSession session;
    take(session.load(take(std::move(source)), PlaybackMode::ChartClock));
    const ClockMappingConfiguration mapping{{0, 1}, {0, 1}, {0, 1}, {1, 1}};
    const GameplayExecutionContext initial{1, 1, 1};
    take(session.beginRealtimeGameplay(
        {1000000, 0, gameplay.budget.maxContainerElements, "reference.fixture.v1", true}, mapping,
        initial, "reference.fake.us"));
    const auto clock = [](std::int64_t tick, std::uint64_t publication, ClockPlaybackState state) {
        ClockObservation result{};
        result.monotonicDomain = "reference.fake.us";
        result.segment = 1;
        result.publicationSequence = publication;
        result.sampleMonotonicUs = tick;
        if (tick > INT64_MAX - 1000000)
            throw std::runtime_error{"host.rpa.clock_invalid"};
        result.validUntilMonotonicUs = tick + 1000000;
        result.source = ClockFramePosition{tick, 1000000};
        result.presented = result.source;
        result.playback = state;
        result.quality = ClockQuality::Exact;
        return result;
    };
    std::uint64_t sequence{}, batch{}, publication{};
    std::int64_t eofTick{};
    for (std::size_t i = 0; i < gameplay.observations.size();) {
        const auto tick = gameplay.observations[i].observationTick.value;
        if (tick < eofTick || tick == INT64_MAX)
            throw std::runtime_error{"host.rpa.message_conflict"};
        std::vector<GameplayOccurrence> members;
        while (i < gameplay.observations.size() &&
               gameplay.observations[i].observationTick.value == tick) {
            members.push_back({gameplay.observations[i], i});
            ++i;
        }
        eofTick = tick + 1;
        auto sample = clock(eofTick, ++publication, ClockPlaybackState::Playing);
        take(session.deliverGameplayCapture(
            {initial, sequence++,
             GameplayClosedBatch{
                 {tick}, {eofTick}, batch++, 0, 1, members.size(), std::move(members)}},
            sample, eofTick));
        take(session.deliverGameplayCapture({initial, sequence++, GameplayCapturePrefix{{eofTick}}},
                                            sample, eofTick));
    }
    const auto eof = clock(eofTick, ++publication, ClockPlaybackState::EndOfStream);
    auto transition = take(session.beginRealtimeTail(
        eof, 2, {{1, 1}, deadline, 0, "reference.declared.test-only", true}, eofTick));
    if (transition.terminalHorizon < eofTick)
        throw std::runtime_error{"host.rpa.clock_invalid"};
    const GameplayExecutionContext tail{1, 2, 1};
    const auto tailNow = transition.terminalHorizon;
    // The fixture clock is explicitly driven; no device or wall clock is impersonated.
    take(session.deliverGameplayCapture(
        {tail, sequence++, GameplayCapturePrefix{{transition.terminalHorizon}}}, eof, tailNow));
    auto decision = take(
        session.requestGameplayAdvance({tail, 1, 0, {transition.terminalHorizon}}, eof, tailNow));
    if (decision.disposition != GameplayAdvanceDisposition::Executed)
        throw std::runtime_error{"host.rpa.control_pending"};
    auto replay = take(session.archiveGameplay());
    auto evaluation = take(session.evaluateGameplayReplay(replay));
    if (!evaluation.evidenceValid || !take(decision.result.sameResult(evaluation.result)))
        throw std::runtime_error{"reference.replay_mismatch"};
    take(session.unload());
    take(replay.toBytes());
    report.event("execution",
                 "testOnly=true clock=explicit_fixture protocol=prefix-tail-replay terminalH=" +
                     std::to_string(transition.terminalHorizon));
    return 0;
} catch (const std::exception& failure) {
    report.diagnostic("execution", failure.what(), "Explicit realtime fixture refused");
    return 1;
}
} // namespace cuexis_reference_host
#endif

// Offline fixture compiler. Uses existing typed author lowering and Packed Chart validation.
#include <cuexis/chart/behavior_candidate.hpp>
#include <cuexis/chart/candidate_assembler.hpp>
#include <cuexis/chart/packed_chart_io.hpp>
#include <cuexis/json/parse.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/playback/realtime_clock.hpp>
#include <cuexis/tools/gameplay_author.hpp>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>

namespace {
template <class T> auto take(cuexis::core::Result<T> value) -> T {
    if (!value)
        throw std::runtime_error(std::string{value.error().code()} + ": " +
                                 std::string{value.error().message()});
    return std::move(*value);
}
auto read(const char* path) -> std::string {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    const auto size = in.tellg();
    if (!in || size < 0 || size > 16 * 1024 * 1024)
        throw std::runtime_error("Fixture file is unavailable or exceeds 16 MiB");
    std::string text(static_cast<std::size_t>(size), '\0');
    in.seekg(0);
    in.read(text.data(), static_cast<std::streamsize>(text.size()));
    if (!in)
        throw std::runtime_error("Fixture file read failed");
    return text;
}
void take(cuexis::core::Result<void> value) {
    if (!value)
        throw std::runtime_error(std::string{value.error().code()} + ": " +
                                 std::string{value.error().message()});
}
auto field(const cuexis::json::Value& value, std::string_view key) -> const cuexis::json::Value& {
    const auto* result = value.find(key);
    if (!result)
        throw std::runtime_error("Missing verification field");
    return *result;
}
auto integer(const cuexis::json::Value& value) -> std::int64_t {
    if (value.signedInteger())
        return *value.signedInteger();
    if (value.unsignedInteger() && *value.unsignedInteger() <= INT64_MAX)
        return static_cast<std::int64_t>(*value.unsignedInteger());
    throw std::runtime_error("Verification requires exact integers");
}
void verifyGameplay(char** argv) {
    using namespace cuexis;
    using namespace playback;
    const auto configuration =
        take(decodeGameplayConfiguration(read(argv[4]), {131072, 64, 8192, 16384, 8192, true}));
    const auto packed = read(argv[3]);
    const auto content = take(GameplayContent::fromPacked(
        std::as_bytes(std::span{packed.data(), packed.size()}), configuration));
    const auto cases = take(json::parse(read(argv[5]), {131072, 32, 8192}));
    for (const auto& scenario : *cases.array()) {
        PlaybackSession session;
        auto source = take(PlaybackSource::fromCxcFileEntry(argv[2], "compiled/chart.v5.json"));
        auto prepared =
            take(session.prepareGameplayLoad(std::move(source), PlaybackMode::HostClock, content));
        take(session.commit(std::move(prepared)));
        const GameplayExecutionContext scope{1, 1, 1};
        take(session.beginRealtimeGameplay({500000, 20000, 4096, "offline.fixture", true},
                                           {{0, 1}, {0, 1}, {0, 1}, {1, 1}}, scope,
                                           "verification.us"));
        std::uint64_t sequence = 0, publication = 0, position = 0;
        auto observation = [&](std::int64_t tick) {
            return ClockObservation{"verification.us",
                                    1,
                                    ++publication,
                                    tick,
                                    tick + 500000,
                                    0,
                                    ClockFramePosition{tick, 1000000},
                                    ClockFramePosition{tick, 1000000},
                                    {},
                                    ClockPlaybackState::Playing,
                                    ClockQuality::Exact,
                                    false,
                                    false,
                                    false};
        };
        std::map<std::int64_t, std::vector<GameplayOccurrence>> groups;
        for (const auto& event : *field(scenario, "inputs").array()) {
            const auto tick = integer(field(event, "tick"));
            groups[tick].push_back(
                {{{tick},
                  position + 1,
                  *field(event, "action").string() == "press" ? GameplayInputAction::Press
                                                              : GameplayInputAction::Release,
                  *field(event, "channel").string(),
                  "domain.binding.one",
                  configuration.sourceClass,
                  {}},
                 0});
            ++position;
        }
        for (auto& [tick, members] : groups) {
            auto clock = observation(tick + 1);
            const auto count = members.size();
            take(session.deliverGameplayCapture(
                {scope, sequence++,
                 GameplayClosedBatch{
                     {tick}, {tick + 1}, sequence, 0, 1, count, std::move(members)}},
                clock, tick + 1));
        }
        const auto end = integer(field(scenario, "end"));
        auto clock = observation(end + 1);
        take(session.deliverGameplayCapture({scope, sequence, GameplayCapturePrefix{{end + 1}}},
                                            clock, end + 1));
        const auto decision =
            take(session.requestGameplayAdvance({scope, 1, 0, {end + 1}}, clock, end + 1));
        if (decision.disposition != GameplayAdvanceDisposition::Executed)
            throw std::runtime_error("Verification advance did not execute");
        const auto result = take(session.queryGameplay());
        const auto score = take(result.score());
        if (score.score != integer(field(scenario, "score")) ||
            score.hits != integer(field(scenario, "hits")) ||
            score.misses != integer(field(scenario, "misses")))
            throw std::runtime_error("Score mismatch: " + std::to_string(score.score) +
                                     " hits=" + std::to_string(score.hits) +
                                     " misses=" + std::to_string(score.misses));
        const auto replay = take(session.archiveGameplay());
        const auto evaluated = take(session.evaluateGameplayReplay(replay));
        if (!evaluated.evidenceValid || !take(result.sameResult(evaluated.result)))
            throw std::runtime_error("Replay result mismatch");
        std::cout << *field(scenario, "name").string() << ": score=" << score.score
                  << " hits=" << score.hits << " misses=" << score.misses << " replay=equal\n";
    }
}
} // namespace
int main(int argc, char** argv) {
    if (argc != 5 && argc != 6) {
        std::cerr << "source-v5.json configuration.json metadata.packed output.packed\n";
        return 2;
    }
    try {
        if (argc == 6 && std::string_view{argv[1]} == "--verify-gameplay") {
            verifyGameplay(argv);
            return 0;
        }
        using namespace cuexis;
        const auto config = take(playback::decodeGameplayConfiguration(
            read(argv[2]), {131072, 64, 8192, 16384, 8192, true}));
        if (!config.testOnly)
            throw std::runtime_error("Explicit testOnly configuration required");
        const auto source = read(argv[1]);
        const auto probe = take(json::parse(source, {16 * 1024 * 1024, 64, 8192}));
        if (probe.find("objects")) {
            auto compiled = take(chart::compileBehaviorCandidate(source));
            std::ofstream output(argv[4], std::ios::binary | std::ios::trunc);
            output << source;
            if (!output.good())
                throw std::runtime_error("Behavior artifact write failed");
            output.close();
            auto repeated = take(chart::compileBehaviorCandidate(read(argv[4])));
            if (compiled.metadata.semanticIdentity != repeated.metadata.semanticIdentity)
                throw std::runtime_error("Behavior artifact identity round-trip failed");
            if (argc == 6) {
                auto foundation = take(chart::PackedChartReader::read(argv[3]));
                const auto& timing = field(probe, "timing");
                if (integer(field(timing, "offsetMs")) != 0 ||
                    integer(field(timing, "defaultBpm")) != 120 ||
                    !field(timing, "tempoEvents").array()->empty() ||
                    !field(timing, "stops").array()->empty() || foundation.timing.offsetMs != 0 ||
                    foundation.timing.defaultBpm != 120 || !foundation.timing.tempoEvents.empty() ||
                    !foundation.timing.stops.empty())
                    throw std::runtime_error(
                        "Fixture foundation requires matching constant 120 BPM");
                foundation.chartId = compiled.runtime.chartId;
                foundation.mainMusic.reset();
                foundation.entities.clear();
                foundation.resourceClosure.resources.clear();
                foundation.defaultCamera = compiled.runtime.camera;
                static_cast<void>(take(chart::PackedChartWriter::writeAtomic(foundation, argv[5])));
            }
            std::cout << "{\"semanticIdentity\":\""
                      << chart::packed::semanticIdentityHex(compiled.metadata.semanticIdentity)
                      << "\",\"entities\":" << compiled.runtime.objects.size()
                      << ",\"requirements\":0,\"behaviors\":" << compiled.runtime.behaviors.size()
                      << ",\"calibration\":\""
                      << take(playback::RealtimeClock::calibrationIdentity(
                             {{0, 1}, {0, 1}, {0, 1}, {1, 1}}))
                      << "\""
                      << ",\"roundTrip\":true}\n";
            return 0;
        }
        tools::gameplay_author::AuthorCompileContext context;
        context.chart = {"chart.entry.one", take(chart::PackedChartReader::read(argv[3]))};
        context.compilerProfileToken = "gameplay.author.t4-k4.v1";
        for (const auto& capability : take(playback::gameplayCapabilities()))
            context.capabilities.recognisedCapabilityIds.push_back(capability.id);
        context.capabilities.enabledCapabilityIds = config.enabledCapabilities;
        context.identityDeclarations = {
            {config.engine[0], config.engine[1], config.engine[2], config.engine[3],
             config.engine[4], config.engine[5]},
            {config.rulesetInterfaceProjection, config.rulesetModuleOrder, config.rulesetBuild},
            {config.session[0], config.session[1], config.session[2], config.session[3]}};
        auto authored = take(tools::gameplay_author::compile(
            read(argv[1]), json::gameplay::SourceKind::chartInline, context));
        for (const auto& entity : authored.chart.entities)
            if (!entity.requirements.empty())
                throw std::runtime_error("This presentation fixture compiler forbids Requirements");
        if (!authored.owners.empty())
            throw std::runtime_error("This presentation fixture compiler forbids Gameplay owners");
        auto assembled = take(chart::assembleCandidateChart(std::move(authored.chart), {}));
        const auto identity = take(chart::packed::semanticIdentity(assembled));
        const auto sizing = take(chart::PackedChartWriter::writeAtomic(assembled, argv[4]));
        const auto decoded = take(chart::PackedChartReader::read(argv[4]));
        if (take(chart::packed::semanticPreimage(decoded)) !=
            take(chart::packed::semanticPreimage(assembled)))
            throw std::runtime_error("Packed Chart complete round-trip mismatch");
        std::cout << "{\"semanticIdentity\":\"" << chart::packed::semanticIdentityHex(identity)
                  << "\",\"entities\":" << assembled.entities.size()
                  << ",\"requirements\":0,\"roundTrip\":true}\n";
        static_cast<void>(sizing);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 2;
    }
}

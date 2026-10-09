#include "player_gameplay.hpp"
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
#include <array>
#include <charconv>
#include <chrono>
#include <fstream>
#include <limits>
namespace cuexis::player {
namespace {
struct Failure {
    core::Error error;
};
void require(bool value, std::string_view message) {
    if (!value)
        throw Failure{core::Error{"player.arguments.unknown", std::string{message}}};
}
template <class T> auto integer(std::string_view text) -> T {
    T number{};
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), number);
    require(parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size(),
            "Gameplay requires exact integers");
    return number;
}
} // namespace
auto readPlayerGameplay(const PlayerOptions& options) -> core::Result<PlayerGameplayProfile> try {
    require(options.gameplayConfiguration && options.gameplayBudget && options.gameplayHStep &&
                options.gameplayTStep,
            "Gameplay requires explicit configuration, budget and Tick steps");
    std::array<std::size_t, 5> values{};
    std::string_view text = *options.gameplayBudget;
    for (std::size_t i = 0; i < values.size(); ++i) {
        const auto comma = text.find(',');
        require((i + 1 == values.size()) == (comma == text.npos),
                "Gameplay budget requires five components");
        values[i] = integer<std::size_t>(text.substr(0, comma));
        require(values[i] > 0, "Gameplay budget must be positive");
        if (comma != text.npos)
            text.remove_prefix(comma + 1);
    }
    PlayerGameplayProfile result;
    result.budget = {values[0], values[1], values[2], values[3], values[4], true};
    std::ifstream file{*options.gameplayConfiguration, std::ios::binary | std::ios::ate};
    const auto size = file.tellg();
    require(file && size >= 0 && static_cast<std::uintmax_t>(size) <= values[0],
            "Gameplay configuration unavailable or exceeds explicit byte bound");
    std::string source(static_cast<std::size_t>(size), '\0');
    file.seekg(0);
    file.read(source.data(), static_cast<std::streamsize>(source.size()));
    require(file.good(), "Gameplay configuration read failed");
    auto decoded = playback::decodeGameplayConfiguration(source, result.budget);
    if (!decoded)
        return core::unexpected(std::move(decoded.error()));
    result.configuration = std::move(*decoded);
    result.hStep = integer<std::int64_t>(*options.gameplayHStep);
    result.tStep = integer<std::int64_t>(*options.gameplayTStep);
    require(result.hStep > 0 && result.tStep > 0, "Gameplay Tick steps must be positive");
    for (const auto& key : options.gameplayKeys) {
        const auto a = key.find(':'), b = key.find(':', a == key.npos ? key.size() : a + 1);
        require(a != key.npos && b != key.npos && b > a + 1 && b + 1 < key.size() &&
                    key.find(':', b + 1) == key.npos,
                "Gameplay key requires scancode:channel:domain");
        const auto code = integer<std::uint32_t>(std::string_view{key}.substr(0, a));
        require(code > 0, "Gameplay scancode must be positive");
        for (const auto& existing : result.keys)
            require(existing.scanCode != code, "Duplicate Gameplay scancode");
        result.keys.push_back({code, key.substr(a + 1, b - a - 1), key.substr(b + 1)});
    }
    if (options.gameplayGuide) {
        auto guide = readPlayerGuide(*options.gameplayGuide, result.budget);
        if (!guide)
            return core::unexpected(std::move(guide.error()));
        for (const auto& note : *guide) {
            const std::string_view labels = "DFJK";
            constexpr std::array<std::uint32_t, 4> codes{7, 9, 13, 14};
            bool found = false;
            for (const auto& mapping : result.keys)
                found |= mapping.scanCode == codes[labels.find(note.key)];
            require(found, "Guide key has no Gameplay scancode mapping");
        }
        result.guide = std::move(*guide);
    }
    return result;
} catch (const Failure& error) {
    return core::unexpected(error.error);
} catch (...) {
    return core::unexpected(
        core::Error{"player.arguments.unknown", "Gameplay profile allocation or parsing failed"});
}
auto PlayerGameplay::mapped(std::uint32_t code) const -> bool {
    for (const auto& key : profile_.keys)
        if (key.scanCode == code)
            return true;
    return false;
}
auto PlayerGameplay::step(playback::PlaybackSession& session, const PlayerInput& input,
                          const playback::RuntimeFrame& frame)
    -> core::Result<playback::GameplayScore> try {
    if (h_ > std::numeric_limits<std::int64_t>::max() - profile_.hStep ||
        t_ > std::numeric_limits<std::int64_t>::max() - profile_.tStep)
        return core::unexpected(core::Error{"player.arguments.unknown", "Gameplay Tick overflow"});
    auto h = h_;
    const auto t = t_ + profile_.tStep;
    std::vector<playback::GameplayInput> observations;
    auto sequence = sequence_;
    const auto arrival = std::chrono::duration_cast<std::chrono::nanoseconds>(
                             std::chrono::steady_clock::now().time_since_epoch())
                             .count();
    for (const auto& event : input.keys)
        for (const auto& mapping : profile_.keys)
            if (mapping.scanCode == event.scanCode) {
                if (h > std::numeric_limits<std::int64_t>::max() - profile_.hStep)
                    return core::unexpected(
                        core::Error{"player.arguments.unknown", "Gameplay Tick overflow"});
                // Explicit test-only sampling clock: one work Tick per transition.
                // Keep SDL transition order without changing kernel collision rules.
                h += profile_.hStep;
                if (sequence == std::numeric_limits<std::uint64_t>::max() ||
                    event.timestampNs >
                        static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
                    return core::unexpected(
                        core::Error{"player.arguments.unknown",
                                    "Gameplay input provenance or sequence overflow"});
                playback::GameplayInput value{{h},
                                              ++sequence,
                                              event.pressed
                                                  ? playback::GameplayInputAction::Press
                                                  : playback::GameplayInputAction::Release,
                                              mapping.channel,
                                              mapping.domain,
                                              profile_.configuration.sourceClass,
                                              {}};
                value.rawTimestamps = {static_cast<std::int64_t>(event.timestampNs), arrival, 0, 0};
                // Fresh SDL discrete transitions do not describe trajectory gaps.
                // Control boundaries discard older poll batches in the frame loop.
                observations.push_back(std::move(value));
            }
    if (observations.empty())
        h += profile_.hStep;
    if (!observations.empty()) {
        auto submitted = session.submitGameplay(observations);
        if (!submitted)
            return core::unexpected(std::move(submitted.error()));
        sequence_ = sequence;
    }
    auto advanced = session.advanceGameplay({h}, {t}, frame);
    if (!advanced)
        return core::unexpected(std::move(advanced.error()));
    auto receipt = session.gameplayAdvanceReceipt();
    if (!receipt)
        return core::unexpected(std::move(receipt.error()));
    if (receipt->error)
        return core::unexpected(*receipt->error);
    h_ = h;
    t_ = t;
    auto result = session.queryGameplay();
    if (!result)
        return core::unexpected(std::move(result.error()));
    auto archive = session.archiveGameplay();
    if (!archive)
        return core::unexpected(std::move(archive.error()));
    auto replay = session.evaluateGameplayReplay(*archive);
    if (!replay)
        return core::unexpected(std::move(replay.error()));
    auto same = result->sameResult(replay->result);
    if (!same)
        return core::unexpected(std::move(same.error()));
    if (!replay->evidenceValid || !*same)
        return core::unexpected(
            core::Error{"player.command.not_loaded", "Complete Gameplay Replay comparison failed"});
    return result->score();
} catch (...) {
    return core::unexpected(
        core::Error{"player.arguments.unknown", "Gameplay input capture allocation failed"});
}
auto PlayerGameplay::seek(playback::PlaybackSession& session, playback::GameplayTick target,
                          const playback::RuntimeFrame& frame) -> core::Result<void> {
    auto archive = session.archiveGameplay();
    if (!archive)
        return core::unexpected(std::move(archive.error()));
    auto cut = archive->cutAt(target);
    if (!cut)
        return core::unexpected(std::move(cut.error()));
    auto restored = session.seekGameplay(*archive, target, *cut, {}, {target.value}, frame);
    if (restored) {
        h_ = target.value;
        t_ = target.value;
    }
    return restored;
}
} // namespace cuexis::player
#endif

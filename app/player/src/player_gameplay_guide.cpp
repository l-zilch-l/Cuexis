#include "player_gameplay_guide.hpp"
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
#include <algorithm>
#include <charconv>
#include <fstream>
#include <sstream>
#include <string_view>
namespace cuexis::player {
namespace {
struct GuideFailure {
    core::Error error;
};
void need(bool value, std::string_view message) {
    if (!value)
        throw GuideFailure{core::Error{"player.arguments.unknown", std::string{message}}};
}
template <class T> auto number(std::string_view text) -> T {
    T value{};
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    need(parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size(),
         "Guide requires exact integers");
    return value;
}
auto markerId(std::string_view id) -> bool {
    if (id.size() != 36)
        return false;
    for (std::size_t i = 0; i < id.size(); ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (id[i] != '-')
                return false;
        } else if (!((id[i] >= '0' && id[i] <= '9') || (id[i] >= 'a' && id[i] <= 'f')))
            return false;
    }
    return true;
}
constexpr render::Color white{0.85F, 0.9F, 1.0F, 1.0F}, grey{0.25F, 0.3F, 0.4F, 1.0F},
    hitColor{0.25F, 1.0F, 0.5F, 1.0F}, missColor{1.0F, 0.3F, 0.35F, 1.0F};
constexpr std::array<render::Color, 4> laneColors{{{0.2F, 0.8F, 1.0F, 1.0F},
                                                   {0.3F, 1.0F, 0.65F, 1.0F},
                                                   {1.0F, 0.75F, 0.25F, 1.0F},
                                                   {0.8F, 0.5F, 1.0F, 1.0F}}};
void line(render::RenderScene& scene, float x, float y, float x2, float y2, render::Color color) {
    auto drawn = scene.addDebugLine({x, y, 0}, {x2, y2, 0}, color);
    if (!drawn)
        throw GuideFailure{std::move(drawn.error())};
}
void box(render::RenderScene& scene, float x, float y, float width, float height,
         render::Color color) {
    line(scene, x - width / 2, y - height / 2, x + width / 2, y - height / 2, color);
    line(scene, x + width / 2, y - height / 2, x + width / 2, y + height / 2, color);
    line(scene, x + width / 2, y + height / 2, x - width / 2, y + height / 2, color);
    line(scene, x - width / 2, y + height / 2, x - width / 2, y - height / 2, color);
}
// Five-column bitmap glyphs rendered through the existing neutral DebugLine pass.
auto glyph(char c) -> std::array<unsigned, 7> {
    switch (c) {
    case 'A':
        return {14, 17, 17, 31, 17, 17, 17};
    case 'B':
        return {30, 17, 17, 30, 17, 17, 30};
    case 'C':
        return {14, 17, 16, 16, 16, 17, 14};
    case 'D':
        return {30, 17, 17, 17, 17, 17, 30};
    case 'E':
        return {31, 16, 16, 30, 16, 16, 31};
    case 'F':
        return {31, 16, 16, 30, 16, 16, 16};
    case 'G':
        return {14, 17, 16, 23, 17, 17, 15};
    case 'H':
        return {17, 17, 17, 31, 17, 17, 17};
    case 'I':
        return {31, 4, 4, 4, 4, 4, 31};
    case 'J':
        return {7, 2, 2, 2, 2, 18, 12};
    case 'K':
        return {17, 18, 20, 24, 20, 18, 17};
    case 'L':
        return {16, 16, 16, 16, 16, 16, 31};
    case 'M':
        return {17, 27, 21, 21, 17, 17, 17};
    case 'N':
        return {17, 25, 21, 19, 17, 17, 17};
    case 'O':
        return {14, 17, 17, 17, 17, 17, 14};
    case 'P':
        return {30, 17, 17, 30, 16, 16, 16};
    case 'Q':
        return {14, 17, 17, 17, 21, 18, 13};
    case 'R':
        return {30, 17, 17, 30, 20, 18, 17};
    case 'S':
        return {15, 16, 16, 14, 1, 1, 30};
    case 'T':
        return {31, 4, 4, 4, 4, 4, 4};
    case 'U':
        return {17, 17, 17, 17, 17, 17, 14};
    case 'V':
        return {17, 17, 17, 17, 17, 10, 4};
    case 'W':
        return {17, 17, 17, 21, 21, 21, 10};
    case 'X':
        return {17, 17, 10, 4, 10, 17, 17};
    case 'Y':
        return {17, 17, 10, 4, 4, 4, 4};
    case 'Z':
        return {31, 1, 2, 4, 8, 16, 31};
    case '0':
        return {14, 17, 19, 21, 25, 17, 14};
    case '1':
        return {4, 12, 4, 4, 4, 4, 14};
    case '2':
        return {14, 17, 1, 2, 4, 8, 31};
    case '3':
        return {30, 1, 1, 14, 1, 1, 30};
    case '4':
        return {2, 6, 10, 18, 31, 2, 2};
    case '5':
        return {31, 16, 16, 30, 1, 1, 30};
    case '6':
        return {14, 16, 16, 30, 17, 17, 14};
    case '7':
        return {31, 1, 2, 4, 8, 8, 8};
    case '8':
        return {14, 17, 17, 14, 17, 17, 14};
    case '9':
        return {14, 17, 17, 15, 1, 1, 14};
    case '-':
        return {0, 0, 0, 31, 0, 0, 0};
    case '/':
        return {1, 1, 2, 4, 8, 16, 16};
    default:
        return {};
    }
}
void text(render::RenderScene& scene, std::string_view value, float x, float y, float size,
          render::Color color = white) {
    for (char c : value) {
        const auto rows = glyph(c);
        for (unsigned r = 0; r < 7; ++r)
            for (unsigned col = 0; col < 5; ++col)
                if (rows[r] & (1U << (4 - col)))
                    for (unsigned stroke = 0; stroke < 5; ++stroke) {
                        const auto pixelY =
                            y - (static_cast<float>(r) + static_cast<float>(stroke) * 0.2F) * size;
                        line(scene, x + static_cast<float>(col) * size, pixelY,
                             x + (static_cast<float>(col) + 0.85F) * size, pixelY, color);
                    }
        x += size * 6;
    }
}
auto hidden(const playback::FrameSnapshot& snapshot, std::string_view id) -> bool {
    const auto found = std::find_if(snapshot.objects.begin(), snapshot.objects.end(),
                                    [&](const auto& o) { return o.id == id; });
    need(found != snapshot.objects.end(), "Guide outcome marker is absent from active content");
    return !found->visible;
}
} // namespace
auto readPlayerGuide(const std::filesystem::path& path,
                     const playback::GameplayConfigurationDecodeBudget& budget)
    -> core::Result<std::vector<PlayerGuideNote>> try {
    std::ifstream file{path, std::ios::binary | std::ios::ate};
    const auto length = file.tellg();
    need(file && length >= 0 && static_cast<std::uintmax_t>(length) <= budget.maxBytes,
         "Guide unavailable or exceeds explicit byte bound");
    std::string bytes(static_cast<std::size_t>(length), '\0');
    file.seekg(0);
    file.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    need(file.good(), "Guide read failed");
    std::istringstream input{bytes};
    std::string magic, countToken;
    need(static_cast<bool>(input >> magic >> countToken) && magic == "cuexis-player-guide-v1",
         "Guide header is invalid");
    const auto count = number<std::size_t>(countToken);
    need(count > 0 && count <= budget.maxContainerElements, "Guide count exceeds explicit bound");
    std::vector<PlayerGuideNote> notes;
    std::vector<std::string> markerIds;
    for (std::size_t i = 0; i < count; ++i) {
        std::string kind, key, head, tail;
        PlayerGuideNote note;
        need(static_cast<bool>(input >> kind >> key >> head >> tail), "Guide record is incomplete");
        need((kind == "tap" || kind == "hold") && key.size() == 1 &&
                 std::string_view{"DFJK"}.find(key[0]) != std::string_view::npos,
             "Guide kind/key is invalid");
        note.key = key[0];
        note.head = number<std::int64_t>(head);
        note.tail = number<std::int64_t>(tail);
        need(note.head >= 0 &&
                 ((kind == "tap" && note.tail == 0) || (kind == "hold" && note.tail > note.head)),
             "Guide Tick relation is invalid");
        for (std::size_t c = 0; c < note.cues.size(); ++c) {
            need(static_cast<bool>(input >> note.cues[c]), "Guide outcome marker is missing");
            need((kind == "tap" && c >= 2) ? note.cues[c] == "-" : markerId(note.cues[c]),
                 "Guide outcome marker is invalid");
            if (note.cues[c] != "-") {
                need(std::find(markerIds.begin(), markerIds.end(), note.cues[c]) == markerIds.end(),
                     "Guide outcome markers must be distinct");
                markerIds.push_back(note.cues[c]);
            }
        }
        notes.push_back(std::move(note));
    }
    std::string extra;
    need(!(input >> extra), "Guide has unexpected trailing fields");
    return notes;
} catch (const GuideFailure& failure) {
    return core::unexpected(failure.error);
} catch (...) {
    return core::unexpected(core::Error{"player.arguments.unknown", "Guide allocation failed"});
}

auto appendPlayerGuide(std::span<const PlayerGuideNote> notes, std::int64_t horizon,
                       const playback::GameplayScore& score, bool playing,
                       const playback::FrameSnapshot& snapshot, render::RenderScene& scene)
    -> core::Result<void> try {
    need(horizon >= 0, "Guide requires a nonnegative work horizon");
    constexpr float judgeY = -1.15F, top = 1.8F;
    const auto yFor = [&](std::int64_t tick) {
        // Pure display conversion after exact integer decode; never fed back into judgement.
        return judgeY + static_cast<float>(
                            std::clamp(tick - horizon, std::int64_t{-30}, std::int64_t{120})) *
                            0.024F;
    };
    text(scene, "DFJK PRACTICE", -2.0F, 2.55F, 0.045F);
    text(scene,
         "SCORE " + std::to_string(score.score) + "  HIT " + std::to_string(score.hits) +
             "  MISS " + std::to_string(score.misses),
         -2.0F, 2.16F, 0.028F);
    line(scene, -2.05F, judgeY, 2.05F, judgeY, white);
    for (std::size_t lane = 0; lane < 4; ++lane) {
        const float x = static_cast<float>(lane) - 1.5F;
        box(scene, x, 0.33F, 0.9F, 3.05F, grey);
        box(scene, x, judgeY, 0.8F, 0.16F, laneColors[lane]);
        text(scene, std::string(1, "DFJK"[lane]), x - 0.17F, -1.47F, 0.07F, laneColors[lane]);
    }
    bool complete = true;
    for (const auto& note : notes) {
        const auto lane = std::string_view{"DFJK"}.find(note.key);
        const float x = static_cast<float>(lane) - 1.5F;
        const bool headHit = hidden(snapshot, note.cues[0]),
                   headMiss = hidden(snapshot, note.cues[1]);
        bool done = headHit || headMiss;
        bool tailHit = false, tailMiss = false, bodyMiss = false;
        if (note.tail) {
            static_cast<void>(hidden(snapshot, note.cues[2]));
            bodyMiss = hidden(snapshot, note.cues[3]);
            tailHit = hidden(snapshot, note.cues[4]);
            tailMiss = hidden(snapshot, note.cues[5]);
            done = tailHit || tailMiss;
        }
        complete &= done;
        const auto outcomeColor = (headMiss || bodyMiss || tailMiss) ? missColor : hitColor;
        if (done) {
            box(scene, x, -1.05F, 0.65F, 0.11F, outcomeColor);
            continue;
        }
        if (note.head > horizon && note.head - horizon > 120)
            continue;
        const float headY = headHit ? judgeY : yFor(note.head);
        if (note.tail) {
            const float tailY = std::min(top, yFor(note.tail));
            const auto color = (headMiss || bodyMiss) ? missColor
                               : headHit              ? hitColor
                                                      : laneColors[lane];
            for (float dx = -0.12F; dx <= 0.13F; dx += 0.06F)
                line(scene, x + dx, std::max(judgeY, headY), x + dx, tailY, color);
            box(scene, x, tailY, 0.6F, 0.12F, color);
            if (headHit || headMiss)
                text(scene, (headMiss || bodyMiss) ? "MISS" : "HOLD", x - 0.25F, -0.88F, 0.021F,
                     color);
        }
        box(scene, x, headY, 0.68F, 0.14F, laneColors[lane]);
        if (headHit && note.tail && note.tail <= horizon)
            text(scene, "RELEASE", x - 0.4F, -0.5F, 0.019F, white);
    }
    if (complete)
        text(scene, "DONE  R AGAIN  ESC EXIT", -2.0F, -2.0F, 0.028F, hitColor);
    else if (!playing)
        text(scene, "SPACE TO START / RESUME", -2.0F, -2.0F, 0.028F);
    else
        text(scene, "PRESS AT LINE  LONG NOTE HOLD", -2.0F, -2.0F, 0.023F);
    return {};
} catch (const GuideFailure& failure) {
    return core::unexpected(failure.error);
} catch (...) {
    return core::unexpected(core::Error{"player.arguments.unknown", "Guide drawing failed"});
}
} // namespace cuexis::player
#endif

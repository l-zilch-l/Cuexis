#include "fake_audio_transport.hpp"
#include "player_control.hpp"
#include "player_gameplay.hpp"
#include "player_log.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cuexis/playback/content_provider.hpp>
#include <cuexis/presentation_renderer/presentation_renderer.hpp>
#include <fstream>
namespace {
using namespace cuexis;
template <class T> auto take(core::Result<T> result) -> T {
    REQUIRE(result);
    return std::move(*result);
}
void take(core::Result<void> result) {
    REQUIRE(result);
}
auto read(const std::filesystem::path& path) -> std::string {
    std::ifstream stream{path, std::ios::binary};
    REQUIRE(stream);
    return {std::istreambuf_iterator<char>{stream}, {}};
}

TEST_CASE("Player focus loss and resume never admit earlier aggregate key input",
          "[candidate][gameplay][player][focus]") {
    const auto root = std::filesystem::path{CUEXIS_BUILD_DIR} / "s7a78-public-fixture";
    player::PlayerOptions options;
    options.gameplayConfiguration = root / "configuration.json";
    options.gameplayBudget = "131072,64,8192,16384,8192";
    options.gameplayHStep = "5";
    options.gameplayTStep = "7";
    options.gameplayKeys = {"4:lane.one:domain.binding.one"};
    const auto profile = take(player::readPlayerGameplay(options));
    const auto text = read(root / "main.packed");
    std::vector<std::byte> bytes(text.size());
    for (std::size_t i = 0; i < text.size(); ++i)
        bytes[i] = std::byte{static_cast<unsigned char>(text[i])};
    auto provider = take(playback::MemoryContentProvider::create({}));
    auto source = take(playback::PlaybackSource::fromGameplayPacked(
        "player-fixture", "compiled/main.packed", bytes, profile.configuration, {}, provider,
        playback::GameplayPrepareIntent::Presentation));
    presentation_renderer::TestPresentationRenderer renderer;
    audio::AudioClipStore store;
    auto logger = take(player::PlayerLogger::create("gameplay-focus-test"));
    player::PlayerControlPorts ports;
    ports.gameplay = profile;
    player::PlayerController controller{renderer, store, 1, 0, std::move(ports), *logger};
    take(controller.apply(
        {.kind = player_support::PlayerCommandKind::Load, .source = std::move(source)}));
    take(controller.apply({.kind = player_support::PlayerCommandKind::Play}));
    class Surface final : public player::PlayerSurface {
      public:
        unsigned n{};
        auto pollInput() -> core::Result<player::PlayerInput> override {
            player::PlayerInput input;
            if (n < 2) {
                input.actions = {player::PlayerInputAction::PlayPause};
                input.keys = {{4, true, 1}};
                input.focusLost = n == 0;
            }
            if (n == 5)
                input.quitRequested = true;
            ++n;
            return input;
        }
        auto drawableSize() -> core::Result<player::PlayerDrawableSize> override {
            return player::PlayerDrawableSize{320, 180};
        }
    } surface;
    player::PlayerFrameLoop loop{
        .controller = controller, .renderer = renderer, .surface = surface, .logger = *logger};
    take(player::runPlayerFrameLoop(loop));
    const auto score = take(take(controller.session().queryGameplay()).score());
    CHECK(controller.gameplay()->horizon().value == 20);
    CHECK(score.hits == 0);
    CHECK(score.misses == 1);
    CHECK(score.score == -1);
}

TEST_CASE("Player freshly loaded discrete key input is not a trajectory discontinuity",
          "[candidate][gameplay][player][input]") {
    const auto root = std::filesystem::path{CUEXIS_BUILD_DIR} / "s7a78-public-fixture";
    player::PlayerOptions options;
    options.gameplayConfiguration = root / "configuration.json";
    options.gameplayBudget = "131072,64,8192,16384,8192";
    options.gameplayHStep = "5";
    options.gameplayTStep = "7";
    options.gameplayKeys = {"7:lane.one:domain.binding.one"};
    const auto profile = take(player::readPlayerGameplay(options));
    const auto text = read(root / "main.packed");
    std::vector<std::byte> bytes(text.size());
    for (std::size_t i = 0; i < text.size(); ++i)
        bytes[i] = std::byte{static_cast<unsigned char>(text[i])};
    auto provider = take(playback::MemoryContentProvider::create({}));
    auto source = take(playback::PlaybackSource::fromGameplayPacked(
        "player-discrete-fixture", "compiled/main.packed", bytes, profile.configuration, {},
        provider, playback::GameplayPrepareIntent::Presentation));
    presentation_renderer::TestPresentationRenderer renderer;
    audio::AudioClipStore store;
    auto logger = take(player::PlayerLogger::create("gameplay-discrete-test"));
    player::PlayerControlPorts ports;
    ports.gameplay = profile;
    player::PlayerController controller{renderer, store, 1, 0, std::move(ports), *logger};
    take(controller.apply(
        {.kind = player_support::PlayerCommandKind::Load, .source = std::move(source)}));
    take(controller.apply({.kind = player_support::PlayerCommandKind::Play}));
    SECTION("fresh load") {}
    SECTION("fresh transition after pause and resume") {
        take(controller.apply({.kind = player_support::PlayerCommandKind::Pause}));
        take(controller.apply({.kind = player_support::PlayerCommandKind::Play}));
    }
    SECTION("fresh transition after Stop and Play") {
        take(controller.apply({.kind = player_support::PlayerCommandKind::Stop}));
        take(controller.apply({.kind = player_support::PlayerCommandKind::Play}));
    }
    SECTION("fresh transition after exact Seek") {
        take(controller.gameplay()->seek(controller.session(), {0}, {0, 0, 0}));
    }
    class Surface final : public player::PlayerSurface {
      public:
        unsigned n{};
        auto pollInput() -> core::Result<player::PlayerInput> override {
            player::PlayerInput input;
            if (n == 0)
                input.keys = {{7, true, 100}};
            if (n == 1)
                input.keys = {{7, false, 200}};
            if (n == 4)
                input.quitRequested = true;
            ++n;
            return input;
        }
        auto drawableSize() -> core::Result<player::PlayerDrawableSize> override {
            return player::PlayerDrawableSize{320, 180};
        }
    } surface;
    player::PlayerFrameLoop loop{
        .controller = controller, .renderer = renderer, .surface = surface, .logger = *logger};
    take(player::runPlayerFrameLoop(loop));
    const auto result = take(controller.session().queryGameplay());
    const auto score = take(result.score());
    CHECK(score.score == 2);
    CHECK(score.hits == 1);
    CHECK(score.misses == 0);
    const auto archive = take(controller.session().archiveGameplay());
    const auto replay = take(controller.session().evaluateGameplayReplay(archive));
    CHECK(replay.evidenceValid);
    CHECK(take(result.sameResult(replay.result)));
}
} // namespace
TEST_CASE("Player typed discrete sampling uses actual Gameplay and complete Replay",
          "[candidate][gameplay][player]") {
    const auto root = std::filesystem::path{CUEXIS_BUILD_DIR} / "s7a78-public-fixture";
    player::PlayerOptions options;
    options.gameplayConfiguration = root / "configuration.json";
    options.gameplayBudget = "131072,64,8192,16384,8192";
    options.gameplayHStep = "5";
    options.gameplayTStep = "7";
    options.gameplayKeys = {"4:lane.one:domain.binding.one"};
    const auto profile = take(player::readPlayerGameplay(options));
    const auto text = read(root / "main.packed");
    std::vector<std::byte> bytes(text.size());
    for (std::size_t i = 0; i < text.size(); ++i)
        bytes[i] = std::byte{static_cast<unsigned char>(text[i])};
    auto provider = take(playback::MemoryContentProvider::create({}));
    auto source = take(playback::PlaybackSource::fromGameplayPacked(
        "player-fixture", "compiled/main.packed", bytes, profile.configuration, {}, provider,
        playback::GameplayPrepareIntent::GameplayOnly));
    playback::PlaybackSession session;
    auto prepared =
        take(session.prepareLoad(std::move(source), playback::PlaybackMode::ChartClock));
    take(session.commit(std::move(prepared)));
    player::PlayerGameplay adapter{profile};
    REQUIRE(adapter.mapped(4));
    REQUIRE_FALSE(adapter.mapped(5));
    player::PlayerInput pressed;
    pressed.keys = {{4, true, 9007199254740993ULL}, {5, true, 2}};
    REQUIRE(take(adapter.step(session, pressed, {0, 0, 0})).hits == 0);
    player::PlayerInput released;
    released.keys = {{4, false, 5}};
    take(adapter.step(session, released, {0, 0, 0}));
    take(adapter.step(session, {}, {0, 0, 0}));
    const auto hit = take(adapter.step(session, {}, {0, 0, 0}));
    CHECK(hit.score == 2);
    CHECK(hit.combo == 1);
    CHECK(hit.hits == 1);
    CHECK(hit.misses == 0);
    take(session.controlGameplay(playback::GameplayControl::Pause));
    CHECK_FALSE(adapter.step(session, pressed, {0, 0, 0}));
    CHECK(adapter.horizon().value == 20);
    take(adapter.seek(session, {20}, {0, 0, 0}));
    CHECK(take(take(session.queryGameplay()).score()).score == 2);
    take(session.controlGameplay(playback::GameplayControl::Stop));
    adapter.reset(true);
    take(session.controlGameplay(playback::GameplayControl::Resume));
    for (int i = 0; i < 4; ++i)
        take(adapter.step(session, {}, {0, 0, 0}));
    CHECK(take(take(session.queryGameplay()).score()).misses == 1);
    options.gameplayKeys.push_back("4:duplicate:domain.binding.one");
    CHECK_FALSE(player::readPlayerGameplay(options));
    options.gameplayKeys.clear();
    options.gameplayHStep = "1.0";
    CHECK_FALSE(player::readPlayerGameplay(options));
}

TEST_CASE("Player rejected audio control preserves actual Gameplay state and complete result",
          "[candidate][gameplay][player][audio][failure]") {
    using namespace cuexis;
    const auto root = std::filesystem::path{CUEXIS_BUILD_DIR} / "s7a78-public-fixture";
    player::PlayerOptions options;
    options.gameplayConfiguration = root / "configuration.json";
    options.gameplayBudget = "131072,64,8192,16384,8192";
    options.gameplayHStep = "5";
    options.gameplayTStep = "7";
    options.gameplayKeys = {"4:lane.one:domain.binding.one"};
    const auto profile = take(player::readPlayerGameplay(options));
    const auto text = read(root / "music.packed");
    std::vector<std::byte> bytes(text.size());
    for (std::size_t i = 0; i < text.size(); ++i)
        bytes[i] = std::byte{static_cast<unsigned char>(text[i])};
    const auto wav = read(std::filesystem::path{CUEXIS_SOURCE_DIR} /
                          "assets/projects/stage1d_project/assets/audio/main.wav");
    std::vector<std::byte> music(wav.size());
    for (std::size_t i = 0; i < wav.size(); ++i)
        music[i] = std::byte{static_cast<unsigned char>(wav[i])};
    auto provider = take(
        playback::MemoryContentProvider::create({{"main", "audio/main.wav", std::move(music)}}));
    auto source = take(playback::PlaybackSource::fromGameplayPacked(
        "player-audio-fixture", "compiled/music.packed", std::move(bytes), profile.configuration,
        {{.id = "audio.main",
          .type = playback::PlaybackAssetType::Audio,
          .rootId = "main",
          .logicalSource = "audio/main.wav"}},
        provider, playback::GameplayPrepareIntent::Presentation));
    presentation_renderer::TestPresentationRenderer renderer;
    audio::AudioClipStore store;
    auto logger = take(player::PlayerLogger::create("gameplay-audio-failure-test"));
    class Seat final : public player::PlayerAudioSeat {
      public:
        test_support::FakeAudioTransport fake;
        explicit Seat(audio::AudioClipStore& store) : fake{store} {}
        auto transport() -> audio::IAudioTransport& override {
            return fake;
        }
        auto recheckBoundDevice() -> core::Result<void> override {
            return {};
        }
        auto prepareReplacement(audio::AudioClipHandle h, double p) -> core::Result<void> override {
            return fake.prepareReplacement(h, p);
        }
        auto activateReplacement() -> core::Result<void> override {
            return fake.activateReplacement();
        }
        auto applyGain(float) -> core::Result<void> override {
            return {};
        }
        auto unload() -> core::Result<void> override {
            return fake.unload();
        }
    };
    Seat* seat{};
    player::PlayerControlPorts ports;
    ports.gameplay = profile;
    ports.prepareClip = [](playback::PreparedPlayback&, audio::AudioClipStore& target) {
        auto clip = take(audio::AudioClip::create(48000, 2, std::vector<float>(96000, 0)));
        return target.registerClip(std::move(clip));
    };
    ports.openAudio = [&](const std::optional<audio::AudioClipHandle>& handle, double,
                          double) -> core::Result<std::unique_ptr<player::PlayerAudioSeat>> {
        auto owned = std::make_unique<Seat>(store);
        seat = owned.get();
        take(seat->fake.load(*handle));
        return std::unique_ptr<player::PlayerAudioSeat>{std::move(owned)};
    };
    player::PlayerController controller{renderer, store, 1, 0, std::move(ports), *logger};
    take(controller.apply({.kind = player_support::PlayerCommandKind::Load,
                           .source = std::move(source),
                           .mode = playback::PlaybackMode::CuexisAudio}));
    const auto before = take(controller.session().queryGameplay());
    REQUIRE(take(controller.session().gameplayState()) == playback::GameplayState::Paused);
    REQUIRE(seat);
    seat->fake.failNextService();
    auto rejected = controller.apply({.kind = player_support::PlayerCommandKind::Play});
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().code() == "audio.fake.service_failed");
    CHECK(take(controller.session().gameplayState()) == playback::GameplayState::Paused);
    CHECK(take(before.sameResult(take(controller.session().queryGameplay()))));
    CHECK(controller.gameplay()->horizon().value == 0);
    auto replay = take(
        controller.session().evaluateGameplayReplay(take(controller.session().archiveGameplay())));
    CHECK(replay.evidenceValid);
    CHECK(take(before.sameResult(replay.result)));
}

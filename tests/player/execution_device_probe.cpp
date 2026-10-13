#include "../../app/player/src/execution_host.hpp"
#include "../rpa/execution_fixture.hpp"
#include <SDL3/SDL.h>
#include <catch2/catch_test_macros.hpp>
#include <cuexis/audio_sdl/sdl_audio.hpp>
#include <cuexis/audio_sdl/wav_decoder.hpp>
#include <cuexis/platform_sdl/sdl_runtime.hpp>
#include <cuexis/platform_sdl/sdl_window.hpp>
#include <cuexis/render_opengl/open_gl_backend.hpp>
#include <filesystem>
#include <iostream>
#include <thread>

using namespace cuexis;
using namespace playback;
using namespace playback::detail;
namespace {
class Device final : public OwnedAudioDevice {
  public:
    Device(audio_sdl::SdlAudioSubsystem subsystem, audio_sdl::SdlAudioTransport transport)
        : subsystem_(std::move(subsystem)), transport_(std::move(transport)) {}
    auto transport() noexcept -> audio::IAudioTransport& override {
        return transport_;
    }
    auto isolateCallbacks() -> core::Result<bool> override {
        return transport_.isolateCallbacks();
    }
    auto recheckDevice() -> core::Result<void> override {
        return transport_.recheckBoundDevice();
    }

  private:
    audio_sdl::SdlAudioSubsystem subsystem_;
    audio_sdl::SdlAudioTransport transport_;
};
auto factory() -> AudioWorker::DeviceFactory {
    return [](audio::AudioClipStore& store) -> core::Result<std::unique_ptr<OwnedAudioDevice>> {
        auto subsystem = audio_sdl::SdlAudioSubsystem::create();
        if (!subsystem)
            return core::unexpected(subsystem.error());
        auto config =
            audio::validateAudioConfig({audio::AudioDeviceRequest::DefaultPlayback, 50, 25, 0.01F});
        if (!config)
            return core::unexpected(config.error());
        auto transport = audio_sdl::SdlAudioTransport::create(*subsystem, store, *config);
        if (!transport)
            return core::unexpected(transport.error());
        return std::unique_ptr<OwnedAudioDevice>{
            std::make_unique<Device>(std::move(*subsystem), std::move(*transport))};
    };
}
auto receive(AudioWorker& worker, std::uint64_t id) -> AudioWorkerReply {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
    while (std::chrono::steady_clock::now() < deadline) {
        auto polled = worker.poll(id);
        REQUIRE(polled);
        if (*polled) {
            INFO(((**polled) ? "" : std::string{(**polled).error().code()}));
            REQUIRE(**polled);
            auto result = std::move(***polled);
            REQUIRE(worker.acknowledge(id));
            return result;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    FAIL("Actual audio owner timed out");
    return {};
}
auto wave() -> std::vector<std::byte> {
    std::vector<std::byte> bytes;
    const auto text = [&](const char* value) {
        for (unsigned i = 0; i < 4; ++i)
            bytes.push_back(std::byte(value[i]));
    };
    const auto integer = [&](std::uint32_t value, unsigned count) {
        for (unsigned i = 0; i < count; ++i)
            bytes.push_back(std::byte((value >> (i * 8)) & 255));
    };
    constexpr std::uint32_t samples = 96000, pcmBytes = samples * 2;
    text("RIFF");
    integer(36 + pcmBytes, 4);
    text("WAVE");
    text("fmt ");
    integer(16, 4);
    integer(1, 2);
    integer(1, 2);
    integer(48000, 4);
    integer(96000, 4);
    integer(2, 2);
    integer(16, 2);
    text("data");
    integer(pcmBytes, 4);
    bytes.resize(44 + pcmBytes);
    return bytes;
}
auto options() -> player::ExecutionHostOptions {
    player::ExecutionHostOptions result;
    result.session = {64, 16, 128, 32, 64, 64, true, {}};
    result.audio = {};
    result.audio.sessionScope = 1;
    result.audio.timeDomain = 1;
    result.audio.testOnly = true;
    result.audio.capacity = 64;
    result.audio.controlReserve = 16;
    result.hostCapacity = 16;
    result.captureCapacity = 64;
    result.captureReserve = 16;
    result.replyDeadline = std::chrono::seconds{5};
    result.viewport = {320, 240};
    return result;
}
} // namespace
TEST_CASE("B5 physical audio endpoint services callbacks isolates resumes and closes",
          "[device][audio]") {
    // Establish SDL's process main-thread owner before the service thread opens audio.
    auto runtime = platform_sdl::SdlRuntime::create();
    REQUIRE(runtime);
    AudioWorkerOptions settings;
    settings.sessionScope = 1;
    settings.timeDomain = 1;
    settings.testOnly = true;
    settings.capacity = 64;
    settings.controlReserve = 16;
    auto worker = AudioWorker::start(settings, factory(), audio_sdl::WavDecoder::decode);
    REQUIRE(worker);
    AudioWorkerCommand decode;
    decode.kind = AudioWorkerCommandKind::decode;
    decode.candidateId = 1;
    decode.encoded = wave();
    auto id = (*worker)->post(std::move(decode), false);
    REQUIRE(id);
    auto decoded = receive(**worker, *id);
    REQUIRE(decoded.clip);
    AudioWorkerCommand command;
    command.step.key = {1, 1, Participant::audio, StepPhase::prepare, 0};
    command.step.target.context.sessionScope = 1;
    command.step.target.context.controlSequence = 1;
    command.step.target.candidateId = 1;
    command.step.target.audioClip = decoded.clip;
    command.step.target.audioPositionMs = 0;
    auto apply = [&](AudioWorkerCommandKind kind, StepPhase phase) {
        command.kind = kind;
        command.step.key.phase = phase;
        auto posted = (*worker)->post(command, true);
        REQUIRE(posted);
        auto reply = receive(**worker, *posted);
        REQUIRE(reply.step);
        INFO((reply.step->cause ? std::string{reply.step->cause->code()} : ""));
        REQUIRE(reply.step->status == StepStatus::applied);
        return *reply.step;
    };
    apply(AudioWorkerCommandKind::prepare, StepPhase::prepare);
    apply(AudioWorkerCommandKind::activate, StepPhase::activate);
    const auto callbackDeadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
    for (;;) {
        auto sample = (*worker)->publication();
        REQUIRE(sample);
        if (sample->clock.presentedFrame > 0)
            break;
        REQUIRE(std::chrono::steady_clock::now() < callbackDeadline);
        std::this_thread::sleep_for(std::chrono::milliseconds{5});
    }
    auto playing = (*worker)->publication();
    REQUIRE(playing);
    REQUIRE(playing->clock.presentedFrame > 0);
    REQUIRE(playing->metrics.serviceCount > 0);
    REQUIRE(playing->settings.deviceSampleRate > 0);
    REQUIRE(playing->fault == AudioPublication::Fault::none);
    std::cout << "{\"event\":\"b5.audio.open\",\"device_rate\":"
              << playing->settings.deviceSampleRate
              << ",\"device_channels\":" << playing->settings.deviceChannels
              << ",\"buffer_frames\":" << playing->settings.deviceBufferFrames
              << ",\"presented_frame\":" << playing->clock.presentedFrame
              << ",\"service_count\":" << playing->metrics.serviceCount << "}\n";
    for (const auto duration : {100, 500}) {
        const auto before = (*worker)->publication();
        REQUIRE(before);
        // This deliberately blocks only the main caller; it is an injected test-only delay.
        std::this_thread::sleep_for(std::chrono::milliseconds{duration});
        const auto after = (*worker)->publication();
        REQUIRE(after);
        REQUIRE(after->metrics.serviceCount > before->metrics.serviceCount);
        REQUIRE(after->clock.presentedFrame > before->clock.presentedFrame);
        REQUIRE(after->fault == AudioPublication::Fault::none);
        std::cout << "{\"event\":\"b5.audio.main_block\",\"injected_ms\":" << duration
                  << ",\"service_delta\":"
                  << after->metrics.serviceCount - before->metrics.serviceCount
                  << ",\"presented_frame_delta\":"
                  << after->clock.presentedFrame - before->clock.presentedFrame
                  << ",\"test_only\":true}\n";
    }
    apply(AudioWorkerCommandKind::fence, StepPhase::fence);
    auto isolated = apply(AudioWorkerCommandKind::isolate, StepPhase::isolate);
    REQUIRE(isolated.effects.callbackIsolated);
    const auto pauseDeadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
    for (;;) {
        auto sample = (*worker)->publication();
        REQUIRE(sample);
        if (sample->clock.source.state == audio::PlaybackState::Paused)
            break;
        REQUIRE(std::chrono::steady_clock::now() < pauseDeadline);
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    auto paused = (*worker)->publication();
    REQUIRE(paused);
    std::this_thread::sleep_for(std::chrono::milliseconds{25});
    auto frozen = (*worker)->publication();
    REQUIRE(frozen);
    REQUIRE(frozen->clock.presentedFrame == paused->clock.presentedFrame);
    std::cout << "{\"event\":\"b5.audio.callback_isolated\",\"frozen_frame\":"
              << frozen->clock.presentedFrame << ",\"proof\":true}\n";
    command.step.key.operationId = 2;
    command.step.target.context.controlSequence = 2;
    command.step.target.expectedGeneration = 1;
    command.step.target.audioPositionMs = frozen->clock.source.positionMs;
    apply(AudioWorkerCommandKind::activate, StepPhase::activate);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{3};
    bool ended = false;
    while (std::chrono::steady_clock::now() < deadline) {
        auto published = (*worker)->publication();
        REQUIRE(published);
        REQUIRE(published->fault == AudioPublication::Fault::none);
        if (published->clock.source.state == audio::PlaybackState::Ended) {
            ended = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    REQUIRE(ended);
    std::cout << "{\"event\":\"b5.audio.eof\",\"actual\":true}\n";
    apply(AudioWorkerCommandKind::stop, StepPhase::stop);
    apply(AudioWorkerCommandKind::isolate, StepPhase::isolate);
    auto retired = apply(AudioWorkerCommandKind::retire, StepPhase::retire);
    REQUIRE(retired.effects.referencesReleased);
    apply(AudioWorkerCommandKind::close, StepPhase::close);
    (*worker)->requestShutdown();
    while (!(*worker)->exited())
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    REQUIRE((*worker)->joinExited());
    std::cout << "Actual SDL endpoint callback/EOF/isolation/Close passed; silent PCM is not "
                 "listening acceptance.\n";
}
TEST_CASE("B5 physical OpenGL device submits reads pixels and directed closes", "[device][gpu]") {
    auto runtime = platform_sdl::SdlRuntime::create();
    REQUIRE(runtime);
    render_opengl::OpenGlConfig config;
    config.vsync = false;
    auto configured = render_opengl::configureOpenGlContext(*runtime, config);
    REQUIRE(configured);
    platform_sdl::WindowConfig windowConfig;
    windowConfig.width = 320;
    windowConfig.height = 240;
    windowConfig.title = "Cuexis B5 automated device probe";
    auto window = platform_sdl::SdlWindow::create(*runtime, windowConfig);
    REQUIRE(window);
    REQUIRE(SDL_HideWindow(static_cast<SDL_Window*>(window->lease().nativeHandle())));
    auto backend = render_opengl::OpenGlBackend::create(*window, std::move(*configured));
    REQUIRE(backend);
    std::cout << "GPU: " << backend->info().vendor << " / " << backend->info().renderer << "\n";
    player::ExecutionHostPorts ports{factory(), audio_sdl::WavDecoder::decode,
                                     []() -> core::Result<void> { return {}; }};
    auto hostOptions = options();
    const ClockMappingConfiguration mapping{{0, 1}, {0, 1}, {0, 1}, {1, 1}};
    hostOptions.realtime = SessionRealtimeStart{
        {1000000, 0, 64, "device.gpu.b5.test-only", true}, mapping, {1, 1, 1}, "steady.us"};
    auto host = player::ExecutionHost::create(*backend, hostOptions, std::move(ports));
    REQUIRE(host);
    test_support::SessionTestContent fixture;
    test_support::rpa(fixture, mapping);
    auto loaded = (*host)->load(test_support::presentation(), fixture.content());
    INFO((loaded ? "" : std::string{loaded.error().code()}));
    REQUIRE(loaded);
    REQUIRE((*host)->capture(test_support::input()));
    const auto now = [] {
        return std::chrono::duration_cast<std::chrono::microseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
    };
    const auto anchor = now();
    std::uint64_t clockSequence = 0;
    auto clock = [&](const HostContext& context) -> core::Result<ClockObservation> {
        const auto current = now();
        return ClockObservation{"steady.us",
                                context.clockSegment,
                                ++clockSequence,
                                current,
                                current + 1000000,
                                0,
                                ClockFramePosition{20 + current - anchor, 1000000},
                                ClockFramePosition{20 + current - anchor, 1000000},
                                {},
                                ClockPlaybackState::Playing,
                                ClockQuality::Exact,
                                false,
                                false,
                                false};
    };
    auto observation = clock((*host)->context());
    REQUIRE(observation);
    auto advance = (*host)->advance(*observation);
    INFO((advance ? "" : std::string{advance.error().code()}));
    REQUIRE(advance);
    std::this_thread::sleep_for(std::chrono::milliseconds{3});
    observation = clock((*host)->context());
    REQUIRE(observation);
    auto paused = (*host)->pause(*observation);
    INFO((paused ? "" : std::string{paused.error().code()}));
    REQUIRE(paused);
    REQUIRE((*host)->state() == HostState::paused);
    auto resumed = (*host)->resume(clock);
    INFO((resumed ? "" : std::string{resumed.error().code()}));
    REQUIRE(resumed);
    REQUIRE((*host)->state() == HostState::running);
    std::this_thread::sleep_for(std::chrono::milliseconds{3});
    observation = clock((*host)->context());
    REQUIRE(observation);
    advance = (*host)->advance(*observation);
    REQUIRE(advance);
    // Actual main-thread GPU presentation uses the Playback worker's accepted context.
    auto frame = (*host)->sample(observation->presented->frame,
                                 {static_cast<double>(observation->presented->frame) / 1000.0, 0,
                                  (*host)->context().clockSegment},
                                 false);
    REQUIRE(frame);
    auto presented = (*host)->present(*frame);
    REQUIRE(presented);
    REQUIRE(presented->status == PresentStatus::succeeded);
    REQUIRE(backend->lastPixelProbe().presentationDrawn);
    const auto& pixel = backend->lastPixelProbe();
    std::cout << "{\"event\":\"b5.gpu.present\",\"frame_id\":" << frame->frameId
              << ",\"pixel_rgba\":[" << unsigned(pixel.rgba[0]) << ',' << unsigned(pixel.rgba[1])
              << ',' << unsigned(pixel.rgba[2]) << ',' << unsigned(pixel.rgba[3])
              << "],\"actual\":true}\n";
    auto closed = (*host)->close();
    INFO((closed ? "" : std::string{closed.error().code()}));
    REQUIRE(closed);
    REQUIRE((*host)->state() == HostState::closed);
    REQUIRE_FALSE(frame->snapshot.objects.empty());
}

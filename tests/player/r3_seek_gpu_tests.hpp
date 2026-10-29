#pragma once

#include "../../engine/playback/src/gameplay_internal.hpp"
#include "../support/environment.hpp"
#include <catch2/generators/catch_generators.hpp>
#include <cuexis/judgement/recovery_codec.hpp>

TEST_CASE("R3 physical GPU Seek verifies target pixels and origin state",
          "[.][device][r3][seek-gpu]") {
    static const bool admitted = [] {
        if (test_support::environmentVariable("R3_SEEK_GPU_BARRIER") == "1") {
            std::cout << "R3_SEEK_GPU_READY\n" << std::flush;
            std::string go;
            REQUIRE(static_cast<bool>(std::getline(std::cin, go)));
            REQUIRE(go == "go");
        }
        return true;
    }();
    REQUIRE(admitted);
    const auto records = GENERATE(33U, 129U);
    const auto targetKind = GENERATE(0U, 1U, 2U, 3U); // origin, early, final, partial
    const bool externalSource = GENERATE(false, true);
    const auto hintKind = GENERATE(0U, 1U, 2U);
    const bool canonical = GENERATE(false, true);
    auto runtime = platform_sdl::SdlRuntime::create();
    REQUIRE(runtime);
    render_opengl::OpenGlConfig gl;
    gl.vsync = false;
    auto configured = render_opengl::configureOpenGlContext(*runtime, gl);
    REQUIRE(configured);
    platform_sdl::WindowConfig windowConfig;
    windowConfig.width = 1920;
    windowConfig.height = 1080;
    windowConfig.highDpi = false;
    windowConfig.title = "Cuexis R3 GPU Seek diagnostic";
    auto window = platform_sdl::SdlWindow::create(*runtime, windowConfig);
    REQUIRE(window);
    auto* native = static_cast<SDL_Window*>(window->lease().nativeHandle());
    const bool hidden = test_support::environmentVariable("R3_SEEK_GPU_VISIBLE") != "1";
    if (hidden)
        REQUIRE(SDL_HideWindow(native));
    int width{}, height{};
    REQUIRE(SDL_GetWindowSizeInPixels(native, &width, &height));
    REQUIRE(width == 1920);
    REQUIRE(height == 1080);
    auto backend = render_opengl::OpenGlBackend::create(*window, std::move(*configured));
    REQUIRE(backend);
    REQUIRE(render_opengl::detail::OpenGlDiagnosticsAccess::setPixelReadback(*backend, true));
    int swapInterval{};
    REQUIRE(SDL_GL_GetSwapInterval(&swapInterval));
    REQUIRE(swapInterval == 0);
    auto settings = options();
    settings.viewport = {1920, 1080};
    settings.seekIdentity = canonical ? player::SeekIdentityPolicy::canonicalContent
                                      : player::SeekIdentityPolicy::owningSource;
    const ClockMappingConfiguration mapping{{0, 1}, {0, 1}, {0, 1}, {1, 1}};
    settings.realtime =
        SessionRealtimeStart{{1000000, 10, 128, "", true}, mapping, {}, "r3.gpu.seek"};
    player::ExecutionHostPorts ports{factory(), audio_sdl::WavDecoder::decode,
                                     []() -> core::Result<void> {
                                         SDL_Event event;
                                         while (SDL_PollEvent(&event)) {
                                         }
                                         return {};
                                     }};
    test_support::SessionTestContent fixture;
    test_support::rpa(fixture, mapping);
    const auto& identity = fixture.request.assembly.sources[0].document.requirements[0].identity;
    GameplayRequirementRef ref{identity.chartEntryId,       identity.invocationId,
                               identity.moduleId,           identity.exportId,
                               identity.requirementLocalId, {}};
    for (const auto& step : identity.emissionPath)
        ref.emissionPath.push_back({step.nodeId, step.repeatIndex});
    // Origin has no Miss and shows orange; replayed Miss hides it until reset.
    fixture.config.bindings = {{"binding.one",
                                ref,
                                GameplayPhase::Tap,
                                GameplayOutcome::Miss,
                                GameplayTimingClass::Any,
                                "019b0000-0000-7abc-8def-000000000002",
                                false,
                                {0},
                                {}}};
    const auto content = fixture.content();
    auto created = player::ExecutionHost::create(*backend, settings, std::move(ports));
    REQUIRE(created);
    auto host = std::move(*created);
    REQUIRE(host->load(test_support::presentation(true), content));
    REQUIRE_FALSE(host->hasAudio());
    std::int64_t target{1103};
    std::uint64_t sequence{};
    player::ExecutionHost::ClockSource clock =
        [&](const HostContext& context) -> core::Result<ClockObservation> {
        ClockObservation observation{};
        observation.monotonicDomain = "r3.gpu.seek";
        observation.segment = context.clockSegment;
        observation.publicationSequence = ++sequence;
        observation.sampleMonotonicUs = std::chrono::duration_cast<std::chrono::microseconds>(
                                            std::chrono::steady_clock::now().time_since_epoch())
                                            .count();
        observation.validUntilMonotonicUs = observation.sampleMonotonicUs + 1000000;
        observation.presented = ClockFramePosition{target, 1000000};
        observation.source = observation.maximumPresented = observation.presented;
        observation.playback = ClockPlaybackState::Playing;
        observation.quality = ClockQuality::Exact;
        return observation;
    };
    for (unsigned i = 0; i < records; ++i) {
        target = 1103 + 2 * i;
        auto observed = clock(host->context());
        REQUIRE(observed);
        REQUIRE(host->advance(*observed));
    }
    auto original = host->archive();
    REQUIRE(original);
    auto encoded = original->toBytes();
    REQUIRE(encoded); // toBytes does not populate the canonical-identity cache.
    auto decoded =
        GameplayReplay::fromBytes(*encoded, content, {encoded->size(), 10000, 10000000, true});
    REQUIRE(decoded);
    const auto archive = externalSource ? *decoded : *original;
    const auto horizon = targetKind == 0   ? 0
                         : targetKind == 2 ? target
                                           : 1103 + 2 * (records / 2) + (targetKind == 3);
    const auto* basis = GameplayAccess::content(content);
    const auto* external = GameplayAccess::replay(*decoded);
    REQUIRE(basis);
    REQUIRE(external);
    auto oracle = judgement::JudgementSession::create();
    REQUIRE(oracle);
    REQUIRE(oracle->configure(basis->configuration));
    REQUIRE(oracle->prepare(basis->capsule.gameplay));
    const auto oracleCut = judgement::replayCutAt(external->archive, judgement::Tick{horizon});
    auto reference = oracle->forkForSeek(external->archive, judgement::Tick{horizon}, oracleCut);
    REQUIRE(reference);
    auto expected = reference->snapshot();
    REQUIRE(expected);
    auto expectedBytes = judgement::encodeSnapshot(*expected);
    REQUIRE(expectedBytes);
    REQUIRE(host->pause(clock));
    target = horizon;
    const auto begin = std::chrono::steady_clock::now();
    std::vector<GameplayCheckpoint> checkpoints;
    if (hintKind) {
        const auto* source = GameplayAccess::replay(archive);
        REQUIRE(source);
        auto checkpoint =
            judgement::JudgementSession::checkpoint(source->archive, judgement::Tick{1103});
        REQUIRE(checkpoint);
        if (hintKind == 2)
            checkpoint = judgement::ReplayCheckpoint{checkpoint->archive, checkpoint->cut,
                                                     checkpoint->horizon, checkpoint->state};
        checkpoints.push_back(GameplayAccess::checkpoint(std::move(*checkpoint)));
    }
    const auto hintsReady = std::chrono::steady_clock::now();
    auto cut = host->replayCut(archive, {horizon});
    REQUIRE(cut);
    REQUIRE(host->seek(archive, *cut, {horizon}, clock, checkpoints));
    const auto ready = std::chrono::steady_clock::now();
    CHECK(host->state() == HostState::running);
    auto frame = host->sample(
        horizon, {static_cast<double>(horizon) / 1000.0, 0, host->context().clockSegment}, false);
    REQUIRE(frame);
    const auto sampled = std::chrono::steady_clock::now();
    auto receipt = host->present(*frame);
    REQUIRE(receipt);
    const auto pixel = backend->lastPixelProbe(); // submit performs synchronous glReadPixels.
    const auto end = std::chrono::steady_clock::now();
    CHECK(frame->presentationTick == std::optional<std::int64_t>{horizon});
    CHECK(frame->context == host->context());
    CHECK(receipt->status == PresentStatus::succeeded);
    CHECK(receipt->context == frame->context);
    CHECK(receipt->frameId == frame->frameId);
    CHECK(receipt->projectionScope == frame->projectionScope);
    REQUIRE(frame->snapshotView().objects.size() == 1);
    CHECK(frame->snapshotView().viewportWidth == 1920);
    CHECK(frame->snapshotView().viewportHeight == 1080);
    const bool visible = targetKind == 0;
    CHECK(frame->snapshotView().objects[0].visible == visible);
    CHECK(pixel.presentationDrawn == visible);
    const std::array<std::uint8_t, 4> orange{255, 204, 51, 255}, background{14, 16, 18, 255};
    CHECK(pixel.rgba == (visible ? orange : background));
    auto actual = host->snapshotGameplay();
    REQUIRE(actual);
    auto actualBytes = actual->toBytes();
    REQUIRE(actualBytes);
    CHECK(*actualBytes == *expectedBytes);
    // Attribute a fresh target-frame delay separately from a second identical GPU submission.
    // This second draw never replaces the cold Host interval or its receipt/state checks.
    const auto repeatBegin = std::chrono::steady_clock::now();
    auto repeated = backend->submit(frame->snapshotView());
    REQUIRE(repeated);
    const auto repeatSubmitted = std::chrono::steady_clock::now();
    REQUIRE(backend->present());
    const auto repeatEnd = std::chrono::steady_clock::now();
    CHECK(backend->lastPixelProbe().rgba == pixel.rgba);
    std::cout << "{\"event\":\"r3.seek.gpu\",\"records\":" << records
              << ",\"target_kind\":" << targetKind << ",\"external\":" << externalSource
              << ",\"hint_kind\":" << hintKind << ",\"canonical\":" << canonical
              << ",\"hint_certified\":" << (hintKind == 1 && !externalSource)
              << ",\"hint_eligible\":" << (hintKind == 1 && !externalSource && horizon >= 1103)
              << ",\"horizon\":" << horizon << ",\"hint_preparation_us\":"
              << std::chrono::duration_cast<std::chrono::microseconds>(hintsReady - begin).count()
              << ",\"request_to_ready_us\":"
              << std::chrono::duration_cast<std::chrono::microseconds>(ready - begin).count()
              << ",\"request_to_gpu_readback_us\":"
              << std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count()
              << ",\"sample_us\":"
              << std::chrono::duration_cast<std::chrono::microseconds>(sampled - ready).count()
              << ",\"host_present_us\":"
              << std::chrono::duration_cast<std::chrono::microseconds>(end - sampled).count()
              << ",\"repeat_submit_us\":"
              << std::chrono::duration_cast<std::chrono::microseconds>(repeatSubmitted -
                                                                       repeatBegin)
                     .count()
              << ",\"repeat_swap_us\":"
              << std::chrono::duration_cast<std::chrono::microseconds>(repeatEnd - repeatSubmitted)
                     .count()
              << ",\"hidden_window\":" << hidden << ",\"visible\":" << visible
              << ",\"width\":" << width << ",\"height\":" << height << ",\"pixel_rgba\":["
              << unsigned(pixel.rgba[0]) << ',' << unsigned(pixel.rgba[1]) << ','
              << unsigned(pixel.rgba[2]) << ',' << unsigned(pixel.rgba[3]) << "],\"renderer\":\""
              << backend->info().renderer << "\",\"vendor\":\"" << backend->info().vendor
              << "\",\"gl_version\":\"" << backend->info().version
              << "\",\"swap_interval\":" << swapInterval
              << ",\"readback_scope\":\"back-buffer\",\"formal\":false}\n";
    REQUIRE(host->close());
    REQUIRE(host->closeOwnersExited());
}

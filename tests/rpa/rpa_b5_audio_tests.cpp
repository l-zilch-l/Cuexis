#include "../../engine/playback/src/audio_worker.hpp"
#include "../audio/fake_audio_transport.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace cuexis;
using namespace cuexis::playback::detail;
namespace {
auto receive(AudioWorker& worker, std::uint64_t id) -> AudioWorkerReply {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{3};
    while (std::chrono::steady_clock::now() < deadline) {
        auto result = worker.poll(id);
        REQUIRE(result);
        if (*result) {
            REQUIRE(**result);
            auto reply = std::move(***result);
            REQUIRE(worker.acknowledge(id));
            return reply;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    FAIL("Audio owner did not publish a terminal reply");
    return {};
}
struct Device final : OwnedAudioDevice {
    explicit Device(audio::AudioClipStore& store, std::atomic<unsigned>& destroyed,
                    bool sampled = false)
        : inner(store), destroyed(destroyed), owner(std::this_thread::get_id()), sampled(sampled) {}
    ~Device() override {
        if (owner != std::this_thread::get_id())
            std::terminate();
        ++destroyed;
    }
    auto transport() noexcept -> audio::IAudioTransport& override {
        return inner;
    }
    auto isolateCallbacks() -> core::Result<bool> override {
        return true;
    }
    auto recheckDevice() -> core::Result<void> override {
        return {};
    }
    auto clockObservation() const noexcept -> std::optional<OwnedAudioClockObservation> override {
        if (!sampled)
            return {};
        return OwnedAudioClockObservation{inner.snapshot(), 123, 48000, 7};
    }
    test_support::FakeAudioTransport inner;
    std::atomic<unsigned>& destroyed;
    std::thread::id owner;
    bool sampled;
};
} // namespace
TEST_CASE("Audio publication preserves device observation time across service polls", "[rpa][b5]") {
    AudioWorkerOptions options;
    options.testOnly = true;
    options.sessionScope = 1;
    options.timeDomain = 1;
    options.capacity = 16;
    options.controlReserve = 4;
    std::atomic<unsigned> destroyed{};
    auto worker = AudioWorker::start(
        options,
        [&](audio::AudioClipStore& store) -> core::Result<std::unique_ptr<OwnedAudioDevice>> {
            return std::unique_ptr<OwnedAudioDevice>{
                std::make_unique<Device>(store, destroyed, true)};
        },
        [](std::span<const std::byte>) -> core::Result<audio::AudioClip> {
            return audio::AudioClip::create(48000, 1, std::vector<float>(48000));
        });
    REQUIRE(worker);
    std::this_thread::sleep_for(std::chrono::milliseconds{20});
    auto before = (*worker)->publication();
    REQUIRE(before);
    std::this_thread::sleep_for(std::chrono::milliseconds{20});
    auto after = (*worker)->publication();
    REQUIRE(after);
    CHECK(after->sequence > before->sequence);
    CHECK(before->sampleMonotonicUs == 123);
    CHECK(after->sampleMonotonicUs == 123);
    CHECK(after->maximumPresentedFrame == 48000);
    CHECK(after->errorBoundUs == 7);
    (*worker)->requestShutdown();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{3};
    while (!(*worker)->exited() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    REQUIRE((*worker)->joinExited());
    CHECK(destroyed == 1);
}
TEST_CASE("B5 owner channel reserves control and reply slots in FIFO order", "[rpa][b5]") {
    OwnerChannel<int, int> channel{3, 1};
    auto first = channel.post(10, false);
    auto second = channel.post(20, false);
    REQUIRE(first);
    REQUIRE(second);
    REQUIRE_FALSE(channel.post(30, false));
    auto control = channel.post(40, true);
    REQUIRE(control);
    REQUIRE_FALSE(channel.ack(*first));
    for (const auto expected : {10, 20, 40}) {
        auto job = channel.take();
        REQUIRE(job);
        REQUIRE(job->second == expected);
        channel.complete(job->first, core::Result<int>{expected + 1});
    }
    REQUIRE_FALSE(channel.post(50, true));
    auto reply = channel.poll(*first);
    REQUIRE(reply);
    REQUIRE(*reply);
    REQUIRE(**reply);
    REQUIRE(***reply == 11);
    REQUIRE(channel.ack(*first));
    REQUIRE(channel.post(50, false));
    channel.complete(*second, core::Result<int>{22});
    REQUIRE(channel.fault() == OwnerChannel<int, int>::Fault::conflict);
    REQUIRE_FALSE(channel.post(60, false));
    REQUIRE(channel.ack(*control));
    REQUIRE(channel.post(70, true));
    channel.closeAdmission();
    REQUIRE_FALSE(channel.post(80, true));
}
TEST_CASE("B5 audio service owns device decode and publication without render drive", "[rpa][b5]") {
    AudioWorkerOptions options;
    options.testOnly = true;
    options.sessionScope = 1;
    options.timeDomain = 77;
    options.capacity = 32;
    options.controlReserve = 8;
    std::atomic<unsigned> destroyed{};
    std::atomic<bool> decodeOnWorker{};
    const auto main = std::this_thread::get_id();
    auto worker = AudioWorker::start(
        options,
        [&](audio::AudioClipStore& store) -> core::Result<std::unique_ptr<OwnedAudioDevice>> {
            return std::unique_ptr<OwnedAudioDevice>{std::make_unique<Device>(store, destroyed)};
        },
        [&](std::span<const std::byte>) -> core::Result<audio::AudioClip> {
            decodeOnWorker.store(std::this_thread::get_id() != main);
            return audio::AudioClip::create(48000, 1, std::vector<float>(48000));
        });
    REQUIRE(worker);
    AudioWorkerCommand decode;
    decode.kind = AudioWorkerCommandKind::decode;
    decode.candidateId = 1;
    decode.encoded = {std::byte{1}};
    auto decodedId = (*worker)->post(decode, false);
    REQUIRE(decodedId);
    auto decoded = receive(**worker, *decodedId);
    REQUIRE(decoded.clip);
    REQUIRE(decodeOnWorker.load());
    auto duplicateId = (*worker)->post(decode, false);
    REQUIRE(duplicateId);
    REQUIRE(receive(**worker, *duplicateId).clip == decoded.clip);
    AudioWorkerCommand command;
    command.kind = AudioWorkerCommandKind::prepare;
    command.step.key = {1, 1, Participant::audio, StepPhase::prepare, 0};
    command.step.target.context.sessionScope = 1;
    command.step.target.context.controlSequence = 1;
    command.step.target.candidateId = 1;
    command.step.target.audioClip = decoded.clip;
    command.step.target.audioPositionMs = 0;
    auto preparedId = (*worker)->post(command, false);
    REQUIRE(preparedId);
    REQUIRE(receive(**worker, *preparedId).step->status == StepStatus::applied);
    command.kind = AudioWorkerCommandKind::activate;
    command.step.key.phase = StepPhase::activate;
    auto activatedId = (*worker)->post(command, false);
    REQUIRE(activatedId);
    REQUIRE(receive(**worker, *activatedId).step->status == StepStatus::applied);
    auto before = (*worker)->publication();
    REQUIRE(before);
    std::this_thread::sleep_for(std::chrono::milliseconds{40});
    auto after = (*worker)->publication();
    REQUIRE(after);
    REQUIRE(after->sequence > before->sequence);
    REQUIRE(after->metrics.serviceCount > before->metrics.serviceCount);
    REQUIRE(after->timeDomain == 77);
    REQUIRE(after->sampleMonotonicUs >= before->sampleMonotonicUs);
    REQUIRE_FALSE(after->serviceError);
    REQUIRE(after->fault == AudioPublication::Fault::none);
    command.kind = AudioWorkerCommandKind::prepareActive;
    command.step.key.operationId = 2;
    command.step.target.expectedGeneration = 1;
    command.step.target.context.controlSequence = 2;
    command.step.key.phase = StepPhase::prepare;
    auto playingPrepare = (*worker)->post(command, false);
    REQUIRE(playingPrepare);
    REQUIRE(receive(**worker, *playingPrepare).step->status == StepStatus::rejectedNotApplied);
    command.step.key.operationId = 3;
    command.step.target.context.controlSequence = 3;
    command.step.key.phase = StepPhase::fence;
    command.kind = AudioWorkerCommandKind::fence;
    auto fenced = (*worker)->post(command, true);
    REQUIRE(fenced);
    REQUIRE(receive(**worker, *fenced).step->status == StepStatus::applied);
    command.step.key.operationId = 4;
    command.step.target.context.controlSequence = 4;
    command.step.key.phase = StepPhase::prepare;
    command.kind = AudioWorkerCommandKind::prepareActive;
    auto preparedActive = (*worker)->post(command, false);
    REQUIRE(preparedActive);
    const auto readiness = receive(**worker, *preparedActive);
    REQUIRE(readiness.step->status == StepStatus::applied);
    REQUIRE(readiness.step->effects.generation == 1);
    command.step.key.operationId = 5;
    command.step.target.context.controlSequence = 5;
    for (const auto phase :
         {StepPhase::stop, StepPhase::isolate, StepPhase::retire, StepPhase::close}) {
        command.step.key.phase = phase;
        command.kind = phase == StepPhase::stop      ? AudioWorkerCommandKind::stop
                       : phase == StepPhase::isolate ? AudioWorkerCommandKind::isolate
                       : phase == StepPhase::retire  ? AudioWorkerCommandKind::retire
                                                     : AudioWorkerCommandKind::close;
        auto id = (*worker)->post(command, true);
        REQUIRE(id);
        auto reply = receive(**worker, *id);
        REQUIRE(reply.step);
        REQUIRE(reply.step->status == StepStatus::applied);
        if (phase == StepPhase::retire)
            REQUIRE(reply.step->effects.referencesReleased);
    }
    (*worker)->requestShutdown();
    for (unsigned i = 0; i < 1000 && !(*worker)->exited(); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    REQUIRE((*worker)->exited());
    REQUIRE((*worker)->joinExited());
    REQUIRE(destroyed.load() == 1);
    REQUIRE_FALSE((*worker)->post(command, true));
}
TEST_CASE("B5 audio factory exception retires its local device before exit publication",
          "[rpa][b5]") {
    AudioWorkerOptions options;
    options.testOnly = true;
    options.sessionScope = 1;
    options.timeDomain = 1;
    options.capacity = 16;
    options.controlReserve = 4;
    std::atomic<unsigned> destroyed{};
    auto worker = AudioWorker::start(
        options,
        [&](audio::AudioClipStore& store) -> core::Result<std::unique_ptr<OwnedAudioDevice>> {
            auto device = std::make_unique<Device>(store, destroyed);
            throw std::runtime_error{"Injected factory failure after local device creation"};
        },
        [](std::span<const std::byte>) {
            return audio::AudioClip::create(48000, 1, std::vector<float>(48));
        });
    REQUIRE(worker);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{3};
    while (!(*worker)->exited() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    REQUIRE((*worker)->exited());
    REQUIRE(destroyed.load() == 1);
    auto published = (*worker)->publication();
    REQUIRE(published);
    REQUIRE(published->fault == AudioPublication::Fault::allocation);
    REQUIRE(published->serviceError);
    REQUIRE((*worker)->joinExited());
    REQUIRE_FALSE((*worker)->post({}, true));
}

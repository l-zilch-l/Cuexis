#include "audio_worker.hpp"
#include <algorithm>
#include <stdexcept>

namespace cuexis::playback::detail {
namespace {
auto error(const char* code) -> core::Error {
    return {std::string{"host.rpa."} + code, "Audio worker rejected operation"};
}
auto nowUs() noexcept -> std::int64_t {
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
} // namespace
struct AudioWorker::State final {
    State(AudioWorkerOptions o, DeviceFactory f, DecodeClip d)
        : options(o), factory(std::move(f)), decode(std::move(d)),
          channel(o.capacity, o.controlReserve) {}
    AudioWorkerOptions options;
    DeviceFactory factory;
    DecodeClip decode;
    OwnerChannel<AudioWorkerCommand, AudioWorkerReply> channel;
    std::atomic<bool> shutdown{}, exited{};
    std::atomic<AudioPublication::Fault> fault{AudioPublication::Fault::none};
    std::thread thread;
    mutable std::mutex publicationMutex;
    AudioPublication publication;
};
AudioWorker::AudioWorker(std::unique_ptr<State> state) : state_(std::move(state)) {}
auto AudioWorker::start(AudioWorkerOptions options, DeviceFactory factory, DecodeClip decode)
    -> core::Result<std::unique_ptr<AudioWorker>> try {
    if (!options.testOnly || !options.sessionScope || !options.timeDomain || !factory || !decode ||
        !options.capacity || options.controlReserve >= options.capacity ||
        options.serviceInterval.count() < 1)
        return core::unexpected(error("profile_incomplete"));
    auto worker = std::unique_ptr<AudioWorker>{
        new AudioWorker{std::make_unique<State>(options, std::move(factory), std::move(decode))}};
    auto* state = worker->state_.get();
    state->thread = std::thread{[state] { run(*state); }};
    return worker;
} catch (...) {
    return core::unexpected(error("capacity_exceeded"));
}
AudioWorker::~AudioWorker() {
    requestShutdown();
    if (state_->thread.joinable())
        state_->thread.join();
}
auto AudioWorker::post(AudioWorkerCommand command, bool control) -> core::Result<std::uint64_t> {
    if (state_->shutdown.load(std::memory_order_acquire) || exited())
        return core::unexpected(error("owner_closed"));
    return state_->channel.post(std::move(command), control);
}
auto AudioWorker::poll(std::uint64_t id)
    -> core::Result<std::optional<core::Result<AudioWorkerReply>>> {
    return state_->channel.poll(id);
}
auto AudioWorker::acknowledge(std::uint64_t id) -> core::Result<void> {
    return state_->channel.ack(id);
}
void AudioWorker::waitFor(std::chrono::milliseconds duration) {
    state_->channel.waitFor(duration);
}
auto AudioWorker::publication() const -> core::Result<AudioPublication> try {
    std::lock_guard lock{state_->publicationMutex};
    auto result = state_->publication;
    result.fault = state_->fault.load(std::memory_order_acquire);
    return result;
} catch (...) {
    return core::unexpected(error("capacity_exceeded"));
}
void AudioWorker::requestShutdown() {
    state_->channel.closeAdmission();
    state_->shutdown.store(true, std::memory_order_release);
    state_->channel.wake();
}
auto AudioWorker::exited() const noexcept -> bool {
    return state_->exited.load(std::memory_order_acquire);
}
auto AudioWorker::joinExited() -> core::Result<void> try {
    if (!exited())
        return core::unexpected(error("control_pending"));
    if (state_->thread.joinable())
        state_->thread.join();
    return {};
} catch (...) {
    return core::unexpected(error("control_pending"));
}
void AudioWorker::run(State& state) noexcept {
    OwnedAudioDevice* clockDevice{};
    const auto publish = [&](audio::IAudioTransport* transport,
                             std::optional<core::Error> failure = {}) {
        AudioPublication next;
        if (transport) {
            next.clock = transport->snapshot();
            next.metrics = transport->metrics();
            next.settings = transport->effectiveSettings();
        }
        next.timeDomain = state.options.timeDomain;
        next.sampleMonotonicUs = nowUs();
        if (transport && clockDevice) {
            if (auto sample = clockDevice->clockObservation()) {
                next.clock = sample->clock;
                next.sampleReady = sample->sampleMonotonicUs.has_value();
                next.sampleMonotonicUs = sample->sampleMonotonicUs.value_or(next.sampleMonotonicUs);
                next.maximumPresentedFrame = sample->maximumPresentedFrame;
                next.errorBoundUs = sample->errorBoundUs;
            }
        }
        next.serviceError = std::move(failure);
        next.fault = state.fault.load(std::memory_order_acquire);
        std::lock_guard lock{state.publicationMutex};
        if (state.publication.sequence == UINT64_MAX)
            throw std::overflow_error{"Audio publication sequence exhausted"};
        next.sequence = state.publication.sequence + 1;
        state.publication = std::move(next);
    };
    try {
        // All dependencies are constructed and destructed here, including the transport/store.
        audio::AudioClipStore store{state.options.clipLimits};
        auto device = state.factory(store);
        if (!device || !*device) {
            state.fault.store(AudioPublication::Fault::device, std::memory_order_release);
            publish(nullptr, device ? error("profile_incomplete") : device.error());
        } else {
            clockDevice = device->get();
            auto& transport = (*device)->transport();
            auto owner = AudioOwner::create(
                state.options.sessionScope, transport,
                [&device] { return (*device)->isolateCallbacks(); }, state.options.capacity,
                state.options.controlReserve);
            if (!owner) {
                state.fault.store(AudioPublication::Fault::allocation, std::memory_order_release);
                publish(&transport, owner.error());
            } else {
                struct Clip final {
                    std::uint64_t candidate{};
                    audio::AudioClipHandle handle;
                    std::vector<std::byte> encoded;
                };
                std::vector<Clip> clips;
                clips.reserve(state.options.clipLimits.maxClips);
                auto nextService = std::chrono::steady_clock::now();
                while (!state.shutdown.load(std::memory_order_acquire)) {
                    if (auto job = state.channel.take()) {
                        const auto execute = [&]() -> core::Result<AudioWorkerReply> {
                            const auto& command = job->second;
                            if (command.kind == AudioWorkerCommandKind::decode) {
                                if (!command.candidateId)
                                    return core::unexpected(error("context_stale"));
                                for (const auto& clip : clips)
                                    if (clip.candidate == command.candidateId) {
                                        if (clip.encoded != command.encoded)
                                            return core::unexpected(error("message_conflict"));
                                        return AudioWorkerReply{clip.handle, {}};
                                    }
                                if (clips.size() == state.options.clipLimits.maxClips)
                                    return core::unexpected(error("capacity_exceeded"));
                                // Reserve owning decode identity before publishing a store handle.
                                auto encoded = command.encoded;
                                auto decoded = state.decode(encoded);
                                if (!decoded)
                                    return core::unexpected(decoded.error());
                                auto handle = store.registerClip(std::move(*decoded));
                                if (!handle)
                                    return core::unexpected(handle.error());
                                clips.push_back({command.candidateId, *handle, std::move(encoded)});
                                return AudioWorkerReply{*handle, {}};
                            }
                            core::Result<StepReceipt> receipt =
                                core::unexpected(error("context_stale"));
                            switch (command.kind) {
                            case AudioWorkerCommandKind::prepareActive:
                                receipt = owner->prepareActive(command.step);
                                break;
                            case AudioWorkerCommandKind::prepare: {
                                if (!command.step.target.audioClip)
                                    return core::unexpected(error("context_stale"));
                                auto lease = store.lease(*command.step.target.audioClip);
                                if (!lease)
                                    return core::unexpected(lease.error());
                                receipt = owner->prepare(command.step, std::move(*lease));
                                break;
                            }
                            case AudioWorkerCommandKind::fence:
                                receipt = owner->pause(command.step);
                                break;
                            case AudioWorkerCommandKind::activate:
                                receipt = owner->activate(command.step);
                                break;
                            case AudioWorkerCommandKind::stop:
                                receipt = owner->stop(command.step);
                                break;
                            case AudioWorkerCommandKind::isolate:
                                receipt = owner->isolate(command.step);
                                break;
                            case AudioWorkerCommandKind::retire:
                                receipt = owner->retire(command.step);
                                break;
                            case AudioWorkerCommandKind::close:
                                receipt = owner->close(command.step);
                                break;
                            case AudioWorkerCommandKind::cancel:
                                receipt = owner->cancel(command.step);
                                break;
                            case AudioWorkerCommandKind::query:
                                receipt = owner->steps().query(command.queryKey);
                                break;
                            default:
                                break;
                            }
                            if (!receipt)
                                return core::unexpected(receipt.error());
                            if (command.kind == AudioWorkerCommandKind::retire &&
                                receipt->status == StepStatus::applied &&
                                receipt->effects.referencesReleased) {
                                for (auto it = clips.begin(); it != clips.end();) {
                                    if (!command.step.target.candidateId ||
                                        command.step.target.candidateId == it->candidate) {
                                        auto removed = store.remove(it->handle);
                                        if (!removed)
                                            return core::unexpected(removed.error());
                                        it = clips.erase(it);
                                    } else
                                        ++it;
                                }
                            }
                            return AudioWorkerReply{{}, std::move(*receipt)};
                        };
                        try {
                            state.channel.complete(job->first, execute());
                        } catch (...) {
                            state.channel.complete(job->first,
                                                   core::unexpected(error("capacity_exceeded")));
                        }
                    }
                    const auto now = std::chrono::steady_clock::now();
                    if (now >= nextService) {
                        auto valid = (*device)->recheckDevice();
                        auto serviced = transport.service();
                        std::optional<core::Error> failure;
                        if (!valid)
                            failure = valid.error();
                        else if (!serviced)
                            failure = serviced.error();
                        if (failure)
                            state.fault.store(valid ? AudioPublication::Fault::service
                                                    : AudioPublication::Fault::device,
                                              std::memory_order_release);
                        publish(&transport, std::move(failure));
                        nextService = now + state.options.serviceInterval;
                    }
                    state.channel.waitFor(state.options.serviceInterval);
                }
                // Shutdown never waits for a main-thread GPU action. Destruction remains local.
                auto stopped = transport.snapshot().source.state == audio::PlaybackState::Empty
                                   ? core::Result<void>{}
                                   : transport.stop();
                auto isolated = (*device)->isolateCallbacks();
                auto unloaded = isolated && *isolated
                                    ? transport.unload()
                                    : core::Result<void>{core::unexpected(error("device_pending"))};
                std::optional<core::Error> failure;
                if (!stopped)
                    failure = stopped.error();
                else if (!isolated)
                    failure = isolated.error();
                else if (!*isolated)
                    failure = error("device_pending");
                else if (!unloaded)
                    failure = unloaded.error();
                publish(&transport, std::move(failure));
            }
        }
    } catch (...) {
        state.fault.store(AudioPublication::Fault::allocation, std::memory_order_release);
        try {
            publish(nullptr, error("capacity_exceeded"));
        } catch (...) {
        }
    }
    state.channel.closeAdmission();
    while (auto pending = state.channel.take())
        state.channel.complete(pending->first, core::unexpected(error("owner_closed")));
    state.exited.store(true, std::memory_order_release);
}
} // namespace cuexis::playback::detail

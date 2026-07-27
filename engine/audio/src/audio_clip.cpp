#include <cuexis/audio/audio_clip.hpp>

#include <cuexis/core/error.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <string>
#include <thread>
#include <utility>

namespace cuexis::audio {
namespace {

std::atomic<std::uint64_t> nextStoreToken{1};

[[nodiscard]] auto allocateToken() noexcept -> std::uint64_t {
    auto token = nextStoreToken.fetch_add(1, std::memory_order_relaxed);
    if (token == 0) {
        token = nextStoreToken.fetch_add(1, std::memory_order_relaxed);
    }
    return token;
}

} // namespace

AudioClip::AudioClip(std::uint32_t sampleRate, std::uint32_t channels, std::int64_t frameCount,
                     std::vector<float> samples) noexcept
    : sampleRate_(sampleRate), channels_(channels), frameCount_(frameCount),
      samples_(std::move(samples)) {}

auto AudioClip::create(std::uint32_t sampleRate, std::uint32_t channels,
                       std::vector<float> interleavedSamples) -> core::Result<AudioClip> {
    if (sampleRate < 8000 || sampleRate > 192000) {
        return core::unexpected(core::Error{"audio.clip.sample_rate_invalid",
                                            "AudioClip sample rate must be in [8000, 192000]"});
    }
    if (channels != 1 && channels != 2) {
        return core::unexpected(core::Error{"audio.clip.channels_invalid",
                                            "AudioClip must contain one or two channels"});
    }
    if (interleavedSamples.empty() || interleavedSamples.size() % channels != 0) {
        return core::unexpected(core::Error{"audio.clip.frame_alignment_invalid",
                                            "AudioClip samples must contain complete frames"});
    }
    if (interleavedSamples.size() > maxDecodedClipBytes / sizeof(float)) {
        return core::unexpected(core::Error{"audio.clip.decoded_limit",
                                            "AudioClip exceeds the decoded PCM byte limit"});
    }
    if (!std::all_of(interleavedSamples.begin(), interleavedSamples.end(),
                     [](float sample) { return std::isfinite(sample); })) {
        return core::unexpected(
            core::Error{"audio.clip.sample_non_finite", "AudioClip samples must all be finite"});
    }
    const auto frames = interleavedSamples.size() / channels;
    if (frames > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max())) {
        return core::unexpected(
            core::Error{"audio.clip.frame_count_overflow", "AudioClip frame count exceeds int64"});
    }
    return AudioClip{sampleRate, channels, static_cast<std::int64_t>(frames),
                     std::move(interleavedSamples)};
}

std::uint32_t AudioClip::sampleRate() const noexcept {
    return sampleRate_;
}

std::uint32_t AudioClip::channels() const noexcept {
    return channels_;
}

std::int64_t AudioClip::frameCount() const noexcept {
    return frameCount_;
}

double AudioClip::durationMs() const noexcept {
    return static_cast<double>(frameCount_) * 1000.0 / static_cast<double>(sampleRate_);
}

std::span<const float> AudioClip::samples() const noexcept {
    return samples_;
}

std::size_t AudioClip::byteSize() const noexcept {
    return samples_.size() * sizeof(float);
}

bool AudioClipHandle::valid() const noexcept {
    return index != invalidIndex && generation != 0 && storeToken != 0;
}

AudioClipLease::AudioClipLease(AudioClipHandle handle,
                               std::shared_ptr<const AudioClip> clip) noexcept
    : handle_(handle), clip_(std::move(clip)) {}

bool AudioClipLease::valid() const noexcept {
    return handle_.valid() && clip_ != nullptr;
}

AudioClipLease::operator bool() const noexcept {
    return valid();
}

AudioClipHandle AudioClipLease::handle() const noexcept {
    return handle_;
}

const AudioClip* AudioClipLease::get() const noexcept {
    return clip_.get();
}

const AudioClip& AudioClipLease::operator*() const noexcept {
    return *clip_;
}

const AudioClip* AudioClipLease::operator->() const noexcept {
    return clip_.get();
}

void AudioClipLease::reset() noexcept {
    handle_ = {};
    clip_.reset();
}

struct AudioClipStore::State final {
    struct Slot final {
        std::uint32_t generation{1};
        std::shared_ptr<const AudioClip> clip;
    };

    explicit State(AudioClipStoreLimits storeLimits)
        : limits(storeLimits), token(allocateToken()), owner(std::this_thread::get_id()) {}

    [[nodiscard]] bool isOwner() const noexcept {
        return owner == std::this_thread::get_id();
    }

    AudioClipStoreLimits limits;
    std::uint64_t token{};
    std::thread::id owner;
    std::vector<Slot> slots;
    std::size_t registeredClips{};
    std::size_t registeredBytes{};
};

AudioClipStore::AudioClipStore(AudioClipStoreLimits limits)
    : state_(std::make_unique<State>(limits)) {}

AudioClipStore::~AudioClipStore() = default;

auto AudioClipStore::registerClip(AudioClip clip) -> core::Result<AudioClipHandle> {
    if (!state_->isOwner()) {
        return core::unexpected(core::Error{"audio.store.not_owner_thread",
                                            "AudioClipStore belongs to another thread"});
    }
    if (state_->limits.maxClips == 0 || state_->limits.maxClipBytes == 0 ||
        state_->limits.maxTotalBytes == 0) {
        return core::unexpected(
            core::Error{"audio.store.limits_invalid", "AudioClipStore limits must be non-zero"});
    }
    const auto bytes = clip.byteSize();
    if (bytes > state_->limits.maxClipBytes) {
        return core::unexpected(
            core::Error{"audio.store.clip_limit", "AudioClip exceeds the store per-clip limit"});
    }
    if (state_->registeredClips >= state_->limits.maxClips ||
        bytes > state_->limits.maxTotalBytes -
                    std::min(state_->registeredBytes, state_->limits.maxTotalBytes)) {
        return core::unexpected(
            core::Error{"audio.store.capacity", "AudioClipStore capacity would be exceeded"});
    }

    auto stored = std::make_shared<const AudioClip>(std::move(clip));
    std::uint32_t index{};
    const auto vacant = std::find_if(state_->slots.begin(), state_->slots.end(),
                                     [](const State::Slot& slot) { return slot.clip == nullptr; });
    if (vacant == state_->slots.end()) {
        if (state_->slots.size() >= AudioClipHandle::invalidIndex) {
            return core::unexpected(
                core::Error{"audio.store.slot_limit", "AudioClipStore slot space is exhausted"});
        }
        index = static_cast<std::uint32_t>(state_->slots.size());
        state_->slots.push_back(State::Slot{.generation = 1, .clip = std::move(stored)});
    } else {
        index = static_cast<std::uint32_t>(std::distance(state_->slots.begin(), vacant));
        vacant->clip = std::move(stored);
    }
    ++state_->registeredClips;
    state_->registeredBytes += bytes;
    return AudioClipHandle{index, state_->slots[index].generation, state_->token};
}

auto AudioClipStore::lease(AudioClipHandle handle) const -> core::Result<AudioClipLease> {
    if (!state_->isOwner()) {
        return core::unexpected(core::Error{"audio.store.not_owner_thread",
                                            "AudioClipStore belongs to another thread"});
    }
    if (!handle.valid() || handle.storeToken != state_->token ||
        handle.index >= state_->slots.size()) {
        return core::unexpected(
            core::Error{"audio.store.handle_invalid", "AudioClipHandle is invalid for this store"});
    }
    const auto& slot = state_->slots[handle.index];
    if (slot.generation != handle.generation || slot.clip == nullptr) {
        return core::unexpected(
            core::Error{"audio.store.handle_stale", "AudioClipHandle is stale"});
    }
    return AudioClipLease{handle, slot.clip};
}

auto AudioClipStore::remove(AudioClipHandle handle) -> core::Result<void> {
    if (!state_->isOwner()) {
        return core::unexpected(core::Error{"audio.store.not_owner_thread",
                                            "AudioClipStore belongs to another thread"});
    }
    auto leased = lease(handle);
    if (!leased) {
        return core::unexpected(std::move(leased.error()));
    }
    auto& slot = state_->slots[handle.index];
    state_->registeredBytes -= slot.clip->byteSize();
    --state_->registeredClips;
    slot.clip.reset();
    ++slot.generation;
    if (slot.generation == 0) {
        ++slot.generation;
    }
    return {};
}

AudioClipStoreMetrics AudioClipStore::metrics() const noexcept {
    return {state_->registeredClips, state_->registeredBytes};
}

std::uint64_t AudioClipStore::storeToken() const noexcept {
    return state_->token;
}

} // namespace cuexis::audio

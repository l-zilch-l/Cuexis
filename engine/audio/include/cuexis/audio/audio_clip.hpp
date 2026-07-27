#pragma once

// Immutable interleaved F32 PCM and an owner-thread weak-handle store.

#include <cuexis/core/result.hpp>

#include <compare>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <vector>

namespace cuexis::audio {

inline constexpr std::size_t maxDecodedClipBytes = 256U * 1024U * 1024U;

class AudioClip final {
  public:
    [[nodiscard]] static auto create(std::uint32_t sampleRate, std::uint32_t channels,
                                     std::vector<float> interleavedSamples)
        -> core::Result<AudioClip>;

    [[nodiscard]] std::uint32_t sampleRate() const noexcept;
    [[nodiscard]] std::uint32_t channels() const noexcept;
    [[nodiscard]] std::int64_t frameCount() const noexcept;
    [[nodiscard]] double durationMs() const noexcept;
    [[nodiscard]] std::span<const float> samples() const noexcept;
    [[nodiscard]] std::size_t byteSize() const noexcept;

  private:
    AudioClip(std::uint32_t sampleRate, std::uint32_t channels, std::int64_t frameCount,
              std::vector<float> samples) noexcept;

    std::uint32_t sampleRate_{};
    std::uint32_t channels_{};
    std::int64_t frameCount_{};
    std::vector<float> samples_;
};

struct AudioClipHandle final {
    static constexpr std::uint32_t invalidIndex = std::numeric_limits<std::uint32_t>::max();

    std::uint32_t index{invalidIndex};
    std::uint32_t generation{};
    std::uint64_t storeToken{};

    [[nodiscard]] bool valid() const noexcept;
    auto operator<=>(const AudioClipHandle&) const = default;
};

class AudioClipLease final {
  public:
    AudioClipLease() = default;
    ~AudioClipLease() = default;

    AudioClipLease(const AudioClipLease&) = delete;
    auto operator=(const AudioClipLease&) -> AudioClipLease& = delete;
    AudioClipLease(AudioClipLease&&) noexcept = default;
    auto operator=(AudioClipLease&&) noexcept -> AudioClipLease& = default;

    [[nodiscard]] bool valid() const noexcept;
    explicit operator bool() const noexcept;
    [[nodiscard]] AudioClipHandle handle() const noexcept;
    [[nodiscard]] const AudioClip* get() const noexcept;
    [[nodiscard]] const AudioClip& operator*() const noexcept;
    [[nodiscard]] const AudioClip* operator->() const noexcept;
    void reset() noexcept;

  private:
    friend class AudioClipStore;
    AudioClipLease(AudioClipHandle handle, std::shared_ptr<const AudioClip> clip) noexcept;

    AudioClipHandle handle_{};
    std::shared_ptr<const AudioClip> clip_;
};

struct AudioClipStoreLimits final {
    std::size_t maxClips{2};
    std::size_t maxClipBytes{maxDecodedClipBytes};
    std::size_t maxTotalBytes{512U * 1024U * 1024U};
};

struct AudioClipStoreMetrics final {
    std::size_t registeredClips{};
    std::size_t registeredBytes{};
};

class AudioClipStore final {
  public:
    explicit AudioClipStore(AudioClipStoreLimits limits = {});
    ~AudioClipStore();

    AudioClipStore(const AudioClipStore&) = delete;
    auto operator=(const AudioClipStore&) -> AudioClipStore& = delete;
    AudioClipStore(AudioClipStore&&) noexcept = delete;
    auto operator=(AudioClipStore&&) noexcept -> AudioClipStore& = delete;

    [[nodiscard]] auto registerClip(AudioClip clip) -> core::Result<AudioClipHandle>;
    [[nodiscard]] auto lease(AudioClipHandle handle) const -> core::Result<AudioClipLease>;
    [[nodiscard]] auto remove(AudioClipHandle handle) -> core::Result<void>;
    [[nodiscard]] AudioClipStoreMetrics metrics() const noexcept;
    [[nodiscard]] std::uint64_t storeToken() const noexcept;

  private:
    struct State;
    std::unique_ptr<State> state_;
};

} // namespace cuexis::audio

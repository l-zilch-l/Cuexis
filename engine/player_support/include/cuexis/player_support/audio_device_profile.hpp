#pragma once

// Internal AudioDeviceProfile v1. Matching uses an injected device list so tests do not
// need SDL. An explicit profile that is missing, too new, or ambiguous is an error.

#include <cuexis/core/result.hpp>

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace cuexis::player_support {

enum class AudioSelectorKind : std::uint8_t {
    SystemDefault,
    Exact,
};

struct AudioDeviceProfile final {
    std::string id{"system-default"};
    AudioSelectorKind selector{AudioSelectorKind::SystemDefault};
    std::string driver{};
    std::string deviceName{};
    std::int64_t outputCorrectionUs{0};
};

struct AudioOutputDevice final {
    std::string driver{};
    std::string deviceName{};
};

struct AudioDeviceMatch final {
    bool systemDefault{false};
    std::string driver{};
    std::string deviceName{};
};

[[nodiscard]] auto loadAudioDeviceProfile(const std::filesystem::path& profilePath,
                                          const std::filesystem::path& schemaPath)
    -> core::Result<AudioDeviceProfile>;

[[nodiscard]] auto matchAudioDevice(const AudioDeviceProfile& profile,
                                    std::span<const AudioOutputDevice> devices)
    -> core::Result<AudioDeviceMatch>;

[[nodiscard]] auto correctedAudioPositionUs(std::int64_t rawAudioPositionUs,
                                            std::int64_t correctionUs)
    -> core::Result<std::int64_t>;

[[nodiscard]] auto reverseSeekSourcePositionUs(std::int64_t targetChartTimeUs,
                                               std::int64_t timingOffsetUs,
                                               std::int64_t correctionUs)
    -> core::Result<std::int64_t>;

} // namespace cuexis::player_support

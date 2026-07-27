#include "frame_diagnostics.hpp"

#include <cuexis/core/error.hpp>
#include <cuexis/version.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace cuexis::player {
namespace {

constexpr std::uint64_t fnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t fnvPrime = 1099511628211ULL;

void hashByte(std::uint64_t& hash, std::uint8_t value) noexcept {
    hash ^= value;
    hash *= fnvPrime;
}

template <typename Integer> void hashInteger(std::uint64_t& hash, Integer value) noexcept {
    using Unsigned = std::make_unsigned_t<Integer>;
    auto bits = static_cast<Unsigned>(value);
    for (std::size_t index = 0; index < sizeof(bits); ++index) {
        hashByte(hash, static_cast<std::uint8_t>(bits & 0xFFU));
        bits >>= 8U;
    }
}

void hashFloat(std::uint64_t& hash, float value) noexcept {
    if (value == 0.0F) {
        value = 0.0F;
    }
    hashInteger(hash, std::bit_cast<std::uint32_t>(value));
}

void hashDouble(std::uint64_t& hash, double value) noexcept {
    if (value == 0.0) {
        value = 0.0;
    }
    hashInteger(hash, std::bit_cast<std::uint64_t>(value));
}

void hashString(std::uint64_t& hash, std::string_view value) noexcept {
    hashInteger(hash, static_cast<std::uint64_t>(value.size()));
    for (const unsigned char character : value) {
        hashByte(hash, character);
    }
}

[[nodiscard]] std::string_view stateName(audio::PlaybackState state) noexcept {
    switch (state) {
    case audio::PlaybackState::Empty:
        return "empty";
    case audio::PlaybackState::Stopped:
        return "stopped";
    case audio::PlaybackState::Playing:
        return "playing";
    case audio::PlaybackState::Paused:
        return "paused";
    case audio::PlaybackState::Ended:
        return "ended";
    case audio::PlaybackState::Error:
        return "error";
    }
    return "unknown";
}

[[nodiscard]] std::string_view modeName(playback::PlaybackMode mode) noexcept {
    switch (mode) {
    case playback::PlaybackMode::ChartClock:
        return "chart_clock";
    case playback::PlaybackMode::HostClock:
        return "host_clock";
    case playback::PlaybackMode::CuexisAudio:
        return "cuexis_audio";
    }
    return "unknown";
}

[[nodiscard]] auto artifactPath(const std::filesystem::path& prefix, std::string_view suffix)
    -> std::filesystem::path {
    auto path = prefix;
    path += suffix;
    return path;
}

[[nodiscard]] auto openArtifact(const std::filesystem::path& path) -> core::Result<std::ofstream> {
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    if (!output) {
        return core::unexpected(
            core::Error{"player.frame_stats.open_failed", "Diagnostic artifact could not be opened"}
                .withContext("artifact", path.filename().string()));
    }
    output.imbue(std::locale::classic());
    output << std::setprecision(std::numeric_limits<double>::max_digits10);
    return output;
}

[[nodiscard]] auto finishArtifact(std::ofstream& output, const std::filesystem::path& path)
    -> core::Result<void> {
    output.flush();
    if (!output) {
        return core::unexpected(core::Error{"player.frame_stats.write_failed",
                                            "Diagnostic artifact could not be written"}
                                    .withContext("artifact", path.filename().string()));
    }
    return {};
}

} // namespace

FrameDiagnostics::FrameDiagnostics(std::filesystem::path prefix, TraceLimits limits)
    : prefix_(std::move(prefix)), limits_(limits) {
    const auto frameCapacity = std::min(limits_.maxRows, limits_.maxBytes / sizeof(FrameRow));
    const auto audioCapacity = std::min(limits_.maxRows, limits_.maxBytes / sizeof(AudioRow));
    frames_.reserve(frameCapacity);
    audio_.reserve(audioCapacity);
}

void FrameDiagnostics::captureFrame(std::uint64_t frameIndex, const playback::RuntimeFrame& frame,
                                    const playback::FrameSnapshot& snapshot) noexcept {
    if (frames_.size() >= frames_.capacity()) {
        ++droppedFrames_;
        return;
    }
    frames_.push_back({frameIndex, frame.chartTimeMs, frame.simulationDeltaTimeMs,
                       frame.timeDiscontinuityId, frameHash(frame, snapshot)});
}

void FrameDiagnostics::captureAudio(std::uint64_t frameIndex, double wallClockMs,
                                    const audio::AudioClockSnapshot& clock,
                                    const audio::AudioMetricsSnapshot& metrics) noexcept {
    if (audio_.size() >= audio_.capacity()) {
        ++droppedAudio_;
        return;
    }
    audio_.push_back({frameIndex, wallClockMs, clock.source.positionMs,
                      clock.estimatedOutputLatencyMs, metrics.queuedFrames, metrics.underrunCount,
                      clock.source.state});
}

auto FrameDiagnostics::exportArtifacts(playback::PlaybackMode mode) const -> core::Result<void> {
    if (prefix_.empty()) {
        return core::unexpected(core::Error{"player.frame_stats.prefix_empty",
                                            "Diagnostic artifact prefix must not be empty"});
    }
    if (!prefix_.parent_path().empty()) {
        std::error_code error;
        std::filesystem::create_directories(prefix_.parent_path(), error);
        if (error) {
            return core::unexpected(
                core::Error{"player.frame_stats.directory_failed",
                            "Diagnostic artifact directory could not be created"});
        }
    }

    const auto framesPath = artifactPath(prefix_, ".frames.csv");
    auto framesOutput = openArtifact(framesPath);
    if (!framesOutput) {
        return core::unexpected(std::move(framesOutput.error()));
    }
    *framesOutput << "frameIndex,chartTimeMs,simulationDeltaTimeMs,discontinuityId,frameHash\r\n";
    for (const auto& row : frames_) {
        *framesOutput << row.frameIndex << ',' << row.chartTimeMs << ','
                      << row.simulationDeltaTimeMs << ',' << row.discontinuityId << ',' << row.hash
                      << "\r\n";
    }
    if (auto finished = finishArtifact(*framesOutput, framesPath); !finished) {
        return finished;
    }

    const auto audioPath = artifactPath(prefix_, ".audio.csv");
    auto audioOutput = openArtifact(audioPath);
    if (!audioOutput) {
        return core::unexpected(std::move(audioOutput.error()));
    }
    *audioOutput << "frameIndex,wallClockMs,sourcePositionMs,estimatedOutputLatencyMs,"
                    "queuedFrames,underrunCount,transportState\r\n";
    for (const auto& row : audio_) {
        *audioOutput << row.frameIndex << ',' << row.wallClockMs << ',' << row.sourcePositionMs
                     << ',' << row.estimatedOutputLatencyMs << ',' << row.queuedFrames << ','
                     << row.underrunCount << ',' << stateName(row.state) << "\r\n";
    }
    if (auto finished = finishArtifact(*audioOutput, audioPath); !finished) {
        return finished;
    }

    const auto metaPath = artifactPath(prefix_, ".meta.json");
    auto metaOutput = openArtifact(metaPath);
    if (!metaOutput) {
        return core::unexpected(std::move(metaOutput.error()));
    }
    *metaOutput << "{\n"
                << "  \"schema\": \"cuexis.frame-stats\",\n"
                << "  \"version\": 1,\n"
                << "  \"framesSchema\": \"cuexis.frame-trace.v1\",\n"
                << "  \"audioSchema\": \"cuexis.audio-telemetry.v1\",\n"
                << "  \"buildVersion\": \"" << version::display << "\",\n"
                << "  \"sdkApiVersion\": \"" << version::sdkApi << "\",\n"
                << "  \"mode\": \"" << modeName(mode) << "\",\n"
                << "  \"frames\": {\"capturedRows\": " << frames_.size()
                << ", \"droppedRows\": " << droppedFrames_
                << ", \"truncated\": " << (droppedFrames_ == 0 ? "false" : "true") << "},\n"
                << "  \"audio\": {\"capturedRows\": " << audio_.size()
                << ", \"droppedRows\": " << droppedAudio_
                << ", \"truncated\": " << (droppedAudio_ == 0 ? "false" : "true") << "}\n"
                << "}\n";
    if (auto finished = finishArtifact(*metaOutput, metaPath); !finished) {
        return finished;
    }
    if (droppedFrames_ != 0 || droppedAudio_ != 0) {
        return core::unexpected(
            core::Error{"player.frame_stats.truncated",
                        "Diagnostic artifacts are incomplete because trace rows were dropped"}
                .withContext("dropped_frame_rows", std::to_string(droppedFrames_))
                .withContext("dropped_audio_rows", std::to_string(droppedAudio_)));
    }
    return {};
}

std::size_t FrameDiagnostics::capturedFrameRows() const noexcept {
    return frames_.size();
}

std::size_t FrameDiagnostics::capturedAudioRows() const noexcept {
    return audio_.size();
}

std::size_t FrameDiagnostics::droppedFrameRows() const noexcept {
    return droppedFrames_;
}

std::size_t FrameDiagnostics::droppedAudioRows() const noexcept {
    return droppedAudio_;
}

std::uint64_t FrameDiagnostics::frameHash(const playback::RuntimeFrame& frame,
                                          const playback::FrameSnapshot& snapshot) noexcept {
    std::uint64_t hash = fnvOffset;
    hashDouble(hash, frame.chartTimeMs);
    hashDouble(hash, frame.simulationDeltaTimeMs);
    hashInteger(hash, frame.timeDiscontinuityId);
    hashInteger(hash, static_cast<std::uint64_t>(snapshot.objects.size()));
    for (const auto& object : snapshot.objects) {
        hashString(hash, object.id);
        for (float value : object.worldMatrix) {
            hashFloat(hash, value);
        }
        hashByte(hash, object.hasTransform ? 1U : 0U);
        hashByte(hash, object.visible ? 1U : 0U);
    }
    hashByte(hash, snapshot.camera.active ? 1U : 0U);
    for (float value : snapshot.camera.viewMatrix) {
        hashFloat(hash, value);
    }
    for (float value : snapshot.camera.projectionMatrix) {
        hashFloat(hash, value);
    }
    hashDouble(hash, snapshot.camera.fovY);
    hashDouble(hash, snapshot.camera.nearPlane);
    hashDouble(hash, snapshot.camera.farPlane);
    hashDouble(hash, snapshot.camera.pitch);
    hashDouble(hash, snapshot.camera.yaw);
    hashDouble(hash, snapshot.camera.roll);
    hashFloat(hash, snapshot.clearRed);
    hashFloat(hash, snapshot.clearGreen);
    hashFloat(hash, snapshot.clearBlue);
    hashFloat(hash, snapshot.clearAlpha);
    hashInteger(hash, snapshot.viewportWidth);
    hashInteger(hash, snapshot.viewportHeight);
    return hash;
}

} // namespace cuexis::player

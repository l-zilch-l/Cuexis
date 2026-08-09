#include "frame_digest_internal.hpp"

#include <cuexis/core/error.hpp>

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace cuexis::playback::detail {
namespace {

constexpr std::uint32_t currentFrameDigestVersion = 3;
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

void hashReference(std::uint64_t& hash,
                   const std::optional<PresentationResourceRef>& reference) noexcept {
    hashByte(hash, reference ? 1U : 0U);
    if (!reference) {
        return;
    }
    hashInteger(hash, static_cast<std::uint32_t>(reference->type));
    hashString(hash, reference->assetId);
    for (const auto byte : reference->identity.sha256) {
        hashByte(hash, byte);
    }
}

[[nodiscard]] auto nonFiniteError() -> core::Error {
    return core::Error{"playback.frame_digest.non_finite",
                       "Frame digest input contains a non-finite numeric value"};
}

[[nodiscard]] auto validateDigestInput(std::uint32_t algorithmVersion, const RuntimeFrame& frame,
                                       const FrameSnapshot& snapshot) -> core::Result<void> {
    if (algorithmVersion < 1 || algorithmVersion > currentFrameDigestVersion) {
        return core::unexpected(
            core::Error{"playback.frame_digest.version_unsupported",
                        "Frame digest algorithm version is unsupported"}
                .withContext("algorithm_version", std::to_string(algorithmVersion)));
    }
    if (!std::isfinite(frame.chartTimeMs) || !std::isfinite(frame.simulationDeltaTimeMs) ||
        !std::isfinite(snapshot.camera.fovY) || !std::isfinite(snapshot.camera.nearPlane) ||
        !std::isfinite(snapshot.camera.farPlane) || !std::isfinite(snapshot.camera.pitch) ||
        !std::isfinite(snapshot.camera.yaw) || !std::isfinite(snapshot.camera.roll) ||
        !std::isfinite(snapshot.clearRed) || !std::isfinite(snapshot.clearGreen) ||
        !std::isfinite(snapshot.clearBlue) || !std::isfinite(snapshot.clearAlpha)) {
        return core::unexpected(nonFiniteError());
    }
    for (const auto& object : snapshot.objects) {
        if (algorithmVersion >= 2) {
            if (!std::isfinite(object.materialOpacity)) {
                return core::unexpected(nonFiniteError());
            }
            for (const float value : object.materialTint) {
                if (!std::isfinite(value)) {
                    return core::unexpected(nonFiniteError());
                }
            }
        }
        for (const float value : object.worldMatrix) {
            if (!std::isfinite(value)) {
                return core::unexpected(nonFiniteError());
            }
        }
    }
    for (const float value : snapshot.camera.viewMatrix) {
        if (!std::isfinite(value)) {
            return core::unexpected(nonFiniteError());
        }
    }
    for (const float value : snapshot.camera.projectionMatrix) {
        if (!std::isfinite(value)) {
            return core::unexpected(nonFiniteError());
        }
    }
    return {};
}

} // namespace

auto computeFrameDigestVersion(std::uint32_t algorithmVersion, const RuntimeFrame& frame,
                               const FrameSnapshot& snapshot) -> core::Result<FrameDigest> {
    if (auto validated = validateDigestInput(algorithmVersion, frame, snapshot); !validated) {
        return core::unexpected(std::move(validated.error()));
    }

    std::uint64_t hash = fnvOffset;
    hashInteger(hash, algorithmVersion);
    hashDouble(hash, frame.chartTimeMs);
    hashDouble(hash, frame.simulationDeltaTimeMs);
    hashInteger(hash, frame.timeDiscontinuityId);
    hashInteger(hash, snapshot.viewportWidth);
    hashInteger(hash, snapshot.viewportHeight);
    hashByte(hash, snapshot.camera.active ? 1U : 0U);
    hashDouble(hash, snapshot.camera.fovY);
    hashDouble(hash, snapshot.camera.nearPlane);
    hashDouble(hash, snapshot.camera.farPlane);
    hashDouble(hash, snapshot.camera.pitch);
    hashDouble(hash, snapshot.camera.yaw);
    hashDouble(hash, snapshot.camera.roll);
    for (const float value : snapshot.camera.viewMatrix) {
        hashFloat(hash, value);
    }
    for (const float value : snapshot.camera.projectionMatrix) {
        hashFloat(hash, value);
    }
    hashFloat(hash, snapshot.clearRed);
    hashFloat(hash, snapshot.clearGreen);
    hashFloat(hash, snapshot.clearBlue);
    hashFloat(hash, snapshot.clearAlpha);
    hashInteger(hash, static_cast<std::uint64_t>(snapshot.objects.size()));
    for (const auto& object : snapshot.objects) {
        hashString(hash, object.id);
        hashByte(hash, object.hasTransform ? 1U : 0U);
        hashByte(hash, object.visible ? 1U : 0U);
        if (algorithmVersion >= 2) {
            hashString(hash, object.materialAssetId);
            hashDouble(hash, object.materialOpacity);
            for (const float value : object.materialTint) {
                hashFloat(hash, value);
            }
        }
        for (const float value : object.worldMatrix) {
            hashFloat(hash, value);
        }
        if (algorithmVersion >= 3) {
            hashReference(hash, object.mesh);
            hashReference(hash, object.material);
        }
    }
    return FrameDigest{.algorithmVersion = algorithmVersion, .value = hash};
}

} // namespace cuexis::playback::detail

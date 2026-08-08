#pragma once

#include <cuexis/core/result.hpp>
#include <cuexis/playback/playback_export.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/playback/presentation.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace cuexis::playback::detail {

enum class NormalizedPresentationPass : std::uint8_t {
    Opaque,
    Transparent,
};

struct NormalizedPresentationRecord final {
    std::size_t objectIndex{};
    double effectiveRgb[3]{};
    double effectiveAlpha{};
    NormalizedPresentationPass pass{NormalizedPresentationPass::Opaque};
    bool backFaceCulling{true};
    bool depthTest{true};
    bool depthWrite{true};
    bool sourceOverBlend{};
    double depthMeters{};
    std::int64_t transparentDepthKey{};
};

struct NormalizedPresentationFrame final {
    std::vector<NormalizedPresentationRecord> opaque;
    std::vector<NormalizedPresentationRecord> transparent;
};

[[nodiscard]] CUEXIS_PLAYBACK_API auto
normalizePresentationFrame(const FrameSnapshot& snapshot,
                           const PresentationResourceManifest& manifest,
                           std::span<const PortableResourcePtr> resources,
                           NormalizedPresentationFrame& destination) -> core::Result<void>;

} // namespace cuexis::playback::detail

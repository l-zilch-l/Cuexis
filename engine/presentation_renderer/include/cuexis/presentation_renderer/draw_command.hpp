#pragma once

// Backend-neutral Portable Presentation draw commands.
// Sort, pass split, and summary digest follow the existing Portable v1 rules.
// This header has no SDL, OpenGL, or native handle types.

#include <cuexis/core/result.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/playback/presentation.hpp>
#include <cuexis/render/render_scene.hpp>

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace cuexis::presentation_renderer {

enum class PresentationPass : std::uint8_t {
    Opaque = 0,
    Transparent = 1,
};

struct DrawCommand final {
    std::string objectId;
    // Index of the source object inside the submitted FrameSnapshot. It is the tie-breaker for
    // both pass sorts, so a backend that retains per-object state can recover it without
    // re-deriving the ordering rules.
    std::size_t objectIndex{};
    std::array<float, 16> worldMatrix{};
    playback::PresentationResourceRef mesh;
    playback::PresentationResourceRef material;
    std::array<double, 4> effectiveColor{};
    PresentationPass pass{PresentationPass::Opaque};
    bool backFaceCulling{true};
    bool depthTest{true};
    bool depthWrite{true};
    bool sourceOverBlend{};
    double depthMeters{};
    std::int64_t transparentDepthKey{};
};

struct DrawSummary final {
    std::uint32_t version{1};
    std::uint32_t viewportWidth{};
    std::uint32_t viewportHeight{};
    std::array<float, 4> clearColor{};
    bool cameraActive{};
    std::array<float, 16> viewMatrix{};
    std::array<float, 16> projectionMatrix{};
    bool debugPassEnabled{};
    std::vector<DrawCommand> opaque;
    std::vector<DrawCommand> transparent;
    std::size_t debugCommandCount{};
};

// Portable summary v1 identity. It covers this summary and every command in pass order, and equals
// the Validation Sink digest of the same submission. It is a derived value rather than a stored
// field because the render path never reads it: computing it per frame costs one serial hash chain
// over every command, so only callers that record or compare summaries ask for it.
[[nodiscard]] auto summaryDigest(const DrawSummary& summary) noexcept -> std::uint64_t;

// Builds one submission from portable resources. manifestEmpty skips draw expansion but still
// validates the debug pass. The result carries no digest; call summaryDigest to derive it.
[[nodiscard]] auto
buildPresentationCommands(const playback::FrameSnapshot& snapshot,
                          std::span<const playback::PortableResourcePtr> resources,
                          bool manifestEmpty, bool debugPassEnabled,
                          const render::RenderScene* debugScene) -> core::Result<DrawSummary>;

} // namespace cuexis::presentation_renderer

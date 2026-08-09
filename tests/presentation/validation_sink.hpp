#pragma once

#include <cuexis/core/diagnostic.hpp>
#include <cuexis/core/result.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/playback/presentation.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace cuexis::test_support {

inline constexpr std::uint32_t validationSummaryVersion = 1;

enum class ValidationPass : std::uint8_t {
    Opaque,
    Transparent,
};

struct ValidationCommand final {
    std::size_t objectIndex{};
    std::string objectId;
    std::array<float, 16> worldMatrix{};
    playback::PresentationResourceRef mesh;
    playback::PresentationResourceRef material;
    std::array<double, 4> effectiveColor{};
    ValidationPass pass{ValidationPass::Opaque};
    bool backFaceCulling{true};
    bool depthTest{true};
    bool depthWrite{true};
    bool sourceOverBlend{};
    double depthMeters{};
    std::int64_t transparentDepthKey{};
};

struct ValidationSummary final {
    std::uint32_t version{validationSummaryVersion};
    std::uint32_t viewportWidth{};
    std::uint32_t viewportHeight{};
    std::array<float, 4> clearColor{};
    bool cameraActive{};
    std::array<float, 16> viewMatrix{};
    std::array<float, 16> projectionMatrix{};
    bool debugPassEnabled{};
    std::vector<ValidationCommand> opaque;
    std::vector<ValidationCommand> transparent;
    std::uint64_t digest{};

    void clear() noexcept;
};

struct ValidationCandidateResult;

class ValidationCandidate final {
  public:
    ValidationCandidate(const ValidationCandidate&) = delete;
    auto operator=(const ValidationCandidate&) -> ValidationCandidate& = delete;
    ValidationCandidate(ValidationCandidate&&) noexcept = default;
    auto operator=(ValidationCandidate&&) noexcept -> ValidationCandidate& = default;

    [[nodiscard]] auto token() const noexcept -> const playback::PresentationCandidateToken&;
    [[nodiscard]] auto manifest() const noexcept -> const playback::PresentationResourceManifest&;
    [[nodiscard]] auto resources() const noexcept -> std::span<const playback::PortableResourcePtr>;
    [[nodiscard]] auto settings() const noexcept -> const playback::EffectivePresentationSettings&;

  private:
    friend struct ValidationCandidateResult;
    friend auto prepareValidationCandidate(playback::PreparedPlayback& prepared,
                                           const playback::PresentationCapabilities& capabilities,
                                           const playback::PresentationRequest& request)
        -> ValidationCandidateResult;

    ValidationCandidate(playback::PresentationCandidateToken token,
                        playback::PresentationResourceManifest manifest,
                        std::vector<playback::PortableResourcePtr> resources,
                        playback::EffectivePresentationSettings settings) noexcept;

    playback::PresentationCandidateToken token_;
    playback::PresentationResourceManifest manifest_;
    std::vector<playback::PortableResourcePtr> resources_;
    playback::EffectivePresentationSettings settings_;
};

struct ValidationCandidateResult final {
    std::optional<ValidationCandidate> candidate;
    core::Diagnostics diagnostics;

    [[nodiscard]] bool hasValue() const noexcept {
        return candidate.has_value() && !diagnostics.hasErrors();
    }
};

[[nodiscard]] auto
validatePresentationData(const playback::PresentationResourceManifest& manifest,
                         std::span<const playback::PortableResourcePtr> resources)
    -> core::Result<void>;

[[nodiscard]] auto
computePresentationIdentity(const playback::PortableResourceValue& value) noexcept
    -> playback::PresentationContentIdentity;

[[nodiscard]] auto prepareValidationCandidate(
    playback::PreparedPlayback& prepared, const playback::PresentationCapabilities& capabilities,
    const playback::PresentationRequest& request) -> ValidationCandidateResult;

class ValidationSink final {
  public:
    void activate(ValidationCandidate&& candidate) noexcept;

    [[nodiscard]] bool active() const noexcept;
    [[nodiscard]] auto activeToken() const noexcept -> const playback::PresentationCandidateToken*;
    [[nodiscard]] auto validateFrame(const playback::FrameSnapshot& snapshot,
                                     ValidationSummary& destination) const -> core::Result<void>;

  private:
    std::optional<ValidationCandidate> active_;
};

} // namespace cuexis::test_support

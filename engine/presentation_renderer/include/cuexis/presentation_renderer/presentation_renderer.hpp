#pragma once

// Internal presentation renderer contract. Not an installed SDK component.
// The renderer owns transaction tokens and neutral submission. It does not own the window.
// A candidate must be destroyed before its renderer. Rebuild invalidates outstanding tokens.

#include <cuexis/playback/playback_session.hpp>
#include <cuexis/playback/presentation.hpp>
#include <cuexis/presentation_renderer/draw_command.hpp>
#include <cuexis/render/render_scene.hpp>

#include <cstdint>
#include <memory>

namespace cuexis::presentation_renderer {

enum class PresentationFailureClass : std::uint8_t {
    Recoverable,
    Unrecoverable,
};

struct RendererIdentity final {
    std::uint64_t instance{};
    std::uint64_t generation{};

    friend bool operator==(const RendererIdentity&, const RendererIdentity&) = default;
};

struct PresentationRendererStatus final {
    bool closed{};
    bool deviceLost{};
    bool active{};
    bool candidateOutstanding{};
    bool frameSubmitted{};
    std::uint32_t surfaceWidth{};
    std::uint32_t surfaceHeight{};
    std::uint64_t generation{};
    std::size_t activeResourceCount{};
};

class IPresentationRenderer;

class PreparedPresentation final {
  public:
    PreparedPresentation() noexcept;
    ~PreparedPresentation();

    PreparedPresentation(const PreparedPresentation&) = delete;
    auto operator=(const PreparedPresentation&) -> PreparedPresentation& = delete;
    PreparedPresentation(PreparedPresentation&& other) noexcept;
    auto operator=(PreparedPresentation&& other) noexcept -> PreparedPresentation&;

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] auto playbackToken() const noexcept
        -> const playback::PresentationCandidateToken*;
    [[nodiscard]] auto rendererIdentity() const noexcept -> RendererIdentity;
    [[nodiscard]] auto settings() const noexcept -> const playback::EffectivePresentationSettings*;

  private:
    friend class IPresentationRenderer;
    friend class TestPresentationRenderer;
    friend auto sealPreparedPresentation(IPresentationRenderer& renderer, std::uint64_t instance,
                                         std::uint64_t rendererGeneration,
                                         std::uint64_t candidateGeneration,
                                         playback::PresentationCandidateToken token,
                                         playback::EffectivePresentationSettings settings)
        -> PreparedPresentation;

    struct State;
    explicit PreparedPresentation(std::unique_ptr<State> state) noexcept;

    std::unique_ptr<State> state_;
};

class IPresentationRenderer {
  public:
    virtual ~IPresentationRenderer() = default;

    IPresentationRenderer(const IPresentationRenderer&) = delete;
    auto operator=(const IPresentationRenderer&) -> IPresentationRenderer& = delete;
    IPresentationRenderer(IPresentationRenderer&&) = delete;
    auto operator=(IPresentationRenderer&&) -> IPresentationRenderer& = delete;

    [[nodiscard]] virtual auto capabilities() const noexcept
        -> const playback::PresentationCapabilities& = 0;
    [[nodiscard]] virtual auto identity() const noexcept -> RendererIdentity = 0;
    [[nodiscard]] virtual auto prepare(playback::PreparedPlayback& prepared,
                                       const playback::PresentationRequest& request = {})
        -> core::Result<PreparedPresentation> = 0;
    // Returns an error for a stale, foreign, or spent candidate. Activate is noexcept only
    // after this check has succeeded.
    [[nodiscard]] virtual auto accepts(const PreparedPresentation& candidate) const
        -> core::Result<void> = 0;
    virtual void activate(PreparedPresentation&& candidate) noexcept = 0;
    virtual void discard(PreparedPresentation&& candidate) noexcept = 0;
    [[nodiscard]] virtual bool hasActivePresentation() const noexcept = 0;
    [[nodiscard]] virtual auto submit(const playback::FrameSnapshot& snapshot,
                                      const render::RenderScene* debugScene = nullptr)
        -> core::Result<DrawSummary> = 0;
    [[nodiscard]] virtual auto present() -> core::Result<void> = 0;
    // Changes the surface size only. Zero size suspends later submission. Playback is untouched.
    [[nodiscard]] virtual auto resize(std::uint32_t width, std::uint32_t height)
        -> core::Result<void> = 0;
    // Drops GPU-equivalent retention, invalidates renderer tokens, and clears device loss.
    [[nodiscard]] virtual auto rebuild() -> core::Result<void> = 0;
    // Releases renderer resources. Does not close an application-owned window.
    [[nodiscard]] virtual auto close() -> core::Result<void> = 0;

  protected:
    IPresentationRenderer() = default;

  private:
    friend class PreparedPresentation;
    virtual void detachCandidate(std::uint64_t rendererGeneration,
                                 std::uint64_t candidateGeneration,
                                 const playback::PresentationCandidateToken& token) noexcept = 0;
};

class TestPresentationRenderer final : public IPresentationRenderer {
  public:
    TestPresentationRenderer();
    ~TestPresentationRenderer() override;

    [[nodiscard]] auto capabilities() const noexcept
        -> const playback::PresentationCapabilities& override;
    [[nodiscard]] auto identity() const noexcept -> RendererIdentity override;
    [[nodiscard]] auto prepare(playback::PreparedPlayback& prepared,
                               const playback::PresentationRequest& request = {})
        -> core::Result<PreparedPresentation> override;
    [[nodiscard]] auto accepts(const PreparedPresentation& candidate) const
        -> core::Result<void> override;
    void activate(PreparedPresentation&& candidate) noexcept override;
    void discard(PreparedPresentation&& candidate) noexcept override;
    [[nodiscard]] bool hasActivePresentation() const noexcept override;
    [[nodiscard]] auto submit(const playback::FrameSnapshot& snapshot,
                              const render::RenderScene* debugScene = nullptr)
        -> core::Result<DrawSummary> override;
    [[nodiscard]] auto present() -> core::Result<void> override;
    [[nodiscard]] auto resize(std::uint32_t width, std::uint32_t height)
        -> core::Result<void> override;
    [[nodiscard]] auto rebuild() -> core::Result<void> override;
    [[nodiscard]] auto close() -> core::Result<void> override;

    [[nodiscard]] auto status() const noexcept -> PresentationRendererStatus;
    void markDeviceLost() noexcept;
    void failNextPresent(PresentationFailureClass failureClass) noexcept;

  private:
    struct Cache;
    struct State;

    void detachCandidate(std::uint64_t rendererGeneration, std::uint64_t candidateGeneration,
                         const playback::PresentationCandidateToken& token) noexcept override;
    [[nodiscard]] auto requireOwner(const char* operation) const -> core::Result<void>;

    std::unique_ptr<State> state_;
};

[[nodiscard]] auto portableRendererCapabilities() noexcept -> playback::PresentationCapabilities;

// Binds a candidate to an existing renderer. The renderer must outlive the candidate.
[[nodiscard]] auto sealPreparedPresentation(IPresentationRenderer& renderer, std::uint64_t instance,
                                            std::uint64_t rendererGeneration,
                                            std::uint64_t candidateGeneration,
                                            playback::PresentationCandidateToken token,
                                            playback::EffectivePresentationSettings settings)
    -> PreparedPresentation;

} // namespace cuexis::presentation_renderer

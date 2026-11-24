#pragma once

// Experimental frame layout values, geometry and the committed laid-out frame for the R5 frame
// bridge. This header is not part of the default installed SDK surface. Targets that actually
// consume the frame bridge opt in through the playback experimental include root.

#include <cuexis/core/result.hpp>
#include <cuexis/playback/playback_export.hpp>
#include <cuexis/playback/playback_session.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace cuexis::playback {
class PlaybackSession;
class PreparedPlayback;
namespace detail {
struct FrameLayoutAccess;
} // namespace detail
} // namespace cuexis::playback

namespace cuexis::playback::experimental {

struct AspectRatio final {
    std::uint32_t width{};
    std::uint32_t height{};
};

struct FrameFraming final {
    AspectRatio aspect;
    double verticalScale{1.0};
};

enum class FrameRotation : std::uint16_t {
    Deg0 = 0,
    Deg90 = 90,
    Deg180 = 180,
    Deg270 = 270,
};

enum class FramingSelection : std::uint8_t {
    Default,
    BestFit,
    Exact,
};

struct FramingRequest final {
    FramingSelection selection{FramingSelection::Default};
    std::optional<AspectRatio> exact;
};

// Absolute target pixel rectangle. The origin is the top-left corner and the range is half-open.
struct PixelRect final {
    std::uint32_t x{};
    std::uint32_t y{};
    std::uint32_t width{};
    std::uint32_t height{};
};

struct FrameTarget final {
    FrameViewport extent;
    PixelRect host;
    FrameRotation rotation{FrameRotation::Deg0};
};

// The content rectangle is relative to host; target is copied from the request.
struct FrameLayout final {
    FrameTarget target;
    PixelRect content;
};

struct SelectedFrameLayout final {
    FrameLayout layout;
    std::optional<FrameFraming> framing;
};

// One committed frame extraction: the snapshot, the layout, the committed source values and the
// resource identity all come from the same owner read. The frame owns the reduced framing table
// and the framing request so that retained relayout never re-reads Runtime or Playback state.
// Move-only; the constructor is private and not exported.
class LaidOutFrame final {
  public:
    CUEXIS_PLAYBACK_API ~LaidOutFrame();
    LaidOutFrame(const LaidOutFrame&) = delete;
    auto operator=(const LaidOutFrame&) -> LaidOutFrame& = delete;
    CUEXIS_PLAYBACK_API LaidOutFrame(LaidOutFrame&&) noexcept;
    CUEXIS_PLAYBACK_API auto operator=(LaidOutFrame&&) noexcept -> LaidOutFrame&;

    [[nodiscard]] CUEXIS_PLAYBACK_API auto snapshot() const noexcept -> const FrameSnapshot&;
    [[nodiscard]] CUEXIS_PLAYBACK_API auto layout() const noexcept -> const FrameLayout&;
    [[nodiscard]] CUEXIS_PLAYBACK_API auto source() const noexcept -> const PresentationSourceInfo&;
    [[nodiscard]] CUEXIS_PLAYBACK_API auto resourceToken() const noexcept
        -> const PresentationCandidateToken&;
    // The selected unrotated author framing; empty for the legacy no-framing path.
    [[nodiscard]] CUEXIS_PLAYBACK_API auto framing() const noexcept -> std::optional<FrameFraming>;

  private:
    friend struct detail::FrameLayoutAccess;

    LaidOutFrame() noexcept = default;

    FrameSnapshot snapshot_;
    FrameLayout layout_;
    PresentationSourceInfo source_;
    PresentationCandidateToken token_;
    std::optional<FrameFraming> framing_;
    std::array<FrameFraming, 16> framings_{};
    std::uint8_t framingCount_{};
    FramingRequest request_;
    // Unscaled camera samples used to rebuild the base projection on retained relayout.
    double baseFovY_{};
    double nearPlane_{};
    double farPlane_{};
};

// Both geometry functions export from every Playback flavor. The rest of the bridge is exported
// by the same implementation and listed individually in the shared export gate.
[[nodiscard]] CUEXIS_PLAYBACK_API auto computeContentRect(std::optional<AspectRatio> aspect,
                                                          FrameTarget target)
    -> core::Result<std::optional<FrameLayout>>;

[[nodiscard]] CUEXIS_PLAYBACK_API auto computeFrameLayout(std::span<const FrameFraming> framings,
                                                          FrameTarget target,
                                                          FramingRequest request = {})
    -> core::Result<std::optional<SelectedFrameLayout>>;

// Owning copy of the prepared framing table; uses the Prepared owner and validity checks.
[[nodiscard]] CUEXIS_PLAYBACK_API auto preparedFramings(const PreparedPlayback&)
    -> core::Result<std::vector<FrameFraming>>;

// Same owner re-entry and committed descriptor read as extractFrame: the committed version check,
// framing selection, projection, snapshot, source and token come from one read.
[[nodiscard]] CUEXIS_PLAYBACK_API auto
extractLaidOutFrame(const PlaybackSession&, FrameTarget,
                    std::optional<std::uint64_t> exactResultVersion = {}, FramingRequest = {})
    -> core::Result<std::optional<LaidOutFrame>>;

// Re-selects and re-projects from the retained frame's own values, resources and token. It never
// reads Runtime, Playback or the device and grants no new presentation lease.
[[nodiscard]] CUEXIS_PLAYBACK_API auto relayoutRetainedFrame(const LaidOutFrame&, FrameTarget)
    -> core::Result<std::optional<LaidOutFrame>>;

} // namespace cuexis::playback::experimental

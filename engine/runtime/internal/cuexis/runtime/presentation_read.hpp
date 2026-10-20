#pragma once

// Owner-bound neutral fields. Neither registry/entity values nor device objects escape.
#include <cuexis/core/math.hpp>
#include <cuexis/core/result.hpp>
#include <cuexis/runtime/runtime_frame.hpp>
#include <cuexis_internal/presentation_revision.hpp>
#include <memory>
#include <optional>
#include <string_view>
#include <type_traits>

namespace cuexis::runtime {
class RuntimeSession;
namespace detail {
struct PresentationWindow;
struct PresentationStamp final {
    std::uint64_t owner{}, layout{}, version{};
    auto operator==(const PresentationStamp&) const -> bool = default;
};
struct PresentationState final {
    PresentationStamp stamp;
    std::uint64_t updateOrdinal{}, mutationEpoch{};
    internal::presentation::Source source{internal::presentation::Source::Prepared};
    std::optional<RuntimeFrame> frame;
};
struct ObjectPresentation final {
    std::string_view id;
    bool hasTransform{}, hasAppearance{}, hasRenderable{};
    core::Mat4 matrix;
    bool visible{true};
    std::string_view material;
    double opacity{1};
    core::Vec3 tint{1, 1, 1};
};
struct CameraPresentation final {
    bool selected{}, present{}, hasTransform{};
    core::Mat4 matrix;
    double fovY{60}, nearPlane{0.1}, farPlane{1000}, pitch{}, yaw{}, roll{};
};
class PresentationRead final {
  public:
    auto readObject(std::size_t index) const -> core::Result<ObjectPresentation>;
    auto readCamera() const -> core::Result<CameraPresentation>;
    auto state() const -> core::Result<PresentationState>;

  private:
    friend struct PresentationAccess;
    // The window is validated once, when the read gate hands the view to its callback. Promoting
    // it here costs one strong-count operation per read window instead of one per object, camera
    // and state read. A client copy keeps the window alive, yet the window still ends with the
    // callback, so an escaped view cannot observe a later frame.
    PresentationRead(std::shared_ptr<PresentationWindow> window, std::uint64_t serial) noexcept
        : window_(std::move(window)), serial_(serial) {}
    // Scalar owner, window and generation checks are the whole per-read cost.
    [[nodiscard]] auto available() const noexcept -> bool;
    [[nodiscard]] auto availableError() const -> core::Error;
    std::shared_ptr<PresentationWindow> window_;
    std::uint64_t serial_;
};
struct PresentationAccess final {
    static auto state(const RuntimeSession&) -> core::Result<PresentationState>;
    // Scalar-only installation tail, called by the serialized owner after a successful write.
    // Read clients use state/read, which perform all owner and window checks.
    static auto installedState(const RuntimeSession&) noexcept -> PresentationState;
    template <typename Callback>
    static auto read(const RuntimeSession& session, PresentationStamp expected, Callback&& callback)
        -> core::Result<void> {
        using Function = std::remove_reference_t<Callback>;
        return read(session, expected,
                    const_cast<void*>(static_cast<const void*>(std::addressof(callback))),
                    [](void* context, const PresentationRead& view) -> core::Result<void> {
                        return (*static_cast<Function*>(context))(view);
                    });
    }

  private:
    static auto read(const RuntimeSession&, PresentationStamp, void*,
                     core::Result<void> (*)(void*, const PresentationRead&)) -> core::Result<void>;
};
} // namespace detail
} // namespace cuexis::runtime

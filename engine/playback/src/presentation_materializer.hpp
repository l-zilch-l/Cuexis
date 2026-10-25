#pragma once

#include <cuexis/chart/chart_runtime.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/runtime/presentation_read.hpp>
#include <map>

namespace cuexis::playback::detail {
struct SnapshotEntity final {
    chart::ChartObjectId id;
    std::size_t runtimeIndex{};
    std::optional<PresentationResourceRef> mesh;
    std::map<std::string, PresentationResourceRef, std::less<>> materials;
    std::size_t maximumMeshAssetIdSize{}, maximumMaterialAssetIdSize{};
};
struct SnapshotLayout final {
    std::vector<SnapshotEntity> entities;
    std::uint64_t runtimeOwner{}, runtimeLayout{};
};
// Conservative logical storage: callee row, caller Result and actual fallback matrix.
// Excludes compiler frames, other temporaries, gather closure state and camera/error work.
inline constexpr std::size_t snapshotReadScratchBytes =
    sizeof(runtime::detail::ObjectVisualRead) +
    sizeof(core::Result<runtime::detail::ObjectVisualRead>) + sizeof(core::Mat4);
static_assert(snapshotReadScratchBytes <= 512);
#if defined(_M_X64) && _MSC_VER == 1951 && _MSVC_STL_VERSION == 145 &&                             \
    (_ITERATOR_DEBUG_LEVEL == 0 || _ITERATOR_DEBUG_LEVEL == 2)
// Registered MSVC/STL x64 Debug and Release layouts; no promise for another ABI.
static_assert(sizeof(runtime::detail::ObjectPresentation) == 128 &&
              alignof(runtime::detail::ObjectPresentation) == 8);
static_assert(sizeof(core::Result<runtime::detail::ObjectPresentation>) == 144 &&
              alignof(core::Result<runtime::detail::ObjectPresentation>) == 8);
static_assert(sizeof(runtime::detail::ObjectVisualRead) == 72 &&
              alignof(runtime::detail::ObjectVisualRead) == 8);
static_assert(sizeof(core::Mat4) == 64 && alignof(core::Mat4) == 4);
#if _ITERATOR_DEBUG_LEVEL == 2
static_assert(sizeof(core::Result<runtime::detail::ObjectVisualRead>) == 144 &&
              alignof(core::Result<runtime::detail::ObjectVisualRead>) == 8);
static_assert(snapshotReadScratchBytes == 280);
#else
static_assert(sizeof(core::Result<runtime::detail::ObjectVisualRead>) == 120 &&
              alignof(core::Result<runtime::detail::ObjectVisualRead>) == 8);
static_assert(snapshotReadScratchBytes == 256);
#endif

#endif

auto snapshotResourceMismatch(std::string_view objectId, std::string_view assetId,
                              PresentationResourceType type) -> core::Error;
auto presentationResourceTypeName(PresentationResourceType type) noexcept -> std::string_view;
auto materializeSnapshot(const runtime::detail::PresentationRead&, const SnapshotLayout&,
                         const FrameViewport&, FrameSnapshot&) -> core::Result<void>;
} // namespace cuexis::playback::detail

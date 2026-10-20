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
auto snapshotResourceMismatch(std::string_view objectId, std::string_view assetId,
                              PresentationResourceType type) -> core::Error;
auto presentationResourceTypeName(PresentationResourceType type) noexcept -> std::string_view;
auto materializeSnapshot(const runtime::detail::PresentationRead&, const SnapshotLayout&,
                         const FrameViewport&, FrameSnapshot&) -> core::Result<void>;
} // namespace cuexis::playback::detail

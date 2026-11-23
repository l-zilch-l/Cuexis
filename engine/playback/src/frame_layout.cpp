#include <cuexis/playback/experimental/frame_layout.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cuexis::playback::experimental {
namespace {

constexpr std::uint32_t maxAspectComponent = 2147483647U;
constexpr std::size_t maxFramingCount = 16;

[[nodiscard]] auto invalidAspect(std::optional<std::size_t> index, AspectRatio aspect)
    -> core::Error {
    core::Error error{"playback.presentation.layout.invalid_aspect",
                      "Frame aspect must use finite positive components within the accepted range"};
    error.withContext("width", std::to_string(aspect.width))
        .withContext("height", std::to_string(aspect.height));
    if (index) {
        error.withContext("index", std::to_string(*index));
    }
    return error;
}

[[nodiscard]] auto invalidTarget(FrameTarget target, std::string_view reason) -> core::Error {
    core::Error error{"playback.presentation.layout.invalid_target",
                      "Frame target is outside its accepted domain"};
    error.withContext("reason", std::string{reason})
        .withContext("extent_width", std::to_string(target.extent.width))
        .withContext("extent_height", std::to_string(target.extent.height))
        .withContext("host_x", std::to_string(target.host.x))
        .withContext("host_y", std::to_string(target.host.y))
        .withContext("host_width", std::to_string(target.host.width))
        .withContext("host_height", std::to_string(target.host.height))
        .withContext("rotation", std::to_string(static_cast<std::uint16_t>(target.rotation)));
    return error;
}

[[nodiscard]] auto invalidFraming(std::size_t index, std::string_view reason) -> core::Error {
    return core::Error{"playback.presentation.layout.invalid_framing",
                       "A prepared framing entry is outside its accepted domain"}
        .withContext("index", std::to_string(index))
        .withContext("reason", std::string{reason});
}

[[nodiscard]] auto invalidSelection(FramingSelection selection) -> core::Error {
    return core::Error{"playback.presentation.layout.invalid_selection",
                       "Framing selection request is inconsistent"}
        .withContext("selection", std::to_string(static_cast<unsigned>(selection)));
}

[[nodiscard]] auto aspectUnavailable(AspectRatio aspect) -> core::Error {
    return core::Error{"playback.presentation.layout.aspect_unavailable",
                       "The requested exact aspect is not declared by the prepared framings"}
        .withContext("width", std::to_string(aspect.width))
        .withContext("height", std::to_string(aspect.height));
}

[[nodiscard]] constexpr auto isAspectInDomain(AspectRatio aspect) noexcept -> bool {
    return aspect.width >= 1U && aspect.height >= 1U && aspect.width <= maxAspectComponent &&
           aspect.height <= maxAspectComponent;
}

[[nodiscard]] auto reducedAspect(AspectRatio aspect) noexcept -> AspectRatio {
    const auto divisor = std::gcd(aspect.width, aspect.height);
    return AspectRatio{aspect.width / divisor, aspect.height / divisor};
}

[[nodiscard]] auto sameAspect(AspectRatio left, AspectRatio right) noexcept -> bool {
    const auto reducedLeft = reducedAspect(left);
    const auto reducedRight = reducedAspect(right);
    return reducedLeft.width == reducedRight.width && reducedLeft.height == reducedRight.height;
}

[[nodiscard]] auto isRotationValid(FrameRotation rotation) noexcept -> bool {
    switch (rotation) {
    case FrameRotation::Deg0:
    case FrameRotation::Deg90:
    case FrameRotation::Deg180:
    case FrameRotation::Deg270:
        return true;
    }
    return false;
}

[[nodiscard]] auto validateTarget(const FrameTarget& target) -> core::Result<void> {
    if (!isRotationValid(target.rotation)) {
        return core::unexpected(invalidTarget(target, "rotation"));
    }
    const auto right = static_cast<std::uint64_t>(target.host.x) + target.host.width;
    const auto bottom = static_cast<std::uint64_t>(target.host.y) + target.host.height;
    if (right > target.extent.width || bottom > target.extent.height) {
        return core::unexpected(invalidTarget(target, "host_out_of_bounds"));
    }
    return {};
}

[[nodiscard]] auto validateFramings(std::span<const FrameFraming> framings) -> core::Result<void> {
    if (framings.size() > maxFramingCount) {
        return core::unexpected(core::Error{"playback.presentation.layout.invalid_framing",
                                            "Prepared framing table exceeds its capacity"}
                                    .withContext("count", std::to_string(framings.size())));
    }
    std::vector<AspectRatio> seen;
    seen.reserve(framings.size());
    for (std::size_t index = 0; index < framings.size(); ++index) {
        const auto& framing = framings[index];
        if (!isAspectInDomain(framing.aspect)) {
            return core::unexpected(invalidAspect(index, framing.aspect));
        }
        if (!std::isfinite(framing.verticalScale) || framing.verticalScale < 0.125 ||
            framing.verticalScale > 8.0) {
            return core::unexpected(invalidFraming(index, "vertical_scale"));
        }
        const auto reduced = reducedAspect(framing.aspect);
        const auto duplicate = std::find_if(seen.begin(), seen.end(), [&](AspectRatio candidate) {
            return candidate.width == reduced.width && candidate.height == reduced.height;
        });
        if (duplicate != seen.end()) {
            auto error = invalidFraming(index, "duplicate_aspect");
            error.withContext("duplicate_index",
                              std::to_string(static_cast<std::size_t>(duplicate - seen.begin())));
            return core::unexpected(std::move(error));
        }
        seen.push_back(reduced);
    }
    return {};
}

[[nodiscard]] auto makeContentRect(std::optional<AspectRatio> aspect, const FrameTarget& target)
    -> core::Result<std::optional<FrameLayout>> {
    const auto width = target.host.width;
    const auto height = target.host.height;
    if (target.extent.width == 0 || target.extent.height == 0 || width == 0 || height == 0) {
        return std::optional<FrameLayout>{};
    }
    PixelRect content;
    if (!aspect) {
        content = PixelRect{0, 0, width, height};
    } else {
        const auto rotated =
            target.rotation == FrameRotation::Deg90 || target.rotation == FrameRotation::Deg270;
        const auto numerator = rotated ? aspect->height : aspect->width;
        const auto denominator = rotated ? aspect->width : aspect->height;
        std::uint64_t contentWidth = 0;
        std::uint64_t contentHeight = 0;
        if (static_cast<std::uint64_t>(width) * denominator <=
            static_cast<std::uint64_t>(height) * numerator) {
            contentWidth = width;
            contentHeight = static_cast<std::uint64_t>(width) * denominator / numerator;
        } else {
            contentWidth = static_cast<std::uint64_t>(height) * numerator / denominator;
            contentHeight = height;
        }
        if (contentWidth == 0 || contentHeight == 0) {
            return std::optional<FrameLayout>{};
        }
        content = PixelRect{static_cast<std::uint32_t>((width - contentWidth) / 2U),
                            static_cast<std::uint32_t>((height - contentHeight) / 2U),
                            static_cast<std::uint32_t>(contentWidth),
                            static_cast<std::uint32_t>(contentHeight)};
    }
    return std::optional<FrameLayout>{FrameLayout{target, content}};
}

} // namespace

auto computeContentRect(std::optional<AspectRatio> aspect, FrameTarget target)
    -> core::Result<std::optional<FrameLayout>> {
    if (aspect && !isAspectInDomain(*aspect)) {
        return core::unexpected(invalidAspect(std::nullopt, *aspect));
    }
    if (auto valid = validateTarget(target); !valid) {
        return core::unexpected(std::move(valid.error()));
    }
    return makeContentRect(aspect, target);
}

auto computeFrameLayout(std::span<const FrameFraming> framings, FrameTarget target,
                        FramingRequest request)
    -> core::Result<std::optional<SelectedFrameLayout>> {
    if (auto valid = validateTarget(target); !valid) {
        return core::unexpected(std::move(valid.error()));
    }
    switch (request.selection) {
    case FramingSelection::Default:
    case FramingSelection::BestFit:
        if (request.exact) {
            return core::unexpected(invalidSelection(request.selection));
        }
        break;
    case FramingSelection::Exact:
        if (!request.exact) {
            return core::unexpected(invalidSelection(request.selection));
        }
        if (!isAspectInDomain(*request.exact)) {
            return core::unexpected(invalidAspect(std::nullopt, *request.exact));
        }
        break;
    default:
        return core::unexpected(invalidSelection(request.selection));
    }
    if (auto valid = validateFramings(framings); !valid) {
        return core::unexpected(std::move(valid.error()));
    }

    if (framings.empty()) {
        if (request.selection == FramingSelection::Exact) {
            return core::unexpected(aspectUnavailable(*request.exact));
        }
        auto content = makeContentRect(std::nullopt, target);
        if (!content) {
            return core::unexpected(std::move(content.error()));
        }
        if (!content->has_value()) {
            return std::optional<SelectedFrameLayout>{};
        }
        return std::optional<SelectedFrameLayout>{
            SelectedFrameLayout{std::move(content->value()), std::nullopt}};
    }

    std::optional<FrameFraming> selected;
    if (request.selection == FramingSelection::Default) {
        selected = framings.front();
    } else if (request.selection == FramingSelection::BestFit) {
        std::uint64_t bestArea = 0;
        for (const auto& candidate : framings) {
            auto content = makeContentRect(candidate.aspect, target);
            if (!content) {
                return core::unexpected(std::move(content.error()));
            }
            if (!content->has_value()) {
                continue;
            }
            const auto area = static_cast<std::uint64_t>(content->value().content.width) *
                              content->value().content.height;
            if (area > bestArea) {
                bestArea = area;
                selected = candidate;
            }
        }
        if (!selected) {
            return std::optional<SelectedFrameLayout>{};
        }
    } else {
        const auto found =
            std::find_if(framings.begin(), framings.end(), [&](const FrameFraming& candidate) {
                return sameAspect(candidate.aspect, *request.exact);
            });
        if (found == framings.end()) {
            return core::unexpected(aspectUnavailable(*request.exact));
        }
        selected = *found;
    }

    auto content = makeContentRect(selected->aspect, target);
    if (!content) {
        return core::unexpected(std::move(content.error()));
    }
    if (!content->has_value()) {
        return std::optional<SelectedFrameLayout>{};
    }
    return std::optional<SelectedFrameLayout>{
        SelectedFrameLayout{std::move(content->value()), std::move(selected)}};
}

} // namespace cuexis::playback::experimental

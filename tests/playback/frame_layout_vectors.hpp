#pragma once

// Shared value checks for the experimental frame layout geometry and projection helper. Both the
// Playback and OpenGL test executables include this header so one vector definition feeds both.

#include "../../engine/playback/src/frame_layout_internal.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace cuexis::test::frame_layout {

using cuexis::core::Mat4;
using cuexis::playback::FrameViewport;
using cuexis::playback::experimental::AspectRatio;
using cuexis::playback::experimental::FrameFraming;
using cuexis::playback::experimental::FrameLayout;
using cuexis::playback::experimental::FrameRotation;
using cuexis::playback::experimental::FrameTarget;
using cuexis::playback::experimental::FramingRequest;
using cuexis::playback::experimental::FramingSelection;
using cuexis::playback::experimental::PixelRect;
using cuexis::playback::experimental::SelectedFrameLayout;

inline auto makeTarget(std::uint32_t extentWidth, std::uint32_t extentHeight, PixelRect host,
                       FrameRotation rotation = FrameRotation::Deg0) -> FrameTarget {
    return FrameTarget{FrameViewport{extentWidth, extentHeight}, host, rotation};
}

inline auto pixelRect(std::uint32_t x, std::uint32_t y, std::uint32_t width, std::uint32_t height)
    -> PixelRect {
    return PixelRect{x, y, width, height};
}

inline auto hasContext(const cuexis::core::Error& error, std::string_view key,
                       std::string_view value) -> bool {
    for (const auto& context : error.context()) {
        if (context.key == key && context.value == value) {
            return true;
        }
    }
    return false;
}

template <typename T>
inline void requireError(const cuexis::core::Result<T>& result, std::string_view code) {
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == code);
}

inline auto requireLayout(const cuexis::core::Result<std::optional<FrameLayout>>& result)
    -> FrameLayout {
    REQUIRE(result.has_value());
    REQUIRE(result->has_value());
    return **result;
}

inline auto requireSelected(const cuexis::core::Result<std::optional<SelectedFrameLayout>>& result)
    -> SelectedFrameLayout {
    REQUIRE(result.has_value());
    REQUIRE(result->has_value());
    return **result;
}

inline void checkRect(PixelRect actual, PixelRect expected) {
    CHECK(actual.x == expected.x);
    CHECK(actual.y == expected.y);
    CHECK(actual.width == expected.width);
    CHECK(actual.height == expected.height);
}

inline auto perspective(double fovDegrees, double aspect) -> Mat4 {
    auto result = cuexis::core::makePerspective(fovDegrees * 3.14159265358979323846 / 180.0, aspect,
                                                0.1, 100.0);
    REQUIRE(result.has_value());
    return *result;
}

inline auto rotationMatrix(FrameRotation rotation) -> Mat4 {
    Mat4 matrix;
    switch (rotation) {
    case FrameRotation::Deg0:
        break;
    case FrameRotation::Deg90:
        matrix.element(0, 0) = 0.0F;
        matrix.element(0, 1) = 1.0F;
        matrix.element(1, 0) = -1.0F;
        matrix.element(1, 1) = 0.0F;
        break;
    case FrameRotation::Deg180:
        matrix.element(0, 0) = -1.0F;
        matrix.element(1, 1) = -1.0F;
        break;
    case FrameRotation::Deg270:
        matrix.element(0, 0) = 0.0F;
        matrix.element(0, 1) = -1.0F;
        matrix.element(1, 0) = 1.0F;
        matrix.element(1, 1) = 0.0F;
        break;
    }
    return matrix;
}

inline auto transformVector(const Mat4& matrix, const std::array<double, 4>& vector)
    -> std::array<double, 4> {
    std::array<double, 4> result{};
    for (std::size_t row = 0; row < 4; ++row) {
        for (std::size_t column = 0; column < 4; ++column) {
            result[row] += static_cast<double>(matrix.element(row, column)) * vector[column];
        }
    }
    return result;
}

// Maps an author-space NDC point through the base projection and then the final projection. Both
// matrices are named explicitly so the check cannot silently apply the perspective twice.
inline auto authorNdcToHostPixel(const Mat4& baseProjection, const Mat4& finalProjection,
                                 double ndcX, double ndcY, const FrameLayout& layout,
                                 double homogeneousW = 4.0) -> std::pair<double, double> {
    const std::array<double, 4> authorClip{ndcX * homogeneousW, ndcY * homogeneousW, -homogeneousW,
                                           homogeneousW};
    const auto inverseBase = cuexis::core::inverse(baseProjection);
    REQUIRE(inverseBase.has_value());
    const auto world = transformVector(*inverseBase, authorClip);
    const auto finalClip = transformVector(finalProjection, world);
    const auto finalNdcX = finalClip[0] / finalClip[3];
    const auto finalNdcY = finalClip[1] / finalClip[3];
    const auto contentX = (finalNdcX + 1.0) * 0.5 * static_cast<double>(layout.content.width);
    const auto contentY = (1.0 - finalNdcY) * 0.5 * static_cast<double>(layout.content.height);
    return {static_cast<double>(layout.content.x) + contentX,
            static_cast<double>(layout.content.y) + contentY};
}

inline void runFrameLayoutGeometryChecks() {
    using enum FrameRotation;
    using enum FramingSelection;

    // R5-01: 21:9 and 9:16 rounding, including the odd remainder split.
    {
        const auto first = requireLayout(cuexis::playback::experimental::computeContentRect(
            AspectRatio{21, 9}, makeTarget(1280, 720, pixelRect(0, 0, 1280, 720))));
        checkRect(first.content, pixelRect(0, 86, 1280, 548));

        const auto odd = requireLayout(cuexis::playback::experimental::computeContentRect(
            AspectRatio{21, 9}, makeTarget(1280, 721, pixelRect(0, 0, 1280, 721))));
        checkRect(odd.content, pixelRect(0, 86, 1280, 548));

        const auto full = requireLayout(cuexis::playback::experimental::computeContentRect(
            AspectRatio{9, 16}, makeTarget(720, 1280, pixelRect(0, 0, 720, 1280))));
        checkRect(full.content, pixelRect(0, 0, 720, 1280));

        const auto boxed = requireLayout(cuexis::playback::experimental::computeContentRect(
            AspectRatio{9, 16}, makeTarget(720, 720, pixelRect(0, 0, 720, 720))));
        checkRect(boxed.content, pixelRect(157, 0, 405, 720));
    }

    // R5-02: non-zero host origin and absolute content arithmetic.
    {
        const auto layout = requireLayout(cuexis::playback::experimental::computeContentRect(
            AspectRatio{21, 9}, makeTarget(1600, 1000, pixelRect(100, 50, 1280, 721))));
        checkRect(layout.content, pixelRect(0, 86, 1280, 548));
        CHECK(layout.target.host.x == 100U);
        CHECK(layout.target.host.y == 50U);
        CHECK(layout.content.x + layout.target.host.x == 100U);
        CHECK(layout.content.y + layout.target.host.y == 136U);
        CHECK(layout.content.width == 1280U);
        CHECK(layout.content.height == 548U);
    }

    // R5-03: actual drawable rounding does not replace the authored ratio.
    {
        const auto layout = requireLayout(cuexis::playback::experimental::computeContentRect(
            AspectRatio{21, 9}, makeTarget(1660, 718, pixelRect(0, 0, 1660, 718))));
        checkRect(layout.content, pixelRect(0, 3, 1660, 711));
    }

    // R5-04: empty targets succeed, out-of-bounds and zero aspects fail.
    {
        const auto emptyHost = cuexis::playback::experimental::computeContentRect(
            AspectRatio{21, 9}, makeTarget(10, 10, pixelRect(10, 0, 0, 10)));
        REQUIRE(emptyHost.has_value());
        CHECK_FALSE(emptyHost->has_value());

        const auto roundedEmpty = cuexis::playback::experimental::computeContentRect(
            AspectRatio{21, 9}, makeTarget(10, 10, pixelRect(0, 0, 1, 1)));
        REQUIRE(roundedEmpty.has_value());
        CHECK_FALSE(roundedEmpty->has_value());

        const auto overflow = cuexis::playback::experimental::computeContentRect(
            AspectRatio{21, 9},
            makeTarget(std::numeric_limits<std::uint32_t>::max(), 10,
                       pixelRect(std::numeric_limits<std::uint32_t>::max(), 0, 1, 10)));
        requireError(overflow, "playback.presentation.layout.invalid_target");

        const auto zeroAspect = cuexis::playback::experimental::computeContentRect(
            AspectRatio{0, 1}, makeTarget(10, 10, pixelRect(0, 0, 10, 10)));
        requireError(zeroAspect, "playback.presentation.layout.invalid_aspect");
    }

    // R5-06 pure layout extent boundary.
    {
        constexpr auto largest = 2147483647U;
        const auto full = requireLayout(cuexis::playback::experimental::computeContentRect(
            AspectRatio{largest, 1}, makeTarget(largest, 1, pixelRect(0, 0, largest, 1))));
        checkRect(full.content, pixelRect(0, 0, largest, 1));
        requireError(
            cuexis::playback::experimental::computeContentRect(
                AspectRatio{2147483648U, 1}, makeTarget(largest, 1, pixelRect(0, 0, largest, 1))),
            "playback.presentation.layout.invalid_aspect");
    }

    // R5-09: Default, BestFit, Exact, ties and rotated display ratios.
    const std::array framings{
        FrameFraming{AspectRatio{2, 1}, 1.0},
        FrameFraming{AspectRatio{1, 2}, 2.0},
        FrameFraming{AspectRatio{16, 9}, 1.0},
    };
    const std::span<const FrameFraming> fullTable{framings};
    const std::span<const FrameFraming> firstTwo{framings.data(), 2};
    {
        const auto square = makeTarget(100, 100, pixelRect(0, 0, 100, 100));
        const auto defaultChoice =
            requireSelected(cuexis::playback::experimental::computeFrameLayout(fullTable, square));
        REQUIRE(defaultChoice.framing.has_value());
        CHECK(defaultChoice.framing->aspect.width == 2U);
        CHECK(defaultChoice.framing->aspect.height == 1U);
        checkRect(defaultChoice.layout.content, pixelRect(0, 25, 100, 50));

        const auto best = requireSelected(cuexis::playback::experimental::computeFrameLayout(
            fullTable, square, FramingRequest{BestFit, std::nullopt}));
        REQUIRE(best.framing.has_value());
        CHECK(best.framing->aspect.width == 16U);
        checkRect(best.layout.content, pixelRect(0, 22, 100, 56));

        const auto tie = requireSelected(cuexis::playback::experimental::computeFrameLayout(
            firstTwo, square, FramingRequest{BestFit, std::nullopt}));
        REQUIRE(tie.framing.has_value());
        CHECK(tie.framing->aspect.width == 2U);

        const auto wide = makeTarget(200, 100, pixelRect(0, 0, 200, 100));
        const auto wideBest = requireSelected(cuexis::playback::experimental::computeFrameLayout(
            fullTable, wide, FramingRequest{BestFit, std::nullopt}));
        REQUIRE(wideBest.framing.has_value());
        CHECK(wideBest.framing->aspect.width == 2U);

        const auto rotated = makeTarget(200, 100, pixelRect(0, 0, 200, 100), Deg90);
        const auto rotatedBest = requireSelected(cuexis::playback::experimental::computeFrameLayout(
            fullTable, rotated, FramingRequest{BestFit, std::nullopt}));
        REQUIRE(rotatedBest.framing.has_value());
        CHECK(rotatedBest.framing->aspect.width == 1U);
        CHECK(rotatedBest.framing->aspect.height == 2U);

        const auto exact = requireSelected(cuexis::playback::experimental::computeFrameLayout(
            fullTable, wide, FramingRequest{Exact, AspectRatio{4, 2}}));
        REQUIRE(exact.framing.has_value());
        CHECK(exact.framing->aspect.width == 2U);
        checkRect(exact.layout.content, pixelRect(0, 0, 200, 100));

        requireError(cuexis::playback::experimental::computeFrameLayout(
                         fullTable, wide, FramingRequest{Exact, AspectRatio{3, 2}}),
                     "playback.presentation.layout.aspect_unavailable");

        const std::span<const FrameFraming> empty;
        const auto legacy = requireSelected(cuexis::playback::experimental::computeFrameLayout(
            empty, wide, FramingRequest{BestFit, std::nullopt}));
        CHECK_FALSE(legacy.framing.has_value());
        checkRect(legacy.layout.content, pixelRect(0, 0, 200, 100));

        requireError(cuexis::playback::experimental::computeFrameLayout(
                         empty, wide, FramingRequest{Exact, AspectRatio{2, 1}}),
                     "playback.presentation.layout.aspect_unavailable");

        const auto rotatedExact =
            requireSelected(cuexis::playback::experimental::computeFrameLayout(
                fullTable, makeTarget(100, 200, pixelRect(0, 0, 100, 200), Deg90),
                FramingRequest{Exact, AspectRatio{2, 1}}));
        REQUIRE(rotatedExact.framing.has_value());
        CHECK(rotatedExact.framing->aspect.width == 2U);
        checkRect(rotatedExact.layout.content, pixelRect(0, 0, 100, 200));

        const auto noArea = cuexis::playback::experimental::computeFrameLayout(
            fullTable, makeTarget(1, 1, pixelRect(0, 0, 1, 1)),
            FramingRequest{BestFit, std::nullopt});
        REQUIRE(noArea.has_value());
        CHECK_FALSE(noArea->has_value());
        requireError(cuexis::playback::experimental::computeFrameLayout(
                         fullTable, wide, FramingRequest{Default, AspectRatio{2, 1}}),
                     "playback.presentation.layout.invalid_selection");
        requireError(cuexis::playback::experimental::computeFrameLayout(
                         fullTable, wide, FramingRequest{BestFit, AspectRatio{2, 1}}),
                     "playback.presentation.layout.invalid_selection");
        requireError(cuexis::playback::experimental::computeFrameLayout(
                         fullTable, wide, FramingRequest{Exact, std::nullopt}),
                     "playback.presentation.layout.invalid_selection");
    }
}

inline void runFrameLayoutValidationChecks() {
    using enum FrameRotation;

    const std::array broad{
        FrameFraming{AspectRatio{42, 18}, 1.0},
    };
    const auto broadLayout = cuexis::playback::experimental::computeFrameLayout(
        std::span<const FrameFraming>{broad}, makeTarget(100, 100, pixelRect(0, 0, 100, 100)));
    REQUIRE(broadLayout.has_value());
    REQUIRE(broadLayout->has_value());
    CHECK((*broadLayout)->framing->aspect.width == 42U);

    const std::array duplicate{
        FrameFraming{AspectRatio{2, 1}, 1.0},
        FrameFraming{AspectRatio{4, 2}, 1.0},
    };
    requireError(cuexis::playback::experimental::computeFrameLayout(
                     std::span<const FrameFraming>{duplicate},
                     makeTarget(100, 100, pixelRect(0, 0, 100, 100))),
                 "playback.presentation.layout.invalid_framing");

    const std::array badScale{
        FrameFraming{AspectRatio{2, 1}, 0.124},
    };
    requireError(cuexis::playback::experimental::computeFrameLayout(
                     std::span<const FrameFraming>{badScale},
                     makeTarget(100, 100, pixelRect(0, 0, 100, 100))),
                 "playback.presentation.layout.invalid_framing");

    std::vector<FrameFraming> tooMany;
    tooMany.reserve(17);
    for (std::uint32_t index = 0; index < 17; ++index) {
        tooMany.push_back(FrameFraming{AspectRatio{index + 1, 1}, 1.0});
    }
    requireError(cuexis::playback::experimental::computeFrameLayout(
                     std::span<const FrameFraming>{tooMany},
                     makeTarget(100, 100, pixelRect(0, 0, 100, 100))),
                 "playback.presentation.layout.invalid_framing");

    requireError(cuexis::playback::experimental::computeContentRect(
                     AspectRatio{2, 1}, makeTarget(100, 100, pixelRect(0, 0, 100, 100),
                                                   static_cast<FrameRotation>(45))),
                 "playback.presentation.layout.invalid_target");
}

inline void runFrameLayoutProjectionChecks() {
    using enum FrameRotation;
    using enum FramingSelection;

    const std::array authored{
        FrameFraming{AspectRatio{21, 9}, 1.0},
    };
    const auto exactRequested = FramingRequest{Exact, AspectRatio{21, 9}};

    // R5-10: authored Q on a non-square content rectangle.
    {
        const auto host = makeTarget(1280, 721, pixelRect(0, 0, 1280, 721));
        const auto selected = requireSelected(cuexis::playback::experimental::computeFrameLayout(
            std::span<const FrameFraming>{authored}, host, exactRequested));
        checkRect(selected.layout.content, pixelRect(0, 86, 1280, 548));
        const auto base = perspective(90.0, 21.0 / 9.0);
        const auto projected = cuexis::playback::detail::composeFrameProjection(base, selected);
        REQUIRE(projected.has_value());
        CHECK(projected->element(0, 0) == Catch::Approx(base.element(0, 0)));
        CHECK(projected->element(1, 1) ==
              Catch::Approx(base.element(1, 1) * (1280.0 * 9.0 / 21.0) / 548.0));
    }

    // R5-10: 90 degrees and non-unit Q with the exact vector.
    {
        const auto host = makeTarget(721, 1280, pixelRect(0, 0, 721, 1280), Deg90);
        const auto selected = requireSelected(cuexis::playback::experimental::computeFrameLayout(
            std::span<const FrameFraming>{authored}, host, exactRequested));
        checkRect(selected.layout.content, pixelRect(86, 0, 548, 1280));
        const auto base = perspective(90.0, 21.0 / 9.0);
        const auto rotated = cuexis::core::multiply(rotationMatrix(Deg90), base);
        const auto projected = cuexis::playback::detail::composeFrameProjection(base, selected);
        REQUIRE(projected.has_value());
        const auto qxx = projected->element(0, 1) / rotated.element(0, 1);
        const auto qyy = projected->element(1, 0) / rotated.element(1, 0);
        CHECK(qxx == Catch::Approx(960.0 / 959.0).margin(1.0e-6));
        CHECK(qyy == Catch::Approx(1.0).margin(1.0e-6));

        const auto pixel = authorNdcToHostPixel(base, *projected, 0.25, -0.5, selected.layout);
        CHECK(pixel.first - static_cast<double>(selected.layout.content.x) ==
              Catch::Approx(958.0 / 7.0).margin(1.0e-5));
        CHECK(pixel.second == Catch::Approx(800.0).margin(1.0e-5));

        const std::array<double, 4> authorClip{0.25 * 4.0, -0.5 * 4.0, -4.0, 4.0};
        const auto inverseBase = cuexis::core::inverse(base);
        REQUIRE(inverseBase.has_value());
        const auto world = transformVector(*inverseBase, authorClip);
        const auto finalClip = transformVector(*projected, world);
        const auto inverseProjected = cuexis::core::inverse(*projected);
        REQUIRE(inverseProjected.has_value());
        const auto recoveredWorld = transformVector(*inverseProjected, finalClip);
        const auto recoveredAuthorClip = transformVector(base, recoveredWorld);
        CHECK(recoveredAuthorClip[0] / recoveredAuthorClip[3] ==
              Catch::Approx(0.25).margin(1.0e-6));
        CHECK(recoveredAuthorClip[1] / recoveredAuthorClip[3] ==
              Catch::Approx(-0.5).margin(1.0e-6));

        const auto rotation = rotationMatrix(Deg90);
        Mat4 rasterScale;
        rasterScale.element(0, 0) = static_cast<float>(qxx);
        rasterScale.element(1, 1) = static_cast<float>(qyy);
        const auto inverseRasterRotation =
            cuexis::core::inverse(cuexis::core::multiply(rasterScale, rotation));
        REQUIRE(inverseRasterRotation.has_value());
        const auto authorGuess = transformVector(*inverseRasterRotation, finalClip);
        const auto wrongWorld = transformVector(*inverseProjected, authorGuess);
        const auto wrongAuthorClip = transformVector(base, wrongWorld);
        const auto wrongX = wrongAuthorClip[0] / wrongAuthorClip[3];
        const auto wrongY = wrongAuthorClip[1] / wrongAuthorClip[3];
        CHECK(std::abs(wrongX - 0.25) + std::abs(wrongY + 0.5) > 1.0e-3);
    }

    // R5-10: four rotations map the same author NDC to the documented pixels.
    {
        const std::array framing{FrameFraming{AspectRatio{2, 1}, 1.0}};
        const auto exact = FramingRequest{Exact, AspectRatio{2, 1}};
        const auto checkAt = [&](FrameRotation rotation, PixelRect host, double expectedX,
                                 double expectedY) {
            const auto target = makeTarget(host.width, host.height,
                                           pixelRect(0, 0, host.width, host.height), rotation);
            const auto selected =
                requireSelected(cuexis::playback::experimental::computeFrameLayout(
                    std::span<const FrameFraming>{framing}, target, exact));
            const auto base = perspective(90.0, 2.0);
            const auto projected = cuexis::playback::detail::composeFrameProjection(base, selected);
            REQUIRE(projected.has_value());
            const auto pixel = authorNdcToHostPixel(base, *projected, 0.5, 0.0, selected.layout);
            CHECK(pixel.first == Catch::Approx(expectedX).margin(1.0e-5));
            CHECK(pixel.second == Catch::Approx(expectedY).margin(1.0e-5));
            return *projected;
        };
        checkAt(Deg0, pixelRect(0, 0, 200, 100), 150.0, 50.0);
        checkAt(Deg180, pixelRect(0, 0, 200, 100), 50.0, 50.0);
        checkAt(Deg90, pixelRect(0, 0, 100, 200), 50.0, 150.0);
        checkAt(Deg270, pixelRect(0, 0, 100, 200), 50.0, 50.0);
    }

    // R5-10: base P vertical scale is distinct from the final matrix entry.
    {
        const auto base = perspective(90.0, 2.0);
        const auto narrow = perspective(2.0 * 180.0 / 3.14159265358979323846 * std::atan(0.5), 2.0);
        CHECK(base.element(1, 1) == Catch::Approx(1.0));
        CHECK(narrow.element(1, 1) == Catch::Approx(2.0));
        const auto scaled90 =
            perspective(2.0 * 180.0 / 3.14159265358979323846 * std::atan(2.0), 2.0);
        const auto scaledNarrow =
            perspective(2.0 * 180.0 / 3.14159265358979323846 * std::atan(1.0), 2.0);
        CHECK(scaled90.element(1, 1) == Catch::Approx(0.5));
        CHECK(scaledNarrow.element(1, 1) == Catch::Approx(1.0));

        const std::array framing{FrameFraming{AspectRatio{2, 1}, 1.0}};
        const auto target = makeTarget(200, 100, pixelRect(0, 0, 200, 100));
        const auto selected = requireSelected(cuexis::playback::experimental::computeFrameLayout(
            std::span<const FrameFraming>{framing}, target,
            FramingRequest{Exact, AspectRatio{2, 1}}));
        const auto projected = cuexis::playback::detail::composeFrameProjection(narrow, selected);
        REQUIRE(projected.has_value());
        CHECK(projected->element(1, 1) == Catch::Approx(narrow.element(1, 1)));

        const auto quarterTarget = makeTarget(100, 200, pixelRect(0, 0, 100, 200), Deg90);
        const auto quarter = requireSelected(cuexis::playback::experimental::computeFrameLayout(
            std::span<const FrameFraming>{framing}, quarterTarget,
            FramingRequest{Exact, AspectRatio{2, 1}}));
        const auto rotatedProjection =
            cuexis::playback::detail::composeFrameProjection(base, quarter);
        REQUIRE(rotatedProjection.has_value());
        CHECK(rotatedProjection->element(1, 1) == 0.0F);
        CHECK(rotatedProjection->element(1, 0) == Catch::Approx(-base.element(0, 0)));
        CHECK_FALSE(rotatedProjection->element(1, 0) == Catch::Approx(base.element(1, 1)));
    }

    // Legacy paths: 0 degrees preserves the exact matrix; 90/270 use the reciprocal aspect and R.
    {
        const std::span<const FrameFraming> empty;
        const auto base = perspective(90.0, 1.5);
        const auto zeroTarget = makeTarget(300, 200, pixelRect(0, 0, 300, 200));
        const auto zero =
            requireSelected(cuexis::playback::experimental::computeFrameLayout(empty, zeroTarget));
        CHECK_FALSE(zero.framing.has_value());
        const auto zeroProjection = cuexis::playback::detail::composeFrameProjection(base, zero);
        REQUIRE(zeroProjection.has_value());
        CHECK(*zeroProjection == base);

        const auto quarterBase = perspective(90.0, 200.0 / 100.0);
        const auto quarterTarget = makeTarget(100, 200, pixelRect(0, 0, 100, 200), Deg90);
        const auto quarter = requireSelected(
            cuexis::playback::experimental::computeFrameLayout(empty, quarterTarget));
        CHECK_FALSE(quarter.framing.has_value());
        const auto quarterProjection =
            cuexis::playback::detail::composeFrameProjection(quarterBase, quarter);
        REQUIRE(quarterProjection.has_value());
        const auto expected = cuexis::core::multiply(rotationMatrix(Deg90), quarterBase);
        CHECK(cuexis::core::nearlyEqual(*quarterProjection, expected, 1.0e-6F));
        const auto pixel =
            authorNdcToHostPixel(quarterBase, *quarterProjection, 0.5, 0.0, quarter.layout);
        CHECK(pixel.first == Catch::Approx(50.0).margin(1.0e-5));
        CHECK(pixel.second == Catch::Approx(150.0).margin(1.0e-5));
        const auto swapped =
            authorNdcToHostPixel(quarterBase, quarterBase, 0.5, 0.0, quarter.layout);
        CHECK(swapped.first == Catch::Approx(75.0).margin(1.0e-5));
        CHECK(swapped.second == Catch::Approx(100.0).margin(1.0e-5));

        const auto reverseTarget = makeTarget(100, 200, pixelRect(0, 0, 100, 200), Deg270);
        const auto reverse = requireSelected(
            cuexis::playback::experimental::computeFrameLayout(empty, reverseTarget));
        const auto reverseProjection =
            cuexis::playback::detail::composeFrameProjection(quarterBase, reverse);
        REQUIRE(reverseProjection.has_value());
        const auto reversePixel =
            authorNdcToHostPixel(quarterBase, *reverseProjection, 0.5, 0.0, reverse.layout);
        CHECK(reversePixel.first == Catch::Approx(50.0).margin(1.0e-5));
        CHECK(reversePixel.second == Catch::Approx(50.0).margin(1.0e-5));
    }

    // Q is also applied after the rotation for a quarter turn.
    {
        const std::array framing{FrameFraming{AspectRatio{21, 9}, 1.0}};
        const auto target = makeTarget(721, 1280, pixelRect(0, 0, 721, 1280), Deg90);
        const auto selected = requireSelected(cuexis::playback::experimental::computeFrameLayout(
            std::span<const FrameFraming>{framing}, target,
            FramingRequest{Exact, AspectRatio{21, 9}}));
        const auto base = perspective(90.0, 21.0 / 9.0);
        const auto projected = cuexis::playback::detail::composeFrameProjection(base, selected);
        REQUIRE(projected.has_value());
        const auto rotated = cuexis::core::multiply(rotationMatrix(Deg90), base);
        CHECK(projected->element(0, 1) / rotated.element(0, 1) ==
              Catch::Approx(960.0 / 959.0).margin(1.0e-6));
        CHECK(projected->element(1, 0) / rotated.element(1, 0) ==
              Catch::Approx(1.0).margin(1.0e-6));
    }

    // Invalid and non-finite inputs stay on the camera diagnostic.
    {
        const std::array framing{FrameFraming{AspectRatio{21, 9}, 1.0}};
        auto target = makeTarget(1280, 721, pixelRect(0, 0, 1280, 721));
        const auto selected = requireSelected(cuexis::playback::experimental::computeFrameLayout(
            std::span<const FrameFraming>{framing}, target,
            FramingRequest{Exact, AspectRatio{21, 9}}));
        auto invalid = selected;
        invalid.layout.target.rotation = static_cast<FrameRotation>(45);
        const auto base = perspective(90.0, 21.0 / 9.0);
        requireError(cuexis::playback::detail::composeFrameProjection(base, invalid),
                     "playback.snapshot.camera_invalid");
        Mat4 nonFinite;
        nonFinite.element(0, 0) = std::numeric_limits<float>::infinity();
        requireError(cuexis::playback::detail::composeFrameProjection(nonFinite, selected),
                     "playback.snapshot.camera_invalid");
    }
}

} // namespace cuexis::test::frame_layout

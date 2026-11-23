#include "frame_layout_vectors.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("rr_frame_layout_geometry", "[playback][frame_layout][r5]") {
    cuexis::test::frame_layout::runFrameLayoutGeometryChecks();
}

TEST_CASE("rr_frame_layout_validation", "[playback][frame_layout][r5]") {
    cuexis::test::frame_layout::runFrameLayoutValidationChecks();
}

TEST_CASE("rr_frame_layout_projection", "[playback][frame_layout][r5]") {
    cuexis::test::frame_layout::runFrameLayoutProjectionChecks();
}

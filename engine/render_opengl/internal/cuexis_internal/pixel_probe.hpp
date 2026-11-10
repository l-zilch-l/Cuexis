#pragma once

#include <cuexis/render_opengl/open_gl_backend.hpp>

namespace cuexis::render_opengl::detail {
// Owner-thread diagnostics only. Ordinary rendering never requests synchronous pixel readback.
struct OpenGlDiagnosticsAccess final {
    static auto setPixelReadback(OpenGlBackend&, bool enabled) -> core::Result<void>;
};
} // namespace cuexis::render_opengl::detail

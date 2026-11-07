#include "../internal/cuexis_internal/gpu_timer.hpp"
#include <glad/glad.h>

namespace cuexis::render_opengl::test_only {
namespace {
auto failure(const char* text) {
    return core::unexpected(core::Error{"pd5.gpu.query", text});
}
} // namespace
GpuTimer::~GpuTimer() noexcept {
    if (initialized_) {
        if (!owner_.isCurrent())
            std::terminate();
        for (auto& slot : slots_)
            glDeleteQueries(2, slot.queries.data());
    }
}
auto GpuTimer::initialize() -> core::Result<void> {
    if (!owner_.isCurrent() || initialized_)
        return failure("Invalid GPU timer owner or initialization");
    GLint bits{};
    glGetQueryiv(GL_TIMESTAMP, GL_QUERY_COUNTER_BITS, &bits);
    if (bits <= 0)
        return failure("GPU timestamp query is unavailable");
    initialized_ = true;
    for (auto& slot : slots_)
        glGenQueries(2, slot.queries.data());
    if (glGetError() != GL_NO_ERROR)
        return failure("GPU query allocation failed");
    return {};
}
auto GpuTimer::close() -> core::Result<void> {
    if (!owner_.isCurrent() || active_ || count_)
        return failure("GPU queries must finish before context close/rebuild");
    if (initialized_) {
        for (auto& slot : slots_) {
            glDeleteQueries(2, slot.queries.data());
            slot.queries = {};
        }
        initialized_ = false;
    }
    return {};
}
auto GpuTimer::begin(std::uint64_t ordinal) -> core::Result<void> {
    if (!owner_.isCurrent() || !initialized_ || active_ || count_ == slots_.size())
        return failure("GPU query owner/state/capacity rejected");
    auto& slot = slots_[(head_ + count_) % slots_.size()];
    slot.ordinal = ordinal;
    glQueryCounter(slot.queries[0], GL_TIMESTAMP);
    active_ = true;
    return {};
}
auto GpuTimer::end() -> core::Result<void> {
    if (!owner_.isCurrent() || !active_)
        return failure("GPU timer end without begin");
    glQueryCounter(slots_[(head_ + count_) % slots_.size()].queries[1], GL_TIMESTAMP);
    active_ = false;
    ++count_;
    if (glGetError() != GL_NO_ERROR)
        return failure("GPU timestamp submission failed");
    return {};
}
auto GpuTimer::poll() -> core::Result<std::optional<Sample>> {
    if (!owner_.isCurrent() || !initialized_ || active_)
        return failure("GPU poll owner/state rejected");
    if (!count_)
        return std::optional<Sample>{};
    const auto& slot = slots_[head_];
    GLint available{};
    glGetQueryObjectiv(slot.queries[1], GL_QUERY_RESULT_AVAILABLE, &available);
    if (!available)
        return std::optional<Sample>{};
    GLuint64 start{}, end{};
    glGetQueryObjectui64v(slot.queries[0], GL_QUERY_RESULT, &start);
    glGetQueryObjectui64v(slot.queries[1], GL_QUERY_RESULT, &end);
    if (glGetError() != GL_NO_ERROR || end < start)
        return failure("GPU timestamp error/wrap; sample rejected");
    const Sample result{slot.ordinal, end - start};
    head_ = (head_ + 1) % slots_.size();
    --count_;
    return std::optional<Sample>{result};
}
} // namespace cuexis::render_opengl::test_only

#include <cuexis/core/log_sink.hpp>

#include <utility>

namespace cuexis::core {

LogSink::LogSink(LogCallback callback) : callback_(std::move(callback)) {}

void LogSink::write(LogSeverity severity, std::string_view category,
                    std::string_view message) const noexcept {
    if (!callback_) {
        return;
    }
    try {
        callback_(LogEvent{.severity = severity, .category = category, .message = message});
    } catch (...) {
        // Diagnostics must never alter adapter or host control flow.
    }
}

} // namespace cuexis::core

#pragma once

// Instance-owned, exception-contained logging callback for optional adapters and hosts.

#include <functional>
#include <string_view>

namespace cuexis::core {

enum class LogSeverity {
    Info,
    Warning,
    Error,
};

struct LogEvent final {
    LogSeverity severity{LogSeverity::Info};
    std::string_view category;
    std::string_view message;
};

using LogCallback = std::function<void(const LogEvent&)>;

class LogSink final {
  public:
    explicit LogSink(LogCallback callback = {});

    void write(LogSeverity severity, std::string_view category,
               std::string_view message) const noexcept;

  private:
    LogCallback callback_;
};

} // namespace cuexis::core

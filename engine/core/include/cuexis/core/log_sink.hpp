#pragma once

// Instance-owned, exception-contained logging callback for optional adapters and hosts.

#include <cuexis/core/abi_warnings.hpp>
#include <cuexis/core/core_export.hpp>

#include <functional>
#include <string_view>

namespace cuexis::core {

CUEXIS_ABI_WARNING_PUSH

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

class CUEXIS_CORE_API LogSink final {
  public:
    explicit LogSink(LogCallback callback = {});

    void write(LogSeverity severity, std::string_view category,
               std::string_view message) const noexcept;

  private:
    LogCallback callback_;
};

CUEXIS_ABI_WARNING_POP

} // namespace cuexis::core

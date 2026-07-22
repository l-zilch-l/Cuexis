//  log 模块实现 — 基于 spdlog 的结构化日志封装
//  初始化创建后台 logger；shutdown 清理并释放资源
//  所有写操作线程安全，异常在模块内部捕获，永不逃逸到调用方

#include <cuexis/core/log.hpp>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

namespace cuexis::core::log {
namespace {

std::mutex loggerMutex;
std::shared_ptr<spdlog::logger> logger;
std::string loggerName;

void write(spdlog::level::level_enum level, std::string_view category,
           std::string_view message) noexcept {
    try {
        const std::scoped_lock lock(loggerMutex);
        if (logger == nullptr) {
            return;
        }
        logger->log(level, "[{}] {}", category, message);
    } catch (...) {
        // Logging must never escape into application or real-time control flow.
    }
}

} // namespace

Result<void> init(std::string_view applicationName) noexcept {
    std::string requestedName;
    bool registeredLogger = false;
    try {
        if (applicationName.empty()) {
            return unexpected(Error{"core.log.invalid_name", "Logger name must not be empty"});
        }
        requestedName.assign(applicationName);

        const std::scoped_lock lock(loggerMutex);
        if (logger != nullptr) {
            return {};
        }

        auto createdLogger = spdlog::stdout_color_mt(requestedName);
        registeredLogger = true;
        createdLogger->set_pattern("%Y-%m-%dT%H:%M:%S.%e%z [%l] [%n] %v");
        createdLogger->flush_on(spdlog::level::err);

        loggerName = requestedName;
        logger = std::move(createdLogger);
        return {};
    } catch (const std::exception& exception) {
        if (registeredLogger) {
            try {
                const std::scoped_lock lock(loggerMutex);
                spdlog::drop(requestedName);
            } catch (...) {
            }
        }
        return unexpected(Error{"core.log.init_failed", "Failed to initialize logging"}
                              .withContext("application_name", requestedName)
                              .withContext("exception", exception.what()));
    } catch (...) {
        if (registeredLogger) {
            try {
                const std::scoped_lock lock(loggerMutex);
                spdlog::drop(requestedName);
            } catch (...) {
            }
        }
        return unexpected(Error{"core.log.init_failed", "Failed to initialize logging"}.withContext(
            "application_name", requestedName));
    }
}

void shutdown() noexcept {
    try {
        const std::scoped_lock lock(loggerMutex);
        auto current = std::exchange(logger, nullptr);
        auto name = std::exchange(loggerName, {});

        if (current != nullptr) {
            try {
                current->flush();
            } catch (...) {
            }
        }
        if (!name.empty()) {
            try {
                spdlog::drop(name);
            } catch (...) {
            }
        }
    } catch (...) {
        // Shutdown and resource release paths are required to remain noexcept.
    }
}

void info(std::string_view category, std::string_view message) noexcept {
    write(spdlog::level::info, category, message);
}

void warn(std::string_view category, std::string_view message) noexcept {
    write(spdlog::level::warn, category, message);
}

void error(std::string_view category, std::string_view message) noexcept {
    write(spdlog::level::err, category, message);
}

} // namespace cuexis::core::log

#include "player_log.hpp"

#include <cuexis/core/error.hpp>

#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <exception>
#include <memory>
#include <string>
#include <utility>

namespace cuexis::player {

struct PlayerLogger::State final {
    std::shared_ptr<spdlog::logger> logger;
    std::shared_ptr<core::LogSink> sink;
};

PlayerLogger::PlayerLogger(std::shared_ptr<State> state) noexcept : state_(std::move(state)) {}

auto PlayerLogger::create(std::string_view applicationName)
    -> core::Result<std::unique_ptr<PlayerLogger>> {
    if (applicationName.empty()) {
        return core::unexpected(
            core::Error{"player.log.invalid_name", "Logger name must not be empty"});
    }
    try {
        auto output = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto logger = std::make_shared<spdlog::logger>(std::string{applicationName}, output);
        logger->set_pattern("%Y-%m-%dT%H:%M:%S.%e%z [%l] [%n] %v");
        logger->flush_on(spdlog::level::err);

        auto state = std::make_shared<State>();
        state->logger = std::move(logger);
        const std::weak_ptr weakLogger{state->logger};
        state->sink = std::make_shared<core::LogSink>([weakLogger](const core::LogEvent& event) {
            const auto locked = weakLogger.lock();
            if (!locked) {
                return;
            }
            switch (event.severity) {
            case core::LogSeverity::Info:
                locked->info("[{}] {}", event.category, event.message);
                break;
            case core::LogSeverity::Warning:
                locked->warn("[{}] {}", event.category, event.message);
                break;
            case core::LogSeverity::Error:
                locked->error("[{}] {}", event.category, event.message);
                break;
            }
        });
        return std::unique_ptr<PlayerLogger>{new PlayerLogger{std::move(state)}};
    } catch (const std::exception& exception) {
        return core::unexpected(
            core::Error{"player.log.init_failed", "Failed to initialize logging"}.withContext(
                "exception", exception.what()));
    } catch (...) {
        return core::unexpected(
            core::Error{"player.log.init_failed", "Failed to initialize logging"});
    }
}

PlayerLogger::~PlayerLogger() {
    if (state_ && state_->logger) {
        try {
            state_->logger->flush();
        } catch (...) {
        }
    }
}

void PlayerLogger::info(std::string_view category, std::string_view message) const noexcept {
    state_->sink->write(core::LogSeverity::Info, category, message);
}

void PlayerLogger::warn(std::string_view category, std::string_view message) const noexcept {
    state_->sink->write(core::LogSeverity::Warning, category, message);
}

void PlayerLogger::error(std::string_view category, std::string_view message) const noexcept {
    state_->sink->write(core::LogSeverity::Error, category, message);
}

auto PlayerLogger::sink() const noexcept -> std::shared_ptr<const core::LogSink> {
    return state_->sink;
}

} // namespace cuexis::player

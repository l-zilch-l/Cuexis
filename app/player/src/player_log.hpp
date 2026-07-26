#pragma once

#include <cuexis/core/log_sink.hpp>
#include <cuexis/core/result.hpp>

#include <memory>
#include <string_view>

namespace cuexis::player {

class PlayerLogger final {
  public:
    [[nodiscard]] static auto create(std::string_view applicationName)
        -> core::Result<std::unique_ptr<PlayerLogger>>;

    ~PlayerLogger();

    PlayerLogger(const PlayerLogger&) = delete;
    auto operator=(const PlayerLogger&) -> PlayerLogger& = delete;
    PlayerLogger(PlayerLogger&&) = delete;
    auto operator=(PlayerLogger&&) -> PlayerLogger& = delete;

    void info(std::string_view category, std::string_view message) const noexcept;
    void warn(std::string_view category, std::string_view message) const noexcept;
    void error(std::string_view category, std::string_view message) const noexcept;

    [[nodiscard]] auto sink() const noexcept -> std::shared_ptr<const core::LogSink>;

  private:
    struct State;
    explicit PlayerLogger(std::shared_ptr<State> state) noexcept;

    std::shared_ptr<State> state_;
};

} // namespace cuexis::player

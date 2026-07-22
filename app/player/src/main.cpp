//  Cuexis Player 入口 — main() 函数
//  初始化日志系统 → 设置 RAII log shutdown → 调用 cuexis::player::run()
//  异常在模块边界捕获，转换为结构化错误日志后返回 EXIT_FAILURE

#include "player_app.hpp"

#include <cuexis/core/error.hpp>
#include <cuexis/core/log.hpp>

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

namespace {

class LogSession final {
  public:
    LogSession() = default;
    ~LogSession() {
        cuexis::core::log::shutdown();
    }

    LogSession(const LogSession&) = delete;
    auto operator=(const LogSession&) -> LogSession& = delete;
};

void appendError(std::string& output, const cuexis::core::Error& error) {
    output.append(error.code());
    output.append(": ");
    output.append(error.message());

    for (const auto& item : error.context()) {
        output.append(" [");
        output.append(item.key);
        output.append("=");
        output.append(item.value);
        output.append("]");
    }

    if (const auto* cause = error.cause(); cause != nullptr) {
        output.append("; caused by ");
        appendError(output, *cause);
    }
}

[[nodiscard]] auto describeError(const cuexis::core::Error& error) -> std::string {
    std::string description;
    appendError(description, error);
    return description;
}

} // namespace

int main(int argumentCount, char** arguments) {
    auto logResult = cuexis::core::log::init("Cuexis Player");
    if (!logResult) {
        std::cerr << describeError(logResult.error()) << '\n';
        return EXIT_FAILURE;
    }
    const LogSession logSession;

    try {
        auto result = cuexis::player::run(argumentCount, arguments);
        if (!result) {
            cuexis::core::log::error("player.failure", describeError(result.error()));
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    } catch (const std::exception& exception) {
        cuexis::core::log::error("player.exception",
                                 std::string{"player.exception.std: "} + exception.what());
        return EXIT_FAILURE;
    } catch (...) {
        cuexis::core::log::error("player.exception",
                                 "player.exception.unknown: Unhandled non-standard exception");
        return EXIT_FAILURE;
    }
}

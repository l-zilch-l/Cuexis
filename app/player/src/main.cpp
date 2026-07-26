//  Cuexis Player 入口 — main() 函数
//  初始化日志系统 → 设置 RAII log shutdown → 调用 cuexis::player::run()
//  异常在模块边界捕获，转换为结构化错误日志后返回 EXIT_FAILURE

#include "player_app.hpp"
#include "player_log.hpp"

#include <cuexis/core/error.hpp>

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

namespace {

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
    auto logger = cuexis::player::PlayerLogger::create("Cuexis Player");
    if (!logger) {
        std::cerr << describeError(logger.error()) << '\n';
        return EXIT_FAILURE;
    }

    try {
        auto result = cuexis::player::run(argumentCount, arguments, **logger);
        if (!result) {
            (*logger)->error("player.failure", describeError(result.error()));
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    } catch (const std::exception& exception) {
        (*logger)->error("player.exception",
                         std::string{"player.exception.std: "} + exception.what());
        return EXIT_FAILURE;
    } catch (...) {
        (*logger)->error("player.exception",
                         "player.exception.unknown: Unhandled non-standard exception");
        return EXIT_FAILURE;
    }
}

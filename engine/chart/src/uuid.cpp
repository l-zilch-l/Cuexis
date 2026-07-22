#include <cuexis/chart/uuid.hpp>

#include <cuexis/core/uuid.hpp>

#include <string>
#include <string_view>
#include <utility>

namespace cuexis::chart {

auto isUuidV7(std::string_view text) noexcept -> bool {
    return core::isUuidV7(text);
}

auto isUuidV5(std::string_view text) noexcept -> bool {
    return core::isUuidV5(text);
}

auto uuidV5(std::string_view namespaceUuid, std::string_view name) -> core::Result<std::string> {
    auto generated = core::uuidV5(namespaceUuid, name);
    if (generated) {
        return generated;
    }

    std::string code{generated.error().code()};
    if (code == "core.uuid.invalid_namespace") {
        code = "chart.uuid.invalid_namespace";
    } else if (code == "core.uuid.name_too_long") {
        code = "chart.uuid.name_too_long";
    }
    core::Error error{std::move(code), std::string{generated.error().message()}};
    for (const auto& context : generated.error().context()) {
        error.withContext(context.key, context.value);
    }
    return core::unexpected(std::move(error));
}

} // namespace cuexis::chart

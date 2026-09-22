#include "config_json.hpp"

#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>

namespace cuexis::player_support::detail {

auto readTextFile(const std::filesystem::path& path) -> core::Result<std::string> {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        return core::unexpected(
            core::Error{"player.config.read_failed", "The configuration file could not be read"}
                .withContext("path", path.string()));
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    if (!input && !input.eof()) {
        return core::unexpected(
            core::Error{"player.config.read_failed", "The configuration file could not be read"}
                .withContext("path", path.string()));
    }
    return buffer.str();
}

auto loadSchema(const std::filesystem::path& schemaPath) -> core::Result<json::Value> {
    auto text = readTextFile(schemaPath);
    if (!text) {
        return core::unexpected(std::move(text.error()).withContext("role", "schema"));
    }
    auto parsed = json::parse(*text, configParseLimits);
    if (!parsed) {
        return core::unexpected(std::move(parsed.error()).withContext("role", "schema"));
    }
    return std::move(*parsed);
}

auto readInteger(const json::Value& value) noexcept -> std::optional<std::int64_t> {
    if (const auto* signedValue = value.signedInteger()) {
        return *signedValue;
    }
    if (const auto* unsignedValue = value.unsignedInteger()) {
        if (*unsignedValue > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
            return std::nullopt;
        }
        return static_cast<std::int64_t>(*unsignedValue);
    }
    return std::nullopt;
}

auto objectField(const json::Value::Object& object, std::string_view key) noexcept
    -> const json::Value* {
    const auto found = object.find(key);
    return found == object.end() ? nullptr : &found->second;
}

} // namespace cuexis::player_support::detail

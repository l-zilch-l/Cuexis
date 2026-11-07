#pragma once

#include <cstdlib>
#include <stdexcept>
#include <string>

namespace cuexis::test_support {
inline auto environmentVariable(const char* name) -> std::string {
#ifdef _MSC_VER
    std::size_t size{};
    if (getenv_s(&size, nullptr, 0, name) != 0) {
        throw std::runtime_error("Cannot inspect test environment variable");
    }
    if (!size) {
        return {};
    }
    std::string value(size, '\0');
    if (getenv_s(&size, value.data(), value.size(), name) != 0) {
        throw std::runtime_error("Cannot read test environment variable");
    }
    value.resize(size - 1);
    return value;
#else
    const auto* value = std::getenv(name);
    return value ? std::string{value} : std::string{};
#endif
}
} // namespace cuexis::test_support

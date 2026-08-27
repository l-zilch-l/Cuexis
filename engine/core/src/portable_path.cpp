#include <cuexis_internal/portable_path.hpp>

namespace cuexis::core::detail {

auto foldAscii(std::string_view value) -> std::string {
    std::string result;
    result.reserve(value.size());
    for (const char character : value) {
        result.push_back(character >= 'A' && character <= 'Z'
                             ? static_cast<char>(character - 'A' + 'a')
                             : character);
    }
    return result;
}

auto isWindowsReservedSegment(std::string_view segment) -> bool {
    const auto dot = segment.find('.');
    const auto stem = foldAscii(segment.substr(0, dot));
    if (stem == "con" || stem == "prn" || stem == "aux" || stem == "nul" || stem == "clock$" ||
        stem == "conin$" || stem == "conout$") {
        return true;
    }
    if (stem.size() == 4 && stem[3] >= '1' && stem[3] <= '9') {
        return stem.starts_with("com") || stem.starts_with("lpt");
    }
    return false;
}

} // namespace cuexis::core::detail

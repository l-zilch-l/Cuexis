#include "chart_project_path_internal.hpp"

#include <cuexis/core/math.hpp>
#include <cuexis_internal/portable_path.hpp>

#include <algorithm>
#include <string_view>

namespace cuexis::chart::detail {
namespace {

[[nodiscard]] auto isAsciiAlphaNumeric(char character) noexcept -> bool {
    return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') ||
           (character >= '0' && character <= '9');
}
using cuexis::core::detail::foldAscii;
using cuexis::core::detail::isWindowsReservedSegment;

} // namespace

auto isPortableProjectPath(std::string_view path) noexcept -> bool {
    if (path.empty() || path.size() > 4096 || path.front() == '/' || path.back() == '/' ||
        path.find('\\') != std::string_view::npos || path.find(':') != std::string_view::npos ||
        path.find("//") != std::string_view::npos) {
        return false;
    }
    std::size_t segmentStart = 0;
    while (segmentStart < path.size()) {
        const auto separator = path.find('/', segmentStart);
        const auto segment = path.substr(segmentStart, separator - segmentStart);
        if (segment.empty() || segment == "." || segment == ".." || segment.back() == ' ' ||
            segment.back() == '.' || isWindowsReservedSegment(segment)) {
            return false;
        }
        if (!std::ranges::all_of(segment, [](char character) {
                return isAsciiAlphaNumeric(character) || character == '.' || character == '_' ||
                       character == '-';
            })) {
            return false;
        }
        if (separator == std::string_view::npos) {
            break;
        }
        segmentStart = separator + 1;
    }
    return true;
}

auto isCxtProjectPath(std::string_view path) noexcept -> bool {
    return isPortableProjectPath(path) && path.ends_with(".cxt");
}

auto portableProjectPathCaseKey(std::string_view path) -> std::string {
    return foldAscii(path);
}

} // namespace cuexis::chart::detail

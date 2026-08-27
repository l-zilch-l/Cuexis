#include "cxc_path_internal.hpp"

#include <cuexis_internal/portable_path.hpp>

#include <algorithm>
#include <ranges>
#include <utility>

namespace cuexis::cxc::detail {
namespace {

[[nodiscard]] auto isAsciiAlphaNumeric(char character) noexcept -> bool {
    return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') ||
           (character >= '0' && character <= '9');
}

} // namespace

auto foldAscii(std::string_view value) -> std::string {
    return core::detail::foldAscii(value);
}

auto insertUniqueArchivePath(std::set<std::string, std::less<>>& foldedPaths, std::string_view path)
    -> bool {
    auto folded = foldAscii(path);
    if (foldedPaths.contains(folded)) {
        return false;
    }

    auto separator = folded.find('/');
    while (separator != std::string::npos) {
        if (foldedPaths.contains(folded.substr(0, separator))) {
            return false;
        }
        separator = folded.find('/', separator + 1U);
    }

    auto descendantPrefix = folded;
    descendantPrefix.push_back('/');
    const auto descendant = foldedPaths.lower_bound(descendantPrefix);
    if (descendant != foldedPaths.end() && descendant->starts_with(descendantPrefix)) {
        return false;
    }
    foldedPaths.emplace(std::move(folded));
    return true;
}

auto isPortablePath(std::string_view path, std::size_t maxBytes, std::size_t maxDepth) -> bool {
    if (path.empty() || maxBytes == 0 || maxDepth == 0 || path.size() > maxBytes ||
        path.front() == '/' || path.back() == '/' || path.find("//") != std::string_view::npos) {
        return false;
    }

    std::size_t depth = 0;
    std::size_t segmentStart = 0;
    while (segmentStart < path.size()) {
        const auto separator = path.find('/', segmentStart);
        const auto segment = path.substr(segmentStart, separator == std::string_view::npos
                                                           ? path.size() - segmentStart
                                                           : separator - segmentStart);
        ++depth;
        if (depth > maxDepth || segment.empty() || segment == "." || segment == ".." ||
            segment.back() == '.' || segment.back() == ' ' ||
            core::detail::isWindowsReservedSegment(segment) ||
            !std::ranges::all_of(segment, [](char character) {
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

auto joinPortablePath(std::string_view base, std::string_view relative, std::size_t maxBytes,
                      std::size_t maxDepth) -> std::optional<std::string> {
    if (!isPortablePath(base, maxBytes, maxDepth) ||
        !isPortablePath(relative, maxBytes, maxDepth)) {
        return std::nullopt;
    }
    if (base.size() >= maxBytes || relative.size() > maxBytes - base.size() - 1U) {
        return std::nullopt;
    }
    std::string result;
    result.reserve(base.size() + relative.size() + 1U);
    result.append(base);
    result.push_back('/');
    result.append(relative);
    return isPortablePath(result, maxBytes, maxDepth)
               ? std::optional<std::string>{std::move(result)}
               : std::nullopt;
}

} // namespace cuexis::cxc::detail

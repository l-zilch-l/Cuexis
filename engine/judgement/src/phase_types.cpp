#include <cuexis/judgement/phase_types.hpp>

namespace cuexis::judgement {

auto factCategoryToken(FactCategory category) noexcept -> std::string_view {
    switch (category) {
    case FactCategory::tap:
        return "tap";
    case FactCategory::holdHead:
        return "hold_head";
    case FactCategory::holdBody:
        return "hold_body";
    case FactCategory::holdTail:
        return "hold_tail";
    }
    return "?";
}

} // namespace cuexis::judgement

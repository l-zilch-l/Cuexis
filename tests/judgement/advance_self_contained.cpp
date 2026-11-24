// Self-containment probe for the advance value header (see the prepared identity probe).
#include <cuexis/judgement/advance.hpp>

namespace {
using AdvanceOutcome = cuexis::judgement::AdvanceOutcome;
static_assert(sizeof(AdvanceOutcome) > 0);
static_assert(static_cast<int>(cuexis::judgement::AdvanceStatus::ongoing) == 0);
static_assert(static_cast<int>(cuexis::judgement::TimingDirection::late) == 2);
} // namespace

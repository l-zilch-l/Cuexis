// Self-containment probe for the normalized score record header (see the prepared identity probe).
#include <cuexis/judgement/normalized_score.hpp>

namespace {
using PreparedNormalizedScore = cuexis::judgement::PreparedNormalizedScore;
static_assert(sizeof(PreparedNormalizedScore) > 0);
} // namespace

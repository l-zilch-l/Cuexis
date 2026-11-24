// Self-containment probe for the prepared gameplay handle header (see the prepared identity probe).
#include <cuexis/judgement/prepared_gameplay.hpp>

namespace {
using PreparedGameplay = cuexis::judgement::runtime::PreparedGameplay;
static_assert(sizeof(PreparedGameplay) > 0);
} // namespace

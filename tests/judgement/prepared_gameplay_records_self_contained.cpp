// Self-containment probe for the runtime prepared records header (see the prepared identity probe).
#include <cuexis/judgement/prepared_gameplay_records.hpp>

namespace {
using PreparedRequirement = cuexis::judgement::runtime::PreparedRequirement;
using CompiledGameplayView = cuexis::judgement::runtime::CompiledGameplayView;
static_assert(sizeof(PreparedRequirement) > 0);
static_assert(sizeof(CompiledGameplayView) > 0);
} // namespace

// Self-containment probe for the runtime phase value header (see the prepared identity probe).
#include <cuexis/judgement/phase_types.hpp>

namespace {
using StableDeclarationId = cuexis::judgement::StableDeclarationId;
using RequirementIdentity = cuexis::judgement::RequirementIdentity;
static_assert(sizeof(StableDeclarationId) > 0);
static_assert(sizeof(RequirementIdentity) > 0);
static_assert(cuexis::judgement::factCategoryOfPhase(cuexis::judgement::PhaseKind::tap) ==
              cuexis::judgement::FactCategory::tap);
} // namespace

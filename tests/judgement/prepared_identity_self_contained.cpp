// Self-containment probe for the runtime value header. This translation unit includes exactly one
// Cuexis header, so a missing standard include or a hidden compile-side dependency fails here.
#include <cuexis/judgement/prepared_identity.hpp>

namespace {
using PreparedIdentityDeclarations = cuexis::judgement::PreparedIdentityDeclarations;
static_assert(sizeof(PreparedIdentityDeclarations) > 0);
} // namespace

// Shared-candidate test support: the two Prepared-owner introspection seams are defined in the
// Playback library but are intentionally not exported. A shared-library test executable cannot
// import them, so it compiles the same private-access definitions locally. The body is identical to
// playback_session_prepared.cpp; nothing here reaches the SDK surface.

#include "playback_candidate_internal.hpp"
#include "playback_session_internal.hpp"

namespace cuexis::playback::detail {

auto CandidateMetadataAccess::preparedOwnerDereferences(const PreparedPlayback& playback) noexcept
    -> std::uint64_t {
    return playback.state_ == nullptr ? 0 : playback.state_->ownerDereferences;
}

void CandidateMetadataAccess::expirePreparedOwnerLease(PreparedPlayback& playback) noexcept {
    if (playback.state_ != nullptr) {
        playback.state_->ownerLease.reset();
    }
}

} // namespace cuexis::playback::detail

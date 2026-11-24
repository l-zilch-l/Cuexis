#pragma once

//  Judgement compile side - the legacy prepared-to-runtime view bridge.
//
//  The runtime kernel consumes `cuexis::judgement::runtime::PreparedGameplay` only. Legacy content
//  is still accepted by the old author API (`prepareGameplay` / `prepareResolvedGameplay`), so the
//  compile side owns the one conversion that turns an owning legacy `PreparedGameplay` into an
//  owning runtime handle. The runtime library never includes this header; a caller that only links
//  `cuexis::judgement` never sees it.

#include <cuexis/core/result.hpp>
#include <cuexis/judgement/gameplay_prepare.hpp>
#include <cuexis/judgement/prepared_gameplay.hpp>

namespace cuexis::judgement::compile {

//  Validates and converts one legacy prepared gameplay into the runtime prepared handle. The
//  conversion copies every runtime record; the returned handle owns its data and stays valid after
//  the legacy handle is destroyed. The legacy path carries no normalized score, so the runtime view
//  has no `normalizedScore`; legacy score/fold behaviour is preserved by the caller, not by this
//  bridge.
[[nodiscard]] auto preparedGameplayView(const PreparedGameplay& prepared)
    -> core::Result<runtime::PreparedGameplay>;

} // namespace cuexis::judgement::compile

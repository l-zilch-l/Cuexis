#pragma once

//  Judgement compile side - the two byte projections the runtime identity needs.
//
//  `chartProjectionBytes` extracts the exact chart projection from the legacy chart identity bytes
//  (the encoder prefix is known and fixed), and `executionSessionBytes` encodes the session
//  mapping, late policy and calibration with the same algorithm the legacy identity factory uses.
//  The runtime side composes the four pieces; this header never exposes an author type to it.

#include <cuexis/core/result.hpp>
#include <cuexis/judgement/gameplay_prepare.hpp>
#include <cuexis/judgement/input_boundary.hpp>
#include <cuexis/judgement/prepared_identity.hpp>
#include <cuexis/judgement/prepared_identity_bytes.hpp>

#include <cstddef>
#include <string_view>
#include <vector>

namespace cuexis::judgement::compile {

//  The chart projection inside one legacy prepared chart identity, with the chart signature and
//  component header removed.
[[nodiscard]] auto chartProjectionBytes(const PreparedGameplay& prepared)
    -> core::Result<std::vector<std::byte>>;

//  The execution session component: the canonical input mapping projection, the late policy and
//  the calibration token, encoded exactly as `writeExecutionSession` does.
[[nodiscard]] auto executionSessionBytes(const InputMappingProfile& mapping,
                                         const LatePolicyParameters& late,
                                         std::string_view calibration) -> std::vector<std::byte>;

//  Convenience bridge: chart projection + execution session + runtime composition.
[[nodiscard]] auto runtimePreparedIdentity(const PreparedGameplay& prepared,
                                           const PreparedIdentityDeclarations& declarations,
                                           const InputMappingProfile& mapping,
                                           const LatePolicyParameters& late,
                                           std::string_view calibration)
    -> core::Result<runtime::PreparedIdentityBytes>;

} // namespace cuexis::judgement::compile

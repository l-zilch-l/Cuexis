#pragma once

//  Judgement compile side - the shared owning products of the beat-us compiler and the prepared
//  artifact Reader.
//
//  Build-only internal root: the running judgement library does not include this header and this
//  header is not installed. `BeatGameplayProduct` is the unterminated, owning compiler result:
//  it carries runtime records, exact beat keys, canonical graph rows and the normalized score, but
//  no identity, no prepared gameplay handle and no wire encoding. The later finalizer (T-13) binds
//  identity and calls the runtime prepare; nothing here links a Reader or Playback target.

#include <cuexis/judgement/normalized_score.hpp>
#include <cuexis/judgement/prepared_gameplay_records.hpp>
#include <cuexis/timing/rational_beat.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace cuexis::judgement {

//  One reference atom of a canonical graph row: the stable reference kind and its owning token.
struct GraphReference final {
    std::uint8_t kind{};
    std::string token;
};

//  One canonical row atom. The concrete alternative is fixed by the corresponding old typed row's
//  traversal; an IEEE bit-pattern field keeps its old atom representation and the token owns the
//  bytes.
using GraphAtom = std::variant<bool, std::int64_t, std::uint64_t, std::string, GraphReference>;

//  One canonical row.
using GraphRow = std::vector<GraphAtom>;

//  The canonical GPH0/GPR0/GPD0/GRC0 row groups. The product keeps every legal declaration,
//  including unused Pattern and Measure definitions.
struct CanonicalGameplayRows final {
    GraphRow gph0;
    std::vector<GraphRow> gpr0;
    std::vector<GraphRow> gpd0Patterns;
    std::vector<GraphRow> gpd0Measures;
    std::vector<GraphRow> gpd0JudgementDomains;
    std::vector<GraphRow> gpd0MergedDeclarations;
    std::vector<GraphRow> grc0Resources;
    std::vector<GraphRow> grc0Relations;
    std::vector<GraphRow> grc0SolverProfiles;
    std::vector<GraphRow> grc0FactBindings;
    std::vector<GraphRow> grc0Claims;
};

//  The exact source beat keys of one requirement, in phase declaration order.
struct CompiledBeatKeys final {
    timing::RationalBeat end{timing::RationalBeat::zero()};
    std::optional<std::pair<timing::RationalBeat, timing::RationalBeat>> body;
    std::vector<std::pair<PhaseKind, timing::RationalBeat>> phaseTargets;
};

//  The unterminated owning result of `compileBeatGameplay`. The runtime records are the shape
//  `prepareCompiledGameplay` consumes; the canonical rows and beat keys are the shape the
//  compiled-artifact Reader produces; the normalized score is already canonical and its basis was
//  derived by the shared runtime helper.
struct BeatGameplayProduct final {
    CanonicalGameplayRows rows;
    std::vector<CompiledBeatKeys> beatKeys;
    runtime::LatePolicyParameters latePolicy;
    std::vector<runtime::PreparedRequirement> requirements;
    std::vector<runtime::PreparedResource> resources;
    std::vector<runtime::PreparedTimerKey> timers;
    PreparedNormalizedScore normalizedScore;
};

} // namespace cuexis::judgement

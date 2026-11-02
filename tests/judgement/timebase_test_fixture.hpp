#pragma once

// Shared timebase fixture for the S7A-2 case units.

#include <catch2/catch_test_macros.hpp>

#include <cuexis/core/error.hpp>
#include <cuexis/judgement/timebase.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace timebase_fixture {

namespace judgement = cuexis::judgement;

using judgement::CanonicalOrdinal;
using judgement::CommitTick;
using judgement::CommitWindow;
using judgement::JudgementTick;
using judgement::LateEventPolicy;
using judgement::LatePolicyParameters;
using judgement::MeasuredParameter;
using judgement::OriginKind;
using judgement::RationalBeat;
using judgement::RationalDuration;
using judgement::Tick;
using judgement::TickCollisionKey;
using judgement::TickSpan;
using judgement::TimebaseProfile;

constexpr std::int64_t kInt64Min = std::numeric_limits<std::int64_t>::min();
constexpr std::int64_t kInt64Max = std::numeric_limits<std::int64_t>::max();

//  -------------------------------------------------------------------------------------------
//  Fixture construction
//  -------------------------------------------------------------------------------------------

inline [[nodiscard]] auto beat(std::int64_t numerator, std::int64_t denominator) -> RationalBeat {
    const auto value = RationalBeat::create(numerator, denominator);
    REQUIRE(value.has_value());
    return *value;
}

inline [[nodiscard]] auto duration(std::int64_t numerator, std::int64_t denominator)
    -> RationalDuration {
    const auto value = RationalDuration::create(numerator, denominator);
    REQUIRE(value.has_value());
    return *value;
}

//  The declared engine.tick.us.v1 binding with one explicit positive tempo and no sections.
inline [[nodiscard]] auto plainProfile(RationalDuration tempo) -> TimebaseProfile {
    TimebaseProfile profile = judgement::makeMicrosecondTimebase();
    profile.initialTempo = tempo;
    return profile;
}

//  All four late-policy parameters measured and a policy declared. The numbers are presence probes.
inline [[nodiscard]] auto measuredPolicy() -> LatePolicyParameters {
    LatePolicyParameters parameters;
    parameters.finalizationWatermark = MeasuredParameter<TickSpan>::measured(TickSpan{0});
    parameters.maxQueueHop = MeasuredParameter<TickSpan>::measured(TickSpan{1});
    parameters.windowCloseThreshold = MeasuredParameter<TickSpan>::measured(TickSpan{2});
    parameters.windowOpenThreshold = MeasuredParameter<TickSpan>::measured(TickSpan{3});
    parameters.policy = LateEventPolicy::rejectLate;
    return parameters;
}

inline [[nodiscard]] auto contextValue(const cuexis::core::Error& error, std::string_view key)
    -> std::string {
    for (const auto& entry : error.context()) {
        if (entry.key == key) {
            return entry.value;
        }
    }
    return {};
}

//  -------------------------------------------------------------------------------------------
//  Mapping helpers
//  -------------------------------------------------------------------------------------------

struct BeatCase final {
    std::int64_t numerator;
    std::int64_t denominator;
    std::int64_t expectedTick;
};

inline void checkMapping(const TimebaseProfile& profile, const std::vector<BeatCase>& cases) {
    for (const BeatCase& item : cases) {
        CAPTURE(item.numerator, item.denominator);
        const auto result =
            judgement::mapBeatToTick(profile, beat(item.numerator, item.denominator));
        REQUIRE(result.has_value());
        CHECK(result->tick().value() == item.expectedTick);
    }
}

//  Pins the negative symmetry of the mapping. The precondition is stated because it is a property
//  of the rate profile as well as of the rounding rule: the mapping is odd about the origin only
//  when the accumulated time is odd about it. A tempo change is not, and a stop is not either under
//  the collapse model (its jump is attached to endBeat, and that half-open choice does not mirror),
//  so those profiles carry their own explicit negative expectations in the table instead of being
//  mirrored here. This helper is therefore only called for a constant tempo at origin beat 0.
inline void checkMirrorSymmetry(const TimebaseProfile& profile,
                                const std::vector<BeatCase>& cases) {
    for (const BeatCase& item : cases) {
        if (item.numerator == 0) {
            continue;
        }
        const auto positive =
            judgement::mapBeatToTick(profile, beat(item.numerator, item.denominator));
        const auto negative =
            judgement::mapBeatToTick(profile, beat(-item.numerator, item.denominator));
        CAPTURE(item.numerator, item.denominator, item.expectedTick);
        REQUIRE(positive.has_value());
        REQUIRE(negative.has_value());
        CHECK(negative->tick().value() == -positive->tick().value());
    }
}

struct KeyCase final {
    std::int64_t tick;
    OriginKind originKind;
    std::uint64_t canonicalOrdinal;
};

inline [[nodiscard]] auto key(const KeyCase& item) -> TickCollisionKey {
    //  The origin kind is a required constructor argument: a key cannot be built by omitting it,
    //  because no member of TickCollisionKey has a default value.
    return TickCollisionKey{Tick{item.tick}, item.originKind,
                            CanonicalOrdinal{item.canonicalOrdinal}};
}

//  Sorts a set of canonical keys by the canonical order alone, whatever arrival order it was given
//  in. This is the property Spec 3.7.2 item 4 protects: the result depends on the keys, never on
//  the sequence the caller happened to hand over.
inline [[nodiscard]] auto canonicalSort(const std::vector<KeyCase>& arrivalOrder)
    -> std::vector<TickCollisionKey> {
    std::vector<TickCollisionKey> keys;
    keys.reserve(arrivalOrder.size());
    for (const KeyCase& item : arrivalOrder) {
        keys.push_back(key(item));
    }
    std::sort(keys.begin(), keys.end(), judgement::canonicalOrder);
    return keys;
}

} // namespace timebase_fixture

using namespace timebase_fixture;

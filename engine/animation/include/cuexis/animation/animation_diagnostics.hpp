#pragma once

// Frozen Stage 4 compile/update diagnostic codes, identity context keys, sort order, and
// truncation sentinel. Format-layer codes remain owned by Chart resolver and Playback
// preflight.

#include <cstddef>
#include <string_view>

namespace cuexis::animation {

inline constexpr std::string_view diagnosticIdentityDuplicate =
    "animation.compile.identity_duplicate";
inline constexpr std::string_view diagnosticClipMissing = "animation.compile.clip_missing";
inline constexpr std::string_view diagnosticClipIdLookupForbidden =
    "animation.compile.clip_id_lookup_forbidden";
inline constexpr std::string_view diagnosticLimitExceeded = "animation.diagnostics.limit_exceeded";

inline constexpr std::string_view sampleClipMissing = "animation.sample.clip_missing";
inline constexpr std::string_view sampleClipIdLookupForbidden =
    "animation.sample.clip_id_lookup_forbidden";
inline constexpr std::string_view sampleDurationInvalid = "animation.sample.duration_invalid";
inline constexpr std::string_view sampleScaleInvalid = "animation.sample.scale_invalid";
inline constexpr std::string_view sampleBeatOverflow = "animation.sample.beat_overflow";
inline constexpr std::string_view sampleValueTypeMismatch = "animation.sample.value_type_mismatch";
inline constexpr std::string_view sampleNonFinite = "animation.sample.non_finite";
inline constexpr std::string_view sampleQuaternionInvalid = "animation.sample.quaternion_invalid";
inline constexpr std::string_view sampleSegmentInvalid = "animation.sample.segment_invalid";

inline constexpr std::string_view mixBindingMissing = "animation.mix.binding_missing";
inline constexpr std::string_view mixBaselineMissing = "animation.mix.baseline_missing";
inline constexpr std::string_view mixGroupOverlap = "animation.mix.group_overlap";
inline constexpr std::string_view mixPriorityOverlap = "animation.mix.priority_overlap";
inline constexpr std::string_view mixAdditiveUnsupported = "animation.mix.additive_unsupported";
inline constexpr std::string_view mixDiscreteWeightUnsupported =
    "animation.mix.discrete_weight_unsupported";
inline constexpr std::string_view mixScaleNonPositive = "animation.mix.scale_non_positive";
inline constexpr std::string_view mixValueTypeMismatch = "animation.mix.value_type_mismatch";
inline constexpr std::string_view mixNonFinite = "animation.mix.non_finite";
inline constexpr std::string_view mixQuaternionInvalid = "animation.mix.quaternion_invalid";

inline constexpr std::string_view contextRecordKind = "record_kind";
inline constexpr std::string_view contextObjectId = "object_id";
inline constexpr std::string_view contextBindingId = "binding_id";
inline constexpr std::string_view contextTemplateId = "template_id";
inline constexpr std::string_view contextChartLocalId = "chart_local_id";
inline constexpr std::string_view contextClipId = "clip_id";
inline constexpr std::string_view contextProperty = "property";

inline constexpr std::string_view fallbackFieldPath = "$/animationProgram";

inline constexpr std::size_t maxDiagnostics{1024};

} // namespace cuexis::animation

#pragma once

//  ChartLimits - default safety budgets for chart parsing and compilation
//  Covers input size, nesting depth, and object/template/behavior count ceilings
//  Callers may supply stricter limits, but must never change format semantics by relaxing
//  the budgets

#include <cstddef>
#include <cstdint>

namespace cuexis::chart {

struct ChartLimits final {
    std::size_t maxInputBytes{16U * 1024U * 1024U};
    std::size_t maxNestingDepth{64};
    std::size_t maxStringBytes{1024U * 1024U};
    std::size_t maxMetadataMembers{1024};
    std::size_t maxTemplates{10000};
    std::size_t maxBehaviors{10000};
    std::size_t maxObjects{100000};
    std::size_t maxPatchesPerTemplate{256};
    std::size_t maxTracksPerBehavior{6};
    std::size_t maxKeysPerTrack{65536};
    std::size_t maxTotalBehaviorKeys{262144};
    std::size_t maxTempoEvents{4096};
    std::size_t maxStops{4096};
    std::size_t maxEventsPerBehavior{65536};
    std::size_t maxTotalBehaviorEvents{262144};
    std::size_t maxExtensions{256};
    std::size_t maxDiagnostics{1024};
    std::size_t maxIdentifierBytes{256};
    std::size_t maxChartParameters{256};
    std::size_t maxAnimationImports{10000};
    std::size_t maxAnimationClips{10000};
    std::size_t maxAnimationTracksPerClip{256};
    std::size_t maxAnimationSegmentsOrStepsPerTrack{65536};
    std::size_t maxTemplateBindingsPerAnimator{256};
    std::size_t maxAnimationLayersPerAnimator{64};
    std::size_t maxBlendGroupsPerLayer{64};
    std::size_t maxClipInstancesPerBlendGroup{256};
    std::size_t maxAnimationMaskEntries{256};
    std::size_t maxAnimationTemplateNameBytes{256};
    std::size_t maxAnimationTracks{65536};
    std::size_t maxAnimationSegmentsAndSteps{1048576};
    std::size_t maxGeneratedAnimationRecords{100000};
    std::size_t maxAnimationTemplateBytes{4U * 1024U * 1024U};
    // Candidate CXT v2 expansion budgets. These are intentionally separate from
    // the legacy CXT v1 animation limits.
    std::size_t maxCxtV2Modules{10000};
    std::size_t maxCxtV2Parameters{256};
    std::size_t maxCxtV2SlotsPerPrototype{256};
    std::size_t maxCxtV2Prototypes{10000};
    std::size_t maxCxtV2Patterns{10000};
    std::size_t maxCxtV2RepeatDepth{16};
    std::size_t maxCxtV2Nodes{100000};
    std::size_t maxCxtV2ExpansionEntities{40000};
    std::size_t maxCxtV2ExpansionRequirements{40000};
    std::int64_t maxBeatNumeratorMagnitude{1000000000000LL};
    std::int64_t maxBeatDenominator{1000000000LL};
};

struct PackedChartLimits final {
    std::size_t maxPackedFileBytes{16U * 1024U * 1024U};
    std::size_t maxPackedDecodedBytes{16U * 1024U * 1024U};
    std::size_t maxPackedSectionBytes{16U * 1024U * 1024U};
    std::size_t maxPackedStrings{100000};
    std::size_t maxPackedReferences{100000};
    std::size_t maxPackedEntities{40000};
    std::size_t maxPackedRequirements{40000};
};

} // namespace cuexis::chart

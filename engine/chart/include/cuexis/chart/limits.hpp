#pragma once

//  ChartLimits — 谱面解析和编译的默认安全预算
//  包括输入大小、嵌套深度、对象/模板/行为数量等上限
//  调用方可以传入更严格的限制，但不得通过放宽预算改变格式语义

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
    std::size_t maxExtensions{256};
    std::size_t maxDiagnostics{1024};
    std::size_t maxIdentifierBytes{256};
    std::size_t maxSimpleBeatBytes{128};
    std::int64_t maxBeatNumeratorMagnitude{1000000000000LL};
    std::int64_t maxBeatDenominator{1000000000LL};
};

} // namespace cuexis::chart

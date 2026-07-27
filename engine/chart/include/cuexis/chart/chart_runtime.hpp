#pragma once

//  ChartRuntime - compiled chart runtime data
//  Stores no entt::entity and does not depend on EnTT; timing is already resolved into a
//  TimingMap
//  RuntimeObject references its parent by index (not by Entity); Behavior is already
//  resolved to type/version
//  ChartCompiler: compiles in stable-ID order, so results are independent of the order of
//  the objects array

#include <cuexis/chart/chart_document.hpp>
#include <cuexis/chart/limits.hpp>
#include <cuexis/chart/timing_map.hpp>

#include <cstddef>
#include <optional>
#include <vector>

namespace cuexis::chart {

struct RuntimeObject final {
    ChartObjectId id;
    std::optional<std::size_t> parentIndex;
    ObjectComponents components;
};

struct RuntimeKey final {
    double chartTimeMs{};
    BehaviorValue value{};
    BehaviorEasing easing{BehaviorEasing::Linear};
};

struct RuntimeTrack final {
    BehaviorProperty property{};
    std::vector<RuntimeKey> keys;
};

struct RuntimeBehavior final {
    BehaviorId id;
    std::string type;
    std::uint32_t version{1};
    std::vector<RuntimeTrack> tracks;
};

struct ChartRuntime final {
    ChartId chartId;
    TimingMap timingMap;
    CameraData camera;
    std::vector<RuntimeBehavior> behaviors;
    std::vector<RuntimeObject> objects;
    std::uint32_t version{1};
    std::optional<AssetId> mainMusic;
};

struct ChartRuntimeResult final {
    std::optional<ChartRuntime> runtime;
    core::Diagnostics diagnostics;

    [[nodiscard]] auto hasValue() const noexcept -> bool {
        return runtime.has_value() && !diagnostics.hasErrors();
    }
};

class ChartCompiler final {
  public:
    [[nodiscard]] static auto compile(const ChartDocument& document, const ChartLimits& limits = {})
        -> ChartRuntimeResult;
};

} // namespace cuexis::chart

#pragma once

//  ChartRuntime — 编译后的谱面运行时数据
//  不保存 entt::entity，不依赖 EnTT；定时已解析为 TimingMap
//  RuntimeObject 使用索引引用父对象（非 Entity），Behavior 已解析为 type/version
//  ChartCompiler: 按稳定 ID 排序编译，结果与 objects 数组顺序无关

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

# ADR 0007：Chart、Runtime 与 World 的边界

日期：2026-07-16

状态：已接受

## 背景

Cuexis 需要把可编辑、可迁移的谱面文档编译成适合播放的数据，并最终创建 EnTT Entity。初步设计同时规定 `cuexis_chart` 只依赖 `cuexis_core`，这与让 Chart 模块直接实例化 EnTT World 相冲突。

Player 和未来的 Studio 预览还需要共享相同的实例化与系统编排逻辑，因此该逻辑不能只放在某个应用中。

## 决策

新增高层引擎模块 `cuexis_runtime`。

```text
cuexis_chart
  负责 ChartDocument、格式校验、版本迁移和 ChartRuntime 编译
  不依赖 EnTT 或 cuexis_world

cuexis_world
  负责 World、通用 Entity 生命周期和基础空间组件
  不依赖 cuexis_chart

cuexis_runtime
  负责 RuntimeSession、ChartWorldInstantiator 和运行时系统编排
  依赖 chart、world 以及实例化所需的引擎前端模块
  不依赖 SDL、OpenGL 或具体渲染后端
```

`ChartRuntime` 是已编译但尚未实例化的数据，不保存 `entt::entity`。`ChartWorldInstantiator` 读取 ChartRuntime，在同一个 World 中创建 Entity，并附加由各功能模块定义的 Component。

Player 和 Studio 预览均使用 `cuexis_runtime`，不得各自实现谱面到 World 的转换路径。

## 备选方案

### 由 cuexis_chart 实例化 World

拒绝。它会引入 `chart -> world/EnTT`，使谱面数据层与运行时实现耦合，也削弱独立校验和格式迁移能力。

### 由 cuexis_world 读取 ChartRuntime

拒绝。World 是通用 ECS 基础设施，不应知道谱面格式或反向依赖 Chart。

### 由 cuexis_gameplay 负责实例化

拒绝。谱面还包含装饰物、灯光和纯视觉元素，把完整实例化过程归入 Gameplay 会造成错误的语义边界。

### 分别由 Player 和 Studio 实现

拒绝。两条转换路径会产生行为差异，也违背 Player 与 Studio 共享引擎运行时的目标。

## 影响

```text
新增 cuexis_runtime CMake target 和 cuexis::runtime 命名空间
ChartRuntime 必须保持与 EnTT 无关
各功能模块拥有自己的 Component 类型
Runtime 是高层组合模块，允许依赖多个引擎前端模块
平台层和具体渲染后端仍由应用组合，不进入 Runtime
```

## 后续风险

`cuexis_runtime` 容易逐渐成为容纳任意逻辑的大模块。新增职责时必须确认它是否确实属于播放或预览会话的组合与生命周期管理；文件解析、资源管理、平台事件、后端渲染和编辑器状态不得迁入 Runtime。

RuntimeSession 的具体所有权、错误恢复和重载流程仍需在实现前进一步设计。

## SDK 转型补充（2026-07-20）

ADR 0027 保留本 ADR 的内部依赖方向，并把 RuntimeSession 定位为 Playback SDK 内部会话实现。Player、Studio 与外部宿主统一通过 PlaybackSession 门面进入；PlaybackSession 可以编排 Project/Chart/Assets/Runtime/Judgement，但不得把 World、EnTT 或后端对象暴露给宿主。

原“Player 和 Studio 均使用 cuexis_runtime”扩展为“Player、Studio 和 external consumer 均使用 cuexis_playback 所封装的唯一 Runtime 路径”。

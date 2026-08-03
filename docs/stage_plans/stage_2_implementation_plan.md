# 阶段 2：Behavior Event 与 Cuexis 表现能力实施计划

状态：Chart v3、Tempo Event 和 Behavior Event 方向已接受（见 `docs/adr/0034-chart-v3-tempo-and-behavior-events.md`）；Step Event 和后续局部 Clip 能力仍待冻结

## 1. 目标与边界

阶段 2 的目标是让 Behavior 表达 Cuexis 谱面表现，并通过 `PlaybackSession` 在任意目标时间确定性采样。它不建设宿主任意代码、脚本、无界回调或通用游戏状态机。

阶段 1C 的 `behavior.transform.keyframe` version 1 保持原有采样语义。阶段 2 的新谱面表达采用 Behavior Event；运行时可以将其编译成内部 Segment 或等价结构，但公共谱面语义以事件为准。

## 2. Behavior Event 核心语义

连续属性事件使用以下字段：

```json
{
  "property": "transform.position.x",
  "startBeat": { "numerator": 16, "denominator": 1 },
  "durationBeats": { "numerator": 4, "denominator": 1 },
  "startValue": 0.0,
  "endValue": 10.0,
  "startSlope": 0.0,
  "endSlope": 0.0
}
```

求值规则：

```text
event 外：保持对象初始基准或前一事件终值
event 开始：应用 startValue，允许与当前基准不同
event 区间：按 Beat 计算归一化进度并插值
event 结束：保持 endValue，直到下一个事件
```

进度函数与 TimingMap 的 Tempo Event 相同，使用 `h(u) = (-2 + m0 + m1)u^3 + (3 - 2m0 - m1)u^2 + m0u` 的端点斜率三次 Hermite 函数。标量和 Vec3 按分量插值；Quaternion 使用 shortest-path slerp，并使用 Hermite 结果作为 slerp 进度。`startSlope` 和 `endSlope` 为归一化进度斜率，必须有限、非负，并满足 `startSlope + endSlope <= 3`。

`durationBeats = 0` 表示瞬时事件，并要求 `startValue == endValue`、`startSlope == 0`、`endSlope == 0`。非零事件使用半开区间 `[startBeat, startBeat + durationBeats)`；无后继事件时结束边界及其后保持终值，相邻事件在同一边界处由后一个事件优先。零持续事件在精确 `startBeat` 处应用其值并建立后续基准。同一属性的事件不得重叠，输入顺序无语义，按 Beat 稳定排序。

负 Beat 事件必须参与 Beat 0 的基准求值。Stop 内 Beat 固定，因此 Behavior Event 的进度也固定；Stop 结束后从同一 Beat 继续采样。

## 3. 离散属性边界

Visibility、Material 选择、ParentBinding 等离散属性不能直接使用连续插值。阶段 2 必须为它们定义受限 `Step Event`，或明确将其延后；不能通过对枚举、布尔或资源引用执行数值插值来隐式定义行为。Step Event 的字段、边界和冲突规则在实现前单独冻结。

## 4. 实施分段

### 2A TimingMap

- 实现 Beat 域 Tempo Event、直接 BPM 插值、零持续事件和负 Beat 语义。
- 实现 Stop 区间、`beatToChartTimeMs` 与逆映射。
- 使用固定边界表和有预算的数值积分，保持单调、有限和可重复。

### 2B Behavior Event 数据与编译

- 新增版本化 Behavior Event reader、schema、diagnostic 和 compiler。
- 将事件按属性分组、排序并检查重叠、类型、范围和预算。
- 保留 v1 Keyframe reader，并提供显式 v1 到 Event 的迁移，不静默改变 v1 结果。
- 实现可选 `groupId` 的多属性同步校验，并在同一事件边界原子提交。

### 2C Runtime 求值

- `chartTimeMs -> BeatSample` 每帧只计算一次，各 BehaviorClip 复用结果。
- 支持绝对采样、Seek、Reload、Stop 和时间不连续。
- 从初始基线重建属性，不使用上一帧最终值作为下一帧基线。
- Runtime 不进行动态分配，不执行脚本或宿主回调。

### 2D 表现属性

- 先实现 Transform 与 Camera 连续属性。
- 冻结并实现 Material、Visibility、ParentBinding 的 Step Event 或记录延期决定。
- FrameSnapshot 只增加宿主需要的最终表现数据，不暴露内部事件/解析器。

### 2E 能力与诊断

- Behavior 类型和版本进入明确的 capability/preflight 检查。
- 区分“格式可解析”与“宿主表现能力支持”。
- 为未支持属性、事件冲突和预算超限提供确定性诊断。

## 5. 验收门禁

- 同一 Chart、同一 Beat/时间输入在 Player、PlaybackSession 和 external consumer 中产生相同结果。
- 覆盖常量区间、连续事件、跳变、相邻事件、零持续事件、负 Beat、Stop、Seek 和 Reload。
- 覆盖标量、Vec3、Quaternion 的有限性、单调性和边界采样。
- v1 Keyframe golden 结果保持不变，迁移结果有独立 golden。
- 恶意或超预算 Chart 不导致无界积分、分配或回调。
- 不支持的离散属性不会被静默近似或数值插值。

## 6. 已冻结的 v3 格式

- Chart 顶层版本为 `3`，v3 使用 `timing.tempoEvents`，不接受 `timing.bpmChanges`。
- 新 Behavior 类型为 `behavior.event` version `1`，事件使用 Chart 全局 Beat。
- 同组连续事件使用可选 `groupId`；同组事件必须具有相同开始 Beat 和持续时间。
- v3 对象暂时沿用 `cuexis.behavior` version `1` 的单 Behavior 绑定结构。

## 7. 待冻结事项

- 数值积分精度、误差预算和极端输入的固定分段预算。
- Step Event 的完整字段、资源引用和边界语义。
- BehaviorClip 的局部 Beat、循环边界和对象绑定结构。

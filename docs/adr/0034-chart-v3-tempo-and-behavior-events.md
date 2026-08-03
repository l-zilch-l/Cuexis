# ADR 0034：Chart v3 Tempo Event 与 Behavior Event

日期：2026-08-03

状态：已接受；代码和 Schema 待实现

## 背景

阶段 1 的 Chart v1/v2 使用 `bpmChanges` 和 `behavior.transform.keyframe`。这种格式适合建立最小闭环，但无法直接表达“只有事件发生时属性才改变”的谱面语义，也无法让 BPM 和 Behavior 共享同一套 Beat 事件边界。

## 决策

Chart v3 保留 v2 的音频和对象结构，但作出以下格式变更：

```text
timing.bpmChanges -> timing.tempoEvents
behavior.transform.keyframe -> behavior.event
```

### Tempo Event

每个事件包含：

```text
startBeat, durationBeats, startBpm, endBpm, startSlope, endSlope
```

BPM 在事件区间内直接插值，不对 milliseconds-per-beat 插值。BPM 范围为 `[1, 65536]`。斜率必须非负且 `startSlope + endSlope <= 3`。零持续事件要求起止 BPM 相同且两个斜率为零；负 Beat 事件参与 Beat 0 基准；Stop 内 Beat 和事件进度均冻结。完整字段见 `docs/CHART_FORMAT.md` 第 2b 节。

### Behavior Event

连续属性事件包含：

```text
property, startBeat, durationBeats, startValue, endValue, startSlope, endSlope
```

事件外保持对象初始基准或前一事件终值，事件开始时应用 `startValue`，区间内使用与 Tempo Event 相同的 Hermite 进度，结束后保持 `endValue`。支持 Transform 和 Camera 连续属性；可选 `groupId` 用于多个属性在同一边界原子生效。Visibility、Material 和 ParentBinding 不得使用连续数值插值，必须由后续 Step Event 规则定义。

v3 Behavior 使用 Chart 全局 Beat，暂时沿用 `cuexis.behavior` version 1 的单 Behavior 绑定。局部 Clip Beat、循环和多 Clip 混合需要新的绑定版本，不得改变 v3 语义。

## 兼容与迁移

v1/v2 loader 的字段和采样结果保持不变。v1/v2 不得伪装成 v3；迁移必须显式执行并保留诊断。`behavior.transform.keyframe` v1 的结果不能通过静默替换为 Event 改变。

## 拒绝的方案

- 继续扩展 `bpmChanges`：字段无法表达持续 BPM 曲线和端点斜率。
- 直接把 v1 Keyframe 改名为 Event：会破坏旧 Chart 的边界和迁移可追溯性。
- 将上一帧属性作为事件基准：结果依赖帧率和更新历史，Seek 不再确定。
- 用数值插值表达 Visibility、Material 选择或 ParentBinding：离散属性没有定义良好的连续插值。

## 影响

Chart Reader、Schema、Compiler、TimingMap、BehaviorSystem、迁移工具和 external consumer fixtures 都需要增加 v3 路由和 golden 测试。v1/v2 fixtures 继续作为回归输入；v3 代码未实现前，loader 必须明确报告版本不支持，而不能降级解释。

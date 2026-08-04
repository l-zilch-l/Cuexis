# Cuexis TimingMap 规范

状态：v1/v2 固定 BPM/offset 已实现且非空旧事件仍被拒绝；v3 Tempo Event/Stop 语义已接受，代码待实现

更新日期：2026-08-04

## 时间域

```text
sourceTimeMs ChartClock、HostClock 或 Cuexis AudioClock 提供的主时间位置
audioTimeMs  Cuexis AudioClock 中从音频首采样开始的位置，是 sourceTimeMs 的一种来源
offsetMs     Beat 0 在音频中的位置
chartTimeMs  相对 Beat 0 的谱面时间
beat         规范有理数拍数
```

符号约定：

```text
chartTimeMs = sourceTimeMs - offsetMs
```

正 `offsetMs` 表示 Beat 0 出现在音频开始之后；负值表示 Beat 0 位于音频首采样之前。

TimingMap 负责 `beat <-> chartTimeMs`。`cuexis_audio` 使用后端无关的
`SourceClockSample{positionMs, state, discontinuityId}` 表达源时钟；`cuexis_playback` 的
`RuntimeTimeline` 负责把 SourceClockSample、offset 和播放状态组合为 RuntimeFrame。
PlaybackSession 仍只消费已经明确来源和 discontinuity 的 RuntimeFrame，不读取宿主墙钟，
也不创建或控制 AudioTransport。

单次准备显式选择 ChartClock、HostClock 或 CuexisAudio。ChartClock 只用于没有
`audio.mainMusic` 的 Chart；HostClock 与 CuexisAudio 只用于声明了主音乐的 Chart。活动
Session 与 reload 不得切换模式，初始化失败不得静默 fallback。

## 输入与判定时间

宿主采集的原始输入必须先规范化为带单调事件时间、arrival time、source 和 sequence 的 InputEvent，再映射到同一 `chartTimeMs` 域。判定使用事件发生时间，不使用处理事件的渲染帧时间。

`arrivalTime` 和记录时的 `frameIndex` 只用于诊断、延迟分析和来源审计，不参与判定窗口或回放调度。相同事件时间的稳定顺序由 sequence 和阶段 11 冻结的明确冲突规则决定。

startRecording 记录的 InputEvent 时间戳使用 chartTimeMs 域，不使用宿主源时间或挂钟时间；loadReplay 注入的事件时间戳在同一 chartTimeMs 域匹配。

HostClock 与 CuexisAudio 模式对相同的规范化 SourceClockSample/control script、InputEvent
序列和 ResolvedSessionConfig 必须生成相同 RuntimeFrame，并产生相同表现、判定、计分和回放
结果。输出延迟、输入延迟、用户校准和谱面 offset 是不同来源，不得合并成无来源的单一常数。

## BPM

v1/v2 结构保留 `bpmChanges`/`stops` 字段，但当前只接受空数组，不定义非空旧事件的运行语义。v3 使用直接描述 BPM 曲线的 `tempoEvents`：

```json
{
  "startBeat": { "numerator": 16, "denominator": 1 },
  "durationBeats": { "numerator": 4, "denominator": 1 },
  "startBpm": 120.0,
  "endBpm": 180.0,
  "startSlope": 0.0,
  "endSlope": 0.0
}
```

`defaultBpm`、`startBpm` 和 `endBpm` 必须位于 `[1.0, 65536.0]`，`durationBeats` 必须非负。BPM 在事件区间内直接插值，不对 milliseconds-per-beat 插值。归一化进度使用端点斜率的三次 Hermite 函数：`h(u) = (-2 + m0 + m1)u^3 + (3 - 2m0 - m1)u^2 + m0u`，其中 `m0=startSlope`、`m1=endSlope`。端点斜率必须有限、非负，并满足 `m0 + m1 <= 3`。`durationBeats = 0` 合法，但起止 BPM 必须相同且两个斜率都为零。

最早事件前使用 `defaultBpm`。非零事件的有效区间为 `[startBeat, startBeat + durationBeats)`；事件开始时应用 `startBpm`，允许它与此前有效 BPM 不同并产生跳变；结束后保持 `endBpm`，相邻事件在同一边界由后一个事件接管。零持续事件也可以把此前基准跳变为其相等的起止值。Tempo Event 不得重叠，同一 Beat 的多个事件也是错误。零持续事件在冲突检测中占用其 Beat，不能位于非零事件内部；前一事件结束边界允许零事件，但同 Beat 不能再开始另一事件。事件输入顺序无语义，编译器按 Beat 排序并预计算分段累计时间。负 Beat 事件参与 Beat 0 的实际 BPM 基准。

## Stop

```json
{
  "beat": { "numerator": 32, "denominator": 1 },
  "durationMs": 250.0
}
```

Stop duration 必须为有限正数。同一 Beat 多个 Stop 是错误。

到达 Stop Beat 时先得到该 Beat 的属性采样值，然后 Beat 在整个 Stop duration 内保持不变；Stop 结束后继续按该 Beat 生效的 BPM 前进。Tempo Event 与 Stop 位于同一 Beat 时，新 BPM 在 Stop 结束后的区间生效。

`beatToChartTimeMs(B)` 返回到达 Beat B、Stop 开始前的时刻。B 之后的 Beat 累计 Stop duration。

逆映射在 Stop 区间返回：

```text
beat = Stop Beat
inStop = true
stopProgress = [0, 1)
```

Stop 使用半开时间区间；精确结束边界返回同一 Beat、`inStop=false`，不返回 progress 1。因此 Behavior 在 Stop 中保持相同 Beat 采样，不发生隐式移动。

## 数值与确定性

Beat 使用规范有理数并进行溢出检查。分段累计时间使用 double 毫秒，但不通过逐帧累加建立映射。相同 Timing 数据必须产生相同分段顺序和边界结果。

负 Beat 合法，按 defaultBpm 和负区间内的 Tempo Event 向 Beat 0 积分。

## 播放倍率

TimingMap 不包含 speedChanges。Entity 运动由 Behavior 控制；未来整曲播放倍率改变 AudioTransport/Timeline 的推进速度，但不修改谱面 Beat 或 Timing 事件。

## 阶段实现

阶段 1 实现 defaultBpm 与 offset，并拒绝非空 BPM Changes/Stops。阶段 2 启用 v3 Tempo Event、Stops 和 TimingMap 逆映射；正式实现前必须按本文语义补齐边界和数值测试。v3 的字段权威是 `docs/CHART_FORMAT.md` 第 2b 节。

# ADR 0018：TimingMap 的 BPM、Stop 与 Offset 语义

日期：2026-07-17；v3 补充：2026-08-03；边界复核：2026-08-04

状态：已接受

## 背景

Beat、音频位置、谱面 offset、BPM Change 和 Stop 若没有精确定义，会在 Player、Studio 和判定系统中产生不同边界结果。

## 决策

`chartTimeMs = audioTimeMs - offsetMs`。TimingMap 映射有理数 Beat 与 chartTimeMs。v1/v2 只实现固定 `defaultBpm` 与 offset，并继续拒绝非空 `bpmChanges`/`stops`；不得为兼容目的给这些旧字段补充新的运行语义。v3 使用 Tempo Event，在事件区间内直接插值 BPM，并启用 Stop。Stop 在指定 Beat 冻结 Beat，结束后使用该 Beat 生效的 BPM。

Tempo Event/Stop 输入顺序无语义；同 Beat 重复 Tempo Event 或 Stop 是错误。Stop 逆映射返回固定 Beat、inStop 和 stopProgress；Stop 内 progress 为 `[0, 1)`，结束边界返回 `inStop=false`。Tempo Event 的字段、零持续和负 Beat 规则见 `docs/CHART_FORMAT.md`。

TimingMap 不包含 speedChanges。

## 备选方案

使用帧增量累计会产生漂移；在 Stop 中继续插值 Beat 会破坏谱面语义，因此不采用。

## 影响

Player、Studio、Behavior 和未来 Judge 使用同一 TimingMap。边界、负 Beat 和逆映射需要单元测试。

## 后续风险

极端 BPM、长谱面和大分母 Beat 需要数值上限与溢出保护。

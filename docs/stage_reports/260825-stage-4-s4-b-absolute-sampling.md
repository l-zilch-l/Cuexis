# Stage 4 S4-B Absolute Sampling Report

状态：S4-B local checkpoint；Stage 4 仍为 unblocked / not started 的整体阶段，默认 Playback 未放开动画

快照日期：2026-08-25

权威范围：[Stage 4 实施计划](../stage_plans/stage_4_implementation_plan.md) 与
[CFU-G2 Stage 4 typed handoff](260816-chart-format-update-g2-stage4-handoff.md)。

## 交付

S4-B 把 compiled program 做成可重复 seek 的局部 Beat 采样。不实现 Layer 混合、PropertyResolver、
Runtime 编排或 Playback capability。

```text
AnimationSampler resolves local Beat from an explicit chart Beat
startBeat, durationScale, finite/infinite iterations, none/hold fill
continuous Hermite segments and step Tracks sample from absolute local Beat
Seek, Stop, and frame-rate arrival rebuild from the target Beat
instance lookup uses AnimationRecordIdentity; generated clip.id is not a global key
```

`AnimationSystem::sample()` 只按 compiled object/layer/group/instance 顺序输出每条 Instance 的
采样结果。Layer/BlendGroup weight、Override/Additive 和 PropertyWrite 仍属于 S4-C。

## 采样合同

Runtime 必须提供绝对 Chart Beat。animation 模块不读取 Chart、CXT 或 TimingMap。

```text
elapsed = chartBeat - startBeat
elapsed < 0: no write
localElapsed = elapsed / durationScale
iterationIndex = floor(localElapsed / durationBeats)
internal iteration end: localBeat = 0 of the next iteration
finite final boundary + none: no write
finite final boundary + hold: sample localBeat = durationBeats
infinite: keep wrapping; fillMode remains none
```

连续 Track 复用 Behavior Event 的 Hermite progress、零持续、间隙保持和 Quaternion shortest-path
slerp。首 Segment 前不写入；间隙保持前一 Segment endValue。Step Track 在首 Step 前不写入，之后
保持最后值。

冻结的 runtime sample diagnostics：

```text
animation.sample.clip_missing
animation.sample.clip_id_lookup_forbidden
animation.sample.duration_invalid
animation.sample.scale_invalid
animation.sample.beat_overflow
animation.sample.value_type_mismatch
animation.sample.non_finite
animation.sample.quaternion_invalid
animation.sample.segment_invalid
```

## 明确未改

- 默认 Playback capability 集合
- 非空动画 `playback.capability.unsupported` 拒绝路径
- FrameSnapshot / FrameDigest v3
- RuntimeSession 求值顺序
- Layer/BlendGroup mixing 与 PropertyResolver
- `cuexis_animation` 不进入安装公共组件

## 本地验证

2026-08-25 MSVC Debug：

```text
cuexis_format_check
cuexis_architecture_tests
cuexis_animation_tests.exe  11 cases / 2282 assertions
cuexis_chart_tests.exe [beat]  4 cases / 33 assertions
python -B tools/check_docs.py
```

S4-B 未把 `cuexis.animation.clip.v1` / `layers.v1` 加入默认 capability。合法生产 fixture 仍只通过
Chart resolver 进入；animation 测试继续使用手工 typed program。

## 下一步

S4-C：按 Mixing 合同实现 Override / Additive Layer 混合。S4-B 关闭不表示 v4 动画 Playback 可用。

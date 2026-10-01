# Stage 4 S4-E Playback Ownership Report

状态：S4-E local checkpoint；Stage 4 仍为 unblocked / not started 的整体阶段，默认 Playback 未放开动画

后续关闭证据：[Cuexis 阶段 4 验收报告](completion.md)。

快照日期：2026-08-27

权威范围：[Stage 4 实施计划](../../../stage_plans/completed/stage-04/plan.md) 与
[CFU-G2 Stage 4 typed handoff](../../chart-format-update/2026-08-16-g2-stage4-handoff.md)。

## 交付

S4-E 让 Playback prepare candidate 在 capability preflight 之后编译并拥有 animation state，commit
原子替换 active Runtime。不把 animation capability 加入默认支持集合。

```text
AnimationCompiler runs after Playback capability preflight
PreparedPlayback RuntimeSession owns compiled AnimationProgram
commit moves the owning RuntimeSession; failed prepare/reload keeps active state
empty Chart v4 programs continue the static path
opt-in PlaybackCapabilitySet evaluates chart_v4_animation.json, CXT Binding, template animator
PreparedSemanticIdentity is assembled from the same chart/CXT/resource/parameter inputs
```

默认 Session 仍以 `playback.capability.unsupported` 拒绝非空动画。公开头不增加 animation
capability 常量。

## 事务合同

candidate 编译结果是 owning `AnimationProgram`，不借用 `ChartV4ResolvedArtifact` 或
`PreparedPlayback` 输入缓冲。commit 只移动已提交的 RuntimeSession。失败 prepare/reload 不改变
active Runtime、identity、diagnostics、frame 或 animation state。

需要求值的测试构造显式 `PlaybackCapabilitySet`，包含既有默认 capability 与
`cuexis.animation.clip.v1` / `cuexis.animation.layers.v1`。

## 明确未改

- 默认 Playback capability 集合
- 非空动画默认拒绝路径
- FrameSnapshot / FrameDigest v3
- 公开 HostOverride API
- `cuexis_animation` 不进入安装公共组件

## 本地验证

2026-08-27 MSVC Debug：

```text
cuexis_format_check
cuexis_architecture_tests
cuexis_playback_tests.exe [s4-e]  6 cases / 91 assertions
cuexis_playback_tests.exe [v4]   12 cases / 187 assertions
cuexis_playback_tests.exe        58 cases / 9201 assertions
python -B tools/check_docs.py
git diff --check
```

S4-E 未把 `cuexis.animation.clip.v1` / `layers.v1` 加入默认 capability。合法生产 fixture 仍只通过
Chart resolver 进入；Playback 测试通过 public prepare 路径加载。

## 下一步

S4-F：consumer、确定性与公开 capability。S4-E 关闭不表示默认 Session 可播放非空动画。

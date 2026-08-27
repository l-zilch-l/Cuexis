# Stage 4 S4-F Consumer, Determinism, and Public Capability Report

状态：S4-F local checkpoint；Stage 4 仍为 unblocked / not started 的整体阶段

后续关闭证据：[Cuexis 阶段 4 验收报告](stage_4_completion_report.md)。

快照日期：2026-08-27

权威范围：[Stage 4 实施计划](../stage_plans/stage_4_implementation_plan.md) 与
[CFU-G2 Stage 4 typed handoff](260816-chart-format-update-g2-stage4-handoff.md)。

## 交付

S4-F 把已冻结的 animation capability 写入 Playback 公共头和默认支持集合，并复用 CFU-F
consumer 路径观察 FrameDigest v3。

```text
capabilityAnimationClipV1 = cuexis.animation.clip.v1
capabilityAnimationLayersV1 = cuexis.animation.layers.v1
allCapabilities() includes both names; capability set version remains 1
default PlaybackSession accepts nonempty legal Chart v4 / CXT animation
trimmed PlaybackCapabilitySet without those names still rejects with
playback.capability.unsupported
filesystem / CXC file / CXC memory / typed project-document share identity and digest
FrameDigest stays v3; SDK API stays 0.6.0
```

Player 继续构造默认 `PlaybackSession`，因此继承公开 capability。本地 S4-F 不跑 GPU smoke；
hosted Player 观察留在 S4-H。

## 确定性合同

G2 合法 fixture `source_project`、`cxc_v1_v4_cxt.cxc` 与 typed CXT documents 在 Seek、
time discontinuity 和不同 `simulationDeltaTimeMs` 下重建同一 FrameSnapshot / FrameDigest v3。
对象顺序按 ObjectId 稳定，不依赖 source 数组 permutation。失败 reload 保持 active identity
与 frame。非法 format fixture 仍在 Reader/resolver 失败，不进入 `engine/animation/`。

CFU-F3 `cfu_f3_determinism.txt` 保持不变：headless / external 对非空动画 CXC 的拒绝路径改用
裁剪 Session，因此 diagnostic signature 与 stop FrameDigest golden 不改写。

## 明确未改

- FrameDigest v3 字段与算法版本
- SDK API `0.6.0`
- 公共 CXC package API
- HostOverride 公共头
- `cuexis_animation` 安装组件
- Stage 4 整体 completed 状态

## 本地验证

2026-08-27 MSVC Debug：

```text
cuexis_format_check
cuexis_architecture_tests
cuexis_playback_tests.exe [s4-f]  10 cases / 142 assertions
cuexis_playback_tests.exe [v4]   16 cases / 241 assertions
cuexis_playback_tests.exe        62 cases / 9255 assertions
cuexis_cfu_f_headless_reference
cuexis_cfu_f3_determinism
python -B tools/check_docs.py
git diff --check
```

CXT 跨 source FrameDigest v3 golden：

```text
animation_identity=fb662e259e2146cf68d6ebc763514b3bc2b460f2a865ece6805bda844586e9b4
animation_digests=105060921077611920,10690198800679353609,18438846932740715847,18147874964077530090
```

CFU-F3 `cfu_f3_determinism.txt` 未改写。Player 使用默认 `PlaybackSession`，本地未跑 GPU smoke。

S4-F 未把 Stage 4 标为 completed，也不是完整 v4 动画 Playback。

## 下一步

S4-G：安全、分配与性能。S4-F 关闭不表示 Stage 4 已完成。

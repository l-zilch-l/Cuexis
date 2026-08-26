# Stage 4 S4-D PropertyResolver Report

状态：S4-D local checkpoint；Stage 4 仍为 unblocked / not started 的整体阶段，默认 Playback 未放开动画

快照日期：2026-08-27

权威范围：[Stage 4 实施计划](../stage_plans/stage_4_implementation_plan.md)、
[ANIMATION_MIXING.md](../formats/ANIMATION_MIXING.md)、
[ADR 0009](../adr/0009-property-evaluation-and-conflicts.md)、
[ADR 0019](../adr/0019-animation-layers-and-runtime-overrides.md) 与
[CFU-G2 Stage 4 typed handoff](260816-chart-format-update-g2-stage4-handoff.md)。

## 交付

S4-D 把 Behavior、Animation、HostOverride 和 StudioPreviewOverride 收敛到统一 PropertyResolver，
并在 RuntimeSession 主线程安全点接入 Override Token 与 BasePropertyCommand。不实现 Playback
事务所有权，也不把 animation capability 加入默认支持集合。

```text
PropertyResolver layers: Initial -> Behavior -> Animation -> HostOverride -> StudioPreviewOverride
RuntimeSession::updatePrepared evaluates AnimationSystem after Behavior and before commit
OverrideToken ownerId, priority, propertyMask, lifetime, writes
same-priority overlap discards the conflict write and keeps the lower layer
BasePropertyCommand mutates Initial, increments baseRevision, then reevaluates
debug snapshot records Initial/Behavior/Animation layers/overrides, weight, mask, conflict
```

公开 HostOverride API 仍只存在于内部 RuntimeSession，使用稳定 `ObjectId`、`PropertyId` 和
`OverrideTokenId`。该快照不得进入安装公共头，也不得泄漏 World/EnTT。

## 求值合同

每帧从 captured Initial 重新求值。Behavior 与 Animation 只写入 `PropertyWriteBuffer`。Camera 与
Appearance 由 Resolver 求值、Runtime 提交，World 不依赖 render 头。

`BasePropertyCommand` 只能在 RuntimeSession owner thread 且非 World callback 的安全点应用。命令
校验新值、更新 Behavior Event 基线与 Camera/Appearance 基线，然后从新 Initial 完整求值。失败
不递增 `baseRevision`。

内部 debug snapshot 在显式启用后有界可截断。Animation Layer 贡献包含 identity、priority、weight
和 mask；FrameSnapshot / FrameDigest v3 仍只观察最终表现。

## 明确未改

- 默认 Playback capability 集合
- 非空动画 `playback.capability.unsupported` 拒绝路径
- FrameSnapshot / FrameDigest v3
- Playback prepare/commit 对 compiled animation state 的事务所有权（S4-E）
- `cuexis_animation` 不进入安装公共组件

## 本地验证

2026-08-27 MSVC Debug：

```text
cuexis_format_check
cuexis_architecture_tests
cuexis_world_tests.exe [s4-d]     6 cases / 57 assertions
cuexis_runtime_tests.exe [s4-d]  10 cases / 197 assertions
cuexis_world_tests.exe           18 cases / 172 assertions
cuexis_runtime_tests.exe         36 cases / 955 assertions
cuexis_animation_tests.exe       24 cases / 2546 assertions
python -B tools/check_docs.py
git diff --check
```

S4-D 未把 `cuexis.animation.clip.v1` / `layers.v1` 加入默认 capability。合法生产 fixture 仍只通过
Chart resolver 进入；animation 测试继续使用手工 typed program。

## 下一步

S4-E：Playback 事务所有权。S4-D 关闭不表示 v4 动画 Playback 可用。

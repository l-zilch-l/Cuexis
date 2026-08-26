# Stage 4 S4-C Layer Mixing Report

状态：S4-C local checkpoint；Stage 4 仍为 unblocked / not started 的整体阶段，默认 Playback 未放开动画

快照日期：2026-08-26

权威范围：[Stage 4 实施计划](../stage_plans/stage_4_implementation_plan.md)、
[ANIMATION_MIXING.md](../formats/ANIMATION_MIXING.md) 与
[CFU-G2 Stage 4 typed handoff](260816-chart-format-update-g2-stage4-handoff.md)。

## 交付

S4-C 按 Mixing 合同把 sampled clips 混成 `PropertyWriteBuffer` 写入。不实现 PropertyResolver、
HostOverride、Runtime 编排或 Playback capability。

```text
AnimationMixer evaluates Override then Additive groups per layer
linear two-step lerp; quaternion shortest-path slerp
discrete winner is max instance weight, then minimum instance identity
additive position delta, positive scale product, rotation tangent exp
zero Layer/Group weight does not write
same-priority and same-layer group overlap discards the conflict write
quaternion hemisphere and Additive permutation golden are insertion-order independent
```

`AnimationSystem::evaluate()` 只把 caller 提供的 object binding 和 baseline 交给 mixer。Runtime
仍未编排 AnimationSystem；写入仍不进入 FrameSnapshot。

## 混合合同

Layer 按 priority 升序应用。同一 Layer 内先 Override，再 Additive。Group/Layer weight 为 0 时
该层级不写入。Additive `effectiveWeight = instanceWeight * groupWeight * layerWeight`，不再二次
Layer 混合。

冻结的 runtime mix diagnostics：

```text
animation.mix.binding_missing
animation.mix.baseline_missing
animation.mix.group_overlap
animation.mix.priority_overlap
animation.mix.additive_unsupported
animation.mix.discrete_weight_unsupported
animation.mix.scale_non_positive
animation.mix.value_type_mismatch
animation.mix.non_finite
animation.mix.quaternion_invalid
```

缺失 entity binding 或 layer input baseline 是硬错误。非法重叠、离散部分 weight 和未定义
Additive 属性丢弃该属性写入并报告诊断，不按数组顺序破平。

## 明确未改

- 默认 Playback capability 集合
- 非空动画 `playback.capability.unsupported` 拒绝路径
- FrameSnapshot / FrameDigest v3
- RuntimeSession 求值顺序
- HostOverride / StudioPreviewOverride / 统一 PropertyResolver
- `cuexis_animation` 不进入安装公共组件

## 本地验证

2026-08-26 MSVC Debug：

```text
cuexis_format_check
cuexis_architecture_tests
cuexis_animation_tests.exe  24 cases / 2546 assertions
python -B tools/check_docs.py
```

S4-C 未把 `cuexis.animation.clip.v1` / `layers.v1` 加入默认 capability。animation 测试继续使用
手工 typed program，不在 `engine/animation/` 解析 JSON/CXC/CXT。

## 下一步

S4-D：统一 PropertyResolver、Override Token 与 Runtime 编排。S4-C 关闭不表示 v4 动画 Playback
可用。

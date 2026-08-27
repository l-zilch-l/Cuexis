# Stage 4 S4-A Module Wiring Report

状态：S4-A local checkpoint；Stage 4 仍为 unblocked / not started 的整体阶段，默认 Playback 未放开动画

后续关闭证据：[Cuexis 阶段 4 验收报告](stage_4_completion_report.md)。

快照日期：2026-08-25

权威范围：[Stage 4 实施计划](../stage_plans/stage_4_implementation_plan.md) 与
[CFU-G2 Stage 4 typed handoff](260816-chart-format-update-g2-stage4-handoff.md)。

## 交付

S4-A 只接线模块并冻结 compile 合同，不实现采样、混合、PropertyResolver 或 Playback capability。

```text
cuexis_animation STATIC library registered in engine/ and CUEXIS_ACTIVE_TARGETS
cuexis_animation_tests Catch2 target and dependency allowlist
AnimationProgramInput extracted to chart/animation_program_input.hpp
AnimationCompiler compile API and owning AnimationProgram
AnimationRecordIdentity lookup; AnimationClip::id is not a global key
frozen compile diagnostic codes, identity context, sort, and truncation sentinel
architecture tests forbid JSON, CXC, CXT source, Playback, SDL, and OpenGL in engine/animation/
```

`ChartV4Resolver::resolve()` 仍返回同一 `AnimationProgramInput`。animation 模块只 include typed
input 头，不 include resolver、CXT document 或 Chart v4 loader。

## Compile 合同

输入是 owning `chart::AnimationProgramInput`。输出是 owning `animation::AnimationProgram`，不借用
resolver artifact、JSON 文本或 CXC package。

lookup 只接受 `AnimationRecordIdentity`。两个 generated Clip 可以合法共用同一 `clip.id`。用
embedded `clip.id` 字符串查找必须失败。

冻结的 runtime compile diagnostics：

```text
animation.compile.identity_duplicate
animation.compile.clip_missing
animation.compile.clip_id_lookup_forbidden
animation.diagnostics.limit_exceeded
```

排序仍是 `(fieldPath, severity, code, message)`。有界容量默认 1,024；超出后用 sentinel 替换最后
一个已接受项。格式层 code 仍由 resolver / Playback preflight 拥有。

## 明确未改

- 默认 Playback capability 集合
- 非空动画 `playback.capability.unsupported` 拒绝路径
- FrameSnapshot / FrameDigest v3
- RuntimeSession 求值顺序
- `cuexis_animation` 不进入安装公共组件，也不进入 `CUEXIS_STATIC_IMPLEMENTATION_TARGETS`

## 本地验证

2026-08-25 MSVC Debug：

```text
cuexis_format_check
cuexis_architecture_tests
cuexis_animation_tests.exe  5 cases / 2103 assertions
Playback resolves empty and parameterized Chart v4 into the existing Runtime
Playback preserves parameter tags and reports resolver failures before capability
python -B tools/check_docs.py
```

非空动画 Playback 拒绝路径仍由 `playback_v4_prepare_tests` 覆盖；S4-A 未把
`cuexis.animation.clip.v1` / `layers.v1` 加入默认 capability。

## 下一步

S4-B：把 compiled program 做成可重复 seek 的局部 Beat 采样。S4-A 关闭不表示 v4 动画 Playback 可用。

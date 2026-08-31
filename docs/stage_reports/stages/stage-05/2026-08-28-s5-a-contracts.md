# Stage 5 S5-A 合同冻结

状态：S5-A contract freeze written；生产代码未改；S5-B 尚未开始

报告日期：2026-08-28

权威计划：[Stage 5 实施计划](../../../stage_plans/completed/stage-05/plan.md) §7.1。
权威合同：[ADR 0040](../../../adr/0040-stage-5-material-shader-contracts.md) 与
[MATERIAL_SHADER.md](../../../formats/MATERIAL_SHADER.md)。

## 1. 结论

S5-A 已冻结 Stage 5 Material/Shader v1 的 payload、Asset Index v3、观察面、capability、预算、
诊断码、模块拓扑和 SDK/digest 决策。本批没有修改 public header、C++、CMake target、Schema、
fixture 或测试。

进入 S5-B 仍须按计划改可选编译器接线，且不得提前公开
`cuexis.shader.asset.v1` / `cuexis.material.parameterized.v1`。

## 2. 冻结摘要

| 项 | 冻结值 |
| --- | --- |
| 源 envelope | 扩展 `CXPRES01`；Shader=4，ParameterizedMaterial=5 |
| Unlit | kind 3 不变；禁止投影为/自 ParameterizedMaterial |
| 派生缓存 | 独立 `CXSCCH01`；非源资产；不进 FrameDigest |
| Asset Index | version 3 增加 `shader`；`material` 仍是 Chart 引用类型 |
| 模块 | 可选内部 `cuexis_shader`；Playback 不链接编译器 |
| vcpkg | 可选 feature `shader-tools`；S5-B 才加入 port |
| Playback capability | `cuexis.shader.asset.v1`、`cuexis.material.parameterized.v1`；S5-G 前不进默认集合 |
| Presentation | capabilities/request version 2 为 additive |
| SDK API | 公开类型落地时 `0.6.0` → `0.7.0` |
| FrameDigest | 保持 v3 |
| ProjectConfig | 保持 v1；profile ID 写在 ShaderAsset 与缓存键 |
| GLSL subset | 450 vertex+fragment；禁止 `#include`；最多 4 keyword |
| 热重载 | 仅 Player Worker + Render safe point |
| Studio / Judgement / 公共 CXC API | 不包含 |

字段布局、预算数字和诊断码不在本报告复制。

## 3. 文档交付

| 文档 | 角色 |
| --- | --- |
| [ADR 0040](../../../adr/0040-stage-5-material-shader-contracts.md) | 决策 |
| [MATERIAL_SHADER.md](../../../formats/MATERIAL_SHADER.md) | 字段/语义/预算/诊断 |
| [Stage 5 plan](../../../stage_plans/completed/stage-05/plan.md) | S5-B 至 S5-H 已按冻结合同细化 |
| [SHADER_PIPELINE.md](../../../proposals/deferred/SHADER_PIPELINE.md) | 不再作为字段权威 |

## 4. 明确未做

- 未改 `presentation.hpp`、CMake allowlist、`vcpkg.json` 或 OpenGL 内联 Unlit shader
- 未授权 S5-B 生产代码
- 未把 Shader 能力写入默认 Playback Session
- 未声称 owner 已关闭整个 Stage 5

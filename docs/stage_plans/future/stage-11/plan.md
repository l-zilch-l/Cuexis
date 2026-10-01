# Stage 11 Implementation Plan: Presentation Geometry and Chart v7

状态：future；未开始

更新日期：2026-10-01

归档来源：[Chart v7 历史设计输入](../../historical-inputs/chart-format-update-for-v7/plan.md)、
[Chart v6 / Model v1](../../historical-inputs/chart-format-update-for-v6/plan.md)、
[Portable Presentation v1](../../../formats/PORTABLE_PRESENTATION.md) 和
[Material/Shader v1](../../../formats/MATERIAL_SHADER.md)。

前置：

```text
Stage 8 Chart v5 production baseline
Stage 9 Chart v6 / Model v1 / Presentation Foundation
Stage 10 Studio MVP only where preview or authoring support is required
```

本阶段吸收原 `chart-format-update-for-v7` 延期计划。它建立 Chart v7 的几何表现合同，
不实现用户后处理绘制，不改变 Judgement、Score 或 Replay。

## 1. 阶段目标

```text
Chart v7
  -> 单轴分段三次 Bézier 曲线形变
  -> 内置 line 与更多常见模型
  -> line 粗细/棱柱表现
  -> CXC 根 shader.json 的声明式接口
```

## 2. 工作批次

### S11-A：单轴曲线形变

- `mappingAxes` 只允许一条 X/Y/Z 轴；两轴留给 Stage 13，三轴永久拒绝。
- 只使用分段三次 Bézier；不支持公式、表达式字符串或运行时生成曲线。
- 采样由绝对 Beat 与当前局部 TRS 一次完成；Seek、Reload、不同帧率必须相同。
- 使用稳定的平行移动标架，C0/C1 边界和拐点必须有 golden。

### S11-B：line 与几何资源

- line 的 portable 表示、粗细和棱柱挤出方式由 ADR/Spec 冻结。
- 不把 line 的视觉轨迹当成 Judgement 轨迹；对象/子对象共享既有 Transform 层级。
- 与 Stage 9 的 Model/Submesh、材质和资源闭包保持独立、可组合。

### S11-C：shader.json 声明接口

- CXC 根可包含空或省略的 `shader.json`；空值表示 SDK 默认。
- 本阶段只定义 schema、identity、capability、参数类型和拒绝路径，不执行用户 shader。
- 不接受 GLSL/HLSL 源、Shader Graph 或任意全屏 pass 图。

## 4. 验收标准

- V7 ADR、Chart v7 Spec、曲线形变 Spec、line 资源合同和 `shader.json` 接口 Spec 先于生产实现。
- 单轴轨迹、标架、Seek/Reload、跨平台 identity 和预算有 deterministic golden。
- v6 对象、v7 单轴对象和无曲线对象的语义边界清晰；不支持能力稳定拒绝。
- 合法 v4-v6 输入的既有 FrameDigest、Judgement、Score 和 Replay 结果不变。
- CXC、Player、headless、external consumer 和 owner acceptance 完整。

## 5. 明确不包含

- 双轴曲线、公式曲线、用户后处理绘制、骨骼、morph、任意脚本。
- 修改 Chart v5/v6 的字段含义或判定语义。
- Android、Vulkan 和整机性能矩阵；它们归 Stage 12。

## 6. 交接

交付 Chart v7 单轴曲线 capability、line/几何资源合同和声明式 `shader.json` 接口。Stage 13
可在保持 v7 identity 不变的前提下增加双轴形变和内置后处理执行。

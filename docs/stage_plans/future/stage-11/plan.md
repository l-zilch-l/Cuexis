# Stage 11 计划（提纲）：Chart v7 几何表现

状态：future；未启动，仅为方向提纲

更新日期：2026-11-23

<!-- cuexis-meta
id: PLAN-future.stage-11.plan
status: future
authority: none
-->

本文件是**未启动阶段的提纲**，不是执行计划。启动本阶段时，用 [plan.md](../../../templates/plan.md)（详细规划协议）重写，并在变更记录里注明替换关系。

归档来源：[Chart v7 历史设计输入](../../historical-inputs/chart-format-update-for-v7/plan.md)、[Chart v6 历史设计输入](../../historical-inputs/chart-format-update-for-v6/plan.md)、[Portable Presentation v1](../../../formats/PORTABLE_PRESENTATION.md) 与 [Material/Shader v1](../../../formats/MATERIAL_SHADER.md)。本阶段吸收原 `chart-format-update-for-v7` 延期计划。

## 阶段目标

候选，未承诺：建立 Chart v7 的几何表现合同——单轴分段三次 Bézier 曲线形变、内置 line 与更多常见模型、line 的粗细/棱柱表现，以及 CXC 根 `shader.json` 的声明式接口。本阶段不实现用户后处理绘制，也不改变 Judgement、Score 或 Replay。

## 范围与边界

- 前置基线为 Stage 8 Chart v5＋Gameplay v3 正式基线 与 Stage 9 的 Chart v6 / Model v1 / Presentation Foundation；Stage 10 Studio MVP 只在其预览或创作支持需要处参与。
- 继承 Stage 8/9 对 [RR 架构](../../active/stage-07-rr/plan.md)的实际交接，曲线与 line 接入已有会话、求值和帧提交边界；离线几何准备放在工具侧，Player 不增加第二条控制链。
- 继承前置正式玩法与旧版本兼容范围，Chart v7 不自动提高 Gameplay 版本。
- 本阶段只扩展 Presentation 与表现格式；曲线形变和几何资源的语义边界在 Chart v7 中定义，不改写 Chart v5/v6 的字段含义。
- 与 Stage 13 的边界：双轴形变和内置后处理执行归 Stage 13，且保持 v7 identity 不变；本阶段只冻结单轴合同与 `shader.json` 接口。
- 平台规模化（Android、Vulkan、整机性能矩阵）归 Stage 12。

## 入口条件

- Stage 8 关闭，Chart v5 production baseline 被接受。
- Stage 9 关闭，Chart v6 / Model v1 / Presentation Foundation 交付。
- Stage 10 的 Studio MVP 在需要预览或创作支持的部分可用。
- 若采用 PD 基线，[PD 细则 §12.2](../../completed/stage-07/04-pd/design.md) 的表现域接入合同已接受。

## 已确认约束

- `mappingAxes` 只允许一条 X/Y/Z 轴；两轴留给 Stage 13，三轴永久拒绝。
- 只使用分段三次 Bézier；不支持公式、表达式字符串或运行时生成曲线。
- 采样使用内容所属版本既有时间映射提供的绝对 Beat 与当前局部 TRS 一次完成，不另行换算或推进时钟；Seek、Reload、不同帧率必须得到相同结果；使用稳定的平行移动标架，C0/C1 边界和拐点必须有 golden。
- line 的视觉轨迹不是 Judgement 轨迹；对象/子对象共享既有 Transform 层级，line 与 Stage 9 的 Model/Submesh、材质和资源闭包保持独立、可组合。
- CXC 根可包含空或省略的 `shader.json`，空值表示 SDK 默认；本阶段只定义 schema、identity、capability、参数类型和拒绝路径，不执行用户 shader；不接受 GLSL/HLSL 源、Shader Graph 或任意全屏 pass 图。
- 合法 v4–v6 输入的既有 FrameDigest、Judgement、Score 和 Replay 结果不变。
- 若采用 PD 基线，沿 [PD 细则 §12.2](../../completed/stage-07/04-pd/design.md) 接入曲线/几何表现域：唯一 evaluator 产生该采样点的值或 Spec 冻结的 portable 参数，提帧不推进曲线时间或寿命；Snapshot、FrameDigest、资源/capability 和预算分别走版本门禁，不把未来几何塞进未知属性包。
- Stage 13 可在保持 v7 identity 不变的前提下增加双轴形变和内置后处理执行。

## 候选方向（非承诺）

启动时可能按以下工作线细化，均未承诺：

- 单轴曲线形变：单轴 `mappingAxes`、分段三次 Bézier、绝对 Beat 采样、平行移动标架、C0/C1 边界与拐点 golden。
- line 与几何资源：line 的 portable 表示、粗细和棱柱挤出方式由 ADR/Spec 冻结；与 Stage 9 的资源闭包、材质和 submesh 合同独立组合。
- `shader.json` 声明接口：schema、identity、capability、参数类型和拒绝路径；只声明不执行。

## 待决问题（启动前必须回答）

1. line 的 portable 表示、粗细和棱柱挤出方式由哪个 ADR/Spec 冻结？由格式 owner 决定。
2. Chart v7 交付 candidate 还是正式 capability？由 owner 在启动时决定。
3. "更多常见模型"是纳入本阶段，还是留在 Stage 9 的 Model v1 目录？由 owner 决定。
4. 发行基线是否包含 PD；若包含，由哪个已接受批次提供 §12.2 的接入合同与证据？
5. 曲线采样的每帧/每对象预算由谁定、进入哪类门禁？

## 验收标准

启动时定义。当前只写方向性判据：

- V7 ADR、Chart v7 Spec、曲线形变 Spec、line 资源合同和 `shader.json` 接口 Spec 先于生产实现被接受。
- 单轴轨迹、标架、Seek/Reload、跨平台 identity 和预算有 deterministic golden。
- v6 对象、v7 单轴对象和无曲线对象的语义边界清晰；不支持能力稳定拒绝。
- 合法 v4–v6 输入的既有 FrameDigest、Judgement、Score 和 Replay 结果不变。
- CXC、Player、headless、external consumer 与 owner acceptance 完整。

具体矩阵、命令、数值与证据形式在启动时由 owner 与规划模型制定，本提纲不承诺。

## 明确不包含

- 双轴曲线、公式曲线、用户后处理绘制、骨骼、morph、任意脚本。
- 修改 Chart v5/v6 的字段含义或判定语义。
- Android、Vulkan 和整机性能矩阵；它们归 Stage 12。

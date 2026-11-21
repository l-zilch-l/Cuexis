# Stage 13 计划（提纲）：高级表现与 Chart v8

状态：future；未启动，仅为方向提纲

更新日期：2026-11-17

<!-- cuexis-meta
id: PLAN-future.stage-13.plan
status: future
authority: none
-->

本文件是**未启动阶段的提纲**，不是执行计划。启动本阶段时，用 [plan.md](../../../templates/plan.md)（详细规划协议）重写，并在变更记录里注明替换关系。

归档来源：[Chart v8 历史设计输入](../../historical-inputs/chart-format-update-for-v8/plan.md)、[确定性粒子 ADR](../../../adr/0020-deterministic-particle-timeline.md) 与 [确定性粒子提案](../../../proposals/deferred/PARTICLE_TIMELINE.md)。本阶段吸收原 `chart-format-update-for-v8` 延期计划并接收确定性粒子提案。

## 阶段目标

候选，未承诺：交付受控的高级表现能力——Chart v8 的双轴 Bézier 形变、模型基本动画预设、可执行的 SDK 内置后处理，以及确定性粒子的 ChartTime/LocalTime、固定步长与版本化随机、有界 Checkpoint 与 Seek 重建。本阶段只扩展 Presentation 和表现格式，不改变 Input、Judgement、Score、Replay 或既有 Chart v5/v6/v7 语义；Stage 13 是可选的高级表现线，[Stage 14](../stage-14/plan.md) 不必等待全部未来表现能力完成。

## 范围与边界

- 前置基线为 Stage 8 Chart v5、Stage 9 Chart v6 / Model v1、[Stage 11](../stage-11/plan.md) 的 Chart v7 单轴表现，以及 [Stage 12](../stage-12/plan.md) 针对所选表现 profile 的规模化与平台证据。
- 与 Stage 11 的边界：双轴形变在 v7 单轴 identity 与标架规则之上扩展，不改变 v7 单轴语义；后处理执行 Stage 11 冻结的包根 `shader.json` 接口。
- 与 Stage 14 的边界：稳定 C ABI 归 Stage 14；本阶段的可选能力未完成不阻塞 Stage 14。
- 本阶段不修改 Judgement、Score、Replay 或 FrameSnapshot 的既有语义，只按各自的版本门禁扩展表现相关字段。

## 入口条件

- Stage 8、Stage 9 与 Stage 11 关闭并被接受。
- Stage 12 针对所选表现 profile 的规模化与平台证据可得。
- 若纳入粒子，[确定性粒子 ADR](../../../adr/0020-deterministic-particle-timeline.md) 已接受。
- 若采用 PD 基线，[PD 细则 §12.5–12.7](../../completed/stage-07/04-pd/design.md) 的候选重建边界已接受。

## 已确认约束

- 只扩展 Presentation 和表现格式，不改变 Input、Judgement、Score、Replay 或既有 Chart v5/v6/v7 语义；Stage 13 是可选高级表现线，Stage 14 不等待本阶段全部能力。
- 双轴：`mappingAxes` 恰好为两条互不相同的 X/Y/Z 轴，第三轴保持欧氏偏移；继续使用分段三次 Bézier，拒绝 `formula`、自由表达式和三轴映射；采样只依赖绝对时间和当前局部 TRS，Seek、Reload、不同帧率必须一致；沿用 Stage 11 的单轴 identity 和标架规则，合成顺序由独立 ADR/Spec 冻结。
- 模型动画复用既有 Behavior/AnimationClip 白名单（position、rotation、scale、visible、alpha、材质槽）；只提供少量冻结的 SDK 预设，不建立骨骼、morph 或运行时控制点生成；预设必须有 capability、资源 identity、预算和 headless golden。
- 后处理只执行 Stage 11 冻结的包根 `shader.json` 接口，只允许 SDK 内置效果与有类型参数；未知效果、参数越界或能力缺失稳定拒绝；后处理不得改变 Judgement/Replay，是否进入 FrameDigest 由 ADR 明确分离。
- 粒子默认使用 ChartTime，允许显式 LocalTime，暂停、Seek、discontinuity 语义固定；固定步长、版本化 RNG、绝对时间重建；Checkpoint 只作运行时缓存，不写入 Chart；GPU 路径不得静默替代具备确定性要求的 CPU 语义；无法在同步调用预算完成的重建只能走已接受的显式任务，分片期间为可诊断 building，超限明确失败，旧同步入口不得以 rebuilding 冒充成功。
- 采用 PD 基线时消费 [PD 细则 §12.2、§12.5–12.7](../../completed/stage-07/04-pd/design.md)：先在所属 ADR/Spec/ABI 接受显式任务、进度/取消/终态、时钟与版本迁移；每会话一个有界候选，固定目标后分片推进，中间步不发布状态修订、不推进 projection cursor、不扣 RemainingFrames；全部必需域完成同一采样点后复用无失败安装/finalize 一次提交，读桥/Renderer 不运行粒子模拟；旧完整图仅在原来源与控制仍合法时可保留显示，不代表目标完成，Seek/Reload 换作用域后不能重绑旧图。
- 候选/Checkpoint/current/scratch 峰值、取消清理、单片和总工作上限分别入账，重建超限不得无限追赶、拼接不同时间的域或显示近似结果；离线/预览可显式等待，实时普通更新准入须满足最坏推进预算。确定步长、RNG、Checkpoint 算法与具体公共任务字段仍由本阶段冻结，PD 不提前实现。
- 合法 v4–v7 输入的既有 digest、Judgement、Score 和 Replay 结果不变；不支持能力稳定拒绝，不降级为 v7 或普通 Transform。

## 候选方向（非承诺）

启动时可能按以下工作线细化，均未承诺：

- 双轴 Bézier：两轴 `mappingAxes`、第三轴欧氏偏移、合成顺序冻结、Seek/Reload 与跨平台 golden。
- 模型基本动画：少量冻结的 SDK 预设，带 capability、资源 identity、预算和 headless golden。
- 内置后处理：首批至少 identity、fade、vignette、color-multiply；Player 与 in-tree OpenGL adapter 执行，不改变 Judgement/Replay。
- 确定性粒子：ChartTime/LocalTime、固定步长、版本化 RNG、有界 Checkpoint 与 Seek 重建；重建走显式任务而非同步入口。

## 待决问题（启动前必须回答）

1. 首批后处理效果目录是否只含 identity、fade、vignette、color-multiply？由 owner 与渲染 owner 决定。
2. 粒子的固定步长、RNG 算法、Checkpoint 算法和公共任务字段由本阶段冻结，启动时由谁批准？
3. Chart v8 与确定性粒子交付 candidate 还是正式 capability？由 owner 在启动时决定。
4. 是否采用 PD 基线；若采用，由哪个已接受批次提供 §12.5–12.7 的任务/取消/终态合同与证据？
5. 后处理是否进入 FrameDigest 由哪份 ADR 决定？

## 验收标准

启动时定义。当前只写方向性判据：

- v8 ADR、Spec、Schema、Reader/Writer 和 capability 矩阵先于生产实现被接受。
- v7 单轴 golden、空 `shader.json` 和无粒子输入回归保持不变。
- 双轴、后处理和粒子均有 headless/adapter golden、Seek/Reload、预算和拒绝测试。
- 不支持能力稳定拒绝，不降级为 v7 或普通 Transform。
- 合法 v4–v7 输入的既有 digest、Judgement、Score 和 Replay 结果不变。
- CXC closure、FrameSnapshot 扩展和跨平台 identity 有完整 owner acceptance。

具体矩阵、命令、数值与证据形式在启动时由 owner 与规划模型制定，本提纲不承诺。

## 明确不包含

- 任意运行时脚本、用户 shader 源、Shader Graph、三轴形变、骨骼、morph。
- 修改 Chart v5 核心语义或把表现轨迹当成 Judgement 轨迹。
- 稳定 C ABI；该合同属于 Stage 14。

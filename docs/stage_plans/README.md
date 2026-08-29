# Stage Plan Index

状态：现行阶段计划索引

更新日期：2026-08-29

阶段计划描述目标、实施批次、门禁和交接。当前阶段以 [CURRENT_STATUS.md](../CURRENT_STATUS.md)
为准；完成后的实际证据以 [阶段报告](../stage_reports/README.md) 为准。

| 计划 | 状态 | 说明 |
| --- | --- | --- |
| [SDK transition plan](cuexis_sdk_transition_plan.md) | historical compatibility entry | 产品转型历史与旧链接入口 |
| [Stage 1B](stage_1b_implementation_plan.md) | completed | 资源生命周期闭环 |
| [Stage 1C](stage_1c_implementation_plan.md) | completed | 时间、Behavior、Headless Playback |
| [Stage 1D](stage_1d_implementation_plan.md) | completed | 主音乐、Clock、Audio |
| [Stage 1E](stage_1e_implementation_plan.md) | completed | SDK 安装与 external consumer |
| [Stage 2](stage_2_implementation_plan.md) | completed | Chart v3、TimingMap、Behavior Event |
| [Stage 3](stage_3_implementation_plan.md) | completed | Portable Presentation 与渲染 adapter |
| [Stage Chart Format Update](stage_chart_format_update_implementation_plan.md) | completed | C0–C4 complete；CFU-D/CFU-E/CFU-F/G 已关闭；G6 owner acceptance recorded 2026-08-24 |
| [Stage 4](stage_4_implementation_plan.md) | completed | Cuexis 表现动画；S4-H hosted 与 owner acceptance 已于 2026-08-27 记录 |
| [Stage 5](stage_5_implementation_plan.md) | future; S5-A frozen; S5-B wired; S5-C/S5-D/S5-E/S5-F/S5-G complete; S5-H local checkpoint | Material、Shader 与能力 Profile；S5-A 合同已冻结；S5-B 已接线；S5-C 已完成；S5-D 已完成；S5-E 已完成；S5-F 已完成；S5-G 已完成；S5-H local checkpoint；hosted 与 owner acceptance 待完成 |
| [260829 Full Review 整改计划](260829-full-review-remediation-plan.md) | active; implementation plan | 144 项全仓 review finding 的批次拆分、决策门、实现引导和验证矩阵；不代表 finding 已关闭 |
| [Stage 6](stage_6_implementation_plan.md) | future | Playback C++ API 与独立 Player 产品化 |
| [Stage 7](stage_7_implementation_plan.md) | future | Cuexis Studio 核心 |
| [Stage 8](stage_8_implementation_plan.md) | future optional | 确定性粒子表现扩展 |
| [Stage 9A](stage_9a_implementation_plan.md) | future | SDK 与宿主性能验证 |
| [Stage 9B](stage_9b_implementation_plan.md) | deferred | Android SDK 与宿主适配验证 |
| [Stage 10](stage_10_implementation_plan.md) | deferred optional | Vulkan adapter 可行性验证 |
| [Stage 11](stage_11_implementation_plan.md) | future | Input、Judgement、Score 与 Replay |
| [Stage 12](stage_12_implementation_plan.md) | future | 稳定 ABI 与 Playback SDK v1 |

Stage 4–12 文件从整理前的 PROJECT_GUIDE 和 SDK transition plan 中拆出。原始完整措辞仍保存在
[文档归档](../archive/README.md)；独立计划维护阶段目标、前置条件、实施范围和验收标准。

## Legacy Planning Sources

Stage 0 和 Stage 1A 的独立实施计划早于当前 Stage Plan 文件约定，没有单独的现行 plan 文件：

| 阶段 | 当前入口 | 原始规划来源 |
| --- | --- | --- |
| Stage 0 | [完成报告](../stage_reports/stage_0_completion_report.md) | [PROJECT_GUIDE archive](../archive/PROJECT_GUIDE_LEGACY_2026-08-10.md) |
| Stage 1A | [完成报告](../stage_reports/stage_1a_completion_report.md) | [PROJECT_GUIDE archive](../archive/PROJECT_GUIDE_LEGACY_2026-08-10.md) |

这两个阶段不是遗漏；当前只维护完成证据，原始目标、任务和验收描述保留在归档快照。

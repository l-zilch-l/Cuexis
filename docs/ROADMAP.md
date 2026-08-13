# Cuexis Roadmap

状态：现行路线图

更新日期：2026-08-13

产品边界由 [ADR 0027](adr/0027-playback-sdk-product-boundary.md) 冻结。本文只维护阶段顺序和当前
交接，不复制阶段实施细节或完成证据。

## 当前阶段

当前活动阶段是 **Stage Chart Format Update**，位于 Stage 3 与 Stage 4 之间，不使用数字别名。

```text
CFU-A inventory and use cases                       completed
CFU-B ADR and format contracts                      accepted
CFU-C Schema / typed Reader / Writer / validator    completed; C0-C4 complete
CFU-D migration                                     next
CFU-E Playback prepare and capability               pending
CFU-F consumers and determinism                     pending
CFU-G cross-platform closure and Stage 4 handoff    pending
```

权威计划：[stage_chart_format_update_implementation_plan.md](stage_plans/stage_chart_format_update_implementation_plan.md)。

## 已完成阶段

| 阶段 | 交付 | 证据 |
| --- | --- | --- |
| Stage 0 | 工程骨架、Core、Platform、World、OpenGL Player | [报告](stage_reports/stage_0_completion_report.md) |
| Stage 1A | Canonical Chart、ChartRuntime、事务实例化 | [报告](stage_reports/stage_1a_completion_report.md) |
| Stage 1B | Project、Asset Index、资源生命周期 | [报告](stage_reports/stage_1b_completion_report.md) |
| Stage 1C | Behavior、RuntimeFrame、Headless Playback | [报告](stage_reports/stage_1c_completion_report.md) |
| Stage 1D | 主音乐、Clock、Audio、Prepared Playback | [报告](stage_reports/stage_1d_completion_report.md) |
| Stage 1E | C++ static/shared preview、安装与 consumer | [报告](stage_reports/stage_1e_completion_report.md) |
| Stage 2 | Chart v3、TimingMap、Behavior/Step Event | [报告](stage_reports/stage_2_completion_report.md) |
| Stage 3 | Portable Presentation、Validation、OpenGL adapter | [报告](stage_reports/stage_3_completion_report.md) |

## 后续阶段

| 阶段 | 目标 | 前置条件 |
| --- | --- | --- |
| [Stage 4](stage_plans/stage_4_implementation_plan.md) | Cuexis 表现动画运行时 | Chart Format Update 全部门禁关闭 |
| [Stage 5](stage_plans/stage_5_implementation_plan.md) | Material/Shader 管线和能力 Profile | Stage 4 稳定表现输入 |
| [Stage 6](stage_plans/stage_6_implementation_plan.md) | Playback C++ API 与 Player 产品化 | 真实 consumer 和配置组合证据 |
| [Stage 7](stage_plans/stage_7_implementation_plan.md) | Cuexis Studio 核心 | 稳定 Playback/格式/预览路径 |
| [Stage 8](stage_plans/stage_8_implementation_plan.md) | 可选确定性粒子表现 | Studio、Material/Shader 基础 |
| [Stage 9A](stage_plans/stage_9a_implementation_plan.md) | SDK 与宿主性能验证 | 主要桌面闭环可测量 |
| [Stage 9B](stage_plans/stage_9b_implementation_plan.md) | Android SDK 与宿主适配验证 | 9A profile 和移动目标恢复决策 |
| [Stage 10](stage_plans/stage_10_implementation_plan.md) | 可选 Vulkan adapter 验证 | 存在真实产品需求 |
| [Stage 11](stage_plans/stage_11_implementation_plan.md) | Input、Judgement、Score、Replay | Studio 与性能基础 |
| [Stage 12](stage_plans/stage_12_implementation_plan.md) | 稳定 ABI 与 Playback SDK v1 | Judgement/Replay 公共生命周期完成 |

## 无限期延后

运行时脚本和逐帧脚本回调没有排期，也不属于“后续阶段待实现”列表。重新启动必须先取得新的
产品决策，再建立独立 ADR、威胁模型、格式合同和阶段计划。

## 维护规则

- 当前状态只在 [CURRENT_STATUS.md](CURRENT_STATUS.md) 更新。
- 阶段细节只在对应 stage plan 更新。
- 完成证据只写入 stage report。
- 路线变化需要 ADR 或项目所有者明确接受，不通过编辑多个摘要暗中改变边界。

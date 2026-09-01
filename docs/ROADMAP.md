# Cuexis Roadmap

状态：现行路线图

更新日期：2026-09-01

产品边界由 [ADR 0027](adr/0027-playback-sdk-product-boundary.md) 冻结。本文只维护阶段顺序和当前
交接，不复制阶段实施细节或完成证据。

## 当前阶段

Stage 5 已于 2026-08-28 关闭并经 PR #20 合并至 `master`。`260829-full-review` 随后于
2026-08-30 关闭；[260830-followup](stage_plans/completed/260830-followup/plan.md) 的文档整理、
Chart/CXC parse-once 和关键模块分支覆盖率三个任务已完成，并于 2026-09-01 经 PR #22 合并至
`master`。当前没有已启动的后续产品阶段；Stage 6 仍是 future，尚未启动。

```text
CFU-A inventory and use cases                       completed
CFU-B ADR and format contracts                      accepted
CFU-C Schema / typed Reader / Writer / validator    completed; C0-C4 complete
CFU-D migration                                     closed; owner recorded no external assets on 2026-08-14
CFU-E Playback prepare and capability               closed; owner-accepted on 2026-08-14
CFU-F consumers and determinism                     closed; final-SHA hosted gates passed 2026-08-16
CFU-G final closure                                 completed; G6 owner acceptance recorded 2026-08-24
```

权威计划：[stage_chart_format_update_implementation_plan.md](stage_plans/completed/chart-format-update/plan.md)。

## 已完成阶段

| 阶段 | 交付 | 证据 |
| --- | --- | --- |
| Stage 0 | 工程骨架、Core、Platform、World、OpenGL Player | [报告](stage_reports/stages/stage-00/completion.md) |
| Stage 1A | Canonical Chart、ChartRuntime、事务实例化 | [报告](stage_reports/stages/stage-01/stage-1a-completion.md) |
| Stage 1B | Project、Asset Index、资源生命周期 | [报告](stage_reports/stages/stage-01/stage-1b-completion.md) |
| Stage 1C | Behavior、RuntimeFrame、Headless Playback | [报告](stage_reports/stages/stage-01/stage-1c-completion.md) |
| Stage 1D | 主音乐、Clock、Audio、Prepared Playback | [报告](stage_reports/stages/stage-01/stage-1d-completion.md) |
| Stage 1E | C++ static/shared preview、安装与 consumer | [报告](stage_reports/stages/stage-01/stage-1e-completion.md) |
| Stage 2 | Chart v3、TimingMap、Behavior/Step Event | [报告](stage_reports/stages/stage-02/completion.md) |
| Stage 3 | Portable Presentation、Validation、OpenGL adapter | [报告](stage_reports/stages/stage-03/completion.md) |
| Stage 4 | Cuexis 表现动画运行时 | [报告](stage_reports/stages/stage-04/completion.md) |
| Stage 5 | Material/Shader 管线和能力 Profile | [报告](stage_reports/stages/stage-05/completion.md) |

## 后续阶段

| 阶段 | 目标 | 前置条件 |
| --- | --- | --- |
| [Stage 6](stage_plans/future/stage-06/plan.md) | Playback C++ API 与 Player 产品化 | 真实 consumer 和配置组合证据 |
| [Stage 7](stage_plans/future/stage-07/plan.md) | Cuexis Studio 核心 | 稳定 Playback/格式/预览路径 |
| [Stage 8](stage_plans/future/stage-08/plan.md) | 可选确定性粒子表现 | Studio、Material/Shader 基础 |
| [Stage 9A](stage_plans/future/stage-09a/plan.md) | SDK 与宿主性能验证 | 主要桌面闭环可测量 |
| [Stage 9B](stage_plans/deferred/stage-09b/plan.md) | Android SDK 与宿主适配验证 | 9A profile 和移动目标恢复决策 |
| [Stage 10](stage_plans/deferred/stage-10/plan.md) | 可选 Vulkan adapter 验证 | 存在真实产品需求 |
| [Stage 11](stage_plans/future/stage-11/plan.md) | Input、Judgement、Score、Replay | Studio 与性能基础 |
| [Stage 12](stage_plans/future/stage-12/plan.md) | 稳定 ABI 与 Playback SDK v1 | Judgement/Replay 公共生命周期完成 |

## 无限期延后

运行时脚本和逐帧脚本回调没有排期，也不属于“后续阶段待实现”列表。重新启动必须先取得新的
产品决策，再建立独立 ADR、威胁模型、格式合同和阶段计划。

## 维护规则

- 当前状态只在 [CURRENT_STATUS.md](CURRENT_STATUS.md) 更新。
- 阶段细节只在对应 stage plan 更新。
- 完成证据只写入 stage report。
- 路线变化需要 ADR 或项目所有者明确接受，不通过编辑多个摘要暗中改变边界。

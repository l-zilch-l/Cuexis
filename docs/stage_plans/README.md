# Stage Plan Index

状态：current

更新日期：2026-11-23

<!-- cuexis-meta
id: INDEX-stage_plans
status: active
authority: index
-->

阶段计划定义目标、范围、批次、门禁和交接。当前实现状态只以
[CURRENT_STATUS.md](../CURRENT_STATUS.md) 为准；完成证据只以
[stage_reports/README.md](../stage_reports/README.md) 为准。

本页负责定位计划；详细范围、历史决策及验收经过见对应正文。阶段顺序见 [ROADMAP](../ROADMAP.md)。

| 目录 | 用途 |
| --- | --- |
| `active/` | 当前执行或已确定即将执行的计划；是否启动以当前状态页为准 |
| `future/` | 后续阶段提纲 |
| `completed/` | 已完成或已按限制退出的计划 |
| `deferred/` | 暂缓、等待触发条件的计划 |
| `historical-inputs/` | 已并入主路线的旧设计输入 |
| `reviews/` | 跨阶段审查后的整改计划；证据归报告目录 |

## 阶段路线

| 计划 | 定位 |
| --- | --- |
| [Stage 7-RR](active/stage-07-rr/plan.md) | 当前复核与重构阶段，含[文档管理与整理（T-18）](active/stage-07-rr/documentation-governance.md)；[任务与分册](active/stage-07-rr/README.md)、[发现登记](active/stage-07-rr/findings.md) |
| [Stage 7.5](active/stage-07-5/plan.md) | 尚未实施；Gameplay Evolution 与性能稳定 |
| [Stage 8](future/stage-08/plan.md) | Chart v5＋Gameplay v3 正式主线及格式发行收敛 |
| [Stage 9](future/stage-09/plan.md) | Presentation Foundation、Chart v6 / Model v1 |
| [Stage 10](future/stage-10/plan.md) | Studio 与创作/打包工作流 |
| [Stage 11](future/stage-11/plan.md) | Chart v7 几何表现与 shader.json |
| [Stage 12](future/stage-12/plan.md) | 规模化、桌面性能、Android 与 Vulkan |
| [Stage 13](future/stage-13/plan.md) | Chart v8、后处理与确定性粒子 |
| [Stage 14](future/stage-14/plan.md) | 稳定 ABI 与 Playback SDK v1 |

## 跨阶段专题

- [Chart v5 跨阶段计划](active/chart-format-update-for-v5/plan.md)：汇合格式设计与各阶段交接。

## 已完成计划

- Stage 0：[完成报告](../stage_reports/stages/stage-00/completion.md)
- Stage 1A：[完成报告](../stage_reports/stages/stage-01/stage-1a-completion.md)
- Stage 1B：[计划](completed/stage-01/stage-1b-plan.md)
- Stage 1C：[计划](completed/stage-01/stage-1c-plan.md)
- Stage 1D：[计划](completed/stage-01/stage-1d-plan.md)
- Stage 1E：[计划](completed/stage-01/stage-1e-plan.md)
- Stage 2：[计划](completed/stage-02/plan.md)
- Stage 3：[计划](completed/stage-03/plan.md)
- Stage Chart Format Update：[计划](completed/chart-format-update/plan.md)
- Stage 4：[计划](completed/stage-04/plan.md)
- Stage 5：[计划](completed/stage-05/plan.md)
- Chart Format Foundation：[计划](completed/chart-format-foundation/plan.md)
- Foundation 交接加固：[计划](completed/chart-format-foundation-hardening/plan.md)
- Stage 6：[计划](completed/stage-06/plan.md)、[关闭报告](../stage_reports/stages/stage-06/completion.md)
- Stage 7：[受限退出计划](completed/stage-07/plan.md)、[批次导航](completed/stage-07/README.md)
- 260830 follow-up：[Chart/CXC parse-once 与关键模块覆盖率计划](completed/260830-followup/plan.md)

## 历史复核计划

以下是已完成的审查整改记录，保留入口供追溯；批次、验证与关闭经过见原文。

- [2026-08 全项目审查整改](reviews/full-review-2026-08/remediation-plan.md)
- [Stage 6 审查整改](reviews/stage-06-review-remediation/plan.md)，含 [R9 宿主命令循环](reviews/stage-06-review-remediation/R9-reference-host-command-loop.md)

## 已并入主阶段的历史输入

| 历史输入 | 现归属 |
| --- | --- |
| [Stage 9A 桌面性能](historical-inputs/stage-09a/plan.md) | Stage 12 |
| [Stage 9B Android](historical-inputs/stage-09b/plan.md) | Stage 12 |
| [旧 Stage 10 Vulkan](historical-inputs/stage-10/plan.md) | Stage 12 |
| [Chart v6 / Model v1](historical-inputs/chart-format-update-for-v6/plan.md) | Stage 9 |
| [Chart v7](historical-inputs/chart-format-update-for-v7/plan.md) | Stage 11 |
| [Chart v8](historical-inputs/chart-format-update-for-v8/plan.md) | Stage 13 |
| [确定性粒子](../proposals/deferred/PARTICLE_TIMELINE.md) | Stage 13 |

## 文档规则

阶段计划不是格式 Spec，不复制完整字段合同；格式 Spec 也不负责决定阶段顺序。
计划中的 candidate/future/deferred 状态不能被摘要文档写成 implemented。阶段编号变化
必须同步更新 [ROADMAP.md](../ROADMAP.md)、[CURRENT_STATUS.md](../CURRENT_STATUS.md)
和相关计划交接。

旧路径映射见 [legacy-paths.md](legacy-paths.md)。

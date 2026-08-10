# Cuexis Current Status

状态：现行状态页

更新日期：2026-08-10

本文是当前阶段和实现状态的唯一摘要。阶段计划、完成报告和审查报告仍然保留各自的历史
细节，但不能绕过本文重新定义当前状态。

## 产品边界

Cuexis 的产品结构是可嵌入的 Playback SDK、独立参考 Player 和独立 Studio。宿主通过
PlaybackSession、FrameSnapshot、ContentProvider 和后续 Judgement/Replay 合同接入；宿主不
访问 RuntimeSession、World、EnTT、SDL、OpenGL 或 JSON DOM。

权威决策：[ADR 0027](adr/0027-playback-sdk-product-boundary.md)。

## 阶段状态

| 阶段 | 状态 | 权威证据 |
| --- | --- | --- |
| Stage 0 | 已完成 | [完成报告](stage_reports/stage_0_completion_report.md) |
| Stage 1A | 已完成 | [完成报告](stage_reports/stage_1a_completion_report.md) |
| Stage 1B | 已完成 | [完成报告](stage_reports/stage_1b_completion_report.md) |
| Stage 1C | 已完成 | [完成报告](stage_reports/stage_1c_completion_report.md) 与 [全量审查](stage_reports/260722-1c-review.md) |
| Stage 1D | 已完成 | [完成报告](stage_reports/stage_1d_completion_report.md) |
| Stage 1E | 已完成 | [完成报告](stage_reports/stage_1e_completion_report.md) |
| Stage 2 | 已完成 | [完成报告](stage_reports/stage_2_completion_report.md) |
| Stage 3 | 已完成 | [完成报告](stage_reports/stage_3_completion_report.md) |
| Stage Chart Format Update | 当前活动阶段，CFU-A 完成，CFU-B 提案 | [实施计划](stage_plans/stage_chart_format_update_implementation_plan.md) |
| Stage 4 | 未开始，等待格式阶段关闭 | [实施计划](stage_plans/stage_4_implementation_plan.md) |

Stage Chart Format Update 是 Stage 3 与 Stage 4 之间的正式名称，不使用 Stage 3.5 作为别名。

## 格式状态

- Chart v1/v2/v3 已实现并继续作为当前生产格式族。
- CXC v1、Chart v4 和 CXT v1 仍是 ADR 0038 提案范围，尚未进入 Schema、Reader、Writer 或
  公共 API 生产支持。
- CXT v1、播放前参数、Template Binding 和运行时脚本无限期延后子决策已于 2026-08-10 接受。
- `.cxt` 是 UTF-8 JSON 声明式模板，不是脚本、字节码或 SDK 隐式内置实现。

格式入口：[formats/README.md](formats/README.md)。

## 脚本边界

运行时脚本和逐帧脚本回调无限期延后，不属于任何已排期阶段。当前不预留 Chart/CXT/CXC 字段、
extension、capability、字节码、模块 ABI 或 Playback 执行入口。离线 authoring generator 可以
作为未来独立工具讨论，但不会进入 CXC，也不会被 pack、prepare 或 Playback 隐式执行。

## 尚未完成的主要能力

- 正式 `cuexis_judgement`、InputEvent、ReplayData 和确定性回放
- Studio 独立应用实现
- 稳定 C ABI 和语言绑定
- Chart v4/CXC/CXT 生产实现
- Stage 4 AnimationSystem 运行时实现

## 状态更新规则

任何阶段状态变化必须同时更新本文和对应的阶段报告或审查报告。历史报告不得被改写成新的
验证结果；应在顶部增加快照说明，并链接后续关闭证据。

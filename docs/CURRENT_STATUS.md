# Cuexis Current Status

状态：current

更新日期：2026-09-01

本文是 Cuexis 当前产品和阶段状态的唯一摘要。ADR 定义决策，Spec 定义字段和语义，阶段计划定义未来
范围，阶段报告保存带日期的实施证据；它们不得绕过本文重新定义当前状态。

## 产品边界

Cuexis 由可嵌入的 Playback SDK、独立参考 Player 和独立 Studio 构成。宿主通过
`PlaybackSession`、`PlaybackSource`、`FrameSnapshot`、ContentProvider 和后续 Judgement/Replay
合同接入；宿主不访问 RuntimeSession、World、EnTT、SDL、OpenGL 或 JSON DOM。

权威决策：[ADR 0027](adr/0027-playback-sdk-product-boundary.md)。公共使用说明见
[API reference](api/README.md)。

## 阶段状态

| 阶段 | 当前状态 | 权威入口 |
| --- | --- | --- |
| Stage 0 | completed | [report](stage_reports/stages/stage-00/completion.md) |
| Stage 1A-1E | completed | [reports](stage_reports/stages/stage-01/README.md) |
| Stage 2 | completed | [plan](stage_plans/completed/stage-02/plan.md) |
| Stage 3 | completed | [plan](stage_plans/completed/stage-03/plan.md) |
| Stage Chart Format Update | completed; CFU-C0-C4, D, E, F and G closed; G6 owner acceptance recorded 2026-08-24 | [plan](stage_plans/completed/chart-format-update/plan.md) |
| Stage 4 | completed; S4-H hosted and owner acceptance recorded 2026-08-27 | [plan](stage_plans/completed/stage-04/plan.md) |
| Stage 5 | completed; S5-A through S5-H closed and merged into `master` 2026-08-28 | [plan](stage_plans/completed/stage-05/plan.md)、[completion](stage_reports/stages/stage-05/completion.md) |
| Stage 6-12 | future or deferred as individually marked | [plan index](stage_plans/README.md) |

## 已关闭的 Full Review

Stage 5 已于 2026-08-28 经 PR #20 合并至 `master`；其 S5-H 报告保留为关闭前的本地检查快照，
现行关闭结论以 [Stage 5 completion](stage_reports/stages/stage-05/completion.md) 为准。

`260829 Full Review` 随后已于 2026-08-30 关闭。最终实现 SHA
`fbe118bb310fffa1446584e0a30fd46bc743413b` 已通过 Linux Quality、Windows MSVC 和 Windows MinGW
hosted 验证；144 项 finding 的 disposition 已记录。该关闭不重新打开已关闭的 Stage 5，也不表示
Chart/CXC parse-once、RT-29、World/Animation 大规模优化、大包解析降本、Studio、Judgement/Replay、
稳定 C ABI 或运行时脚本已经实现。

证据：[Full Review final closure](stage_reports/reviews/full-review-2026-08/2026-08-30-final.md)。

## 当前格式与 SDK 合同

- Chart v1/v2/v3 Reader、迁移和 Playback 路径继续保留；Chart v4 的静态、参数化和合法非空动画已由
  默认 Playback Session 求值。
- FrameDigest v1-v3、canonical bytes/order、合法输入 identity 与默认 capability 维持兼容。
- CXC v1、CXT v1、Chart v4 的格式语义以 [formats index](formats/README.md) 为准；内部 CXC 不是独立
  公共 package SDK。
- SDK API 为 `0.7.0`。安装后的 Playback headers 不泄露 EnTT、SDL、OpenGL/GLAD、JSON DOM、
  RuntimeSession 或 World。
- Stage 5 的 default `allCapabilities()` 包含 shader asset 和 parameterized material capability；
  显式裁剪 Session 仍可稳定拒绝它们。Playback 热路径不调用 shader compiler。

## 延期和禁止边界

运行时脚本和逐帧 script callback 无限期延后；不为它们预留 Chart/CXT/CXC 字段、extension、
capability、bytecode、ABI 或 Playback hook。离线 authoring generator 只能作为未来独立工具讨论。

已关闭的 [260830-followup 维护计划](stage_plans/completed/260830-followup/plan.md) 的文档整理、
Chart/CXC parse-once 和关键模块分支覆盖率三个任务均已完成。任务 3 的最终 SHA
`299596c533a8c66a78b5c4ada341b1163528fb25` 已通过 Linux Quality、Windows MSVC 和 Windows
MinGW；分模块覆盖率和环境残余见其[完成报告](stage_reports/reviews/260830-followup/2026-08-31-task-3-hosted-verification.md)。
PR #22 已于 2026-09-01 合并至 `master`，关闭总结见
[260830-followup 最终关闭报告](stage_reports/reviews/260830-followup/2026-09-01-final.md)。
RT-29、T1 World/Animation 大规模优化、T2 大包解析降本和 T4 Stage 6/API/Player 工作仍需另外的
触发证据和明确授权；Stage 6 仍为 future，尚未启动。

## 更新规则

任何产品阶段状态变化必须同时更新本文与相应的计划或新报告。历史报告不得被改写为新的验证结果；
新的关闭、复核或纠正必须形成新的带日期报告。

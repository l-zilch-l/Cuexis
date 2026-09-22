# Stage Plan Index

状态：current

更新日期：2026-09-22

阶段计划定义目标、范围、批次、门禁和交接。当前实现状态只以
[CURRENT_STATUS.md](../CURRENT_STATUS.md) 为准；完成证据只以
[stage_reports/README.md](../stage_reports/README.md) 为准。

目录约定：

```text
active
  当前正在执行或已确定即将执行的计划

future
  已规划但尚未进入近期实施队列的阶段计划

deferred
  暂不排期、等待触发条件或需要重新立项的设计输入
```

当前 `active` 目录中的两个计划职责不同：

```text
stage-06
  当前实施计划：Playback C++ API 与 Player 产品化。Foundation 交接加固已于 2026-09-17
  关闭并经 owner 接受，本计划自此恢复实施

chart-format-update-for-v5
  Chart v5 跨阶段总工作包，负责维护 Foundation、Stage 6、Stage 7A 和 Stage 8 的总体设计与交接
```

## 已完成计划

- Stage 0：[completion report](../stage_reports/stages/stage-00/completion.md)
- Stage 1A：[completion report](../stage_reports/stages/stage-01/stage-1a-completion.md)
- Stage 1B：[plan](completed/stage-01/stage-1b-plan.md)
- Stage 1C：[plan](completed/stage-01/stage-1c-plan.md)
- Stage 1D：[plan](completed/stage-01/stage-1d-plan.md)
- Stage 1E：[plan](completed/stage-01/stage-1e-plan.md)
- Stage 2：[plan](completed/stage-02/plan.md)
- Stage 3：[plan](completed/stage-03/plan.md)
- Stage Chart Format Update：[plan](completed/chart-format-update/plan.md)
- Stage 4：[plan](completed/stage-04/plan.md)
- Stage 5：[plan](completed/stage-05/plan.md)
- Chart Format Foundation：[plan](completed/chart-format-foundation/plan.md)
- Foundation 交接加固（R0-R5）：[plan](completed/chart-format-foundation-hardening/plan.md)，
  完成证据见 [R5 报告](../stage_reports/stages/chart-format-foundation/2026-09-17-r5-regression-and-handoff.md)
- 260830 follow-up：[plan](completed/260830-followup/plan.md)

CFU 与 260830 follow-up 的当前状态：Chart/CXC parse-once 和关键模块分支覆盖率已完成，
具体证据仍以 [260830-followup plan](completed/260830-followup/plan.md) 为准。

## 当前主路线

- [Stage 6](active/stage-06/plan.md)：**当前阶段，2026-09-17 启动（首批 S6-A）**，以 Chart v5
  Core/Packed 为主要开发基线的 Playback/Player/CXC candidate path；Chart v4 保留为兼容回退。
  前置的
  [Foundation 交接加固](completed/chart-format-foundation-hardening/plan.md) 已于 2026-09-17
  关闭并经 owner 接受，允许/禁止消费边界见其
  [R5 报告](../stage_reports/stages/chart-format-foundation/2026-09-17-r5-regression-and-handoff.md)。
  2026-09-20 已细化子批次依赖、阻塞决策和验收矩阵。八项关键决策已在
  [ADR 0042](../adr/0042-stage-6-productization-boundaries.md) 冻结。A1、A2、B1 和 C1
  已有退出证据；D1、C2、E1/E2 及后续批次尚未完成。设计冻结不表示这些后续批次已经实现。
- [Stage 7A / 7B+](future/stage-07/plan.md)：7A 冻结最小 Input/Judgement/Score/Replay 内核，
  7B+ 持续扩展 Slide、Flick、多指、校准和高级判定能力。
- [Stage 8](future/stage-08/plan.md)：Chart v5 正式语义、CXT v2、Packed Chart 和 CXC 发行收敛；
  只依赖 Stage 7A，不等待全部 Stage 7B+。
- [Stage 9](future/stage-09/plan.md)：Presentation Environment、天空盒、模型和有限形变。
- [Stage 10](future/stage-10/plan.md)：Studio 和完整创作/打包工作流。
- [Stage 11](future/stage-11/plan.md)：性能、移动端、Vulkan、粒子和高级表现。
- [Stage 12](future/stage-12/plan.md)：稳定 ABI 与 Playback SDK v1。

历史编号专题：

- Stage 9A：[旧桌面性能计划](future/stage-09a/plan.md)，现归入 Stage 11A。
- Stage 9B：[Android 设计输入](deferred/stage-09b/plan.md)，现归入 Stage 11B。
- Stage 10：[旧 Vulkan 设计输入](deferred/stage-10/plan.md)，现归入 Stage 11C。

## 格式专题

- [Chart v5 format plan](active/chart-format-update-for-v5/plan.md)：保留 active 的跨阶段总工作包，正式发行门禁归 Stage 8；
  Foundation 及其交接加固关闭后为 Stage 6 提供 candidate Core/Packed path，在 Stage 7A 判定合同冻结
  后完成正式发行收敛。
- [Chart v6 / Model v1](deferred/chart-format-update-for-v6/plan.md)：Stage 9 模型批次的设计输入。
- [Chart v7](deferred/chart-format-update-for-v7/plan.md)：Stage 9 形变批次的设计输入。
- [Chart v8](deferred/chart-format-update-for-v8/plan.md)：Stage 11 高级表现的设计输入。

## Stage 11 子计划

以下旧计划不再代表主阶段编号，但保留为 Stage 11 的专题输入：

- [桌面性能](future/stage-09a/plan.md)
- [Android](deferred/stage-09b/plan.md)
- [Vulkan](deferred/stage-10/plan.md)
- [确定性粒子](future/stage-08/plan.md)

这些文件在恢复实施前必须按 Stage 11 目标更新为对应子计划，不能继续把旧编号解释为
独立主阶段。

## 文档规则

阶段计划不是格式 Spec，不复制完整字段合同；格式 Spec 也不负责决定阶段顺序。
计划中的 candidate/future/deferred 状态不能被摘要文档写成 implemented。阶段编号变化
必须同步更新 [ROADMAP.md](../ROADMAP.md)、[CURRENT_STATUS.md](../CURRENT_STATUS.md)
和相关计划交接。

旧路径映射见 [legacy-paths.md](legacy-paths.md)。

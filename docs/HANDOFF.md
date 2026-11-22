# 当前交接

状态：active

更新日期：2026-11-22

<!-- cuexis-meta
id: STATUS-HANDOFF
status: active
authority: owns
-->

当前状态以 [CURRENT_STATUS](CURRENT_STATUS.md) 为准；阶段范围、设计记录、提案与发现分别见下列文档。

## 当前阶段与工作项

- [计划](stage_plans/active/stage-07-rr/plan.md)、[登记册](stage_plans/active/stage-07-rr/findings.md)、[可实施性索引](stage_plans/active/stage-07-rr/design-readiness.md)。

## 当前任务

下一次对话仅做 Stage 7-RR 文档描述优化，范围与步骤见[下一步](#下一步)。

## Plan 文档拆分（长期约定）

目标：把 active plan 拆成四类文档；先在 Stage 7-RR 试点，不一次性迁移全部计划。

```text
stage_plans/active/<stage>/
  plan.md          # 总览：定位/目标/边界/前置基线/任务索引（ID+链接）/验收入口
  schedule.md      # 调度图：只含 ID、波次、前置、并行/独占
  tasks/T-xx.md    # 每任务一卡，子代理自包含
  acceptance.md    # 阶段验收矩阵，引用任务 ID
```

规则：

- `schedule.md` **不得含任何 plan/task 正文**：不写任务名、描述、状态、owner、验收；只写
  `波次 | 任务 ID | 前置 ID | 可并行/独占`，可加 Mermaid 依赖图。
- 独占波次只能有一个任务；前置只能引用更早波次；每个任务在调度表中恰好出现一次。
- `tasks/T-xx.md` 写目标、输入、写域、步骤、**任务级验收**、证据、停止条件；**不写 `depends_on`**，
  依赖只存在 `schedule.md`，避免两份真相。
- `acceptance.md` 只写阶段验收矩阵、交叉验证（主 agent 重跑 + 独立子代理抽查/阴性对照）、
  证据清单与 deferral，不重复任务卡细项。
- 新增 `check_plan_schedule.py` 校验 ID 覆盖、前置存在、波次单调、独占单任务、表内无任务正文。
- future 阶段仍只用提纲模板，不建 `tasks/` 或 `schedule.md`，直到启动。

## 下一步

1. 优先核对并优化[计划](stage_plans/active/stage-07-rr/plan.md)、[架构设计](stage_plans/active/stage-07-rr/architecture-design.md)、[简化提案](stage_plans/active/stage-07-rr/simplification-proposal.md)、[可实施性索引](stage_plans/active/stage-07-rr/design-readiness.md)，再沿相关链接核对合同、任务卡与[阶段验收](stage_plans/active/stage-07-rr/acceptance.md)。
2. 优化语言清晰度、术语一致性、重复内容与引用，明确现状、已接受设计和候选状态，保持既定设计语义；实质冲突记入[登记册](stage_plans/active/stage-07-rr/findings.md)，交 owner 裁定。

本次交接仅安排文档优化；[T-01](stage_plans/active/stage-07-rr/tasks/T-01.md)及后续实施须 owner 后续明确启动，旧[计划启动登记](stage_plans/active/stage-07-rr/plan.md#12-交接与变更控制)不代表下一次对话直接开工。

## 阻塞与待决

[实质冲突登记](stage_plans/active/stage-07-rr/findings.md)、[阶段停止条件](stage_plans/active/stage-07-rr/plan.md#10-回滚阻塞与停止条件)、[后继采用边界](stage_plans/active/stage-07-rr/plan.md#唯一派单授权入口)。

## 验证状态

[本轮检查与回执](stage_reports/stages/stage-07-rr/2026-11-22-checkout-review-resolution.md#执行记录)、[逐项处置](stage_reports/stages/stage-07-rr/2026-11-22-checkout-review-resolution.md#结果与证据)。

## 最近改动

- [独立 checkout 复审登记](stage_plans/active/stage-07-rr/findings.md#16-独立-checkout-复审处置2026-11-22)。
- [设计缺陷处置](stage_plans/active/stage-07-rr/findings.md)：RR-F523–RR-F529 按最小改法落地，含 A-04 阶段口径、F419 排入 T-06、F401 归 T-16、F420 待裁与无音轨终点订正；三处标注待 owner 追认（2026-11-22）。
- [旧问题承接](stage_plans/active/stage-07-rr/findings.md)：在 T-04/06/12/13/15/16 点名 14 条旧发现由新设计承接，并关闭六条已修未标的登记（2026-11-22）。
- [T-17 单独立项](stage_plans/active/stage-07-rr/tasks/T-17.md)：会话层 P1 正确性（RR-F501/F508/F509）单独成卡，可拆两段派子代理；未启动实施（2026-11-22）。

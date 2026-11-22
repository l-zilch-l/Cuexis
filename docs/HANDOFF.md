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

从[独立 checkout 复审处置](stage_reports/stages/stage-07-rr/2026-11-22-checkout-review-resolution.md)接续 [T-01：基线、派单包与冷启动](stage_plans/active/stage-07-rr/tasks/T-01.md)，按[现行实施范围](stage_plans/active/stage-07-rr/plan.md#唯一派单授权入口)执行。

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

1. 按[本次启动授权](stage_plans/active/stage-07-rr/plan.md#12-交接与变更控制)执行 [T-01](stage_plans/active/stage-07-rr/tasks/T-01.md#步骤)：核验当前提交和[本轮修订及证据](stage_reports/stages/stage-07-rr/2026-11-22-checkout-review-resolution.md#执行记录)，绑定实施基线与合同修订，登记采用。
2. T-01 验收后按[调度表](stage_plans/active/stage-07-rr/schedule.md)派发现行 T-01–T-09 图中的后续任务；各卡按[阶段验收](stage_plans/active/stage-07-rr/acceptance.md)取证。

## 阻塞与待决

[T-01 停止条件](stage_plans/active/stage-07-rr/tasks/T-01.md#停止条件)、[阶段停止条件](stage_plans/active/stage-07-rr/plan.md#10-回滚阻塞与停止条件)、[后继采用边界](stage_plans/active/stage-07-rr/plan.md#唯一派单授权入口)。

## 验证状态

[本轮检查与回执](stage_reports/stages/stage-07-rr/2026-11-22-checkout-review-resolution.md#执行记录)、[逐项处置](stage_reports/stages/stage-07-rr/2026-11-22-checkout-review-resolution.md#结果与证据)。

## 最近改动

- [独立 checkout 复审登记](stage_plans/active/stage-07-rr/findings.md#16-独立-checkout-复审处置2026-11-22)。

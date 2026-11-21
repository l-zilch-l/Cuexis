# 当前交接

状态：active

更新日期：2026-11-21

<!-- cuexis-meta
id: STATUS-HANDOFF
status: active
authority: owns
-->

当前状态以 [CURRENT_STATUS](CURRENT_STATUS.md) 为准；阶段范围、设计记录、提案与发现分别见下列文档。

## 当前阶段与工作项

- 计划：[Stage 7-RR plan](stage_plans/active/stage-07-rr/plan.md)；登记册：[findings](stage_plans/active/stage-07-rr/findings.md)。
- 设计：[architecture-design](stage_plans/active/stage-07-rr/architecture-design.md)；提案：[simplification-proposal](stage_plans/active/stage-07-rr/simplification-proposal.md)。
- 文档路由与阶段状态：[MAP](MAP.md)、[CURRENT_STATUS](CURRENT_STATUS.md)。

## 当前任务

本轮已按 owner 授权补齐[提案](stage_plans/active/stage-07-rr/simplification-proposal.md)中 P1–P5 五项设计缺口的待审方案并同步台账。新增 P1–P6 技术推荐仍待逐项审阅；已接受设计见[附页](stage_plans/active/stage-07-rr/architecture-design.md)，本轮记录见[计划](stage_plans/active/stage-07-rr/plan.md)和[提案复核记录](stage_plans/active/stage-07-rr/findings.md#12-提案复核记录2026-11-21)。

## Plan 文档拆分（下一轮并行工作）

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

1. 逐项审阅[提案](stage_plans/active/stage-07-rr/simplification-proposal.md)新增的 P1–P6 技术推荐；实施仍须满足[计划](stage_plans/active/stage-07-rr/plan.md)的范围、前置与授权门禁。

## 阻塞与待决

待审推荐：[架构精简提案](stage_plans/active/stage-07-rr/simplification-proposal.md)；已接受设计：[设计附页](stage_plans/active/stage-07-rr/architecture-design.md)；阶段范围：[计划](stage_plans/active/stage-07-rr/plan.md)；发现：[登记册](stage_plans/active/stage-07-rr/findings.md)。

## 验证状态

主代理本轮实跑 `check_docs.py`、`gen_docs_map.py --check`、`check_docs_facts.py` 与 `git diff --check`，四项均通过（退出码 0）。实现与运行验收：未开始。

## 最近改动

- 本轮共识订正、台账同步与有限基线差异核对见[计划](stage_plans/active/stage-07-rr/plan.md)和[提案复核记录](stage_plans/active/stage-07-rr/findings.md#12-提案复核记录2026-11-21)。

# 当前交接

状态：active

更新日期：2026-11-18

<!-- cuexis-meta
id: STATUS-HANDOFF
status: active
authority: owns
-->

本页只交接进度、停工与验证状态。当前阶段以 [CURRENT_STATUS](CURRENT_STATUS.md) 为准，阶段范围与工作以 [Stage 7-RR 计划](stage_plans/active/stage-07-rr/plan.md) 为准，发现状态以 [发现登记册](stage_plans/active/stage-07-rr/findings.md) 为准。

## 当前阶段与工作项

- 当前阶段：Stage 7-RR；计划：[plan.md](stage_plans/active/stage-07-rr/plan.md)；登记册：[findings.md](stage_plans/active/stage-07-rr/findings.md)。
- 架构讨论正文：[设计附页](stage_plans/active/stage-07-rr/architecture-design.md)。本交接不续写讨论台账。
- 背景与证据入口：[架构研究](stage_reports/stages/stage-07-rr/2026-11-17-architecture-research.md)、[架构证据](stage_reports/stages/stage-07-rr/2026-11-17-architecture-evidence.md)、[Chart 表示与转换点](architecture/CHART_REPRESENTATIONS.md)、[ADR 0041](adr/0041-legacy-format-exit-policy.md)、[ADR 0042](adr/0042-stage-6-productization-boundaries.md)。
- 仓库：E:\Cuexis-worktree；分支：stage-7-rr；关联 PR #47（替代已关闭的 #46）。

## 当前任务

本轮仅做架构设计与计划，不改生产代码；架构讨论正文见[设计附页](stage_plans/active/stage-07-rr/architecture-design.md)，不在交接页展开。当前工作分三项：完成只读精简决策表；将架构结论写入相关 future 阶段计划提纲；在 Stage 7-RR 试点拆分 Plan 文档。RR-0–RR-6 的职责与完成定义以[plan §6](stage_plans/active/stage-07-rr/plan.md#6-工作分解与依赖)为准。

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

1. 产出只读精简决策表：模块 × 产品线/格式/时间/渲染维度 × 当前状态（production/candidate/test-only/dead）× 消费者 × 合同指针 × 删除/合并成本 × 阻塞项。
2. 将架构结论写入相关 future 阶段计划的新增语义（范围、约束、待决问题、验收方向）；继续使用提纲模板，不写实现批次、数值矩阵或实现落点。
3. 按[plan §6](stage_plans/active/stage-07-rr/plan.md#6-工作分解与依赖)为 RR-0–RR-6 建立任务卡，并产出 `schedule.md` 与 `acceptance.md`。
4. 实现并运行 `check_plan_schedule.py`；按上面的规则核验任务 ID 覆盖、依赖与波次、独占波次及调度表内容。
5. 做一次真实无上下文委派，派发时只提供一张任务卡，不附父对话或额外说明，验证任务卡能否独立完成对应任务；此项尚未执行。
6. 试点完成并评估后，再决定是否推广到其他 active plan；future 阶段启动前不创建任务卡或调度表。
7. 本轮只做设计与计划，不写实现批次、不改生产代码；完成试点后更新本页。

## 阻塞与待决

当前只读设计可继续，重构实施尚未获准；未决取舍见[设计附页](stage_plans/active/stage-07-rr/architecture-design.md)，阶段范围见[计划](stage_plans/active/stage-07-rr/plan.md)，发现状态见[登记册](stage_plans/active/stage-07-rr/findings.md)。

## 验证状态

本地文档检查通过：check_docs.py、check_docs_facts.py、gen_docs_map.py --check、git diff --check。产品构建与 CI 本轮未重跑。

## 最近改动

- HANDOFF 收敛为进度与状态入口；架构讨论归设计附页，阶段范围归计划，发现状态归登记册。

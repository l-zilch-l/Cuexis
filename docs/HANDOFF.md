# 当前交接

状态：active

更新日期：2026-11-22

<!-- cuexis-meta
id: STATUS-HANDOFF
status: active
authority: owns
-->

当前状态以 [CURRENT_STATUS](CURRENT_STATUS.md) 为准；阶段范围、发现与可实施性索引分别见下列文档，设计记录与提案见[下一步](#下一步)。

## 当前阶段与工作项

- [计划](stage_plans/active/stage-07-rr/plan.md)、[登记册](stage_plans/active/stage-07-rr/findings.md)、[可实施性索引](stage_plans/active/stage-07-rr/design-readiness.md)。

## 当前任务

下一次对话做正式实施前的计划检查：核对计划、任务卡、调度与阶段验收是否仍与已接受设计一致，找出实施前必须解决的偏差。本次检查不启动实施，也不派单。

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

1. 先读[计划](stage_plans/active/stage-07-rr/plan.md)、[架构设计附页](stage_plans/active/stage-07-rr/architecture-design.md)、三份候选合同与[验收](stage_plans/active/stage-07-rr/acceptance.md)，再按卡核对。检查项：
   - 设计基线：D-01–D-25 的已接受边界都有现行卡或具名延期承接；D-05 的 RR-3 仍只含选项 R1/R5/R6。
   - 任务卡：T-01–T-09 与 T-17 的目标、写域、步骤、验收覆盖所属 D 项和承接的 RR-F 编号；T-17 仍限定在 D-05 之外。
   - 合同对齐：每张卡引用的合同章节存在；合同里的可测规则都有验收落点，尤其 T-11 的表几何、选表与 `scoreMode`，T-14 的 F420 观察口径，T-06 的 F419／F135／F508。
   - 调度与验收：前置只引更早波次；A-01–A-06 覆盖全部现行卡；候选包仍标 `NEXT`，未被当成现行图。
   - 状态与指针：candidate／active 用词、证据路径、HANDOFF 与 CURRENT_STATUS 一致。
   - 轻量遗留：O2 的图例、O6 的术语定义、O12 的波次说明是否可接受；不可接受就列为实施前修正项。
2. 结论按“成立／部分成立／不成立”逐条写落点。文字级偏差直接修正；实质偏差回写[登记册](stage_plans/active/stage-07-rr/findings.md)并交 owner 裁定，不改设计范围。
3. 检查通过后，实施启动仍须 owner 明确授权；从 [T-01](stage_plans/active/stage-07-rr/tasks/T-01.md) 起按[调度](stage_plans/active/stage-07-rr/schedule.md)绑定基线、登记采用。

## 阻塞与待决

- 实施未授权；`NEXT` 包单独采用。
- 22 个提交未推送，等 owner 决定。
- RR-F401 随 T-16，RR-F420 判据随 T-14，RR-F510 候选能力待晋升裁定。
- O2／O6／O12 三处轻量处理待 owner 认可或列入修正项。
- [实质冲突登记](stage_plans/active/stage-07-rr/findings.md)、[阶段停止条件](stage_plans/active/stage-07-rr/plan.md#10-回滚阻塞与停止条件)、[后继采用边界](stage_plans/active/stage-07-rr/plan.md#唯一派单授权入口)。

## 验证状态

最近一次全量门禁在 `2d0e9f3d`（2026-11-22 21:14）通过：`python -B tools/check_docs.py`（769 篇）、`python -B tools/verification/check_docs_facts.py`（SDK 0.9.1、构建 26.11.18-1）、`python -B tools/verification/check_plan_schedule.py`（10 张现行卡、7 张候选卡）、`git diff --check`。工作树干净，HEAD 为 `2d0e9f3d`，领先 `origin/stage-7-rr` 22 个提交。历史回执见[本轮检查](stage_reports/stages/stage-07-rr/2026-11-22-checkout-review-resolution.md#执行记录)。

## 最近改动

- [文档描述优化收口](stage_plans/active/stage-07-rr/plan.md)：O1–O14 按 owner 决定落地，两组 P 类订正应用 42 条（2026-11-22）。
- [独立 checkout 复审登记](stage_plans/active/stage-07-rr/findings.md#16-独立-checkout-复审处置2026-11-22)。
- [设计缺陷处置](stage_plans/active/stage-07-rr/findings.md)：RR-F523–RR-F529 按最小改法落地，含 A-04 阶段口径、F419 排入 T-06、F401 归 T-16、F420 归 T-14 与无音轨终点订正（2026-11-22）。
- [旧问题承接](stage_plans/active/stage-07-rr/findings.md)：§22 按承接方归组，T-09 按该表写具名延后（2026-11-22）。
- [T-17 单独立项](stage_plans/active/stage-07-rr/tasks/T-17.md)：会话层 P1 正确性（RR-F501/F508/F509）单独成卡，未启动实施（2026-11-22）。
- [Seek 路由订正](stage_plans/active/stage-07-rr/findings.md)：三份历史文档与 ADR 0043 的指针改指 candidate 权威，F510 改述为候选能力、无 active 合同（2026-11-22）。

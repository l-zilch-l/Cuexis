# 当前交接

状态：active

更新日期：2026-11-18

<!-- cuexis-meta
id: STATUS-HANDOFF
status: active
authority: owns
-->

本页是**对话级交接**：记录当前任务、下一步、阻塞与验证状态，供新对话直接接手。
阶段事实仍以 [CURRENT_STATUS](CURRENT_STATUS.md) 为准，问题清单以
[Stage 7-RR 登记册](stage_plans/active/stage-07-rr/findings.md) 为准。

## 当前阶段与工作项

- 阶段：Stage 7-RR
- 工作项：`stage_plans/active/stage-07-rr/plan.md`
- 登记册：`stage_plans/active/stage-07-rr/findings.md`
- 仓库：`E:\Cuexis-worktree`（已从 D: 迁移；DSH 工作区已绑定 E）
- 分支/PR：`stage-7-rr` / PR #47（取代因分支改名而关闭的 #46）；SDK API `0.9.1`；构建 `26.11.18-1`
- 本地/远程：`stage-7-rr` 已推送并跟踪 `origin/stage-7-rr`；PR #47 取代 #46
- 本地分支：`stage-7-rr`（当前）、`master`、2 个 worktree 分支、4 个未合并 `r4-*`；
  已删除 12 个已合并旧分支
- 当前状态：文档重构与整理已收口，门禁全绿；**没有进行中的实现批次**

## 当前任务

下一对话承担**项目架构的精简设计、Plan 文档拆分与后续阶段计划调整**，只做设计与计划，
**不实施代码**：确定哪些产品线/格式线/时间基/渲染路径/会话层保留、合并或退役；把结论写成
未来阶段计划可消费的语义（范围、约束、待决问题、验收方向）；并按下面的方案拆分 active plan。
不写实现批次、不改生产代码。

## 已知架构事实（下一轮起点）

- 默认主线 A：`Player -> PlaybackSession -> Chart v1–v4 + CXC v1 -> Runtime/OpenGL`。
- candidate 链 B：`Player --execution-test-only -> ExecutionHost -> Judgement + gameplay_packed`；
  它有自己的所有权线程、时钟（`RealtimeClock`）、提交门与帧循环。
- 默认不可达但默认编译：`cuexis_chart` 的 packed/cxt_v2/candidate 约 **40.9%**（7,302/17,864 行）；
  `gameplay_packed` 4,821 行；`judgement` 20,518 行无默认生产消费者。
- 时间：`PlayerClock`、`RealtimeClock`、`HostClock`、`CuexisAudio` 四个实现；A/B 链 offset 语义不等价，
  `tickScale` 未被 playback 读取。
- 渲染：B 链（presentation_renderer）是生产路径；A 链 `renderFrame` 只剩“清一帧”，
  `renderPresentationFrame` 是绕过校验的半入口。
- 会话：`RuntimeSession` 只被 `PlaybackSession` 调用 9 个方法；`ExecutionHost` 与 Playback 有 11 项
  重复职责；公共头随 `CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE` 给出 39/51 两条声明面。
- Chart v1–v3 Reader、v1/v2→v4 迁移受 **ADR 0041** 保护，不能当死代码删；CXC v1 是 **accepted**
  当前容器；Packed/CXT v2/Chart v5 是 candidate，默认零入口。

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

1. 读 [架构研究](stage_reports/stages/stage-07-rr/2026-11-17-architecture-research.md)、
   [架构证据](stage_reports/stages/stage-07-rr/2026-11-17-architecture-evidence.md)、
   [Chart 表示与转换点](architecture/CHART_REPRESENTATIONS.md)、本阶段 plan/findings 与 ADR 0041/0042。
2. 产出**只读盘点版“精简决策表”**：模块 × 产品线/格式/时间/渲染维度 × 当前状态
   （production/candidate/test-only/dead）× 消费者 × 合同指针 × 删除/合并成本 × 阻塞项。
3. 在 Stage 7-RR 试点 Plan 拆分：RR-0–RR-6 拆成任务卡、写 `schedule.md` 与 `acceptance.md`；
   用一轮真实子代理任务验证“只读任务卡可独立完成”，再决定是否推广。
4. 把精简结论写成未来阶段计划的**新增语义**（范围、约束、待决问题、验收方向），future 计划仍用
   提纲模板；不写批次、数值矩阵或实现落点。
5. 涉及格式/合同退出的，先立/改 ADR 或 proposal，取得 owner 接受；本轮不改生产代码。
6. 结束时更新本页；owner 未决定前不得进入实现。

## 阻塞与待决

- **B 链去留**：`ExecutionHost + judgement` 是否作为产品保留？决定 Packed/CXT v2/Chart v5 的去向。
- **候选格式**：Packed、CXT v2、Chart v5 是提升为 active、继续 candidate，还是删除/归档？
- **时间语义**：是否统一 A/B 的 offset 与时钟抽象；`tickScale` 是否进入合同。
- **渲染**：是否删除 legacy `renderFrame` 与 `renderPresentationFrame` 半入口。
- **公开面**：是否消除 `CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE` 造成的 39/51 双声明面（ADR 0042 冲突）。
- **RR-3 路线**：已定 R1+R5+R6；R2/R3/R4 deferred，是否在本轮重启。
- 以上都需要 owner 决策；未决定前只做盘点与选项，不落实现。

## 验证状态

- 文档门禁：`check_docs.py` 0（740 篇）、`check_docs_facts.py` 0（版本漂移 0）、
  `gen_docs_map.py --check` 0、`check_tools_inventory.py` 0（30 scripts）。
- 合同测试：status / target / section 三个 contract tests 全 OK；schema `--check` OK；
  `update_version.py --check` 一致（`26.11.18-1`）；`git diff --check` 0。
- CI：`e1059507` 的 8 个 run 全 success；PR #47 的 CI 正在进行（pending）；#46 已因分支改名关闭。
- 构建已按 UTC 2026-11-18 推进到 `26.11.18-1`；本地日期与 UTC 跨日时，先检查 UTC 再推。

## 最近改动

- 文档体系：HANDOFF、plan/report/handoff 模板、PR 模板；active/future 两级 plan 规则。
- 分支：`stage-7.5` 重命名为 `stage-7-rr`；PR #46 关闭，新建 PR #47。
- 清理：删除 12 个已合并本地分支；保留 `master`、2 个 worktree 分支和 4 个未合并 `r4-*`。
- 计划：Stage 7-RR 计划重写；7 份 future plan 改为提纲；登记 D-04/D-05（RR-3 先 R1+R5+R6）。
- 文档事实：版本漂移归零；证据 meta 检查；Chart 表示与转换点页；`CHART_V4`/`PACKED` 两处失真修正。
- 代码/工具：Judgement seek 按 candidate 收口；删除 `engine/particles`；`cuexis_debug` 标 test-only；
  `check_tools_inventory.py`；`test_measurement_seek.py` 迁移；构建身份 `26.11.18-1`。

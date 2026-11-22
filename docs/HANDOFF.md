# 当前交接

状态：active

更新日期：2026-11-23

<!-- cuexis-meta
id: STATUS-HANDOFF
status: active
authority: owns
-->

## 当前阶段与工作项

当前阶段为 Stage 7-RR；进度以 [CURRENT_STATUS](CURRENT_STATUS.md) 为准。接手入口为[计划](stage_plans/active/stage-07-rr/plan.md)、[任务索引](stage_plans/active/stage-07-rr/plan.md#任务索引)、[唯一调度](stage_plans/active/stage-07-rr/schedule.md)和[阶段验收](stage_plans/active/stage-07-rr/acceptance.md)。设计到任务的对应关系见[可实施性索引](stage_plans/active/stage-07-rr/design-readiness.md)，发现与裁定见[登记册](stage_plans/active/stage-07-rr/findings.md)。

## 当前任务

owner 最新要求是复查 plan 和 task 中可能阻碍 agent 执行的决策、待定项和不确定表述，确认后编写接手文档。本轮审查与订正记录见[接手复查报告](stage_reports/stages/stage-07-rr/2026-11-23-execution-readiness-handoff.md)；本页是下一位 agent 的执行入口。

工作区为 `E:\Cuexis-worktree`，分支 `stage-7-rr`。本次审查时的历史 HEAD 为 `db6042b659139ac86bb52192e18132309c2aab27`，当时输入另含尚未提交的文档和三份新报告。owner 随后要求提交并推送，这些输入随本次文档提交一同交付；接手须取得包含本页及三份报告的后继提交，不能只 checkout 审查前的旧 HEAD。开始实施时重新核对实际 HEAD/tree 与工作树，按 T-01 绑定输入。生产实现尚未启动。

### 接手时沿用的决定

- 本轮统一实施 T-01–T-17；`NEXT` 是实验能力的范围标记，`candidate` 表示未发布或未实现，均不表示重新等待范围裁定。全面结构清理归属和具体验收见[登记册 §24](stage_plans/active/stage-07-rr/findings.md#24-统一实施前设计裁定2026-11-23)。
- RR 完整实现并实验验证 Gameplay v3；Stage 7.5 完整范围验收后，Stage 8 以 Chart v5＋Gameplay v3 为正式主线。已承诺旧内容按原规则、时间与身份兼容；不能用隐式迁移或重新编译代替旧成品直读。旧报告中的 v2 正式目标已被[最新续议](stage_plans/active/stage-07-rr/findings.md#stage-8-正式主线续议2026-11-23)取代。
- v5 沿用 CXT2、Clip2、Behavior Event2 和整数 `render.alpha` 的原设计；新候选不接受旧动画作者语法。设备等待的计时、超时判断及处置全部由 Player 实现，SDK 不实现这套机制。
- F501 的有效性与独立提交检查、F135 的分配失败/容量/观测缺失区分、F203 的登记前整批验证均已接受，分别按 T-03/T-06/T-17 及其合同执行。
- 公共 API 逐组兼容新增，旧入口在实际过渡发布、替代稳定性和消费者证据满足后才能获准退出；不预填统一退役版本或统一等待 Stage 14。仓内清理与本轮消费者验证按任务完成，默认 SDK 晋升和正式发行另行验收。

工作约定：按完整议题复用无父对话上下文的子代理，派单提供完整输入；避免为小问题新增维护框架；用清楚、自然的中文解释背景、操作与结果。局部实现和测试组织由 agent 决定，已接受规则不重复提问。

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

1. 若 owner 下一条指令明确启动实施，直接执行 [T-01](stage_plans/active/stage-07-rr/tasks/T-01.md)：固定实际 HEAD/tree、文档修订、补丁及相关未跟踪原文/哈希，取得当前冷启动回执，登记一次采用。本次文档复查不代替实施时的基线绑定。
2. 依[调度](stage_plans/active/stage-07-rr/schedule.md)推进。各卡先创建自己负责的 target/schema/测试，再按卡执行正反验收；后继卡只消费已交付的产物。T-02/T-05/T-17 允许按独立写域并行，共享 CMake、登记册、状态页和生成物由集成者串行汇合。
3. 每卡留下对应修订的实际证据，T-07 在实现汇合后整理文档，T-09 核全部验收与发现并提交阶段退出接受。不要将本轮已纳入的实验能力或全面结构清理整体延期。

## 阻塞与待决

本轮已完成 plan、全部任务卡及关联合同的复查和交叉复核；已发现的文档阻碍均已订正，未发现仍需 owner 先裁定的当前 RR 产品选择。具体修订与复核回执见上述报告。

实际运行失败、缺失的旧交付资料或新发现的合同冲突须据实记录。按 T-01 区分“有依据的不适用”和“已承诺但缺证据”，保持旧兼容行为；仅在需改变规则、兼容承诺或接受缺证据退出时询问 owner，暂停受影响切片，其余独立工作继续。不能把 future 提纲的发行矩阵、设备容量或版本决定当作当前 RR 的设计前置。

完整构建、容量探针、hosted 和设备长任务按仓库约定，先说明范围、环境可行性和预计时长，待 owner 选定后执行。阶段退出仍需 owner 接受；这些是执行与证据门禁，不能预先写成通过。

## 验证状态

本轮文档检查、事实检查（漂移 0）、生成物新鲜度、调度检查及其 27 项测试、diff 空白检查均通过；本地链接/章节锚无错误。调度副本的错误前置和缺失停止条件均按预期拒绝，正本哈希不变。两组独立复核及其增量复验均已返回，实际命令、修订与限制见[复查报告](stage_reports/stages/stage-07-rr/2026-11-23-execution-readiness-handoff.md)。

提交前另验日期版本一致性与当前版本门通过；版本检查器整套自测受 Windows 进程内 Git 配置环境恢复影响，出现 `missing config value GIT_CONFIG_VALUE_0`。原用例逐个独立进程复验：24 项通过，2 项因缺少可用 POSIX shell 按原条件跳过；未修改检查器或放宽断言。

本次提交交付文档，并按更新 PR 的仓库规则同步日期构建身份与生成路由；SDK API 与产品实现未改变。产品运行、安装消费者、窗口/设备和正式发行的通过状态必须由对应实施任务取得。提交和远端同步状态以当前 Git 记录为准。

## 最近改动

本次清除重复设计审批和候选范围移交的残留表述，统一逐组 API 迁移条件，并补齐工作树输入与旧兼容证据的交接规则。逐项修订及任务卡交叉复核见[复查报告](stage_reports/stages/stage-07-rr/2026-11-23-execution-readiness-handoff.md)。

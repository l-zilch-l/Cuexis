# Stage 7-RR Plan: Review and Refactoring After Stage-7

状态：active；Stage 7 与 Stage 7.5 之间的独立复核与架构减重重构阶段

更新日期：2026-11-17

<!-- cuexis-meta
id: PLAN-active.stage-07-rr.plan
status: active
authority: owns
-->

本计划覆盖 Stage 7 受限退出之后、Stage 7.5 产品实施之前的复核、合同收敛、架构减重与交接；不覆盖 Stage 7.5 的能力演进、GE-R 性能整改与发布。阶段进度以 [CURRENT_STATUS](../../../CURRENT_STATUS.md) 为准，当前任务与下一步以 [HANDOFF](../../../HANDOFF.md) 为准；问题清单以[发现登记册](findings.md)为准，报告入口见 [Stage 7-RR 报告索引](../../../stage_reports/stages/stage-07-rr/README.md)。

归档来源：2026-11-17 全项目只读复核的 10 份子代理报告与 owner 抽查记录（会话内记录，未逐份入库；结论逐条转写进 [findings](findings.md)并标注是否经独立复核）；同日 8 份架构研究子代理报告（依赖分层、双产品线、格式线、时间源、渲染路径、会话层次、文档与注释、公共 API 面）；上一轮[源码拆分安全网独立审计](../../../stage_reports/stages/stage-07/90-shared/source-split-audit/index.md)；[Stage 7 受限退出与缺陷交接](../../../stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-14-limited-exit-and-defect-handoff.md)。本阶段已落盘的研究、证据与审计报告见第 11 节；返回[阶段计划索引](../../README.md)。

## 规划前必须回答的问题
1. 可验收成果是什么、哪些明确不做？见第 2、13 节。
2. 依赖哪些冻结合同、证据与构建？不满足时的停工判据是什么？见第 3、10 节。
3. 哪些事项 owner 已决定、哪些必须再问？见第 4 节。
4. 关键路径与批次依赖是什么、哪些批次可并行？见第 6 节。
5. 不可逆点、迁移点与兼容断点在哪，失败如何回滚？见第 5、10 节。
6. 每个批次的完成定义、正反例证据与验收命令是什么？见第 7、9 节。
7. 预算、上限与版本数字的权威源在哪？本计划只链接，不复述。
8. 会产出哪些报告、放在哪、用哪个模板？见第 11 节。
## 1. 计划定位与边界
`RR` 只表达路线位置（Stage 7 之后、Stage 7.5 之前），不是 Stage 7 的子阶段，也不改变 Stage 8–14 的编号；与 [Stage 7-RPA 计划](../../completed/stage-07/05-rpa-new/plan.md)不是同一对象。本阶段不重开 Stage 7 的受限退出判定，不改写其历史失败、未认证范围或 testOnly 状态。

```text
Stage 7（受限退出） -> Stage 7-RR（本阶段） -> Stage 7.5（Gameplay Evolution） -> Stage 8
```
[Stage 7.5 计划](../stage-07-5/plan.md)的入场前置已包含“本阶段当前计划范围完成并取得 owner 接受”；本阶段只交付复核、合同收敛、减重决策与交接，不决定 Stage 7.5 的能力范围或实施顺序。

- [发现登记册](findings.md)是问题与证据状态的唯一清单；本计划不复制其正文，只用 ID 引用。`docs/stage_reports/stages/stage-07-rr/` 下的报告是证据，不是权威，不据报告的“下一步”推断当前任务。
- 本计划是执行合同：批次范围、先后、验收与停止条件以本计划为准；发现问题仍回写 findings。本计划不决定 SDK minor、发行版本号、合并与发布；稳定 C ABI 归 Stage 14，需要修订 ADR/Spec 时按第 10 节由 owner 接受。
## 2. 阶段目标
### 可验收成果
1. **结构证据与减重决策**：交付依赖分层、格式线、时间源、渲染路径、会话层次与公共 API 面的代码证据，并对每个合并/下沉/删除/标 candidate 的对象交付具名决策、受影响合同、历史证据处置与回归证据；判定：RR-0 研究/证据报告落盘，RR-3/RR-4 退出报告与删除/合并清单可逐项核对。
2. **状态与合同一致**：现行文档不再与版本、分支、阶段归属、格式状态和 SDK 陈述冲突，历史值显式标为历史；“文档承诺存在而实现不存在”或“实现比文档更严/更松”的条目被修正，无法实现的标为未实现；判定：RR-1 文档门禁全过且无事实冲突，RR-2 每条有 Spec/ADR 裁定或“未实现”标签。
3. **失败语义统一**：同一失败在文档、实现与宿主观测面得到同一个稳定码，静默失败被消除或显式登记；判定：RR-2 的正反 fixture 与受影响测试通过。
4. **交接可接手**：结论、处置理由与残留缺口写入 Stage 7.5/Stage 8 交接，不留在会话记录里；判定：RR-6 退出报告与 HANDOFF/CURRENT_STATUS 指针一致。
### 非目标
- 不追求“所有 P2 清零”或“结构无技术债”；P2 可 deferred，但须写明归属阶段与恢复条件。
- 不提升 gameplay、渲染、音频或性能能力；不新增格式、capability、SDK 或生产保证范围。
- 不为行数预算拆文件；owner 已取消纯行数拆分，行数只作为 findings §4 的事实记录。
- 不删减历史证据来让文档“看起来瘦”；减重不等于删证据。
## 3. 前置基线、依赖与入场门禁

| 依赖 | 当前状态 | 权威指针 | 不满足时的动作 |
| --- | --- | --- | --- |
| 代码/合同复核基线 `aac83e5e`（2026-11-16） | 已固定 | [CURRENT_STATUS](../../../CURRENT_STATUS.md)、[架构研究](../../../stage_reports/stages/stage-07-rr/2026-11-17-architecture-research.md) | 代码/schema/CMake 变动后重跑证据命令；旧报告标为历史，不把旧结论外推到新基线 |
| SDK API `0.9.1` | 已固定 | [CuexisVersion.cmake](../../../../cmake/CuexisVersion.cmake) | 停止安装面/ABI 批次，先取得版本决定 |
| 日期构建 `26.11.18-1` | 当前权威 | 同上；早期报告中的构建字面量是历史写作时点 | 以版本文件为准，不回写旧报告 |
| Stage 7 受限退出交接 | 已接受 | [R4 交接](../../../stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-14-limited-exit-and-defect-handoff.md) | 不重开判定；受影响批次改为只登记、不改写 |
| Stage 7.5 入场前置 | 待本阶段接受 | [Stage 7.5 计划](../stage-07-5/plan.md) | 本阶段未取得 owner 接受前，7.5 不开始产品实施 |
| 文档门禁与证据 | 已接入 | [BUILDING](../../../guides/BUILDING.md)、[发现登记册](findings.md)、[报告索引](../../../stage_reports/stages/stage-07-rr/README.md) | 门禁失败即停该批；条目未复核或报告缺失时，不据此删代码、不改合同 |

### 入场门禁（代码开始门槛）

| 门禁 | 允许开始 | 禁止 |
| --- | --- | --- |
| 基线与合同冻结 | 文档订正、证据补证、非 wire 的合同澄清 | 改变 wire 字节、接受集合或稳定码语义；未取得 ADR/Spec 接受就动实现 |
| RR-0 完成且 owner 选定 RR-3 选项 | 按选定选项做删除/合并/下沉 | 未选定前做删除/合并；RR-2 与 RR-3 同时改同一对象 |
| 批次取得 Spec/ADR 裁定 | 实现 + focused tests + 正反 fixture | 以“代码已经是新行为”回写合同；让 Reader 静默接受两种解释 |
| 证据路径冻结 | 新增报告、追加 findings 处置 | 移动证据套件目录、改写已归档报告或回填旧 SHA |

### 当前进展基线（2026-11-17）
- RR-0 已完成：8 份专题结论见[研究](../../../stage_reports/stages/stage-07-rr/2026-11-17-architecture-research.md)与[证据](../../../stage_reports/stages/stage-07-rr/2026-11-17-architecture-evidence.md)，6 个减重选项 R1–R6 与建议路线已登记；选项未被 owner 选定，RR-3 未开始。
- RR-1/RR-5 的文档治理机械层已完成（[报告](../../../stage_reports/stages/stage-07-rr/2026-11-17-documentation-refactor.md)）：170 篇合同/计划补元数据、生成 MAP/AGENTS 块、424 篇证据报告加统一头、计划状态纠偏、7 处假断言订正、`check_docs_facts.py` 接入；合同语义重写与 R3/R4/R5 相关正文仍未开始。
- 假保证扫描与登记册全量核查已完成：前者 48 条候选、26 条经 owner 复核确认（4 条文档过期已就地修正），最高危的 `ChartWriter` 保存丢数据（RR-F401）与三个常量诊断字段（RR-F419/F420）未开始修复，处置顺序见扫描报告末节；后者条目全部有结论，复核中推翻或收窄了若干断言，未复核项已清零；具体计数与分级以 findings 为准。
- tools/ 审计完成（[报告](../../../stage_reports/stages/stage-07-rr/2026-11-17-tools-audit.md)）：120 个脚本审计完成，证据套件原位冻结、门禁修补、报告来源登记，RR-F511–RR-F520 的已确认缺陷已修或登记；日期构建以版本文件为准，本阶段早期报告的构建字面量是历史值。
## 4. 已确认方向与决策台账

| 决策 ID | 决策 | 日期 | owner | 影响 |
| --- | --- | --- | --- | --- |
| D-01 | Stage 7-RR 是 Stage 7 与 Stage 7.5 之间的独立阶段；`RR` 只表达路线位置，不改变 Stage 8–14 编号 | 2026-11-17 | owner | 全计划；与 Stage 7-RPA 区分 |
| D-02 | 本阶段以架构减重重构为主；文档与代码注释纳入减重范围 | 2026-11-17 | owner | RR-3、RR-5 |
| D-03 | 取消文件拆分工作；行数预算只作为事实记录保留在 findings §4 | 2026-11-17 | owner | 第 13 节排除项；RR-3/RR-4 不做拆分 |
| D-04 | 合同先于实现：改变 wire、接受集合或稳定码语义前，先取得对应 ADR/Spec 接受 | 2026-11-17 | owner | RR-2、RR-3、RR-4 |
| D-05 | RR-3 减重路线：先做 R1 构建隔离、R5 渲染收敛、R6 文档减重；R2/R3/R4 记入 deferral，待本轮退出后再评估 | 2026-11-17 | owner | RR-3 范围；RR-6 交接需列出 deferred 项与恢复条件 |

以上只列有证据的已决项。RR-F510、版本漂移策略与 particles/debug 清理已在 2026-11-17 按 owner 决定处置，见变更记录与 findings。

### 待 owner 决策

当前无待决项。RR-3 路线按 D-05 执行；若 R2/R3/R4 需要重启，另立决定并更新本表。

## 5. 假设、约束与风险

| 假设/风险 | 失效信号 | 缓解/回滚 | 责任 |
| --- | --- | --- | --- |
| `aac83e5e` 的代码/合同事实仍适用于当前工作树 | 产品代码、schema 或 CMake 在基线后变动 | 重跑证据命令；旧报告标为历史；不把旧结论外推 | RR-0/RR-1 |
| findings 的“已核实/未复核”状态足以支撑处置 | 未复核条目被直接用于删代码或改合同 | 先补证并写入报告；未补证不动 | RR-2/RR-3 |
| 减重不会破坏受限退出边界与历史证据 | 证据 manifest 路径变化、报告被改写、回归失败 | 删除/合并清单 + 合同清单 + 回归证据；失败回滚新增入口 | RR-3/RR-6 |
| RR-2 的合同裁定能及时给出 | 某条目长期无 Spec/ADR 裁定 | 先标“未实现/待裁定”，不把结构改动塞进同一批 | RR-2 |
| 文档门禁通过等于文档正确 | 机械检查通过但正文仍与实现不符 | 批次报告人工复核 + 事实检查 + 阴性对照 | RR-1/RR-5 |
| 默认构建的测试覆盖足以证明回归 | 关键测试只在 candidate 宏下注册或零生效 | 按报告记录的手工复现命令；在报告中显式写未覆盖 | RR-2/RR-6 |

**不可逆点**：证据套件相对路径已进入验收哈希账本（`binding.py`、`prepare_*` 的 glob、`etw_safety` 逐文件封包哈希），移动目录会让历史验收记录永久失配；`docs/stage_reports/**` 与 `docs/archive/**` 是冻结证据，不重写历史结论、不回填自身 SHA；SDK `0.9.1` 的安装头/对象布局与 C ABI 一旦发布不能静默改变；格式 wire、语义 identity 与稳定诊断码一经接受即成为互操作契约，变更必须先有 ADR/Spec 并以 fixture/hash 固化；`findings.md` 的 ID 与结论列不删除、不重编号，处置只追加。
## 6. 工作分解与依赖
### 任务看板

| ID | 任务 | 前置 | 主要产物 | 完成定义 |
| --- | --- | --- | --- | --- |
| RR-0 | 架构研究（已完成） | 无 | [研究](../../../stage_reports/stages/stage-07-rr/2026-11-17-architecture-research.md)、[证据](../../../stage_reports/stages/stage-07-rr/2026-11-17-architecture-evidence.md)、findings §6 | 8 个专题有结论；6 个选项 R1–R6 与建议路线有证据；不改产品代码 |
| RR-1 | 状态与文档漂移 | 无（可与 RR-2/RR-3 并行） | 当前文档订正、历史基线标注、版本漂移处置 | 现行文档与 `CuexisVersion.cmake`、`vcpkg.json`、分支事实不冲突；历史值标为历史 |
| RR-2 | 合同与诊断收敛 | 每条先有 Spec/ADR 裁定 | 合同修订、focused tests/fixtures、findings 处置 | 每条有裁定或“未实现”标签；同一失败一个稳定码；静默成功消除或登记 |
| RR-3 | 架构减重 | RR-0 完成 + owner 选定选项 | 选项决定、删除/合并清单、合同影响清单、回归证据 | 每个对象有具名决策 + 合同清单 + 回归；不破坏受限退出边界 |
| RR-4 | 公共面收缩 | RR-2 对 candidate 入口的裁定；R1 构建隔离（如选） | 安装头/安装集/版本权威订正 | production 安装头不暴露 candidate 入口；external consumer/reference host 门禁通过 |
| RR-5 | 文档与注释减重 | 无（可与 RR-2/RR-3 并行） | 单一权威、过程文档处置、注释对齐与检查 | 机械断言类内容有测试保护或被删除；注释失真样例清除并加入检查 |
| RR-6 | 退出与交接 | RR-1–RR-5 退出或有明确 deferral | 退出报告、删除/合并清单、Stage 7.5/Stage 8 交接 | owner 接受退出报告；CURRENT_STATUS/AGENTS/ROADMAP/索引同步；不声称未取得的结论 |

关键路径：RR-0 → owner 选定 RR-3 选项 → RR-3 → RR-6。RR-2 的裁定是 RR-3 同对象修改与 RR-4 candidate 宏处理的前置；RR-1、RR-4、RR-5 可与 RR-2/RR-3 并行，但 RR-2 与 RR-3 不得在同一对象上同时动工（先定语义，再动结构）。当前最先卡住全盘的是 RR-3 选项未选定；RR-0 已完成，RR-1/RR-5 的机械层已完成，剩余工作集中在 RR-2、RR-3、RR-4 与 RR-6。
## 7. 分批细化
### RR-0：架构研究（已完成）
- **范围与任务**：只读研究依赖分层、双产品线、格式线、时间源、渲染路径、会话层次、文档/注释与公共 API 面；汇总实测计数与违规边，提出 R1–R6 与建议路线，条目并入 findings §6。
- **落点与证据**：[研究](../../../stage_reports/stages/stage-07-rr/2026-11-17-architecture-research.md)、[证据](../../../stage_reports/stages/stage-07-rr/2026-11-17-architecture-evidence.md)、[findings §6](findings.md#6-架构层发现2026-11-17-研究)。
- **退出条件**：研究/证据报告落盘；选项有证据；owner 未选定前 RR-3 不进代码。
### RR-1：状态与文档漂移
- **范围与任务**：P0/P1 中的版本、分支、阶段归属、格式状态与 SDK 陈述；对齐当前事实，历史基线标为历史，按 owner 决定处置剩余版本漂移；不含合同语义重写。
- **落点与证据**：[CURRENT_STATUS](../../../CURRENT_STATUS.md)、[ROADMAP](../../../ROADMAP.md)、`docs/formats/README.md`、`docs/api/*`、[LIMITS](../../../guides/LIMITS.md)、`check_docs_facts.py`；证据为[文档治理报告](../../../stage_reports/stages/stage-07-rr/2026-11-17-documentation-refactor.md)与 RR-1 退出报告。
- **退出条件**：`check_docs.py`、`check_docs_facts.py`、`gen_docs_map.py --check` 通过；现行文档无事实冲突；历史值显式标为历史；owner 接受版本漂移处置。
### RR-2：合同与诊断收敛
- **范围与任务**：Packed Header CRC/GridDelta、Chart Beat 上限、CXC playback entry、CXT v2 §6 未实现面；`same_tick_collision` 码互换、`catch(...)` 三态、`player.execution.test_only` 单码、`packed.io.*` 双入口；每条先取 Spec/ADR 裁定，再改实现或标“未实现”，补前后 fixture 与 focused tests，处置回写 findings §9。
- **落点与证据**：`docs/formats/*`、`docs/adr/*`、`docs/api/*`；`engine/chart`、`engine/playback`、`engine/judgement` 与 `tests/`；证据为批次报告 + 前后 fixture + findings 状态回写。
- **退出条件**：每条有裁定或“未实现”标签；同一失败只有一个稳定码；“静默成功”路径被消除或显式登记；受影响测试通过，失败/回滚路径不被削弱。
### RR-3：架构减重
- **范围与任务**：按 owner 选定的 R1–R6 处置格式线最小集、双产品线、时间源、渲染路径、会话层与文档；列删除/合并/下沉清单（对象、服务的 ADR/Spec、受影响测试与证据、移除理由），实施构建隔离与结构收敛，跑回归并更新受影响合同；不与 RR-2 同对象并行。
- **落点与证据**：`CMakeLists.txt`、`CMakePresets.json`、`engine/**`、`app/**`、`docs/architecture/*`、`docs/formats/*`；证据为 RR-3 退出报告 + 删除/合并清单 + 回归测试/预设结果。
- **退出条件**：每个被删/合并对象有具名决策、合同清单与回归证据；不破坏 Stage 7 受限退出边界；不把历史失败改写为通过；未取得 hosted/跨平台证据的结论不声称。
### RR-4：公共面收缩
- **范围与任务**：candidate 入口移出生产安装面、`cuexis_internal/` 不外泄、零消费者符号处理、版本权威单一化；盘点安装头与安装集，移出/加门 candidate 入口，清内部头泄漏，统一版本权威，验证 external consumer 与 reference host；不决定 SDK minor 与发行。
- **落点与证据**：`include/cuexis/playback/*`、安装集与 `cmake/VerifySharedExports.cmake`、`cmake/VerifyArchitecture.cmake`、`cmake/CuexisVersion.cmake`、`examples/reference_host/*`、`docs/api/*`；证据为安装头 diff + 构建/外部消费者日志 + RR-4 报告。
- **退出条件**：production 构建的安装头不暴露 candidate 入口；缩面不影响 external consumer/reference host 门禁；版本只有一个权威源；[ADR 0042](../../../adr/0042-stage-6-productization-boundaries.md) 的“开关不改变公开类定义”冲突被解决或显式接受。
### RR-5：文档与注释减重
- **范围与任务**：同一合同只保留一处权威、过程文档按 owner 政策处置、注释与实现对齐；合并/去重合同表，按决定处置过程文档，清理失真注释，为可机械断言内容加保护，复核 7 处已修假断言不复发；不做无证据删文档。
- **落点与证据**：`docs/formats/*`、`docs/api/*`、`docs/architecture/*`、`docs/guides/*`、代码注释、`tools/check_docs.py`、`tools/verification/check_docs_facts.py`；证据为 RR-5 报告 + 检查器输出 + 注释抽样记录。
- **退出条件**：机械断言类内容有测试保护或被删除；注释失真样例被清除并加入检查；owner 接受过程文档处置。
### RR-6：退出与交接
- **范围与任务**：汇总处置、残留缺口、Stage 7.5/Stage 8 交接；逐条列 P0/P1 处置，P2 写归属阶段与恢复条件，汇总删除/合并清单，更新 CURRENT_STATUS、AGENTS、ROADMAP、索引与 HANDOFF，写退出报告；不新增修复。
- **落点与证据**：`docs/stage_reports/stages/stage-07-rr/` 的退出报告、[CURRENT_STATUS](../../../CURRENT_STATUS.md)、[ROADMAP](../../../ROADMAP.md)、[HANDOFF](../../../HANDOFF.md)、阶段索引；证据为退出报告 + findings 最终状态 + 第 9 节命令输出。
- **退出条件**：owner 接受退出报告；P0/P1 全部有处置；不声称未取得的 hosted/跨平台/GPU/设备结论；文档门禁全过。
## 8. 统一任务交付模板
每个任务/PR 必须包含：

```text
Scope / Contract / Implementation
Positive evidence / Negative evidence
Compatibility and rollback evidence
Changed limits, diagnostics, capability
Known gaps and next-task handoff
```

没有失败证据、兼容证据或明确 handoff 的任务不算完成；只改文档的任务必须写“只改文档”并列出所跑检查。
## 9. 验收标准
每个批次退出时同时具备：发现条目的具名处置（fixed / deferred with reason / rejected with evidence）；实现与 focused tests（或“只改文档”的明确说明）；失败与回滚路径不被本次修改削弱；文档、状态与交接同步。本阶段整体退出还需：P0/P1 全部有处置，P2 的 deferred 写明归属阶段与恢复条件；有删除/合并清单；受影响测试全绿且改变接受集合的修复附前后 fixture；不声称未取得的 hosted、跨平台、GPU 或设备结论。
### 验收矩阵

| 验收项 | 命令/方法 | 期望 | 证据类型 | 接受者 |
| --- | --- | --- | --- | --- |
| 文档结构与链接 | `python -B tools/check_docs.py` | 退出码 0；无坏链、无缺章节 | 终端输出 | owner/CI |
| 生成物与版本事实 | `python -B tools/check_docs_facts.py` | 退出码 0；版本漂移警告按 owner 决定处置 | 终端输出 | owner/CI |
| 地图新鲜度 | `python -B tools/verification/gen_docs_map.py --check` | 退出码 0 | 终端输出 | owner/CI |
| 空白错误 | `git diff --check` | 退出码 0，无 whitespace error | 终端输出 | owner/CI |
| 代码格式 | `cuexis_format_check`（按 [BUILDING](../../../guides/BUILDING.md)） | 通过 | 日志 | owner/CI |
| 批次报告落盘 | 报告路径存在，且含结论/范围与方法/执行记录/结果与证据/限制与未做/对登记册与合同的影响 | 报告可链接、结论按四档分级 | 报告 | owner |
| findings 状态回写 | 逐条核对 P0/P1 的 fixed/deferred/rejected 与证据链接 | 无“未复核”或“已修复”无证据 | findings diff | owner |
| 代码批次回归 | 按 [BUILDING](../../../guides/BUILDING.md) 执行受影响 target/CTest | 全部通过；失败与回滚证据入报告 | CTest/日志 | owner/CI |
| 外部消费者/参考宿主 | 按 BUILDING 的 external consumer/reference host 门禁 | 通过 | 日志 | owner/CI |

### 正例与反例
- 正例：新增/保留模板要求的 H2 后 `check_docs.py` 为 0；反例：删掉“验收标准”章节会失败。
- 正例：findings 条目带报告链接、命令输出与前后 fixture 后标 fixed；反例：只写“已修复”会被退回。
- 正例：证据套件原位保留并被报告引用；反例：移动目录导致历史 manifest 失配，必须回滚。
- 正例：合同修订先有 ADR/Spec 接受；反例：实现先行、事后回写合同会被拒绝。
## 10. 回滚、阻塞与停止条件
1. 合同变更未取得接受：停止受影响实现，保留最小复现，回到最后接受的 revision。
2. 确定性失败（跨编译器/平台差异、未声明浮点、溢出、容器顺序依赖）：阻塞该批，不以“环境差异”放行。
3. 预算/容量失败：区分内容、稳态、快照与 Replay；不得事后缩小 fixture 或改口径。
4. 安全失败（不可信输入导致未界定循环/IO/过量分配）：稳定拒绝并保留旧 active 状态。
5. 集成失败：回滚新增入口，不删除旧路径；旧路径的退出按 [ADR 0041](../../../adr/0041-legacy-format-exit-policy.md) 单独排期。
6. owner 未接受 ADR/Spec/例外：状态保持 planned/active/blocked，不把“待决定”写成已完成。
7. RR-3 选项未选定：停止任何删除/合并/下沉，只做研究、文档与 RR-2 的合同裁定。
8. 证据路径移动或已归档报告被改写：立即停止并回滚；不得用更新 manifest 来“修复”历史证据。
## 11. 证据与报告计划

| 报告 | 触发 | 路径 | 模板 |
| --- | --- | --- | --- |
| 报告索引 | 持续 | [README](../../../stage_reports/stages/stage-07-rr/README.md) | 索引页 |
| 架构与实现研究 | RR-0（已完成） | [2026-11-17-architecture-research.md](../../../stage_reports/stages/stage-07-rr/2026-11-17-architecture-research.md) | `docs/templates/report.md` |
| 架构与实现探索证据记录 | RR-0（已完成） | [2026-11-17-architecture-evidence.md](../../../stage_reports/stages/stage-07-rr/2026-11-17-architecture-evidence.md) | `docs/templates/report.md` |
| 文档治理重构 | RR-1/RR-5 机械层（已完成） | [2026-11-17-documentation-refactor.md](../../../stage_reports/stages/stage-07-rr/2026-11-17-documentation-refactor.md) | `docs/templates/report.md` |
| 假保证与假实现扫描 | RR-0/RR-2 输入（已完成） | [2026-11-17-false-guarantees-scan.md](../../../stage_reports/stages/stage-07-rr/2026-11-17-false-guarantees-scan.md) | `docs/templates/report.md` |
| 外部审查（59 条） | RR-0/RR-2 输入（已完成） | [2026-11-17-external-findings-review.md](../../../stage_reports/stages/stage-07-rr/2026-11-17-external-findings-review.md) | `docs/templates/report.md` |
| tools/ 目录审计 | RR-1/RR-4 输入（已完成） | [2026-11-17-tools-audit.md](../../../stage_reports/stages/stage-07-rr/2026-11-17-tools-audit.md) | `docs/templates/report.md` |
| 后续批次报告 | 每批退出 | `docs/stage_reports/stages/stage-07-rr/` 下带日期报告 | [report.md](../../../templates/report.md) |

报告只记录实际执行的命令、结果与限制；结论只来自证据报告，批次退出、删除/合并、合同变更、版本漂移豁免、RR-F510、particles/debug 清理必须由 owner 接受。不得用报告“下一步”推断当前工作；当前任务只看 [CURRENT_STATUS](../../../CURRENT_STATUS.md) 与 [HANDOFF](../../../HANDOFF.md)。
## 12. 交接与变更控制
- 每批结束或暂停时更新 [HANDOFF](../../../HANDOFF.md)：当前任务、下一步、阻塞与验证状态。计划变更必须先在本表登记；未登记的范围变化不算计划变更。

| 日期 | 变更 | 原因 | 决策依据 |
| --- | --- | --- | --- |
| 2026-11-17 | 按[详细规划模板](../../../templates/plan.md)重写本计划：新增规划问题、任务看板、分批细化、验收矩阵、回滚与变更控制；保留 RR-0–RR-6 语义、目标、排除项与归档来源 | 模板升级为详细规划协议；原计划缺少任务级交付与验收矩阵 | owner 2026-11-17 的模板与工作规则 |
| 2026-11-17 | 登记 D-04/D-05；关闭 RR-F510、版本漂移、particles/debug 三项待决；RR-3 只做 R1/R5/R6，R2/R3/R4 defer | owner 2026-11-17 统一决策 | owner |

## 13. 排除项与旧路径
- **不做纯行数拆分**：owner 已取消；行数预算只作为 findings §4 的事实记录，不为凑预算拆文件。
- **不做 Stage 7.5 能力演进**：不实现 S7B-0–4 / S7C-1/2，不做 GE-R 性能整改与预算测量。
- **不发布、不合并、不提升 SDK minor**：稳定 C ABI 归 Stage 14；不把 preview 承诺扩大。
- **不重跑、不改写历史失败与未认证范围**：不提高 caps，不补造证据，不把旧失败改成通过。
- **不修改判定窗口、格式 wire 布局或 identity 语义本身**：若必要，先立 ADR/Spec 修订，由 owner 接受后另行排期，不塞进重构批次。
- **不把复核发现写成已修复**：本计划登记问题与决策，不是修复声明；修复结论只写入带日期的报告。
- **不移动 tools 证据套件目录、不改写证据 manifest 路径、不重写已归档报告**；证据套件原位冻结。
- **旧路径**：[stage_plans/legacy-paths.md](../../legacy-paths.md)只登记旧路径映射，不在本计划重写合同；Stage 7 的 completed 计划与报告保持原位。
- **旧格式退出**：Chart v1–v4、CXT v1、CXC v1 等由 [ADR 0041](../../../adr/0041-legacy-format-exit-policy.md) 约束；不缩短兼容窗口，除非 owner 另有决定。

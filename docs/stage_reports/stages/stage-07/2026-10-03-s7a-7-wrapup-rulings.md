# 第 7 轮（收尾澄清与缺陷）裁定与收尾落地记录

状态：dated implementation record（第 7 轮裁定的文档落地证据；不是阶段关闭、不是 owner acceptance、
不是实施证据、**不是** Judgement 实现完成声明）

更新日期：2026-10-03

上级文档：[Stage 7A 实施计划](../../../stage_plans/active/stage-07/plan.md) §1.1 / §S7A-3 / §S7A-4 / §S7A-8 / §S7A-9 ·
[Gameplay V2 acceptance package](../../../proposals/gameplay-v2-acceptance/README.md) ·
[未决语义分轮裁决清单](../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) §7、§8、§9 ·
[批次门禁](../../../proposals/gameplay-v2-acceptance/BATCH_GATES.md) §1

本文是**带日期的落地记录**（reports own dated evidence）：它登记 Codex 对第 7 轮（收尾澄清与缺陷）的
**16 行 / 按项计 18 项**裁定、`S7A7-R01…R16` 编号、逐条落进哪些文档的哪一节、五条残余 `open` 合同项的
处置词补齐、计数变化与行 / 项口径订正、`D-9`（owner-only）与 `D-12`（待 owner 确认）的明确收尾状态、
`D-13` / `D-14` 的最终证据指针，以及本轮跑过的门禁与结果。它不复制 Spec / ABI 的合同正文，也不构成
任何实施声明；未在本文列出的门禁一律视为未运行。

> **表述纪律。** 本轮冻结的是**文档合同与门禁**。**不得**把本文或本轮写成"Judgement 实现完成"或
> "Stage 7A 完成"，也不得据本文声称 hosted / GPU / 真实设备 / 音频证据已通过。**裁定完成不等于实现完成**：
> 实现批次 S7A-3…S7A-9 仍未完成。

## 1. provenance

| 卡 | thread | 模型 | verdict | confidence | 日期 | 内容 |
| --- | --- | --- | --- | --- | --- | --- |
| 第 7 轮主裁定卡（本轮 16 行 / 18 项） | `01a0fe61-b7a3-7853-a380-0793fee14e14` | `gpt-6-astra`（consult） | `adopt` | **0.96** | 2026-10-03 | 采纳第 7 轮全部建议：`P1-01`、`P1-02`、`P1-05`、`P1-06`、`P1-10`、`P1-15`、`P2-02`、`P2-07` 与缺陷 `D-2` / `D-7`、`D-5`、`D-6`、`D-9`、`D-11`、`D-12`、`D-13`、`D-14` 全部接受，编号 `S7A7-R01…R16`（R01=P1-01→S7A-3；R02=P1-02→S7A-3；R03=P1-05→阻塞 S7B-1；R04=P1-06→阻塞 S7C-2；R05=P1-10→S7A-9；R06=P1-15→S7A-3；R07=P2-02→S7A-4；R08=P2-07→S7A-3；R09=D-2/D-7；R10=D-5；R11=D-6；R12=D-9→S7A-8 owner-only；R13=D-11；R14=D-12；R15=D-13；R16=D-14）；`D-9` 为 **owner-only**，`D-12` 为**待 owner 确认** |
| 第 7 轮口径确认卡（三处计数 / 编号口径） | `01a0fe61-3476-7882-9674-5f6b05035237` | `gpt-6-astra`（consult） | `adopt` | **0.99** | 2026-10-03 | 确认 `S7A6-R01…R12` ↔ 15 行映射成立；确认第 6 轮 = **15 行 / 17 项**、第 7 轮 = **16 行 / 18 项**、逐轮合计 **76 行 / 84 项**与 §0.1 一致；确认 ADR 0044 的"第 7 轮 16 行 / 17 项"与"76 行 / 83 项"是**误推**；确认 `P2-03` **不属第 6 轮**（仍归第 2 轮） |

**两卡分工说明。** 主卡给出 16 行裁定的全部语义与编号；口径确认卡只回答"三处计数 / 编号口径"，**不重新
裁定已冻结语义**。因此本文 §2 的裁定要点以主卡为准，§3 的行 / 项口径以口径确认卡为准。

**日期口径说明。** 两张卡的文件名时间戳为 2026-10-02T20:52:37.188Z 与 2026-10-02T21:08:34.383Z（UTC），
裁定与落地按**本地日期 2026-10-03** 登记（与第 2、3、4、5、6 轮先例一致）；本文、Spec、ABI、工作表、
ADR 0044 与各计数登记均使用 2026-10-03。

## 2. 裁定要点（16 行）

| # | 编号 | 对应项 | 裁定要点 | 落点 | 首次消费 |
| --- | --- | --- | --- | --- | --- |
| 1 | `S7A7-R01` | P1-01 判定域 typed contract | 7A **只接受静态 typed 判定域记录**；**几何属 Gameplay closure**；坐标系与量程**在判定域内声明**，不依赖宿主视口 / 渲染分辨率 / 表现层 transform；与 **Presentation transform 完全隔离**（改表现 transform 不改变 Fact / Score / Combo / Statistics / identity）；**未来动态 frame** 属 **7B+ 候选**、7A **稳定拒绝**（不新增 R 条目） | Spec §3.8.7 第一组、§5.2 判定域行、`## S7A-3` 节 ⑧；BATCH_GATES §"第 7 轮裁定后的收尾门禁" | **S7A-3**（准入门禁） |
| 2 | `S7A7-R02` | P1-02 CXT local relation 合并 | 合并**在 prepare 编译期完成**（运行期不合并）；**稳定 ID = 来源文档 identity + 声明序号**（跨文档唯一、与物理顺序无关）；**同名不合并而是拒绝**；**跨 invocation 引用必须显式**（隐式解析拒绝）；合并按稳定 ID 排序，**重排不改变合并结果** | Spec §3.8.7 第二组、`## S7A-3` 节 ⑧ | **S7A-3**（准入门禁） |
| 3 | `S7A7-R03` | P1-05 `input.trajectory.v1` | 属**连续输入**，**7A 整体拒绝**；schema、量化、断点后 contact 生命周期、区域边界与采样失败处置**保留为 S7B-1 准入项**；7A 只接受离散 `press` / `release` / `update` / `absence` / `step`；**不新增 R 条目**（复用 §7.2 既有连续输入拒绝条目） | Spec §3.7.7 表、§7.2；`## S7A-3` 节 ⑧ 之外的既有拒绝面 | **阻塞 S7B-1** |
| 4 | `S7A7-R04` | P1-06 Ruleset package 边界 | 随 `Q-17` 登记为 **S7C-2 准入项**（entry kind、package identity、版本迁移、缺包行为、prepare 时机）；7A 遇 package 输入**稳定拒绝**；entry kind **位置保留但不解析**；命中既有 **R-09** 映射 | Spec §3.16、§7.2 | **阻塞 S7C-2** |
| 5 | `S7A7-R05` | P1-10 预算分层与成本归属 | 沿用 [预算与证据计划](../../../proposals/gameplay-v2-acceptance/BUDGET_AND_EVIDENCE_PLAN.md) 的**五类 profile**（内容 / 稳态 / 快照与 Seek / Replay 与解码 / Packed wire）；V2 新增 section **复用** Packed envelope 预算与 `referenceCount` 口径；新增成本按"**谁产生谁计数**"归属；**7A 只登记计数与上界、不冻结限额**；**最坏值计数入口作为 S7A-9 证据项**；**禁止在 S7A-9 前冻结任何限额** | Spec §8.5 | **S7A-9** |
| 6 | `S7A7-R06` | P1-15 两路 authoring 等价性 | 等价 = prepare 后 **canonical graph 与 derived capability closure 逐字段相等**；**忽略 `sourceMap` 与物理顺序**（输入数组 / 参数 / 引用顺序、Packed 压缩 / 字典顺序）；**逐字段 semantic diff 作为 S7A-3 的 golden 证据**；不改变 §7.2 的 19 条拒绝面 | Spec §3.8.7 第三组、`## S7A-3` 节 ⑧ | **S7A-3**（准入门禁 / golden） |
| 7 | `S7A7-R07` | P2-02 措辞 | 采纳给定替换文本："**不解析/编译 solver；runtime coordinator 只执行已准备的确定性 policy**"；名称仍为 `coordinator.policy.greedy_v1`，**不得**把 runtime 行为称为 solver | Spec §3.12 第 2 条、§3.13 第 1 条；ABI 域 4 与 §S7A-4 节 ④；plan §S7A-4 | **S7A-4** |
| 8 | `S7A7-R08` | P2-07 三项命名统一 | 统一为 **`source closure`**（源 Chart / 模块 / 参数 / 源映射 / 元数据，属 source·content）/ **`diagnostic map`**（只服务诊断、编辑器与迁移，**不进入任何 closure、不参与 identity**）/ **`content-artifact identity`**（载体 bytes 与物理编码参数，属 artifact）；三者**各自独立命名与归属、不得互相替代**；**`P1-14` 的 REF0 / manifest / diagnostic 归属仍以 Spec §3.8.6 为准**；ABI 侧对应 `SourceBuildIdentity` / `CompiledSemanticIdentity` / `ArtifactIdentity` 三行**只改语义文字、不新增类型行**（仍 9 域 / **126 = 96 + 30**） | Spec §5.1.1、§5.2 `sourceMap` 行、§6.5；ABI 域 7 | **S7A-3** |
| 9 | `S7A7-R09` | D-2 / D-7（合并行） | 以实现与 **ADR 0042** 为准（`open/play/pause/seek/reload/quit`；实现另有 `tick` 运行时步骤，**不在**六动词之内），订正**计划**的措辞；**不改 ADR 0042、不改实现** | plan §1.2 表、§S7A-8.3 验证表、§S7A-8 工作内容第 3 条；RULING_WORKSHEET §7 D-2 / D-7 行 | 文档订正（不阻塞实现） |
| 10 | `S7A7-R10` | D-5 | 规定"**后续审查文档必须使用 ADR / 格式编号引用**"（描述性指代不算可机械核对的引用）；本项**不回溯重写**已发布的 Alignment Review | RULING_WORKSHEET §7 D-5 行；本文 §2 | 文档订正（不阻塞实现） |
| 11 | `S7A7-R11` | D-6 | `tools/check_stage6_a2.py` 的**实际行号为 `:259`**（历史报告中写作 `:252`）；**在引用处加订正注记、不改写历史报告正文** | RULING_WORKSHEET §7 D-6 行；ABI `## S7A-7 之后的收尾` 第 3 条；本文 §6.3 | 文档订正（不阻塞实现） |
| 12 | `S7A7-R12` | D-9 | **owner-only**：版本门禁 shell 守卫须覆盖 WSL 启动器 `bash.exe`（**视图一致性探针 + 显式拒绝 `System32\bash.exe` + 独立负例**）；候选补丁**已在工作区、未提交**；剩余动作是**按 ADR 0042 具名复核后落到 `master`**，再于 S7A-8 消费；**在落 `master` 前不得宣称 S7A-8 版本门禁闭合** | 本文 §5；[D-9 候选补丁证据](2026-10-02-d9-shell-guard-candidate.md)；BATCH_GATES §"第 7 轮裁定后的收尾门禁" | **S7A-8** |
| 13 | `S7A7-R13` | D-11 | 稳定 C ABI 的阶段归属以 **Stage 14** 为准；`AGENTS.md` 第 21、260 行写 Stage 12 是**孤例**，由 **owner 择时订正**；**本轮不改 `AGENTS.md`**；同一句写入 `CURRENT_STATUS.md`、`ROADMAP.md`、ADR 0043 三处 | [CURRENT_STATUS.md](../../../CURRENT_STATUS.md) §Gameplay v2 收敛状态、[ROADMAP.md](../../../ROADMAP.md) 主实施路线前言、[ADR 0043](../../../adr/0043-gameplay-judgement-ruleset-convergence.md) 后续边界；ABI `## S7A-7 之后的收尾` | 文档订正（不阻塞实现） |
| 14 | `S7A7-R14` | D-12 | **待 owner 确认**：Gameplay I 被标 `superseded` 后，研究稿的 8 处指向行应**改指 ADR 0044 / V2 Spec / V2 ABI 并注明 Gameplay I 已 `superseded`**，**不得改写论证正文**，**须先取得 owner 对历史稿修改的确认**；**本轮只登记、不编辑**那 8 处：`docs/proposals/research/gameplay/README.md`:8-10、`GAMEPLAY_FOLD_CALCULUS_DRAFT.md`:3、`GAMEPLAY_RULESET_DISCUSSION.md`:14 / :665、`GAMEPLAY_STRESS_TEST_DRAFT.md`:16-18 | RULING_WORKSHEET §7 D-12 行、OPEN_QUESTIONS §4.5、BATCH_GATES 收尾门禁节、ABI 收尾节 | 文档订正（不阻塞实现） |
| 15 | `S7A7-R15` | D-13（含 `AGENTS.md`:110 附带子项） | **已处置**（owner 2026-10-03 裁定候选处置 (a) 并执行）：三个 spike 文件 `clang-format -i`；`cuexis_format_check` **156 条违规 → 0**（exit 1 → 0）；"仅格式改动"的决定性证据（去空白后与 HEAD 完全一致；`continuity_report.cpp` 仅多出恰好 7 处相邻字面量拆分标记，拼接后值不变）；`AGENTS.md`:110 的 glob 描述**已订正**为"递归覆盖 `app/` / `engine/` / `tests/` / `tools/` 与 `cmake/*.hpp.in`，target 仅在 `CUEXIS_BUILD_DEVELOPER_TOOLS` 打开时存在，且会扫到不被任何 CMake target 编译的文件" | RULING_WORKSHEET §7 D-13 行；本文 §6.1 | 只补最终证据指针（**无遗留实现子项**） |
| 16 | `S7A7-R16` | D-14 | **已处置**：保留方案 **(a)**——以 ABI `CapabilityRecord` 行的 `待冻结` 为准，`CM-X01` 登记到**第 6 轮**（与 `Q-15` / `Q-19` 同批）、首次消费 **S7A-7**；`CONTRACT_MATRIX` 的处置词在第 6 轮转回 `accept`（第 6 轮计数变化条已写 `open` → `accept`） | RULING_WORKSHEET §7 D-14 行、CONTRACT_MATRIX §12 `CM-X01` 行与 §14 第 6 轮计数变化条；本文 §6.2 | 只补最终证据指针（**无遗留实现子项**） |

### 2.1 `S7A7-Rxx` ↔ 行映射

**本轮的编号由主卡直接给出**（与第 6 轮不同）：主卡明确列出 `R01=P1-01→S7A-3；R02=P1-02→S7A-3；
R03=P1-05→S7B-1 阻塞；R04=P1-06→S7C-2 阻塞；R05=P1-10→S7A-9；R06=P1-15→S7A-3；R07=P2-02→S7A-4；
R08=P2-07→S7A-3；R09=D-2/D-7→文档订正；R10=D-5→文档订正；R11=D-6→文档订正；R12=D-9→S7A-8 owner-only；
R13=D-11→文档订正；R14=D-12→文档订正且历史稿改动须 owner 先确认；R15=D-13→补最终证据指针；
R16=D-14→补最终证据指针`。因此本表**不需要推导**，也不存在"待 Codex 确认"的映射。上一轮（第 6 轮）的
`S7A6-R01…R12` ↔ 15 行映射由本轮的**口径确认卡**一并确认成立（见 §3）。

## 3. 行 / 项口径与计数（16 行 / 18 项；76 行 / 84 项）

**第 7 轮 = 16 行 / 18 项。** 上表为 **16 行**；按"**合并行与附带子项各记 2 项**"计为 **18 项**：
`D-2 / D-7`（`S7A7-R09`）是**合并行**（同一行同时裁定两个编号，计 1 行 / 2 项）；`D-13` 的
**`AGENTS.md`:110 glob 描述附带子项**（`S7A7-R15`）是**独立的处置对象**（格式修复与 glob 描述订正各自
独立，计为独立项）。因此 **16 行 / 18 项**。

**逐轮折算（订正后自洽）。**

| 轮 | 行 | 项 |
| --- | --- | --- |
| 第 1 轮（2026-10-02） | 14 | 15 |
| 第 2 轮（2026-10-03） | 6 | 7 |
| 第 3 轮（2026-10-03） | 6 | 7 |
| 第 4 轮（2026-10-03） | 6 | 6 |
| 第 5 轮（2026-10-03） | 13 | 14 |
| 第 6 轮（2026-10-03） | 15 | 17 |
| **第 7 轮（2026-10-03）** | **16** | **18** |
| **合计** | **76** | **84** |

合计 **76 行 / 84 项**，与 [RULING_WORKSHEET.md](../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md)
§0.1 绑定的合计**逐字一致**。

**口径订正说明（不改写历史报告正文）。** [第 6 轮门禁记录](2026-10-03-s7a-6-gate-rulings.md) §9.4 / §10.4
记载的"第 7 轮 16 行 / **17** 项"与"逐轮合计 **76 行 / 83 项**，与 §0.1 的 84 相差 1 项"由本轮**口径确认卡**
（confidence **0.99**）判定为**误推**：第 7 轮实为 **16 行 / 18 项**，逐轮合计 **76 行 / 84 项**自洽。
**该带日期的报告是历史证据，本文不重写它的正文**；订正以 [ADR 0044](../../../adr/0044-gameplay-v2-semantic-kernel.md)
的"口径订正说明"、[RULING_WORKSHEET.md](../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) §7 的
"行 / 项口径"段、[Spec](../../../formats/GAMEPLAY_V2_SPEC.md) `## S7A-7 之后的收尾` 与本文承载。

**五条残余 `open` 合同项的处置词补齐（不占 `S7A7-R` 编号）。** `CM-V06`、`CM-V07`、`CM-V13`、`CM-I06`、
`CM-X06` 五项在**第 1 轮（2026-10-02）已被裁定**（§0.1 第 1 轮末列 5 项即此五项、§9 第 1 轮行亦已登记），
但 [CONTRACT_MATRIX.md](../../../proposals/gameplay-v2-acceptance/CONTRACT_MATRIX.md) 中它们长期仍为 `open`，
与 §0.1 / §8 构成**自相矛盾**。第 7 轮按第 1 轮裁定**只补齐处置词与裁定正文**：

- `CM-V06`（Canonical Gameplay Graph 的物理归属，随 `Q-02`）→ **`accept`**；
- `CM-V07`（`candidateRevision` 不进 `compiledSemanticIdentity`，但进 artifact compatibility / interchange）→ **`accept`**；
- `CM-V13`（**语义唯一、物理入口可多**：Chart v5 是 gameplay typed graph 的唯一聚合语义模型）→ **`accept`**；
- `CM-I06`（唯一 identity projection 表）→ **`accept`**；
- `CM-X06`（capability 派生来源唯一；**声明少于派生即稳定失败**，多于派生同样不成立）→ **`accept`**。

**计数（第 7 轮落地后）。**

| 登记项 | 目标值 |
| --- | --- |
| `RULING_WORKSHEET §8` 的 `open` 行 | **0**（第 1–7 轮分布均为 0） |
| `CONTRACT_MATRIX` 六类处置 | `accept` **52** / `revise` 26 / `supersede` 17 / `retain` 17 / `open` **0** / `reject` 2 = **114** |
| `open` 集合 | **空**（第 7 轮补齐后无残余） |
| `§0.1` 合计 | **76 行 / 84 项 / 末列 21**（末列是**历史归属列**，不随处置词变化） |
| §7.2 稳定拒绝清单 | 仍 **19 条**（**不新增 R 条目**） |
| §9.2 诊断类别 | 仍**九类** |
| ABI 追踪目录 | 仍 **9 域 / 126 = 96 + 30**、§未决项条目数不变、类型行**不新增** |

## 4. 本轮落地清单（逐文档）

| 文档 | 本轮改动 |
| --- | --- |
| [RULING_WORKSHEET.md](../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) | 文档角色段补"7 轮全部裁定 / 76 行 / 84 项 / `open` 0"；§0 的"owner 裁定（待填）"改为已全部填入；§0.1 第 7 行填 `16` 与五项 `CM-V*`、合计行末列补第 7 轮说明、末列口径段重写（历史 21、逐轮 `open` 全 0）；§6 的映射由"推导，待确认"改为"已确认"并补 `P2-03` 仍属第 2 轮；§7 十六行填 `owner 裁定` 列；§7 后新增第 7 轮登记块（provenance、编号与首次消费、行 / 项口径、五条 `CM-V*` 补齐、不新增计数、表述纪律）；§8 `open` → 0；§9 第 7 行登记 |
| [CONTRACT_MATRIX.md](../../../proposals/gameplay-v2-acceptance/CONTRACT_MATRIX.md) | §0 统计（`accept` 52 / `open` 0 / **`open` 集合为空**）+ 第 7 轮计数变化段；`CM-V06`、`CM-V07`、`CM-V13`、`CM-I06`、`CM-X06` 五行 `open` → `accept` 并写裁定正文与落点；§5 `CM-P05` 依赖行补 `CM-I06` 闭合说明；§14 统计与批次表（S7A-1 行改为 `—`）、追加第 7 轮计数变化条、结尾句改为 `open` **0** |
| [GAMEPLAY_V2_SPEC.md](../../../formats/GAMEPLAY_V2_SPEC.md) | §1.1 第 2 项改为"第 1–7 轮已全部裁定（无待裁定轮次）"；**新增 §3.8.7**（S7A-3 准入门禁三组规范文本）；§3.12 第 2 条与 §3.13 第 1 条改 `P2-02` 措辞；**新增 §5.1.1**（三项命名统一定义）；§5.2 `sourceMap` 行与 §6.5 闭包表按新命名对齐；§6.6 改为已裁定（`CM-X06`）；**新增 §8.5**（预算分层与成本归属）；`## S7A-3` 节补第 ⑧ 行与收尾段；`## S7A-4` / `## S7A-5` / `## S7A-6` 收尾段改"已于第 7 轮补齐"；`## S7A-7` 节范围外措辞；**新增 `## S7A-7 之后的收尾` 一节**；§11 前言与第 7 行 + 后续批次行；§12 索引措辞 |
| [GAMEPLAY_V2_ABI.md](../../../api/GAMEPLAY_V2_ABI.md) | §边界声明沿用（D-11 的 Stage 14 口径）；域 4 solver / coordinator 段改 `P2-02` 措辞；域 7 三行 identity 语义文字对齐 `P2-07`（`IdentityProjection` / `InterchangeCompatibility` 的**登记首词仍为 `待冻结`**）；`## S7A-3` 节补第 ⑦ 行；§S7A-4 节 ④ 行改措辞；§S7A-7 节范围外措辞与 126 条追踪段计数（52 / 0）；**新增 `## S7A-7 之后的收尾` 一节**；§未决项 #1 / #2 / #17 语义文字与前言；§相关索引 |
| [stage-07/plan.md](../../../stage_plans/active/stage-07/plan.md) | §1.1 未授权句改为"第 1–7 轮已裁定、无待裁定轮次"；§S7A-3 末补"第 7 轮准入门禁"段（四项，不新增工作项）；§S7A-4 第 ④ 项改 `P2-02` 措辞；§1.2 表与 §S7A-8.3 验证表、§S7A-8 工作内容第 3 条改六动词为 `open/play/pause/seek/reload/quit`（`D-2` / `D-7`） |
| [CURRENT_STATUS.md](../../../CURRENT_STATUS.md) | Gameplay v2 收敛状态段：第 1–7 轮已裁定 / 无待裁定轮次 + 裁定不等于实现完成 + **稳定 C ABI 唯一归属 Stage 14**（`D-11`） |
| [ROADMAP.md](../../../ROADMAP.md) | 主实施路线前言补**稳定 C ABI 唯一归属 Stage 14** 与 `AGENTS.md` 孤例说明（`D-11`） |
| [ADR 0043](../../../adr/0043-gameplay-judgement-ruleset-convergence.md) | 后续边界补**阶段归属澄清（`D-11`）**段 |
| [ADR 0044](../../../adr/0044-gameplay-v2-semantic-kernel.md) | "仍待裁定的部分（第 7 轮）" → "**第 7 轮已裁定：无待裁定轮次**"（16 行 / **18** 项、逐轮 76 / 84、口径订正说明、五条 `CM-V*` 收尾、`D-9` / `D-12` 状态）；接受门禁表第 7 轮行改为已裁定并补语义清单；第 4 轮行改 `P2-02` 措辞；`compiledSemanticIdentity` 未冻结项指向第 6 轮冻结；证据边界段补第 7 轮两卡 |
| [reading-order.md](../../../guides/reading-order.md) | Gameplay V2 段改为"第 1–7 轮已全部裁定"并补裁定不等于实现完成 |
| [OPEN_QUESTIONS.md](../../../proposals/gameplay-v2-acceptance/OPEN_QUESTIONS.md) | **新增 §4.5 第 7 轮裁定登记**（provenance、16 行表、行 / 项口径、五条 `CM-V*` 补齐、计数变化）；§4.4 映射改"已确认"并标注 `P2-03` 归属；§4.3 计数段落改为"当时"；§4 的 `Q-02` / `P2-02` 行补裁定指针；§6 第八批块改为已裁定 |
| [acceptance README.md](../../../proposals/gameplay-v2-acceptance/README.md) | §1 表 `CONTRACT_MATRIX` / `RULING_WORKSHEET` 行；A4 / A5 状态；§4 追加**第 7 轮段**（16 行 / 18 项、首次消费、`open` 0 / `accept` 52、口径订正、`D-9` / `D-12`）与 D-14 尾注订正 |
| [BATCH_GATES.md](../../../proposals/gameplay-v2-acceptance/BATCH_GATES.md) | S7A-3 节追加"第 7 轮追加的 S7A-3 准入门禁"；**新增"第 7 轮裁定后的收尾门禁"一节**（provenance、六个批次的正例 / 负例 / golden / 停止条件、跨批次文档门禁、`D-9` / `D-12` 状态、计数不变声明） |
| 本文 | 第 7 轮裁定与收尾的带日期落地记录 |

## 5. `D-9`：owner-only（**不得提前关闭**）

**内容。** 版本门禁（`.github` 与 `tools/` 侧的 shell 守卫）在默认 PATH 下**不覆盖 WSL 启动器 `bash.exe`**；
Codex 裁决（`adopt`，confidence 0.98）为：**视图一致性探针 + 显式拒绝 `System32\bash.exe` + 独立负例**。

**当前状态（如实登记）。**

- 候选补丁**已在工作区**、**未提交**（证据见 [D-9 候选补丁与验证证据](2026-10-02-d9-shell-guard-candidate.md)）；
- 剩余动作：**owner 按 ADR 0042 具名复核**该补丁，然后**落到 `master`**；
- 落地后由 **S7A-8** 消费。

**禁止声明。** 在该动作完成前，**不得**宣称 S7A-8 的版本门禁已闭合，**不得**把 `D-9` 计为已关闭，
**实现方不得自行落库**。本项是**唯一**在本轮保持"已裁定但未落地"的实现侧动作。

## 6. 缺陷处置的证据指针

### 6.1 `D-13`（格式门禁）——已处置，无遗留实现子项

保留候选处置 **(a)**：

- 三个 spike 文件（`tools/research/gameplay_fold_spike/` 的 `continuity.cpp`、`continuity_report.cpp`、
  `differential_report.cpp`）已执行 `clang-format -i`（clang-format 22.1.3），改动在**工作区、未提交**；
- `cuexis_format_check`：**exit 1（156 条违规）→ exit 0（0 条）**；
- **"仅格式改动"的决定性证据**：去除全部空白后比较，`continuity.cpp` 与 `differential_report.cpp` 与 HEAD
  **完全一致**；`continuity_report.cpp` 仅比 HEAD 多出恰好 **7** 处 `""` 相邻字面量拆分标记
  （`BreakStringLiterals` 行为，C++ 相邻字面量拼接后字符串值不变），移除后与 HEAD 完全一致——**零语义变更**；
- **附带子项**：`AGENTS.md`:110 的 glob 描述**已订正**为递归覆盖 `app/` / `engine/` / `tests/` / `tools/` 与
  `cmake/*.hpp.in`，并注明该 target 仅在 `CUEXIS_BUILD_DEVELOPER_TOOLS` 打开时存在、且会扫到不被任何 CMake
  target 编译的文件。

**无遗留实现子项。** 本项**不修改** `CUEXIS_FORMAT_FILES`、CMake 与工作流。

### 6.2 `D-14`（`CM-X01` 台账冲突）——已处置，无遗留实现子项

保留方案 **(a)**：

- 以 [ABI](../../../api/GAMEPLAY_V2_ABI.md) `CapabilityRecord` 行的 `待冻结` 为准；
- `CM-X01` 登记到**第 6 轮**（与 `Q-15` / `Q-19` 同批），首次消费 **S7A-7**；
- [CONTRACT_MATRIX.md](../../../proposals/gameplay-v2-acceptance/CONTRACT_MATRIX.md) 的处置词在第 6 轮已转回
  `accept`（§12 `CM-X01` 行与 §14 第 6 轮计数变化条即为落点）。

证据指针：第 6 轮登记块与 `S7A6-R02` 行、`CONTRACT_MATRIX` §12 / §14、本文 §2 第 16 行。**无遗留实现子项。**

### 6.3 `D-6`（行号订正）

`tools/check_stage6_a2.py` 的**实际行号为 `:259`**（历史报告写作 `:252`）。**本轮不改写任何带日期的历史
报告正文**，只在引用处（RULING_WORKSHEET §7 D-6 行、[ABI](../../../api/GAMEPLAY_V2_ABI.md)
`## S7A-7 之后的收尾` 第 3 条与本文）登记订正后的行号作为引用依据。

### 6.4 `D-2` / `D-7`（六动词命名）

以实现与 ADR 0042 为准：**`open/play/pause/seek/reload/quit`**；实现另有 `tick` **运行时步骤**，**不在**六动词
之内。**只订正计划措辞**（plan §1.2 表、§S7A-8.3 验证表、§S7A-8 工作内容第 3 条）；**不改 ADR 0042、不改实现**。

## 7. 缺陷 `D-12`：**待 owner 确认**（本轮只登记、不编辑）

Gameplay I 被标 `superseded` 后，研究稿中仍有把 I 当作现行权威的**指向行**（共 8 处）：
`docs/proposals/research/gameplay/README.md`:8-10、`GAMEPLAY_FOLD_CALCULUS_DRAFT.md`:3、
`GAMEPLAY_RULESET_DISCUSSION.md`:14 / :665、`GAMEPLAY_STRESS_TEST_DRAFT.md`:16-18。

**处置文本。** "**改指 ADR 0044 / V2 Spec / V2 ABI 并注明 Gameplay I 已 `superseded`，不得改写论证正文，
须先取得 owner 对历史稿修改的确认**"。

**本轮动作。** **只登记、不编辑**那 8 处（`FORMAT_BOUNDARY.md` 的同类指向已在 `D-4` 落地时同步）。
**"待 owner 确认"是本项的明确登记状态**；在该确认取得前不得落笔改指向行。

## 8. `D-11`：稳定 C ABI 的阶段归属

**以 Stage 14 为准。** `AGENTS.md` 第 21、260 行写 Stage 12 是**孤例**；`ROADMAP.md`、
`docs/stage_plans/active/stage-07/plan.md`、`docs/adr/0043-*.md`、Gameplay I ABI 与 Spec 均写 Stage 14。

**本轮动作。** 在**三处**（[CURRENT_STATUS.md](../../../CURRENT_STATUS.md)、[ROADMAP.md](../../../ROADMAP.md)、
[ADR 0043](../../../adr/0043-gameplay-judgement-ruleset-convergence.md)）各加一句"稳定 C ABI **唯一归属
Stage 14**"，并在 [ABI](../../../api/GAMEPLAY_V2_ABI.md) `## S7A-7 之后的收尾` 登记最小说明。
**本轮不改 `AGENTS.md`**，由 owner 择时订正。

## 9. 收尾门禁（各批次的实施门禁，不是"实现完成"声明）

| 批次 | 本批次必须验证 | 停止条件（任一命中即停） |
| --- | --- | --- |
| **S7A-3** | typed prepare（`P1-01`）；**Chart v5 inline ↔ CXT v2 emission 的逐字段 semantic diff**（`P1-15`）；**REF0 首次写入**（`P1-14`）；CXT local relation 按稳定 ID 合并（`P1-02`）；三项命名统一（`P2-07`） | 用默认值 / 临时 typedef / 伪成功绕过 §3.8.5 的 13 项不得消费清单**任一项**；把动态 frame 或未定的 wire 表示当作已冻结 |
| **S7A-4** | kernel 与 coordinator 边界（`P2-02`）；六步 / 八阶段唯一映射；7A 资源子集 | 消费 §3.13 不得消费清单任一项；把 `SolverProfile` 默认列表或 `max*` 数值当作已冻结 |
| **S7A-5 / S7A-6** | 生命周期、snapshot / replay、codec 与不得消费清单（Spec §3.14–§3.22、§3.18） | 消费 §3.22 不得消费清单任一项；把预算数值当作限额 |
| **S7A-7** | 集成（第 6 轮门禁）：`entryKind` 与 manifest 七项、`compiledSemanticIdentity` + 完整 capability closure、缺 Presentation 的 headless 路径、`GameplayOverride` precedence、`(factId, targetId)` 幂等 | 在 `CM-P04` / `CM-P06` / `CM-P08` / `CM-D04` / `CM-X01` 未闭合前开始集成 |
| **S7A-8** | 候选 / 版本门禁、同 SHA 交接；**`D-9` 的 owner-only 版本门禁 shell 守卫**（覆盖 `bash.exe`） | 在 owner 具名复核并把 `D-9` 补丁落到 `master` 前宣称版本门禁闭合 |
| **S7A-9** | 本地矩阵（Debug / Release / headless / MinGW）、hosted 同 SHA（Linux / MSVC / MinGW）、**五类预算实测**（`P1-10`）、交接与 owner 接受 | 在实测并单独接受前冻结任何限额；把研究观察值写成限额；用 `0` / "不限制"表示跳过 |

**跨批次文档门禁。** `python -B tools/check_docs.py`（exit 0）、`cuexis_format_check`（exit 0）、version gate、
architecture / allowlist 校验与 CTest 全部通过；`git diff --check` 与"新增 / 改动文件零行尾空白 / 零制表符"通过。

## 10. 门禁执行结果（本轮实跑）

| 门禁 | 命令 | 结果 |
| --- | --- | --- |
| 文档检查 | `python -B tools/check_docs.py` | 见 §10.1（exit 0） |
| 差异空白检查 | `git diff --check` | 见 §10.1（exit 0） |
| 行尾空白 / 制表符 | 对改动与新增 Markdown 逐文件扫描 | 见 §10.1（0 命中） |
| 计数复核 | 脚本按处置列直读 `CONTRACT_MATRIX` | `accept` 52 / `revise` 26 / `supersede` 17 / `retain` 17 / `open` 0 / `reject` 2 = **114**，与 §3 表一致 |
| 陈旧值 grep | 见 §10.2 | 见 §10.2 的逐条解释 |

### 10.1 门禁结果

- `python -B tools/check_docs.py`：**exit 0**。
- `git diff --check`：**exit 0**（无空白错误）。
- 改动与新增 Markdown 的行尾空白 / 制表符扫描：**0 命中**。
- 计数复核：`CONTRACT_MATRIX` 六类处置词直读为 `accept` 52 / `revise` 26 / `supersede` 17 / `retain` 17 /
  `open` 0 / `reject` 2，合计 114，与 §3 的计数表一致。

**未运行的门禁（如实登记）。** 本轮**不**运行 C++ 构建、CTest、`cuexis_format_check` 与 hosted 作业——本轮是
**纯文档改动**，不修改生成输入、也不作出需要新实现证据的声明。`cuexis_format_check` 的"156 → 0"是 `D-13`
的**历史证据**（§6.1），不是本轮实跑结果。

### 10.2 陈旧值 grep 与残余命中（逐条解释）

| 模式 | 目标 | 实测 | 逐条解释 |
| --- | --- | --- | --- |
| `第 7 轮待裁定` / `第 7 轮尚未裁定` / `第 7 轮尚未裁定的语义` | 0（live 文本） | **0** | Spec、ABI、plan、ADR 0044、CURRENT_STATUS、reading-order、工作表、OPEN_QUESTIONS、acceptance README 均已改为"第 1–7 轮已全部裁定（无待裁定轮次）" |
| `5 项 open` / `` `open` 5 `` / `open` **5**（当前状态表述） | 0（live 文本） | **0**（当前状态表述）；**残余命中均为当轮历史叙述** | 残余出现在第 6 轮 provenance 段（"第 6 轮落地后 `open` 5"）与历史报告 `2026-10-03-s7a-6-gate-rulings.md`。前者已加"**第 6 轮落地后的当时计数**"与"第 7 轮最终为 0"限定；后者是**带日期的历史证据**，按约定不改写 |
| `第 6 至第 7 轮` / `第 5-7 轮` | 0 | **0** | 上一轮与更早轮次已改 |
| `16 行 / 17 项`（第 7 轮口径） | 0（live 文本） | **0**（live 文本）；历史报告与 ADR 的"口径订正说明"中作为**被引述的旧值**出现 | ADR 0044 与本文 §3 引用它时都紧跟"是误推 / 实为 18 项"的订正，属**订正说明的必要引用**，非陈旧值；`2026-10-03-s7a-6-gate-rulings.md` 是历史证据 |
| `83 项`（逐轮合计） | 0（live 文本） | **0**（live 文本）；历史报告与订正说明中作为**被引述的旧值**出现 | 同上；Spec `## S7A-7 之后的收尾`、ADR 0044、工作表 §7 与本文均给出"实为 84 项自洽"的结论 |

**结论。** live 文本中上述五种陈旧表述的**当前状态表述**均为 0 命中；所有残余命中都落在
（a）**带日期的历史报告**（`2026-10-03-s7a-6-gate-rulings.md` 等，按既定约定不改写历史正文），或
（b）**口径订正说明中作为被引述旧值的必要引用**（都紧跟订正结论，不构成自相矛盾）。二者均已在正文中
显式标注，不遗留歧义。

## 11. 解释性判断与需 owner 复核的项

### 11.1 本轮冻结的边界（解释性判断）

本轮只涉及**文档合同**（判定域 typed contract、CXT 合并规则、两路等价性、三项命名、`P2-02` 措辞、预算分层
与成本归属）与**门禁**（S7A-3 准入门禁、六个批次的正例 / 负例 / golden / 停止条件）以及**缺陷处置登记**
（`D-2` / `D-5` / `D-6` / `D-7` / `D-9` / `D-11` / `D-12` / `D-13` / `D-14`）。**不涉及**任何实现完成度、任何
数值承诺、任何字节编码。因此本轮**不构成**"Judgement 实现完成"或"Stage 7A 完成"。

### 11.2 需 owner 复核：`D-9`（owner-only）

见 §5。`D-9` 是**唯一**在本轮保持"已裁定但未落地"的实现侧动作：候选补丁未提交，须 owner 按 ADR 0042 具名
复核后落 `master`。**在该动作完成前不得宣称 S7A-8 版本门禁闭合。**

### 11.3 需 owner 复核：`D-12`（待 owner 确认）

见 §7。研究稿 8 处指向行**只登记、不编辑**；落笔前须先取得 owner 对历史稿修改的确认。

### 11.4 需 owner 复核：`AGENTS.md` 的 Stage 12 孤例（`D-11`）

见 §8。本轮**不改 `AGENTS.md`**，由 owner 择时把稳定 C ABI 的阶段号统一为 Stage 14。

### 11.5 已消除的上一轮待复核项

[第 6 轮门禁记录](2026-10-03-s7a-6-gate-rulings.md) §10.2 / §10.3 / §10.4 / §10.5 列出的四项待复核项
（`S7A6-Rxx` 映射、15 行 vs 17 项、global 1 项口径差、ADR 第 7 轮缺陷行措辞）**已由本轮全部消除**：
映射与口径由**口径确认卡**（0.99）确认；1 项差由"第 7 轮 16 行 / 18 项"解释为误推；ADR 第 7 轮缺陷行措辞已
在本轮一并处置。

## 12. 相关索引

- [Stage 7A 实施计划](../../../stage_plans/active/stage-07/plan.md)：批次、依赖与关闭标准（§1.1 / §S7A-3 / §S7A-4 / §S7A-8 / §S7A-9）
- [未决语义分轮裁决清单](../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md)：§7 第 7 轮登记块、§8、§9 第 7 行
- [S7A-0.2 合同逐项台账](../../../proposals/gameplay-v2-acceptance/CONTRACT_MATRIX.md)：114 项、**`open` 0**（`accept` 52）
- [未决问题清单](../../../proposals/gameplay-v2-acceptance/OPEN_QUESTIONS.md)：§4.5 第 7 轮裁定登记
- [批次门禁](../../../proposals/gameplay-v2-acceptance/BATCH_GATES.md)：§"第 7 轮裁定后的收尾门禁（2026-10-03）"
- [Gameplay V2 Spec](../../../formats/GAMEPLAY_V2_SPEC.md)：§3.8.7、§5.1.1、§8.5、`## S7A-7 之后的收尾`
- [Gameplay V2 ABI](../../../api/GAMEPLAY_V2_ABI.md)：域 4 / 域 7 语义文字、`## S7A-7 之后的收尾`
- [ADR 0044](../../../adr/0044-gameplay-v2-semantic-kernel.md)：第 7 轮已裁定、口径订正说明
- [D-9 候选补丁与验证证据](2026-10-02-d9-shell-guard-candidate.md)：owner-only 的未提交补丁
- [第 6 轮门禁报告](2026-10-03-s7a-6-gate-rulings.md)：上一轮的同类证据（其"83 项"为历史误推，见本文 §3）
- [第 5 轮门禁报告](2026-10-03-s7a-5-6-gate-rulings.md)：本报告的体例先例

# S7A-4 准入门禁：第 4 轮（仲裁、资源与事实序）裁定落地记录

状态：dated implementation record（第 4 轮裁定的文档落地证据；不是阶段关闭、不是 owner acceptance、
不是实施证据）

更新日期：2026-10-03

上级文档：[Stage 7A 实施计划](../../../../stage_plans/active/stage-07/plan.md) §S7A-4 ·
[Gameplay V2 acceptance package](../../../../proposals/gameplay-v2-acceptance/README.md) ·
[未决语义分轮裁决清单](../../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) §4、§8、§9 ·
[批次门禁](../../../../proposals/gameplay-v2-acceptance/BATCH_GATES.md) §1

本文是**带日期的落地记录**（reports own dated evidence）：它登记 Codex 对第 4 轮（仲裁、资源与事实序；
进入 S7A-4 前）的六项裁定与 `SolverProfile` 的语义闭合、唯一六步 / 八阶段映射、7A 资源子集与身份规则、
solver / coordinator 边界、P1-12 的早 / 晚判定、逐条落进哪些文档的哪一节、计数变化、S7A-4 的不得消费清单，
以及本轮跑过的门禁与结果。它不复制 Spec / ABI 的合同正文，也不构成任何实施声明；未在本文列出的门禁
一律视为未运行。

## 1. provenance

| 卡 | thread | 模型 | verdict | confidence | 日期 | 内容 |
| --- | --- | --- | --- | --- | --- | --- |
| 主裁定卡（第 4 轮六项裁定） | `s7a4-arbitration-rulings` | `gpt-6-astra`（consult） | `adopt` | **0.97** | 2026-10-03 | 采纳第 4 轮全部六项建议并按限定修订：**以六步 Tick 为外层、八阶段 Coordination window 为第 4 步内部规范**；7A 只支持 `free` / `held` / `terminal` 的 `capacity = 1` exclusive 资源与 prepare 后确定性 coordinator；阶段语义进 engine identity；所有未冻结数值与编码排除在 S7A-4 之外 |
| 处置词与计数确认卡 | `s7a4-arbitration-rulings` | `gpt-6-astra`（consult） | `adopt` | **0.99** | 2026-10-03 | 确认 `CM-C05`、`CM-C09` 均 `open` → **`accept`** 且退出 `open` 清单；最终 **`open` 14 / `accept` 38 / `revise` 26 / `supersede` 17 / `retain` 17 / `reject` 2 = 114**；§8 第 4 轮分布降为 **0**；§0.1 末列**仍保留** `CM-C05`、`CM-C09`；`CM-C04` / `CM-C06` / `CM-C07` 处置词**不变**；`S7A4-R01…R06` 映射可登记 |

**日期口径说明。** 两张卡的文件名时间戳为 2026-10-02T20:02:20.457Z / 20:04:37.920Z（UTC），裁定与落地按
**本地日期 2026-10-03** 登记（与第 2、3 轮先例一致）；本文、Spec §3.9–§3.13 / §S7A-4、ABI §S7A-4 与各计数
登记均使用 2026-10-03。

**verdict 口径说明。** 两张卡均为 `adopt`，因此本轮不存在"主卡未决、补卡关闭"的分工：六项裁定
（Q-03、Q-11、Q-16、CM-C05、CM-C09、P1-12）与 `SolverProfile` 的语义闭合在同一条话题内一次给出，
计数与处置词由第二张卡独立确认。两卡共同构成本轮 **6 行裁定**。

**`S7A4-R01…R06` 与合同项 / 未决项的对应。** `S7A4-R01`→`Q-03`、`S7A4-R02`→`Q-11`、`S7A4-R03`→`Q-16`、
`S7A4-R04`→`CM-C05`、`S7A4-R05`→`CM-C09`、`S7A4-R06`→`P1-12`；**六项的首次消费批次均为 `S7A-4`**。

## 2. 裁定要点

| # | 编号 | 裁定要点 | 落点 |
| --- | --- | --- | --- |
| 1 | `S7A4-R01` / Q-03（**接受**，阻塞 S7A-4） | 拆分为 **prepare / compile solver**（展开、验证、**上界证明**、**唯一性证明**、生成 prepared profile）与 **runtime coordinator**（只执行 prepared profile 指定的确定性候选排序、资源检查与提交）；runtime coordinator **不解析也不编译 solver**；名称采用 **`coordinator.policy.greedy_v1`**，**不把 runtime 行为称为 solver**；`P2-02` 措辞随之改写 | Spec §3.12、§S7A-4 节；ABI 域 4 与 §S7A-4 节 |
| 2 | `S7A4-R02` / Q-11（**修改后接受**，阻塞 S7A-4） | 7A 资源子集**冻结为 `free` / `held` / `terminal`**：`free + claim -> held`；`held + 合法 update -> held`；终止后按资源策略进入 `free` 或**永久 `terminal`**；`gap`、`handoff_pending`、非零 grace、handoff、`capacity > 1`、owner 集合与并列 slot **一律稳定拒绝**；`CM-C04` 的"只启用前三态"据此改写，不再采用 preliminary 状态表的 gap/handoff 分支 | Spec §3.10、§S7A-4 节；ABI 域 4；CONTRACT_MATRIX §5 |
| 3 | `S7A4-R03` / Q-16（**接受**，阻塞 S7A-4） | **唯一**映射：**六步＝外层**、**八阶段＝第 4 步内部完整展开**；阶段顺序与阶段语义进 **`judgement identity` 的 engine 组件**、**不进 chart/content identity**；**S7A-4 只冻结阶段名称、顺序、映射与语义边界**，不冻结阶段编号、序列化编码、预算与窗口数值；`SK §4.2` 五步变体标为**非权威推导、已被取代** | Spec §3.9、§S7A-4 节；ABI 域 4、域 7 与 §S7A-4 节；SEMANTIC_KERNEL §4.2 |
| 4 | `S7A4-R04` / CM-C05（**接受**，阻塞 S7A-4） | 随 Q-16 的唯一映射裁定关闭；同 Tick 交易阶段顺序不再是未决点。处置词 `open` → **`accept`** | Spec §3.9；CONTRACT_MATRIX §5、§14；RULING_WORKSHEET §4、§8、§9 |
| 5 | `S7A4-R05` / CM-C09（**修改后接受**，阻塞 S7A-4） | 随 Q-11 关闭；8 问逐条落定——`resourceId` 只在 prepared canonical graph 的资源命名空间内解释；`capacity = 1` 用唯一 slot；slot / lease / 最小 contact handle / claim identity 进 canonical graph 与 prepared judgement inputs；lease / contact 由引擎按规范阶段分配、宿主不得提供；`contact end` 不复用旧 handle；`observe` 只产生 Observation、不占用也不改变资源 owner；非零 grace / `gap` / `handoff_pending` / `capacity > 1` 稳定拒绝。处置词 `open` → **`accept`** | Spec §3.10、§S7A-4 节；ABI 域 4；CONTRACT_MATRIX §5、§14 |
| 6 | `S7A4-R06` / P1-12（**接受**，阻塞 S7A-4） | Exact **由判定窗口半宽定义**而非要求零 tick；**窗口外早击不产生 Fact 但产生诊断**；**Miss / absence 由 deadline 产生 Fact**；Hold 的 head / body / tail **分别记录有符号 error、不合并** | Spec §3.11、§S7A-4 节；ABI 域 5 与 §单位与量程 |
| 7 | `SolverProfile` 语义闭合（CM-C06 / CM-C07，阻塞 S7A-4） | 在 S7A-4 冻结**字段语义**、**缺失 / 歧义 / 无法证明唯一时的 prepare 拒绝语义**、`objective` / `tieBreak` 为**有序语义列表**；**不冻结**默认列表、具体算法预算、`K` / fuel / `max*` 数值与序列化表示（留给 **S7A-9** 与后续预算批次）。**处置词不变**（`revise` / `accept`） | Spec §3.12；CONTRACT_MATRIX §5；ABI 域 4 |

### 2.1 登记口径（门禁归属）

- **阻塞 S7A-4 的门禁**：`Q-03`、`Q-11`、`Q-16`、`CM-C05`、`CM-C09`、`P1-12` 六项的语义，以及
  `CM-C06` / `CM-C07` 的**语义闭合**（`SolverProfile` 字段语义、prepare 拒绝语义、`objective` / `tieBreak`
  有序语义列表）。
- **只阻塞首次消费它们的后续批次**（**不是** S7A-4 门禁）：`SolverProfile` 的**默认列表**与具体算法预算、
  `K` / fuel / `max*` **数值**（记 **S7A-9**）；**唯一性证明的 proof 编码**；**wire / serialization**
  （阶段编号与编码、资源状态表逐字段表示、identity 字节）；**`terminal` 编码**；**判定窗口半宽的具体数值**。
- **属 S7B+ / S7C 候选**：后续 `gap` / handoff / `capacity > 1` 语义。
- **只冻结语义边界、不冻结表示**：S7A-4 冻结阶段**名称 / 顺序 / 映射 / 语义边界**、7A 资源子集与身份规则、
  早 / 晚判定边界、solver 边界与 `SolverProfile` 字段语义；**不冻结**阶段编号、序列化编码、预算与窗口
  数值。
- **拒绝面不新增 R 编号**：本轮五类拒绝按既有 R-01 / R-02 / R-04 / R-07 与 §9.2 / §9.3 的 prepare 原子
  失败条件处置，§7.2 的稳定拒绝清单**仍为 19 条**。

## 3. 唯一映射（六步外层 / 八阶段内层）

**本轮的"地基"是一条唯一映射，不是两套并列顺序。** 外层是计划 §S7A-4 的六步 Tick 顺序，内层是
Preliminary Design §5.3 的八阶段 Coordination window；**八阶段是外层第 4 步的内部完整展开**。

| 外层步骤（Tick 顺序） | 内层展开（Coordination window 阶段） |
| --- | --- |
| 1. 激活到期 requirement | —（无内层展开） |
| 2. 处理到期 Release / tail 或 hard-deadline timer | —（无内层展开） |
| 3. 规范化并应用输入 | 八阶段第 1 步：收集本窗口内的 Observation |
| 4. 每个事件重建候选并按 resource / claimKey / requirementId / fanout 仲裁 | 八阶段第 2–7 步：生成 Candidate；结算到期 `gap` 与 recovery；处理 `handoff_pending` 竞争；求解仍为 `free` 的资源与新的 binding / quota；以不可变 `CoordinationCommit` 一次性提交 |
| 5. 由 commit 生成 Fact，随后按 causal total order 规范排序 | 八阶段第 8 步：从 commit 生成 Fact（不允许中途向 Ruleset 或 Presentation 可见） |
| 6. Tick 末提交 Hook / signal，下一 tick 才可见 | —（无内层展开） |

**7A 的收窄。** 内层第 3、4、5 步（`gap` / recovery / handoff 分支）在 7A **不被消费**：遇 `gap` /
`handoff_pending` / 非零 grace / handoff 声明时**稳定拒绝**，不得用空实现或默认值绕过。

**归属层级。** 八阶段的顺序与阶段语义进入 **`judgement identity` 的 engine 组件**，**不进入**
chart/content identity；`EngineIdentity` 因此承载该映射，而重排源数组、改 `sourceMap`、换表现资源或改
Packed 压缩顺序都不改变 judgement identity。

**被取代的推导。** [SEMANTIC_KERNEL.md](../../../../proposals/research/gameplay-v2/SEMANTIC_KERNEL.md) §4.2 的
"五步"阶段划分是研究期**非权威推导**，已被本轮取代；该研究稿只在 §4.2 加标注，**正文保留不改**。
golden / trace 与实现都必须按本节这一条解释，**不存在第三个映射**。

## 4. 7A 资源子集与身份规则

**状态子集（冻结）**：`free` + claim → `held`；`held` + 合法 update → `held`；终止后按资源策略进入
`free`（槽位可复用）或**永久 `terminal`**（槽位不可复用、后续 claim 稳定拒绝）。

**稳定拒绝（7A）**：`gap`、非零 grace（`grace = 0` 是唯一合法值）、`handoff_pending`、handoff
（含 handoff Hold）、`capacity > 1`、owner 集合、并列 slot。前两项映射 §7.2 的 **R-04**，后三项映射
**R-01**；**不新增 R 编号**。

**身份规则（冻结）**：① `resourceId` 只在 prepared canonical graph 的资源命名空间内解释，不由宿主 /
表现层 / Session 重新解释；② `capacity = 1` 使用**唯一 slot**；③ slot identity、lease identity、最小
contact handle identity、claim identity 进入 canonical graph / prepared judgement inputs（因此参与
judgement identity）；④ lease 与 contact 由引擎按规范阶段分配，宿主不得提供；⑤ `contact end` **不复用**
旧 handle，新接触必须取得新 handle，复用只能通过终止后槽位复用并由规范阶段提交；⑥ `observe` 只产生
Observation，**不占用、不改变资源 owner**（不进入状态表、不生成 lease、不需要 slot）；⑦ 同 Tick 阶段顺序
严格服从 §3 的唯一映射。

**处置词效果**：`CM-C04`（`revise`，只更新未决点文字）、`CM-C05`（`open` → `accept`）、
`CM-C09`（`open` → `accept`）、`CM-C03` / `CM-C12`（`revise`，只加裁定指针）。

## 5. solver / coordinator 边界与 `SolverProfile`

| 角色 | 职责 | 禁止 |
| --- | --- | --- |
| prepare / compile solver | 展开、验证、**上界证明**、**唯一性证明**，生成 prepared profile | 在运行期执行；把无法证明唯一的图放行 |
| runtime coordinator | 只执行 prepared profile 指定的**确定性候选排序、资源检查与提交** | **解析或编译 solver**；被称为 "solver" |

命名边界：runtime 侧采用 **`coordinator.policy.greedy_v1`**；`P2-02`（第 7 轮登记项）的措辞随之改写，
但**不因本轮提前关闭**。

**`SolverProfile` 的冻结强度**：字段**语义**、**prepare 拒绝语义**（缺失 / 歧义 / 无法证明唯一）与
`objective` / `tieBreak` 的**有序语义列表**在本批冻结；**默认列表**、`K` / fuel / `maxCandidates` /
`maxBranches` / `maxFuel` **数值**与**序列化表示**不在本批（S7A-9 / 后续预算批次）；
**唯一性证明的 proof 算法与证据形式**登记为首次消费它的后续批次阻塞项。

## 6. P1-12：早 / 晚判定与 error 记录

1. **Exact 由判定窗口半宽定义**，不要求 `error = 0`；窗口内（含量化误差）判为 exact。**半宽数值不在
   本批**，但"由半宽定义而非零 tick"已冻结。
2. **窗口外早击不产生 Fact，但产生诊断**：不得生成 Hit / Miss Fact，也不得静默丢弃或计为 Miss；
   诊断复用 §9.2 / §9.3 的既有稳定码，**不新增 R 编号**。
3. **Miss / absence 由 deadline 产生 Fact**，deadline 到达由外层第 2 步处理，不由输入事件触发。
4. **Hold 的 head / body / tail 分别记录有符号 error，不合并**（三个 phase 各自保留
   `observationTick - chartTick` 的符号，表达 early / exact / late）。

ABI 侧只承载**单位与符号**（`TimingError` 为有符号整数 tick 差），窗口语义的权威在 Spec §3.11。

## 7. 计数变化

| 项 | 第 3 轮落地后 | 第 4 轮落地后 |
| --- | --- | --- |
| `CONTRACT_MATRIX` 六类处置 | `open` 16 / `accept` 36 / `revise` 26 / `supersede` 17 / `retain` 17 / `reject` 2 = **114** | `open` **14** / `accept` **38** / `revise` 26 / `supersede` 17 / `retain` 17 / `reject` 2 = **114** |
| §14 的 `open` 清单与批次表 | 16 项；S7A-4 行 = `CM-C05`、`CM-C09` | **14 项**；S7A-4 行改为 `—`（门禁已满足） |
| `RULING_WORKSHEET` §8 `open` 行 | 16（第 4 轮 **2**） | **14（第 4 轮 0）** |
| §0.1 末列（"由哪一轮一并关闭"） | 21 | **21 不变**，但已闭合数由 5 变 **7**（仍保留 `CM-C05`、`CM-C09`） |
| §0.1 行 / 项总计 | 76 行 / 84 项 | **76 行 / 84 项 不变** |
| §7.2 稳定拒绝清单 | 19 条 | **19 条 不变** |
| ABI §类型清单 追踪目录 | 9 域 / 126 = 96 + 30 | **9 域 / 126 = 96 + 30 不变（不新增 ABI 条目）** |

**处置词变化**：`CM-C05`、`CM-C09` 由 `open` 改为 `accept`。**处置词不变**：`CM-C04`（`revise`）、
`CM-C06`（`revise`）、`CM-C07`（`accept`）——这三项只更新未决点文字。

## 8. S7A-4 不得消费清单

按 Spec §3.13 与 ABI §S7A-4 节一致登记：runtime solver 或 global solver；`max_cardinality`；
bounded backtracking；`capacity > 1`；Chord / `binding`；`gap` / handoff；`sameContact`；连续轨迹
（含 Slider continuity）；非零 grace；`boundedRelationInstance`；运行时脚本 / IO / 随机 / 墙钟；
默认数值限额；未冻结枚举或整数 typedef；任何序列化字节布局；把 `observe` 解释成占用资源的实现。

其中第 1、2、3 项属 `S7A4-R01` 的边界，第 4–9 项属 `S7A4-R02` / `S7A4-R05` 的资源子集，
第 10 项沿用第 3 轮（`CM-C10` / `P1-03`），第 11–14 项沿用第 1–3 轮的表示冻结边界与 S1-05 口径，
第 15 项属 `S7A4-R05` 的 `observe` 语义。

## 9. 改动的文档与落点

| 文档 | 改动落点 |
| --- | --- |
| [RULING_WORKSHEET.md](../../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) | 第 11 行 `16 项 open` → 14；§0.1 合计行末列口径段（第 4 轮闭合、14、分轮第 4 轮 **0**）；§4 六行 `owner 裁定` 单元格（含 `S7A4-R0x`、首次消费批次、落点、provenance）与 §4 后的第 4 轮登记块（唯一映射声明、归属层级、登记口径、处置词不变说明）；§8 `open` 行（14）与其解释段；§9 第 4 行裁定登记 |
| [CONTRACT_MATRIX.md](../../../../proposals/gameplay-v2-acceptance/CONTRACT_MATRIX.md) | §0 统计（38 / 26 / 17 / 17 / 14 / 2 = 114）与第 4 轮变化段；§4 `CM-C03`、`CM-C12` 裁定指针；§5 `CM-C04` / `CM-C05` / `CM-C06` / `CM-C07` / `CM-C09` 未决点文字；§14 `open` 14、S7A-4 行改 `—`、第 4 轮计数变化段 |
| [GAMEPLAY_V2_SPEC.md](../../../../formats/GAMEPLAY_V2_SPEC.md) | 新增 §3.9（唯一映射）、§3.10（资源子集与身份规则）、§3.11（早 / 晚判定）、§3.12（solver / coordinator 与 `SolverProfile`）、§3.13（不得消费清单）；§7.1 `greedy_v1` 命名指针；§7.2 第 4 轮映射段（不新增 R 编号）；新增 `## S7A-4 限定冻结范围与登记规则（2026-10-03）`；§11 第 4 行改"已裁定"并新增后续批次行；§11.1 第 6 条；§S7A-1 / §S7A-2 / §S7A-3 三节的轮次范围改为"第 5 至第 7 轮" |
| [GAMEPLAY_V2_ABI.md](../../../../api/GAMEPLAY_V2_ABI.md) | 域 4 的 `ResourceId` / `ResourceSlot` / `ResourceState` / `ResourceLease` / `ClaimKey` / `SolverProfile` / `CandidateId` 状态格与新增三段（资源身份、solver / coordinator 命名边界、唯一映射）；域 7 不透明性范围段；§单位与量程 `error` 条；§稳定与非稳定声明 的 `SolverProfile` 行；§未决项 前言、#7 / #8 / #9 改"已裁定"、新增 #20；新增 `## S7A-4 限定冻结范围与登记规则（2026-10-03）`（含"不新增 ABI 条目"收尾声明）；§相关索引 |
| [OPEN_QUESTIONS.md](../../../../proposals/gameplay-v2-acceptance/OPEN_QUESTIONS.md) | 更新日期行；§1 `Q-03` / `Q-11` 与 §2 `Q-16` 的裁定指针；§4 `P2-02` 措辞确认；新增 §4.1 第 4 轮裁定登记（provenance + 7 行 + 登记口径）；§6 第四批标注"已裁定" |
| [README.md](../../../../proposals/gameplay-v2-acceptance/README.md) | §1 `RULING_WORKSHEET` 行（14 项、§2 / §3 / §4 / §9）；§2 A4 行（第 1–4 轮已裁定、第 5–7 轮待裁定）；§4.3 第 4 轮段 |
| [BATCH_GATES.md](../../../../proposals/gameplay-v2-acceptance/BATCH_GATES.md) | §1 S7A-4 行的正例 / 负例 / 诊断 / golden / 停止条件；新增"第 4 轮裁定后的 S7A-4 门禁（2026-10-03）"段 |
| [SUPPORT_AND_REJECTION_MATRIX.md](../../../../proposals/gameplay-v2-acceptance/SUPPORT_AND_REJECTION_MATRIX.md) | §2 资源行备注（冻结子集指针）；§3 `R-04` 行补入**非零 grace**；§3 后的第 4 轮最小同步说明（**条数仍 19**） |
| [SEMANTIC_KERNEL.md](../../../../proposals/research/gameplay-v2/SEMANTIC_KERNEL.md) | §4.2 的"非权威推导、已被取代"标注块（正文不改） |
| [ADR 0044](../../../../adr/0044-gameplay-v2-semantic-kernel.md) | "仍待裁定的部分"段（第 5 至第 7 轮 **44 行 / 49 项**与算式）、第 4 轮表行、威胁 5（`SolverProfile` 数值预算）、威胁 6（`CM-C04` 分支与后续语义）、未决与后续第 2 项、证据边界第 7 项 |
| [plan.md](../../../../stage_plans/active/stage-07/plan.md) | §1.1 未授权项改为"第 5 至第 7 轮"；§S7A-4 的"第 4 轮裁定后的范围收窄（2026-10-03）"段（拒绝面由"不属于 7A"升级为稳定拒绝，新增非零 grace / owner 集合 / 并列 slot；**不新增工作项**） |
| [stage_reports/README.md](../../../README.md) | Stage 7A 清单追加本文条目 |
| 本文（新增） | 第 4 轮裁定的带日期落地证据 |

## 10. 跑过的门禁与结果

| 门禁 | 命令 / 方式 | 结果 |
| --- | --- | --- |
| 文档检查 | `python -B tools/check_docs.py` | 见 §10.1 |
| 空白检查（已跟踪差异） | `git diff --check` | exit 0（仅 LF/CRLF 提示，见 §10.2） |
| 空白 / 制表符扫描（含未跟踪文件） | 逐文件正则扫描 12 个改动文件 + 本文 | 0 处尾随空白、0 处制表符 |
| 计数脚本直读复核 | 见 §10.3 | 六类处置 114；§14 `open` 14；§8 `open` 14；§0.1 76 行 / 84 项 / 末列 21；§7.2 19 条；ABI 9 域 126 = 96 + 30 |
| 表格列数一致性 | 见 §10.3 | 3 处不一致**均为前置内容**（`\|` 转义与代码内裸 `|`），非本轮引入 |
| 陈旧值 grep | 见 §10.4 | 残余命中逐条解释 |

### 10.1 `check_docs.py` 实际输出

```text
Documentation checks passed: 333 Markdown files and 20 candidate JSON/CXT files validated.
```

exit code **0**（在本文创建前，同一命令因本文尚不存在而报 18 处 `broken relative link:
.../2026-10-03-s7a-4-gate-rulings.md`；这 18 条命中恰好证明各文档对本文的相对链接深度正确，本文创建后
全部消失）。

### 10.2 空白扫描

`git diff --check`：

```text
exit=0
```

仅输出 `warning: in the working copy of '...', LF will be replaced by CRLF the next time Git touches it`
（针对工作区中**已存在**的改动文件，属行尾风格提示，不是 `--check` 的错误）。

逐文件扫描（12 个已存在文件 + 本文）：

```text
scanned=13 violations=0
```

（扫描同时检查尾随空白与制表符；本文单独复扫同样为 0。）

### 10.3 计数脚本直读复核

```text
CONTRACT_MATRIX contract rows: 114
dispositions: {'accept': 38, 'revise': 26, 'supersede': 17, 'retain': 17, 'open': 14, 'reject': 2} total: 114
sec14 open bullet: ['14']
sec14 accept bullet: ['38']
RW sec8 open row: | `open` 合同项（CONTRACT_MATRIX §14） | 14 | 第 1 轮 5、第 2 轮 **0**、第 3 轮 **0**、第 4 轮 **0**、第 5 轮 4、第 6 轮 5 |
RW sec01 合计 row: | 合计 | — | **76 行 / 84 项** | **21**（...） |
RW sec9 round rows: ['1', '2', '3', '4', '5', '6', '7']
SPEC 7.2 R-rows: 19 ['R-01', ..., 'R-19']
```

ABI §类型清单 逐域统计（按 `### 域 N` 分段数表格行）：

```text
域 1 时间基: rows=10 frozen=8 pending=2
域 2 输入与观测: rows=12 frozen=10 pending=2
域 3 Requirement、Pattern 与 Measure: rows=19 frozen=16 pending=3
域 4 协调与资源: rows=22 frozen=17 pending=5
域 5 提交、Fact 与判定结果: rows=14 frozen=11 pending=3
域 6 Ruleset 事务与会话状态: rows=15 frozen=10 pending=5
域 7 identity 与兼容判定: rows=13 frozen=11 pending=2
域 8 capability、入口与诊断: rows=13 frozen=9 pending=4
域 9 Replay、Snapshot 与 Seek: rows=8 frozen=4 pending=4
TOTAL 126 frozen 96 pending 30
```

结论：**本轮不新增 ABI 条目**——9 域 / 126 = 96 + 30 与第 3 轮完全一致；域 4 的 22 条只改状态格文字，
`冻结目标` / `待冻结` 首词分布不变（17 / 5）。

### 10.4 陈旧值 grep 与残余命中（逐条解释）

| 陈旧值 | 命中 | 逐条解释 |
| --- | --- | --- |
| `16 项 open` | 1 | `2026-10-03-s7a-3-gate-rulings.md` §11 索引行——**带日期历史证据，按约束不改**；它描述第 3 轮时点的计数。（本文对同一计数的引用把反引号放在"16"之前，形式不同，不命中本模式，故本文计数为 0。） |
| `` `open` 16 `` / `` `accept` 36 `` | 各 8（其中本文 **2**） | ① `GAMEPLAY_V2_ABI.md` §S7A-3、② `GAMEPLAY_V2_SPEC.md` §S7A-3 的 provenance 段——描述**第 3 轮确认卡**的时点计数，正确；③ `CONTRACT_MATRIX.md` §0 第 3 轮变化段（18 → 16）与 ④ §14 第 4 轮变化段（16 → 14）、⑤ acceptance `README.md` §4.3 第 4 轮段（16 → 14）、⑥ `RULING_WORKSHEET.md` §8 第 3 轮确认卡段——均为**变化叙述中的"起点值"**，不是现值；⑦ `2026-10-03-s7a-3-gate-rulings.md` 命中属历史记录；⑧本文 §7 计数变化表与 §10.4 本表——**作为解释对象的引用**，属预期命中 |
| `` `open` 18 `` | 9（其中本文 **1**） | `CONTRACT_MATRIX.md` §0 与 §14 的**第 2 / 3 轮变化起点值**、`OPEN_QUESTIONS.md` §3.1 第 3 轮登记，以及 `2026-10-03-s7a-2/3-gate-rulings.md` 的历史记录；本文命中同为解释对象。均为变化叙述，非现值 |
| `第 4 轮 2` | 4（其中本文 **1**） | ① acceptance `README.md` 第 50 行的命中实为 **"第 4 轮 2026-10-03 仲裁…"** 的子串误报；②③ `2026-10-03-s7a-3-gate-rulings.md` 第 77 / 204 行——第 3 轮时点"第 4 轮 2"确实正确，历史证据不改；④本文 §10.4 本表为解释对象 |
| `第 4-7 轮` | 3（其中本文 **1**） | `docs/guides/reading-order.md` 第 61 行、`docs/CURRENT_STATUS.md` 第 241 行——**均不在本轮授权的 12 份文档内**，属**建议的后续一行同步**（改为"第 5-7 轮"并补"第 4 轮已裁定"）。本轮按"只改指定文档、避免与并行工作混写同一文件"的约束**未改**，登记为残余项；本文命中为对该残余的登记 |
| `第 4 至第 7 轮` | 5（其中本文 **1**） | 4 处在 `2026-10-03-s7a-3-gate-rulings.md`（第 144 / 152 / 223 / 227 行）——第 3 轮落地记录对自己当时所做事实同步的叙述，属历史证据；本文命中为解释对象 |
| `只启用前三态` | 7（其中本文 **2**） | ① `GAMEPLAY_V2_SPEC.md` §3.10、② `CONTRACT_MATRIX.md` §5 `CM-C04`、③ `OPEN_QUESTIONS.md` §4.1、④⑤ `RULING_WORKSHEET.md` §4 两处——全部是**对该措辞被取代这一事实的引用**（"据此改写"），不是仍然有效的表述；⑥⑦本文 §4 与 §10.4 为解释对象 |
| `solver 边界矛盾` | 0 | 该问法已由 `Q-03` 的裁定文本取代，仓库内无残留 |
| `第 4 轮 \| CM-C05、CM-C09` | 0 | §14 批次表该行已改为"门禁已满足" |

> **2026-10-03 追注（残余项已消解，仅定位值作废）**：上表 `第 4-7 轮` 行的两处引用，其实质已在同日修好——
> `docs/guides/reading-order.md` 第 61 行与 `docs/CURRENT_STATUS.md`（§阶段状态表格行、§当前格式与 SDK 合同
> → "Gameplay v2 收敛状态"）现均写作"**第 1–7 轮已裁定、无待裁定轮次**"。同一日 `docs/CURRENT_STATUS.md`
> 已由 336 行精简为 **136** 行（阶段表随后按 owner 反馈还原为逐阶段独立行），因此该行给出的行号**不再是有效定位值**；引用该文档时请按**章节**读。
> 通用口径：行号只作**定位**、不作**归属**。

**额外发现（非本轮引入，未改）**：`CONTRACT_MATRIX.md` §13 的 `CXC_FORMAT.md` 行与 `OPEN_QUESTIONS.md`
§4 `P2-05` 行在代码跨度内含**未转义的 `|`**，会使该表格多出一列；`RULING_WORKSHEET.md` §7 同一内容使用
`\|` 转义。该项与第 4 轮无关，登记为后续一行修正。

## 11. 相关索引

- [Gameplay V2 字段与运行语义规范](../../../../formats/GAMEPLAY_V2_SPEC.md)：§3.9–§3.13 与 §S7A-4 节的权威正文
- [Gameplay V2 ABI](../../../../api/GAMEPLAY_V2_ABI.md)：域 4 / 域 5 / 域 7 与 §S7A-4 节
- [未决语义分轮裁决清单](../../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md)：§4 裁定与 §9 登记
- [S7A-0.2 合同逐项台账](../../../../proposals/gameplay-v2-acceptance/CONTRACT_MATRIX.md)：114 项与 14 项 `open`
- [支持 / 拒绝矩阵](../../../../proposals/gameplay-v2-acceptance/SUPPORT_AND_REJECTION_MATRIX.md)：19 条稳定拒绝
- [批次门禁](../../../../proposals/gameplay-v2-acceptance/BATCH_GATES.md)：S7A-4 门禁
- [Stage 7A 实施计划](../../../../stage_plans/active/stage-07/plan.md)：§S7A-4
- [第 3 轮门禁报告](2026-10-03-s7a-3-gate-rulings.md) · [第 2 轮门禁报告](2026-10-03-s7a-2-gate-rulings.md) ·
  [S7A-0 接受记录](../readiness/2026-10-02-s7a-0-acceptance.md)
- [V2 语义内核](../../../../proposals/research/gameplay-v2/SEMANTIC_KERNEL.md)：§4.2 的非权威推导标注

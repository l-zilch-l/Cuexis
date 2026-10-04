# S7A-5 / S7A-6 准入门禁：第 5 轮（Ruleset、事实序、Score 与 Snapshot）裁定落地记录

状态：dated implementation record（第 5 轮裁定的文档落地证据；不是阶段关闭、不是 owner acceptance、
不是实施证据）

更新日期：2026-10-03

上级文档：[Stage 7A 实施计划](../../../stage_plans/active/stage-07/plan.md) §S7A-4 / §S7A-5 / §S7A-6 ·
[Gameplay V2 acceptance package](../../../proposals/gameplay-v2-acceptance/README.md) ·
[未决语义分轮裁决清单](../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) §5、§8、§9 ·
[批次门禁](../../../proposals/gameplay-v2-acceptance/BATCH_GATES.md) §1

本文是**带日期的落地记录**（reports own dated evidence）：它登记 Codex 对第 5 轮（Ruleset、事实序、Score
与 Snapshot；进入 S7A-5 / S7A-6 前）的十三项裁定、逐条落进哪些文档的哪一节、计数变化、两批的不得消费
清单与停止条件，以及本轮跑过的门禁与结果。它不复制 Spec / ABI 的合同正文，也不构成任何实施声明；
未在本文列出的门禁一律视为未运行。

**本轮是两批（S7A-5 与 S7A-6）的**准入门禁****：十三项裁定同时覆盖两个批次的消费面，因此本文标题与
落点都按"S7A-5 / S7A-6"合并登记，而裁定要点表逐条标注各自的首次消费批次。

## 1. provenance

| 卡 | thread | 模型 | verdict | confidence | 日期 | 内容 |
| --- | --- | --- | --- | --- | --- | --- |
| 主裁定卡（第 5 轮十三项裁定） | `01a0fe49-cfe5-7933-8d03-3d0a65d34c8b` | `gpt-6-astra`（consult） | `adopt` | **0.95** | 2026-10-03 | 采纳第 5 轮全部建议并按限定修订：**唯一规范事实总序** `(commitTick, originKindPriority, canonicalOrdinal)`（禁用 `ingressSequence` / 容器顺序 / 线程完成顺序）、Fact 身份生成语义与 `FactSemanticRevision` 归属、Ruleset Tick **三阶段**与 `faulted` 行为矩阵、`RegisterKind` 三类、7A 只用内置 Ruleset、两套 header 字段集与 `SnapshotPayload` 闭包、`SeekLatencyCommitment` 的类型与收紧关系、`Outcome` / `FactCategory` / grade / `TimingError` 与统计规则、Life 在 7A 关闭、`P1-14` 归属表作为 Spec 附录冻结；编号 `S7A5-R01…R11` |
| 处置词与计数确认卡 | 同上 | `gpt-6-astra`（consult） | `adopt` | **0.99** | 2026-10-03 | 确认 `CM-F06`、`CM-S08`、`CM-S10`、`CM-K07` 四项 `open` → **`accept`** 且退出 `open` 清单；最终 **`open` 10 / `accept` 42 / `revise` 26 / `supersede` 17 / `retain` 17 / `reject` 2 = 114**；§8 第 5 轮分布为 **0**；§0.1 合计行末列**仍为 21**（已闭合数由 7 增至 **11**）；`CM-S02` / `CM-K01` 处置词**不变**（均 `revise`）；`CM-S11` / `CM-K08` **不是** CONTRACT_MATRIX 的行、**不改变任何计数**；`S7A5-R01…R11` 映射可登记 |
| ABI 状态格首词与追踪计数追问卡 | 同上 | `gpt-6-astra`（consult） | `adopt` | **0.98** | 2026-10-03 | 确认第 5 轮涉及的 **9 行状态格首词全部保持 `待冻结`**（`RegisterKind`、`ScoreState`、`ComboState`、`LifeState`、`StatisticsSnapshot`、`EventCodecId`、`ReplayDecodeBudget`、`SnapshotHeader`、`SeekLatencyCommitment`）；**不新增 §未决项条目**（无 `#21`）、**不新增 §类型清单条目**；ABI 追踪目录 **9 域 / 126 = 96 + 30 不变**；只允许在既有状态格内追加"附加限定"文字 |

**日期口径说明。** 三张卡的文件名时间戳为 2026-10-02T20:27:24.487Z / 20:29:30.701Z / 20:34:38.916Z（UTC），
裁定与落地按**本地日期 2026-10-03** 登记（与第 2、3、4 轮先例一致）；本文、Spec §3.8.6 / §3.9 / §3.14–§3.22
/ §S7A-5 / §S7A-6、ABI §S7A-5 / §S7A-6 与各计数登记均使用 2026-10-03。

**verdict 口径说明。** 三张卡均为 `adopt`，因此本轮不存在"主卡未决、补卡关闭"的分工：十三项裁定
（`S7A5-R01…R11`）由主卡一次给出，处置词与计数由第二张卡独立确认，ABI 首词与追踪计数由第三张卡独立
确认。三卡共同构成本轮 **13 行裁定 / 14 项**（`CM-S10` 与 `P1-07` 合并登记在同一行）。

**编号登记口径（易错点，已按卡 2 修正）。** `S7A5-R01…R09` 覆盖九项语义裁定；**`S7A5-R10` 只用于
`LifeState`（Life 关闭）**；`P1-14` **另登记为 `S7A5-R11`**。**R10 与 R11 不得合并**，也不得把 `P1-14`
记入 `R10`——`P1-14` 是"归属表"类裁定，与 Life 无关。

## 2. 裁定要点

| # | 编号 | 裁定要点 | 首次消费 | 落点 |
| --- | --- | --- | --- | --- |
| 1 | `S7A5-R01` / Q-12（**接受**，阻塞 S7A-4 至 S7A-6） | Fact 的**唯一**规范总序为 **`(commitTick, originKindPriority, canonicalOrdinal)`**；`canonicalOrdinal` 由引擎从 `(originScope, originOrdinal, localOrdinal, phasePriority, factKindPriority)` 派生；**不得使用 `ingressSequence`、容器顺序或线程完成顺序**；Fact 的 `tick` **即** `commitTick`；`observationTick` 只定位 observation origin 并参与其 `originOrdinal` 派生，**不是**总序首键。**原含 `ingressSequence` 的旧 tuple 被取代** | **S7A-4**（字节合同属 S7A-6） | Spec §3.9 第 3 条、§3.17；ABI §S7A-5 节；plan §S7A-4 第 5 步 |
| 2 | `S7A5-R02` / CM-F06（**接受**，`open` → `accept`） | `originId` 不透明、由 `(originKind, originScope, originOrdinal)` 唯一确定（scope / ordinal 由引擎派生、宿主不得提供）；`commitId` 按总序**单调分配**；`factId` = `(commitId, localOrdinal)` 的**语义身份**；`phasePriority` 是 engine phase registry 的**稳定语义 rank**、**只**用于 `canonicalOrdinal`、**绝非评分**；timer ordinal 由 prepared timer identity 派生、seek / replay **不重新编号**；7A correction 仍由 **R-08** 拒绝；生成语义与 phase registry 进 **`FactSemanticRevision`**（属 `JudgementIdentity.engine`）。**物理字节布局、varint / endianness、`FactId` / `CommitId` 编码与 byte fixture 留 S7A-6** | S7A-5（编码 S7A-6） | Spec §3.17、§3.9；ABI 域 5 / 域 7 与 §未决项 #10；CONTRACT_MATRIX §6 |
| 3 | `S7A5-R03` / Q-08（**接受**，阻塞 S7A-5 与 S7A-6） | Ruleset Tick 固定为**三阶段**：①追加并封存**已排序** Fact Ledger → ②**原子校验并提交全部** StateDelta / Score / Combo / Statistics / RuleEffect → ③**只从已提交 Fact Ledger 与 FactBinding 投影**只读 PresentationEvent。第二阶段失败时**不追加 fault Fact**、**不发布** RuleEffectEvent 与 PresentationEvent、**保留旧 state**，session 可查询 `faulted`；`faulted` 的 `submit` / `advance` / `seek` / `replay` / 就地 `reload` 与 **`snapshot`** 一律**稳定失败**；只有**显式 reset** 或**创建替换新 session 的 reload / recovery** 可离开，且**不得恢复或伪造未提交 StateDelta** | S7A-5（`snapshot` 面 S7A-6） | Spec §3.14、§9.4、§10 第 6 条、§S7A-5 节；ABI 域 6 与 §S7A-5 节；plan §S7A-5 第 5 条与验证段 |
| 4 | `S7A5-R04` / CM-S02（**接受**，处置词**不变** `revise`） | `RegisterKind` 三类：`exclusive` 单 owner（第二 owner / 第二写入稳定失败）；`commutative_monoid` 只能由已声明贡献者以已声明且**可交换、可结合**的算子合成（未知 operator、重复 contribution identity、非交换 / 非结合合成稳定失败）；`ledger_derived` **禁止直接 StateDelta 写入**、只能从已提交 Fact Ledger **确定性重建**；**7A 只接受这三类**；Hook 的"下一 Tick 可见"**不变** | S7A-5 | Spec §3.15、§S7A-5 节；ABI 域 6 `RegisterKind`；CONTRACT_MATRIX §7 |
| 5 | `S7A5-R05` / Q-17 + CM-S08（**接受**，`CM-S08` `open` → `accept`） | 7A **只用内置、静态注册的 Ruleset Interface / 模块**；任何 `cuexis.ruleset` package 输入命中**既有 R-09**，诊断 `ruleset.package_unsupported`；**不得读** package hash / manifest / 迁移矩阵。package 的**形状 / identity / 完整性 / 版本迁移**归 **S7C-2** | S7A-5（S7A-6 Replay 亦消费） | Spec §3.16、§7.2 的 R-09；ABI 域 6 与 §未决项 #14；CONTRACT_MATRIX §7 |
| 6 | `S7A5-R06` / Q-13 + CM-K01 + CM-K07（**接受**，`CM-K07` `open` → `accept`；`CM-K01` 处置词**不变** `revise`） | **全量 Snapshot header 字段集** = `formatVersion`、四分量 `JudgementIdentity`、`factSemanticRevision`、`stateSchemaRevision`、normalized event count、Fact count、payload byte count、typed byte-budget descriptor；**Replay header 字段集** = `formatVersion`、四分量 identity、`NormalizationProfile` metadata、effective late-policy metadata、`eventCodecId`、normalized event count、encoded byte count、`ReplayDecodeBudget`。**拼写规范：类型 `EventCodecId`、字段 `eventCodecId`**。`factSemanticRevision` 是 Fact / 排序 / 折叠解释语义、**进 engine identity**；`stateSchemaRevision` **只**标识 `SnapshotPayload` 的无损状态结构、**不进** judgement identity | S7A-6 | Spec §3.18（另见 §3.17 第 3 条）、§S7A-6 节；ABI 域 9 与 §未决项 #12 / #16；CONTRACT_MATRIX §10 |
| 7 | `S7A5-R07` / Q-13（**接受**） | `SnapshotPayload` **闭包**（十四条：已提交 Fact Ledger 至 cursor 的前缀 + cursor、活动实例、Pattern 状态、Release / tail phase、exclusive resource 状态、finalization watermark、未提交窗口、`preparedGrace`、离散 sequence 状态、Hook snapshot、Fold / Score / Combo / Statistics 状态、pending signal queue、`SessionState`、fault 诊断 / 状态）与**不得保存清单**（Presentation / Effect cache、Animation 临时值、HostOverride token、未提交 StateDelta、连续采样相位）；**FactBinding 与 prepared immutable graph 在 header identity 验证后重新取得、不重复保存** | S7A-6 | Spec §3.18 第 4、5 条；ABI §S7A-6 节；plan §S7A-6 第 4 条 |
| 8 | `S7A5-R08` / CM-K08（**接受**，本次登记新增、不改变计数） | `SeekLatencyCommitment` = 带 measurement-profile 引用的 **typed engine commitment**；语义为从最近可用快照恢复并推进到目标的 **wall-clock seek latency**；会话**只能收紧、不得放宽**；测量入口为 [BUDGET_AND_EVIDENCE_PLAN.md](../../../proposals/gameplay-v2-acceptance/BUDGET_AND_EVIDENCE_PLAN.md) §5 的 restore time、Seek p95 / max 与无损性；**具体 `maxSeekLatency` 数值禁止**进入 Schema / header / 运行时约束 / 对外承诺，**提供具体值稳定拒绝**；seek **仍须无损** | S7A-6（数值 S7A-9） | Spec §3.19、§10 第 5 条、§S7A-6 节；ABI 域 9 与 §未决项 #16；plan §S7A-6 第 5 条 |
| 9 | `S7A5-R09` / CM-S10 + P1-07（**接受**，`open` → `accept`） | `Outcome` = **`{hit, miss}`**；Hold head / body / tail 是各自附属的 **phase-local outcome / error**、**不扩充** `Outcome`；`FactCategory` 与 phase **一一对应**，集合 `{tap, hold_head, hold_body, hold_tail}`；grade table **可选**、缺失即 **absent**、只报 Outcome、**不隐式升级**；`TimingError` = `observationTick - chartTick` 的**有符号整数 tick 差**；seek / replay **从 Fact Ledger 重建统计**；reset 清空新 session 状态且**不产生 Fact** | S7A-5 | Spec §3.20、§9.4；ABI 域 5 / 域 6；CONTRACT_MATRIX §7 |
| 10 | `S7A5-R10` / CM-S11（**接受**，本次登记新增、**只用于 `LifeState`**、不改变计数） | **Life 在 7A 关闭**；ABI 只保留 `LifeState` 的类型追踪与拒绝说明；任何 Life capability / Life policy / `LifeState` 初值或占位默认值一律以 **`capability.disabled` 稳定拒绝**，**不创建、不更新** `LifeState` | S7A-5 | Spec §3.21、§S7A-5 节；ABI 域 6 `LifeState` |
| 11 | `S7A5-R11` / P1-14（**接受**，`P1-14` **另登记**、不并入 R10） | `P1-14` 的规则与**具体归属矩阵**作为 **Spec 附录 §3.8.6** 冻结：判定必需引用 → **REF0 / judgement closure**；纯表现引用 → **manifest / presentation closure**；**source map 仅诊断**；归属由 prepare 判定、跨类即拒绝。**REF0 的物理字段 / 编号 / 编码由 S7A-3 的首次 Packed 写入消费**；**S7A-7** 负责 entry / manifest 集成；**不阻塞 S7A-5 / S7A-6** | **S7A-3**（集成 S7A-7） | Spec §3.8.6、§S7A-3 节引用；ABI §未决项 的 `P1-14` 指向段 |
| 12 | 处置词确认（第二张卡） | `CM-F06`、`CM-S08`、`CM-S10`、`CM-K07` 四项 `open` → **`accept`**；`CM-S02`、`CM-K01` **保持 `revise`**、只更新未决点文字；`CM-S11`、`CM-K08` **不是** CONTRACT_MATRIX 的行 | — | CONTRACT_MATRIX §0 / §6 / §7 / §10 / §14；RULING_WORKSHEET §5 / §8 / §9 |
| 13 | ABI 状态格与追踪计数确认（第三张卡） | 9 行状态格**首词全部保持 `待冻结`**，只追加"附加限定"文字；**不新增 §未决项条目**（无 `#21`）、**不新增 §类型清单条目**；**9 域 / 126 = 96 + 30 不变** | — | ABI §类型清单 域 5 / 域 6 / 域 9、§未决项、§S7A-5 / §S7A-6 两节 |

### 2.1 登记口径（门禁归属）

- **阻塞 S7A-5 的门禁**：`Q-08`、`CM-S02`、`Q-12`、`CM-F06`、`Q-17`、`CM-S08`、`CM-S10` / P1-07、
  `CM-S11`。
- **阻塞 S7A-6 的门禁**：`Q-08`、`Q-12` / `CM-F06`、`Q-13` / `CM-K01` / `CM-K07`、`CM-K08`、
  `CM-S10` / P1-07、`CM-S11`、`Q-17` / `CM-S08`。
- **不阻塞 S7A-5 / S7A-6**：`P1-14`（`S7A5-R11`）——它阻塞 **S7A-3** 的 REF0 首次写入与 **S7A-7** 的
  entry / manifest 集成。
- **阻塞 S7A-9 或其首次序列化消费者**：全部具体预算数值、`maxSeekLatency` 数值、Replay / Snapshot
  字节布局、Event / Fact codec 编码、`FactId` / `CommitId` 物理编码；package 形状 / 迁移归 **S7C-2**。
- **只冻结语义、不冻结表示**：本轮十三项全部只冻结**语义与行为边界**与**字段集**，不冻结宽度的物理
  表示、字节布局与数值。
- **拒绝面不新增 R 编号**：package 输入按既有 **R-09**、correction 按既有 **R-08**、Life 按
  `capability.disabled` 处置，§7.2 的稳定拒绝清单**仍为 19 条**。

## 3. 唯一规范事实总序（`S7A5-R01` / `S7A5-R02`）

**本轮改写的关键一条。** 旧计划文本按 `(observationTick、commitTick、ingressSequence、requirementId、
phase、factKind)` 排序，**含 `ingressSequence`**；本轮把它整体替换为引擎可复现的规范总序。

**Spec §3.9 第 3 条（落地后的最终文本，逐字引用）**：

> 3. **外层第 5 步的 causal total order 由第 5 轮（2026-10-03，`S7A5-R01`）定案**：Fact 的**唯一**规范总序
>    为 **`(commitTick, originKindPriority, canonicalOrdinal)`**。`canonicalOrdinal` 是引擎从
>    `(originScope, originOrdinal, localOrdinal, phasePriority, factKindPriority)` 派生的**规范序**；
>    **不得使用 `ingressSequence`、容器顺序或线程完成顺序**。Fact 的 `tick` **即** `commitTick`；
>    `observationTick` 只定位 observation origin 并参与其 `originOrdinal` 派生，**不是**总序首键；
>    `commitTick` 是提交事务的 Tick（§3.7.5）。**原含 `ingressSequence` 的旧 tuple 已被本条取代，不得再
>    作为实现、trace 或 golden 依据**；其**物理字节合同**（`FactId` / `CommitId` 编码、varint / endianness、
>    reference-evaluator byte fixture）属 **S7A-6** 的首次序列化消费（`CM-F06` / `Q-12`，见 §3.17 与
>    §11.1 第 6 条）。

**plan §S7A-4 第 5 步（落地后的最终文本，逐字引用）**：

> 5. 按**唯一规范事实总序** `(commitTick, originKindPriority, canonicalOrdinal)` 规范排序 Fact；其中
>    `canonicalOrdinal` 由引擎从 `(originScope, originOrdinal, localOrdinal, phasePriority,
>    factKindPriority)` 派生，**不得**使用 `ingressSequence`、容器顺序或线程完成顺序（第 5 轮
>    `S7A5-R01`；Spec §3.9 第 3 条、§3.17）。Fact 的 `tick` 即 `commitTick`；`observationTick` 只用于
>    定位 observation origin

**修改前（plan §S7A-4 第 5 步原文）**：

```text
5. 按 causal total order（observationTick、commitTick、ingressSequence、requirementId、phase、
   factKind）规范排序 Fact
```

**`originKindPriority` 与 `canonicalOrdinal` 的分工（冻结）**：`originKindPriority` 是**按 `originKind`
的规范常量 rank**（`observation` / `timer` / `coordination` / `correction`），作为总序第二键；`canonicalOrdinal`
是**同一 Tick 内的细粒度次序**，由引擎从五个派生分量合成，**宿主不可提供**。二者**都不得**被解释为
评分、置信度或优先级策略——尤其 `phasePriority` **绝非评分**（ABI 域 5 `OriginKindPriority` 与
`PhasePriority` 行的既有语义不变）。

**备注（`observationTick` 的位置）**：`observationTick` 从旧 tuple 的**首键**降为**派生输入**——它仍决定
observation 的 `originOrdinal`，但**不再**直接充当排序首键；总序首键是 `commitTick`。

## 4. Ruleset 与 Score：三阶段、`faulted`、`RegisterKind`、内置 Ruleset（`S7A5-R03` / `R04` / `R05` / `R09`）

### 4.1 三阶段与 `faulted`（`S7A5-R03`，同时阻塞 S7A-5 与 S7A-6）

| 阶段 | 内容 | 失败时 |
| --- | --- | --- |
| ① 追加并封存 | 把本 Tick 的 Fact 按**规范总序**追加进 Fact Ledger 并**封存**该前缀 | 本阶段不产生 `faulted`（尚未有状态变更） |
| ② 原子校验并提交 | StateDelta / Score / Combo / Statistics / RuleEffect 的**全部**范围、冲突、预算与 owner 校验，通过后**一次性提交** | **不追加 fault Fact**、**不发布** RuleEffectEvent 与 PresentationEvent、**保留旧 state**、不提交半个 Tick、session 可查询 `faulted` |
| ③ 只读投影 | **只从已提交 Fact Ledger 与 FactBinding** 投影 PresentationEvent | 投影失败不回滚阶段 ②（阶段 ③ 无状态写入权） |

**`faulted` 行为矩阵（冻结）**：`submit` / `advance` / `seek` / `replay` / **就地 `reload`** /
**`snapshot`** 一律**稳定失败**；只有**显式 `reset`** 或**创建替换新 session 的 `reload` / recovery**
可以离开，且**不得恢复或伪造未提交的 StateDelta**。

**plan §S7A-5 第 5 条（落地后，逐字引用要点）**：

> 5. 将一次 Tick 的 Ruleset 处理实现为**三阶段**（第 5 轮 `S7A5-R03`）：①追加并封存**已排序**的 Fact
>    Ledger；②**原子校验并提交全部** StateDelta、Score/Combo/Statistics 与 RuleEffect——只有全部通过范围、
>    冲突、预算和 owner 校验才提交；③**只从已提交 Fact Ledger 与 FactBinding 投影**只读 PresentationEvent。
>    第二阶段任一失败时：**不追加 fault Fact**、**不发布** RuleEffectEvent 与 PresentationEvent、**保留旧
>    state**、**不提交半个 Tick**，session 进入可查询的 `faulted` 状态。`faulted` 的 `submit` / `advance` /
>    `seek` / `replay` / 就地 `reload` 与 **`snapshot`** 一律**稳定失败**；只有**显式 `reset`** 或**创建
>    替换新 session 的 `reload` / recovery** 可以离开该状态，且**不得恢复或伪造未提交的 StateDelta**。

**修改前（plan §S7A-5 第 5 条原文）**：把同一 Tick 描述为"原子 transaction：Fact Ledger 先提交且不可
回滚……任一失败保留旧 state，不提交半个 Tick，session 进入可查询的 `faulted` 状态并停止接受新的
judgement mutation"——**未**写明"不追加 fault Fact / 不发布 RuleEffect / 不得恢复未提交 StateDelta"，
也**未**把 `snapshot` 列入 `faulted` 的失败操作集合。

**`faulted` 与 snapshot 的歧义消除（Spec §9.4 与 §10 第 6 条）**：`faulted` **不可新建 snapshot**
（`snapshot` 稳定失败）；同时 `SnapshotPayload` 闭包**仍包含** fault 诊断 / 状态（这是"恢复后仍可查询
fault 来源"的需要），**但那不构成"faulted 可产出新快照"**。两条同时成立、不矛盾。

### 4.2 `RegisterKind` 三类（`S7A5-R04`）

| kind | 合法来源 | 稳定拒绝 |
| --- | --- | --- |
| `exclusive` | **唯一 owner** 的写入 | 第二 owner、第二写入 |
| `commutative_monoid` | 只由**已声明贡献者**以**已声明**且**可交换、可结合**的算子合成 | 未知 operator、重复 contribution identity、非交换 / 非结合合成 |
| `ledger_derived` | 只从**已提交 Fact Ledger 确定性重建** | **任何直接 StateDelta 写入** |

**7A 只接受这三类**，其他 kind 稳定拒绝；Hook 的"下一 Tick 可见"**不变**。`CM-S02` 的处置词**保持
`revise`**（第二张卡确认），本轮只把三类命名与冲突策略写入正文。

### 4.3 内置 Ruleset 边界（`S7A5-R05`）

7A **只用内置、静态注册的 Ruleset Interface / 模块**：`RulesetInterfaceRef` / `RulesetManifest` **不来自
外部 package**。任何 `cuexis.ruleset` package 输入 → **既有 R-09**、诊断 `ruleset.package_unsupported`；
**不得读** package hash / manifest / 迁移矩阵（读取即越权）。package 的形状 / identity / 完整性 / 版本
迁移归 **S7C-2**。`CM-S08` 由 `open` → **`accept`**。

### 4.4 Outcome / category / grade / error 与统计（`S7A5-R09`）

1. `Outcome` = **`{hit, miss}`**。
2. Hold 的 **head / body / tail** 是各自附属的 **phase-local outcome / error**，**不得**扩充 `Outcome`。
3. `FactCategory` 与 phase **一一对应**：`{tap, hold_head, hold_body, hold_tail}`。
4. grade table **可选**；**缺失即 absent**，只报 Outcome，**永不隐式升级**。
5. `TimingError` = `observationTick - chartTick` 的**有符号整数 tick 差**。
6. seek / replay **从 Fact Ledger 重建统计**；reset 清空新 session 状态且**不产生 Fact**。

### 4.5 Life 关闭（`S7A5-R10`，**只用于 `LifeState`**）

Life 在 7A **关闭**：ABI 保留 `LifeState` 的**类型追踪与拒绝说明**；任何 Life capability / Life policy /
`LifeState` 初值或**占位默认值**一律以 **`capability.disabled` 稳定拒绝**，**不创建、不更新**
`LifeState`。**不得**用默认值、占位实现或"伪成功"绕过（S1-05）。

## 5. Outcome / Snapshot / Replay / Seek（`S7A5-R06` / `R07` / `R08`）

### 5.1 两套 header 字段集（`S7A5-R06`）

| header | 字段集（冻结） |
| --- | --- |
| Snapshot | `formatVersion`、四分量 `JudgementIdentity`、`factSemanticRevision`、`stateSchemaRevision`、normalized event count、Fact count、payload byte count、typed byte-budget descriptor |
| Replay | `formatVersion`、四分量 identity、`NormalizationProfile` metadata、effective late-policy metadata、`eventCodecId`、normalized event count、encoded byte count、`ReplayDecodeBudget` |

**拼写规范（normative）**：类型 **`EventCodecId`**、字段 **`eventCodecId`**（小写首字母）。本文档集内
所有出现必须统一到本条。

**revision 归属**：`factSemanticRevision` 描述 **Fact / 排序 / 折叠解释语义**、**进 engine identity**（与
`S7A5-R01` / `R02` 的生成语义同源）；`stateSchemaRevision` **只**标识 `SnapshotPayload` 的**无损状态
结构**、**不进** judgement identity。

### 5.2 `SnapshotPayload` 闭包与不得保存清单（`S7A5-R07`）

**闭包（十四条）**：已提交 Fact Ledger 至 cursor 的**前缀 + cursor**；活动实例；Pattern 状态；
Release / tail phase；exclusive resource 状态；finalization watermark；未提交窗口；`preparedGrace`；
离散 sequence 状态；Hook snapshot；Fold / Score / Combo / Statistics 状态；pending signal queue；
`SessionState`；fault 诊断 / 状态。

**不得保存（五条）**：Presentation / Effect cache；Animation 临时值；HostOverride token；**未提交
StateDelta**；连续采样相位。

**重新取得、不重复保存**：**FactBinding** 与 **prepared immutable graph** 在 header identity 验证后
**重新取得**，不进 payload。typed byte-budget descriptor **不是**已接受的数值承诺。

### 5.3 `SeekLatencyCommitment`（`S7A5-R08`）

`SeekLatencyCommitment` = 带 measurement-profile 引用的 **typed engine commitment**；语义为从最近可用
快照恢复并推进到目标的 **wall-clock seek latency**；**会话只能收紧、不得放宽**；测量入口为
[BUDGET_AND_EVIDENCE_PLAN.md](../../../proposals/gameplay-v2-acceptance/BUDGET_AND_EVIDENCE_PLAN.md) §5 的
restore time、Seek p95 / max 与无损性；**具体 `maxSeekLatency` 数值禁止**进入 Schema / header / 运行时
约束 / 对外承诺，**提供具体值稳定拒绝**；seek **仍须无损**（逐位等于从起点运行）。数值的**首次接受在
S7A-9**。

**plan §S7A-6 第 5 条（落地后的最终文本，逐字引用）**：

> 5. Seek 从最近快照推进，必须逐位等于从起点运行；快照间隔为引擎内部参数，不进入 identity。
>    `Seek 正确性已承诺；SeekLatencyCommitment 的类型和收紧关系已定义，具体 maxSeekLatency 数值待
>    S7A-9 测量并接受。`（第 5 轮 `S7A5-R08`：会话只能收紧、不得放宽；**具体 `maxSeekLatency` 数值禁止**
>    进入 Snapshot / Replay 字段、运行时约束或对外承诺，提供具体值稳定拒绝。）

**修改前（plan §S7A-6 第 5 条原文）**：

```text
5. Seek 从最近快照推进，必须逐位等于从起点运行；快照间隔为引擎内部参数，不进入 identity，
   `maxSeekLatency` 是引擎承诺，会话只能收紧。
```

（旧文本的 `maxSeekLatency` 是**无类型承诺**，容易被读成"已接受的具体值"；新文本把它替换为
`SeekLatencyCommitment` 类型 + 收紧关系 + S7A-9 测量入口。）

## 6. `P1-14` 归属表（`S7A5-R11`）

`P1-14` 的裁决落点是 **Spec 附录 §3.8.6**，只冻结**归属规则与矩阵**、不冻结 REF0 的物理表示。

**归属规则（冻结）**：①任一 gameplay 引用必须**唯一**归入下列三类之一，**不得跨类、不得同时登记**：
**REF0 / judgement closure**（判定必需——缺失或改变会改变 Fact / Score / Combo / Statistics / Fact 顺序 /
identity / 拒绝语义）、**manifest / presentation closure**（纯表现——只影响渲染与资源解析；缺它允许
headless judgement 继续）、**诊断 / source map**（只服务诊断、编辑器与迁移定位，不进任何 closure、不参与
identity）；②判定必需引用**必须**进 REF0，不得只进 manifest 或只留 source map；③纯表现引用**只进**
manifest、**不得**进 REF0；④`sourceMap` **仅诊断**；⑤归属**由 prepare 判定**并随 canonical graph 确定
（同一输入必须得到**同一**归属）；⑥归属无法唯一确定或出现**跨类引用**时**拒绝**（按 §9.3 的 prepare
原子失败条件与 §9.2 的既有类别处置，**不新增 R 编号**）。

**归属矩阵（冻结，8 行，逐字对应 Spec §3.8.6）**：

| 引用 / 内容 | 归属 | 依据 / 说明 |
| --- | --- | --- |
| Requirement / Pattern / Measure / resource / claimKey / emission 的判定引用 | **REF0 / judgement closure** | 缺失或改变即改变判定（§4、§5.2） |
| Requirement identity 六元组与 `compiledSemanticIdentity` 的判定投影 | **REF0 / judgement closure** | identity 变更即判定变更（§2.3、§4） |
| timebase / late policy / normalization profile 的判定投影 | **REF0 / judgement closure** | 改变同 Tick 映射或迟到处置（§3.7） |
| prepared solver profile 与 coordination policy 的判定投影 | **REF0 / judgement closure** | 改变候选排序与资源提交（§3.12） |
| FactBinding 的**判定侧存在性**引用 | **REF0 / judgement closure**（仅存在性与判定域判定） | 判定闭包内悬空必须失败；绑定细节属表现侧 |
| Presentation / Effect graph、材质、Shader、动画模板、音频与表现资源 bytes | **manifest / presentation closure** | 纯表现；缺它允许 headless judgement（第 6 轮 `Q-15` 细化） |
| `render.visible` 等表现 override、UI 文案与默认绑定 | **manifest / presentation closure** | 不进入 judgement identity（§5.2） |
| `sourceMap`、旧字段路径、Studio 节点与模块定位 | **诊断 / source map** | 只服务诊断 / 编辑器 / 迁移（§4） |

**落点与首次消费**：**REF0 的物理字段、编号与编码**由 **S7A-3 的首次 Packed 写入**消费（本节只冻结归属
规则与矩阵、**不冻结表示**）；**S7A-7** 负责 entry / manifest 集成；`P1-14` **不阻塞 S7A-5 / S7A-6**；
ABI 侧只有**指向性引用**、**不新增类型行**。

## 7. 计数变化

| 项 | 第 4 轮落地后 | 第 5 轮落地后 |
| --- | --- | --- |
| `CONTRACT_MATRIX` 六类处置 | `open` 14 / `accept` 38 / `revise` 26 / `supersede` 17 / `retain` 17 / `reject` 2 = **114** | `open` **10** / `accept` **42** / `revise` 26 / `supersede` 17 / `retain` 17 / `reject` 2 = **114** |
| §14 的 `open` 清单与批次表 | 14 项；S7A-5 行 = `CM-S08` / `CM-S10`，S7A-6 行 = `CM-K07` | **10 项**；S7A-5 与 S7A-6 两行均改为 `—（本批次门禁已满足）` |
| `RULING_WORKSHEET` §8 `open` 行 | 14（第 5 轮 **4**） | **10（第 5 轮 0）**；分布 `第 1 轮 5、第 2 轮 0、第 3 轮 0、第 4 轮 0、第 5 轮 0、第 6 轮 5` |
| §0.1 末列（"由哪一轮一并关闭"） | 21 | **21 不变**，但已闭合数由 7 增至 **11**（新增 `CM-F06`、`CM-S08`、`CM-S10`、`CM-K07`） |
| §0.1 行 / 项总计 | 76 行 / 84 项 | **76 行 / 84 项 不变** |
| §7.2 稳定拒绝清单 | 19 条 | **19 条 不变**（package → R-09、correction → R-08、Life → `capability.disabled`，**不新增 R 编号**） |
| §9 裁定登记表 | 第 1–4 行正式登记 | 第 5 行改为**正式登记**（13 行 / `S7A5-R01…R11` / 三卡 provenance / 计数自洽） |
| ABI §类型清单 追踪目录 | 9 域 / 126 = 96 + 30 | **9 域 / 126 = 96 + 30 不变（不新增 ABI 条目）** |
| ABI §未决项 条目数 | 20 条 | **20 条 不变**（无 `#21`；只更新 #3 / #10 / #11 / #12 / #14 / #16 / #20） |

**处置词变化**：`CM-F06`、`CM-S08`、`CM-S10`、`CM-K07` 由 `open` 改为 `accept`。**处置词不变**：
`CM-S02`（`revise`）、`CM-K01`（`revise`）——这两项只写裁定正文与未决点文字。**不进入 CONTRACT_MATRIX**：
`CM-S11`、`CM-K08`（`P1-14` 亦非 CONTRACT_MATRIX 行，作为 Spec 附录登记）。

**ABI 9 行状态格首词复核（第三张卡的直接落点）**：

| 行 | 首词 | 附加限定（本轮追加，逐字） |
| --- | --- | --- |
| `RegisterKind` | `待冻结` | `（CM-S02，第 5 轮，绑定登记；首次消费 S7A-5；附加限定：S7A5-R04 已冻结 exclusive / commutative_monoid / ledger_derived 三类命名与冲突策略；编码与布局仍由首次消费它的后续批次冻结）` |
| `ScoreState` | `待冻结` | `（Q-08 + CM-S10 / P1-07，第 5 轮；首次消费 S7A-5；附加限定：S7A5-R09 已冻结 7A 最小 outcome、category、可选 grade、有符号 tick error 以及事务失败、reset、seek、replay 语义；字段表示、编码与数值预算仍待后续批次）` |
| `ComboState` | `待冻结` | 同 `ScoreState`（逐字相同） |
| `LifeState` | `待冻结` | `（CM-S11，第 5 轮，本次登记新增；首次消费 S7A-5；附加限定：S7A5-R10 已冻结 7A 关闭 Life 与稳定拒绝路径；不得用默认值或占位实现，类型表示与编码仍待后续批次）` |
| `StatisticsSnapshot` | `待冻结` | `（CM-S10 / P1-07，第 5 轮；首次消费 S7A-5；附加限定：S7A5-R09 已冻结由 Fact Ledger 重建及 reset/seek/replay 统计语义；字段表示、编码与数值预算仍待后续批次）` |
| `EventCodecId` | `待冻结` | `（Q-13，第 5 轮；首次消费 S7A-6；附加限定：S7A5-R06 已冻结其 Replay 标识角色与未知/不匹配门禁语义；具体 codec 编码、字节布局与 reference fixture 由 S7A-6 冻结）` |
| `ReplayDecodeBudget` | `待冻结` | `（Q-13，第 5 轮；首次消费 S7A-6；附加限定：S7A5-R06 已冻结事件数、字节数、解码时间三类预算字段及超限错误语义；预算数值仍阻塞 S7A-9）` |
| `SnapshotHeader` | `待冻结` | `（CM-K07 / Q-13，第 5 轮；首次消费 S7A-6；附加限定：S7A5-R06 已冻结四分量 identity、semantic revision、state schema revision、事件/Fact 计数及字节预算字段集；字节布局与预算数值仍待后续批次）` |
| `SeekLatencyCommitment` | `待冻结` | `（CM-K08，第 5 轮，本次登记新增；首次消费 S7A-6；附加限定：S7A5-R08 已冻结类型、承诺语义、会话只能收紧及测量口径入口；具体 maxSeekLatency 数值仍登记为 S7A-9 未冻结预算项，在本轮落地前不进入 Snapshot / Replay 字段、运行时约束或对外承诺，提供具体值稳定拒绝）` |

另有 **2 行非 9 行清单内的状态格**同步为一致口径（首词不变、仍为 `冻结目标`）：
`ReplayHeader` → `冻结目标（字段集已由第 5 轮冻结，CM-K01 / S7A5-R06，见 §S7A-6 节；字节布局待冻结）`；
`SnapshotPayload` → `冻结目标（字段集与闭包已由第 5 轮冻结，S7A5-R07，见 §S7A-6 节与 Spec §3.18；
逐字段表示待冻结）`。**域 5** 的 `FactCategory` 与 `Outcome` 两行（首词仍 `待冻结`）追加了 `S7A5-R09`
的附加限定。**以上改动都不改变 126 条追踪的逐域计数。**

## 8. S7A-5 / S7A-6 不得消费清单

**S7A-5（Spec §3.22，8 项）**：ingress 排序（含 `ingressSequence`、容器顺序与线程完成顺序）；
correction；Ruleset package（含 hash / manifest / 迁移矩阵）；Life（capability / policy / `LifeState`
初值）；默认 grade 表与默认数值限额；未冻结字节编码；未测量 seek 数值；表现资源 / UI / 默认绑定对
judgement 的影响。**另**：不得把 `phasePriority` 用作评分。

**S7A-6（Spec §3.22，S7A-5 的 8 项 + 另 5 项）**：保存表现缓存 / Animation 临时值 / HostOverride
token；保存未提交 delta；保存或恢复连续采样状态（连续重采样格点与采样相位）；保存 snapshot interval
identity；把 typed byte-budget descriptor 解释为**已接受的数值承诺**。

**门禁语义**：清单内每一项都**不得**在本批次被消费、保存或绕过（S1-05 口径：不得用默认值、临时
typedef、序列化编码或"伪成功"实现绕过阻塞）。对应到
[BATCH_GATES.md](../../../proposals/gameplay-v2-acceptance/BATCH_GATES.md) 的"第 5 轮裁定后的 S7A-5 /
S7A-6 门禁"段：两批的**停止条件**都包含"消费清单任一项"与"把未测量数值 / 未冻结编码当作已冻结"。

## 9. 改动的文档与落点

| 文档 | 改动落点（节 / 行与 old → new） |
| --- | --- |
| [RULING_WORKSHEET.md](../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) | 第 11 行 `14 项 open` → **10 项**；§0.1 合计行末列口径段重写（第 2/3/4/5 轮逐轮闭合、末列 **21** 不变、"当前仍 open 者 10 项"）与分轮分布（第 5 轮 **4 项、已闭合**）；§5 全部 **13 行**的 `owner 裁定` 单元格填入（Q-08→R03、CM-S02→R04、Q-17→R05、CM-S08→R05、Q-12→R01+R02、Q-13→R06+R07、CM-K01→R06、CM-K07→R06+R07、CM-K08→R08、CM-F06→R02、CM-S10/P1-07→R09、CM-S11→R10、P1-14→R11）；§5 后的第 5 轮登记块（3 卡 provenance、confidence、**编号修正说明**、门禁归属、**不改计数说明**）；§8 `open` 行 14 → **10** + 分布；长解释段 `14` → `10`（新增 `14→10` 步骤、剩余 10 项清单、`10 + 19 + 15 + 10 + 14 + 3 + 2 = 73`、第 5 轮确认卡一句）；§9 第 5 行替换为**正式登记**（13 行 / `S7A5-R01…R11` / 三卡 / 已闭合项 / `open` 10 / `accept` 42 / 总数 114 / §0.1 末列 21 / §7.2 仍 19 / ABI 126 = 96 + 30） |
| [CONTRACT_MATRIX.md](../../../proposals/gameplay-v2-acceptance/CONTRACT_MATRIX.md) | §0 统计 `accept` **42** / `open` **10** + "10 项 `open` 的清单"；§0 变化叙述追加第 5 轮段（4 项 `open`→`accept`、`CM-S02`/`CM-K01` 仍 `revise`、`CM-S11`/`CM-K08` 非行、3 卡 provenance）；`CM-F06`（§6，L158）→ `accept` + R01+R02 正文；`CM-S02`（§7，L166）**保留 `revise`** + R04 全文；`CM-S08`（§7，L172）→ `accept` + R05；`CM-S10`（§7，L174）→ `accept` + R09；`CM-K01`（§10，L205）**保留 `revise`** + R06 字段集与 `EventCodecId` / `eventCodecId` 拼写规范；`CM-K07`（§10，L211）→ `accept` + R06+R07；§14 计数 → `open` 10 / `accept` 42；§14 S7A-5 / S7A-6 两行 → `—（本批次门禁已满足）`；§14 追加第 5 轮计数变化 bullet；收尾脚本复核行 → 10 项 |
| [GAMEPLAY_V2_SPEC.md](../../../formats/GAMEPLAY_V2_SPEC.md) | 新增 `#### 3.8.6`（`P1-14` 归属规则 6 条 + 8 行矩阵 + 落点段）；§3.9 第 3 条**重写为规范总序**（并注明"原含 `ingressSequence` 的旧 tuple 已被本条取代"）；新增 §3.14（三阶段表 + 7 条规则 + `faulted` 矩阵）、§3.15（`RegisterKind` 表 + 5 条）、§3.16（内置 Ruleset + R-09 + 4 条）、§3.17（Fact 身份 / commit / revision，9 条）、§3.18（两套 header 字段集 + 闭包 14 项 + 不得保存 5 项 + 8 条）、§3.19（`SeekLatencyCommitment`，7 条）、§3.20（Outcome / category / grade / `TimingError`，7 条）、§3.21（Life 关闭，4 条）、§3.22（两批不得消费清单）；§7.2 R-09 行更新 + 第 5 轮拒绝映射表（4 场景、**不新增 R 行**）+ S7A-5 / S7A-6 指针；§9.4 按 R03 重写（4 条，含"faulted 不可新建 snapshot"）；§10 表行与 bullet 1–6 重写（含 `maxSeekLatency` 数值禁令与 faulted / snapshot 消歧）；§11 引入语改"第 6 至第 7 轮…六节"、第 5 轮行改"已裁定" + 新增后续批次行、§11.1 第 3 / 6 条更新并新增第 7 条；§1.1 第 2 条重写（第 6/7 轮待定）；四处 §S7A-x 的"第 5 至第 7 轮" → "第 6 至第 7 轮"；§S7A-4 表行① 字节合同归属 → S7A-6；新增 `## S7A-5 限定冻结范围与登记规则（2026-10-03）`（7 行范围表 + 不得消费清单 + 收尾声明）与 `## S7A-6 …` |
| [GAMEPLAY_V2_ABI.md](../../../api/GAMEPLAY_V2_ABI.md) | §类型清单 域 5 `FactCategory` / `Outcome`、域 6 `RegisterKind` / `ScoreState` / `ComboState` / `LifeState` / `StatisticsSnapshot`、域 9 `ReplayHeader` / `EventCodecId` / `ReplayDecodeBudget` / `SnapshotHeader` / `SnapshotPayload` / `SeekLatencyCommitment` 的状态格（**9 行首词不变**，见 §7 表）；域 6 原子性规则段 + 新增"第 5 轮在本域冻结的语义"5 条；域 9 收尾段 + 新增"第 5 轮在本域冻结的语义"9 条；§S7A-1 的轮次范围（"第 5 至第 7 轮" → "第 6 至第 7 轮"、"第 2、3、4 轮" → "第 2、3、4、5 轮"）与 §S7A-2 / §S7A-3 / §S7A-4 的授权段与表行归属同步；§未决项 前言（新增第 5 轮段、#3 / #10 / #11 / #12 / #14 / #16 / #20 更新、**不新增条目**）+ `P1-14` 指向段；新增 `## S7A-5 限定冻结范围与登记规则（2026-10-03）` 与 `## S7A-6 …` 两节；§相关索引 的 worksheet 行（第 1 至第 5 轮结论） |
| [OPEN_QUESTIONS.md](../../../proposals/gameplay-v2-acceptance/OPEN_QUESTIONS.md) | 更新日期行；§1 `Q-08` / `Q-12` / `Q-13` / `Q-17` 四行与 §3 `P1-07` / `P1-14` 两行的裁定指针（**保留 2026-10-02 原始文字**）；新增 **§4.2 第 5 轮裁定登记**（provenance + **13 行**表 + 计数变化段 + 门禁归属）；§6 第五批与第六批标注"已裁定" |
| [README.md](../../../proposals/gameplay-v2-acceptance/README.md) | §1 `RULING_WORKSHEET` 行（14 项 → **10 项**、§2/§3/§4/**§5**/§9）；§2 A4 行（第 1–5 轮已裁定、第 6–7 轮待裁定）；§4.3 新增第 5 轮段（13 行、4 项 `open`→`accept`、`open` 14→10 / `accept` 38→42、`P1-14` 不阻塞、`CM-S02`/`CM-K01` 不变） |
| [BATCH_GATES.md](../../../proposals/gameplay-v2-acceptance/BATCH_GATES.md) | §1 S7A-5 行（正例 / 负例 / 诊断 / golden / 停止条件全面扩写，负例含**不得消费清单 8 项**）；§1 S7A-6 行（同上，负例含**13 项**）；新增"**第 5 轮裁定后的 S7A-5 / S7A-6 门禁（2026-10-03）**"段（3 卡 provenance + 两批正例 / 负例 / golden / 停止条件表 + 共同判定口径 4 条） |
| [ADR 0044](../../../adr/0044-gameplay-v2-semantic-kernel.md) | "仍待裁定的部分"标题与正文（"第 5 至第 7 轮 44 行 / 49 项" → "**第 6 至第 7 轮 31 行 / 35 项**"，算式 `76 − 14 − 6 − 6 − 6 − 13 = 31`、`84 − 15 − 7 − 7 − 6 − 14 = 35`）；第 5 轮表行改为**已裁定**并列出十三项落点；未决与后续第 2 项；证据边界第 7 项（追加第 5 轮三卡） |
| [plan.md](../../../stage_plans/active/stage-07/plan.md) | §1.1（第 5 至第 7 轮 → 第 6 至第 7 轮、第 2/3/4/5 轮已裁定）；§S7A-4 **第 5 步**替换为规范总序（见 §3）；§S7A-5 第 5 条与验证段（三阶段 / `faulted` / `RegisterKind` / package / Life / 不得消费清单）；§S7A-6 第 2、4、5、6 条与验证段（两套 header、闭包、`SeekLatencyCommitment`、faulted 不可新建 snapshot、不得保存清单）；§3.1 台账 S7A-5.1…5.5 与 S7A-6.2 / 6.3 / 6.4 行的验收文字（**不新增工作项**） |
| [CURRENT_STATUS.md](../../../CURRENT_STATUS.md) | L241 "第 5-7 轮语义仍**不授权**" → "第 6-7 轮…（第 2、3、4、5 轮已裁定的语义由各自小节授权）" |
| [reading-order.md](../../../guides/reading-order.md) | L61 "仍待第 5-7 轮语义裁定（第 1、2、3、4 轮已裁定…§3、§9）" → "仍待第 **6-7** 轮…（第 1、2、3、4、**5** 轮已裁定…§3、**§5**、§9）" |
| [stage_reports/README.md](../../README.md) | Stage 7A 清单追加本文条目 |
| 本文（新增） | 第 5 轮裁定的带日期落地证据 |

**未改动的相邻文档（按约束）**：`2026-10-02-*.md` 与 `2026-10-03-s7a-2/3/4-*.md` 四份既有带日期报告
（按"新增带日期记录、不改历史"口径**只读**）；`SUPPORT_AND_REJECTION_MATRIX.md`、
`BUDGET_AND_EVIDENCE_PLAN.md` §5、`SEMANTIC_KERNEL.md`、`PRELIMINARY_DESIGN.md`（本轮只作为引用来源）。

## 10. 跑过的门禁与结果

| 门禁 | 命令 / 方式 | 结果 |
| --- | --- | --- |
| 文档检查 | `python -B tools/check_docs.py` | 见 §10.1 |
| 空白检查（已跟踪差异） | `git diff --check` | exit 0（仅 LF/CRLF 提示，见 §10.2） |
| 空白 / 制表符扫描（含未跟踪文件） | 逐文件正则扫描全部改动文件 + 本文 | 0 处尾随空白、0 处制表符 |
| 计数脚本直读复核 | 见 §10.3 | 六类处置 114（`open` 10 / `accept` 42）；§14 `open` 10；§8 `open` 10；§0.1 76 行 / 84 项 / 末列 21；§7.2 19 条；ABI 9 域 126 = 96 + 30 |
| 陈旧值 grep | 见 §10.4 | 残余命中逐条解释 |

### 10.1 `check_docs.py` 实际输出

```text
Documentation checks passed: 335 Markdown files and 20 candidate JSON/CXT files validated.
```

exit code **0**。在本文创建前，同一命令报 **17 处** `broken relative link:
.../2026-10-03-s7a-5-6-gate-rulings.md`（分布在 ADR 0044 ×2、`GAMEPLAY_V2_ABI.md` ×3、
`GAMEPLAY_V2_SPEC.md` ×6、`BATCH_GATES.md` ×1、`CONTRACT_MATRIX.md` ×2、`OPEN_QUESTIONS.md` ×1、
acceptance `README.md` ×1、`RULING_WORKSHEET.md` ×1）——这 17 条命中恰好证明各文档对本文的**相对链接
深度正确**，本文创建后全部消失。

### 10.2 空白扫描

`git diff --check`：`exit=0`，仅输出 `warning: in the working copy of '...', LF will be replaced by CRLF
the next time Git touches it`（行尾风格提示，不是 `--check` 的错误）。

逐文件扫描（本文档集的 **13** 份改动文件，尾随空白 + 制表符）：

```text
files=13 trailingWS=0 tabs=0
```

### 10.3 计数脚本直读复核

```text
CONTRACT_MATRIX contract rows: 114
dispositions: {'accept': 42, 'revise': 26, 'supersede': 17, 'retain': 17, 'open': 10, 'reject': 2} total: 114
sec14 open bullet: ['10']
sec14 accept bullet: ['42']
RW sec8 open row: | `open` 合同项（CONTRACT_MATRIX §14） | 10 | 第 1 轮 5、第 2 轮 **0**、第 3 轮 **0**、第 4 轮 **0**、第 5 轮 **0**、第 6 轮 5 |
RW sec01 合计 row: | 合计 | — | **76 行 / 84 项** | **21**（...） |
RW sec9 round rows: ['1', '2', '3', '4', '5', '6', '7']
SPEC 7.2 R-rows: 19 ['R-01', ..., 'R-19']
ABI domain rows: 126 frozen 96 pending 30
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

结论：**本轮不新增 ABI 条目**——9 域 / 126 = 96 + 30 与第 4 轮完全一致；域 5 / 域 6 / 域 9 的改动只落在
状态格文字与既有段落，`冻结目标` / `待冻结` 首词分布不变。**30 条 `待冻结` 行的首词逐条复核全部保持
`待冻结`**（含第 5 轮涉及的 9 行）。

### 10.4 陈旧值 grep 与残余命中（逐条解释）

| 陈旧值 | 命中 | 逐条解释 |
| --- | --- | --- |
| `ingressSequence`（大小写不敏感） | Spec **4**、ABI **4**、ADR 0044 **1** | Spec 的 4 处全部是**禁令语境**（§3.9 第 3 条 ×2："不得使用…"与"原含 … 的旧 tuple 已被本条取代"；§3.17；§3.22）；ABI 中 **2 处是既有类型名 `IngressSequence`**（域 2 类型行与 Gameplay I → V2 字段映射表，描述"宿主提供的会话内单一序列"这一**类型**，不是排序键）、另 2 处为禁令（§未决项 #10 与 §S7A-5 节）；ADR 0044 的 1 处在第 5 轮表行内、同为禁令。**无一处**把它写成有效排序键；plan.md 的同类命中亦为禁令（`不得` 使用）。`第 5 至第 7 轮` 在 Spec / ABI / plan / ADR 中为 **0** |
| `第 5-7 轮` | 0 | `CURRENT_STATUS.md` L241 与 `reading-order.md` L61 本轮**已改**为"第 6-7 轮"；两份 `2026-10-03-s7a-3/4-gate-rulings.md` 中的同类字符串属**带日期历史证据**、按约束不改 |
| `第 5 至第 7 轮` | 0（约定范围内） | Spec / ABI / plan / ADR 均已改为"第 6 至第 7 轮"；历史报告内的同类表述属历史证据 |
| `14 项 open` / `` `open` 14 `` | 0 | worksheet 第 11 行、§8 `open` 行、CONTRACT_MATRIX §0 / §14、README §1 与 §4.3 均改为 10；变化叙述中的"14 → 10"起点值不匹配该模式（写作"由 14 降为 10"），属预期 |
| `` `open` 合同项由 16 降为 **14** `` 等 | 保留 | 第 3 / 4 轮变化叙述中的**起点值**，属历史叙述、非现值 |
| `causal total order` | 保留（Spec §3.9 第 3 条标题性引用 + 历史报告） | 该词已由规范总序取代；Spec 正文只在"由第 5 轮定案"的表述中出现一次，其余在带日期历史报告中 |
| `只启用前三态` / `solver 边界矛盾` 等第 4 轮陈旧值 | 不适用 | 本轮未触碰第 4 轮措辞 |
| `phasePriority` 用作评分 | 0 | 全部为禁令或"仅用于 `canonicalOrdinal`"的限定表述 |

**额外发现（非本轮引入，未改）**：`CONTRACT_MATRIX.md` §13 的 `CXC_FORMAT.md` 行与 `OPEN_QUESTIONS.md`
§4 `P2-05` 行在代码跨度内含**未转义的 `|`**，会使该表格多出一列；该项与第 5 轮无关，登记为后续一行
修正（与第 4 轮报告 §10.4 的同一发现一致）。

**额外发现（本轮新登记，未改）**：`GAMEPLAY_V2_SPEC.md` 的 §S7A-2 行⑤"提交事务的字段布局与
Snapshot / Replay 承载"与 §S7A-3 行③"Release / tail 的 Snapshot / Replay 承载与逐字段 phase 表"两处，
其"（第 5 / 6 轮）"归属在本轮之后应读作"字段集已由第 5 轮冻结、逐字段与字节部分属 S7A-6"；ABI 对应
两行本轮已按此改写，**Spec 侧同义两行保留原文**以避免扩大本轮改动面。**建议在 S7A-6 落地批次一并
统一措辞。**

### 10.5 解释性判断与需 owner / Codex 复核项

1. **`S7A5-R11` 的编号归属是本轮最容易误记的一点。** 三张卡的原始表述里，`P1-14` 与 `CM-S11` 相邻出现；
   本文与 worksheet §5 按"**`R10` 只用于 `LifeState`，`P1-14` 另登记 `R11`**"处理，并在 worksheet §5
   的登记块里写明该修正。**若 owner 认为 `P1-14` 应并入既有编号体系而不是新起 `R11`，需 Codex 复核。**
2. **`S7A5-R01` 把 `tick` 解释为 `commitTick`。** 该解释与第 2 轮的 `commitTick` / commit window 定义
   （Spec §3.7.5）一致，但它使"Fact 的 `tick` 字段"与"`observationTick`"的分工更严格（后者降为派生
   输入）。**本轮按卡 1 的措辞落地**；若后续批次希望保留 `observationTick` 作为 Fact 的可查询字段，
   需在不改变总序的前提下另行裁定。
3. **"faulted 不可新建 snapshot"与"闭包含 fault 诊断 / 状态"的并存**：本文按 Spec §3.14 第 7 条与
   §10 第 6 条解释为"快照**不是** faulted 会话的新产物，但**既有快照的 payload 结构**必须能承载 fault
   信息以便恢复后查询"。**两句话不矛盾**，但实现者容易读成"faulted 仍可 snapshot"，故在 plan §S7A-6
   第 6 条与 BATCH_GATES 停止条件中显式重复。**该消歧属解释性落地，建议 owner 在 S7A-6 前复核一次。**
4. **`SeekLatencyCommitment` 的"数值禁止"范围**：本轮把"具体 `maxSeekLatency` 数值"排除在 Schema /
   header / 运行时约束 / 对外承诺之外，并把测量入口指向 `BUDGET_AND_EVIDENCE_PLAN.md` §5。**该 §5 本轮
   未改**（按要求只引用），因此"S 型预算的登记格式"仍按该文件的既有口径执行。
5. **`P1-14` 的 8 行矩阵属于"行为冻结"**：REF0 的物理字段 / 编号 / 编码留给 S7A-3 的首次 Packed 写入。
   这意味着 **S7A-3 开工前必须先把该矩阵映射成物理字段表**，否则 §3.8.6 只有规则没有载体。**该前置项
   已在 P1-14 行登记为首次消费 S7A-3。**
6. **`CM-S02` / `CM-K01` 处置词保持 `revise`**：语义已冻结但处置词不改为 `accept`，原因是两者的
   "字段集 / 编码"仍部分未冻结。**若 owner 希望按"语义已冻结即可 `accept`"的口径统一，需重新裁定
   处置词并重算六类计数**（本轮不改，计数自洽以卡 2 为准）。

### 10.6 工作区归属确认

**本轮的改动只有 13 份文件**（worksheet、contract matrix、SPEC、ABI、OPEN_QUESTIONS、acceptance README、
BATCH_GATES、ADR 0044、plan.md、CURRENT_STATUS、reading-order、stage_reports/README、本文），全部落在
本轮的文档授权面内。**未触碰** `engine/judgement/**`、`tests/judgement/**`（并行 S7A-2 输入半区的实现与
测试）与 `cmake/VerifyArchitecture.cmake`（并行任务持有）；**无** `git add`、**无**提交、**无**临时脚本
残留（本轮只使用 `python -B tools/check_docs.py` 与 `git diff --check` 两条既有命令，以及只读的
PowerShell 统计，工作区未新增任何工具文件）。

**归属说明（避免误读 `git status`）。** 本工作区是"新阶段文档集尚未纳入版本控制"的状态：
`git status --porcelain -uall` 共 **82** 条（**30** 条已跟踪修改 + **52** 条未跟踪），其构成是——
(a) 30 条**既有已跟踪修改**（`.gitignore`、`AGENTS.md`、根 / `engine` / `tests` 的 `CMakeLists.txt`、
`cmake/VerifyArchitecture.cmake`、`cmake/VerifyExternalConsumer.cmake`、`tools/check_version_gate_tests.py`、
`tools/research/gameplay_fold_spike/*` 与 Stage 7A 早期落地的若干 docs 标注），(b) **未跟踪**的 V2 文档集
（ADR 0044 / Spec / ABI / acceptance package / research 稿 / stage-07 带日期报告），(c) **未跟踪**的
`engine/judgement/**` 与 `tests/judgement/**` 共 **18** 个文件（并行 S7A-2 输入半区）。**(a) 与 (c) 都不由
本轮产生**；本轮只改动了 (b) 中属于本文档集的 13 份 + 新增本文，因此**不能**用 `git status` 的条数衡量
本轮的改动面。**本轮不新增、不修改任何 C++ 文件**。

## 11. 相关索引

- [Gameplay V2 字段与运行语义规范](../../../formats/GAMEPLAY_V2_SPEC.md)：§3.8.6、§3.9 第 3 条、
  §3.14–§3.22 与 §S7A-5 / §S7A-6 两节的权威正文
- [Gameplay V2 ABI](../../../api/GAMEPLAY_V2_ABI.md)：域 5 / 域 6 / 域 9、§未决项 与 §S7A-5 / §S7A-6 两节
- [未决语义分轮裁决清单](../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md)：§5 裁定与 §9 登记
- [S7A-0.2 合同逐项台账](../../../proposals/gameplay-v2-acceptance/CONTRACT_MATRIX.md)：114 项与 10 项 `open`
- [未决问题与 owner 决策请求](../../../proposals/gameplay-v2-acceptance/OPEN_QUESTIONS.md)：§4.2 第 5 轮登记
- [支持 / 拒绝矩阵](../../../proposals/gameplay-v2-acceptance/SUPPORT_AND_REJECTION_MATRIX.md)：19 条稳定拒绝
- [批次门禁](../../../proposals/gameplay-v2-acceptance/BATCH_GATES.md)：第 5 轮裁定后的 S7A-5 / S7A-6 门禁
- [Stage 7A 实施计划](../../../stage_plans/active/stage-07/plan.md)：§S7A-4 第 5 步、§S7A-5、§S7A-6
- [第 4 轮门禁报告](2026-10-03-s7a-4-gate-rulings.md) · [第 3 轮门禁报告](2026-10-03-s7a-3-gate-rulings.md) ·
  [第 2 轮门禁报告](2026-10-03-s7a-2-gate-rulings.md) ·
  [第 2 轮实现裁定记录](2026-10-03-s7a-2-implementation-rulings.md) ·
  [S7A-0 接受记录](2026-10-02-s7a-0-acceptance.md)
- [预算与证据计划](../../../proposals/gameplay-v2-acceptance/BUDGET_AND_EVIDENCE_PLAN.md) §5：Seek 延迟测量的入口

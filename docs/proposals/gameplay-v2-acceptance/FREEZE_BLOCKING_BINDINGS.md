# Gameplay V2 ABI 待冻结条目绑定登记（S7A-1 登记缺陷处置）

状态：candidate（已由 Codex 裁决并落地登记：thread `s7a1-freeze-bindings`，verdict `adopt`，confidence 0.96，2026-10-03。仍属 candidate 体系，不构成生产冻结或实施授权）

更新日期：2026-10-03

上级文档：[Gameplay V2 acceptance package](README.md) ·
[未决语义分轮裁决清单](RULING_WORKSHEET.md) ·
[Stage 7A 实施计划](../../stage_plans/active/stage-07/plan.md)

文档角色：登记与处置记录（原为提案，2026-10-03 裁决后转为登记）。它处置一件事：
[Gameplay V2 ABI](../../api/GAMEPLAY_V2_ABI.md) 的 30 条 `待冻结` 条目中，**缺可追溯阻塞批次**的 16 条的
最终绑定——裁决编号、首次消费批次与附加限定；并保留依据、分组小结、其余 14 条的核实、Codex 裁定栏与风险。

## 1. 性质与效力

1. 本文件的状态词仍是 `candidate`；它是**登记与处置记录**，不是 ADR、Spec、ABI 或实施授权，也不构成
   生产冻结。"拟绑定"字样只出现在对提案基线的引述里，最终值以 §3 的"最终裁决编号（轮次）"列与 §6 为准。
2. 绑定已由 Codex 于 **2026-10-03** 裁决（thread `s7a1-freeze-bindings`，verdict `adopt`，confidence 0.96）
   并**落地登记**：ABI 的 16 条状态格已写为 `待冻结（<编号/轮次>；首次消费 S7A-x）`，
   [RULING_WORKSHEET.md](RULING_WORKSHEET.md) §7B 给出 16 行登记表，ABI `§登记缺陷` 已标为 2026-10-03 关闭。
3. 本次落地只涉及"裁决编号 + 首次消费批次 + 附加限定"与相关计数/缺陷登记：它**不改** Spec 语义、
   不改 ADR 0044 的决策、不改 Stage plan 的批次范围，也不改 CMake/C++。
4. 本文件**不**声称任何条目已被冻结、已获授权或已可实施：`待冻结` 行在对应轮次裁定前仍是阻塞项；
   "以后再定"仍不是可接受的裁定。

## 2. 为何需要

1. **S1-03 的登记要求。** [RULING_WORKSHEET.md §7A](RULING_WORKSHEET.md#L198-L204) 第 202 行（owner
   2026-10-02 接受）规定：ABI 每一条必须登记为"本批次角色/边界冻结"或"**登记为后续批次阻塞项**"，
   后者**必须关联裁决编号与首次消费批次**。同口径写进 [plan.md §S7A-1](../../stage_plans/active/stage-07/plan-a.md)
   第 229-232 行（"后者须带裁决编号与首次消费批次"）。
2. **S7A-1 的退出判据含"按域计数、无遗漏"。** plan 第 232 行把核对方式定为"**按域计数、无遗漏**"，
   ABI [§126 条追踪规则](../../api/GAMEPLAY_V2_ABI.md#L473-L505) 第 497-500 行给出可机械复核的口径
   （九段行数合计 126；每行状态格首词命中两者之一；不得出现第三种登记或未登记的条目）。
3. **现状存在登记缺陷。** ABI §登记缺陷 第 516-529 行自述：30 条 `待冻结` 中 16 条未给出可核对的
   阻塞轮次——13 条状态格完全没有裁决编号，3 条给出编号但该编号在裁决清单的批次表中未绑定轮次。
   这些行既不能算"本批次角色/边界冻结"（首词是 `待冻结`），也不满足"阻塞项须带编号与批次"，
   因此在被处置前会**卡住 S7A-1 的退出判据**。
4. **编号不能编造。** 绑定编号只从既有轮次范围取值，或在裁决明确授权时新增一条（[RULING_WORKSHEET.md](RULING_WORKSHEET.md)
   第 2 轮…第 7 轮表格中的 `CM-xxx` / `P1-xx` / `Q-xx`，见 [§0.1](RULING_WORKSHEET.md#L29-L47) 与各轮表）。
   本次最终新增 3 条（`CM-T13`、`CM-S11`、`CM-K08`，均标注"本次登记新增"），并把两条既有编号
   （`CM-S02`、`CM-K01`）补登记到第 5 轮表行；除这 3 条外没有硬填或编造编号。

## 3. 主表：16 条缺可追溯阻塞批次的 `待冻结` 条目（最终绑定）

行号以工作区当前文件为准（复核方式见 §8）。**条目编号**约定为 `ABI:<行号>`：`<行号>` 指
[GAMEPLAY_V2_ABI.md](../../api/GAMEPLAY_V2_ABI.md) 当前行号，与 ABI §登记缺陷 的 (a)/(b) 类行号逐条一致
（13 条 (a) 类与 3 条 (b) 类，未发生偏移）。本表已按 Codex 裁决改为**最终结果**："Codex 裁定与附加限定"列
给出逐条裁定、与原候选的差异（若有）以及最终附加限定；原提案的候选绑定不再单列一份，避免两份互相矛盾的"拟绑定"。

| 条目编号 | 名称 | 所属域 | 登记前状态（ABI 状态格） | 最终裁决编号（轮次） | 首次消费批次 | Codex 裁定与附加限定（2026-10-03） | 依据 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `ABI:67` | `TickSpan` / `TickDelta` | 域 1 时间基 | `待冻结` | `Q-04` / `CM-T03`（第 2 轮） | S7A-2 | **接受并登记**（原候选同此）。附加限定：随 `CM-T03` 的 Tick 宽度与 tie rule 一并裁定 | [RULING_WORKSHEET §2](RULING_WORKSHEET.md#L99-L107) 第 103-104 行（Q-04 七问含负时间与溢出；CM-T03 定 Tick 映射与 tie rule）；[plan §S7A-1](../../stage_plans/active/stage-07/plan-a.md) 第 245-249 行把 `Tick` 存储宽度与时间转换登记为第 2 轮阻塞项；[plan §S7A-2](../../stage_plans/active/stage-07/plan-a.md) 第 272-283 行与 [§3.1 S7A-2.1](../../stage_plans/active/stage-07/plan-a.md) |
| `ABI:85` | `ContactRef` | 域 2 输入与观测 | `待冻结（7A 只登记，不做 handoff）` | `Q-11` / `CM-C09`（第 4 轮） | S7A-4 | **修改后接受**（原候选把"S7A-4 是否消费最小 contact handle"列为 owner 待裁定点）。附加限定：S7A-4 只消费最小 contact handle，不消费 handoff / `sameContact` 语义 | [RULING_WORKSHEET §4](RULING_WORKSHEET.md#L125) 第 125 行（Q-11 七问含 `lease/contact identity`，且"7A 只接受 `capacity = 1`、exclusive、无复杂 handoff"）；ABI §未决项 #8（第 578 行）阻塞 S7A-4；[plan §S7A-4](../../stage_plans/active/stage-07/plan-a.md) 第 352-355 行 |
| `ABI:86` | `DomainAmount` | 域 2 输入与观测 | `待冻结（量程待冻结）` | `CM-T13`（第 2 轮，**本次登记新增**：T 系列下一空号，Codex 2026-10-03 裁决授权新增一条第 2 轮裁决） | S7A-2 | **修改后接受**（原候选为"（无既有编号）建议并入第 2 轮或新增"）。附加限定：S7A-2 的"越界量程"负例以该裁决为准 | [plan §S7A-1](../../stage_plans/active/stage-07/plan-a.md) 第 223 行把 `Amount`（`DomainAmount`）列入本批次必须覆盖的角色；[plan §S7A-2](../../stage_plans/active/stage-07/plan-a.md) 第 276-279 行（离散量与量化边界）；[BATCH_GATES §S7A-2](BATCH_GATES.md#L51-L61) 第 56 行把"越界量程"列为负例 |
| `ABI:110` | `Measure` | 域 3 Requirement、Pattern 与 Measure | `待冻结` | `CM-S10` / `P1-07`（第 5 轮） | S7A-4 | **修改后接受**（原候选标记"时点冲突"，把"S7A-4 能否移出该表示或把编号提前"留给 owner）。附加限定：S7A-4 仅消费单分量量化结果与有符号 tick error；grade / 多分量聚合留 S7A-5 | [RULING_WORKSHEET §5](RULING_WORKSHEET.md#L142) 第 142 行（7A 最小结果集合、Hold 的 head/body/tail 局部结果、错误单位）；[plan §S7A-4](../../stage_plans/active/stage-07/plan-a.md) 第 345-351 行与 [§3.1 S7A-4.2 / S7A-4.3](../../stage_plans/active/stage-07/plan-a.md)；对照 [plan §S7A-3.3](../../stage_plans/active/stage-07/plan-a.md) 的多分量 Measure/grading fixture |
| `ABI:132` | `ResourceLease` | 域 4 协调与资源 | `待冻结` | `Q-11` / `CM-C09`（第 4 轮） | S7A-4 | **接受并登记**。附加限定：`capacity > 1`、handoff 保持稳定拒绝 | [RULING_WORKSHEET §4](RULING_WORKSHEET.md#L125) 第 125 行（lease identity、`capacity = 1` 与 `> 1` 用不同状态表）；ABI §未决项 #8（第 578 行）阻塞 S7A-4；[plan §S7A-4](../../stage_plans/active/stage-07/plan-a.md) 第 352-355 行 |
| `ABI:143` | `CandidateId` | 域 4 协调与资源 | `待冻结` | `Q-16` / `CM-C05`（第 4 轮） | S7A-4 | **修改后接受**（原候选为"（无既有编号）建议并入 `Q-16` / `CM-C05`"；最终按该编号绑定）。附加限定：只承担确定性候选标识 | [plan §S7A-4](../../stage_plans/active/stage-07/plan-a.md) 第 339 行固定顺序第 4 步"每个事件重建候选并按 resource / claimKey / requirementId / fanout 仲裁"；[RULING_WORKSHEET §4 第 126 行](RULING_WORKSHEET.md#L126)（Q-16 两级阶段顺序）与 [第 128 行](RULING_WORKSHEET.md#L128)（CM-C09 仲裁顺序） |
| `ABI:144` | `LocalOutcome` | 域 4 协调与资源 | `待冻结` | `CM-S10` / `P1-07`（第 5 轮） | S7A-4 | **修改后接受**（原候选为 `P2-04`（第 6 轮）为主、`CM-S10` / `P1-07`（第 5 轮）为备；最终取 `CM-S10` / `P1-07`（第 5 轮））。附加限定：S7A-4 仅消费 Hold phase-local 的最小 `localOutcome`，完整 outcome / grade 由 S7A-5 冻结 | [RULING_WORKSHEET §6 第 158 行](RULING_WORKSHEET.md#L158) 逐字点名 `localOutcome`；[§5 第 142 行](RULING_WORKSHEET.md#L142) 含"Hold 的 head/body/tail 局部结果"；[plan §S7A-4](../../stage_plans/active/stage-07/plan-a.md) 第 349-351 行 Hold head/body/Release/tail |
| `ABI:182` | `RegisterKind` | 域 6 Ruleset 事务与会话状态 | `待冻结（CM-S02）`（编号存在，未绑轮次） | `CM-S02`（第 5 轮，**绑定登记**：既有编号、此前无轮次表行） | S7A-5 | **修改后接受**（原候选为"建议第 5 轮新增，或在 `Q-08` / `CM-S10` 行内追加"；最终在 §5 新增 `CM-S02` 行，标注 `绑定登记`）。附加限定：由第 5 轮新增 RegisterKind 裁决承载 | [plan §S7A-5](../../stage_plans/active/stage-07/plan-a.md) 第 372-375 行（Interface / arbitration / fold / modules、可交换可结合的合成算子）；[CONTRACT_MATRIX.md 第 150 行](CONTRACT_MATRIX.md#L150)（`CM-S02` 处置 `revise`：命名与冲突策略需冻结） |
| `ABI:186` | `ScoreState` | 域 6 Ruleset 事务与会话状态 | `待冻结` | `Q-08` + `CM-S10` / `P1-07`（第 5 轮） | S7A-5 | **接受并登记**（原候选的 owner 待裁定点为 `ScoreState` 字段集与饱和/溢出语义；该点并入第 5 轮 `Q-08` + `CM-S10` / `P1-07` 裁定） | [plan §S7A-5](../../stage_plans/active/stage-07/plan-a.md) 第 376-377 行与 [§3.1 S7A-5.3](../../stage_plans/active/stage-07/plan-a.md)；[BATCH_GATES §S7A-5](BATCH_GATES.md#L87-L97) 第 91 行；[RULING_WORKSHEET §5 第 135 行](RULING_WORKSHEET.md#L135)（Q-08："失败语义不定则 Score 与 Snapshot 在 fault 后无定义"） |
| `ABI:187` | `ComboState` | 域 6 Ruleset 事务与会话状态 | `待冻结` | `Q-08` + `CM-S10` / `P1-07`（第 5 轮） | S7A-5 | **接受并登记**（同上；断连判定与重算规则并入第 5 轮同一裁定） | 同 `ScoreState`；另见 [plan §3.1 S7A-5.3](../../stage_plans/active/stage-07/plan-a.md) 第 521 行"连击断开"与 [BATCH_GATES §S7A-5](BATCH_GATES.md#L91) 第 91 行"combo 断连" |
| `ABI:188` | `LifeState` | 域 6 Ruleset 事务与会话状态 | `待冻结` | `CM-S11`（第 5 轮，**本次登记新增**） | S7A-5 | **修改后接受**（原候选为"需新增裁决，或明确 7A 关闭 Life 并给出拒绝路径"；最终新增 `CM-S11` 并由第 5 轮裁定）。附加限定：7A 关闭 Life，输入该能力必须稳定拒绝 | [plan §S7A-5](../../stage_plans/active/stage-07/plan-a.md) 第 376-377 行"可选 LifeState"、[§3.1 S7A-5.3](../../stage_plans/active/stage-07/plan-a.md) 第 521 行"可选 Life policy"、[BATCH_GATES §S7A-5](BATCH_GATES.md#L91) 第 91 行"life 边界" |
| `ABI:189` | `StatisticsSnapshot` | 域 6 Ruleset 事务与会话状态 | `待冻结` | `CM-S10` / `P1-07`（第 5 轮） | S7A-5 | **接受并登记** | [RULING_WORKSHEET §5](RULING_WORKSHEET.md#L142) 第 142 行"seek 后统计从 Fact Ledger 重建，reset 清空且不产生 Fact"；[OPEN_QUESTIONS.md 第 62 行](OPEN_QUESTIONS.md#L62)（P1-07 含"统计累计与 reset/seek 规则"）；ABI §未决项 #11（第 581 行）阻塞 S7A-5 |
| `ABI:237` | `CapabilityRecord` | 域 8 capability、入口与诊断 | `待冻结（CM-X01）`（编号存在，未绑轮次） | `CM-X01`（第 6 轮，**绑定登记**：既有编号、此前无轮次表行，与 `Q-15` / `Q-19` 同批） | S7A-7 | **修改后接受**（原候选为"（无轮次表编号）"、首次消费 S7A-1；最终把 `CM-X01` 绑定到第 6 轮，与 `Q-15` / `Q-19` 同批，首次消费改登记为 S7A-7）。D-14 按方案 (a) 处置：以 ABI 第 237 行的 `待冻结` 为准，并把 [CONTRACT_MATRIX.md](CONTRACT_MATRIX.md) 的 `accept` 修正为 `open`。附加限定：S7A-1 只登记角色与"阻塞 S7A-7 capability registry 门禁"，不实现字段布局或运行时表面 | [plan §S7A-1](../../stage_plans/active/stage-07/plan-a.md) 第 220-227 行把"诊断和 capability"列入本批次必须覆盖的角色；[BATCH_GATES §S7A-1](BATCH_GATES.md#L39-L49) 第 45 行诊断类别 `unsupported_capability`；[CONTRACT_MATRIX.md 第 211 行](CONTRACT_MATRIX.md#L211)（`CM-X01` 处置 `accept`、未决点"无"）；缺陷 `D-14`（[OPEN_QUESTIONS.md](OPEN_QUESTIONS.md) §5 的 D-14 行，2026-10-03 按方案 (a) 处置） |
| `ABI:270` | `EventCodecId` | 域 9 Replay、Snapshot 与 Seek | `待冻结（CM-K01）`（编号存在，未绑轮次） | `Q-13`（第 5 轮），并把 `CM-K01` 绑定到同一轮次（**绑定登记**） | S7A-6 | **修改后接受**（原候选为"owner 确认把 `CM-K01` 一族统一绑到第 5 轮即可"；最终显式绑定：§5 新增 `CM-K01` 行，标注 `绑定登记`）。附加限定：把 `CM-K01` 绑定到同一轮次 | [RULING_WORKSHEET §5 第 139 行](RULING_WORKSHEET.md#L139)（Q-13："Snapshot/Replay schema 未与 V2 状态闭包对齐"）；ABI §仍待冻结 第 436 行（"Replay header 字段集与 `eventCodecId`"记第 5 轮）；[plan §S7A-6](../../stage_plans/active/stage-07/plan-a.md) 第 403-404 行与 [§3.1 S7A-6.2](../../stage_plans/active/stage-07/plan-a.md) |
| `ABI:271` | `ReplayDecodeBudget` | 域 9 Replay、Snapshot 与 Seek | `待冻结` | `Q-13`（第 5 轮） | S7A-6 | **接受并登记**。附加限定：字段集由 `Q-13` 冻结，预算数值继续阻塞 S7A-9 预算门禁 | 字段集与 `EventCodecId` 同源，依据 [RULING_WORKSHEET §5 第 139 行](RULING_WORKSHEET.md#L139)（Q-13："Snapshot/Replay schema"）；数值部分见 ABI [§未决项](../../api/GAMEPLAY_V2_ABI.md#L564-L586) #16（第 586 行，S7A-9 前保持未冻结）与 [§登记缺陷 第 528-529 行](../../api/GAMEPLAY_V2_ABI.md#L528-L529) 的预算类说明 |
| `ABI:275` | `SeekLatencyCommitment` | 域 9 Replay、Snapshot 与 Seek | `待冻结（数值待测量）` | `CM-K08`（第 5 轮，**本次登记新增**） | S7A-6 | **修改后接受**（原候选为"（无轮次表编号）`P1-10`（第 7 轮）只覆盖预算计数与最坏值入口"；最终新增 `CM-K08` 并由第 5 轮裁定）。附加限定：第 5 轮冻结类型、承诺语义、会话收紧关系与测量口径入口，数值限额仍登记为 S7A-9 未冻结预算项；在第 5 轮落地前 S7A-6 只保留类型名作追踪项，**不**进入 Snapshot / Replay 字段、运行时约束或对外承诺；提供具体 commitment 值的输入稳定拒绝，seek 本身仍按正确性与可重放契约执行 | [plan §S7A-6](../../stage_plans/active/stage-07/plan-a.md) 第 410-411 行"`maxSeekLatency` 是引擎承诺，会话只能收紧"；[RULING_WORKSHEET §7 第 172 行](RULING_WORKSHEET.md#L172)（P1-10：只登记计数与上界，不冻结限额）；ABI §未决项 #16（第 586 行） |

## 4. 分组小结（按最终绑定重算）

**按最终绑定的首次消费批次**（合计 **16**）：

- **S7A-2（2 条）**：`ABI:67` `TickSpan` / `TickDelta`、`ABI:86` `DomainAmount`。
- **S7A-4（5 条）**：`ABI:85` `ContactRef`、`ABI:110` `Measure`、`ABI:132` `ResourceLease`、
  `ABI:143` `CandidateId`、`ABI:144` `LocalOutcome`。
- **S7A-5（5 条）**：`ABI:182` `RegisterKind`、`ABI:186` `ScoreState`、`ABI:187` `ComboState`、
  `ABI:188` `LifeState`、`ABI:189` `StatisticsSnapshot`。
- **S7A-6（3 条）**：`ABI:270` `EventCodecId`、`ABI:271` `ReplayDecodeBudget`、`ABI:275` `SeekLatencyCommitment`。
- **S7A-7（1 条）**：`ABI:237` `CapabilityRecord`。
- **S7A-1（0 条）**：原 (b) 组的两条在裁决后分别落到 S7A-2 与 S7A-7，对 S7A-1 只剩"登记角色"
  （见 `ABI:237` 的附加限定）。因此本组不再有"S7A-1 退出前必须绑定"的条目。

**按最终裁决轮次**（合计 **16**）：第 2 轮 **2** 条（`ABI:67`、`ABI:86`）；第 4 轮 **3** 条（`ABI:85`、
`ABI:132`、`ABI:143`）；第 5 轮 **10** 条（`ABI:110`、`ABI:144`、`ABI:182`、`ABI:186`、`ABI:187`、
`ABI:188`、`ABI:189`、`ABI:270`、`ABI:271`、`ABI:275`）；第 6 轮 **1** 条（`ABI:237`）。

**按编号来源（最终）**（合计 **16**）：由既有轮次表编号直接承载 **11** 条（`ABI:67`、`ABI:85`、`ABI:110`、
`ABI:132`、`ABI:143`、`ABI:144`、`ABI:186`、`ABI:187`、`ABI:189`、`ABI:270`、`ABI:271`）；既有编号但此前没有
轮次表行、本次**绑定登记** **2** 条（`ABI:182` `CM-S02`、`ABI:237` `CM-X01`）；**本次登记新增编号** **3** 条
（`ABI:86` `CM-T13`、`ABI:188` `CM-S11`、`ABI:275` `CM-K08`）。

**原 (a)/(b)/(c) 口径已失效。** 提案期按"是否影响 S7A-1""是否需 owner 指定编号"分为 (b) 2 条、(c) 10 条、
(a) 4 条。Codex 裁决后：`CapabilityRecord` 的首次消费改登记为 S7A-7、`DomainAmount` 为 S7A-2，
`CM-T13`、`CM-S11`、`CM-K08` 三个新编号与 `CM-S02`、`CM-K01` 两处轮次表绑定登记补齐，故 (b)/(c) 的区分
不再适用；上列三段计数为现行口径（逐条依据见 §3 末列）。

## 5. 对其它 14 条的核实结果

30 条 `待冻结` 减去 §3 的 16 条，余下 14 条在 ABI 状态格内给出了裁决编号。逐条核实"编号 + 轮次 +
首次消费批次是否真的可追溯"：

| 条目 | 现称绑定 | 是否成立 | 说明 |
| --- | --- | --- | --- |
| `ABI:69` `FinalizationWatermark` | `CM-T08 / P1-04` | 成立 | [RULING_WORKSHEET §2](RULING_WORKSHEET.md#L106) 第 106 行合并行属第 2 轮（[§0.1](RULING_WORKSHEET.md#L34) 第 34 行同时关闭 `CM-T08`）；ABI §未决项 #6（第 576 行）阻塞 S7A-2 / S7A-4，首次消费批次 S7A-2 与第 2 轮门禁一致 |
| `ABI:108` `PhasePriority` | `CM-F06` | 成立（附跨条目时点风险） | [RULING_WORKSHEET §5](RULING_WORKSHEET.md#L141) 第 141 行 `CM-F06` 随 Q-12 属第 5 轮；ABI §未决项 #10（第 580 行）阻塞 S7A-5。风险见本节末 |
| `ABI:111` `Grade` | `CM-S10 / P1-07` | 成立（附跨条目时点风险） | [RULING_WORKSHEET §5 第 142 行](RULING_WORKSHEET.md#L142)、第 5 轮；ABI §未决项 #11（第 581 行）阻塞 S7A-5。风险见本节末 |
| `ABI:134` `ClaimPolicy` | `P2-06` | **部分成立** | 编号与轮次可追溯（[§6](RULING_WORKSHEET.md#L160) 第 160 行，第 6 轮 → 解锁 S7A-7），但 [plan §S7A-3](../../stage_plans/active/stage-07/plan-a.md) 第 314-319 行要求 prepare 阶段解析 claim / ownership / `preparedGrace`，据此推断 `ClaimPolicy` 的分层是 S7A-3 的输入：**轮次晚于首次消费批次** |
| `ABI:141` `SolverProfile` | `CM-C06` | **部分成立** | `CM-C06` 在 [RULING_WORKSHEET.md](RULING_WORKSHEET.md) 的第 2-7 轮表中没有行（包内只出现在 [CONTRACT_MATRIX.md 第 125 行](CONTRACT_MATRIX.md#L125) 的合同项与 [OPEN_QUESTIONS.md 第 29 行](OPEN_QUESTIONS.md#L29) 的 Q-03"涉及合同项"列；ABI 自身在第 141、433、577 行引用它，但同样不给轮次表归属）。ABI §登记缺陷 第 512-514 行自述其轮次借自 OPEN_QUESTIONS 的 Q-03 行（第 4 轮），§仍待冻结 第 433 行亦写"第 4 轮"；首次消费批次 S7A-4（§未决项 #7，第 577 行）与之一致。属**借道绑定**，需 owner 确认 `CM-C06` 随 Q-03 一并裁定 |
| `ABI:162` `FactCategory` | `CM-S10` | 成立（附跨条目时点风险） | [RULING_WORKSHEET §5 第 142 行](RULING_WORKSHEET.md#L142)（category 由 phase 派生）；ABI §未决项 #11（第 581 行）阻塞 S7A-5。风险见本节末 |
| `ABI:163` `Outcome` | `CM-S10` | 成立（附跨条目时点风险） | [RULING_WORKSHEET §5 第 142 行](RULING_WORKSHEET.md#L142)（7A 最小 `outcome {hit, miss}`）；ABI §未决项 #11（第 581 行）阻塞 S7A-5。风险见本节末 |
| `ABI:164` `ReasonCode` | `P2-04` | 成立 | [§6](RULING_WORKSHEET.md#L158) 第 158 行逐字点名 `reasonCode`，第 6 轮 → 解锁 S7A-7；与 ABI §未决项 #13（第 583 行，`CM-D04` 阻塞 S7A-7）同批 |
| `ABI:216` `IdentityProjection` | `CM-I06 / Q-10` | 成立（已裁定） | 第 1 轮已接受（[§1](RULING_WORKSHEET.md#L84-L86) 第 84-86 行、[§9](RULING_WORKSHEET.md#L261) 第 261 行）；ABI §未决项 #1（第 571 行）阻塞 S7A-1，首次消费批次 S7A-1；ABI §S7A-1 第 503-505 行要求 #1-#4 在本批次内给出处置 |
| `ABI:217` `InterchangeCompatibility` | `Q-01 Codex 修订` | 成立（已裁定） | 第 1 轮已接受（[§1](RULING_WORKSHEET.md#L84) 第 84 行、[§0.2](RULING_WORKSHEET.md#L61-L63) 第 61-63 行）；ABI §未决项 #2（第 572 行）阻塞 S7A-1 |
| `ABI:245` `DiagnosticCode` | `CM-D04` | 成立 | [§6](RULING_WORKSHEET.md#L156) 第 156 行（`CM-D04 / P1-09`）属第 6 轮 → 解锁 S7A-7；ABI §未决项 #13（第 583 行）阻塞 S7A-7。对照 [BATCH_GATES §S7A-1](BATCH_GATES.md#L45) 第 45 行与 [§S7A-7](BATCH_GATES.md#L117) 第 117 行的诊断类别 |
| `ABI:246` `DiagnosticCategory` | `CM-D04` | 成立 | 同上（`CM-D04 / P1-09`，[§6 第 156 行](RULING_WORKSHEET.md#L156)，第 6 轮） |
| `ABI:247` `DiagnosticSeverity` | `CM-D04` | 成立 | 同上（`CM-D04 / P1-09`，[§6 第 156 行](RULING_WORKSHEET.md#L156)，第 6 轮） |
| `ABI:272` `SnapshotHeader` | `CM-K07 / Q-13` | 成立 | [§5](RULING_WORKSHEET.md#L139-L140) 第 139-140 行合并行属第 5 轮（[§0.1](RULING_WORKSHEET.md#L37) 第 37 行同时关闭 `CM-K07`）；ABI §未决项 #12（第 582 行）阻塞 S7A-6 |

**核实结论**：14 条中 **12 条成立**（其中 4 条附带下述时点风险），**2 条部分成立**（`ClaimPolicy`、
`SolverProfile`），**无不成立**。

**跨条目时点风险（不计入上表判定，但需 owner 裁定）**：第 5 轮的 `CM-S10 / P1-07`、`CM-F06 / Q-12`
（及其 ABI §未决项 #10 / #11 第 580-581 行的 S7A-5 登记）与 [plan §S7A-4](../../stage_plans/active/stage-07/plan-a.md)
第 335-343 行的六步 Tick 顺序（第 5 步按 `phase` 排序）、[§3.1 S7A-4.2 / S7A-4.4](../../stage_plans/active/stage-07/plan-a.md)
第 514-516 行与 [BATCH_GATES §S7A-4](BATCH_GATES.md#L79) 第 79 行（含重复输入、Miss 的 Fact golden）
之间存在重叠：S7A-4 的产物需要 phase 排序与 hit/miss Fact，而这两组表示被登记在第 5 轮。这不改变
上述 4 条的"编号 + 轮次 + 批次"可追溯性判定（它们各自与其 `阻塞` 列登记自洽），但 owner 在裁定
第 4、5 轮时需要明确 S7A-4 可消费的最小表示，或按 S1-05 把受影响字段移出 S7A-4。

**2026-10-03 裁决后的状态。** 上述跨条目时点风险已由 Codex 卡片的跨条目边界条款收敛：S7A-4 只产生并消费
`phase`、phase-ordering token、单分量量化 tick error、`hit` / `miss` 及 Hold 的最小 phase-local outcome；
`phasePriority` 仅作确定性事实序排序键（不是评分结果）；`category`、`grade`、Score / Combo / Life / Statistics
统一由 S7A-5 消费和冻结（同文见 [GAMEPLAY_V2_ABI.md](../../api/GAMEPLAY_V2_ABI.md) §登记缺陷 与
[RULING_WORKSHEET.md](RULING_WORKSHEET.md) §7B）。本节 14 条的判定不变（12 条成立、2 条部分成立）。

## 6. Codex 裁定栏（2026-10-03）

裁定四选一：**接受并登记** / **修改后接受**（写明改动） / **拒绝并给替代** / **登记为阻塞项**。
协议行见 [OPEN_QUESTIONS.md §6](OPEN_QUESTIONS.md#L138-L139) 第 138-139 行与
[RULING_WORKSHEET.md §0](RULING_WORKSHEET.md#L25-L26) 第 25-26 行：**"以后再定"不是可接受的裁定**；
登记为阻塞项时，对应批次不得启动。

**provenance。** owner 把本组 16 条的裁定委派给 Codex 咨询；Codex 于 **2026-10-03** 裁决：thread
`s7a1-freeze-bindings`，verdict `adopt`，confidence 0.96。下表为该裁定的逐条结果（同内容也记在 §3 的
"Codex 裁定与附加限定"列）。

| 条目 | 裁定（Codex，2026-10-03） |
| --- | --- |
| `ABI:67` `TickSpan` / `TickDelta` | 接受并登记（附加限定：随 `CM-T03` 的 Tick 宽度与 tie rule 一并裁定） |
| `ABI:85` `ContactRef` | 修改后接受（限定 S7A-4 只消费最小 contact handle，不消费 handoff / `sameContact`） |
| `ABI:86` `DomainAmount` | 修改后接受（新增 `CM-T13`，第 2 轮，本次登记新增） |
| `ABI:110` `Measure` | 修改后接受（限定 S7A-4 只消费单分量量化结果与有符号 tick error；grade / 多分量聚合留 S7A-5） |
| `ABI:132` `ResourceLease` | 接受并登记（附加限定：`capacity > 1`、handoff 保持稳定拒绝） |
| `ABI:143` `CandidateId` | 修改后接受（按 `Q-16` / `CM-C05` 绑定；限定只承担确定性候选标识） |
| `ABI:144` `LocalOutcome` | 修改后接受（由 `P2-04`（第 6 轮）改为 `CM-S10` / `P1-07`（第 5 轮）；限定 S7A-4 只消费 Hold 最小 `localOutcome`） |
| `ABI:182` `RegisterKind` | 修改后接受（`CM-S02` 绑定登记到第 5 轮） |
| `ABI:186` `ScoreState` | 接受并登记 |
| `ABI:187` `ComboState` | 接受并登记 |
| `ABI:188` `LifeState` | 修改后接受（新增 `CM-S11`，第 5 轮，本次登记新增；7A 关闭 Life） |
| `ABI:189` `StatisticsSnapshot` | 接受并登记 |
| `ABI:237` `CapabilityRecord` | 修改后接受（`CM-X01` 绑定到第 6 轮，与 `Q-15` / `Q-19` 同批；首次消费 S7A-7；D-14 按方案 (a) 处置） |
| `ABI:270` `EventCodecId` | 修改后接受（`CM-K01` 绑定到第 5 轮，与 `Q-13` 同批） |
| `ABI:271` `ReplayDecodeBudget` | 接受并登记（字段集随 `Q-13`；数值留 S7A-9） |
| `ABI:275` `SeekLatencyCommitment` | 修改后接受（新增 `CM-K08`，第 5 轮，本次登记新增；数值留 S7A-9） |

裁定已落地：ABI 的 16 条状态格与 §未决项 / §登记缺陷 已按此更新，
[RULING_WORKSHEET.md](RULING_WORKSHEET.md) §0.1 / §2 / §5 / §6 / §7 / §7A / §7B / §8 的计数与登记已同步，
[CONTRACT_MATRIX.md](CONTRACT_MATRIX.md) 的 `CM-X01` 与 [OPEN_QUESTIONS.md](OPEN_QUESTIONS.md) 的 D-14 行
已按方案 (a) 修正。

## 7. 风险与停止条件

**Codex 卡列出的风险（2026-10-03）。**

1. **`D-14` 的台账差异。** `CM-X01` 在合同台账记 `accept`、在 ABI 记 `待冻结`。本次已按方案 (a) 修正台账
   （`accept` → `open`）；但若 capability registry 字段集不在第 6 轮门禁内裁定，S7A-7 的 capability
   registry 门禁仍无字段基准。
2. **`ABI:275` 的拆分登记。** `SeekLatencyCommitment` 的类型 / 承诺语义属第 5 轮，数值限额属 S7A-9。
   把两者合成一次冻结会把未测量的数值写成承诺；本次按拆分登记，S7A-9 前数值保持未冻结。
3. **S7A-4 不得消费完整 `grade` / `category`。** S7A-4 只产生并消费 `phase`、phase-ordering token、
   单分量量化 tick error、`hit` / `miss` 及 Hold 的最小 phase-local outcome；`phasePriority` 仅作确定性
   事实序排序键，**不是**评分结果；完整 `category`、`grade` 与 Score / Combo / Life / Statistics 统一由
   S7A-5 消费和冻结。

**新的次序约束（同卡）。**

4. **`CM-X01` 必须在第 6 轮门禁内裁定**，否则 S7A-7 违规启动（capability registry 字段集没有已冻结依据）。
5. **`CM-K08` 必须在第 5 轮门禁内裁定**，否则 S7A-6 会伪实现 commitment 值（把未测量的 `maxSeekLatency`
   写进运行时约束或对外承诺）。

**仍适用的判据、批次与停止条件。**

6. **会被违反的判据。** 若 16 条在对应轮次门禁前仍未被裁定，将同时违反：
   - S1-03 的登记要求（[RULING_WORKSHEET.md §7A](RULING_WORKSHEET.md#L202) 第 202 行；[plan.md §S7A-1](../../stage_plans/active/stage-07/plan-a.md) 第 229-232 行）；
   - S7A-1 的核对方式"**按域计数、无遗漏**"（plan 第 232 行）与 ABI [§126 条追踪规则](../../api/GAMEPLAY_V2_ABI.md#L497-L500) 第 497-500 行的判据；
   - S7A-0 的退出门禁"所有未决语义均有 owner 决策或阻塞项；无'实施时再决定'的公共字段"
     （[plan §S7A-0](../../stage_plans/active/stage-07/plan-a.md) 第 212-214 行），
     以及 [BATCH_GATES §S7A-0](BATCH_GATES.md#L37) 第 37 行的停止条件。
7. **受影响的批次（按最终绑定）。** S7A-2（`TickSpan`/`TickDelta`、`DomainAmount`）；
   S7A-4（`ContactRef`、`Measure`、`ResourceLease`、`CandidateId`、`LocalOutcome`）；
   S7A-5（`RegisterKind`、`ScoreState`、`ComboState`、`LifeState`、`StatisticsSnapshot`）；
   S7A-6（`EventCodecId`、`ReplayDecodeBudget`、`SeekLatencyCommitment`）；S7A-7（`CapabilityRecord`）。
   其中 `CapabilityRecord`（`ABI:237`）"合同台账记 `accept`、ABI 第 237 行记 `待冻结（CM-X01）`"的台账差异
   已登记为缺陷 **`D-14`**，并于 2026-10-03 按方案 (a) 处置（[OPEN_QUESTIONS.md](OPEN_QUESTIONS.md) §5 的 D-14 行）。
8. **门禁覆盖缺口。** 现有批次停止条件只覆盖了部分相关编号：
   [BATCH_GATES §S7A-2](BATCH_GATES.md#L61) 第 61 行（`CM-T03`/`CM-T04`/`CM-T08`）、
   [§S7A-4](BATCH_GATES.md#L85) 第 85 行（`CM-C05`/`CM-C09`）、
   [§S7A-5](BATCH_GATES.md#L97) 第 97 行（`CM-S09`/`CM-S10`）、
   [§S7A-6](BATCH_GATES.md#L109) 第 109 行（`CM-K07`/`CM-P04`）。
   `DomainAmount`（`CM-T13`）、`RegisterKind`（`CM-S02`）、`LifeState`（`CM-S11`）、`CM-K01` 一族、
   `SeekLatencyCommitment`（`CM-K08`）与 `CapabilityRecord`（`CM-X01`）目前没有对应的停止条件入口；
   §5 的 `ClaimPolicy`（第 6 轮 → S7A-7 门禁，但首次消费在 S7A-3）同样没有。
   若只补绑定而不补停止条件，会出现"已登记但门禁不拦"。
9. **停止条件。** 命中上述判据时，按 [BATCH_GATES §0](BATCH_GATES.md#L21-L22) 第 21-22 行立即停止
   受影响批次、保留最小复现、回滚到最后接受的合同 revision，**不得**用默认值、临时 typedef、
   序列化编码或"伪成功"实现绕过（[plan §S7A-1](../../stage_plans/active/stage-07/plan-a.md)
   第 245-249 行；[RULING_WORKSHEET §7A S1-05](RULING_WORKSHEET.md#L204) 第 204 行）。

## 8. 复核基线与方法

- 复核基线：工作区（HEAD `449e864` + 未提交工作区改动），文档版本日期 2026-10-02/2026-10-03；
  本次绑定登记的裁决日期 2026-10-03（thread `s7a1-freeze-bindings`，verdict `adopt`，confidence 0.96）。
- ABI 条目行号：以"状态格首词为 `待冻结`"筛选，得 30 行；再按"状态格内是否给出裁决编号"分组，
  得 §3 的 16 条与 §5 的 14 条。该分组与 ABI §登记缺陷 的历史自述逐条一致
  （13 条 (a) 类行号 67/85/86/110/132/143/144/186/187/188/189/271/275、3 条 (b) 类行号
  182/237/270 完全对应；**未发现行号偏移**，`SolverProfile` 位于第 141 行）。本次只在 16 条的状态格内
  追加绑定括注、未增删 §类型清单 的行，故上述行号在绑定后仍成立。
- 域计数：§3 的 16 条分布为域 1 一条、域 2 两条、域 3 一条、域 4 三条、域 6 五条、域 8 一条、
  域 9 三条；与 ABI [§域计数表](../../api/GAMEPLAY_V2_ABI.md#L484-L495) 第 484-495 行的 30 条
  `待冻结` 分域计数相容。
- 轮次编号来源：[RULING_WORKSHEET.md](RULING_WORKSHEET.md) 第 2-7 轮表、[§0.1 轮次总览](RULING_WORKSHEET.md#L29-L47)、
  [§7B 绑定登记](RULING_WORKSHEET.md)，以及 [OPEN_QUESTIONS.md](OPEN_QUESTIONS.md) 的对应 Q/P 行。
  注意：§3 / §5 中的行号锚点（`#Lxxx`）以提案登记时（2026-10-03 前）的行号为准；本次在 §2 / §5 / §6
  增删行后部分锚点已偏移，定位请以编号与原文片段为准。
- 本文件自身的门禁：`python -B tools/check_docs.py` 与 `git diff --check` 必须 exit 0。

## 9. 相关索引

- [Gameplay V2 acceptance package](README.md)：本登记所属准入库及其索引
- [Gameplay V2 ABI](../../api/GAMEPLAY_V2_ABI.md)：被处置的 30 条 `待冻结` 与 §登记缺陷 的所在文档
- [未决语义分轮裁决清单](RULING_WORKSHEET.md)：§7B 绑定登记、S1-01…S1-05 与第 1-7 轮裁定范围
- [未决问题与 owner 决策请求](OPEN_QUESTIONS.md)：裁定协议与逐条 Q/P 编号
- [批次证据门禁表](BATCH_GATES.md)：各批次正例/负例/诊断/golden/停止条件
- [目标、依赖与安装图](TARGETS_AND_DEPENDENCIES.md)：S1-04 的模块与 target 决策登记
- [S7A-0.2 合同逐项台账](CONTRACT_MATRIX.md)：`CM-xxx` 合同项的处置词与未决点
- [Stage 7A 实施计划](../../stage_plans/active/stage-07/plan.md)：批次范围与退出判据

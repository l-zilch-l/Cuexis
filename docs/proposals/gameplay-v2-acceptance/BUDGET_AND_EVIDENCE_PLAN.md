# Gameplay V2 预算与证据计划（S7A-0.3）

状态：candidate；S7A-0 准入产出，待 owner acceptance

更新日期：2026-10-02

上级文档：[Gameplay V2 acceptance package](README.md) ·
[Stage 7A 实施计划](../../stage_plans/active/stage-07/plan.md)

文档角色：预算与测量计划（planning artifact）。它定义计数口径、测量输入、硬/软属性、
超限动作和预期证据文件；它不是生产限额、不是 ABI 常量，也不是实施声明。

## 0. 为什么单独写这一份

[Stage 7A 计划](../../stage_plans/active/stage-07/plan.md) 的 S7A-0.3 要求为内容、稳态、
快照/Seek、Replay、Packed 五类预算建立 profile 草案，并在 S7A-0 输出中区分阈值与实测。
现有仓库里已经存在一组**可被误用**的数字：`docs/stage_reports/reviews/gameplay-ruleset-2026-10/2026-10-01-fold-spike.md`
记录了 96 条 requirement、672 个输入事件、activity peak 3、Gap timer peak 2、4,777 字节快照。
计划第 188 行、ADR 0043 第 99-100 行和
[Chart v5 Alignment Review](../research/gameplay-v2/CHART_V5_ALIGNMENT_REVIEW.md) 第 455 行都明确
禁止把这类研究切片值写成 ABI 常量。本计划的作用是：**把数字和口径分开登记，并写清它们在
V2 语义下还不能作为限制的原因。**

## 1. 约束

1. 本计划产出的任何数值默认属性是「待测量」或「研究切片值」。只有 [Packed 候选物理合同 §3.3](../../formats/PACKED_CHART_FORMAT.md)
   已冻结的表是生产硬预算；Gameplay 运行时预算在 Stage 7A 关闭前保持未冻结。
2. 不得事后缩小 fixture 或把隐藏事件排除计数来"通过"预算（计划 §9.3）。
3. 只允许调整已被 owner 接受的 profile；未接受的 profile 只能作为候选。
4. 预算失败必须区分五类，不能用一个 `budget_exceeded` 覆盖全部（见 §7）。
5. 研究 spike 的实现模型与 V2 不同（见 §8.3），因此它的实测值只能作为**量级参考**，
   不能作为 V2 的基线数字。

## 2. 五类 profile 总表

| Profile | 计数对象 | 什么时候计 | 默认属性 | 超限动作 | 预期证据文件 |
| --- | --- | --- | --- | --- | --- |
| 内容 | 编译后图规模（requirement / relation / resource / emission / Pattern 状态 / factBinding） | prepare（或离线 assembler） | 硬门禁（待冻结阈值） | prepare 原子拒绝，保留旧 active session | `s7a-3-*.json`、`s7a-7-*.json` |
| 稳态 | 每 Tick 活动度、候选重算、solver fuel、Fact 数、reducer 调用、signal 队列 | 每个 Tick | 硬门禁（待冻结阈值） | 进入 `faulted`，不提交半个 Tick | `s7a-4-*.json`、`s7a-5-*.json` |
| 快照 / Seek | 快照字节、快照数、恢复耗时、Seek 延迟 | snapshot / seek / replay | 软目标 + 引擎承诺（`maxSeekLatency`） | 记录回归；引擎承诺不得事后放宽，会话只能收紧 | `s7a-6-*.json` |
| Replay / 解码 | 事件数、字节数、解码耗时、错误类别 | 编码与解码 | 硬门禁（Replay 头部自描述） | 稳定 `replay_format_error` 类诊断 | `s7a-6-replay-*.json` |
| Packed wire | 文件 / section / 实体 / 要求 / 字符串 / 引用计数 | Writer envelope、Reader 预检与复算 | 已冻结（Foundation R3） | 稳定 `packed.budget.*` 诊断 | 既有 R3/R4/R5 容量数据 |

## 3. 内容 profile（prepare-time）

### 3.1 计数口径

| 计数项 | 口径 | 检查时点 | 诊断码（候选） |
| --- | --- | --- | --- |
| requirement 数 | Canonical Graph 中 `requirements[]` 的最终条数（含 CXT 展开结果） | prepare / assembler | `budget.content.requirements` |
| relation 数 | `exclusive` / `binding` / `temporal` / `quota` 四类边的条数，逐类与合计分别计 | prepare | `budget.content.relations` |
| resource 数 | 资源声明条数（Stage 7A 只有 `capacity=1` exclusive 子集） | prepare | `budget.content.resources` |
| group 成员数 | 单个 group 的最大成员数与其展开后的实例数 | prepare | `budget.content.group_cardinality` |
| emission 数 | CXT v2 invocation 的展开 emission 总数（显式 Chart v5 inline 记为 1） | assembler | `budget.content.emissions` |
| Pattern 确定化状态数 | **确定化并最小化之后**的单条 Pattern 状态数，不用 NFA 规模 | prepare | `budget.content.pattern_states` |
| 补算子操作数 | 单个 complement 的 DFA 操作数上界 | prepare | `budget.content.complement_operands` |
| boundedRelationInstance | 单个有界实例的 `count` 与实例身份总数 | prepare | `budget.content.bounded_instances` |
| solver 候选上界 | 单个 coordination window 内可产生的候选数静态上界 | prepare | `budget.content.candidates` |
| solver fuel | `solverProfile.maxFuel` / `maxBranches` 的声明值与静态上界 | prepare | `budget.content.solver_fuel` |
| factBinding 数 | `factBindings[]` 条数与单个 binding 的 `requirementRefs` / `targetRefs` 基数 | prepare | `budget.content.fact_bindings` |
| sourceMap 条目 | 仅诊断用途，单独计，不进入 judgement identity | prepare | `budget.content.source_map` |

### 3.2 属性与动作

- 上表除 sourceMap 外全部是**硬门禁候选**：任何一项超过已接受阈值时 prepare 必须原子失败，
  诊断必须指出字段路径、requirement/relation 身份和实际计数。
- 未冻结阈值前，实现必须至少提供**计数与报告能力**，并把实测值写入证据文件；
  未接受阈值时不得用 `0` 或"不限制"表示跳过（`0` 必须保持字面上限语义，
  与 Packed 预算表的零值策略一致）。
- Pattern 状态预算必须按确定化后的状态数检查；按 NFA 规模检查会误伤宽窗口条目
  （fold spike §4.7 的次要发现）。

## 4. 稳态 profile（每 Tick）

| 计数项 | 口径 | 诊断码（候选） |
| --- | --- | --- |
| `activityPeak` | 同一 Tick 内同时处于活动状态的 Requirement 实例数峰值 | `budget.steady.activity` |
| 候选重算数 | 每个规范化输入事件重建的 Candidate 数（不是每次 Tick 的总和） | `budget.steady.candidates` |
| coordination 求解分支 | 单次 Coordination window 的实际分支数与 fuel 消耗 | `budget.steady.solver_fuel` |
| Fact / Tick | 单个 Tick 追加到 Fact Ledger 的 FactRecord 数 | `budget.steady.facts` |
| reducer 调用数 | 单个 Tick 内 Ruleset reducer 调用次数与读写寄存器数 | `budget.steady.reducers` |
| signal queue 深度 | 待下一 Tick 投递的 signal 数 | `budget.steady.signals` |
| timer 数 | 单个 Tick 到期的 timer 数与存活 timer 总数 | `budget.steady.timers` |
| late queue | `queue_next_tick` 的待转发事件数与最大 hop | `budget.steady.late_queue` |

属性：硬门禁候选。任一超出已接受上界时，Ruleset state transaction 不提交，session 进入
可查询的 `faulted` 状态；已提交 Fact 保留，PresentationEvent 可由 Fact Ledger 重建
（计划 §S7A-5.5）。

## 5. 快照与 Seek profile

| 计数项 | 口径 | 属性 |
| --- | --- | --- |
| 单快照字节 | 一次全量 snapshot 的规范字节数，分静态部分与实例部分 | 软目标（记录回归） |
| 快照数 / 间隔 | 快照间隔是引擎内部参数，不进入 identity | 引擎内部 |
| 恢复耗时 | 从快照恢复到可继续 submit 的时间 | 软目标 |
| Seek p95 / max | 从最近快照推进到目标时间的耗时 | `maxSeekLatency` 是引擎承诺，会话只能收紧 |
| 无损性 | 恢复后尾部输入 Fact/Score/Combo/Statistics 与连续播放逐位一致 | **硬门禁（正确性，不是性能）** |

Seek 和快照的正确性门禁与性能门禁必须分开记录：性能可以记为环境限制，语义无损不可以。

## 6. Replay 与解码 profile

| 计数项 | 口径 | 属性 |
| --- | --- | --- |
| 事件数 | 规范化 Observation 流的事件条数 | 硬（Replay 头部自描述并在解码前预检） |
| 字节数 | Replay 载荷字节数 | 硬（同上） |
| 解码耗时 | 解码 + 校验耗时 | 软目标（记录，不阻塞语义） |
| 错误类别 | 截断、乱序、重复、未知版本、identity mismatch、超事件数/字节数、未知 capability | 硬（逐一稳定失败） |

Replay 不得因为解码预算而静默丢弃事件；超预算必须整体拒绝（计划 §S7A-6.3）。

## 7. Packed wire profile（已冻结部分）

以下数值来自 [Packed 候选物理合同 §3.3](../../formats/PACKED_CHART_FORMAT.md)，由 Chart Format
Foundation R3 冻结并已在 R4/R5 复跑。本计划**不改动、不放宽**它们，只登记 V2 需要对齐的部分。

| 预算 | 冻结默认值 | 诊断 |
| --- | --- | --- |
| `maxPackedFileBytes` | 16,777,216 | `packed.budget.file_bytes` |
| `maxPackedDecodedBytes` | 16,777,216 | `packed.budget.decoded_bytes` |
| `maxPackedSectionBytes` | 16,777,216 | `packed.budget.section_bytes` |
| `maxPackedEntities` | 40,000 | `packed.budget.entities` |
| `maxPackedRequirements` | 40,000 | `packed.budget.requirements` |
| `maxPackedStrings` | 100,000 | `packed.budget.strings` |
| `maxPackedReferences` | 100,000 | `packed.budget.references` |

V2 相关的对齐要求：Packed 必须无损承载 canonical requirements、资源/group/relation 表、
prepared grace、solver profile 与 capability revision；新增的 gameplay semantic section 必须
按同一套逐 section 预算与 `referenceCount` 口径计数，不得为 V2 新开一个绕过 envelope 预检的入口。
`preparePeakBytes` 仍是观测项，未接受阈值，也不得用来放宽上表（同 §3.3 末段）。

## 8. 现有研究证据（登记，不作限额）

### 8.1 已完成的测量

来源：[Fold Spike 报告](../../stage_reports/reviews/gameplay-ruleset-2026-10/2026-10-01-fold-spike.md)、
[预算与规模上界草案](../research/gameplay/GAMEPLAY_BUDGET_AND_SCALE_DRAFT.md)。

| 测量 | 环境 | 值 |
| --- | --- | --- |
| 代表性研究切片 | GCC 16.1.0 `-O2` / clang 22.1.8 `-O2` | 96 requirement、672 输入事件、activity peak 3、Gap timer peak 2、单快照 4,777 字节、Seek p95 0.014 / 0.015 ms |
| 五档合成 profile | 同上 | requirement 3,000–40,000；单 Tick 最大运算 17–94；运算/s 最大 14,021；采样/s 最大 3,324；单次运行 5–30 ms |
| 快照间隔扫描（1/5/15/60 s） | 同上 | 单快照最大 1,289 字节；Seek p95 0.24–5.78 ms，max 6.22 ms；无损 = 是 |
| Pattern 规模 | 同上 | 标准条目最小 DFA 8–122 状态；对抗性 `¬(any* · press · any{n})` 在 n=10 处超过 4,096 预算被拒绝 |
| 建议候选值（**非冻结**） | 同上 | `activityPeak` 1,024；`patternStateBudget` 4,096；`maxSeekLatency` 50 ms；`sampleCostPerSecond` 65,536 |

### 8.2 禁止用法

`96`、`672`、`3`、`2`、`4,777`、`1,289`、`0.014`、`1,024`、`4,096`、`50 ms`、`65,536`
以及 §8.1 的其它数值**都不是生产限额**。它们不得出现在公共头、Schema、ABI、默认
`PackedChartLimits`、capability 声明或 Replay 头部预算中，也不得被改写成"已验证上限"。
`eventBudget` 在 spike 中明确没有建议值（运算计数单位尚未与真实 CPU 代价标定）。

### 8.3 与 V2 的差距（决定这些数字不能直接复用）

| 差距 | 说明 |
| --- | --- |
| Tick 阶段模型 | spike 使用 Gameplay I 的四阶段顺序（激活 / 定时器 / 输入 / 折叠）；V2 计划 §S7A-4 固定六步 Tick 顺序，Preliminary Design §5.3 又给出八步 Coordination window 阶段 |
| 无 Candidate / CoordinationCommit | spike 直接产生 Fact；V2 需要候选收集、排序、可行集合求解和一次提交 |
| 无因果总序 FactRecord | spike 使用 `(requirementId, phase, outcome)` 排序 |
| 无 Ruleset State Transaction | spike 只有一个"每 100 连击放宽 10%"的限时模块；V2 有读写集、reducer 类型与整 Tick 原子提交 |
| 无 TimebaseProfile / finalizationWatermark | spike 时间域是单一 Tick，无迟到策略、无 `queue_next_tick` 队列上界 |
| grace 语义不同 | spike 的 grace 是 Gameplay I 的 `handoff + Gap`；V2 把它拆成 7A `preparedGrace` 与后续 `continuityGrace` |
| 平台覆盖 | 只有 Windows x86-64 的两个编译器；跨 OS/架构一致未验证（spike 自述局限第 4 条） |

结论：S7A-3 至 S7A-6 必须**重新测量**V2 语义下的计数项；§8.1 只用于量级对照与"是否远离边界"的判断。

## 9. 测量计划

| 批次 | 测量输入 | 执行入口（候选） | 预期证据文件 | 属性 |
| --- | --- | --- | --- | --- |
| S7A-3 | typed file/memory source、CXT candidate 正负例、置换输入 | `ctest --preset debug -R cuexis_judgement_*` + `cuexis_gameplay_capacity` 工具 | `docs/stage_reports/stages/stage-07/*-s7a-3-capacity.json` | 硬门禁 + 峰值 |
| S7A-4 | Tap/Hold/Release/tail 边界矩阵、单资源冲突、同 Tick 置换 | 同上 | `*-s7a-4-steady.json` | 硬门禁 |
| S7A-5 | 同 Tick 多 Fact、模块顺序、长局累计、fault 事务 | 同上 | `*-s7a-5-fold.json` | 硬门禁 |
| S7A-6 | Replay 往返、任意 seek 点、多快照间隔、篡改/截断 | 同上 | `*-s7a-6-snapshot-seek.json` | 软目标 + 无损硬门禁 |
| S7A-7 | v5 Core/Packed/Graph 双入口、headless/Player 生命周期 | Playback + headless consumer | `*-s7a-7-integration.json` | 硬门禁 |
| S7A-9 | 代表性真实内容 + 最坏内容 | 最终 SHA 全矩阵 | `*-s7a-9-budget.json`、`*-s7a-9-regression.md` | 阈值与实测分开 |

规则：

1. 每个证据文件必须记录 fixture 身份、命令、SHA、配置、环境与计数口径版本；
2. 实测与阈值分列，容量变化必须能追溯到 fixture 或 commit（计划 §S7A-9.3）；
3. GPU、真实设备、音频和网络输入无法在当前环境复现时，单列为"未执行/环境限制"，
   不得推断通过（计划 §8 证据要求）。

## 10. 待 owner 决策

1. 是否接受"Gameplay 运行时预算在 S7A-9 之前保持未冻结、只提供计数与报告"这一处置；
2. 五类 profile 的阈值来源：由 S7A-9 实测 + 留量倍数给出，还是先接受一组临时上限；
3. `maxSeekLatency` 的引擎承诺值是否沿用 50 ms 候选，还是留到 S7A-6 实测后再定；
4. `eventBudget` 的计数单位（运算计数 ↔ 真实 CPU 代价换算）是否作为 S7A-4 的独立测量项。

# 历史未决问题与决策登记

状态：decision register；第 1--7 轮决策已闭合，无待裁定轮次；实现验收仍按各批次计划进行

更新日期：2026-10-03（第 2 轮裁定登记见 §2.1、第 3 轮裁定登记见 §3.1、第 4 轮裁定登记见 §4.1、第 5 轮裁定登记见 §4.2、第 2 轮输入半批复核登记见 §4.3、第 6 轮裁定登记见 §4.4；§1–§5 的证据列保留 2026-10-02 原文）

上级文档：[Gameplay V2 acceptance package](README.md) ·
[Stage 7A 实施计划](../../stage_plans/active/stage-07/plan.md)

文档角色：历史未决清单与决策登记。计划的 S7A-0 退出门禁要求"所有未决语义均有 owner 决策或阻塞项；
无'实施时再决定'的公共字段"。本文件保留各轮未决点、候选处置与"不决定的后果"的历史记录；
第 1--7 轮已登记裁定，已裁定事项不得重新解释为待 owner 决策。

## 0. 口径

- 分级沿用 [Chart v5 Alignment Review](../research/gameplay-v2/CHART_V5_ALIGNMENT_REVIEW.md)：
  P0 直接阻塞实现；P1 不阻塞最小 Tap/Hold 内核但会让 7B+/Studio/包工具重复改写同一字段；
  P2 会让实现者与内容工具作出不同解释。
- 每条给出：涉及合同项（[CONTRACT_MATRIX.md](CONTRACT_MATRIX.md) 的 ID）、候选处置、
  必须决定的批次、若不决定的后果。
- **任何 P0 未裁定前，不得进入 S7A-1 的公共头或实现批次**（计划 §1.1）。

## 1. P0：直接阻塞（15 条，来自 Alignment Review）

| 编号 | 未决点 | 涉及合同项 | 候选处置（本 package 的建议） | 必须决定的批次 |
| --- | --- | --- | --- | --- |
| Q-01 | 版本层级未闭合：`version = 5` 是否只接受 `gameplay.version = 2`；旧 `1` 的处置；Packed `semanticVersion`/`candidateRevision` 是否含 gameplay revision；`candidateRevision` 是否改变 `compiledSemanticIdentity` | CM-V02/V03/V04/V07 | 接受"外层 5 + gameplay 2"唯一组合；旧 v1 仅显式离线迁移或最早入口拒绝；Packed header 显式带 gameplay revision；`candidateRevision` **不**改变 `compiledSemanticIdentity`（只改 artifact identity） | S7A-1 |
| Q-02 | Canonical Gameplay Graph 没有物理归属 | CM-V06 | 采用"Chart v5 JSON 区段为语义视图 + CXC `gameplay-graph` 为可选物理 entry"，并要求两者恢复同一 typed graph | S7A-1 / S7A-3（**已裁定（第 1 轮，Codex 2026-10-02）**；第 7 轮 2026-10-03 按第 1 轮裁定把 `CM-V06` 的处置词补齐为 `accept`，见 §4.5 的"五条 `CM-V*` 处置词补齐"段。上列"候选处置"列为 2026-10-02 的原始建议文本，保留不改） |
| Q-03 | compile-time solver 与 runtime coordinator 边界矛盾（"Playback 不执行 solver" vs `greedy_v1` 是 7A 能力） | CM-L10、CM-C06 | 拆成 compile/prepare solver（展开、验证、算上界、证明唯一性、生成 prepared profile）与 runtime coordinator（只按 prepared profile 做确定性排序与资源检查）；若 `greedy_v1` 仍需逐候选决策，改名为 `coordinator.policy.greedy_v1` | S7A-4（**已裁定（第 4 轮，Codex 2026-10-03，`S7A4-R01`）**：见 §4.1 裁定登记；上列"候选处置"列为 2026-10-02 的原始建议文本，保留不改） |
| Q-04 | Beat、TimingMap 与 Tick 无规范映射（七问：舍入/负时间/同 Tick 碰撞/溢出/tempo-stop 影响/时间域语义/pause-seek-reload/commitTick 粒度） | CM-T02/T03/T04 | 接受 `TimebaseProfile` 绑定与"作者 RationalBeat → prepare 冻结整数 judgementTick"；`chartBeat`/`judgementTick`/`observationTick` 三者分开；TimebaseProfile 与 offset/calibration/late policy 进 judgement identity | S7A-2（**已裁定（第 2 轮，Codex 2026-10-03）**：见 §2.1 裁定登记；上列"候选处置"列为 2026-10-02 的原始建议文本，保留不改。**2026-10-03 实现复核修订**：`stop` 语义改为塌缩 / 跳变 + 精确累计函数 `F`，见 §2.1 末段。**2026-10-03 输入半批复核（`CM-T10` 相关）**：负 `observationTick` 合法且参与单调排序、只有校准会话时钟回退被拒（属时间次序关系错误 → `invalid_relation`，不是 `budget_exceeded`）、不可表示的 calibrated clock 值复用既有 tick 域表示失败、跨 gap / 重连 / 丢样复用既有 `input.continuous_unsupported`（映射既有 **R-05**），见 §4.3） |
| Q-05 | CXT v2 `effects` 与 V2 "Requirement 不含 effects"冲突 | CM-L04、CM-V09 | 删除 Gameplay 语义内的 Requirement `effects`；非空 `effects` 只能显式 lowering 为 Presentation/Effect Graph；若保留必须声明 presentation-only 且不影响 Replay | S7A-3（**已裁定（第 3 轮，Codex 2026-10-03）**：修改后接受；见 §3.1 裁定登记。上列"候选处置"列为 2026-10-02 的原始建议文本，保留不改） |
| Q-06 | CXC Gameplay/Ruleset 发布入口未闭合（`playback=true` 的 `entryKind`、`gameplay-graph` 的 closure/预算/拒绝、Ruleset package 是否第三种 entry） | CM-V10、CM-V11 | `playback=true` 必须带明确 `entryKind`；7A 允许 `packed-chart` 与 `gameplay-graph` 两种；Ruleset package 只是 prepare 输入；manifest 记录 entry kind、compiled semantic identity、artifact identity、Ruleset binding、capability closure、source-of | S7A-7（**已裁定（第 6 轮，Codex 2026-10-03，`S7A6-R01`）**：接受——`playback = true` 必须显式 `entryKind`，7A **只允许** `packed-chart` 与 `gameplay-graph`，`author-source` **不是** Playback entry（仅审查 / 迁移 / 复现），Ruleset package **不是第三种 entry**（与第 5 轮内置 Ruleset 决定一致），manifest 必须记录 entry kind / compiled semantic identity / artifact identity / Ruleset binding / capability closure / resource·presentation closure / `sourceOf`；见 §4.4 裁定登记。上列"候选处置"列**保留 2026-10-02 原文不改**） |
| Q-07 | Packed `REQ0`/`CNS0` 兼容边界：新 wire revision 还是新增 section；旧 reader 行为 | CM-V08 | 保留 REQ0/CNS0 Foundation 语义；7A gameplay 另立 candidate wire revision 或新 section 组；未知必需 section / 未知 capability / 新 requirement kind 一律稳定拒绝 | S7A-3 / S7A-7（**已裁定（第 3 轮，Codex 2026-10-03）**：修改后接受，**只冻结行为、不冻结 wire 编号 / 布局 / 编码**；具体表示在 S7A-3 消费前另行闭合；见 §3.1 裁定登记） |
| Q-08 | Ruleset 事务失败后状态未定义（是否产生 PresentationEvent、是否追加 fault Fact、faulted 后 advance/seek/snapshot/replay 行为、RuleEffect 能否在 state 未提交时排队） | CM-S09、CM-K06 | 拆三阶段：Fact commit（不可撤回）/ Ruleset state commit（失败则整 Tick 不提交）/ Presentation projection（是否继续由 Fact 重建须显式选择并测试）；session 进入可查询 `faulted` | S7A-5（**已裁定（第 5 轮，Codex 2026-10-03，`S7A5-R03`）**：见 §4.2 裁定登记；上列"候选处置"列为 2026-10-02 的原始建议文本，保留不改） |
| Q-09 | FactBinding 与 AnimationSystem 生命周期未闭合（precedence、立即/排队/延迟应用、多 Fact 幂等、seek/reload 后 token 重建、lifetime 终止条件） | CM-P04、CM-P08 | 7A 用独立 Gameplay-to-Presentation adapter，把已提交 FactBinding 转成带 source Fact 的 HostOverride token；AnimationSystem 不读 Judgement；seek/replay 从 Fact Ledger 重建 token；precedence 成为命名常量并进入交接测试 | S7A-7（**已裁定（第 6 轮，Codex 2026-10-03，`S7A6-R03` + `S7A6-R05`）**：接受——命名层 `GameplayOverride`，precedence 常量 **`Initial < Behavior < Animation < GameplayOverride/FactBinding < StudioPreviewOverride`**，`render.visible = false` 是该层**显式值**；adapter **只消费已提交 FactBinding**；**当前有效立即应用、未来 `effectivePresentationTick` 排队**；`(factId, targetId)` 去重且重复幂等；**seek / replay / reload 从 Fact Ledger 重建 token**；lifetime 到期或 reset / session replacement 时终止；**表现失败绝不回写 Fact Ledger**；`groupCommit` 部分提交不回滚并记稳定 `partial-group` 诊断。见 §4.4 裁定登记。上列"候选处置"列**保留 2026-10-02 原文不改**） |
| Q-10 | 四类 identity 字段归属不一致（prepared grace、solver profile、late policy、normalization profile、TimebaseProfile、resourceDecisionPolicyRef、FactBinding aggregation、Ruleset build hash、几何边界、sourceMap、三种 semanticIdentity 关系） | CM-I06、CM-P05 | 建立唯一 identity projection 表，逐项标注 source/semantic/artifact/judgement/presentation；以"实际生效的 prepared value"为准；能改变 Fact Ledger 内容或顺序者必须改变 judgement identity | S7A-1 / S7A-6 |
| Q-11 | 资源状态机缺 `capacity > 1` 与命名空间合同（`resourceId` 归属、`capacity>1` 语义、lease/contact identity、`grace = 0`、gap 到期、terminal 范围、observe 是否占资源、同 Tick phase 顺序） | CM-C09、CM-C03 | 资源定义为 typed ResourceSlot 或显式 owner set；`capacity = 1` 与 `> 1` 使用不同状态表；命名空间/slot/lease identity 进 canonical graph；7A 只接受 `capacity = 1`、exclusive、无复杂 handoff | S7A-4（**已裁定（第 4 轮，Codex 2026-10-03，`S7A4-R02` / `S7A4-R05`）**：修改后接受；见 §4.1 裁定登记。上列"候选处置"列为 2026-10-02 的原始建议文本，保留不改） |
| Q-12 | Fact 因果总序生成算法未冻结（`originId`/`commitId`/`phasePriority`/`factId` 的生成与编码、seek 后 timer 重新编号、correction 能否影响同 Tick 排序） | CM-F06、CM-F03/F04 | 规范 tuple `(originKind, originScope, originOrdinal, localOrdinal)`；scope/ordinal 由引擎基于 canonical window/observation identity 派生，宿主不能提供；phase registry、commit allocation、factId encoding 以 bytes 写入 reference evaluator 并进入 Fact semantic revision | S7A-5 / S7A-6（**已裁定（第 5 轮，Codex 2026-10-03，`S7A5-R01` + `S7A5-R02`）**：见 §4.2 裁定登记；上列"候选处置"列为 2026-10-02 的原始建议文本，保留不改。最终总序为 **`(commitTick, originKindPriority, canonicalOrdinal)`**，**不使用 ingress 顺序**；**物理字节与 reference fixture 仍留 S7A-6**） |
| Q-13 | Snapshot/Replay schema 未与 V2 状态闭包对齐（未提交 window、Candidate cache、lease、program registers、Fact Ledger 存储策略、schema revision/预算、跨版本接受矩阵） | CM-K04、CM-K07 | 7A 采用全量语义 Snapshot；header 含四分量 identity、semantic revision、state schema revision、event/fact count、byte budget；表现缓存不保存；增量 snapshot / 压缩 ledger / 跨 minor 迁移在无全量 golden 前不进公共合同 | S7A-6（**已裁定（第 5 轮，Codex 2026-10-03，`S7A5-R06` + `S7A5-R07`）**：见 §4.2 裁定登记；上列"候选处置"列为 2026-10-02 的原始建议文本，保留不改。**字段集与 `SnapshotPayload` 闭包已冻结；字节布局与预算数值仍留后续批次 / S7A-9**） |
| Q-14 | capability 派生来源不唯一（Packed META / CXT extension / Chart gameplay / registry / Session negotiation 五来源） | CM-X06 | source 只声明最低必需 capability；assembler 派生完整 closure；Packed header 保存排序后的 derived closure；CXC manifest 复制 artifact-required capability；Session 只报告四态、不改写 graph；声明少于派生必须失败或由 compiler 补齐 | S7A-1 / S7A-3 |
| Q-15 | Gameplay / Presentation / Effect 发布粒度不清（三张图 vs Chart v5 聚合 vs semantic/packed/presentation-pack 三类 entry；缺 Presentation 能否 headless；FactBinding target 缺失如何处理） | CM-P06、CM-I05 | 7A 把 Gameplay graph 作为必需判定闭包，Presentation/Effect 作为可选只读投影；缺 Presentation 允许 headless judgement；缺影响判定的 domain/action/table 必须拒绝；Effect target 缺失不改 Fact Ledger，但需明确诊断与事件丢弃策略 | S7A-7（**已裁定（第 6 轮，Codex 2026-10-03，`S7A6-R04`）**：接受——Gameplay graph 是**必需判定闭包**、Presentation / Effect 是**可选只读投影**；缺 Presentation 允许 headless judgement；**判定闭包内 target / domain / action / table 缺失 = prepare 原子失败**（`identity_closure_incomplete` 或 `invalid_relation`）；**纯表现 target 缺失**允许空绑定、丢弃该投影事件并给稳定 `presentation-target-missing` 诊断、**不 fault session**、不改 Fact Ledger。见 §4.4 裁定登记。上列"候选处置"列**保留 2026-10-02 原文不改**） |

## 2. 本 package 追加的未决项（4 条）

| 编号 | 未决点 | 涉及合同项 | 候选处置 | 必须决定的批次 |
| --- | --- | --- | --- | --- |
| Q-16 | 两级求值顺序不匹配：计划 §S7A-4 固定**六步 Tick 顺序**，Preliminary Design §5.3 给出**八步 Coordination window 阶段**，两者映射关系未定义 | CM-C05 | 明确"Tick 六步"是外层、"Coordination window 八步"是第 4 步内部展开；把两者的对应关系写成一张阶段表并进入 engine identity | S7A-4（**已裁定（第 4 轮，Codex 2026-10-03，`S7A4-R03` / `S7A4-R04`）**：接受；见 §4.1 裁定登记。上列"候选处置"列为 2026-10-02 的原始建议文本，保留不改） |
| Q-17 | 7A 是否包含 `cuexis.ruleset` package 支持（计划把 package 放在 S7C-2） | CM-S08 | 7A 只用内置 Ruleset；package 形状保留为 S7C-2 能力，遇到 package 输入稳定拒绝 | S7A-5（**已裁定（第 5 轮，Codex 2026-10-03，`S7A5-R05`）**：接受——7A 只用内置 Ruleset；`cuexis.ruleset` package 命中 **R-09**（`ruleset.package_unsupported`），**不得读** hash / manifest / 迁移矩阵；package 形状 / identity / 迁移归 **S7C-2**。见 §4.2 裁定登记；上列"候选处置"列为 2026-10-02 的原始建议文本，保留不改） |
| Q-18 | Release/tail 的 inclusion decision：计划 §S7A-3.3 说"仅在明确声明时进入"，Preliminary Design §3.1 把 Release 列入 7A 最小闭环 | CM-R06 | 接受"Release/tail 是同一 Requirement 的可选 phase、**必须显式声明**才启用"，并保留"未声明却要求 tail 语义即拒绝" | S7A-3 / S7A-4（**已裁定（第 3 轮，Codex 2026-10-03）**：接受；`P2-10` 随之关闭、不另设选择；见 §3.1 裁定登记） |
| Q-19 | 7A 的 Packed/CXC 是否必须携带 `compiledSemanticIdentity` 与 capability closure（Q-06 的子问题），以及 `author-source` entry 的用途边界 | CM-V10、CM-X06 | 是；两者都必须携带可比较的 compiled semantic identity；`author-source` 只能用于审查/迁移/复现 | S7A-7（**已裁定（第 6 轮，Codex 2026-10-03，`S7A6-R02`）**：接受——**`compiledSemanticIdentity` 与完整 capability closure 必须同时携带且可比较**；`author-source` **只能**用于审查 / 迁移 / 复现，**不构成 Playback entry**；`CapabilityRecord` 七字段与逐字段 identity 归属同时冻结。见 §4.4 裁定登记。上列"候选处置"列**保留 2026-10-02 原文不改**） |

### 2.1 第 2 轮裁定登记（2026-10-03）

**provenance。** 决策卡 thread `s7a1-freeze-bindings`，verdict `adopt`，confidence 0.99，日期 2026-10-03，
模型 `gpt-6-astra`；补充确认卡 thread `cm-x01-disposition`（`CM-X01` 保持 `open`，不改计数），verdict
`adopt`，confidence 0.99，2026-10-03。带日期的落地证据见
[第 2 轮门禁报告](../../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-2-gate-rulings.md)。**本小节只登记裁定
结果，不改写 §1–§5 表中 2026-10-02 的原始证据文字。**

| 编号 | 涉及合同项 | 裁定（第 2 轮，2026-10-03） | 落点 |
| --- | --- | --- | --- |
| Q-04 | CM-T02/T03/T04 | **接受**：`ChartTick` / `judgementTick` / `observationTick` / `commitTick` 均为**有符号 64 位整数**，单位由 `engine.tick.us.v1` 的 `TimebaseProfile` 声明；**禁止隐式单位换算**；**溢出稳定拒绝** | Spec §0.1、§3.7.1；ABI 域 1 与 §单位与量程（**2026-10-03 实现复核修订**见本节末段） |
| CM-T03 | CM-T03 | **接受**：RationalBeat → `judgementTick` 用**精确有理数运算**，**四舍五入到最近整数、半数取偶**；tempo / stop / 负 Beat 同一规则；同 Tick 按 **`(tick, originKind, canonicalOrdinal)`** 排序，**禁止按 ingress 顺序排序** | Spec §3.7.2；CONTRACT_MATRIX §3（`open` → `accept`）。**2026-10-03 实现复核修订**：`stop` 为**塌缩 / 跳变 + 精确累计函数** `F`（见本节末段） |
| CM-T04 | CM-T04 | **接受**：`observationTick` **唯一**来自输入进入判定管线时捕获的**校准后会话时钟**；设备 / 到达 / 音频 / 渲染帧时间**仅诊断上下文**；**S7C-1 只能扩展 `CalibrationProfile` 参数，不得替换 canonical source** | Spec §3.7.3；CONTRACT_MATRIX §3（`open` → `accept`） |
| CM-T08 / P1-04 | CM-T07、CM-T08 | **接受**：`finalizationWatermark`、queue hop、窗口 open/close、重复排队条件均为 **typed 参数**（由 `TimebaseProfile` / ruleset 提供）；默认值**只在 profile registry 登记为 `pending_measurement`**；**缺失或未测量在 prepare 稳定拒绝**；**重复排队稳定拒绝 + 诊断码**；**具体数值为后续批次阻塞项** | Spec §3.7.4、§3.7.7；CONTRACT_MATRIX §3（`open` → `accept`） |
| P2-03 | —（术语项） | **接受**：`commitTick` 是**事务提交发生的 Tick**；window 是**相邻两个可提交 Tick 之间的观察区间**；**事实只能在 `commitTick` 提交** | Spec §0.1 与 §3.7.5；ABI 域 1 `CommitTick` |
| CM-T13（新增裁决项） | —（非 CONTRACT_MATRIX 合同项） | **接受**：每个 `InputDomain` 必须携带 typed **`AmountSpec`**（canonical 整数量化 / scale / 可表示范围 / 边界策略）；量化用**精确整数运算**、**最近值半数取偶**；**超范围 / 窄化 / 溢出稳定拒绝 + 诊断码**；**本轮不冻结具体业务量程数值或限额** | Spec §3.7.6；ABI 域 2 `DomainAmount` 与 §数值域与量程。**裁定指针（2026-10-03 输入半批复核）**：canonical 整数宽度**明文冻结为有符号 64 位**、判定顺序**可表示性先于范围**、域声明进 session identity——见 §4.3 |
| F-04（文本缺陷） | —（缺陷，来自 typed contract review §6） | **已闭合（第 2 轮，Codex 2026-10-03）**：ABI 字段级映射表新增 `InputEvent.source` 行；`SourceClass`（设备 / 来源类别）与 `SourceBuildIdentity`（生产者、参数、compiler profile、source map 的构建来源）分工写明；后者**不进入判定语义** | [GAMEPLAY_V2_ABI.md](../../api/GAMEPLAY_V2_ABI.md) 字段级映射表（`InputEvent.source` 行）+ 输入域小节（域 2 / 域 7） |

**登记口径。** 上述 7 条**全部登记为阻塞 S7A-2 的门禁**；`S7C-1` 校准扩展、**具体 late-policy 数值**、
**具体业务量程限额**、**连续输入能力**登记为**后续批次**阻塞项（不是 S7A-2 门禁）。`CM-X01` 的处置词
保持 `open`（第 6 轮），相关计数不回改。

**第 2 轮语义的实现复核修订（2026-10-03）。** S7A-2 时基半批实现落地后，同一 Codex 话题
thread `s7a1-freeze-bindings` 的两张续裁卡（verdict 均 `adopt`，confidence **0.98** / **0.99**）修订了
上表两处表述：①`stop` 语义由"比例替换"修订为**塌缩 / 跳变 + 精确累计函数**——`F(originBeat) = 0`、
`F(endBeat) = F(startBeat) + duration`、`tick(beat) = roundHalfToEven(F(beat))`（**只舍入一次**，
不得写成整数递推 `tick(endBeat) = tick(startBeat) + duration`），`stop` 区间内映射**非单射**，半开方向
正向 `(originBeat, beat]` / 负向 `(beat, originBeat]`，**有 stop 时不再关于 origin 奇对称**（有向端点关系
与单调性仍成立），且该语义**属 S7A-2 必须冻结的部分**；②`validatePrepare` **只**检查 late-policy 参数
已声明且已测量，**不检查** `finalizationWatermark` / queue hop / 窗口开闭阈值之间的**量级关系**——该校验
登记为**阻塞 S7A-4 的 late-window / 判定消费门禁**，最终数值与限额仍记 **S7A-9**。修订另登记三分法
（声明结构不完整 → `invalid_relation`；已声明但声明域内数值非法 → `budget_exceeded`；late-policy 缺失 /
未声明 / 未测量 → `late_policy_incomplete`；`profileId` / `unitToken` 保持字符串、空值表示未声明、
**不是**默认单位或可用 profile）与 **Tick → Beat 反查**契约（不得返回单一 Beat，登记为首次消费该反查的
后续批次阻塞项）。**上表第 2 轮的原始裁定文字保留不改**；修订不改变任何计数（`open` 仍 14、§7.2 仍
19 条、§9.2 仍九类）。落点：Spec §3.7.2 / §3.7.4 第 5 条 / §3.7.7 表 / §9.3；带日期证据见
[第 2 轮实现裁定记录](../../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-2-implementation-rulings.md)。

## 3. P1：不阻塞最小内核，但会重复改写（15 条）

| 编号 | 未决点 | 涉及合同项 |
| --- | --- | --- |
| P1-01 | 判定域的 typed contract：判定几何是否属 Gameplay closure、坐标系与量程声明、与 Presentation transform 的隔离、未来动态 frame 是否静态 typed record | CM-R01、CM-I06 |
| P1-02 | CXT local relation 的全局合并规则（namespace、稳定 ID、同名合并、跨 invocation 引用、source/invocation 重排规范化） | CM-C02、CM-V09 |
| P1-03 | 有界循环的 canonical form（完全展开 vs `boundedRelationInstance`）及其 identity/snapshot/fact order | CM-C10 |
| P1-04 | late policy 的 `finalizationWatermark`、最大 queue hop、窗口 open/close、重复排队条件与状态表 | CM-T07、CM-T08 |
| P1-05 | `input.trajectory.v1` 的 source event schema、量化类型、坐标系、断点后 contact 生命周期、区域边界含义、采样失败处置 | CM-L02 |
| P1-06 | Ruleset package 的执行与分发边界（CXC entry kind、package identity、版本迁移、缺包行为、prepare 时机） | CM-S08 |
| P1-07 | 7A 最小 `outcome`/`category`/`grade` 集合、缺 grade 表行为、错误单位、统计累计与 reset/seek 规则（**已裁定（第 5 轮，Codex 2026-10-03，`S7A5-R09`）**，见 §4.2：`Outcome` = `{hit, miss}`、Hold 三段为 phase-local、`FactCategory` 与 phase 一一对应、grade 可选且缺失即 absent、`TimingError` 为有符号整数 tick 差、seek / replay 从 Fact Ledger 重建统计、reset 不产生 Fact；本行仍按 §3 表列、不改写 2026-10-02 的原始文字） | CM-S10 |
| P1-08 | ABI 类型与 canonical model 的映射（`InputEvent.observationTime`、`JudgementFact.error`、`JudgementResult.eventSequence`） | CM-I07 |
| P1-09 | 诊断层级与稳定拒绝码（code/category/severity/source path/identity component/recoverability/faulted 行为） | CM-D02、CM-D04 |
| P1-10 | 预算分层与 Packed/Runtime 对齐；新增 Candidate/branch/lease/timer/Fact/state/Presentation/Snapshot/Seek 成本的归属与最坏值计数入口 | 见 [BUDGET_AND_EVIDENCE_PLAN.md](BUDGET_AND_EVIDENCE_PLAN.md) |
| P1-11 | FactBinding `aggregation` 与重复事实（多 phase、重复 Hit、Miss 后 late Hit、部分 group 失败、correction、target 已隐藏的幂等） | CM-P08 |
| P1-12 | early/late timing error 与窗口边界（Exact 是否要求零 tick、量化误差、窗口外早击是否产生 Fact、Miss/absence deadline、Hold head/body/tail error） | CM-P02 |
| P1-13 | Presentation target 缺失策略（判定闭包内必须失败 vs 纯表现允许空绑定） | CM-P06 |
| P1-14 | resource closure 与 Packed reference closure 的归属表（哪些进 REF0、哪些只进 manifest、哪些只是诊断/source map）（**已裁定（第 5 轮，Codex 2026-10-03，`S7A5-R11`）**，见 §4.2：规则与**具体归属矩阵**作为 Spec 附录冻结；判定必需引用进 REF0 / judgement closure、纯表现引用进 manifest / presentation closure、source map 仅诊断；REF0 物理字段 / 编号 / 编码由 **S7A-3** 首次 Packed 写入消费，entry / manifest 集成属 **S7A-7**） | CM-V08、CM-I02 |
| P1-15 | Chart v5 inline 与 CXT v2 emission 的等价性定义（字段顺序、默认补全、sourceMap 忽略规则、local relation 提升）与逐字段 semantic diff | CM-V09、CM-I03 |

### 3.1 第 3 轮裁定登记（2026-10-03）

provenance：三张 Codex 决策卡来自同一话题 thread `s7a3-prepare-rulings`，模型 `gpt-6-astra`，模式 consult：
主裁定卡 verdict `need_info`、confidence 0.8（本轮 5 条裁定与规范字节边界）；补答 `CM-X05` 的卡 verdict
`reject`、confidence 0.88；计数一致性确认卡 verdict `adopt`、confidence 0.99。三张卡的文件名时间戳为
2026-10-02T19:4xZ（UTC），统一按**本地日期 2026-10-03** 登记（与第 2 轮先例一致）。带日期的落地证据见
[第 3 轮门禁报告](../../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-3-gate-rulings.md)。**本小节只登记裁定
结果，不改写 §1–§5 表中 2026-10-02 的原始证据文字。**

| 编号 | 涉及合同项 | 裁定（第 3 轮，2026-10-03） | 落点 |
| --- | --- | --- | --- |
| Q-05 | CM-L04、CM-V09 | **修改后接受**：Requirement **不含** Gameplay `effects`；非空 `effects` 只能**显式 lowering 为不影响 Judgement / Replay 的 Presentation 数据**，否则**拒绝**。**阻塞 S7A-3** | Spec §3.3、§3.8.1、§7.2；ABI 域 3 `RequirementRecord` |
| Q-07 | CM-V08 | **修改后接受**：`REQ0` / `CNS0` 保持 Foundation 语义；7A 新语义须使用**可与旧语义区分的 entry / section 边界**；未知必需 section、未知 capability、新 requirement kind **稳定拒绝**。**第 3 轮只冻结行为，不冻结 wire 编号、布局或序列化编码**，具体表示在 S7A-3 消费前另行闭合。**阻塞 S7A-3** | Spec §2.5、§3.8.2；ABI 域 8 与 §S7A-3 节 |
| Q-18 | CM-R06 | **接受**：Release / tail **仅作为同一 Requirement 显式声明的可选 phase**；内容要求 tail 语义却未声明时**拒绝**。**阻塞 S7A-3** | Spec §3.8.3、§7.2；ABI 域 3 `Phase` |
| P2-10 | —（重述 Q-18，非合同项） | **随 Q-18 关闭，不另设选择** | 同 Q-18；[RULING_WORKSHEET.md](RULING_WORKSHEET.md) §8 的"8 个合并行"注已登记该重述关系 |
| CM-C10 / P1-03 | CM-C10 | **修改后接受**：7A 的 prepare-time 有界循环**完全展开**，展开结果**唯一决定** identity / snapshot / fact order；`boundedRelationInstance` **不进 7A 合同**，遇到时**稳定拒绝**，列为 **7B+ / S7C 候选**。7A 选择**阻塞 S7A-3**；候选表示只阻塞**首次拟消费它**的后续批次。处置词 `open` → `accept` | Spec §3.8.4、§7.2；ABI 域 3 `PatternPrimitive`；CONTRACT_MATRIX §3 |
| CM-X05 | CM-X05 | **由 D-3 落地关闭**：W 类缺口原无定义；D-3 修订后由 plan §5.3 规定登记与引用规则，SUPPORT §6 定义字段。处置词 `open` → **`revise`**（**不是** `accept`）；**不再阻塞 S7A-3**，门禁清单标为"D-3 已关闭；仅具体 W 缺口在首次消费前须完成登记" | [CONTRACT_MATRIX.md](CONTRACT_MATRIX.md) §12；[SUPPORT_AND_REJECTION_MATRIX.md](SUPPORT_AND_REJECTION_MATRIX.md) §6 与 §6.1；Spec §1.3 |
| 规范字节边界（张力点） | 计划 §S7A-3 第 5 条 vs S1-05 | **裁定**：identity 的**规范字节是 prepare 内部确定性派生物**，**不是公共 ABI 或 Packed 编码**；冻结**语义等价、排序与身份域边界**，**暂不冻结编码**；S7A-3 实现须用**同一算法在各工具链产出相同字节**，以**跨工具链 golden、输入置换、file / memory 对照**验证；**编码实现须在 S7A-3 消费前闭合**；ABI **仅声明对外 identity 的不透明性**，不承诺内部字节格式 | Spec §5.5 与 §S7A-3 节；ABI 域 7 与 §稳定与非稳定声明 |

**登记口径。** 上述 `Q-05`、`Q-07`、`Q-18` / `P2-10`、`CM-C10` / `P1-03` 与规范字节边界登记为**阻塞 S7A-3
的门禁**（identity 的**编码实现**也须在 S7A-3 消费前闭合）；`CM-C10` 的**候选表示**
（`boundedRelationInstance`）与 SUPPORT §6 的 `W-01…W-05` 候选项只阻塞**首次拟消费它**的后续批次。
`CM-X05` 由 D-3 落地关闭，**不再阻塞 S7A-3**。计数：`CONTRACT_MATRIX` 的 `open` 18 → **16**、`accept`
35 → **36**、`revise` 25 → **26**，总数仍 **114**；`RULING_WORKSHEET` §8 的 `open` 行 18 → **16**（第 3 轮
分布 2 → **0**）；§0.1 第 3 轮末列仍保留 `CM-X05`、`CM-C10`（该列语义是"哪一轮一并关闭"，不随处置词变化）。

## 4. P2：需要澄清（10 条）

| 编号 | 需要澄清 |
| --- | --- |
| P2-01 | "Chart v5 唯一聚合入口"是语义唯一还是物理入口唯一 |
| P2-02 | "Playback 不执行 solver"的措辞改为"不解析/编译 solver；runtime coordinator 只执行已准备的确定性 policy"（**措辞已由第 4 轮 `S7A4-R01` 确认改写方向**，见 §4.1；并**已由第 7 轮 `S7A7-R07` 逐处落地并关闭**，2026-10-03，见 §4.5——落点 Spec §3.12 / §3.13、ABI 域 4、plan §S7A-4） |
| P2-03 | `commitTick` 的 window / transaction 定义 |
| P2-04 | `phase` / `category` / `outcome` / `grade` / `localOutcome` / `reasonCode` 的层级与互斥关系 |
| P2-05 | `action = press|release|update|absence|step` 与 `requiredAction` / `domain` / `InputDomain` 的关系 |
| P2-06 | `resources` 既像 claim intent 又像全局定义，应拆为 `resourceRef` / `claimPolicy` / resource record |
| P2-07 | `sourceMap` / source-build identity / content identity 的重叠，应统一为 source closure / diagnostic map / content-artifact identity |
| P2-08 | Effect Graph / EffectEvent / RuleEffectEvent / PresentationEvent 的词汇表，尤其哪些进 Snapshot/Replay/FrameSnapshot |
| P2-09 | "所有未知字段默认拒绝"与 CXC `extensions` 保留未知可选 inspection metadata 的区分 |
| P2-10 | 见 Q-18（Release/tail inclusion decision） |

### 4.1 第 4 轮裁定登记（2026-10-03）

**provenance。** 两张决策卡来自同一 Codex 话题 thread `s7a4-arbitration-rulings`（模型 `gpt-6-astra`，
consult）：主裁定卡 verdict `adopt`、confidence 0.97（本轮六项裁定；"以六步 Tick 为外层、八阶段为第 4 步
内部规范"，7A 仅支持 `free` / `held` / `terminal` 的 `capacity = 1` exclusive 资源与 prepare 后确定性
coordinator）；处置词与计数确认卡 verdict `adopt`、confidence 0.99（`CM-C05` / `CM-C09` 均 `open` →
`accept`；最终 `open` 14 / `accept` 38；总数仍 114）。两张卡的文件名时间戳为 2026-10-02T20:0xZ（UTC），
本轮统一按**本地日期 2026-10-03** 登记（与第 2、3 轮先例一致）。带日期的落地证据见
[第 4 轮门禁报告](../../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-4-gate-rulings.md)。**本小节只登记裁定
结果，不改写 §1–§5 表中 2026-10-02 的原始证据文字**（`Q-03` / `Q-11` / `Q-16` 三行仅在"必须决定的批次"
列追加裁定指针）。

| 编号 | 涉及合同项 | 裁定（第 4 轮，2026-10-03） | 落点 |
| --- | --- | --- | --- |
| Q-03 / `S7A4-R01` | CM-L10、CM-C06 | **接受**：拆分为 **prepare / compile solver**（展开、验证、**上界证明**、**唯一性证明**、生成 prepared profile）与 **runtime coordinator**（只执行 prepared profile 指定的确定性候选排序、资源检查与提交）；runtime coordinator **不解析也不编译 solver**；名称采用 **`coordinator.policy.greedy_v1`**，**不把 runtime 行为称为 solver**（`P2-02` 措辞随之改写） | Spec §3.12、§S7A-4 节；ABI 域 4 与 §S7A-4 节 |
| Q-11 / `S7A4-R02` | CM-C09、CM-C03 | **修改后接受**：7A 资源子集**冻结为 `free` / `held` / `terminal`**——`free + claim -> held`；`held + 合法 update -> held`；终止后按资源策略进入 `free` 或**永久 `terminal`**；**`gap`、`handoff_pending`、非零 grace、handoff、`capacity > 1`、owner 集合与并列 slot 一律稳定拒绝**；`CM-C04` 的"只启用前三态"据此改写，不再采用 preliminary 状态表的 gap/handoff 分支 | Spec §3.10、§S7A-4 节；ABI 域 4；CONTRACT_MATRIX §5 |
| Q-16 / `S7A4-R03` | CM-C05 | **接受**：**唯一**映射——**六步＝外层**、**八阶段＝第 4 步内部完整展开**（第 3 步＝八阶段第 1 步 Observation 收集；第 4 步＝八阶段第 2–7 步，7A 对 gap/recovery/handoff 输入稳定拒绝；第 5 步＝八阶段第 8 步由 commit 生成 Fact 后按 causal total order 排序；第 6 步 Tick 末提交 Hook / signal）；阶段顺序与语义进 **`judgement identity` 的 engine 组件**、**不进 chart/content identity**；**只冻结阶段名称、顺序、映射与语义边界**；`SK §4.2` 五步变体标为**非权威推导、已被取代** | Spec §3.9、§S7A-4 节；ABI 域 4 与 §S7A-4 节；[SEMANTIC_KERNEL.md](../research/gameplay-v2/SEMANTIC_KERNEL.md) §4.2 |
| CM-C05 / `S7A4-R04` | CM-C05 | **接受**：随 `Q-16` 关闭；处置词 `open` → **`accept`** | Spec §3.9；[CONTRACT_MATRIX.md](CONTRACT_MATRIX.md) §5（`open` → `accept`） |
| CM-C09 / `S7A4-R05` | CM-C09、CM-C03 | **修改后接受**：随 `Q-11` 关闭；`resourceId` **只在 prepared canonical graph 的资源命名空间内**解释；`capacity = 1` 资源使用**唯一 slot**；slot / lease / 最小 contact handle / claim identity **进入 canonical graph 与 prepared judgement inputs**；lease / contact 由引擎**按规范阶段分配**、宿主不得提供；`contact end` **不复用**旧 handle；**`observe` 只产生 Observation，不占用、不改变资源 owner**；非零 grace / `gap` / `handoff_pending` / `capacity > 1` 稳定拒绝。处置词 `open` → **`accept`** | Spec §3.10、§S7A-4 节；ABI 域 4；[CONTRACT_MATRIX.md](CONTRACT_MATRIX.md) §5（`open` → `accept`） |
| P1-12 / `S7A4-R06` | CM-P02 | **接受**：Exact **由判定窗口半宽定义**而非要求零 tick；**窗口外早击不产生 Fact 但产生诊断**；**Miss / absence 由 deadline 产生 Fact**；Hold 的 head / body / tail **分别记录有符号 error、不合并** | Spec §3.11、§S7A-4 节；ABI 域 5 与 §单位与量程 |
| `SolverProfile` 语义（`CM-C06` / `CM-C07`） | CM-C06、CM-C07 | **语义闭合**：在 S7A-4 冻结**字段语义**、**缺失 / 歧义 / 无法证明唯一时的 prepare 拒绝语义**、`objective` / `tieBreak` 为**有序语义列表**；**不冻结**默认列表、具体算法预算、`K` / fuel / `max*` 数值或序列化表示（留给 **S7A-9** 与后续预算批次）。**处置词不变**（`revise` / `accept`） | Spec §3.12；[CONTRACT_MATRIX.md](CONTRACT_MATRIX.md) §5 |

**登记口径（门禁归属）。** 上表 7 行中的 `Q-03`、`Q-11`、`Q-16`、`CM-C05`、`CM-C09`、`P1-12` 及
`CM-C06` / `CM-C07` 的**语义闭合**全部登记为**阻塞 S7A-4 的门禁**；**具体预算数值、默认 profile 清单、
proof 编码、wire / serialization、`terminal` 编码、后续 `gap` / handoff / `capacity > 1` 语义**登记为
**首次消费它们的后续批次**阻塞项（数值与默认列表记 **S7A-9**；其余由首次消费它的批次阻塞；后续
`gap` / handoff / `capacity > 1` 属 **S7B+ / S7C** 候选），**均不阻塞 S7A-4**。`CM-C04`、`CM-C06`、
`CM-C07` 的处置词**不变**（`revise` / `revise` / `accept`），只更新未决点文字。第 4 轮**不新增 R 编号**，
§3 的拒绝清单仍为 **19 条**（映射见 Spec §7.2 的第 4 轮段）。

### 4.2 第 5 轮裁定登记（2026-10-03）

**provenance。** 三张决策卡来自同一 Codex 话题 thread `01a0fe49-cfe5-7933-8d03-3d0a65d34c8b`（consult）：
①主裁定卡 verdict `adopt`、confidence 0.95（第 5 轮 13 行裁定，编号 `S7A5-R01…R11`；`S7A5-R10` 只用于
`LifeState`，`P1-14` 另登记为 `S7A5-R11`）；②处置词与计数确认卡 verdict `adopt`、confidence 0.99
（`CM-F06`、`CM-S08`、`CM-S10`、`CM-K07` 四条 `open` → `accept`；最终 `open` **10** / `accept` **42**；
总数仍 114；§0.1 末列仍为 **21**；`CM-S11`、`CM-K08` 不改变任何计数）；③ABI 状态格首词与追踪计数追问卡
verdict `adopt`、confidence 0.98（9 行状态格首词全部保持 `待冻结`；**126 = 96 + 30** 不变；**不新增
§未决项 #21、不新增 §类型清单条目**）。三张卡的文件名时间戳为 2026-10-02T20:2x–20:3xZ（UTC），统一按
**本地日期 2026-10-03** 登记（与第 2、3、4 轮先例一致）。带日期的落地证据见
[第 5 轮门禁报告](../../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-5-6-gate-rulings.md)。**本小节只登记裁定
结果，不改写 §1–§3 表中 2026-10-02 的原始证据文字**（`Q-08` / `Q-12` / `Q-13` / `Q-17` 四行仅在"必须决定的
批次"列追加裁定指针；`P1-07` / `P1-14` 两行只在未决点列追加指针）。

| 编号 | 涉及合同项 | 裁定（第 5 轮，2026-10-03） | 落点 |
| --- | --- | --- | --- |
| Q-08 / `S7A5-R03` | CM-S09、CM-K06 | **接受**：Ruleset Tick 固定为**三阶段**——①追加并封存**已排序** Fact Ledger；②**原子校验并提交全部** StateDelta / Score / Combo / Statistics / RuleEffect；③**只从已提交 Fact Ledger 与 FactBinding 投影**只读 PresentationEvent。第二阶段失败时**不追加 fault Fact**、**不发布** RuleEffect / PresentationEvent、**保留旧 state**，session 进入可查询 `faulted`；`faulted` 的 `submit` / `advance` / `seek` / `replay` / 就地 `reload` 与 **`snapshot`** 一律**稳定失败**；只有**显式 reset** 或**创建替换新 session 的 reload / recovery** 可离开，且**不得恢复或伪造未提交 StateDelta** | Spec §3.14、§9.4、§10 第 6 条、§S7A-5 节；ABI 域 6 与 §S7A-5 节；[plan.md §S7A-5 验证段](../../stage_plans/active/stage-07/plan.md) |
| Q-12 / `S7A5-R01` | CM-F06、CM-F03/F04 | **接受**：唯一规范总序为 **`(commitTick, originKindPriority, canonicalOrdinal)`**；`canonicalOrdinal` 由引擎从 `(originScope, originOrdinal, localOrdinal, phasePriority, factKindPriority)` 派生；**不得使用 `ingressSequence`、容器顺序或线程完成顺序**；Fact 的 `tick` 即 `commitTick`；`observationTick` 只定位 observation origin、**不是**总序首键。修订 Spec §3.9 第 3 条与 plan §S7A-4 第 5 步（删除含 `ingressSequence` 的旧 tuple） | Spec §3.9 第 3 条、§3.17；ABI 域 5；[plan.md §S7A-4 第 5 步](../../stage_plans/active/stage-07/plan.md) |
| Q-13 / `S7A5-R06` + `S7A5-R07` | CM-K04、CM-K07 | **接受**：**全量 Snapshot header 字段集** = `formatVersion`、四分量 `JudgementIdentity`、`factSemanticRevision`、`stateSchemaRevision`、normalized event count、Fact count、payload byte count、typed byte-budget descriptor；**Replay header 字段集** = `formatVersion`、四分量 identity、`NormalizationProfile` metadata、effective late-policy metadata、`eventCodecId`、normalized event count、encoded byte count、`ReplayDecodeBudget`。**拼写规范：类型 `EventCodecId`、字段 `eventCodecId`**。`factSemanticRevision` 进 engine identity；`stateSchemaRevision` 只标识 `SnapshotPayload` 的无损状态结构、**不进** judgement identity。`SnapshotPayload` 闭包（Fact Ledger 至 cursor 的前缀 + cursor、活动实例、Pattern 状态、Release / tail phase、exclusive resource 状态、finalization watermark、未提交窗口、`preparedGrace`、离散 sequence 状态、Hook snapshot、Fold / Score / Combo / Statistics 状态、pending signal queue、`SessionState`、fault 诊断 / 状态）与**不得保存清单**（Presentation / Effect cache、Animation 临时值、HostOverride token、未提交 StateDelta、连续采样相位）见 Spec §3.18；FactBinding 与 prepared immutable graph **重新取得、不重复保存**。**字节布局与预算数值留后续批次 / S7A-9** | Spec §3.18、§S7A-6 节；ABI 域 9 与 §未决项 #12 / #16 |
| Q-17 / `S7A5-R05` | CM-S08 | **接受**：7A **只用内置、静态注册的 Ruleset Interface / 模块**；任何 `cuexis.ruleset` package 输入命中**既有 R-09**，诊断 `ruleset.package_unsupported`，**不得读** package hash / manifest / 迁移矩阵；package 形状 / identity / 迁移归 **S7C-2** | Spec §3.16、§7.2 的 R-09；ABI 域 6 与 §未决项 #14 |
| CM-S08 / `S7A5-R05` | CM-S08 | **随 `Q-17` 关闭，不另设选择**：与 `Q-17` 同一裁定；**处置词 `open` → `accept`**。7A 不含 package；package 形状归 S7C-2 | 同 `Q-17`；[CONTRACT_MATRIX.md](CONTRACT_MATRIX.md) §7 |
| CM-F06 / `S7A5-R02` | CM-F06 | **接受**：`originId` 不透明、由 `(originKind, originScope, originOrdinal)` 唯一确定（scope / ordinal 由引擎派生、宿主不能提供）；`commitId` 按规范总序单调分配；`factId` = `(commitId, localOrdinal)` 的语义身份；`phasePriority` 是 engine phase registry 的稳定语义 rank、仅用于 `canonicalOrdinal`、**绝非评分**；timer ordinal 由 prepared timer identity 派生、seek / replay **不重新编号**；7A correction 仍由 **R-08** 拒绝；生成语义与 phase registry 进 **`FactSemanticRevision`**（属 `JudgementIdentity.engine`）。**物理字节布局、varint / endianness、`FactId` / `CommitId` 编码与 reference-evaluator byte fixture 留 S7A-6**。**处置词 `open` → `accept`** | Spec §3.17、§3.9；ABI 域 5 / 域 7 与 §未决项 #10；[CONTRACT_MATRIX.md](CONTRACT_MATRIX.md) §6 |
| `CM-S02` / `S7A5-R04` | CM-S02 | **接受（处置词不变，仍为 `revise`）**：`exclusive` 单 owner（第二 owner / 第二写入稳定失败）；`commutative_monoid` 只能由已声明贡献者以已声明且可交换、可结合的算子合成（未知 operator、重复 contribution identity、非交换 / 非结合合成稳定失败）；`ledger_derived` 禁止直接 StateDelta 写入、只能从已提交 Fact Ledger 重建；7A 只接受这三类、其他 kind 稳定拒绝；Hook 的"下一 Tick 可见"不变 | Spec §3.15、§S7A-5 节；ABI 域 6 `RegisterKind`；[CONTRACT_MATRIX.md](CONTRACT_MATRIX.md) §7 |
| `CM-K01` / `S7A5-R06` | CM-K01 | **接受（处置词不变，仍为 `revise`）**：Replay header 字段集随 `Q-13` 冻结；**`EventCodecId` 随该轮冻结**，拼写规范为类型 `EventCodecId` / 字段 `eventCodecId`；**预算数值仍留 S7A-9** | Spec §3.18、§S7A-6 节；ABI 域 9；[CONTRACT_MATRIX.md](CONTRACT_MATRIX.md) §10 |
| `CM-K07` / `S7A5-R06` + `S7A5-R07` | CM-K07 | **接受**：Snapshot header 字段集与 `SnapshotPayload` 闭包随 `Q-13` 冻结（处置词 `open` → `accept`） | Spec §3.18；ABI 域 9 与 §未决项 #12；[CONTRACT_MATRIX.md](CONTRACT_MATRIX.md) §10 |
| `CM-K08` / `S7A5-R08` | CM-K08 | **接受**：`SeekLatencyCommitment` = 带 measurement-profile 引用的 typed engine commitment；语义为从最近可用快照恢复并推进到目标的 **wall-clock seek latency**；会话只能**收紧**、不得放宽；测量入口为 [BUDGET_AND_EVIDENCE_PLAN.md](BUDGET_AND_EVIDENCE_PLAN.md) §5 的 restore time / Seek p95 / max 与无损性；**具体 `maxSeekLatency` 数值禁止**进入 Schema / header / 运行时约束 / 对外承诺，提供具体值稳定拒绝；seek 仍须满足无损正确性。**数值首次接受在 S7A-9** | Spec §3.19、§10 第 5 条、§S7A-6 节；ABI 域 9 与 §未决项 #16；[plan.md §S7A-6 第 5 条](../../stage_plans/active/stage-07/plan.md) |
| `CM-S10` / P1-07 / `S7A5-R09` | CM-S10 | **接受**：`Outcome` = `{hit, miss}`；Hold head / body / tail 是各自附属的 phase-local outcome / error、**不扩充** `Outcome`；`FactCategory` 与 phase 一一对应（`{tap, hold_head, hold_body, hold_tail}`）；grade table 可选、缺失即 **absent** 且**不隐式升级**；`TimingError` = `observationTick - chartTick` 的有符号整数 tick 差；seek / replay 从 Fact Ledger 重建统计；reset 清空新 session 状态且**不产生 Fact**。**处置词 `open` → `accept`** | Spec §3.20、§9.4；ABI 域 5 / 域 6；[CONTRACT_MATRIX.md](CONTRACT_MATRIX.md) §7 |
| `CM-S11` / `S7A5-R10` | —（非 CONTRACT_MATRIX 合同项） | **接受**：Life 在 7A **关闭**；ABI 仅保留 `LifeState` 类型追踪与拒绝说明；任何 Life capability / Life policy / `LifeState` 初值或占位默认值一律以 **`capability.disabled` 稳定拒绝**，**不创建、不更新** `LifeState`。**`S7A5-R10` 只用于 `LifeState`** | Spec §3.21、§S7A-5 节；ABI 域 6 `LifeState` |
| P1-14 / `S7A5-R11` | CM-V08、CM-I02 | **接受**：`P1-14` 的规则与**具体归属矩阵**作为 [Spec §3.8.6](../../formats/GAMEPLAY_V2_SPEC.md) **附录**冻结——判定必需引用进 **REF0 / judgement closure**，纯表现引用进 **manifest / presentation closure**，**source map 仅诊断**；归属由 prepare 判定、跨类即拒绝。**首次消费是 S7A-3 的 REF0 首次 Packed 写入**，entry / manifest 集成属 **S7A-7**；**不阻塞 S7A-5 / S7A-6**；ABI 侧只有指向性引用、**不新增类型行**。**与 `S7A5-R10` 分开登记、不并入 R10** | Spec §3.8.6；ABI §未决项 的 `P1-14` 指向段；[plan.md §S7A-3](../../stage_plans/active/stage-07/plan.md) |

**计数变化（第 5 轮）。** `CM-F06`、`CM-S08`、`CM-S10`、`CM-K07` 四条由 `open` 改为 **`accept`**：
`open` 14 → **10**、`accept` 38 → **42**，`revise` 26 / `supersede` 17 / `retain` 17 / `reject` 2 不变，总数仍
**114**；§0.1 合计行的末列仍为 **21**（历史归属列）；§7.2 仍 **19 条**、§9.2 仍九类；ABI 仍 9 域 /
**126 = 96 + 30**、9 行状态格首词均不变。`CM-S02`、`CM-K01` 的处置词**保持 `revise`**、只写裁定正文；
`CM-S11`、`CM-K08` **不是** CONTRACT_MATRIX 的行，**不改变任何计数**。落点见
[CONTRACT_MATRIX.md](CONTRACT_MATRIX.md) §0、§6、§7、§10 与 §14。

**登记口径（门禁归属）。** `Q-08`、`CM-S02`、`Q-12`、`CM-F06`、`Q-17`、`CM-S08`、`CM-S10` / P1-07 与
`CM-S11` 登记为**阻塞 S7A-5** 的门禁；`Q-08`、`Q-12` / `CM-F06`、`Q-13` / `CM-K01` / `CM-K07`、`CM-K08`、
`CM-S10` / P1-07、`CM-S11` 与 `Q-17` / `CM-S08` 登记为**阻塞 S7A-6** 的门禁；`P1-14`（`S7A5-R11`）
**不阻塞 S7A-5 / S7A-6**，它阻塞 **S7A-3** 的 REF0 首次写入与 **S7A-7** 的 entry / manifest 集成；
**全部具体预算数值、Replay / Snapshot 字节布局、Event / Fact codec 与 `maxSeekLatency` 数值**登记为
**阻塞 S7A-9 或其首次序列化消费者**的后续阻塞项。第 5 轮**不新增 R 编号**，§3 的拒绝清单仍为 **19 条**
（映射见 Spec §7.2 的第 5 轮段）；`CM-S11`、`CM-K08` **不是** CONTRACT_MATRIX 的行、**不改变任何计数**。

### 4.3 第 2 轮输入半批复核登记（2026-10-03）

**provenance。** 两张续裁卡同属 Codex 话题 thread `s7a1-freeze-bindings`（与第 2 轮主裁定同 thread，
模型 `gpt-6-astra`，consult），verdict 均为 `adopt`，confidence **0.97**（6 处语义裁量）与 **0.99**（两处
死令牌与措辞对齐）；文件名时间戳为 2026-10-02T20:4xZ（UTC），按第 2、3、4、5 轮先例统一按**本地日期
2026-10-03** 登记。带日期的落地证据见
[第 2 轮输入半批实现裁定记录](../../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-2-input-implementation-rulings.md)。
**本小节只登记裁定结果，不改写 §1–§5 表中 2026-10-02 的原始证据文字**（相关行的 2026-10-02 原文保留，
只在行内附裁定指针）。

| 条目 | 涉及合同项 | 复核裁定（第 2 轮输入半批，2026-10-03） | 落点 |
| --- | --- | --- | --- |
| `AmountSpec` canonical 宽度 | CM-T13（非合同项） | **明文冻结为有符号 64 位整数**；该宽度是 S7A-2 冻结的规范表示，并与 Tick 的有符号 64 位域一致；任何无法在该域内精确表示的量化、乘法或窄化稳定拒绝；**不得**靠"复用 Tick 宽度"默示 | Spec §3.7.6 第 5 条；ABI §数值域与量程、域 2 `DomainAmount` |
| 判定顺序 | CM-T13（非合同项） | **先验 canonical 可表示性、再验声明范围与边界策略**：可表示但越界 → `amount_out_of_range`；根本不可表示 → `amount_narrowed`；`exactNumerator == INT64_MIN` 因 `\|INT64_MIN\|` 无有符号 64 位表示而按"不可表示"拒绝 | Spec §3.7.6 第 6 条；ABI §数值域与量程 |
| 三条原子失败映射 | CM-T10 / CM-T13 | `same_tick_collision` → `invalid_relation`、`amount_narrowed` → `budget_exceeded`、`time_reversal` → `invalid_relation`；**同一 canonical 身份在同一 Tick 再次入场属输入身份 / 次序关系非法**，与迟到策略重复排队的 `late_policy_incomplete` **分开** | Spec §3.7.8 表、§9.3 |
| 负 `observationTick` | CM-T04 / CM-T10 | 有符号 64 位域内**负值合法**并参与单调排序；**只有校准会话时钟回退被拒**，属**时间次序关系错误** → `invalid_relation`（**不是** `budget_exceeded`）；负值本身不触发任何拒绝码 | Spec §3.7.3 第 4 条、§3.7.8、§9.3；ABI §时间单位与表达 |
| clock 不可表示 | CM-T04 | 复用既有 **tick 域表示失败**（`budget_exceeded`），**不保留**独立令牌；入口违反"先捕获校准会话时钟"的顺序只走既有 `invalid_relation` 原子失败路径 | Spec §3.7.3 第 5 条 |
| 域声明进 identity | **CM-T09** | session identity 分量＝`profileId` / `profileVersion` / `sourceClass` **＋规范化后的域 token 集合与每项域声明的完整 `AmountSpec` 字段**；**域重排不改变 identity**，增删域或改其 `AmountSpec` **是** identity 变化；**7A 不引入 per-domain 版本字段**（会话级版本由 `profileVersion` 承载）；**不得**推迟到 S7A-4 | Spec §5.2 行；ABI 域 2 `InputMapping`、域 6 `NormalizationProfile` |
| 不连续表示复用既有码 | **CM-T10** | 跨 gap / 重连 / 丢样与连续输入能力**都复用**既有 ABI 冻结码 `input.continuous_unsupported`（类别 `capability_disabled`，`capabilityId = input.trajectory.v1`，`remediation = S7B-1`），靠 `field.path`（`continuityCapability` vs `discontinuity`）区分，映射既有 **R-05**；**不新增 ABI 码、不新增 R 条目** | Spec §3.7.7 表、§3.7.8、§7.2；CONTRACT_MATRIX `CM-T10` |
| 死令牌删除 | —（实现批次） | **删除**两个无引用的 7A 令牌（"calibrated clock 非法 / 负值非法"与"observationTick 缺失"）；clock 不可表示统一复用既有 tick 溢出码 | 实现批次 `src`（src-only 令牌，不进公共码表） |

**登记口径。** 上表各项**全部登记为阻塞 S7A-2 的门禁已满足后的语义补充**（不是新批次门禁）；其中
`CM-T09` 的域声明进 identity **不得**推迟到 S7A-4。**计数不变**：`CONTRACT_MATRIX` 的六类处置词与
**114** 总数不变（`CM-T09` 仍 `retain`、`CM-T10` 仍 `accept`；`CM-T13` 不是合同项、不计入 `open` 或末列）；
`RULING_WORKSHEET` §8 的 `open` 行仍 **10**、§0.1 仍 76 行 / 84 项 / 末列 21；§7.2 仍 **19 条**、§9.2 仍
**九类**；ABI 仍 9 域 / **126 = 96 + 30**、§未决项**不新增编号**。

### 4.4 第 6 轮裁定登记（2026-10-03）

**provenance。** 单张决策卡，Codex 话题 thread `01a0fe61-3476-7882-9674-5f6b05035237`（consult，模型
`gpt-6-astra`）：verdict **`adopt`**，confidence **0.97**（第 6 轮 15 行裁定，编号 `S7A6-R01…R12`；五条
`open` 合同项 `CM-P04`、`CM-P06`、`CM-P08`、`CM-D04`、`CM-X01` 转为 `accept`；**第 6 轮落地后的当时计数**
为 `open` **5** / `accept` **47**，总数仍 114——**第 7 轮（见 §4.5）再把残余五条 `CM-V*` 补齐为 `accept`，
最终 `open` 0 / `accept` 52**）。卡的文件名时间戳为 2026-10-02T20:51:47.167Z（UTC），统一按**本地日期
2026-10-03** 登记（与第 2、3、4、5 轮先例一致）。带日期的落地证据见
[第 6 轮门禁报告](../../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-6-gate-rulings.md)。**本小节只登记裁定
结果，不改写 §1–§3 表中 2026-10-02 的原始证据文字**（`Q-06` / `Q-09` / `Q-15` / `Q-19` 四行只在"必须
决定的批次"列追加裁定指针，其余各行不动）。

| 编号 | 涉及合同项 | 裁定（第 6 轮，2026-10-03） | 落点 |
| --- | --- | --- | --- |
| `S7A6-R01` / Q-06 | CM-V10、CM-V11、CM-X01 | **接受**：CXC 发布入口与 `entryKind` —— `playback = true` **必须显式 `entryKind`**；7A **只允许** `packed-chart` 与 `gameplay-graph`；`author-source` **不是** Playback entry（仅审查 / 迁移 / 复现）；**Ruleset package 不是第三种 entry**（与第 5 轮 `S7A5-R05` 的内置 Ruleset 决定一致）；manifest 必须记录 **entry kind、compiled semantic identity、artifact identity、Ruleset binding、capability closure、resource / presentation closure 与 `sourceOf`** | Spec §3.6、§3.23、§S7A-7 节；ABI 域 8；[RULING_WORKSHEET.md](RULING_WORKSHEET.md) §6 |
| `S7A6-R02` / Q-19 + CM-X01 | CM-X01、CM-V10 | **接受**：**`compiledSemanticIdentity` 与完整 capability closure 必须同时携带且可比较**；`CapabilityRecord` 七字段与**逐字段 identity 归属**冻结 —— `semanticKind` / `requiredFormat` / `replayImpact` / `supportedDomains` 进 **semantic projection**；`staticBudget` / `snapshotCost` 进 **closure 与 interchange compatibility**（**不进** judgement semantic hash）；`stableRejectCode` **只进诊断码表 / closure**、**不进 identity** | Spec §3.6、§5.6、§6.4、§9.6、§S7A-7 节；ABI 域 7 / 域 8 |
| `S7A6-R03` / Q-09 + CM-P04 | CM-P04、CM-P08 | **接受**：表现桥接命名层 **`GameplayOverride`**，precedence 常量 **`Initial < Behavior < Animation < GameplayOverride/FactBinding < StudioPreviewOverride`**；**`render.visible = false` 是该层的显式值**；adapter **只消费已提交 FactBinding**；**当前有效立即应用、未来 `effectivePresentationTick` 排队**；`(factId, targetId)` 去重且重复幂等；**seek / replay / reload 从 Fact Ledger 重建 token**；声明 lifetime 到期或 reset / session replacement 时终止；**表现失败绝不回写 Fact Ledger** | Spec §3.23、§S7A-7 节；ABI 域 8 |
| `S7A6-R04` / Q-15 + CM-P06 · P1-13 | CM-P06 | **接受**：Gameplay graph 是**必需判定闭包**、Presentation / Effect 是**可选只读投影**；**缺 Presentation 允许 headless judgement**；**判定闭包内 target / domain / action / table 缺失 = prepare 原子失败**（`identity_closure_incomplete` 或 `invalid_relation`）；**纯表现 target 缺失**允许**空绑定**、**丢弃该投影事件**并给稳定 **`presentation-target-missing`** 诊断，**不 fault session**、**不改 Fact Ledger** | Spec §3.23、§9.7、§S7A-7 节；ABI 域 8 |
| `S7A6-R05` / CM-P08 · P1-11 | CM-P08 | **接受**：去重键 **`(factId, targetId)`**；`any` / `all` / `groupCommit` **均幂等**；`groupCommit` **部分提交不回滚**，记稳定 **`partial-group`** 诊断并**进入稳定拒绝路径**；**correction 仅能影响未提交窗口** | Spec §3.23、§9.7、§S7A-7 节；ABI 域 8 |
| `S7A6-R06` / CM-D04 · P1-09 | CM-D04 | **接受**：四层诊断模型（§9.1）+ 九类 category（§9.2）+ `faulted` 行为（§9.4）；**集中码表**落 **`schemas/cuexis.gameplay-diagnostics.v2.codes.json`**，并新增一个 **CTest 校验项**（code 唯一性、category / severity / faulted 映射完整性、`stableRejectCode` 已登记、**输入 / 几何三码不可重复扩展**）；**除已冻结的 `input.continuous_unsupported`、`input.direction_unsupported`、`geometry.inference_rejected` 及既有 R-09 映射外，诊断码字符串须经集中码表登记与 CTest 校验后才能进入公共 ABI** | Spec §9.1、§9.2、§9.6、§S7A-7 节；ABI 域 8、§错误与诊断映射 |
| `S7A6-R07` / P1-08 | CM-T04 | **接受**：`observationTime` 拆为 **`observationTick` + 原始时间戳**；**`observationTick` 仍唯一来自校准会话时钟**；`error` 为**有符号整数 tick 差**；`eventSequence` 为**会话内单调序号**；**不做隐式单位换算** | Spec §3.24、§S7A-7 节；ABI 域 1 / 域 2 / 域 5 |
| `S7A6-R08` / P2-04 | CM-S10 | **接受**：`phase` / `category` 为**分类层**（`FactCategory` 与 `{tap, hold_head, hold_body, hold_tail}` 一一对应）；`outcome` **仅** `{hit, miss}`；`grade` **可选**、缺失即 **absent**；`localOutcome` **仅**用于 Hold 段分解；`reasonCode` **仅**解释拒绝 / 降级；**同层互斥、跨层可组合** | Spec §3.25、§S7A-7 节；ABI 域 5 |
| `S7A6-R09` / P2-05 | CM-C11 | **接受**：`action`（**输入侧动词集合**）、`requiredAction`（**requirement 侧声明**）、`domain` / `InputDomain`（**作用域绑定**）三者**独立、不互相推导** | Spec §3.25、§S7A-7 节；ABI 域 2 / 域 3 / 域 5 |
| `S7A6-R10` / P2-06 | CM-C04、CM-C09 | **接受**：`resources` 拆为 **`resourceRef`**（引用）/ **`claimPolicy`**（申请策略）/ **resource record**（全局定义），**分别归属不同层**；资源三分法**沿用 S7A-4 已冻结的 `free` / `held` / `terminal` 子集** | Spec §3.25、§3.10、§S7A-7 节；ABI 域 4 |
| `S7A6-R11` / P2-08 | CM-K07 | **接受**：**只有 Fact Ledger 进 Replay / Snapshot**；`RuleEffectEvent` / `PresentationEvent` / `EffectEvent` **均为投影**；**`EffectEvent` 只是投影内部名** | Spec §3.25、§3.18、§S7A-7 节；ABI 域 9 |
| `S7A6-R12` / P2-09 | CM-L12 | **接受**：**语义字段一律"未知即拒绝"**；`extensions` **只允许命名空间化**的 inspection metadata，且**必须证明不影响判定**；输入 / 几何三码**不可重复扩展** | Spec §3.25、§9.6、§S7A-7 节；ABI 域 8 |

**编号映射（已确认，2026-10-03）。** 卡只要求"在 `RULING_WORKSHEET.md` §6 登记 `S7A6-R01…R12`"，**未给出
编号到行的逐行对应**；上表映射由"12 个编号 / 15 行"的成组关系**推导**得出（编号按 §6 表的**行出现顺序**
推进，两个编号合并同一行的情形不减号）：`S7A6-R01`=Q-06；`S7A6-R02`=Q-19 + CM-X01；`S7A6-R03`=Q-09 +
CM-P04；`S7A6-R04`=Q-15 + CM-P06 / P1-13；`S7A6-R05`=CM-P08 / P1-11；`S7A6-R06`=CM-D04 / P1-09；
`S7A6-R07`=P1-08；`S7A6-R08`=P2-04；`S7A6-R09`=P2-05；`S7A6-R10`=P2-06；`S7A6-R11`=P2-08；
`S7A6-R12`=P2-09。**该映射原属推导；第 7 轮口径确认卡（thread `01a0fe61-3476-7882-9674-5f6b05035237`，
verdict `adopt`、confidence 0.99，2026-10-03）已确认该映射成立**（见 §4.5）。全部 15 行的**首次消费批次
一律为 `S7A-7`**；`P2-06` 的资源三分法**沿用 S7A-4 已冻结的 `free` / `held` / `terminal` 子集**。

**本轮不涉及 `P2-03`**（卡未涉及该行，本轮不动；它已在第 2 轮登记；第 7 轮口径确认卡再次确认 `P2-03` **不属第 6 轮**、仍归第 2 轮）。**登记纪律**：本轮**只冻结文档合同与
门禁**，**不得**把本轮写成"Judgement 实现完成"或"Stage 7A 完成"。

**计数变化。** `RULING_WORKSHEET` §8 的 `open` 行由 **10** 降为 **5**（分布：第 1 轮 5、第 2 轮 0、
第 3 轮 0、第 4 轮 0、第 5 轮 0、第 6 轮 0）；`CONTRACT_MATRIX` 六类处置**当时**变为 `accept` **47** /
`revise` 26 / `supersede` 17 / `retain` 17 / `open` **5** / `reject` 2 = **114**；§0.1 仍
**76 行 / 84 项 / 末列 21**；§7.2 仍 **19 条**、§9.2 仍**九类**；ABI 仍 9 域 / **126 = 96 + 30**、
§未决项**不新增编号**。**`CM-V06`、`CM-V07`、`CM-V13`、`CM-I06`、`CM-X06` 五条 `open` 本轮不改处置词**
（其与 §0.1 第 33 行 / §8 的自相矛盾由**第 7 轮**单独裁定；第 7 轮已按第 1 轮裁定补齐为 `accept`，见 §4.5，故 `open` 最终为 **0**）。

### 4.5 第 7 轮裁定登记（2026-10-03）

**provenance。** 两张决策卡，均来自 Codex（consult，模型 `gpt-6-astra`）：①**主卡** thread
`01a0fe61-b7a3-7853-a380-0793fee14e14`，verdict **`adopt`**，confidence **0.96**；②**口径确认卡**
（同议题小追问：三处计数 / 编号口径）thread `01a0fe61-3476-7882-9674-5f6b05035237`，verdict **`adopt`**，
confidence **0.99**。卡的文件名时间戳为 2026-10-02T20:52:37.188Z 与 2026-10-02T21:08:34.383Z（UTC），统一按
**本地日期 2026-10-03** 登记。带日期的落地证据见
[第 7 轮收尾裁定与缺陷处置记录](../../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-7-wrapup-rulings.md)。
**本小节只登记裁定结果，不改写 §1–§3 表中 2026-10-02 的原始证据文字。**

第 7 轮共 **16 行 / 18 项**，编号 `S7A7-R01…R16`：

| 编号 | 涉及项 | 裁定（第 7 轮，2026-10-03） | 落点 / 首次消费 |
| --- | --- | --- | --- |
| `S7A7-R01` | P1-01 | **接受**：7A 只接受**静态 typed 判定域记录**；几何属 Gameplay closure；坐标系与量程在判定域内声明、不依赖宿主视口 / 分辨率 / 表现层 transform；与 Presentation transform **完全隔离**；未来动态 frame 属 7B+ 候选、7A 稳定拒绝 | Spec §3.8.7 第一组、§5.2 判定域行、`## S7A-3` 节 ⑧；**S7A-3** |
| `S7A7-R02` | P1-02 | **接受**：CXT local relation 的全局合并**在 prepare 编译期完成**；**稳定 ID = 来源文档 identity + 声明序号**；**同名不合并而是拒绝**；跨 invocation 引用必须显式；按稳定 ID 排序使**重排不改变合并结果** | Spec §3.8.7 第二组；**S7A-3** |
| `S7A7-R03` | P1-05 | **接受**：`input.trajectory.v1` 属连续输入，**7A 整体拒绝**；schema 保留为 **S7B-1** 准入项；7A 只接受离散 press / release / update / absence / step；**不新增 R 条目**（复用既有连续输入拒绝面） | Spec §3.7.7 表、§7.2；**阻塞 S7B-1** |
| `S7A7-R04` | P1-06 | **接受**：Ruleset package 的执行与分发边界随 `Q-17` 登记为 **S7C-2** 准入项；7A 遇 package 输入**稳定拒绝**；entry kind 位置保留但不解析；命中既有 **R-09** 映射 | Spec §3.16、§7.2；**阻塞 S7C-2** |
| `S7A7-R05` | P1-10 | **接受**：沿用 [BUDGET_AND_EVIDENCE_PLAN.md](BUDGET_AND_EVIDENCE_PLAN.md) 的**五类 profile**（内容 / 稳态 / 快照与 Seek / Replay 与解码 / Packed wire）；7A **只登记计数与上界、不冻结限额**；新增成本归属按"**谁产生谁计数**"；**最坏值计数入口作为 S7A-9 证据项**；**禁止在 S7A-9 前冻结任何限额** | Spec §8.5；**S7A-9** |
| `S7A7-R06` | P1-15 | **接受**：等价 = prepare 后 **canonical graph 与 derived capability closure 逐字段相等**；**忽略 `sourceMap` 与物理顺序**；**逐字段 semantic diff 作为 S7A-3 的 golden 证据** | Spec §3.8.7 第三组；**S7A-3** |
| `S7A7-R07` | P2-02 | **接受**：采纳给定替换文本——"**不解析/编译 solver；runtime coordinator 只执行已准备的确定性 policy**" | Spec §3.12 第 2 条、§3.13 第 1 条；ABI 域 4；plan §S7A-4；**S7A-4** |
| `S7A7-R08` | P2-07 | **接受**：统一为三项 **`source closure` / `diagnostic map` / `content-artifact identity`**，各自独立命名与归属；**不新增 ABI 类型行**；`P1-14` 的归属**仍以 Spec §3.8.6 为准** | Spec §5.1.1、§5.2 `sourceMap` 行、§6.5；ABI 域 7；**S7A-3** |
| `S7A7-R09` | D-2 / D-7（合并行，2 项） | **接受**：以实现与 ADR 0042 为准（`open/play/pause/seek/reload/quit`；实现另有 `tick` 运行时步骤），订正计划措辞；**不改 ADR 0042、不改实现** | plan §1.2 表 / §S7A-8 验证表；文档订正 |
| `S7A7-R10` | D-5 | **接受**：规定"**后续审查文档必须使用 ADR / 格式编号引用**"（描述性指代不算可机械核对的引用） | 本表 D-5 行；文档订正 |
| `S7A7-R11` | D-6 | **接受**：`tools/check_stage6_a2.py` 的**实际行号为 `:259`**（历史报告写 `:252`）；**在引用处加订正注记，不改写历史报告正文** | 本表 D-6 行、本轮报告；文档订正 |
| `S7A7-R12` | D-9 | **接受（owner-only）**：版本门禁 shell 守卫须覆盖 WSL 启动器 `bash.exe`（视图一致性探针 + 显式拒绝 `System32\bash.exe` + 独立负例）；候选补丁**已在工作区、未提交**；剩余动作是**按 ADR 0042 具名复核后落到 `master`**，再于 S7A-8 消费；**在落 `master` 前不得宣称 S7A-8 版本门禁闭合** | 本轮报告 D-9 段；**S7A-8** |
| `S7A7-R13` | D-11 | **接受**：稳定 C ABI 的阶段归属以 **Stage 14** 为准；`AGENTS.md` 第 21、260 行写 Stage 12 是**孤例**，由 owner 择时订正；**本轮不改 `AGENTS.md`** | CURRENT_STATUS / ROADMAP / ADR 0043 / 本轮报告；文档订正 |
| `S7A7-R14` | D-12 | **接受（待 owner 确认）**：Gameplay I 被标 `superseded` 后，研究稿 8 处指向行应改指 ADR 0044 / V2 Spec / V2 ABI 并注明 I 已 `superseded`，**不得改写论证正文**；**须先取得 owner 对历史稿修改的确认**；**本轮只登记、不编辑** | 本表 D-12 行、本轮报告；文档订正 |
| `S7A7-R15` | D-13（含 `AGENTS.md`:110 附带子项，2 项） | **接受**：保留候选处置 (a)；补齐"已处置"的最终证据指针——三个 spike 文件 `clang-format -i` 已做（`cuexis_format_check` **156 条违规 → 0**，exit 1 → 0，改动在工作区、未提交），`AGENTS.md`:110 的 glob 描述**已订正**；**无遗留实现子项** | 本表 D-13 行、本轮报告 D-13 段；只补最终证据指针 |
| `S7A7-R16` | D-14 | **接受**：保留方案 (a)（`CM-X01` 已由第 6 轮 `S7A6-R02` 绑定、首次消费 S7A-7，`CONTRACT_MATRIX` 处置词已转回 `accept`）；补齐最终证据指针；**无遗留实现子项** | 本表 D-14 行、CONTRACT_MATRIX §12 / §14、本轮报告 D-14 段；只补最终证据指针 |

**行 / 项口径（16 行 / 18 项）。** 上表为 **16 行**；按"合并行与附带子项各记 2 项"计为 **18 项**：
`D-2 / D-7`（`S7A7-R09`）是**合并行**（1 行 / 2 项），`D-13` 的 **`AGENTS.md`:110 glob 描述附带子项**
（`S7A7-R15`）是**独立处置对象**（1 行 / 2 项）。**第 7 轮 = 16 行 / 18 项**；逐轮折算
（第 1 轮 14/15、第 2 轮 6/7、第 3 轮 6/7、第 4 轮 6/6、第 5 轮 13/14、第 6 轮 15/17、第 7 轮 16/18）
合计 **76 行 / 84 项**，与 §0.1 的绑定合计逐字一致。**第 6 轮门禁报告中"76 行 / 83 项"是当时按"第 7 轮
16 行 / 17 项"误推所致，该带日期的报告正文不改写**（口径确认卡第 3 条）。

**五条 `CM-V*` 处置词补齐（不占 `S7A7-R` 编号）。** `CM-V06`、`CM-V07`、`CM-V13`、`CM-I06`、`CM-X06`
在 [CONTRACT_MATRIX.md](CONTRACT_MATRIX.md) 中长期为 `open`，与 §0.1 第 33 行 / §8 自相矛盾；第 7 轮按
**第 1 轮（2026-10-02）已完成的裁定**只补齐**处置词与裁定正文**（`open` → `accept`），
`open` 行由 5 降为 **0**；最终 **`accept` 52 / `revise` 26 / `supersede` 17 / `retain` 17 / `open` 0 /
`reject` 2 = 114**，**`open` 集合为空**；§0.1 末列**历史 21 不变**（末列是历史归属列，不随处置词变化）。
`P2-03` **不属第 6 轮**（口径确认卡确认其仍归第 2 轮）。

**计数变化（第 7 轮）。** `RULING_WORKSHEET` §8 的 `open` 行由 **5** 降为 **0**（第 1–7 轮全为 0）；
`CONTRACT_MATRIX` 六类处置为 `accept` **52** / `revise` 26 / `supersede` 17 / `retain` 17 / `open` **0** /
`reject` 2 = **114**；§0.1 仍 **76 行 / 84 项 / 末列 21**；§7.2 仍 **19 条**、§9.2 仍**九类**；ABI 仍
9 域 / **126 = 96 + 30**、§未决项**不新增编号**、类型行**不新增**。**本轮只冻结文档合同与门禁**，
**不得**写成"Judgement 实现完成"或"Stage 7A 完成"（实现批次 S7A-3…S7A-9 仍未完成）；`D-9` 为
**owner-only**、`D-12` **待 owner 确认**。

### 4.6 第 6 轮 `CM-D04` / `P1-09` 首次消费后续补齐登记（2026-10-03）

**provenance。** 单卡：Codex 话题 thread `01a0fe6b-6e57-7051-87f8-cf9588e85568`（consult，模型
`gpt-6-astra`），verdict **`adopt`**，confidence **0.96**，决策卡文件名时间戳
`2026-10-02T21:03:39.462Z`（UTC），按先例统一按**本地日期 2026-10-03** 登记。**本卡是第 6 轮
`CM-D04` / `P1-09` 首次消费的后续补齐，不是新的裁定轮次**，因此本小节**不新增轮次行、不改动任何既有
计数**。带日期的落地证据见
[第 6 轮诊断分类学后续补齐记录](../../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-diagnostics-taxonomy-rulings.md)。
**本小节只追加登记，不改写 §1–§4 表中 2026-10-02 / 2026-10-03 的既有文字与计数。**

| 条目 | 涉及项 | 裁定（第 6 轮首次消费后续补齐，2026-10-03） | 落点 / 首次消费 |
| --- | --- | --- | --- |
| 九类为唯一 category 权威 | `CM-D02` / `CM-D04` / `P1-09`（`S7A6-R06`） | 字面 `capability` **不是** category 取值；六类 category 位置里的裸 `capability` 已逐字替换为 `capability_disabled`（Spec §3.7.7 表、§7.2、§9.3、§S7A-2 节第 5 项 + ABI §S7A-2 输入半批补充 ⑤ 行，共 6 处）；ABI 码族分组词加"**不是 `category` 的取值**、任何分组词都不得作为 `category` 写出"的防误用句 | Spec §3.7.7 / §7.2 / §9.3 / §S7A-2；ABI §诊断分层 / §码族来源 / §S7A-2 输入半批补充 |
| 已登记码 category 定案 | `CM-D04` / `P1-09` | R-11 / R-13 → `invalid_relation`（确认）、**R-12 由推导的 `unknown_capability` 改正为 `invalid_relation`**、R-14 / R-15 → `capability_disabled`（确认）、`capability.revision_mismatch` → `unknown_capability`（确认）、R-17 → `ambiguous_migration`（确认）、`capability.budget_insufficient` → `budget_exceeded`（确认） | 集中码表 `codes[]` 8 条；Spec §7.2 |
| 运行期 `faulted` 专有码 | Q-08 / `CM-D04` / `P1-09`（`S7A6-R06`） | 新增并登记 **`ruleset.transaction_failed`**（`invalid_relation` / `error` / **`session_faulted`** / 首次消费 **S7A-5**）；Spec §9.4 明确**第二阶段 Ruleset state commit 失败统一使用此码**，整 Tick 不提交、旧 state 保留、可查询 `faulted`、不追加 fault Fact、不发布 RuleEffect / PresentationEvent 的行为**不变** | Spec §9.4 末段、§7.2 第 5 轮映射行、§9.6 第 5 条；ABI §错误与诊断映射；集中码表 |
| 表现码原样登记 | `S7A6-R04` / `S7A6-R05`（`Q-15` / `CM-P06` / `P1-13`、`CM-P08` / `P1-11`） | `presentation-target-missing` / `partial-group` **码串原样**；`invalid_relation` / `error` / `session_unaffected` / 首次消费 **S7A-7** | Spec §9.6 第 5 条、§9.7 / §9.8；ABI 域 8 / §错误与诊断映射；集中码表 |
| 首次消费批次补齐 | `CM-D04` / `P1-09` | 六个此前无批次声明的码统一登记首次消费 **S7A-3**：`capability.budget_insufficient`、`capability.revision_mismatch`、`capability.permanently_unsupported`、`capability.future_development`、`input.direction_unsupported`（**R-06**）、`format.gameplay_version_unsupported`（**R-17**） | 集中码表 `ownerBatch`；Spec §7.2 |
| 13 个 `judgement.s7a2.*` 令牌 | `S7A6-R06` 第 3 条 | 全部保持 **src-only、永不进入公共码表或 ABI**；码表状态词改为 **`not_registered_src_only`（明确不登记，不是待登记）** | 集中码表 `pendingRegistration` 13 条 + 顶层 `srcOnlyPolicyNote` |
| `severity` / `faulted` 取值集 | `CM-D04` | `severity` 闭集 **`info` / `warning` / `error`**（与 core `DiagnosticSeverity{Info, Warning, Error}` 对应；7A 已登记码一律 `error`）；`faulted` 闭集 **`session_unaffected` / `session_faulted`**（后者唯一承载者为 `ruleset.transaction_failed`） | Spec §9.1；ABI 域 8 `DiagnosticSeverity` / §诊断分层 |
| 计数器 | — | **一律不变**：`CONTRACT_MATRIX` **114**（`accept` 52 / `revise` 26 / `supersede` 17 / `retain` 17 / `open` 0 / `reject` 2）、§0.1 **76 行 / 84 项 / 末列 21**、Spec §7.2 **19 条**、类别**九类**、ABI **9 域 / 126 = 96 + 30**、§未决项条目数不变；**不新增 ABI 类型条目或域** | 本表 §4.5 的计数段落逐字不变 |

**本轮的表述纪律。** 本卡落地的是**文档与集中码表的分类学语义**；**不得**写成"Judgement 实现完成"或
"Stage 7A 完成"——实现批次 S7A-3…S7A-9 仍未完成。**本小节不新增计数、不改写既有计数**。


## 5. 本轮新发现的缺陷（需一并处置）

本表登记缺陷本体与本 package 的候选处置；第 1 轮经 Codex 修订后的处置见
[RULING_WORKSHEET.md](RULING_WORKSHEET.md) §0.2 与 §1。

| 编号 | 缺陷 | 证据 | 建议处置 |
| --- | --- | --- | --- |
| D-1 | `docs/CURRENT_STATUS.md` 称四项 Stage 6 交接"登记在 Stage 7 计划的 §2 与 §6" | 实测四项表在 §1.2（第 95-107 行）；§2 是总体依赖、§6 是 7B+ 分批规划 | 接受本 package 时订正 CURRENT_STATUS 的引用 |
| D-2 | "六动词命令循环"三方命名不一致 | ADR 0042：`open/play/pause/seek/reload/quit`；计划：`load/play/pause/stop/seek/reload`；实现：`open/play/pause/tick/seek/reload/quit` | 以实现与 ADR 为准，修改计划措辞（见 [STAGE6_HANDOVER_LEDGER.md](STAGE6_HANDOVER_LEDGER.md) §3） |
| D-3 | 计划引用的"W 类缺口"在仓库内无定义出处 | 计划第 641、662、718、805 行使用；全仓无定义 | 采用 [SUPPORT_AND_REJECTION_MATRIX.md](SUPPORT_AND_REJECTION_MATRIX.md) §6 的字段定义，并在 V2 Spec 附录登记 |
| D-4 | `FORMAT_BOUNDARY.md` 的 `gameplay.version = 1` 与 Preliminary Design / Amendment 的 `2` 冲突 | FORMAT_BOUNDARY §5.1/§6 vs PD §4.1、AM §2 | 在 V2 Spec 落定后把 FORMAT_BOUNDARY 该处标为过期草案 |
| D-5 | Alignment Review 通篇不引用 ADR 0042/0043 与格式文档文件名，只用描述性指代 | 全文无编号引用 | 后续审查文档必须带编号引用，否则无法机械核对权威关系 |
| D-6 | Stage 6 交付报告对 `tools/check_stage6_a2.py` 的行号引用与实际不符 | 报告写 `:252`，实际 token 在 `:259` | 在引用处加订正注记，不改写历史报告正文 |
| D-7 | 计划 §1.2 第 3 行把交接项描述为"对 load/play/pause/stop/seek/reload 的交接回归"，但该动词集在实现中不存在 | 同 D-2 | 同 D-2 |
| D-8 | 构建系统现状缺口：`cuexis_media_import_tests` 在 active 列表中但无 `cuexis_verify_target_dependencies` 调用 | 根 `CMakeLists.txt` active 段与 allowlist 段 | 登记为独立小缺陷，在 S7A-1 的 allowlist 批次内补齐（不属 Gameplay 语义） |
| D-9 | 版本门禁自检的 shell 守卫不覆盖"WSL 启动器名为 `bash.exe`"：`usable_posix_shell()` 只按名字含 `wsl` 排除 shim | S7A-0 基线实测：默认 `PATH` 下 `ctest` 失败 1 项（`cuexis_contract_version_gate`），把 Git Bash 置于 `PATH` 首位后 19/19 通过；详见 [S7A-0 基线报告](../../stage_reports/stages/stage-07/readiness/2026-10-02-s7a-0-baseline.md) §4.2 | **方案已裁决**（Codex `adopt`，confidence 0.98）：采用"视图一致性探针 + 显式拒绝 `System32\bash.exe` + 独立负例"。**候选补丁已在工作区就绪但未提交**，验证见 [D-9 候选补丁证据](../../stage_reports/stages/stage-07/readiness/2026-10-02-d9-shell-guard-candidate.md)。剩余动作属 owner：按 ADR 0042 具名复核后落到 `master`（可信基线前置），再于 S7A-8 消费 |

| D-10 | 本 package §2.1 的文档动作清单把 `CHART_V5_GAMEPLAY_AMENDMENT.md` 写成 `docs/formats/` 下的路径，但该文件唯一存在的路径是 `docs/proposals/research/gameplay-v2/CHART_V5_GAMEPLAY_AMENDMENT.md` | `README.md` §2.1 第 63 行（已订正）vs 同包 `CONTRACT_MATRIX.md` 第 231 行的正确相对路径；全仓库 `glob **/CHART_V5_GAMEPLAY_AMENDMENT.md` 仅一处命中 | 路径引用已修正；缺陷本身与 D-5 同属"引用规范"类，建议合并登记。**轮次归属随第 1 轮的 Codex 裁决确定** |
| D-11 | 稳定 C ABI 的归属阶段在仓库内不一致 | `AGENTS.md` 第 21、260 行写 **Stage 12**；`docs/ROADMAP.md` 第 49 行与 `docs/stage_plans/active/stage-07/plan.md` 第 836 行写 **Stage 14**；`docs/adr/0043-…` 第 120 行、`docs/api/GAMEPLAY_JUDGEMENT_ABI.md` 第 8 行、`docs/formats/GAMEPLAY_JUDGEMENT_SPEC.md` 第 148 行也写 Stage 14 | 以 **Stage 14** 为准（当前阶段计划 + ROADMAP + ADR 三处一致，`AGENTS.md` 为孤例），登记为文档一致性缺陷；**不改 `AGENTS.md`**（它受 `check_docs.py` 的必备/禁用片段校验约束，且不属本次冻结范围），由 owner 决定何时订正上游指引 |
| D-12 | Gameplay I 被标 `superseded` 后，研究稿里仍有把它当现行权威的指向 | `docs/proposals/research/gameplay/README.md`:8-10、`GAMEPLAY_FOLD_CALCULUS_DRAFT.md`:3、`GAMEPLAY_RULESET_DISCUSSION.md`:14/:665、`GAMEPLAY_STRESS_TEST_DRAFT.md`:16-18 仍写“以 I 收敛工作稿为准／已被其吸收” | 第 7 轮只改这些**指向行**（改指 ADR 0044 / Gameplay V2 Spec / V2 ABI 并注明 I 已 `superseded`），**不改写研究论证正文**；`FORMAT_BOUNDARY.md` 的同类指向已在 D-4 落地时同步 |
| D-13 | 格式门禁在 HEAD 曾为红：`cuexis_format_check` 失败，CI 的 `Check formatting` 步骤在 HEAD 必然失败。**已修复**（owner 2026-10-03 裁定候选处置 (a)：对这 3 个文件执行 `clang-format -i`；改动已在工作区，未提交） | 缺陷期：`cmake --build --preset debug --target cuexis_format_check` **exit 1**，共 **156** 条 clang-format 违规，全部来自 `tools/research/gameplay_fold_spike/` 的 3 个文件：`continuity.cpp`(25)、`continuity_report.cpp`(128)、`differential_report.cpp`(3)；三者均已跟踪，且登记核实时工作区与 HEAD 内容一致（`git diff --quiet HEAD -- tools/research/gameplay_fold_spike` exit 0，HEAD = `449e864`）；违规在研究切片提交 `63f25d4`、`ef0ba20`（2026-10-01）即已存在，当前 156 条由 `aabb253`、`b726ae9`（2026-10-02）与 `5dbb758`（2026-10-01）形成，**先于** Stage 7A S7A-1 批次，不是本次改动引入；旁证：本批新增的 `engine/judgement/` 与 `tests/judgement/` 下 9 个 C++ 文件 `clang-format --dry-run --Werror` 全部 exit 0，失败清单中没有 Stage 7A 新增文件。CI 确实运行该门禁：`.github/workflows/windows-msvc.yml`:82-84、`.github/workflows/windows-mingw.yml`:122-125；门禁实现见根 `CMakeLists.txt`:536-555（`add_custom_target(cuexis_format_check ...)` 对 `${CUEXIS_FORMAT_FILES}` 跑 `--dry-run --Werror`，glob 含 `tools/*.cpp`、`tools/*.hpp`）。**修复后复核（2026-10-03）**：`clang-format -i`（clang-format 22.1.3）后 `cuexis_format_check` **exit 0、0 条违规**；"仅格式改动"的决定性证据＝去除全部空白后比较：`continuity.cpp` 与 `differential_report.cpp` 与 HEAD **完全一致**，`continuity_report.cpp` 仅比 HEAD 多出恰好 **7** 处 `""` 相邻字面量拆分标记（HEAD 0 处、工作区 7 处），移除这 7 处标记后与 HEAD 完全一致——即 `BreakStringLiterals` 把超长字面量拆成相邻字面量，C++ 相邻字面量拼接后字符串值不变，**零语义变更**。**附带文档不一致（遗留子项，2026-10-03 owner 裁定修正并已执行）**：`AGENTS.md`:110 写 "Files are globbed from `app/`, `engine/`, `tests/`, and `cmake/*.hpp.in`"，遗漏 `tools/`，与实际 glob 范围不符；且 `tools/research/gameplay_fold_spike/` 无 `CMakeLists.txt`、不被任何 CMake target 编译（该目录名在 `CMakeLists.txt`、`cmake/*.cmake`、`tools/*/CMakeLists.txt` 中 0 命中，全仓仅 4 处文档引用命中）却仍被 `cuexis_format_check` 扫描 | **已处置**：owner 2026-10-03 裁定候选处置 **(a)**——对这 3 个文件执行 clang-format，并复核研究语义未被改动（复核结论：零语义变更，见左栏）；未采用 (b)/(c)，`CUEXIS_FORMAT_FILES`、CMake 与工作流均未改动；对 hosted 验证的即时阻塞已随之解除。**遗留子项（2026-10-03 owner 裁定修正并已执行）**：`AGENTS.md`:110 已改为准确描述——glob **递归**覆盖 `app/`、`engine/`、`tests/`、`tools/` 与 `cmake/*.hpp.in`，并注明该 target 仅在 `CUEXIS_BUILD_DEVELOPER_TOOLS` 打开时存在、且会扫到不被任何 CMake target 编译的文件（如 `tools/research/` 下的研究切片）。owner 只裁定订正描述，**未**裁定缩小门禁范围，`CUEXIS_FORMAT_FILES` 保持原样 |
| D-14 | 合同项 `CM-X01` 在两份台账中处置相反：一份记"已接受、无未决点"，另一份记"待冻结、未绑轮次"。**影响**：`CapabilityRecord`（[GAMEPLAY_V2_ABI.md](../../api/GAMEPLAY_V2_ABI.md) 第 237 行）的轮次归属无法确定，而该行被 [plan §S7A-1](../../stage_plans/active/stage-07/plan.md) 第 220-227 行的角色清单直接点名（第 227 行"诊断和 capability"），其登记缺陷会卡住 S7A-1"按域计数、无遗漏"的退出判据 | ① [CONTRACT_MATRIX.md](CONTRACT_MATRIX.md) 第 211 行：`CM-X01` 处置列 `accept`、未决列"无"；② [GAMEPLAY_V2_ABI.md](../../api/GAMEPLAY_V2_ABI.md) 第 237 行 `CapabilityRecord` 状态格 `待冻结（CM-X01）`；③ 同文件 `### 登记缺陷`（第 507 行起）把第 237 行 `CapabilityRecord`（`CM-X01`）列入 **(b) 类**——"给出编号，但该编号在裁决清单与合同台账的批次表中未绑定轮次"（第 524-526 行）；④ 全库检索确认 `CM-X01` 在 [RULING_WORKSHEET.md](RULING_WORKSHEET.md) 第 1-7 轮任何表中**都没有行**（缺陷登记时该文件全文 0 命中 `CM-X01`；2026-10-03 处置后已在本表第 6 轮补登记该行，命中数不再为 0）。发现来源：[FREEZE_BLOCKING_BINDINGS.md](FREEZE_BLOCKING_BINDINGS.md) §3 的 `ABI:237` 行已记录该台账差异但未给缺陷编号 | **只登记，不选处置**；候选处置（并列列出，均未选择）：(a) 以 ABI 的 `待冻结` 为准，指定 `CM-X01` 的轮次归属并修正 CONTRACT_MATRIX 的 `accept`；(b) 以 CONTRACT_MATRIX 的 `accept` 为准（即该合同项已裁定），把 ABI 第 237 行的 `待冻结（CM-X01）` 改判为已冻结并给出裁定时间与证据；(c) 登记为阻塞项，交 capability 相关批次处理。**轮次归属：第 7 轮**（与 D-11..D-13 一致）。**处置已定（2026-10-03）**：owner 已把该缺陷的裁定委派给 Codex 咨询；Codex 采纳候选处置 **(a)**——以 [GAMEPLAY_V2_ABI.md](../../api/GAMEPLAY_V2_ABI.md) 第 237 行的 `待冻结` 为准，修正 [CONTRACT_MATRIX.md](CONTRACT_MATRIX.md) 第 211 行的 `accept`（改为 `open`，未决点写明 capability registry 字段集），并把 capability-registry 裁决登记到**第 6 轮**（与 `Q-15` / `Q-19` 同批）、首次消费 **S7A-7**。provenance：thread `s7a1-freeze-bindings`，verdict `adopt`，confidence 0.96，2026-10-03。落地见 [RULING_WORKSHEET.md](RULING_WORKSHEET.md) §7B 与 [FREEZE_BLOCKING_BINDINGS.md](FREEZE_BLOCKING_BINDINGS.md) §6。 |

## 6. owner 决策清单（可直接逐条裁定）

裁定顺序建议与批次依赖一致：

```text
第一批（进入 S7A-1 前必须裁定）
  Q-01 版本层级          Q-02 Canonical Graph 归属     Q-10 identity 归属
  Q-14 capability 派生   D-1/D-3/D-4 文档缺陷处置

第二批（进入 S7A-2 前）
  Q-04 时间域与 TimebaseProfile      —— 已裁定（第 2 轮，Codex 2026-10-03，见 §2.1）

第三批（进入 S7A-3 前）
  Q-05 CXT effects       Q-07 Packed REQ0 边界        Q-18 Release/tail 决策
  —— 已裁定（第 3 轮，Codex 2026-10-03，见 §3.1；同批另含 CM-X05（由 D-3 落地关闭）
     与 CM-C10 / P1-03（修改后接受：有界循环完全展开））

第四批（进入 S7A-4 前）
  Q-03 solver/coordinator  Q-11 资源状态机             Q-16 两级阶段映射
  —— 已裁定（第 4 轮，Codex 2026-10-03，见 §4.1；同批另含 CM-C05、CM-C09（随 Q-16 / Q-11 关闭并转
     `accept`）与 P1-12（早/晚判定与 error 记录））

第五批（进入 S7A-5 前）
  Q-08 Ruleset 事务失败   Q-17 ruleset package 归属    P1-07 outcome/grade 集合
  —— 已裁定（第 5 轮，Codex 2026-10-03，见 §4.2；同批另含 CM-S02、CM-S08、CM-F06、CM-S10、CM-S11
     与 P1-14，并含 CM-K01 / CM-K07 / CM-K08）

第六批（进入 S7A-6 前）
  Q-12 因果总序算法      Q-13 Snapshot/Replay schema
  —— 已裁定（第 5 轮，Codex 2026-10-03，见 §4.2；因果总序的**语义**同时供 S7A-5 消费，物理字节布局与
     两套 header 的字节布局随本批留待 S7A-6 首次序列化消费时冻结）
  —— 注：本批的"批次名"指 S7A-6，其**轮次**是**第 5 轮**，勿与下面的第 6 轮混淆

第七批（进入 S7A-7 前）
  Q-06 CXC 入口          Q-09 FactBinding 生命周期    Q-15 发布粒度   Q-19 identity 携带
  —— 已裁定（第 6 轮，Codex 2026-10-03，thread `01a0fe61-3476-7882-9674-5f6b05035237`，单卡 `adopt`
     0.97，见 §4.4；15 行 / 17 项、编号 `S7A6-R01…R12`，并含 `CM-P04`、`CM-P06`、`CM-P08`、`CM-D04`、
     `CM-X01` 五条 `open` → `accept` 与 `P1-08`、`P2-04`…`P2-09`；五条 `open` 转 `accept` 后当时为
     `open` 5 / `accept` 47、总数仍 114。**本轮只冻结文档合同与门禁，不表示 Judgement 实现完成**）

第八批（收尾澄清与缺陷；进入 S7A-8 前）
  P1-01 判定域 typed contract   P1-02 CXT local relation 合并   P1-05 input.trajectory.v1
  P1-06 Ruleset package 边界    P1-10 预算分层与成本归属       P1-15 两路 authoring 等价性
  P2-02 solver/coordinator 措辞 P2-07 三项命名统一             D-2/D-7、D-5、D-6、D-9、D-11、D-12、D-13、D-14
  —— 已裁定（第 7 轮，Codex 2026-10-03，主卡 thread `01a0fe61-b7a3-7853-a380-0793fee14e14` `adopt` 0.96
     + 口径确认卡 `adopt` 0.99，见 §4.5；**16 行 / 18 项**、编号 `S7A7-R01…R16`；五条残余 `open`
     （`CM-V06`/`CM-V07`/`CM-V13`/`CM-I06`/`CM-X06`）按第 1 轮裁定补齐为 `accept`，**`open` 最终为 0**、
     `accept` 52 / 114）。**D-9 为 owner-only（候选补丁未提交）、D-12 待 owner 确认**；
     **本轮只冻结文档合同与门禁，不表示 Judgement 实现完成或 Stage 7A 完成**
  —— 注：第 7 轮之后**无待裁定轮次**（第 1–7 轮全部已裁定）；本文件不再产生新的 S7A-3 语义选择
  —— 追加注（2026-10-03）：第 6 轮 `CM-D04` / `P1-09` 首次消费暴露的**码表分类学冲突**已由后续补齐卡
     （thread `01a0fe6b-6e57-7051-87f8-cf9588e85568`，`adopt` 0.96）裁定并落地，见 §4.6；该卡**不是新的
     裁定轮次**，**不新增批次行、不改动本清单的任何计数**
```

对每条 Q 的裁定，请给出：接受候选处置 / 修改后接受 / 拒绝并给出替代 / 登记为阻塞项。
**登记为阻塞项时，对应批次不得启动；"以后再定"不是可接受的裁定。**

## 7. 不在本文件范围

- S7B+/S7C 的能力级未决项（每个 capability 自己的准入包，见计划 §5.1）；
- 稳定 C ABI（Stage 14）；
- Chart v5 正式发行矩阵与 40,000 语义实体 / 16 MiB Packed 门禁（Stage 8）；
- 真实设备、GPU、音频与网络输入的实测项（单列为环境限制）。

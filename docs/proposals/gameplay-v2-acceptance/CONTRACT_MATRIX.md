# S7A-0.2 合同逐项台账

状态：candidate；S7A-0 准入产出，待 owner acceptance

更新日期：2026-10-03

上级文档：[Gameplay V2 acceptance package](README.md) ·
[Stage 7A 实施计划](../../stage_plans/active/stage-07/plan.md)

文档角色：合同表征（characterization）。它把 Gameplay V2 候选与研究稿、现有 Gameplay I
历史候选逐项对齐，标出处置词、默认值、失败路径、identity 影响和未决点；它不是 Spec、
不是 ABI，也不产生实施授权。

## 0. 台账口径

引用缩写：

```text
PD    docs/proposals/research/gameplay-v2/PRELIMINARY_DESIGN.md
SK    docs/proposals/research/gameplay-v2/SEMANTIC_KERNEL.md
DD    docs/proposals/research/gameplay-v2/DESIGN_DECISIONS.md
FB    docs/proposals/research/gameplay-v2/FORMAT_BOUNDARY.md
AM    docs/proposals/research/gameplay-v2/CHART_V5_GAMEPLAY_AMENDMENT.md
IR    docs/proposals/research/gameplay-v2/INPUT_REPLAY_EXTENSIONS.md
RM    docs/proposals/research/gameplay-v2/REVIEW_AND_MIGRATION.md
AR    docs/proposals/research/gameplay-v2/CHART_V5_ALIGNMENT_REVIEW.md
ST    docs/proposals/research/gameplay-v2/GAMEPLAY_EXPRESSIVENESS_STRESS_TEST.md
I-ADR docs/adr/0043-gameplay-judgement-ruleset-convergence.md
I-SPEC docs/formats/GAMEPLAY_JUDGEMENT_SPEC.md
I-ABI docs/api/GAMEPLAY_JUDGEMENT_ABI.md
```

处置词（只允许这六个）：

| 处置 | 含义 |
| --- | --- |
| `accept` | 接受 V2 研究稿的该项提案，作为 V2 合同正文写入 |
| `revise` | 方向接受，但字段、边界或数值必须先改写，改写条件见"未决/风险"列 |
| `supersede` | 用 V2 语义替代 Gameplay I 的对应合同，旧文本改标 superseded |
| `retain` | 旧合同内容继续有效，V2 不修改它 |
| `open` | 未决；**owner 未决策前不得进入公共头或实现批次**（计划 §1.1） |
| `reject` | 明确拒绝，登记稳定拒绝路径 |

统计（本次台账，由脚本按处置列计数）：共 **114** 项；`accept` **52**、`revise` 26、`supersede` 17、
`retain` 17、`open` **0**、`reject` 2。
**`open` 集合为空**（第 7 轮已把最后 5 条 `open` 按第 1 轮裁定补齐处置词与裁定正文）。逐项清单与所属批次见 §14；
汇总见 [OPEN_QUESTIONS.md](OPEN_QUESTIONS.md)。
（2026-10-03 缺陷 D-14：`CM-X01` 经方案 (a) 由 `accept` 修正为 `open`，计数随之为 `accept` 32 / `open` 21；
`CM-S02`、`CM-K01` 的处置词仍为 `revise`，只做轮次绑定，未变处置词。
2026-10-03 第 2 轮：`CM-T03`、`CM-T04`、`CM-T08` 三条 `open` 由第 2 轮裁定并闭合，处置词改为 `accept`，
计数随之变为 `accept` 35 / `open` 18，总数仍为 114。provenance：thread `s7a1-freeze-bindings`，verdict
`adopt`，confidence 0.99；`CM-X01` 的 `open` 口径另经补充确认卡 thread `cm-x01-disposition`（`adopt`，
confidence 0.99，2026-10-03）确认**保持 `open`**、不回改计数。
2026-10-03 第 3 轮：`CM-C10` 由第 3 轮裁定（修改后接受）转为 `accept`，`CM-X05` 由第 3 轮按 D-3 修订
转为 `revise`；计数随之变为 `accept` 36 / `revise` 26 / `open` 16，总数仍为 114。provenance：thread
`s7a3-prepare-rulings`（三卡：`need_info` 0.8、`reject` 0.88、计数确认卡 `adopt` 0.99），2026-10-03；
逐条落点见 [RULING_WORKSHEET.md](RULING_WORKSHEET.md) §3 与
[第 3 轮门禁报告](../../stage_reports/stages/stage-07/2026-10-03-s7a-3-gate-rulings.md)。
2026-10-03 第 4 轮：`CM-C05`、`CM-C09` 两条 `open` 由第 4 轮裁定并闭合，处置词均由 `open` 改为
**`accept`**；计数随之变为 **`accept` 38 / `open` 14**，其余四类不变，总数仍为 114。provenance：
thread `s7a4-arbitration-rulings`（`gpt-6-astra`，consult：主裁定卡 `adopt` 0.97、处置词与计数确认卡
`adopt` 0.99），2026-10-03；逐条落点见 [RULING_WORKSHEET.md](RULING_WORKSHEET.md) §4、§8 与 §9，以及
[第 4 轮门禁报告](../../stage_reports/stages/stage-07/2026-10-03-s7a-4-gate-rulings.md)。
2026-10-03 第 5 轮：`CM-F06`、`CM-S08`、`CM-S10`、`CM-K07` 四条 `open` 由第 5 轮裁定并闭合，处置词均由
`open` 改为 **`accept`**；计数随之变为 **`accept` 42 / `open` 10**，其余四类不变，总数仍为 114。
provenance：同一 Codex 话题 thread `01a0fe49-cfe5-7933-8d03-3d0a65d34c8b`（consult，三卡：主裁定卡
`adopt` **0.95**、处置词与计数确认卡 `adopt` **0.99**、ABI 状态格首词与追踪计数追问卡 `adopt` **0.98**），
2026-10-03；逐条落点见 [RULING_WORKSHEET.md](RULING_WORKSHEET.md) §5、§8 与 §9，以及
[第 5 轮门禁报告](../../stage_reports/stages/stage-07/2026-10-03-s7a-5-6-gate-rulings.md)。**`CM-S02`、
`CM-K01` 的处置词仍为 `revise`**，本轮只把裁定正文写入其未决点文字、不改处置词；`CM-S11`、`CM-K08`
**不是**本台账的行，因此**不改变任何计数**。
2026-10-03 输入半批复核：`CM-T09`、`CM-T10` 的**未决点文字**按同 thread `s7a1-freeze-bindings` 的两张续裁卡
（verdict 均为 `adopt`，confidence **0.97** / **0.99**）更新，**处置词一律不动**（`retain` / `accept`）；
**`CM-T13` 不是本台账的行**（它是 [RULING_WORKSHEET.md](RULING_WORKSHEET.md) §7B 登记的裁决项、绑定 ABI 的
`DomainAmount` 行），其 `AmountSpec` **canonical 整数宽度明文冻结为有符号 64 位**与**域声明进 session
identity** 的正文写入 [GAMEPLAY_V2_SPEC.md](../../formats/GAMEPLAY_V2_SPEC.md) §3.7.6 / §5.2、
[GAMEPLAY_V2_ABI.md](../../api/GAMEPLAY_V2_ABI.md) §数值域与量程 / 域 2 `DomainAmount`、
[RULING_WORKSHEET.md](RULING_WORKSHEET.md) §2 与 [OPEN_QUESTIONS.md](OPEN_QUESTIONS.md) §4.3，
因此**同样不改变任何计数**。
2026-10-03 第 6 轮：`CM-P04`、`CM-P06`、`CM-P08`、`CM-D04`、`CM-X01` 五条 `open` 由第 6 轮裁定并闭合，
处置词均由 `open` 改为 **`accept`**；计数随之变为 **`accept` 47 / `open` 5**，其余四类不变，总数仍为 114。
provenance：Codex 话题 thread `01a0fe61-3476-7882-9674-5f6b05035237`（consult，单卡 verdict `adopt`、
confidence **0.97**），2026-10-03；逐条落点见 [RULING_WORKSHEET.md](RULING_WORKSHEET.md) §6、§8 与 §9，以及
[第 6 轮门禁报告](../../stage_reports/stages/stage-07/2026-10-03-s7a-6-gate-rulings.md)。**处置词仅这五条改变**：
`CM-V06`、`CM-V07`、`CM-V13`、`CM-I06`、`CM-X06` 五条 `open` 本轮不改处置词（其与 §0.1 第 33 行 /
§8 的自相矛盾由第 7 轮单独裁定）；`CM-S02`、`CM-K01` 仍为 `revise`。
2026-10-03 第 7 轮（收尾澄清与缺陷）：`CM-V06`、`CM-V07`、`CM-V13`、`CM-I06`、`CM-X06` 五条残余 `open`
**按第 1 轮（2026-10-02）已完成的裁定补齐处置词与裁定正文**，处置词均由 `open` 改为 **`accept`**；计数随之变为
**`accept` 52 / `open` 0**，其余四类不变（`revise` 26 / `supersede` 17 / `retain` 17 / `reject` 2），
总数仍为 114，**`open` 集合为空**。§0.1 末列**历史 21 不变**（末列是"由哪一轮一并关闭"的历史归属列，不随
处置词变化）。provenance：**主卡** thread `01a0fe61-b7a3-7853-a380-0793fee14e14`（consult，模型
`gpt-6-astra`，verdict `adopt`、confidence **0.96**）与**口径确认卡** thread
`01a0fe61-3476-7882-9674-5f6b05035237`（verdict `adopt`、confidence **0.99**），2026-10-03；逐条落点见
[RULING_WORKSHEET.md](RULING_WORKSHEET.md) §7、§8 与 §9，以及
[第 7 轮收尾裁定与缺陷处置记录](../../stage_reports/stages/stage-07/2026-10-03-s7a-7-wrapup-rulings.md)。
`CM-S02`、`CM-K01` 仍为 `revise`；`CM-T13`、`CM-S11`、`CM-K08` 不是本台账的行，**不改变任何计数**。

## 1. 版本与载体

| ID | 合同项 | V2 来源 | Gameplay I 现状 | 处置 | 未决 / 风险 |
| --- | --- | --- | --- | --- | --- |
| CM-V01 | Chart 外层 `version = 5` 保留 | PD §0、AM §2 | 由 Chart v5 candidate 计划持有 | `retain` | 无 |
| CM-V02 | Gameplay 语义版本固定 `gameplay.version = 2` | PD §0/§4.1、AM §2 | 无对应字段；Gameplay I 只有 Spec/ABI | `supersede` | 无 |
| CM-V03 | 不引入 `semanticRevision = 2` 作为同义替代 | PD §0、计划 §5.1 | 无 | `accept` | 无 |
| CM-V04 | 旧 `gameplay.version = 1` 只能显式离线迁移或最早入口稳定拒绝；Reader 不得按 `resources`/`factBindings` 是否存在猜版本 | PD §4.1、AR P0-01 | 无 | `accept` | 迁移器属离线工具范围（S7A-8.2） |
| CM-V05 | `FORMAT_BOUNDARY` §5.1/§6 仍写 `"gameplay": {"version": 1}`，与 PD/AM 的 `2` 冲突 | FB §5.1/§6 vs PD §4.1 | 无 | `revise` | 需在 V2 Spec 落定后把 FB 该处标为过期草案；PD 自述优先级更高（PD L11） |
| CM-V06 | Canonical Gameplay Graph 的物理归属（Chart JSON 区段 / 编译中间层 / CXC semantic entry） | FB §14 未决 1、AR P0-02 | 无 | `accept` | **已裁定（第 1 轮 2026-10-02，第 7 轮 2026-10-03 补齐处置词与裁定正文，`S7A7-R01…R16` 批次的收尾澄清）**：随 `Q-02` 裁定——**所有合法物理入口必须规范化并恢复为同一 Canonical Gameplay Graph**；Chart v5 JSON 区段是**语义视图**、CXC `gameplay-graph` 是**可选物理 entry**，二者是**可选且可互相替代**的载体，**不是并存的语义**；拒绝"物理唯一"（A2）与"只作编译中间层"（A3）两个备选方案。**已不再阻塞** Packed section 设计与 CXC entry 矩阵。落点 [GAMEPLAY_V2_SPEC.md](../../formats/GAMEPLAY_V2_SPEC.md) §3.1、§3.6 与 [ADR 0044](../../adr/0044-gameplay-v2-semantic-kernel.md) 的备选方案 A2 / A3。provenance：thread `s7a0-r1-p0b` / 第 1 轮 Codex 修订文本（`adopt` 0.97 / 0.82），第 7 轮收尾确认见 thread `01a0fe61-b7a3-7853-a380-0793fee14e14`（`adopt` **0.96**）与 `01a0fe61-3476-7882-9674-5f6b05035237`（`adopt` **0.99**），2026-10-03 |
| CM-V07 | Packed header `semanticVersion`/`candidateRevision` 与 gameplay revision 的关系 | AR P0-01、AR P0-21 | Packed v1 candidate header（Foundation R3 冻结基础 section） | `accept` | **已裁定（第 1 轮 2026-10-02，第 7 轮 2026-10-03 补齐处置词与裁定正文）**：随 `Q-01`（Codex 修订）——Packed header **显式携带 gameplay revision**；`candidateRevision` **不进入** `compiledSemanticIdentity`，但**必须进入 artifact compatibility / interchange 判定**；**不可互换的 Packed 产物不得仅凭 `compiledSemanticIdentity` 判等价**，须同时给出覆盖工具链与载体差异的**兼容矩阵**（不能只补 `candidateRevision` 一项）。在互换性矩阵细则（Spec §2.4）与 §11.1 第 1 条登记为该矩阵的条目，属后续批次。落点 [GAMEPLAY_V2_SPEC.md](../../formats/GAMEPLAY_V2_SPEC.md) §2.3、§2.4、§5.2 的 `candidateRevision` 行；[GAMEPLAY_V2_ABI.md](../../api/GAMEPLAY_V2_ABI.md) 域 7 与 §未决项 #2。provenance 同上 |
| CM-V08 | Packed `REQ0`/`CNS0` 兼容边界：新 wire revision 还是新增 phase/measure/claim section | AR P0-07 | REQ0 为 tap/point/lane 演示 profile，CNS0 `eventCount = 0` | `revise` | 旧 reader 遇新 section 是拒绝还是忽略未定；禁止静默解释成 tap |
| CM-V09 | CXT v2 只作作者层 source input，不得独立生成全局资源或 Coordinator | FB §4.2/§5.2、AM §3 | CXT v2 candidate 合同已存在 | `revise` | CXT v2 Requirement 仍有 `effects`（见 CM-L04） |
| CM-V10 | CXC v1 内允许 `packed-chart` 与独立 `gameplay-graph` 两种 Playback entry | PD §0、AM §8 | CXC v1 candidate extension 要求 `playback=true` 指向 Packed Chart | `revise` | manifest closure / 预算 / 稳定拒绝编码未定（见 CM-C09、AR P0-06） |
| CM-V11 | `cuexis.ruleset` package 是 prepare 输入，不是第三种 Playback entry | AR P0-06、I-ADR §8 | Ruleset package 复用 CXC v1 载体形状 | `accept` | 7A 是否包含 package 见 CM-S08 |
| CM-V12 | capability 使用 `capabilityId` + 单调 `revision`，语义变化必须新增 ID 或 revision | IR §4、计划 §5.1 | 仅"capability + 稳定拒绝"原则 | `accept` | 无 |
| CM-V13 | "Chart v5 是唯一聚合入口"是语义唯一还是物理入口唯一 | AR P2-1、AR C-13 | 无 | `accept` | **已裁定（第 1 轮 2026-10-02，第 7 轮 2026-10-03 补齐处置词与裁定正文）**：**独立登记**（Codex 裁决），与 `Q-02` 同批关闭并引用其裁决；采用**语义唯一、物理入口可多**——**"Chart v5 是 gameplay typed graph 的唯一聚合语义模型；JSON、CXC 可选 entry 等物理载体必须恢复同一 typed graph"**。因此 **CXC 可以有独立 `gameplay-graph` Playback entry，但不得有第二个聚合语义入口**。落点 [GAMEPLAY_V2_SPEC.md](../../formats/GAMEPLAY_V2_SPEC.md) §3.1、§3.6、§5.3；[ADR 0044](../../adr/0044-gameplay-v2-semantic-kernel.md) 备选方案 A2。provenance 同上 |

## 2. 分层与所有权

| ID | 合同项 | V2 来源 | Gameplay I 现状 | 处置 | 未决 / 风险 |
| --- | --- | --- | --- | --- | --- |
| CM-L01 | L1 Input / L2 Program / L3 Ruleset / L4 Presentation 四层 | I-ADR §1、PD §2 | 已定义 | `retain` | V2 把 L3 细化为 Coordination + Ruleset transaction |
| CM-L02 | L1 在 7A 只做离散 press/release/update 规范化；连续量重采样属 S7B-1 | 计划 §S7A-2.2/§S7A-2.5、PD §8.3 | I-SPEC §2 已含连续量重采样 | `supersede` | 7A 遇到连续轨迹/最低上报率/reconstruction 声明必须稳定拒绝 |
| CM-L03 | RequirementProgram 只产生 `Candidate` / `LocalSignal` / `LocalClose`，不直接产生 Hit/Miss | DD §2.1、SK §3、PD §4.3 | I-SPEC §5 由实例直接产生 Fact | `supersede` | 这是 V2 最大的结构性替代 |
| CM-L04 | RequirementRecord 不含 `effects`；表现统一进 FactBinding | DD §2.4、PD §4.2、AM §3 | I-SPEC §3 的 Requirement 含 `Effect`；CXT v2 有 `effects` | `supersede` | 非空 `effects` 的兼容/迁移策略未定（AR P0-05） |
| CM-L05 | CoordinationGraph 成为跨 Requirement 关系的一等载体 | DD §2.2、FB §5.3 | 靠 L3 聚合与约定 | `supersede` | 无 |
| CM-L06 | Ruleset 从"处理器列表 + Hook"改为 typed state transaction | DD §2.5、PD §7 | I-SPEC §7 的 fold + Hook ownership | `supersede` | 事务失败后语义未定（AR P0-08） |
| CM-L07 | Hook 降级为"L2 可见参数 / L3 本地状态"两类明确接口 | DD §4 | Hook 既像接口又像模块状态 | `revise` | 二分后的接口名与投影未定 |
| CM-L08 | L4 只读，不得回写判定 | I-ADR §1、PD §2 | 已定义 | `retain` | 无 |
| CM-L09 | AnimationSystem 不认识 Judgement/Chart JSON/CXT/Ruleset，只收 typed animation input | PD §10.3 | AnimationSystem 已有 typed 输入 | `accept` | precedence 常量未定（CM-P04） |
| CM-L10 | compile/prepare solver 与 runtime coordinator 边界：Packed decoder 不执行 solver，runtime 只执行已准备的确定性排序与资源检查 | AR P0-03 | I-SPEC 无 solver 概念 | `revise` | "Playback 不执行 solver"与 `greedy_v1` 的运行期作用需重新措辞 |
| CM-L11 | 运行时不得执行 IO、随机、墙钟、宿主回调或动态 Requirement 生成 | I-ADR 威胁模型、PD §1.2 | 已定义 | `retain` | 无 |
| CM-L12 | 未声明能力必须稳定拒绝，不得降级为 Tap/Hold/旧语义 | I-SPEC §11、PD §11 | 已定义 | `retain` | 拒绝码的统一见 CM-D04 |

## 3. 时间与输入

| ID | 合同项 | V2 来源 | Gameplay I 现状 | 处置 | 未决 / 风险 |
| --- | --- | --- | --- | --- | --- |
| CM-T01 | 四个时间域：`chartTick` / `observationTick` / `commitTick` / `presentationTime` | SK §1 | I-SPEC 只有单一 observation/tick 概念 | `accept` | `presentationTime` 允许浮点，只能读已提交状态 |
| CM-T02 | 推荐 `TimebaseProfile = engine.tick.us.v1`，单位由 Engine Table Registry 声明 | 计划 §S7A-2.1、PD §4.1 | 无 | `revise` | 逐字段边界（tempo/stop/负 Beat/同 Tick/offset/舍入）未冻结（AR P0-04） |
| CM-T03 | 作者 RationalBeat → 运行时整数 `judgementTick` 的 exact mapping 与 tie rule | 计划 §S7A-2.1、AR P0-04 | 无（Spec 只要求一次性量化） | `accept` | **已裁定（第 2 轮，Codex 2026-10-03）**：映射使用**精确有理数运算**、**四舍五入到最近整数、半数取偶**（负值对称）；**tempo、stop、负 Beat 使用同一规则**；同 Tick 碰撞按规范键 **`(tick, originKind, canonicalOrdinal)`** 排序，**禁止按 ingress 顺序排序**。落点 [GAMEPLAY_V2_SPEC.md](../../formats/GAMEPLAY_V2_SPEC.md) §3.7.2。provenance：thread `s7a1-freeze-bindings`，verdict `adopt`，confidence 0.99，2026-10-03。**2026-10-03 实现复核修订（同一话题）**：`stop` 语义由"比例替换"修订为**塌缩/跳变 + 精确累计函数 `F`**——`F(originBeat) = 0`、`F(endBeat) = F(startBeat) + duration`、`tick(beat) = roundHalfToEven(F(beat))`（**只舍入一次**，不得读成整数 Tick 递推）；区间内**非单射**；半开跳变方向使**镜像配置失去奇对称**，但有向端点关系与单调性不变。provenance：verdict `adopt`，confidence 0.98 / 0.99；落点 [S7A-2 实现复核报告](../../stage_reports/stages/stage-07/2026-10-03-s7a-2-implementation-rulings.md) |
| CM-T04 | `observationTick` 的来源（设备时间 / 宿主到达时间 / 音频时间 / 校准后会话时间） | SK §1、AR P0-04 | I-SPEC §2 允许 `JudgementConfig` 声明是否参与判定 | `accept` | **已裁定（第 2 轮，Codex 2026-10-03）**：`observationTick` **唯一**来自输入进入判定管线时捕获的**校准后会话时钟**；设备 / 宿主到达 / 音频 / 渲染帧时间**仅保留为诊断上下文**；**S7C-1 只能扩展 `CalibrationProfile` 参数，不得替换 canonical source**。落点 [GAMEPLAY_V2_SPEC.md](../../formats/GAMEPLAY_V2_SPEC.md) §3.7.3。provenance 同上（thread `s7a1-freeze-bindings`，`adopt`，0.99，2026-10-03） |
| CM-T05 | `NormalizedObservation` 字段集（observationId / observationTick / ingressSequence / domain / action / channel / contact / position·amount / discontinuity / sourceClass） | SK §2、PD §8.1 | I-SPEC §2 的 `InputEvent` 字段更少 | `supersede` | ABI 的 `observationTime` 需改名（AR P1-08） |
| CM-T06 | 宿主适配器必须在进入 PlaybackSession 前提供单一 `ingressSequence`；引擎不使用线程完成顺序 | SK §2、PD §8.1 | I-SPEC 有 sequence 但未规定提供者 | `accept` | 无 |
| CM-T07 | 迟到策略只接受 `reject_late` 与 `queue_next_tick`；`reopen_uncommitted_window` 保持 candidate | PD §8.2、ST F04、AR P1-04 | 无 | `accept` | 策略必须写入 session judgement identity |
| CM-T08 | `finalizationWatermark`、最大 queue hop、窗口 open/close、重复排队条件 | PD §8.2、AR P1-04 | 无 | `accept` | **已裁定（第 2 轮，Codex 2026-10-03）**：四项均为 `TimebaseProfile` / ruleset 提供的 **typed 参数**；默认值**只在 profile registry 登记为 `pending_measurement`**，不得成为 ABI 常量、隐式零值或研究切片限额；**缺失或未测量值在 prepare 稳定拒绝**；**重复排队稳定拒绝并返回诊断码**。**具体数值**（watermark / queue hop / 窗口开闭阈值）登记为**后续批次阻塞项**（不是 S7A-2 门禁）。落点 [GAMEPLAY_V2_SPEC.md](../../formats/GAMEPLAY_V2_SPEC.md) §3.7.4。provenance 同上。**2026-10-03 实现复核补充**：`validatePrepare` 只校验四项参数**已声明且已测量**，**不校验**参数之间的量级关系；量级关系校验登记为**阻塞 S7A-4 的 late-window / 判定消费门禁**，具体数值与限额继续记 **S7A-9**（落点 Spec §3.7.4 第 5 条、§3.7.7 表、[S7A-2 实现复核报告](../../stage_reports/stages/stage-07/2026-10-03-s7a-2-implementation-rulings.md)） |
| CM-T09 | InputMapping profile 属 session，不改变 Canonical Gameplay Graph | IR §1、计划 §S7A-2.3 | I-SPEC §2 把映射/校准放入 session 元数据 | `retain` | 映射的 source identity/版本/量化必须进 session identity。**2026-10-03 输入半批复核（`adopt` 0.97 / 0.99，同 thread `s7a1-freeze-bindings`；处置词不变）**：session identity 分量为 `profileId`、`profileVersion`、`sourceClass`，**加规范化后的域 token 集合与每项域声明的完整 `AmountSpec` 字段（`scale` / `minimum` / `maximum` / `boundaryPolicy`）**；**域声明顺序不承载语义**（重排不是 identity 变化），**增删任何域或改动其 `AmountSpec` 是 identity 变化**；**7A 不引入 per-domain 版本字段**——会话级版本由 `profileVersion` 承载，**不得**推迟到 S7A-4。落点 [GAMEPLAY_V2_SPEC.md](../../formats/GAMEPLAY_V2_SPEC.md) §5.2 行、§3.7.6 与 [GAMEPLAY_V2_ABI.md](../../api/GAMEPLAY_V2_ABI.md) 域 2 `InputMapping` / 域 6 `NormalizationProfile`、§数值域与量程、§S7A-2 输入半批冻结补充 |
| CM-T10 | 时间回退、负时间、重复 sequence、跨 discontinuity 的显式行为 | 计划 §S7A-2.4 | I-SPEC §11 只要求稳定失败 | `accept` | 逐项 expected 结果需在 Spec 写成状态表。**2026-10-03 输入半批复核（`adopt` 0.97 / 0.99；处置词不变）**：四类行为结论如下——①**校准会话时钟回退**被稳定拒绝，属**时间次序关系错误** → `invalid_relation`（**不是** `budget_exceeded`）；②**负 `observationTick` 合法**（有符号 64 位域内）并参与单调排序，负值本身不触发任何拒绝码；**不可表示的 calibrated clock 值**复用既有 **tick 域表示失败**（`budget_exceeded`），**不保留**独立令牌，入口顺序违规只走既有 `invalid_relation` 路径；③**重复 `ingressSequence` 与迟到队列重复 canonical 身份**映射 `late_policy_incomplete`，**与同 Tick 同 canonical 身份重入（`invalid_relation`）分开**——后者属输入身份 / 次序关系非法；④**不连续表示**（跨 gap / 重连 / 丢样）与连续输入能力**都复用**既有 ABI 冻结码 `input.continuous_unsupported`（类别 `capability_disabled`，`capabilityId = input.trajectory.v1`，`remediation = S7B-1`），靠 `field.path`（`continuityCapability` vs `discontinuity`）区分，映射既有 **R-05**；**不新增 ABI 码、不新增 R 条目**。落点 [GAMEPLAY_V2_SPEC.md](../../formats/GAMEPLAY_V2_SPEC.md) §3.7.3、§3.7.7、§3.7.8、§9.3 与 [GAMEPLAY_V2_ABI.md](../../api/GAMEPLAY_V2_ABI.md) §时间单位与表达、§S7A-2 输入半批冻结补充 |
| CM-T11 | 设备最低上报率与能力协商；不满足时拒绝或按显式策略降级 | IR §3、PD §8.3 | I-SPEC §2 只承诺同类别在最低上报率以上的格点相同 | `accept` | 7A 只涉及离散；连续能力属 S7B-1 |
| CM-T12 | 实时输入与 Replay 输入必须走同一 `submit → advance → coordinate → fold` 路径 | IR §2、PD §8.4 | I-ABI §3 已要求同一提交路径 | `retain` | 无 |

## 4. Requirement / Pattern / Measure

| ID | 合同项 | V2 来源 | Gameplay I 现状 | 处置 | 未决 / 风险 |
| --- | --- | --- | --- | --- | --- |
| CM-R01 | RequirementRecord 字段集（requirementId / anchor / judgementDomainRef / action / program / phases[] / measures[] / resources[] / resourceDecisionPolicyRef / localClosePolicy / capabilityRefs[] / sourceMapRef） | PD §4.2 | I-SPEC §3 字段集更少且含 `grip`/`grading` | `revise` | `resources` 一词同时承担 claim intent 与全局资源定义（AR P2-6） |
| CM-R02 | 7A Pattern 原语：atom、sequence、choice、bounded repeat、skip、instant、complement | 计划 §S7A-3.2 | I-SPEC §3 列表含 `sameContact`，无 `choice` 的 7A 限制说明 | `revise` | `sameContact`、连续轨迹、跨 Requirement relation 在 7A 稳定拒绝 |
| CM-R03 | 非确定匹配固定 `leftmost-first` | I-ADR §3、I-SPEC §3 | 已定义 | `retain` | 是否进入 engine identity 需在 Spec 明确 |
| CM-R04 | `sameContact`、连续轨迹、跨 Requirement relation 仅登记为后续 capability | 计划 §S7A-3.2 | `sameContact` 是 I-SPEC 的 Pattern 约束 | `revise` | 7A 拒绝码需与 `unsupported_capability` 区分 |
| CM-R05 | Measure 允许多个 phase/category 分量，逐分量 grading | I-SPEC §3、PD §4.2 | 已定义 | `retain` | 分量排序置换必须不改变规范结果 |
| CM-R06 | Release/tail 是同一 Requirement 的可选 phase，不自动生成独立 Requirement | 计划 §S7A-3.3、AM §6 | I-SPEC 的 Hold 有 head/body，无显式 tail phase | `supersede` | ST 与 AR 提示"仅在明确声明时进入"与"Release 列入最小闭环"需一项 inclusion decision（AR P2-10） |
| CM-R07 | 未声明 tail/release 语义却要求 tail 时稳定拒绝，不得按隐式 legacy profile 推断 | 计划 §S7A-3.3 | 无 | `accept` | 无 |
| CM-R08 | `preparedGrace` 解析：量化 → `allowChartGrace` 与 `[min,max]` 校验 → 缺省继承 prepare 前冻结的默认值 → sticky/observe 非法覆盖拒绝 | I-SPEC §4、I-ADR §2 | 已定义 | `revise` | 命名必须与后续 `continuityGrace` 分离；Gameplay I 用 `grace`/`holdGrace` |
| CM-R09 | 要求 tail 语义时的 hard deadline：Hold 为 `end + preparedGrace`（7A）；Slider 公式归 S7B-1 | I-SPEC §4 | 已定义（含 Slider） | `revise` | 7A 不含 Slider，公式保留为后续 capability |
| CM-R10 | `bounded repeat` 只允许 prepare-time 有限静态结构，不是运行期计数器或动态生成 | 计划 §S7A-3.2、PD §5.5 | I-SPEC 允许 bounded repeat | `revise` | 与 `boundedRelationInstance`（CM-C10）的取舍未定 |
| CM-R11 | 同一最终 grace 与 policy 即共享 judgement identity；显式/继承来源只进 content identity 与诊断 | I-ADR §6 | 已定义 | `retain` | 无 |

## 5. Coordination 与资源

| ID | 合同项 | V2 来源 | Gameplay I 现状 | 处置 | 未决 / 风险 |
| --- | --- | --- | --- | --- | --- |
| CM-C01 | CandidateRecord 字段集（candidateId / requirementId / phase / localOutcome / observationIds[] / claims[] / measure / validUntil / priorityKey / causalOriginHint） | PD §4.3、SK §3 | 无 | `accept` | 被拒绝的 Candidate 不得进入 Ruleset/Presentation/Replay |
| CM-C02 | CoordinationGraph 四类一等关系：`exclusive` / `binding` / `temporal` / `quota` | SK §4.1、PD §5.1 | 无正式载体 | `accept` | 7A 只用 `exclusive` 的 `capacity=1` 子集 |
| CM-C03 | 7A 只允许单一 `capacity=1` exclusive resource，`free → held → terminal/free` | 计划 §S7A-4.3、PD §3.1 | I-SPEC §6 的 `claim`/`observe` 与槽位复用 | `revise` | 槽位复用与"未终止归属阻挡"需写成状态表。**第 4 轮（2026-10-03，`S7A4-R05`）已给出该状态表的语义边界**：`capacity = 1` 资源使用**唯一 slot**，slot identity 进 canonical graph；终止后按资源策略进入 `free`（槽位可复用）或**永久 `terminal`**（不可复用）；`gap` / `handoff_pending` / 非零 grace 稳定拒绝。**处置词不变（仍为 `revise`）**：状态表的逐字段表示与 `terminal` 编码不在本轮冻结（见 §5 `CM-C04`、`CM-C09`） |
| CM-C04 | 完整资源状态机 `free` / `held` / `gap` / `handoff_pending` / `terminal` | PD §5.2、SK §4.3 | I-SPEC 用 `owner/leaseStart/leaseEnd` + Gap | `revise` | **已裁定（第 4 轮，2026-10-03，`S7A4-R02`/`S7A4-R05`）**：7A 冻结为 **`free` / `held` / `terminal`** 子集——`free + claim -> held`；`held + 合法 update -> held`；终止后按资源策略进入 `free` 或**永久 `terminal`**；**`gap`、`handoff_pending`、非零 grace、handoff、`capacity > 1`、owner 集合与并列 slot 一律稳定拒绝**。**不再采用** preliminary 状态表的 gap/handoff 分支（原"只启用前三态"表述已被本行取代）。落点 [GAMEPLAY_V2_SPEC.md](../../formats/GAMEPLAY_V2_SPEC.md) §3.10、§S7A-4 节与 [GAMEPLAY_V2_ABI.md](../../api/GAMEPLAY_V2_ABI.md) 域 4。provenance：thread `s7a4-arbitration-rulings`，`adopt` 0.97 / `adopt` 0.99，2026-10-03 |
| CM-C05 | 同 Tick 交易阶段顺序：计划为六步 Tick 顺序，PD 为八步 Coordination window 阶段 | 计划 §S7A-4 固定顺序、PD §5.3 | I-SPEC §5 为六步（含折叠） | `accept` | **已裁定（第 4 轮，2026-10-03，`S7A4-R04`，随 `Q-16`/`S7A4-R03`）**：写成**唯一**映射——**六步＝外层 Tick 顺序、八阶段＝第 4 步内部完整展开**（外层第 1 步激活到期 requirement；第 2 步 Release/tail 或 hard-deadline timer；第 3 步规范化并应用输入＝八阶段第 1 步 Observation 收集；第 4 步＝八阶段第 2–7 步，7A 对 gap/recovery/handoff 输入稳定拒绝；第 5 步＝八阶段第 8 步由 commit 生成 Fact 后按 causal total order 排序；第 6 步 Tick 末提交 Hook/signal）。阶段顺序与语义进 **`judgement identity` 的 engine 组件**，不进 chart/content identity；**只冻结阶段名称、顺序、映射与语义边界**。`SK §4.2` 五步变体标为**非权威推导、已被取代**。落点 [GAMEPLAY_V2_SPEC.md](../../formats/GAMEPLAY_V2_SPEC.md) §3.9 与 [GAMEPLAY_V2_ABI.md](../../api/GAMEPLAY_V2_ABI.md) 域 4 / §S7A-4 节。provenance 同上 |
| CM-C06 | `SolverProfile`（solverId / revision / algorithm / objective / tieBreak / maxCandidates / maxBranches / maxFuel / rejectIfNonUnique） | PD §5.4 | 无 | `revise` | **已裁定语义边界（第 4 轮，2026-10-03，`S7A4-R01`）**：在 S7A-4 冻结**字段语义**、**缺失 / 歧义 / 无法证明唯一时的 prepare 拒绝语义**，以及 `objective` / `tieBreak` 为**有序语义列表**；**不冻结**默认列表、具体算法预算、`K` / fuel / `max*` 数值与序列化表示（这些留给 **S7A-9** 与后续预算批次）。**处置词不变（仍为 `revise`）**：默认目标函数清单与数值预算在 S7A-9 / 后续预算批次闭合前保持未冻结。落点 [GAMEPLAY_V2_SPEC.md](../../formats/GAMEPLAY_V2_SPEC.md) §3.12；solver≠coordinator 的命名边界见同节与 [GAMEPLAY_V2_ABI.md](../../api/GAMEPLAY_V2_ABI.md) 域 4。provenance 同上 |
| CM-C07 | 7A 只接受能证明 `greedy_v1` 唯一的图；超 fuel/目标缺失/无法证明唯一 → prepare 稳定拒绝 | PD §3.1/§5.4 | 无 | `accept` | **已裁定语义边界（第 4 轮，2026-10-03，`S7A4-R01`）**：唯一性证明由 **prepare/compile solver** 负责（展开、验证、上界证明、唯一性证明并生成 prepared profile）；runtime coordinator **不解析也不编译 solver**，只执行 prepared profile 指定的确定性候选排序、资源检查与提交；名称采用 **`coordinator.policy.greedy_v1`**，**不把 runtime 行为称为 solver**。**处置词不变（仍为 `accept`）**：**proof 编码**（唯一性证明的算法与证据形式、字节表示）登记为**后续批次阻塞项**（首次消费它的后续批次），不在 S7A-4 冻结。落点 [GAMEPLAY_V2_SPEC.md](../../formats/GAMEPLAY_V2_SPEC.md) §3.12、§S7A-4 节。provenance 同上 |
| CM-C08 | `commitId` 由 Coordinator/引擎分配，宿主不能提供 | SK §5、PD §6.1 | 无 | `accept` | 分配算法未冻结（见 CM-F06） |
| CM-C09 | `resourceId` 命名空间、`capacity > 1` 的 owner 集合/slot 语义、lease/contact identity、grace=0 与 gap 到期行为、terminal 终态范围 | AR P0-11 | I-SPEC §6 部分覆盖 | `accept` | **已裁定（第 4 轮，2026-10-03，`S7A4-R05`，随 `Q-11`/`S7A4-R02`）**：8 问逐条落定——`resourceId` **只在 prepared canonical graph 的资源命名空间内**解释；`capacity = 1` 资源使用**唯一 slot**；slot identity、lease identity、最小 contact handle identity、claim identity **进入 canonical graph / prepared judgement inputs**；lease / contact 由引擎**按规范阶段分配**，`contact end` **不复用**旧 handle；**`observe` 只产生 Observation，不占用、不改变资源 owner**；同 Tick 阶段严格服从 `Q-16` 映射；`capacity > 1`（owner 集合 / 并列 slot）与 gap / `handoff_pending` / **非零 grace** 在 7A **稳定拒绝**。落点 [GAMEPLAY_V2_SPEC.md](../../formats/GAMEPLAY_V2_SPEC.md) §3.10、§S7A-4 节与 [GAMEPLAY_V2_ABI.md](../../api/GAMEPLAY_V2_ABI.md) 域 4。provenance 同上 |
| CM-C10 | `boundedRelationInstance`（count / window / resource / source path / 预算）与完全展开二选一 | PD §5.5、AR P1-03 | 无 | `accept` | **已裁定（第 3 轮，2026-10-03）**：7A 的 prepare-time 有界循环**完全展开**，展开结果**唯一决定** identity / snapshot / fact order；`boundedRelationInstance` **不进 7A 合同**，遇到时**稳定拒绝**，列为 **7B+/S7C 候选**。7A 选择**阻塞 S7A-3**；候选表示只阻塞**首次拟消费它**的后续批次。落点 [GAMEPLAY_V2_SPEC.md](../../formats/GAMEPLAY_V2_SPEC.md) §3.8、§7.2 与 [GAMEPLAY_V2_ABI.md](../../api/GAMEPLAY_V2_ABI.md) §S7A-3 节。provenance：thread `s7a3-prepare-rulings`，裁定卡 `need_info`（0.8）与计数确认卡 `adopt`（0.99），2026-10-03 |
| CM-C11 | 跨 Requirement 关系不得隐藏在 Pattern 或 claim side effect | DD §2.2、计划 §S7B-3.4 | 无 | `accept` | 需要 W 类缺口登记表（见 CM-X05） |
| CM-C12 | `observe` / `consume` / `claim` 的可见性与消费语义；固定 `fanout = 1`；`strayWhen` 的 `consumeEmpty` 与 `noCandidate` 区分；每事件逐次重算候选 | I-SPEC §6、I-ADR §4、计划 §S7A-4.4 | 已定义 | `revise` | V2 需要在 Candidate/提交模型下重申"考虑过但被拒绝"与"没有任何实例考虑过"的区别；多资源 fanout 与 `sameContact` 仍拒绝。**第 4 轮（2026-10-03，`S7A4-R05`）已裁定 `observe` 语义**：`observe` 只产生 Observation，**不占用、不改变资源 owner**；同 Tick 阶段顺序服从 `CM-C05` 的唯一映射。**处置词不变（仍为 `revise`）** |

**第 6 轮对本节的处置。** 第 6 轮的 `P2-06` 裁定把 `resources` 拆为 **`resourceRef`** / **`claimPolicy`** /
**resource record** 三项、分别归属不同层（落点 [GAMEPLAY_V2_SPEC.md](../../formats/GAMEPLAY_V2_SPEC.md)
§3.25），其**资源三分法沿用本节 `CM-C04` / `CM-C09` 已冻结的 `free` / `held` / `terminal` 子集**，
**不新增资源状态、不改 `P2-06` 以外的任何条款**。本节各行的**处置词一律不变**（`CM-C03` / `CM-C04` /
`CM-C06` / `CM-C12` 仍为 `revise`，其余照旧）；本轮**不新增合同项行**。

## 6. Fact Ledger

| ID | 合同项 | V2 来源 | Gameplay I 现状 | 处置 | 未决 / 风险 |
| --- | --- | --- | --- | --- | --- |
| CM-F01 | Fact Ledger append-only；不撤回、不覆盖、不按更高等级替换 | DD §2.3、PD §6.1 | I-SPEC §7 只要求不得改写已发 Fact | `accept` | 抵消只能追加 correction Fact |
| CM-F02 | FactRecord 字段集（factId / chartTick / commitId / phasePriority / causalOrigin / requirementId / componentIndex / emissionIndex / category / outcome / grade / measure / error / evidenceObservationIds[] / reasonCode） | PD §6.1 | I-ABI 的 `JudgementFact` 只有 5 个字段 | `supersede` | 见 CM-I07 的 ABI 映射 |
| CM-F03 | 因果总序 `(chartTick, phasePriority, originKindPriority, originId, localOrdinal, commitId, requirementId, componentIndex, emissionIndex, factId)` | PD §6.2 | I-SPEC §5/§7 与 I-ADR §4 用 `(requirementId, phase, outcome)` | `supersede` | `phasePriority` 注册表与 revision 未冻结 |
| CM-F04 | `originKindPriority` 规范常量：observation 0 / timer 1 / coordination 2 / correction 3 | PD §6.2 | 无 | `accept` | 改变必须提升 Fact semantic revision |
| CM-F05 | correction capability 在 7A 关闭，遇到该字段稳定拒绝；`correctionDepth ≤ 1` | PD §6.2 | 无 | `accept` | 无 |
| CM-F06 | `originId` / `commitId` / `factId` / `phasePriority` 的生成与排序字节合同 | AR P0-12 | 无 | `accept` | **已裁定（第 5 轮，2026-10-03，`S7A5-R01` + `S7A5-R02`，随 `Q-12` 关闭）**：唯一规范总序为 `(commitTick, originKindPriority, canonicalOrdinal)`，`canonicalOrdinal` 由引擎从 `(originScope, originOrdinal, localOrdinal, phasePriority, factKindPriority)` 派生，**不得使用 `ingressSequence` / 容器顺序 / 线程完成顺序**；`originId` 不透明、由 `(originKind, originScope, originOrdinal)` 唯一确定；`commitId` 按总序单调分配；`factId` = `(commitId, localOrdinal)` 的语义身份；`phasePriority` 是 engine phase registry 的稳定语义 rank、仅用于 `canonicalOrdinal`、**绝非评分**；timer ordinal 由 prepared timer identity 派生、seek / replay **不重新编号**；7A correction 仍由 **R-08** 拒绝；生成语义与 phase registry 进 **`FactSemanticRevision`**（属 `JudgementIdentity.engine`）。**物理字节布局、varint / endianness、`FactId` / `CommitId` 编码与 reference-evaluator byte fixture 留 S7A-6**（首次序列化消费）。处置词 `open` → **`accept`**。落点 [GAMEPLAY_V2_SPEC.md](../../formats/GAMEPLAY_V2_SPEC.md) §3.9、§3.17 与 [GAMEPLAY_V2_ABI.md](../../api/GAMEPLAY_V2_ABI.md) 域 5 / 域 7、§S7A-5 节。provenance：thread `01a0fe49-cfe5-7933-8d03-3d0a65d34c8b`，主裁定卡 `adopt` 0.95 / 计数确认卡 `adopt` 0.99，2026-10-03 |
| CM-F07 | `factId` 只作最终唯一化字段，不替代其它因果字段 | PD §6.2 | 无 | `accept` | `factId` 编码（排序字段 / 内容 hash / opaque）未定 |

## 7. Ruleset

| ID | 合同项 | V2 来源 | Gameplay I 现状 | 处置 | 未决 / 风险 |
| --- | --- | --- | --- | --- | --- |
| CM-S01 | 每个模块声明 `readSet` / `writeSet` / `reducer` / `visibility` | PD §7.1 | I-SPEC §7 只声明 Interface/fold/modules/Hook | `supersede` | 无 |
| CM-S02 | 寄存器类型 `exclusive` / `commutative_monoid` / `ledger_derived` | PD §7.1、SK §6 | I-SPEC 的"派生值用可交换可结合算子" | `revise` | **已裁定语义与冲突策略（第 5 轮，2026-10-03，`S7A5-R04`，随 `Q-08` 的三阶段一并冻结）**：`exclusive` **只能有一个声明 owner**，第二 owner 或第二写入**稳定失败**；`commutative_monoid` **只能由已声明贡献者以已声明且可交换、可结合的 combine operator 合成**，未知 operator、**重复 contribution identity**、非交换 / 非结合组合**稳定失败**；`ledger_derived` **禁止直接 StateDelta 写入**，只能从**已提交 Fact Ledger** 确定性重建；**7A 只接受这三类**，其他 kind **稳定拒绝**；Hook 的"下一 Tick 可见"不变。**处置词不变（仍为 `revise`）**：命名与冲突策略已冻结，但字段表示与编码仍由首次消费它的后续批次冻结。落点 [GAMEPLAY_V2_SPEC.md](../../formats/GAMEPLAY_V2_SPEC.md) §3.15 与 [GAMEPLAY_V2_ABI.md](../../api/GAMEPLAY_V2_ABI.md) 域 6 `RegisterKind`、§S7A-5 节。provenance 同上 |
| CM-S03 | 一个 Tick 的 StateDelta 全部校验通过才原子提交；失败保留旧 state，不提交半个 Tick | PD §7.2 | I-SPEC 未定义事务边界 | `supersede` | faulted 后的行为未定（CM-S09） |
| CM-S04 | `RuleEffectEvent` 只影响下一 Tick，必须进入 Snapshot | PD §7.3、SK §6 | I-SPEC §5 的"信号下一 Tick 可见" | `retain` | 无 |
| CM-S05 | `PresentationEvent` 只读，Ruleset 不得读取，Presentation adapter 不得写回 | PD §7.3 | I-ADR §1 的 L4 只读 | `retain` | 无 |
| CM-S06 | `programPolicy`（locked/extendable/open）与 `outcomeScope`（declared/extended），后者只在 open 下可用 | I-SPEC §7 | 已定义 | `retain` | 非法组合的拒绝路径需与 V2 诊断表对齐 |
| CM-S07 | 模块/折叠顺序冻结在 package manifest，并进入 ruleset identity | I-SPEC §7、PD §9.2 | 已定义 | `retain` | 无 |
| CM-S08 | 7A 是否包含 `cuexis.ruleset` package 支持 | 计划 §6 S7C-2、AR P1-06 | I-ADR §8 已给包形状 | `accept` | **已裁定（第 5 轮，2026-10-03，`S7A5-R05`，随 `Q-17` 关闭）**：7A **只用内置、静态注册的 Ruleset Interface / 模块**；任何 `cuexis.ruleset` package 输入命中**既有 R-09**，诊断 `ruleset.package_unsupported`，**不得读** package hash / manifest / 迁移矩阵；package 的形状 / identity / 迁移归 **S7C-2**。处置词 `open` → **`accept`**。落点 [GAMEPLAY_V2_SPEC.md](../../formats/GAMEPLAY_V2_SPEC.md) §3.16、§7.2 的 R-09 与 [GAMEPLAY_V2_ABI.md](../../api/GAMEPLAY_V2_ABI.md) 域 6、§S7A-5 节。provenance 同上 |
| CM-S09 | Ruleset 事务失败后的 session/Fact/Presentation/Snapshot/Replay 行为 | AR P0-08 | I-SPEC 未定义 | `revise` | 建议拆"Fact commit / state commit / presentation projection"三阶段 |
| CM-S10 | 7A 最小 `outcome`/`category`/`grade` 集合与错误单位、统计 reset/seek 规则 | AR P1-07 | I-SPEC/ABI 未列集合 | `accept` | **已裁定（第 5 轮，2026-10-03，`S7A5-R09`）**：Outcome = **`{hit, miss}`**；Hold head / body / tail 是各自**附属的 phase-local outcome / error**、**不能扩充 Outcome**；`FactCategory` 与 phase **一一对应**，集合 `{tap, hold_head, hold_body, hold_tail}`；grade table **可选**、缺失时 grade 为 **absent**、只报 Outcome，**绝不以 error / category / 默认表隐式升级**；`TimingError` = `observationTick - chartTick` 的**有符号整数 tick 差**；seek / replay 从 **Fact Ledger** 重建统计；reset 清空新 session 状态且**不产生 Fact**。处置词 `open` → **`accept`**。落点 [GAMEPLAY_V2_SPEC.md](../../formats/GAMEPLAY_V2_SPEC.md) §3.20、§9.4 与 [GAMEPLAY_V2_ABI.md](../../api/GAMEPLAY_V2_ABI.md) 域 5 / 域 6。provenance 同上 |

## 8. 表现桥接

| ID | 合同项 | V2 来源 | Gameplay I 现状 | 处置 | 未决 / 风险 |
| --- | --- | --- | --- | --- | --- |
| CM-P01 | `FactBinding` 字段集（bindingId / requirementRefs[] / phaseFilter / outcomeFilter / aggregation / targetRefs[] / eventTemplates[]） | PD §10.1 | 无 | `accept` | `requirementRefs`/`targetRefs` 必须是稳定 canonical ID |
| CM-P02 | `timingError = observationTick - chartTick`；< 0 early、= 0 exact、> 0 late | PD §10.2 | I-ABI 有 `error` 字段但无语义 | `supersede` | Exact 是否要求零 tick、量化误差、窗口外早击是否产生 Fact 未定（AR P1-12） |
| CM-P03 | `effectivePresentationTick` 规则：Hit/Release 用产生该 Fact 的 Observation tick；Miss/absence 用 hard deadline；groupCommit 用 CoordinationCommit tick；correction 用可见规则时间 | PD §10.2 | 无 | `accept` | 无 |
| CM-P04 | `render.visible = false` 的 gameplay override 与 Behavior/Animation/HostOverride 的 precedence 命名常量 | PD §10.3、AR P0-09 | 已有 HostOverride/OverrideToken 与 PropertyResolver | `accept` | **已裁定（第 6 轮，2026-10-03，`S7A6-R03`，随 `Q-09` 关闭）**：命名层为 **`GameplayOverride`**，precedence 常量为 **`Initial < Behavior < Animation < GameplayOverride/FactBinding < StudioPreviewOverride`**；`render.visible = false` 是该层的**显式值**；adapter **只消费已提交 FactBinding**；**当前有效立即应用、未来 `effectivePresentationTick` 排队**；`(factId, targetId)` 去重且重复幂等；**seek / replay / reload 从 Fact Ledger 重建 token**；声明 lifetime 到期或 reset / session replacement 时终止；**表现失败绝不回写 Fact Ledger**。处置词 `open` → **`accept`**。落点 [GAMEPLAY_V2_SPEC.md](../../formats/GAMEPLAY_V2_SPEC.md) §3.23、§S7A-7 节与 [GAMEPLAY_V2_ABI.md](../../api/GAMEPLAY_V2_ABI.md) 域 8。provenance：thread `01a0fe61-3476-7882-9674-5f6b05035237`，verdict `adopt`，confidence **0.97**，2026-10-03 |
| CM-P05 | FactBinding 仅当 aggregation 改变判定事实时进入 judgement identity，纯表现只进 presentation identity | AR P0-10、PD §9.2 | I-ADR §6 的 chart 分量 | `revise` | 需要唯一 identity projection 表（CM-I06）；**`CM-I06` 已按第 1 轮裁定并由第 7 轮补齐处置词，`open` → `accept`（2026-10-03），该依赖已闭合**；本行的 `revise` 处置词不变，逐字段归属以 [GAMEPLAY_V2_SPEC.md](../../formats/GAMEPLAY_V2_SPEC.md) §5.2 的 §5.6 `CapabilityRecord` 行与 judgment / presentation 归属为准 |
| CM-P06 | Presentation target 缺失策略：判定闭包内悬空必须失败；纯表现 target 缺失允许 headless 并保留诊断 | AR P1-13 | 无 | `accept` | **已裁定（第 6 轮，2026-10-03，`S7A6-R04`，随 `Q-15` 关闭）**：**判定闭包内 target / domain / action / table 缺失** = **prepare 原子失败**，类别为 **`identity_closure_incomplete`** 或 **`invalid_relation`**（§9.2 既有九类，**不新增第十类**、**不新增 R 条目**）；**纯表现 target 缺失**允许**空绑定**、**丢弃该投影事件**并给稳定 **`presentation-target-missing`** 诊断，**不 fault session**、**不改 Fact Ledger**；缺 Presentation 允许 headless judgement。处置词 `open` → **`accept`**。落点 [GAMEPLAY_V2_SPEC.md](../../formats/GAMEPLAY_V2_SPEC.md) §3.23、§9.7、§S7A-7 节与 [GAMEPLAY_V2_ABI.md](../../api/GAMEPLAY_V2_ABI.md) 域 8。provenance：thread `01a0fe61-3476-7882-9674-5f6b05035237`，verdict `adopt`，confidence **0.97**，2026-10-03 |
| CM-P07 | 表现缓存、Animation layer 临时值、HostOverride token 不进入 Snapshot；Seek 后由 Fact Ledger 重建事件 | PD §8.4/§10.3 | I-SPEC §10 未提及表现缓存 | `accept` | 无 |
| CM-P08 | `aggregation` 三值 `any` / `all` / `groupCommit` 的幂等、去重键与重建算法 | AR P1-11 | 无 | `accept` | **已裁定（第 6 轮，2026-10-03，`S7A6-R05`，随 `Q-09` 关闭）**：去重键 = **`(factId, targetId)`**；`any` / `all` / `groupCommit` **均幂等**（同键重复应用不产生第二次效果）；`groupCommit` **部分提交不回滚**，记稳定 **`partial-group`** 诊断并**进入稳定拒绝路径**；**correction 仅能影响未提交窗口**。处置词 `open` → **`accept`**。落点 [GAMEPLAY_V2_SPEC.md](../../formats/GAMEPLAY_V2_SPEC.md) §3.23、§9.7、§S7A-7 节与 [GAMEPLAY_V2_ABI.md](../../api/GAMEPLAY_V2_ABI.md) 域 8。provenance：thread `01a0fe61-3476-7882-9674-5f6b05035237`，verdict `adopt`，confidence **0.97**，2026-10-03 |

## 9. Identity

| ID | 合同项 | V2 来源 | Gameplay I 现状 | 处置 | 未决 / 风险 |
| --- | --- | --- | --- | --- | --- |
| CM-I01 | 四分量 identity：`engine` / `ruleset` / `chart` / `session` | I-ADR §6、FB §9 | 已定义 | `retain` | V2 改变 `chart` 分量的内容（编译闭包） |
| CM-I02 | 四类身份分离：source/build、semantic、artifact、judgement | FB §3.4、PD §9.2 | I-ADR 只有四分量，未分 source/artifact | `supersede` | 无 |
| CM-I03 | Requirement identity tuple `(chartEntryId, invocationId, moduleId, exportId, emissionPath, requirementLocalId)` | FB §7 | 无 | `accept` | 性质：改 Beat/参数不改 requirement identity，只改 canonical/judgement identity |
| CM-I04 | 表现资源、source 顺序、注释、Packed 压缩顺序不进入 judgement identity | FB §7/§8、PD §9.2 | I-ADR §6 已排除表现资源与默认绑定 | `accept` | 无 |
| CM-I05 | 无法证明是纯表现的自定义类型默认归入 judgement closure | FB §8 | 无 | `accept` | 保守方向，会使更多 Replay 失效 |
| CM-I06 | 唯一 identity projection 表（逐字段标注 source/semantic/artifact/judgement/presentation 归属） | AR P0-10 | 无 | `accept` | **已裁定（第 1 轮 2026-10-02，第 7 轮 2026-10-03 补齐处置词与裁定正文）**：随 `Q-10`（Codex 修订）建表——以**实际生效的 prepared value** 为准，逐字段标注 source / semantic / artifact / judgement / presentation 归属，并**分别列明 semantic identity、artifact identity 与 interchange / compatibility 判定三条**。唯一落点 [GAMEPLAY_V2_SPEC.md](../../formats/GAMEPLAY_V2_SPEC.md) §5.2，按**四分量 + 附属字段行**组织、不新增额外分栏；相关 `CapabilityRecord` 逐字段归属见 §5.6（第 6 轮）。**已不再阻塞** Packed section、FactBinding 与 TimebaseProfile 的归属。provenance 同 CM-V06 |
| CM-I07 | ABI 到 canonical model 的映射（`InputEvent.observationTime`、`JudgementFact.error`、`JudgementResult.eventSequence` 与 V2 类型的关系） | AR P1-08 | I-ABI §2 给出候选类型 | `revise` | 必须区分内部 model 与公共 typed preview |

## 10. Replay / Snapshot / Seek

| ID | 合同项 | V2 来源 | Gameplay I 现状 | 处置 | 未决 / 风险 |
| --- | --- | --- | --- | --- | --- |
| CM-K01 | Replay header：formatVersion、四分量 identity、normalizationProfileMetadata、lateEventPolicy、eventCodecId、eventCount/byteBudget | IR §2、PD §8.4、I-ABI §5 | I-ABI §5 已有四分量 + 格式版本 + 事件数/字节预算 | `revise` | 增加 lateEventPolicy 与 codec id。**已裁定字段集（第 5 轮，2026-10-03，`S7A5-R06`，随 `Q-13` / `CM-K07` 一并冻结）**：Replay header = `formatVersion`、四分量 `JudgementIdentity`、`NormalizationProfile` metadata、effective late-policy metadata、`eventCodecId`、normalized event count、encoded byte count、`ReplayDecodeBudget`；**拼写规范：类型 `EventCodecId`、字段 `eventCodecId`**（修正 ABI 第 377 行与 379 / 680 行的不一致）。**处置词不变（仍为 `revise`）**：**具体预算数值**（事件数 / 字节数 / 解码时间的上限）仍按 [GAMEPLAY_V2_ABI.md](../../api/GAMEPLAY_V2_ABI.md) §未决项 #16 留 **S7A-9**，字节布局与 codec 编码由**首次序列化消费它的 S7A-6** 冻结。落点 [GAMEPLAY_V2_SPEC.md](../../formats/GAMEPLAY_V2_SPEC.md) §3.18 与 [GAMEPLAY_V2_ABI.md](../../api/GAMEPLAY_V2_ABI.md) 域 9、§S7A-6 节。provenance 同上 |
| CM-K02 | Replay 只保存规范化 Observation，不允许直接注入 Fact；解码不执行 IO/脚本 | IR §2、PD §8.4 | I-SPEC §2 已要求 | `retain` | 无 |
| CM-K03 | 稳定错误：非法版本、identity mismatch、乱序、时间回退、截断、重复、超事件数/字节数、解码超时、未知 capability | 计划 §S7A-6.3 | I-ABI §4 列出 10 类诊断 | `revise` | 与 CM-D02 合并 |
| CM-K04 | Snapshot 全量字段集（当前 Tick、未提交 windows、Candidate 缓存、资源 lease、RequirementProgram 状态、Fact cursor、Ruleset registers、未投递 signal、采样相位、late watermark、session/fault state） | PD §8.4、SK §7 | I-SPEC §10 字段集不含 Candidate/window/watermark | `supersede` | schema、state revision 与预算未定（AR P0-13） |
| CM-K05 | Seek 从最近已提交快照推进，结果逐位等于从起点运行；快照间隔不进 identity；`maxSeekLatency` 是引擎承诺，会话只能收紧 | I-SPEC §10、I-ABI §3 | 已定义 | `retain` | 承诺数值未定（见预算计划 §10） |
| CM-K06 | faulted session 只能由显式 reset/reload 替换；恢复旧快照不得伪造未提交的 StateDelta | 计划 §S7A-5.5/§S7A-6.6 | 无 | `accept` | 与 CM-S09 联动 |
| CM-K07 | Snapshot header：四分量 identity、semantic revision、state schema revision、event/fact count、byte budget | AR P0-13 | 无 | `accept` | **已裁定（第 5 轮，2026-10-03，`S7A5-R06` + `S7A5-R07`，随 `Q-13` 关闭）**：全量 Snapshot header 字段集 = `formatVersion`、四分量 `JudgementIdentity`、`factSemanticRevision`、`stateSchemaRevision`、normalized event count、Fact count、payload byte count、typed byte-budget descriptor；`SnapshotPayload` 闭包（已提交 Fact Ledger 至 cursor 的前缀 + cursor、活动实例、Pattern 状态、Release / tail phase、exclusive resource 状态、finalization watermark、未提交窗口、`preparedGrace`、离散 sequence 状态、Hook snapshot、Fold / Score / Combo / Statistics 状态、pending signal queue、`SessionState`、fault 诊断 / 状态）与其**不得保存清单**见 Spec §3.18；**`factSemanticRevision` 进 engine identity、`stateSchemaRevision` 不进 judgement identity**；**无全量 golden 前不得引入增量 snapshot / 压缩 ledger / 跨 minor 迁移**。处置词 `open` → **`accept`**；字节布局与预算数值仍待后续批次。落点 [GAMEPLAY_V2_SPEC.md](../../formats/GAMEPLAY_V2_SPEC.md) §3.18 与 [GAMEPLAY_V2_ABI.md](../../api/GAMEPLAY_V2_ABI.md) 域 9、§S7A-6 节。provenance 同上 |

## 11. 诊断

| ID | 合同项 | V2 来源 | Gameplay I 现状 | 处置 | 未决 / 风险 |
| --- | --- | --- | --- | --- | --- |
| CM-D01 | prepare 原子失败条件（capability / 悬空引用 / 未终止展开 / solver / 资源状态机 / late policy / Ruleset 读写冲突 / 连续输入声明 / Presentation target / 迁移歧义） | PD §11 | I-ABI §4 列出 10 类 | `revise` | 合并为一张稳定 code 表 |
| CM-D02 | 诊断类别合并：V2 九类（`unknown_capability`、`capability_disabled`、`budget_exceeded`、`ambiguous_migration`、`non_terminating_source`、`non_unique_solution`、`invalid_relation`、`late_policy_incomplete`、`identity_closure_incomplete`）与 ABI 十类 | PD §11、I-ABI §4 | I-ABI §4 | `supersede` | 必须给出旧码到新码的映射，不能出现两个同义码 |
| CM-D03 | capability 支持矩阵四态：未知 / 已知未启用 / 已启用可用 / 已启用但资源或预算不足，四者诊断不同 | IR §4 | I-SPEC §11 只有"稳定拒绝" | `supersede` | 四态的查询 API 需只读 |
| CM-D04 | 诊断层级 `code` / `category` / `severity` / `source path` / `identity component` / `recoverability` / faulted 行为 | AR P1-09 | I-ABI §4 只要求"指出分量、requirement 或字段" | `accept` | **已裁定（第 6 轮，2026-10-03，`S7A6-R06`）**：采用 **§9.1 已冻结的四层诊断模型**（稳定字符串 `code` + `category` 枚举 + `severity` + `faulted` 行为）+ **§9.2 九类 category** + **§9.4 `faulted` 行为**；`source path` 与 `identity component` 是**上下文而非稳定码**；**集中码表**落 **`schemas/cuexis.gameplay-diagnostics.v2.codes.json`**，并新增一个 **CTest 校验项**（校验 code 唯一性、category / severity / faulted 映射完整性、`stableRejectCode` 已登记、**输入 / 几何三码不可重复扩展**）；**除已冻结的 `input.continuous_unsupported`、`input.direction_unsupported`、`geometry.inference_rejected` 及既有 R-09 映射外，诊断码字符串须经集中码表登记与 CTest 校验后才能进入公共 ABI**。处置词 `open` → **`accept`**。落点 [GAMEPLAY_V2_SPEC.md](../../formats/GAMEPLAY_V2_SPEC.md) §9.1、§9.2、§9.6、§S7A-7 节与 [GAMEPLAY_V2_ABI.md](../../api/GAMEPLAY_V2_ABI.md) §错误与诊断映射。provenance：thread `01a0fe61-3476-7882-9674-5f6b05035237`，verdict `adopt`，confidence **0.97**，2026-10-03 |
| CM-D05 | 诊断必须指出字段路径、requirement/identity 分量与实际计数 | 计划 §S7A-1.4 | I-ABI §4 已有方向 | `accept` | 无 |

## 12. 扩展与兼容

| ID | 合同项 | V2 来源 | Gameplay I 现状 | 处置 | 未决 / 风险 |
| --- | --- | --- | --- | --- | --- |
| CM-X01 | capability registry 声明字段：`semanticKind` / `requiredFormat` / `staticBudget` / `snapshotCost` / `replayImpact` / `supportedDomains` / `stableRejectCode` | IR §4、PD §3.2 | 计划 §5.1 的能力准入包模板 | `accept` | **已裁定（第 6 轮，2026-10-03，`S7A6-R02`，与 `Q-19` 同批；首次消费 S7A-7）**：七字段与**逐字段 identity 归属**冻结——`semanticKind`（进 **semantic projection**）、`requiredFormat`（进 **semantic projection**）、`staticBudget`（进 **closure 与 interchange compatibility**，**不进** judgement semantic hash）、`snapshotCost`（进 **closure / interchange**，**不进** semantic hash）、`replayImpact`（进 **semantic projection**）、`supportedDomains`（进 **semantic projection**）、`stableRejectCode`（**只进诊断码表 / closure**，**不进 identity**）；**`compiledSemanticIdentity` 与完整 capability closure 必须同时携带且可比较**。处置词 `open` → **`accept`**。（2026-10-03 由缺陷 D-14 方案 (a) 曾自 `accept` 修正为 `open`：以 [GAMEPLAY_V2_ABI.md](../../api/GAMEPLAY_V2_ABI.md) 第 237 行的 `待冻结` 为准；thread `s7a1-freeze-bindings`，verdict `adopt`，confidence 0.96。补充确认卡 thread `cm-x01-disposition`，verdict `adopt`，confidence 0.99，确认当时**保持 `open`**、不改计数。本轮由第 6 轮裁定转回 `accept`。）落点 [GAMEPLAY_V2_SPEC.md](../../formats/GAMEPLAY_V2_SPEC.md) §5.6、§6.4、§9.6、§S7A-7 节与 [GAMEPLAY_V2_ABI.md](../../api/GAMEPLAY_V2_ABI.md) 域 7 / 域 8。provenance：thread `01a0fe61-3476-7882-9674-5f6b05035237`，verdict `adopt`，confidence **0.97**，2026-10-03 |
| CM-X02 | 能力来源四类：engine / ruleset package / chart module / session adapter | IR §4 | 无 | `accept` | 无 |
| CM-X03 | 兼容性四层：格式 / 语义 / 判定 / 设备 | IR §5 | 无 | `accept` | 向后兼容只在对应层明确声明时成立 |
| CM-X04 | H 类政策锁定：H01-H05、H07 为能力边界不再开发；H06、H08 未来开发、现不支持 | ST §3、PD §3.3 | 计划 §11 已继承 | `accept` | 见 [SUPPORT_AND_REJECTION_MATRIX.md](SUPPORT_AND_REJECTION_MATRIX.md) |
| CM-X05 | "W 类缺口"登记表 | 计划 §6/§6.1/§10 引用 | **仓库内无定义出处** | `revise` | W 类缺口原无定义；D-3 修订后由 plan §5.3 规定登记与引用规则，[SUPPORT_AND_REJECTION_MATRIX.md](SUPPORT_AND_REJECTION_MATRIX.md) §6 定义字段。**D-3 已落地，CM-X05 关闭，不再阻塞 S7A-3**。provenance：thread `s7a3-prepare-rulings`，补答卡 `reject`（0.88，处置词裁决）与计数确认卡 `adopt`（0.99），2026-10-03 |
| CM-X06 | capability 派生来源唯一性：Packed META `requiredFeatures`、CXT `requiredExtensions`、Chart `gameplay.capabilities`、registry、Session 协商 | AR P0-14 | 无 | `accept` | **已裁定（第 1 轮 2026-10-02，第 7 轮 2026-10-03 补齐处置词与裁定正文）**：随 `Q-14`（Codex 修订）——**source 只声明最低必需 capability**，assembler 派生完整 closure，Packed header 保存排序后的 derived closure，CXC manifest 复制 artifact-required capability，Session 只报告四态、不改写 graph；**声明少于 assembler 派生 closure 时稳定失败**，**不允许** compiler 静默补齐，失败时点固定在最早可确定 closure 不足的编译 / 装配阶段。落点 [GAMEPLAY_V2_SPEC.md](../../formats/GAMEPLAY_V2_SPEC.md) §5.3、§5.4 与 [GAMEPLAY_V2_ABI.md](../../api/GAMEPLAY_V2_ABI.md) 域 7 的 capability 行。provenance 同 CM-V06 |
| CM-X07 | 扩展闭合 checklist：新 capability 必须同时有 Replay / Packed / 预算 / 拒绝 / identity 影响 | ST §6 P2、计划 §5.1 | 无 | `accept` | 无 |
| CM-X08 | 语义字段与 inspection 字段的区分（"未知字段默认拒绝" vs CXC extensions 保留未知可选 inspection metadata） | AR P2-9 | CXC v1 candidate extension | `revise` | 需在 Schema 层显式分区 |
| CM-X09 | 运行期修改 InputMapping 一律稳定拒绝 | ST F08 | 无 | `reject` | `reject` 项，不进 7A |
| CM-X10 | 全局最优 / `max_cardinality` / bounded backtracking solver 在 7A 不使用 | PD §3.2、计划 §S7A-4.5 | 无 | `reject` | 稳定拒绝；后续 capability 需独立 fuel/identity |

## 13. 旧合同处置映射

下表是 owner 接受本 package 后要执行的**最小编辑**；在此之前不得改动这三份文档的权威状态。

| 文档 | 目标处置 | 需要的最小编辑 | 保留内容 |
| --- | --- | --- | --- |
| [ADR 0043](../../adr/0043-gameplay-judgement-ruleset-convergence.md) | `superseded`（部分） | 状态行改为 `superseded by ADR 0044`，并在顶部给出被替代条目清单（§3 核心表达模型的 Effect、§4 的 Fact 排序、§5 的连续量重采样、§2 的 grace 语义拆分） | §1 四层边界、§5 整数域与量程证明、§7 预算分层原则、威胁模型 |
| [Gameplay Judgement Spec](../../formats/GAMEPLAY_JUDGEMENT_SPEC.md) | `superseded`（字段与运行语义） | 状态行改为 `superseded by Gameplay V2 Spec`；§3/§4/§5/§6/§7/§9/§10 由 V2 Spec 承接 | §1 范围与层次、§8 数值与几何、§11 能力与拒绝、§12 明确排除 |
| [Gameplay Judgement ABI](../../api/GAMEPLAY_JUDGEMENT_ABI.md) | `retained as historical` | 状态行改为 `historical-only`；保留为 typed preview 的**早期草案**与命名来源，V2 ABI 在其上重建 | §1 生命周期形态、§3 会话操作、§4 诊断方向、§6 非目标 |
| [CHART_V5_GAMEPLAY_AMENDMENT.md](../research/gameplay-v2/CHART_V5_GAMEPLAY_AMENDMENT.md) | `revise` | §2 的顶层形状需与 PD §4.1 对齐（增加 `timebaseRef`/`rulesetRef`/`solverProfiles`/`factBindings`/`capabilities`） | §4 Packed 增量方向、§5 identity/回放、§7 兼容与迁移 |
| [FORMAT_BOUNDARY.md](../research/gameplay-v2/FORMAT_BOUNDARY.md) | `revise` | §5.1/§6 的 `gameplay.version = 1` 改为 `2` 或标注为过期草案 | §7 Requirement identity、§8 四类闭包、§13 验证门 |
| [PACKED_CHART_FORMAT.md](../../formats/PACKED_CHART_FORMAT.md) | `retain` + 增补 | 不改动 §3.3 冻结预算表；V2 新增 section 需另立 candidate wire revision 并复用同一 envelope 预检 | 全文 |
| [CXC_FORMAT.md](../../formats/CXC_FORMAT.md) | `revise` | 在既有容器内增加 `entryKind = gameplay-graph | packed-chart | author-source` 与 manifest closure 字段 | ZIP32 Stored 载体形状、closure 校验方向 |
| [CXT_V2_FORMAT.md](../../formats/CXT_V2_FORMAT.md) | `revise` | 删除/降级 Requirement 内 `effects`，明确 CXT 只生成局部 Requirement 与 local relation | 模板、Prototype/Instance、Slot/Binding、有限展开 |

## 14. 统计与结论

- 本台账共 **114** 项，其中 `open` **0** 项、`revise` 26 项、`supersede` 17 项、`retain` 17 项、
  `accept` **52** 项、`reject` 2 项。**`open` 集合为空**。
- 计划 §S7A-0 的退出门禁要求"无'实施时再决定'的公共字段"。按当前状态，
  **`open` 项已全部裁定闭合（0 项），不存在"实施时再决定"的公共字段**。按最早受影响的批次列出：

| 批次 | 该批次前必须裁定的 `open` 项 |
| --- | --- |
| S7A-1 | —（`CM-V06`、`CM-V07`、`CM-V13`、`CM-I06`、`CM-X06` 已按第 1 轮（2026-10-02）裁定并由第 7 轮（2026-10-03）补齐处置词与裁定正文，`open` → `accept`；本批次门禁已满足） |
| S7A-2 | —（`CM-T03`、`CM-T04`、`CM-T08` 已由第 2 轮（Codex 2026-10-03）裁定并闭合，本批次门禁已满足） |
| S7A-3 | —（`CM-X05` 由 D-3 处置并已于 2026-10-03 关闭，不再阻塞 S7A-3；`CM-C10` 已由第 3 轮裁定并转为 `accept`；本批次门禁已满足；**仅具体 W 缺口在首次消费前须完成登记**，见 [SUPPORT_AND_REJECTION_MATRIX.md](SUPPORT_AND_REJECTION_MATRIX.md) §6） |
| S7A-4 | —（`CM-C05`、`CM-C09` 已由第 4 轮（Codex 2026-10-03，thread `s7a4-arbitration-rulings`）裁定并均转为 `accept`，本批次门禁已满足；`CM-C04` / `CM-C06` / `CM-C07` 的处置词不变，只更新未决点文字） |
| S7A-5 | —（本批次门禁已满足） |
| S7A-6 | —（本批次门禁已满足） |
| S7A-7 | —（本批次门禁已满足） |

- 计数变化（2026-10-03，缺陷 D-14）：`CM-X01` 由 `accept` 修正为 `open` 并进入 S7A-7 行，`open` 20 → 21、
  `accept` 33 → 32（依据缺陷 D-14 方案 (a)，thread `s7a1-freeze-bindings`，verdict `adopt`，confidence 0.96）。
- 计数变化（2026-10-03，第 2 轮裁定）：`CM-T03`、`CM-T04`、`CM-T08` 由 `open` 改为 `accept`（第 2 轮，Codex
  2026-10-03，thread `s7a1-freeze-bindings`，verdict `adopt`，confidence 0.99），`open` 21 → **18**、
  `accept` 32 → **35**；S7A-2 行的三个 `open` 项随之闭合（该行改为 `—`）。总数仍为 114。
- 计数变化（2026-10-03，第 3 轮裁定）：`CM-C10` 由 `open` 改为 `accept`（第 3 轮，修改后接受：prepare-time
  有界循环完全展开，`boundedRelationInstance` 遇则稳定拒绝），`CM-X05` 由 `open` 改为 `revise`（第 3 轮按
  D-3 修订：W 类缺口的字段定义在 SUPPORT §6、登记与引用规则在 plan §5.3），`open` 18 → **16**、
  `accept` 35 → **36**、`revise` 25 → **26**；§14 的 S7A-3 行随之闭合（该行改为 `—`）。总数仍为 114。
  provenance：thread `s7a3-prepare-rulings`（`need_info` 0.8、`reject` 0.88、`adopt` 0.99），2026-10-03；
  带日期落地记录见 [第 3 轮门禁报告](../../stage_reports/stages/stage-07/2026-10-03-s7a-3-gate-rulings.md)。
- 计数变化（2026-10-03，第 4 轮裁定）：`CM-C05`、`CM-C09` 由 `open` 改为 `accept`（第 4 轮：`CM-C05` 随
  `Q-16` 的唯一六步 / 八阶段映射关闭；`CM-C09` 随 `Q-11` 裁定关闭，7A 资源子集冻结为 `free` / `held` /
  `terminal`，`gap` / `handoff_pending` / 非零 grace / handoff / `capacity > 1` / owner 集合 / 并列 slot
  稳定拒绝），`open` 16 → **14**、`accept` 36 → **38**；`revise` 26 / `supersede` 17 / `retain` 17 /
  `reject` 2 不变；§14 的 S7A-4 行随之闭合（该行改为 `—`）。总数仍为 114。provenance：thread
  `s7a4-arbitration-rulings`（`gpt-6-astra`，consult：主裁定卡 `adopt` 0.97、处置词与计数确认卡 `adopt`
  0.99），2026-10-03；带日期落地记录见
  [第 4 轮门禁报告](../../stage_reports/stages/stage-07/2026-10-03-s7a-4-gate-rulings.md)。**`CM-C04`、
  `CM-C06`、`CM-C07` 的处置词不变**（`revise` / `revise` / `accept`），只在 §5 更新未决点文字。
- 计数变化（2026-10-03，第 5 轮裁定）：`CM-F06`、`CM-S08`、`CM-S10`、`CM-K07` 由 `open` 改为 `accept`
  （第 5 轮：`CM-F06` 随 `Q-12` 的事实序与身份生成裁定关闭并 `S7A5-R02` 承接；`CM-S08` 随 `Q-17` 的
  "7A 只用内置 Ruleset、package 命中 R-09"关闭；`CM-S10` 随 `P1-07` 的最小 outcome / category / grade、
  错误单位与统计 reset / seek 规则关闭；`CM-K07` 随 `Q-13` 的 Snapshot header 字段集与
  `SnapshotPayload` 闭包关闭），`open` 14 → **10**、`accept` 38 → **42**；`revise` 26 / `supersede` 17 /
  `retain` 17 / `reject` 2 不变；§14 的 S7A-5 行与 S7A-6 行随之闭合（两行均改为 `—（本批次门禁已满足）`）。
  总数仍为 114。provenance：同一 Codex 话题 thread `01a0fe49-cfe5-7933-8d03-3d0a65d34c8b`（consult，
  三卡：主裁定卡 `adopt` 0.95、处置词与计数确认卡 `adopt` 0.99、ABI 状态格首词与追踪计数追问卡
  `adopt` 0.98），2026-10-03；带日期落地记录见
  [第 5 轮门禁报告](../../stage_reports/stages/stage-07/2026-10-03-s7a-5-6-gate-rulings.md)。**`CM-S02`、
  `CM-K01` 的处置词不变**（均仍为 `revise`），只在 §7 / §10 更新未决点文字；**`CM-S11`、`CM-K08` 不是本
  台账的行**，不改变任何计数。
- 计数变化（2026-10-03，第 6 轮裁定）：`CM-P04`、`CM-P06`、`CM-P08`、`CM-D04`、`CM-X01` 由 `open` 改为
  `accept`（第 6 轮：`CM-P04` 随 `Q-09` 的 `GameplayOverride` precedence 裁定关闭；`CM-P06` 随 `Q-15`
  的判定闭包 / 纯表现 target 缺失策略关闭；`CM-P08` 随 `Q-09` 的 `aggregation` 去重键与幂等裁定关闭；
  `CM-D04` 随 `P1-09` 的四层诊断模型、九类 category 与集中码表 + CTest 校验项关闭；`CM-X01` 随 `Q-19`
  的 `CapabilityRecord` 七字段与 identity 归属裁定关闭），`open` 10 → **5**、`accept` 42 → **47**；
  `revise` 26 / `supersede` 17 / `retain` 17 / `reject` 2 不变；§14 的 S7A-7 行随之闭合（该行改为
  `—（本批次门禁已满足）`）。总数仍为 114。provenance：Codex 话题 thread
  `01a0fe61-3476-7882-9674-5f6b05035237`（consult，单卡 verdict `adopt`、confidence **0.97**），2026-10-03；
  带日期落地记录见 [第 6 轮门禁报告](../../stage_reports/stages/stage-07/2026-10-03-s7a-6-gate-rulings.md)。
  **`CM-V06`、`CM-V07`、`CM-V13`、`CM-I06`、`CM-X06` 五条 `open` 本轮不改处置词**（其与 §0.1 第 33 行 /
  §8 的自相矛盾由**第 7 轮**单独裁定，本轮不替第 7 轮改口径）；**`CM-S02`、`CM-K01` 仍为 `revise`**。
- 计数变化（2026-10-03，第 7 轮裁定）：`CM-V06`、`CM-V07`、`CM-V13`、`CM-I06`、`CM-X06` 五条由 `open` 改为
  `accept`（第 7 轮**按第 1 轮（2026-10-02）已完成的裁定补齐处置词与裁定正文**：`CM-V06` 随 `Q-02` 的物理载体
  规范化关闭；`CM-V07` 随 `Q-01` 的 `candidateRevision` 不进 `compiledSemanticIdentity`、但进 artifact
  compatibility / interchange 与兼容矩阵关闭；`CM-V13` 随 `Q-02` 采用"语义唯一、物理入口可多"关闭；
  `CM-I06` 随 `Q-10` 的唯一 identity projection 表关闭；`CM-X06` 随 `Q-14` 的 capability 派生来源唯一性与
  "声明少于派生即稳定失败"关闭），`open` 5 → **0**、`accept` 47 → **52**；
  `revise` 26 / `supersede` 17 / `retain` 17 / `reject` 2 不变；§14 的 S7A-1 行随之闭合（该行改为 `—`）。
  总数仍为 114，**`open` 集合为空**。§0.1 第 33 行的末列**历史 21 不变**（末列是"由哪一轮一并关闭"的历史
  归属列，不随处置词变化）。provenance：**主卡** thread `01a0fe61-b7a3-7853-a380-0793fee14e14`（consult，
  `gpt-6-astra`，verdict `adopt`、confidence **0.96**）与**口径确认卡** thread
  `01a0fe61-3476-7882-9674-5f6b05035237`（verdict `adopt`、confidence **0.99**），2026-10-03；带日期落地
  记录见 [第 7 轮收尾裁定与缺陷处置记录](../../stage_reports/stages/stage-07/2026-10-03-s7a-7-wrapup-rulings.md)。
  **`CM-S02`、`CM-K01` 仍为 `revise`**；`CM-T13`、`CM-S11`、`CM-K08` 不是本台账的行，不改变任何计数。
- 另：`CM-S02`（寄存器类型）、`CM-K01`（Replay header / `eventCodecId`）的处置词仍为 `revise`，
  其轮次归属（第 5 轮）由 [RULING_WORKSHEET.md](RULING_WORKSHEET.md) §5 与 §7B 绑定登记；二者**不在**本 `open` 清单内。
  第 5 轮已把它们的**裁定正文**写入未决点（`CM-S02` → `S7A5-R04`；`CM-K01` → `S7A5-R06`），但处置词保持不变。

- CM-X05（W 类缺口）是本轮发现的**计划自引用缺陷**：计划引用了仓库中不存在的定义。该缺陷已由 D-3 的
  落地处置关闭（字段定义在 [SUPPORT_AND_REJECTION_MATRIX.md](SUPPORT_AND_REJECTION_MATRIX.md) §6、登记与引用
  规则在 plan §5.3），其处置词经第 3 轮裁定为 `revise`；**不再阻塞 S7A-3**。
  本台账的 114 项计数与 `open` **0** 项清单可用脚本按处置列复核。

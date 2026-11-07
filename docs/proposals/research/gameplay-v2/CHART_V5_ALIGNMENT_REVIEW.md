# Chart v5 与 Gameplay V2 对齐审查

状态：candidate；研究审查，未接受，未实施

日期：2026-10-02

本文审查 [Chart v5 实施稿](../../../stage_plans/active/chart-format-update-for-v5/plan.md) 与
[Gameplay V2 初步设计](PRELIMINARY_DESIGN.md) 之间仍未闭合的字段、版本、执行和发布契约。
本文不修改生产 Chart v5、CXT v2、Packed Chart、CXC v1、Gameplay Judgement Spec、ABI 或代码。
除非另有说明，本文的结论均为研究建议，不是已接受的合同。
路线方向本身已经收敛，本文不再把已确认路线伪装成待选项。

## 1. 审查结论

### 1.1 Decision disposition

| 项目 | 已确认处置 | 本审查仍需细化的合同 |
| --- | --- | --- |
| Chart/Gameplay version | 外层 `version = 5`；Gameplay 固定 `gameplay.version = 2`；旧 revision 显式迁移或稳定拒绝 | Schema、Packed wire 和 Replay 接受矩阵 |
| Playback entry | CXC v1 允许独立 `gameplay-graph` Playback entry，并保留 `packed-chart` entry | manifest closure、预算、artifact/source-of 字段与诊断编码 |
| Stage 7A phases | Tap/Hold；Release/tail 是同一 Requirement 的 phase | phase、deadline、tail outcome 的逐字段合同 |
| Presentation bridge | early/exact/late/Miss 通过 FactBinding 到现有表现路径 | HostOverride/Behavior precedence、生命周期和 seek/replay 重建 |
| Exclusive resource | 仅一个 `capacity=1` resource，`free -> held -> terminal/free` | resource identity、commit window 和 fault diagnostics |
| Unsupported policy | H01-H05/H07：能力边界，不会再开发，不受支持；H06/H08：未来开发，现不支持 | 发行矩阵中的稳定 reject code |

后文的 P0/P1 条目只讨论这些已确认路线如何落成可验证合同，不重新打开上述方向选择。

Gameplay V2 的结构方向是正确的：Chart v5 作为唯一聚合入口，CXT v2 只做作者层有限生成，
局部 Requirement 与跨 Requirement Coordination 分离，Fact Ledger 与 Ruleset transaction 分离，
Presentation 通过只读绑定消费判定事实。这套结构能够覆盖绝大多数二维、离散、有限状态、节奏
驱动玩法。

当前初稿还不能进入正式实施，原因不是表达力不足，而是各层之间尚未形成闭合的交接合同。最严重的
问题集中在以下五条链路：

```text
Chart v5 version
  -> gameplay semantic revision
  -> Canonical Gameplay Graph
  -> Packed wire sections
  -> CXC playback entry

Beat / TimingMap
  -> canonical judgement time
  -> observation time
  -> late policy / commit order

Requirement candidate
  -> resource coordination
  -> Fact causal order
  -> Ruleset transaction
  -> PresentationEvent

source / semantic / artifact / judgement identity
  -> Snapshot / Replay compatibility

feature declaration
  -> derived capability closure
  -> prepare preflight
  -> stable rejection
```

如果这些交接仍由实现阶段临时决定，最可能出现的结果是：Chart v5 能解析但不能判定，Packed 能
解码但不能恢复完整 Gameplay graph，Replay 能读取但不能证明判定等价，或者 early/late 能触发
一次性隐藏却无法在 seek、reload 和动画重新采样后保持一致。

## 2. 权威关系与当前冲突

| 内容 | 当前文件给出的状态 | 审查结论 |
| --- | --- | --- |
| 外层 Chart | Chart v5，仍由 active Chart v5 计划管理 | 可以保留 |
| Gameplay semantic revision | 路线已固定为 `gameplay.version = 2`；不引入同义的 `semanticRevision = 2` | 需要在 ADR/Schema/迁移器中写清旧 revision 的显式迁移或稳定拒绝 |
| CXT | CXT v2 同时描述 Prototype、Pattern、Animation，Requirement 仍有 `effects` | 需要明确 Gameplay 与 Presentation 的 lowering 边界 |
| Packed | Foundation 只正式冻结基础 section、`REQ0`、`CNS0` 和 candidate header；`BEH0/BHD0/ANM0/FXS0` 仍未完成合同 | V2 所需图、资源、关系、solver、binding 没有 wire 归属 |
| CXC | CXC v1 允许 `packed-chart` 与独立 `gameplay-graph` Playback entry | 两类 entry 的 closure、预算、identity 和共用 prepare path 尚未形成完整矩阵 |
| Stage 7A | active 计划以 `InputEvent`、`JudgementRequirement`、`JudgementFact`、Ruleset、Replay 和 Snapshot 为交付 | ABI、Spec 与 V2 的 Candidate/Commit/FactRecord 模型还没有逐字段对齐 |
| 压测范围 | D=26、C=42、E=17、B=3、O=8；H01-H05/H07 是能力边界，H06/H08 是未来开发 | 该口径正确，但 E/B 不能被算作当前支持 |

### 2.1 必须保留的范围声明

压测中的 H 类处置应保持如下文字，不得在后续文档中弱化：

- H01-H05、H07：能力边界，不会再开发，不受支持。
- H06、H08：未来开发，现不支持。

这意味着 `extensions`、`capabilities`、CXT Pattern 或 Ruleset package 不能为这些能力预留
未定义的执行入口。未来若重新开启 H06/H08，必须另立语义、版本、安全和预算合同。

## 3. P0：实施前必须解决

下列问题会直接阻塞 Chart v5 candidate path、Packed round-trip 或 Stage 7A 实现。没有明确
决策时，不能把相关字段写入生产 Schema 或公共 ABI。

### P0-01 版本层级未闭合

当前仍需在接受文档中分层记录：

```text
Chart.version = 5
CanonicalSemanticChart.semanticVersion = 5
gameplay.version = 2
Packed.packedVersion = 1
Packed.candidateRevision = 1
capabilityId + revision
Replay formatVersion / judgement semantic version
```

这些数字的作用不同，接受矩阵仍需补齐。例如，必须明确 `format: cuexis.chart`、`version: 5`
的文件只接受 `gameplay.version = 2`；旧 `gameplay.version = 1` 只能走显式迁移或稳定拒绝。
同时要说明 Packed header 中的 `semanticVersion = 5` 是否包含 Gameplay revision，以及
`candidateRevision` 改变时是否改变 `compiledSemanticIdentity`。

**路线收口：**

```text
outerFormat        = cuexis.chart / 5
gameplaySemantic   = gameplay.version / 2
packedWire         = packedVersion / 1
packedCandidate    = candidateRevision / explicit candidate value
capability         = capabilityId + monotonic revision
judgementSemantic  = engine judgement semantic revision
replayFormat       = replay container version
```

生产文件必须显式携带或由唯一父合同推导这些层级。旧 `gameplay.version = 1` 只能通过显式
离线迁移器升为 revision 2，或在最早可判定入口稳定拒绝；禁止根据是否存在 `resources`、
`factBindings` 等字段猜版本。

### P0-02 Canonical Gameplay Graph 没有物理归属

V2 把 `requirements`、`resources`、`groups`、`relations`、`solverProfiles`、`factBindings`
和 `capabilities` 视为 Canonical Gameplay Graph，但 Chart v5 计划只明确了 Core/Packed 基础
模型，Packed 现行合同还只正式描述 `REQ0`/`CNS0` 等基础部分。

未定义的内容包括：

- Gameplay graph 是 Chart v5 JSON 的正式区段、编译中间层，还是 CXC 独立 semantic entry；
- Packed 中哪些 section 保存 Gameplay graph；
- section 的 wire revision、局部索引、引用编码和预算；
- decoder 是否只恢复图，还是允许做任何 solver/Pattern lowering；
- graph 缺少 Presentation graph 时是否仍可作为 Stage 7A playback 输入。

**路线收口：** Stage 7A 以 Canonical Gameplay Graph 为唯一播放语义，Chart v5 与 Packed 都是
它的输入/物理投影。CXC v1 可承载独立 `gameplay-graph` Playback entry；它与 `packed-chart`
entry 必须恢复同一 typed graph、prepared identity 和 Judgement kernel，不形成第二种判定 contract。

Packed 新增 section 可以另立 candidate wire revision；不能扩大旧 `REQ0` 的含义来表达 Hold、
Release、资源 claim 或关系，否则旧 Reader 会把新语义误读为旧 tap profile。具体 section code
应在 Packed amendment 中逐项列出，至少覆盖 graph metadata、resource、relation/group、solver
profile、binding 和 capability closure。

### P0-03 compile-time solver 与 runtime coordinator 的边界矛盾

文档一方面规定 Playback 不执行 solver，另一方面又规定运行期先收集 Candidate，再进行
Coordination transaction，并把 `greedy_v1` 列为 Stage 7A 能力。Packed decoder 不执行 solver
是清楚的，但“Playback 不执行 solver”与“runtime coordinator 求解”目前冲突。

**建议收口：** 把两个概念拆开：

1. **compile/prepare solver**：展开有限关系、验证图、计算候选上界、证明 `greedy_v1` 的唯一性，
   生成 prepared solver profile。
2. **runtime coordinator**：只按 prepared profile 执行已冻结的候选排序、资源检查和确定性提交，
   不解释 CXT、不改写图、不选择未声明的算法。

如果 Stage 7A 的 `greedy_v1` 仍需要运行期逐候选决策，应将其名称改为 coordinator policy，
并把“唯一性证明”保留在 prepare。未来 `max_cardinality` 或 bounded backtracking 必须各自
拥有 capability、fuel、目标函数、唯一性规则和 Snapshot 成本。

### P0-04 Beat、TimingMap 与 Tick 没有规范映射

现有 Chart v5/TimingMap 以 RationalBeat、Tempo、Stop 和绝对 Beat 求值；V2 又引入
`chartTick`、`observationTick`、`commitTick`，并说默认可以是微秒。当前没有回答：

- RationalBeat 如何转换为整数 `chartTick`；舍入、负时间、同 Tick 碰撞和溢出规则是什么；
- tempo event 和 stop 如何影响 chart tick、observation tick 和窗口 deadline；
- `chartTick` 是音乐位置、音频时间、还是可暂停的逻辑时间；
- `observationTick` 使用设备时间、宿主到达时间、音频时间还是校准后的 session 时间；
- pause/resume、seek、reload、speed 和 time discontinuity 如何影响各时间域；
- timing error 的单位、符号、端点闭合和 `queue_next_tick` 的窗口含义是什么；
- `commitTick` 是每个 coordination window、每个 transaction 还是每个 Fact 的时间。

**建议收口：** 作者层保留 RationalBeat；准备阶段绑定一个显式 `TimebaseProfile`，产生整数
判定 tick，并冻结 exact conversion、tie rule、tempo/stop/pause/seek 行为。建议不要继续让
`chartTick` 同时承担“音乐 Beat 位置”和“会话物理时间”两个含义；至少命名上区分
`chartBeat`/`judgementTick`/`observationTick`。若保留现有四时间域，必须给出一张逐操作状态表。
`TimebaseProfile`、offset/calibration 和 late policy 应进入 judgement identity。

### P0-05 CXT v2 `effects` 与 V2 `FactBinding` 冲突

CXT v2 当前 Requirement 仍有 `effects` 字段，而 V2 明确规定 Requirement 不含 `effects`，表现
统一由 `factBindings`/Effect Graph 产生。Packed Foundation 又要求演示 profile 的 effects 为空。

未定义的情况包括：

- CXT v2 的 `effects` 是否删除、保留兼容、还是仅表示 Presentation schedule；
- 非空 effects 如何迁移到 FactBinding；
- effects 是否会进入 judgement identity、resource closure 或 eventCount；
- CXT v2 的 Animation module 与 Gameplay Requirement module 是否共用同一 Requirement record。

**建议收口：** 在 Chart v5 Gameplay V2 语义中删除 Requirement effects。对已有 candidate CXT
输入，非空 effects 只能通过显式 lowering 转为 Presentation/Effect Graph；不能直接复制进
`gameplay.requirements`。若 CXT v2 保留字段，必须明确它是 presentation-only、不能引用判定
状态、不能影响 Replay，并为旧语义提供稳定拒绝或迁移诊断。

### P0-06 CXC v1 的 Gameplay 与 Ruleset 发布入口未闭合

CXC v1 当前 candidate extension 明确 `playback=true` 必须指向 Packed Chart，但 V2 又提出
Canonical Gameplay Graph、semantic entry 和独立 `cuexis.ruleset` package。文档没有给出完整矩阵：

| entry | format | 是否 playback | 是否可独立加载 | 与 Packed 的 identity 关系 |
| --- | --- | --- | --- | --- |
| Chart source | `cuexis.chart` | 否 | source/compile 工具 | source identity |
| CXT source | `cuexis.animation-template` | 否 | compile 工具 | source identity |
| Canonical graph | 未定 | 未定 | 未定 | compiled semantic identity |
| Packed chart | Packed v1 candidate | 是 | Playback | compiled semantic identity |
| Ruleset package | `cuexis.ruleset` candidate | 未定 | prepare | ruleset identity |

**路线收口：** CXC v1 的 `playback=true` 必须带明确 `entryKind`。Stage 7A 允许两种 Playback
entry：`packed-chart` 与独立 `gameplay-graph`；Ruleset package 仍是 prepare 输入，不是第三种
Playback entry。manifest 必须记录 entry kind、compiled semantic identity、artifact identity、
Ruleset binding、capability closure 和 source-of 关系。

### P0-07 Packed `REQ0` 的兼容边界未定义

现行 Foundation `REQ0` 是 tap/point/lane 演示 profile，`CNS0` 是约束集，且 eventCount=0；
V2 需要 Hold、Release、phase、measure、resource claim、relation 和绑定。不能仅扩大现有
`REQ0` 字段的解释，因为这会破坏旧 revision 的稳定拒绝和 round-trip。

必须明确：

- 采用新 `REQ` wire revision，还是新增 phase/measure/claim section；
- `RequirementId`、resource、group、relation 的索引域；
- prepared grace、deadline、solver profile 是否写入 chart graph section；
- 旧 Packed reader 遇到新 section 是稳定拒绝还是忽略；
- Header `requiredFeatures` 与 gameplay capability 的一致性校验。

**建议收口：** 保留 `REQ0/CNS0` 的 Foundation 语义；为 Stage 7A gameplay candidate 另设明确
wire revision 或新 section 组。未知必需 section、未知 capability 和新 requirement kind 一律
稳定拒绝，不能静默解释成 tap。

### P0-08 Ruleset transaction 失败后的状态没有完整定义

V2 选择“Fact 已追加，Ruleset StateDelta 任一项失败则整个 Tick 不提交”，这是保守且合理的方向，
但后续状态未定义：

- Fact 已提交后是否仍产生 PresentationEvent；
- 是否追加 `transaction-fault` Fact，还是只生成 diagnostic；
- session 进入 `faulted` 后，Snapshot、Replay、advance、seek、reset 和 reload 如何工作；
- StateDelta 失败是正常拒绝、可恢复错误还是不可恢复错误；
- `ordered_fact_prefix` 的模块在失败重试时是否可以看到相同前缀；
- RuleEffectEvent 是否可能在 state 未提交时排队。

**建议收口：** 把“Fact commit”“Ruleset state commit”“Presentation projection”拆成三个明确
阶段。建议 Fact 一旦提交即不可撤回；Ruleset 事务失败则不提交 RuleEffect/score state，Session
进入可查询的 `faulted` 状态；Presentation 是否继续从已提交 Fact 重建必须显式选择并测试。正常
内容不应触发该路径，但 Replay/Snapshot 必须能表示它。

### P0-09 FactBinding 与现有 AnimationSystem 的生命周期未闭合

现有动画系统使用 `render.visible`、PropertyResolver 和有生命周期的 HostOverride/OverrideToken。
V2 的 `FactBinding` 已有 `targetRefs`、`aggregation`、`presentationTick` 和 `lifetime`，
但还没有定义：

- `render.visible=false` 的 gameplay override 与 Behavior/Animation/HostOverride 的 precedence；
- 事件是否立即应用、排队到下一个 RuntimeFrame，或按 presentationTime 延迟；
- early 命中在判定时间之前消失、late 命中在判定时间之后消失时，如何处理已经过去的采样点；
- same target 多个 Requirement、多次 Hit、Miss、Hold body/tail 的幂等和恢复规则；
- reload/seek 后是否重建 token、如何撤销过期 token、是否允许一个 target 多个 token；
- `one_shot` 与 `until_base_recompute` 的确切终止条件。

**建议收口：** Stage 7A 使用独立 Gameplay-to-Presentation adapter，将已提交 FactBinding 转成
带 `eventId`、`targetRef`、`property`、`value`、`precedence`、`lifetime` 和 source Fact 的
HostOverride token。AnimationSystem 不读取 Judgement。Seek/replay 从 Fact Ledger 重建 token，
而不是保存表现缓存。需要接受的最终 precedence 必须成为一个命名常量并进入绑定/动画交接测试。

### P0-10 四类 identity 的字段归属仍不一致

研究稿把 source/build、semantic、artifact、judgement 分开是正确的，但不同文档对哪些字段进入
judgement identity 的表述不完全一致。例如：

- `prepared grace`、solver profile、late policy、normalization profile、TimebaseProfile；
- `resourceDecisionPolicyRef`、FactBinding aggregation、Ruleset package build hash；
- 判定几何与 Presentation geometry 的边界；
- CXT invocation path、sourceMap、Packed compression order；
- `semanticIdentity`、`compiledSemanticIdentity`、`PreparedSemanticIdentity` 的关系。

**建议收口：** 建立唯一 identity projection 表，至少逐项标明 `source/content`、`semantic/chart`、
`artifact`、`judgement`、`presentation` 的归属。规则应以“实际生效的 prepared value”为准，
来源路径只进入诊断；只要能改变 Fact Ledger 的内容或顺序，就必须改变 judgement identity。
FactBinding 只有在其 aggregation 会改变判定事实时才进入 judgement；纯表现 binding 只进入
presentation identity。

### P0-11 资源状态机没有 capacity>1 和命名空间合同

V2 已补充 `free/held/gap/handoff_pending/terminal`，但仍未定义：

- `resourceId` 是 Chart-local、graph-global、Ruleset-owned 还是 session-owned；
- capacity>1 时是一个 owner 集合、多个 slot，还是多个并列 resource；
- owner、contact、lease 的 identity 和复用规则；
- `grace=0` 是否直接从 held 进入 terminal/free；
- gap 到期且没有新 claim 时的 Fact、状态和是否可重用；
- `terminal` 是 requirement-local、resource-lifetime 还是 chart-session 终态；
- observe claim 是否占用资源、是否可以参与 binding；
- 同 Tick recovery、release、handoff、new claim 的唯一 phase 顺序。

**建议收口：** 把资源定义成 typed `ResourceSlot` 或明确的 owner set；capacity=1 与 capacity>1
必须有不同的状态表。资源命名空间、slot identity、lease identity 和回收策略进入 canonical
graph。Stage 7A 只接受 capacity=1、exclusive、无复杂 handoff 的 profile；其他状态保留
capability 并稳定拒绝。

### P0-12 Fact 因果总序的生成算法未冻结

当前排序键已经从 `(requirementId, phase, outcome)` 扩展到 `causalOrigin`，但下列问题仍会使
不同实现产生不同 Fact Ledger：

- `originId` 如何由 Observation、Timer、Coordination、Correction 生成；
- `commitId` 是全局递增、window-local 还是由 `(window, ordinal)` 派生；
- `phasePriority` 的注册表和 revision；
- 同一个 observation 触发多个 Candidate/Fact 时的 localOrdinal；
- timer 在 seek 后的重新编号；
- correction 是否能影响同 Tick 排序；
- `factId` 是排序字段、内容 hash、还是 opaque sequence。

**建议收口：** 用规范 tuple 生成所有因果键：

```text
(originKind, originScope, originOrdinal, localOrdinal)
```

其中 scope 和 ordinal 由引擎基于 canonical window/observation identity 派生，宿主不能提供。
排序所需的 phase registry、commit allocation 和 factId encoding 必须以 bytes 形式写进 reference
evaluator，并进入 Fact semantic revision；不能依赖容器遍历顺序或线程完成顺序。

### P0-13 Snapshot/Replay schema 没有与 V2 状态闭包对齐

V2 列出了需要保存的状态，但 Stage 7A ABI 只给出生命周期和少数候选类型，没有具体 Snapshot
schema。尚未明确：

- 未提交 window、Candidate cache、resource lease、RequirementProgram registers 的编码；
- Fact Ledger 保存全量、游标、digest 还是外部引用；
- Ruleset registers、pending signals、sampling phase、late watermark 的版本和预算；
- PresentationEvent 是否重建、如何处理已经过期的 event；
- Snapshot 跨 engine minor、ruleset revision、chart semantic revision 的接受矩阵；
- Snapshot 失败恢复和 faulted session 的行为。

**建议收口：** Stage 7A 采用全量语义 Snapshot 作为第一版，明确 header 中的四分量 identity、
semantic revision、state schema revision、event/fact count 和 byte budget。表现缓存不保存，恢复
后由 Fact Ledger 与当前 Presentation binding 重建。增量 snapshot、压缩 ledger 和跨 minor
迁移不要在没有全量 golden 前进入公共合同。

### P0-14 capability 派生来源不唯一

当前同时有 Packed META `requiredFeatures`、CXT `requiredExtensions`、Chart gameplay
`capabilities`、Stage 7 capability registry 和 Session capability negotiation。未定义谁是 source
of truth，也未定义 source 声明与 assembler 派生结果不一致时的优先级。

**建议收口：**

1. Source 只声明必要的最低 capability/profile；
2. assembler 从 canonical graph、Ruleset 和 presentation closure 派生完整 capability closure；
3. Packed header 保存排序后的 derived closure；
4. CXC manifest 复制 artifact-required capability 供早期拒绝；
5. Session 只报告“未知/已知未启用/已启用但设备或预算不足/可用”，不改写 Chart graph。

声明少于派生结果必须 prepare 失败或由 compiler 补齐并记录；声明多于派生结果应按 source
校验策略明确拒绝或标为可选，不能让不同入口产生不同 identity。

### P0-15 Gameplay、Presentation、Effect 的发布粒度不清

FORMAT_BOUNDARY 将三者描述为三张图，Chart v5 修订案又要求它们聚合在 Chart v5，CXC 建议又
出现 semantic、packed、presentation-pack 三类 entry。未定义发布粒度会影响：

- 更换皮肤是否保持 Replay；
- 缺失表现资源时是否仍允许 headless judgement；
- Effect graph 是否可以独立于 Gameplay graph 更新；
- 一个 Packed artifact 是否必须带完整 Presentation closure；
- FactBinding target 缺失是 prepare failure、presentation-only failure 还是空绑定。

**建议收口：** Stage 7A 把 Gameplay graph 作为必需判定闭包，把 Presentation/Effect 作为可选
只读投影；同一 Chart v5 semantic artifact 中可以有 typed projections，但 identity 和加载能力
分开。缺失 Presentation 时允许 headless judgement；缺失影响判定的 domain/action/table 时必须
拒绝。Effect target 缺失不应改变 Fact Ledger，但必须有明确诊断和事件丢弃策略。

## 4. P1：实施前应冻结

这些问题未必阻塞最小 Tap/Hold kernel，但如果现在不冻结，后续 7B+、Studio 和包工具会重复改写
同一字段。

### P1-01 判定域和几何的 typed contract

`judgementDomain` 可以是抽象 lane、button、contact、region 或 geometry。当前 Stage 7A 只需要
离散 domain，但 Chart v5 文档没有明确：判定几何是否属于 Gameplay closure、坐标系/量程如何声明、
geometry 与 Presentation transform 如何隔离、未来动态 frame 是否以静态 typed record 表达。
建议 Stage 7A 只接受注册的离散 domain profile；任何 geometry、camera 或 render transform 推断
都稳定拒绝或进入后续 capability。

### P1-02 CXT local relation 的全局合并规则

多个 CXT invocation 合并时，local resource/relation 的 namespace、稳定 ID、同名合并、跨
invocation 引用、source reorder 和 invocation reorder 的 canonicalization 还没有完整算法。
应冻结 `invocationId + emissionPath + localId` 的寻址 tuple，并明确哪些 relation 必须提升为
Chart-level relation。

### P1-03 有界循环的 canonical form

`CoordinationGraph` 要求 DAG，但 Roll、交替和周期交接可能需要有界循环。需要二选一：prepare
完全展开，或写出带 count、window、resource、source path 和预算的 `boundedRelationInstance`。
必须定义 identity、snapshot 和 fact order，不能只说“有限”或“运行期不会无限循环”。

### P1-04 late policy 的 finalization watermark

`reject_late` 和 `queue_next_tick` 方向清楚，但 `finalizationWatermark`、最大 queue hop、窗口
open/close、重复排队、observationTick 不变而 commitTick 改变的条件还没有数值和状态表。
`reopen_uncommitted_window` 应继续保持 candidate，直到有 window epoch、重开上限、snapshot 和
Replay 合同。

### P1-05 连续输入 capability 的输入来源和量化

`input.trajectory.v1` 列出了 reconstruction、samplingPhase、minimumRate、maximumGap 等字段，
但没有 source event schema、position/amount 的量化类型、坐标系、断点后的 contact 生命周期、
区域边界含义、采样失败时是 reject/session terminate/requirement close。应先定义 capability
profile，再决定 Packed 表达；不能因为 `position` 字段存在就宣称支持 Slider。

### P1-06 Ruleset package 的执行与分发边界

Ruleset package 的 manifest、module/fold order、programPolicy、outcomeScope 和 sandbox 方向
明确，但 CXC entry kind、package identity、版本迁移、缺包行为和 prepare 时机还未闭合。需要明确
package 是 Chart 外部 session input、CXC closure 必需 entry，还是可以使用内置 ruleset alias。

### P1-07 Score、Combo、Life、Statistics 的 vocabulary

FactRecord 允许 `category/outcome/grade/measure/error`，Ruleset 又可以声明寄存器，但没有冻结
Stage 7A 的最小 outcome/category/grade 集合、缺少 grade 表时的行为、错误单位、统计累计和 reset/
seek 规则。没有这张表，Replay digest 只能证明输入相同，不能证明 Score 结果相同。

### P1-08 ABI 类型与 Canonical model 不一致

候选 ABI 的 `InputEvent.observationTime`、`JudgementFact.error`、`JudgementResult.eventSequence`
过于简化，V2 使用 `observationTick`、Candidate、CoordinationCommit、FactRecord、causalOrigin、
measure、evidenceObservationIds 和 transaction events。必须建立 ABI-to-canonical mapping，明确
哪些字段只在内部 model、哪些进入公共 typed preview，避免实现先按简化 struct 冻结后再破坏性扩展。

### P1-09 诊断层级和稳定拒绝码

当前同时出现 `unsupported_capability`、`unknown_capability`、`capability_disabled`、
`budget_exceeded`、`late_policy_incomplete` 等分类。需要统一 code、category、severity、source
path、identity component、recoverability 和 faulted-session 行为。未知字段、未知 capability、
已知但未启用、设备不足、资源不足和预算不足不能共用一个 code。

### P1-10 预算分层没有与 Packed/Runtime 对齐

Chart v5 已有 Packed file/decoded/entity/section budget，Gameplay V2 还要加 Candidate、branch、
resource lease、timer、Fact、Ruleset state、Presentation event、Snapshot 和 Seek cost。需要
明确预算是 source、prepare、wire、steady-state、snapshot、replay 还是 package closure，并给出
最坏值的计数入口。研究切片的 4,777 字节 snapshot 不能成为 ABI 常量。

### P1-11 FactBinding aggregation 与重复事实

`any/all/groupCommit` 只给出概念，没有定义同一 Requirement 多个 phase、重复 Hit、Miss 后 late
Hit、部分 group 成员失败、Fact correction 和 target 已隐藏时的幂等。应规定 binding 的输入事实
集合、终态条件、重复事件去重键和重建算法，并把它从 judgement identity 中按“是否改变 Fact”
区分出来。

### P1-12 early/late 的 timing error 与窗口边界

`timingError = observationTick - chartTick` 方向清楚，但 Exact 是否要求零 tick、量化后误差
如何处理、早击在窗口外是否仍可产生 Fact、Miss/absence deadline 的 effective presentation
tick、Hold head/body/tail 的 error 定义，都需要表格化。early/late 不能只作为 UI 标签，它必须
有可重放的事件时点和稳定边界。

### P1-13 Presentation target 缺失策略

Requirement 可以无表现对象，但 FactBinding 指向缺失 target 时，当前同时存在“判定可独立运行”与
“prepare 必须拒绝悬空 target”两种倾向。建议区分：判定闭包中的悬空 domain/action 必须失败；
纯表现 target 缺失允许 headless/空绑定，并保留诊断；声明为 required presentation 的 target
缺失则按 presentation capability 明确失败。

### P1-14 资源 closure 与 Packed reference closure

判定域、动作、几何、table、Ruleset、FactBinding target 和 Presentation resource 的引用在不同
文档中使用不同闭包。需要一张 closure table，说明哪些进入 REF0、哪些只进 CXC manifest、哪些只
是诊断/source map，避免“无效动画资源也必须闭包”与“判定资源不应含贴图”产生冲突。

### P1-15 Chart v5 inline 与 CXT v2 emission 的等价性

压测要求 inline Requirement、CXT 展开和 Packed round-trip 得到同一 graph，但没有定义 semantic
comparison 的字段顺序、默认补全、sourceMap 忽略规则和 local relation 提升规则。应提供 reference
compiler 和逐字段 semantic diff，而不是只比较最终 hash。

## 5. P2：表述和文档层需要澄清

这些问题通常不会阻塞实现，但会令实现者和内容工具作出不同解释。

1. “Chart v5 是唯一 Gameplay 聚合入口”与“CXC 可以有 semantic entry”应明确是语义唯一来源，
   还是物理入口也唯一。
2. “Playback 不执行 solver”应改成“Playback 不解析/编译 solver；runtime coordinator 只执行
   已准备的 deterministic policy”，否则与 Candidate pipeline 冲突。
3. “commitTick 由 engine/coordination 语义决定”过于抽象，应给出 commit window 和 transaction
   的定义。
4. `phase`、`category`、`outcome`、`grade`、`localOutcome`、`reasonCode` 的层级和互斥关系未说明。
5. `action = press|release|update|absence|step` 与 `requiredAction`、`domain`、`InputDomain`
   的关系不清，应区分输入事件动作、Requirement 目标动作和局部程序 phase。
6. `resources` 在 Requirement 中既像 claim intent，又像全局资源定义；应分别命名 resourceRef、
   claimPolicy 和 resource record。
7. `sourceMap`、`source/build identity`、`content identity` 在不同文档中有重叠，应统一为 source
   closure、diagnostic map 和 content/artifact identity。
8. `Effect Graph`、`EffectEvent`、`RuleEffectEvent`、`PresentationEvent` 需要词汇表，尤其要说明
   哪些事件能进入 Snapshot、Replay 和 FrameSnapshot。
9. “所有未知字段默认拒绝”与 CXC `extensions` 保留未知可选 inspection metadata 的规则应区分
   semantic fields 与 inspection fields。
10. Stage 7A “Release/tail 仅在明确声明时进入”与初步方案把 Release 列入最小闭环之间应给出
    一项明确的 Stage 7A inclusion decision。

## 6. 推荐的收口链路

在不重新发明第二套谱面格式的前提下，建议把实施链路固定为：

```text
Chart v5 JSON + CXT v2 source
  -> typed source validation
  -> finite expansion / parameter freeze
  -> Canonical Gameplay Graph + Presentation projection + Effect projection
  -> prepared graph validation and capability derivation
  -> Packed candidate lowering
  -> CXC playback entry validation
  -> Stage 7A prepare
  -> runtime normalization / coordinator / Fact Ledger / Ruleset
  -> FactBinding adapter / RuntimeFrame / Snapshot / Replay
```

各层的唯一职责建议如下：

| 层 | 允许职责 | 禁止职责 |
| --- | --- | --- |
| Chart/CXT source | 作者结构、有限模板、源映射、显式 Gameplay/Presentation 关联 | 运行期输入、隐式 geometry、任意 effects |
| Canonical graph | 已展开的 Requirement、关系、资源、binding、capability、时间和 identity 投影 | JSON DOM、未解析参数、Runtime Entity handle |
| Packed | 对 canonical graph 的无损物理编码 | CXT AST、脚本、solver 推理、默认字段猜测 |
| Prepare | capability/预算/引用/唯一性/快照成本验证，生成 prepared policy | 运行中补字段、静默降级、改写 source |
| Runtime coordinator | 按 prepared policy 生成唯一 CoordinationCommit | 解释未声明关系、动态生成 Requirement、读取表现 |
| Fact Ledger | 追加不可变事实与规范因果序 | 撤回、覆盖、依赖容器顺序 |
| Ruleset | 读取 Fact，提交有界 state transaction 和下一 Tick signal | 读取 Presentation、改写 Requirement、回写旧 Fact |
| Presentation adapter | FactBinding 到 HostOverride/typed event 的只读投影 | 改变判定、直接写 Component、依赖当前渲染帧 |

## 7. Stage 7A 实施准入应新增的证据

在 active Chart v5 计划和 Stage 7A 计划进入实现批次前，建议新增以下准入证据：

1. **Version matrix**：Chart 5、Gameplay semantic revision、Packed wire/candidate revision、
   capability revision、Judgement semantic version、Replay/Snapshot schema 的接受/拒绝矩阵。
2. **Canonical graph fixture**：同一 Tap/Hold/Release 内容分别由 inline Chart、CXT v2 emission、
   Packed round-trip 得到逐字段相等的 graph。
3. **Timebase fixture**：tempo、stop、负 Beat、同 Tick 碰撞、pause/resume、seek、正负 offset、
   late window 和 boundary rounding 的 reference result。
4. **Coordination fixture**：同 Tick recovery/handoff/new claim、capacity=1、Candidate 顺序置换、
   `greedy_v1` 唯一性证明和不支持 solver 的稳定拒绝。
5. **Fact order fixture**：observation、timer、coordination、correction 的 origin、commit、factId
   和跨编译器排序结果。
6. **Ruleset failure fixture**：StateDelta overflow/conflict、Fact 已提交、Presentation/RuleEffect
   可见性、faulted session、snapshot/reload 行为。
7. **Early/late animation fixture**：early、exact、late、Miss、重复 Fact、seek/replay、target 缺失、
   Behavior 冲突、HostOverride 生命周期和 visibility precedence。
8. **Snapshot/Replay fixture**：保存未提交窗口、Candidate、资源 lease、program state、Fact cursor、
   Ruleset registers、pending signal、sampling phase 和 late watermark，恢复后逐 Fact/Score/Presentation
   event 对照。
9. **Capability derivation fixture**：source declaration、assembler-derived closure、Packed header、
   CXC manifest、Session negotiation 不一致时的稳定诊断。
10. **Budget fixture**：Packed bytes、decoded bytes、prepare peak、candidate/branch/timer、steady tick、
    Fact、snapshot、Replay 和 seek 成本分开测量。

## 8. 实施前细化清单（路线已决）

以下问题应在 ADR/Spec 进入实施准入前逐项写成可执行合同，并记录接受、拒绝或 deferred。
它们不再重新打开已确认的 Gameplay v2、双 Playback entry、Release/tail、early/late bridge 或
single resource 方向：

1. Stage 7A 的 Packed section/wire revision 如何表达 Requirement phase、resource、relation、solver、FactBinding 和 capability？旧 Reader 如何拒绝？
2. `CXT v2 Requirement.effects` 的 source-only 保留、显式 lowering 和稳定拒绝规则；
3. compile/prepare solver 与 runtime coordinator 的边界和 `greedy_v1` 命名是什么？
4. Beat/TimingMap 到 judgement tick 的 exact mapping、stop/pause/seek/offset/late 规则是什么？
5. resourceId、capacity=1、owner/lease/contact、grace=0 和 terminal 的完整状态表是什么；handoff 与 capacity>1 必须保持稳定拒绝？
6. causalOrigin、commitId、factId、phasePriority 的生成和排序字节合同是什么？
7. Ruleset transaction 失败后的 session、Fact、Presentation、Snapshot 和 Replay 行为是什么？
8. `render.visible` Gameplay override 与 Behavior/Animation/HostOverride 的 precedence/lifetime 常量是什么？
9. 哪些 graph/closure 字段进入 semantic、judgement、artifact、presentation identity？
10. Packed META、Chart capability、CXT extension、CXC manifest 和 Session registry 的派生关系是什么？
11. Presentation 缺失时是否允许 headless judgement；FactBinding target 缺失如何报告？
12. H01-H05/H07 与 H06/H08 的稳定 reject code 如何进入 Stage 8 发行矩阵？

## 9. 最终判断

当前初稿已经完成了“把玩法从单 Requirement 命中提升为跨 Requirement 唯一交易”的结构性
重构，方向足够支撑后续高度自定义和扩展。它现在的主要风险是交接合同未闭合，而不是缺少更多
Pattern。建议先完成本审查稿列出的 P0 收口，再把决定后的最小字段集合反映到 Chart v5
candidate amendment、Packed wire amendment、CXT v2 amendment、CXC entry matrix 和 Stage 7A
contract matrix。P0 未关闭前，不应把 D+C+E 的 88.5% 候选表达力写成当前支持，也不应让实现者
通过默认值自行决定版本、时间、资源、因果顺序或表现覆盖规则。


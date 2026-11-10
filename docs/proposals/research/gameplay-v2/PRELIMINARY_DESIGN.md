# Gameplay V2 初步设计方案

状态：candidate；研究提案，未接受，未实施

更新日期：2026-10-02

本文把 [Gameplay V2 表达力压测报告](GAMEPLAY_EXPRESSIVENESS_STRESS_TEST.md) 中的结论收敛为一份
可供实施前评审的初步方案。它修改的是 Chart v5 的候选 Gameplay 语义，不创建第二套判定实现，
也不表示任何 Reader、Writer、Packed decoder、Judgement、Ruleset 或动画桥接已经实施。

本文的建议优先级高于本目录早期的局部草案，但在 ADR、生产 Schema、ABI 和 Stage 7A 实施准入
完成前，所有字段和数值都仍是 candidate。

## 0. 已确认路线与仍待细化

以下路线决定已由 owner 确认，后续计划和本目录文档必须以此为基线：

- Chart 外层保持 `version = 5`，Gameplay 语义固定为 `gameplay.version = 2`；不引入
  `semanticRevision = 2` 作为同义替代，也不允许 Reader 通过字段存在与否猜版本。
- Canonical Gameplay Graph 允许作为 CXC v1 独立 `gameplay-graph` Playback entry；它与
  `packed-chart` entry 共用 Graph、prepare、Judgement、Ruleset、Snapshot、Replay 和
  Presentation path，不形成第二套语义实现。
- Stage 7A 包含 Tap/Hold、同一 Requirement 内的 Release/tail phase、early/exact/late/Miss
  Presentation bridge，以及单一 `capacity=1` exclusive resource。
- H01-H05/H07 是“能力边界，不会再开发，不受支持”；H06/H08 是“未来开发，现不支持”。

仍待实施前细化的只是这些既定路线的交接合同：Packed section/wire revision、TimebaseProfile
的逐字段边界、CXC manifest 的 closure/budget、Ruleset fault 的诊断编码、以及现有
HostOverride/AnimationSystem 的 precedence 常量。它们不再是路线方向选择。

## 1. 设计目标和非目标

### 1.1 目标

V2 要让一份准备完成的 Chart v5 Gameplay 图能够在不加载渲染器、音频后端、World、EnTT 或
宿主引擎的情况下，完成以下闭包：

```text
同一 Canonical Gameplay Graph
+ 同一 Ruleset / Engine Table
+ 同一 Session 投影
+ 同一 Normalized Observation 流
=> 唯一 CoordinationCommit 序列
=> 唯一 Fact Ledger
=> 唯一 Ruleset 状态和 Replay digest
```

同时需要满足：

1. Tap、Hold、Release、共享资源、和弦、有限交替、有限 quota 和保护规则可以通过组合的
   typed 语义表示，而不是不断增加专用 Pattern 名称。
2. 多个 Requirement 的关系有独立载体，不借用 Presentation parent、对象数组顺序或 Entity ID。
3. 每一个会影响结果的状态、排序、采样、求解和迟到策略都有 prepare-time 上界、identity 影响和
   稳定拒绝路径。
4. 实时输入和 Replay 输入共用同一条规范化与提交路径。
5. 判定事实、Ruleset 状态和表现事件可以分别快照、回放和演进。
6. 最基本的 early/late 音符消失效果可以接入现有 AnimationSystem，而不让 AnimationSystem
   读取 Judgement 或 Chart。

### 1.2 非目标

下列能力继续不属于当前模型：

- 任意运行时脚本 VM、运行时动态生成 Requirement、随机谱面或随机判定；
- 墙钟、IO、网络或宿主逐帧回调直接参与判定；
- H01-H05、H07 所列能力：能力边界，不会再开发，不受支持；
- H06 三维物理斩击和 H08 任意自由体感传感器融合：未来开发，现不支持；
- 以渲染位置、材质、动画状态或相机状态隐式生成判定域；
- 在 Playback 中解析 CXT、执行 Pattern 展开或运行 Coordination solver；
- 通过任意 `extensions` 字段绕过能力注册、预算、Replay 和身份合同。

## 2. 建议的总体数据流

```text
Chart v5 + CXT v2 source
  -> typed lowering / finite expansion / validation
  -> Canonical Gameplay Graph
  -> prepared RequirementProgram + CoordinationGraph
  -> normalized observation timeline
  -> Candidate collection
  -> Coordination transaction
  -> append-only Fact Ledger
  -> Ruleset State Transaction
  -> PresentationEvent / RuleEffectEvent
  -> RuntimeFrame / Snapshot / Replay
```

每一层只拥有一种方向的权限：

| 层 | 可以做什么 | 明确不能做什么 |
| --- | --- | --- |
| Chart/CXT lowering | 解析、展开、规范化、预算验证 | 运行时读取输入、生成 Fact |
| Input normalization | 转换设备或 Replay 为 Observation | 改写 Chart、直接发 Fact |
| RequirementProgram | 局部匹配、计量、有限状态、产生 Candidate | 占用全局资源、写 Ruleset |
| CoordinationGraph | 资源、绑定、顺序、基数、原子提交 | 读取渲染状态、生成新的 Requirement |
| Fact Ledger | 追加不可变事实、提供因果总序 | 撤回或覆盖旧 Fact |
| Ruleset | 读取 Fact、原子更新状态、排队下一 Tick 信号 | 读取表现缓存、回写 Requirement |
| Presentation adapter | 将 Fact 映射为只读表现事件 | 改变判定、写 Ruleset |
| AnimationSystem | 按 typed animation input 求值和混合 | 读取 Judgement、Chart JSON 或宿主状态 |

## 3. V2.1 的能力分层

压测中的 `D=26` 和 `C=42` 说明核心结构足以覆盖大量玩法；`E=17` 说明连续输入、全局最优
匹配和复杂设备能力必须独立演进。建议把发行能力分为三层：

### 3.1 Stage 7A 最小闭环

Stage 7A 只冻结以下子集：

- Tap、Hold head/body、显式声明的 Release/tail；
- 一个离散 `exclusive` resource，支持 `free -> held -> terminal` 和有界 Hold grace；
- `RequirementProgram` 的 atom、sequence、有限 timer 和局部 measure；
- `greedy_v1` solver，要求图结构使稳定贪心等价于唯一解；
- append-only Fact Ledger、无 correction 的基础 vocabulary；
- typed Ruleset transaction、score/combo/statistics、下一 Tick signal；
- `reject_late` 与一跳 `queue_next_tick` 两种迟到策略；
- 完整 Snapshot/Seek/Replay；
- 基于 FactBinding 的 early/exact/late/Miss `render.visible` 事件；其中 early/late 使用
  Observation tick 与 Chart tick 的差值，exact/Miss 也必须走同一重建路径。

Chord、复杂 handoff、quota、有限交替、全局最优多指匹配、连续轨迹和方向量均保持为后续
capability。Stage 7A 不通过改变已冻结字段含义来“顺便支持”它们。

Stage 7A 的资源子集只允许 `free`、`held`、`terminal`/`free` 和同一 owner 的有限恢复；
`gap`、`handoff_pending`、跨 owner lease 交接以及 `recover`/`handoff` claim intent 属于
后续 resource capability。完整 V2 状态机可以研究这些状态，但不能把它们误算为 Stage 7A 支持。

### 3.2 后续 typed capability

后续 capability 至少包括：

| Capability | 解决的问题 | 必须额外冻结的内容 |
| --- | --- | --- |
| `coordination.binding.v1` | 两键/多键和弦、部分和弦 | cardinality、原子性、solver profile、tie-break |
| `coordination.temporal.v1` | 先后、交替、有限跟随 | 时间容差、实例展开、失败策略 |
| `coordination.quota.v1` | N 选 M、计数、保护窗口 | 部分成功、失败时点、窗口关闭 |
| `resource.handoff.v1` | Hold 断开恢复、共享接触交接 | 生命周期、同 Tick phase、grace |
| `coordination.optimal.v1` | 全局最大可行集合 | 目标函数、fuel、唯一性证明、预算 |
| `input.trajectory.v1` | Slider、区域覆盖、连续位移 | 重建模型、最低采样率、断点和预算 |
| `input.direction.v1` | Flick、方向、速度 | 向量量化、时间差、最小弧度/速度 |
| `ruleset.correction.v1` | 追加抵消事实 | source Fact、可见时点、递归禁止 |

每个 capability 都必须在 registry 中声明 `capabilityId`、revision、required format、static
budget、snapshot cost、replay impact 和 stable reject code。

### 3.3 永久拒绝与未来开发

压测 H 类的支持政策直接进入能力矩阵：H01-H05、H07 是永久能力边界；H06、H08 只能通过新的
独立设计和 ADR 进入未来阶段，不能在现有 Chart v5 `extensions` 中预留未定义字段。

## 4. Canonical Gameplay Graph

### 4.1 图的组成

Canonical Gameplay Graph 是 Chart v5 Gameplay 的唯一运行时语义来源，建议包含：

```text
graphRevision
timebaseRef
rulesetRef
requirements[]
resources[]
groups[]
relations[]
solverProfiles[]
factBindings[]
capabilities[]
sourceMap (diagnostic only)
```

其中 `sourceMap` 只服务诊断、编辑器和迁移，不参与 judgement identity。图中禁止 JSON DOM、CXT
AST、未解析参数、对象数组下标、运行时 Entity handle 和隐式默认的判定几何。

建议 Chart v5 使用以下候选形状；`gameplay.version = 2` 仅表示本提案的候选语义版本，Chart
本身仍为 `version = 5`：

```json
{
  "format": "cuexis.chart",
  "version": 5,
  "gameplay": {
    "version": 2,
    "timebaseRef": "engine.tick.us.v1",
    "rulesetRef": "ruleset.default.v1",
    "requirements": [],
    "resources": [],
    "groups": [],
    "relations": [],
    "solverProfiles": [],
    "factBindings": [],
    "capabilities": [],
    "extensions": {}
  }
}
```

已有 `gameplay.version = 1` 的候选文档不能静默解释成本文语义。它必须由显式离线迁移器
转换为 `gameplay.version = 2`，或在最早可判定入口稳定拒绝；不能让 Reader 根据字段是否存在
猜测。

### 4.2 RequirementRecord

每个 Requirement 是一个可独立验证的局部程序实例：

```text
RequirementRecord {
  requirementId        canonical stable id
  anchor                chartTick / interval
  judgementDomainRef    abstract input domain
  action                press | release | update | absence | step
  program               finite RequirementProgram
  phases[]              head | body | tail | close | custom capability phase
  measures[]            typed integer measures and grading
  resources[]           claim intent: observe | consume | recover | handoff
  resourceDecisionPolicyRef
  localClosePolicy      deadline and terminal reason
  capabilityRefs[]
  sourceMapRef          diagnostic only
}
```

`RequirementRecord` 不含 `effects`。表现映射统一进入 `factBindings`；会影响 Score、Life、Combo
或后续判定的行为统一进入 Ruleset。

### 4.3 CandidateRecord

RequirementProgram 只能产生三种局部结果：`Candidate`、`LocalSignal`、`LocalClose`。
Candidate 至少包含：

```text
CandidateRecord {
  candidateId
  requirementId
  phase
  localOutcome
  observationIds[]
  claims[]
  measure
  validUntil
  priorityKey
  causalOriginHint
}
```

Candidate 是临时值，不是 Fact。被 Coordinator 拒绝的 Candidate 可以留在诊断或审计快照中，
但不能进入 Ruleset、Presentation 或 Replay 的已提交事实流。

## 5. CoordinationGraph 的细化

### 5.1 关系类型

第一版继续只保留四种关系：

```text
exclusive(resource, capacity, members, policy)
binding(group, members, cardinality, atomicity, failurePolicy)
temporal(before | after | within, left, right, tolerance)
quota(window, members, min, max, policy)
```

它们是组合语义，不是专用玩法。关系成员使用 canonical ID，不能使用数组位置或名称模糊查找。

### 5.2 资源状态机

压测发现 `owner/leaseStart/leaseEnd` 不足以定义 Handoff 和同 Tick 竞争。资源必须有显式状态和
显式的 `resourceDecisionPolicyRef`：

```text
free
held
gap
handoff_pending
terminal
```

建议状态转移如下：

| 当前状态 | 事件 | 下一状态 | 说明 |
| --- | --- | --- | --- |
| `free` | 新 claim 被提交 | `held` | 生成新的 lease |
| `held` | 合法 update | `held` | 保留 owner 和 contact |
| `held` | 断开且 grace > 0 | `gap` | owner 保留，等待恢复 |
| `gap` | 原 owner 在 grace 内恢复 | `held` | 这是 recovery，不是新 claim |
| `gap` | grace 到期 | `handoff_pending` 或 `terminal` | 由 handoff policy 决定 |
| `handoff_pending` | 新 claim 被提交 | `held` | 关闭旧 lease，建立新 lease |
| `handoff_pending` | 窗口关闭且无 claim | `free` 或 `terminal` | 由资源策略决定 |
| `terminal` | 任意 claim | `terminal` | 稳定拒绝 |

`contact end` 不自动复用旧 contact handle。旧 lease 的终止、恢复、新 claim 和和弦绑定必须在同一
Coordination window 中由显式 phase 求值。

### 5.3 同 Tick 交易顺序

为解除 C13，建议把一个 Coordination window 固定为以下逻辑阶段：

1. 规范化并收集本窗口内的 Observation；
2. 运行所有 RequirementProgram，生成 Candidate；
3. 结算已经到期的 `gap`，生成 lease-close 候选；
4. 处理同一 owner 的 recovery；
5. 处理 `handoff_pending` 的新 owner 竞争；
6. 求解仍为 `free` 的资源和新的 binding/quota；
7. 以一个不可变 `CoordinationCommit` 同时提交释放、恢复、handoff 和新 claim；
8. 从 commit 生成 Fact，不允许中途向 Ruleset 或 Presentation 可见。

这不是实现遍历顺序，而是进入 canonical identity 的语义 phase。若某一玩法需要不同顺序，必须
注册新的 `resourceDecisionPolicy` capability；不能在同一 policy 中由作者数组顺序决定。

### 5.4 Solver profile、目标函数和预算

稳定排序不能替代全局最优匹配。CoordinationGraph 必须引用一个明确的 `solverProfile`：

```text
SolverProfile {
  solverId
  revision
  algorithm: greedy | bounded_backtracking | max_cardinality
  objective: lexicographic list
  tieBreak: canonical tuple list
  maxCandidates
  maxBranches
  maxFuel
  rejectIfNonUnique
}
```

建议默认目标函数为：

```text
binding completeness
-> committed cardinality
-> candidate priority
-> earliest causal origin
-> canonical candidate id
```

Stage 7A 只接受能证明 `greedy_v1` 唯一的图；需要最大基数、最大权重或完整和弦优先的图必须
声明后续 solver capability。超过 `maxFuel`、无法证明唯一解或目标函数缺失时，prepare 稳定拒绝，
运行时不能截断、随机挑选或退回贪心。

### 5.5 有界循环

CoordinationGraph 的运行时结构仍必须是 DAG。CXT repeat、有限交替和周期动作采用以下两种
方式之一：

1. 优先在 prepare 阶段完全展开为有序实例；或
2. 生成 `boundedRelationInstance`，明确 `count`、每个实例的 source path、窗口、资源和预算。

未知道长度的循环、运行时 while、由输入决定实例数量的图一律拒绝。`boundedRelationInstance` 的
`count` 和实例身份进入 semantic identity。

## 6. Fact Ledger 和因果总序

### 6.1 FactRecord

Fact Ledger 只追加，不覆盖、不撤回、不按更高等级替换旧 Fact。建议结构为：

```text
FactRecord {
  factId
  chartTick
  commitId
  phasePriority
  causalOrigin {
    originKind: observation | timer | coordination | correction
    originId
    localOrdinal
  }
  requirementId
  componentIndex
  emissionIndex
  category
  outcome
  grade
  measure
  error
  evidenceObservationIds[]
  reasonCode
}
```

`originId` 由引擎在规范化或协调阶段分配，宿主不能提交自定义 origin。一个 timer 域、输入域或
correction 域各自允许本地 ordinal，但不能用一个无来源类型的整数混在一起。

### 6.2 总序和 correction

建议总序为：

```text
(chartTick,
 phasePriority,
 originKindPriority,
 originId,
 localOrdinal,
 commitId,
 requirementId,
 componentIndex,
 emissionIndex,
 factId)
```

`originKindPriority` 也必须是规范常量，而不是实现枚举值。初步建议为：

```text
observation  = 0
timer        = 1
coordination = 2
correction   = 3
```

同一类 origin 再使用引擎分配的 `originId` 和 `localOrdinal`。如果未来需要改变这个优先级，必须
提升 Fact semantic revision；不能在同一 revision 内按模块注册顺序决定。

`factId` 只作为最终唯一化字段，不替代其他因果字段。Correction 属于独立 capability：

- correction 必须引用一个已经提交的 source Fact；
- correction 只能追加，不能删除或改写 source Fact；
- correction 在下一次 Ruleset transaction 可见，不能回到过去的提交窗口；
- correction 不得生成另一个 correction，`correctionDepth` 最大为 1；
- Stage 7A 不启用 correction capability，遇到该字段稳定拒绝。

这样可以保留审计和 Replay 的事实历史，同时避免 Ruleset 通过递归抵消形成不确定的 Fact 流。

## 7. Ruleset State Transaction

### 7.1 读取和写入声明

每个 Ruleset module 在 prepare 时声明：

```text
readSet: 允许读取的 Fact category、state register、signal queue
writeSet: 可写的 state register 和可发出的 signal
reducer: exclusive | commutative_monoid | ledger_derived
visibility: tick_start | ordered_fact_prefix
```

模块不能读取另一个模块的 pending delta，也不能通过 PresentationEvent 传递隐含状态。

- `tick_start` 只能读取 Tick 开始时的状态；
- `ordered_fact_prefix` 可以读取当前 Fact 总序中已经出现的前缀；
- `ledger_derived` 只能由 Fact Ledger 重算，不能直接写。

### 7.2 事务边界

一个 Tick 的过程是：

1. 固定 Tick-start state 和 Fact prefix；
2. 按 Fact 总序运行已验证的 reducers；
3. 收集 `StateDelta`、下一 Tick signal 和只读 PresentationEvent；
4. 校验范围、冲突、预算、reducer closure 和 signal queue；
5. 所有校验通过才原子提交 state；
6. 任意失败都放弃整个 StateDelta，保留旧 state，并产生稳定诊断。

CoordinationCommit 和 Fact Ledger 在此之前已经不可变地提交；Ruleset 失败不能撤回事实。生产
实现应通过 prepare 上界和整数证明消除正常运行中的事务失败；若仍发生运行时不可恢复错误，Session
进入明确的 faulted 状态，不能提交半个 Tick。

### 7.3 RuleEffectEvent 与 PresentationEvent

```text
RuleEffectEvent       -> 只影响下一 Tick 的有界规则输入
PresentationEvent     -> 只读表现、音频、诊断或 UI 事件
```

Ruleset 不能读取 PresentationEvent；Presentation adapter 不能写回 Ruleset。下一 Tick signal 必须
进入 Snapshot，避免 Seek 后丢失冷却、护盾或保护窗口。

## 8. 输入、迟到事件、连续输入和 Replay

### 8.1 NormalizedObservation

规范输入至少包含：

```text
observationId
observationTick
ingressSequence
domain
action
channel
contact
position / amount
discontinuity
sourceClass
```

输入适配器必须在进入 PlaybackSession 前提供单一 ingress 序列。引擎不使用线程完成顺序、设备
扫描码或宿主容器顺序作为判定语义。

### 8.2 Stage 7A 迟到策略

Stage 7A 只接受以下两种会话级策略，并把策略写入 session judgement identity：

| 策略 | 规则 |
| --- | --- |
| `reject_late` | Coordination window 进入 `finalized` 后到达的 Observation 不再产生 Candidate，只产生稳定的 rejected diagnostic。 |
| `queue_next_tick` | 尚未进入最终化边界、但错过原窗口的 Observation 最多转发到下一个 eligible window；`observationTick` 不变，`commitTick` 取新窗口，禁止重复排队。 |

每个窗口有显式 `finalizationWatermark` 和最大 queue hop。超过边界或 queue budget 时稳定拒绝。
`reopen_uncommitted_window` 保留为 candidate capability，未来必须另行定义 `windowEpoch`、重开
次数、Snapshot 可见性和 Replay 记录；在这些条件冻结前不能进入 Stage 7A。

### 8.3 连续输入 capability

拥有 `position` 或 `update` 字段不等于支持连续轨迹。`input.trajectory.v1` 至少需要：

```text
reconstruction: endpoint_linear | polyline | raw_samples
samplingPhase
minimumRate
maximumGap
discontinuityPolicy
outOfBoundsPolicy
maxSegmentsPerWindow
```

两次 update 之间是否穿越区域、越界后回到区域、低采样率是否拒绝以及断点后能否恢复，都由这些
字段确定。`discontinuity = true` 时不得静默插值；超出最低速率或段数预算时必须稳定拒绝或按
capability 声明的 policy 结束当前 Requirement。

### 8.4 Replay 和 Snapshot

Replay 只保存规范化 Observation，不允许直接注入 Fact。Replay header 至少绑定：

```text
engineJudgementIdentity
rulesetIdentity
chartSemanticIdentity
sessionJudgementIdentity
lateEventPolicy
normalizationProfile
eventCodecId
```

Snapshot 必须保存当前 Tick、未提交 windows、Candidate 状态、RequirementProgram 状态、资源状态机、
Fact cursor、Ruleset registers、未投递 signals、连续采样相位和 late-event watermark。表现缓存、
Animation layer 临时值和 HostOverride token 不保存；Seek 后由 Fact Ledger 重新生成 PresentationEvent。

## 9. Chart v5、CXT v2、Packed 和 identity

### 9.1 CXT v2 合并规则

CXT v2 继续是 Chart v5 的作者层模板和有限生成输入。每个 CXT invocation 的局部结果必须按：

```text
(chartEntryId, invocationId, emissionPath, localRequirementId)
```

生成 canonical RequirementId。local relation 只能引用同一 invocation 的输出；跨 invocation 关系
必须由 Chart v5 `coordination.relations` 显式声明或由离线 assembler 显式提升。

合并规则固定为：

1. 同名 resource 默认视为不同命名空间；只有 Chart v5 显式 `mergeResource` 才能合并；
2. local relation 先 namespaced，再按 canonical member ID 规范排序；
3. CXT 数组重排、invocation 重排和 source map 改变不得改变 graph；
4. repeat 必须有限展开或生成 boundedRelationInstance；
5. source path 只进入 sourceMap 和诊断，不进入 judgement identity。

### 9.2 Packed Chart

Packed Chart 仍是 Chart v5 的物理发行编码。它必须无损表达 canonical requirements、资源和关系
表、solver profile、prepared grace、capability revision、fact binding 和 identity 分量。Packed
decoder 只解码已验证表，不执行 CXT、Pattern、脚本或 solver。

Source identity、semantic identity、artifact identity 和 judgement identity 分开：

| Identity | 覆盖内容 | Replay 用途 |
| --- | --- | --- |
| source/build | 作者 bytes、参数、编译器 profile | 诊断，不直接绑定 |
| semantic | canonical Gameplay Graph 和声明闭包 | chart identity 分量 |
| artifact | CXC/Packed 物理 bytes | 分发和缓存 |
| judgement | engine、ruleset、chart、session 投影和 capability | Replay 兼容判断 |

Gameplay relation、判定域、窗口、grace、solver profile、late policy、normalization profile 或
Ruleset projection 改变时必须改变 judgement identity。贴图、材质、动画 parent、表现层级和
Packed 压缩顺序变化不应改变 judgement identity。

## 10. Early/Late 最小表现方案

### 10.1 绑定模型

判定核心只产生 Fact。Chart v5 的 `factBindings` 显式把 Fact 映射到 Presentation target：

```text
FactBinding {
  bindingId
  requirementRefs[]
  phaseFilter
  outcomeFilter
  aggregation: any | all | groupCommit
  targetRefs[]
  eventTemplates[]
}
```

`requirementRefs` 与 `targetRefs` 都是稳定 canonical ID；不能通过 parent、Entity 顺序或名称猜测。

### 10.2 Early/Late 时间语义

对有 Observation 证据的 Fact：

```text
timingError = observationTick - chartTick
timingError < 0  => early
timingError = 0  => exact
timingError > 0  => late
```

命中隐藏事件的时间使用 `effectivePresentationTick(fact)`：

- Hit/Release：采用产生该 Fact 的 Observation tick；
- Miss/absence deadline：采用该 Requirement 的 hard deadline；
- 多成员 groupCommit：采用 CoordinationCommit tick；
- correction：采用 correction 可见的规则时间，不改写 source Fact 的表现历史。

因此 early 命中会在判定时间之前隐藏 note，late 命中会在判定时间之后隐藏 note。动画系统仍只
收到绝对 `localSampleTime` 和 typed property event，不知道 early/late 的计算过程。

最小事件模板为：

```text
PresentationEvent {
  eventId
  factId / commitId
  presentationTick
  targetRef
  operation: set_property
  property: render.visible
  value: false
  precedence: gameplay_fact_binding
  lifetime: until_base_recompute | one_shot
}
```

### 10.3 与现有 AnimationSystem 的桥接

建议增加独立的 Gameplay-to-Presentation adapter：

```text
Fact Ledger
  -> FactBinding evaluator
  -> PresentationEvent
  -> typed OverrideToken / animation input
  -> PropertyResolver
  -> AnimationSystem output
```

AnimationSystem 不认识 `JudgementFact`、Chart JSON、CXT 或 Ruleset。适配器通过已有
`HostOverride/OverrideToken` 边界提交 `render.visible = false`，并使用固定优先级解决与静态
`behavior.event` 的冲突。Gameplay binding 不能直接写 Component；若动画层在下一次绝对采样中仍
产生值，adapter 必须重新提交有效的 override，而不是修改动画基线。

同一目标绑定多个 Requirement 时：

- `any`：任一满足筛选条件的终态 Fact 到达即触发；
- `all`：所有引用 Requirement 都进入满足筛选条件的终态才触发；
- `groupCommit`：只有指定 CoordinationCommit 成功才触发。

表现缓存不进入 Snapshot。Replay/Seek 时从 Fact Ledger 重新计算事件，保证 early/late、Miss 和
groupCommit 的显示结果与实时路径一致。

## 11. 拒绝条件和诊断分类

Prepare 必须在以下任一条件出现时原子失败：

- 未知、未启用或预算不足的 capability；
- Requirement、resource、group、relation、factBinding 的悬空引用；
- 未终止的 CXT 展开、未界定的循环或超出 boundedRelationInstance；
- solver profile 缺失、目标函数不完整、fuel 超限或无法证明唯一结果；
- 资源状态机存在非法转移、同 Tick policy 未声明或 grace 量化不一致；
- late policy 缺少 finalizationWatermark、queue hop 或 snapshot 规则；
- Ruleset read/write 冲突、correction 递归、reducer 不满足声明的结合性/交换性；
- 连续输入声明不足以解释采样空洞、断点、轨迹重建或预算；
- Presentation target 缺失、aggregation 不合法或离散属性 override 权重不为 1；
- v4/v5 迁移无法判断判定域、资源共享或表现/Gameplay 关系。

诊断至少区分：`unknown_capability`、`capability_disabled`、`budget_exceeded`、
`ambiguous_migration`、`non_terminating_source`、`non_unique_solution`、`invalid_relation`、
`late_policy_incomplete` 和 `identity_closure_incomplete`。

## 12. 研究实施批次和验收证据

建议按以下顺序推进，仍然只描述研究和实施准入，不授权直接修改生产代码：

### P0：语义闭包

1. 将已确认的 `gameplay.version = 2` 反映到 Schema、Graph、Packed 和 CXC 接受矩阵；
2. 冻结资源状态机、同 Tick phase、causalOrigin、solverProfile 和 finalizationWatermark；
3. 生成 Canonical Gameplay Graph 的 reference compiler/evaluator；
4. 用 96 项压测案例重新输出 D/C/E/B/O，解除 C13、F04、C12 的结构性歧义。

### P1：格式与规则闭包

1. 冻结 CXT local relation 的 namespace/merge/提升规则；
2. 冻结 Ruleset read/write set、transaction rollback 和 correction capability；
3. 冻结连续输入 capability 的轨迹、采样和 discontinuity 字段；
4. 写出 Chart v5 source、canonical graph、Packed round-trip 的 semantic diff 工具。

### P2：Stage 7A 最小实现准入

1. 只实现 Tap/Hold/Release、单一资源、greedy_v1、基础 Fact vocabulary、typed Ruleset transaction；
2. 接入 `reject_late`、`queue_next_tick`、Snapshot/Seek/Replay；
3. 接入 Gameplay-to-Presentation adapter，验证 early、late、exact、Miss 四类隐藏事件；
4. 对同一规范输入流验证实时、Replay、Seek 后输出逐 Fact 一致。

### P3：能力扩展门禁

每个后续 capability 必须提供：

- reference evaluator 与独立实现逐 Fact 对照；
- 输入、候选、关系成员、Packed 数组重排不改变结果的置换测试；
- Snapshot/Seek/Replay 等价性；
- 正例、非终止、溢出、预算、未知 capability 和歧义迁移负例；
- semantic identity、judgement identity、artifact identity 分离证据；
- 最坏 Candidate、分支、timer、snapshot 和运行时间预算。

在上述证据完成前，能力只能标为 `candidate` 或 `rejected/deferred`，不能被 Stage 8 默认发行矩阵隐式纳入。

## 13. 当前开放问题（路线已定，合同仍待细化）

以下问题不是路线方向选择，但仍需要在 ADR/Spec/实施准入前写成可执行合同，不能在实现阶段临时猜测：

1. `max_cardinality` solver 是否进入第一批后续能力，以及其最坏 fuel 模型；
2. `queue_next_tick` 的 queue hop 和 finalizationWatermark 默认值；
3. correction Fact 是否只允许下一 Tick，或允许同 Tick 的独立第二事务；
4. CXC `gameplay-graph` entry 的 manifest closure、budget 和拒绝编码；
5. `render.visible` gameplay override 与现有 HostOverride 的最终 precedence 常量；
6. 连续轨迹的默认 reconstruction 是否只允许 `endpoint_linear`，以及跨设备最低采样率矩阵。

这些问题不影响本方案作为初步设计的结构结论：Gameplay 必须以 Chart v5 Canonical Graph 为基准，
局部 Requirement 与全局 Coordination 分离，Fact/Ruleset/Presentation 分层，且所有扩展都必须
有 capability、预算、identity、Snapshot、Replay 和稳定拒绝合同。

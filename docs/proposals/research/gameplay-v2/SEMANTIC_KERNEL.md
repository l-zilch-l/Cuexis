# V2 语义内核

状态：candidate；研究提案，未接受，未实施

更新日期：2026-10-02

本文给出 V2 的最小运行语义。它不是 C++ ABI，也不是作者 DSL；任何 JSON、文本 DSL 或 Studio 图形编辑器都必须编译到本文定义的 Canonical Gameplay Graph。

## 1. 四个时间域

V2 不把“时间”压缩成一个字段。

| 时间域 | 用途 | 是否进入判定身份 |
| --- | --- | --- |
| `chartTick` | 谱面锚点、Requirement 窗口、Coordinator 窗口 | 是 |
| `observationTick` | 输入规范化后的观测时间 | 通过事件账本进入 Replay |
| `commitTick` | Coordination 交易提交的逻辑时间 | 由 engine/coordination 语义决定；不单独作为 identity 字段 |
| `presentationTime` | 表现层插值、音画绑定和 UI | 否 |

`chartTick`、`observationTick` 和 `commitTick` 都使用有界整数单位。默认可以是微秒，但单位应由 Engine Table Registry 声明，不把单位名称写死在作者格式里。`presentationTime` 允许浮点，只能读取已提交状态。

输入事件迟到时不得直接改写过去的 Fact。会话必须选择并冻结一种策略：`reject_late`、`queue_next_tick` 或 `reopen_uncommitted_window`。最后一种只允许作用于尚未提交的 Coordination window，不能撤回已进入 Fact Ledger 的记录。

## 2. 规范输入事件

```text
NormalizedObservation {
  observationId       单调的会话内标识
  observationTick     规范时间
  ingressSequence     原始提交顺序，经 L1 固定
  domain              button | contact | axis | pointer | orientation
  action              begin | update | end | press | release | step
  channel             逻辑通道，不是设备扫描码
  contact             可选的长寿命接触句柄
  position/amount     已量化的域值
  discontinuity       是否跨过采样空洞、设备重连或丢样本
  sourceClass         设备类别，不包含序列号
}
```

L1 可以丢弃设备私有字段，但不得隐藏会影响判定的 discontinuity。离散边沿保留原始时间；连续量按 Interface 声明的格点重采样。任何低于最低上报率的设备能力必须在 prepare/session 协商时拒绝或选用显式降级策略，不能在运行中猜测。

同一 `observationTick` 内的事件先按 `ingressSequence` 排序。若多个宿主线程提交事件，宿主适配器必须在进入 PlaybackSession 前提供单一序列；引擎不得使用线程完成顺序。

## 3. RequirementProgram

RequirementProgram 是单实例有限程序，输入为当前 Observation、当前资源可见性和时间。它只能产生三种输出：

```text
Candidate       对一个或多个 phase 的局部满足描述，不改变全局状态
LocalSignal    供同一 Requirement 的后续状态使用，不进入 Ruleset
LocalClose     宣告本实例不再产生候选，带有结算原因
```

程序可以保留当前设计的 atom、sequence、alternation、bounded repeat、negation、bind 和 measure，但它们的目标是生成 Candidate，而不是直接 `emit Hit`。程序仍必须满足有限状态、有限寄存器、有限 timer、无递归、无 IO、无随机和 deadline 可证明。

Candidate 至少包含：

```text
candidateId, requirementId, phase, category
observations[]        使用的 observationId 集合
claims[]              资源和槽位的临时声明
measure               误差、覆盖、方向等量化结果
localOutcome          local_hit | local_miss | break | progress | ...
validUntil            候选仍可提交的 chartTick
priorityKey           由规则集 Interface 声明的稳定键
```

Candidate 不允许引用另一个 Requirement 的内部寄存器。跨 Requirement 关系只能通过 CoordinationGraph 的 group、resource、temporal 和 quota 边表达。

## 4. CoordinationGraph 与交易

### 4.1 图结构

节点是 Requirement 或有限的协调组，边是静态关系。每个节点声明候选上限、可参与的 phase、最晚提交时间和失败策略。图必须是有向无环的依赖图；需要循环的玩法必须改写成有界窗口内的计数或 repeat，不能使用运行期图重写。

四类一等关系：

```text
exclusive(resource, capacity)
binding(group, cardinality, atomicity)
temporal(edge, relation, tolerance)
quota(window, min, max, policy)
```

`exclusive` 解决同一接触点、按键槽位或判定线资源竞争；`binding` 解决和弦和多段共同结算；`temporal` 解决交替、先后和有限跟随；`quota` 解决“窗口内命中至少 N 次”或保护音符。

### 4.2 交易阶段

> **非权威推导，已被取代（2026-10-03）。** 本节的**五步**阶段划分是研究期的**非权威推导**，**已被
> S7A-4 的"六步外层 / 八阶段内层"规范取代**，**不得**继续作为实现依据。第 4 轮（仲裁、资源与事实序）
> 裁定：外层是计划 §S7A-4 的**六步 Tick 顺序**，内层是本节相关的**八阶段 Coordination window**，
> 且**八阶段是外层第 4 步的内部完整展开**。唯一映射与归属层级见
> [Gameplay V2 Spec §3.9](../../../formats/GAMEPLAY_V2_SPEC.md) 与
> [Gameplay V2 ABI 域 4](../../../api/GAMEPLAY_V2_ABI.md)；裁定登记见
> [RULING_WORKSHEET.md §4](../../gameplay-v2-acceptance/RULING_WORKSHEET.md)，带日期证据见
> [第 4 轮门禁报告](../../../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-4-gate-rulings.md)。
> **本节正文（含五步划分与其后的求解策略段）保留不改，作为历史论证记录。**

一个 Coordination window 的求值分为五步：

1. 收集在 `[open, close]` 内产生且未过期的 Candidate。
2. 删除违反本地状态、资源 owner 或静态关系的 Candidate。
3. 按 `priorityKey`、`candidateId` 做规范排序；不得使用数组位置。
4. 在静态上界内求解可行集合；求解器必须给出唯一结果或按声明的 tie-break 选唯一结果。
5. 生成一个不可变 `CoordinationCommit`，分配 `commitId`，一次性转移资源 owner 并提交被选 Candidate。

默认求解策略为稳定贪心：按排序依次尝试，违反 binding/cardinality 的组使用回溯上限 `K`；超过 K 直接 prepare 拒绝该图，而不是运行中截断。需要最优匹配的模式可以声明 `solverId` capability，但必须提供同样的确定性和上界证明。

### 4.3 资源生命周期

资源包含 `resourceId`、`owner`、`leaseStart`、`leaseEnd`、`terminal` 和 `handoffPolicy`。Observation 的 contact 句柄不会因设备 end 自动复用。一个候选只能在交易提交时获得资源；被拒绝的候选不得留下半占用状态。`handoff` 是一次新的候选竞争，不是隐式抢占。

## 5. Fact Ledger

Fact Ledger 是 append-only。V2 不允许撤回、覆盖或按“更高等级”替换旧 Fact。若 Ruleset 需要抵消效果，必须追加一个新的 correction Fact，并由 Ruleset 明确解释；旧 Fact 仍可用于审计和 Replay 差异。

```text
FactRecord {
  factId
  chartTick
  commitId
  phasePriority
  sourceEventSequenceOrTimerOrdinal
  requirementId
  componentIndex
  emissionIndex
  category / outcome / grade
  measure / error
  evidenceObservationIds[]
  reasonCode
}
```

总序为：`chartTick -> phasePriority -> sourceEventSequenceOrTimerOrdinal -> commitId -> requirementId -> componentIndex -> emissionIndex`。其中 `commitId` 由 Coordinator 分配，不能由宿主提供。Ruleset 的每个 reducer 只能读取当前 Tick 之前或本次总序中已出现的 Fact；信号默认在下一 Tick 可见。

## 6. Ruleset State Transaction

Ruleset 声明一组有界寄存器：`score`、`combo`、`life`、计数器、技能冷却、用户扩展状态等。每个寄存器属于以下一种类型：

- `exclusive`：一个模块拥有写权，写入顺序固定。
- `monoid`：多个模块提供贡献，使用声明的可交换、可结合 reducer。
- `ledger_derived`：只能从 Fact Ledger 重新计算，不允许直接写。

一个 Tick 的 reducer 读取 Fact 总序和 Tick 开始时的状态，产生 `StateDelta`。所有 Delta 通过校验后原子提交；其中任何一项越界、冲突或预算超限，整个交易失败并保留上一状态。不得提交半个 Tick。

状态交易输出两类事件：

```text
RuleEffectEvent     影响后续判定的有界信号，只能下一 Tick 生效
PresentationEvent   只读表现事件，不能被 Ruleset 读取或写回
```

这样可以表达“Hold 断开后扣血并变灰”，但扣血是 RuleEffectEvent/StateDelta，变灰是 PresentationEvent；两者不再共享一个模糊的 Effect 字段。

## 7. 快照、Seek 与 Replay

快照必须保存：当前 Tick、未提交 Coordination windows、Candidate 缓存、资源 lease、RequirementProgram 状态、Fact Ledger 游标、Ruleset state、未投递信号和周期采样相位。表现缓存不保存。

Seek 只能从一个已提交快照开始，先恢复账本和图状态，再按规范 Observation 尾部重放。禁止从 Presentation 或已压缩的 Score 反推 Requirement 状态。

V2 的关键不变量：

```text
same canonical graph + same normalized observations + same engine tables
  => same CoordinationCommit sequence
  => same Fact Ledger
  => same Ruleset state and replay digest
```

若无法证明其中任一箭头，能力只能作为 candidate 并稳定拒绝权威 Replay。

## 8. 静态验证清单

prepare 必须检查：类型和引用闭包、时间不回退、候选/资源/窗口上界、图无非法循环、solver fuel、整型范围、Fact vocabulary、reducer 冲突、Effect 方向、快照大小、Replay identity 和 capability revision。任何未知字段默认拒绝；允许扩展的字段必须位于明确的 extension namespace。

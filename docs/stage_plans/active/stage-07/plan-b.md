# Stage 7B+ Plan: Capability Evolution

状态：future；各能力按独立依赖和 owner 接受情况排期
更新日期：2026-10-09

本分册定义 S7B/S7C 能力的准入、依赖、批次和验收。Stage 7 全局边界、Stage 8 选入规则、统一证据矩阵、停止条件和跨阶段交接见[总计划](plan.md)。能力字段和运行语义必须先进入其权威 ADR/Spec/ABI；候选能力不视为已实现。

S7A 是 Stage 8 的硬前置；Stage 8 不等待全部 S7B/S7C。每项能力单独准入、实现、预算与验收，未选入发行矩阵的能力保持稳定拒绝。

## 与RPA及发行主线的关系

Stage7B+是保留的独立持续能力线，不被RPA替代或从路线省略。Slide/Flick、方向、连续轨迹、
多指及S7C高级校准策略按本计划逐项准入；基础实体设备和实时同步的实施/验收整体归
[RPA](../../future/realtime-playback-foundation/plan.md)。需要实时宿主边界的具体能力依赖RPA
对应合同，其他研究与无该依赖的工作可独立安排。Stage8只消费已选入并通过验收的7B+能力，
不等待整个能力线结束；本次文档修订不启动7B+/S7C产品实施。

## 5. Stage 7B+ 能力线的统一规则

### 5.1 新能力准入包

每个 7B+ 能力在开工前必须有一份 capability proposal，至少包含：

```text
capability name and stable ID
RequirementKind / Pattern / Measure / Ruleset ownership
prepare-time fields and quantization
runtime state additions and snapshot representation
InputDomain / Action / mapping requirements
arbitration and ownership interaction
timing, geometry and numeric range
identity components and Replay version impact
budget profile and worst-case limits
unsupported/rejection diagnostics
v4/v5/CXC/Packed representation decision
headless golden and migration strategy
Stage 8 inclusion decision: included / candidate / deferred / rejected
```

能力不能通过修改 7A 已发行字段含义来“顺便支持”。如果不能保持旧 Replay 或旧 v5 语义，必须
创建新的 capability revision 或 Replay revision，并在 capability negotiation 和 replay header 中
显式出现。这里的 revision 不是 Chart 外层 `version = 5`，也不是 Gameplay `gameplay.version = 2`
的替代字段；不得重新引入 `semanticRevision` 作为同义版本。

### 5.2 统一实现和退出门禁

每个能力都必须经过：

```text
proposal + threat model
  -> independent reference predicate / model
  -> typed prepare and static validation
  -> runtime implementation
  -> deterministic golden and differential tests
  -> Replay / identity / snapshot tests
  -> budget and cross-toolchain tests
  -> capability / rejection / migration tests
  -> selected release matrix and owner acceptance
```

研究性实现、手写条目、单平台结果和只测正例的 golden 都不能单独退出。实现与参考模型必须
至少有短轨迹穷举；长轨迹可用固定种子随机化补充，但若结果依赖自动机/Pattern 编译，须增加
编译产物与独立谓词的等价性检查。跨编译器结果差异先归为阻塞，不能以“整数运算应当确定”
解释；所有几何平方、乘法和加法必须有范围证明或显式拒绝。

### 5.3 W 类缺口记录（W-class gap record）

“W 类缺口”指：某条能力需求**无法在 7A 已冻结的合同层内安全表达**，且不允许隐含近似或静默降级。
它不是一个能力，也不是待办愿望，而是**一条记录 + 一次稳定拒绝**。

字段定义（唯一权威）在
[SUPPORT_AND_REJECTION_MATRIX.md](../../../proposals/gameplay-v2-acceptance/SUPPORT_AND_REJECTION_MATRIX.md) §6：
`wId` / `statement` / `unableLayer` / `affectedCaseIds` / `blockedBy` / `attemptedWorkarounds` /
`stableRejectCode` / `ownerDecision` / `reviewDate`。本节只规定它在 Stage 7A 计划中的登记与引用规则，
**不重新定义字段**，也不改变其处置词。

- 遇到无法表达的跨实体约束、几何量或连续量时，**先登记** W 类缺口记录，**再**稳定拒绝；不得把无法表达
  的关系藏进 Pattern、近似为已支持语义，或留成未声明行为。
- 引用具体缺口时必须给出已登记的 `wId`。**未登记的缺口不得编造编号**；需要引用时先补登记。
- 登记**不等于**纳入：W 类缺口不进入 7A 支持集合，也不自动成为 7B+ 能力线承诺；是否纳入由 owner
  决策并写入 `ownerDecision`。
- 每条记录必须带 `stableRejectCode`，使拒绝可被 golden 与诊断测试机械核对。
- W 类缺口记录与 capability proposal（§5.1）是两种不同对象：前者描述“现在不能表达什么”，后者描述
  “将来如何安全地表达”。

## 6. Stage 7B+ 分批规划

### S7B-0：Capability registry、版本和扩展测试骨架

**目标。** 在加入高级判定前，先建立可查询、可拒绝、可重放的扩展框架。

**范围。** capability ID/revision、RequirementKind 注册、支持矩阵、prepare preflight、稳定
拒绝、identity/replay revision、测试 fixture registry、Packed/CXC entry 标记和运行期只读查询。

**不做。** 不实现具体 Slide/Flick；不开放动态插件、运行时脚本、宿主回调或未审查的 Ruleset
字节码。

**退出。** 未知 capability、已知但未启用 capability、revision 不匹配、预算不足和旧 Replay
在新引擎上的行为均稳定；7A capability 查询和拒绝回归保持全绿。

### S7B-1：连续输入、区域采样、Slide 和 handoff continuity（新增 capability）

**目标。** 在 7A 的离散 `InputEvent`、Tap/Hold 生命周期和单一 resource 基础上，引入新的
`resource.handoff.v1` 与连续采样 capability，支持接触轨迹和 Slider，不改变 Tap/Hold 基础语义。
handoff、Gap 恢复和 `sameContact` 不属于 7A；本批次不得把 7A 的 `prepared grace` 或
Release/tail 解释成已经存在的 handoff。

**范围。** 连续 position/amount 规范格点、区域/轨迹域、局部坐标到判定域的量化、segment 进度、
handoff Hold、Slider continuity grace、尾部 deadline、sameContact 和多段 coverage。S7B-1 的 segment
进度只表示单一连续 Requirement 的 coverage；分段 tick 的独立计分语义属于 S7B-4。

**固定边界。** handoff 使用独立的 prepared `continuityGrace`，判定式为
`event.t < releasedAt + continuityGrace`；不得复用或覆盖 7A 的 `preparedGrace`。恢复按普通
consume 仲裁；7A Hold deadline 仍为 `end+preparedGrace`；Slider deadline 为
`t1+max(window.maxLate,continuityGrace)`；Gap 中保留段号和进度；不新增运行期 Hook、`reconnect`
Fact 或 Pattern 原语。

**必须验证。** 1/4/8 ms 上报率、重采样相位、插值误差、轨迹边界含/不含、连续/断开/恢复、
sticky 与 handoff、多个 Gap 重叠、Slider 段不回退、C10-C14 矩阵、快照恢复、新头部竞争、
跨编译器宽整数一致性和真实几何量程。

**退出产物。** capability Spec/ABI 增量、区域/轨迹 golden、Replay identity、预算 profile、
Packed 表达或稳定拒绝、Stage 8 inclusion 建议。`resource.handoff.v1`、连续 Slider 和
`sameContact` 必须各自有 capability ID/revision；它们不得改变 7A `preparedGrace` 或终止后
单 resource slot reuse；不能因“研究 spike 通过”自动进入 Stage 8
默认发行。

### S7B-2：Flick、方向、速度和位移约束

**目标。** 支持具有明确方向和连续量约束的动作。

**范围。** press/release 或 press/motion 的动作窗口、方向向量/角度、最小位移、速度区间、
方向容差、一次性量化、设备能力声明和输入丢样本处理。

**固定边界。** 所有几何和速度计算在 prepare 声明的整数域完成；不使用平台浮点容差、墙钟或
未声明的设备采样率；不能把 Flick 失败降级成 Tap 命中。

**必须验证。** 方向正交/反向/边界、零位移、过快/过慢、press-release 顺序、采样稀疏、设备
最低上报率、跨坐标系、量程溢出、重复事件和 replay identity；独立参考谓词与 runtime 差分。

**不包含。** 3D 方向斩、连续音高、自由体感控制；这些超出当前 L2 范围，需另立 ADR。

### S7B-3：多接触点、和弦、资源占用和跨 requirement 聚合（新增 capability）

**目标。** 支持多指/多通道输入，同时保留 requirement 间仲裁的确定性。

**范围。** 接触点 identity、生命周期、并行 requirement、chord 聚合、fanout、claimKey、槽位
复用、observe/consume 可见性、同接触点约束和有限跨 requirement fold。这里的 fanout、并行槽位/
接触点复用、`capacity > 1` 和多 contact ownership 都是新的 capability；7A 的 `fanout=1`、单一
`capacity=1` exclusive resource 和终止后的有限 slot reuse 语义保持不变，不能通过本批次回写
或扩大 7A。每个扩展必须有独立 capability ID/revision、identity、Replay 和预算。

**设计边界。** 多指 Slide 默认是多个独立 requirement 加 L3 聚合，不恢复 Pattern 积；跨 requirement
共享/交替等约束必须作为显式 Ruleset/aggregate capability，不能隐藏在 Pattern 或 claim side effect。
跨实体约束若无法在现有 L2/L3 表达，先登记 W 类缺口（登记与引用规则见 §5.3）并稳定拒绝。

**必须验证。** 新 capability 的同锚点 fanout 1 或声明上限内的有限 fanout、交错接触、接触点复用前后、未终止归属
阻挡、observe 同时可见、同 Tick 多手、输入顺序置换、聚合部分成功/失败、快照中接触归属和
设备上限预算；`all` 不能表示无界 fanout，必须先解析为有限上限；同时验证 7A 固定 fanout=1 和单 resource golden 未被改变。

### S7B-4：高级尾判、Lift、计数和连续动作族（新增 capability）

**目标。** 在不改写 7A Hold 或其显式 Release/tail phase 的前提下，增加需要额外状态、独立
grading 或有界重复的动作语义。7A 已支持的 Release/tail 仍是同一 Requirement 内的最小 phase；
本批次不得重新定义它、改变其边界或把它拆成隐式独立 Requirement。

**候选子能力。** 高级 Hold tail/release grading、Lift、Roll/连打、有限计数、Spinner/累积量、
分段 Slider tick。这里的高级 tail/release 指超出 7A 最小 phase 的独立尾判、Lift 或累积语义，
不是再次声明 7A Release/tail。每个子能力单独编号和 capability，不把它们捆成一次大版本。

**共同门槛。** phase/category 独立 Fact；计量和 grading 可分别定义；累积型 measure 必须声明
采样周期敏感性，端点型 measure 必须给出一个周期的最坏延迟；计数、循环和状态数有 checked
预算；超出范围稳定拒绝。

**不自动承诺。** 研究案例可表达不等于全部进入 v5；复杂规则若需要跨 requirement 约束、3D
几何或连续音高，归入 W 类缺口（§5.3）或超出范围。

### S7C-1：校准、设备延迟和 Judgement Policy

基础实时架构与正式命名迁移归[独立Stage RPA](../../future/realtime-playback-foundation/plan.md)。
S7C-1负责校准参数及高级设备策略；本批不承接基础同步缺口，未因此启动实施。

**目标。** 将 timing offset、input latency、窗口等级和设备能力变成显式、可复现的策略。

**范围。** `JudgementConfig`、`CalibrationProfile`、输入/输出 offset 的单位和符号、应用时点、
设备最低上报率、等级窗口、Life/Accuracy policy 和 capability 协商。

**边界。** 校准不得读隐式历史或宿主私有类型；Chart offset、输入 offset、输出估算不能重复
叠加；profile 变更必须通过 prepare/session transaction；设备不满足能力时拒绝或按显式策略降级。

**验证。** 相同 profile 跨机器一致、正负 offset、seek/reload、profile 迁移、热插拔/设备变化、
窗口边界、identity 变化和 Replay 重放；未经批准不把设备序列号或硬件私有信息写入 Chart identity。

### S7C-2：Ruleset package、模块扩展和 Replay 演进

**目标。** 允许内置和分发 Ruleset 复用同一 Interface，同时保持不可信包的静态安全边界。

**范围。** `cuexis.ruleset` package manifest、interface projection、module/fold 顺序、Hook owner、
`programPolicy`、`outcomeScope`、package identity、签名/完整性（如另行批准）和 Replay revision。

**边界。** Ruleset 包复用 CXC v1 ZIP32 Stored 载体形状但不伪装成 Chart entry。包内只能是
manifest、常量和已注册的有限模块参数；不得包含任意脚本、通用/用户可执行字节码、动态插件
加载、宿主 API、IO、随机数、递归/无限循环或动态 Requirement。模块实现来自引擎的静态 registry，
模块冲突在 prepare 拒绝；签名/完整性不扩大执行权限。

**验证。** 包字节/清单置换、未知模块/outcome/hook、独占 owner 冲突、可交换聚合、程序策略、
包升级和旧 Replay 兼容/拒绝；恶意/超预算包的时间、内存和 decoded bytes 限制。

### 6.1 Stage 7B+ 小目标核验台账

7B+ 的每一行都可以单独立项，但不能跳过 S7B-0 的 capability 和拒绝框架。若一个能力同时需要
多个小目标，只有表中所有依赖行退出后，能力才可进入 Stage 8 的 `included` 候选。

| ID | 小目标 | 必要产物 | 核验标准 |
| --- | --- | --- | --- |
| S7B-0.1 | capability registry | 稳定 ID/revision、RequirementKind 表和查询 API | 已知/未知/编译但未启用三种状态可区分；查询为只读且不改变会话；ID/revision 进入 capability identity |
| S7B-0.2 | prepare preflight | capability、预算、依赖和拒绝检查器 | 缺 capability、错误 revision、依赖未满足、预算超限和非法组合在 prepare 失败；无半准备状态 |
| S7B-0.3 | 扩展版本策略 | capability/Replay revision、Replay header 和迁移/拒绝表 | additive 与 breaking 变化分类明确；不新增同义的 `semanticRevision` 字段；旧 Replay/旧 v5 在新引擎上的接受或拒绝稳定；禁止静默降级 |
| S7B-0.4 | extension fixture harness | reference predicate、差分 runner、固定种子和报告格式 | 短轨迹穷举、边界负例、长轨迹固定种子、跨编译器逐行对照可重复；研究模型与产品 binary 分开 |
| S7B-1.1 | 连续量规范格点 | 连续 position/amount event fixture | 同设备类别在最低上报率以上格点相同；插值误差、相位和最大延迟有声明上界；低速设备稳定拒绝 |
| S7B-1.2 | 区域/轨迹判定域 | 2D region、corridor、segment 的 typed domain | 边界含义、局部坐标、半宽/长度量程和转换 identity 冻结；平方/乘加不溢出，超量程明确拒绝 |
| S7B-1.3 | `resource.handoff.v1` Gap capability | 独立 continuityGrace、Gap state、timer 和 snapshot 增量 | 作为新增 capability 独立协商；不得重用 7A preparedGrace；C10-C14 全部通过；严格 `<`、零 grace、一次 Break、普通仲裁恢复和 Hook 不追溯修改均可复现；7A 无 handoff 回归保持通过 |
| S7B-1.4 | Slider 段进度 | segment index、progress、head/body/tail Fact | 断开恢复后段号不回退、head 只产生一次、尾 deadline 使用 `max(window.maxLate, grace)`；新头部竞争按 claimKey 决定 |
| S7B-1.5 | 连续能力预算 | activity/Gap/sample/geometry profile | 真实和压力内容分别测量 activity peak、Gap timer、sample count、snapshot bytes 和 Seek；研究 96 条切片不直接冻结限额 |
| S7B-1.6 | Slide release/拒绝 | 支持/不支持的 Slide 表和 Packed/CXC 决策 | 缺轨迹、错误接触点、段顺序、超密度和未声明采样策略稳定拒绝；不降级成 Hold/Tap |
| S7B-2.1 | 方向量化 | direction vector/angle typed projection | 正交、反向、零向量和边界方向得到规范整数结果；平台浮点库和未声明容差不参与决策 |
| S7B-2.2 | Flick 生命周期 | press→motion→release 状态和窗口 | 正确顺序、早/晚动作、重复 release、无 press、窗口边界和超窗口均有稳定 Fact/diagnostic |
| S7B-2.3 | 位移/速度约束 | min/max displacement、speed/amount measure | 量程、单位、除零、过快/过慢和累计误差有 checked arithmetic；同一规范事件跨工具链结果一致 |
| S7B-2.4 | 稀疏输入和丢样本 | sparse/duplicate/drop fixture | 声明最低上报率以下拒绝或按批准策略处理；不能把缺样本假设成静止或成功 Flick |
| S7B-2.5 | Flick replay/identity | identity、Replay 和 migration golden | 改变方向表、量化、窗口或采样策略只影响规定 identity 分量；旧 Replay 不被静默解释为 Tap |
| S7B-2.6 | Flick capability 选入 | Stage 8 matrix entry 或 deferred record | 编码、预算、拒绝、headless、external consumer 和回滚证据齐全后才能标记 included/candidate |
| S7B-3.1 | 接触点生命周期 | contact begin/move/end、source identity fixture | 接触点唯一性、重用、越界、同时结束和 device limit 有明确状态；顺序置换不改变已声明语义 |
| S7B-3.2 | 多点 claim 仲裁 | claimKey/fanout/observe/slot fixture | 新 capability 的同锚点、交错输入、未终止归属、已终止复用和 observe 可见性与 Ruleset 一致；每事件消费数符合声明的 fanout；7A 固定 fanout=1 回归通过 |
| S7B-3.3 | chord 聚合 | 多 requirement + L3 aggregate capability | 部分成功、超时、成员顺序置换、重复成员和缺成员均有稳定结果；聚合不回写 L2 Requirement |
| S7B-3.4 | 跨 requirement 约束审计 | W-class gap record（§5.3）或显式 aggregate contract | 同指/交替/共享资源等无法表达的关系不得藏入 Pattern；无法安全表达时生成稳定 unsupported 诊断 |
| S7B-3.5 | 多点 snapshot/replay | contact ownership 和 aggregate snapshot | 任意 contact/Gap/aggregate 恢复后尾部输入逐位一致；Replay 不依赖设备私有 pointer 或线程顺序 |
| S7B-3.6 | 多点容量 profile | max contacts、activity、Fact 和 snapshot measurements | 设备上限、最坏候选数、聚合状态和 snapshot bytes 分别计量；超限保留旧状态并稳定失败 |
| S7B-4.1 | 高级 tail grading/Lift | phase-specific requirement 和 grading | 仅针对超出 7A 最小 Release/tail 的独立 capability；高级 tail 或 Lift 的时间/电平边界独立计量；7A Hold/Release/tail golden 不改变；未声明尾判明确拒绝 |
| S7B-4.2 | Roll/有限计数 | bounded repeat/count register | count 上限、重复输入、超计数、窗口和饱和均有边界；循环有 checked state/event budget，不允许无限 repeat |
| S7B-4.3 | Spinner/累积 measure | sample-period declaration、accumulator 和 grading | 累积型采样敏感性进入 capability/identity；周期变化结果可解释；溢出和过量采样稳定拒绝 |
| S7B-4.4 | Slider tick/分段事件 | finite tick anchors、coverage 和 arbitration | tick 数量、段间隙、重复命中、缺段、同接触点和段顺序有 golden；不生成未声明的动态 requirement |
| S7B-4.5 | 复杂动作族独立选入 | 每个子能力独立 ID、Spec、matrix entry | 每个子能力单独有 Replay、budget、migration、拒绝和 owner decision；禁止把“动作族全部完成”作为模糊结论 |
| S7C-1.1 | CalibrationProfile 数据 | profile schema、单位/符号/范围和 identity 表 | 正负 offset、零值、越界、未知版本和迁移稳定；Chart/input/output offset 不重复叠加 |
| S7C-1.2 | 应用时点和事务 | prepare/session apply 状态矩阵 | profile 只在批准时点生效；活动会话不回读文件；失败保留旧 effective profile 和旧 identity |
| S7C-1.3 | 窗口/Accuracy policy | JudgementConfig、grade table 和 life policy | 窗口严格边界、等级映射、Life/Accuracy 累计和 reset/seek/replay 一致；表 ID 只增不改 |
| S7C-1.4 | 设备能力协商 | capability negotiation 和 fallback policy | 不满足最低上报率、触摸/压力/方向能力或设备变化时按显式拒绝/降级；不写入未经批准的硬件私有 identity |
| S7C-1.5 | 校准 Replay/迁移 | profile identity、Replay fixture、旧版本矩阵 | 同 profile 跨机器相同；profile 变化造成预期 identity 变化；旧 Replay 兼容性有明确接受或拒绝 |
| S7C-2.1 | Ruleset package manifest | `cuexis.ruleset` manifest、interface projection 和 hash | package 字节/清单顺序规范化；缺字段、未知字段、重复 module 和 hash mismatch 在 prepare 失败 |
| S7C-2.2 | Module/fold ownership | module order、Hook owner、combine operator 表 | 独占写目标只有一个 owner；派生值使用可交换/可结合算子；冲突不能靠实例枚举顺序决定 |
| S7C-2.3 | programPolicy/outcomeScope | locked/extendable/open 和 declared/extended fixture | 非法组合、未声明 outcome、未声明 Hook 和 open 之外的扩展稳定拒绝；结果集合进入 ruleset identity |
| S7C-2.4 | Ruleset package sandbox | budget、decoded bytes、termination 和 no-IO checks | 不可信包不能执行 IO、随机、宿主回调、无限循环、递归或动态 Requirement；超时/超内存保留旧 session |
| S7C-2.5 | Ruleset replay evolution | package identity、Replay revision 和 migration | 包顺序、模块、Hook 或 fold 语义变化影响预期 identity；旧 Replay 不因“模块名称相同”而误接收 |
| S7C-2.6 | package/candidate publication | CXC v1 Stored entry、entry kind 和 production/candidate matrix | Ruleset entry 不伪装成 Chart entry；source/compiled/playback 分层清晰；默认 Writer 不发布未接受 package |

## 7. 7B+ 与 Stage 8 的选入规则

Stage 8 只在其发行矩阵中明确选入某项 7B+ 时消费该能力。选入必须同时满足：

- Stage 7B+ capability 和 typed prepare 合同已接受；
- Chart v5/CXT v2/Packed/CXC 有明确编码、identity 和 resource closure；
- 默认 Writer、candidate Writer、Playback preflight 和 Player 入口的支持/拒绝一致；
- headless golden、Replay、snapshot/seek、budget、cross-toolchain 和 external consumer 证据齐全；
- 迁移、旧版本拒绝和回滚路径可执行；
- owner 在 Stage 8 发行矩阵中明确接受。

未选入的能力仍可在 Stage 8 前开发，但只能通过显式 candidate/capability 入口使用；Stage 8
默认生产入口必须稳定拒绝，不能把它当作 Tap/Hold 或 v4 兼容语义。Stage 8 关闭后，7B+ 可以
继续增加兼容 capability；不得以修改已发行 v5 字段含义的方式追补。

## 验收标准

Stage 7 的验收由 Stage 7A 硬前置和每个 7B+ capability 的独立退出标准组成。任何研究性 spike、
单平台结果、只测正例的 golden 或历史报告都不能替代本计划要求的实现和验证证据。

# Stage 7.5 Plan: Gameplay Evolution

状态：active；独立阶段规划与交接准备，尚未开始产品实施。

更新日期：2026-11-14

返回[阶段计划索引](../../README.md)；报告入口见[独立阶段报告索引](../../../stage_reports/stages/stage-07-5/README.md)。
当前进度唯一以[CURRENT_STATUS](../../../CURRENT_STATUS.md)为准。

## 1. 阶段定位与范围

owner 要求将原 Stage 7B+ 从 Stage 7 完整拆出，接受 Gameplay Evolution 名称，并指出无编号会使接手者困惑。
因此登记为 **Stage 7.5：Gameplay Evolution**，独立目录为 `stage-07-5`，位于 Stage 7 与 Stage 8 之间；
小数编号表示路线顺序，不表示 Stage 7 子阶段。Stage 8–14 编号保持。

本阶段拥有高级输入／判定能力演进，以及已交接的局部重构与性能稳定工作；计划、实施报告、预算和退出单独管理。
Stage 7 保留既有内核、诊断、RPA／PD 的实施与受限关闭证据，后续工作不再追加到 Stage 7。
原 `S7B-0–4`、`S7C-1/2` 是保留的批次追溯标识，不再表示所属阶段号；相关 capability、wire、Replay、
identity 及已接受合同不因本次拆分改名或重编号。

| 旧名称／批次 | 当前归属 |
| --- | --- |
| Stage 7B+／S7B/S7C 能力线 | Stage 7.5：Gameplay Evolution |
| S7B-0–4 | 本阶段 capability、连续输入、方向、多接触点及高级动作能力工作线 |
| S7C-1/2 | 本阶段高级校准／设备策略、Ruleset 包及 Replay 演进工作线 |
| R4 已知性能缺陷 | 本阶段 GE-R 局部重构与性能稳定；原始发现与失败仍归 Stage 7 证据 |

## 阶段目标

以独立 capability 扩展连续输入、方向动作、多接触点、复杂持续约束、校准和 Ruleset，
并在明确适用范围内消除或处置已交接的复制／保活成本，形成可交给 Stage 8 的实现、预算及限制清单。
能力字段和运行语义必须先进入权威 ADR／Spec／ABI；独立阶段成立不等于产品能力已经实施或生产接受。

## 2. 前置基线与依赖

主路线为 **Stage 7 受限关闭交接 → Stage 7.5 → Stage 8**。
归档来源：[拆分前 Stage 7 总计划](../../../archive/stage-07-planning/2026-11-14-stage-07-before-gameplay-evolution-split.md)，
实际实施基线及 owner 接受边界见
[R4／RPA 受限退出与缺陷交接](../../../stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-14-limited-exit-and-defect-handoff.md)。
其已验证代码为 `d1951326`，SDK `0.9.1`；后续工具／实现差异另行绑定，不能冒称该 SHA 已认证新能力。
[独立诊断前置](../stage-07/02-diagnostics/plan.md)的 N3 已退出，正式码及兼容合同继续继承。

RPA 入场前置按 owner 明确接受的受限退出满足。原性能／内存／GPU／设备限制、P8 历史缺口、F3 延期及 testOnly
继续有效，拆分不扩大生产范围、不重置正式次数，也不把历史 fail／inconclusive／blocked 改成 pass。

| 能力工作线 | 最低依赖 | 边界 |
| --- | --- | --- |
| S7B-0 | 受限 RPA-F4 交接及 7A typed contract | registry、拒绝表与 harness 不隐含新增运行时能力 |
| S7B-1 continuous／Slide | S7B-0；连续 Slider 另依赖连续采样子项 | handoff 不自动要求 Slider，不复用 7A preparedGrace |
| S7B-2 Flick | S7B-0；使用连续 motion 时依赖相应 S7B-1 子项 | 离散子集可独立提案，不假设轨迹已支持 |
| S7B-3 multi-contact | S7B-0、contact identity／input projection；轨迹子项另依赖 S7B-1 | 键盘多键不等于多触点能力 |
| S7B-4 action families | S7B-0 及实际消费的 S7B-1/2/3 子项 | 每个动作族独立 capability，既有 Release/tail 不重做 |
| S7C-1 calibration | S7B-0 与实际设备／profile 合同 | 高级策略不自动接受历史 F3 生产参数或精确 AV |
| S7C-2 Ruleset package | S7B-0、已接受 Ruleset Interface | 静态模块／常量包，不开放脚本、字节码或动态插件 |

所用基线若包含[PD](../stage-07/04-pd/plan.md)，须消费其实际合同 revision、SDK 与安装消费者交接；
phase/outcome、FactBinding／聚合、Snapshot／Replay 和表现预算的影响逐项核验。
必需绑定不支持时在 prepare 拒绝，不静默跳过 Fact 后推进 cursor；细则见
[PD 能力边界](../stage-07/04-pd/design.md#121-能力演进与-factbinding)及
[PD 版本交接](../stage-07/04-pd/design.md#127-跨阶段交接版本与维护成本)。

## 3. 工作线与实施顺序

| 工作线 | 内容与退出 |
| --- | --- |
| GE-0 基线承接 | 读取已接受限制、固定源码／工具／内容／设备身份，维护一份本阶段门账及遗留责任；不重新认证历史失败 |
| S7B-0–4、S7C-1/2 | 按 §5–7 原有能力要求逐项准入、实施和验收，保留原批次 ID |
| GE-R 局部重构与性能稳定 | 能力工作线之后集中处理输入历史复制、Replay 保活和 Gameplay evaluation 事务成本；见 §8 |
| GE-F 独立最终接受 | 新实现身份的必要本地／hosted／消费者与预算验证、遗留处置和 Stage 8 交接，取得 owner 接受 |

沿用先完成能力工作线、后集中整改的既定顺序。原“7B+ 后集中整改”在本阶段对应能力工作线之后的 GE-R，
不再回到已收束的 Stage 7。若缺陷中途阻断功能实施，只做必要最小修复，并记录受影响回归与新候选。
本次仅完成阶段拆分与规划，不启动上述产品实现、性能采样、重构或新 PR。

## 4. 遗留责任与维护边界

新增复制／保活缺陷、计时未定、完整 memory／GPU／P8、旧 DFJK oracle、P7 normal／长活动与 F3
继续以[已接受交接表](../../../stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-14-limited-exit-and-defect-handoff.md#3-遗留项处置与恢复条件)为历史来源。
代理在本阶段承接后续整改及取证；owner 承接实体事实与生产接受。已有听音／O2 无异常反馈继续有效，
未提供的仪器数据不补造，也不因阶段迁移要求重复同一主观验收。

新进展只写本阶段报告；Stage 7 封存包与已发布关闭记录不改。
阶段计划拥有未来范围，Spec／ADR 拥有语义；报告记录实际结果；CURRENT_STATUS 是唯一当前摘要。
不会因拆分引入新公共接口、预算数值、格式字段或默认启用能力。

## 5. Stage 7.5 能力线的统一规则

### 5.1 新能力准入包

每个 Gameplay Evolution 能力在开工前必须有一份 capability proposal，至少包含：

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
`stableRejectCode` / `ownerDecision` / `reviewDate`。本节只规定它在本独立阶段中的登记与引用规则，
**不重新定义字段**，也不改变其处置词。

- 遇到无法表达的跨实体约束、几何量或连续量时，**先登记** W 类缺口记录，**再**稳定拒绝；不得把无法表达
  的关系藏进 Pattern、近似为已支持语义，或留成未声明行为。
- 引用具体缺口时必须给出已登记的 `wId`。**未登记的缺口不得编造编号**；需要引用时先补登记。
- 登记**不等于**纳入：W 类缺口不进入 7A 支持集合，也不自动成为 Gameplay Evolution 能力线承诺；是否纳入由 owner
  决策并写入 `ownerDecision`。
- 每条记录必须带 `stableRejectCode`，使拒绝可被 golden 与诊断测试机械核对。
- W 类缺口记录与 capability proposal（§5.1）是两种不同对象：前者描述“现在不能表达什么”，后者描述
  “将来如何安全地表达”。

## 6. Stage 7.5 分批规划

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

基础实时架构归[Stage 7-RPA子阶段](../stage-07/03-rpa-legacy/plan.md)，正式命名迁移归[独立诊断前置](../stage-07/02-diagnostics/plan.md)。
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

### 6.1 Stage 7.5 小目标核验台账

Gameplay Evolution 的每一行都可以单独立项，但不能跳过 S7B-0 的 capability 和拒绝框架。若一个能力同时需要
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

## 7. Gameplay Evolution 与 Stage 8 的选入规则

Stage 8启动须先取得本计划当前计划范围的完成与owner接受记录。
发行矩阵只决定哪些已验收能力进入正式发行，不用于跳过尚未完成的Gameplay Evolution任务。选入必须同时满足：

- Stage 7.5 capability 和 typed prepare 合同已接受；
- Chart v5/CXT v2/Packed/CXC 有明确编码、identity 和 resource closure；
- 默认 Writer、candidate Writer、Playback preflight 和 Player 入口的支持/拒绝一致；
- headless golden、Replay、snapshot/seek、budget、cross-toolchain 和 external consumer 证据齐全；
- 迁移、旧版本拒绝和回滚路径可执行；
- owner 在 Stage 8 发行矩阵中明确接受。

已验收但未选入正式发行的能力只通过明确的candidate/capability入口使用；默认生产入口
必须稳定拒绝，不能将其当作Tap/Hold或v4兼容语义。新增计划外能力另行立项；不得自动
把本计划未完成任务推迟到Stage 8之后，也不得修改已发行v5字段含义追补。

## 验收标准

Stage 7.5 须完成 S7B-0–4、S7C-1/2 当前计划范围的逐项退出、GE-R 处置与 GE-F 最终接受，
汇总实现、预算、拒绝、兼容、跨平台证据和 Stage 8 交接清单，并取得 owner 接受。范围调整须显式修订计划，
不能以发行矩阵中的candidate/deferred代替未完成任务的验收。任何研究性 spike、
单平台结果、只测正例的 golden 或历史报告都不能替代本计划要求的实现和验证证据。

## 8. GE-R：局部重构与性能稳定

针对已有证据分别处理累积 receipts／accepted 历史的复制、Replay 对独立历史状态的持有，
以及 Gameplay 呈现事务完整 evaluation 复制／重复 reset／旧 candidate 析构成本。
使用原有可复用表示与明确所有权，具体选型在实施前依据当前代码和独立对照确定；
不以清空 Replay、删除历史、改变判定语义、弱化事务失败或事后提高 caps 换帧率。

验证须保持旧 query／Snapshot／Replay 结果、页边界、故障／回滚和旧状态不可变语义；
同内容、同输入、同适用环境分别比较 correctness、Advance／呈现耗时、输入累积斜坡、
长活动内存与最终释放。进程 private commit 不代替完整语义 owning 图，present wall 不代替 GPU 时间。

原明显卡顿、renderer submit 差额与长期影响仍未完全归因。每轮先声明问题、组数、停止条件和受影响门，
不无界补跑、不继承或重置旧正式额度。新增高频／连续／多点能力须独立注册对应负载与预算，
不能从旧密集键盘诊断或研究切片推定容量。性能范围须在新测量前明确，不能把某一次改善比例作为通用保证。

GE-R 退出须有可复核的机制与成本证据、必要语义回归及全部未解决项的具名处置；
若仍需保留缺陷，须 owner 在 GE-F 明确接受范围、责任和恢复门，不能仅将其移出门账。

## 9. GE-F：独立验收与 Stage 8 交接

本阶段独立维护实现点、工具差异、命令、内容、profile、环境、原始输出／hash、
headless／static／shared／external consumer／平台和未执行项。新代码或版本改变时，
按实际影响完成新候选验证；d195 的 F1/F2 只证明原实现点。

确定性、语义或生命周期失败阻塞受影响能力；超预算保留原慢值／超额，停止条件与正式额度继续适用。
不可信内容应稳定拒绝并保留旧 active session；合同变化先进入 ADR／Spec／ABI／版本决定。
运行时脚本、逐帧 callback、宿主字节码、任意 IO／随机、动态 Requirement 与未版本化插件执行权无限期排除。

退出包包括全部 S7B/S7C 适用行的证据、GE-R 结论、继承缺口的准确处置、能力／拒绝／依赖闭包表，
以及 Chart v5/CXT v2/Packed/CXC 表达、Replay／identity、资源 closure 和发行选入建议。
F3 与 testOnly 的改变仍需具体校准／生产接受；只退出开发阶段不自动解除。
Stage 8 必须取得本阶段当前范围的完成及 owner 接受，发行矩阵仅决定默认选入哪些已验收能力。

## 10. 排除项与旧路径

本阶段不实现 Stage 8 默认 Writer／正式发行，也不实现 Studio、稳定 C ABI、任意脚本或未准入的 3D／自由体感能力。
[Stage 7 原计划入口](../stage-07/06-b/plan.md)保留一轮兼容导航；历史报告中的 7B+ 名称及 S7B/S7C ID
保留原语境。活合同与新交接统一引用本路径；后续阶段只消费本阶段实际接受的接口与证据。

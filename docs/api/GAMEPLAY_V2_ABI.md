# Gameplay V2 ABI（候选）：内部 typed preview 边界

状态：candidate

更新日期：2026-10-04

文档角色：内部技术参考（候选）

上级文档：[Gameplay V2 acceptance package](../proposals/gameplay-v2-acceptance/README.md) ·
[Stage 7A 实施计划](../stage_plans/active/stage-07/plan.md) ·
[Gameplay V2 研究重构](../proposals/research/gameplay-v2/README.md)

## 边界声明

本文是 Stage 7A Gameplay V2 的 **ABI 表征文档**：它给出 typed 边界的类型清单、所有权与生命周期、
单位与量程、错误与诊断映射，以及哪些内容属于本阶段承诺、哪些仍待冻结。它**不是** Spec，也**不是**
ADR，更**不是**已发布 SDK 的宿主 API。

四条边界必须先讲清楚，否则后续所有条款都会被误读：

1. **内部 / preview typed 边界。** 本 ABI 描述的是 Gameplay typed kernel 与 Playback 集成之间的
   C++ 边界。它面向仓库内消费者与 reference host，不面向第三方宿主。ADR 0027 冻结的宿主入口仍是
   `cuexis::playback::PlaybackSession`；本文不新增、不替换、不重命名任何宿主入口。
2. **不是已发布 SDK 的宿主 API。** 现有安装包（SDK API `0.7.0`）不暴露本文的任何类型。本文不改变
   SDK API 版本，也不构成版本升级授权；Stage 7A 的 typed 类型即使实现，也只进入内部/preview 安装
   组件，且必须由实现批次的安装图（见 [依赖与安装图](../proposals/gameplay-v2-acceptance/TARGETS_AND_DEPENDENCIES.md)）
   显式登记。
3. **不是稳定 C ABI。** 稳定 C ABI 不属于本阶段。Stage 7A 的实施计划把稳定 C ABI 明确交给后续阶段
   （[plan.md §10](../stage_plans/active/stage-07/plan.md)：“Stage 14 再决定稳定 C ABI；
   Stage 7 的 C++ typed preview 不等于稳定 ABI”）。本文的类型可以带 `tl::expected`、标准库容器和
   C++ 异常边界，因为它们不跨稳定 ABI。
4. **语义权威在 Spec。** 字段含义、运行语义、求值顺序、版本层级与拒绝语义的唯一权威是
   [Gameplay V2 字段与运行语义规范](../formats/GAMEPLAY_V2_SPEC.md)。本文只说明“边界上出现哪些类型、
   谁持有它们、单位是什么、错误怎么映射”，**不复述** Spec 的字段语义表格。两者冲突时以 Spec 为准，
   并应把冲突登记为缺陷而不是在代码中隐式选择。

   三份文档的分工固定：决策与威胁模型属
   [ADR 0044](../adr/0044-gameplay-v2-semantic-kernel.md)，字段与运行语义属上述 Spec，typed 边界、
   所有权、单位与错误映射属本文。三份互链，且任何一份都不得复制另一份的正文。

候选状态的含义：本文的**结构与边界声明**已随第 1 轮裁决（2026-10-02，14 行全部接受）确定
（限定冻结口径见 §S7A-1 限定冻结范围与登记规则），并随第 2 轮裁决（2026-10-03，时间域与迟到策略）
补入 Tick 宽度与单位来源、`observationTick` canonical 来源、late-policy 类型化、`commitTick` / window
定义与 `DomainAmount` 的 `AmountSpec`（限定冻结口径见 §S7A-2 限定冻结范围与登记规则）；再随第 3 轮裁决
（2026-10-03，prepare、装配与 entry）补入 `RequirementRecord` **不含 Gameplay `effects`**、`REQ0` / `CNS0`
的 entry / section 边界**行为**（并由 2026-10-04 S7A-3 裁决补齐首次 Packed 物理合同）、Release / tail 的**显式声明**、
prepare-time 有界循环的**完全展开**与 `boundedRelationInstance` 的稳定拒绝，以及**对外 identity 的
不透明性**声明（限定冻结口径见 §S7A-3 限定冻结范围与登记规则）；**其余**字段宽度、
序列化字节、最终命名和安装组件仍待实现批次与 ABI **一起**冻结
（[plan.md](../stage_plans/active/stage-07/plan.md) §S7A-1）。因此本文中任何类型名都只是**候选名**，
不是已冻结符号；范围外的表示与行为不因本文存在而获得授权。

## 类型清单

本清单按域分组。命名遵循 [AGENTS.md](../../AGENTS.md) 的 C++ 约定：类型 PascalCase、函数与变量
camelCase、命名空间 `cuexis::module`、文件名 `snake_case.hpp`。所有类型都属于 Gameplay 模块族，
不得依赖 SDL、OpenGL、Audio、World、EnTT 或 JSON DOM，也不得把第三方类型带过公共边界。

每个条目后标注其**当前状态**：

- `冻结目标`：第 1 轮裁决与已接受的 acceptance package 已确定其存在与语义角色；实现批次必须交付。
- `待冻结`：类型存在已确定，但字段集、宽度或命名仍属未决项（见本文 §未决项）。

### 域 1：时间基

| 候选类型 | 角色 | 状态 |
| --- | --- | --- |
| `Tick` | 有界整数逻辑时间值域；判定侧唯一时间载体 | 冻结目标（宽度见下表；其余宽度细节待冻结） |
| `ChartTick` | 谱面锚点、Requirement 窗口与 Coordination 窗口时间 | 冻结目标 |
| `ObservationTick` | 输入规范化后的观测时间；候选类型名，规范字段名 `observationTick` | 冻结目标（canonical 来源见下表与 §单位与量程） |
| `CommitTick` | 事务提交发生的 Tick；候选类型名，规范字段名 `commitTick` | 冻结目标（window 定义见下表） |
| `PresentationTime` | 表现层插值时间；允许浮点，只读已提交状态 | 冻结目标 |
| `TimeInterval` | 半开区间 `[start, end)` | 冻结目标 |
| `TickSpan` / `TickDelta` | Tick 位移与有符号差 | 待冻结（Q-04 / CM-T03，第 2 轮已裁定宽度与单位：**有符号 64 位整数**；位移与差值溢出稳定拒绝，不饱和、不截断。首次消费 S7A-2；附加限定：随 `CM-T03` 的 tie rule 一并裁定，tie rule 见 Spec §3.7.2） |
| `TimebaseProfileRef` | 指向 `TimebaseProfile` 的稳定引用 | 冻结目标（单位声明的唯一来源，见下表） |
| `FinalizationWatermark` | 窗口最终化边界 | 待冻结（CM-T08 / P1-04，第 2 轮已裁定为 **typed 参数**：由 `TimebaseProfile` / ruleset 提供，默认值只在 profile registry 登记为 `pending_measurement`，**不得**写成 ABI 常量或隐式零值；缺失或未测量在 prepare 稳定拒绝） |
| `LateEventPolicy` | `reject_late` / `queue_next_tick` 的会话级枚举 | 冻结目标（重复排队稳定拒绝并返回诊断码；queue hop、窗口 open/close 与 watermark 的**具体数值不属本批次**，第 2 轮 2026-10-03） |

**第 2 轮已冻结的事实（2026-10-03，状态格首词不变）。** 上表的登记首词（`冻结目标` / `待冻结`）与 §126 条
追踪规则的逐域计数**不因本节改动**；第 2 轮补入的宽度与单位事实如下：

- **宽度与单位。** `ChartTick` / `judgementTick` / `observationTick` / `commitTick` 均为**有符号 64 位整数**；
  单位由 `engine.tick.us.v1` 的 `TimebaseProfile` 声明；**禁止隐式单位换算**；**溢出稳定拒绝**，不得饱和、
  截断或落入未定义行为。`TickSpan` / `TickDelta` 的位移与差值同规则。
- **`observationTick` 的 canonical 来源。** 唯一来自输入进入判定管线时捕获的**校准后会话时钟**；设备时间、
  宿主到达时间、音频时间与渲染帧时间只保留为诊断上下文；S7C-1 只能扩展 `CalibrationProfile` 参数，
  不得替换 canonical source。
- **`commitTick` 与 commit window。** `commitTick` 是事务提交发生的 Tick；commit window 是相邻两个可提交
  Tick 之间的观察区间；事实只能在 `commitTick` 提交。
- **迟到策略是 typed 参数。** `finalizationWatermark`、queue hop、窗口 open/close 与重复排队条件由
  `TimebaseProfile` / ruleset 提供；默认值只在 profile registry 登记为 `pending_measurement`；缺失或未测量
  在 prepare 稳定拒绝；重复排队稳定拒绝并返回诊断码。**具体数值不属本批次。**
- **Beat → Tick 是精确累计函数，`stop` 是塌缩 / 跳变（第 2 轮语义经 2026-10-03 实现复核修订）。**
  `F(originBeat) = 0`；`stop` 在 `[startBeat, endBeat)` 内使**累计速率为零**，并在 `endBeat` 处把声明的
  `duration` 加入精确累计值（`F(endBeat) = F(startBeat) + duration`）；最终
  **`tick(beat) = roundHalfToEven(F(beat))`**，**只舍入一次**——**不得**写成整数 Tick 递推
  `tick(endBeat) = tick(startBeat) + duration`（那会引入第二次舍入）。半开跳变方向为正向
  `(originBeat, beat]`、负向 `(beat, originBeat]`，因此镜像 `stop` 配置下映射**不再关于 `originBeat`
  奇对称**（有向端点关系与单调性仍成立）；`stop` 区间内映射**非单射**，因此任何 **Tick → Beat 反查
  不得返回单一 Beat**（返回区间 / 集合或稳定拒绝歧义，登记为首次消费该反查的后续批次阻塞项）。
- **缺失 tempo 属结构类，空 `profileId` / `unitToken` 不是默认。** `initialTempo` 是可缺省的 optional：
  **未声明**属声明**结构不完整** → `invalid_relation`；**已声明但非正**属声明域内**数值非法** →
  `budget_exceeded`；late-policy 的缺失 / 未测量属 `late_policy_incomplete`，**三类必须分开**（Spec §9.3
  三分法）。`profileId` / `unitToken` **保持字符串**，**空值表示"未声明"**，由 prepare 以
  `invalid_relation` 拒绝；空值**不是**默认单位、也不是可用 profile（**不改为 optional**）。

语义正文见 [Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md) §0.1 与 §3.7；本轮的授权范围见本文
`## S7A-2 限定冻结范围与登记规则（2026-10-03）`。

时间域划分的依据见 [Semantic Kernel §1](../proposals/research/gameplay-v2/SEMANTIC_KERNEL.md#L9-L20)；
`ChartTick` / `ObservationTick` / `CommitTick` / `PresentationTime` 四域并存，不得压成一个字段。

### 域 2：输入与观测

| 候选类型 | 角色 | 状态 |
| --- | --- | --- |
| `NormalizedObservation` | 规范化观测；实时与 Replay 的唯一语义输入 | 冻结目标 |
| `ObservationId` | 会话内单调观测标识 | 冻结目标 |
| `IngressSequence` | 宿主在进入 PlaybackSession 前提供的单一提交顺序 | 冻结目标 |
| `InputDomain` | 抽象输入域（button / contact / axis / pointer / orientation 族） | 冻结目标（枚举集待冻结，P2-05） |
| `InputAction` | 输入侧动词集合（`press` / `release` / `update` 等） | 冻结目标（7A 只开 `press`/`release`/`update`） |
| `ChannelRef` | 逻辑通道引用（非设备扫描码） | 冻结目标 |
| `ContactRef` | 可选长寿命接触句柄 | 待冻结（Q-11 / CM-C09，第 4 轮；首次消费 S7A-4；附加限定：S7A-4 只消费最小 contact handle，不消费 handoff / `sameContact` 语义） |
| `DomainAmount` | 已量化的域值（位置 / 量） | 待冻结（CM-T13，第 2 轮，本次登记新增：T 系列下一空号，Codex 2026-10-03 裁决授权新增一条第 2 轮裁决；首次消费 S7A-2；附加限定：S7A-2 的“越界量程”负例以该裁决为准。**第 2 轮已冻结的部分**：每个 `InputDomain` 必须携带 typed **`AmountSpec`**，声明 canonical **整数量化**、**scale**、**可表示范围**与**边界策略**；量化使用精确整数运算、**最近值半数取偶**（负值对称）；**超范围 / 窄化 / 溢出稳定拒绝并返回诊断码**，不得饱和或截断。**本轮不冻结具体业务量程数值或限额**；语义正文见 [Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md) §3.7.6。**2026-10-03 输入半批复核（`adopt` 0.97 / 0.99，状态格首词不变）**：`AmountSpec` 的 canonical 整数表示为有符号 64 位整数；该宽度是 S7A-2 冻结的规范表示，并与 Tick 的有符号 64 位域一致；任何无法在该域内精确表示的量化、乘法或窄化稳定拒绝；判定顺序为**先验 canonical 可表示性、再验声明范围与边界策略**（Spec §3.7.6 第 5、6 条）） |
| `SourceClass` | 设备类别（不含序列号）；`InputEvent.source` 在 V2 的对应表达 | 冻结目标（分工见 §与 Gameplay I ABI 的关系；与 `SourceBuildIdentity` 的边界见域 7） |
| `DiscontinuityFlag` | 跨采样空洞 / 重连 / 丢样本标记 | 冻结目标 |
| `InputMapping` | 设备→域的映射 profile，属 session | 冻结目标（**2026-10-03 输入半批复核（CM-T09）**：进 session identity 的是 `profileId`、`profileVersion`、`sourceClass`，**加规范化后的域 token 集合与每项域声明的完整 `AmountSpec` 字段**；域声明顺序不承载语义、重排不改变 identity；运行期修改映射一律稳定拒绝；语义正文见 [Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md) §5.2 与域 6 `NormalizationProfile` 行） |
| `MappingProfileRef` | 映射 profile 的稳定引用与版本 | 冻结目标 |

`NormalizedObservation` 的字段集以 `docs/formats/GAMEPLAY_V2_SPEC.md` 为准；本文只固定它在 ABI 上
必须**分离表达 tick 与原始时间戳**（见 §单位与量程）。

**来源类别的分工（F-04 闭合，2026-10-03）。** 输入域上有两个来源相关类型，二者**不得混用**：

- **`SourceClass`**（域 2 第 87 行）是**规范事件中的设备 / 来源类别**，对应 Gameplay I `InputEvent.source`
  （`SourceIdentity`）的 V2 去向；它进入 canonical 观测并参与判定边界（不含序列号）。
- **`SourceBuildIdentity`**（域 7 第 207 行）**只承载生产者、参数、compiler profile 与 source map 的构建
  来源**，**不进入判定语义**，也不得用于替代或补全 `SourceClass`。

二者的字段级映射见 §与 Gameplay I ABI 的关系的字段级映射表（`InputEvent.source` 行）。

### 域 3：Requirement、Pattern 与 Measure

| 候选类型 | 角色 | 状态 |
| --- | --- | --- |
| `RequirementId` | Requirement 稳定寻址身份（六元组） | 冻结目标 |
| `RequirementRecord` | 单个局部程序实例的 prepared 记录 | 冻结目标（**不含 Gameplay `effects`**；Q-05，第 3 轮 2026-10-03：非空 `effects` 只能显式 lowering 为不影响 Judgement / Replay 的 Presentation 数据，否则稳定拒绝——语义正文见 [Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md) §3.8.1） |
| `RequirementLocalId` | 局部 ID，六元组分量之一 | 冻结目标 |
| `EmissionPath` | 由稳定 `nodeId` 与 Repeat index 构成的有序路径 | 冻结目标 |
| `RequirementProgram` | 有限局部程序（IR，非作者 DSL） | 冻结目标 |
| `PatternRef` | 对编译后 Pattern 的引用 | 冻结目标 |
| `PatternPrimitive` | `atom` / `sequence` / `choice` / `bounded repeat` / `skip` / `instant` / `complement` | 冻结目标（枚举集已冻结目标。CM-C10 / P1-03，第 3 轮 2026-10-03：`bounded repeat` 在 7A 为 prepare-time **完全展开**，展开结果唯一决定 identity / snapshot / fact order；`boundedRelationInstance` **不进 7A 合同**、遇则稳定拒绝并列为 7B+ / S7C 候选——见 [Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md) §3.8.4） |
| `MatchPolicy` | 固定 `leftmost-first` | 冻结目标 |
| `Phase` | `head` / `body` / `tail` 等分量分类 | 冻结目标（注册表待冻结。Q-18，第 3 轮 2026-10-03：Release / tail 仅作**同一 Requirement 显式声明的可选 phase**，不自动生成独立 Requirement；内容要求 tail 语义却未显式声明时稳定拒绝——见 [Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md) §3.8.3） |
| `PhasePriority` | `Phase` 的规范排序权重 | 待冻结（CM-F06） |
| `MeasureSpec` | 多分量量化规格与逐分量 grading | 冻结目标（字段集待冻结） |
| `Measure` | 单个 Fact 上的量化结果 | 待冻结（CM-S10 / P1-07，第 5 轮；首次消费 S7A-4；附加限定：S7A-4 仅消费单分量量化结果与有符号 tick error，grade / 多分量聚合留 S7A-5） |
| `Grade` | 可选等级表结果 | 待冻结（CM-S10 / P1-07） |
| `LocalClosePolicy` | 实例终态与 hard deadline 策略 | 冻结目标 |
| `PreparedGrace` | prepare 后只读的最终宽限值 | 冻结目标 |
| `GraceResolutionPolicy` | 显式/继承/default 的解析策略 | 冻结目标 |
| `ResourceClaimIntent` | Requirement 侧的 `observe` / `consume` / `claim` 声明 | 冻结目标 |
| `CapabilityRef` | Requirement 声明的 capability 引用 | 冻结目标 |
| `SourceMapRef` | 诊断用源映射引用（不进判定身份） | 冻结目标 |

`RequirementRecord` 不含 `effects`；表现统一走 FactBinding，会影响判定的行为统一走 Ruleset 事务。
`GripPolicy` 是 Gameplay I 候选 ABI 的旧类型，V2 用 `PreparedGrace` + `GraceResolutionPolicy` +
`ResourceClaimIntent` 取代（见 §与 Gameplay I ABI 的关系）。

**第 3 轮已冻结的事实（2026-10-03，状态格首词不变）。** 第 3 轮（prepare、装配与 entry）只冻结**行为**；
第 3 轮当时不冻结 wire 编号、section 布局或序列化编码。2026-10-04 的 S7A-3 设计裁决
另行闭合首次 Packed 消费的 Gameplay Capsule v2 candidate 物理合同；它不是生产 wire
或实现验收，也不改变上表登记首词（`冻结目标` / `待冻结`）：

- **`effects`。** `RequirementRecord` 不含 Gameplay `effects`；非空 `effects` 只能显式 lowering 为不影响
  Judgement / Replay 的 Presentation 数据，否则稳定拒绝（Q-05）。
- **`REQ0` / `CNS0` 的兼容边界。** 保持 Foundation 语义；7A 新语义须使用可区分的 entry / section 边界；
  未知必需 section、未知 capability 与新 requirement kind 稳定拒绝。首次 Packed 消费采用
  `candidateRevision=2`、`GPH0/GPR0/GPD0/GRC0/REF0`、section revision 1、`rowCodec=1`、
  semantic `flags=0` 与 REF0 kind 8--13；**只有四个 Gameplay sections 有行表头，REF0 无新表头**。
  完整物理字段由 [Gameplay Capsule v2 format](../formats/GAMEPLAY_CAPSULE_V2_FORMAT.md) 唯一拥有。
- **Release / tail。** 仅作**显式声明**的可选 phase；未显式声明的 tail 语义要求稳定拒绝（Q-18 / P2-10）。
- **有界循环。** prepare-time **完全展开**；`boundedRelationInstance` 不在 7A 合同内，遇则稳定拒绝，
  只阻塞**首次拟消费它**的后续批次（CM-C10 / P1-03）。

语义正文见 [Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md) §3.8；本轮的授权范围见本文
`## S7A-3 限定冻结范围与登记规则（2026-10-03）`。

**2026-10-04 S7A-3 设计收口。** S2 的来源 provenance 与有效消费 policy 分离；
同最终 grace/policy 不因显式/继承/default 来源不同而改变 judgement identity。
T4 的 body 覆盖、phase-keyed 半开窗口、checked deadline 和即时终止由 Spec §3.8.11 规定。
K4 的 signed i64 priority / unsigned u64 tieRank 固定升序，同资源唯一；
claim identity 不作竞争 fallback，capacity=1 所有候选共用 slot 0，observe 不占 slot。
Spec §3.8.12 拥有实例展开与局部 proof；Capsule kind 13 是固定 typed 投影命名空间，
只有 capability 子域进行能力检查，不是新增能力集合。本轮**不新增公共 ABI 类型行**，
仍为 **9 域 / 126 = 96 + 30**，不新增公共诊断码。runtime resource state、
Snapshot/Replay/FactId/CommitId codec 和 S7A-9 数值上限不在本次闭合范围。

### 域 4：协调与资源

| 候选类型 | 角色 | 状态 |
| --- | --- | --- |
| `CoordinationGraph` | 跨 Requirement 关系的一等载体（DAG） | 冻结目标 |
| `ResourceId` | 资源标识 | 冻结目标（命名空间**已由第 4 轮裁定（2026-10-03，`S7A4-R05`）**：`resourceId` 只在 **prepared canonical graph 的资源命名空间内**解释，不由宿主 / 表现层 / Session 重新解释，见 §协调与资源的身份规则段；逐字段表示与字节编码随实现批次） |
| `ResourceRecord` | 资源全局定义 | 冻结目标 |
| `ResourceSlot` | `capacity = 1` 的排他槽位 | 冻结目标（第 4 轮：`capacity = 1` 资源使用**唯一 slot**；slot identity **进入 canonical graph / prepared judgement inputs**） |
| `ResourceState` | `free` → `held` → `terminal`/`free`（7A 子集） | 冻结目标（第 4 轮冻结 7A 子集为 `free` / `held` / `terminal`；**`gap`、`handoff_pending`、非零资源 `declaredGapGrace`、handoff、`capacity > 1`、owner 集合、并列 slot 一律稳定拒绝**；`terminal` 编码不在本批） |
| `ResourceLease` | 一次持有关系（含 `leaseStart` / `leaseEnd`） | 待冻结（Q-11 / CM-C09，**第 4 轮已裁定（2026-10-03）**；首次消费 S7A-4；附加限定：**lease identity 进入 canonical graph / prepared judgement inputs**，lease 由**引擎按规范阶段分配**、宿主不得提供；`capacity > 1`、handoff、`gap`、非零资源 `declaredGapGrace` 保持稳定拒绝；逐字段表示随实现批次） |
| `ResourceDecisionPolicyRef` | 资源决策策略引用 | 冻结目标 |
| `ClaimPolicy` | claim 申请策略（与 resource 定义分层） | 待冻结（P2-06） |
| `ClaimKey` | prepare 后的稳定 claim 寻址键，不是 K4 竞争排序键 | 冻结目标（第 4 轮：claim identity 进入 canonical graph / prepared judgement inputs；K4 仅按显式 priority/tieRank 竞争） |
| `ExclusiveRelation` | `exclusive(resource, capacity, members, policy)`；7A 只用 `capacity = 1` | 冻结目标 |
| `BindingRelation` | `binding(group, ...)`；**7A 稳定拒绝** | 冻结目标（拒绝路径） |
| `TemporalRelation` | `temporal(...)`；**7A 稳定拒绝** | 冻结目标（拒绝路径） |
| `QuotaRelation` | `quota(...)`；**7A 稳定拒绝** | 冻结目标（拒绝路径） |
| `SolverProfileRef` | 求解/协调 profile 引用 | 冻结目标 |
| `SolverProfile` | `solverId` / `revision` / `algorithm` / `objective` / `tieBreak` / 预算 / `rejectIfNonUnique` | 待冻结（CM-C06；**第 4 轮已裁定字段语义（2026-10-03）**：**字段语义**、**缺失 / 歧义 / 无法证明唯一时的 prepare 拒绝语义**、`objective` / `tieBreak` 为**有序语义列表**已冻结；默认列表、`K` / fuel / `max*` 数值仍归 **S7A-9** 与后续预算批次。2026-10-04 仅闭合 Capsule 静态行与 K4 显式准入 profile，不授权公共 proof/runtime codec。边界见 §协调与资源的 solver / coordinator 段） |
| `CandidateRecord` | 局部候选（临时值，不是 Fact） | 冻结目标 |
| `CandidateId` | 候选稳定标识 | 待冻结（Q-16 / CM-C05，**第 4 轮已裁定（2026-10-03）**；首次消费 S7A-4；附加限定：只承担确定性候选标识，其生成服从 §协调与资源的唯一六步 / 八阶段映射） |
| `LocalOutcome` | 仅用于 Hold 段分解的局部结果 | 待冻结（CM-S10 / P1-07，第 5 轮；首次消费 S7A-4；附加限定：S7A-4 仅消费 Hold phase-local 的最小 `localOutcome`，完整 outcome / grade 由 S7A-5 冻结） |
| `CoordinationCommit` | 一个窗口的不可变提交 | 冻结目标 |
| `CommitId` | 由引擎分配，宿主不得提供 | 冻结目标（编码待冻结，CM-F06） |
| `FanoutPolicy` | 7A 固定 `fanout = 1` | 冻结目标 |
| `StrayPolicy` | `consumeEmpty` / `noCandidate` 区分 | 冻结目标 |

`capacity > 1`、owner 集合、并列 slot、`gap` / `handoff_pending` 都是**拒绝路径**：类型可以不出现，
但拒绝码必须稳定（见 §错误与诊断映射）。

**资源身份与 `resourceId` 命名空间（第 4 轮已裁定，2026-10-03，`S7A4-R05`）。** 类型级事实如下，语义正文见
[Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md) §3.10：

1. `resourceId` **只在 prepared canonical graph 的资源命名空间内**解释；它不是全局字符串命名空间，也不得
   被宿主、表现层或 Session 重新解释。
2. `capacity = 1` 资源使用**唯一 slot**；**slot identity、lease identity、最小 contact handle identity、
   claim identity 进入 canonical graph / prepared judgement inputs**（因此参与 judgement identity）。
3. lease 与 contact 由**引擎按规范阶段**分配；宿主不得提供 lease / contact identity。
4. **`contact end` 不复用旧 handle**：新接触必须取得新 handle；handle 复用只能通过资源的终止后**槽位复用**
   发生，且必须由规范阶段提交。
5. **`observe` 只产生 Observation，不占用、不改变资源 owner**：`observe` 不进入资源状态表、不生成 lease、
   不需要 slot。"`observe` 占用资源"的实现属 §S7A-4 节不得消费清单第 15 项。
6. 7A 资源状态子集为 `free` / `held` / `terminal`；`gap` / `handoff_pending` / 非零资源 `declaredGapGrace` / handoff /
   `capacity > 1` / owner 集合 / 并列 slot **一律稳定拒绝**（对应 Spec §7.2 的 R-04 与 R-01，**不新增
   R 编号**）。

**solver / coordinator 的命名边界（第 4 轮已裁定，2026-10-03，`S7A4-R01`；`P2-02` 措辞由第 7 轮
`S7A7-R07` 收官，2026-10-03）。** 本条只固定命名与职责边界，
不定义 `SolverProfile` 的字段布局：**prepare / compile solver** 负责展开、验证、上界证明、唯一性证明并生成
prepared profile；**不解析/编译 solver；runtime coordinator 只执行已准备的确定性 policy**（即 prepared
profile 指定的确定性候选排序、资源检查与提交）。若名称仍需区分，runtime 侧采用
**`coordinator.policy.greedy_v1`**；**不得**把 runtime 行为称为 solver。语义正文见
[Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md) §3.12。

**同 Tick 阶段的唯一映射（第 4 轮已裁定，2026-10-03，`S7A4-R03` / `S7A4-R04`）。** 六步 Tick 顺序是**外层**，
八阶段 Coordination window 是**第 4 步的内部完整展开**；二者不是两套并列顺序。该映射的**阶段名称、顺序、
映射与语义边界**已冻结（正文见 [Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md) §3.9），**阶段编号、
序列化编码、预算与窗口数值不冻结**。**八阶段的顺序与阶段语义进入 `judgement identity` 的 engine 组件，
不进入 chart/content identity**（见 §域 7 的不透明性与分量段）。

### 域 5：提交、Fact 与判定结果

| 候选类型 | 角色 | 状态 |
| --- | --- | --- |
| `FactRecord` | append-only 账本条目 | 冻结目标 |
| `FactId` | 最终唯一化字段（编码待冻结） | 冻结目标 |
| `CausalOrigin` | `originKind` / `originId` / `localOrdinal` | 冻结目标 |
| `OriginKind` | `observation` / `timer` / `coordination` / `correction` | 冻结目标 |
| `OriginKindPriority` | 规范常量 `0/1/2/3` | 冻结目标 |
| `FactCategory` | 事实分类层 | 待冻结（CM-S10，第 5 轮；附加限定：S7A5-R09 已冻结与 phase 一一对应的集合 `{tap, hold_head, hold_body, hold_tail}`；字段表示与编码仍待后续批次） |
| `Outcome` | 结果层，7A 最小集合 `{hit, miss}` | 待冻结（CM-S10，第 5 轮；附加限定：S7A5-R09 已冻结集合 `{hit, miss}` 及"HOLD head/body/tail 为附属的 phase-local outcome / error、不得扩充 Outcome"；grade 缺失即 absent 且不隐式升级；字段表示与编码仍待后续批次） |
| `ReasonCode` | 仅解释拒绝/降级的码 | 待冻结（P2-04） |
| `EvidenceObservationIds` | 证据观测集合 | 冻结目标 |
| `FactCursor` | 账本只读游标 | 冻结目标 |
| `FactLedgerView` | 只读账本视图（不拥有 Fact） | 冻结目标 |
| `JudgementResult` | 面向消费者的一次判定结果投影 | 冻结目标 |
| `EventSequence` | **会话内单调序号** | 冻结目标 |
| `TimingError` | **有符号整数 tick 差** | 冻结目标 |

`JudgementResult` 取代 Gameplay I 的 `JudgementFact` 五字段结构；V2 的权威事实是 `FactRecord`，
`JudgementResult` 只是其面向消费者的只读投影。`correction` 在 7A 关闭，出现即稳定拒绝。

### 域 6：Ruleset 事务与会话状态

| 候选类型 | 角色 | 状态 |
| --- | --- | --- |
| `RulesetInterfaceRef` | Ruleset Interface 的稳定引用 | 冻结目标 |
| `RulesetModuleId` | 模块标识 | 冻结目标 |
| `RulesetManifest` | 模块顺序与折叠顺序（进 ruleset identity） | 冻结目标 |
| `RegisterKind` | `exclusive` / `commutative_monoid` / `ledger_derived` | 待冻结（CM-S02，第 5 轮，绑定登记；首次消费 S7A-5；附加限定：S7A5-R04 已冻结 exclusive / commutative_monoid / ledger_derived 三类命名与冲突策略；编码与布局仍由首次消费它的后续批次冻结） |
| `StateDelta` | 一个 Tick 的状态增量集合 | 冻结目标 |
| `RuleEffectEvent` | 只影响下一 Tick 的有界信号 | 冻结目标 |
| `PresentationEvent` | 只读表现事件 | 冻结目标 |
| `ScoreState` | 分数状态 | 待冻结（Q-08 + CM-S10 / P1-07，第 5 轮；首次消费 S7A-5；附加限定：S7A5-R09 已冻结 7A 最小 outcome、category、可选 grade、有符号 tick error 以及事务失败、reset、seek、replay 语义；字段表示、编码与数值预算仍待后续批次） |
| `ComboState` | 连击状态 | 待冻结（Q-08 + CM-S10 / P1-07，第 5 轮；首次消费 S7A-5；附加限定：S7A5-R09 已冻结 7A 最小 outcome、category、可选 grade、有符号 tick error 以及事务失败、reset、seek、replay 语义；字段表示、编码与数值预算仍待后续批次） |
| `LifeState` | 可选生命状态 | 待冻结（CM-S11，第 5 轮，本次登记新增；首次消费 S7A-5；附加限定：S7A5-R10 已冻结 7A 关闭 Life 与稳定拒绝路径；不得用默认值或占位实现，类型表示与编码仍待后续批次） |
| `StatisticsSnapshot` | 统计查询结果 | 待冻结（CM-S10 / P1-07，第 5 轮；首次消费 S7A-5；附加限定：S7A5-R09 已冻结由 Fact Ledger 重建及 reset/seek/replay 统计语义；字段表示、编码与数值预算仍待后续批次） |
| `SessionState` | 会话生命周期状态（含 `faulted`） | 冻结目标 |
| `JudgementConfig` | session 级判定配置 | 冻结目标 |
| `LoadoutRef` | session loadout 引用 | 冻结目标 |
| `NormalizationProfile` | 离散输入规范化 profile（进 session identity） | 冻结目标（**2026-10-03 输入半批复核（CM-T09）**：session identity 分量为 `profileId`、`profileVersion`、`sourceClass`，**加规范化后的域 token 集合与每项域声明的完整 `AmountSpec` 字段（`scale` / `minimum` / `maximum` / `boundaryPolicy`）**；**域声明顺序不承载语义**——重排不是 identity 变化，**增删任何域或改动其 `AmountSpec` 是 identity 变化**；**7A 不引入 per-domain 版本字段**，会话级版本由 `profileVersion` 承载；语义正文见 [Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md) §5.2 与 §3.7.6） |

原子性规则：一个 Tick 的 `StateDelta` 全部通过校验才提交；失败保留旧 state、进入可查询 `faulted`，
且不得提交半个 Tick。`faulted` 会话的 `submit` / `advance` / `snapshot` / `seek` / `replay` / 就地 `reload`
行为以 Spec 为准。

**第 5 轮在本域冻结的语义（2026-10-03，`S7A5-R03` / `S7A5-R04` / `S7A5-R09` / `S7A5-R10`）。**
本域的**字段集与编码不在本轮**——`RegisterKind`、`ScoreState`、`ComboState`、`LifeState`、
`StatisticsSnapshot` 五行的首词仍为 `待冻结`（见上表）。本轮只把下列语义写入合同，供 S7A-5 逐条对齐：

1. **Ruleset Tick 三阶段**（`S7A5-R03`）：①追加并封存**已排序** Fact Ledger → ②**原子校验并提交全部**
   StateDelta / Score / Combo / Statistics / RuleEffect → ③**只从已提交 Fact Ledger 与 FactBinding
   投影**只读 PresentationEvent。第二阶段失败时**不追加 fault Fact**、**不发布** RuleEffectEvent 与
   PresentationEvent、**保留旧 state**，session 进入可查询 `faulted`；`faulted` 的 `submit` / `advance` /
   `seek` / `replay` / 就地 `reload` 与 **`snapshot`** 一律**稳定失败**，只有**显式 reset** 或**创建替换
   新 session 的 reload / recovery** 可离开，且**不得恢复或伪造未提交 StateDelta**（Spec §3.14）。
2. **`RegisterKind` 三类**（`S7A5-R04`）：`exclusive` 单 owner（第二 owner / 第二写入稳定失败）；
   `commutative_monoid` 只能由已声明贡献者以已声明且可交换、可结合的算子合成（未知 operator、重复
   contribution identity、非交换 / 非结合合成稳定失败）；`ledger_derived` 禁止直接 StateDelta 写入、
   只能从已提交 Fact Ledger 重建；**7A 只接受这三类**，其他 kind 稳定拒绝；Hook 的"下一 Tick 可见"不变
   （Spec §3.15）。
3. **Outcome / category / grade / error 与统计**（`S7A5-R09`）：`Outcome` = `{hit, miss}`；Hold 的 head /
   body / tail 是各自附属的 phase-local outcome / error、**不扩充** `Outcome`；`FactCategory` 与 phase
   一一对应（`{tap, hold_head, hold_body, hold_tail}`）；grade table 可选、缺失即 **absent** 且不隐式升级；
   `TimingError` = `observationTick - chartTick` 的有符号整数 tick 差；seek / replay 从 Fact Ledger 重建
   统计；reset 清空新 session 状态且**不产生 Fact**（Spec §3.20）。
4. **Life 关闭**（`S7A5-R10`）：任何 Life capability / Life policy / `LifeState` 初值或占位默认值一律以
   **`capability.disabled` 稳定拒绝**，**不创建、不更新** `LifeState`（Spec §3.21）。
5. `SessionState` 的 `faulted` 语义以 Spec §3.14 为准；本域**不新增类型行**，126 条追踪目录不变。

### 域 7：identity 与兼容判定

四类身份（source/build、semantic、artifact、judgement）与四分量投影（`engine` / `ruleset` / `chart` /
`session`）的定义见 [格式、入口与 identity 矩阵 §4](../proposals/gameplay-v2-acceptance/FORMAT_ENTRY_AND_IDENTITY.md#L63-L81)。
ABI 侧只固定**三条并行判定**必须分别可表达，不得合并成一个“相等”布尔值：

| 候选类型 | 角色 | 状态 |
| --- | --- | --- |
| `SourceBuildIdentity` | 作者 bytes、参数、compiler profile、**`source closure`**；**只承载构建来源，不进入判定语义** | 冻结目标（与域 2 的 `SourceClass` 分工：来源类别进判定边界，构建来源不进；见 §与 Gameplay I ABI 的关系）。**第 7 轮（2026-10-03，`S7A7-R08`）**：三项统一命名见 [Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md) §5.1.1——本行承载 **`source closure`**（`diagnostic map` 只服务诊断、不参与 identity；`content-artifact identity` 对应本表 `ArtifactIdentity` 行）。**不新增类型行** |
| `CompiledSemanticIdentity` | Canonical Gameplay Graph 与声明闭包的语义身份 | 冻结目标 |
| `ArtifactIdentity` | CXC / Packed 物理 bytes 身份（三项统一命名中的 **`content-artifact identity`**，`S7A7-R08`） | 冻结目标 |
| `JudgementIdentity` | `engine` / `ruleset` / `chart` / `session` 四分量 | 冻结目标 |
| `EngineIdentity` | 判定语义版本、实际引用的定点表 `tableId`、引擎阶段顺序、TimebaseProfile、late policy、normalization profile | 冻结目标 |
| `RulesetIdentity` | Interface 投影、折叠/模块顺序、Build hash、capability、Hook 合成 | 冻结目标 |
| `ChartIdentity` | 编译后的 Requirement/Pattern/Measure、锚点量化、prepared grace、coordination 的判定投影、solver profile | 冻结目标 |
| `SessionIdentity` | Loadout、默认 grace 来源、离散输入规范化 profile、JudgementConfig | 冻结目标 |
| `RequirementIdentity` | 六元组：`(chartEntryId, invocationId, moduleId, exportId, emissionPath, requirementLocalId)` | 冻结目标 |
| `IdentityProjection` | 逐字段归属记录（semantic / artifact / interchange 三列） | 待冻结（CM-I06 / Q-10；**第 7 轮，2026-10-03，`S7A7-R08`**：处置词已按第 1 轮裁定补齐为 `accept`，**首词仍保持 `待冻结`**——逐字段归属语义正文以 [Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md) §5.2 为准，字段表示与编码仍待后续批次） |
| `InterchangeCompatibility` | 产物互换性判定结果（含工具链与载体差异） | 待冻结（Q-01 Codex 修订；**第 7 轮，2026-10-03**：已按第 1 轮裁定补齐处置词，**首词仍保持 `待冻结`**——互换性矩阵细则见 Spec §2.4 与 §11.1 第 1 条，字段表示仍待后续批次） |
| `CandidateRevision` | Packed 候选期修订号 | 冻结目标（首次 Gameplay Capsule 消费显式使用 2，不改变 Foundation revision 1；公共类型表示仍随对应批次冻结） |
| `PreparedValue` | 判定所依据的“实际生效 prepared value” | 冻结目标 |

三条判定的分工（第 1 轮 Q-10 Codex 修订）：

- **semantic identity**：由 Canonical Gameplay Graph 与声明闭包决定；以实际 prepared value 为准。
- **artifact identity**：由物理 bytes 决定；`candidateRevision` 参与其判定。
- **interchange / compatibility 判定**：回答“两个产物能否互换使用”；它必须分别考虑工具链与载体
  差异，不能只比 `candidateRevision`，也不能只凭 `CompiledSemanticIdentity` 判等价。

每个字段归入哪一条判定，由 `IdentityProjection` 表逐字段给出（待冻结）；本 ABI 只承诺三条判定在
类型上**分别可表达**。

**对外 identity 的不透明性（第 3 轮，2026-10-03）。** 本 ABI **仅声明对外 identity 的不透明性**：四类
身份与四分量投影对外只以不透明值 + 相等判定暴露，**不承诺内部字节格式**。identity 的规范字节是 prepare
内部确定性派生物，**不是公共 ABI，也不是 Packed 编码**；其冻结边界（语义等价、排序、身份域边界）与
"同一算法在各工具链产出相同字节"的验证要求见 [Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md) §5.5。

**不透明性的范围不扩到 chart/content identity 的归属（第 4 轮，2026-10-03，`S7A4-R03`）。** 不透明性只针对
**对外表示的字节格式**，不改变身份域归属：八阶段的**顺序与阶段语义**进入 `EngineIdentity`（judgement
identity 的 engine 组件），**不进入** `ChartIdentity` 或 chart/content identity；重排源数组、改 `sourceMap`、
换表现资源或改 Packed 压缩顺序都**不得**改变 judgement identity（[Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md)
§3.9 的"归属层级"与 §5.2 的同一口径）。

### 域 8：capability、入口与诊断

| 候选类型 | 角色 | 状态 |
| --- | --- | --- |
| `CapabilityId` | 全局唯一稳定 ID（如 `judgement.tap.v1`） | 冻结目标 |
| `CapabilityRevision` | 单调递增；语义变化必须新增 ID 或 revision | 冻结目标 |
| `CapabilityRecord` | registry 条目（`semanticKind` / `requiredFormat` / `staticBudget` / `snapshotCost` / `replayImpact` / `supportedDomains` / `stableRejectCode`） | 待冻结（CM-X01，第 6 轮，与 `Q-15` / `Q-19` 同批；首次消费 S7A-7；附加限定：S7A-1 只登记角色与“阻塞 S7A-7 capability registry 门禁”，不实现字段布局或运行时表面）。**语义文本已由第 6 轮冻结（2026-10-03，`S7A6-R02`）**：七字段与**逐字段 identity 归属**——`semanticKind` / `requiredFormat` / `replayImpact` / `supportedDomains` 进 **semantic projection**；`staticBudget` / `snapshotCost` 进 **closure 与 interchange compatibility**、**不进** judgement semantic hash；`stableRejectCode` **只进诊断码表 / closure**、**不进 identity**（详见 §S7A-7 节与 [Spec §5.6](../formats/GAMEPLAY_V2_SPEC.md)）。**逐字段物理表示与类型布局仍属后续序列化批次 / S7A-9**，故首词保持 `待冻结`） |
| `CapabilityState` | 四态查询：未知 / 已知未启用 / 已启用但预算不足 / 可用 | 冻结目标（第 6 轮确认：§6.3 的四态集合不再扩展；额外两态 `capability.revision_mismatch` / `capability.permanently_unsupported` / `capability.future_development` 仍按 Spec §6.3 单列，不并入四态） |
| `DeclaredCapabilitySet` | source 声明的最低必需集合 | 冻结目标 |
| `DerivedCapabilityClosure` | 离线 assembler 从 canonical graph、Ruleset、presentation closure 派生的完整闭包 | 冻结目标（**第 6 轮，2026-10-03，`S7A6-R02`**：必须与 `compiledSemanticIdentity` **同时携带且可比较**；缺一或不可比较即 prepare 原子失败 `identity_closure_incomplete`。见 §S7A-7 节） |
| `EntryKind` | `packed-chart` / `gameplay-graph` / `author-source` | 冻结目标（7A 允许 `packed-chart` 与 `gameplay-graph` 两种 Playback entry；`author-source` 必须显式非 Playback。Q-07 行为边界见本域"entry 与 section 的行为边界"段。**第 6 轮，2026-10-03，`S7A6-R01`**：入口集合是**闭集**；`author-source` **不是** Playback entry（仅审查 / 迁移 / 复现）；**Ruleset package 不是第三种 entry**；manifest 必须记录 entry kind / compiled semantic identity / artifact identity / Ruleset binding / capability closure / resource·presentation closure / `sourceOf`） |
| `PlaybackEntryRef` | 显式 `playback = true` 的入口引用 | 冻结目标（**第 6 轮，2026-10-03，`S7A6-R01`**：`playback = true` **必须显式携带 `EntryKind`**；缺 `entryKind` 即稳定拒绝，不得推断） |
| `CanvasOfEntry` | 入口载荷承载（Chart v5 JSON 区段 / CXC entry） | 冻结目标 |
| `JudgementError` | 可恢复错误载荷（见 §错误与诊断映射） | 冻结目标（**第 6 轮，2026-10-03，`S7A6-R06`**：诊断载荷的四层结构 = 稳定 `code` + `category`（九类）+ `severity` + `faulted` 行为；`source path` / `identity component` 只作上下文。**码字符串的合法性以集中码表为准**，见下行与 §错误与诊断映射） |
| `DiagnosticCode` | 稳定字符串码 | 待冻结（CM-D04）。**语义文本已由第 6 轮冻结（2026-10-03，`S7A6-R06`）**：**集中码表**固定为 `schemas/cuexis.gameplay-diagnostics.v2.codes.json`，并新增一个 **CTest 校验项**（候选名 `cuexis_gameplay_diagnostics_codes`，由工具侧在 CMake 注册；正式名以工具侧注册为准），校验 code 唯一性、`category` / `severity` / `faulted` 映射完整性、`stableRejectCode` 已登记、以及**输入 / 几何三码不可重复扩展**；**除已冻结的 `input.continuous_unsupported`、`input.direction_unsupported`、`geometry.inference_rejected` 与既有 R-09 映射外，码字符串须先登记并通过该 CTest 校验才能进入公共 ABI**。**逐条码表内容与字符编码表示仍属工具侧落地与后续批次**，故首词保持 `待冻结`） |
| `DiagnosticCategory` | 分类枚举 | 待冻结（CM-D04）。**语义文本已由第 6 轮冻结（2026-10-03，`S7A6-R06`）**：枚举**保持九类**（Spec §9.2），**不新增第十类**；本轮新增的 `presentation-target-missing` / `partial-group` 与本轮复用的 `identity_closure_incomplete` / `invalid_relation` **都映射到既有九类**。**枚举成员的物理编码仍属后续序列化批次**，故首词保持 `待冻结`） |
| `DiagnosticSeverity` | 严重度 | 待冻结（CM-D04）。**语义文本已由第 6 轮冻结（2026-10-03，`S7A6-R06`）**：每条码必须有合法 `severity` 与明确的 `faulted` 行为，二者由集中码表 + CTest 校验项保证完整性。**severity 取值域与逐码判定仍属后续批次**，故首词保持 `待冻结`）。**取值集补齐（第 6 轮 `CM-D04` / `P1-09` 的首次消费后续补齐，决策卡 2026-10-02T21:03:39.462Z，`adopt` 0.96）**：`severity` 为闭集 **`info` / `warning` / `error`**（与 core `DiagnosticSeverity{Info, Warning, Error}` 一一对应，见 [Spec §9.1](../formats/GAMEPLAY_V2_SPEC.md)），7A 已登记的稳定拒绝与原子失败**一律 `error`**；`faulted` 为闭集 **`session_unaffected`** / **`session_faulted`**（`session_faulted` 的唯一承载者是运行期规则集事务失败码 `ruleset.transaction_failed`；其余码一律 `session_unaffected`）。**前两者暂未使用**：`info` 与 `warning` 是闭集成员，7A 已登记码中无一条使用它们 |

**公共诊断码的判据与承载面（2026-10-03，Spec §9.9 的 ABI 侧口径）。** 一条诊断码串是**公共码**，当且仅当
它的**具名码串**、**适用场景**与**九类映射**在正典文本中**可明确相互追溯**——即三者都能在
[Spec §7.2](../formats/GAMEPLAY_V2_SPEC.md)、[Spec §3.7.8](../formats/GAMEPLAY_V2_SPEC.md) 或**本域（域 8）
的具名行**中读到，且彼此**有明确的交叉指向**。**场景行与码串行分处不同章节是允许的**（例如场景行在
Spec §3.7.8、码串行在 Spec §9.3 或本域），只要二者**显式互相指向**。**名称前缀不决定公开性**
（`judgement.s7a2.*` 前缀既不使其私有、也不使其公开），也**不设 owner 命名条件**；无法如此追溯的码串
**不得**作为公共码、只能作 src-only 令牌。公共码**一律**通过**既有**承载面消费：`JudgementError` /
`DiagnosticCode`（本域）与 7A 既有的稳定拒绝载荷；**不新增 ABI 域、不新增类型、不新增错误枚举**
（`DiagnosticCategory` 仍取 Spec §9.2 的九类，`DiagnosticSeverity` 仍取上行的闭集）。
**公共承载面与 src-only 令牌由机器可读表区分**：非空 `publicConsumptionSurface` 只出现在已登记公共码上，
`pendingRegistration[]` 的每一条（13 条 src-only 令牌与 7 条空位）都写 `publicConsumptionSurface: null`；
**公共码 29 条 / src-only 令牌 13 条 / 空位 7 条是三个不同数字**，不得把"字段覆盖 29/29"读成"表内一切都
公开"。逐条的"码 → 来源映射行 → 域 8 承载面"清单见 **Spec §9.9 第 4 条**；机器可读权威为
`schemas/cuexis.gameplay-diagnostics.v2.codes.json`（`codes[].publicConsumptionSurface` /
`pendingRegistration[].publicConsumptionSurface` / `diagnosticStringPolicy` 字段），本节不复制其内容。

**entry 与 section 的行为边界（Q-07，第 3 轮 2026-10-03）。** `playback = true` 必须带明确 `EntryKind`；
Packed `REQ0` / `CNS0` 保持 **Foundation 语义**，7A 新语义必须使用**可与旧语义区分的 entry / section
边界**；未知必需 section、未知 capability 与新 requirement kind 在 ABI 边界上**稳定拒绝**（拒绝载荷走
`JudgementError` 与稳定 `DiagnosticCode`，不静默解释成 tap 或旧语义）。**第 3 轮当时只冻结该行为**；
2026-10-04 的物理合同由 [Gameplay Capsule v2 format](../formats/GAMEPLAY_CAPSULE_V2_FORMAT.md)
限定补齐，语义归 [Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md) §3.8.2。
`EntryKind` 的枚举值仍为候选名，其最终命名
随实现批次与 ABI 一起冻结（§未决项 #3）。

capability 派生的方向与失败规则：

```text
source（Chart v5 / CXT v2）      只声明最低必需 capability
  -> 离线 assembler              派生完整 closure
  -> Packed header               保存排序后的 derived closure
  -> CXC manifest                复制 artifact-required capability
  -> Session                     只报告四态，不改写 graph
```

**声明少于派生 closure 时稳定失败**（第 1 轮 Q-14 Codex 修订）：不允许 compiler 静默补齐；失败时点
固定在最早可确定 closure 不足的编译/装配阶段。该规则的语义正文归 Spec；本 ABI 只固定“派生的
`DerivedCapabilityClosure` 与声明的 `DeclaredCapabilitySet` 是两个可分别查询的值，且不存在把它们
合并的操作”。

### 域 9：Replay、Snapshot 与 Seek

| 候选类型 | 角色 | 状态 |
| --- | --- | --- |
| `ReplayHeader` | `formatVersion`、四分量 identity、normalization profile 元数据、late policy、`eventCodecId`、事件数与字节预算 | 冻结目标（**字段集已由第 5 轮冻结**，CM-K01 / `S7A5-R06`，见 §S7A-6 节；**字节布局待冻结**） |
| `ReplayEventStream` | 规范化 Observation 流（不允许注入 Fact） | 冻结目标 |
| `EventCodecId` | 事件编解码标识 | 待冻结（Q-13，第 5 轮；首次消费 S7A-6；附加限定：S7A5-R06 已冻结其 Replay 标识角色与未知/不匹配门禁语义；具体 codec 编码、字节布局与 reference fixture 由 S7A-6 冻结） |
| `ReplayDecodeBudget` | 解码预算（事件数 / 字节数 / 时间） | 待冻结（Q-13，第 5 轮；首次消费 S7A-6；附加限定：S7A5-R06 已冻结事件数、字节数、解码时间三类预算字段及超限错误语义；预算数值仍阻塞 S7A-9） |
| `SnapshotHeader` | 四分量 identity、semantic revision、state schema revision、事件/Fact 计数、字节预算 | 待冻结（CM-K07 / Q-13，第 5 轮；首次消费 S7A-6；附加限定：S7A5-R06 已冻结四分量 identity、semantic revision、state schema revision、事件/Fact 计数及字节预算字段集；字节布局与预算数值仍待后续批次） |
| `SnapshotPayload` | 全量语义状态（第一版全量，不做增量） | 冻结目标（**字段集与闭包已由第 5 轮冻结**，`S7A5-R07`，见 §S7A-6 节与 Spec §3.18；**逐字段表示待冻结**） |
| `SeekTarget` | 目标 Tick 与允许的误差语义 | 冻结目标 |
| `SeekLatencyCommitment` | `maxSeekLatency` 引擎承诺；会话只能收紧 | 待冻结（CM-K08，第 5 轮，本次登记新增；首次消费 S7A-6；附加限定：S7A5-R08 已冻结类型、承诺语义、会话只能收紧及测量口径入口；具体 maxSeekLatency 数值仍登记为 S7A-9 未冻结预算项，在本轮落地前不进入 Snapshot / Replay 字段、运行时约束或对外承诺，提供具体值稳定拒绝） |

Replay 与 Snapshot 只承载语义状态与规范化事件；表现缓存、Animation layer 临时值和 HostOverride
token 都不保存，Seek/Replay 后由 Fact Ledger 与 FactBinding 重建。只有 Fact Ledger 进
Replay/Snapshot，三类投影事件（RuleEffectEvent / PresentationEvent / EffectEvent）不进。

**第 5 轮在本域冻结的语义（2026-10-03，`S7A5-R06` / `S7A5-R07` / `S7A5-R08`）。** 本域的**字段集与语义
在本轮冻结**，而 `EventCodecId`、`ReplayDecodeBudget`、`SnapshotHeader`、`SeekLatencyCommitment` 四行的
首词仍为 `待冻结`（**字节布局、编码与数值预算不在本轮**，见上表）：

1. **全量 Snapshot header 字段集**：`formatVersion`、四分量 `JudgementIdentity`、`factSemanticRevision`、
   `stateSchemaRevision`、normalized event count、Fact count、payload byte count、typed byte-budget
   descriptor。
2. **Replay header 字段集**：`formatVersion`、四分量 identity、`NormalizationProfile` metadata、effective
   late-policy metadata、`eventCodecId`、normalized event count、encoded byte count、
   `ReplayDecodeBudget`。
3. **拼写规范**：类型 **`EventCodecId`**、字段 **`eventCodecId`**（小写首字母）。本文件内
   `ReplayHeader` 行与 `EventCodecId` 行必须统一到本条。
4. **revision 归属**：`factSemanticRevision` 是 Fact / 排序 / 折叠解释语义、**进 engine identity**；
   `stateSchemaRevision` **只**标识 `SnapshotPayload` 的无损状态结构、**不进** judgement identity。
5. **`SnapshotPayload` 闭包**（十四条：已提交 Fact Ledger 至 cursor 的前缀 + cursor、活动实例、Pattern
   状态、Release / tail phase、exclusive resource 状态、finalization watermark、未提交窗口、
   `preparedGrace`、离散 sequence 状态、Hook snapshot、Fold / Score / Combo / Statistics 状态、pending
   signal queue、`SessionState`、fault 诊断 / 状态）与**不得保存清单**（Presentation / Effect cache、
   Animation 临时值、HostOverride token、未提交 StateDelta、连续采样相位）见 Spec §3.18 第 4、5 条。
   **FactBinding 与 prepared immutable graph** 由 header identity 验证后**重新取得、不重复保存**。
6. **`SeekLatencyCommitment`**（`S7A5-R08`）：带 measurement-profile 引用的 typed engine commitment，
   语义为从最近可用快照恢复并推进到目标的 wall-clock latency；会话只能收紧、不得放宽；测量入口见
   [BUDGET_AND_EVIDENCE_PLAN.md](../proposals/gameplay-v2-acceptance/BUDGET_AND_EVIDENCE_PLAN.md) §5；
   **具体 `maxSeekLatency` 数值稳定拒绝**，seek 仍须满足无损正确性（Spec §3.19）。
7. **typed byte-budget descriptor 不是**已接受的数值承诺；**具体预算数值**（事件数 / 字节数 / 解码时间）
   仍阻塞 **S7A-9**。
8. **`faulted` 与 snapshot**：`faulted` **不可新建 snapshot**（`snapshot` 稳定失败）；快照**闭包仍包含**
   fault 诊断 / 状态，但那不构成"faulted 可产出新快照"（Spec §3.14 第 7 条、§10 第 6 条）。
9. 本域**不新增类型行**：9 域 / **126 = 96 + 30** 不变。

## 所有权与生命周期

### 生命周期动词

typed 边界的会话生命周期沿用 Gameplay I 已冻结的形态，不新增动词：

```text
create configuration
  -> prepare(chart, ruleset, loadout, inputMapping, timebase, judgementConfig)
  -> submit normalized observation
  -> advance to absolute time
  -> read JudgementResult / ScoreState / StatisticsSnapshot
  -> snapshot / seek / replay / reset
```

`prepare` 冻结 Chart、Ruleset、Loadout、InputMapping、Timing 和 JudgementConfig；失败**原子**返回，
不产生半准备会话。运行时不得修改 requirement 集合、`preparedGrace`、仲裁策略或 Interface 投影；
实时输入与 Replay 输入必须走同一 `submit → advance` 路径。

### 所有权规则

| 项 | 规则 |
| --- | --- |
| `GameplaySession`（候选名） | 受 owner-thread 约束；可变状态唯一 owner；不共享给其他线程 |
| prepared 图与 Requirement 记录 | prepare 后只读；由 session 持有，外部只能取得非拥有视图 |
| `FactLedgerView` / Fact 迭代 | 非拥有、只读、生命周期不长于其 session；不得跨 session 持有 |
| `JudgementResult` | 值类型投影，按值返回；不引用 session 内部存储 |
| `SnapshotPayload` | 自持有；可独立校验；恢复不依赖原 session 存活 |
| `ReplayEventStream` | 自持有或由调用方持有的缓冲；解码不执行 IO 或脚本 |
| capability 查询 | 只读；不改写 graph，不改变 session 状态 |
| 跨模块所有权 | 不得跨模块边界 `new` / `delete`；需要共享时用 `shared_ptr`，非拥有观察用裸指针 |
| 实时路径与析构 | 不得抛异常；异常不得跨模块公共边界 |

### 生命周期与失败不变量

1. prepare 失败、事务失败、Replay 解码失败都不得发布半成品，也不得改变已有 active 会话。
2. Ruleset 事务失败时保留旧 state；Fact Ledger 先提交且不可回滚；session 进入可查询 `faulted`。
3. `faulted` 会话只能被显式 `reset` / `reload` 替换；恢复旧快照不得伪造未提交的 `StateDelta`。
4. 时间只能单调推进；Seek/Reload 是显式事务替换，不是隐式回退。
5. 无输入事件时 Judgement 保持休眠，不得改变既有 `FrameSnapshot` / `FrameDigest` / Presentation
   candidate。

## 单位与量程

### 时间单位与表达

- **不使用隐式单位换算。** 边界上不提供“把秒转成 tick”之类的便捷转换；单位由
  `TimebaseProfile`（7A 推荐 `engine.tick.us.v1`）与 Engine Table Registry 声明，不写死在类型名里。
  Stage 7A 推荐 profile 与逐字段边界仍待第 2 轮裁定（CM-T02 / CM-T03 / CM-T04）。
- **`observationTime` 拆成两个字段表达。** 逻辑观测时间用 tick 表达，原始设备/到达时间戳单独保留，
  两者**不得合并成一个字段**，也不得在 ABI 上做隐式换算。判定只使用 tick；原始时间戳只服务诊断、
  迟到策略实现与设备差异分析，不构成第二套时间语义。
- **`error`（`TimingError`）用有符号整数 tick 差。** 语义为 `observationTick - chartTick`；符号表达
  early / exact / late。**窗口语义已由第 4 轮（2026-10-03，`S7A4-R06`）裁定**：Exact **由判定窗口半宽
  定义**（**不要求零 tick**）；窗口外早击**不产生 Fact 但产生诊断**；Miss / absence **由 deadline 产生
  Fact**；Hold 的 head / body / tail **分别记录有符号 error、不合并**。ABI 侧只承载**单位与符号**，
  不重定义窗口语义；语义正文见 [Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md) §3.11。判定窗口的
  **具体半宽数值**不在本批（后续预算 / profile 批次）。
- **`eventSequence`（`EventSequence`）是会话内单调序号。** 它与宿主到达顺序、线程完成顺序和容器
  顺序无关；同一 Tick 的事件顺序由规范排序键决定，不由该序号单独决定。
- **四个时间域不得混用。** `ChartTick` / `ObservationTick` / `CommitTick` 为**有符号 64 位整数**域
  （`judgementTick` 同理；`judgementTick` 是 prepare 期冻结的规范字段名，ABI 侧不另立同义候选类型）；
  `PresentationTime` 允许浮点但只读已提交状态。表现时间不得反向进入判定。
- **Tick 宽度与单位来源已由第 2 轮（2026-10-03）冻结。** 上述五类（`ChartTick` / `judgementTick` /
  `observationTick` / `commitTick`，以及 `TickSpan` / `TickDelta`）均为**有符号 64 位整数**；单位由
  `engine.tick.us.v1` 的 `TimebaseProfile` 声明；**禁止隐式单位换算**；**溢出稳定拒绝**，不得饱和、
  截断或落入未定义行为。最终命名、序列化字节与逐字段布局仍待实现批次与 ABI 一起冻结（§未决项 #3），
  本行的宽度与单位声明不因此延后。语义正文见 [Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md) §3.7.1。
- **`observationTick` 的 canonical 来源固定。** 它唯一来自输入进入判定管线时捕获的**校准后会话时钟**；
  设备时间、宿主到达时间、音频时间与渲染帧时间只保留为诊断上下文；S7C-1 只能扩展 `CalibrationProfile`
  参数，不得替换 canonical source（[Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md) §3.7.3）。
- **负 `observationTick` 合法，只有校准时钟回退被拒（2026-10-03 输入半批复核）。** 有符号 64 位域内的
  **负值合法**并**参与单调排序**，负值本身**不触发任何拒绝码**；**只有校准会话时钟回退**被稳定拒绝，
  且属**时间次序关系错误**，映射 `invalid_relation`（**不是** `budget_exceeded`）。**不可表示的
  calibrated clock 值**复用既有 **tick 域表示失败**（`judgement.s7a2.timebase.tick_overflow` /
  `budget_exceeded`），**不保留**独立令牌；入口违反"先捕获校准会话时钟"的顺序只走既有
  `invalid_relation` 原子失败路径（[Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md) §3.7.3 第 4、5 条、
  §9.3）。
- **`commitTick` 与 commit window。** `commitTick` 是事务提交发生的 Tick；commit window 是相邻两个可
  提交 Tick 之间的观察区间；事实只能在 `commitTick` 提交（[Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md) §3.7.5）。
- **迟到策略参数是 typed 参数，不是常量。** `finalizationWatermark`、queue hop、窗口 open/close 与重复
  排队条件均由 `TimebaseProfile` / ruleset 提供；默认值只在 profile registry 登记为 `pending_measurement`，
  **不得**成为 ABI 常量、隐式零值或研究切片限额；缺失或未测量在 prepare 稳定拒绝，重复排队稳定拒绝并
  返回诊断码（[Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md) §3.7.4）。**具体数值不属本批次。**
  `validatePrepare` **只**检查"已声明且已测量"，**不校验** `finalizationWatermark` / queue hop / 窗口
  开闭阈值之间的量级关系；该量级关系校验登记为**阻塞 S7A-4 的 late-window / 判定消费门禁**，最终数值与
  限额仍由 **S7A-9** 承载（Spec §3.7.4 第 5 条、§3.7.7 表）。
- **Beat → Tick 的 `F()` 与 `stop` 塌缩（第 2 轮经 2026-10-03 实现复核修订）。** `F(originBeat) = 0`、
  `F(endBeat) = F(startBeat) + duration`、`tick(beat) = roundHalfToEven(F(beat))`（**只舍入一次**）；
  `stop` 区间内映射冻结于同一累计值（**非单射**），半开方向为正向 `(originBeat, beat]`、负向
  `(beat, originBeat]`，因此镜像 `stop` 下映射**不再关于 `originBeat` 奇对称**。缺失 `initialTempo` 属
  **结构**类（`invalid_relation`）、已声明非正 tempo 属**数值**类（`budget_exceeded`）；空 `profileId` /
  `unitToken` 表示"未声明"、**不是**默认单位或可用 profile。语义正文见
  [Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md) §3.7.2 与 §9.3。

### 数值域与量程

- 决策侧使用一次性整数投影；禁止未声明的浮点与宿主数学库参与判定。
- 每个判定域必须声明坐标/尺寸量程；每一项判定不等式都必须有不溢出证明。“使用整数”本身不是
  确定性证明。
- 几何平方、乘法、加法与窄化必须有范围证明或显式拒绝；失败映射为稳定诊断而不是未定义行为。
- **本文不登记任何具体量程数值、上限或限额。** 生产限额必须由真实内容测量后单独接受；研究稿的
  切片数值只是量级参考，不得写成 ABI 常量
  （[plan.md §3](../stage_plans/active/stage-07/plan.md)）。
- **每个 `InputDomain` 必须携带 typed `AmountSpec`（第 2 轮 CM-T13，2026-10-03）。** `AmountSpec` 声明
  canonical **整数量化**、**scale**、**可表示范围**与**边界策略**四项；量化使用**精确整数运算**与
  **最近值半数取偶**（round-half-to-even，负值对称，与 Tick 侧 tie rule 同一规则）。
- **canonical 整数宽度已明文冻结（2026-10-03 输入半批复核）。** `AmountSpec` 的 canonical 整数表示为有符号 64 位整数；该宽度是 S7A-2 冻结的规范表示，并与 Tick 的有符号 64 位域一致；任何无法在该域内精确表示的量化、乘法或窄化稳定拒绝。该宽度**不得**靠"复用 §时间单位与表达 的 Tick 宽度"默示，**也不得**借宿主类型扩大该域（正文见 [Spec §3.7.6](../formats/GAMEPLAY_V2_SPEC.md)）。
- **判定顺序：可表示性先于范围（2026-10-03 输入半批复核）。** 先验证 canonical 可表示性、再验证声明
  范围与边界策略：**可表示但越界** → `judgement.s7a2.input.amount_out_of_range` / `budget_exceeded`；
  **根本不可表示** → `judgement.s7a2.input.amount_narrowed` / `budget_exceeded`；`exactNumerator ==
  INT64_MIN` 因 `|INT64_MIN|` 无有符号 64 位表示而按"不可表示"拒绝。两者同属 `budget_exceeded`，
  但**顺序不得颠倒**。
- **越界一律稳定拒绝。** 超范围、窄化（narrowing）与溢出一律**稳定拒绝并返回诊断码**，不得饱和、
  不得截断、不得回落成默认量；拒绝路径复用既有拒绝条目，不新增 R 编号（[Spec §3.7.6、§3.7.8](../formats/GAMEPLAY_V2_SPEC.md)）。
- **本轮不冻结具体业务量程数值或限额。** `AmountSpec` 的类型、**canonical 宽度**、量化规则与越界处置
  已冻结，**数值**登记为后续批次阻塞项，仍由真实内容测量后单独接受（§未决项 #18）。
- **域声明进 session identity（CM-T09，2026-10-03 输入半批复核）。** session identity 分量为
  `profileId`、`profileVersion`、`sourceClass`，**加规范化后的域 token 集合与每项域声明的完整
  `AmountSpec` 字段（`scale` / `minimum` / `maximum` / `boundaryPolicy`）**；域声明顺序不承载语义，
  增删域或改动其 `AmountSpec` 是 identity 变化；**7A 不引入 per-domain 版本字段**（会话级版本由
  `profileVersion` 承载），该冻结**不得**推迟到 S7A-4（正文见 [Spec §5.2](../formats/GAMEPLAY_V2_SPEC.md)）。

## 错误与诊断映射

### 错误通道

- 可恢复错误统一通过 `cuexis::core::Result<T, E>`（`core` 内别名到 `tl::expected`）返回。
- **不得忽略 `Result`**：必须显式处理，或带记录地显式丢弃。
- 不得用异常表达可恢复失败；异常不得跨模块公共边界。
- 宿主回调、析构与实时路径不得抛异常。

### 诊断分层

诊断统一为四层（CM-D04 / P1-09；**第 6 轮已冻结，2026-10-03，`S7A6-R06`**）：

```text
code           稳定字符串码（唯一权威，写入机器可读码表）
category       分类枚举（identity / capability / budget / format / migration / lifecycle ...）
severity       严重度
faulted        失败是否使 session 进入 faulted
```

上例括号内的词（`identity` / `capability` / `budget` / `format` / `migration` / `lifecycle` 等）**仅为码族
分组词**，**不是 `category` 的取值**；`category` 的合法取值只有 [Spec §9.2](../formats/GAMEPLAY_V2_SPEC.md)
的九类，**任何码族分组词都不得作为 `category` 写出**（第 6 轮 `CM-D04` / `P1-09` 首次消费后续补齐，
决策卡 2026-10-02T21:03:39.462Z，`adopt` 0.96；实测反例：共享 capability 拒绝路径曾发 `capability`、
会话生命周期顺序违规曾发 `lifecycle`，二者都必须走九类内的既有类别）。

`source path` 与 `identity component` 是**上下文**，不是稳定码：它们随诊断附带，用于指认字段路径、
Requirement 与 identity 分量，不参与码的等价比较。

**第 6 轮补充（2026-10-03，`S7A6-R06` / `S7A6-R12`）。** 四层结构是**最终结构**，不再变更；
`category` **保持九类**（Spec §9.2），**不新增第十类**；**表现侧失败不置 `faulted`**——
`presentation-target-missing`（纯表现 target 缺失：允许空绑定、丢弃该投影事件）与 `partial-group`
（`groupCommit` 部分提交：已提交部分不回滚、进入稳定拒绝路径）**都不是 `faulted`**，不得阻止后续
`submit` / `advance`，也不得回写 Fact Ledger（分界见 [Spec §9.7 / §9.8](../formats/GAMEPLAY_V2_SPEC.md)）。

**已登记码的逐条定案（第 6 轮 `CM-D04` / `P1-09` 的首次消费后续补齐，决策卡 2026-10-02T21:03:39.462Z，
`adopt` 0.96）。** 下列四条的 `category` / `severity` / `faulted` 与首次消费批次已定案，并已登记在
`schemas/cuexis.gameplay-diagnostics.v2.codes.json`：

| 码 | category | severity | faulted | 首次消费 |
| --- | --- | --- | --- | --- |
| `identity_closure_incomplete` | `identity_closure_incomplete` | `error` | `session_unaffected` | S7A-7 |
| `presentation-target-missing` | `invalid_relation` | `error` | `session_unaffected` | S7A-7 |
| `partial-group` | `invalid_relation` | `error` | `session_unaffected` | S7A-7 |
| `ruleset.transaction_failed` | `invalid_relation` | `error` | **`session_faulted`** | S7A-5 |

`ruleset.transaction_failed` 是运行期唯一 `faulted` 路径（第二阶段 Ruleset state commit 失败）的专用稳定码，
行为见 [Spec §9.4](../formats/GAMEPLAY_V2_SPEC.md)；`presentation-target-missing` 与 `partial-group` 均
**不置 `faulted`**、不回写 Fact Ledger（Spec §9.7 / §9.8）。两条表现码的**码串原样保留**（不加族前缀）。

### 码表位置

诊断的层级与类别由 Spec 拥有（[Gameplay V2 Spec §9](../formats/GAMEPLAY_V2_SPEC.md)）；ABI
只消费它们，不重新定义。**机器可读码表的位置与校验门禁已由第 6 轮冻结（2026-10-03，`S7A6-R06`）**：
固定为 **`schemas/cuexis.gameplay-diagnostics.v2.codes.json`**，并新增**一个 CTest 校验项**（候选名
`cuexis_gameplay_diagnostics_codes`，由工具侧在 CMake 注册；正式名以工具侧注册为准），校验四件事：
① `code` 唯一性；② `category` / `severity` / `faulted` 映射完整性；③ `CapabilityRecord.stableRejectCode`
已登记；④ **输入 / 几何三码不可重复扩展**（`input.continuous_unsupported` /
`input.direction_unsupported` / `geometry.inference_rejected` 各只允许一条）。**逐条码表内容**随各轮裁定
逐条增补，仍属接受后的独立工作项、不属 S7A-0；本节只链接它，不复制内容。**码表文件与校验项本身已落地**
（第 6 轮 `CM-D04` / `P1-09` 的首次消费后续补齐，决策卡 2026-10-02T21:03:39.462Z，`adopt` 0.96）：权威内容
以 `schemas/cuexis.gameplay-diagnostics.v2.codes.json` 为准，可执行门禁为 CTest 校验项
**`cuexis_gameplay_diagnostics_codes`**（脚本 `cmake/VerifyGameplayDiagnosticsCodes.cmake`，可在 CMake 脚本
模式下直接运行）。ABI 侧的约束是：
**除已冻结的 `input.continuous_unsupported`、`input.direction_unsupported`、
`geometry.inference_rejected` 与既有 R-09 映射外**，诊断码字符串必须先在集中码表登记并通过该 CTest
校验，才能进入公共 ABI（含公共头与本节表格）。

### 码族来源（分组，不是最终码表）

以下分组来自已接受的 support/rejection matrix，作为码表的输入，**不是**本文冻结的码表：

- capability 族：`capability.unknown` / `capability.disabled` / `capability.budget_insufficient` /
  `capability.revision_mismatch` / `capability.permanently_unsupported` /
  `capability.future_development`
- 资源与协调族：`resource.capacity_unsupported` / `resource.handoff_unsupported` /
  `coordination.relation_unsupported` / `solver.profile_unsupported` / `pattern.relation_unsupported`
- 输入与几何族：`input.continuous_unsupported` / `input.direction_unsupported` /
  `geometry.inference_rejected`
- Fact 与迟到族：`fact.correction_unsupported` / `late.reopen_unsupported`
- 格式与迁移族：`format.gameplay_version_unsupported` / `migration.ambiguous`
- 会话与生命周期族：`session.mutation_rejected`
- Ruleset 族：`ruleset.package_unsupported`

该段仅为码族分组示例，不是第二套 category 枚举；权威枚举为 SPEC §9.2 九类。

两条硬规则：**禁止静默降级**——未知必需 section、未知 capability、新 requirement kind 一律稳定拒绝，
不得解释成 tap/hold/v4 语义；**拒绝必须给出替代路径**——R-01 至 R-11 类拒绝必须附 capabilityId 与
替代能力。

## 稳定与非稳定声明

### 本阶段（7A）承诺的内容

1. 边界定位：这是内部/preview typed 边界，不是宿主 API，也不是稳定 C ABI。
2. 生命周期动词与所有权规则（见 §所有权与生命周期）：owner-thread、原子 prepare、只读 prepared 值、
   `Result` 错误通道、不抛异常跨界。
3. 单位与表达规则：tick 与原始时间戳分离、`error` 为有符号 tick 差、`eventSequence` 为会话内单调
   序号、不做隐式单位换算；**有符号 64 位 tick 域内的负 `observationTick` 合法并参与单调排序，
   只有校准会话时钟回退映射 `invalid_relation`**（2026-10-03 输入半批复核）。
4. 三条 identity 判定必须分别可表达：semantic / artifact / interchange-compatibility。
5. capability 声明少于派生 closure 时**稳定失败**，不静默补齐。
6. 所有物理入口（Chart v5 JSON 区段与 CXC `gameplay-graph`）恢复**同一** Canonical Gameplay Graph；
   JSON 区段是语义视图，CXC entry 是可选物理 entry，两者可互相替代而非并存语义。
7. 版本层级：Chart 外层 `version = 5` + `gameplay.version = 2`；不引入 `semanticRevision` 作为同义
   替代；旧 `gameplay.version = 1` 只能显式离线迁移或在最早入口稳定拒绝。
8. 明确拒绝面（19 条）具备稳定拒绝路径，不降级。
9. 公共头纯 ASCII 规则适用于将来真正安装的公共头：安装公共头必须纯 ASCII，注释写英文；该规则在
   本次不写头文件时只是被引用的约束条款，不是已交付内容。

### 仍待冻结（不得在实现前声称已定）

| 事项 | 归属 | 依据 |
| --- | --- | --- |
| 最终命名、整数宽度、字段集 | 实现批次与 ABI 一起冻结 | [plan.md §S7A-1](../stage_plans/active/stage-07/plan.md) |
| 所有权与生命周期、异常边界 | 实现批次与 ABI 一起冻结 | 同上（[plan.md §S7A-1](../stage_plans/active/stage-07/plan.md) 列五类：命名、整数宽度、所有权、异常边界、序列化；§3 的所有权陈述是边界约束，不是已冻结的所有权细分） |
| 序列化字节与线格式 | 实现批次 + Packed/CXC candidate wire revision | 同上；[FORMAT_ENTRY_AND_IDENTITY §7](../proposals/gameplay-v2-acceptance/FORMAT_ENTRY_AND_IDENTITY.md#L127-L136) |
| 安装组件归属与 target/allowlist | S7A-1 与实现批次 | [TARGETS_AND_DEPENDENCIES.md](../proposals/gameplay-v2-acceptance/TARGETS_AND_DEPENDENCIES.md) |
| 完整 `IdentityProjection` 表 | 第 1 轮 Q-10 已接受方向，表本身待写 | CM-I06 |
| `TimebaseProfile` 逐字段边界与 tie rule | 第 2 轮**已裁定（2026-10-03）**：宽度为有符号 64 位整数、单位由 `engine.tick.us.v1` 的 `TimebaseProfile` 声明、禁止隐式换算、溢出稳定拒绝；映射与 tie rule 见 Spec §3.7.2；逐字段布局随实现批次 | CM-T03 / CM-T04 |
| `finalizationWatermark`、queue hop、窗口 open/close 状态表 | 第 2 轮**已裁定（2026-10-03）**：7A **不冻结数值**，但**类型与角色已冻结**为 typed 参数，默认值只在 profile registry 登记为 `pending_measurement`，缺失或未测量在 prepare 稳定拒绝；**具体数值**仍属后续批次阻塞项 | CM-T08 / P1-04 |
| `SolverProfile` 默认目标函数清单、`K` / fuel / `max*` 数值与序列化表示 | **第 4 轮已裁定字段语义（2026-10-03）**：字段语义、prepare 拒绝语义与 `objective` / `tieBreak` 有序语义列表已冻结；**默认列表与数值留给 S7A-9**、序列化留给首次消费它的批次 | CM-C06 / CM-C07 / AR P0-03；见 §未决项 #20 |
| 诊断码表（code / category / severity）与机器可读文件位置 | **第 6 轮已裁定（2026-10-03，`S7A6-R06`）**：文件位置固定为 `schemas/cuexis.gameplay-diagnostics.v2.codes.json` + CTest 校验项 **`cuexis_gameplay_diagnostics_codes`**（code 唯一性、category / severity / faulted 映射完整性、`stableRejectCode` 已登记、输入 / 几何三码不可重复扩展）；**码表文件与校验项已落地**，**逐条内容随各轮裁定增补**（第 6 轮首次消费后续补齐，决策卡 2026-10-02T21:03:39.462Z，`adopt` 0.96：新增 `ruleset.transaction_failed` 与 `presentation-target-missing` / `partial-group` 的登记，并定案八条既登记的 category 与六个码的首次消费批次）；`severity` / `faulted` 取值集见 §诊断分层（闭集 `info` / `warning` / `error` 与 `session_unaffected` / `session_faulted`） | CM-D04 / P1-09 |
| `SnapshotHeader` 字段集与 state schema revision | 第 5 轮 | CM-K07 / Q-13 |
| Replay header 字段集与 `eventCodecId` | 第 5 轮 | CM-K01 |
| `outcome` / `category` / `grade` 最小集合 | 第 5 轮 | CM-S10 / P1-07 |
| `candidateRevision` 与 artifact/interchange 的完整兼容矩阵 | 第 1 轮已定方向，矩阵待定义 | Q-01 Codex 修订 |
| 7A 是否含 `cuexis.ruleset` package | 第 5 轮 | CM-S08 / Q-17 |
| 所有预算数值、上限、限额 | S7A-9 前保持未冻结 | A8 / [plan.md §3](../stage_plans/active/stage-07/plan.md) |

### 明确不承诺的内容

- 不承诺稳定二进制兼容、跨工具链替换或跨编译器 ABI。
- 不承诺已发布 SDK 暴露这些类型，也不承诺 `PlaybackSession` 已调用判定内核。
- 不承诺字段宽度、字节布局、错误码数值稳定。
- 不承诺对外 identity 的**内部规范字节格式**（identity 对外不透明；规范字节是 prepare 内部派生物，
  见 [Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md) §5.5）。
- 不承诺任何研究稿数值成为生产限额。
- 不预留运行时脚本、逐帧回调、宿主字节码或动态 Requirement 生成的字段/入口。

## S7A-1 限定冻结范围与登记规则（2026-10-02）

**顶层状态仍为 `candidate`。** 本文不改为 `frozen` / `active`：授权来自**范围精确的限定冻结**，
不来自状态词。按 [Stage 7A 实施计划](../stage_plans/active/stage-07/plan.md) §1.1 冻结顺序第 2 步的
**分批冻结**口径（2026-10-02 owner 接受第 1 轮补充 S1-01…S1-05，登记见 [RULING_WORKSHEET.md](../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) §7A；
该次准入咨询 thread `s7a1-admission-rulings` 记入 [S7A-0 接受记录](../stage_reports/stages/stage-07/2026-10-02-s7a-0-acceptance.md) §4.2），
本文只在下列四项范围内为 S7A-1 骨架提供依据：①权威来源与版本层级；②类型角色与边界；③模块与安装
边界；④既有所有权与异常边界承诺。**共同基线标识**：`RULING_WORKSHEET §7A` 的 `S1-01…S1-05`（[ADR 0044](../adr/0044-gameplay-v2-semantic-kernel.md) 的"接受门禁"第 6 条引用同一编号）。

**范围精确的授权表述。** 在**本节限定的范围内**授权 S7A-1 骨架实施；范围外（含未列出的字段表示、
整数宽度、序列化字节与线格式、枚举集、数值/限额，以及第 6、7 轮裁定中仍被登记为"后续批次"的语义——
第 1–7 轮已全部裁定（无待裁定轮次），上列各项不因裁定状态而进入本节授权范围；同样不在本节范围内）仍不授权。
未决部分只保留"不承诺布局"的不完整类型或纯文档声明，不得用默认值、临时 typedef、序列化编码或
"伪成功"实现绕过（plan §S7A-1，S1-03 / S1-05）。

### 已冻结（本批次）与未冻结（阻塞批次）

| 范围 | 已冻结（本批次可实施） | 未冻结（阻塞批次 / 轮次） |
| --- | --- | --- |
| ① 权威来源与版本层级 | Spec 是字段与运行语义唯一权威；ADR 0044 / Spec / 本文三份互链分工；外层 `version = 5` + `gameplay.version = 2` 唯一组合；旧 `gameplay.version = 1` 只走显式离线迁移或最早入口稳定拒绝；`candidateRevision` 不进 `compiledSemanticIdentity` | Packed / CXC 的 Gameplay Capsule v2 载体已由 2026-10-04 S7A-3 裁决冻结为 `candidateRevision=2`、`GPH0/GPR0/GPD0/GRC0/REF0`；Chart v5 `gameplay` 区段、Replay、Snapshot 与 diagnostics Schema 工件：接受后的独立工作项，不属本批次 |
| ② 类型角色与边界 | §类型清单 9 域 126 条的**角色与边界**（存在性、职责、7A 拒绝路径），以及 §边界声明 的四条边界 | 逐字段表示、字段集、整数宽度与最终命名：未决项 #3（S7A-1 起，与实现批次一起冻结）；枚举集 `InputDomain`（第 6 轮 P2-05）、`Outcome` / `FactCategory` / `Grade`（第 5 轮 CM-S10 / P1-07，**语义已于 2026-10-03 裁定并冻结，集合与规则见 Spec §3.20；字段表示仍待冻结**）、`RegisterKind`（CM-S02，第 5 轮，绑定登记见 §登记缺陷，**三类命名与冲突策略已冻结、编码待冻结**）；数值与限额：未决项 #16（S7A-9 前） |
| ③ 模块与安装边界 | 新建 `engine/judgement/` 内部 STATIC target `cuexis_judgement`；保留 `cuexis_gameplay` stub 且不做别名；本阶段不拆 `cuexis_input`；第一版不安装判定公共头、不进 `CUEXIS_PUBLIC_EXPORT_TARGETS`、不触发 SDK API 升版；`cuexis_judgement` 进 `CUEXIS_STATIC_IMPLEMENTATION_TARGETS`（S1-04，落点 [TARGETS_AND_DEPENDENCIES.md §9](../proposals/gameplay-v2-acceptance/TARGETS_AND_DEPENDENCIES.md)） | target 与 allowlist 的实际落地，以及 active-target 清单、架构检查、静态/shared consumer 与"安装树无判定头"的通过证据：本批次内（未决项 #4）；`tools/gameplay_assembler/` 与 `tools/chart_candidate/` 的实现：后续批次 |
| ④ 所有权与异常边界承诺 | §所有权与生命周期 的既有承诺：owner-thread 唯一 owner、prepare 原子失败、prepared 值只读、非拥有视图、`Result` 错误通道、异常不跨模块公共边界、析构与实时路径不抛异常 | 所有权细分、快照字段表与序列化布局：与实现批次一起冻结；§仍待冻结 表其余各行照旧 |

### 126 条追踪规则

§类型清单 的 9 域共 **126** 条（逐域计数见下表），是 S7A-1 的**完整追踪目录**：每条必须且只能处于
两种登记之一。**登记以状态格首词为准**；括号内的"待冻结"只表示该条的表示细节仍待冻结，不构成
第二种登记，也不重复计入阻塞项。

| 登记 | 判定方式 | 与既有标记的映射 |
| --- | --- | --- |
| 本批次角色/边界冻结 | 状态格首词为 `冻结目标` | `冻结目标`：第 1 轮裁决与已接受的 acceptance package 已确定其存在与语义角色，实现批次必须交付 |
| 后续批次阻塞项 | 状态格首词为 `待冻结` | `待冻结`：类型存在已确定，字段集 / 宽度 / 命名属未决项，随状态格给出的裁决编号进入后续批次；未给出编号者见下节 |

| 域 | 条目数 | `冻结目标` | `待冻结` |
| --- | --- | --- | --- |
| 1 时间基 | 10 | 8 | 2 |
| 2 输入与观测 | 12 | 10 | 2 |
| 3 Requirement、Pattern 与 Measure | 19 | 16 | 3 |
| 4 协调与资源 | 22 | 17 | 5 |
| 5 提交、Fact 与判定结果 | 14 | 11 | 3 |
| 6 Ruleset 事务与会话状态 | 15 | 10 | 5 |
| 7 identity 与兼容判定 | 13 | 11 | 2 |
| 8 capability、入口与诊断 | 13 | 9 | 4 |
| 9 Replay、Snapshot 与 Seek | 8 | 4 | 4 |
| **合计** | **126** | **96** | **30** |

核对方式（可机械复核）：按 `### 域 N` 分段统计表格行，每段截至下一个 `#` 级标题为止（不并入本节
自身的表格），九段行数合计须等于 126；每行状态格首词必须命中 `冻结目标` 或 `待冻结` 之一，
两者之和须等于 126（96 + 30）；不得出现第三种登记或未登记的条目。
**判据**：126 条无遗漏（按域计数可核对）；实际代码涉及的每个字段、签名和行为都必须有已冻结依据。
S7A-1 内只实现已经裁定的接口与状态行为；未决部分只保留"不承诺布局"的不完整类型或纯文档声明。

§未决项 表的 `阻塞` 列登记的是**首次消费批次**：登记为 S7A-1 的 #1–#4 必须在本批次内给出处置
（关闭，或按 S1-05 把受影响的按值字段、运算与可运行方法移出本批次），其中 #3 的逐字段表示与宽度
按本节规则保持未冻结。同一条目在不同表示层可有不同首次消费批次时，`阻塞` 列按段标注（见 #11、#16）；
16 条 `待冻结` 的逐条绑定见 [RULING_WORKSHEET.md §7B](../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md)。

### 登记缺陷：未给出阻塞轮次的 `待冻结` 行（2026-10-03 已关闭）

**关闭记录（2026-10-03）。** 本节曾登记 16 条缺“可核对阻塞轮次”的 `待冻结` 行；该缺陷已于 2026-10-03 由
Codex 裁决一并处置（thread `s7a1-freeze-bindings`，verdict `adopt`，confidence 0.96）：**16 条全部具备
“裁决编号 + 首次消费批次”，未绑定条目为 0**。逐条绑定见
[RULING_WORKSHEET.md §7B](../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) 的 16 行登记表；依据、附加
限定、分组与风险见 [FREEZE_BLOCKING_BINDINGS.md](../proposals/gameplay-v2-acceptance/FREEZE_BLOCKING_BINDINGS.md)
（§3 已改为最终结果、§6 已填 Codex 裁定）。以下内容保留缺陷本体、分类与行号，作为**历史证据**，已随本次绑定关闭。

**缺陷本体（历史）。** 30 条 `待冻结` 中 14 条在状态格内给出裁决编号（13 条已直接绑定轮次/批次：`CM-T08 / P1-04` →
第 2 轮、`CM-F06` → 第 5 轮、`CM-S10 / P1-07` → 第 5 轮、`P2-04` / `P2-06` → 第 6 轮、
`CM-K07 / Q-13` → 第 5 轮、`CM-D04` → 第 6 轮、`CM-I06 / Q-10` 与 `Q-01 Codex 修订` → 第 1 轮
已裁定 / 未决项 #1、#2）。第 141 行 `SolverProfile`（`CM-C06`）是第 14 条：其轮次来自
[OPEN_QUESTIONS.md](../proposals/gameplay-v2-acceptance/OPEN_QUESTIONS.md) 的 Q-03 行（S7A-4，第 4 轮），
而非裁决清单的行内登记。

下列 16 条当时未给出可核对的阻塞轮次，**曾属登记缺陷，登记时未代为补充轮次**（已于 2026-10-03 全部绑定完毕）：

**（a）状态格未给出任何裁决编号（13 条，已关闭）**：第 67 行 `TickSpan` / `TickDelta`；第 85 行 `ContactRef`
（只写"7A 只登记，不做 handoff"）；第 86 行 `DomainAmount`（只写"量程待冻结"）；第 110 行 `Measure`；
第 132 行 `ResourceLease`；第 143 行 `CandidateId`；第 144 行 `LocalOutcome`；第 186 行 `ScoreState`；
第 187 行 `ComboState`；第 188 行 `LifeState`；第 189 行 `StatisticsSnapshot`；第 271 行
`ReplayDecodeBudget`；第 275 行 `SeekLatencyCommitment`（只写"数值待测量"）。

**（b）给出编号，但该编号在裁决清单与合同台账的批次表中未绑定轮次（3 条，已关闭）**：第 182 行
`RegisterKind`（`CM-S02`）；第 237 行 `CapabilityRecord`（`CM-X01`）；第 270 行 `EventCodecId`
（`CM-K01`）。

以上行号以缺陷登记时的版本为准（位于同文件 §类型清单 之内，未因本次绑定改动移位）。预算类行
（第 271、275 行）另有 [§未决项](#未决项) #16 的统一登记：本次绑定后 `ReplayDecodeBudget` 的字段集与
`SeekLatencyCommitment` 的类型 / 语义随第 5 轮冻结，**数值仍留 S7A-9**，与 #16 不冲突。

**跨条目边界（同卡裁决，2026-10-03）。** S7A-4 只产生并消费 `phase`、phase-ordering token、单分量量化
tick error、`hit` / `miss` 及 Hold 的最小 phase-local outcome；`phasePriority` 仅作确定性事实序排序键，
**不是**评分结果；`category`、`grade`、Score / Combo / Life / Statistics 统一由 S7A-5 消费和冻结。

**第 5 轮的跨条目边界（2026-10-03）。** S7A-5 消费 `RegisterKind` 的三类语义与冲突策略、三阶段事务与
`faulted` 行为、Fact 身份生成语义、`Outcome` / `FactCategory` / grade / `TimingError` 与统计 reset / seek /
replay 语义、Life 的关闭与稳定拒绝；S7A-6 消费两套 header 的字段集、`SnapshotPayload` 闭包与不得保存清单、
`SeekLatencyCommitment` 的类型与承诺语义。**两套 header 的字节布局与 codec 编码、`FactId` / `CommitId`
物理编码、`maxSeekLatency` 与全部预算数值**均**不属于**本轮：前者由 S7A-6（首次序列化消费）冻结，
数值记 S7A-9。`P1-14` 的 REF0 / manifest / 诊断归属表由 Spec §3.8.6 承载，**首次消费是 S7A-3**、
entry / manifest 集成是 **S7A-7**，**不阻塞 S7A-5 / S7A-6**。

## S7A-2 限定冻结范围与登记规则（2026-10-03）

**顶层状态仍为 `candidate`。** 本文不改为 `frozen` / `active`：授权来自**范围精确的限定冻结**，不来自
状态词。按 [Stage 7A 实施计划](../stage_plans/active/stage-07/plan.md) §1.1 冻结顺序第 2 步的**分批冻结**
口径，第 2 轮（时间域与迟到策略）于 2026-10-03 由 Codex 裁定并落进本文；该轮是进入 S7A-2 的**准入门禁**。

**provenance。** 决策卡 thread `s7a1-freeze-bindings`，verdict `adopt`，confidence 0.99，日期 2026-10-03，
模型 `gpt-6-astra`；补充确认卡 thread `cm-x01-disposition`，verdict `adopt`，confidence 0.99，日期
2026-10-03（`CM-X01` 保持 `open`，本批次不改任何计数）。**输入规范化半批的语义另经 2026-10-03 两张续裁卡
复核与补充**（同一 thread，verdict 均为 `adopt`，confidence **0.97** / **0.99**，按本地日期 2026-10-03
登记）：`AmountSpec` 的 canonical 整数宽度**明文冻结**为有符号 64 位、判定顺序为**可表示性先于范围**、
`same_tick_collision` / `time_reversal` → `invalid_relation` 与 `amount_narrowed` → `budget_exceeded`
三条原子失败映射、负 `observationTick` 合法且只拒回退、**域声明集合与完整 `AmountSpec` 字段进 session
identity**、不连续表示复用既有 `input.continuous_unsupported`；复核**不改变任何计数**（见本文
`## S7A-2 输入半批冻结补充（2026-10-03）`）。带日期的落地证据见
[第 2 轮门禁报告](../stage_reports/stages/stage-07/2026-10-03-s7a-2-gate-rulings.md)与
[第 2 轮输入半批实现裁定记录](../stage_reports/stages/stage-07/2026-10-03-s7a-2-input-implementation-rulings.md)。

**共同基线标识**：`RULING_WORKSHEET §7A` 的 `S1-01…S1-05`（[ADR 0044](../adr/0044-gameplay-v2-semantic-kernel.md)
的"接受门禁"第 6 条引用同一编号），与 §S7A-1 节共用同一基线，不另立第二套编号。

**范围精确的授权表述。** 在**本节限定的范围内**授权 S7A-2 按第 2 轮裁定实施时间域与迟到策略的 typed
边界；范围外（含具体 late-policy 数值、具体业务量程限额、S7C-1 `CalibrationProfile` 扩展字段、连续输入
能力、序列化字节与逐字段布局、所有预算数值）仍不授权。

| 范围 | 本文中属本批次的部分 | 本批次之外 |
| --- | --- | --- |
| ① Tick 宽度、单位与溢出（Q-04） | §类型清单 域 1 的 `Tick` / `ChartTick` / `ObservationTick` / `CommitTick` / `TimeInterval` / `TickSpan` / `TickDelta` 状态格：**有符号 64 位整数**、单位由 `engine.tick.us.v1` 的 `TimebaseProfile` 声明、禁止隐式换算、溢出稳定拒绝；§单位与量程 的对应条目 | 最终命名、序列化字节与逐字段布局（§未决项 #3，与实现批次一起冻结）；研究切片数值不得成为限额 |
| ② Tick 语义与 tie rule（Q-04 / CM-T03） | §单位与量程 的 tick / 原始时间戳分离与 `error` 为有符号 tick 差；映射与 tie rule 的语义正文在 [Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md) §3.7.2（含 **`F()` 精确累计函数**、`stop` 塌缩与**非单射**、半开跳变方向与"有 stop 时失去奇对称"，2026-10-03 实现复核修订）；域 1 的"第 2 轮已冻结的事实"段 | 映射的代码形状、参考实现与 golden 文件（S7A-2 实现批次）；**Tick → Beat 反查的实现**（登记为首次消费它的后续批次阻塞项） |
| ③ `observationTick` 来源与 S7C-1 边界（Q-04 / CM-T04） | 域 1 `ObservationTick` 状态格与 §单位与量程 的 canonical 来源句：**校准后会话时钟**为唯一 canonical source；设备 / 到达 / 音频 / 渲染帧时间仅诊断上下文；**负值在有符号 64 位域内合法并参与单调排序，只有校准时钟回退映射 `invalid_relation`**；**不可表示的 calibrated clock 值复用 tick 域表示失败（`budget_exceeded`）、不保留独立令牌**（2026-10-03 输入半批复核） | S7C-1 的 `CalibrationProfile` 扩展字段集与逐参数定义（后续批次阻塞项） |
| ④ late policy 类型化（CM-T08 / P1-04） | 域 1 `FinalizationWatermark` / `LateEventPolicy` 状态格：typed 参数、默认值只在 profile registry 记 `pending_measurement`、缺失或未测量在 prepare 稳定拒绝、重复排队稳定拒绝 + 诊断码；**重复排队（`late_policy_incomplete`）与同 Tick 同 canonical 身份重入（`invalid_relation`）分开**（2026-10-03 输入半批复核） | **具体 late-policy 数值**（watermark、queue hop、窗口开闭阈值）：后续批次阻塞项，不得成为 ABI 常量、隐式零值或研究切片限额 |
| ⑤ `commitTick` / commit window（P2-03） | 域 1 `CommitTick` 状态格与 §单位与量程 的定义 | 提交事务的字段布局与 Snapshot / Replay 承载：**承载字段集已由第 5 轮冻结**（Spec §3.18）；**字段布局与字节布局**属 S7A-6 |
| ⑥ `AmountSpec` 与越界处置（CM-T13） | 域 2 `DomainAmount` 状态格与 §数值域与量程：typed `AmountSpec`（canonical 整数量化 / scale / 可表示范围 / 边界策略）、**canonical 整数宽度明文冻结为有符号 64 位**、精确整数运算、最近值半数取偶、**判定顺序＝可表示性先于范围**、超范围 / 窄化 / 溢出稳定拒绝 + 诊断码；**域声明集合与完整 `AmountSpec` 字段进 session identity、域重排不改变 identity、7A 不引入 per-domain 版本字段**（§数值域与量程、域 6 `NormalizationProfile` 行，2026-10-03 输入半批复核） | **具体业务量程数值与限额**：后续批次阻塞项，本轮不冻结；**per-domain 版本字段**不在 7A（会话级版本由 `profileVersion` 承载） |
| ⑦ 来源分工与 F-04 闭合 | 域 2 `SourceClass`、域 7 `SourceBuildIdentity`、"来源类别的分工"段与 §与 Gameplay I ABI 的关系 的字段级映射 `InputEvent.source` 行及 F-04 闭合记录 | 无（F-04 为纯文本缺陷，闭合即完成） |

**S7A-2 不得消费清单（实现须逐条对齐）。** 按 S1-05 的"不得用默认值、临时 typedef、序列化编码或伪成功
实现绕过阻塞"口径，S7A-2 实现**不得消费**：浮点或宿主数学库参与 Tick 判定；隐式秒 / Beat → Tick 转换；
未校准原始时间作为 `observationTick`；按 ingress 顺序解决同 Tick 冲突；late-policy 数值默认常量；饱和或
未定义溢出；未声明的 `DomainAmount` 范围 / 量化；临时整数 typedef；未冻结序列化编码；连续轨迹 /
minimumRate / reconstruction / discontinuity 表示；S7C-1 扩展字段；研究切片限额。逐条判定与证据见
[第 2 轮门禁报告](../stage_reports/stages/stage-07/2026-10-03-s7a-2-gate-rulings.md)。

**§类型清单 的 126 条追踪不受本节影响。** 本节只补状态格内**已冻结的事实**，不改变任何条目的登记首词
（`冻结目标` / `待冻结`），因此 §126 条追踪规则 的逐域计数与 `96 / 30` 合计保持不变。

## S7A-2 输入半批冻结补充（2026-10-03）

**顶层状态仍为 `candidate`。** 本节的授权同样来自**范围精确的限定冻结**，不来自状态词：它登记 S7A-2
**输入规范化半批**在 2026-10-03 实现复核后的语义裁定，**不改写** `## S7A-2` 节的任何登记首词，也不新增
类型条目。

**provenance。** 两张续裁卡同属 Codex 话题 thread `s7a1-freeze-bindings`（与第 2 轮主裁定同 thread，
模型 `gpt-6-astra`，consult），verdict 均为 `adopt`，confidence **0.97** / **0.99**；文件名时间戳为
2026-10-02T20:4xZ（UTC），按第 2、3、4、5 轮先例统一按**本地日期 2026-10-03** 登记。带日期的落地证据见
[第 2 轮输入半批实现裁定记录](../stage_reports/stages/stage-07/2026-10-03-s7a-2-input-implementation-rulings.md)；
逐条落点见 [RULING_WORKSHEET.md](../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) §2、§9 与
[OPEN_QUESTIONS.md](../proposals/gameplay-v2-acceptance/OPEN_QUESTIONS.md) §4.3。

| 范围 | 本文中属本批次的部分 | 本批次之外 |
| --- | --- | --- |
| ① `AmountSpec` 的 canonical 宽度（CM-T13，卡①） | 域 2 `DomainAmount` 状态格与 §数值域与量程：**canonical 整数表示为有符号 64 位整数**、与 Tick 的 64 位域一致、无法精确表示的量化 / 乘法 / 窄化稳定拒绝；**判定顺序＝可表示性先于范围**（`amount_out_of_range` vs `amount_narrowed`，含 `exactNumerator == INT64_MIN` 按不可表示拒绝） | **具体业务量程数值与限额**（后续批次 / S7A-9）；`AmountSpec` 的字段布局与序列化编码（与实现批次一起冻结） |
| ② 三条原子失败映射（卡①②） | §数值域与量程 与 `## S7A-2` 节 ③ / ④ 行：`same_tick_collision` → `invalid_relation`、`amount_narrowed` → `budget_exceeded`、`time_reversal` → `invalid_relation`；**同一 canonical 身份在同一 Tick 再次入场属输入身份 / 次序关系非法**，与重复排队的 `late_policy_incomplete` **分开** | 与迟到策略窗口数值相关的行为（后续批次）；**不新增 ABI 码** |
| ③ 负 tick 与 clock 不可表示（卡①②） | §时间单位与表达、§稳定与非稳定声明 与 `## S7A-2` 节 ③ 行：**负 `observationTick` 合法并参与单调排序**；只有**校准会话时钟回退**被拒（`invalid_relation`）；**不可表示的 calibrated clock 值复用既有 tick 域表示失败**（`budget_exceeded`），**不保留独立令牌**；入口顺序违规只走既有 `invalid_relation` 路径 | S7C-1 的 `CalibrationProfile` 扩展字段（后续批次阻塞项） |
| ④ 域声明进 session identity（CM-T09，卡①②） | 域 6 `NormalizationProfile` 行、域 2 `InputMapping` 行 与 §数值域与量程：identity 分量＝`profileId` / `profileVersion` / `sourceClass` **＋规范化后的域 token 集合与每项域声明的完整 `AmountSpec` 字段**；**重排不改变 identity**，增删域或改其 `AmountSpec` **是** identity 变化；**7A 不引入 per-domain 版本字段**（会话级版本由 `profileVersion` 承载） | 域声明集合的**规范化字节编码**（identity 内部派生物，见 Spec §5.5）；**不得**推迟到 S7A-4 |
| ⑤ 不连续表示与连续能力复用既有码（CM-T10，卡①） | §数值域与量程 与 `## S7A-2` 节 ④ 行：跨 gap / 重连 / 丢样与 trajectory / `minimumReportRate` / reconstruction **都复用**既有 ABI 冻结码 `input.continuous_unsupported`（类别 `capability_disabled`，`capabilityId = input.trajectory.v1`，`remediation = S7B-1`），靠 `field.path`（`continuityCapability` vs `discontinuity`）区分，映射既有 **R-05** | 连续输入能力本身（**S7B-1**）；**不新增 ABI 码、不新增 R 条目** |
| ⑥ 死令牌删除（卡②） | **删除**两个无引用的 7A 令牌（"calibrated clock 非法 / 负值非法"与"observationTick 缺失"）；clock 不可表示统一复用既有 tick 溢出码，负 `observationTick` 不触发任何拒绝码 | 将来若确有独立语义需求，须另立裁决；**本批次不保留替代令牌** |

**计数与登记首词不变（本节的核心声明）。** 本节**只更新既有条目的语义文字**：
9 域 / **126 = 96 + 30** 不变，**未新增类型条目**；§类型清单 各行登记首词（`冻结目标` / `待冻结`）
**一律不动**；§未决项 **不新增编号**（只更新既有 #18）；公共码表**不新增 ABI 码**；R 清单**不新增条目**。

**§类型清单 的 126 条追踪不受本节影响。** 本节只把输入规范化半批已冻结的**语义与身份归属**写入既有条目
与既有段落，因此 §126 条追踪规则 的逐域计数与 `96 / 30` 合计保持不变。

## S7A-3 限定冻结范围与登记规则（2026-10-04 更新）

**顶层状态仍为 `candidate`。** 本文不改为 `frozen` / `active`：授权来自**范围精确的限定冻结**，不来自
状态词。按 [Stage 7A 实施计划](../stage_plans/active/stage-07/plan.md) §1.1 冻结顺序第 2 步的**分批冻结**
口径，第 3 轮（prepare、装配与 entry）于 2026-10-03 由 Codex 裁定并落进本文；该轮是进入 S7A-3 的
**准入门禁**。

**provenance。** 三张决策卡来自同一 Codex 话题 thread `s7a3-prepare-rulings`（模型 `gpt-6-astra`，consult）：
主裁定卡 verdict `need_info`，confidence 0.8；补答 `CM-X05` 的卡 verdict `reject`，confidence 0.88；计数
一致性确认卡 verdict `adopt`，confidence 0.99（`open` 16 / `accept` 36 / `revise` 26，总数仍 114）。三卡的
文件名时间戳为 2026-10-02T19:4xZ（UTC），统一按**本地日期 2026-10-03** 登记（与第 2 轮先例一致）。带日期的
落地证据见 [第 3 轮门禁报告](../stage_reports/stages/stage-07/2026-10-03-s7a-3-gate-rulings.md)。

**共同基线标识**：`RULING_WORKSHEET §7A` 的 `S1-01…S1-05`（[ADR 0044](../adr/0044-gameplay-v2-semantic-kernel.md)
的"接受门禁"第 6 条引用同一编号），与 §S7A-1 / §S7A-2 两节共用同一基线，不另立第二套编号。

**范围精确的授权表述。** 在**本节限定的范围内**授权 S7A-3 按第 3 轮及 2026-10-04 设计裁决实施 prepare、装配与 entry 的
**行为**边界、identity 的**不透明性**声明及 Gameplay Capsule v2 Packed 物理合同；范围外（含
`boundedRelationInstance` 候选表示、identity 内部规范字节的编码实现、§类型清单 各条的逐字段表示，以及
第 6、7 轮裁定中仍被登记为"后续批次"的语义；第 4、5 轮的语义另见 `## S7A-4 限定冻结范围与登记规则（2026-10-03）` 与
`## S7A-5` / `## S7A-6` 两节，**不在本节授权范围内**）仍不授权。

| 范围 | 本文中属本批次的部分 | 本批次之外 |
| --- | --- | --- |
| ① `effects` 不属 Requirement（Q-05） | 域 3 `RequirementRecord` 状态格与"第 3 轮已冻结的事实"段：不含 Gameplay `effects`；非空 `effects` 只能显式 lowering 为不影响 Judgement / Replay 的 Presentation 数据，否则稳定拒绝 | `effects` lowering 表与 Presentation / Effect Graph 的逐字段映射（属 presentation closure 与后续批次） |
| ② `REQ0` / `CNS0` 行为边界（Q-07） | 域 8 `EntryKind` 状态格与"entry 与 section 的行为边界"段；Gameplay Capsule v2 的 revision、section、字段、编码与拒绝矩阵 | `boundedRelationInstance` 候选表示及后续 codec；实现证据仍待 S7A-3 完成 |
| ③ Release / tail 显式声明（Q-18 / P2-10） | 域 3 `Phase` 状态格与"第 3 轮已冻结的事实"段：仅作同一 Requirement 显式声明的可选 phase；未显式声明的 tail 语义要求稳定拒绝 | Release / tail 的 Snapshot / Replay **承载字段集已由第 5 轮冻结**（Spec §3.18、`S7A5-R07`）；**逐字段 phase 表与字节布局**属 S7A-6 |
| ④ 有界循环完全展开（CM-C10 / P1-03） | 域 3 `PatternPrimitive` 状态格与"第 3 轮已冻结的事实"段：prepare-time 完全展开、展开结果唯一决定 identity / snapshot / fact order；`boundedRelationInstance` 不在 7A 合同内、遇则稳定拒绝 | **`boundedRelationInstance` 候选表示**（7B+ / S7C）与具体 `stableRejectCode`：只阻塞首次拟消费它的后续批次 |
| ⑤ identity 不透明性（张力点裁定） | 域 7"对外 identity 的不透明性"段与 §稳定与非稳定声明 的"明确不承诺"清单：对外只以不透明值 + 相等判定暴露，不承诺内部字节格式 | **内部规范字节的编码实现须在 S7A-3 消费前闭合**；语义等价 / 排序 / 身份域边界见 [Spec §5.5](../formats/GAMEPLAY_V2_SPEC.md) |
| ⑥ `CM-X05` 的 D-3 落点（W 类缺口） | 不在本文：字段定义在 [SUPPORT §6](../proposals/gameplay-v2-acceptance/SUPPORT_AND_REJECTION_MATRIX.md)，登记与引用规则在 plan §5.3；`CM-X05` 已由 D-3 落地关闭、不再阻塞 S7A-3 | 具体 W 缺口在**首次消费前**由消费批次补全字段（[SUPPORT §6.1](../proposals/gameplay-v2-acceptance/SUPPORT_AND_REJECTION_MATRIX.md)） |
| ⑦ S7A-3 准入门禁（第 7 轮，`P1-01` / `P1-02` / `P1-15` / `P2-07`） | 语义正文在 [Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md) §3.8.7（判定域 typed contract、CXT local relation 合并规则、Chart v5 inline 与 CXT v2 emission 等价性）与 §5.1.1（`source closure` / `diagnostic map` / `content-artifact identity` 三项命名统一）；本文侧对应域 7 的 `SourceBuildIdentity` / `CompiledSemanticIdentity` / `ArtifactIdentity` 三行语义文字对齐（**只改语义文字，登记首词不变，不新增类型行**） | 动态 frame 判定（7B+）；CXT 合并的编码表示与 semantic diff 的机器格式（属实现与后续批次，diff 的 golden 内容由 S7A-3 产出） |

**S7A-3 不得消费清单（实现须逐条对齐）。** 与 [Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md) §3.8.5
一致：旧 revision 的隐式 V2 解释；旧 `REQ0` / `CNS0` 对新 kind 的兼容解释；未定的新 wire 布局或编号；
Gameplay `effects`（未显式 lowering 者）；隐式 Release / tail；`boundedRelationInstance`；运行期 repeat
计数或动态 Requirement；`sameContact`；连续轨迹；跨 Requirement relation / resource migration；handoff
Hold；接触跟随 Slider；未闭合的数值限额、默认枚举、临时整数 typedef 与公共序列化编码。逐条判定与证据见
[第 3 轮门禁报告](../stage_reports/stages/stage-07/2026-10-03-s7a-3-gate-rulings.md)；第 7 轮的准入门禁
（`P1-01` / `P1-02` / `P1-15` / `P2-07`，`S7A7-R01` / `S7A7-R02` / `S7A7-R06` / `S7A7-R08`）见
[第 7 轮收尾裁定与缺陷处置记录](../stage_reports/stages/stage-07/2026-10-03-s7a-7-wrapup-rulings.md)。

**S7A-3 未闭合合同组合（2026-10-03）。** S7A-3 第二半按
[G2 + R2 + P1 裁定](../stage_reports/stages/stage-07/2026-10-03-s7a-3-contract-selection-g2-r2-p1.md)
推进：G2 解析并冻结 prepare-time `preparedGrace`，R2 生成不可变资源计划并把实际 owner /
lease / contact 分配留给规范运行阶段，P1 用新 candidate revision 与必需 semantic section
组隔离 Gameplay v2。具体 wire 数字、字段布局和编码仍须在首次 Packed 写入前一次性闭合；本记录
不表示实现完成，也不改变状态预算 `INCOMPLETE GATE`。

**§类型清单 的 126 条追踪不受本节影响。** 本节只把第 3 轮已冻结的**行为与不透明性**事实写入既有条目的
状态格内（登记首词不变：`冻结目标` / `待冻结`），因此 §126 条追踪规则 的逐域计数与 `96 / 30` 合计保持不变。

## S7A-4 限定冻结范围与登记规则（2026-10-03）

**顶层状态仍为 `candidate`。** 本文不改为 `frozen` / `active`：授权来自**范围精确的限定冻结**，不来自
状态词。按 [Stage 7A 实施计划](../stage_plans/active/stage-07/plan.md) §1.1 冻结顺序第 2 步的**分批冻结**
口径，第 4 轮（仲裁、资源与事实序）于 2026-10-03 由 Codex 裁定并落进本文；该轮是进入 S7A-4 的
**准入门禁**。

**provenance。** 两张决策卡来自同一 Codex 话题 thread `s7a4-arbitration-rulings`（模型 `gpt-6-astra`，
consult）：主裁定卡 verdict `adopt`，confidence 0.97；处置词与计数确认卡 verdict `adopt`，confidence 0.99
（确认 `CM-C05` / `CM-C09` 均 `open` → `accept`；最终 `open` 14 / `accept` 38；总数仍 114）。两张卡的
文件名时间戳为 2026-10-02T20:0xZ（UTC），统一按**本地日期 2026-10-03** 登记（与第 2、3 轮先例一致）。
带日期的落地证据见 [第 4 轮门禁报告](../stage_reports/stages/stage-07/2026-10-03-s7a-4-gate-rulings.md)。

**共同基线标识**：`RULING_WORKSHEET §7A` 的 `S1-01…S1-05`（[ADR 0044](../adr/0044-gameplay-v2-semantic-kernel.md)
的"接受门禁"第 6 条引用同一编号），与 §S7A-1 / §S7A-2 / §S7A-3 三节共用同一基线，不另立第二套编号。

**范围精确的授权表述。** 在**本节限定的范围内**授权 S7A-4 按第 4 轮裁定实施仲裁、资源与事实序的
类型级事实与边界；范围外（含逐字段表示与序列化字节、`SolverProfile` 默认列表与 `max*` 数值、proof 编码、
`terminal` 编码、后续 `gap` / handoff / `capacity > 1` 语义，以及第 6、7 轮裁定中仍被登记为"后续批次"的语义）仍不授权
（第 5 轮的语义另见 `## S7A-5` / `## S7A-6` 两节，**不在本节授权范围内**）。

| 范围 | 本文中属本批次的部分 | 本批次之外 |
| --- | --- | --- |
| ① 资源身份与 `resourceId` 命名空间（Q-11 / CM-C09） | 域 4 的 `ResourceId` / `ResourceSlot` / `ResourceState` / `ResourceLease` / `ClaimKey` 状态格与"资源身份与 `resourceId` 命名空间"段：命名空间只在 prepared canonical graph 内、唯一 slot、slot / lease / 最小 contact handle / claim identity 进 canonical graph 与 prepared judgement inputs、引擎按规范阶段分配、`contact end` 不复用旧 handle、`observe` 不占用资源 | **各 identity 的字节表示与字段布局**：与实现批次一起冻结（§未决项 #3）；`terminal` 编码见 #20 |
| ② 唯一六步 / 八阶段映射（Q-16 / CM-C05） | 域 4"同 Tick 阶段的唯一映射"段：六步＝外层、八阶段＝第 4 步内部完整展开；阶段名称 / 顺序 / 映射 / 语义边界已冻结 | **阶段编号 / 序列化编码 / 预算 / 窗口数值**：首次消费它们的后续批次（#20）；Fact 因果总序的**语义**已由第 5 轮冻结（`S7A5-R01`，Spec §3.9 第 3 条、§3.17），**字节合同**属 **S7A-6** |
| ③ 归属层级（Q-16） | 域 7"不透明性的范围不扩到 chart/content identity 的归属"段与 `EngineIdentity` 状态格：八阶段顺序与语义进 judgement identity 的 engine 组件，不进 chart/content identity | chart/content identity 的逐字段归属按既有投影表推进 |
| ④ solver / coordinator 边界（Q-03 / CM-C06 / CM-C07） | 域 4 `SolverProfile` 状态格与"solver / coordinator 的命名边界"段：prepare/compile solver 负责展开 / 验证 / 上界证明 / 唯一性证明并生成 prepared profile；**不解析/编译 solver；runtime coordinator 只执行已准备的确定性 policy**（`P2-02` 措辞，第 7 轮 `S7A7-R07` 收官，2026-10-03）；名称采用 `coordinator.policy.greedy_v1`；`SolverProfile` **字段语义**、**prepare 拒绝语义**与 `objective` / `tieBreak` **有序语义列表**已冻结 | **默认列表、`K` / fuel / `max*` 数值**（S7A-9）、**序列化表示**与**唯一性证明的 proof 编码**（首次消费它们的批次）：§未决项 #20 |
| ⑤ 早 / 晚判定与 error（P1-12） | §单位与量程 的 `error` 条：Exact 由窗口半宽定义、窗口外早击不产生 Fact 但产生诊断、Miss / absence 由 deadline 产生 Fact、Hold head/body/tail error 分别记录不合并（ABI 只承载单位与符号） | **判定窗口半宽的具体数值**：后续预算 / profile 批次（§未决项 #20） |
| ⑥ 拒绝面（§错误与诊断映射） | 域 4 的 R-01 / R-04 / R-07 映射不变；五类第 4 轮拒绝**不新增 R 编号**，清单仍为 19 条 | 新增 R 编号必须另立裁决 |

**S7A-4 不得消费清单（实现须逐条对齐）。** 与 [Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md) §3.13 一致：
runtime solver 或 global solver；`max_cardinality`；bounded backtracking；`capacity > 1`；Chord / `binding`；
`gap` / handoff；`sameContact`；连续轨迹（含 Slider continuity）；非零资源 `declaredGapGrace`；`boundedRelationInstance`；
运行时脚本 / IO / 随机 / 墙钟；默认数值限额；未冻结枚举或整数 typedef；未获限定授权的 runtime/proof/identity 序列化字节布局（已授权 Capsule 静态输入除外）；把 `observe`
解释成占用资源的实现。逐条判定与证据见
[第 4 轮门禁报告](../stage_reports/stages/stage-07/2026-10-03-s7a-4-gate-rulings.md)。

**§类型清单 的 126 条追踪不受本节影响。** 本节只把第 4 轮已冻结的**字段语义与行为边界**事实写入既有条目的
状态格内（登记首词不变：`冻结目标` / `待冻结`），因此 §126 条追踪规则 的逐域计数与 `96 / 30` 合计保持
不变——**本条即"本轮不新增 ABI 条目"的落点**：9 域 / **126 = 96 + 30** 不变。

## S7A-5 限定冻结范围与登记规则（2026-10-03）

**顶层状态仍为 `candidate`。** 本文不改为 `frozen` / `active`：授权来自**范围精确的限定冻结**，不来自
状态词。按 [Stage 7A 实施计划](../stage_plans/active/stage-07/plan.md) §1.1 冻结顺序第 2 步的**分批冻结**
口径，第 5 轮（Ruleset、事实序、Score 与 Snapshot）于 2026-10-03 由 Codex 裁定并落进本文；该轮与
`## S7A-6` 共为进入 S7A-5 / S7A-6 的**准入门禁**。

**provenance。** 三张决策卡来自同一 Codex 话题 thread `01a0fe49-cfe5-7933-8d03-3d0a65d34c8b`（consult）：
①主裁定卡 verdict `adopt`，confidence 0.95（`S7A5-R01…R11`；`S7A5-R10` 只用于 `LifeState`，`P1-14` 另登记
为 `S7A5-R11`）；②处置词与计数确认卡 verdict `adopt`，confidence 0.99（`CM-F06`、`CM-S08`、`CM-S10`、
`CM-K07` 四条 `open` → `accept`；最终 `open` 10 / `accept` 42；§0.1 末列仍为 21；`CM-S11`、`CM-K08`
**不改变任何计数**）；③ABI 状态格首词与追踪计数追问卡 verdict `adopt`，confidence 0.98（9 行状态格首词
**全部保持 `待冻结`**；追踪计数 **126 = 96 + 30** 不变；**不新增 §未决项条目、不新增 §类型清单条目**）。
三张卡的文件名时间戳为 2026-10-02T20:2x–20:3xZ（UTC），统一按**本地日期 2026-10-03** 登记。带日期的落地
证据见 [第 5 轮门禁报告](../stage_reports/stages/stage-07/2026-10-03-s7a-5-6-gate-rulings.md)。

**共同基线标识**：`RULING_WORKSHEET §7A` 的 `S1-01…S1-05`（[ADR 0044](../adr/0044-gameplay-v2-semantic-kernel.md)
的"接受门禁"第 6 条引用同一编号），与 §S7A-1 / §S7A-2 / §S7A-3 / §S7A-4 四节共用同一基线，不另立第二套编号。

**范围精确的授权表述。** 在**本节限定的范围内**授权 S7A-5 按第 5 轮裁定实施 Ruleset 折叠与 Score / Combo /
Statistics 的**语义边界**；范围外（含逐字段表示与序列化字节、全部预算与限额数值、`maxSeekLatency` 数值、
package 形状与迁移、`P1-14` 的 REF0 物理字段，以及第 6、7 轮裁定中仍被登记为"后续批次"的语义）仍不授权（S7A-6 的字段集
与闭包另见 `## S7A-6`，**不在本节授权范围内**）。

| 范围 | 本文中属本批次的部分 | 本批次之外 |
| --- | --- | --- |
| ① 三阶段与 `faulted`（Q-08 / `S7A5-R03`） | 域 6 的原子性规则与"第 5 轮在本域冻结的语义"第 1、5 条；`SessionState` 状态格（含 `faulted`）的行为以 Spec §3.14 / §9.4 为准 | 序列化编码与诊断码表汇总（第 6 轮）；快照字节布局属 S7A-6 |
| ② `RegisterKind` 三类（CM-S02 / `S7A5-R04`） | 域 6 `RegisterKind` 状态格的附加限定与同段第 2 条；7A 只接受三类、其他 kind 稳定拒绝 | **编码与布局**（首次消费它的后续批次）；状态格首词保持 `待冻结` |
| ③ 内置 Ruleset 边界（Q-17 / CM-S08 / `S7A5-R05`） | 域 6 `RulesetInterfaceRef` / `RulesetManifest` 不来自外部 package；package 输入命中 R-09（Spec §3.16、§7.2） | **package 形状 / identity / 完整性 / 迁移**归 S7C-2；**不新增 R 编号** |
| ④ Fact 身份与总序（Q-12 / CM-F06 / `S7A5-R01` + `S7A5-R02`） | 域 5 的 `FactRecord` / 域 7 的 identity 归属段；生成语义见 Spec §3.9 第 3 条与 §3.17 | **物理字节布局、varint / endianness、`FactId` / `CommitId` 编码与 byte fixture** 属 **S7A-6**（§未决项 #10） |
| ⑤ Outcome / category / grade / error（CM-S10 / P1-07 / `S7A5-R09`） | 域 5 的 `Outcome` / `FactCategory` 状态格附加限定、`TimingError` 的"有符号整数 tick 差"角色，以及域 6 同段第 3 条 | 字段表示、溢出 / 饱和与编码（后续批次）；数值限额记 S7A-9 |
| ⑥ Life 关闭（CM-S11 / `S7A5-R10`） | 域 6 `LifeState` 状态格的附加限定与同段第 4 条；`capability.disabled` 稳定拒绝 | `LifeState` 的类型表示与编码（后续批次） |
| ⑦ 不得消费清单 | Spec §3.22 的 S7A-5 清单（8 项），逐条对齐 | 清单内每一项都不得在本批次被消费或绕过（S1-05 口径） |

**S7A-5 不得消费清单（实现须逐条对齐）。** 与 [Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md) §3.22 一致：
ingress 排序（含 `ingressSequence`、容器顺序与线程完成顺序）；correction；Ruleset package（含 hash / manifest /
迁移矩阵）；Life（capability / policy / `LifeState` 初值）；默认 grade 表与默认数值限额；未冻结的字节编码；
未测量的 seek 数值；表现资源 / UI / 默认绑定对 judgement 的影响。另：**不得把 `phasePriority` 用作评分**。
逐条判定与证据见 [第 5 轮门禁报告](../stage_reports/stages/stage-07/2026-10-03-s7a-5-6-gate-rulings.md)。

**§类型清单 的 126 条追踪不受本节影响。** 本节只把第 5 轮已冻结的**语义与行为边界**写入既有条目的状态格
（登记首词不变：`冻结目标` / `待冻结`），并**未新增任何类型行或 §未决项条目**：9 域 / **126 = 96 + 30** 不变。

## S7A-6 限定冻结范围与登记规则（2026-10-03）

**顶层状态仍为 `candidate`。** 授权来自**范围精确的限定冻结**，不来自状态词。第 5 轮于 2026-10-03 由 Codex
裁定并落进本文；本条与 `## S7A-5` 共用同一 baseline、同一三张决策卡的 provenance（见上一节），不重复全段。

**范围精确的授权表述。** 在**本节限定的范围内**授权 S7A-6 按第 5 轮裁定实施 Snapshot / Replay / Seek 的
**字段集与语义**（两套 header 字段集、`SnapshotPayload` 闭包、不得保存清单、`SeekLatencyCommitment` 的类型与
收紧关系、`faulted` 与 snapshot 的关系）；范围外（含两套 header 的**字节布局与编码**、Event / Fact codec
编码、`FactId` / `CommitId` 物理编码、全部预算数值与 `maxSeekLatency` 数值、连续重采样格点与采样相位，以及
第 6、7 轮裁定中仍被登记为"后续批次"的语义）仍不授权。

| 范围 | 本文中属本批次的部分 | 本批次之外 |
| --- | --- | --- |
| ① Snapshot header 字段集（CM-K07 / `S7A5-R06`） | 域 9 `SnapshotHeader` 状态格的附加限定与"第 5 轮在本域冻结的语义"第 1 条 | 字节布局、字段顺序、对齐与编码；预算数值记 S7A-9 |
| ② Replay header 字段集（CM-K01 / Q-13 / `S7A5-R06`） | 域 9 `ReplayHeader` 行与同段第 2、3 条；**拼写规范：类型 `EventCodecId`、字段 `eventCodecId`** | codec 编码与字节布局；`ReplayDecodeBudget` 数值记 S7A-9 |
| ③ `SnapshotPayload` 闭包与不得保存（`S7A5-R07`） | 域 9 `SnapshotPayload` 行与同段第 5 条；Spec §3.18 第 4、5 条 | 逐字段表示（与实现批次一起冻结）；增量 / 压缩 / 跨 minor 迁移在无全量 golden 前不进合同 |
| ④ revision 归属（`S7A5-R06`） | 域 7 的 identity 归属段与同段第 4 条：`factSemanticRevision` 进 engine identity、`stateSchemaRevision` 不进 | 两个 revision 的取值与编码（后续批次） |
| ⑤ `SeekLatencyCommitment`（CM-K08 / `S7A5-R08`） | 域 9 `SeekLatencyCommitment` 状态格的附加限定与同段第 6 条 | **具体 `maxSeekLatency` 数值**记 S7A-9（§未决项 #16） |
| ⑥ `faulted` 与 snapshot（`S7A5-R03`） | 同段第 8 条：**`faulted` 不可新建 snapshot**；快照闭包仍**包含** fault 诊断 / 状态 | 恢复事务的具体表示与诊断码表（第 6 轮） |
| ⑦ 不得消费清单 | Spec §3.22 的 S7A-6 清单（S7A-5 的 8 项 + 另 5 项），逐条对齐 | 清单内每一项都不得在本批次被消费、保存或绕过 |

**S7A-6 不得消费清单（实现须逐条对齐）。** 与 [Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md) §3.22 一致：
上节 S7A-5 的 8 项**全部适用**，另加：保存表现缓存 / Animation 临时值 / HostOverride token；保存未提交
delta；保存或恢复连续采样状态（连续重采样格点与采样相位）；保存 snapshot interval identity；把 typed
byte-budget descriptor 解释为**已接受的数值承诺**。逐条判定与证据见
[第 5 轮门禁报告](../stage_reports/stages/stage-07/2026-10-03-s7a-5-6-gate-rulings.md)。

**§类型清单 的 126 条追踪不受本节影响。** 本节同样只更新既有条目的语义与附加限定，**未新增类型行**：
9 域 / **126 = 96 + 30** 不变（见 §126 条追踪规则）。

## S7A-7 限定冻结范围与登记规则（2026-10-03）

**顶层状态仍为 `candidate`。** 授权来自**范围精确的限定冻结**，不来自状态词。第 6 轮于 2026-10-03 由
Codex 裁定并落进本文。provenance：Codex 话题 thread `01a0fe61-3476-7882-9674-5f6b05035237`（consult，
模型 `gpt-6-astra`），单卡 verdict **`adopt`**、confidence **0.97**；第 6 轮 **15 行登记 / 按项计 17 项**，
编号 `S7A6-R01…R12`（编号到行的映射由本 package 推导得出，**已由口径确认卡确认**
（thread `01a0fe61-3476-7882-9674-5f6b05035237`，verdict `adopt`、confidence **0.99**，2026-10-03；
**第 7 轮复核**），见
[RULING_WORKSHEET.md](../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) §6）。卡的文件名时间戳为
2026-10-02T20:51:47.167Z（UTC），按**本地日期 2026-10-03** 登记（与第 2、3、4、5 轮先例一致）。带日期落地
证据见 [第 6 轮门禁报告](../stage_reports/stages/stage-07/2026-10-03-s7a-6-gate-rulings.md)。

**共同基线标识。** 本节与 `## S7A-1` … `## S7A-6` 各节共用 `RULING_WORKSHEET §7A` 的 `S1-01…S1-05`
基线标识（[ADR 0044](../adr/0044-gameplay-v2-semantic-kernel.md) 的"接受门禁"第 6 条引用同一编号），
**不另立第二套编号**。

**范围精确的授权表述。** 在**本节限定的范围内**授权后续实现按第 6 轮裁定实施：发布入口与 manifest 七项
字段清单（域 8 的 `EntryKind` / `PlaybackEntryRef` 行与 [Spec §3.6 / §3.23](../formats/GAMEPLAY_V2_SPEC.md)）、
capability registry 的**语义文本**与逐字段 identity 归属（域 8 `CapabilityRecord` / `DerivedCapabilityClosure`
行与 Spec §5.6 / §6.4）、诊断的**四层结构**与集中码表 + CTest 校验门禁（域 8 `JudgementError` /
`DiagnosticCode` / `DiagnosticCategory` / `DiagnosticSeverity` 行与 §错误与诊断映射）、`P1-08` 的单位与
表达规则（§单位与量程）、以及 `P2-04`…`P2-09` 的 Replay / Snapshot 归属（域 9 与 Spec §3.25）。
**范围外**（含：逐字段物理表示与类型布局、整数宽度、序列化字节与线格式、枚举成员的编码、severity
取值域与逐码 `faulted` 判定、逐条码表内容、具体预算数值与 `maxSeekLatency` 数值、Ruleset package 形状 /
identity / 迁移、Presentation 缓存与 HostOverride token 的 snapshot 持久化、未提交 delta 与连续采样相位、
运行时脚本 / 逐帧回调、Life capability 与任何第二判定路径，以及**第 7 轮已裁定但需在各自批次实施的语义**）
仍不授权。

| 范围 | 本文中属本批次的部分 | 本批次之外 |
| --- | --- | --- |
| ① 发布入口与 manifest（`S7A6-R01`） | 域 8 `EntryKind` / `PlaybackEntryRef` 行的第 6 轮补充；`author-source` 非 Playback entry、Ruleset package 非第三种 entry、manifest 七项字段清单 | manifest 逐字段的物理编码与预算数值（后续序列化批次 / S7A-9） |
| ② identity 与闭包携带（`S7A6-R02`） | 域 8 `CapabilityRecord` / `DerivedCapabilityClosure` 行的第 6 轮补充；Spec §5.6 的七字段逐字段归属 | 字段的物理表示、类型布局与整数宽度（实现批次）；具体预算数值 |
| ③ 诊断四层与码表（`S7A6-R06`） | 域 8 `JudgementError` / `DiagnosticCode` / `DiagnosticCategory` / `DiagnosticSeverity` 行的第 6 轮补充；§错误与诊断映射的码表位置段；Spec §9.1 / §9.2 / §9.6 | 逐条码表内容、severity 取值域、逐码 `faulted` 判定、码字符串的字符编码（工具侧落地 / 后续批次） |
| ④ 表现失败与 `faulted` 的分界（`S7A6-R04` / `S7A6-R05`） | §错误与诊断映射"第 6 轮补充"段：`presentation-target-missing` / `partial-group` 均**不置 `faulted`**、不回写 Fact Ledger | 表现 token 的宿主表示与快照持久化（明确禁止） |
| ⑤ `P1-08` 单位与表达（`S7A6-R07`） | §单位与量程的既有三条规则（tick 与原始时间戳分离、`error` 为有符号 tick 差、`eventSequence` 为会话内单调序号、无隐式换算） | 字段的物理类型与宽度（实现批次） |
| ⑥ Replay / Snapshot 归属（`S7A6-R11`） | 域 9 段的"只有 Fact Ledger 进 Replay/Snapshot，三类投影事件不进" | 三类投影事件的宿主表示（不进合同） |
| ⑦ 不得消费清单 | 下表 11 项，逐条对齐 | 清单内每一项都不得在本批次被消费、保存或绕过 |

**S7A-7 不得消费清单（表示与行为）。** S7A-7 **不得**消费：① 具体预算数值；② `maxSeekLatency`
具体数值；③ 未冻结的 wire / codec 字节编码；④ 未冻结的 `FactId` / `CommitId` 字节编码；⑤ Ruleset
package 形状 / identity / 迁移；⑥ Presentation 缓存或 HostOverride token 的 snapshot 持久化；
⑦ 未提交 delta；⑧ 连续采样相位；⑨ 运行时脚本 / 逐帧回调；⑩ Life capability 或任何第二判定路径；
⑪ 把语义字段的"未知"降级为默认值。该清单与第 2 / 4 / 5 轮清单**不冲突**（不重复、不放松）。

**后续批次阻塞项（本轮登记，不属于 S7A-7 门禁）。**

| 阻塞项 | 首次消费批次 |
| --- | --- |
| 诊断码表的**逐条内容继续增补**、`severity` 取值域与逐码 `faulted` 判定 | 权威内容已在 `schemas/cuexis.gameplay-diagnostics.v2.codes.json`（第 6 轮首次消费后续补齐，决策卡 2026-10-02T21:03:39.462Z）；新增码仍须经登记 + CTest 校验项 `cuexis_gameplay_diagnostics_codes` |
| 全部类型行的**逐字段物理表示、整数宽度与序列化字节** | 实现批次（与 ABI 一起冻结，§未决项 #3） |
| `stableRejectCode` 与 `CapabilityRecord` 具体预算 / 快照成本数值 | **S7A-9**（测量后单独接受） |
| `cuexis.ruleset` package 形状 / identity / 迁移 | **S7C-2**（第 5 轮已定方向） |

**§类型清单 的 126 条追踪不受本节影响。** 本节只更新**既有条目**的语义文本与附加限定，**未新增类型行、
未新增 §未决项 条目**：9 域 / **126 = 96 + 30** 不变（见 §126 条追踪规则）。第 6 轮五条合同项
`CM-P04`、`CM-P06`、`CM-P08`、`CM-D04`、`CM-X01` 的处置词由 `open` 改为 **`accept`**；**第 7 轮**
（2026-10-03）再把残余五条 `CM-V06`、`CM-V07`、`CM-V13`、`CM-I06`、`CM-X06` 按第 1 轮裁定补齐处置词
（`open` → `accept`），合同台账最终 **`accept` 52 / `open` 0**，总数仍 114，但这**不改动本文的域数与类型行数**。

**表述纪律。** 本节冻结的是**文档合同与门禁**；**不得**把本轮写成"Judgement 实现完成"或
"Stage 7A 完成"，也**不得**据本节声称 hosted / GPU / 真实设备 / 音频证据已通过。

## S7A-7 之后的收尾：第 7 轮裁定与无待裁定轮次（2026-10-03）

**provenance。** 第 7 轮（收尾澄清与缺陷）由 Codex 主卡 thread `01a0fe61-b7a3-7853-a380-0793fee14e14`
（consult，模型 `gpt-6-astra`，verdict `adopt`、confidence **0.96**）与口径确认卡 thread
`01a0fe61-3476-7882-9674-5f6b05035237`（verdict `adopt`、confidence **0.99**）裁定，2026-10-03。逐条登记见
[RULING_WORKSHEET.md](../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) §7–§9；带日期证据见
[第 7 轮收尾裁定与缺陷处置记录](../stage_reports/stages/stage-07/2026-10-03-s7a-7-wrapup-rulings.md)。
本轮共 **16 行 / 18 项**（`S7A7-R01…R16`）。

**本文受影响的三项。**

1. **`P2-02` 措辞（`S7A7-R07`，首次消费 S7A-4）**：域 4"solver / coordinator 的命名边界"段与 §S7A-4 节
   ④ 已统一为"**不解析/编译 solver；runtime coordinator 只执行已准备的确定性 policy**"；
   **状态格首词不变**（`冻结目标` / `待冻结` 一律不动）。
2. **`P2-07` 三项命名统一（`S7A7-R08`，首次消费 S7A-3）**：域 7 的 `SourceBuildIdentity` /
   `CompiledSemanticIdentity` / `ArtifactIdentity` **三行语义文字**对齐到 `source closure` /
   `diagnostic map` / `content-artifact identity`（定义见 [Spec §5.1.1](../formats/GAMEPLAY_V2_SPEC.md)）；
   **保留三行、不新增类型行**，9 域 / **126 = 96 + 30** 不变；`IdentityProjection` 与
   `InterchangeCompatibility` 状态格的**首词仍为 `待冻结`**，只在语义文字里补齐裁定。
3. **`D-6` 行号订正（`S7A7-R11`）**：`tools/check_stage6_a2.py` 的**实际行号为 `:259`**（历史报告中
   写作 `:252`）。**本文不改写任何带日期的历史报告正文**，只在此处登记订正后的行号作为引用依据。

**`D-11` 的最小登记（`S7A7-R13`）。** 稳定 C ABI **唯一归属 Stage 14**（见本文 §边界声明 第 3 条与
[plan.md §10](../stage_plans/active/stage-07/plan.md)）；`AGENTS.md` 第 21、260 行的 Stage 12 是**孤例**，
**本轮不改 `AGENTS.md`**，由 owner 择时订正。同一句话另写入 [CURRENT_STATUS.md](../CURRENT_STATUS.md)、
[ROADMAP.md](../ROADMAP.md) 与 [ADR 0043](../adr/0043-gameplay-judgement-ruleset-convergence.md)。

**`D-9`（owner-only）与 `D-12`（待 owner 确认）。** `D-9` 的候选补丁已在工作区且**未提交**，须由 owner 按
ADR 0042 具名复核后落到 `master`；**在该动作完成前不得宣称 S7A-8 的版本门禁闭合**。`D-12` 只登记、**不编辑**
研究稿的 8 处指向行，处置文本为"改指 ADR 0044 / V2 Spec / V2 ABI 并注明 Gameplay I 已 `superseded`，不得改写
论证正文，**须先取得 owner 对历史稿修改的确认**"。

**第 7 轮后无待裁定轮次（计数不变声明）。** 五条残余 `open` 合同项（`CM-V06`、`CM-V07`、`CM-V13`、
`CM-I06`、`CM-X06`）按第 1 轮裁定补齐处置词与裁定正文，`open` 集合为空；合同台账最终
**`accept` 52 / `revise` 26 / `supersede` 17 / `retain` 17 / `open` 0 / `reject` 2 = 114**。
**行 / 项口径**：第 7 轮 = **16 行 / 18 项**（合并行 `D-2 / D-7` 与 `D-13` 的 `AGENTS.md`:110 附带子项
各记 2 项）；逐轮合计 **76 行 / 84 项**，与工作表 §0.1 一致；五条 `CM-V*` 的补齐**只改处置词与裁定正文**、
**不占** `S7A7-R` 编号。
本文**不新增** §未决项条目、不新增类型行；9 域 / **126 = 96 + 30** 不变；登记首词一律不变。**本轮只冻结
文档合同与门禁**——**不得**写成"Judgement 实现完成"或"Stage 7A 完成"，实现批次 S7A-3…S7A-9 仍未完成。

**第 6 轮 `CM-D04` / `P1-09` 首次消费的后续补齐（决策卡 2026-10-02T21:03:39.462Z，thread
`01a0fe6b-6e57-7051-87f8-cf9588e85568`，verdict `adopt`、confidence **0.96**）。** 该后续裁决只处理
**诊断分类学**，**不新增类型条目**（9 域 / **126 = 96 + 30** 不变）、**不新增 §未决项编号**、**不新增 R 条目**
（§7.2 仍 19 条）、**不新增类别**（§9.2 仍九类）。本轮在本文落地的三处：

1. **九类为唯一权威**：§错误与诊断映射正文把 `input.continuous_unsupported` 的类别由字面 `capability`
   订正为 **`capability_disabled`**；§码族来源 段追加"该段仅为码族分组示例，不是第二套 category 枚举；
   权威枚举为 SPEC §9.2 九类"。
2. **`severity` / `faulted` 取值集**：§诊断分层 与域 8 `DiagnosticSeverity` 行写明闭集
   `info` / `warning` / `error` 与 `session_unaffected` / `session_faulted`（`session_faulted` 的唯一承载者
   是运行期事务失败码 `ruleset.transaction_failed`）。
3. **已登记码的逐条定案**：§错误与诊断映射新增四行登记表（`identity_closure_incomplete`、
   `presentation-target-missing`、`partial-group`、`ruleset.transaction_failed`）与首次消费批次；§仍待冻结
   行、§S7A-7 节的后续阻塞项行与 §码表位置 段改为指向**已落地**的
   `schemas/cuexis.gameplay-diagnostics.v2.codes.json` + CTest 校验项 `cuexis_gameplay_diagnostics_codes`
   （脚本 `cmake/VerifyGameplayDiagnosticsCodes.cmake`）。

**落地证据**见 [第 6 轮诊断分类学后续补齐记录](../stage_reports/stages/stage-07/2026-10-03-s7a-diagnostics-taxonomy-rulings.md)，
逐条 provenance 另见 [RULING_WORKSHEET.md](../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) §9 的
同次追加与 [OPEN_QUESTIONS.md](../proposals/gameplay-v2-acceptance/OPEN_QUESTIONS.md) §4.6。**本次落地只改
文档与集中码表语义，不表示 Judgement 实现完成、也不表示 Stage 7A 完成**（实现批次 S7A-3…S7A-9 仍未完成）。

## 与 Gameplay I ABI 的关系

[Gameplay Judgement ABI](GAMEPLAY_JUDGEMENT_ABI.md) 是 Gameplay I 的候选 ABI 文档，已于 2026-10-02 标注为
`superseded as typed preview input；retained as design-input history`，保留为 typed preview 的早期草案与命名来源
（[CONTRACT_MATRIX §13](../proposals/gameplay-v2-acceptance/CONTRACT_MATRIX.md#L222-L235)）。

**本次不修改该文件。** 冻结顺序要求先建立并冻结 V2 三份互链文档（ADR 0044 / Gameplay V2 Spec /
本文），**之后**才允许给 Gameplay I 的合同加 `superseded` / `historical` 标注；提前标注会产生
“已取代但替代不完整”的窗口（[RULING_WORKSHEET §0.2](../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md#L72-L75)）。

处置映射（保留项与替代项的分工）：

| Gameplay I ABI 部分 | V2 处置 |
| --- | --- |
| §1 生命周期形态 | 保留（本文沿用同一动词序列） |
| §2 候选类型（`InputEvent` / `JudgementRequirement` / `JudgementFact` / `JudgementResult` / `JudgementIdentity`） | 取代：`NormalizedObservation`、`RequirementRecord`、`FactRecord`、`JudgementResult` 投影、四分量 `JudgementIdentity` |
| §3 会话操作 | 保留 |
| §4 诊断方向 | 保留方向；码表由 CM-D04 统一，旧码到新码必须给出映射，不得出现两个同义码 |
| §5 兼容与版本 | 强化：增加 artifact 与 interchange 两条独立判定 |
| §6 非目标 | 保留；其中“稳定 C ABI 仍属于 Stage 14”与 [plan.md §10](../stage_plans/active/stage-07/plan.md) 一致 |

字段级映射（旧候选字段 → V2 边界）：

| Gameplay I 字段 | V2 边界表达 |
| --- | --- |
| `InputEvent.observationTime` | 拆为 tick 字段与原始时间戳字段两个（不再是一个 `observationTime`） |
| `InputEvent.sequence` | `IngressSequence`（宿主提供，会话内单一序列） |
| `InputEvent.source` | SourceClass（规范事件中的设备/来源类别）；SourceBuildIdentity 只承载生产者、参数、compiler profile 与 source map 的构建来源，不进入判定语义 |
| `JudgementRequirement.grip` | `PreparedGrace` + `GraceResolutionPolicy` + `ResourceClaimIntent` |
| `JudgementRequirement.measures` | `MeasureSpec`（多分量、逐分量 grading） |
| `JudgementFact.{phase, outcome, grade, error}` | `FactRecord` 的 `Phase` / `Outcome` / `Grade` / `TimingError`，并补齐 `CausalOrigin` 与证据 |
| `JudgementResult.eventSequence` | `EventSequence`（会话内单调序号） |
| `JudgementIdentity.{engine, ruleset, chart, session}` | 保留四分量，另加 `CompiledSemanticIdentity` / `ArtifactIdentity` / `InterchangeCompatibility` |

**缺陷 F-04 闭合记录（2026-10-03）。** 上表此前登记了 `observationTime` / `sequence` / `grip` / `measures` /
`JudgementFact` 四字段 / `eventSequence` / 四分量 identity，但**未登记 `InputEvent.source`（Gameplay I 的
`SourceIdentity`）的去向**；该缺口是 [S7A-1 typed contract review](../stage_reports/stages/stage-07/2026-10-02-s7a-1-typed-contract-review.md)
§10 中唯一仍开放的文本缺陷（F-04）。本轮按 Codex 裁定（thread `s7a1-freeze-bindings`，verdict `adopt`，
confidence 0.99，日期 2026-10-03）**补入上表 `InputEvent.source` 行，并在域 2 / 域 7 与 §输入域分工处补齐
`SourceClass` 与 `SourceBuildIdentity` 的分工**；F-04 随之闭合。带日期证据见
[第 2 轮门禁报告](../stage_reports/stages/stage-07/2026-10-03-s7a-2-gate-rulings.md)。

## 未决项

本文的未决项按“阻塞哪个批次”列出。第 1 轮已裁定的 5 项（CM-V06/V07/V13/I06/X06）在本文中只留下
“表本身待写”的形式；**第 2 轮（时间域与迟到策略）已于 2026-10-03 裁定**（thread `s7a1-freeze-bindings`，
verdict `adopt`，confidence 0.99），其已冻结事实见 §类型清单 域 1 / 域 2、§单位与量程 与 §S7A-2 节；
**第 3 轮（prepare、装配与 entry）同样已于 2026-10-03 裁定**（thread `s7a3-prepare-rulings`，三卡 verdict
`need_info` 0.8 / `reject` 0.88 / `adopt` 0.99），其已冻结事实见 §类型清单 域 3 / 域 7 / 域 8、§稳定与非稳定
声明 与 §S7A-3 节；**第 4 轮（仲裁、资源与事实序）亦已于 2026-10-03 裁定**（thread
`s7a4-arbitration-rulings`，主裁定卡 `adopt` 0.97 / 处置词与计数确认卡 `adopt` 0.99），其已冻结事实见
§类型清单 域 4 / 域 5 / 域 7、§单位与量程 与 §S7A-4 节；**第 5 轮（Ruleset、事实序、Score 与 Snapshot）
已于 2026-10-03 裁定**（thread `01a0fe49-cfe5-7933-8d03-3d0a65d34c8b`，三卡 verdict `adopt` 0.95 / 0.99 /
0.98），其已冻结事实见 §类型清单 域 5 / 域 6 / 域 7 / 域 9、§S7A-5 节 与 §S7A-6 节；**第 2 轮的输入规范化
半批语义亦已于 2026-10-03 由同 thread 的两张续裁卡复核与补充**（thread `s7a1-freeze-bindings`，verdict
均为 `adopt`，confidence **0.97** / **0.99**），其已冻结事实见 §类型清单 域 2 `DomainAmount` 行、域 6
`NormalizationProfile` 行、§数值域与量程、§时间单位与表达 与 `## S7A-2 输入半批冻结补充（2026-10-03）`
一节；**第 6 轮（发布粒度、表现桥接与诊断）同样已于 2026-10-03 裁定**（thread
`01a0fe61-3476-7882-9674-5f6b05035237`，单卡 verdict `adopt`、confidence **0.97**），其已冻结事实见
§类型清单 域 8（`CapabilityRecord` / `DerivedCapabilityClosure` / `EntryKind` / `PlaybackEntryRef` /
`JudgementError` / `DiagnosticCode` / `DiagnosticCategory` / `DiagnosticSeverity` 行）、§错误与诊断映射、
§仍待冻结 的诊断码表行、§S7A-7 节 与 [Spec §3.23–§3.25 / §5.6 / §9.6–§9.8](../formats/GAMEPLAY_V2_SPEC.md)；
**第 7 轮（收尾澄清与缺陷）同样已于 2026-10-03 裁定**（主卡 thread `01a0fe61-b7a3-7853-a380-0793fee14e14`，
verdict `adopt`、confidence **0.96**；口径确认卡 thread `01a0fe61-3476-7882-9674-5f6b05035237`，
`adopt`、**0.99**；16 行 / 18 项、`S7A7-R01…R16`），其已冻结事实见 §类型清单 域 4（solver / coordinator
措辞）与域 7（三行 identity 命名文字）、`## S7A-7 之后的收尾` 一节与
[Spec §3.8.7 / §5.1.1 / §8.5](../formats/GAMEPLAY_V2_SPEC.md)；**第 1–7 轮无待裁定轮次**，`open` 集合为空
（`accept` 52 / 114）。
**本表本轮不新增条目**：第 5 轮只把已冻结的**语义与行为边界**写入既有条目的状态格与既有行（#3、#10、
#11、#12、#14、#16、#20），§类型清单 的 9 域 / **126 = 96 + 30** 追踪目录保持不变（见 §126 条追踪规则 与
§S7A-5 / §S7A-6 两节的收尾声明）。**2026-10-03 输入半批复核同样不新增条目**：它只更新既有 #18 行与既有
类型行的语义文字，**未新增编号**。**第 6 轮亦不新增条目**：它只更新既有域 8 的 8 行语义文本与既有
§错误与诊断映射 / §仍待冻结 的行，**未新增 §未决项 编号、未新增类型行**，9 域 / **126 = 96 + 30** 不变
（见 §S7A-7 节的收尾声明）。**第 7 轮同样不新增条目**：它只更新既有域 4 / 域 7 的语义文字与既有行
（#1、#2、#17 的语义文字），**未新增 §未决项 编号、未新增类型行、未改动登记首词**，9 域 /
**126 = 96 + 30** 不变（见 `## S7A-7 之后的收尾` 一节）。

**`P1-14` 的指向性引用（第 5 轮，`S7A5-R11`）。** `P1-14` 的 REF0 / judgement closure、manifest /
presentation closure 与诊断 source map 的**具体归属矩阵**在
[Spec §3.8.6](../formats/GAMEPLAY_V2_SPEC.md) 冻结；本文**只作指向**，**不新增类型行**：判定必需引用进
REF0 / judgement closure，纯表现引用进 manifest / presentation closure，source map 仅诊断；REF0 的物理字段 /
编号 / 编码由 **S7A-3** 的首次 Packed 写入消费，entry / manifest 集成属 **S7A-7**。`P1-14` **不阻塞
S7A-5 / S7A-6**。

| # | 未决项 | 阻塞 | 参考 |
| --- | --- | --- | --- |
| 1 | ~~`IdentityProjection` 表（semantic / artifact / interchange 三列）尚未写出~~ **已裁定（第 1 轮，Codex 2026-10-02；第 7 轮 2026-10-03 按第 1 轮裁定补齐处置词与裁定正文，`S7A7-R08`）**：唯一 projection 表**落点**为 [Spec §5.2](../formats/GAMEPLAY_V2_SPEC.md)，以**实际生效的 prepared value** 为准，逐字段标注 source / semantic / artifact / judgement / presentation 归属，并分别列明三条判定（§域 7 的三条分工）；三项统一命名为 `source closure` / `diagnostic map` / `content-artifact identity`（Spec §5.1.1）。**本表不另立第二张投影表** | S7A-1（门禁已满足） | CM-I06 / Q-10 |
| 2 | ~~`candidateRevision` 的完整 artifact/interchange 兼容矩阵（覆盖工具链与载体差异）尚未定义~~ **已裁定（第 1 轮，Codex 2026-10-02；第 7 轮 2026-10-03 补齐处置词，`S7A7-R08`）**：`candidateRevision` **不进** `compiledSemanticIdentity`，但**必须进入** artifact compatibility / interchange 判定；**不可互换的产物不得仅凭 `CompiledSemanticIdentity` 判等价**；须给出覆盖工具链与载体差异的兼容矩阵。**矩阵细则**仍属 [Spec §2.4 / §11.1 第 1 条](../formats/GAMEPLAY_V2_SPEC.md) 的后续条目 | S7A-1（语义已冻结）／矩阵细则后续批次 | Q-01 Codex 修订 |
| 3 | 类型最终命名、整数宽度、字段集、序列化字节均未冻结 | S7A-1 起 | [plan.md §S7A-1](../stage_plans/active/stage-07/plan.md)（**第 2 轮例外**：`ChartTick` / `judgementTick` / `observationTick` / `commitTick` 与 `TickSpan` / `TickDelta` 的**宽度与单位来源已冻结**为有符号 64 位整数 + `engine.tick.us.v1` 的 `TimebaseProfile`；**第 3 轮例外**：identity 的**规范字节边界**已冻结为"prepare 内部确定性派生物、语义等价 / 排序 / 身份域边界"，**编码实现须在 S7A-3 消费前闭合**，见 [Spec §5.5](../formats/GAMEPLAY_V2_SPEC.md)；**第 5 轮例外**：`RegisterKind` 三类**命名**（`exclusive` / `commutative_monoid` / `ledger_derived`）与 Fact 身份**生成语义**已冻结（Spec §3.15、§3.17），Snapshot / Replay 两套 **header 字段集**与 `SnapshotPayload` **闭包**已冻结（Spec §3.18），**拼写规范**为类型 `EventCodecId` / 字段 `eventCodecId`；**第 6 轮例外**：域 8 的 capability registry 与诊断行的**语义文本**已冻结（`CapabilityRecord` 七字段的 identity 归属、诊断四层结构、集中码表位置 + CTest 校验门禁），见 §S7A-7 节与 [Spec §5.6 / §9.6](../formats/GAMEPLAY_V2_SPEC.md)；**最终命名、整数宽度与序列化字节仍在此行**） |
| 4 | 安装组件归属、target 与 allowlist 落地方式未定 | S7A-1 | TARGETS_AND_DEPENDENCIES |
| 5 | ~~`Tick` 宽度、`ChartTick` ↔ `ObservationTick` 转换边界与 tie rule 未定~~ **已裁定（第 2 轮，Codex 2026-10-03）**：宽度为有符号 64 位整数，单位由 `engine.tick.us.v1` 的 `TimebaseProfile` 声明，禁止隐式换算，溢出稳定拒绝；RationalBeat → `judgementTick` 为精确有理数映射、四舍五入到最近整数、半数取偶、tempo/stop/负 Beat 同一规则；同 Tick 按 `(tick, originKind, canonicalOrdinal)` 排序，禁止按 ingress 顺序。**本次修订（2026-10-03 实现复核）：`stop` 语义为塌缩 / 跳变 + 精确累计函数 `F`（`F(originBeat) = 0`、`F(endBeat) = F(startBeat) + duration`、`tick(beat) = roundHalfToEven(F(beat))`、只舍入一次、区间内非单射、半开方向正向 `(originBeat, beat]` / 负向 `(beat, originBeat]`、有 stop 时不再奇对称），不再表述为"stop 把区间映射为不推进的 Tick"** | S7A-2（门禁已满足） | CM-T03 / CM-T04；落点 [Spec §3.7](../formats/GAMEPLAY_V2_SPEC.md) |
| 6 | ~~`FinalizationWatermark` 与 queue hop 的状态表未定（7A 不冻结数值）~~ **已裁定（第 2 轮，Codex 2026-10-03）**：`finalizationWatermark`、queue hop、窗口 open/close 与重复排队条件均为 typed 参数，由 `TimebaseProfile` / ruleset 提供；默认值只在 profile registry 登记为 `pending_measurement`；缺失或未测量在 prepare 稳定拒绝；重复排队稳定拒绝并返回诊断码。**具体数值仍为后续批次阻塞项**（不是 S7A-2 门禁） | S7A-2（参数化已冻结）／后续批次（数值） | CM-T08 / P1-04 |
| 7 | ~~`SolverProfile` 字段与默认目标函数未冻结；`greedy_v1` 命名待定~~ **已裁定语义边界（第 4 轮，Codex 2026-10-03，`S7A4-R01`）**：`SolverProfile` 的**字段语义**、**缺失 / 歧义 / 无法证明唯一时的 prepare 拒绝语义**、`objective` / `tieBreak` 为**有序语义列表**已冻结；名称边界采用 **`coordinator.policy.greedy_v1`**（runtime 侧不称 solver）。**默认目标函数列表、具体算法预算、`K` / fuel / `max*` 数值与序列化表示仍未冻结**，留给 **S7A-9** 与后续预算批次（见 #20）。落点 [Spec §3.12](../formats/GAMEPLAY_V2_SPEC.md) 与 §类型清单 域 4 | S7A-4（语义边界已冻结）／S7A-9（默认列表与数值） | CM-C06 / CM-C07 / AR P0-03 |
| 8 | ~~`ResourceId` 命名空间、lease/contact identity、`grace = 0` 与 gap 到期行为未定~~ **已裁定（第 4 轮，Codex 2026-10-03，`S7A4-R05`）**：`resourceId` 只在 **prepared canonical graph 的资源命名空间内**解释；`capacity = 1` 使用**唯一 slot**；**slot / lease / 最小 contact handle / claim identity 进入 canonical graph 与 prepared judgement inputs**；lease 与 contact 由**引擎按规范阶段分配**、宿主不得提供；**`contact end` 不复用旧 handle**；**`observe` 只产生 Observation、不占用也不改变资源 owner**；**非零资源 `declaredGapGrace`**、`gap` / `handoff_pending` 稳定拒绝（资源 `declaredGapGrace = 0` 是 7A 唯一合法值，不限制 Requirement preparedGrace）。落点 [Spec §3.10](../formats/GAMEPLAY_V2_SPEC.md) 与 §类型清单 域 4 | S7A-4（门禁已满足） | CM-C09 / Q-11 |
| 9 | ~~六步 Tick 顺序与八步 Coordination window 阶段的映射未定义~~ **已裁定（第 4 轮，Codex 2026-10-03，`S7A4-R03` / `S7A4-R04`）**：**唯一**映射——六步＝外层、八阶段＝**第 4 步内部完整展开**（第 3 步＝八阶段第 1 步；第 4 步＝八阶段第 2–7 步；第 5 步＝八阶段第 8 步后按 causal total order 排序；第 6 步 Tick 末提交 Hook / signal）；阶段顺序与语义进 **judgement identity 的 engine 组件**，不进 chart/content identity；**只冻结阶段名称、顺序、映射与语义边界**。落点 [Spec §3.9](../formats/GAMEPLAY_V2_SPEC.md) 与 §类型清单 域 4 | S7A-4（门禁已满足） | CM-C05 / Q-16 |
| 10 | ~~`originId` / `commitId` / `factId` / `phasePriority` 的生成与排序字节合同未定~~ **已裁定生成语义（第 5 轮，Codex 2026-10-03，`S7A5-R01` + `S7A5-R02`）**：唯一规范总序为 `(commitTick, originKindPriority, canonicalOrdinal)`，`canonicalOrdinal` 由引擎从 `(originScope, originOrdinal, localOrdinal, phasePriority, factKindPriority)` 派生、**不得使用 `ingressSequence` / 容器顺序 / 线程完成顺序**，Fact 的 `tick` 即 `commitTick`；`originId` 不透明、由 `(originKind, originScope, originOrdinal)` 唯一确定；`commitId` 按总序单调分配；`factId` = `(commitId, localOrdinal)`；`phasePriority` 仅用于 `canonicalOrdinal`、**绝非评分**；timer ordinal 由 prepared timer identity 派生、seek / replay 不重新编号；correction 仍由 R-08 拒绝；生成语义与 phase registry 进 `FactSemanticRevision`（属 `JudgementIdentity.engine`）。**物理字节布局、varint / endianness、`FactId` / `CommitId` 编码与 reference-evaluator byte fixture 仍未冻结**，由首次序列化消费它们的 **S7A-6** 阻塞 | S7A-5（生成语义已冻结）／**S7A-6**（物理编码） | CM-F06 / Q-12；落点 [Spec §3.9](../formats/GAMEPLAY_V2_SPEC.md) 与 [Spec §3.17](../formats/GAMEPLAY_V2_SPEC.md) |
| 11 | ~~7A 最小 `outcome` / `category` / `grade` 集合与统计 reset/seek 规则未定~~ **已裁定（第 5 轮，Codex 2026-10-03，`S7A5-R09`）**：`Outcome` = `{hit, miss}`；Hold 的 head / body / tail 是各自附属的 phase-local outcome / error、**不扩充** `Outcome`；`FactCategory` 与 phase 一一对应（`{tap, hold_head, hold_body, hold_tail}`）；grade table 可选、缺失即 **absent** 且**不隐式升级**；`TimingError` = `observationTick - chartTick` 的**有符号整数 tick 差**；seek / replay 从 Fact Ledger 重建统计；reset 清空新 session 状态且**不产生 Fact**。**字段表示与编码仍未冻结**（首次消费它的后续批次） | S7A-5（门禁已满足） | CM-S10 / P1-07；落点 [Spec §3.20](../formats/GAMEPLAY_V2_SPEC.md) |
| 12 | ~~`SnapshotHeader` 字段集、state schema revision 与预算未定~~ **已裁定字段集（第 5 轮，Codex 2026-10-03，`S7A5-R06` + `S7A5-R07`）**：全量 Snapshot header = `formatVersion`、四分量 `JudgementIdentity`、`factSemanticRevision`、`stateSchemaRevision`、normalized event count、Fact count、payload byte count、typed byte-budget descriptor；`factSemanticRevision` 进 engine identity、`stateSchemaRevision` **不进** judgement identity；`SnapshotPayload` 闭包（14 项）与不得保存清单（5 项）已冻结，FactBinding 与 prepared immutable graph **重新取得、不重复保存**；**字节布局与预算数值仍未冻结**（S7A-6 / S7A-9） | S7A-6（字段集已冻结）／S7A-9（预算数值） | CM-K07 / Q-13；落点 [Spec §3.18](../formats/GAMEPLAY_V2_SPEC.md) |
| 13 | 诊断码表（code / category / severity）与机器可读文件位置未定 | S7A-7 | CM-D04 / P1-09 |
| 14 | ~~7A 是否包含 `cuexis.ruleset` package 未裁定~~ **已裁定（第 5 轮，Codex 2026-10-03，`S7A5-R05`）**：7A **只用内置、静态注册的 Ruleset Interface / 模块**；任何 `cuexis.ruleset` package 输入命中**既有 R-09**（诊断 `ruleset.package_unsupported`），**不得读** package hash / manifest / 迁移矩阵。**遗留部分**：package 的**形状 / identity / 完整性 / 版本迁移**仍待冻结，归 **S7C-2**（不阻塞 S7A-5 / S7A-6） | S7A-5（门禁已满足）／**S7C-2**（package 形状与迁移） | CM-S08 / Q-17；落点 [Spec §3.16](../formats/GAMEPLAY_V2_SPEC.md) 与 Spec §7.2 的 R-09 |
| 15 | `PresentationEvent` 与既有 HostOverride / OverrideToken 的 precedence 命名常量未定 | S7A-7 | CM-P04 / Q-09 |
| 16 | 所有预算数值与限额在 S7A-9 前保持未冻结，仅登记计数口径与待测量项 | S7A-9（数值与限额）／S7A-6（`ReplayDecodeBudget`、typed byte-budget descriptor 字段集；`SeekLatencyCommitment` 类型与承诺语义；两套 header 字节布局） | A8 / [plan.md §3](../stage_plans/active/stage-07/plan.md)；CM-K08（第 5 轮，本次登记新增）。**第 5 轮更新（2026-10-03，`S7A5-R06` / `S7A5-R08`）**：`ReplayDecodeBudget` 的**三类预算字段**（事件数 / 字节数 / 解码时间）与超限错误语义、typed byte-budget descriptor、`SeekLatencyCommitment` 的**类型、承诺语义、会话只能收紧与测量口径入口**已冻结；**所有具体数值**（含 `maxSeekLatency`）仍在此行、保持未冻结，且**禁止**进入 Schema / header / 运行时约束 / 对外承诺，提供具体值稳定拒绝 |
| 17 | capability registry 条目字段集（`CapabilityRecord`）已由第 6 轮冻结语义、D-14 按方案 (a) 处置（以本文 `CapabilityRecord` 行的 `待冻结` 为准，登记首词不变） | S7A-7 | CM-X01（第 6 轮，与 Q-15 / Q-19 同批） |
| 18 | **第 2 轮留下的后续批次阻塞项**（不是 S7A-2 门禁）：具体 late-policy 数值（watermark / queue hop / 窗口开闭阈值）、具体业务量程数值与限额（`AmountSpec` 的取值）、S7C-1 的 `CalibrationProfile` 扩展字段、连续输入能力（连续轨迹 / minimumRate / reconstruction / discontinuity，属 S7B-1）；另登记两项（2026-10-03 实现复核）：**late-policy 参数量级关系的校验**阻塞 **S7A-4** 的 late-window / 判定消费（最终数值与限额记 **S7A-9**），**Tick → Beat 反查契约**阻塞**首次消费该反查的后续批次**（`S7A-3` / `S7A-4` / `S7A-5` 均不消费它）。**2026-10-03 输入半批复核（同 thread 两张续裁卡，`adopt` 0.97 / 0.99）后本行仍只承载上述遗留**：`AmountSpec` 的 **64 位 canonical 宽度已冻结**、域声明集合与完整 `AmountSpec` 字段**已进 session identity**、**7A 不引入 per-domain 版本字段**（会话级版本由 `profileVersion` 承载）、不连续表示与连续输入能力**复用** `input.continuous_unsupported`（`field.path` 区分）并归 **S7B-1**、不可表示的 calibrated clock 值**复用**既有 tick 域表示失败——上列各项**均不再是未决项**，本行**不新增编号** | 后续批次（数值与限额）；S7C-1；S7B-1；**S7A-4**（late-policy 参数量级关系的校验）；**首次消费 Tick → Beat 反查的后续批次** | CM-T08 / P1-04（数值）、CM-T13（数值）、S7C-1 校准扩展、CM-L02 / P1-05（连续输入）；CM-T09 / CM-T13（2026-10-03 输入半批复核，已冻结部分） |
| 19 | **第 3 轮遗留项（已部分闭合）**：Gameplay Capsule v2 的首次 Packed wire 合同已由 S7A-3 裁决闭合；仍遗留 `boundedRelationInstance` 候选表示与具体 `stableRejectCode`（CM-C10 / P1-03，7B+ / S7C）、[SUPPORT §6.1](../proposals/gameplay-v2-acceptance/SUPPORT_AND_REJECTION_MATRIX.md) 的 `W-01…W-05` 候选项（按首次实际消费登记，W-02 须先核对已闭合的 F-04） | 后续批次（**只阻塞首次拟消费它的批次**）；实现证据、identity 内部编码与完整 E1 验收仍属 S7A-3 工作 | Q-07、CM-C10 / P1-03、CM-X05（已由 D-3 落地关闭，处置词 `revise`） |
| 20 | **第 4 轮留下的后续批次阻塞项**（**不是** S7A-4 门禁）：① `SolverProfile` 的**默认列表**（默认目标函数、默认 tie-break 顺序）与具体算法预算、`K` / fuel / `maxCandidates` / `maxBranches` / `maxFuel` **数值**；② 唯一性证明的 **proof 算法与证据形式（proof 编码）**；③ **wire / serialization** 表示（阶段编号与编码、资源状态表逐字段表示、identity 字节）；④ **`terminal` 编码**；⑤ 后续 **`gap` / handoff / `capacity > 1` 语义**（S7B+ / S7C 候选）；⑥ 判定窗口**半宽的具体数值** | ① **S7A-9**（与后续预算批次，数值测量后单独接受）；②③④⑥ 由**首次消费它们的后续批次**阻塞；⑤ **S7B+ / S7C** 候选。**第 5 轮另留下**（2026-10-03，见 §S7A-5 / §S7A-6 节与 #10 / #12 / #14 / #16）：两套 header 的**字节布局**与 Event / Fact codec 编码、`FactId` / `CommitId` 物理编码与 varint / endianness、`maxSeekLatency` 与全部预算**数值**、`cuexis.ruleset` package **形状** | Q-03 / CM-C06 / CM-C07、Q-16 / CM-C05、Q-11 / CM-C09、P1-12（第 4 轮）；Q-12 / CM-F06、Q-13 / CM-K01 / CM-K07、CM-K08、CM-S08 / Q-17（第 5 轮遗留）；落点 [Spec §S7A-4 节](../formats/GAMEPLAY_V2_SPEC.md)、[Spec §11.1 第 6 条](../formats/GAMEPLAY_V2_SPEC.md)、[Spec §S7A-5 节](../formats/GAMEPLAY_V2_SPEC.md) 与 [Spec §11.1 第 7 条](../formats/GAMEPLAY_V2_SPEC.md) |

## 相关索引

- [Gameplay V2 字段与运行语义规范](../formats/GAMEPLAY_V2_SPEC.md)：V2 字段与运行语义的唯一权威 Spec
- [ADR 0044：Gameplay V2 语义内核与冻结边界](../adr/0044-gameplay-v2-semantic-kernel.md)：决策与威胁模型
- [Gameplay V2 acceptance package](../proposals/gameplay-v2-acceptance/README.md)：接受的准入包与文档动作清单
- [未决语义分轮裁决清单](../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md)：7 轮裁定协议与第 1 至第 7 轮结论（**无待裁定轮次，`open` 集合为空**）
- [支持 / 拒绝矩阵与 capability registry 草案](../proposals/gameplay-v2-acceptance/SUPPORT_AND_REJECTION_MATRIX.md)：7A 支持集合与 19 条拒绝
- [格式、入口与 identity 矩阵](../proposals/gameplay-v2-acceptance/FORMAT_ENTRY_AND_IDENTITY.md)：四类身份与四分量投影
- [S7A-0.2 合同逐项台账](../proposals/gameplay-v2-acceptance/CONTRACT_MATRIX.md)：114 项合同与处置词
- [Stage 7A 实施计划](../stage_plans/active/stage-07/plan.md)：批次、冻结顺序与关闭标准
- [S7A-0 owner 接受记录](../stage_reports/stages/stage-07/2026-10-02-s7a-0-acceptance.md)：接受对象与 §4 冻结顺序
- [第 7 轮收尾裁定与缺陷处置记录](../stage_reports/stages/stage-07/2026-10-03-s7a-7-wrapup-rulings.md)：第 7 轮的 16 行 / 18 项、五条 `CM-V*` 补齐、`D-9` / `D-12` 与收尾门禁
- [V2 语义内核](../proposals/research/gameplay-v2/SEMANTIC_KERNEL.md)：时间域、规范化观测、Fact Ledger 与 Ruleset 事务
- [Gameplay V2 初步设计方案](../proposals/research/gameplay-v2/PRELIMINARY_DESIGN.md)：候选字段形状与资源状态机
- [Gameplay V2 ABI 的 Gameplay I 前身](GAMEPLAY_JUDGEMENT_ABI.md)：已标为 `superseded`（保留为历史候选输入）
- [内部模块速查](internal-module-catalog.md)：判定类型的模块归属边界
- [API 参考导航](README.md)：本目录的边界与权威说明
- [文档整理政策](../DOCUMENTATION_POLICY.md)：文档角色与状态词枚举

# Stage 7 Implementation Plan: Gameplay Foundation and Judgement Evolution

状态：active；Stage 7A 分批实施，Stage 7B+ 为可在 Stage 8 前后持续交付的能力线；本计划不构成阶段关闭或发行记录

更新日期：2026-10-06

归档来源：[旧版 PROJECT_GUIDE](../../../archive/PROJECT_GUIDE_LEGACY_2026-08-10.md)、
[SDK transition plan 快照](../../../archive/CUEXIS_SDK_TRANSITION_PLAN_2026-08-10.md) 和
原 Stage 11 Judgement 计划；当前候选字段与语义输入见本文列出的 Gameplay V2 研究目录，以及
由 S7A-0 产生并接受的 V2 ADR、Spec、ABI、Schema 和 Replay 合同。

本计划只定义阶段目标、批次、依赖、交付物和验收门禁。现有 [Gameplay Judgement Spec](../../../formats/GAMEPLAY_JUDGEMENT_SPEC.md)、
[Gameplay Judgement ABI](../../../api/GAMEPLAY_JUDGEMENT_ABI.md) 和 [ADR 0043](../../../adr/0043-gameplay-judgement-ruleset-convergence.md)
是 Gameplay I 的历史候选基线；V2 的字段与运行语义以 [Gameplay V2 redesign research](../../../proposals/research/gameplay-v2/README.md)
为实施前输入，并必须在 S7A-0 中修订或 supersede 这三份合同后才能冻结。本文不把研究 spike、
候选 ABI 或本计划本身解释为已经实施的产品能力。

前置是 [Stage 6](../../completed/stage-06/plan.md) 的交接边界和已接受的剩余事项；主要内容
输入为 Chart v5 Core/Packed candidate，Chart v4 / CXT v1 / CXC v1 作为兼容与回退输入。共同
模型见 [音乐游戏玩法抽象模型](../../../architecture/GAMEPLAY_ABSTRACTION_MODEL.md)。

Stage 7A 是 Stage 8 的硬前置，只交付最小可玩的 Input / Judgement / Score / Replay kernel。
本路线以 Chart v5 `gameplay.version = 2` 为 Gameplay semantic 基线，并允许 CXC v1 内独立的
Canonical Gameplay Graph Playback entry。Graph entry 与 Packed Chart entry 必须进入同一 typed
prepare、Judgement、Ruleset、Snapshot 和 Replay 路径；不得形成第二套判定实现。
Stage 7B+ 是长期能力线；Stage 8 只接收明确列入发行矩阵并已完成本计划门禁的能力，不等待全部
7B+。任意运行时脚本、逐帧脚本回调、宿主字节码和通用 Script VM 始终不在 Stage 7 范围内。

## 阶段目标

Stage 7A 的目标是冻结并交付一个可由 Chart v5 candidate、CXC Graph/Packed Playback entry、Player、
Headless consumer、Reference Host 和后续 Studio 共同消费的最小判定闭环：输入规范化、要求准备、
Tap/Hold/Release/tail 求值、单一 capacity=1 exclusive resource、Ruleset 折叠、Score/Combo/Statistics、
Replay identity、无损 Snapshot/Seek，以及基于 FactBinding 的 early/exact/late/Miss 表现桥接。Stage 7A 关闭
必须同时收口 Stage 6 登记的四项前置，并把可核验的合同、golden、拒绝路径和预算证据交给 Stage 8。

Stage 7B+ 的目标是以独立 capability 扩展连续输入、方向动作、多接触点、复杂持续约束、校准和
Ruleset 演进。每项能力都有自己的版本、identity、Replay、预算、稳定拒绝和发行选入决策，不能
通过修改 7A 已冻结字段含义来获得隐式支持。

## 1. 产品边界和不变量

Stage 7 的稳定数据流为：

```text
InputEvent
  -> InputMapping / normalization
  -> prepared JudgementRequirement
  -> JudgementFact / JudgementResult
  -> Ruleset Fold
  -> Score / Combo / Statistics
  -> read-only Presentation or host feedback
```

必须保持以下边界：

1. L1 Input 只负责设备事件、映射、必要的离散规范化和 Replay 规范化；连续量重采样属于
   后续 capability。L2 Program 只负责
   Requirement 的 Pattern、Measure、归属声明和 Fact；L3 Ruleset 负责仲裁、折叠、模块和计分；
   L4 Presentation 只读结果和有限 Effect Schedule。
2. Judgement 不读取渲染帧率、GPU、World、EnTT、SDL、OpenGL、音频后端或宿主引擎类型。结果
   由绝对观测时间和显式快照重建，不以“上一帧结果”作为未声明的语义状态。
3. 判定域是抽象的 `InputDomain`、离散位置、区域、通道或经注册的几何域；公共 Judgement API
   不绑定 lane、Note、屏幕像素、键盘布局或具体渲染后端。
4. 任何新 RequirementKind、Pattern、Outcome、Hook、几何域或策略都必须有版本、capability、
   稳定拒绝诊断和独立 identity 影响说明；不支持内容不得静默降级为 Tap、Hold 或旧版本语义。
5. Score、Combo、Life（若启用）、Statistics、Release/tail phase、单一 exclusive resource ownership
   和 Replay 游标均属于会话状态，
   不回写 Chart、Behavior、World 或作者源。
6. 运行期不执行 IO、随机数、墙钟、宿主回调或动态 Requirement 生成。声明式 Behavior、有限
   Effect Schedule 和 Judgement Result Binding 可消费结果，但不能改变已经产生的判定事实。

### 1.1 权威输入和冻结顺序

Gameplay I 的 ADR/Spec/ABI 仅作为历史候选基线和差异对照。Stage 7A 的实际实施授权必须来自
以 [Gameplay V2 redesign research](../../../proposals/research/gameplay-v2/README.md) 为输入的
V2 acceptance package；该 package 必须修订或明确 supersede 旧合同，并覆盖 ADR、Spec、typed
ABI、Schema、Replay 和 diagnostics。接受前不得从两套合同中自行拼接字段或语义。

实施前必须按以下顺序处理文档状态：

```text
Gameplay V2 acceptance package owner acceptance
  -> V2 ADR / Spec / ABI / Schema / Replay / diagnostics 冻结
  -> Gameplay I Spec / ABI / ADR 明确标记为 superseded、retained 或 historical-only
  -> Stage 7A typed contract review
  -> 实现批次和 golden
  -> Stage 7A 关闭
```

第 2 步是**分批冻结**，不是"一次性冻结全部 V2 工件"（2026-10-02 owner 接受第 1 轮补充 S1-01…S1-05）：

- 每批只冻结该批次的**权威来源与版本层级、类型角色与边界、模块与安装边界、所有权与异常边界承诺**。
- Schema / Replay / Snapshot / diagnostics 工件、逐字段表示、整数宽度、序列化与线格式、枚举集与数值限额
  **不因三份文档存在而视为已冻结**，按各自轮次登记为阻塞项。
- 三份 V2 文档的顶层状态保持 `candidate`；授权来自**范围精确**的限定冻结，不来自状态词。
- **已授权**（受限 S7A-1）：内部 typed 骨架——类型角色与边界、模块与安装边界、既有所有权 / 异常承诺，
  以及已经裁定的接口与状态行为。**未授权**：该范围外的任何字段表示、宽度、序列化、枚举集、数值，
  以及第 1–7 轮已裁定但需在各自批次实施的语义（第 2、3、4、5、6、7 轮已由各自小节裁定，
  **第 1–7 轮无待裁定轮次**，仍不在本批次授权范围）；受影响的可运行方法必须移出本批次，不得用默认值、临时 typedef、
  序列化编码或"伪成功"绕过。
- 开工前提是"**本批次依赖的** P0 全部裁定"，不是"全部 P0 裁定"。

**执行顺序偏差（2026-10-02 如实记录）。** 第 3 步（Gameplay I 标注 `superseded`）在第 2 步的限定冻结
之前就已执行；**不倒填**冻结时间，纠正动作是限定冻结后重新核验替代链接与 retained 映射，记录见
[S7A-0 接受记录 §4.2](../../../stage_reports/stages/stage-07/readiness/2026-10-02-s7a-0-acceptance.md)。

如果实现阶段发现 V2 合同、历史 Gameplay I 文档、研究证据或真实内容预算之间矛盾，先建立可复现失败报告并停止
受影响批次；不得在代码中隐式选择替代语义。修改四层边界、identity 分区、grace 语义、
Pattern 原语、仲裁顺序或拒绝策略，必须先更新 ADR/Spec，再重新验证全部受影响批次。

### 1.2 Stage 6 交接前置

Stage 6 关闭后由 Stage 7A 承接四项未完成内容。它们是关闭前置，不是“以后再补”的 backlog：

| 交接项 | Stage 7A 必须交付的结果 | 关闭时最低证据 |
| --- | --- | --- |
| 显式 candidate 与实验隔离 | `Cuexis_ALLOW_EXPERIMENTAL`、candidate flavor/安装元数据、Player `--candidate-entry` 与项目/CXC 入口、至少一个真实启用 candidate 的 preset 或 CI | 默认 OFF 与显式 ON 的构建/安装/运行矩阵；production consumer 证明不会误启用 candidate；错误入口稳定拒绝 |
| 离线 typed assembler 与 feature 派生 | 从 typed Chart/CXT/source 输入派生 feature、requirements、resource closure 和 capability 的离线入口；禁止测试注入 feature 作为生产后门 | 正例、缺字段、非法引用、预算、确定性 identity 和原子失败用例；Playback 不展开 CXT |
| 具名宿主六动词命令循环 | 对现有 Reference Host 的 `open/play/pause/seek/reload/quit` 交接回归，不重新实现 R9（**措辞订正**：ADR 0042 的六动词即此序列；实现另有 `tick` 运行时步骤，不在六动词之内。缺陷 `D-2` / `D-7`，第 7 轮 `S7A7-R09`，**不改 ADR 0042、不改实现**） | 同一 SHA 的 hosted 回归、命令状态矩阵、错误/旧状态保持证据 |
| SDK API `0.7.1` | 先修订 version-gate 的条件化放行机制，再按版本规范落实 additive API 版本；若 owner 接受例外，必须有 ADR/版本规范记录 | version gate 的默认拒绝、一次显式放行、0.7.0 consumer 兼容、fresh/clean build、安装元数据和 hosted 证据 |

四项中任一项既未实现也没有被 ADR 或所属 Spec 明示接受的例外，Stage 7A 不得关闭。Stage 6
的关闭事实不被改写；只把剩余证据归入当前阶段。

## 2. 总体依赖和发布路线

```text
S7A-0 基线和合同准入
  -> S7A-1 类型/模块边界
  -> S7A-2 输入规范化和映射
  -> S7A-3 Requirement prepare / typed assembler
  -> S7A-4 Kernel lifecycle、Tap/Hold/Release、仲裁
  -> S7A-5 Fold、Score、Combo、Statistics
  -> S7A-6 Identity、Replay、Snapshot、Seek
  -> S7A-7 Playback/Chart candidate/Player/Headless 集成
  -> S7A-8 四项 Stage 6 交接收口
  -> S7A-9 跨平台硬化、Stage 8 handoff、owner acceptance

S7A-1 + S7A-2 + S7A-3
  -> S7B-0 capability registry / extension harness
S7A-9 + S7B-0
      +-> S7B-1 连续输入和 Slide/handoff
      +-> S7B-2 Flick / direction / velocity
      +-> S7B-3 多接触点、和弦和资源占用
      +-> S7B-4 复杂 Hold、尾判、Roll/Count/Spinner 类约束
      +-> S7C-1 calibration / device policy
      +-> S7C-2 ruleset package / module / replay evolution

S7A-9 ------------------------------> Stage 8
已选 7B+ capability + 独立发行矩阵 ------> 可选地加入 Stage 8
未选 7B+ capability -------------------> 稳定拒绝，继续在 Stage 8 后交付
```

允许并行：S7A-2 的设备适配研究、S7A-3 的 typed prepare 表征、S7B-0 的 capability 目录可以
在合同准入后并行准备。涉及同一公共头、CMake target、安装导出、Replay 版本或 identity 算法的
集成必须串行；下游 golden 通过不能替代上游合同、失败路径和架构检查。

Stage 8 的发行矩阵必须在其启动前写出：对每项 7B+ capability 标注 `included`、`candidate`
或 `rejected/deferred`，并附 capability、Packed/CXC 表达、Replay/identity、预算和拒绝测试
入口。未写入矩阵的 7B+ 能力不得被 v5 默认 Writer 或 Player 默认入口隐式启用。

S7B-0 的 registry、fixture harness 和拒绝骨架可以在 S7A-1/2/3 完成后并行建立；它们不得改变
7A 的公共字段或运行语义。任何实际消费 7A Judgement kernel、InputMapping、Snapshot 或 Replay
的 7B+ capability，必须在 S7A-9 的 typed handoff、拒绝矩阵和 owner acceptance 完成后才能
进入实现集成。7B+ 的研究 proposal、reference model 和 candidate-only prepare 不构成对 7A
或 Stage 8 的实现依赖，也不能绕过 S7B-0。

### 2.1 7B+ 能力依赖细化

以下依赖按 capability 子项计算，不要求整批完成后才能开始无关研究；只有消费相应输入/运行时
状态的子项才继承该依赖。

| Capability 线 | 最低依赖 | 边界说明 |
| --- | --- | --- |
| S7B-0 | S7A-1/2/3 typed contract 准入 | 可先建静态 registry、拒绝表和测试 harness；不得加运行时入口或改动 V2 合同 |
| S7B-1 region/continuous sampling | S7A-9 + S7B-0 | 连续事件规范化、重采样和 Snapshot 集成须等 7A handoff；区域谓词 reference model 可先独立研究 |
| S7B-1 handoff/Slider continuity | S7A-9 + S7B-0；连续 Slider 另依赖 continuous sampling | `resource.handoff.v1`、Gap 状态与恢复仲裁单独接受；handoff 不自动要求 Slider 能力 |
| S7B-2 Flick | S7A-9 + S7B-0；若使用连续 motion 样本则依赖 S7B-1 continuous sampling | 基于离散 press/release 的 Flick 子集可单独提案；不能假设连续轨迹已支持 |
| S7B-3 multi-contact | S7A-9 + S7B-0 + 明确的 contact identity/input projection | 静态多输入聚合与多接触轨迹拆分；轨迹子项另依赖 S7B-1，不能因支持键盘多键就暗示支持多触点 |
| S7B-4 action families | S7A-9 + S7B-0；仅依其实际消费的 S7B-1/2/3 子 capability | 每个动作族独立依赖和 capability；7A Release/tail 无需重新实现或等待本批次 |
| S7C-1 calibration/device policy | S7A-9 + S7B-0 | 可在 Stage 8 前后交付；只能通过明确 profile transaction 扩展，不能改写已冻结的默认 InputMapping |
| S7C-2 Ruleset package | S7A-9 + S7B-0 + accepted Ruleset Interface | 只扩展静态注册模块/常量包；不能把包支持解释为允许脚本/字节码或动态插件 |

Stage 8 的选入只要求其发行矩阵实际选中 capability 的依赖闭包退出。比如只选入一个不依赖连续
输入的 Flick 子能力，不因此隐式选入 Slider、handoff 或多接触点。

## 3. Stage 7A 交付批次

### S7A-0：实施准入、基线和合同表征

**目的。** 把 Gameplay V2 candidate 转成实施者可执行的合同清单，并确认实现前的仓库和工具链基线；
在 V2 合同接受前，不允许进入公共头或产品实现批次。

**工作内容。**

1. 记录执行时 HEAD、分支、工作区、UTC 日期、SDK API、显示版本、工具链、preset、依赖和
   hosted workflow 入口；不要把历史报告 SHA 当作本次证据。
2. 对 Gameplay v2 的版本层级、Canonical Gameplay Graph、CXC Graph/Packed 双 Playback entry、
   ADR 0043 的四层边界、Tick 顺序、`leftmost-first`、`consume/observe/claim`、单一 exclusive
   resource、Release/tail、early/late FactBinding、TimebaseProfile、四分量 identity、快照字段和
   拒绝诊断建立逐项 contract matrix。
3. 由 owner 接受 V2 acceptance package，并明确哪些字段仍为候选；同时给 Gameplay I 的 ADR/Spec/ABI
   建立 superseded/retained/historical-only 映射。对生产预算只登记待测量的上界，不把
   96 条研究切片、4,777 字节快照或建议值写成 ABI 常量。
4. 以现有 Core `Result<T,E>`、公共头 ASCII、target allowlist、架构检查和静态/shared
   package gate 为实现约束；列出新增 target、依赖和安装组件。
5. 为每个后续批次指定正例、负例、诊断类别、golden、执行命令、预期证据文件和停止条件。

**输出。** dated baseline report、accepted contract checklist、支持/拒绝矩阵、Chart v5/CXC entry
矩阵、依赖与安装图、预算测量计划、四项 Stage 6 交接台账、未决问题清单。

**退出门禁。** V2 ADR/Spec/ABI/Schema/Replay/diagnostics 状态已明确；旧 Gameplay I 合同的
替代关系已记录；所有未决语义均有 owner 决策或阻塞项；无“实施时再决定”的公共字段；基线
Debug/Release、文档和既有 package/architecture gate 结果已记录。

### S7A-1：Typed Kernel 边界和模块骨架

**目的。** 建立不泄露实现依赖的内部/preview C++ 边界，不先接入 Player 或渲染器。

**必须冻结的对象。** 以 [Gameplay V2 ABI](../../../api/GAMEPLAY_V2_ABI.md) 的类型清单为权威来源；
S7A-1 至少覆盖下列角色（括号内是 V2 ABI 的候选类型名，**命名本身仍待冻结**）：
`Tick`/时间区间、输入规范化（`NormalizedObservation`）、`InputDomain`、`Action`（`InputAction`）、
位置/通道（`ChannelRef`）、`Amount`（`DomainAmount`）、来源类别（`SourceClass`）、sequence、
`JudgementRequirement`（`RequirementRecord`）、`PatternRef`、`MeasureSpec`、
prepared grace 与资源意图（`PreparedGrace` / `GraceResolutionPolicy` / `ResourceClaimIntent`，
取代原 `GripPolicy`）、`JudgementFact`（`FactRecord`）、`JudgementResult`、`JudgementIdentity`、
诊断和 capability。
实际命名、整数宽度、所有权、异常边界和序列化由实现批次与 ABI 一起冻结。
冻结范围（2026-10-02，**S1-03**）：以 V2 ABI 的九域 126 条为**完整追踪目录**，以本节列出的角色及
生命周期骨架所需依赖为 **S7A-1 实施范围**；既不以旧的 17 项作为封闭清单，也不一次冻结 126 条的
全部表示。ABI 中每一条按"本批次角色 / 边界冻结"或"登记为后续批次阻塞项"（后者须带裁决编号与首次
消费批次）登记，核对方式是**按域计数、无遗漏**。

**模块与安装边界（2026-10-02，S1-04）。** 新建 `engine/judgement/` 作为内部 STATIC target
`cuexis_judgement`；保留现有 `cuexis_gameplay` stub 及其 Runtime 依赖，**不做别名**；本阶段**不拆**
`cuexis_input`，输入层以独立头文件分区 + 架构检查约束；采用安装选项 1——不安装判定公共头、不新增
公开组件或 Playback 方法、本批次不触发 SDK API 升版；`cuexis_judgement` 加入
`CUEXIS_STATIC_IMPLEMENTATION_TARGETS`，沿用"仅静态包导出内部 archive、无头文件"的既有机制。
`tools/gameplay_assembler/` 与 `tools/chart_candidate/` 的名称已接受，但工具实现仍属后续批次。
退出判据：active-target 清单、allowlist、架构检查与静态/shared consumer 验证通过，且安装树中**没有**
判定头文件。决策登记见
[TARGETS_AND_DEPENDENCIES.md §9](../../../proposals/gameplay-v2-acceptance/TARGETS_AND_DEPENDENCIES.md) 与
[RULING_WORKSHEET.md](../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) 的 S1-04。

**本批次的冻结范围（2026-10-02，S1-03 / S1-05）。** 本批次只冻结类型**角色与边界**、时间域分离，
**不**冻结 `InputDomain` 枚举集（第 6 轮 P2-05）、`Tick` 存储宽度、时间转换与 tie rule（第 2 轮
CM-T03 / CM-T04）；这些登记为各自消费批次的阻塞项。禁止用默认枚举值、临时整数 typedef、序列化编码或
"伪成功"实现绕过；受影响的按值字段、运算与可运行方法必须移出本批次。可用性判据：骨架可独立编译，
其实现与测试都不依赖上述未决表示或行为。

**工作内容。**

1. 选择模块位置和 target 方向，使 Input/Judgement/Replay 不依赖 SDL、OpenGL、Audio、World、
   EnTT 或 JSON DOM；公共 header 只出现 Cuexis 类型和已批准的 `Result`。
2. 建立 create/configure/prepare/submit/advance/query/snapshot/seek/reset 生命周期骨架；prepare
   失败不得产生半准备会话，运行期不得修改 requirement 集合、preparedGrace、仲裁策略或
   Interface 投影。
3. 为所有可变状态标注 owner thread、读取时点、快照归属和重置行为；析构、实时路径和公共
   边界不得抛异常。
4. 为诊断建立稳定 code/category、字段路径、requirement/identity 分量和预算上下文；不把
   unsupported capability 映射成旧 kind。

**验证。** 头文件泄漏扫描、依赖 allowlist、架构脚本、静态/shared 安装 consumer、异常和
   ownership 编译检查；无 GPU 的空会话、prepare 失败和销毁路径。

### S7A-2：输入规范化、映射和时钟边界

**目的。** 使设备输入进入唯一、可回放、与渲染解耦的规范流。

**工作内容。**

1. 固定作者 RationalBeat 与运行时整数 `judgementTick` 的 TimebaseProfile；Stage 7A 推荐
   `engine.tick.us.v1`，并为 tempo、stop、负 Beat、同 Tick 碰撞、pause/resume、seek、offset、
   finalization watermark 和舍入边界建立 exact mapping。不得把音乐 Beat、会话物理时间和渲染帧
   时间混成一个未命名 Tick。
2. 定义离散边沿与连续量的边界：Stage 7A 只开放离散 press/release/update（例如按钮状态更新）
   的规范化；连续轨迹、
   minimumRate、reconstruction 和 discontinuity capability 留给 S7B-1。离散 press/release 保留
   规范 observation tick，不读取渲染帧时间。
3. 实现显式 `InputMapping`，覆盖键盘、鼠标、触摸、手柄和外部控制器的最小映射；映射的
   source identity、版本、量化和默认值进入 session identity。
4. 规定到达时间、设备时间、音频/Chart 时间、渲染帧时间和 `observationTick` 的转换；时间
   回退、越界、重复 sequence、跨 discontinuity 和负时间都要有明确行为。
5. Stage 7A 不定义连续量重采样或周期 predicate 的采样相位；若输入声明连续轨迹、最低上报率
   或 reconstruction capability，prepare 必须稳定拒绝并指向 S7B-1。后续 capability 必须单独
   证明联合采样相位和最坏延迟，不能借用 7A 合同。
6. 使实时输入和 Replay 规范事件走完全相同的 normalize/submit 路径；原始设备事件可以留在
   诊断中，但不是 Replay 语义输入。

**验证。** 同一设备类别的离散 press/release/update、时间抖动、输入顺序、重复 sequence、
   时间回退和 discontinuity golden；连续轨迹、最低上报率和重采样输入必须命中稳定 unsupported
   诊断。跨 GCC/clang/MSVC 的规范事件逐字节一致。

### S7A-3：Requirement prepare、编译和离线 typed assembler

**目的。** 把 Chart/CXT typed 数据变为只读、可测量、带 identity 的 prepared requirement；
Playback 不读取 CXT AST，也不在运行时展开未编译 Pattern。

**2026-10-04 设计收口。** 完整组合为 G2-S2 + G2-T4 + R2-K4 + P1-W1 + E1；
S2/T4/K4 语义见 [V2 Spec](../../../formats/GAMEPLAY_V2_SPEC.md) §3.8.10–§3.8.12，
首次 Packed 消费字段仅见 [Gameplay Capsule v2 format](../../../formats/GAMEPLAY_CAPSULE_V2_FORMAT.md)。
[设计收口报告](../../../stage_reports/stages/stage-07/implementation/2026-10-04-s7a-3-design-closure.md)
只证明文档合同闭合；本批次的 Reader/Writer、identity 修正与 E1 实测门禁仍需实施，不缩减验收。

**工作内容。**

1. 实现 typed Chart/CXT/candidate 输入到 Canonical Gameplay Graph/Requirement/Pattern/Measure 的离线
   assembler，派生 feature、capability 和 resource closure；Writer 不替调用方修复缺失 feature。
   `gameplay.version = 2` 是唯一 V2 语义入口；旧 revision 显式迁移或稳定拒绝。CXC `gameplay-graph`
   与 Packed entry 必须得到同一 graph。
2. 实现 Stage 7A 所需的 Pattern 原语 atom、sequence、choice、bounded repeat、skip、instant 和
   complement；这里的 `bounded repeat` 只允许 prepare-time 的有限静态结构，不是运行期计数器、
   累积器或动态 Requirement 生成。`sameContact`、连续轨迹和跨 Requirement relation 仅登记为后续 capability，遇到
   时稳定拒绝。非确定匹配固定为 `leftmost-first`；检查 Pattern 与 arm/deadline 的包含关系、
   终止性、状态数、展开数量和时间/内存预算。
3. Measure 允许多个 phase/category 分量，逐分量套用 grading；Stage 7A 固定覆盖 Hold head/body 与
   显式 Release/tail。Release/tail 是同一 Requirement 的可选 phase，不自动生成独立 Requirement；
   仅当输入内容要求 Release/tail 语义却未显式声明 phase 时稳定拒绝，不得按隐式 legacy profile
   推断。
4. 对 Stage 7A 的单一 capacity=1 exclusive resource 只在 prepare 阶段解析 claim/ownership/preparedGrace；
   handoff Hold、接触跟随 Slider、`sameContact` 和跨 Requirement resource migration 仍是后续
   capability。量化、范围、继承、`allowChartGrace`、sticky/observe 非法覆盖和
   `graceResolutionPolicy` 均稳定诊断；准备后最终值命名为 `preparedGrace`，与后续 capability
   的 `continuityGrace` 等字段不得混用。准备后
   只读最终值。
5. 生成 chart/content/prepared identity 的规范字节；相同最终 grace 与 policy 即共享 judgement
   identity，显式/继承来源只进入 content identity 和诊断。
6. 失败必须原子：非法引用、未知 capability、pattern 不终止、范围溢出、资源冲突、预算超限
   或 identity collision 时不发布半成品、不改变已有 active session。

**验证。** typed file/memory source 一致性；CXT/Candidate 正例和负例；输入数组、参数和引用
   顺序置换不改变 canonical bytes；真实内容预算测量；Pattern 参考谓词与编译产物差分；宽整数
   范围证明覆盖平方、乘法、加法和窄化；不把研究切片测量直接变成生产限额。

**第 7 轮准入门禁（2026-10-03，`P1-01` / `P1-02` / `P1-15` / `P2-07`）。** 本批次的准入门禁另含第 7 轮
   裁定的三组规范文本与一项命名统一，语义正文见 [GAMEPLAY_V2_SPEC.md](../../../formats/GAMEPLAY_V2_SPEC.md)
   §3.8.7 与 §5.1.1：

① **判定域 typed contract**（静态 typed 判定域记录；几何属 Gameplay closure；坐标系在判定域内
   声明量程、不依赖宿主视口 / 分辨率 / 表现层 transform；与 Presentation transform 完全隔离；动态 frame
   属 7B+ 候选并稳定拒绝）；

② **CXT local relation 的全局合并规则**（prepare 编译期完成；稳定 ID = 来源
   文档 identity + 声明序号；同名拒绝；跨 invocation 引用必须显式；按稳定 ID 排序使重排不改结果）；

③ **Chart v5 inline 与 CXT v2 emission 的等价性**（canonical graph 与 derived capability closure 逐字段
   相等；忽略 `sourceMap` 与物理顺序；**逐字段 semantic diff 作为本批次的 golden 证据**）；

④ **三项命名
   统一**（`source closure` / `diagnostic map` / `content-artifact identity`）。

   **以上四项均不新增工作项**，
   只把 `## S7A-3` 既有验证项实现时必须满足的语义边界写明；`CM-X05` 的 W 缺口登记仍按
   SUPPORT §6.1 在首次消费前完成。

**S7A-3 第二半 part 1 实现复核裁定后的范围收窄（2026-10-03；三轮实现复核 + 两张落地位置卡）。**
   本批次的 Pattern arm / deadline **包含性门禁**与**两容量声明**经三轮实现复核（`reject` 0.94 / 0.91 / 0.93）
   与两张落地位置卡（`adopt` 0.94 / 0.97）裁定后，范围与语义边界固定如下；语义正文见
   [GAMEPLAY_V2_SPEC.md](../../../formats/GAMEPLAY_V2_SPEC.md) §3.8.8 与 §8.2：

① **包含性门禁状态化**（卡 1，`adopt` 0.88）：门禁返回独立的
   **`ContainmentStatus{contained, gate_incomplete}`**（`core::Result<ContainmentStatus, core::Error>`）；
   `gate_incomplete` 是**非诊断状态**、不是内容超限诊断；**装配路径必须显式保守拒绝**，
   不得把"成功返回"当作"已包含"。

② **两容量是需求级声明字段**（卡 2，`adopt` 0.83）：非空 Pattern 的真实 arm / deadline 容量由
   **`RequirementRecord.maxArmElements` / `maxDeadlineElements`** 承载；任一容量**缺席或装配时仍不可测得**
   ⇒ `gate_incomplete` ⇒ **装配原子拒绝**；只有**已接受容量被可证明超出**才走既有 `arm_bound_exceeded`。

   状态数下界的判据是"**最长可接受轨迹长度 `L` + 1**"（受检加法），**不是**"份数 + 1"：
   `repeat(skip,1,UINT64_MAX)` 的份额是 `UINT64_MAX`，但它只接受空词（`L = 0`）、真值状态数为 **1**，
   按"份数 + 1"会推出 ≥ 2^64 而**误拒**该例；`maximumLengthGap`（有限但不可表示）仍可推下界，
   而 `complement` 一类 `maximumLengthUnbounded` **不得**推下界。**计数缺口本身不是拒绝理由**。

③ **两容量进身份、且只经身份**（卡 3 `adopt` 0.82 ＋ 卡 4 `adopt` 0.94 ＋ 卡 5 `adopt` 0.97）：
   **有效容量值**进入需求身份投影；`0` 与 `UINT64_MAX` 是普通值、**不用哨兵值表示缺席**；
   缺席或 pending **不产生可发布的编译身份**；字节落点是 `writeChartProjection` 的每条 requirement 投影
   （`writeRequirementIdentity` 之后、`writePatternNode` 之前），**不新增投影分量**；

   **`canonicalCompare` 不改**——以 `identity` 为首键已能区分"仅容量不同"的记录，再比即冗余死代码——
   但为满足既有**逐字段语义 diff 合同**，`compareRequirement` **必须**在 `.pattern` 之后、`.measure` 之前
   为两容量各加一条 `compareValue`；四态顺序 **`缺席 < pending < measured(0) < measured(其他值按数值)`**
   由**唯一共享辅助函数**规定，投影与身份编码**都调它**、禁止各自内联（防口径漂移）；

   **不改** `ReferenceKind::requirementIdentityProjection`、`closureOf` 与 Spec §3.8.6 归属矩阵——
   该标识是归属矩阵的**引用类别**，**不是**投影函数。

④ **本批次必须收口的两处既有状态**：`PatternArmBound` 已具备两容量字段
   （`MeasuredParameter<std::uint64_t>`，该类型以 `optional` 表达未测、**无零默认无哨兵**）
   但全仓**无构造点**，而 `RequirementRecord` **尚无容量字段** ⇒ 装配消费点必须**新建**构造并把声明字段
   映射为 `MeasuredParameter`；既有"无有限最长长度 ⇒ 保守拒绝"的措辞**必须**改为 `gate_incomplete` 并补测试。

⑤ **装配消费点（以真实 arm / deadline 容量调用门禁并验证原子失败）已落地**；`gateIncomplete` 在装配路径中保守拒绝，
   已测超限仍走既有诊断，身份只写入已测有效值。数值限额在 S7A-9 前**不冻结**；本批次只能按受限功能验收口径退出，不得声称容量整体证明完成。
   逐字裁定与主控独立复核见两份带日期记录：
   [S7A-3 第二半 part 1 实现裁定落地记录](../../../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-3-implementation-rulings.md)
   与 [S7A-3 落地位置裁定（卡 4 / 卡 5）](../../../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-3-landing-location-rulings.md)。

### S7A-4：Judgement Kernel 生命周期、Tap/Hold/Release 和仲裁

**目的。** 交付最小、无渲染、确定性的可玩判定。

**2026-10-05 完整方案与交接。** 推荐组合 B 已细化为
[实施交接](../../../archive/stage-07-planning/s7a-3-4-implementation-handoff.md) 的 A–G 卡：先 S7A-3 余项、candidate revision3、
author 双路与 affine，再 late/contact/phase/kernel/timer/Fact。新增合同唯一落点为
[execution Spec](../../../formats/gameplay-v2-execution-profile.md)、
[author profile](../../../formats/gameplay-v2-author-profile.md) 与
[typed supplement](../../../api/gameplay-v2-execution-types.md)；理由见
[ADR 0045](../../../adr/0045-gameplay-v2-execution-profile.md)。
上述设计交付当时只补齐 late-policy 合同，不含产品实现；状态预算缺测不判通过。
同日 owner 授权实施后，本工作区已落地 A/B/D/E/F 与 E1 逐行审计；Windows 与 Linux GCC/Clang 完整矩阵均通过，S7A-3/4 受限功能验收完成，容量整体证明未完成；
见 [本地验收证据](../../../stage_reports/stages/stage-07/verification/2026-10-05-s7a-3-4-functional-acceptance.md)。
不能把局部功能通过记为容量整体证明或 Stage 7A 完成。
旧推荐比较保留于 [推荐登记](../../../proposals/gameplay-v2-acceptance/S7A-4_RECOMMENDED_DESIGN.md)，
已有实现/hosted 证据见 [余项报告](../../../stage_reports/stages/stage-07/verification/2026-10-05-s7a-3-remaining-evidence.md)。

**固定 Tick 顺序。**

```text
1. 激活到期 requirement
2. 处理到期 Release/tail 或 hard-deadline timer
3. 应用规范化输入事件
4. 每个事件重建候选并按 resource / 显式 K4 pair / fanout 仲裁；claimKey/requirementId 仅寻址
5. 按**唯一规范事实总序** `(commitTick, originKindPriority, canonicalOrdinal)` 规范排序 Fact；其中
   `canonicalOrdinal` 由引擎从 `(originScope, originOrdinal, localOrdinal, phasePriority,
   factKindPriority)` 派生，**不得**使用 `ingressSequence`、容器顺序或线程完成顺序（第 5 轮
   `S7A5-R01`；Spec §3.9 第 3 条、§3.17）。Fact 的 `tick` 即 `commitTick`；`observationTick` 只用于
   定位 observation origin
6. Tick 末提交 Hook/signal，下一 Tick 才可见
```

**工作内容。**

1. 实现 Tap 的时间、域、动作和位置匹配；明确 `[start,end)`、严格边界、重复输入、迟到输入、
   stray 和 Miss。
2. 实现 Hold 的 head、body、Release/tail phase、deadline 和 coverage；Release/tail 是同一
   Requirement 的终止阶段，不能隐式生成第二个 Requirement。头部、主体和尾部 Fact 可以并存，
   但必须通过 phase/causal key 关联；未进入 7B 的复杂尾判稳定拒绝。
3. 实现单一 `capacity=1` exclusive resource 的 `free -> held -> terminal/free` 生命周期。
   每个 Requirement 最多声明一个 resource claim；冲突按 prepare 后显式 `(priority,tieRank)` 升序决定，
   同资源 occupying pair 必须唯一，claimKey/requirementId 不作 fallback；不能
   通过输入顺序、线程顺序或全局最优搜索改变赢家。capacity>1、Chord、handoff 和 Slider
   continuity 不属于 7A。
4. 实现 `observe`、`consume`、单一 resource 的 `claim`、同一 resource 的终止后槽位复用、
   `strayWhen` 和固定 `fanout=1`；`sameContact`、handoff、连续轨迹和跨 Requirement
   资源迁移留给后续 capability。7A 不实现多资源 fanout，也不把“有限 fanout”解释成多指能力。
5. 处理同 Tick 输入、实例枚举、输入数组顺序和 timer 顺序，保证 causal total order 消除实现
   容器顺序影响。

**验证。** Tap/Hold/Release/tail 全边界矩阵；单 resource 冲突、重复/冲突输入、observe、
   固定 fanout=1、终止后槽位复用、stray 两种政策、暂停/Seek/reload/audio discontinuity；capacity>1、
   Chord、handoff 和全局最优 solver 必须稳定拒绝。渲染帧率、World 遍历和后端替换不会改变 Fact。

**第 4 轮裁定后的范围收窄（2026-10-03）。** 第 4 轮（仲裁、资源与事实序；进入 S7A-4 前）已裁定并落进
    [GAMEPLAY_V2_SPEC.md](../../../formats/GAMEPLAY_V2_SPEC.md) §3.9–§3.13：①外层**六步 Tick 顺序**与内层
    **八阶段 Coordination window** 是**唯一**映射（八阶段＝第 4 步内部完整展开），阶段顺序与语义进
    judgement identity 的 engine 组件、不进 chart/content identity（§3.9）；②7A 资源子集冻结为
    **`free` / `held` / `terminal`**，`gap`、`handoff_pending`、**非零资源 `declaredGapGrace`**、handoff、`capacity > 1`、
    owner 集合与并列 slot **一律稳定拒绝**（§3.10）；③资源身份规则（`resourceId` 只在 prepared canonical
    graph 命名空间内、唯一 slot、slot/lease/最小 contact handle/claim identity 进 canonical graph、
    `contact end` 不复用旧 handle、`observe` 只产生 Observation 且不占用资源，§3.10）；④**prepare / compile
    solver** 与 **runtime coordinator** 分离（**不解析/编译 solver；runtime coordinator 只执行已准备的
    确定性 policy**，名称 `coordinator.policy.greedy_v1`，§3.12；`P2-02` 措辞由第 7 轮 `S7A7-R07` 收官，
    2026-10-03）；⑤早 / 晚判定与 Hold head/body/tail error 分别记录（§3.11）。
    本节的 `capacity>1` / Chord / handoff / Slider continuity 拒绝面因此由"不属于 7A"升级为**稳定拒绝**，
    并新增 **非零资源 gap grace**、**owner 集合**与**并列 slot** 三项；Requirement preparedGrace 按 T4 消费，
    不属于该拒绝清单；实现还须遵守 §3.13 的 S7A-4 不得消费清单。
    本节的工作内容 1–5 与上列内容**不冲突、不新增工作项**；仍未冻结的部分（阶段编号、序列化编码、预算与
    窗口数值、`SolverProfile` 默认列表与 `max*` 数值、proof 编码、`terminal` 编码）只阻塞**首次消费它们的
    后续批次**，不是本批次门禁。

### 后续实施队列（2026-10-06）

S7A-3/4已达到受限功能验收，容量整体仍未完成；新SHA的hosted证据与归档见
[交接报告](../../../stage_reports/stages/stage-07/handoffs/2026-10-05-s7a-3-4-hosted-and-handoff.md)。
owner已明确下一轮目标为 **S7A-5.1–5.5 + S7A-6.1–6.5**，按本文§3.2的J0–J7先5后6、
共同验收。五批依赖、已选方向和剩余未定项由本文统一维护；
九项各五套备选的比较正文见 [5/6实施输入](../../../proposals/implementation-input/stage-07/s7a-5-6-design-selection.md)。
2026-10-06追加复核的U01–U10也各有五套处置比较；推荐与准入归本文§3.2，理由和反例归实施输入§13。
接手文档导出到owner桌面，不在active目录维护第二份执行正文；
原接手/排期快照见 [旧路径映射](legacy-paths.md)。本轮只有文档准备，容量整体仍归9。

### S7A-5：Ruleset Fold、Score、Combo 和 Statistics

**目的。** 将 Fact 以声明式、可重放的 Ruleset 逻辑折叠为会话结果。

**工作内容。**

1. 实现 Ruleset Interface、arbitration、fold、modules、defaults、Hook ownership、programPolicy
   和 outcomeScope 的只读 prepared 视图；模块顺序冻结在 package manifest。
2. 明确同一写目标唯一 owner；派生 Hook 使用可交换、可结合的合成算子；信号只在下一 Tick
   可见；折叠不得生成/删除 Requirement 或改写已产生 Fact。
3. 定义 `JudgementOutcome`、grade、timing error、ScoreState、ComboState、可选 LifeState 和
   `StatisticsSnapshot` 的初始值、增量、溢出/饱和及 reset/seek 行为。
4. 把内容、Ruleset、Loadout 和 session 配置对结果的影响写入 identity；表现资源、默认绑定和
   UI 文案不进入 judgement identity。
5. 将一次 Tick 的 Ruleset 处理实现为**三阶段**（第 5 轮 `S7A5-R03`）：①追加并封存**已排序**的 Fact
   Ledger；②**原子校验并提交全部** StateDelta、Score/Combo/Statistics 与 RuleEffect——只有全部通过范围、
   冲突、预算和 owner 校验才提交；③**只从已提交 Fact Ledger 与 FactBinding 投影**只读 PresentationEvent。
   第二阶段任一失败时：**不追加 fault Fact**、**不发布** RuleEffectEvent 与 PresentationEvent、**保留旧
   state**、**不提交半个 Tick**，session 进入可查询的 `faulted` 状态。`faulted` 的 `submit` / `advance` /
   `seek` / `replay` / 就地 `reload` 与 **`snapshot`** 一律**稳定失败**；只有**显式 `reset`** 或**创建
   替换新 session 的 `reload` / recovery** 可以离开该状态，且**不得恢复或伪造未提交的 StateDelta**。
   `RegisterKind` 只接受 `exclusive` / `commutative_monoid` / `ledger_derived` 三类（`S7A5-R04`）；
   7A 只用内置、静态注册的 Ruleset，任何 `cuexis.ruleset` package 输入命中 R-09（`S7A5-R05`）。

**验证。** 同 Tick 多 Fact 顺序置换、模块顺序变更、Hook 冲突、信号延迟、combo 断连、life 边界、
    长局累计/饱和、reset/seek/replay 结果一致性；StateDelta 冲突/溢出/预算失败必须验证
    **不追加 fault Fact**、旧 state 保留、Fact 保留、RuleEffect 不可见、PresentationEvent 可由已提交
    Fact 重建，以及 `faulted` session 的 `submit`/`advance`/`seek`/`replay`/`reload` 与 **`snapshot`**
    错误。Ruleset 缺 capability、未知 outcome、非法 owner、三类之外的 `RegisterKind` 与 package 输入
    稳定失败；Life capability / policy / `LifeState` 初值必须以 `capability.disabled` 稳定拒绝。**不得**
    消费 Spec §3.22 的 S7A-5 不得消费清单任一项（ingress 排序、correction、Ruleset package、Life、
    默认 grade 表与默认数值限额、未冻结字节编码、未测量 seek 数值、表现资源 / UI / 默认绑定对 judgement
    的影响），也**不得**把 `phasePriority` 用作评分。

### S7A-6：Identity、Replay、Snapshot 和 Seek

**目的。** 使结果可验证、可重放、可定位并能在不依赖渲染的情况下恢复。

**工作内容。**

1. 落实四分量 identity：`engine`（判定语义、tableId、阶段顺序）、`ruleset`（Interface 投影、
   模块/折叠顺序、Build hash、能力）、`chart`（编译 Pattern/Measure、锚点、最终 preparedGrace）、
   `session`（Loadout、默认 grace 来源、离散输入规范化 profile 和 JudgementConfig）。7A 不引入
   连续重采样格点或连续输入采样相位；它们属于独立后续 capability。
2. Replay 头部包含格式版本、四分量 identity、`NormalizationProfile` metadata、生效 late-policy metadata、
   `eventCodecId`、规范化事件数、编码字节数与 `ReplayDecodeBudget`（第 5 轮 `S7A5-R06`；拼写规范：
   **类型 `EventCodecId`、字段 `eventCodecId`**）；事件流使用 S7A-2 的规范事件，不保存不可复现的设备
   私有对象。**具体 codec 编码、字节布局与 reference fixture 由 S7A-6 冻结；预算数值仍阻塞 S7A-9。**
3. 实现非法版本、identity mismatch、事件乱序、时间回退、截断、重复、超事件数/字节数、
   解码时间超限和未知 capability 的稳定错误。
4. 快照无损保存活动实例、Pattern 状态、Release/tail phase、exclusive resource 状态、
   finalization watermark、未提交窗口、preparedGrace、离散事件 sequence 状态、Hook 快照、Fold 状态、统计、
   pending signal queue、Fact cursor 和 session/fault state；第一版采用全量快照。Snapshot header 字段集为
   `formatVersion`、四分量 `JudgementIdentity`、`factSemanticRevision`、`stateSchemaRevision`、
   normalized event count、Fact count、payload byte count 与 typed byte-budget descriptor
   （`S7A5-R06`）；**`factSemanticRevision` 进 engine identity、`stateSchemaRevision` 不进**。
   FactBinding 与 prepared immutable graph 在 header identity 验证后**重新取得、不重复保存**。
5. Seek 从最近快照推进，必须逐位等于从起点运行；快照间隔为引擎内部参数，不进入 identity。
   `Seek 正确性已承诺；SeekLatencyCommitment 的类型和收紧关系已定义，具体 maxSeekLatency 数值待
   S7A-9 测量并接受。`（第 5 轮 `S7A5-R08`：会话只能收紧、不得放宽；**具体 `maxSeekLatency` 数值禁止**
   进入 Snapshot / Replay 字段、运行时约束或对外承诺，提供具体值稳定拒绝。）
6. Snapshot 恢复、Seek 和 Replay 不保存 Animation layer 临时值或 HostOverride token；Presentation
   adapter 必须从已提交 Fact Ledger 和 FactBinding 重建当前仍有效的 PresentationEvent。**`faulted`
   session 不可新建 snapshot**（`snapshot` 稳定失败；快照闭包仍含 fault 诊断 / 状态，但那不构成
   "faulted 可产出新快照"）；`faulted` session 只能被显式 reset 或以替换新 session 的 reload / recovery
   替换，恢复旧快照不得伪造未提交的 StateDelta。**不得**保存表现缓存、未提交 delta、连续采样状态或
   snapshot interval identity，也**不得**把 typed byte-budget descriptor 解释为已接受的数值承诺。

**验证。** 录制/回放、跨进程/跨工具链 golden、截断和篡改、不同快照间隔、任意 seek 点、
   pause/reload/discontinuity、faulted session、长事件流和预算峰值；Replay 与实时输入的
   Fact/Result/Score/Combo/Statistics 逐项一致，Snapshot 恢复后的 PresentationEvent 与连续播放一致；
   两套 header 字段集与 `SnapshotPayload` 闭包（Spec §3.18 十四条）无损往返；类型 `EventCodecId` /
   字段 `eventCodecId` 拼写统一。**不得**消费 Spec §3.22 的 S7A-6 不得消费清单任一项（S7A-5 的 8 项 +
   表现缓存 / Animation 临时值 / HostOverride token、未提交 delta、连续采样状态、snapshot interval
   identity、把 typed byte-budget descriptor 当作已接受数值承诺），也**不得**把未测量数值（预算 /
   `maxSeekLatency`）或未冻结编码（两套 header 字节布局、Event / Fact codec、`FactId` / `CommitId`
   编码）当作已冻结。

### S7A-7：Playback、Chart candidate、Headless、Player 和 external consumer 集成

**目的。** 将 kernel 接入既有产品边界，不把 Judgement 类型倒灌到 Runtime 或 Presentation。

**工作内容。**

1. 在 PlaybackSession/Prepared Playback 中增加显式创建和查询入口，保持 owner-thread、Result、
   生命周期和公共头 ASCII 规则；旧 Playback 入口行为不因无输入而改变。
2. 增加 CXC v1 `gameplay-graph` Playback entry，并让 `packed-chart` 与 Graph entry 在 prepare
   前都校验 gameplay.version、Ruleset binding、capability closure、resource closure 和 compiled
   semantic identity；二者必须恢复同一 Canonical Gameplay Graph。
3. 增加 Gameplay-to-Presentation adapter：只消费已提交 FactBinding，映射为现有
   HostOverride/OverrideToken 或 typed PresentationEvent，不读取 Judgement 内部状态、不直接写
   Component。Stage 7A 至少验证 early/exact/late/Miss 的 `render.visible` bridge；early/late 只
   改变表现生命周期，不改变 Fact、Score 或 Replay identity。
4. 让 Chart v5 Core/Packed candidate 提供 S7A-3 所需的 prepared requirements 和 capability；
   v4/CXT v1 回退继续工作，未支持的 v4/v5 内容稳定拒绝。
5. 无 InputEvent 的纯播放和 Studio Preview 中 Judgement 保持休眠，不能改变 FrameSnapshot、
   FrameDigest 或 Presentation candidate。
6. Player 仅显示/播放 JudgementResult、Combo、Score、Miss 和基础反馈；不得在 Player 内维护
   私有 Runtime 或第二套判定路径。
7. 建立不带 GPU 的 headless consumer、安装树 static/shared consumer 和具名 Reference Host
   回归；实时输入与 Replay 必须经过同一公共入口。

**验证。** v4、v5 candidate、`packed-chart`/`gameplay-graph`、file/memory source、
   Playback/Headless/Player/external consumer 的生命周期矩阵；Graph/Packed semantic equivalence、
   early/exact/late/Miss bridge、默认 production 与 candidate opt-in 隔离；无 SDL/OpenGL/EnTT/JSON
   DOM 泄露；prepare/submit/advance/snapshot/seek/reset 的成功、Ruleset fault 和失败事务均覆盖。
   **第 6 轮已裁定（2026-10-03，`S7A6-R01…R12`）**：本批次另须覆盖"码表 CTest 校验项"——诊断码表
   `schemas/cuexis.gameplay-diagnostics.v2.codes.json` 的校验项（code 唯一性、category / severity /
   faulted 映射完整性、`stableRejectCode` 已登记、输入 / 几何三码不可重复扩展）须随本批次通过（该项由
   并行工具线在 CMake 注册），并逐条遵守本文 `## S7A-7 限定冻结范围与登记规则（2026-10-03）` 的
   不得消费清单。**本节不新增工作项**；`CM-P04` / `CM-P06` / `CM-P08` / `CM-D04` / `CM-X01` 五条合同项
   已由第 6 轮转为 `accept`，不再是本批次的 `open` 门禁。

### S7A-8：Stage 6 交接收口和版本门禁

**目的。** 在不重开 Stage 6 的前提下，完成四项登记的处置和 SDK/实验入口的可追溯性。

**工作内容。**

1. 实现并验证 candidate 构建/安装 flavor、显式 entry、Player `--candidate-entry`、项目/CXC
   metadata 选择和默认 OFF 保护；至少一个 preset 或 CI 真实开启 candidate 并被测试消费。
2. 接入离线 typed assembler 和 feature/resource closure 派生；移除测试注入 feature 的生产后门，
   记录 candidate 输入到 prepared requirement 的完整 identity 链。
3. 对 Reference Host 的六动词命令循环（**`open/play/pause/seek/reload/quit`**，即 ADR 0042 的六动词；
   实现另有 `tick` 运行时步骤，见缺陷 `D-2` / `D-7` 与第 7 轮 `S7A7-R09`）做同 SHA 交接回归，覆盖成功、
   拒绝、旧 active 状态和 reload/seek/discontinuity；不把旧报告复制成新证据。
4. 先提交 version-gate 的条件化放行设计：默认阻止 SDK API 变化，只有显式、可审计的放行输入
   才允许 0.7.1；再运行 update_version、fresh configure、clean-first build、安装 consumer 和
   hosted gate。若 owner 接受不升版本，必须在 ADR/版本文档记录例外和后续归属。

**退出门禁。** 四项均有实现和门禁证据，或有已接受的明确例外；任何一项不能以“Stage 6 已关闭”、
   “未来 Stage 8 会处理”或本地脚本单独通过替代。

### S7A-9：最终硬化、跨平台证据、Stage 8 handoff 和关闭

**目的。** 把所有实现证据收敛成可审查的 Stage 7A 关闭包。

**工作内容。**

1. 运行 Debug/Release/shared/headless/MinGW 以及 Linux Quality、Windows MSVC、Windows MinGW
   hosted 矩阵；sanitize/coverage/clang-tidy 只按仓库支持的平台执行，不能伪造 Windows 等价证据。
2. 对代表性真实内容和最坏内容测量 requirement 数、activity、Pattern 状态、Fact、Replay、
   快照、Seek、prepare 峰值和运行时间；分别记录硬门禁、软目标和环境限制。
3. 运行架构、target allowlist、公共头泄露、static/shared package、external consumer、version
   gate、docs、git diff check 和许可证检查；GPU/真实设备/音频证据单独标记。
4. 建立 Stage 8 handoff：S7A typed contract、v4/v5 输入映射、requirement/Replay identity、
   golden fixture、capability/拒绝表、预算 profile、迁移注意事项和未纳入 7B+ 清单。
5. 形成 dated completion report、问题 disposition、未执行项、回滚路径和 owner acceptance；未
   满足任何硬门禁时状态保持 active/blocked，不缩小矩阵宣称完成。

### 3.1 Stage 7A 小目标核验台账

下表把 S7A 批次拆成可以独立分派和独立退出的小目标。每个小目标必须单独记录实现 SHA、测试
fixture、正例、负例、命令、原始输出和未覆盖项；“依赖批次已完成”不能替代本行核验。

| ID | 小目标 | 必要产物 | 核验标准 |
| --- | --- | --- | --- |
| S7A-0.1 | 固定执行基线 | dated baseline、HEAD/分支/工具链/preset 表 | 工作区状态、版本、SDK API、依赖和 hosted 入口均可复现；重新运行同一命令得到同类基线；历史 SHA 未被冒充为当前证据 |
| S7A-0.2 | 合同逐项台账 | ADR/Spec/ABI → 字段、行为、诊断、owner 的映射表 | 每个公共字段都有来源、消费者、默认值、失败路径和 identity 影响；不存在“实现时决定”的开放项 |
| S7A-0.3 | 预算与证据计划 | 内容、稳态、快照/Seek、Replay、Packed 五类 profile 草案 | 每类预算有计数口径、测量输入、硬/软属性和超限动作；研究切片数值明确标为非生产限额 |
| S7A-0.4 | Gameplay V2 acceptance package 准入 | V2 owner acceptance 记录、revision 和旧合同映射 | owner 接受前禁止进入公共头和实现批次；若有例外，例外包含范围、期限、替代门禁和责任人 |
| S7A-1.1 | 公共 typed 类型 | 头文件、Result/诊断枚举、生命周期声明 | 外部 consumer 可编译；公共头无 SDL/OpenGL/EnTT/World/JSON DOM/第三方实现类型；非 ASCII 扫描通过 |
| S7A-1.2 | 会话状态所有权 | owner-thread、borrow/own、快照字段表 | 每个可变字段有唯一 owner 和读取时点；并发/重复调用/销毁测试不产生数据竞争或悬空引用 |
| S7A-1.3 | prepare/运行期事务 | create→prepare→submit→advance→query→reset 骨架 | prepare 任何失败均无半准备会话；运行期修改 requirements、grace、规则投影被稳定拒绝；旧 active 状态保持不变 |
| S7A-1.4 | 诊断合同 | code/category、字段路径和 identity 分量表 | identity mismatch、unsupported、invalid、budget、time、overflow 等最小类别逐一可触发；同一错误在不同容器顺序下诊断稳定 |
| S7A-1.5 | 模块与依赖门禁 | CMake target/allowlist/安装图 | architecture test、target allowlist、static/shared 安装 consumer 和 headless 配置通过；不得由测试 target 偷渡依赖 |
| S7A-2.1 | 绝对时间域 | Tick、区间、单调推进和 discontinuity 规则 | `[start,end)`、负时间、回退、相等边界、超范围和 pause/seek/reload 均有正反例；渲染帧时间不影响结果 |
| S7A-2.2 | InputMapping | 键盘/鼠标/触摸/手柄/外设最小映射 fixture | 相同映射规范 bytes 和 identity 一致；未知 device/action/channel、重复绑定和越界量程稳定拒绝 |
| S7A-2.3 | 离散事件规范化 | press/release/hold source fixture | 离散边沿保留原始观测时间；到达顺序、sequence、重复事件和跨设备 source identity 的处理符合 Spec |
| S7A-2.4 | 离散规范化边界 | press/release/update、sequence、discontinuity fixture | Stage 7A 不启用连续轨迹；离散 observation tick、重复/回退 sequence、越界和 discontinuity 均有稳定结果；连续采样能力转入 S7B-1 |
| S7A-2.5 | 采样 capability gate | continuous input opt-in/unsupported fixture | 未声明连续 capability 时稳定拒绝 position/update 轨迹；S7A 不把重采样相位或最低上报率写入默认语义 |
| S7A-2.6 | 规范事件编码 | canonical event bytes 和 replay input adapter | 实时与 Replay 输入经过同一 normalize/submit；字段、顺序、量化和 source 版本跨工具链逐字节一致 |
| S7A-3.1 | typed source 读取 | Chart/CXT/candidate typed adapter | file/memory 输入等价；JSON DOM 不越过 json_support；缺失、重复、未知字段按合同诊断 |
| S7A-3.2 | Pattern 编译 | Pattern IR/compiled state、reference predicate | atom/sequence/choice/repeat/skip/instant/complement 均有最小和边界 fixture；`sameContact`、连续轨迹和跨 Requirement relation 稳定拒绝；非终止、窗口外匹配和状态预算稳定拒绝；短轨迹穷举与参考谓词一致 |
| S7A-3.3 | Measure/phase 编译 | 多分量 Measure、phase/category、grading fixture | Hold head/body 的 Fact 分离且等级独立；分量顺序置换不改变规范结果；未声明 tail/release 不被隐式启用 |
| S7A-3.4 | prepared grace | explicit/inherited/zero/min/max/sticky/observe fixture | `allowChartGrace`、范围、量化、policy 和来源诊断正确；prepare 后 Hook 改变不追溯已准备值；最终值和 policy 正确影响 judgement identity |
| S7A-3.5 | canonical identity | chart/content/prepared identity 预映像 | 输入数组、参数、资源引用顺序置换不改变 canonical identity；改变语义字段必改变相应分量；表现资源不污染 judgement identity |
| S7A-3.6 | prepare 原子性和预算 | overflow、reference、closure、budget failure fixture | 任一失败不发布部分 prepared chart、不改变 active session；诊断指出字段/requirement 和实际计数；无 checked arithmetic 溢出 |
| S7A-4.1 | Tick scheduler | 固定六步顺序的 trace fixture | 每一步产生的可见状态、timer 和 signal 与 Spec 一致；同 Tick 的输入不会看到本 Tick 新 Hook；阶段顺序变更会改变 engine identity 或被拒绝 |
| S7A-4.2 | Tap | 时间/域/动作/位置边界 fixture | 命中、早到、晚到、正好边界、重复、冲突、无候选和 Miss 的 Fact、eventSequence、stray 完全符合预期 |
| S7A-4.3 | Hold | head/body/release/deadline/coverage fixture | 头部与主体可同时存在；中途断开、末端释放、超 deadline、零 grace 和严格 `<` 边界均稳定；不依赖帧率 |
| S7A-4.4 | 基础仲裁 | overlap、单 resource、claimKey、固定 fanout=1、observe、strayWhen fixture | 每事件候选逐次重算；实例枚举和输入数组置换不改变赢家；observe 可见但不消费；`noCandidate` 与 `consumeEmpty` 区分正确；sameContact/handoff/多资源 fanout 稳定拒绝 |
| S7A-4.5 | Release/tail 与 exclusive resource | release boundary、单 resource conflict、terminal/free fixture | Release/tail 只属于原 Requirement；`free -> held -> terminal/free` 只允许 capacity=1；重复 release、资源冲突、capacity>1、Chord、handoff、多资源 fanout 和全局最优 solver 稳定拒绝 |
| S7A-4.6 | 生命周期重建 | pause/seek/reload/audio discontinuity fixture | 同一绝对目标时间重建出的活动实例、Fact 和 timer 与连续播放一致；不把上一帧结果作为新基线 |
| S7A-5.1 | Ruleset prepare | interface/arbitration/fold/module manifest | 缺 module、unknown outcome、Hook owner 冲突、programPolicy/outcomeScope 非法均在 prepare 拒绝；manifest 顺序进入 ruleset identity；**只用内置、静态注册的 Ruleset**，`cuexis.ruleset` package 输入命中 R-09（`ruleset.package_unsupported`）且不读 hash / manifest / 迁移矩阵 |
| S7A-5.2 | Fact fold / transaction | 同 Tick 多 Fact、StateDelta conflict/overflow fixture | **唯一规范事实总序** `(commitTick, originKindPriority, canonicalOrdinal)` 后结果稳定（`canonicalOrdinal` 由引擎派生，**禁止** `ingressSequence` / 容器顺序 / 线程完成顺序）；Fact Ledger 先封存；StateDelta、Score/Combo/Statistics/RuleEffect 全部通过校验才提交；失败时**不追加 fault Fact**、**不发布** RuleEffect / PresentationEvent、**保留旧 state**、不提交半个 Tick，session 可查询 `faulted`；`faulted` 的 `submit`/`advance`/`seek`/`replay`/`reload`/`snapshot` 稳定失败，仅显式 reset 或替换新 session 的 reload / recovery 可离开且不伪造未提交 StateDelta；`RegisterKind` 只接受三类 |
| S7A-5.3 | Score/Combo | 命中、Miss、断连、饱和、grade 缺失、reset fixture | 初始值、增量、负值、饱和与连击断开明确；`Outcome` = `{hit, miss}`、Hold 三段为附属 phase-local outcome / error、grade 缺失即 absent 且不隐式升级、`TimingError` 为有符号整数 tick 差；长局无未定义溢出；**Life 在 7A 关闭**，任何 Life capability / policy / `LifeState` 初值以 `capability.disabled` 稳定拒绝 |
| S7A-5.4 | Hook/signal 可见性 | 同 Tick/下 Tick signal fixture | Hook 在 Tick 末提交、下一 Tick 可见；`commutative_monoid` 派生值顺序无关；`exclusive` 第二 owner / 第二写入、未知 operator、重复 contribution identity、非交换 / 非结合合成与 `ledger_derived` 直接写入稳定拒绝 |
| S7A-5.5 | Statistics snapshot | 统计查询和快照 fixture | 查询不修改状态；reset/seek/replay 后计数逐项一致（seek / replay 从 Fact Ledger 重建，reset 清空新 session 状态且不产生 Fact）；统计字段与 identity/版本关系明确；**不得**把 `phasePriority` 用作评分 |
| S7A-6.1 | 四分量 identity | engine/ruleset/chart/session golden | 改变各分量的语义字段只影响对应分量；同最终 grace/policy 的 explicit/inherited judgement identity 相同，content/diagnostic 保留来源差异 |
| S7A-6.2 | Replay header/codec | 版本、identity、事件/字节预算 fixture | header 字段集为 `formatVersion`、四分量 identity、`NormalizationProfile` metadata、生效 late-policy metadata、`eventCodecId`、规范化事件数、编码字节数与 `ReplayDecodeBudget`（类型 `EventCodecId` / 字段 `eventCodecId` 拼写统一）；正常编码可往返；截断、乱序、重复、未知版本、超限和 mismatch 稳定失败；解码不执行 IO/脚本；**预算数值与 codec 字节布局在 S7A-9 / 首次序列化前不得写成约束** |
| S7A-6.3 | 全量 snapshot | 活动实例、Release/tail、resource、离散 sequence、pending signal、fault fixture | header 字段集为 `formatVersion`、四分量 `JudgementIdentity`、`factSemanticRevision`、`stateSchemaRevision`、normalized event count、Fact count、payload byte count 与 typed byte-budget descriptor（`factSemanticRevision` 进 engine identity、`stateSchemaRevision` 不进；FactBinding 与 prepared immutable graph 重新取得、不重复保存）；snapshot bytes 可独立校验；恢复后同一尾部输入 Fact/Score/Combo/Statistics 逐位一致；PresentationEvent 从 FactBinding 重建；**`faulted` 不可新建 snapshot**；不能恢复未提交 delta 或不存在的连续采样相位，也不保存 snapshot interval identity |
| S7A-6.4 | Seek | 多快照间隔、多目标时间 fixture | 任意 seek 点与从起点运行结果一致且无损；`SeekLatencyCommitment` 的类型、承诺语义与会话只能收紧已定义，**具体 `maxSeekLatency` 数值待 S7A-9 测量并接受**、实测与承诺分开记录；目标时间非法时旧状态保持 |
| S7A-6.5 | Replay 与实时等价 | 同一事件的双路径对照 | 实时和 Replay 的 normalize、Fact、Score、Combo、Statistics 和错误完全一致，不仅最终分数相等 |
| S7A-7.1 | Playback 生命周期入口 | PlaybackSession/Prepared Playback integration | 无输入时旧 FrameSnapshot/FrameDigest 不变；submit/advance/query/snapshot/seek/reset 线程和寿命符合既有合同 |
| S7A-7.2 | v5 candidate 消费 | typed Core/Packed candidate fixture | source→prepared→runtime 的 requirements、identity、resource closure 一致；不支持 candidate 稳定拒绝；v4 fallback 保持通过 |
| S7A-7.3 | Headless consumer | 无 GPU 的 executable/CTest fixture | 在无 SDL/OpenGL/GPU 环境中完成 prepare、判定、Replay、Seek；输出与 Player/实时路径 golden 一致 |
| S7A-7.4 | Player 反馈 | 最小 feedback fixture | Player 只消费 JudgementResult/Score/Combo/Miss；不创建私有 Runtime 或第二判定路径；无输入纯播放行为不变 |
| S7A-7.5 | 安装 external consumer | clean staging static/shared consumer | 安装树外编译、正确链接、公共头泄漏/ASCII/版本拒绝通过；candidate 入口不由默认 production target 暴露 |
| S7A-8.1 | candidate 隔离 | default-OFF、explicit-ON、wrong-entry fixture | 默认构建/安装不能加载 v5 candidate；显式 entry 可用；错误 flavor、裸 Packed 和缺 metadata 稳定拒绝；至少一个 preset/CI 真正开启并消费 |
| S7A-8.2 | 离线 assembler 生产入口 | chart-candidate tool 与 closure report | feature/resource closure 由 typed assembler 派生；测试注入后门被检测；输入到 prepared identity 有完整链路 |
| S7A-8.3 | Reference Host 交接 | 同 SHA 六动词命令回归 | `open/play/pause/seek/reload/quit` 状态矩阵、失败保持旧 active、discontinuity 和重载证据完整；不复制历史报告代替当前运行 |
| S7A-8.4 | SDK 0.7.1 门禁 | version-gate change、放行记录、consumer matrix | 默认变更仍失败；显式一次性放行可审计；0.7.0 consumer source-compatible；fresh/clean build、安装元数据和 hosted gate 一致 |
| S7A-9.1 | 本地最终矩阵 | Debug/Release/shared/headless/MinGW logs | 所有批次 fixture、architecture、allowlist、package、docs 和 version gate 在最终 SHA 运行；失败按责任归属，不沿用旧日志 |
| S7A-9.2 | hosted 三平台矩阵 | Linux Quality、Windows MSVC、Windows MinGW evidence | 同一最终行为 SHA 被验证；本地与 hosted 配置差异列明；缺 GPU/设备项单列为未执行或独立证据 |
| S7A-9.3 | 预算和回归报告 | machine-readable measurements、regression report | 内容/稳态/快照/Replay/Packed 计数口径分开；实测与阈值分开；容量变化能追溯到 fixture/commit |
| S7A-9.4 | Stage 8 handoff/owner acceptance | handoff checklist、completion report | 交接物、未纳入 7B+、回滚路径和开放问题齐全；owner 明确接受；否则计划保持 active |

**§3.1 与本批次裁定段的对应（2026-10-03）。** 本表作为分派单位的拆分不变，但以下三行的**核验标准**被
   `### S7A-3` 的"第二半 part 1 实现复核裁定后的范围收窄"**细化**（不新增工作项，只收紧判据）：
   ① **S7A-3.2**（Pattern 编译）的"状态预算稳定拒绝"细化为：包含性门禁返回独立的
   **`ContainmentStatus{contained, gate_incomplete}`**；`gate_incomplete` 是**非诊断状态**，装配路径必须**显式保守拒绝**；
   状态数下界按"**最长可接受轨迹长度 `L` + 1**"（受检加法），`maximumLengthUnbounded`（如 `complement`）**不得**推下界。
   ② **S7A-3.6**（prepare 原子性和预算）新增：两个**需求级声明字段**
   （`RequirementRecord.maxArmElements` / `maxDeadlineElements`）**缺席或装配时不可测得** ⇒ `gate_incomplete` ⇒
   **装配原子拒绝**；只有**已接受容量被可证明超出**才走既有 `arm_bound_exceeded`。
   ③ **S7A-3.5**（canonical identity）新增：已测有效容量经 `writeChartProjection` 进需求身份投影
   （`writeRequirementIdentity` 之后、`writePatternNode` 之前）；四态顺序
   **`缺席 < pending < measured(0) < measured(其他值按数值)`** 由**唯一共享辅助函数**规定；`canonicalCompare`
   **不改**，`compareRequirement` 在 `.pattern` 与 `.measure` 之间为两容量各加一条 `compareValue`。
   **派单提示**：本表仍是分派单位，但**装配消费点接线**（以真实 arm / deadline 容量调用门禁并验证原子失败）
   是进入 S7A-4 的**硬前置**；Spec §3.8.8 已记录接线落地，整批关闭仍须复验，不能据此宣称 S7A-3 完成。

**S7A-3 未闭合合同组合已选定（2026-10-03）。** 按
[G2 + R2 + P1 裁定记录](../../../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-3-contract-selection-g2-r2-p1.md)，
后续实现采用：① G2，完整解析并冻结 prepare-time `preparedGrace`，与资源
`declaredGapGrace` 分离；② R2，在 prepare 生成不可变资源计划，runtime 只执行已准备的确定性
policy 并由引擎分配 owner / lease / contact；③ P1，以新 candidate revision 加新必需 semantic
section 组隔离 Gameplay v2。2026-10-04 已进一步闭合 S2/T4/K4、Capsule revision 2 与完整
物理字段，见本计划 S7A-3 段及 Capsule；不再等待 wire 数字选择。
默认预算数值、运行时资源状态编码和 Snapshot/Replay/FactId/CommitId codec 不由本次闭合。
状态预算 `INCOMPLETE GATE` 仍独立保持；S7A-3/4仅达到受限功能验收完成，容量整体证明未完成。

### 3.2 S7A-5 / S7A-6 联合实施目标、决策和准入

#### 3.2.1 本轮目标与后续批次

2026-10-06 owner已选择5+6为下一实施会话目标：5交付可验证的Fold/结果状态，6消费该状态
完成identity、Replay、全量Snapshot、Seek；5.5的真实恢复验收与6.3–6.5同轮退出。
不先写5整批完成再把真实恢复逐项挂到另一会话。

| 批次 | 相对工作量 / 风险 | 安排与门禁 |
| --- | --- | --- |
| 5.1–5.5 | 高；首次状态表示、算术和seal后失败 | 下一轮前半；先完成字段准入，再PreparedRuleset/grade/Fold/Hook |
| 6.1–6.5 | 很高；状态闭包、wire、late admission和任意恢复 | 下一轮后半；消费稳定Fold状态，最后与5共同验收 |
| 7.1–7.5 | 很高；Playback/Chart/Player/consumer跨模块 | 5/6退出后再实施，不将内部headless fixture记成产品桥已完成 |
| 8.1–8.4 | 高；四项Stage6交接及owner-only版本门禁 | 7之后；D-9 master落库/具名接受、SDK放行仍独立，前置可提前安排 |
| 9.1–9.4 | 很高；五类容量、最终SHA、平台/设备与owner接受 | 最后收口；从5/6累计测量，未接受数值保持INCOMPLETE GATE |

没有本轮性能实测；工作量是结构评估，不是工时或提速承诺。
SDK0.7.0保持，Life/package/correction/连续能力继续既有稳定拒绝。

#### 3.2.2 已选实现方向

各方案比较和取舍保存在 [5/6实施输入](../../../proposals/implementation-input/stage-07/s7a-5-6-design-selection.md)；
本文登记选定方向，字段合同仍归Spec/ABI及首用profile，不用计划代替生产冻结。
P56和U编号仅为本计划局部追踪，不增加CONTRACT_MATRIX、R编号或九类诊断。

| 本地决策 | 已选方向 | 消费前的最后门禁 |
| --- | --- | --- |
| P56-01 / R1 | owning typed Interface/有序manifest、有限内置操作；无任意回调/解释器 | Interface/module/operator/值tag/owner/policy/scope的闭合集和拒绝表 |
| P56-02 / G2 | phase-local有符号tick区间grade表，seal前求值一次 | 可达域覆盖、Miss/error absence、表归属与新旧profile兼容 |
| P56-03 / N1 | i64 Score、u64计数；明确checked/clamp；已证明carrier的operator | 逐Fact中间值、单位元/结合性、第二写与贡献去重人工golden |
| P56-04 / T2 | kernel seal独立；Fold候选/预留fault载体，阶段2一次发布 | 无失败发布点、双cursor/双frontier、成功前缀后失败 |
| P56-05 / S2 | Kernel/Ingress/Fold三个owning全量状态族 | Spec十四项逐成员映射，私有状态与借用闭包，不只存KernelProjection |
| P56-06 / W2 | 有序section、LE定宽record、独立摘要/identity校验 | 完整字段/tag/长度/section/revision表与人工bytes golden |
| P56-07 / E2 | accepted规范输入 + admission/control journal + Fact结构校验 | 共用canonical输入入口、转发复算、非法输入与文件错误分层 |
| P56-08 / K2 | 明确目标cut + 全量快照按(horizon,cut)索引 | 同horizon晚到、未advance/负Tick、录制分支与原子Seek |
| P56-09 / B2 | pending measurement与accepted阈值分开；事实counts独立 | 生产数值拒绝、test-only门禁、无隐藏0/MAX/无限值 |

#### 3.2.3 复核后仍须落定的项目

以下没有被“选R1/W2”等架构方向自动解决。当前状态为 **待合同落定**：
推荐处置已经给出，实施方须在对应消费点之前写入权威字段/行为补充，并以最小反例验证；
普通表示细节可依owner已有实施目标自主完成，不额外请求已授予的授权。
若实质改变已裁定语义，先给反例与影响再修该项；禁止用临时typedef/默认值/伪成功跳过。

| 未定项 | 具体剩余问题与推荐处置 | 最早阻塞 / 关闭证据 |
| --- | --- | --- |
| U01 profile/Interface与静态操作闭合集 | 还缺programPolicy/outcomeScope/arbitration/Loadout有效字段与支持值、module/operator ID/revision、旧/新profile组合表。推荐显式registry，未知即拒绝；arbitration只消费已裁定T4/K4，不让Fold重选赢家 | J0→5.1；全量字段/owner/identity/拒绝表，legacy不被新门禁误拒绝 |
| U02 grade的表归属与缺失分支 | 缺表、声明token缺表、Miss/无error、表外值的分支尚未写字段合同。推荐表属Measure/chart语义、求值算法属engine、grade→score映射属ruleset；新profile必须有显式presence政策与可达域覆盖，不从error补默认grade | J0/J1→5.3；closed端点、各phase/Miss/error absence/new-old profile人工golden |
| U03 算术检查粒度 | 需要明确逐Fact还是最终Tick和才检查、clamp发生点、maxCombo是否取中间峰值。推荐按规范Fact顺序逐步checked/clamp，模块局部累计后exclusive每Tick一次最终写；MAX、+1、-1不能用最终和掩盖中间失败 | J0/J2→5.2/5.3；中间溢出、负值、饱和顺序、同Tick多Fact/第二写golden |
| U04 空Tick和无Fact工作 | kernel当前跳过没有timer/input/signal的时间区间。推荐新profile明示模块trigger集合，空Tick为状态/贡献单位元，无自主每Tick副作用；有due timer或signal则是真正工作Tick，即使无Fact仍处理显式Hook状态 | J0/J2→5.4；大空区间、不同advance分段与稀疏signal等价，不能枚举全部整数Tick |
| U05 Hook目的地/载荷/身份与t+1极值 | 还缺signal的typed目的地、有限载荷、贡献键、持久Hook值与队列事件的区分。推荐仅指向已声明step/Hook目标，pending保存语义值/可见时刻，不存RuleEffect envelope；t+1不可表示在阶段2提交前失败 | J0/J2→5.4；未知target/重复贡献、同Tick不可见、nextTick可见、极值与失败不发布 |
| U06 Replay接入规范化之后的入口 | 当前规范事件存i64量和校准tick，不存原始量/设备时间；不能把canonical量当raw再次量化。推荐live raw normalize后与Replay decoded canonical value汇合到同一canonical validator/admission/submit核心；Replay不提供权威ObservationId或dispatch | J0/J3→6.2/6.5；非单位scale、负值/端点、重复sequence/subject、queue_next_tick对照 |
| U07 Seek后继续live mutation的录制分支 | 当前K2只有target cut/快照选择，没有未来journal如何处理的合同。推荐seek保留owning原archive，首次已接受submit、成功advance或保留提交/发布fault的advance才fork目标前缀；未发布失败不fork；保留precut已接受的pending输入，跨目标advance物化至target | J0/J3/J5→6.2/6.4/6.5；back/forward seek、空推进、提交前失败与保留前缀/Foldfault分开、旧owning证据存活 |
| U08 counts/cursors/frontiers含义 | Snapshot normalized count是已接受至cut（含pending）的事件数；Replay event count不含control；Fact count是sealed前缀大小；Fold cursor只覆盖已成功阶段2。推荐再存各自optional的Fold committed frontier和kernel sealed frontier，零Fact Tick不能只用相同Fact count表示进度；horizon/frontier互不充当默认值 | J0/J4→6.3；pending输入、零Fact信号Tick、seal后Fold失败、各count互不替代 |
| U09 codec/诊断具体登记 | 宽度方向已选，magic/section/tag/presence/UTF-8现行token规则、revision接受集、checksum覆盖、length/count覆盖和错误优先级仍缺完整表。推荐byte计数覆盖完整payload含section framing，不含外层header/digest；先结构可表示性，再identity/引用，再语义重建，细码先登记 | J0/J3→6.2/6.3；独立Writer/Reader人工bytes、截断/篡改/重算摘要后的语义错、跨编译器对照 |
| U10 fault schema与恢复接受面 | fault字段属于payload闭包，但live faulted不可新建快照；需要明确Reader可验证状态与实际restore权限。推荐typed稳定diagnostic，不序列化本机异常blob/线程地址；若读取合法faulted payload，保持faulted，不能转healthy；旧healthy snapshot只恢复显式新session，不就地解除当前faulted | J0/J4→6.3；schema/fault矩阵、两个提交前缀、诊断一致、未提交delta不可伪造 |

**U06入口纠正。** “复用normalize”是共用规范化合同与canonical校验，不要求把已规范化事件逆推成
raw声明后再次量化/校准。原始raw入口仍运行S7A-2；Replay输出owning canonical records后复算
admission/forwarding。不能单独实现第二套去重、排序或late处理，也不能用caller填ObservationId。

**U07分支纠正。** 截断的是target cut之后的journal操作，不是按observationTick过滤；
target cut之前已接受的future pending输入保持，取消它们需显式reset/session replacement。
seek本身不修改原录制证据；新分支的session scope不进judgement identity，旧owning值保持有效。
原archive和分支archive有独立只读寿命；快照索引按所属archive/cut选择，不能跨分支误复用。
若target cut位于原advance内部，新分支先将该控制记录物化为advance(target)，不能保留原来
更大的horizon；若目标超过已录制末端，物化推进至目标的控制记录。query/snapshot不fork，
Replay/Seek内部复放也不伪装成新的live mutation。首次已接受submit或成功advance提交新分支；
advance若已保留kernel seal前缀或发布可查询fault，即使整体Result失败也必须保留新分支证据。
只有没有持久发布的预检/候选构造失败才不fork；不能为保持旧journal回滚已seal Fact。
分支前缀、control outcome/稳定诊断及发布容量在mutation前预构造，随真实提交/fault发布且不再分配；
记录requestedHorizon和实际前缀/frontier，不把失败的请求horizon当成功推进。

**仍独立未接受。** 业务分值/grade阈值由显式Ruleset/Measure声明提供，不在计划填默认数值；
生产内容/稳态/Snapshot/Replay预算与maxSeekLatency数值归9，D-9/master/SDK放行归8，
7B+和外部Ruleset package不在5/6首次消费范围。不用这些后续门禁阻止不消费它们的5/6功能实现，
也不提前宣称容量或owner门禁通过。

#### 3.2.4 实施顺序与退出

| 卡 | 交付 | 内部退出条件 |
| --- | --- | --- |
| J0 | U01–U10字段/owner/表示/初值/reset/identity/诊断/profile/byte表 | 先补相应Spec/ABI/profile；没有需要猜的必需字段，不等于功能已实现 |
| J1 | owning PreparedRuleset、G2 grade与prepare原子性 | 缺失/未知/重复/错配稳定拒绝，新旧profile与identity正反例齐全 |
| J2 | perTick Fold、三类Register、Score/Combo/Statistics、t+1 Hook | kernel未seal与seal后Fold失败分开；完整失败查询与独立Fold oracle |
| J3 | 四分量identity、W2 codec、E2输入/control journal与Replay | 完整bytes与预检、共用canonical入口、转发复算与Fact逐commit对照 |
| J4 | S2完整owning Snapshot/restore | 十四项→真实私有成员；原session销毁后可校验，新候选恢复失败保持旧active |
| J5 | K2 Seek/目标cut与U07原子分支 | 多间隔/任意点/late/pending/负Tick与从起点独立oracle一致 |
| J6 | live/Replay/restore/Seek联合失败和旧fixture回归 | 十个小目标逐字段对账；人工golden先校验oracle，不只比较score/hash |
| J7 | Debug/Release/headless/shared/MinGW、Linux与hosted证据，计数与报告 | 功能行准确IV/IU/M/LB；真实恢复回填5.5，未接受容量保留9，不进入7 |

全部旧T4/K4、L1/L2、S1/S2、author双路/affine、Capsule2/3与Playback包消费者回归保留。
按labels枚举非零case后运行，Windows不伪跑Linux-only sanitizer/coverage。
CPU/内存/bytes/prepare/Fold/restore/Seek测量是9的输入，不直接接受阈值；
缺设备/GPU/音频验证单列，Stage7A不关闭。
按owner要求推送后不等CI；hosted结果尚未返回时标待回填，绑定实际SHA，不提前记通过。
桌面接手只是本节的导出执行快照；后续决策始终更新本计划/对应权威文档。

## 4. Stage 7A 关闭标准

Stage 7A 只有同时满足以下条件才能标记 completed：

1. 相同 Chart、InputEvent、InputMapping、Timing、Ruleset、Loadout 和 JudgementConfig 产生逐位
   一致的 Fact、JudgementResult、Score、Combo 和 Statistics。
2. 渲染帧率、World/Entity 遍历顺序、输入数组顺序、容器顺序、后端替换和不同快照间隔不改变
   结果；Seek/Reload/Pause/Audio discontinuity 后从同一目标时间重建得到相同结果。
3. Tap/Hold/Release/tail 的半开区间、严格边界、重复输入、冲突输入、Miss、stray、observe、claim、
   固定 fanout=1、终止后 slot reuse、单一 exclusive resource 和 prepared grace 都有正反 golden；capacity>1、
   Chord、handoff、连续轨迹和全局最优 solver 稳定拒绝。
4. 实时输入与 Replay 走同一 Judgement 路径，Replay identity、版本、事件/字节预算和篡改/截断
   失败可诊断；快照无损，恢复后尾部输入结果逐位一致。
5. 判定系统不暴露 World、EnTT、SDL、OpenGL、JSON DOM、宿主 SDK 或私有 Player 类型；安装公共
   头纯 ASCII，static/shared、headless 和 external consumer 生命周期测试通过。
6. Chart v5 Core、`packed-chart` 和 `gameplay-graph` candidate 能消费 S7A requirements，并与
   typed source 得到相同 Canonical Graph、prepared identity 和判定结果；early/exact/late/Miss
   bridge 与 v4 回退、无输入 Playback 行为保持兼容。
7. candidate 与 production 隔离、离线 typed assembler、Reference Host 交接回归和 SDK API `0.7.1`
   版本门禁四项已逐项实现并有证据，或已由 ADR/Spec 明示接受例外。
8. 真实内容预算、跨平台 hosted、架构/package/version/docs 检查和 owner acceptance 完整；研究
   spike 只作为背景，不被误写成产品实现或公共限额。

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

## 8. 统一验收矩阵和证据要求

每个批次至少维护以下列，不以“测试通过”一句话替代：

| 维度 | 必须记录 |
| --- | --- |
| 合同 | Spec/ADR/ABI 版本、字段和 capability/revision |
| 正例 | 最小、边界、组合、真实内容和压力 fixture |
| 负例 | 未知值、越界、溢出、冲突、预算、身份、截断和非终止 |
| 确定性 | 输入/实例/容器顺序、跨编译器/平台、seek/replay 对照 |
| 生命周期 | prepare、submit、advance、pause、seek、reload、reset、销毁和失败事务 |
| 身份 | engine/ruleset/chart/session、content/prepared/replay identity 及变更原因 |
| 预算 | prepare 峰值、稳态 tick、activity、Fact、Replay、快照、Seek 和 decoded bytes |
| 边界 | headless、static/shared、external consumer、candidate/production、GPU/设备限制 |
| 证据 | 命令、SHA、配置、环境、原始日志/机器可读结果、报告路径和未执行项 |

必须区分：本地验证与 hosted 验证、离线/UI 验证与真实设备/GPU 验证、配置阈值与实测结果、
研究模型与产品实现。任意不可用工具、连接失败或缺少设备只降低证据覆盖，不能推断通过。

## 9. 回滚、阻塞和停止条件

1. 合同变更：停止受影响实现，保留最小复现，回滚到最后接受的 ADR/Spec revision，重新估算
   identity、Replay、Packed 和 Stage 8 影响后再继续。
2. 确定性失败：任何跨编译器/平台差异、未声明浮点、整数溢出或容器顺序依赖都阻塞批次；不以
   固定编译器、排序偶然性或缩小输入规避。
3. 预算失败：区分内容预算、运行稳态、快照/Seek、Replay/解码和 Packed wire；只允许调整已
   接受的 profile，不得事后缩小 fixture 或把隐藏事件排除计数。
4. 安全失败：不可信 Chart/Ruleset/Replay 导致未界定循环、IO、宿主回调、动态生成或过量分配时，
   立即稳定拒绝并保留旧 active session。
5. 集成失败：candidate、v4 fallback、旧 Replay 或无输入 Playback 受影响时，回滚新增入口，
   不删除旧路径，不把 Player smoke 分支当成集成完成。
6. owner 未接受 ADR/Spec/例外时，状态保持 planned/active/blocked；不得用文档措辞把候选能力
   写成 implemented 或 production。

## 10. 交接物和后续阶段关系

### 交给 Stage 8

```text
Stage 7A typed Input / Requirement / Judgement / Score / Replay contract
Tap/Hold/Release and prepared-grace semantics
InputMapping and four-part judgement identity
Headless golden, replay fixtures, snapshot/seek guarantees
v4 fallback and v5 candidate consumption map
candidate/production isolation and offline assembler evidence
capability/rejection table for all 7B+ items
selected 7B+ release matrix, if any
measured gameplay budgets and unresolved W-class gaps (§5.3)
```

Stage 8 不得重新发明输入、判定、Replay 或 Score 模型；只把已选 capability 编入 Chart v5/CXT
v2/Packed/CXC 正式发行，并保留未选能力的拒绝路径。

### 交给 Stage 9/10/12/14

- Stage 9 只消费稳定的 Judgement/Presentation 观察面；模型、环境和几何表现不得改变判定事实。
- Stage 10 只消费已冻结的 typed authoring/prepare/capability 合同；Studio 不能成为新的判定实现。
- Stage 12 消费真实内容的 gameplay、Replay、Seek 和设备 profile 测量，不修改单一设备的判定语义。
- Stage 14 再决定稳定 C ABI；Stage 7 的 C++ typed preview 不等于稳定 ABI。

## 11. 明确不包含

Stage 7A 不包含完整 Slide、复杂 Flick、多指、旋转、3D 方向斩、连续音高、Studio 编辑器、
运行时脚本、通用状态机、宿主回调、稳定 C ABI 或 Chart v5 默认 Writer。

Stage 7B+ 仍不包含任意 Script VM、逐帧脚本 callback、宿主字节码、无限 Pattern、运行时随机、
IO、动态对象/Requirement 生成或未版本化的插件执行权。超出当前 2D/离散和声明式模型的能力必须
另立 ADR、威胁模型、预算、ABI 和阶段计划。

压测中的 H 类支持政策在 Stage 7 继承并锁定：H01-H05、H07 是能力边界，不会再开发，不受支持；
H06（三维物理斩击）和 H08（任意自由体感传感器融合）是未来开发，现不支持。它们不能通过
7B+ 的 capability registry 预留隐式入口。

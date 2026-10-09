# Stage 7 Implementation Plan: Gameplay Foundation and Judgement Evolution

状态：active；Stage 7A 分批实施，Stage 7B+ 为可在 Stage 8 前后持续交付的能力线；本计划不构成阶段关闭或发行记录

更新日期：2026-10-07

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

2026-10-06 逐项源码与门禁核对及实施建议见
[四项交接复核](../../../stage_reports/stages/stage-07/verification/2026-10-06-stage6-handover-audit.md)。
该报告更新旧台账事实，记录 S7A-8.1–8.4 的建议顺序与退出证据；不表示已实施 S7A-7/8/9、
接受生产预算或批准新的版本/门禁合同。

**2026-10-07 owner 实施授权补充：** 四项在现有 `stage-7` 实施并提交 PR #32，审批由项目
所有者本人完成。S7A-8.1 使用 ON/OFF binary flavor、显式 consumer 许可和安装前缀 stamp；
S7A-8.2 沿用 `candidate.static-tap-lanes4-v1` 的 typed Foundation assembler，实际 CLI 输出
Packed、project/CXC entry metadata 和原子嵌入的 closure report，发布前运行真实 Playback prepare。
此 versioned Foundation profile 不取代 Gameplay v2 authored/derived closure，不接受旧 Gameplay
语义进入当前 Judgement；Judgement/Playback bridge 仍属 S7A-7，本批不将其声称为已完成。
S7A-8.3 对安装后的宿主运行既有七动词及显式 candidate entry。S7A-8.4 准备兼容的 SDK
`0.7.1`，通过可信 base owner registry 与精确 API 审批记录验证；同作者 PR 的本人审批路径
按 [VERSIONING](../../../guides/VERSIONING.md#s7a-84-owner-approval-record) 与 ADR 0042 补充实施。
代码交付、hosted 证据、保护启用和 owner 审批分别记录；缺可信 bootstrap 不自动放行或关闭
S7A-8.4，不扩张至 S7A-7 产品集成、S7A-9 预算/阶段关闭。

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

S7A 的详细目标、执行卡、实施决策和关闭标准拆至[Stage 7A 分册](plan-a.md)。下列标题保留为导航锚点。




详见 [plan-a.md：3. Stage 7A 交付批次](plan-a.md).

### S7A-0：实施准入、基线和合同表征

详见 [plan-a.md：S7A-0：实施准入、基线和合同表征](plan-a.md).

### S7A-1：Typed Kernel 边界和模块骨架

详见 [plan-a.md：S7A-1：Typed Kernel 边界和模块骨架](plan-a.md).

### S7A-2：输入规范化、映射和时钟边界

详见 [plan-a.md：S7A-2：输入规范化、映射和时钟边界](plan-a.md).

### S7A-3：Requirement prepare、编译和离线 typed assembler

详见 [plan-a.md：S7A-3：Requirement prepare、编译和离线 typed assembler](plan-a.md).

### S7A-4：Judgement Kernel 生命周期、Tap/Hold/Release 和仲裁

详见 [plan-a.md：S7A-4：Judgement Kernel 生命周期、Tap/Hold/Release 和仲裁](plan-a.md).

### 后续实施队列（2026-10-06）

详见 [plan-a.md：后续实施队列（2026-10-06）](plan-a.md).

### S7A-5：Ruleset Fold、Score、Combo 和 Statistics

详见 [plan-a.md：S7A-5：Ruleset Fold、Score、Combo 和 Statistics](plan-a.md).

### S7A-6：Identity、Replay、Snapshot 和 Seek

详见 [plan-a.md：S7A-6：Identity、Replay、Snapshot 和 Seek](plan-a.md).

### S7A-7：Playback、Chart candidate、Headless、Player 和 external consumer 集成

详见 [plan-a.md：S7A-7：Playback、Chart candidate、Headless、Player 和 external consumer 集成](plan-a.md).

### S7A-8：Stage 6 交接收口和版本门禁

详见 [plan-a.md：S7A-8：Stage 6 交接收口和版本门禁](plan-a.md).

### S7A-9：最终硬化、跨平台证据、Stage 8 handoff 和关闭

详见 [plan-a.md：S7A-9：最终硬化、跨平台证据、Stage 8 handoff 和关闭](plan-a.md).

### 3.1 Stage 7A 小目标核验台账

详见 [plan-a.md：3.1 Stage 7A 小目标核验台账](plan-a.md).

### 3.2 S7A-5 / S7A-6 联合实施目标、决策和准入

详见 [plan-a.md：3.2 S7A-5 / S7A-6 联合实施目标、决策和准入](plan-a.md).

#### 3.2.1 本轮目标与后续批次

详见 [plan-a.md：3.2.1 本轮目标与后续批次](plan-a.md).

#### 3.2.2 已选实现方向

详见 [plan-a.md：3.2.2 已选实现方向](plan-a.md).

#### 3.2.3 复核后仍须落定的项目

详见 [plan-a.md：3.2.3 复核后仍须落定的项目](plan-a.md).

#### 3.2.4 实施顺序与退出

详见 [plan-a.md：3.2.4 实施顺序与退出](plan-a.md).

### 3.3 S7A-5/6 本轮实施记录（2026-10-06）

详见 [plan-a.md：3.3 S7A-5/6 本轮实施记录（2026-10-06）](plan-a.md).

### 3.4 S7A-7/8 下一轮决策、首用合同与接手（2026-10-07）

详见 [plan-a.md：3.4 S7A-7/8 下一轮决策、首用合同与接手（2026-10-07）](plan-a.md).

2026-10-09 owner要求把架构重构与正式命名迁移设为独立阶段，详见
[Stage RPA](../../future/realtime-playback-foundation/plan.md)。S7A-7/8保留原有集成收口，
不再计入RT78/DN78新增工作。实体设备/实时同步实施和最终验收整体归RPA，旧失败随交接保留；S7A-9不新增RPA前置。
Stage7B+高级能力线保持，Stage8消费7A、RPA与已选入7B+成果；
范围迁移不构成Stage7A关闭或Stage8发行许可。原plan-a §3.4.18/19保留兼容索引。

#### 3.4.1 已定边界与仍然待办的区分

详见 [plan-a.md：3.4.1 已定边界与仍然待办的区分](plan-a.md).

#### 3.4.2 九组主干选择：每组五方案及选优

详见 [plan-a.md：3.4.2 九组主干选择：每组五方案及选优](plan-a.md).

#### 3.4.3 复核补出的六组选择：每组五方案及选优

详见 [plan-a.md：3.4.3 复核补出的六组选择：每组五方案及选优](plan-a.md).

#### 3.4.4 十二行首用合同核对：字段、所有权、失败与证据

详见 [plan-a.md：3.4.4 十二行首用合同核对：字段、所有权、失败与证据](plan-a.md).

#### 3.4.5 下一轮执行顺序与退出

详见 [plan-a.md：3.4.5 下一轮执行顺序与退出](plan-a.md).

#### 3.4.6 重审补充：消费者组合门禁（2026-10-07）

详见 [plan-a §3.4.6](plan-a.md#346-重审补充消费者组合门禁2026-10-07)。

#### 3.4.7 重审缺口的五方向方案与选优（2026-10-07）

详见 [plan-a §3.4.7](plan-a.md#347-重审缺口的五方向方案与选优2026-10-07)。

## 4. Stage 7A 关闭标准

详见 [plan-a.md：4. Stage 7A 关闭标准](plan-a.md).

## 5. Stage 7B+ 能力线的统一规则

S7B/S7C 的完整准入包、实现退出门禁和 W 类缺口规则见[Stage 7B+ 分册](plan-b.md)。下列标题保留为导航锚点。




详见 [plan-b.md：5. Stage 7B+ 能力线的统一规则](plan-b.md).

### 5.1 新能力准入包

详见 [plan-b.md：5.1 新能力准入包](plan-b.md).

### 5.2 统一实现和退出门禁

详见 [plan-b.md：5.2 统一实现和退出门禁](plan-b.md).

### 5.3 W 类缺口记录（W-class gap record）

详见 [plan-b.md：5.3 W 类缺口记录（W-class gap record）](plan-b.md).

## 6. Stage 7B+ 分批规划

详见 [plan-b.md：6. Stage 7B+ 分批规划](plan-b.md).

### S7B-0：Capability registry、版本和扩展测试骨架

详见 [plan-b.md：S7B-0：Capability registry、版本和扩展测试骨架](plan-b.md).

### S7B-1：连续输入、区域采样、Slide 和 handoff continuity（新增 capability）

详见 [plan-b.md：S7B-1：连续输入、区域采样、Slide 和 handoff continuity（新增 capability）](plan-b.md).

### S7B-2：Flick、方向、速度和位移约束

详见 [plan-b.md：S7B-2：Flick、方向、速度和位移约束](plan-b.md).

### S7B-3：多接触点、和弦、资源占用和跨 requirement 聚合（新增 capability）

详见 [plan-b.md：S7B-3：多接触点、和弦、资源占用和跨 requirement 聚合（新增 capability）](plan-b.md).

### S7B-4：高级尾判、Lift、计数和连续动作族（新增 capability）

详见 [plan-b.md：S7B-4：高级尾判、Lift、计数和连续动作族（新增 capability）](plan-b.md).

### S7C-1：校准、设备延迟和 Judgement Policy

详见 [plan-b.md：S7C-1：校准、设备延迟和 Judgement Policy](plan-b.md).

### S7C-2：Ruleset package、模块扩展和 Replay 演进

详见 [plan-b.md：S7C-2：Ruleset package、模块扩展和 Replay 演进](plan-b.md).

### 6.1 Stage 7B+ 小目标核验台账

详见 [plan-b.md：6.1 Stage 7B+ 小目标核验台账](plan-b.md).

## 7. 7B+ 与 Stage 8 的选入规则

详见 [plan-b.md：7. 7B+ 与 Stage 8 的选入规则](plan-b.md).


## 验收标准

Stage 7 整体验收组合 Stage 7A 硬前置与每个 7B+ capability 的独立退出标准。S7A 详细关闭标准见[plan-a §4](plan-a.md#4-stage-7a-关闭标准)；7B+准入与 Stage 8 选入标准见[plan-b §7](plan-b.md#7-7b-与-stage-8-的选入规则)。研究 spike、单平台结果、只测正例的 golden 或历史报告均不能替代当前实现证据。

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

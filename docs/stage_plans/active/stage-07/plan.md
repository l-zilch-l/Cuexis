# Stage 7 Implementation Plan: Gameplay Foundation and Realtime Playback

状态：completed；按拆分后的既有范围受限收束，未认证范围与 testOnly 保留。

更新日期：2026-11-14

owner 已接受 R4／RPA／PD-5 受限退出及原 7A 收尾交接，随后要求将尚未实施的 Stage 7B+ 完整独立。
后续能力、局部重构及性能稳定归[Stage 7.5：Gameplay Evolution](../stage-07-5/plan.md)，
Stage 8–14 编号不变。Stage 7 不再等待原 7B+ 的产品实现，也不因迁出而把那些能力记为已完成。

本阶段保留 Input／Judgement／Score／Replay 内核、独立诊断前置、实时宿主、PD 和 RPA 整改的历史范围及受限结果。
受限关闭范围与未认证责任见[已接受交接](../../../stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-14-limited-exit-and-defect-handoff.md)。
该记录在拆分前写明整体 Stage 7 未关闭，本次范围迁移补充其后续状态；原记录和封存包不回写。
当前状态归[CURRENT_STATUS](../../../CURRENT_STATUS.md)，拆分前全文见
[归档快照](../../../archive/stage-07-planning/2026-11-14-stage-07-before-gameplay-evolution-split.md)。
为避免已封存引用大规模迁移，本阶段保留既有 `active/stage-07` 路径，不再在此启动新实施批次。

## 阶段目标

形成可嵌入 SDK 的最小玩法闭环、四层语义边界、单一 typed prepare／Judgement／Ruleset／Snapshot／Replay 路径，
以及 realtime host 和表现观察边界。Chart v5／Canonical Graph／Packed 仍是显式 candidate；
Chart v4／CXT v1／CXC v1 兼容路径保持，正式发行收敛归 Stage 8。
当前受限退出不等于全部性能、内存、GPU、设备仪器或生产参数已通过。

## 分册与子阶段

| Stage 7 范围 | 入口 | 交接 |
| --- | --- | --- |
| Stage 7A | [内核与集成](01-a/plan.md) | 原 S7A-9 随 RPA 受限交接接受 |
| 独立诊断前置 | [N0–N3](02-diagnostics/plan.md) | 正式码及兼容合同，N3 已退出 |
| Legacy RPA | [原范围兼容入口](03-rpa-legacy/plan.md) | realtime host／设备及原 S7A-9 历史范围 |
| Stage 7-PD | [PD](04-pd/plan.md) | 带限制退出，PD-5“未通过但定案”保留 |
| New RPA | [R0–R4](05-rpa-new/plan.md) | 已知缺陷及未认证范围交给独立 Stage 7.5 |

原 Stage 7B+ 已从子阶段集合移出；S7B/S7C 是 Stage 7.5 的历史批次 ID。
本计划只保留既有边界和交接，字段／语义的权威仍是 ADR／Spec／ABI，带日期报告保存原始结果。
归档来源：[Stage 6 交接](../../completed/stage-06/plan.md)、
[旧 PROJECT_GUIDE](../../../archive/PROJECT_GUIDE_LEGACY_2026-08-10.md)和
[SDK transition plan](../../../archive/CUEXIS_SDK_TRANSITION_PLAN_2026-08-10.md)。

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
[S7A-0 接受记录 §4.2](../../../stage_reports/stages/stage-07/01-a/s7a-0/2026-10-02-s7a-0-acceptance.md)。

如果实现阶段发现 V2 合同、历史 Gameplay I 文档、研究证据或真实内容预算之间矛盾，先建立可复现失败报告并停止
受影响批次；不得在代码中隐式选择替代语义。修改四层边界、identity 分区、grace 语义、
Pattern 原语、仲裁顺序或拒绝策略，必须先更新 ADR/Spec，再重新验证全部受影响批次。

### 1.2 Stage 6 交接前置

下列SDK 0.7.1为Stage 6交接和S7A-8.4历史增量基线；当前分支 SDK 为 0.9.1，
诊断独立前置及当前交付状态见[CURRENT_STATUS](../../../CURRENT_STATUS.md)，不沿用旧审批。

Stage 6 关闭后由 Stage 7A 承接四项未完成内容。它们是关闭前置，不是“以后再补”的 backlog：

| 交接项 | Stage 7A 必须交付的结果 | 关闭时最低证据 |
| --- | --- | --- |
| 显式 candidate 与实验隔离 | `Cuexis_ALLOW_EXPERIMENTAL`、candidate flavor/安装元数据、Player `--candidate-entry` 与项目/CXC 入口、至少一个真实启用 candidate 的 preset 或 CI | 默认 OFF 与显式 ON 的构建/安装/运行矩阵；production consumer 证明不会误启用 candidate；错误入口稳定拒绝 |
| 离线 typed assembler 与 feature 派生 | 从 typed Chart/CXT/source 输入派生 feature、requirements、resource closure 和 capability 的离线入口；禁止测试注入 feature 作为生产后门 | 正例、缺字段、非法引用、预算、确定性 identity 和原子失败用例；Playback 不展开 CXT |
| 具名宿主六动词命令循环 | 对现有 Reference Host 的 `open/play/pause/seek/reload/quit` 交接回归，不重新实现 R9（**措辞订正**：ADR 0042 的六动词即此序列；实现另有 `tick` 运行时步骤，不在六动词之内。缺陷 `D-2` / `D-7`，第 7 轮 `S7A7-R09`，**不改 ADR 0042、不改实现**） | 同一 SHA 的 hosted 回归、命令状态矩阵、错误/旧状态保持证据 |
| SDK API `0.7.1` | 先修订 version-gate 的条件化放行机制，再按版本规范落实 additive API 版本；若 owner 接受例外，必须有 ADR/版本规范记录 | version gate 的默认拒绝、一次显式放行、0.7.0 consumer 兼容、fresh/clean build、安装元数据和 hosted 证据 |

四项中任一项既未实现也没有被 ADR 或所属 Spec 明示接受的例外，Stage 7A 不得关闭。Stage 6
的关闭事实不被改写；只把剩余证据归入当前阶段。


详细实施授权、旧版本与历次工具门保留在[拆分前快照](../../../archive/stage-07-planning/2026-11-14-stage-07-before-gameplay-evolution-split.md#12-stage-6-交接前置)
及各批次报告；不据历史准备文字重新启动实施。

## 2. 总体依赖和发布路线

```text
Stage 7A 内核／集成
  -> 独立诊断前置 N0–N3
  -> RPA／PD／RPA 整改及原 S7A-9 统一收尾
  -> Stage 7 受限关闭交接
  -> Stage 7.5：Gameplay Evolution（独立阶段）
  -> Stage 8：Chart v5 正式发行收敛
```

本阶段只交接已实际接受的 typed contract、实现及限制。
原高级能力依赖表、capability 准入、W 类缺口、S7B/S7C 验收和局部重构属于
[Stage 7.5 独立计划](../stage-07-5/plan.md)，不再作为本阶段关闭前置。

## 3. Stage 7A 交付批次

S7A 的详细目标、执行卡、实施决策和关闭标准拆至[Stage 7A 分册](01-a/plan.md)。下列标题保留为导航锚点。




详见 [plan-a.md：3. Stage 7A 交付批次](01-a/plan.md).

### S7A-0：实施准入、基线和合同表征

详见 [plan-a.md：S7A-0：实施准入、基线和合同表征](01-a/plan.md).

### S7A-1：Typed Kernel 边界和模块骨架

详见 [plan-a.md：S7A-1：Typed Kernel 边界和模块骨架](01-a/plan.md).

### S7A-2：输入规范化、映射和时钟边界

详见 [plan-a.md：S7A-2：输入规范化、映射和时钟边界](01-a/plan.md).

### S7A-3：Requirement prepare、编译和离线 typed assembler

详见 [plan-a.md：S7A-3：Requirement prepare、编译和离线 typed assembler](01-a/plan.md).

### S7A-4：Judgement Kernel 生命周期、Tap/Hold/Release 和仲裁

详见 [plan-a.md：S7A-4：Judgement Kernel 生命周期、Tap/Hold/Release 和仲裁](01-a/plan.md).

### 后续实施队列（2026-10-06）

详见 [plan-a.md：后续实施队列（2026-10-06）](01-a/plan.md).

### S7A-5：Ruleset Fold、Score、Combo 和 Statistics

详见 [plan-a.md：S7A-5：Ruleset Fold、Score、Combo 和 Statistics](01-a/plan.md).

### S7A-6：Identity、Replay、Snapshot 和 Seek

详见 [plan-a.md：S7A-6：Identity、Replay、Snapshot 和 Seek](01-a/plan.md).

### S7A-7：Playback、Chart candidate、Headless、Player 和 external consumer 集成

详见 [plan-a.md：S7A-7：Playback、Chart candidate、Headless、Player 和 external consumer 集成](01-a/plan.md).

### S7A-8：Stage 6 交接收口和版本门禁

详见 [plan-a.md：S7A-8：Stage 6 交接收口和版本门禁](01-a/plan.md).

### S7A-9：最终硬化、跨平台证据、Stage 8 handoff 和关闭

旧编号保留为导航；工作及验收整体移入[RPA-F1–F4综合收尾](03-rpa-legacy/plan.md#综合收尾工作线原s7a-9)，plan-a不再单独排期。

### 3.1 Stage 7A 小目标核验台账

详见 [plan-a.md：3.1 Stage 7A 小目标核验台账](01-a/plan.md).

### 3.2 S7A-5 / S7A-6 联合实施目标、决策和准入

详见 [plan-a.md：3.2 S7A-5 / S7A-6 联合实施目标、决策和准入](01-a/plan.md).

#### 3.2.1 本轮目标与后续批次

详见 [plan-a.md：3.2.1 本轮目标与后续批次](01-a/plan.md).

#### 3.2.2 已选实现方向

详见 [plan-a.md：3.2.2 已选实现方向](01-a/plan.md).

#### 3.2.3 复核后仍须落定的项目

详见 [plan-a.md：3.2.3 复核后仍须落定的项目](01-a/plan.md).

#### 3.2.4 实施顺序与退出

详见 [plan-a.md：3.2.4 实施顺序与退出](01-a/plan.md).

### 3.3 S7A-5/6 本轮实施记录（2026-10-06）

详见 [plan-a.md：3.3 S7A-5/6 本轮实施记录（2026-10-06）](01-a/plan.md).

### 3.4 S7A-7/8 下一轮决策、首用合同与接手（2026-10-07）

详见 [plan-a.md：3.4 S7A-7/8 下一轮决策、首用合同与接手（2026-10-07）](01-a/plan.md).

2026-10-09 owner最新指令将架构重构与正式命名迁移作为Stage 7内独立验收的子阶段，详见
[Stage 7-RPA](03-rpa-legacy/plan.md)。S7A-7/8保留原有集成收口，
不再计入RT78/DN78新增工作。实体设备/实时同步实施和最终验收整体归RPA，旧失败随交接保留；
2026-10-10后续将原S7A-9整体移入RPA-F1–F4，不再保留独立7A收尾队列。
上述为当时实施范围迁移记录。现行后续顺序为 Stage 7 受限关闭交接 → 独立 Stage 7.5 → Stage 8；
原 plan-a §3.4.18/19 保留兼容索引，历史迁移不单独构成发行许可。

#### 3.4.1 已定边界与仍然待办的区分

详见 [plan-a.md：3.4.1 已定边界与仍然待办的区分](01-a/plan.md).

#### 3.4.2 九组主干选择：每组五方案及选优

详见 [plan-a.md：3.4.2 九组主干选择：每组五方案及选优](01-a/plan.md).

#### 3.4.3 复核补出的六组选择：每组五方案及选优

详见 [plan-a.md：3.4.3 复核补出的六组选择：每组五方案及选优](01-a/plan.md).

#### 3.4.4 十二行首用合同核对：字段、所有权、失败与证据

详见 [plan-a.md：3.4.4 十二行首用合同核对：字段、所有权、失败与证据](01-a/plan.md).

#### 3.4.5 下一轮执行顺序与退出

详见 [plan-a.md：3.4.5 下一轮执行顺序与退出](01-a/plan.md).

#### 3.4.6 重审补充：消费者组合门禁（2026-10-07）

详见 [plan-a §3.4.6](01-a/plan.md#346-重审补充消费者组合门禁2026-10-07)。

#### 3.4.7 重审缺口的五方向方案与选优（2026-10-07）

详见 [plan-a §3.4.7](01-a/plan.md#347-重审缺口的五方向方案与选优2026-10-07)。

## 4. Stage 7A 关闭标准

详见 [plan-a.md：4. Stage 7A 关闭标准](01-a/plan.md).

## 5. Stage 7B+ 能力线的统一规则

本节已迁出至[Stage 7.5 规则](../stage-07-5/plan.md#5-stage-75-能力线的统一规则)。
旧标题仅供历史定位；新工作不在此维护。

## 6. Stage 7B+ 分批规划

批次及原 S7B/S7C ID 见[Stage 7.5 批次](../stage-07-5/plan.md#6-stage-75-分批规划)。

## 7. 7B+ 与 Stage 8 的选入规则

独立阶段的退出与发行关系见[Stage 7.5 交接](../stage-07-5/plan.md#9-ge-f独立验收与-stage-8-交接)。

## 验收标准

本阶段按拆分后的既有范围收束；R4／RPA、PD-5 及原 7A 收尾包已有 owner 受限接受，
N3 退出沿既有记录。全部未认证项、原失败、额度和 testOnly 不变。
Stage 7.5 的未实施能力不回写为 Stage 7 已交付，后续生产接受仍须对应证据与 owner 裁定。

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

交给[Stage 7.5](../stage-07-5/plan.md)的内容包括：7A typed Input／Requirement／Judgement／Score／Replay、
Tap/Hold/Release／prepared-grace、InputMapping 与 identity、已有 headless／Snapshot／Seek／消费者证据、
v4 fallback／v5 candidate 隔离，以及受限退出表中的失败、复制／保活缺陷、未认证范围、责任和恢复条件。
Stage 7.5 自行维护高级 capability、拒绝表、预算、GE-R 及 GE-F，不再回挂 Stage 7 整改。

Stage 8 消费 Stage 7 的既有语义及 Stage 7.5 的实际接受结果；不得重新发明判定或隐式启用未选能力。
Stage 9 只消费稳定 Judgement／Presentation 观察面；Stage 10 不另造判定实现；Stage 12 消费真实 profile，
不改判定语义；Stage 14 再决定稳定 C ABI，当前 C++ preview 不等于稳定 ABI。

## 11. 明确不包含

本阶段不实现完整 Slide／复杂 Flick／多触点／高级校准／Ruleset 包，也不承担后续重构与新性能认证；
这些已归独立 Stage 7.5。Studio、Chart v5 默认 Writer、稳定 C ABI、任意脚本、逐帧 callback、
宿主字节码、动态 Requirement、IO／随机和未版本化插件执行权不因阶段拆分获得授权。
H 类支持政策与既有稳定拒绝保持，不能借新阶段 registry 预留隐式执行入口。

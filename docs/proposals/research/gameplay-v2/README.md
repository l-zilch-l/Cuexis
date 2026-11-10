# Gameplay V2 研究重构

状态：candidate；研究稿，未接受，未实施

更新日期：2026-10-02

本文档组是对现有 Gameplay I 工作稿的结构性重写提案。它不是 ADR、生产格式、ABI 或实现声明。
现有研究稿、ADR 0043、Gameplay Judgement Spec 和候选 ABI 仍保留为历史基线与差异对照；它们
不再是 Stage 7A 的实施授权。Stage 7A 必须先完成一份以本文档组为输入的 V2 acceptance package，
并修订或明确 supersede ADR、Spec、typed ABI、Schema、Replay 和诊断合同；在该 package 被 owner
接受前，任何实现都不得同时解释旧 Gameplay I 与 V2 字段。

## 为什么需要 V2

现有设计已经确认了 prepare-time lowering、整数判定、严格拒绝、Replay/Seek 确定性和四分量 identity 等重要原则，但语义仍分散在 Pattern、Measure、Ruleset Fold 和资源仲裁中。尤其是：

- Requirement、Spec 和 ABI 对 `Effect`、`category`、`phase`、`eventSequence` 的字段集合不一致；
- `Pattern` 只描述单条 requirement，而和弦、共享接触点、交替、原子成功/失败等跨 requirement 关系没有正式载体；
- 当前 Fact 排序 `(requirementId, phase, outcome)` 不是因果全序；同键 Fact 的顺序仍可能依赖实现；
- 输入时间、迟到事件、Seek、接触生命周期和采样相位没有共同的规范时间线；
- Ruleset 的模块冲突、事务边界和 Effect 提交时点仍主要依赖约定；
- identity 需要覆盖“编译后闭包”，而不是只覆盖若干源字段。

V2 的目标不是增加更多专用 Pattern，而是让这些关系成为可验证、可扩展、可版本化的基础语义。

## V2 总体模型

```text
Chart v5 source (Chart + CXT v2 imports)
  -> Normalized Observation Timeline
  -> Local Requirement Programs
  -> Coordination Graph / Transactions
  -> Canonical Gameplay Graph / Fact Ledger
  -> Ruleset State Transactions
  -> Immutable Effect Stream
  -> Presentation / Replay / Snapshot
```

核心分层：

1. **输入时间线**：把设备、Replay 或 Autoplay 变成同一套规范 observation。
2. **局部 Requirement Program**：只负责一个 requirement 内部的匹配、计量和终态。
3. **Coordination Graph**：负责多个 requirement 之间的资源、绑定、顺序、基数和原子事务。
4. **Fact Ledger**：把判定证据变成不可变、可排序、可重放的事实记录。
5. **Ruleset State Transaction**：按确定顺序归约 Fact，提交计分、连击、生命和技能状态。
6. **Effect Stream**：只读消费的表现/音频/诊断效果；不能回写判定。

Chart v5 是 Stage 7 的谱面语义基准，外层保持 `version = 5`，Gameplay 固定使用
`gameplay.version = 2`。CXT v2 只是作者层模板/有限生成输入；Canonical Gameplay Graph 既是
Chart v5 与 Packed 的共同语义来源，也可以作为 CXC v1 独立 `gameplay-graph` Playback entry，
与 `packed-chart` entry 共用 prepare、Judgement、Ruleset、Snapshot、Replay 和 Presentation path。
Stage 7A 的最小合同是 Tap/Hold、同一 Requirement 的 Release/tail、early/exact/late/Miss bridge
和单一 `capacity=1` exclusive resource。

## 设计原则

- 所有会影响结果的语义都必须在 prepare 后有规范表示。
- 局部匹配和跨 requirement 关系正交；不要把全局关系伪装成 Pattern 原语。
- 每个可观察结果都有因果来源和规范排序键。
- 不支持的能力稳定拒绝，不静默降级。
- 扩展通过带版本的 capability 和类型化节点加入；不修改既有字段含义。
- Replay 使用与实时输入相同的规范提交路径。
- Snapshot 保存语义状态，而不是缓存实现细节。

## 文档

- [Semantic Kernel](SEMANTIC_KERNEL.md)：时间、观察、Requirement、协调、Fact、Ruleset 事务、Effect 和 identity。
- [Design Decisions](DESIGN_DECISIONS.md)：对现有设计的保留、删除、替换、迁移和开放问题。
- [Format Boundary](FORMAT_BOUNDARY.md)：作者源、Canonical Gameplay Graph、Ruleset 包、CXC/Packed 与表现图的分层。
- [Chart v5 Gameplay Amendment](CHART_V5_GAMEPLAY_AMENDMENT.md)：对当前 Chart v5/CXT v2/Packed 定义的具体 Gameplay 增量修改。
- [Input, Replay and Extensions](INPUT_REPLAY_EXTENSIONS.md)：规范输入时间线、Replay 绑定和 capability 注册。
- [Review and Migration](REVIEW_AND_MIGRATION.md)：当前缺陷、V2 处置、迁移阶段和验收证据。
- [Chart v5 Alignment Review](CHART_V5_ALIGNMENT_REVIEW.md)：Chart v5 实施稿与 Gameplay V2 的版本、时间、Packed/CXC、Ruleset、Snapshot/Replay 和动画交接未决项审查。
- [Gameplay Expressiveness Stress Test](GAMEPLAY_EXPRESSIVENESS_STRESS_TEST.md)：96 个玩法、格式和生命周期案例的表达力压测、覆盖率口径与阻塞项。
- [Preliminary Design](PRELIMINARY_DESIGN.md)：结合压测结果的 Chart v5 Gameplay V2 初步总体设计，包含 Stage 7A 子集、资源状态机、求解器、迟到事件、Ruleset 事务、连续输入和 early/late 表现桥接。

## 与现有研究稿的关系

V2 保留现有稿件中已经证明有效的内容：整数数值域、量程/溢出证明、有限状态、prepare-time grace、观察与消费分离、严格能力拒绝、全量 Snapshot 起步策略和“无运行时任意脚本”边界。

V2 替换或提升以下边界：

- 以 `CoordinationGraph` 替代“只靠 Ruleset Fold 聚合跨音符”的表达；
- 以 `FactRecord` 统一原有 `phase/category/outcome/grade/error` 和证据来源；
- 以规范时间线替代分散的 observationTime、Tick、sequence 约定；
- 以状态事务和不可变 Effect 取代隐含的模块写入顺序；
- 以 prepared closure identity 替代仅按源字段拼接的 identity。

所有条目仍是 candidate，进入 ADR/Spec/ABI 前必须通过格式评审、案例矩阵、预算测量和跨平台确定性验证；
“已选路线”不等于“已实施能力”。接受后的 V2 合同必须在旧文档中留下明确的 superseded/retained
映射，避免实施者从两套字段定义中自行选择。

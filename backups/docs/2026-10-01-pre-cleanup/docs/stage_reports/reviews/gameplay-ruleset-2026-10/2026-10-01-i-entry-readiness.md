# Gameplay Ruleset：I 收敛前置审查

状态：dated review（I 收敛前置门，非实施报告）

更新日期：2026-10-01

本记录承接提交 `63f25d4` 的连续音符宽限 spike 证据，并更新至 C10-C14 完成后的状态，检查 Hold / 接触跟随型 Slider 的
断连宽限设计是否已经在候选文档中闭合，以及是否具备进入 I 收敛（ADR、Spec、预算与 ABI、
Stage 7 范围修订）的条件。它不启动 I 收敛，也不改变当前阶段状态。

**历史快照说明（2026-10-01）**：本文记录入口审查时点的“暂不进入 I”结论。用户随后已授权
进入 I 收敛；当前整理以 [I 收敛整理报告](2026-10-01-i-convergence.md)、[ADR 0043](../../../adr/0043-gameplay-judgement-ruleset-convergence.md)、
[Gameplay Judgement Spec](../../../formats/GAMEPLAY_JUDGEMENT_SPEC.md) 和
[Gameplay Judgement ABI](../../../api/GAMEPLAY_JUDGEMENT_ABI.md) 为准。

## 1. 审查范围

本轮只检查以下交叉合同：

```text
Ruleset Interface     游戏作者默认值、允许范围、谱面覆盖开关
Chart / Requirement   单音符 grace: Tick 与 explicit / inherited 来源
Prepare               量化、范围校验、最终值冻结、sticky / observe 拒绝
Runtime               Gap、严格 < 边界、普通 consume 仲裁、快照字段
Deadline / Budget     Hold 与 Slider 的 hard deadline、活动度扫描线、Gap timer 计数
Identity              chart / ruleset / session 三个分量的归属
```

不检查产品 `engine/`、Stage 7A ABI、真实设备输入、音频时钟或完整计分实现。

## 2. 当前设计判断

### 2.1 核心方案健全

复用 `handoff + Gap + holdGrace` 是当前最小改动路径：

- Hold 与连续 Slider 共用恢复路径；Slider 在 Gap 中保留 `seg`、路径进度和已发出的 head Fact。
- 不新增 `continuityGrace` Hook、`reconnect` Fact 或 Pattern 算子。
- 恢复按下仍参加普通 `consume` 候选集，不绕过新音符仲裁。
- 宽限比较使用严格 `evt.t < releasedAt + grace`，超时只产生一次 `Break`。

这套语义与 `hole` 的连续假段模型一致，未发现需要改动核心 Fold 概念的反例。

### 2.2 作者可定制性已经形成候选合同

当前候选值解析为：

```text
Ruleset Interface  声明 holdGrace 默认值、允许范围、allowChartGrace
谱面作者          可在 handoff Hold / 连续 Slider 写 grace: Tick（若被允许）
prepare            显式值量化并校验；缺省值继承 prepare 前的 holdGrace 默认值
运行中             prepared requirement 只读最终 grace，不因后续 Hook 改写 timer
```

显式谱面值优先于默认值，但不与技能贡献隐式叠加。`sticky` 或 `observe` 型 requirement 提供
覆盖时在 prepare 稳定拒绝。

身份口径已进一步裁决：`JudgementIdentity` 记录最终 prepared `grace` 与
`graceResolutionPolicy`；显式 / 继承来源、原始字段和继承层次只进入 `ContentIdentity` 与诊断。

### 2.3 文档合同已统一，但证据还没有完全闭合

本轮已同步 Program IR、Fold Calculus、Ruleset Fold、Skill Hooks、Identity、Budget 和讨论记录：

- Hold deadline 使用 `end + grace`。
- Slider deadline 使用 `t1 + max(W.max.late, grace)`，避免 Gap 接近尾部时提前 `onDeadline`。
- `grace` 的来源与量化值进入 chart identity；默认值、范围和覆盖开关进入 ruleset；Loadout 提供的
  prepare-time 默认值进入 session。
- 活动度扫描线与 Gap timer 只读取 prepared 值，不读取运行中 Hook。

因此，设计表达已经自洽；尚未完成的是针对这些边界的专项压测和真实内容测量。

## 3. 证据与缺口

| 门 | 当前证据 | 判断 |
| --- | --- | --- |
| 统一宽限的定向行为 | 9 个案例通过 | 通过候选 spike |
| 并发状态压力 | 2,048 条 Hold、26,624 个输入事件通过 | 通过候选 spike |
| GCC / clang 一致性 | 连续性输出逐行一致；D12 修复后几何摘要一致 | 通过研究性证据 |
| 每条 requirement 不同 `grace` | GCC/clang 通过 C10 | 通过研究性 spike |
| `grace = 0`、最小值、最大值 | GCC/clang 通过 C11 | 通过研究性 spike |
| explicit / inherited identity 区分 | GCC/clang 通过 C12：最终值相同 identity 相同，来源 / policy 可区分 | 通过研究性 spike |
| prepare 后 Hook 变化不追溯 | GCC/clang 通过 C13 | 通过研究性 spike |
| Slider 尾部 deadline | GCC/clang 通过 C14 | 通过研究性 spike |
| 代表性研究切片与预算测量 | GCC/clang 通过：96 条 requirement、672 事件、activity peak 3、Gap timer peak 2、快照恢复无损 | 通过研究性 spike；不等于真实作者内容 |
| 真实内容分布与预算冻结 | 尚未完成 | 未通过 |

C10-C14 已登记并完成于 [Gameplay 压测草案](../../../proposals/GAMEPLAY_STRESS_TEST_DRAFT.md) §2.1。
证据仍限定为研究性 spike：它没有接入产品 `engine/`，也没有替代真实内容切片。

## 4. I 收敛判断

**当前不能开始 I 收敛。**

原因不是核心设计不可行，而是 I 的输入合同还缺少一类证据：

1. 预算草案中的候选值仍需更广泛的真实内容分布测量；不能把研究性 spike 的合成数字直接
   写成 ABI 或生产限制。

此外，用户已经明确要求停在 I 收敛之前；本记录遵守该停止点。即使 C10-C14 后续通过，是否
进入 I 仍需一次明确的阶段授权。

## 5. 后续准入条件

在不进入 I 的前提下，下一批工作应只做：

1. 用更多真实内容分布验证 activity、Gap timer、尾部 deadline 与 Seek 成本；当前代表性切片只完成路径证据。
2. 把测得的 activity、Gap timer、尾部 deadline 影响写回预算证据；继续保持候选值与冻结值分开。
3. 由用户确认是否解除“I 前停止”后，才创建 ADR / Spec / ABI 变更。

## 6. 结论

连续音符宽限的**设计方向健全、实现路径最小、作者自定义入口已明确**；但它还没有达到可
进入 I 收敛的证据门槛。当前状态应保持为：**I 前置文档整理完成，I 收敛暂缓**。

相关证据：[连续音符宽限 spike](2026-10-01-fold-spike.md)。

# ADR 0043：Gameplay Judgement 与 Ruleset 收敛边界

状态：superseded；I 收敛工作稿，未实施，从未被接受为实施基线；已由 [ADR 0044](0044-gameplay-v2-semantic-kernel.md)（Gameplay V2 语义内核与冻结边界）取代，字段与运行语义改为 [Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md)，typed 边界改为 [Gameplay V2 ABI](../api/GAMEPLAY_V2_ABI.md)。本文件保留为 Gameplay I 的历史候选基线与差异对照，不删除、不再更新结论。

日期：2026-10-01

## 背景

Stage 7 的 Gameplay 研究已经完成 Fold/Pattern/Measure、Ruleset Fold、Identity、Skill Hook、
预算和连续音符宽限的交叉审查。研究 spike 在 GCC 与 clang 下完成了 34 项案例、D11/D12
确定性复测，以及 C10-C14 的 prepared grace 矩阵；证据仍是研究性实现，不代表 `engine/`
或公开 SDK 已实现。

本 ADR 把已经完成的设计取舍提升为 I 收敛输入。字段与执行语义见
[Gameplay Judgement Spec](../formats/GAMEPLAY_JUDGEMENT_SPEC.md)，候选公共类型和生命周期见
[Gameplay Judgement ABI](../api/GAMEPLAY_JUDGEMENT_ABI.md)，压测证据见
[Fold Spike 报告](../stage_reports/reviews/gameplay-ruleset-2026-10/2026-10-01-fold-spike.md)。

## 决策

### 1. 分层边界

判定拆成四层：

```text
L1 Input       设备事件、映射、连续量重采样、Replay 规范化
L2 Program     Requirement 的 Pattern、Measure、资源归属和 Fact
L3 Ruleset     仲裁、Fact 折叠、模块、Hook 合成和计分状态
L4 Presentation 只读 JudgementState / Effect，不反向改变判定事实
```

L2 不读取宿主、World、渲染帧或墙钟；L3 不生成或删除 Requirement；L4 不参与判定。
运行时脚本和逐帧脚本回调不属于任何一层，继续无限期禁止。

### 2. 连续音符宽限采用最小改动方案

Hold 和接触跟随型 Slider 复用 `handoff + Gap`。每个 requirement 在 `prepare` 得到冻结的
最终 `grace`：

```text
显式 grace       量化并校验 [min, max]，允许时优先使用
未显式提供       继承 Ruleset / Loadout 的 holdGrace 默认值
sticky/observe   提供覆盖时稳定拒绝
运行中 Hook      不得追溯改写 prepared grace 或已安排的 timer
```

恢复比较为严格 `event.t < releasedAt + grace`。Hold 的 hard deadline 为 `end + grace`；
Slider 为 `t1 + max(window.maxLate, grace)`。恢复按下仍进入普通 `consume` 仲裁，不绕过新
音符候选集。`grace = 0` 自然退化为不允许断连。

该方案不新增 Hook、Fact 或 Pattern 算子，且允许游戏作者和谱面作者分别控制默认值与单条
音符覆盖。最终值和 `graceResolutionPolicy` 进入 JudgementIdentity；显式/继承来源只进入
ContentIdentity 与诊断。

### 3. 核心表达模型

Requirement 是 `Pattern + Measure + Grading + Effect`。Pattern 保留原子、序列、择一、有界
重复、跳步、同接触点、瞬时和补；删除积。Measure 允许并列分量，每个分量有自己的 phase 与
category。多指 Slide 表达为多个独立 requirement，由 L3 聚合。

非确定 Pattern 使用 `leftmost-first`；该策略进入 Interface 与 JudgementIdentity。`sameContact`
是 Pattern 约束，资源绑定是 Effect，谁拥有资源由 Ruleset 仲裁决定。

### 4. 仲裁与折叠

仲裁是 Ruleset 级声明，不是 Pattern 的隐含行为。`consume` 候选按声明的 `claimKey` 稳定排序，
每个事件逐次重算候选，取前 `fanout` 项；默认 `fanout = 1`。实例枚举顺序不得改变结果。

同 Tick 的 Fact 在折叠前按 `(requirementId, phase, outcome)` 规范排序；折叠处理器顺序由包
清单冻结。Hook 在 Tick 末统一提交，下一个 Tick 才可见。`sameContact`、槽位复用、`strayWhen`
和 `observe` 的可见性按 Spec 的状态矩阵执行。

### 5. 确定性与数值域

离散边沿保留原始时间戳；连续量按 Interface 声明的规范格点线性重采样。决策侧使用一次性
整数投影，禁止未声明的浮点或宿主数学库。每个判定域必须声明坐标/尺寸量程，且每一项判定
不等式都必须证明不溢出；仅“使用整数”不足以保证确定性。

定点表由 `tableRegistry` 统一登记，tableId 即语义内容，只增不改。引擎阶段顺序、信号的
下一 Tick 投递和 Hook 快照时点属于 engine 语义；Ruleset 的处理器/模块顺序属于 ruleset
内容 hash。

### 6. Identity 与版本

判定 Replay 使用四个分量：`engine`、`ruleset`、`chart`、`session`。资源和表现绑定不进入
JudgementIdentity。`engine` 包含判定语义版本与实际引用的 tableId；`ruleset` 包含 Interface
投影、处理器顺序、模块和 Build hash；`chart` 包含编译后的 Pattern/Measure、量化值和 prepared
grace；`session` 包含 Loadout、默认 grace、输入重采样和判定配置。

同一最终 grace 和 policy 即共享 judgement identity，即使来源不同；来源差异保留在内容身份
和诊断中。判定语义版本与 SDK API 版本独立；判定语义变更必须按清单项显式放行并作为发行事件。

### 7. 预算与快照

容量、稳态和 Seek 成本分开计。活动度、Gap timer 和周期采样共享每 Tick 预算乘数；L3 Fact
成本单独计。快照必须无损，且保存连续量、Gap 状态、prepared grace、采样相位、Hook 快照和
折叠状态。第一版采用全量快照；`maxSeekLatency` 是引擎承诺，会话只能收紧。

研究切片的 96 条 requirement、672 个事件、activity peak 3、Gap timer peak 2 和 4,777
字节快照仅作为路径证据，不冻结生产 ABI 限额。生产限额必须由真实内容测量后单独接受。

### 8. Ruleset 包与扩展性

内置模式和分发 Ruleset 共用同一 Interface。Ruleset 包复用 CXC v1 的 ZIP32 Stored 载体和
manifest 形状，但使用 `cuexis.ruleset` format，不复用 Chart entry 分层。`programPolicy` 与
`outcomeScope` 分离；默认只能产出已声明 outcome/category。独占 Hook 所有权静态指定，必要时
使用 fallback；派生 Hook 使用可交换、可结合的合成算子；模块冲突在 prepare 拒绝。

新增能力必须版本化并有 capability 与稳定拒绝路径，不能通过修改既有字段含义实现“隐式更新”。

## 取舍与拒绝项

| 议题 | 选择 | 被拒绝方案 | 原因 |
| --- | --- | --- | --- |
| 宽限位置 | requirement prepared grace | 运行期全局可变 Hook | 会使重放和 timer 依赖历史状态 |
| 宽限来源 | 默认值 + 可选单条覆盖 | 隐式叠加技能贡献 | 结果来源不透明，难以做 identity |
| 匹配策略 | leftmost-first | leftmost-longest | 后者需要回看，扩大状态和 Seek 成本 |
| 多指表达 | 独立 requirement + L3 聚合 | Pattern 积 | 积没有提供必要语义，增加复杂度 |
| 快照 | 第一版全量 | 先做增量 | 尚无证据证明全量不可行，增量差分增加风险 |
| ABI | C++ typed preview | 现在冻结稳定 C ABI | 稳定 C ABI 属 Stage 14，过早冻结会阻塞演进 |

## 威胁模型

Ruleset、Chart 和 Replay 都可能来自不可信来源。prepare 必须拒绝非法引用、越界/溢出、非终止
程序、预算超限、未知 outcome、未声明 Hook、身份不匹配和不支持 capability。运行期不得执行
IO、随机数、墙钟、宿主回调或动态生成 Requirement。失败必须稳定、可诊断，不能静默降级为 Tap、
Hold 或旧版本语义。

## 后续边界

本 ADR 不声明 `engine/` 实现完成，不冻结真实预算数值，不切换 Chart v5 默认 Writer，也不
启动 Studio 语法、完整 Stage 7B 高级能力或 Stage 14 C ABI。owner acceptance、Spec/ABI 实现和
真实内容预算测量是后续工作项。

**阶段归属澄清（缺陷 `D-11`，2026-10-03）。** 稳定 C ABI 的**唯一归属阶段是 Stage 14**，本 ADR 上表
"稳定 C ABI 属 Stage 14"即为该口径；Stage 7A / 7B / 8 只交付 C++ typed preview（见
[Stage 14 计划](../stage_plans/future/stage-14/plan.md)）。`AGENTS.md` 中把稳定 C ABI 写作 Stage 12 的
表述是**孤例**，由 owner 择时订正；本轮不修改 `AGENTS.md`。

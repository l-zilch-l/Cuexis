# Gameplay Judgement Spec

状态：candidate；I 收敛规范工作稿，未实施，待 ADR 0043 owner acceptance

日期：2026-10-01

本文是 Gameplay 判定字段和运行语义的单一候选权威。研究背景、案例推演和缺陷发现保留在
[Gameplay 设计压测](../proposals/GAMEPLAY_STRESS_TEST_DRAFT.md)、[Fold Calculus 封闭性验证](../proposals/GAMEPLAY_CALCULUS_CLOSURE_CHECK.md)
和 [Fold Spike 报告](../stage_reports/reviews/gameplay-ruleset-2026-10/2026-10-01-fold-spike.md)。

## 1. 范围与层次

```text
InputEvent -> InputMapping -> JudgementRequirement -> JudgementFact
           -> Ruleset Fold -> Score/Combo/Statistics
```

判定独立于渲染帧率、World、EnTT、SDL、OpenGL、音频后端和宿主引擎。L1 负责输入归一化；L2
负责 requirement；L3 负责仲裁和折叠；L4 只能消费结果。

## 2. 输入合同

`InputEvent` 至少包含规范观测时间、InputDomain、Action、位置或通道、可选 amount/pressure、
source identity 和 sequence。离散边沿不重采样；连续量按 Interface 声明的规范格点线性插值。
Replay 记录重采样后的规范事件流，而不是设备原始事件流。

设备类型之间不承诺完全等价；只承诺同一设备类别在声明的最低上报率以上具有相同求值格点。
输入映射和校准进入 session 元数据，是否参与判定由 `JudgementConfig` 明确声明。

## 3. Requirement

```text
requirementId
time interval / anchor
judgement domain
required action
pattern
measure[]
grading
grip / ownership
prepared grace (optional)
```

Pattern 原语：atom、sequence、choice、bounded repeat、skip、sameContact、instant 和 complement。
不提供积。非确定匹配采用 `leftmost-first`，同一输入轨迹因此只有一个规范匹配。

Measure 可以有多个分量；每个分量带 `phase`、`category` 和量化规则。典型 Hold 为 head 时间误差
加 body coverage；L3 可分别应用时间窗口和覆盖率等级。

## 4. 连续音符 grace

仅 `handoff` Hold 和接触跟随型 Slider 可使用 `grace`。Ruleset Interface 声明：

```text
holdGrace       默认 Tick
holdGraceMin    最小 Tick
holdGraceMax    最大 Tick
allowChartGrace 是否允许 requirement 显式覆盖
graceResolutionPolicy 解析策略版本
```

prepare 顺序：量化显式值；检查允许开关和 `[min,max]`；未提供时复制 prepare 前冻结的默认值；
对 sticky/observe 的非法覆盖返回稳定错误。prepared requirement 只读该值。

运行语义：松开时进入 Gap 并记录 `releasedAt`；若后续相同资源的按下满足
`event.t < releasedAt + grace`，恢复原阶段；否则在 timer 到期时产生一次 Break。恢复事件仍
参加普通 consume 仲裁。

```text
Hold deadline   end + grace
Slider deadline t1 + max(window.maxLate, grace)
```

`grace = 0` 不允许同 Tick 或零间隔恢复。精确边界不接受，避免不同时间表示产生歧义。

## 5. 生命周期和求值顺序

每个 Tick 固定为：

```text
1. 激活到期 requirement
2. 处理到期 Gap / hard-deadline timer
3. 应用规范化输入事件
4. 为每个事件重建候选并执行仲裁
5. 按规范 Fact 顺序执行 Ruleset Fold
6. 提交信号和 Hook 写入，下一 Tick 可见
```

同 Tick 的所有实例读取同一份 Hook 快照。Controller 在更新它的事件上读取更新前的值。
实例枚举顺序不是语义输入；Fact 在折叠前按 `(requirementId, phase, outcome)` 排序。

## 6. 资源归属与仲裁

`sameContact` 只约束 Pattern 匹配必须使用同一接触点；`claim` 才产生资源独占。`observe` 可以
观察但不消费。`consume` 候选按 `claimKey` 和 requirementId 稳定排序，取前 `fanout` 项；每个
事件最多由一个 consume 实例消费，除非 Ruleset 显式提高 fanout。

槽位内已终止归属可被新 claim 隐式释放；未终止归属不进入候选。`strayWhen` 由 Ruleset 选择：
`consumeEmpty` 或 `noCandidate`。后者表示没有任何实例考虑过该事件，不是“实例考虑后拒绝”。

## 7. Ruleset Fold

Ruleset 包声明 interface、arbitration、fold、modules、defaults、Hook ownership 和能力位。
折叠程序只能读 Fact、声明状态和已声明 Hook；不能修改 L2 实例、生成 Requirement 或改变已发
Fact。模块处理顺序固定在包清单中；同一写目标只能有一个 owner，派生值必须使用可交换、可结合
的合成算子。信号只能在下一 Tick 投递。

`programPolicy` 控制程序来源（locked / extendable / open）；`outcomeScope` 控制结果集合
（declared / extended）。`extended` 只在 open 下可用。默认 outcomeScope 为 declared。

## 8. 数值、几何和静态验证

作者数据可以使用浮点，但 prepare 必须一次量化到声明的整数域。几何判定禁止依赖未声明的浮点
容差、平台数学库或有符号溢出。每个判定域声明坐标/尺寸量程；验证器必须证明判定式的每一项
在量程内。静态验证还必须检查类型引用、终止性、结算完备、预算、Hook 范围和禁止项。

## 9. Identity

判定身份由四个分量组成：

| 分量 | 内容 |
| --- | --- |
| engine | 判定语义版本、实际引用的定点表 tableId、引擎阶段顺序 |
| ruleset | Interface 投影、折叠/模块顺序、Build hash、Hook 合成和能力 |
| chart | 编译后的 Pattern/Measure、锚点量化、显式 grace 字段与量化值 |
| session | Loadout、默认 grace、实际 Hook 值、重采样格点和 JudgementConfig |

最终 prepared grace 与 `graceResolutionPolicy` 进入 judgement identity。explicit/inherited 来源、
原始字段和继承层次进入 content identity 与诊断，不单独改变 judgement identity。表现资源和
默认绑定不进入 judgement identity。

## 10. 快照、Seek 与预算

快照必须无损保存：活动实例、Pattern 状态、接触归属、Gap timer、releasedAt、prepared grace、
采样相位、Hook 快照、Fold 状态、统计和信号队列。Seek 从最近快照连续推进，结果必须逐位等于
从起点播放。第一版使用全量快照；快照间隔是引擎内部参数，不进入 identity。预算分为容量、稳态
和 Seek 三类；activity、Gap timer 和周期采样共享每 Tick 预算，L3 Fact 成本单独计。

## 11. 能力和拒绝

不支持的 RequirementKind、Pattern、Outcome、Hook、3D 几何、连续音高和运行时脚本必须稳定
拒绝，不能降级。Ruleset、Chart、Replay 的身份不匹配、预算超限、非法时间回退、量程不满足和
资源归属冲突同样稳定失败。

## 12. 明确排除

本 Spec 不定义作者 DSL/JSON/Studio 编辑器、不承诺 Stage 7B 全部高级能力、不冻结 Chart v5
默认 Writer、不实现产品 `engine/`、不定义稳定 C ABI。稳定 C ABI 继续归 Stage 14。

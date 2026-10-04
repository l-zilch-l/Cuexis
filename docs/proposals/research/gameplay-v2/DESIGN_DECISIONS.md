# V2 设计决策与新增语义

状态：candidate；研究提案，未接受，未实施

更新日期：2026-10-02

## 1. 现有设计中保留的部分

以下决定是正确方向，应在 V2 中保留：

| 现有决定 | V2 处理 | 理由 |
| --- | --- | --- |
| 离散边沿与连续量分开规范化 | 保留 | 这是 Replay 可复现和设备差异隔离的基础。 |
| prepare 时量化、整数决策域 | 保留并扩大到所有编译产物 | 只在局部 Requirement 量化仍会留下跨图关系溢出和单位混用。 |
| Pattern 只表达局部顺序 | 保留 | 正则式局部程序易验证、易快照；不应承担全局关系。 |
| Ruleset 与 Presentation 分离 | 保留并强化 | 结果状态和画面效果必须经过不同闭包。 |
| Replay 记录规范化输入 | 保留 | Replay 不应绑定键位、设备型号或渲染资源。 |
| capability + 稳定拒绝 | 保留 | 扩展必须可查询、可拒绝、可记录，不能静默降级。 |
| 无运行时脚本、无 IO、无随机 | 保留 | V2 的 Coordination 也必须是有界声明式数据，不是通用 VM。 |

## 2. 必须改造的部分

### 2.1 从“Fact 直接提交”改为“候选—协调—提交”

当前局部程序在看到输入后即可发出 `Hit` 或 `Miss`，然后由 L3 处理。这对 Tap/Hold 足够，但对共享接触点、和弦、交替、互斥音符和“同一输入只能使一个组成功”不够。V2 中局部程序只发出 `Candidate`：它包含候选要求、需要的资源、满足的局部条件、评分摘要和截止时间。Coordinator 根据图上的关系一次性选择一个可行集合；只有被选中的候选才转成 Fact。

### 2.2 新增显式 CoordinationGraph

CoordinationGraph 是编译后 Canonical Gameplay Graph 的一部分，边和节点都带静态类型。第一版只需要四类关系：

- `exclusive(resource)`：同一资源在一个提交窗口内最多被一个候选占用。
- `binding(group)`：一组候选必须全部成功、全部失败或按声明的 cardinality 提交。
- `temporal(before/after/within)`：候选之间的有限时间关系，不能创建运行期 Requirement。
- `quota(min/max)`：在有界窗口内的最少/最多成功数量。

关系不使用 Presentation 的 parent/tree，也不依赖源文件数组顺序。关系求解必须有静态上界；无法在 prepare 时证明候选数量、窗口和回溯深度的图稳定拒绝。

### 2.3 FactRecord 采用因果总序

现有 `(requirementId, phase, outcome)` 排序不足以区分同 Tick 的输入和 timer 因果。V2 的排序键为：

```text
(chartTick,
 phasePriority,
 sourceEventSequenceOrTimerOrdinal,
 coordinationCommitId,
 requirementId,
 componentIndex,
 emissionIndex)
```

`FactRecord` 必须包含 `factId`、产生它的输入或 timer 因果、Requirement、component/phase/category、outcome、可选 grade/measure/error、reason 和 evidence。Ruleset 只能读取已提交账本；同一 Tick 的多条 Fact 不得靠容器遍历顺序决定。

### 2.4 Effect 从 Requirement 移出

Requirement 只描述判定输入和产生的事实。音符变灰、按键音、粒子、UI 提示等由 `FactBinding` 或 `EffectSchedule` 从事实映射到只读表现事件。Effect 进入 Presentation closure，不进入 JudgementIdentity。需要影响分数、生命或失败的行为必须先成为 Ruleset state transition，不能通过表现 Effect 回写。

### 2.5 把 Ruleset 折叠改成有类型的状态交易

Ruleset 不再只是“处理器列表 + Hook”。它应声明：输入 Fact 类型、状态寄存器、每个寄存器的 reducer、冲突策略、模块权限、信号投递时点和输出投影。每个 Tick 先按 Fact 总序产生一个 state transaction，再原子提交。两个模块写同一独占寄存器必须在 prepare 拒绝；派生寄存器只能使用声明的可交换、可结合 reducer。

## 3. 新增语义数量的估计

V2 不建议继续无上限增加 Pattern 原语。相对于当前工作稿，真正新增的**核心语义约 9 项**：

1. `NormalizedObservation` 的 ingress sequence、discontinuity 和 late-event policy。
2. `Candidate` 与 `CoordinationCommit` 两阶段结果。
3. `CoordinationGraph` 及 exclusive/binding/temporal/quota 四类关系。
4. 资源 claim 的生命周期与提交窗口。
5. 带因果键的 `FactRecord`。
6. Fact ledger 的 append-only/retraction 禁止规则。
7. Ruleset state transaction 与寄存器 reducer/冲突策略。
8. 独立的 `EffectEvent`/`FactBinding` 表现事件语义。
9. Canonical Gameplay Graph、source map 和四类 identity/closure 的分离。

这 9 项中有 5 项是结构补洞（1、2、5、6、7），4 项是为了可扩展性和格式解耦（3、4、8、9）。它们不是九种新玩法；Chord、共享接触、交替、保护音符、Hold 断连等都应由这些语义组合表达。

## 4. 明确删除或降级的内容

- 删除 Requirement 内的 `Effect` 字段。
- 删除“由 requirementId 排序即可保证确定性”的假设。
- 降级 `Hook` 为 L2 可见参数或 L3 本地状态的两类明确接口；不再把所有技能能力都包装成 Hook。
- 删除 CXT 作为 Gameplay 程序来源的隐式约定；CXT 只保留表现/动画模板，Gameplay 使用独立 module。
- 不新增通用脚本 VM、动态 Requirement 生成、运行期谱面变换、运行期播放倍率或任意插件执行权。

## 5. 扩展性判据

新增玩法只有在以下条件下才允许进入已有语义：

1. 能编译为有限 RequirementProgram 和有限 CoordinationGraph。
2. 所有状态、候选数、定时器和求解分支都有 prepare-time 上界。
3. 新结果使用已有 Fact vocabulary；若必须增加 outcome/category，必须加 capability/revision 并更新 identity。
4. 不改变既有输入事件的解释、Fact 总序或 Ruleset reducer。
5. 可由 canonical graph、snapshot 和 normalized Replay 完整恢复。

否则应新增独立 capability 或另立 ADR，而不是添加一个“任意扩展字段”。

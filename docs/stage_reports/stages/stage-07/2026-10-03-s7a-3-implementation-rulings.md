# S7A-3 实施裁定记录（第二半 part 1 第一轮实现复核修订 + 第 2、3 轮复审改正）

状态：dated implementation record（S7A-3 第二半 part 1 的语义裁定、缺陷修订与文档落地证据；不是阶段关闭、
不是 owner acceptance、不是实施验证、**不是发布或提交声明**，也**不表示 S7A-3 已实现完成**）

更新日期：2026-10-03

上级文档：[Stage 7A 实施计划](../../../stage_plans/active/stage-07/plan.md) §S7A-3 ·
[Gameplay V2 acceptance package](../../../proposals/gameplay-v2-acceptance/README.md) ·
[未决语义分轮裁决清单](../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) §3、§9

本文是**带日期的落地记录**（reports own dated evidence）：它登记 S7A-3 第二半 part 1 第一轮实现复核
（verdict `reject`、confidence 0.94）后，逐项修订的**语义裁定**、**门禁状态变更**、**诊断码增删**、
**Spec 同步**与**该轮跑过的门禁及结果**；并登记**第 2 轮实现复审**（verdict `reject`、confidence 0.91）
后的改正：§1 至 §14 为第 1 轮正文（其中与第 2 轮冲突的口径已**就地改正**并标注历史），**§15 为第 2 轮
的正文**。它不复制 Spec / ABI 的合同正文，也不构成任何实施完成声明；未在本文列出的门禁一律视为未运行。
第 3 轮准入门禁本身的裁定落地证据仍以
[第 3 轮门禁报告](2026-10-03-s7a-3-gate-rulings.md)为准，本文**不回改**该报告。

## 1. provenance

| 卡 | thread | verdict | confidence | 日期 / 模型 | 内容 |
| --- | --- | --- | --- | --- | --- |
| 实现复核卡（第 1 轮） | `s7a3b-implementation-review` | `reject` | **0.94** | 2026-10-03，`gpt-6-astra` | 8 项逐条处置：第 2 项**驳回**、第 1 / 3 / 5 / 6 项**修订**、第 4 / 7 / 8 项**接受**（第 7 项另加两项测试要求） |
| 实现复审卡（第 2 轮） | `01a0fe91-416c-7f31-a48a-d1890a0e68d1` | `reject` | **0.91** | 2026-10-03，`gpt-6-astra` | 9 项逐条处置：第 3 / 4 / 5 项**驳回**（测量失败与数值溢出仍形成未经接受的内容拒绝门禁）、第 6 / 8 项**修订**、第 1 / 2 / 7 / 9 项**接受**；另要求把第 3–5 项的反例与门禁状态写进本文与 Spec、修订后重跑全部门禁，并登记两条风险 |

**登记口径。** 第 1 轮卡的 threadId 为 `01a0fe91-416c-7f31-a48a-d1890a0e68d1`。裁决卡文件名时间戳为
2026-10-02T21:46:00.993Z（UTC），按第 2 至第 7 轮先例统一按**本地日期 2026-10-03** 登记。本轮修订属
**S7A-3 第二半 part 1 的实现批次内部**，不新开裁定轮次、不新增 R 条目、不新增 ABI 类型行，也不改变
契约计数（§7.2 仍 19 条、§9.2 仍九类、契约处置词不变）。

**第 2 轮复审卡的口径。** 该卡的 thread 同为 `01a0fe91-416c-7f31-a48a-d1890a0e68d1`，verdict `reject`、
confidence **0.91**，模型 `gpt-6-astra`，裁决卡文件名时间戳 `2026-10-02T22-33-54-872Z`（UTC），同样按
**本地日期 2026-10-03** 登记。它的结论口径（原文）："暂不接受 S7A-3 第二半 part 1：**第 3–5 项的测量
失败和数值溢出仍会形成未经接受的内容拒绝门禁**。"其逐条处置、改正落点与本轮门禁见 **§15**。第 2 轮
同样**不新开裁定轮次 / R 条目 / ABI 类型行**，也**不改变**契约计数。

**Codex 的登记指令（第 1 轮，原文）。** "在 S7A-3 实施裁定记录中登记第 1、2、3、5 项的语义及门禁变更，
并同步 Spec；不新增 ABI 类型、诊断类别、R 条目或契约计数。修正后重跑 judgement 测试、architecture
CTest、format check 和文档检查，再决定是否接受这一半。"

**Codex 的登记指令（第 2 轮，原文口径）。** "把第 3–5 项的反例与门禁状态写进**现有** S7A-3 裁定记录
与 Spec；修订后重跑全部门禁。"（本文 §15 即该落地记录。）

## 2. 逐项裁定（8 项）

| # | 复核处置 | 裁定结论（本轮落地） | 落点 |
| --- | --- | --- | --- |
| 1 | revise | `skip` / `instant` 语言相同、**身份不同**；`complement` 是**前缀消费、可组合**的；`maximumTraceLength = nullopt` 只表示"目前没有有限上界可用"，**不表示**"已证明无界" | Spec §3.8.8 第 1、2 条；`gameplay_assembler.hpp`（`CompiledPattern` 类注释、`maximumTraceLength` 注释）；`pattern_dfa.hpp`（complement 构造注释与 `complementOf`）；测试 `skip / instant / complement` 段 |
| 2 | **reject** | **删除**空操作数的一律拒绝（含诊断码与测试），改为：份数序 = **递增份数下标**、接受集 = 可允许份数上的**并集**、**不要求每份消费元素**、在位置集**不动点**终止；补**终止性**与**等价性**测试 | Spec §3.8.4 第 5、6 条；`gameplay_assembler.cpp`（repeat 分支：`copyCountChecked` + `multiplyCounts`，删除 nullable 拒绝）、`PatternTraceEvaluator` 的 repeat 不动点分支；`source_codes.hpp` 删除 `kRepeatOperandEmptyCode`；测试新增段 |
| 3 | revise | 三项**互不替代**的计数：**完全展开计数**（按可允许份数求和，每分支上界 `maximum × 单份展开数`，无可表示值 ⇒ **测量缺口 / 缺席**）、**interned 子模式数**、Spec §8.2 的**确定化并最小化后状态数**（按需测出；**本行口径已被第 2 轮复审改正**：测不出 ⇒ **测量缺口**、**不拒绝**，见 §4 与 §15.4；表内其余文字为第 1 轮原文）；全部**受检算术** | Spec §8.2 新增"Pattern 的三项计数口径"段；`gameplay_assembler.hpp`（`fullyExpandedCount` / `largestBranchExpansion` / `internedSubpatternCount` / `stateCount` 声明与注释）；`gameplay_assembler.cpp`（`CompiledMetrics`、`buildCompiledPattern`、`measurePatternStateCount`、`enforcePatternBudget`） |
| 4 | accept | `leftmostAcceptingArm` 维持：最低的、接受整条轨迹的根 choice 操作数下标（非 choice 根 ⇒ `nullopt`） | `gameplay_assembler.hpp` / `.cpp` 既有实现与测试，不改 |
| 5 | revise | arm / deadline 包含性门禁标记为 **incomplete（未接入装配路径）**；接入时必须检查**全部可接受长度**（最短**与最长**），不是只读最短 | Spec §3.8.8 第 3 条；`gameplay_assembler.hpp` 的 `PatternArmBound` / `checkPatternContainment` 注释（`INCOMPLETE GATE`）；`gameplay_assembler.cpp` 的 `checkPatternContainment` 与第二半注释块；测试新增反例段 |
| 6 | revise | `PhaseKind::tap`、phase 派生的 `FactCategory`、grade token 维持进 identity + diff；`compileMeasure` **只存**不透明 token；明确 grade **求值**不在本批次 | Spec §3.8.8 第 4 条；`gameplay_assembler.hpp`（`CompiledMeasure` / `CompiledMeasureComponent::declaredGradeTokens` 注释）；测试头注释与断言注释措辞 |
| 7 | accept + 补充 | ① 新增**溢出路径**测试（测量记账）；② 新增**经 identity 投影的深 Pattern** 测试；测试测量值与私有记账常数**不得**变成生产阈值 | 测试：`referenceEvaluatorWorkingBytes` 段、`S7A-3 a deeply nested pattern is projected through identity`；见第 7 节（S7A-3.2 扩展登记） |
| 8 | accept（仅校验） | ABI 的家族误用句（`码族来源` 段："该段仅为码族分组示例，不是第二套 category 枚举；权威枚举为 SPEC §9.2 九类"）与 Spec 的 `capability_disabled` 均已落地，**不重复追加** | [ABI](../../../api/GAMEPLAY_V2_ABI.md) `码族来源` 段（复核时 L589–592，并行文档线编辑后现为 L605–607 / L669）；Spec `capability_disabled` 段 |

## 3. 第 2 项：份数序、终止性与等价性证据

**删除的内容。** 本轮删除了"操作数可匹配空轨迹 ⇒ 一律稳定拒绝"的规则，包括诊断码
`judgement.s7a3.pattern.repeat_operand_matches_empty` 及其在 `source_codes.hpp` 的常量、在
`gameplay_assembler.cpp` 的检查点与在测试中的整段断言。删除理由（Codex 原文）："有限上界本身不依赖每份
都消费元素。"

**份数序（确定性复制顺序）。** 有界循环 `repeat(X, minimum, maximum)` 的完全展开按**递增的份数下标**
`k = minimum, minimum + 1, …, maximum` 进行；接受集是各份数分支接受集的**并集**，**不要求任何一份消费
元素**。因此 `repeat(X, m, M)` 与手写的 `choice(X^m, X^{m+1}, …, X^M)`（其中 `X^k` 是 k 份连接）语言
相同；`X` 可空时各分支的语言**允许重合**，重合不构成拒绝理由。

**终止性论证。** 求值/构造的每一步只做两件事之一：把位置集沿操作数消费**前移一格**，或到达
`next == current` 的**不动点**（此时无条件并集并返回）。位置集宽度受轨迹长度限制，前移次数受轨迹长度
限制，不动点立即返回，因此步数上界是 `min(maximum, 每元素一步) + 常数`，与 `maximum` 的大小无关：
声明 `maximum = UINT64_MAX` 同样终止（测试 `the largest representable bound still terminates`）。

**等价性证据。** 测试 `an operand that can match the empty trace is expanded, not refused`：

- `repeat(skip, 1, 4)`：接受集 `{ε}`，`fullyExpandedCount() == 10`（Σk = 1+2+3+4，每份 1 个实例）、
  `largestBranchExpansion() == 4`、`stateCount() == 1`（最小自动机只有一个状态）；
- `repeat(choice(atom.one, skip), 1, 2)` 与手写 `choice(skip, atom.one, sequence(atom.one, atom.one))`
  （即 `{ε, a, aa}`）在**长度 0 至 3 的全部 15 个词**上逐词一致，且两者的**最小化状态数相等**（3）。

## 4. 第 3 项：两项计数的定义与测量方法

> **本节口径已按第 2 轮复审改正（见 §15.3 / §15.4）。** 下表左右两列中的"无表示 / 测不出时的行为"
> 一栏是**上一轮**写入的口径：它曾把"计数无表示"与"状态数测不出"都映射成 `budget_exceeded` 拒绝。
> 该映射被第 2 轮 Codex 复审**驳回**（thread `01a0fe91-416c-7f31-a48a-d1890a0e68d1`、verdict
> `reject`、confidence 0.91、裁决卡 `2026-10-02T22-33-54-872Z`，驳回理由：**内部构造失败与计数超出
> `uint64` 范围都不得充当内容拒绝门禁**），并在本轮删除。**现行**口径是"计数无表示 / 测不出 ⇒ 该维度
> 的**测量缺口**（访问器缺席），本身绝不构成拒绝；只有**已测量**的计数确实超过**已接受**上限时才按
> `budget_exceeded` 拒绝"。表中两行已就地改为现行口径，历史口径仅在本注中保留。

| 计数 | 定义 | 无表示 / 测不出时的行为（**现行**） | 访问器 |
| --- | --- | --- | --- |
| 完全展开计数 | 按可允许份数求和得到的**图形原语实例数**（每份展开计 1，逐层相加） | **测量缺口**：返回 `nullopt`（缺席），**不是**拒绝 | `fullyExpandedCount()` → `std::optional<std::uint64_t>` |
| 每分支上界 | `maximum × 单份展开数`（单个分支的最大实例数） | **测量缺口**：返回 `nullopt`（缺席）。该计数**没有**自己的预算维度，所以它**只**记缺口、**从不**触发拒绝。（上一轮口径为"无表示 ⇒ `judgement.s7a3.pattern.expansion_not_representable`（`budget_exceeded`）"，该码见 §15.3） | `largestBranchExpansion()` → `std::optional<std::uint64_t>` |
| interned 子模式数 | 编译内部表示中**不同**子模式的个数（结构相同的子模式共享一项） | — | `internedSubpatternCount()` → `std::uint64_t` |
| Spec §8.2 状态数 | **确定化并最小化之后**的状态数 | 测量工作集上界内构造不出 ⇒ **测量缺口**：返回 `nullopt`（缺席），**不得**用前两项顶替，**不得**写入替代数值，**不得**由此拒绝；状态预算门禁因此保持 **incomplete**（§15.5）。（上一轮口径为"报未测量并按 `budget_exceeded` 拒绝，码 `judgement.s7a3.pattern.state_count_not_measured`"，该码见 §15.3） | `stateCount()` → `std::optional<std::uint64_t>`（按需测出，非 `noexcept`） |

**已测量计数与已接受上限的比较（现行）。** 状态数**测得出来**时仍按已接受上限比较：`stateCount()`
超过 `maxStateCount` 是 `judgement.s7a3.pattern.budget_exceeded`（类别 `budget_exceeded`，path
`pattern.maxStateCount`）。测试 `a measured count above the accepted bound is still a budget overrun`
锁死这一半；测试 `a state-count measurement gap never refuses content` 锁死另一半（缺口不拒绝）。

**测量方法。** Spec §8.2 的状态数由 `detail::buildMinimalPatternDfa` 在**按需**调用时测出：子集构造
（`determinise`）→ `trim`（可达 + 共可达）→ `mergeEquivalent`（无环时自底向上 hash-consing，有环时
补 sink 做 Moore 精细化）→ `minimise`。测量本身受**本模块私有**工作集上界约束
（`kPatternDfaConstructionStates = 65536` 状态、`kPatternDfaConstructionWork = 1 << 24` 单位），该上界是
**测量能力**的上界而非内容限额；当有界循环的 `(子状态数 + 1) × (maximum + 1)` 对空间不可能容纳时，
`patternRepeatPairSpace` 立即判定并 `abandon()`，**不materialise任何一份拷贝**。**但放弃测量不等于拒绝**：
缺口由访问器如实报告，编译继续成功（§15.3）。

**另一项独立测量（Risk 1）。** 参考求值器的**运行时**工作集单列：
`referenceEvaluatorWorkingBytes(traceLength) = internedSubpatternCount × (traceLength + 1)²`，受检算术，
无表示时返回 `nullopt`。它**不属于**编译预算（`compiledBytes` 也不覆盖它），因此不得把二者互相当作承诺。

## 5. 第 5 项：门禁状态变更与"全部可接受长度"检查

**状态：incomplete。** "当前 Pattern 是否能被声明的 `maxArmElements` / `maxDeadlineElements` 容纳"这道
包含性门禁**尚未接入装配路径**；本轮只在实现与 Spec 中**如实标记**，不声称其已生效。标记位置：

- `gameplay_assembler.hpp`：`PatternArmBound` 定义处的 `INCOMPLETE GATE` 段与
  `checkPatternContainment` 声明处的 `INCOMPLETE GATE` 段；
- `gameplay_assembler.cpp`：第二半注释块中"the arm/deadline containment gate is incomplete"段；
- Spec §3.8.8 第 3 条（新增）。

**全部可接受长度检查。** `checkPatternContainment` 同时计算 `shortest` 与 `longest`，并用同一个
`exceedsCapacity` 判定**两个**容量维度；**反例**（测试 `every acceptable length has to fit, not only the
shortest one`）：`choice(atom.one, sequence(a, a, a))` 的最短匹配为 1、最长为 3。`maxArmElements = 2`
（或 `maxDeadlineElements = 2`）时**拒绝**（`judgement.s7a3.pattern.arm_bound_exceeded`，path
`pattern.maxArmElements` / `pattern.maxDeadlineElements`）；只读最短匹配的旧实现会错误放行，本测试锁死该
错误。容量为 3 时接受。对**无有限最长匹配**的 Pattern（如 `complement(atom.one)`），即使容量取
`UINT64_MAX` 也**拒绝**，而不是只按最短匹配放行。

## 6. 第 6 项：grade 的携带与求值分界

`compileMeasure` **只保存**声明里写下的 grade token 内容（不透明字符串序列）与"是否声明过表"的
`GradePresence`，并对缺席保持缺席。本轮修正的只是**注释、命名与报告措辞**：编译产物**不求值** grade——
不套默认、不聚合、不把缺席升级为任何等级、不把 token 解释成等级值。`PhaseKind::tap`、由 phase 派生的
`FactCategory` 与 grade token **仍然**进入 identity 与 semantic diff（它们是内容身份的一部分）。
grade 的**求值**不在本批次（grade 标度与聚合规则属 CM-S10 / P1-07，尚无被接受的默认值）。

## 7. S7A-3.2 扩展登记（第 7 项的补充 ① ②）

**① 溢出路径测试（测量记账）。** 新增段验证 `referenceEvaluatorWorkingBytes` 的**受检算术**：
`traceLength = 0 ⇒ 1`、`= 4 ⇒ 25`（单状态 Pattern）、`= 100 ⇒ 10201`；`traceLength = 1 << 40` 与
`SIZE_MAX` ⇒ **缺席**（`nullopt`），**不是**回绕后的数值。测试同时断言编译记账（`compiledBytes`）与运行时
工作集是**两项不同的测量**。

**② 经 identity 投影的深 Pattern。** 新增测试
`S7A-3 a deeply nested pattern is projected through identity`（`gameplay_assembler_tests.cpp`）：深度 1024 的
`sequence` 链、只改**最深叶子**的 atom 引用，两次装配都成功，且 **canonical chart / content 字节不同**、
`equivalent(...) == false`。它锁死 Risk 2：`writePatternNode`（identity 投影）此前是**递归**的，而编译遍历
是迭代的；本轮把它改为显式栈的**迭代**前序投影，并同时把 `renderPatternNode`
（`gameplay_graph.cpp`，semantic diff 读取同一棵声明树）也改为迭代，使深声明不再依赖宿主栈。

**测试测量值的使用边界。** 上述测量值与 `kStateAccountingBytes` / `kOperandAccountingBytes` /
`kPatternDfaConstruction*` 等**私有记账常数**只用于测试断言与实现内部，**不得**被提升为生产阈值
（Spec §8.3 / §8.4：预算在 S7A-9 前保持未冻结）。

## 8. 诊断码增删

> **本节按第 2 轮复审改正（见 §15.3）。** 下表是**截至本轮的净变化**（相对**本批次基线**，即 S7A-3
> 第二半 part 1 第一轮实现之前的树）。第 1 轮的两条记录保留在"第 1 轮动作"一栏作为可追溯历史；其中
> `judgement.s7a3.pattern.state_count_not_measured` 是**第 1 轮新增、本轮按 Codex 复审驳回删除**，
> 因此**净变化为 0 条新增**；`judgement.s7a3.pattern.expansion_not_representable` 是**本批次第 1 轮
> 既有、本轮按同一条驳回删除**（它的两个路径：无条件拒绝分支与记账溢出拒绝，都改成了"测量缺口 / 只对
> 已接受上限拒绝"）。

| 码 | 第 1 轮动作 | 本轮（第 2 轮复审）动作 | 净变化 | 现行类别（§9.2 九类之一） | path |
| --- | --- | --- | --- | --- | --- |
| `judgement.s7a3.pattern.repeat_operand_matches_empty` | **删除**（连带其检查点与测试段） | — | 删除 | （原为 `invalid_relation`） | `pattern.root.repeatBounds` |
| `judgement.s7a3.pattern.state_count_not_measured` | **新增** | **删除**（连带拒绝分支与测试段；Codex 复审驳回） | **0**（新增后删除） | （原为 `budget_exceeded`） | `pattern.maxStateCount` |
| `judgement.s7a3.pattern.expansion_not_representable` | 既有（未改） | **删除**（两个调用点都改判据；Codex 复审驳回） | 删除 | （原为 `budget_exceeded`） | `pattern.root`（原） |

**净账。** 相对本批次基线：诊断码**净增 0 条**、**净删 2 条**（`state_count_not_measured` 是"新增后
删除"，`expansion_not_representable` 是"既有后删除"），现行存活码为
`…pattern.budget_exceeded`、`…pattern.atom_outside_declared_arms`、`…pattern.arm_bound_exceeded`、
`…measure.category_not_derived_from_phase`、`…second_half.not_implemented`，全部**不变**。本轮**不新增**
ABI 类型、**不新增**诊断类别（仍 §9.2 九类）、**不新增** R 条目（§7.2 仍 19 条）。
**第 3 轮复审改正后仍为净增 0 条 / 净删 2 条**（该轮复用既有 `…pattern.budget_exceeded` 与既有
`pattern.maxStateCount`，不新增不删除任何码；见 §16.5）。

**码表登记口径（Codex 第 8 项，逐字）。** 见 §15.8：**本轮不改集中码表计数**；S7A-3 的 src-only 前缀
**不得**被判为**永久豁免**——任何码在**获得公共映射并对外承诺之前**必须**逐条登记**并在集中码表校验器
CTest（`cuexis_gameplay_diagnostics_codes`）中**通过**。

## 9. Spec 同步（before → after，逐字）

改动共四处：§8.2 计数口径、§3.8.4 第 5 / 6 条、新增 §3.8.8、S7A-3 节末的登记段。以下 before / after 均
为**逐字**摘录（仅省略号处为未改动上下文）。

### 9.1 §8.2 计数口径登记

**before**（该节末段，原文）：

```
计数语义要点：Pattern 状态数按**确定化并最小化之后**的状态数计，不按 NFA 规模计；
`activityPeak` 是同一 Tick 内同时活动的 Requirement 实例数峰值；候选重算数按每个规范化输入
事件重建的 Candidate 数计，不是每次 Tick 的总和。
```

**after**（原文其后追加）：

```
**Pattern 的三项计数口径（S7A-3 实现批次登记，2026-10-03；不改变本表任何一行、不冻结任何数值）。**
实现批次登记三项**互不替代**的计数，禁止互相顶替或改名上报：

1. **完全展开计数**：prepare-time 完全展开后，按声明可允许的份数求和得到的图形原语实例数；其
   **每分支上界**是 `maximum × 单份展开数`。求和无**可表示**值时记为**测量缺口**（缺席），
   不是拒绝理由。
2. **interned 子模式数**：编译内部表示中**不同**子模式的个数；结构相同的子模式共享一个条目。
   它既不等于第 1 项，也不等于第 3 项。
3. **状态数**：即上文"确定化并最小化之后"的计数，按需测出。测不出时（测量工作集上界内无法构造）
   必须报**未测量**并按既有类别 `budget_exceeded` 拒绝，**不得**以第 1 项或第 2 项代替，也**不得**
   写入替代数值。

三项计数与其上界一律使用**受检算术**：溢出即失败或记为缺席，**不得**回绕、饱和或截断。
```

### 9.2 §3.8.4 有界循环

**before**（该节原末条）：

```
4. 运行期 repeat 计数、累积器与动态 Requirement 生成不在 7A（§7.2 的 R-14）。
```

**after**：

```
4. 运行期 repeat 计数、累积器与动态 Requirement 生成不在 7A（§7.2 的 R-14）。
5. **份数序与接受集**（S7A-3 实现批次登记，2026-10-03）：完全展开的**份数序**唯一确定为**递增的份数
   下标**；接受集是可允许份数上的**并集**。**不要求每份消费元素**：操作数能匹配空轨迹时该循环仍
   **完全展开**，**不得**因此稳定拒绝；展开在位置集**不动点**处终止，不按声明上界逐份行走。
6. **静态上界**（同上）：`maximum` 是静态声明记录，**不产生**运行期计数器、累积器或动态
   Requirement（第 4 条）。取最大可表示值时同样终止；此时其完全展开计数记为**测量缺口**（§8.2 第 1
   项），不构成拒绝。
```

### 9.3 新增 §3.8.8

**before**：不存在该小节（§3.8.7 第 4 条之后直接进入 §3.9）。

**after**（新增小节，全文）：

```
#### 3.8.8 Pattern 原语语义与包含性门禁状态（S7A-3 实现批次登记，2026-10-03）

本节登记 S7A-3 实现批次（第一、二半）在第一轮实现复核中裁定的**语义澄清**与**门禁状态变更**。
provenance：Codex 话题 thread `s7a3b-implementation-review`，verdict `reject`、confidence 0.94，日期
2026-10-03，模型 `gpt-6-astra`；逐条处置与落地证据见
[S7A-3 实施裁定记录](../stage_reports/stages/stage-07/2026-10-03-s7a-3-implementation-rulings.md)。
本节的登记**不新增** ABI 类型、诊断类别、R 条目或契约计数（§7.2 仍 19 条、§9.2 仍九类、契约处置词不变）。

1. **`skip` 与 `instant` 语言相同、身份不同**：两者都接受空轨迹且不消费元素，语言完全一致；区别只在
   **声明身份**（两者不互相归一化，各自是独立的 interned 子模式）。因此"语言相同"**不得**被读成
   "同一子模式"，也**不得**据此合并二者的 identity 投影。
2. **`complement` 是前缀消费的、可组合的**：`complement(X)` 接受**所有操作数都到达不了的前缀**；
   `sequence(complement(X), Y)` 保留每一个切分点，不是一个把余下轨迹整体吞掉的后缀 / 结尾锚定读法。
   补的接受集可以无有限最长匹配，此时"无有限上界可用（未测出有限上界）"与"已证明无界"是两件事，
   报告里**只**能写前者。
3. **arm / deadline 包含性门禁状态：未完成（incomplete）。** S7A-3 的"Pattern 是否能被声明的
   `maxArmElements` / `maxDeadlineElements` 容纳"这道门禁**尚未接入装配路径**，本批次**不**声称它已生效；
   它**不**改变 §7.2 既有拒绝面，也**不**新增类别。门禁接入时必须检查**全部可接受长度**（最短与最长），
   **不得**只读最短匹配；当不存在有限最长匹配时必须**拒绝**而不是放行。
4. **grade 的携带与求值分界**：编译后的 Measure **只携带**声明里写下的 grade token（不透明内容），
   **不求值** grade——不套默认、不做聚合、不把缺席升级为某个等级。grade 的**求值**不在本批次
   （grade 标度与聚合规则见 §3.20 / CM-S10 / P1-07，仍未接受默认值）。
```

### 9.4 S7A-3 节末登记段

**before**（该节末段，原文结尾）：

```
本批次仍需按 §3.8.5 的不得消费清单与 §3.8.7 的准入门禁实施，预算数值同样不在本批次（§8.2–§8.5，S7A-9 前
保持未冻结）。
```

**after**（其后追加）：

```
**实现批次（第一、二半）的语义与门禁登记。** S7A-3 实施过程中裁定的语义澄清与门禁状态变更（`skip` /
`instant` 的语言同一与身份区分、`complement` 的前缀消费与可组合性、有界循环的份数序与可空操作数展开、
三项 Pattern 计数口径、arm / deadline 包含性门禁**未完成**、grade 只携带不求值）登记在 §3.8.4 第 5、6 条、
§3.8.8 与 §8.2，并附带日期证据
[第 1、2 半实现裁定记录](../stage_reports/stages/stage-07/2026-10-03-s7a-3-implementation-rulings.md)。
登记**不改变任何计数**（§7.2 仍 19 条、§9.2 仍九类、契约处置词与 ABI 行不变）。
```

## 10. 实现侧事实（文件与行为）

| 文件 | 本轮改动 |
| --- | --- |
| `engine/judgement/src/pattern_dfa.hpp` | 新增 `patternRepeatPairSpace` 与 `ConstructionBudget::abandon()`；`RepeatNfa` 构造先判对空间、判不下即放弃，**不 materialise 拷贝**；最小化重构为 `trim` → `mergeEquivalent`（无环 hash-consing / 有环 Moore 精细化）；`buildMinimalPatternDfa` 按使用计数释放中间自动机 |
| `engine/judgement/src/source_codes.hpp` | 删除 `kRepeatOperandEmptyCode`；新增 `kPatternStateCountNotMeasuredCode`（path `pattern.maxStateCount`） |
| `engine/judgement/include/cuexis/judgement/gameplay_assembler.hpp` | 重写 `PatternCompileBudget` / `PatternArmBound` 注释（三项计数口径、`INCOMPLETE GATE`、"ALL acceptable lengths"）；访问器改为 `declarationDepth` / `fullyExpandedCount`（`optional`）/ `largestBranchExpansion` / `internedSubpatternCount` / `stateCount`（按需测出）/ `referenceEvaluatorWorkingBytes`；`CompiledMeasure` 与 grade token 注释明确"只携带不求值" |
| `engine/judgement/src/gameplay_assembler.cpp` | `writePatternNode` 改为**迭代**前序投影（L533）；指标与受检算术（`copyCountChecked` L2262 / `addCounts` / `multiplyCounts`）；repeat 分支**不再拒绝**可空操作数；新增 `measurePatternStateCount`（L2627）与 `enforcePatternBudget`（L2676）的三行 + 两个显式维度；`PatternTraceEvaluator` 的 **complement 前缀语义**与 repeat **不动点**分支；`checkPatternContainment`（L3100）检查全部可接受长度 |
| `engine/judgement/src/gameplay_graph.cpp` | `renderPatternNode` 改为**迭代**（L256），semantic diff 读取深声明不再依赖宿主栈 |
| `tests/judgement/gameplay_pattern_compile_tests.cpp` | 头注释重写（第 2 / 3 / 5 / 7 项）；删除空操作数拒绝段；新增份数序 / 终止 / 等价段、三项计数段、未测量状态数段、运行时工作集段、全部可接受长度反例段 |
| `tests/judgement/gameplay_assembler_tests.cpp` | 新增深 Pattern 经 identity 投影段（深度 1024，只改最深叶子） |
| `tests/judgement/gameplay_measure_compile_tests.cpp` | 头注释与断言注释措辞：grade token 是携带内容、本批次不求值 |

## 11. 本轮门禁与结果（第 1 轮）

> **第 2 轮复审后的门禁结果见 §15.13（该节是本文档**现行**的门禁证据）。** 下表是第 1 轮跑过的门禁，
> 只作为该轮的历史证据保留：第 2 轮改动后已重跑全部门禁，其中 test cases / assertions 计数与部分门禁
> 命令已变（例如第 4 / 5 项顺序、以及"直接运行可执行文件"的要求）。

| # | 门禁 | 结果 |
| --- | --- | --- |
| 1 | `cmake --build --preset debug --target cuexis_judgement cuexis_judgement_tests --clean-first` | exit **0**，`: error` / `: warning` **0** 条 |
| 2 | `out\build\debug\bin\cuexis_judgement_tests.exe` | exit **0**，`All tests passed (3277 assertions in 102 test cases)`（基线 3164 / 99） |
| 3 | `ctest --preset debug -R cuexis_architecture --no-tests=error` | exit **0**，`100% tests passed, 0 tests failed out of 1` |
| 4 | `cmake --build --preset debug --target cuexis_format_check` | exit **0** |
| 5 | `ctest --preset debug -R cuexis_gameplay_diagnostics_codes --no-tests=error` | exit **0**，`100% tests passed, 0 tests failed out of 1` |
| 6 | 公开头文件 pure ASCII / `CR = 0` 扫描 | `gameplay_assembler.hpp`：`nonAscii = 0`、`CR = 0`；`src/*.hpp` 与三个测试文件同样 `nonAscii = 0`、`CR = 0`、无行尾空白、无制表符 |
| 7 | `source_codes.hpp` 的**诊断码串**在 `engine/**/include/**` 的命中数 | **0**（唯一命中的字面量是**字段路径** `gameplay.version`，出现在既有注释中，不是诊断码） |
| 8 | `python -B tools/check_docs.py` 与 `git diff --check` | 前者 exit **0**（`341 Markdown files and 20 candidate JSON/CXT files validated`）；后者 exit **0** |

## 12. 门禁原始输出（第 1 轮）

第 1 轮门禁的**命令、原始输出与真实退出码**逐字记录在该轮实现报告“S7A-3 第二半 part 1 修订轮报告”
第 ⑧ 节；未在该节列出的门禁视为未运行。本节不重复粘贴原始日志。**第 2 轮**的门禁命令、原始输出与真实
退出码逐字记录在 **§15.13**（含 clean-first 构建、直接运行 `cuexis_judgement_tests.exe`、两个 CTest、
格式检查、文档检查与空白检查，以及构建目录依赖扫描修复的环境说明）。

## 13. 建议索引行（逐字，供 `docs/stage_reports/README.md` 插入）

> **本节已被 §15.15 取代。** 下面这行是第 1 轮的建议文本，它仍引用了本轮**已删除**的
> `state_count_not_measured` 口径；维护者应改用 §15.15 的更新版。

本节给出**建议的索引行文本**，由索引维护者插入 `docs/stage_reports/README.md` 的 Stage 7A 列表中
（本文档**不**自行修改该索引）。插入位置：紧接 `2026-10-03-s7a-2-input-implementation-rulings.md`
一行之后。文本（逐字，含前导 `  ` 缩进、结尾逗号与换行）：

```
  [S7A-3 第二半 part 1 实现裁定落地记录（第一轮实现复核修订）](stages/stage-07/2026-10-03-s7a-3-implementation-rulings.md)（2026-10-03，复核 verdict `reject` 的 8 项逐条处置：空操作数一律拒绝被**驳回**并改为份数序 / 并集 / 不动点语义、三项 Pattern 计数口径（完全展开 / interned / 确定化并最小化后状态数）、`skip` 与 `instant` 的语言同一与身份区分、`complement` 的前缀消费语义、arm / deadline 包含性门禁标记为**未完成**、grade 只携带不求值、诊断码一删一增、Spec §3.8.4 / §3.8.8 / §8.2 同步与门禁结果；**只登记语义与门禁状态，不表示 S7A-3 或 Stage 7A 实现完成**）、
```

## 14. 不变量声明

1. 本轮**不改变**任何计数：§7.2 稳定拒绝清单仍 **19** 条；§9.2 类别仍 **九**类；契约处置词不变；
   [ABI](../../../api/GAMEPLAY_V2_ABI.md) 仍 **9** 域 **126 = 96 + 30** 行，且**不新增**类型行。
2. 本轮**不新增** R 条目、**不新增**诊断类别；诊断码的**净变化**见第 8 节（相对本批次基线**净增 0 条**、
   **净删 2 条**；第 1 轮曾记的一条"新增"已在本轮按 Codex 复审驳回删除）。
3. 本轮**不新增** ABI 类型、**不改** `docs/api/GAMEPLAY_V2_ABI.md`、`schemas/**`、`tools/**`、
   根 / `engine` / `tests` 的 `CMakeLists.txt`、`.gitignore`、`AGENTS.md`、`docs/proposals/**` 既有计数与
   表格。
4. 门禁状态**不因本文档而改变**：arm / deadline 包含性门禁仍为 **incomplete**；S7A-3 第二半第三部分仍
   未实现（`judgement.s7a3.second_half.not_implemented` 仍由两个入口返回）；预算数值在 S7A-9 前保持
   **未冻结**。

## 15. 第 2 轮实现复审（verdict `reject`、confidence 0.91）的改正与本轮门禁

本节是第 2 轮复审的落地正文。§1 至 §14 是第 1 轮正文；与之冲突的两处口径已在 §4 与 §8 **就地改正**并
标注历史，其余处不改。本节**不表示** S7A-3 或 Stage 7A 实现完成。

### 15.1 provenance（第 2 轮）

| 项 | 值 |
| --- | --- |
| thread | `01a0fe91-416c-7f31-a48a-d1890a0e68d1` |
| verdict | `reject` |
| confidence | **0.91** |
| 模型 | `gpt-6-astra` |
| 裁决卡文件名时间戳 | `2026-10-02T22-33-54-872Z`（UTC；按第 2 至第 7 轮先例登记为**本地日期 2026-10-03**） |
| 结论口径（原文） | "暂不接受 S7A-3 第二半 part 1：**第 3–5 项的测量失败和数值溢出仍会形成未经接受的内容拒绝门禁**。" |

第 1 轮卡（`s7a3b-implementation-review`，`reject`，0.94）的 provenance 与逐条处置保留在 §1 至 §14，
**不回改**；本节的改写只针对被第 2 轮明确驳回的口径。

### 15.2 九项逐条处置

| 项 | Codex 裁定 | 处置 | 落点 |
| --- | --- | --- | --- |
| 1 | accept | `skip` / `instant` 语言相同、**身份不同**的现行口径不变 | §3、Spec §3.8.8 第 1 条 |
| 2 | accept | **不为** `maximumTraceLength` 增加 ABI 文本 | `docs/api/GAMEPLAY_V2_ABI.md` **未改**（本轮 0 行） |
| 3 | **reject** | 删除"状态数测不出 ⇒ `budget_exceeded`"：删码、删拒绝分支、删测试段的拒绝断言；**状态预算门禁保持 incomplete** | §15.3、§15.5 |
| 4 | **reject** | 补测 `repeat(skip,1,UINT64_MAX)`（`maxStateCount = 1` 时**接受**），并**实测** `repeat(atom,1,UINT64_MAX)` 的最小状态数 | §15.6（测试 `a repeat of the empty word has one state and is not a gap`、`the state count of a bounded repeat grows with its declared bound`） |
| 5 | **reject** | 删除"溢出 ⇒ 无条件拒绝"：溢出=**测量缺口**；只有**已接受**上限被**确实超过**才拒绝；实现**相对预算早停比较** | §15.3、§15.4 |
| 6 | **revise** | **装配消费点**是 S7A-3 验收 / 进入 S7A-4 的**硬前置**（**本半批未实现**，只登记） | §15.7、Spec §3.8.8 第 6 条 |
| 7 | accept | `renderPatternNode` 迭代化、深身份测试保留 | §7、§15.10 |
| 8 | **revise** | **不得**有"前缀永久豁免于登记"；任何码获得**公共映射**前必须**逐条登记**并通过集中码表校验器 CTest | §15.8（耐久文本） |
| 9 | accept | **不**新增第 21 条 pending 条目；`ruleset.*` 保持 `open` | §15.10 |

### 15.3 被驳回的映射与删除点（Codex 第 3、5 项）

**删除 1：状态数缺口的拒绝映射。** `engine/judgement/src/source_codes.hpp` 的
`judgement.s7a3.pattern.state_count_not_measured` 常量**已删除**；`gameplay_assembler.cpp` 的
`enforcePatternBudget` 状态分支不再在缺口时拒绝。现行行为：

| 情形 | 现行行为 |
| --- | --- |
| 状态数**测得出来**且 `> maxStateCount` | `judgement.s7a3.pattern.budget_exceeded`（`budget_exceeded`，path `pattern.maxStateCount`） |
| 状态数**测不出来**（测量工作集内构造不出） | 访问器 `stateCount()` 返回**缺席**；编译**成功**；该维度**未enforced**，门禁记 **incomplete** |

**删除 2：无表示计数的无条件拒绝。** `judgement.s7a3.pattern.expansion_not_representable` 常量**已删除**，
其**两个**调用点都改成按维度判据（`pattern.root` 的无条件拒绝分支、记账溢出的 `accountingOverflow`
分支）。现行行为见 §15.4。

**字段与语义改名（`CompiledMetrics` / `CompiledPatternStorage`）。**

| 旧 | 新 | 含义 |
| --- | --- | --- |
| `maximumBoundUnavailable` | `maximumLengthUnavailable` | 最长匹配测不出（含无有限上界） |
| `branchOverflow` | `largestBranchGap` | 每分支上界无可表示值 |
| `fullyExpandedOverflow` | `fullyExpandedGap` | 完全展开计数无可表示值 |
| —（新增） | `minimumLengthGap` | 最短匹配无可表示值 |
| `largestBranchExpansion` / `evaluationSteps` / `compiledBytes` / `minimumTraceLength` 返回 `std::uint64_t` | 同名的四个访问器返回 `std::optional<std::uint64_t>` | 缺席 = 该计数**没有可表示值**（**不是**拒绝） |

`addCounts` / `multiplyCounts` 的参数改为**带 gap 标志**的语义；`multiplyCounts` 额外把**零因子**排除在
gap 传播之外（见 §15.4 第 3 例）。

### 15.4 溢出、缺口与拒绝的三分判据（Codex 第 5 项）

**判据（现行）。** 计数一律**受检算术**，**不回绕、不饱和、不截断**。计数**没有可表示值**时记该维度的
**测量缺口**（缺席）：

| 情形 | 计数是否为**经证明的下界** | 该维度是否有**已接受上限** | 现行判决 | 锁定它的测试 |
| --- | --- | --- | --- | --- |
| 溢出 / 缺口 | 是（精确受检算术溢出、或单调累加溢出 ⇒ 真值 ≥ 2^64） | **有**（必有 `uint64` 表示，故必然小于真值） | **确实超限 ⇒ `budget_exceeded` 拒绝**（稳定拒绝） | `an accepted bound plus a proven lower bound: a real overrun is refused` |
| 溢出 / 缺口 | 是 | **无** | **只记缺口，不拒绝**（编译成功、计数缺席） | `no accepted bound: the count is a measurement gap and nothing is refused` |
| 缺口 | **不是**（可被零份数乘掉） | 有 | **只能记缺口，不得据此拒绝**；乘积精确为 0 | `an accepted bound is not exceeded by a count that only looks large` |

**相对预算的早停比较。** `steps`（编译工作计数）与 `bytes`（记账大小）都**单调增长**，因此对**已接受**
上限可以边算边比：一旦可判定超过上限即停止并以该维度的 `budget_exceeded` 拒绝（`earlyStopAgainst`，
`maxEvaluationSteps` / `maxCompiledBytes` 各一处）。**没有已接受上限时不比较、不拒绝**；缺口 + 已接受
上限 = 确定超限（真值 ≥ 2^64 > 上限）。

**为什么不给 `fullyExpanded` 做早停。** 完全展开计数**非单调**：`repeat(X, 0, 0)` 会把操作数的计数乘 0，
中间值可能大于根值。因此它**不**做早停，只在根上与已接受上限比较一次。**次序后果**（必须如实登记）：
一个同时超过多个维度的内容现在可能**先**报 `pattern.maxEvaluationSteps` / `pattern.maxCompiledBytes`
（遍历先后决定），而不是先报状态数或展开计数；各维度单独判定的测试仍逐维锁死。

**零因子修正。** `multiplyCounts` 在左右任一因子**精确为 0**（且该因子本身无 gap）时直接返回 0 并**不**
传播对方的 gap：`repeat(<无表示计数的子模式>, 0, 0)` 的完全展开计数、每分支上界、最短/最长匹配长度都
是 **0**，即使 `maxExpansionCount = 0` 也**接受**。这是"计数不是经证明的下界时不得据以拒绝"的落地。

### 15.5 状态预算门禁状态（Codex 第 3 项，耐久文本）

> **本节按第 3 轮复审改正（见 §16.3–§16.4、§16.8）。** "测不出时只报缺口，该维度不 enforced"仍然成立，
> 但**不再**意味着"无从比较"：计数缺席而**可证明为下界**且该下界超过**已接受**上限时，该维度**按
> `budget_exceeded` 拒绝**。门禁状态本身仍为 **incomplete**。

**状态预算门禁保持 incomplete。** 本批次**不**声称 `maxStateCount` 已验证、**不**声称任何内容"已证明在
状态预算内"：状态数测不出时只报缺口，该维度不 enforced。耐久文本位置：
`gameplay_assembler.hpp` 的 `maxStateCount` 字段注释段（`INCOMPLETE GATE`，约 L598–L619）、
`gameplay_assembler.cpp` 的 `enforcePatternBudget` 状态分支注释、Spec §3.8.8 第 5 条与 §8.2 第 3 项。

### 15.5b 第 3 轮复审对该门禁的改正（见 §16）

| 项 | 第 2 轮口径（本节上表与 §15.4） | 第 3 轮改正后的现行口径 |
| --- | --- | --- |
| 缺口 + 已接受上限 | 缺口即"无从比较"⇒ **接受** | 缺口若**可证明为下界**且下界 > 上限 ⇒ **`budget_exceeded` 拒绝**；否则仍记缺口 |
| `repeat(atom.one, 1, UINT64_MAX)` 配 `measured(1)` | **接受**（缺口） | **拒绝**（`2^64` 是可证明下界；**上一轮按缺口放行，经 Codex 第 3 轮复审驳回**） |
| 构造失败 | 只记缺口 | 只记缺口（**不**因构造上界拒绝）；若声明推出的下界已超上限则按上行拒绝 |
| 无已接受上限 | 不比较不拒绝 | **不变**：不比较不拒绝，门禁 incomplete |
| 计数的码与 path | — | 复用既有 `judgement.s7a3.pattern.budget_exceeded` / `budget_exceeded` / `pattern.maxStateCount`，**不新增码** |

### 15.6 第 4 项实测：最小状态数的两种独立测量

| 声明 | 模块计数（`stateCount()`） | 独立测量（非空残差类计数） | 结论 |
| --- | --- | --- | --- |
| `repeat(skip, 1, UINT64_MAX)` | **1**（本轮新增精确 `{ε}` 化简后可测出；改前报缺口） | 1（窗口 3） | 与 Codex 断言**一致**：空词的重复只有一个状态；`maxStateCount = 1` 时**接受** |
| `repeat(atom.one, 1, M)`，M = 1,2,3,4 | 2, 3, 4, 5 | 2, 3, 4, 5（窗口 4） | 两侧一致，状态数随声明上界线性增长 |
| `repeat(atom.one, 1, M)`，M = 8 | 9 | 残差窗口在 2 处饱和（**该独立测量的窗口限制**，非模块缺口） | 模块侧仍为 9 |
| `repeat(atom.one, 1, UINT64_MAX)` | **2^64**（即 `UINT64_MAX + 1`，**无 `uint64` 表示**）⇒ 报**缺席** | —（无法构造） | 与 Codex"并非 2"**一致**：本批次**未**找到任何把该值报成 2 的落点（改前的测试也已断言缺席），故**不需要**"与 Codex 断言不符"的保留意见 |

**测量方法。** 模块侧：`detail::buildMinimalPatternDfa`（子集构造 → trim → 等价合并 → 最小化），受模块
私有工作集上界约束（超出即**放弃测量**并报缺席，不拒绝）。独立侧：`distinctNonEmptyResiduals`——按
Myhill–Nerode 意义枚举长度 ≤ 窗口的非空词的**残差集合**个数，与模块实现**不共享代码路径**。
对空词语言的精确化简：`pattern_dfa.hpp` 的 `isExactEmptyWordLanguage` + `repeatOf` 的 `{ε}` 短路
（约 L916、L961）。

### 15.7 第 6 项：装配消费点是硬前置（Codex 第 6 项，耐久文本）

**逐字要求。** 包含性门禁的**装配消费点**是 S7A-3 实现验收与**进入 S7A-4** 之前的 **HARD PREREQUISITE**：
在**真实**的 arm / deadline 容量下调用该门禁，并证明失败是**原子**的（不发布半个 prepared chart、不改变
active session）。**本半批不实现**该消费点（§5 的 `incomplete` 状态因此仍成立）。

**准入表述（耐久文本）。** **"在装配消费点落地并测试之前，不得验收 S7A-3，不得进入 S7A-4。"** 该句同时
写入 Spec §3.8.8 第 6 条；`gameplay_assembler.hpp` 的 `PatternArmBound` / `checkPatternContainment`
两处 `INCOMPLETE GATE` 注释（约 L628、L785）指向同一前置。

### 15.8 第 8 项：码表登记口径（Codex 第 8 项，耐久文本）

**逐字要求。** **"S7A-3 的 src-only 前缀不得被判为永久豁免；任何码在获得公共映射之前必须逐条登记，并在
集中码表校验器 CTest 中通过。"**

- **现行处置**：`judgement.s7a3.*` 仍是**模块内部前缀**（无 ABI / schema / 对外承诺映射），集中码表
  （`schemas/**` 与 `docs/api/GAMEPLAY_JUDGEMENT_ABI.md` 的码表）**本轮 0 改动**，因此**计数不变**。
- **后续批次的登记义务**：任何批次一旦让某个 `judgement.s7a3.*` 码进入**公共映射**（ABI 文本、schema、
  对外契约、trace 或诊断的对外承诺），该批次**必须**：① 在其自身记录中**逐码登记**（码、类别、
  severity / faulted、path、首次消费批次）；② 使该码在集中码表校验器 CTest
  `cuexis_gameplay_diagnostics_codes` 中**通过**（本轮实测该门禁 exit 0，见 §15.13 第 4 项）。
- **首次消费批次口径**：码的"首次消费批次"栏以**首次把它写进公共映射的批次**为准（而不是首次在
  `src/` 里抛出的批次）；本轮的 5 个存活码仍无公共映射，故该栏仍为"未消费"。
- **不得豁免**：不存在"S7A-3 已结束所以此后免登记"的类别；前缀不构成豁免依据。

### 15.9 第 7 项：两条风险登记（Codex 第 7 项）

> **Risk ①（声明树身份投影与完全展开语义）**：声明的身份投影目前仍按**原始** `repeat` 上下界写出，
> 而 Spec §3.8.4 要求的是**完全展开**语义。**装配在消费该投影之前必须证明**：快照与事实序与 §3.8.4 的
> 完全展开一致（份数序、并集语义、不动点语义），否则投影与实际语言不符。
>
> **Risk ②（无有限上界时的保守拒绝）**：`complement` 没有有限最长匹配时，包含性检查采用**保守拒绝**
> （即使容量取 `UINT64_MAX` 也拒绝）。该行为是本门禁**当前的可观察行为**，在装配消费点（§15.7）落地
> **之前**必须**修正并测试**：要么给出可证明的包含性判据，要么把该拒绝明确登记为合同行为并逐条锁定。

两条风险同时写入 Spec §3.8.8（第 3 条第 3 句与 §3.8.4 的既有展开语义段）与本记录；它们是**未决项**，
不改变 §7.2 的 19 条拒绝面。

### 15.10 第 1、2、7、9 项的接受项确认

1. **第 1 项**：`skip` / `instant` 的"语言同一、身份不同"口径**未改**；两者的语言同一测试与身份区分
   测试（分别构造、`internedSubpatternCount` 各自为 1、`sequence(skip, instant)` 为 3 个 interned
   子模式）仍通过。
2. **第 2 项**：`maximumTraceLength` **没有** ABI 文本；`docs/api/GAMEPLAY_V2_ABI.md` 本轮**0 改动**。
3. **第 7 项**：`renderPatternNode` 的**迭代**实现与深身份测试（嵌套深度用例）保留且通过。
4. **第 9 项**：**不**新增第 21 条 pending 条目；`ruleset.*` 仍为 `open`（§15.14 计数不变）。

### 15.11 Spec 同步（前 → 后，逐字）

| 位置 | 改动前 | 改动后 |
| --- | --- | --- |
| §8.2 第 3 项（状态数） | "测不出时……必须报**未测量**并按既有类别 `budget_exceeded` **拒绝**" | "记为**测量缺口**（缺席）……该缺口使**状态预算门禁保持 incomplete**：它**不构成**对内容的拒绝" |
| §8.2 末段（受检算术） | "溢出即失败或记为缺席"（未区分拒绝与缺口） | 新增"**超范围计数与拒绝的界分**"三条：溢出 = 该维度**测量缺口**；**只有已接受上限被确实超过才拒绝**（含"是下界 + 有上限 ⇒ 拒绝""无上限 ⇒ 不拒绝""不是下界 ⇒ 不得据此拒绝"三种情形）；**相对预算的早停比较**；零因子不得传播缺口 |
| §3.8.8 provenance | 只登记第 1 轮卡（0.94） | 增加第 2 轮卡（0.91、`2026-10-02T22-33-54-872Z`）与"改正了第 5、6 条与 §8.2 判据" |
| §3.8.8 第 3 条 | 只写"无有限最长匹配 ⇒ 拒绝" | 增加"**任一端无可表示值（测量缺口）时同样保守拒绝**"与"接入前必须固定并测试"（= Risk ②） |
| §3.8.8 第 5 条（新增） | — | 计数缺口不是拒绝理由；状态预算门禁保持 incomplete；内部构造上界**不得**充当内容阈值 |
| §3.8.8 第 6 条（新增） | — | 装配消费点是 S7A-3 验收与进入 S7A-4 的**硬前置**（§15.7） |
| 本节 §4、§8、§14 第 2 条 | 旧口径（未测量 ⇒ 拒绝；"一删一增"） | 就地改为现行口径并保留"上一轮曾如此、本轮按 Codex 复审改正"的可追溯说明 |

### 15.12 本轮实现落点（文件:行，均为本工作树内文件）

> 本节记录**第 2 轮**的落点行号；第 3 轮复审改正后的落点行号见 §16.6。行号会随后续改动漂移，故只作
> 定位辅助。

| 文件 | 落点 |
| --- | --- |
| `engine/judgement/src/gameplay_assembler.cpp` | `multiplyCounts` 零因子（约 L2315–L2325）；`earlyStopAgainst`（约 L2335）；`fullyExpandedGap` / 各原语指标（约 L2363–L2582）；根计数（约 L2639）；bytes 早停（约 L2673）；`enforcePatternBudget` 状态分支（约 L2778）；四个访问器签名（约 L3099、L3118、L3122、L3146）；`checkPatternContainment` 的双端缺失保守拒绝（约 L3200） |
| `engine/judgement/src/source_codes.hpp` | 删除 `state_count_not_measured` 与 `expansion_not_representable` 两个常量与旧策略注释；改写为"缺口 / 已接受上限"策略段（约 L344–L360） |
| `engine/judgement/include/cuexis/judgement/gameplay_assembler.hpp` | `maxStateCount` 的 `INCOMPLETE GATE` 段（L600–L606）；`maxEvaluationSteps` / `maxCompiledBytes` 早停语义（L608–L615）；四个访问器改为 `std::optional<std::uint64_t>` 并更新注释（L699、L716、L722、L740–L741）；`PatternArmBound` / `checkPatternContainment` 的 `INCOMPLETE GATE`（L628、L785） |
| `engine/judgement/src/pattern_dfa.hpp` | `isExactEmptyWordLanguage`（L916）与 `repeatOf` 的 `{ε}` 短路（L961）；第 3 轮仅改注释四处（见 §16.6） |
| `tests/judgement/gameplay_pattern_compile_tests.cpp` | `distinctNonEmptyResiduals`（L562）；`a repeat of the empty word has one state and is not a gap`（L597）；`the state count of a bounded repeat grows with its declared bound`（L635）；`a state-count measurement gap never refuses content`（L670，**第 3 轮改判并改名为 `a state-count measurement gap refuses only a proven overrun`**，见 §16.7）；`a count with no representable value is a gap, an overrun, or neither`（L858，三个 SECTION 对应 §15.4 的三行；第 3 轮新增状态维度同款断言） |
| `docs/formats/GAMEPLAY_V2_SPEC.md` | §3.8.8（provenance、第 3 条、第 5、6 条）与 §8.2（三项计数、超范围判据）；第 3 轮再改见 §16.8 |
| 本记录 | §4、§8、§14 第 2 条就地改正；§1 增加第 2 轮 provenance；本节 §15；第 3 轮见 §16 |

### 15.13 本轮门禁（命令、原始输出、真实退出码）

**① 构建（clean-first 为必需）**

> 本节保留**第 2 轮**的原始输出，不回改；**第 3 轮复审改正后的门禁数字见 §16.9**（第 2 轮的
> `All tests passed (3335 assertions in 104 test cases)` 是第 3 轮的改动前基线）。

```text
cmake --build --preset debug --target cuexis_judgement cuexis_judgement_tests --clean-first
[1/2] Cleaning all built files...
Cleaning... 29 files.
...
[27/29] Building CXX object tests\judgement\CMakeFiles\cuexis_judgement_tests.dir\gameplay_pattern_compile_tests.cpp.obj
[28/29] Linking CXX executable bin\cuexis_judgement_tests.exe
exit 0（`: error` 0 行、`: warning` 0 行）
```

**② judgement 测试（直接运行可执行文件，不用 `ctest -R`）**

```text
out\build\debug\bin\cuexis_judgement_tests.exe
Randomness seeded to: 2594964266
===============================================================================
All tests passed (3335 assertions in 104 test cases)
exit 0
```

改动前基线：`All tests passed (3277 assertions in 102 test cases)`；本轮 **+58 assertions / +2 test cases**。

**③ 架构 CTest**

```text
ctest --preset debug -R cuexis_architecture --no-tests=error
1/1 Test #1: cuexis_architecture_tests ........   Passed    0.88 sec
100% tests passed, 0 tests failed out of 1
exit 0
```

**④ 集中码表校验器 CTest（Codex 第 8 项要求它必须通过）**

```text
ctest --preset debug -R cuexis_gameplay_diagnostics_codes --no-tests=error
1/1 Test #2: cuexis_gameplay_diagnostics_codes ...   Passed    0.33 sec
100% tests passed, 0 tests failed out of 1
exit 0
```

**⑤ 格式检查**

```text
cmake --build --preset debug --target cuexis_format_check
[1/2] CMakeFiles\cuexis_format_check-3e5f1bf.bat e4b58b20f698a638
exit 0
```

（首次运行 exit 1：`gameplay_pattern_compile_tests.cpp` 55 处、`gameplay_assembler.cpp` 30 处、
`pattern_dfa.hpp` 6 处 `-Wclang-format-violations`；已用仓库 `.clang-format` 对这 3 个文件 `clang-format -i`
后复跑为 exit 0。）

**⑥ 文档检查**

```text
python -B tools/check_docs.py
Documentation checks passed: 341 Markdown files and 20 candidate JSON/CXT files validated.
exit 0
```

**⑦ 空白检查**

```text
git diff --check
（无输出；stderr 只有工作树中 15 个与本轮无关的既有文件的 `LF will be replaced by CRLF` 警告）
exit 0
```

**⑦′ 未跟踪文件的补充空白检查（因为 `git diff --check` 看不到 untracked 文件）。** 本轮改动的
`engine/judgement/**`、`tests/judgement/**`、`docs/formats/GAMEPLAY_V2_SPEC.md` 与本记录在 `git status
--short` 中仍是 `??`（untracked），故另以脚本逐字节检查 **CR / 行尾空白 / 文件末换行 / 公共头 ASCII**：

```text
engine/judgement/include/cuexis/judgement/gameplay_assembler.hpp  CR=0 nonASCII=0 trailingWS=0 finalLF=True
engine/judgement/src/gameplay_assembler.cpp                       CR=0 nonASCII=0 trailingWS=0 finalLF=True
engine/judgement/src/source_codes.hpp                             CR=0 nonASCII=0 trailingWS=0 finalLF=True
engine/judgement/src/pattern_dfa.hpp                              CR=0 nonASCII=0 trailingWS=0 finalLF=True
tests/judgement/gameplay_pattern_compile_tests.cpp                CR=0 nonASCII=0 trailingWS=0 finalLF=True
docs/formats/GAMEPLAY_V2_SPEC.md                                  CR=0 nonASCII=132871 trailingWS=0 finalLF=True
docs/stage_reports/.../2026-10-03-s7a-3-implementation-rulings.md CR=0 nonASCII=30910  trailingWS=0 finalLF=True
```

（公共头 `gameplay_assembler.hpp` 为**纯 ASCII**、无 CR、无行尾空白、有末换行，符合安装头约束。）

**⑧ 构建目录的依赖扫描修复（环境说明，供复现者参考）。** 本机 MSVC 的 `/showIncludes` 前缀在
`out/build/debug/CMakeFiles/rules.ninja` 中曾被存成 GBK 双重编码的乱码（`E5 A8 89 …`，而 cl 实际输出
`E6 B3 A8 …`），导致 Ninja 静默丢弃全部头文件依赖、**改私有头不触发重编译**。根因是
`CMakeFiles/4.3.3/CMakeCXXCompiler.cmake` 里缓存的 `CMAKE_CXX_CL_SHOWINCLUDES_PREFIX`；在
`chcp 65001` 下删除该文件并重新配置后已恢复正确（`.ninja_deps` 现记录 `pattern_dfa.hpp` /
`source_codes.hpp` / `gameplay_assembler.hpp`）。**本轮的 clean-first 构建因此是必需的，不是保守**；
此后该构建目录的增量依赖可信。

### 15.14 不变量声明（本轮）

1. **不改变任何计数**：§7.2 稳定拒绝清单仍 **19** 条；§9.2 类别仍 **九**类；契约处置词不变；
   [ABI](../../../api/GAMEPLAY_V2_ABI.md) 仍 **9** 域 **126 = 96 + 30** 行（本轮 0 行改动）。
2. **诊断码净变化**：相对本批次基线**净增 0 条**、**净删 2 条**（§8；第 3 轮复审改正后**不变**，见 §16.5）。
3. **不新增** R 条目、**不新增**诊断类别、**不新增** ABI 类型；不改 `docs/api/**`、`schemas/**`、
   `tools/**`、`docs/proposals/**`、`docs/stage_reports/README.md`、任何 `CMakeLists.txt`、`cmake/**`、
   `.gitignore`、`AGENTS.md`。
4. **门禁状态不因本文档而改变**：arm / deadline 包含性门禁仍 **incomplete** 且装配消费点是**硬前置**；
   状态预算门禁仍 **incomplete**（第 3 轮**只**恢复"可证明下界超上限 ⇒ 拒绝"，未关闭门禁，见 §16.11 第 1
   条）；S7A-3 第二半第三部分仍未实现；预算数值在 S7A-9 前仍**未冻结**。
5. **未提交**：本轮**不**执行 `git add` / `git commit`；工作树保持未提交状态。

### 15.15 建议索引行（更新版，供 `docs/stage_reports/README.md` 维护者使用）

第 1 轮的建议索引行（§13）已失效：它仍引用被删除的 `state_count_not_measured` 口径。**维护者**应把它替换
为**§16.13 的那一行**（它已把第 3 轮复审的改正写进索引摘要；本记录**不**自行修改索引）。下面保留 §15
当时给出的版本作为历史：

```
  [S7A-3 第二半 part 1 实现裁定落地记录（两轮实现复核修订）](stages/stage-07/2026-10-03-s7a-3-implementation-rulings.md)（2026-10-03，第 1 轮 verdict `reject` 0.94 与第 2 轮 verdict `reject` 0.91 的逐条处置：空操作数一律拒绝被**驳回**并改为份数序 / 并集 / 不动点语义、三项 Pattern 计数口径（完全展开 / interned / 确定化并最小化后状态数）且**计数缺口不是拒绝理由**（只有已接受上限被确实超过才 `budget_exceeded`）、`skip` 与 `instant` 的语言同一与身份区分、`complement` 的前缀消费语义、arm / deadline 包含性门禁**未完成**且装配消费点是进入 S7A-4 的**硬前置**、状态预算门禁保持 **incomplete**、码表登记不得永久豁免、grade 只携带不求值、诊断码净增 0 / 净删 2、Spec §3.8.4 / §3.8.8 / §8.2 同步、两条已登记风险与七项门禁结果；**只登记语义与门禁状态，不表示 S7A-3 或 Stage 7A 实现完成**）、
```

## 16. 第 3 轮实现复审（verdict `reject`、confidence 0.93）的改正与本轮门禁

本节是第 3 轮复审的落地正文。§1 至 §14 是第 1 轮正文，§15 是第 2 轮正文；被第 3 轮**改判**的口径以
**本节为准**（§15.3 的"状态数缺口 ⇒ 不拒绝"表、§15.4 的三分表、§15.5 与 §15.14 第 4 条的门禁表述、
本记录 §4 / §8 的相应句子都在本节就地改正，历史原文保留不回改）。本节**不表示** S7A-3 或 Stage 7A
实现完成。

### 16.1 provenance（第 3 轮）

| 项 | 值 |
| --- | --- |
| thread | `01a0fe91-416c-7f31-a48a-d1890a0e68d1`（与第 2 轮**同一话题**） |
| **Codex 会话** | **`01a1006a-d7ca-7300-b6d9-67dedf427789`** |
| verdict | `reject` |
| confidence | **0.93** |
| 模型 | `gpt-6-astra` |
| 裁决卡文件名 | `2026-10-03T06-20-24-581Z-codex-复审载荷-s7a-3-part-1-修订轮-两个待裁定点-主控底稿-发送方式-nod.md` |
| 裁决卡时间戳 | `2026-10-03T06:20:24.581Z`（UTC；按第 2 至第 7 轮先例登记为**本地日期 2026-10-03**） |
| 模式 | `consult`（主控底稿） |
| 需要补充的信息 | （无） |

**两条局限（如实登记，必须与结论一起读）。**

| # | 局限 | 事实与影响 |
| --- | --- | --- |
| ① | `resumed: false` | 本卡是**新建 Codex 会话**（`01a1006a-d7ca-7300-b6d9-67dedf427789`），**不是**续接第 2 轮的会话；其裁定依据靠**主控载荷内联**的第 2 轮逐字原文与代码取证，会话上下文不共享。 |
| ② | `toolCount: 0` | Codex **未独立读取工作区、未复跑任何门禁**。卡内「风险」逐字自述："本卡仅依据提问中提供的实现、Spec 摘要和测试结果补答，未独立读取工作区或复跑验证。"故本卡结论有权威、**事实基础为主控载荷**；本轮门禁由主控独立复跑，不复用实现者自述。 |

**裁决（verbatim）。**

> 驳回当前修订：必须区分可证明超限的算术溢出与不可证明的构造失败，并把包含性保守拒绝与预算缺口分成独立门禁。

**理由（verbatim）。**

> 题载证据明确要求只有已接受上限被确实超过时才按 budget_exceeded 拒绝；repeat(atom,1,UINT64_MAX) 的数学状态数为 2^64，且被描述为可证明下界，因此不能与构造失败一并按缺口放行。相邻 Spec 条目和代码现状同时表明包含性门禁对最长匹配缺席采用保守拒绝，而预算比较仅在有 accepted measuredValue 时进行，故应分离为独立的 gate_incomplete 与内容超限规则。

**建议动作（verbatim，三条）。**

> - 将"可证明下界已超过已接受上限"的算术溢出路径恢复为 budget_exceeded 内容拒绝；仅构造失败或非下界计数记为测量缺口，不得拒绝。
> - 在 Spec 中明确：缺席/非下界=测量缺口且不构成预算拒绝；已测得或可证明下界超过已接受上限=budget_exceeded。
> - 将包含性门禁单独定义为 gate_incomplete 的保守拒绝，明确其不是内容超限诊断，也不改变状态预算门禁的 incomplete 记录；补齐装配消费点前不得验收 S7A-3。

（判定 + 理由 + 三条建议动作 = 卡内**四段权威指令**，逐字登记如上。）

### 16.2 本轮边界（只做第 1、2 条）

| 条 | 本轮处置 |
| --- | --- |
| 第 1 条（恢复条件性拒绝） | **本轮落地**：实现 + 测试（§16.3–§16.7） |
| 第 2 条（Spec 措辞） | **本轮落地**：Spec §8.2 / §3.8.8 / §3.8.4（§16.8） |
| 第 3 条（包含性门禁 = `gate_incomplete` 保守拒绝） | **本轮不改代码**：`checkPatternContainment` 的返回契约、错误码与 `PatternArmBound` **保持现状**（公开头**签名 0 改动**）；只在 Spec 把"两道独立门禁"写清（§3.8.8 第 3 条新增段、§8.2 界分第 5 句）。其**表示形式**属下一批，本批不预设。 |
| 不做 | 不新增 / 不删除诊断码；不改 ABI、`schemas/**`、`tools/**`、任何 `CMakeLists.txt`、`cmake/**`、`.gitignore`、`AGENTS.md`、`docs/stage_reports/README.md`；不 `git add` / 不 `git commit`。 |

### 16.3 三分判据（现行，逐档落地）

判据是"**该计数是否可证明为下界**"，**不是**"是否溢出"，且 ①② 都以"**有已接受上限**"为前提。

| 档 | 情形 | 现行处置 | 落点 | 锁定它的测试 |
| --- | --- | --- | --- | --- |
| ① | **有**已接受上限，计数**已测得** > 上限 | `budget_exceeded` 拒绝 | `enforcePatternBudget`（`measured.value` 分支） | `a measured count above the accepted bound is still a budget overrun` |
| ② | **有**已接受上限，计数**缺席但可证明为下界**（含"下界 ≥ 2^64"）且 > 上限 | `budget_exceeded` **拒绝**（本轮恢复的路径） | 同上（`provenLowerBound` / `provenLowerBoundNotRepresentable` 分支） | `a measured state bound plus a proven lower bound: a real overrun is refused`、`the widest representable accepted bound is exceeded as well`、`an accepted bound plus a proven lower bound: a real overrun is refused`（完全展开维度） |
| ③ | **无**已接受上限 | 只记缺口，**不比较、不拒绝**（门禁 incomplete） | 所有分支都以 `measuredValue() != nullptr` 为闸门 | `no accepted bound: the count is a measurement gap and nothing is refused`、`a pending dimension is reported and never enforced with a substituted number` |
| ④ | 计数**不是**下界（零份数乘 0） | 只记缺口，**不得**拒绝 | `multiplyCounts` 零因子规则 | `an accepted bound is not exceeded by a count that only looks large`（并新增状态维度同款断言） |
| ⑤ | **构造失败**（工作集内构造不出，且**推不出**超过上限的下界） | 只记缺口，**不得**拒绝 | `measurePatternStateCount` 的 `value` 缺席 + 下界 ≤ 上限 | `a construction that does not complete is not a refusal by itself` |

### 16.4 判据的关键：可证明下界，而非溢出

1. **状态数下界（声明推出，不依赖构造）**：设 `L` 为某声明的最长可接受轨迹长度。取长度 `L` 的接受
   轨迹 `w`，沿最小自动机读出状态序列。若该序列出现重复（`q_i = q_j`，`i < j`），则从 `q_i` 出发的那段
   转移可**重复任意次**（首尾同状态）而仍走完原来的后缀 ⇒ 可泵出**更长**的接受轨迹，与 `L` 最长矛盾；
   故该路径经过 `L + 1` 个**互不相同**的状态 ⇒ **状态数 ≥ `L + 1`**（等价说法：`w` 的 `L + 1` 个前缀的
   残差语言**两两不等价**——若某两个等价，则较长的那个前缀的残差语言里会有比 `L − i` 更长的词，与 `L`
   最长矛盾）。实现用既有受检助手 `checkedAdd` 计算 `L + 1`：**溢出 ⇒ 下界 ≥ 2^64 ⇒ 超过任何 `uint64`
   已接受上限**。这与"计数缺席"完全解耦：`repeat(atom.one, 1, UINT64_MAX)` 的 `L = UINT64_MAX` 是
   **可测得的**，其计数 `2^64` 才是不可表示的。
2. **为什么不是"份数 + 1"**：按声明份额推下界会给出**假拒绝**。`repeat(skip, 1, UINT64_MAX)` 的份额
   是 `UINT64_MAX`，但 `skip` 不消费元素，其**最长可接受长度是 0**、真值状态数是 **1**；按份额推会
   得到 2^64 并拒绝，与"该例配 `maxStateCount = measured(1)` 必须**接受**"直接冲突。`L + 1` 口径在
   `repeat(atom.one, 1, UINT64_MAX)` 上与 Codex 的 `2^64` 完全一致（`L = UINT64_MAX`）。
3. **最长匹配长度的三态区分（本轮拆出）**：`maximumLengthGap`＝**有限但无可表示值**（⇒ 上面第 1 条
   可用，推出 ≥ 2^64）；`maximumLengthUnbounded`＝**无有限最长匹配**（如 `complement` 消费剩余轨迹）
   ⇒ **不得**推导下界（无有限最长词时有限自动机完全可能很小，如 `Σ*`）；两者都缺席时
   `maximumTraceLength()` 仍缺席（对外可观察行为不变）。此拆分**仅**为本条判据服务，不改变任何既有访问
   器语义。
4. **维度与码**：本轮**不新增不删除**任何诊断码，② 档复用**既有**的
   `judgement.s7a3.pattern.budget_exceeded`（类别 `budget_exceeded`）与既有 path
   `pattern.maxStateCount`（展开维度 `pattern.maxExpansionCount`、早停维度
   `pattern.maxEvaluationSteps` / `pattern.maxCompiledBytes`、深度 `pattern.maxDeclarationDepth` 同理）。
   理由：Codex 只要求恢复**处置**，未要求新的可区分码；新增码会改变码表计数，而第 3 条（`gate_incomplete`
   的表示形式）尚未在本批落地。

### 16.5 净变化 / 净账（第 3 轮）

| 项 | 结果 |
| --- | --- |
| 本轮新增码 | **0 条** |
| 本轮删除码 | **0 条** |
| 相对本批次基线净账 | **净增 0 条 / 净删 2 条**（与 §8 表一致；两条删除均为第 2 轮动作） |
| 现行存活 `pattern.*` 码 | `…pattern.budget_exceeded`、`…pattern.atom_outside_declared_arms`、`…pattern.arm_bound_exceeded`、`…measure.category_not_derived_from_phase`、`…second_half.not_implemented`（**全部不变**） |
| 集中码表计数 | **不变**（`schemas/**` 与本轮 0 改动；门禁 exit 0，见 §16.9 第 4 项） |
| R 条目 / §9.2 类别 / ABI | 仍 **19** 条 / 仍**九**类 / ABI 行本轮 **0 改动** |

### 16.6 实现落点（文件:行；行号为写入后的实际行号）

| 文件 | 落点 |
| --- | --- |
| `engine/judgement/src/gameplay_assembler.cpp` | `CompiledMetrics` 新字段 `maximumLengthGap` / `maximumLengthUnbounded`（约 L2405–L2413）；sequence / choice / complement / repeat 四个原语的指标推导（约 L2508–L2600，`unbounded` 时**丢弃** gap）；存储字段 `maximumTraceLengthGap`（L2223）与赋值（L2711）；`PatternStateCountMeasurement`（L2754）与 `measurePatternStateCount` 的**独立下界推导**（L2776–L2786）；`enforcePatternBudget` 的扩写判据（函数 L2846，状态分支 L2911 起）；`CompiledPattern::stateCount()` 保持返回 `optional`、只取 `.value`（L3234） |
| `engine/judgement/src/source_codes.hpp` | 策略段改写为"可证明下界 / 无上限 / 非下界 / 构造失败"四分口径与两轮历史（约 L341–L371）；`kPatternBudgetExceededCode` 注释（L375） |
| `engine/judgement/src/pattern_dfa.hpp` | **仅注释**四处：头注释（L11–L18）、构造上界常量注释（L36–L44）、`abandon()`（L88–L91）、`PatternDfaResult`（L137–L141）——把"计数不可用一律不拒绝"改写为"不可用＝测量缺口；**可证明下界**超过**已接受**上限才是 `budget_exceeded`"（主控授权，见 §16.2 说明） |
| `engine/judgement/include/cuexis/judgement/gameplay_assembler.hpp` | **仅注释**：`maxExpansionCount`（L585–L596）、`maxStateCount`（L598–L619）、`maxEvaluationSteps` 早停段（L620–L626）、`stateCount()` 访问器（L719–L725）；**签名 0 改动**、**纯 ASCII** |
| `tests/judgement/gameplay_pattern_compile_tests.cpp` | 文件头第 6 条口径（L27–L32）；被改判用例重写为 `a state-count measurement gap refuses only a proven overrun`（L678）；新增 `the state-count lower bound is the longest acceptable trace plus one`（L792）；④ 档新增状态维度断言（L1051 起） |
| `docs/formats/GAMEPLAY_V2_SPEC.md` | §8.2 三项计数（L1530）与新增"超范围计数与拒绝的界分"三句对照（L1560）；§3.8.8 provenance 与第 3、5、8 条；§3.8.4 第 6 条；S7A-3 节末新增"第 3 轮实现复审的改正"段（L2131） |
| 本记录 | 标题、§8 净账指针、新增 §15.5b（门禁改正对照表）、§15.12 / §15.13 / §15.14 / §15.15 的指针与本 §16 |

### 16.7 测试（逐档覆盖、被改判的用例、未弱化的断言）

1. **被改判的用例（必须逐字记明）**：`repeat(atom.one, 1, UINT64_MAX)` 配 `maxStateCount =
   measured(1)`。**上一轮按缺口放行，经 Codex 第 3 轮复审驳回**；现断言 **`budget_exceeded` 拒绝**，
   码 `judgement.s7a3.pattern.budget_exceeded`、类别 `budget_exceeded`、path `pattern.maxStateCount`，
   并显式断言类别 **≠ `non_terminating_source`**。注释内以原文写下了这句改判说明。
   同一用例**新增**：最宽可表示上限（`UINT64_MAX`）同样拒绝（`2^64 > UINT64_MAX`）。
2. **⑤ 档（构造失败不拒绝）用例**：`repeat(atom.one, 1, 100000)` 的 `L = 100000`、`stateCount()`
   缺席；上限 **100001 接受**、上限 **100000 拒绝**。两者合起来证明拒绝来自**可证明下界**而不是构造
   失败本身。
3. **新增下界一致性用例**：对 `atom`、`sequence(a,b)`、`choice(a,b)`、`repeat(atom.one,1,4)`、
   `repeat(skip,1,UINT64_MAX)`、`nestedChain(16)` 逐例断言 **`stateCount() ≥ maximumTraceLength() + 1`**
   （两侧均由模块测得，互为独立证据）；并断言嵌套最大 repeat 的 `L` 有限但无可表示值 ⇒ `maximumTraceLength()`
   缺席且 `maxStateCount = 1` 时**拒绝**。
4. **③ / ④ 档保留**：无上限 ⇒ 接受 + 缺口；`repeat(repeat(atom,1,UINT64_MAX),0,0)` 各计数 0、
   `maxExpansionCount = 0` **接受**（§15.4 第 3 行原样保留），并新增状态维度同款断言
   （`maxStateCount = 1` ⇒ 接受，`maximumTraceLength() == 0`，**不得**被操作数的缺口污染）。
5. **保留的既有断言（未弱化）**：`repeat(skip,1,UINT64_MAX)` 状态数 = **1** 且配 `measured(1)` **接受**；
   `repeat(atom.one,1,M)`（M=1..4）→ **2/3/4/5** 且与独立残差测量一致；`repeat(atom.one,1,UINT64_MAX)`
   的计数仍**缺席**且测试注释仍写明"计数**不是 2**"；完全展开维度的"已测得超限拒绝 / 无上限不拒绝 /
   零因子不拒绝"三行全部保留。
6. **数字**：模块测试 **3407 断言 / 105 用例**（改动前基线 **3335 / 104**）⇒ **+72 断言 / +1 用例**。

### 16.8 Spec 同步（前 → 后）

| 位置 | 改动前 | 改动后 |
| --- | --- | --- |
| §8.2 计数口径第 1 项 | "求和无可表示值时记为测量缺口（缺席），**不是拒绝理由**" | 补"缺口本身不是拒绝理由；该计数**是经证明的下界**（真值 ≥ 2^64），存在已接受上限时按界分第 2 句拒绝" |
| §8.2 计数口径第 3 项 | "该缺口使状态预算门禁保持 incomplete：它**不构成**对内容的拒绝" | 补"缺口本身不构成拒绝；**拒绝判据**为已测得计数**或声明自身推出的可证明下界**超过已接受上限；缺口既不表示"已验证不超限"也不表示"已拒绝"" |
| §8.2 末段（新增三句对照） | 第 2 轮三条（溢出=缺口；有上限被确实超过才拒绝；无上限不拒绝） | 改写为**逐句可判定**的第 1–3 句：**缺席 / 非下界 ⇒ 测量缺口且不构成预算拒绝**；**已测得 或 可证明下界 超过已接受上限 ⇒ `budget_exceeded`**（含 `L + 1` 的状态数下界与"不新增码"）；**无已接受上限 ⇒ 不比较、不拒绝（门禁 incomplete）**。并新增第 5 句：本界分**只**适用于预算维度，§3.8.8 第 3 条的包含性保守拒绝是**另一道门禁**、不是预算诊断、不受本界分约束 |
| §3.8.4 第 6 条 | "完全展开计数记为测量缺口（§8.2 第 1 项），不构成拒绝" | 补"不**单独**构成拒绝；该计数仍是经证明的下界，**有已接受上限**时按 §8.2 界分第 2 句 `budget_exceeded`，**无**上限时不比较不拒绝（第 3 轮复审改正）" |
| §3.8.8 provenance | 只到第 2 轮（0.91） | 增第 3 轮（会话 `01a1006a-…`、`reject`、**0.93**、卡 `2026-10-03T06-20-24-581Z`）与其两条改正、第 3 条"仍 incomplete 且表示形式不变" |
| §3.8.8 第 3 条 | 只写"任一端无可表示值时保守拒绝、接入前必须固定并测试" | 新增一段：该保守拒绝是**包含性门禁自身**的未完成判定，**不是**内容超限诊断（**不得**报成 `budget_exceeded`、不占预算 path），**不改变**状态预算门禁的 incomplete 记录；反向：预算缺口**不**构成本条的保守拒绝理由；表示形式本批不决定 |
| §3.8.8 第 5 条 | "缺口不得单独构成任何内容拒绝……**不得**据此拒绝" | 改为"缺口本身不得构成拒绝，但**不**等于无从比较：**已测得**或**可证明下界**（溢出 ⇒ ≥ 2^64；`L` 有限 ⇒ 状态数 ≥ `L + 1`）超过**已接受**上限时按既有 path / 类别拒绝；无上限时不比较不拒绝" |
| §3.8.8 第 8 条 | "状态数测不出时……也**不得**据此拒绝" | 改为"缺口本身不得成为拒绝理由；该维度的拒绝**只**来自已测得计数或声明推出的可证明下界超过已接受上限；缺口 ≠ 已验证不超限、≠ 已拒绝，门禁**保持 incomplete**" |
| S7A-3 节末 | 只到"第 2 轮实现复审的改正" | 新增"第 3 轮实现复审的改正（2026-10-03）"段：两条改正、判据、**不新增不删除码**、净账仍 0 / 2、两卡局限 |

### 16.9 本轮门禁（命令、原始输出、真实退出码）

**① 构建（`--clean-first` 为必需，理由见 §15.13 第 8 项）**

```text
cmake --build --preset debug --target cuexis_judgement cuexis_judgement_tests --clean-first
[1/2] Cleaning all built files...
[27/29] Building CXX object tests\judgement\CMakeFiles\cuexis_judgement_tests.dir\gameplay_pattern_compile_tests.cpp.obj
[28/29] Linking CXX executable bin\cuexis_judgement_tests.exe
exit 0；日志 35 行，`: error` 0 行、`: warning` 0 行、`no work to do` 0 次
```

**② judgement 测试（直接运行可执行文件）**

```text
out\build\debug\bin\cuexis_judgement_tests.exe
All tests passed (3407 assertions in 105 test cases)
exit 0
```

改动前基线（本轮开工时实测）：`All tests passed (3335 assertions in 104 test cases)` ⇒ **+72 断言 / +1 用例**。

**③ 架构 CTest**

```text
ctest --preset debug -R cuexis_architecture --no-tests=error
1/1 Test #1: cuexis_architecture_tests ........   Passed    0.80 sec
100% tests passed, 0 tests failed out of 1
exit 0
```

**④ 集中码表校验器 CTest**

```text
ctest --preset debug -R cuexis_gameplay_diagnostics_codes --no-tests=error
1/1 Test #2: cuexis_gameplay_diagnostics_codes ...   Passed    0.33 sec
100% tests passed, 0 tests failed out of 1
exit 0
```

**⑤ 格式检查**

```text
cmake --build --preset debug --target cuexis_format_check
[0/2] Re-checking globbed directories...
[1/2] CMakeFiles\cuexis_format_check-3e5f1bf.bat e4b58b20f698a638
exit 0
```

（**首次运行 exit 1**：`-Wclang-format-violations` 共 **114** 处 —— `gameplay_assembler.cpp` 17、
`pattern_dfa.hpp` 6、`gameplay_assembler.hpp` 21、`gameplay_pattern_compile_tests.cpp` 70、
`source_codes.hpp` 0。已用仓库 `.clang-format` 对这 4 个文件 `clang-format -i`（改动行数 25 / 6 / 22 /
73，均落在本轮编辑区域），复跑 exit 0，随后重新 `--clean-first` 构建与跑测仍全绿。）

**⑥ 文档检查**

```text
python -B tools/check_docs.py
Documentation checks passed: 341 Markdown files and 20 candidate JSON/CXT files validated.
exit 0
```

**⑦ 空白检查**

```text
git diff --check
（无输出）
exit 0
```

（stderr 仅 15 条与本轮无关的既有文件 `LF will be replaced by CRLF` 警告，与 §15.13 第 7 项同一批文件。）

### 16.10 未跟踪文件的逐字节扫描（`git diff --check` 的盲区）

本轮改动的 `engine/judgement/**`、`tests/judgement/**`、`docs/formats/GAMEPLAY_V2_SPEC.md` 与本记录在
`git status --short` 中仍是 `??`（untracked），故另按逐字节检查 **CR / nonASCII / trailingWS / finalLF**：

```text
gameplay_assembler.hpp             CR=0 nonASCII=0      trailingWS=0 finalLF=True
gameplay_assembler.cpp             CR=0 nonASCII=0      trailingWS=0 finalLF=True
source_codes.hpp                   CR=0 nonASCII=0      trailingWS=0 finalLF=True
pattern_dfa.hpp                    CR=0 nonASCII=0      trailingWS=0 finalLF=True
gameplay_pattern_compile_tests.cpp CR=0 nonASCII=171    trailingWS=0 finalLF=True
GAMEPLAY_V2_SPEC.md                CR=0 nonASCII=137128 trailingWS=0 finalLF=True
本记录                              CR=0 nonASCII=47982  trailingWS=0 finalLF=True
```

说明：测试文件的 171 个非 ASCII 字节**全部**来自被改判用例的那句**逐字中文改判说明**（派单书要求
"在注释里写明'上一轮按缺口放行，经 Codex 第 3 轮复审驳回'"）。AGENTS.md 的纯 ASCII 约束**只**适用于
安装树里的公开头（`engine/*/include/cuexis/**/*.hpp`）：`engine/judgement/include/cuexis/judgement/` 下
**7 个公开头全部 nonASCII=0**，本轮改动过的 `gameplay_assembler.hpp` 亦为 0。两个 Markdown 文件的高
nonASCII 值为中文正文；本记录那一行是**登记时的实测值**，此后每增删一个中文字符都会变化。

### 16.11 已登记局限与未决项（如实）

1. **状态预算门禁仍 incomplete（未关闭）**：本轮的恢复只让"**可证明的**超限"成为拒绝；当计数缺席且
   声明推不出超过上限的下界时，该维度**仍不 enforced**、不给出任何"在预算内"的结论。
2. **构造失败仍是不可判定的区间**：`repeat(atom.one, 1, 100000)` 类声明（计数缺席、下界 ≤ 上限）就是
   这样的区间；本轮只把它锁成"不拒绝 + 缺口"，**未**增加任何构造能力。
3. **下界推导的适用范围如实登记**：`L + 1` 的推导需要 `L` **可测得**或**有限但无可表示值**；`L` 不存在
   （`complement` 等无有限最长匹配）时**不**推导下界（见 §16.4 第 3 条）。
4. **第 3 条（包含性门禁 `gate_incomplete`）的状态**：本轮**未**改 `checkPatternContainment`；装配消费点
   仍是 S7A-3 验收与进入 S7A-4 的**硬前置**（§15.7）。"在装配消费点落地并测试之前，不得验收 S7A-3，
   不得进入 S7A-4"仍然有效。
5. **Codex 卡的两点局限**（`resumed: false`、`toolCount: 0`）见 §16.1，必须与结论一起引用。
6. **预算数值未冻结**：S7A-9 前不冻结（Spec §8.4）；本轮**不**引入任何数值限额。

### 16.12 不变量声明（本轮）

1. **不改变任何计数**：§7.2 稳定拒绝清单仍 **19** 条；§9.2 类别仍 **九**类；契约处置词不变；
   [ABI](../../../api/GAMEPLAY_V2_ABI.md) 仍 **9** 域 **126 = 96 + 30** 行（本轮 0 行改动）。
2. **诊断码净变化**：相对本批次基线**净增 0 条**、**净删 2 条**；本轮**不新增不删除**（复用既有
   `judgement.s7a3.pattern.budget_exceeded` 与既有 path）。
3. **公开头签名 0 改动**：仅注释；`engine/judgement/include/cuexis/judgement/` 下 7 个公开头**纯 ASCII**。
4. **不改** `docs/api/**`、`schemas/**`、`tools/**`、`docs/proposals/**`、`docs/stage_reports/README.md`、
   任何 `CMakeLists.txt`、`cmake/**`、`.gitignore`、`AGENTS.md`。
5. **门禁状态不因本文档而改变**：arm / deadline 包含性门禁仍 **incomplete** 且装配消费点是**硬前置**；
   状态预算门禁仍 **incomplete**；S7A-3 第二半第三部分仍未实现；预算数值在 S7A-9 前仍**未冻结**。
6. **未提交**：本轮**不**执行 `git add` / `git commit`；工作树保持未提交状态。

### 16.13 建议索引行（更新版，供 `docs/stage_reports/README.md` 维护者使用）

维护者应把 §15.15 那行替换为下面这行（本记录**不**自行修改索引）：

```
  [S7A-3 第二半 part 1 实现裁定落地记录（三轮实现复核修订）](stages/stage-07/2026-10-03-s7a-3-implementation-rulings.md)（2026-10-03，第 1 轮 `reject` 0.94、第 2 轮 `reject` 0.91 与第 3 轮 `reject` 0.93 的逐条处置：空操作数一律拒绝被**驳回**并改为份数序 / 并集 / 不动点语义、三项 Pattern 计数口径（完全展开 / interned / 确定化并最小化后状态数）、**判据是"该计数是否可证明为下界"而非"是否溢出"**——缺席 / 非下界 / 构造失败＝测量缺口且不构成预算拒绝，**已测得或可证明下界超过已接受上限＝`budget_exceeded`**（状态维度下界由声明推出：最长可接受轨迹长度 `L` 有限 ⇒ 状态数 ≥ `L + 1`），无已接受上限则不比较不拒绝且门禁保持 **incomplete**、包含性保守拒绝与预算缺口**分成两道独立门禁**、`skip` / `instant` 语言同一与身份区分、`complement` 前缀消费语义、arm / deadline 包含性门禁**未完成**且装配消费点是进入 S7A-4 的**硬前置**、码表登记不得永久豁免、grade 只携带不求值、诊断码净增 0 / 净删 2、Spec §3.8.4 / §3.8.8 / §8.2 同步、两条已登记风险与七项门禁结果；**只登记语义与门禁状态，不表示 S7A-3 或 Stage 7A 实现完成**）、
```


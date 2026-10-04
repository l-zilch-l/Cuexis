# S7A-3 落地位置裁定（卡 4 / 卡 5）

## 1. 角色与出处

本文件是**带日期的裁定登记**，只登记裁定原文要点与主控的独立复核，**不表示任何实现完成**，也不改变任何门禁状态。

**权威范围在 plan。** 卡 1–5 已按 S7A-3 批次的权威范围登记进
[plan.md](../../../stage_plans/active/stage-07/plan.md) 的 `### S7A-3` 小节（"S7A-3 第二半 part 1 实现复核裁定后的范围收窄"）；
本文件承担**逐字要点与独立复核证据**，plan 承担**约束范围**。两者若有出入，以 plan 的范围规定为准，并以本文件的逐字原文订正 plan。

两张卡由主控通过只读工具向顾问取得（`ask_codex`，`sandbox: read-only`，模式 `consult`，工作区 `D:\Cuexis-worktree`）：

| 卡 | 时间 | 裁决 | 模型 | 工具调用 | 耗时 | 断流 |
|---|---|---|---|---|---|---|
| 卡 4 | 2026-10-03 15:54 | `adopt` **0.94** | `gpt-6-astra` | 3 | 241365 ms | 断开一次并**自愈** |
| 卡 5 | 2026-10-03 16:27 | `adopt` **0.97** | `gpt-6-astra` | 4 | 200413 ms | 无 |

前序三张卡（`adopt` 0.88 / 0.83 / 0.82）的逐条处置见 `2026-10-03-s7a-3-implementation-rulings.md` 的 §16；卡 4 与卡 5 是对同一批次**落地位置**的补充裁定。

## 2. 卡 4（`adopt` 0.94）：两条容量字段的"比较"落在哪一层

**裁决逐字**：*"采纳 A：容量只通过 `requirementIdentityProjection` 进入 `identity`，`canonicalCompare` 不改；为满足现有逐字段语义 diff 合同，`compareRequirement` 必须显式比较两容量。"*

**行动项（逐字要点）**：

1. **保留** `canonicalCompare(const RequirementRecord&, const RequirementRecord&)` 现有实现（先比 `identity`，再比 `stableId`），**不要**追加容量字段——以 `identity` 为首键已能区分"仅容量不同"的记录，再比即冗余死代码。
2. 新增**共享的容量排序辅助函数**，**唯一**规定 `缺席 < pending < measured(0) < measured(其他值按数值)`；`requirementIdentityProjection` 与身份编码**都调用它**、**禁止各自内联**（防口径漂移）。
3. 在 `compareRequirement`（`engine/judgement/src/gameplay_graph.cpp:533`）中为 `maxArmElements`、`maxDeadlineElements` **各加一条 `compareValue`**，放在 **`.pattern`（L557）之后、`.measure`（L558）之前**，按**语义关联**而非结构体声明顺序。
4. 测试覆盖**四态排序 + 自反性 + 反对称性 + 传递性**。
5. 记录"保守过分离"时**不得**描述成"Pattern 语义不等价"。

**自述风险**：漏掉两容量 ⇒ **人读 diff 文本与实际身份变化不一致**；`canonicalCompare` **不**承担四态顺序，若投影与身份编码各自实现顺序会**漂移**，必须由共享辅助函数统一。

**主控独立复核（三项全符）**：

| 卡 4 的说法 | 实测 |
|---|---|
| hpp 中 `canonicalCompare` 声明在 L1131 | **L1131**（同族 6 个重载 L1131 / 1134 / 1136 / 1139 / 1142 / 1145） |
| Spec §8.2 在 `GAMEPLAY_V2_SPEC.md:1529` | **L1529 = `### 8.2 计数口径登记（上界，不冻结数值）`** |
| 两容量插在 `.pattern` 之后、`.measure` 之前 | `gameplay_graph.cpp`：`.pattern` **L557** 紧邻 `.measure` **L558** ⇒ 可直接执行 |

## 3. 卡 5（`adopt` 0.97）：两容量的"有效值"写进哪一处身份投影

**背景（主控实测上报的歧义）**：卡 3 / 卡 4 使用的 `requirementIdentityProjection` 在代码里**不是函数**——它是 `ReferenceKind` 的**枚举值**（`engine/judgement/include/cuexis/judgement/gameplay_graph.hpp:110`），全仓只出现在 `closureOf` 的 `case` 分支（同文件 **L137**，归入 `judgementRef0`），此外没有任何使用点。把"投影"指成它，会让落地者去改引用归属矩阵，而不是去改字节投影。

**裁决逐字**：*"采纳：把两容量的已测有效值写入 `writeChartProjection` 的现有 requirement identity 字节序列（位于 requirement identity 之后、pattern 之前），不改 `ReferenceKind::requirementIdentityProjection`/`closureOf`；缺席或 pending 直接阻止身份发布而不写入字节。"*

**行动项（逐字要点）**：

1. 在**装配成功、两项容量均已测得、包含性门禁通过**之后，把容量写进 `writeChartProjection(IdentityByteWriter&, const CanonicalGameplayGraph&)`（`engine/judgement/src/gameplay_assembler.cpp:622`）的**每条 requirement 投影**，位置在 `writeRequirementIdentity(writer, requirement.identity)`（**L636**）**之后**、`writePatternNode(writer, requirement.pattern.root)`（**L650**）**之前**；作为既有 chart / compiled identity 字节序列的一部分，**不新增独立投影分量**。
2. **不要**修改 `ReferenceKind::requirementIdentityProjection`、`closureOf` 或 Spec §3.8.6 归属矩阵——容量是既有 requirement / pattern 判据内容，**不是**新的外部引用类别。
3. 把容量序列化封装为**一个**共享 writer / helper，**只由** `writeChartProjection` 调用（避免 closure 判定另有副本）。
4. 缺席或 pending **不编码任何哨兵值**：在身份生成前由装配门禁判为**不可发布**并原子失败，只有 `measuredValue()` 非空的两个有效值才进入字节投影。

**自述风险**：当前 `RequirementRecord` 尚无容量字段 ⇒ 落地前必须先完成卡 2 的字段与装配传递；**若把容量写进 `writePatternNode`**，会错误地把 requirement 级约束绑到 PatternNode 结构上，且无法表达"同一 Pattern 在不同需求容量下的身份差异"；把 pending 编成 `0` / 最大值 / 其他哨兵会违反 `MeasuredParameter` 的无默认值语义，制造**可发布但不可证明**的编译身份。

**主控独立复核（全部相符）**：

| 卡 5 的说法 | 实测 |
|---|---|
| `writeChartProjection` 定义在 L622 | **L622**（L623–L634 先写 `gameplayVersion` / `graphRevision` / `rulesetRef` / timebase / latePolicy 与 requirement 计数；L635 起逐条 requirement） |
| `writeRequirementIdentity` 调用在 requirement 投影开头 | **定义 L524、调用 L636** |
| `writePatternNode` 调用在其后 | **定义 L533、调用 L650** |
| 身份由 `makeChartIdentity` 经 `writeChartProjection` 生成 | `makeChartIdentity` 在 **L1847**，于 L1850 / L1857 / L1881 调用 `writeChartProjection` |
| `MeasuredParameter` 无零默认、无哨兵 | `engine/judgement/include/cuexis/judgement/timebase.hpp:361-385`：以 `std::optional` 表达未测，只有 `pendingMeasurement()` / `measured(Value)` 两个构造入口，`measuredValue()` 返回指针 ⇒ **无零默认、无哨兵** |

## 4. 对落地计划的影响

- **第 3 步订正**：由"两容量进 `requirementIdentityProjection`"改为"新增共享容量排序辅助函数 + 在 **`writeChartProjection`** 的每条 requirement 投影里（`writeRequirementIdentity` 之后、`writePatternNode` 之前）写入两容量的**已测有效值** + 在 `compareRequirement` 的 `.pattern` 之后插入两条 `compareValue`"；**`canonicalCompare` 不改、`ReferenceKind` / `closureOf` / Spec §3.8.6 不改**。
- **第 4 步前置**：消费点必须**新建** `PatternArmBound` 的构造——该结构已存在于 `engine/judgement/include/cuexis/judgement/gameplay_assembler.hpp:647-657`（`declaredActionRefs` / `maxArmElements` / `maxDeadlineElements`，后两者为 `MeasuredParameter<std::uint64_t>`），但**全仓无任何构造点**；需把需求级声明字段映射为 `MeasuredParameter`。
- **既有行为必须改**：`gameplay_assembler.hpp:641-646` 逐字写着"a pattern with no finite longest length cannot be shown to be contained: **that case is refused conservatively**"。卡 1 / 卡 2 要求把"不可测得"表达成**独立的 `gate_incomplete` 状态**，因此第 4 步**必须改这里并加测试**，不能沿用"一律拒绝"。

## 5. 本文件不主张的事

1. **不表示实现完成**：卡 4 / 卡 5 只决定"落在哪一层"，两容量字段、共享排序辅助函数、`compareRequirement` 的两条 `compareValue`、装配消费点**全部尚未实现**。
2. **不改变门禁状态**：arm / deadline 包含性门禁仍 **incomplete**；`INCOMPLETE GATE` 仍为 **5 处**（`gameplay_assembler.hpp` L612 / L641 / L800；`gameplay_assembler.cpp` L2845 / L2897）；装配消费点仍是进入 S7A-4 的**硬前置**。
3. **不冻结数值**：两容量的合法性与单位按 Spec §3.3 定义，**数值限额在 S7A-9 前不冻结**。
4. **不动 ABI / 诊断码 / 引用矩阵**：无 ABI 变更；`judgement.s7a3.*` 净增 0 / 净删 2；`ReferenceKind` 与 Spec §3.8.6 归属矩阵**不改**。
5. **未提交**：本文件与工作树改动都保持未提交。
6. **未在本文件独立核验的部分**：卡 4 / 卡 5 的行动项本身（如共享辅助函数的具体形态、字节写入的确切编码长度前缀）尚未设计，需在实现时按 `IdentityByteWriter` 的既有约定确定。

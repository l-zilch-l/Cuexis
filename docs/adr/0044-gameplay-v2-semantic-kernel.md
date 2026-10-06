# ADR 0044：Gameplay V2 语义内核与冻结边界

状态：candidate（本 ADR 是 V2 文档组之一；按 2026-10-02 owner 接受的**限定冻结**口径，它在下方"接受门禁"限定的范围内授权受限 S7A-1 骨架，范围外——含未列出的字段表示、宽度、序列化与后续轮次语义——不授权。决策本身已被 owner 接受，见下一行）

决策状态：已接受（owner 于 2026-10-02 接受[未决语义分轮裁决清单](../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md)第 1 轮经 Codex 修订后的 14 行文本）；实施状态：**未实施**——本 ADR 自身不定义字段集合与 typed 边界，不冻结公共头，也不表示 `engine/` 已有任何实现

决策日期：2026-10-02

更新日期：2026-10-03

关系：本 ADR 与 [Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md)（字段与运行语义的唯一权威 Spec，当前 `candidate`）、[Gameplay V2 ABI](../api/GAMEPLAY_V2_ABI.md)（typed 内部 / preview C++ 边界，当前 `candidate`）互链，构成 S7A-0 冻结顺序要求的三份文档。本 ADR 只写决策、影响、威胁模型与接受门禁，不复制 Spec 的字段参考，不复制逐次测试日志或构建结果，不写出研究切片数值。本 ADR 不改动 [ADR 0043](0043-gameplay-judgement-ruleset-convergence.md)、[Gameplay Judgement Spec](../formats/GAMEPLAY_JUDGEMENT_SPEC.md) 与 [Gameplay Judgement ABI](../api/GAMEPLAY_JUDGEMENT_ABI.md)；它们的替代关系标注晚于本 ADR 与两份 V2 文档的冻结。

## 背景

Stage 7A 以 Gameplay V2 为实施基线，而不是以 Gameplay I 的候选合同为基线。[Gameplay V2 acceptance package](../proposals/gameplay-v2-acceptance/README.md) 已被 owner 于 2026-10-02 整包接受（见 [S7A-0 接受记录](../stage_reports/stages/stage-07/readiness/2026-10-02-s7a-0-acceptance.md)），其第 1 轮 14 行语义裁决也已被 owner 接受，接受对象是经 Codex 修订后的文本（[RULING_WORKSHEET.md](../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) §0.2、§1、§9）。第 1 轮覆盖 Q-01、Q-02、Q-10、Q-14 四项语义未决点，以及合同项 CM-V06、CM-V07、CM-V13、CM-I06、CM-X06 与缺陷 D-3、D-4、D-10。

接受并**不**等于可以实施。计划 §1.1 规定了冻结顺序：先建立并冻结 V2 的 ADR / Spec / ABI，之后才允许把 Gameplay I 的 ADR 0043、Gameplay Judgement Spec 与 Gameplay Judgement ABI 标注为 `superseded` / `historical`（[plan.md](../stage_plans/active/stage-07/plan.md) §1.1、[接受记录](../stage_reports/stages/stage-07/readiness/2026-10-02-s7a-0-acceptance.md) §4）。冻结顺序的存在理由与 Schema / Replay / Snapshot / diagnostics 无关：后者属独立契约工件，不是建立替代关系的最低前置（RULING_WORKSHEET.md "执行顺序裁决"）。先 supersede 再补替代文件会产生"已取代但替代不完整"的窗口，实施者可能同时解释两套字段。

第 1 轮的四项裁决不是字段细节，而是横跨 Chart v5、Packed、CXC、离线装配、Session 与 Replay 的结构性决策：它们分别决定"哪些版本组合被接受""同一内容存在几种语义""什么算作同一判定""谁有权决定 capability 闭包"。在任何一项未定时，S7A-1 的公共类型与 identity 算法没有可比对基准（RULING_WORKSHEET.md 第 1 轮"不决定的后果"列）。

仓库现状也要求把决策与字段分开：Gameplay I 的 ADR 0043 状态为 `proposed`，其 Spec 与 ABI 为 `candidate`，按 V2 研究索引的说法只保留为历史基线与差异对照（[研究索引](../proposals/research/gameplay-v2/README.md)）；Chart v5 的 `gameplay` 区段 Schema、Replay 合同、Snapshot Schema 与集中 diagnostics 码表**尚不存在**（acceptance package §2.2）。因此本 ADR 只固定跨格式决策，字段与运行语义交由 V2 Spec，typed 边界交由 V2 ABI。四层抽象的共同语义基础见[音乐游戏玩法抽象模型](../architecture/GAMEPLAY_ABSTRACTION_MODEL.md)。

## 备选方案

下表列出在冻结这些决策前被明确否决的方案。否决理由均来自已接受材料，不是本 ADR 新增的判断。

| 备选方案 | 内容 | 否决理由 |
| --- | --- | --- |
| A1：Canonical Gameplay Graph 只放在 Packed 中 | 以 Packed 的 typed section 作为 gameplay graph 的唯一物理与语义定义，source 只是输入 | 会让 Packed wire 成为语义定义本身，source→Packed 的 round-trip 无法做独立语义比较；且 Packed 本身是带 `candidateRevision` 的候选期载体（[FORMAT_ENTRY_AND_IDENTITY.md](../proposals/gameplay-v2-acceptance/FORMAT_ENTRY_AND_IDENTITY.md) §6、§7） |
| A2：物理入口唯一 | Chart v5 JSON 区段是唯一载体，CXC 不得有独立 `gameplay-graph` Playback entry | 与 CM-V13 / P2-01 的裁决直接冲突；若定为物理唯一，CXC entry 矩阵自相矛盾（RULING_WORKSHEET.md 第 1 轮 CM-V13/P2-01 行） |
| A3：graph 只作为编译中间层 | Canonical Graph 只在 prepare 期存在，不成为任何可独立引用的载体 | 与"`gameplay-graph` entry 可独立 Playback"冲突，且使 CXC 两种 entry 无法共享同一语义来源（FORMAT_ENTRY_AND_IDENTITY.md §6 选项 3） |
| A4：引入独立的 `semanticRevision` 版本号 | 用与 `gameplay.version` 平行的字段表达判定语义变更 | 制造第二个同义版本轴；计划 §5.1 明确禁止重新引入 `semanticRevision` 作为同义版本，合同项 CM-V03 亦为 `accept`（不引入） |
| A5：capability 由 compiler 在装配期静默补齐 | 声明少于派生闭包时，compiler 自动补全声明并继续 | 掩盖声明与实际需求的不一致，破坏确定性与能力协商；第 1 轮 Codex 修订已把原二选一固定为"稳定失败"（RULING_WORKSHEET.md §0.2 第 1 条、§1 Q-14 行） |
| A6：仅凭 `compiledSemanticIdentity` 判等价 | 两个 Packed 产物 identity 相同即视为可互换 | 不可互换的产物（工具链 / 载体差异）会被误判为等价；Codex 修订要求补 artifact compatibility / interchange 判定与兼容矩阵（RULING_WORKSHEET.md §0.2 第 2 条） |
| A7：旧 gameplay v1 在最早入口按字段存在性推断并接受 | 依据 `resources` / `factBindings` 等字段是否存在猜测版本，运行期隐式迁移 | Reader 猜版本会把新语义读成旧 kind，属静默降级；CM-V04 要求显式离线迁移或最早入口稳定拒绝（[CONTRACT_MATRIX.md](../proposals/gameplay-v2-acceptance/CONTRACT_MATRIX.md) §1） |

未列入上表的方案（例如为 7B+ 能力预留未定义字段、把表现几何提升为判定域）在支持 / 拒绝矩阵中已作为明确拒绝项登记（[SUPPORT_AND_REJECTION_MATRIX.md](../proposals/gameplay-v2-acceptance/SUPPORT_AND_REJECTION_MATRIX.md) §3、§5）。

## 决策

以下四项决策的文本采用 owner 已接受的裁决原文（含 Codex 修订），本 ADR 不重写、不加码，只补充理由、影响范围与对应 Spec 章节。

### 决策 1：版本层级固定为外层 Chart `version = 5` 加 `gameplay.version = 2`

**决策原文（Q-01，含 Codex 修订）。** 接受"外层 5 + gameplay 2"唯一组合；旧 v1 仅显式离线迁移，否则在最早入口拒绝；Packed header 显式带 gameplay revision；`candidateRevision` **不**改变 `compiledSemanticIdentity`，只改 artifact identity。`candidateRevision` 必须进入 artifact compatibility / interchange 判定，不可互换的 Packed 产物不得仅凭 `compiledSemanticIdentity` 判等价；同时须定义覆盖工具链与载体差异的兼容矩阵，不能只补 `candidateRevision` 一项。

**理由。** 接受面必须先于类型冻结确定：只要"哪些版本组合合法"未定，Chart v5、Packed、CXC 三处就无法确定各自接受到哪一层，S7A-1 的公共类型与 identity 算法没有基准（RULING_WORKSHEET.md 第 1 轮 Q-01 行"不决定的后果"）。禁止 Reader 通过字段存在性猜版本，是为了不让"未知必需内容"被解释成旧语义（CONTRACT_MATRIX.md §1 CM-V04；[CHART_V5_ALIGNMENT_REVIEW.md](../proposals/research/gameplay-v2/CHART_V5_ALIGNMENT_REVIEW.md) P0-01）。把 `candidateRevision` 排除在 `compiledSemanticIdentity` 之外、同时纳入 artifact compatibility，是为了区分两种不同问题："这是不是同一判定语义"与"这两个产物能不能互换"；只回答前者会造成错误等价，只回答后者会造成错误不等价。

**边界。** 本决策不写 Packed header 字段布局、不写兼容矩阵的行与列、不写 `compiledSemanticIdentity` 的字节构成；那些属 V2 Spec 与 Packed 候选 wire revision 工作。研究切片数值不进入本 ADR，也不得据此成为 ABI 常量（[plan.md](../stage_plans/active/stage-07/plan.md) §S7A-0 工作内容 3）。

**对应 Spec 章节。** [Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md) §2「版本层级与兼容」（§2.1 唯一合法版本组合、§2.2 旧 `v1` 处置、§2.3 三种 identity 判定与互换性、§2.4 兼容矩阵、§2.5 稳定失败）。兼容矩阵的可互换性最小充分条件仍由该 Spec 登记为未决。

### 决策 2：Canonical Gameplay Graph 语义唯一、物理入口可多

**决策原文（Q-02 + CM-V13 / P2-01，含 Codex 修订）。** Chart v5 JSON 区段是**语义视图**，CXC `gameplay-graph` 是**可选物理 entry**；所有合法物理入口必须规范化并恢复为**同一** Canonical Gameplay Graph；JSON 区段与 CXC entry 是可选且**可互相替代**的载体，不是并存的语义。Codex 给出的表述是："**Chart v5 是 gameplay typed graph 的唯一聚合语义模型；JSON、CXC 可选 entry 等物理载体必须恢复同一 typed graph**"。

**理由。** 若表述成"并存语义"，同一内容会有两个语义入口，Packed section 与 CXC entry 矩阵无法设计，`packed-chart` 与 `gameplay-graph` 也无法被要求进入同一 typed prepare、Judgement、Ruleset、Snapshot、Replay 与 Presentation 路径（FORMAT_ENTRY_AND_IDENTITY.md §2 硬要求 2）。把唯一性限定在语义层、放开物理载体，才能在允许第二种 Playback entry 的同时保持单一判定实现。

**边界。** 本决策不定义 `gameplay-graph` entry 的 manifest closure、预算与拒绝编码（属 CM-V10，第 6 轮）；不定义 graph 的 wire 格式；不改变 Chart v5 作为作者聚合清单的既有职责（FORMAT_ENTRY_AND_IDENTITY.md §2 载体矩阵）。

**对应 Spec 章节。** [Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md) §3「Canonical Gameplay Graph」（其中 §3.6 载体与 entry 的 `entryKind` 与 manifest closure 归第 6 轮裁定）。

### 决策 3：建立唯一 identity projection 表，并分别列明三条判定

**决策原文（Q-10，含 Codex 修订）。** 建立唯一 identity projection 表，逐字段标注 source / semantic / artifact / judgement / presentation；以"实际生效的 prepared value"为准；凡能改变 Fact Ledger 内容或顺序者必须改变 judgement identity。表中须**分别列明** semantic identity、artifact identity 与 interchange / compatibility 判定三条，均以实际 prepared value 为准。

**理由。** identity 分区不定，Replay 就无法证明"同一判定"，S7A-6 的全量 Snapshot 没有可比对基准，Packed section、FactBinding 与 TimebaseProfile 的归属也无处登记（CONTRACT_MATRIX.md §9 CM-I06）。以"实际生效的 prepared value"为基准，而不是以来源写法为基准，才能让同一最终值的显式声明与继承声明共享 judgement identity，同时把来源差异留在 source / content identity 与诊断中。三条判定必须分开列，是因为语义相同、artifact 不同、互换性不同的组合真实存在（见决策 1）；缺少 interchange 列时，Q-01 的互换性约束无处登记。

**边界。** 本决策只固定这张表的义务与列语义，不冻结行的内容；完整逐字段表是 CM-I06 的产出，仍会随第 5、6、7 轮的 fact 因果总序（Q-12）、Snapshot header（Q-13）、resource / Packed reference closure 归属（P1-14）、sourceMap 与 content identity 的拆分（P2-07）继续补齐。本 ADR 不写 identity 分量算法。

**对应 Spec 章节。** [Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md) §5「identity projection 表」；typed 边界见 [Gameplay V2 ABI](../api/GAMEPLAY_V2_ABI.md) 的「类型清单」与「稳定与非稳定声明」。

### 决策 4：capability 由 source 声明、assembler 派生，声明不足即稳定失败

**决策原文（Q-14，含 Codex 修订）。** source 只声明最低必需 capability；assembler 派生完整 closure；Packed header 保存排序后的 derived closure；CXC manifest 复制 artifact-required capability；Session 只报告四态、不改写 graph。声明少于 assembler 派生 closure 时**稳定失败**，**不允许** compiler 静默补齐；失败时点固定在最早可确定 closure 不足的编译 / 装配阶段。

**理由。** 当前有五处 capability 来源并存（Packed META、CXT extension、Chart gameplay、本地 registry、Session 协商）。优先级不定，同一内容可被两个工具解释成不同 capability 集，进而产生不同 identity（CONTRACT_MATRIX.md §12 CM-X06；CHART_V5_ALIGNMENT_REVIEW.md P0-14）。把派生权收归离线 assembler、把 Session 限制为只读四态报告，可以让"谁声明"与"谁判定"分离。禁止静默补齐，是因为补齐会把"内容声明不足"这一事实从证据链中删除；把失败时点固定在最早可确定处，则保证失败可复现、可诊断，而不是延迟到运行期才暴露。

**边界。** 本决策不改 7A 的四态查询集合，也不定义 capability registry 的字段与类型（属 V2 Spec / ABI）。"声明多于派生"的处置（拒绝或标记为可选）仍待 Spec 落定，不在本 ADR 冻结。

**对应 Spec 章节。** [Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md) §6「capability 派生与闭包」（其中 §6.6「声明多于派生」的处置**已由第 1 轮 `Q-14` / `CM-X06` 裁定**——声明集合不得多于派生闭包、多于派生同样不成立；第 7 轮 2026-10-03 按第 1 轮裁定补齐处置词与正文，见该 Spec §6.6 与 §11.1）。

## 影响

1. **对 S7A-1。** 版本层级、graph 归属、identity 分区与 capability 派生已有决策基准，公共类型与 identity 算法可以在 V2 Spec / ABI 冻结后按这四项对齐；但在 V2 文档冻结、Gameplay I 标注与 typed contract review 完成之前，仍不得进入公共头与实现批次（[RULING_WORKSHEET.md](../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) §9 第 1 轮"是否解锁该批次"列）。
2. **对格式工作。** 本 ADR 是输入而非字段合同：Chart v5 的 gameplay 修订、CXC 的 `entryKind` 与 manifest closure、Packed 的候选 wire revision、CXT v2 的 `effects` 降级都属 acceptance package §2.1 列出的修订动作，其字段正文在 V2 Spec 与各自格式文档中落定（[acceptance README](../proposals/gameplay-v2-acceptance/README.md) §2.1）。
3. **对 Replay 与 Snapshot。** 三条 identity 判定与"实际生效的 prepared value"基准会直接影响 Replay header 与 Snapshot header 的可比较字段；互换性判定必须失败关闭（fail closed），不能仅凭 semantic identity 接受一个不可互换的产物。
4. **对 capability 与拒绝路径。** source 声明成为下界而非事实；派生闭包与拒绝码必须一致；7B+ 新能力仍须带独立 capability ID / revision、稳定拒绝与 identity 影响说明（[plan.md](../stage_plans/active/stage-07/plan.md) §5.1）。
5. **对文档状态。** 本 ADR 与 V2 Spec、V2 ABI 三份文档冻结且互链校验通过后，Gameplay I 的三份合同才能加 `superseded` / `historical` 标注；在此之前旧文档保持原状态，不得零散改动（[acceptance README](../proposals/gameplay-v2-acceptance/README.md) §3）。**实际执行偏差（2026-10-02 如实记录）：** 标注已先于限定冻结发生；不倒填冻结时间，纠正记录见 [plan.md](../stage_plans/active/stage-07/plan.md) §1.1 与[接受记录](../stage_reports/stages/stage-07/readiness/2026-10-02-s7a-0-acceptance.md) §4.2。
6. **不影响的内容。** 不修改 `engine/`、CMake、公共安装头或 SDK API 版本；不构成 Stage 8 发行矩阵的选入；不解除四项 Stage 6 交接的关闭前置；不把任何研究切片测量值变成生产限额——仓库唯一已冻结的生产预算表是 [PACKED_CHART_FORMAT.md](../formats/PACKED_CHART_FORMAT.md) §3.3 的 7 项上限，其余预算保持"只登记上界、不冻结限额"，口径见 [BUDGET_AND_EVIDENCE_PLAN.md](../proposals/gameplay-v2-acceptance/BUDGET_AND_EVIDENCE_PLAN.md)。
7. **不实施的成本。** 上述四项决策在实现前保持"已接受、未实施"；任何消费者不得据本 ADR 声称字段、ABI 或预算已经冻结。

## 威胁模型

Gameplay 内容、Packed / CXC 载体、Ruleset 与 Replay 都可能来自不可信来源。本 ADR 冻结的四项决策分别针对以下威胁；具体码表与字段级拒绝路径由 V2 Spec 与 diagnostics 工件拥有。

### 威胁 1：运行时脚本、任意 IO 与随机性

任意运行时脚本 VM、逐帧脚本回调、宿主字节码、动态 Requirement 生成、随机数、墙钟与 IO 参与判定属永久能力边界（H01-H05、H07）。本 ADR 与两份 V2 文档都不得为它们预留字段、extension、capability、字节码或隐式执行入口；命中时稳定拒绝，不得降级为 Tap / Hold / v4 语义（[SUPPORT_AND_REJECTION_MATRIX.md](../proposals/gameplay-v2-acceptance/SUPPORT_AND_REJECTION_MATRIX.md) §3 R-14、§5；[DOCUMENTATION_POLICY.md](../DOCUMENTATION_POLICY.md) "脚本边界"）。决策 4 的派生闭包是这条边界的执行点之一：未声明的执行能力不得由 assembler 补出。

### 威胁 2：非确定性数值与浮点

决策域必须使用 prepare 期冻结的整数投影；不得在判定路径读取未声明的浮点或宿主数学库；每个判定域必须声明量程并证明不等式不溢出。跨编译器或跨平台出现差异按阻塞处理，不得以"整数运算应当确定"解释（[plan.md](../stage_plans/active/stage-07/plan.md) §5.2；[ADR 0043](0043-gameplay-judgement-ruleset-convergence.md) §5 的整数域与量程证明属保留部分）。身份判定若依赖未声明的数值路径，会同时破坏决策 1 的语义等价与决策 3 的 judgement identity。

### 威胁 3：能力伪造与降级欺骗

不可信 source 可能**多声明** capability（伪造能力、绕过预算或进入未实现路径）或**少声明**（隐瞒实际需求）。决策 4 规定：声明少于 assembler 派生闭包时稳定失败，不补齐；声明的排序后闭包进入 Packed header，CXC manifest 复制 artifact-required capability 供早期拒绝。"声明多于派生"的处置仍待 Spec 落定，但任何情况都不得让不同入口对同一 source 得到不同 identity。降级欺骗的另一种形式是把不支持的能力解释成旧 kind；这由支持 / 拒绝矩阵的 19 条稳定拒绝与"不得回落 Tap/Hold/v4"规则阻断（SUPPORT_AND_REJECTION_MATRIX.md §3 规则 4）。

### 威胁 4：identity 混淆导致的错误等价或错误不等价

错误等价：把语义相同但不可互换的 Packed 产物判为等价（决策 1 的 interchange 判定与兼容矩阵即为此设）。错误不等价：把表现资源、source 数组顺序、注释或 source map 纳入 judgement identity，使换皮肤、源重排或 Packed 压缩顺序变化错误地失效 Replay（[FORMAT_ENTRY_AND_IDENTITY.md](../proposals/gameplay-v2-acceptance/FORMAT_ENTRY_AND_IDENTITY.md) §4.3）。两种错误的共同控制手段是决策 3 的 projection 表：逐字段标注归属，并以"实际生效的 prepared value"为基准，而不是以来源写法或工具链中间产物为基准。

### 威胁 5：预算越界与解压炸弹

不可信载体可能声明超预算的 section、结构或展开数量。控制手段是：Reader / Writer 的 envelope 预检、逐 section 预算、未知必需 section 稳定拒绝（不得忽略），以及本 ADR 不新开任何绕过预检的入口（FORMAT_ENTRY_AND_IDENTITY.md §7）。有界循环在 7A 采用 prepare 期完全展开，展开数量、时间与内存必须有静态上界（CM-C10 / P1-03；**第 3 轮已于 2026-10-03 裁定**：7A 采用 prepare-time **完全展开**，展开结果**唯一决定** identity / snapshot / fact order；`boundedRelationInstance` **不进 7A 合同**，遇到时稳定拒绝并列为 7B+ / S7C 候选，见 [Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md) §3.8.4 与 [S7A-3 准入门禁记录](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-3-gate-rulings.md)）。**第 4 轮（2026-10-03）**另行确认 `SolverProfile` 的**数值预算**（`K` / fuel / `maxCandidates` / `maxBranches` / `maxFuel`）与**默认 profile 清单**不在 S7A-4 冻结、留给 **S7A-9** 与后续预算批次；本轮只冻结**字段语义**与 prepare 拒绝语义，`max_cardinality`、global / 最优 solver 与 bounded backtracking 仍稳定拒绝（不得作为运行时预算绕过入口，见 [Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md) §3.12 / §3.13 与 [S7A-4 准入门禁记录](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-4-gate-rulings.md)）。预算数值方面：只有 [PACKED_CHART_FORMAT.md](../formats/PACKED_CHART_FORMAT.md) §3.3 的 7 项是已冻结生产上限；V2 gameplay 预算在 S7A-9 前保持未冻结，研究切片数值不得作为限额、ABI 常量或拒绝阈值（plan.md §S7A-0 工作内容 3；BUDGET_AND_EVIDENCE_PLAN.md §8.2 禁止用法）。

### 威胁 6：W 类缺口被当作可用能力

W 类缺口是已知无法在现有 L1-L4 与格式层安全表达、且尚未形成 capability 提案的玩法或格式需求；它与 capability 的区别是尚未形成归属与预算路径。风险是把这类缺口当作"已支持能力"或为其预留隐式入口。控制手段：命中 W 类缺口时稳定拒绝并给出 `wId`；不得编造编号；registry 不得为未列入能力的缺口预留字段或执行入口；登记不等于纳入，W 类缺口不进入 7A 支持集合（[plan.md](../stage_plans/active/stage-07/plan.md) §5.3 的登记与引用规则；[SUPPORT_AND_REJECTION_MATRIX.md](../proposals/gameplay-v2-acceptance/SUPPORT_AND_REJECTION_MATRIX.md) §6 的字段定义与 §6.1 的 W-01 至 W-05 **研究稿候选**——**编号仅为候选标识，尚未登记为 `wId`**，其字段缺项在首次实际消费前由消费批次补全并按 `S1-03` 标注；RULING_WORKSHEET.md §1 D-3 行）。字段定义的唯一权威在 SUPPORT_AND_REJECTION_MATRIX.md §6，登记与引用规则在 Stage plan §5.3，本 ADR 沿用该落点，不在 V2 Spec 附录重新定义字段（亦见 [Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md) §1.3）。**第 4 轮（2026-10-03）把 `CM-C04` 的完整资源状态机分支**（`gap` / `handoff_pending` / 非零 grace）**与 `capacity > 1`、owner 集合、并列 slot 归入同一口径**：7A 资源子集冻结为 `free` / `held` / `terminal`，子集之外**稳定拒绝**、**不得预留隐式字段或执行入口**，其后续语义登记为 **S7B+ / S7C 候选**——**登记不等于纳入**（[Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md) §3.10、[SUPPORT_AND_REJECTION_MATRIX.md](../proposals/gameplay-v2-acceptance/SUPPORT_AND_REJECTION_MATRIX.md) §3 的 R-01 / R-04、[S7A-4 准入门禁记录](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-4-gate-rulings.md)）。

### 威胁 7：载体间语义漂移

同一内容可经 Chart v5 JSON 区段、CXT v2 emission、Packed Chart 与 CXC `gameplay-graph` 四条路径进入。若任一入口产生不同 canonical graph、不同 prepared identity 或不同 capability 闭包，就会出现两套判定实现。决策 2 要求所有合法物理入口恢复同一 Canonical Gameplay Graph；验证手段是独立的语义比较（逐字段 semantic diff 与 round-trip），只比 hash 不算证据（FORMAT_ENTRY_AND_IDENTITY.md §2 硬要求 1、§8）。"Chart v5 inline 与 CXT v2 emission 的等价性定义"仍待第 7 轮（P1-15）裁定，因此该威胁在冻结完成前保持开放登记。

### 威胁 8：Replay / Snapshot 篡改与身份误配

Replay 与 Snapshot 也是不可信输入：截断、乱序、时间回退、重复、超事件 / 字节预算、未知 capability、identity mismatch 与旧版本都必须在解码前稳定失败，且解码不得执行 IO、脚本或动态生成。身份误配会直接掩盖决策 3 的错误等价，因此恢复与 seek 的入口必须同时校验语义、artifact 与互换性判定，而不能只校验其中一条。

## 接受门禁

### 已被 owner 接受的部分

| 项 | 状态 | 依据 |
| --- | --- | --- |
| Q-01 版本层级（含 artifact 互换性约束与兼容矩阵要求） | 已接受（2026-10-02，按 Codex 修订文本） | RULING_WORKSHEET.md §0.2、§1、§9 |
| Q-02 Canonical Gameplay Graph 归属（含 CM-V06） | 已接受 | 同上 |
| Q-10 唯一 identity projection 表与三条判定（含 CM-I06） | 已接受 | 同上 |
| Q-14 capability 派生与"声明不足即稳定失败"（含 CM-X06） | 已接受 | 同上 |
| CM-V13 / P2-01 语义唯一、物理入口可多 | 独立登记并随 Q-02 同批关闭 | RULING_WORKSHEET.md §1、§8 |
| D-3、D-4、D-10 文档缺陷处置 | 已接受（D-3 落 Stage plan；D-4 只标版本条款被取代；D-10 路径引用已订正） | RULING_WORKSHEET.md §1、§9 |
| 第 1 轮是否解锁批次 | 是（语义前置已满足），但公共头与实现批次仍受下方门禁约束；第 1 轮补充 S1-01…S1-05 进一步把授权限定为**受限 S7A-1 骨架** | RULING_WORKSHEET.md §7A、§9 |

### 第 7 轮已裁定（2026-10-03）：无待裁定轮次

**第 6 轮（发布粒度、表现桥接与诊断）已于 2026-10-03 由 Codex 裁定**（thread `01a0fe61-3476-7882-9674-5f6b05035237`，单卡 verdict `adopt`、confidence **0.97**，落点见 [S7A-6 / S7A-7 准入门禁记录](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-6-gate-rulings.md)）：15 行登记 / 按项计 17 项，编号 `S7A6-R01…R12`；`CM-P04`、`CM-P06`、`CM-P08`、`CM-D04`、`CM-X01` 五项处置词 `open` → `accept`（合同台账随之变为 `accept` 47 / `open` 5，总数仍 114）；**所有 15 行的首次消费批次一律 S7A-7**（`P2-06` 的资源三分法沿用 S7A-4 已冻结的 `free` / `held` / `terminal` 子集）。第 6 轮的语义正文见 [Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md) §3.6、§3.23–§3.25、§5.6、§6.4、§9.6–§9.8、§S7A-7 节与 [Gameplay V2 ABI](../api/GAMEPLAY_V2_ABI.md) 域 8 / 域 9 与 `## S7A-7 限定冻结范围与登记规则（2026-10-03）` 节；**本轮只冻结文档合同与门禁，不表示 Judgement 实现完成或 Stage 7A 完成**。

**第 7 轮（收尾澄清与缺陷）已于 2026-10-03 由 Codex 裁定**：主卡 thread `01a0fe61-b7a3-7853-a380-0793fee14e14`（consult，模型 `gpt-6-astra`，verdict `adopt`、confidence **0.96**）与口径确认卡 thread `01a0fe61-3476-7882-9674-5f6b05035237`（verdict `adopt`、confidence **0.99**），逐条登记见 [RULING_WORKSHEET.md](../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) §7–§9，带日期证据见 [第 7 轮收尾裁定与缺陷处置记录](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-7-wrapup-rulings.md)。**第 7 轮共 16 行 / 18 项**，编号 `S7A7-R01…R16`。

**逐轮口径（订正后自洽）**：第 1 轮 14 行 / 15 项、第 2 轮 6 行 / 7 项、第 3 轮 6 行 / 7 项、第 4 轮 6 行 / 6 项、第 5 轮 13 行 / 14 项、第 6 轮 15 行 / 17 项、**第 7 轮 16 行 / 18 项**；合计 **76 行 / 84 项**，与 [RULING_WORKSHEET.md](../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) §0.1 绑定的 84 项**完全一致**（合并行按"行 = 实项 + 每合并行 1、项 = 实项 + 每合并行 2"折算，合并行为 `CM-V13 / P2-01`、`CM-T08 / P1-04`、`CM-C10 / P1-03`、`CM-S10 / P1-07`、`CM-P06 / P1-13`、`CM-P08 / P1-11`、`CM-D04 / P1-09`、`D-2 / D-7`）。

**口径订正说明（不改写历史报告正文）**：本 ADR 此前的 "第 7 轮 16 行 / **17** 项" 与"逐轮合计 76 行 / **83** 项与 §0.1 的 84 相差 1" 由第 7 轮口径确认卡（confidence **0.99**）判定为**推导错误**：第 7 轮实为 **16 行 / 18 项**，逐轮合计 76 行 / 84 项自洽。**第 6 轮门禁报告中"76 行 / 83 项"的表述是那份带日期报告的历史正文，本轮不重写它**；以本节与工作表 §8 为准。

**第 7 轮同时收尾五条残余 `open` 合同项**：`CM-V06`、`CM-V07`、`CM-V13`、`CM-I06`、`CM-X06` 按第 1 轮（2026-10-02）已完成的裁定**只补齐处置词与裁定正文**，`open` → `accept`，合同台账最终 **`accept` 52 / `revise` 26 / `supersede` 17 / `retain` 17 / `open` 0 / `reject` 2 = 114**，`open` 集合为空。**第 7 轮之后无待裁定轮次**；`D-9` 仍为 **owner-only**（候选补丁未提交，须 owner 按 ADR 0042 具名复核后落 `master`，在完成前不得宣称 S7A-8 的版本门禁闭合），`D-12` 仍**待 owner 确认**（本轮只登记、不编辑研究稿的指向行）。此前各轮状态见下：**第 4 轮（仲裁、资源与事实序）已于 2026-10-03 裁定**（Q-03、Q-11、Q-16、CM-C05、CM-C09 与 P1-12 共 6 行；`CM-C05`、`CM-C09` 处置词 `open` → `accept`，`CM-C04`、`CM-C06`、`CM-C07` 处置词不变、只更新未决点文字；provenance：thread `s7a4-arbitration-rulings`，主裁定卡 verdict `adopt`、confidence 0.97 / 处置词与计数确认卡 verdict `adopt`、confidence 0.99，落点见 [S7A-4 准入门禁记录](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-4-gate-rulings.md)），**第 5 轮（Ruleset、事实序、Score 与 Snapshot）亦已于 2026-10-03 裁定**（Q-08、CM-S02、Q-17、CM-S08、Q-12、Q-13、CM-K01、CM-K07、CM-K08、CM-F06、CM-S10 / P1-07、CM-S11 与 P1-14 共 13 行 / 14 项，编号 `S7A5-R01…R11`；`CM-F06`、`CM-S08`、`CM-S10`、`CM-K07` 处置词 `open` → `accept`，`CM-S02`、`CM-K01` 处置词不变、只写裁定正文；`CM-S11`、`CM-K08` 不是合同项、不改变任何计数；provenance：thread `01a0fe49-cfe5-7933-8d03-3d0a65d34c8b`，三卡 verdict 均 `adopt`、confidence 0.95 / 0.99 / 0.98，落点见 [S7A-5 / S7A-6 准入门禁记录](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-5-6-gate-rulings.md)）。第 7 轮已一并处置缺陷 **D-13**（格式门禁：在 HEAD 曾为红，候选处置 (a) 已由 owner 执行完毕——三个 spike 文件 `clang-format -i`、`cuexis_format_check` 由 156 处降到 0、`AGENTS.md`:110 的 glob 描述已订正）与 **D-14**（合同项 `CM-X01` 台账冲突：按方案 (a) 记入第 6 轮、首次消费 S7A-7，并把合同台账的 `accept` 修正回 `open` 后经第 7 轮重新闭合为 `accept`）。与本 ADR 四项决策直接相关、并会反向补全其边界的至少包括：

| 轮 | 与本 ADR 的关系 |
| --- | --- |
| 第 2 轮（时间域与迟到策略）**已裁定（Codex 2026-10-03）** | TimebaseProfile 与 offset / calibration / late policy 的归属进入 judgement identity（决策 3 的表行）：Tick 宽度与单位来源、RationalBeat → `judgementTick` 的 tie rule、`observationTick` 的 canonical 来源与 S7C-1 边界、late policy 的 typed 参数化（数值不冻结）、`commitTick` / window 定义与 `AmountSpec` 越界处置已落进 [Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md) §0.1、§3.7 与 [Gameplay V2 ABI](../api/GAMEPLAY_V2_ABI.md) 的域级登记；具体 late-policy 数值、具体业务量程限额、S7C-1 扩展与连续输入能力登记为后续批次阻塞项。**2026-10-03 实现复核修订（最小指针）**：第 2 轮的 `stop` 语义修订为**精确累计函数**表述（Spec §3.7.2 第 3 条：`tick(beat) = roundHalfToEven(F(beat))`、只舍入一次、区间内非单射、半开方向正向 `(originBeat, beat]` / 负向 `(beat, originBeat]`、有 `stop` 时不再关于 origin 奇对称），并补三分法（Spec §9.3）与迟到参数量级门禁 / Tick → Beat 反查两项阻塞登记；**不改变本 ADR 的任何决策语义**，见 [第 2 轮实现裁定记录](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-2-implementation-rulings.md) |
| 第 3 轮（prepare、装配与 entry）**已裁定（Codex 2026-10-03）** | Q-07 / CM-V08 决定 Packed 的 gameplay revision 以何种 entry / section 边界承载（决策 1 的载体落点）——本轮**只冻结行为**，wire 编号 / 布局 / 序列化编码在 S7A-3 消费前另行闭合；CM-C10 / P1-03 决定有界循环 canonical form（威胁 5）：7A 采用 prepare-time 完全展开，`boundedRelationInstance` 稳定拒绝并列为 7B+ / S7C 候选；Q-05 的 `effects` 边界与 identity 规范字节边界（决策 3：规范字节是 prepare 内部确定性派生物，不是公共 ABI 或 Packed 编码）亦已落进 [Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md) §3.8 / §5.5 与 [Gameplay V2 ABI](../api/GAMEPLAY_V2_ABI.md) 域 3 / 域 7 / 域 8 |
| 第 4 轮（仲裁、资源与事实序）**已裁定（Codex 2026-10-03）** | Q-16 / CM-C05 把 Tick 六步与 Coordination window 八阶段对应为**唯一**映射（六步＝外层、八阶段＝第 4 步内部完整展开），并把阶段顺序与语义归入 **judgement identity 的 engine 组件**（决策 3；见 [Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md) §3.9）；Q-11 / CM-C09 冻结 **7A 资源子集** `free` / `held` / `terminal` 与资源身份规则，`gap` / `handoff_pending` / 非零 grace / handoff / `capacity > 1` / owner 集合 / 并列 slot 稳定拒绝（威胁 6；§3.10）；Q-03 / CM-C06 / CM-C07 拆分 **prepare / compile solver** 与 **runtime coordinator**（**不解析/编译 solver；runtime coordinator 只执行已准备的确定性 policy**——第 7 轮 `S7A7-R07` 收官该措辞，2026-10-03）、名称 `coordinator.policy.greedy_v1`，`SolverProfile` 只冻结字段语义与 prepare 拒绝语义、数值预算留给 S7A-9（威胁 5；§3.12）；P1-12 冻结早 / 晚判定与 Hold error 记录（§3.11） |
| 第 5 轮（Ruleset、事实序、Score 与 Snapshot）**已裁定（Codex 2026-10-03）** | Q-12 / CM-F06 冻结 **Fact 因果总序**（唯一 `(commitTick, originKindPriority, canonicalOrdinal)`、`canonicalOrdinal` 由引擎从 `(originScope, originOrdinal, localOrdinal, phasePriority, factKindPriority)` 派生、**禁用 `ingressSequence` / 容器顺序 / 线程完成顺序**、Fact 的 `tick` 即 `commitTick`）与 **Fact 身份生成语义**（`originId` / `commitId` / `factId` / `phasePriority` / timer ordinal / correction 仍由 R-08 拒绝），生成语义与 phase registry 进 **`FactSemanticRevision`**（属 `JudgementIdentity.engine`）——直接补全决策 3 的 projection 表（见 [Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md) §3.9 第 3 条、§3.17）；CM-K07 / Q-13 / CM-K01 冻结 **全量 Snapshot header 与 Replay header 字段集**、`SnapshotPayload` 闭包与不得保存清单，`factSemanticRevision` 进 engine identity 而 `stateSchemaRevision` 不进（威胁 8；§3.18）；Q-08 冻结 **Ruleset Tick 三阶段与 `faulted` 行为矩阵**（威胁 6；§3.14、§9.4）；CM-S02 冻结 `RegisterKind` 三类与冲突策略（§3.15）；Q-17 / CM-S08 冻结 **7A 只用内置 Ruleset**（package 命中 R-09、形状归 S7C-2；§3.16）；CM-S10 / P1-07 冻结 Outcome / category / grade / `TimingError` 与统计 reset·seek·replay（§3.20）；CM-S11 冻结 **Life 在 7A 关闭**（§3.21）；P1-14 的 **resource / Packed reference closure 归属表**作为 Spec 附录 §3.8.6 冻结、首次消费 S7A-3、集成 S7A-7；`SeekLatencyCommitment` 的类型、承诺语义与会话只能收紧已冻结，数值留 S7A-9（§3.19） |
| 第 6 轮（发布粒度、表现桥接与诊断）**已裁定（Codex 2026-10-03）** | Q-06 / CM-V10 的 CXC `entryKind`、manifest closure 与预算（决策 2 的落地）：`playback = true` 必须显式 `entryKind`，7A 只允许 `packed-chart` 与 `gameplay-graph`，`author-source` **不是** Playback entry，Ruleset package **不是**第三种 entry（Spec §3.6、§S7A-7 节）；Q-19 / `CM-X01` 的 `compiledSemanticIdentity` 与**完整 capability closure 必须同时携带且可比较**（决策 1 与决策 4；`CM-X01` 的 `CapabilityRecord` 七字段 identity 归属表落 **Spec §5.6 与 §6.4**，`stableRejectCode` **不进 identity**；`CM-X06` 的"capability 派生来源唯一"面由第 7 轮按第 1 轮裁定补齐为 `accept`，**不属第 6 轮**）；表现桥接的 precedence 与 adapter 生命周期（`CM-P04` / `CM-P06` / `CM-P08`，Spec §3.23）；`CM-D04` / P1-09 的四层诊断模型、九类 category 与集中码表 + CTest 校验项（Spec §9.1、§9.2、§9.6、§9.7）；P1-08 的 ABI 映射（Spec §3.24；`observationTick` 仍唯一来自校准会话时钟，`error` 为有符号整数 tick 差，`eventSequence` 为会话内单调序号）；P2-04…P2-09 的分类层 / 互斥与组合、`action`/`requiredAction`/`domain` 三者独立、`resources` 三分拆、只有 Fact Ledger 进 Replay/Snapshot、`extensions` 仅命名空间化 inspection metadata（Spec §3.25、§3.26）。**15 行 / 17 项，首次消费批次一律 S7A-7**；本轮只冻结文档合同与门禁，不表示 Judgement 实现完成 |
| 第 7 轮（收尾澄清与缺陷）**已裁定（Codex 2026-10-03，主卡 thread `01a0fe61-b7a3-7853-a380-0793fee14e14` `adopt` 0.96 + 口径确认卡 `adopt` 0.99，`S7A7-R01…R16`，16 行 / 18 项）** | P1-15 的 inline / CXT v2 等价性与逐字段 semantic diff（威胁 7）；P1-01 的静态 typed 判定域（几何属 Gameplay closure、与 Presentation transform 完全隔离）；P1-02 的 CXT local relation 全局合并规则；P1-10 的预算分层与"谁产生谁计数"（数值仍留 S7A-9）；P2-02 的 solver / coordinator 措辞（"不解析/编译 solver；runtime coordinator 只执行已准备的确定性 policy"）；P2-07 的三项命名统一（`source closure` / `diagnostic map` / `content-artifact identity`）；D-2 / D-5 / D-6 / D-7 / D-9 / D-11 / D-12 / D-13 / D-14 缺陷处置。落点 [Spec §3.8.7 / §5.1.1 / §8.5 / §S7A-7 之后的收尾](../formats/GAMEPLAY_V2_SPEC.md)、[V2 ABI `## S7A-7 之后的收尾`](../api/GAMEPLAY_V2_ABI.md) 与 [第 7 轮收尾裁定与缺陷处置记录](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-7-wrapup-rulings.md) |

### 本 ADR 施加的接受门禁

1. **冻结顺序门禁。** Gameplay I 的 ADR 0043、Gameplay Judgement Spec、Gameplay Judgement ABI 的 `superseded` / `historical` 标注本应**晚于**本 ADR、V2 Spec、V2 ABI 三份文档的建立与限定冻结；每份都要写状态词并给出替代链接（RULING_WORKSHEET.md "执行顺序裁决"；[接受记录](../stage_reports/stages/stage-07/readiness/2026-10-02-s7a-0-acceptance.md) §4 第 3 步）。**该顺序在执行中被提前（标注先于限定冻结）**，按 2026-10-02 owner 接受的 S1-02 处理：如实记录偏差、不倒填冻结时间，并在限定冻结后重新核验三份旧文档的替代链接与 retained 映射，复核结果记入[接受记录](../stage_reports/stages/stage-07/readiness/2026-10-02-s7a-0-acceptance.md) §4.2。
2. **互链与状态一致门禁。** 三份文档必须状态一致、互链完整，且本 ADR 的决策文本与其在 V2 Spec 中的对应条款不得互相矛盾；链接完整性以仓库文档检查为准。
3. **索引门禁。** 本 ADR 必须登记到 [ADR 索引](README.md) 并从主索引可达，否则 `tools/check_docs.py` 的可达性与单一 H1 校验不通过（[DOCUMENTATION_POLICY.md](../DOCUMENTATION_POLICY.md) §链接和重复、§自动检查）。
4. **未决项门禁。** 在**本批次依赖的** P0 全部裁定前，S7A-1 的公共头与实现批次不得启动；登记为阻塞项的批次不得启动，"以后再定"不是可接受的裁定（RULING_WORKSHEET.md §0 使用说明；[acceptance README](../proposals/gameplay-v2-acceptance/README.md) §4.3）。
5. **不得越界的声明门禁。** 本 ADR 不冻结 Schema / Replay / Snapshot / diagnostics 工件，不把研究切片数值写成任何限额或常量，不构成 Stage 8 选入，也不声称任何实现已完成。
6. **实施门禁。** 四项决策在实现前保持"已接受、未实施"；受限 S7A-1 的实施授权时点是：第 1 轮补充 S1-01…S1-05 登记完成 + 三份文档的**限定基线冻结** + 替代关系复核 + typed review 关闭记录齐备，且 `check_docs` 与 `git diff --check` 通过（plan.md §1.1 冻结顺序块）。

## 与 ADR 0043 的关系

[ADR 0043](0043-gameplay-judgement-ruleset-convergence.md) 是 Gameplay I 的收敛工作稿，当前状态为 `proposed`。按 owner 已接受的处置映射，本 ADR 与两份 V2 文档冻结后，0043 将被标注为 `superseded`（部分保留），并链接替代文档（[acceptance README](../proposals/gameplay-v2-acceptance/README.md) §3）。

映射的预期分工是：0043 的 §1 四层边界、§5 整数域与量程证明、§7 预算分层原则与威胁模型属保留部分；§2 的 grace 语义（拆为 7A 的 `preparedGrace` 与后续 `continuityGrace`）、§3 核心表达模型中的 `Effect`、§4 的 Fact 排序 `(requirementId, phase, outcome)` 属被替代部分。Gameplay Judgement Spec 的字段与运行语义、Gameplay Judgement ABI 的候选类型同样由 V2 取代或降为 `historical`（retained）。

**本次不改动 0043，也不改动 Gameplay Judgement Spec 与 ABI。** 冻结顺序要求先有完整的替代文档，再标注旧文档；在 V2 Spec 与 V2 ABI 建立并冻结前提前 supersede，会产生"已取代但替代不完整"的窗口。本 ADR 只记录该关系与执行时点，不执行标注，也不改变 ADR 0043 的任何既有决策语义。

## 未决与后续

1. **本 ADR 只固定决策，不固定字段。** 版本字段的接受面细节、Packed header 的 gameplay revision 承载形式、identity projection 表的行、capability registry 字段与诊断码表，全部由 [Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md) 与 [Gameplay V2 ABI](../api/GAMEPLAY_V2_ABI.md) 拥有（两者当前为 `candidate`，随冻结完成后再与本 ADR 一起进入冻结状态）；本 ADR 与它们的分工不得互相复制正文。
2. **第 1–7 轮已全部裁定，无待裁定轮次**（第 2 轮至第 7 轮均已于 2026-10-03 裁定，见"接受门禁"表；第 7 轮另把五条残余 `open` 合同项按第 1 轮裁定补齐，`open` 集合为空），其中直接补全本 ADR 边界的条目见"接受门禁"表。**裁定完成不等于实现完成**：本 ADR 的相关决策在实现批次完成前仍保持"已接受但未实施"；`D-9` 为 owner-only（版本门禁候选补丁未提交），`D-12` 待 owner 确认（研究稿指向行）。
3. **兼容矩阵尚未定义。** 决策 1 要求覆盖工具链与载体差异的兼容矩阵；其轴、判定算法与失败码由 V2 Spec 设计，本 ADR 只固定"必须存在且必须失败关闭"。
4. **`compiledSemanticIdentity` 的字节构成与互换性判定算法未冻结**，且两者必须分离；分离方式需在 Packed 候选 wire revision 工作中确认（第 6 轮已冻结 `compiledSemanticIdentity` 与完整 capability closure **必须同时携带且可比较**及其逐字段 identity 归属，见"接受门禁"表第 6 轮行）。
5. **W 类缺口的登记位置已按第 1 轮裁决落定，但字段定义与登记规则分处两份文档。** 字段定义（唯一权威）在 [SUPPORT_AND_REJECTION_MATRIX.md](../proposals/gameplay-v2-acceptance/SUPPORT_AND_REJECTION_MATRIX.md) §6；登记与引用规则在 [plan.md](../stage_plans/active/stage-07/plan.md) §5.3，原四处 W 类缺口引用已改为指向该节（[Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md) §1.3 声明不在 Spec 内重新定义）。本 ADR 沿用该落点，不在 V2 Spec 附录重复定义字段；冻结复核时应确认计划正文的四处引用与 §5.3 一致。
6. **本 ADR 未覆盖的相邻边界**：Chart v4 / CXT v1 回退行为、ADR 0042 的 candidate 隔离、四项 Stage 6 交接收口、SDK API 版本门禁均不由本 ADR 改变；它们的实施与证据要求仍以 [Stage 7A 实施计划](../stage_plans/active/stage-07/plan.md) 为准。
7. **证据边界。** 本 ADR 的唯一证据来源是 2026-10-02 的 owner 接受记录、第 1 轮裁决文本、S7A-0 准入包，以及 2026-10-03 的第 2 轮裁定（thread `s7a1-freeze-bindings`，verdict `adopt`，confidence 0.99，落点见 [S7A-2 准入门禁记录](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-2-gate-rulings.md)，其 `stop` 语义另经同日**实现复核**由同 thread 两张续裁卡修订为**精确累计函数**表述，verdict 均 `adopt`、confidence 0.98 / 0.99，落点见 [第 2 轮实现裁定记录](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-2-implementation-rulings.md)）、第 3 轮裁定（thread `s7a3-prepare-rulings`，三卡 verdict `need_info` 0.8 / `reject` 0.88 / `adopt` 0.99，落点见 [S7A-3 准入门禁记录](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-3-gate-rulings.md)）与第 4 轮裁定（thread `s7a4-arbitration-rulings`，主裁定卡 verdict `adopt`、confidence 0.97 / 处置词与计数确认卡 verdict `adopt`、confidence 0.99，落点见 [S7A-4 准入门禁记录](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-4-gate-rulings.md)）、第 5 轮裁定（thread `01a0fe49-cfe5-7933-8d03-3d0a65d34c8b`，三卡 verdict 均 `adopt`、confidence 0.95 / 0.99 / 0.98——主裁定卡给出 `S7A5-R01…R11`，处置词与计数确认卡把 `CM-F06`、`CM-S08`、`CM-S10`、`CM-K07` 由 `open` 改为 `accept`（最终 `open` 10 / `accept` 42，总数仍 114），ABI 状态格首词与追踪计数追问卡确认 9 行首词仍为 `待冻结`、**126 = 96 + 30** 不变且不新增 §未决项 / §类型清单条目；落点见 [S7A-5 / S7A-6 准入门禁记录](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-5-6-gate-rulings.md)）、**第 6 轮裁定**（thread `01a0fe61-3476-7882-9674-5f6b05035237`，单卡 verdict `adopt`、confidence **0.97**——给出 `S7A6-R01…R12`，把 `CM-P04`、`CM-P06`、`CM-P08`、`CM-D04`、`CM-X01` 由 `open` 改为 `accept`（最终 `open` 5 / `accept` 47，总数仍 114），并冻结 `Q-06`、`Q-09`、`Q-15`、`Q-19` 与 `P1-08`、`P2-04`…`P2-09`；落点见 [S7A-6 / S7A-7 准入门禁记录](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-6-gate-rulings.md)）、**第 7 轮裁定**（主卡 thread `01a0fe61-b7a3-7853-a380-0793fee14e14`，verdict `adopt`、confidence **0.96**；口径确认卡 thread `01a0fe61-3476-7882-9674-5f6b05035237`，verdict `adopt`、confidence **0.99**——给出 `S7A7-R01…R16`（16 行 / 18 项），把残余 `CM-V06`、`CM-V07`、`CM-V13`、`CM-I06`、`CM-X06` 按第 1 轮裁定补齐为 `accept`（最终 `open` 0 / `accept` 52，总数仍 114），并处置 `D-2` / `D-5` / `D-6` / `D-7` / `D-11` / `D-13` / `D-14`；落点见 [第 7 轮收尾裁定与缺陷处置记录](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-7-wrapup-rulings.md)）与第 7 轮 D-13 / D-14 的 owner 与 Codex 处置；**`D-9` 仍为 owner-only、`D-12` 仍待 owner 确认**，不得据本 ADR 声称这两项已闭合；hosted、GPU、真实设备与音频证据均未运行，不得据本 ADR 推断其通过。

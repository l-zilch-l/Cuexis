# 未决语义分轮裁决清单

状态：candidate；S7A-0 准入产出，随 Gameplay V2 acceptance package 一并接受；**7 轮裁定已全部登记完成（2026-10-02 至 2026-10-03）**

更新日期：2026-10-03

上级文档：[Gameplay V2 acceptance package](README.md) ·
[未决问题与 owner 决策请求](OPEN_QUESTIONS.md) ·
[Stage 7A 实施计划](../../stage_plans/active/stage-07/plan.md)

文档角色：裁定工作表。它把 [CONTRACT_MATRIX.md](CONTRACT_MATRIX.md) 的合同项（登记时为 5 项 `open`，第 7 轮后为 **0**）与
[OPEN_QUESTIONS.md](OPEN_QUESTIONS.md) 的 53 项未决问题**合并成一张按批次依赖排序的裁定清单**，
使 owner 可以逐轮批完，而不必在两份文档之间来回对照。

**本表已完成全部 7 轮裁定（2026-10-02 至 2026-10-03）。** 第 7 轮（收尾澄清与缺陷）于 2026-10-03 由 Codex
裁定（thread `01a0fe61-b7a3-7853-a380-0793fee14e14`，verdict `adopt`，confidence **0.96**），并由同一话题的
口径确认卡（thread `01a0fe61-3476-7882-9674-5f6b05035237`，verdict `adopt`，confidence **0.99**）确认第 6 轮
`S7A6-R01…R12` 映射与三处计数口径。**第 7 轮后已裁定 76 行 / 84 项，当前 `open` 0 项**（`open` 集合为空，
见 §8 覆盖核对与 [CONTRACT_MATRIX.md](CONTRACT_MATRIX.md) §0 统计）。

它不改变任何既有合同项的状态；裁定结果由 owner 在本文件 §9 登记（**第 1–7 轮已全部登记**），然后才据此冻结 V2 的
ADR / Spec / ABI / Schema / Replay / diagnostics。**裁定完成不等于实现完成**：S7A-3…S7A-9 的实现批次仍未完成。

## 0. 怎么用

- 每行给出：**编号** | **未决点** | **建议处置** | **不决定的后果** | **owner 裁定**（第 1–7 轮已全部填入，
  **无 `待填` 行**；第 7 轮的 16 行见 §7）。
- 建议列的来源分两种，必须区分：
  - `包建议`：已被 [OPEN_QUESTIONS.md](OPEN_QUESTIONS.md) 记录过的候选处置，本清单只做摘要与归并；
  - `新建议`：本清单首次给出的建议（P1/P2 类原先没有候选处置），属**提案**，owner 可以整体否决
    而不影响 `包建议` 的可接受性。
- 裁定四选一：**接受** / **修改后接受**（写明改动）/ **拒绝 + 替代** / **登记为阻塞项**
  （登记阻塞的批次不得启动；"以后再定"不是可接受的裁定）。
- 快速批法：可以只写"第 N 轮全部接受建议"，或逐条写"Q-04 修改后接受：<改动>"。

### 0.1 轮次总览

| 轮 | 目的（解锁批次） | 表内行数 | 一并关闭的 `open` 合同项 |
| --- | --- | --- | --- |
| 1 | 进入 S7A-1 前 | 14 | CM-V06、CM-V07、CM-V13、CM-I06、CM-X06 |
| 2 | 进入 S7A-2 前 | 6 | CM-T03、CM-T04、CM-T08 |
| 3 | 进入 S7A-3 前 | 6 | CM-X05、CM-C10 |
| 4 | 进入 S7A-4 前 | 6 | CM-C05、CM-C09 |
| 5 | 进入 S7A-5 / S7A-6 前 | 13 | CM-F06、CM-S08、CM-S10、CM-K07 |
| 6 | 进入 S7A-7 前 | 15 | CM-P04、CM-P06、CM-P08、CM-D04、CM-X01 |
| 7 | 收尾澄清与缺陷 | 16 | CM-V06、CM-V07、CM-V13、CM-I06、CM-X06（第 1 轮已裁定、第 7 轮补齐处置词与裁定正文） |
| 合计 | — | **76 行 / 84 项** | **21**（该列＝"由哪一轮一并关闭"，**不是**"当前仍 `open`"；第 1 轮五项、第 2 轮三条、第 3 轮两条、第 4 轮两条、第 5 轮四条与第 6 轮五条均已实际闭合，当前仍 `open` 者 **0** 项，见 §8 覆盖核对） |

覆盖核对见 §8。

**末列口径（2026-10-03 第 2、3、4、5、6、7 轮裁定后）。** 末列登记**该轮一并关闭的 `open` 合同项**（列语义是"哪一轮
一并关闭"，**不是**"当前仍 `open`"；它是**历史归属列，不是当前状态列**），共 **21** 项：`CM-X01` 因缺陷 D-14
方案 (a) 由 `accept` 修正为 `open`，计入第 6 轮（第 6 轮 4 → 5）；第 5 轮为 4 项（`CM-F06`、`CM-S08`、
`CM-S10`、`CM-K07`）；第 1 轮为 5 项（`CM-V06`、`CM-V07`、`CM-V13`、`CM-I06`、`CM-X06`，第 7 轮补齐
处置词与裁定正文）。**第 1 轮那五项（`CM-V06`、`CM-V07`、`CM-V13`、`CM-I06`、`CM-X06`）、第 2 轮那三条
（`CM-T03`、`CM-T04`、`CM-T08`）、第 3 轮那两条（`CM-X05`、`CM-C10`）、第 4 轮那两条（`CM-C05`、`CM-C09`）、
第 5 轮那四条与第 6 轮那五条（`CM-P04`、`CM-P06`、`CM-P08`、`CM-D04`、`CM-X01`）均已实际闭合**（处置词由
`open` 分别改为 `accept`、`revise` / `accept`、`accept` / `accept`、第 5 轮四条的 `accept`、第 6 轮五条的
`accept`，以及第 7 轮五条的 `accept`，见
[CONTRACT_MATRIX.md](CONTRACT_MATRIX.md) §3、§5、§12 与 §0 统计），因此本列仍列它们（它们正是由对应轮次
一并关闭的），但**当前仍 `open` 的合同项已由 21 经 18、16、14、10、5 降为 0**，分轮分布同步为
第 1 轮 **0**、第 2 轮 **0**、第 3 轮 **0**、第 4 轮 **0**、第 5 轮 **0**、第 6 轮 **0**、第 7 轮 **0**（第 7 轮
闭合的是第 1 轮的残余五项、不新增 `open` 归属；见 §8 覆盖核对的 `open` 行）。
`CM-S02`、`CM-K01`（处置词 `revise`，本次绑定到第 5 轮）与 `CM-T13`、`CM-S11`、`CM-K08`（本次登记新增的
裁决项、**不是** CONTRACT_MATRIX 的合同项）均**不计入末列**；逐条绑定见 §7B。

### 0.2 Codex 咨询记录（第 1 轮）

owner 要求第 1 轮"先与 Codex 商讨再决定是否接受"。三次咨询的结果如下，逐字结论以桥落盘的决策卡为准：

| 轮次 | thread | 模型 | verdict | confidence | 关键结论 |
| --- | --- | --- | --- | --- | --- |
| 第 1 次 | `s7a0-r1-p0b` | `gpt-6-astra`（hard / medium） | **reject** | 0.97 | 第 1 轮**不可原样接受**：Q-14 必须定为"声明不足即稳定失败"；Q-01 必须补产物互换性约束；Q-02、Q-10 原则可采纳 |
| 第 2 次 | `s7a0-freeze-order` | `gpt-6-astra`（hard / medium） | **adopt** | 0.97 | 先建并冻结 ADR 0044 / V2 Spec / V2 ABI 三份互链文档，再标 Gameplay I 为 `superseded`；**不必**等 Schema/Replay/diagnostics 工件；D-10 归本轮；D-8 并入 S7A-1 allowlist 批次 |
| 第 3 次 | `s7a0-r1-rest` | `gpt-6-astra`（hard / medium） | **adopt** | 0.82 | CM-V13/P2-01 **独立登记**并与 Q-02 同批关闭（语义唯一、非物理唯一）；D-3 定义落在 **Stage plan** 并给 4 处引用补定义链接；D-4 在研究稿**顶部与 §5.1/§6** 标注被 V2 Spec 取代并链接条款、保留正文；D-3/D-4 归第 1 轮 |

第 1 次咨询未覆盖 CM-V13/P2-01、D-3、D-4 与冻结顺序，由第 2、3 次补齐；第 1 次的 `reject` 只针对
Q-14 留空与 Q-01 的互换性缺口，不是对整轮的全盘否决。

**Codex 修订后的第 1 轮建议**（下表 §1 各行已按此改写，并在行内标注 `Codex 修订`）：

1. **Q-14**：声明少于 assembler 派生 closure 时**稳定失败**，不允许 compiler 静默补齐；
   失败时点固定在最早可确定 closure 不足的编译/装配阶段。
2. **Q-01**：`candidateRevision` 虽不进入 `compiledSemanticIdentity`，但**必须进入 artifact compatibility /
   interchange 判定**；不可互换的 Packed 产物不得仅凭 `compiledSemanticIdentity` 判等价；
   需定义覆盖工具链与载体差异的兼容矩阵，不能只补 `candidateRevision` 一项。
3. **Q-02**：所有合法物理入口必须规范化并恢复为**同一** Canonical Gameplay Graph；
   Chart v5 JSON 区段与 CXC entry 是可选且**可互相替代**的载体，不是并存的语义。
4. **Q-10**：projection 表须分别列明 semantic identity、artifact identity 与 interchange/compatibility
   判定三条，均以实际 prepared value 为准。

**Codex 给出的风险**（原样保留）：仅补 `candidateRevision` 而不定义兼容矩阵，仍可能遗漏工具链或载体差异；
静默补齐会掩盖声明与实际需求的不一致，破坏确定性与能力协商。

**执行顺序裁决**（第 2 次咨询）：Gameplay I 三份文档的标注必须**晚于** ADR 0044 / V2 Spec / V2 ABI 三份
文档的建立与冻结，且每份都要写 `superseded` 状态词并给出替代链接；不得在 V2 Spec 未完成时提前 supersede
（否则出现"已取代但替代不完整"的窗口）。Schema / Replay / Snapshot / diagnostics 属后续契约工件，
**不是**建立替代关系的最低前置。

## 1. 第 1 轮：版本层级、图归属、identity 与 capability（进入 S7A-1 前）

> **owner 裁定（2026-10-02）：本轮 14 行全部接受，接受对象为经 Codex 修订后的文本**（修订内容见 §0.2 与下表
> 各行的 `Codex 修订` 标注）。因此下表"owner 裁定"列统一视为**接受**，不再逐格填写；登记见 §9。

| 编号 | 未决点 | 建议处置 | 不决定的后果 | owner 裁定 |
| --- | --- | --- | --- | --- |
| Q-01 `包建议` + `Codex 修订` | 版本层级未闭合：外层 `version = 5` 与 `gameplay.version = 2` 的唯一性、旧 `1` 的处置、Packed header 是否带 gameplay revision、`candidateRevision` 是否改变 `compiledSemanticIdentity` | 接受"外层 5 + gameplay 2"唯一组合；旧 v1 仅显式离线迁移，否则在最早入口拒绝；Packed header 显式带 gameplay revision；`candidateRevision` **不**改变 `compiledSemanticIdentity`，只改 artifact identity。**Codex 修订**：`candidateRevision` 必须进入 artifact compatibility / interchange 判定，不可互换的 Packed 产物不得仅凭 `compiledSemanticIdentity` 判等价；同时须定义覆盖工具链与载体差异的兼容矩阵（不能只补 `candidateRevision` 一项） | 版本层级不定则 Chart v5 / Packed / CXC 三处无法确定接受面，S7A-1 的类型冻结失去基准；互换性缺口会让"同 identity"被误判为可互换 | |
| Q-02 `包建议` + `Codex 修订` | Canonical Gameplay Graph 没有物理归属 | Chart v5 JSON 区段为**语义视图**，CXC `gameplay-graph` 为**可选物理 entry**。**Codex 修订（表述）**：所有合法物理入口必须规范化并恢复为**同一** Canonical Gameplay Graph；JSON 区段与 CXC entry 是可选且**可互相替代**的载体，不是并存的语义 | Packed section 与 CXC entry 矩阵无法设计；若表述成"并存语义"，会出现两个语义入口 | |
| Q-10 `包建议` + `Codex 修订` | 四类 identity 的字段归属不一致（prepared grace、solver profile、late policy、TimebaseProfile、resourceDecisionPolicyRef、aggregation、Ruleset build hash、几何边界、sourceMap 等） | 建立唯一 identity projection 表，逐字段标注 source / semantic / artifact / judgement / presentation；以"实际生效的 prepared value"为准；凡能改变 Fact Ledger 内容或顺序者必须改变 judgement identity。**Codex 修订**：表中须**分别列明** semantic identity、artifact identity 与 interchange / compatibility 判定三条，均以实际 prepared value 为准 | identity 分区不定则 Replay 无法证明"同一判定"，S7A-6 全量 Snapshot 没有可比对基准；缺 interchange 列则 Q-01 的互换性约束无处登记 | |
| Q-14 `包建议` + `Codex 修订` | capability 派生来源不唯一（Packed META / CXT extension / Chart gameplay / registry / Session 协商 五来源） | source 只声明最低必需 capability；assembler 派生完整 closure；Packed header 保存排序后的 derived closure；CXC manifest 复制 artifact-required capability；Session 只报告四态、不改写 graph。**Codex 修订（原二选一已定）**：声明少于 assembler 派生 closure 时**稳定失败**，**不允许** compiler 静默补齐；失败时点固定在最早可确定 closure 不足的编译 / 装配阶段 | 五来源优先级不定则同一内容可被两个工具解释成不同 capability 集；静默补齐会掩盖声明与实际需求的不一致，破坏确定性与能力协商 | |
| CM-V06 | 与 Q-02 同一未决点（物理归属） | 随 Q-02 裁定，不另设选择 | 同 Q-02 | |
| CM-V07 | 与 Q-01 同一未决点（Packed header 与 semantic identity） | 随 Q-01 裁定，不另设选择 | 同 Q-01 | |
| CM-V13 / P2-01 `新建议` + `Codex 裁决` | "Chart v5 是唯一聚合入口"是**语义唯一**还是**物理入口唯一** | **独立登记**，与 Q-02 同批关闭并引用 Q-02 的裁决；采用**语义唯一、物理入口可多**。Codex 给出的表述：**"Chart v5 是 gameplay typed graph 的唯一聚合语义模型；JSON、CXC 可选 entry 等物理载体必须恢复同一 typed graph"** | 若定为物理唯一，则 CXC `gameplay-graph` 与 Q-02 直接冲突；两处不定则 CXC entry 矩阵自相矛盾 | |
| CM-I06 | 唯一 identity projection 表 | 随 Q-10 裁定；接受 Q-10 即接受建表 | 同 Q-10 | |
| CM-X06 | capability 派生来源唯一性 | 随 Q-14 裁定 | 同 Q-14 | |
| D-1 | `CURRENT_STATUS.md` 曾把四项 Stage 6 交接写成"§2 与 §6"，实为 §1.2（第 95-107 行） | 已按建议订正；请确认该订正不改变任何合同含义 | 不改则后续阶段引用继续指向错误章节 | |
| D-3 `Codex 修订` | 计划引用的"W 类缺口"在仓库内无定义（计划第 641、662、718、805 行） | 采用 [SUPPORT_AND_REJECTION_MATRIX.md](SUPPORT_AND_REJECTION_MATRIX.md) §6 的字段定义（wId/statement/unableLayer/affectedCaseIds/blockedBy/attemptedWorkarounds/stableRejectCode/ownerDecision/reviewDate）。**Codex 修订（落点变更）**：定义落在 **Stage plan**（不落 V2 Spec 附录），并给计划那 4 处引用补定义链接；涉及具体缺口时必须附 `wId`，**不得编造编号** | 计划里的四批验收条件引用无定义术语，无法机械核对 | |
| D-4 `Codex 修订` | `FORMAT_BOUNDARY.md` 的 `gameplay.version = 1` 与 Preliminary Design / Amendment 的 `2` 冲突 | **Codex 修订（强度与位置）**：在该 research proposal 的**顶部**以及 **§5.1/§6** 标注 `gameplay.version = 1` 已被 V2 Spec 的版本合同取代，并链接对应条款；**保留历史研究内容**，不得据此把整篇研究稿标为 superseded | 两个版本号并存，实施者可能按 1 实现；若整篇标 superseded 则过度取代 | |
| D-8 | `cuexis_media_import_tests` 在 active 列表但无 `cuexis_verify_target_dependencies` 调用 | 登记为独立小缺陷，在 S7A-1 的 allowlist 批次内补齐（不属 Gameplay 语义）。**Codex 裁决**：并入 S7A-1 的 allowlist 批次，不单独先落 | 新 target 的依赖 allowlist 门禁对该 target 空缺 | |
| D-10 | 本 package §2.1 的文档动作清单把 `CHART_V5_GAMEPLAY_AMENDMENT.md` 写成 `docs/formats/` 下的路径，实际唯一路径是 `docs/proposals/research/gameplay-v2/CHART_V5_GAMEPLAY_AMENDMENT.md` | 路径引用**已修正**；缺陷本体与 D-5 同属"引用规范"类。**Codex 裁决**：登记为第 10 条缺陷，归入本次准入包复核轮（本表第 1 轮） | 不修则冻结执行时会去改一个不存在的路径，或漏改真正的文档 | |

## 2. 第 2 轮：时间域与迟到策略（进入 S7A-2 前）

> **owner 裁定（2026-10-03，第 2 轮）：本轮 6 行全部接受**（Codex 决策卡 thread `s7a1-freeze-bindings`，
> verdict `adopt`，confidence 0.99，模型 `gpt-6-astra`；补充确认卡 thread `cm-x01-disposition`，verdict
> `adopt`，confidence 0.99，确认 `CM-X01` 保持 `open`、不改任何计数）。**该轮 7 条裁定（含缺陷 F-04）
> 全部登记为阻塞 S7A-2 的门禁**；具体 late-policy 数值、具体业务量程限额、S7C-1 校准扩展与连续输入能力
> 登记为**后续批次阻塞项**（不是 S7A-2 门禁）。带日期的落地证据见
> [第 2 轮门禁报告](../../stage_reports/stages/stage-07/2026-10-03-s7a-2-gate-rulings.md)。

| 编号 | 未决点 | 建议处置 | 不决定的后果 | owner 裁定 |
| --- | --- | --- | --- | --- |
| Q-04 `包建议` | Beat / TimingMap / Tick 无规范映射（七问：舍入、负时间、同 Tick 碰撞、溢出、tempo/stop 影响、时间域语义、pause/seek/reload、commitTick 粒度） | 接受 `TimebaseProfile` 绑定与"作者 RationalBeat → prepare 冻结整数 `judgementTick`"；`chartBeat` / `judgementTick` / `observationTick` 三者分开；TimebaseProfile 与 offset / calibration / late policy 进 judgement identity | 输入无法唯一落到 Tick，Replay 无法复现，S7A-4 的仲裁没有时间基准 | **接受**（2026-10-03）：`ChartTick` / `judgementTick` / `observationTick` / `commitTick` 均为**有符号 64 位整数**，单位由 `engine.tick.us.v1` 的 `TimebaseProfile` 声明；**禁止隐式单位换算**；**溢出稳定拒绝**（不饱和、不截断）。落点：Spec §0.1、§3.7.1；ABI 域 1 与 §单位与量程。provenance：thread `s7a1-freeze-bindings`，verdict `adopt`，confidence 0.99，2026-10-03。**修订指针（2026-10-03 实现复核）**：本节 `stop` 语义由"比例替换"修订为**塌缩 / 跳变 + 精确累计函数**（Spec §3.7.2 第 3 条，`tick(beat) = roundHalfToEven(F(beat))`、只舍入一次、区间内非单射、半开方向正向 `(originBeat, beat]` / 负向 `(beat, originBeat]`），并补迟到参数量级门禁与 Tick → Beat 反查登记（Spec §3.7.4 第 5 条、§3.7.7 表、§9.3）；两张续裁卡见本节表后 provenance 块 |
| CM-T03 | 作者 RationalBeat → `judgementTick` 的 exact mapping 与 tie rule | 随 Q-04 裁定；同 Tick 碰撞的 tie rule 必须写进 Spec 而非留给实现 | 同 Q-04 | **接受**（2026-10-03）：映射使用**精确有理数运算**，**四舍五入到最近整数、半数取偶**；**tempo、stop、负 Beat 使用同一规则**；同 Tick 碰撞按规范键 **`(tick, originKind, canonicalOrdinal)`** 排序，**禁止按 ingress 顺序排序**。落点：Spec §3.7.2。provenance 同上（thread `s7a1-freeze-bindings`，`adopt`，0.99，2026-10-03）。**修订指针（2026-10-03 实现复核，同一 thread 续裁卡 `adopt` 0.98 / 0.99）**：`stop` 模型由"比例替换"修订为**塌缩 / 跳变 + 精确累计函数**——`F(originBeat) = 0`、`F(endBeat) = F(startBeat) + duration`、`tick(beat) = roundHalfToEven(F(beat))`（**只舍入一次**，**不得**写成整数递推 `tick(endBeat) = tick(startBeat) + duration`）；区间内映射**非单射**；半开方向正向 `(originBeat, beat]` / 负向 `(beat, originBeat]`，**有 `stop` 时不再关于 origin 奇对称**（有向端点关系与单调性仍成立）；该语义**属 S7A-2 必须冻结的时间映射语义**。落点：Spec §3.7.2（含 2026-10-03 实现复核修订） |
| CM-T04 | `observationTick` 的来源（设备时间 / 宿主到达时间 / 音频时间 / 校准后会话时间） | 随 Q-04 裁定；与 CalibrationProfile（S7C-1）的耦合点必须显式登记为"7A 固定、S7C-1 可扩展" | 观察时间来源不定则早/晚判定不可复现 | **接受**（2026-10-03）：`observationTick` **唯一**来自输入进入判定管线时捕获的**校准后会话时钟**；设备时间 / 宿主到达时间 / 音频时间 / 渲染帧时间**仅保留为诊断上下文**；**S7C-1 只能扩展 `CalibrationProfile` 参数，不得替换 canonical source**。落点：Spec §3.7.3；ABI 域 1。provenance 同上 |
| `CM-T13` `新登记`（本次登记新增：T 系列下一空号，Codex 2026-10-03 裁决授权新增一条第 2 轮裁决） | 输入域值的量程与量化规则（`DomainAmount` 的取值域、量化 / 离散化规则、越界行为） | 冻结每个 `InputDomain` 的量的表示边界与越界处置（稳定拒绝 + 诊断码）；数值限额不在本批冻结（遵守 plan 关于研究切片值不得成为生产限额的规定） | S7A-2 的“越界量程”负例无判据 | **接受**（2026-10-03）：每个 `InputDomain` 必须携带 typed **`AmountSpec`**，声明 canonical **整数量化**、**scale**、**可表示范围**与**边界策略**；量化使用**精确整数运算**、**最近值半数取偶**；**超范围 / 窄化 / 溢出稳定拒绝并返回诊断码**；**本轮不冻结具体业务量程数值或限额**（后续批次阻塞项）。落点：Spec §3.7.6；ABI 域 2 与 §数值域与量程。provenance 同上。**补充指针（2026-10-03 输入半批复核，同 thread 续裁卡 `adopt` 0.97 / 0.99，处置词与计数不变）**：`AmountSpec` 的 **canonical 整数宽度明文冻结为有符号 64 位**（与 Tick 的 64 位域一致）、**判定顺序＝可表示性先于范围**（可表示但越界 → `amount_out_of_range`；根本不可表示 → `amount_narrowed`；`exactNumerator == INT64_MIN` 按不可表示拒绝）、**域声明集合与完整 `AmountSpec` 字段进 session identity**（Spec §3.7.6 第 5、6 条、§5.2 行；见本节表后 provenance 块与本文件 §9 第 2 轮行） |
| CM-T08 / P1-04 `新建议` | late policy 的 `finalizationWatermark`、最大 queue hop、窗口 open/close、重复排队条件与状态表 | 7A **不冻结数值**：把 watermark、queue hop、窗口 open/close 定义为 typed 参数，由 TimebaseProfile / ruleset 提供；默认值登记为待测量（禁止写成 ABI 常量，计划第 188 行）；重复排队条件定义为稳定拒绝 + 诊断码 | 数值先冻结会把研究切片值变成生产限额，违反计划第 188 行 | **接受**（2026-10-03）：四项参数均为 `TimebaseProfile` / ruleset 提供的 **typed 参数**；默认值**只在 profile registry 登记为 `pending_measurement`**，不得成为 ABI 常量、隐式零值或研究切片限额；**缺失或未测量值在 prepare 稳定拒绝**（`late_policy_incomplete`）；**重复排队稳定拒绝并返回诊断码**。**具体 late-policy 数值**登记为后续批次阻塞项。落点：Spec §3.7.4；ABI 域 1。provenance 同上。**修订指针（2026-10-03 实现复核）**：`validatePrepare` **只**检查参数"已声明且已测量"，**不检查** `finalizationWatermark` / queue hop / 窗口开闭阈值之间的**量级关系**；该量级关系校验登记为**阻塞 S7A-4 的 late-window / 判定消费门禁**，最终数值与限额仍由 **S7A-9** 承载（Spec §3.7.4 第 5 条、§3.7.7 表） |
| P2-03 `新建议` | `commitTick` 的 window / transaction 定义 | 定义 `commitTick` 为事务提交发生的 Tick；window 是相邻两个可提交 Tick 之间的观察区间；写入 V2 Spec 术语表 | 事实排序与 coordination window 的边界无法机械核对 | **接受**（2026-10-03）：`commitTick` 是**事务提交发生的 Tick**；window 是**相邻两个可提交 Tick 之间的观察区间**；**事实只能在 `commitTick` 提交**。落点：Spec §0.1 术语表与 §3.7.5；ABI 域 1。provenance 同上 |

> **本次登记新增（第 2 轮）已裁定。** `CM-T13` 由 Codex 2026-10-03 裁决授权新增（T 系列下一空号；thread
> `s7a1-freeze-bindings`，verdict `adopt`，confidence 0.99），用于承载 `DomainAmount` 的取值域、量化与
> 越界行为裁定；该行已于第 2 轮裁定（`AmountSpec`、精确整数运算、最近值半数取偶、越界稳定拒绝、数值不冻结）。
>
> **缺陷 F-04 已由第 2 轮一并闭合**：ABI 字段级映射表补入 `InputEvent.source` 行，并在输入域小节写明
> `SourceClass` 与 `SourceBuildIdentity` 的分工；证据见
> [第 2 轮门禁报告](../../stage_reports/stages/stage-07/2026-10-03-s7a-2-gate-rulings.md)。
>
> **`CM-X01` 的处置词不变。** 补充确认卡（thread `cm-x01-disposition`，verdict `adopt`，confidence 0.99，
> 2026-10-03）确认 `CONTRACT_MATRIX.md` 的 `CM-X01` **保持 `open`**（第 6 轮），不改任何计数；
> `CM-S02` / `CM-K01` 的 `revise` 口径不适用于该项。
>
> **第 2 轮语义在实现复核后被修订（2026-10-03）。** S7A-2 时基半批实现落地后，同一 Codex 话题
> thread `s7a1-freeze-bindings` 又出两张续裁卡，verdict 均为 `adopt`，confidence **0.98** 与 **0.99**：
> ①7 项语义裁量卡（stop 语义模型、`differenceTicks(INT64_MIN)`、拒绝类别归属、`commitAt` 窗口、
> `validatePrepare` 边界、`roundHalfToEven` 守卫、S1-05 接口收紧）；②4 项收尾卡（分数 duration 的
> **单次舍入**即**精确累计函数** `F`、负向 `(b, origin]` 与"有 stop 时失去奇对称"、缺失 `initialTempo`
> 属**结构类**、`profileId` / `unitToken` **保持字符串且空值表示未声明**）。**修订结论**：`stop` 模型由
> "比例替换"修订为**塌缩 / 跳变 + 精确累计函数**（`tick(beat) = roundHalfToEven(F(beat))`，只舍入一次，
> 区间内非单射），并登记三分法（Spec §9.3）、迟到参数量级门禁（Spec §3.7.4 第 5 条、§3.7.7 表）与
> Tick → Beat 反查阻塞项（Spec §3.7.2 第 6 条、§3.7.7 表）。**修订不改变任何计数**：§7.2 仍 19 条、
> §9.2 仍九类、`open` 仍 14、六类处置词与 114 总数不变。带日期的落地证据见
> [第 2 轮实现裁定记录](../../stage_reports/stages/stage-07/2026-10-03-s7a-2-implementation-rulings.md)。
>
> **第 2 轮的输入规范化语义在实现复核后被修订 / 补充（2026-10-03）。** S7A-2 输入规范化半批实现落地后，
> 同一 Codex 话题 thread `s7a1-freeze-bindings` 再出两张续裁卡，verdict 均为 `adopt`，confidence **0.97**
> 与 **0.99**（文件名时间戳 2026-10-02T20:4xZ（UTC），按第 2、3、4、5 轮先例统一按**本地日期 2026-10-03**
> 登记）：①6 处语义裁量卡（`AmountSpec` canonical 宽度、`same_tick_collision` 类别、不连续表示复用既有码、
> 窄化先于越界、负 tick 与回退类别、域声明进 session identity）；②两处死令牌与措辞对齐卡（删除两个无引用
> 令牌、clock 不可表示统一复用 tick 溢出码、7A 不引入 per-domain 版本字段、64 位原文同步写入 Spec / ABI
> 类型行 / `CM-T13`）。**修订 / 补充结论**：
> - `CM-T09` 的**量化身份**：域声明进 session identity，分量＝`profileId` / `profileVersion` / `sourceClass`
>   ＋规范化后的域 token 集合与每项域声明的完整 `AmountSpec` 字段；域重排不算变化；**7A 不引入 per-domain
>   版本字段**（会话级版本由 `profileVersion` 承载）；**不得**推迟到 S7A-4（Spec §5.2 行）。
> - `CM-T10` 的**四类行为结论**：校准会话时钟回退 → `invalid_relation`；负 `observationTick` 合法；重复
>   sequence / 重复排队 → `late_policy_incomplete`（与同 Tick 同 canonical 身份重入的 `invalid_relation`
>   分开）；不连续表示与连续能力**都复用**既有 `input.continuous_unsupported`（`field.path` 区分，映射
>   既有 **R-05**）（Spec §3.7.3、§3.7.7、§3.7.8、§9.3）。
> - `CM-T13` 的 **64 位冻结**：`AmountSpec` 的 canonical 整数表示为有符号 64 位整数；该宽度是 S7A-2 冻结的
>   规范表示，并与 Tick 的有符号 64 位域一致；任何无法在该域内精确表示的量化、乘法或窄化稳定拒绝；判定
>   顺序为**可表示性先于范围**（Spec §3.7.6 第 5、6 条）。
> - **`time_reversal` 的类别从 `budget_exceeded` 修正为 `invalid_relation`**：时钟回退是**时间次序关系
>   错误**，不是数值预算失败（Spec §3.7.3 第 4 条、§9.3；ABI §时间单位与表达）。
>
> **本次修订 / 补充同样不改变任何计数**：§7.2 仍 **19 条**、§9.2 仍**九类**、`open` 仍 **10**、六类处置词与
> **114** 总数不变；`CM-T13` 仍是**非合同项的裁决项**（不计入 `open` 或末列）。带日期的落地证据见
> [第 2 轮输入半批实现裁定记录](../../stage_reports/stages/stage-07/2026-10-03-s7a-2-input-implementation-rulings.md)。

## 3. 第 3 轮：prepare、装配与 entry（进入 S7A-3 前）

| 编号 | 未决点 | 建议处置 | 不决定的后果 | owner 裁定 |
| --- | --- | --- | --- | --- |
| Q-05 `包建议` | CXT v2 `effects` 与 V2"Requirement 不含 effects"冲突 | 删除 Gameplay 语义内的 Requirement `effects`；非空 `effects` 只能显式 lowering 为 Presentation/Effect Graph；若保留必须声明 presentation-only 且不影响 Replay | 旧字段继续被当作判定输入，S7A-3 的 assembler 无法定义唯一派生 | **修改后接受**（2026-10-03）：Requirement **不含** Gameplay `effects`；非空 `effects` 只能**显式 lowering 为不影响 Judgement/Replay 的 Presentation 数据**，否则**拒绝**。落点：Spec §3.3、§3.8、§7.2；ABI 域 3 `RequirementRecord`。**阻塞 S7A-3**。provenance：thread `s7a3-prepare-rulings`，verdict `need_info`，confidence 0.8，2026-10-03 |
| Q-07 `包建议` | Packed `REQ0`/`CNS0` 兼容边界：新 wire revision 还是新增 section、旧 reader 行为 | 保留 REQ0/CNS0 的 Foundation 语义；7A gameplay 另立 candidate wire revision 或新 section 组；未知必需 section、未知 capability、新 requirement kind 一律稳定拒绝 | 旧 reader 可能把新语义读成旧 kind（计划明确禁止） | **修改后接受**（2026-10-03）：`REQ0`/`CNS0` 保持 Foundation 语义；7A 新语义须使用**可与旧语义区分的 entry/section 边界**；未知必需 section、未知 capability、新 requirement kind **稳定拒绝**。**第 3 轮只冻结行为，不冻结 wire 编号、布局或序列化编码**；具体表示在 S7A-3 消费前另行闭合。落点：Spec §2.5、§3.6、§3.8；ABI §S7A-3 节。**阻塞 S7A-3**。provenance 同上（thread `s7a3-prepare-rulings`，`need_info`，0.8，2026-10-03） |
| Q-18 `包建议` | Release/tail 的 inclusion decision：计划说"仅显式声明时进入"，Preliminary Design 把 Release 列入 7A 最小闭环 | 接受"Release/tail 是同一 Requirement 的可选 phase、**必须显式声明**才启用"，并保留"未声明却要求 tail 语义即拒绝" | 7A 闭环范围不定，S7A-3/S7A-4 的 golden 无法确定 | **接受**（2026-10-03）：Release/tail **仅作为同一 Requirement 显式声明的可选 phase**；内容要求 tail 语义却未声明时**拒绝**。落点：Spec §3.8、§7.2；ABI 域 3 `Phase`。**阻塞 S7A-3**。provenance 同上（thread `s7a3-prepare-rulings`，`need_info`，0.8，2026-10-03） |
| CM-X05 | "W 类缺口"登记表 | 随 D-3 裁定 | 同 D-3 | **`revise`（由 D-3 落地关闭，2026-10-03）**：W 类缺口原无定义；D-3 修订后由 plan §5.3 规定登记与引用规则，SUPPORT §6 定义字段。**D-3 已落地，CM-X05 关闭，不再阻塞 S7A-3**；门禁清单标为"**D-3 已关闭；仅具体 W 缺口在首次消费前须完成登记**"。落点：[CONTRACT_MATRIX.md](CONTRACT_MATRIX.md) §12、[SUPPORT_AND_REJECTION_MATRIX.md](SUPPORT_AND_REJECTION_MATRIX.md) §6。provenance：thread `s7a3-prepare-rulings`，verdict `reject`，confidence 0.88，2026-10-03 |
| CM-C10 / P1-03 `新建议` | 有界循环 canonical form：完全展开 vs `boundedRelationInstance` | 7A 选**完全展开**（`boundedRelationInstance` 不进 7A 公共合同）；展开序、identity、snapshot 与 fact order 由展开结果唯一决定；bounded form 登记为 7B+/S7C 候选并稳定拒绝 | 两者并存会使同一内容有两种 identity 与两种 fact order | **修改后接受**（2026-10-03）：7A 的 prepare-time 有界循环**完全展开**，展开结果**唯一决定** identity / snapshot / fact order；`boundedRelationInstance` **不进 7A 合同**，遇到时**稳定拒绝**，列为 **7B+/S7C 候选**。7A 选择**阻塞 S7A-3**；候选表示只阻塞**首次拟消费它**的后续批次。落点：Spec §3.8、§7.2；ABI 域 3 `PatternPrimitive`。provenance 同上（thread `s7a3-prepare-rulings`，`need_info`，0.8，2026-10-03） |
| P2-10 `新建议` | Release/tail inclusion decision（重述 Q-18） | 随 Q-18 裁定，不另设选择 | 同 Q-18 | **随 Q-18 关闭，不另设选择**（2026-10-03）。落点同 Q-18。provenance 同上（thread `s7a3-prepare-rulings`，`need_info`，0.8，2026-10-03） |

> **第 3 轮（prepare、装配与 entry）已裁定（2026-10-03）。** 三张决策卡均来自同一 Codex 话题
> `s7a3-prepare-rulings`（模型 `gpt-6-astra`，consult）：①主裁定卡 verdict `need_info`、confidence 0.8；
> ②补答 CM-X05 的卡 verdict `reject`、confidence 0.88；③计数一致性确认卡 verdict `adopt`、confidence 0.99。
> 三张卡的文件名时间戳为 2026-10-02T19:4xZ（UTC），本轮统一按**本地日期 2026-10-03** 登记（与第 2 轮先例一致）。
> 带日期的落地证据见
> [第 3 轮门禁报告](../../stage_reports/stages/stage-07/2026-10-03-s7a-3-gate-rulings.md)。
>
> **登记口径。** 上表 6 行的**语义裁定**中，`Q-05`、`Q-07`、`Q-18` / `P2-10`、`CM-C10` / `P1-03` 与
> 规范字节边界（见 §9 第 3 轮行）登记为**阻塞 S7A-3 的门禁**；`CM-C10` 的**候选表示**
> （`boundedRelationInstance`）与 `SUPPORT §6` 的 `W-01…W-05` 候选项只阻塞**首次拟消费它**的后续批次。
> `CM-X05` 由 D-3 落地关闭，**不再阻塞 S7A-3**；门禁清单中它标为"**D-3 已关闭；仅具体 W 缺口在首次消费前
> 须完成登记**"。
>
> **Q-07 的冻结强度（本轮只冻结行为）。** 第 3 轮允许冻结"未知必需 section / 未知 capability / 新
> requirement kind 稳定拒绝"这类**消费与拒绝行为**，以及"新语义必须可与 `REQ0`/`CNS0` 的 Foundation
> 语义区分"这一**边界要求**；**不**冻结 wire 编号、section 布局或序列化编码——具体表示在 S7A-3
> **消费前**另行闭合（见 Spec §3.8 与 §S7A-3 节）。

## 4. 第 4 轮：仲裁、资源与事实序（进入 S7A-4 前）

| 编号 | 未决点 | 建议处置 | 不决定的后果 | owner 裁定 |
| --- | --- | --- | --- | --- |
| Q-03 `包建议` | compile-time solver 与 runtime coordinator 边界矛盾（"Playback 不执行 solver" vs `greedy_v1` 是 7A 能力） | 拆成 compile/prepare solver（展开、验证、算上界、证明唯一性、生成 prepared profile）与 runtime coordinator（只按 prepared profile 做确定性排序与资源检查）；若 `greedy_v1` 仍需逐候选决策，改名 `coordinator.policy.greedy_v1` | 边界不定则 7A 的 solver 语义与 P2-02 的措辞自相矛盾 | **接受**（2026-10-03，`S7A4-R01`）：拆分为 **prepare/compile solver**（展开、验证、上界证明、唯一性证明、生成 prepared profile）与 **runtime coordinator**（只执行 prepared profile 指定的确定性候选排序、资源检查与提交）；runtime coordinator **不解析也不编译 solver**；名称确认采用 **`coordinator.policy.greedy_v1`**，**不把 runtime 行为称为 solver**；`P2-02` 措辞随本裁定改写。**首次消费批次：S7A-4**。落点：Spec §3.12、§S7A-4 节；ABI 域 4 与 §S7A-4 节。provenance：thread `s7a4-arbitration-rulings`，主卡 verdict `adopt`，confidence 0.97，2026-10-03 |
| Q-11 `包建议` | 资源状态机缺 `capacity > 1` 与命名空间合同（`resourceId` 归属、lease/contact identity、`grace = 0`、gap 到期、terminal 范围、observe 是否占资源、同 Tick phase 顺序） | 资源定义为 typed ResourceSlot 或显式 owner set；`capacity = 1` 与 `> 1` 使用不同状态表；命名空间 / slot / lease identity 进 canonical graph；7A 只接受 `capacity = 1`、exclusive、无复杂 handoff | 单一 exclusive resource 是 7A 的核心不变量，不定则无法冻结仲裁顺序 | **修改后接受**（2026-10-03，`S7A4-R02`）：7A 资源子集**冻结为 `free` / `held` / `terminal`**——`free + claim -> held`；`held + 合法 update -> held`；终止后按资源策略进入 `free` 或**永久 `terminal`**。**`gap`、`handoff_pending`、非零 grace、handoff、`capacity > 1`、owner 集合与并列 slot 一律稳定拒绝**；据此把 `CM-C04` 的"只启用前三态"改写为该明确子集，**不再采用 preliminary 状态表的 gap/handoff 分支**。身份规则随 `S7A4-R05` 一并落定。**首次消费批次：S7A-4**。落点：Spec §3.10、§S7A-4 节；ABI 域 4；CONTRACT_MATRIX §5。provenance：thread `s7a4-arbitration-rulings`，主卡 verdict `adopt`，confidence 0.97，2026-10-03 |
| Q-16 `包建议` | 两级求值顺序不匹配：计划固定**六步 Tick 顺序**，Preliminary Design 给出**八步 Coordination window 阶段** | 明确"Tick 六步"是外层、"Coordination window 八步"是第 4 步内部展开；写成阶段对应表并进入 engine identity | 两级顺序无映射则同 Tick 事实顺序不可复现 | **接受**（2026-10-03，`S7A4-R03`）：写成**唯一**映射——**六步＝外层**、**八阶段＝第 4 步内部完整展开**：外层第 1 步激活到期 requirement；第 2 步处理 Release/tail 或 hard-deadline timer；第 3 步规范化并应用输入＝八阶段第 1 步 Observation 收集；第 4 步完整展开八阶段第 2–7 步（生成 Candidate、处理 gap-close/recovery/handoff 分支、求解 `free` 资源、形成并提交 `CoordinationCommit`；**7A 对 gap/recovery/handoff 输入稳定拒绝**）；第 5 步＝八阶段第 8 步由 commit 生成 Fact，随后按 causal total order 排序；第 6 步 Tick 末提交 Hook/signal（下一 Tick 才可见）。阶段顺序与阶段语义进 **`judgement identity` 的 engine 组件**，**不进 chart/content identity**；**S7A-4 只冻结阶段名称、顺序、映射与语义边界**，不冻结阶段编号、序列化编码、预算或窗口数值。`SK §4.2` 的五步变体标注为**非权威推导、已被取代**。**首次消费批次：S7A-4**。落点：Spec §3.9、§S7A-4 节；ABI 域 4 与 §S7A-4 节；[SEMANTIC_KERNEL.md](../research/gameplay-v2/SEMANTIC_KERNEL.md) §4.2。provenance 同上 |
| CM-C05 | 同 Tick 交易阶段顺序 | 随 Q-16 裁定 | 同 Q-16 | **接受**（2026-10-03，`S7A4-R04`）：随 `Q-16` 的唯一映射裁定关闭；两级阶段顺序不再是未决点。处置词 `open` → **`accept`**。**首次消费批次：S7A-4**。落点：Spec §3.9、§S7A-4 节；[CONTRACT_MATRIX.md](CONTRACT_MATRIX.md) §5。provenance 同上 |
| CM-C09 | `resourceId` 命名空间与 `capacity > 1` 语义等 8 问 | 随 Q-11 裁定；`capacity > 1` 在 7A 必须稳定拒绝 | 同 Q-11 | **修改后接受**（2026-10-03，`S7A4-R05`）：随 `Q-11` 裁定关闭；身份规则＝`resourceId` **只在 prepared canonical graph 的资源命名空间内**解释；`capacity = 1` 资源使用**唯一 slot**；slot identity、lease identity、最小 contact handle identity、claim identity **进入 canonical graph / prepared judgement inputs**；lease / contact 由引擎**按规范阶段分配**，`contact end` **不复用**旧 handle；**`observe` 只产生 Observation，不占用、不改变资源 owner**；同 Tick 阶段严格服从 `Q-16` 映射；`capacity > 1`、`gap` / `handoff_pending` / 非零 grace 稳定拒绝。处置词 `open` → **`accept`**。**首次消费批次：S7A-4**。落点：Spec §3.10、§S7A-4 节；ABI 域 4；CONTRACT_MATRIX §5。provenance 同上 |
| P1-12 `新建议` | early/late timing error 与窗口边界（Exact 是否要求零 tick、量化误差、窗口外早击是否产生 Fact、Miss 的 deadline、Hold head/body/tail error） | Exact **不要求零 tick**，按窗口半宽定义；窗口外早击不产生 Fact 但产生诊断；Miss/absence 由 deadline 产生 Fact；Hold 的 head/body/tail error 分别记录、不合并 | 早/晚判定的边界不定则 Fact Ledger 内容不可复现 | **接受**（2026-10-03，`S7A4-R06`）：Exact **由判定窗口半宽定义**而非要求零 tick；**窗口外早击不产生 Fact 但产生诊断**；**Miss/absence 由 deadline 产生 Fact**；Hold 的 head/body/tail **分别记录有符号 error，不合并**。**首次消费批次：S7A-4**。落点：Spec §3.11、§S7A-4 节；ABI 域 5 与 §单位与量程。provenance 同上 |

> **第 4 轮（仲裁、资源与事实序；进入 S7A-4 前）已裁定（2026-10-03）。** 两张决策卡来自同一 Codex 话题
> `s7a4-arbitration-rulings`（模型 `gpt-6-astra`，consult）：①主裁定卡 verdict `adopt`、confidence 0.97
> （代码块"裁决"：采纳第 4 轮全部六项建议并按限定修订——**以六步 Tick 为外层、八阶段为第 4 步内部规范**，
> 7A 仅支持 **`free` / `held` / `terminal`** 的 `capacity = 1` exclusive 资源与 prepare 后确定性 coordinator，
> 并把所有未冻结数值与编码排除在 S7A-4 外）；②处置词与计数确认卡 verdict `adopt`、confidence 0.99
> （`CM-C05` / `CM-C09` 均 `open` → `accept`，最终计数 `open` 14 / `accept` 38，§0.1 末列仍保留两条）。
> 两张卡的文件名时间戳为 2026-10-02T20:0xZ（UTC），本轮统一按**本地日期 2026-10-03** 登记（与第 2、3 轮
> 先例一致）。带日期的落地证据见
> [第 4 轮门禁报告](../../stage_reports/stages/stage-07/2026-10-03-s7a-4-gate-rulings.md)。
>
> **唯一映射声明（本轮的地基）。** **六步＝外层 Tick 顺序；八阶段＝第 4 步的内部展开**。二者不是两套并列
> 顺序，也不存在第二个映射；实现、golden 与 trace 都必须按这一条解释（正文见 Spec §3.9 与 ABI 域 4）。
> 研究稿 [SEMANTIC_KERNEL.md](../research/gameplay-v2/SEMANTIC_KERNEL.md) §4.2 的五步变体**不是**第三种顺序，
> 已标注为**非权威推导、已被 S7A-4 六步外层 / 八阶段内层规范取代**，不得继续作为实现依据。
>
> **归属层级。** 八阶段的顺序与阶段语义进入 **`judgement identity` 的 engine 组件**，**不进入
> chart/content identity**。**S7A-4 只冻结阶段名称、顺序、映射与语义边界**；阶段编号、序列化编码、
> 预算与窗口数值均**不在本轮冻结范围**。
>
> **登记口径（门禁归属）。** `Q-03`、`Q-11`、`Q-16`、`CM-C05`、`CM-C09`、`P1-12` 及 `CM-C06` / `CM-C07`
> 的**语义闭合**登记为**阻塞 S7A-4** 的门禁；**具体预算数值、默认 profile 清单、proof 编码、
> wire/serialization、terminal 编码、后续 gap / handoff / `capacity > 1` 语义**，登记为
> **首次消费它们的后续批次**的阻塞项（分别见 Spec §S7A-4 节与 §11.1、ABI §未决项 #20 与 §S7A-4 节）。
>
> **本轮不改变 `CM-C04`、`CM-C06`、`CM-C07` 的处置词**（依次仍为 `revise` / `revise` / `accept`），只按
> 本轮的资源子集、`SolverProfile` 语义与唯一性证明边界改写它们的未决点文字（见
> [CONTRACT_MATRIX.md](CONTRACT_MATRIX.md) §5）。

## 5. 第 5 轮：Ruleset、事实序、Score 与 Snapshot（进入 S7A-5 / S7A-6 前）

| 编号 | 未决点 | 建议处置 | 不决定的后果 | owner 裁定 |
| --- | --- | --- | --- | --- |
| Q-08 `包建议` | Ruleset 事务失败后状态未定义（是否产生 PresentationEvent、是否追加 fault Fact、faulted 后 advance/seek/snapshot/replay 行为） | 拆三阶段：Fact commit（不可撤回）/ Ruleset state commit（失败则整 Tick 不提交）/ Presentation projection（是否继续由 Fact 重建须显式选择并测试）；session 进入可查询 `faulted` | 失败语义不定则 Score 与 Snapshot 在 fault 后无定义 | **接受**（2026-10-03，`S7A5-R03`）：Ruleset Tick 固定为**三阶段**——①先追加并封存**已排序**的 Fact Ledger；②再**原子校验并提交全部** StateDelta / Score / Combo / Statistics / RuleEffect；③最后**只从已提交 Fact Ledger 与 FactBinding 投影**只读 PresentationEvent。第二阶段失败时**不追加 fault Fact**、**不发布** RuleEffect / PresentationEvent、**保留旧 state**，session 进入可查询 `faulted`；faulted 的 `submit` / `advance` / `seek` / `replay` / **就地 reload** 以及 **snapshot** 一律**稳定失败**；只有**显式 reset** 或**创建替换新 session 的 reload / recovery** 可离开，且**不得恢复或伪造未提交 StateDelta**。**首次消费批次：S7A-5**（snapshot 行为由 S7A-6 消费）。落点：Spec §3.14、§9.4、§10、§S7A-5 节；ABI 域 6 与 §S7A-5 节；`plan.md` §S7A-5 验证段的消歧。provenance：thread `01a0fe49-cfe5-7933-8d03-3d0a65d34c8b`，主裁定卡 verdict `adopt`，confidence 0.95，2026-10-03 |
| `CM-S02` `绑定登记`（本次绑定登记到第 5 轮；裁定栏留空，内容待第 5 轮裁定） | 寄存器类型 `exclusive` / `commutative_monoid` / `ledger_derived` 的命名与冲突策略 | 随 `Q-08` 的 Ruleset 三阶段（Fact commit / state commit / presentation projection）一并裁定：寄存器类型是 state commit 的合成算子分类，命名与冲突策略必须冻结；7A 只接受已冻结的三类 | 未冻结则 Ruleset state commit 的合成算子与冲突处置无判据 | **接受**（2026-10-03，`S7A5-R04`，**处置词保持 `revise`**、只写裁定正文）：`exclusive` **只能有一个声明 owner**，第二 owner 或第二写入**稳定失败**；`commutative_monoid` **只能由已声明贡献者以已声明且可交换、可结合的 combine operator 合成**，未知 operator、**重复 contribution identity**、非交换 / 非结合组合**稳定失败**；`ledger_derived` **禁止直接 StateDelta 写入**，只能从**已提交 Fact Ledger** 确定性重建。**7A 只接受这三类**，其他 kind **稳定拒绝**；Hook 的"下一 Tick 可见"不变。**首次消费批次：S7A-5**。落点：Spec §3.15；ABI 域 6 `RegisterKind`；[CONTRACT_MATRIX.md](CONTRACT_MATRIX.md) §7。provenance 同上（thread `01a0fe49-cfe5-7933-8d03-3d0a65d34c8b`，主卡 `adopt` 0.95） |
| Q-17 `包建议` | 7A 是否包含 `cuexis.ruleset` package 支持 | 7A 只用内置 Ruleset；package 形状保留为 S7C-2 能力，遇到 package 输入稳定拒绝 | 若 7A 含 package，则需同时冻结 package identity 与迁移矩阵，范围显著扩大 | **接受**（2026-10-03，`S7A5-R05`）：7A **只用内置、静态注册的 Ruleset Interface / 模块**；任何 `cuexis.ruleset` package 输入命中**既有 R-09**，诊断 `ruleset.package_unsupported`，**不得读** package hash、manifest 或迁移矩阵；package 形状 / identity / 迁移归 **S7C-2**。**首次消费批次：S7A-5**（S7A-6 的 Replay 亦消费）。落点：Spec §3.16、§7.2 的 R-09；ABI 域 6 与 §S7A-5 节。provenance 同上 |
| CM-S08 | 同 Q-17 | 随 Q-17 裁定 | 同 Q-17 | **随 `Q-17` 关闭**（2026-10-03，`S7A5-R05`）：与 `Q-17` 同一裁定，**不另设选择**——7A 只用内置 Ruleset，package 输入命中 R-09（`ruleset.package_unsupported`），package 形状归 S7C-2。**首次消费批次：S7A-5**；落点同 `Q-17`。provenance 同上 |
| Q-12 `包建议` | Fact 因果总序生成算法未冻结（`originId`/`commitId`/`phasePriority`/`factId` 的生成与编码、seek 后 timer 重新编号、correction 能否影响同 Tick 排序） | 规范 tuple `(originKind, originScope, originOrdinal, localOrdinal)`；scope/ordinal 由引擎基于 canonical window/observation identity 派生，宿主不能提供；phase registry、commit allocation、factId encoding 以 bytes 写入 reference evaluator 并进入 Fact semantic revision | 总序不定则 Replay digest 无法作为判定等价证据 | **接受**（2026-10-03，`S7A5-R01` + `S7A5-R02`）：**`S7A5-R01` 顺序定案**——**唯一**规范总序为 **`(commitTick, originKindPriority, canonicalOrdinal)`**；`canonicalOrdinal` 是引擎从 `(originScope, originOrdinal, localOrdinal, phasePriority, factKindPriority)` 派生的**规范序**；**不得使用 `ingressSequence`、容器顺序或线程完成顺序**；Fact 的 `tick` **即** `commitTick`；`observationTick` 只定位 observation origin 并参与其 `originOrdinal` 派生，**不是**总序首键；`commitTick` 是提交事务的 Tick。修订 Spec §3.9 第 3 条与 `plan.md` §S7A-4 第 5 步（**删除含 `ingressSequence` 的旧 tuple**）。**首次消费批次：S7A-4**。**`S7A5-R02` 身份生成**——`originId` 不透明、由 `(originKind, originScope, originOrdinal)` 唯一确定；`commitId` 按总序单调分配；`factId` = `(commitId, localOrdinal)` 的**语义身份**；`phasePriority` 是 engine phase registry 的**稳定语义 rank**、仅用于 `canonicalOrdinal`、**绝非评分**；timer 由 **prepared timer identity** 派生 ordinal、**seek / replay 不重新编号**；7A correction 仍由 **R-08** 拒绝；生成语义与 phase registry 进 **`FactSemanticRevision`**（属 `JudgementIdentity.engine`）；**物理字节布局、varint / endianness、`FactId` / `CommitId` 编码与 reference-evaluator byte fixture 留 S7A-6**。**首次消费批次：S7A-5**（序列化部分 S7A-6）。落点：Spec §3.9、§3.17；ABI 域 5 / 域 7 与 §S7A-5 节；`plan.md` §S7A-4 第 5 步。provenance：thread `01a0fe49-cfe5-7933-8d03-3d0a65d34c8b`，主卡 `adopt` 0.95，2026-10-03 |
| Q-13 `包建议` | Snapshot/Replay schema 未与 V2 状态闭包对齐 | 7A 采用全量语义 Snapshot；header 含四分量 identity、semantic revision、state schema revision、event/fact count、byte budget；表现缓存不保存；增量 snapshot / 压缩 ledger / 跨 minor 迁移在无全量 golden 前不进公共合同 | Snapshot 字段不定则 S7A-6 无法产出 golden | **接受**（2026-10-03，`S7A5-R06` + `S7A5-R07`）：**全量 Snapshot header 字段集** = `formatVersion`、四分量 `JudgementIdentity`、`factSemanticRevision`、`stateSchemaRevision`、normalized event count、Fact count、payload byte count、typed byte-budget descriptor；**Replay header 字段集** = `formatVersion`、四分量 identity、`NormalizationProfile` metadata、effective late-policy metadata、`eventCodecId`、normalized event count、encoded byte count、`ReplayDecodeBudget`。**拼写规范：类型 `EventCodecId`、字段 `eventCodecId`**。`factSemanticRevision` = Fact / 排序 / 折叠解释语义、**进 engine identity**；`stateSchemaRevision` 只标识 `SnapshotPayload` 无损状态结构、**不进** judgement identity。`SnapshotPayload` 闭包与**不得保存清单**见 Spec §3.18。**首次消费批次：S7A-6**。落点：Spec §3.18；ABI 域 9 与 §S7A-6 节；`plan.md` §S7A-6 第 4 条。provenance 同上 |
| `CM-K01` `绑定登记`（本次绑定登记到第 5 轮；裁定栏留空，内容待第 5 轮裁定） | Replay header 字段集（`formatVersion` / 四分量 identity / normalization profile 元数据 / late policy / `eventCodecId` / 事件数与字节预算） | 随 `Q-13` 的 Snapshot / Replay schema 一并裁定：Replay header 字段集与 `EventCodecId` 随该轮冻结；预算数值仍按 ABI §未决项 #16 留 S7A-9 | 未冻结则 S7A-6 的 Replay golden 无字段基准 | **接受**（2026-10-03，`S7A5-R06`，**处置词保持 `revise`**、只写裁定正文）：Replay header 字段集随 `Q-13` 一并冻结（`formatVersion`、四分量 identity、`NormalizationProfile` metadata、effective late-policy metadata、`eventCodecId`、normalized event count、encoded byte count、`ReplayDecodeBudget`）；**`EventCodecId` 随该轮冻结**，**拼写规范为类型 `EventCodecId`、字段 `eventCodecId`**（修正 ABI 第 377 行与 379 / 680 行的不一致）；**预算数值仍按 ABI §未决项 #16 留 S7A-9**。**首次消费批次：S7A-6**。落点：Spec §3.18；ABI 域 9。provenance 同上 |
| CM-K07 | Snapshot header 字段集 | 随 Q-13 裁定 | 同 Q-13 | **接受**（2026-10-03，`S7A5-R06` + `S7A5-R07`）：全量 Snapshot header 字段集随 `Q-13` 冻结（`formatVersion`、四分量 `JudgementIdentity`、`factSemanticRevision`、`stateSchemaRevision`、normalized event count、Fact count、payload byte count、typed byte-budget descriptor）；`SnapshotPayload` 闭包与不得保存清单见 Spec §3.18。**首次消费批次：S7A-6**。落点：Spec §3.18；ABI 域 9 与 §S7A-6 节。provenance 同上 |
| `CM-K08` `新登记`（本次登记新增；裁定栏留空，内容待第 5 轮裁定） | `SeekLatencyCommitment`（`maxSeekLatency`）的类型、承诺语义、会话收紧关系与测量口径入口 | 第 5 轮冻结类型、承诺语义、会话收紧关系与测量口径入口；数值限额继续登记为 S7A-9 未冻结预算项。在第 5 轮落地前，S7A-6 只保留类型名作追踪项，**不**进入 Snapshot / Replay 字段、运行时约束或对外承诺；提供具体 commitment 值的输入稳定拒绝，seek 本身仍按正确性与可重放契约执行 | 未冻结则 S7A-6 会伪实现 commitment 值——把未测量的 `maxSeekLatency` 写成运行时约束或对外承诺 | **接受**（2026-10-03，`S7A5-R08`）：`SeekLatencyCommitment` = **带 measurement-profile 引用的 typed engine commitment**；语义 = 从**最近可用快照**恢复并推进到目标的 **wall-clock seek latency**；会话可声明**更严格**值、**不得放宽 engine 值**；测量入口 = [BUDGET_AND_EVIDENCE_PLAN.md](BUDGET_AND_EVIDENCE_PLAN.md) §5 的 restore time、Seek p95 / max 与无损性口径。**具体 `maxSeekLatency` 数值仍禁止进入 Schema / header / 运行时约束 / 对外承诺**，提供具体值**稳定拒绝**；seek 仍须满足**无损正确性**。**首次消费批次：S7A-6**（数值首次消费 S7A-9）。落点：Spec §3.19、§10 第 5 条；ABI 域 9 与 §S7A-6 节；`plan.md` §S7A-6 第 5 条。provenance 同上 |
| CM-F06 | `originId`/`commitId`/`factId`/`phasePriority` 的生成与排序字节合同 | 随 Q-12 裁定 | 同 Q-12 | **接受**（2026-10-03，`S7A5-R02`，随 `Q-12` 关闭）：生成语义（`originId` 不透明、`commitId` 单调分配、`factId` = `(commitId, localOrdinal)`、`phasePriority` 仅用于 `canonicalOrdinal` 且非评分、timer ordinal 由 prepared timer identity 派生且 seek / replay 不重新编号、correction 仍由 R-08 拒绝）在本轮冻结；**物理字节布局、varint / endianness、`FactId` / `CommitId` 编码与 reference-evaluator byte fixture 留 S7A-6**。**首次消费批次：S7A-5**（字节合同 S7A-6）。落点：Spec §3.17、§3.9；ABI 域 5 / 域 7 与 §S7A-5 节。provenance 同上 |
| CM-S10 / P1-07 `新建议` | 7A 最小 `outcome`/`category`/`grade` 集合与错误单位、统计 reset/seek 规则 | 7A 最小结果集合为 outcome {hit, miss} 与 Hold 的 head/body/tail 局部结果；category 由 phase 派生；grade 表**可选**，缺表时只报 outcome（不隐式升级）；错误单位统一为有符号整数 tick 差；seek 后统计从 Fact Ledger 重建，reset 清空且不产生 Fact | 无此表时 Replay digest 只能证明输入相同，不能证明 Score 相同 | **接受**（2026-10-03，`S7A5-R09`）：Outcome = **`{hit, miss}`**；Hold head / body / tail 是各自**附属的 phase-local outcome / error**、**不能扩充 Outcome**；`FactCategory` 与 phase **一一对应**，集合 `{tap, hold_head, hold_body, hold_tail}`；grade table **可选**、缺失时 grade 为 **absent**、只报 Outcome，**绝不以 error / category / 默认表隐式升级**；`TimingError` = `observationTick - chartTick` 的**有符号整数 tick 差**；seek / replay 从 **Fact Ledger** 重建统计；reset 清空新 session 状态且**不产生 Fact**。**首次消费批次：S7A-5**。落点：Spec §3.20、§9.4；ABI 域 5 / 域 6。provenance 同上 |
| `CM-S11` `新登记`（本次登记新增；裁定栏留空，内容待第 5 轮裁定） | `LifeState` 在 7A 的关闭状态与稳定拒绝路径 | 7A 关闭 Life：`LifeState` 只保留类型名与拒绝路径，输入该能力必须稳定拒绝，不得用默认值或占位实现 | 未冻结则 Life 的启用条件与边界无判据，且可能被默认值绕过 | **接受**（2026-10-03，`S7A5-R10`，**编号只用于 LifeState**）：Life 在 7A **关闭**；ABI 仅保留 `LifeState` 类型**追踪**与**拒绝说明**；任何 Life capability / Life policy / `LifeState` 初值或占位默认值一律以 **`capability.disabled` 稳定拒绝**，**不创建或更新 `LifeState`**。**首次消费批次：S7A-5**。落点：Spec §3.21；ABI 域 6 `LifeState` 与 §S7A-5 节。[CONTRACT_MATRIX.md](CONTRACT_MATRIX.md) 中**无** `CM-S11` 行，故**不改变任何计数**。provenance 同上 |
| P1-14 `新建议` | resource closure 与 Packed reference closure 的归属表（哪些进 REF0、哪些只进 manifest、哪些只是诊断/source map） | 判定必需引用进 REF0 / 语义闭包；纯表现引用与 source map 只进 manifest / 诊断；归属表随 V2 Spec 附录发布 | 归属不定会让 REF0 混入表现依赖，破坏"判定闭包可独立校验" | **接受**（2026-10-03，`S7A5-R11`，**与 `S7A5-R10` 分开登记**）：`P1-14` 的规则与**具体归属矩阵**本轮作为 [GAMEPLAY_V2_SPEC.md](../../formats/GAMEPLAY_V2_SPEC.md) **附录**冻结——**判定必需引用进 REF0 / judgement closure；纯表现引用进 manifest / presentation closure；source map 仅诊断**；REF0 的物理字段 / 编号 / 编码由 **S7A-3 的首次 Packed 写入**消费；**S7A-7** 负责 entry / manifest 集成。**首次消费批次：S7A-3**（后续集成 S7A-7）；`P1-14` **不阻塞 S7A-5 / S7A-6**。落点：Spec §3.8.6（附录归属表）与 §S7A-3 节引用；ABI §未决项 的指向性引用。provenance 同上 |

> **本次登记与绑定登记（第 5 轮）。** `CM-S11`、`CM-K08` 是**本次登记新增**的裁决项（Codex 卡“新增一条第 5 轮
> 裁决（`LifeState`）”与“新增一条第 5 轮裁决（`SeekLatencyCommitment`）”，thread `s7a1-freeze-bindings`，
> verdict `adopt`，confidence 0.96，2026-10-03）；`CM-S02`、`CM-K01` 是**既有合同项**（`revise`）的**绑定登记**
> ——二者此前在 CONTRACT_MATRIX 与 ABI 中给出编号，但第 1-7 轮表内没有行，即 ABI §登记缺陷 (b) 类的成因。
> 四行的裁定栏在本轮（2026-10-03）按 `S7A5-R04` / `S7A5-R05` / `S7A5-R06` / `S7A5-R08` 填入；`CM-S02`、
> `CM-K01` 的处置词**保持 `revise`**，`CM-S11`、`CM-K08` **不进入** CONTRACT_MATRIX 计数。

> **第 5 轮（Ruleset、事实序、Score 与 Snapshot；进入 S7A-5 / S7A-6 前）已裁定（2026-10-03）。** 三张决策卡
> 来自同一 Codex 话题 thread `01a0fe49-cfe5-7933-8d03-3d0a65d34c8b`（consult）；文件名时间戳为
> 2026-10-02T20:2x–20:3xZ（UTC），本轮统一按**本地日期 2026-10-03** 登记（与第 2、3、4 轮先例一致）：
>
> 1. **主裁定卡**（第 5 轮 `S7A5-R01…R11` 及 `Q-08` / `CM-S02` / `Q-17` / `CM-S08` / `Q-12` / `Q-13` /
>    `CM-K01` / `CM-K07` / `CM-K08` / `CM-F06` / `CM-S10` / `CM-S11` / `P1-14` 的逐条结论）verdict `adopt`，
>    confidence **0.95**；
> 2. **处置词与计数确认卡** verdict `adopt`，confidence **0.99**——`CM-F06`、`CM-S08`、`CM-S10`、`CM-K07`
>    四条 `open` → **`accept`** 并全部**移出** `open` 集合；`CM-S02`、`CM-K01` **保持 `revise`**；计数
>    `open` 14 → **10**、`accept` 38 → **42**（其余不变、总数仍 114）；§0.1 合计行的**末列仍为 21**
>    （该列是"由哪一轮一并关闭"的**历史归属**，不是当前状态）；**`CM-S11`、`CM-K08` 不是 CONTRACT_MATRIX
>    的行**，因此**不改变任何计数**；
> 3. **ABI 状态格首词与追踪计数追问卡** verdict `adopt`，confidence **0.98**——9 行状态格首词**全部保持
>    `待冻结`**，并逐字给出落地文字；**追踪计数 126 = 96 + 30 不变**、9 个域的条目数与逐域分项均不变；
>    **不新增 §未决项 #21、不新增 §类型清单条目**。
>
> 带日期的落地证据见
> [第 5 轮门禁报告](../../stage_reports/stages/stage-07/2026-10-03-s7a-5-6-gate-rulings.md)。
>
> **编号归属修正说明（本轮）。** `S7A5-R01…R09` 按主卡所述的编号与**首次消费批次**登记
> （R01→S7A-4；R02→S7A-5，序列化部分 S7A-6；R03→S7A-5；R04→S7A-5；R05→S7A-5，S7A-6 的 Replay 亦消费；
> R06→S7A-6；R07→S7A-6；R08→S7A-6，数值 S7A-9；R09→S7A-5）。**`S7A5-R10` 只用于 `LifeState`**（首次消费
> **S7A-5**）；**`P1-14` 另登记为 `S7A5-R11`**（首次消费 **S7A-3** 的 REF0 首次写入，后续由 **S7A-7** 做
> entry / manifest 集成）——**不并入 R10**，以免错误宣称 S7A-5 消费 REF0 归属表。
>
> **登记口径（门禁归属）。** `Q-08`、`CM-S02`、`Q-12`、`CM-F06`、`Q-17`、`CM-S08`、`CM-S10 / P1-07` 与
> `CM-S11` 登记为**阻塞 S7A-5** 的门禁；`Q-08`、`Q-12 / CM-F06`、`Q-13 / CM-K01 / CM-K07`、`CM-K08`、
> `CM-S10 / P1-07`、`CM-S11` 与 `Q-17 / CM-S08` 登记为**阻塞 S7A-6** 的门禁；`P1-14` **不阻塞 S7A-5 / S7A-6**，
> 它阻塞 **S7A-3** 的 REF0 首次写入与 **S7A-7** 的 entry / manifest 集成；**全部具体预算数值、Replay /
> Snapshot 字节布局、Event / Fact codec 与 `maxSeekLatency` 数值**登记为**阻塞 S7A-9 或其首次序列化
> 消费者**的后续阻塞项（S7A-5 / S7A-6 各自的不得消费清单见 Spec §3.22 与 ABI §S7A-5 / §S7A-6 节）。
>
> **本轮不改变 `CM-S02`、`CM-K01` 的处置词**（仍为 `revise`），只把裁定正文写入其未决点文字；`CM-S11`、
> `CM-K08` **不进入** CONTRACT_MATRIX，因此**不改变任何计数**；**§7.2 仍为 19 条**——`cuexis.ruleset`
> package 的拒绝映射到**既有 R-09**，**不新增 R 条目、不新增拒绝类别**。

## 6. 第 6 轮：发布粒度、表现桥接与诊断（进入 S7A-7 前）

| 编号 | 未决点 | 建议处置 | 不决定的后果 | owner 裁定 |
| --- | --- | --- | --- | --- |
| Q-06 `包建议` | CXC Gameplay/Ruleset 发布入口未闭合（`playback = true` 的 `entryKind`、`gameplay-graph` 的 closure/预算/拒绝、Ruleset package 是否第三种 entry） | `playback = true` 必须带明确 `entryKind`；7A 允许 `packed-chart` 与 `gameplay-graph` 两种；Ruleset package 只是 prepare 输入，不是第三种 entry；manifest 记录 entry kind、compiled semantic identity、artifact identity、Ruleset binding、capability closure、source-of | CXC 入口不定则 Player/Headless 无法确定加载路径 | **接受**（2026-10-03，`S7A6-R01`；首次消费 **S7A-7**）：`playback = true` 必须显式 `entryKind`；7A **只允许** `packed-chart` 与 `gameplay-graph`；`author-source` **不是** Playback entry（仅审查 / 迁移 / 复现）；**Ruleset package 不是第三种 entry**（与第 5 轮 `S7A5-R05` 的内置 Ruleset 决定一致）；manifest 必须记录 **entry kind、compiled semantic identity、artifact identity、Ruleset binding、capability closure、resource / presentation closure 与 `sourceOf`**。落点：Spec §3.6、§3.23、§S7A-7 节；ABI 域 8。provenance：thread `01a0fe61-3476-7882-9674-5f6b05035237`，verdict `adopt`，confidence **0.97**，2026-10-03 |
| Q-09 `包建议` | FactBinding 与 AnimationSystem 生命周期未闭合（precedence、立即/排队/延迟应用、多 Fact 幂等、seek/reload 后 token 重建、lifetime 终止条件） | 7A 用独立 Gameplay-to-Presentation adapter，把已提交 FactBinding 转成带 source Fact 的 HostOverride token；AnimationSystem 不读 Judgement；seek/replay 从 Fact Ledger 重建 token；precedence 成为命名常量并进入交接测试 | 表现层可能反向影响判定或无法在 seek 后重建 | **接受**（2026-10-03，`S7A6-R03`；首次消费 **S7A-7**）：独立 **Gameplay-to-Presentation adapter**，命名层为 **`GameplayOverride`**，precedence 为 **`Initial < Behavior < Animation < GameplayOverride/FactBinding < StudioPreviewOverride`**（命名常量）；**`render.visible = false` 是该层的显式值**；adapter **只消费已提交 FactBinding**；**当前有效立即应用、未来 `effectivePresentationTick` 排队**；`(factId, targetId)` 去重且重复幂等；**seek / replay / reload 从 Fact Ledger 重建 token**；声明 lifetime 到期或 reset / session replacement 时终止；**表现失败绝不回写 Fact Ledger**。落点：Spec §3.23、§S7A-7 节；ABI 域 8。provenance：thread `01a0fe61-3476-7882-9674-5f6b05035237`，`adopt`，confidence **0.97**，2026-10-03 |
| Q-15 `包建议` | Gameplay / Presentation / Effect 发布粒度不清（缺 Presentation 能否 headless、FactBinding target 缺失如何处理） | 7A 把 Gameplay graph 作为**必需判定闭包**，Presentation/Effect 作为**可选只读投影**；缺 Presentation 允许 headless judgement；缺影响判定的 domain/action/table 必须拒绝；Effect target 缺失不改 Fact Ledger，但需明确诊断与事件丢弃策略 | 粒度不定则 headless 与会渲染两条路径语义分叉 | **接受**（2026-10-03，`S7A6-R04`；首次消费 **S7A-7**）：Gameplay graph 是**必需判定闭包**，Presentation / Effect 是**可选只读投影**；**缺 Presentation 允许 headless judgement**；**判定闭包内 target / domain / action / table 缺失 = prepare 原子失败**（`identity_closure_incomplete` 或 `invalid_relation`）；**纯表现 target 缺失**允许空绑定、**丢弃该投影事件**、给稳定 **`presentation-target-missing`** 诊断，**不 fault session**、**不改 Fact Ledger**。落点：Spec §3.23、§9.7、§S7A-7 节；ABI 域 8。provenance：thread `01a0fe61-3476-7882-9674-5f6b05035237`，`adopt`，confidence **0.97**，2026-10-03 |
| Q-19 `包建议` | 7A 的 Packed/CXC 是否必须携带 `compiledSemanticIdentity` 与 capability closure，以及 `author-source` entry 的用途边界 | 是，两者都必须携带可比较的 compiled semantic identity；`author-source` 只能用于审查/迁移/复现，不构成 Playback entry | 缺少可比较 identity 则无法拒绝"同内容不同语义"的包 | **接受**（2026-10-03，`S7A6-R02`；首次消费 **S7A-7**）：**`compiledSemanticIdentity` 与完整 capability closure 必须同时携带且可比较**；`author-source` **只能**用于审查 / 迁移 / 复现，**不构成 Playback entry**。落点：Spec §3.6、§5.6、§6.4、§S7A-7 节；ABI 域 8。provenance：thread `01a0fe61-3476-7882-9674-5f6b05035237`，`adopt`，confidence **0.97**，2026-10-03 |
| `CM-X01` `新登记`（本次登记新增的**表内行**；编号本身是既有 CONTRACT_MATRIX 合同项，经 D-14 方案 (a) 修正为 `open`） | capability registry 条目字段集（`CapabilityRecord`：`semanticKind` / `requiredFormat` / `staticBudget` / `snapshotCost` / `replayImpact` / `supportedDomains` / `stableRejectCode`） | 与 `Q-15` / `Q-19` 同批裁定：冻结 registry 条目字段集与 capability closure 的携带要求；首次消费 S7A-7。D-14 已按方案 (a) 处置——以 [GAMEPLAY_V2_ABI.md](../../api/GAMEPLAY_V2_ABI.md) 第 237 行的 `待冻结` 为准，并把 [CONTRACT_MATRIX.md](CONTRACT_MATRIX.md) 第 211 行的 `accept` 修正为 `open` | S7A-7 的 capability registry 门禁无字段基准；`CapabilityRecord` 的轮次归属无法确定 | **接受**（2026-10-03，`S7A6-R02`，与 `Q-19` 同批；首次消费 **S7A-7**）：冻结 `CapabilityRecord` 七字段**及逐字段 identity 归属**——`semanticKind`（能力语义种类，进 **semantic projection**）、`requiredFormat`（所需格式与 revision，进 **semantic projection**）、`staticBudget`（静态预算描述，进 **closure 与 interchange compatibility**，**不进** judgement semantic hash）、`snapshotCost`（快照成本描述，进 **closure / interchange**，**不进** semantic hash）、`replayImpact`（Replay 影响枚举，进 **semantic projection**）、`supportedDomains`（域集合，进 **semantic projection**）、`stableRejectCode`（稳定字符串拒绝码，**只进诊断码表 / closure**，**不进 identity**）；**`compiledSemanticIdentity` 与完整 capability closure 必须同时携带且可比较**。处置词 `open` → **`accept`**（经 D-14 方案 (a) 由 `accept` 修正为 `open` 后，本轮再由本项裁定转回 `accept`）。落点：Spec §5.6、§6.4、§9.6、§S7A-7 节；ABI 域 7 / 域 8；[CONTRACT_MATRIX.md](CONTRACT_MATRIX.md) §9（`open` → `accept`）。provenance：thread `01a0fe61-3476-7882-9674-5f6b05035237`，`adopt`，confidence **0.97**，2026-10-03（D-14 的方案 (a) 归属另见 thread `s7a1-freeze-bindings`，`adopt`，confidence 0.96） |
| CM-P04 | `render.visible = false` 的 gameplay override 与 Behavior/Animation/HostOverride 的 precedence 命名常量 | 随 Q-09 裁定 | 同 Q-09 | **随 `Q-09` 裁定**（2026-10-03，`S7A6-R03`；首次消费 **S7A-7**）：precedence 命名为 **`Initial < Behavior < Animation < GameplayOverride/FactBinding < StudioPreviewOverride`** 常量；`render.visible = false` 是 `GameplayOverride` 层的**显式值**。处置词 `open` → **`accept`**。落点：Spec §3.23、§S7A-7 节；ABI 域 8；[CONTRACT_MATRIX.md](CONTRACT_MATRIX.md) §8（`open` → `accept`）。provenance：thread `01a0fe61-3476-7882-9674-5f6b05035237`，`adopt`，confidence **0.97**，2026-10-03 |
| CM-P06 / P1-13 `新建议` | Presentation target 缺失策略 | 与 Q-15 一致：判定闭包内悬空**必须失败**；纯表现 target 缺失允许空绑定 + 稳定诊断 | 两种倾向并存会让同一内容在 headless 与渲染下判定不同 | **接受**（2026-10-03，`S7A6-R04`；首次消费 **S7A-7**）：**判定闭包内 target / domain / action / table 缺失 = prepare 原子失败**（`identity_closure_incomplete` 或 `invalid_relation`）；**纯表现 target 缺失**允许**空绑定**、**丢弃该投影事件**并给稳定 **`presentation-target-missing`** 诊断，**不 fault session**、**不改 Fact Ledger**。处置词 `open` → **`accept`**。落点：Spec §3.23、§9.7、§S7A-7 节；ABI 域 8；[CONTRACT_MATRIX.md](CONTRACT_MATRIX.md) §8（`open` → `accept`）。provenance 同上（thread `01a0fe61-3476-7882-9674-5f6b05035237`，`adopt`，0.97，2026-10-03） |
| CM-P08 / P1-11 `新建议` | `aggregation` 三值 `any`/`all`/`groupCommit` 的幂等、去重键与重建算法 | 去重键 = `(factId, targetId)`；同键重复应用幂等；部分 group 失败时已提交部分不回滚但产生诊断并进入稳定拒绝路径；correction 只允许影响未提交窗口 | 重复 Hit / Miss 后 late Hit / 部分 group 失败的行为不定则表现不可复现 | **接受**（2026-10-03，`S7A6-R05`；首次消费 **S7A-7**）：去重键 = **`(factId, targetId)`**；`any` / `all` / `groupCommit` **均幂等**；`groupCommit` **部分提交不回滚**，记稳定 **`partial-group`** 诊断并**进入稳定拒绝路径**；**correction 仅能影响未提交窗口**。处置词 `open` → **`accept`**。落点：Spec §3.23、§9.7、§S7A-7 节；ABI 域 8；[CONTRACT_MATRIX.md](CONTRACT_MATRIX.md) §8（`open` → `accept`）。provenance 同上（thread `01a0fe61-3476-7882-9674-5f6b05035237`，`adopt`，0.97，2026-10-03） |
| CM-D04 / P1-09 `新建议` | 诊断层级 `code`/`category`/`severity`/`source path`/`identity component`/`recoverability`/faulted 行为 | 统一为四层：稳定 `code`（字符串）+ `category`（枚举）+ `severity` + `faulted` 行为；`source path` 与 `identity component` 是上下文而非稳定码；码表集中为一份机器可读文件并注册为 CTest 校验项 | 未统一前不得写入公共头（也就无法冻结 ABI） | **接受**（2026-10-03，`S7A6-R06`；首次消费 **S7A-7**）：采用 §9.1 已冻结的**四层诊断模型** + §9.2 **九类 category** + §9.4 **`faulted` 行为**；**集中码表**落在 **`schemas/cuexis.gameplay-diagnostics.v2.codes.json`**，并新增一个 **CTest 校验项**（校验：code 唯一性、category / severity / faulted 映射完整性、`stableRejectCode` 已登记、**输入 / 几何三码不可重复扩展**）；**除已冻结的 `input.continuous_unsupported`、`input.direction_unsupported`、`geometry.inference_rejected` 及既有 R-09 映射外，诊断码字符串须经集中码表登记与 CTest 校验后才能进入公共 ABI**。处置词 `open` → **`accept`**。落点：Spec §9.1、§9.2、§9.6、§S7A-7 节；ABI 域 8、§错误与诊断映射；[CONTRACT_MATRIX.md](CONTRACT_MATRIX.md) §10（`open` → `accept`）。provenance 同上（thread `01a0fe61-3476-7882-9674-5f6b05035237`，`adopt`，0.97，2026-10-03） |
| P1-08 `新建议` | ABI 类型与 canonical model 的映射（`InputEvent.observationTime`、`JudgementFact.error`、`JudgementResult.eventSequence`） | ABI 只暴露 canonical 单位：`observationTime` 由 tick + 原始时间戳两个字段分离表达；`error` 用有符号整数 tick 差；`eventSequence` 为会话内单调序号；不做隐式单位换算 | 隐式换算会让 ABI 与 canonical 语义出现第二套时间定义 | **接受**（2026-10-03，`S7A6-R07`；首次消费 **S7A-7**）：`observationTime` 拆为 **`observationTick` + 原始时间戳**两个字段分离表达，**`observationTick` 仍唯一来自校准会话时钟**；`error` 为**有符号整数 tick 差**；`eventSequence` 为**会话内单调序号**；**不做隐式单位换算**。落点：Spec §3.24、§S7A-7 节；ABI 域 1 / 域 2 / 域 5。provenance 同上（thread `01a0fe61-3476-7882-9674-5f6b05035237`，`adopt`，0.97，2026-10-03） |
| P2-04 `新建议` | `phase`/`category`/`outcome`/`grade`/`localOutcome`/`reasonCode` 的层级与互斥 | 单一定义表：`phase`/`category` 为分类层，`outcome`/`grade` 为结果层，`localOutcome` 只用于 Hold 段分解，`reasonCode` 只解释拒绝/降级；同层互斥、跨层可组合 | 层级不定则结果字段可被多重解释 | **接受**（2026-10-03，`S7A6-R08`；首次消费 **S7A-7**）：`phase` / `category` 为**分类层**，`FactCategory` 与 phase **一一对应** `{tap, hold_head, hold_body, hold_tail}`；`outcome` **仅** `{hit, miss}`；`grade` **可选**、缺失即 **absent**（不隐式升级）；`localOutcome` **仅**用于 Hold 段分解；`reasonCode` **仅**解释拒绝 / 降级；**同层互斥、跨层可组合**。落点：Spec §3.25、§S7A-7 节；ABI 域 5。provenance 同上（thread `01a0fe61-3476-7882-9674-5f6b05035237`，`adopt`，0.97，2026-10-03） |
| P2-05 `新建议` | `action = press\|release\|update\|absence\|step` 与 `requiredAction`/`domain`/`InputDomain` 的关系 | `action` 是输入侧动词集合；`requiredAction` 是 requirement 侧声明；`domain`/`InputDomain` 是作用域绑定；三者不互相推导 | 关系不定则 7A 的输入映射与 requirement 匹配出现二义 | **接受**（2026-10-03，`S7A6-R09`；首次消费 **S7A-7**）：`action`（**输入侧动词集合**）、`requiredAction`（**requirement 侧声明**）、`domain` / `InputDomain`（**作用域绑定**）三者**独立、不互相推导**。落点：Spec §3.25、§S7A-7 节；ABI 域 2 / 域 3 / 域 5。provenance 同上（thread `01a0fe61-3476-7882-9674-5f6b05035237`，`adopt`，0.97，2026-10-03） |
| P2-06 `新建议` | `resources` 既像 claim intent 又像全局定义 | 拆为 `resourceRef`（引用）/ `claimPolicy`（申请策略）/ resource record（全局定义）三项，分别归属不同层 | 不拆则 claim 语义与资源定义混在同一字段 | **接受**（2026-10-03，`S7A6-R10`；首次消费 **S7A-7**）：`resources` 拆为 **`resourceRef`**（引用）/ **`claimPolicy`**（申请策略）/ **resource record**（全局定义）三项，**分别归属不同层**；资源三分法**沿用 S7A-4 已冻结的 `free` / `held` / `terminal` 子集**。落点：Spec §3.25、§3.10、§S7A-7 节；ABI 域 4。provenance 同上（thread `01a0fe61-3476-7882-9674-5f6b05035237`，`adopt`，0.97，2026-10-03） |
| P2-08 `新建议` | Effect Graph / EffectEvent / RuleEffectEvent / PresentationEvent 的词汇表与 Snapshot/Replay 归属 | Effect Graph 与 RuleEffectEvent 属规则集投影；PresentationEvent 属表现投影；`EffectEvent` 作为共同基类名应限定为投影内部名；**只有 Fact Ledger 进 Replay/Snapshot**，三类投影事件都不进 | 词汇不定则 Replay 可能被要求携带表现事件 | **接受**（2026-10-03，`S7A6-R11`；首次消费 **S7A-7**）：**只有 Fact Ledger 进 Replay / Snapshot**；`RuleEffectEvent` / `PresentationEvent` / `EffectEvent` **均为投影**；**`EffectEvent` 只是投影内部名**。落点：Spec §3.25、§3.18、§S7A-7 节；ABI 域 9。provenance 同上（thread `01a0fe61-3476-7882-9674-5f6b05035237`，`adopt`，0.97，2026-10-03） |
| P2-09 `新建议` | "未知字段默认拒绝"与 CXC `extensions` 保留 inspection metadata 的区分 | 语义字段一律"未知即拒绝"；`extensions` 只允许命名空间化的 inspection metadata，且必须证明不影响判定 | 区分不清则出现绕过版本门禁的隐式扩展通道 | **接受**（2026-10-03，`S7A6-R12`；首次消费 **S7A-7**）：**语义字段一律"未知即拒绝"**；`extensions` **只允许命名空间化**的 inspection metadata，且**必须证明不影响判定**；输入 / 几何三码**不可重复扩展**（既有 `input.continuous_unsupported`、`input.direction_unsupported`、`geometry.inference_rejected` 保持唯一）。落点：Spec §3.25、§9.6、§S7A-7 节；ABI 域 8。provenance 同上（thread `01a0fe61-3476-7882-9674-5f6b05035237`，`adopt`，0.97，2026-10-03） |

> **本次登记新增行（第 6 轮）。** `CM-X01` 的表内行为本次登记新增（编号本身是既有 CONTRACT_MATRIX 合同项，
> 经 D-14 方案 (a) 修正为 `open`）；与 `Q-15` / `Q-19` 同批，首次消费 S7A-7。
>
> **第 6 轮登记块（发布粒度、表现桥接与诊断；2026-10-03）。**
>
> **provenance。** 单张决策卡，Codex 话题 thread `01a0fe61-3476-7882-9674-5f6b05035237`（consult，模型
> `gpt-6-astra`）：verdict **`adopt`**，confidence **0.97**。卡的文件名时间戳为 2026-10-02T20:51:47.167Z
> （UTC），按**本地日期 2026-10-03** 登记（与第 2、3、4、5 轮先例一致）。带日期的落地证据见
> [第 6 轮门禁报告](../../stage_reports/stages/stage-07/2026-10-03-s7a-6-gate-rulings.md)。**本轮不改写
> §1–§5 与 §7 表中 2026-10-02 的原始证据文字**，只在 §6 本轮的"owner 裁定"列写入裁定正文，并在
> §9 第 6 行登记。
>
> **登记口径。** 上表 15 行的语义裁定**全部登记为 accepted**（逐行裁定词见"owner 裁定"列：`Q-15` 与
> `CM-P06 / P1-13`、`CM-P08 / P1-11` 记**接受**，`Q-09` 与 `CM-P04` 记**接受 / 随 `Q-09` 裁定**，
> `CM-D04 / P1-09` 记**接受**，其余各行均记**接受**），**每条的首次消费批次一律为 `S7A-7`**；`P2-06` 的
> 资源三分法**沿用 S7A-4 已冻结的 `free` / `held` / `terminal` 子集**（不新引入资源状态）。本轮**只冻结
> 文档合同与门禁**，**不得**写成"Judgement 实现完成"或"Stage 7A 完成"。
>
> **编号映射（已确认，2026-10-03）。** 卡只要求"在 §6 登记 `S7A6-R01…R12`"，未给出编号到行的逐行对应；
> 下表映射由"12 个编号 / 15 行"的成组关系推导得出（编号按 §6 表的**行出现顺序**推进，两个编号合并
> 同一行的情形不减号；`Q-19` 与 `CM-X01` 合并为 `S7A6-R02`，`CM-P04` 随 `Q-09` 计入 `S7A6-R03`，
> `CM-P06 / P1-13` 计入 `S7A6-R04`，`CM-P08 / P1-11` 计入 `S7A6-R05`，`CM-D04 / P1-09` 计入 `S7A6-R06`）。
> **该映射已由口径确认卡确认**（thread `01a0fe61-3476-7882-9674-5f6b05035237`，verdict `adopt`，
> confidence **0.99**，2026-10-03；"建议动作"第 1 条"确认并保留映射"），**不再是待确认的推导**：
>
> | 编号 | 行（§6 表内编号） | 首次消费 |
> | --- | --- | --- |
> | `S7A6-R01` | Q-06 | S7A-7 |
> | `S7A6-R02` | Q-19 + CM-X01 | S7A-7 |
> | `S7A6-R03` | Q-09 + CM-P04 | S7A-7 |
> | `S7A6-R04` | Q-15 + CM-P06 / P1-13 | S7A-7 |
> | `S7A6-R05` | CM-P08 / P1-11 | S7A-7 |
> | `S7A6-R06` | CM-D04 / P1-09 | S7A-7 |
> | `S7A6-R07` | P1-08 | S7A-7 |
> | `S7A6-R08` | P2-04 | S7A-7 |
> | `S7A6-R09` | P2-05 | S7A-7 |
> | `S7A6-R10` | P2-06 | S7A-7 |
> | `S7A6-R11` | P2-08 | S7A-7 |
> | `S7A6-R12` | P2-09 | S7A-7 |
>
> **行 / 项口径（已确认）。** 上表为 **15 行**；按合并行各记 2 项计为 **17 项**（`CM-P06 / P1-13`、
> `CM-P08 / P1-11`、`CM-D04 / P1-09` 三行为合并行），即**第 6 轮 = 15 行 / 17 项**（口径确认卡第 2 条：
> 原卡的"15 项"实指**行数**，登记值应为 **15 行 / 17 项**）。`Q-19` 是 §2 的追加未决项、`CM-X01` 是
> CONTRACT_MATRIX 合同项，两者合并为 `S7A6-R02` 登记。**§6 表本轮共 15 行**，其第 1 列为
> **12 个单编号行**（`Q-06`、`Q-09`、`Q-15`、`Q-19`、`CM-X01`、`CM-P04`、`P1-08`、`P2-04`、`P2-05`、
> `P2-06`、`P2-08`、`P2-09`）与 **3 个合并行**（`CM-P06 / P1-13`、`CM-P08 / P1-11`、`CM-D04 / P1-09`）；
> 卡内"上述 15 条阻塞 S7A-7"的措辞按**行**计，本表据此登记，按**项**计则为 17 项。
>
> **`P2-03`。** 本卡**未涉及** `P2-03`，本轮**不动**该行（它已在第 2 轮裁定登记于 §2，见 §2 表 `P2-03` 行）；
> 口径确认卡第 1 条确认 **`P2-03` 不属于第 6 轮、维持第 2 轮已裁定状态**。

## 7. 第 7 轮：收尾澄清与缺陷

| 编号 | 未决点 | 建议处置 | 不决定的后果 | owner 裁定 |
| --- | --- | --- | --- | --- |
| P1-01 `新建议` | 判定域的 typed contract：判定几何是否属 Gameplay closure、坐标系与量程声明、与 Presentation transform 的隔离、未来动态 frame | 7A 只接受**静态 typed 判定域记录**；几何属 Gameplay closure，坐标系统一在判定域内声明量程；与 Presentation transform 完全隔离（不得继承或依赖）；动态 frame 登记为 7B+ 候选并稳定拒绝 | 几何若依赖表现 transform，则判定随视口变化，破坏可复现性 | **接受**（2026-10-03，`S7A7-R01`；首次消费 **S7A-3**）：**7A 只接受静态 typed 判定域记录**；**几何属 Gameplay closure**，坐标系统一在判定域内声明量程；**与 Presentation transform 完全隔离**（不得继承、不得依赖）；**动态 frame 登记为 7B+ 候选并稳定拒绝**。**该文本按本节"建议处置"列的给定文本冻结为 S7A-3 的准入门禁**（照抄工作表文本，不自创）。落点：Spec §3.8 的 S7A-3 准入门禁段、§5.2 判定域与判定几何行、§S7A-3 节、`## S7A-3 限定冻结范围与登记规则（2026-10-03）` 与 `plan.md` §S7A-3 的准入门禁引用句。provenance：thread `01a0fe61-b7a3-7853-a380-0793fee14e14`，verdict `adopt`，confidence **0.96**，2026-10-03 |
| P1-02 `新建议` | CXT local relation 的全局合并规则（namespace、稳定 ID、同名合并、跨 invocation 引用、重排规范化） | 合并在 prepare 编译期完成；稳定 ID = 来源文档 identity + 声明序号；**同名不合并而是拒绝**；跨 invocation 引用必须显式；重排不改变合并结果（按稳定 ID 排序） | 合并规则不定则同一 CXT 有两种展开结果 | **接受**（2026-10-03，`S7A7-R02`；首次消费 **S7A-3**）：**合并在 prepare 编译期完成**；**稳定 ID = 来源文档 identity + 声明序号**；**同名不合并而是拒绝**；**跨 invocation 引用必须显式**；**重排不改变合并结果**（按稳定 ID 排序）。**该文本按本节"建议处置"列的给定文本冻结为 S7A-3 的准入门禁**。落点：Spec §3.8 的 S7A-3 准入门禁段、§3.8.6 归属表引用、§S7A-3 节与 `## S7A-3 限定冻结范围与登记规则（2026-10-03）`。provenance 同上 |
| P1-05 `新建议` | `input.trajectory.v1` 的 schema、量化、断点后 contact 生命周期、区域边界、采样失败处置 | 属连续输入，**7A 整体拒绝**；schema 保留为 S7B-1 准入项；7A 只接受离散 press/release/update/absence/step | 若在 7A 半支持轨迹，会与 19 条拒绝清单冲突 | **接受**（2026-10-03，`S7A7-R03`；**阻塞 S7B-1**）：`input.trajectory.v1`（schema、量化、断点后 contact 生命周期、区域边界、采样失败处置）**属连续输入，7A 整体稳定拒绝**；其 schema **保留为 S7B-1 的准入项**；**7A 只接受离散 `press` / `release` / `update` / `absence` / `step`**。**登记为阻塞 S7B-1 的准入项**（不是 7A 的未决阻塞）。落点：Spec §7.2 的既有连续输入拒绝条目（**不新增 R 条目**）、plan §S7B-1 的准入项。provenance 同上 |
| P1-06 `新建议` | Ruleset package 的执行与分发边界（entry kind、package identity、版本迁移、缺包行为、prepare 时机） | 随 Q-17：登记为 S7C-2 准入项，7A 遇 package 输入稳定拒绝；entry kind 位置保留但不解析 | 同 Q-17 | **接受**（2026-10-03，`S7A7-R04`；**阻塞 S7C-2**）：随第 5 轮 `Q-17` 裁定——**登记为 S7C-2 的准入项**；**7A 遇 package 输入稳定拒绝**；**entry kind 位置保留但不解析**（Ruleset package 不是第三种 Playback entry，与第 6 轮 `S7A6-R01` 一致）。**登记为阻塞 S7C-2 的准入项**。落点：Spec §3.16 Ruleset 输入边界、§7.2 的既有 R-09 映射、plan §S7C-2 的准入项。provenance 同上 |
| P1-10 `新建议` | 预算分层与 Packed/Runtime 对齐、新增成本的归属与最坏值计数入口 | 沿用 [BUDGET_AND_EVIDENCE_PLAN.md](BUDGET_AND_EVIDENCE_PLAN.md) 的五类 profile；7A 只登记计数与上界，不冻结限额；归属按"谁产生谁计数"；最坏值入口作为 S7A-9 证据项 | 预算归属不清会把研究值变成隐含生产限额 | **接受**（2026-10-03，`S7A7-R05`；首次消费 **S7A-9**）：**沿用 [BUDGET_AND_EVIDENCE_PLAN.md](BUDGET_AND_EVIDENCE_PLAN.md) 的五类 profile**（内容 / 稳态 / 快照与 Seek / Replay 与解码 / Packed wire）；**7A 只登记计数与上界、不冻结限额**；**归属按"谁产生谁计数"**；**最坏值入口作为 S7A-9 的证据项**。**禁止在 S7A-9 之前冻结任何限额**。落点：Spec §8.2 计数口径登记段与新增的 `P1-10` 五类 profile / 归属 / 计数上界段、`plan.md` §S7A-9。provenance 同上 |
| P1-15 `新建议` | Chart v5 inline 与 CXT v2 emission 的等价性定义与逐字段 semantic diff | 等价 = prepare 后 canonical graph 与 derived capability closure 逐字段相等；忽略 sourceMap 与物理顺序；逐字段 diff 作为 S7A-3 的 golden 证据 | 无等价定义则两条 authoring 路径无法互相校验 | **接受**（2026-10-03，`S7A7-R06`；首次消费 **S7A-3**）：**等价 = prepare 后 canonical graph 与 derived capability closure 逐字段相等**；**忽略 `sourceMap` 与物理顺序**；**逐字段 diff 作为 S7A-3 的 golden 证据**。**该文本按本节"建议处置"列的给定文本冻结为 S7A-3 的准入门禁**。落点：Spec §3.5 图级等价性、§3.8 的 S7A-3 准入门禁段、§S7A-3 节、`## S7A-3 限定冻结范围与登记规则（2026-10-03）`。provenance 同上 |
| P2-02 `新建议` | "Playback 不执行 solver"的措辞 | 采纳给定替换文本："不解析/编译 solver；runtime coordinator 只执行已准备的确定性 policy" | 措辞不改会与 Q-03 的拆分直接矛盾 | **接受**（2026-10-03，`S7A7-R07`；首次消费 **S7A-4**）：采纳给定替换文本"**不解析/编译 solver；runtime coordinator 只执行已准备的确定性 policy**"；**同步到** [GAMEPLAY_V2_SPEC.md](../../formats/GAMEPLAY_V2_SPEC.md) §3.12 及相关的**不得消费清单**、`plan.md` 的 **S7A-4 边界段**、[GAMEPLAY_V2_ABI.md](../../api/GAMEPLAY_V2_ABI.md) 的 **solver / coordinator 段及其状态格**（**状态格首词一律不变**）。该措辞已由第 4 轮 `S7A4-R01` 确认改写方向，本轮补齐逐处落地。落点：Spec §3.12、§3.13；ABI 域 4；plan §S7A-4 第 4 轮裁定后的范围收窄段。provenance 同上 |
| P2-07 `新建议` | `sourceMap` / source-build identity / content identity 的重叠 | 统一为三项：source closure / diagnostic map / content-artifact identity，各自独立命名与归属 | 三者混用会让 identity 表出现重复条目 | **接受**（2026-10-03，`S7A7-R08`；首次消费 **S7A-3** 的 identity 表与 REF0 首次写入）：**统一为三项命名**——**`source closure`** / **`diagnostic map`** / **`content-artifact identity`**，各自独立命名与归属。**`P1-14` 的 REF0 / manifest / diagnostic 归属仍以 Spec §3.8.6 为准**（不因本项改名而改归属）；ABI **保留** `SourceBuildIdentity`、`CompiledSemanticIdentity`、`ArtifactIdentity` **三行**、**不新增类型**、**维持 9 域 / 126 = 96 + 30**。落点：Spec §5（§5.1 表结构命名与 §5.2 逐字段归属、§6.5 四类闭包）、ABI 域 7 三行语义文字。provenance 同上 |
| D-2 / D-7 | "六动词命令循环"三方命名不一致（ADR 0042：`open/play/pause/seek/reload/quit`；计划：`load/play/pause/stop/seek/reload`；实现：`open/play/pause/tick/seek/reload/quit`） | 以实现与 ADR 为准修改计划措辞（见 [STAGE6_HANDOVER_LEDGER.md](STAGE6_HANDOVER_LEDGER.md) §3）；不改写 ADR 与实现 | 交接回归的验收对象无法确定 | **接受**（2026-10-03，`S7A7-R09`；**文档订正，不阻塞实现**）：计划措辞改为"**`open/play/pause/seek/reload/quit` 为 ADR 0042 的六动词；实现另有 `tick` 运行时步骤**"；**不改 ADR 0042、不改实现**。落点：`plan.md` §1.2 第 3 行与 §S7A-8 第 3 条、§S7A-9 与 §3.1 台账中的同族表述。provenance 同上 |
| D-5 | Alignment Review 通篇只用描述性指代、不引用 ADR/格式文档编号 | 规定后续审查文档必须带编号引用，否则无法机械核对权威关系 | 同类引用缺陷会复发 | **接受**（2026-10-03，`S7A7-R10`；**文档订正，不阻塞实现**）：记录为"**后续审查文档必须使用 ADR / 格式编号引用**"（描述性指代不算可机械核对的引用）。落点：本表 D-5 行、本轮报告《对齐与缺陷处置》段。provenance 同上 |
| D-6 | Stage 6 交付报告对 `tools/check_stage6_a2.py` 的行号引用写 `:252`，实际在 `:259` | 在引用处加订正注记，不改写历史报告正文 | 后续核对者会读到错误行号 | **接受**（2026-10-03，`S7A7-R11`；**文档订正，不阻塞实现**）：记录为"**在引用处注明 `tools/check_stage6_a2.py` 的实际行号 `:259`**"；**不改写历史报告正文**（`2026-10-02-*` / `2026-10-03-*` 带日期报告一律不改）。落点：本表 D-6 行、本轮报告的证据指针段。provenance 同上 |
| D-9 | 版本门禁 shell 守卫不覆盖 WSL 启动器 `bash.exe` | 采用 Codex 裁决（`adopt`，confidence 0.98）：视图一致性探针 + 显式拒绝 `System32\bash.exe` + 独立负例；候选补丁已在工作区、**未提交**；剩余动作是按 ADR 0042 具名复核后落到 `master`，再于 S7A-8 消费 | 默认 PATH 下 `cuexis_contract_version_gate` 仍会假失败 | **接受（owner-only）**（2026-10-03，`S7A7-R12`；首次消费 **S7A-8**）：候选补丁已在工作区且**未提交**，须由 **owner 按 ADR 0042 具名复核后落到 `master`**；**在该动作完成前不得宣称 S7A-8 的版本门禁闭合**。台账表述见 [BATCH_GATES.md](BATCH_GATES.md) 的「第 7 轮裁定后的收尾门禁（2026-10-03）」与本轮报告。provenance 同上（候选补丁的裁决 provenance 另见 [D-9 候选补丁与验证证据](../../stage_reports/stages/stage-07/2026-10-02-d9-shell-guard-candidate.md)） |
| D-11 `新发现` | 稳定 C ABI 的阶段归属不一致：`AGENTS.md` 第 21、260 行写 Stage 12，而 `ROADMAP.md` 第 49 行、Stage 7A plan 第 836 行、ADR 0043 第 120 行、Gameplay I ABI 第 8 行与 Spec 第 148 行都写 Stage 14 | 以 **Stage 14** 为准（阶段计划 + ROADMAP + ADR 一致，`AGENTS.md` 为孤例）；登记为文档一致性缺陷，本次**不改** `AGENTS.md`（受 `check_docs.py` 片段校验约束、不属冻结范围），由 owner 决定订正时点 | 两个阶段号并存会让后续 ABI 文档各引其一 | **接受**（2026-10-03，`S7A7-R13`；**文档订正，不阻塞实现**）：以 **Stage 14 为唯一阶段号**；**本轮不改 `AGENTS.md`**；在 [CURRENT_STATUS.md](../../CURRENT_STATUS.md)、[ROADMAP.md](../../ROADMAP.md)、[ADR 0043](../../adr/0043-gameplay-judgement-ruleset-convergence.md) **各加一句"稳定 C ABI 唯一归属 Stage 14"**，并注明 `AGENTS.md` 的 Stage 12 为**孤例**、由 **owner 择时订正**。落点：上述三处新增句、本表 D-11 行与本轮报告。provenance 同上 |
| D-12 `新发现` | Gameplay I 被标 `superseded` 后，研究稿里仍有把 I 当作现行权威的指向：`docs/proposals/research/gameplay/README.md`:8-10、`GAMEPLAY_FOLD_CALCULUS_DRAFT.md`:3、`GAMEPLAY_RULESET_DISCUSSION.md`:14/:665、`GAMEPLAY_STRESS_TEST_DRAFT.md`:16-18 仍写"以 I 收敛工作稿为准／已被其吸收" | 第 7 轮只改这些**指向行**（改指 ADR 0044 / V2 Spec / V2 ABI，并注明 I 已 `superseded`），**不改写研究论证正文**；`FORMAT_BOUNDARY.md` 的同类指向已在 D-4 落地时同步 | 读者被引向已被取代的合同；若连研究正文一起改写则构成过度取代 | **接受（待 owner 确认）**（2026-10-03，`S7A7-R14`；**文档订正，不阻塞实现**）：**本轮只登记、不编辑**那 8 处指向行（`docs/proposals/research/gameplay/README.md`:8-10、`GAMEPLAY_FOLD_CALCULUS_DRAFT.md`:3、`GAMEPLAY_RULESET_DISCUSSION.md`:14 / :665、`GAMEPLAY_STRESS_TEST_DRAFT.md`:16-18）；处置文本为"**改指 ADR 0044 / V2 Spec / V2 ABI 并注明 Gameplay I 已 `superseded`，不得改写论证正文，须先取得 owner 对历史稿修改的确认**"。**"待 owner 确认"是本项的明确登记状态**：在该确认取得前不得落笔改指向行。落点：本表 D-12 行、[OPEN_QUESTIONS.md](OPEN_QUESTIONS.md) §4.5、[BATCH_GATES.md](BATCH_GATES.md) 的收尾门禁节。provenance 同上 |
| D-13 `新发现` | 格式门禁在 HEAD 曾为红：`cuexis_format_check` 失败，CI 的 `Check formatting` 步骤在 HEAD 必然失败；156 条 clang-format 违规全部来自 `tools/research/gameplay_fold_spike/` 的 `continuity.cpp`(25)、`continuity_report.cpp`(128)、`differential_report.cpp`(3)。登记核实时 HEAD = `449e864`，三文件与 HEAD 一致（`git diff --quiet HEAD -- tools/research/gameplay_fold_spike` exit 0）；违规先于 S7A-1 批次存在。附带子项：`AGENTS.md`:110 的 glob 描述与实现不符 | **已处置（owner 2026-10-03 裁定候选处置 (a)）**：对这 3 个文件执行 `clang-format -i`（clang-format 22.1.3）→ 已执行，改动在工作区、未提交；`cuexis_format_check` 由 **exit 1（156 条违规）→ exit 0（0 条）**。**"仅格式改动"的决定性证据**：去除全部空白后比较，`continuity.cpp` 与 `differential_report.cpp` 与 HEAD **完全一致**，`continuity_report.cpp` 仅比 HEAD 多出恰好 **7** 处 `""` 相邻字面量拆分标记（HEAD 0 处、工作区 7 处），移除这 7 处标记后与 HEAD 完全一致——即 `BreakStringLiterals` 把超长字面量拆成相邻字面量，C++ 相邻字面量拼接后字符串值不变，**零语义变更**。未采用 (b)/(c)，`CUEXIS_FORMAT_FILES`、CMake 与工作流均未改动。**遗留子项（2026-10-03 owner 裁定修正并已执行）**：`AGENTS.md`:110 已改为准确描述（glob **递归**覆盖 `app/`、`engine/`、`tests/`、`tools/` 与 `cmake/*.hpp.in`，并注明该 target 仅在 `CUEXIS_BUILD_DEVELOPER_TOOLS` 打开时存在、且会扫到不被任何 CMake target 编译的文件）。以下记的是**修正前**的状态：`AGENTS.md`:110 写 "Files are globbed from `app/`, `engine/`, `tests/`, and `cmake/*.hpp.in`"，遗漏 `tools/`；且 `tools/research/gameplay_fold_spike/` **无 `CMakeLists.txt`、不被任何 CMake target 编译**（该目录名在 `CMakeLists.txt`、`cmake/*.cmake`、`tools/*/CMakeLists.txt` 中 0 命中，全仓仅 4 处文档引用命中）却仍被 `cuexis_format_check` 扫描，构成"文档描述 ≠ 实际门禁范围" | 修复前：hosted 四作业的 `Check formatting` 步骤在 HEAD 一律失败，任何验证都无法给出绿色结论（该即时阻塞已随修复解除） | **接受**（2026-10-03，`S7A7-R15`；**只补最终证据指针，无遗留实现子项**）：保留候选处置 (a) 的裁定；补齐"已处置"的**最终证据指针**——三个 spike 文件 `clang-format -i` 已做（`cuexis_format_check` 由 **156 条违规 → 0**，exit 1 → 0，改动在工作区、未提交），`AGENTS.md`:110 的 glob 描述**已订正**（递归覆盖 `app/`、`engine/`、`tests/`、`tools/` 与 `cmake/*.hpp.in`，target 仅在 `CUEXIS_BUILD_DEVELOPER_TOOLS` 打开时存在）。**无遗留实现子项。** 落点：本表 D-13 行与本轮报告的 `D-13` 证据指针段。provenance 同上 |
| D-14 `新发现` | 合同项 `CM-X01` 在两份台账中处置相反：[CONTRACT_MATRIX.md](CONTRACT_MATRIX.md) 第 211 行记处置 `accept`、未决"无"，而 [GAMEPLAY_V2_ABI.md](../../api/GAMEPLAY_V2_ABI.md) 第 237 行记 `待冻结（CM-X01）`，同文件 `### 登记缺陷` 的 (b) 类又把该行列为"给出编号但未绑轮次"（第 524-526 行）；`CM-X01` 在本表第 1-7 轮任何表中都没有行（2026-10-03 处置后已在本表第 6 轮补登记该行） | **只登记，不选处置**；候选处置（并列列出，均未选择）：(a) 以 ABI 的 `待冻结` 为准，指定 `CM-X01` 的轮次归属并修正 CONTRACT_MATRIX 的 `accept`；(b) 以 CONTRACT_MATRIX 的 `accept` 为准，把 ABI 第 237 行改判为已冻结并给出裁定时间与证据；(c) 登记为阻塞项，交 capability 相关批次处理。详见 [OPEN_QUESTIONS.md](OPEN_QUESTIONS.md) §5 的 D-14 行。**处置已定（Codex，2026-10-03）**：采纳方案 **(a)**——以 ABI 第 237 行的 `待冻结` 为准，把 `CM-X01` 登记到第 6 轮（与 `Q-15` / `Q-19` 同批）、首次消费 S7A-7，并修正 CONTRACT_MATRIX 的 `accept`；provenance：thread `s7a1-freeze-bindings`，verdict `adopt`，confidence 0.96 | `CapabilityRecord`（ABI:237）的轮次归属无法确定，而 plan §S7A-1 第 220-227 行的角色清单直接点名该行，会卡住 S7A-1"按域计数、无遗漏"的退出判据 | **接受**（2026-10-03，`S7A7-R16`；**只补最终证据指针，无遗留实现子项**）：保留方案 (a)（`CM-X01` 已由第 6 轮 `S7A6-R02` 绑定、首次消费 S7A-7，`CONTRACT_MATRIX` 的处置词已由第 6 轮转回 `accept`）；补齐"已处置"的**最终证据指针**——第 6 轮登记块与 `S7A6-R02` 行、[CONTRACT_MATRIX.md](CONTRACT_MATRIX.md) §12 `CM-X01` 行与 §14 第 6 轮计数变化条、本轮报告的 `D-14` 证据指针段。**无遗留实现子项。** 落点：本表 D-14 行与本轮报告。provenance 同上 |

> **第 7 轮登记块（收尾澄清与缺陷；2026-10-03）。**
>
> **provenance。** 第 7 轮由两张决策卡裁定，均来自 Codex（consult，模型 `gpt-6-astra`）：①**主卡**
> thread `01a0fe61-b7a3-7853-a380-0793fee14e14`，verdict **`adopt`**，confidence **0.96**；
> ②**口径确认卡**（同议题小追问：三处计数 / 编号口径）thread `01a0fe61-3476-7882-9674-5f6b05035237`，
> verdict **`adopt`**，confidence **0.99**。卡的文件名时间戳为 2026-10-02T20:52:37.188Z 与
> 2026-10-02T21:08:34.383Z（UTC），统一按**本地日期 2026-10-03** 登记（与第 2、3、4、5、6 轮先例一致）。
> 带日期落地证据见
> [第 7 轮收尾裁定与缺陷处置记录](../../stage_reports/stages/stage-07/2026-10-03-s7a-7-wrapup-rulings.md)。
> **本轮不改写 §1–§6 表中 2026-10-02 / 2026-10-03 的原始证据文字**，只在 §7 本轮的"owner 裁定"列写入裁定
> 正文、在 §6 的映射推导处补"已确认"、并在 §9 第 7 行登记。
>
> **编号与首次消费批次（16 行 → `S7A7-R01…R16`）。**
>
> | 编号 | 行 | 裁定词 | 首次消费批次 |
> | --- | --- | --- | --- |
> | `S7A7-R01` | P1-01 | 接受 | **S7A-3**（准入门禁） |
> | `S7A7-R02` | P1-02 | 接受 | **S7A-3**（准入门禁） |
> | `S7A7-R03` | P1-05 | 接受 | **阻塞 S7B-1** |
> | `S7A7-R04` | P1-06 | 接受 | **阻塞 S7C-2** |
> | `S7A7-R05` | P1-10 | 接受 | **S7A-9**（只登记计数与上界，**禁止**在 S7A-9 前冻结限额） |
> | `S7A7-R06` | P1-15 | 接受 | **S7A-3**（准入门禁 / golden 证据） |
> | `S7A7-R07` | P2-02 | 接受 | **S7A-4**（措辞统一） |
> | `S7A7-R08` | P2-07 | 接受 | **S7A-3**（identity 表与 REF0 首次写入） |
> | `S7A7-R09` | D-2 / D-7 | 接受 | 文档订正（不阻塞实现） |
> | `S7A7-R10` | D-5 | 接受 | 文档订正（不阻塞实现） |
> | `S7A7-R11` | D-6 | 接受 | 文档订正（不阻塞实现） |
> | `S7A7-R12` | D-9 | 接受（owner-only） | **S7A-8**（owner 复核落 `master` 前不闭合） |
> | `S7A7-R13` | D-11 | 接受 | 文档订正（不阻塞实现） |
> | `S7A7-R14` | D-12 | 接受（**待 owner 确认**） | 文档订正（不阻塞实现） |
> | `S7A7-R15` | D-13 | 接受 | 只补最终证据指针（无遗留实现子项） |
> | `S7A7-R16` | D-14 | 接受 | 只补最终证据指针（无遗留实现子项） |
>
> **行 / 项口径（第 7 轮 = 16 行 / 18 项）。** 上表为 **16 行**；按合并行与附带子项各记 2 项计为
> **18 项**：`D-2 / D-7` 是**合并行**（同一行同时裁定两个编号，计 1 行 / 2 项），`D-13` 的
> **`AGENTS.md`:110 glob 描述附带子项**是**独立的处置对象**（格式修复与 glob 描述订正各自独立，
> 计为独立项）。因此 **第 7 轮 = 16 行 / 18 项**。逐轮折算据此自洽：第 1 轮 14 行 / 15 项、第 2 轮
> 6 行 / 7 项、第 3 轮 6 行 / 7 项、第 4 轮 6 行 / 6 项、第 5 轮 13 行 / 14 项、第 6 轮 **15 行 / 17 项**、
> 第 7 轮 **16 行 / 18 项** ⇒ 逐轮合计 **76 行 / 84 项**，与 §0.1 的绑定合计**逐字一致**。
> 第 6 轮门禁报告中"**76 行 / 83 项**"是当时按"第 7 轮 16 行 / 17 项"误推所致；**该带日期的报告正文不改写**，
> 订正只以 [ADR 0044](../../adr/0044-gameplay-v2-semantic-kernel.md) 的**指针说明**与本轮报告承载
> （口径确认卡第 3 条与"建议动作"第 4 条）。
>
> **五条 `CM-V*` 处置词补齐说明（第 7 轮）。** 第 1 轮已裁定 `CM-V06`、`CM-V07`、`CM-V13`、`CM-I06`、
> `CM-X06` 五项（§0.1 第 1 轮末列 5 项即此五项、§9 第 1 轮行亦已登记），但 [CONTRACT_MATRIX.md](CONTRACT_MATRIX.md)
> 中它们长期仍为 `open`，与 §0.1 / §8 构成自相矛盾。第 7 轮按第 1 轮裁定**补齐处置词与裁定正文**：
> 五条 `open` → **`accept`**，`open` 行由 5 降为 **0**，最终
> **`accept` 52 / `revise` 26 / `supersede` 17 / `retain` 17 / `open` 0 / `reject` 2 = 114**；
> §0.1 末列**历史 21 不变**（末列是历史归属列，不随处置词变化）。落点：CONTRACT_MATRIX §1 / §3 / §9 / §12 的
> 五条行、§0 统计、§5 同步段与 §14 计数变化条。
>
> **本轮不新增任何计数项。** §7.2 仍 **19 条**（不新增 R 条目）、§9.2 仍**九类**、ABI 仍 **9 域 /
> 126 = 96 + 30**、§未决项条目数不变；`P2-07` 的命名统一**不新增 ABI 类型行**。本轮**只冻结文档合同与
> 门禁**：**不得**写成"Judgement 实现完成"或"Stage 7A 完成"——实现批次（S7A-3…S7A-9）仍未完成。

> **缺陷 F-04：已闭合（第 2 轮，Codex 2026-10-03）。** 本表第 7 轮**不新登记 F-04 行**（它来自
> [S7A-1 typed contract review](../../stage_reports/stages/stage-07/2026-10-02-s7a-1-typed-contract-review.md)
> §6 的缺陷台账，不是本表第 1-7 轮的编号行）。闭合方式与落点：在
> [GAMEPLAY_V2_ABI.md](../../api/GAMEPLAY_V2_ABI.md) 的**字段级映射表**新增原文行
> `| InputEvent.source | SourceClass（规范事件中的设备/来源类别）；SourceBuildIdentity 只承载生产者、参数、compiler profile 与 source map 的构建来源，不进入判定语义 |`，
> 并在该文件的输入域小节（域 2 `SourceClass`、域 7 `SourceBuildIdentity`）写明分工；F-04 是 S7A-2 的**文档
> 准入门禁**。provenance：thread `s7a1-freeze-bindings`，verdict `adopt`，confidence 0.99，2026-10-03。

## 7A. 第 1 轮补充：S7A-1 准入闭合（与原 7 轮分开计数）

**性质。** 本块是**咨询裁决 + owner 已接受**，不是 Codex 自行生效的裁决：Codex 咨询
（`gpt-6-astra`，hard/medium，verdict `reject`，confidence 0.94，2026-10-02，thread
`s7a1-admission-rulings`）否决直接启动 S7A-1；**owner 于 2026-10-02 接受其修订后的 5 条文本**，
并按 §9 追加登记。咨询输入是
[Stage 7A typed contract review](../../stage_reports/stages/stage-07/2026-10-02-s7a-1-typed-contract-review.md)
§7"条件性通过"结论中"必须先裁定的最小集合"表列出的 4 项无轮次归属事项（冻结完成判据、冻结范围、
模块/target 决策、`InputDomain` 与 `Tick` 的时点冲突）。本块的三份 V2 文档指
[ADR 0044](../../adr/0044-gameplay-v2-semantic-kernel.md)、
[Gameplay V2 Spec](../../formats/GAMEPLAY_V2_SPEC.md)、
[Gameplay V2 ABI](../../api/GAMEPLAY_V2_ABI.md)。

| 编号 | 未决点 | 处置（Codex 修订后，owner 2026-10-02 接受） | 解锁/影响 |
| --- | --- | --- | --- |
| S1-01 | 冻结顺序第 2 步"冻结"缺完成判据：三份 V2 文档均 `candidate` 且自称冻结前不授权，但第 3 步（标 Gameplay I `superseded`）已经执行 | 将第 2 步明确为**分批冻结**：本次只冻结 S7A-1 的权威来源、类型角色、模块边界及既有所有权/异常承诺；Schema、Replay、diagnostics 与后续轮次字段**不视为已冻结**。三份 V2 文档保留 `candidate`，另列同一基线编号、冻结范围、未冻结项及其阻塞批次，并把"冻结前不授权"改为**范围精确的授权**。同步 `plan §1.1`、ADR 0044 门禁与接受记录；把"P0 全部裁定"改为"**本批次依赖的 P0 全部裁定**" | 判据：三份文档对本次可实施范围表述一致，范围内没有未裁定依赖 |
| S1-02 | 执行顺序偏差与替代关系复核：Gameplay I 的标注发生在 V2 文档冻结之前 | 在接受记录追加**带真实日期**的偏差与纠正记录，承认标注提前发生，**不倒填**冻结时间；限定基线冻结后重新核验旧文档替代链接与 retained 映射，并为 typed review 添加关闭复核。保留原报告与历史标注证据，**不宣称**原执行顺序合规 | 判据：形成"当前基线冻结 → 替代关系复核 → typed review 关闭"的可追溯记录 |
| S1-03 | S7A-1 冻结范围未界定：plan 列 17 项角色 vs V2 ABI 类型清单 9 域 126 条 | 以 **ABI 九域 126 条为完整追踪目录**，以 plan 现行列出的角色及生命周期骨架所需依赖为 **S7A-1 实施范围**；既不以旧 17 项作为封闭清单，也不一次冻结 126 条的全部表示。为每条登记"本批次角色/边界冻结"或"登记为后续批次阻塞项"，后者必须关联裁决编号与首次消费批次。S7A-1 内只实现已经裁定的接口与状态行为；未决部分仅保留不承诺布局的不完整类型或文档声明 | 判据：126 条无遗漏；实际代码涉及的每个字段、签名和行为都有已冻结依据 |
| S1-04 | 模块位置与 target 的五项决策无轮次归属 | 接受方案 A：新建 `engine/judgement/` 内部 STATIC target `cuexis_judgement`；保留现有 `cuexis_gameplay` stub 与 Runtime 依赖、不做别名指向；本阶段不拆 `cuexis_input`，以独立头文件分区与架构检查约束输入层；采用安装选项 1——不安装判定公共头、不新增公开组件或 Playback 方法、本批次不触发 SDK API 升版；`cuexis_judgement` 加入 `CUEXIS_STATIC_IMPLEMENTATION_TARGETS`，沿用仅静态包导出内部 archive、无头文件的机制；接受 `tools/gameplay_assembler/` 与 `tools/chart_candidate/` 名称，但工具实现仍属后续批次 | 判据：五项均有确定结果；退出时 active-target 清单、allowlist、架构检查与静态/shared consumer 验证通过，安装树没有判定头文件 |
| S1-05 | 时点冲突：`InputDomain` 属第 6 轮（P2-05）、`Tick` 宽度属第 2 轮（CM-T03/T04），但 plan 要求 S7A-1 冻结它们 | S7A-1 只冻结 `InputDomain` 的**角色与时间域分离**，不冻结枚举集、Tick 存储宽度、转换与 tie rule；P2-05 保持第 6 轮、CM-T03/T04 保持第 2 轮，并明确登记为各自消费批次的阻塞项。S7A-1 **禁止**用默认枚举值、临时整数 typedef、序列化编码或伪成功实现绕过这些阻塞；受影响的按值字段、运算与可运行方法必须移出本批次交付范围。同步缩减 plan 中依赖这些语义的生命周期与验收要求 | 判据：骨架可独立编译，其实现与测试均不依赖上述未决表示或行为 |

上表"Codex 修订后"的出处为 Codex 咨询（`gpt-6-astra`，hard/medium，verdict `reject`，
confidence 0.94，2026-10-02，thread `s7a1-admission-rulings`）；owner 于 2026-10-02 接受其修订后的
5 条文本。本块**不**改写 §0.1 统计行、第 1-7 轮的表、§8 原合计行与 §9 原有行。

**计数（与原 7 轮分开计数）。**

- 原 7 轮：**76 行 / 84 项**（口径与 §0.1 合计行、§8 原合计行一致，已含第 7 轮 D-13、D-14 与本次绑定登记的 6 行）。
- 本补充块：**5 行 / 5 项**（S1-01..S1-05，无合并行、无重复登记）。
- 含补充总计：**81 行 / 89 项**（其中补充 5 行）。

**最小开工集合（Codex 卡）。** 补充裁决确认并登记 → 三份 V2 文档的限定基线冻结 →
范围 / target / 时点条款同步 → 替代关系复核 + typed review 关闭记录 →
`python -B tools/check_docs.py` 与 `git diff --check` 通过；全部满足后**仅**解锁上述受限 S7A-1。
D-8（`cuexis_media_import_tests` 补 `cuexis_verify_target_dependencies`）留在该批次 allowlist 的实施
与退出验收中，不提前落地。

**风险（Codex 卡，三条）。**

1. 只改状态或补一条"已冻结"记录而不明确授权范围，会留下相互矛盾的开工门禁；
2. 角色级冻结只授权受限骨架，若保留原计划全部可运行生命周期要求，就必须提前闭合相关后续语义裁决，
   不能靠占位实现交付；
3. typed review 报告的部分行号与 F-01/F-02 描述已落后于现行 plan 文本，关闭复核必须对照现行文本。

## 7B. S7A-1 待冻结条目绑定登记（Codex，2026-10-03）

**provenance。** thread `s7a1-freeze-bindings`；verdict `adopt`；confidence 0.96；日期 2026-10-03。
本块登记 [GAMEPLAY_V2_ABI.md](../../api/GAMEPLAY_V2_ABI.md) 的 16 条“缺可追溯阻塞批次”的 `待冻结` 条目的
**最终绑定**：16 条全部具备“裁决编号 + 首次消费批次”，**未绑定条目为 0**；ABI 的 `§登记缺陷` 随之关闭。
本块只登记编号与批次，不复制提案正文；详细依据、附加限定、分组与风险见
[FREEZE_BLOCKING_BINDINGS.md](FREEZE_BLOCKING_BINDINGS.md)。

| ABI 行 | 条目 | 裁决编号（轮次） | 首次消费批次 |
| --- | --- | --- | --- |
| 67 | `TickSpan` / `TickDelta` | `Q-04` / `CM-T03`（第 2 轮） | S7A-2 |
| 85 | `ContactRef` | `Q-11` / `CM-C09`（第 4 轮） | S7A-4 |
| 86 | `DomainAmount` | `CM-T13`（第 2 轮，本次登记新增） | S7A-2 |
| 110 | `Measure` | `CM-S10` / `P1-07`（第 5 轮） | S7A-4 |
| 132 | `ResourceLease` | `Q-11` / `CM-C09`（第 4 轮） | S7A-4 |
| 143 | `CandidateId` | `Q-16` / `CM-C05`（第 4 轮） | S7A-4 |
| 144 | `LocalOutcome` | `CM-S10` / `P1-07`（第 5 轮） | S7A-4 |
| 182 | `RegisterKind` | `CM-S02`（第 5 轮，绑定登记） | S7A-5 |
| 186 | `ScoreState` | `Q-08` + `CM-S10` / `P1-07`（第 5 轮） | S7A-5 |
| 187 | `ComboState` | `Q-08` + `CM-S10` / `P1-07`（第 5 轮） | S7A-5 |
| 188 | `LifeState` | `CM-S11`（第 5 轮，本次登记新增） | S7A-5 |
| 189 | `StatisticsSnapshot` | `CM-S10` / `P1-07`（第 5 轮） | S7A-5 |
| 237 | `CapabilityRecord` | `CM-X01`（第 6 轮，与 `Q-15` / `Q-19` 同批） | S7A-7 |
| 270 | `EventCodecId` | `Q-13`（第 5 轮），`CM-K01` 绑定同一轮次 | S7A-6 |
| 271 | `ReplayDecodeBudget` | `Q-13`（第 5 轮） | S7A-6 |
| 275 | `SeekLatencyCommitment` | `CM-K08`（第 5 轮，本次登记新增） | S7A-6 |

**跨条目边界（同卡裁决）。** S7A-4 只产生并消费 `phase`、phase-ordering token、单分量量化 tick error、
`hit` / `miss` 及 Hold 的最小 phase-local outcome；`phasePriority` 仅作确定性事实序排序键，**不是**评分结果；
`category`、`grade`、Score / Combo / Life / Statistics 统一由 S7A-5 消费和冻结。

## 8. 覆盖核对

| 类别 | 数量 | 本清单分布 |
| --- | --- | --- |
| `open` 合同项（CONTRACT_MATRIX §14） | 0 | 第 1 轮 **0**、第 2 轮 **0**、第 3 轮 **0**、第 4 轮 **0**、第 5 轮 **0**、第 6 轮 **0**、第 7 轮 **0**（`open` 集合为空） |
| Q-01..Q-19（P0） | 19 | 第 1 轮 4、第 2 轮 1、第 3 轮 3、第 4 轮 3、第 5 轮 4、第 6 轮 4 |
| P1-01..P1-15 | 15 | 第 2 轮 1、第 3 轮 1、第 4 轮 1、第 5 轮 2、第 6 轮 4、第 7 轮 6 |
| P2-01..P2-10 | 10 | 第 1 轮 1、第 2 轮 1、第 3 轮 1、第 6 轮 5、第 7 轮 2 |
| D-1..D-14 | 14 | 第 1 轮 5、第 7 轮 9 |
| `CM-T13` / `CM-S11` / `CM-K08`（本次绑定登记新增裁决项，非合同项） | 3 | 第 2 轮 1、第 5 轮 2 |
| `CM-S02` / `CM-K01`（本次绑定登记的既有 `revise` 合同项，非 `open`） | 2 | 第 5 轮 2 |
| 合计 | **84** | 76 行 + 8 个合并行（每行含两个编号） |
| 第 1 轮补充 S1-01..S1-05 | 5 | 第 1 轮补充 5 |

**计数口径。** 上表“合计 **84**”仍是**原 7 轮**口径（76 行 + 8 个合并行），不含第 1 轮补充；
第 1 轮补充在 §7A 单独成块、单独计数（**5 行 / 5 项**）。含补充的总计为 **89 项 / 81 行**
（其中补充 5 行）。两个数字口径不同、互不矛盾。

**`open` 行与两张新增表行的分工（2026-10-03 第 2、3、4、5、6、7 轮裁定后）。** `open` 合同项一行现为 **0** 项：
第 2 轮裁定前为 21，其中第 2 轮的三条 `open` 合同项（`CM-T03`、`CM-T04`、`CM-T08`）**已由第 2 轮（Codex
2026-10-03）裁定并闭合**，处置词由 `open` 改为 `accept`，故第 2 轮分布由 3 变为 **0**、总数由 21 变为 18；
第 3 轮的两条 `open` 合同项（`CM-X05`、`CM-C10`）**已由第 3 轮（Codex 2026-10-03）裁定并闭合**，处置词由
`open` 分别改为 `revise` 与 `accept`，故第 3 轮分布由 2 变为 **0**、总数由 18 变为 **16**；
第 4 轮的两条 `open` 合同项（`CM-C05`、`CM-C09`）**已由第 4 轮（Codex 2026-10-03，thread
`s7a4-arbitration-rulings`，主卡 `adopt` 0.97 / 计数确认卡 `adopt` 0.99）裁定并闭合**，处置词均由 `open`
改为 **`accept`**，故第 4 轮分布由 2 变为 **0**、总数由 16 变为 **14**；
第 5 轮的**四条** `open` 合同项（`CM-F06`、`CM-S08`、`CM-S10`、`CM-K07`）**已由第 5 轮（Codex 2026-10-03，
thread `01a0fe49-cfe5-7933-8d03-3d0a65d34c8b`，主卡 `adopt` 0.95 / 计数确认卡 `adopt` 0.99）裁定并闭合**，
处置词均由 `open` 改为 **`accept`**，故第 5 轮分布由 4 变为 **0**、总数由 14 变为 **10**；
第 6 轮的**五条** `open` 合同项（`CM-P04`、`CM-P06`、`CM-P08`、`CM-D04`、`CM-X01`）**已由第 6 轮（Codex
2026-10-03，thread `01a0fe61-3476-7882-9674-5f6b05035237`，单卡 `adopt` 0.97）裁定并闭合**，处置词均由
`open` 改为 **`accept`**，故第 6 轮分布由 5 变为 **0**、总数由 10 变为 **5**；
第 7 轮的**五条**残余 `open` 合同项（`CM-V06`、`CM-V07`、`CM-V13`、`CM-I06`、`CM-X06`，**全部属第 1 轮**）
**已由第 7 轮（Codex 2026-10-03，thread `01a0fe61-b7a3-7853-a380-0793fee14e14`，主卡 `adopt` 0.96 /
口径确认卡 `adopt` 0.99）按第 1 轮裁定补齐处置词与裁定正文并闭合**，处置词均由 `open` 改为 **`accept`**，
故第 1 轮分布由 5 变为 **0**、总数由 5 变为 **0**；
该 0 项严格等于 [CONTRACT_MATRIX.md](CONTRACT_MATRIX.md) §14 修正后的 `open` 计数（先由 20 → 21：
`CM-X01` 经 D-14 方案 (a) 由 `accept` 修正为 `open`，其分布进第 6 轮；再由 21 → 18：第 2 轮三条转 `accept`；
再由 18 → 16：第 3 轮两条分别转 `revise` / `accept`；再由 16 → 14：第 4 轮两条转 `accept`；再由 14 → 10：
第 5 轮四条转 `accept`；由 10 → 5：第 6 轮五条转 `accept`；最后由 5 → **0**：第 7 轮五条按第 1 轮裁定转
`accept`），**`open` 集合为空**。
`CM-S02`、`CM-K01` 的处置词是 `revise`，虽在本轮一并绑定轮次，但**不**计入 `open` 行，另列一行（**2** 项）；
`CM-T13`、`CM-S11`、`CM-K08` 是本次登记新增的裁决项、不是 CONTRACT_MATRIX 的合同项，也**不**计入 `open` 行，
另列一行（**3** 项）。本表的行/项总计（**76 行 / 84 项**）**不随 `open` 处置词变化**：第 2、3、4、5、6、7 轮只是把
合同项的处置词由 `open` 改为 `accept` / `revise` 并登记裁定，行数与项数均不变。分类串
**`0 + 19 + 15 + 10 + 14 + 3 + 2 = 63`** 是**按类别主编号统计的分类计数**（其中 `open` 类由 21 经 18、16、
14、10、5 降为 **0**）；
**`84`** 是 **76 行加 8 个合并行**的原 7 轮行/项总计。两者口径不同，**不能互相相加或替代**。
（该分类串在 2026-10-03 第 2 轮为 `18 + 19 + 15 + 10 + 14 + 3 + 2 = 81`，与上一段"含补充的总计为
**89 项 / 81 行**"中的行数 `81` 数值相同属巧合；第 3 轮把 `open` 类由 18 降为 16 后为 **79**，第 4 轮再降为
14 后为 **77**，第 5 轮再降为 10 后为 **73**，第 6 轮再降为 5 后为 **68**，第 7 轮再降为 **0** 后为 **63**，
均不再与该行数相同。
第 2 轮的口径经第 2 轮落地追问裁定确认；第 3 轮由计数一致性确认卡
（thread `s7a3-prepare-rulings`，verdict `adopt`，confidence 0.99，2026-10-03）确认 `open` 16 / `accept` 36 /
`revise` 26、总数仍 114，并确认 §0.1 末列仍保留 `CM-X05`、`CM-C10`；第 4 轮由处置词与计数确认卡
（thread `s7a4-arbitration-rulings`，verdict `adopt`，confidence 0.99，2026-10-03）确认 `CM-C05`、`CM-C09`
均转为 `accept`、最终 `open` 14 / `accept` 38、总数仍 114，并确认 §0.1 末列仍保留 `CM-C05`、`CM-C09`；
第 5 轮由处置词与计数确认卡（thread `01a0fe49-cfe5-7933-8d03-3d0a65d34c8b`，verdict `adopt`，confidence
0.99，2026-10-03）确认 `CM-F06`、`CM-S08`、`CM-S10`、`CM-K07` 四条均转为 `accept`、最终 `open` 10 /
`accept` 42、总数仍 114，并确认 §0.1 末列仍为 **21**、`CM-S11` / `CM-K08` 不改变任何计数；
第 6 轮由同一 thread 的单卡（thread `01a0fe61-3476-7882-9674-5f6b05035237`，verdict `adopt`，confidence
**0.97**，2026-10-03）确认 `CM-P04`、`CM-P06`、`CM-P08`、`CM-D04`、`CM-X01` 五条均转为 `accept`、最终
`open` 5 / `accept` 47、总数仍 114，并确认 §0.1 合计行仍为 **76 行 / 84 项 / 末列 21**、§7.2 仍 **19 条**；
第 7 轮由**口径确认卡**（同一 thread `01a0fe61-3476-7882-9674-5f6b05035237`，verdict `adopt`，confidence
**0.99**，2026-10-03）确认第 6 轮 `S7A6-R01…R12` 映射、第 6 轮 **15 行 / 17 项**、第 7 轮 **16 行 / 18 项**，
以及 §0.1 的 **76 行 / 84 项**与 ADR 原有的 **31 行 / 35 项**（= 17 + 18）口径**正确**；主卡
（thread `01a0fe61-b7a3-7853-a380-0793fee14e14`，verdict `adopt`，confidence **0.96**）确认第 7 轮 16 条的
裁定与五条 `CM-V*` 的 `open` → `accept`（`open` 5 → **0**、`accept` 47 → **52**），并确认 §0.1 末列历史
**21 不变**。）

**双重登记说明**（不是计数错误，是同一未决点在两份文档中的两次登记）：
CM-V13 ↔ P2-01、CM-X05 ↔ D-3、CM-C10 ↔ P1-03、CM-C09 ↔ Q-11、CM-C05 ↔ Q-16、
CM-F06 ↔ Q-12、CM-S08 ↔ Q-17、CM-S10 ↔ P1-07、CM-K07 ↔ Q-13、CM-P04 ↔ Q-09、
CM-P06 ↔ Q-15/P1-13、CM-P08 ↔ Q-09/P1-11、CM-D04 ↔ P1-09、CM-T08 ↔ P1-04、
CM-I06 ↔ Q-10、CM-X06 ↔ Q-14、CM-V06 ↔ Q-02、CM-V07 ↔ Q-01。

**8 个合并行**（同一行同时裁定两个编号，避免同一语义被批两次）：
CM-V13/P2-01、CM-T08/P1-04、CM-C10/P1-03、CM-S10/P1-07、CM-P06/P1-13、CM-P08/P1-11、
CM-D04/P1-09、D-2/D-7。另有 P2-10 单列一行，内容即 Q-18 的重述（裁定随 Q-18，不重复计数）。

**第 7 轮登记后的 `open` 集合闭合节（2026-10-03）。** 第 6 轮五条 `open` 合同项（`CM-P04`、`CM-P06`、
`CM-P08`、`CM-D04`、`CM-X01`）已由 Codex 单卡（thread `01a0fe61-3476-7882-9674-5f6b05035237`，verdict
`adopt`，confidence **0.97**）转为 **`accept`**；`open` 行由 10 降为 **5**，第 6 轮分布由 5 变为 **0**。
第 7 轮再由主卡（thread `01a0fe61-b7a3-7853-a380-0793fee14e14`，verdict `adopt`，confidence **0.96**）
把剩余 5 项（`CM-V06`、`CM-V07`、`CM-V13`、`CM-I06`、`CM-X06`，**全部属第 1 轮**）按第 1 轮裁定转为
**`accept`**；`open` 行由 5 降为 **0**、第 1 轮分布由 5 变为 **0**，最终
`open` **0** / `accept` **52** / `revise` 26 / `supersede` 17 / `retain` 17 / `reject` 2 = **114**，
**`open` 集合为空**。该五条与 [CONTRACT_MATRIX.md](CONTRACT_MATRIX.md) §0.1 第 33 行 / §8 之间的自相矛盾
由此闭合：第 1 轮早已裁定并将它们计入 §0.1 末列 5 项与 §9 第 1 轮行，第 7 轮只是**补齐处置词与裁定正文**
（`open` → `accept`），**不新增行 / 项**。
§0.1 合计行**保持 76 行 / 84 项 / 末列 21**（末列是历史归属列，不随 `open` 处置词变化）。

## 9. 裁定登记

owner 逐轮裁定后在此登记；**登记完成后才据此冻结 V2 文档**.

| 轮 | 裁定日期 | 结果（接受/修改后接受/拒绝+替代/登记为阻塞项） | 影响的合同项 | 是否解锁该批次 |
| --- | --- | --- | --- | --- |
| 1 | 2026-10-02 | **接受**（按 Codex 修订后的 14 行文本；含 Q-14 定为稳定失败、Q-01 补 artifact 互换性约束与兼容矩阵、Q-02/Q-10 表述收紧、CM-V13/P2-01 独立登记、D-3 落 Stage plan、D-4 仅标版本条款被取代、D-10 新登记） | CM-V06/V07/V13/I06/X06 | **是**（语义前置已满足；S7A-1 的公共头与实现批次仍须等 V2 ADR/Spec/ABI 冻结 + Gameplay I 标注 + typed contract review 完成） |
| 2 | 2026-10-03 | **接受**（第 2 轮 6 行全部接受，并一并闭合缺陷 F-04；provenance：thread `s7a1-freeze-bindings`，verdict `adopt`，confidence 0.99，模型 `gpt-6-astra`；补充确认卡 thread `cm-x01-disposition`，verdict `adopt`，confidence 0.99，确认 `CM-X01` 保持 `open`、不改计数）。要点：Tick 宽度为**有符号 64 位整数** + 单位由 `engine.tick.us.v1` 的 `TimebaseProfile` 声明 + 禁止隐式换算 + 溢出稳定拒绝；RationalBeat → `judgementTick` 精确有理数映射、四舍五入到最近整数、半数取偶、负值对称，同 Tick 按 `(tick, originKind, canonicalOrdinal)` 排序；`observationTick` 唯一来自校准后会话时钟，S7C-1 只能扩展 `CalibrationProfile` 参数；late policy 四项参数 typed 化、默认值记 `pending_measurement`、缺失或未测量在 prepare 稳定拒绝、重复排队稳定拒绝 + 诊断码；`commitTick` / commit window 定义；`AmountSpec` 与越界稳定拒绝，**数值不冻结**。F-04 闭合落点＝ABI 字段级映射表新增 `InputEvent.source` 行。**第 2 轮实现复核修订（2026-10-03，同 thread 两张续裁卡，`adopt` 0.98 / 0.99）**：`stop` 语义由"比例替换"修订为**塌缩 / 跳变 + 精确累计函数**（Spec §3.7.2 第 3 条），并补三分法（§9.3）、迟到参数量级门禁（§3.7.4 第 5 条、§3.7.7 表）与 Tick → Beat 反查阻塞项（§3.7.2 第 6 条）；计数与处置词不变；见 [第 2 轮实现裁定记录](../../stage_reports/stages/stage-07/2026-10-03-s7a-2-implementation-rulings.md)。**第 2 轮输入规范化半批的续裁（2026-10-03，同 thread 两张续裁卡，`adopt` 0.97 / 0.99）**：`AmountSpec` 的 canonical 整数宽度**明文冻结为有符号 64 位**（不得靠复用 Tick 宽度默示）、**判定顺序＝可表示性先于范围**（`amount_out_of_range` vs `amount_narrowed`，含 `exactNumerator == INT64_MIN` 按不可表示拒绝）、三条原子失败映射（`same_tick_collision` → `invalid_relation`、`amount_narrowed` → `budget_exceeded`、`time_reversal` → `invalid_relation`；`time_reversal` 类别**自 `budget_exceeded` 修正**为 `invalid_relation`）、负 `observationTick` 合法且只拒回退、**域声明集合与完整 `AmountSpec` 字段进 session identity**（`CM-T09`，**不得**推迟到 S7A-4；**7A 不引入 per-domain 版本字段**）、不连续表示与连续能力**都复用**既有 `input.continuous_unsupported`（`field.path` 区分，映射既有 **R-05**）、删除两个无引用死令牌且 clock 不可表示统一复用 tick 溢出码；**计数与处置词不变**（§7.2 仍 19 条、§9.2 仍九类、`open` 仍 10、114 总数不变）；见 [第 2 轮输入半批实现裁定记录](../../stage_reports/stages/stage-07/2026-10-03-s7a-2-input-implementation-rulings.md)、§2 表后 provenance 块与 [OPEN_QUESTIONS.md](OPEN_QUESTIONS.md) §4.3 | **CM-T03、CM-T04、CM-T08 已闭合**（`open` → `accept`）；`CM-T13` 为新增裁决项（非合同项）；F-04 为文本缺陷，闭合计入本轮 | **是**（S7A-2 的文档准入门禁已满足：7 条裁定全部落地并计数自洽；具体 late-policy 数值、具体业务量程限额、S7C-1 校准扩展与连续输入能力登记为**后续批次阻塞项**，不是本批次门禁；实现批次仍受 plan §S7A-2 的验证要求约束） |
| 3 | 2026-10-03 | **接受 / 修改后接受**（第 3 轮 6 行裁定；provenance：thread `s7a3-prepare-rulings`，模型 `gpt-6-astra`，三卡 verdict `need_info` 0.8、`reject` 0.88、计数确认卡 `adopt` 0.99）。要点：Q-05 **修改后接受**（Requirement 不含 Gameplay `effects`；非空 `effects` 只能显式 lowering 为不影响 Judgement/Replay 的 Presentation 数据，否则拒绝）；Q-07 **修改后接受**（`REQ0`/`CNS0` 保持 Foundation 语义；7A 新语义须使用可与旧语义区分的 entry/section 边界；未知必需 section / 未知 capability / 新 requirement kind 稳定拒绝；**只冻结行为，不冻结 wire 编号、布局或序列化编码**，具体表示在 S7A-3 消费前另行闭合）；Q-18 **接受** + P2-10 随之关闭（Release/tail 仅作同一 Requirement 显式声明的可选 phase，未声明却要求 tail 语义即拒绝）；CM-C10 / P1-03 **修改后接受**（prepare-time 有界循环完全展开，展开结果唯一决定 identity / snapshot / fact order；`boundedRelationInstance` 不进 7A 合同、遇则稳定拒绝、列为 7B+/S7C 候选）；**规范字节边界**（identity 的规范字节是 prepare 内部确定性派生物，不是公共 ABI 或 Packed 编码；冻结语义等价、排序与身份域边界，暂不冻结编码；编码实现须在 S7A-3 消费前闭合）；CM-X05 由 D-3 落地关闭（处置词 `revise`） | **CM-X05、CM-C10 已闭合**（`open` → `revise` / `accept`）；`CM-X05` 不再阻塞 S7A-3 | **是**（S7A-3 的文档准入门禁已满足：6 行裁定与规范字节边界全部落地并计数自洽；门禁清单中 `CM-X05` 标为"D-3 已关闭；仅具体 W 缺口在首次消费前须完成登记"；`boundedRelationInstance` 候选表示与 SUPPORT §6 的 `W-01…W-05` 候选项只阻塞首次拟消费它的后续批次，不是本批次门禁；实现批次仍受 plan §S7A-3 的验证要求约束） |
| 4 | 2026-10-03 | **接受 / 修改后接受**（第 4 轮 6 行裁定；provenance：同一 Codex 话题 thread `s7a4-arbitration-rulings`，模型 `gpt-6-astra`，consult：主裁定卡 verdict `adopt`、confidence 0.97；处置词与计数确认卡 verdict `adopt`、confidence 0.99；两卡文件名时间戳为 2026-10-02T20:0xZ（UTC），按本地日期 2026-10-03 登记）。要点：**Q-03 / `S7A4-R01` 接受**——拆分为 prepare/compile solver（展开、验证、上界证明、唯一性证明、生成 prepared profile）与 runtime coordinator（只执行 prepared profile 指定的确定性候选排序、资源检查与提交），runtime coordinator 不解析也不编译 solver，名称采用 `coordinator.policy.greedy_v1`，不把 runtime 行为称为 solver；**Q-11 / `S7A4-R02` 修改后接受**——7A 资源子集冻结为 `free` / `held` / `terminal`（`free + claim -> held`、`held + 合法 update -> held`、终止后按资源策略进入 `free` 或永久 `terminal`），`gap` / `handoff_pending` / 非零 grace / handoff / `capacity > 1` / owner 集合 / 并列 slot 一律稳定拒绝，`CM-C04` 的"只启用前三态"据此改写；**Q-16 / `S7A4-R03` 接受**——六步＝外层、八阶段＝第 4 步内部完整展开的**唯一**映射（第 3 步＝八阶段第 1 步、第 4 步＝八阶段第 2–7 步、第 5 步＝八阶段第 8 步后按 causal total order 排序、第 6 步 Tick 末提交 Hook/signal），阶段顺序与语义进 `judgement identity` 的 engine 组件、**不进 chart/content identity**，**S7A-4 只冻结阶段名称、顺序、映射与语义边界**（不冻结编号、编码、预算、窗口数值），`SK §4.2` 五步变体标为**非权威推导、已被取代**；**CM-C05 / `S7A4-R04` 接受**——随 `Q-16` 关闭；**CM-C09 / `S7A4-R05` 修改后接受**——随 `Q-11` 关闭：`resourceId` 只在 prepared canonical graph 资源命名空间内解释、`capacity = 1` 用唯一 slot、slot/lease/最小 contact handle/claim identity 进 canonical graph 与 prepared judgement inputs、lease/contact 由引擎按规范阶段分配且 `contact end` 不复用旧 handle、`observe` 只产生 Observation 不占用也不改变资源 owner；**P1-12 / `S7A4-R06` 接受**——Exact 由判定窗口半宽定义而非要求零 tick、窗口外早击不产生 Fact 但产生诊断、Miss/absence 由 deadline 产生 Fact、Hold 的 head/body/tail 分别记录有符号 error 不合并；**`SolverProfile`（`CM-C06`）**在 S7A-4 冻结字段语义、缺失 / 歧义 / 无法证明唯一时的 prepare 拒绝语义与 `objective` / `tieBreak` 的有序语义列表，**不冻结**默认列表、算法预算、`K` / fuel / `max*` 数值与序列化表示（留给 S7A-9 与后续预算批次） | **CM-C05、CM-C09 已闭合**（`open` → `accept`，两条）；`CM-C04`、`CM-C06`、`CM-C07` 处置词不变（`revise` / `revise` / `accept`），只更新未决点文字 | **是**（S7A-4 的文档准入门禁已满足：6 行裁定与唯一映射全部落地并计数自洽，最终 `open` 14 / `accept` 38 / `revise` 26 / `supersede` 17 / `retain` 17 / `reject` 2，总数仍 114；**具体预算数值、默认 profile 清单、proof 编码、wire / serialization、terminal 编码、后续 gap / handoff / `capacity > 1` 语义**登记为**首次消费它们的后续批次**阻塞项，不是本批次门禁；实现批次仍受 plan §S7A-4 的验证要求与 Spec §3.13 的不得消费清单约束） |
| 5 | 2026-10-03 | **接受 / 修改后接受**（第 5 轮 13 行裁定，编号 `S7A5-R01…R11`；provenance：同一 Codex 话题 thread `01a0fe49-cfe5-7933-8d03-3d0a65d34c8b`，consult，三卡——①主裁定卡 verdict `adopt`、confidence 0.95；②处置词与计数确认卡 verdict `adopt`、confidence 0.99；③ABI 状态格首词与追踪计数追问卡 verdict `adopt`、confidence 0.98；文件名时间戳为 2026-10-02T20:2x–20:3xZ（UTC），按本地日期 2026-10-03 登记）。要点：**`S7A5-R01` 顺序定案**——唯一规范总序为 `(commitTick, originKindPriority, canonicalOrdinal)`，`canonicalOrdinal` 由引擎从 `(originScope, originOrdinal, localOrdinal, phasePriority, factKindPriority)` 派生，**不得使用 `ingressSequence`、容器顺序或线程完成顺序**，Fact 的 `tick` 即 `commitTick`，`observationTick` 不是总序首键（首次消费 **S7A-4**，修订 Spec §3.9 第 3 条与 `plan.md` §S7A-4 第 5 步）；**`S7A5-R02`** Fact 身份生成与 `FactSemanticRevision` 归属，物理字节布局留 S7A-6；**`S7A5-R03`** Ruleset Tick 三阶段与 `faulted` 行为矩阵（不追加 fault Fact、不发布 RuleEffect / PresentationEvent、保留旧 state；faulted 的 submit / advance / seek / replay / 就地 reload 与 **snapshot** 稳定失败；仅显式 reset 或创建替换新 session 的 reload / recovery 可离开，且不得恢复 / 伪造未提交 StateDelta），并消除 plan §S7A-5 验证段与 Spec §9 / §10 的"faulted snapshot"歧义；**`S7A5-R04`** `RegisterKind` 三类语义与冲突策略冻结（`exclusive` 单 owner、`commutative_monoid` 已声明可交换可结合合成、`ledger_derived` 禁止直接 StateDelta 写入；7A 只接受这三类）；**`S7A5-R05`** 7A 只用内置 Ruleset，`cuexis.ruleset` package 命中 **R-09**（`ruleset.package_unsupported`），package 形状归 S7C-2；**`S7A5-R06` / `S7A5-R07`** 全量 Snapshot header 与 Replay header 字段集、`SnapshotPayload` 闭包与不得保存清单、`factSemanticRevision` 进 engine identity 而 `stateSchemaRevision` 不进、拼写规范类型 `EventCodecId` / 字段 `eventCodecId`；**`S7A5-R08`** `SeekLatencyCommitment` 类型、承诺语义、会话只能收紧与测量口径入口，具体 `maxSeekLatency` 数值仍禁止进入 Schema / header / 运行时约束 / 对外承诺；**`S7A5-R09`** Outcome `{hit, miss}`、phase-local head/body/tail、`FactCategory` 与 phase 一一对应、grade 可选且缺失即 absent、`TimingError` 为有符号整数 tick 差、seek / replay 从 Fact Ledger 重建统计、reset 不产生 Fact；**`S7A5-R10`** Life 在 7A 关闭（仅保留 `LifeState` 追踪与拒绝说明，`capability.disabled` 稳定拒绝）；**`S7A5-R11`** `P1-14` 的 REF0 / manifest / 诊断-source map 归属表作为 Spec 附录冻结，首次消费 S7A-3、集成 S7A-7 | **`CM-F06`、`CM-S08`、`CM-S10`、`CM-K07` 四条已闭合**（`open` → **`accept`**，四条，全部移出 `open` 集合）；`CM-S02`、`CM-K01` 处置词**保持 `revise`**（只写裁定正文）；`CM-S11`（`S7A5-R10`）、`CM-K08`（`S7A5-R08`）为本次登记新增的裁决项、**不是** CONTRACT_MATRIX 行，**不改变任何计数**；`P1-14` 登记为 `S7A5-R11`、**不并入 R10** | **是**（S7A-5 与 S7A-6 的文档准入门禁均已满足：13 行裁定与唯一映射全部落地并计数自洽，最终 `open` **10** / `accept` **42** / `revise` 26 / `supersede` 17 / `retain` 17 / `reject` 2，总数仍 114；§0.1 末列仍为 **21**；§7.2 仍 **19** 条、不新增拒绝类别；ABI 9 域 / **126 = 96 + 30** 与 9 行状态格首词均不变。**后续阻塞项**：Replay / Snapshot 字节布局、Event / Fact codec 与 `FactId` / `CommitId` 编码留 **S7A-6**；全部具体预算数值与 `maxSeekLatency` 数值留 **S7A-9** 或其首次序列化消费者；`cuexis.ruleset` package 形状 / identity / 迁移留 **S7C-2**；`P1-14` 的 REF0 首次写入属 **S7A-3**、entry / manifest 集成属 **S7A-7**） |
| 6 | 2026-10-03 | **接受**（第 6 轮 15 行 / 17 项全部登记为 accepted，编号 `S7A6-R01…R12`；provenance：Codex 话题 thread `01a0fe61-3476-7882-9674-5f6b05035237`，consult，模型 `gpt-6-astra`，单卡 verdict `adopt`、confidence **0.97**；卡文件名时间戳为 2026-10-02T20:51:47.167Z（UTC），按本地日期 2026-10-03 登记）。要点：**`S7A6-R01` / Q-06 接受**——`playback = true` 必须显式 `entryKind`，7A 只允许 `packed-chart` 与 `gameplay-graph`，`author-source` 不是 Playback entry，Ruleset package 不是第三种 entry，manifest 记录 entry kind / compiled semantic identity / artifact identity / Ruleset binding / capability closure / resource·presentation closure / `sourceOf`；**`S7A6-R02` / Q-19 + CM-X01 接受**——`compiledSemanticIdentity` 与完整 capability closure 必须同时携带且可比较，`CapabilityRecord` 七字段及逐字段 identity 归属冻结（`stableRejectCode` 只进诊断码表 / closure，不进 identity）；**`S7A6-R03` / Q-09 + CM-P04 接受**——`GameplayOverride` 命名层与 `Initial < Behavior < Animation < GameplayOverride/FactBinding < StudioPreviewOverride` precedence，`render.visible = false` 为该层显式值，adapter 只消费已提交 FactBinding、当前有效立即应用 / 未来 `effectivePresentationTick` 排队、`(factId, targetId)` 幂等、seek / replay / reload 从 Fact Ledger 重建 token、lifetime 到期或 reset / session replacement 时终止、表现失败绝不回写 Fact Ledger；**`S7A6-R04` / Q-15 + CM-P06·P1-13 接受**——判定闭包内 target / domain / action / table 缺失 = prepare 原子失败（`identity_closure_incomplete` 或 `invalid_relation`），纯表现 target 缺失允许空绑定、丢弃该投影事件并给稳定 `presentation-target-missing` 诊断、不 fault session；**`S7A6-R05` / CM-P08·P1-11 接受**——去重键 `(factId, targetId)`、三值 aggregation 均幂等、`groupCommit` 部分提交不回滚并记稳定 `partial-group` 诊断后进入稳定拒绝路径、correction 仅影响未提交窗口；**`S7A6-R06` / CM-D04·P1-09 接受**——四层诊断模型（§9.1）+ 九类 category（§9.2）+ `faulted` 行为（§9.4），集中码表落 `schemas/cuexis.gameplay-diagnostics.v2.codes.json` 并新增一个 CTest 校验项（code 唯一性、category / severity / faulted 映射完整性、`stableRejectCode` 已登记、输入 / 几何三码不可重复扩展），除已冻结的 `input.continuous_unsupported` / `input.direction_unsupported` / `geometry.inference_rejected` 及既有 R-09 映射外，诊断码字符串须经码表登记与 CTest 校验后才能进入公共 ABI；**`S7A6-R07` / P1-08 接受**——`observationTime` 拆为 `observationTick` + 原始时间戳（`observationTick` 仍唯一来自校准会话时钟），`error` 为有符号整数 tick 差，`eventSequence` 为会话内单调序号，不做隐式单位换算；**`S7A6-R08` / P2-04 接受**——`phase` / `category` 为分类层且 `FactCategory` 与 `{tap, hold_head, hold_body, hold_tail}` 一一对应，`outcome` 仅 `{hit, miss}`，`grade` 可选缺失即 absent，`localOutcome` 仅 Hold 段分解，`reasonCode` 仅解释拒绝 / 降级，同层互斥、跨层可组合；**`S7A6-R09` / P2-05 接受**——`action`、`requiredAction`、`domain` / `InputDomain` 三者独立、不互相推导；**`S7A6-R10` / P2-06 接受**——`resources` 拆为 `resourceRef` / `claimPolicy` / resource record，资源三分法沿用 S7A-4 的 `free` / `held` / `terminal` 子集；**`S7A6-R11` / P2-08 接受**——只有 Fact Ledger 进 Replay / Snapshot，`RuleEffectEvent` / `PresentationEvent` / `EffectEvent` 均为投影、`EffectEvent` 只是投影内部名；**`S7A6-R12` / P2-09 接受**——语义字段一律"未知即拒绝"，`extensions` 只允许命名空间化且必须证明不影响判定的 inspection metadata。**本轮不涉及 `P2-03`，不动该行**；**不得**把本轮写成"Judgement 实现完成"或"Stage 7A 完成"——本卡冻结的只是文档合同与门禁 | **`CM-P04`、`CM-P06`、`CM-P08`、`CM-D04`、`CM-X01` 五条已闭合**（`open` → `accept`）；`CM-V06`、`CM-V07`、`CM-V13`、`CM-I06`、`CM-X06` **处置词不变**（仍 `open`，第 7 轮专责 §0.1 与 §8 的自相矛盾裁定） | **是**（S7A-7 的文档准入门禁已满足：15 行 / 17 项裁定与 `S7A6-R01…R12` 编号登记完成、五条 `open` 合同项转 `accept`、最终 `open` 5 / `accept` 47 / `revise` 26 / `supersede` 17 / `retain` 17 / `reject` 2、总数仍 114，计数自洽；**S7A-7 实现批次仍受 plan §S7A-7 的验证要求与本轮"不得消费清单"约束**） |
| 7 | 2026-10-03 | **接受 / 修改后接受（含 owner-only 与待 owner 确认）**（第 7 轮 16 行 / 18 项全部登记为 accepted，编号 `S7A7-R01…R16`；provenance：Codex 话题 thread `01a0fe61-b7a3-7853-a380-0793fee14e14`，consult，模型 `gpt-6-astra`，**主卡 verdict `adopt`、confidence 0.96**；同议题小追问的**口径确认卡** thread `01a0fe61-3476-7882-9674-5f6b05035237`，verdict `adopt`、confidence **0.99**；两卡文件名时间戳为 2026-10-02T20:52:37.188Z 与 2026-10-02T21:08:34.383Z（UTC），按本地日期 2026-10-03 登记）。要点：**`S7A7-R01` / P1-01 接受**——7A 只接受静态 typed 判定域记录、几何属 Gameplay closure、坐标系与量程在判定域内声明、与 Presentation transform 完全隔离、动态 frame 登记为 7B+ 候选并稳定拒绝（首次消费 **S7A-3**，准入门禁）；**`S7A7-R02` / P1-02 接受**——CXT local relation 全局合并在 prepare 编译期完成、稳定 ID = 来源文档 identity + 声明序号、同名不合并而是拒绝、跨 invocation 引用必须显式、按稳定 ID 排序使重排不改变结果（首次消费 **S7A-3**）；**`S7A7-R03` / P1-05 接受**——`input.trajectory.v1` 属连续输入、7A 整体稳定拒绝，schema 保留为 **S7B-1 准入项**（阻塞 S7B-1）；**`S7A7-R04` / P1-06 接受**——Ruleset package 登记为 **S7C-2 准入项**，7A 遇 package 输入稳定拒绝、entry kind 位置保留但不解析（阻塞 S7C-2）；**`S7A7-R05` / P1-10 接受**——沿用 `BUDGET_AND_EVIDENCE_PLAN.md` 五类 profile、归属按"谁产生谁计数"、7A 只登记计数与上界，**禁止在 S7A-9 前冻结限额**（首次消费 **S7A-9**）；**`S7A7-R06` / P1-15 接受**——Chart v5 inline 与 CXT v2 emission 的等价 = prepare 后 canonical graph 与 derived capability closure 逐字段相等、忽略 `sourceMap` 与物理顺序、逐字段 diff 作为 S7A-3 的 golden 证据（首次消费 **S7A-3**）；**`S7A7-R07` / P2-02 接受**——措辞统一为"不解析/编译 solver；runtime coordinator 只执行已准备的确定性 policy"，同步 Spec §3.12 及不得消费清单、plan §S7A-4 边界段、ABI solver / coordinator 段及其状态格（状态格首词不变）；**`S7A7-R08` / P2-07 接受**——统一为 `source closure` / `diagnostic map` / `content-artifact identity` 三项，`P1-14` 归属仍以 Spec §3.8.6 为准，ABI 保留三行、不新增类型、维持 9 域 / 126 = 96 + 30；**`S7A7-R09` / D-2·D-7 接受**——计划措辞改为"`open/play/pause/seek/reload/quit` 为 ADR 0042 的六动词；实现另有 `tick` 运行时步骤"，不改 ADR 0042 与实现；**`S7A7-R10` / D-5 接受**——记录为"后续审查文档必须使用 ADR / 格式编号引用"；**`S7A7-R11` / D-6 接受**——记录为"在引用处注明 `tools/check_stage6_a2.py` 实际行号 `:259`"，不改历史报告正文；**`S7A7-R12` / D-9 接受（owner-only）**——候选补丁已在工作区且未提交，须由 owner 按 ADR 0042 具名复核后落到 `master`，**在该动作完成前不得宣称 S7A-8 版本门禁闭合**（首次消费 **S7A-8**）；**`S7A7-R13` / D-11 接受**——以 **Stage 14** 为唯一阶段号，本轮不改 `AGENTS.md`，在 `CURRENT_STATUS.md`、`ROADMAP.md`、ADR 0043 各加一句"稳定 C ABI 唯一归属 Stage 14"并注明 `AGENTS.md` 的 Stage 12 为孤例、由 owner 择时订正；**`S7A7-R14` / D-12 接受（待 owner 确认）**——只登记不编辑那 8 处指向行，处置文本为"改指 ADR 0044 / V2 Spec / V2 ABI 并注明 Gameplay I 已 `superseded`，不得改写论证正文，**须先取得 owner 对历史稿修改的确认**"；**`S7A7-R15` / D-13 接受**——只补"已处置"最终证据指针（三个 spike 文件 `clang-format -i` 已做、`cuexis_format_check` 由 156 条违规 → 0；`AGENTS.md`:110 glob 描述已订正），无遗留实现子项；**`S7A7-R16` / D-14 接受**——只补"已处置"最终证据指针（已按方案 (a) 把 `CM-X01` 绑到第 6 轮、`CONTRACT_MATRIX` 处置词转回 `accept`），无遗留实现子项 | **`CM-V06`、`CM-V07`、`CM-V13`、`CM-I06`、`CM-X06` 已按第 1 轮裁定补齐处置词与裁定正文**（`open` → `accept`，五条）；`open` 由 5 降为 **0**、`accept` 由 47 升为 **52**，总数仍 114，`open` 集合为空；`revise` 26 / `supersede` 17 / `retain` 17 / `reject` 2 不变 | **是**（第 7 轮是**收尾澄清与缺陷**轮：16 行 / 18 项全部登记并落地，`open` 集合为空，§0.1 合计仍 **76 行 / 84 项 / 末列 21**，§7.2 仍 **19 条**、§9.2 仍**九类**、ABI 仍 **9 域 / 126 = 96 + 30**、§未决项条目数不变；**S7A-3 由 `P1-01` / `P1-02` / `P1-15` 解锁**；`P1-05` / `P1-06` 分别**阻塞 S7B-1 / S7C-2**；`P1-10` 的限额留 S7A-9；**S7A-7 仅在 `CM-P04` / `CM-P06` / `CM-P08` / `CM-D04` / `CM-X01` 闭合后集成**（已于第 6 轮闭合）；`D-9` owner-only、`D-12` 待 owner 确认；**本轮只冻结文档合同与门禁，实现批次 S7A-3…S7A-9 仍未完成**） |
| 1 补充 | 2026-10-02 | 接受（第 1 轮补充：S7A-1 准入闭合，按 Codex 修订后的 5 条文本） | — | 否（仅解锁受限 S7A-1：须先完成限定基线冻结、替代关系复核、typed review 关闭记录） |

**第 6 轮 `CM-D04` / `P1-09` 首次消费的后续补齐登记（2026-10-03，不新增轮次行、不改既有计数）。**
本卡**不占用新的轮次号**：它是第 6 轮 `CM-D04` / `P1-09`（`S7A6-R06`）首次消费后暴露的**码表分类学冲突**
的后续补齐，因此**§0.1 的轮次总览与逐轮行 / 项合计（76 行 / 84 项 / 末列 21）逐字不变**，
**§9 的"裁定登记"表不新增行**，也**不改动 §6 表内任何既有文字与计数**。provenance：Codex 话题 thread
`01a0fe6b-6e57-7051-87f8-cf9588e85568`（consult，模型 `gpt-6-astra`），单卡 verdict **`adopt`**、
confidence **0.96**；决策卡文件名时间戳 `2026-10-02T21:03:39.462Z`（UTC），按先例统一按**本地日期
2026-10-03** 登记。**本块只追加登记**。

| 条目 | 涉及项 | 裁定（第 6 轮首次消费后续补齐，2026-10-03） | 落点 / 首次消费 |
| --- | --- | --- | --- |
| 九类为唯一 category 权威 | `CM-D02` / `CM-D04` / `P1-09`（`S7A6-R06`） | 九类是**唯一** category 枚举；字面 `capability` 逐字替换为 `capability_disabled`（Spec §3.7.7 表、§7.2、§9.3、§S7A-2 节第 5 项 + ABI §S7A-2 输入半批补充 ⑤ 行，共 **6 处**）；ABI 的码族分组词明确为"**不是 `category` 的取值**"；src-only `kCapabilityCategory` **保持原样、不需改名**，只在进入公共码表时映射 | Spec §3.7.7 / §7.2 / §9.3 / §S7A-2；ABI §诊断分层 / §码族来源 / §S7A-2 输入半批补充 |
| 八条已登记码 category 定案 | `CM-D04` / `P1-09` | R-11 → `invalid_relation`、**R-12 → `invalid_relation`（改正，原推导为 `unknown_capability`）**、R-13 → `invalid_relation`、R-14 → `capability_disabled`、R-15 → `capability_disabled`、`capability.revision_mismatch` → `unknown_capability`、R-17 → `ambiguous_migration`、`capability.budget_insufficient` → `budget_exceeded` | 集中码表 `codes[]`；Spec §7.2 |
| 运行期 `faulted` 专有码 | Q-08 / `CM-D04` / `P1-09`（`S7A6-R06`） | 新增并登记 **`ruleset.transaction_failed`**（`invalid_relation` / `error` / **`session_faulted`** / 首次消费 **S7A-5**）；Spec §9.4 明确"第二阶段 Ruleset state commit 失败**统一**使用此码"；**整 Tick 不提交、旧 state 保留、可查询 `faulted`、事实不可回滚、不追加 fault Fact、不发布 RuleEffect / PresentationEvent** 的行为不变 | Spec §9.4 末段、§7.2 第 5 轮映射行、§9.6 第 5 条；ABI §错误与诊断映射；集中码表 |
| 两个表现码原样登记 | `S7A6-R04` / `S7A6-R05`（`Q-15` / `CM-P06` / `P1-13`、`CM-P08` / `P1-11`） | `presentation-target-missing` 与 `partial-group` **码串原样保留**；`invalid_relation` / `error` / `session_unaffected` / 首次消费 **S7A-7** | Spec §9.6 第 5 条、§9.7 / §9.8；ABI 域 8 / §错误与诊断映射；集中码表 |
| 首次消费批次补齐 | `CM-D04` / `P1-09` | 六个此前无批次声明的码统一登记首次消费 **S7A-3**：`capability.budget_insufficient`、`capability.revision_mismatch`、`capability.permanently_unsupported`、`capability.future_development`、`input.direction_unsupported`（**R-06**）、`format.gameplay_version_unsupported`（**R-17**） | 集中码表 `ownerBatch`；Spec §7.2 |
| 13 个 `judgement.s7a2.*` 令牌 | `S7A6-R06` 第 3 条 | 全部保持 **src-only、永不进入公共码表或 ABI**；码表状态词改为 **`not_registered_src_only`（明确不登记，不是待登记）**，防后续误当 pending | 集中码表 `pendingRegistration` 13 条 + 顶层 `srcOnlyPolicyNote` |
| `severity` / `faulted` 取值集 | `CM-D04` | `severity` 闭集 **`info` / `warning` / `error`**（以 core `DiagnosticSeverity{Info, Warning, Error}` 为准；7A 已登记码一律 `error`）；`faulted` 闭集 **`session_unaffected` / `session_faulted`**（`session_faulted` 唯一承载者 = `ruleset.transaction_failed`） | Spec §9.1；ABI 域 8 `DiagnosticSeverity` / §诊断分层 |
| 校验器严格约束保留 | `S7A6-R06` | 九类**恰好**、`R-01…R-19` **恰好一次**、输入 / 几何族**只允许** `input.continuous_unsupported` / `input.direction_unsupported` / `geometry.inference_rejected` 三码；**新增 category、R 编号或输入 / 几何族码必须显式修改校验器**（期望行为，不放宽） | `cmake/VerifyGameplayDiagnosticsCodes.cmake`（**本轮零改动**）；负例 4 证明约束仍生效 |
| 计数器 | — | **一律不变**：`CONTRACT_MATRIX` **114**、六类分布 `accept` 52 / `revise` 26 / `supersede` 17 / `retain` 17 / `open` 0 / `reject` 2；§0.1 **76 行 / 84 项 / 末列 21**；Spec §7.2 **19 条**；类别**九类**；ABI **9 域 / 126 = 96 + 30**；§未决项条目数不变 | 本表 §0.1 / §8 / §9 逐字不变 |

**本轮表述纪律。** 本卡落地的是**文档与集中码表语义**；**不得**写成"Judgement 实现完成"或"Stage 7A 完成"
（实现批次 S7A-3…S7A-9 仍未完成）。带日期的落地证据见
[第 6 轮诊断分类学后续补齐记录](../../stage_reports/stages/stage-07/2026-10-03-s7a-diagnostics-taxonomy-rulings.md)；
未决问题侧的同一登记见 [OPEN_QUESTIONS.md](OPEN_QUESTIONS.md) §4.6。

## 10. 相关索引

- [Gameplay V2 acceptance package](README.md)：本清单的包内索引与接受清单
- [CONTRACT_MATRIX.md](CONTRACT_MATRIX.md)：114 项合同逐项台账
- [OPEN_QUESTIONS.md](OPEN_QUESTIONS.md)：未决问题原文与候选处置
- [SUPPORT_AND_REJECTION_MATRIX.md](SUPPORT_AND_REJECTION_MATRIX.md)：支持集合、19 条拒绝与 W 类缺口定义
- [BATCH_GATES.md](BATCH_GATES.md)：每批次的正例/负例/诊断/golden/停止条件
- [Stage 7A 实施计划](../../stage_plans/active/stage-07/plan.md)：冻结顺序与批次依赖

# S7A-2 准入门禁：第 2 轮（时间域与迟到策略）裁定落地记录

状态：dated implementation record（第 2 轮裁定的文档落地证据；不是阶段关闭、不是 owner acceptance、
不是实施证据）

更新日期：2026-10-03

上级文档：[Stage 7A 实施计划](../../../stage_plans/active/stage-07/plan.md) §S7A-2 ·
[Gameplay V2 acceptance package](../../../proposals/gameplay-v2-acceptance/README.md) ·
[未决语义分轮裁决清单](../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) §2、§9

本文是**带日期的落地记录**（reports own dated evidence）：它登记 Codex 对第 2 轮（时间域与迟到策略）
的裁定、逐条落进哪些文档的哪一节、F-04 的闭合落点、S7A-2 的不得消费清单，以及本轮跑过的门禁与结果。
它不复制 Spec / ABI 的合同正文，也不构成任何实施声明；未在本文列出的门禁一律视为未运行。

## 1. provenance

| 卡 | thread | 模型 | verdict | confidence | 日期 | 内容 |
| --- | --- | --- | --- | --- | --- | --- |
| 决策卡（第 2 轮主裁定） | `s7a1-freeze-bindings` | `gpt-6-astra`（consult） | `adopt` | **0.99** | 2026-10-03 | 第 2 轮 7 条裁定（含 F-04 闭合），全部登记为阻塞 S7A-2 的门禁 |
| 补充确认卡（处置词口径） | `cm-x01-disposition` | `gpt-6-astra`（consult） | `adopt` | **0.99** | 2026-10-03 | `CONTRACT_MATRIX.md` 的 `CM-X01` **保持 `open`**；不改为 `revise`；`CM-S02` / `CM-K01` 的 `revise` 口径不适用于本项；**不回改任何计数** |
| 落地追问卡（同 thread 续问） | `s7a1-freeze-bindings` | `gpt-6-astra`（consult） | `adopt` | **0.99** | 2026-10-03 | 稳定拒绝**维持既有类别映射**（`R-01…R-19` 仍 19 条、不新增 `R-20`）；**不接受** §8 分类串的旧写法，改为显式写明 `= 81` 并补双重口径说明（见 §2.3） |

**日期口径说明。** 两张卡的文件名时间戳为 2026-10-02T19:04Z 与 2026-10-02T19:29Z（UTC），
裁定与落地按**本地日期 2026-10-03** 登记；本文、Spec §3.7 / §S7A-2、ABI §S7A-2 与各计数登记均使用
2026-10-03。

**confidence 口径说明（需要复核的点）。** 两张卡首部均记 **0.99**；同一 thread `s7a1-freeze-bindings`
的前一批落地（`FREEZE_BLOCKING_BINDINGS.md`、`RULING_WORKSHEET.md` §7B 等处）记 **0.96**。本次落地在
**第 2 轮相关**的新登记处统一采用卡首部的 **0.99**，并保留旧 0.96 作为历史绑定登记的证据；两处数字
来源不同（不同卡 / 不同批次），**未回改**既有 0.96 行。

## 2. 七条裁定要点

| # | 编号 | 裁定要点 | 落点 |
| --- | --- | --- | --- |
| 1 | Q-04 / CM-T03 / CM-T04（统一裁决） | `ChartTick`、`judgementTick`、`observationTick`、`commitTick` 均为**有符号 64 位整数**；单位由 `engine.tick.us.v1` 的 `TimebaseProfile` 声明；**禁止隐式单位换算**；**溢出稳定拒绝** | Spec §0.1、§3.7.1；ABI 域 1、§单位与量程 |
| 2 | CM-T03（RationalBeat → `judgementTick`） | 精确有理数运算；**四舍五入到最近整数、半数取偶**；**负值对称**；tempo / stop / 负 Beat 使用**同一规则**；同 Tick 碰撞按规范键 **`(tick, originKind, canonicalOrdinal)`** 排序，**禁止按 ingress 顺序排序** | Spec §3.7.2 |
| 3 | CM-T04（`observationTick` 唯一来源） | 输入进入判定管线时捕获的**校准后会话时钟**；设备时间、宿主到达时间、音频时间、渲染帧时间**仅保留为诊断上下文**；**S7C-1 只能扩展 `CalibrationProfile` 参数，不得替换 canonical source** | Spec §3.7.3；ABI 域 1 `ObservationTick` |
| 4 | CM-T08 / P1-04（迟到策略） | `finalizationWatermark`、queue hop、窗口 open/close、重复排队条件均为 `TimebaseProfile` / ruleset 提供的 **typed 参数**；默认值**只在 profile registry 登记为 `pending_measurement`**；**不得**成为 ABI 常量、隐式零值或研究切片限额；**缺失或未测量值在 prepare 稳定拒绝**；**重复排队稳定拒绝并返回诊断码** | Spec §3.7.4；ABI 域 1 `FinalizationWatermark` / `LateEventPolicy` |
| 5 | P2-03（`commitTick` / window） | `commitTick` 是**事务提交发生的 Tick**；window 是**相邻两个可提交 Tick 之间的观察区间**；**事实只能在 `commitTick` 提交** | Spec §0.1 术语表与 §3.7.5；ABI 域 1 `CommitTick` |
| 6 | CM-T13（第 2 轮新增，量程与量化） | 每个 `InputDomain` 必须携带 typed **`AmountSpec`**：canonical **整数量化**、scale、**可表示范围**、**边界策略**；量化用**精确整数运算**、**最近值半数取偶**；**超范围 / 窄化 / 溢出稳定拒绝并返回诊断码**；**本轮不冻结具体业务量程数值或限额** | Spec §3.7.6；ABI 域 2 `DomainAmount`、§数值域与量程 |
| 7 | F-04（文本缺陷关闭） | ABI 字段级映射表新增原文行（`InputEvent.source` → `SourceClass`；`SourceBuildIdentity` 只承载生产者、参数、compiler profile 与 source map 的构建来源，**不进入判定语义**）；并在 ABI 输入域小节补齐该分工 | ABI 字段级映射表 + 域 2 / 域 7 + "来源类别的分工"段 |

### 2.1 登记口径（全部阻塞 S7A-2）

上述 7 条**全部登记为阻塞 S7A-2** 的门禁。**S7C-1 校准扩展**、**具体 late-policy 数值**、
**具体业务量程限额**、**连续输入能力**登记为**后续批次**阻塞项——它们**不是** S7A-2 的门禁，
也不得据此阻塞 S7A-2 的语义冻结。

### 2.2 稳定拒绝的登记判定（解释性判断）

本轮新增的四类稳定拒绝（Tick 溢出；`DomainAmount` 越界 / 窄化 / 量化溢出；late-policy 参数缺失或
未测量；重复排队）**未新增 `R-20` 等条目**，而是**全部映射到既有条目**，因此 §7.2 的
"稳定拒绝清单（19 条）"**仍为 19 条**：

| 新增拒绝场景 | 映射到的既有条目 | 理由 |
| --- | --- | --- |
| Tick 溢出 | §9.2 类别 `budget_exceeded` + §9.3 的"越界即原子失败"路径 | 溢出＝已声明的 64 位表示范围不可满足；它是**已声明宽度内的值不可表示**，不是能力缺失，故走类别与原子失败路径而非能力拒绝面 |
| `DomainAmount` 越界 / 窄化 / 量化溢出 | 同上（§9.2 `budget_exceeded` + §9.3 越界路径） | R-01…R-19 是能力 / 禁止项 / 迁移歧义三级拒绝面，**不覆盖输入量项**；量程越界属"声明量程内的值不可表示"，与上一条同族 |
| late-policy 参数缺失或未测量 | §9.2 类别 `late_policy_incomplete` + §9.3 prepare 原子失败条件 | §9.3 已把"late policy 缺少 `finalizationWatermark`、queue hop 或 snapshot 规则"列为 prepare 原子失败；本轮把它扩展到"未测量（`pending_measurement`）"，属既有条目覆盖 |
| 重复排队 | §9.2 类别 `late_policy_incomplete` + §9.3 同一原子失败条件 + §5.2 条件行（能改变 Fact Ledger 内容或顺序者进 judgement identity） | 重复排队的判据是 canonical 观测 identity；按 ingress 顺序消解会违反 §5.2 的确定性要求，故按"策略无法给出确定处置即稳定拒绝"处置，不新增条目 |

**为什么优先映射而不新增**：`SUPPORT_AND_REJECTION_MATRIX.md` 的 R 清单（19 条）是**能力级**拒绝面
（后续 capability / 禁止项 / 迁移歧义），其条数被多处计数引用。本轮四类拒绝都是"已声明范围或已声明
能力内的值 / 参数不可满足"，属 §9.2 类别与 §9.3 原子失败条件的覆盖范围；把越界或量化失败记成 R 条目
等于把它**误报为"能力被拒绝"**，会与 §7.1 支持集合与 capability 派生链冲突，并触发 SPEC / ABI /
包 README / 矩阵的计数联动。若后续裁定认为必须新增 R 编号，须另立裁决并同步全部计数；本轮经比对
**四条全部有既有覆盖**，因此未新增任何 R 条目。**该判定已由 2026-10-03 的落地追问卡确认（见 §2.3）。**

### 2.3 落地追问裁定（2026-10-03）

本节登记 §1 表中第三张卡（同 thread `s7a1-freeze-bindings` 的续问，`adopt`，confidence 0.99）
对落地过程中两处判断的裁定：

1. **稳定拒绝的落点：维持映射既有条目（选择 (a)）。** 四类新拒绝继续映射 §9.2 的 `budget_exceeded` /
   `late_policy_incomplete` 与 §9.3 原子失败条件；§7.2 的 `R-01…R-19` **保持 19 条**，**不新增 `R-20`
   或专用拒绝码**。理由：这些场景不是能力级拒绝，新增 R 编号会误报"能力被拒绝"并触发多处计数联动。
   落地复核项：核对四类映射、重复排队的 identity 条件行，以及"能力级拒绝面与值 / 参数失败分离"的措辞一致。
2. **§8 计数口径：不接受把分类串写成 83（选择 (c)）。** `18 + 19 + 15 + 10 + 14 + 3 + 2` 的算术和是
   **81**，不是 83（中间汇报的算术笔误；文档中并未出现该错误值，已 grep 确认）。裁定要求：在
   [RULING_WORKSHEET.md](../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) §8 的计数口径段
   显式写为 **`18 + 19 + 15 + 10 + 14 + 3 + 2 = 81`**（**按类别主编号统计的分类计数**），并补充
   "**`84` 是 76 行加 8 个合并行的原 7 轮行 / 项总计；两者口径不同，不能互相相加或替代**"；
   **不得**新增"−3"之类的负项行来伪造算式闭合。已按此改写该口径段（同段另注明：此处 `81` 与
   "含补充的总计为 89 项 / 81 行"中的行数 `81` 属不同口径，数值相同是巧合）。

## 3. F-04 闭合

| 项 | 内容 |
| --- | --- |
| 缺陷本体 | `GAMEPLAY_V2_ABI.md` 字段级映射表登记了 `observationTime` / `sequence` / `grip` / `measures` / `JudgementFact` 四字段 / `eventSequence` / 四分量 identity，但**未登记 `InputEvent.source`（Gameplay I 的 `SourceIdentity`）的去向** |
| 来源 | [S7A-1 typed contract review](2026-10-02-s7a-1-typed-contract-review.md) §6 F-04 与 §10 关闭复核（该处原记"未闭合"；其保留的是**审查当时**的事实陈述，本次闭合不回改该报告） |
| 闭合动作 | ABI 字段级映射表新增 `InputEvent.source` 行；ABI 输入域小节（域 2 `SourceClass`、域 7 `SourceBuildIdentity`）补齐分工；Spec / 工作表 / 台账 / 未决问题各登记一处 |
| 语义 | `SourceClass`（域 2）＝规范事件中的设备 / 来源类别，**进入** canonical 观测与判定边界；`SourceBuildIdentity`（域 7）＝只承载生产者、参数、compiler profile 与 source map 的构建来源，**不进入**判定语义，不得用于替代或补全 `SourceClass` |
| 最终落点 | `docs/api/GAMEPLAY_V2_ABI.md` 字段级映射表的 `InputEvent.source` 行（原文见 §4） |

## 4. 改动的文档与落点

| 文件 | 落点 | 要点 |
| --- | --- | --- |
| [GAMEPLAY_V2_SPEC.md](../../../formats/GAMEPLAY_V2_SPEC.md) | §0.1 新增"时间域术语"；§3.7 新增（含 3.7.1–3.7.8）；§7.2 增映射段；§11 第 2 轮行改"已裁定"并新增后续批次行；§11.1 增第 4 条；新增 `## S7A-2 限定冻结范围与登记规则（2026-10-03）`；更新日期 2026-10-02 → 2026-10-03 | 术语、宽度与单位、tie rule、`observationTick` 来源、late policy 类型化、`commitTick` / window、`AmountSpec`、本批次不冻结项、19 条不变的映射判定 |
| [GAMEPLAY_V2_ABI.md](../../../api/GAMEPLAY_V2_ABI.md) | 候选状态段；域 1 表 + "第 2 轮已冻结的事实"段；域 2 `DomainAmount` / `SourceClass`；域 7 `SourceBuildIdentity`；"来源类别的分工"段；§单位与量程；§数值域与量程；字段级映射表 + F-04 闭合记录；§仍待冻结两行；新增 `## S7A-2 限定冻结范围与登记规则（2026-10-03）`；§未决项 #3 / #5 / #6 与新增 #18 | 保留既有"待冻结（Q-04 / CM-T03，第 2 轮）"状态词，只补已冻结事实；保留 `CM-X01` 的"待冻结（第 6 轮）" |
| [RULING_WORKSHEET.md](../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) | 文档角色句（21 → 18）；§0.1 末列口径段；§2 裁定列逐行填写 + 三条注；§7 后新增 F-04 闭合注；§8 `open` 行 21 → 18 与口径段；§9 新增第 2 轮行 | 第 2 轮状态标为已裁定；§0.1 末列仍为 21（列语义＝"哪一轮一并关闭"） |
| [CONTRACT_MATRIX.md](../../../proposals/gameplay-v2-acceptance/CONTRACT_MATRIX.md) | §0 统计与说明；§3 `CM-T03` / `CM-T04` / `CM-T08` 三行 `open` → `accept`；§12 `CM-X01` 口径确认注；§14 计数、`open` 清单与 S7A-2 行；更新日期 | `accept` 32 → 35、`open` 21 → 18，总数仍 114 |
| [OPEN_QUESTIONS.md](../../../proposals/gameplay-v2-acceptance/OPEN_QUESTIONS.md) | Q-04 行批次格追加裁定指针；新增 §2.1 第 2 轮裁定登记表（含 F-04）；§6 第二批标注已裁定；更新日期 | **不改写** §1–§5 的 2026-10-02 证据列文字 |
| [acceptance README.md](../../../proposals/gameplay-v2-acceptance/README.md) | §1 `RULING_WORKSHEET.md` 行；§2 A4 状态；§4.3 未决段；更新日期 | `open` 21 → 18、第 2 轮状态已裁定 |
| [ADR 0044](../../../adr/0044-gameplay-v2-semantic-kernel.md) | "仍待裁定的部分"段与第 2 轮表行 | **最小同步**：原文称"第 2 至第 7 轮……除 D-13 / D-14 外均尚未裁定"，与本次第 2 轮已裁定的既成事实矛盾，故补一句限定并给第 2 轮行加注；不改任何决策语义与门禁 |

### 4.1 F-04 的最终落点原文

`docs/api/GAMEPLAY_V2_ABI.md` 字段级映射表（`| Gameplay I 字段 | V2 边界表达 |` 表内，紧邻其它
`InputEvent.*` 行），该行原文为：

```text
| `InputEvent.source` | SourceClass（规范事件中的设备/来源类别）；SourceBuildIdentity 只承载生产者、参数、compiler profile 与 source map 的构建来源，不进入判定语义 |
```

## 5. S7A-2 不得消费清单（实现须逐条对齐）

按 S1-05 的"不得用默认值、临时 typedef、序列化编码或伪成功实现绕过阻塞"口径，S7A-2 实现**不得消费**：

1. 浮点或宿主数学库参与 Tick 判定；
2. 隐式秒 / Beat → Tick 转换；
3. 未校准原始时间作为 `observationTick`；
4. 按 ingress 顺序解决同 Tick 冲突；
5. late-policy 数值默认常量；
6. 饱和或未定义溢出；
7. 未声明的 `DomainAmount` 范围 / 量化；
8. 临时整数 typedef；
9. 未冻结序列化编码；
10. 连续轨迹 / minimumRate / reconstruction / discontinuity 表示；
11. S7C-1 扩展字段；
12. 研究切片限额。

该清单同时写入 [Spec §S7A-2 节](../../../formats/GAMEPLAY_V2_SPEC.md) 与
[ABI §S7A-2 节](../../../api/GAMEPLAY_V2_ABI.md)，供实现逐条对齐。

## 6. 跑过的门禁与结果

| 门禁 | 命令 | 结果 |
| --- | --- | --- |
| 文档链接 / 索引 / 可达性 / 单一 H1 / 候选 JSON 一致性 | `python -B tools/check_docs.py` | 见 §6.1（实际输出与 exit code 原文） |
| 行尾空白与制表符扫描（全部改动文件） | 见 §6.2 | 见 §6.2（逐文件 0 处） |
| 陈旧计数 grep | `21 项 open` / `open 21` / `accept 32` / `19 条` / `76 行 / 84 项` / `20 项 open` | 见 §6.3（残余命中逐条解释） |
| 处置列脚本复核 | `python -B tools/_tmp_count_dispositions.py`（临时脚本，跑完即删） | `accept` 35 / `revise` 25 / `supersede` 17 / `retain` 17 / `open` 18 / `reject` 2，共 114 |
| ABI 126 条登记复核 | 按 `### 域 N` 分段统计状态格首词 | 9 域共 126 条，`冻结目标` 96 / `待冻结` 30（逐域与 §126 条追踪规则 表一致） |
| C++ 构建与 CTest | 未运行 | **本轮为纯文档改动**，不修改 C++、CMake、Schema 或生成输入，按仓库政策（AGENTS.md §Documentation check）不需要构建或 CTest |

### 6.1 `check_docs.py` 实际输出

```text
Documentation checks passed: 330 Markdown files and 20 candidate JSON/CXT files validated.
exit code: 0
```

（`330` 为该次运行的 Markdown 计数，含本次新增的本文；`Documentation checks passed` 即 exit 0。）

### 6.2 空白扫描

对本次改动的全部文件（Spec / ABI / RULING_WORKSHEET / CONTRACT_MATRIX / OPEN_QUESTIONS / acceptance
README / 本文）逐行扫描**行尾空白**（含 `\t`）：**0 处**。

### 6.3 陈旧值 grep 与残余命中（逐条解释）

扫描模式与结果（`docs/` 全量）：

| 模式 | 命中位置 | 判定 |
| --- | --- | --- |
| `21 项 \`open\`` | acceptance `README.md` 第 37 行原值（已改）、`RULING_WORKSHEET.md` 第 11 行原值（已改） | **已修**（改为 18 项 `open` 合同项） |
| `\`open\` 21` | `CONTRACT_MATRIX.md` 第 47 行（缺陷 D-14 的**历史计数说明**）；第 263 行（第 2 轮计数变化的**变更日志**行） | **保留**：两处都是"从 21 变到 18"的过程记录，写 21 是正确的历史值；第 0 行统计与 §14 已改为 18 |
| `\`accept\` 32` | `CONTRACT_MATRIX.md` 第 47 行、第 261 行、第 264 行；本报告 §4 的变更日志行 | **保留**：均为变更日志（32 → 35）；当前统计行为 `accept` 35 |
| `20 项 \`open\`` | `2026-10-02-s7a-0-baseline.md` 第 199、207、214 行与第 180 行（`open` 20 项）；`2026-10-02-s7a-0-acceptance.md` 第 21 行 | **保留（历史证据）**：这些是 D-14 之前的 2026-10-02 基线/接受记录，按仓库政策"历史报告不可被改写为新的验证结果"，不改 |
| `19 条` | `2026-10-02-s7a-0-*.md` 第 181 / 22 / 38 行（历史报告）；`GAMEPLAY_V2_ABI.md` 第 470、710 行；`GAMEPLAY_V2_SPEC.md` 第 494（标题）、519、750 行；本报告第 54、63、82 行；acceptance `README.md` 第 30、49、128 行；`RULING_WORKSHEET.md` 第 213、370 行；`ADR 0044` 第 107 行 | **保留**：19 条的 R 清单**本轮未改条数**，上述全部指同一份 R-01…R-19 能力级拒绝面；脚本复核 §7.2 的 `| R-nn |` 行数确为 **19** |
| `76 行 / 84 项` | `RULING_WORKSHEET.md` 第 40、263、336 行；acceptance `README.md` 第 37 行；`ADR 0044` 第 145 行的"62 行 / 69 项"派生口径 | **保留**：本轮**不新增/删除任何分轮表行**，76 行 / 84 项口径不变；第 2 轮只是处置词由 `open` 变 `accept` |

**结论**：除 6.3 表列"已修"的两处外，其余命中都是**历史证据、变更日志或本轮未变的 19 条 / 76 行口径**，
逐条已解释，无需修改。

## 7. 相关索引

- [Stage 7A 实施计划](../../../stage_plans/active/stage-07/plan.md)：§S7A-2 的目的、工作内容与验证要求
- [未决语义分轮裁决清单](../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md)：第 2 轮裁定列与 §9 裁定登记
- [S7A-0.2 合同逐项台账](../../../proposals/gameplay-v2-acceptance/CONTRACT_MATRIX.md)：114 项与 18 项 `open`
- [未决问题与 owner 决策请求](../../../proposals/gameplay-v2-acceptance/OPEN_QUESTIONS.md)：§2.1 第 2 轮裁定登记
- [Gameplay V2 Spec](../../../formats/GAMEPLAY_V2_SPEC.md)：§0.1、§3.7 与 §S7A-2 节
- [Gameplay V2 ABI](../../../api/GAMEPLAY_V2_ABI.md)：域 1 / 域 2 / 域 7、字段级映射表与 §S7A-2 节
- [S7A-1 typed contract review](2026-10-02-s7a-1-typed-contract-review.md)：F-04 的登记来源（历史证据，不回改）
- [S7A-1 冻结绑定登记](../../../proposals/gameplay-v2-acceptance/FREEZE_BLOCKING_BINDINGS.md)：同 thread 前一批次的绑定登记（confidence 0.96）

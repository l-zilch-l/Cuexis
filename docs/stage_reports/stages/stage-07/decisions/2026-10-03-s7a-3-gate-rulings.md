# S7A-3 准入门禁：第 3 轮（prepare、装配与 entry）裁定落地记录

状态：dated implementation record（第 3 轮裁定的文档落地证据；不是阶段关闭、不是 owner acceptance、
不是实施证据）

更新日期：2026-10-03

上级文档：[Stage 7A 实施计划](../../../../stage_plans/active/stage-07/plan.md) §S7A-3 ·
[Gameplay V2 acceptance package](../../../../proposals/gameplay-v2-acceptance/README.md) ·
[未决语义分轮裁决清单](../../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) §3、§8、§9

本文是**带日期的落地记录**（reports own dated evidence）：它登记 Codex 对第 3 轮（prepare、装配与 entry；
进入 S7A-3 前）的裁定、`CM-X05` 的 D-3 依据与最终处置词、逐条落进哪些文档的哪一节、计数变化、`W-01…W-05`
候选项的门禁归属、identity 规范字节边界、S7A-3 的不得消费清单，以及本轮跑过的门禁与结果。它不复制
Spec / ABI 的合同正文，也不构成任何实施声明；未在本文列出的门禁一律视为未运行。

## 1. provenance

| 卡 | thread | 模型 | verdict | confidence | 日期 | 内容 |
| --- | --- | --- | --- | --- | --- | --- |
| 主裁定卡（第 3 轮 5 条裁定 + 规范字节边界） | `s7a3-prepare-rulings` | `gpt-6-astra`（consult） | `need_info` | **0.8** | 2026-10-03 | Q-05 修改后接受、Q-07 修改后接受且只冻结行为、Q-18/P2-10 接受、CM-C10/P1-03 完全展开，以及"规范字节是 prepare 内部派生物"的边界；`CM-X05` 因缺 D-3 原文**本轮暂不判定**，因此该卡为 `need_info` |
| 补答卡（`CM-X05` 处置与配套动作） | `s7a3-prepare-rulings` | `gpt-6-astra`（consult） | `reject` | **0.88** | 2026-10-03 | `CM-X05` **已由 D-3 的落地处置关闭**，但应记为 `revise` 而非 `accept`；SUPPORT §6 第 165 行改写；`W-01…W-05` 标为研究稿候选并给出各条门禁归属；不恢复旧行号对应的四处引用 |
| 计数一致性确认卡 | `s7a3-prepare-rulings` | `gpt-6-astra`（consult） | `adopt` | **0.99** | 2026-10-03 | 确认第 3 轮落地后为 `open` **16** / `accept` **36** / `revise` **26**、总数仍 **114**；§8 第 3 轮分布降为 **0**；§0.1 末列**仍保留** `CM-X05`、`CM-C10` |

**日期口径说明。** 三张卡的文件名时间戳为 2026-10-02T19:42Z / 19:44Z / 19:46Z（UTC），裁定与落地按
**本地日期 2026-10-03** 登记；本文、Spec §3.8 / §5.5 / §S7A-3、ABI §S7A-3 与各计数登记均使用 2026-10-03。

**verdict 口径说明。** 主裁定卡为 `need_info`，其含义是**该卡自身的第（1）问（`CM-X05`）未决**，其余五项
（Q-05、Q-07、Q-18/P2-10、CM-C10/P1-03、规范字节边界）在该卡中已给出**可直接执行**的裁定；`CM-X05` 由
补答卡按 D-3 关闭。三张卡共同构成本轮 6 行裁定，不因主卡 verdict 不是 `adopt` 而视为未裁定。

## 2. 裁定要点

| # | 编号 | 裁定要点 | 落点 |
| --- | --- | --- | --- |
| 1 | Q-05（修改后接受，**阻塞 S7A-3**） | Requirement **不含** Gameplay `effects`；非空 `effects` 只能**显式 lowering 为不影响 Judgement / Replay 的 Presentation 数据**，否则**拒绝** | Spec §3.3、§3.8.1、§7.2；ABI 域 3 `RequirementRecord` |
| 2 | Q-07（修改后接受，**阻塞 S7A-3**） | `REQ0` / `CNS0` 保持 Foundation 语义；7A 新语义须使用**可与旧语义区分的 entry / section 边界**；未知必需 section、未知 capability、新 requirement kind **稳定拒绝**。**第 3 轮只冻结行为，不冻结 wire 编号、布局或序列化编码**；具体表示在 S7A-3 消费前另行闭合 | Spec §2.5、§3.8.2；ABI 域 8 与 §S7A-3 节 |
| 3 | Q-18（接受）+ P2-10（随 Q-18 关闭，不另设选择）（**阻塞 S7A-3**） | Release / tail **仅作为同一 Requirement 显式声明的可选 phase**；内容要求 tail 语义却未声明时**拒绝** | Spec §3.8.3、§7.2；ABI 域 3 `Phase` |
| 4 | CM-C10 / P1-03（修改后接受，**阻塞 S7A-3**） | 7A 的 prepare-time 有界循环**完全展开**，展开结果**唯一决定** identity / snapshot / fact order；`boundedRelationInstance` **不进 7A 合同**，遇到时**稳定拒绝**，列为 **7B+ / S7C 候选**。7A 选择阻塞 S7A-3；候选表示只阻塞**首次拟消费它**的后续批次 | Spec §3.8.4、§7.2；ABI 域 3 `PatternPrimitive`；CONTRACT_MATRIX §3 |
| 5 | 规范字节边界（张力点裁定） | identity 的**规范字节是 prepare 内部确定性派生物**，**不是公共 ABI 或 Packed 编码**；冻结**语义等价、排序与身份域边界**，**暂不冻结编码**；S7A-3 实现须用**同一算法在各工具链产出相同字节**，以**跨工具链 golden、输入置换、file / memory 对照**验证；**编码实现须在 S7A-3 消费前闭合**；ABI **仅声明对外 identity 的不透明性**，不承诺内部字节格式 | Spec §5.5 与 §S7A-3 节；ABI 域 7、"明确不承诺"清单与 §S7A-3 节 |
| 6 | CM-X05（由 D-3 落地关闭） | 处置词 `open` → **`revise`**（**不是** `accept`）；未决点栏改为"W 类缺口原无定义；D-3 修订后由 plan §5.3 规定登记与引用规则，SUPPORT §6 定义字段"，并注明"**D-3 已落地，CM-X05 关闭，不再阻塞 S7A-3**"；门禁清单标为"**D-3 已关闭；仅具体 W 缺口在首次消费前须完成登记**" | CONTRACT_MATRIX §12 与 §14；SUPPORT §6 与 §6.1；Spec §1.3 |

### 2.1 登记口径（门禁归属）

- **阻塞 S7A-3 的门禁**：Q-05、Q-07、Q-18 / P2-10、CM-C10 / P1-03 的语义，以及 identity 的规范字节边界
  （**编码实现**须在 S7A-3 消费前闭合）。
- **不阻塞 S7A-3**：`CM-X05`（D-3 已落地关闭；仅具体 W 缺口在首次消费前须完成登记）。
- **只阻塞首次拟消费它的后续批次**：`boundedRelationInstance` 候选表示（7B+ / S7C）与 SUPPORT §6.1 的
  `W-01…W-05` 候选项；**S7A-3 若具体消费 `W-03` / `W-04` / `W-05`，须先完成相应记录**，不消费的候选项
  不构成该批次门禁。
- **只冻结行为、不冻结表示**：Q-07 的新 wire 编号 / section 布局 / 序列化编码，以及 §3.8.4 的
  `boundedRelationInstance` 具体 `stableRejectCode`；两者在 S7A-3 **消费前**另行闭合。

## 3. `CM-X05` 的 D-3 依据与最终处置

| 项 | 内容 |
| --- | --- |
| 缺陷本体 | 计划正文引用"W 类缺口"，而**仓库内没有任何文档定义它的编号、字段或登记位置**（[CONTRACT_MATRIX.md](../../../../proposals/gameplay-v2-acceptance/CONTRACT_MATRIX.md) §12 原记"仓库内无定义出处"） |
| D-3 的既有裁定 | [RULING_WORKSHEET.md](../../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) 第 1 轮 D-3 行：采用 SUPPORT §6 的字段定义，但**Codex 修订把落点改为 Stage plan**（不落 V2 Spec 附录），并给计划那 4 处引用补定义链接；涉及具体缺口时必须附 `wId`，**不得编造编号** |
| D-3 已落地的证据 | `plan.md` §5.3 已定义"W 类缺口"及登记 / 引用规则（字段定义的唯一权威仍指 SUPPORT §6）；现存正文引用（plan 第 703、724 行）均显式指向 §5.3；[S7A-0 接受记录](../readiness/2026-10-02-s7a-0-acceptance.md) 第 96 行登记"D-3 已落地"（该报告为 2026-10-02 历史证据，**本次不回改**） |
| 最终处置词 | `open` → **`revise`**（**不是** `accept`）：D-3 改变了原建议的定义落点，且 SUPPORT §6 的五列候选表**缺少** `attemptedWorkarounds` / `stableRejectCode` / `ownerDecision` / `reviewDate`，不能视为已登记条目 |
| 门禁结论 | **D-3 已落地，`CM-X05` 关闭，不再阻塞 S7A-3**；门禁清单标为"**D-3 已关闭；仅具体 W 缺口在首次消费前须完成登记**" |
| 不做的动作 | **不恢复**旧行号对应的四处引用（plan 正文现存两处引用已指向 §5.3，**无需补链**）；本轮**不改** plan §5.3 的 W 类缺口规则本身（仅在后继事实同步中修正"哪些轮次尚未裁定"的轮次范围表述，见 §9.4 第 222 行） |

> **行号订正（2026-10-03 17:20 追加，上面的第 703、724 行是 03:58 的测量值，原文不改）**：
> 此后计划正文有两批位移——(a) **第 4–7 轮的裁定块**插入正文（+98 行）；(b) 2026-10-03 对
> `### S7A-3` 两块裁定段的**纯格式整理**（只切段与调缩进，归一化后零字改动，+17 行）。
> 现值：§5.3 定义在 **L729**，两处**指向 §5.3 的正文引用**在 **L818 / L839**（+17 与上述 +98 共同造成）。
> **结论不变**（两处仍显式指向 §5.3，不需要补链），但这也再次印证：**行号只能定位、不能归属**，
> 引用定义位置时应优先给**章节号**（本节给的就是 `§5.3`）。

## 4. 计数变化

| 计数 | 第 3 轮前 | 第 3 轮后 | 依据 |
| --- | --- | --- | --- |
| `CONTRACT_MATRIX` 总数 | 114 | **114** | 只改处置词，不增删条目 |
| `accept` | 35 | **36** | `CM-C10` `open` → `accept` |
| `revise` | 25 | **26** | `CM-X05` `open` → `revise` |
| `supersede` | 17 | 17 | 未变 |
| `retain` | 17 | 17 | 未变 |
| `open` | 18 | **16** | `CM-X05`、`CM-C10` 同时移出 |
| `reject` | 2 | 2 | 未变 |
| `CONTRACT_MATRIX` §14 的 `open` 清单条数 | 18 | **16** | 移出 `CM-X05`、`CM-C10` 并加带日期的变更说明 |
| `RULING_WORKSHEET` §8 的 `open` 行 | 18（第 3 轮 2） | **16（第 3 轮 0）** | 分轮分布改为第 1 轮 5、第 2 轮 0、第 3 轮 **0**、第 4 轮 2、第 5 轮 4、第 6 轮 5 |
| `RULING_WORKSHEET` §0.1 行项合计 / 末列 | 76 行 / 84 项；21 | **76 行 / 84 项；21** | 本轮不新增 / 删除分轮表行；末列语义是"由哪一轮一并关闭"，**仍保留** `CM-X05`、`CM-C10` |
| `GAMEPLAY_V2_SPEC` §7.2 拒绝清单 | 19 | **19** | 第 3 轮三类拒绝**不新增 R 条目**（映射见 Spec §7.2 第 3 轮段） |
| 包 `README.md` 的 `open` 计数 | 18 | **16** | 与台账一致 |
| §8 的分类串 | `18 + 19 + 15 + 10 + 14 + 3 + 2 = 81` | **`16 + 19 + 15 + 10 + 14 + 3 + 2 = 79`** | `open` 类由 18 降为 16；该串与"76 行 / 84 项"口径不同，不能互相替代 |

## 5. `W-01…W-05` 的处置与候选项门禁归属

`SUPPORT_AND_REJECTION_MATRIX.md` §6 的 5 行是**研究稿候选**：其编号**仅为候选标识**，尚**不是**已登记的
W 类缺口，**不得作为已登记 `wId` 引用**；表中未给出 `attemptedWorkarounds`、`stableRejectCode`、
`ownerDecision` 与实际 `reviewDate`，本文件与 SUPPORT **不代填、不预填拒绝码或日期**。具体条目**首次被某批次
引用或用于稳定拒绝前**，由**该批次**补全全部字段，并按 `S1-03` 标注**裁决编号 + 首次消费批次**。

| 候选项 | 门禁归属（首次实际消费批次） |
| --- | --- |
| W-01 | 运行期 lease / 和弦定序批次 |
| W-02 | 先核对**已闭合的 F-04**；**只有剩余重开语义被消费时**才登记 |
| W-03 | S7A-3 的**未展开循环表示的拒绝边界** |
| W-04 | **首次消费跨 Requirement 关系**的批次 |
| W-05 | **首次将判定几何纳入 closure** 的批次 |

**S7A-3 若具体消费 `W-03` / `W-04` / `W-05`，须先完成相应记录；不消费的候选项不构成该批次门禁。**

## 6. 规范字节边界（张力点裁定）

问题：plan §S7A-3 第 5 条要求"生成 chart / content / prepared identity 的规范字节"，而 S1-05 禁止本批次
冻结序列化编码。裁定与落点如下：

1. **性质。** identity 的规范字节是 **prepare 内部确定性派生物**，**不是公共 ABI**，也**不是 Packed 编码**。
2. **冻结面。** 冻结**语义等价、排序与身份域边界**（哪些字段进哪条 identity、相等判定依据、规范排序）；
   **暂不冻结编码**。
3. **验证面。** S7A-3 实现须用**同一算法在各工具链产出相同字节**，并以**跨工具链 golden、输入置换
   （数组 / 参数 / 引用顺序）与 file / memory 对照**验证。
4. **闭合时点。** **编码实现须在 S7A-3 消费前闭合**；在此之前不得把任一内部字节序当作已冻结合同，也不得
   据它单独做产物互换性判定。
5. **ABI 侧。** **仅声明对外 identity 的不透明性**，不承诺内部字节格式。

落点原文见 [Spec §5.5](../../../../formats/GAMEPLAY_V2_SPEC.md) 与
[ABI 域 7](../../../../api/GAMEPLAY_V2_ABI.md)（"对外 identity 的不透明性"段）。

## 7. S7A-3 不得消费清单（实现须逐条对齐）

按 plan §S7A-3 的验证要求与 S1-05 的"不得用默认值、临时 typedef、序列化编码或伪成功实现绕过阻塞"口径，
S7A-3 实现**不得消费**：

1. 旧 revision 的隐式 V2 解释；
2. 旧 `REQ0` / `CNS0` 对新 kind 的兼容解释；
3. 未定的新 wire 布局或编号；
4. Gameplay `effects`（未显式 lowering 为不影响判定的 Presentation 数据者）；
5. 隐式 Release / tail；
6. `boundedRelationInstance`；
7. 运行期 repeat 计数或动态 Requirement；
8. `sameContact`；
9. 连续轨迹；
10. 跨 Requirement relation / resource migration；
11. handoff Hold；
12. 接触跟随 Slider；
13. 未闭合的数值限额、默认枚举、临时整数 typedef 与公共序列化编码。

该清单同时写入 [Spec §3.8.5](../../../../formats/GAMEPLAY_V2_SPEC.md)、
[ABI §S7A-3 节](../../../../api/GAMEPLAY_V2_ABI.md) 与 [BATCH_GATES.md](../../../../proposals/gameplay-v2-acceptance/BATCH_GATES.md)
的 S7A-3 负例 / 停止条件。

## 8. 改动的文档与落点

| 文件 | 落点 | 要点 |
| --- | --- | --- |
| [GAMEPLAY_V2_SPEC.md](../../../../formats/GAMEPLAY_V2_SPEC.md) | §1.2 权威位置行拆分（W 类缺口字段定义 → SUPPORT §6）；§1.3 引用边界；§2.5 第 1 条；§3.3 的 Q-05 段；新增 §3.8（含 3.8.1–3.8.5）；新增 §5.5；§7.2 第 3 轮段；§S7A-2 节的"第 3 至第 7 轮"→"第 4 至第 7 轮"；§11 第 3 轮行改"已裁定"并新增后续批次行；§11.1 增第 5 条；新增 `## S7A-3 限定冻结范围与登记规则（2026-10-03）` | 五条裁定 + 规范字节边界 + 不得消费清单 + 19 条不变的映射判定 |
| [GAMEPLAY_V2_ABI.md](../../../../api/GAMEPLAY_V2_ABI.md) | 候选状态段；域 3 三行 + "第 3 轮已冻结的事实"段；域 7 不透明性段；域 8 `EntryKind` 与"entry 与 section 的行为边界"段；§稳定与非稳定声明；§仍待冻结 / §未决项 #3 与新增 #19；新增 `## S7A-3 限定冻结范围与登记规则（2026-10-03）` | 保留既有登记首词（`冻结目标` / `待冻结`），只补已冻结事实 |
| [RULING_WORKSHEET.md](../../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) | 文档角色句（18 → 16）；§0.1 第 3 轮末列口径段；§3 裁定列逐行填写 + 三条注；§8 `open` 行 18 → 16 与口径段；§9 新增第 3 轮行；更新日期 | §0.1 末列仍为 **21**（列语义＝"哪一轮一并关闭"） |
| [CONTRACT_MATRIX.md](../../../../proposals/gameplay-v2-acceptance/CONTRACT_MATRIX.md) | §0 统计与说明；§3 `CM-C10` `open` → `accept`；§12 `CM-X05` `open` → `revise`；§14 计数、`open` 清单、S7A-3 行与变更说明；更新日期 | `accept` 35 → 36、`revise` 25 → 26、`open` 18 → 16，总数仍 114 |
| [SUPPORT_AND_REJECTION_MATRIX.md](../../../../proposals/gameplay-v2-acceptance/SUPPORT_AND_REJECTION_MATRIX.md) | §6 开篇缺陷说明与登记位置句；新增 §6.1 候选项地位 / 门禁归属；更新日期 | **保留 5 行原样内容**，不补字段值 |
| [OPEN_QUESTIONS.md](../../../../proposals/gameplay-v2-acceptance/OPEN_QUESTIONS.md) | Q-05 / Q-07 / Q-18 行批次格追加裁定指针；新增 §3.1 第 3 轮裁定登记表与登记口径；§6 第三批标注已裁定；更新日期 | **不改写** §1–§5 的 2026-10-02 证据列文字 |
| [acceptance README.md](../../../../proposals/gameplay-v2-acceptance/README.md) | §1 `SUPPORT` 与 `RULING_WORKSHEET` 行；§2 A4 状态；§4.3 未决段；更新日期 | `open` 18 → 16、第 3 轮状态已裁定 |
| [BATCH_GATES.md](../../../../proposals/gameplay-v2-acceptance/BATCH_GATES.md) | §1 S7A-3 的负例 / golden / 停止条件 + 新增"第 3 轮裁定后的 S7A-3 门禁"段；更新日期 | `W-03`/`W-04`/`W-05` 的首次消费登记要求写入停止条件 |
| [ADR 0044](../../../../adr/0044-gameplay-v2-semantic-kernel.md) | "仍待裁定的部分"段、第 3 轮表行、威胁 5、证据边界 | **最小同步**：把"第 3 至第 7 轮尚未裁定"改为"第 4 至第 7 轮"，并把候选 / W 类缺口措辞与本次事实对齐；不改任何决策语义与接受门禁 |
| [plan.md](../../../../stage_plans/active/stage-07/plan.md) 第 99 行、[CURRENT_STATUS.md](../../../../CURRENT_STATUS.md) 第 33 / 241 行、[reading-order.md](../../../../guides/reading-order.md) 第 61 行、Spec §S7A-1 节、ABI §S7A-1 节 | **落地后复核的事实同步（2026-10-03）**：`第 2（至 / -）7 轮尚未裁定` → `第 4（至 / -）7 轮尚未裁定`（Spec 第 777 行、ABI 第 546 行、plan 第 99 行、CURRENT_STATUS 第 241 行、reading-order 第 61 行）；CURRENT_STATUS 第 33 行的 Stage 7A 行另补"第 1、2、3 轮语义已裁定" | **不是新裁定**：这些都是**现行**授权 / 引导表述（不是历史证据），第 2、3 轮裁定落地后原措辞已不成立；同步后全仓该措辞为 **0 处**，历史报告与带日期记录一律保留原样 |

## 9. 跑过的门禁与结果

| 门禁 | 命令 | 结果 |
| --- | --- | --- |
| 文档链接 / 索引 / 可达性 / 单一 H1 / 候选 JSON 一致性 | `python -B tools/check_docs.py` | 见 §9.1（实际输出与 exit code 原文） |
| 行尾空白与制表符扫描（全部改动文件） | 见 §9.2 | 见 §9.2（逐文件 0 处） |
| 计数脚本直读复核 | 见 §9.3 | 六类处置 114 / §14 `open` 16 / §8 `open` 16 / §0.1 76 行 84 项 / §7.2 19 条 |
| ABI 126 条登记复核 | 按 `### 域 N` 分段统计状态格首词 | 9 域共 126 条，`冻结目标` 96 / `待冻结` 30（逐域与 §126 条追踪规则 表一致；第 3 轮只补事实，不改首词） |
| 陈旧值 grep | `open` 18 / `accept` 35 / `revise` 25 / `20 项 open` / `21 项 open` | 见 §9.4（残余命中逐条解释） |
| C++ 构建与 CTest | 未运行 | **本轮为纯文档改动**，不修改 C++、CMake、Schema 或生成输入，按仓库政策（AGENTS.md §Documentation check）不需要构建或 CTest |

### 9.1 `check_docs.py` 实际输出

```text
> python -B tools/check_docs.py
Documentation checks passed: 332 Markdown files and 20 candidate JSON/CXT files validated.
exit code: 0
```

该计数包含本轮新增的本报告；同一命令在本轮改动前为 331 个 Markdown 文件。`git diff --check` 同步返回
exit 0（其输出的 `LF will be replaced by CRLF` 仅为行尾转换提示，且其中多数文件不是本轮改动目标）。

### 9.2 空白扫描

对本次改动的全部文件（Spec / ABI / RULING_WORKSHEET / CONTRACT_MATRIX / SUPPORT / OPEN_QUESTIONS /
acceptance README / BATCH_GATES / ADR 0044 / stage_reports README / 本文）逐行扫描**行尾空白**（含制表符）：

```text
docs/formats/GAMEPLAY_V2_SPEC.md: trailing=0 tabs=0
docs/api/GAMEPLAY_V2_ABI.md: trailing=0 tabs=0
docs/proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md: trailing=0 tabs=0
docs/proposals/gameplay-v2-acceptance/CONTRACT_MATRIX.md: trailing=0 tabs=0
docs/proposals/gameplay-v2-acceptance/SUPPORT_AND_REJECTION_MATRIX.md: trailing=0 tabs=0
docs/proposals/gameplay-v2-acceptance/OPEN_QUESTIONS.md: trailing=0 tabs=0
docs/proposals/gameplay-v2-acceptance/README.md: trailing=0 tabs=0
docs/proposals/gameplay-v2-acceptance/BATCH_GATES.md: trailing=0 tabs=0
docs/adr/0044-gameplay-v2-semantic-kernel.md: trailing=0 tabs=0
docs/stage_reports/README.md: trailing=0 tabs=0
docs/stage_reports/stages/stage-07/2026-10-03-s7a-3-gate-rulings.md: trailing=0 tabs=0
```

### 9.3 计数脚本直读复核

脚本按 `CONTRACT_MATRIX.md` 的处置列计数、按 §14 批次表与 `RULING_WORKSHEET.md` §8 逐行统计：

```text
CONTRACT_MATRIX rows: 114
dispositions: accept 36, revise 26, supersede 17, retain 17, open 16, reject 2
§14 open list entries: 16
RULING_WORKSHEET §8 open row: 16  (第 1 轮 5、第 2 轮 0、第 3 轮 0、第 4 轮 2、第 5 轮 4、第 6 轮 5)
§0.1 totals: 76 行 / 84 项, 末列 21
§0.1 第 3 轮行末列: CM-X05、CM-C10（保留；列语义＝"由哪一轮一并关闭"）
SPEC §7.2 rejection rows: 19
ABI 类型清单: 9 域 / 126 条 = 冻结目标 96 + 待冻结 30
```

### 9.4 陈旧值 grep 与残余命中（逐条解释）

扫描模式与判定：

| 模式 | 残余命中 | 判定 |
| --- | --- | --- |
| `open` 18 / `18 项 \`open\`` | `CONTRACT_MATRIX.md` 第 50、54、265、268、272 行；`OPEN_QUESTIONS.md` 第 116–117 行；acceptance `README.md` 第 147 行；`RULING_WORKSHEET.md` 第 348–356 行（§8 口径段） | **保留**：全部是**变更日志**（"18 → 16"）或 §8 口径段对 21 → 18 → 16 的历史推导；当前统计行、§14 清单、§8 `open` 行与包 README 的现行值均为 **16** |
| `` `accept` 35 `` / `` `revise` 25 `` / `` `accept` 32 `` | `CONTRACT_MATRIX.md` 第 47、50、269、273 行；`OPEN_QUESTIONS.md` 第 116–117 行；acceptance `README.md` 第 147 行 | **保留**：均为变更日志（32 → 35 → 36、25 → 26）；现行统计为 36 / 26 |
| `20 项 \`open\`` | `2026-10-02-s7a-0-baseline.md` 第 180、199、207、214 行；`2026-10-02-s7a-0-acceptance.md` 第 21 行 | **保留（历史证据）**：D-14 之前的 2026-10-02 基线与接受记录；按仓库政策"历史报告不可被改写为新的验证结果"，**不改** |
| 末列 `21` | `RULING_WORKSHEET.md` §0.1 合计行（第 40 行）与 §0.1 改口径段（第 44–50 行）；`CONTRACT_MATRIX.md` 第 47、265、268 行 | **保留**：§0.1 末列语义是"**由哪一轮一并关闭**"，与当前 `open` 无关（第 3 轮行仍保留 `CM-X05`、`CM-C10`）；台账中的 21 属变更日志 |
| `19 条` | Spec §7.2、ABI 相关索引、SUPPORT §7.1、acceptance README §4.2、CONTRACT_MATRIX §13 等 | **保留**：R 清单本轮未改条数；脚本复核 §7.2 的 `| R-nn |` 行数确为 **19** |
| `76 行 / 84 项` | `RULING_WORKSHEET.md` §0.1 与 §8、Spec/ABI §S7A-3 节、acceptance README §1 | **保留**：本轮不新增 / 删除任何分轮表行，口径不变 |
| `第 2 至第 7 轮尚未裁定`（含 `plan.md` 的"第 2-7 轮"写法） | 初稿复核时 3 处：Spec §S7A-1 节第 777 行、ABI §S7A-1 节第 546 行、`plan.md` 第 99 行 | **已在落地后复核中同步**：这三处是**现行授权表述**（不是历史证据），第 2、3 轮裁定后"第 2 至第 7 轮尚未裁定"已不成立，故改为"**第 4 至第 7 轮**尚未裁定"，并注明第 2、3 轮已裁定的语义由各自小节授权、仍不在本节 / 本批次范围内。属**事实同步，不是新裁定**；同步后仓库内该措辞为 **0 处** |
| `第 3 至第 7 轮尚未裁定` | **0 处** | 已在本轮改写：Spec §S7A-2 节与 ADR 0044 的"仍待裁定的部分"段；仓库内不再有此措辞 |

**结论**：残留命中都是**变更日志、历史证据、§0.1 末列语义，或已同步的轮次范围表述**，逐条已解释。
其中"第 2 至第 7 轮尚未裁定"的 3 处已由**落地后复核的事实同步**改为"第 4 至第 7 轮"（上表第 222 行），
其余命中按上表保留。

## 10. 相关索引

- [Stage 7A 实施计划](../../../../stage_plans/active/stage-07/plan.md)：§S7A-3 的目的、工作内容与验证要求
- [未决语义分轮裁决清单](../../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md)：第 3 轮裁定列与 §9 裁定登记
- [S7A-0.2 合同逐项台账](../../../../proposals/gameplay-v2-acceptance/CONTRACT_MATRIX.md)：114 项与 16 项 `open`
- [支持 / 拒绝矩阵与 capability registry 草案](../../../../proposals/gameplay-v2-acceptance/SUPPORT_AND_REJECTION_MATRIX.md)：§6 字段定义与 §6.1 候选项门禁归属
- [未决问题与 owner 决策请求](../../../../proposals/gameplay-v2-acceptance/OPEN_QUESTIONS.md)：§3.1 第 3 轮裁定登记
- [批次证据门禁表](../../../../proposals/gameplay-v2-acceptance/BATCH_GATES.md)：S7A-3 的正例 / 负例 / 停止条件
- [Gameplay V2 Spec](../../../../formats/GAMEPLAY_V2_SPEC.md)：§1.3、§3.8、§5.5 与 §S7A-3 节
- [Gameplay V2 ABI](../../../../api/GAMEPLAY_V2_ABI.md)：域 3 / 域 7 / 域 8 与 §S7A-3 节
- [S7A-0 接受记录](../readiness/2026-10-02-s7a-0-acceptance.md)：D-3 已落地的登记来源（历史证据，不回改）
- [S7A-2 准入门禁记录](2026-10-03-s7a-2-gate-rulings.md)：上一轮的落地先例（日期口径与门禁写法）

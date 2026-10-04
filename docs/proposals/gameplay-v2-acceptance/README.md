# Gameplay V2 acceptance package

状态：active

owner 接受：2026-10-02（记录见 [S7A-0 接受记录](../../stage_reports/stages/stage-07/2026-10-02-s7a-0-acceptance.md)）
更新日期：2026-10-04

上级文档：[Stage 7A 实施计划](../../stage_plans/active/stage-07/plan.md) ·
[Gameplay V2 redesign research](../research/gameplay-v2/README.md)

文档角色：准入包。它把 Gameplay V2 研究稿收敛为**可逐条裁定的合同清单、支持/拒绝矩阵与决策请求**，
使 S7A-0 的退出门禁（"V2 ADR/Spec/ABI/Schema/Replay/diagnostics 状态已明确；旧 Gameplay I 合同的
替代关系已记录；所有未决语义均有 owner 决策或阻塞项；无'实施时再决定'的公共字段"）可以被核验。

它**不是** ADR、Spec、ABI、Schema 或 Replay 合同正文。owner 已接受本包（2026-10-02），因此：

- [ADR 0043](../../adr/0043-gameplay-judgement-ruleset-convergence.md)、
  [Gameplay Judgement Spec](../../formats/GAMEPLAY_JUDGEMENT_SPEC.md) 与
  [Gameplay Judgement ABI](../../api/GAMEPLAY_JUDGEMENT_ABI.md) 的权威状态只能按
  [接受记录](../../stage_reports/stages/stage-07/2026-10-02-s7a-0-acceptance.md) §4 的冻结顺序改动
  （先建 V2 文档、后标注替代关系），不得零散改动；
- 在 P0 未全部裁定前，仍不得进入公共头或产品实现批次（计划 §1.1）；
- 本 package 的任何内容都不得被表述为"已实施"或"生产合同"；V2 文档在冻结完成前同样保持 candidate。

## 1. 包内文档

| 文档 | 覆盖的 S7A-0 产出 | 内容 |
| --- | --- | --- |
| [CONTRACT_MATRIX.md](CONTRACT_MATRIX.md) | S7A-0.2 合同逐项台账 | 114 项合同项：V2 来源、Gameplay I 现状、处置词、默认值、失败路径、identity 影响、未决点；末节给出旧合同的最小编辑清单与逐批次的 `open` 清单（**第 7 轮后 `open` 为 0**：`accept` 52 / `revise` 26 / `supersede` 17 / `retain` 17 / `reject` 2） |
| [SUPPORT_AND_REJECTION_MATRIX.md](SUPPORT_AND_REJECTION_MATRIX.md) | 支持/拒绝矩阵 | 7A 支持集合、19 条明确拒绝、capability registry 草案、H 类政策、W 类缺口字段定义（§6）与候选项地位 / 门禁归属（§6.1）、capability 派生来源 |
| [FORMAT_ENTRY_AND_IDENTITY.md](FORMAT_ENTRY_AND_IDENTITY.md) | Chart v5/CXC entry 矩阵、identity 分区 | 版本层级接受/拒绝矩阵、8 类载体与 Playback entry、四类身份与四分量、Requirement identity、Packed 承载、迁移矩阵 |
| [TARGETS_AND_DEPENDENCIES.md](TARGETS_AND_DEPENDENCIES.md) | 依赖与安装图 | 模块方案二选一、新增 target 草案、allowlist 草案、安装/导出影响、新增模块的 15 步、依赖图与禁止边 |
| [BUDGET_AND_EVIDENCE_PLAN.md](BUDGET_AND_EVIDENCE_PLAN.md) | S7A-0.3 预算与证据计划 | 五类 profile 的计数口径/属性/超限动作、现有研究证据登记、与 V2 的差距、测量计划 |
| [BATCH_GATES.md](BATCH_GATES.md) | S7A-0 工作内容 5 | S7A-0…S7A-9 每批的正例/负例/诊断/golden/命令/证据文件/停止条件 |
| [STAGE6_HANDOVER_LEDGER.md](STAGE6_HANDOVER_LEDGER.md) | 四项 Stage 6 交接台账 | 逐项已实现/未接线/阻塞/门禁入口/最小关闭证据，含三处新发现的命名与引用缺陷 |
| [OPEN_QUESTIONS.md](OPEN_QUESTIONS.md) | 未决问题清单 | 15 条 P0、4 条追加未决、15 条 P1、10 条 P2、14 条本轮新发现缺陷，以及按批次排序的裁定清单 |
| [RULING_WORKSHEET.md](RULING_WORKSHEET.md) | 逐轮裁决工作表 | 把合同项（登记时为 5 项 `open`，第 7 轮后为 **0**）与 53 项未决问题合并为 7 轮可批清单（76 行 / 84 项登记，**第 1–7 轮已全部裁定、无待裁定轮次**），给出建议处置、不决定的后果与裁定登记表，并以 §7B 登记 16 条 `待冻结` 的最终绑定、以 §2 / §3 / §4 / §5 / §6 / §7 / §9 登记第 2、3、4、5、6、7 轮裁定 |
| [FREEZE_BLOCKING_BINDINGS.md](FREEZE_BLOCKING_BINDINGS.md) | S1-03 登记缺陷的处置登记 | 为 [Gameplay V2 ABI](../../api/GAMEPLAY_V2_ABI.md) 中 16 条缺可追溯阻塞批次的 `待冻结` 条目登记最终裁决编号与首次消费批次、分组小结、其余 14 条的逐条核实、Codex 裁定栏与停止条件；**已由 Codex 裁决并落地登记（thread `s7a1-freeze-bindings`，verdict `adopt`，confidence 0.96，2026-10-03），不改任何既有文档的 Spec 语义** |
| [S7A-3_PACKED_W1_CANDIDATE.md](S7A-3_PACKED_W1_CANDIDATE.md) | S7A-3 P1-W1 设计选择历史 | 三种组织方案与增强 A 的选择理由；2026-10-04 按 owner 授权完成文档裁决；完整物理字段仅由 [Gameplay Capsule v2 format](../../formats/GAMEPLAY_CAPSULE_V2_FORMAT.md) 拥有，E1 实现证据仍待完成 |
| 基线报告 | S7A-0.1 固定执行基线 | `docs/stage_reports/stages/stage-07/2026-10-02-s7a-0-baseline.md` |

## 2. 接受清单（acceptance checklist）

下表是接受过程与逐轮裁定记录，不拥有当前实施状态。2026-10-04 的 S7A-3 设计收口见
[设计报告](../../stage_reports/stages/stage-07/2026-10-04-s7a-3-design-closure.md)；
局部实现与未完成验收以 [CURRENT_STATUS](../../CURRENT_STATUS.md) 为准。

owner 接受本 package 时，请逐条确认：

| # | 确认项 | 状态（2026-10-02 接受后） |
| --- | --- | --- |
| A1 | V2 的版本层级固定为 `version = 5` + `gameplay.version = 2`，不引入 `semanticRevision` | 接受（建议）；待第 1 轮裁定登记 |
| A2 | Canonical Gameplay Graph 的物理归属选定（Chart JSON 区段 / CXC typed entry / 中间层） | 接受（建议）；待第 1 轮裁定登记 |
| A3 | 7A 只交付 [SUPPORT_AND_REJECTION_MATRIX.md](SUPPORT_AND_REJECTION_MATRIX.md) §2 支持集合内的能力，其余按 19 条拒绝清单稳定拒绝 | 已接受 |
| A4 | 15 条 P0 与 4 条追加未决逐条裁定（接受 / 修改后接受 / 拒绝 / 登记阻塞） | **已裁定并登记（第 1–7 轮全部已完成）**（[RULING_WORKSHEET.md](RULING_WORKSHEET.md) 的 7 轮协议）：第 1 轮 2026-10-02；第 2 轮 2026-10-03 时间域与迟到策略；第 3 轮 2026-10-03 prepare、装配与 entry；第 4 轮 2026-10-03 仲裁、资源与事实序；第 5 轮 2026-10-03 Ruleset、事实序、Score 与 Snapshot；第 6 轮 2026-10-03 发布粒度、表现桥接与诊断；第 7 轮 2026-10-03 收尾澄清与缺陷（16 行 / 18 项、`S7A7-R01…R16`）。**无待裁定轮次；`open` 集合为空（0 项）**。**裁定完成不等于实现完成**（本表不重新定义实施状态；`D-9` owner-only、`D-12` 待 owner 确认） |
| A5 | 9 条新发现缺陷（D-1…D-9）的处置方式 | 已接受：D-1 已订正；D-3/D-4/D-8 在第 1 轮；D-2/D-5/D-6/D-7/D-9 在第 7 轮（另 `D-10` 随第 1 轮、`D-11`/`D-12`/`D-13`/`D-14` 在第 7 轮） |
| A6 | 旧 Gameplay I 三份合同的处置映射（第 3 节） | 已接受（执行时点见[接受记录](../../stage_reports/stages/stage-07/2026-10-02-s7a-0-acceptance.md) §4） |
| A7 | 四项 Stage 6 交接收口的本阶段目标与例外 | 已接受（只确认目标，不解除关闭前置） |
| A8 | Gameplay 运行时预算在 S7A-9 前保持未冻结，只提供计数与报告 | 已接受 |
| A9 | 接受后按 §2.1 执行文档编辑，并建立独立的 V2 ADR / Spec / typed ABI | 已接受，执行中 |
| A10 | 认可 V2 的 Schema / Replay / Snapshot / diagnostics 工件**尚未创建**（§2.2），其创建属接受后的独立工作项，不属 S7A-0 | 已接受 |

### 2.1 接受后要执行的文档动作

```text
新建  docs/adr/0044-gameplay-v2-semantic-kernel.md          V2 决策与威胁模型
新建  docs/formats/GAMEPLAY_V2_SPEC.md                      V2 字段与运行语义（含 W 类缺口附录）
新建  docs/api/GAMEPLAY_V2_ABI.md                           V2 typed preview 边界
修订  docs/proposals/research/gameplay-v2/CHART_V5_GAMEPLAY_AMENDMENT.md   与 PD §4.1 对齐
修订  docs/formats/CXT_V2_FORMAT.md                        降级 Requirement effects
修订  docs/formats/CXC_FORMAT.md                           增加 entryKind 与 manifest closure
修订  docs/formats/PACKED_CHART_FORMAT.md                  仅增加 V2 section 的 candidate wire revision 说明
标注  ADR 0043 / GAMEPLAY_JUDGEMENT_SPEC                    superseded（顶部给出被替代条目清单）
标注  GAMEPLAY_JUDGEMENT_ABI                                historical-only
订正  docs/CURRENT_STATUS.md                               四项交接位置引用（D-1）
```

### 2.2 合同工件（Schema / Replay / diagnostics）现状

计划 §S7A-0 的退出门禁要求 "V2 ADR/Spec/ABI/**Schema**/Replay/diagnostics 状态已明确"。现状如下：

| 工件 | 现状 | 接受后需要 |
| --- | --- | --- |
| `schemas/cuexis.chart.v1..v4.schema.json` | 存在 | 不需改（保留 v1-v4） |
| Chart v5 / `gameplay` 区段 Schema | **不存在** | 新建 `schemas/cuexis.chart.v5.schema.json`，包含 `gameplay.version = 2` 的 requirements / coordination / solverProfiles / factBindings / capabilities |
| `schemas/cuexis.animation-template.v2.schema.json` | 存在（CXT v2） | 删除或降级 Requirement 内 `effects` |
| `schemas/cuexis.chart-entry.v1.schema.json` | 存在 | 扩展 `entryKind`（`packed-chart` / `gameplay-graph` / `author-source`）与 manifest closure 字段 |
| `schemas/cuexis.cxc.v1.schema.json` | 存在 | 扩展 CXC manifest 的 entry kind、compiled semantic identity、Ruleset binding、capability closure、source-of |
| Ruleset package Schema | **不存在**（`cuexis.ruleset` 仍是文本合同） | 若 7A 不含 package（Q-17），推迟到 S7C-2 |
| Replay Schema | **不存在**（Replay 合同是候选文本） | 新建 Replay header + 事件流 Schema（或等价 typed 合同） |
| Snapshot Schema | **不存在** | 新建 Snapshot header + 全量语义状态 Schema |
| diagnostics 码表 | 散落在 [ABI §4](../../api/GAMEPLAY_JUDGEMENT_ABI.md) 与 [Preliminary Design §11](../research/gameplay-v2/PRELIMINARY_DESIGN.md) 的文本中 | 汇总为一张机器可读的 code/category/severity 表（CM-D04） |
| Schema 校验入口 | `tools/check_stage6_a2.py`、`cmake/VerifyPlayerDistribution.cmake` | V2 需要新增或复用校验入口，并注册为 CTest |

**结论**：V2 的 Schema、Replay、Snapshot 与 diagnostics 工件目前**尚不存在**；本 package 只能把它们
的状态登记为"待创建"，不能在 S7A-0 内声称已闭合。这也是 Q-13、Q-19 与 CM-D04 必须裁定的原因。

### 2.3 接受**不**解锁的内容

- 不授权修改 `engine/`、CMake、公共安装头或 SDK API 版本；
- 不授权开 PR、合并或发布；
- 不构成 Stage 8 发行矩阵的选入；
- 不把研究 spike 的数值变成生产限额；
- 不解除四项 Stage 6 交接的关闭前置（§2 的 A7 只是目标确认）。

## 3. Gameplay I 合同处置映射

| 文档 | 当前状态 | 目标处置 | 保留部分 | 被替代部分 |
| --- | --- | --- | --- | --- |
| [ADR 0043](../../adr/0043-gameplay-judgement-ruleset-convergence.md) | `proposed`（工作稿） | `superseded`（部分） | §1 四层边界、§5 整数域与量程证明、§7 预算分层原则、威胁模型 | §2 grace 语义（拆为 7A `preparedGrace` 与后续 `continuityGrace`）、§3 核心表达模型的 `Effect`、§4 Fact 排序 `(requirementId, phase, outcome)` |
| [Gameplay Judgement Spec](../../formats/GAMEPLAY_JUDGEMENT_SPEC.md) | `candidate`（工作稿） | `superseded`（字段与运行语义） | §1 范围与层次、§8 数值与几何、§11 能力与拒绝、§12 明确排除 | §2 连续量重采样（7A 关闭）、§3 Requirement 字段与 Pattern 列表、§4 grace、§5 求值顺序、§6 资源归属、§7 Fold、§9 identity 内容、§10 快照字段 |
| [Gameplay Judgement ABI](../../api/GAMEPLAY_JUDGEMENT_ABI.md) | `candidate` | `historical-only`（retained） | §1 生命周期形态、§3 会话操作、§4 诊断方向、§6 非目标 | §2 候选类型（V2 需要 Candidate/Commit/FactRecord/causalOrigin/measure/evidence） |
| [FORMAT_BOUNDARY.md](../research/gameplay-v2/FORMAT_BOUNDARY.md) | `candidate research` | `revise` | §7 Requirement identity、§8 四类闭包、§13 验证门 | §5.1/§6 的 `gameplay.version = 1`（与 PD/AM 冲突，D-4） |
| [PACKED_CHART_FORMAT.md](../../formats/PACKED_CHART_FORMAT.md) | `candidate`，§3.3 已冻结 | `retain` + 增补 | 全文（尤其 §3.3 冻结预算表） | 无；V2 只新增 candidate wire revision 说明 |
| [CXC_FORMAT.md](../../formats/CXC_FORMAT.md) | `candidate` | `revise` | ZIP32 Stored 载体形状、closure 校验方向 | `playback=true` 必须指向 Packed Chart 的单一约定 |
| [CXT_V2_FORMAT.md](../../formats/CXT_V2_FORMAT.md) | `candidate` | `revise` | 模板、Prototype/Instance、Slot/Binding、有限展开 | Requirement 内 `effects`、CXT 作为 Gameplay 程序来源的隐式约定 |

映射的执行时点是 **owner 接受本 package 之后**，且必须一次性完成，避免出现"两套字段定义同时有效"
的窗口（[Gameplay V2 研究索引](../research/gameplay-v2/README.md) 已明确禁止实施者从两套合同中自行选择）。

## 4. 结论摘要

### 4.1 支持

Stage 7A 的支持面是：离散输入规范化与映射、`TimebaseProfile`、两种迟到策略、
Tap/Hold head-body/显式 Release-tail、单一 `capacity = 1` exclusive resource、
`greedy_v1` coordination、append-only Fact Ledger、typed Ruleset 事务、Score/Combo/Statistics、
全量 Snapshot/Seek/Replay、以及基于 FactBinding 的 early/exact/late/Miss 表现桥接；
输入侧接受 Chart v5 source、Packed Chart 与 CXC 的 `packed-chart` / `gameplay-graph` 两种 entry。

### 4.2 拒绝

19 条明确拒绝（含 `capacity > 1`、Chord、temporal/quota、handoff、连续轨迹、方向、全局最优 solver、
correction、`reopen_uncommitted_window`、`sameContact`、几何推断、运行中修改映射、H01-H05/H07、
H06/H08、未知必需 section 被解释成 tap、旧 `gameplay.version = 1`、非空 CXT effects、迁移歧义）。

### 4.3 未决

15 条 P0 + 4 条追加未决 + 15 条 P1 + 10 条 P2 + 14 条缺陷。
**在 P0 全部裁定之前，S7A-1 的公共头与实现批次不得启动。**

**第 2 轮（时间域与迟到策略）已于 2026-10-03 裁定**：Q-04、CM-T03、CM-T04、CM-T08 / P1-04、P2-03 与新增
`CM-T13` 共 6 行全部接受，缺陷 F-04 一并闭合（provenance：thread `s7a1-freeze-bindings`，verdict `adopt`，
confidence 0.99；补充确认卡 thread `cm-x01-disposition`，verdict `adopt`，confidence 0.99，确认 `CM-X01`
保持 `open`、不改计数）；因此 `open` 合同项由 21 降为 **18**（`CM-T03`、`CM-T04`、`CM-T08` 转为 `accept`）。
该轮的 7 条裁定**全部登记为阻塞 S7A-2 的门禁**；**具体 late-policy 数值、具体业务量程限额、S7C-1 校准
扩展与连续输入能力**登记为**后续批次阻塞项**（不是 S7A-2 门禁）。带日期证据见
[第 2 轮门禁报告](../../stage_reports/stages/stage-07/2026-10-03-s7a-2-gate-rulings.md)。
**第 2 轮语义在实现复核后被修订（2026-10-03，同 thread 两张续裁卡，verdict `adopt`，confidence 0.98 /
0.99）**：`stop` 模型由"比例替换"修订为**塌缩 / 跳变 + 精确累计函数**（`tick(beat) =
roundHalfToEven(F(beat))`，只舍入一次，区间内非单射，半开方向正向 `(originBeat, beat]` / 负向
`(beat, originBeat]`，有 `stop` 时不再关于 origin 奇对称），并补三分法（Spec §9.3）、迟到参数量级门禁
（**阻塞 S7A-4 的 late-window / 判定消费门禁**，数值与限额仍记 **S7A-9**）与 Tick → Beat 反查阻塞项
（首次消费该反查的后续批次；`S7A-3` / `S7A-4` / `S7A-5` 均不消费它）；**`open` 计数不变，仍 14**，六类
处置词与总数 114 不变。带日期证据见
[第 2 轮实现裁定记录](../../stage_reports/stages/stage-07/2026-10-03-s7a-2-implementation-rulings.md)。

**第 3 轮（prepare、装配与 entry；进入 S7A-3 前）已于 2026-10-03 裁定**：Q-05、Q-07、Q-18、CM-C10 / P1-03
共 6 行（含 `P2-10` 的重述行）落地，`CM-X05` 由 D-3 落地关闭（处置词 `open` → `revise`，**不是** `accept`），
`CM-C10` 处置词 `open` → `accept`；因此 `open` 合同项由 18 降为 **16**、`accept` 35 → **36**、`revise`
25 → **26**，总数仍 **114**（provenance：thread `s7a3-prepare-rulings`，三卡 verdict `need_info` 0.8 /
`reject` 0.88 / 计数确认卡 `adopt` 0.99，2026-10-03）。`Q-05`、`Q-07`、`Q-18` / `P2-10`、`CM-C10` / `P1-03`
与 **identity 规范字节边界**（规范字节是 prepare 内部确定性派生物、不是公共 ABI 或 Packed 编码；只冻结
语义等价 / 排序 / 身份域边界，编码实现须在 S7A-3 消费前闭合）**全部登记为阻塞 S7A-3 的门禁**；
`boundedRelationInstance` 候选表示与 [SUPPORT §6.1](SUPPORT_AND_REJECTION_MATRIX.md) 的 `W-01…W-05` 候选项
只阻塞**首次拟消费它**的后续批次。`CM-X05` 在本批次门禁清单中标为"D-3 已关闭；仅具体 W 缺口在首次消费前
须完成登记"。带日期证据见
[第 3 轮门禁报告](../../stage_reports/stages/stage-07/2026-10-03-s7a-3-gate-rulings.md)。

**第 4 轮（仲裁、资源与事实序；进入 S7A-4 前）已于 2026-10-03 裁定**：Q-03、Q-11、Q-16、CM-C05、CM-C09
与 P1-12 共 6 行落地（`CM-C05` 随 `Q-16`、`CM-C09` 随 `Q-11` 一并关闭），`CM-C05`、`CM-C09` 处置词
`open` → **`accept`**；因此 `open` 合同项由 16 降为 **14**、`accept` 36 → **38**，`revise` 26 /
`supersede` 17 / `retain` 17 / `reject` 2 不变，总数仍 **114**（provenance：thread
`s7a4-arbitration-rulings`，主裁定卡 `adopt` 0.97 / 处置词与计数确认卡 `adopt` 0.99，2026-10-03）。
本轮冻结的事实是：**六步＝外层、八阶段＝第 4 步内部完整展开**的**唯一**映射（阶段顺序与语义进
`judgement identity` 的 engine 组件、不进 chart/content identity；S7A-4 只冻结阶段名称、顺序、映射与
语义边界）、**7A 资源子集 `free` / `held` / `terminal`**（`gap` / `handoff_pending` / 非零 grace /
handoff / `capacity > 1` / owner 集合 / 并列 slot 稳定拒绝）、**资源身份规则**（`resourceId` 只在
prepared canonical graph 命名空间内、唯一 slot、slot/lease/最小 contact handle/claim identity 进
canonical graph、`observe` 不占用资源）、**solver / coordinator 边界**（runtime coordinator 不解析也不
编译 solver，名称采用 `coordinator.policy.greedy_v1`）、`SolverProfile` 的**字段语义**与 prepare 拒绝语义，
以及 **P1-12 的早 / 晚判定**（Exact 由窗口半宽定义、窗口外早击不产生 Fact 但产生诊断、Miss / absence 由
deadline 产生 Fact、Hold head/body/tail error 不合并）。该轮的 6 行裁定与 `CM-C06` / `CM-C07` 的
**语义闭合**全部登记为**阻塞 S7A-4 的门禁**；**具体预算数值、默认 profile 清单、proof 编码、
wire / serialization、`terminal` 编码与后续 `gap` / handoff / `capacity > 1` 语义**登记为**首次消费它们的
后续批次**阻塞项（数值与默认列表记 S7A-9）。`CM-C04`、`CM-C06`、`CM-C07` 的处置词**不变**（`revise` /
`revise` / `accept`），只更新未决点文字。带日期证据见
[第 4 轮门禁报告](../../stage_reports/stages/stage-07/2026-10-03-s7a-4-gate-rulings.md)。

**第 5 轮（Ruleset、事实序、Score 与 Snapshot；进入 S7A-5 / S7A-6 前）已于 2026-10-03 裁定**：Q-08、CM-S02、
Q-17、CM-S08、Q-12、Q-13、CM-K01、CM-K07、CM-K08、CM-F06、CM-S10 / P1-07、CM-S11 与 P1-14 共 13 行落地
（编号 `S7A5-R01…R11`；`CM-S08` 随 `Q-17` 一并关闭），`CM-F06`、`CM-S08`、`CM-S10`、`CM-K07`
四条处置词 `open` → **`accept`**；因此 `open` 合同项由 14 降为 **10**、`accept` 38 → **42**、`revise` 26 /
`supersede` 17 / `retain` 17 / `reject` 2 不变，总数仍 **114**（provenance：thread
`01a0fe49-cfe5-7933-8d03-3d0a65d34c8b`，主裁定卡 `adopt` 0.95 / 处置词与计数确认卡 `adopt` 0.99 / ABI 状态格
首词与追踪计数追问卡 `adopt` 0.98，2026-10-03）。本轮冻结的事实是：**唯一规范总序
`(commitTick, originKindPriority, canonicalOrdinal)`**（`canonicalOrdinal` 由引擎从 `(originScope,
originOrdinal, localOrdinal, phasePriority, factKindPriority)` 派生，**禁用 `ingressSequence` / 容器顺序 /
线程完成顺序**，Fact 的 `tick` 即 `commitTick`，`observationTick` 不是总序首键；首次消费 **S7A-4**，Spec §3.9
第 3 条与 plan §S7A-4 第 5 步随之改写）、**Ruleset Tick 三阶段与 `faulted` 行为矩阵**（失败时**不追加 fault
Fact**、**不发布** RuleEffect / PresentationEvent、**保留旧 state**；`faulted` 的 `submit` / `advance` /
`seek` / `replay` / 就地 `reload` 与 **`snapshot`** 稳定失败；只有显式 reset 或创建替换新 session 的
reload / recovery 可离开，且不得恢复 / 伪造未提交 StateDelta）、**`RegisterKind` 三类的命名与冲突策略**
（`exclusive` / `commutative_monoid` / `ledger_derived`；7A 只接受三类）、**7A 只用内置 Ruleset**（package
命中既有 R-09，形状归 S7C-2）、**Fact 身份生成语义与 `FactSemanticRevision` 归属**（物理字节留 S7A-6）、
**Snapshot / Replay 两套 header 字段集与 `SnapshotPayload` 闭包**（含不得保存清单；拼写规范类型
`EventCodecId` / 字段 `eventCodecId`）、**`SeekLatencyCommitment` 的类型、承诺语义与会话只能收紧**（数值禁止
进入 Schema / header / 运行时约束 / 对外承诺）、**Outcome `{hit, miss}` / `FactCategory` 与 phase 一一对应 /
grade 可选且缺失即 absent / `TimingError` 为有符号整数 tick 差 / 统计 reset·seek·replay 规则**、**Life 在
7A 关闭**（`capability.disabled` 稳定拒绝）与 **`P1-14` 的 REF0 / manifest / 诊断归属表**（作为 Spec 附录
§3.8.6 冻结；首次消费 S7A-3、集成 S7A-7）。该轮的 13 行裁定全部登记为**阻塞 S7A-5 与 / 或 S7A-6 的门禁**
（`P1-14` 除外：它**不阻塞** S7A-5 / S7A-6）；**两套 header 的字节布局与 codec 编码、`FactId` / `CommitId`
物理编码、全部预算数值与 `maxSeekLatency` 数值、package 形状**登记为**首次序列化消费者 / S7A-9 / S7C-2**
的后续阻塞项（`RULING_WORKSHEET §8` 的 `open` 由 14 降为 **10**，`§0.1` 合计行的末列仍为 **21**）。`CM-S02`、
`CM-K01` 的处置词**不变**（均仍为 `revise`），只更新未决点文字；`CM-S11`、`CM-K08` **不是** CONTRACT_MATRIX
的行、不改变任何计数。带日期证据见
[第 5 轮门禁报告](../../stage_reports/stages/stage-07/2026-10-03-s7a-5-6-gate-rulings.md)。

**第 6 轮（发布粒度、表现桥接与诊断）已于 2026-10-03 裁定**：Q-06、Q-19 + `CM-X01`、Q-09 + `CM-P04`、
Q-15 + `CM-P06` / `P1-13`、`CM-P08` / `P1-11`、`CM-D04` / `P1-09`、`P1-08`、`P2-04`、`P2-05`、`P2-06`、
`P2-08`、`P2-09` 共 **15 行 / 17 项**全部接受，编号 **`S7A6-R01…R12`**（provenance：Codex 话题 thread
`01a0fe61-3476-7882-9674-5f6b05035237`，consult，单卡 verdict `adopt`、confidence **0.97**；卡文件名时间戳
2026-10-02T20:51:47.167Z（UTC），按本地日期 2026-10-03 登记）。该轮**每条的首次消费批次一律 S7A-7**，
`P2-06` 的资源三分法沿用 S7A-4 已冻结的 `free` / `held` / `terminal` 子集；**本轮不涉及 `P2-03`**。
`CM-P04`、`CM-P06`、`CM-P08`、`CM-D04`、`CM-X01` 五条合同项处置词 `open` → **`accept`**，因此
`RULING_WORKSHEET §8` 的 `open` 由 10 降为 **5**、`CONTRACT_MATRIX` **当时**变为 **`accept` 47 / `open` 5**
（总数仍 **114**），`§0.1` 合计行仍 **76 行 / 84 项 / 末列 21**、§7.2 仍 **19 条**。**`CM-V06`、`CM-V07`、
`CM-V13`、`CM-I06`、`CM-X06` 五条 `open` 本轮不改处置词**（其自相矛盾由第 7 轮单独裁定）。
**本轮只冻结文档合同与门禁，不表示 Judgement 实现完成或 Stage 7A 完成。** 带日期证据见
[第 6 轮门禁报告](../../stage_reports/stages/stage-07/2026-10-03-s7a-6-gate-rulings.md)。

**第 7 轮（收尾澄清与缺陷）已于 2026-10-03 裁定**：`P1-01`、`P1-02`、`P1-05`、`P1-06`、`P1-10`、`P1-15`、
`P2-02`、`P2-07` 与缺陷 `D-2 / D-7`、`D-5`、`D-6`、`D-9`、`D-11`、`D-12`、`D-13`、`D-14` 共
**16 行 / 18 项**全部接受，编号 **`S7A7-R01…R16`**（provenance：主卡 Codex 话题 thread
`01a0fe61-b7a3-7853-a380-0793fee14e14`，consult，模型 `gpt-6-astra`，verdict `adopt`、confidence **0.96**；
口径确认卡 thread `01a0fe61-3476-7882-9674-5f6b05035237`，verdict `adopt`、confidence **0.99**；卡文件名
时间戳 2026-10-02T20:52:37.188Z 与 2026-10-02T21:08:34.383Z（UTC），按本地日期 2026-10-03 登记）。
该轮**每条的首次消费批次**：`P1-01` / `P1-02` / `P1-15` / `P2-07` → **S7A-3**（准入门禁）、`P2-02` →
**S7A-4**、`P1-10` → **S7A-9**、`P1-05` **阻塞 S7B-1**、`P1-06` **阻塞 S7C-2**、`D-9` → **S7A-8**
（**owner-only**）、其余为文档订正。`CM-V06`、`CM-V07`、`CM-V13`、`CM-I06`、`CM-X06` 五条残余 `open`
按**第 1 轮已完成的裁定**补齐处置词与裁定正文（`open` → **`accept`**，**不占 `S7A7-R` 编号**），
因此 `RULING_WORKSHEET §8` 的 `open` 由 5 降为 **0**、`CONTRACT_MATRIX` 变为
**`accept` 52 / `revise` 26 / `supersede` 17 / `retain` 17 / `open` 0 / `reject` 2 = 114**，
**`open` 集合为空**；`§0.1` 合计行仍 **76 行 / 84 项 / 末列 21**（末列是历史归属列）、§7.2 仍 **19 条**、
§9.2 仍**九类**、ABI 仍 **9 域 / 126 = 96 + 30**。**行 / 项口径订正**：第 7 轮实为 **16 行 / 18 项**
（第 6 轮门禁报告中的"76 行 / 83 项"是误推，该带日期报告正文不改写），逐轮合计 76 行 / 84 项自洽。
**`D-9` 为 owner-only**（候选补丁未提交，须 owner 按 ADR 0042 具名复核后落 `master`）、
**`D-12` 待 owner 确认**（本轮只登记、不编辑研究稿的 8 处指向行）。**第 7 轮之后无待裁定轮次。**
**本轮只冻结文档合同与门禁，不表示 Judgement 实现完成或 Stage 7A 完成**（实现批次 S7A-3…S7A-9 仍未完成）。
带日期证据见
[第 7 轮收尾裁定与缺陷处置记录](../../stage_reports/stages/stage-07/2026-10-03-s7a-7-wrapup-rulings.md)。

除 7 轮清单本身外，还有一类未决点不在 Q/P/D 编号体系内：ABI 的 30 条 `待冻结` 中有 16 条
（13 条无任何裁决编号、3 条的编号未绑轮次）缺可追溯的阻塞批次。[FREEZE_BLOCKING_BINDINGS.md](FREEZE_BLOCKING_BINDINGS.md)
与 [RULING_WORKSHEET.md](RULING_WORKSHEET.md) §7B 已登记该缺陷的**最终绑定**：16 条全部具备"裁决编号 +
首次消费批次"（本次登记新增 `CM-T13`、`CM-S11`、`CM-K08` 三条裁决项，并把 `CM-S02`、`CM-K01` 补登记到
第 5 轮表行）。该绑定由 Codex 于 2026-10-03 裁决（thread `s7a1-freeze-bindings`，verdict `adopt`，
confidence 0.96），ABI `§登记缺陷` 随之关闭；缺陷 D-14 按方案 (a) 处置（`CM-X01` 记入第 6 轮、
首次消费 S7A-7；合同台账的 `accept` **曾**被修正为 `open`，第 7 轮已按第 6 轮 `S7A6-R02` 重新闭合为
`accept`，见本文件 §4 的第 7 轮段与 [第 7 轮报告](../../stage_reports/stages/stage-07/2026-10-03-s7a-7-wrapup-rulings.md)）。

## 5. 事实与证据边界

| 类别 | 处理方式 |
| --- | --- |
| 研究切片数值（96 / 672 / 3 / 2 / 4,777 / 1,289 / 1,024 / 4,096 / 50 ms / 65,536 等） | **不是**生产限额；只作量级参考，登记在预算计划 §8 |
| 已冻结的生产预算 | 只有 [Packed 候选物理合同 §3.3](../../formats/PACKED_CHART_FORMAT.md) 的 7 项 |
| 本地已验证 | 只限基线报告里实际运行的命令与结果 |
| hosted 未验证 | 本 package 与基线报告都没有运行 hosted；不得声称通过 |
| GPU / 真实设备 / 音频 / 网络输入 | 未执行；单列为环境限制 |
| 历史报告 SHA | 不作为本次证据（计划 §S7A-0.1 明确禁止冒充） |

## 6. 相关索引

- [Gameplay V2 redesign research](../research/gameplay-v2/README.md)：V2 研究稿全集
- [Stage 7A 实施计划](../../stage_plans/active/stage-07/plan.md)：批次、依赖与关闭标准
- [音乐游戏玩法抽象模型](../../architecture/GAMEPLAY_ABSTRACTION_MODEL.md)：共同语义基础
- [S7A-0 基线报告](../../stage_reports/stages/stage-07/2026-10-02-s7a-0-baseline.md)：执行基线证据
- [Packed 候选物理合同](../../formats/PACKED_CHART_FORMAT.md)：已冻结预算表
- [版本规范](../../guides/VERSIONING.md)：SDK API 版本门禁与更新流程

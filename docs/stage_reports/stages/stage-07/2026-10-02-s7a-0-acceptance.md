# Gameplay V2 acceptance package：owner 接受记录

状态：completed（准入接受；V2 文档冻结为后续独立工作项）

快照日期：2026-10-02

后续关闭证据：本记录 §4 的冻结顺序执行证据、§5 的逐轮裁定登记
（[RULING_WORKSHEET.md](../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) §9）

上级文档：[Stage 7A 实施计划](../../../stage_plans/active/stage-07/plan.md)（§1.1 冻结顺序、§S7A-0 退出门禁）·
[S7A-0 执行基线与合同表征](2026-10-02-s7a-0-baseline.md)（同一批次的执行基线证据）·
[Gameplay V2 acceptance package](../../../proposals/gameplay-v2-acceptance/README.md)

## 1. 被接受的对象

owner 于 2026-10-02 接受 [Gameplay V2 acceptance package](../../../proposals/gameplay-v2-acceptance/README.md)
整包，即：

| 包内文档 | 接受含义 |
| --- | --- |
| [CONTRACT_MATRIX.md](../../../proposals/gameplay-v2-acceptance/CONTRACT_MATRIX.md) | 114 项合同台账的**结构与处置词**被接受为实施输入的索引；其中 20 项 `open` 的语义仍待逐轮裁定 |
| [SUPPORT_AND_REJECTION_MATRIX.md](../../../proposals/gameplay-v2-acceptance/SUPPORT_AND_REJECTION_MATRIX.md) | 7A 支持集合、19 条明确拒绝、H 类政策与 W 类缺口字段定义被接受 |
| [FORMAT_ENTRY_AND_IDENTITY.md](../../../proposals/gameplay-v2-acceptance/FORMAT_ENTRY_AND_IDENTITY.md) | 版本层级接受/拒绝矩阵、8 类载体与 entry、四类身份与四分量被接受为冻结目标 |
| [TARGETS_AND_DEPENDENCIES.md](../../../proposals/gameplay-v2-acceptance/TARGETS_AND_DEPENDENCIES.md) | 依赖与安装图、新增 target 草案、allowlist 草案被接受为 S7A-1 的待落地设计（不构成已实施） |
| [BUDGET_AND_EVIDENCE_PLAN.md](../../../proposals/gameplay-v2-acceptance/BUDGET_AND_EVIDENCE_PLAN.md) | 五类预算 profile 与"只登记上界、不冻结限额"的处置被接受 |
| [BATCH_GATES.md](../../../proposals/gameplay-v2-acceptance/BATCH_GATES.md) | 每批次的正例/负例/诊断/golden/命令/证据/停止条件被接受为验收口径 |
| [STAGE6_HANDOVER_LEDGER.md](../../../proposals/gameplay-v2-acceptance/STAGE6_HANDOVER_LEDGER.md) | 四项 Stage 6 交接的台账、阻塞点与最小关闭证据被接受 |
| [OPEN_QUESTIONS.md](../../../proposals/gameplay-v2-acceptance/OPEN_QUESTIONS.md) ·
  [RULING_WORKSHEET.md](../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) | 53 项未决问题的**登记方式**与 7 轮裁定协议被接受；逐条结论仍待裁定 |
| [2026-10-02-s7a-0-baseline.md](2026-10-02-s7a-0-baseline.md) | Debug 基线证据被接受为本次实施基线（Release 与 hosted 见 §5） |

## 2. 接受清单逐项状态（对应包内 README §2 的 A1-A10）

| # | 确认项 | 本次状态 | 依据 |
| --- | --- | --- | --- |
| A1 | 版本层级固定为 `version = 5` + `gameplay.version = 2`，不引入 `semanticRevision` | **接受**（建议），待裁定登记 | [RULING_WORKSHEET.md](../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) 第 1 轮 Q-01 |
| A2 | Canonical Gameplay Graph 的物理归属 | **接受**（建议），待裁定登记 | 同上，Q-02 与 CM-V06 |
| A3 | 7A 只交付支持集合内的能力，其余按 19 条拒绝清单稳定拒绝 | **已接受** | 包内 SUPPORT_AND_REJECTION_MATRIX §2 |
| A4 | 15 条 P0 与 4 条追加未决逐条裁定 | **进行中**（7 轮协议已生效） | RULING_WORKSHEET §0.1 与 §9 |
| A5 | 9 条新发现缺陷的处置方式 | **部分已接受**：D-1 已订正；D-3/D-4/D-8 在第 1 轮；D-2/D-5/D-6/D-7/D-9 在第 7 轮 | RULING_WORKSHEET 第 1、7 轮 |
| A6 | 旧 Gameplay I 三份合同的处置映射 | **已接受**（映射表本身） | 包内 README §3；执行时点在 §4 |
| A7 | 四项 Stage 6 交接收口的本阶段目标与例外 | **已接受**（目标确认，不解除关闭前置） | 包内 STAGE6_HANDOVER_LEDGER §6 |
| A8 | Gameplay 运行时预算在 S7A-9 前保持未冻结 | **已接受** | 包内 BUDGET_AND_EVIDENCE_PLAN §10 |
| A9 | 接受后按 README §2.1 执行文档编辑并建立独立 V2 ADR/Spec/typed ABI | **已接受，执行中** | 本记录 §4 |
| A10 | 认可 V2 的 Schema / Replay / Snapshot / diagnostics 工件尚未创建，其创建属接受后的独立工作项 | **已接受** | 包内 README §2.2 |

## 3. 接受**不**解锁的内容（重申，避免误读）

- 不授权修改 `engine/`、CMake、公共安装头或 SDK API 版本；
- 不授权开 PR、合并或发布；
- 不构成 Stage 8 发行矩阵的选入；
- 不把研究 spike 的数值变成生产限额；
- 不解除四项 Stage 6 交接的关闭前置；
- **不解除"P0 未裁定前不得进入 S7A-1 公共头与实现批次"**（计划 §1.1；包内 README §4.3）。

## 4. 本接受解锁的文档动作（顺序固定）

计划 §1.1 的冻结顺序在本接受之后展开：

```text
1. owner acceptance                        <- 本记录
2. V2 ADR / Spec / ABI / Schema / Replay / diagnostics 冻结
3. Gameplay I 的 Spec / ABI / ADR 标记 superseded / retained / historical-only
4. Stage 7A typed contract review
5. 实现批次与 golden
```

执行时点与依据：

| 步 | 目标文档 | 来源 | 前置 |
| --- | --- | --- | --- |
| 2a | 新建 `docs/adr/0044-*.md`（V2 决策与威胁模型） | 包内 README §2.1 | 第 1 轮 Q-01/Q-02/Q-10/Q-14 裁定 |
| 2b | 新建 `docs/formats/GAMEPLAY_V2_SPEC.md`（字段与运行语义，含 W 类缺口附录） | 同上 | 第 1-5 轮裁定 |
| 2c | 新建 `docs/api/GAMEPLAY_V2_ABI.md`（typed preview 边界） | 同上 | 第 2-6 轮裁定 |
| 2d | 修订 `CHART_V5_GAMEPLAY_AMENDMENT.md` / `CXT_V2_FORMAT.md` / `CXC_FORMAT.md` / `PACKED_CHART_FORMAT.md` | 同上 | 第 3、6 轮裁定 |
| 2e | Schema / Replay / Snapshot / diagnostics 工件 | 包内 README §2.2 | 独立工作项，不属 S7A-0 |
| 3 | Gameplay I 三份合同的顶部标注 | 包内 README §3 | **必须晚于 2a/2b/2c**：政策第 50 行要求 superseded 文档链接替代文件 |
| 4 | Stage 7A typed contract review | 计划 §1.1 | 2 与 3 完成 |

**状态词映射**（政策第 37 行的枚举为 `active / candidate / future / deferred / completed / historical / superseded / archived`）：
包内 README §3 使用的处置词 `superseded` 直接对应枚举值；`historical-only（retained）` 落到文档状态行时
写为 `historical`，并在正文列出 retained 部分——**`retained` 与 `historical-only` 不是状态枚举值**。

### 4.1 执行进度（2026-10-02 追记）

| 步 | 状态 | 落地证据 |
| --- | --- | --- |
| 2a | **已建立**（`candidate`，未实施） | [ADR 0044](../../../adr/0044-gameplay-v2-semantic-kernel.md)：顶层状态 `candidate`（与包内"冻结完成前保持 candidate"一致），另列 `决策状态：已接受` |
| 2b | **已建立**（`candidate`） | [Gameplay V2 Spec](../../../formats/GAMEPLAY_V2_SPEC.md)：字段与运行语义唯一权威；W 类缺口按 §1.3 指向 plan §5.3 |
| 2c | **已建立**（`candidate`） | [Gameplay V2 ABI](../../../api/GAMEPLAY_V2_ABI.md)：typed 内部 / preview 边界，9 域 126 条目（类型清单表格行计数；草案期曾口头统计为 144，已按 [typed contract review](2026-10-02-s7a-1-typed-contract-review.md) 的核对订正） |
| 2d | 未开始 | 需第 3、6 轮裁定 |
| 2e | 未开始 | 独立工作项（Schema / Replay / Snapshot / diagnostics） |
| 3 | **已完成** | ADR 0043、Gameplay Judgement Spec、Gameplay Judgement ABI 顶部均标 `superseded` 并链接替代文档；`docs/README.md`、`docs/adr/README.md`、`docs/formats/README.md`、`docs/api/README.md`、`docs/CURRENT_STATUS.md`、`docs/proposals/README.md`、`docs/guides/reading-order.md` 同步对齐 |
| 4 | 未开始 | 前置 2 与 3 已满足；S7A-1 的公共头与实现仍须等 typed contract review |

缺陷处置：D-3（W 类缺口定义落 plan §5.3，四处引用已指向 §5.3）与 D-4（研究稿 §5.1/§6 标版本取代、
正文保留）已落地；D-11（稳定 C ABI 阶段归属不一致）与 D-12（研究稿残留的 Gameplay I 权威指向）已登记。

校验与边界：`python -B tools/check_docs.py` 通过（328 个 Markdown、20 个候选 JSON/CXT）；
三份 V2 文档**均未**冻结、未实施，也**未**运行任何 C++ 构建或 CTest；全部改动留在工作区，未提交。

### 4.2 执行顺序偏差与纠正记录（2026-10-02 追记）

**偏差事实。** 冻结顺序第 2 步要求"先建立并（限定）冻结 V2 ADR / Spec / ABI，再标注 Gameplay I"。
实际执行中，第 3 步（把 ADR 0043、Gameplay Judgement Spec、Gameplay Judgement ABI 标为 `superseded`
并链接替代文档）在 §4.1 的 2a/2b/2c 之后、**限定冻结之前**就完成了。**不倒填**冻结时间，也不改写原
报告与历史标注证据。

**依据与处置。** 该偏差按 2026-10-02 owner 接受的 Codex 修订（第 1 轮补充 S1-02；(thread
`s7a1-admission-rulings`，verdict `reject`，confidence 0.94）处理：如实记录、不倒填，并在限定冻结后
**重新核验**替代关系。

**替代关系复核（S1-02）。** 逐份核对旧文档的状态词、替代链接与 retained 映射是否仍指向现行权威：

| 旧文档 | 状态词 | 替代链接 | retained 映射 | 复核结果 |
| --- | --- | --- | --- | --- |
| ADR 0043 | `superseded` | ADR 0044 / V2 Spec / V2 ABI | 部分保留（§1 四层边界、§5 整数域、§7 预算分层、威胁模型） | 链接有效；与 ABI 处置映射一致 |
| Gameplay Judgement Spec | `superseded as field contract` | V2 Spec | 保留为设计输入历史（§1/§8/§11/§12） | 链接有效；字段权威已指向 V2 Spec |
| Gameplay Judgement ABI | `superseded as typed preview input` | V2 ABI | 保留为早期草案与命名来源（§1/§3/§4/§6） | 链接有效；ABI"与 Gameplay I ABI 的关系"已同步 |

**typed review 关闭复核。** 按 S1-02 的第三项动作，关闭复核记入
[S7A-1 typed contract review](2026-10-02-s7a-1-typed-contract-review.md) §10（其中 F-04 保持未闭合，
留待第 2 轮输入域合同）。

## 5. 证据边界

| 项 | 状态 |
| --- | --- |
| Debug 基线（`ctest --preset debug --no-tests=error`） | 已实跑并记录于 [基线报告](2026-10-02-s7a-0-baseline.md)；同批次修复后复跑为 749/749 |
| Release 基线 | **已实跑**：749/749 通过、exit 0、`Total Test time (real) = 525.90 sec`，记入 [Release 基线证据](2026-10-02-s7a-0-release-baseline.md)（未追写进 Debug 基线报告） |
| hosted（Linux Quality / Windows MSVC / Windows MinGW / Version Gate） | **未运行**；不得据本地结果推断通过 |
| GPU / 真实设备 / 音频 / 网络输入 | 未执行，单列为环境限制 |
| 四项 Stage 6 交接的实现状态 | 见 STAGE6_HANDOVER_LEDGER；本接受只确认目标，不宣称完成 |

## 6. 相关索引

- [Gameplay V2 acceptance package](../../../proposals/gameplay-v2-acceptance/README.md)
- [未决语义分轮裁决清单](../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md)
- [S7A-0 执行基线与合同表征](2026-10-02-s7a-0-baseline.md)
- [Stage 7A 实施计划](../../../stage_plans/active/stage-07/plan.md)
- [文档整理政策](../../../DOCUMENTATION_POLICY.md)

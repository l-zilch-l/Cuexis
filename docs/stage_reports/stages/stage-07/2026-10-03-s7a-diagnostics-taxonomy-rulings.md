# 第 6 轮 `CM-D04` / `P1-09` 首次消费的诊断分类学后续补齐记录

状态：dated implementation record（第 6 轮诊断码表分类学冲突裁定的文档与集中码表落地证据；
不是阶段关闭、不是 owner acceptance、不是实施证据、**不是** Judgement 实现完成声明、
**也不是** Stage 7A 完成声明）

更新日期：2026-10-03

上级文档：[Stage 7A 实施计划](../../../stage_plans/active/stage-07/plan.md) §1.1 / §S7A-3 / §S7A-5 / §S7A-7 ·
[Gameplay V2 acceptance package](../../../proposals/gameplay-v2-acceptance/README.md) ·
[未决语义分轮裁决清单](../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) §6 / §9 ·
[未决问题与 owner 决策请求](../../../proposals/gameplay-v2-acceptance/OPEN_QUESTIONS.md) §4.4 / §4.6 ·
[批次门禁](../../../proposals/gameplay-v2-acceptance/BATCH_GATES.md) §S7A-3 / §S7A-5 / §S7A-7

本文是**带日期的落地记录**（reports own dated evidence）：它登记 Codex 对**第 6 轮 `CM-D04` / `P1-09`
（`S7A6-R06`）首次消费的后续补齐**裁定（决策卡 `2026-10-02T21:03:39.462Z`，thread
`01a0fe6b-6e57-7051-87f8-cf9588e85568`，verdict `adopt`、confidence **0.96**），以及随后的**处置确认**裁定
（决策卡 `2026-10-02T21:51:38.591Z`，question `diagnostics-code-table-disposition`，thread
`01a0fe97-f1b6-7332-b0c9-cafc3c9f25ec`，verdict `reject`（逐项处置）、confidence **0.7**，见 **§11**），
逐条落进 [GAMEPLAY_V2_SPEC.md](../../../formats/GAMEPLAY_V2_SPEC.md) /
[GAMEPLAY_V2_ABI.md](../../../api/GAMEPLAY_V2_ABI.md) /
[`schemas/cuexis.gameplay-diagnostics.v2.codes.json`](../../../../schemas/cuexis.gameplay-diagnostics.v2.codes.json)
的哪一行，并记录本轮跑过的门禁与原始输出。它不复制 Spec / ABI 的合同正文，也不构成任何实施声明；
未在本文列出的门禁一律视为未运行。**§11.2 是公共诊断码判据的独立裁定说明**（Spec §9.9 的带日期证据）；
**§11.3 C-4 是历史报告 `类别 capability` 的订正说明**。

> **表述纪律。** 本轮落地的是**文档与集中码表的分类学语义**。**不得**把本文写成"Judgement 实现完成"或
> "Stage 7A 完成"：实现批次 S7A-3…S7A-9 仍未完成，本轮的校验证据只是 **CMake 脚本模式**下的码表校验器
> 正例 / 负例与文档门禁，不是构建、CTest 全量、hosted、GPU、真实设备或音频证据。

## 1. provenance

| 卡 | thread | 模型 | verdict | confidence | 日期 | 内容 |
| --- | --- | --- | --- | --- | --- | --- |
| 第 6 轮 `CM-D04` / `P1-09` 首次消费的后续补齐卡 | `01a0fe6b-6e57-7051-87f8-cf9588e85568` | `gpt-6-astra`（consult） | `adopt` | **0.96** | 2026-10-03 | 采纳全部建议动作：九类为**唯一** category 权威、字面 `capability` 逐字替换为 `capability_disabled`、ABI 码族分组示例加"仅为示例"限定、八条已登记码 category 定案、新增并登记 `ruleset.transaction_failed`、`presentation-target-missing` / `partial-group` 原样登记、六个码的首次消费批次登记为 S7A-3、13 个 `judgement.s7a2.*` 令牌永久 src-only、校验器严格约束保留、`severity` / `faulted` 取值集以 core 既有面为准、计数一律不变 |

**日期口径。** 卡的文件名时间戳为 `2026-10-02T21:03:39.462Z`（UTC），按**本地日期 2026-10-03** 登记
（与第 2、3、4、5、6、7 轮先例一致）。

**轮次归属。** 本卡是**第 6 轮 `CM-D04` / `P1-09` 首次消费的后续补齐**，**不是新的裁定轮次**：
它的输入是第 6 轮码表落地后暴露的文档内部矛盾（`docs/decisions` 的标题即"诊断码表落地后的分类学冲突
（第 6 轮 `CM-D04`/`P1-09` 的首次消费）"）。因此本文与各 document 的登记都写成"第 6 轮 … 的首次消费
后续补齐"，**不新立"第 8 轮"编号**，也不改动 §0.1 的逐轮行 / 项合计（**76 行 / 84 项不变**）。

**与前一轮的关系。** 第 7 轮（`S7A7-R01…R16`）已于 2026-10-03 裁定并登记；本卡的裁定**不修改**第 7 轮的
任何结论，只补齐第 6 轮 `CM-D04` / `P1-09` 的码表分类学。

## 2. 逐条执行（10 项要求 → 落点）

| # | 本任务要求 | 裁定要求 | 落点 | 结果 |
| --- | --- | --- | --- | --- |
| 1 | 九类为唯一权威；`类别 capability` → `类别 capability_disabled` | 逐字替换全部命中 + ABI 码族分组示例加"仅为示例"限定 | Spec §3.7.7 表（L381）、§7.2（L1374）、§9.3（L1620）、§S7A-2 节第 5 项（L1879）；ABI §S7A-2 输入半批补充 ⑤ 行（L874）、§诊断分层代码块之后（L589-L592）、§码族来源（L651） | 完成：6 处逐字替换 + 2 处限定 / 防误用句 |
| 2 | 集中码表类别修订（8 条） | R-11 / R-12 / R-13 → `invalid_relation`；R-14 / R-15 → `capability_disabled`；`capability.revision_mismatch` → `unknown_capability`；R-17 → `ambiguous_migration`；`capability.budget_insufficient` → `budget_exceeded` | 集中码表 `codes[]` 8 条 + Spec §7.2 新增"九类 category 定案"表 | 完成 |
| 3 | 新增并登记 `ruleset.transaction_failed` | `invalid_relation` / `error` / `session_faulted` / 首次消费 S7A-5；Spec §9.4 明确"第二阶段 Ruleset state commit 失败统一使用此码" | 集中码表新条目；Spec §9.4 末段（L1638-L1652）；Spec §7.2 第 5 轮映射表行（L1423）；Spec §9.6 第 5 条表（L1713）；ABI §错误与诊断映射登记表（L607） | 完成（四处落点 + 码表 1 条） |
| 4 | 原样登记两个表现码 | `presentation-target-missing` / `partial-group`：`invalid_relation` / `error` / `session_unaffected` / 首次消费 S7A-7；写入 Spec §9.6 / §9.7、ABI 域 8 与错误映射、集中码表 | Spec §9.6 第 5 条表（L1712-L1713 与 L1715）、Spec §9.7 既有表（已一致）；ABI 域 8 `DiagnosticCategory` 行（已一致）、§错误与诊断映射登记表（L604-L606）；集中码表 2 条 | 完成（并按本条**订正**：Spec §9.6 第 4 条原缺 `severity` / `faulted` / 首次消费，现补齐） |
| 5 | 六个未声明批次的码登记为 S7A-3 | `capability.budget_insufficient`、`capability.revision_mismatch`、`capability.permanently_unsupported`、`capability.future_development`、`input.direction_unsupported`（R-06）、`format.gameplay_version_unsupported`（R-17） | 集中码表 `ownerBatch` 6 处（由 `null` 改为 `S7A-3`）；Spec §7.2 批次补齐段（L1361-L1364） | 完成 |
| 6 | 13 个 `judgement.s7a2.*` 令牌保持 src-only | 把状态从"待登记/待定"改为"**明确不登记（src-only）**" | 集中码表 `pendingRegistration` 13 条的 `"status": "not_registered_src_only"` + 顶层 `srcOnlyPolicyNote` | 完成（状态词 `not_registered_src_only`） |
| 7 | 保留校验器严格约束 | 九类恰好、`R-01…R-19` 恰好一次、输入 / 几何族仅三码；新增 category / R 编号 / 输入几何码必须显式改校验器 | **未改** `cmake/VerifyGameplayDiagnosticsCodes.cmake`；负例 4 证明约束仍生效（§5.2） | 完成（校验器零改动） |
| 8 | `severity` / `faulted` 值集自洽并被文档明示 | core `DiagnosticSeverity` 只有 `Info`/`Warning`/`Error`；faulted 区分 `session_unaffected` / `session_faulted` | Spec §9.1 新增取值集表（L1534-L1545）；ABI 域 8 `DiagnosticSeverity` 行（L374）与 §诊断分层 | 完成 |
| 9 | 计数一律不变 | CONTRACT_MATRIX 114（52/26/17/17/0/2）、Spec §7.2 19 条、九类、ABI 9 域 / 126 = 96 + 30、§未决项条目数 | 见 §6 复核 | 完成（未新增 ABI 类型条目或域） |
| 10 | 顺手做一次文档同步核对 | 把"待冻结 / 待汇总 / 待登记"的陈旧表述改为指向集中码表 + CTest 校验项 `cuexis_gameplay_diagnostics_codes` | Spec §9.5（L1655-L1671）、Spec §10 工件表（L1791）、Spec §11.1 第 3 条（L2277-L2279）、ABI §码表位置（L619-L627）、§仍待冻结（L685）、§S7A-7 后续批次阻塞项（L1086） | 完成（逐条见 §7） |

### 2.1 第 1 项：把码族分组词当作 `category` 的**全部**活体命中（逐处行号 + 改前 → 改后逐字）

扫描方法：对 `docs/` / `engine/` / `tests/` / `tools/` / `schemas/` / `cmake/` 全量文本扫描"把码族分组词写成
**类别取值**"的两种形态——① 六类 category 位置里带反引号的"类别 + 空格 + capability"；② 括号内把类别写成
裸 capability 的 `（capability / capabilityId / remediation）` 形态。本任务允许修改的两个文件中**全部命中为
6 处**（其中 5 处是字面替换、1 处是新增防误用句）；此外有 **3 处**同类字面落在**不允许修改的路径**内，
仅报告（见本节末）。

| # | 文件 | 行 | 形态 | 改前（逐字） | 改后（逐字） |
| --- | --- | --- | --- | --- | --- |
| 1 | `docs/formats/GAMEPLAY_V2_SPEC.md` | L381（§3.7.7 表行） | ``类别 `capability` `` | ``（类别 `capability`，`capabilityId = input.trajectory.v1`，`` | ``（类别 `capability_disabled`，`capabilityId = input.trajectory.v1`，`` |
| 2 | `docs/formats/GAMEPLAY_V2_SPEC.md` | L1374（§7.2 输入规范化段） | ``类别 `capability` `` | ``**R-05** 的 `input.continuous_unsupported`（类别 `capability`，`` | ``**R-05** 的 `input.continuous_unsupported`（类别 `capability_disabled`，`` |
| 3 | `docs/formats/GAMEPLAY_V2_SPEC.md` | L1620（§9.3 引用行） | ``类别 `capability` `` | `   `input.continuous_unsupported`（类别 `capability`，` | `   `input.continuous_unsupported`（类别 `capability_disabled`，` |
| 4 | `docs/formats/GAMEPLAY_V2_SPEC.md` | **L1879（§S7A-2 节第 5 项）** | 裸 `` `capability` `` 作 category（`（`capability` / `capabilityId` / `remediation`）` 形态） | ``5. 不连续表示与连续输入能力**都复用**既有 `input.continuous_unsupported`（`capability` /`` | ``5. 不连续表示与连续输入能力**都复用**既有 `input.continuous_unsupported`（`capability_disabled` /`` |
| 5 | `docs/api/GAMEPLAY_V2_ABI.md` | L874（§S7A-2 输入半批冻结补充 ⑤ 行；该行同时含"类别 capability"字面） | 六类 category 位置 | ``（类别 `capability`，`capabilityId = input.trajectory.v1`，`` | ``（类别 `capability_disabled`，`capabilityId = input.trajectory.v1`，`` |
| 6 | `docs/api/GAMEPLAY_V2_ABI.md` | L589–L592（§诊断分层，新增防误用句；原 L584 行**未改**） | 码族分组词示例 | （新增，无改前文本） | 见 §2.2 |

**明确说明：第 4 处与第 5 处是并行实现线报告的两处活体命中，本轮一并改完并逐字登记如上。**
第 4 处位于 §S7A-2 节第 5 项，同段第 6 项的计数声明（`§7.2 仍 **19 条**、§9.2 仍**九类**、契约处置词不变`）、
第 5 项的"**不新增 ABI 码、不新增 R 条目**（§3.7.7 表、§7.2）"与 `capabilityId` / `remediation` / `field.path` /
**R-05** 全部**逐字未动**，只改了括号里的类别值。

**未修改的 3 处（第 2 批按 C 授准已处置，见 §11.3）：** `docs/proposals/gameplay-v2-acceptance/CONTRACT_MATRIX.md`:146（`CM-T10` 行）、
`docs/proposals/gameplay-v2-acceptance/OPEN_QUESTIONS.md`:244、
`docs/stage_reports/stages/stage-07/2026-10-03-s7a-2-input-implementation-rulings.md`:43 当时仍写 ``类别 `capability` ``。
它们**不在第 1 批的允许修改路径**内（报告与提案登记只允许追加 provenance，历史报告正文不得改写），且三处都是
**带日期的历史记录 / 提案证据**。**第 2 批处置结果**：前两处已改为 `capability_disabled`；历史报告
**正文不改写**，由 **§11.3 C-4 的订正说明**替代（该说明可从本文 §9 与
[stage_reports/README.md](../../README.md) 的索引行到达）。

**同段内的 `类别 / capabilityId / remediation 同上`（Spec L382）不含该字面**，故未改；它引用的是同一行的
类别，替换后自动一致。

### 2.2 ABI §诊断分层：代码块**逐字未改**，只在其后追加防误用句

裁定要求"该段仅为码族分组示例，不是第二套 category 枚举"。执行时的两点判断：

1. **` ```text ` 代码块（原 L582–L587，现 L582–L587）逐字未改**，特别是 `category       分类枚举（identity / capability / budget / format / migration / lifecycle ...）` 这一行**保持原样**——它在第 6 轮冻结的 `S7A6-R06` 结构块内。
2. 在代码块**之后**追加一句防误用句，措辞明确到"分组词不是取值"（原文逐字见下），并附上并行实现线的两处实测反例：

```
上例括号内的词（`identity` / `capability` / `budget` / `format` / `migration` / `lifecycle` 等）**仅为码族
分组词**，**不是 `category` 的取值**；`category` 的合法取值只有 [Spec §9.2](../formats/GAMEPLAY_V2_SPEC.md)
的九类，**任何码族分组词都不得作为 `category` 写出**（第 6 轮 `CM-D04` / `P1-09` 首次消费后续补齐，
决策卡 2026-10-02T21:03:39.462Z，`adopt` 0.96；实测反例：共享 capability 拒绝路径曾发 `capability`、
会话生命周期顺序违规曾发 `lifecycle`，二者都必须走九类内的既有类别）。
```

同时在 §码族来源 段（章节名见下，行号只作带日期的证据指针）追加了裁定原文要求的限定句（现 L651）：

```
该段仅为码族分组示例，不是第二套 category 枚举；权威枚举为 SPEC §9.2 九类。
```

## 3. 码表 JSON 的变更摘要

文件：`schemas/cuexis.gameplay-diagnostics.v2.codes.json`（ASCII / LF / 无 BOM；顶层结构未重构，
新增一个顶层键 `rulingProvenance` 与一个 `srcOnlyPolicyNote`）。

### 3.1 新增已登记码（`codes[]`：21 → **29** 条）

| 码 | family | category | severity | faulted | 首次消费 | stableRejectCode |
| --- | --- | --- | --- | --- | --- | --- |
| `identity_closure_incomplete` | `judgement.prepare` | `identity_closure_incomplete` | `error` | `session_unaffected` | S7A-7 | `null`（不是 registry reject code） |
| `presentation-target-missing` | `presentation` | `invalid_relation` | `error` | `session_unaffected` | S7A-7 | `null` |
| `partial-group` | `aggregation` | `invalid_relation` | `error` | `session_unaffected` | S7A-7 | `null` |
| `ruleset.transaction_failed` | `ruleset` | `invalid_relation` | `error` | **`session_faulted`** | **S7A-5** | `null` |

说明：`identity_closure_incomplete` / `presentation-target-missing` / `partial-group` 三条此前只以
`pendingRegistration`（`code: null`）形式存在，本轮按裁定登记为正式码；`ruleset.transaction_failed`
是本轮**唯一真正新增的码串**，也是全表**唯一** `faulted = session_faulted` 的条目。`stableRejectCode = null`
的理由逐条写在条目的 `note` 里：它们都不是 `CapabilityRecord`（域 8）的 `stableRejectCode`，而
`CapabilityRecord.stableRejectCode` 是校验器第 ③ 项要求"已登记"的那组值。

### 3.2 类别修订（逐条）

| 码 | 改前 category | 改后 category | 改前 `categoryBasis` | 改后 `categoryBasis` |
| --- | --- | --- | --- | --- |
| `pattern.relation_unsupported`（R-11） | `invalid_relation` | `invalid_relation`（**确认**） | `derived_pending_owner_confirmation` | `owner_ruled` |
| `geometry.inference_rejected`（R-12） | `unknown_capability` | **`invalid_relation`（改正）** | `derived_pending_owner_confirmation` | `owner_ruled` |
| `session.mutation_rejected`（R-13） | `invalid_relation` | `invalid_relation`（**确认**） | `derived_pending_owner_confirmation` | `owner_ruled` |
| `capability.permanently_unsupported`（R-14） | `capability_disabled` | `capability_disabled`（**确认**） | `derived_pending_owner_confirmation` | `owner_ruled` |
| `capability.future_development`（R-15） | `capability_disabled` | `capability_disabled`（**确认**） | `derived_pending_owner_confirmation` | `owner_ruled` |
| `capability.revision_mismatch` | `unknown_capability` | `unknown_capability`（**确认**） | `derived_pending_owner_confirmation` | `owner_ruled` |
| `format.gameplay_version_unsupported`（R-17） | `ambiguous_migration` | `ambiguous_migration`（**确认**） | `derived_pending_owner_confirmation` | `owner_ruled` |
| `capability.budget_insufficient` | `budget_exceeded` | `budget_exceeded`（**确认**） | `derived_pending_owner_confirmation` | `owner_ruled` |

**只有 R-12（`geometry.inference_rejected`）的 category 值真的变了**：由推导的 `unknown_capability` 改为
裁定要求的 `invalid_relation`。其余七条是"推导 → owner 裁定确认"，因此只改 `categoryBasis` 与 `note`，
值不变——这正是第 2 项要求"逐条确认或改正"的落地方式。`derived_pending_owner_confirmation` 在本文件中
**已归零**（这正是第 6 项要消除的"待登记/待定"状态词在 `codes[]` 中的残留）。

### 3.3 批次登记（逐条）

| 码 | 改前 `ownerBatch` | 改后 `ownerBatch` |
| --- | --- | --- |
| `capability.budget_insufficient` | `null` | `S7A-3` |
| `capability.revision_mismatch` | `null` | `S7A-3` |
| `capability.permanently_unsupported` | `null` | `S7A-3` |
| `capability.future_development` | `null` | `S7A-3` |
| `input.direction_unsupported`（R-06） | `null` | `S7A-3` |
| `format.gameplay_version_unsupported`（R-17） | `null` | `S7A-3` |
| `ruleset.transaction_failed` | （新条目） | `S7A-5` |
| `presentation-target-missing` | （新条目） | `S7A-7` |
| `partial-group` | （新条目） | `S7A-7` |
| `identity_closure_incomplete` | （新条目） | `S7A-7` |

### 3.4 13 个 `judgement.s7a2.*` 令牌的状态词

13 条 `pendingRegistration` 条目全部加上 **`"status": "not_registered_src_only"`**，并新增顶层键
`srcOnlyPolicyNote` 明确声明：**该状态词是"明确不登记（src-only）"，不是"待登记"**，未来读者不得把它
当作 pending。逐条令牌：

`judgement.s7a2.timebase.profile_value_out_of_range`、`...timebase.rational_invalid`、
`...timebase.interval_reversed`、`...timebase.commit_window_invalid`、
`...timebase.commit_not_at_commit_tick`、`...late.parameter_pending`、`...late.policy_undeclared`、
`...late.duplicate_queue_entry`、`...late.queue_hop_exceeded`、
`...input.ingress_sequence_duplicate`、`...timebase.profile_invalid`、
`...input.mapping_declaration_invalid`、`...input.runtime_mapping_change`（共 13 条）。

### 3.5 `pendingRegistration` 与 `ownerReviewRequired` 的结构变化

| 动作 | 改前 | 改后 |
| --- | --- | --- |
| `presentation-target-missing` / `partial-group` 的 `code: null` 待登记条目 | 2 条 | **删除**（已登记为正式码，见 §3.1） |
| 运行期 `faulted` 码的 `code: null` 待登记条目 | 1 条 | **删除**（`ruleset.transaction_failed` 已登记；校验器第 5 条要求不得同时出现在两处） |
| `pendingRegistration` 条目总数 | 23 | **20**（13 src-only + 4 个 `code: null` 待闭合 + 1 个 `code_family` + 2 个 `category_set` + 1 个 `code_set`） |
| `ownerReviewRequired` 条目数 | 11 | **11（条数不变）** |
| `ownerReviewRequired` 条目的 `disposition` | 无 | 本轮关闭 10 条（severity 值集、输入/几何类别拼写、ABI 示例类别表、结构性禁止、禁止与未来能力、revision mismatch、版本迁移类别、码/类同义、src-only `kCapabilityCategory` 令牌、运行期 faulted 码），值均为 `closed_by_the_2026-10-02T21-03-39-462Z_card`；**仍开放 1 条**：Gameplay I 十类与 V2 九类的合并映射（`CM-D02`，不在本卡范围内） |
| `codes[]` / `categories[]` / `rejectionEntries[]` | 21 / 9 / 19 | **29 / 9 / 19** |

### 3.6 变更后的顶层计数

| 计数 | 值 | 校验器原样输出 |
| --- | --- | --- |
| 已登记码（`codes[]`） | **29** | `29 codes (3 frozen input/geometry)` |
| 类别（`categories[]`） | **9** | `9 categories` |
| `severity` 取值 | **3** | `3 severities`（`info` / `warning` / `error`） |
| `faulted` 取值 | **2** | `2 faulted behaviors`（`session_unaffected` / `session_faulted`） |
| `R-` 条目（`rejectionEntries[]`） | **19**（R-01…R-19 各一次） | `19 rejection entries` |
| 待登记条目（`pendingRegistration[]`） | **20** | `20 pending entries` |

## 4. 校验器正例（原始输出与退出码）

命令（按并发约束，**不运行** `cmake --build` / `ctest`，只跑 CMake 脚本模式）：

```
cmake -DCUEXIS_SOURCE_DIR="D:/Cuexis-worktree" -P cmake/VerifyGameplayDiagnosticsCodes.cmake
```

原始输出：

```
-- gameplay diagnostics code table OK: 29 codes (3 frozen input/geometry), 9 categories, 3 severities, 2 faulted behaviors, 19 rejection entries, 20 pending entries
```

退出码：**0**。

**变量说明（先读脚本确认）。** `cmake/VerifyGameplayDiagnosticsCodes.cmake` 的默认路径由
`CUEXIS_SOURCE_DIR` 拼出（`${CUEXIS_SOURCE_DIR}/schemas/cuexis.gameplay-diagnostics.v2.codes.json`，
本任务描述给的单命令未带该变量时会 `FATAL_ERROR: CUEXIS_SOURCE_DIR must name the repository root`），
`CUEXIS_DIAGNOSTIC_CODES_FILE` 是可选覆盖；两者都**未修改**，根 `CMakeLists.txt` 里注册
`cuexis_gameplay_diagnostics_codes` 的那段同样未改。

## 5. 校验器负例（原始输出与退出码）

负例全部使用 `$env:TEMP` 下的副本（`%TEMP%\cuexis-diag-codes-neg\`），**仓库文件未被污染**。

### 5.1 负例 1：把某码的 category 改成非法值

样本 `neg1-illegal-code-category.json`（`codes[0].category` 由 `unknown_capability` 改为 `capability`）：

```
CMake Error at cmake/VerifyGameplayDiagnosticsCodes.cmake:46 (message):
  cuexis_gameplay_diagnostics_codes: codes[0] ('capability.unknown') has
  category 'capability', which is not one of the registered categories:
  unknown_capability, capability_disabled, budget_exceeded,
  ambiguous_migration, non_terminating_source, non_unique_solution,
  invalid_relation, late_policy_incomplete, identity_closure_incomplete
Call Stack (most recent call first):
  cmake/VerifyGameplayDiagnosticsCodes.cmake:256 (cuexis_codes_fail)
```

退出码：**1**。预期消息逐字命中：`has category 'capability', which is not one of the registered categories`。

### 5.2 负例 2：删掉一个 `R-` 条目（R-07）

样本 `neg2-missing-r07.json`：

```
CMake Error at cmake/VerifyGameplayDiagnosticsCodes.cmake:46 (message):
  cuexis_gameplay_diagnostics_codes: GAMEPLAY_V2_SPEC.md 7.2 freezes exactly
  19 rejection entries (R-01..R-19), 'rejectionEntries' holds 18
Call Stack (most recent call first):
  cmake/VerifyGameplayDiagnosticsCodes.cmake:340 (cuexis_codes_fail)
```

退出码：**1**。预期消息逐字命中：`freezes exactly 19 rejection entries (R-01..R-19), 'rejectionEntries' holds 18`。

### 5.3 负例 3：给输入族加第四个码

样本 `neg3-fourth-input-code.json`（新增 `input.discontinuity_unsupported`）：

```
CMake Error at cmake/VerifyGameplayDiagnosticsCodes.cmake:46 (message):
  cuexis_gameplay_diagnostics_codes: code 'input.discontinuity_unsupported'
  extends the frozen input.* family.  Only input.continuous_unsupported,
  input.direction_unsupported, geometry.inference_rejected may be registered
  there
Call Stack (most recent call first):
  cmake/VerifyGameplayDiagnosticsCodes.cmake:293 (cuexis_codes_fail)
```

退出码：**1**。预期消息逐字命中：`extends the frozen input.* family`。

### 5.4 负例 4（附加）：把九类之一改名

样本 `neg4-illegal-category-name.json`（`categories[0].name` 由 `unknown_capability` 改为 `capability`）：

```
CMake Error at cmake/VerifyGameplayDiagnosticsCodes.cmake:46 (message):
  cuexis_gameplay_diagnostics_codes: GAMEPLAY_V2_SPEC.md 9.2 category
  'unknown_capability' is missing from 'categories'
Call Stack (most recent call first):
  cmake/VerifyGameplayDiagnosticsCodes.cmake:149 (cuexis_codes_fail)
```

退出码：**1**。预期消息逐字命中：`GAMEPLAY_V2_SPEC.md 9.2 category 'unknown_capability' is missing from 'categories'`。

## 6. 文档门禁与计数复核

### 6.1 本轮跑过的门禁

| 门禁 | 命令 | 结果 |
| --- | --- | --- |
| 码表校验器（正例） | `cmake -DCUEXIS_SOURCE_DIR="D:/Cuexis-worktree" -P cmake/VerifyGameplayDiagnosticsCodes.cmake` | **exit 0**（§4） |
| 码表校验器（负例 ×4） | 同命令 + `-DCUEXIS_DIAGNOSTIC_CODES_FILE=<TEMP 副本>` | **全部 exit 1**，消息逐字命中（§5） |
| 文档门禁 | `python -B tools/check_docs.py` | **exit 0**（§6.2） |
| 空白检查 | `git diff --check` | **exit 0**（§6.3） |
| 行尾空白 / 制表符扫描 | 改动文件的逐行扫描 | 新增行 **0** 行尾空白 / **0** 制表符（§6.4） |

**未运行且不应运行的门禁**：`cmake --build`、`ctest`（另一条线正在 `engine/judgement/**` 做
`--clean-first` 构建，会争用 `out/build/debug`）。因此本文不声称任何构建或 CTest 全量结果。

### 6.2 `python -B tools/check_docs.py`

原始输出（逐字）：

```
Documentation checks passed: 339 Markdown files and 20 candidate JSON/CXT files validated.
```

退出码：**0**。

**历史与订正说明。** 在本报告创建**之前**，`check_docs.py` 是 **exit 1**，唯一失败项为
`docs/api/GAMEPLAY_V2_ABI.md: broken relative link: ../stage_reports/stages/stage-07/2026-10-03-s7a-diagnostics-taxonomy-rulings.md`
（ABI §S7A-7 之后的收尾一节先写下了指向本文的相对链接，而本文当时尚未创建）。本文创建并挂入
[stage_reports/README.md](../../README.md) 索引后恢复全绿：**339 个 Markdown 文件 + 20 个候选
JSON/CXT 文件全部通过，exit 0**。

### 6.3 计数复核

| 计数 | 期望 | 实际 | 结论 |
| --- | --- | --- | --- |
| CONTRACT_MATRIX 总数 | 114 | 114 | 未改（本轮不碰 `CONTRACT_MATRIX.md`） |
| 六类分布 | `accept` 52 / `revise` 26 / `supersede` 17 / `retain` 17 / `open` 0 / `reject` 2 | 同上 | 未改 |
| §0.1 行 / 项 | 76 行 / 84 项 / 末列 21 | 同上 | 未改（本卡不是新轮次） |
| Spec §7.2 拒绝条目 | 19 | 19（表格 R-01…R-19 各一次） | 未改 |
| Spec §9.2 类别 | 九类 | 九类 | 未改 |
| ABI 域数 / 类型行 | 9 域 / 126 = 96 + 30 | 9 域 / 126 = 96 + 30 | 未改（本轮不新增类型条目） |
| ABI §未决项条目数 | 20 | 20 | 未改（本轮不新增编号） |
| 码表 `codes[]` | — | 29 | 21 → 29（+8：4 条正式化 + 1 条新增 + 3 条此前只有待登记形态） |

> 计数口径说明：`codes[]` 由 21 增至 29 是**码表本身的登记增长**，不是 Spec §7.2 的 R 条目增长、
> 也不是类别增长；九类与 19 条 R 条目都未变。

### 6.4 空白 / 制表符 / 编码扫描（逐文件实测）

| 文件 | 行数 | BOM | CRLF | Tab | 行尾空白 | 备注 |
| --- | --- | --- | --- | --- | --- | --- |
| `docs/formats/GAMEPLAY_V2_SPEC.md` | 2299 | 无 | 0 | 0 | 0 | 纯 LF |
| `docs/api/GAMEPLAY_V2_ABI.md` | 1293 | 无 | 0 | 0 | 0 | 纯 LF |
| `schemas/cuexis.gameplay-diagnostics.v2.codes.json` | 1052 | 无 | 0 | 0 | 0 | **非 ASCII 字符 0（纯 ASCII）**、纯 LF |
| `docs/stage_reports/README.md` | 78 | 无 | 0 | 0 | 0 | 纯 LF（+1 索引行） |
| `docs/proposals/gameplay-v2-acceptance/OPEN_QUESTIONS.md` | 462 | 无 | 0 | 0 | 0 | 只追加 §4.6 与第八批追加注 |
| `docs/proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md` | 686 | 无 | 0 | 0 | 0 | 只追加 §9 之后的同次追加块 |
| 本文 | 413 | 无 | 0 | 0 | 0 | 纯 LF |

`git diff --check`：**exit 0**（无行尾空白 / 制表符告警）。

## 7. 陈旧表述核对（`待冻结` / `待汇总` / `待登记` 指向码表的活体命中）

扫描范围：`docs/api/GAMEPLAY_V2_ABI.md` 与 `docs/formats/GAMEPLAY_V2_SPEC.md`（其余文件不在允许修改
范围内）。逐条解释：

| # | 位置 | 改前 | 改后 | 解释 |
| --- | --- | --- | --- | --- |
| 1 | Spec §9.5 开头段（现 L1657-L1663） | 原句随后写"该表的**逐条内容**待工具侧落地"（原 L1655 段落） | 改为"**`schemas/cuexis.gameplay-diagnostics.v2.codes.json`** + CTest 校验项 **`cuexis_gameplay_diagnostics_codes`**（脚本 `cmake/VerifyGameplayDiagnosticsCodes.cmake`，可在 CMake 脚本模式下运行）+ 该表的**逐条内容继续增补**（新增码仍须经登记 + 该 CTest 校验）" | 原句把已落地的码表说成"待工具侧落地"，且不提校验门禁；现指向文件、校验项与脚本并说明增补规则 |
| 2 | Spec §9.5 第 6 轮更新段（现 L1665-L1671） | "**仍未冻结**的是：`severity` 取值域与 `faulted` 行为的逐码具体判定…" | 改为"**未冻结**的只剩 Replay 十类稳定错误与 V2 九类的**合并映射表**，以及**尚未登记的字符串**；`severity` / `faulted` 的真值已由集中码表持有（§9.1）" | `severity` 取值域与逐码 `faulted` 已由本卡定案，原句已过期 |
| 3 | Spec §10 工件表 diagnostics 行（现 L1756） | 原为"**待创建**"（与同表其余"尚不存在"工件同列） | 改为"**已落地**为 `schemas/cuexis.gameplay-diagnostics.v2.codes.json`（+ CTest 校验项与脚本）" | 该表是"工件是否存在"的表；码表文件已存在，故"待创建"已不准确 |
| 4 | Spec §11.1 第 3 条（现 L2242-L2247） | "…**逐条码表内容**仍属后续批次" | 追加"**码表文件与校验项本身已落地**（`schemas/cuexis.gameplay-diagnostics.v2.codes.json`）" | 保留"逐条内容仍属后续批次"的正确部分，只补落地事实 |
| 5 | ABI §码表位置（现 L619-L636） | "**逐条码表内容**仍属工具侧落地（属接受后的独立工作项、不属 S7A-0）" | 改为"**逐条码表内容**随各轮裁定逐条增补…**码表文件与校验项本身已落地**（权威内容以 JSON 为准，可执行门禁为 CTest 校验项 + 脚本 `cmake/VerifyGameplayDiagnosticsCodes.cmake`）" | 同上 |
| 6 | ABI §仍待冻结 diagnostics 行（现 L691） | "**逐条码表内容与 severity 取值域**仍属工具侧落地与后续批次" | 改为"**码表文件与校验项已落地**，**逐条内容随各轮裁定增补**（并列本卡三项落地）；`severity` / `faulted` 取值集见 §诊断分层" | `severity` 取值域已定案，原句过期 |
| 7 | ABI §S7A-7 后续批次阻塞项第 1 行（现 L1092） | 阻塞项列写"诊断码表的**逐条内容**（除已冻结三码与既有 R-09 映射外）、`severity` 取值域与 `faulted` 判定"，首次消费列写"并行工具线" | 改为阻塞项"诊断码表的**逐条内容继续增补**、`severity` 取值域与逐码 `faulted` 判定"，首次消费"权威内容已在 `schemas/cuexis.gameplay-diagnostics.v2.codes.json`（决策卡 2026-10-02T21:03:39.462Z）；新增码仍须经登记 + CTest 校验项 `cuexis_gameplay_diagnostics_codes`" | 原句暗示整表未落地；现区分"已落地内容"与"后续增补" |

**无"待汇总"活体命中**：两个文件里没有把该码表描述为"待汇总"的表述。

**保留不改的 3 处（仅报告，不修）**：Spec §9.5 中"未登记的码字符串仍属后续批次"的措辞保留——
它描述的是**尚未登记的候选字符串**（而非该码表本身），与 §9.6 第 3 条的 src-only 约束一致。

## 8. 需要 Codex 复核处与我自己的判断

### 8.1 并行实现线的两处实测缺陷（本卡"防误用句"的证据）

并行实现线（`engine/judgement/**`，本任务未触碰）在按本裁定改正前，实测有两处**把码族分组词当成
`category` 发出**的缺陷：

| # | 位置 | 缺陷 | 改正后的类别 |
| --- | --- | --- | --- |
| 1 | 共享 capability 拒绝路径 | 曾把 `category` 写成 **`capability`** | `capability_disabled` |
| 2 | 会话生命周期顺序违规 | 曾把 `category` 写成 **`lifecycle`** | `invalid_relation` |

两者都已按本裁定改走九类内的既有类别，且都不在九类内。这正是 ABI §诊断分层 追加句要写明
"**任何码族分组词都不得作为 `category` 写出**"的理由：原代码块的 `identity / capability / budget /
format / migration / lifecycle ...` 列表**极易被读成合法取值**。

**裁定要求的边界（本轮严格遵守）：** 该 ` ```text ` 代码块（原 L582–L587 / 现 L582–L587）位于第 6 轮冻结的
`S7A6-R06` 结构块内，**逐字未改**（含 `category       分类枚举（identity / capability / budget / format /
migration / lifecycle ...）` 这一行），只在其**之后**追加限定句；§码族来源 段的族列表也逐字未改。

### 8.2 我做的解释性判断（未获逐字裁定，但按裁定文义推导）

1. **把三条"只有待登记形态"的码正式化**：`identity_closure_incomplete`、`presentation-target-missing`、
   `partial-group` 先前在码表里只有 `code: null` 的 `pendingRegistration` 条目。裁定第 4 条只要求
   `presentation-target-missing` / `partial-group` 写入码表；为避免同一码既"待登记"又"已登记"（校验器第 5 条
   会直接失败），我把三条都登记为正式码。**请复核 `identity_closure_incomplete` 是否也该保留一条
   `pendingRegistration` 形态**（我判断不该：Spec §9.6 第 4 条已把它列为冻结码）。
2. **`stableRejectCode = null`**：四个新条目都不是 `CapabilityRecord.stableRejectCode`，因此置 `null`。
   `CapabilityRecord.stableRejectCode` 是 `JudgementError` 的码字符串，`ruleset.transaction_failed` 是运行期
   失败码而非 registry reject code。**请复核**这一区分是否与域 8 `CapabilityRecord` 行的文义一致。
3. **`severity` / `info` / `warning` 的定位**：裁定说"core `DiagnosticSeverity` 只有 Info/Warning/Error"
   且"全部取 error"。我把 `info` / `warning` 登记为**闭集成员但本轮无码使用**，而不是把它们从值集里删掉。
   **请复核**值集是否应只保留 `error`。
4. **`format.gameplay_version_unsupported` 的 source 指针（A4 `revise`，已按裁定执行）**：原指针是行号锚
   `GAMEPLAY_V2_ABI.md#L633-L634`，现已换成**章节引用** `GAMEPLAY_V2_ABI.md 码族来源（格式与迁移族）`；
   同一批次里其余**单行事实**的行号锚也一并换成章节引用（逐条见 §11.2 与集中码表
   `sourceAnchorPolicy`）。**请复核**"整段语义范围锚（`#L599-L617`）保留为带日期证据指针"这一例外是否可接受。
5. **`ownerReviewRequired` 的处置方式**：裁定关闭了 10 条，我保留 11 条并给被关闭者加
   `"disposition": "closed_by_the_2026-10-02T21-03-39-462Z_card"`，而不是删除条目——保留可追溯性，同时
   避免读者把它当作仍开放。**请复核**是否应改为删除。
6. **不新立"第 8 轮"**：本卡是第 6 轮首次消费的后续补齐，不是新轮次；因此 §0.1 的逐轮行 / 项合计
   （76 行 / 84 项）不变，登记写成"第 6 轮 … 的首次消费后续补齐"。**请复核**登记措辞是否需要
   另外的轮次标识。
7. **ABI §诊断分层的示例段**：我按裁定只加了限定句（并在其中列出上述两处实测反例），**没有**改示例里的
   `identity / capability / budget / format / migration / lifecycle` 类名列表（裁定只要求加限定、不要求改
   列表）。**请复核**是否也要求把示例列表改成九类之一。
8. **六个码的 S7A-3 依据（A8 已按裁定落地）**：`capability.*` 与 `input.direction_unsupported`（R-06）在
   `BATCH_GATES.md` 的 S7A-3 诊断行里原非逐字列出（该行列出的是 `budget.content.*` /
   `pattern.relation_unsupported` / `non_terminating_source` / `invalid_relation` /
   `identity_closure_incomplete` / `migration.ambiguous`）。我把它们登记为 S7A-3 是依据裁决正文
   （"BATCH_GATES 将能力 / 迁移拒绝的首次实际消费落在 S7A-3"）；**A8 授准后已把该行补齐**（逐字见
   §11.3 C-3）。**请复核**补齐措辞是否覆盖了 S7A-3 的全部实际消费面。

### 8.3 越界残留的处置（第 2 批已按 C 授准执行；本小节保留"执行前"的判断记录）

> **本小节是第 2 批执行前的记录，保留可追溯性。** 第 2 批已按裁定完成 C-1 / C-2 / C-3 的**现行措辞**改动
> 与 C-4 的**历史报告订正说明**；实际改动与逐字对照见 **§11.3**。以下三条**不再是未改项**，只记录当时的
> 判断口径：
>
> - `docs/proposals/gameplay-v2-acceptance/CONTRACT_MATRIX.md`:146（`CM-T10` 行）与 `OPEN_QUESTIONS.md`:244
>   的"类别 capability"已在第 2 批改为 `capability_disabled`（§11.3 C-1 / C-2）。
> - `docs/stage_reports/stages/stage-07/2026-10-03-s7a-2-input-implementation-rulings.md`:43 属**带日期的
>   历史报告**，正文**不改写**，订正说明见 §11.3 C-4。
> - `docs/proposals/gameplay-v2-acceptance/BATCH_GATES.md`:45 与 S7A-3 诊断行已在第 2 批更新
>   （§11.3 C-3）。

- **src-only 类名令牌的现状（第 2 批核对结果）**：`engine/judgement/src/source_codes.hpp` 原定义的
  `kCapabilityCategory{"capability"}` **已由并行实现线改名**为 `kCapabilityDisabledCategory{"capability_disabled"}`
  （`source_codes.hpp` 与 `src/session.cpp` 现均只出现后者；源码注释明确写"**no token spelling a bare
  "capability" category**"）。本任务**未改任何 C++**，也不为并行线的改动背书；集中码表
  `ownerReviewRequired` 的对应条目已按**实际现状**改写。风险仍记在该条目：
  **任何公共投影若泄漏 `capability` 字面值即违反九类门禁**。

## 9. 相关索引

- [GAMEPLAY_V2_SPEC.md](../../../formats/GAMEPLAY_V2_SPEC.md)：§9.1 取值集、§9.2 九类、§9.4 运行期 `faulted` 专有码、§9.5 待裁定项、§9.6 登记约定、**§9.9 诊断码公开性的判定依据（B 的独立裁定正文）**
- [GAMEPLAY_V2_ABI.md](../../../api/GAMEPLAY_V2_ABI.md)：域 8（含**公共诊断码的判据与承载面**段）、§错误与诊断映射、§仍待冻结、§S7A-7 后续批次阻塞项
- [`schemas/cuexis.gameplay-diagnostics.v2.codes.json`](../../../../schemas/cuexis.gameplay-diagnostics.v2.codes.json)：集中码表（`rulingProvenance` / `srcOnlyPolicyNote`）
- [`cmake/VerifyGameplayDiagnosticsCodes.cmake`](../../../../cmake/VerifyGameplayDiagnosticsCodes.cmake)：CTest 校验项 `cuexis_gameplay_diagnostics_codes` 的脚本（本轮未修改）
- [第 6 轮门禁报告](2026-10-03-s7a-6-gate-rulings.md)：第 6 轮 15 行 / 17 项裁定与码表冻结
- [第 7 轮收尾报告](2026-10-03-s7a-7-wrapup-rulings.md)：第 7 轮 16 行 / 18 项裁定与 `open` 闭合
- [BATCH_GATES.md](../../../proposals/gameplay-v2-acceptance/BATCH_GATES.md)：S7A-1 诊断类别的九类口径、S7A-3 首次消费说明（§11.3 C-3）
- [CONTRACT_MATRIX.md](../../../proposals/gameplay-v2-acceptance/CONTRACT_MATRIX.md)：`CM-T10` 行的类别口径订正（§11.3 C-1）
- **§11 处置确认裁定的逐条落地**：A1–A8 核对、B 的公共性判据与五条映射、C 的越界残留处置、四道门禁
- **§11.3 C-4** 即 [2026-10-03-s7a-2-input-implementation-rulings.md](2026-10-03-s7a-2-input-implementation-rulings.md) 第 43 行 `类别 capability` 的**订正说明**（该历史报告正文不改写）
## 10. 改动文件清单（每个文件改了什么 + 逐字节口径）

改动集合**只**包含下列 12 个文件；**没有**改动 `engine/**`、`tests/**`、`tools/**` 或根 `CMakeLists.txt`，
**没有**执行 `git add` / `git commit`。下表行数为**本批结束时**的实测值（口径见 §11.5）。

| 文件 | 改动性质 | 改动点 | 逐字节口径 |
| --- | --- | --- | --- |
| `docs/formats/GAMEPLAY_V2_SPEC.md` | 就地改写 + 追加 | 4 处类别逐字替换（L381 / L1374 / L1620 / L1879）＋ §9.1 取值集表、§9.4 专有码段、§7.2 两表 / 段、§9.5 / §10 / §11.1 同步、**新增 §9.9**（5 条 + 附则 + 5 行核对表，**第 3 批收紧第 1 条判据并加第 3 条附则**）、§12 追加索引 | 见 §11.5、纯 LF、无 BOM、无 Tab、无行尾空白 |
| `docs/api/GAMEPLAY_V2_ABI.md` | 就地改写 + 追加 | 1 处类别逐字替换（L874）＋ §诊断分层防误用句、§码族来源限定句、§错误与诊断映射 4 行表、**域 8「公共诊断码的判据与承载面」段（第 3 批收紧判据 + 加公共承载面 / src-only 区分）**、`info` / `warning` 暂未使用注、§码表位置 / §仍待冻结 / §S7A-7 阻塞项 / 收尾节同步 | 见 §11.5、纯 LF、无 BOM、无 Tab、无行尾空白 |
| `schemas/cuexis.gameplay-diagnostics.v2.codes.json` | 就地改写 + 新增条目 | 8 条 category 定案、6 条 `ownerBatch`、13 条 `status`、4 条新码、3 条待登记删除、10 条 `disposition`（含裁定卡标识）、**49 处活锚 → 稳定引用**（30 处章节引用 / 6 处删除 / 13 处符号名）、**20 条 `pendingRegistration` 全部加 `publicConsumptionSurface: null`**、**第 4 批为 `[16]` 加纯事实字段 `alsoObservedFamilies`**、顶层 `rulingProvenance` / `srcOnlyPolicyNote` / **`diagnosticStringPolicy`（含 `publicSurfaceRule`）** / **`sourceAnchorPolicy`（只留策略 + 计数 + 证据指针）** | 见 §11.5、**纯 ASCII**、纯 LF、无 BOM、无 Tab、无行尾空白、**`#L` 出现 0 次** |
| `docs/stage_reports/stages/stage-07/2026-10-03-s7a-diagnostics-taxonomy-rulings.md` | **新建 + 第 2 / 3 批扩写** | 本文（11 节，含 §11.7 的 49 行对照证据记录） | 见 §11.5、纯 LF、无 BOM、无 Tab、无行尾空白 |
| `docs/stage_reports/README.md` | 追加 1 行索引 | 1 | 见 §11.5、纯 LF、无 BOM、无 Tab、无行尾空白 |
| `docs/proposals/gameplay-v2-acceptance/OPEN_QUESTIONS.md` | **只追加** §4.6 + 第八批追加注；**第 2 批改 1 个词** | 3 块 | 见 §11.5、纯 LF、无 BOM、无 Tab、无行尾空白 |
| `docs/proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md` | **只追加** §9 之后的同次追加块 | 1 块 | 见 §11.5、纯 LF、无 BOM、无 Tab、无行尾空白；**§9 表未新增行、§0.1 / §8 未改** |
| `docs/proposals/gameplay-v2-acceptance/CONTRACT_MATRIX.md` | **第 2 批（C-1）改 1 个词** | 1 | 见 §11.5、**原本即 CRLF**、无 BOM、无 Tab、无行尾空白；计数与表体结构未动 |
| `docs/proposals/gameplay-v2-acceptance/BATCH_GATES.md` | 第 2 批（A8 + C-3）改 2 行 + 追加 2 段；第 3 批撤下 6 行映射表；**第 4 批限定 S7A-1 口径到本行 + 追加"后续行为未冻结历史占位"注记** | 6 | 321 行（LF）、纯 LF、无 BOM、无 Tab、无行尾空白；未新增 / 删除类别名 |
| `docs/DOCUMENTATION_POLICY.md` | **第 3 批新增 1 条规则**（`## 链接和重复` 列表末尾，逐字见 §11.8） | 1 | 见 §11.5、纯 LF、无 BOM、无 Tab、无行尾空白 |
| `cmake/VerifyGameplayDiagnosticsCodes.cmake` | **第 3 批新增 Rule 6 + 一行 STATUS**（正例行逐字保留） | 2 | 见 §11.5、纯 LF、无 BOM、无 Tab、无行尾空白 |
| `docs/stage_reports/stages/stage-07/2026-10-03-s7a-2-input-implementation-rulings.md` | **未改（历史报告）** | 0 | 正文保持原样，订正说明见 §11.3 C-4 |

**未改动的相关文件（有意保留）：** `docs/CURRENT_STATUS.md`、`docs/ROADMAP.md`、
`docs/adr/0044-gameplay-v2-semantic-kernel.md`（不在允许修改路径内）、
`engine/judgement/src/source_codes.hpp` 与 `session.cpp`（并行实现线所有，本任务未触碰）、
`docs/stage_reports/stages/stage-07/2026-10-03-abi-document-restoration.md`（另一条线所有，见 §11.7 的
并发判断）。

**共享文件 `CMakeLists.txt` 的归属（避免误认）。** `CMakeLists.txt` 在 `git status` 里是 ` M`，但**该文件
里的改动不是一条线独占的**：其中 `cuexis_gameplay_diagnostics_codes` 的 `add_test` + `set_tests_properties`
块（现 L~243-L256，`LABELS "gameplay;judgement;diagnostics;schemas"` / `TIMEOUT 60`）**是第 6 轮由本任务
加入的**（就是本文档所说的那个 CTest 校验项）；同一文件里的其它 hunk 属并行线。**本批（第 3 批）未再改
`CMakeLists.txt`。** 同一情况也适用于 `docs/stage_reports/stages/stage-07/` 目录：其中
`2026-10-03-abi-document-restoration.md` 与 S7A-3…S7A-7 各报告属其它线，本文只拥有
`2026-10-03-s7a-diagnostics-taxonomy-rulings.md`。

## 11. 第 2 批：处置确认裁定的逐条落地（A1–A8 / B / C）

**provenance。** Codex 话题 thread `diagnostics-code-table-disposition`，threadId
`01a0fe97-f1b6-7332-b0c9-cafc3c9f25ec`，模型 `gpt-6-astra`，difficulty `hard`，**verdict `reject`**（指
"不原样接受全部 8 项"而给出逐项处置），**confidence 0.7**，决策卡
`2026-10-02T21:51:38.591Z`。**该卡自报风险**：它是**补答**（首次尝试空答复、桥自动补答），
**没有独立核验映射行与既有 ABI 承载方式**，并要求"执行前须按既有合同核对具体文字，不得据此新增 ABI 项"。
因此本节逐条 **先回原文核对、再动手**，并把核对结果与两处口径说明写进 §11.2。

**第 3 批续问卡（2026-10-03，同 thread，`resumed: true`）。** 卡 `2026-10-02T22:02:31.461Z`，模型
`gpt-6-astra`，**verdict `reject`（逐项处置）**，**confidence 0.78**。它**推翻**了第 2 批"保留 1 处行号锚"
的宽松判断，并**否掉**了第 2 批自行推导的 6 行 Gameplay I → V2 映射表。5 项处置的落地见 **§11.7**
（第 1 项：JSON 行号锚清零 + 逐条对照移入带日期证据记录）、**§11.8**（第 2 项：文档政策新规则）、
**§11.9**（第 3 项：撤下映射表）、**§11.2**（第 4 项：判据收紧）、**§11.10**（第 5 项：公共承载面与
src-only 的区分 + 校验器新检查）。

### 11.1 A1–A8 逐条核对结果与是否改动

| 项 | 裁定 | 核对结果（回原文） | 是否改动 |
| --- | --- | --- | --- |
| **A1** | accept：3 条"已确定首次消费"的候选从 `pendingRegistration` 移到注册码；不得两处并存 | 核对：`identity_closure_incomplete` / `presentation-target-missing` / `partial-group` 均**只**在 `codes[]` 中，`pendingRegistration` 中**无**同名条目（3 条待登记已删除）。校验器"pending 与 registered 不得并列"（§4 正例 exit 0）本身就是证明 | **无需改动**（现状已符合） |
| **A2** | accept：4 条新码保留 `stableRejectCode = null` | 核对：4 条（`identity_closure_incomplete` / `presentation-target-missing` / `partial-group` / `ruleset.transaction_failed`）`stableRejectCode` 均为 `null`，且 `note` 已写明"非 `CapabilityRecord.stableRejectCode`" | **无需改动** |
| **A3** | accept：保留 `info` / `warning` / `error` 闭集，并注明前两者**暂未使用** | 核对：JSON `severityValues` 为 3 值闭集；已登记的 29 条**全部** `severity = error` | **已补注**：ABI 域 8 `DiagnosticSeverity` 行尾追加"**前两者暂未使用**：`info` 与 `warning` 是闭集成员，7A 已登记码中无一条使用它们" |
| **A4** | revise：把 `format.gameplay_version_unsupported` 的 source 指针换成**稳定章节引用**；全仓检索该锚全部出现处并一并替换 | 核对：该锚在 JSON 中为 `GAMEPLAY_V2_ABI.md#L633-L634`（**1 处**，属 `format.gameplay_version_unsupported`）；新报告 §8.2 曾写 `#L640-L641`（**同一事实的错误行号**）。**实测**：JSON `codes[].source` 里行号锚字面量 **30 处**（第 2 批曾误记为 29，见下）；`severitySources` 另有 **6 处**（3 个值各 2 处：`diagnostics-identity-and-compatibility.md` 与 `CODE_POLICY.md`）；13 条 src-only 令牌的 `pendingRegistration[].source` 另有 **13 处** | **已替换 49 处活锚**：30 处 → 章节引用；6 处 `severitySources` 行号锚**删除**（只留文档名 + 按符号 `DiagnosticSeverity` 引用核心枚举）；13 处 src-only 行号锚 → **符号名引用**。JSON 新增顶层 `sourceAnchorPolicy` 说明口径；报告 §8.2 的 `#L640-L641` 已改为章节引用 |
| **A4-c**（第 3 批） | revise：那 17 条码仍在用的 `#L599-L617` **也必须走**；如需保留行范围，只许放在带文档版本 / 提交标识的历史证据记录里 | 核对：第 2 批把该锚留作"带日期证据指针"，并把它连同 10 条"旧锚 → 新引用"对照一并写进 JSON 策略字段——**这使 JSON 仍含 13 处行号锚字面量**（1 处保留列表 + 10 处对照列表 + 2 处 severity 注） | **已清零**：JSON **不再保留任何**行号锚（`datedLineRangeAnchors` / `replacedAnchors` 两项整块撤下），只留**策略文本 + 替换计数 + 指向带日期证据记录的指针**；49 处活锚的逐条对照见 **§11.7**（锚定 ABI 快照 `c49447e4…`） |
| **A4-b**（A4 核对副产物） | —（自行核对时发现） | 核对：13 处 src-only 行号锚**已全部漂移一位**——被引范围内的令牌仍落在引用的最后一行，但范围本身随并行实现线新增注释下移了 1 行。逐条实测：`profile_value_out_of_range` 引 75-76 / 实际 76、`rational_invalid` 引 77-79 / 实际 79、`interval_reversed` 引 80-82 / 实际 82、`commit_window_invalid` 引 83-85 / 实际 85、`commit_not_at_commit_tick` 引 86-88 / 实际 88、`late.parameter_pending` 引 94-95 / 实际 95、`late.policy_undeclared` 引 96-101 / 实际 101、`late.duplicate_queue_entry` 引 102-104 / 实际 104、`late.queue_hop_exceeded` 引 105-107 / 实际 107、`input.ingress_sequence_duplicate` 引 117-120 / 实际 120、`timebase.profile_invalid` 引 124-126 / 实际 126、`input.mapping_declaration_invalid` 引 127-133 / 实际 133、`input.runtime_mapping_change` 引 134-137 / 实际 137 | **已改**（见 A4）。第 3 批 **保留** 该修正，并**据此**在 `docs/DOCUMENTATION_POLICY.md` 立了通用规则（见 §11.8）——**不**写进 Gameplay Spec |
| **A5** | revise：保留 11 条 `ownerReviewRequired`；已关闭的 10 条标记处置并**引用裁定卡标识**；剩余 1 条保持未关闭 | 核对：条数 **11**；`disposition` 值原为 `closed_by_the_2026-10-02T21-03-39-462Z_card`（仅卡时间戳，**未引用 thread / verdict / confidence**）；未关闭的 1 条为 **Gameplay I 十类合并映射**（`CM-D02`） | **已订正 disposition 值（10 处）**为 `closed_by_decision_card_2026-10-02T21-03-39-462Z__thread_01a0fe6b-6e57-7051-87f8-cf9588e85568__verdict_adopt__confidence_0.96`；条数与"仅 1 条未关闭"**不变** |
| **A6** | accept：仍属"第 6 轮后续补齐"，不新增轮次行、不改 §0.1 的 76 行 / 84 项与末列 21 | 核对：卡片归属、文档登记措辞、`RULING_WORKSHEET.md` §0.1 与 §9 | **无需改动**（§0.1 与 §9 表体逐字未动） |
| **A7** | accept：ABI `### 诊断分层` 的冻结示例**逐字不动**，只保留其后的九类限定句与两处实测反例 | 核对：` ```text ` 块（现 L582-L587）含 `category       分类枚举（identity / capability / budget / format / migration / lifecycle ...）`，本轮**未触碰**；其后限定句与两处反例在 L589-L592 | **无需改动**（本批 A4 的替换未触该块） |
| **A8** | revise：`BATCH_GATES.md` 的 S7A-3 首次消费说明在单独获准范围内更新 | 核对：原 S7A-3 诊断类别行为 `budget.content.*`、`pattern.relation_unsupported`、`non_terminating_source`、`invalid_relation`、`identity_closure_incomplete`、`migration.ambiguous`，**未列出**本轮 6 个码 | **已改 `BATCH_GATES.md` S7A-3 诊断类别行**（逐字见 §11.3）；**第 3 批另按 §11.9 撤下** S7A-1 的 6 行映射表 |

### 11.2 B：公共诊断码的判据与五条映射（**本节即独立裁定说明**）

裁定要求把"分界依据是**有没有已文档化的公共映射行**、不是名称前缀"与"这 5 条到 ABI 域 8 的映射"写成
**可索引到的正文**（不能只在 JSON 注释里）。（**注**：该句是**第 2 批**的裁定原文；判据措辞已在**第 3 批**
收紧为下文的"三者可明确相互追溯"，见 §11.2 末段与 §11.7 第 4 项。）落地位置如下（三处，互为指针）：

1. **正典裁定正文**：`docs/formats/GAMEPLAY_V2_SPEC.md` **§9.9 诊断码公开性的判定依据**
   （新增节，5 条 + 5 行核对表）。
2. **ABI 消费侧口径**：`docs/api/GAMEPLAY_V2_ABI.md` **域 8** 表格后新增段"**公共诊断码的判据与承载面**"。
3. **机器可读依据**：集中码表 `diagnosticStringPolicy` + 每条 `codes[].publicConsumptionSurface`。

**判据（第 3 批收紧后的最终措辞，逐字，四处同义）。** 一条诊断码串是**公共码**，当且仅当它的
**具名码串**、**适用场景**与**九类映射**在正典文本中**可明确相互追溯**——即三者都能在 `SPEC §7.2` 拒绝条目行、
`SPEC §3.7.8` 映射表行或 `ABI 域 8` 具名行中读到，且彼此**有明确的交叉指向**。**场景行与码串行分处不同章节
是允许的**（例如场景行在 §3.7.8、码串行在 §9.3 或 ABI），只要二者**显式互相指向**。**名称前缀不决定
公开性**（`judgement.s7a2.*` 既不使其私有、也不使其公开）；**不设 owner 命名条件**。无法如此追溯的码串
**不得**作为公共码，只能作 src-only 令牌。公共码**一律**通过**既有**承载面消费：`JudgementError` /
`DiagnosticCode`（ABI 域 8）与 7A 既有稳定拒绝载荷；**不新增 ABI 域、不新增类型、不新增错误枚举**；
`DiagnosticCategory` 仍取 SPEC §9.2 九类。按此口径，**5 条已登记码保持注册、13 条 src-only 保持不变**。

**该判据的四处同义落点**（第 3 批统一为同一句语义）：①`SPEC §9.9` 第 1 条；②`ABI 域 8`
「公共诊断码的判据与承载面」段；③JSON `diagnosticStringPolicy.rule`；④本段。
**从"有已文档化的公共映射行"改为"三者可明确相互追溯"**，并显式写入"场景行与码串行可分处不同章节、
但须有明确交叉指向"与"不设 owner 命名条件"。

**"码 → 来源映射行 → 域 8 映射"逐条清单（本轮逐条回原文核对）。**

| # | 码 | 来源映射行（已文档化） | 域 8 承载面 | 类别 | 核对结论 |
| --- | --- | --- | --- | --- | --- |
| 1 | `judgement.s7a2.timebase.tick_overflow` | SPEC §3.7.8 映射表 **Tick 溢出行**（该行未逐字带码串；码串逐字出现在 ABI §时间单位与表达"负 `observationTick` 合法…"段） | `JudgementError` / `DiagnosticCode` | `budget_exceeded` | 有映射行；**两处口径说明①** |
| 2 | `judgement.s7a2.timebase.time_reversal` | SPEC §3.7.8 映射表 **校准会话时钟回退**行（结论亦见 SPEC §9.3 输入半批映射表第 3 行） | `JudgementError` / `DiagnosticCode` | `invalid_relation` | 有映射行 |
| 3 | `judgement.s7a2.input.amount_out_of_range` | SPEC §3.7.8 映射表 **`DomainAmount` 量越界 / 窄化 / 量化溢出**行（未逐字带码串） | `JudgementError` / `DiagnosticCode` | `budget_exceeded` | 有映射行；**两处口径说明②** |
| 4 | `judgement.s7a2.input.amount_narrowed` | 同上行（§9.3 输入半批映射表第 2 行给出"窄化 → `budget_exceeded`"） | `JudgementError` / `DiagnosticCode` | `budget_exceeded` | 有映射行；**两处口径说明②** |
| 5 | `judgement.s7a2.input.same_tick_collision` | SPEC §3.7.8 映射表 **同一 canonical 身份在同一 Tick 再次入场**行（未逐字带码串；码串见 §9.3 输入半批映射表第 1 行） | `JudgementError` / `DiagnosticCode` | `invalid_relation` | 有映射行；**两处口径说明①** |

**两处口径说明（必须随清单一起读，且不构成"缺映射行"）。**
① 第 1、5 条的 §3.7.8 行文给的是**场景**、不是码串本身，码串逐字出现在 §9.3 或 ABI 的对应段；
② 第 3、4 条的 §3.7.8 行文同样以场景命名，码串逐字出现在 ABI §数值域与量程"判定顺序：可表示性先于
范围"段。**场景行与码串行指向同一映射，且都在已冻结的 §3.7.8 / §9.3 / ABI 正文内**，因此五条**全部**
有真实映射行，**没有**任何一条需要"自行补一行映射"；本轮**未新增任何映射行**，也**未新增 ABI 域 / 类型 /
错误枚举**（ABI 域数仍 9、类型行仍 126 = 96 + 30、§未决项条目数不变）。

**13 条 src-only 令牌保持只供源码使用**：其 `pendingRegistration[].status = not_registered_src_only`，
并由顶层 `srcOnlyPolicyNote` 明说"该状态词不是待登记"；它们与上述 5 条的分界正是**有无已文档化映射行**，
而不是 `judgement.s7a2.*` 前缀（5 条中的第 1、2、3、4、5 条与 13 条令牌**同前缀**，但前者有映射行、
后者没有——这就是"前缀不决定公开性"的实证）。13 条令牌的 `source` 指针已从行号改为**符号名**
（`k…Code`），因为它们原来的 13 处行号锚**已全部漂移一位**（见 §11.1 A4-b）——这也证明"行号锚不是稳定合同"。

**公共性判据的三处落点（互为指针，任一处可索引到）。** ①Spec **§9.9**（正典裁定正文）；②ABI **域 8**
"公共诊断码的判据与承载面"段（消费侧口径）；③集中码表 `diagnosticStringPolicy` +
`codes[].publicConsumptionSurface`（机器可读依据）。三者措辞互不冲突：载体、判据与"不新增 ABI 面"的
约束逐字一致，均未复制任何 ABI 类型定义。

### 11.3 C：越界残留的处置（现行措辞改动 + 历史报告订正说明）

**C-1 `docs/proposals/gameplay-v2-acceptance/CONTRACT_MATRIX.md`（`CM-T10` 行，现 L146）**

改前（逐字）：``…都复用**既有 ABI 冻结码 `input.continuous_unsupported`（类别 `capability`，`capabilityId = input.trajectory.v1`…``
改后（逐字）：``…都复用**既有 ABI 冻结码 `input.continuous_unsupported`（类别 `capability_disabled`，`capabilityId = input.trajectory.v1`…``
**只改这一个词**；该行其余文字、114 项计数、六类分布与表体结构**未动**。

**C-2 `docs/proposals/gameplay-v2-acceptance/OPEN_QUESTIONS.md`（§4.3 表内，现 L244）**

改前（逐字）：``…既有 ABI 冻结码 `input.continuous_unsupported`（类别 `capability`，`capabilityId = input.trajectory.v1`，`remediation = S7B-1`）…``
改后（逐字）：``…既有 ABI 冻结码 `input.continuous_unsupported`（类别 `capability_disabled`，`capabilityId = input.trajectory.v1`，`remediation = S7B-1`）…``
**只改这一个词**；§4.3 表体结构、§4.5 / §4.6 与本文件计数**未动**。

**C-3 `docs/proposals/gameplay-v2-acceptance/BATCH_GATES.md`**

- **S7A-1 诊断类别行（现 L45）**：
  改前（逐字）：``| 诊断类别 | `identity_mismatch`、`unsupported_capability`、`invalid_requirement`、`budget_exceeded`、`time_regression`、`session.mutation_rejected` |``
  改后（逐字）：``| 诊断类别（Gameplay V2 九类；Gameplay I 的六个旧类名已按 Spec §9.2 / §9.9 并入） | `invalid_relation`、`capability_disabled`、`budget_exceeded`、`session.mutation_rejected` |``
  并在 S7A-1 表后新增"**S7A-1 诊断类别的九类口径**"段 + 6 行"旧类名 → V2 类别"映射表（`identity_mismatch` → `invalid_relation`；`unsupported_capability` → `capability_disabled`；`invalid_requirement` → `invalid_relation`；`time_regression` → `invalid_relation`；`budget_exceeded` → 同名；`session.mutation_rejected` → 类别 `invalid_relation`），每行都注明依据（§9.3 输入半批映射表、§9.3 三分法、第 6 轮 `capability` → `capability_disabled` 裁定）。
  该段同时**明确边界**：这是**现行文本的类别口径订正**，**不改变** Gameplay I ABI 的十类集合、**也不宣布** `CM-D02` 的完整 old→new 映射表已冻结（后者仍是集中码表 `ownerReviewRequired` 中**唯一保持未关闭**的项）。
- **S7A-3 诊断类别行（A8，现 L69）**：在原六个条目后追加"**另加** 2026-10-02T21:51:38.591Z 裁定把下列六个码的**首次消费**登记为 S7A-3"以及六个码及其类别（`capability.budget_insufficient` → `budget_exceeded`；`capability.revision_mismatch` → `unknown_capability`；`capability.permanently_unsupported` → `capability_disabled`；`capability.future_development` → `capability_disabled`；`input.direction_unsupported` → `capability_disabled`（**R-06**）；`format.gameplay_version_unsupported` → `ambiguous_migration`（**R-17**））。**未新增 / 未删除任何类别名**，S7A-3 其余行与文件计数未动。

**C-4 历史报告订正说明（独立一节，不改写正文）**

`docs/stage_reports/stages/stage-07/2026-10-03-s7a-2-input-implementation-rulings.md`（现 L43）仍写
``类别 `capability` ``，属**带日期的历史报告**，按仓库规则**不得改写正文**。订正说明落在
**本文 §11.3 C-4 即本段**（`docs/stage_reports/README.md` L38 的新报告索引行与本文 §9 相关索引都指向本文，
因此该订正说明**可被索引到**）：该行的类别应读作 **`capability_disabled`**（第 6 轮 `CM-D04` / `P1-09`
首次消费后续补齐裁定，决策卡 2026-10-02T21:03:39.462Z，`adopt` 0.96）；**历史报告正文保持原样**，
本订正说明是它的唯一权威替代读法。同类历史残留还有两条，均**只报告不改写**：
CONTRACT_MATRIX 的历史脚注（若后续出现）与其它带日期报告中的 `类别 capability` 措辞。

### 11.4 收尾门禁（原始输出与真实退出码）

| # | 门禁 | 原始输出 | 退出码 |
| --- | --- | --- | --- |
| ① | 码表校验器（正例，第 3 批） | 两行：`-- gameplay diagnostics code table OK: 29 codes (3 frozen input/geometry), 9 categories, 3 severities, 2 faulted behaviors, 19 rejection entries, 20 pending entries` 与 `-- gameplay diagnostics public surface split: 29 public codes carry a publicConsumptionSurface, 20 pending entries carry none (13 of them are src-only tokens)` | **0** |
| ①b | 校验器负例（7 个，见 §11.10 与下表） | 各自的 `CMake Error … cuexis_gameplay_diagnostics_codes: …` | **各 1** |
| ② | 文档门禁 | `Documentation checks passed: 340 Markdown files and 20 candidate JSON/CXT files validated.` | **0** |
| ③ | 空白检查 | `git diff --check` 无输出 | **0** |
| ④ | 逐字节 | 见 §11.5 | — |
| ⑤ | 计数 | 见 §11.6 | — |

**7 个负例的原始消息与退出码（均 `-DCUEXIS_DIAGNOSTIC_CODES_FILE=<负例>`，脚本模式）**

| 负例 | 触发的消息（逐字，CMake 折行已还原为空格） | 退出码 |
| --- | --- | --- |
| `neg1-illegal-code-category.json` | `codes[0] ('capability.unknown') has category 'not_a_category', which is not one of the registered categories: unknown_capability, capability_disabled, budget_exceeded, ambiguous_migration, non_terminating_source, non_unique_solution, invalid_relation, late_policy_incomplete, identity_closure_incomplete` | **1** |
| `neg2-missing-r07.json` | `GAMEPLAY_V2_SPEC.md 7.2 freezes exactly 19 rejection entries (R-01..R-19), 'rejectionEntries' holds 18` | **1** |
| `neg3-fourth-input-code.json` | `code 'input.brand_new_unsupported' extends the frozen input.* family. Only input.continuous_unsupported, input.direction_unsupported, geometry.inference_rejected may be registered there` | **1** |
| `neg4-illegal-category-name.json` | `GAMEPLAY_V2_SPEC.md 9.2 category 'unknown_capability' is missing from 'categories'` | **1** |
| **`neg5-srconly-with-public-surface.json`（新）** | `pendingRegistration[0] is a src-only token (status = not_registered_src_only) but declares a public carrier of JSON type STRING.  A src-only token must not claim a public consumption surface.  Use JSON null` | **1** |
| **`neg6-public-code-without-surface.json`（新）** | `codes[0] has no 'publicConsumptionSurface'.  A public registered code must name the existing carrier it is consumed through` | **1** |
| **`neg7-open-slot-with-public-surface.json`（新）** | `pendingRegistration[13] declares a public carrier of JSON type STRING.  Nothing in 'pendingRegistration' is public.  Use JSON null` | **1** |

**未运行**：`cmake --build`、`ctest`（另一条线正在构建 `engine/judgement/**`）。退出码取自
`$LASTEXITCODE`（**不加管道**，避免被 `Select-String` 的退出码覆盖）。

### 11.5 逐字节口径

**计数口径。** 下表的"行数"一律是**该文件的 LF 数**（含末行换行），因此比 `Get-Content` / 编辑器
"行数"口径可能多 1（例如 `GAMEPLAY_V2_SPEC.md` 编辑器报 2364 行、本表报 **2365**）。

**全批统一**：`BOM=False / CRLF=0 / Tab=0 / 行尾空白=0`；JSON 与 `.cmake` 为**纯 ASCII**（非 ASCII 字符 0）。
**两个 CRLF 例外**（都是该文件**原本即 CRLF**，本批只在其内部做定点改动、**未改其行尾风格**）：
`CONTRACT_MATRIX.md`（**373 CRLF / 0 bare LF**）与 `DOCUMENTATION_POLICY.md`（**145 CRLF / 0 bare LF**）。
逐文件实测（第 3 批结束时）：

| 文件 | 行数（LF） | BOM | CRLF | Tab | 行尾空白 | 非 ASCII |
| --- | --- | --- | --- | --- | --- | --- |
| `docs/formats/GAMEPLAY_V2_SPEC.md` | 2365 | False | 0 | 0 | 0 | 41869 |
| `docs/api/GAMEPLAY_V2_ABI.md` | 1309 | False | 0 | 0 | 0 | 29368 |
| `schemas/cuexis.gameplay-diagnostics.v2.codes.json` | 1173 | False | 0 | 0 | 0 | **0** |
| `docs/stage_reports/stages/stage-07/2026-10-03-s7a-diagnostics-taxonomy-rulings.md` | **936（自指，随本次收尾再变；此为测量时刻值）** | False | 0 | 0 | 0 | 14391 |
| `docs/stage_reports/README.md` | 79 | False | 0 | 0 | 0 | 1650 |
| `docs/proposals/gameplay-v2-acceptance/OPEN_QUESTIONS.md` | 462 | False | 0 | 0 | 0 | 14704 |
| `docs/proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md` | 686 | False | 0 | 0 | 0 | 27378 |
| `docs/proposals/gameplay-v2-acceptance/CONTRACT_MATRIX.md` | 373 | False | **373（原本即 CRLF）** | 0 | 0 | 10396 |
| `docs/proposals/gameplay-v2-acceptance/BATCH_GATES.md` | 321 | False | 0 | 0 | 0 | 7603 |
| `docs/DOCUMENTATION_POLICY.md` | 145 | False | **145（原本即 CRLF）** | 0 | 0 | 1699 |
| `cmake/VerifyGameplayDiagnosticsCodes.cmake` | 529 | False | 0 | 0 | 0 | **0** |

**注**：`GAMEPLAY_V2_ABI.md` 的 1309 LF 是**本任务自己改后的**值；本节 §11.7 记录的行号锚基准版本是
**1303 LF** 的定版（`c49447e4…`），二者必须分开读。

### 11.6 计数复核（全部不变）

| 计数 | 期望 | 实测 | 结论 |
| --- | --- | --- | --- |
| Spec §9.2 类别 | 九类 | 九类（校验器 `9 categories`） | 不变 |
| Spec §7.2 拒绝条目 | 19 | 19（校验器 `19 rejection entries`） | 不变 |
| ABI 域数 / 类型行 | 9 域 / 126 = 96 + 30 | 9 域 / 126 = 96 + 30 | **未新增域或类型行** |
| ABI §未决项条目数 | 20 | 20 | 不变 |
| 契约 | 114（`accept` 52 / `revise` 26 / `supersede` 17 / `retain` 17 / `open` 0 / `reject` 2） | 同上 | **本批只改 `CM-T10` 行 1 个词，未动计数** |
| §0.1 行 / 项 / 末列 | 76 行 / 84 项 / 21 | 76 行 / 84 项 / 21 | 不变（本卡不是新轮次） |
| 已登记码 / 待登记 / `ownerReviewRequired` | — | 29 / 20 / 11（其中**未关闭 1 条**：Gameplay I 十类合并映射） | 与 A1 / A5 一致 |
| 公共码 / src-only 令牌 | — | **29 / 13**（判据见 §11.2） | 新增 `diagnosticStringPolicy` 记录 |
| 公共承载面 / src-only / 空位 | — | **29 / 13 / 7**（校验器 public-surface split 行实测；三者互斥且相加为 `codes[]` 与 `pendingRegistration[]` 两数组长度） | 第 3 批新增区分与检查（§11.10） |
| JSON 内字面行号锚 | **0** | **0**（全文 `#L` 出现次数实测 0） | 第 3 批验收标准（§11.7） |
| 活锚返工总数 | — | **49**（30 码来源 + 6 severity 字面量 + 13 src-only）＋ 13 处记录内字面量，共清除 **62** | 与 JSON `replacementCounts` 同源（§11.7） |
| `BATCH_GATES.md` 的 6 行 old→new 推导表 | **0 行** | **0 行**（整表撤下，改为指针） | 第 3 批第 3 项（§11.9） |
| 第 4 批：`pendingRegistration` 条目数 / `openSlotCount` / src-only | 20 / 7 / 13（**不得变**） | **20 / 7 / 13**（加 `alsoObservedFamilies` 字段后复测；`[16].family` 仍为 `budget.*`、`registered` 仍 `false`、`code` 仍 `null`） | 未新增 pending 条目、未改校验器（§11.11） |

### 11.7 第 3 批第 1 项：JSON 行号锚清零 + 49 处活锚逐条对照（带日期的证据记录）

**为什么本节在这里而不是在 `2026-10-03-abi-document-restoration.md`。** 续问卡要求把"旧锚 → 新引用"的逐条
对照移入带日期的证据记录。该重建报告**由另一条线负责**，本轮核对时它的 mtime 是 `2026-10-03 06:01:55`
（几乎与本次改动同时），且其 §5 正在记录本轮编辑。按任务书给的退路——"**如你判断追加会与它冲突，就先只把
对照写进你自己的报告 §11.x，并告知我**"——我判断**并发追加会与它冲突**，故把对照写在本节，并在 JSON
`sourceAnchorPolicy.evidenceRecord` 里把指针指向本节。**如需归并，请把本节整块移动到该重建报告即可，内容
自足、不依赖本报告其它节。**

**锚定的定版（必须区分两个版本，否则本节读者会误以为行号对得上现行文件）。** 本节全部"旧行号锚"都是对
**版本 A** 读出的；而 `GAMEPLAY_V2_ABI.md` 在**本次裁定落地过程中**已被本任务自己改过一次（域 8 判据段落
收紧），因此现行落盘件是**版本 B**。**这正是行号锚不可作活引用的当场实证**：仅仅一次同文件的措辞编辑，
就使下面 30 行里的 `#L599-L617` 全部再次移位。

| 项 | 版本 A：行号锚读出的定版（**证据基准**） | 版本 B：现行落盘件 |
| --- | --- | --- |
| 文件 | `docs/api/GAMEPLAY_V2_ABI.md` | 同 |
| sha256 | `c49447e4b5418aa77b1d6c55a80977974140cf65ff078f0f53cf146bf44f6acd` | `dd79f021e18326da97119a3800de423bb03fa1012fe6b08e4262b67d68372c7d` |
| 字节数 | 155406 | 156201 |
| LF 数 | 1303 | 1309 |
| mtime | 2026-10-03 05:53:34 | 2026-10-03 06:04:10 |
| 关系 | 版本 A 的 sha256 / 字节 / LF / mtime 已于本轮**重新实测核对一致** | 版本 B = 版本 A + 本任务本轮对域 8「公共诊断码的判据与承载面」段的收紧（**+6 LF**，无表格行增删） |

**版本 A 的持久归档位置（复核入口）。** 版本 A **已不再存在于任何工作树中**：该文件在工作树里已被就地编辑
为版本 B，而它是 **untracked**（无 git 历史），无法用提交取回。因此主控已把它逐字归档到**工作树之外**的
固定路径；复核者按下列路径取回，不需要 `%TEMP%` 备份：

| 项 | 值 |
| --- | --- |
| 归档件 | `D:\Cuexis-worktree\.dsh\ai-work-linker\archive\2026-10-03-abi-truncation\GAMEPLAY_V2_ABI-version-A-c49447e4.md` |
| sha256 | `c49447e4b5418aa77b1d6c55a80977974140cf65ff078f0f53cf146bf44f6acd` |
| 字节数 | 155406 |
| LF 数 | 1303 |
| 归档说明 | 同目录 `MANIFEST.md`（含 A/B 两版对照表、依赖版本 A 的产物清单、`Get-FileHash` 取回校验步骤与限制披露） |

**取回校验（逐字，来自该 `MANIFEST.md` §5；主控已用 `Get-FileHash` 复核一致）**：

```powershell
$p = 'D:\Cuexis-worktree\.dsh\ai-work-linker\archive\2026-10-03-abi-truncation\GAMEPLAY_V2_ABI-version-A-c49447e4.md'
(Get-FileHash $p -Algorithm SHA256).Hash.ToLower()   # 必须等于 c49447e4b5418aa77b1d6c55a80977974140cf65ff078f0f53cf146bf44f6acd
(Get-Item $p).Length                                  # 必须等于 155406
```

**结论**：下表的行号只对**版本 A** 有意义；对照的目标本来就是"把活引用从行号改成章节名 / 符号名"，
所以版本 B 不必与行号对得上。**任何复核都须先按上表固定路径取回版本 A，并按 sha256 与字节数校验**，
再与本节表内数值逐字比对；不一致即说明取回的不是证据基准版本。

**限制披露（照该 `MANIFEST.md` §6，不得省略）**：该归档与工作树内的副本**都不在版本控制之下**
（`D:\tools\AI_Work_linker` **不是** git 仓库；工作树内的 `docs/api/GAMEPLAY_V2_ABI.md` 是 untracked）。
因此"可持久取回"目前依赖**磁盘上的固定路径**，**而不是提交标识**；`%TEMP%` 下的同内容临时备份
（`cuexis-doc-bak-20261003-060406`）只是临时副本，**不得**作为唯一来源。根治办法是 owner 授权把工作树
（含本快照）纳入版本控制。

**替换计数（JSON `sourceAnchorPolicy.replacementCounts` 逐字同源）**：`codes[].source` 章节引用替换
**30** 处、`severitySources` 字面量删除 **6** 处、src-only 令牌符号名替换 **13** 处，**活锚合计 49**；
另有 **13** 处字面量只存在于策略记录自身（10 处对照列表 + 1 处保留范围列表 + 2 处 severity 注），随记录
一并撤下，**总清除 62 处**。以下三张表即这 49 处的逐条对照。

**表 1 / 30 处 `codes[].source`（旧行号锚 → 现行章节引用）**

| # | 码 | 旧行号锚（已退役） | 现行引用 |
| --- | --- | --- | --- |
| 1 | `capability.unknown` | `GAMEPLAY_V2_ABI.md#L599-L617` | `GAMEPLAY_V2_ABI.md 错误与诊断映射（诊断分层与其后各段）` |
| 2 | `capability.disabled` | `GAMEPLAY_V2_ABI.md#L599-L617` | 同上 |
| 3 | `capability.budget_insufficient` | `GAMEPLAY_V2_ABI.md#L599-L617` | 同上 |
| 4 | `capability.budget_insufficient` | `GAMEPLAY_V2_ABI.md#L362-L363` | `GAMEPLAY_V2_ABI.md 域 8 CapabilityId / CapabilityRevision 行` |
| 5 | `capability.revision_mismatch` | `GAMEPLAY_V2_ABI.md#L599-L617` | `GAMEPLAY_V2_ABI.md 错误与诊断映射（诊断分层与其后各段）` |
| 6 | `capability.revision_mismatch` | `GAMEPLAY_V2_ABI.md#L361` | `GAMEPLAY_V2_ABI.md 域 8 CapabilityRevision 行` |
| 7 | `capability.permanently_unsupported` | `GAMEPLAY_V2_ABI.md#L599-L617` | `GAMEPLAY_V2_ABI.md 错误与诊断映射（诊断分层与其后各段）` |
| 8 | `capability.future_development` | `GAMEPLAY_V2_ABI.md#L599-L617` | 同上 |
| 9 | `resource.capacity_unsupported` | `GAMEPLAY_V2_ABI.md#L599-L617` | 同上 |
| 10 | `resource.handoff_unsupported` | `GAMEPLAY_V2_ABI.md#L599-L617` | 同上 |
| 11 | `coordination.relation_unsupported` | `GAMEPLAY_V2_ABI.md#L599-L617` | 同上 |
| 12 | `solver.profile_unsupported` | `GAMEPLAY_V2_ABI.md#L599-L617` | 同上 |
| 13 | `pattern.relation_unsupported` | `GAMEPLAY_V2_ABI.md#L599-L617` | 同上 |
| 14 | `input.continuous_unsupported` | `GAMEPLAY_V2_ABI.md#L603-L609` | `GAMEPLAY_V2_ABI.md 错误与诊断映射（已登记码的逐条定案）` |
| 15 | `input.continuous_unsupported` | `GAMEPLAY_V2_ABI.md#L608-L609` | 同上 |
| 16 | `input.direction_unsupported` | `GAMEPLAY_V2_ABI.md#L603-L609` | 同上 |
| 17 | `geometry.inference_rejected` | `GAMEPLAY_V2_ABI.md#L603-L609` | 同上 |
| 18 | `fact.correction_unsupported` | `GAMEPLAY_V2_ABI.md#L599-L617` | `GAMEPLAY_V2_ABI.md 错误与诊断映射（诊断分层与其后各段）` |
| 19 | `late.reopen_unsupported` | `GAMEPLAY_V2_ABI.md#L599-L617` | 同上 |
| 20 | `format.gameplay_version_unsupported` | `GAMEPLAY_V2_ABI.md#L599-L617` | 同上 |
| 21 | `format.gameplay_version_unsupported` | `GAMEPLAY_V2_ABI.md#L633-L634` | `GAMEPLAY_V2_ABI.md 码族来源（格式与迁移族）` |
| 22 | `migration.ambiguous` | `GAMEPLAY_V2_ABI.md#L599-L617` | `GAMEPLAY_V2_ABI.md 错误与诊断映射（诊断分层与其后各段）` |
| 23 | `session.mutation_rejected` | `GAMEPLAY_V2_ABI.md#L599-L617` | 同上 |
| 24 | `ruleset.package_unsupported` | `GAMEPLAY_V2_ABI.md#L599-L617` | 同上 |
| 25 | `judgement.s7a2.timebase.tick_overflow` | `GAMEPLAY_V2_ABI.md#L517-L521` | `GAMEPLAY_V2_ABI.md 数值域与量程（负 observationTick 合法）` |
| 26 | `judgement.s7a2.timebase.time_reversal` | `GAMEPLAY_V2_ABI.md#L515-L521` | 同上 |
| 27 | `judgement.s7a2.input.amount_out_of_range` | `GAMEPLAY_V2_ABI.md#L552-L558` | `GAMEPLAY_V2_ABI.md 数值域与量程（判定顺序：可表示性先于范围）` |
| 28 | `judgement.s7a2.input.amount_narrowed` | `GAMEPLAY_V2_ABI.md#L552-L558` | 同上 |
| 29 | `presentation-target-missing` | `GAMEPLAY_V2_ABI.md#L592-L596` | `GAMEPLAY_V2_ABI.md 错误与诊断映射（第 6 轮补充）` |
| 30 | `partial-group` | `GAMEPLAY_V2_ABI.md#L592-L596` | 同上 |

**表 2 / 6 处 `severitySources` 字面量（已删除，不保留范围）**

| # | 值 | 旧行号锚（已删除） | 现行引用 |
| --- | --- | --- | --- |
| 31 | `info` | `docs/api/diagnostics-identity-and-compatibility.md#L30` | 只留文档名 `docs/api/diagnostics-identity-and-compatibility.md` |
| 32 | `info` | `docs/guides/CODE_POLICY.md#L100` | 只留文档名 `docs/guides/CODE_POLICY.md` |
| 33 | `warning` | `docs/api/diagnostics-identity-and-compatibility.md#L30` | 同上 |
| 34 | `warning` | `docs/guides/CODE_POLICY.md#L100` | 同上 |
| 35 | `error` | `docs/api/diagnostics-identity-and-compatibility.md#L30` | 同上 |
| 36 | `error` | `docs/guides/CODE_POLICY.md#L100` | 同上 |

三个 `severitySources` 值都还保留 `engine/core/include/cuexis/core/diagnostic.hpp (DiagnosticSeverity)`——
**核心枚举按符号引用，不按行**。

**表 3 / 13 处 src-only 令牌（旧行号锚 → 现行符号名引用）**

| # | 令牌（码串） | 旧行号锚（已退役） | 现行引用（`source` 字段） |
| --- | --- | --- | --- |
| 37 | `judgement.s7a2.timebase.profile_value_out_of_range` | `source_codes.hpp#L75-L76` | `engine/judgement/src/source_codes.hpp kProfileValueOutOfRangeCode token definition (…)` |
| 38 | `judgement.s7a2.timebase.rational_invalid` | `source_codes.hpp#L77-L79` | `… kRationalInvalidCode token definition (…)` |
| 39 | `judgement.s7a2.timebase.interval_reversed` | `source_codes.hpp#L80-L82` | `… kIntervalReversedCode token definition (…)` |
| 40 | `judgement.s7a2.timebase.commit_window_invalid` | `source_codes.hpp#L83-L85` | `… kCommitWindowInvalidCode token definition (…)` |
| 41 | `judgement.s7a2.timebase.commit_not_at_commit_tick` | `source_codes.hpp#L86-L88` | `… kCommitNotAtCommitTickCode token definition (…)` |
| 42 | `judgement.s7a2.late.parameter_pending` | `source_codes.hpp#L94-L95` | `… kLatePolicyPendingCode token definition (…)` |
| 43 | `judgement.s7a2.late.policy_undeclared` | `source_codes.hpp#L96-L101` | `… kLatePolicyUndeclaredCode token definition (…)` |
| 44 | `judgement.s7a2.late.duplicate_queue_entry` | `source_codes.hpp#L102-L104` | `… kDuplicateQueueCode token definition (…)` |
| 45 | `judgement.s7a2.late.queue_hop_exceeded` | `source_codes.hpp#L105-L107` | `… kQueueHopExceededCode token definition (…)` |
| 46 | `judgement.s7a2.input.ingress_sequence_duplicate` | `source_codes.hpp#L117-L120` | `… kIngressSequenceDuplicateCode token definition (…)` |
| 47 | `judgement.s7a2.timebase.profile_invalid` | `source_codes.hpp#L124-L126` | `… kProfileInvalidCode token definition (…)` |
| 48 | `judgement.s7a2.input.mapping_declaration_invalid` | `source_codes.hpp#L127-L133` | `… kInputMappingInvalidCode token definition (…)` |
| 49 | `judgement.s7a2.input.runtime_mapping_change` | `source_codes.hpp#L134-L137` | `… kRuntimeMappingChangeCode token definition (…)` |

**表 3 的实测支撑（A4-b 的完整数据）**：这 13 个旧范围**每一个都已漂移 1 行**——令牌仍在被引范围的最后一行，
范围本身随并行实现线新增注释下移。逐条"引用范围 / 令牌实际行"：75-76/76、77-79/79、80-82/82、83-85/85、
86-88/88、94-95/95、96-101/101、102-104/104、105-107/107、117-120/120、124-126/126、127-133/133、
134-137/137。**这正是 `DOCUMENTATION_POLICY.md` 新规则的实测依据（§11.8）。**

**JSON 侧现只保留**：`sourceAnchorPolicy.rule`（策略文本）、`replacementCounts`（上列计数）、
`srcOnlyTokenAnchors` / `severityAnchorNote`（说明）、`replacementReason`（裁定来源）、以及
`evidenceRecord`（四项：`path` / `section` / `holds` / `role`——**只是指针**，连版本标识都留在本节）。
**不再保留任何行号锚字面量，也不在机器表里保留任何行号**（全文 `#L` 出现次数实测 **0**）。

### 11.8 第 3 批第 2 项：`docs/DOCUMENTATION_POLICY.md` 新增规则（逐字全文）

加在 `## 链接和重复` 的既有列表末尾（**只加这一条**，该文件其余内容未动）：

> - **行号锚只能作为带日期的定版证据，不得作为活合同来源。** 活引用一律写章节名或符号名；确需引用具体行时，
>   必须写在**带日期的证据记录**里，并同时注明所依据的**文档版本或提交标识 / 快照哈希**，使该行号可被复核到
>   某一个定版。理由有实测依据：`engine/judgement/src/source_codes.hpp` 的 13 处行号锚在并行实现线新增注释后
>   **已全部漂移 1 行**（令牌仍在被引范围的最后一行），说明行号随文档增删必然失效。

**纪律遵守**：该通用规则**没有**写进 `docs/formats/GAMEPLAY_V2_SPEC.md`；Spec 侧只保留了 §9.9 的**诊断码
公开性判据**（Gameplay 专属语义，不是通用文档规则）。

### 11.9 第 3 批第 3 项：`BATCH_GATES.md` 撤下 6 行 old→new 映射表

**撤下前（逐字，S7A-1 表后）**：

```
**S7A-1 诊断类别的九类口径（2026-10-03，第 6 轮 `CM-D04` / `P1-09` 首次消费后续补齐）。** 上表的诊断类别
曾用 Gameplay I 的类别名（`identity_mismatch` / `unsupported_capability` / `invalid_requirement` /
`time_regression`）。按 Spec §9.2 九类为**唯一** category 权威与 Spec §9.9 的**公共性判据**（有已文档化的
公共映射行即公共码，前缀不决定公开性），它们与 Gameplay I 类别集一样必须并入九类；S7A-1 实际消费的
映射如下（**已逐条回原文核对**，未新增任何类别或映射行）：

| Gameplay I 类名 | 并入的 V2 类别 | 依据 |
| --- | --- | --- |
| `identity_mismatch` | `invalid_relation` | … |
| `unsupported_capability` | `capability_disabled` | … |
| `invalid_requirement` | `invalid_relation` | … |
| `time_regression` | `invalid_relation` | … |
| `budget_exceeded` | `budget_exceeded`（同名） | … |
| `session.mutation_rejected`（稳定码） | 类别 `invalid_relation` | … |

**口径说明。** 该映射是**现行文本的类别口径订正**（Gameplay I 类名 → V2 九类），**不改变**Gameplay I ABI 的
十类集合本身，也**不宣布** `CM-D02` 的完整 old→new 映射表已冻结——后者仍是
[RULING_WORKSHEET](RULING_WORKSHEET.md) 与集中码表 `ownerReviewRequired` 里唯一**保持未关闭**的项
（见 [第 6 轮诊断分类学后续补齐记录](…)）。
```

**撤下后（逐字，现行全文）**：

```
**S7A-1 诊断类别的九类口径（2026-10-03）。** 上表的诊断类别曾在 Gameplay I 时期写作 `identity_mismatch` /
`unsupported_capability` / `invalid_requirement` / `time_regression` 等旧类名；那些名字**不是** Gameplay V2
的 `category` 取值，本批次起**不再使用**。类别口径以 **Spec §9.2 九类**与 **Spec §9.3 映射表**为准；
Gameplay I 旧类名到 V2 九类的 **old→new 合并映射**由**仍未关闭的 `CM-D02`** 统一冻结，本文件**不预先发布**
任何推导映射表（`CM-D02` 的登记见 [RULING_WORKSHEET](RULING_WORKSHEET.md) 与
[CONTRACT_MATRIX](CONTRACT_MATRIX.md)；其状态见集中码表 `ownerReviewRequired`）。
```

**S7A-1 诊断类别行同步微调**（仍反映"不再使用 Gameplay I 旧类名"这一事实，**不附任何推导映射表**）：

改前：``| 诊断类别（Gameplay V2 九类；Gameplay I 的六个旧类名已按 Spec §9.2 / §9.9 并入） | `invalid_relation`、`capability_disabled`、`budget_exceeded`、`session.mutation_rejected` |``
改后：``| 诊断类别（Gameplay V2 九类；**不再使用 Gameplay I 的旧类名**，口径见下） | `invalid_relation`、`capability_disabled`、`budget_exceeded`、`session.mutation_rejected` |``

**结论**：6 行推导表**整表删除**，不再有"看着像确定的映射表 + 尚未冻结免责声明"并存的写法。

### 11.10 第 3 批第 5 项：公共承载面 vs src-only 的区分 + 校验器新检查

**JSON 侧（区分方式）**：

- `codes[]`（29 条已登记公共码）：每条 `publicConsumptionSurface` 为**非空字符串**（既有承载面描述）。
- `pendingRegistration[]`（20 条）：**每一条**都写 `publicConsumptionSurface: null`——13 条 src-only 令牌
  （`status = not_registered_src_only`）与 7 条尚未选出码串的空位（`code = null`）都不带公共承载面。
- `diagnosticStringPolicy.publicSurfaceRule` 写明上述语义，并明确"**公共码 29 / src-only 13 / 空位 7 是三个
  不同数字**，`pendingRegistration` 里出现公共承载面即为表错误"。

**校验器侧（`cmake/VerifyGameplayDiagnosticsCodes.cmake` 新增 Rule 6）**：

1. `codes[]` 每项必须存在 `publicConsumptionSurface`，JSON 类型必须是 `STRING`，且**非空**；
   缺失 / 类型不对 / 空串分别给独立消息（缺项消息会指出"公共已登记码必须写明既有承载面"）。
2. `pendingRegistration[]` 每项必须存在 `publicConsumptionSurface`，且 JSON 类型必须是 **`NULL`**；
   若为 `STRING`：当该项 `status = not_registered_src_only` 时报
   **"is a src-only token … but declares a public carrier … A src-only token must not claim a public
   consumption surface. Use JSON null"**，否则报**"Nothing in 'pendingRegistration' is public. Use JSON null"**。
3. 结束时**追加**一行 STATUS 报告切分，使 29/29 不可能被读成"全部公开"：
   `gameplay diagnostics public surface split: 29 public codes carry a publicConsumptionSurface, 20 pending entries carry none (13 of them are src-only tokens)`。
4. **原正例行逐字保留**（未改一字），退出码不变。
5. 实现要点：CMake `string(JSON … TYPE …)` 对**已存在**成员返回 `ERROR_VARIABLE=NOTFOUND`、对 **JSON null**
   返回类型 `NULL`、对**缺失**成员返回 `member '…' not found`。故"缺失"与"值为 null"必须用 **TYPE** 区分，
   不能用 `GET`（`GET` 对 null 返回空串，会与"空字符串"混淆）。该行为已用探针脚本实测确认。

### 11.11 第 4 批：通配族覆盖缺口的如实登记（`alsoObservedFamilies`）

**背景。** 第 4 批（Codex 第 3 张卡 `2026-10-02T22-11-45-270Z`，同 thread，`reject` 逐项处置，
confidence **0.82**）指出：`BATCH_GATES.md` 后续门禁行的旧类名与通配族，在集中码表里**没有完全覆盖**。
核实结果如下（逐字段回原文）：

| 登记项 | 字段级实际覆盖 | 是否覆盖 `ruleset.*` |
| --- | --- | --- |
| `pendingRegistration[16]`（`kind: code_family`） | `family: "budget.*"` | **否**（`family` 字段只到 `budget.*`） |
| `pendingRegistration[17]`（`kind: category_set`） | `family: "gameplay-i-legacy-categories"`，`names` 为 Gameplay I 十类旧名 | **否**（`ruleset.*` 不是类别名） |
| `pendingRegistration[16].reason` | 散文里**已列出** `budget.content.*` / `budget.steady.*` / `budget.*` / `ruleset.*` | 仅**散文级**，非字段级 |

**结论**：`ruleset.*` **没有任何登记项**；缺口是**字段级**的，散文里提到过但机器可读字段没记。

**所选做法：补字段（主控首选方案），并同时在本文登记缺口。** 先在判断可行性时核对了三件事：

1. **校验器是否对 `pendingRegistration[]` 断言闭集字段**？逐条读 `cmake/VerifyGameplayDiagnosticsCodes.cmake`
   的 `pendingRegistration` 相关代码（`LENGTH` / `TYPE`+`GET` on `code` / Rule 6 的 `publicConsumptionSurface`
   与 `status`）：**没有任何字段集闭包断言**，校验器只按名取它需要的字段 → **加字段无风险**。
2. **既有文档是否断言该数组的字段集**？`GAMEPLAY_V2_SPEC.md` §9.9 第 3 条附则、`GAMEPLAY_V2_ABI.md` 域 8
   只断言"每一条都写 `publicConsumptionSurface: null`"，**不枚举字段集** → 加字段不使任何文档失准。
3. **是否改变计数或需改校验器**？**不改变任何计数、未改校验器**：条目数仍 **20**，`openSlotCount` 仍 **7**，
   `srcOnly` 仍 **13**，`family: "budget.*"` **原样不动**，**未新增任何 pending 条目**。

**新增字段（JSON `pendingRegistration[16]`，逐字；纯事实，不代表已注册）**：

```json
"alsoObservedFamilies": ["budget.content.*", "budget.steady.*", "ruleset.*"]
```

**语义（必须与 `registered: false` / `kind: "code_family"` 一起读）**：`family` 仍是该条目**唯一的主覆盖族**
（`budget.*`）；`alsoObservedFamilies` 列出**同一批门禁行里同时出现、而同一条目并未以 `family` 覆盖**的通配族。
**这三个族与 `budget.*` 一样都未注册、都无码串、都不得作为 V2 `category` 取值或批次放行依据**——该字段
**不构成登记**，也**不改变** `[16]` 的未注册性质。

**为什么选补字段而不是只登记缺口**：缺口是**字段级**的，只写在报告里会让机器可读表继续在被程序化消费时
漏掉 `ruleset.*`；补一个纯事实字段能让校验器与下游脚本在**不改任何计数、不改校验器**的前提下看到完整事实。
报告与 `BATCH_GATES.md` 的注记同时保留文字版缺口说明，两边互为指针。




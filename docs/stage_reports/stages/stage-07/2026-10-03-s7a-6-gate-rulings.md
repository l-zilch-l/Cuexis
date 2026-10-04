# S7A-6 / S7A-7 准入门禁：第 6 轮（发布粒度、表现桥接与诊断）裁定落地记录

状态：dated implementation record（第 6 轮裁定的文档落地证据；不是阶段关闭、不是 owner acceptance、
不是实施证据、**不是** Judgement 实现完成声明）

更新日期：2026-10-03

上级文档：[Stage 7A 实施计划](../../../stage_plans/active/stage-07/plan.md) §S7A-7 ·
[Gameplay V2 acceptance package](../../../proposals/gameplay-v2-acceptance/README.md) ·
[未决语义分轮裁决清单](../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) §6、§8、§9 ·
[批次门禁](../../../proposals/gameplay-v2-acceptance/BATCH_GATES.md) §1

本文是**带日期的落地记录**（reports own dated evidence）：它登记 Codex 对第 6 轮（发布粒度、表现桥接与
诊断；进入 S7A-7 前）的 **15 行 / 按项计 17 项**裁定、`S7A6-R01…R12` 编号、逐条落进哪些文档的哪一节、
计数变化、S7A-7 的不得消费清单，以及本轮跑过的门禁与结果。它不复制 Spec / ABI 的合同正文，也不构成
任何实施声明；未在本文列出的门禁一律视为未运行。

> **表述纪律。** 本轮冻结的是**文档合同与门禁**。**不得**把本文或本轮写成"Judgement 实现完成"或
> "Stage 7A 完成"，也不得据本文声称 hosted / GPU / 真实设备 / 音频证据已通过。

## 1. provenance

| 卡 | thread | 模型 | verdict | confidence | 日期 | 内容 |
| --- | --- | --- | --- | --- | --- | --- |
| 第 6 轮主裁定卡（本轮 15 行 / 17 项） | `01a0fe61-3476-7882-9674-5f6b05035237` | `gpt-6-astra`（consult） | `adopt` | **0.97** | 2026-10-03 | 采纳第 6 轮全部建议：`Q-06`、`Q-19` + `CM-X01`、`Q-09` + `CM-P04`、`Q-15` + `CM-P06` / `P1-13`、`CM-P08` / `P1-11`、`CM-D04` / `P1-09`、`P1-08`、`P2-04`、`P2-05`、`P2-06`、`P2-08`、`P2-09` 全部接受，编号 `S7A6-R01…R12`；`CM-P04`、`CM-P06`、`CM-P08`、`CM-D04`、`CM-X01` 五条合同项 `open` → `accept`；每条的首次消费批次**一律 S7A-7** |

**单卡口径说明。** 本轮只有**一张**决策卡（verdict `adopt`），因此不存在"主卡未决、补卡关闭"的分工：
15 行裁定、`S7A6-R01…R12` 编号与 `open` → `accept` 计数在同一条话题内一次给出。**卡内未给出
`Rxx` → 行的逐条映射**，本文 §2.1 的映射由本 package 依据卡内 `question` 的证据清单**推导**得出，
**待 Codex 确认**。

**日期口径说明。** 卡的文件名时间戳为 2026-10-02T20:51:47.167Z（UTC），裁定与落地按**本地日期
2026-10-03** 登记（与第 2、3、4、5 轮先例一致）；本文、Spec §3.23–§3.25 / §5.6 / §9.6–§9.8 /
§S7A-7、ABI §S7A-7 与各计数登记均使用 2026-10-03。

## 2. 裁定要点

| # | 编号（推导，待 Codex 确认） | 对应行 | 裁定要点 | 落点 |
| --- | --- | --- | --- | --- |
| 1 | `S7A6-R01` | Q-06 CXC 入口 | 发布入口是**闭集**：`playback = true` 只允许 `packed-chart` / `gameplay-graph`；`author-source` **不是** Playback entry（审查 / 迁移 / 复现）；**Ruleset package 不是第三种 entry**；manifest **必须记录七项**（entry kind / compiled semantic identity / artifact identity / Ruleset binding / capability closure / resource·presentation closure / `sourceOf`）；`playback = true` **必须显式携带 `entryKind`** | Spec §3.6、§3.23 第 1 条、§S7A-7；ABI 域 8 `EntryKind` / `PlaybackEntryRef` 行、§S7A-7 |
| 2 | `S7A6-R02` | Q-19 identity 携带 + `CM-X01` | `compiledSemanticIdentity` 与**完整 capability closure** 必须**同时携带且可比较**；`CapabilityRecord` 七字段逐字段 identity 归属：`semanticKind` / `requiredFormat` / `replayImpact` / `supportedDomains` 进 semantic projection；`staticBudget` / `snapshotCost` 进 closure 与 interchange compatibility（**不进** judgement semantic hash）；`stableRejectCode` 只进诊断码表 / closure（**不进 identity**） | Spec §5.6、§6.4、§3.23 第 1 条；ABI 域 8 `CapabilityRecord` / `DerivedCapabilityClosure` 行 |
| 3 | `S7A6-R03` | Q-09 FactBinding 生命周期 + `CM-P04` | 表现桥接：`GameplayOverride` 命名层；precedence 常量为 `Initial < Behavior < Animation < GameplayOverride/FactBinding < StudioPreviewOverride`；`render.visible = false` 是该层**显式值**；adapter **只消费已提交 FactBinding**；当前有效立即应用、未来 `effectivePresentationTick` 排队；`(factId, targetId)` 幂等；seek / replay / reload **从 Fact Ledger 重建 token**；declared lifetime 到期或 reset / session replacement 时终止；**表现失败绝不回写 Fact Ledger**（无 Adapter 实现细节承诺） | Spec §3.23 第 3、4 条；ABI 域 8；CONTRACT_MATRIX §5 `CM-P04` |
| 4 | `S7A6-R04` | Q-15 发布粒度 + `CM-P06` / `P1-13` | 粒度分界：**判定闭包内** target / domain / action / table 缺失 = **prepare 原子失败**（`identity_closure_incomplete` / `invalid_relation`）；**纯表现** target 缺失 = 允许**空绑定** + 丢弃该投影事件 + 稳定诊断 `presentation-target-missing`；**不 fault session、不回写 Fact Ledger**；`aggregation` 三值 `any` / `all` / `groupCommit` 均幂等；`groupCommit` **部分提交不回滚**、记稳定 `partial-group`、进入**稳定拒绝路径**；**correction 仅影响未提交窗口** | Spec §3.23 第 2、4 条、§9.3、§9.7；ABI §错误与诊断映射 |
| 5 | `S7A6-R05` | `CM-P08` / `P1-11` | 承接 #4：**缺 Presentation 允许 headless judgement**（判定结果不因缺少 Presentation 数据而改变）；表现投影是**只读**、失败只降级不升级为判定失败；`aggregation` 的幂等与部分提交 golden 入 S7A-7 门禁 | Spec §3.23、§9.7；BATCH_GATES §S7A-7 行 |
| 6 | `S7A6-R06` | `CM-D04` / `P1-09` | 诊断**四层模型**（稳定字符串 `code` + 九类 `category` + `severity` + `faulted`）为最终模型，`source path` / `identity component` **永远只是上下文**；**集中码表**固定为 `schemas/cuexis.gameplay-diagnostics.v2.codes.json`；新增**一个 CTest 校验项**（code 唯一性、category / severity / faulted 映射完整性、`stableRejectCode` 已登记、**输入 / 几何三码不可重复扩展**）；**未登记的码字符串不得进入公共 ABI**；九类**不新增第十类** | Spec §9.1、§9.2、§9.6、§9.8、§10 表；ABI §诊断分层、§码表位置、域 8 四行、§仍待冻结 |
| 7 | `S7A6-R07` | P1-08 | ABI 与 canonical model 映射：`InputEvent.observationTime` **拆为 `observationTick`（校准会话时钟，唯一来源）+ 原始时间戳（仅溯源，不参与 canonical 排序）**；`JudgementFact.error` **有符号整数 tick 差**；`JudgementResult.eventSequence` **会话内单调序号**（不是 §3.9 第 3 条的规范总序键）；**不做隐式单位换算** | Spec §3.24、§3.7.3；ABI §单位与量程、§未决项 #3 |
| 8 | `S7A6-R08` | P2-04 | 分类层（`phase` / `category`）与结果层（`outcome` / `grade`）分离；`FactCategory` 与 phase 一一对应；`outcome` 仅 `{hit, miss}`；`grade` 可选、缺失即 absent；`localOutcome` 只用于 Hold 段分解；`reasonCode` 只解释拒绝 / 降级；**同层互斥、跨层可组合** | Spec §3.25 第 1 条 |
| 9 | `S7A6-R09` | P2-05 | `action` / `requiredAction` / `domain`（`InputDomain`）三个**独立字段**，不互相推导、不互相替代 | Spec §3.25 第 2 条 |
| 10 | `S7A6-R10` | P2-06 | `resources` **三分拆**：`resourceRef`（引用）/ `claimPolicy`（申请策略）/ resource record（全局定义）分别归属不同层；资源状态**沿用 S7A-4 的 `free` / `held` / `terminal` 子集**，**不新增资源状态** | Spec §3.25 第 3 条、§3.10 |
| 11 | `S7A6-R11` | P2-08 | **只有 Fact Ledger 进 Replay / Snapshot**；Effect Graph 与 `RuleEffectEvent` 属规则集投影、`PresentationEvent` 属表现投影，三类投影事件都不进；`EffectEvent` 只是投影内部名，不是可序列化对外类型 | Spec §3.25 第 4 条、§3.18；ABI 域 9 |
| 12 | `S7A6-R12` | P2-09 | 语义字段**未知即拒绝**；`extensions` **只允许命名空间化**的 inspection metadata 且**必须证明不影响判定**；**输入 / 几何三码不可重复扩展**（`input.continuous_unsupported` / `input.direction_unsupported` / `geometry.inference_rejected` 各只允许一条） | Spec §3.25 第 5 条、§9.6 第 2 条；ABI §码表位置 |

### 2.1 `S7A6-Rxx` ↔ 15 行映射（**推导，待 Codex 确认**）

**卡内只给出证据清单，未指定 `Rxx` 与行的对应。** 下表由本 package 依据卡内 `question` 的 15 行证据
顺序**推导**：

| 编号 | 行 |
| --- | --- |
| `S7A6-R01` | Q-06 |
| `S7A6-R02` | Q-19 + `CM-X01` |
| `S7A6-R03` | Q-09 + `CM-P04` |
| `S7A6-R04` | Q-15 + `CM-P06` / `P1-13` |
| `S7A6-R05` | `CM-P08` / `P1-11` |
| `S7A6-R06` | `CM-D04` / `P1-09` |
| `S7A6-R07` | P1-08 |
| `S7A6-R08` | P2-04 |
| `S7A6-R09` | P2-05 |
| `S7A6-R10` | P2-06 |
| `S7A6-R11` | P2-08 |
| `S7A6-R12` | P2-09 |

**`P2-03` 无操作。** 卡内 15 行证据清单**不含 `P2-03`**，卡文亦从未提及；`P2-03` 已在**第 2 轮**
（thread `s7a1-freeze-bindings`）裁定并登记，本轮**不重新裁定、不改其处置词、不分配 `S7A6-Rxx`
编号**。

**行 / 项口径。** 本轮表内为 **15 行**、按项计数为 **17 项**：12 个单编号行 + 3 个每行含两个编号的
合并行（`Q-19` + `CM-X01`、`Q-15` + `CM-P06` / `P1-13`… 计 2 项、`CM-P08` / `P1-11` 计 2 项、
`CM-D04` / `P1-09` 计 2 项），即 12 + 3 行、12 + 5 = 17 项。该口径与工作表 §0.1 的"行 = 实项 + 每合并行
1、项 = 实项 + 每合并行 2"约定一致。**该约定本身与工作表 §6 登记的推导过程一并提交 Codex / owner
复核**（见 §10.4）。

### 2.2 登记口径（门禁归属）

- **阻塞 S7A-7 的门禁**：`S7A6-R01…R12` 全部 15 行 / 17 项，**首次消费批次一律 S7A-7**；含
  `CM-P04`、`CM-P06`、`CM-P08`、`CM-D04`、`CM-X01` 五条由 `open` 转 `accept` 的合同项。
- **只阻塞首次消费它们的后续批次**（**不是** S7A-7 门禁）：诊断码表的**逐条内容**（含 `severity`
  取值域与逐码 `faulted` 判定，由并行工具线随 `schemas/cuexis.gameplay-diagnostics.v2.codes.json` +
  CTest 校验项落地）；manifest / Packed header 的**逐字段物理编码**与**预算数值**（后续序列化批次 /
  **S7A-9**）；`stableRejectCode` 与 `CapabilityRecord` 的 **ABI 类型行表示**（对应 ABI 实现批次）；
  `cuexis.ruleset` package 形状 / identity / 迁移（**S7C-2**）。
- **不新增编号**：本轮**不新增 R 编号**（§7.2 仍 **19 条**）、**不新增诊断类别**（§9.2 仍**九类**）、
  **不新增 §未决项 条目**、**不新增 ABI 类型行**（9 域 / **126 = 96 + 30** 不变）。
- **`P2-06` 的资源三分法沿用 S7A-4 已冻结的 `free` / `held` / `terminal` 子集**，不新增资源状态。

## 3. 计数变化

| 计数 | 第 5 轮后 | 第 6 轮后 | 说明 |
| --- | --- | --- | --- |
| `CONTRACT_MATRIX` 总项数 | 114 | **114** | 不变 |
| `accept` | 42 | **47** | +5（`CM-P04`、`CM-P06`、`CM-P08`、`CM-D04`、`CM-X01`） |
| `revise` | 26 | **26** | 不变 |
| `supersede` | 17 | **17** | 不变 |
| `retain` | 17 | **17** | 不变 |
| `open` | 10 | **5** | −5；余下 `CM-V06`、`CM-V07`、`CM-V13`、`CM-I06`、`CM-X06`（全属第 1 轮） |
| `reject` | 2 | **2** | 不变 |
| `RULING_WORKSHEET` §0.1 合计 | 76 行 / 84 项 | **76 行 / 84 项** | 不变；末列仍 **21** |
| `RULING_WORKSHEET` §8 `open` 分布 | 第 1 轮 5、其余 0 | **第 1 轮 5、第 2–6 轮 0** | 本轮第 6 轮分布为 **0** |
| Spec §7.2 稳定拒绝清单 | 19 | **19** | 不新增 R 条目 |
| Spec §9.2 诊断类别 | 九类 | **九类** | 不新增第十类 |
| ABI 追踪目录 | 9 域 / 126 = 96 + 30 | **9 域 / 126 = 96 + 30** | 不新增类型行 |
| ABI §未决项 条目数 | 20 | **20** | 不新增编号；只更新既有 #3 文字 |

**§0.1 的"当前仍 `open`"与末列 21 的关系。** 末列的语义是"该轮一并关闭的 `open` 合同项"，
**不是**"当前仍 `open`"。第 2 轮三条、第 3 轮两条、第 4 轮两条、第 5 轮四条与第 6 轮五条已合计闭合
**16** 条，故 21 → **5**；§0.1 合计行仍写作 **76 行 / 84 项 / 末列 21**（历史口径不变）。

**第 6 轮登记后的 `open` 集合闭合核。** 当前 `open` 集合 = `CM-V06`、`CM-V07`、`CM-V13`、`CM-I06`、
`CM-X06`，共 **5** 条，全部属第 1 轮；其处置词与 §0.1 / §8 的自相矛盾**不在本轮范围**，由**第 7 轮**
单独裁定。

## 4. 发布粒度与表现桥接（`S7A6-R01` / `S7A6-R03` / `S7A6-R04` / `S7A6-R05`）

### 4.1 入口闭集与 manifest 七项

`playback = true` 只允许 `packed-chart` 与 `gameplay-graph`；`author-source` 只能用于审查 / 迁移 /
复现（**不是** Playback entry）；Ruleset package 是 prepare 输入且 7A 遇 package 输入稳定拒绝（第 5 轮
`S7A5-R05` 的内置 Ruleset 决定），**不是第三种 Playback entry**。manifest 必须记录七项：entry kind /
compiled semantic identity / artifact identity / Ruleset binding / capability closure /
resource·presentation closure / `sourceOf`。**字段清单冻结，物理编码与预算数值不冻结。**

### 4.2 precedence 与 adapter 生命周期

```text
Initial < Behavior < Animation < GameplayOverride/FactBinding < StudioPreviewOverride
```

`render.visible = false` 是 `GameplayOverride` 层的**显式值**而非"缺少覆盖"；`GameplayOverride` 与
`FactBinding` 在 precedence 中**同一层**。adapter 只消费**已提交** FactBinding；当前有效立即应用、
未来在 `effectivePresentationTick` 排队；去重键 `(factId, targetId)` 幂等；seek / replay / reload
**一律从 Fact Ledger 重建 token**；declared lifetime 到期或 reset / session replacement 时终止；
**表现失败绝不回写 Fact Ledger**。

### 4.3 判定闭包 vs 表现投影

| 情形 | 处置 | 是否 `faulted` | 是否改 Fact Ledger |
| --- | --- | --- | --- |
| 判定闭包内 target / domain / action / table 缺失 | prepare **原子失败** | 否 | 否 |
| `compiledSemanticIdentity` 与 closure 缺一或不可比较 | prepare **原子失败** | 否 | 否 |
| **纯表现** target 缺失 | 空绑定 + 丢弃投影事件 + `presentation-target-missing` | **否** | **否** |
| `groupCommit` 部分提交 | 已提交部分保留 + `partial-group` + 稳定拒绝路径 | 否 | 否（correction 仅影响未提交窗口） |

**缺 Presentation 允许 headless judgement**：同一内容在 headless 与渲染路径下的判定必须逐项一致。

## 5. 诊断（`S7A6-R06`）

四层模型为**最终模型**：稳定字符串 `code` + `category`（九类）+ `severity` + `faulted` 行为；
`source path` 与 `identity component` 永远只是上下文，不得升级为稳定码。集中码表固定为
**`schemas/cuexis.gameplay-diagnostics.v2.codes.json`**（该文件由**并行工具线**创建，本轮**不创建、
不修改**），并新增**一个 CTest 校验项**（候选名 `cuexis_gameplay_diagnostics_codes`，正式名以工具侧
CMake 注册为准；本轮**不创建**该 CTest 项），校验：

1. `code` 唯一性；
2. `category` / `severity` / `faulted` 映射完整性（每条码映射到九类之一，并具备 severity 与明确的
   `faulted` 行为）；
3. `CapabilityRecord.stableRejectCode` 已登记；
4. **输入 / 几何三码不可重复扩展**——`input.continuous_unsupported`、`input.direction_unsupported`、
   `geometry.inference_rejected` 各只允许一条，不得新增同义码或别名。

**除已冻结的三码与既有 R-09 映射外**，诊断码字符串必须先登记并通过该 CTest 校验才能进入公共 ABI。
本轮登记语义的三条码：`identity_closure_incomplete`（映射既有九类同名类）、
`presentation-target-missing`（映射 `invalid_relation`、**不 fault**）、`partial-group`（映射
`invalid_relation`、**不 fault**）。**逐条码表内容与 `severity` 取值域仍属工具侧落地与后续批次。**

## 6. ABI 映射与 `resources` 三分拆（`S7A6-R07`…`S7A6-R12`）

- **P1-08**：`observationTime` 拆为 `observationTick`（唯一来自校准会话时钟）+ 原始时间戳（仅溯源）；
  `error` 为有符号整数 tick 差；`eventSequence` 为会话内单调序号；**禁止隐式单位换算**。
- **P2-04**：分类层（`phase` / `category`）与结果层（`outcome` / `grade`）分离；`outcome` 仅
  `{hit, miss}`；`grade` 缺失即 absent；同层互斥、跨层可组合。
- **P2-05**：`action` / `requiredAction` / `domain` 三独立字段，不互相推导。
- **P2-06**：`resources` 三分拆 `resourceRef` / `claimPolicy` / resource record；资源状态沿用
  `free` / `held` / `terminal`，不新增状态。
- **P2-08**：只有 Fact Ledger 进 Replay / Snapshot；`RuleEffectEvent` / `PresentationEvent` /
  `EffectEvent` 三类投影事件都不进。
- **P2-09**：语义字段未知即拒绝；`extensions` 仅命名空间化且须证明不影响判定；输入 / 几何三码不可
  重复扩展。

## 7. S7A-7 不得消费清单

S7A-7 **不得**消费（表示与行为，**11 项**）：

1. 具体预算数值；
2. `maxSeekLatency` 具体数值；
3. 未冻结的 wire / codec 字节编码；
4. 未冻结的 `FactId` / `CommitId` 字节编码；
5. Ruleset package 形状 / identity / 迁移；
6. Presentation 缓存或 HostOverride token 的 snapshot 持久化；
7. 未提交 delta；
8. 连续采样相位；
9. 运行时脚本 / 逐帧回调；
10. Life capability 或任何第二判定路径；
11. 把语义字段的"未知"降级为默认值。

该清单与第 2 / 4 / 5 轮清单**不冲突**（不重复、不放松），已同步写入
[BATCH_GATES.md](../../../proposals/gameplay-v2-acceptance/BATCH_GATES.md) §"第 6 轮裁定后的 S7A-7
门禁"、[Spec](../../../formats/GAMEPLAY_V2_SPEC.md) `## S7A-7` 节与
[ABI](../../../api/GAMEPLAY_V2_ABI.md) `## S7A-7` 节。

## 8. 改动的文档与落点

| 文档 | 落点 |
| --- | --- |
| [ADR 0044](../../../adr/0044-gameplay-v2-semantic-kernel.md) | "仍待裁定的部分（第 7 轮）"标题；第 6 轮段（15 行 / 17 项、`S7A6-R01…R12`）；第 7 轮剩余量重算 **16 行 / 17 项**（`76 − (14+6+6+6+13+15) = 16`、`84 − (15+7+7+6+14+17) = 17`）；第 6 轮表行标为**已裁定（Codex 2026-10-03）**；第 7 轮行标为**后续工作项**；§证据边界追加第 6 轮卡 |
| [RULING_WORKSHEET.md](../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) | 抬头 `10 项 open` → `5 项`；§0.1 合计行的"当前仍 `open` 者 **5**"与末列口径段；§6 的 15 行全部填入**接受** + `S7A6-Rxx` + 首次消费 `S7A-7` + 落点 + provenance；§6 之后的第 6 轮登记块（含 `Rxx` ↔ 行映射表并标注**待 Codex 确认**、`P2-03` 无操作声明）；§8 `open` 行 → **5**、分布与叙述；§9 第 6 行完整填入、第 7 行仍 `待填` |
| [CONTRACT_MATRIX.md](../../../proposals/gameplay-v2-acceptance/CONTRACT_MATRIX.md) | §0 统计 `accept` **47** / `open` **5**；§0 第 6 轮变化段；§5 五行（`CM-P04` / `CM-P06` / `CM-P08` / `CM-D04` / `CM-X01`）`open` → `accept` + 裁定正文 + 落点 + provenance；§5 末尾"第 6 轮对本节的处置"段；§14 首条、S7A-7 门禁行与计数变化段。**五条 `open` 集合行（`CM-V06` / `CM-V07` / `CM-V13` / `CM-I06` / `CM-X06`）未动** |
| [OPEN_QUESTIONS.md](../../../proposals/gameplay-v2-acceptance/OPEN_QUESTIONS.md) | 抬头更新日期；新增 **§4.4 第 6 轮裁定登记（2026-10-03）**（provenance + 12 行 `S7A6-Rxx` 表 + 推导映射说明 + `P2-03` 无操作 + 计数变化）；`Q-06` / `Q-09` / `Q-15` / `Q-19` 行追加指针（保留 2026-10-02 候选处置文本）；§6 批次清单标注（`第六批` 澄清轮次、`第七批` 标为已裁定、新增 `第八批（进入 S7A-8 前）`） |
| [acceptance README.md](../../../proposals/gameplay-v2-acceptance/README.md) | §1 工作表行的 `5 项 open` 与登记范围 `§2 / §3 / §4 / §5 / §6 / §9`；§2 A4 → **第 1–6 轮已裁定、第 7 轮待裁定**；§4.3 新增第 6 轮段（15 行 / 17 项、五条 `open` → `accept`、`open` 10 → 5、`accept` 42 → 47、总数 114、只冻结文档合同与门禁） |
| [BATCH_GATES.md](../../../proposals/gameplay-v2-acceptance/BATCH_GATES.md) | §S7A-7 行扩写（正例 / 负例 / 诊断类别 / golden / 命令含码表 CTest 项 / 停止条件六条）；新增 **§"第 6 轮裁定后的 S7A-7 门禁（2026-10-03）"**（provenance、批次表、11 项不得消费清单、门禁归属）；§S7A-6 停止条件关于 `CM-P04` 的表述改为**已随第 6 轮闭合（`S7A6-R03`）** |
| [GAMEPLAY_V2_SPEC.md](../../../formats/GAMEPLAY_V2_SPEC.md) | §1.1 第 2 项第 6 轮标为已裁定；§3.6 三行（Graph entry / `author-source` / Ruleset package）加第 6 轮结论 + manifest 七项段；新增 **§3.23**（发布粒度 / 表现桥接 / 聚合）、**§3.24**（`P1-08` 映射）、**§3.25**（`P2-04`…`P2-09`）；§5.2 的 derived closure 行、新增 **§5.6**（`CapabilityRecord` 逐字段 identity 归属）；§6.4 改标第 6 轮并冻结字段集与归属；§9.1 / §9.2 加第 6 轮冻结段、§9.3 补判定闭包 vs 纯表现分界、§9.5 更新、新增 **§9.6 / §9.7 / §9.8**；§10 表 diagnostics 行与 §11.1 第 3 条；§11 引言与 §11 轮次表第 6 / 第 7 行；新增 **`## S7A-7 限定冻结范围与登记规则（2026-10-03）`**；§12 索引行。**§7.2 仍 19 条** |
| [GAMEPLAY_V2_ABI.md](../../../api/GAMEPLAY_V2_ABI.md) | §类型清单 域 8 的 9 行语义文本（`CapabilityRecord` / `CapabilityState` / `DerivedCapabilityClosure` / `EntryKind` / `PlaybackEntryRef` / `JudgementError` / `DiagnosticCode` / `DiagnosticCategory` / `DiagnosticSeverity`；**其中 `CapabilityRecord` / `DiagnosticCode` / `DiagnosticCategory` / `DiagnosticSeverity` 四行首词仍为 `待冻结`，其余五行首词保持 `冻结目标`**）；§错误与诊断映射的 §诊断分层与 §码表位置改写、§码族来源保持；§稳定与非稳定声明 的 §仍待冻结 诊断码表行；§S7A-1 范围语句；§未决项 引言补第 6 轮段与"不新增条目"说明、#3 行补第 6 轮例外；新增 **`## S7A-7 限定冻结范围与登记规则（2026-10-03）`**（含 **9 域 / 126 = 96 + 30 不变** 声明）。**9 域 / 126 = 96 + 30、§未决项条目数均不变** |
| [plan.md](../../../stage_plans/active/stage-07/plan.md) | §1.1 第 3 条 → 第 7 轮（第 2–6 轮已裁定）；§S7A-7 的"验证"段补一句码表 CTest 项与不得消费清单（**不新增工作项**） |
| [CURRENT_STATUS.md](../../../CURRENT_STATUS.md) | L241 `第 6-7 轮` → `第 7 轮`、`第 2、3、4、5 轮已裁定` → `第 2、3、4、5、6 轮已裁定` |
| [reading-order.md](../../../guides/reading-order.md) | L61 同上两处替换 |
| [stage_reports README.md](../../README.md) | Stage 7A 区块新增本文索引行 |
| 本文 | 第 6 轮裁定的带日期落地记录 |

**未创建的工件（明确归属其他线）。** `schemas/cuexis.gameplay-diagnostics.v2.codes.json` 与配套
CTest 校验项由**并行工具线**创建；`engine/judgement/**`（S7A-2 相关）由**并行实现线**修改。本轮
**只改文档**，**未创建、未修改** `schemas/**`、`tools/**`、`tests/**`、`cmake/**`、根
`CMakeLists.txt` 与 `engine/**`。

## 9. 跑过的门禁与结果

| 门禁 | 命令 | 结果 |
| --- | --- | --- |
| 文档检查 | `python -B tools/check_docs.py` | 首次运行因本文尚不存在而报 10 处 broken link（全部指向本文）；创建本文后重跑，见 §9.1 |
| 空白 / Tab 扫描 | PowerShell 对全部改动 / 新建文件 | 0 命中，见 §9.2 |
| 补丁空白检查 | `git diff --check` | exit 0，见 §9.3 |
| 计数脚本复核 | PowerShell 直读文档计数 | 见 §9.4 |
| 陈旧值 grep | PowerShell 全仓 `docs/**` 扫描 | 见 §9.5 |

### 9.1 `check_docs.py` 实际输出

```text
$ python -B tools/check_docs.py
Documentation checks passed.
exit=0
```

创建本文**之前**该命令失败，报出 **10 处** broken relative link，**全部**指向
`docs/stage_reports/stages/stage-07/2026-10-03-s7a-6-gate-rulings.md`；写入本文后重跑即通过。这同时
证明本轮在各文档中登记的 10 条指向本文的链接路径均已正确写入。

### 9.2 空白 / Tab 扫描

对 ADR 0044、RULING_WORKSHEET、CONTRACT_MATRIX、OPEN_QUESTIONS、acceptance README、BATCH_GATES、
Spec、ABI、plan.md、CURRENT_STATUS、reading-order、stage_reports README 与本文逐行扫描行尾空白与
Tab 字符：**0 命中**。

### 9.3 `git diff --check`

对工作树运行 `git diff --check`：**exit 0**（无 whitespace error，无冲突标记）。

### 9.4 计数脚本直读复核

| 复核项 | 目标 | 实测 |
| --- | --- | --- |
| CONTRACT_MATRIX 六类处置词 | `accept` 47 / `revise` 26 / `supersede` 17 / `retain` 17 / `open` 5 / `reject` 2 = 114 | **一致** |
| `open` 集合 | `CM-V06` / `CM-V07` / `CM-V13` / `CM-I06` / `CM-X06` | **一致** |
| Spec §7.2 稳定拒绝行数（ID 形如 `` `R-nn` ``） | 19 | **19** |
| Spec §9.2 类别数 | 九类 | **9** |
| Spec §0.1 合计 | 76 行 / 84 项 / 末列 21 | **一致** |
| Spec §8 `open` | 5（第 1 轮 5、第 2–6 轮 0） | **一致** |
| Spec §9 第 6 行 | 已填（接受 + `S7A6-R01…R12`） | **一致** |
| ABI 追踪目录 | 9 域 / 126 = 96 + 30 | **一致** |
| ABI §未决项 条目数 | 20 | **20** |

**脚本直读的已知差异（如实登记、不静默修正）。** 按"行 = 实项 + 每合并行 1、项 = 实项 + 每合并行 2"
的约定，逐轮脚本复核给出：第 1 轮 14 行 / 15 项、第 2 轮 6 行 / 7 项、第 3 轮 6 行 / 7 项、第 4 轮
6 行 / 6 项、第 5 轮 13 行 / 14 项、第 6 轮 **15 行 / 17 项**、第 7 轮 16 行 / 17 项；合计为
**76 行 / 83 项**，与工作表 §0.1 绑定的 **76 行 / 84 项**相差 **1 项**。该差异**不由本轮引入**，也
**不在本轮修正**：本轮只把第 6 轮自己的 **15 行 / 17 项**登记录入，并**如实向 Codex / owner 报告**
该 1 项口径差（见 §10.4）。工作表 §0.1 合计行**保持** 76 行 / 84 项 / 末列 21 不变。

### 9.5 陈旧值 grep 与残余命中（逐条解释）

| 模式 | 目标 | 实测 | 逐条解释 |
| --- | --- | --- | --- |
| `第 6 至第 7 轮` / `第 6-7 轮` | 0（live 文档） | **0**（`docs/**` 除历史报告外） | Spec / ABI / plan / ADR / CURRENT_STATUS / reading-order 均已改为 `第 7 轮` 或 `第 6、7 轮`（后者表示"第 6、7 轮均未裁定"，但第 6 轮现已裁定，正文相应段落已注明；残留 6 处均在**历史报告** `2026-10-03-s7a-5-6-gate-rulings.md`，属历史证据，按约定不改） |
| `第 5-7 轮` | 0 | **0** | 上一轮已改 |
| `10 项 \`open\`` | 0（live 文档） | **0**（除历史报告 `2026-10-03-s7a-5-6-gate-rulings.md`） | 该报告是第 5 轮历史证据，保留原样 |
| `` `open` 10 `` / `` `open` **10** `` | 仅历史 / 版本叙述 | ABI:950、Spec:1837、BATCH_GATES:184/196、OPEN_QUESTIONS:186、RULING_WORKSHEET:533/568 | **均为第 5 轮的 provenance 段落**，描述的是"第 5 轮确认卡把 `open` 降为 10"这一**当轮事实**；本轮在第 5 轮段落之外另加第 6 轮段落说明 `open` 10 → **5**，因此该数值在文本中是对第 5 轮状态的准确引述，**不构成陈旧值**。其余命中的 `2026-10-03-s7a-2-input-implementation-rulings.md:154` 与 `2026-10-03-s7a-5-6-gate-rulings.md` 多处均为历史报告 |
| `CM-P04`（S7A-6 停止条件） | 不再表述为"仍未决" | **0** | BATCH_GATES §S7A-6 停止条件已改为"**亦已随第 6 轮裁定闭合（2026-10-03，`S7A6-R03`）**" |
| `36 行 / 35 项`（ADR 旧数） | 0 | **0** | ADR 第 2 项已重算为 16 行 / 17 项 |
| `77 行 / 85 项`（ADR 旧数） | 0 | **0** | 同上 |

## 10. 解释性判断与需 Codex / owner 复核的项

### 10.1 本轮冻结的边界（解释性判断）

本轮的裁定内容**只**涉及：文档合同（入口闭集、manifest 字段清单、identity 归属、四层诊断模型、
`P2-04`…`P2-09` 的分类 / 字段 / 拆分 / 归属）与**门禁**（S7A-7 的正例 / 负例 / golden / 停止条件、
码表 CTest 校验项的**存在与校验内容**）。**不涉及**任何实现完成度、任何数值承诺、任何字节编码。
因此本轮**不构成**"Judgement 实现完成"或"Stage 7A 完成"。

### 10.2 需 Codex 复核：`S7A6-Rxx` ↔ 行的映射

卡内**未指定** `Rxx` 与 15 行的对应关系；§2.1 的映射由本 package 依卡内证据清单顺序**推导**。请 Codex
确认或更正该映射。已同步在 RULING_WORKSHEET §6 登记块、OPEN_QUESTIONS §4.4、acceptance README
（§4.3 未标"推导"字样，工作表与 OPEN_QUESTIONS 已标注）与本文 §2.1 标注为**推导 / 待确认**。

### 10.3 需 Codex 复核：行 / 项口径（15 行 vs 17 项）

卡内 理由 段出现"上述 **15 项**"的措辞，而本轮证据清单按项计数为 **17 项**（含 3 个合并行、共 5 个
额外编号）。本文按工作表约定登记为 **15 行 / 17 项**。请 Codex 确认：
（a）"15 项"是否指**行数**（即 15 行内 17 个编号）；
（b）合并行的"行 / 项"折算约定是否以工作表 §0.1 为准。

### 10.4 需 Codex / owner 复核：global 1 项口径差

如 §9.4 所述，按工作表约定脚本直读 round 1–7 合计为 **76 行 / 83 项**，与工作表 §0.1 绑定的
**76 行 / 84 项**相差 **1 项**。可能来源是该约定对某个合并行的折算与工作表登记不一致（物理逐表计数
时第 5 轮为 13 行，而工作表口径记为 14 项等）。本轮**不动** §0.1 的合计行，**只如实报告**，请
Codex / owner 在**第 7 轮**一并裁定 §0.1 / §8 的自相矛盾与该 1 项差。

### 10.5 需 owner 复核：ADR 第 7 轮缺陷行的既有措辞

ADR 0044 的第 7 轮缺陷行（`D-2` / `D-7`、`D-13` / `D-14` 附近）仍保留早前轮次写下的
"已由…裁定/落地"措辞；本轮**未改动**该措辞（它属第 7 轮范围）。请 owner / Codex 在第 7 轮一并处置。

## 11. 相关索引

- [Stage 7A 实施计划](../../../stage_plans/active/stage-07/plan.md)：批次、依赖与关闭标准
- [未决语义分轮裁决清单](../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md)：§6 第 6 轮登记块、§8、§9 第 6 行
- [S7A-0.2 合同逐项台账](../../../proposals/gameplay-v2-acceptance/CONTRACT_MATRIX.md)：114 项与 **5 项 `open`**
- [未决问题清单](../../../proposals/gameplay-v2-acceptance/OPEN_QUESTIONS.md)：§4.4 第 6 轮裁定登记
- [批次门禁](../../../proposals/gameplay-v2-acceptance/BATCH_GATES.md)：§"第 6 轮裁定后的 S7A-7 门禁（2026-10-03）"
- [Gameplay V2 Spec](../../../formats/GAMEPLAY_V2_SPEC.md)：§3.23–§3.25、§5.6、§9.6–§9.8、`## S7A-7`
- [Gameplay V2 ABI](../../../api/GAMEPLAY_V2_ABI.md)：域 8 语义文本、`## S7A-7`
- [ADR 0044](../../../adr/0044-gameplay-v2-semantic-kernel.md)：第 6 轮已裁定、第 7 轮剩余量
- [第 5 轮门禁报告](2026-10-03-s7a-5-6-gate-rulings.md)：上一轮的同类证据（S7A-5 / S7A-6）
- [第 4 轮门禁报告](2026-10-03-s7a-4-gate-rulings.md)：本报告的体例先例

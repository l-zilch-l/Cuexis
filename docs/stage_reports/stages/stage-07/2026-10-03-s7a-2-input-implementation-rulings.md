# S7A-2 输入规范化半批实现裁定落地记录（第 2 轮输入半批复核）

状态：dated implementation record（S7A-2 输入规范化半批的语义裁定与文档落地证据；不是阶段关闭、
不是 owner acceptance、不是实施验证、**不是发布或提交声明**）

更新日期：2026-10-03

上级文档：[Stage 7A 实施计划](../../../stage_plans/active/stage-07/plan.md) §S7A-2 ·
[Gameplay V2 acceptance package](../../../proposals/gameplay-v2-acceptance/README.md) ·
[未决语义分轮裁决清单](../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) §2、§9

本文是**带日期的落地记录**（reports own dated evidence）：它登记 S7A-2 **输入规范化半批**实现落地后，
Codex 对实现中语义裁量的裁定与补充、由此对**第 2 轮已落地语义**（输入侧的身份、时间域与越界处置）的
**修订 / 补充**、修订后的落点，以及本轮跑过的门禁与结果。它不复制 Spec / ABI 的合同正文，也不构成任何
实施声明；未在本文列出的门禁一律视为未运行。第 2 轮本身的裁定落地证据仍以
[第 2 轮门禁报告](2026-10-03-s7a-2-gate-rulings.md)为准，时基半批的复核以
[第 2 轮实现裁定记录](2026-10-03-s7a-2-implementation-rulings.md)为准，本文**不回改**这两份报告。

## 1. provenance

两张卡同属 Codex 话题 thread `s7a1-freeze-bindings`（与第 2 轮主裁定同 thread，`gpt-6-astra`，consult）：

| 卡 | 文件名时间戳（UTC） | verdict | confidence | 内容 | 登记日期 |
| --- | --- | --- | --- | --- | --- |
| ①6 处语义裁量卡 | 2026-10-02T20:42:29.114Z | `adopt`（含修改） | **0.97** | `AmountSpec` canonical 宽度、`same_tick_collision` 类别、不连续表示复用既有码、"窄化先于越界"、负 tick 与回退类别、域声明进 session identity | 2026-10-03（本地） |
| ②两处死令牌与措辞对齐卡 | 2026-10-02T20:47:58.460Z | `adopt` | **0.99** | 删除两个无引用令牌、clock 不可表示统一复用 tick 溢出码、7A 不引入 per-domain 版本字段、64 位原文同步写入 Spec / ABI 类型行 / `CM-T13` | 2026-10-03（本地） |

**日期口径说明。** 两张卡的文件名时间戳为 2026-10-02T20:4xZ（UTC），按第 2、3、4、5 轮先例统一按
**本地日期 2026-10-03** 登记。

**thread 口径说明。** 两卡沿用第 2 轮的 thread `s7a1-freeze-bindings`，因此本文不另立新话题；这也意味着
**第 2 轮的已落地输入语义在实现复核后被同 thread 修订 / 补充**，修订属第 2 轮范围，不构成第 6 轮或新裁定轮。
两卡与 `2026-10-03-s7a-2-implementation-rulings.md` 的两卡**同为第 2 轮实现复核**、互为补充：前者复核时基
半批（`stop` 模型、tick 宽度、late-policy 门禁），本文复核输入半批（`AmountSpec` 宽度、域声明身份、
时间域行为与拒绝类别）。

## 2. 六处语义裁量裁定（卡 ①）

| # | 裁量点 | 裁定结论 | 落点 |
| --- | --- | --- | --- |
| 1 | `AmountSpec` 的 canonical 整数宽度 | **明文冻结为有符号 64 位整数**（与 Tick 的有符号 64 位域一致）；**不得**靠"复用 Tick 宽度"默示；任何无法在该域内精确表示的量化、乘法或窄化稳定拒绝 | Spec §3.7.6 第 5 条；ABI §数值域与量程、域 2 `DomainAmount` 行；`CONTRACT_MATRIX` 的 `CM-T13`（**该编号不是本台账的行**，按 §7B 登记的裁决项处理）与 `RULING_WORKSHEET` §2 |
| 2 | `same_tick_collision` 的类别 | 归 **`invalid_relation`**（输入身份 / 次序关系非法），**不归** `late_policy_incomplete`；与迟到策略的**重复排队**分开登记 | Spec §3.7.8 表、§9.3 第 1 条；ABI §S7A-2 节 ④ 行 |
| 3 | 不连续表示复用既有码 | **维持复用**既有 ABI 冻结码 `input.continuous_unsupported`（类别 `capability`，`capabilityId = input.trajectory.v1`，`remediation = S7B-1`），靠 `field.path`（`continuityCapability` vs `discontinuity`）区分，映射既有 **R-05**；**不为 discontinuity 另造码** | Spec §3.7.7 表、§3.7.8、§7.2；`CONTRACT_MATRIX` 的 `CM-T10`；ABI §S7A-2 节 ④ 行 |
| 4 | "窄化"先于"越界" | **维持**：先验 canonical 可表示性，再验声明范围与边界策略；可表示但越界 → `amount_out_of_range`；根本不可表示 → `amount_narrowed`；`exactNumerator == INT64_MIN` 因 `\|INT64_MIN\|` 无有符号 64 位表示而按"不可表示"拒绝 | Spec §3.7.6 第 6 条、§9.3 第 2 条；ABI §数值域与量程 |
| 5 | 负值校准时钟 | **接受**：有符号 64 位域内负 `observationTick` **合法**并参与单调排序；**只有校准会话时钟回退被拒**，属**时间次序关系错误** → `invalid_relation`（不是 `budget_exceeded`） | Spec §3.7.3 第 4 条、§9.3 第 3 条；ABI §时间单位与表达、§稳定与非稳定声明 |
| 6 | 域声明与 session identity | **拒绝**"域声明表不进 session identity"的实现：域 token 集合与每项域声明的完整 `AmountSpec` 字段**必须进** session identity；**域重排不算变化**；**不得**推迟到 S7A-4 | Spec §5.2 行、§3.7.6；ABI 域 2 `InputMapping`、域 6 `NormalizationProfile`、§数值域与量程；`CONTRACT_MATRIX` 的 `CM-T09`；`RULING_WORKSHEET` §2、§9 |

**另两问的裁定（卡 ① 第 7、8 问，未列入上表六项）：** 本轮**需要**同步 Spec / ABI，并更新 `CM-T09` /
`CM-T10` 的未决点文字（`CM-T13` 为非合同项、按 §7B 与 `OPEN_QUESTIONS` §4.3 承载）；**不新增 R 条目**
（§7.2 仍 19 条），§9.3 的三分法**补写三条映射**而**不新增第十类**。

## 3. A–E 落点表

| 编号 | 裁定内容 | Spec 落点 | ABI 落点 | 台账 / 工作表落点 |
| --- | --- | --- | --- | --- |
| A | `AmountSpec` 的 canonical 64 位冻结 + 判定顺序（可表示性先于范围） | §3.7.6 第 5、6 条 | §数值域与量程、域 2 `DomainAmount` 行 | `CM-T13`（非本台账行；`RULING_WORKSHEET` §2 行 + §7B 绑定） |
| B | 三条原子失败映射 + "同 Tick 同 canonical 身份重入 → `invalid_relation`" | §3.7.8 表（新增"输入半批"映射块）、§9.3 | §数值域与量程、§S7A-2 节 ④ 行 | `RULING_WORKSHEET` §2 表后 provenance、§9 第 2 轮行；`OPEN_QUESTIONS` §4.3 |
| C | 负 `observationTick` 合法；回退 → `invalid_relation`；clock 不可表示复用 tick 域表示失败 | §3.7.3 第 4、5 条、§3.7.8、§9.3 | §时间单位与表达、§稳定与非稳定声明、§S7A-2 节 ③ 行 | `CM-T10`（`CONTRACT_MATRIX` §3）；`OPEN_QUESTIONS` Q-04 行与 §4.3 |
| D | 域声明进 session identity；重排不算变化；7A 不引入 per-domain 版本字段 | §5.2 identity 行、§3.7.6 | 域 2 `InputMapping`、域 6 `NormalizationProfile`、§数值域与量程 | `CM-T09`（`CONTRACT_MATRIX` §3）；`OPEN_QUESTIONS` §4.3 |
| E | 不连续表示复用既有 `input.continuous_unsupported`（`field.path` 区分，映射 R-05） | §3.7.7 表、§3.7.8、§7.2 | §S7A-2 输入半批冻结补充 | `CM-T10`（`CONTRACT_MATRIX` §3）；`OPEN_QUESTIONS` §4.3 |

**登记落点补充。** Spec 侧另在 `## S7A-2 限定冻结范围与登记规则（2026-10-03）` 补 6 条登记与 provenance，
并在 §11.1 增加第 8 条（per-domain 版本字段留空、无独立 clock 令牌）；ABI 侧新增
`## S7A-2 输入半批冻结补充（2026-10-03）` 一节并更新既有 §未决项 #18（**不新增编号**）。

## 4. 死令牌删除与"不可表示 clock 复用 tick 溢出"（卡 ② 第 1 点）

**事实。** 输入半批落地后，实现自查发现 `engine/judgement/src/source_codes.hpp`（src-only，不安装、不进公共
码表）中有两个**无任何引用**的令牌：`kCalibratedClockInvalidCode`（其说明文字为"a calibrated clock value
that is not representable, **or a negative calibrated clock value**"）与 `kObservationTickMissingCode`。
前者的文字与已冻结的"负 `observationTick` 合法、只有回退被拒"**直接矛盾**；两者都属死码。

**裁定。** **删除这两个令牌**，不保留、不重述：

1. **负 `observationTick` 不触发任何拒绝码**——它不是失败情形，保留一个"负值非法"令牌会重新允许把合法输入
   错误拒绝并形成未登记的并行错误路径。
2. **不可表示的 calibrated clock 值统一复用既有 `kTickOverflowCode`**，按 **`budget_exceeded`** 的 Tick 域
   表示失败处理；**不保留**独立于 `time_reversal` / 窄化的 7A 令牌。
3. **入口违反"先捕获校准会话时钟"的顺序**只走既有 **`invalid_relation`** 原子失败路径，同样不保留独立令牌。
4. **`observationTick` 缺失**不另立 7A 令牌（结构缺失由既有 `invalid_relation` 路径承载）。

**落点。** 实现批次 `src`（令牌删除与其注释清理）；语义落点 Spec §3.7.3 第 5 条与 §9.3 第 3 条；ABI
§时间单位与表达与 `## S7A-2 输入半批冻结补充` 的 ⑥ 行。

## 5. `time_reversal` 的类别从 `budget_exceeded` 修正为 `invalid_relation`

**旧表述。** 时基半批落地时，`judgement.s7a2.timebase.time_reversal` 报 **`budget_exceeded`**（第 2 轮
实现复核时按"数值域失败"归类）。

**修正后的表述。** `time_reversal` 报 **`invalid_relation`**。理由：

1. **判据是关系而非数值。** `observationTick` 的唯一 canonical source 是**校准后会话时钟**（§3.7.3），
   而"时钟回退"违反的是**时间次序关系**，不是某个数值越出其声明量程。§9.3 三分法把**结构 / 次序关系非法**
   归 `invalid_relation`、把**声明域内数值不可满足**归 `budget_exceeded`；回退属前者。
2. **避免语义混淆。** 把回退记为 `budget_exceeded` 会与数值预算失败（Tick 溢出、量程越界、窄化）混为一类，
   使调用方无法区分"值太大"与"时间往回走"。
3. **与负值合法保持一致。** 负 `observationTick` 合法且参与单调排序（§3.7.3 第 4 条），因此拒绝的唯一
   理由是**次序**；只有把该拒绝登记为关系错误，负值合法与回退拒绝才不矛盾。
4. **不新增类别、不新增 R 条目。** `invalid_relation` 是既有九类之一；该映射不新增 ABI 码、不新增 R 编号。

**落点。** Spec §3.7.3 第 4 条、§3.7.8 表（输入半批第三条）、§9.3 表与第 3 条；ABI §时间单位与表达、
§S7A-2 节 ③ 行；`CONTRACT_MATRIX` 的 `CM-T10` 未决点文字；`RULING_WORKSHEET` §2 表后 provenance 与 §9。

## 6. 域声明进 identity 与"重排不算变化"（卡 ① 第 6 问、卡 ② 第 2 点）

**裁定。** session identity 分量为 `profileId`、`profileVersion`、`sourceClass`，**加规范化后的域 token
集合与每项域声明的完整 `AmountSpec` 字段**（`scale` / `minimum` / `maximum` / `boundaryPolicy`）：

- **域声明顺序不承载语义**：同一集合的**重排不是 identity 变化**（实现按域 token 规范化比较，不按表位置）。
- **增删任何域或改动其 `AmountSpec` 是 identity 变化**（会改变 `NormalizedObservation` 的 canonical 内容）。
- **7A 不引入 per-domain 版本字段**：卡 ① 对"每个域声明各自带一个版本"的假设在 7A 的答案是**没有这个
  字段**——会话级版本完全由 `profileVersion` 承载；`AmountSpec` 无独立版本字段，声明之间没有别的可差异项。
- **不得推迟到 S7A-4**：`CM-T09` 已明确"映射的 source identity / 版本 / 量化必须进 session identity"，
  实现排除域声明表与文档冲突，故在本轮冻结。

**落点。** Spec §5.2 identity 行、§3.7.6；ABI 域 2 `InputMapping`、域 6 `NormalizationProfile`、
§数值域与量程；`CONTRACT_MATRIX` 的 `CM-T09`；`OPEN_QUESTIONS` §4.3。

## 7. 实现侧事实与门禁

**实现侧事实（供判断，不是请求确认的实现细节；`engine/judgement/**`、`tests/judgement/**` 属并行实现，
可读但不代表批次已收口）。**

1. `contributesToSameSessionIdentity` 现比较**三分量 + 规范化域声明集合**：`profileId`、`profileVersion`、
   `sourceClass` 逐项相等，且域声明集合按**域 token** 规范化比较、`AmountSpec` 全部字段逐一相等，
   表位置不参与（`engine/judgement/src/input.cpp` 的 `contributesToSameSessionIdentity` /
   `sameDomainDeclarationSet`）。
2. `rejectRuntimeMappingChange` 在身份不同时按**第一个不同分量**报字段路径（
   `firstDifferingIdentityPath`：`profileId` → `profileVersion` → `sourceClass` → 域声明集合），
   使运行时修改映射的拒绝能指名真正变化的分量，而不是固定路径。
3. `source_codes.hpp` 中两个死令牌已删除，clock 不可表示复用 `kTickOverflowCode`；`kTimeReversalCode`
   已按 `invalid_relation` 归类。
4. **测试**：`cuexis_judgement_tests` 由 1838（基线）增至 **1976 断言 / 57 用例**；本文写作时直读
   `out/build/debug/bin/cuexis_judgement_tests.exe`，输出为 `All tests passed (1976 assertions in 57 test
   cases)`。该运行只证明该二进制自报全过，**不是**本次文档批次的验证声明。
5. 连续 / 不连续输入在入口按 `field.path`（`continuityCapability` vs `discontinuity`）分别稳定拒绝，
   两者复用同一 ABI 冻结码与 `capabilityId`。

**本轮文档门禁（本文作者运行）。**

| 门禁 | 命令 | 结果 |
| --- | --- | --- |
| 文档门禁 | `python -B tools/check_docs.py` | 本文与全部改动文件**无问题**；**仓库整体当前 exit 1**，唯一失败项在本文范围之外：`docs/adr/0044-gameplay-v2-semantic-kernel.md` 指向**尚不存在**的 `../stage_reports/stages/stage-07/2026-10-03-s7a-6-gate-rulings.md`（第 6 / 7 轮并行改动的链接，见 §9） |
| 空白与制表符 | 对本文与全部改动文件扫描 | 行尾空白 0、制表符 0 |
| 差异空白 | `git diff --check` | exit 0 |
| judgement 测试（只读复核） | `out\build\debug\bin\cuexis_judgement_tests.exe` | `All tests passed (1976 assertions in 57 test cases)` |

## 8. 计数不变声明

本轮输入半批复核**只更新既有条目的语义文字**，所有计数保持不变：

| 计数 | 值 | 说明 |
| --- | --- | --- |
| `CONTRACT_MATRIX` 总数 | **114** | `accept` 42 / `revise` 26 / `supersede` 17 / `retain` 17 / `open` 10 / `reject` 2 |
| `CM-T09` / `CM-T10` 处置词 | `retain` / `accept` | **一律不动**，只更新未决点文字 |
| `CM-T13` | 非本台账行 | 按 §7B 登记的裁决项，不计入 `open` 或 §0.1 末列 |
| `RULING_WORKSHEET` §0.1 | 76 行 / 84 项 / 末列 21 | 不动 |
| `RULING_WORKSHEET` §8 `open` | **10** | 不动 |
| Spec §7.2 R 条目 | **19** | 不新增 |
| Spec §9.2 类别 | **九类** | 不新增第十类 |
| ABI 类型条目 | 9 域 / **126 = 96 + 30** | **未新增类型条目** |
| ABI §未决项条目数 | 不变 | 只更新既有 #18，**不新增编号** |
| ABI 9 行状态格首词 | `冻结目标` / `待冻结` | 一律不动 |

## 9. 陈旧值复核与门禁记录

**陈旧值 grep（逐条解释）。**

| 检索项 | 命中 | 解释 |
| --- | --- | --- |
| 旧表述「`量化`＋`版本`」（相邻两词，per-domain 假设） | **0** | 卡 ② 要求删除的"每个域声明各自带一个版本"假设表述在全部文档中**已无字面残留**（含本文亦不写该字面序列）；现存文字一律为"**7A 不引入 per-domain 版本字段**"的否定式说明 |
| `amount_out_of_range` 的类别 | 见 Spec §3.7.6 第 6 条 / §3.7.8 表 / §9.3 表 | 一律 `budget_exceeded`（"可表示但越界"分支），无其他类别 |
| `time_reversal` 的类别 | 见 Spec §3.7.3 第 4 条 / §3.7.8 表 / §9.3 表 / ABI §时间单位与表达 | 一律 `invalid_relation`；**历史证据**中的 `budget_exceeded` 归类只出现在本文 §5 的**修正说明**与带日期的既有报告中，不回改 |
| `budget_exceeded` 出现处 | Spec §3.7.6 / §3.7.8 / §9.3、ABI §数值域与量程 | 仅覆盖 Tick 溢出、量程越界、窄化、声明域内数值非法；**不再**覆盖 `time_reversal` |
| `same_tick_collision` / `time_reversal` 与 `budget_exceeded` 同处出现 | 仅本文 §5 的修正说明 | 可解释为**修正说明**（"从 `budget_exceeded` 修正为 `invalid_relation`"），不构成与现行 Spec 的冲突 |

**门禁输出。** `python -B tools/check_docs.py`：本批次改动与新增文件**全部通过**（链接、H1、索引可达性、
阶段索引与计划结构检查均无告警）；**仓库整体当前 exit 1**，唯一失败项**不在本批次范围内**——
`docs/adr/0044-gameplay-v2-semantic-kernel.md`（由第 6 / 7 轮并行任务在本次门禁运行期间修改）指向
`../stage_reports/stages/stage-07/2026-10-03-s7a-6-gate-rulings.md`，该文件**不存在**（目录中只有合并的
`2026-10-03-s7a-5-6-gate-rulings.md`）。本批次**不改写 ADR 0044**（它不在本批次的落点清单内），故该项
留给 ADR 0044 的所属改动处置。其余：改动 / 新增文件行尾空白与制表符 **0**；`git diff --check` **exit 0**
（输出为空）。逐条命令与结果见 §7 表。

**两处既有的表格渲染瑕疵（非本批次引入，仅登记不改）。** 逐行核对表格列数时发现两处**已存在**的未转义
竖线：`OPEN_QUESTIONS.md` §4 的 `P2-05` 行（`action = press|release|update|absence|step`）与
`CONTRACT_MATRIX.md` §13 的 `CXC_FORMAT.md` 行（`entryKind = gameplay-graph | packed-chart |
author-source`）。两者都在本批次落点之外、且早于本批次存在，**本批次不改**；本批次新增 / 改写的表格
（含本文 §1、§2、§3、§7、§8、§9）列数逐块一致。

## 10. 相关索引

- [第 2 轮门禁报告](2026-10-03-s7a-2-gate-rulings.md)：第 2 轮 7 条裁定的原始落地证据（历史，不回改）
- [第 2 轮实现裁定记录](2026-10-03-s7a-2-implementation-rulings.md)：时基半批的 7 项 + 4 项复核（同 thread，互为补充）
- [Stage 7A 实施计划](../../../stage_plans/active/stage-07/plan.md)：§S7A-2 的目的与验证要求
- [未决语义分轮裁决清单](../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md)：§2 第 2 轮行、表后 provenance 与 §9 裁定登记
- [未决问题与 owner 决策请求](../../../proposals/gameplay-v2-acceptance/OPEN_QUESTIONS.md)：§2.1 与 §4.3 输入半批复核登记
- [Gameplay V2 Spec](../../../formats/GAMEPLAY_V2_SPEC.md)：§3.7.3、§3.7.6、§3.7.7、§3.7.8、§5.2、§9.3、§S7A-2 节、§11.1
- [Gameplay V2 ABI](../../../api/GAMEPLAY_V2_ABI.md)：域 2 `DomainAmount` / `InputMapping`、域 6 `NormalizationProfile`、§数值域与量程、§时间单位与表达、§未决项 #18、`## S7A-2 输入半批冻结补充`
- [合同逐项台账](../../../proposals/gameplay-v2-acceptance/CONTRACT_MATRIX.md)：§3 的 `CM-T09` / `CM-T10` 未决点文字与 §0 的 `CM-T13` 说明
- [ADR 0044](../../../adr/0044-gameplay-v2-semantic-kernel.md)：第 2 轮行与证据边界的最小指针

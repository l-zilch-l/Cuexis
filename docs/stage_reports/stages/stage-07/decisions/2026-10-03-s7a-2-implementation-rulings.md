# S7A-2 时基实现裁定落地记录（第 2 轮实现复核修订）

状态：dated implementation record（S7A-2 时基实现批次的语义裁定与文档落地证据；不是阶段关闭、
不是 owner acceptance、不是实施验证、**不是发布或提交声明**）

更新日期：2026-10-03

上级文档：[Stage 7A 实施计划](../../../../stage_plans/active/stage-07/plan.md) §S7A-2 ·
[Gameplay V2 acceptance package](../../../../proposals/gameplay-v2-acceptance/README.md) ·
[未决语义分轮裁决清单](../../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) §2、§9

本文是**带日期的落地记录**（reports own dated evidence）：它登记 S7A-2 时基半批实现落地后，Codex 对
实现中语义裁量的裁定、由此对**第 2 轮已落地语义**（尤其 `stop` 模型）的**修订**、修订后的落点，以及
本轮跑过的门禁与结果。它不复制 Spec / ABI 的合同正文，也不构成任何实施声明；未在本文列出的门禁一律
视为未运行。第 2 轮本身的裁定落地证据仍以
[第 2 轮门禁报告](2026-10-03-s7a-2-gate-rulings.md)为准，本文**不回改**该报告。

## 1. provenance

两张卡同属 Codex 话题 thread `s7a1-freeze-bindings`（与第 2 轮主裁定同 thread，`gpt-6-astra`，consult）：

| 卡 | 文件名时间戳（UTC） | verdict | confidence | 内容 | 登记日期 |
| --- | --- | --- | --- | --- | --- |
| ①7 项语义裁量卡 | 2026-10-02T20:06:36.075Z | `adopt` | **0.98** | stop 语义模型、`differenceTicks(INT64_MIN)`、拒绝类别归属、`commitAt` 窗口、`validatePrepare` 边界、`roundHalfToEven` 守卫、S1-05 接口收紧 | 2026-10-03（本地） |
| ②4 项收尾裁定卡 | 2026-10-02T20:16:22.741Z | `adopt` | **0.99** | 分数 duration 的**单次舍入**（精确累计函数）、负向 `(b, origin]` 与失去奇对称、缺失 `initialTempo` 的类别、`profileId` / `unitToken` 是否 optional | 2026-10-03（本地） |

**日期口径说明。** 两张卡的文件名时间戳为 2026-10-02T20:0xZ / 20:1xZ（UTC），按第 2、3、4 轮先例统一按
**本地日期 2026-10-03** 登记。

**thread 口径说明。** 两卡沿用第 2 轮的 thread `s7a1-freeze-bindings`，因此本文不另立新话题；这也意味着
**第 2 轮的已落地语义在实现复核后被同 thread 修订**，修订属第 2 轮范围，不构成第 5 轮或新裁定轮。

## 2. 七项裁量裁定（卡 ①）

| # | 裁量点 | 裁定结论 | 落点 |
| --- | --- | --- | --- |
| 1 | `stop` 的语义模型 | **拒绝比例替换模型**，采用**塌缩 / 跳变**：`[startBeat, endBeat)` 内映射冻结于同一累计值，`endBeat` 处一次性计入声明的 `duration` | Spec §3.7.2 第 3 条；`timebase.hpp` 头注释 |
| 2 | `differenceTicks(INT64_MIN, 0)` | **必须接受**（该值在冻结的 64 位域内可表示）；只有 checked subtraction / addition **真正越界**才稳定拒绝（`budget_exceeded`）。`offsetTicks` 同规则；`makeInterval` 只按 `end < start` 拒绝 | 实现批次（`src`），语义受 Spec §3.7.1 第 4 条约束 |
| 3 | 新增拒绝的类别归属 | 声明**结构 / 次序**非法 → `invalid_relation`；数值超出其**声明域** → `budget_exceeded`；late-policy **缺失 / 未声明 / 未测量** → `late_policy_incomplete`。**不新增第十类、不新增 R 条目** | Spec §9.3 三分法、§3.7.8 表（六类映射） |
| 4 | `commitAt` 的窗口语义 | 窗口良构当且仅当 **`openTick <= commitTick <= closeTick`（两端闭）**；只接受 `tick == commitTick`，其余稳定拒绝（`commit_not_at_commit_tick`），**不**"挪到下一个可提交 Tick" | Spec §3.7.5（既有语义，本次确认一致） |
| 5 | `validatePrepare` 的边界 | 只检查 late-policy 参数"**已声明且已测量**"，**不检查** `finalizationWatermark` / queue hop / 窗口开闭阈值之间的**量级关系**；该校验登记为**阻塞 S7A-4 的 late-window / 判定消费门禁**，具体数值与最终限额仍记 **S7A-9** | Spec §3.7.4 第 5 条、§3.7.7 表；[BATCH_GATES.md](../../../../proposals/gameplay-v2-acceptance/BATCH_GATES.md) 的 S7A-4 段 |
| 6 | `roundHalfToEven` 的不可达范围守卫 | **保留守卫并保留注释**（表示变更或未来路径的防御性检查，不是应删除的死代码） | 实现批次（`src`） |
| 7 | S1-05 接口收紧 | 删除 `RationalBeat` / `RationalDuration` 的**公开零值默认构造**（改 checked factory）；`LatePolicyParameters::policy` 用 `std::optional`（未声明即 `nullopt`）；删除 `TimebaseProfile::lateEventPolicy`（重复的第二策略来源）；去掉会构造分配诊断的函数上的 `noexcept`（纯比较的 `canonicalOrder` 保留）；`TickCollisionKey` **要求构造点显式提供 `OriginKind`**；`makeMicrosecondTimebase()` 显式声明 `profileId` / `unitToken` / `tickScale` / `originBeat`、**刻意不声明 tempo** | 实现批次（头 / `src`）；语义受 Spec §3.7.1 第 3 条与 §9.3 三分法约束 |

## 3. 四项收尾裁定（卡 ②）

| # | 问题 | 裁定结论 | 落点 |
| --- | --- | --- | --- |
| A | 分数 `duration` 下"跳变与舍入的顺序" | 采纳**精确累计值上跳变、最终只舍入一次**：`F(originBeat) = 0`、`F(endBeat) = F(startBeat) + duration`、`tick(beat) = roundHalfToEven(F(beat))`；**不得**解释为整数 Tick 递推 `tick(endBeat) = tick(startBeat) + duration` | Spec §3.7.2 第 3 条（含反例）；`timebase.hpp` 头注释 |
| B | 负向约定与奇对称 | 负向累计区间为 **`(b, origin]`**，凡 `endBeat` 落入该区间的 `stop` jump 计入后再取负；**接受"有 `stop` 时 Beat → Tick 不再关于 `origin` 奇对称"**，并写明**有向端点关系与单调性仍成立** | Spec §3.7.2 第 2、3 条 |
| C | 缺失 `initialTempo` 的类别 | **未声明**属**结构不完整** → `invalid_relation`（`field.path = initialTempo`）；**已声明但非正**属**声明域内数值非法** → `budget_exceeded`；两者与"缺失 / 未测量 late-policy"的 `late_policy_incomplete` **明确区分** | Spec §9.3 三分法、§3.7.8 表；ABI 域 1 |
| D | `profileId` / `unitToken` 是否改 optional | **不改**：保持 `std::string_view`，**空值表示"未声明"**，由 `validatePrepare` 以 `invalid_relation` 拒绝；空值**不是**默认单位、也**不是**可用 profile | Spec §9.3；ABI 域 1 与 §时间单位与表达 |

## 4. `F()` 定义与"两次舍入"反例

**定义（Spec §3.7.2 第 3 条，逐字落点）。** `F(originBeat) = 0`；`stop` 在 `[startBeat, endBeat)` 内使
**累计速率为零**，并在 `endBeat` 处把声明的 `duration` 加入精确累计值，即
`F(endBeat) = F(startBeat) + duration`；最终 **`tick(beat) = roundHalfToEven(F(beat))`**，**全程只舍入
一次**，舍入发生在精确累计域上。

**反例（卡 ② 问题 A 的原始数值）。** `tempo = 1/1`、`originBeat = 0`、`stop = [1/2, 3)`、`duration = 3`：

| 读法 | `tick(1/2)` | `tick(3)` | 判定 |
| --- | --- | --- | --- |
| **精确累计函数（采纳）** | `roundHalfToEven(1/2) = 0` | `roundHalfToEven(1/2 + 3) = roundHalfToEven(7/2) = 4` | **正确** |
| 字面整数递推（拒绝） | `0` | `tick(1/2) + 3 = 0 + 3 = 3` | **错误**：先取整再加 `duration`，引入**第二次舍入** |

**为什么必须写进 Spec：** 该差异只在 `F(startBeat) + duration` **不是整数**（或 `tick(startBeat)` 本身
被舍入过）时出现；若 Spec 只写 `tick(endBeat) = tick(startBeat) + duration`，实现可以合法地读出第二种
读法，从而与"同一精确积分、同一舍入"的要求冲突。因此 Spec 同时给出 **`F` 的定义**与**禁止整数递推**的
明文，并用上表反例固定（`tick(3) = 4` 保持正确）。

## 5. 负向约定 `(b, origin]` 与"失去奇对称"

**负向约定。** `b < originBeat` 时，累计方向为 `(b, originBeat]`：凡 `endBeat` 落在该半开区间内的 `stop`
jump **计入后**再取负。这是使**两侧** `stop` 端点关系（`F(endBeat) = F(startBeat) + duration`）都成立的
唯一半开方向（另一种方向会让 `F(0) = 0 ≠ F(-2) + 4`）。

**镜像反例（卡 ② 问题 B 的原始数值）。** `tempo = 1/1`、`originBeat = 0`，`stop = [0,2)` 与
`stop = [-2,0)` 各 `duration = 4`：

| beat | 落入的累计区间 | `tick(beat)` |
| --- | --- | --- |
| `1` | `(0, 1]`：`[0,2)` 内冻结（jump 在 2，不计入） | `0` |
| `-1` | `(-1, 0]`：`[-2,0)` 的 jump 发生在 0，**计入** | `-4` |

于是 `tick(-1) = -4 ≠ -tick(1) = 0`：**有 `stop` 时，镜像配置下映射不再关于 `originBeat` 奇对称**。
Spec §3.7.2 第 2 条同时写明：`roundHalfToEven` **作为舍入函数**仍对负值对称
（`roundHalfToEven(-x) = -roundHalfToEven(x)`），**但该性质不等于整个映射的奇对称**；第 3 条明确
**有向端点关系与单调性在两侧仍成立**（映射仍非递减）。实现侧已把断言从"奇对称"改为**显式列出两侧
数值**。

## 6. 三分法（拒绝类别，Spec §9.3）

| 情形 | 判据 | 类别 |
| --- | --- | --- |
| 声明**结构不完整** | 缺 `profileId`、缺 `unitToken`（含空值）、未声明 `initialTempo` | `invalid_relation` |
| 已声明但**声明域内数值非法** | 非正 `tickScale`、非正 tempo、非正 `stop` duration，以及数值越出其声明范围 | `budget_exceeded` |
| **late-policy 缺失 / 未声明 / 未测量** | 参数缺失、未声明，或停在 `pending_measurement` | `late_policy_incomplete` |

- 第 1 类与第 2 类**必须区分**：前者是**声明本身不完整**（结构 / 次序），后者是**声明完整但值在其声明域
  内不可满足**（值）。
- 第 3 类与"缺失 / 未测量 late-policy"**明确区分**于前两类：它既不是 profile 结构缺陷，也不是数值越界。
- **不新增第十类**（Spec §9.2 仍九类）、**不新增 R 条目**（Spec §7.2 仍 19 条）。
- `profileId` / `unitToken` **保持字符串**（`std::string_view`，**不改为 optional**）：空值表示"未声明"，
  由 prepare 以 `invalid_relation` 拒绝；空值**不是**默认单位或可用 profile。

## 7. 迟到参数量级门禁（Spec §3.7.4 第 5 条、§3.7.7 表、BATCH_GATES 的 S7A-4 段）

`validatePrepare` **只**检查 late-policy 参数"**已声明且已测量**"，**不检查** `finalizationWatermark` /
queue hop / 窗口开闭阈值之间的**量级关系**（序关系、上界与相互一致性）。裁定把"**参数量级关系的校验**"
登记为**阻塞 S7A-4 的 late-window / 判定消费门禁**（S7A-4 在按 late policy 消费窗口与判定之前必须补齐），
**具体实测数值与最终限额**仍由 **S7A-9 的硬化与测量门禁**承载。本项**不阻塞 S7A-2**，也**不新增 R
编号**。落点：

- [Spec §3.7.4 第 5 条](../../../../formats/GAMEPLAY_V2_SPEC.md)：把原"具体数值登记为后续批次阻塞项"改写为
  "只检查已声明且已测量 + 量级关系校验阻塞 S7A-4 + 数值与限额记 S7A-9"；
- [Spec §3.7.7 表](../../../../formats/GAMEPLAY_V2_SPEC.md)：新增"late-policy 参数量级关系"行；
- [Spec §9.3](../../../../formats/GAMEPLAY_V2_SPEC.md) 三分法段、[Spec §11](../../../../formats/GAMEPLAY_V2_SPEC.md)
  第 2 轮后续批次行；
- [BATCH_GATES.md](../../../../proposals/gameplay-v2-acceptance/BATCH_GATES.md) 的 S7A-4 节新增一段门禁说明；
- [GAMEPLAY_V2_ABI.md](../../../../api/GAMEPLAY_V2_ABI.md) §时间单位与表达与 §未决项 #18。

## 8. Tick → Beat 反查契约与批次归属

**契约。** 塌缩模型下 Beat → Tick **非单射**（`[startBeat, endBeat)` 内多个 Beat 落到同一 Tick），因此
**任何 Tick → Beat 反查不得返回单一 Beat**：必须返回**区间 / 集合**，或对歧义**稳定拒绝**。

**登记与归属。** 登记为**"首次消费 Tick → Beat 反查的后续批次"**的阻塞项（Spec §3.7.2 第 6 条、
§3.7.7 表、ABI §未决项 #18）。**归属依据**：

1. 第 2 轮（`s7a1-freeze-bindings`，含本次两张续裁卡）**只冻结 Beat → Tick 的正向映射语义**；反查不在
   本批次冻结面内，故只登记契约、不冻结实现。
2. **按既有先例**（[RULING_WORKSHEET.md](../../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md)
   §3 的 `boundedRelationInstance` / `CM-C10` 行："候选表示只阻塞**首次拟消费它**的后续批次"），本轮
   **不指定具体批次编号**，只写"首次消费该反查的后续批次"，避免预占后续轮次的语义。
3. **已排除批次（本轮核对）**：`S7A-3`（prepare / 装配 / entry）、`S7A-4`（仲裁、资源与事实序）、
   `S7A-5`（Ruleset、事实序、Score 与 Snapshot）**均不消费** Tick → Beat 反查——它们的不得消费清单
   （Spec §3.8.5、§3.13）与工作内容都不含 Tick → Beat 查询；因此该反查**不是**这三个批次的门禁。
4. **候选消费方（未裁定，故不写死）**：Seek / Snapshot / Replay 侧若出现"由 Tick 定位作者 Beat"的需求，
   该批次即为首次消费者，须在消费前钉死"区间 / 集合 vs 稳定拒绝"的选择与诊断码。**本轮不替该批次
   预先决定**，也不据此新增 R 条目或第十类。

## 9. 改动的文档与落点

| 文件 | 落点 | 要点 |
| --- | --- | --- |
| [GAMEPLAY_V2_SPEC.md](../../../../formats/GAMEPLAY_V2_SPEC.md) | §3.7.2 标题 + 第 2、3 条改写与新增第 5、6 条；§3.7.4 第 5 条；§3.7.7 表两行；§3.7.8 表两行与"六类"措辞；§9.3 三分法段；§S7A-2 节 provenance 与 ②／④ 行；§11 第 2 轮行与后续批次行、§11.1 第 4 条 | 精确累计函数 `F`、`stop` 塌缩 / 非单射 / 半开方向、三分法、量级门禁、Tick → Beat 反查登记 |
| [GAMEPLAY_V2_ABI.md](../../../../api/GAMEPLAY_V2_ABI.md) | 域 1"第 2 轮已冻结的事实"段新增两条；§时间单位与表达新增两条；§S7A-2 节 ② 行；§未决项 #5 与 #18 | **不新增类型条目**：9 域 / **126 = 96 + 30** 不变 |
| [RULING_WORKSHEET.md](../../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) | §2 `Q-04` / `CM-T03` / `CM-T08` 行的 `owner 裁定` 列补修订指针；§2 表后 provenance 块补两张续裁卡与"实现复核后修订"说明；§9 第 2 行补续裁指针 | **不新增/删除任何分轮表行**；`open` 仍 14、§7.2 仍 19 条 |
| [OPEN_QUESTIONS.md](../../../../proposals/gameplay-v2-acceptance/OPEN_QUESTIONS.md) | §1 的 `Q-04` 批次格补指针；§2.1 `Q-04` / `CM-T03` 行补指针 + 新增"实现复核修订"段 | **不改 §1–§5 的 2026-10-02 证据列原文** |
| [acceptance README.md](../../../../proposals/gameplay-v2-acceptance/README.md) | §4.3 第 2 轮段补修订指针 | `open` 计数**不变**（仍 14） |
| [BATCH_GATES.md](../../../../proposals/gameplay-v2-acceptance/BATCH_GATES.md) | S7A-4 节新增"late-policy 参数量级关系的校验"门禁段 | 只补门禁，不改任何批次的其它门禁 |
| [ADR 0044](../../../../adr/0044-gameplay-v2-semantic-kernel.md) | "接受门禁"表第 2 轮行 + "证据边界"第 7 条 | **最小指针**，不改任何决策语义 |
| 本文 + [stage_reports/README.md](../../../README.md) | 新增本文并挂进阶段索引 | 本轮的带日期落地证据 |

**本轮未改动的文档（有意为之）。** `CONTRACT_MATRIX.md` 不在本批次授权的文档清单内，故其 `CM-T03` /
`CM-T08` 行的"未决 / 风险"列仍保留第 2 轮的原始裁定文字；该处只复述第 2 轮结论、不含 `stop` 语义的
错误表述（未出现"比例替换"或"不推进的 Tick"），修订指针以
[RULING_WORKSHEET.md](../../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) §2 的 `owner 裁定`
列为准。若后续要求台账同源，应在另一次授权内只改这两行的说明文字（**计数不受影响**）。

## 10. 实现侧已达成的事实（实现批次的输入，不是本批验证）

以下事实**由实现批次报告并经其复跑**，本文只作登记；**本批为纯文档批次，未复跑 C++ 构建或 CTest**，
因此这些数字**不构成本文的验证结论**，也不表示任何代码已发布、已提交或已合入：

| 项 | 实现侧状态（引用） |
| --- | --- |
| `timebase.hpp` 头注释 | 已按卡 ② 问题 A / B 改写：写明 `F()` 定义、只舍入一次、不得整数递推、负向 `(beat, origin]`、删除"负值结构上保持对称"的绝对表述 |
| `TickCollisionKey` | 三参数构造，**强制显式 `originKind`**（无默认成员值，省略参数为编译错误） |
| `TimebaseProfile::initialTempo` | `std::optional<RationalDuration>`，**未声明即 `nullopt`**；`makeMicrosecondTimebase()` 刻意不声明 tempo |
| `RationalBeat` / `RationalDuration` | **零值不可默认构造**（checked factory `create(numerator, denominator)`） |
| 测试 | 全量 **1145 断言 / 34 用例**；`timebase:*` 子集 **1058 断言 / 19 用例** |
| 门禁 | 格式门禁与架构门禁通过；R 行仍 19、未新增类别 |

**注。** 上述数据来自实现批次的自述与复跑记录（含卡 ② 的问题描述），本轮文档批次**未**独立运行
`cmake` / `ctest` / `cuexis_format_check`；若需要把这些数字升级为独立证据，须由实现批次另行出具带
commit 的记录。

## 11. 跑过的门禁与结果

| 门禁 | 命令 | 结果 |
| --- | --- | --- |
| 文档链接 / 索引 / 可达性 / 单一 H1 / 候选 JSON 一致性 | `python -B tools/check_docs.py` | 见 §11.1（实际输出与 exit code 原文） |
| 行尾空白与制表符扫描（本次改动与新增文件） | 见 §11.2 | 见 §11.2（逐文件 0 处） |
| `git diff --check` | `git diff --check` | 见 §11.3（exit 0，无输出） |
| 计数脚本直读复核 | 内联 Python（无临时文件） | 六类 `accept` 38 / `revise` 26 / `supersede` 17 / `retain` 17 / `open` 14 / `reject` 2 = **114**；§8 `open` 行 **14**；§0.1 **76 行 / 84 项 / 末列 21**；SPEC §7.2 的 `R-nn` 行数 **19**；ABI **9 域 / 126 = 96 + 30** |
| 陈旧值 grep | `比例替换` / `不推进的 Tick` / `stop 只把` / 绝对"对称"表述 | 见 §12（残余命中逐条解释） |
| C++ 构建与 CTest | 未运行 | **本轮为纯文档改动**，不修改 C++、CMake、Schema 或生成输入，按仓库政策（AGENTS.md §Documentation check）不需要构建或 CTest |

### 11.1 `check_docs.py` 实际输出

```text
Documentation checks passed: 334 Markdown files and 20 candidate JSON/CXT files validated.
exit code: 0
```

（`334` 为该次运行的 Markdown 计数，含本次新增的本文；`Documentation checks passed` 即 exit 0。）

### 11.2 空白扫描

对本次改动与新增的全部文件逐行扫描**行尾空白**（含 `\t`）与**制表符**：**0 处**。

### 11.3 `git diff --check`

exit code **0**，无输出（本次改动与新增文件均无空白错误）。

## 12. 陈旧值 grep 与残余命中（逐条解释）

扫描模式与结果（`docs/` 全量，含历史报告）：

| 模式 | 命中位置 | 判定 |
| --- | --- | --- |
| `比例替换` | 本文、[RULING_WORKSHEET.md](../../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) §2（`Q-04` / `CM-T03` 行）与 §9 第 2 行、[OPEN_QUESTIONS.md](../../../../proposals/gameplay-v2-acceptance/OPEN_QUESTIONS.md) §2.1 末段、[acceptance README.md](../../../../proposals/gameplay-v2-acceptance/README.md) §4.3、[Spec](../../../../formats/GAMEPLAY_V2_SPEC.md) 的 §S7A-2 provenance 行 | **保留**：全部是**修订说明**（"由比例替换修订为塌缩 / 跳变"），必须点名被拒绝的旧模型；Spec / ABI 的**语义正文**已无该词，也未把"比例替换"写成现行语义 |
| `不推进的 Tick` | [GAMEPLAY_V2_ABI.md](../../../../api/GAMEPLAY_V2_ABI.md) §未决项 #5 的修订注一处（以显式否定式引用被取代的旧措辞） | **保留**：属"**已被取代**"的引用，不是现行措辞；Spec §3.7.2 正文已**清零** |
| `stop 只把` | 无 | **已修**（Spec §3.7.2 第 3 条改写后清零） |
| 绝对"对称"表述 | [Spec](../../../../formats/GAMEPLAY_V2_SPEC.md) §0.1 术语行"半数取偶…对负值对称"、§3.7.2 第 2 条、§3.7.6 第 2 条"含负值对称"；[ABI](../../../../api/GAMEPLAY_V2_ABI.md) 域 2 `DomainAmount` 行与 §数值域与量程 | **保留（已限定）**：这些措辞都指向**舍入函数本身**的逐值性质（`roundHalfToEven(-x) = -roundHalfToEven(x)`），不是映射的奇对称；Spec §3.7.2 第 2 条已在同处明确"该性质**不是**整个映射的奇对称，存在 `stop` 时不成立" |
| `半数取偶、负值对称` | [RULING_WORKSHEET.md](../../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) §9 第 2 行**前半**（第 2 轮结论的原文复述） | **保留（已加修订指针）**：同一行末尾已在 2026-10-03 补入"第 2 轮实现复核修订"段（`stop` 为精确累计函数）；前半属第 2 轮结论复述，且该处的"负值对称"指舍入函数 |
| 头文件"负值结构上保持对称" | `engine/judgement/include/cuexis/judgement/timebase.hpp` | **已修（实现批次）**：头注释按卡 ② 改写为"the mapping as a whole is not necessarily odd-symmetric about the origin once stops exist"；本文不改代码 |
| 历史报告中的第 2 轮原文（含"负值对称"） | [2026-10-03-s7a-2-gate-rulings.md](2026-10-03-s7a-2-gate-rulings.md) §2 第 2 行 | **保留（历史证据）**：按仓库政策不改带日期报告；修订指针见 §9 与本报告 |
| `CONTRACT_MATRIX.md` `CM-T03` / `CM-T08` 行 | 该文件不在本批授权的文档清单内 | **保留（有意）**：只复述第 2 轮结论（"半数取偶（负值对称）"指舍入函数），**不含** `stop` 语义的错误表述（无"比例替换"、无"不推进的 Tick"）；见 §9 末段 |

**结论**：Spec / ABI **语义正文**中已被取代的 `stop` 措辞（"比例替换"、"不推进的 Tick"、"stop 只把…"）
**清零**（ABI §未决项 #5 保留一处显式否定式的"已被取代"引用）；其余命中都是**修订记录、舍入函数自身的
逐值对称性、历史报告或授权范围外的台账文字**，逐条已解释，均不构成与现行 Spec §3.7.2 语义的冲突。

## 13. 相关索引

- [第 2 轮门禁报告](2026-10-03-s7a-2-gate-rulings.md)：第 2 轮 7 条裁定的原始落地证据（历史，不回改）
- [Stage 7A 实施计划](../../../../stage_plans/active/stage-07/plan.md)：§S7A-2 的目的与验证要求
- [未决语义分轮裁决清单](../../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md)：§2 第 2 轮行与 §9 裁定登记
- [未决问题与 owner 决策请求](../../../../proposals/gameplay-v2-acceptance/OPEN_QUESTIONS.md)：§2.1 与实现复核修订段
- [Gameplay V2 Spec](../../../../formats/GAMEPLAY_V2_SPEC.md)：§3.7.2、§3.7.4、§3.7.7、§3.7.8、§9.3
- [Gameplay V2 ABI](../../../../api/GAMEPLAY_V2_ABI.md)：域 1、§时间单位与表达、§未决项 #5 / #18
- [批次证据门禁表](../../../../proposals/gameplay-v2-acceptance/BATCH_GATES.md)：S7A-4 节新增的 late-policy 量级关系门禁
- [ADR 0044](../../../../adr/0044-gameplay-v2-semantic-kernel.md)：第 2 轮行与证据边界第 7 条的最小指针

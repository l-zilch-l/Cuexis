# 支持 / 拒绝矩阵与 capability registry 草案

状态：candidate；S7A-0 准入产出，待 owner acceptance

更新日期：2026-10-03（§6 登记与字段定义权威落点、§6.1 W-01…W-05 地位与门禁归属为 2026-10-03 第 3 轮裁定登记）

上级文档：[Gameplay V2 acceptance package](README.md) ·
[Stage 7A 实施计划](../../stage_plans/active/stage-07/plan.md)

文档角色：能力边界表征。它把"Stage 7A 支持什么、拒绝什么、拒绝时用什么码"写成可核验的表，
并给出 capability registry 的候选字段。它不是 Spec 正文，也不表示任何能力已经实现。

## 1. 四态 capability 查询

[Input/Replay/Extensions §4](../research/gameplay-v2/INPUT_REPLAY_EXTENSIONS.md) 要求支持矩阵至少区分四态，
且四者诊断不同：

| 状态 | 含义 | 候选拒绝码 |
| --- | --- | --- |
| 未知 | 本地 registry 不认识该 `capabilityId` | `capability.unknown` |
| 已知但未启用 | registry 认识，但本次 session/编译未启用 | `capability.disabled` |
| 已启用但资源/预算不足 | 能力可用，但静态预算或设备能力不满足 | `capability.budget_insufficient` |
| 可用 | 通过 prepare 前全部校验 | 无（成功） |

额外需要的两态（V2 新增，不等同于上面四态）：

| 状态 | 含义 | 候选拒绝码 |
| --- | --- | --- |
| revision 不匹配 | 内容声明 revision 与引擎支持区间不相交 | `capability.revision_mismatch` |
| 永久不支持 / 未来开发 | H 类能力边界 | `capability.permanently_unsupported`、`capability.future_development` |

## 2. Stage 7A 支持集合

下表是 Stage 7A **唯一**允许的实现范围；不在表内的能力必须命中 §3 的稳定拒绝。

| 域 | 7A 支持项 | 来源 | 备注 |
| --- | --- | --- | --- |
| 输入 | 离散 `press` / `release` / `update` 规范化；单一 `ingressSequence` | 计划 §S7A-2.2、SK §2 | 不读取渲染帧时间 |
| 输入 | `InputMapping` 覆盖键盘、鼠标、触摸、手柄、外设的最小映射 | 计划 §S7A-2.3 | 映射 profile 属 session |
| 时间 | `TimebaseProfile`（推荐 `engine.tick.us.v1`）与整数 `judgementTick` | 计划 §S7A-2.1 | 逐字段边界待冻结（CM-T02/CM-T03） |
| 迟到 | `reject_late`、一跳 `queue_next_tick` | PD §8.2、ST F03 | `reopen_uncommitted_window` 不在 7A |
| Requirement | Tap、Hold head/body、显式声明的 Release/tail phase | 计划 §S7A-4.1/§S7A-4.2、PD §3.1 | tail 是同一 Requirement 的 phase |
| Pattern | atom、sequence、choice、bounded repeat（prepare-time 有限）、skip、instant、complement；`leftmost-first` | 计划 §S7A-3.2 | `sameContact`、跨 Requirement relation 拒绝 |
| 资源 | 单一 `capacity = 1` exclusive resource；`free → held → terminal/free`；同 owner 有限恢复；终止后槽位复用 | 计划 §S7A-4.3/§S7A-4.5 | 第 4 轮冻结 7A 子集为 `free` / `held` / `terminal`；`gap`/`handoff_pending`/**非零 grace**/handoff/`capacity > 1`/owner 集合/并列 slot 不启用、稳定拒绝（见 §3 与 Spec §3.10） |
| 仲裁 | 每事件重算候选；`observe`、`consume`、`claim`；固定 `fanout = 1`；`strayWhen` 两种政策 | 计划 §S7A-4.4 | 多资源 fanout 拒绝 |
| 协调 | `greedy_v1` coordinator/solver profile；能证明唯一解的图 | PD §3.1/§5.4 | 未声明 profile 或无法证明唯一 → 拒绝 |
| Fact | append-only Fact Ledger；基础 vocabulary；无 correction | PD §3.1/§6.2 | correction 字段出现即拒绝 |
| Ruleset | typed state transaction、score/combo/statistics、下一 Tick signal、内置 ruleset | PD §3.1 | `cuexis.ruleset` package 不在 7A（见 §4） |
| 快照 | 全量语义 Snapshot / Seek / Replay | PD §3.1、AR P0-13 倾向 | schema 待冻结（CM-K07） |
| 表现 | FactBinding → `render.visible` 的 early/exact/late/Miss 桥接 | PD §10 | precedence 常量待冻结（CM-P04） |
| 入口 | Chart v5 source、Packed Chart、CXC `packed-chart` 与 `gameplay-graph` entry | PD §0、AM §8 | 两者必须恢复同一 Canonical Graph |
| 兼容 | Chart v4 / CXT v1 既有回退路径 | 计划 §S7A-7.4 | 行为不因无输入而改变 |

## 3. 明确拒绝集合（稳定拒绝，不得降级）

| ID | 被拒绝的能力 | 拒绝理由类型 | 候选拒绝码 | 来源 |
| --- | --- | --- | --- | --- |
| R-01 | `capacity > 1` 资源、owner 集合、并列 slot | 能力边界 | `resource.capacity_unsupported` | 计划 §S7A-4.3、AR P0-11 |
| R-02 | Chord / `binding` 关系 | 后续 capability | `coordination.relation_unsupported` | PD §3.2 |
| R-03 | `temporal` / `quota` 关系、有限交替、保护窗口 | 后续 capability | `coordination.relation_unsupported` | PD §3.2 |
| R-04 | Handoff Hold、`gap` / `handoff_pending`、非零 grace（`grace > 0`；7A 的 `grace = 0` 是唯一合法值）、`resource.handoff.v1` | 后续 capability | `resource.handoff_unsupported` | 计划 §S7B-1.3、Spec §3.10（第 4 轮） |
| R-05 | 连续轨迹、Slider、区域覆盖、`input.trajectory.v1`、最低上报率、reconstruction | 后续 capability | `input.continuous_unsupported` | 计划 §S7A-2.5、ST D01-D14 |
| R-06 | Flick、方向、速度、`input.direction.v1` | 后续 capability | `input.direction_unsupported` | PD §3.2 |
| R-07 | 全局最优 / `max_cardinality` / bounded backtracking solver | 后续 capability | `solver.profile_unsupported` | PD §3.2 |
| R-08 | Fact correction、`correctionDepth > 1` | 后续 capability | `fact.correction_unsupported` | PD §6.2 |
| R-09 | `cuexis.ruleset` package 作为 7A 输入（若 owner 判定 7A 不含 package） | 批次归属 | `ruleset.package_unsupported` | CM-S08（待决策） |
| R-10 | `reopen_uncommitted_window` 迟到策略 | 后续 capability | `late.reopen_unsupported` | PD §8.2、ST F04 |
| R-11 | `sameContact` 约束、跨 Requirement relation 藏在 Pattern 内 | 结构约束 | `pattern.relation_unsupported` | 计划 §S7A-3.2 |
| R-12 | 由渲染位置、材质、相机或动画隐式推断 `judgementDomain` | 语义禁止 | `geometry.inference_rejected` | FB §3.3、计划 §S7A-7.1 |
| R-13 | 运行中修改 InputMapping / Loadout / Ruleset 投影 | 生命周期禁止 | `session.mutation_rejected` | IR §3、ST F08 |
| R-14 | 运行时脚本、逐帧回调、宿主字节码、动态 Requirement 生成、随机、墙钟、IO | 永久能力边界 | `capability.permanently_unsupported` | ST H01-H05/H07 |
| R-15 | 三维物理斩击、任意自由体感传感器融合 | 未来开发 | `capability.future_development` | ST H06/H08 |
| R-16 | 未知必需 section、未知 capability、新 requirement kind 被解释成 tap | 静默降级禁止 | `capability.unknown` | AR P0-07 |
| R-17 | 旧 `gameplay.version = 1` 未显式迁移 | 版本迁移 | `format.gameplay_version_unsupported` | CM-V04 |
| R-18 | 非空 CXT v2 `effects` 直接进入 `gameplay.requirements` | 分层禁止 | `migration.ambiguous` | AR P0-05 |
| R-19 | 迁移期无法判断判定域 / 资源共享 / 表现与 Gameplay 关系 | 迁移歧义 | `migration.ambiguous` | FB §11.1 |

规则：

1. R-01 至 R-11 是**后续 capability**，拒绝时必须给出 capabilityId 与替代路径；
2. R-12 至 R-16 是**禁止**，不得通过 `extensions` 字段预留入口；
3. R-17 至 R-19 是**迁移**，必须由离线工具显式处理，Playback 不做隐式猜测；
4. 任何拒绝都不得回落到 Tap/Hold/v4 语义（计划 §1.4、§11）。

**第 4 轮（2026-10-03）的最小同步。** 第 4 轮冻结的 7A 资源子集（`free` / `held` / `terminal`）与
"`gap` / `handoff_pending` / 非零 grace / handoff / `capacity > 1` / owner 集合 / 并列 slot 稳定拒绝"
**早已由 R-01 与 R-04 覆盖**：`capacity > 1`、owner 集合与并列 slot 属 **R-01**
（`resource.capacity_unsupported`），`gap` / `handoff_pending` / handoff 属 **R-04**
（`resource.handoff_unsupported`）。本文件因此只做**一处必要补全**：把**非零 grace（`grace > 0`）**显式写进
R-04 行的被拒绝能力描述，并在 §2 的资源行备注补上冻结子集指针。**条数仍为 19 条**，未新增 R 编号，
§4 的 capability registry 草案与 §5 的 H 类政策均不变。provenance：thread `s7a4-arbitration-rulings`
（`adopt` 0.97 / `adopt` 0.99），2026-10-03；语义正文见
[Spec §3.10](../../formats/GAMEPLAY_V2_SPEC.md)，带日期证据见
[第 4 轮门禁报告](../../stage_reports/stages/stage-07/2026-10-03-s7a-4-gate-rulings.md)。

## 4. capability registry 草案

字段来自 [Input/Replay/Extensions §4](../research/gameplay-v2/INPUT_REPLAY_EXTENSIONS.md) 与
[Preliminary Design §3.2](../research/gameplay-v2/PRELIMINARY_DESIGN.md)。命名与类型仍待 ABI 批次冻结。

| 字段 | 含义 | 7A 取值示例 |
| --- | --- | --- |
| `capabilityId` | 全局唯一稳定 ID | `judgement.tap.v1` |
| `revision` | 单调递增；语义变化必须新增 ID 或 revision | `1` |
| `semanticKind` | 语义类别（input / requirement / resource / coordination / ruleset / presentation） | `requirement` |
| `requiredFormat` | 要求的格式层与版本 | `cuexis.chart@5 + gameplay.version=2` |
| `staticBudget` | prepare-time 计数上界（见预算计划 §3） | 待冻结 |
| `snapshotCost` | 快照增量类型（全量第一版下统一计入全量快照） | `full-v1` |
| `replayImpact` | 是否改变 Replay 兼容判断 | `identity + event vocabulary` |
| `supportedDomains` | 允许的 InputDomain 集合 | 离散按键/触点/通道 |
| `stableRejectCode` | 拒绝时的稳定码 | 见 §3 |

候选 registry 条目（7A 与第一批后续能力）：

| capabilityId | 批次 | 状态 |
| --- | --- | --- |
| `judgement.tap.v1` | S7A-4 | 7A included |
| `judgement.hold.v1`（head/body） | S7A-4 | 7A included |
| `requirement.release_tail.v1` | S7A-4 | 7A included（待 CM-R06 的 inclusion decision） |
| `resource.exclusive.capacity1.v1` | S7A-4 | 7A included |
| `coordination.greedy_v1` | S7A-4 | 7A included（命名待定，AR P0-03） |
| `input.discrete.v1` | S7A-2 | 7A included |
| `late.reject_late.v1` / `late.queue_next_tick.v1` | S7A-2 | 7A included |
| `fact.ledger.append_only.v1` | S7A-5 | 7A included |
| `ruleset.state_transaction.v1` | S7A-5 | 7A included |
| `presentation.fact_binding.visibility.v1` | S7A-7 | 7A included |
| `coordination.binding.v1` | S7B-3 | 后续 |
| `coordination.temporal.v1` | S7B-4 | 后续 |
| `coordination.quota.v1` | S7B-4 | 后续 |
| `resource.handoff.v1` | S7B-1 | 后续 |
| `coordination.optimal.v1` | 后续 | 后续 |
| `input.trajectory.v1` | S7B-1 | 后续 |
| `input.direction.v1` | S7B-2 | 后续 |
| `ruleset.correction.v1` | 后续 | 后续 |
| `ruleset.package.v1` | S7C-2 | 后续 |

## 5. H 类支持政策（锁定）

来源：[表达力压测 §3 H 组](../research/gameplay-v2/GAMEPLAY_EXPRESSIVENESS_STRESS_TEST.md)。
政策文字不得弱化，也不得为这些能力预留未定义字段或执行入口。

| ID | 能力 | 政策 | 候选码 |
| --- | --- | --- | --- |
| H01 | 任意运行时脚本 VM | 能力边界，不再开发，不受支持 | `capability.permanently_unsupported` |
| H02 | 运行时动态生成 Requirement | 能力边界，不再开发，不受支持 | 同上 |
| H03 | 随机生成谱面或随机判定 | 能力边界，不再开发，不受支持 | 同上 |
| H04 | 墙钟依赖 | 能力边界，不再开发，不受支持 | 同上 |
| H05 | IO / 网络直接参与判定 | 能力边界，不再开发，不受支持 | 同上 |
| H06 | Beat Saber 类三维物理斩击 | 未来开发，现不支持 | `capability.future_development` |
| H07 | Rocksmith 类连续音高 | 能力边界，不再开发，不受支持 | `capability.permanently_unsupported` |
| H08 | 任意自由体感传感器融合 | 未来开发，现不支持 | `capability.future_development` |

## 6. W 类缺口登记表（本轮定义）

[Stage 7A 计划](../../stage_plans/active/stage-07/plan.md) 曾在正文使用"W 类缺口"，而**仓库内没有任何
文档定义它的编号、字段或登记位置**（该缺陷登记为 **D-3**，见
[RULING_WORKSHEET.md](RULING_WORKSHEET.md) 第 1 轮 D-3 行；引用的计划行号以缺陷登记时的版本为准）。
**D-3 落地后的分工（2026-10-03 第 3 轮裁定）：** 字段定义与具体登记项**以本节为权威**；Stage plan §5.3
规定 W 类缺口的定义、登记和引用规则；V2 Spec 仅引用，**不设登记表附录**。

定义：W 类缺口是**已知无法在现有 L1-L4 与格式层安全表达、且尚未形成 capability 提案**的
玩法或格式需求。它与 capability 的区别是：capability 已有明确归属与预算路径，W 类还没有。

候选字段：

```text
wId                  稳定编号（W-01 起）
statement            一句话缺口描述
unableLayer          无法表达的层：L1 | L2 | L3 | L4 | format | tooling
affectedCaseIds      表达力压测案例编号（如 C13、F04）
blockedBy            阻塞它的合同项（如 CM-C05、CM-P04）
attemptedWorkarounds 已尝试的现有表达方式与失败原因
stableRejectCode     命中该缺口时的稳定拒绝码
ownerDecision        pending | accepted-as-gap | promoted-to-capability | rejected
reviewDate           最近一次复核日期
```

登记位置：**字段定义与具体登记项以本节为权威；Stage plan §5.3 规定 W 类缺口的定义、登记和引用规则；
V2 Spec 仅引用，不设登记表附录**。
研究稿候选（**编号仅为候选标识，尚未登记**，字段缺项与门禁归属见下表后的说明）：

| wId | statement | unableLayer | affectedCaseIds | blockedBy |
| --- | --- | --- | --- | --- |
| W-01 | 同 Tick 内 "先结束旧 lease 还是先求和弦" 的规范序 | L3 | C13 | CM-C05 |
| W-02 | 迟到事件重开未提交窗口的 epoch / 重开上限 / Replay 记录 | L1 + L3 | F04 | CM-T08 |
| W-03 | 未展开的循环关系（A after B after A、长度未知的重复节奏） | L2 + format | B 组多条 | CM-C10 |
| W-04 | 跨 Requirement 的同指/交替/共享资源约束 | L3 | C 组多条 | CM-C11 |
| W-05 | 判定几何与表现几何的边界（哪些字段进 judgement closure） | L2 + format | D 组多条 | CM-I06 |

### 6.1 W-01…W-05 的地位与门禁归属（2026-10-03 第 3 轮裁定）

上表 5 行是**研究稿候选**，其编号**仅为候选标识**，尚**不是**已登记的 W 类缺口，**不得作为已登记
`wId` 引用**。表中未给出 `attemptedWorkarounds`、`stableRejectCode`、`ownerDecision` 与实际 `reviewDate`
四项字段，本文件**不代填、不预填拒绝码或日期**：具体条目**首次被某批次引用或用于稳定拒绝前**，由**该批次**
补全全部字段（含 `attemptedWorkarounds`、`stableRejectCode`、`ownerDecision` 与实际 `reviewDate`），并按
`S1-03` 的口径标注**裁决编号 + 首次消费批次**。provenance：thread `s7a3-prepare-rulings`，补答卡 verdict
`reject`，confidence 0.88，日期 2026-10-03；带日期的落地记录见
[第 3 轮门禁报告](../../stage_reports/stages/stage-07/2026-10-03-s7a-3-gate-rulings.md)。

候选项的**门禁归属按首次实际消费判定**（登记在各候选项首次被消费的批次，而不是本表）：

| 候选项 | 门禁归属（首次实际消费批次） |
| --- | --- |
| W-01 | 运行期 lease / 和弦定序批次 |
| W-02 | 先核对**已闭合的 F-04**；**只有剩余重开语义被消费时**才登记 |
| W-03 | S7A-3 的**未展开循环表示的拒绝边界** |
| W-04 | **首次消费跨 Requirement 关系**的批次 |
| W-05 | **首次将判定几何纳入 closure** 的批次 |

**S7A-3 若具体消费 `W-03` / `W-04` / `W-05`，须先完成相应记录；不消费的候选项不构成该批次门禁。**

## 7. capability 派生来源（未决）

[Alignment Review P0-14](../research/gameplay-v2/CHART_V5_ALIGNMENT_REVIEW.md) 指出五处来源并存：
Packed META `requiredFeatures`、CXT v2 `requiredExtensions`、Chart v5 `gameplay.capabilities`、
Stage 7 本地 registry、Session capability 协商。建议的派生链（**待 owner 接受**）：

```text
source（Chart v5 / CXT v2）      只声明必需的最低 capability / profile
  -> 离线 assembler              从 canonical graph、Ruleset、presentation closure 派生完整闭包
  -> Packed header               保存排序后的 derived closure
  -> CXC manifest                复制 artifact-required capability，供早期拒绝
  -> Session                     只报告四态，不改写 graph
```

不一致处置（候选）：声明 **少于**派生 → prepare 失败，或由 compiler 补齐并记录；
声明 **多于**派生 → 拒绝或标记为可选。任何情况都不得让不同入口对同一 source 得到不同 identity。

## 8. 与 Stage 8 发行矩阵的关系

Stage 8 只消费其发行矩阵中显式选入的 capability，且必须同时满足计划 §7 的六项条件。
本文件不构成选入；§2 的 included 项指 **Stage 7A 范围**，不是 Stage 8 发行决定。

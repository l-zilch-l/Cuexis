# 批次证据门禁表（S7A-0.5）

状态：candidate；S7A-0 准入产出，待 owner acceptance

更新日期：2026-10-04

上级文档：[Gameplay V2 acceptance package](README.md) ·
[Stage 7A 实施计划](../../stage_plans/active/stage-07/plan.md) §3 与 §3.1

文档角色：批次门禁表征。计划 §S7A-0 工作内容 5 要求"为每个后续批次指定正例、负例、诊断类别、
golden、执行命令、预期证据文件和停止条件"。本文件逐批落实该要求；命令中的 target 与测试名
在实现批次建立之前是**候选名**，必须在对应批次内先冻结再运行。

## 0. 共同规则

1. 每个批次必须单独记录实现 SHA、测试 fixture、正例、负例、命令、原始输出和未覆盖项；
   "依赖批次已完成"不能替代本行核验（计划 §3.1）。
2. 证据文件落在 `docs/stage_reports/stages/stage-07/` 下，命名为
   `<日期>-s7a-<批次>-<主题>.md` 与（机器可读）`*.json`。
3. 本地证据与 hosted 证据、离线/UI 证据与真实设备/GPU 证据、阈值与实测必须分开记录。
4. 停止条件命中时立即停止该批次，保留最小复现，回滚到最后接受的合同 revision，
   不得在代码中隐式选择替代语义（计划 §1.1、§9）。
5. 研究性 spike、手写条目、单平台结果和只测正例的 golden 都不能单独退出（计划 §5.2）。

## 1. 批次门禁

### S7A-0 实施准入、基线和合同表征

| 项 | 内容 |
| --- | --- |
| 正例 | 本 package 的 9 份文档可被 `tools/check_docs.py` 接受；基线命令可重复得到同类结果 |
| 负例 | 基线报告中的失败命令（首次 configure 被文件权限拒绝）如实保留；不得改写成成功 |
| 诊断类别 | 不适用（无运行期诊断） |
| golden | 无；基线只记录事实 |
| 命令 | `cmake --preset debug --fresh`、`cmake --build --preset debug`、`ctest --preset debug --no-tests=error`、`python -B tools/check_docs.py`、`git diff --check`、`python -B tools/update_version.py --check` |
| 证据文件 | `2026-10-02-s7a-0-baseline.md` |
| 停止条件 | owner 未接受 V2 acceptance package；或存在"实施时再决定"的公共字段 |

### S7A-1 Typed Kernel 边界和模块骨架

| 项 | 内容 |
| --- | --- |
| 正例 | 空会话 create→prepare→submit→advance→query→snapshot→seek→reset 全链路；外部 consumer 可编译 |
| 负例 | prepare 失败不产生半准备会话；运行期修改 requirement 集合/preparedGrace/仲裁策略被拒绝；旧 active 状态保持 |
| 诊断类别（Gameplay V2 九类；**本行不再使用 Gameplay I 的旧类名**，口径见下） | `invalid_relation`、`capability_disabled`、`budget_exceeded`、`session.mutation_rejected` |
| golden | 公共头泄漏扫描（SDL/OpenGL/EnTT/World/JSON DOM/第三方实现类型）+ 非 ASCII 扫描通过 |
| 命令 | `cmake --preset debug`、`ctest --preset debug -R cuexis_architecture_tests`、`ctest --preset debug -R cuexis_judgement_tests`、`headless-debug` configure |
| 证据文件 | `*-s7a-1-kernel-boundary.md` |
| 停止条件 | 公共头出现第三方/后端类型；或 target allowlist 无法表达所需依赖 |

**S7A-1 诊断类别的九类口径（2026-10-03）。** 上表的诊断类别曾在 Gameplay I 时期写作 `identity_mismatch` /
`unsupported_capability` / `invalid_requirement` / `time_regression` 等旧类名；那些名字**不是** Gameplay V2
的 `category` 取值，**本行的类别口径**不再使用它们。类别口径以 **Spec §9.2 九类**与 **Spec §9.3 映射表**为准；
Gameplay I 旧类名到 V2 九类的 **old→new 合并映射**由**仍未关闭的 `CM-D02`** 统一冻结，本文件**不预先发布**
任何推导映射表（`CM-D02` 的登记见 [RULING_WORKSHEET](RULING_WORKSHEET.md) 与
[CONTRACT_MATRIX](CONTRACT_MATRIX.md)；其状态见集中码表 `ownerReviewRequired`）。

**后续门禁行的旧类名与通配族是未冻结的历史占位（2026-10-03）。** 本文件**后续批次行**的诊断类别栏里仍出现的
Gameplay I 旧类名与通配族名——**S7A-2 / S7A-3 / S7A-4 / S7A-5 / S7A-6 / S7A-7 / S7A-8 各批次的"诊断类别"
行**（本注记按**批次行名**引用而**不**按行号引用：本文件一经增删行号即移位，见
[docs/DOCUMENTATION_POLICY.md](../../DOCUMENTATION_POLICY.md) 的"行号锚只作带日期的定版证据"规则），例如
`identity_mismatch` / `time_regression` / `invalid_requirement` / `capability.disabled` /
`replay_format_error` / `budget.content.*` / `budget.steady.*` / `budget.*` / `ruleset.*`——都是
**未冻结的历史占位**：它们**不得**作为 Gameplay V2 的 `category` 取值，**也不得**作为该批次**放行依据**，
待对应裁定（`CM-D02` 及其余未关闭项）后再替换为 Spec §9.2 九类中的取值。本次编辑时刻（2026-10-03）实测
上述行为 S7A-2 L82 / S7A-3 L94 / S7A-4 L133 / S7A-5 L168 / S7A-6 L180 / S7A-7 L192 / S7A-8 L204——
该行号只是**当日观测**，随本文件增删即失效，**不得作为引用依据**。（S7A-9 的"全部既有类别回归"行同样以本注记为准。）

集中码表已把其中**部分**登记为未注册遗留项（`pendingRegistration[16]` / `[17]`），但**并未完全覆盖**：
`[16].family` 只覆盖 `budget.*`、`[17]` 只覆盖 Gameplay I 的十类旧名，**`ruleset.*` 落在两者之外**。该覆盖
缺口已如实登记在机器表 `pendingRegistration[16].alsoObservedFamilies`（纯事实字段，**不表示已注册**）与
[第 6 轮诊断分类学后续补齐记录](../../stage_reports/stages/stage-07/2026-10-03-s7a-diagnostics-taxonomy-rulings.md)
§11.11。**本文件不据此声称这些名字或通配族已被覆盖、已被登记或已获放行。**


### S7A-2 输入规范化、映射和时钟边界

| 项 | 内容 |
| --- | --- |
| 正例 | 键盘/鼠标/触摸/手柄/外设的最小映射 fixture；离散 press/release/update 规范字节；同设备类别在声明能力内的格点一致 |
| 负例 | 未知 device/action/channel、重复绑定、越界量程、时间回退、重复 sequence、负时间、跨 discontinuity、连续轨迹/最低上报率/reconstruction 声明 |
| 诊断类别 | `input.continuous_unsupported`、`capability.disabled`、`time_regression`、`invalid_requirement`、`late.reopen_unsupported` |
| golden | 规范事件字节跨 GCC/clang/MSVC 逐字节一致；同一设备类别的 press/release/update golden |
| 命令 | `ctest --preset debug -R cuexis_judgement_tests`、`ctest --preset release -R cuexis_judgement_tests`、跨工具链对照脚本 |
| 证据文件 | `*-s7a-2-input-normalization.md`、`*-s7a-2-canonical-events.json` |
| 停止条件 | CM-T03/CM-T04/CM-T08 未获 owner 决策；或跨编译器出现字节差异 |

### S7A-3 Requirement prepare 与离线 typed assembler

| 项 | 内容 |
| --- | --- |
| 正例 | typed file/memory source 一致性；CXT candidate 正例；inline 与 CXT emission 得到同一 canonical graph；数组/参数/引用顺序置换后 canonical bytes 不变 |
| 负例 | 缺字段、非法引用、未知 capability、Pattern 不终止、范围溢出、资源冲突、预算超限、identity collision；`sameContact`/跨 Requirement relation；以及 §S7A-3 不得消费清单（旧 revision 的隐式 V2 解释、旧 `REQ0`/`CNS0` 对新 kind 的兼容解释、未定的新 wire 布局或编号、未显式 lowering 的 Gameplay `effects`、隐式 Release/tail、`boundedRelationInstance`、运行期 repeat 计数 / 动态 Requirement、连续轨迹、跨 Requirement resource migration、handoff Hold、接触跟随 Slider） |
| 诊断类别 | `budget.content.*`、`pattern.relation_unsupported`、`non_terminating_source`、`invalid_relation`、`identity_closure_incomplete`、`migration.ambiguous`；**另加** 2026-10-02T21:51:38.591Z 裁定把下列六个码的**首次消费**登记为 S7A-3：`capability.budget_insufficient`（`budget_exceeded`）、`capability.revision_mismatch`（`unknown_capability`）、`capability.permanently_unsupported`（`capability_disabled`）、`capability.future_development`（`capability_disabled`）、`input.direction_unsupported`（`capability_disabled`，映射既有 **R-06**）、`format.gameplay_version_unsupported`（`ambiguous_migration`，**R-17**） |
| golden | Pattern 参考谓词与编译产物差分；短轨迹穷举；宽整数范围证明（平方/乘法/加法/窄化）；identity 规范字节的跨工具链 golden + 输入置换 + file/memory 对照 |
| 命令 | `ctest --preset debug -R cuexis_judgement_tests`、`cuexis_chart_candidate <fixture> --report <json>` |
| 证据文件 | `*-s7a-3-prepare.md`、`*-s7a-3-capacity.json`、`*-s7a-3-closure.json` |
| 停止条件 | 第 3 轮门禁未落地（Q-05 / Q-07 / Q-18 / CM-C10 的语义与 identity 规范字节边界），或把 `CM-X05` 当作阻塞项（**D-3 已关闭；仅具体 W 缺口在首次消费前须完成登记**；S7A-3 若具体消费 `W-03`/`W-04`/`W-05` 须先完成相应记录），或把**未获授权**的 wire / codec 当作已冻结，或 research 切片值被写成生产限额；2026-10-04 Capsule candidate 物理合同为明确授权例外，范围见下段 |

**2026-10-04 S7A-3 设计收口。** 组合 G2-S2 + G2-T4 + R2-K4 + P1-W1 + E1 已裁决，
语义见 Spec §3.8.10–§3.8.12；授权的首次 Packed candidate 物理合同仅见
[Gameplay Capsule v2 format](../../formats/GAMEPLAY_CAPSULE_V2_FORMAT.md)，完整 E1 清单见其 §11。
上表的“未定 wire”负例只针对该范围之外的未授权编码；Capsule 不授权 runtime resource state、
Snapshot/Replay/FactId/CommitId codec 或新预算数值。Reader/Writer 与全部 E1 实测完成前不关闭
S7A-3；包含性 gateIncomplete 的原子拒绝与状态预算 INCOMPLETE GATE 仍独立。

**第 3 轮裁定后的 S7A-3 门禁（2026-10-03）。** `CM-X05` **不再阻塞 S7A-3**（D-3 已落地关闭）；`CM-C10`
已由第 3 轮裁定并转为 `accept`（prepare-time 有界循环**完全展开**，展开结果唯一决定 identity / snapshot /
fact order）。Q-05 / Q-07 / Q-18 与 **identity 规范字节边界**登记为**阻塞 S7A-3 的门禁**（其中 identity 的
**编码实现**须在 S7A-3 消费前闭合）；`boundedRelationInstance` 候选表示与
[SUPPORT §6.1](SUPPORT_AND_REJECTION_MATRIX.md) 的 `W-01…W-05` 候选项只阻塞**首次拟消费它**的后续批次；
**S7A-3 若具体消费 `W-03` / `W-04` / `W-05`，须先完成相应记录**，不消费的候选项不构成该批次门禁。
provenance：thread `s7a3-prepare-rulings`，三卡 verdict `need_info` 0.8 / `reject` 0.88 / 计数确认卡 `adopt`
0.99，2026-10-03；带日期证据见
[第 3 轮门禁报告](../../stage_reports/stages/stage-07/2026-10-03-s7a-3-gate-rulings.md)。

**第 7 轮追加的 S7A-3 准入门禁（2026-10-03）。** 第 7 轮另把四项裁定登记为 S7A-3 的**准入门禁**，语义正文见
[Spec §3.8.7](../../formats/GAMEPLAY_V2_SPEC.md)（判定域 typed contract、CXT local relation 全局合并规则、
Chart v5 inline 与 CXT v2 emission 等价性）与 [Spec §5.1.1](../../formats/GAMEPLAY_V2_SPEC.md)（三项命名统一）：
① **`P1-01`**（`S7A7-R01`）——7A 只接受静态 typed 判定域记录；几何属 Gameplay closure；坐标系与量程在判定域内
声明；与 Presentation transform **完全隔离**；动态 frame 为 7B+ 候选且 7A 稳定拒绝。
② **`P1-02`**（`S7A7-R02`）——CXT local relation 的合并在 **prepare 编译期**完成；**稳定 ID = 来源文档
identity + 声明序号**；同名**拒绝**（不合并）；跨 invocation 引用**必须显式**；按稳定 ID 排序使**重排不改变
合并结果**。
③ **`P1-15`**（`S7A7-R06`）——两路 authoring 的**等价 = canonical graph 与 derived capability closure 逐字段
相等**；忽略 `sourceMap` 与物理顺序；**逐字段 semantic diff 作为 S7A-3 的 golden 证据**。
④ **`P2-07`**（`S7A7-R08`）——三项命名统一为 **`source closure` / `diagnostic map` / `content-artifact
identity`**（**不新增 ABI 类型行**，ABI 仍 9 域 / **126 = 96 + 30**）。
**四项均不新增工作项**，只把既有验证项必须满足的语义边界写明；`CM-X05` 的 W 缺口登记仍按
[SUPPORT §6.1](SUPPORT_AND_REJECTION_MATRIX.md) 在首次消费前完成。provenance：主卡 thread
`01a0fe61-b7a3-7853-a380-0793fee14e14`（`adopt` 0.96）+ 口径确认卡 `adopt` 0.99；带日期证据见
[第 7 轮收尾裁定与缺陷处置记录](../../stage_reports/stages/stage-07/2026-10-03-s7a-7-wrapup-rulings.md)。

### S7A-4 Judgement Kernel 生命周期、Tap/Hold/Release 和仲裁

| 项 | 内容 |
| --- | --- |
| 正例 | Tap/Hold/Release/tail 全边界矩阵；`[start,end)`、严格边界、重复输入、迟到输入、stray、Miss；单 resource 冲突；观测/消费/claim；终止后槽位复用；**唯一映射 trace**（六步外层 + 第 4 步内层八阶段，第 3/4/5/6 步与八阶段第 1/2–7/8 步一一对应）；`observe` **不占用也不改变资源 owner**；`contact end` 不复用旧 handle（新接触取得新 handle）；P1-12 边界（Exact 由窗口半宽定义、窗口外早击不产生 Fact 但产生诊断、Miss/absence 由 deadline 产生 Fact、Hold head/body/tail error 分别记录不合并）；`SolverProfile` 的字段语义、`objective`/`tieBreak` 有序语义列表与 prepare 拒绝语义 |
| 负例 | `capacity > 1`、owner 集合、并列 slot、`gap`、`handoff_pending`、非零资源 `declaredGapGrace`、Chord、handoff、多资源 fanout、跨 owner handoff、全局最优 solver；**把 runtime 行为当作 solver**（runtime 只执行 prepared profile，名称 `coordinator.policy.greedy_v1`）；`max_cardinality`、bounded backtracking；**把 `observe` 实现成占用资源**；以及 §3.13 不得消费清单其余各项（`sameContact`、连续轨迹、`boundedRelationInstance`、运行时脚本/IO/随机/墙钟、默认数值限额、未冻结枚举或整数 typedef、未授权序列化字节布局）；Requirement preparedGrace 不在资源 gap 拒绝清单 |
| 诊断类别 | `resource.capacity_unsupported`、`resource.handoff_unsupported`、`coordination.relation_unsupported`、`solver.profile_unsupported`、`budget.steady.*`；窗口外早击与 prepare 歧义诊断复用 §9.2 / §9.3 的既有码（**不新增 R 编号**，清单仍 19 条） |
| golden | 固定六步 Tick 顺序 trace（外层）；同 Tick 输入的候选/提交结果；**八阶段内层 trace 与唯一映射表逐行对应**；contact handle 不复用与 `observe` 不占资源的差分；渲染帧率、World 遍历与后端替换不改变 Fact |
| 命令 | `ctest --preset debug -R cuexis_judgement_tests`、`ctest --preset debug -R cuexis_judgement_capacity` |
| 证据文件 | `*-s7a-4-kernel.md`、`*-s7a-4-steady.json` |
| 停止条件 | 第 4 轮门禁未落地（CM-C05 / CM-C09 / Q-03 / Q-11 / Q-16 / P1-12 的语义与 CM-C06 / CM-C07 的语义闭合未落进 Spec §3.9–§3.13 与 ABI 域 4 / 域 5 / 域 7），或消费 §3.13 不得消费清单中的任何一项，或把默认数值限额 / 默认 profile 清单 / proof 编码 / wire / serialization / `terminal` 编码当作已冻结 |

**第 4 轮裁定后的 S7A-4 门禁（2026-10-03）。** `CM-C05`、`CM-C09` 已由第 4 轮裁定并转为 `accept`
（`CM-C05` 随 `Q-16` 的唯一六步 / 八阶段映射关闭；`CM-C09` 随 `Q-11` 关闭，7A 资源子集冻结为
`free` / `held` / `terminal`，`gap` / `handoff_pending` / 非零资源 `declaredGapGrace` / handoff / `capacity > 1` /
owner 集合 / 并列 slot 稳定拒绝）；Q-03、Q-16、P1-12 与 `CM-C06` / `CM-C07` 的**语义闭合**
登记为**阻塞 S7A-4 的门禁**（落点 Spec §3.9–§3.13、§S7A-4 节与 ABI 域 4 / 域 5 / 域 7）。
**具体预算数值、默认 profile 清单、proof 编码、wire / serialization、`terminal` 编码，
以及后续 `gap` / handoff / `capacity > 1` 语义**只阻塞**首次消费它们的后续批次**
（数值与默认列表记 **S7A-9**；后续 `gap` / handoff / `capacity > 1` 属 **S7B+ / S7C** 候选），
**不是** S7A-4 门禁。**第 4 轮不新增 R 编号**，§7.2 的稳定拒绝清单仍为 **19 条**。provenance：thread
`s7a4-arbitration-rulings`，主卡 `adopt` 0.97 / 处置词与计数确认卡 `adopt` 0.99，2026-10-03；
带日期证据见
[第 4 轮门禁报告](../../stage_reports/stages/stage-07/2026-10-03-s7a-4-gate-rulings.md)。

**第 2 轮实现复核补入的 S7A-4 门禁：late-policy 参数量级关系的校验（2026-10-03）。**
第 2 轮只冻结 late-policy 参数的**类型与角色**；`validatePrepare` 目前**只**检查参数"已声明且已测量"，
**不校验** `finalizationWatermark` / queue hop / 窗口开闭阈值之间的**量级关系**（序关系、上界与相互
一致性）。该量级关系的校验登记为**阻塞 S7A-4 的 late-window / 判定消费门禁**：S7A-4 在按 late policy
消费窗口与判定之前必须补齐该校验；**具体实测数值与最终限额**仍由 **S7A-9 的硬化与测量门禁**承载。
本项**不阻塞 S7A-2**，也**不新增 R 编号**（§7.2 仍 19 条、§9.2 仍九类）。落点：Spec §3.7.4 第 5 条、
§3.7.7 表与 §9.3；provenance：第 2 轮实现复核的两张续裁卡（thread `s7a1-freeze-bindings`，verdict
`adopt`，confidence 0.98 / 0.99，2026-10-03）；带日期证据见
[第 2 轮实现裁定记录](../../stage_reports/stages/stage-07/2026-10-03-s7a-2-implementation-rulings.md)。

### S7A-5 Ruleset Fold、Score、Combo 和 Statistics

| 项 | 内容 |
| --- | --- |
| 正例 | 同 Tick 多 Fact 顺序置换结果稳定；模块顺序变更、Hook 冲突（下一 Tick 可见）、信号延迟、combo 断连、长局累计/饱和；三阶段提交的成功路径（Fact 先封存 → state 原子提交 → 只读投影）；`phasePriority` 只影响事实序而不影响任何评分 |
| 负例 | StateDelta 冲突/溢出/预算失败；Ruleset 缺 capability、未知 outcome、非法 owner；`RegisterKind` 三类之外的 kind、重复 contribution identity、非交换 / 非结合合成、`exclusive` 的第二 owner / 第二写入、`ledger_derived` 的直接 StateDelta 写入；**不得消费清单逐项**：ingress 排序（含 `ingressSequence` / 容器顺序 / 线程完成顺序）、correction、Ruleset package（含 hash / manifest / 迁移矩阵）、Life（capability / policy / `LifeState` 初值）、默认 grade 表与默认数值限额、未冻结字节编码、未测量 seek 数值、表现资源 / UI / 默认绑定对 judgement 的影响 |
| 诊断类别 | `ruleset.*`、`budget.steady.*`、`fact.correction_unsupported`、`invalid_relation`、`capability.disabled`（Life）、`ruleset.package_unsupported`（package 输入） |
| golden | 失败事务保留旧 state、Fact 保留、RuleEffect 不可见、PresentationEvent 可由已提交 Fact 重建；**`faulted` session 的 `submit` / `advance` / `seek` / `replay` / 就地 `reload` 与 `snapshot` 一律稳定失败**，且只有显式 reset 或创建替换新 session 的 reload / recovery 可离开；grade 缺失时只报 Outcome 且不隐式升级；reset 不产生 Fact |
| 命令 | `ctest --preset debug -R cuexis_judgement_tests` |
| 证据文件 | `*-s7a-5-fold.md`、`*-s7a-5-fold.json` |
| 停止条件 | ① CM-S09（fault 后行为）/ CM-S10（outcome / grade 集合）未决——**两者均随第 5 轮裁定闭合（2026-10-03），本条不再阻塞**；② **消费了 Spec §3.22 的 S7A-5 不得消费清单任一项**；③ **把未测量数值或未冻结编码当作已冻结**（默认 grade 表、默认限额、`LifeState` 占位值、未测量 seek 数值、任何序列化字节布局）；④ 用默认值 / 临时 typedef / 伪成功实现绕过阻塞（S1-05） |

### S7A-6 Identity、Replay、Snapshot 和 Seek

| 项 | 内容 |
| --- | --- |
| 正例 | 录制/回放；跨进程/跨工具链 golden；不同快照间隔；任意 seek 点；pause/reload/discontinuity；长事件流；两套 header 字段集与 `SnapshotPayload` 十四条闭包项的无损往返（FactBinding 与 prepared immutable graph 重新取得、不重复保存） |
| 负例 | 非法版本、identity mismatch、乱序、时间回退、截断、重复、超事件数/字节数、解码超限、未知 capability；**`faulted` session 上新建 / 取得 snapshot**；**不得消费清单逐项**（S7A-5 的 8 项 + 另 5 项）：保存表现缓存 / Animation 临时值 / HostOverride token、保存未提交 delta、保存或恢复连续采样状态（连续重采样格点与采样相位）、保存 snapshot interval identity、把 typed byte-budget descriptor 解释为已接受的数值承诺 |
| 诊断类别 | `replay_format_error`、`identity_mismatch`、`budget.*`、`capability.disabled`、`ruleset.package_unsupported` |
| golden | Replay 与实时输入的 Fact/Result/Score/Combo/Statistics 逐项一致；Snapshot 恢复后 PresentationEvent 与连续播放一致；**`faulted` 不可新建 snapshot**；`factSemanticRevision` 进 engine identity 而 `stateSchemaRevision` 不进；类型 `EventCodecId` / 字段 `eventCodecId` 拼写统一；seek 逐位等于从起点运行且未把未测量数值写成约束 |
| 命令 | `ctest --preset debug -R cuexis_judgement_tests`、`ctest --preset debug -R cuexis_judgement_capacity` |
| 证据文件 | `*-s7a-6-snapshot-seek.md`、`*-s7a-6-replay.json` |
| 停止条件 | ① CM-K07（snapshot schema / revision）未决——**已随第 5 轮裁定闭合（2026-10-03）**；CM-P04（precedence 常量）**亦已随第 6 轮裁定闭合（2026-10-03，`S7A6-R03`）**，但它**不阻塞**本批次的 Snapshot / Replay 字段集与 §3.18 闭包，且**不得**在本批次新建表现 precedence 常量（S7A-6 不消费表现优先级语义）；② **消费了 Spec §3.22 的 S7A-6 不得消费清单任一项**；③ **把未测量数值（预算 / `maxSeekLatency`）或未冻结编码（两套 header 字节布局、Event / Fact codec 编码、`FactId` / `CommitId` 编码）当作已冻结**；④ 以增量 snapshot / 压缩 ledger / 跨 minor 迁移绕过全量 golden |

### S7A-7 Playback、Chart candidate、Headless、Player 与 external consumer 集成

| 项 | 内容 |
| --- | --- |
| 正例 | v4、v5 candidate、`packed-chart`/`gameplay-graph`（另加 `author-source` 只作审查 / 迁移 / 复现输入）、file/memory source、Playback/Headless/Player/external consumer 的生命周期矩阵；early/exact/late/Miss bridge；**`playback = true` 显式携带 `entryKind`** 且 manifest 记录 entry kind / compiled semantic identity / artifact identity / Ruleset binding / capability closure / resource·presentation closure / `sourceOf`；**`compiledSemanticIdentity` 与完整 capability closure 同时携带且可比较**；**缺 Presentation 的 headless judgement 路径**；**表现桥接**：`GameplayOverride` 命名层 + `Initial < Behavior < Animation < GameplayOverride/FactBinding < StudioPreviewOverride` precedence 常量、`render.visible = false` 作为该层显式值、adapter 只消费已提交 FactBinding、当前有效立即应用 / 未来 `effectivePresentationTick` 排队、`(factId, targetId)` 幂等、seek/replay/reload 从 Fact Ledger 重建 token、lifetime 到期或 reset / session replacement 时终止；**`aggregation` 三值（`any` / `all` / `groupCommit`）的幂等与部分提交 golden** |
| 负例 | 无输入纯播放行为变化；后端类型泄漏；错误入口；缺 metadata；**`author-source` 被当作 Playback entry**；**Ruleset package 被当作第三种 entry**；**判定闭包内 target / domain / action / table 悬空却只警告不失败**；**纯表现 target 缺失时 fault session 或回写 Fact Ledger**；**`groupCommit` 部分提交被回滚或 correction 影响已提交窗口**；**诊断码字符串未经码表登记与 CTest 校验即进入公共 ABI**；**输入 / 几何三码被重复定义或扩展**；**未命名空间化的 `extensions`**；**不得消费清单逐项**：具体预算 / `maxSeekLatency` 数值、未冻结的 wire / codec / `FactId` / `CommitId` 字节编码、Ruleset package 形状 / identity / 迁移、Presentation 缓存或 HostOverride token 的 snapshot 持久化、未提交 delta 与连续采样相位、运行时脚本 / 逐帧回调、Life capability 或任何第二判定路径、把语义字段的"未知"降级为默认值 |
| 诊断类别 | `capability.unknown`、`geometry.inference_rejected`、`identity_mismatch`、`identity_closure_incomplete`、`invalid_relation`、**`presentation-target-missing`**、**`partial-group`**（后三条须先在 `schemas/cuexis.gameplay-diagnostics.v2.codes.json` 登记并通过 CTest 校验项后才可进入公共 ABI） |
| golden | Graph/Packed semantic equivalence；无输入时 FrameSnapshot/FrameDigest 不变；headless 与 Player/实时路径 golden 一致；**同一内容在 headless 与渲染两条路径下判定逐项一致**（缺 Presentation 只减少投影、不改判定）；**seek / replay / reload 后从 Fact Ledger 重建的表现 token 与实时路径 golden 一致**；**重复应用同一 `(factId, targetId)` 产生一次效果**；**`groupCommit` 部分提交后的稳定 `partial-group` 诊断与稳定拒绝路径可复现**；**码表校验项（code 唯一性、category / severity / faulted 映射完整性、`stableRejectCode` 已登记、输入 / 几何三码不可重复扩展）通过** |
| 命令 | `ctest --preset debug -R cuexis_reference_host_staging`、`ctest --preset debug -R cuexis_external_consumer`、`ctest --preset headless-debug`、`ctest --preset debug -R cuexis_gameplay_diagnostics_codes`（**码表 CTest 校验项**：由工具侧在 `schemas/cuexis.gameplay-diagnostics.v2.codes.json` 旁落地，候选名 `cuexis_gameplay_diagnostics_codes`；落地前的正式名以工具侧 CMake 注册为准）、`cuexis_player.exe --smoke-test`（需 GPU） |
| 证据文件 | `*-s7a-7-integration.md`、`*-s7a-7-integration.json` |
| 停止条件 | ① 判定类型倒灌 Runtime/Presentation；或 v4 回退/无输入行为被改变；② **消费了本文件"第 6 轮裁定后的 S7A-7 门禁"段所列不得消费清单任一项**；③ **把 `author-source` 或 Ruleset package 当作 Playback entry**；④ **把四层诊断模型、九类 category 或集中码表改写成第二套定义**（含绕过码表直接新增公共 ABI 诊断码字符串）；⑤ **把本轮裁定误写为"Judgement 实现完成"或"Stage 7A 完成"**；⑥ **把未冻结的字节编码 / 预算数值当作已冻结** |

### S7A-8 Stage 6 交接收口和版本门禁

| 项 | 内容 |
| --- | --- |
| 正例 | 默认 OFF 与显式 ON 的构建/安装/运行矩阵；离线 assembler 产出 closure report；同 SHA 命令循环回归；`0.7.0` consumer 兼容 |
| 负例 | 默认构建/安装不得加载 v5 candidate；错误 flavor、裸 Packed、缺 metadata 稳定拒绝；测试注入 feature 后门被检测；version gate 默认变更仍失败 |
| 诊断类别 | `capability.disabled`、`format.gameplay_version_unsupported` |
| golden | 命令状态矩阵；错误/旧 active 状态保持；一次显式放行的可审计记录 |
| 命令 | `cmake --preset debug-media-tools`（候选新增 preset）、`ctest --preset debug -R cuexis_reference_host_command_parser`、`python -B tools/check_version_gate_tests.py`、`python -B tools/check_version_gate.py --check-current`、`python -B tools/update_version.py --check` |
| 证据文件 | `*-s7a-8-handover.md`、`*-s7a-8-candidate-isolation.json` |
| 停止条件 | 四项中任一项既未实现也无 owner 接受的例外 |

### S7A-9 最终硬化、跨平台证据、Stage 8 handoff 和关闭

| 项 | 内容 |
| --- | --- |
| 正例 | Debug/Release/shared/headless/MinGW 本地矩阵；Linux Quality、Windows MSVC、Windows MinGW hosted 同 SHA |
| 负例 | sanitize/coverage/clang-tidy 在 MSVC 上不得被伪造成 Windows 等价证据；GPU/设备/音频项单列为未执行 |
| 诊断类别 | 全部既有类别回归 |
| golden | 架构、target allowlist、公共头泄漏、static/shared package、external consumer、version gate、docs、`git diff --check`、许可证检查 |
| 命令 | 见计划 §S7A-9.1；本地 `cmake --preset <p> --fresh` + `ctest --preset <p> --no-tests=error` |
| 证据文件 | `*-s7a-9-local-matrix.md`、`*-s7a-9-hosted.md`、`*-s7a-9-budget.json`、`*-s7a-9-regression.md`、`completion.md` |
| 停止条件 | 任一硬门禁未满足；此时状态保持 active/blocked，不缩小矩阵宣称完成 |

## 第 5 轮裁定后的 S7A-5 / S7A-6 门禁（2026-10-03）

**provenance。** 三张决策卡来自同一 Codex 话题 thread `01a0fe49-cfe5-7933-8d03-3d0a65d34c8b`（consult）：
主裁定卡 verdict `adopt`、confidence 0.95（`S7A5-R01…R11`）；处置词与计数确认卡 verdict `adopt`、
confidence 0.99（`CM-F06`、`CM-S08`、`CM-S10`、`CM-K07` 四条 `open` → `accept`；最终 `open` **10** /
`accept` **42**；总数仍 114）；ABI 状态格首词与追踪计数追问卡 verdict `adopt`、confidence 0.98（9 行状态格
首词仍为 `待冻结`；**126 = 96 + 30** 不变；**不新增 §未决项 #21、不新增 §类型清单条目**）。文件名时间戳为
2026-10-02T20:2x–20:3xZ（UTC），统一按**本地日期 2026-10-03** 登记。带日期证据见
[第 5 轮门禁报告](../../stage_reports/stages/stage-07/2026-10-03-s7a-5-6-gate-rulings.md)。语义正文见
[Spec §3.14–§3.22](../../formats/GAMEPLAY_V2_SPEC.md) 与 [ABI §S7A-5 / §S7A-6](../../api/GAMEPLAY_V2_ABI.md)。

| 批次 | 本批次必须验证的正例 | 必须覆盖的负例 | golden / 不变量 | 停止条件 |
| --- | --- | --- | --- | --- |
| **S7A-5** | 三阶段提交成功路径；同 Tick 多 Fact 顺序置换稳定；`RegisterKind` 三类的单 owner / 已声明合成 / 账本派生重建；Hook 下一 Tick 可见；grade 缺失时只报 Outcome | 第二阶段任一项校验失败（StateDelta 冲突 / 溢出 / 预算）；三类之外的 kind、重复 contribution identity、非交换非结合合成、`exclusive` 第二 owner、`ledger_derived` 直接写入；package 输入；Life capability / policy / `LifeState` 初值；**S7A-5 不得消费清单的全部 8 项** | 失败时**不追加 fault Fact**、**不发布** RuleEffect / PresentationEvent、**保留旧 state**；`faulted` 的 `submit` / `advance` / `seek` / `replay` / 就地 `reload` / `snapshot` **全部稳定失败**；仅显式 reset 或替换新 session 的 reload / recovery 可离开且不伪造未提交 StateDelta；reset 不产生 Fact | ① **消费 Spec §3.22 的 S7A-5 不得消费清单任一项**；② **把未测量的数值或未冻结的编码当作已冻结**；③ 用默认值 / 临时 typedef / 伪成功实现绕过阻塞（S1-05）；④ 未按 §3.9 第 3 条的规范总序排序（或使用 `ingressSequence` / 容器顺序 / 线程完成顺序） |
| **S7A-6** | 两套 header 字段集的无损往返；`SnapshotPayload` 十四条闭包项齐备；FactBinding 与 prepared immutable graph 重新取得、不重复保存；seek 逐位等于从起点运行；Replay 与实时路径逐项一致 | 非法版本、identity mismatch、乱序、时间回退、截断、重复、超事件数 / 字节数、解码超限、未知 capability；**`faulted` 上新建 / 取得 snapshot**；**S7A-6 不得消费清单的全部 13 项**（S7A-5 的 8 项 + 表现缓存 / Animation 临时值 / HostOverride token、未提交 delta、连续采样状态、snapshot interval identity、把 typed budget descriptor 当作已接受数值承诺） | `factSemanticRevision` 进 engine identity、`stateSchemaRevision` 不进；类型 `EventCodecId` / 字段 `eventCodecId` 拼写统一；`faulted` **不可新建 snapshot** 但闭包仍含 fault 诊断 / 状态；seek 仍满足无损正确性 | ① **消费 Spec §3.22 的 S7A-6 不得消费清单任一项**；② **把未测量数值（预算 / `maxSeekLatency`）或未冻结编码（两套 header 字节布局、Event / Fact codec、`FactId` / `CommitId` 编码）当作已冻结**；③ 以增量 snapshot / 压缩 ledger / 跨 minor 迁移绕过全量 golden；④ 在 `faulted` 上产出新 snapshot |

**两批共同的判定口径。** ①两批的**门禁已满足**（第 5 轮 13 行裁定落地、计数自洽：`open` 10 / `accept` 42 /
总数 114，§0.1 末列仍 21，§7.2 仍 19 条，ABI 仍 126 = 96 + 30）；②两批**都不新增** R 编号、§未决项条目或
ABI 类型行；③`P1-14`（`S7A5-R11`）**不阻塞**这两批——它的首次消费是 **S7A-3** 的 REF0 首次 Packed 写入，
entry / manifest 集成属 **S7A-7**；④具体预算数值与 `maxSeekLatency` 数值的首次接受在 **S7A-9**。

## 第 6 轮裁定后的 S7A-7 门禁（2026-10-03）

**provenance。** 单张决策卡，Codex 话题 thread `01a0fe61-3476-7882-9674-5f6b05035237`（consult，模型
`gpt-6-astra`）：verdict **`adopt`**，confidence **0.97**（第 6 轮 15 行 / 17 项裁定，编号 `S7A6-R01…R12`；
`CM-P04`、`CM-P06`、`CM-P08`、`CM-D04`、`CM-X01` 五条 `open` → `accept`；最终 `open` **5** /
`accept` **47**，总数仍 **114**）。文件名时间戳为 2026-10-02T20:51:47.167Z（UTC），统一按**本地日期
2026-10-03** 登记（与第 2、3、4、5 轮先例一致）。带日期证据见
[第 6 轮门禁报告](../../stage_reports/stages/stage-07/2026-10-03-s7a-6-gate-rulings.md)。语义正文见
[Spec §3.6 / §3.23–§3.25 / §5.6 / §9.6–§9.8 / §S7A-7](../../formats/GAMEPLAY_V2_SPEC.md) 与
[ABI 域 8 / 域 9 / §S7A-7](../../api/GAMEPLAY_V2_ABI.md)。

| 批次 | 本批次必须验证的正例 | 必须覆盖的负例 | golden / 不变量 | 停止条件 |
| --- | --- | --- | --- | --- |
| **S7A-7** | 见上表"正例"行：`entryKind` 显式与 manifest 七项记录、`compiledSemanticIdentity` + 完整 capability closure 同时携带且可比较、缺 Presentation 的 headless 路径、`GameplayOverride` precedence 常量与 adapter 生命周期、`(factId, targetId)` 幂等与 `groupCommit` 部分提交、码表 CTest 校验项通过 | 见上表"负例"行：`author-source` 被当作 Playback entry、Ruleset package 被当作第三种 entry、判定闭包悬空不失败、纯表现 target 缺失 fault session / 回写 Fact Ledger、`groupCommit` 部分提交被回滚、未登记诊断码进入公共 ABI、输入 / 几何三码被重复扩展、非命名空间化 `extensions`，以及**不得消费清单全部 11 项** | 同一内容在 headless 与渲染下**判定逐项一致**；seek / replay / reload 后表现 token 由 Fact Ledger 重建且与实时路径 golden 一致；重复 `(factId, targetId)` 只产生一次效果；`partial-group` 诊断与稳定拒绝路径可复现；**码表校验项（code 唯一性、category / severity / faulted 映射完整性、`stableRejectCode` 已登记、输入 / 几何三码不可重复扩展）通过** | ① **消费不得消费清单任一项**；② **把 `author-source` 或 Ruleset package 当作 Playback entry**；③ **把四层诊断模型 / 九类 category / 集中码表改写成第二套定义或绕过码表新增公共 ABI 诊断码**；④ **把未冻结的字节编码或预算数值当作已冻结**；⑤ **把本轮写为"Judgement 实现完成"或"Stage 7A 完成"** |

**S7A-7 不得消费清单（11 项，表示与行为）。** ① 具体预算数值；② `maxSeekLatency` 具体数值；③ 未冻结的
wire / codec 字节编码；④ 未冻结的 `FactId` / `CommitId` 字节编码；⑤ Ruleset package 形状 / identity /
迁移；⑥ Presentation 缓存或 HostOverride token 的 snapshot 持久化；⑦ 未提交 delta；⑧ 连续采样相位；
⑨ 运行时脚本 / 逐帧回调；⑩ Life capability 或任何第二判定路径；⑪ 把语义字段的"未知"降级为默认值。
该清单与第 2 / 4 / 5 轮清单不冲突（不重复、不放松）。

**门禁归属。** 第 6 轮 15 行 / 17 项**全部阻塞 S7A-7**（首次消费批次一律 S7A-7）；`P2-06` 的资源三分法
**沿用 S7A-4 已冻结的 `free` / `held` / `terminal` 子集**，**不新增资源状态**；本轮**不新增** R 编号、
§未决项条目或 ABI 类型行（§7.2 仍 **19 条**，ABI 仍 **126 = 96 + 30**）；**本轮只冻结文档合同与门禁**。

## 第 7 轮裁定后的收尾门禁（2026-10-03）

**provenance。** 第 7 轮由两张决策卡裁定，均来自 Codex（consult，模型 `gpt-6-astra`）：①**主卡** thread
`01a0fe61-b7a3-7853-a380-0793fee14e14`，verdict **`adopt`**，confidence **0.96**；②**口径确认卡**
（同议题小追问：三处计数 / 编号口径）thread `01a0fe61-3476-7882-9674-5f6b05035237`，verdict **`adopt`**，
confidence **0.99**。卡的文件名时间戳为 2026-10-02T20:52:37.188Z 与 2026-10-02T21:08:34.383Z（UTC），统一按
**本地日期 2026-10-03** 登记。带日期证据见
[第 7 轮收尾裁定与缺陷处置记录](../../stage_reports/stages/stage-07/2026-10-03-s7a-7-wrapup-rulings.md)。
**第 7 轮 = 16 行 / 18 项**（`S7A7-R01…R16`；`D-2 / D-7` 是合并行、`D-13` 的 `AGENTS.md`:110 附带子项各记
2 项）；逐轮合计 **76 行 / 84 项**，与 `RULING_WORKSHEET §0.1` 一致。**第 1–7 轮无待裁定轮次。**

| 批次 | 本批次必须验证的正例 | 必须覆盖的负例 | golden / 不变量 | 停止条件 |
| --- | --- | --- | --- | --- |
| **S7A-3** | typed prepare（`P1-01` 静态 typed 判定域记录、坐标系与量程在判定域内声明）；**Chart v5 inline 与 CXT v2 emission 的逐字段 semantic diff（`P1-15`）**；**REF0 首次写入**（`P1-14`，归属表见 Spec §3.8.6）；CXT local relation 的全局合并按稳定 ID 排序（`P1-02`）；三项命名统一 `source closure` / `diagnostic map` / `content-artifact identity`（`P2-07`） | 判定域继承 Presentation transform、几何落 presentation closure、动态 frame 被接受；同名 relation 被静默合并、隐式跨 invocation 引用被解析；两路 authoring 的 diff 非空；把 `diagnostic map` 当作 identity 分量 | 两路 authoring 的 canonical graph 与 derived capability closure **逐字段相等**；合并结果对输入 / 参数 / 引用顺序置换**不变**；REF0 内容与 §3.8.6 归属矩阵一致 | ① **用默认值 / 临时 typedef / 伪成功绕过** §3.8.5 的 13 项不得消费清单**任一项**；② **把动态 frame 或 Capsule 授权之外的 wire 表示当作已冻结**；③ 把本轮写为"Judgement 实现完成"或"Stage 7A 完成" |
| **S7A-4** | kernel 与 coordinator 边界（`P2-02`："**不解析/编译 solver；runtime coordinator 只执行已准备的确定性 policy**"）；六步 / 八阶段唯一映射；7A 资源子集 `free` / `held` / `terminal` | runtime 侧解析或编译 solver、把 runtime 行为称为 solver；`max_cardinality` / bounded backtracking / `capacity > 1` / `gap` / handoff / 非零资源 `declaredGapGrace` | `coordinator.policy.greedy_v1` 只消费 prepared profile；阶段顺序变更会改变 engine identity 或被拒绝 | ① **消费 §3.13 不得消费清单任一项**；② **把 `SolverProfile` 默认列表或 `max*` 数值当作已冻结** |
| **S7A-5 / S7A-6** | 生命周期、snapshot / replay、codec 与不得消费清单（Spec §3.14–§3.22、§3.18） | 半个 Tick 提交、`faulted` 不可查询、字节布局未冻结即声称已定 | 唯一规范事实总序、`faulted` 行为矩阵、`SnapshotPayload` 闭包 | ① **消费 §3.22 不得消费清单任一项**；② **把预算数值当作限额** |
| **S7A-7** | 集成（见上节第 6 轮门禁）：`entryKind` 与 manifest 七项、`compiledSemanticIdentity` + 完整 capability closure、缺 Presentation 的 headless 路径、`GameplayOverride` precedence、`(factId, targetId)` 幂等 | 同上一节的负例行；**且必须先满足集成前置** | 同上一节 golden 行 | ① **在 `CM-P04` / `CM-P06` / `CM-P08` / `CM-D04` / `CM-X01` 未闭合前开始集成**；② 消费 S7A-7 不得消费清单 11 项 |
| **S7A-8** | 候选 / 版本门禁、同 SHA 交接；`D-9` 的 owner-only 版本门禁 shell 守卫（视图一致性探针 + 显式拒绝 `System32\bash.exe` + 独立负例，覆盖 WSL 启动器 `bash.exe`） | 未覆盖 `bash.exe` 的守卫被当作已闭合；候选补丁未提交即宣称门禁通过 | 同 SHA 交接证据链完整；负例可复现 | ① **在 owner 按 ADR 0042 具名复核并把 `D-9` 补丁落到 `master` 前，不得宣称 S7A-8 版本门禁闭合**；② **`D-9` 是 owner-only，实现方不得自行落库** |
| **S7A-9** | 本地矩阵（Debug / Release / headless / MinGW）、hosted 同 SHA（Linux / MSVC / MinGW）、**五类预算实测**（内容 / 稳态 / 快照与 Seek / Replay 与解码 / Packed wire；`P1-10` 的"谁产生谁计数"与最坏值计数入口）、交接与 owner 接受 | 把研究观察值写成限额；用 `0` / "不限制"表示跳过；把五类合并成一个 `budget_exceeded` | **五类预算阈值另行单独接受**（Spec §8.5）；计数与最坏值入口可复现 | ① **在 S7A-9 实测并单独接受前冻结任何限额**；② 把环境限制项写成已通过 |

**跨批次文档门禁。** `python -B tools/check_docs.py`（exit 0）、`cuexis_format_check`（exit 0）、version
gate、architecture / allowlist 校验与 CTest 全部通过；`git diff --check` 与"新增 / 改动文件零行尾空白与
零制表符"通过。

**`D-9` 与 `D-12` 的收尾状态（不得提前关闭）。** `D-9`：候选补丁**已在工作区、未提交**，剩余动作是 owner
按 ADR 0042 **具名复核后落到 `master`**，再于 **S7A-8** 消费——**在此之前不得宣称 S7A-8 的版本门禁闭合**。
`D-12`：研究稿 8 处指向行**只登记、不编辑**，处置文本为"改指 ADR 0044 / V2 Spec / V2 ABI 并注明 Gameplay I
已 `superseded`，不得改写论证正文，**须先取得 owner 对历史稿修改的确认**"——**"待 owner 确认"是明确登记状态**。

**计数不变声明。** 五条残余 `open`（`CM-V06` / `CM-V07` / `CM-V13` / `CM-I06` / `CM-X06`）按第 1 轮裁定补齐为
`accept`，**`open` 集合为空**；`accept` **52** / `revise` 26 / `supersede` 17 / `retain` 17 / `open` **0** /
`reject` 2 = **114**；`§0.1` 仍 **76 行 / 84 项 / 末列 21**；§7.2 仍 **19 条**、§9.2 仍**九类**、ABI 仍
**9 域 / 126 = 96 + 30**、§未决项条目数不变、类型行**不新增**。**本轮只冻结文档合同与门禁**——**不得**写成
"Judgement 实现完成"或"Stage 7A 完成"（实现批次 S7A-3…S7A-9 仍未完成）。

## 2. 统一验收矩阵列（每个批次都要有）

| 维度 | 必须记录 |
| --- | --- |
| 合同 | Spec/ADR/ABI 版本、字段与 capability/revision |
| 正例 | 最小、边界、组合、真实内容、压力 fixture |
| 负例 | 未知值、越界、溢出、冲突、预算、身份、截断、非终止 |
| 确定性 | 输入/实例/容器顺序、跨编译器/平台、seek/replay 对照 |
| 生命周期 | prepare、submit、advance、pause、seek、reload、reset、销毁、失败事务 |
| 身份 | engine/ruleset/chart/session、content/prepared/replay identity 及变更原因 |
| 预算 | prepare 峰值、稳态 tick、activity、Fact、Replay、快照、Seek、decoded bytes |
| 边界 | headless、static/shared、external consumer、candidate/production、GPU/设备限制 |
| 证据 | 命令、SHA、配置、环境、原始日志/机器可读结果、报告路径、未执行项 |

## 3. 与计划 §3.1 小目标台账的对应

计划 §3.1 已把每个批次拆成可独立退出的小目标（S7A-0.1 … S7A-9.4）。本文件的批次门禁是
该台账的**横向补充**：小目标回答"这一行做什么、怎么算退出"，本文件回答"这一批用什么正例/负例/
诊断/golden/命令/证据验证"。两者都必须逐行核验，不能互相替代。

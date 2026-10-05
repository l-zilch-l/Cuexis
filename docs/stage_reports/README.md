# Stage Report Index

状态：current index

更新日期：2026-10-05

本轮设计交付：[S7A-3/4 完整 execution 方案与交接](stages/stage-07/2026-10-05-s7a-3-4-execution-design.md)
（2026-10-05，仅设计文档完成；新增合同与 revision3 未实施）。
后续复核：[S7A-3/4 剩余歧义审计与修订](stages/stage-07/2026-10-05-s7a-3-4-ambiguity-audit.md)
（同日较晚快照，登记 15 组实际缺项和修订，不表示运行验收）。

报告保存带日期的实施、审查和验证证据。报告中的“下一步”只代表其快照日期，不能重新定义
[CURRENT_STATUS.md](../CURRENT_STATUS.md)。

`reviews/<topic>/` 保存跨阶段专题复核。一篇复核可以包含多份正文（例如按审查轴拆分的
Standards / Spec 记录），由该专题自己的 `README.md` 承担导航；轴与轴之间保持独立，
不合并成一张表，也不跨轴重排。

## 按阶段归档

- Stage 0：[completion](stages/stage-00/completion.md)
- [Stage 1](stages/stage-01/README.md)
- Stage 2：[completion](stages/stage-02/completion.md)、[review](stages/stage-02/2026-08-06-review.md)
- Stage 3：[completion](stages/stage-03/completion.md)、[review](stages/stage-03/2026-08-08-review.md)
- [Stage Chart Format Update](chart-format-update/README.md)
- [Stage 4](stages/stage-04/README.md)
- [Stage 5](stages/stage-05/README.md)
- [Chart Format Foundation](stages/chart-format-foundation/README.md)
- [Stage 6](stages/stage-06/README.md)：[关闭报告](stages/stage-06/completion.md)（2026-09-27 关闭并归档）
- Stage 7A：[Gameplay V2 acceptance package：owner 接受记录](stages/stage-07/2026-10-02-s7a-0-acceptance.md)（2026-10-02，准入接受与冻结顺序）、
  [S7A-3 剩余实施与验收证据核对](stages/stage-07/2026-10-05-s7a-3-remaining-evidence.md)（2026-10-05，收窄过时未完成表述，核对既有实现及当前 SHA hosted，登记 authoring 双路、K4 affine、完整 E1 与独立状态预算余项；不关闭 S7A-3）、
  [S7A-0 执行基线与合同表征](stages/stage-07/2026-10-02-s7a-0-baseline.md)（2026-10-02，S7A-0 批次证据）、
  [S7A-0 Release 基线证据](stages/stage-07/2026-10-02-s7a-0-release-baseline.md)（2026-10-02，749/749 本地 Release 结论）、
  [D-9 候选补丁与验证证据](stages/stage-07/2026-10-02-d9-shell-guard-candidate.md)（2026-10-02，未提交的候选修复，待 owner 按 ADR 0042 复核）、
  [S7A-1 typed contract review](stages/stage-07/2026-10-02-s7a-1-typed-contract-review.md)（2026-10-02，冻结顺序第 4 步：条件性通过，含 4 项必须先裁定项）、
  [S7A-2 准入门禁：第 2 轮（时间域与迟到策略）裁定落地记录](stages/stage-07/2026-10-03-s7a-2-gate-rulings.md)（2026-10-03，7 条裁定、F-04 闭合、S7A-2 不得消费清单与门禁结果）、
  [S7A-3 准入门禁：第 3 轮（prepare、装配与 entry）裁定落地记录](stages/stage-07/2026-10-03-s7a-3-gate-rulings.md)（2026-10-03，6 行裁定、`CM-X05` 的 D-3 依据、计数变化、W 候选项门禁归属、identity 规范字节边界与 S7A-3 不得消费清单）、
  [S7A-4 准入门禁：第 4 轮（仲裁、资源与事实序）裁定落地记录](stages/stage-07/2026-10-03-s7a-4-gate-rulings.md)（2026-10-03，6 行裁定与 `SolverProfile` 语义闭合、唯一六步 / 八阶段映射、7A 资源子集与身份规则、solver / coordinator 边界、P1-12 早 / 晚判定、计数变化与 S7A-4 不得消费清单）、
  [S7A-5 / S7A-6 准入门禁：第 5 轮（Ruleset、事实序、Score 与 Snapshot）裁定落地记录](stages/stage-07/2026-10-03-s7a-5-6-gate-rulings.md)（2026-10-03，13 行裁定 / `S7A5-R01…R11`、唯一规范事实总序、Ruleset 三阶段与 `faulted`、`RegisterKind` 三类、两套 header 字段集与 `SnapshotPayload` 闭包、`SeekLatencyCommitment`、Outcome 与统计、Life 关闭、`P1-14` 归属表、计数变化与两批不得消费清单）、
  [S7A-2 时基实现裁定落地记录（第 2 轮实现复核修订）](stages/stage-07/2026-10-03-s7a-2-implementation-rulings.md)（2026-10-03，7 项裁量 + 4 项收尾裁定、`stop` 塌缩与精确累计函数 `F` 对第 2 轮语义的修订、三分法、迟到参数量级门禁与 Tick → Beat 反查登记、实现侧事实与门禁结果）、
  [S7A-2 输入规范化半批实现裁定落地记录（第 2 轮输入半批复核）](stages/stage-07/2026-10-03-s7a-2-input-implementation-rulings.md)（2026-10-03，6 处语义裁量 + 2 处死令牌与措辞对齐、`AmountSpec` 的 64 位 canonical 冻结与"可表示性先于范围"、三条原子失败映射、负 `observationTick` 合法与 `time_reversal` 类别修正、域声明进 session identity、不连续表示复用既有码、实现侧事实与计数不变声明）、
  [S7A-3 第二半 part 1 实现裁定落地记录（三轮实现复核修订）](stages/stage-07/2026-10-03-s7a-3-implementation-rulings.md)（2026-10-03，决策卡 thread `01a0fe91-416c-7f31-a48a-d1890a0e68d1` 的**三轮**实现复核逐条处置（第 1 轮 verdict `reject` 0.94、第 2 轮 verdict `reject` 0.91、第 3 轮 verdict `reject` 0.93）：空操作数一律拒绝被**驳回**并改为份数序 / 并集 / 不动点语义（`repeat(skip,1,4)` 接受 `{ε}`、`repeat(choice(a,ε),1,2)` 与显式式在长度 0–3 全部 15 个词上逐词一致且最小化状态数相等）、三项 Pattern 计数口径（完全展开计数按可允许份数求和并允许"测量缺口"、每分支上界 `maximum × 单份`、interned 子模式数、以及按 Spec §8.2 确定化并最小化后的状态数；三项计数一律遵守"**计数缺口不是拒绝理由**"——无可表示值时记为该维度的**测量缺口**（访问器缺席），只有**已接受**上限被**确实超过**才 `budget_exceeded`，第 1 轮"测不出即 `judgement.s7a3.pattern.state_count_not_measured`"的口径已被第 2 轮复核 `reject` **删除**）、`skip` 与 `instant` 的语言同一与身份区分、`complement` 的前缀消费与可组合语义、`maximumTraceLength` 缺席措辞（仅"本次测量无有限上界可报"，另把内部标志 `maximumUnbounded` 先更名 `maximumBoundUnavailable`、第 2 轮复审再更名为现行 **`maximumLengthUnavailable`**（同批 `branchOverflow` → **`largestBranchGap`**、`fullyExpandedOverflow` → **`fullyExpandedGap`**，并新增 **`minimumLengthGap`**；旧名在代码中已 0 命中））、arm / deadline 包含性门禁标记为**未完成**且必须检查**全部**可接受长度、**装配消费点（以真实 arm / deadline 容量调用门禁并验证原子失败）是进入 S7A-4 的硬前置**、状态预算门禁保持 **incomplete**、S7A-3 的 src-only 前缀**不得裁定一律永久免登记**（任何码获公共映射并对外承诺前须逐条登记并通过 CTest）、grade token 只携带不求值（`tap` / phase 派生类别 / grade token 仍进 identity 与 semantic diff）、诊断码相对本批次基线**净增 0 / 净删 2**（`…pattern.repeat_operand_matches_empty`、`…pattern.state_count_not_measured`、`…pattern.expansion_not_representable` 的增删账见正文 §15）、深 1024 的 Pattern 经 identity 与 `renderPatternNode` 递归改迭代、第 3 轮定案的三种状态数口径与 **`L + 1`** 下界（最长可接受轨迹长度 `L` 有限 ⇒ 状态数 ≥ `L + 1`；“份数 + 1”会误拒 `repeat(skip,1,UINT64_MAX)`）、**包含性保守拒绝与预算缺口分成两道独立门禁**、`maximumLengthGap` / `maximumLengthUnbounded` 两标志、Spec §3.8.4 / §3.8.8（新增）/ §8.2 同步与门禁结果；**只登记语义与门禁状态，不表示 S7A-3 或 Stage 7A 实现完成**）、
  [S7A-3 落地位置裁定（卡 4 / 卡 5：比较层与身份投影写入点）](stages/stage-07/2026-10-03-s7a-3-landing-location-rulings.md)（2026-10-03，决策卡 thread `01a0fe91-416c-7f31-a48a-d1890a0e68d1` 的卡 4（`adopt` 0.94）与卡 5（`adopt` 0.97）：容量只经身份投影进 `identity`、**`canonicalCompare` 不改**而由 `compareRequirement` 在 `.pattern`(L557) 与 `.measure`(L558) 之间显式比较两容量、四态排序 `缺席 < pending < measured(0) < measured(其他)` 由**唯一共享辅助函数**规定、容量**已测有效值**写入 **`writeChartProjection`**（`gameplay_assembler.cpp:622`，`writeRequirementIdentity` L636 之后、`writePatternNode` L650 之前）、**不改** `ReferenceKind::requirementIdentityProjection` / `closureOf` / Spec §3.8.6、缺席或 pending **不写哨兵字节**而在身份生成前原子失败；含两张卡共 9 项事实的主控独立复核（`writeChartProjection` L622、`writeRequirementIdentity` L524 调用 L636、`writePatternNode` L533 调用 L650、`makeChartIdentity` L1847、`MeasuredParameter` 无零默认无哨兵）与“未实现”声明；**只登记落地位置与裁定，不表示 S7A-3 或 Stage 7A 实现完成**）、
  [S7A-3 未闭合合同组合裁定：G2 + R2 + P1](stages/stage-07/2026-10-03-s7a-3-contract-selection-g2-r2-p1.md)（2026-10-03，owner 选择 prepared grace 的 G2、资源 claim / ownership prepare 计划的 R2、REF0 / Packed 首次写入隔离的 P1；登记解析顺序、身份归属、prepare/runtime 职责边界、旧 Reader 拒绝规则与实现停止条件；**只记录合同选择，不表示 S7A-3 或 Stage 7A 完成**）、
  [S7A-3 后续合同讨论与决策登记](stages/stage-07/2026-10-03-s7a-3-decision-register.md)（2026-10-03 初始讨论，2026-10-04 按 owner 自主裁决授权记录 **G2-S2 + G2-T4 + R2-K4 + P1-W1 + E1**；保留未选方案历史，链接时间 / 竞争 / Capsule 唯一规范；**设计裁定不表示实现完成**）、
  [S7A-3 实现推进记录：G2-S2 / R2 与 P1-W1 阻塞点](stages/stage-07/2026-10-03-s7a-3-implementation-progress.md)（2026-10-03，G2-S2 精确解析、R2 不可变资源计划与历史 3519/110 测试证据；当日 P1-W1 物理缺口已由 2026-10-04 设计裁决取代，Reader/Writer 与 E1 仍须实现验收；**本轮未重跑历史 runtime 证据，不表示 S7A-3 完成**）、
  [S7A-3 设计收口与实施交接报告](stages/stage-07/2026-10-04-s7a-3-design-closure.md)（2026-10-04，最终组合、字段追溯、REF0 固定子域、结构 hash、来源 identity 修正任务、全部 E1 与独立预算 / 包含性门禁、文档验证；**仅文档设计完成，不关闭实施验收**）、
  [S7A-6 / S7A-7 准入门禁：第 6 轮（发布粒度、表现桥接与诊断）裁定落地记录](stages/stage-07/2026-10-03-s7a-6-gate-rulings.md)（2026-10-03，15 行 / 17 项裁定 / `S7A6-R01…R12`、入口闭集与 manifest 七项、`CapabilityRecord` 逐字段 identity 归属、`GameplayOverride` precedence 与 adapter 生命周期、判定闭包 / 表现投影分界与聚合、四层诊断与集中码表 + 一个 CTest 校验项、`P1-08` 与 `P2-04`…`P2-09` 映射、计数变化 47 / 5 / 114、S7A-7 不得消费清单与门禁结果；**只冻结文档合同与门禁，不表示实现完成**）、
  [第 7 轮（收尾澄清与缺陷）裁定与收尾落地记录](stages/stage-07/2026-10-03-s7a-7-wrapup-rulings.md)（2026-10-03，16 行 / 18 项裁定 / `S7A7-R01…R16`、五条残余 `open` 按第 1 轮裁定补齐为 `accept`（最终 `open` 0 / `accept` 52 / 114）、行 / 项口径订正（第 6 轮报告的"76 行 / 83 项"为误推，实为 76 行 / 84 项自洽）、`D-11` 的 Stage 14 口径、`D-9` owner-only 与 `D-12` 待 owner 确认、`D-13` / `D-14` 最终证据指针、收尾门禁与门禁结果；**只冻结文档合同与门禁，不表示实现完成、也不表示 Stage 7A 完成**）、
  [第 6 轮 `CM-D04` / `P1-09` 首次消费的诊断分类学后续补齐记录](stages/stage-07/2026-10-03-s7a-diagnostics-taxonomy-rulings.md)（2026-10-03，决策卡 2026-10-02T21:03:39.462Z / thread `01a0fe6b-6e57-7051-87f8-cf9588e85568` / `adopt` 0.96 ＋ 处置确认卡 2026-10-02T21:51:38.591Z / thread `01a0fe97-f1b6-7332-b0c9-cafc3c9f25ec` / verdict `reject`（逐项处置）/ 0.7：九类为唯一 category 权威与 6 处裸 `capability` → `capability_disabled` 逐字替换、ABI 码族分组词的"不是取值"限定句、八条已登记码 category 定案（含 R-12 改正为 `invalid_relation`）、新增并登记 `ruleset.transaction_failed`（`invalid_relation` / `error` / `session_faulted` / S7A-5）与 `presentation-target-missing` / `partial-group`（`invalid_relation` / `error` / `session_unaffected` / S7A-7）、六个码首次消费登记为 S7A-3、13 个 `judgement.s7a2.*` 令牌状态词 `not_registered_src_only`、**公共诊断码判据（具名码串 / 适用场景 / 九类映射可在正典文本中明确相互追溯；前缀与 owner 命名均不决定公开性）与五条"码 → 来源映射行 → 域 8 承载面"清单写入 Spec §9.9 / ABI 域 8、并留 §11.2 独立裁定说明**、49 处活行号锚 → 稳定章节 / 符号引用且 JSON 内字面行号锚清零（逐条对照见 §11.7，锚定 ABI 定版 `c49447e4…`）、`docs/DOCUMENTATION_POLICY.md` 新增"行号锚只作带日期定版证据"规则、`BATCH_GATES.md` 撤下 6 行 Gameplay I → V2 推导表改为指向 Spec §9.2 / §9.3 与未关闭 `CM-D02`、公共承载面与 13 条 src-only 令牌由 `publicConsumptionSurface`（公共码非空 / 待登记一律 `null`）区分并由校验器新增 Rule 6 强制、BATCH_GATES / CONTRACT_MATRIX / OPEN_QUESTIONS 现行措辞订正与历史报告订正说明（§11.3 C-4）、`severity` / `faulted` 闭集明示（含 `info` / `warning` 暂未使用）、码表校验器正例 + 7 负例（含"src-only 令牌携带公共承载面"）与 `check_docs.py` / `git diff --check` 结果、计数一律不变（九类 / 19 / 9 域 126 / 20 / 114 / 76 行 84 项 / 29-13-7）；**只落地文档与集中码表语义，不表示 Judgement 实现完成、也不表示 Stage 7A 完成**）、
  [ABI 文档事故性截断与重建的证据记录](stages/stage-07/2026-10-03-abi-document-restoration.md)（2026-10-03，`docs/api/GAMEPLAY_V2_ABI.md`（untracked 候选）曾被一条 PowerShell 写命令截断为 2 行 / 309 字节（`AddRange` 绑定异常后 `WriteAllLines` 仍执行）；由 62 个 transcript + 8 份片段转储 + 29 条写入命令重建成 1245 行 / 148685 字节 / sha256 `d3e06a99…c585aa`，1040 / 1048 非空行逐字可溯（99.24%）；双线独立交叉验证：948 条直接观测行中 922 条在落盘件逐字命中（97.3%）、逐域 `126 = 96 + 30` 拆分完全相同、**实质冲突 0**，残余仅 L744 / L746 引号字形与 L752 / L753 折行点两处纯排版项；含根因、缓解纪律与"不表示实现完成、也不表示 Stage 7A 完成"声明）、
  [S7A-3 第二半 part 1：主控独立复验的带日期证据](stages/stage-07/2026-10-03-s7a-3-independent-verification.md)（2026-10-03，控制侧对实现交付的**两次独立复跑**且不复用代理自述数字：第 2 轮复审改正（`All tests passed (3335 assertions in 104 test cases)`、8/8 门禁、越界 0、STAGED=0、构建"真编译"取证）与第 3 轮改正（`3407 assertions / 105 test cases`、10/10 门禁、定点用例 50 / 35 / 25 断言与 `[budget]` 255 / 9、逐字节扫描、行尾未被静默改写、与代理自述逐项相符）；含 `git diff --check` 对 untracked 改动无鉴别力的盲区、`--clean-first` 与"真编译"取证的硬性要求、按名复跑的 Catch2 逗号分隔陷阱、静止守卫假阳性判据，以及"不主张 S7A-3 / Stage 7A 完成、不主张状态预算门禁完整、不主张包含性门禁已生效、不主张预算数值已冻结"的表述纪律与未覆盖范围清单；**只登记复验证据与方法学结论，不表示实现完成、也不表示 Stage 7A 完成**）、

- [S7A-3/4 实施进度与暂停点](stages/stage-07/2026-10-05-s7a-3-4-implementation-progress.md)（2026-10-05，工作区实现、聚焦验证和剩余验收；不关闭整批）
- [S7A-3 E1 与 S7A-4 逐项审计](stages/stage-07/2026-10-05-s7a-3-4-e1-audit.md)
- [S7A-3/4 受限功能验收与最终矩阵](stages/stage-07/2026-10-05-s7a-3-4-functional-acceptance.md)
- [Stage 7 CI 耗时基线与优化](stages/stage-07/2026-10-05-ci-runtime-optimization.md)
- [S7A-3/4 新SHA hosted、后续规划与临时证据归档](stages/stage-07/2026-10-05-s7a-3-4-hosted-and-handoff.md)

## 跨阶段专题

- [CFU and Stage 4 review](reviews/cfu-stage-04/2026-08-28-review.md)
- Documentation reorganization：[initial report](reviews/documentation-reorganization-2026-08/2026-08-30-reorganization.md)、
  [root consolidation](reviews/documentation-reorganization-2026-08/2026-08-30-root-consolidation.md)、
  [README consolidation](reviews/documentation-reorganization-2026-08/2026-08-30-readme-consolidation.md)
- 260830-followup：[最终关闭报告](reviews/260830-followup/2026-09-01-final.md)、[任务 2 Chart/CXC parse-once 完成报告](reviews/260830-followup/2026-08-31-task-2-chart-cxc-parse-once.md)、
  [任务 3 hosted 完成报告](reviews/260830-followup/2026-08-31-task-3-hosted-verification.md) 和
  [任务 3 本地快照](reviews/260830-followup/2026-08-31-task-3-critical-branch-coverage.md)
- [Full Review 2026-08](reviews/full-review-2026-08/README.md)
- [Full Review final closure](reviews/full-review-2026-08/2026-08-30-final.md)
- [SDK transition verification](sdk-transition/verification.md)
- [Stage Verification 2026-09 核验记录](reviews/stage-verification-2026-09/2026-09-01-findings.md)（historical review snapshot；
  三项核验问题已在 Stage 6 关闭报告中记录处置）
- Stage 6 双轴复核（`13dab93..eaaf375`，事后复核）：[汇总](reviews/stage-06-review-2026-09/2026-09-28-summary.md)、
  [Standards 轴](reviews/stage-06-review-2026-09/2026-09-28-standards.md)、
  [Spec 轴](reviews/stage-06-review-2026-09/2026-09-28-spec.md)、
  [切片附录](reviews/stage-06-review-2026-09/2026-09-28-slice-appendix.md)、
  [二轮细化](reviews/stage-06-review-2026-09/2026-09-28-refinement.md)（historical review snapshot）
- Stage 6 复核修正（批次报告）：[R1 文档与证据链修正](reviews/stage-06-review-2026-09/2026-09-28-r1-document-and-evidence.md)、
  [W1 诊断码与版本门禁](reviews/stage-06-review-2026-09/2026-09-28-w1-diagnostics-and-version-gate.md)、
  [W2 媒体导入修正](reviews/stage-06-review-2026-09/2026-09-28-w2-media-import.md)、
  [W3 发布事务修正](reviews/stage-06-review-2026-09/2026-09-28-w3-publication-transaction.md)、
  [W4 渲染收敛](reviews/stage-06-review-2026-09/2026-09-28-w4-render-convergence.md)、
  [W5 宿主与分发门禁](reviews/stage-06-review-2026-09/2026-09-28-w5-host-and-distribution-gates.md)、
  [W6 代码健康度](reviews/stage-06-review-2026-09/2026-09-28-w6-code-health.md)、
  [交付报告](reviews/stage-06-review-2026-09/2026-09-28-delivery-report.md)
- Gameplay Ruleset 设计（candidate 提案的研究性实测，非实施）：
  [Fold Spike 预算实测与确定性检查](reviews/gameplay-ruleset-2026-10/2026-10-01-fold-spike.md)（2026-10-01）、
  [I 收敛前置审查](reviews/gameplay-ruleset-2026-10/2026-10-01-i-entry-readiness.md)（2026-10-01）、
  [I 收敛整理报告](reviews/gameplay-ruleset-2026-10/2026-10-01-i-convergence.md)（2026-10-01）
- Stage 6 复核修正批次 R9（关闭后复核；最终行为 SHA `cc14fcd`，owner 已于 2026-09-29 接受）：
  [R9 参考宿主命令循环与 play/pause](reviews/stage-06-review-2026-09/2026-09-28-r9-reference-host-command-loop.md)

## 历史路径

- [Legacy stage-report paths](legacy-paths.md)

历史报告不可被改写为新的验证结果。新的关闭或复核必须创建新的报告，并从当前状态页或相应索引链接。

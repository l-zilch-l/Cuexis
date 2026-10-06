# Cuexis Current Status

状态：current

更新日期：2026-10-06

本文是 Cuexis 当前产品和阶段状态的唯一摘要。ADR 定义决策，Spec 定义字段和语义，阶段计划定义未来
范围，阶段报告保存带日期的实施证据；它们不得绕过本文重新定义当前状态。

## 产品边界

Cuexis 由可嵌入的 Playback SDK、独立参考 Player 和独立 Studio 构成。宿主通过
`PlaybackSession`、`PlaybackSource`、`FrameSnapshot`、ContentProvider 和后续 Judgement/Replay
合同接入；宿主不访问 RuntimeSession、World、EnTT、SDL、OpenGL 或 JSON DOM。

权威决策：[ADR 0027](adr/0027-playback-sdk-product-boundary.md)。公共使用说明见
[API reference](api/README.md)。

## 阶段状态

| 阶段 | 当前状态 | 权威入口 |
| --- | --- | --- |
| Stage 0 | completed | [report](stage_reports/stages/stage-00/completion.md) |
| Stage 1A-1E | completed | [reports](stage_reports/stages/stage-01/README.md) |
| Stage 2 | completed | [plan](stage_plans/completed/stage-02/plan.md) |
| Stage 3 | completed | [plan](stage_plans/completed/stage-03/plan.md) |
| Stage Chart Format Update | completed | [plan](stage_plans/completed/chart-format-update/plan.md) |
| Stage 4 | completed | [plan](stage_plans/completed/stage-04/plan.md) |
| Stage 5 | completed | [plan](stage_plans/completed/stage-05/plan.md)、[completion](stage_reports/stages/stage-05/completion.md) |
| 260830-followup（维护计划） | completed | [plan](stage_plans/completed/260830-followup/plan.md)、[关闭报告](stage_reports/reviews/260830-followup/2026-09-01-final.md) |
| Chart Format Foundation | completed | [plan](stage_plans/completed/chart-format-foundation/plan.md)、[关闭记录](stage_reports/stages/chart-format-foundation/2026-09-16-closure-and-handoff.md) |
| Chart Format Foundation Hardening（Foundation 交接加固） | completed | [plan](stage_plans/completed/chart-format-foundation-hardening/plan.md)、[R5 报告](stage_reports/stages/chart-format-foundation/2026-09-17-r5-regression-and-handoff.md) |
| Stage 6 | completed | [plan](stage_plans/completed/stage-06/plan.md)、[关闭报告](stage_reports/stages/stage-06/completion.md) |
| Stage 7A | active；S7A-3/4受限功能验收已完成。S7A-5.1–5.5与S7A-6.1–6.5联合实现已落地，J0–J7受限功能验收与本地跨工具链证据回填完成；实际Fold/Hook、canonical Replay、全量Snapshot、exact Seek和惰性分支已接线。生产容量/state-budget、S7A-7/8/9及Stage6 handover门禁独立保留；新SHA hosted待推送后回填，不关闭阶段 | [plan](stage_plans/active/stage-07/plan.md)、[5/6报告](stage_reports/stages/stage-07/verification/2026-10-06-s7a-5-6-functional-acceptance.md)、[3/4证据](stage_reports/stages/stage-07/verification/2026-10-05-s7a-3-4-functional-acceptance.md) |
| Stage 7B+ | future；Slide、Flick、多指、校准和高级 Judgement 能力持续演进 | [plan](stage_plans/active/stage-07/plan.md) |
| Stage 8 | future；Chart v5 / CXT v2 / Packed Chart 正式发行与语义收敛 | [plan](stage_plans/future/stage-08/plan.md) |
| Stage 9 | future；Presentation Foundation 与 Chart v6 / Model v1 | [plan](stage_plans/future/stage-09/plan.md) |
| Stage 10 | future；Chart v5 Studio 和发行工作流 | [plan](stage_plans/future/stage-10/plan.md) |
| Stage 11 | future；Chart v7 单轴几何表现与 shader.json 声明接口 | [plan](stage_plans/future/stage-11/plan.md) |
| Stage 12 | future；规模化、桌面性能、Android 与 Vulkan | [plan](stage_plans/future/stage-12/plan.md) |
| Stage 13 | future；Chart v8、高级表现与确定性粒子 | [plan](stage_plans/future/stage-13/plan.md) |
| Stage 14 | future；稳定 ABI 与 Playback SDK v1 | [plan](stage_plans/future/stage-14/plan.md) |

**Stage 6 未完成项的归属变更（2026-09-29）**：关闭后对 [ADR 0042](adr/0042-stage-6-productization-boundaries.md)
逐条核查发现四项只部分实现或未实现，且**此前不被任何阶段计划列入范围**（`completion.md` 的 Stage 7A
与 Stage 8 交接清单均未登记，Stage 8 计划正文对四项零命中）。它们是：显式 candidate 与实验隔离
（`Cuexis_ALLOW_EXPERIMENTAL`、candidate flavor、Player `--candidate-entry`、没有任何 preset 或 CI
开启 candidate）、离线 typed assembler 与 feature 派生、具名宿主六动词命令循环、SDK API `0.7.1`。
经项目所有者指定，四项自 2026-09-29 起由 **Stage 7A 承接，并作为其关闭前置条件**，登记在
[Stage 7 计划](stage_plans/active/stage-07/plan.md) 的 §1.2（并在 §3 的 S7A-8 工作内容与 §3.1 台账的
S7A-8.1–S7A-8.4 展开；2026-10-02 订正，原文误写为 §2 与 §6）。**这四项的S7A-8关闭门禁尚未整体退出**；Reference Host命令循环基线已有实现，3/4也已有离线author局部实现，真实candidate/assembler产品接线、同SHA交接和SDK放行仍需8逐项验证。本段只记录
归属与前置条件，不构成任何实现或发布声明。逐条证据见
[复核交付报告 §10](stage_reports/reviews/stage-06-review-2026-09/2026-09-28-delivery-report.md)；
2026-10-02 的当前状态逐项复核（含"具名宿主命令循环已由 PR #31 合入 `master`、动词集为
`open/play/pause/tick/seek/reload/quit`"这一取代性事实）见
[S7A-0 四项交接收口台账](proposals/gameplay-v2-acceptance/STAGE6_HANDOVER_LEDGER.md)。

其中 **SDK API `0.7.1` 另有门禁放行通路未接线的实测阻塞**（2026-09-29）：`version-gate.yml` 未传入
`--allow-sdk-api-change`，候选分支无法自行开启；而该改动按 ADR 0042 需代码所有者复核，本仓库没有
CODEOWNERS。本批次不升版本，该阻塞随 `0.7.1` 一并归入 Stage 7A 关闭前置，细节见
[复核交付报告 §10](stage_reports/reviews/stage-06-review-2026-09/2026-09-28-delivery-report.md)。

## 当前格式与 SDK 合同

### Gameplay v2 收敛状态（2026-10-04）

Gameplay V2 已成为 Stage 7A 的实施基线：Gameplay I 的 ADR/Spec/ABI 已标注 `superseded`，只保留为历史
候选基线与差异对照（取舍见 [ADR 0043](adr/0043-gameplay-judgement-ruleset-convergence.md)）。现行权威为
[ADR 0044](adr/0044-gameplay-v2-semantic-kernel.md)、[Gameplay V2 Spec](formats/GAMEPLAY_V2_SPEC.md) 与
[Gameplay V2 ABI](api/GAMEPLAY_V2_ABI.md)（三份顶层均为 `candidate`；部分实现不等于整批验收）。按 2026-10-02 的第 1 轮
补充（S1-01…S1-05，见 [裁决清单](proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) §7A），冻结采用
**分批冻结**口径：只授权**受限** S7A-1 骨架（类型角色与边界、模块与安装边界、所有权与异常承诺），
该受限骨架授权本身**不授权**字段表示、整数宽度、序列化与线格式、枚举集或其余批次语义；
后续各批次已经闭合的时间、输入与 Capsule candidate 表示仅按其明确合同消费
（第 1–7 轮**已全部裁定、无待裁定轮次**，语义由各自小节授权；第 7 轮"收尾澄清与缺陷"见
[第 7 轮收尾裁定与缺陷处置记录](stage_reports/stages/stage-07/decisions/2026-10-03-s7a-7-wrapup-rulings.md)）。
**裁定完成不等于实现完成**：受限 S7A-1 骨架、S7A-2 两个半批与 S7A-3 第二半 part 1 已落地并有带日期记录；
**S7A-3/4 受限功能验收完成，容量整体证明未完成**；包含性 gateIncomplete 与真实容量装配消费点已由当前 Spec §3.8.8
记录落地，不再作为“尚未接线”缺口。2026-10-05 的只读核对补齐既有实现与当前 SHA hosted 证据，
见 [剩余证据核对](stage_reports/stages/stage-07/verification/2026-10-05-s7a-3-remaining-evidence.md)；该报告属于此前只读核对。随后本工作区实施了 runtime T4/K4 与真实 author adapter，聚焦 C++ 验证见
[受限功能验收证据](stage_reports/stages/stage-07/verification/2026-10-05-s7a-3-4-functional-acceptance.md)；E1 功能行与四工具链完整矩阵已补齐；state-budget 保持 INCOMPLETE GATE，Stage7A 不关闭。
提交a0c8b7e的hosted与临时证据归档见 [新SHA交接报告](stage_reports/stages/stage-07/handoffs/2026-10-05-s7a-3-4-hosted-and-handoff.md)；
本轮由owner授权实施S7A-5.1–5.5 + S7A-6.1–6.5；目标、九项已选方向、十三项首用合同和
J0–J7执行卡归 [主计划§3.2](stage_plans/active/stage-07/plan.md#32-s7a-5--s7a-6-联合实施目标决策和准入)。
[方案比较与选优](proposals/implementation-input/stage-07/s7a-5-6-design-selection.md) 保存每项五套备选及反例；
[U01–U13重审](stage_reports/stages/stage-07/verification/2026-10-06-s7a-5-6-u01-u13-rereview.md) 修正六项边界，其他七项保留方向并补条件，消费合同已落入Spec/ABI/profile，实施与验证见本轮报告。
active/stage-07只保留主计划与 [旧路径映射](stage_plans/active/stage-07/legacy-paths.md)，
历史接手和交付评估已归档；当前接手导出至owner桌面，决策仍写回plan/对应合同。
5/6受限功能验收完成，Debug/Release/headless/shared/MinGW/GCC/Clang本地证据已回填，Replay/Snapshot六套字节一致；新提交hosted待回填。尚未实施7–9、接受生产预算或关闭阶段；既有
[排期修订记录](stage_reports/stages/stage-07/handoffs/2026-10-06-s7a-5-6-planning-selection.md) 保留当时证据，
本次目录/桌面交接见 [整理记录](stage_reports/stages/stage-07/handoffs/2026-10-06-s7a-5-6-plan-and-desktop-handoff.md)。设计来源见
[Gameplay V2 redesign research](proposals/research/gameplay-v2/README.md)。Hold 的 prepared grace、作者
默认值与单条覆盖、严格边界、identity 分区和预算/快照原则已统一；Slider continuity/grace
仍属于 Stage 7B+ capability，不是 Stage 7A 默认语义。
**稳定 C ABI 的唯一归属阶段是 Stage 14**（不是 Stage 12）：Stage 7A/7B/8 只提供 C++ typed preview，
稳定 C ABI 由 [Stage 14 计划](stage_plans/future/stage-14/plan.md) 承接；`AGENTS.md` 中把稳定 C ABI 写作
Stage 12 的表述是**孤例**（第 21、260 行），由 owner 择时订正，本轮不改 `AGENTS.md`（缺陷 `D-11`）。
在当前路线修订中，Stage 7A/Stage 8 的 Gameplay semantic 基线固定为 Chart v5 下的
`gameplay.version = 2`。Canonical Gameplay Graph 可作为 CXC v1 独立 `gameplay-graph`
Playback entry，与 `packed-chart` entry 共用 typed prepare、Judgement、Ruleset、Snapshot、
Replay 和 Presentation bridge。Stage 7A 的目标合同包括 Tap/Hold/Release-tail、单一
`capacity=1` exclusive resource、TimebaseProfile、Ruleset fault transaction，以及
early/exact/late/Miss 的 early/late Presentation bridge；TimebaseProfile 已随 S7A-2 落地，T4/K4 当前有工作区实现；Ruleset、Snapshot/Replay、
Playback/Presentation bridge 等仍待后续批次，当前工作不关闭 S7A-4…S7A-9 的完整验收。

V2 三份顶层文档仍是工作稿、待 owner acceptance；C10–C14 与代表性切片仍是研究性证据，不能直接作为生产 ABI 限额。

- Chart v1/v2/v3 Reader、迁移和 Playback 路径继续保留；Chart v4 的静态、参数化和合法非空动画已由
  默认 Playback Session 求值。
- FrameDigest v1-v3、canonical bytes/order、合法输入 identity 与默认 capability 维持兼容。
- CXC v1、CXT v1、Chart v4 的格式语义以 [formats index](formats/README.md) 为准；内部 CXC 不是独立
  公共 package SDK。
- Chart v5 的发行路径仍是**显式 candidate**：正式默认 Writer、40,000 语义实体和 16 MiB Packed entry
  的发行门禁关闭前，Chart v5 不作为默认发行格式。Stage 7A 冻结最小 Judgement / Input / Replay 合同；
  [Stage 8](stage_plans/future/stage-08/plan.md) 完成 Chart v5 / Gameplay v2 / Canonical Gameplay
  Graph / CXT v2 / Packed Chart 与两种 Playback entry 的正式发行收敛；Stage 7B+ 的高级判定能力不阻塞
  Stage 8，按版本化 capability 持续交付。详细格式工作见
  [Chart v5 format plan](stage_plans/active/chart-format-update-for-v5/plan.md)（其 Foundation 交付已归档）。
- CXT v2 候选合同见 [CXT_V2_FORMAT.md](formats/CXT_V2_FORMAT.md)：模板、Prototype/Instance、
  Slot/Binding/ValueSource、Pattern、有限确定性展开和动画扩展；不包含任意脚本。
- Packed 候选物理合同见 [PACKED_CHART_FORMAT.md](formats/PACKED_CHART_FORMAT.md)：
  身份/字典、Archetype/实体差异流、无损 Beat 和容量 profile。它不是已实现的生产
  Reader；40k/16 MiB 仍需实测验收，更改被冻结的 CXT 参数需要显式重新编译。
- Gameplay revision 2 的完整 Packed 字段由 [Capsule format](formats/GAMEPLAY_CAPSULE_V2_FORMAT.md)
  唯一拥有；来源 / 有效 policy、T4 时间端点、K4 实例顺序与局部 proof 已设计闭合。
  来源身份投影修正、Reader/Writer、file/memory 与三工具链 focused 已有实施证据，
  当前 SHA hosted 包含既有 Capsule golden；完整 E1 与 authoring/affine 余项见上述核对报告。
  历史 3519/110 与 focused 测试数字不代表本轮复跑或整批新合同验收。
- CXC v1 仍为容器版本，v5 candidate playback path 与 v4 entry 并存。Stage 7A / Stage 8 负责收敛
  `gameplay-graph` 与 `packed-chart` 两种 Playback entry；Stage 8 只有在二者恢复同一 Canonical Gameplay
  Graph、prepared identity 和 Judgement kernel 后，才可正式发行。
- Chart v6 / Model v1、Chart v7 和 Chart v8 仍不可加载，分别作为 Stage 9/11/13 的表现设计输入；
  不再作为 Judgement 的隐性前置。
- SDK API 为 `0.7.0`。安装后的 Playback headers 不泄露 EnTT、SDL、OpenGL/GLAD、JSON DOM、
  RuntimeSession 或 World。
- Playback 的 default `allCapabilities()` 包含 shader asset 和 parameterized material capability；
  显式裁剪 Session 仍可稳定拒绝它们。Playback 热路径不调用 shader compiler。

## 延期和禁止边界

运行时脚本和逐帧 script callback 无限期延后；不为它们预留 Chart/CXT/CXC 字段、extension、
capability、bytecode、ABI 或 Playback hook。离线 authoring generator 只能作为未来独立工具讨论。

RT-29、T1 World/Animation 大规模优化与 T2 大包解析降本仍需另外的触发证据和明确授权。
[Chart v5 format plan](stage_plans/active/chart-format-update-for-v5/plan.md) 现作为 Stage 8 的详细格式
工作包；其前置 [Chart Format Foundation](stage_plans/completed/chart-format-foundation/plan.md) 与
[交接加固 R0–R5](stage_plans/completed/chart-format-foundation-hardening/plan.md) 已于 2026-09-17 完成并归档。
2026-09-01 阶段核验的三个问题（版本门禁、后端中立表现渲染边界、常用媒体支持）已随 Stage 6 关闭处置为
`closed`，历史现象不改写；删除旧 Reader 仍按 ADR 0041，不因 v5 弃用窗口实施。Stage 7B+ 可与 Stage 8
并行推进；Stage 8 关闭后交出经批准的 SDK 发行版本 / Chart v5 / CXT v2 接手基线。

## 更新规则

任何产品阶段状态变化必须同时更新本文与相应的计划或新报告。历史报告不得被改写为新的验证结果；
新的关闭、复核或纠正必须形成新的带日期报告。

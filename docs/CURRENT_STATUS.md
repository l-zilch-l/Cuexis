# Cuexis Current Status

状态：current

更新日期：2026-09-22

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
| Stage Chart Format Update | completed; CFU-C0-C4, D, E, F and G closed; G6 owner acceptance recorded 2026-08-24 | [plan](stage_plans/completed/chart-format-update/plan.md) |
| Stage 4 | completed; S4-H hosted and owner acceptance recorded 2026-08-27 | [plan](stage_plans/completed/stage-04/plan.md) |
| Stage 5 | completed; S5-A through S5-H closed and merged into `master` 2026-08-28 | [plan](stage_plans/completed/stage-05/plan.md)、[completion](stage_reports/stages/stage-05/completion.md) |
| Chart Format Foundation | completed；保留 PR #24 与 2026-09-16 owner 完成确认；后续交接缺口由独立加固阶段处理，不代表技术门禁全部通过 | [plan](stage_plans/completed/chart-format-foundation/plan.md)、[关闭记录](stage_reports/stages/chart-format-foundation/2026-09-16-closure-and-handoff.md) |
| Chart Format Foundation Hardening | completed；R0-R5 全部完成（D1-D10 已裁定，A19/A20/A21/A22 已修复），本地六配置全量回归、最终 SHA 容量复跑与同 SHA hosted 三平台验证（`e0ca9ff`，docs-only 复验 `c24f34e`）全绿，owner 于 2026-09-17 接受 R5 交接清单 | [plan](stage_plans/completed/chart-format-foundation-hardening/plan.md)、[R5 报告](stage_reports/stages/chart-format-foundation/2026-09-17-r5-regression-and-handoff.md)、[R5 容量数据 Debug](stage_reports/stages/chart-format-foundation/2026-09-17-r5-capacity-data.json)、[Release](stage_reports/stages/chart-format-foundation/2026-09-17-r5-capacity-data-release.json)、[R4 报告](stage_reports/stages/chart-format-foundation/2026-09-17-r4-roundtrip-capacity-rollback.md)、[R4 容量数据](stage_reports/stages/chart-format-foundation/2026-09-17-r4-capacity-data.json)、[R3 报告](stage_reports/stages/chart-format-foundation/2026-09-17-r3-budgets-and-arithmetic.md)、[R2 报告](stage_reports/stages/chart-format-foundation/2026-09-17-r2-profile-rejection.md)、[R1 报告](stage_reports/stages/chart-format-foundation/2026-09-17-r1-semantic-identity.md)、[R0 报告](stage_reports/stages/chart-format-foundation/2026-09-16-r0-baseline-and-reproduction.md) |
| Stage 6 | active；当前实施阶段，2026-09-17 启动（首批 S6-A：合同、基线和依赖决策）。交接加固已关闭并经 owner 接受，本阶段实施 v5-first candidate path，保留 v4 回退；消费边界见 [R5 报告 §10](stage_reports/stages/chart-format-foundation/2026-09-17-r5-regression-and-handoff.md) | [plan](stage_plans/active/stage-06/plan.md) |
| Stage 7A | future；最小 Input / Judgement / Score / Replay Kernel，作为 Stage 8 硬前置 | [plan](stage_plans/future/stage-07/plan.md) |
| Stage 7B+ | future；Slide、Flick、多指、校准和高级 Judgement 能力持续演进 | [plan](stage_plans/future/stage-07/plan.md) |
| Stage 8 | future；Chart v5 / CXT v2 / Packed Chart 正式发行与语义收敛 | [plan](stage_plans/future/stage-08/plan.md) |
| Stage 9 | future；Presentation Environment、天空盒、模型和有限形变 | [plan](stage_plans/future/stage-09/plan.md) |
| Stage 10 | future；Chart v5 Studio 和发行工作流 | [plan](stage_plans/future/stage-10/plan.md) |
| Stage 11 | future；性能、Android、Vulkan、粒子和高级表现 | [plan](stage_plans/future/stage-11/plan.md) |
| Stage 12 | future；稳定 ABI 与 Playback SDK v1 | [plan](stage_plans/future/stage-12/plan.md) |

Stage 6 计划已于 2026-09-20 按可实施性评审细分为基线/决策、candidate 消费、配置与控制、
中立渲染、离线媒体导入、宿主安装及最终验证子批次，依赖顺序和验收矩阵见
[Stage 6 plan](stage_plans/active/stage-06/plan.md)。S6-A1 基线、入口和证据矩阵已于
2026-09-20 完成，实际证据见 [S6-A1 报告](stage_reports/stages/stage-06/2026-09-20-s6-a1-baseline.md)。
owner 随后授权冻结关键实施决策，结论见
[ADR 0042](adr/0042-stage-6-productization-boundaries.md)：显式 experimental entry、typed
身份/lowering、高层内部 renderer、配置与提交顺序、离线媒体栈、版本门禁和具名参考宿主。
Stage 6 的条件性源兼容 SDK 目标为 `0.7.1`，当前实现仍为 `0.7.0`；Stage 8 正式 v5
不预留版本号，按届时基线与公共合同差异遵循 [版本规范](guides/VERSIONING.md) 裁定。
S6-A2 已于 2026-09-21 完成合同落盘与表征，形成 entry/config/media Spec、三份 Schema、
API/安装草案、依赖图及独立 identity/media/config golden；实际证据见 [S6-A2 报告](stage_reports/stages/stage-06/2026-09-21-s6-a2-contracts-and-characterization.md)。
A2 只证明冻结决策可以被明确描述和表征，不代表 v5 Playback、Player、renderer、media importer、
版本门禁或 Reference Host 已实现；SDK 仍为 `0.7.0`，三个工程问题继续 open。A2 之后 B1 与 C1
已分别取得退出证据。D1 正在实现且尚未退出；C2、E1/E2 及其后续批次仍按计划依赖图保持未完成。
S6-B1 已落下版本比较器、独立负例测试、trusted-baseline workflow 和发行 checklist；当前显示版本
为 `26.09.22-1`，SDK API 仍为 `0.7.0`。`26.09.21-2` 在 UTC `2026-09-22` 已过期，PR #28 因此前进到当天 build 1。bootstrap PR #26 已将门禁纳入 `master`，并启用
`Version advancement (pre-merge)` required check、strict latest-base 和 admin enforcement。
候选 PR #27 的 `d4697549a50e9c517ac393c27786826aa43ce9cc` 以 trusted baseline
`4545742ed63ae2d8f11ad07e80930ce5b88fa0ce` 通过 protected Version Gate run `35586930775`，
并在同一 SHA 通过 Linux Quality、Windows MSVC、Windows MinGW；随后合并为 `master`
`46b65d1f2345f543b98e7e87fe5ec9ed735f10bf`。本地 focused tests、docs、fresh configure、
clean-first build 与 `683/683` CTest 证据见 [S6-B1 报告](stage_reports/stages/stage-06/2026-09-20-s6-b1-version-gate.md)。
B1 与 S6-G02 已满足其计划门禁。S6-C1 已于 2026-09-22 在本地退出：默认关闭的显式
candidate 工厂、生产 CXC bridge、typed lowering、Runtime opacity 和 candidate prepared
identity 已接入。本地 MSVC 快照见
[实现报告](stage_reports/stages/stage-06/2026-09-22-s6-c1-candidate-source.md)；
退出 SHA `3e11b167f8fccae47b0bd8b0a94080e649ba544f` 的 hosted 默认 OFF 矩阵见
[C1 退出报告](stage_reports/stages/stage-06/2026-09-22-s6-c1-exit.md)。该退出不是
Stage 6 完成、PR 合并或 owner acceptance。SDK API 仍为 `0.7.0`。S6-D1 已开始：内部
`cuexis_presentation_renderer` 与无 GPU 测试 renderer 有本地证据，见
[D1 报告](stage_reports/stages/stage-06/2026-09-22-s6-d1-renderer-contract.md)。
OpenGL 迁移仍属 D2。C2、E1/E2 及后续批次仍按依赖图未完成。

## 已关闭的 Full Review

Stage 5 已于 2026-08-28 经 PR #20 合并至 `master`；其 S5-H 报告保留为关闭前的本地检查快照，
现行关闭结论以 [Stage 5 completion](stage_reports/stages/stage-05/completion.md) 为准。

`260829 Full Review` 随后已于 2026-08-30 关闭。最终实现 SHA
`fbe118bb310fffa1446584e0a30fd46bc743413b` 已通过 Linux Quality、Windows MSVC 和 Windows MinGW
hosted 验证；144 项 finding 的 disposition 已记录。该关闭不重新打开已关闭的 Stage 5，也不表示
Chart/CXC parse-once、RT-29、World/Animation 大规模优化、大包解析降本、Studio、Judgement/Replay、
稳定 C ABI 或运行时脚本已经实现。

证据：[Full Review final closure](stage_reports/reviews/full-review-2026-08/2026-08-30-final.md)。

## 当前格式与 SDK 合同

- Chart v1/v2/v3 Reader、迁移和 Playback 路径继续保留；Chart v4 的静态、参数化和合法非空动画已由
  默认 Playback Session 求值。
- FrameDigest v1-v3、canonical bytes/order、合法输入 identity 与默认 capability 维持兼容。
- CXC v1、CXT v1、Chart v4 的格式语义以 [formats index](formats/README.md) 为准；内部 CXC 不是独立
  公共 package SDK。
- Chart v5 分三步：Chart Format Foundation 在 Stage 6 之前交付 CXT v2 Core、Packed 原型、
  CXC entry 设计和容量门禁；Stage 6 以 v5 Core/Packed candidate 为主要开发和验证路径，
  同时保留 v4 回退；Stage 7A 冻结最小 Judgement/Input/Replay 合同；Stage 8 再完成正式
  Chart v5、CXT v2、Packed Chart 和 CXC playback entry 的发行收敛。Stage 7B+ 高级判定
  能力不阻塞 Stage 8，改以版本化 capability 持续交付。正式默认 Writer、40,000 语义实体
  和 16 MiB Packed entry 发行门禁关闭前，Chart v5 只能通过显式 candidate path 使用，不能
  作为默认发行格式。详细计划见
  [Stage 8](stage_plans/future/stage-08/plan.md) 和
  [Chart v5 format plan](stage_plans/active/chart-format-update-for-v5/plan.md)。
- CXT v2 候选合同见 [CXT_V2_FORMAT.md](formats/CXT_V2_FORMAT.md)：模板、Prototype/Instance、
  Slot/Binding/ValueSource、Pattern、有限确定性展开和动画扩展；不包含任意脚本。
- Packed 候选物理合同见 [PACKED_CHART_FORMAT.md](formats/PACKED_CHART_FORMAT.md)：
  身份/字典、Archetype/实体差异流、无损 Beat 和容量 profile。它不是已实现的生产
  Reader；40k/16 MiB 仍需实测验收，更改被冻结的 CXT 参数需要显式重新编译。
- CXC v1 仍为容器版本。Foundation 负责 entry 映射和 Packed 验证原型，Stage 6 验证 v5
  candidate playback path 并保留 v4 entry，Stage 8 负责在 CXC v1 内正式发行已验证的
  Chart v5 Packed playback entry。
- Chart v6 / Model v1、Chart v7 和 Chart v8 仍不可加载，分别作为 Stage 9/11 的表现设计输入；
  不再作为 Judgement 的隐性前置。
- SDK API 为 `0.7.0`。安装后的 Playback headers 不泄露 EnTT、SDL、OpenGL/GLAD、JSON DOM、
  RuntimeSession 或 World。
- Stage 5 的 default `allCapabilities()` 包含 shader asset 和 parameterized material capability；
  显式裁剪 Session 仍可稳定拒绝它们。Playback 热路径不调用 shader compiler。

## 延期和禁止边界

运行时脚本和逐帧 script callback 无限期延后；不为它们预留 Chart/CXT/CXC 字段、extension、
capability、bytecode、ABI 或 Playback hook。离线 authoring generator 只能作为未来独立工具讨论。

已关闭的 [260830-followup 维护计划](stage_plans/completed/260830-followup/plan.md) 的文档整理、
Chart/CXC parse-once 和关键模块分支覆盖率三个任务均已完成。任务 3 的最终 SHA
`299596c533a8c66a78b5c4ada341b1163528fb25` 已通过 Linux Quality、Windows MSVC 和 Windows
MinGW；分模块覆盖率和环境残余见其[完成报告](stage_reports/reviews/260830-followup/2026-08-31-task-3-hosted-verification.md)。
PR #22 已于 2026-09-01 合并至 `master`，关闭总结见
[260830-followup 最终关闭报告](stage_reports/reviews/260830-followup/2026-09-01-final.md)。
RT-29、T1 World/Animation 大规模优化和 T2 大包解析降本仍需另外的触发证据和明确授权。项目
所有者已于 2026-09-02 建立 [Chart v5 format plan](stage_plans/active/chart-format-update-for-v5/plan.md)；
该计划现作为 Stage 8 的详细格式工作包，其 Foundation 前置工作见
[Chart Format Foundation](stage_plans/completed/chart-format-foundation/plan.md)。原完成确认保留；
后继的 [Foundation 交接加固](stage_plans/completed/chart-format-foundation-hardening/plan.md)
（R0-R5）已于 2026-09-17 完成并归档：R0 基线/复现与决策 D1-D3 于 2026-09-16 完成，R1 语义
身份闭环、R2 前置（两组独立负例、失败顺序策略、调用关系与未知值盘点）与 R2 本体（统一
profile validator、section 注册表与 flags 策略、A09 内部次序校验、Writer 规范化）、R3 预算与
checked arithmetic（Spec §3.3 冻结预算表、`maxPackedSectionBytes` 与逐字段诊断、目录乘积与
u32 窄化、payload 计数驱动的预留、limits 只收紧不放宽、A13 计数口径裁定）、R4 端到端/容量/
回滚（完整语义往返与 re-encode 规范性、CXT 阶梯、真实 CXC candidate 包七场景、原子写回滚
四场景、机器可读容量数据与峰值口径、A10/A11/A12/A17/A18 修复、D9/D10 裁定）均于 2026-09-17
完成，R5 回归与交接收尾同日完成：修复会阻塞 hosted CI 的 A19（headless 配置无法生成）、
A20/A21（GCC `-Werror` release 构建的两个伪诊断）与 A22（该 GCC 专用告警降级被误用于 Clang
的 sanitizer 构建），Debug/Release/shared-debug/headless-debug/MinGW headless/GCC Release
`-Werror` 六个配置全量 CTest 全绿，实现 SHA `0e501a5` 的 Debug/Release 容量复跑与 R4 逐字节
一致，回归矩阵、允许/禁止消费清单与 Stage 6 接手命令写入 [R5 报告](stage_reports/stages/chart-format-foundation/2026-09-17-r5-regression-and-handoff.md)。
hosted 迭代四轮：首轮（`524db9f`）MSVC 通过、MinGW `release` 与 Linux 的 GCC release 任务因
A20 失败；第二轮（`2cc478e`）GCC 任务转为通过、Clang sanitizer 两项因 A22 失败；A22 修复后
第三轮（`e0ca9ff`）Linux Quality、Windows MSVC、Windows MinGW **三个 workflow 全部成功**，
第四轮（`c24f34e`，仅记录文档）再次全绿；实现 SHA `9314646` 与报告 SHA 之间只有 `docs/` 变化。
owner 于同日接受 R5 交接清单，Stage 6 因此从 future 恢复 **active**，并于 2026-09-17 启动
（首批 S6-A：合同、基线和依赖决策），作为当前实施阶段接手
v5-first candidate path；Stage 6 完成后进入
Stage 7A，再进入 Stage 8。Stage 7B+ 可以与 Stage 8 前后并行持续；Stage 8 关闭后交出
经批准的实际 SDK 发行版本 / Chart v5 / CXT v2 正式接手基线。
2026-09-01 阶段核验记录中的版本门禁、后端中立表现渲染边界和常用媒体支持三个 open 问题仍归属于
[Stage 6 plan](stage_plans/active/stage-06/plan.md)，尚未因列入计划而视为解决。删除旧 Reader 仍按
ADR 0041，不因 v5 弃用窗口或 Stage 6 启动而实施。

## 更新规则

任何产品阶段状态变化必须同时更新本文与相应的计划或新报告。历史报告不得被改写为新的验证结果；
新的关闭、复核或纠正必须形成新的带日期报告。

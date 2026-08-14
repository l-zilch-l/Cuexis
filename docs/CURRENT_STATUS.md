# Cuexis Current Status

状态：现行状态页

更新日期：2026-08-14

本文是当前阶段和实现状态的唯一摘要。阶段计划、完成报告和审查报告仍然保留各自的历史
细节，但不能绕过本文重新定义当前状态。

## 产品边界

Cuexis 的产品结构是可嵌入的 Playback SDK、独立参考 Player 和独立 Studio。宿主通过
PlaybackSession、FrameSnapshot、ContentProvider 和后续 Judgement/Replay 合同接入；宿主不
访问 RuntimeSession、World、EnTT、SDL、OpenGL 或 JSON DOM。

权威决策：[ADR 0027](adr/0027-playback-sdk-product-boundary.md)。

## 阶段状态

| 阶段 | 状态 | 权威证据 |
| --- | --- | --- |
| Stage 0 | 已完成 | [完成报告](stage_reports/stage_0_completion_report.md) |
| Stage 1A | 已完成 | [完成报告](stage_reports/stage_1a_completion_report.md) |
| Stage 1B | 已完成 | [完成报告](stage_reports/stage_1b_completion_report.md) |
| Stage 1C | 已完成 | [完成报告](stage_reports/stage_1c_completion_report.md) 与 [全量审查](stage_reports/260722-1c-review.md) |
| Stage 1D | 已完成 | [完成报告](stage_reports/stage_1d_completion_report.md) |
| Stage 1E | 已完成 | [完成报告](stage_reports/stage_1e_completion_report.md) |
| Stage 2 | 已完成 | [完成报告](stage_reports/stage_2_completion_report.md) |
| Stage 3 | 已完成 | [完成报告](stage_reports/stage_3_completion_report.md) |
| Stage Chart Format Update | 当前活动阶段；CFU-C0–C4 已完成；CFU-D1/D2 已关闭；CFU-E 已关闭；CFU-D3 为下一批次；整包 CFU-D 未关 | [实施计划](stage_plans/stage_chart_format_update_implementation_plan.md)、[D1/D2 报告](stage_reports/260813-chart-format-update-d-migration.md)、[E0 报告](stage_reports/260814-chart-format-update-e0-api.md)、[E1 报告](stage_reports/260814-chart-format-update-e1-source.md)、[E2 报告](stage_reports/260814-chart-format-update-e2-prepare.md)、[E3 报告](stage_reports/260814-chart-format-update-e3-identity.md)、[E4 报告](stage_reports/260814-chart-format-update-e4-gates.md) 与 [E 关闭报告](stage_reports/260814-chart-format-update-e-close.md) |
| Stage 4 | 未开始，等待格式阶段关闭 | [实施计划](stage_plans/stage_4_implementation_plan.md) |

Stage Chart Format Update 是 Stage 3 与 Stage 4 之间的正式名称，不使用 Stage 3.5 作为别名。

## 格式状态

- Chart v1/v2/v3 已实现并继续保留全部 Reader、迁移和 Playback 路径。Chart v4 的静态与参数化
  内容已可由 Playback prepare；非空动画仍在 Stage 4 前稳定拒绝。
- ADR 0038 已于 2026-08-11 整体接受，Stage Chart Format Update 已进入 CFU-C 实施。
- CFU-C1 已建立 Chart v4、CXT v1、CXC manifest v1 Schema、正式 fixture、typed source model/Reader
  和内部 `cuexis_cxc` manifest 目标。
- CFU-C2 已建立 Chart v4/CXT v1 canonical Writer、ChartParameter 冻结与 identity、project-document
  lookup、CXT import/identity、Template Binding deterministic lowering、资源闭包、capability 推导和
  checked aggregate budgets。
- CFU-C3 已建立内部 `cuexis_cxc` strict ZIP32 Stored Reader/Writer、manifest/project closure、owning
  file/memory package、精确 package identity、package-backed Asset ContentProvider、独立 Chart/CXT
  project-document table、committed binary fixtures 和 static/shared package leakage gates。该内部检查点
  仍不是完整 CXC 产品支持，不是公共 package API，也不包含 Playback 接入或动画求值。
- CFU-C4 已在本地实现 `cuexis_cxc_pack`、`cuexis_cxc_validate`、`cuexis_cxc_unpack`、source snapshot、
  sibling staging/atomic commit、no-overwrite、exit `0/1/2` 和 binary round-trip CMake 门禁。当前
  hosted Linux/Windows/MinGW、sanitizer、coverage 和 Linux developer-tool gates 已在最终 SHA
  `41ddb6a980b816b2c0b3b1e25df9268603bcc883` 通过；CFU-C4 已关闭。该关闭仍不是完整 CXC 公共产品
  支持，不包含 Playback 接入或 Stage 4 动画求值。
- CFU-D1/D2 已关闭：`cuexis_chart` 与 `cuexis_chart_migrator` 提供显式 JSON lift；保留
  `migrateToV3` 与默认 CLI v3 输出；新增 `migrateToV4` / `--target 4`；v1/v2 → v4 复用 v3 路径；
  v4 报告增加 canonical identity 与字段计数。本 worktree Debug 证据见
  [D1/D2 报告](stage_reports/260813-chart-format-update-d-migration.md)。整包 CFU-D 未关；
  CFU-D3 现在可以开始，仓库外旧 Chart 资产确认仍未开始。
- CFU-E0 已于 2026-08-14 经项目所有者接受并关闭：冻结 `ChartParameterSet`、
  `PlaybackPrepareOptions`、`PreparedSemanticIdentity`、owning typed project-document source、CXC
  file/memory factory、options overload、semantic identity observation 和 SDK API `0.6.0` 实施目标。
  E0 只关闭设计与评审门禁。
- CFU-E1 已于 2026-08-14 关闭：`PlaybackSource` 已统一为 entry path、bounded owning
  project-document table、可选 `AssetDatabase`、owning provider 和内部 CXC package identity metadata；
  新增 `PlaybackProjectDocument`、`TypedPlaybackProjectSource` 与 CXC file/memory factory，保留旧
  factory 和 `TypedPlaybackProject` 三字段布局。SDK API 已同步为 `0.6.0`，static/shared clean
  consumer、version rejection 和 export/import 门禁通过。本地证据见 [E1 报告](stage_reports/260814-chart-format-update-e1-source.md)。
- CFU-E2 已于 2026-08-14 关闭：public ParameterSet 保留 number/rational/weight tag 并在每次 prepare
  转换为 Chart typed input；Playback 按 parse/semantic/import/parameter/budget、capability、compile 的
  顺序接入 Chart v4。静态和参数化 v4 可进入现有 Runtime；CXC/CXT/v4 格式能力已声明，任意非空
  Clip/CXT/Binding/Layer/Instance 在 Stage 4 前仍以 `playback.capability.unsupported` 失败。证据见
  [E2 报告](stage_reports/260814-chart-format-update-e2-prepare.md)。
- CFU-E3 已于 2026-08-14 关闭：成功 prepare 在资源获取与 typed 校验之后组装
  `PreparedSemanticIdentity`；同一规范内容加同一冻结参数跨 chart-text / typed / memory /
  filesystem / CXC 得到同一值；失败 reload 不改 active identity。证据见
  [E3 报告](stage_reports/260814-chart-format-update-e3-identity.md)。
- CFU-E4 已于 2026-08-14 关闭本地门禁：Debug/Release `362/362`、shared-debug
  package/export/import `10/10`、format/docs/version/whitespace 与 Playback-only
  consumer identity 观察均通过。证据见
  [E4 报告](stage_reports/260814-chart-format-update-e4-gates.md)。
- CFU-E 已于 2026-08-14 经项目所有者接受并关闭：最终 SHA
  `2dc5f6cc1f413132502896705fd46163fec760b2` 的 Linux Quality、Windows MSVC 与
  Windows MinGW 全部成功。证据见
  [E 关闭报告](stage_reports/260814-chart-format-update-e-close.md)。该关闭不是完整
  CXC 公共产品支持、公共 package API、完整 v4 动画 Playback、整包 CFU-D 或 CFU-G。
- CFU-C 至 CFU-G 的详细实现批次、模块落点、API 门禁、测试矩阵、跨平台验收和 Stage 4 交接方案
  已写入实施计划；当前只允许按批次推进，不得越界实现 Stage 4 动画求值。
- CXT v1、播放前参数、Template Binding 和运行时脚本无限期延后子决策已于 2026-08-10 接受。
- `.cxt` 是 UTF-8 JSON 声明式模板，不是脚本、字节码或 SDK 隐式内置实现。
- 已接受合同区分 CXC package identity 与跨 source Prepared semantic identity，并保持 FrameSnapshot
  和 FrameDigest v3 不变。

格式入口：[formats/README.md](formats/README.md)。

## 脚本边界

运行时脚本和逐帧脚本回调无限期延后，不属于任何已排期阶段。当前不预留 Chart/CXT/CXC 字段、
extension、capability、字节码、模块 ABI 或 Playback 执行入口。离线 authoring generator 可以
作为未来独立工具讨论，但不会进入 CXC，也不会被 pack、prepare 或 Playback 隐式执行。

## 尚未完成的主要能力

- 正式 `cuexis_judgement`、InputEvent、ReplayData 和确定性回放
- Studio 独立应用实现
- 稳定 C ABI 和语言绑定
- CFU-D3 运行时等价、外部旧 Chart 资产确认、CFU-F hosted 确定性/安全门禁和最终跨平台产品门禁
- Stage 4 AnimationSystem 运行时实现

## 状态更新规则

任何阶段状态变化必须同时更新本文和对应的阶段报告或审查报告。历史报告不得被改写成新的
验证结果；应在顶部增加快照说明，并链接后续关闭证据。

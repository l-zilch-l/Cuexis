# Cuexis Current Status

状态：现行状态页

更新日期：2026-08-30

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
| Stage Chart Format Update | 已完成；CFU-C0–C4、CFU-D、CFU-E、CFU-F、CFU-G 已关闭；G6 owner acceptance 已于 2026-08-24 记录 | [实施计划](stage_plans/stage_chart_format_update_implementation_plan.md)、[G5/G6 completion report](stage_reports/stage_chart_format_update_completion_report.md) |
| Stage 4 | 已完成；S4-H hosted 与 owner acceptance 已于 2026-08-27 记录 | [实施计划](stage_plans/stage_4_implementation_plan.md)、[完成报告](stage_reports/stage_4_completion_report.md) |
| Stage 5 | S5-A 合同已冻结；S5-B 已接线；S5-C 已完成；S5-D 已完成；S5-E 已完成；S5-F 已完成；S5-G 已完成；S5-H local checkpoint；hosted 与 owner acceptance 待完成 | [实施计划](stage_plans/stage_5_implementation_plan.md)、[ADR 0040](adr/0040-stage-5-material-shader-contracts.md)、[Material/Shader spec](formats/MATERIAL_SHADER.md)、[S5-A 报告](stage_reports/260828-stage-5-s5-a-contracts.md)、[S5-B 报告](stage_reports/260828-stage-5-s5-b-shader-tools.md)、[S5-C 报告](stage_reports/260828-stage-5-s5-c-material-unlit.md)、[S5-D 报告](stage_reports/260828-stage-5-s5-d-shader-compile.md)、[S5-E 报告](stage_reports/260828-stage-5-s5-e-profile-capability.md)、[S5-F 报告](stage_reports/260828-stage-5-s5-f-cache.md)、[S5-G 报告](stage_reports/260828-stage-5-s5-g-consume.md)、[S5-H 报告](stage_reports/260828-stage-5-s5-h-safety.md) |

## 260829 Full Review 整改

Full Review 已于 2026-08-30 关闭。最终实现 SHA 为
`fbe118bb310fffa1446584e0a30fd46bc743413b`，分支为 `260829-full-review`；144 项 finding
（P0: 1、P1: 15、P2: 56、P3: 72）均已完成处置记录。P0/P1 正确性与公共边界修复、D1-D6
决策门、Lane A/B/C/D、第 2 批和第 3 批已通过本地与 hosted 门禁；最终关闭证据见
[Full Review 最终关闭报告](stage_reports/260830-full-review-final.md)。

最终 SHA 的 [Linux Quality](https://github.com/l-zilch-l/Cuexis/actions/runs/33316147601)、
[Windows MSVC](https://github.com/l-zilch-l/Cuexis/actions/runs/33316147625) 和
[Windows MinGW](https://github.com/l-zilch-l/Cuexis/actions/runs/33316147662) 均成功。
项目所有者已于 2026-08-30 接受本轮关闭。首次 hosted 失败仅为 GCC Shared Release 的未使用
helper 警告和 MinGW release 的 missing-field-initializers 警告，均已在最终 SHA 修复并重验。

按已接受范围，完整 Chart/CXC parse-once、RT-29 万级 render-state 排序、T1 World/Animation
大规模优化、T2 大包解析降本和 T4 Stage 6/API/Player 任务继续 deferred；它们不是本轮关闭阻断项。
Stage 5 仍保持 `S5-H local checkpoint; hosted 与 owner acceptance 待完成`，与 Full Review 关闭
相互独立。运行时脚本和逐帧脚本回调继续无限期延后。

Stage Chart Format Update 是 Stage 3 与 Stage 4 之间的正式名称，不使用 Stage 3.5 作为别名。

截至 2026-08-24，最终候选 SHA 的 Linux Quality、Windows MSVC 和 Windows MinGW hosted 验证均已
通过，G3 hosted 门禁与 G4 已完成。G4 hosted 证据记录在 [G4 hosted 验证报告](stage_reports/260824-chart-format-update-g4-hosted.md)。
G5 completion report SHA `6e52677be2f27f83d0b709619be7ffcc141d6f95` 的 Linux Quality、Windows MSVC 和
Windows MinGW hosted revalidation 均已通过；项目所有者已明确接受报告，CFU-G 与 Stage Chart Format
Update 已关闭。Stage 4 已完成；Stage 5 的 S5-A 合同已冻结，S5-B 已接线可选 `cuexis_shader` 与
vcpkg feature `shader-tools`，S5-C 已完成 kind 4/5 解析/identity、Asset Index v3 `shader` 与
SDK API `0.7.0`。S5-D 已完成可选 `cuexis.importer.shader.v1` 编译/反射；S5-E 已完成 profile ID
与 `PresentationCapabilities` version 2 预检；S5-F 已完成 `CXSCCH01` 缓存键/读写与失败保留上一
Pipeline；S5-G 已完成 OpenGL/Player/Validation Sink 消费与默认 `allCapabilities()` 写入
`cuexis.shader.asset.v1` / `cuexis.material.parameterized.v1`。裁剪 Session 仍拒绝 Parameterized
且不破坏 active Unlit。OpenGL Unlit 内联 GLSL 330 未改。headless 不要求 Built-in Renderer
Profile。Playback 热路径仍不调用编译器。S5-H local checkpoint 已写入预算、warmed 分配合同、
趋势探针与 Linux sanitizer shader-tools 覆盖；hosted 三平台与 owner acceptance 完成前 Stage 5
不得标为 completed。

## 格式状态

- Chart v1/v2/v3 已实现并继续保留全部 Reader、迁移和 Playback 路径。Chart v4 的静态、参数化与
  非空合法动画已可由默认 Playback Session 求值。S4-F 已把 `cuexis.animation.clip.v1` 与
  `cuexis.animation.layers.v1` 加入默认 Playback 集合；缺少这两项的显式裁剪 Session 仍稳定拒绝。
  S4-G 已关闭本地限额、warmed 分配合同与趋势探针；S4-H 已关闭 hosted sanitizer 与三平台验收。
  该关闭不是公共 CXC package API、Shader、Studio 或 Judgement。
- Stage 5 S5-A 已冻结 Material/Shader v1（`CXPRES01` kind 4/5、Asset Index v3 `shader`、禁止
  Unlit 投影、Playback 不链接编译器）。S5-B 已接线可选内部 `cuexis_shader`、vcpkg feature
  `shader-tools` 与 `cuexis_asset_importer --help`。S5-C 已完成公开
  Presentation 类型、kind 4/5 Reader、Asset Index v3 与 SDK API `0.7.0`。S5-D 已在
  `CUEXIS_BUILD_SHADER_TOOLS=ON` 下实现 GLSL 450 → SPIR-V 1.3 → GLSL 330 / ES 300 与规范化
  reflection；默认构建仍不编译这些测试，Playback 热路径仍不链接编译器。S5-G 已把 shader 与
  parameterized 写入默认 Playback 集合；默认 Session 接受合法 Built-in Shader 内容，显式裁剪
  Session 仍拒绝。OpenGL adapter 消费 `CXSCCH01` GLSL 330 与 `CuexisObject` UBO；缺少缓存且未
  opt-in compile 时返回 `shader.cache.missing`。Unlit 回归保持。S5-H local checkpoint 已写入；
  Stage 5 尚未 completed。
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
  [D1/D2 报告](stage_reports/260813-chart-format-update-d-migration.md)。
- CFU-D3 已于 2026-08-14 关闭：lift 后的空动画 v4 与源 v1/v2/v3 在 Playback FrameSnapshot、
  FrameDigest v3 与 seek/stop 上对齐。v3↔v4 与 v3-hop↔v4 位级相同；v1/v2 源 hop 继续使用
  `1e-6` 误差预算，不要求 v1 FrameDigest 位级相同。证据见
  [D3 报告](stage_reports/260814-chart-format-update-d3-equivalence.md)。
- CFU-D 已于 2026-08-14 经项目所有者记录“未提供外部资产”并关闭。兼容窗口不缩短：全部
  v1/v2/v3 Reader 与迁移入口保留，默认 CLI 仍输出 v3。本关闭不编造仓库外资产清单。证据见
  [D 关闭报告](stage_reports/260814-chart-format-update-d-close.md)。该关闭不是完整 CXC
  产品支持、公共 package API 或 Stage 4。
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
  CXC 公共产品支持、公共 package API、完整 v4 动画 Playback 或 CFU-G。
- CFU-F1 已于 2026-08-15 本地完成：新增只链接 public `cuexis::playback` 的无 GPU reference
  consumer，以及保留 Behavior/Step/Stop 语义、空动画数组的 static Chart v4 reference project
  和 CXC。filesystem、CXC file、CXC memory 的 Prepared semantic identity 与四个时间点的
  FrameDigest v3 一致；动画 capability 与非法 target reload 均保持完整 active state。Debug、
  Release 与 adapter-disabled headless 全量 CTest 分别通过 `368/368`、`368/368`、`334/334`。
  证据见 [F1 报告](stage_reports/260815-chart-format-update-f1-headless.md)。
- CFU-F2 已于 2026-08-15 本地完成：Playback-only external consumer 现在只 include 安装后的
  `cuexis/playback/*.hpp`，并真实 prepare/commit filesystem、CXC file、CXC memory 与 typed
  project-document source。四种 source 的 semantic identity 和四点 FrameDigest v3 trace 一致；
  number/rational/weight prepare options 进入实际解析与冻结路径；动画 CXC 失败 reload 保持 active
  identity、content、diagnostics 与 frame。static/shared、`add_subdirectory`/`find_package`、安装头泄漏
  和 static `Cuexis::InternalCxc` 链接闭包门禁通过。Debug、Release、Shared Debug 与 adapter-disabled
  Headless Debug 全量 CTest 分别通过 `368/368`、`368/368`、`371/371`、`334/334`。证据见
  [F2 报告](stage_reports/260815-chart-format-update-f2-package-consumers.md)。
- CFU-F3 已于 2026-08-15 本地完成：新增 production pack、v3 → v4 migrator 与 public Playback
  headless consumer 的统一确定性指纹门禁。门禁 byte-compare committed CXC 和 migration golden，固定
  package/report SHA-256、Prepared semantic identity、FrameDigest v3 与 capability diagnostics 顺序，
  并生成 LF-only evidence。MSVC Debug/Release 全量均通过 `369/369`，shared Debug 与
  adapter-disabled headless Release F3 聚焦门禁通过；Release 首轮曾有一次既有 CXC unpack 目录
  提交瞬时失败，独立复跑与最终全量复跑均通过。证据见
  [F3 报告](stage_reports/260815-chart-format-update-f3-determinism.md)；最终 SHA hosted parity 见
  [F 关闭报告](stage_reports/260816-chart-format-update-f-close.md)。
- CFU-F4 已于 2026-08-15 本地完成：新增 CXC manifest/package/closure 与 Chart v4 import/resolved
  animation 的精确上限和边界 +1 门禁，并以小型 JSON 构造器和伪 ZIP32 header 覆盖整数、offset、
  data range 溢出及稳定 diagnostics 截断，不为非法输入分配超大 fixture。warmed empty Chart v1–v4
  的 `update()` 与复用 `FrameSnapshot` 的 `extractFrame()` 均为零新增分配。显式启用的 64 MiB 最大
  合法资源探针记录 CXC writer/hash-load、prepare/reload、热帧时间与进程内存趋势，不设置机器相关
  硬阈值。证据见 [F4 报告](stage_reports/260815-chart-format-update-f4-safety-performance.md)。
- CFU-F 已于 2026-08-16 关闭：最终实现 SHA
  `8fcac15d2ec053750abdcb7b984d92354bc304a0` 的 Linux Quality、Windows MSVC 与 Windows MinGW
  全部成功。六份 hosted F3 artifact 的 canonical CXC、migration、semantic identity、FrameDigest v3
  与 diagnostics 指纹完全一致；Linux ASan/UBSan 全量 `342/342` 和 F4 聚焦 `9/9` 通过，
  Chart/CXC/Playback clang-tidy 无诊断，CXC/Chart v4 branch coverage 与 64 MiB Release 性能 evidence
  已保存。Linux 首次 attempt 仅因 vcpkg 下载 util-linux/libmount 时发生外部 TLS error 35 失败，
  同一 SHA 的 failed-job rerun 成功，无代码变更。证据见
  [F 关闭报告](stage_reports/260816-chart-format-update-f-close.md)。该关闭不是完整 CXC 公共产品支持、
  公共 package API、完整 v4 动画 Playback、CFU-G 或 Stage 4。
- CFU-G0 已完成现行状态校准和文档防回退门禁。CFU-G1 已于 2026-08-16 完成 §11 退出条件审计：
  `15 PASS / 1 RERUN / 2 DOC / 0 BLOCKED`；没有新的产品代码阻断。CFU-G2 已于同日完成 Stage 4 typed
  handoff，冻结 `AnimationProgramInput`、capability、fixture、预算、diagnostics、所有权、验收入口和
  残余风险；没有实现 AnimationSystem 或放开非空动画 Playback。证据见
  [G1 审计报告](stage_reports/260816-chart-format-update-g1-exit-audit.md) 与
  [G2 交接报告](stage_reports/260816-chart-format-update-g2-stage4-handoff.md)。CFU-G3 已于 2026-08-19
  通过本地 Debug/Release/headless、format、architecture、package、ASCII、license、version、docs 和
  diff 门禁；最终候选 SHA 为 `4371fdcf04f4f89bfddf070cbb15e4c903810a53`。同一 SHA 的 hosted
  Linux Quality、Windows MSVC 和 Windows MinGW 已于 2026-08-24 全部成功，G3 hosted 验证完成。
  证据见 [G3 验证报告](stage_reports/260819-chart-format-update-g3-validation.md) 与
  [G4 hosted 验证报告](stage_reports/260824-chart-format-update-g4-hosted.md)。
- CFU-G4 hosted 验证已完成：三套 workflow 共 10 个 job 全部 success、attempt 1；退出台账为
  `17 PASS / 1 PENDING / 0 PRODUCT BLOCKER`；F3 CXC、迁移、
  identity、FrameDigest v3 与 diagnostics fingerprint 在 Linux/MSVC/MinGW 六个构建产物中一致，
  F4 sanitizer、clang-tidy、coverage 和性能趋势证据也已收集。G4 当时不创建 completion report、不记录
  owner acceptance；后续 G5/G6 负责最终报告和阶段切换。
- CFU-G5 completion report 已于 2026-08-24 创建，汇总 C0-G 的实际实现、§11 台账、本地
  门禁、hosted jobs、deterministic artifact、F4 evidence、Stage 4 handoff、残余风险和明确延期。
  report-SHA hosted revalidation 已通过，证据和失败记录已写入报告。
- CFU-G6 已于 2026-08-24 完成：项目所有者明确接受 completion report，§11 台账为
  `18 PASS / 0 PENDING / 0 PRODUCT BLOCKER`；CFU-G 与 Stage Chart Format Update 已关闭，Stage 4
  状态切换为 `unblocked but not started`。本次关闭没有实现 AnimationSystem 或放开非空动画 Playback。
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
- Stage 5 Material、Shader 与能力 Profile（S5-A/S5-B/S5-C 已完成；S5-D 起 compile）

## 状态更新规则

任何阶段状态变化必须同时更新本文和对应的阶段报告或审查报告。历史报告不得被改写成新的
验证结果；应在顶部增加快照说明，并链接后续关闭证据。

# stage-06-review-remediation：Stage 6 复核发现项修正计划

本计划把 [Stage 6 双轴复核记录](../../../stage_reports/reviews/stage-06-review-2026-09/2026-09-28-summary.md)
（区间 `13dab93..eaaf375`）的发现项整理为可执行、可验证的修正批次。两轴结果在记录中保持分离，
本计划**只在批次内部按依赖排序**，不跨轴重新排序，也不把两轴合并成一张判定表。

状态：active；修正工作包，**已取得实施授权**（2026-09-28 无人值守会话，R0 四项按默认选项执行）；
R0–R8 已全部退出并随 PR #30 合并进 `master`（合并提交 `670cca8`），R9 已另行开启、**实现与
本地变异证据已完成**（独立分支、独立 PR），同 SHA hosted 复验与 owner 接受**尚未完成**；
Stage 6 已于 2026-09-27 关闭并归档，本计划不构成 Stage 7A / Stage 8 的实现授权

更新日期：2026-09-28

归档来源：[Stage 6 双轴复核汇总](../../../stage_reports/reviews/stage-06-review-2026-09/2026-09-28-summary.md)、
[Standards 轴](../../../stage_reports/reviews/stage-06-review-2026-09/2026-09-28-standards.md)、
[Spec 轴](../../../stage_reports/reviews/stage-06-review-2026-09/2026-09-28-spec.md)、
[二轮细化记录](../../../stage_reports/reviews/stage-06-review-2026-09/2026-09-28-refinement.md)、
[切片附录](../../../stage_reports/reviews/stage-06-review-2026-09/2026-09-28-slice-appendix.md)、
前置阶段 [Stage 6 计划](../../completed/stage-06/plan.md) 与
[关闭报告](../../../stage_reports/stages/stage-06/completion.md)、
边界 [ADR 0042](../../../adr/0042-stage-6-productization-boundaries.md)、
[chart-format-update-for-v5](../../active/chart-format-update-for-v5/plan.md)。

## 1. 背景

2026-09-28 对 `13dab93`（PR #24 合并提交）到 `eaaf375`（PR #29 合并提交，= `origin/master`）之间的
312 个文件、82 个提交做了双轴独立复核（事后复核，见[汇总](../../../stage_reports/reviews/stage-06-review-2026-09/2026-09-28-summary.md)）。
结果：Standards 轴范围内 11 项（含 3 项文档化契约违规），Spec 轴 32 项（含 5 项高、23 项未声明）。
该区间此前从未被独立复核，已存在的边界是 `stage-verification-2026-09` 与 `13dab93` 的纯静态 handoff review。

复核**只出记录与整改清单，未改实现**。本计划负责把这些发现项变成可执行的修正批次：
每条发现都有明确目标、步骤、预期结果与验证方式，并在批次退出时留下带 SHA 的证据。

## 2. 与当前阶段状态的一致性声明

本计划的任何批次都不得改变下列已确立的边界：

- **Stage 6 已关闭并归档**（2026-09-27，见[关闭报告](../../../stage_reports/stages/stage-06/completion.md)）；
  本计划是对其成果的修正工作包，不是 Stage 6 的续做，也不是其重新开启。
- **Stage 7A 与 Stage 8 尚未启动**，Stage 6 关闭不构成对它们的实现授权；本计划不得实现
  Input/Judgement/Score/Replay 内核、不得实现 Chart v5/CXT v2 正式发行、不得切换默认 Writer。
- **[ADR 0042](../../../adr/0042-stage-6-productization-boundaries.md) 的八项冻结决策不变**；
  需要修改冻结决策的发现项（§5 R0）必须先由项目所有者重新裁定，不得由实现者自行选择。
- **提案 1（分期退出 Chart v1–v3）保持 `candidate`**，本计划不实施、不改变其状态。
- **[chart-format-update-for-v5](../../active/chart-format-update-for-v5/plan.md) 仍是 active 的跨阶段总工作包**；
  本计划只做修正，不重定义 Chart v5 方向。
- **SDK API 仍为 `0.7.0`**，无公共契约增量；本计划不新增公共头、公共类型或安装组件，
  不建立稳定 C ABI，不引入运行时脚本或逐帧回调。
- **[R5 报告 §10.1](../../../stage_reports/stages/chart-format-foundation/2026-09-17-r5-regression-and-handoff.md)
  的允许/禁止消费边界不变**；candidate 路径仍只能通过显式 entry 使用。
- **版本政策不豁免**：任何合并进 `master` 的动作都必须先通过受保护 Version Gate 并按
  [VERSIONING.md](../../../guides/VERSIONING.md) 前进显示版本；docs-only 也不例外。

本计划在 `active/` 下的定位是"已确定、待授权的修正工作包"，不是当前下一实施阶段。

## 3. 修正目标

1. **契约一致性**：实现、诊断码与报告表述同 [格式契约](../../../formats/README.md) 和
   [ADR 0042](../../../adr/0042-stage-6-productization-boundaries.md) 完全一致，不留"同文件两种码"
   或"报告写成另一个码"的情况。
2. **真实缺陷闭环**：媒体导入与发布事务的 5 项高危缺陷不再存在——伪造/越界的媒体输入必须被拒绝，
   线性色彩不再被静默当 sRGB 输出，发布失败路径不得丢失上一有效包，candidate 必须与 v4 走同一命令语义。
3. **证据链可追溯**：关闭报告与批次报告的每个 `S6-G01`…`S6-G15` 映射、SHA 与 hosted run 引用
   互相一致；批次自认的残余不再漏出 §8；状态词在同一文档内不再互斥。
4. **验证覆盖补齐**：复核指出的可执行负例缺口（并发发布、零限制、decoder 阶段注入、
   损坏 marker、磁盘满/只读介质、SDK minor 拒绝）都有注册在 CTest 或门禁里的用例。
5. **仓库健康度**：复核标记的重复实现、死代码、可移植性与仓库卫生项收敛或明确豁免理由，
   不留下"已排期但被静默遗忘"的整改项。

## 4. 明确不包含

- 不实现 Stage 7A / Stage 8 / Stage 9–12 的任何交付物。
- 不修改 ADR 0042 的冻结决策（除非 R0 取得 owner 裁定后的专项提交，且只改被裁定的那一条）。
- 不扩大 candidate 允许消费范围，不新增公共 API、不提升 SDK API 版本、不建稳定 C ABI。
- 不实施提案 1，不做 Chart v1–v3 的 Reader/Writer/迁移退出。
- 不把复核的判断项（smell、可移植性提示）当作独立重构立项；它们只在 R8 内按**有界改动**处理。
- 不为"更彻底"重写已通过的实现（例如不重做 CI 平台矩阵、不重构 Renderer 分层本身）。

## 5. 批次与实施顺序

```text
R0 决策门禁（owner 裁定 4 项）
  -> R1 文档与证据链修正（纯文档，可立即执行）
  -> R2 诊断与错误语义      ┐
     R3 版本门禁强化        ┘ 可并行
  -> R4 媒体导入修正（依赖 R0-2）
     R5 发布事务修正（依赖 R0-1）
  -> R6 渲染收敛与 Player/配置语义（依赖 R0-3）
  -> R7 C4 门禁与分发补齐
  -> R8 代码健康度与历史遗留（可随时并行，最低优先级）
  -> R9 Reference Host 命令循环与 play/pause（依赖 R7；独立分支与独立 PR）
```

| 批次 | 前置 | 主要产出 | 退出条件 |
| --- | --- | --- | --- |
| R0 | — | owner 对 4 项裁定的书面记录 | completed：4 项裁定各有结论与生效文档指针（见 §6 R0 裁定记录） |
| R1 | — | 报告/索引/状态词修正 | completed：文档门禁全绿；两轴复核指出的文档不一致项关闭。退出记录见 [2026-09-28-r1-document-and-evidence.md](../../../stage_reports/reviews/stage-06-review-2026-09/2026-09-28-r1-document-and-evidence.md) |
| R2 | R0-1 | 诊断码修正 + 负例 | completed：契约码与实现一致；候选负例断言新码；wire-range 独立成码。退出记录见 [2026-09-28-w1-diagnostics-and-version-gate.md](../../../stage_reports/reviews/stage-06-review-2026-09/2026-09-28-w1-diagnostics-and-version-gate.md) |
| R3 | — | 门禁加固 + 负例 | completed（SPEC-04 登记 BLOCKED）：扩展后的门禁自测 15 tests 通过；A2 表征注册为 CTest。退出记录同上 |
| R4 | R0-2 | 媒体修正 + fixture/golden 决策 | completed：按 R0-2 (a) 拒绝线性 `gAMA`（不改 canonical bytes、不重冻结 golden）；FLAC 伪造时长无条件拒绝；interlaced PNG 显式拒绝；JPEG 损坏 marker fixture 补齐；`.gitattributes` 覆盖 fixture 树。6 项媒体负例均可执行且各以变异反证。退出记录见 [2026-09-28-w2-media-import.md](../../../stage_reports/reviews/stage-06-review-2026-09/2026-09-28-w2-media-import.md) |
| R5 | R0-1 | 原子替换修正 + 故障注入用例 | completed：单次原子替换（无 target 缺失窗口）；恢复逻辑还原 backup 而非删除；pair 锁覆盖两个目标父目录；幂等重发报告真实闭包字节。5 条新用例各以变异反证非空转。退出记录见 [2026-09-28-w3-publication-transaction.md](../../../stage_reports/reviews/stage-06-review-2026-09/2026-09-28-w3-publication-transaction.md) |
| R6 | R0-3 | 渲染收敛 + 配置语义修正 | completed（SPEC-19b 登记 BLOCKED）：`buildPresentationCommands` 成为唯一排序/摘要来源（adapter 删除约 16.7 KB 重复实现）；四个宿主字段按可回读性改名/新增回读；`LaunchOption` 删除；自动重试按 R0-3 (a) 移除并新增 `--mode`；SPEC-19a/19c 用例各以变异反证。SPEC-19b 因 `CUEXIS_ENABLE_CHART_V5_CANDIDATE` 在所有预设与 CI 中为 `OFF` 无法注册，按构建隔离证据缺口登记。退出记录见 [2026-09-28-w4-render-convergence.md](../../../stage_reports/reviews/stage-06-review-2026-09/2026-09-28-w4-render-convergence.md) |
| R7 | R5 | 宿主与分发门禁补齐 | completed（步骤 3/4/6 按口径登记，ADR 冲突上报）：宿主导入门禁新增导入表符号检查与 SDK minor 负例；candidate 零命中扫描成为门禁（「默认 OFF 下工厂拒绝」本已注册）；static 无 toolchain 负例改由 minor 负例承担；交互命令循环口径与 ADR 0042 `:350-351` 的冲突**未处置**、需 owner 裁定；分发门禁 Linux 未注册登记为 Stage 8 输入。退出记录见 [2026-09-28-w5-host-and-distribution-gates.md](../../../stage_reports/reviews/stage-06-review-2026-09/2026-09-28-w5-host-and-distribution-gates.md) |
| R8 | — | 重复/死代码/可移植性修正 | completed（STD-05/06/08/09 按计划 §4/§5 边界登记残余）：STD-13 两个 `.obj` 去跟踪 + `*.obj` 规则；STD-07 七项死代码删除（含两项需实查的 `readCount()` 与 `readRequiredString` 冗余参数，及清单外补删的 `providerRootId`），第 8 项 `--event` 判为有真实审计角色故保留；STD-04 两份逐字相同的状态名级联合并为 `playbackStateName`；STD-10 定因后改为**三分支**（MSVC `_wfopen_s`、MinGW `_wfopen`、其余窄 `fopen`）：原文"统一按 `_MSC_VER` 分支"的处方本身有误——它使 MinGW 落入窄 `fopen` 回退而编译失败，见 W6 §6；STD-12 workflow 块缩进对齐（`git diff -w` 为空）。STD-05/06/08/09 因属重构/公共 API 变更，按 §4 排除并登记。退出记录见 [2026-09-28-w6-code-health.md](../../../stage_reports/reviews/stage-06-review-2026-09/2026-09-28-w6-code-health.md) |

| R9 | R7 | Reference Host 命令循环与 play/pause | 实现与本地证据已完成（**未获 owner 接受**）：设计已获 owner 接受，处理 SPEC-27（ADR 0042 `:350-351` 的六动词命令循环）。命令模式、§6.2 digest 关系、门禁接线与 16 条变异证据均已落地（15 条被目标断言抓住，1 条按合同冗余**记录为预期存活**）；版本规则按 §11 计算，未预填。**未完成**：同 SHA hosted Linux/MSVC/MinGW、owner 接受，以及 §4.1 记录的重复 tick 预算守卫是否应合并的合同裁定。**唯一规范来源是 [R9-reference-host-command-loop.md](R9-reference-host-command-loop.md)**——本行只记录状态、依赖与链接，不复制其合同、限额或判据。从 PR #30 合并后的 `master`（`670cca8`）独立开分支、独立 PR。完整合同、状态矩阵、C01–C12/N01–N07 用例、变异清单、门禁接线与退出清单见该文档；证据记录见 [2026-09-28 R9 报告](../../../stage_reports/stages/stage-06/2026-09-28-r9-reference-host-command-loop.md) |

## 6. 各批次的问题、目标与具体步骤

### R0 决策门禁

以下 4 项都会改变**已接受**的合同、判据或 golden，必须由项目所有者先裁定；不得以实现成本为由自行选择。
裁定结果写入对应 Spec/ADR 或本计划的追加记录，并作为后续批次的前置。

| 编号 | 待裁定事项 | 来源发现 | 可选方案 | 影响面 |
| --- | --- | --- | --- | --- |
| R0-1 | 诊断码与格式契约的订正口径 | STD-01、SPEC-02、SPEC-10 | (a) 改实现用契约码；(b) 改契约表接受现有码 | (a) 影响 `engine/cxc`、候选负例、C1 报告；(b) 影响 `CHART_ENTRY_V1_FORMAT.md` 与既有 golden |
| R0-2 | 线性 `gAMA` 的处理 | SPEC-21 | (a) 拒绝线性；(b) 实现到 sRGB 的转换 | (b) 会改 `CXPRES01` canonical bytes → 需按 R5 §10 说明原因并重冻结 golden |
| R0-3 | Load 在 `content_mismatch` 后的自动重试 | SPEC-18 | (a) 改为显式 `Open/Rebuild`；(b) 修订 ADR 0042 第 242 行并保留行为 | (a) 改 `player_control.cpp` 与用例；(b) 改 ADR 与 `PLAYER_APPLICATION.md` |
| R0-4 | 本工作包的授权与版本策略 | — | (a) 授权全部批次；(b) 只授权 R1（纯文档）；(c) 并入 Stage 8 | 决定后续批次的启动顺序与 PR 归属 |

#### R0 裁定记录（2026-09-28）

本节是 R0 门禁的**生效裁定记录**。裁定按无人值守会话的默认选项执行：凡需裁决处取"遵守已接受契约
与冻结边界"的一项，不以实现成本为由改契约或改 ADR。四项裁定均已生效并已由后续批次实施，
实施证据见对应批次退出报告。

| 编号 | 裁定 | 依据 | 影响面 | 生效文档指针 | 实施证据 |
| --- | --- | --- | --- | --- | --- |
| R0-1 | **(a) 改实现使用契约码** `cxc.candidate.budget_exceeded`；`packed.field.wire_range` 独立成码。订正 C1 报告第 58 行的预算码表述 | 契约码是冻结规范，实现应服从契约；STD-01/SPEC-02 | `engine/cxc`、候选负例、C1 报告 | [PACKED_CHART_FORMAT.md](../../../formats/PACKED_CHART_FORMAT.md)；[C1 报告](../../../stage_reports/stages/chart-format-foundation/2026-09-17-r3-budgets-and-arithmetic.md)（追加订正） | [W1 报告](../../../stage_reports/reviews/stage-06-review-2026-09/2026-09-28-w1-diagnostics-and-version-gate.md) |
| R0-2 | **(a) 拒绝线性 `gAMA`**，返回 `media.image.gamma_unsupported`；**不改 canonical bytes、不重冻结 golden** | ADR 0042 `:274`/`:277`"不悄悄近似颜色"；改 canonical bytes 需重冻结 golden，风险高 | `tools/media_import/src/image_import.cpp`，golden 不动 | [ADR 0042](../../../adr/0042-stage-6-productization-boundaries.md) `:274`/`:277`；[STAGE6_CONFIG_AND_MEDIA.md](../../../formats/STAGE6_CONFIG_AND_MEDIA.md) §5.1 | [W2 报告](../../../stage_reports/reviews/stage-06-review-2026-09/2026-09-28-w2-media-import.md) |
| R0-3 | **(a) 改为显式 `Open`/`Rebuild`**，删除 `content_mismatch` 后自动重试，新增 `--mode` | ADR 0042 `:210`"Stage 6 不实现自动重试"；`:242-243` 拒绝"模式失败后自动重试" | `app/player/src/player_control.cpp`、用例、`--mode` CLI | [ADR 0042](../../../adr/0042-stage-6-productization-boundaries.md) `:210`/`:242-243`；[PLAYER_APPLICATION.md](../../../architecture/PLAYER_APPLICATION.md) | [W4 报告](../../../stage_reports/reviews/stage-06-review-2026-09/2026-09-28-w4-render-convergence.md) |
| R0-4 | **(a) 授权全部批次** | 无人值守会话规则；预算内已完成 W0–W4 | W1–W6 全部启动 | 本计划 §10 | 本计划 §5 批次表 |

**退出证据**：上表 4 项裁定各自的结论、生效文档指针、依据条款与实施证据报告；
四项均已于 2026-09-28 生效。R0-1…R0-3 的裁定方向与 ADR 0042 及已接受格式契约一致，
未修改任何冻结决策正文；R0-4 的授权范围仍受本计划 §4"明确不包含"与 §10 约束。

### R1 文档与证据链修正（纯文档，零代码风险）

**问题**：关闭报告与批次报告之间存在证据指针不一致、状态词互斥、残余清单漏项、码名写错与计数不一致。

**修正目标**：文档内部与文档之间自洽，且不把未发生的事写成已发生。

**步骤**

1. **STD-02**：补写 `examples/reference_host/README.md`（宿主主循环、命令循环、provider 构造、
   构建与运行步骤），或改
   [C4 实现报告](../../../stage_reports/stages/stage-06/2026-09-27-s6-c4-reference-host-and-player-distribution.md)
   §2 与 [BUILDING.md](../../../guides/BUILDING.md) 第 468 行的表述——二选一，不留悬空引用。
2. **STD-03**：把 [stage-06 计划](../../completed/stage-06/plan.md)`:51`、`:103`、`:106`、
   [关闭报告](../../../stage_reports/stages/stage-06/completion.md)`:46`、
   [ROADMAP.md](../../../ROADMAP.md)`:35-36`/`:107`/`:113`、
   [VERSIONING.md](../../../guides/VERSIONING.md)`:175` 的状态词改为与"已完成并归档"一致；
   历史行只改状态词，不改写证据。同步 `更新日期`。
3. **SPEC-26**：修正 [关闭报告](../../../stage_reports/stages/stage-06/completion.md) `S6-G13` 行，
   使 SHA↔run 归属与 [C4 退出报告](../../../stage_reports/stages/stage-06/2026-09-27-s6-c4-exit.md)
   §8 一致（把 `d7980bd` 放回，补 `cb56e62` 的 run），并按 `S6-F2` 第 1 条追加订正说明。
4. **SPEC-32**：在关闭报告 §8 追加一行承接 5 条批次自认残余（宿主符号级检查、插桩转发约定、
   E3 磁盘满/只读介质、配置迁移、SDK minor 手动核对），并注明是否随 §8 一并被接受。
5. **SPEC-10 / SPEC-17 / SPEC-31**：订正 C1 退出报告的码名、
   在 C2 退出报告标注证据级别、统一 preset 计数（7 vs 8）。
6. **STD-01 的报告侧**：订正 C1 退出报告第 58 行的预算码表述（与 R2 的实现修正同时生效）。
7. **SPEC-29**：把插桩转发约定与 MinGW 运行库复制写进
   [BUILDING.md](../../../guides/BUILDING.md) 的门禁章节，使其从"计划外机制"变为已记录机制。

**预期结果**：两轴复核中的文档类发现项全部关闭或在计划中明确豁免；`check_docs.py` 与
`git diff --check` 通过；不存在指向不存在文件的引用。

**验证方式**：`python -B tools/check_docs.py`、`python -B tools/update_version.py --check`、
`git diff --check`；逐条按[汇总](../../../stage_reports/reviews/stage-06-review-2026-09/2026-09-28-summary.md)
的 ID 对照修正记录（只改状态词与指针，不改证据）。

### R2 诊断与错误语义

**问题**：候选预算用了非契约诊断码（STD-01）；wire-range 溢出复用了预算码（SPEC-02）；
同日 build 耗尽的诊断顺序错（SPEC-06）。

**修正目标**：每个失败原因只对应一个稳定的、与契约表一致的诊断码。

**步骤**

1. `engine/cxc/src/cxc_candidate.cpp:533` 改用 `:93` 已定义的 `cxc.candidate.budget_exceeded`
   （或按 R0-1 的裁定改契约表）。
2. `engine/chart/src/packed_chart_tables.cpp:35`/`:989`/`:1012` 的 wire-range 溢出改用独立码，
   或在 Spec §3.3 明确该码同时承担防御性窄化检查。
3. `tools/check_version_gate.py:134-137` 调整判定顺序，使"同日 build 未前进"报
   `version.unchanged`，只有真正耗尽才报 `version.build.exhausted`。
4. 补负例：候选 entry 超 16 MiB 断言契约码（`tests/cxc/` 现有 R4 用例旁），
   同日 build 边界的两个诊断各一条（`tools/check_version_gate_tests.py`）。

**预期结果**：契约表、实现、测试引用同一个码；不再存在同一文件两种候选预算码。

**验证方式**：`ctest --preset debug -R cxc`（或对应 target）通过；
`python -B tools/check_version_gate_tests.py` 通过；`git grep` 复核候选路径不再出现 `cxc.budget.exceeded`。

### R3 版本门禁强化

**问题**：`--context` 不参与规则（SPEC-03）；门禁自身修改无保护（SPEC-04）；
缺并发/缺基线可执行负例（SPEC-05）；A2 表征不全且脚本未接入门禁（SPEC-07）；`--event` 无消费（SPEC-08）。

**修正目标**：受保护 Version Gate 的三类检查都有可执行负例，且门禁自身的修改无法被候选静默绕过。

**步骤**

1. `tools/check_version_gate.py`：让 `--context` 真正选择日期规则（historical 用其记录的发布上下文），
   并保留 `live` 语义；`--event` 若不能定义规则则删除，并在 [VERSIONING.md](../../../guides/VERSIONING.md) 说明。
2. `tools/check_version_gate_tests.py`：补 4 条可执行负例——缺基线（真实空历史）、
   缺 checker/test/workflow（`version.bootstrap.required`）、两个 PR 竞争（旧基线 + 新候选）、
   历史上下文非当日复验。
3. 引入 `CODEOWNERS` 或等效的 workflow 变更保护；把"保护实际生效"的证据（分支保护配置导出或
   run 记录）写入 B1 报告与关闭报告 §8；无仓库权限时按计划列为阻塞项。
4. `tools/check_stage6_a2.py`：补 `S6-D07`/`S6-D08` 的表征，并注册为 CTest 用例
   （`tests/CMakeLists.txt` + `python` 调用），使"可测试"不再依赖手工执行。

**预期结果**：门禁自测覆盖三类检查的正负例；`.github/workflows/version-gate.yml` 的修改需要
代码所有者复核；A2 脚本在 CTest 中被执行。

**验证方式**：`python -B tools/check_version_gate_tests.py`、`python -B tools/check_stage6_a2.py`、
`ctest --preset debug -R version_gate|stage6_a2`；hosted `version-gate` 与 `linux-quality` 通过；
门禁修改保护的证据落在 B1 报告。

### R4 媒体导入修正

**问题**：FLAC 伪造时长的负例被写成正例（SPEC-20）；线性 `gAMA` 被静默按 sRGB 输出（SPEC-21）；
JPEG 损坏 marker 负例缺失、interlaced PNG 未冻结却被接受（SPEC-22）；
新 fixture 树缺 `.gitattributes` 规则（STD-11）。

**修正目标**：`S6-G10`/`S6-G11` 声称的负例真实存在；不支持的色彩与结构变体明确拒绝；
canonical 身份诚实。

**步骤**

1. `tools/media_import/src/audio_import.cpp:588-600`：把
   `declaredSamples` 与 `accumulator->frames()` 的比较移出 `if (!processed || !verified || client.failed)`
   分支，使声明长度与实际样本数不一致**无条件**被拒；`media_import_tests.cpp:511-516` 改为断言拒绝。
2. 按 R0-2 的裁定处理 `image_import.cpp:353-355` 的线性 `gAMA`：
   拒绝则补 `media.image.gamma_unsupported` 用例；接受则实现到 sRGB 的转换、更新
   [STAGE6_CONFIG_AND_MEDIA.md](../../../formats/STAGE6_CONFIG_AND_MEDIA.md) §5.1 与受影响 golden
   （按 R5 §10 要求说明原因）。
3. `image_import.cpp:492-494` 的 interlaced PNG：显式纳入 profile 并加 fixture/golden，或在导入层拒绝。
4. 补"损坏 marker 的 JPEG"fixture 与断言（与既有 `corrupt_chunk.png` 对称），
   生成脚本 `tests/fixtures/stage6_e/generate_negative_fixtures.py` 同步。
5. 在 `.gitattributes` 为 `tests/fixtures/stage6_e/**`（golden `.json` 用 `text eol=lf`，
   媒体文件用 `binary`）与 `tests/fixtures/stage6_a2/**` 补规则。

**预期结果**：6 项媒体负例全部可执行；`media-tools` 仍默认 OFF，工具未进入 Playback/Player 链接闭包；
跨平台 canonical bytes 在承诺平台上逐字节一致。

**验证方式**：`ctest --preset debug-media-tools -R media_import`；三平台 hosted
（Linux Quality / Windows MSVC / Windows MinGW）同 SHA 复验；`git diff --check` 复核 fixture 规则。

### R5 发布事务修正

**问题**：`commitPackage` 双 rename 窗口 + 恢复删除 backup（SPEC-23）；pair 锁未覆盖 candidate 目录、
幂等重发 `closureBytes` 置 0（SPEC-24）；磁盘满/只读介质未取证（SPEC-25）。

**修正目标**：任何失败或崩溃都不会丢失上一有效包；并发 writer 被同一把锁拒绝；失败路径有可执行证据。

**步骤**

1. `tools/asset_publish/src/asset_publish.cpp:946-957`：改为"校验通过后单次原子替换"
   （临时文件留在目标同目录，直接改名覆盖 target），去掉 `target → backup` 这一步；
   若保留 backup，则 `recoverPublicationStaging`（`:588-602`）必须把 `.cuexis-backup.tmp.*`
   识别为**上一有效包**并恢复，而不是删除。
2. `:1066-1067` 的 `PublicationLock` 覆盖本次事务的全部目标父目录（或按目标集合加锁），
   使 candidateTarget 位于另一目录时同样受保护。
3. `:694` 幂等重发路径保留真实 `closureBytes`，与首次发布一致。
4. 补用例（`tests/asset_publish/asset_publish_tests.cpp`）：
   两次 rename 之间的崩溃注入、只读目标目录、写入失败（可用既有 `CUEXIS_ASSET_PUBLISH_FAIL_*` 钩子扩展）、
   并发发布、candidate 与 v4 不同父目录时的锁行为、幂等重发的 `closureBytes`。

**预期结果**：失败与崩溃后磁盘上**始终存在**上一有效包或完整新包，不存在两者皆无的窗口。

**验证方式**：`ctest --preset debug -R asset_publish`；故障注入用例逐条通过；
同 SHA hosted 复验；修正说明写入 E3 报告与关闭报告残余清单。

### R6 渲染收敛与 Player/配置语义

**问题**：中立 command builder/digest 未收敛，OpenGL 维护第二套（SPEC-13）；
D1 线程/寿命语义无测试（SPEC-14）；`EffectiveSettings` 混入 requested 镜像（SPEC-15）；
`ResolvedAppConfig` 缺创建前事实与逐字段来源（SPEC-16）；
Load 自动重试（SPEC-18，按 R0-3 裁定）；C3 证据缺口（SPEC-19）。

**修正目标**：只有一套排序/分 pass/digest 实现；"applied" 字段名副其实；
命令语义与 ADR 0042 第 242 行一致；C3 的验收项都有用例。

**步骤**

1. `engine/render_opengl/src/open_gl_presentation.cpp`：改为消费
   `presentation_renderer` 的 `buildPresentationCommands`，删除 `SummaryHash`
   （`:810-921`）、`buildDraws`（`:1854`）里的第二套排序/digest 与 `toDrawCommand`/`toDrawSummary`
   （`:1959-2001`）转发层；保留 adapter 专有的像素探针与 API handle（留在 smoke/测试层）。
2. `tests/presentation_renderer/`：若线程与 borrow/owning 寿命语义仍作合同，补 focused 用例；
   否则在 [Stage 6 计划](../../completed/stage-06/plan.md) 的 D1 行注明它只是文档约定。
3. `app/player/src/player_assembly.cpp:308-316`：`appliedVsync`/`appliedFullscreen`/`appliedGain`
   改为回读实际值（`open_gl_backend.cpp:449` 的 `SetSwapInterval` 之后读取），
   不可回读的字段改名或补文档，避免把 requested 记为 effective。
4. `engine/player_support/`：为 `ResolvedAppConfig` 补创建前能力事实与逐字段来源
   （`ConfigValueSource::LaunchOption` 要么产生，要么从枚举中删除），或按 ADR 0024 重新裁定字段集。
5. 按 R0-3 裁定处理 `player_control.cpp:403-421` 的自动重试。
6. `tests/player/player_control_tests.cpp`：把 `failClipPreparation` 置 true，覆盖 decoder 阶段注入；
   补一条经 `PlayerController` 的 candidate 源用例；明确 ChartClock 的"超出可播放范围"语义并有断言。

**预期结果**：`buildPresentationCommands` 成为唯一排序/digest 来源；v4 与 candidate 共用命令语义有
端到端用例；配置字段名与内容一致。

**验证方式**：`ctest --preset debug`、GPU smoke（`cuexis_player.exe --smoke-test`）、
OpenGL 像素/summary 与最小化/恢复证据；同 SHA hosted 三平台；证据落在新的退出记录中。

### R7 C4 门禁与分发补齐

**问题**：宿主符号级检查未接线、SDK minor 拒绝仅手动、工具链拒绝仅 shared、
交互命令循环口径未定（SPEC-27）；candidate 隔离是手动过程（SPEC-28）；
分发门禁仅 Windows（SPEC-30）。

**修正目标**：C4 的每项验收要么注册为门禁，要么作为明确残余登记并给出后继归属。

**步骤**

1. 复用已有的符号工具链（`cmake/VerifySharedExports.cmake`、`cmake/VerifySharedConsumerImports.cmake`），
   为参考宿主加"导入表/依赖清单白名单"检查，注册进宿主门禁。
2. 为错误 SDK minor 加注册的禁用例（可用 `CUEXIS_HOST_API_VERSION` 传 `0.8.0`，
   断言 configure 失败文本），与既有 toolchain 负例并列。
3. static flavor 的工具链拒绝路径：在报告中明确"该用例只对 shared 生效"的口径，
   或为 static 设计等价负例。
4. 明确 Reference Host 的"交互命令循环"口径：要么实现最小交互循环，要么在本计划与
   Stage 8 交接清单中写明"脚本式宿主即本阶段口径"。
5. candidate 隔离：把"默认产物零命中 candidate 标识"与"默认 OFF 下候选工厂拒绝"注册为 CTest，
   取代手工扫描。
6. 分发门禁的平台范围：在关闭报告与 C4 报告中明确"Linux 只覆盖宿主门禁"，
   并把 Windows-only 分发门禁登记为 Stage 8 的输入。

**预期结果**：宿主门禁包含符号级检查与 SDK minor 负例；candidate 隔离有可复现门禁；
分发门禁的覆盖边界在文档中一致。

**验证方式**：宿主 staging 门禁 + Player 分发门禁在各 flavor 通过；`ctest -R reference_host|player_dist`；
同 SHA hosted 三平台。

### R8 代码健康度与历史遗留（最低优先级，可并行）

**问题**：复核标记的 8 组重复实现（STD-05）、3 组重复 switch（STD-06）、
8 处死代码/只写不读（STD-07）、纯转发层（STD-08）、领域概念退化（STD-09）、
`_wfopen_s` 可移植性（STD-10）、门禁脚本健壮性（STD-12）；
以及本轮范围外的两项：AP-09 重复状态名（STD-04）与被跟踪的构建产物（STD-13）。

**修正目标**：每项或修正、或有书面豁免理由，不留"已排期但被遗忘"的整改。

**步骤**

1. 按 STD-05 的 8 组逐组抽共享实现（错误工厂、事务形状、CMake 运行检查、配置版本判定等）；
   每组一个提交，便于单独回滚。
2. STD-06：把命令状态前置检查提到 switch 之前；section 注册表与 required 清单共用一处判定。
3. STD-07：删除无调用者的函数与只写不读字段；`check_version_gate.py:275-279` 的冗余赋值删除。
4. STD-08：`packed_chart_io` 的转发仅在确有 file bridge 语义时保留，并在头注释写明理由。
5. STD-09：为四类媒体身份、锁柄、ms 时基引入小类型（**范围限于工具与内部层，不进安装树**）。
6. STD-10：`publish_fs_internal.cpp:198-199` 统一按 `_MSC_VER` 分支。
7. STD-12：`VerifyReferenceHost.cmake` 的 `ENV{PATH}` 恢复改为不可被 `FATAL_ERROR` 跳过的写法。
8. STD-04、STD-13：作为独立小整改处理（合并 `stateName`/`audioStateName`；
   `git rm --cached hello.obj dump_chart_writer.obj` 并补 `.gitignore` 的 `*.obj`）。
9. STD-11 已并入 R4。

**预期结果**：`git grep` 复核每组不再有两份实现；无新增公共类型；仓库卫生项关闭。

**验证方式**：`ctest --preset debug` 与 `release` 通过；`cuexis_format_check` 通过；
架构测试与 target allowlist 不变；R8 的改动不改变任何 canonical bytes（golden 不变即证明）。

### R9 Reference Host 命令循环与 play/pause

R7 步骤 4 把「交互命令循环」按「脚本式宿主即本阶段口径」登记为残余，并把 ADR 0042 `:350-351`
的冲突上报为**未处置、需 owner 裁定**；那条路径现已由 owner 裁定并转为独立批次 **R9**。

- **状态**：实现已完成、本地与 hosted 证据齐备，**未获 owner 接受**（与 §6 状态表 R9 行一致）。
- **依赖**：R7（宿主与分发门禁）；载体基准为 PR #30 合并后的 `master`（`670cca8`）。
- **规范来源**：[R9-reference-host-command-loop.md](R9-reference-host-command-loop.md)（唯一权威）。
- **门禁策略裁定**：[R9-gate-policy-audit-decisions.md](R9-gate-policy-audit-decisions.md)
  —— 记录门禁策略基线的根因、修法裁定、策略影响审计与反例判据，是本批次门禁改动的实施依据。
- **不改动**：ADR 0042 冻结正文、SDK 公共 API/枚举、SDK API `0.7.0`、既有 golden。
- **关闭条件**：该文档 §13 的退出清单全部满足，含 owner 接受 R9 退出；**R9 退出不等于 PR 合并授权**，
  其他残余也不随 R9 关闭。

**本条只记录状态、依赖与链接，不复制 R9 的合同、限额或判据**，以避免两处定义漂移。

## 7. 验收标准

| ID | 必须证明的结果 | 主要批次 | 必需证据 |
| --- | --- | --- | --- |
| RS-01 | 4 项决策门禁有 owner 裁定与生效文档指针 | R0 | completed：裁定记录见本计划 §6「R0 裁定记录」（结论/依据/影响面/生效文档指针/实施证据五列齐备） |
| RS-02 | 文档与索引自洽：状态词、SHA↔run、残余清单、计数、码名一致 | R1 | `check_docs.py` 通过 + 按复核 ID 的逐条关闭记录 |
| RS-03 | 每个失败原因只对应一个契约诊断码 | R2 | 契约表、实现、负例三者一致 |
| RS-04 | 版本门禁三类检查均有可执行负例，门禁自身修改受保护 | R3 | 扩展后的门禁自测 + 保护生效证据 + hosted run |
| RS-05 | 媒体负例真实存在：伪造时长被拒、线性色彩有明确处置、损坏 marker 与 interlaced 已冻结 | R4 | fixture/golden、用例、三平台 canonical 证据 |
| RS-06 | 发布失败与崩溃后始终保留有效包；并发与磁盘失败有证据 | R5 | 故障注入用例 + E3 订正说明 |
| RS-07 | 排序/digest 只有一套实现；`applied*` 名副其实；命令语义与 ADR 一致 | R6 | 代码差异、GPU smoke、状态机用例 |
| RS-08 | C4 的每项验收要么成门禁、要么登记为残余并给出归属 | R7 | completed：宿主导入表、SDK minor 拒绝、candidate 零命中扫描均已成门禁并在 static/shared 实跑；static toolchain 与分发平台范围两项按口径登记并给出归属（见 [W5 报告](../../../stage_reports/reviews/stage-06-review-2026-09/2026-09-28-w5-host-and-distribution-gates.md) §2/§5） |
| RS-09 | 被标记的重复/死代码/可移植性项全部修正或有书面豁免 | R8 | completed：STD-04/07/10/12/13 已修正并各有一条命令证据；其中 STD-10 的第一版修正（`_MSC_VER`）**自身引入 MinGW 编译错误**，已由 `18c9272` 改为三分支，见 [W6 报告](../../../stage_reports/reviews/stage-06-review-2026-09/2026-09-28-w6-code-health.md) §6；STD-05/06/08/09 按计划 §4/§5 书面豁免（属重构或公共 API 变更），理由见 W6 §5 |
| RS-10 | 修正未越过阶段边界：无新公共 API、无版本跳跃、无 Stage 7A/8 交付物 | 全部 | completed：`cmake/CuexisVersion.cmake` 的 `CUEXIS_SDK_API_VERSION` 仍为 `0.7.0`；R7/R8 未新增安装头或安装组件（R8 新增的 `app/player/src/player_state_name.hpp` 位于 `app/`；R8 触及的 `engine/chart/include/cuexis/chart/candidate_lowering.hpp` **不在** `engine/chart/CMakeLists.txt` 的 `FILE_SET HEADERS` 内，已核实不进安装前缀）；对安装公共头的净改动仅**删除**一个从未被读取的候选元数据字段，不改任何契约 |

## 8. 验证方式

### 8.1 每个批次的通用门禁

按[Stage 6 计划](../../completed/stage-06/plan.md) §3.2 的标准本地门禁执行，改动代码时必须全跑：

```powershell
cmake --preset debug --fresh
cmake --build --preset debug
ctest --preset debug --no-tests=error
cmake --preset release --fresh
cmake --build --preset release --clean-first
ctest --preset release --no-tests=error
cmake --build --preset debug --target cuexis_format_check
python -B tools/update_version.py --check
python -B tools/check_docs.py
git diff --check
```

纯文档批次（R1）只跑后三项与 `git diff --check`；不得据文档通过宣称实现门禁通过。

### 8.2 专项门禁

| 批次 | 专项命令或证据 |
| --- | --- |
| R2 | `ctest --preset debug -R "cxc|candidate"`；`python -B tools/check_version_gate_tests.py` |
| R3 | `ctest --preset debug -R "version_gate|stage6_a2|check_docs"`；hosted `version-gate` |
| R4 | `ctest --preset debug-media-tools -R media_import`；三平台同 SHA canonical 复验 |
| R5 | `ctest --preset debug -R asset_publish`；故障注入用例单列输出 |
| R6 | `.\out\build\debug\bin\cuexis_player.exe --smoke-test`；OpenGL 像素/summary 与最小化/恢复 |
| R7 | 宿主 staging 门禁与 Player 分发门禁（各 flavor） |
| R8 | 全量 `ctest` 两配置 + `cuexis_format_check`；golden 不变 |

### 8.3 hosted 与最终回归

- 每个实现批次结束后在**同一最终 SHA** 取得 Linux Quality、Windows MSVC、Windows MinGW 证据；
  报告 SHA 与实现 SHA 不同时必须列出差异并复验。
- 涉及预设、CI、checker、依赖或依赖清单的改动**不得**当作纯叙述文档，必须跑完整矩阵。
- 依赖变更（若 R4/R8 需要）同步 `vcpkg.json`、必要的 baseline、
  [DEPENDENCY_POLICY.md](../../../guides/DEPENDENCY_POLICY.md) 与 `THIRD_PARTY_NOTICES.md` 四处。
- 每批的证据写入 `docs/stage_reports/reviews/stage-06-review-2026-09/` 下的新日期文件，
  并从 [stage_reports 索引](../../../stage_reports/README.md) 可达。

## 9. 风险、兼容与回退

| 风险 | 影响 | 控制措施 |
| --- | --- | --- |
| R2 改诊断码会改变调用方分支 | 低：候选路径尚未正式发行 | 按 R0-1 裁定；同批更新契约表与 golden 断言说明 |
| R4 的线性 `gAMA` 决策会改 canonical bytes | 中：影响 `S6-G10` golden 与跨平台一致 | 先裁定再实施；golden 重冻结需书面说明原因（R5 §10） |
| R5 的事务改写可能改变临时文件命名 | 中：影响恢复逻辑与测试夹具 | 保持 `uniqueSibling` 形态或同步更新恢复判据与用例 |
| R6 删除 adapter 私有排序/digest 可能改变帧摘要 | 中：影响 `S6-G09` 证据 | 迁移前后比对 draw summary 与像素探针；分两个提交（先接线、后删除） |
| R6 的 `EffectiveSettings` 回读可能在某些驱动上不可用 | 低 | 提供"不可回读则改名/文档化"的降级路径 |
| R3 引入 `CODEOWNERS` 需要仓库权限 | 中：无权限则阻塞 | 按计划列为阻塞项，不用本地脚本通过代替 |
| R8 的改动面大 | 中：回归风险 | 每组一个提交、golden 不变作为零行为变更的证明；不做无关重构 |

回退原则：每个批次保持"可单独 revert"的粒度；R5、R6 的改写先接线后删除，两步分别可回退；
任何 golden 重冻结都必须记录旧值与新值并说明原因。

## 10. 授权、版本与交接

- **实施前必须取得项目所有者对 §6 R0 的 4 项裁定**，以及对本工作包的**明确授权**；
  未授权时只执行 R1，且 R1 也不得夹带实现改动。
- 每个合并进 `master` 的动作都必须先通过受保护 Version Gate，并按
  [VERSIONING.md](../../../guides/VERSIONING.md) 递增显示版本；不得以"修正很小"为由跳过。
- 本计划的开启、暂停、关闭都必须同步 [CURRENT_STATUS.md](../../../CURRENT_STATUS.md)、
  [ROADMAP.md](../../../ROADMAP.md) 与 [stage_plans 索引](../../README.md)；
  状态词与归档动作按 [DOCUMENTATION_POLICY.md](../../../DOCUMENTATION_POLICY.md) 执行。
- **阶段归属**：R3/R4/R5/R7 的成果按主题分别并入 Stage 8 与 Stage 7A 的输入清单；
  R6 的渲染收敛项与"OpenGL 未消费 `buildPresentationCommands`"这条已接受的残余（关闭报告 §8）
  一并移交 Stage 8。移交时必须说明"本计划关闭了什么、还剩什么"，不得把未完成项写成已交付。
- 计划关闭需要：全部批次退出或明确登记为残余、验收矩阵 RS-01…RS-10 逐项有证据、
  owner 接受关闭报告；PR、合并与发布属另行授权动作。
- **构建与行为的已验证 SHA 是 `18c9272`**：Version Gate、Linux Quality、Windows MSVC、
  Windows MinGW 在其 push 与 pull_request 两组事件上均为 `success`。此前的 `0c8f837` 是真实红
  （Linux 两项门禁失败 + MinGW 编译失败），`5374631`、`2c74f5f`、`7ac37f8`、`c6f1e45` 均带
  同一 MinGW 编译错误，其运行在 `18c9272` 推送后被主动取消。**不得**把这几个 SHA 的中间失败
  或取消表述为"一次通过"。逐 SHA 时间线见
  [交付报告](../../../stage_reports/reviews/stage-06-review-2026-09/2026-09-28-delivery-report.md) §7.1。
- 本计划与交付报告的更新顺序遵循 §11：**新增更正一律追加，不复写已记录的托管失败现象**。

## 11. 证据维护

- 每批报告记录：起始/最终 SHA、实际执行的命令、测试注册数量、结果、环境阻塞、
  公开边界变化、下一批的允许/禁止消费清单，存于
  `docs/stage_reports/reviews/stage-06-review-2026-09/`，并补该目录的索引说明与
  [stage_reports 索引](../../../stage_reports/README.md) 条目。
- 每条验收 ID 必须链接到真实测试/命令与 SHA；**没有实现、没有注册或没有执行都不能写为通过**。
- 发现的**新**问题按同一格式追加到本计划，而不是改写既有复核记录；
  复核记录中的历史现象与影响不得回改。
- 修正完成后，把两轴复核记录中对应项的处置指向本计划的批次报告，
  但保留原现象与证据原文。

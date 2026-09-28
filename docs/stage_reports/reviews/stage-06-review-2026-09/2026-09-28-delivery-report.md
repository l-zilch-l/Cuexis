# Stage 6 复核修正：交付报告

状态：completed
快照日期：2026-09-28
更新日期：2026-09-28

本报告是 [Stage 6 复核修正计划](../../../stage_plans/active/stage-06-review-remediation/plan.md)
的**总交付记录**。它汇总 R0–R8 全部批次的结论、逐条处置两轴复核的发现项、
列出所有**未关闭项及其归属与恢复条件**，并给出可复核的门禁证据。

## 1. 交付范围

| 项 | 内容 |
| --- | --- |
| 分支 | `stage-06-review-workspace` |
| PR | [#30](https://github.com/l-zilch-l/Cuexis/pull/30)，base `master`（`eaaf375`） |
| 基线 | `eaaf375`（PR #29 合并点 = `master`） |
| 复核范围 | `13dab93`（PR #24）→ `eaaf375`：312 个文件、+39278 / −2320 行、82 个提交 |
| 版本 | `26.09.27-1` → `26.09.28-1`（SDK API 仍 `0.7.0`，无版本跳跃） |

**边界遵守**（计划 §2 硬边界，逐条自检）：

- ADR 0042 冻结决策正文**未修改**（R7 发现的冲突以上报处置，见 §4）。
- 未实现 Stage 7A/8/9–12 任何交付物；未实现提案 1（Chart v1–v3 退役）。
- 历史报告的现象与证据原文**未改写**；凡原文有误处一律以"追加订正说明"保留原句。
- 无 `force push`、无 git config 变更、无远程分支删除、未跳过任何 hook。
- **未把任何未运行、未通过、未注册的检查写成"通过"**（见 §5 的两类如实登记）。
- 未越过版本门禁；每批一次版本滚动、串行合并。

## 2. 批次总表

| 批次 | 目标 | 状态 | 退出记录 |
| --- | --- | --- | --- |
| R0 | 4 项决策门禁的 owner 裁定记录 | completed | 计划 §6「R0 裁定记录」 |
| R1 | 文档与证据链订正 | completed | [R1](2026-09-28-r1-document-and-evidence.md) |
| R2 | 诊断码契约对齐 | completed | [W1](2026-09-28-w1-diagnostics-and-version-gate.md) |
| R3 | 版本门禁加固 | completed（SPEC-04 登记 BLOCKED） | [W1](2026-09-28-w1-diagnostics-and-version-gate.md) |
| R4 | 媒体导入修正 | completed | [W2](2026-09-28-w2-media-import.md) |
| R5 | 发布事务原子化 | completed | [W3](2026-09-28-w3-publication-transaction.md) |
| R6 | 渲染收敛 + 配置语义 | completed（SPEC-19b 登记 BLOCKED） | [W4](2026-09-28-w4-render-convergence.md) |
| R7 | 宿主与分发门禁补齐 | completed（3 项登记，1 项上报） | [W5](2026-09-28-w5-host-and-distribution-gates.md) |
| R8 | 代码健康度 | completed（4 类书面豁免） | [W6](2026-09-28-w6-code-health.md) |

## 3. Standards 轴：13 项逐条处置

| ID | 严重度 | 处置 | 归属 |
| --- | --- | --- | --- |
| STD-01 | 高 | **已修正**：改用契约码 `cxc.candidate.budget_exceeded`；`packed.field.wire_range` 独立成码 | R2 |
| STD-02 | 高 | **已修正**：补齐 `examples/reference_host/README.md` | R1 |
| STD-03 | 高 | **已修正**：状态词与已归档事实对齐（plan/completion/ROADMAP/VERSIONING） | R1 |
| STD-04 | 低（范围外） | **已修正**：两份逐字相同的级联合并为 `playbackStateName` | R8 |
| STD-05 | 中 | **书面豁免**：8 组重复实现中，最大两组是 renderer 中立层与 adapter 的逐字重复，收敛等于重构 Renderer 分层，计划 §4 明确排除 | 残余，见 §4 |
| STD-06 | 中 | **书面豁免**：重复 switch 属结构性重构，计划 §4 排除 | 残余，见 §4 |
| STD-07 | 中 | **已修正 7 项**：6 项死代码删除 + `HostContent::providerRootId`（声明后从未被读，`readBlob` 反而硬编码 `"main"`）。第 8 项 `--event` 判为**有真实角色**（把事件类型带进审计记录，三条作业路径各传不同值），已在 W1 以 `choices` + help 明确"仅记录"，非死代码 | R8 / W1 |
| STD-08 | 低 | **书面豁免**：纯转发层剥离属重构，计划 §4 排除 | 残余，见 §4 |
| STD-09 | 中 | **书面豁免**：类型强化是**公共 API 变更**，计划 §2 声明本计划不新增公共类型 | 残余，见 §4 |
| STD-10 | 中 | **已修正**：`_wfopen_s` 改按 `_MSC_VER`（补齐 `0753e6a` 只修一半的同类缺陷） | R8 |
| STD-11 | 低 | **已修正**：`.gitattributes` 覆盖 `tests/fixtures/stage6_e/**` 与 `stage6_a2/**` | W2 |
| STD-12 | 低 | **已修正**：`ENV{PATH}` 恢复不可再被 `FATAL_ERROR` 跳过；分发脚本 GLOB 行为已记录 | W5 / R8 |
| STD-13 | 低（范围外） | **已修正**：两个 `.obj` 去跟踪 + `*.obj` 规则 | R8 |

## 4. 未关闭项：归属与恢复条件

本计划**没有**把任何未处置项静默丢弃。以下每项都有明确归属与可执行的恢复条件。

| # | 项 | 为什么未在本 PR 处置 | 归属与恢复条件 |
| --- | --- | --- | --- |
| 1 | **ADR 0042 `:350-351` 与实现的冲突** | ADR 承诺宿主支持 `open/play/pause/seek/reload/quit`，实际参考宿主只有 argv 解析、**零 stdin**（全仓 `git grep stdin` 只命中 `README.md`）。ADR 正文冻结，本计划 §1 禁止修改 | **需 owner 裁定**：(a) 订正 ADR `:350-351` 到脚本式宿主口径，或 (b) 在 Stage 8 授权 play/pause/quit 并相应放宽 A2 契约。在此之前 A2 表征冻结在**当前**脚本式面上（`tools/check_stage6_a2.py:294-377`） |
| 2 | SPEC-04（阶段验收号） | 需跨阶段编号口径裁定 | BLOCKED，恢复条件见计划 §9 |
| 3 | SPEC-19b（`CUEXIS_ENABLE_CHART_V5_CANDIDATE`） | 该开关在所有预设与 CI 中为 `OFF`，**无法注册**对应门禁，属构建隔离证据缺口 | BLOCKED；恢复条件：在至少一个预设中开启该选项并注册门禁 |
| 4 | STD-05/06/08（重复实现、重复 switch、中间人） | 属重构，计划 §4/§5 排除把 smell 当独立重构立项 | 残余；最大两组（renderer 中立层 vs adapter）随 **R6 的渲染收敛项移交 Stage 8** |
| 5 | STD-09（原始沉迷） | 类型强化变更公共 API，计划 §2 排除 | 残余；登记为后续设计项，需独立 ADR |
| 6 | 分发门禁在 Linux 未注册 | `CMakePresets.json:98-102` 的 `headless-*` 系列设 `CUEXIS_BUILD_PLAYER: "OFF"`，Linux 门禁不构建 Player。**门禁本身没有 Windows 锁定**（受 `if(CUEXIS_BUILD_PLAYER)` 保护） | 登记为 **Stage 8 输入**；恢复条件：在 Linux 启用 `CUEXIS_BUILD_PLAYER` |
| 7 | static 无 toolchain 拒绝负例 | static 安装包的 `CuexisConfig.cmake` 本身**不含**兼容性检查块（在 `if(CUEXIS_LIBRARY_TYPE STREQUAL "SHARED")` 内），故不存在可拒绝的面 | 已按口径登记（不造"永远空转"的负例）；static 的拒绝面由**与 flavor 无关**的 SDK minor 负例承担，且该负例在 static 下真实运行 |

## 5. 诚实性登记（本计划最重要的部分）

### 5.1 未把未运行的检查写成通过

- **R7 步骤 5 澄清**：复核称"candidate 隔离靠人工"。实查后区分：**安装树扫描**此前确实无门禁
  （现已成门禁）；但**工厂断言**`"Candidate factories stay disabled and do not read their inputs"`
  一直是已注册用例（`tests/playback/playback_candidate_tests.cpp:147-167`）。二者不可混为一谈。
- **R7 步骤 4**：交互命令循环的 ADR 冲突**未处置**，以"上报"而非"修复"登记（见 §4 第 1 项）。
- **R7 步骤 3**：static toolchain 负例**不造**，理由是会造成永久空转（见 §4 第 7 项）。
- **R8 STD-05/06/08/09**：**未修正**，以书面豁免登记，不写成已完成。

### 5.2 本机两个测试失败的定因（不掩盖、不误报）

本机全量 `ctest` 结束时有两个失败，均已定因，并**证明与本 PR 无关**：

1. **`cuexis_contract_version_gate`**：失败用例断言 workflow 抽取出的 shell 块可执行。
   根因：本机 `C:\Windows\system32\bash.exe` 是 **WSL 启动器**，不向子进程传递 Windows 环境变量，
   导致 `CUEXIS_BASE_SHA` 在脚本内为空。把原生 bash（`C:\msys64\usr\bin`）前置后 **19/19 全绿**。
2. **`cuexis_player_distribution`**：`VerifyPlayerDistribution.cmake:217` 报"packaged Player failed
   without a stable diagnostic"。根因：本机 `out/build/debug/bin/` 不含任何 DLL（本地 `.buildenv.ps1`
   把 vcpkg DLL 放在 `PATH` 而非 `bin/`），门禁**故意净化 `PATH`** 后 `spdlogd.dll`/`fmtd.dll`
   解析失败，进程以 `0xC0000135` 退出。

**这两项在未修改的 baseline 上同样复现**（已用 stash + 重建验证），且 hosted 三平台在
`05f9a20` 上全部 `success`。因此它们是**本机沙箱既有偏差，不是本 PR 的回归**；
本 PR 不对它们做任何"通过"声明。

### 5.3 修正中发现的、复核清单本身的不精确

- STD-07 列 8 项。实查后 **7 项成立并已删除**（含清单未强调的 `HostContent::providerRootId` 从未被读、
  而 `readBlob` 硬编码 `"main"`——复核此条完全准确）；第 8 项 `--event` 经查**并非死代码**：
  它把 `pull_request`/`merge_group`/`push`/`workflow_dispatch` 四种事件名带进审计记录，
  由三条作业路径各传不同值，已由 W1 以 `choices` + help 明确"仅记录；事件分流由 workflow job 条件承担"。
  按计划 §R3 步骤 1 的措辞（"若不能定义规则则删除"，反之为可保留的记录角色），保留并登记。
- STD-04 的落地需要**构建才能暴露的接线**：`tests/player/` 下**两个**测试目标都直接编译
  `frame_diagnostics.cpp`，新符号必须同时接入两者（首建 `LNK2019`）。复核清单未指出。
- STD-02 的原文表述有一处不精确，已在 R1 报告 §3 据实修正并保留原句。

## 6. 门禁证据

| 门禁 | 结果 |
| --- | --- |
| `python -B tools/check_docs.py` | `Documentation checks passed: 273 Markdown files and 20 candidate JSON/CXT files validated.` |
| `python -B tools/check_version_gate_tests.py`（原生 bash） | `Ran 19 tests ... OK` |
| `python -B tools/update_version.py --check` | `Cuexis version is consistent: 26.09.28-1` |
| `cmake --build --preset debug` | 0 错误 |
| `cmake --build --preset debug --target cuexis_format_check` | 通过 |
| `ctest --preset debug -R cuexis_reference_host_staging`（static + shared） | 均 `100% tests passed` |
| `ctest --preset debug -j1 --no-tests=error`（全量） | `744/746` 通过；2 项为本机既有偏差，见 §5.2 |
| `git diff --check` | 干净 |
| **hosted（同 SHA `05f9a20`）** | Version Gate ✅、Linux Quality ✅、Windows MSVC ✅、Windows MinGW ✅ |

## 7. 复核索引

- 两轴复核记录：[汇总](2026-09-28-summary.md)｜[Standards](2026-09-28-standards.md)｜
  [Spec](2026-09-28-spec.md)｜[切片附录](2026-09-28-slice-appendix.md)
- 批次退出报告：[R1](2026-09-28-r1-document-and-evidence.md)｜[W1](2026-09-28-w1-diagnostics-and-version-gate.md)｜
  [W2](2026-09-28-w2-media-import.md)｜[W3](2026-09-28-w3-publication-transaction.md)｜
  [W4](2026-09-28-w4-render-convergence.md)｜[W5](2026-09-28-w5-host-and-distribution-gates.md)｜
  [W6](2026-09-28-w6-code-health.md)

# Stage 6 复核修正：R8 代码健康度

状态：completed
快照日期：2026-09-28
更新日期：2026-09-28

本报告是 [Stage 6 复核修正计划](../../../stage_plans/active/stage-06-review-remediation/plan.md)
批次 **W6（R8）** 的退出记录。它逐条处置
[Standards 轴](2026-09-28-standards.md) 的 STD-04、STD-07、STD-10、STD-12、STD-13，
并登记 STD-05/06/08/09（重复、开关、中间人、原始沉迷）的**有界处置**边界。

**修正原则**：不改写任何历史报告的现象与证据原文；每条删除都先**独立核实无调用者**，
不采信复核清单的结论；**未运行/未注册的检查不写成通过**。

## 1. 批次边界

- 起始 SHA：`e8be5fc`（W5 批次 HEAD）。
- 涉及路径：`app/player/`、`tests/player/CMakeLists.txt`、`engine/chart/include/cuexis/chart/`、
  `engine/cxc/src/`、`examples/reference_host/`、`tools/check_version_gate.py`、
  `tools/asset_publish/src/publish_fs_internal.cpp`、`.github/workflows/version-gate.yml`、`.gitignore`。
- 未触碰：ADR 0042 冻结决策正文、Stage 7A/8/9–12 任何交付物、提案 1、
  `engine/render_opengl/`、`engine/presentation_renderer/`、`tools/media_import/`（STD-05/08 的载体）。
- **未改任何公共 API、未改 SDK API 版本（仍为 `0.7.0`）**、未改任何 golden。

## 2. 逐条处置

| 复核 ID | 严重度 | 现象（原文摘要） | 本批次修正 | 证据 |
| --- | --- | --- | --- | --- |
| STD-13 | 低（范围外） | `hello.obj`（46,882 B）与 `dump_chart_writer.obj`（589,044 B）被 git 跟踪，`.gitignore` 无 `*.obj` | `git rm --cached` 两者（保留在工作区为未跟踪）；`.gitignore` 追加 `*.obj` 规则 | `.gitignore`；`git ls-files \| Select-String '\.obj$'` 修正前后 |
| STD-07 | 低 | 8 项死代码 | 7 项删除，逐项先核实无调用者（见 §2.1）；第 8 项 `--event` 判为有真实角色，保留并说明 | 各源文件；§3 反证 |
| STD-04 | 低（范围外） | `audioStateName` 与 `stateName` 是逐字相同的 `PlaybackState` 级联 | 合并为单一 `playbackStateName`（`app/player/src/player_state_name.{hpp,cpp}`），两个调用点改用之 | 新文件；`player_app.cpp`、`frame_diagnostics.cpp` |
| STD-10 | 低 | `publish_fs_internal.cpp:198` 按 `_WIN32` 选 `_wfopen_s`，但同文件 `:42` 已按 `_MSC_VER` 选 `_dupenv_s` | 统一按 `_MSC_VER` 分支（`_wfopen_s` 是 MSVC CRT 扩展，MinGW 无此符号） | `tools/asset_publish/src/publish_fs_internal.cpp` |
| STD-12（第 3 条） | 低 | workflow 的 `run: \|` 块内缩进不齐 | 三处 materialize 块对齐到 10/12 空格 | `.github/workflows/version-gate.yml` |

### 2.1 STD-07 的六项，与两项需要真实调查的判断

| # | 项 | 位置 | 核实方式与结论 |
| --- | --- | --- | --- |
| 1 | `PlayerController::timingOffsetMs()` | `player_control.cpp:254` / `.hpp:104` | `git grep` 仅命中私有成员 `timingOffsetMs_`（仍在用）与**无关类型** `RuntimeTimeline`/`ChartClock` 的同名方法；访问器零调用者，删除 |
| 2 | `HostReport::steps()` | `host_report.cpp:45` | 仅命中声明/定义；`steps_` 仍被 `event()`/`summary()` 使用，故 `host.summary ... steps=` 输出不变 |
| 3 | `HostFileProvider::readCount()` | `host_content.cpp:90` | **需调查**：复核提示"可能被门禁期望使用"。实查 `VerifyReferenceHost.cmake` 只断言字面量 `host.summary outcome=ok`，从不断言 `readCount`；`readCount_` 仍用于 `revision=` 与 `provider.read index=N`。删除后暂存门禁通过 |
| 4 | `CandidateRuntimeMetadata::flags/candidateRevision` | `candidate_lowering.hpp:40-41` | 全仓零读取者。**注意同名陷阱**：仍然存活的是**另一个类型** `packed::PackedChartProfile`（`packed_chart_tables.hpp:20`，由 `packed_chart_tables.cpp:887` 校验），未触碰 |
| 5 | `_result_json` 的重复赋值 | `check_version_gate.py:275-279` | `base_version`/`candidate_version` 是 `GateResult` 的**声明字段**（`:75-76`），`asdict` 本就会输出；两行赋值是 no-op，删除 |
| 6 | `readRequiredString` 的第三参数 | `cxc_candidate.cpp:55` | **需调查**：判为**真正多余**而非"诊断被静默丢弃"。依据：首参 `json::Reader` 自身持有 `Diagnostics&`，`requiredField` 会自行上报 `json.field.missing`，传参冗余。删除签名参数与 10 处调用点后 `cuexis_cxc_tests` 全绿 |
| 7 | `HostContent::providerRootId{"main"}` | `host_content.hpp:44` | **补删**：声明后全仓**零读取**（`git grep` 仅命中该声明行），而 `readBlob` 直接硬编码 `request.rootId != "main"` 与 `.rootId = "main"`。删除后全量构建 0 错误，`cuexis_reference_host_staging` 仍通过 |

**第 8 项 `--event` 判为保留**（非死代码）：它把事件名带进审计记录，
`version-gate.yml` 三条作业路径各传不同值（`pull_request`/`merge_group`、`push`、`workflow_dispatch`），
且已由 W1 以 `choices` + help 明确"仅记录"。计划 §R3 步骤 1 的措辞是"若不能定义规则则删除"，
反之为可保留的记录角色。

**未触碰（按计划第 7 条与复核自身说明保留）**：`NullJudgeSystem`（对应尚未存在的 Stage 7A Judgement）、
`--event`（复核明确记为"仅记录"，属有意设计）。

### 2.2 STD-04 的落地细节

两个级联**逐字相同**（已用 `git show HEAD:...` 并排比对确认），因此合并严格保持行为不变。
新实现的 switch 体逐字照搬。初次构建出现 `LNK2019`：`tests/player/` 下**两个**测试目标都直接编译
`frame_diagnostics.cpp`，故 `player_state_name.cpp` 需同时接入两个目标；`player_app.cpp` 不被任何
测试目标编译（已 grep 确认）。这是复核清单未指出、由构建暴露的真实接线要求。

## 3. 反证记录（每条删除必须能被真实推翻）

| # | 断言 | 变异/核实方式 | 结果 |
| --- | --- | --- | --- |
| A | `*.obj` 规则不会误伤正当文件 | 修正前后 `git ls-files \| Select-String '\.obj$'` | 修正前**恰好**只有那两个文件，修正后为空——无其他被跟踪 `*.obj` 会落入新规则 |
| B | 删除项确实无调用者 | 每项删除后 `git grep <符号>` | 无残留调用点（同名不同型/不同成员的情况已在 §2.1 逐项区分） |
| C | `readRequiredString` 的参数是冗余而非丢弃诊断 | 查 `json::Reader::requiredField` 的上报路径 | `requiredField` 自行上报 `json.field.missing`；且 `cuexis_cxc_tests` 62 用例 772 断言全绿 |
| D | STD-10 的 `_wfopen_s` 改动行为不变 | 构建 `cuexis_asset_publish` + 直接运行测试二进制 | 构建 0 错误；`All tests passed (332 assertions in 23 test cases)` |
| E | STD-12 缩进改动是纯空白 | `git diff -w` | **输出为空**，证明仅空白变化；且 workflow 的 bootstrap 块被**逐字抽取**执行的门禁用例仍 19/19 通过 |
| F | `providerRootId` 确实零读取 | 删除后 `git grep providerRootId` + 全量构建 + 暂存门禁 | 仅剩历史复核报告中的提及（不得改写）；构建 0 错误；`cuexis_reference_host_staging` 通过 |

**方法论记录**：本例最值得记的两点——
（i）**同名不同型的陷阱**：`candidateRevision` 同时存在于一个已死类型与一个存活类型上，
按名字 grep 会得到"仍在使用"的错误结论；必须按**限定名**（`metadata.candidateRevision`）核实。
（ii）**删除的连带接线**：删除一个 `static` 自由函数所在的 `.cpp` 中内容后，若有多个测试目标
直接编译该 `.cpp`，新符号必须**同时**接入所有这些目标——只有构建才能暴露，静态阅读不会。

## 4. 门禁与命令输出

| 命令 | 结果 |
| --- | --- |
| `cmake --build --preset debug` | 全量构建 0 错误 |
| `cmake --build --preset debug --target cuexis_format_check` | 通过（首次在 `cxc_candidate.cpp` 命中真实格式违规，修正后通过） |
| `ctest --preset debug -j1 -R "cxc\|player_diagnostics\|player_control\|playback\|candidate" --no-tests=error` | `100% tests passed, 0 tests failed out of 30` |
| `ctest --preset debug -R cuexis_reference_host_staging` | `Passed`（1/1） |
| `cuexis_asset_publish_tests.exe` | `All tests passed (332 assertions in 23 test cases)` |
| `cuexis_cxc_tests` | `All tests passed (772 assertions in 62 test cases)` |
| `python -B tools/check_version_gate_tests.py`（原生 bash） | `Ran 19 tests ... OK` |
| `python -B tools/check_docs.py` | `Documentation checks passed: 272 Markdown files and 20 candidate JSON/CXT files validated.` |
| `ctest --preset debug -j1 --no-tests=error`（全量） | `99% tests passed, 2 tests failed out of 748`，失败集 = {#19, #747}，见 §4.1；另 1 项 symlink 用例在本机跳过 |

### 4.1 本机全量 `ctest` 的两个失败：均为本机既有，非本批次回归

本机 `ctest --preset debug -j1 --no-tests=error` 结束时有两个失败：
`cuexis_player_distribution`（#19）与 `cuexis_contract_version_gate`（#747）。

**已按"不得把未通过写成通过"的要求逐一定因，并证明其与本批次无关**：

1. **#747 `cuexis_contract_version_gate`**：失败用例是
   `test_bootstrap_shell_fails_when_the_trusted_tree_lacks_the_gate`，报
   `version.bootstrap.required: trusted baseline  lacks`。根因是本机
   `C:\Windows\system32\bash.exe` 是 **WSL 启动器**，不向子进程传递 Windows 环境变量
   （`wsl.exe` 同样存在于本机），因此 `CUEXIS_BASE_SHA`/`TRUSTED_ROOT` 在脚本内为空。
   把原生 bash（`C:\msys64\usr\bin`）前置到 PATH 后**19/19 全绿**。
   **同一失败在未修改文件上即可复现**（直接运行 `check_version_gate_tests.py` 于干净 HEAD）。
2. **#19 `cuexis_player_distribution`**：`VerifyPlayerDistribution.cmake:217`
   报"packaged Player failed without a stable diagnostic"。根因是本机
   `out/build/debug/bin/` **不含任何 DLL**（本机 `.buildenv.ps1` 把 vcpkg DLL 放在 `PATH` 上，
   而非复制进 `bin/`），门禁**故意净化 `PATH`** 后运行时 `spdlogd.dll`/`fmtd.dll` 解析失败，
   进程以 `0xC0000135`(DLL not found) 退出。把 vcpkg 的 `debug\bin` 放回 PATH 后
   Player 立即给出正常的 `player.chart.open_failed` 诊断。

**关键旁证**：`05f9a20` 的 **hosted Windows MSVC 与 Windows MinGW run 均为 `success`**，
即 `cuexis_player_distribution` 在真实环境中通过。故这两项是本机沙箱环境的既有偏差，
**不是本批次引入的回归**；本批次因此不对它们做任何"通过"声明。

## 5. 残余与未核对

- **STD-05/06/08/09（重复实现、重复 switch、中间人、原始沉迷）未处置**。理由：计划 §5 明确
  把复核的 smell 判断项限制在 **R8 的有界改动**内，且 §4 禁止把 smell 当独立重构立项。
  其中规模最大的 STD-05 第 1、2 组是 `render_opengl` 与 `presentation_renderer` 的中立层与 adapter
  逐字重复，收敛它等于**重构 Renderer 分层本身**，属计划 §4 明确排除项。这些项**登记为残余**，
  其归属见计划 §10（R6 的渲染收敛项移交 Stage 8），不写成已完成。
- **STD-09 的类型强化**（把四类身份从 `std::string` 改为强类型、`void* handle_` 改为明确的拥有类型）
  是**公共 API 变更**：计划 §2 声明本计划"不新增公共头、不新增公共类型"，故不在本批次范围。
  登记为后续设计项。
- **STD-11 已在 W2 批次处置**（`.gitattributes` 覆盖新 fixture 树），不在本批次范围。
- **hosted 三平台复验**：本批次改动触及 `app/player`（新增源文件、两个测试目标接线）、
  `engine/cxc`、`engine/chart`、`.github/workflows`。**必须在同 SHA 的三平台门禁上复验**，
  本报告 §4 的本机结论不能替代它。

## 6. 变更文件

- 实现：`app/player/src/player_state_name.{hpp,cpp}`（新增）、`app/player/src/player_app.cpp`、
  `app/player/src/frame_diagnostics.cpp`、`app/player/src/player_control.{hpp,cpp}`、
  `engine/chart/include/cuexis/chart/candidate_lowering.hpp`、`engine/cxc/src/cxc_candidate.cpp`、
  `examples/reference_host/src/host_content.{hpp,cpp}`、`examples/reference_host/src/host_report.{hpp,cpp}`、
  `tools/check_version_gate.py`、`tools/asset_publish/src/publish_fs_internal.cpp`
- 构建：`app/player/CMakeLists.txt`、`tests/player/CMakeLists.txt`
- 仓库：`.gitignore`（新增 `*.obj`）、移除跟踪 `hello.obj`、`dump_chart_writer.obj`
- CI：`.github/workflows/version-gate.yml`（仅缩进）
- 文档：本报告、`examples/reference_host/README.md`（`readCount()` 删除后的证据描述订正）、
  `docs/guides/BUILDING.md`（R7 门禁记录）、`docs/stage_reports/README.md`（索引）

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

## 复核更正（独立审计，2026-09-28）

本次复核逐条核对了 R8 的删除与改动。以下为**已确认的实质性声明**与**必须更正的表述**。
本节为**追加**：上文历史观察与证据原文未被改写。

### 1. 已确认的声明（逐条实查）

- **STD-13**：`git ls-files | Select-String "\.obj$"` 输出为空（count=0）；`.gitignore:14` 为 `*.obj`；两个文件仍保留在工作区且为未跟踪（`hello.obj` 46,882 B、`dump_chart_writer.obj` 589,044 B，与应用新规则后的忽略状态一致）。
- **STD-07 七项删除全部属实**：
  1. `PlayerController::timingOffsetMs()`：`e8be5fc:app/player/src/player_control.hpp` 存在该访问器声明，HEAD 已无；仅存私有成员 `player_control.hpp:156`（`double timingOffsetMs_{};`）。
  2. `HostReport::steps()`：`git grep -n steps -- examples/reference_host` 仅剩 `host_report.cpp:11,37` 与 `host_report.hpp:35`（`steps_` 仍在用，`host.summary ... steps=` 输出不变）。
  3. `HostFileProvider::readCount()`：仅剩 `host_content.cpp:91,94,119,122` 与 `host_content.hpp:37`（`readCount_` 仍在用）。
  4. `CandidateRuntimeMetadata::flags/candidateRevision`：HEAD 的 `engine/chart/include/cuexis/chart/candidate_lowering.hpp` 中该结构体只剩 `compilerProfile`/`semanticIdentity`/`resourceClosure`/`objects`。**同名不同型**的存活类型 `packed::PackedChartProfile` 被正确保留：`packed_chart_tables.hpp:19-22` 仍有 `candidateRevision{1};` 与 `flags{1};`，并由 `packed_chart_io.hpp:34,41`、`packed_chart_io.cpp:221`、`packed_chart_tables.cpp:885` 使用。
  5. `_result_json`：`tools/check_version_gate.py:334-335` 现仅剩 `return json.dumps(asdict(result), sort_keys=True)`，两行同值赋值已无；`base_version`/`candidate_version` 仍是 `GateResult` 声明字段（`:75-76`）。
  6. `readRequiredString`：`engine/cxc/src/cxc_candidate.cpp:55` 已是两参数；`json::Reader::requiredField` 自身上报 `json.field.missing`（`engine/json_support/src/reader.cpp:57-73`）。
  7. `HostContent::providerRootId`：HEAD 的 `examples/reference_host/src/host_content.hpp` 中 `struct HostContent` 只有 `projectDirectory` 与 `chartEntryPath`。
- **第 8 项 `--event` 确为保留且有真实角色**：`tools/check_version_gate.py:354-360` 定义该选项（含 `choices` 与「recorded only」help）、`:399` 在通过消息中输出 `event={args.event}(recorded)`；`.github/workflows/version-gate.yml:77`、`:120`、`:163` 三条作业路径分别传值。
- **STD-04**：新增 `app/player/src/player_state_name.{hpp,cpp}`，单一 `playbackStateName`（6 分支 switch）。`e8be5fc` 的 `frame_diagnostics.cpp:19-33`（`stateName`）与 `player_app.cpp:27-42`（`audioStateName`）逐字相同，二者在 HEAD 均已不存在；`git grep -n playbackStateName` 命中 `frame_diagnostics.cpp:140`、`player_app.cpp:197`、`player_state_name.cpp:5`。接线：`app/player/CMakeLists.txt:20-21`、`tests/player/CMakeLists.txt:5` 与 `:35`；全仓只有这两个测试目标直接编译 `frame_diagnostics.cpp`，二者均已接入 `player_state_name.cpp`。
- **STD-10**：`tools/asset_publish/src/publish_fs_internal.cpp:202` 为 `#if defined(_MSC_VER)`；残留的两处 `_WIN32`（`:19`、`:63`）用于 `<Windows.h>`/`CreateFileW`/unistd，与 CRT 扩展选择无关，正确。
- **STD-12**：`git diff -w e8be5fc..HEAD -- .github/workflows/version-gate.yml` 输出为空（非 `-w` 统计为 21 insertions / 21 deletions），证明仅空白变化；以原生 bash（`C:\msys64\usr\bin` 前置到 PATH）复跑 `python -B tools/check_version_gate_tests.py` 得 `Ran 19 tests in 9.055s` 与 `OK`，bootstrap 块抽取仍可执行。

本次实际运行的门禁与用例（本机 MSVC）：

| 命令 | 逐字结果 |
| --- | --- |
| `cuexis_cxc_tests.exe` | `All tests passed (772 assertions in 62 test cases)` |
| `cuexis_asset_publish_tests.exe`（二进制位于 `out\build\debug-media-tools\bin\`，**不在** `out\build\debug\bin\`） | `All tests passed (332 assertions in 23 test cases)` |
| `cuexis_player_control_tests.exe` | `All tests passed (479 assertions in 26 test cases)` |
| `cuexis_player_diagnostics_tests.exe` | `All tests passed (78 assertions in 4 test cases)` |
| `python -B tools/check_docs.py` | `Documentation checks passed: 274 Markdown files and 20 candidate JSON/CXT files validated.` |
| `ctest --preset debug -R cuexis_reference_host_staging -V` | `100% tests passed, 0 tests failed out of 1` |

### 2. 必须更正：STD-05 残余登记部分失效

R8 §5 把「STD-05 第 1、2 组是 `render_opengl` 与 `presentation_renderer` 的中立层与 adapter 逐字重复」列为规模最大的残余未修项。**该表述在 R8 修订版上已不成立**：这两组重复已由 `dcc4232`（R6 render convergence，2026-09-28 06:07）删除，而 `dcc4232` 是 R8 各提交（`e8be5fc`、`3f9551b`）的祖先 —— R8 开工时它们已不存在，因此 R8 不可能「留下未修」。

证据：`engine/render_opengl/include/cuexis/render_opengl/open_gl_backend.hpp:62-64` 现直接别名中立层类型（`using OpenGlDrawCommand = presentation_renderer::DrawCommand;` 等）；`git grep -n "SummaryHash\|summaryDigest\|hashCommand\|transformPoint\|finiteMatrix" -- engine` 在 `render_opengl` 下**零命中**，实现只剩 `engine/presentation_renderer/src/draw_command.cpp`（域串 `cuexis.validation.summary.v1` 见 `:61`）与 `engine/playback/src/presentation_extraction.cpp`。

**应把第 1、2 组从残余清单中划掉。仍然成立的 STD-05 组（本次至少实查以下两组）**：

- 版本门禁工作流三处近乎逐字的 trusted-baseline materialize 块：`.github/workflows/version-gate.yml:48-67`、`:95-114`、`:139-157`。
- 错误工厂重复：`engine/chart/src/packed_chart_tables.cpp`、`engine/chart/src/packed_profile.cpp`、`engine/chart/src/packed_semantic_identity.cpp` 各有一份 `auto fail(`。

（STD-06 重复开关：`engine/player_support/src/player_command.cpp:74-128` 仍在 Play/Pause/Stop/Seek/Reload 各重复 `Empty`/`Failed` 前置级联；STD-08 中间人：`tools/cxc_common/src/cxc_candidate.cpp:7-9` 仍是纯转发；STD-09 原始沉迷：如 `app/player/src/player_control.hpp:156` 的裸 `double`。此三项的残余登记属实。）

### 3. 两处数字更正

- R8 §2.1 第 6 项写「删除签名参数与 **10 处调用点**」：`git grep -n readRequiredString -- engine/cxc/src/cxc_candidate.cpp` 的匹配行数为 10，其中 1 行是 `:55` 的定义，**实际调用点 9 处**。
- R8 §2 STD-12 写「三处 materialize 块对齐到 10/12 空格」：`for path in \` 三行（`.github/workflows/version-gate.yml:54`、`:100`、`:143`）仍为 **11** 空格，且在该提交 diff 中是未改动的上下文行（块体 12、`done` 10）。空白-only 的性质仍由 `git diff -w` 为空证实，但「10/12」措辞不准确。

### 4. 测试数量漂移

R8 §4.1 记录全量 `ctest --preset debug -j1 --no-tests=error` 为 **748** 个测试、失败集 `{#19, #747}`。当前树（HEAD `0c8f837`）本机唯一可查的 `out/build/debug/Testing/Temporary/LastTest.log`（2026-09-28 19:55）显示 **751**（`750/751`、`751/751`）——R7/R8 之后又新增了测试。**748 这一数字现已无法复现**，属漂移，而非虚假结论。

本次唯一复现到的失败是版本门禁在 WSL `bash.exe` shim 下的用例。LastTest.log 逐字为：

```text
AssertionError: 0 != 1 : the extracted bootstrap block is not executable shell: ::error title=Version gate bootstrap required::version.bootstrap.required: trusted baseline  lacks ; owner must review the checker and protection rules before the first bootstrap merge. The workflow intentionally fails and never falls back to candidate code.
Ran 19 tests in 12.785s
FAILED (failures=1)
```

把 `C:\msys64\usr\bin` 前置到 PATH 后为 **19/19 OK**（见 §1）。分发门禁（#19）的本机失败本次未重跑，不作任何通过声明。

### 5. 未核实（本次无法运行）

| 声明 | 状态 | 原因 |
| --- | --- | --- |
| `cmake --build --preset debug` 全量构建 0 错误 | **未核实** | 复核期间被禁止运行 `cmake --build`（构建树被其他代理并发使用），只允许运行既有测试二进制 |
| `cmake --build --preset debug --target cuexis_format_check` 通过 | **未核实** | 同上 |
| 全量 `ctest` 748 个测试及其失败集 | **未核实** | 未重跑全量 `ctest`；且数量已漂移为 751（见 §4） |

### 6. 必须更正：STD-10 的修正方向把 MinGW 构建打红了

R8 §2 的 STD-10 行把方向定为「统一按 `_MSC_VER` 分支」，理由是「`_wfopen_s` 是 MSVC CRT 扩展，MinGW 无此符号」。**这句理由对 `_wfopen_s` 从未被证实，而该修正本身让 `debug-media-tools` 的 Windows MinGW 构建直接编译失败。** 托管证据：`0c8f837` 的 MinGW run（`36417041282`，job `108910546976`）在 `publish_fs_internal.cpp` 报

```text
error: cannot convert 'const std::filesystem::__cxx11::path::value_type*'
       {aka 'const wchar_t*'} to 'const char*'
```

原因是改动后 MinGW 落入 `#else` 的**窄** `std::fopen(file.c_str(), "rb")`，而 MinGW 的 `std::filesystem::path::value_type` 同样是 `wchar_t`：宽路径无法传给窄 `fopen`。那不是回退，是编译错误。

**`readEnvValue` 的 `_MSC_VER` 是对的，不能一起回退。** `0753e6a` 记录过一次真实的 MinGW 链接失败 `undefined reference to '__imp__dupenv_s'`；本次以 mingw-w64 的导入库复核，`libmsvcrt-os.a`（msvcrt 路径）中 `_dupenv_s` **absent**、`_wfopen` **present**，与该失败完全对应。因此两处站点的守卫**本就应当不同**，因为其回退分支的可行性不同：`readEnvValue` 的 `getenv` 回退在 MinGW 上有效，而 `readFileBytes` 的窄 `fopen` 回退在 MinGW 上根本无法编译。STD-10 原文「统一」这一处方本身即为错误——它把「两处写法不一致」当成了缺陷，而实际的一致条件应当是「回退分支各自可用」。

现行修正为三分支：

| 工具链 | 分支条件 | 打开方式 | 路径类型 |
| --- | --- | --- | --- |
| MSVC | `_MSC_VER` | `_wfopen_s` | 宽 |
| MinGW | `_WIN32 && !_MSC_VER` | `_wfopen` | 宽 |
| Linux | 其余 | `std::fopen` | 窄 |

**方法学更正（重要）**：本机 `ucrt64` **不能**代表 CI 使用的 msvcrt 变体，不得作为其代理。本次先用 ucrt64 测得 `_wfopen_s` 与 `_dupenv_s` 均可编译且可链接，据此一度准备回退 `_MSC_VER`；而 `0753e6a` 的真实 CI 证据恰恰相反。原因是 UCRT 变体提供安全 CRT 实现、msvcrt 变体不提供。真正可用的验证是：

- 用 ucrt64 g++ 编译**真实源文件**（会走 `_WIN32 && !_MSC_VER` 分支）→ **通过**；
- 把同一分支换回旧写法编译同一文件（对照）→ 复现出与托管 CI **逐字相同**的 `wchar_t* → const char*` 错误。

即该检验非空转。`_wfopen` 在 msvcrt 上的存在性由 `libmsvcrt-os.a` 佐证，最终判据仍是 MinGW CI 本身。

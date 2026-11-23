# T-03 命令与文件清单

状态：evidence

更新日期：2026-11-23

<!-- cuexis-meta
id: EVIDENCE-stage-07-rr.t-03-commands
status: evidence
authority: none
date: 2026-11-23
-->

## 环境

| 项 | 值 |
| --- | --- |
| 工作目录 | `E:\Cuexis-worktree` |
| 分支 | `stage-7-rr` |
| 派单基线 | `3b8bf41e3b26207c4a59860d40b47b747c7545db` |
| 开读时 `HEAD` | `569253d8cfd6c795559a4daacf15adbbafb30942` |
| 收尾时 `HEAD` | `6c77a888a5ac5e564393dc8c2e60b22fc1656d1f`（并发提交 `03334ecd`、`6c77a888`；`3b8bf41e` 为祖先） |
| 工作树 | 收尾时只剩本卡改动；开读期间其他代理的未提交改动已由其提交 |
| 构建/CTest | 主代理在 MSVC 19.51 + `candidate-debug` 执行并通过（见下）；本代理未运行 |
| 安装/故障注入/ABI | 未执行 |

## 执行的命令

```text
# 文档门禁（改 docs 后；本卡 docs 改完时均 exit 0）
python -B tools/check_docs.py
  exit 0
python -B tools/verification/check_docs_facts.py
  exit 0
# 中途 check_docs.py 曾 exit 1：四份并发文件链接到当时尚未创建的
# docs/stage_reports/stages/stage-07-rr/implementation/2026-11-23-t-03-contracts-and-diagnostics.md
# 该文件随 b46ce396 创建；复核重跑 check_docs.py 与 check_docs_facts.py 均 exit 0

# 基线与改动范围
git rev-parse --abbrev-ref HEAD
git rev-parse HEAD
git merge-base --is-ancestor 3b8bf41e HEAD
git status --porcelain=v1
git diff --stat
git log --oneline -3

# 只读源码/文档检索（PowerShell Select-String / Get-Content）
crc32 / header CRC：engine/chart/src/packed_chart_tables_{writer,inspect,decoder_header}.cpp
mode 0 拒绝：engine/chart/src/packed_chart_tables_decoder_{tables,model}.cpp
Beat 上限：engine/chart/src/canonical_chart_loader_primitives.cpp、limits.hpp
CXC profile：engine/cxc/src/cxc_candidate.cpp、engine/cxc/include/cuexis/cxc/cxc_candidate.hpp
身份构造：engine/judgement/src/execution_kernel_prepare.cpp、gameplay_assembler_assemble.cpp
输入三层：engine/judgement/src/input.cpp、execution_kernel_submit.cpp、execution_profile_tests.cpp
观测桥：app/execution_host/frame_observation.{cpp,hpp}
Player 执行：app/player/src/player_execution.cpp
安装面：engine/playback/CMakeLists.txt、playback_session.hpp、playback_source.hpp、presentation_candidate.hpp
digest：engine/playback/include/cuexis/playback/frame_digest.hpp、src/frame_digest*.cpp
诊断分类：schemas/cuexis.gameplay-diagnostics.v2.codes.json
既有测试约定：tests/{chart,cxc,judgement,rpa} 的相关文件与 CMakeLists.txt
```

上面是本代理执行的文档门禁与只读命令。下列构建/CTest 由主代理执行，本代理未运行：

```text
cmake --build --preset candidate-debug --target cuexis_chart_tests cuexis_cxc_tests cuexis_judgement_tests
  exit 0（三个可执行均链接）
ctest --test-dir out/build/candidate-debug -N -R '^rr2_contract'
  首次枚举：rr2_contract_packed #121、rr2_contract_beat #170、
            rr2_contract_entry #326、rr2_contract_input #488
  复核（b46ce396）：rr2_contract_packed #251、rr2_contract_beat #175、
            rr2_contract_entry #326、rr2_contract_input #452；B3 两条 #580/#614
  编号随 catch_discover_tests 每次重写漂移，不是身份；正文只用名称
ctest --test-dir out/build/candidate-debug --no-tests=error -R '^rr2_contract_packed$'
ctest --test-dir out/build/candidate-debug --no-tests=error -R '^rr2_contract_beat$'
ctest --test-dir out/build/candidate-debug --no-tests=error -R '^rr2_contract_entry$'
ctest --test-dir out/build/candidate-debug --no-tests=error -R '^rr2_contract_input$'
  上一版各 1/1 passed，退出 0；本轮新增 Beat 约分与 input 断言待重跑
cmake --build --preset candidate-debug --target cuexis_rpa_tests
  exit 0
两条 B3 RPA 对照（-R '^…$'）各 1/1 passed，退出 0
```

本代理没有执行 `cmake --preset`、`cmake --build`、`ctest`、安装、故障注入或任何会生成 `out/build`
的命令；`out/build` 的状态归主代理的构建步骤。

## 实读输入

- 任务与计划：`docs/stage_plans/active/stage-07-rr/tasks/T-03.md`、`plan.md`、`findings.md`
  §15/§22/§23/§24、`architecture-design.md` D-18/D-24/D-25、`tasks/T-06.md`。
- 合同：`docs/formats/{PACKED_CHART_FORMAT,CHART_FORMAT,CXC_FORMAT,CHART_ENTRY_V1_FORMAT,CXT_V2_FORMAT,GAMEPLAY_V2_SPEC,gameplay-v2-execution-profile}.md`、`docs/api/{frames-digests-and-timelines,realtime-host-boundary,gameplay-v2-execution-types}.md`、`docs/guides/LIMITS.md`、`docs/templates/report.md`。
- 代码/CMake：`engine/chart`（limits、packed tables、canonical loader）、`engine/cxc`（candidate、hash）、
  `engine/judgement`（input、submit、prepare、assembler identity、session）、`engine/playback`
  （session/prepared/frame digest/CMake）、`app/execution_host/frame_observation.*`、
  `app/player/src/player_execution.cpp`、`schemas/cuexis.gameplay-diagnostics.v2.codes.json`。
- 测试：`tests/chart/{packed_chart_io_tests,chart_loader_tests,packed_fixture_support.*}`、
  `tests/cxc/{cxc_candidate_bridge_tests,cxc_test_support.*}`、
  `tests/judgement/{execution_test_fixture,fold_recovery_fixture,fold_recovery_seek_tests,execution_profile_tests,execution_kernel_oracle_tests,observation_normalization_tests}.*`、
  `tests/rpa/{rpa_b3_schedule_tests,rpa_b3_checkpoint_tests}.cpp` 与三份 CMakeLists。

## 本代理改动的文件

```text
docs/formats/PACKED_CHART_FORMAT.md
docs/formats/CHART_FORMAT.md
docs/formats/CXT_V2_FORMAT.md
docs/formats/GAMEPLAY_V2_SPEC.md
docs/formats/gameplay-v2-execution-profile.md
docs/api/gameplay-v2-execution-types.md
docs/api/frames-digests-and-timelines.md
docs/api/realtime-host-boundary.md
tests/chart/packed_chart_io_tests.cpp
tests/chart/chart_loader_tests.cpp
tests/cxc/cxc_candidate_bridge_tests.cpp
tests/judgement/execution_kernel_oracle_tests.cpp
out/review/stage-07-rr/T-03/report.md
out/review/stage-07-rr/T-03/finding-disposition.md
out/review/stage-07-rr/T-03/candidate-install-and-macro-diff.md
out/review/stage-07-rr/T-03/rr-f132-identity-binding.md
out/review/stage-07-rr/T-03/commands-and-files.md
```

`tests/{chart,cxc,judgement}/CMakeLists.txt` 未改：四个用例追加在既有已注册源文件内，由既有
`catch_discover_tests` 自动发现；主代理的 `ctest -N` 已确认 Test #121/#170/#326/#488。
`tests/rpa/CMakeLists.txt` 未改：两个 RPA 用例已存在并已注册，主代理实测各 1/1 通过。

## 未改但相关的文件

- `engine/**`、`app/**` 的生产源码、安装头、`cmake/` 与根 CMake。
- `docs/CURRENT_STATUS.md`、`docs/HANDOFF.md`、`docs/MAP.md`、`AGENTS.md`、
  `docs/stage_plans/**`（含 `findings.md`）、`docs/stage_reports/**`。
- `docs/formats/{CXC_FORMAT,CHART_ENTRY_V1_FORMAT}.md`：复核后未发现需订正差异。

## `out/review/stage-07-rr/T-03/` 的既有文件

目录下另有 `configure.cmd`、`configure.log`、`build-tests.cmd`、`build-tests.log`（时间为
2026-11-23 19:58–20:04）。它们不是本代理执行或生成的命令；本轮构建/CTest 结果以主代理回执为准，
没有用这些日志替代回执，也未删除文件。本代理的结论来自源码/合同/测试源的静态对照、文档门禁，以及
主代理回执的四用例与两条 RPA 对照结果。

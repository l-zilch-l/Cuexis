# Tools

状态：current

更新日期：2026-11-17

本目录只放脚本与工具源码。每个脚本必须属于下面三类之一；新增门禁时在对应分类登记一行。

## 1. 活门禁（CI / CTest / pre-push 使用）

只允许这一类脚本被 CI 或 CMake 调用。改动它们必须同时跑对应门禁。

| 脚本 / target | 调用方 |
| --- | --- |
| `tools/check_docs.py` | 包装 `verification/check_docs.py`；预推送 + CI `documentation-contracts` |
| `tools/verification/check_docs.py` | 文档总门禁（链接/H1/可达性/状态合同） |
| `tools/verification/check_findings.py` | 文档总门禁调用：发现定义/引用、字段、处置闭环和可信 Git 原文保护；可用 `--base-ref` 指定同一验收基线 |
| `tools/verification/check_docs_governance.py` | 文档总门禁调用：命名、布局、元数据、唯一主题和重复疑点；遗留例外在 `docs/governance_rules.json` 逐路径登记 |
| `tools/verification/check_source_naming.py` / `tools/verification/test_check_source_naming.py` | 文档总门禁调用：生产源码、安装头与构建系统文件名的阶段/任务编号检查；遗留路径按 `source_naming_exceptions` 逐路径豁免；自测进预推送 Quick 与 CI |
| `tools/verification/test_check_findings.py` / `tools/verification/test_check_docs_governance.py` / `tools/verification/test_check_docs_facts.py` | 预推送 Quick 与 CI 的治理、登记册及事实正反回归 |
| `tools/verification/check_plan_schedule.py` | 文档总门禁调用的 Stage 7-RR 调度检查；七段标题只认行首 0–3 个空格后的正式 H2，忽略围栏、注释和缩进代码中的伪标题；分列 active/candidate/completed，仅 active 列入当前派单，completed 可作 active 前置，active/completed 均不得依赖 candidate；独立运行 `python -B tools/verification/check_plan_schedule.py [--stage-dir PATH]` |
| `tools/verification/check_docs_facts.py` | 预推送与 CI：生成物新鲜度及代码派生事实；当前版本漂移失败，历史声明按局部语境解释 |
| `tools/verification/gen_docs_map.py` / `tools/verification/gen_docs_limits.py` / `tools/verification/gen_docs_codes.py` / `tools/verification/gen_player_options.py` | 生成对应文档，由 `check_docs_facts.py` 导入校验；地图生成器另支持 `--check` 独立复验 |
| `tools/check_version_gate.py` / `tools/check_version_gate_tests.py` | CI `version-gate`；CTest `cuexis_contract_version_gate` |
| `tools/update_version.py` | CI Linux 与预推送的 `--check` |
| `tools/check_pre_push.ps1` | 预推送编排入口（Quick/Linux/Full） |
| `tools/ci/gnu_asset_mirror.py` / `tools/ci/gnu_asset_mirror_tests.py` | CI Linux |
| `tools/verification/check_docs_status_contract_tests.py` / `tools/verification/check_docs_target_contract_tests.py` / `tools/verification/check_docs_section_contract_tests.py` | 预推送 Quick：文档合同回归测试 |
| `tools/verification/check_header_self_containment.py` | 安装头自包含检查（CMake/预推送调用方见 BUILDING） |
| `tools/verification/run_rpa_acceptance_matrix.py` / `tools/run_rpa_acceptance_matrix.py` | RPA 验收矩阵复现（人工） |
| `tools/verification/check_pe_export_gate.py` | CTest `cuexis_pe_export_gate_negatives` |
| `tools/verification/check_gameplay_assembler_tests.py` | CTest `cuexis_gameplay_assembler_cli`（candidate） |
| `tools/verification/check_gameplay_diagnostic_formalization.py` | CTest `cuexis_gameplay_diagnostic_residuals/negatives` |
| `tools/verification/check_gameplay_host_tests.py` | CTest `cuexis_gameplay_installed_consumer` |
| `tools/verification/check_stage6_a2.py` | CTest `cuexis_contract_s6_a2` |

C++ 门禁 target 同样算活门禁；它们由 `tools/CMakeLists.txt` 构建，测试在 `tests/` 注册：

```text
cuexis_cxc_{pack,validate,unpack}        tests/cxc + cmake/VerifyCxcTools.cmake
cuexis_chart_{validator,migrator,candidate}  tests/chart + cmake/VerifyChartTools.cmake / VerifyCandidateCli.cmake
cuexis_media_import / cuexis_media_importer   cmake/VerifyMediaImporter.cmake（media-tools preset）
cuexis_gameplay_author / cuexis_gameplay_assemble  tests/gameplay_author + VerifyGameplayAssemblerCli.cmake（candidate）
cuexis_asset_importer                    VerifyAssetImporter.cmake（shader-tools preset 覆盖）
```

## 2. 维护者脚本（人工运行，不进 CI）

按用途放在 `verification/`、`cxc/`、`chart/`、`gameplay/`、`practice/`、`shader/`、`media/`、
`research/` 下。它们可以依赖仓库内的构建产物，但不得被 CI 依赖。

常用维护者入口：

| 脚本 | 用途 |
| --- | --- |
| `tools/with-msvc.ps1` / `tools/with-msvc.cmd` | 在普通 shell 中加载 VS x64 开发环境后执行命令（vswhere 定位，保留 VCPKG_ROOT） |
| `tools/prepare_sdk_approval.ps1` / `tools/prepare_sdk_approval.cmd` | 生成并发布 SDK owner 批准评论（只在需要 SDK API 变更时） |
| `tools/check_sdk_approval_assistant_tests.ps1` | 批准助手回归测试 |
| `tools/measure_playback_method.py` | 播放方法耗时测量（人工） |
| `tools/verification/test_check_plan_schedule.py` | 调度门禁的临时 fixture 正反回归；运行 `python -B tools/verification/test_check_plan_schedule.py` |
| `tools/gameplay/gameplay_assembler/generate_author_schema.py` | 生成 gameplay author schema；带 `--check` |
| `tools/practice/` | v5 练习 fixture 与设备验收包（见 `practice/README.md`） |

## 3. 冻结的证据工具（只用于复现历史报告）

以下目录是 R4 / PD 证据链的一次性工具，保留是为了报告可复现；**不得新增依赖，也不进 CI**：

```text
verification/execution_acceptance/     R4 验收与容量/内存证据
verification/playback_performance/     PD-5 观测、ETW 与性能证据
```

**不要物理搬迁这些目录。** 它们的相对路径已经进入验收哈希账本：`execution_acceptance/binding.py:73`
把 `tools/verification/execution_acceptance/budget-proposal.v1.json` 写成字面键，六个 `prepare_*`
把目录 glob 写进 preparation，`playback_performance/etw_safety/` 按文件哈希封包。搬家会让历史
验收记录对不上。冻结的正确做法是“原位 + 无新依赖 + 文件头注明复现报告”。

注意：`execution_acceptance` 的 22 个测试曾在 `tests/pd/CMakeLists.txt` 注册为 CTest，但整块被默认
OFF 的 `CUEXIS_ENABLE_CHART_V5_CANDIDATE` 包住、CI 与预推送都不打开，实际生效数为 0（RR-F511）；
2026-11-17 已按 owner 决定移除该注册。需要复现时直接
`python -B -m unittest discover -s tools/verification/execution_acceptance -t .`。

已停用工具见 [archive/README.md](archive/README.md)（research spike 与 C2 对照脚本）。

## 规则

- 新门禁必须同时：接入 `tools/check_pre_push.ps1`、CI 或 CMake/CTest，并登记到上表。
- 一次性证据脚本必须在文件头注释写明它复现哪份报告（日期 + 报告路径）。
- 不再使用的脚本先移到 `tools/archive/` 并保留原相对路径，不要直接删除；删除需要 owner 明确同意。
  但第 3 节的哈希绑定套件例外——它们只原位冻结，不搬家。
- 文档链接到工具时用仓库相对路径，移动脚本必须同步更新引用（`check_docs.py` 会检查 Markdown 链接）。
- 本表由 `tools/verification/check_tools_inventory.py` 在预推送校验：`tools/` 下每个脚本都必须在
  本表登记，或被第 3 节/`practice/`/`archive/` 的目录说明覆盖；登记项指向不存在的文件也会失败。

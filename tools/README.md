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
| `tools/verification/check_docs_facts.py` | 预推送：生成物新鲜度与版本漂移；CI `documentation-contracts` 同步运行 |
| `tools/verification/gen_docs_{map,limits,codes}.py` | 不是独立门禁；由 `check_docs_facts.py` 导入校验，没有外部 `--check` 调用 |
| `tools/check_version_gate.py` / `check_version_gate_tests.py` | CI `version-gate`；CTest `cuexis_contract_version_gate` |
| `tools/update_version.py` | CI Linux 与预推送的 `--check` |
| `tools/check_pre_push.ps1` | 预推送编排入口（Quick/Linux/Full） |
| `tools/ci/gnu_asset_mirror.py` / `gnu_asset_mirror_tests.py` | CI Linux |
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
- 本表由人维护；`check_docs.py` 不校验它，表漂移不会被门禁发现（RR-F515）。

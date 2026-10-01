# Stage 6 复核修正：R2 诊断码与错误语义 + R3 版本门禁强化

状态：completed
快照日期：2026-09-28
更新日期：2026-09-28

本报告是 [Stage 6 复核修正计划](../../../stage_plans/reviews/stage-06-review-remediation/plan.md)
批次 **W1（R2 + R3）** 的退出记录。它逐条处置
[两轴复核记录](2026-09-28-summary.md) 中属于诊断码语义与版本门禁的发现项。

**修正原则**：不改写任何历史报告的现象与证据原文；凡原文有误的，一律以"追加订正说明"保留原句。
所有新增断言都以**实际变异代码站点后测试必须失败**的方式反证其非空转，反证记录见 §3。

## 1. 批次边界

- 起始 SHA：`af3134d`（W0 / R1 合并前的工作分支 HEAD）。
- 涉及路径：`engine/cxc/src/`、`engine/chart/src/`、`tests/cxc/`、`tests/chart/`、
  `tests/contracts/`（新增）、`tests/CMakeLists.txt`、`tools/check_version_gate.py`、
  `tools/check_version_gate_tests.py`、`tools/check_stage6_a2.py`、
  `docs/formats/PACKED_CHART_FORMAT.md`，以及一处历史报告的追加订正。
- 未触碰：ADR 0042 冻结决策正文、Stage 7A/8/9–12 任何交付物、提案 1（Chart v1–v3 退出）。

## 2. 逐条处置

| 复核 ID | 严重度 | 现象（原文摘要） | 本批次修正 | 证据 |
| --- | --- | --- | --- | --- |
| STD-01 | 高 | 候选 entry 的 16 MiB 上限报 `cxc.budget.exceeded`，契约码是 `cxc.candidate.budget_exceeded` | `engine/cxc/src/cxc_candidate.cpp` 改用契约码；候选负例断言新码且 `CHECK_FALSE` 旧码 | `cxc_candidate.cpp`；`tests/cxc/cxc_candidate_roundtrip_tests.cpp` 的 R4 预算用例 |
| SPEC-02 | 低 | `packed.budget.section_bytes` 被复用来报 uint32 wire-range 溢出 | wire-range 改用独立稳定码 `packed.field.wire_range`；真正的 section 字节预算保留原码 | `engine/chart/src/packed_chart_tables.cpp`（`narrowU32` + 两处 header 计数器守位）；`docs/formats/PACKED_CHART_FORMAT.md` |
| SPEC-06 | 低 | 同日 build 达上限时 `version.build.exhausted` 抢在 `version.unchanged` 之前，诊断语义错 | 把 `version.unchanged` 判定提到 `exhausted` 之前；加 `2147483647` 边界用例 | `tools/check_version_gate.py`；`tools/check_version_gate_tests.py` |
| SPEC-03 | 中 | `historical` 复验上下文形同虚设（与 live 同规则） | `compare_snapshots` 按 context 分流：`live` 用严格同日规则（`release_date.future`/`stale`）；`historical` 只保留 `candidate_date > trusted_utc_date` 上界 | `tools/check_version_gate.py`；`tools/check_version_gate_tests.py` 的分叉断言 |
| SPEC-05 | 中 | 版本门禁的缺失/自举路径只用假 SHA 覆盖，未用真实仓库验证 | 新增基于临时真实 git 仓库的用例：真实基线缺版本文件、真实候选缺门禁产物、真实 bootstrap shell 执行 | `tools/check_version_gate_tests.py` |
| SPEC-07 | 中 | A2 表征对 `S6-D07`/`S6-D08` 无覆盖，且脚本未进任何门禁 | 补 `check_version_gate_contract()`/`check_reference_host_contract()`，并把脚本注册为 CTest 用例 | `tools/check_stage6_a2.py`；`tests/contracts/CMakeLists.txt`；`tests/CMakeLists.txt` |
| SPEC-04 | 中 | 门禁本身缺少"代码所有者复核 + 独立负例"的可执行保障 | **BLOCKED**，见 §5 | — |
| SPEC-08 | 低 | `--event` 无规则消费 | 加 `choices` 与 help 文案，明确"仅记录；事件分流由 workflow job 条件承担" | `tools/check_version_gate.py` |
| SPEC-17（补强） | 低 | 门禁覆盖仍不完整 | 由 SPEC-05 的真实仓库用例承担 | 同上 |

## 3. 反证记录（每条新增断言必须能被真实变异推翻）

| 断言 | 变异方式 | 结果 |
| --- | --- | --- |
| `cxc.candidate.budget_exceeded` 负例 | 把实现改回 `cxc.budget.exceeded` | 用例失败（`CHECK_FALSE` 命中） |
| `packed.field.wire_range` 源码守护 | 把 `narrowU32` 的码改回 `packed.budget.section_bytes` | 用例失败（`packed_budget_tests.cpp:606`） |
| bootstrap shell 正例 | 不提供 `TRUSTED_ROOT` | 断言失败（`unbound variable`），证明正例真的执行了脚本而非空转 |
| bootstrap shell 反例 | 基线含门禁文件 | 断言失败（rc=0），证明反例不是"因无关原因失败" |
| A2 宿主契约 | 注释掉 `find_package(Cuexis ...)` 整行 | 断言失败 |
| A2 宿主契约 | 把 `target_link_libraries(... Cuexis::Playback)` 改为 `Cuexis::Internal` | 断言失败 |
| A2 D07 契约 | 把 ADR 的冻结子句 `Stage 6 SDK 目标冻结为 0.7.1` 改写 | 断言失败 |
| A2 D07 契约 | 把该子句的版本改为 `0.9.9` | 断言失败 |

**方法论记录**：首轮反证中曾出现"变异后仍通过"的假阴性，经核实是**变异打在了非目标站点**
（同一 token 在注释或兄弟分支中出现多次，`replace(...,1)` 命中了第一处）。
修正变异位置后全部被捕获。这说明：A2 的字符串契约检查必须匹配**整行结构或完整字符串字面量**
（如 `argument == "--x"`、`target_link_libraries(... Cuexis::Playback)`），
不能只匹配裸 token；本批次已据此加固。

## 4. 门禁与命令输出

| 命令 | 结果 |
| --- | --- |
| `python -B tools/check_version_gate_tests.py` | `Ran 15 tests ... OK` |
| `python -B tools/check_stage6_a2.py` | `S6-A2 characterization passed: schemas, entry fixtures, identity/media goldens, boundaries, S6-D07 version gate and S6-D08 named host` |
| `python -B tools/check_docs.py` | `Documentation checks passed: 267 Markdown files and 20 candidate JSON/CXT files validated.` |
| `python -B tools/update_version.py --check` | `Cuexis version is consistent: 26.09.27-2` |
| `git diff --check` | 通过（无空白错误） |
| `ctest --preset debug -R cuexis_contract_ --no-tests=error` | `100% tests passed, 0 tests failed out of 2`（`cuexis_contract_version_gate`、`cuexis_contract_s6_a2`） |
| `cuexis_chart_tests.exe "[spec-02],[packed]"` | `All tests passed (40785 assertions in 61 test cases)` |
| `cuexis_cxc_tests.exe "[hardening]"` | `All tests passed (54 assertions in 14 test cases)` |

## 5. 残余与未核对

> **追加订正（2026-09-28，W3 批次）**：本批次新增的用例
> `test_compare_refs_rejects_invalid_missing_and_non_ancestor_refs` 使用了**环境仓库的 `HEAD^`**，
> 因此在 hosted 的 `actions/checkout` 浅检出（`fetch-depth: 1`）下 `HEAD^` 无法解析，
> 导致 `cuexis_contract_version_gate` 在 `GCC Coverage` 与 `GCC Adapter Coverage` 两个 job 中失败
> （run `36348824124`，`633/634` 与 `679/680`），而本机全历史检出下 15 tests 全绿。
> 这是本批次引入的**真实可移植性缺陷**，由 hosted 门禁捕获；已在 W3 批次改为自建临时仓库
> （不依赖环境历史深度），并在 depth-1 浅克隆中复现失败条件后验证修复。详见
> [W3 报告 §7](2026-09-28-w3-publication-transaction.md)。
> **本节第 4 节所列"本机通过"的结论仅对本机全历史检出成立，不等于 hosted 通过**——这正是
> 任务说明中"不得把未通过写成通过"红线要求区分的情形。

- **SPEC-04：BLOCKED**。ADR 0042 §S6-D07（`:321`）要求"修改门禁本身需要代码所有者复核与独立负例"。
  当前状态：仓库内**没有 `CODEOWNERS`**（`git ls-files | grep -i codeowners` 无输出），
  分支保护与 required reviewers 属于 **GitHub 仓库外设置**，提交任何文件都不能使其生效。
  按计划 R0 的默认裁决，本批次**只登记为 BLOCKED 并给出恢复条件**：
  配置 `CODEOWNERS` + 对 `tools/check_version_gate.py` 与 `.github/workflows/version-gate.yml`
  启用 PR required review，然后追加一条"门禁被修改时若无 owner 复核即失败"的负例。
  本批次**不把未运行或未生效的检查写成通过**。
- `version.build.exhausted` 分支在 `uint32` 上限处仍不可被正常输入触达（预算远低于该上限），
  属防御性分支；本批次只修正其**判定顺序**与边界测试，未改动其可达性。
- Windows 下 `ctest` 逐项证据以本机 MSVC 19.51 构建为准；hosted 三平台结论见 PR 检查。

## 6. 变更文件

- 实现：`engine/cxc/src/cxc_candidate.cpp`、`engine/chart/src/packed_chart_tables.cpp`
- 测试：`tests/cxc/cxc_candidate_roundtrip_tests.cpp`、`tests/chart/packed_budget_tests.cpp`、
  `tests/contracts/CMakeLists.txt`（新增）、`tests/CMakeLists.txt`
- 工具：`tools/check_version_gate.py`、`tools/check_version_gate_tests.py`、`tools/check_stage6_a2.py`
- 文档：`docs/formats/PACKED_CHART_FORMAT.md`、
  `docs/stage_reports/stages/chart-format-foundation/2026-09-17-r3-budgets-and-arithmetic.md`（追加订正）

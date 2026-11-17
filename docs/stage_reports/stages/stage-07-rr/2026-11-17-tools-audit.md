# tools/ 目录审计（2026-11-17）

状态：historical

<!-- cuexis-meta
id: REPORT-TOOLS-AUDIT-2026-11-17
status: historical
authority: none
-->

本报告记录 `tools/` 全目录审计结果。审计分 5 批、由无上下文子代理完成；只读源码、CMake、
CI 配置与文档，未运行任何脚本或构建，未读取 `out/`。

## 1. 结论摘要

`tools/` 共 219 个文件、120 个脚本（`.py`/`.ps1`/`.sh`/`.cmd`），约 2.1 MB。审计不支持
“整目录搬迁归档”的整理方式：R4 与 PD-5 两个证据套件的相对路径已经进入验收哈希账本
（`execution_acceptance/binding.py:73` 的字面键、六个 `prepare_*` 的目录 glob、
`playback_performance/etw_safety/` 的逐文件封包哈希），搬目录会让历史验收记录永久失配。

整理因此定为三条：**原位冻结证据套件**、**修门禁可达性**、**登记报告来源**。

## 2. 分类统计

| 批次 | 文件数 | keep-gate | keep-maintainer | archive | delete-candidate |
| --- | --- | --- | --- | --- | --- |
| `execution_acceptance`（R4） | 50 | 22 | 14 | 14 | 0 |
| `playback_performance`（PD-5） | 37 py | 19 | 17 | 1 | 0 |
| `cxc`/`chart`/`gameplay`/`shader`/`media` | 54 | 51 | 3 | 0 | 0 |
| 根级 + `ci/` + `verification/` 顶层 | 27 | 17 | 10 | 0 | 0 |
| `research` / `practice` / `common` | 34 | 9 | 6 | 19 | 0 |

“delete-candidate”为 0 的原因是每个脚本至少被一份证据报告按路径引用；引用不等于需要，真正的
退役候选是 `playback_performance/compare_read_candidates.py`（无测试、无 docs 引用、只服务已关闭
的 C2 候选）。

## 3. 关键发现（已写回登记册）

| ID | 事实 |
| --- | --- |
| RR-F511 | `execution_acceptance` 的 22 个测试注册为 CTest，但被默认 OFF 的 `CUEXIS_ENABLE_CHART_V5_CANDIDATE` 包住，CI 的 `debug`/`release` 不打开，预推送只跑这两个 preset → 实际生效 0 |
| RR-F512 | candidate preset 的测试过滤为 `[Cc]andidate\|reference_host`，`cuexis_gameplay_assembler_cli` 与 `cuexis_gameplay_installed_consumer` 不被选中 |
| RR-F513 | `cuexis_asset_importer_tool_tests` 需要 `DEVELOPER_TOOLS` + `SHADER_TOOLS`，hosted presets 显式关闭前者 → 仅本地可达；`BUILDING.md:139-140` 漏写前提 |
| RR-F514 | `generate_author_schema.py` 生成合同 schema，但无新鲜度门禁 |
| RR-F515 | `tools/README.md` 原先只登记 Python 门禁，漏 C++ target；已重写 |
| RR-F516 | `compare_read_candidates.py` 是唯一 archive 候选 |
| RR-F517 | R4 归档文件无“复现哪份报告”注释，两处硬编码本机 VS/vcpkg 路径 |
| RR-F518 | CI `documentation-contracts` 不跑 `check_docs_facts.py`；生成块原先声称“CI/预推送校验”，已改文案 |
| RR-F519 | 三个 `check_docs_*_contract_tests.py` 没有 CI/CTest/预推送入口 |

## 4. 整理动作

已完成：

1. 重写 `tools/README.md`：三类生命周期 + 活门禁表（含 C++ target）+ “不要物理搬迁”的哈希依据 +
   原位冻结的正确做法。
2. 修正 `gen_docs_map.py` 的生成文案（不再声称 CI 校验），重生成 `AGENTS.md` 与 `docs/MAP.md`。
3. 登记 RR-F511–RR-F519。
4. 按 owner 决定移除 `tests/pd/CMakeLists.txt` 中零生效的 `cuexis_execution_acceptance_tool_tests`
   注册；复现方式改为人工 `unittest discover`，写入 `tools/README.md`。
5. 归档 `tools/research/gameplay_fold_spike/`（19 文件）与 `compare_read_candidates.py`，新增
   [tools/archive/README.md](../../../../tools/archive/README.md) 记录原路径与原因；同目录 README 引用同步更新。
6. RR-F518：CI `documentation-contracts` 增加 `python3 -B tools/verification/check_docs_facts.py`。
7. RR-F519：预推送 Quick checks 接入三个 `check_docs_*_contract_tests.py`；接入时发现 section 测试因
   stage-07 迁移已漂移出 11 个失败，路径同步到 `completed/stage-07` 后 19 个用例全过。
8. RR-F512：三个 candidate test preset 的过滤加入 `assembler_cli|installed_consumer`。
9. RR-F513：顶层 `add_subdirectory(tools)` 与 `tools/CMakeLists.txt` 改为 `SHADER_TOOLS` 单独即可构建
   `cuexis_asset_importer`，使 hosted shader-tools preset 的 importer 测试可达。

待 owner 决定：无（本批四项均已闭合）。

## 5. 其余三目录

`tools/research/` 19 个文件全部属于 2026-10-01 的 fold spike（`gameplay_fold_spike/`）：无 CMake
target、无代码 import、只被一份 `evidence` 报告与两处散文按路径提及，设计链已被 ADR 0043 → 0044
取代 → **整体 archive 候选**。`tools/practice/` 6 个文件全部是维护者脚本，其中
`make_judgement_practice.py` 的产物仍是 R4 `player_evidence` 的输入，保留。`tools/common/` 9 个
文件是活库 `cuexis_asset_publish`（3 个工具可执行 + 2 个测试 TU 依赖，CI 覆盖），保留。

## 6. 验证

本报告落盘时执行的本地检查：

```text
check_docs_status_contract_tests.py   -> 0（OK）
check_docs_target_contract_tests.py   -> 0（OK）
check_docs_section_contract_tests.py  -> 0（19 tests OK）
check_pre_push.ps1 语法解析            -> 0
python -B tools/check_docs.py         -> 0（731 篇）
check_docs_facts.py                   -> 0
git diff --check                      -> 0
```

CI 与 CTest 的实际执行结果需在推送后由 hosted run 确认；本报告不声称它们已绿。

## 7. 未做

本报告未执行任何移动或删除：`research` 归档会改动 R4 evidence manifest 记录的文件路径，
`compare_read_candidates.py` 归档会改变同目录 README 的引用。两项都在 owner 确认后单独提交；
`tools/research/` 的 19 个文件保持原内容，未改写任何证据报告，证据 manifest 仍按 2026-10-08 的
原路径记录，不再回溯更新。

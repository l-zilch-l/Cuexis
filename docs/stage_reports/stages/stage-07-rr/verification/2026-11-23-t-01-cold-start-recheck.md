# T-13 冷启动缺口复验回执

状态：evidence

更新日期：2026-11-23

<!-- cuexis-meta
id: EVIDENCE-stage-07-rr.t-01-cold-start-recheck
status: evidence
authority: none
date: 2026-11-23
-->

状态：完成。结论类型：设计冷启动缺口复验（静态核对）；不含构建、产品运行、安装或设备证据。

## 基线与范围

| 项 | 值 |
| --- | --- |
| 复验基线 | `3b8bf41e3b26207c4a59860d40b47b747c7545db`（tree `613858153c9b3714822fe702c63cff7ab5fa90bb`） |
| 分支 / 工作树 | `stage-7-rr`；`git status --short` 0 行，干净 |
| 对比基线 | `a22e255f6716b22f96ad8a183136eae4fb04ac2f`（首轮冷启动基线） |
| 范围 | 仅 T-13 四项补注；T-04/T-06 未改，不复验 |
| 未执行 | 未 configure、未构建、未运行任何 CTest；`out/build` 不存在 |

## 四项补注逐项核对

| 检查项 | 证据 | 结论 | 说明 |
| --- | --- | --- | --- |
| 1 `cmake/VerifySharedExports.cmake` 进入写域且授权受限 | [T-13.md](../../../../../../docs/stage_plans/active/stage-07-rr/tasks/T-13.md) 第 25 行；[T-08.md](../../../../../../docs/stage_plans/active/stage-07-rr/tasks/T-08.md) 第 25、34 行 | 成立 | 写域新增「实验 shared 内部导出在本卡新增时只向 `cmake/VerifySharedExports.cmake` 追加本卡具名符号行，以 T-08 已交付的精确清单为基础，不改其旧项与命名空间规则，并在同 preset 重跑 `cuexis_shared_export_surface`」。T-08 的写域包含该 gate，步骤 2 要求精确限定名、禁止命名空间通配；T-13 只追加本卡具名行，职责不冲突，且 T-08 先于 T-13 执行。 |
| 2 独立消费者工程与可执行命令 | T-13.md 第 25、46 行；[T-10.md](../../../../../../docs/stage_plans/active/stage-07-rr/tasks/T-10.md) 第 43 行 | 成立 | 写域明确 `tests/consumers/gameplay-compiled-reader/` 为本卡新增、自带 `CMakeLists.txt`、按 T-10 的 `tests/consumers/judgement-runtime` 同一形式；验收给出 `cmake -S tests/consumers/gameplay-compiled-reader -B out/rr-gameplay-compiled-reader`、`cmake --build out/rr-gameplay-compiled-reader`、`ctest --test-dir out/rr-gameplay-compiled-reader -N -R '^rr_gameplay_compiled_reader$'`、`ctest --test-dir out/rr-gameplay-compiled-reader --no-tests=error -R '^rr_gameplay_compiled_reader$'`。测试名、目录名与 T-10 形式一致。 |
| 3 Chart v4 用例创建责任 | T-13.md 第 54 行；`tests/chart/CMakeLists.txt:16,61` | 成立 | 明确「本卡在该测试源新增 `TEST_CASE("Chart v4 loader stops animation clip parsing at budget")`」，源文件 `tests/chart/chart_v4_c2_closure_tests.cpp` 已在 `cuexis_chart_tests` 源表（第 16 行）并由 `catch_discover_tests`（第 61 行）注册，不新建测试目标；命令含 `-N` 注册核对与 `--no-tests=error` 执行。测试名全仓只出现在文档，无冲突。 |
| 4 D-20 清单指针 | T-13.md 第 52 行；`docs/stage_reports/stages/stage-07-rr/readiness/2026-11-23-legacy-corpus-inventory.md` | 成立 | 链接指向该报告，文件存在（46,716 bytes，`status: evidence`）；报告结论为「部分成立：仓库内未找到具名正式发行判定组合，历史发行资料无法核实，内部候选 L1–L4 与格式夹具 F1/F2 已绑定，缺口交 T-09」，与 T-13 第 52 行的摘要一致。 |

## 改动范围与结构核对

- 相对 `a22e255f`，T-13.md 只改 4 个段落（写域、消费者验收、D-20、RR-F431 用例）；`## 目标/输入/写域/步骤/任务级验收/证据/停止条件` 七段仍完整，未改标题、输入、步骤、证据或停止条件。
- T-13 的 markdown 链接检查：17/17 文件存在，12/12 带锚点链接命中。
- `python -B tools/verification/check_plan_schedule.py` 退出 0：17 张 active、0 张 candidate、1 张 completed；T-13 仍为 active。
- T-10 同步补了同一 D-20 指针，链接检查 7/7、锚点 6/6；与 T-13 第 52 行「与 T-10 核对同一规则」一致。
- 新名称 `rr_gameplay_compiled_reader`、`gameplay-compiled-reader`、`Chart v4 loader stops animation clip parsing at budget` 在源码/CMake 中均无同名项；本轮无需处理名称冲突。

## 自洽性与职责边界

1. **与 T-08 的 shared 导出职责**：T-08 的写域明确包含 `cmake/{VerifySharedExports,...}.cmake`，步骤 2 要求按 T-06 的 R5 精确清单之外逐项放行、禁止命名空间通配。T-13 现在只追加本卡具名符号行并保留 T-08 旧项，按调度 T-08 先于 T-13，属于同一 gate 的顺序扩展，没有发现职责冲突。T-08 的验收仍以 T-06 后的符号表为基线、T-13 在同 preset 重跑该 gate，二者证据时点可区分。
2. **与 T-10 的消费者形式**：T-10 用 `tests/consumers/judgement-runtime` 与 `out/rr-judgement-runtime`，T-13 用 `tests/consumers/gameplay-compiled-reader` 与 `out/rr-gameplay-compiled-reader`，同为独立工程、独立 configure/build；T-13 增加 CTest 正向用例，属于该消费者自己的验收，不加入现有测试目标。
3. **preset 与测试名**：chart 用例使用 `candidate-debug` + `cuexis_chart_tests`，消费者用例使用独立 build 目录 + `rr_gameplay_compiled_reader`，均与既有 `rr_*`/`ctest --test-dir` 约定一致。
4. **D-20**：报告结论与 T-13 摘要一致；T-13 明确「内部候选夹具不可冒充正式发布语料」，与报告「缺口交 T-09」的边界一致，没有把 `无法核实` 写成通过。

## 新问题与实施时确认点

未发现阻断派单的新问题。以下为非阻断残余/实施确认点，不构成 T-13 卡缺陷：

1. **D-20 报告内部相对路径**：报告在 `readiness/` 下，但正文仍按抽取目录写 `hashes.txt`、`commands.log`、`本目录 fixtures/s7a78-host-fixture/`；这些文件实际冻结在 `docs/stage_reports/stages/stage-07-rr/evidence/t-01-baseline/`（`hashes.txt` 与 `legacy-corpus/fixtures/...` 均存在，fixture SHA-256 与 out/ 副本一致）。T-13 的指针本身有效；实施时应使用 evidence 目录的冻结副本并核 `hashes.txt`，建议后续在报告或 dispatch 中补一句定位说明。
2. **「同 preset」未点名**：T-13 第 25 行说「同 preset 重跑 `cuexis_shared_export_surface`」，按 T-06/T-08 约定应为 `candidate-shared-debug`（该 gate 只在 SHARED 配置注册）；实施时按此执行并记录即可。
3. **消费者工程注册与包定位**：消费者自带 `CMakeLists.txt` 需 `enable_testing()` 并注册 `add_test(NAME rr_gameplay_compiled_reader ...)`，且需按 T-10 形式定位已安装的 `cuexis::gameplay_packed_read`（Cuexis 包/prefix）；PA-06 的阴性编译单元归属也需在实现时落到具体文件/CMake。卡面已给出目录、命令与测试名，这些属于实现落点，不影响派单。
4. **D-20 正式发行缺口**：报告结论为「仓库内未找到具名正式发行判定组合、D-20 正式条款无法核实」；T-13 已按卡面交 T-09，不能把内部候选 L1–L4 写成正式发行通过。

## 结论与派单

| 项 | 结论 |
| --- | --- |
| 第 1 项 shared 导出白名单 | 成立 |
| 第 2 项独立消费者工程与命令 | 成立 |
| 第 3 项 RR-F431 用例创建责任 | 成立 |
| 第 4 项 D-20 清单指针 | 成立 |
| T-13 整体 | 成立 |
| 可派单 | 可派单（四项补注已落实；上述 1–3 为实施时确认点，非产品规则变更） |

## 证据与限制

- 本回执只做静态核对；未运行 `cmake`/`ctest`、未运行产品、GL、设备或安装消费者用例；`out/build` 不存在，相关执行项记为未执行。
- 未重跑文档总门禁与 T-01 的调度正反例；本轮只运行了 `check_plan_schedule.py` 和卡片链接/锚点检查。
- 未修改 T-13 或任何已跟踪文件；写入仅限 `out/review/stage-07-rr/T-01/cold-start/recheck/`。

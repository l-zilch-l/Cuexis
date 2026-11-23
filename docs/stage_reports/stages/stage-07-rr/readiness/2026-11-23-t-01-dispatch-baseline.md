# T-01 基线、派单包与冷启动

状态：evidence

更新日期：2026-11-23

<!-- cuexis-meta
id: EVIDENCE-stage-07-rr.t-01-dispatch-baseline
status: evidence
authority: none
date: 2026-11-23
-->

## 结论

**成立**：Stage 7-RR 的实施基线已固定在提交 `a22e255f6716b22f96ad8a183136eae4fb04ac2f`，派单输入逐文件哈希可从该提交的 blob 复现，工作树干净且 `out/` 不参与派单；调度正反例、文档总入口和事实检查在本基线全部退出 0；独立冷启动对 R1、R5 帧桥与一个 `NEXT` 包分别给出成立/成立/部分成立。

**部分成立**：D-20 的旧内容清单只找到内部候选与格式兼容夹具，仓库内没有任何具名正式发行判定组合，历史发行资料是否存在无法核实；因此 D-20 的正式发行回归就现有证据而言不适用，该缺口按具名残余交 T-09，不能作为本轮通过项。

**部分成立**：T-13 冷启动发现四项派单包缺口（shared 导出白名单、独立消费者工程、RR-F431 用例创建责任、D-20 清单指针），均已按卡内规则登记并在修订提交 `3b8bf41e` 补齐；独立复验判定四项全部成立、T-13 可派单，见[冷启动复验](../verification/2026-11-23-t-01-cold-start-recheck.md)。

## 范围与方法

输入为本仓库 `stage-7-rr` 分支的提交 `a22e255f`（T-18 文档治理落盘后的首个干净基线）、[计划](../../../../stage_plans/active/stage-07-rr/plan.md)、[调度](../../../../stage_plans/active/stage-07-rr/schedule.md)、[验收](../../../../stage_plans/active/stage-07-rr/acceptance.md)、[登记册](../../../../stage_plans/active/stage-07-rr/findings.md)与 T-01–T-17 全部任务卡。历史研究基线 `9623efa3`、首次实施交接 `8599c83a`、计划检查交接 `db6042b6` 与治理输入基线 `ca5cbb91` 只作对照，不替代本次采用。

读法遵循 [MAP](../../../../MAP.md)：只打开与基线、派单和冷启动相关的文档。未 configure、未构建、未运行任何产品测试、安装消费者、设备或 hosted 任务；本轮不产生产品能力证据。命令与原始输出保存在 `out/review/stage-07-rr/T-01/`，提交内只保留可复核的收据、机器清单和语料夹具。

哈希口径：规范哈希取基线提交中该路径的 blob 字节（`git cat-file blob <BASE>:<path>`），在任何 checkout 上都能复现。本机 Windows 的 `core.autocrlf=true` 使 134 个文本输入在工作树按 CRLF 物化；`manifest.json` 同时记录工作树字节哈希，行尾归一化后与 blob 相同，内容差异为 0。

## 执行记录

```text
git rev-parse HEAD            -> a22e255f6716b22f96ad8a183136eae4fb04ac2f
git rev-parse 'HEAD^{tree}'   -> 7869115460b0c17a1f833f30700cd06f34e8e3c3
git status --short            -> (empty)
git ls-files --others --exclude-standard -> 0 files
git diff --name-status a22e255f --       -> (empty)
git diff --binary a22e255f --            -> (empty)

python -B tools/check_docs.py                        -> 0 (778 Markdown / 20 candidate JSON-CXT; target facts NOT CHECKED)
python -B tools/verification/check_docs_facts.py     -> 0 (SDK 0.9.1, build 26.11.22-1, drift 0)
python -B tools/verification/check_plan_schedule.py  -> 0 (17 active / 0 candidate / 1 completed)
python -B tools/verification/test_check_plan_schedule.py -> 0 (Ran 27 tests, OK)
git diff --check                                     -> 0
```

派单清单由 [capture_baseline.py](#证据) 生成：120 个全局输入、17 张任务卡、191 个输入引用；[hashes.txt](../evidence/t-01-baseline/hashes.txt) 固定 202 个唯一路径（含全部 17 张卡），缺失 0。另保存设计基线到本次基线的逐文件差异（`9623efa3..a22e255f` 共 167 项：61 新增、100 修改、6 重命名）。

## 结果与证据

### 基线采用

| 结论 | 证据 | 状态 |
| --- | --- | --- |
| 基线提交、tree、分支与干净工作树可复现 | [baseline.md](../evidence/t-01-baseline/baseline.md)、[manifest.json](../evidence/t-01-baseline/manifest.json) | 成立 |
| 202 个派单路径（含 17 张卡）的 blob 哈希复现，缺失 0 | [hashes.txt](../evidence/t-01-baseline/hashes.txt) | 成立 |
| 17 张卡按 scope code（DOC/RR2/R1/R5/RR4/NEXT 与 T-17 独立修复）绑定到同一基线 | [baseline.md](../evidence/t-01-baseline/baseline.md)「逐卡采用绑定」 | 成立 |
| 设计基线到本次基线无产品源码变化 | `history/name-status-9623efa3..a22e255f.txt`；`engine/`、`app/`、`tests/`、`schemas/` 0 项 | 成立 |
| 版本文件变化仅构建日期 `26.11.18-1` → `26.11.22-1`，SDK API 仍 `0.9.1` | 同上 diff；[CuexisVersion.cmake](../../../../../cmake/CuexisVersion.cmake) | 成立 |
| T-18 治理封存输入只作历史证据，不作本次基线 | `evidence/documentation-governance/{inputs.zip,working-tree.patch,migration.json}` 已入库，`ca5cbb91+补丁` 不再需要重放 | 成立 |

采用记录与决策绑定见 [adoption.md](../evidence/t-01-baseline/adoption.md) 的要点摘要（完整版在 `out/review/stage-07-rr/T-01/adoption.md`，本提交保留同一内容的可复核表）：

| 任务 | scope | 主合同/输入 | 前置 |
| --- | --- | --- | --- |
| T-01 | `DOC` | `plan.md`、`acceptance.md`、`schedule.md`、`findings.md` | T-18 |
| T-02 | `DOC` | `CURRENT_STATUS.md`、`formats/README.md`、`VERSIONING.md` | T-04 |
| T-03 | `RR2` | `api/*`、`formats/*`、`schemas/cuexis.gameplay-diagnostics.v2.codes.json` | T-01 |
| T-04 | `R1` | `contracts/runtime-timing.md#构建头与安装`、`#r1八个-playback-tu` | T-03 |
| T-05 | `R5` | `contracts/render-player-writer.md#内存与布局` | T-04 |
| T-06 | `R5` | `contracts/render-player-writer.md#提交资源与交付` | T-05、T-17 |
| T-07 | `DOC` | 三份候选合同、[报告模板](../../../../templates/report.md) | T-02、T-06、T-08、T-15、T-16 |
| T-08 | `RR4` | `contracts/runtime-timing.md#rr-4-逐符号兼容迁移` | T-03、T-04、T-06 |
| T-09 | `DOC` | `acceptance.md`、`findings.md` | T-07、T-17 |
| T-10 | `NEXT` | `contracts/runtime-timing.md#prepared-与当次推进` | T-08 |
| T-11 | `NEXT` | `contracts/runtime-timing.md#作者版本stop比例和最小多等级`、[CXT2](../../../../formats/CXT_V2_FORMAT.md) | T-06、T-10 |
| T-12 | `NEXT` | `contracts/runtime-timing.md#谱面时间-api` | T-10、T-11 |
| T-13 | `NEXT` | [后继成品合同](../../../../stage_plans/active/stage-07-rr/contracts/prepared-artifact.md) | T-12 |
| T-14 | `NEXT` | `contracts/runtime-timing.md#playback-时钟与实时准入` | T-08、T-13、T-17 |
| T-15 | `NEXT` | `contracts/render-player-writer.md#命令与启动` | T-11、T-14 |
| T-16 | `NEXT` | `contracts/render-player-writer.md#p5内部-writer-与显式迁移` | T-13 |
| T-17 | 独立修复 | [Prepared 有效性](../../../../api/playback-session.md#preparedplayback-有效性修订f501已接受未实现)、[RuntimeSession](../../../../architecture/RUNTIME_SESSION.md) | T-03、T-04 |

随基线生效的已接受范围：T-01–T-17 统一实施，含 F302、F310、F313/F316、F207 的全面结构清理；RR 完整实现并实验验证 Gameplay v3，Stage 8 以 Chart v5＋Gameplay v3 为正式主线；F501 有效性、F135 `allocation_failed` 与观测丢失分类、F203 登记前整批验证为已接受未实现。不随基线生效：实际发布、SDK minor 晋升、公共 API 退役、设备与 hosted 证据。

### 调度正反例（独立代理）

| 核对项 | 结论 | 关键证据 |
| --- | --- | --- |
| 正例：调度检查与 27 项自测退出 0 | 成立 | 独立复跑，见[调度正反例回执](../verification/2026-11-23-t-01-schedule-controls.md) |
| 阴性 A：副本给 T-05 加 T-06 前置退出 1，唯一错误精确指到第 20 行 | 成立 | 同上；`prerequisite T-06 must be in an earlier wave` |
| 阴性 B：删除、围栏、注释三种隐藏 T-05 `## 停止条件` 均退出 1，唯一错误为七段结构 | 成立 | 同上 |
| 正本 `schedule.md` 哈希前后一致（`85cdee01…83b7`），30 个文件未变 | 成立 | 同上 |
| 派单清单哈希、独立 checkout 与 167/61/100/6 差异 | 成立（两轮复核后改口径与两处补正） | 交叉复核 178/178 路径一致、5104 文件解出；行尾差异导致 3 个抽样文件字节不同，已把规范哈希改为 blob 口径。第二轮复核又发现 `hashes.txt` 漏 11 张卡、4 个夹具文本被行尾归一化、工作树哈希随修订提交过期，均已修正并复验 |

**第二轮交叉核验的处置。** 复核代理按基线 `a22e255f` 重算了 `hashes.txt` 的 191 项，全部一致，同时指出三处：哈希表只含 6 张任务卡，其余 11 张只在 `manifest.json` 的 `tasks[].card` 里；`legacy-corpus/fixtures/` 的 4 个文本夹具在提交时被 CRLF→LF 归一化，与语料清单记录的条目哈希不符；`sha256_worktree` 与 `worktree_variant` 是采用当时的工作树快照，后续修订提交会改变部分输入，若不注明会被误读成当前值。处置：`hashes.txt` 重建为 202 个路径并逐项对 `a22e255f` 的 blob 复算，0 不一致；`.gitattributes` 把 T-01 夹具目录标为 `binary`，重新入库后 6 个文件的 blob 哈希逐项等于 S7A-7/8 证据清单记录值；`manifest.json` 增加 `adoption_snapshot`，写明 blob 值为复核口径、工作树哈希属于采用快照。复核原始记录在 `out/review/stage-07-rr/T-01/schedule-controls/blobs/`。

### D-20 旧内容清单

[旧谱兼容语料清单](2026-11-23-legacy-corpus-inventory.md)从格式矩阵、ADR 0041、现有 golden、`tests/judgement/` 与交付记录追溯到 7 个组合：4 个判定候选（L1–L4）、2 个格式兼容（F1–F2）、1 个已退出 wire（R1）。结论为部分成立：

- 唯一存储态旧判定成品是 S7A-7/8 证据包中的 `main.cxc`（包 `012efd46…`，条目 `a8cf5275…`），带设置、输入和交付时人工 golden，但只有内部交付身份。
- L2–L4 的输入与预期内嵌在测试源码，无独立成品；F1/F2 无判定内容，字节 golden 由 production Writer 生成；R1 已被现行 Reader 拒绝。
- 仓库内具名正式发行判定组合为 0；`plan.md` §13 与验收页的发布门禁都写明本阶段不发布。历史发行资料是否存在无法核实。

独立[交叉核验](../../../../stage_plans/active/stage-07-rr/findings.md#24-统一实施前设计裁定2026-11-23)确认 A/B/D 三项成立、C 项的「语料不存在」表述过强，已按 9 条意见改写为「仓库内未找到、历史资料无法核实」，并把缺口交 T-09。T-10/T-13 的 D-20 条目已指向该清单。

### 独立冷启动

[冷启动回执](../verification/2026-11-23-t-01-cold-start.md)只用卡片、链接输入与采用记录，抽查 R1（T-04）、R5 帧桥（T-06）与 `NEXT` 包（T-13）：38 个链接文件全部存在、25 个锚点全部命中、点名的 preset/target/CTest 均存在。结论 T-04 成立、T-06 成立、T-13 部分成立。

T-13 的四项缺口：`cmake/VerifySharedExports.cmake` 未进写域、`tests/consumers/gameplay-compiled-reader` 未标本卡新增且无注册方式、`Chart v4 loader stops animation clip parsing at budget` 未标创建责任与测试源、D-20 清单没有指针。四项均属派单包注记，不改产品规则，已在后续修订提交补齐并由同一冷启动代理复验（见下节）。

## 后续修订与复验

按 T-01「发现不存在的入口…该卡回到设计复核」，T-13 在派单前回到设计复核补注：写域补 shared 导出白名单的窄授权与独立消费者工程，验收补 `tests/consumers/gameplay-compiled-reader` 的配置/构建/注册命令，RR-F431 用例标为本卡新增 `TEST_CASE`，D-20 条目指向本报告的语料清单。T-10 的 D-20 条目同步补指针。补注随提交 `3b8bf41e` 入库后，由同一冷启动代理复验：四项全部成立，T-13 可派单，七段结构与 17/17 卡片链接、12/12 锚点未坏，见[冷启动复验](../verification/2026-11-23-t-01-cold-start-recheck.md)。复验另留三条非阻断点：语料清单的 `hashes.txt`/`commands.log`/`fixtures` 仍按取证时 `out/` 路径书写（冻结副本在 `evidence/t-01-baseline/`），`cuexis_shared_export_surface` 的实验侧重跑 preset 已明确为 `candidate-shared-debug`，消费者 CMake 的 `add_test` 与包定位留到 T-13 实施时确认。

## 限制与未做

本轮只做基线、派单与静态冷启动，没有 configure 或构建，`out/build` 不存在，CMake target 事实缺输入：`check_docs.py` 的 target 合同项按设计标 `NOT CHECKED`，严格入口会因缺输入失败。所有构建、CTest、安装消费者、设备与 hosted 项都随对应任务取得，不能由本报告或治理门禁代替。

D-20 结论只覆盖仓库内可得材料；仓库外是否曾发布过具名判定组合无法核实，因此「正式发行回归不适用」必须带此限定。语料清单的内部候选 L1–L4 不能升级为正式支持承诺。该清单的 `hashes.txt` 记录取证时的工作树字节（含内部文件），规范基线哈希以[本报告附件](../evidence/t-01-baseline/hashes.txt)的 blob 口径为准；两者在行尾上可能不同。

`out/review/stage-07-rr/T-01/` 保留完整日志、副本、checkout 与 110KB 级清单；本提交只固定收据、哈希表与夹具。独立 checkout 若要核对派单，按 [hashes.txt](../evidence/t-01-baseline/hashes.txt) 与 [manifest.json](../evidence/t-01-baseline/manifest.json) 的 blob 口径复算，不需要 `out/`。

## 证据

- [baseline.md](../evidence/t-01-baseline/baseline.md)：基线身份、工作树清点、全局输入哈希、逐卡采用表。
- [manifest.json](../evidence/t-01-baseline/manifest.json)：机器清单（blob 与工作树双哈希、逐卡输入与命令、七段结构）。
- [hashes.txt](../evidence/t-01-baseline/hashes.txt)：202 个路径的 blob SHA-256（120 个全局输入、191 个输入引用去重、17 张任务卡）。
- [fixtures/s7a78-host-fixture](../evidence/t-01-baseline/legacy-corpus/fixtures/s7a78-host-fixture/)：L1 旧成品与设置/输入/命令的字节副本。
- 阶段索引：[Stage 7-RR 报告索引](../README.md)。

`out/review/stage-07-rr/T-01/` 下另有：`gates.log`（本次门禁原文）、`history/`（三条历史差异清单）、`schedule-controls/`（17 次运行原始日志与副本）、`cold-start/`、`legacy-corpus-crosscheck/`、`manifest_crosscheck.py` 与 `capture_baseline.py`。

## 对登记册与合同的影响

- 登记册新增 §28「T-01 基线与派单（2026-11-23）」：记录采用基线、D-20 语料结论、T-13 冷启动四项缺口与处置；不新设产品规则。
- 计划 §12 追加采用记录行，§4「排期与实施状态」改为 T-01 完成、下一卡 T-03。
- [CURRENT_STATUS](../../../../CURRENT_STATUS.md) 与 [HANDOFF](../../../../HANDOFF.md) 同步当前任务与验证状态。
- T-10、T-13 卡补 D-20 清单指针；T-13 另补四项派单注记，均在修订提交登记，未改产品规则、wire、接受集或兼容窗口。

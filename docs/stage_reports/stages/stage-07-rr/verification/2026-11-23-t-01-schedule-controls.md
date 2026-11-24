# T-01 调度正反例与环境基线独立复核回执

状态：evidence

更新日期：2026-11-23

<!-- cuexis-meta
id: EVIDENCE-stage-07-rr.t-01-schedule-controls
status: evidence
authority: none
date: 2026-11-23
-->

- 复核代理：`controls`（独立于 T-01 实施代理）
- 工作树：`E:\Cuexis-worktree`，分支 `stage-7-rr`
- 运行时间：2026-11-23T19:21:03+08:00
- 证据目录：`out/review/stage-07-rr/T-01/schedule-controls/`
- 运行环境：Python 3.14.6（`C:\msys64\ucrt64\bin\python.exe`），`PYTHONIOENCODING=utf-8`
- 方法：`run_controls.py` 只读正本阶段目录，所有改动只落在 `copies/` 下四份独立副本；每次运行单独保存原始 stdout/stderr 字节；未修改检查器或任何已跟踪文件，未 `git add`/`commit`，未运行完整构建。

## 结论摘要

五项核对全部符合预期，`controls.json` 的 `summary.all_match` 为 `true`，17 次子进程运行的 `matched_expectation` 全为 `true`。

| 核对项 | 结论 | 关键证据 |
| --- | --- | --- |
| 1. 基线事实 | 成立 | HEAD、tree 与期望逐字符相同，`git status --short` 为空 |
| 2. 正例 | 成立 | 检查器退出 0；自测退出 0 且 `Ran 27 tests ... OK` |
| 3. 阴性对照 A（T-05 前置加 T-06） | 成立 | 基线副本退出 0；改动后退出 1，唯一错误精确命中 T-05 所在第 20 行 |
| 4. 阴性对照 B（删/藏 T-05 停止条件） | 成立 | 删除、`text` 围栏、HTML 注释三种变体均退出 1，且唯一错误为七段结构错误 |
| 5. 正本完整性 | 成立 | `schedule.md` SHA-256 前后同为 `85cdee…83b7`；30 个正本文件清单前后一致；最终 `git status --short` 为空 |

## 与 T-01「任务级验收」原文对照

| 验收原文条目 | 结论 | 说明 |
| --- | --- | --- |
| 第一条：两个正例退出 0；副本 `--stage-dir` 先退出 0；仅给 T-05 前置加 T-06 后退出 1，唯一错误精确对应 T-05 行；保存前后哈希、命令、stdout/stderr、退出码、单行差异和回执，确认正本 `schedule.md` 哈希未变、副本其他文件未变 | 成立 | 本回执第二、三、五节逐项留证；正本与副本清单见 `hashes/` |
| 第二条：`python -B tools/check_docs.py` 与 `git diff --check` 退出 0；抽查派单清单合同哈希和输入路径；冷启动回执覆盖交叉验证指定的卡 | 无法核实 | 本代理未执行这两条命令，也未核对派单清单与冷启动回执；不在本次调度控制取证范围，不据此下结论 |
| 第三条：另一副本删除 T-05 `## 停止条件` 须退出 1 并报告七段结构错误；标题放进代码块或注释也不得通过；检查器分别列出 active 与 candidate 卡，active 不得依赖 candidate；当前 T-01–T-17 均为 active | 部分成立 | 删除、围栏、注释三个负例均成立；正例输出确认列出 17 张 active 卡（T-01–T-17）、0 张 candidate、1 张 completed（T-18）。当前无 candidate 卡，「active 不得依赖 candidate」未在真实数据上单独构造负例，仅由退出 0 的 27 项自测间接覆盖，该子句本代理无法独立核实 |
| 第四条：D-20 清单每个正式支持组合可回溯，内部夹具不得冒充正式语料 | 无法核实 | 不在本次调度控制取证范围 |

本代理核对项未触发 T-01 停止条件：HEAD/tree 可复现，副本运行除预期错误外无异常，未出现需要追问产品规则的情形。

## 1. 基线事实

| 命令 | stdout | 期望 | 结论 |
| --- | --- | --- | --- |
| `git rev-parse HEAD` | `a22e255f6716b22f96ad8a183136eae4fb04ac2f` | 同左 | 成立 |
| `git rev-parse 'HEAD^{tree}'` | `7869115460b0c17a1f833f30700cd06f34e8e3c3` | 同左 | 成立 |
| `git status --short` | （空，0 字节） | 空 | 成立 |
| `git branch --show-current` | `stage-7-rr` | `stage-7-rr` | 成立 |
| `git check-ignore out` | `out`，退出 0 | `out/` 被忽略 | 成立 |

全部命令退出码 0，stderr 为空。原始记录：`logs/baseline-head.*`、`logs/baseline-tree.*`、`logs/baseline-status.*`、`logs/baseline-branch.*`、`logs/baseline-check-ignore-out.*`。`out/` 被 `.gitignore` 忽略，所以证据新增不会污染工作树。

## 2. 正例

`python -B tools/verification/check_plan_schedule.py`：退出 0，stderr 为空。stdout 全文：

```text
Plan schedule checks passed: 17 active task cards; 0 candidate cards; 1 completed cards.
Current-stage cards (adoption and prerequisites still apply): T-01, T-02, T-03, T-04, T-05, T-06, T-07, T-08, T-09, T-10, T-11, T-12, T-13, T-14, T-15, T-16, T-17
Candidate-only cards (separate adoption required): none
Completed cards: T-18
```

`python -B tools/verification/test_check_plan_schedule.py`：退出 0，stdout 为空；unittest 按默认行为把结果写到 stderr，内容为 27 个点号、`Ran 27 tests in 0.649s` 和 `OK`。测试数 27 与期望一致，stderr 中无失败或错误信息。

原始记录：`logs/positive-check.stdout.log`、`logs/positive-check.stderr.log`、`logs/positive-test.stdout.log`、`logs/positive-test.stderr.log`。

## 3. 阴性对照 A：T-05 行加 T-06 前置

副本：`copies/A/stage-07-rr/`（正本阶段目录整体复制，30 个文件）。

1. 未改动前运行 `python -B tools/verification/check_plan_schedule.py --stage-dir <副本>`，退出 0，输出与正例相同（`logs/negA-baseline.stdout.log`）。
2. 只在副本 `schedule.md` 第 20 行做字节级替换，单行差异为：

```diff
- | 5 | T-05 | T-04 | 可并行 |
+ | 5 | T-05 | T-04, T-06 | 可并行 |
```

3. `schedule.md` SHA-256：改动前 `85cdee01bb41c7feb48348cc719508e9400618dd886bae3cbb967ec3d6ba83b7`（与正本相同），改动后 `9298a8f40dcca553f98562fc044f512610469c85636efad53ac3b13da57660cb`。见 `hashes/copy-A-schedule.sha256.txt`、`hashes/copy-A-schedule-line.diff`、`hashes/copy-A-schedule-unified.diff`。
4. 再次运行同一命令，退出 1，stderr 为空，stdout 只有一条 finding：

```text
Plan schedule checks failed:
- E:\Cuexis-worktree\out\review\stage-07-rr\T-01\schedule-controls\copies\A\stage-07-rr\schedule.md: line 20: prerequisite T-06 must be in an earlier wave
```

stdout 中以 `- ` 开头的行数为 1，唯一错误精确对应 T-05 所在第 20 行。原始记录：`logs/negA-mutated.stdout.log`、`logs/negA-mutated.stderr.log`。

5. 副本文件清单前后各 30 项，变化文件仅 `schedule.md`（`hashes/copy-A-manifest.before.json`、`hashes/copy-A-manifest.after.json`）。

## 4. 阴性对照 B：删除或隐藏 T-05 `## 停止条件`

三份独立副本均先确认基线退出 0：

- `copies/B-delete/stage-07-rr/`：删除整节（标题、空行、正文三行）：

```diff
- ## 停止条件
-
- 越过宿主区写入、失败路径漏恢复或布局验证需要启用新 Reader/成品时停在纯函数/GL helper。
```

- `copies/B-fence/stage-07-rr/`：同样删除整节，再把完全相同的内容放进 ```` ```text ```` 围栏追加到卡尾。
- `copies/B-comment/stage-07-rr/`：同样删除整节，再把完全相同的内容放进 `<!-- ... -->` 注释追加到卡尾。

三份副本改动后的运行均退出 1，stderr 为空，stdout 各只有一条 finding。三份副本除 `<副本路径>` 外逐字符相同：

```text
Plan schedule checks failed:
- <副本路径>\tasks\T-05.md: task sections must occur once in order: 目标, 输入, 写域, 步骤, 任务级验收, 证据, 停止条件
```

删除后标题已不是标题；围栏与注释变体中标题字面仍存在，但检查器按代码块/注释语义跳过，未把它当作章节，因此三种变体都被同一结构错误拒绝，说明放行不依赖字面匹配。原始记录：`logs/B-delete-*.log`、`logs/B-fence-*.log`、`logs/B-comment-*.log`；差异见 `hashes/B-*-delete.diff`、`hashes/B-*-inject.diff`、`hashes/B-*-final-unified.diff`。

各副本改动前后清单均为 30 项，变化文件仅 `tasks/T-05.md`。T-05 SHA-256 改动前为 `d910f4187a2003743f02bbd1af8ad85cc6767bda0cc36b341f9d8fb55c5ec5da`；删除后为 `97f7b6ede2ecce8de3f93e5f1d6181ab466ad98592848534a403e395b9d8edc1`，围栏后为 `8bc857690b08119a996a7478d2dac60446d69d31eb528d5b5d24112de8062df8`，注释后为 `dc1c91681daed0dfbda7b60737ef16d0674ab01ac9e5db6ca5797147191b356a`。三份副本的 `schedule.md` 均保持 `85cdee…83b7`。见 `hashes/B-*-sha256.txt`、`hashes/B-*-manifest.*.json`。

## 5. 正本完整性

- 正本 `docs/stage_plans/active/stage-07-rr/schedule.md` SHA-256：开始时 `85cdee01bb41c7feb48348cc719508e9400618dd886bae3cbb967ec3d6ba83b7`，全部结束后相同（`hashes/schedule-sha256.start.txt`、`hashes/schedule-sha256.end.txt`）。另用 PowerShell `Get-FileHash -Algorithm SHA256` 独立重算，结果同为 `85cdee…83b7`（`logs/manual-final-verify.log`）。
- 正本阶段目录 30 个文件的 SHA-256 清单开始时与结束时逐项相同，变化文件列表为空（`hashes/original-stage-manifest.start.json`、`hashes/original-stage-manifest.end.json`）。
- 全部结束后 `git status --short` 无输出，HEAD 仍为 `a22e255f…ac2f`，tree 仍为 `78691154…e3c3`。新增内容全部位于被忽略的 `out/` 下。

## 证据索引

- `controls.json`：17 次运行的命令、cwd、退出码、完整 stdout/stderr、是否命中预期、单行/注入 diff、副本前后哈希与清单、正本完整性、汇总判定。
- `commands.log`：`run_controls.py` 执行的完整子进程命令序列及输出文件映射（含环境与只读输入说明）。
- `logs/`：17 次运行各自独立的 `.stdout.log` 与 `.stderr.log`（34 个文件，按原始字节保存），另有独立人工终检 `manual-final-verify.log`。
- `hashes/`：正本与副本清单、`schedule.md`/`T-05.md` 前后哈希、单行差异、unified diff。
- `copies/`：四份隔离副本，保留最终改动状态供交叉核对。
- `run_controls.py`：取证驱动，可重跑；只清理自身的 `logs/`、`copies/`、`hashes/` 后重新生成上述证据，不写正本。

## 限制

本回执只覆盖调度检查正反例与环境基线事实。`check_docs.py`、`git diff --check`、派单清单哈希与路径、冷启动回执、D-20 清单未由本代理核对，相关验收条目标为无法核实。未运行构建或产品测试，本项检查不需要它们。

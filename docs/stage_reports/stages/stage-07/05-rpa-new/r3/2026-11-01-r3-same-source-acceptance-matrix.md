# R3 同源本地验收矩阵（2026-11-01）

状态：local matrix；test-only；**不是** R3/R4 退出、设备验证、hosted 验证或 owner 接受

日期：2026-11-01

源码：`466ff76a0b2a2f45b90280331bded77897ef6421`（worktree_sha256 `9b6217829df4243771b7513e1c4ab64635b0e01d57143c380df5f9f119962078`）

返回 [RPA 报告索引](../README.md)。当前阶段状态只见
[CURRENT_STATUS](../../../../../CURRENT_STATUS.md)。
本报告记录一次完整、unfiltered、同源的本地验收矩阵运行；判定证据附件见
[矩阵证据](2026-11-01-rpa-acceptance-matrix-evidence/index.md)。

## 为什么现在重跑

冻结的 1080p 容量点由 `f90c2e1` 时代的二进制测得。从 `f90c2e1` 到本次被测的 `466ff76` 之间有
38 个提交：产品源码（`engine/` + `app/`）改动 38 个文件、+896/−426 行，测试改动 25 个文件、
+1253/−95 行，另有文档与证据。

`out/rpa-acceptance/` 里此前只有一次尝试（`20261031-220604-9e3ab6ac`，2026-10-31 22:06），它在
行 8 因磁盘耗尽（`OSError: [Errno 28] No space left on device`）中断：前 7 行 `local_pass`，行 8
未完成，行 9–11 未开始，清单里没有写 `source_unchanged`。因此本轮是这 11 行在同一份源码上的
**第一次完整执行**，“当前源码能否在本机按要求构建并通过全套用例”必须由它回答，不能把任何旧行
结论平移过来。

矩阵的用途只有两个：证明每个配置从同一份工作树状态出发可构建、可跑完整 CTest；证明构建输入
在校验期间没有漂移（每行前后重算哈希）。它不产生性能门、不产生 240 FPS 结论、不验证设备、键盘、
听音、设备丢失恢复或 hosted 同 SHA。

## 方法

- 运行器：`tools/verification/run_rpa_acceptance_matrix.py`，通过
  `out/rpa-acceptance/<时间戳>-<uuid8>/` 保存每一次尝试。
- 11 行，预设覆盖 MSVC 与 MinGW、Debug 与 Release、静态与共享、带 Chart v5 候选与不带、图形与
  headless，以及安装消费者与 `find_package` 消费路径；每行实际走到哪个阶段由结果一节给出。
- 每行依次执行 `cmake --preset <preset> --fresh -B <build> <options>`、`cmake --build <build>
  --clean-first --parallel 4`、`ctest --test-dir <build> --no-tests=error --output-on-failure -j 4`；
  候选行的 `[measurement]` 用例按同一条命令重复 5 次，逐次保留日志。
- 每行在构建前后各重算一次工作树哈希（tracked 文件 + 允许目录下的未跟踪实现文件），并要求相等；
  任何行不等则整次运行退出非零。因此运行期间不允许改动 `app/`、`tests/`、`tools/`、`engine/`、
  `cmake/`、`schemas/`、`docs/`。
- 构建目录为 `out/build/` 下的既有预设目录，每行 `--fresh` + `--clean-first`，不复用旧对象。

## 结果

全部数字取自同目录 `summary.json`，`summary.json` 由 `summarize_matrix.py` 从 `manifest.json`
与磁盘上的阶段日志重新推导；本报告的表格由 `fill_report.py` 生成，不手抄。

| 行 | 预设 | 状态 | configure | build | ctest | 通过 / 失败 / 总数 | 测量调用 |
| --- | --- | --- | ---: | ---: | ---: | --- | ---: |
| `msvc-on-debug` | `candidate-debug` | local_pass | 21.98 s | 295.86 s | 461.21 s | 1416 / 0 / 1416 | 10 |
| `msvc-on-release` | `release` | failed_ctest | 20.13 s | 405.93 s | 658.6 s | 1415 / 1 / 1416 | 0 |
| `msvc-on-shared-debug` | `candidate-shared-debug` | local_pass | 20.15 s | 264.15 s | 472.17 s | 1399 / 0 / 1399 | 10 |
| `msvc-off-debug` | `debug` | local_pass | 19.39 s | 211.18 s | 368.38 s | 1209 / 0 / 1209 | 0 |
| `msvc-off-headless-debug` | `headless-debug` | local_pass | 16.08 s | 168.71 s | 361.22 s | 1064 / 0 / 1064 | 0 |
| `msvc-off-release` | `release` | local_pass | 18.54 s | 268.29 s | 512.84 s | 1209 / 0 / 1209 | 0 |
| `msvc-off-headless-release` | `headless-release` | local_pass | 15.58 s | 222.88 s | 509.76 s | 1064 / 0 / 1064 | 0 |
| `msvc-off-shared-debug` | `headless-shared-debug` | local_pass | 15.66 s | 157.08 s | 370.18 s | 1062 / 0 / 1062 | 0 |
| `mingw-on-debug` | `mingw-headless-debug` | failed_build | 127.81 s | 296.93 s |  | — | 0 |
| `mingw-on-shared-debug` | `mingw-headless-debug` | failed_build | 18.18 s | 268.74 s |  | — | 0 |
| `mingw-on-release` | `mingw-headless-debug` | failed_build | 17.8 s | 296.45 s |  | — | 0 |
| **合计** | | 7 / 11 行 local_pass | | | | 9838 / 1 / 9839 | 20 |

### 失败与未覆盖

- `msvc-on-release`（`out/build/rpa-candidate-release`）在 `#904 Audio publication preserves device observation time across service polls` 失败。该行因此由运行器判定为 `failed_ctest`，其按目标各 5 次的 `[measurement]` 重复**没有执行**（运行目录里不存在该行的 `*-budget-*` 文件），所以这一行既不提供性能观测，也不能计入“全绿”。
- `mingw-on-debug` 状态 `failed_build`，在构建阶段终止，未进入 CTest：`tests/pd/projection_integration_tests.cpp:79:24: error: 'count_if' is not a member of 'std::ranges'; did you mean 'std::count_if'?`（该行构建日志里共有 1 条不同的 `error:` 行）。这是 GCC 对仓库测试源码报出的真实可移植性缺陷，不是“预设不可用”，也不计为通过。
- `mingw-on-shared-debug` 状态 `failed_build`，在构建阶段终止，未进入 CTest：`tests/pd/projection_integration_tests.cpp:79:24: error: 'count_if' is not a member of 'std::ranges'; did you mean 'std::count_if'?`（该行构建日志里共有 1 条不同的 `error:` 行）。这是 GCC 对仓库测试源码报出的真实可移植性缺陷，不是“预设不可用”，也不计为通过。
- `mingw-on-release` 状态 `failed_build`，在构建阶段终止，未进入 CTest：`tests/pd/transaction_allocations.hpp:57:14: error: 'void free(void*)' called on pointer returned from a mismatched allocation function [-Werror=mismatched-new-delete]`（该行构建日志里共有 1 条不同的 `error:` 行）。这是 GCC 对仓库测试源码报出的真实可移植性缺陷，不是“预设不可用”，也不计为通过。

### 构建输入未漂移

`manifest.json` 的 `source.worktree_sha256` 为 `9b6217829df4243771b7513e1c4ab64635b0e01d57143c380df5f9f119962078`，覆盖 3111 个 tracked 文件；每行在构建前后各重算一次并要求相等。
本次 11 行（含未进入 CTest 的行）逐行 `source_unchanged` 均为真，`manifest_source_unchanged` 为真。

## 结论

本次 11 行里 7 行 `local_pass`，1 行有用例失败，3 行没有进入 CTest。

因此本次运行**不是全绿**：同源完整矩阵（P6）在本轮**未达成**。这一结论不因为任何单行在孤立重跑中通过而改变——被记录的是 CTest 实际跑出的结果。

缺口的直接原因是 `msvc-on-release` 的 CTest 失败：一行一个用例，且按运行器的规则，该行在失败后不再执行 `[measurement]` 重复。孤立诊断（同一二进制、按 sha256 绑定）记录在 `out/verification-20261031/r3-matrix-flake-904/diagnosis.json`，它回答“能不能复现、在什么负载下复现”，不改变这一行在本轮矩阵中的判定。

`mingw-on-debug`、`mingw-on-shared-debug`、`mingw-on-release` 行在构建阶段就终止：GCC（`x64-mingw-static`）对仓库测试源码报出的错误已按日志原文登记在上一节。这些是真实的可移植性缺陷，不是工具链借口，也不能读成“MinGW 不可用”；但它们确实使本轮**没有 MinGW 覆盖**，因此该配置同样不计入全绿。

即使全绿，本矩阵也只证明“同一工作树状态在本机各配置下可构建、可跑完整用例”，不构成 R3/R4 退出、hosted 验证、设备验证或 owner 接受。

## 测量重复（`[measurement]`）

`[measurement]` 是 test-only 观测，不是门限；每行只在 CTest 全通过后才重复。

| 行 | 目标 | 每目标重复 | 事件数 | 事件名 |
| --- | --- | ---: | ---: | --- |
| `msvc-on-debug` | `cuexis_execution_host_tests`<br>`cuexis_rpa_tests` | 5 | 20 | `b5.cpu.content_boundary`、`rpa.f3.kernel_budget` |
| `msvc-on-shared-debug` | `cuexis_execution_host_tests`<br>`cuexis_rpa_tests` | 5 | 20 | `b5.cpu.content_boundary`、`rpa.f3.kernel_budget` |

每个事件带 `attempt` 字段，同一目标内两次事件同属一次进程调用时 `attempt` 相同；原始逐次日志以 `*-budget-<n>.log` 存放在运行目录并已按 sha256 登记。

## 未执行项

运行器在 `manifest.json` 中显式登记下列**未执行**项，本报告不把它们当已通过：

- `physical_keyboard`：未接真实键盘：矩阵不发送物理按键，也不验证键盘扫描码到行为的整条链路。
- `listening`：未做听音：没有任何一组由人耳确认音频输出正确。
- `device_loss_recovery`：未做设备丢失与恢复：没有拔插、切换或热失效注入。
- `production_calibration`：未做生产标定：没有以生产夹具或生产时间戳校准任何阈值。
- `hosted_same_sha`：未做同一 SHA 的 hosted 验证：Linux Quality、Windows MSVC、Windows MinGW 的 hosted 作业都没有在本轮触发。
- `owner_acceptance`：未取得 owner 接受：本报告只登记本机结果。

## 边界

- `test_only: true`、`final_acceptance: false`、`production_thresholds_accepted: false`：
  本次运行采用 test-only 参数，不提升任何生产门限，也不构成阶段接受。
- 不含 GPU 渲染吞吐、显示、VSync、键盘、听音、设备丢失恢复、生产标定与 hosted 同 SHA 验证。
- MinGW 行使用 headless 预设，不覆盖 MinGW 的窗口/GPU 路径。
- 本轮 MinGW 行都在构建阶段终止、没有进入 CTest，所以本轮**没有** MinGW 覆盖。它们失败于 GCC
  16.1.0（`x64-mingw-static`）对仓库测试源码的检查，是真实的可移植性缺陷而不是“预设不可用”；
  具体错误行按阶段日志原文登记在上一节。这不把 MinGW 记为通过，也不把它的缺口算进 MSVC 的结论。
- 行内耗时是构建机器的观测值，不是性能门；`[measurement]` 用例的数值同样是 test-only 观测。
- 每行 `skipped_tests` 按 CTest 日志原文登记：各行都跳过同一个符号链接用例，共享候选行另有 5 个
  跳过（两个 parse-once 复用用例与三项可执行文件分配注入用例）；跳过名单不代替其余用例的通过，
  也不把它读成"该配置不需要这些用例"。
- 各行总数不可互相比较：注册的用例数随配置变化（静态候选 1416、共享候选 1399、无候选 Debug
  1209、无候选 headless Debug 1064），差额来自该配置注册的用例集合，不表示有用例被漏跑。

## 复现

```powershell
$env:VCPKG_ROOT='D:\vcpkg'
python -B tools/verification/run_rpa_acceptance_matrix.py `
  --vcvars 'D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat' `
  --jobs 4 --repeat-measurements 5
```

复核：

```powershell
python -B out/verification-20261031/evidence-staging/r3-matrix/summarize_matrix.py `
  out/rpa-acceptance/<时间戳>-<uuid8>
```

`summarize_matrix.py` 重读 `manifest.json`、按磁盘字节重算每个阶段日志的 sha256、并从 CTest
日志正文重新推导通过/失败/总数，与运行器记录的 `test_counts` 对照；它把结果压成同目录的
`summary.json`，归档与本报告的数字都取自该文件与 `manifest.json`，不手抄。

# R3 同源本地验收矩阵第四次运行（2026-11-01）

状态：local matrix；test-only；**不是** R3/R4 退出、设备验证、hosted 验证或 owner 接受

日期：2026-11-01

源码：`6256af8b7d349b14a6d5b006d7632711152e96fa`（worktree_sha256 `9fa7831c937b07864b45b30b8dea201fb983beff43e50014d9bd0c7d3e565294`，覆盖 4259 个文件）

返回 [RPA 报告索引](../README.md)。当前阶段状态只见
[CURRENT_STATUS](../../../../../CURRENT_STATUS.md)。
判定证据附件见
[第四次运行证据](2026-11-01-rpa-acceptance-matrix-run4-evidence/index.md)。

## 为什么现在重跑

第三次运行在 `8963889` 上拿到 11/11（13 613 个用例、0 失败）。此后中立路径 CPU 杠杆落在
`6256af8`：`DrawSummary` 不再携带 digest，OpenGL 适配器不再逐帧把 draw command 复制进
`std::vector<PreparedDraw>`。那一批改的是 `engine/` 与 `tests/`，效果由
[中立路径 CPU 杠杆](2026-11-01-r3-neutral-path-cpu-lever.md) 回答；本矩阵回答的是另一件事：**同一批
11 个配置在新源码上是否仍然全部可构建、可跑完整 CTest**，以及用例总数从 13 613 变成 13 635 是否只是
杠杆提交新增的两个用例。

矩阵的用途只有两个：证明每个配置从同一份工作树状态出发可构建、可跑完整 CTest；证明构建输入在校验
期间没有漂移（每行前后重算哈希）。它不产生性能门、不产生 240 FPS 结论、不验证设备、键盘、听音、
设备丢失恢复或 hosted 同 SHA。

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
- **11 棵构建树里有 10 棵在本轮开始时是空的**：2026-11-01 的清理提交（`1474316`）删掉了其中 10 棵，
  只留下 `debug`。所以每一行都是 `--fresh` + `--clean-first` 从空目录重建，没有复用任何旧对象；
  本批总墙钟 9 416 s（2.62 h）高于第三次运行的 8 186 s（2.27 h），差值来自重建而不是用例变慢。
- 磁盘：启动时磁盘下限 12.0 GiB，实际可用 87.1 GiB。

## 结果

全部数字取自同目录 `summary.json`，`summary.json` 由 `summarize_matrix.py` 从 `manifest.json`
与磁盘上的阶段日志重新推导；本报告的表格由 `fill_matrix_report.py` 生成，不手抄。

| 行 | 构建目录 | 状态 | configure | build | ctest | 通过 / 失败 / 总数 | 测量事件 |
| --- | --- | --- | ---: | ---: | ---: | --- | ---: |
| `msvc-on-debug` | `D:\Cuexis-redesign\out\build\candidate-debug` | local_pass | 27.44 s | 321.24 s | 439.32 s | 1418 / 0 / 1418 | 20 |
| `msvc-on-release` | `D:\Cuexis-redesign\out\build\rpa-candidate-release` | local_pass | 19.77 s | 406.89 s | 650.34 s | 1418 / 0 / 1418 | 20 |
| `msvc-on-shared-debug` | `D:\Cuexis-redesign\out\build\rpa-candidate-shared-debug` | local_pass | 23.22 s | 271.99 s | 441.4 s | 1401 / 0 / 1401 | 20 |
| `msvc-off-debug` | `D:\Cuexis-redesign\out\build\debug` | local_pass | 19.59 s | 197.2 s | 358.67 s | 1211 / 0 / 1211 | 0 |
| `msvc-off-headless-debug` | `D:\Cuexis-redesign\out\build\b3-default-off` | local_pass | 18.4 s | 176.27 s | 363.43 s | 1066 / 0 / 1066 | 0 |
| `msvc-off-release` | `D:\Cuexis-redesign\out\build\release` | local_pass | 18.18 s | 295.46 s | 550.14 s | 1211 / 0 / 1211 | 0 |
| `msvc-off-headless-release` | `D:\Cuexis-redesign\out\build\headless-release` | local_pass | 17.84 s | 244.11 s | 559.86 s | 1066 / 0 / 1066 | 0 |
| `msvc-off-shared-debug` | `D:\Cuexis-redesign\out\build\headless-shared-debug` | local_pass | 18.25 s | 181.89 s | 395.48 s | 1064 / 0 / 1064 | 0 |
| `mingw-on-debug` | `D:\Cuexis-redesign\out\build\mingw-headless-debug` | local_pass | 22.5 s | 412.31 s | 665.4 s | 1266 / 0 / 1266 | 20 |
| `mingw-on-shared-debug` | `D:\Cuexis-redesign\out\build\rpa-boundary-mingw-shared` | local_pass | 17.72 s | 350.05 s | 697.69 s | 1248 / 0 / 1248 | 20 |
| `mingw-on-release` | `D:\Cuexis-redesign\out\build\rpa-mingw-release` | local_pass | 17.75 s | 391.82 s | 636.81 s | 1266 / 0 / 1266 | 20 |
| 合计 | | 11 / 11 行 | | | | 13635 / 0 / 13635 | 120 |

11 行中 11 行 `local_pass`，共执行 13635 个用例、失败 0 个，记录 120 次测量事件；清单记录的源码在运行期间未发生改变。

### 这一轮说明了什么

本节只对 `out/rpa-acceptance/20261101-155550-03d71bca` 这一次运行成立，不向其它运行平移。

- **11 行全部 `local_pass`、0 失败**：新源码上第二次完整矩阵（第一次是 `8963889` 上的第三次运行）。
  两次之间唯一的实现改动是中立路径杠杆那一提交，它没有让任何配置的构建或全量用例集失败。
- **13 635 个用例、0 失败**，比第三次运行的 13 613 多 22 个，即每行多 2 个。来源已核到具体用例：
  杠杆提交在 `tests/json_support/serialize_tests.cpp` 新增三个 `TEST_CASE`，其中
  `json::serialize golden capture` 带隐藏标签 `[.serialize-capture]`，不进入默认注册，所以每行只多
  注册两个 —— `json::serialize reproduces the recorded golden bytes` 与
  `json::serialize pretty output is indented by two spaces per level`，两者在每行都执行并通过。
- **6 行记录测量事件、共 120 次，其余 5 行 0 次**：三个候选行与三个 MinGW 行各 20 次（2 个可执行
  文件 × 5 次重复）。这些是 test-only 观测，不构成性能门，也不进入任何吞吐结论。
- **残留失败清单被捕获并排除，而不是记成本轮失败**：`msvc-on-release` 的构建目录里躺着上一次运行留下
  的 `LastTestsFailed.log`（一条 `cuexis_architecture_tests`，29 B）。运行器在本行 CTest 之前把它
  捕获为 `<行名>-previous-LastTestsFailed.log` 再删除，`summary.json` 的 `stale_failure_lists` 逐条
  记录，归档按排除项处理；本行判定以 `-ctest.log.txt` 为准（0 失败）。
- **行内耗时只说明规模**：configure 17.7–27.4 s、build 176.3–412.3 s、ctest 358.7–697.7 s，差值来自
  各配置注册的用例数与编译器，不是性能结论。

矩阵能证明的是两件事：每个配置从同一份工作树状态出发可构建、可跑完整 CTest；以及校验期间构建输入
没有漂移（每行前后重算哈希相等）。它**不能**证明 R3 的性能门、设备路径、听音或 hosted 同 SHA。

### 与 40K @ 240 FPS 结论的关系

矩阵变绿不改变吞吐结论。`40K` 在 1080p 上仍需要 10.71×/13.71×，每实体成本 1.115–1.257 µs 对
0.1042 µs 的预算，差距是结构性的而不是参数性的，见
[40K @ 240 FPS 与预算判定](2026-10-31-r3-40k-240fps-and-budget-verdict.md)。中立路径杠杆把单帧提交
成本压低了约三分之一，那组数字在
[中立路径 CPU 杠杆](2026-11-01-r3-neutral-path-cpu-lever.md) 里，且同样**不是** 40K 达标证据。
本矩阵既没有测 GPU 吞吐也没有测渲染路径；把矩阵读成性能绿灯，是本报告最容易被误用的方向。

### 失败与未覆盖

- 没有失败用例，也没有无判定的行。

运行目录里另有**不属于本轮结果**的残留文件，逐条列在这里而不是列在上面：

- `msvc-on-release`：`msvc-on-release-previous-LastTestsFailed.log`（29 字节，1 条，首条为 `cuexis_architecture_tests`）——本轮 ctest 之前从构建目录捕获的上一次运行清单。

CTest 只在有用例失败时才重写 `Testing/Temporary/LastTestsFailed.log`；该行本轮 0 失败时，构建目录里留下的仍是上一次运行的清单，运行器按原样收进了运行目录。判定以各行的 `-ctest.log` 为准（11 份日志里 `***Failed` 出现 0 次），`summary.json` 把这些文件记在 `stale_failure_lists` 而不是 `failed_tests`，归档时以 `LastTestsFailed.log` 的排除理由登记。

### 跳过的用例

| 用例 | 跳过它的行 |
| --- | --- |
| `Secure file rejects physical containment escapes through symlinks` | 11 行：`msvc-on-debug`（#75）、`msvc-on-release`（#76）、`msvc-on-shared-debug`（#80）、`msvc-off-debug`（#77）、`msvc-off-headless-debug`（#64）、`msvc-off-release`（#76）、`msvc-off-headless-release`（#66）、`msvc-off-shared-debug`（#66）、`mingw-on-debug`（#66）、`mingw-on-shared-debug`（#67）、`mingw-on-release`（#66） |
| `PB-08 owning-copy queries contain allocation failures at the Playback boundary` | 3 行：`msvc-on-shared-debug`（#1296）、`msvc-off-shared-debug`（#1030）、`mingw-on-shared-debug`（#1209） |
| `PD3a extraction failure after install preserves committed state and override lifetime` | 3 行：`msvc-on-shared-debug`（#1298）、`msvc-off-shared-debug`（#1029）、`mingw-on-shared-debug`（#1214） |
| `PD3b materializer allocation failure retains the public extraction error boundary` | 3 行：`msvc-on-shared-debug`（#1300）、`msvc-off-shared-debug`（#1032）、`mingw-on-shared-debug`（#1216） |
| `Playback legacy prepare reuses the initial Chart parse` | 3 行：`msvc-on-shared-debug`（#1265）、`msvc-off-shared-debug`（#933）、`mingw-on-shared-debug`（#1163） |
| `Playback v4 prepare reuses the initial Chart parse` | 3 行：`msvc-on-shared-debug`（#1155）、`msvc-off-shared-debug`（#978）、`mingw-on-shared-debug`（#1103） |

跳过不等于通过：这些用例在对应配置下由 CTest 报为 `***Skipped`，本报告只登记它们是哪些、出现在
哪些行，不推测跳过原因，也不把跳过当作覆盖。

### 构建输入未漂移

`manifest.json` 的 `source.worktree_sha256` 为 `9fa7831c937b07864b45b30b8dea201fb983beff43e50014d9bd0c7d3e565294`，覆盖 4259 个文件；每行在构建
前后各重算一次并要求相等，清单级 `source_unchanged` 为 `true`。同一份清单记录的
`implementation_sha256` 为 `579f034be96d296d26364ecc11ff524459b6b164f4e96a2570bbb68dcf0985d8`，它排除 `docs/` 与 `*.md`，是发布后唯一还能复核的
那个（理由见"边界"最后两条）。

## 未执行项

运行器在 `manifest.json` 中显式登记下列**未执行**项，本报告不把它们当已通过：

- `physical_keyboard`：未接真实键盘：矩阵不发送物理按键，也不验证键盘扫描码到行为的整条链路。
- `listening`：未做听音：没有任何一组由人耳确认音频输出正确。
- `device_loss_recovery`：未做设备丢失与恢复：没有拔插、切换或热失效注入。
- `production_calibration`：未做生产标定：没有以生产夹具或生产时间戳校准任何阈值。
- `hosted_same_sha`：未做同一 SHA 的 hosted 验证：Linux Quality、Windows MSVC、Windows MinGW 的
  hosted 作业都没有在本轮触发。
- `owner_acceptance`：未取得 owner 接受：本报告只登记本机结果。

## 边界

- `test_only: true`、`final_acceptance: false`、`production_thresholds_accepted: false`：
  本次运行采用 test-only 参数，不提升任何生产门限，也不构成阶段接受。
- 不含 GPU 渲染吞吐、显示、VSync、键盘、听音、设备丢失恢复、生产标定与 hosted 同 SHA 验证。
- MinGW 行使用 headless 预设，不覆盖 MinGW 的窗口/GPU 路径。
- 行内耗时是构建机器的观测值，不是性能门；`[measurement]` 用例的数值同样是 test-only 观测。
- 每行 `skipped_tests` 按 CTest 日志原文登记；跳过名单不代替其余用例的通过，也不把它读成
  "该配置不需要这些用例"。
- 各行总数不可互相比较：注册的用例数随配置变化，差额来自该配置注册的用例集合，不表示有用例被漏跑。
- 本次运行的源码状态被记为 `worktree_sha256` `9fa7831c937b07864b45b30b8dea201fb983beff43e50014d9bd0c7d3e565294`（全部 4259 个受检文件）与
  `implementation_sha256` `579f034be96d296d26364ecc11ff524459b6b164f4e96a2570bbb68dcf0985d8`；后者排除 `docs/` 与 `*.md`，是本报告真正要闭合的
  那一个。承载这些字节的提交是 `6256af8`：在该提交上重跑 `identity()`，两个聚合哈希都与运行记录逐字
  相等，两条闭合记录（工作树与全新检出）都归零漂移，见证据目录的 `identity-closure.json` 与
  `identity-closure-fresh-checkout.json`。
- 发布这份报告与附件的提交在其之后，必然改写 `worktree_sha256`（它自己就新增了 `docs/` 下的文件），
  中间还夹着一次只搬移 `docs/stage_reports/` 旧阶段报告的文档整理提交（`a93c979`）。所以**不能**用
  `worktree_sha256` 复核发布后的工作树；复核一律回到 `6256af8`。
- 本批与第三次运行不是同一份源码，两份清单互不覆盖、不互相补行，也不做逐行差分。

## 复现

```powershell
$env:VCPKG_ROOT='D:\vcpkg'
python -B tools/verification/run_rpa_acceptance_matrix.py `
  --vcvars 'D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat' `
  --jobs 4 --repeat-measurements 5
```

复核：

```powershell
python -B out/verification-20261101/evidence-staging/r3-matrix-run4/summarize_matrix.py `
  out/rpa-acceptance/<时间戳>-<uuid8>
```

`summarize_matrix.py` 重读 `manifest.json`、按磁盘字节重算每个阶段日志的 sha256、并从 CTest
日志正文重新推导通过/失败/总数，与运行器记录的 `test_counts` 对照；它把结果压成同目录的
`summary.json`，归档与本报告的数字都取自该文件与 `manifest.json`，不手抄。

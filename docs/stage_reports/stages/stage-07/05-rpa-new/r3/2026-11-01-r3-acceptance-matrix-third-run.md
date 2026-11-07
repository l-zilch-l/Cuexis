# R3 同源本地验收矩阵第三次运行（2026-11-01）

状态：local matrix；test-only；**不是** R3/R4 退出、设备验证、hosted 验证或 owner 接受

日期：2026-11-01

源码：`3fbc31f68430ce2d0ad576094ff617cc09772f10`（worktree_sha256 `df091e437d832afb25ccc68f5204075567dbf5b6fb6c3739d8a21eedfb93b571`，覆盖 3388 个文件，工作树含本次的测量修复）

返回 [RPA 报告索引](../README.md)。当前阶段状态只见
[CURRENT_STATUS](../../../../../CURRENT_STATUS.md)。
判定证据附件见
[第三次运行证据](2026-11-01-rpa-acceptance-matrix-run3-evidence/index.md)。

## 为什么现在重跑

第二次运行（`out/rpa-acceptance/20261101-021741-4daf2bb2`）在同一份源码 `3fbc31f` 上只记录了 10 行：
前 8 行 `local_pass`，第 9 行 `failed_ctest`（51 个用例以 `0xc0000374` 结束），第 10 行在 CTest
阶段撞上磁盘写满而没有留下判定，第 11 行未启动，清单也没有收尾。它暴露的缺陷有两个，都在测量侧：

- **仪器**：`tests/pd/pd_allocation_probe.hpp` 的偏移载荷替换在 MinGW 静态拓扑上会与
  `libstdc++-6.dll` 内的 `operator delete` 相遇，UCRT 堆被写坏。修复是把全局替换的可用性判断收成
  `tests/CMakeLists.txt` 里的一个条件（`MINGW OR SHARED`），在这些拓扑上整个编译掉，只保留显式
  PMR 上游观测。机制、对照矩阵与修复后复跑记录在
  [第二次运行与 MinGW 分配探针极限](2026-11-01-r3-run2-and-mingw-allocation-probe-limit.md)。
- **环境**：磁盘在运行中途耗尽。修复是运行器的 12 GiB 启动下限与 `skipped_disk_space` 行状态：
  宁可拒绝开始，也不再产出“看起来像用例失败”的截断记录。

两处修复都只动测试侧（三处 `tests/**/CMakeLists.txt`、探针头文件、`tests/pd/README.md`）与运行器，
一行未改 `engine/`、`app/`。因此第三次运行要回答的问题很具体：**修好仪器与磁盘下限之后，这 11 行
能否在同一份源码上完整跑完**。第二次运行的任何一行结论都不能平移过来：它不是一次矩阵。

矩阵的用途只有两个：证明每个配置从同一份工作树状态出发可构建、可跑完整 CTest；证明构建输入在
校验期间没有漂移（每行前后重算哈希）。它不产生性能门、不产生 240 FPS 结论、不验证设备、键盘、
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
- 磁盘：启动时磁盘下限 12.0 GiB，实际可用 19.9 GiB。

## 结果

全部数字取自同目录 `summary.json`，`summary.json` 由 `summarize_matrix.py` 从 `manifest.json`
与磁盘上的阶段日志重新推导；本报告的表格由 `fill_matrix_report.py` 生成，不手抄。

| 行 | 构建目录 | 状态 | configure | build | ctest | 通过 / 失败 / 总数 | 测量事件 |
| --- | --- | --- | ---: | ---: | ---: | --- | ---: |
| `msvc-on-debug` | `D:\Cuexis-redesign\out\build\candidate-debug` | local_pass | 20.54 s | 265.76 s | 392.72 s | 1416 / 0 / 1416 | 20 |
| `msvc-on-release` | `D:\Cuexis-redesign\out\build\rpa-candidate-release` | local_pass | 19.25 s | 345.73 s | 556.58 s | 1416 / 0 / 1416 | 20 |
| `msvc-on-shared-debug` | `D:\Cuexis-redesign\out\build\rpa-candidate-shared-debug` | local_pass | 18.39 s | 233.38 s | 403.32 s | 1399 / 0 / 1399 | 20 |
| `msvc-off-debug` | `D:\Cuexis-redesign\out\build\debug` | local_pass | 17.96 s | 184.41 s | 322.05 s | 1209 / 0 / 1209 | 0 |
| `msvc-off-headless-debug` | `D:\Cuexis-redesign\out\build\b3-default-off` | local_pass | 14.64 s | 147.57 s | 320.56 s | 1064 / 0 / 1064 | 0 |
| `msvc-off-release` | `D:\Cuexis-redesign\out\build\release` | local_pass | 17.26 s | 244.29 s | 465.73 s | 1209 / 0 / 1209 | 0 |
| `msvc-off-headless-release` | `D:\Cuexis-redesign\out\build\headless-release` | local_pass | 14.19 s | 205.86 s | 466.2 s | 1064 / 0 / 1064 | 0 |
| `msvc-off-shared-debug` | `D:\Cuexis-redesign\out\build\headless-shared-debug` | local_pass | 14.29 s | 148.23 s | 337.87 s | 1062 / 0 / 1062 | 0 |
| `mingw-on-debug` | `D:\Cuexis-redesign\out\build\mingw-headless-debug` | local_pass | 16.93 s | 362.99 s | 581.83 s | 1264 / 0 / 1264 | 20 |
| `mingw-on-shared-debug` | `D:\Cuexis-redesign\out\build\rpa-boundary-mingw-shared` | local_pass | 14.72 s | 304.32 s | 610.35 s | 1246 / 0 / 1246 | 20 |
| `mingw-on-release` | `D:\Cuexis-redesign\out\build\rpa-mingw-release` | local_pass | 14.11 s | 357.39 s | 573.92 s | 1264 / 0 / 1264 | 20 |
| 合计 | | 11 / 11 行 | | | | 13613 / 0 / 13613 | 120 |

11 行中 11 行 `local_pass`，共执行 13613 个用例、失败 0 个，记录 120 次测量事件；清单记录的源码在运行期间未发生改变。

### 这一轮说明了什么

本节只对 `out/rpa-acceptance/20261101-051925-efdc7b6e` 这一次运行成立，不向其它运行平移。

- **11 行全部 `local_pass`、0 失败**：这是同一份源码上第一次完整的矩阵。第一轮 7/11（一个 `#904`
  音频用例的时序抖动，加上三处 MinGW 构建/源码失败），第二轮前 8 行通过后在第 9 行崩、第 10 行
  撞上磁盘写满、第 11 行未启动。前两轮暴露的两个缺陷都在测量侧，修好之后同一批配置全部走完，
  说明它们来自仪器而不是产品。
- **`mingw-on-debug` 的 51 个 `0xc0000374` 用例这次真正执行并通过，而不是被跳过**：该行本轮跳过
  的用例与其它行是同一组（见下一节），没有任何用例因为探针不可用而被静默跳掉。这一点是本轮最
  需要盯住的地方：把缺陷"修绿"的廉价办法是跳过，这里没有走那条路。
- **MinGW Release 在 `CUEXIS_WARNINGS_AS_ERRORS=ON` 下构建成功**：第一轮这一行止步于构建阶段，
  本轮 `mingw-on-release` 的 configure、build、ctest 三段全部退出码 0。它只说明该配置在本机可
  构建、可跑完 CTest；MinGW 行用的是 headless 预设，不覆盖窗口与 GPU 路径。
- **6 行记录了测量事件，其余 5 行没有**：三个候选行与三个 MinGW 行各 20 次（2 个可执行文件 ×
  5 次重复），非候选/非 MinGW 行 0 次。这些是 test-only 观测，不构成性能门，也不进入任何
  吞吐结论。
- **行内耗时只说明规模**：configure 14–21 s、build 147–363 s、ctest 320–610 s，差值来自各配置
  注册的用例数与编译器，不是性能结论。

矩阵能证明的是两件事：每个配置从同一份工作树状态出发可构建、可跑完整 CTest；以及校验期间构建
输入没有漂移（每行前后重算哈希相等）。它**不能**证明 R3 的性能门、设备路径、听音或 hosted 同 SHA。

### 与 40K @ 240 FPS 结论的关系

矩阵变绿不改变吞吐结论。`40K` 在 1080p 上仍需要 10.71×/13.71×，每实体成本 1.115–1.257 µs 对
0.1042 µs 的预算，差距是结构性的而不是参数性的，见
[40K @ 240 FPS 与预算判定](2026-10-31-r3-40k-240fps-and-budget-verdict.md)。两份结论互不影响：
一份回答"能不能跑完全部测试"，一份回答"40K 能不能 240 FPS"，本矩阵既没有测 GPU 吞吐也没有测
渲染路径。把矩阵读成性能绿灯，是本报告最容易被误用的方向。

### 失败与未覆盖

- 没有失败用例，也没有无判定的行。

运行目录里另有**不属于本轮结果**的残留文件，逐条列在这里而不是列在上面：

- `msvc-on-release`：`msvc-on-release-LastTestsFailed.log`（78 字节，1 条，首条为 `Audio publication preserves device observation time across service polls`）——该行本轮 ctest 日志记录 0 个失败。
- `mingw-on-debug`：`mingw-on-debug-LastTestsFailed.log`（3847 字节，51 条，首条为 `Hash core deep Patterns use charged pages instead of recursion`）——该行本轮 ctest 日志记录 0 个失败。

CTest 只在有用例失败时才重写 `Testing/Temporary/LastTestsFailed.log`；该行本轮 0 失败时，构建目录里留下的仍是上一次运行的清单，运行器按原样收进了运行目录。判定以各行的 `-ctest.log` 为准（11 份日志里 `***Failed` 出现 0 次），`summary.json` 把这些文件记在 `stale_failure_lists` 而不是 `failed_tests`，归档时以 `LastTestsFailed.log` 的排除理由登记。

### 跳过的用例

| 用例 | 跳过它的行 |
| --- | --- |
| `Secure file rejects physical containment escapes through symlinks` | 11 行：`msvc-on-debug`（#78）、`msvc-on-release`（#77）、`msvc-on-shared-debug`（#80）、`msvc-off-debug`（#74）、`msvc-off-headless-debug`（#66）、`msvc-off-release`（#75）、`msvc-off-headless-release`（#62）、`msvc-off-shared-debug`（#67）、`mingw-on-debug`（#62）、`mingw-on-shared-debug`（#64）、`mingw-on-release`（#66） |
| `PB-08 owning-copy queries contain allocation failures at the Playback boundary` | 3 行：`msvc-on-shared-debug`（#1301）、`msvc-off-shared-debug`（#1025）、`mingw-on-shared-debug`（#1210） |
| `PD3a extraction failure after install preserves committed state and override lifetime` | 3 行：`msvc-on-shared-debug`（#1294）、`msvc-off-shared-debug`（#1028）、`mingw-on-shared-debug`（#1214） |
| `PD3b materializer allocation failure retains the public extraction error boundary` | 3 行：`msvc-on-shared-debug`（#1302）、`msvc-off-shared-debug`（#1024）、`mingw-on-shared-debug`（#1211） |
| `Playback legacy prepare reuses the initial Chart parse` | 3 行：`msvc-on-shared-debug`（#1123）、`msvc-off-shared-debug`（#1014）、`mingw-on-shared-debug`（#1072） |
| `Playback v4 prepare reuses the initial Chart parse` | 3 行：`msvc-on-shared-debug`（#1137）、`msvc-off-shared-debug`（#989）、`mingw-on-shared-debug`（#1152） |

跳过不等于通过：这些用例在对应配置下由 CTest 报为 `***Skipped`，本报告只登记它们是哪些、出现在
哪些行，不推测跳过原因，也不把跳过当作覆盖。

### 构建输入未漂移

`manifest.json` 的 `source.worktree_sha256` 为 `df091e437d832afb25ccc68f5204075567dbf5b6fb6c3739d8a21eedfb93b571`，覆盖 3388 个文件；每行在构建
前后各重算一次并要求相等。同一份清单记录的 `implementation_sha256` 为
`9682ef140391fc2820faed2767dfc6e5d5b13a83dfbbacd35d6ea783f8c91ddc`，它排除 `docs/` 与 `*.md`，是发布后唯一还能复核的那个（理由见"边界"最后一
条）。这两个值到提交 `8963889` 的闭合是实测的，不是推断的：在该提交上重跑 `identity()`，3388 个
文件按工作树字节全部相等、两个聚合哈希逐字相等、`docs/` 与 `*.md` 差异 0；附件里的
[identity-closure.json](2026-11-01-rpa-acceptance-matrix-run3-evidence/identity-closure.json) 是这份
记录，[identity-closure-fresh-checkout.json](2026-11-01-rpa-acceptance-matrix-run3-evidence/identity-closure-fresh-checkout.json)
是同一提交在干净检出上的对照（见"边界"最后一条）。

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
- 各行总数不可互相比较：注册的用例数随配置变化，差额来自该配置注册的用例集合，不表示有用例被
  漏跑。
- 本次运行的源码状态含六处测量修复，并在运行开始时被记为 `worktree_sha256` `df091e437d832afb25ccc68f5204075567dbf5b6fb6c3739d8a21eedfb93b571`
  （全部 3388 个受检文件）与 `implementation_sha256` `9682ef140391fc2820faed2767dfc6e5d5b13a83dfbbacd35d6ea783f8c91ddc`；后者排除
  `docs/` 与 `*.md`，是本报告真正要闭合的那一个。承载这些字节的提交是 `8963889`：把本机工作树切到
  该提交后重跑 `identity()`，3388 个文件全部按工作树字节相等，`worktree_sha256` 与
  `implementation_sha256` 都与运行记录逐字相等，`docs/` 与 `*.md` 差异 0
  （[identity-closure.json](2026-11-01-rpa-acceptance-matrix-run3-evidence/identity-closure.json)）。
- 这两个聚合哈希里的"字节"包含每个文件的换行形式，所以它们只在**带同样换行形式的工作树**上逐字
  成立，不是提交本身的性质。同一提交做一次干净检出（`git worktree add --detach`）实测：按内容仍是
  3388/3388、漂移 0，但 2829 个文件字节相等、488 个只在换行形式上不同、4 个本来就是 CRLF、67 个在
  工作树里是混合换行（`core.autocrlf=true` 会把它们统一写成 CRLF），两个聚合哈希于是不同
  （[identity-closure-fresh-checkout.json](2026-11-01-rpa-acceptance-matrix-run3-evidence/identity-closure-fresh-checkout.json)）。
  结论：复核要做在内容层面，逐文件的原始字节由 `manifest.json` 的 `source.files` 与归档里的
  `source-manifest.json` 给出；探针头文件的字节已被固化证据逐文件记录，这也是提交 `8e275a5` 把它钉成
  `text eol=lf` 的原因。发布这份报告与附件的那次提交在 `8963889` 之后，必然改写 `worktree_sha256`
  （它自己就新增了 `docs/` 下的文件），所以不能用 `worktree_sha256` 复核；复核一律回到 `8963889`。
- 运行器在本轮之后又改了**一处只影响副产物**的地方：CTest 之前先把构建目录里上一次遗留的
  `Testing/Temporary/LastTestsFailed.log` 捕获为 `<行>-previous-LastTestsFailed.log` 再删除，并且
  只在该行真的有用例失败时才收 `LastTestsFailed.log`；同一次提交把探针头文件钉成 LF，使清单里的
  逐文件哈希在别的机器上也能重建。这两处都不改变任何构建、任何用例结果或任何行的结论，但会改变
  `implementation_sha256`，所以那个值只对 `8963889` 成立；用现在的运行器复跑会得到同样的 11 行
  结论，且不会再有那两个残留文件。

## 复现

```powershell
$env:VCPKG_ROOT='D:\vcpkg'
python -B tools/verification/run_rpa_acceptance_matrix.py `
  --vcvars 'D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat' `
  --jobs 4 --repeat-measurements 5
```

复核：

```powershell
python -B out/verification-20261031/evidence-staging/r3-matrix-run3/summarize_matrix.py `
  out/rpa-acceptance/<时间戳>-<uuid8>
```

`summarize_matrix.py` 重读 `manifest.json`、按磁盘字节重算每个阶段日志的 sha256、并从 CTest
日志正文重新推导通过/失败/总数，与运行器记录的 `test_counts` 对照；它把结果压成同目录的
`summary.json`，归档与本报告的数字都取自该文件与 `manifest.json`，不手抄。

源码闭合（在 `8963889` 上复核这份运行记录下来的 3388 个文件哈希）：

```powershell
git worktree add --detach out/_identity-8963889 8963889
python -B out/verification-20261031/evidence-staging/r3-matrix-run3/verify_identity_closure.py `
  out/rpa-acceptance/<时间戳>-<uuid8> --root out/_identity-8963889 `
  --head 8963889b66049b31e2eaadc4bc69a329762bd638 --reference D:\Cuexis-redesign
git worktree remove out/_identity-8963889
```

`--root` 是被比对的检出，`--reference` 是带运行当时换行形式的第二份检出：只有它能解释"混合换行"
那一类文件（内容相等、换行不纯）。去掉 `--reference` 时这些文件会被记为漂移；在本机工作树上直接跑
（不带 `--root`）则应当得到 3388/3388 逐字相等与两个聚合哈希相等，即
[identity-closure.json](2026-11-01-rpa-acceptance-matrix-run3-evidence/identity-closure.json) 记录的
那次结果。

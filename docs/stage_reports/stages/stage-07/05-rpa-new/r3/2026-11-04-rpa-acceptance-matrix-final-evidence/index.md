# 最终 R3 同源本地验收矩阵证据（2026-11-04）

状态：local matrix evidence；test-only；**不是** R3/R4 退出、设备验证、hosted 验证或生产接受。

2026-11-04 补记：hosted 口径与 R3 关闭由 owner 同日裁定解决，见
[R3 hosted 口径裁定与批次退出](../2026-11-04-r3-hosted-delta-and-exit-ruling.md)。本文 §4 的
`hosted_same_sha` 未执行记录保持原字节，不回填。

源码：`d70df15a463cb1fe4329bffb8f0c99bcac676c15`

worktree SHA-256：`a3914f7a647d4d3bc19d93d46abe57d35c4761c6afe48fdccce75a7e0513723e`

implementation SHA-256：`0785ea0442e8c4ba4bdffcf4bbbccded12da9d232a9ab2c213a4f6569703da9d`

原始运行目录：
`D:/Cuexis-redesign/out/rpa-acceptance/20261104-050730-f7d80b3f/`

返回 [最终本地验证与 R3c 决定](../2026-11-04-r3-final-validation-and-r3c-decision.md)。当前状态只以
[CURRENT_STATUS](../../../../../../CURRENT_STATUS.md) 为准。

## 0. 结论

11/11 行均为 `local_pass`，0 失败；逐行构建前后源码身份一致，清单级 `source_unchanged=true`。
共执行 **13,635** 个 CTest 用例与 **120** 个候选 `[measurement]` 事件，所有行退出码为 0。
运行期间没有改源码或文档。启动时磁盘可用 **102.21 GiB**，高于 12 GiB 下限；运行器未中止、未补行。

本次只证明同一冻结工作树可在 11 个配置中完成全新 configure/build/CTest。它不产生设备、键盘、
听音、GPU 吞吐、生产标定、hosted 或 owner 接受结论。

## 1. 方法

固定命令：

```powershell
python -B tools/verification/run_rpa_acceptance_matrix.py `
  --vcvars 'D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat' `
  --jobs 4 --repeat-measurements 5
```

每行执行 `--fresh` configure、`--clean-first` build、全量 `ctest --no-tests=error --output-on-failure -j 4`；
候选行把 `cuexis_execution_host_tests [measurement]` 与 `cuexis_rpa_tests [measurement]` 各重复 5 次。
运行器在每行前后及清单末尾重算受检文件哈希。详细命令、日志哈希、二进制哈希与完整文件清单在原始
`manifest.json`；本目录的 [`summary.json`](summary.json) 是其精简、可审阅投影。

## 2. 结果

| 行 | configure (s) | build (s) | CTest (s) | 用例 / 失败 | measurement | skipped |
| --- | ---: | ---: | ---: | --- | ---: | ---: |
| `msvc-on-debug` | 20.05 | 328.47 | 475.29 | 1418 / 0 | 20 | 1 |
| `msvc-on-release` | 18.09 | 435.17 | 676.58 | 1418 / 0 | 20 | 1 |
| `msvc-on-shared-debug` | 18.60 | 310.32 | 491.60 | 1401 / 0 | 20 | 6 |
| `msvc-off-debug` | 18.18 | 234.83 | 400.51 | 1211 / 0 | 0 | 1 |
| `msvc-off-headless-debug` | 18.25 | 202.62 | 395.16 | 1066 / 0 | 0 | 1 |
| `msvc-off-release` | 18.79 | 306.20 | 564.33 | 1211 / 0 | 0 | 1 |
| `msvc-off-headless-release` | 18.05 | 259.36 | 556.62 | 1066 / 0 | 0 | 1 |
| `msvc-off-shared-debug` | 17.73 | 202.10 | 418.73 | 1064 / 0 | 0 | 6 |
| `mingw-on-debug` | 128.93 | 429.45 | 792.42 | 1266 / 0 | 20 | 1 |
| `mingw-on-shared-debug` | 19.86 | 383.73 | 652.20 | 1248 / 0 | 20 | 6 |
| `mingw-on-release` | 19.25 | 449.48 | 627.80 | 1266 / 0 | 20 | 1 |

合计阶段耗时约 10,079 秒（2.80 小时，含全部 configure/build/CTest/measurement，不含人工等待）。
行耗时只用于说明运行成本，不是性能门。

`skipped` 是 CTest 原文的 `***Skipped` 行数，不当作通过。主要重复项包括：
`Secure file rejects physical containment escapes through symlinks` 在全部行跳过；
`Playback legacy/v4 prepare reuses the initial Chart parse`、PD3a/PD3b 与 PB-08 在 SHARED 行按平台/
构建条件跳过。完整原文保留在原始运行的 `*-ctest.log` 与 `*-LastTest.log`。

## 3. 证据与复算

- [`summary.json`](summary.json)：11 行状态、耗时、用例数、skip 原文、源码身份、未执行项与总计。
- [`verification.json`](verification.json)：独立复算的 11 行、13,635 用例、0 失败、120 次 measurement 与
  所有阶段日志 SHA-256；5,052 个文件按归档内容哈希校验，`status=pass`。
- [`source-content-sha256.json`](source-content-sha256.json)：`d70df15a` 的 5,052 个受检文件在
  CRLF/CR 归一化为 LF 后的 SHA-256 清单，用来跨 Windows checkout 的 line-ending 差异复核内容。
- [`identity-closure.json`](identity-closure.json)：运行清单的 source before/after 相同；CRLF checkout 与
  LF checkout 均为 `d70df15a`，5,052 个文件归一化后全部匹配内容清单；CRLF checkout 对运行工作树原始
  SHA-256 的逐字匹配为 4,470/5,052，余差仅为 line ending 变化。
- [`verify_matrix_evidence.py`](verify_matrix_evidence.py)：只读复算器；检查 11 行、0 失败、阶段日志哈希、
  `source_unchanged` 与两个 checkout 的内容清单闭合。
- [`manifest.json`](manifest.json)：本证据目录内文件的字节、SHA-256 与 `content_sha256`。

复算命令：

```powershell
python -B verify_matrix_evidence.py `
  D:/Cuexis-redesign/out/rpa-acceptance/20261104-050730-f7d80b3f/manifest.json `
  --source-root D:/Cuexis-r3matrix-identity `
  --fresh-checkout D:/Cuexis-r3matrix-identity-lf
```

## 4. 未执行与边界

运行器显式登记：

- `physical_keyboard`
- `listening`
- `device_loss_recovery`
- `production_calibration`
- `hosted_same_sha`
- `owner_acceptance`

`test_only=true`、`final_acceptance=false`、`production_thresholds_accepted=false`。MinGW 行使用 headless
预设，不覆盖其窗口/GPU 路径；本矩阵不能替代 hosted 同 SHA，也不能据此宣布 R3 退出。

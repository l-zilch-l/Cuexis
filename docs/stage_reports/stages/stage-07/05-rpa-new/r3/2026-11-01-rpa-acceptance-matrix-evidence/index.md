# 同源本地验收矩阵附件

状态：冻结证据索引。返回 [矩阵报告](../2026-11-01-r3-same-source-acceptance-matrix.md)。

- [运行清单原件](source-manifest.json)：运行器在结束时写出的 `manifest.json` 原样复制
- [复核汇总](summary.json)：由 `summarize_matrix.py` 重新读取日志与清单后压出
- [本目录清单](manifest.json)：每个复制文件的原始路径、字节数、源哈希、归档哈希与换行处理
- 逐行日志：`<行名>-configure.log.txt`、`<行名>-build.log.txt`、`<行名>-ctest.log.txt`
- 有失败用例的行另有 `*-LastTestsFailed.log.txt`（CTest 写出的失败清单）
- 候选行测量日志：`<行名>-<目标>-budget-<N>.log.txt` 与同名 `.cmd.txt`
- [失败用例的孤立诊断](diagnosis.json)：同一失败二进制（按 sha256 绑定）在空闲、并行与带后台负载
  三种工况下的重复结果；它回答“能否复现”，**不**改变该行在本批中的判定

归档是**平铺**的：运行目录的内容按原文件名复制到本目录根下，`.log` / `.cmd` 追加 `.txt` 以免被
当成可执行内容；运行器自己的 `manifest.json` 改名为 `source-manifest.json`，把 `manifest.json`
这个名字留给本目录清单。

## 内容与证明范围

| 文件 | 内容 | 证明什么 |
| --- | --- | --- |
| `*-configure.log.txt` | 每行 `cmake --preset … --fresh` 的完整输出 | 该行在给定预设、选项与 `VCPKG_ROOT` 下的配置结论（成功或失败的原文） |
| `*-build.log.txt` | 每行 `cmake --build … --clean-first --parallel` 的完整输出 | 该行从零构建到底的结论；配置未通过的行没有这一阶段 |
| `*-ctest.log.txt` | 每行 `ctest --no-tests=error --output-on-failure` 的完整输出 | 全量用例被调度、执行并结束；正文含通过/失败/总数、耗时与失败用例名 |
| `*-LastTestsFailed.log.txt` | CTest 写出的失败用例清单 | 该行确实有失败用例，且编号与 `*-ctest.log.txt` 正文一致 |
| `*-budget-<N>.log.txt` | 进入测量阶段的候选行 `[measurement]` 用例第 N 次重复 | 单次通过的旁边有 5 次独立重复；其数值属 test-only 观测 |
| `*-budget-<N>.cmd.txt` | 测量命令包装脚本（含 vcvars 与环境恢复） | 测量命令可复现，环境变量不跨行泄漏 |
| `diagnosis.json` | 失败用例在同一二进制上的孤立重复 | 失败是否可复现、在什么负载下可复现；不构成放宽或推翻本批判定 |
| `source-manifest.json` | 运行器写出的原始清单 | 行顺序、阶段命令、退出码、耗时、日志哈希、`test_counts`、`skipped_tests`、二进制哈希、前后工作树哈希 |
| `summary.json` | 独立复核结果 | 日志哈希与 `test_counts` 被重新计算并逐行对照；有任一问题即退出 1 |

## 主张与支撑对照

每行到底走到哪一步、结论是什么，只以 `summary.json` 的 `rows[].status` 与
`source-manifest.json` 的阶段退出码为准；本索引不重复这些数字，只说明它们在哪个文件里。

| 主张 | 支撑 | 明确不支撑 |
| --- | --- | --- |
| 本批记录的每个结论都能从归档字节重新推导 | 每阶段日志 + `summary.json` 的重新哈希与重新计数 | 不把未执行阶段当通过：配置失败的行没有 CTest 结论 |
| 只有 `local_pass` 的行主张“全新配置、从零构建、全量用例通过” | 该行三个阶段退出码 0 与 `*-ctest.log.txt` 正文 | 其余行不主张构建或 CTest 通过，也不因孤立诊断通过而改判 |
| 构建期间源码未漂移 | 每行 `source_before == source_after`，以及清单级 `source_unchanged` | 只覆盖登记的实现目录；`out/` 与文档不参与该指纹 |
| 二进制与日志一一对应 | 每行按 exe 名记录 sha256，日志按阶段命名 | 不是可复现构建证明（未固定全部环境变量与编译器补丁版本） |
| 进入测量阶段的候选行，其 `[measurement]` 用例重复 5 次 | `*-budget-N.log.txt` 五份，各自独立退出码 0 | 数值不是性能门，也不能与本批其他登记组拼成连续基线 |
| 未进入 CTest 的行是主机能力问题，不是产品缺陷 | 该行 `*-configure.log.txt` 的错误原文 | 不因此把该配置记为通过；也不外推到别的机器 |

## 排除的文件与理由

下列文件在运行目录里存在，但**不**进入归档，`manifest.json` 的 `excluded[]` 逐条记录其字节数与
哈希及原因：

- `*-LastTest.log`：CTest 追加的抓取副本，与已归档的 `-ctest.log` 同文本；判定用的是后者。
  （有失败用例的行，其失败清单 `*-LastTestsFailed.log` **会**归档。）
- `*-CMakeCache.txt`：配置缓存；其 sha256 已按行记为 `cache_sha256`，重复归档只会带来本机绝对路径。

## 读取顺序

1. `summary.json`：逐行结论、总计，以及独立复核是否全部一致。
2. `source-manifest.json`：原始清单，含每阶段命令、退出码、耗时、日志哈希与工作树哈希。
3. 任一行的 `*-ctest.log.txt` 结尾：`N% tests passed`、总用例数与总耗时；失败行另看
   `*-LastTestsFailed.log.txt`。
4. `diagnosis.json`：失败用例在空闲 / 并行 / 带负载下的重复结果与分类。
5. `*-budget-*.log.txt`：`[measurement]` 用例的重复观测。

## 边界

- 本次运行 `test_only: true`、`final_acceptance: false`、`production_thresholds_accepted: false`。
- 未执行：物理键盘、听音、设备丢失恢复、生产标定、hosted 同 SHA、owner 接受。清单的
  `unexecuted[]` 逐项登记，本索引不把它们当已通过。
- MinGW 三行在预设里只覆盖 headless 路径；它们在本机是否可测取决于主机是否提供 MinGW 编译器，
  实际结论见 `summary.json` 的行状态与报告，本索引不预设结果。
- 文本证据在归档时统一为 CRLF，使新检出得到相同字节；`manifest.json` 对每个文件同时记录
  `source_sha256`（工作树原字节）与 `sha256`（归档字节），`eol` 标明处理方式。

# 同源本地验收矩阵第四次运行附件

状态：冻结证据索引。返回
[第四次运行报告](../2026-11-01-r3-acceptance-matrix-fourth-run.md)。

- [运行清单原件](source-manifest.json)：运行器在结束时写出的 `manifest.json` 原样复制
- [复核汇总](summary.json)：由 `summarize_matrix.py` 重新读取日志与清单后压出
- [本目录清单](manifest.json)：每个复制文件的原始路径、字节数、源哈希、归档哈希与换行处理
- 逐行日志：`<行名>-configure.log.txt`、`<行名>-build.log.txt`、`<行名>-ctest.log.txt`
- 候选行测量日志：`<行名>-<目标>-budget-<N>.log.txt` 与同名 `.cmd.txt`
- [闭合记录](identity-closure.json)：在 `6256af8` 上重跑 `identity()`，4259 个文件按工作树字节逐个相等
- [新检出对照](identity-closure-fresh-checkout.json)：同一提交、全新检出的同一比对，给出换行形式差异与
  0 处内容漂移
- [比对脚本](verify_identity_closure.py)：上面两份记录的生产者，`--root` 指定被读的检出、`--reference`
  指定带运行换行形式的第二份检出

归档是**平铺**的：运行目录的内容按原文件名复制到本目录根下，`.log` / `.cmd` 追加 `.txt` 以免被
当成可执行内容；运行器自己的 `manifest.json` 改名为 `source-manifest.json`，把 `manifest.json`
这个名字留给本目录清单。本批没有二进制附件，因此没有 `.gitattributes`。

## 内容与证明范围

| 文件 | 内容 | 证明什么 |
| --- | --- | --- |
| `*-configure.log.txt` | 每行 `cmake --preset … --fresh` 的完整输出 | 该行在给定预设、选项与 `VCPKG_ROOT` 下的配置结论 |
| `*-build.log.txt` | 每行 `cmake --build … --clean-first --parallel 4` 的完整输出 | 该行从**空目录**构建到底的结论 |
| `*-ctest.log.txt` | 每行 `ctest --no-tests=error --output-on-failure -j 4` 的完整输出 | 全量用例被调度、执行并结束；正文含通过/失败/总数、耗时与失败用例名 |
| （本批无此项） | 失败用例清单 | 本批 11 行都是 0 失败，没有本轮产生的清单；残留的旧清单已排除，见排除一节 |
| `*-budget-<N>.log.txt` | 进入测量阶段的候选行 `[measurement]` 用例第 N 次重复 | 单次通过的旁边有 5 次独立重复；其数值属 test-only 观测 |
| `*-budget-<N>.cmd.txt` | 测量命令包装脚本（含 vcvars 与环境恢复） | 测量命令可复现，环境变量不跨行泄漏 |
| `source-manifest.json` | 运行器写出的原始清单 | 行顺序、阶段命令、退出码、耗时、日志哈希、`test_counts`、`skipped_tests`、二进制哈希、前后工作树哈希、磁盘下限与启动可用空间 |
| `summary.json` | 独立复核结果 | 日志哈希与 `test_counts` 被重新计算并逐行对照；有任一问题即退出 1 |

## 主张与支撑对照

每行到底走到哪一步、结论是什么，只以 `summary.json` 的 `rows[].status` 与
`source-manifest.json` 的阶段退出码为准；本索引不重复这些数字，只说明它们在哪个文件里。

| 主张 | 支撑 | 明确不支撑 |
| --- | --- | --- |
| 本批记录的每个结论都能从归档字节重新推导 | 每阶段日志 + `summary.json` 的重新哈希与重新计数 | 不把未执行阶段当通过：没有 CTest 阶段的行没有 CTest 结论 |
| 只有 `local_pass` 的行主张“全新配置、从零构建、全量用例通过” | 该行三个阶段退出码 0 与 `*-ctest.log.txt` 正文 | 其余行不主张构建或 CTest 通过 |
| 构建期间源码未漂移 | 每行 `source_before == source_after`，以及清单级 `source_unchanged` 为 `true` | 只覆盖登记的实现目录；`out/` 与文档不参与该指纹 |
| 二进制与日志一一对应 | 每行按 exe 名记录 sha256，日志按阶段命名 | 不是可复现构建证明（未固定全部环境变量与编译器补丁版本；同一份源码在本机两次重建会得到不同的 exe 哈希） |
| 进入测量阶段的候选行，其 `[measurement]` 用例重复 5 次 | `*-budget-N.log.txt` 五份，各自独立退出码 0；`summary.json` 记 120 次测量事件 | 数值不是性能门，也不能与本批其他登记组拼成连续基线 |
| 这次运行没有因磁盘而截断 | `source-manifest.json` 的 `free_space_floor_bytes`、`free_bytes_at_start` 与 `aborted` | 不保证别的机器或别的时点的可用空间；也不外推到更长的运行 |

## 源码闭合记录

运行清单在运行开始时把 4259 个受检文件的 on-disk 字节各记一个 sha256，压成
`worktree_sha256` `9fa7831c937b07864b45b30b8dea201fb983beff43e50014d9bd0c7d3e565294`；把其中
`docs/` 与 `*.md` 排除后再压成 `implementation_sha256`
`579f034be96d296d26364ecc11ff524459b6b164f4e96a2570bbb68dcf0985d8`。这两个值要闭合到提交
`6256af8`，因此这里保存两份实测记录：

| 记录 | 检出 | 结果 |
| --- | --- | --- |
| [identity-closure.json](identity-closure.json) | 本机工作树，检出到 `6256af8` | 4259 个文件按工作树字节全部相等，两个聚合哈希都与运行记录逐字相等，`docs/` 与 `*.md` 差异 0 |
| [identity-closure-fresh-checkout.json](identity-closure-fresh-checkout.json) | `git worktree add --detach` 的干净检出，同一提交 | 按内容仍是 4259/4259、漂移 0：3702 个文件字节相等、486 个只在换行形式上不同、4 个本来就是 CRLF、67 个在工作树里是混合换行；两个聚合哈希因此不同 |

第二份记录说明了第一份成立的前提：聚合哈希把每个文件的换行形式也算进字节，而
`core.autocrlf=true` 会让干净检出把这些文件统一写成 CRLF。所以 `worktree_sha256` 是**工作树历史**
的哈希，不是提交的哈希；换个机器、换次检出都会不同。复核一律在**内容**层面做：每个非文档文件
要么字节相等，要么只差换行形式，要么（在带同样换行形式的第二份检出作参照时）折掉换行后相等。

两份记录都在 2026-11-01 的文档整理提交（`a93c979`，只搬移 `docs/stage_reports/` 下的旧阶段报告）
之前取得。发布本目录与报告的提交在其之后，必然改写 `worktree_sha256`，所以**不能**用
`worktree_sha256` 复核发布后的工作树；复核一律回到 `6256af8`。

## 排除的文件与理由

下列文件在运行目录里存在，但**不**进入归档，`manifest.json` 的 `excluded[]` 逐条记录其字节数与
哈希及原因：

- `*-LastTest.log`：CTest 追加的抓取副本，与已归档的 `-ctest.log` 同文本；判定用的是后者。
- `msvc-on-release-previous-LastTestsFailed.log`（29 B）：CTest 的失败清单草稿。CTest 只在有用例
  失败时才重写它，而本行本轮 0 失败，所以文件里躺的是同一构建目录里上一次运行留下的清单（一条
  `cuexis_architecture_tests`）。运行器在本轮之前已把它捕获为 `-previous-` 再删除，`summary.json`
  的 `stale_failure_lists` 逐条记录文件名、条数与原因；本行判定以 `-ctest.log.txt` 为准。
- `*-CMakeCache.txt`：配置缓存；其 sha256 已按行记为 `cache_sha256`，重复归档只会带来本机绝对路径。

## 读取顺序

1. `summary.json`：逐行结论、总计（11 行、13 635 个用例、0 失败、120 次测量事件）、磁盘下限与独立
   复核是否全部一致。
2. `source-manifest.json`：原始清单，含每阶段命令、退出码、耗时、日志哈希与工作树哈希。
3. 任一行的 `*-ctest.log.txt` 结尾：`N% tests passed`、总用例数与总耗时。
4. `*-budget-*.log.txt`：`[measurement]` 用例的重复观测。

## 边界

- 本次运行 `test_only: true`、`final_acceptance: false`、`production_thresholds_accepted: false`。
- 未执行：物理键盘、听音、设备丢失恢复、生产标定、hosted 同 SHA、owner 接受。清单的
  `unexecuted[]` 逐项登记，本索引不把它们当已通过。
- MinGW 行在预设里只覆盖 headless 路径；本索引不预设这些行的结果，结论见 `summary.json` 的行状态
  与报告。
- 本批与第三次运行（`20261101-051925-efdc7b6e`）不是同一份源码：那次是 `8963889`，本批是
  `6256af8`（多出中立路径 CPU 杠杆那一提交）。两份清单互不覆盖，也不互相补行；用例总数从 13 613
  变成 13 635，差值的来源见报告。
- 本轮 11 棵构建树里有 10 棵在 2026-11-01 的清理提交（`1474316`）中被删除，因此每一行都是从空
  目录重建；这既解释了本批 2.62 h 的墙钟，也意味着本轮不再有任何旧对象被复用。
- 归档里的文本证据在归档时统一为 CRLF，使新检出得到相同字节；`manifest.json` 对每个文件同时记录
  `source_sha256`（工作树原字节）与 `sha256`（归档字节），`eol` 标明处理方式。这只保证**归档副本**
  可复核；运行当时工作树的聚合哈希仍受工作树换行形式影响，见源码闭合记录一节。

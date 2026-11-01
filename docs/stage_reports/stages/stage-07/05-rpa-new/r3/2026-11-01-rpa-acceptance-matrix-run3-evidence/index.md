# 同源本地验收矩阵第三次运行附件

状态：冻结证据索引。返回
[第三次运行报告](../2026-11-01-r3-acceptance-matrix-third-run.md)。

- [运行清单原件](source-manifest.json)：运行器在结束时写出的 `manifest.json` 原样复制
- [复核汇总](summary.json)：由 `summarize_matrix.py` 重新读取日志与清单后压出
- [本目录清单](manifest.json)：每个复制文件的原始路径、字节数、源哈希、归档哈希与换行处理
- 逐行日志：`<行名>-configure.log.txt`、`<行名>-build.log.txt`、`<行名>-ctest.log.txt`
- 本批没有 `*-LastTestsFailed.log.txt`：两行的构建目录里残留着上一次运行的失败清单，CTest 在本轮
  0 失败时不会重写它，运行器把旧文件收了进来；它不属于本轮结果，按排除项处理
- 候选行测量日志：`<行名>-<目标>-budget-<N>.log.txt` 与同名 `.cmd.txt`
- [关闭记录](identity-closure.json)：在 `8963889` 上重跑 `identity()`，3388 个文件按工作树字节逐个相等
- [新检出对照](identity-closure-fresh-checkout.json)：同一提交、全新检出的同一比对，给出换行形式差异与
  0 处内容漂移
- [比对脚本](verify_identity_closure.py)：上面两份记录的生产者，`--root` 指定被读的检出、`--reference`
  指定带运行换行形式的第二份检出

归档是**平铺**的：运行目录的内容按原文件名复制到本目录根下，`.log` / `.cmd` 追加 `.txt` 以免被
当成可执行内容；运行器自己的 `manifest.json` 改名为 `source-manifest.json`，把 `manifest.json`
这个名字留给本目录清单。

## 内容与证明范围

| 文件 | 内容 | 证明什么 |
| --- | --- | --- |
| `*-configure.log.txt` | 每行 `cmake --preset … --fresh` 的完整输出 | 该行在给定预设、选项与 `VCPKG_ROOT` 下的配置结论（成功或失败的原文） |
| `*-build.log.txt` | 每行 `cmake --build … --clean-first --parallel` 的完整输出 | 该行从零构建到底的结论；配置未通过的行没有这一阶段 |
| `*-ctest.log.txt` | 每行 `ctest --no-tests=error --output-on-failure` 的完整输出 | 全量用例被调度、执行并结束；正文含通过/失败/总数、耗时与失败用例名 |
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
| 构建期间源码未漂移 | 每行 `source_before == source_after`，以及清单级 `source_unchanged` | 只覆盖登记的实现目录；`out/` 与文档不参与该指纹 |
| 二进制与日志一一对应 | 每行按 exe 名记录 sha256，日志按阶段命名 | 不是可复现构建证明（未固定全部环境变量与编译器补丁版本） |
| 进入测量阶段的候选行，其 `[measurement]` 用例重复 5 次 | `*-budget-N.log.txt` 五份，各自独立退出码 0 | 数值不是性能门，也不能与本批其他登记组拼成连续基线 |
| 这次运行没有因磁盘而截断 | `source-manifest.json` 的 `free_space_floor_bytes`、`free_bytes_at_start` 与 `aborted` | 不保证别的机器或别的时点的可用空间；也不外推到更长的运行 |

## 源码闭合记录

运行清单在运行开始时把 3388 个受检文件的 on-disk 字节各记一个 sha256，压成
`worktree_sha256`；把其中 `docs/` 与 `*.md` 排除后再压成 `implementation_sha256`。这两个值要闭合到
提交 `8963889`，因此这里保存两份实测记录：

| 记录 | 检出 | 结果 |
| --- | --- | --- |
| [identity-closure.json](identity-closure.json) | 本机工作树，检出到 `8963889` | 3388 个文件按工作树字节全部相等，`worktree_sha256` 与 `implementation_sha256` 都与运行记录逐字相等，`docs/` 与 `*.md` 差异 0 |
| [identity-closure-fresh-checkout.json](identity-closure-fresh-checkout.json) | `git worktree add --detach` 的干净检出，同一提交 | 按内容仍是 3388/3388、漂移 0：2829 个文件字节相等、488 个只在换行形式上不同、4 个本来就是 CRLF、67 个在工作树里是混合换行；两个聚合哈希因此不同 |

第二份记录说明了第一份成立的前提：聚合哈希把每个文件的换行形式也算进字节，而
`core.autocrlf=true` 会让干净检出把这些文件统一写成 CRLF。所以 `worktree_sha256` 是**工作树历史**
的哈希，不是提交的哈希；换个机器、换次检出都会不同。复核一律在**内容**层面做：每个非文档文件
要么字节相等，要么只差换行形式，要么（在带同样换行形式的第二份检出作参照时）折掉换行后相等。
第一份记录能逐字相等，是因为本机工作树从克隆起就没被整体重写，保留着运行当时的混合换行；这也是
提交 `8e275a5` 把探针头文件钉成 `text eol=lf` 的原因——那份头文件的字节被固化证据逐文件记录，
不能让检出把它变成 CRLF。

## 排除的文件与理由

下列文件在运行目录里存在，但**不**进入归档，`manifest.json` 的 `excluded[]` 逐条记录其字节数与
哈希及原因：

- `*-LastTest.log`：CTest 追加的抓取副本，与已归档的 `-ctest.log` 同文本；判定用的是后者。
- `msvc-on-release-LastTestsFailed.log`、`mingw-on-debug-LastTestsFailed.log`：CTest 的失败清单
  草稿文件。CTest 只在有用例失败时才重写它，而这两行本轮都是 0 失败，所以文件里躺的是同一构建
  目录里上一次运行的清单（`msvc-on-release` 是第一次运行的那条音频用例，`mingw-on-debug` 是第二次
  运行的那 51 个 `0xc0000374` 用例）。它与 `summary.json` 的 `stale_failure_lists` 一一对应，判定
  以该行 `-ctest.log.txt` 为准。运行器已在本轮之后改掉这一采集方式：CTest 之前先把旧文件捕获为
  `<行名>-previous-LastTestsFailed.log` 再删除，且只在该行真的失败时才收 `LastTestsFailed.log`。
- `*-CMakeCache.txt`：配置缓存；其 sha256 已按行记为 `cache_sha256`，重复归档只会带来本机绝对路径。

## 读取顺序

1. `summary.json`：逐行结论、总计、磁盘下限与独立复核是否全部一致。
2. `source-manifest.json`：原始清单，含每阶段命令、退出码、耗时、日志哈希与工作树哈希。
3. 任一行的 `*-ctest.log.txt` 结尾：`N% tests passed`、总用例数与总耗时；本批 11 行都是 0 失败，
   所以没有失败清单可看，残留的旧列表在排除一节。
4. `*-budget-*.log.txt`：`[measurement]` 用例的重复观测。

## 边界

- 本次运行 `test_only: true`、`final_acceptance: false`、`production_thresholds_accepted: false`。
- 未执行：物理键盘、听音、设备丢失恢复、生产标定、hosted 同 SHA、owner 接受。清单的
  `unexecuted[]` 逐项登记，本索引不把它们当已通过。
- MinGW 行在预设里只覆盖 headless 路径；本索引不预设这些行的结果，结论见 `summary.json` 的行状态
  与报告。
- 本批与第二次运行（`20261101-021741-4daf2bb2`）不是同一次运行：那次只有 10 行、没有收尾，且
  测量仪器在 MinGW 静态拓扑上带着缺陷；它的残留日志与机制说明在
  [MinGW 分配探针与第二次运行证据](../2026-11-01-mingw-allocation-probe-evidence/index.md)，
  两份清单互不覆盖，也不互相补行。
- 归档里的文本证据在归档时统一为 CRLF，使新检出得到相同字节；`manifest.json` 对每个文件同时记录
  `source_sha256`（工作树原字节）与 `sha256`（归档字节），`eol` 标明处理方式。这只保证**归档副本**
  可复核；运行当时工作树的聚合哈希仍受工作树换行形式影响，见源码闭合记录一节。

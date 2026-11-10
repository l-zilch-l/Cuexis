# MinGW 分配探针缺陷与第二次矩阵运行附件

状态：冻结证据索引。返回 [诊断报告](../2026-11-01-r3-run2-and-mingw-allocation-probe-limit.md)。

本目录回答几个互不替代的问题，因此分成六组文件：

- `run2/`：第二次本地验收矩阵运行的原文。它把缺陷暴露出来：`mingw-on-debug` 行 51 个用例以
  `0xc0000374` 结束，`mingw-on-shared-debug` 行没有判定，`mingw-on-release` 行没有启动。
- `repro/`：把缺陷缩到三行 Catch2 用例的复现、只替换运算符但保持载荷在原位的对照、不链接任何
  测试框架的隔离程序、四个变体加一个探索项的记录，以及失败行实际编译的那份探针头文件。
- `gdb/`：两份**捕获的**调试器会话记录、驱动它们的带条件断点命令文件，以及绑定这两次会话的记录。
- `isolation/`：四次不经过测试框架的运行——两种链接方式 × 两种释放模式——把“载荷偏移”与
  “C++ 运行时在 DLL 里”拆开。
- `post-fix/`：修复后以全新进程重跑每个被缺陷打坏的用例集，逐份保存 Catch2 汇总行与退出码。
- `scripts/`：重建并重跑上面每一份记录的程序，以及把本次诊断与它所比较的两次矩阵运行绑在一起的
  来源表。

- [本目录清单](manifest.json)：每个复制文件的原始路径、字节数、源哈希、归档哈希与换行处理，
  以及 `excluded[]` 与 `binding`
- [变体记录](repro/variants.json)：五个变体的编译命令、退出码、可执行文件哈希与期望对照
- [调试器记录](gdb/trace-sessions.json)：两次会话的 gdb 版本、断点、命中条件、命中数与被绑定二进制哈希
- [隔离矩阵](isolation/isolation-matrix.json)：四次运行的预期退出码、实测退出码、转录哈希与判定
- [修复后汇总](post-fix/summary.json)：六个用例集全新进程运行的退出码、断言数、用例数与期望对照
- [来源表](scripts/source-table.json)：运行 2 与运行 3 的工作树哈希、逐文件状态，以及六个修复路径
  在两轮运行与提交之间的对应关系
- [运行清单原件](run2/source-manifest.json)：运行器在结束时写出的 `manifest.json` 原样复制

`run2/` 内部保留了原文件名（`.log` / `.cmd` 追加 `.txt`，运行器自己的 `manifest.json` 改名为
`source-manifest.json`，把 `manifest.json` 这个名字留给本目录清单）。其余各组的文件名与暂存时相同。

## 内容与证明范围

| 文件 | 内容 | 证明什么 |
| --- | --- | --- |
| `run2/source-manifest.json` | 第二次矩阵运行的原始清单 | 行顺序、阶段命令、退出码、耗时、日志哈希、`test_counts`、前后工作树哈希；含未收尾的 `source_unchanged` |
| `run2/*-ctest.log.txt` | 每行 `ctest --output-on-failure` 完整输出 | 行 9 的 51 个失败用例正文、行 10 的 6 个 `No space left on device` 失败，都是原文 |
| `run2/*-LastTestsFailed.log.txt` | CTest 写出的失败用例清单 | 失败编号与 ctest 正文一致，不靠手抄 |
| `repro/tiny-repro.cpp` | 8 行复现：只包含探针头、分配一个 `std::string`、断言长度 | 缺陷不需要产品代码参与即可出现 |
| `repro/control-passthrough-new.cpp` | 同样替换运算符，但载荷留在原位 | 把“替换运算符”与“载荷偏移”分开 |
| `repro/isolation-no-framework.cpp` | 同一份头文件、不链 Catch2、两种释放模式写在同一个程序里 | 框架、DLL 加载与断言库都不参与时，崩溃仍然只取决于释放路径 |
| `scripts/record_variants.py` | 重建五个变体、逐个运行、与期望退出码对照 | 五个退出码可被任何人重新算出来；脚本在任何一个不符时退出 1 |
| `repro/variants.json` | 上述脚本的输出 | 编译标志、编译器版本、头文件哈希、每个可执行文件哈希与退出码 |
| `scripts/record_dll_free_trace.py` | 用 gdb 在 `main` 之后断 `free`、带内存条件抓两次会话 | 两份 `gdb/` 记录可被重新跑出来 |
| `gdb/payload-free.cmd` | 上述脚本写出的 gdb 命令文件 | 断点位置、条件与输出格式逐字可见，不靠描述 |
| `scripts/record_isolation_matrix.py` | 重建两个隔离程序、四次运行、与期望退出码对照 | `isolation/` 的四次运行可被重新跑出来 |
| `scripts/record_postfix_verification.py` | 六个用例集全新进程运行、解析 Catch2 汇总行 | `post-fix/` 的退出码与计数可被重新跑出来 |
| `scripts/record_source_table.py` | 逐文件比对两轮运行、提交与今天的工作树 | 来源表里每个哈希都由脚本算出来，脚本在没有差异或关系不符时退出 1 |
| `scripts/fill_probe_report.py` | 把上列 JSON 填进报告表格的脚本 | 报告里的表格不手抄；脚本在任何一个期望不符时拒绝填表 |
| `repro/pd_allocation_probe.as-run-2.hpp` | 失败行实际编译的头文件字节（4481 B，`e8c9aaa7…`） | 复现绑定的是当时那份探针，不是修好之后的版本 |
| `gdb/dll-free-of-offset-payload.txt` | 动态运行时下的 `free` 会话记录 | 命中时指针前 16 字节是 `{base, size}` 自指对，返回地址在 `~unique_ptr` 内部的 `call *%rdx` 之后 |
| `gdb/static-runtime-clean-free.txt` | 静态运行时的同一断点会话记录 | 同一断点下命中数为 0：写坏的字节不再经过 DLL 的 `operator delete` |
| `isolation/*-roundtrip.txt`、`isolation/*-mismatch.txt` | 四次运行的完整输出 | 两种运行时在 `roundtrip` 下都退出 0，在 `mismatch` 下都 `0xc0000374` |
| `post-fix/*.log` | 修复后六个用例集的全新进程输出 | 每个用例集退出码 0 且断言数、用例数与期望一致 |

## 主张与支撑对照

| 主张 | 支撑 | 明确不支撑 |
| --- | --- | --- |
| 行 9 的 51 个失败是**测试仪器**缺陷，不是产品缺陷 | `repro/variants.json` 的五变体分离 + `gdb/` 两份会话记录 + `post-fix/mingw-gameplay-packed.log` | 不主张产品在 MinGW 下没有别的缺陷，也不主张 MinGW 的窗口/GPU 路径已被覆盖 |
| 在测试框架里，缺陷同时需要“偏移载荷”与“C++ 运行时在 DLL 里” | `dynamic-runtime` 非零退出、`static-runtime` / `resource-only` / `passthrough-operators` 零退出；`gdb/` 两份记录的命中数 1/0 | 不把 MSVC 的通过读成“MSVC 更安全”的证据，只读成“该拓扑未暴露” |
| 更基本地说，决定成败的是“载荷遇到一条绕过替换的释放路径”；运行时只是这种路径是否存在的代理 | `isolation/` 四次运行：`roundtrip` 两种运行时都 0，`mismatch` 两种运行时都崩 | 不主张 DLL 的 `operator delete` 是在**某一条具体指令上**被调用的——那由 `gdb/` 的返回地址负责，二者不可互相替代 |
| 今天的探针头文件在动态运行时下**仍然**会崩 | `repro/variants.json` 的 `current-header-dynamic` 行（探索项）+ 该行可执行文件哈希 | 不把这一行读成“加固无效就该撤销”；它的方向是接收侧，而出事的是发送侧 |
| 修复只改测试仪器，产品二进制不受影响 | `manifest.json` 的 `binding.gate_files` 六个路径，产品目标不在变更集内 | 不是可复现构建证明，也没有重跑产品性能 |
| 修好之后被打坏的用例集恢复通过 | `post-fix/summary.json` 六条记录，逐条断言数对照 | 不把单进程通过读成矩阵结论；矩阵结论只能由一次完整运行给出 |
| 第二次运行不是矩阵结论 | `run2/source-manifest.json` 的行状态与缺项：`mingw-on-shared-debug` 无状态、`mingw-on-release` 未启动、无 `summary.json` | 不把 8 行 `local_pass` 读成“矩阵全绿”，也不把未完成行读成“通过” |

## 与两轮矩阵运行的关系

第二次运行出事的六个路径，在第三次运行里逐字带着同样的修复，第三次运行因此是
11/11 `local_pass`、13 613 个用例零失败。`scripts/source-table.json` 把这件事写成可核对的对应关系：

- `run2.head` 是 `3fbc31f6…`，`run2.states` 为 3388 个未修改、0 个修改、0 个新增——**第二次运行
  是一份干净检出**，不是带未提交修改的运行。这一点与本目录早期的错误说法相反，报告里已更正。
- `run3.head` 同为 `3fbc31f6…`，`run3.states` 为 3382 个未修改、6 个修改，六个修改就是那六个修复路径。
- 六个路径的 `run_equals_later` 全为真：第三次运行磁盘上的字节等于随后提交 `8963889` 的 blob，
  其中五个只差换行（`blob-crlf`），探针头文件逐字节相同（`blob`）。

## 排除的文件与理由

下列文件存在但**不**进入归档，`manifest.json` 的 `excluded[]` 逐条记录字节数、哈希与原因：

- `*.exe` / `*.obj` / `*.pyc`：0.4–86 MB 的静态链接 Catch2 调试二进制、中间对象与字节码缓存；
  全部可由 `scripts/` 里的记录脚本与登记的命令重建。
- `gdb-*.txt`：第一轮诊断用的 gdb **命令文件**，当时的会话输出没有落盘；归档改用重新捕获的
  `gdb/*.txt`，避免把没有输出的命令文件当成记录。
- `probe*.cpp`、`probe-*.cmd.txt`、`tiny*.exe`、`plain.exe`、`sha-noreplace.exe`、`guard.exe`、
  `freehook.exe`、`*.ps1`：第一轮诊断的临时件与取命令行用的驱动；已被 `repro/`、`scripts/` 与
  记录里的 `command` 数组取代。
- `*/as-run-include/pd_allocation_probe.hpp`：as-run 头文件的暂存副本，正文已作为
  `repro/pd_allocation_probe.as-run-2.hpp` 归档，并被两份 JSON 以 sha256 绑定。
- `repro/pd_allocation_probe.magic-check-intermediate.hpp`：诊断中途的一版头文件（改成泄漏而不是
  释放），从未被测量，已被提交版本取代。
- 本目录所附的报告本身：它发布在 `docs/stage_reports/stages/stage-07/05-rpa-new/r3/` 下并指向本目录，
  本目录不复制它。
- `run2/*-LastTest.log`、`run2/*-CMakeCache.txt`：CTest 抓取副本与配置缓存，正文与已归档文件
  相同、缓存哈希已在清单里。

## 读取顺序

1. `run2/source-manifest.json`：先确认第二次运行到底走到哪一行、哪一行没有判定。
2. `run2/mingw-on-debug-ctest.log.txt`：`0xc0000374` 的原文与出现次数。
3. `gdb/trace-sessions.json` 与 `gdb/dll-free-of-offset-payload.txt`：崩在哪条释放路径上。
4. `isolation/isolation-matrix.json`：把框架拿掉之后，结论是否还成立。
5. `repro/variants.json` 与 `repro/pd_allocation_probe.as-run-2.hpp`：五个变体、失败行编译的头文件字节。
6. `post-fix/summary.json`：修复后六个用例集的断言数对照。
7. `scripts/source-table.json`：这次诊断与两轮矩阵运行的来源对应。

## 边界

- 本归档 `test_only`：它不构成 R3/R4 退出、设备验证、hosted 验证或 owner 接受，也不提升任何
  生产门限。
- 归档里没有产品行为证据：它只说明测试仪器在哪些拓扑下会把正确的产品判成失败。
- 在编译掉替换的拓扑（MinGW、共享产品库）上，全量 `operator new` 普查不可用；这些行的
  `allocationScope` 是 `explicit-pmr-upstream`，因此**不得**据本归档宣称任何全量分配普查。
- 文本证据统一为 CRLF，使新检出得到相同字节；`manifest.json` 对每个文件同时记录
  `source_sha256`（原字节）与 `sha256`（归档字节），`eol` 标明处理方式。

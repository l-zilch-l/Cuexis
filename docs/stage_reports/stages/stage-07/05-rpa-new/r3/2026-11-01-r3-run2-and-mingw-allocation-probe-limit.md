# R3 第二次同源矩阵尝试与 MinGW 分配探针极限（2026-11-01）

状态：local diagnostic；test-only；**不是** R3/R4 退出、矩阵通过、hosted 验证或 owner 接受

日期：2026-11-01

源码：提交 `3fbc31f68430ce2d0ad576094ff617cc09772f10` 本身，工作树干净（见
[本次运行的源码](#本次运行的源码)）。运行目录里的 `source.worktree_sha256` 为
`cfd039e8a8bc4e458d137a08cde8dc7a38c5528433b4eb73f439077337833035`，覆盖 3388 个受检文件；
其中去掉 `docs/` 与 `*.md` 的 `source.implementation_sha256` 为
`2294be30ee2733045e0afad56724c8da3f50e29da037bce6aac2e3de5594ba0f`。
崩溃现场的探针头文件字节就是该提交的那一份：sha256
`e8c9aaa7896fd5d538a94ba0d734988f0de51a83035f7e30fd843505ce6f284f`，4481 字节。

返回 [RPA 报告索引](../README.md)。当前阶段状态只见
[CURRENT_STATUS](../../../../../CURRENT_STATUS.md)。
判定证据附件见 [MinGW 分配探针与第二次运行证据](2026-11-01-mingw-allocation-probe-evidence/index.md)。

## 摘要

这是 2026-11-01 的第二次同源矩阵尝试（运行目录
`out/rpa-acceptance/20261101-021741-4daf2bb2`）。它的价值不在于“又跑了一次”，而在于它在两个
独立位置暴露了**测试仪器与测试环境**的缺陷，且这两处都不是产品缺陷：

- 11 行里只记录了 10 行：前 8 行 `local_pass`，第 9 行 `failed_ctest`（51 个用例，退出码
  `0xc0000374`），第 10 行在 CTest 阶段撞上磁盘写满而没有留下判定，第 11 行未启动。清单没有收尾
  （`source_unchanged` 缺失）。因此这次运行**不是一次矩阵**，不替代第一次运行的结论，也不能读成
  “8/11 通过”。
- 第 9 行的 51 个失败用例，每一个在崩溃前都已经打印 `All tests passed (N assertions in 1 test
  case)`，随后进程以 `0xc0000374`（`STATUS_HEAP_CORRUPTION`）结束。这不是断言失败，而是用例结束后
  的堆调用崩溃。
- 根因是测量仪器：`tests/pd/pd_allocation_probe.hpp` 把载荷放在块首之后 16 字节（块首写
  `{base, size}`），并替换全局 `operator new/delete`。MinGW 的 C++ 运行时来自
  `libstdc++-6.dll`：`Catch::Detail::unique_ptr<T>` 的析构是一次 `call *%rdx`——虚拟删除析构函数
  调度；当 `T` 是 `std::ostringstream` 这种由 DLL 提供实现的类时，那个删除析构函数住在 DLL 里，用它
  自己的 `operator delete` 释放本 exe 分配的偏移载荷，UCRT 堆因此被写坏。三组独立证据——调试器
  记录、无框架对照、变体矩阵——见 [机制](#机制)。
- 仓库侧的配置缺陷把这个仪器编译进了两个本不该有它的拓扑：`tests/gameplay_packed/CMakeLists.txt`
  只对 `SHARED` 关掉替换，`tests/pd/CMakeLists.txt` 完全没关。修复把它收成
  `tests/CMakeLists.txt` 里的一个条件（`MINGW OR SHARED`），不改产品代码。
- 修复后同一套用例在本机复跑全绿（MinGW `cuexis_gameplay_packed_tests` 5643 断言 / 51 用例，退出 0；
  另两套探针与两个 MSVC 目标同样退出 0），见 [修复后的复跑](#修复后的复跑)。
- 第二个缺口是磁盘：第 10 行丢失的 6 个用例全在安装/消费者路径，CTest 连自己的
  `LastTestsDisabled.log` 都写不出来。运行器因此加入 12 GiB 启动下限与 `skipped_disk_space` 行状态，
  宁可拒绝开始，也不再产出“看起来像用例失败”的截断记录。

产品侧的结论只有一句：在这两个拓扑上，把全局替换编译掉之后同一套用例全绿，所以**不因为这次崩溃
改动产品代码**。同时，任何“全局分配普查”在这些拓扑上都不可用，只能登记为
`explicit-pmr-upstream` 范围，不能声称覆盖全局分配。

## 这次运行走到了哪里

运行目录 `out/rpa-acceptance/20261101-021741-4daf2bb2`，`manifest.json` 里 `rows` 共 10 条。

| 行 | 预设 | 状态 | configure | build | ctest | 通过 / 失败 / 总数 |
| --- | --- | --- | ---: | ---: | ---: | --- |
| `msvc-on-debug` | `candidate-debug` | local_pass | 50.63 s | 281.66 s | 405.82 s | 1416 / 0 / 1416 |
| `msvc-on-release` | `release` | local_pass | 19.12 s | 365.38 s | 629.00 s | 1416 / 0 / 1416 |
| `msvc-on-shared-debug` | `candidate-shared-debug` | local_pass | 22.29 s | 256.22 s | 431.35 s | 1399 / 0 / 1399 |
| `msvc-off-debug` | `debug` | local_pass | 21.91 s | 206.82 s | 347.12 s | 1209 / 0 / 1209 |
| `msvc-off-headless-debug` | `headless-debug` | local_pass | 17.87 s | 164.10 s | 342.01 s | 1064 / 0 / 1064 |
| `msvc-off-release` | `release` | local_pass | 18.56 s | 269.43 s | 514.52 s | 1209 / 0 / 1209 |
| `msvc-off-headless-release` | `headless-release` | local_pass | 15.61 s | 223.78 s | 515.24 s | 1064 / 0 / 1064 |
| `msvc-off-shared-debug` | `headless-shared-debug` | local_pass | 17.87 s | 163.86 s | 366.51 s | 1062 / 0 / 1062 |
| `mingw-on-debug` | `mingw-headless-debug` | failed_ctest | 18.42 s | 382.65 s | 783.54 s | 1264 / 51 / 1264 |
| `mingw-on-shared-debug` | `mingw-headless-debug` | 无判定 | 15.76 s | 331.20 s | — | — |

这 10 行覆盖的拓扑分别是：MSVC 静态候选 Debug / Release、MSVC 共享候选 Debug、MSVC 无候选
Debug / Release、两个 headless 变体、一个 headless 共享变体，以及两个 MinGW 静态候选行。没有到来的
第 11 行是 MinGW Release（`rpa-mingw-release`，该构建目录至今是一个 0.16 GB 的空壳）。

三条必须写明的缺口：

- **第 10 行没有判定。** 它的构建成功了（331.20 s，退出 0），CTest 跑完了 1246 个用例，但
  `manifest.json` 里没有该行的 ctest 阶段记录，`status` 为 `None`。原因见
  [第 10 行：磁盘写满](#第-10-行磁盘写满)。
- **第 11 行从未开始。** `rows` 里没有它，运行目录里也没有 `mingw-on-release-*` 文件。
- **清单没有收尾。** `source_unchanged` 为 `None`，`summary.json` 从未生成；目录里共 136 个文件、
  29,035,935 字节，是运行被中断后的原始残留。

因此这次运行既不能计入“全绿”，也不能用来回答“当前源码是否通过全套用例”。它回答的是另外两个问题：
MinGW 静态候选拓扑为什么在用例全通过之后崩溃，以及这台机器在什么条件下会让一次矩阵运行失去判定。

## 行 9：51 个用例先打印“全部通过”再堆损坏

`mingw-on-debug-ctest.log`（337,439 字节）里：

- `96% tests passed, 51 tests failed out of 1264`，ctest 退出码 8。
- `0xc0000374` 出现 **102** 次（每个失败用例打印两次：一次在 `***Exception` 行、一次在退出码行），
  `***Exception:` 行 **51** 条。
- `All tests passed (N assertions in 1 test case)` 出现 **48** 次——也就是说 51 个失败用例里有 48 个
  在崩溃前已经完整跑完并报告通过，另外 3 个在打印汇总之前就崩了。
- `mingw-on-debug-LastTestsFailed.log` 恰好 51 条，编号 `#900`–`#950` 连续，全部属于
  `gameplay_packed` 这一套（例如
  `900:Hash core deep Patterns use charged pages instead of recursion`、
  `902:Revision4 omits only physical projections and retains complete Gameplay2`、
  `950:Compiled Graph typed SAX roundtrip and table permutations preserve full Capsule`）。
- 这些失败用例的正文里带着 `scope=global-cpp-allocations`。这个字符串是
  `tests/gameplay_packed/hash_core_tests.cpp:19` 在**没有**定义
  `CUEXIS_PD_ALLOCATION_PROBE_RESOURCE_ONLY` 时才会出现的编译期常量，所以日志本身就是“全局替换
  被编译进了这一行”的直接证据。

`hash-observation …` 行只在这 24 个用例里出现，是因为 CTest 只在用例失败时才转储标准输出；MSVC 那
8 行的日志里没有这些行，不等于没有观测。

把两件事放在一起：用例本身跑完了、断言全过、退出码是堆损坏。这类签名排除了“产品逻辑算错”，指向
“释放路径上有指针不该被交给堆”。

## 机制

### 仪器做了什么

`tests/pd/pd_allocation_probe.hpp` 在自己的 `operator new` 里多申请 16 字节，把
`struct Header { void* base; std::size_t size; }` 放在块首，返回 `header + 1`：

```cpp
auto* base = std::malloc(size + sizeof(Header) + alignment);
const auto address = (reinterpret_cast<std::uintptr_t>(base) + sizeof(Header) + alignment - 1) &
                     ~(static_cast<std::uintptr_t>(alignment) - 1);
auto* result = reinterpret_cast<void*>(address);
auto* header = static_cast<Header*>(result) - 1;
*header = {base, size};
```

`alignment` 至少是 `alignof(std::max_align_t)`，而 `sizeof(Header)` 是 16，所以
`malloc` 返回的 16 字节对齐块之后，载荷落在 `base + 16`。`operator delete` 收到的是这个偏移指针，
退 16 字节就能拿到头。这套做法在同一条分配/释放链路上是自洽的，前提是**每个** `delete` 都回到同一个
替换实现。

### 一次 `call *%rdx` 把载荷交给了 DLL

MinGW 默认动态链接 `libstdc++-6.dll`。Catch2 的 `Catch::Detail::unique_ptr<T>` 不是裸 `delete`：
它的析构是 `delete m_ptr`（`catch_unique_ptr.hpp:60`），而 `T` 在这些用例里是
`std::__cxx11::basic_ostringstream<…>` 这类**实现住在 DLL 里**的类。于是析构被编译成一次虚拟删除
析构函数调度——`call *%rdx`——被调用的那份代码在 DLL 里，它收尾时用的是**DLL 自己的**
`operator delete`。偏移载荷就这样被送进一个不认这个头的释放器，UCRT 堆被就地写坏，崩溃发生在之后
的某次堆调用里——这正是“断言全过、退出码是 `0xc0000374`”的形状。

`msvc` 不受影响，因为 MSVC 的 C++ 运行时是 exe 里的静态库：同一处 `call` 落到本 exe 的删除析构
函数，收尾仍走被替换的 `operator delete`。第 1–8 行因此通过。

`tests/pd/README.md` 把这条极限写进了文档：偏移载荷的替换实现在 `MINGW OR SHARED` 拓扑上必须编译
掉，改为显式 PMR 上游；在这些拓扑上不存在全局分配普查。

### 调试器记录

`gdb/` 下的两份记录用同一个断点、同一个命中条件对比两种链接方式：

调试器为 `GNU gdb (GDB) 17.2`。两次运行都用 `archive-src/tiny-repro.cpp`（sha256 `022dac3b63037db4430fea3226f6a9e425e6202924197bb33dba9bf8a1fb04a9`），编译时用的是 as-run 头文件（sha256 `e8c9aaa7896fd5d538a94ba0d734988f0de51a83035f7e30fd843505ce6f284f`，4481 字节）。断点打在 CRT 导出的 `free` 上——在这个 CRT 里 `_free_base` 不是一个符号，而且在共享库加载之前下断点只会解析到 exe 里一个还没被填充的死 thunk，所以断点是在 `main` 之后下的。命中条件只读内存：

```text
$rcx > 0x10000 && *(unsigned long long*)($rcx - 16) == $rcx - 16
```

也就是“被释放的指针前面 16 字节，正好是这个头文件写给自己的 `{base, size}` 自指对”。这样命中就只能是替换实现的载荷，不可能是某个块的真实块首。

- `dynamic-runtime.exe`（sha256 `9f74cadd4b69410cd6dc11ad2747f50771cd0dea7c1c9d31b6d9ca10481c7985`，20814385 字节）：`free` 断点命中 **1** 次（应当出现）；不带条件的同一个断点也命中 1 次，说明它确实解析到了实现而不是死 thunk；`libstdc++-6.dll` 在模块表；不挂调试器直接运行退出 `0xc0000374`。记录：`gdb/dll-free-of-offset-payload.txt`。
  - 断点输出：`PAYLOAD_FREE 1 ptr=0x7928a0 wm16=0x0000000000792890 wm8=0x0000000000000178 probe_hit=0 ret=0x7ff6e8c514f7`
  - 断点输出：`PAYLOAD_FREE_HITS 1`
  - 返回地址属于：`Catch::Detail::unique_ptr<std::__cxx11::basic_ostringstream<char, std::char_traits<char>, std::allocator<char> > >::~unique_ptr() + 39`
  - 该处的指令：

```text
0x7ff6e8c514eb <_ZN5Catch6Detail10unique_ptrINSt7__cxx1119basic_ostringstreamIcSt11char_traitsIcESaIcEEEED1Ev+27>:	add    $0x8,%rdx
0x7ff6e8c514ef <_ZN5Catch6Detail10unique_ptrINSt7__cxx1119basic_ostringstreamIcSt11char_traitsIcESaIcEEEED1Ev+31>:	mov    (%rdx),%rdx
0x7ff6e8c514f2 <_ZN5Catch6Detail10unique_ptrINSt7__cxx1119basic_ostringstreamIcSt11char_traitsIcESaIcEEEED1Ev+34>:	mov    %rax,%rcx
0x7ff6e8c514f5 <_ZN5Catch6Detail10unique_ptrINSt7__cxx1119basic_ostringstreamIcSt11char_traitsIcESaIcEEEED1Ev+37>:	call   *%rdx
0x7ff6e8c514f7 <_ZN5Catch6Detail10unique_ptrINSt7__cxx1119basic_ostringstreamIcSt11char_traitsIcESaIcEEEED1Ev+39>:	nop
```

  同一份记录里，进程在这次释放之后以 `Critical error detected c0000374` 结束，即崩溃发生在这条释放路径之后，而不是断言失败。

- `static-runtime.exe`（sha256 `0a0b418fc1869619fa0b38c1d99695fae3ee0f3201704240beb6289e95acb44e`，23548799 字节）：`free` 断点命中 **0** 次（应当为零）；不带条件的同一个断点也命中 1 次，说明它确实解析到了实现而不是死 thunk；`libstdc++-6.dll` 不在模块表；不挂调试器直接运行退出 `0x00000000`。记录：`gdb/static-runtime-clean-free.txt`。
  - 断点输出：`PAYLOAD_FREE_HITS 0`

这份记录里最关键的是返回地址：它不在替换实现的 `release` 里，而在 `~unique_ptr` 内部——恰好是
`call *%rdx` 的下一条指令。也就是说，把载荷交给别人的不是某个漏改的 `delete` 调用点，而是这次间接
调用本身：调用目标由运行时的虚表决定，编译期根本看不见。

### 无框架对照

调试器记录的是真实用例，但真实用例同时带着 Catch2、DLL、文件系统负载。为了把“偏移载荷”和“DLL
运行时”拆开，另有一个不链接任何测试框架的对照程序（`archive-src/isolation-no-framework.cpp`），
只用同一份 as-run 头文件，两种链接方式 × 两种场景共 4 次运行：

| 运行时 | 模式 | 场景 | 预期 | 实测 | 判定 |
| --- | --- | --- | --- | --- | --- |
| `dynamic-runtime` | `roundtrip` | DLL 运行时，全部释放都回到替换 | `0x00000000` | `0x00000000` | 符合 |
| `dynamic-runtime` | `mismatch` | DLL 运行时，多一次 `new` + `std::free` | `0xc0000374` | `0xc0000374` | 符合 |
| `static-runtime` | `roundtrip` | 静态运行时，全部释放都回到替换 | `0x00000000` | `0x00000000` | 符合 |
| `static-runtime` | `mismatch` | 静态运行时，多一次 `new` + `std::free` | `0xc0000374` | `0xc0000374` | 符合 |

`roundtrip` 让每一次 `new` 都经由替换实现的 `delete`，`mismatch` 在同样一段代码里多出**一次**
`new` + `std::free` 的直接配对。四种组合的读法很干脆：

- 只要每次释放都回到替换，两种运行时都不崩——**偏移载荷本身**不是问题。
- 只要出现一次不经过替换的释放，两种运行时都崩——**DLL 运行时本身**也不是问题。

也就是说，崩溃的充分条件既不是“载荷偏移”也不是“DLL”，而是“偏移载荷遇到了一条绕过替换的释放
路径”。静态运行时把这种路径的出现概率降到零（这个程序里它一次都没有），动态运行时因为删除析构
函数住在 DLL 里而必然出现——前面那份 gdb 记录抓到的就是它。

### 对照矩阵

四个真正的对照加一个探索项，全部在同一个编译器、同一份 Catch2 静态库、同一台机器上重新编译，
其中前四个使用**当初出事的头文件字节**（`e8c9aaa7896fd5d538a94ba0d734988f0de51a83035f7e30fd843505ce6f284f`，
4481 字节，就是 `3fbc31f` 里那一份），而不是今天树里的版本
（`92c7313ff424add52bb28e077e7010dd0cb55698b0058048c23125438f641d8f`，6297 字节）。
之所以必须这样，是因为今天树里的头文件已经改过：它给块首加了魔数、把外来 `delete` 交回
`std::free`、并补上了 `nothrow` 与对齐重载，用今天的字节复现不出当初的崩溃。

| 变体 | 头文件 | 附加开关 | 预期 | 实测 | 判定 |
| --- | --- | --- | --- | --- | --- |
| `dynamic-runtime`（偏移载荷 + DLL 内 C++ 运行时） | as-run `e8c9aaa7`（4481 B） | — | `0xc0000374` | `0xc0000374` | 符合 |
| `static-runtime`（偏移载荷 + 静态 C++ 运行时） | as-run `e8c9aaa7`（4481 B） | `-static-libstdc++` `-static-libgcc` | `0x00000000` | `0x00000000` | 符合 |
| `resource-only`（替换编译掉，只留显式 PMR 上游） | as-run `e8c9aaa7`（4481 B） | `-DCUEXIS_PD_ALLOCATION_PROBE_RESOURCE_ONLY=1` | `0x00000000` | `0x00000000` | 符合 |
| `passthrough-operators`（替换在块首，无偏移） | 无探针头文件 | — | `0x00000000` | `0x00000000` | 符合 |
| `current-header-dynamic`（今天的头文件） | current `92c7313f`（6297 B） | — | `（无预测）` | `0xc0000374` | 仅记录 |

这四行合起来说明：崩溃同时需要**偏移载荷的替换实现**与**DLL 里的 C++ 运行时**。缺任何一个，同一
个用例就退出 0。第五行是探索项：今天的头文件在同样的动态运行时下表现如何，它只登记、不作预测，也
不参与任何判定——但它登记的事实很重要：加魔数与外来释放处理**并没有**让今天的头文件在动态运行时
下变安全，它依旧 `0xc0000374`。原因是方向反了：那处加固处理的是“外来的块进到本 exe 的
`operator delete`”，而出事的路径是“本 exe 的载荷被别人的 `free` 释放”，替换实现根本没有被调用，
接收侧再怎么加固也拦不住。所以安全的做法不是“把头文件加固到能容忍”，而是
[修复后的复跑](#修复后的复跑)里那条：在装不了全局替换的拓扑上把它整个编译掉。

## 仓库侧的缺陷：仪器被编译进了两个不该有的拓扑

出事时仓库里有两处独立的条件，而且**都不对**：

| 文件 | 出事时的写法 | 后果 |
| --- | --- | --- |
| `tests/gameplay_packed/CMakeLists.txt:3` | `if(CUEXIS_LIBRARY_TYPE STREQUAL "SHARED")` 才定义 `RESOURCE_ONLY` | MinGW 静态候选行仍然装全局替换 |
| `tests/pd/CMakeLists.txt` | 在 `STATIC` 分支里无条件定义 `CUEXIS_PD_ALLOCATION_OBSERVER=1`，从不定义 `RESOURCE_ONLY` | 探针目标在 MinGW 静态拓扑上装全局替换 |

两处各写一半，所以“MinGW 静态”这个组合两边都漏。修复不是再加一个例外，而是把判断收到一处：

- `tests/CMakeLists.txt` 在 `find_package(Catch2 …)` 之后设一个变量：
  `if(MINGW OR CUEXIS_LIBRARY_TYPE STREQUAL "SHARED") set(CUEXIS_PD_ALLOCATION_PROBE_RESOURCE_ONLY ON)`，
  并把“为什么是这两个拓扑”的依据写在旁边（DLL 内运行时、跨模块所有权）。
- `tests/pd/CMakeLists.txt` 与 `tests/gameplay_packed/CMakeLists.txt` 都只读这个变量决定
  `RESOURCE_ONLY` 还是 `OBSERVER`，不再各自判断库类型。

结果是：在全局替换不可用的拓扑上，探针**整个编译掉**，而不是“装了但假装没装”。这也意味着这些
拓扑上不存在全局分配普查，任何普查结论只能登记为 `explicit-pmr-upstream` 范围。

## 修复后的复跑

修复只改测试侧的 CMake 与探针头文件，不触碰产品代码。判据是“同一套用例在修复后的构建里退出 0 且
断言/用例数与预期一致”，六个目标逐个在修复后重新构建并运行：

| 目标 | 构建目录 | 退出码 | 断言 | 用例 | 判定 |
| --- | --- | ---: | ---: | ---: | --- |
| `mingw-gameplay-packed` | `mingw-headless-debug` | 0 | 5643 | 51 | 符合 |
| `mingw-allocation-probe` | `mingw-headless-debug` | 0 | 16414 | 2 | 符合 |
| `mingw-baseline-probe` | `mingw-headless-debug` | 0 | 16844 | 6 | 符合 |
| `msvc-debug-gameplay-packed` | `debug` | 0 | 5641 | 51 | 符合 |
| `msvc-candidate-allocation-probe` | `candidate-debug` | 0 | 16428 | 4 | 符合 |
| `msvc-candidate-gameplay-packed` | `candidate-debug` | 0 | 5641 | 51 | 符合 |

这些复跑是定位性验证，不是矩阵：它们从修复后的构建树里直接取可执行文件运行，不重新执行
`cmake --fresh` + `--clean-first`，因此不能替代矩阵行，也不构成 R3/R4 退出的证据。

## 第 10 行：磁盘写满

`mingw-on-shared-debug-ctest.log` 里 `99% tests passed, 6 tests failed out of 1246`，缺失的 6 个
用例全在安装与消费者路径：

- `#8 cuexis_external_consumer_find_package`（76.01 s）
- `#9 cuexis_external_consumer_add_subdirectory_playback`（9.03 s）
- `#10 …_add_subdirectory_core`（0.11 s）
- `#11 …_add_subdirectory_audio_sdl`（0.12 s）
- `#12 cuexis_gameplay_installed_consumer`（0.17 s）
- `#13 cuexis_reference_host_staging`（0.10 s）

日志里出现 12 次 `No space left on device`，最后几行是 CTest 自己写不出记录文件：

```text
CMake Error: Cannot open file for write: D:/Cuexis-redesign/out/build/rpa-boundary-mingw-shared/Testing/Temporary/LastTestsDisabled.log.tmp
CMake Error: : System Error: No space left on device
Cannot create log file: LastTestsDisabled.log
Errors while running CTest
```

运行器随后在把该行产物复制回运行目录时也失败，于是既没有 ctest 阶段记录，也没有清单收尾。

这不是用例失败，也不是产品问题：磁盘在运行中途耗尽。当时的处置与永久修复：

- 当时：删除 `D:\vcpkg\buildtrees` 与未被引用的 `out/verification-20261028/consumers`，回收 16.70 GB。
- 永久：`tools/verification/run_rpa_acceptance_matrix.py` 增加 `FREE_SPACE_FLOOR = 12 GiB`，启动时
  低于下限直接拒绝开始（退出 2），每行开始前再检查一次，低于下限则把该行记为
  `skipped_disk_space`、写入 `manifest["aborted"]`、保存并终止整次运行。清单里新增
  `free_space_floor_bytes` 与 `free_bytes_at_start`，每行新增 `free_bytes_at_start`。
- 收尾逻辑改为无条件写入 `source_after` 与 `source_unchanged`，只有“源码未漂移且每行
  `local_pass`”才返回 0。

第二次运行的 `free_space_floor_bytes` 字段不存在，正是因为它跑在这次修复之前。

## 本次运行的源码

第二次运行的 `head` 是 `3fbc31f68430ce2d0ad576094ff617cc09772f10`，工作树**干净**：运行目录的
`source.files` 记了 3388 个文件，`record_source_table.py` 逐个比对出
`unmodified 3388 / modified 0 / added 0`（三种形态：`blob` 1634、`blob-crlf` 1687、
`worktree-eol` 67——后两种只是行尾差异，仓库是 CRLF 落盘）。所以这一轮的源码不需要“重建”：它就是
那个提交，`git checkout 3fbc31f` 出来的字节逐文件相同。

闭合值也随之确定：

| 字段 | 值 |
| --- | --- |
| `head` | `3fbc31f68430ce2d0ad576094ff617cc09772f10` |
| `source.worktree_sha256` | `cfd039e8a8bc4e458d137a08cde8dc7a38c5528433b4eb73f439077337833035` |
| `source.implementation_sha256` | `2294be30ee2733045e0afad56724c8da3f50e29da037bce6aac2e3de5594ba0f` |
| 受检文件数 | 3388 |

**这六个测量修复不在这一轮里。** 它们是在定位完成之后才做的，做完整套后才有第三次运行
（`out/rpa-acceptance/20261101-051925-efdc7b6e`，11 行全 `local_pass`，`source_unchanged: true`），
随后以 `8963889` 提交。第三次运行记录的六个路径，逐个都等于该提交的 blob（其中五个是同一 blob 的
CRLF 落盘形态，头文件逐字节相同）：

| 文件 | 第三次运行记录（落盘 sha256） | `8963889` blob |
| --- | --- | --- |
| `tests/CMakeLists.txt` | `90fb929cbf9f2503d770d94dd56557bc3442eeb43cbe50c54778b8a2a772f1a8` | `1bb1562c78f4b2b8dd1b44684e4da8b4819849f4bb04d71d07423a1fc1580412` |
| `tests/gameplay_packed/CMakeLists.txt` | `ee599b1f8166672f0958ea9696a221de8537787b4e35dffe704de63a28643b59` | `b715f9e8637f55bcfc90f66c899b2ee524cdec1a19014d3e04ab098964deaaa5` |
| `tests/pd/CMakeLists.txt` | `9584cd7a476cb5f5a9bf417079c17f1384f474f6b924515778e19b2caee90503` | `fe4f19d146381815771b77e0bb7eb0356fad4a7908acf0a008576194a4539c51` |
| `tests/pd/README.md` | `d455357e08c42007a67e5828df1db495c3e93e9eeb7f4cc3172b96d219205843` | `d740dcd1a462f5dcd80d90a74c785eee6b312298151e2d1b1e94935233f38cb7` |
| `tests/pd/pd_allocation_probe.hpp` | `92c7313ff424add52bb28e077e7010dd0cb55698b0058048c23125438f641d8f` | `92c7313ff424add52bb28e077e7010dd0cb55698b0058048c23125438f641d8f` |
| `tools/verification/run_rpa_acceptance_matrix.py` | `a90d5d965442a1dda8bcf1e02c6ca646b87ff5e8227cf2d9600621dd2a259cd8` | `6fb1a5a99de231023d16367761f162a785ca3d223b33b1c7b8336f006aa132a6` |

三次运行的源码关系因此是：

| 运行 | 目录 | 源码 | `implementation_sha256` |
| --- | --- | --- | --- |
| 第二次 | `20261101-021741-4daf2bb2` | `3fbc31f`，干净 | `2294be30…` |
| 第三次 | `20261101-051925-efdc7b6e` | `3fbc31f` + 上表六个文件 | `9682ef14…` |
| 今天 | 工作树 | `8963889` 之后只加了文档与运行器收尾 | —— |

闭合时要注意**用哪个**哈希：`worktree_sha256` 覆盖全部 3388 个受检文件，而发布这份报告的那次提交
自己就往 `docs/` 里新增了文件，所以提交后 `worktree_sha256` 必然不同——它不能当凭据。
`implementation_sha256` 排除 `docs/` 与 `*.md`，是唯一能跨发布提交闭合的值。第一次发布之后，
`implementation_sha256` 又会因为 `tools/` 下的运行器收尾而继续变化；那种变化属于“仪器事后整理”，
它的差别必须在报告里点名，不能假装没发生。

### 一处更正

本报告的早期草稿把上表六个文件写成“第二次运行时未提交的改动”，并把第二次运行的
`worktree_sha256` / `implementation_sha256` 写成了 `df091e43…` / `9682ef14…`。那是第三次运行的值。
第二次运行的 `manifest.json` 里这两个字段是 `cfd039e8…` / `2294be30…`，且 `record_source_table.py`
在它记录的 3388 个文件里找不到任何一处改动。以 `manifest.json` 与逐文件哈希为准，草稿那段作废。

## 证据与复现

证据归档：[MinGW 分配探针与第二次运行证据](2026-11-01-mingw-allocation-probe-evidence/index.md)，
内含六组：出事运行的行级日志与清单（`run2/`）、最小复现源与变体记录（`repro/`）、调试器记录
（`gdb/`）、无框架对照（`isolation/`）、修复后复跑记录（`post-fix/`）、可原样重放的记录脚本与
来源表（`scripts/`），以及把六组绑定到同一份头文件字节与同一组退出码的 `manifest.json`。

五个记录脚本都可以从零重放，但要在工作副本里运行：脚本把输出写到自己所在目录，在归档目录里
直接跑会把新结果写进冻结证据。

```powershell
$work = "out/verification-<日期>/evidence-staging/r3-mingw-probe"
# 先把 2026-11-01-mingw-allocation-probe-evidence/scripts/*.py 复制进 $work，再逐条运行：
python -B $work/record_variants.py             # 写 $work/variants.json
python -B $work/record_dll_free_trace.py       # 写 $work/gdb/
python -B $work/record_isolation_matrix.py     # 写 $work/isolation/
python -B $work/record_postfix_verification.py # 写 $work/post-fix/
python -B $work/record_source_table.py         # 写 $work/source-table.json（只读重算）
```

前三个脚本使用 `C:\msys64\ucrt64\bin\c++.exe`（GCC 16.1.0）与
`out/build/mingw-headless-debug/vcpkg_installed/x64-mingw-static` 下的 Catch2 静态调试库；第四个直接
运行各构建树里已有的测试可执行文件；第五个只读，重算两轮运行与两个提交之间的逐文件对应关系。
前三个脚本都会先从 `git show 3fbc31f:tests/pd/…` 取出 as-run 头文件字节、断言它的 sha256 等于清单
里记录的值，再用这份字节编译——不是用今天树里的版本。每个脚本在任何一个期望不符时退出 1，
因此“重放得到同样的记录”这件事可以被任何人自己验证。
归档不收录 20–24 MB 的 MinGW 可执行文件，只记录它们的字节数与 sha256。

## 边界

- 本报告是定位记录，不是矩阵：11 行只记录了 10 行，第 10 行无判定，第 11 行未开始，清单未收尾。
- 本报告不构成 R3/R4 退出、阶段接受、生产标定、hosted 同 SHA 验证或 owner 接受。
- 不含 GPU 渲染吞吐、显示、VSync、键盘、听音、设备丢失恢复。
- MinGW 行使用 headless 预设，不覆盖 MinGW 的窗口/GPU 路径。
- 修复只改测试侧：三处 CMake、探针头文件、`tests/pd/README.md` 与运行器的磁盘下限。产品代码
  （`engine/`、`app/`）一行未改，因此本报告不主张任何产品行为变化。
- “修复后复跑全绿”只说明这套用例不再被仪器带崩；它不证明这些用例在 MinGW 上覆盖了并发、
  窗口或 GPU 路径这些本轮没有执行的部分。
- 在这些拓扑上全局分配普查不可用；`scope=explicit-pmr-upstream` 的观测只覆盖显式 PMR 上游，
  不能读成进程全局分配总量。

# Stage 6 复核修正：R7 C4 门禁与分发补齐

状态：completed
快照日期：2026-09-28
更新日期：2026-09-28

本报告是 [Stage 6 复核修正计划](../../../stage_plans/reviews/stage-06-review-remediation/plan.md)
批次 **W5（R7）** 的退出记录。它逐条处置
[Spec 轴](2026-09-28-spec.md) 的 SPEC-27 / SPEC-28 / SPEC-30 与
[Standards 轴](2026-09-28-standards.md) 的 STD-12。

**修正原则**：不改写任何历史报告的现象与证据原文；所有新增断言都以**实际变异代码站点后测试必须失败**
的方式反证其非空转，反证记录见 §3。**未注册为门禁的项不写成通过**，一律登记为残余并给出归属。

## 1. 批次边界

- 起始 SHA：`05f9a20`（W4 批次 HEAD）；本批次工作分支的第一个提交是 R0 裁定记录。
- 涉及路径：`cmake/VerifyReferenceHost.cmake`、`CMakeLists.txt`、本报告与索引。
- 未触碰：ADR 0042 冻结决策正文（见 §5 的 ADR 冲突登记）、Stage 7A/8/9–12 任何交付物、
  提案 1（Chart v1–v3 退出）、`examples/reference_host/` 的源码与 CMake。
- **未新增公共 API、未改 SDK API 版本（仍为 `0.7.0`）**，未改任何 golden。

## 2. 逐条处置

| 计划步骤 | 复核 ID | 现象（原文摘要） | 本批次处置 | 证据 |
| --- | --- | --- | --- | --- |
| 步骤 1 | SPEC-27 | 宿主符号级检查未接线；机制（`VerifySharedExports`/`VerifySharedConsumerImports`）已有但不用于宿主 | **已补齐**：宿主门禁新增导入表检查，拒绝宿主可执行文件导入**内部模块**（9 个名称的禁用表，非允许集；措辞更正见 §4.2 末）；shared 下另要求确实导入了 Playback | `cmake/VerifyReferenceHost.cmake`；`CMakeLists.txt` |
| 步骤 2 | SPEC-27 | 「错误 SDK minor 拒绝」仅手动核对，未注册 | **已补齐**：新增负例，以 `CUEXIS_HOST_API_VERSION=0.8.0` 配置宿主并要求失败文本 | 同上 |
| 步骤 3 | SPEC-27 | 工具链拒绝路径仅 shared | **登记口径**（不改实现）：见 §2.1 | 本报告 §2.1 |
| 步骤 4 | SPEC-27 | 交互命令循环口径未定；ADR 0042 `:350-351` 与实现不符 | **登记口径 + ADR 冲突上报**：见 §2.2 | 本报告 §2.2 |
| 步骤 5 | SPEC-28 | candidate 隔离是手动过程 | **已补齐一半**：安装树 candidate 标识零命中扫描成为门禁；「默认 OFF 下候选工厂拒绝」**本已注册**，见 §2.3 | `cmake/VerifyReferenceHost.cmake`；`tests/playback/playback_candidate_tests.cpp` |
| 步骤 6 | SPEC-30 | 分发门禁仅 Windows | **登记口径 + Stage 8 归属**：见 §2.4 | 本报告 §2.4 |
| 附加 | STD-12 | `ENV{PATH}` 恢复可被 `FATAL_ERROR` 跳过 | **已修正**：恢复点紧邻唯一需要净化 PATH 的 `execute_process` | `cmake/VerifyReferenceHost.cmake` |

### 2.1 步骤 3：static flavor 的口径

shared 的「错误工具链拒绝」用例（`VerifyReferenceHost.cmake` 的 SHARED 分支）有真实根因：
`CuexisConfig.cmake.in` 的整段兼容性检查位于 `if(Cuexis_LIBRARY_TYPE STREQUAL "SHARED")` 之内，
static 安装包的配置**本身不带** toolchain 门禁。门禁此前的 `STATUS` 提示是诚实表述，不是假装通过。

**裁定**：不为 static 发明等价负例。理由——static 包没有 toolchain 门禁这一**事实本身**是当前设计，
为它造负例只能测到「某个不存在的东西不存在」，不产生保护价值。改为**由步骤 2 的新负例承担
static 的拒绝面**：SDK minor 拒绝来自安装包自身的 `SameMinorVersion` 版本文件，与 flavor 无关，
因此在 static 下真实执行（见 §3 反证 D）。

### 2.2 步骤 4：交互命令循环口径，以及 ADR 0042 的冲突（需 owner 处置）

**实现事实**：`examples/reference_host/src/main.cpp` 只解析 argv，全目录**无任何 stdin 读取**
（`git grep -n "std::cin\|getline\|stdin" -- examples/reference_host/` 仅命中 `README.md` 的说明句
与三处 `#include <cstdint>`）。`README.md:58-62` 已把「脚本式宿主」记为本阶段口径并已指向 R7。

**不能改实现的原因**：`tools/check_stage6_a2.py` 的 `check_reference_host_contract()`（注册为
CTest `cuexis_contract_s6_a2`）已经把**脚本驱动面**冻结为契约（必需 flag 集与 report stage 集）。
实现一个真正的 REPL 会**破坏这条既有门禁**，属于计划 §4 明确排除的「为更彻底重写已通过的实现」。

**未决冲突（本批次如实上报，未处置）**：[ADR 0042](../../../adr/0042-stage-6-productization-boundaries.md)
`:350-351` 写「自己拥有命令循环…；支持 open/play/pause/seek/reload/quit」，
而实现没有 play/pause/quit 命令。ADR 正文属冻结决策，计划 §4 规定**不得由实现者自行修改**，
本批次因此**不改 ADR**。恢复条件：owner 裁定二者之一——(a) 按实现把 ADR `:350-351` 的记忆化命令
列表订正为「脚本式命令序列」，(b) 在 Stage 8 授权补 play/pause/quit 并同步放开 A2 契约。
在裁定前，本批次只记录事实，不把该分歧写成已解决。

### 2.3 步骤 5 的两半：一半已存在，一半本次补齐

- 「默认 OFF 下候选工厂拒绝」**本批次之前就已注册且已在所有预设/CI 中运行**，无需新写：
  `tests/playback/playback_candidate_tests.cpp` 的用例
  `"Candidate factories stay disabled and do not read their inputs"`（tag `[playback][candidate][source]`），
  在 `#ifndef CUEXIS_ENABLE_CHART_V5_CANDIDATE` 下断言三个工厂均返回
  `playback.candidate.disabled`。经 `catch_discover_tests` 自动注册。
  复核记录把该项列为「手动过程」是对**零命中扫描**而言，不是对工厂断言而言；本报告澄清这一区分。
- 「安装树零命中扫描」本次成为门禁（扫描暂存 prefix 的 `*.hpp` / `*.cmake` / `*.txt`）。
  **扫描五个具体 token 而非子串 `candidate`**：安装的公共头**合法包含**
  `PresentationCandidateToken` 与 `CandidateMetadataAccess`（S5-C 已接受的展示面），
  子串扫描会在**正确**的树上失败。此点在实现前用真实安装树验证过（§3 反证 E）。

### 2.4 步骤 6：分发门禁的平台范围

`cuexis_player_distribution` 在 CMake 里**并非** Windows 锁定，它由 `if(CUEXIS_BUILD_PLAYER)` 保护；
Linux 上不注册的原因是 `CMakePresets.json` 的 `headless-*` 族把 `CUEXIS_BUILD_PLAYER` 设为 `OFF`。
因此诚实表述是「Linux 预设不构建 Player，故分发门禁不在 Linux 注册」，而非「门禁仅支持 Windows」。

**处置**：只登记口径与归属，不改预设（改预设＝扩大 CI 面，属计划 §4 排除的「重做 CI 平台矩阵」）。
Stage 8 归属见 §5。

## 3. 反证记录（每条新增断言必须能被真实变异推翻）

| # | 断言 | 变异方式 | 结果 |
| --- | --- | --- | --- |
| A | 宿主导入表无内部 Cuexis 库 | 用合成的导入列表（含 `cuexis_runtime-0.7d.dll`）喂给门禁的禁止清单逻辑 | **失败**：报出 `The reference host imports the internal Cuexis library cuexis_runtime-0.7d.dll`；移除该项后通过 |
| B | shared 宿主必须导入 `cuexis_playback` | 用不含 `cuexis_playback` 的导入列表 | 逻辑要求命中 `^cuexis_playback`；static 分支不启用（static 无导入库） |
| C | SDK minor 拒绝负例 | 以 `0.8.0` 真实配置宿主 | **失败**：`Could not find a configuration file for package "Cuexis" that is compatible with requested version "0.8.0"`；断言文本取自该真实输出 |
| D | minor 拒绝与 flavor 无关 | 在 **static**（`debug` 预设）下执行门禁 | 真实执行并报 `Reference host refused an incompatible SDK minor` |
| E | candidate 零命中扫描 | (i) 真实安装树扫描 39 个文件 | (i) **通过**（零命中）|
| E | 同上 | (ii) 向一个已安装 `*.hpp` 追加 `CUEXIS_ENABLE_CHART_V5_CANDIDATE` | (ii) **失败**并指出文件名；还原后再次通过 |
| E | 同上 | (iii) 前缀不存在（空扫描集） | (iii) **失败**（`found no installed files`），防止空转通过 |

**方法论记录（重要）**：步骤 2 的负例**最初写错了断言文本**。按 `examples/reference_host/CMakeLists.txt:28-31`
的显式守卫，文本应是 `The reference host supports SDK API 0.7.x; found ...`；但实际配置显示该守卫
**对 `0.8.0` 不可达**——安装包的 `SameMinorVersion` 版本文件在 `find_package` 内先行中止。
若照守卫措辞写断言，这条用例将**永远不匹配**，即在门禁里长期空转。
本批次据实改为断言真实输出。**教训**：「读代码推断错误文本」不能替代「实跑一次取错误文本」。

## 4. 门禁与命令输出

| 命令 | 结果 |
| --- | --- |
| `ctest --preset debug -R cuexis_reference_host_staging`（STATIC） | `100% tests passed, 0 tests failed out of 1`（8.44 s） |
| 同上，shared（`debug` + `CUEXIS_LIBRARY_TYPE=SHARED`） | `100% tests passed, 0 tests failed out of 1`（8.93 s） |
| 门禁 verbose 输出（static） | `Reference host import surface verified`、`Reference host refused an incompatible SDK minor` |
| 门禁 verbose 输出（shared） | 上述两条 + `Reference host refused a foreign-toolchain package` |
| `python -B tools/check_docs.py` | `Documentation checks passed` |
| `git diff --check` | 通过（无空白错误） |

### 4.1 本机环境说明（不影响本批次结论）

本机 `C:\Windows\system32\bash.exe` 是 **WSL 启动器**，不向子进程传递 Windows 环境变量，
因此 `tools/check_version_gate_tests.py` 的 bootstrap 用例在本机**报错**。
**这不是本批次或 `05f9a20` 的缺陷**：`usable_posix_shell()` 正是为拒绝此类 shim 而设计，
而该函数只检查「shell 能跑 `exit 0`」与「shell 能解析 `git`」——WSL 启动器两者都通过，
于是走进了正例分支。以原生 bash（`C:\msys64\usr\bin` 前置到 PATH）复跑为 **19 tests OK**。
因该差异只存在于本机沙箱，本批次**不改**该探测逻辑；hosted 上 MSYS2 环境不存在 WSL shim。

## 4.2 新增门禁在 hosted 上连续暴露的三个缺陷（均已修复）

本批次新增/改动的宿主导入表检查在推送后**连续三次在 hosted 上判失败**，
三次都是**新增门禁自身的缺陷**，不是被测代码的问题，也不是环境问题。
按发现顺序记录如下；三次都由门禁自己的反空转守卫或既有闸门暴露，而非人工审查发现。

### 缺陷 1：`nm` 读不出导入表（MinGW）

首次推送后 **hosted Windows MinGW** 判失败：

```
The host import inspection parsed no libraries from .../cuexis_reference_host.exe;
the check would pass vacuously
```

根因：非 MSVC 分支选了 `nm`，而 **`nm` 读不了 PE 的导入表**。本机直接验证：
对该 `.exe` 运行 `nm -D -undefined-only` 与 `nm -D --dynamic` 均返回 `no symbols`，
且旧正则匹配的是 `.so` 名，PE 镜像里根本不含。

### 缺陷 2：同一根因在 Linux 上同样成立（ELF）

随后 **hosted Linux（`GCC Coverage` 与 `GCC Adapter Coverage`）** 报出**同样的**
反空转消息，只是路径变成 `.../host-build/cuexis_reference_host`。

根因与缺陷 1 相同：`nm -D` 在 ELF 上输出的是**符号**
（`libc_start_main@GLIBC_2.34`），**从不输出库文件名**；依赖关系位于 `DT_NEEDED`。
本机以真实 ELF 复核：

| 方式 | 解析结果 | 守卫判定 |
| --- | --- | --- |
| 旧：`nm -D` + 库名正则 | 库名匹配 **0** 行（只得到符号） | 失败 |
| 新：`objdump -p` + `NEEDED` 正则 | `libc.so.6` 等真实依赖 | 通过 |

**这恰是本批次 §3 反空转守卫设计要拦的失败模式**：若没有该守卫，这项检查会在
"什么都没解析到"的情况下于**两个平台上都报成功**，且**长期静默**。

### 缺陷 3：共享变量被挪用，跨闸门打坏 `cuexis_shared_export_surface`（Linux）

缺陷 1/2 的修正把 `CUEXIS_SYMBOL_TOOL` 从 `nm` 改成了 `objdump`，但**该变量有两个消费者**，
而它们需要不同的工具。Linux CI 立刻抓到（`GCC Shared Release` 与 `Clang Shared Debug`）：

```
cuexis_shared_export_surface
CMake Error at cmake/VerifySharedExports.cmake:21 (message):
  Shared symbol inspection failed: /usr/bin/objdump: unrecognized option
  '--defined-only'
```

导出闸门读的是**动态符号表**，`nm -D --defined-only` 才是它的正确读取器；
`objdump` 不认识该选项。这是**我在修正缺陷 1/2 时引入的回归**，且**未登记在任何报告中**。

### 缺陷 4：ELF 的 `lib` 前缀让导入检查在 Linux 上既空转又误判

同一批 Linux 作业还暴露了导入检查本身更深的一处错误。ELF soname 带 `lib` 前缀与
`.so` 版本链，CI 打印出的真实导入表是：

```
libcuexis_playback-0.7.so.0.7;libcuexis_content-0.7.so.0.7;libcuexis_core-0.7.so.0.7;
libstdc++.so.6;libgcc_s.so.1;libc.so.6
```

两处判断同时失效：

| 判断 | 旧写法 | 在 ELF 上的后果 |
| --- | --- | --- |
| 归属守卫 | `^cuexis_` | 跳过**每一个** Cuexis 库 → 内部库禁用表**永不生效**（空转） |
| shared 正例 | `^cuexis_playback` | 永远匹配不上 → 正确的 Linux 宿主**误报失败** |

CI 的报错正是后者：

```
The shared reference host does not import cuexis_playback: libcuexis_playback-0.7.so.0.7;...
```

### 四项缺陷的修正

1. **拆开工具变量**（缺陷 3）：`CUEXIS_SYMBOL_TOOL` 恢复为 MSVC `dumpbin` / 非 MSVC `nm`，
   供导出闸门使用；另立 `CUEXIS_IMPORT_TOOL` 为 MSVC `dumpbin` / 非 MSVC `objdump`，
   供宿主导入闸门使用。两个变量各有一个消费者，不再共享。
2. **导入工具统一为 `objdump -p`**（缺陷 1/2）：同一分支同时解析 PE 的 `DLL Name:` 与
   ELF 的 `NEEDED`。
3. **归一化库名**（缺陷 4）：先去掉前导 `lib`，再在 `.so` 处截断，
   再做归属与禁用判断。`libcuexis_playback-0.7.so.0.7` → `cuexis_playback-0.7`。
4. **static 口径显式化**（缺陷 4 的附带问题）：static 宿主不导入任何 Cuexis 库，
   归属判断无可判断对象。现在显式打印
   `Reference host is static: it imports no Cuexis library, so the internal-library ownership check has nothing to judge`，
   而不是让 `verified` 那一行暗示它做了无法做的检查。

### 证据

**本机（MSVC, SHARED）**：`cuexis_reference_host_staging` 与 `cuexis_shared_export_surface`
均通过，`100% tests passed, 0 tests failed out of 2`；staging 另报
`Reference host refused a foreign-toolchain package` 与 `Reference host refused an incompatible SDK minor`。

**真实二进制（PE）**：`objdump -p` 读本机 MSVC 构建的宿主得到 **8** 条 `DLL Name:`，
含 `cuexis_playback-0.7d.dll`；反空转守卫通过。

**真实二进制（ELF）**：在 WSL（Ubuntu 26.04）对 `/bin/ls` 运行 `objdump -p` 得到
`NEEDED libc.so.6` 等；对同一 ELF 运行 `nm -D` 得到**0** 个库名形状的行——
这是"`nm` 在此根本上不可能工作"的直接证明。归一化对真实 ELF 无任何误判（全部非 Cuexis）。

**归一化正反例**（PE 与 ELF 两种写法各跑接受/内部泄漏/无 Playback 导入）：

| 用例 | 归一化结果 | 判定 |
| --- | --- | --- |
| PE `cuexis_playback-0.7.dll` | `cuexis_playback-0.7.dll` | 接受 |
| PE `cuexis_world-0.7.dll` | `cuexis_world-0.7.dll` | 拒绝（内部库） |
| ELF `libcuexis_playback-0.7.so.0.7.0` | `cuexis_playback-0.7` | 接受 |
| ELF `libcuexis_world-0.7.so.0.7.0` | `cuexis_world-0.7` | 拒绝（内部库） |
| ELF `libcuexis_render_opengl-0.7.so.0.7.0` | `cuexis_render_opengl-0.7` | 拒绝（内部库） |
| 仅 OS 库（PE 或 ELF），SHARED | （无 Cuexis） | 拒绝（未导入 Playback） |
| 仅 OS 库，STATIC | （无 Cuexis） | 接受 + 显式说明 |

**口径声明**：本机**既无 MinGW 工具链也无 Linux 环境**，故这两个平台的最终复验
**只能由 hosted CI 提供**。本报告不宣称本机已通过这两个平台；
在 hosted 复验变绿之前，本批次不得写成"已验证"。

### 方法论备注（值得留存）

一个"防止空转"的守卫，其价值在于它会**先烧到自己**。缺陷 1–2 四次失败中，
没有一次是被测代码有问题，而是检查器本身没能力观察——若没有守卫，
缺陷会以"永远通过"的形态长期存活。
缺陷 3 则是另一个教训：**共享一个变量对**给两个需求不同的消费者，
是一个只在特定平台上才显形的耦合；本机 MSVC 全程绿，因为 MSVC 上两者恰好都是 `dumpbin`。

### 复核更正（同日，追加）

上述缺陷 3 与缺陷 4 由本批次报告的**独立复核**（对 R7/R8 的对抗式审计）发现并复现，
其结论已并入本节。审计同时指出：§2 第 1 行把该检查描述为"每个 Cuexis 归属导入都属于
**允许集**"是**措辞过强**——代码实际是一份 **9 个名称的禁用表**，不是允许集。
本机 MSVC 宿主的真实导入表恰好证明了这一点：它导入了 `cuexis_content-0.7d.dll` 与
`cuexis_core-0.7d.dll`，二者**不在**禁用表中，因而直接通过。
该措辞已在 §2 更正；禁用表本身是否应收紧为允许集，属**产品决策**，不在本批次范围内。

## 5. 残余与未核对

- **ADR 0042 `:350-351` 与交互命令循环实现的冲突**：未处置（需 owner 裁定），详见 §2.2。
  这是本批次**唯一**未闭环的复核项。
- **static flavor 无 toolchain 拒绝负例**：按 §2.1 裁定为「不为不存在的东西造负例」，
  由 minor 负例承担 static 拒绝面。若 owner 认为 static 也应有 toolchain 门禁，
  那是**产品决策**（要改 `CuexisConfig.cmake.in` 的兼容性块），不属本修正批次。
- **分发门禁不在 Linux 注册**：登记为 Stage 8 输入，详见 §2.4。
- **Stage 7A / Stage 8 归属未落盘**：计划 §10 要求 R7 成果并入 Stage 8 与 Stage 7A 的输入清单。
  本批次**只在本报告记录归属意向**，未修改 `docs/stage_plans/future/stage-08/plan.md`
  与 `docs/stage_reports/stages/stage-06/completion.md` §7——那两者属于阶段计划/关闭报告的
  正式修订，应由阶段关闭动作完成。恢复条件：Stage 8 启动时按计划 §10 执行移交。
- **hosted 三平台复验未完成**：本批次的门禁改动与 `CMakeLists.txt` 的工具发现移位
  影响所有 flavor 的注册路径，**必须在同 SHA 的 Linux Quality / Windows MSVC / Windows MinGW
  上复验**后才可作为本批次的最终结论。
  已知历史：`0c8f837`（仅修缺陷 1/2 的那一版）在 Linux 上**确实失败**，
  失败的是 `cuexis_shared_export_surface` 与 `cuexis_reference_host_staging`
  （`GCC Shared Release` / `Clang Shared Debug`），即 §4.2 的缺陷 3 与缺陷 4；
  修复提交为 `2c74f5f`，其 hosted 结果**尚未返回**，故在此之前本批次不得写成"已验证"。
- **导入检查的覆盖边界（本次复核新增登记）**：
  - 该检查是 **9 个名称的禁用表**，不是允许集。宿主真实导入了 `cuexis_content` 与
    `cuexis_core`，二者不在表内而直接通过。是否收紧为允许集属**产品决策**（见 §4.2 末）。
  - **static 下该判断无可判断对象**：static 宿主不导入任何 Cuexis 库，
    禁用表永不触发。现已显式打印说明（§4.2 修正 4），但"内部库未泄漏"这一结论
    在 static flavor 上**并未被本门禁证实**，只是无害。

## 6. 变更文件

- 门禁：`cmake/VerifyReferenceHost.cmake`（新增导入表检查、SDK minor 负例、candidate 零命中扫描；
  `ENV{PATH}` 恢复点前移；库名归一化 `lib` 前缀与 `.so` 后缀；static 口径显式说明）
- 构建：`CMakeLists.txt`（工具发现上移到门禁注册之前；**两个工具变量各自独立**——
  `CUEXIS_SYMBOL_TOOL` = MSVC `dumpbin` / 非 MSVC `nm` 供导出闸门，
  `CUEXIS_IMPORT_TOOL` = MSVC `dumpbin` / 非 MSVC `objdump` 供宿主导入闸门；
  宿主门禁接收后者）
- 文档：本报告、`docs/stage_reports/reviews/stage-06-review-2026-09/README.md`（索引）、
  `docs/stage_reports/README.md`（可达性）

## 7. 追加订正（2026-09-29）：批次 R9 与本报告的一处错误登记

本节为**追加**内容，不改动本文 §1–§6 的任何原文、现象与证据。

### 7.1 §2.2 步骤 4 与残余清单的「未处置」

本文 §2.2 与残余清单把「交互命令循环」的口径记为「登记口径」，把 ADR 0042 `:350-351` 的冲突记为
**未处置（需 owner 裁定）**。该记录在写下时准确。owner 其后裁定并开启批次 **R9**，追加三点订正：

1. **零 stdin 不是缺陷**。参考宿主按设计从命令文件读取指令，不读标准输入。
2. **缺口是命令不可由外部下达，以及缺少 play/pause 语义**。七动词中 `play` / `pause` / `quit`
   当时无法被外部下达（`SPEC-27`）。本文新增的符号级导入表检查与 minor 负例只覆盖 argv 面，
   这正是 `SPEC-27` 能在门禁全绿下存活的机械原因。
3. **原计划允许本批次补实现，不只限 Stage 8**。

R9 据此实现，见
[R9 报告](../../stages/stage-06/2026-09-28-r9-reference-host-command-loop.md)。

### 7.2 §6 登记了一个从未存在的文件

本文 §6 的文档项写有 `docs/stage_reports/reviews/stage-06-review-2026-09/README.md`（索引）。
**该文件从未在任何 ref 上提交过**（对该路径执行 `git log --all --diff-filter=A` 为空），现在也不存在。

索引职责实际由 `docs/stage_reports/README.md` 承担；并且 `tools/check_docs.py` 的
`STAGE_NAVIGATION_INDEXES` 白名单**不含**该路径，新建它会违反检查器——R9 规范 §9 第 3 项据此明确
「**不新建叶目录 README**，也不新增 `reviews/README`」。原句保留，本订正为其更正。

R9 状态：实现完成、本地验证完成，并在最后行为 SHA `71de8b1` 上通过 hosted 四工作流验证；**尚未获 owner
接受退出**，`SPEC-27` 仍为 open。

### 7.3 §5 的两条 hosted 残余声明已被取代

本文 §5 第一条写「**hosted 三平台复验未完成**」，其下第三条写「修复提交为 `2c74f5f`，其 hosted 结果
**尚未返回**，故在此之前本批次不得写成"已验证"」。两条在写下时都准确，且第二条的**自我约束是正确
的**——不要拿未返回的结果当已验证。但它们的**后续状态**记录在别处，单独读本文会得出"本批次仍未
验证"的错误结论，故在此指向后续记录：

- `2c74f5f` **不是**"尚未返回"，而是**被取代后主动取消**（它仍带 MinGW 编译错误）。该 SHA 之后的
  `7ac37f8`、`c6f1e45` 同样被取消；中间 SHA 的取消是主动清理，不是失败遮蔽。登记见
  [交付报告](2026-09-28-delivery-report.md) 的 SHA 表（`2c74f5f` 一行与紧邻说明）与
  [修正计划 §4](../../../stage_plans/reviews/stage-06-review-remediation/plan.md)。
- 本批次（R7）在[交付报告 §2](2026-09-28-delivery-report.md) 的批次总表中记为 `completed
  （3 项登记，1 项上报）`，其门禁改动此后持续在 hosted 上运行；R9 在最后行为 SHA `71de8b1` 的验证中
  同样覆盖了 `VerifyReferenceHost.cmake` 的导入表检查与 minor 负例。

**原文保留不改**：本节只追加后续指向，不修改 §5 的任何字句与当时判断。

### 7.4 R9 状态已被 owner 接受退出取代（2026-09-29）

§7.2 末尾那条状态行写 R9「**尚未获 owner 接受退出**，`SPEC-27` 仍为 open」。该行写入时准确，
现已被取代：**owner 于 2026-09-29 接受 R9 退出**，R9 规范文档 §0.2 的六项关闭要求至此逐条满足，
`SPEC-27` 记为 **closed**。

最终 SHA 的证据：`71de8b1` 之后的文档提交把 tip 推到 `cc14fcd`，该 tip 在 push 与 pull_request 两个
事件上共 7 个运行全部通过。**上句原文保留不改**，以本行为准。逐条对照与"本次接受不做什么"见
[交付报告 §11](2026-09-28-delivery-report.md) 与 [R9 报告 §10](../../stages/stage-06/2026-09-28-r9-reference-host-command-loop.md)。

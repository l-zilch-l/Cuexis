# Stage 6 复核修正：R7 C4 门禁与分发补齐

状态：completed
快照日期：2026-09-28
更新日期：2026-09-28

本报告是 [Stage 6 复核修正计划](../../../stage_plans/active/stage-06-review-remediation/plan.md)
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
| 步骤 1 | SPEC-27 | 宿主符号级检查未接线；机制（`VerifySharedExports`/`VerifySharedConsumerImports`）已有但不用于宿主 | **已补齐**：宿主门禁新增导入表检查，要求宿主可执行文件的每个 Cuexis 归属导入都属于允许集；内部模块显式列名 | `cmake/VerifyReferenceHost.cmake`；`CMakeLists.txt` |
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
- **hosted 三平台复验未完成**：本批次的门禁改动与 `CMakeLists.txt` 的 `find_program` 移位
  影响所有 flavor 的注册路径，**必须在同 SHA 的 Linux Quality / Windows MSVC / Windows MinGW
  上复验**后才可作为本批次的最终结论。

## 6. 变更文件

- 门禁：`cmake/VerifyReferenceHost.cmake`（新增导入表检查、SDK minor 负例、candidate 零命中扫描；
  `ENV{PATH}` 恢复点前移）
- 构建：`CMakeLists.txt`（符号工具发现上移到门禁注册之前；向宿主门禁传入
  `CUEXIS_SYMBOL_TOOL` / `CUEXIS_SYMBOL_TOOL_KIND`）
- 文档：本报告、`docs/stage_reports/reviews/stage-06-review-2026-09/README.md`（索引）、
  `docs/stage_reports/README.md`（可达性）

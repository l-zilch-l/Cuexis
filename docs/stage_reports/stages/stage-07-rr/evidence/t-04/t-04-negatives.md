# T-04 / task-15 链接与结构阴性

状态：evidence

<!-- cuexis-meta
id: EVIDENCE-stage-07-rr.t-04-negatives
status: evidence
authority: none
date: 2026-11-24
-->

- 执行代理：negatives（root: lead）
- 副本创建提交：`3943fb1365853b6f5af5c8a41ef322d147863ee6`。执行期间主树先到 `0bb2d927`
  （只改 `docs/`），随后 `92539ad4` 修复了本任务暴露的门禁缺陷；该提交只改
  `cmake/VerifyPlaybackCandidateIsolation.cmake` 一行。
- 门禁脚本：修复前 SHA-256
  `BC7DB017462F0A3B1D0BF89FA56F3CC17731DBC89BE18CE4C7D6AC997A6FF8B1`，固定为
  ``scripts/VerifyPlaybackCandidateIsolation.cmake.pinned``；
  修复后（`92539ad4`）SHA-256
  `49E9A7BDD23844B7309D799BF9D25D5FCC1AAF182F49E35C46D95563566157CB`，固定为
  ``scripts/VerifyPlaybackCandidateIsolation.fixed-92539ad4.cmake``。
  A1/A2/A3/B 的原始取证使用修复前脚本；A2 另用修复后主树脚本复跑，A1/A3 也用修复后脚本复跑确认首错不变。
- 判定日期：2026-11-24

## 判定摘要

| 项 | 内容 | 结论 |
|---|---|---|
| A1 | `cxt_v2_loader.cpp` 放回基础 Chart 源表 | **成立**：配置成功，门禁退出 1，首条错误点名 `cxt_v2_loader.cpp` |
| A2 | 给 `cuexis_chart` 加 codec 链接 | **成立**：修复后主树门禁（`92539ad4`）在候选配置直接报 `base Chart target 'cuexis_chart' links 'cuexis_chart_codec'`；默认配置首错仍是更早的 Playback 闭包命中且同样点名 codec；修复前假阴性保留为历史 |
| A3 | 基础 Chart 的 PUBLIC include 加 `tools_include` 根 | **成立**：配置成功，门禁退出 1，首条错误点名 `tools_include` |
| B | 两个链接阴性 probe + tools 对照 | **成立**：两个阴性链接均退出 1120 并给出未解析符号名，tools 对照退出 0 |
| 第 5 项整体 | A1/A2/A3 结构阴性 + B 链接阴性 | **成立**：A1/A2/A3 与 B 均达到各自判据；A2 经 `92539ad4` 修复后门禁复跑确认 |

未发现其他失败项；没有把 CMake 配置错误当作阴性结论。A2 的配置失败单独记录并标注为“不是门禁结果”，
其修复前假阴性已由 `92539ad4` 修复并以修复后门禁复跑关闭。

## 环境与边界

- Windows，MSVC 14.51.36231（VS 18 Community，`vcvars64.bat`），CMake 4.3.3，Ninja。
- 每个阴性使用独立副本：`copies/A1`、`copies/A2`、`copies/A3`，由 `robocopy` 从主树复制
  （排除 `out`、`.git`、`docs`、`.idea`、`.mnemon`、`cmake-build-debug`、`.github`）。生产树只读；
  所有写入都在本目录下，没有 `git add`/`commit`。
- 每个副本构建目录在配置前已放空文件 `<build>/.cmake/api/v1/query/codemodel-v2`。
- 为免重复安装 vcpkg 包，副本配置复用 `out/build/headless-debug/vcpkg_installed`：
  `-DVCPKG_MANIFEST_INSTALL=OFF -DVCPKG_INSTALLED_DIR=<主树 headless-debug 的 vcpkg_installed>`。
  配置日志里保留该命令；该选项只影响依赖安装方式，不影响 codemodel 生成。
- 配置统一用副本内 `cmake --preset headless-debug -B <build> --fresh`（A2 另有一个
  `candidate-headless-debug` 配置）；不需要完整构建，门禁只读 codemodel、cinstall 脚本与目标表。
- 门禁调用变量与主树 `CMakeLists.txt` 的 `cuexis_rr_playback_isolation` 测试注册一致
  （`CUEXIS_BINARY_DIR`、`CUEXIS_ENABLE_CHART_V5_CANDIDATE`、`CUEXIS_PLAYER_TARGET`、
  `CUEXIS_LIBRARY_TYPE`、`CUEXIS_BUILD_DEVELOPER_TOOLS`、`CUEXIS_BUILD_TESTS`、
  `CUEXIS_SOURCE_DIR`）。脚本用主树中的版本执行，副本只提供被检查的 codemodel。

### 阳性对照

1. 主树 `out/build/headless-debug` 原样配置：门禁退出 0，
   ``logs/baseline-main-headless-debug-gate.log``。
2. 每个副本在施加变异前（或撤回变异后）用同一配置命令重新配置：门禁退出 0：
   A1 ``a1/gate-positive.log``，
   A2 ``a2/gate-positive.log``，
   A3 ``a3/gate-positive.log``。

## A1：`cxt_v2_loader.cpp` 放回基础 Chart 源表

- 变异：``a1/mutation.diff``，在 `engine/chart/CMakeLists.txt` 的
  `target_sources(cuexis_chart ...)` 中加回 `src/cxt_v2_loader.cpp`，tools 源表不动。
- 配置：`scripts/configure-copy.cmd copies\A1 copies\A1\build a1\configure.log <vcpkg>`，
  退出码 0（``a1/configure.exit``）。
- 门禁：`scripts/gate.cmd copies\A1\build copies\A1 a1\gate-rerun.log`，退出码 1
  （``a1/gate-rerun.exit``，原始运行见 ``a1/gate.log``）。
- 首条错误（``a1/first-error.txt``）：

  ```text
  CMake Error at cmake/VerifyPlaybackCandidateIsolation.cmake:288 (message):
    VerifyPlaybackCandidateIsolation: Chart target 'cuexis_chart' compiles the
    unexpected translation unit 'cxt_v2_loader.cpp'; each Chart TU belongs to
    exactly one of cuexis_chart, cuexis_chart_codec and cuexis_chart_tools
  ```

- codemodel 佐证：`cuexis_chart` 的源表包含 `engine/chart/src/cxt_v2_loader.cpp`
  （``a1/codemodel-cuexis_chart-cxt-source.txt``）。
- 阳性对照：撤回变异并 fresh 配置后门禁退出 0（``a1/configure-positive.log``、
  ``a1/gate-positive.log``）。
- 取证后按任务约束清理了构建对象文件；保留 codemodel、库与配置日志。清理后同一构建目录重跑门禁
  仍退出 1（``a1/gate-after-trim.log``）。

**结论：成立。**

## A2：给 `cuexis_chart` 加 codec 链接（成立；修复前曾暴露门禁缺陷）

### 修复后复跑（主树门禁 92539ad4）

主树 `cmake/VerifyPlaybackCandidateIsolation.cmake` 的 `cuexis_target_dependency_names()` 已改为
读取 File API 依赖对象的 `id` 成员（commit `92539ad4`，一行修改；固定副本与 SHA-256 见文首）。
A2 的两个既有配置不重新配置，直接用修复后的主树门禁复跑：

- **候选配置**（与 lead 报告的复跑命令一致）：
  `cmake -DCUEXIS_BINARY_DIR=.../copies/A2/build-candidate
  -DCUEXIS_ENABLE_CHART_V5_CANDIDATE=ON -DCUEXIS_BUILD_DEVELOPER_TOOLS=ON
  -DCUEXIS_BUILD_TESTS=ON -DCUEXIS_SOURCE_DIR=.../copies/A2
  -P cmake/VerifyPlaybackCandidateIsolation.cmake`
  退出 1（``a2/gate-fixed-candidate.exit``、
  ``a2/gate-fixed-candidate.log``），首条错误：

  ```text
  CMake Error at cmake/VerifyPlaybackCandidateIsolation.cmake:548 (message):
    VerifyPlaybackCandidateIsolation: base Chart target 'cuexis_chart' links
    'cuexis_chart_codec'; the base Reader/CXT1/v4 Resolver closure must stay
    independent
  ```

  这就是卡第 5 项要求的“门禁非零并点名该链接边”。

- **默认配置**（`headless-debug`：`CUEXIS_ENABLE_CHART_V5_CANDIDATE=OFF`、
  `CUEXIS_BUILD_DEVELOPER_TOOLS=OFF`、`CUEXIS_BUILD_TESTS=ON`）：
  退出 1（``a2/gate-fixed-off.exit``、
  ``a2/gate-fixed-off.log``），首条错误
  （``a2/first-error-fixed-off.txt``）：

  ```text
  CMake Error at cmake/VerifyPlaybackCandidateIsolation.cmake:453 (message):
    VerifyPlaybackCandidateIsolation: ordinary cuexis_playback closure links
    candidate target 'cuexis_chart_codec'
  ```

  453 行的闭包检查在脚本中位于 548 行的基础 Chart 链接检查之前，FATAL 在首错停止，所以默认配置
  修复后的首错仍然是 Playback 闭包命中，并没有变成 base Chart 边；门禁非零且点名
  `cuexis_chart_codec` 的结论不变。

- **阳性对照**：修复后门禁对主树 clean 的 `headless-debug` 与 `candidate-headless-debug` 配置
  均退出 0（``logs/gate-fixed-main-headless-debug.log``、
  ``logs/gate-fixed-main-candidate-headless-debug.log``）；
  A1/A3 的变异副本用修复后脚本复跑，退出码与首错不变
  （``a1/gate-fixed.log``、``a3/gate-fixed.log``）。

下列 A2a/A2b 与根因分析是**修复前**的取证记录，保留用于说明缺陷如何被发现。两次复跑的汇总见
``a2/fixed-gate-rerun.md``。

### A2a（修复前记录）按要求直接加链接：配置阶段先被拦下，未到门禁

- 变异：``a2/mutation-requested.diff``，
  在 `engine/chart/CMakeLists.txt` 的 Chart 支持库分支末尾加
  `target_link_libraries(cuexis_chart PUBLIC cuexis::chart_codec)`。
- 配置退出码 1（``a2/configure-requested.exit``，
  ``a2/configure-requested.log``），首条错误：

  ```text
  CMake Error at cmake/VerifyArchitecture.cmake:652 (message):
    Cuexis target cuexis_chart directly links non-allowlisted dependency
    cuexis::chart_codec.  Allowed dependencies:
    cuexis::core;cuexis::json_support
  ```

- 这是配置期 `cuexis_verify_target_dependencies` 的 ALLOWED 检查，**不是** codemodel 门禁结果，
  不能当阴性结论。按任务约束，改用不触发该检查的等价接线继续验证门禁本身。

### A2b（修复前记录）等价接线：让 codemodel 记录该链接边，再跑门禁

- 变异：``a2/mutation-compensated.diff``，在根 `CMakeLists.txt` 的
  `cuexis_verify_target_dependencies(cuexis_chart ...)` 之后加
  `target_link_libraries(cuexis_chart PRIVATE "$<BUILD_INTERFACE:$<LINK_ONLY:cuexis_chart_codec>>")`。
  `LINK_ONLY` 只保留构建树链接、不传播 codec 的 PUBLIC include 根，使被测点收敛到“链接边”本身；
  位置在 ALLOWED 检查之后，配置可完成。codemodel 的 `linkLibraries`、`interfaceLinkLibraries`
  与 `dependencies` 都记录了 `cuexis_chart_codec`（``a2/codemodel-cuexis_chart-dependencies.json``）。
- 默认配置（`headless-debug`）：配置退出 0（``a2/configure-off.exit``），
  门禁退出 1（``a2/gate-off.exit``），首条错误
  （``a2/first-error-off.txt``）：

  ```text
  CMake Error at cmake/VerifyPlaybackCandidateIsolation.cmake:453 (message):
    VerifyPlaybackCandidateIsolation: ordinary cuexis_playback closure links
    candidate target 'cuexis_chart_codec'
  ```

  这条是“普通 Playback 闭包链接候选目标”的检查命中了 codec，**不是**门禁里专门的基础 Chart
  链接检查（该检查在脚本 545–552 行，本应报 `base Chart target 'cuexis_chart' links
  'cuexis_chart_codec'`）。
- 候选配置（`candidate-headless-debug`，`CUEXIS_ENABLE_CHART_V5_CANDIDATE=ON`）：配置退出 0
  （``a2/configure-candidate.exit``），门禁退出 **0**，输出为
  `ON closure OK ...`（``a2/gate-candidate.exit``、
  ``a2/gate-candidate.log``）。候选闭包本来就允许 codec，故闭包检查不会命中；
  而专门的基础 Chart 链接检查没有报错，形成假阴性。

### 修复前假阴性与根因（历史记录）

门禁的 `cuexis_target_dependency_names()`（`VerifyPlaybackCandidateIsolation.cmake` 191–216 行）
把 File API target JSON 的 `dependencies` 元素当成字符串 ID 读取：

```cmake
string(JSON dependency_id GET "${target_json}" dependencies ${dependency_index})
```

本机 CMake 4.3.3 写出的元素是对象，ID 在 `id` 成员中；同一脚本的闭包函数（249 行）读的是
`... dependencies ${dependency_index} id`。用同一份 codemodel 复现
（``a2/diagnose-dependency-helper.log``）：

```text
-- cuexis_chart dependencies (raw File API target JSON), count=1
--   entry[0] = { "id" : "cuexis_chart_codec::@301fa28775cfcdfa2213" }
-- cuexis_target_dependency_names() result = []
-- correct id-member extraction = [cuexis_chart_codec]
```

因此 545–552 行的禁止链接检查对任何目标都取不到名字；默认配置只是因为前面的 Playback 闭包检查
先命中才非零，候选配置则完全漏检。建议把该行改为读取 `... id`（本次验证只读，未修改生产脚本）。

为确认直接原因，另用同一份 codemodel 跑了一个诊断副本：只把 207 行改成读取
`... dependencies ${dependency_index} id`（``scripts/VerifyPlaybackCandidateIsolation.probe-id-fix.cmake``，
不是生产脚本）。候选配置门禁随即退出 1 并报出目标错误
（``a2/gate-candidate-with-id-fix.log``、
``a2/gate-candidate-with-id-fix.exit``）：

```text
CMake Error at .../VerifyPlaybackCandidateIsolation.probe-id-fix.cmake:548 (message):
  VerifyPlaybackCandidateIsolation: base Chart target 'cuexis_chart' links
  'cuexis_chart_codec'; the base Reader/CXT1/v4 Resolver closure must stay
  independent
```

这证明候选配置的假阴性确实来自 `cuexis_target_dependency_names()` 的取值缺陷；接线与 fixture 均无关。

**结论：成立（以修复后的主树门禁 `92539ad4` 为准）。** 候选配置复跑退出 1，首错直接点名
`base Chart target 'cuexis_chart' links 'cuexis_chart_codec'`；默认配置复跑退出 1，首错仍是脚本中
更早的 Playback 闭包检查（453 行），同样点名 `cuexis_chart_codec`。上面的假阴性、根因与修复前复现
保留为历史记录，已由 `92539ad4` 关闭。

## A3：基础 Chart 的 PUBLIC include 加 `tools_include`

- 变异：``a3/mutation.diff``，在 `engine/chart/CMakeLists.txt` 的
  `target_include_directories(cuexis_chart PUBLIC ...)` 后加入
  `$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/tools_include>`。
- 配置退出 0（``a3/configure.exit``）。
- 门禁退出 1（``a3/gate.exit``），首条错误
  （``a3/first-error.txt``）：

  ```text
  CMake Error at cmake/VerifyPlaybackCandidateIsolation.cmake:329 (message):
    VerifyPlaybackCandidateIsolation: target 'cuexis_chart' exposes the
    forbidden include root
    '.../copies/A3/engine/chart/tools_include'
  ```

- codemodel 佐证：`cuexis_chart` 的 include 根中出现
  `.../copies/A3/engine/chart/tools_include`
  （``a3/codemodel-cuexis_chart-tools-include.txt``）。
- 阳性对照：变异前同副本配置与门禁均退出 0（``a3/configure-positive.log``、
  ``a3/gate-positive.log``）。

**结论：成立。**

## B：两个链接阴性 probe 与 tools 对照

probe 位于 ``b/probe_lowering.cpp`` 与 ``b/probe_encode.cpp``，
不在 `tests/` 或 `engine/`。两个 probe 都手写了与
`cuexis::chart::lowerCandidateRuntime` / `cuexis::chart::packed::encode` 等签名的声明并在 `main`
中实际调用；调用点不解引用空引用，只用于链接取证。编译使用副本 A1 的新鲜构建产物
（`copies/A1/build/lib/*.lib`，由 clean 副本构建，``b/build-chart-libs.log``，
SHA-256 见 ``b/library-hashes.txt``）。

### B1 `lowerCandidateRuntime`，只链 `cuexis_chart`

- 编译：`scripts/compile-probe-lowering.cmd`，退出 0（``b/compile-lowering.log``）。
- 链接：`link /nologo /SUBSYSTEM:CONSOLE /OUT:link-lowering-chart.exe probe_lowering.obj
  copies\A1\build\lib\cuexis_chart.lib`，退出 **1120**
  （``b/link-lowering-chart.exit``）。
- 未解析符号（``b/link-lowering-chart.log``）：

  ```text
  ?lowerCandidateRuntime@chart@cuexis@@YA?AUCandidateRuntimeArtifactResult@12@AEBUCanonicalSemanticChart@12@AEBUChartLimits@12@@Z
  ```

### B2 `packed::encode`，只链 `cuexis_chart_codec`

- 编译：`scripts/compile-probe-encode.cmd`，退出 0（``b/compile-encode.log``）。
- 链接：`link ... probe_encode.obj copies\A1\build\lib\cuexis_chart_codec.lib`，退出 **1120**
  （``b/link-encode-codec.exit``）。
- 未解析符号（``b/link-encode-codec.log``）：

  ```text
  ?encode@packed@chart@cuexis@@YA?AV?$expected@V?$vector@W4byte@std@@V?$allocator@W4byte@std@@@2@@std@@VError@core@cuexis@@@tl@@AEBUCanonicalSemanticChart@23@UPackedChartProfile@123@UPackedChartLimits@23@@Z
  ```

### B3 tools 对照

- `link ... probe_lowering.obj cuexis_chart_tools.lib cuexis_chart_codec.lib cuexis_chart.lib
  cuexis_core.lib cuexis_json_support.lib`，退出 0
  （``b/link-lowering-tools.exit``、``b/link-lowering-tools.log``）。
- `link ... probe_encode.obj` 同组库，退出 0
  （``b/link-encode-tools.exit``、``b/link-encode-tools.log``）。

### B4 dumpbin 交叉核对

- `cuexis_chart.lib` 中 `lowerCandidateRuntime` 匹配 0
  （``b/dumpbin-chart-lowering.log``）。
- `cuexis_chart_codec.lib` 中 `packed::encode` 匹配 0
  （``b/dumpbin-codec-encode.log``）。
- `cuexis_chart_tools.lib` 中两者都有定义（`SECT... External`），另有其他对象对它们的 UNDEF 引用
  （``b/dumpbin-tools-both.log``）。
- probe 目标文件中的 UNDEF 记录
  （``b/dumpbin-probe-lowering-undef.log``、
  ``b/dumpbin-probe-encode-undef.log``）与链接器输出一致。

**结论：成立。**

## 失败项与处理

1. **A2（已关闭）。** 修复前，直接按要求加 `PUBLIC cuexis::chart_codec` 会先在配置期被
   `cuexis_verify_target_dependencies` 的 ALLOWED 检查拒绝；改用等价接线后，默认配置门禁虽然非零，
   但报的是 Playback 闭包检查，候选配置门禁退出 0。根因是 `cuexis_target_dependency_names()`
   不读 File API 依赖对象的 `id` 成员，导致专门的基础 Chart 链接断言失效。主树已在 `92539ad4`
   修复该取值；用修复后的主树门禁复跑 A2 候选配置得到第 548 行预期错误并点名链接边，默认配置
   得到更早的第 453 行 Playback 闭包错误且同样点名 codec。A2 判定据此改为成立，修复前记录保留在
   A2 小节。
2. 无其他失败项。B 的两个阴性链接失败均为预期 LNK2019/LNK1120；对照链接均成功，未出现
   fixture、include 或 runtime-library 不匹配导致的假失败。

## 证据索引

- `a1/`：变异 diff、配置、门禁、首条错误、codemodel 源表摘录、阳性对照。
- `a2/`：直接变异与等价变异 diff、默认/候选配置、修复前门禁输出、修复后门禁复跑及汇总
  （`a2/fixed-gate-rerun.md`）、依赖处理诊断、codemodel 依赖摘录。
- `a3/`：变异 diff、配置、门禁、首条错误、codemodel include 摘录、阳性对照。
- `b/`：两个 probe 源文件、编译/链接脚本、链接与 dumpbin 日志、库哈希、构建日志。
- `logs/`：主树 baseline 门禁与修复后 clean 配置门禁。
- `scripts/`：本次使用的配置/门禁/构建/链接脚本，以及修复前、修复后（`92539ad4`）固定版本的门禁脚本。
- `copies/`：三个独立源码副本及其构建目录（构建对象文件在取证后可按任务约束清理；
  配置日志、codemodel、库文件和判定文件已保留）。

## 复现命令与退出码

完整命令在 `scripts/*.cmd`；逐项退出码：

| 步骤 | 命令 | 退出码 | 日志/记录 |
|---|---|---|---|
| 主树 baseline | `scripts/gate.cmd out\build\headless-debug <主树>` | 0 | `logs/baseline-main-headless-debug-gate.log` |
| A1 配置（变异） | `scripts/configure-copy.cmd copies\A1 copies\A1\build ...` | 0 | `a1/configure.log` |
| A1 门禁 | `scripts/gate.cmd copies\A1\build copies\A1 ...` | 1 | `a1/gate.log`、`a1/gate-rerun.log` |
| A1 阳性 | 撤回变异后配置 + 门禁 | 0 / 0 | `a1/configure-positive.log`、`a1/gate-positive.log` |
| A2 阳性 | clean A2 配置 + 门禁 | 0 / 0 | `a2/configure-positive.log`、`a2/gate-positive.log` |
| A2a 直接链接配置 | `scripts/configure-copy.cmd copies\A2 ...` | 1 | `a2/configure-requested.log` |
| A2b 默认配置 | `scripts/configure-copy.cmd copies\A2 ...` | 0 | `a2/configure-off.log` |
| A2b 默认门禁 | `scripts/gate.cmd copies\A2\build copies\A2 ...` | 1 | `a2/gate-off.log` |
| A2b 候选配置 | `scripts/configure-copy-candidate.cmd copies\A2 copies\A2\build-candidate ...` | 0 | `a2/configure-candidate.log` |
| A2b 候选门禁（修复前，历史） | `scripts/gate-candidate.cmd copies\A2\build-candidate copies\A2 ...` | 0 | `a2/gate-candidate.log` |
| A2 修复后候选门禁 | `scripts/gate-fixed-candidate.cmd`（主树门禁 `92539ad4`） | 1 | `a2/gate-fixed-candidate.log` |
| A2 修复后默认门禁 | `scripts/gate-fixed-off.cmd`（主树门禁 `92539ad4`） | 1 | `a2/gate-fixed-off.log` |
| 修复后 clean 主树门禁 | 主树 `headless-debug` / `candidate-headless-debug` | 0 / 0 | `logs/gate-fixed-main-*.log` |
| A1/A3 修复后门禁 | 变异副本 + 主树门禁 `92539ad4` | 1 / 1 | `a1/gate-fixed.log`、`a3/gate-fixed.log` |
| A3 阳性 | clean A3 配置 + 门禁 | 0 / 0 | `a3/configure-positive.log`、`a3/gate-positive.log` |
| A3 配置（变异） | `scripts/configure-copy.cmd copies\A3 ...` | 0 | `a3/configure.log` |
| A3 门禁 | `scripts/gate.cmd copies\A3\build copies\A3 ...` | 1 | `a3/gate.log` |
| B 编译 | `scripts/compile-probe-lowering.cmd` / `...encode.cmd` | 0 / 0 | `b/compile-*.log` |
| B 阴性链接 | `scripts/link-lowering-chart.cmd` / `...encode-codec.cmd` | 1120 / 1120 | `b/link-*-chart.log`、`b/link-encode-codec.log` |
| B 对照链接 | `scripts/link-lowering-tools.cmd` / `...encode-tools.cmd` | 0 / 0 | `b/link-*-tools.log` |

`robocopy` 返回 1 表示“成功复制文件”，不是失败。所有配置均用 `--fresh`，且配置前已放置
File API query 空文件。

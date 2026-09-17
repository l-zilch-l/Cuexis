# Chart Format Foundation Hardening R5：回归、Hosted 与交接

- 批次：R5（回归矩阵、同 SHA hosted 记录、关闭报告）
- 起始 SHA：`eeda4e4`（R4 提交；开工前工作区干净、无未提交修改）
- 本轮实现 SHA：`25e546d0a6f50d8865e8f5ba9c43cba98a43d194`
  （`fix: keep headless configurations configurable after R4`，唯一实现改动为
  `tests/cxc/CMakeLists.txt`）
- 分支：`codex/chart-format-foundation-hardening`（无 upstream，尚未推送）
- 平台与工具链：Windows x64；MSVC 14.51.36231（VS 18 Community）；
  GCC 16.1.0（MSYS2 ucrt64，`x64-mingw-static`）；Ninja；`VCPKG_ROOT=D:\vcpkg`
- 状态：**本地回归矩阵与最终 SHA 容量复跑完成；同 SHA hosted 验证未执行（需 owner 授权
  推送）；owner acceptance 未记录。** 因此本计划保持 active，Stage 6 保持 future，
  本报告不声明 R5 关闭。
- 容量数据：[Debug](2026-09-17-r5-capacity-data.json)、
  [Release](2026-09-17-r5-capacity-data-release.json)（均以实现 SHA `25e546d` 运行）

## 1. 结论摘要

1. 本轮发现并修复一个会**阻塞 hosted CI 的 R4 回归（A19）**：`tests/cxc` 无条件链接
   developer tool 层，导致所有 headless preset 在 generate 阶段失败，即 Linux Quality
   矩阵（5 个 headless 配置）全部无法配置。修复后 MSVC headless 与 MinGW headless 均可
   配置、构建并通过全量用例。
2. 本地回归矩阵 5 个配置全部绿色：Debug 683/683、Release 683/683、shared-debug
   686/686、headless-debug 612/612、mingw-headless-debug 612/612。
3. 兼容性回归满足 R5.2：自 R0 基线 `5085840` 起 **fixtures / schemas / canonical golden
   零改动**，v4 identity、FrameDigest v1-v3、Chart v4/CXT/CXC canonical bytes 相关用例
   全部通过；公共头保持纯 ASCII，`engine/chart` 未新增目标依赖。
4. 最终 SHA 容量复跑完成，且与 R4（`4ebf244`）的 wire 字节**完全一致**：
   low-reuse 40k = 1,232,408 bytes / decoded 1,232,024；CXT 阶梯 = 1,290 bytes；
   high-reuse 40k = 1,247,446 bytes。
5. 未闭合项：hosted 三平台（待授权）、owner acceptance、A16（CXT capability 声明来源，
   留给 Stage 6）、父图环路 O(n²) 观测项。

## 2. 新发现并修复的缺陷 A19：headless 配置无法生成

### 2.1 复现（先复现，后修复）

对 R4 提交内容执行计划 §5 建议的 MinGW headless 配置：

```powershell
cmake --preset mingw-headless-debug --fresh -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
```

结果（失败，非环境问题）：

```text
CMake Error at tests/cxc/CMakeLists.txt:25 (target_link_libraries):
  Target "cuexis_cxc_tests" links to:
    cuexis::cxc_tool_common
  but the target was not found.
CMake Generate step failed.  Build files cannot be regenerated correctly.
```

### 2.2 根因

- `CMakeLists.txt:106` 只在 `CUEXIS_BUILD_DEVELOPER_TOOLS` 为 ON 时
  `add_subdirectory(tools)`；`cuexis::cxc_tool_common`
  （`tools/cxc_common/CMakeLists.txt` 的 ALIAS）只存在于该分支。
- `tests/cxc/CMakeLists.txt` 却由 R4 在链接列表中**无条件**加入
  `cuexis::cxc_tool_common` 与 `cuexis::chart`（R4 新增
  `cxc_candidate_extension_tests.cpp`、`cxc_candidate_roundtrip_tests.cpp`，两者都
  `#include <cuexis/tools/cxc_candidate.hpp>`）。
- `headless-debug` 明确 `CUEXIS_BUILD_DEVELOPER_TOOLS=OFF`，而
  `headless-release`、`headless-shared-debug`、`headless-shared-release`、
  `headless-sanitize`（及 `-shader-tools`、`headless-coverage`、`headless-clang-tidy`）
  全部继承它。`.github/workflows/linux-quality.yml` 的 5 个矩阵项都是 headless preset，
  因此该分支内容在 hosted Linux 上必然 configure 失败；Windows MinGW/Windows MSVC
  矩阵使用的是非 headless 的 `debug`/`release`，所以只有 Linux Quality 会暴露它。
- 影响面判定：不是"某个新用例失败"，而是**整套 headless 配置无法生成**，包括架构测试与
  7 个 external consumer gate。

### 2.3 修复

`tests/cxc/CMakeLists.txt`：

- 基础用例源文件保留在 `cuexis_cxc_test_sources` 中，`cuexis_cxc_tests`、
  `CUEXIS_SOURCE_DIR` 定义与 include 目录在所有配置下都注册；
- 两个 tool 依赖的 candidate 用例源文件与 `cuexis::cxc_tool_common`/`cuexis::chart`
  链接只在 `if(TARGET cuexis::cxc_tool_common)` 成立时加入；
- 否则在 configure 期输出显式 `message(STATUS ...)`，不做静默跳过。

### 2.4 修复证据（正例 / 反例 / 边界）

| 项 | 结果 |
| --- | --- |
| 修复前 `mingw-headless-debug --fresh` | 失败：`cuexis::cxc_tool_common` not found（见 §2.1） |
| 修复后 `mingw-headless-debug --fresh` + 构建 | **成功**（209 个构建边；链接 `cuexis_cxc_tests`） |
| 修复后 `headless-debug --fresh` + 构建（MSVC） | **成功**（209 个构建边） |
| configure 期可见性 | 日志出现 `-- cuexis_cxc_tests: developer tool layer absent; candidate CXC cases excluded` |
| headless 用例数（应减少） | `cuexis_cxc_tests` 用例数 131（tool ON）→ **104**（headless），差值 27 = extension 20 + roundtrip 7 |
| 非 headless 用例数（应不变） | Debug 仍为 131，candidate 用例仍被编译与注册 |
| headless 全量 CTest（MSVC） | **612/612 通过**（修复前无法 configure） |
| headless 全量 CTest（MinGW） | **612/612 通过**（修复前无法 configure） |

覆盖边界（记录，不隐藏）：candidate CXC 集成用例只在
`CUEXIS_BUILD_DEVELOPER_TOOLS=ON` 的配置中存在。这一边界由 configure 期消息显式暴露，
并已写入 `docs/guides/BUILDING.md`；hosted Linux（全 headless）不覆盖它们，覆盖它们的是
hosted Windows MinGW/MSVC 的 `debug`/`release` 矩阵。为降低"GCC 从未编译过这些用例"的
风险，本轮另建 `out/build/mingw-tools`（gcc + `x64-mingw-static` +
`CUEXIS_BUILD_DEVELOPER_TOOLS=ON`，关闭 player/SDL/GL）单独编译并运行它们，见 §3.3。

## 3. 本地回归矩阵

所有结果均为实现 SHA `25e546d` 上的产物（构建输入在本轮改动后重建，R4 时期的 Debug/
Release 全量运行已被下表取代，符合计划 R5.4）。日志保存在 `out/evidence/r5/`。

### 3.1 五个配置的全量 CTest

| preset | 库类型 / developer tools | 用例数 | 结果 | 耗时 | 跳过 |
| --- | --- | --- | --- | --- | --- |
| `debug` | static / ON | 683 | **683/683 通过** | 523.81 s | 1（symlink 平台用例） |
| `release` | static / ON | 683 | **683/683 通过** | 362.76 s | 1（同上） |
| `shared-debug` | `CUEXIS_LIBRARY_TYPE=SHARED` / ON | 686 | **686/686 通过** | 489.35 s | 3（见 §3.2） |
| `headless-debug` | static / **OFF** | 612 | **612/612 通过** | 608.70 s | 1（symlink） |
| `mingw-headless-debug` | GCC 16.1.0 ucrt64 / **OFF** | 612 | **612/612 通过** | 720.31 s | 1（symlink） |

- `shared-debug` 比 static 多 3 个 `shared` 标签用例（共享库拓扑专项）。
- 所有配置中 `cuexis_architecture_tests` 均为第 1 个用例并通过（私有候选实现未进入公共
  头/安装集，未新增 adapter 传递依赖）。
- 所有配置中 7 个 `cuexis_external_consumer_*`（`find_package`、`add_subdirectory`、
  playback/core/audio_sdl 变体）全部通过：安装头 + 包消费边界在 static、shared、headless
  与 MinGW 下都成立。
- hosted Linux 使用的 sanitize/coverage/clang-tidy preset 在 Windows/MSVC 上按仓库既有
  说明不可运行，本轮**未执行**，留给 hosted。

### 3.2 shared 配置的跳过项（预先存在，非本轮引入）

| 用例 | 跳过原因（源码内 `SKIP`） |
| --- | --- |
| `Secure file rejects physical containment escapes through symlinks` | 平台条件，static/shared 均跳过 |
| `Playback v4 prepare reuses the initial Chart parse` | `CUEXIS_PLAYBACK_PARSE_COUNT_PROBE_UNAVAILABLE`：parse 探针在共享库下不可见 |
| `PB-08 owning-copy queries contain allocation failures at the Playback boundary` | `CUEXIS_PLAYBACK_ALLOCATION_TEST_SHARED`：故障注入替换的是测试可执行文件的 allocator，DLL 内分配在该边界之外 |

三者都是源码内显式 `SKIP`，与本轮改动无关；static 配置保留了对应的真实注入覆盖。

### 3.3 GCC 编译 candidate 用例（补覆盖边界）

为确认"headless 排除的用例在 GCC 下可编译、可运行"，另建
`out/build/mingw-tools`（gcc + `x64-mingw-static` + `CUEXIS_BUILD_DEVELOPER_TOOLS=ON`，
player/SDL/GL 全关，命令见 §7）：

| 目标 | 结果 |
| --- | --- |
| 编译 `cuexis_cxc_tests`、`cuexis_chart_tests` | 成功（99 个构建边，含 `cxc_candidate_extension_tests.cpp`、`cxc_candidate_roundtrip_tests.cpp`、`packed_roundtrip_tests.cpp`、`packed_semantic_identity_tests.cpp`、`packed_profile_rejection_tests.cpp`） |
| 运行 `cuexis_cxc_tests`（GCC） | **59/59 用例通过**（745 assertions，与 MSVC Debug 相同） |
| 运行 `cuexis_chart_tests`（GCC） | **210/210 用例通过**（41,848 assertions，与 MSVC Debug 相同） |
| GCC 下的 40k 容量用例 | `packedBytes=1232408 decodedBytes=1232024`，与 MSVC Debug/Release 完全一致 |
| 意义 | R1-R4 新增的候选/往返/身份用例已确认可在 GCC 编译并通过；hosted Linux 因全 headless 不覆盖它们，hosted Windows MinGW/MSVC 的 `debug`/`release` 矩阵才是其 hosted 覆盖 |

## 4. 兼容性回归（R5.2）

### 4.1 既有格式的 canonical bytes 未被改变

- `git diff --name-status 5085840..HEAD -- tests/fixtures schemas` = **0 个文件**：
  自 R0 基线起没有 fixture、schema 或 golden 字节被修改。
- 新增的唯一数据文件是容量证据 JSON（`docs/stage_reports/...`），不参与读写路径。
- wire 层不变：仍为 `flags=1`、`candidateRevision=1`，无新增字段或 section；
  R4 记录的 writer 字节变化只发生在"修复真实数据丢失/排序规范化"的路径上，已由 Spec 与
  R4 报告逐条解释。

### 4.2 v4 identity / FrameDigest / 默认路由回归（Debug 全量日志中的具体用例）

| 用例（CTest 编号） | 结果 |
| --- | --- |
| 556 FrameDigest preserves v1 and v2 historical definitions | Passed |
| 558 FrameDigest v3 hashes portable ref presence, type, ID, and identity | Passed |
| 560 v2 hop lift matches FrameSnapshot and FrameDigest v3 | Passed |
| 653 Static v3 lift matches FrameSnapshot and FrameDigest v3 | Passed |
| 649 Default Playback seek, stop, discontinuity, and frame rate keep FrameDigest v3 | Passed |
| 659 Default Playback CXT sources share identity, FrameDigest v3, and object order | Passed |
| 545 Filesystem, memory, host and CXC sources share one static v4 semantic identity | Passed |
| 579 Parameterized v4 identity and FrameDigest follow frozen parameter values | Passed |
| 218 Chart v4 canonical source and resolved identity are whitespace invariant | Passed |
| 248 Parameter identity matches the frozen binary encoding vector | Passed |
| 183 Chart v4 and CXT canonical source bytes remain writer-parity baselines | Passed |
| 337 CXT v2 expansion identity is independent of source array order | Passed |
| 400 CXC project documents preserve source bytes while typed readers expose canonical bytes | Passed |
| 669 Validation Sink produces canonical pass order, effective state, and stable digest | Passed |

即：新 hash 路径没有改变既有格式的合法 canonical bytes，也没有改变 v4 identity 与
FrameDigest v1-v3 的冻结定义（相关用例源码在本阶段未被修改）。

### 4.3 SDK 边界

| 检查 | 结果 |
| --- | --- |
| `engine/chart/CMakeLists.txt` 依赖变化 | 仅新增两个源文件（`packed_profile.cpp`、`packed_semantic_identity.cpp`），无新增目标依赖 |
| 修改过的公共头 ASCII 纯度 | `limits.hpp`、`packed_chart_tables.hpp` 非 ASCII 字节数 = 0 |
| `cuexis_architecture_tests` | 5 个配置全部通过（含安装头不得暴露 EnTT/SDL/GL/JSON DOM 等规则） |
| external consumer gates | 7 个用例 × 5 个配置全部通过 |
| 默认 Writer / v1-v4 Reader | 未切换、未删除；`chart-format-update-for-v5` 未移动 |

## 5. 最终 SHA 容量复跑（R4 遗留项闭合）

`cmake/VerifyChartCapacity.cmake` 以运行时 `git rev-parse HEAD` 作为实现 SHA；本轮在干净
工作区、HEAD = `25e546d` 时复跑，机器可读结果保存为
[Debug](2026-09-17-r5-capacity-data.json) 与
[Release](2026-09-17-r5-capacity-data-release.json)。

| Profile | 构建 | Packed bytes | decoded bytes | IDN0 bytes | encode | decode | peakDelta |
| --- | --- | --- | --- | --- | --- | --- | --- |
| low-reuse-v1（40k 实体） | Debug | 1,232,408 | 1,232,024 | 680,012 | 4.07 s | 3.45 s | 50.6 MB |
| low-reuse-v1（40k 实体） | Release | 1,232,408 | 1,232,024 | 680,012 | 0.174 s | 0.179 s | 20.2 MB |
| cxt-stair-ladder（16 实体） | Debug | 1,290 | 874 | 115 | 2.4 ms | 1.7 ms | 0 |
| cxt-stair-ladder（16 实体） | Release | 1,290 | 874 | 115 | 0.12 ms | 0.10 ms | 0 |
| high-reuse-v1（可选观测） | Debug | 1,247,446 | 1,247,030 | 199,543 | 6.30 s | 4.44 s | 97.1 MB |
| high-reuse-v1（可选观测） | Release | 1,247,446 | 1,247,030 | 199,543 | 0.354 s | 0.264 s | 36.5 MB |

- 三个 profile 全部 `status=ok`；40k profile 的 Packed 产物 **1,232,408 bytes < 16 MiB**
  硬上限，容量门禁成立；未降低实体数量或替换样本。
- 与 R4（`4ebf244`）运行逐字节比较：三个 profile 的 `packedBytes`/`decodedBytes`
  **完全一致**，说明产物与文档/提交状态无关，确定性成立。
- R4 报告记录的"`implementationSha` 是运行时 HEAD 而非提交后 SHA"遗留项在本轮闭合：
  R4 数据（`4ebf244`）与 R5 数据（`25e546d`）分别作为独立证据保存，互不覆盖，且同字节。
- 峰值口径与不可测项沿用 R4：进程级 working set（Windows `PeakWorkingSetSize`，
  POSIX `getrusage`），只作观测；`notMeasured` 仍列出 `preparePeakBytes`（本阶段无 prepare
  实现）、typed-model profile 的 source bytes、decoded bytes 当作堆占用、IDN0 表对象内存。
- 本节数据在 Debug/Release 上都不作为硬门禁，硬门禁仍是 Spec §3.3 的字节/计数预算。
- **未执行**：POSIX `getrusage` 分支（需 Linux，留给 hosted）。

## 6. 未执行 / 受阻 / 未闭合项

| 项 | 状态 | 说明 |
| --- | --- | --- |
| 同 SHA hosted：Linux Quality、Windows MSVC、Windows MinGW（计划 R5.3） | **未执行（等待 owner 授权推送）** | 分支无 upstream，R0-R5 从未推送；推送/触发远端工作流须显式授权 |
| owner acceptance（计划 R5.5） | **未记录** | 需要 owner 明确接受后才能归档计划并恢复 Stage 6 |
| Linux POSIX 容量分支、sanitize/coverage/clang-tidy | 未执行（平台） | 只能在 Linux/hosted 上运行；Windows 上按仓库说明不可运行 |
| A16：CXT v2 展开不声明 capability `features`、不派生闭包 | 未闭合（Stage 6 决策） | 见 R4 报告 §1.1；E2E 显式注入 `CanonicalFeature{"cuexis.gameplay.candidate.lanes4", 1}` |
| 父图环路检测 O(n²)（每实体一次 visited 分配） | 观测项 | 本轮 40k decode：Debug 3.45 s / Release 0.179 s；未改算法 |
| 高复用/混合 profile | 可选观测，非门禁 | R4 已记录，本轮沿用 |
| GPU smoke | 未执行（本阶段不涉及渲染改动） | 不虚构 GPU 需求或结果 |

## 7. 命令与结果

```powershell
# 修复与验证 headless 生成
cmake --preset mingw-headless-debug --fresh -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build --preset mingw-headless-debug
ctest --preset mingw-headless-debug --no-tests=error
cmake --preset headless-debug --fresh
cmake --build --preset headless-debug
ctest --preset headless-debug --no-tests=error

# 非 headless 与 shared 回归（最终 SHA）
cmake --preset debug --fresh ;    cmake --build --preset debug    ; ctest --preset debug --no-tests=error
cmake --preset release --fresh ;  cmake --build --preset release --clean-first ; ctest --preset release --no-tests=error
cmake --preset shared-debug --fresh ; cmake --build --preset shared-debug ; ctest --preset shared-debug --no-tests=error

# 最终 SHA 容量复跑
$env:CUEXIS_RUN_PERFORMANCE_PROBE=1
ctest --preset debug   -R cuexis_chart_capacity --no-tests=error
ctest --preset release -R cuexis_chart_capacity --no-tests=error

# 用例覆盖边界补测（GCC + developer tools ON）
cmake -S . -B out/build/mingw-tools -G Ninja -DCMAKE_BUILD_TYPE=Debug `
  -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ `
  -DCMAKE_TOOLCHAIN_FILE=D:/vcpkg/scripts/buildsystems/vcpkg.cmake `
  -DVCPKG_TARGET_TRIPLET=x64-mingw-static -DVCPKG_MANIFEST_NO_DEFAULT_FEATURES=ON `
  -DVCPKG_MANIFEST_FEATURES=tests -DBUILD_TESTING=ON `
  -DCUEXIS_BUILD_PLAYER=OFF -DCUEXIS_BUILD_SDL_ADAPTER=OFF `
  -DCUEXIS_BUILD_AUDIO_SDL_ADAPTER=OFF -DCUEXIS_BUILD_OPENGL_ADAPTER=OFF `
  -DCUEXIS_BUILD_DEVELOPER_TOOLS=ON -DCUEXIS_BUILD_TESTS=ON
cmake --build out/build/mingw-tools --target cuexis_cxc_tests cuexis_chart_tests
.\out\build\mingw-tools\bin\cuexis_cxc_tests.exe
.\out\build\mingw-tools\bin\cuexis_chart_tests.exe

# 文档与格式门禁
cmake --build --preset debug --target cuexis_format_check
python -B tools/check_docs.py
python -B -m unittest discover -s tools -p "check_docs*tests.py"
git diff --check
```

| 项 | 结果 |
| --- | --- |
| Debug 全量 CTest | **683/683**（523.81 s） |
| Release 全量 CTest | **683/683**（362.76 s） |
| shared-debug 全量 CTest | **686/686**（489.35 s；3 个记录在案的跳过） |
| headless-debug 全量 CTest（MSVC） | **612/612**（608.70 s） |
| mingw-headless-debug 全量 CTest（GCC 16.1.0） | **612/612**（720.31 s） |
| GCC + developer tools 下编译/运行 | `cuexis_cxc_tests` 59/59、`cuexis_chart_tests` 210/210 通过（GCC 16.1.0） |
| 容量探针（Debug / Release，SHA `25e546d`） | 通过（44.53 s / 2.42 s；三个 profile `status=ok`） |
| `cuexis_format_check` | 通过（exit 0） |
| `check_docs.py` | 通过（231 Markdown + 20 candidate JSON/CXT） |
| `check_docs` 单元测试 | 6/6 通过 |
| `git diff --check` | 干净 |

## 8. 改动文件

- 实现：`tests/cxc/CMakeLists.txt`（A19 修复：条件化 candidate 源文件与工具层链接，
  configure 期显式报告排除）
- 证据：本报告、`2026-09-17-r5-capacity-data.json`、
  `2026-09-17-r5-capacity-data-release.json`、`out/evidence/r5/*.log`（本地，不入库）
- 文档：阶段报告 README、加固计划看板与状态行、`CURRENT_STATUS.md`、
  `docs/guides/BUILDING.md`（headless 覆盖边界说明）

## 9. R5 退出条件对照

| 计划条目 | 状态 | 证据 |
| --- | --- | --- |
| R5.1 Debug/Release、headless、架构、static/shared package 与安装头 external consumer | **完成（本地）** | §3.1：5 个配置全量绿色；架构与 7 个 consumer 用例在全部配置通过 |
| R5.2 旧 Chart/CXT/CXC、默认路由、合法 v4 identity/FrameDigest 回归；新增 hash 不改变既有 canonical bytes | **完成** | §4：fixtures/schemas 零改动；§4.2 逐用例；§4.3 SDK 边界 |
| R5.3 固定最终候选 SHA 并取得 Linux Quality / Windows MSVC / Windows MinGW 运行 | **未执行** | 需要 owner 授权推送；分支当前无 upstream |
| R5.4 实现/构建变化后在最终 SHA 重新验证，并按 report-SHA revalidation 记录 | **部分完成** | 实现 SHA `25e546d` 上的全量回归与容量复跑完成；报告提交后的 SHA 变化须在推送后用同一分支的 hosted 运行记录（见 §10） |
| R5.5 形成 completion report，逐项关闭 R0-R5，列出允许/禁止消费、identity/revision 政策、预算、残余与 Stage 6 接手命令，记录 owner acceptance | **报告完成；acceptance 待记录** | 本报告 §10、§11；owner 明确接受尚未取得 |
| R5.6 归档计划、Stage 6 恢复 active、同步状态/索引/路线图/旧路径映射/AGENTS/检查器 | **未触发** | 条件未满足；计划保持 active，Stage 6 保持 future |

## 10. 交接内容（供 owner acceptance 审定）

### 10.1 允许 Stage 6 消费

- `flags=1`、`candidateRevision=1` 的 Packed candidate 产物，且只能经
  `packed::decode`（唯一语义入口）读取；`packed::inspect` 仅做结构/预算/CRC 检查。
- 语义身份：typed preimage（域 `cuexis.chart.semantic.v5.candidate.1` + `NUL` + `u16(5)`）
  的 SHA-256，用于与 CXC `compiledSemanticIdentity` 比对；entry bytes 用
  `artifactIdentity` 独立比对。
- Foundation profile 子集：feature `cuexis.gameplay.candidate.lanes4`/1，domain
  `candidate.lanes4`，action `press`，恰好一个 lane 约束且 lane ∈ [0,3]，仅 tap/point，
  空 effects。
- 预算（Spec §3.3，tighten-only，`0` 是字面上限而非"无限"）：16 MiB 文件/section/decoded、
  40,000 实体与 requirements、100,000 字符串与引用。
- CXC candidate package 的只读消费：注册扩展 `cuexis.chart-entry.v1`，
  `playback=true` 的 entry 必须同包存在（D3）。
- 容量证据可作趋势与回归基线，不可当作"任意谱面 ≤ 16 MiB"的承诺。

### 10.2 禁止消费

- 不得把 candidate 当作 Chart v5 正式发行、默认 Writer 或公共 SDK 能力；SDK API 仍为
  `0.7.0`。
- 不得实现或消费 Hold/Release、Slide/Flick、多指，以及未登记的 Behavior/Animation/Effect
  section（`BEH0`/`BHD0`/`ANM0`/`FXS0` 明确拒绝）。
- 不得让 Packed Reader 执行 CXT，不得在 `engine/animation/` 增加 source parser。
- 不得基于 `inspect` 做语义判断（它不做 profile/identity 校验）。
- 不得放宽或绕过预算，不得把 `0` 解释为 unlimited，不得以计数替代语义比较。
- 不得在 `decode` 之外发布部分 `CanonicalSemanticChart`，不得为旧零 hash artifact 增加
  "零值即跳过"后门。
- 不得为运行时脚本/逐帧回调预留字段、capability 或执行 hook。

### 10.3 identity / revision 政策

- 保持 `flags=1`、`candidateRevision=1`；v4 identity 与 FrameDigest v1-v3 算法冻结不变。
- 语义身份在任何有效产物发布前计算并写入 header；读取时在结构、预算、注册表/CRC、
  payload、profile、identity 顺序校验后重算比对；不匹配返回稳定诊断，不发布部分结果。
- 语义内容变化必须改变 hash；格式允许的重排不得改变 hash；重复项不得排序后静默去重。

### 10.4 已知残余与接手命令

- 残余：A16（CXT capability 声明来源与闭包派生）需 Stage 6 决策；父图环路检测 O(n²)
  观测项；candidate CXC 用例的覆盖依赖 `CUEXIS_BUILD_DEVELOPER_TOOLS=ON`；hosted 三平台
  与 Linux 专项 gate 尚未在本分支运行。
- 接手命令：

```powershell
cmake --preset debug --fresh
cmake --build --preset debug
ctest --preset debug --no-tests=error
cmake --build --preset debug --target cuexis_format_check
python -B tools/check_docs.py
```

## 11. 后续动作

1. 取得 owner 对推送的明确授权后，推送
   `codex/chart-format-foundation-hardening`，在报告提交产生的 SHA 上运行 Linux Quality、
   Windows MSVC、Windows MinGW 三个 workflow，并把 workflow/run、SHA、工具链、关键命令
   与产物写入本报告的新增小节（不把旧 SHA 称为同 SHA 证据）。
2. 记录 owner 明确接受。
3. 只有第 1、2 步完成，才归档本计划、把 Stage 6 从 future 恢复 active，并同步
   `CURRENT_STATUS.md`、路线图、索引与 `AGENTS.md`。

Stage 6 仍为 future；R0-R5 的任何修复都没有为 Stage 6 提供新能力。

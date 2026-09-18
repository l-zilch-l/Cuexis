# Chart Format Foundation Hardening R5：回归、Hosted 与交接

- 批次：R5（回归矩阵、同 SHA hosted 记录、关闭报告）
- 起始 SHA：`eeda4e4`（R4 提交；开工前工作区干净、无未提交修改）
- 本轮实现 SHA：`9314646`（三个修复提交：`25e546d` headless 生成阻断 A19；
  `0e501a5` GCC `-Werror` 构建阻断 A20/A21；`9314646` A21 选项误用于 Clang 的 A22）
- hosted 运行（详见 §11.1）：
  - 首轮 `524db9f`：Windows MSVC 成功；Windows MinGW `release` 与 Linux Quality 的
    `GCC Release`/`GCC Shared Release` 因 A20 失败
  - 第二轮 `2cc478e`：Windows MSVC、Windows MinGW **成功**（A20 已闭合）；Linux Quality 的
    `GCC Release`/`GCC Shared Release` 转为**成功**，但两个 Clang sanitizer 任务因 A22 失败
  - 第三轮：含 A22 修复的 SHA 待推送复验
- 分支：`codex/chart-format-foundation-hardening`（已推送，upstream 为 origin 同名分支）
- 平台与工具链：Windows x64；MSVC 14.51.36231（VS 18 Community）；
  GCC 16.1.0（MSYS2 ucrt64，`x64-mingw-static`）；Clang 22.1.8（本机 clang++ 前端口检查）；
  Ninja；`VCPKG_ROOT=D:\vcpkg`
- 状态：**本地回归矩阵与容量复跑完成；hosted 验证进行中（A19/A20/A21/A22 已修复，待第三轮
  复验）；owner acceptance 未记录。** 因此本计划保持 active，Stage 6 保持 future，
  本报告不声明 R5 关闭。
- 容量数据：[Debug](2026-09-17-r5-capacity-data.json)、
  [Release](2026-09-17-r5-capacity-data-release.json)（均以实现 SHA `0e501a5` 运行；后续
  提交只改构建告警选项与文档，不改 chart 实现）

## 1. 结论摘要

1. 本轮发现并修复三个会**阻塞 hosted CI 的构建缺陷**：
   - **A19**（`tests/cxc` 无条件链接 developer tool 层）使所有 headless preset 在 generate
     阶段失败，即 Linux Quality 的 5 个 headless 配置全部无法配置；修复见 §2。
   - **A20/A21**（GCC 在 `-Werror` release 构建下被 `std::vector<std::byte>` 三向比较的
     伪 `-Wstringop-overread`、以及 libstdc++ `std::string` 拷贝路径的伪
     `-Wmaybe-uninitialized` 阻断）使 hosted Windows MinGW `release` 与 Linux Quality 的
     `GCC Release`/`GCC Shared Release` 构建失败；修复见 §2.5。首轮 hosted 已实测复现该失败。
   - **A22**（A21 的 GCC 专用降级选项被无条件传给 Clang，而 `headless-sanitize` 系列用
     `-Werror` 构建，Clang 以 unknown warning option 报错）使第二轮 hosted 的
     `Clang ASan + UBSan` 与 `Clang ASan + UBSan shader-tools` 失败；修复见 §2.6。
2. 本地回归矩阵 6 个配置全部绿色：Debug 683/683、Release 683/683、shared-debug
   686/686、headless-debug 612/612、mingw-headless-debug 612/612、
   GCC Release + `-Werror`（含 developer tools）628/628。
3. 兼容性回归满足 R5.2：自 R0 基线 `5085840` 起 **fixtures / schemas / canonical golden
   零改动**，v4 identity、FrameDigest v1-v3、Chart v4/CXT/CXC canonical bytes 相关用例
   全部通过；公共头保持纯 ASCII，`engine/chart` 未新增目标依赖。
4. 最终 SHA 容量复跑完成，且与 R4（`4ebf244`）及修复前（`25e546d`）的 wire 字节**完全一致**：
   low-reuse 40k = 1,232,408 bytes / decoded 1,232,024；CXT 阶梯 = 1,290 bytes；
   high-reuse 40k = 1,247,446 bytes，说明 A20/A21 的比较器改写对语义与 wire 完全中性。
5. 未闭合项：hosted 第三轮复验、owner acceptance、A16（CXT capability 声明来源，留给
   Stage 6）、父图环路 O(n²) 观测项。

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

### 2.5 A20/A21：GCC 在 `-Werror` release 构建下的两个阻断

首轮推送（报告 SHA `524db9f`）的 hosted 运行实测结果：Windows MSVC **成功**；Windows
MinGW `release` 与 Linux Quality 的 `GCC Release`、`GCC Shared Release` 在 Build 步骤
**失败**（其余 Linux 任务，含 Clang ASan+UBSan 与 Clang Shared Debug，均通过）。这三处失败
的共同点是 `CUEXIS_WARNINGS_AS_ERRORS=ON` 的 GCC 构建。

**A20：`std::vector<std::byte>` 三向比较的伪 `-Wstringop-overread`**

- 复现（本地 GCC 16.1.0，Release + `-Werror`）：
  `engine/chart/src/packed_chart_tables.cpp` 与 `packed_semantic_identity.cpp` 编译失败：
  ```text
  stl_algobase.h:1894:39: error: 'int __builtin_memcmp(const void*, const void*, long long
  unsigned int)' specified bound [9223372036854775808, 18446744073709551615] exceeds maximum
  object size 9223372036854775807 [-Werror=stringop-overread]
  ```
- hosted 复现：run `35253027366`（Windows MinGW / MSYS2 GCC **16.2.0**）与 run
  `35253027313`（Linux Quality / GCC **13**）在同一批 TU 上报同一诊断，TU 分别为
  `engine/chart/src/packed_semantic_identity.cpp`、`packed_chart_tables.cpp`。
- 根因：C++20 `operator<=>` 作用于 `std::vector<std::byte>` 时，libstdc++ 走
  `lexicographical_compare_three_way` → `__builtin_memcmp`，GCC 无法界定其长度上界，
  于是报出伪诊断；Debug（无优化）与 MSVC 不报。
- 修复：新增内部比较器 `identity_detail::ByteKeyLess`（`packed_identity_internal.hpp`），
  按"无符号字节值升序、再按长度"显式逐字节比较，排序语义与 `std::less<vector<byte>>`
  完全一致；用于实体排序与 ordinal 映射（`packed_chart_tables.cpp`）、Archetype 默认值
  选择表、语义预映像的 identity 集合/排序/parent 与 visited 映射
  （`packed_semantic_identity.cpp`）。

**A21：libstdc++ `std::string` 拷贝路径的伪 `-Wmaybe-uninitialized`**

- 复现：同一 `-Werror` 构建在 `tests/chart/packed_profile_rejection_tests.cpp` 与
  `tests/chart/chart_capacity_probe.cpp` 上失败：
  ```text
  basic_string.h:298:21: error: '..._M_allocated_capacity' may be used uninitialized
  [-Werror=maybe-uninitialized]
  ```
  诊断回溯链落在 `CanonicalEntity` 拷贝构造 → `std::vector<CanonicalEntity>` 的
  `uninitialized_copy` → libstdc++ `basic_string`，指针落在标准库头而非项目代码。
- 根因：GCC 高位优化下的已知伪阳性（SSO 分支的 `_M_allocated_capacity` 实际不会被读取）。
- 修复：在 `cmake/CuexisWarnings.cmake` 中，对 `_tests$` / `_probe$` 目标追加
  `-Wno-error=maybe-uninitialized`（仅降级该诊断，仍作为普通 warning 输出；真实 finding
  在其它诊断上继续 `-Werror` 失败）。顺带把原先只匹配 `_performance_probe$` 的模式扩展为
  `_probe$`，使 R4 新增的 `cuexis_chart_capacity_probe` 与既有探针享受同一测试期策略。

**验证**（均在实现 SHA `0e501a5`）：

| 项 | 结果 |
| --- | --- |
| GCC 16.1.0 Release + `-Werror` 全量构建（`out/build/mingw-werror`，含 developer tools） | **成功**（此前同一命令在 A20 处失败） |
| 同一树全量 CTest | **628/628 通过** |
| MSVC Debug/Release/shared-debug/headless-debug 全量 | 683/683、683/683、686/686、612/612 |
| GCC MinGW headless 全量 | 612/612 |
| 容量 wire 字节 | 与修复前逐字节一致（§5），证明排序语义未变 |
| `-Wno-error` 生效性验证 | `g++ -Wall -Wno-error=unused-variable -Werror` 下该诊断仍为 warning，确认按诊断降级与选项顺序无关 |

### 2.6 A22：GCC 专用告警降级被误用于 Clang

- 复现：第二轮 hosted（report SHA `2cc478e`）的 Linux Quality 中，`Clang ASan + UBSan` 与
  `Clang ASan + UBSan shader-tools` 在 Build 步骤失败：
  ```text
  error: unknown warning option '-Werror=maybe-uninitialized'; did you mean
  '-Werror=uninitialized'? [-Werror,-Wunknown-warning-option]
  ```
  触发者是 `/usr/bin/clang++` 编译命令（例如 `engine/core` 与 chart 测试目标）。第一轮
  hosted（无该选项）中两个任务是通过的，因此这是 A21 修复自身引入的回归。
- 根因：A21 的 `-Wno-error=maybe-uninitialized` 放在非 MSVC 分支里，没有区分 GCC 与 Clang；
  Clang 没有 `-Wmaybe-uninitialized`，把未知告警选项视为诊断，而 `headless-sanitize` 与
  `headless-sanitize-shader-tools` 预设设置了 `CUEXIS_WARNINGS_AS_ERRORS=ON`，于是该诊断
  升级为构建失败。
- 修复：把该选项放进 `if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")`；Clang 不获得任何 GCC
  专用选项，`-Wno-missing-field-initializers` 保持不变（Clang 支持）。
- 验证：
  | 检查 | 结果 |
  | --- | --- |
  | 最小 CMake 探针（同一 `cuexis_enable_warnings`，两个编译器各配置一次） | GCC：`-Werror` + `-Wno-error=maybe-uninitialized` + `-Wno-missing-field-initializers`；Clang：`-Werror` + `-Wno-missing-field-initializers`，**无** `maybe-uninitialized` |
  | Clang 编译最小探针目标（`-Werror`） | 成功（若选项仍传入会直接报 unknown warning option） |
  | `clang++ -std=c++20 -Wall -Wextra -Wpedantic -Werror` 编译 `packed_chart_tables.cpp`、`packed_semantic_identity.cpp`、`chart_capacity_probe.cpp` | 三个 TU 全部 0 warning 通过（Clang 22.1.8） |
  | GCC 16.1.0 Release + `-Werror` 全量重建 | 成功（GCC 仍获得该降级选项） |
  | MSVC `debug` 重新配置 + 重建 | 成功（`if(MSVC)` 分支未改动，行为不变） |
  | GCC 全量 CTest（`mingw-headless-debug`、`mingw-werror`） | 见 §3.1（A22 修复后重跑） |
- 说明：A22 只改构建告警选项，不改 chart 实现，因此 `0e501a5` 上的容量数据与语义结论继续
  成立；第三轮 hosted 用于确认 Clang 任务恢复绿色。

## 3. 本地回归矩阵

所有结果均为实现 SHA `0e501a5` 上的产物；A19/A20/A21 修复后构建输入已变化，因此 `25e546d`
时期的运行被下表取代，符合计划 R5.4。日志保存在 `out/evidence/r5/`。表中耗时含并行执行
造成的相互拖慢（例如 Debug 与 MinGW 同时运行），只作参考，不作门禁。

### 3.1 六个配置的全量 CTest

| preset | 库类型 / developer tools | 用例数 | 结果 | 耗时 | 跳过 |
| --- | --- | --- | --- | --- | --- |
| `debug` | static / ON | 683 | **683/683 通过** | 915.75 s | 1（symlink 平台用例） |
| `release` | static / ON | 683 | **683/683 通过** | 350.28 s | 1（同上） |
| `shared-debug` | `CUEXIS_LIBRARY_TYPE=SHARED` / ON | 686 | **686/686 通过** | 371.08 s | 3（见 §3.2） |
| `headless-debug` | static / **OFF** | 612 | **612/612 通过** | 316.18 s | 1（symlink） |
| `mingw-headless-debug` | GCC 16.1.0 ucrt64 / **OFF** | 612 | **612/612 通过** | 921.05 s | 1（symlink） |
| `mingw-werror`（自建：GCC Release + `-Werror`，player/SDL/GL OFF） | GCC 16.1.0 / **ON** | 628 | **628/628 通过** | 1127.63 s | 1（symlink） |

- `shared-debug` 比 static 多 3 个 `shared` 标签用例（共享库拓扑专项）。
- A22 修复只改非 MSVC 目标的告警选项（GCC 侧选项不变、Clang 侧移除该选项），因此 GCC 两行
  在 `9314646` 上重跑确认：`mingw-headless-debug` **612/612**（602.41 s）、
  `mingw-werror` **628/628**（528.05 s）；MSVC 重新配置与重建通过、`if(MSVC)` 分支未改动。
- Clang 侧以本机 `clang++` 22.1.8 直接编译 `packed_chart_tables.cpp`、
  `packed_semantic_identity.cpp`、`chart_capacity_probe.cpp`，`-Wall -Wextra -Wpedantic
  -Werror` 零告警；并以最小 CMake 探针确认 GCC 收到而 Clang 不收到 GCC 专用选项（§2.6）。
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
| GCC Release + `-Werror`（`out/build/mingw-werror`，developer tools ON） | 全量构建成功、628/628 用例通过，即 A20/A21 修复后候选用例同样通过 `-Werror` 门禁 |
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
工作区、HEAD = `0e501a5`（A20/A21 修复后）时复跑，机器可读结果保存为
[Debug](2026-09-17-r5-capacity-data.json) 与
[Release](2026-09-17-r5-capacity-data-release.json)。

| Profile | 构建 | Packed bytes | decoded bytes | IDN0 bytes | encode | decode | peakDelta |
| --- | --- | --- | --- | --- | --- | --- | --- |
| low-reuse-v1（40k 实体） | Debug | 1,232,408 | 1,232,024 | 680,012 | 3.37 s | 3.04 s | 50.6 MB |
| low-reuse-v1（40k 实体） | Release | 1,232,408 | 1,232,024 | 680,012 | 0.194 s | 0.191 s | 20.1 MB |
| cxt-stair-ladder（16 实体） | Debug | 1,290 | 874 | 115 | 2.7 ms | 1.8 ms | 0 |
| cxt-stair-ladder（16 实体） | Release | 1,290 | 874 | 115 | 0.13 ms | 0.11 ms | 0 |
| high-reuse-v1（可选观测） | Debug | 1,247,446 | 1,247,030 | 199,543 | 11.61 s | 7.82 s | 96.8 MB |
| high-reuse-v1（可选观测） | Release | 1,247,446 | 1,247,030 | 199,543 | 0.522 s | 0.370 s | 36.5 MB |

- 三个 profile 全部 `status=ok`；40k profile 的 Packed 产物 **1,232,408 bytes < 16 MiB**
  硬上限，容量门禁成立；未降低实体数量或替换样本。
- 与 R4（`4ebf244`）及修复前（`25e546d`）运行逐字节比较：三个 profile 的
  `packedBytes`/`decodedBytes` **完全一致**，说明产物与提交状态无关、且 A20/A21 的比较器
  改写对 wire 完全中性，确定性成立。
- 耗时在不同运行间存在明显波动（同机同 SHA 的 Debug low-reuse encode 在 3.37–4.07 s 之间，
  high-reuse 6.3–11.6 s 之间，主要受并行负载影响），这正是不把耗时/峰值当硬门禁的原因。
- R4 报告记录的"`implementationSha` 是运行时 HEAD 而非提交后 SHA"遗留项在本轮闭合：
  R4 数据（`4ebf244`）与 R5 数据（`0e501a5`）分别作为独立证据保存，互不覆盖，且同字节。
- 峰值口径与不可测项沿用 R4：进程级 working set（Windows `PeakWorkingSetSize`，
  POSIX `getrusage`），只作观测；`notMeasured` 仍列出 `preparePeakBytes`（本阶段无 prepare
  实现）、typed-model profile 的 source bytes、decoded bytes 当作堆占用、IDN0 表对象内存。
- 本节数据在 Debug/Release 上都不作为硬门禁，硬门禁仍是 Spec §3.3 的字节/计数预算。
- **未执行**：POSIX `getrusage` 分支（需 Linux，留给 hosted）。

## 6. 未执行 / 受阻 / 未闭合项

| 项 | 状态 | 说明 |
| --- | --- | --- |
| 同 SHA hosted：Linux Quality、Windows MSVC、Windows MinGW（计划 R5.3） | **已尝试、未通过；修复待重推复验** | 首轮 run `35253027313`/`35253027357`/`35253027366`（SHA `524db9f`）：MSVC 成功，MinGW `release` 与 Linux `GCC Release`/`GCC Shared Release` 因 A20 在 Build 失败；修复见 §2.5，SHA `0e501a5` 待推送复验 |
| owner acceptance（计划 R5.5） | **未记录** | 需要 owner 明确接受后才能归档计划并恢复 Stage 6 |
| Linux POSIX 容量分支、sanitize/coverage/clang-tidy | 未在本机执行（平台） | 只能在 Linux/hosted 上运行；首轮 hosted 中 Clang ASan+UBSan、Clang Shared Debug、coverage、clang-tidy 任务均通过 |
| A16：CXT v2 展开不声明 capability `features`、不派生闭包 | 未闭合（Stage 6 决策） | 见 R4 报告 §1.1；E2E 显式注入 `CanonicalFeature{"cuexis.gameplay.candidate.lanes4", 1}` |
| 父图环路检测 O(n²)（每实体一次 visited 分配） | 观测项 | 本轮 40k decode：Debug 3.32 s / Release 0.190 s；未改算法 |
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

# GCC Release + -Werror（复现 A20/A21 并验证修复）
cmake -S . -B out/build/mingw-werror -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ `
  -DCMAKE_TOOLCHAIN_FILE=D:/vcpkg/scripts/buildsystems/vcpkg.cmake `
  -DVCPKG_TARGET_TRIPLET=x64-mingw-static -DVCPKG_MANIFEST_NO_DEFAULT_FEATURES=ON `
  -DVCPKG_MANIFEST_FEATURES=tests -DBUILD_TESTING=ON `
  -DCUEXIS_BUILD_PLAYER=OFF -DCUEXIS_BUILD_SDL_ADAPTER=OFF `
  -DCUEXIS_BUILD_AUDIO_SDL_ADAPTER=OFF -DCUEXIS_BUILD_OPENGL_ADAPTER=OFF `
  -DCUEXIS_BUILD_DEVELOPER_TOOLS=ON -DCUEXIS_BUILD_TESTS=ON -DCUEXIS_WARNINGS_AS_ERRORS=ON
cmake --build out/build/mingw-werror
ctest --test-dir out/build/mingw-werror --no-tests=error

# 文档与格式门禁
cmake --build --preset debug --target cuexis_format_check
python -B tools/check_docs.py
python -B -m unittest discover -s tools -p "check_docs*tests.py"
git diff --check
```

| 项 | 结果 |
| --- | --- |
| Debug 全量 CTest | **683/683**（915.75 s，含并行拖慢） |
| Release 全量 CTest | **683/683**（350.28 s） |
| shared-debug 全量 CTest | **686/686**（371.08 s；3 个记录在案的跳过） |
| headless-debug 全量 CTest（MSVC） | **612/612**（316.18 s） |
| mingw-headless-debug 全量 CTest（GCC 16.1.0） | **612/612**（921.05 s，含并行拖慢） |
| GCC Release + `-Werror` 全量构建（含 developer tools） | **成功**（A20/A21 修复前失败） |
| GCC Release + `-Werror` 全量 CTest | **628/628**（1127.63 s） |
| GCC + developer tools 下编译/运行 | `cuexis_cxc_tests` 59/59、`cuexis_chart_tests` 210/210 通过（GCC 16.1.0） |
| 容量探针（Debug / Release，SHA `0e501a5`） | 通过（61.78 s / 3.10 s；三个 profile `status=ok`） |
| 首轮 hosted（SHA `524db9f`） | MSVC 成功；MinGW `release`、Linux `GCC Release`、`GCC Shared Release` 因 A20 失败（§2.5、§9） |
| `cuexis_format_check` | 通过（exit 0） |
| `check_docs.py` | 通过（232 Markdown + 20 candidate JSON/CXT） |
| `check_docs` 单元测试 | 6/6 通过 |
| `git diff --check` | 干净 |

## 8. 改动文件

- 实现（A19）：`tests/cxc/CMakeLists.txt`（条件化 candidate 源文件与工具层链接，
  configure 期显式报告排除）
- 实现（A20/A21）：`engine/chart/src/packed_identity_internal.hpp`（新增
  `identity_detail::ByteKeyLess`）、`engine/chart/src/packed_chart_tables.cpp`、
  `engine/chart/src/packed_semantic_identity.cpp`、`cmake/CuexisWarnings.cmake`
- 证据：本报告、`2026-09-17-r5-capacity-data.json`、
  `2026-09-17-r5-capacity-data-release.json`、`out/evidence/r5/*.log`（本地，不入库）
- 文档：阶段报告 README、加固计划看板与状态行、`CURRENT_STATUS.md`、
  `docs/guides/BUILDING.md`（headless 覆盖边界说明）

## 9. R5 退出条件对照

| 计划条目 | 状态 | 证据 |
| --- | --- | --- |
| R5.1 Debug/Release、headless、架构、static/shared package 与安装头 external consumer | **完成（本地）** | §3.1：6 个配置全量绿色；架构与 7 个 consumer 用例在全部配置通过 |
| R5.2 旧 Chart/CXT/CXC、默认路由、合法 v4 identity/FrameDigest 回归；新增 hash 不改变既有 canonical bytes | **完成** | §4：fixtures/schemas 零改动；§4.2 逐用例；§4.3 SDK 边界 |
| R5.3 固定最终候选 SHA 并取得 Linux Quality / Windows MSVC / Windows MinGW 运行 | **进行中；两个平台已绿、Linux 待第三轮** | 首轮 `524db9f`：MSVC `35253027357` 成功；Linux `35253027313` 与 MinGW `35253027366` 因 A20 失败。第二轮 `2cc478e`：MSVC `35258433530`、MinGW `35258433516` 成功（A20 闭合）；Linux `35258433569` 的 GCC Release/GCC Shared Release 转为成功，Clang sanitizer 两项因 A22 失败。A22 已在 `9314646` 修复，第三轮运行见 §11.1 |
| R5.4 实现/构建变化后在最终 SHA 重新验证，并按 report-SHA revalidation 记录 | **进行中** | 每次实现变化后都重建并在新 SHA 重跑：`25e546d` 与 `0e501a5` 各有完整矩阵与容量复跑；报告提交 SHA 与实现 SHA 的差异只允许落在 `docs/`，并在 §11 记录复验运行 |
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
  观测项；candidate CXC 用例的覆盖依赖 `CUEXIS_BUILD_DEVELOPER_TOOLS=ON`；hosted 三平台的
  最终同 SHA 复验仍在进行（§11.1），Linux 的 sanitizer/coverage/clang-tidy 只在 hosted 执行。
- 接手命令：

```powershell
cmake --preset debug --fresh
cmake --build --preset debug
ctest --preset debug --no-tests=error
cmake --build --preset debug --target cuexis_format_check
python -B tools/check_docs.py
```

## 11. 后续动作

1. 推送含 A22 修复（`9314646`）与本次报告更新的分支，在**新的报告 SHA** 上完成第三轮
   Linux Quality、Windows MSVC、Windows MinGW，并把 run、SHA、工具链与关键命令补记到
   §11.1。
2. 记录 report-SHA revalidation：实现 SHA 与记录用报告 SHA 之间只允许 `docs/` 变化
   （§11.3），不把早期 SHA 的结果当作修复后证据。
3. 记录 owner 明确接受（用户于本轮指示"修复后推送、hosted 全绿后继续完成 R5，不开 PR"，
   仍以 §10 的交接清单为准取得最终 acceptance）。
4. 只有第 1-3 步完成，才归档本计划、把 Stage 6 从 future 恢复 active，并同步
   `CURRENT_STATUS.md`、路线图、索引与 `AGENTS.md`。

### 11.1 hosted 运行记录

| 轮次 | SHA | run | 结果 |
| --- | --- | --- | --- |
| 首轮（A20 修复前） | `524db9f` | Linux Quality `35253027313` | **失败**：`GCC Release`、`GCC Shared Release` 在 Build 步骤因 `-Werror=stringop-overread` 失败（GCC 13）；其余任务（clang-tidy、Documentation contracts、GCC Coverage、Clang ASan+UBSan、Clang Shared Debug、Adapter/Shader Tools Coverage）通过 |
| 首轮（A20 修复前） | `524db9f` | Windows MSVC `35253027357` | **成功**（矩阵全部通过） |
| 首轮（A20 修复前） | `524db9f` | Windows MinGW `35253027366` | **失败**：`release` 在 Build 步骤因同一诊断失败（MSYS2 GCC 16.2.0），`debug` 通过 |
| 第二轮（A20/A21 已修复，A22 未修复） | `2cc478e` | Windows MSVC `35258433530` | **成功** |
| 第二轮 | `2cc478e` | Windows MinGW `35258433516` | **成功**：`release` 与 `debug` 均通过，A20 闭合 |
| 第二轮 | `2cc478e` | Linux Quality `35258433569` | **失败**：`GCC Release`、`GCC Shared Release` 转为**成功**（A20 闭合），但 `Clang ASan + UBSan` 与 `Clang ASan + UBSan shader-tools` 在 Build 步骤因 A22（`unknown warning option '-Werror=maybe-uninitialized'`）失败；其余任务通过 |
| 第三轮（A22 已修复） | 本轮报告 SHA | 待运行 | 待填写 |

首轮失败不是环境问题：本地 GCC 16.1.0 与 hosted GCC 13 / 16.2.0 在相同 TU 上报出相同诊断，
修复后本地同类 `-Werror` 构建与全量用例均已通过。第二轮 Linux 的 A22 同样是可复现的构建
配置缺陷，已在本地以 Clang 22.1.8 复现同类选项拒绝并验证修复（§2.6）。

### 11.2 本轮内的 SHA 变化与重验证

| 阶段 | SHA | 触发 | 处理 |
| --- | --- | --- | --- |
| A19 修复 | `25e546d` | headless 生成阻断 | 五个配置全量 CTest + 容量复跑 |
| A20/A21 修复 | `ea71bd0` | GCC `-Werror` release 构建阻断 | 六个配置全量 CTest + 容量复跑 |
| 同一修复的 clang-format 规范化（仅 `packed_chart_tables.cpp` 一处换行） | `0e501a5`（amend） | `cuexis_format_check` 要求 | 六个配置重建并重跑全量 CTest，容量探针在 `0e501a5` 重跑且 wire 字节不变（本报告所有数据均取自该 SHA） |
| A22 修复（GCC 专用选项只给 GNU） | `9314646` | 第二轮 hosted 的 Clang sanitizer 任务失败 | 按编译器分别验证选项分派；GCC Release + `-Werror` 全量重建、MSVC 重新配置与重建、Clang 22.1.8 编译三个受影响 TU（含容量探针）全部通过；GCC 两套全量 CTest 重跑见 §3.1 |

即：每次实现或构建输入变化都在同一轮内于新 SHA 重新构建、重跑全量用例并重跑容量探针，
未用旧 SHA 运行替代修复后证据。报告提交导致的 SHA 变化只允许落在 `docs/`（§11.3 记录其
范围与验证）。

Stage 6 仍为 future；R0-R5 的任何修复都没有为 Stage 6 提供新能力。

### 11.3 report-SHA revalidation

- hosted 运行总是针对被推送的分支头 SHA。记录证据本身会产生新的提交，因此仓库惯例是
  记录实现 SHA 与报告 SHA，并证明两者之间只有 `docs/` 变化（`git diff --name-only
  <实现 SHA>..<报告 SHA>` 中非 `docs/` 条目数为 0）。
- 本轮每一轮 hosted 使用的 SHA 都已用该检查确认：`2cc478e` 相对实现 SHA `0e501a5` 只改
  `docs/`（6 个文件）；第三轮的报告 SHA 相对实现 SHA `9314646` 同样只改 `docs/`，其
  非 `docs/` 条目数在提交后记录为本节数据。
- 因此报告的结论是"实现 SHA X 的代码 + 仅文档差异"，而不是把更早 SHA 的运行当作修复后
  证据；每次实现改动后都在新 SHA 重跑本地全量与容量探针（§11.2）。

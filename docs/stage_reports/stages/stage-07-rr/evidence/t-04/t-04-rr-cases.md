# T-04 RR-F216 / RR-F217 / RR-F434 验收证据

状态：evidence

<!-- cuexis-meta
id: EVIDENCE-stage-07-rr.t-04-rr-cases
status: evidence
authority: none
date: 2026-11-24
-->

- 仓库：`E:\Cuexis-worktree`
- 提交：`92539ad4`（`git status --short` 与 `git diff --stat` 均为空）
- 日期：2026-11-24
- 主机：Windows x64；CMake 4.3.3；Ninja；MSVC 19.51.36256
- 统一命令前缀：

```text
cmd /d /c "call "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && <命令>"
```

本轮只写 `out/` 和新建的 `out/build/rr-f216-*`，未修改生产源码，未执行 `git add` 或 `git commit`。

## 结论

| 项 | 判定 | 本机依据 | 无法核实或未做 |
| --- | --- | --- | --- |
| RR-F216 | 成立（Windows 可核实范围） | 三个全新目录配置：默认不传库类型和显式 STATIC 均通过；显式 SHARED 退出码 1，首条错误为 `PD allocation observer requires Windows STATIC` | 非 Windows 分支无法在本机核实 |
| RR-F217 | 成立 | GL ON / shader tools OFF 的 `out/build/debug` 构建 `cuexis_shader_tests` 成功；`ctest -N` 注册 6 个 cache 用例，实际运行 6/6 通过；codemodel 显示源码只有 `shader_cache_tests.cpp`，链接边无 shader compiler 源、无 `cuexis_shader`、无 shaderc | 本机无 tools ON + tests ON + GL ON 的现成配置，未做 compiler/cache 对照 |
| RR-F434 | 成立 | 已有 Playback 摘要用例注册 2 个，实际运行 2/2 通过；版本 0 和 4 被 `playback.frame_digest.version_unsupported` 拒绝，公共默认 v3 与历史 v1/v2 的版本号和摘要值保持不变 | 无 |

## RR-F216 库类型与 observer 初始化顺序

判定：成立（Windows 主机可核实范围）。非 Windows 分支无法在本机核实。

代码核对：

- `CMakeLists.txt:32` 在 observer 门禁之前 include `cmake/CuexisLibraryType.cmake`。
- `cmake/CuexisLibraryType.cmake:6-8` 把 `CUEXIS_LIBRARY_TYPE` 以 `STATIC` 初始化到缓存，并设置可选取值 `STATIC;SHARED`。
- `CMakeLists.txt:98-103` 的 observer 门禁读取 `CUEXIS_LIBRARY_TYPE`；Windows 上显式 `SHARED` 会触发 `CMakeLists.txt:100` 的 `PD allocation observer requires Windows STATIC`。
- `CMakeLists.txt:99` 的条件是 `NOT WIN32 OR NOT CUEXIS_LIBRARY_TYPE STREQUAL "STATIC"`，非 Windows 分支只在非 Windows 主机上执行。

三个配置都放在新目录并带 `--fresh`，没有使用任何旧构建目录的缓存值。

### 配置 1：observer ON，不显式传库类型

目录：`out/build/rr-f216-default`

```text
cmd /d /c "call "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && cmake --preset headless-debug -B out/build/rr-f216-default --fresh -DCUEXIS_ALLOCATION_LIFETIME_OBSERVER=ON"
```

退出码：0

生成的缓存值：

```text
CUEXIS_ALLOCATION_LIFETIME_OBSERVER:BOOL=ON
CUEXIS_LIBRARY_TYPE:STRING=STATIC
CUEXIS_BUILD_TESTS:BOOL=ON
```

结果：配置完成，输出包含 `Configuring done` 与 `Build files have been written`。默认 STATIC 在 observer 门禁读取之前已经解析完成。

### 配置 2：observer ON，显式 STATIC

目录：`out/build/rr-f216-explicit-static`

```text
cmd /d /c "call "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && cmake --preset headless-debug -B out/build/rr-f216-explicit-static --fresh -DCUEXIS_ALLOCATION_LIFETIME_OBSERVER=ON -DCUEXIS_LIBRARY_TYPE=STATIC"
```

退出码：0

生成的缓存值：

```text
CUEXIS_ALLOCATION_LIFETIME_OBSERVER:BOOL=ON
CUEXIS_LIBRARY_TYPE:STRING=STATIC
CUEXIS_BUILD_TESTS:BOOL=ON
```

结果：配置完成。

### 配置 3：observer ON，显式 SHARED

目录：`out/build/rr-f216-explicit-shared`

```text
cmd /d /c "call "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && cmake --preset headless-debug -B out/build/rr-f216-explicit-shared --fresh -DCUEXIS_ALLOCATION_LIFETIME_OBSERVER=ON -DCUEXIS_LIBRARY_TYPE=SHARED"
```

退出码：1

首条错误：

```text
CMake Error at CMakeLists.txt:100 (message):
  PD allocation observer requires Windows STATIC
```

失败配置仍写出的缓存值：

```text
CUEXIS_ALLOCATION_LIFETIME_OBSERVER:BOOL=ON
CUEXIS_LIBRARY_TYPE:STRING=SHARED
```

结果：配置被拒绝，输出包含 `Configuring incomplete, errors occurred!`。拒绝原因是 observer 的 Windows/STATIC 准入条件，命令使用的变量名来自实际 CMake 规则。

### 无法核实的点

非 Windows 分支无法在本机核实。`CMakeLists.txt:99` 的 `NOT WIN32` 项在 Windows 主机上不执行；要核实需在非 Windows 主机上配置。没有通过 `-DWIN32=...` 覆盖来模拟，因为该平台变量由 CMake 按目标系统设置，命令行覆盖不能代表真实非 Windows 配置。

证据：

- `rr-f216-default-configure.log`
- `rr-f216-explicit-static-configure.log`
- `rr-f216-explicit-shared-configure.log`
- [rr-f216-summary.log](rr-f216-summary.log)

## RR-F217 shader cache 测试接线

判定：成立。

使用的配置是 `out/build/debug`，缓存值为：

```text
CUEXIS_BUILD_OPENGL_ADAPTER:BOOL=ON
CUEXIS_BUILD_SHADER_TOOLS:BOOL=OFF
CUEXIS_BUILD_TESTS:BOOL=ON
CUEXIS_LIBRARY_TYPE:STRING=STATIC
```

GL ON 使 `engine/shader/CMakeLists.txt:31-37` 创建 `cuexis_shader_cache`；shader tools OFF 使 `engine/shader/CMakeLists.txt:68-103` 不创建 `cuexis_shader`。`tests/CMakeLists.txt:30-32` 以 `TARGET cuexis_shader_cache` 为条件加入 shader 测试目录，`tests/shader/CMakeLists.txt:4-18` 以 `cuexis_shader_cache` 单独编入 `shader_cache_tests.cpp`。

### 构建

```text
cmd /d /c "call "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && cmake --build --preset debug --target cuexis_shader_tests"
```

退出码：0。最终构建日志包含 9 个构建步骤，并成功链接 `bin\cuexis_shader_tests.exe`。

### 注册与运行

先确认非零注册：

```text
cmd /d /c "call "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && ctest --test-dir out/build/debug --no-tests=error -N -R "^(S5-F|A2) .*[Cc]ache""
```

退出码：0，注册 6 个用例：

```text
Test #270: S5-F cache key changes with identity, profile, keyword, entry, and tools
Test #271: A2 cache key normalizes profiles, keywords, entries, and tools
Test #272: A2 cache store rejects an invalid record without creating its directory
Test #273: A2 cache rejects a nonempty keyword set without semantic source identity
Test #275: A2 cache key filename uses lowercase hexadecimal
Test #276: S5-F freezes cache diagnostic codes and tool identities
Total Tests: 6
```

实际运行：

```text
cmd /d /c "call "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && ctest --test-dir out/build/debug --no-tests=error --output-on-failure -R "^(S5-F|A2) .*[Cc]ache""
```

退出码：0，结果为 `100% tests passed, 0 tests failed out of 6`。

表达式使用 `^(S5-F|A2) .*[Cc]ache`，限定在 shader 测试用例名前缀上，避免把其他目标里仅名字含 cache 的 Playback/PD 用例混入。`tests/shader/shader_cache_tests.cpp:208-484` 中依赖 `ShaderCompiler` 的 cache 用例受 `CUEXIS_SHADER_COMPILER_AVAILABLE` 保护；tools OFF 时该宏未定义，这些用例不编入，符合目标条件选择编译源的接线设计。

### 源码与链接边检查

codemodel 已通过已有的 `.cmake/api/v1/query/codemodel-v2` 查询重新生成：

```text
cmd /d /c "call "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && cmake --preset debug"
```

退出码：0。检查命令如下，结果写入 `rr-f217-codemodel-check.log`：

```powershell
Get-Content out/build/debug/.cmake/api/v1/reply/target-cuexis_shader_tests-Debug-*.json |
  ConvertFrom-Json | Select-Object type, sources, linkLibraries, dependencies
Select-String -Path out/build/debug/build.ninja `
  -Pattern '^build bin\\cuexis_shader_tests\.exe' -Context 0,5
```

解析 `.cmake/api/v1/reply/index-*.json` 及其 `cuexis_shader_tests` target JSON 后得到：

```text
target cuexis_shader_tests type: EXECUTABLE
sources:
  tests/shader/shader_cache_tests.cpp
direct linkLibraries:
  cuexis_shader_cache
  Catch2::Catch2WithMain
dependencies:
  cuexis_core
  cuexis_shader_cache
```

`cuexis_shader_cache` 自身只编译 `engine/shader/src/shader_cache.cpp`、`engine/shader/src/shader_reflection.cpp` 并链接 `cuexis_core`。`shader_compiler.hpp` 作为 cache 的公共头文件出现在 FILE_SET 中，不参与编译；`src/shader_compiler*.cpp` 与 `src/shader_pipeline_cache.cpp` 没有出现。当前 codemodel 中名字含 `shader` 的目标只有 `cuexis_shader_cache` 和 `cuexis_shader_tests`，没有 `cuexis_shader`。

`build.ninja` 的 `cuexis_shader_tests.exe` 链接边为：

```text
build bin\cuexis_shader_tests.exe ... : CXX_EXECUTABLE_LINKER__cuexis_shader_tests_Debug tests\shader\CMakeFiles\cuexis_shader_tests.dir\shader_cache_tests.cpp.obj | lib\cuexis_shader_cache.lib ... Catch2Maind.lib lib\cuexis_core.lib ... Catch2d.lib ...
  LINK_LIBRARIES = lib\cuexis_shader_cache.lib ... Catch2Maind.lib ... lib\cuexis_core.lib ... glm.lib ... Catch2d.lib ...
```

在该链接边中 `shaderc` 出现 0 次，`cuexis_shader.lib` 出现 0 次；目标源码中 `shader_compiler` 出现 0 次。target JSON 的编译定义只有 `CUEXIS_CORE_STATIC_DEFINE`、`NOMINMAX`、`WIN32_LEAN_AND_MEAN`，没有 `CUEXIS_SHADER_COMPILER_AVAILABLE`。

### tools ON 对照

扫描 `out/build` 下现有 10 个 `CMakeCache.txt`，全部为 `CUEXIS_BUILD_SHADER_TOOLS:BOOL=OFF`；`out/build/debug-shader-tools` 不存在。`candidate-headless-debug` 的 GL 也是 OFF，`rr-f302-tools` 的 tests 是 OFF。

结论：本机无 tools ON + tests ON + GL ON 的现成配置，未做 compiler/cache 对照。

证据：

- `rr-f217-build.log`
- `rr-f217-ctest-list.log`
- [rr-f217-ctest-run.log](rr-f217-ctest-run.log)
- `rr-f217-codemodel-configure.log`
- [rr-f217-codemodel-check.log](rr-f217-codemodel-check.log)
- `rr-f217-codemodel-target.json`
- `rr-f217-config-scan.log`

## RR-F434 定版本摘要拒绝

判定：成立。

相关实现与测试：

- `engine/playback/src/frame_digest.cpp:8,14`：公共默认版本为 3，`computeFrameDigest` 走 `computeFrameDigestVersion(3, ...)`。
- `engine/playback/src/frame_digest_version.cpp:77-82`：版本 `< 1` 或 `> 3` 返回 `playback.frame_digest.version_unsupported`。
- `tests/playback/frame_digest_tests.cpp:41-76`：已有历史版本保持用例和不支持版本拒绝用例。
- `tests/playback/CMakeLists.txt:3,26`：`frame_digest_tests.cpp` 与 `frame_digest_version.cpp` 都在 `cuexis_playback_tests` 内。

在 `tests/playback` 检索 `version_unsupported`、`computeFrameDigestVersion`、`FrameDigest` 后，定版本摘要的现有用例只有 `frame_digest_tests.cpp` 中这两个；另一个 `version_unsupported` 命中在 `presentation_tests.cpp`，属于 Presentation payload 版本，不经过 `frame_digest_version.cpp`。测试输入是 `RuntimeFrame` 与 `FrameSnapshot`，对应 Presented 帧摘要；没有第二个定版本摘要入口。

### 注册与运行

先确认注册：

```text
cmd /d /c "call "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && ctest --test-dir out/build/debug --no-tests=error -N -R "^FrameDigest (preserves v1 and v2 historical definitions|fixed-version entry rejects unsupported algorithm versions)$""
```

退出码：0，注册 2 个用例：

```text
Test #716: FrameDigest preserves v1 and v2 historical definitions
Test #722: FrameDigest fixed-version entry rejects unsupported algorithm versions
Total Tests: 2
```

实际运行：

```text
cmd /d /c "call "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && ctest --test-dir out/build/debug --no-tests=error --output-on-failure -R "^FrameDigest (preserves v1 and v2 historical definitions|fixed-version entry rejects unsupported algorithm versions)$""
```

退出码：0，结果为 `100% tests passed, 0 tests failed out of 2`。

为记录断言级别的输入与输出，另外直接运行同一测试二进制并打开成功断言输出：

```text
out/build/debug/bin/cuexis_playback_tests.exe "FrameDigest fixed-version entry rejects unsupported algorithm versions" --success
out/build/debug/bin/cuexis_playback_tests.exe "FrameDigest preserves v1 and v2 historical definitions" --success
```

两个命令的退出码都是 0，分别报告 `All tests passed (8 assertions in 1 test case)` 和 `All tests passed (6 assertions in 1 test case)`。

### 版本分支结果

| 分支 | 输入 | 结果 |
| --- | --- | --- |
| 不支持版本（高于当前） | `computeFrameDigestVersion(4, frame, snapshot)` | 无值；错误码 `playback.frame_digest.version_unsupported` |
| 不支持版本（低于 1） | `computeFrameDigestVersion(0, frame, snapshot)` | 无值；错误码 `playback.frame_digest.version_unsupported` |
| 公共默认版本 | `computeFrameDigest(frame, snapshot)` | `algorithmVersion == 3`，摘要值 `9292624206614054870` |
| 固定版本 3 对照 | `computeFrameDigestVersion(3, frame, snapshot)` | 有值，摘要值与公共默认结果相等 |
| 历史合法版本 1 | `computeFrameDigestVersion(1, frame, snapshot)` | `algorithmVersion == 1`，摘要值 `7174372098521454160` |
| 历史合法版本 2 | `computeFrameDigestVersion(2, frame, snapshot)` | `algorithmVersion == 2`，摘要值 `7850652359432829177` |

这些结果覆盖不支持版本拒绝、默认版本保持和历史合法版本保持。原条目提到 `version_unsupported` 没有生产调用方；本轮没有改变这一点，验收点是补上内部定版本入口的拒绝测试并确认历史算法不变。生产代码没有新增或修改。

证据：

- `rr-f434-source-search.log`
- `rr-f434-build.log`
- `rr-f434-ctest-list.log`
- [rr-f434-ctest-run.log](rr-f434-ctest-run.log)
- [rr-f434-assertions.log](rr-f434-assertions.log)

## 证据文件

| 文件 | 内容 |
| --- | --- |
| `rr-f216-default-configure.log` | observer ON、不传库类型的全新配置输出，尾部附退出码与缓存值 |
| `rr-f216-explicit-static-configure.log` | observer ON、显式 STATIC 的全新配置输出 |
| `rr-f216-explicit-shared-configure.log` | observer ON、显式 SHARED 的拒绝输出与缓存值 |
| [rr-f216-summary.log](rr-f216-summary.log) | 三个配置的命令、退出码、首条错误、缓存值与非 Windows 限制 |
| `rr-f217-build.log` | `cuexis_shader_tests` 构建输出 |
| `rr-f217-ctest-list.log` | cache 用例 `ctest -N` 输出 |
| [rr-f217-ctest-run.log](rr-f217-ctest-run.log) | 6 个 cache 用例实际运行输出 |
| `rr-f217-codemodel-configure.log` | `cmake --preset debug` 重新生成 codemodel 的配置输出 |
| [rr-f217-codemodel-check.log](rr-f217-codemodel-check.log) | target 源码、链接库、依赖、编译定义与 `build.ninja` 链接边核对结果 |
| `rr-f217-codemodel-target.json` | codemodel 的 `cuexis_shader_tests` target JSON |
| `rr-f217-config-scan.log` | 现有构建配置的 shader tools/GL/tests 标志扫描 |
| `rr-f434-source-search.log` | `tests/playback` 的摘要版本用例检索结果与实现行 |
| `rr-f434-build.log` | `cuexis_playback_tests` 构建输出 |
| `rr-f434-ctest-list.log` | 摘要版本用例 `ctest -N` 输出 |
| [rr-f434-ctest-run.log](rr-f434-ctest-run.log) | 摘要版本用例实际运行输出 |
| [rr-f434-assertions.log](rr-f434-assertions.log) | 两个摘要用例的 Catch2 成功断言明细与退出码 |

## 限制

- RR-F216 的非 Windows 分支需要非 Windows 主机，未在本机执行；Windows 上的三条配置结论来自 `out/build/rr-f216-*` 三个全新目录。
- RR-F217 没有跑 tools ON 的 compiler/cache 对照，因为本机没有 tools ON + tests ON + GL ON 的现成配置；现有配置全部为 shader tools OFF。
- 配置期间 vcpkg 做了 registry 检查并从本机二进制缓存还原依赖，没有安装 shader-tools 依赖，也没有为对照新增构建配置。
- 所有测试结论来自本轮最终重建的 `cuexis_shader_tests.exe` 与 `cuexis_playback_tests.exe`。

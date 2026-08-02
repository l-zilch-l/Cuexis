# Building Cuexis

状态：阶段 1D 现行构建、安装与质量门禁规范

更新日期：2026-07-27

## 当前仓库说明

当前仓库提供阶段 0 工程基础、阶段 1A Chart/Runtime 闭环、阶段 1B 资源生命周期、
阶段 1C typed Behavior/Playback 闭环，以及阶段 1D 的主音乐内容、Prepared Playback、
ChartClock/HostClock/CuexisAudio、RuntimeTimeline、WAV 解码和可选 SDL 音频适配器。以下命令是
受支持的标准入口；旧的 IDE 私有构建目录和手工编译产物不能作为验收依据。

当前正式激活的库 target 为：

```text
cuexis_core
cuexis_audio
cuexis_filesystem
cuexis_content
cuexis_json_support
cuexis_project
cuexis_platform_sdl
cuexis_world
cuexis_assets
cuexis_chart
cuexis_behavior
cuexis_gameplay
cuexis_render
cuexis_debug
cuexis_runtime
cuexis_render_opengl
cuexis_playback
cuexis_audio_sdl
```

应用 target 为 `cuexis_player`。`app/studio/` 目录已存在但尚未接入 CMake。对应模块测试、架构扫描、Player 失败路径和 `cuexis_format_check` 由顶层 CMake 统一注册。

ADR 0027 已将长期交付方向调整为 Playback SDK + 独立 Player + 独立 Studio。当前
`cuexis_playback` 已通过正式 Runtime 路径驱动 Behavior，并提供 Prepared load/reload、
主音乐内容视图和后端无关 RuntimeTimeline。当前 C++20 static/shared package 导出
Playback、Content、Audio，可选导出 AudioSDL；四类 `add_subdirectory`/`find_package(Cuexis)`
外部 consumer 门禁验证基础包不会引入 SDL。ADR 0033 的 matching-toolchain C++ shared preview
构建、部署和 consumer 门禁已实现；稳定 C ABI、Studio 与宿主专用 adapter 仍属于后续阶段。

## Windows/MSVC 前置条件

```text
Windows 10/11 x64
Visual Studio 2022，Desktop development with C++
MSVC 工具集和 Windows SDK
CMake >= 3.25
Ninja
clang-format
Git
vcpkg
```

设置 `VCPKG_ROOT` 指向 vcpkg 根目录。不得在 Preset 中提交个人绝对路径。

使用 Visual Studio Developer PowerShell/Command Prompt，或确保 `cl.exe`、Ninja 和 CMake 已处于 PATH。

## 配置、构建和测试

```powershell
cmake --preset debug --fresh
cmake --build --preset debug
ctest --preset debug --no-tests=error
```

Release：

```powershell
cmake --preset release --fresh
cmake --build --preset release --clean-first
ctest --preset release --no-tests=error
```

无 SDL/OpenGL/Player 的 Playback 构建：

```powershell
cmake --preset headless-debug --fresh
cmake --build --preset headless-debug
ctest --preset headless-debug --no-tests=error

cmake --preset headless-release --fresh
cmake --build --preset headless-release --clean-first
ctest --preset headless-release --no-tests=error
```

Release 与 `headless-release` 预设强制 `CUEXIS_WARNINGS_AS_ERRORS=ON`。基础 Debug 预设保留
`OFF`，便于日常开发先观察新工具链诊断。

构建目录固定在 `out/build/<preset>`，安装或打包目录不得与源码混合。

## vcpkg

项目使用 manifest mode 和固定 baseline。新增依赖同时更新：

```text
vcpkg.json
vcpkg-configuration.json（仅在升级 baseline 时）
docs/DEPENDENCY_POLICY.md 规定的依赖记录
THIRD_PARTY_NOTICES.md
```

不得把 `D:/vcpkg` 等机器路径提交到项目配置。

当前 Windows/MSVC 验收固定 `x64-windows` triplet。无头基础依赖为 EnTT、GLM、
nlohmann-json、json-schema-validator 和 tl-expected；`audio-sdl` feature 增加 SDL3，`player`
feature 增加 SDL3、glad 与 spdlog，`tests` feature 增加 Catch2。准确版本和许可证记录见根目录
`vcpkg.json` 与 `THIRD_PARTY_NOTICES.md`。

## 生成文件

版本头生成到 `${binaryDir}/generated/cuexis/version.hpp`，不写回源码树。Shader、资源缓存和测试发现文件也属于构建产物。

## 最低验证

提交工程结构改动前必须完成 Debug configure/build/test 和格式检查：

```powershell
cmake --build --preset debug --target cuexis_format_check
```

阶段完成、版本生成信息变更或正式验收必须在 fresh configure 后执行 clean build，避免旧对象文件保留过期的生成头内容：

```powershell
cmake --preset debug --fresh
cmake --build --preset debug --clean-first
ctest --preset debug --no-tests=error

cmake --preset release --fresh
cmake --build --preset release --clean-first
ctest --preset release --no-tests=error
```

Player 冒烟测试需要交互式桌面和支持 OpenGL 3.3 Core 的 GPU，因此与默认 CTest 分开执行。
普通启动和 `--audio-smoke-test` 在未给出 `--project`/`--chart` 时加载阶段 1D 项目；既有
`--smoke-test` 固定加载无音频的阶段 1C 项目并保留三帧确定性语义：

```powershell
.\out\build\debug\bin\cuexis_player.exe --smoke-test
```

真实默认音频设备门禁执行 90 帧 load/play/pause/resume/seek/stop/reload 脚本，并可同时导出
确定性帧轨迹、设备遥测和 metadata sidecar：

```powershell
.\out\build\debug\bin\cuexis_player.exe --audio-smoke-test `
  --frame-stats .\out\artifacts\stage1d-debug
```

输出固定为 `<prefix>.frames.csv`、`<prefix>.audio.csv` 和 `<prefix>.meta.json`。人工门禁必须检查
非静音连续播放、Pause 静音、Resume/Seek/Stop/Reload 行为，并确认 sidecar 中
`droppedRows = 0`、`truncated = false`；物理设备听感与时钟精度不能由 dummy CTest 代替。

阶段 1A 方案 A/B 仍可通过 `--chart` 作为无资源回归入口；方案 B 示例命令为：

```powershell
.\out\build\debug\bin\cuexis_player.exe --smoke-test --chart .\out\build\debug\bin\assets\charts\stage1a_example.cuexis.chart.simple.json
```

默认阶段 1D 项目包含 Chart v2、Asset Index v2、索引内非静音 WAV 和 typed
`audio.mainMusic` 引用。Player 在 Window/GL/Audio device 创建前完成 Project、Index、Chart、
Source 和 WAV preflight，再按内容选择 ChartClock 或 CuexisAudio；已选模式失败时不会静默回退。
构建时会先清理目标 demo project 目录再复制，避免遗留已删除资产。Release 或后端相关改动还应
完成 Release build/test、1C 三帧 GPU smoke、1D 物理音频 smoke，以及阶段 1A A/B Chart 回归；
算法单元测试不得依赖窗口、GPU、物理音频设备或墙钟。

## 常见错误

```text
找不到 cl.exe：从 Visual Studio Developer 环境运行
找不到 vcpkg toolchain：检查 VCPKG_ROOT
依赖版本漂移：检查 baseline 和 overlay port
Catch2 测试未发现：确认 CUEXIS_BUILD_TESTS、BUILD_TESTING 和 test preset
OpenGL 启动失败：记录 SDL driver、GL version、vendor 和 renderer
```

## SDK 组件与安装

当前选项：

```text
CUEXIS_BUILD_PLAYER
CUEXIS_BUILD_SDL_ADAPTER
CUEXIS_BUILD_AUDIO_SDL_ADAPTER
CUEXIS_BUILD_OPENGL_ADAPTER
CUEXIS_BUILD_TESTS
CUEXIS_BUILD_DEVELOPER_TOOLS
```

作为顶层项目时默认构建 Player、adapter、测试和开发工具；作为 `add_subdirectory` 子项目时
这些选项默认关闭。`CUEXIS_BUILD_AUDIO_SDL_ADAPTER` 与平台 SDL adapter 独立；只有 Player
同时要求 SDL platform、SDL audio 和 OpenGL 三个 adapter。Cuexis C++20 preview 通过
`CUEXIS_LIBRARY_TYPE=STATIC|SHARED` 选择 linkage，默认值为 `STATIC`。基础入口目标为
`Cuexis::Playback`、`Cuexis::Content` 和 `Cuexis::Audio`：

```powershell
cmake --install out/build/headless-release --prefix out/install/headless-release
```

```cmake
find_package(Cuexis 0.3 CONFIG REQUIRED COMPONENTS Playback Content Audio)
target_link_libraries(my_host PRIVATE Cuexis::Playback Cuexis::Content Cuexis::Audio)
```

需要直接使用 `Result`、`Error` 或 `Diagnostics` 的 consumer 可以单独请求支持组件
`Cuexis::Core`；Playback、Content 和 Audio 会传递依赖它。

需要 SDL 音频 adapter 的包必须以启用 `CUEXIS_BUILD_AUDIO_SDL_ADAPTER=ON` 的配置构建和安装，
并由 consumer 显式请求组件：

```cmake
find_package(Cuexis 0.3 CONFIG REQUIRED COMPONENTS Audio AudioSDL)
target_link_libraries(my_host PRIVATE Cuexis::AudioSDL)
```

只有请求 `AudioSDL` 时 `CuexisConfig.cmake` 才查找 SDL3 并载入独立的
`CuexisAudioSDLTargets.cmake`；基础 Playback/Content/Audio consumer 不查找 SDL3。

`0.3` 是当前 Playback preview 的 SDK API 兼容版本，不是日期构建版本。安装后的
`Cuexis_VERSION`/`Cuexis_API_VERSION` 返回完整 API 版本，`Cuexis_VERSION_DISPLAY` 返回
`yy.mm.dd.hh-v[-suffix]` 构建身份。

安装树包含 `CuexisTargets.cmake`、`CuexisConfig.cmake`、同 minor 版本兼容文件、生成的
`cuexis/version.hpp`、`LICENSE`、`NOTICE`、第三方 notices 和实际无头依赖版权文本。CTest 中的
五个 `cuexis_external_consumer_*` 门禁分别验证 add_subdirectory/find_package 的基础包、Core 和
AudioSDL 组件。基础 find_package 门禁显式禁用 SDL3 查找；安装包门禁同时扫描全部已安装公共头
是否为纯 ASCII，并校验基础许可证清单及 AudioSDL 安装的 SDL3 copyright：

```powershell
ctest --preset headless-debug -R "^cuexis_external_consumer_" --output-on-failure
```

### Shared preview

阶段 1E 的唯一 Cuexis linkage 选择为：

```text
CUEXIS_LIBRARY_TYPE=STATIC|SHARED
```

不得把 `BUILD_SHARED_LIBS` 当作 Cuexis 支持入口。当前 static/shared preview SDK API 均为
`0.3.0`。一个 build tree 与 install prefix 只能包含一种 Cuexis linkage，consumer 继续链接
相同的 `Cuexis::` target 名，不得硬编码 DLL/shared object 文件名。可直接使用
`shared-debug`、`shared-release`、`headless-shared-debug` 和 `headless-shared-release` presets。

shared preview 要求 consumer 使用匹配的 Cuexis SDK minor、编译器工具链、C++ 标准库、架构、运行时
和 Debug/Release 配置；升级 Cuexis 后必须重新编译 consumer。`SameMinorVersion` 是 package/source
请求规则，绝不构成可替换二进制的 ABI 承诺。shared package 的基础 Playback/Content/Audio consumer
不应安装或查找 EnTT、GLM、JSON/schema validator、SDL3、glad 或 spdlog 开发包；只有显式
`AudioSDL` component 才能查找 SDL3。CTest 会检查完整部署、导出符号、consumer import table、
private target/header 泄漏、配置与 MSVC runtime 不匹配，以及 clean staging 运行。具体规则见
[ADR 0033](adr/0033-cpp-shared-library-preview-boundary.md)。

阶段 1E 首批正式 shared 平台矩阵是 Windows x64/MSVC 与 Linux x64/GCC 或 Clang。Windows
Release/Debug 分别固定动态 CRT `/MD` 与 `/MDd`，不支持与 `/MT` consumer 混用。MinGW、macOS、
其他架构和跨编译器消费在取得同等级 build/install/deploy/runtime 证据前仅为实验组合。

## 跨平台质量入口

Linux/Clang 或 GCC 环境可使用：

```bash
cmake --preset headless-sanitize --fresh -DCMAKE_CXX_COMPILER=clang++ \
  -DVCPKG_TARGET_TRIPLET=x64-linux
cmake --build --preset headless-sanitize --clean-first
ctest --preset headless-sanitize --no-tests=error

cmake --preset headless-clang-tidy --fresh -DCMAKE_CXX_COMPILER=clang++ \
  -DVCPKG_TARGET_TRIPLET=x64-linux
cmake --build --preset headless-clang-tidy --target cuexis_playback

cmake --preset headless-coverage --fresh -DCMAKE_CXX_COMPILER=g++ \
  -DVCPKG_TARGET_TRIPLET=x64-linux
cmake --build --preset headless-coverage --clean-first
ctest --preset headless-coverage --no-tests=error -E "^cuexis_external_consumer_"
```

`.github/workflows/` 持续验证 Windows/MSVC、Windows/MinGW、Linux/GCC Release、
Linux/GCC shared Release、Linux/Clang shared Debug、Linux/Clang ASan+UBSan、clang-tidy 和
不低于 40% 的 engine 行覆盖率。100k Transform 稀疏
更新与 FrameSnapshot 缓冲复用是确定性结构门禁；墙钟时间只作为趋势证据，不作为跨机器
硬阈值。


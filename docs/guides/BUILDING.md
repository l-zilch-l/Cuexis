# Building Cuexis

状态：阶段 3 最终验收后的现行构建、安装与质量门禁规范

更新日期：2026-08-28

## 当前仓库说明

当前仓库提供阶段 0 工程基础、阶段 1A-1E Playback Core 闭环、阶段 2 的 Chart/Behavior/迁移
能力，以及阶段 3A-3G 的 Portable Presentation v1、Validation Sink、OpenGL Player adapter、外部
package consumer 与性能 probe。阶段 3 本地和 hosted 跨平台矩阵已经关闭；以下命令是受支持的
标准入口，旧的 IDE 私有构建目录和手工编译产物不能作为验收依据。

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

应用 target 为 `cuexis_player`，开发工具 target 为 `cuexis_chart_validator` 和
`cuexis_chart_migrator`。`app/studio/` 目录已存在但尚未接入 CMake。对应模块测试、架构扫描、
Player 失败路径和 `cuexis_format_check` 由顶层 CMake 统一注册。

ADR 0027 已将长期交付方向调整为 Playback SDK + 独立 Player + 独立 Studio。当前
`cuexis_playback` 已通过正式 Runtime 路径驱动 Behavior，并提供 Prepared load/reload、
主音乐内容视图和后端无关 RuntimeTimeline。当前 C++20 static/shared package 导出
Playback、Content、Audio，可选导出 AudioSDL；七个 `add_subdirectory`/`find_package(Cuexis)`
外部 consumer 模式包含两个只消费 `Cuexis::Playback` 的 Stage 3 宿主，并验证基础包不会引入
SDL/OpenGL。ADR 0033 的 matching-toolchain C++ shared preview
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
docs/guides/DEPENDENCY_POLICY.md 规定的依赖记录
THIRD_PARTY_NOTICES.md
```

不得把 `D:/vcpkg` 等机器路径提交到项目配置。

当前 Windows/MSVC 验收固定 `x64-windows` triplet。无头基础依赖为 EnTT、GLM、
nlohmann-json、json-schema-validator 和 tl-expected；`audio-sdl` feature 增加 SDL3，`player`
feature 增加 SDL3、glad 与 spdlog，`tests` feature 增加 Catch2。可选 feature `shader-tools`
增加 shaderc、SPIRV-Tools 与 SPIRV-Cross，仅在 `CUEXIS_BUILD_SHADER_TOOLS=ON` 时由内部
`cuexis_shader` 使用；默认 Debug 与 headless 预设不得下载这些 port。准确版本和许可证记录见根目录
`vcpkg.json` 与 `THIRD_PARTY_NOTICES.md`。

可选 Shader 编译器（S5-B 接线，默认关闭）：

```powershell
cmake --preset debug-shader-tools --fresh
cmake --build --preset debug-shader-tools
ctest --preset debug-shader-tools --no-tests=error
```

`debug-shader-tools` 在默认 Debug feature 之上追加 `shader-tools`，并打开
`CUEXIS_BUILD_SHADER_TOOLS`。它构建内部静态库 `cuexis_shader`、`cuexis_shader_tests` 和
developer-tools 下的 `cuexis_asset_importer`，不把编译器链入 `cuexis_playback`。

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
普通启动和 `--audio-smoke-test` 在未给出 `--project`/`--chart` 时加载阶段 1D 项目；当前
`--smoke-test` 固定加载无音频的 `stage3_project`，执行六帧真实 presentation 脚本：Opaque、
textured Transparent、全不可见 clear、失败 Playback reload、失败 adapter prepare 和成功原子
reload。它同时断言规范化 draw summary 与中心像素结果：

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

canonical Stage 1A 示例仍可通过 `--chart` 作为无资源回归入口：

```powershell
.\out\build\debug\bin\cuexis_player.exe --smoke-test --chart .\out\build\debug\bin\assets\charts\stage1a_example.cuexis.chart.json
```

阶段 2A.1 已移除 `cuexis.chart.simple`；该格式稳定报告 `chart.format.unsupported`，构建产物不再复制 Simple fixture。

Chart v3 示例、校验器和显式迁移器可直接运行。默认目标仍是 v3；`--target 4` 才输出静态空动画
v4：

```powershell
.\out\build\debug\bin\cuexis_chart_validator.exe `
  --input .\assets\charts\stage2_example.cuexis.chart.json

.\out\build\debug\bin\cuexis_chart_migrator.exe `
  --input .\tests\fixtures\stage2_migration_v1.cuexis.chart.json `
  --output .\out\artifacts\stage2-migrated.cuexis.chart.json `
  --report .\out\artifacts\stage2-migration-report.json

.\out\build\debug\bin\cuexis_chart_migrator.exe `
  --input .\tests\fixtures\chart_format_update\valid\chart_v3_static_migration.json `
  --output .\out\artifacts\stage-cfu-d-migrated-v4.cuexis.chart.json `
  --report .\out\artifacts\stage-cfu-d-migration-v4-report.json `
  --target 4
```

迁移器要求输入、输出和报告路径互不冲突，失败不修改目标。无 `--target` 的旧调用继续拒绝
v3 输入。`cuexis_chart_tool_tests` 会校验 v3 golden、v4 chart golden、报告字段、目标回滚和
CLI 退出合同。Player 可使用 `--chart` 加载 Stage 2 示例进行 GPU smoke；算法、迁移和
headless Playback 验收不依赖 GPU。Playback 已可 prepare/load 静态或参数化 v4；任意非空
Clip/CXT/Binding/Layer/Instance 在 Stage 4 前仍以 capability 错误拒绝。

默认阶段 1D 项目包含 Chart v2、Asset Index v2、索引内非静音 WAV 和 typed
`audio.mainMusic` 引用。Player 在 Window/GL/Audio device 创建前完成 Project、Index、Chart、
Source 和 WAV preflight，再按内容选择 ChartClock 或 CuexisAudio；已选模式失败时不会静默回退。
构建时会先清理目标 demo project 目录再复制，避免遗留已删除资产。Release 或后端相关改动还应
完成 Release build/test、Stage 3 六帧 GPU smoke、1D 物理音频 smoke，以及 canonical Chart 回归；
算法单元测试不得依赖窗口、GPU、物理音频设备或墙钟。

Stage 3 最大合法资源与热路径趋势 probe 不属于默认构建，必须显式执行：

```powershell
cmake --build --preset release --target stage3_performance_probe
.\out\build\release\bin\stage3_performance_probe.exe
```

该 probe 生成 64 MiB 上限 Texture2D，并报告 prepare、manifest/acquisition、Validation candidate、
warmed update/extract/validate 和 reload peak memory。数值用于同机趋势比较，不是跨机器验收阈值；
确定性性能合同由资源硬预算和零分配测试承担。

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
find_package(Cuexis 0.6 CONFIG REQUIRED COMPONENTS Playback Content Audio)
target_link_libraries(my_host PRIVATE Cuexis::Playback Cuexis::Content Cuexis::Audio)
```

需要直接使用 `Result`、`Error` 或 `Diagnostics` 的 consumer 可以单独请求支持组件
`Cuexis::Core`；Playback、Content 和 Audio 会传递依赖它。

需要 SDL 音频 adapter 的包必须以启用 `CUEXIS_BUILD_AUDIO_SDL_ADAPTER=ON` 的配置构建和安装，
并由 consumer 显式请求组件：

```cmake
find_package(Cuexis 0.6 CONFIG REQUIRED COMPONENTS Audio AudioSDL)
target_link_libraries(my_host PRIVATE Cuexis::AudioSDL)
```

只有请求 `AudioSDL` 时 `CuexisConfig.cmake` 才查找 SDL3 并载入独立的
`CuexisAudioSDLTargets.cmake`；基础 Playback/Content/Audio consumer 不查找 SDL3。

Stage 3 不安装 `OpenGL` component。`cuexis_render_opengl` 仍是 Player 使用的仓库内可选 target；
安装包显式请求 `COMPONENTS OpenGL` 会失败，基础 Playback package 不查找 OpenGL 或 GLAD。

`0.6` 是当前 Playback preview 的 SDK API 兼容 minor，不是日期构建版本。安装后的
`Cuexis_VERSION`/`Cuexis_API_VERSION` 返回完整 API 版本，`Cuexis_VERSION_DISPLAY` 返回
`yy.mm.dd-v[-suffix]` 构建身份。版本更新必须通过
`python -B tools/update_version.py yy.mm.dd-v` 同步 CMake 与 `vcpkg.json`；
`python -B tools/update_version.py --check` 只执行一致性检查。

安装树包含 `CuexisTargets.cmake`、`CuexisConfig.cmake`、同 minor 版本兼容文件、生成的
`cuexis/version.hpp`、`LICENSE`、`NOTICE`、第三方 notices 和实际无头依赖版权文本。CTest 中的
七个 `cuexis_external_consumer_*` 模式验证 add_subdirectory/find_package 的基础包、Playback-only、
Core 和 AudioSDL 组件。Playback-only consumer 从自己的 staging fixture 完成
load/prepare/resource validation/update/extract，只链接 `Cuexis::Playback`。基础 find_package 门禁
显式禁用 SDL3 查找，并验证不发现 OpenGL/GLAD、`0.5`/`0.7` 请求被拒绝、未支持的 `OpenGL`
component 被拒绝；安装包门禁同时扫描全部已安装公共头是否为纯 ASCII，并精确校验基础许可证
清单及 AudioSDL 安装额外增加的 SDL3 copyright：

```powershell
ctest --preset headless-debug -R "^cuexis_external_consumer_" --output-on-failure
```

### Shared preview

阶段 1E 的唯一 Cuexis linkage 选择为：

```text
CUEXIS_LIBRARY_TYPE=STATIC|SHARED
```

不得把 `BUILD_SHARED_LIBS` 当作 Cuexis 支持入口。当前 static/shared preview SDK API 均为
`0.7.0`。一个 build tree 与 install prefix 只能包含一种 Cuexis linkage，consumer 继续链接
相同的 `Cuexis::` target 名，不得硬编码 DLL/shared object 文件名。可直接使用
`shared-debug`、`shared-release`、`headless-shared-debug` 和 `headless-shared-release` presets。

shared preview 要求 consumer 使用匹配的 Cuexis SDK minor、编译器工具链、C++ 标准库、架构、运行时
和 Debug/Release 配置；升级 Cuexis 后必须重新编译 consumer。`SameMinorVersion` 是 package/source
请求规则，绝不构成可替换二进制的 ABI 承诺。shared package 的基础 Playback/Content/Audio consumer
不应安装或查找 EnTT、GLM、JSON/schema validator、SDL3、glad 或 spdlog 开发包；只有显式
`AudioSDL` component 才能查找 SDL3。CTest 会检查完整部署、Stage 3 manifest/acquisition/preflight
导出符号、通用与 Playback-only consumer import table、private target/header 泄漏、配置与 MSVC
runtime 不匹配，以及 clean staging 运行。具体规则见
[ADR 0033](../adr/0033-cpp-shared-library-preview-boundary.md)。

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

WSL 或本地 Linux 可以提前发现跨编译器问题，但不能替代 hosted `ubuntu-latest` 发布证据。阶段
完成报告只能引用包含当前实现 commit 的 workflow run URL；旧分支、旧 commit 或只有文档变更的
run 不得关闭当前阶段门禁。


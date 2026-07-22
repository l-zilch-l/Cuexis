# Building Cuexis

状态：阶段 1B 现行构建规范，已随 ADR 0028 和 cuexis_playback 更新；SDK 组件化与安装流程待阶段 1E 实现

更新日期：2026-07-21

## 当前仓库说明

当前仓库提供阶段 0 工程基础、阶段 1A Chart/Runtime 闭环、阶段 1B 的 ProjectConfig、Asset Index、AssetDatabase、ResourceManager、ResourceScope 和 Renderable Handle 实例化闭环，以及基础 `cuexis_playback` 模块（`PlaybackSession`、`FrameSnapshot`、`RuntimeFrame`、`IContentProvider`）与 ADR 0028 相机投影/事件扩展。以下命令是受支持的标准入口；旧的 IDE 私有构建目录和手工编译产物不能作为验收依据。

当前正式激活的库 target 为：

```text
cuexis_core
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
```

应用 target 为 `cuexis_player`。`app/studio/` 目录已存在但尚未接入 CMake。对应模块测试、架构扫描、Player 失败路径和 `cuexis_format_check` 由顶层 CMake 统一注册。

ADR 0027 已将长期交付方向调整为 Playback SDK + 独立 Player + 独立 Studio。当前已实现基础 `cuexis_playback` 门面（`PlaybackSession::update()` 仍为 stub，不驱动行为求值）；组件化开关、`install/export` 和 `find_package(Cuexis)` 待阶段 1E 实现。相关改造与外部 consumer 门禁属于[SDK 转型方案](stage_plans/cuexis_sdk_transition_plan.md)和阶段 1E。

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

当前 Windows/MSVC 验收固定 `x64-windows` triplet。直接依赖包括 SDL3、EnTT、GLM、nlohmann-json、json-schema-validator、spdlog、fmt、glad、Catch2 和 tl-expected；准确版本和许可证记录见根目录 `vcpkg.json` 与 `THIRD_PARTY_NOTICES.md`。运行时 DLL 由 vcpkg 的 CMake 集成复制到构建输出目录；正式安装和打包流程属于阶段 1E。

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

Player 图形冒烟测试需要交互式桌面和支持 OpenGL 3.3 Core 的 GPU，因此与默认 CTest 分开执行。未给出 `--project` 或 `--chart` 时，Player 加载构建后复制到可执行文件旁的阶段 1B 项目：

```powershell
.\out\build\debug\bin\cuexis_player.exe --smoke-test
```

阶段 1A 方案 A/B 仍可通过 `--chart` 作为无资源回归入口；方案 B 示例命令为：

```powershell
.\out\build\debug\bin\cuexis_player.exe --smoke-test --chart .\out\build\debug\bin\assets\charts\stage1a_example.cuexis.chart.simple.json
```

默认阶段 1B 项目包含 `cuexis.project.json`、独立 `cuexis.asset-index.json`、3 个 Renderable 对象和 Mesh/Material/Texture CPU blob；Material 依赖 Texture。构建时会先清理目标 demo project 目录再复制，避免遗留已删除资产。Player 实例化真实 typed Handle 和 Session ResourceScope，图形输出仍由 DebugDraw 为每个 Transform 生成 XYZ 轴线。冒烟模式创建 SDL3 Window 与 OpenGL Context、通过 OpenGL Debug Line 管线渲染三帧后自动退出。Release 或后端相关改动还应完成 Release build/test，以及默认 Project 与阶段 1A A/B Chart 回归冒烟；算法单元测试不得依赖窗口或 GPU。

## 常见错误

```text
找不到 cl.exe：从 Visual Studio Developer 环境运行
找不到 vcpkg toolchain：检查 VCPKG_ROOT
依赖版本漂移：检查 baseline 和 overlay port
Catch2 测试未发现：确认 BUILD_TESTING 和 test preset
OpenGL 启动失败：记录 SDL driver、GL version、vendor 和 renderer
```

## SDK 转型后的构建门禁

阶段 1E 实现后，本文件必须增加并实际验证以下入口；在对应 CMake 选项存在前不提供虚构命令。

计划引入的 CMake 组件选项（来自 SDK 转型方案 §10）：

```text
CUEXIS_BUILD_PLAYER
CUEXIS_BUILD_STUDIO
CUEXIS_BUILD_SDL_BACKEND
CUEXIS_BUILD_OPENGL_BACKEND
CUEXIS_BUILD_TESTS
CUEXIS_BUILD_EXAMPLES
CUEXIS_BUILD_SHARED
```

门禁入口：

```text
关闭 Player/Studio/SDL/OpenGL 后构建 headless Playback
通过 add_subdirectory 的仓库外 consumer
通过安装目录 find_package(Cuexis CONFIG REQUIRED) 的仓库外 consumer
静态/共享库和受支持 MSVC Runtime 组合
安装公共头依赖扫描、包版本与组件依赖检查
```


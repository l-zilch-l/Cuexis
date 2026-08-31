# Cuexis 阶段 0 完成报告

状态：已完成  
报告日期：2026-07-18  
完成版本：`26.07.17.17-1`  
阶段目标：建立可长期维护、可构建、可测试、可运行的 C++ 工程基础闭环。

## 1. 完成结论

阶段 0 的任务和验收标准已经完成。当前仓库具备正式 CMake/vcpkg 构建入口、Core 基础设施、SDL3 Platform、EnTT World、最小 Render 前端、OpenGL Backend、Cuexis Player、模块化测试、架构守卫和真实 GPU 三帧冒烟入口。

最终 Debug、Release 以及 MSVC 14.44（对应 Visual Studio 2022 17.14/v143）兼容构建均通过。三套构建各运行 `32/32` 项 CTest；Debug 与 Release 格式检查通过；三套 Player 均在真实 NVIDIA GPU 上建立 OpenGL 3.3 Core Context 并恰好完成 3 帧。

Runtime、Chart、Assets、Audio、Behavior、Gameplay、Animation、Particles、Studio 和工具程序没有进入阶段 0 正式构建。阶段 0 没有为了远期功能提前实现空框架或扩大依赖范围。

## 2. 实际交付范围

| 领域 | 完成内容 |
| --- | --- |
| 工程 | CMake 目标化工程、Debug/Release Presets、x64-windows vcpkg manifest、统一输出目录 |
| 版本 | 单一 CMake 版本源、生成 `version.hpp`、Debug 后缀、manifest 配置期一致性校验 |
| Core | `Result/Error`、稳定错误码、context/cause、结构化日志封装、`ThreadChecker` |
| Platform | SDL3 Video Runtime、Window、事件轮询、Drawable Size、Window Lease、单活动窗口 |
| World | EnTT Registry 所有权和受限的 `withRegistry()` 回调访问 |
| Render | 后端无关的最小 `RenderFrame` / `RenderBackend` 契约 |
| OpenGL | Context 属性配置、一次性配置 token、OpenGL 版本与 Core Profile 校验、清屏与交换 |
| Player | 组合 Runtime、Window、World 和 OpenGL Backend，输出版本及 GPU 信息 |
| 测试 | Catch2 v3、CTest 发现、架构扫描、失败路径测试、独立 GPU smoke |
| 工程质量 | `.clang-format`、MSVC `/W4 /permissive- /Zc:__cplusplus`、依赖和 target 白名单 |
| 法务与文档 | Apache-2.0 LICENSE、NOTICE、第三方清单、构建说明和阶段状态同步 |

## 3. 关键实现结果

### 3.1 构建、版本和架构守卫

- `CMakePresets.json` 提供 `debug`、`release` 配置、构建和测试 preset。
- 正式构建只激活 `cuexis_core`、`cuexis_platform_sdl`、`cuexis_world`、`cuexis_render`、`cuexis_render_opengl`、`cuexis_player` 及对应测试。
- 配置期检查阶段 0 target 集合。未来 `cuexis_*` target 若被意外加入正式构建，会直接导致配置失败。
- 每个阶段 0 target 都有直接依赖白名单，防止 Core、World、Render 前端或 Platform 引入错误方向的依赖。
- CTest 保留源码级架构扫描，检查 Core 不包含 SDL/GL，OpenGL 头和 `gl*` 调用不出现在 `render_opengl` 之外。
- 版本单一来源为 `cmake/CuexisVersion.cmake`，最终规范版本为 `26.07.17.17-1`。
- Debug 显示版本为 `26.07.17.17-1-dev`，Release 显示版本为 `26.07.17.17-1`。
- 生成头会显式校验 CMake project version、数值分量、后缀和 `vcpkg.json` 版本。

### 3.2 Core

- `cuexis::core::Result<T>` 基于 `tl-expected`，公共 API 不暴露第三方错误类型。
- `Error` 包含稳定 code、可读 message、context 和可选 cause。
- spdlog/fmt 被封装在 Core 实现内部，其他模块只使用 Cuexis 日志接口。
- 日志 `init/write/shutdown` 使用统一生命周期锁，并覆盖并发写入与关闭测试。
- `ThreadChecker` 记录创建线程，并在 Debug 构建检查线程亲和不变量。
- 内存耗尽到无法构造 `Error` 的情况按 `CODE_POLICY.md` 视为进程级 Fatal。

### 3.3 SDL Platform 和 World

- `SdlRuntime` 共享内部 Runtime State，要求在 SDL main thread 创建和使用。
- `SdlWindow::create()` 显式接收 Runtime，并在创建前校验标题、尺寸、Runtime 和线程。
- 阶段 0 明确限制一个活动 SDL Window。
- `SdlWindowLease` 同时保持 Window 和 Runtime 存活，允许 Backend 独立持有窗口生命周期。
- Runtime、Window、Window Lease 的公共头已明确 copy/move、查询、赋值、析构和最后一个 Lease 释放的线程契约。
- Window ID 在创建时缓存并校验，事件处理不依赖失效的临时句柄。
- `World` 通过 `withRegistry(callback)` 暴露临时访问，编译期禁止回调返回 Registry 内部指针或引用。

### 3.4 OpenGL Backend

- 默认请求 OpenGL 3.3 Core Context。
- Debug Context 创建失败时记录原因，并回退到普通 Context；普通 Context 失败通过 `Result` 返回。
- `configureOpenGlContext()` 返回不可复制的一次性 `OpenGlContextConfiguration`。
- token 的 move 会清空源对象，重复消费返回 `render.opengl.configuration_unavailable`。
- generation 状态保证新配置或失败配置会使旧 token 返回 `render.opengl.configuration_stale`，避免 SDL 全局 GL 属性与 Backend 配置失配。
- Context 创建后查询实际 GL major/minor 和 profile mask，验证实际版本不低于请求且具有 Core Profile bit。
- configure、create 和 renderFrame 在 SDL/GL 操作前检查 SDL main thread，并返回稳定的 `render.opengl.not_main_thread`。
- Backend 每帧重新绑定自己的 Context，只在当前 Context 属于自己时解绑。
- VSync 配置失败记录警告并降级，不破坏渲染闭环。
- Context 创建回滚和正常销毁均记录 SDL 清理错误。

### 3.5 Cuexis Player

- 启动日志打印完整 Cuexis 版本。
- 日志记录 SDL Video Driver、OpenGL Version、Vendor 和 Renderer。
- `--smoke-test` 必须恰好完成 3 帧；提前退出返回 `player.smoke_test.incomplete`。
- 未知命令行参数返回 `player.arguments.unknown` 和进程退出码 `1`。
- SDL 初始化失败返回 `platform.sdl.init_failed` 和进程退出码 `1`。
- `main()` 捕获标准异常和未知异常，避免异常穿越应用边界。

## 4. 直接依赖与许可证

固定 vcpkg baseline：`8e8dfb4ba483886936ded5ca201b500b8d8b0096`

| 依赖 | 解析版本 | 阶段 0 用途 | 许可证 |
| --- | --- | --- | --- |
| SDL3 | 3.4.12 | Window、事件和 OpenGL 平台集成 | zlib，部分配置包含 MIT/Apache-2.0 |
| EnTT | 3.16.0 | World ECS Registry | MIT |
| spdlog | 1.17.0#1 | Core 日志实现 | MIT |
| fmt | 12.2.0 | Core/spdlog 格式化 | MIT |
| glad | 0.1.36 | OpenGL 函数加载 | MIT，生成输入包含 Khronos 相关许可证 |
| Catch2 | 3.15.2 | 测试 target | BSL-1.0 |
| tl-expected | 1.3.1 | C++20 Result 基础实现 | CC0-1.0 |

阶段 0 未新增上述清单之外的直接依赖。具体分发方式、上游地址和退出路径记录在仓库根目录的 `THIRD_PARTY_NOTICES.md`。正式发布前仍须根据实际打包产物重新收集完整的传递依赖 notice。

## 5. 最终验证环境

标准验证环境：

```text
操作系统：Windows x64
Developer Shell：Visual Studio 2026 Developer PowerShell 18.7.3
MSVC：19.51.36248，工具目录 14.51.36231
Windows SDK：10.0.26100.0
GPU：NVIDIA GeForce RTX 4060 Laptop GPU
Driver/OpenGL：NVIDIA 596.36 / OpenGL 3.3.0
```

Visual Studio 2022 兼容环境：

```text
MSVC 工具集：14.44.35207
cl.exe：19.44.35228
对应产品线：Visual Studio 2022 17.14 / v143
vcpkg triplet：临时 x64-windows-msvc144 overlay，仅位于被忽略的 out/compat
依赖策略：所有目标依赖以 14.44 ABI 独立重建，不复用 14.51 二进制包
```

## 6. 实际测试结果

| 验证项 | 结果 |
| --- | --- |
| Debug fresh configure | 通过 |
| Debug clean build | 通过，26 个 Ninja 步骤 |
| Debug CTest | `32/32` 通过，0 失败，2.28 秒 |
| Debug clang-format dry-run | 通过 |
| Debug GPU smoke | 通过，`26.07.17.17-1-dev`，恰好 3 帧 |
| Release fresh configure | 通过 |
| Release clean build | 通过，26 个 Ninja 步骤 |
| Release CTest | `32/32` 通过，0 失败，1.32 秒 |
| Release clang-format dry-run | 通过 |
| Release GPU smoke | 通过，`26.07.17.17-1`，恰好 3 帧 |
| MSVC 14.44 clean build | 通过，项目和依赖均使用 14.44 |
| MSVC 14.44 CTest | `32/32` 通过，0 失败，2.43 秒 |
| MSVC 14.44 GPU smoke | 通过，恰好 3 帧 |

三次 GPU smoke 的共同关键输出：

```text
Video driver: windows
Version: 3.3.0 NVIDIA 596.36
Vendor: NVIDIA Corporation
Renderer: NVIDIA GeForce RTX 4060 Laptop GPU/PCIe/SSE2
Completed frames: 3
```

失败路径实测：

| 场景 | 退出码 | 稳定错误码 |
| --- | --- | --- |
| `--cuexis-invalid-option` | `1` | `player.arguments.unknown` |
| `SDL_VIDEODRIVER=cuexis-invalid-driver --smoke-test` | `1` | `platform.sdl.init_failed` |

32 项 CTest 覆盖：

```text
架构源码扫描
Player 未知参数和 SDL 初始化失败
Result/Error 与 cause/context
版本数值、后缀、CMake 和 manifest 一致性
ThreadChecker 正常与跨线程路径
日志初始化、关闭、空名称及并发 write/shutdown
SDL 非法窗口配置、主线程、单窗口和 Runtime/Lease 生命周期
OpenGL 配置边界、一次性 token、stale token 和 Worker 拒绝
World 回调式 Registry 访问与 Entity 生命周期
```

## 7. 验收标准对照

| 阶段 0 验收标准 | 证据 | 结论 |
| --- | --- | --- |
| Cuexis Player 可以启动窗口 | 三套真实 GPU smoke | 通过 |
| 日志正常输出 | 启动、SDL、OpenGL、smoke 和失败日志 | 通过 |
| 版本号正常显示 | Debug/Release 日志及版本测试 | 通过 |
| Core 不依赖 SDL/OpenGL | 配置期依赖白名单和 CTest 源码扫描 | 通过 |
| OpenGL 调用只存在于 render_opengl | CTest 架构扫描和最终静态重审 | 通过 |
| 项目可通过 preset 构建 | Debug/Release fresh configure 与 clean build | 通过 |
| CTest 可发现并运行阶段 0 测试 | Debug/Release/14.44 各 `32/32` | 通过 |
| vcpkg manifest 与 baseline 完整 | 配置成功、固定 baseline、版本一致性测试 | 通过 |

## 8. 最终架构重审

最终重审确认：

- Core 源码和 target 都没有 SDL、glad 或 OpenGL 依赖。
- OpenGL 头和 `gl*` 调用只存在于 `engine/render_opengl`。
- World 与 Render 前端没有直接链接 SDL/OpenGL。
- Platform 没有直接链接 glad/OpenGL。
- Window 强持有 Runtime State，Backend 按值持有 Window Lease。
- Runtime、Window、Lease 和 Backend 的线程要求已在公共头中说明。
- 稳定错误码覆盖配置、线程、生命周期、Context、Loader、版本、Profile、Frame 和 Swap 失败。
- Runtime、Chart、Assets、Audio、Gameplay、Studio 等未来模块没有加入根构建。
- 激活范围内未发现 TODO、FIXME、HACK、临时绕过或调试残留。
- 完成验证后没有遗留 CMake、Ninja、CTest、vcpkg、Player 等后台进程。

## 9. 验证期间发现并解决的问题

### 9.1 vcpkg 用户缓存与 Release 配置

沙箱环境首次访问用户级 vcpkg registry cache 时被拒绝，随后有一个超时的子级 vcpkg 进程短暂持有 Release 安装目录锁。该孤立进程树被精确终止，最终改用本机已有且包含固定 baseline 的 `D:/vcpkg` 完成 fresh configure。项目源码和依赖版本未因此改变，最终没有残留进程。

### 9.2 MSVC 14.44 首次兼容链接失败

首次 14.44 验证中，Cuexis 源文件全部由 `cl 19.44` 编译，Player 也成功链接，但 vcpkg 自动选择同机较新的 14.51 工具集恢复 Catch2 等二进制缓存。测试链接时出现 14.44 STL 缺少新版 `__std_*` 符号的 `LNK2019/LNK1120`，因此该次结果没有被计为兼容通过。

随后使用临时 overlay triplet 将 vcpkg 的 `VCPKG_PLATFORM_TOOLSET` 固定为 `v143`，`VCPKG_PLATFORM_TOOLSET_VERSION` 固定为 `14.44.35207`。vcpkg 明确检测到 14.44，并重新构建 Catch2、fmt、SDL3、spdlog、glad、EnTT 和 tl-expected。重建后 clean build、`32/32` CTest 和 GPU smoke 全部通过。

## 10. 剩余非阻断风险与明确非目标

- CI provider 尚未确定，因此仓库没有加入 GitHub Actions、Azure Pipelines 等特定服务配置；本地 configure/build/CTest/format 入口已经标准化。
- GPU smoke 只覆盖当前 Windows/NVIDIA 环境，没有形成 AMD、Intel 或多驱动矩阵。
- 阶段 0 有意限制为一个活动 Window，并要求 SDL/Window/OpenGL 对象在 main/owner thread 使用和销毁；未实现跨线程销毁队列。
- Debug Context 回退已实现并经过正常 GPU 路径验证，但没有注入驱动级 Context 创建故障。
- 安装、打包和发布 notice 聚合不属于阶段 0；正式发布前仍需进行独立打包验收。
- CI、跨平台、音频、资源、谱面 Runtime、Gameplay 和 Studio 均属于后续阶段，不能据此视为已经实现。

## 11. 阶段结论和后续边界

阶段 0 已达到“可配置、可构建、可测试、可运行、边界可检查”的完成状态，可以作为阶段 1A 的稳定工程基础。

下一阶段应按 `PROJECT_GUIDE.md` 进入规范谱面与最小实例化闭环，并继续遵守以下边界：

```text
不让 Chart 直接拥有 Runtime World
不让业务模块依赖 SDL/OpenGL 后端类型
不提前实现 Audio、Studio、完整 RenderGraph 或 Gameplay
每个子阶段保留独立 demo、失败路径、CTest 和完成报告
```

## 12. 相关文档

- `../PROJECT_GUIDE.md`
- `../PROJECT_REVIEW.md`
- `../BUILDING.md`
- `../CODE_POLICY.md`
- `../VERSIONING.md`
- `../DEPENDENCY_POLICY.md`
- `../../THIRD_PARTY_NOTICES.md`


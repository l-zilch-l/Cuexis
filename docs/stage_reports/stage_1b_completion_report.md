# Cuexis 阶段 1B 完成报告

状态：实现与全部验收门禁完成  
报告日期：2026-07-18（2026-07-21 复核：`cuexis_playback` 已随 SDK 转型方案及 ADR 0028 后续激活为第 15 个非测试 target）  
完成版本：`26.07.18.18-1`  
阶段目标：完成 ProjectConfig 到 AssetDatabase、ResourceManager、RuntimeSession 和 Player 的同步资源生命周期闭环。

## 1. 完成结论

阶段 1B 的代码范围已经完成。仓库现可从固定 `cuexis.project.json` 定位项目，通过每个资产根独立的 `cuexis.asset-index.json` 建立不可变 AssetDatabase，同步加载有界 Mesh/Material/Texture CPU blob，以 typed Handle、move-only Lease 和 ResourceScope 管理生命周期，并在 RuntimeSession 的准备事务中把资源 Handle 发布到 RenderableComponent。

默认 Player 项目包含 3 个 Renderable、3 个 CPU blob 和 `material.basic -> texture.white` 依赖。图形输出继续使用阶段 1A DebugDraw，正式资源内容格式与真实 Mesh GPU 绘制没有提前进入本阶段。

Debug 与 Release 均已完成 fresh configure、clean build、`153/153` 项 CTest、架构扫描和全仓格式门禁，未发现剩余 P0/P1 问题。Debug/Release 下的默认阶段 1B Project 以及阶段 1A canonical/simple 共 6 组 GPU smoke 也全部通过。

## 2. 实际交付范围

| 领域 | 完成内容 |
| --- | --- |
| ProjectConfig | `cuexis.project` v1 typed Reader、规范 UUIDv7、固定文件定位、`{root, path}` 入口、opaque extensions |
| 路径安全 | portable ASCII 相对路径、root overlap/别名、reparse/物理 containment、入口 regular-file 校验 |
| 保存 | 显式同目录临时文件、完整重载校验、平台原子 replace；加载不自动写回 |
| Asset Index | 每 root 独立 `cuexis.asset-index.json`、typed record、全局 AssetId/来源/类型/依赖校验 |
| AssetDatabase | 不可变索引、有界 blob 读取、读取时 containment 复核；不使用目录枚举发现 AssetId |
| ResourceManager | 同步 CPU blob、typed slot/state、manager token、generation、contentRevision、固定 typed fallback |
| 生命周期 | move-only `ResourceLease`、有序索引 `ResourceScope`、依赖闭包去重、Required/Fallback/Optional |
| Runtime | 外部不可移动 Manager 注入、结构预验证、临时 Scope/World、owner 校验、活动诊断和事务 reload |
| Player | `--project`、与 `--chart` 互斥、默认阶段 1B project、保留阶段 1A canonical/simple 回归入口 |
| Demo | 1 root、3 Renderable、Mesh/Material/Texture 各 1 个 CPU blob、Material 到 Texture 的一层依赖 |

正式激活的 14 个非测试 target：

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
cuexis_player
```

## 3. Project 与配置整合

阶段 1A 的 ADR 0024 已建立跨阶段配置所有权。阶段 1B 按 ADR 0025/0026 完成第一份真实持久化配置消费者，并冻结：

```text
<project-root>/cuexis.project.json
format: cuexis.project
version: 1
projectId: canonical UUIDv7
assetRoots: named portable project-relative roots
entry.chart: {root, path} bootstrap locator
<asset-root>/cuexis.asset-index.json
format: cuexis.asset-index
version: 1
```

两种格式均有 Draft 7 Schema artifact；运行时权威仍是 Cuexis-owned typed Reader 与 semantic validator，以提供稳定字段路径和诊断代码。JSON DOM 不进入 Project、Assets 或 Runtime 公共契约。ProjectConfig 不包含窗口、音频设备、用户目录、资源设备预算、ImporterProfile 或 ShaderTargetProfile。

后续配置整合继续遵循“首次真实消费时冻结”的路线：阶段 1C 不新增配置文件，Timing 与 Behavior 参数保留在版本化 Chart/Behavior 数据；阶段 5 冻结 ImporterProfile/ShaderTargetProfile；阶段 6 随 Player 消费者实现 UserPreferences 与最小音频设备配置；阶段 9 冻结 DeviceProfile；阶段 11 冻结 Input/Calibration profile。

## 4. 资源与 Runtime 生命周期

AssetDatabase 负责 `AssetId -> type/source/dependencies`，ResourceManager 负责 `AssetId -> typed runtime slot`。阶段 1B 的资源内容仅为有界 opaque CPU blob；ResourceManager 不依赖 ProjectConfig、SDL 或 OpenGL。

Handle 由 `index + generation + managerToken` 标识。generation 防槽位复用 ABA，manager token 拒绝跨 Manager 同槽位别名；Session token 进一步拒绝 Prepared 数据跨 Session 或同地址替换 owner 提交。`contentRevision` 和 Reloading 状态作为未来内容热重载扩展点保留，当前没有文件监听或正式热重载 API。

ResourceScope 使用有序索引对 `(type, AssetId)` 去重，并持有直接请求和传递依赖的 Lease。单次请求具有事务回滚；先前 Fallback 结果不能被后续 Required 请求静默复用。RuntimeSession 先执行无副作用结构校验，再进行资源 I/O 和 World 实例化；成功/失败 reload 均保持完整事务语义，销毁顺序固定为 `World -> ResourceScope`。

## 5. 默认安全预算

ProjectConfig：

```text
输入：1 MiB
JSON 嵌套深度：32
单个 JSON key/string value：16 KiB
portable path：4096 bytes
asset root：16
扩展成员：256
诊断：1024
```

Asset Index / AssetDatabase / ResourceManager：

```text
Asset Index 输入：64 MiB
JSON 嵌套深度：32
单个 JSON key/string value：16 KiB
AssetId：256 bytes
portable source path：4096 bytes
资产记录：100000
单资产依赖：256
依赖深度：64
单 CPU blob：64 MiB
诊断：1024
```

上述值是代码级解析和资源耗尽防护上限，不是 UserPreferences 或 DeviceProfile 的可调设备预算。

## 6. 测试与验证

实际验证命令基于 Visual Studio Developer 环境、MSVC 19.51、Ninja 和 vcpkg manifest mode。两个配置均使用 clean build，避免版本生成头变化后复用旧对象文件：

```powershell
cmake --preset debug --fresh -DCMAKE_MAKE_PROGRAM="D:/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe" -DCUEXIS_CLANG_FORMAT_EXECUTABLE="D:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin/clang-format.exe"
cmake --build --preset debug --clean-first
ctest --preset debug --no-tests=error
cmake --build --preset debug --target cuexis_format_check

cmake --preset release --fresh -DCMAKE_MAKE_PROGRAM="D:/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe" -DCUEXIS_CLANG_FORMAT_EXECUTABLE="D:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin/clang-format.exe"
cmake --build --preset release --clean-first
ctest --preset release --no-tests=error
cmake --build --preset release --target cuexis_format_check
```

沙箱环境需要先通过 `VsDevCmd.bat -arch=x64` 建立编译环境，并设置 `VCPKG_ROOT=D:\vcpkg`；这是本机启动要求，不进入 CMake Preset 或项目配置。

| 验证项 | 结果 |
| --- | --- |
| Debug fresh configure | 通过，vcpkg manifest 依赖可用 |
| Debug 完整 build | 通过 |
| Debug CTest | `153/153` 通过，0 失败 |
| Debug architecture scan | 通过，包含在 CTest 中 |
| Debug 全仓 clang-format dry-run | 通过，0 条格式诊断 |
| Release fresh configure/clean build | 通过 |
| Release CTest/architecture scan | `153/153` 通过，0 失败；架构扫描包含在 CTest 中 |
| Release clang-format dry-run | 通过，0 条格式诊断 |
| Debug 默认阶段 1B Project GPU smoke | 通过，版本 `26.07.18.18-1-dev`，OpenGL 3.3，3 帧 |
| Debug 阶段 1A canonical/simple GPU smoke | 2/2 通过，各 3 帧 |
| Release 默认阶段 1B Project GPU smoke | 通过，版本 `26.07.18.18-1`，OpenGL 3.3，3 帧 |
| Release 阶段 1A canonical/simple GPU smoke | 2/2 通过，各 3 帧 |

测试组成：

```text
13 个 Catch2 测试可执行文件
145 个 Catch2 TEST_CASE
1 个 CMake 架构扫描
7 个 CMake Player CLI/失败路径
总计 153 项 CTest
```

默认阶段 1B GPU smoke 的验收输出合同为：

```text
Asset roots: 1
Ready resources: 3
Renderable objects: 3
Committed objects: 3
Debug commands: 9
Scoped resources: 3
Completed frames: 3
```

Debug 与 Release 均实际观察到上述输出。GPU 环境为 Windows SDL driver、NVIDIA GeForce RTX 4060 Laptop GPU，驱动报告 OpenGL `3.3.0 NVIDIA 596.36`。阶段 1A canonical/simple 回归均观察到 3 objects、9 Debug commands、0 Renderable、0 Scoped resources 和 3 frames。

验收前曾发现一轮 Debug 冒烟复用了旧 `player_app.obj`，启动版本为 `26.07.18.14-1-dev`。该轮已作废；最终证据全部来自 `--fresh` 后执行 `--clean-first` 生成的 `26.07.18.18-1-dev` / `26.07.18.18-1` 二进制。

## 7. 验收标准对照

| 阶段 1B 验收标准 | 证据 | 结论 |
| --- | --- | --- |
| 有效 ProjectConfig 建立 AssetDatabase 并定位入口 | 默认 fixture、Project/Asset typed tests、Player 组合路径 | 通过 |
| 配置损坏、重复根和越界路径稳定失败 | Project、Asset Index、AssetDatabase 路径与失败测试 | 通过 |
| ProjectConfig 不保存本机偏好 | v1 Schema、typed model、ADR 0025 | 通过 |
| 资源缺失按引用策略确定处理 | Required/Fallback/Optional 与缓存 fallback 回归测试 | 通过 |
| Scope 释放强引用，旧 Handle 不命中新槽位 | unload/reload、generation、manager token 和生命周期测试 | 通过 |
| Debug/Release 与 GPU 完整门禁 | 两套 clean build、各 `153/153` CTest、格式检查及 6 组 GPU smoke | 通过 |

## 8. 残余风险

当前没有已知 P0/P1 问题。以下风险不改变阶段 1B 已完成的公共边界：

| 优先级 | 风险 | 后续处理 |
| --- | --- | --- |
| P2 | 当前只有单 blob 64 MiB 上限，没有进程级总驻留内存硬预算、LRU 或背压；大量同时 Ready 的资产仍可能抬高峰值内存 | 在正式大资源/Importer 接入前以测量数据设计总预算和淘汰策略，不把值写进 ProjectConfig |
| P2 | 物理路径与 reparse 防护当前只在 Windows/MSVC 验收环境形成证据，Linux/Android 的文件身份和大小写语义尚无矩阵 | 对应平台进入支持范围时补充平台 fixture 与越界/别名测试 |
| P2 | Player 在 SDL/OpenGL 初始化后才读取 Asset Index、建立 AssetDatabase 并编译 Chart；错误项目会增加启动成本并可能短暂显示窗口 | 在阶段 1C/Player preflight 细化时把无 GPU 副作用的项目与资源验证前移 |
| P2 | 资源版 `ChartWorldInstantiator::instantiate` 是公开 API，只校验 Handle 所属 Manager，不自行持有 Lease/Scope | RuntimeSession 调用路径已经持有 Scope；后续收窄 API 可见性或把调用者寿命责任写入公共契约 |
| P3 | 同步主线程 CPU blob 加载会随大依赖闭包增加启动或 reload 停顿 | 保留当前同步契约，测量后再设计异步任务、取消和优先级 API |
| P3 | `player_stage1b_assets` 每次构建都会清理并重拷贝 demo project | 当前 fixture 很小；资产规模扩大时改为带 output/stamp 的增量复制 |
| P3 | 原子保存保证进程内原子可见与失败保留旧文件，但断电持久性所需的目录级 flush 尚未冻结 | 在 Studio 出现真实写回消费者前形成平台保存 ADR |

## 9. 阶段 1C 边界

阶段 1C 的范围是 BehaviorSystem、Transform Keyframe、`chartTimeMs` 驱动，以及 Seek/时间跳转后的绝对时间重采样。它应复用已经发布的 RuntimeSession、World、Handle 和 ResourceScope，不修改 ProjectConfig/Asset Index 的职责，也不增加全局配置单例。

**SDK 转型补充（2026-07-20）**：阶段 1C 同时交付第一版 PlaybackSession C++ 门面、宿主直接提交 RuntimeFrame 的 headless 路径和 FrameSnapshot 输出，Player 改为 PlaybackSession 的薄组合层。详见[阶段 1C 实施计划](../stage_plans/stage_1c_implementation_plan.md)与 [SDK 转型方案](../stage_plans/cuexis_sdk_transition_plan.md) §12.1。

明确不进入阶段 1C：

```text
AudioClock、音频设备与 Transport
正式 Mesh/Material/Texture 内容格式、Importer 和真实 Mesh GPU 绘制
异步 ResourceManager、LRU、文件监听与内容热重载
UserPreferences、DeviceProfile、Input/Calibration profile
正式输入、判定、计分、Studio 与资源浏览器
ContentProvider 完整抽象、install/export、仓库外 consumer（阶段 1E）
稳定 C ABI、语言绑定（本报告编写时的后续路线；当前已调整为阶段 12，见 ADR 0027）
```

## 10. 相关文档

- [项目技术指南](../PROJECT_GUIDE.md)
- [阶段 1B 实施计划](../stage_plans/stage_1b_implementation_plan.md)
- [构建与验证](../BUILDING.md)
- [RuntimeSession 规范](../RUNTIME_SESSION.md)
- [ADR 0024：配置所有权与分阶段格式](../adr/0024-configuration-ownership-and-staged-formats.md)
- [ADR 0025：ProjectConfig v1 与路径安全](../adr/0025-project-config-v1-and-path-security.md)
- [ADR 0026：Asset Index 与来源解析](../adr/0026-asset-index-and-source-resolution.md)
- [ProjectConfig v1 Schema](../../schemas/cuexis.project.v1.schema.json)
- [Asset Index v1 Schema](../../schemas/cuexis.asset-index.v1.schema.json)

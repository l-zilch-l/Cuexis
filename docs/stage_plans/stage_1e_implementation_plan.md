# 阶段 1E 实施计划：SDK 封装与外部消费闭环

状态：实现完成；Windows/MSVC full/headless static/shared Debug/Release 自动化验收完成；Linux GCC/Clang shared CI 执行结果仍是合入前门禁
规划日期：2026-07-20  
进展更新：2026-07-29
最终验收前置：[阶段 1C 审查问题关闭](../stage_reports/260722-1c-review.md)、[阶段 1D 实施计划](stage_1d_implementation_plan.md)；独立的 packaging/consumer 工作允许提前实施
产品边界：[ADR 0027](../adr/0027-playback-sdk-product-boundary.md)、[ADR 0030](../adr/0030-playback-preview-api-version-and-result.md)、[ADR 0032](../adr/0032-playback-clock-and-prepared-audio-transaction.md)、[ADR 0033](../adr/0033-cpp-shared-library-preview-boundary.md)、[SDK 转型方案](cuexis_sdk_transition_plan.md)

## 1. 阶段目标

阶段 1E 证明 Cuexis Playback Core 可以作为仓库外 C++ 项目的可安装 preview 依赖，而不只是仓库内 Player 的共享代码：

```text
typed/memory Project or Chart source
-> ContentProvider
-> PlaybackSession
-> load/update/seek/reload/extract/unload
-> FrameSnapshot
-> external consumer
```

本阶段完成后，关闭 Player、Studio、SDL、OpenGL 和物理音频设备仍能完成 headless 播放闭环；独立 Player 继续使用相同 PlaybackSession，不保留应用私有 Runtime 路径。该里程碑不等于完整 Playback SDK v1：正式 Judgement/Replay、长期 C++ 兼容承诺和稳定 C ABI 仍未交付。

同步 Filesystem/Memory/Host ContentProvider、ResourceManager 与 Playback 注入、adapter-disabled
preset、C++20 static/shared `Cuexis::Playback`/`Cuexis::Content` 安装导出，以及
add_subdirectory/find_package 两种隔离 consumer 均已完成。ADR 0033 的同工具链 C++ shared
library preview、完整 static/shared 组件矩阵、最终 Playback source 边界和部署/兼容门禁已经
落地；稳定 C ABI 仍不属于本阶段。

阶段 1D 已在该基础上增加 `Cuexis::Audio` 与可选 `Cuexis::AudioSDL`，当时把 preview SDK API
提升到 `0.2.0`；阶段 1E 的不兼容 source/package 边界进一步提升到当前 `0.3.0`。AudioSDL
只有在 consumer 显式请求对应 component 时才传播 SDL3；
纯 Playback/HostClock consumer 不得因此查找、链接或初始化 SDL。

## 2. 已接受边界

```text
第一版 preview 消费接口为 C++20 + CMake package
阶段 1E 同时交付 static 与 shared C++ preview，但 shared 只支持匹配工具链/运行时的重编译消费
SameMinorVersion 只描述源码/package 请求兼容，不代表二进制兼容
稳定 C ABI、语言绑定和官方 Unity/Unreal adapter 必须在正式 Judgement/Replay 完成后进入阶段 12
ProjectConfig 继续是 Player/Studio 标准入口
SDK 同时接受 typed/memory source，不强制物理项目目录
AssetId、Asset Index、Handle、Lease、Scope 和 ResourceManager 语义保持
Filesystem、Memory 和 Host ContentProvider 使用同一逻辑索引与预算
公共 Playback 头不暴露 RuntimeSession、World、EnTT、SDL、OpenGL 或 JSON DOM
公共 Playback 创建接口不接收 AssetDatabase、ResourceManager 或其他内部模块对象
shared 宿主内容扩展只通过 Cuexis 创建的 HostContentProvider callback wrapper
shared 宿主时间输入只通过 SourceClockSample/HostClock，不直接继承 IAudioClock/IAudioTransport
作为依赖构建时不自动构建 App、测试、格式 target 或复制 demo 资产
一个 build tree/install prefix 只包含 STATIC 或 SHARED 一种 Cuexis linkage
```

## 3. ContentProvider 责任

ContentProvider 只解决“根据已校验逻辑来源取得有界字节”，不接管 AssetId 发现、依赖解析、缓存槽位或资源类型语义。

推荐职责：

```text
AssetDatabase
  AssetId -> type + rootId + logical source + dependencies

ContentProvider
  logical source -> bounded immutable bytes / read result

ResourceManager
  request -> slot + Handle/Lease/Scope + dependencies + contentRevision
```

必须提供：

```text
FilesystemContentProvider
  复用 ADR 0025 的 portable path、physical containment 和诊断脱敏

MemoryContentProvider
  测试与纯内存宿主使用；数据身份和 revision 显式提供

HostContentProvider
  连接宿主 VFS/归档/下载缓存；第一版可以是同步回调
```

规则：

```text
Provider 不能通过目录枚举发现 AssetId
Provider 不能改变 Asset Index 的类型和依赖
所有返回字节仍执行大小、格式、溢出和语义校验
短读、缺失、重复完成、超限、异常和宿主回调失败均转换为稳定 Error
Provider callback 的线程、重入、阻塞和数据有效期必须明确
阶段 1E 不冻结 Future/协程/取消或流式解码 API
```

## 4. Playback 公共头边界

阶段 1C 的 PlaybackSession 第一版在 1E 整理为可安装 preview 组件。具体类型名仍可在兼容政策约束下调整，但公共职责固定为：

```text
创建：通过 Playback-owned source/session config 注入内容源、预算、能力和诊断 Sink
加载：Project/Chart source -> Prepared Playback -> adapter 内容准备/激活 -> 无失败 commit
更新：显式 RuntimeFrame；阶段 11 再加入正式 InputEvent
控制：Seek/Reload/Unload 的事务语义
输出：不可变或调用期只读 FrameSnapshot
内容：PlaybackContentInfo 与调用期 MainMusicSourceView，不返回资源 slot/Handle/Lease
查询：稳定 Chart/Object/状态身份，不返回 Entity/World
错误：稳定 code、上下文和有界 Diagnostics
```

公共头 allowlist 必须拒绝：

```text
entt/*
SDL3/*
glad/*、OpenGL/Vulkan 后端类型
nlohmann/* 与 JSON Schema validator
spdlog/*、fmt/*
RuntimeSession、World 和 Registry 私有访问头
AssetDatabase、ResourceManager 和其他内部模块所有权类型
```

内部模块可继续使用现有依赖。若第一版 C++ API 暂时适配 `core::Result`，必须记录源码兼容范围；稳定 C ABI 仍不得暴露 `tl::expected`。

## 5. CMake 组件化

现有选项保持兼容；阶段 1E 组件矩阵必须表达以下能力：

```text
build Playback
build Player
build Studio
build SDL platform/audio adapters
build OpenGL adapter
build tests
build examples/external consumers
CUEXIS_LIBRARY_TYPE=STATIC|SHARED
```

一个 build tree 和 install prefix 只允许一种 Cuexis linkage，不支持在同一 prefix 混装 static
与 shared。`CUEXIS_LIBRARY_TYPE` 是 Cuexis 唯一 linkage 选择；不得用全局
`BUILD_SHARED_LIBS` 隐式改变 Cuexis 或父项目依赖的类型。shared 最终验收前默认值保持
`STATIC`。

安装 target 名冻结为 `Cuexis::Core`、`Cuexis::Playback`、`Cuexis::Content`、
`Cuexis::Audio` 和 `Cuexis::AudioSDL`。`Core` 是公共 Result/Error/Diagnostics 的支持运行时，
通常由产品组件传递使用。Playback 可以传播后端无关 Audio 与 Content；Playback、Content 与
Audio 不得传播 SDL3，AudioSDL 可以传播其真实 SDL3 依赖。

shared binary 拓扑固定为 Core、Content、Audio、Playback 和可选 AudioSDL。World、Runtime、
Assets、Chart、Behavior、Gameplay、Render、JSON support 和 filesystem mechanics 在 shared
构建中作为 PIC 内部库吸收到所属公共 binary，不作为独立支持 DLL 或 package component。
SDL platform/OpenGL adapter 不进入 Playback Core shared package。

static package 为完成公开 target 的链接闭包可以安装内部 archive 和 CMake implementation
target，但其名称、头文件和直接使用不属于 SDK 契约。`0.3.0` 起安装公共头 allowlist 只包含
Core、Content、Audio、Playback、可选 AudioSDL 以及生成的 version/export headers；不得因为
内部 archive 被分发就安装 Assets/Runtime/World/JSON 等内部模块头。

行为要求：

```text
顶层开发构建可保留现有便利默认值
add_subdirectory 消费默认不构建 App、测试、format 或复制 fixture
禁用 SDL/OpenGL 后 configure 不执行对应 find_package
每个导出组件只传播真实 PUBLIC 依赖
所有安装头使用 FILE_SET 或等价显式 allowlist
生成头同时支持 BUILD_INTERFACE 与 INSTALL_INTERFACE
shared binary 默认 hidden visibility，只通过各组件 CUEXIS_*_API 宏导出正式公共符号
禁止 CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS，禁止导出 RuntimeSession/World/EnTT/后端实现符号
安装 package 暴露并校验 Cuexis_LIBRARY_TYPE
shared package 记录并校验 compiler/toolset、C++ standard、architecture、configuration 和 runtime
Windows/MSVC shared 仅支持 Release /MD 与 Debug /MDd，拒绝 /MT 混用
```

## 6. 安装与 Package

必须生成和验证：

```text
CuexisTargets.cmake
CuexisConfig.cmake
CuexisConfigVersion.cmake
Cuexis::Core、Cuexis::Playback、Cuexis::Content、Cuexis::Audio 与可选 Cuexis::AudioSDL 命名空间目标
公共 headers 与 generated version header
按已选组件安装的运行库、导入库和许可证文件
THIRD_PARTY_NOTICES 与实际分发组件一致
```

static package 可以查找完成链接所需的内部依赖。shared package 只查找公共头或显式请求组件
所需依赖：基础 Core/Playback/Content/Audio consumer 不得要求 EnTT、GLM、JSON/schema
validator、SDL3、glad 或 spdlog 开发包；当前 `core::Result` 的 `tl-expected` 头依赖除外。只有
显式请求 AudioSDL 时才能查找 SDL3。包版本兼容规则不得把项目显示版本误用为内容格式或
未来 C ABI 版本。

当前 static/shared preview 的 SDK API 为 `0.3.0`，因为 Playback 创建边界和 package linkage
契约发生了不兼容变化。日期构建版本、内容格式版本和未来 C ABI 版本仍与其独立。

## 7. External Consumer

建立位于正常测试边界之外、只使用公开消费方式的最小工程：

```text
consumer_add_subdirectory
  以 Cuexis 源码子目录消费

consumer_find_package
  先安装到临时 prefix
  使用 find_package(Cuexis 0.3 CONFIG REQUIRED COMPONENTS Playback Content Audio)
  分别验证 static/shared 的 Playback/Audio 与可选 AudioSDL component
```

consumer 不得：

```text
包含仓库私有 src/ 头
读取源码树固定路径
依赖 Player fixture 自动复制
直接构造 RuntimeSession、World 或 ResourceManager 私有状态
链接 SDL/OpenGL，除非该 consumer 明确测试对应可选 component
```

headless fixture 应从内存或 consumer 自己的测试目录提供 Project/Chart/Index/资源，并用显式 RuntimeFrame 驱动。
shared consumer 必须从干净 staging 目录运行，不能借用 Cuexis build tree 中的 DLL/shared object；
门禁必须部署完整 runtime closure 并核对许可证。

## 8. 实施批次

### 1E-0：契约与迁移清单

- 冻结 ContentProvider 同步第一版的输入、输出、线程、重入、错误和 revision 语义。
- 冻结 Playback 公共头 allowlist、FrameSnapshot 有效期和 preview 包组件名；不把当前函数集合误称为长期稳定 ABI。
- 依 ADR 0033 冻结 static/shared binary topology、linkage 选择、工具链要求和重新编译政策。
- 明确旧 AssetDatabase `readBlob` 到 Provider 的兼容迁移，不一次性重写 ResourceManager。

### 1E-1：内容来源分离

- 从 AssetDatabase 物理读取路径提取 FilesystemContentProvider。
- 实现 MemoryContentProvider 与测试 Provider。
- ResourceManager 只通过 Provider 读取已索引逻辑来源。
- 保持阶段 1B 路径安全、Handle/Lease/Scope 和事务回滚测试。

### 1E-2：Playback 公共组件

- 整理阶段 1C 的 `cuexis_playback` target 与安装头。
- 使用 Pimpl/内部实现或等价边界隐藏 RuntimeSession、World 和 EnTT。
- 用 Playback-owned source/session config 替换接受 AssetDatabase 的公共构造接口；Assets 继续是内部模块。
- 使 installed shared consumer 只通过 HostContentProvider wrapper 扩展内容，不把 C++ 虚表继承当作插件 ABI。
- 将 Player 组合逻辑中可复用的 load/update/reload/extract 迁入 Playback；CLI/窗口/设备保留在 App。
- 增加 Session owner、跨 Session、销毁测试，以及拥有型 FrameSnapshot 在
  update/reload/unload/Session 销毁后仍保持数据有效的生命周期测试。

### 1E-3：组件化构建

- 增加 App、adapter、test/example 和 shared/static 选项。
- 实现 `CUEXIS_LIBRARY_TYPE=STATIC|SHARED`，禁止同一 build/install prefix 混装两种 linkage。
- 建立 Core/Content/Audio/Playback/AudioSDL 导出宏、默认 hidden visibility 和内部 PIC 静态库。
- 使禁用组件时不查找对应第三方包。
- 增加 install/export/package config、版本文件和许可证安装。
- 增加已安装公共头依赖扫描。
- 使 shared package config 不传播 EnTT/GLM/JSON 等 private development dependencies。

### 1E-4：外部消费验证

- 扩展 add_subdirectory 和 install + find_package consumer，使 static/shared 分别运行。
- 两种 linkage 执行相同 headless load/update/seek/reload/unload 和 Frame hash 测试。
- shared consumer 从干净 staging 目录运行并验证 runtime deployment closure。
- 对比 Player、仓库内 headless、static consumer 和 shared consumer 的确定性结果。

### 1E-5：门禁与交付

- Debug/Release fresh configure、clean build、完整 CTest、format、architecture。
- 验证至少 headless-only、Player-full、static package 和 shared package 组合。
- 保留阶段 1A-1D 的 Chart、Project、GPU 和物理音频回归，并验证 AudioSDL 未请求时 SDL3 不进入消费闭包。
- 扫描 shared 导出符号和 imported libraries，拒绝内部实现符号及基础包的 SDL/OpenGL 依赖。
- 在 Windows/MSVC 与 Linux GCC/Clang 上执行 shared build/install/consumer 门禁。
- 创建 `docs/stage_reports/stage_1e_completion_report.md`。

## 9. 测试矩阵

| 范围 | 必须覆盖 |
| --- | --- |
| Provider | filesystem/memory/host 等价输入、缺失、短读、超限、异常、revision、生命周期 |
| Security | Filesystem containment 保留；Memory/Host 字节仍执行格式和预算校验 |
| Playback | load/update/seek/reload/unload、失败回滚、FrameSnapshot 独立生命周期、错误 owner |
| Multi-session | 独立 Clock/Provider、跨 Session 对象拒绝、无隐式全局当前状态 |
| CMake | 顶层/子目录、组件禁用、无后端 configure、Audio/AudioSDL、`STATIC`/`SHARED` 矩阵 |
| Install | headers、targets、config/version、generated header、licenses、NOTICES |
| Consumer | static/shared add_subdirectory 与 find_package 只使用公共接口；shared 从干净 staging 运行 |
| Shared ABI preview | Windows x64 MSVC 与 Linux x64 GCC/Clang；匹配工具链/CRT、导出宏、Cuexis factory/wrapper 创建与销毁、异常转换、owner thread/reentry |
| Architecture | 公共头、导出符号、import 和 Playback 链接闭包不泄漏后端/实现依赖 |
| Parity | Player、internal headless、static/shared consumer 的 Runtime/Frame hash 一致 |

## 10. Playback Core Preview 验收标准

```text
仓库外工程可以通过 add_subdirectory 或 find_package 消费 Cuexis::Playback preview
STATIC 与 SHARED package 均可安装、部署并从干净 consumer 目录运行
关闭 App、SDL、OpenGL 和物理音频时仍可完成完整 headless 播放流程
Project/Chart 可以来自 typed/memory source，资源可以来自 HostContentProvider
公共 SDK 头不暴露 EnTT、SDL、OpenGL、JSON DOM 或其他实现类型
多个 PlaybackSession 具有独立 owner、Clock、Provider、状态和诊断
失败 Provider/load/reload 不破坏上一有效 Session
销毁 Session 不留下线程、设备、日志全局状态或宿主 Context
Player 只使用正式 PlaybackSession，且结果与 external consumer 一致
安装包的组件依赖、版本、许可证和 THIRD_PARTY_NOTICES 正确
shared package 不要求 consumer 安装 private EnTT/GLM/JSON/SDL/OpenGL 开发依赖
导出符号不暴露 RuntimeSession、World、EnTT 或后端实现 API
```

## 11. 明确非目标

```text
稳定 C ABI、无需重编译的长期二进制兼容承诺和完整 Playback SDK v1 声明
跨 Cuexis minor、工具链、标准库、CRT、架构或 Debug/Release 混用 shared binaries
同一 install prefix 同时提供 static 与 shared Cuexis
运行时插件发现、任意 LoadLibrary/dlopen unload 或扩展插件 ABI
Unity、Unreal、C# 或其他正式语言绑定
跨图形 API 离屏纹理共享
异步 ContentProvider、协程、取消和流式资源
资源打包器、网络下载器或通用 VFS 实现
正式 Judgement、Score、InputProfile 和 ReplayData
Studio 与完整 Player 产品化
```

## 12. 配置与持久化

阶段 1E 不新增项目确定性字段或用户偏好格式。ContentProvider 和 HostCapabilities 是当前会话注入，不是全局配置文件。ProjectConfig v1/Asset Index 既有格式保持；typed/memory source 只是新的读取入口，不绕过 Schema、语义和预算校验。

## 13. 已解决与延后项

1. PlaybackSession/PlaybackSource 使用不透明内部状态，公共构造边界不再接受 AssetDatabase。
2. FrameSnapshot 保持拥有型有效期；长期大小优化继续由后续实测驱动，不改变 1E 契约。
3. external consumer 使用 `FrameDigest` algorithm version 1，golden value 为
   `6726938620466257503`。
4. shared 文件 stem 为 `cuexis_<module>-0.3`，Debug 增加 `d` postfix；Linux 同时设置
   `SOVERSION 0.3`，consumer 只通过 CMake target 引用文件。
5. Linux GCC shared Release 与 Clang shared Debug 已加入 CI；其远端执行结果仍是合入前证据，
   不由 Windows 本地报告代替。

## 14. 向阶段 2、6、11 和 12 的交接

阶段 2 的 Behavior 扩展必须持续通过 external consumer 和 FrameSnapshot 验证，不能重新要求宿主访问 World/EnTT。阶段 6 在 1E 的真实消费证据上稳定 C++ 使用、弃用和升级政策，但不冻结 C ABI。阶段 11 完成正式 Input/Judgement/Replay 公共契约及实现；阶段 12 才能基于完整 SDK 生命周期冻结 C ABI、语言包装和首个正式宿主 adapter。所有后续阶段都不得绕过 1E 已确认的所有权、线程和包组件边界。

# 阶段 1E 实施计划：SDK 封装与外部消费闭环

状态：Playback Core C++ preview 实施中；ContentProvider、静态包、组件开关与外部 consumer 基础门禁已落地
规划日期：2026-07-20  
进展更新：2026-07-27
最终验收前置：[阶段 1C 审查问题关闭](../stage_reports/260722-1c-review.md)、[阶段 1D 实施计划](stage_1d_implementation_plan.md)；独立的 packaging/consumer 工作允许提前实施
产品边界：[ADR 0027](../adr/0027-playback-sdk-product-boundary.md)、[ADR 0030](../adr/0030-playback-preview-api-version-and-result.md)、[ADR 0032](../adr/0032-playback-clock-and-prepared-audio-transaction.md)、[SDK 转型方案](cuexis_sdk_transition_plan.md)

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

当前已通过 1C P3 修复完成同步 Filesystem/Memory/Host ContentProvider、ResourceManager 与
Playback 注入、adapter-disabled preset、C++20 静态 `Cuexis::Playback`/`Cuexis::Content`
安装导出，以及 add_subdirectory/find_package 两种隔离 consumer。共享库导出、完整组件矩阵
和阶段 1E 最终 API 冻结仍未宣告完成。

阶段 1D 已在该基础上增加 `Cuexis::Audio` 与可选 `Cuexis::AudioSDL`，并把 preview SDK API
提升到当前的 `0.2.0`。AudioSDL 只有在 consumer 显式请求对应 component 时才传播 SDL3；
纯 Playback/HostClock consumer 不得因此查找、链接或初始化 SDL。

## 2. 已接受边界

```text
第一版 preview 消费接口为 C++20 + CMake package
阶段 1E 只记录当前 C++ 源码兼容范围，不承诺长期 ABI
稳定 C ABI、语言绑定和官方 Unity/Unreal adapter 必须在正式 Judgement/Replay 完成后进入阶段 12
ProjectConfig 继续是 Player/Studio 标准入口
SDK 同时接受 typed/memory source，不强制物理项目目录
AssetId、Asset Index、Handle、Lease、Scope 和 ResourceManager 语义保持
Filesystem、Memory 和 Host ContentProvider 使用同一逻辑索引与预算
公共 Playback 头不暴露 RuntimeSession、World、EnTT、SDL、OpenGL 或 JSON DOM
作为依赖构建时不自动构建 App、测试、格式 target 或复制 demo 资产
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
创建：注入内容源、预算、能力和诊断 Sink
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
```

内部模块可继续使用现有依赖。若第一版 C++ API 暂时适配 `core::Result`，必须记录源码兼容范围；稳定 C ABI 仍不得暴露 `tl::expected`。

## 5. CMake 组件化

现有选项保持兼容；阶段 1D/1E 组件矩阵至少表达以下能力：

```text
build Playback
build Player
build Studio
build SDL platform/audio adapters
build OpenGL adapter
build tests
build examples/external consumers
build static/shared
```

安装 target 名冻结为 `Cuexis::Playback`、`Cuexis::Content`、`Cuexis::Audio` 和
`Cuexis::AudioSDL`。Playback 可以传播后端无关 Audio；Playback 与 Audio 不得传播 SDL3，
AudioSDL 可以传播其真实 SDL3 依赖。

行为要求：

```text
顶层开发构建可保留现有便利默认值
add_subdirectory 消费默认不构建 App、测试、format 或复制 fixture
禁用 SDL/OpenGL 后 configure 不执行对应 find_package
每个导出组件只传播真实 PUBLIC 依赖
所有安装头使用 FILE_SET 或等价显式 allowlist
生成头同时支持 BUILD_INTERFACE 与 INSTALL_INTERFACE
```

## 6. 安装与 Package

必须生成和验证：

```text
CuexisTargets.cmake
CuexisConfig.cmake
CuexisConfigVersion.cmake
Cuexis::Playback、Cuexis::Content、Cuexis::Audio 与可选 Cuexis::AudioSDL 命名空间目标
公共 headers 与 generated version header
按已选组件安装的运行库、导入库和许可证文件
THIRD_PARTY_NOTICES 与实际分发组件一致
```

包版本兼容规则在本阶段首次记录，但不把项目显示版本误用为内容格式或未来 C ABI 版本。

## 7. External Consumer

建立位于正常测试边界之外、只使用公开消费方式的最小工程：

```text
consumer_add_subdirectory
  以 Cuexis 源码子目录消费

consumer_find_package
  先安装到临时 prefix
  当前基线使用 find_package(Cuexis 0.2 CONFIG REQUIRED COMPONENTS Playback Content)
  分别验证 Playback/Audio 与可选 AudioSDL component
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

## 8. 实施批次

### 1E-0：契约与迁移清单

- 冻结 ContentProvider 同步第一版的输入、输出、线程、重入、错误和 revision 语义。
- 冻结 Playback 公共头 allowlist、FrameSnapshot 有效期和 preview 包组件名；不把当前函数集合误称为长期稳定 ABI。
- 明确旧 AssetDatabase `readBlob` 到 Provider 的兼容迁移，不一次性重写 ResourceManager。

### 1E-1：内容来源分离

- 从 AssetDatabase 物理读取路径提取 FilesystemContentProvider。
- 实现 MemoryContentProvider 与测试 Provider。
- ResourceManager 只通过 Provider 读取已索引逻辑来源。
- 保持阶段 1B 路径安全、Handle/Lease/Scope 和事务回滚测试。

### 1E-2：Playback 公共组件

- 整理阶段 1C 的 `cuexis_playback` target 与安装头。
- 使用 Pimpl/内部实现或等价边界隐藏 RuntimeSession、World 和 EnTT。
- 将 Player 组合逻辑中可复用的 load/update/reload/extract 迁入 Playback；CLI/窗口/设备保留在 App。
- 增加 Session owner、跨 Session、销毁测试，以及拥有型 FrameSnapshot 在
  update/reload/unload/Session 销毁后仍保持数据有效的生命周期测试。

### 1E-3：组件化构建

- 增加 App、adapter、test/example 和 shared/static 选项。
- 使禁用组件时不查找对应第三方包。
- 增加 install/export/package config、版本文件和许可证安装。
- 增加已安装公共头依赖扫描。

### 1E-4：外部消费验证

- 建立 add_subdirectory consumer。
- 建立 install + find_package consumer。
- 两者执行相同 headless load/update/seek/reload/unload 和 Frame hash 测试。
- 对比 Player、仓库内 headless 和仓库外 consumer 的确定性结果。

### 1E-5：门禁与交付

- Debug/Release fresh configure、clean build、完整 CTest、format、architecture。
- 验证至少 headless-only、Player-full、static package 和支持的 shared package 组合。
- 保留阶段 1A-1D 的 Chart、Project、GPU 和物理音频回归，并验证 AudioSDL 未请求时 SDL3 不进入消费闭包。
- 创建 `docs/stage_reports/stage_1e_completion_report.md`。

## 9. 测试矩阵

| 范围 | 必须覆盖 |
| --- | --- |
| Provider | filesystem/memory/host 等价输入、缺失、短读、超限、异常、revision、生命周期 |
| Security | Filesystem containment 保留；Memory/Host 字节仍执行格式和预算校验 |
| Playback | load/update/seek/reload/unload、失败回滚、FrameSnapshot 独立生命周期、错误 owner |
| Multi-session | 独立 Clock/Provider、跨 Session 对象拒绝、无隐式全局当前状态 |
| CMake | 顶层/子目录、组件禁用、无后端 configure、Audio/AudioSDL、静态/共享支持矩阵 |
| Install | headers、targets、config/version、generated header、licenses、NOTICES |
| Consumer | add_subdirectory 与 find_package 只使用公共接口 |
| Architecture | 公共头与 Playback 链接闭包不泄漏后端/实现依赖 |
| Parity | Player、internal headless、external consumer 的 Runtime/Frame hash 一致 |

## 10. Playback Core Preview 验收标准

```text
仓库外工程可以通过 add_subdirectory 或 find_package 消费 Cuexis::Playback preview
关闭 App、SDL、OpenGL 和物理音频时仍可完成完整 headless 播放流程
Project/Chart 可以来自 typed/memory source，资源可以来自 HostContentProvider
公共 SDK 头不暴露 EnTT、SDL、OpenGL、JSON DOM 或其他实现类型
多个 PlaybackSession 具有独立 owner、Clock、Provider、状态和诊断
失败 Provider/load/reload 不破坏上一有效 Session
销毁 Session 不留下线程、设备、日志全局状态或宿主 Context
Player 只使用正式 PlaybackSession，且结果与 external consumer 一致
安装包的组件依赖、版本、许可证和 THIRD_PARTY_NOTICES 正确
```

## 11. 明确非目标

```text
稳定 C ABI、长期二进制兼容承诺和完整 Playback SDK v1 声明
Unity、Unreal、C# 或其他正式语言绑定
跨图形 API 离屏纹理共享
异步 ContentProvider、协程、取消和流式资源
资源打包器、网络下载器或通用 VFS 实现
正式 Judgement、Score、InputProfile 和 ReplayData
Studio 与完整 Player 产品化
```

## 12. 配置与持久化

阶段 1E 不新增项目确定性字段或用户偏好格式。ContentProvider 和 HostCapabilities 是当前会话注入，不是全局配置文件。ProjectConfig v1/Asset Index 既有格式保持；typed/memory source 只是新的读取入口，不绕过 Schema、语义和预算校验。

## 13. 剩余待确认

1. `cuexis_playback` 最终 Pimpl/内部实现拆分与仍未落地的错误返回细节。
2. FrameSnapshot 的长期大小预算；拥有型有效期已经由 ADR 0030 冻结。
3. shared library 在阶段 1E 的支持范围；本阶段不冻结稳定 C ABI。
4. external consumer fixture 的最终确定性 hash 规范。

同步 ContentProvider、Prepared Playback/MainMusicSourceView、owner-thread/reentry 规则、
`Cuexis::Audio`/`Cuexis::AudioSDL` 名称和 `0.2.0` 版本提升已由 ADR 0030/0032 与阶段 1D 冻结，
不再列为开放选择。

## 14. 向阶段 2、6、11 和 12 的交接

阶段 2 的 Behavior 扩展必须持续通过 external consumer 和 FrameSnapshot 验证，不能重新要求宿主访问 World/EnTT。阶段 6 在 1E 的真实消费证据上稳定 C++ 使用、弃用和升级政策，但不冻结 C ABI。阶段 11 完成正式 Input/Judgement/Replay 公共契约及实现；阶段 12 才能基于完整 SDK 生命周期冻结 C ABI、语言包装和首个正式宿主 adapter。所有后续阶段都不得绕过 1E 已确认的所有权、线程和包组件边界。

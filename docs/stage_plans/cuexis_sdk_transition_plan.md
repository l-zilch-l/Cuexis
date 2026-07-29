# Cuexis SDK 改造与阶段路线调整方案

状态：产品方向与阶段调整已接受；阶段 1C、1D 功能边界和阶段 1E Playback Core preview 已实现并完成跨平台验收；稳定 C ABI 延后到必选 Judgement/Replay 完成后的阶段 12
规划日期：2026-07-20  
当前基线：[阶段 1D 完成报告](../stage_reports/stage_1d_completion_report.md)
相关实施计划：[阶段 1C](stage_1c_implementation_plan.md)、[阶段 1D](stage_1d_implementation_plan.md)、[阶段 1E](stage_1e_implementation_plan.md)
shared preview 边界：[ADR 0033](../adr/0033-cpp-shared-library-preview-boundary.md)

## 0. 文档定位

本文定义 Cuexis 从“以完整音乐游戏引擎为长期方向”调整为“可嵌入的 Cuexis 谱面处理与播放 SDK”的正式产品方向，并重新审视阶段 0、1A、1B 已完成内容以及阶段 1C 至阶段 12 的后续路线。

本文与 [ADR 0027](../adr/0027-playback-sdk-product-boundary.md) 共同作为 SDK 产品边界和阶段调整的权威来源。既有阶段完成报告继续作为历史事实保留；既有 ADR 的内部模块决策在不与 ADR 0027 冲突时继续有效。本文中的示例类型名只表达职责，不代表已经冻结的公共 API。

## 1. 新产品目标

Cuexis 的核心交付物调整为：

```text
Cuexis Playback SDK
  处理、校验、编译和播放 Cuexis 谱面及其受支持资源
  承担谱面播放期间的判定、计分、按键采样记录与确定性回放
  可作为其他游戏引擎、应用或工具的第三方依赖
  不拥有宿主的游戏主循环、场景系统或完整玩法框架

Cuexis Player
  使用 Playback SDK、SDL 和内建渲染后端组成的独立参考播放器
  提供项目加载、播放控制、诊断和独立应用体验

Cuexis Studio
  独立谱面与资源编辑程序
  使用同一 Playback SDK 进行预览，但不依赖 Player 的 CLI、窗口或主循环实现
```

Cuexis 的核心价值集中在：

```text
Cuexis 格式、版本和迁移
谱面与资源的确定性校验和编译
Timing、Behavior、Animation 和表现时间轴
资源身份、生命周期、Reload 和失败回滚
任意时间采样、Seek 和可重复播放
按键采样记录与确定性回放
后端无关的帧输出、诊断和宿主集成契约
```

## 2. 明确非目标

Playback SDK 不以独立完成一款通用游戏为目标。以下能力不进入核心 SDK 的必选范围：

```text
通用场景编辑和关卡系统
物理、导航、AI、联网、账号和排行榜
宿主窗口、应用主循环和平台生命周期总控
完整 UI、存档、本地化、成就和商店发布框架
通用角色动画、游戏对象脚本或任意游戏逻辑插件系统
强制由 Cuexis 管理输入设备、音频设备或图形 Context
完整玩法状态机、UI、排行榜和通用游戏流程框架（判定与计分结果本身属于 SDK 交付物，见阶段 11）
```

Cuexis 可以提供后端适配器、参考实现和可选扩展，但这些模块不得反向成为 Chart、Runtime 或 Playback SDK 的必选依赖。

## 3. 产品与 Target 拆分

推荐目标结构如下，具体 target 名称在实施前通过 ADR 确认：

```text
公共 SDK
  cuexis_playback           高层播放会话和宿主门面
  cuexis_judgement          判定与计分模块（Playback SDK 必选交付）
  cuexis_chart              可选的格式、校验和编译前端

内部前端模块
  cuexis_core
  cuexis_project
  cuexis_assets
  cuexis_world
  cuexis_runtime
  cuexis_behavior
  cuexis_animation
  cuexis_particles
  cuexis_render
  cuexis_audio

可选适配器
  cuexis_content_filesystem
  cuexis_platform_sdl
  cuexis_audio_sdl
  cuexis_render_opengl
  后续宿主引擎或 C ABI 适配器

独立应用
  cuexis_player
  cuexis_studio
```

`cuexis_player` 不再承载可复用播放逻辑，只负责独立应用组合。Player 和 Studio 都必须通过 `cuexis_playback` 使用正式播放路径，不能各自直接拼装另一套 Chart -> Runtime -> Render 流程。

## 4. 宿主与 SDK 职责

| 领域 | Playback SDK | 宿主项目或独立应用 |
| --- | --- | --- |
| 主循环 | 接收显式帧输入并更新 Session | 决定何时更新、暂停和退出 |
| 时间 | 校验并消费 `chartTimeMs`、delta 和 discontinuity | 提供宿主时钟，或选择 Cuexis AudioClock 适配器 |
| 内容 | 校验、编译、解析 AssetId 和资源依赖 | 提供文件系统、VFS、归档或内存中的字节 |
| 音频 | 定义后端无关时钟/Transport 契约 | 使用宿主音频，或选择 `audio_sdl` |
| 渲染 | 输出稳定的 FrameSnapshot/RenderPacket | 使用宿主渲染适配器，或选择 Cuexis 内建后端 |
| 输入 | 接收标准化 InputEvent/InputFrame | 将宿主输入事件转换为 InputEvent，传给 SDK |
| 判定与计分 | **计算并输出判定事件、分数、连击和统计数据** | 消费判定结果、管理游戏状态和 UI 展示 |
| 记录与回放 | **按 chartTimeMs 记录全部 InputEvent，生成可复现的回放文件** | 决定何时开始/停止记录，保存或加载回放数据 |
| 日志 | 发布结构化诊断与可注入日志 Sink | 决定日志系统、界面展示和持久化 |
| 配置 | 接收已校验的 typed config | 定位并组合应用、用户和设备配置 |

SDK 必须支持没有 SDL、OpenGL、物理音频设备和真实窗口的 headless 使用方式。

## 5. 高层播放接口方向

高层 SDK 需要建立单一 `PlaybackSession` 门面。以下仅表达职责，不冻结类型名或签名：

```text
create             注入内容源、诊断 Sink、预算和宿主能力
prepare/load       从 Project、Chart 或内存输入构建候选会话
commit             原子发布候选会话
update             消费绝对时间帧输入和宿主 InputEvent
seek               通过新时间和 discontinuity 完整重采样
reload             失败保留上一有效会话
extractFrame       返回后端无关且有明确生命周期的帧快照
extractResult      返回判定事件、分数、连击和统计数据的累积快照
startRecording     开始记录本次播放的全部 InputEvent（按 chartTimeMs 时间戳）
stopRecording      停止记录并返回可序列化的 ReplayData
loadReplay         从 ReplayData 注入预记录 InputEvent，替代宿主实时输入
query              查询谱面身份、时长、对象或播放状态
unload             释放 World、Scope、派生资源和会话状态
diagnostics        返回稳定错误码、字段路径和上下文
```

公共 SDK 边界不得暴露：

```text
entt::entity、entt::registry 或 cuexis::world::World
SDL_Window、SDL_AudioDeviceID 或其他 SDL 类型
GLuint、OpenGL Context 或后端专用枚举
JSON DOM、nlohmann 类型或 Schema Validator 类型
spdlog、fmt 或第三方容器类型
内部 ResourceManager 槽位、线程对象或后端缓存对象
```

Playback Core preview 的 C++20 公共 API 允许使用 `cuexis::core::Result<T, E>`；该别名由
`tl::expected` 实现，因此 preview consumer 存在明确的 `tl-expected` 头文件源码依赖。公共
签名不得直接写出 `tl::expected`，也不得暴露其他第三方类型。这个例外只适用于不承诺二进制
兼容的 C++ preview；阶段 12 的稳定 C ABI 不得暴露 `tl::expected`、STL 所有权或跨模块异常。
其余高层 SDK 数据继续使用 Cuexis 自有 ID、数学值、状态、诊断和拥有型快照隔离内部实现。

## 6. API、ABI 与语言绑定策略

第一步先建立可测试的 C++20 SDK 门面和源码消费方式，并在阶段 1E 验证 static 与同工具链
shared preview。Session 生命周期和 Judgement/Replay 尚未稳定时，不承诺长期二进制 ABI。

```text
阶段 1E
  验证 C++ 所有权、线程和错误语义，记录 preview 源码兼容范围
  提供 static/shared CMake package、部署门禁和外部 C++ consumer
  shared consumer 使用匹配工具链/运行时并在升级 SDK 后重新编译

阶段 6
  稳定 C++ 使用、弃用和升级政策
  继续真实宿主验证，但不冻结 C ABI

阶段 11
  完成必选 Input/Judgement/Replay 公共契约、实现和外部消费验证

阶段 12
  根据完整 SDK 生命周期证据冻结 opaque handle C ABI 与长期二进制兼容政策
  提供薄 C++ RAII wrapper，再进入 Unity、C# 或其他语言适配
```

稳定二进制边界必须满足：

```text
异常不跨模块边界
创建模块负责释放其内存
字符串、数组和快照具有明确有效期
回调线程和重入规则可测试
公共结构具有 size/version 字段或等价版本策略
可以查询 SDK 版本、ABI 版本和启用组件
Windows CRT、Debug/Release 和静态/动态链接组合有明确支持矩阵
```

## 7. 内容与资源来源改造

ProjectConfig、Asset Index、AssetId、Handle、Lease、Scope 和资源事务继续保留。需要把“资产身份和依赖”与“从本机路径读取字节”分开：

```text
AssetDatabase
  保存 AssetId、类型、逻辑来源、依赖和 root 身份

IContentProvider
  根据受校验的逻辑来源返回有界字节或只读流

FilesystemContentProvider
  实现现有 portable path、physical containment 和原子保存规则

HostContentProvider
  由宿主连接 Unity/Unreal 资源包、VFS、归档、下载缓存或内存
```

规则：

```text
所有宿主提供的字节仍按不可信输入执行格式、范围和预算校验
SDK 不枚举宿主目录，也不根据文件名猜测 AssetId
ProjectConfig 是 Player/Studio 的标准项目入口，但不是嵌入 SDK 的唯一输入方式
嵌入方可以直接提供已读取的 Project/Chart/Index 文本和逻辑内容 Provider
文件系统安全规则继续适用于 Filesystem adapter，不强加给非文件系统 Provider
ResourceManager 不直接打开任意路径，只通过已注入 Provider 请求已索引内容
```

阶段 1B 已有同步加载可以继续作为第一版。异步、取消和流式读取仍等待真实宿主需求，不因 SDK 改造提前实现通用任务系统。

## 8. 时间与音频改造

阶段 1C 的绝对 `RuntimeFrame` 是嵌入模式的基础，应保留并提升为 Playback SDK 的正式输入语义：

```text
chartTimeMs
simulationDeltaTimeMs
timeDiscontinuityId
```

音频/时间支持三种显式组合模式：

```text
ChartClock 模式
  只用于明确没有主音乐引用的 Chart
  不创建音频设备

HostClock 模式
  宿主负责播放主音乐
  宿主提交 SourceClockSample
  Cuexis 不创建音频设备

CuexisAudio 模式
  cuexis_audio_sdl 或未来其他后端播放主音乐
  AudioClockSnapshot 转换为 SourceClockSample
```

`cuexis_playback` 的 RuntimeTimeline 把三种来源、Chart offset 和播放状态统一为 RuntimeFrame。
PlaybackSession 本身仍只消费 RuntimeFrame。Chart 中存在主音乐引用时，内容引用仍是 Required；
但“由谁解码和创建设备”由已选择的运行模式决定。SDL 设备失败导致启动失败是独立 Player 的
策略，不得变成所有嵌入宿主的统一要求。模式在准备时冻结，失败和 reload 均禁止无诊断切换。

判定结果按 `chartTimeMs` 时间戳索引，与宿主时钟模式无关。同一谱面在 HostClock 与
CuexisAudio 模式下对相同 SourceClockSample/control script 和 InputEvent 序列产生相同结果。

## 9. 渲染集成改造

渲染是 SDK 转型风险最高的部分，必须把公共帧语义与具体图形 API 分开。

第一层必须是后端无关输出：

```text
RuntimeSession
-> Render extraction
-> immutable FrameSnapshot / RenderPacket
-> Host Render Adapter 或 Cuexis RenderBackend
```

第一版嵌入验收优先支持宿主消费帧数据，不直接承诺跨引擎共享 OpenGL/Vulkan Context。后续可以按真实宿主增加：

```text
宿主 RenderPacket 转换器
离屏纹理输出适配器
Unity Native Plugin
Unreal Module
其他图形 API 专用桥接
```

阶段 3、5 和 8 必须定义表现能力分层：

```text
Portable Presentation Profile
  宿主适配器应能实现的标准 Transform、Mesh、材质参数和粒子数据

Built-in Renderer Profile
  Cuexis Player/Studio 内建后端支持的高级 Shader、Pipeline 或调试效果

Host-specific Extension
  由特定宿主适配器声明，不改变基础 Chart/Runtime 语义
```

不能承诺任意 Cuexis Shader 自动映射到所有游戏引擎。Project/Chart 必须能声明所需表现能力，宿主不满足时产生确定性诊断或显式降级，不能静默改变结果。

## 10. 构建、安装与分发改造

根 CMake 必须从“始终构建所有当前应用”改为组件化构建。推荐选项：

```text
CUEXIS_BUILD_PLAYER
CUEXIS_BUILD_STUDIO
CUEXIS_BUILD_SDL_BACKEND
CUEXIS_BUILD_OPENGL_BACKEND
CUEXIS_BUILD_TESTS
CUEXIS_BUILD_EXAMPLES
CUEXIS_LIBRARY_TYPE=STATIC|SHARED
```

作为顶层项目时可以使用面向开发的默认值；通过 `add_subdirectory` 或安装包消费时，不应自动构建 Player、Studio、测试、格式 target 或复制 demo 资产。

必须提供：

```text
install(TARGETS ... FILE_SET HEADERS ...)
CuexisTargets.cmake
CuexisConfig.cmake
CuexisConfigVersion.cmake
命名空间导出目标 Cuexis::Playback 等
BUILD_INTERFACE / INSTALL_INTERFACE include 路径
shared library 导出宏和符号可见性规则
按组件记录第三方依赖、许可证和分发义务
```

至少维护两种消费验证：

```text
add_subdirectory consumer
find_package(Cuexis 0.3 CONFIG REQUIRED) consumer
```

当前 static/shared preview 为 `0.3`，consumer 必须显式请求所接受的版本。两种 linkage 均覆盖
Playback/Content/Audio 与可选 AudioSDL 的 component-aware 行为；同一 install prefix 不得
混装 static/shared。

## 11. 阶段 0 至 1B 调整

### 11.1 阶段 0：工程骨架

阶段 0 的主体成果保留：

```text
CMake/vcpkg 基线
Core 与 SDL/OpenGL 后端隔离
Cuexis 自有数学和错误模型
World、Render 前端和 OpenGL Backend
模块化测试、格式检查和架构扫描
```

需要追加的 SDK 改造：

```text
把 app 构建改为可选
建立 install/export/package 基础
定义 public/private header 和符号可见性规则
架构扫描增加“公共 SDK 头不得包含后端和第三方实现类型”
增加外部 consumer 构建测试
日志初始化从 SDK 全局行为移到应用或宿主注入
```

不需要重写 Core、World 或 OpenGL Backend；它们从“完整引擎基础”重新定位为 SDK 内部模块与可选参考后端。

### 11.2 阶段 1A：规范谱面与实例化闭环

阶段 1A 的主体成果保留：

```text
Canonical/Simple Chart loader
Cuexis-owned JSON Value、Reader、Schema artifact 和稳定字段路径诊断
有理数 Beat、TimingMap 和确定性 ChartRuntime 编译
ChartDocument -> ChartRuntime -> RuntimeSession 唯一路径
事务式 RuntimeSession、World 实例化和 RenderScene
```

需要调整：

```text
把现有 RuntimeSession 定位为内部会话实现
新增不暴露 World/EnTT 的 PlaybackSession 门面
findEntity(entt::entity) 和 withWorld 只保留为内部或受限调试 API
Chart loader 继续支持文本/内存输入，不要求宿主先建立本机文件
NullInput/NullJudge 是阶段 1A 的占位实现，SDK 版本将通过 PlaybackSession 的 InputEvent 和判定模块提供正式判定与计分能力
NoteTag/ElementTag 只有在属于 Cuexis 格式语义时保留，不扩张为完整游戏对象框架
```

### 11.3 阶段 1B：资源生命周期闭环

阶段 1B 的主体成果保留：

```text
ProjectConfig v1 与 {root, path} bootstrap locator
独立 cuexis.asset-index.json
AssetId、AssetDatabase 和稳定依赖图
ResourceHandle、manager token、generation 和 contentRevision
Lease、Scope、Required/Fallback/Optional
Runtime prepare/reload 的资源事务与失败回滚
```

需要调整：

```text
AssetDatabase 不再承担唯一的物理文件读取实现
把 readBlob 的实际 I/O 委托给 IContentProvider
保留 Filesystem adapter 的路径安全和 containment 规则
增加 Memory/Host Provider，用同一索引和预算读取宿主资源
PreparedProject 增加从已读取配置和逻辑 root 构建的应用侧入口
ProjectConfig 继续是 Player/Studio 标准入口，但嵌入 SDK 可以从 typed/memory 输入启动
测试增加宿主 Provider 失败、短读、超限、revision 变化和多 Session 共享/隔离
```

## 12. 阶段 1C 及之后的路线调整

### 12.1 阶段 1C：时间、基础行为与 Headless Playback

保留现有 Transform Keyframe、绝对时间采样、PropertyResolver 和 `RuntimeFrame` 计划，并增加：

```text
建立第一版 PlaybackSession C++ 门面
支持宿主直接提交 RuntimeFrame
输出不依赖 SDL/OpenGL 的 FrameSnapshot
Player 改为 PlaybackSession 的薄组合层
增加 headless、多 Session、重复 Seek 和宿主帧序列测试
保证同一 Chart/RuntimeFrame 在 Player 与 headless consumer 中得到相同结果
```

1C 不负责稳定 C ABI、Unity/Unreal 插件或异步资源系统。

### 12.2 阶段 1D：主音乐内容与可选音频适配器

按 ADR 0031/0032 冻结 Asset Index v2、Chart v2 主音乐引用、AudioClip、AudioClock、
Prepared Playback 和 SDL AudioTransport：

```text
cuexis_audio 保持后端无关
cuexis_audio_sdl 是可选组件
cuexis_playback 依赖 cuexis_audio，但不依赖 cuexis_audio_sdl/SDL
正式支持 ChartClock、HostClock 与 CuexisAudio 三种不可变模式
Player 使用 CuexisAudio，headless consumer 使用 ChartClock/HostClock/FakeClock
Prepared Playback 内部持有 Source Lease，只公开 MainMusicSourceView
主音乐内容 Required 规则与音频设备所有权规则分开
安装导出 Cuexis::Audio 与显式请求的 Cuexis::AudioSDL，preview API 提升到 0.2.0
```

### 12.3 新增阶段 1E：Playback Core Preview 与外部消费闭环

新的第三方依赖目标使现有“1D 后直接进入阶段 2”的安排不再充分。正式新增 1E，专门验证 Playback Core 是否可以作为 C++20 preview 被仓库外项目安装和消费。该阶段不宣告完整 Playback SDK v1，也不承诺稳定 ABI。

任务：

```text
完成 CMake install/export/package
完成组件化 static/shared 构建选项和无 App 构建
建立公开头 allowlist、符号可见性和版本查询
建立 Filesystem、Memory 和 Host Content Provider
建立独立 external consumer fixture，不直接访问仓库私有头
验证 load/update/seek/reload/unload 和 FrameSnapshot
验证无 SDL、无 OpenGL、无音频设备的完整 headless 闭环
验证 Player 与 external consumer 的确定性一致性
验证 shared consumer 的干净目录部署、私有依赖闭包和 static/shared parity
记录第一版 C++ SDK 兼容性政策
```

验收标准：

```text
外部工程可以通过 find_package(Cuexis) 链接 Playback 组件
关闭全部可选后端时仍能解析、编译、更新和提取帧
公共 SDK 头不暴露 EnTT、SDL、OpenGL 和私有 JSON 类型
销毁 SDK Session 不留下全局线程、设备、日志或缓存状态
多个 Session 可以使用独立时钟和资源 Provider
shared 组件只导出正式公共符号，基础 Playback closure 不引入 SDL/OpenGL
shared consumer 升级 SDK 后重新编译，且使用匹配的工具链、运行时和配置
```

### 12.4 阶段 2：Cuexis Behavior 表达能力

保留通用 Curve、TimingMap、BehaviorClip、Material/Visibility Track 和调试能力，但目标改为“表达 Cuexis 谱面表现”，不扩张为任意游戏脚本系统。

新增要求：

```text
所有 Behavior 可以通过 PlaybackSession 任意时间采样
宿主不需要访问 World 或 EnTT 即可获得结果
Behavior 扩展具有版本和能力声明
不得执行宿主任意代码、脚本或无界回调
```

### 12.5 阶段 3：可移植表现前端与渲染适配

将“通用渲染前端稳定化”调整为“Cuexis 表现输出稳定化”：

```text
冻结 FrameSnapshot/RenderPacket 所有权和有效期
定义 Portable Presentation Profile
定义宿主 Camera/Viewport 输入和坐标转换契约
实现内建 OpenGL adapter，并建立一个不渲染的验证 Sink
明确材质、纹理、Mesh 和排序数据如何交给宿主
验证宿主跳过不支持 Pass 时的诊断与显式降级策略
```

Light、完整 RenderGraph、通用 Buffer/Pipeline API 只有在 Cuexis 表现内容存在真实消费者时才进入实现。

### 12.6 阶段 4：Cuexis 表现动画

保留 AnimationClip、Layer、BlendGroup、OverrideToken 和 PropertyResolver，但范围限制为 Cuexis 谱面/资源预览所需动画。

```text
不建设通用角色状态机、骨骼动画控制器或游戏对象脚本系统
HostOverride 用于宿主临时覆盖可公开属性
StudioPreviewOverride 用于编辑预览
覆盖结束后仍按确定性基线恢复
```

### 12.7 阶段 5：材质、Shader 与宿主能力 Profile

保留版本化 Material、ShaderAsset、ImporterProfile、ShaderTargetProfile、缓存和热重载，但增加宿主可移植性分层。

```text
Portable material schema 是跨宿主最低契约
自定义 Shader 可以只声明 Built-in Renderer Profile 支持
宿主 adapter 显式报告能力，不承诺自动翻译任意 Shader
Project 加载时校验所需 profile 与宿主能力
Studio 可以编辑高级 Shader，但必须显示目标宿主兼容性
```

### 12.8 阶段 6：Playback SDK 与独立 Player 产品化

原“Cuexis Player 产品化”调整为 SDK 与独立 Player 共同产品化：

```text
稳定 PlaybackSession C++ 使用、弃用和升级政策
在已支持的 shared preview 基础上继续验证真实宿主和 C++ 升级/弃用政策，但不冻结 opaque handle C ABI
提供 preview 版本查询并为后续薄 C++ RAII wrapper 保留边界
完善安装包、许可证、符号、Debug/Release 和升级说明
完善 Player 用户控制、UserPreferences 和 AudioDeviceProfile
Player 继续作为 SDK 的唯一独立参考播放器
建立至少一个真实宿主适配证明，但不把该宿主依赖引入核心 SDK
```

Player 的用户偏好和设备配置属于独立应用；嵌入宿主可以直接提供 typed launch/session config，不需要使用 Player UserPreferences。

### 12.9 阶段 7：Cuexis Studio 独立程序

Studio 保持独立应用定位，并调整依赖关系：

```text
EditorDocument 与 Runtime 继续分离
Viewport 只通过 Playback SDK 预览
Studio 不链接 Player 的 CLI、窗口或 UserPreferences 实现
Studio 与 Player/外部宿主对同一 Project/Chart 得到相同编译和采样结果
Studio 显示目标宿主 Profile 和不兼容表现能力
```

### 12.10 阶段 8：可选粒子表现扩展

粒子只作为 Cuexis 表现扩展，不作为通用游戏粒子引擎：

```text
CPU 确定性时间轴、Checkpoint 和 Seek 语义保留
通过 ParticleRenderPacket 或等价快照输出给宿主
内建渲染器实现参考 adapter
宿主不支持时按能力声明失败或显式使用受控降级
```

### 12.11 阶段 9A：SDK 与宿主性能验证

在现有桌面性能、资源、音频和时间精度测量上增加：

```text
PlaybackSession update/extract 成本
FrameSnapshot 大小、复制次数和生命周期
宿主回调、VFS 和资源上传成本
多 Session 内存与线程开销
内建 Player 与外部 consumer 的性能差异
shared library/C ABI 调用和数据交换开销
```

DeviceProfile 仍提供硬预算，但宿主能力与 Cuexis 内建设备能力必须区分来源。

### 12.12 阶段 9B：移动端与宿主适配验证

Android 验证从“完整移动端游戏发布”调整为：

```text
Android SDK 构建、安装包和宿主生命周期验证
OpenGL ES 或宿主渲染 adapter 能力验证
APK/AAB 资源或宿主 AssetManager Content Provider
后台恢复、Context 丢失、音频时钟和内存压力
不要求 Cuexis 自己提供完整移动端 UI 与游戏外壳
```

### 12.13 阶段 10：可选内建 Vulkan Backend

Vulkan 只验证 Cuexis 内建渲染 adapter，不影响 Playback SDK、Chart、Behavior、World 或宿主 RenderPacket 契约。没有独立 Player/Studio 或目标宿主需求时，可以继续延期。

### 12.14 阶段 11：输入、判定与计分

前置条件：完成阶段 7 Studio 核心和阶段 9A 性能验证。

目标：实现 Cuexis 判定与计分模块，使 SDK 承担谱面播放期间的判定计算职责，并在播放结束后将判定结果回传给宿主。同时实现按键采样记录与确定性回放能力。

任务：

```text
定义带单调时间戳、来源、到达时间和 sequence 的 InputEvent / InputFrame
宿主通过 PlaybackSession::update(frame, inputEvents) 提交标准化输入事件
定义 JudgementEvent、ScoreResult、Combo 和 Statistics 的稳定公共类型
实现 cuexis_judgement 模块：Tap/Miss 判定、分数计算、连击和统计
PlaybackSession::extractResult() 返回从 Session 开始累积的判定结果快照
判定结果快照包含：判定事件列表、当前分数、最大连击、精度统计
宿主消费判定结果，自行管理游戏状态、UI 展示和玩法流程
```

#### 按键采样记录与回放

```text
PlaybackSession::startRecording()
  开始按 chartTimeMs 时间戳记录 Session 生命周期内收到的全部 InputEvent
  记录内容包含：InputEvent 完整数据、chartTimeMs、frameIndex

PlaybackSession::stopRecording() -> ReplayData
  停止记录，返回可序列化的 ReplayData（包含 InputEvent 序列、Session 配置快照）

PlaybackSession::loadReplay(ReplayData)
  注入预记录的 InputEvent 序列替代宿主实时输入
  注入后宿主仍可正常调用 update/extractFrame/extractResult
  ReplayData 中的 Session 配置快照与当前 Session 不一致时产生明确诊断
回放模式下宿主不得同时提交实时 InputEvent，否则 Session 拒绝并返回错误
```

`arrivalTime` 和 `frameIndex` 是诊断/审计元数据，不参与判定或回放调度；语义顺序由规范化事件时间、chartTimeMs 和 sequence 决定。事件数量、序列化字节和 JudgementResult 历史必须有阶段 11 冻结的硬上限，`extractResult()` 不得每帧复制无界累积列表。

验收标准：

```text
给定相同谱面、相同 InputEvent 序列和相同校准参数时，判定和分数结果确定一致
判定模块不依赖 SDL、具体音频后端或渲染后端
宿主可以在停止播放后获取完整判定结果快照
纯播放和 Studio 预览可以不注入 InputEvent，此时判定模块处于休眠状态
Score、Combo 和 Statistics 的计算属于 cuexis_judgement，不作为宿主必选实现
定义宿主可查询的 Note/Event 时间流和稳定对象 ID，供宿主自行扩展判定逻辑
startRecording 后记录的 InputEvent 序列与宿主提交的原始事件完全一致
同一谱面分别实时播放和回放时，extractResult() 和 extractFrame() 结果确定一致
ReplayData 序列化后反序列化再注入，结果与原始实时播放确定一致
回放模式下宿主尝试提交实时 InputEvent 时，Session 返回明确错误
```

`cuexis_judgement` 是 Playback SDK 的必选交付模块。对不需要判定的纯播放或 Studio 预览场景，不注入 InputEvent 即自动跳过判定。判定结果不进入 Chart、Behavior 或 World 的持久化数据。

ReplayData 与 Session 配置快照按版本化格式序列化。版本不支持、快照字段不一致或回放内容受损时，回放加载失败并返回稳定诊断。

### 12.15 阶段 12：稳定 ABI 与正式 Playback SDK v1

前置条件：阶段 11 的 Input/Judgement/Replay 公共契约、实现、外部 consumer 和确定性回放门禁全部完成，且阶段 1E/6 已积累 C++ 所有权、线程、错误和真实宿主消费证据。

```text
冻结 opaque handle C ABI、allocator、字符串、数组、回调和快照有效期，以及长期二进制兼容政策
定义 C ABI version、符号可见性、兼容/弃用和 capability 查询
验证 Windows CRT、Debug/Release、static/shared 与支持平台矩阵
提供薄 C++ RAII wrapper 和至少一个正式宿主 adapter
发布完整 Playback SDK v1 集成、升级、许可证和部署文档
```

阶段 12 不改变 Chart、Project、Asset Index、ReplayData 等内容格式版本；C ABI 版本、C++ SDK API 版本、项目显示版本和内容版本继续独立演进。

## 13. 配置整合调整

现有“无全局 EngineConfig、应用层组合 typed config”的原则保留。配置消费者调整如下：

| 配置 | 独立 Player | Studio | 嵌入宿主 |
| --- | --- | --- | --- |
| ProjectConfig | 标准启动入口 | 标准项目入口和写回 | 可选；也可提交 typed/memory project source |
| UserPreferences | Player 自有 | Studio 自有 | 不读取 Player/Studio 偏好 |
| DeviceProfile | 内建后端匹配和预算 | 预览与兼容性检查 | 由宿主能力快照和 Cuexis Profile 共同约束 |
| LaunchOptions | CLI/平台入口 | 编辑器启动入口 | 宿主创建参数 |
| ResolvedAppConfig | Player 组合结果 | Studio 组合结果 | 宿主 adapter 组合结果 |
| ResolvedSessionConfig | 播放会话确定性快照 | 预览会话快照 | PlaybackSession 创建时注入 |
| JudgementResult | Session 结束或查询时的只读快照 | Studio 预览可查询 | **SDK 返回的判定结果属于宿主，宿主负责持久化和展示** |
| ReplayData | 不直接生成；通过 SDK API 导出/导入 | 不直接生成 | **宿主保存/加载/分享回放文件，ReplayData 属于宿主** |

SDK 不读取 Player 或 Studio 的用户目录，不写回宿主配置，也不通过全局单例修改活动 Session。

## 14. 测试与门禁

SDK 转型后的最低门禁：

```text
内部模块单元测试和现有阶段回归全部保留
headless PlaybackSession 测试不加载 SDL/OpenGL
Filesystem/Memory/Host Content Provider 使用同一输入得到相同结果
外部 consumer 只能包含已安装公共头
add_subdirectory 与 find_package 两种消费方式通过
多个 Session、不同线程 owner、重复创建销毁和失败回滚通过
宿主 Clock、Cuexis AudioClock 和直接目标采样结果一致
Player、Studio Preview 和 external consumer 对相同输入产生相同 Runtime/Frame hash
公共头依赖扫描拒绝 EnTT、SDL、OpenGL、JSON DOM 和其他实现类型
静态库、共享库和支持的 MSVC 运行库组合有构建测试
相同谱面与 InputEvent 序列产生确定一致的判定、分数和统计结果
无 InputEvent 的 Session 判定模块休眠，不影响纯播放和 Studio 预览
判定结果快照的公共类型不包含 EnTT、SDL 或后端实现类型
startRecording 记录的事件与宿主提交的原始 InputEvent 完全一致
实时播放与回放模式的 extractResult/extractFrame 输出确定一致
ReplayData 序列化/反序列化往返后，注入回放结果与原始实时播放确定一致
回放模式下宿主提交实时 InputEvent 时返回明确错误
```

真实 Unity、Unreal 或移动宿主验证不替代通用 consumer 测试；特定宿主 adapter 的失败不能改变核心 SDK 的平台无关契约。

## 15. 迁移顺序

改造采用渐进方式，始终保持独立 Player 可运行：

```text
1. 接受产品边界和 SDK 架构 ADR
2. 在 1C 内建立 headless RuntimeFrame + PlaybackSession 最小闭环
3. 将 Player 现有组合逻辑迁入可复用门面，Player 改为薄应用层
4. 在 1D 建立 ChartClock/HostClock/CuexisAudio、RuntimeTimeline 与 Prepared Playback
5. 引入 ContentProvider，同时保留现有 Filesystem 行为和路径安全
6. 在 1E 完成安装包和外部 consumer
7. 外部消费门禁通过后再扩展阶段 2 以上表现系统
8. 阶段 11 完成必选 Judgement/Replay 并通过外部验证后，阶段 12 再冻结 C ABI 和官方宿主 adapter
```

不得通过一次性重写 Runtime、ResourceManager 或 Player 来完成转型。每一步都必须保持现有 Chart fixture、资源事务、Reload 回滚和 GPU smoke 可复核。

## 16. 风险与控制

| 风险 | 控制措施 |
| --- | --- |
| PlaybackSession 成为新的 God Object | 只编排会话；格式、资源、音频和渲染仍由各自模块拥有 |
| 为支持所有宿主过度抽象 | 先用 headless consumer 和一个真实宿主验证，不提前建立动态插件框架 |
| 公共 API 泄漏内部依赖 | 公共头 allowlist、安装后 consumer 和 include 扫描门禁 |
| 宿主 VFS 绕过路径安全 | 所有字节继续做格式/预算校验；文件系统 containment 只归 Filesystem adapter |
| 音频时间语义分裂 | 三种 Clock 先归一化为 SourceClockSample，再由同一 RuntimeTimeline 生成 RuntimeFrame |
| 任意 Shader 无法跨引擎 | Portable/Built-in/Host-specific Profile 分层和能力诊断 |
| C ABI 过早冻结 | 先冻结 C++ 生命周期和所有权，再冻结 opaque handle ABI |
| Player 与 SDK 行为分叉 | Player 必须只调用正式 Playback SDK，并做结果 hash 对照测试 |
| Studio 形成第二套 Runtime | Viewport 必须使用同一 PlaybackSession 和编译路径 |

## 17. 已接受方向与分阶段确认项

以下方向已经接受：

```text
Playback SDK + 独立 Player + 独立 Studio
第一版使用 C++20 门面和 static/shared CMake package；1E 只交付 Playback Core preview，稳定 C ABI 延后到阶段 12
第一版宿主渲染以 FrameSnapshot/RenderPacket 为主
ProjectConfig 继续服务 Player/Studio，SDK 同时接受 typed/memory source
新增阶段 1E，并覆盖原 1D 计划中的“不创建阶段 1E”结论
cuexis_judgement 是 SDK 必选交付模块，但无 InputEvent 时保持休眠
ReplayData 使用版本化 Cuexis 自有格式身份，具体 Schema 在阶段 11 冻结
阶段 1D 使用 ChartClock/HostClock/CuexisAudio 三种不可变模式和 Prepared Playback 内容边界
cuexis_playback 可以依赖 cuexis_audio，但不得依赖 cuexis_audio_sdl/SDL
1D 交付 Cuexis::Audio/Cuexis::AudioSDL components，并将 preview API 提升到 0.2.0
ADR 0033 的同工具链 C++ shared preview 已以 0.3.0 实现，稳定 C ABI 仍在阶段 12
```

以下细节仍须在对应阶段编码前确认：

```text
第一版真实宿主适配目标；确认前只使用通用 external C++ consumer
PlaybackSession 在 1D 已冻结类型之外的最终 C++ 函数集合和错误返回细节
FrameSnapshot/RenderPacket 的内存布局与宿主上传策略
C ABI 的 handle、字符串、数组、allocator 和兼容性细节
ReplayData 的 format ID、Schema、迁移、大小预算和完整性校验
InputProfile、CalibrationProfile 与 JudgementConfigSnapshot 的具体格式
```

## 18. SDK 转型完成标准

Cuexis 只有满足以下条件后，才能声明完整 Playback SDK v1；阶段 1E 只能声明可安装的 Playback Core C++ preview：

```text
仓库外工程可通过正式包配置消费 Cuexis Playback
不构建或初始化 SDL/OpenGL 时可以完成谱面加载、更新、Seek 和帧提取
公共 SDK 接口不暴露内部 ECS、后端或第三方实现类型
宿主可以提供内容、时间和诊断，并明确选择音频/渲染适配方式
宿主可以通过 InputEvent 驱动判定，在播放结束后获取确定性的判定结果快照
Player 和 Studio 使用同一 SDK 路径，不存在应用私有 Runtime 分支
多 Session、失败回滚、Reload、资源释放和线程所有权有完整测试
判定、计分和统计对相同输入产生确定一致的累积结果
按键采样记录完整可靠，回放结果与实时播放确定一致，ReplayData 可序列化往返
版本、兼容性、许可证、部署文件和最小集成示例齐全
阶段 12 的稳定 C ABI、支持矩阵与正式宿主 adapter 门禁通过
Cuexis 核心范围不包含完成通用游戏所需的场景、玩法和平台外壳系统
```

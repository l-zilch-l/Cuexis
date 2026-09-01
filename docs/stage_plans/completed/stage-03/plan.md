# 阶段 3：可移植表现前端与渲染适配实施计划

状态：3A-3G 已完成并关闭；阶段 3 已于 2026-08-08 最终验收完成

决策依据：[ADR 0037](../../../adr/0037-stage-3-portable-presentation-contracts.md)。总体阶段边界见
[SDK 改造与阶段路线调整方案](../../historical/sdk-transition/plan.md)。

阶段 2 的 Windows/MSVC、MinGW、GPU 和 external consumer 本地门禁，以及 Stage 3 3G 的本地
Windows/MSVC、WSL GCC/Clang、sanitizer、package consumer 验收均已完成。Stage 3 commit
`b71ef23b2f258d88e274a9b4b13665ef10a39845` 的 hosted Linux Quality run 已全部通过，Stage 2
遗留跨平台前置与 Stage 3 全阶段门禁均已关闭。

## 0. 当前启动状态

项目所有者已于 2026-08-07 接受
[Portable Presentation Profile v1](../../../formats/PORTABLE_PRESENTATION.md) 的 S3A-01..S3A-12 精确合同。
3A、3B、3C、3D、3E 已关闭；3F 已于 2026-08-08 完成并关闭。项目所有者随后授权进入
3G；Windows/MSVC、WSL Linux 和 hosted Linux 最终矩阵均已完成。Stage 3 已于 2026-08-08
最终关闭。

当前代码事实：

- `PlaybackSession::extractFrame()` 输出拥有型 `FrameSnapshot`；`ObjectSnapshot` 已包含 Mesh 与
  Material portable ref，并保留对象 ID、World Matrix、Visibility、Material asset ID、opacity 和
  tint。ref 拥有 AssetId、resource type 和 SHA-256 semantic identity。
- `PreparedPlayback` 与活动 `PlaybackSession` 已公开 owning candidate/active manifest 和按 ref 获取
  owning immutable portable resource 的接口；candidate token、owner-thread、stale 与 rollback 合同已
  实现。
- `cuexis_assets` 继续只管理内部有界 opaque blob；Playback presentation 前端在 prepare 中把
  `CXPRES01` v1 严格解析为 Portable Mesh/Texture2D/Unlit Material。`ResourceManager` 的 Handle、
  Lease、Scope、generation 和 provider revision 仍是内部合同。
- `cuexis_runtime` 当前把 Renderable 解析成内部 Mesh/Material Handle，并用 `ResourceScope`
  保持生命周期；此路径不得成为宿主 API。
- `cuexis_render` 仍只有内部 DebugLine `RenderScene`/`RenderBackend`，不是安装后的 SDK 表现合同；
  `cuexis_render_opengl` 已增加独立 Stage 3 presentation adapter，真实上传和绘制 Mesh、Texture2D
  与 Unlit Material，同时保留 Debug Pass。
- Player 已只从 `FrameSnapshot` 和 Playback candidate/acquisition 派生 OpenGL presentation cache，
  不再解释应用私有资源字节；真实 Mesh/Texture/Material 绘制已在 3E 完成。
- `tests/presentation/` 已提供只依赖安装公共 Playback/Core 头的无 GPU Validation Sink；它完成
  candidate preflight、owning resource validation、no-fail activation 和规范化 frame summary，且不
  安装为 SDK component。
- 基础安装包当前只导出 Core、Audio、Content 和 Playback；OpenGL adapter 没有安装 component。
  其现有 public header 直接引用 `platform_sdl` 和内部 `render_backend.hpp`，target 也固定为 STATIC，
  因此不能只增加一条 `install(TARGETS)` 就声明为受支持组件。
- `tests/external/playback_consumer.cpp` 已作为 Playback-only 的 add_subdirectory/find_package 宿主，
  只链接 `Cuexis::Playback`，从独立 staging fixture 验证 manifest、owning acquisition、preflight、
  update/extract、FrameDigest v3 和 unload 后资源所有权。
- `stage1b_project` 的 Mesh/Material/Texture 文件只是 opaque 生命周期 fixture，不是合法 Stage 3
  portable payload，后续不得原地改写它们冒充新格式。

3A 允许的工作只有：阅读代码、更新 ADR/规范、编写公共 API sketch、冻结格式/预算/错误码和
package 拓扑。3A 期间禁止修改 public header、C++ source、CMake target、Schema、fixture 或测试。
3A 的全部决策经用户接受后，才能进入 3B。

Stage 2 hosted Linux 是 Stage 3 实现之外的跨平台发布门禁。该门禁最终由 commit
`b71ef23b2f258d88e274a9b4b13665ef10a39845` 的 Linux Quality
[run 31270268057](https://github.com/l-zilch-l/Cuexis/actions/runs/31270268057) 关闭；没有使用
Windows 或 WSL 本地结果替代 hosted 证据。

## 1. 目标与产品边界

阶段 3 的目标不是建设通用渲染引擎，而是交付以下最小闭环：

```text
PlaybackSession
-> owning FrameSnapshot（唯一权威 Runtime 输出）
-> portable Mesh / Texture2D / Unlit Material 获取边界
-> 无 GPU Validation Sink
-> 内建 OpenGL adapter 或宿主 adapter
```

完成后，仓库外宿主应能在不访问 World、EnTT、ResourceManager、SDL 或 OpenGL 类型的前提下：

- 提前检查候选播放内容的 Portable Presentation Profile v1 要求。
- 获取拥有型 portable resource，建立自己的 CPU/GPU cache。
- 每帧只消费 `FrameSnapshot` 并确定生成 Opaque/Transparent 绘制顺序。
- 在 load/reload 失败时保留上一有效 Playback 与 adapter 状态。
- 选择无 GPU 验证路径，或选择 Cuexis 内建 OpenGL adapter。

## 2. 明确非目标

```text
公共 RenderPacket 作为第二套 Playback 输出
通用 PipelineDesc、BufferDesc、Descriptor/BindingSet 或 CommandList API
可编程 Shader、Reflection、Variant、ImporterProfile 或 ShaderTargetProfile
通用 Material 参数 Schema、完整 RenderState 或 Shader Graph
Light、Shadow、Particle、UI、RenderGraph、FrameGraph 或多 Pass 后处理
宿主相机覆盖、StudioPreviewOverride 或 HostOverride
Vulkan、Metal、Direct3D、Unity、Unreal 或跨 Context 纹理共享
资源流式加载、异步 Provider、文件监听或通用热重载系统
项目级 RenderConfig、UserPreferences、DeviceProfile 或新的持久化字段
稳定 C ABI 或跨工具链长期二进制兼容承诺
```

这些能力分别由阶段 4、5、6、8、9、10 和 12 在真实消费者出现后处理。

## 3. 已接受的公共合同

### 3.1 FrameSnapshot 是唯一权威帧

- 延续 ADR 0030：成功提取的 `FrameSnapshot` 不借用 Session、World、Registry、RenderScene
  或 adapter 存储。
- Stage 3 扩展 `ObjectSnapshot` 的 Mesh 和 portable resource 身份，但不改变 Snapshot 的拥有型
  基本语义。
- adapter 可以生成内部命令包或排序列表；`RenderPacket` 若作为内部术语保留，只在一次受控
  adapter 调用中有效，不进入 Playback 公共 API、package consumer 或 FrameDigest。
- Player、Validation Sink、OpenGL adapter 和 external host 均从同一 Snapshot 与 resource
  manifest 派生，不允许应用私有 Runtime 提取路径。

### 3.2 Portable Presentation Profile v1

Stage 3 固定以下语义范围；3A 负责冻结精确 C++ 布局和数值预算。

| 资源 | v1 必需语义 | 明确不包含 |
| --- | --- | --- |
| Mesh | Triangle List、finite float position、可选 UV0、显式 index、确定 winding | 骨骼、Morph、邻接、通用 vertex layout、任意 topology |
| Texture2D | 2D、单层、RGBA8、sRGB/linear、mip 0、固定 linear + clamp | 压缩纹理、cube、array、3D、自定义 Sampler、运行时转码 |
| Unlit Material | base color、可选 base-color texture、Opaque/Blend、double-sided | Shader、反射、任意参数、Mask、PBR、Light、完整 RenderState |

对象的最终颜色和 alpha 按以下职责组合：

```text
effectiveColor.rgb = material.baseColor.rgb * object.materialTint
effectiveAlpha = material.baseColor.a * object.materialOpacity
```

`material.alphaMode == Blend` 或 `effectiveAlpha < 1` 的对象进入 Transparent；其余进入 Opaque。
精确浮点边界、premultiplied/straight alpha、depth test/write 和 cull 规则在 3A 冻结并由两个
consumer 共同验证。v1 不支持 Alpha Mask，不能把它隐式映射为 Opaque 或 Blend。

### 3.3 资源 manifest、引用和拥有型获取

公共职责模型如下，类型名仅为 3A 设计占位：

```text
PreparedPlayback
  -> candidate PresentationResourceManifest
  -> validate/acquire candidate portable resources
  -> prepare host/OpenGL cache
  -> commit Playback + activate adapter cache

FrameSnapshot::ObjectSnapshot
  -> typed mesh reference
  -> typed material reference

PortableMaterial
  -> optional typed Texture2D reference
```

资源引用必须至少表达：

- 资源类型和规范化 AssetId。
- 能区分成功 reload 后内容变化的 portable 语义内容身份。
- FrameDigest 和 adapter cache 可使用、且跨等价 Provider 不变的规范化语义身份。

公共 resource ref 和 `FrameSnapshot` 不包含 provider revision、manager token、slot
index/generation 或 source epoch。实现若需要这些字段，只能在 Playback/adapter 候选状态内部
用于 owner/stale 检查；公共 cache key 固定由资源类型、AssetId 和语义内容身份构成。

资源获取合同：

- v1 为同步、owner-thread、无回调、无隐式并发。
- 返回值拥有全部公开数组和字符串；不返回内部 blob 的 span、指针或 ResourceLease。
- 读取、解析、依赖闭包、类型、数值、预算或 capability 失败时返回 `core::Result`/Diagnostics。
- 候选 manifest 在 `PreparedPlayback` commit 前可验证；失败不得发布半个 Playback 或半个 cache。
- 成功取得的 resource 在 Session update/reload/unload/销毁后仍有效。
- source 退役后，Session 不保证重新解析此前未取得的旧 resource；需要旧帧的宿主必须保留其
  owning resource/cache。
- adapter 只消费 owning portable resource，不读取 Provider 字节、不解析 asset-index、不访问
  opaque blob。
- Stage 3 Renderable 的 Mesh/Material/Texture2D 必须严格成功。现有 Stage 1B built-in fallback
  blob 不属于 Portable v1；候选若解析到 fallback，在 commit 前失败。通用 ResourceManager 的
  Fallback API 和历史回归保持不变。

候选事务顺序固定为：

```text
PlaybackSession::prepareLoad/prepareReload
-> PreparedPlayback candidate manifest + owning acquisition
-> Validation/OpenGL/host adapter prepare candidate cache（可失败，不改活动状态）
-> PlaybackSession::commit（仍可能因 stale/owner/lifecycle 失败）
-> adapter activate candidate cache（只允许不可失败的 move/swap）
```

若 adapter prepare 或 Playback commit 失败，丢弃全部候选并保留旧 Playback/cache。不得先激活
adapter 再调用 Playback commit，也不得通过 callback 让 `PlaybackSession` 在操作期间重入。

### 3.4 Camera、Viewport 与 adapter 坐标转换

- 宿主仍通过 `FrameViewport` 提供 width/height，不增加 camera override。
- Chart/Behavior 相机是唯一权威相机；Stage 3 不修改 ADR 0028 的活动相机选择规则。
- Snapshot 保持右手系、米制、列向量/列主序的 view/projection 结果。
- adapter 独自负责 clip-space、Y 方向、depth range 和 API-specific matrix conversion。
- adapter 转换不得改变 Snapshot、FrameDigest、Chart、Behavior 或 World。
- presentation-only override 延期到阶段 4/7 的真实 Host/Studio 消费者和新 ADR。

### 3.5 Pass、排序和 capability

固定 Pass 只有：

```text
Opaque       Portable v1 必需
Transparent  Portable v1 必需
Debug        adapter/application 可选，不属于项目内容
```

排序合同：

- Validation Sink 使用稳定 Object ID 生成 Opaque 顺序。
- OpenGL/host adapter 可以为批处理重排 Opaque，但不得改变深度和混合可观察结果；测试模式必须
  能输出规范化顺序用于 parity。
- Transparent 按活动相机空间深度从远到近；深度相同按稳定 Object ID 排序。
- 3A 必须冻结深度取对象 origin 还是显式 bounds center、无活动相机时的失败/回退、非有限值拒绝
  和排序键比较规则；在此之前不得实现透明排序生产代码。
- Debug Pass 只能派生坐标轴、包围信息或验证标记，不进入 Chart、portable resource 或
  FrameDigest。

支持 Portable v1 的 adapter 必须支持 Opaque 和 Transparent。缺少任一必需能力时，候选内容
在提交前失败；Stage 3 不建立“跳过对象仍继续播放”的项目级静默降级。

### 3.6 Typed request 与 effective settings

Stage 3 可以增加以下进程内职责，但最终类型名由 3A 冻结：

```text
PresentationCapabilities  adapter 的事实能力
PresentationRequest       当前创建/诊断请求
EffectivePresentationSettings 实际启用的 Portable profile 与 Debug 状态
```

它们不持久化，不进入 Chart/ProjectConfig/UserPreferences。viewport 是逐帧输入；backend、窗口、
swap interval、MSAA 和 OpenGL context 属性由应用/adapter config 拥有。Stage 6/9 才组合应用级
ResolvedAppConfig 和 DeviceProfile。

## 4. 模块与依赖方向

目标依赖方向：

```text
host / player
  -> cuexis_playback
  -> optional cuexis_render_opengl adapter

cuexis_playback
  -> internal assets/runtime/render-front-end responsibilities
  -X-> SDL / OpenGL / GLAD / host engine SDK

validation sink
  -> Playback public presentation contract
  -X-> SDL / OpenGL / window / GPU

cuexis_render_opengl
  -> Playback presentation contract + internal render helper
  -> platform/OpenGL implementation dependencies
```

约束：

- `cuexis_render` 继续是内部实现 target，不把现有 `RenderScene`/`RenderBackend` 头直接声明为
  Stage 3 宿主 API。
- Playback 安装头不得出现 SDL、OpenGL、GLAD、EnTT、GLM、JSON DOM、ResourceManager、
  ResourceHandle 或实现日志类型。
- adapter-disabled preset 必须配置、构建、测试并安装 Playback，不查找 OpenGL/SDL 图形依赖。
- OpenGL adapter 是否安装为独立 component 由 3A 的 package topology 决策冻结；基础 Playback
  package 永远不传递图形依赖。
- Validation Sink 默认是 `tests/`/external-consumer 使用的测试支持实现，不导出、不安装，也不
  成为 Playback 传递依赖。若 3A 决定安装它，必须作为显式独立 component 重新评审。
- Stage 3 沿用现有 `engine/playback`、`engine/assets`、`engine/runtime`、`engine/render_opengl` 和
  `app/player` 边界；不得仅为目录命名新建空 `sdk/`、`adapters/` 或通用 presentation engine。
- 新增 target 必须注册到 `CUEXIS_ACTIVE_TARGETS` 和依赖 allowlist。

### 4.1 当前代码落点与预计修改面

| 职责 | 当前落点 | 进入对应批次后的允许方向 | 禁止方向 |
| --- | --- | --- | --- |
| 公共 Snapshot/resource API | `engine/playback/include/cuexis/playback/` | 3A 冻结后由 3C 增加纯 ASCII public types | 把 Assets Handle、span、GLM 或 backend type 放入公共头 |
| 候选 manifest/acquisition | `PreparedPlayback` private state 与 Playback facade | 3B 在 candidate 生命周期内提供只读 manifest 和 owning value | 公开 ResourceManager、Provider、Lease 或 blob |
| payload 读取与规范化 | `cuexis_assets` + Playback private implementation | 3B 复用 AssetDatabase/ResourceManager 预算并产出 portable value | Player/OpenGL 各自解析源字节 |
| Runtime Renderable | `cuexis_runtime`、`RenderableComponent`、ResourceScope | 保留内部 Handle；3C 提取为公共 semantic ref | 宿主访问 World/EnTT/Handle |
| Frame extraction/digest | `playback_session.cpp`、`frame_digest.cpp` | 3C 扩展 Snapshot 并新增 digest version | 修改旧 digest version 1/2 定义 |
| Validation Sink | `tests/` 与 `tests/external/` | 3D 从安装公共头独立实现 normalized oracle | 依赖内部 Assets/Render header 或 GPU |
| OpenGL adapter | `engine/render_opengl/` | 3E 拥有 GPU cache、固定 Unlit pipeline 和 draw | Playback/Player 直接调用 GL 或解析 payload |
| Player composition | `app/player/` | 3E 只编排 candidate prepare/commit、adapter activation 和 submit | 保留应用私有资源解释或第二条 Runtime 提取路径 |
| Package consumer | root CMake、`cmake/`、`tests/external/` | 3F 扩展 static/shared/add_subdirectory/find_package 门禁 | 基础 Playback 隐式发现 SDL/OpenGL |
| Reference content | 新建 Stage 3 专用 fixture | 3B 创建 `stage3_project` portable fixture | 改写 Stage 1B opaque fixture 破坏历史回归 |

## 5. 实施顺序

顺序固定为：

```text
3A 合同、格式和 package 决策门禁
-> 3B Portable resource 解析与候选 manifest
-> 3C FrameSnapshot 与共同表现提取
-> 3D 无 GPU Validation Sink
-> 3E OpenGL adapter 与 Player 真实绘制
-> 3F 安装包与 external consumer
-> 3G 全链路、性能和跨平台验收
```

3A 未关闭前不得修改公共 C++ 接口。3B/3C 必须先让 headless 数据闭环可验证，再进入 OpenGL
生产代码。3D 必须先通过，3E 才能作为第二个 consumer 证明合同不是只适配单一后端。

### 3A：合同、格式与版本门禁

状态：已关闭。精确合同已写入 `docs/formats/PORTABLE_PRESENTATION.md` 并于 2026-08-07 接受。

任务：

- 为 FrameSnapshot resource ref、portable Mesh/Texture2D/Unlit Material、manifest/acquisition、
  capability/request/effective settings 编写公共 API sketch。
- 冻结 Mesh winding、index width、UV 缺省、坐标有限性、RGBA8 channel/order、texture origin、
  color-space、alpha blend、depth/cull 和 double-sided 语义。
- 冻结 Transparent 深度键、平局顺序和无活动相机合同。
- 冻结每资源与每 Session 的 vertex/index/texture dimension/decoded bytes/resource count/diagnostic
  预算；默认值必须与现有 AssetDatabase/ResourceManager 上限一致或给出显式收窄理由。
- 决定 versioned 文件 payload、typed-memory payload 或两者的最小输入格式；每种格式都必须有
  format/version、字节序、大小和未知字段/版本拒绝规则。
- 冻结内容身份、runtime revision、candidate source epoch 与 FrameDigest 的区别；相同内容通过
  Filesystem/Memory/Host Provider 必须得到相同语义 identity。
- 冻结 `PreparedPlayback` 候选 manifest 的读取时机、owner/reentry、commit/discard 和失败原子性。
- 决定 OpenGL adapter 是否导出为可选 CMake component、component 名称、SDL surface 所有权、
  static/shared 闭包和许可证依赖。
- 若导出 OpenGL component，必须决定是重设计其 public header 只依赖受支持组件，还是把所需
  Platform/Render 边界一并正式组件化；不得安装当前依赖内部 header 的拓扑后宣称闭包完整。
- 评审 `CUEXIS_SDK_API_VERSION`；公共 Snapshot/API 变化不得以 patch 版本静默发布。

3A 必须逐项关闭以下决策，不得以“实现时再看”进入 3B：

| ID | 必须冻结的内容 | 当前提案 |
| --- | --- | --- |
| S3A-01 | public header/type 分组、方法签名 sketch、Result/Diagnostics 返回和 owner/reentry 规则 | `presentation.hpp` + Playback facade；已关闭 |
| S3A-02 | resource ref 仅包含 type + AssetId + semantic content identity；identity 算法、长度和碰撞处理 | 32-byte SHA-256 canonical semantic identity；已关闭 |
| S3A-03 | Mesh/Texture2D/Unlit Material payload format ID、version、字节序、unknown version 和截断规则 | `CXPRES01` little-endian binary v1；已关闭 |
| S3A-04 | Mesh winding、index width、position/UV layout、finite、顶点/index 数量和 bounds/sort origin | uint32 CCW triangle list、optional UV0、derived AABB center；已关闭 |
| S3A-05 | RGBA8 channel order、texture origin、sRGB/linear、dimension、mip 0、filter 和 address mode | top-left RGBA8、Linear/sRGB、linear+clamp、mip0；已关闭 |
| S3A-06 | Unlit base color/texture、straight/premultiplied alpha、Opaque/Blend、depth/cull/double-sided | linear Unlit、straight alpha、fixed depth/blend/cull；已关闭 |
| S3A-07 | Material payload texture ref 与 AssetIndex dependencies 的唯一权威关系、闭包顺序和环处理 | payload ref 与唯一 index dependency 必须精确一致；已关闭 |
| S3A-08 | manifest entry/order、per-resource/per-session 预算、owning acquisition 和旧 source 退役规则 | AssetId 排序、64 MiB/resource、512 MiB/session；已关闭 |
| S3A-09 | PreparedPlayback candidate token、adapter prepare、Playback commit、no-fail activation 和 discard | opaque token + pre-commit prepare + noexcept activate；已关闭 |
| S3A-10 | Transparent 深度 key/tie、无活动相机、非有限矩阵、空帧和全不可见帧规则 | AABB center、1/4096 m key、ObjectId tie、empty success；已关闭 |
| S3A-11 | PresentationCapabilities/Request/EffectiveSettings 与现有 PlaybackCapabilitySet 的职责分离 | adapter facts separate from Chart/Runtime capabilities；已关闭 |
| S3A-12 | 稳定 error/diagnostic code 表、SDK API 版本、FrameDigest 新版本和 package component topology | SDK 0.5.0、Digest v3、无 OpenGL install component；已关闭 |

3A 的规范性交付物固定为：

```text
docs/formats/PORTABLE_PRESENTATION.md
  resource format/field/normalization tables
  public API sketch and lifetime examples
  budget and diagnostic code tables
  sorting, capability and transaction protocol

docs/adr/0037-stage-3-portable-presentation-contracts.md
  only when 3A decisions refine or replace an accepted architectural statement

docs/stage_plans/stage_3_implementation_plan.md
  mark S3A-01..S3A-12 closed and record the accepted package/version decision
```

API sketch 是文档代码块，不是 public header。3A 完成时应先把上述文档交给用户确认；确认前停止，
不自动进入 3B。

本轮没有修改 ADR 0037，因为提案没有替换其已接受的架构方向；精确值只有在项目所有者接受后才
成为规范。接受后应把本文 S3A-01..S3A-12 标为已关闭，并将 3B 标为进行中。

验收目标：

- ADR、API sketch、格式表、预算表、错误 code 表和包拓扑不存在未决的公共语义。
- `FrameSnapshot` 与内部 adapter command 的所有权和有效期不再使用斜杠式模糊表述。
- 阶段 3/5 分工明确：Stage 3 固定 Unlit，Stage 5 扩展 versioned Material/Shader。
- 所有生产代码任务都能引用已接受的字段、范围、生命周期和失败合同。

### 3B：Portable resource 与候选 manifest

状态：已于 2026-08-07 完成并关闭。

目标：把 opaque Mesh/Material/Texture blob 转换成经验证、拥有型、后端无关的 portable resource，
并在 Playback commit 前发布候选 manifest。

任务：

- 实现 3A 冻结的 Mesh、Texture2D、Unlit Material 读取/typed 输入与规范化。
- 对版本、类型、字节序、数组范围、index、有限性、颜色空间、引用和总预算执行严格校验。
- Material 依赖 Texture2D；manifest 按稳定 AssetId 排序并提供有界依赖闭包。
- 计算 provider 无关的规范化内容 identity；若实现还需要 runtime revision/epoch，只保存在
  Playback/adapter 候选状态内部。
- 为 candidate PreparedPlayback 暴露只读 manifest 和 owning acquisition。
- 失败 candidate 不改变当前 ResourceManager/Session/adapter 可观察状态。
- Stage 3 presentation 闭包使用严格必需资源；不把 Stage 1B fallback payload 转换成 portable 值。
- 新建独立 `assets/projects/stage3_project` 与最小 memory/host provider 等价 fixture；保留
  `stage1b_project` 原始 opaque bytes 和历史测试。
- 保留阶段 1B ResourceHandle/Lease/Scope、Provider path security 和 reload rollback 内部回归。

验收目标：

- Filesystem/Memory/Host Provider 的等价资源产生相同 portable 值、manifest 顺序和内容 identity。
- 非法 index、NaN/Inf、截断 payload、超限纹理、错误类型和依赖环在 commit 前稳定失败。
- acquisition 返回值在 reload/unload/Session 销毁后可读。
- 失败 reload 不改变旧 manifest、旧 FrameSnapshot 生成结果或已激活 cache。
- 资源解析/规范化有独立 allocation 与内存预算；不在每帧 update 中重复解析。

关闭证据（2026-08-07，本地 Windows/MSVC）：

- Windows/MSVC static Debug 全量 260 项通过；Windows symlink containment 测试按既有平台条件
  跳过。static/shared Playback、allocation 与 Player diagnostics 组，以及 add_subdirectory/
  find_package external consumer 已通过。
- Windows/MSVC static Release 完整构建通过；3C/architecture 筛选 12 项和五个 external/package
  consumer 通过。
- `cuexis_format_check`、直接 `clang-tidy`、公共头 ASCII 扫描和 `git diff --check` 已通过。
  `clangd --check` 无源码诊断；仅报告其既有 tweak 重构动作自检失败。
- 以上均为本地 Windows 证据，不关闭 Stage 2 hosted Linux GCC/Clang、sanitizer 与 package
  consumer run URL 门禁。

### 3C：FrameSnapshot 与共同表现提取

状态：已于 2026-08-07 关闭。

目标：让 Snapshot 完整引用真实绘制所需资源，并建立 Validation/OpenGL 共用的确定表现提取规则。

任务：

- 按 3A sketch 扩展 `ObjectSnapshot` 的 Mesh/Material portable ref 和必要的排序输入。
- 保持现有 Transform、Visibility、Material ID/Opacity/Tint、Camera、clear color 和 viewport 语义。
- prepare 阶段为所有可能的 Stage 2 Material Step 候选预留 Snapshot/resource ref 容量。
- 从 Snapshot + portable resources 确定 effective color/alpha、Pass、cull 和规范化 sort record。
- 禁止共同提取层访问 OpenGL、SDL、World、EnTT 或 host-engine 类型。
- 升级 FrameDigest 算法版本；hash 可移植语义和规范化内容 identity，排除 pointer、handle、
  source epoch、provider revision、GPU ID 和 adapter debug 数据。
- 保留旧 digest 算法定义和 golden，不在旧版本中静默加入字段。

验收目标：

- 同一候选内容在 Player、headless、static/shared consumer 中得到相同 Snapshot 与 digest。
- Material Step、opacity/tint Behavior、Visibility、Seek、Stop 和 Reload 正确改变最终 Pass/值。
- 已返回旧 Snapshot 在 update/reload/unload/Session 销毁后仍可读。
- warmed-up update/extract 不增加动态分配；resource acquisition 不混入每帧热路径。
- 公共头 ASCII、依赖 allowlist、导出符号和 shared boundary 门禁通过。

关闭结果：

- `ObjectSnapshot` 已增加 optional owning Mesh/Material `PresentationResourceRef`；prepare 为初始
  Material 和全部 Material Step 候选建立 ref 表并预留 AssetId 容量。
- 共同 normalizer 只消费 Snapshot、manifest 和 owning portable resources，严格校验 ref/type/
  identity/manifest membership 与 Material -> Texture2D 依赖，生成 effective RGB/alpha、固定
  raster state、Opaque/Transparent pass、AABB-center depth 和规范化排序记录。
- Opaque 按 Object ID raw ASCII 升序；Transparent 按 1/4096 m depth key 降序、Object ID 升序；
  全不可见/空帧无需 active Camera 并产生空记录。
- FrameDigest 默认算法已升级为 v3，新增 Mesh/Material ref presence、type、AssetId 和 32-byte
  identity；v1/v2 实现与 golden 保持不变。
- Stage 3 fixture 覆盖 Material Step、opacity/tint、Visibility、Stop、Seek、Reload 与生命周期；
  warmed update/extract/normalize 保持零分配。
- 3C 关闭时 Validation Sink 尚属于 3D；该批次现已单独完成。OpenGL adapter 和 Player 真实绘制
  已在 3E 作为第二个独立 consumer 完成。

### 3D：无 GPU Validation Sink

状态：已于 2026-08-08 完成并关闭。

目标：建立 Stage 3 的第一个独立 consumer，在无窗口、无 SDL、无 OpenGL 环境验证完整表现合同。

任务：

- 消费 candidate manifest、owning resources 和 FrameSnapshot，不读取内部 module header。
- 校验资源存在、类型、依赖、内容 identity、Pass capability、effective alpha 和排序输入。
- 输出有界、规范化的 command summary/digest，覆盖 Opaque/Transparent 与 optional Debug 状态。
- 对缺失 Portable v1 capability、非法引用、退役 epoch、预算溢出和 unsupported profile 返回稳定
  Diagnostics。
- 建立 adapter-disabled add_subdirectory/find_package consumer，证明安装包公共合同足够实现宿主
  adapter。

验收目标：

- Validation Sink 在 headless preset 下配置、构建和运行，不查找或链接 SDL/OpenGL/GLAD。
- 输入数组、asset index 和 provider 顺序不改变规范化结果。
- 同一 Snapshot 重复验证无分配或在 3A 冻结的复用预算内。
- 缺少 Opaque/Transparent 必需能力时在 commit 前失败；关闭 Debug 只改变 effective setting。
- 规范化 summary 成为 OpenGL adapter parity 的独立 oracle，而不是复制 OpenGL 实现。

关闭结果：

- `PreparedPlayback::validatePresentation()` 已作为 public Playback API 实现 version/profile、Portable
  v1 七项必需语义能力和 resource/decoded/texture/mesh limits 的 preflight。Debug 是唯一可降级
  capability；unsupported Debug 返回 warning 和 disabled effective setting。
- `tests/presentation/validation_sink.hpp/.cpp` 是不安装的 public-only consumer。它只包含 Cuexis
  Core/Playback 公共头，独立重新计算 SHA-256 semantic identity，并校验 manifest/resource type、
  ref、identity、Material -> Texture2D dependency、count/byte/dimension budget 和规范顺序。
- Validation candidate 在 Playback commit 前获取 owning resources；commit 成功后通过 `noexcept`
  activation 切换。stale Playback candidate 失败不会替换当前 Sink token 或发布半状态。
- frame validation 输出 versioned、bounded `ValidationSummary`，包含 viewport/camera/clear、effective
  color/alpha、depth/cull/blend、Opaque/Transparent canonical order 和 optional Debug setting；summary
  使用 domain-separated FNV-1a 64-bit digest。非法帧先清空 destination，失败时不留下 partial commands。
- `cuexis_presentation_validation_tests` 是独立 Catch2/CTest target，已注册 active target 与 dependency
  allowlist；`playback_allocation_tests` 证明 warmup 后连续 128 次 Validation Sink frame validation 为
  零动态分配。
- add_subdirectory 与 find_package external consumer 已从 Stage 3 fixture 执行
  prepare -> validation -> commit -> activate -> update/extract -> summary，并固定 ValidationSummary
  digest golden `18316288860163381829`；FrameDigest v3 external consumer golden 为
  `6442711505793857933`。find_package 路径只使用干净安装目录中的公共头和库。

关闭证据（2026-08-08，本地 Windows/MSVC）：

- static Debug 全量 CTest 270/270 通过；Windows symlink containment 测试按既有平台条件跳过。
- shared Debug 全量 CTest 272/272 通过，包含 shared export/import、3D focused tests 与
  add_subdirectory/find_package external consumer。
- `headless-debug --fresh` 237/237 通过；Windows symlink containment 测试按既有平台条件跳过，且
  配置不启用 SDL/OpenGL adapter。
- static Release fresh/clean build 与全量 CTest 270/270 通过，包含 3D/external/architecture、static
  add_subdirectory 与 find_package external consumer。
- `cuexis_format_check`、公共安装头 ASCII 扫描和 `git diff --check` 通过；直接 `clang-tidy` 检查
  `open_gl_presentation.cpp`、`player_app.cpp` 与 `math.cpp` 以 0 退出；`clangd --check` 对相同
  文件在关闭 tweak 自测后均为 0 errors。
- Stage 2 hosted Linux GCC/Clang、sanitizer 与 package consumer run URL 仍未关闭，因此 3D 关闭
  不等于 Stage 3 完成；该门禁仍必须在 3G 前关闭。

### 3E：OpenGL adapter 与 Player 真实绘制

状态：已于 2026-08-08 完成并关闭。

目标：把现有 Debug Line-only 路径升级为 Stage 3 的第二个 consumer，并保持图形 API 隔离。

任务：

- 用 candidate manifest/acquisition 事务准备 Mesh VBO/IBO、Texture2D 和固定 Unlit pipeline。
- GPU 对象只由 OpenGL adapter 创建、缓存和销毁；公共 cache key 使用 3A 冻结的资源类型、
  AssetId 和 semantic content identity。source epoch 只能存在于候选状态内部。
- upload/compile/capability 失败时丢弃 candidate GPU 状态，不切换上一有效 renderer cache。
- 实现 Opaque depth test/write、Transparent 标准 alpha blend 和冻结的 back-to-front 排序。
- 实现 double-sided/cull、texture origin/color-space 和 adapter clip-space 转换。
- Debug Pass 继续支持基础坐标轴/诊断，但不替代真实 Mesh 绘制。
- Player 删除应用私有资源解释逻辑，只组合 Playback、candidate adapter prepare/commit 和帧提交。
- 合法全不可见帧继续 clear/present；空 Opaque/Transparent command list 不是错误。

验收目标：

- 最小 reference project 在 Debug/Release GPU smoke 中真实绘制 Mesh、Texture 和 Unlit Material。
- Opaque/Transparent 规范化顺序与 Validation Sink 一致。
- 资源 reload 成功时只切换到完整 candidate cache；任一上传失败时旧帧仍可继续绘制。
- OpenGL 调用和 GL 类型只出现在 `engine/render_opengl/`；Playback/public headers 无图形依赖。
- GPU smoke 覆盖正常、全透明、全不可见、无活动对象、成功 reload 和失败 reload。
- 加入可复核像素/离屏结果或规范化 draw summary golden；只统计“三帧完成”不足以证明真实材质正确。

关闭结果：

- `cuexis_render_opengl` 已实现 move-only candidate manifest/acquisition、Mesh VAO/VBO/IBO、RGBA8
  linear/sRGB Texture2D、固定 GLSL 330 Unlit pipeline、Opaque/Transparent depth/blend/cull 状态、
  canonical draw order、Debug Pass、draw summary 和中心像素 probe。
- OpenGL adapter 的 prepare/commit/activate/discard 事务只发布完整 GPU cache；上传、shader、capability
  或 Playback reload 失败时保留旧 cache，退休 GPU 对象延迟到 context current 时销毁。
- Player 已只消费 `FrameSnapshot` 和 public Playback candidate/acquisition，并以
  `assets/projects/stage3_project` 执行六帧真实 presentation smoke：Opaque、textured Transparent、
  全不可见 clear、Playback reload 失败、adapter prepare 失败和成功原子切换。
- Validation Sink 与 OpenGL adapter 对同一 fixture 产生相同 summary golden
  `18316288860163381829`；FrameDigest v3 external consumer golden 为 `6442711505793857933`。
- `core::makePerspective()` 已修正 OpenGL projection 的 perspective divide/depth translation 位置，
  并以 near/far plane 映射到 canonical NDC `[-1,+1]` 的单元测试固定合同。
- Debug/Release GPU smoke 均记录 OpenGL 3.3.0、NVIDIA Corporation、GeForce RTX 4060 Laptop GPU；
  代表像素为 Opaque `{255,204,51,255}`、textured Transparent `{51,32,71,192}`，全不可见帧保持
  clear color `{14,16,18,255}`。

关闭证据（2026-08-08，本地 Windows/MSVC）：

- `headless-debug --fresh` 构建和 237/237 CTest、shared Debug 构建和 272/272 CTest、Release
  fresh/clean 构建和 270/270 CTest 均通过。
- `cuexis_format_check`、public installed header ASCII 扫描、`git diff --check`、直接
  `clang-tidy` 和 `clangd --check` 均通过；Windows symlink containment 测试按既有平台条件跳过。
- Debug/Release GPU smoke 均完成 6 帧并通过像素、summary、失败保留旧 cache 和成功 reload 断言。

Stage 2 hosted Linux GCC/Clang、sanitizer 与 package consumer run URL 仍是开放跨平台发布门禁；
3F 的关闭结果见下节，3G 本地结果见 3G 小节与完成报告。

### 3F：安装包与 external consumer

状态：已于 2026-08-08 完成并关闭。

目标：证明 Stage 3 不是 Player 私有功能，并保持 adapter-disabled Playback package 的最小闭包。

任务：

- 更新 static/shared add_subdirectory 与 find_package consumer，读取 manifest/resource/Snapshot。
- consumer 自己实现 Validation Sink 或最小 host adapter，不包含内部 Assets/Render headers。
- 按 3A 已接受的 package 拓扑保持 OpenGL component 不安装；验证基础包不发现 SDL/OpenGL，且
  显式请求未支持的 `OpenGL` component 会被拒绝。
- 扩展 public header ASCII、第三方泄漏、导出符号、import library 和 package version 检查。
- 验证旧最低版本 consumer 被 package compatibility 规则正确接受或拒绝。

验收目标：

- static/shared consumer 可从干净安装目录完成 load/prepare/resource validation/update/extract。
- adapter-disabled consumer 不需要 GPU、Window、SDL、OpenGL、GLAD 或内部开发头。
- Player、Validation Sink、add_subdirectory、static/shared find_package consumer 的 Snapshot/digest
  parity 通过。
- 安装的 component、generated version、licenses 和 THIRD_PARTY_NOTICES 与实际闭包一致。

关闭结果：

- 新增 `add_subdirectory_playback` 与 `find_package_playback` 两个最小宿主模式；二者共用只包含
  Core/Playback public header 的 consumer，从 consumer work dir 中的独立 `stage3_project` staging
  fixture 完成 candidate/active manifest、owning Mesh/Texture2D/Unlit Material acquisition、capability
  preflight、commit/update/extract、FrameDigest v3 parity 和 unload 后资源存活验证；`625 ms` 的
  digest golden 固定为 `8424169740673868033`。
- `find_package_playback` 只链接 `Cuexis::Playback`，不显式请求 Content/Audio。基础包继续不安装
  Platform、RenderOpenGL、内部 Runtime/World/Assets header 或 OpenGL component，也不发现 SDL3、
  OpenGL 或 GLAD；显式 `COMPONENTS OpenGL` 配置稳定失败。
- external package 门禁使用 clean staging，精确核对基础包的 EnTT、GLM、JSON Schema Validator、
  nlohmann-json 与 tl-expected notices；只有 AudioSDL 安装额外包含 SDL3 notice。`0.4` 与 `0.6`
  package 请求按当前 `0.5` same-minor preview 规则稳定拒绝。
- shared export 门禁覆盖 `presentationManifest`、`acquirePresentationResource` 和
  `validatePresentation`。Playback-only EXE 只直接导入 Playback/Core DLL；Playback DLL 只导入
  Audio/Content/Core，EXE 与 DLL 均不导入 SDL、GLAD 或内部 Runtime DLL。

关闭证据（2026-08-08，本地 Windows/MSVC）：

- standard Debug fresh configure/build 与 272/272 CTest、headless Debug fresh configure/build 与
  239/239 CTest、static Release 272/272 CTest、shared Debug 275/275 CTest 均通过；Windows symlink
  containment 测试按既有平台条件跳过。
- 七个 external/package consumer 在 static、headless 与 shared 路径通过；shared export surface、
  通用 shared consumer imports 和 Playback-only consumer imports 全部通过。
- `cuexis_format_check`、源码与 clean install public header ASCII 扫描、未跟踪文本尾随空白扫描和
  `git diff --check` 通过；Playback-only consumer 的直接 `clang-tidy` 与 `clangd --check` 均为
  0 errors。
- Stage 2 hosted Linux GCC/Clang、sanitizer 与 package consumer run URL 仍是 3G 前的开放门禁；
  3F 关闭不等于 Stage 3 完成，也不授权自动进入 3G。

### 3G：全链路、性能与跨平台验收

状态：已于 2026-08-08 完成并关闭。

任务：

- 执行 Debug/Release fresh configure、clean build、完整 CTest、format 和 architecture。
- 执行 static/shared、headless、adapter-disabled、Player-full 和外部 package consumer 矩阵。
- 在 Windows/MSVC 与 hosted Linux GCC/Clang 上验证 portable resource 和 package consumer。
- 运行 Linux sanitizer/clang-tidy/coverage workflow；只把真实工具输出作为证据。
- 执行 Debug/Release GPU smoke，并记录 GPU、driver、OpenGL version、draw summary 和像素/离屏证据。
- 测量最大合法 resource prepare、manifest/acquisition、warmed-up update/extract/validate、OpenGL
  cache hit submit 和 reload peak memory。
- 审查所有 warning、导出面、SDK API、FrameDigest、文档、示例和完成报告。
- 创建 `docs/stage_reports/stage_3_completion_report.md`，区分本地 GPU、hosted CPU/package 和未覆盖
  的驱动/平台风险。

验收目标：

- Stage 2 全部回归保持；Stage 2 hosted Linux 前置已有可引用 run URL。
- 两个独立 consumer（Validation Sink、OpenGL adapter）通过同一 Portable v1 fixture。
- 失败 load/reload/upload 不发布半状态，旧 Playback 与 renderer 可继续工作。
- warmed-up Playback update/extract 和 Validation 路径满足 3A 冻结的零分配/预算合同。
- Windows 与 Linux 的 Snapshot、portable identity、FrameDigest 和 normalized command summary 一致。
- GPU 结果验证真实 Mesh/Texture/alpha，而不只是窗口和三帧循环。
- 所有外部 package、架构、ASCII、license 和 adapter-disabled 门禁通过。

本地验收结果（2026-08-08）：

- Windows/MSVC standard Debug 与 Release 最终格式化后均通过 `272/272` CTest；3B-3F 已完成的
  static/shared、headless、adapter-disabled 和七种 external consumer 矩阵保持通过。Windows
  symlink containment 测试按既有平台条件跳过。
- Debug/Release 六帧 GPU smoke 在 NVIDIA GeForce RTX 4060 Laptop GPU、driver 596.36、OpenGL
  3.3.0 上通过真实 Mesh、Texture2D、alpha、empty frame、失败保留旧 cache 和成功 reload 断言。
- 最大合法 Texture2D performance probe 覆盖 prepare、manifest/acquisition、Validation candidate、
  warmed update/extract/validate、OpenGL cache submit 和 reload peak memory；墙钟结果只作为趋势证据。
- WSL Ubuntu 的 GCC headless Release `237/237`、GCC shared Release `238/238`、Clang shared Debug
  `238/238`、Clang ASan+UBSan `237/237`、Playback clang-tidy 和 GCC coverage `230/230` 通过；
  coverage 为 lines `76.3%`、functions `91.1%`、branches `40.5%`。
- `cuexis_format_check`、`git diff --check`、public installed header ASCII、architecture、export/import、
  component/version rejection 和 license 门禁通过。精确证据与残余风险见
  [阶段 3 验收报告](../../../stage_reports/stages/stage-03/completion.md)。
- commit `b71ef23b2f258d88e274a9b4b13665ef10a39845` 已推送到 `origin/codex/stage-3`。Hosted
  Linux Quality [run 31270268057](https://github.com/l-zilch-l/Cuexis/actions/runs/31270268057)
  的 GCC Release、GCC shared Release、Clang shared Debug、Clang ASan+UBSan、clang-tidy 和 GCC
  coverage 全部成功；Windows MSVC run `31270267999` 与 Windows MinGW run `31270268010` 也全部成功。

## 6. 测试矩阵

| 范围 | 必须覆盖 |
| --- | --- |
| Resource format | version/type、truncation、unknown、endianness、finite、index、dimension、byte budget |
| Provider parity | Filesystem/Memory/Host 等价内容的 manifest、portable value、identity 和诊断 |
| Dependency | Material->Texture2D、缺失、类型错误、环、重复、稳定排序、总预算 |
| Ownership | resource/Snapshot 在 update/reload/unload/Session 销毁后有效；旧 source 重新获取边界 |
| Transaction | initial load、successful reload、failed parse、failed capability、failed upload、discard |
| Frame | Mesh/Material ref、Visibility、Tint/Opacity、Camera、Viewport、clear color、empty frame |
| Pass | Opaque、Blend、effective alpha、all-transparent、all-invisible、stable tie |
| Camera | viewport、aspect、active/inactive、clip conversion、non-finite reject、无 host override |
| Validation | headless、adapter-disabled、normalized summary、capability/effective settings |
| OpenGL | upload/cache/release、context failure、shader failure、真实 draw、pixel/summary golden |
| Performance | prepare peak、decoded bytes、cache hit、warmed update/extract/validate、reload peak |
| Package | static/shared、add_subdirectory/find_package、clean staging、optional adapter component |
| Architecture | public include、target allowlist、exports/imports、无 SDL/OpenGL/EnTT/JSON 泄漏 |
| Cross-platform | MSVC、hosted GCC/Clang、sanitizer、endianness assumptions、digest parity |

## 7. 版本、Digest 与兼容性

- `FrameSnapshot`、resource acquisition 或 package component 的公共变化必须评审
  `CUEXIS_SDK_API_VERSION`；3B 首次加入 public acquisition 时已提升到 `0.5.0`。
- 新 FrameDigest 使用新的 algorithm version。旧 version 1/2 定义和 golden 保留，用于历史 fixture
  与兼容测试。
- 新 digest 必须包含影响 portable presentation 的 Object/Camera/resource semantic identity，排除
  adapter cache、source epoch、provider revision、GPU handle 和 Debug Pass。
- static/shared consumer 同时验证 package minimum version、generated header 和运行时版本查询。
- Stage 3 仍是 matching-toolchain C++ preview，不承诺稳定 C ABI。

## 8. 配置与持久化

Stage 3 不修改 Chart v3、ProjectConfig v1、UserPreferences、DeviceProfile、ResolvedAppConfig 或
ResolvedSessionConfig 格式。Portable resource payload 自身可以有版本化 format ID，但它属于
Asset 内容，不是应用配置。

Presentation capability/request/effective settings 是进程内 typed 值：

- capabilities 是 adapter 事实，不是用户覆盖层。
- request 只能请求 Stage 3 已实现字段，不能注入任意 key/value。
- effective settings 只报告实际结果，不回写 Project 或用户配置。
- backend-specific config 由 OpenGL adapter/应用拥有，不进入 Playback deterministic identity。

## 9. 安全、预算与线程

- 所有 resource bytes 按不可信输入处理；Provider 来源不同不改变版本、预算和语义校验。
- 每个数组、字符串、resource、依赖闭包、decoded byte、manifest 和 diagnostic 都有硬上限。
- 乘法和 offset 计算必须检查整数溢出；不得根据未验证 count 分配。
- public resource acquisition 与 Playback 操作使用 Session owner thread；不增加隐式线程安全。
- GPU object 创建、提交和销毁遵守 adapter/context owner thread。
- resource prepare 可以在后续阶段扩展 Worker，但 Stage 3 v1 不建立异步回调或取消 ABI。
- 异常不得跨公共模块边界；析构、discard 和失败 rollback 不抛出。

## 10. 文档交付

Stage 3 实现过程中至少维护：

```text
docs/adr/0037-stage-3-portable-presentation-contracts.md
docs/stage_plans/stage_3_implementation_plan.md
docs/PROJECT_GUIDE.md
docs/stage_plans/cuexis_sdk_transition_plan.md
docs/formats/PORTABLE_PRESENTATION.md（3A 创建并冻结格式、API sketch、预算和诊断）
BUILDING/package component/consumer 文档
stage_3_completion_report.md（仅在全部门禁后创建）
```

文档不得把 planned type、target、component 或测试写成已实现。公共头最终必须 pure ASCII；中文
设计说明只保留在 docs、src 或 tests 中。

## 11. 阶段完成定义

只有同时满足以下条件，阶段 3 才能标为完成：

1. ADR 0037 与 3A 精确合同全部落地，无遗留公共语义占位。
2. Portable Mesh/Texture2D/Unlit Material、candidate manifest 和 owning acquisition 已实现。
3. `FrameSnapshot` 是唯一公共帧，FrameDigest 新版本和全部生命周期测试通过。
4. Validation Sink 与 OpenGL adapter 两个独立 consumer 产生相同规范化结果。
5. Player 真实绘制资源，不再只用 Debug axes 代替 Renderable。
6. load/reload/resource/upload 失败保持 Playback 与 renderer 事务一致。
7. static/shared、adapter-disabled、external consumer、ASCII、architecture 和 license 门禁通过。
8. Windows 本地 GPU 证据和 hosted Linux GCC/Clang/sanitizer/package run URL 齐全。
9. Stage 3 完成报告准确区分已验证平台、GPU 驱动和残余风险。

关闭结果：以上九项全部满足，阶段 3 于 2026-08-08 最终完成。

## 12. 历史交接：Stage 3 review 清单

状态：completed historical handoff

本清单已用于启动 [260808 Stage 3 review](../../../stage_reports/stages/stage-03/2026-08-08-review.md)。它保留当时的
审查范围和操作顺序，不再代表当前下一步；该 review 的独立复审状态以其自身快照说明为准。
当时按以下顺序执行：

1. 阅读仓库 `AGENTS.md`、ADR 0037、`docs/formats/PORTABLE_PRESENTATION.md`、本计划第 0、3、4、5、6、
   11 节和阶段 3 完成报告。确认工作树状态；保留任何尚未提交的最终关闭文档，不覆盖用户改动。
2. 以 commit `b71ef23b2f258d88e274a9b4b13665ef10a39845` 为 Stage 3 实现基线，并确认当前 review
   HEAD 包含该 commit。Linux Quality run `31270268057`、Windows MSVC run `31270267999` 和
   Windows MinGW run `31270268010` 是最终 CI 证据；旧计划 run 不得替代。
3. 采用 code-review 立场，先报告问题，再给摘要。finding 按 P0-P3 严重度排序，必须包含可复现的
   行为风险、紧凑文件/行号引用和缺失测试；没有问题时明确写出 remaining test gaps 与残余风险。
4. 审查 3B/3C：untrusted payload/整数溢出/预算、candidate/active manifest、owning acquisition、
   source/provider 生命周期、Snapshot portable refs、normalizer、FrameDigest v1-v3 兼容和热路径分配。
5. 审查 3D/3E：public capability preflight、Validation Sink 独立性、prepare/commit/activate 事务顺序、
   stale/failure rollback、OpenGL context owner、GPU cache retire、Opaque/Transparent 排序与像素 golden。
6. 审查 3F/3G：static/shared export/import、package component/version、adapter-disabled closure、公共头
   ASCII/第三方泄漏、license/NOTICE、target allowlist，以及本地与 hosted 证据是否支持完成声明。
7. review 阶段默认只读代码并运行必要的定向验证，不自动修复 finding。将结论写入
   `docs/stage_reports/260808-stage-3-review.md`；修复批次必须由项目所有者在 review 后单独授权。

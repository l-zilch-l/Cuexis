# ADR 0037：Stage 3 可移植表现与渲染适配合同

日期：2026-08-07

状态：已接受

关系：细化 ADR 0027 和 ADR 0006 的宿主渲染边界，并取代其中
`FrameSnapshot/RenderPacket` 作为并列公共输出的模糊表述；不改变其 Playback SDK 产品方向。

## 背景

阶段 2 已让 `PlaybackSession` 输出拥有型 `FrameSnapshot`，并加入 Visibility、Material ID、
Opacity 和 Tint。当前 Mesh、Material 和 Texture 仍是有界 opaque CPU blob，
`ObjectSnapshot` 也没有 Mesh 引用、内容版本、Pass 或排序合同。Player 虽然只消费
`FrameSnapshot`，但仍通过应用私有转换生成 Debug Line，尚未形成宿主可实现的真实资源上传和
绘制闭环。

路线文档曾同时使用 `FrameSnapshot/RenderPacket`，并把 `PipelineDesc`、`BufferDesc`、
`MaterialAsset` 与固定 Pass 都列入阶段 3。这会产生两套权威帧模型，并使阶段 3 提前承担阶段 5
的材质和 Shader 管线。资源引用若只有 AssetId 而没有可验证的内容身份，reload 后的 adapter
缓存也无法安全失效。

## 决策

### 单一权威帧模型

公共 Runtime 输出继续只有 `cuexis::playback::FrameSnapshot`。它保持 ADR 0030 已冻结的拥有型
值对象合同；Stage 3 是扩展其可观察表现字段，不重新定义其基本所有权。

`RenderPacket` 不作为第二个 Playback 公共输出类型。内建或宿主 adapter 可以把
`FrameSnapshot` 和 portable resource 转换为短生命周期命令包、排序列表或上传批次，但这些
对象只属于 adapter 输入，必须从同一 Snapshot 确定派生，不得形成另一条 Runtime 求值路径。

### Portable Presentation Profile v1

Stage 3 只冻结真实绘制闭环所需的最小表现资源：

- Mesh：Triangle List；有限的 float position 必需，UV0 可选；使用显式 index；不包含骨骼、
  Morph、邻接、任意顶点语义或通用 vertex layout。
- Texture2D：二维、单层、非压缩 RGBA8；显式 sRGB/linear 色彩空间；v1 只要求 mip 0；固定
  linear filtering 与 clamp-to-edge，不建立通用 Sampler API。
- Unlit Material：base color、可选 base-color Texture2D、`Opaque`/`Blend` alpha mode 和
  double-sided 标志。对象 Snapshot 的 tint/opacity 与材质值确定组合。

精确公共 C++ 字段布局、版本字段、序列化 format ID 和数值/内存预算必须在 3A 文档门禁中
冻结后才能写生产代码。它们不得扩张为可编程 Shader、通用 Pipeline/Buffer、反射、Variant、
任意参数 Schema 或完整 RenderState。

### 资源引用、获取与 reload

`FrameSnapshot` 使用资源类型、类型化 AssetId 和确定性语义内容身份引用 portable resource。
公共引用不包含 provider revision、ResourceManager generation、source epoch 或其他进程内身份。
语义内容身份必须能区分同一 AssetId 在成功 reload 后发生的内容变化，并让跨
Filesystem/Memory/Host Provider 的相同规范化内容得到同一身份。

Playback 提供只读、同步、owner-thread 的资源 manifest/acquisition 边界，返回拥有型 portable
resource 值。它不公开 `AssetDatabase`、`ResourceManager`、ResourceHandle、ContentProvider 或
内部 blob。adapter 可以按资源类型、AssetId 和语义内容身份缓存；已获得的资源值在后续
update、reload、unload 和 Session 销毁后仍有效。

候选 load/reload 的资源 manifest 必须在 `PreparedPlayback` 提交前可验证和准备。portable
resource 解析/获取、adapter 能力验证或 GPU 上传失败时，候选 Playback 和 adapter 状态都不得
发布。adapter 不得重新解释 Provider 原始字节或 opaque blob。

事务顺序固定为：Playback 生成候选 `PreparedPlayback`；宿主读取候选 manifest 并获取拥有型
resource；adapter 在不修改活动 cache 的前提下准备候选状态；Playback commit 成功后，adapter
以不可失败的 move/swap 激活候选 cache。Playback commit 若因 stale、owner 或 lifecycle 检查
失败，宿主丢弃 adapter 候选。不得先切换 adapter，再执行仍可能失败的 Playback commit。

Stage 3 的 Mesh、Material 和 Texture2D 是必需表现资源。现有 Stage 1B 的 opaque built-in
fallback blob 不是合法 Portable Presentation v1 内容，不能进入 manifest、公开 acquisition 或
真实绘制路径。通用 `ResourceManager` 的 Fallback 行为和历史测试继续保留，但 Stage 3 候选若
解析到 fallback 必须在 commit 前失败；未来若需要可视占位资源，必须定义显式、有效、版本化的
portable asset 和单独策略。

旧 `FrameSnapshot` 的字段和值在 source 切换后仍有效。若宿主要在 source 退役后继续渲染旧帧，
必须保留此前取得的拥有型资源；Session 不承诺在旧 source 退役后重新解析尚未取得的旧资源。

用于 adapter cache 失效的 source epoch、manager token 或 provider revision 是运行时身份，不得
进入确定性 `FrameDigest`。Stage 3 的 digest 只使用可移植的语义字段和规范化内容身份。

### Camera、Viewport 与空间

Stage 3 不增加宿主相机覆盖。活动相机继续来自 Chart/Behavior，宿主每次提取帧时只提供
`FrameViewport`。右手系、`+X` 右、`+Y` 上、`+Z` 后、默认朝 `-Z`、米制、列向量和列主序合同
保持不变。

`FrameSnapshot` 的 view/projection 是 Cuexis 权威相机结果。OpenGL、Vulkan 或宿主引擎的
clip-space、Y 方向和深度范围转换由 adapter 完成，不回写 Chart、Behavior、World 或 Snapshot。
presentation-only camera override 留到真实 Studio 或宿主消费者出现后另行形成 ADR。

### Pass、排序与能力

Portable Presentation Profile v1 的项目内容只使用 `Opaque` 和 `Transparent`：

- `Opaque` 对 adapter 是必需能力；可在不改变结果的前提下重排或批处理，但验证输出使用稳定
  Object ID 顺序。
- `Transparent` 对 adapter 是必需能力；按活动相机空间深度从远到近排序，深度相同以稳定
  Object ID 打破平局。精确深度键和非活动相机失败合同在 3A 冻结。
- `Debug` 是 adapter/应用生成的可选诊断 Pass，不属于 Chart 持久化内容，不改变
  `FrameDigest`。宿主可以关闭它并获得明确 effective setting；不得把跳过 Opaque/Transparent
  当作受控降级。

Stage 3 不引入 Light、Particle、UI 或 RenderGraph。宿主不满足 Portable v1 必需能力时在候选
提交前稳定失败。Stage 3 没有项目级静默降级字段。

### 配置所有权

Stage 3 只增加表现模块所需的 typed request、capability input 和 effective settings。它们是
进程内值，不是文件，不进入 Chart、ProjectConfig、UserPreferences、DeviceProfile 或
ResolvedSessionConfig。

viewport 是逐帧输入；backend 选择、窗口、swap interval、MSAA 和图形 API 专有设置由应用或
adapter 拥有。Stage 6/9 才负责把 UserPreferences、LaunchOptions、DeviceProfile 和
ResolvedAppConfig 组合为应用级配置。

## 模块与包边界

- `cuexis_playback` 拥有 FrameSnapshot、portable resource 引用和候选资源获取门面。
- `cuexis_render` 继续是内部表现转换模块，不成为受支持的直接安装 API。
- 无 GPU Validation Sink 必须在 adapter-disabled/headless 构建中工作。默认作为测试支持与仓库外
  consumer oracle，不成为新的安装组件；若 3A 要改变这一点，必须同时冻结 package 拓扑。
- `cuexis_render_opengl` 是可选 adapter，不得成为 Playback 的传递依赖。
- Player 必须使用与 external consumer 相同的 Playback/portable resource 路径。

Stage 3 是否把 OpenGL adapter 导出为独立可选 CMake component，以及 component 的最终名称，
在 3A 根据 static/shared 包闭包、SDL surface 所有权和干净 consumer 证据冻结。无论结果如何，
基础 `Cuexis::Playback` package 都不得要求 SDL、OpenGL、GLAD 或窗口系统。

## 影响

- `FrameSnapshot` 可观察字段变化需要 `CUEXIS_SDK_API_VERSION` 评审、FrameDigest 新算法版本、
  static/shared consumer 和旧 Snapshot 生命周期回归。
- Asset blob 必须经过版本、类型、有限性、引用、预算和依赖闭包校验后才能发布 portable
  resource。
- OpenGL adapter 和 Validation Sink 必须消费同一排序、能力和资源合同。
- 阶段 5 从 Stage 3 的固定 Unlit Material 向版本化 Material/Shader 工作流扩展，不重新定义
  Portable v1 已有语义。

## 被拒绝的方案

### 公共 FrameSnapshot 与公共 RenderPacket 并存

拒绝。它会建立两套所有权、版本、digest 和 Runtime parity 合同。

### Stage 3 冻结通用 Buffer/Pipeline/Shader API

拒绝。当前只有固定 Unlit Material 消费者，通用接口会与阶段 5 重复并提前固化无验证抽象。

### 继续传递 opaque blob

拒绝。宿主无法验证字段、预算、字节序、顶点布局或颜色空间，也无法建立确定的上传和缓存合同。

### 宿主完全控制 Camera

拒绝。它会改变 Chart Camera、Behavior FOV、FrameDigest 和回放的可观察结果。Stage 3 只接受
Viewport 和 adapter 坐标转换。

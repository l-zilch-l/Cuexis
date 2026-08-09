# Cuexis Portable Presentation Profile v1

状态：Stage 3A 精确合同已接受；Stage 3B、3C 已于 2026-08-07 关闭；Stage 3D-3G 已于
2026-08-08 关闭；阶段 3 已最终验收完成

日期：2026-08-08

依据：[ADR 0037](adr/0037-stage-3-portable-presentation-contracts.md) 与
[Stage 3 实施计划](stage_plans/stage_3_implementation_plan.md)。

## 0. 文档定位

本文关闭 Stage 3A 的设计输入。项目所有者已于 2026-08-07 接受 S3A-01..S3A-12，授权按
Stage 3 计划实施 public header、C++ source、CMake target、fixture 和测试。Stage 3B 已实现 portable
resource、candidate manifest/acquisition、identity、预算和 rollback 合同；Stage 3C 已实现 Snapshot
portable refs、共同表现提取和 FrameDigest v3；Stage 3D 已实现 public capability preflight 与无 GPU
Validation Sink；Stage 3E 已实现 in-tree OpenGL presentation adapter 与 Player 真实绘制；Stage 3F
已完成 Playback-only external consumer、clean install、package compatibility 和 shared import/export
闭包。本文中的 API code block 是合同示例；实际可用表面以已实现公共头和测试为准。

Portable Presentation Profile v1 只定义：

```text
FrameSnapshot as the only public frame
Mesh triangle-list resource
Texture2D RGBA8 resource
Unlit Material resource
candidate manifest and owning acquisition
Opaque and Transparent normalized presentation
headless validation and in-tree OpenGL consumption
```

它不定义 Shader、通用 Pipeline/Buffer、RenderPacket、Light、Particle、UI、RenderGraph、宿主相机
覆盖、异步加载、稳定 C ABI 或跨工具链 binary compatibility。

## 1. 公共头和类型分组（S3A-01）

Stage 3 生产实现采用以下 public header 分组：

```text
cuexis/playback/presentation.hpp
  presentation enums
  semantic content identity
  typed resource references
  owning portable resource values
  manifest, capabilities, request and effective settings

cuexis/playback/playback_session.hpp
  PreparedPlayback candidate manifest/acquisition methods
  active PlaybackSession manifest/acquisition methods
  FrameSnapshot ObjectSnapshot resource references

cuexis/playback/frame_digest.hpp
  FrameDigest algorithm version 3
```

不安装新的 `cuexis/render` 公共接口。Playback public header 不得出现 SDL、OpenGL、GLAD、EnTT、
GLM、JSON DOM、ResourceManager、ResourceHandle、ResourceLease、Provider revision 或 GPU handle。

建议的 C++20 public sketch：

```cpp
namespace cuexis::playback {

enum class PresentationResourceType : std::uint8_t {
    Mesh = 1,
    Texture2D = 2,
    UnlitMaterial = 3,
};

enum class PresentationColorSpace : std::uint8_t {
    Linear = 1,
    Srgb = 2,
};

enum class PresentationAlphaMode : std::uint8_t {
    Opaque = 1,
    Blend = 2,
};

struct PresentationContentIdentity final {
    std::array<std::uint8_t, 32> sha256{};
};

struct PresentationResourceRef final {
    PresentationResourceType type{PresentationResourceType::Mesh};
    std::string assetId;
    PresentationContentIdentity identity;
};

struct PortableMesh final {
    std::vector<float> positions;
    std::vector<float> uv0;
    std::vector<std::uint32_t> indices;
    float boundsMin[3]{};
    float boundsMax[3]{};
};

struct PortableTexture2D final {
    std::uint32_t width{};
    std::uint32_t height{};
    PresentationColorSpace colorSpace{PresentationColorSpace::Linear};
    std::vector<std::byte> pixelsRgba8;
};

struct PortableUnlitMaterial final {
    float baseColor[4]{1.0F, 1.0F, 1.0F, 1.0F};
    PresentationAlphaMode alphaMode{PresentationAlphaMode::Opaque};
    bool doubleSided{};
    std::optional<PresentationResourceRef> baseColorTexture;
};

using PortableResourceValue =
    std::variant<PortableMesh, PortableTexture2D, PortableUnlitMaterial>;

struct PortableResource final {
    PresentationResourceRef reference;
    PortableResourceValue value;
};

using PortableResourcePtr = std::shared_ptr<const PortableResource>;

struct PresentationManifestEntry final {
    PresentationResourceRef reference;
    std::uint64_t encodedByteCount{};
    std::uint64_t decodedByteCount{};
    std::vector<PresentationResourceRef> dependencies;
};

struct PresentationResourceManifest final {
    std::uint32_t version{1};
    std::vector<PresentationManifestEntry> entries;
    std::uint64_t totalEncodedBytes{};
    std::uint64_t totalDecodedBytes{};
};

struct PresentationCapabilities final {
    std::uint32_t version{1};
    std::uint32_t portableProfileVersion{1};
    bool opaquePass{};
    bool transparentPass{};
    bool linearTexture{};
    bool srgbTexture{};
    bool straightAlphaBlend{};
    bool backFaceCulling{};
    bool doubleSided{};
    bool debugPass{};
    std::uint64_t maxResourceBytes{};
    std::uint64_t maxTotalDecodedBytes{};
    std::uint32_t maxTextureDimension{};
    std::uint32_t maxMeshVertices{};
    std::uint32_t maxMeshIndices{};
};

struct PresentationRequest final {
    std::uint32_t version{1};
    std::uint32_t portableProfileVersion{1};
    bool enableDebugPass{};
};

struct EffectivePresentationSettings final {
    std::uint32_t version{1};
    std::uint32_t portableProfileVersion{1};
    bool debugPassEnabled{};
};

struct PresentationValidationResult final {
    std::optional<EffectivePresentationSettings> settings;
    core::Diagnostics diagnostics;

    [[nodiscard]] bool hasValue() const noexcept;
};

class PresentationCandidateToken final {
  public:
    friend bool operator==(const PresentationCandidateToken&,
                           const PresentationCandidateToken&) = default;
  private:
    friend class PreparedPlayback;
    friend class PlaybackSession;
    std::uint64_t sessionToken_{};
    std::uint64_t candidateGeneration_{};
};

} // namespace cuexis::playback
```

`PortableResourcePtr` 是有意使用的共享所有权：候选 Playback、宿主 CPU cache 和 adapter candidate
cache 可以共享同一不可变资源；它不是内部 ResourceLease，也不借用 Provider blob。

`PreparedPlayback` sketch：

```cpp
class PreparedPlayback final {
  public:
    [[nodiscard]] auto presentationCandidateToken() const
        -> core::Result<PresentationCandidateToken>;
    [[nodiscard]] auto presentationManifest() const noexcept
        -> const PresentationResourceManifest*;
    [[nodiscard]] auto validatePresentation(
        const PresentationCapabilities& capabilities,
        const PresentationRequest& request) const
        -> PresentationValidationResult;
    [[nodiscard]] auto acquirePresentationResource(
        const PresentationResourceRef& reference) const
        -> core::Result<PortableResourcePtr>;
};
```

活动 Session 提供 owning manifest copy 和 owning resource：

```cpp
class PlaybackSession final {
  public:
    [[nodiscard]] auto presentationManifest() const
        -> core::Result<PresentationResourceManifest>;
    [[nodiscard]] auto acquirePresentationResource(
        const PresentationResourceRef& reference) const
        -> core::Result<PortableResourcePtr>;
};
```

`ObjectSnapshot` 保留 Stage 2 字段，并增加：

```cpp
std::optional<PresentationResourceRef> mesh;
std::optional<PresentationResourceRef> material;
```

Renderable 必须同时提供 Mesh 和 Material ref；非 Renderable 两者均为空。既有
`materialAssetId` 暂时保留，并必须与 `material->assetId` 相同，避免 Stage 2 consumer 的字段含义
被静默改变。

## 2. 所有权、线程和重入

- `PreparedPlayback::presentationManifest()` 返回借用指针，只在该 candidate 未被移动、提交、丢弃
  或销毁时有效。Manifest 内部字符串和数组由 candidate 拥有。
- `PlaybackSession::presentationManifest()` 返回 owning copy；其值不借用 Session。
- `acquirePresentationResource()` 返回的 `PortableResourcePtr` 及其中全部数组和字符串拥有自身
  生命周期，在 update、reload、unload、candidate discard、Session 销毁后仍有效。
- candidate prepare 必须在返回前完成全部表现闭包的读取、解析、规范化、依赖校验、identity 计算
  和预算检查。Acquisition 不再解析 Provider 字节。
- 所有 PreparedPlayback/PlaybackSession 表现方法只能从所属 Session owner thread 调用。
- Session 操作期间发生重入时返回 `playback.session.reentrant`；不执行部分操作。
- 公共方法捕获分配异常并转换为 `core::Result`；异常不得跨模块边界。
- `validatePresentation()` 使用有界 `PresentationValidationResult`，使一次 preflight 可以同时报告
  多个缺失 capability 和 Debug warning；owner/reentry 失败也作为 error diagnostic 返回。
- 析构、candidate discard 和 adapter candidate discard 必须 `noexcept`。

## 3. Resource ref 和语义身份（S3A-02）

`PresentationResourceRef` 的确定性部分固定为：

```text
PresentationResourceType
normalized AssetId
32-byte SHA-256 semantic content identity
```

AssetId 沿用 AssetDatabase 的 portable ASCII 身份和 256-byte 上限，不进行大小写折叠、Unicode
归一化或路径解释。

identity 使用 SHA-256，不使用 FrameDigest 的 FNV-1a，也不使用 Provider revision、source path、
文件时间、manager token、slot generation、candidate token 或 GPU object ID。SHA-256 输入是本规范
定义的 canonical semantic encoding：

```text
domain bytes: "cuexis.portable.presentation.v1\0"
resource type as little-endian uint32
normalized typed fields in the order defined below
```

规范化规则：

- 所有整数使用固定宽度 little-endian。
- float 使用 IEEE-754 binary32 位模式；`-0` 规范化为 `+0`；NaN/Inf 在 hash 前拒绝。
- 字符串先写 little-endian uint32 byte length，再写原始 portable ASCII bytes。
- Mesh identity 包含 positions、UV presence/values 和 indices；derived bounds 不重复进入 identity。
- Texture identity 包含 width、height、color space 和 RGBA8 pixels。
- Material identity 包含 base color、alpha mode、double-sided、texture AssetId，并包含被引用 Texture2D
  的 semantic identity。因此纹理内容 reload 会改变 Material identity。

同一 active/candidate 比较域内若两个不同 canonical semantic encoding 得到同一 SHA-256，整个
candidate 以 `playback.presentation.identity_collision` 失败。相同 canonical encoding 必须复用
同一不可变 resource value。跨 Filesystem、Memory 和 Host Provider 的相同内容必须产生完全相同
的 identity。

四类身份互不替代：

| 身份 | 作用 | 是否公开 | 是否进入 FrameDigest v3 |
| --- | --- | --- | --- |
| Semantic content identity | 跨 Provider 的 portable 内容身份和 cache key | 是 | 是 |
| ResourceManager content revision | 当前 provider slot 的运行时修订 | 否 | 否 |
| Candidate/source epoch | stale candidate 和 adapter transaction 检查 | 仅 opaque token | 否 |
| FrameDigest | 一帧可观察语义的版本化摘要 | 是 | 自身 |

## 4. 统一 binary payload（S3A-03）

Stage 3 v1 的资产输入只支持一种 versioned binary payload。Filesystem、Memory、Host Provider 都
返回完全相同的 bytes；不增加第二套 typed-memory 输入格式。解析后的 public typed values 是输出，
不是另一种持久化输入。

每个 payload 使用 24-byte common envelope：

| Offset | Size | Field | Rule |
| ---: | ---: | --- | --- |
| 0 | 8 | magic | ASCII bytes `CXPRES01`, no terminator |
| 8 | 4 | resourceKind | little-endian: Mesh=1, Texture2D=2, UnlitMaterial=3 |
| 12 | 4 | payloadVersion | little-endian, v1 must equal 1 |
| 16 | 8 | totalByteCount | little-endian, must equal actual byte count |

规则：

- 所有整数为 unsigned little-endian；float 为 little-endian IEEE-754 binary32。
- `resourceKind` 必须与 Asset Index type 一致。
- unknown magic、kind、version、reserved bit、truncation、overflow、trailing bytes 均稳定拒绝。
- 实现先校验 envelope、计数和全部乘加溢出，再分配或读取 body。
- v1 不允许扩展尾部字段；新增字段必须使用新 payloadVersion。

Stage 1B opaque fixture 保持兼容但不伪装成 portable resource：若一个候选的全部必需
Mesh/Material blob 都不声明或尝试 `CXPRES` envelope，则候选继续使用历史内部资源路径，
`presentationManifest()` 返回空指针且 acquisition 不可用。一旦闭包中任何必需 blob 声明、尝试
`CXPRES` magic/prefix，或具有结构上可识别的 v1 envelope，其全部表现闭包都必须通过本节严格
校验；不能把损坏或混合 blob 回退为 Stage 1B opaque 内容。无 Renderable 的空闭包发布 version 1
空 manifest。

## 5. Mesh v1（S3A-04）

Mesh body 紧随 common envelope：

| Size | Field | Rule |
| ---: | --- | --- |
| 4 | vertexCount | `[1, 1,048,576]` |
| 4 | indexCount | `[3, 3,145,728]`, multiple of 3 |
| 4 | flags | bit 0 = UV0 present; all other bits zero |
| 4 | reserved | zero |
| `vertexCount * 3 * 4` | positions | x/y/z float array |
| optional `vertexCount * 2 * 4` | uv0 | u/v float array |
| `indexCount * 4` | indices | uint32 triangle-list indices |

语义：

- Cuexis 空间为右手系、米制、`+X` 右、`+Y` 上、`+Z` 后，默认视线朝 `-Z`。
- Front face 为从正面观察的 counter-clockwise winding。
- 每个 index 必须小于 vertexCount；实现不重排、不推断、不生成 index。
- position 必须有限，且每个分量绝对值不超过 `1,000,000` 米。
- UV0 缺失时 adapter 使用 `(0,0)`；存在时每个分量必须有限且位于 `[0,1]`。
- 三个重复 index 或面积为零的三角形以 `playback.presentation.mesh.degenerate_triangle` 拒绝。
- local AABB 从全部 position 确定计算；bounds min/max 写入 owning PortableMesh。
- Transparent sort origin 固定为 local AABB center，不增加单独可编辑 pivot/bounds 字段。

## 6. Texture2D v1（S3A-05）

Texture2D body：

| Size | Field | Rule |
| ---: | --- | --- |
| 4 | width | `[1,8192]` |
| 4 | height | `[1,8192]` |
| 4 | colorSpace | Linear=1, sRGB=2 |
| 4 | reserved | zero |
| `width * height * 4` | pixels | tightly packed RGBA8 |

语义：

- 第一行是图像顶部，行内从左到右；UV `(0,0)` 表示左上角，`v` 向下增加。
- channel order 固定为 R、G、B、A；无 row padding；只包含 mip 0。
- RGB 在 `Srgb` 时采样后转为 linear；Alpha 始终按 linear 值解释。
- pixel alpha 是 straight alpha，不允许 premultiplied payload。
- sampler 固定为 min/mag linear filtering 和 clamp-to-edge。
- dimension 限制和 64 MiB decoded-resource 限制同时生效；因此 8192 square RGBA8 不合法。
- adapter 负责图形 API 的 texture origin、format 和 sRGB upload 转换，不修改 public pixels。

## 7. Unlit Material v1（S3A-06、S3A-07）

Material body：

| Size | Field | Rule |
| ---: | --- | --- |
| 4 | alphaMode | Opaque=1, Blend=2 |
| 4 | doubleSided | exactly 0 or 1 |
| 16 | baseColor | linear RGBA float32, each in `[0,1]` |
| 4 | textureAssetIdByteCount | 0 or `[1,256]` |
| 4 | reserved | zero |
| variable | textureAssetId | portable ASCII AssetId bytes |

`Opaque` Material 的 baseColor alpha 必须为 1。对象的 Stage 2 opacity 仍可使最终对象进入
Transparent。`Blend` 允许 `[0,1]` alpha。v1 不支持 Mask、cutoff、PBR 或任意参数。

Material payload 是 texture slot 的唯一语义权威；Asset Index dependencies 是加载闭包的唯一
结构权威，两者必须严格一致：

```text
no texture in payload -> dependency list must be empty
one Texture2D AssetId in payload -> dependency list must contain exactly that one ID
Mesh and Texture2D -> dependency list must be empty
```

dependency type 必须是 Texture；重复、额外、缺失、类型错误或环均在 candidate prepare 失败。
Portable v1 的表现依赖深度固定最多 2（Material -> Texture2D），不改变通用 AssetDatabase 的
64-depth 历史预算。

最终颜色在 linear space 计算。对象级 alpha 与纹理 alpha 分离，使 Pass 可以在不执行纹理采样或
光栅化的情况下确定：

```text
sampleColor = texture ? sample(baseColorTexture) : (1,1,1,1)
effectiveRgb = sampleColor.rgb * material.baseColor.rgb * object.materialTint
objectAlpha = material.baseColor.a * object.materialOpacity
coverageAlpha = material.alphaMode == Blend ? sampleColor.a : 1
fragmentAlpha = coverageAlpha * objectAlpha
```

所有因子均在 `[0,1]`。不执行隐式 clamp、premultiply 或 gamma-space multiplication。
`Opaque` Material 忽略 texture alpha；`Blend` Material 使用 texture alpha。Validation/排序不扫描
texture pixels，normalized record 中的 `effectiveAlpha` 指 `objectAlpha`。

Pass 分类：

```text
Transparent when material.alphaMode == Blend or objectAlpha < 1
Opaque otherwise
```

`objectAlpha == 0` 仍生成 Transparent normalized record；是否由 GPU early-discard 不改变验证摘要。

Raster state：

| State | Opaque | Transparent |
| --- | --- | --- |
| Depth test | enabled, Less | enabled, Less |
| Depth write | enabled | disabled |
| Blend | disabled | source-over, SrcAlpha / OneMinusSrcAlpha, Add |
| Front face | CCW | CCW |
| Cull | back unless doubleSided | back unless doubleSided |

Portable canonical clip space 使用右手 view、`-Z` forward、NDC X/Y/Z `[-1,1]`、Y up。现有
FrameSnapshot projection matrix 保持该合同；非 OpenGL adapter 独自转换 depth range/Y 方向。

## 8. Manifest、闭包和预算（S3A-08）

Candidate manifest 包含所有可能在播放期间被 Snapshot 引用的 Mesh、Material 和 Texture2D，
包括 Material Step Event 的全部候选值，而不是只包含 prepare 时刻当前可见资源。

Manifest entry 稳定排序键：

```text
AssetId raw ASCII bytes ascending
then PresentationResourceType numeric value ascending
```

同一 candidate 中 `(AssetId,type)` 唯一。Dependency entry 使用同样的完整 typed ref 和 semantic
identity。Asset Index 输入顺序、Provider 返回顺序和 Runtime Entity 顺序不影响 manifest。

硬预算：调用方可以设置更严格上限，但不能放宽以下 Profile v1 最大值。

| Budget | Maximum | 依据 |
| --- | ---: | --- |
| Encoded bytes per resource | 64 MiB | 与 ContentProvider/ResourceManager 默认上限一致 |
| Decoded bytes per resource | 64 MiB | 避免单资源扩张；覆盖 v1 Mesh/Texture |
| Total encoded presentation bytes | 512 MiB | 新增 candidate/session 峰值硬上限 |
| Total decoded presentation bytes | 512 MiB | 新增 CPU cache 硬上限 |
| Manifest entries | 65,536 | 收窄 AssetDatabase 100,000 上限以限制 host/GPU cache |
| AssetId bytes | 256 | 与 AssetDatabase 一致 |
| Dependencies per portable entry | 1 | v1 仅 Material -> Texture2D |
| Portable dependency depth | 2 | v1 固定闭包 |
| Mesh vertices | 1,048,576 | 固定 uint32/count 和 decoded byte 预算 |
| Mesh indices | 3,145,728 | 最多 1,048,576 triangles |
| Texture dimension | 8,192 | 与 decoded byte limit 同时生效 |
| Snapshot/normalized records | 100,000 | 与 Chart maxObjects 一致 |
| Diagnostics per operation | 1,024 | 与现有 Chart/Assets/Runtime 一致 |

所有计数、offset、stride、`width * height * 4`、vertex/index bytes 和 session total 使用 checked
integer arithmetic。达到 diagnostic limit 时保留一个稳定 limit diagnostic，不继续无界收集。

`decodedByteCount` 是独立于 STL layout/allocator 的规范预算值：

```text
Mesh = positions bytes + UV0 bytes + index bytes + 24 bounds bytes
Texture2D = RGBA8 pixel bytes + 12 bytes for width/height/colorSpace
Unlit Material = 32 fixed bytes + optional (37 bytes + texture AssetId bytes) typed ref
Resource ref = 37 bytes + AssetId bytes
Manifest entry = 20 fixed bytes + one resource ref + all dependency refs
Manifest header = 24 bytes
```

Session total decoded bytes是各 resource decodedByteCount 加 manifest overhead；不会使用
`sizeof(std::vector)`、allocator capacity、shared_ptr control block 或平台 ABI 大小。3G 另行测量真实
peak memory，但规范预算在 Windows/Linux 上必须相同。

旧 source 退役规则：

- 已取得的 `PortableResourcePtr` 永久保持其 owning value。
- owning manifest copy 永久保持 refs 和 identity，但不使 source 保持可获取。
- 未在旧 candidate/active source 退役前取得的旧 resource，不能通过新 Session 状态重新请求。
- 宿主若缓存旧 FrameSnapshot，必须同时保留其引用的 `PortableResourcePtr`。

## 9. Candidate transaction（S3A-09）

固定状态机：

```text
prepareLoad / prepareReload
  -> fully validated PreparedPlayback
  -> candidate token + manifest + owning acquisition
  -> validate PresentationCapabilities and request
  -> adapter prepares candidate cache without touching active cache
  -> PlaybackSession::commit(candidate)
  -> adapter activate(candidate cache) by noexcept move/swap
```

规则：

- Candidate token 只用于把 adapter candidate 与 PreparedPlayback 对齐，不是内容 identity。
- token 在 PreparedPlayback move 后随 candidate 移动；moved-from candidate 无效。
- adapter prepare 不允许 callback 进入 PlaybackSession，也不允许激活 active cache。
- Playback commit 仍执行现有 owner、reentry、session token、generation 和 lifecycle 检查。
- commit 失败时宿主销毁 adapter candidate；旧 Playback/cache 保持不变。
- commit 成功后的 adapter activation 必须只做不可失败的 owner-thread move/swap；资源上传、Shader
  编译、容量增长和 capability 检查都必须提前完成。
- PreparedPlayback 未被 commit 时，其析构等价于 discard；无日志回调、无异常。
- SDK 无法替宿主强制 adapter 调用顺序；Player 和官方示例必须把此顺序作为唯一支持路径。

## 10. Frame、排序和空场景（S3A-10）

共同表现提取只消费 owning/immutable portable resources 和 FrameSnapshot。

- `visible == false` 或缺少 Mesh/Material ref 的非 Renderable 不产生 command。
- Renderable ref 缺失、ref type 错误、identity 不匹配或 resource 不在 manifest 中时整帧失败，不发布
  partial summary/commands。
- 所有 world/view/projection、bounds、effective color 和 depth 中间值必须有限。
- local sort origin 为 Mesh AABB center，经 Object world matrix 和 Camera view matrix 转换。
- camera-space depth 定义为 `-viewPosition.z`，单位为米。
- bounds center、world transform 和 view transform 从 Snapshot/PortableMesh 的 binary32 值按固定
  column-major 顺序提升到 binary64 计算，不允许 adapter 使用自己的 bounds/pivot。
- Transparent canonical depth key 为 `round(depthMeters * 4096)` 的 signed 64-bit 值，halfway case
  away from zero；先按 key 降序，再按 Object ID raw ASCII bytes 升序。该 1/4096 m 量化是跨编译器
  稳定排序合同。
- depth key 计算溢出或非有限时整帧失败。
- Opaque canonical order 固定按 Object ID raw ASCII bytes 升序。Adapter 可以内部批处理重排，但必须
  能输出 canonical order 用于 parity，且不得改变 depth/blend 可观察结果。
- 有任何 visible Renderable 时必须有 active Camera；否则返回
  `playback.presentation.frame.camera_required`。
- 空帧、无 Renderable 或全不可见帧在无 active Camera 时也成功，产生空 Opaque/Transparent list，
  仍允许 clear/present。
- active Camera 的矩阵非有限或不可用时，只要存在 visible Renderable 就失败。
- Debug records 在 Opaque/Transparent 后单独产生，不进入 Chart、portable resource、排序或
  FrameDigest。

## 11. Capability、request 和 effective settings（S3A-11）

`PlaybackCapabilitySet` 继续表示 Chart/Runtime 语义（例如 Chart v3、Behavior Event、Material
Snapshot）；它由 PlaybackSession 在资源读取前执行 preflight。

`PresentationCapabilities` 是 adapter 事实，只用于 candidate presentation preflight：

- Portable v1 必须支持 Opaque、Transparent、linear Texture、sRGB Texture、straight-alpha
  source-over、back-face culling 和 double-sided disable-cull。
- adapter 的 count/byte/dimension limits 必须大于等于 candidate manifest 的实际要求。
- 缺少任何必需能力时 candidate presentation validation 失败，不产生降级后的有效设置。
- Debug 是唯一可选能力。Request 关闭 Debug 时 effective 为 false；Request 开启但 adapter 不支持
  时 validation 成功、effective 为 false，并产生 `playback.presentation.debug_unavailable` warning。
- capability/request/effective values 不持久化，不进入 Chart、ProjectConfig、UserPreferences、
  ResolvedSessionConfig、semantic identity 或 FrameDigest。
- backend、window、swap interval、MSAA 和 context attributes 不进入这些 Playback types。

## 12. Diagnostics（S3A-12）

以下 code 在 Stage 3 v1 中稳定；实现可以增加 context，但不得复用 code 表达不同失败。

| Code | Meaning |
| --- | --- |
| `playback.presentation.payload.magic_invalid` | Common magic mismatch |
| `playback.presentation.payload.type_mismatch` | Payload kind and Asset Index type differ |
| `playback.presentation.payload.version_unsupported` | Unknown payload version |
| `playback.presentation.payload.truncated` | Header/body bytes are missing |
| `playback.presentation.payload.size_mismatch` | Declared and actual size differ or trailing bytes exist |
| `playback.presentation.payload.reserved_nonzero` | Reserved field/flag is non-zero |
| `playback.presentation.payload.integer_overflow` | Checked offset/count calculation overflowed |
| `playback.presentation.resource.budget_exceeded` | Per-resource encoded/decoded limit exceeded |
| `playback.presentation.session.budget_exceeded` | Manifest/session count or total-byte limit exceeded |
| `playback.presentation.mesh.vertex_count_invalid` | Vertex count outside v1 range |
| `playback.presentation.mesh.index_count_invalid` | Index count outside range or not divisible by three |
| `playback.presentation.mesh.index_out_of_range` | Index does not name a vertex |
| `playback.presentation.mesh.value_invalid` | Position/UV is non-finite or outside v1 range |
| `playback.presentation.mesh.degenerate_triangle` | Triangle has repeated indices or zero area |
| `playback.presentation.texture.dimension_invalid` | Width/height invalid |
| `playback.presentation.texture.color_space_unsupported` | Color space is not Linear or sRGB |
| `playback.presentation.texture.pixel_size_invalid` | Pixel byte count is not width*height*4 |
| `playback.presentation.material.value_invalid` | Base color, alpha mode or double-sided invalid |
| `playback.presentation.material.texture_reference_invalid` | Texture AssetId/type is invalid |
| `playback.presentation.dependency.mismatch` | Payload reference and Asset Index dependencies differ |
| `playback.presentation.dependency.cycle` | Portable dependency closure contains a cycle |
| `playback.presentation.resource.missing` | Required indexed resource is unavailable |
| `playback.presentation.identity_collision` | Different canonical values produced one identity |
| `playback.presentation.reference.invalid` | Ref type/AssetId/identity is not in candidate/active manifest |
| `playback.presentation.capability.version_unsupported` | Capability/request version unsupported |
| `playback.presentation.capability.profile_unsupported` | Portable profile version unsupported |
| `playback.presentation.capability.required_missing` | Required adapter semantic capability missing |
| `playback.presentation.capability.limit_insufficient` | Adapter limit is below candidate requirement |
| `playback.presentation.debug_unavailable` | Optional Debug request not enabled (warning) |
| `playback.presentation.frame.camera_required` | Visible Renderable exists without active camera |
| `playback.presentation.frame.non_finite` | Matrix/color/depth calculation is non-finite |
| `playback.presentation.frame.resource_mismatch` | Snapshot ref and acquired resource differ |
| `playback.presentation.frame.command_budget_exceeded` | Normalized record limit exceeded |

通用 lifecycle 失败继续复用现有 `playback.session.not_owner_thread`、
`playback.session.reentrant`、`playback.prepared.invalid` 和 candidate/session mismatch code，不建立同义
presentation code。

所有 resource error 至少提供 `asset_id`、`resource_type`；binary error 增加 `byte_offset`；budget
error 增加 `limit` 和 `actual`；capability error 增加 `capability`。

## 13. SDK、Digest 和 package 决策（S3A-12）

提案版本：

```text
CUEXIS_SDK_API_VERSION: 0.4.0 -> 0.5.0
FrameDigest algorithm: 2 -> 3
Portable Presentation Profile: 1
Presentation manifest: 1
Presentation capability/request/effective settings: 1
Binary payload envelope/body: 1
```

`0.5.0` 是 minor preview 升级，因为新增 public types/methods 并扩展 FrameSnapshot。Stage 3B 在
首次加入 manifest/acquisition public API 时更新实际版本文件；Stage 3C 在同一 minor 中补齐
FrameSnapshot resource refs 与 FrameDigest v3。

FrameDigest v3 包含 Stage 2 v2 的全部字段，并按 ObjectSnapshot 顺序增加：

```text
mesh presence, type, AssetId, 32 identity bytes
material presence, type, AssetId, 32 identity bytes
```

v3 不包含 portable resource arrays、manifest order、provider revision、candidate token、source epoch、
GPU cache、normalized command order 或 Debug Pass。Material/Texture 的全部语义通过递归 semantic
identity 进入摘要。算法 version 1/2 的实现定义和 golden 必须保留，不能把新字段加入旧算法。

Package topology 提案：

- Portable Presentation public API 是现有 `Playback` component 的一部分，不建立 `Presentation`
  component。
- Validation Sink 继续是 tests/external-consumer oracle，不安装。
- Stage 3 不安装 `OpenGL` component。现有 `cuexis_render_opengl` 保持仓库内可选 build target，供
  Cuexis Player 使用。
- 原因：当前 adapter public header 依赖 SDL window/runtime 和内部 `cuexis_render` backend；在没有
  独立 native-surface/context ownership ADR 前导出会迫使 Platform/Render 一并成为公共组件。
- 基础 `Cuexis::Playback` static/shared package 继续不查找 SDL3、OpenGL 或 GLAD。
- 外部 Stage 3 consumer 通过安装后的 Playback presentation API 实现自己的 Validation/host adapter。
- OpenGL package component 延期到存在不依赖内部 header 的 surface/context 合同后重新评审，不在
  Stage 3 中以不完整闭包发布。

Stage 3 继续使用 matching-toolchain C++ shared preview；稳定 C ABI 仍属于阶段 12。

## 14. 3A 决策清单

| ID | 提案结论 | 状态 |
| --- | --- | --- |
| S3A-01 | Playback presentation header/type/method sketch 与 owner/reentry 规则 | 已关闭 |
| S3A-02 | type + AssetId + SHA-256 canonical semantic identity | 已关闭 |
| S3A-03 | 单一 `CXPRES01` little-endian binary payload v1 | 已关闭 |
| S3A-04 | uint32 indexed CCW Mesh、optional UV0、derived AABB center | 已关闭 |
| S3A-05 | top-left RGBA8、Linear/sRGB、linear+clamp、mip0 | 已关闭 |
| S3A-06 | linear Unlit、straight alpha、Opaque/Blend、fixed depth/cull | 已关闭 |
| S3A-07 | Material payload selects Texture; index dependency must exactly match | 已关闭 |
| S3A-08 | sorted owning manifest、64 MiB/resource、512 MiB/session | 已关闭 |
| S3A-09 | opaque candidate token、prepare/commit/no-fail activate | 已关闭 |
| S3A-10 | AABB center depth、1/4096 m key、ObjectId tie、empty-frame success | 已关闭 |
| S3A-11 | Playback semantic capability 与 adapter presentation capability 分离 | 已关闭 |
| S3A-12 | diagnostics table、SDK 0.5.0、Digest v3、no OpenGL package component | 已关闭 |

项目所有者已接受全部提案，S3A-01..S3A-12 已关闭并允许进入 3B。

## 15. 3B 关闭状态

Stage 3B 已于 2026-08-07 关闭。实现提供 `presentation.hpp` owning value、PreparedPlayback
candidate token/manifest/acquisition、active Session owning manifest/acquisition、`CXPRES01` v1
解析与规范化、SHA-256 semantic identity、64 MiB/512 MiB 预算、严格依赖验证、Stage 3 fixture 和
Filesystem/Memory/Host 等价测试。Stage 1B 全 opaque closure 继续可加载且不发布 portable manifest。

本地 Windows/MSVC 的 Debug、Release 筛选、headless static、headless shared、external/package
consumer、格式、静态分析和公共头 ASCII 门禁已通过。Stage 2 hosted Linux GCC/Clang、sanitizer 与
package consumer run URL 仍未关闭，因此此状态不表示 Stage 3 跨平台最终验收。项目所有者后续已
单独接受 3D 实施批次。

## 16. 3C 关闭状态

Stage 3C 已于 2026-08-07 关闭。`FrameSnapshot::ObjectSnapshot` 现在拥有 optional Mesh/Material
`PresentationResourceRef`，prepare 为初始 Material 和全部 Material Step 候选建立绑定并预留热路径
容量。已返回 Snapshot 在 update、reload、unload 和 Session 销毁后继续拥有完整 ref 值。

共同 normalizer 只消费 Snapshot、manifest 和 owning immutable portable resources；它不访问 World、
EnTT、SDL、OpenGL 或 host-engine 类型。实现严格校验 ref/type/identity/manifest membership 和
Material -> Texture2D 依赖，计算 effective RGB/object alpha、固定 depth/cull/blend state、Opaque/
Transparent pass、AABB-center camera depth 与规范化排序。只有 visible Renderable 触发相机和数值
计算；空帧、无 Renderable 和全不可见帧无需 active Camera 并产生空记录。

FrameDigest 默认算法版本已升级为 3，按 ObjectSnapshot 顺序增加 Mesh/Material presence、type、
AssetId 和 32-byte semantic identity。版本 1/2 的算法定义与 historical golden 保留不变。Stage 3
fixture 和测试覆盖 Material Step、opacity/tint、Visibility、Stop、Seek、Reload、Provider parity、
资源生命周期、static/shared package consumer 和 warmed 零分配路径。

本地 Windows/MSVC Debug 全量 260 项通过，Release 完整构建、3C/architecture 筛选与五个 external
consumer 通过；format、direct clang-tidy、公共头 ASCII 和 `git diff --check` 通过。Stage 2 hosted
Linux GCC/Clang、sanitizer 与 package consumer run URL 仍未关闭，因此 3C 关闭不等于 Stage 3 完成。
项目所有者后续已单独接受 3D 实施批次。

## 17. 3D 关闭状态

Stage 3D 已于 2026-08-08 关闭。`PreparedPlayback::validatePresentation()` 现在公开执行 candidate
presentation preflight：它验证 capability/request version、Portable profile version、七项 Portable
v1 必需语义能力，以及 candidate 的 per-resource、total decoded、texture dimension、mesh vertex 和
mesh index limits。Debug 是唯一可选降级项；请求 Debug 而 adapter 不支持时 validation 成功，
effective setting 为 false，并产生 `playback.presentation.debug_unavailable` warning。

`tests/presentation/validation_sink.hpp/.cpp` 提供 Stage 3 的第一个独立 consumer。该实现不安装，不
包含 Assets、Runtime、Render、SDL、OpenGL 或 GLAD header，只消费 Core/Playback 公共 API。它从
candidate manifest 获取 owning portable resources，独立重新计算 SHA-256 semantic identity，并验证
resource type/ref、manifest membership、Material -> Texture2D dependency、预算和规范资源顺序。这个
独立实现刻意不调用 Playback 的内部 common normalizer，因此可作为 3E OpenGL adapter parity 的
外部 oracle。

Validation Sink 遵循同一事务顺序：先 prepare validation candidate，之后 Playback commit，最后
`noexcept` activate；stale/failed candidate 不替换活动 token。每帧只消费活动 owning resources 与
`FrameSnapshot`，生成 versioned、bounded `ValidationSummary`：viewport、clear、camera、effective
color/alpha、fixed depth/cull/blend、Opaque/Transparent canonical order 和 optional Debug setting。
summary digest 使用 domain `cuexis.validation.summary.v1` 的 canonical FNV-1a 64-bit 编码；非法帧在
返回错误前清空 destination，不发布 partial commands。

测试覆盖 capability/profile/limit diagnostics、legacy opaque candidate、moved-from/stale candidate、
resource identity/type/dependency/order/budget、prepare/commit/activate、Material Step、Stop、Visibility、
Seek、Reload、canonical pass order、effective state、stable digest、camera/ref failure 和 empty/invisible
frame。warmup 后 128 次重复 frame validation 的动态分配计数为零。add_subdirectory 与 find_package
external consumer 从 Stage 3 fixture 完成 prepare -> validation -> commit -> activate -> frame summary，
并固定 ValidationSummary golden `18316288860163381829`；FrameDigest v3 external consumer golden 为
`6442711505793857933`。find_package consumer 只使用干净安装目录中的公共头。

本地 Windows/MSVC static Debug 全量 CTest 269/269、headless 236/236、shared Debug、static Release
筛选、static/shared external consumer、shared export/import、format、公共头 ASCII、`git diff --check`
和 direct `clang-tidy` 门禁均通过；`clangd --check` 无源码诊断，仅报告 ExtractFunction tweak 自检
失败。Windows symlink containment 测试按既有平台条件跳过。Stage 2 hosted Linux GCC/Clang、
sanitizer 与 package consumer run URL 仍是 3G 前的开放门禁，所以 3D 关闭不等于 Stage 3 完成。

## 18. 3E 关闭状态

Stage 3E 已于 2026-08-08 关闭。`cuexis_render_opengl` 现在是 Portable Presentation Profile v1 的
第二个独立 consumer：它以 candidate manifest 和 owning acquisition 事务准备 Mesh VAO/VBO/IBO、
RGBA8 linear/sRGB Texture2D 与固定 GLSL 330 Unlit pipeline，并按规范化 Opaque/Transparent 顺序
设置 depth、source-over blend、cull/double-sided 状态。Debug Pass、draw summary 和中心像素 probe
继续由 adapter 内部拥有，不向 Playback 公共头泄漏 GL 类型。

OpenGL candidate 是 move-only/noexcept activation surface。prepare、shader、upload 或 capability
失败时 candidate 被丢弃，活动 GPU cache 保持不变；Playback commit 成功后才 no-fail activate。
retired GPU cache 延迟到 context current 时销毁，因此析构和 activation 不要求在无 current context
时调用 OpenGL。Player 只组合 public Playback candidate/acquisition、`FrameSnapshot` 和 OpenGL
presentation adapter，不解析 portable payload 字节，也不维护第二条 Runtime/World 提取路径。

`--smoke-test` 固定加载 `stage3_project` 并执行六帧真实表现脚本：Opaque、textured Transparent、
全不可见 clear、失败 Playback reload、失败 adapter prepare 和成功原子 reload。Validation Sink 与
OpenGL adapter 的共同 summary golden 为 `18316288860163381829`；代表像素为 Opaque
`{255,204,51,255}`、textured Transparent `{51,32,71,192}` 和 clear `{14,16,18,255}`。投影修复后，
`core::makePerspective()` 由单元测试固定 near/far plane 到 OpenGL canonical NDC `[-1,+1]` 的映射。

本地 Windows/MSVC 验收包括 headless Debug 237/237、shared Debug 272/272、Release 270/270、
Debug/Release 六帧 GPU smoke、format、干净安装树 public header ASCII、direct `clang-tidy` 和
`clangd --check`。GPU 证据为 OpenGL 3.3.0 NVIDIA 596.36、NVIDIA Corporation、NVIDIA GeForce
RTX 4060 Laptop GPU。Windows symlink containment 测试按既有平台条件跳过。Stage 2 hosted Linux
run URL 与 Stage 3G 仍未关闭，因此本文不声明 Stage 3 已完成。

## 19. 3F 关闭状态

Stage 3F 已于 2026-08-08 关闭。`tests/external/playback_consumer.cpp` 现在由
`add_subdirectory_playback` 和 `find_package_playback` 两个模式复用，只链接 `Cuexis::Playback`，
并且只包含安装公共头。consumer 从自己的 clean work dir 加载 staged `stage3_project`，验证
candidate/active manifest、owning resource acquisition、Portable Mesh/Texture2D/Unlit Material 值、
capability preflight、commit/update/extract、FrameDigest v3 parity 和 unload 后资源所有权；`625 ms`
的 digest golden 固定为 `8424169740673868033`。

安装包继续只导出 Core、Audio、Content、Playback 与可选 AudioSDL；Stage 3 不安装 OpenGL
component。基础包不安装 Platform、RenderOpenGL 或内部 Runtime/World/Assets header，也不查找 SDL3、
OpenGL 或 GLAD。package 门禁明确拒绝 `0.4`、`0.6` 和未支持的 `OpenGL` component，并分别精确核对
基础包与 AudioSDL 包的 notice 集合。

shared 门禁要求 Playback 导出 manifest/acquisition/preflight API，并验证 Playback-only EXE 只直接
导入 Playback/Core，Playback DLL 只导入 Audio/Content/Core；两者均不导入 SDL、GLAD 或内部 Runtime
DLL。本地 Windows/MSVC standard Debug 272/272、headless Debug 239/239、static Release 272/272、
shared Debug 275/275 CTest 全部通过；format、public header ASCII、diff/whitespace 与 Playback-only
consumer 的直接 `clang-tidy`/`clangd --check` 门禁也通过。Stage 2 hosted Linux GCC/Clang、
sanitizer 与 package consumer run URL 仍是 3G 前的开放门禁，所以 3F 关闭不等于 Stage 3 完成。

## 20. 3G 与阶段 3 最终关闭状态

Stage 3G 已于 2026-08-08 关闭。Windows/MSVC fresh Debug/Release、static/shared、headless、
adapter-disabled、external package、GPU smoke 和 performance probe 均通过；WSL GCC/Clang、shared、
ASan+UBSan、clang-tidy 与 coverage 本地矩阵也通过。

最终实现 commit 为 `b71ef23b2f258d88e274a9b4b13665ef10a39845`。Hosted Linux Quality
[run 31270268057](https://github.com/l-zilch-l/Cuexis/actions/runs/31270268057) 的 GCC Release、
GCC shared Release、Clang shared Debug、Clang ASan+UBSan、clang-tidy 和 GCC coverage 全部成功；
Windows MSVC run `31270267999` 与 Windows MinGW run `31270268010` 也全部成功。Stage 2 遗留
hosted Linux 门禁与 Stage 3 完成定义全部关闭。

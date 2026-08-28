# Cuexis Material and Shader Profile v1

状态：accepted contract；S5-C payload parse implemented；S5-D compile/reflect implemented behind shader-tools；S5-E profile/capability preflight implemented；S5-F CXSCCH01 cache implemented；S5-G OpenGL/Player/Validation Sink consumption implemented；S5-A 已于 2026-08-28 冻结

日期：2026-08-28

依据：[ADR 0040](../adr/0040-stage-5-material-shader-contracts.md)、[ADR 0021](../adr/0021-spirv-centered-shader-pipeline.md)
与 [Stage 5 实施计划](../stage_plans/stage_5_implementation_plan.md)。Portable Unlit v1 仍以
[PORTABLE_PRESENTATION.md](PORTABLE_PRESENTATION.md) 为唯一权威；本文不修改其字段。

归档设计输入：[SHADER_PIPELINE.md](../proposals/deferred/SHADER_PIPELINE.md)。字段、预算和
诊断码以本文为准。

## 0. 文档定位

本文关闭 S5-A 的设计输入。它冻结 Stage 5 生产实现必须遵守的 payload、观察面、capability、
预算、诊断码、模块拓扑和 SDK 决策。S5-B 起才允许按批次改 public header、C++、CMake、Schema、
fixture 与测试。实现完成前不得把本文描述为“已支持”。

Stage 5 v1 只定义：

```text
Asset Index v3 shader type
CXPRES01 Shader and ParameterizedMaterial kinds
GLSL 450 portable subset source
SPIR-V canonical IR and derived GLSL 330 / ES 300 cache
declared parameter schema, variant keywords and set/binding
three-layer renderer profiles
headless source acquisition without a shader compiler
```

它不定义 Shader Graph、PBR、Light、Mask、Compute、通用 Pipeline/Buffer API、Studio 编辑器、
稳定 C ABI 或把派生缓存当作 Chart 引用的源资产。

## 1. 公共头和类型分组（S5A-01）

Stage 5 把公开类型留在现有 Playback presentation 头中，不安装 `cuexis/shader`：

```text
cuexis/playback/presentation.hpp
  PresentationResourceType Shader and ParameterizedMaterial
  PortableShader
  PortableParameterizedMaterial
  PresentationCapabilities version 2 built-in fields
  PresentationRequest / EffectivePresentationSettings additive fields

cuexis/playback/playback_session.hpp
  capabilityShaderAssetV1
  capabilityMaterialParameterizedV1
```

`cuexis/shader/*.hpp` 只存在于可选内部库，不得安装。Playback public header 仍不得出现 SDL、
OpenGL、GLAD、EnTT、GLM、JSON DOM、shaderc、SPIRV-Cross、ResourceHandle 或 GPU handle。

建议的 C++20 public sketch（实现以冻结后的头为准；注释必须是 ASCII）：

```cpp
namespace cuexis::playback {

enum class PresentationResourceType : std::uint8_t {
    Mesh = 1,
    Texture2D = 2,
    UnlitMaterial = 3,
    Shader = 4,
    ParameterizedMaterial = 5,
};

enum class ShaderStage : std::uint8_t {
    Vertex = 1,
    Fragment = 2,
};

enum class ShaderParameterType : std::uint8_t {
    Float = 1,
    Vec2 = 2,
    Vec3 = 3,
    Vec4 = 4,
    Int = 5,
    Bool = 6,
    Texture2D = 7,
};

enum class RendererProfileKind : std::uint8_t {
    Portable = 1,
    BuiltIn = 2,
    HostExtension = 3,
};

struct ShaderBinding final {
    std::uint32_t set{};
    std::uint32_t binding{};
    ShaderParameterType type{ShaderParameterType::Float};
    std::string name;
};

struct ShaderParameterSchemaEntry final {
    std::string name;
    ShaderParameterType type{ShaderParameterType::Float};
    std::uint32_t set{};
    std::uint32_t binding{};
    std::array<float, 4> defaultNumeric{};
    std::int32_t defaultInt{};
    bool defaultBool{};
};

struct PortableShader final {
    std::string vertexSource;
    std::string fragmentSource;
    std::string vertexEntry{"main"};
    std::string fragmentEntry{"main"};
    std::vector<std::string> variantKeywords;
    std::vector<ShaderParameterSchemaEntry> parameters;
    std::vector<ShaderBinding> bindings;
    PresentationAlphaMode defaultAlphaMode{PresentationAlphaMode::Opaque};
    bool defaultDoubleSided{};
    std::string requiredRendererProfile;
    std::vector<std::string> requiredHostExtensions;
};

struct ShaderParameterValue final {
    std::string name;
    ShaderParameterType type{ShaderParameterType::Float};
    std::array<float, 4> numeric{};
    std::int32_t integer{};
    bool boolean{};
    std::optional<PresentationResourceRef> texture;
};

struct PortableParameterizedMaterial final {
    PresentationResourceRef shader;
    PresentationAlphaMode alphaMode{PresentationAlphaMode::Opaque};
    bool doubleSided{};
    std::vector<std::string> selectedKeywords;
    std::vector<ShaderParameterValue> parameters;
};

} // namespace cuexis::playback
```

`PortableResourceValue` 在 SDK `0.7.0` 增加 `PortableShader` 与 `PortableParameterizedMaterial`
替代项。旧 Unlit 替代项与字段布局保持不变。

`MaterialHandle` / `ResourceHandle` 仍是内部 assets 合同，不进入 Playback 公共头。Chart 与
FrameSnapshot 只使用稳定 AssetId 与 `PresentationResourceRef`。

## 2. 观察规则（S5A-08）

```text
UnlitMaterial payload            -> ObjectSnapshot.material.type = UnlitMaterial
ParameterizedMaterial payload    -> ObjectSnapshot.material.type = ParameterizedMaterial
Shader payload                   -> manifest dependency only; never an ObjectSnapshot material
```

禁止把 ParameterizedMaterial 投影成 UnlitMaterial，即使其 shader 在视觉上等价于内建 Unlit。
需要 Portable 最低能力的内容必须继续使用 kind 3 Unlit payload。同一 AssetId 不得因消费者不同
而改变 type 或 identity。

Renderable 仍必须同时提供 Mesh 与 Material ref。Material ref 的 type 只能是 UnlitMaterial 或
ParameterizedMaterial。Shader 出现在对象 material 上是 `playback.presentation.reference.invalid`。

对象级 `materialTint` / `materialOpacity` 对两类材质都有效。Built-in v1 adapter 通过保留
object uniform block 绑定它们，不把它们写入 Material payload。Pass 分类沿用 Portable v1：

```text
Transparent when effectiveAlphaMode == Blend or object.materialOpacity < 1
Opaque otherwise
```

`effectiveAlphaMode` 来自 ParameterizedMaterial.alphaMode（若缺省则用 Shader 默认值）。Unlit
路径的颜色公式不变。Parameterized 路径的 shader 输出颜色在 adapter 中再乘对象 tint/opacity：

```text
finalRgb = shaderRgb * object.materialTint
finalA   = shaderA * object.materialOpacity
```

## 3. Asset Index v3（S5A-02）

新增格式版本：

```text
cuexis.asset-index version 3
  保留 v1/v2 的 mesh / material / texture / audio
  增加 shader
```

Reader 必须先读显式 `version`，再使用对应类型表。v1/v2 遇到 `type: "shader"` 仍是未知类型
错误。v3 项目可以不含 shader 记录。

```json
{
  "format": "cuexis.asset-index",
  "version": 3,
  "assets": [
    {
      "id": "shader.sprite",
      "type": "shader",
      "source": "shaders/sprite.shader.bin",
      "dependencies": []
    },
    {
      "id": "material.sprite",
      "type": "material",
      "source": "materials/sprite.material.bin",
      "dependencies": ["shader.sprite", "texture.sprite"]
    }
  ]
}
```

依赖规则：

| Index type | Payload kind | Dependencies |
| --- | --- | --- |
| `shader` | Shader = 4 | 必须为空 |
| `material` | UnlitMaterial = 3 | 空，或恰好一个 Texture |
| `material` | ParameterizedMaterial = 5 | 恰好一个 Shader，加上 0 到 8 个 Texture；集合必须与 payload 引用一致 |
| `mesh` / `texture` / `audio` | 既有 | 不变；不得依赖 shader |

依赖图不得成环。Shader 是叶节点。Portable 表现依赖深度仍最多 2（Material → Shader 或
Material → Texture）。Audio 仍不得进入表现闭包。

`cuexis.project` 保持 version 1。CXC v1 manifest/container 字段不改。

## 4. 统一 binary payload（S5A-03）

源资产继续使用 Portable v1 的 24-byte `CXPRES01` envelope。`resourceKind` 增加 4 和 5；
`payloadVersion` 对 kind 4/5 必须为 1。unknown kind/version、reserved 非零、截断、溢出和
trailing bytes 仍稳定拒绝。

一旦候选闭包中任何必需 blob 声明 `CXPRES` magic，全部表现闭包必须通过严格校验。kind 4/5
不能回退为 Stage 1B opaque blob。

派生编译缓存使用独立 envelope，不得使用 `CXPRES01`：

| Offset | Size | Field | Rule |
| ---: | ---: | --- | --- |
| 0 | 8 | magic | ASCII `CXSCCH01`, no terminator |
| 8 | 4 | cacheVersion | little-endian, v1 equals 1 |
| 12 | 4 | reserved | zero |
| 16 | 8 | totalByteCount | little-endian, must equal actual byte count |

v1 body 紧随 envelope，全部整数 little-endian；字符串与 blob 先写 uint32 长度再写原始字节：

```text
sourceIdentity                 32 bytes
importerProfile                counted string
targetProfileCount             uint32, then counted strings (sorted unique)
keywordCount                   uint32, then counted strings (sorted unique)
vertexEntry / fragmentEntry    counted strings
toolCount                      uint32, then name/version counted strings (sorted by name)
vertexSpirv / fragmentSpirv    counted bytes
vertexGlsl330 / fragmentGlsl330 / vertexGlslEs300 / fragmentGlslEs300
                               counted strings
reflection                     counted bytes, cuexis.shader.reflection.v1
key                            32-byte SHA-256 of the cache key inputs
```

缓存键按第 10 节 length-prefixed SHA-256。文件名是该 key 的小写 hex 加 `.cxscch01`。目录由
importer/Player 显式传入，不扫描工程树。缓存不是 Asset Index 源，不进入
`acquirePresentationResource()`，不进入 FrameDigest。

## 5. ShaderAsset v1（S5A-04）

Shader body 紧随 common envelope。整数为 little-endian；字符串先写 uint32 字节长度再写
原始字节；源码为 UTF-8、禁止 BOM、行结束必须是 LF。CR 或 CRLF 以
`playback.presentation.shader.source_encoding_invalid` 拒绝。

| Size | Field | Rule |
| ---: | --- | --- |
| 4 | flags | bit 0 reserved zero; all bits zero in v1 |
| 4 | vertexEntryByteCount | `[1,64]`, ASCII identifier |
| 4 | fragmentEntryByteCount | `[1,64]`, ASCII identifier |
| 4 | keywordCount | `[0,4]` |
| 4 | parameterCount | `[0,32]` |
| 4 | bindingCount | `[0,16]` |
| 4 | hostExtensionCount | `[0,8]` |
| 4 | defaultAlphaMode | Opaque=1, Blend=2 |
| 4 | defaultDoubleSided | exactly 0 or 1 |
| 4 | profileByteCount | `[1,64]` |
| 4 | vertexSourceByteCount | `[1,262144]` |
| 4 | fragmentSourceByteCount | `[1,262144]` |
| variable | vertexEntry, fragmentEntry, profile, keywords, parameters, bindings, host extensions, sources | see below |

`requiredRendererProfile` v1 只允许：

```text
cuexis.renderer.builtin.v1
```

或一个 host-extension ID。不得填写 Portable profile；Portable 内容使用 Unlit payload。

Keyword、parameter name、binding name、entry 与 profile ID 必须匹配：

```text
^[A-Za-z][A-Za-z0-9_]{0,31}$
```

名称不得使用前缀 `cuexis`（大小写不敏感）。Keyword 在 shader 内唯一；parameter/binding name
在 shader 内唯一。

每个 parameter 记录：

```text
uint32 nameByteCount
name bytes
uint32 ShaderParameterType
uint32 set
uint32 binding
float32[4] defaultNumeric   // unused lanes zero; Bool/Int/Texture ignore except as specified
int32 defaultInt            // Int only; others zero
uint32 defaultBool          // Bool only, 0 or 1; others zero
```

Texture2D parameter 的 numeric/int/bool 默认值必须为零，默认纹理为空（运行时由材质提供；
材质缺省该槽位是错误）。

每个 binding 记录 `set`、`binding`、type、name。parameter 的 set/binding 必须出现在 binding
表中且 type 一致。binding 表可以包含无对应 parameter 的资源，但必须仍出现在声明 schema 中；
未声明资源是导入错误。

保留 object block，不写入 ShaderAsset binding 表：

```text
set = 0
binding = 0
block name CuexisObject
fields: mat4 world; mat4 viewProjection; vec3 tint; float opacity
```

用户 binding 的 `(set,binding)` 不得使用 `(0,0)`。v1 允许 `set` 仅为 0；`binding` 为 `[1,16]`。

### GLSL 450 portable subset

每个 stage 必须是单一 translation unit：

```text
#version 450
```

允许：vertex 与 fragment；显式 `layout(set,N,binding=M)`；`in`/`out` 匹配的用户 varying；
数值/布尔 uniform 与 sampled image。

禁止：`#include`；geometry/tessellation/compute；subroutine；8/16-bit storage；sparse
texture；ray tracing；mesh/task shader；旧版 compatibility profile；未声明宏；`#line` 以外
的编译器扩展；在源码中写 `CuexisObject` 以外的 `cuexis` 前缀符号。

Variant：编译器仅为选中的 keyword 定义 `#define NAME 1`；未选中的 keyword 保持未定义。源码
必须用 `#ifdef NAME`，不得依赖未声明名字。

## 6. Parameterized Material v1（S5A-05）

Material body：

| Size | Field | Rule |
| ---: | --- | --- |
| 4 | alphaMode | Opaque=1, Blend=2 |
| 4 | doubleSided | exactly 0 or 1 |
| 4 | keywordCount | `[0,4]`，必须是 shader 声明集的子集 |
| 4 | parameterCount | 必须等于 shader.parameterCount |
| 4 | shaderAssetIdByteCount | `[1,256]` |
| 4 | reserved | zero |
| variable | shaderAssetId, selected keywords, parameter values |  |

每个 parameter value 按 shader schema 的声明顺序写出，name 必须与 schema 对应项相同，type
必须相同。Texture2D 值写 `uint32 textureAssetIdByteCount` 加 AssetId 字节，count 为 `[1,256]`。
数值写 `float32[4]`；Int 写 `int32` 后跟 12 字节 zero padding；Bool 写 `uint32` 0/1 后跟
12 字节 zero padding。

`Opaque` 不要求某个参数 alpha 为 1；对象 opacity 仍可把物体送入 Transparent。v1 不支持 Mask。

Asset Index dependencies 必须与 payload 引用集合位级一致：

```text
dependencies = { shaderAssetId } union { every Texture2D parameter AssetId }
```

顺序按 AssetId raw ASCII bytes 升序比较；重复、额外、缺失、类型错误均失败。

## 7. Identity（S5A-06）

继续使用 SHA-256 与 Portable v1 的 domain 前缀规则，但 kind 4/5 的 canonical encoding 为：

```text
domain bytes: "cuexis.portable.presentation.v1\0"
resource type as little-endian uint32
normalized typed fields in the order defined below
```

Shader identity 包含：entry names、keyword 集合（按 ASCII 升序）、parameter schema、binding
表、default render state、profile、host extensions、LF 规范化后的 vertex/fragment source。
不包含 SPIR-V、GLSL 330/ES 300、工具版本或缓存字节。

Parameterized Material identity 包含：alphaMode、doubleSided、selected keywords（升序）、
全部 parameter 值，以及 shader 的 AssetId 与 semantic identity，以及每个纹理的 AssetId 与
identity。因此 shader 或纹理内容变化会改变材质 identity。

同一 candidate 内不同 canonical encoding 得到同一 SHA-256 仍以
`playback.presentation.identity_collision` 失败。

派生缓存键不进入 semantic identity。四类身份互不替代的表沿用 Portable v1。

## 8. Manifest、闭包和预算（S5A-07）

Candidate manifest 包含播放期间 Snapshot 可能引用的全部 Mesh、Unlit Material、Parameterized
Material、Shader 与 Texture2D，包括 Material Step Event 的全部候选。排序键仍是 AssetId 升序，
然后 `PresentationResourceType` 数值升序。

硬预算在 Portable v1 之上增加，且不得放宽 v1 已有上限：

| Budget | Maximum |
| --- | ---: |
| Encoded/decoded bytes per resource | 64 MiB |
| Total encoded/decoded presentation bytes | 512 MiB |
| Manifest entries | 65,536 |
| Shader vertex or fragment source bytes | 262,144 |
| Shader source bytes total | 524,288 |
| SPIR-V bytes per stage (cache) | 1,048,576 |
| Variant keywords per shader | 4 |
| Variants per shader (`2^keywordCount`) | 16 |
| Parameters per shader | 32 |
| Bindings per shader | 16 |
| Texture parameters per material | 8 |
| Host extensions per shader | 8 |
| Dependencies per parameterized material | 9 |
| Portable dependency depth | 2 |
| Diagnostics per operation | 1,024 |

`decodedByteCount` 规范值：

```text
Shader = 48 header bytes
  + entry/profile/keyword/parameter/binding/extension string bytes with uint32 lengths
  + vertex source bytes + fragment source bytes
Parameterized Material = 24 fixed bytes
  + shader ref (37 bytes + AssetId)
  + keyword strings
  + per numeric/int/bool parameter 20 bytes + name
  + per texture parameter 8 bytes + name + texture ref
```

Session total 仍不使用 `sizeof(std::vector)` 或 allocator capacity。

## 9. Profiles 与 capability（S5A-09）

Toolchain 拥有的稳定 ID：

```text
cuexis.importer.shader.v1
cuexis.target.spirv.v1
cuexis.target.glsl330.v1
cuexis.target.glsles300.v1
cuexis.renderer.portable.v1
cuexis.renderer.builtin.v1
```

`cuexis.importer.shader.v1` 固定：GLSL 450 → SPIR-V（Vulkan 1.1 / SPIR-V 1.3 语义）→
SPIRV-Tools validate → SPIRV-Cross reflect 与 GLSL 330 Core / GLSL ES 300；优化级别为零，
以保证跨平台 SPIR-V 稳定。实际工具版本写入缓存键，不写入 ShaderAsset identity。

Playback 内容能力（`PlaybackCapabilitySet`）：

```text
cuexis.shader.asset.v1
cuexis.material.parameterized.v1
```

闭包含 Shader kind 时需要前者；含 ParameterizedMaterial 时两者都需要。S5-G 已把它们写入默认
`allCapabilities()`；显式裁剪 Session 缺少时诊断 `playback.capability.unsupported`。

`PresentationCapabilities` version 1 仍只表示 Portable Unlit。version 2 增加 Built-in 字段，
默认全 false / 0：

```text
builtInRendererProfileVersion     0 = none, 1 = builtin.v1
parameterizedMaterial
shaderGlsl450Source
shaderSpirv
shaderGlsl330
shaderGlslEs300
declaredVariants
maxShaderSourceBytes
maxSpirvBytes
maxVariantKeywords
maxVariantsPerShader
maxMaterialParameters
maxTextureBindings
hostExtensionIds                  owning ASCII IDs, sorted unique
```

Portable-only adapter 保持 version 1。ParameterizedMaterial 候选在 version < 2 或
`parameterizedMaterial == false` 或 profile/host extension 不匹配时失败，诊断
`playback.presentation.capability.required_missing`。不存在项目级静默降级字段。

`PresentationRequest` version 2 增加：

```text
enableShaderCompile     default false; requires shader-tools at the adapter/application
enableShaderHotReload   default false; optional like Debug
```

生产 Player 默认不在首帧或 `update()` 同步编译。`enableShaderCompile` 只允许 importer 之外的
显式 prepare 路径（Studio/开发用）。headless Validation Sink 忽略这两项。

`EffectivePresentationSettings` version 2 报告实际 `shaderCompileEnabled` 与
`shaderHotReloadEnabled`。Request 开启但 adapter 不支持时：compile 是错误；hot reload 是
warning `playback.presentation.shader_hot_reload_unavailable` 且 effective false。

## 10. 编译、缓存与热重载（S5A-10）

编译只发生在 `cuexis_shader` 与 `cuexis_asset_importer`，或 Player 在
`enableShaderCompile` 下的 Worker。Playback `prepare`/`update`/`extractFrame` 不得调用编译器。

缓存键按以下顺序做 SHA-256（各字段 length-prefixed）：

```text
"cuexis.shader.cache.v1\0"
source shader semantic identity
importer profile ID
sorted target profile IDs
sorted selected keywords
vertex entry
fragment entry
tool name/version tuples (shaderc, glslang, spirv-tools, spirv-cross)
```

缓存值包含 SPIR-V、GLSL 330、GLSL ES 300、规范化 reflection 与创建它的 profile identity。
缓存可删除重建。修改 profile 或工具版本只失效键匹配的条目。

Reflection 必须与 ShaderAsset 声明的 parameter/binding 匹配。未声明资源、重复 `(set,binding)`、
跨 stage 类型冲突、缺少保留 `CuexisObject` 的生成失败，均为导入错误。

OpenGL texture unit 与 UBO 映射只存在于 `cuexis_render_opengl`。

热重载（仅 Player 证明）：

```text
Worker compiles a candidate cache
owner-thread Render safe point performs a noexcept swap
failure keeps the previous Pipeline and reports full diagnostics
```

源资产字节变化会改变 semantic identity，必须走既有 Playback `prepareReload`。仅缓存重建且
源 identity 不变时，不改 Playback identity 或 FrameDigest。

禁止运行时任意字符串宏和未声明 keyword 组合。

## 11. 模块与 package 拓扑（S5A-12 的模块部分）

```text
cuexis_shader          optional STATIC; vcpkg feature shader-tools
  -> cuexis_core
  optional -> cuexis_assets / cuexis_project as needed for paths
  -X-> cuexis_playback public headers, SDL, OpenGL, JSON DOM

cuexis_playback
  parses CXPRES kinds 4/5 and owns PortableShader / PortableParameterizedMaterial
  -X-> shaderc, SPIRV-Cross, SDL, OpenGL

cuexis_render_opengl
  consumes owning portable values plus CXSCCH01 cache
  maps set/binding to texture units / UBO
  -X-> writes GLuint back to Material/Chart/Playback

cuexis_asset_importer  developer tool; uses cuexis_shader; not a Playback hot path
```

`cuexis_shader` 不是安装 package component。基础 `Cuexis::Playback` 继续不查找 SDL、OpenGL、
GLAD 或 shaderc。CMake 选项建议名 `CUEXIS_BUILD_SHADER_TOOLS`，默认随 headless/Playback-only
为 OFF。S5-B 记录 baseline 上真实的 imported target 名称。

Architecture tests 必须拒绝：`engine/shader/` 包含 SDL/glad/GL/playback 公共头/nlohmann；
`engine/playback/` 包含 shaderc/spirv/glad/GL。

## 12. Diagnostics（S5A-11）

Portable v1 已有 presentation code 保持含义。Stage 5 新增 code 不得复用旧 code 表达不同失败。

| Code | Meaning |
| --- | --- |
| `playback.presentation.shader.source_encoding_invalid` | BOM, CR, or non-UTF-8 shader source |
| `playback.presentation.shader.stage_unsupported` | Stage other than vertex/fragment |
| `playback.presentation.shader.subset_invalid` | Portable GLSL subset violation or `#include` |
| `playback.presentation.shader.entry_invalid` | Entry name empty, too long, or not ASCII identifier |
| `playback.presentation.shader.keyword_invalid` | Duplicate, too many, or illegal keyword |
| `playback.presentation.shader.schema_invalid` | Parameter/binding name, type, set/binding illegal |
| `playback.presentation.shader.reserved_binding` | User binding occupies set 0 binding 0 |
| `playback.presentation.shader.profile_unsupported` | requiredRendererProfile is not a frozen ID |
| `playback.presentation.material.shader_reference_invalid` | Shader AssetId/type/identity missing or wrong kind |
| `playback.presentation.material.parameter_mismatch` | Name/type/count/keyword set does not match schema |
| `playback.presentation.material.keyword_undeclared` | Selected keyword not declared by the shader |
| `playback.capability.unsupported` | Existing code; also used for shader/parameterized content |
| `shader.compile.failed` | Compiler/validator/cross failure with tool log in message |
| `shader.reflect.mismatch` | SPIR-V reflection does not match declared schema |
| `shader.cache.missing` | Built-in adapter prepare requires cache and it is absent |
| `shader.cache.key_invalid` | Cache envelope/version/key does not match |
| `shader.cache.tool_mismatch` | Cache tool versions do not match current importer profile |
| `shader.hot_reload.failed` | Candidate compile failed; previous pipeline kept |
| `shader.diagnostics.limit_exceeded` | Compile/import diagnostic sentinel |
| `playback.presentation.shader_hot_reload_unavailable` | Warning; request asked for hot reload, adapter cannot |

资源错误至少提供 `asset_id` 与 `resource_type`；binary 错误增加 `byte_offset`；budget 错误
增加 `limit` 与 `actual`；capability 错误增加 `capability`；compile 错误增加 `stage` 与
`tool`。到达 1,024 条诊断时停止接收，最后一项换为对应 sentinel。

## 13. SDK、Digest 与 package（S5A-12）

```text
CUEXIS_SDK_API_VERSION: 0.7.0            public types landed in S5-C
FrameDigest algorithm: remains 3
Portable Presentation Profile: 1 (unchanged)
Built-in Renderer Profile: 1
Asset Index: 3 adds shader
CXPRES payloadVersion for kinds 4/5: 1
Cache envelope CXSCCH01 version: 1
Presentation capability/request/effective: version 2 is additive
```

`0.7.0` 是 preview minor：新增公开类型与 `validatePresentation` 可观察字段。不给现有
aggregate 在中间插入字段；新 capability 字段接在 version 1 布局之后，默认零值。

FrameDigest v3 已包含 material type、AssetId 与 32 identity bytes。ParameterizedMaterial 的
identity 递归包含 shader 与纹理，因此不需要 digest v4。v1/v2 digest golden 保持不变。

Package：Portable/Parameterized 公开 API 仍属于 `Playback` component。Validation Sink 不安装。
OpenGL adapter 与 `cuexis_shader` 均不安装。稳定 C ABI 仍属 Stage 12。

## 14. Implementation status

S5-A 已冻结本文。S5-B 已接线可选编译器。S5-C 已实现 `CXPRES01` kind 4/5 Reader、Asset Index
v3 `shader`、公开 capability 常量和 SDK API `0.7.0`。S5-D 已实现 `cuexis.importer.shader.v1`
编译、校验、反射与 GLSL 330 / ES 300。S5-E 已落地 toolchain / renderer profile ID、
`PresentationCapabilities` version 2 与 parse → playback capability → presentation capability
预检；headless 不要求 Built-in Renderer Profile。S5-F 已实现 `CXSCCH01` Writer/Reader、规范
缓存键、importer 写出与失败保留上一 Pipeline。S5-G 已把 `cuexis.shader.asset.v1` 与
`cuexis.material.parameterized.v1` 写入默认 `allCapabilities()`；OpenGL 消费 `CXSCCH01`
GLSL 330、`CuexisObject` UBO 与 set/binding → texture unit；Unlit 内联 GLSL 330 未改；
Validation Sink 校验 kind 4/5 identity/schema 且 Portable-only 拒绝 Parameterized；裁剪
Session 仍拒绝且不破坏 active Unlit。S5-H 尚未开始。完成证据只写入阶段报告。当前产品状态以
[CURRENT_STATUS.md](../CURRENT_STATUS.md) 为准。

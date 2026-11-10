# 表现资源与能力预检

状态：active

更新日期：2026-08-30

适用版本：SDK API `0.7.0`

文档角色：公共 API 参考

权威头文件：[presentation.hpp](../../engine/playback/include/cuexis/playback/presentation.hpp)、
[playback_session.hpp](../../engine/playback/include/cuexis/playback/playback_session.hpp)

## 快速结论

1. `PreparedPlayback` 持有 candidate presentation，Session 持有 active presentation。
2. renderer 先读取 manifest，再执行 capability validation，最后获取资源。
3. `commit` 同时提交播放内容与 presentation candidate。
4. 资源返回为 `shared_ptr<const PortableResource>`，调用方不能修改。
5. Playback 热路径不调用 shader compiler。

## 标准流程

1. 从 `PreparedPlayback::presentationManifest` 读取 candidate manifest。
2. 构造 `PresentationCapabilities` 和 `PresentationRequest`。
3. 调用 `validatePresentation`。
4. 只有 `PresentationValidationResult::hasValue()` 为真时才继续。
5. 通过 `acquirePresentationResource` 获取需要上传或缓存的资源。
6. adapter 准备完成后调用 Session `commit`。

失败时不提交 candidate，既有 active presentation 保持不变。

## 资源类型速查

| `PresentationResourceType` | 公共值类型 | 主要内容 |
| --- | --- | --- |
| `Mesh` | `PortableMesh` | position、UV、index 和 bounds。 |
| `Texture2D` | `PortableTexture2D` | RGBA8 pixels、尺寸和 color space。 |
| `UnlitMaterial` | `PortableUnlitMaterial` | base color、alpha mode、double-sided、texture。 |
| `Shader` | `PortableShader` | source、entry、variant、parameter schema、binding 和 profile 要求。 |
| `ParameterizedMaterial` | `PortableParameterizedMaterial` | shader ref、keyword 和 parameter values。 |

`PresentationResourceRef` 由 type、asset ID 和 `PresentationContentIdentity` 构成。manifest entry 还包含
encoded/decoded byte count 与依赖列表。

## 能力预检速查

| 能力组 | 代表字段 |
| --- | --- |
| pass 与混合 | opaque、transparent、straight alpha、debug pass |
| texture | linear、sRGB、最大尺寸 |
| mesh | 最大 vertex/index 数量 |
| 内存预算 | 单资源与总 decoded bytes |
| shader profile | GLSL 450、SPIR-V、GLSL 330、GLSL ES 300 |
| material | parameterized material、variant、parameter、texture binding 上限 |
| host extension | `hostExtensionIds` |

`PresentationRequest` 表达本次请求的 debug、shader compile 和 hot reload 选项；validation 返回
`EffectivePresentationSettings` 与 diagnostics。

## Stage 5 边界

- 默认 Playback capability 集合包含 `cuexis.shader.asset.v1` 和
  `cuexis.material.parameterized.v1`。
- 显式裁剪 Session 可以稳定拒绝 shader 或 parameterized material。
- 缺少缓存且未 opt-in compile 时，adapter 返回稳定诊断，不在 Playback 热路径隐式编译。
- OpenGL、SPIR-V 工具和 shader compiler 不成为 Playback 的传递依赖。

## 失败与边界

- capability、budget、profile 或 extension 不满足时，在资源上传前失败。
- candidate token 不能跨 Session 混用，也不能由宿主构造。
- acquisition 失败不得部分替换 active presentation。
- renderer adapter 不能把 GPU handle 写回公共 portable resource。

相关内容：[Material/Shader v1](../formats/MATERIAL_SHADER.md)、
[Portable Presentation v1](../formats/PORTABLE_PRESENTATION.md)、[诊断规则](diagnostics-identity-and-compatibility.md)。

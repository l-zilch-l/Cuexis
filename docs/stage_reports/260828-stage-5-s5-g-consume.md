# Stage 5 S5-G Consume and Public Capability

状态：S5-G local checkpoint；S5-H 尚未开始；Stage 4 已关闭

报告日期：2026-08-28

权威范围：[Stage 5 实施计划](../stage_plans/stage_5_implementation_plan.md) §7.7。
权威合同：[ADR 0040](../adr/0040-stage-5-material-shader-contracts.md) 与
[MATERIAL_SHADER.md](../formats/MATERIAL_SHADER.md)。Portable Unlit v1 仍以
[PORTABLE_PRESENTATION.md](../formats/PORTABLE_PRESENTATION.md) 为准。

## 1. 结论

S5-G 已把 `cuexis.shader.asset.v1` 与 `cuexis.material.parameterized.v1` 写入默认
`allCapabilities()`。默认 Session 接受合法 Built-in Shader 内容。显式裁剪 Session 仍以
`playback.capability.preflight_failed` / `playback.capability.unsupported` 拒绝 Parameterized
CXC，且不破坏已提交的 active Unlit。OpenGL Unlit 继续走内联 GLSL 330，规范化摘要合同不变。
Parameterized 对象绑定 `CXSCCH01` 的 GLSL 330、`CuexisObject` std140 UBO（144 字节）与
set/binding → texture unit 映射（跳过保留 `(0,0)`）。GLuint 不写回 Playback 类型。Validation
Sink 校验 kind 4/5 identity、schema、dependency 与失败诊断，不读 SPIR-V，不安装为 SDK
component；Portable-only（version 1 / `parameterizedMaterial == false`）对 Parameterized 失败。
HostOverride `MaterialTint` / `MaterialOpacity` 进入 FrameSnapshot 后由 CuexisObject 与 Sink
`effectiveColor` 观察，不经过第二套 Runtime。Playback 热路径仍不链接 shaderc。SDK API 保持
`0.7.0`。CFU-F3 fingerprint 不含 capability ID，本批次不改 golden。

## 2. 交付

```text
default allCapabilities() += cuexis.shader.asset.v1, cuexis.material.parameterized.v1
trimmed Session still rejects Parameterized; active Unlit preserved on prepareReload fail
OpenGL Unlit: existing inline GLSL 330 program, uniforms, golden/digest unchanged
OpenGL Parameterized: CXSCCH01 load by Playback 32-byte CXPRES identity
  compile GLSL 330 with GL (not shaderc); CuexisObject UBO binding 0
  Texture2D set/binding -> sequential texture units, skip (0,0)
missing cache + no opt-in compile -> shader.cache.missing
opt-in compile only if CUEXIS_HAS_SHADER_TOOLS (PresentationRequest.enableShaderCompile)
adapter-private cache dir: OpenGlBackend::setShaderCacheDirectory
Player --shader-cache-dir DIR
builtInPresentationCapabilities(maxTextureDimension, debugPass) is GPU-free
  parameterizedMaterial/shaderGlsl330/declaredVariants true
  shaderGlsl450Source/shaderSpirv true only with CUEXIS_HAS_SHADER_TOOLS
cuexis_shader_cache always built when OpenGL or shader-tools is on
  sources: shader_cache.cpp + shader_reflection.cpp, core only
cuexis_shader remains shader-tools-only (compiler + ShaderPipelineCache)
OpenGL PRIVATE cuexis::shader_cache; optional PRIVATE cuexis::shader
Playback never includes cuexis/shader or shaderc
Validation Sink kind 4/5 identity/schema/deps; frame path accepts ParameterizedMaterial
  effectiveColor = (1,1,1,1) * snapshot tint/opacity
Portable-only v1 still fails at validatePresentation
Player transaction: prepare -> acquire -> adapter prepare -> commit -> activate
  commit fail still discardPresentation
SDK API remains 0.7.0
```

## 3. 非范围

S5-H 安全/分配/hosted 关闭、Studio、Judgement、公共 CXC API、Vulkan、Shader Graph、运行时脚本。
不改 OpenGL Unlit 内联 GLSL 330。不把 GLuint 写回 Playback。不把 cache 目录放进
PresentationRequest / ProjectConfig。

## 4. 下一步

S5-H：spec 预算、warmed 分配合同、performance probe、Debug/Release/`--fresh` 全量门禁与 hosted
三平台验收。关闭不表示 Studio、Judgement、公共 CXC API 或 Shader Graph 已开始。

# Stage 5 S5-E Profile and Presentation Capability

状态：S5-E local checkpoint；S5-F 尚未开始；默认 Playback 仍只保证 Unlit v1

报告日期：2026-08-28

权威范围：[Stage 5 实施计划](../stage_plans/stage_5_implementation_plan.md) §7.5。
权威合同：[ADR 0040](../adr/0040-stage-5-material-shader-contracts.md) 与
[MATERIAL_SHADER.md](../formats/MATERIAL_SHADER.md) §9。Portable Unlit v1 仍以
[PORTABLE_PRESENTATION.md](../formats/PORTABLE_PRESENTATION.md) 为准。

## 1. 结论

S5-E 已落地冻结的 toolchain / renderer profile ID，以及 `PresentationCapabilities` /
`PresentationRequest` / `EffectivePresentationSettings` version 2 的 additive 字段。
Parameterized 候选的 presentation preflight 顺序为 parse → playback capability → presentation
capability。version 1 adapter 或 `parameterizedMaterial == false` 以
`playback.presentation.capability.required_missing` 失败。headless 不要求 Built-in Renderer
Profile。Unlit version 1 validate 路径保持不变。ProjectConfig 仍为 v1。默认
`allCapabilities()` 仍不含 `cuexis.shader.asset.v1` / `cuexis.material.parameterized.v1`。

## 2. 交付

```text
public profile IDs in cuexis/playback/presentation.hpp
duplicated IDs in cuexis/shader/shader_diagnostics.hpp (no Playback include)
cuexis.importer.shader.v1
cuexis.target.spirv.v1 / glsl330.v1 / glsles300.v1
cuexis.renderer.portable.v1 / builtin.v1
PresentationCapabilities version 2 fields appended after version 1 layout
PresentationRequest version 2: enableShaderCompile / enableShaderHotReload
EffectivePresentationSettings version 2: shaderCompileEnabled / shaderHotReloadEnabled
Parameterized + version < 2 or parameterizedMaterial false
  -> playback.presentation.capability.required_missing
shader limit fields below spec budget
  -> playback.presentation.capability.limit_insufficient
hostExtensionIds must cover ShaderAsset.requiredHostExtensions
OpenGL reports version 2 + builtin.v1, parameterizedMaterial false, empty hostExtensionIds
Validation Sink Unlit path remains version 1
enableShaderCompile without adapter compile flags -> required_missing shader_compile
enableShaderHotReload without compile flags
  -> warning playback.presentation.shader_hot_reload_unavailable, effective false
SDK API remains 0.7.0
ProjectConfig remains v1; no compile flags in cuexis.project.json
```

Cuexis OpenGL 不发明宿主 extension ID。headless Parameterized 验证使用 version 2、
`parameterizedMaterial == true`、`builtInRendererProfileVersion == 0`、compile/hot-reload false。

## 3. 本地验证

2026-08-28 MSVC Debug（`CUEXIS_BUILD_SHADER_TOOLS=OFF`）：

```text
cmake --build --preset debug succeeded
cuexis_format_check passed
python -B tools/check_docs.py passed (167 Markdown / 20 JSON-CXT)
ctest --preset debug --no-tests=error  464/464
cuexis_architecture_tests passed
S5-E Catch2 cases:
  Frozen toolchain profile IDs match the Stage 5 contract
  Parameterized preflight order is parse, playback capability, then presentation
  Host extension IDs must be advertised by presentation capabilities
Unlit Validation Sink version 1 path unchanged; additive version 2 request accepted
external find_package / add_subdirectory consumers passed
```

S5-D 的 461 项加上本批 3 个 S5-E 用例为 464。default `allCapabilities()` 回归与 S5-C kind 4/5
parse 测试继续通过。

## 4. 明确未做

- 未写出 `CXSCCH01` 缓存
- 未把 `cuexis.shader.asset.v1` / `cuexis.material.parameterized.v1` 写入默认 `allCapabilities()`
- 未改 OpenGL 内联 GLSL 330 Unlit
- 未让 Playback `prepare` / `update` / `extractFrame` 调用编译器
- 未把编译标志写入 `cuexis.project.json`
- 未声称 owner 已关闭整个 Stage 5

## 5. 下一步

S5-F：实现 `CXSCCH01` Writer/Reader、规范缓存键，以及失败保留上一 Pipeline。默认 Session 仍必须
只保证 Unlit v1。

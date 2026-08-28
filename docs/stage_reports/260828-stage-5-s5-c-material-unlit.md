# Stage 5 S5-C Versioned Material and Unlit Coexistence

状态：S5-C local checkpoint；S5-D 尚未开始；默认 Playback 仍只保证 Unlit v1

报告日期：2026-08-28

权威范围：[Stage 5 实施计划](../stage_plans/stage_5_implementation_plan.md) §7.3。
权威合同：[ADR 0040](../adr/0040-stage-5-material-shader-contracts.md) 与
[MATERIAL_SHADER.md](../formats/MATERIAL_SHADER.md)。Portable Unlit v1 仍以
[PORTABLE_PRESENTATION.md](../formats/PORTABLE_PRESENTATION.md) 为准。

## 1. 结论

S5-C 已把 `CXPRES01` Shader=4 与 ParameterizedMaterial=5 接到 Playback 公开类型和 headless
Reader，并把 SDK API 升到 `0.7.0`。默认 `allCapabilities()` 仍不含
`cuexis.shader.asset.v1` / `cuexis.material.parameterized.v1`，因此合法 Parameterized fixture
在默认 Session 以 `playback.capability.unsupported` 失败。本批未调用 shaderc，未替换 OpenGL
内联 Unlit shader。

## 2. 交付

```text
PresentationResourceType Shader=4 ParameterizedMaterial=5
PortableShader / PortableParameterizedMaterial at the end of PortableResourceValue
SDK API 0.7.0 (SameMinorVersion; 0.6 and 0.8 rejected)
Asset Index v3 type shader; v1/v2 still reject
assets::AssetType::Shader and ResourceManager loadShader
CXPRES01 kind 4/5 Reader: LF UTF-8, #version 450, no #include, identity, budgets
public capabilityShaderAssetV1 / capabilityMaterialParameterizedV1
not in allCapabilities(); second preflight after preparePresentation
ObjectSnapshot.material allows Unlit or Parameterized; Shader is dependency-only
OpenGL adapter refuses kind 4/5 with render.opengl.presentation.resource_type_invalid
external playback_consumer visits kinds 4/5
schema cuexis.asset-index.v3.schema.json
```

Identity 输入遵循 spec：Shader 哈希 entry、排序 keyword、schema、binding、默认 RenderState、
profile、排序 host extension 和 LF 源码，不含 SPIR-V/cache。ParameterizedMaterial 递归包含
shader 与纹理 identity。

## 3. 本地验证

2026-08-28 MSVC Debug（`CUEXIS_BUILD_SHADER_TOOLS=OFF`）：

```text
cmake --build --preset debug succeeded
ctest --preset debug  461/461 passed (S5-B was 452; +9 S5-C tests)
python -B tools/check_docs.py passed
cuexis_format_check passed
cuexis_architecture_tests passed
cuexis_external_consumer_find_package* passed at SDK 0.7.0
cuexis_playback_tests.exe dependents have no shaderc / SPIRV-* / OpenGL
default Session parameterized reject: playback.capability.preflight_failed
opt-in parse/identity, texture identity, and CR encoding tests pass
Unlit presentation and Validation Sink tests remain
```

## 4. 明确未做

- 未调用 shaderc / SPIRV-Tools / SPIRV-Cross
- 未实现 `CXSCCH01` 缓存或 ImporterProfile / ShaderTargetProfile
- 未把 shader/parameterized capability 写入默认 `allCapabilities()`
- 未改 OpenGL 内联 GLSL 330 Unlit
- 未让 Validation Sink 消费 kind 4/5 帧（identity helper 已 additive；validate 仍拒绝）
- 未声称 owner 已关闭整个 Stage 5

## 5. 下一步

S5-D：在 `CUEXIS_BUILD_SHADER_TOOLS=ON` 时实现 GLSL 450 → SPIR-V → GLSL 330 / ES 300 与
规范化 reflection。Playback 热路径仍不得链接或调用编译器。

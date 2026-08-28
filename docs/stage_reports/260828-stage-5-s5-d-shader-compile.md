# Stage 5 S5-D ShaderAsset Compile and Reflection

状态：S5-D local checkpoint；S5-E 尚未开始；默认 Playback 仍只保证 Unlit v1

报告日期：2026-08-28

权威范围：[Stage 5 实施计划](../stage_plans/stage_5_implementation_plan.md) §7.4。
权威合同：[ADR 0040](../adr/0040-stage-5-material-shader-contracts.md) 与
[MATERIAL_SHADER.md](../formats/MATERIAL_SHADER.md)。Portable Unlit v1 仍以
[PORTABLE_PRESENTATION.md](../formats/PORTABLE_PRESENTATION.md) 为准。

## 1. 结论

S5-D 已在可选 `cuexis_shader` 中实现 `cuexis.importer.shader.v1`：shaderc 将 GLSL 450 编译为
SPIR-V（Vulkan 1.1 / SPIR-V 1.3，优化级别 0，无 debug），SPIRV-Tools 以 `SPV_ENV_VULKAN_1_1`
校验，SPIRV-Cross 反射并生成 GLSL 330 Core 与 GLSL ES 300。规范化 reflection 按 name 排序，
不含 texture unit。`cuexis_asset_importer --compile` 读取 GLSL 源并打印 SPIR-V / reflection
摘要，不写出 `CXSCCH01`。Playback 热路径仍不链接或调用编译器。

## 2. 交付

```text
ShaderCompiler::compile implements cuexis.importer.shader.v1
shaderc: Vulkan 1.1, SPIR-V 1.3, opt 0, filenames vertex.glsl / fragment.glsl
selected keywords only as #define NAME 1
SPIRV-Tools validate SPV_ENV_VULKAN_1_1
SPIRV-Cross GLSL 330 Core and GLSL ES 300
CuexisObject required at set 0 binding 0 in both stages
reflection bindings/parameters sorted by name; no texture unit field
canonical encoding cuexis.shader.reflection.v1
frozen codes: shader.compile.failed / shader.reflect.mismatch
plus playback.presentation.shader.{keyword_invalid,subset_invalid,reserved_binding,schema_invalid}
cuexis_asset_importer --compile --vertex --fragment [--keyword] [--binding]
no CXSCCH01 write
engine/shader does not parse JSON/CXC/CXT or include Playback headers
```

同一 LF 源连续两次编译得到相同 SPIR-V 字节与 reflection canonical encoding。跨 Windows/Linux
的位级 SPIR-V 身份依赖 opt 0 与固定输入文件名；本批在 Windows MSVC 上验证了进程内确定性。

## 3. 本地验证

2026-08-28 MSVC `debug-shader-tools`（`CUEXIS_BUILD_SHADER_TOOLS=ON`）：

```text
cmake --build --preset debug-shader-tools --target
    cuexis_shader cuexis_shader_tests cuexis_asset_importer cuexis_format_check succeeded
cuexis_shader_tests.exe  10 cases / 47 assertions passed
    sprite golden identity, keyword variant, duplicate binding,
    undeclared keyword, #include, reserved (0,0), missing CuexisObject,
    undeclared sampler, shaderc failure
cuexis_asset_importer_tool_tests passed (--help 0, no-arg 2,
    --compile CuexisObject-only ok, #include -> subset_invalid)
cuexis_architecture_tests passed
cuexis_format_check passed
cuexis_player LINK_LIBRARIES has no cuexis_shader / shaderc / SPIRV-* / glslang
cuexis_playback_tests.exe dependents have no shaderc / SPIRV-* / OpenGL
```

2026-08-28 MSVC Debug（`CUEXIS_BUILD_SHADER_TOOLS=OFF`）：

```text
ctest --preset debug  461/461 (shader compile tests are not part of this preset)
no cuexis_shader_tests.exe / cuexis_asset_importer.exe in out/build/debug/bin
```

## 4. 明确未做

- 未实现 ImporterProfile / ShaderTargetProfile 或 `PresentationCapabilities` version 2
- 未写出 `CXSCCH01` 缓存
- 未把 `cuexis.shader.asset.v1` / `cuexis.material.parameterized.v1` 写入默认 `allCapabilities()`
- 未改 OpenGL 内联 GLSL 330 Unlit
- 未让 Playback `prepare` / `update` / `extractFrame` 调用编译器
- 未声称 owner 已关闭整个 Stage 5

## 5. 下一步

S5-E：落地 toolchain / renderer profile ID 与 `PresentationCapabilities` version 2。默认
Session 仍必须只保证 Unlit v1。

# Stage 5 S5-B Shader Tools Wiring

状态：S5-B local checkpoint；S5-C 尚未开始；默认 Playback 仍只保证 Unlit v1

报告日期：2026-08-28

权威范围：[Stage 5 实施计划](../../../stage_plans/completed/stage-05/plan.md) §7.2。
权威合同：[ADR 0040](../../../adr/0040-stage-5-material-shader-contracts.md) 与
[MATERIAL_SHADER.md](../../../formats/MATERIAL_SHADER.md)。

## 1. 结论

S5-B 已把可选 Shader 编译器接到内部静态库，且没有打开 kind 4/5 解析、没有替换 OpenGL 内联
Unlit shader、没有改默认 `allCapabilities()`。默认 Debug 与 headless 仍不下载 shaderc。

## 2. 交付

```text
vcpkg feature shader-tools (not a default feature)
CUEXIS_BUILD_SHADER_TOOLS option default OFF
preset debug-shader-tools
STATIC cuexis_shader (cuexis::core + unofficial::shaderc::shaderc
    + spirv-cross-core/glsl/reflect + SPIRV-Tools-static)
frozen codes shader.compile.failed / shader.reflect.mismatch /
    shader.diagnostics.limit_exceeded (maxDiagnostics 1024)
ShaderCompiler::compile returns shader.compile.failed until S5-D
cuexis_shader_tests Catch2 target
cuexis_asset_importer --help exit 0; no-arg exit 2
architecture bans: engine/shader vs SDL/GL/JSON/Playback/OpenGL adapter;
    playback/runtime vs shaderc/spirv/glslang/cuexis/shader
```

Baseline 未升级。解析版本：shaderc 2026.2、SPIRV-Tools 1.4.350.1、SPIRV-Cross 1.4.350.1、
传递 glslang 16.4.0。许可证已记入 `THIRD_PARTY_NOTICES.md`。`cuexis_shader` 不在安装
`CuexisTargets` / `CuexisConfig.cmake` 中。

## 3. 本地验证

2026-08-28 MSVC Debug（`CUEXIS_BUILD_SHADER_TOOLS=OFF`）：

```text
vcpkg install list has no shaderc / spirv-tools / spirv-cross / glslang
cuexis_shader.lib / cuexis_shader_tests.exe / cuexis_asset_importer.exe absent
ctest --preset debug  452/452 passed
cuexis_architecture_tests passed
```

2026-08-28 MSVC headless-debug（adapter off, feature `tests` only）：

```text
vcpkg install list is Catch2, EnTT, GLM, JSON, minizip-ng, tl-expected
no shaderc / SDL / glad / spdlog
cmake --build --preset headless-debug succeeded
ctest --preset headless-debug  417 tests; one nested package rebuild hit
    MSVC C1090 PDB lock and passed on retry
no cuexis_shader / cuexis_asset_importer targets
```

2026-08-28 MSVC `debug-shader-tools`：

```text
Cuexis shader tools: unofficial::shaderc::shaderc, spirv-cross-core,
    spirv-cross-glsl, spirv-cross-reflect, SPIRV-Tools-static
cuexis_shader.lib linked
cuexis_shader_tests.exe linked against shaderc
cuexis_asset_importer.exe linked against cuexis_shader and shaderc
cuexis_player.exe LINK_LIBRARIES has no cuexis_shader / shaderc / SPIRV-*
ctest -R "shader|asset_importer"  3/3 passed
cuexis_architecture_tests passed
importer --help exit 0; no arguments exit 2
```

`presentation.hpp` 与 `open_gl_presentation.cpp` 无 diff。

## 4. 明确未做

- 未解析 `CXPRES01` kind 4/5
- 未改 Asset Index、SDK API `0.6.0` 或 FrameDigest v3
- 未把 `cuexis.shader.asset.v1` / `cuexis.material.parameterized.v1` 写入默认 capability
- 未实现 GLSL 450 编译、反射或 `CXSCCH01` 缓存写入
- 未把 `cuexis_shader` 做成安装 package component
- 未声称 owner 已关闭整个 Stage 5

## 5. 下一步

S5-C：additive 扩展公开 Presentation 类型与 Asset Index v3 `shader`，并把 SDK API 升到
`0.7.0`。默认 Session 必须继续拒绝 Parameterized 内容。

# Cuexis Shader Pipeline 规范

状态：superseded as field contract；retained as design-input history

更新日期：2026-08-28

字段、预算和诊断码以 [MATERIAL_SHADER.md](../../formats/MATERIAL_SHADER.md) 与
[ADR 0040](../../adr/0040-stage-5-material-shader-contracts.md) 为准。本文保留 SPIR-V 中心管线
的历史讨论，不再作为生产 spec。排期以 [Stage 5 plan](../../stage_plans/stage_5_implementation_plan.md)
为准。

## 规范输入与中间表示

ShaderAsset 使用受约束的 GLSL 450 源码、入口、阶段、属性 Schema、显式 set/binding 和 RenderState。规范中间表示为 SPIR-V。

```text
GLSL 450 portable subset
  -> shaderc / glslang
  -> SPIR-V
  -> SPIRV-Cross reflection and target generation
```

目标：

```text
Vulkan       SPIR-V
OpenGL       GLSL 330 Core
OpenGL ES    GLSL ES 300
```

业务层不直接编写目标后端变体。平台差异通过受控宏和 importer profile 处理。

## ShaderAsset

```text
stable AssetId
source stages and entry points
declared variant keywords
property schema
resource set/binding declarations
render state
portable profile requirements
```

Material 保存属性值和资源 Handle，不保存 uniform location、texture unit、GLuint 或 VkDescriptorSet。

## Reflection 与 Binding

Reflection 从 SPIR-V 产生统一描述。源码声明的 property 与反射资源必须匹配；未声明资源、重复 binding、类型变化和跨 stage 冲突均为导入错误。

OpenGL 后端负责把 set/binding 映射到 texture unit、uniform block 等实现；映射不进入 Material 或 Chart。

## Variant

Variant 只能来自 ShaderAsset 声明的有限 keyword 集。缓存键至少包含：

```text
source content hash
include dependency hash
compiler and tool versions
target profile
entry point
sorted keyword set
```

禁止运行时任意组合未声明宏。Importer 应报告可能的 variant 数量，超过预算时失败或要求减少 keyword。

## 编译与缓存

Player 使用导入缓存，不在首帧同步编译生产 Shader。Studio 热重载可以在 Worker 编译，成功后于 Render safe point 替换 Pipeline；失败保留上一有效 Shader/Pipeline 并显示完整诊断。

缓存是派生产物，可删除重建，不是源资产。缓存格式必须带工具版本和 target profile。

## Portable Subset

Android 移动端验证启动前，所有移动目标 Shader 必须在 CI 或验证工具中同时编译为 GLSL 330、GLSL ES 300 和 SPIR-V。无法跨目标表达的功能必须通过明确 capability/variant 隔离，不能依赖 SPIRV-Cross 偶然生成可用代码。

SDK 宿主集成把表现能力分为：

```text
Portable Presentation Profile  跨宿主最低材质/属性契约
Built-in Renderer Profile       Player/Studio 内建后端支持的高级 Shader
Host-specific Extension         特定宿主 adapter 显式声明的能力
```

不能承诺任意 ShaderAsset 自动转换为 Unity、Unreal 或其他宿主材质。Project/Chart 声明的 profile 与宿主 capability 不兼容时必须产生稳定诊断或按显式策略降级；运行时不得静默替换 Shader、重写项目内容或复用不兼容缓存。

headless Playback（阶段 1E）必须在无 GPU、无 Shader 编译的情况下完成谱面加载、更新、Seek 和帧快照提取；Shader 管线只在宿主选择内建 Renderer Profile 时激活。

## Shader Graph

未来 Shader Graph 输出同一规范 GLSL/SPIR-V 管线，不建立第二套后端编译和反射系统。

## 第三方工具

优先使用 shaderc/glslang、SPIRV-Tools 和 SPIRV-Cross。它们封装在资产导入工具和 Shader 编译模块中，版本固定于 vcpkg baseline，并进入 THIRD_PARTY_NOTICES。

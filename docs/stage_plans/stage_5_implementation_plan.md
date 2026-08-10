# Stage 5 Implementation Plan: Material and Shader Pipeline

状态：future；未开始，等待 Stage 4 稳定表现输入

更新日期：2026-08-10

归档来源：[旧版 PROJECT_GUIDE](../archive/PROJECT_GUIDE_LEGACY_2026-08-10.md) 与
[SDK transition plan 快照](../archive/CUEXIS_SDK_TRANSITION_PLAN_2026-08-10.md)。设计输入见
[Shader Pipeline proposal](../proposals/deferred/SHADER_PIPELINE.md) 和 ADR 0021。

## 1. 阶段目标

在 Portable Presentation v1 之上建立可由 Studio、Player 和宿主 adapter 消费的版本化材质与
Shader 工作流，并明确 Portable、Built-in Renderer 和 Host-specific 三层能力。Shader Graph
不属于本阶段。

## 2. Stage 5A：材质资产与参数

- 定义版本化 MaterialAsset、MaterialHandle、参数 Schema 和 RenderState。
- 实现材质参数上传、默认 Shader 引用和运行时预览。
- 定义跨宿主最低 Portable Material Schema。
- 保持 Chart/FrameSnapshot 使用稳定资源身份，不暴露 uniform location、texture unit 或后端对象。

## 3. Stage 5B：ShaderAsset 与跨目标编译

- 接入经依赖政策批准的 shaderc/glslang、SPIRV-Tools 和 SPIRV-Cross。
- 实现 GLSL 450 到 SPIR-V、GLSL 330 和 GLSL ES 300 的受控管线。
- 实现 Reflection、声明式 Variant、Binding 和属性 Schema 校验。
- 定义版本化 ImporterProfile 与 ShaderTargetProfile；ProjectConfig 只引用 profile ID。
- 允许 Built-in Renderer 和 Host-specific Extension 显式声明高级能力。

## 4. Stage 5C：缓存、诊断与热重载

- 定义包含源码、依赖、工具版本、profile、目标能力和 keyword 集的规范缓存键。
- 实现导入缓存、完整编译诊断和失败时保留上一有效 Pipeline。
- 禁止运行时任意字符串宏覆盖和未声明 Variant。
- 实现 Worker 编译与 Render safe point 的原子 Pipeline 替换。

## 5. 验收标准

- 修改合法材质参数可在 Player/Studio 中实时预览。
- Shader 编译或热重载失败不会崩溃，也不会替换上一有效 Pipeline。
- ShaderAsset、MaterialAsset 和公共 Playback API 不绑定 OpenGL/Vulkan 专有类型。
- 目标 Shader 同时通过 GLSL 330、GLSL ES 300 和 SPIR-V 验证。
- 材质可被 Chart/Entity 引用，并向 Studio Inspector 提供稳定 Reflection 数据。
- 宿主 adapter 必须显式报告 capability；系统不承诺自动转换任意 ShaderAsset。
- profile 缺失、版本不支持或能力不兼容时稳定失败，不触发运行时隐式重导入。
- 修改 profile 只失效受影响缓存，且派生资产记录规范化 profile identity。
- headless Playback 不要求 GPU、Shader 编译器或内建渲染 adapter。

## 6. 明确不包含

- Shader Graph、任意运行时代码生成和未声明宏组合。
- 把宿主 Shader API 或图形后端对象暴露到 Chart、Material 或 Playback 公共头。

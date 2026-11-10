# ADR 0021：以 SPIR-V 为中心的 Shader 管线

日期：2026-07-17

状态：已接受

## 背景

项目需要 OpenGL、OpenGL ES 和未来 Vulkan，同时支持反射、热重载和材质属性。

## 决策

使用受约束 GLSL 450，经 shaderc/glslang 编译为 SPIR-V，再用 SPIRV-Cross 反射并生成 GLSL 330 Core/GLSL ES 300；Vulkan 直接使用 SPIR-V。Binding、Variant 和缓存键显式定义。

Studio 编译失败保留上一有效 Pipeline。Shader Graph 未来复用相同管线。

## 备选方案

分别手写桌面、ES 和 Vulkan Shader 会长期漂移；只以 OpenGL uniform 接口抽象无法表达 Vulkan，因此不采用。

## 影响

vcpkg 增加固定版本 Shader 工具，资产导入与 CI 验证所有目标 Profile。

## 后续风险

跨编译不能保证任意 GLSL 可移植，必须维护 portable subset 和 capability 测试。

## SDK 转型补充（2026-07-20）

Shader 能力分为 Portable Presentation、Built-in Renderer 和 Host-specific Extension。SPIR-V 中心管线继续服务内建后端与资产工具，但外部宿主不承诺直接消费任意 SPIR-V/GLSL。宿主 adapter 必须显式报告 capability；不兼容时稳定失败或按项目声明的策略降级。

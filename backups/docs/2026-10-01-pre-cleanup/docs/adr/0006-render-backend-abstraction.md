# ADR 0006：分离渲染前端与后端

日期：2026-07-17

状态：已接受

## 背景

首个后端是 OpenGL，但长期需要 OpenGL ES 和 Vulkan 验证。业务层直接使用 OpenGL 会阻塞后端替换。

## 决策

World 经 RenderSystem 提取 RenderScene 和 RenderCommand，RenderBackend 执行图形 API。OpenGL 只存在于 `cuexis_render_opengl`；Component、Material 和 Chart 不保存 GLuint 等后端对象。

## 备选方案

业务代码直接调用 OpenGL实现较快但形成不可迁移状态机耦合；第一阶段直接开发 Vulkan 会使维护成本翻倍，均不采用。

## 影响

阶段 0 建立最小渲染前端和 OpenGL 后端，完整 Pipeline 和 RenderPass 按阶段演进。

## 后续风险

抽象可能只是 OpenGL 接口换名。延期的阶段 10 必须用 Vulkan 原型验证其表达能力。

## SDK 转型补充（2026-07-20）

ADR 0027 在 RenderScene/RenderCommand 与具体后端之间增加宿主可消费的 FrameSnapshot/RenderPacket 边界。OpenGL 是 Player/Studio 的可选内建 adapter；headless Playback 和宿主渲染不得依赖它。阶段 3 优先验证 portable presentation 和宿主 Sink，阶段 10 的 Vulkan 只验证内建后端，不改变 SDK 帧契约。

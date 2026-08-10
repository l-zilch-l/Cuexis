# Stage 10 Implementation Plan: Optional Vulkan Adapter Validation

状态：deferred；可选，未排入当前实施序列

更新日期：2026-08-10

归档来源：[旧版 PROJECT_GUIDE](../archive/PROJECT_GUIDE_LEGACY_2026-08-10.md) 与
[SDK transition plan 快照](../archive/CUEXIS_SDK_TRANSITION_PLAN_2026-08-10.md)。

## 1. 阶段目标

验证可选 Cuexis 内建 Vulkan 渲染 adapter 是否值得产品化，并识别现有 RenderBackend 抽象中的
缺口。本阶段不改变 Playback SDK、Chart、Behavior、World、Judgement 或 FrameSnapshot 合同。

## 2. 恢复条件

- 存在独立 Player、Studio 或真实宿主的 Vulkan 需求。
- Stage 5 的 ShaderAsset 到 SPIR-V 路径和 profile 合同已经稳定。
- 项目所有者接受 Vulkan ADR 评审与独立 adapter 预算。

## 3. 实施范围

- 审查 RenderBackend、PipelineDesc、BindingSet 和资源生命周期抽象。
- 验证 ShaderAsset 到 SPIR-V 和派生缓存路径。
- 在隔离的实验 LaunchOptions/RenderConfig 中验证 auto/opengl/vulkan 请求。
- 实现 capability 检查、显式回退策略和 EffectiveSettings。
- 必要时实现最小 VulkanBackend 原型并绘制基础 Mesh。
- 记录 OpenGL 假抽象、接口缺口、Context/Surface 所有权和部署风险。
- 形成独立 Vulkan ADR；接受前不把后端选项写入正式 ProjectConfig。

## 4. 验收标准

- 明确列出需要调整的前端/后端接口和 OpenGL 假抽象。
- Chart、Behavior、World、FrameSnapshot 和 Judgement 不需要修改。
- adapter 只消费现有 portable FrameSnapshot/presentation 合同。
- 切换后端只影响应用/宿主 adapter、Render 后端和派生 Shader 缓存。
- 请求的后端不可用时产生稳定诊断；是否回退完全由显式配置决定。
- OpenGL 与 Vulkan 对 Portable Profile 场景产生等价的规范化表现摘要。
- Vulkan public/implementation types 不进入 Playback、Chart 或 Render 前端公共合同。
- 最终形成接受、延期或拒绝 Vulkan 产品化的 ADR 结论。

## 5. 明确不包含

- 把 Vulkan 设为默认后端或替换宿主渲染器。
- 为 Vulkan 修改项目内容或建立第二套 Shader/Material 格式。

# Stage 12 Implementation Plan: Scale, Desktop Performance and Target Platforms

状态：future；未开始

更新日期：2026-10-01

归档来源：[旧 Stage 9A 性能计划](../../historical-inputs/stage-09a/plan.md)、
[Android 历史输入](../../historical-inputs/stage-09b/plan.md)、
[Vulkan 历史输入](../../historical-inputs/stage-10/plan.md) 和
[旧 Stage 11 规模化计划](../stage-11/plan.md)。

前置：

```text
Stage 7A Gameplay Foundation and selected 7B+ capabilities
Stage 8 Chart v5 / CXT v2 / Packed Chart
Stage 9 Presentation Foundation
Stage 10 Studio workflows needed by the measured scenarios
Stage 11 Chart v7 geometry contract for any measured presentation profile
```

本阶段合并原 Stage 11 的规模化目标、旧 Stage 9A 桌面性能计划、Stage 9B Android 计划和旧
Stage 10 Vulkan 计划。它验证既有合同在真实内容、目标设备和可选后端上的可用性，不重新定义
Chart、Input、Judgement、Replay 或 FrameSnapshot 语义。

## 1. 阶段目标

验证 Chart、Playback、Gameplay 和 Presentation 合同在大规模内容、桌面设备、Android 以及
可选 Vulkan adapter 上的可用性，形成可复现的性能、预算、生命周期和平台能力证据。

## 2. 工作批次

### S12-A：桌面性能与大谱面

基础实时架构与正式命名迁移归[Stage 7-RPA子阶段](../../active/stage-07/plan-rpa.md)。
S12-A在已验收的时间/所有权/发布合同上做规模与平台优化，RPA不预先接受本批数值阈值。

- 测量 Packed/CXC load、CXT expansion、prepare 峰值、Runtime 内存、Judgement query、Replay、
  FrameSnapshot、Seek/Reload、AudioClock、CPU/GPU frame time 和资源上传。
- 覆盖 40,000 semantic entities、Pattern 高低重复、复杂资源闭包、动画和 Judgement requirements。
- 形成版本化 DesktopDeviceProfile，区分硬预算、软目标、用户偏好、EffectiveSettings 和降级原因。
- 统计关闭后不得改变 FrameSnapshot、JudgementResult 或 Replay。

### S12-B：Android SDK 与宿主适配

- 验证 Android Playback SDK、headless、可选 OpenGL ES 3.0 adapter、APK/AAB、AssetManager、
  后台恢复、Context 丢失、内存压力和真实设备音频/输入链路。
- 为 KTX2/Basis Universal、meshoptimizer、Ogg/Vorbis 和 GLSL ES 300 定义 target profile；派生资源
  记录 source hash、importer、profile 和压缩参数。
- 只验证原始输入时间戳和延迟链路；InputProfile/CalibrationProfile 只有在另行批准后持久化。

### S12-C：Vulkan 可选 adapter

- 审查 RenderBackend、PipelineDesc、BindingSet、资源生命周期和 SPIR-V/cache 路径。
- 在隔离的 LaunchOptions/RenderConfig 中验证显式 auto/opengl/vulkan 请求、capability、回退和
  EffectiveSettings。
- 如有真实需求，实现最小 Vulkan adapter；不向 Playback、Chart、Judgement 或 FrameSnapshot 暴露 Vulkan 类型。
- OpenGL/Vulkan 对同一 portable profile 产出等价的规范化表现摘要。

## 3. 验收标准

- 目标桌面、Android 和 Vulkan 矩阵有可复现真实测量；环境和设备限制明确记录。
- 40,000 实体下加载、展开、判定、采样和渲染预算分别有硬门禁或明确降级。
- Android 生命周期、资源派生、音频、触摸时间戳和内存压力有设备证据。
- Vulkan 形成接受、延期或拒绝产品化的 ADR；不可用后端稳定诊断，回退只由显式策略决定。
- static/shared、external consumer、headless 和目标平台门禁通过，owner acceptance 完整。

## 4. 明确不包含

- 修改 Chart v5/v6/v7 核心语义或为单一设备修改 Judgement。
- 任意运行时脚本、宿主 UI、在线服务、编辑器 ABI 或稳定 C ABI。
- 把 Android、Vulkan 或性能 profile 写入 Chart 内容 identity。

## 5. 交接

交付设备与宿主能力矩阵、预算和降级规则、Input/Judgement/Replay 性能证据、Android/Vulkan
capability 合同以及 Stage 14 所需的真实 consumer 和部署证据。

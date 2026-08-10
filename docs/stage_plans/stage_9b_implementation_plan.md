# Stage 9B Implementation Plan: Android SDK and Host Adapter Validation

状态：deferred；未排入当前实施序列

更新日期：2026-08-10

归档来源：[旧版 PROJECT_GUIDE](../archive/PROJECT_GUIDE_LEGACY_2026-08-10.md) 与
[SDK transition plan 快照](../archive/CUEXIS_SDK_TRANSITION_PLAN_2026-08-10.md)。设计输入见
[Mobile Strategy proposal](../proposals/deferred/MOBILE_STRATEGY.md) 和 ADR 0022。

## 1. 阶段目标

验证 Android Playback SDK 构建、ContentProvider、宿主生命周期和可选内建 adapter。本阶段不
交付完整移动端游戏外壳、商店发布流程或 Cuexis 自有移动端 UI，也不是 Studio、Stage 9A 或
Stage 11 的前置条件。

## 2. 恢复条件

- Stage 5 已冻结 ImporterProfile、ShaderTargetProfile 和移动目标资源边界。
- Stage 6 已建立可复用的 package、配置和 AudioDeviceProfile 路径。
- Stage 9A 已提供设备预算和性能测量方法，或项目所有者明确接受独立移动端测量计划。
- 存在真实 Android 产品或宿主适配需求，并接受独立实施批次与设备矩阵。

## 3. 实施范围

- 建立 Android Playback SDK、headless 和可选 OpenGL ES 3.0 adapter 构建验证。
- 为 KTX2/Basis Universal、meshoptimizer、Ogg Vorbis 和 GLSL ES 300 定义目标 profile。
- 定义只引用兼容目标和预算的版本化 AndroidDeviceProfile。
- 根据 PreflightCapabilities 确定性选择 profile，并复用 AudioDeviceProfile。
- 验证 APK/AAB、AssetManager ContentProvider、后台恢复、Context 丢失和内存压力。
- 验证真实设备上的 CPU/GPU/音频预算与生命周期恢复。
- Stage 11 未完成时只验证原始输入时间戳和延迟链路，不提前持久化 InputProfile 或 CalibrationProfile。

## 4. 验收标准

- AndroidDeviceProfile 可以复现目标约束、预算和 profile 选择。
- ProjectConfig 目标不兼容时稳定失败，用户偏好不能突破硬件上限。
- 宿主不使用内建 renderer/audio 时可以只构建对应 headless/host adapter 组件。
- profile 缺失、损坏或版本不支持时产生稳定诊断，不静默采用未受控预算。
- AssetManager、后台恢复、Context 丢失和内存压力路径具有真实设备证据。
- AudioDeviceProfile 校准与输出设备身份绑定。
- Stage 11 已完成时，InputProfile/CalibrationProfile 复用既有 Schema，不与 Chart offset 混合。
- Playback 公共头不暴露 Android、JNI、OpenGL ES 或宿主引擎类型。

## 5. 明确不包含

- 完整移动端游戏、UI、商店发布和宿主玩法生命周期。
- 在 Stage 11 前冻结触摸绑定或主观判定校准格式。

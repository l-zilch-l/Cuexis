# Stage 9A Implementation Plan: SDK and Host Performance Validation

状态：future；未开始

更新日期：2026-08-10

归档来源：[旧版 PROJECT_GUIDE](../archive/PROJECT_GUIDE_LEGACY_2026-08-10.md) 与
[SDK transition plan 快照](../archive/CUEXIS_SDK_TRANSITION_PLAN_2026-08-10.md)。

## 1. 阶段目标

在桌面 Player、Studio Preview 和外部 consumer 上完成 SDK、表现、资源、音频与时间精度的
测量闭环，为 Stage 11 Judgement/Replay 和后续宿主适配提供可重复的预算依据。

## 2. 测量范围

- PlaybackSession update/extract、FrameSnapshot 大小、复制次数和生命周期成本。
- 多 Session、ContentProvider、宿主回调、资源上传和 shared library 数据交换成本。
- CPU/GPU frame time、Entity、DrawCall、粒子和资源内存。
- 音频 underrun、AudioClock 输出延迟和时钟稳定性。
- 输入事件发生时间到 Timeline 的映射，不以处理帧时间替代事件时间。
- 内建 Player、Studio Preview 与 external consumer 的性能差异。

## 3. DeviceProfile 与预算

- 基于测量定义版本化 DesktopDeviceProfile。
- 建立 CPU、GPU、音频、粒子 Checkpoint 和瞬时上传预算。
- 声明兼容的 ImporterProfile/ShaderTargetProfile ID 或能力约束。
- 区分硬预算、软目标和用户画质偏好。
- 记录 profile 来源、能力探测、请求值、EffectiveSettings 和裁剪原因。
- 宿主能力与 Cuexis 内建 adapter 能力必须分开记录。

## 4. 验收标准

- 性能面板可显示核心指标，并能导出有版本的诊断记录。
- 为目标桌面设备记录可复现的帧时间、内存、音频和输入时间基线。
- 判定时间链路可以使用事件发生时间，并验证与渲染帧调度解耦。
- DesktopDeviceProfile 默认值具有测量证据。
- 超过硬预算时稳定加载失败或执行明确的确定性降级。
- Player、Studio Preview 和兼容宿主可以消费同一 profile。
- profile 版本不支持、能力不匹配或同优先级候选冲突时稳定失败。
- ProjectConfig 的目标 profile 与 DeviceProfile 不兼容时不进行运行时重导入或缓存替换。
- 硬预算、软目标和用户偏好优先级具有边界测试，EffectiveSettings 可解释最终结果。
- 性能采集本身有界，关闭统计后不改变 FrameSnapshot 或判定语义。

## 5. 明确不包含

- 以单一开发机结果替代目标设备矩阵。
- 把设备预算、用户偏好或能力探测写入 Chart 语义。

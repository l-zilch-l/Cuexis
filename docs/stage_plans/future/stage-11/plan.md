# Stage 11 Implementation Plan: Scale, Platforms and Advanced Presentation

状态：future；未开始

更新日期：2026-09-05

归档来源：[旧 Stage 9A 性能计划](../stage-09a/plan.md)、
[Android 设计输入](../../deferred/stage-09b/plan.md)、
[Vulkan 设计输入](../../deferred/stage-10/plan.md) 和
[粒子时间轴提案](../../../proposals/deferred/PARTICLE_TIMELINE.md)。

前置：

```text
Stage 7A Gameplay Foundation
Stage 7B+ capabilities selected for scale/platform validation
Stage 8 Chart v5 / CXT v2 / Packed Chart
Stage 9 Presentation extensions
Stage 10 Studio workflows
```

本阶段不重新定义 Chart、Input、Judgement 或 Presentation 语义，而是验证它们在
大规模内容和更多运行环境中的可用性。

## 1. 阶段目标

```text
40,000 实体可测量运行
Packed Chart / CXT 展开性能
完整 Input -> Judgement 链路性能
桌面设备预算
Android SDK / host validation
可选 Vulkan adapter
高级粒子和后处理
```

## 2. 子批次

### S11-A：桌面性能基线

测量：

```text
Packed Chart load/decode
CXT finite expansion
prepare peak memory
ChartRuntime memory
Judgement query
Input timestamp mapping
FrameSnapshot extraction
Seek / Reload
CXC package load
audio clock stability
CPU/GPU frame time
```

必须区分硬预算、软目标和用户偏好，并形成版本化 DesktopDeviceProfile。

### S11-B：大谱面压力

至少覆盖：

```text
40,000 semantic entities
high repetition Pattern
low repetition Chart
large resource closure
many animation events
many Judgement requirements
worst-case Packed sections
```

性能测试不能只测渲染，也必须测从输入到判定结果的完整链路。

### S11-C：Android

Android 只能消费已有公共合同：

```text
PlaybackSession
FrameSnapshot
InputEvent
JudgementResult
Presentation capability
```

需要验证：

```text
音频时钟
触摸输入
资源派生
内存预算
Packed Chart 解码
CXC closure
安装包和宿主
```

### S11-D：Vulkan

Vulkan 是可选 Presentation Adapter。不得建立第二套 Chart、Judgement 或
FrameSnapshot 求值路径，也不得向公共 SDK 暴露 Vulkan 类型。

### S11-E：高级表现

在性能合同稳定后，可实现：

```text
确定性粒子
后处理实现
模型动画
更复杂的 Presentation Environment
```

粒子必须使用绝对时间、版本化随机种子和有界 Checkpoint/重建，不得引入任意
脚本化发射逻辑。

## 3. 交接

本阶段向 Stage 12 交付：

```text
设备与宿主能力矩阵
性能预算和降级规则
Input/Judgement/Replay 性能证据
Packed/CXC 大内容证据
Android/Vulkan capability 合同
稳定的 FrameSnapshot / JudgementResult 使用证据
```

## 4. 验收标准

- 目标设备矩阵有可复现的真实测量。
- 40,000 实体下，加载、解码、展开、判定、采样和渲染均在约定预算内。
- 超预算时稳定失败或执行明确、可诊断的确定性降级。
- Android 和 Vulkan 不复制第二套语义。
- 粒子和后处理不会改变 Judgement、Score、Replay 或既有 Chart identity。
- 性能采集关闭后不改变 FrameSnapshot、JudgementResult 或 Replay。
- static/shared、external consumer 和目标平台门禁通过。

## 5. 明确不包含

- 修改 Chart v5/CXT v2 的核心语义。
- 修改 Judgement 判定规则以适配单一设备。
- 任意运行时脚本。
- 宿主 UI、在线服务和编辑器 ABI。

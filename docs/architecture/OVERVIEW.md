# Cuexis Architecture Overview

状态：现行架构总览

更新日期：2026-08-10

依据：[ADR 0027](../adr/0027-playback-sdk-product-boundary.md)

## 产品结构

```text
Cuexis Playback SDK
  embeddable chart/content/timing/playback/presentation core

Cuexis Player
  reference application using Playback SDK and optional built-in adapters

Cuexis Studio
  separate authoring application using the same PlaybackSession preview path
```

RuntimeSession、World、EnTT、SDL 和 OpenGL 是内部实现或可选 adapter，不是宿主集成接口。

## 公共宿主边界

宿主通过以下概念工作：

```text
PlaybackSource / ContentProvider
PlaybackSession / PreparedPlayback
RuntimeFrame / SourceClockSample
FrameSnapshot / FrameDigest
portable presentation acquisition
future InputEvent / JudgementResult / ReplayData
```

公共头不得暴露 EnTT、SDL、OpenGL/GLAD、JSON DOM、RuntimeSession、World 或实现日志类型。

## 内容流

```text
Source Project or typed/memory source
  -> ProjectConfig and Asset Index validation
  -> Chart/CXT/resource validation
  -> PreparedPlayback candidate
  -> capability and adapter candidate validation
  -> noexcept commit / move-swap activation
  -> active PlaybackSession
```

Load/reload 失败必须保留旧活动 Session 和 adapter cache。CXC 是候选交换包，不是 Chart 替代品
或 Runtime cache。

## Runtime 流

```text
SourceClockSample
  -> RuntimeTimeline
  -> RuntimeFrame
  -> Behavior / Animation / other systems
  -> PropertyResolver
  -> owning FrameSnapshot
  -> host or built-in adapter
```

FrameSnapshot 是唯一公共帧。Adapter 可以派生内部命令包，但不能建立第二套 Runtime 求值路径。

## 时间与确定性

- Chart 保存 Beat；TimingMap 负责 Beat 与 chartTimeMs 的确定性映射。
- RuntimeFrame 使用绝对目标时间和 discontinuity identity。
- Seek/reload 从目标时间重建，不使用上一帧最终值作为新基线。
- 数组、对象和资源输入顺序不能作为未声明的 tie-break。
- 影响 PreparedPlayback 的参数必须在 prepare 前冻结并进入规范化 identity。

## 属性求值

```text
Initial
-> Behavior
-> Animation Layers
-> HostOverride
-> StudioPreviewOverride
-> PropertyResolver commit
```

宿主、Judgement 和 Studio 不直接修改最终 Component，也不访问 World。

## 脚本边界

运行时脚本和逐帧脚本回调无限期延后。没有已排期阶段、格式字段、extension、capability、字节码、
模块 ABI 或 Playback 执行入口。离线 authoring generator 是独立工具议题，不进入 CXC，也不被
pack、prepare 或 Playback 隐式执行。

## 深入阅读

- [模块边界](MODULE_BOUNDARIES.md)
- [RuntimeSession](../RUNTIME_SESSION.md)
- [配置所有权 ADR](../adr/0024-configuration-ownership-and-staged-formats.md)
- [Chart/Runtime/World ADR](../adr/0007-chart-runtime-world-boundary.md)
- [属性求值 ADR](../adr/0009-property-evaluation-and-conflicts.md)
- [Portable Presentation](../PORTABLE_PRESENTATION.md)

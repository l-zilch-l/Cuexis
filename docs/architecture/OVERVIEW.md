# Cuexis Architecture Overview

状态：现行架构总览

更新日期：2026-10-28

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

Cuexis 对狭义音乐游戏的统一玩法模型见
[音乐游戏玩法抽象模型](GAMEPLAY_ABSTRACTION_MODEL.md)。该模型将核心要求归纳为：
在正确的时间区间内，针对正确的判定位置或输入域，执行正确的动作；画面表现、
输入设备和有限行为效果可以独立组合。

## 公共宿主边界

宿主通过以下概念工作：

```text
PlaybackSource / ContentProvider
PlaybackSession / PreparedPlayback
RuntimeFrame / SourceClockSample
FrameSnapshot / FrameDigest
portable presentation acquisition
candidate Gameplay / execution coordination (explicit opt-in)
```

公共头不得暴露 EnTT、SDL、OpenGL/GLAD、JSON DOM、RuntimeSession、World 或实现日志类型。

当前分支 SDK API 为 `0.9.1` preview，candidate Gameplay / 执行协调入口与普通播放路径显式区分，
不把内部 Judgement 类型作为宿主 API。入口和所有权见 [API 索引](../api/README.md)与
[实时宿主边界](../api/realtime-host-boundary.md)，当前交付/接受状态见
[CURRENT_STATUS](../CURRENT_STATUS.md)。

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

## 执行状态与恢复

当前 candidate 保留单一 kernel / Fold，将不可变准备结果、历史前缀与运行实例的可变状态分开。
phase / fact / matcher 使用分页共享和写时分离；内部封存来源可复用准备与自然恢复锚点。
内置 Host 的 Seek 默认用 owning Replay 来源绑定，canonical 内容摘要及外部导入验证仍保留。
Runtime 自有 Transform 更新使用可信缓存，可变 World 回调使其失效；默认更新路径保留完整验证。

所有权、信任前提、缓存上限及维护约束集中在
[执行状态、恢复与准备路径](execution-state-and-recovery.md)。这些内部设计不改变产品分层，
也不自动构成设备或生产性能接受。

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

这不排除 Chart 层的声明式 Behavior、Animation、Step Track 或判定结果到有限
表现效果的绑定。它们必须具有明确的触发条件、目标、时间范围和预算，并且不能
退化为任意运行时代码。

## 深入阅读

- [模块边界](MODULE_BOUNDARIES.md)
- [Player 应用结构](PLAYER_APPLICATION.md)
- [RuntimeSession](RUNTIME_SESSION.md)
- [执行状态、恢复与准备路径](execution-state-and-recovery.md)
- [配置所有权 ADR](../adr/0024-configuration-ownership-and-staged-formats.md)
- [Chart/Runtime/World ADR](../adr/0007-chart-runtime-world-boundary.md)
- [属性求值 ADR](../adr/0009-property-evaluation-and-conflicts.md)
- [Portable Presentation](../formats/PORTABLE_PRESENTATION.md)
- [音乐游戏玩法抽象模型](GAMEPLAY_ABSTRACTION_MODEL.md)

# 帧、摘要与时间线

状态：active

更新日期：2026-08-30

适用版本：SDK API `0.7.0`

文档角色：公共 API 参考

权威头文件：[playback_session.hpp](../../engine/playback/include/cuexis/playback/playback_session.hpp)、
[frame_digest.hpp](../../engine/playback/include/cuexis/playback/frame_digest.hpp)、
[runtime_timeline.hpp](../../engine/playback/include/cuexis/playback/runtime_timeline.hpp)

## 快速结论

| 合同 | 类型 | 用途 |
| --- | --- | --- |
| 时间输入 | `RuntimeFrame` | 驱动一次 Session 更新。 |
| 帧观察 | `FrameSnapshot` | 宿主和 renderer 读取可播放状态。 |
| 确定性摘要 | `FrameDigest` | 测试跨 source、平台和迁移 parity。 |
| 时钟转换 | `RuntimeTimeline`、`ChartClock` | 构造连续且可表达跳变的 `RuntimeFrame`。 |

这四类合同不可互换。identity 描述 prepared 内容，FrameDigest 描述指定输入下的帧观察结果。

## `RuntimeFrame` 速查

| 字段 | 含义 |
| --- | --- |
| `chartTimeMs` | 当前 chart-local 时间。 |
| `simulationDeltaTimeMs` | 当前模拟帧的增量。 |
| `timeDiscontinuityId` | seek、stop、reset 或 clock jump 的单调标识。 |

时间发生非连续变化时必须提升 discontinuity ID，不能只修改 `chartTimeMs`。

## `FrameSnapshot` 速查

snapshot 是独立值，不包含 entity、World、GLM 或 GPU handle。

| 区域 | 主要内容 |
| --- | --- |
| `objects` | ID、world matrix、visibility、material、mesh/material resource ref、opacity、tint。 |
| `camera` | view/projection matrix、FOV、near/far、pitch/yaw/roll。 |
| clear color | RGBA。 |
| viewport | width 与 height。 |

需要减少分配时使用 `extractFrame(viewport, destination)` 复用 destination。比较两次播放结果时，必须使用
相同 viewport、时间序列和 capability 集合。

## `FrameDigest` 合同

`computeFrameDigest` 以 `RuntimeFrame`、`FrameSnapshot` 和 digest version 为输入。FrameDigest v1-v3、
canonical bytes/order 和合法输入结果是兼容合同。

- 新实现不得改变已有 digest version 的结果。
- digest 用于确定性验证，不替代 snapshot 本身。
- 跨 source parity 应同时比较 semantic identity、关键帧 snapshot 和指定版本 digest。

## 时间线速查

| 类型 | 输入 | 输出/行为 |
| --- | --- | --- |
| `RuntimeTimeline` | audio source-clock sample | 生成 `RuntimeFrame`，维护 timing offset 和 discontinuity ID。 |
| `ChartClock` | chart time 与 simulation delta | 生成 chart-clock frame；`markDiscontinuity` 标记跳变。 |

`PlaybackMode::ChartClock`、`HostClock` 和 `CuexisAudio` 选择 Session 的时钟模式。具体 SDL audio backend
不是 Playback 的传递依赖。

## 失败与边界

- NaN、无穷值、负 viewport 或无效 timing offset 必须通过 Result/diagnostics 拒绝。
- seek/stop 后必须显式表达 discontinuity。
- snapshot 对象顺序和 digest canonical order 不得依赖 unordered traversal。
- 帧 API 不暴露 renderer backend 或内部 World 生命周期。

相关内容：[PlaybackSession 生命周期](playback-session.md)、[Timing Model](../formats/TIMING_MODEL.md)、
[诊断与兼容性](diagnostics-identity-and-compatibility.md)。

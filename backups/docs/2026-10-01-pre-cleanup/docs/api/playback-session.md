# `PlaybackSession` 生命周期

状态：active

更新日期：2026-08-30

适用版本：SDK API `0.7.0`

文档角色：公共 API 参考

权威头文件：[playback_session.hpp](../../engine/playback/include/cuexis/playback/playback_session.hpp)

## 快速结论

| 项目 | 合同 |
| --- | --- |
| 主入口 | `cuexis::playback::PlaybackSession` |
| 内容输入 | `PlaybackSource` 或 Chart JSON 文本 |
| 每帧输入 | `RuntimeFrame` |
| 每帧输出 | `FrameSnapshot` |
| 推荐加载方式 | `prepareLoad` -> 检查候选 -> `commit` |
| 线程要求 | Session 与非空 `PreparedPlayback` 必须留在创建线程 |

## 标准流程

1. 创建 `PlaybackSession`。默认构造使用默认 capability；传入 `PlaybackCapabilitySet` 可建立裁剪 Session。
2. 构造 `PlaybackSource`，调用 `prepareLoad`。
3. 检查 `PreparedPlayback` 的内容信息、semantic identity 和 presentation manifest。
4. 接受候选结果后调用 `commit`。
5. 每个 tick 调用 `update(RuntimeFrame)`。
6. 需要绘制时调用 `extractFrame(FrameViewport)`。
7. 使用 `prepareReload` / `commit` 替换内容，或调用 `unload` 清空 Session。

## API 速查

| 操作 | API | 说明 |
| --- | --- | --- |
| 查询状态 | `state`、`capabilities` | 返回 `core::Result<T>`。 |
| 两阶段加载 | `prepareLoad`、`commit` | 提交前可检查候选内容。 |
| 两阶段重载 | `prepareReload`、`commit` | 失败不改变 active 内容。 |
| 便利加载 | `load`、`loadChart` | 等价于内部 prepare + commit。 |
| 便利重载 | `reload` | 适合不需要检查候选的调用方。 |
| 推进播放 | `update` | 消费一个 `RuntimeFrame`。 |
| 提取帧 | `extractFrame` | 可返回新 snapshot，也可复用 destination。 |
| 查询内容 | `chartInfo`、`contentInfo`、`semanticIdentity` | 读取 active 内容。 |
| 查询诊断 | `diagnostics`、`lastOperationDiagnostics` | 区分 active 状态与最近操作。 |
| 宿主覆盖 | `acquireHostOverride`、`releaseHostOverride` | 以 token 管理临时属性覆盖。 |

## 状态与事务

| `SessionState` | 含义 |
| --- | --- |
| `Empty` | 没有 active 内容。 |
| `Ready` | 内容已提交，可以开始更新。 |
| `Running` | 已执行播放更新。 |
| `Failed` | Session 本身进入不可继续状态；普通 prepare/reload 失败不应破坏 active 内容。 |

`PreparedPlayback` 只表示候选内容。只有 `commit` 会替换 active 状态。prepare 或 reload 失败时，既有
FrameSnapshot、semantic identity、active diagnostics 和 presentation 必须保持不变。

`ReloadPolicy::KeepChartTime` 保持目标 chart time；`RestartAtZero` 从零开始。参数通过
`PlaybackPrepareOptions::parameters` 在 prepare 时冻结，并参与最终语义结果。

## 每帧输入

| `RuntimeFrame` 字段 | 含义 |
| --- | --- |
| `chartTimeMs` | 当前 chart-local 时间。 |
| `simulationDeltaTimeMs` | 本帧模拟增量。 |
| `timeDiscontinuityId` | seek、stop 或时钟跳变后的不连续标识。 |

## 失败与边界

- 所有 `core::Result<T>` 必须显式处理。
- 公共调用不能让异常跨越模块边界。
- 非空 `PreparedPlayback` 不得跨线程移动或离开创建线程析构。
- Session 不暴露 RuntimeSession、World、entity 或 renderer backend。
- 运行时脚本和逐帧 script callback 无限期延后，不存在 Playback hook。

相关内容：[输入来源](sources-and-content.md)、[帧输出](frames-digests-and-timelines.md)、
[诊断规则](diagnostics-identity-and-compatibility.md)。

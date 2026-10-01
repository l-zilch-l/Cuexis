# ADR 0032: Playback Clock and Prepared Audio Transaction

状态：已接受

日期：2026-07-27

## 背景

PlaybackSession 已经消费绝对 `RuntimeFrame`，但阶段 1D 还需要把 HostClock 或 CuexisAudio
位置、Chart offset、Pause、Seek 和 reload 组合为同一帧语义。应用若为发现主音乐再次解析
Chart，或直接取得 PlaybackSession 内部 ResourceManager 的 Handle/Lease，会形成第二条加载
路径并破坏 SDK 边界。音频与 Runtime 分别提交也无法为 reload 提供可验证的失败回滚。

## 决策

### 后端无关 Clock 与 Timeline

`cuexis_audio` 定义后端无关的 `SourceClockSample`：

```text
positionMs       当前 source 时间，有限且非负
state            Stopped / Playing / Paused / Ended / Error
discontinuityId  只比较变化，不要求跨实现使用相同数值
```

AudioClockSnapshot 在此基础上增加 source-domain `presentedFrame`、sample rate 和估算输出延迟。
Host adapter 直接产生 SourceClockSample；CuexisAudio adapter 从 AudioClockSnapshot 产生同一输入。

`cuexis_playback` 可以依赖后端无关的 `cuexis_audio`，并提供 `RuntimeTimeline`：

```text
SourceClockSample + Timing offset
-> chartTimeMs = positionMs - offsetMs
-> RuntimeFrame
```

RuntimeTimeline 为 SourceClock discontinuity、显式 Chart reload 和其他 Timeline 跳转生成 Session
级 discontinuity。首帧、Pause、Stopped 和 discontinuity 后首帧的
`simulationDeltaTimeMs` 为 0。同一 segment 的 Playing 位置必须单调不减。

PlaybackSession 本身仍只消费 RuntimeFrame，不创建、控制或查询 AudioTransport。

### 显式模式

单次播放准备必须选择且冻结以下模式之一：

```text
ChartClock    Chart 不得声明主音乐
HostClock     Chart 必须声明主音乐，宿主负责内容消费和 SourceClockSample
CuexisAudio   Chart 必须声明主音乐，Cuexis SDL adapter 负责播放和 Clock
```

活动 Session 和 reload 不得切换模式。替换 Chart 与已选模式不兼容时，reload 失败并保留旧状态。
任何模式初始化失败都不得静默切换到另一模式。

### Prepared Playback

PlaybackSession 增加 move-only、owner-thread 的 Prepared load/reload 概念。准备阶段完成：

```text
Chart load/compile
RuntimeSession prepare
Snapshot layout prepare
mainMusic AssetId/type/Required source resolution
operation diagnostics
```

Prepared 对象内部持有候选 Runtime 状态和 AudioSourceLease，并公开高层
`PlaybackContentInfo` 与只读 `MainMusicSourceView`。公开对象只提供 AssetId、offset、revision 和
受限字节 view，不暴露 ResourceManager、slot、Handle 或 Lease。同步 decoder 或 Host adapter
消费完成后可以释放 encoded Lease。

Prepared token 绑定创建它的 PlaybackSession generation。跨 Session、重复提交、活动状态已经
变化或 token 过期必须在修改状态前失败。所有校验完成后，commit 只执行无分配、不可失败的
状态交换。旧的便捷 `loadChart` 可以作为无音频 ChartClock 路径的包装，但不能用于绕过 v2
内容准备。

### Host 主音乐契约

HostClock adapter 在 Session owner thread 同步接收 MainMusicSourceView，必须在返回前完成复制、
保留或内容身份确认。它不得保留 view、重入同一 PlaybackSession，或让异常越过边界。准备失败
返回 Result 并且不提交 Session。成功后宿主独立拥有播放设备并持续提交 SourceClockSample。

### Reload 保证

reload 的顺序为：

```text
Prepared Playback replacement
-> 完整读取、解码和注册候选 Clip 或准备 Host replacement
-> 准备 Transport replacement
-> 激活音频 replacement
-> 无失败 Playback commit
-> RuntimeTimeline 发布一次 discontinuity
```

Chart、索引、内容、解码、Store、目标时间和候选 Runtime 等确定性失败必须保留旧音乐、旧
Runtime、旧 Clock 和 discontinuity。物理设备在最终激活时不可恢复地失效属于外部故障；此时
Transport 进入 Error，不能承诺旧设备仍可恢复。文档和验收不得把硬件失效描述为完全可回滚
的软件事务。

`KeepChartTime` 使用新 offset 计算目标 source position。`RestartAtZero` 表示目标
`chartTimeMs = 0`，因此 source position 是新 offset，而不是无条件回到 source frame 0。

## 模块方向

```text
audio       -> core
playback    -> audio + existing playback core dependencies
audio_sdl   -> audio + SDL3
runtime     -X-> audio / SDL
chart       -X-> audio / SDL
playback    -X-> audio_sdl / SDL
```

Playback 的 SessionState 不镜像 Transport 状态。Pause 由 SourceClockSample 和 RuntimeFrame
表达；Playback 不提供 Transport pause/resume。普通音频错误属于应用/adapter，只有内部不可
恢复 invariant 才使 PlaybackSession 进入 Failed。

## 影响

- Player、headless consumer 和 Host adapter 使用同一 RuntimeTimeline 生成 RuntimeFrame。
- mode parity 必须比较相同 SourceClockSample/control script 产生的 RuntimeFrame 和
  FrameSnapshot，而不是先假定两边 RuntimeFrame 已经相同。
- 阶段 1D 增加公开 preview 类型，需要独立 SDK API 版本评审。
- 完整 async prepare、取消和 Worker 解码仍不属于阶段 1D。

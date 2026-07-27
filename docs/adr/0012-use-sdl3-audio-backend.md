# ADR 0012：使用 SDL3 音频后端和采样帧时钟

日期：2026-07-17

状态：已接受

## 背景

Cuexis 需要首个可用的主音乐播放后端，并需要一个不把 SDL 类型传播到 Runtime、Chart 或 Gameplay 的 AudioClock 边界。音游时钟还必须支持暂停、Seek 和设备重建后的确定性恢复。

## 决策

`cuexis_audio` 定义后端无关的 `IAudioTransport`、只读 `IAudioClock` 和 `AudioClockSnapshot`。`cuexis_audio_sdl` 使用 SDL3 Audio Device 与 `SDL_AudioStream` 实现首个后端。

AudioClock 内部以整数采样帧计数，向外提供毫秒位置、估算输出延迟、播放状态和 `discontinuityId`。Seek、Stop、重新加载、设备重建和输出格式重建递增不连续 ID；Runtime 据此从新的 `chartTimeMs` 完整重求值。

AudioClock 只表达音频资源的预计输出位置，不应用 BPM、谱面 offset、用户校准或输入延迟。Timeline 负责从音频位置转换到 `chartTimeMs` 和 beat。

第一版整首预解码 WAV 到内存，实现单主音乐流、播放、暂停、停止、Seek、延迟估算和错误诊断。压缩格式后续通过独立 Decoder 和成熟开源库增加。

SDL 音频实时路径禁止文件 I/O、动态分配、格式化日志、阻塞锁、资源查询、JSON 和 ECS 操作。

## 备选方案

### 使用 SDL_mixer 作为主音乐核心

拒绝。它适合普通游戏混音，但隐藏的播放和缓冲过程不利于建立可诊断、可替换的音游主时钟。

### 首版直接使用平台专用 WASAPI

拒绝。它会过早增加平台代码和维护成本。SDL 后端应先通过实测验证；只有判定精度不足时才增加专用后端。

### 自行实现压缩音频解码

拒绝。OGG、MP3 和 FLAC 已有成熟开源实现，自研不会形成 Cuexis 的核心价值。

### 使用浮点毫秒逐帧累加

拒绝。它会产生累计误差，并使位置与实际提交的采样数据脱离。

## 影响

```text
新增 cuexis_audio_sdl target（可选 adapter，非 Playback SDK 必选依赖）
宿主/Player 组合层选定运行模式：ChartClock、HostClock 或 CuexisAudio（SDL Transport）
Runtime 与 Judgement 不依赖 SDL 或 audio_sdl
阶段 1D 增加 WAV 播放、AudioClock 集成和三模式 RuntimeTimeline 验证
正式判定开发前需要测量参考后端的输出延迟；判定精度不阻碍 HostClock 模式宿主
```

## 后续风险

SDL 不保证所有平台提供精确的硬件播放光标。第一版时钟是经过缓冲延迟修正的估算值；如果实测抖动或偏差不能满足判定需求，需要增加平台专用后端，同时保持 `cuexis_audio` 接口不变。

压缩格式解码库、音效并发策略、设备热插拔恢复和用户校准交互仍需在相应功能进入实现时细化。

## SDK 转型补充（2026-07-20）

SDL Audio 是可选 CuexisAudio adapter，不是 Playback SDK 必选依赖。正式支持 ChartClock、
HostClock 与 CuexisAudio 三种模式：无主音乐 Chart 使用 ChartClock；宿主可以自行播放音乐并
提交 SourceClockSample；Player 使用 SDL Transport。三种来源由同一 RuntimeTimeline 归一化为
RuntimeFrame。HostClock 与 CuexisAudio 对相同 SourceClockSample/control script、InputEvent
和 ResolvedSessionConfig 必须产生相同表现与判定结果。

主音乐 AssetId 的 Required 内容语义继续有效，但“内容存在”与“由谁解码/创建设备”分开。SDL 设备失败只在已选择 SDL 模式时失败，不得影响明确选择 HostClock 的宿主。

## 阶段 1D 细化（2026-07-27）

阶段 1D 按 [ADR 0031](0031-main-music-content-format-v2.md) 和
[ADR 0032](0032-playback-clock-and-prepared-audio-transaction.md) 冻结以下细节。

### AudioClip 与 Store

decoded AudioClip 是 immutable interleaved F32 PCM，只接受 1 或 2 channels、8000 至 192000 Hz。
AudioClipStore 使用 index、generation 和 store token；Handle 是弱引用，Lease 是强所有权。
Store 只在 owner thread 注册和移除 Clip，Lease 可以安全只读并活过 Store，Store 销毁后旧 Handle
失效。阶段 1D 最多同时注册活动 Clip 与一个 reload 候选，总 decoded 上限 512 MiB，单 Clip
上限 256 MiB。

编码 WAV 上限为 64 MiB。decoder 必须先有界扫描 RIFF chunk 和声明尺寸，再执行 PCM/F32 转换和
AudioClip 创建；不能只在已经分配完整输出后检查 decoded 大小。第一版只接受 RIFF/WAVE PCM 与 IEEE F32，
拒绝 ADPCM、压缩 WAV 和 RF64。输出 sample 必须有限且按完整 frame 对齐。

### Config 与状态机

AudioConfig 在任何 SDL 初始化前纯校验。阶段 1D 只请求默认 playback route，target queue 与
low-water 默认 200/100 ms，gain 默认 1.0。Transport 状态为 Empty、Stopped、Playing、
Paused、Ended、Error。load 只从 Empty 进入 Stopped；replacement 使用 Prepared 路径，不把
load 当作隐式 reload。Error 只允许读取快照、unload 和销毁，不自动 reopen 设备。

Seek 使用 source-frame 域，有限毫秒值必须位于闭区间 `[0, durationMs]`，按明确的最近 frame
规则转换且不得 silent clamp。位置实际变化时 discontinuity 改变一次；Pause/Resume 不改变。
自然 EOF 不改变 discontinuity。

### presentedFrame 估算

SDL 没有跨后端统一的硬件播放光标，`SDL_GetAudioStreamQueued()` 只表示尚未转换的输入字节，
不得被单独解释为已经输出的位置。第一版使用 SDL postmix callback 只原子累计 device-domain
frames；owner-thread `service()` 根据 segment 起点、实际 device rate、device buffer frames 和
已提交 source frame 计算估算 presentedFrame，并执行范围 clamp 与 segment 内单调滤波。

callback 只更新预分配的 lock-free 整数原子，不格式化、不分配、不调用宿主或访问资源。
完整 AudioClockSnapshot 由 owner thread 通过 sequence counter 和整数原子字段发布；不得假设
`std::atomic<AudioClockSnapshot>` 对大结构总是 lock-free。

EOF 以估算 presentedFrame 到达 clip frameCount 为准，不能仅以 input queue 归零判断。
Underrun 在数据耗尽后冻结于最后可表示的 source position；补充数据后从同一位置继续，不改变
discontinuity，只累计每次 episode 的预分配计数。

### 默认设备限制

SDL 的 `SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK` 可能在系统默认 route 变化时透明迁移。阶段 1D 的
“不自动恢复”表示 Cuexis 不主动 enumerate、reopen 或选择另一设备；可观察到 device format
变化或 removal 时进入 Error 并改变 discontinuity。SDL 对同格式 route 的透明迁移可能无法从
公共 API 可靠识别，该限制必须进入 EffectiveAudioSettings、人工门禁和完成报告，不能宣称
已经固定物理设备身份。特定设备身份在阶段 6 的 AudioDeviceProfile 中解决。

### Reload 错误边界

Chart、内容、解码、Store、目标位置和候选 Runtime 的准备失败必须保留旧音乐与旧 Runtime。
物理设备在最终激活时失效属于外部不可恢复错误，Transport 进入 Error；不把“旧硬件一定恢复”
作为软件强保证。Playback commit 在音频候选成功激活后只执行已经准备好的无分配状态交换。

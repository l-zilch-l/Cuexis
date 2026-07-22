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
宿主/Player 组合层选定运行模式：HostClock（宿主自备音频并提交 RuntimeFrame）或 CuexisAudio（SDL Transport）
Runtime 与 Judgement 不依赖 SDL 或 audio_sdl
阶段 1D 增加 WAV 播放、AudioClock 集成和 HostClock/CuexisAudio 双模式验证
正式判定开发前需要测量参考后端的输出延迟；判定精度不阻碍 HostClock 模式宿主
```

## 后续风险

SDL 不保证所有平台提供精确的硬件播放光标。第一版时钟是经过缓冲延迟修正的估算值；如果实测抖动或偏差不能满足判定需求，需要增加平台专用后端，同时保持 `cuexis_audio` 接口不变。

压缩格式解码库、音效并发策略、设备热插拔恢复和用户校准交互仍需在相应功能进入实现时细化。

## SDK 转型补充（2026-07-20）

SDL Audio 是可选 CuexisAudio adapter，不是 Playback SDK 必选依赖。正式支持 HostClock 与 CuexisAudio 两种组合模式：宿主可以自行播放音乐并提交归一化 RuntimeFrame；Player 使用 SDL Transport。两种模式对相同 Chart、InputEvent 和 ResolvedSessionConfig 必须产生相同表现与判定结果。

主音乐 AssetId 的 Required 内容语义继续有效，但“内容存在”与“由谁解码/创建设备”分开。SDL 设备失败只在已选择 SDL 模式时失败，不得影响明确选择 HostClock 的宿主。

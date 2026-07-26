# 阶段 1D 实施计划：主音乐内容与可选音频适配器闭环

状态：HostClock/CuexisAudio 双模式方向已接受，格式、诊断轨迹与具体 API 待编码前确认
规划日期：2026-07-18；SDK 调整：2026-07-20；一致性修订：2026-07-27
强制前置：[阶段 1C 实施计划](stage_1c_implementation_plan.md)  
并行 packaging 与最终验收：[阶段 1E 实施计划](stage_1e_implementation_plan.md)
现有音频决策：[ADR 0012](../adr/0012-use-sdl3-audio-backend.md)、[ADR 0027](../adr/0027-playback-sdk-product-boundary.md)

## 1. 阶段目标

阶段 1D 建立正式主音乐资产与时钟闭环：

```text
Chart typed main-music AssetId
-> Asset Index audio source
-> ResourceManager AudioSource Lease
-> bounded WAV decode
-> immutable decoded AudioClip
-> 选择 HostClock 或 CuexisAudio 模式
-> HostClock，或 SDL AudioTransport -> AudioClockSnapshot
-> 宿主/Player Timeline -> RuntimeFrame
-> PlaybackSession -> 1C Behavior absolute resampling
```

第一版 CuexisAudio adapter 只支持整首 WAV 预解码、Windows 默认播放设备、单主音乐流，以及播放、暂停、停止、Seek、Reload 和可诊断的估算输出时钟。HostClock 模式不创建 SDL 设备，由宿主提供绝对时间；两种模式必须归一化为相同 RuntimeFrame 语义。

## 2. 格式与版本推荐

Asset Index v1 已冻结为 Mesh/Material/Texture；Chart v1 顶层也没有主音乐字段。1D 不修改二者的既有语义，推荐新增：

```text
cuexis.asset-index version 2
  在 v1 类型基础上增加 audio

cuexis.chart version 2
  在 v1 内容基础上增加 typed audio block

cuexis.project version 1
  保持不变，entry.chart 仍是唯一 bootstrap locator
```

Chart v2 推荐结构：

```json
"audio": {
  "version": 1,
  "mainMusic": {
    "domain": "asset",
    "id": "audio.main"
  }
}
```

Asset Index v2 推荐记录：

```json
{
  "id": "audio.main",
  "type": "audio",
  "source": "audio/main.wav",
  "dependencies": []
}
```

v1 Reader 和 fixture 必须继续工作且不接受 `audio` 类型；v2 Reader 接受全部 v1 类型和 Audio。加载器不自动迁移、不写回或把 v2 数据伪装为 v1。阶段 1D 默认 Project 使用 Chart v2 + Asset Index v2；阶段 1A canonical/simple v1 继续作为显式无音频 ChartClock 回归入口。

Chart 缺少 `audio` block 表示显式无主音乐。存在 block 时主音乐 Asset/内容解析仍是 Required；CuexisAudio 模式下 WAV 解码或设备失败使 Player/会话失败，HostClock 模式下宿主内容/时钟契约失败使会话失败。已经选择的模式不得静默切换到 ChartClock 或另一模式。

## 3. 音频资产与所有权

编码源和解码结果是不同生命周期：

```text
AssetId
-> assets::AudioSourceHandle + ResourceScope/Lease
-> encoded WAV blob
-> audio_sdl::WavDecoder（非音频实时路径）
-> audio::AudioClipStore
-> AudioClipHandle + AudioClipLease
-> SdlAudioTransport 持有 decoded Lease
```

规则：

- `AudioSourceHandle` 属于 Asset ResourceManager，表示受索引约束的编码文件。
- `AudioClipHandle` 属于 `cuexis_audio`，表示不可变 decoded PCM；两种 Handle 不可互换。
- AudioClipStore 使用 `index + generation + storeToken`，Handle 是弱引用，Lease 是强所有权。
- Store 不读文件、不解析 Asset Index、不控制设备，也不是第二个通用 ResourceManager。
- CuexisAudio adapter 的应用组合负责从 Source Lease 到 decoded Clip 的桥接；解码完成后可释放 encoded Lease。HostClock adapter 通过受控宿主契约交付/确认主音乐内容，不要求创建 AudioClipStore。
- Transport 构造时注入 Store，`load(handle)` 成功后内部持有 Lease，直到 unload/reload/destroy。
- Reload 先完整读取、解码和注册新 Clip；失败时旧音乐、Clock 和 discontinuity 均不改变，成功后一次替换。

如果后续实现选择强所有权 `AudioClipRef` 而不是 Store，必须修改/补充 ADR 0012，不得把 `shared_ptr` 包装后仍称为弱 Handle。

## 4. 模块边界

`cuexis_audio` 从 INTERFACE 升为 STATIC，提供：

```text
AudioConfig / ValidatedAudioConfig
AudioClip / AudioClipStore / AudioClipHandle / AudioClipLease
PlaybackState
AudioClockSnapshot / IAudioClock / IAudioTransport
AudioMetricsSnapshot / EffectiveAudioSettings
```

`cuexis_audio_sdl` 从 INTERFACE 升为 STATIC，提供 Pimpl 封装的：

```text
SdlAudioSubsystem
SdlAudioTransport
WavDecoder
SDL Audio Device / SDL_AudioStream
queue service 与原子 Clock publish
```

依赖方向：

```text
audio       -> core
audio_sdl   -> audio + SDL3
assets      -> core
runtime     -X-> audio / SDL
chart       -X-> audio / SDL
audio_sdl   -X-> platform_sdl
playback    -X-> audio_sdl / SDL
player      -> playback + assets + audio + audio_sdl + platform/render
host        -> playback + optional host audio adapter
```

Audio SDL 子系统由后端自己的 RAII 对象管理；选择该 adapter 的 Player/宿主控制创建/析构顺序，不在 `platform_sdl` 与 `audio_sdl` 之间传递 SDL 对象。PlaybackSession 只接收归一化 RuntimeFrame，不持有 SDL 对象。

## 5. AudioConfig 与 EffectiveSettings

阶段 1D 的 CuexisAudio 模式只提供单一代码默认值和显式内存注入：

```text
deviceRequest      DefaultPlayback（1D 唯一选项）
targetQueueMs      200
refillLowWaterMs   100
gain               1.0
```

校验范围：

```text
40 <= targetQueueMs <= 1000
10 <= refillLowWaterMs < targetQueueMs
gain finite 且 0.0 <= gain <= 1.0
```

所有字段在 `SDL_InitSubSystem` 和设备创建前校验。1D 不暴露或持久化 `SDL_AudioDeviceID`，不接受任意设备字符串。

设备创建后发布只读 `EffectiveAudioSettings`：实际 sample rate、channels、device buffer frames、queue 水位、估算输出延迟和协商差异。它只用于诊断，不写回 ProjectConfig。阶段 6 再由 UserPreferences/AudioDeviceProfile 选择受控设备身份和校准。

## 6. AudioClip 与安全预算

decoded Clip 统一为 immutable interleaved F32 PCM：

```text
sample rate              8000..192000 Hz
channels                 1 或 2
encoded WAV              64 MiB
decoded PCM              256 MiB
frame count              checked int64
diagnostics              1024
```

所有 frame/channel/byte 乘法必须检查溢出，PCM 字节必须整帧对齐，非有限 sample 拒绝。WAV 只从已加载的有界内存解码；文件 I/O、解码、Store 注册和诊断格式化不得进入音频实时路径。

上述值是第一版内存耗尽防护，不是 DeviceProfile 性能预算。阶段 9A 根据测量结果另行建立设备预算。

## 7. Transport 状态机

增加 `Empty`，完整状态为：

```text
Empty, Stopped, Playing, Paused, Ended, Error
```

| 操作 | 成功结果 | discontinuity |
| --- | --- | --- |
| 初始 | `Empty`, frame 0 | 0 |
| load | `Stopped`, frame 0 | 改变一次 |
| play | Stopped/Paused -> Playing | 不变 |
| pause | Playing -> Paused | 不变 |
| seek | Playing 保持 Playing；其他已加载状态变 Paused | 实际位置改变时改变一次 |
| stop | `Stopped`, frame 0 | 实际重置时改变一次 |
| 自然结束 | `Ended`, frame = duration | 不变 |
| unload | `Empty` | 改变一次 |
| 设备/格式失效 | `Error` 或明确重建后的状态 | 改变一次 |

重复 play/pause/stop 是无副作用成功，不重复改变 ID。`seekMs` 必须有限且位于 `[0, durationMs]`，转换到最近 source frame，禁止 silent clamp。`play` 在 Ended 不隐式倒回，调用方必须先 stop 或 seek。

Pause 冻结位置。Underrun 不改变 discontinuity，Clock 在无有效输出数据时冻结；补充数据后从同一位置继续，并只更新预分配计数和限频诊断。不可恢复 SDL 错误进入 Error、冻结最后位置且只发布一次状态变化；1D 不自动换设备。

## 8. AudioClock 语义

`presentedFrame` 定义为 decoded Clip 的 source-frame 域中，估算已经到达输出设备的帧位置：

```text
不是 submitted frame
不是 SDL 设备 sample-rate 域 frame
不是跨 Seek 单调的全局计数
范围为 [0, clip.frameCount]
positionMs = presentedFrame * 1000.0 / clip.sampleRate
```

同一 discontinuity segment 内，Playing 时位置单调不减；Paused/Stopped/Ended/underrun 时按状态冻结；Seek 可以跳变。`estimatedOutputLatencyMs` 只用于诊断和 presented position 估算，不包含 Chart offset、输入延迟或用户校准。

第一版由选择 CuexisAudio adapter 的应用 owner thread 定期调用 `transport.service()`，按 low-water 向 SDL_AudioStream 补充预解码数据。`snapshot()` 只读取预发布的原子快照，不调用 SDL、不分配、不使用阻塞 mutex。若实现 SDL callback，仍必须满足同一实时限制，并先通过专项审查。HostClock 模式不执行本节 SDL service。

实时路径禁止：

```text
文件 I/O、动态分配、格式化日志、阻塞锁
AssetDatabase/ResourceManager 查询、JSON、ECS、设备重建
```

## 9. Timeline 与 1C 集成

宿主或 Player 是 Clock/Timeline 组合层，PlaybackSession 是统一消费门面：

```text
transport.service()
-> AudioClockSnapshot
-> chartTimeMs = positionMs - TimingMap.offsetMs
-> RuntimeFrame{chartTimeMs, simulationDeltaTimeMs, discontinuityId}
-> PlaybackSession::update()
-> internal RuntimeSession::update()
```

HostClock 模式从宿主位置生成相同 RuntimeFrame，跳过 `transport.service()` 和 AudioClockSnapshot。

Pause、Stopped 和 discontinuity 后的首帧使用 `simulationDeltaTimeMs = 0`。Playback/Runtime/Behavior 只接收 RuntimeFrame，不拥有、控制或依赖具体 AudioTransport。

如果 Chart 明确无 `audio` block，Player/宿主可以使用 ChartClock。存在主音乐引用时，Required 内容或已选模式的初始化错误均为启动/播放错误，不允许静默无声运行；HostClock 是显式模式，不是失败 fallback。

## 10. 诊断轨迹与导出

诊断必须区分确定性执行轨迹和设备遥测；两者可以按 `frameIndex` 关联，但不得混为同一
parity 判据：

```text
确定性帧轨迹
frameIndex, chartTimeMs, simulationDeltaTimeMs, discontinuityId, frameHash

设备遥测
frameIndex, wallClockMs, sourcePositionMs, estimatedOutputLatencyMs,
queuedFrames, underrunCount, transportState
```

`frameHash` 只覆盖规范化 `RuntimeFrame` 与后端无关 `FrameSnapshot` 的稳定字段，不包含地址、
容器 capacity、日志时间、GPU 对象或设备遥测。HostClock/CuexisAudio mode parity 只对输入相同
RuntimeFrame 序列后的确定性帧轨迹做逐行比较。`wallClockMs`、latency、queue 和 underrun 受
设备与调度影响，只用于设备趋势、人工 smoke 和问题定位，禁止跨模式逐列相等比较。

音画 drift 使用已发布的 source position 和谱面 offset 计算：

```text
driftMs = chartTimeMs - (sourcePositionMs - offsetMs)
```

`sourcePositionMs` 是 AudioClock 的 presented position，不是 submitted/queued position；
`offsetMs` 来自当前 TimingMap。drift 不得把 `estimatedOutputLatencyMs` 再减一次，也不得混入
输入延迟或用户校准。

轨迹采集归应用/宿主 owner thread 所有。音频实时路径只发布预分配的原子快照和计数，不写
CSV、不格式化、不分配。默认每类轨迹最多 `65,536` 行且最多 `16 MiB`，以先到者为准；达到
上限后停止追加、保留稳定前缀并累计 `droppedRows`，不得覆盖旧行或无界增长。任何自动 parity
门禁要求 `droppedRows == 0`，否则结果为未完成而不是通过。

运行结束后使用 locale-independent、RFC 4180 兼容的离线导出器写出两类 CSV。导出元数据至少
包含 trace schema/version、构建显示版本、SDK API 版本、capturedRows、droppedRows 和
`truncated`；浮点必须用可往返的有限十进制表示。原始设备标识和用户路径不得进入默认导出。

## 11. 实施批次

### 1D-0：ADR 与格式冻结

- 新增 Asset Index v2 / Chart v2 主音乐引用 ADR。
- 扩充 ADR 0012：HostClock/CuexisAudio 模式、AudioClipStore、状态机、presentedFrame、queue、underrun 和恢复。
- 冻结 AudioConfig/EffectiveSettings、宿主 Clock contract 和 Timeline 所有权。

### 1D-1：版本化内容与 Source 资源

- 新增 Asset Index v2、Chart v2 Schema 和 typed Reader，保留全部 v1 Reader/测试。
- AssetDatabase/ResourceManager 增加 AudioSource type、Handle、Lease 与 Required 请求。
- 默认 1D fixture 增加有界、非静音 WAV；不使用未索引路径。

### 1D-2：Audio front-end

- 实现 AudioConfig 纯校验，保证非法输入零 SDL 副作用。
- 实现 immutable AudioClip、Store token/generation、Handle/Lease。
- 实现 Transport 状态机、Clock snapshot、metrics 与 Fake Transport/Clock。
- 实现 HostClock adapter/fake，使其输出与 CuexisAudio 相同的 RuntimeFrame 契约。

### 1D-3：SDL backend

- 从内存 WAV 解码到 F32 PCM，并执行全部大小/格式校验。
- 实现 Audio subsystem/device/stream RAII、queue service、格式协商和 EffectiveSettings。
- 实现 play/pause/stop/seek/unload、EOF、underrun、Error 和安全销毁。

### 1D-4：Playback、Player 与 reload

- 在 Window/GL/Audio device 创建前完成 Project、Index、Chart、Source 和 WAV preflight。
- 组合 HostClock/CuexisAudio -> Timeline -> RuntimeFrame -> PlaybackSession，并驱动 1C Behavior。
- 主音乐 reload 使用 prepare/replace 强保证；失败保留旧音乐和旧 Runtime 状态。
- 保留 v1 canonical/simple 的显式 ChartClock 回归。
- 验证不链接 audio_sdl 的 headless HostClock consumer。

### 1D-5：诊断与验收

- 日志输出请求/实际格式、queue、latency estimate、state、discontinuity 和 underrun count。
- 实现固定容量的确定性帧轨迹与设备遥测采集，并在 owner thread 离线导出两类 CSV。
- mode parity 只比较确定性帧轨迹；设备遥测只用于 drift、趋势和人工 smoke 证据。
- 默认 CTest 只使用 Fake Clock、内存 PCM、损坏 WAV 和 SDL dummy driver，不依赖物理设备或墙钟。
- Debug/Release 分别执行物理音频 smoke 和音画联合 smoke。

### 1D-6：门禁与交付

- 激活 audio/audio_sdl 与测试 targets，更新依赖 allowlist、架构扫描、BUILDING 和专项文档。
- Debug/Release fresh configure + clean build + 完整 CTest + format + architecture。
- 保留 1C/1A GPU 回归，创建 `docs/stage_reports/stage_1d_completion_report.md`。

## 12. 测试矩阵

| 范围 | 必须覆盖 |
| --- | --- |
| Version | v1/v2 路由、v1 拒绝 Audio、v2 类型/引用、无自动迁移 |
| Config | 默认值、NaN/Inf、范围、low-water 关系、非法配置零 SDL 副作用 |
| Store | generation/store token、Lease、跨 Store 拒绝、reload 替换 |
| WAV | 正常、截断、恶意尺寸、格式/声道/采样率、预算与溢出 |
| State | 完整转移表、幂等、Seek 0/末尾/越界、Pause、EOF、Error |
| Clock | source-frame 到 ms、segment 单调性、延迟修正、并发 snapshot |
| Recovery | underrun 冻结/恢复/限频，设备失败，reload 成功/失败强保证 |
| Timeline | offset 符号、Pause delta、Seek discontinuity、1C Transform 重采样 |
| Deterministic trace | hash 字段、Seek/discontinuity、CSV 往返、行/字节上限、droppedRows |
| Device telemetry | drift 公式、presented position、非确定列不参与逐列 parity |
| Mode parity | HostClock/CuexisAudio 对相同 RuntimeFrame 序列的确定性帧轨迹一致 |
| Architecture | audio 无 SDL、playback/runtime/chart 无 audio_sdl、audio_sdl 无 platform_sdl |

## 13. 人工音频门禁

Debug 和 Release 各在 Windows 默认设备执行一次：

```text
非静音 fixture 连续播放且无明显爆音
Pause 至少 2 秒，静音且 Clock 不前进
Resume 连续；Seek 后 ID 只改变一次，画面跳到正确 Behavior 状态
Stop 回到 0；失败 Reload 保留旧音乐
导出确定性帧轨迹和设备遥测；记录格式、buffer、queue、drift、估算延迟和 underrun
确认 capturedRows 符合预期且 droppedRows = 0
```

物理设备和 GPU 不进入默认 CTest。托管 CI 只能证明纯逻辑、dummy backend、构建、格式和架构门禁，不能宣称覆盖真实听感或硬件时钟精度。

## 14. 配置整合

阶段 1D 不新增 UserPreferences、DeviceProfile 或持久化 Audio 配置文件。CuexisAudio 的 `AudioConfig` 只有代码默认值和显式内存注入；HostClock contract 也是当前会话输入。Chart 的 `audio.mainMusic` 是确定性内容引用，不是设备配置。

阶段 6 成为首个持久化 Player UserPreferences / AudioDeviceProfile 消费者，负责设备身份、输出校准、来源追踪和动态/重建设备应用规则。阶段 9A 再根据测量冻结设备预算。

## 15. 明确非目标

```text
特定设备持久化、UserPreferences、AudioDeviceProfile、校准
OGG/MP3/FLAC、流式解码、异步解码和文件热重载
音效混音、空间音频、播放倍率、循环和 gapless
设备热插拔自动恢复、平台专用精确硬件播放光标
正式输入、判定、计分和 Studio 音频编辑
重做已落地的 ContentProvider、SDK install/export 或仓库外 consumer；1D 只扩展 AudioSource
并保持现有门禁
```

## 16. 待确认选择

1. 使用 Asset Index v2 增加 `audio`，使用 Chart v2 增加 `audio.mainMusic`；ProjectConfig v1 不变。
2. 采用 Source Handle/Lease -> AudioClipStore Handle/Lease 的双层派生资源所有权。
3. AudioConfig 1D 只支持默认设备，默认 queue/low-water 为 200/100 ms。
4. decoded PCM 固定 interleaved F32、1/2 channels，64 MiB encoded / 256 MiB decoded。
5. 采用本计划的状态机、source-domain `presentedFrame` 和“已选模式不 silent fallback”规则。
6. 冻结 HostClock/CuexisAudio 的显式模式选择、主音乐内容交付和错误边界。
7. 1D 复用已落地的 ContentProvider、安装包和 external consumer；1E 最终验收以 1D 音频
   契约通过为前置，但 packaging/consumer 工作可以并行推进。
8. 采用本计划的双轨诊断、固定容量、停止追加与 `droppedRows` 失败门禁。

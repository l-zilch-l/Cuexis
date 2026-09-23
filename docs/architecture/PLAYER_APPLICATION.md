# Player 应用结构

状态：现行应用结构

更新日期：2026-09-23

`cuexis_player` 是参考播放器，不是安装进 SDK 的组件。宿主集成走 `PlaybackSession`。Player 自己组合窗口、OpenGL backend、音频设备和帧循环。

正式内容路径是：

```text
PlaybackSource
  -> PlaybackSession::prepareLoad / commit
  -> IPresentationRenderer::prepare / activate
  -> RuntimeTimeline
  -> PlaybackSession::update / extractFrame
  -> IPresentationRenderer::submit / present
```

CuexisAudio 使用后端无关的 `IAudioTransport`。SDL、OpenGL 和 AudioSDL 类型只出现在装配源文件和 smoke 源文件里。

统一命令入口和 `Empty` / `Loaded` / `Playing` / `Paused` / `Stopped` / `Failed` 状态表仍属于 S6-C3。本文记录当前源文件、对象寿命和帧顺序。

## 1. 依赖

```text
cuexis_player_support
  -> cuexis_core / cuexis_json_support

cuexis_player 控制源文件
  -> cuexis_playback / cuexis_presentation_renderer / cuexis_audio / cuexis_player_support

cuexis_player 装配源文件与 smoke 源文件
  -> cuexis_platform_sdl / cuexis_render_opengl / cuexis_audio_sdl
```

`cuexis_player_support` 拥有偏好、音频 profile、配置目录和 session 配置身份。它不进入安装列表。它的源文件不包含 Playback、SDL、OpenGL、AudioSDL 或 nlohmann JSON DOM。Playback 不反向依赖 Player 或 `cuexis_player_support`。

`cuexis_player` 直接链接 Playback、audio、AudioSDL、core、filesystem、platform_sdl、render、render_opengl、player_support 和 spdlog。`IPresentationRenderer` 经 `cuexis_render_opengl` 的公开依赖到达控制源文件，Player 不再为它增加一条直接 CMake 链接。

## 2. 可执行文件入口

`app/player/src/main.cpp` 创建 `PlayerLogger`，调用 `cuexis::player::run`，把 `Result` 错误和逃出的异常写成日志后返回进程退出码。播放逻辑从 `run` 开始。

同一 target 里还有三份支撑文件：

| 源文件 | 职责 |
| --- | --- |
| `player_log.cpp` | 进程日志。OpenGL 配置可以拿走它的 `LogSink` |
| `frame_diagnostics.cpp` | `--frame-stats` 的帧和音频行导出 |
| `snapshot_scene.cpp` | 把 `FrameSnapshot` 的调试轴追加到后端无关的 `RenderScene` |

## 3. 源文件

### 3.1 `player_app.cpp`

组合根。`run` 在这个翻译单元里声明 `PlaybackSession`、准备结果、`AudioClipStore`、SDL runtime、窗口和 `OpenGlBackend`，所以这些对象的析构顺序由这里的声明顺序决定。它调用其他文件，不写帧循环正文。

启动顺序：

1. `parsePlayerOptions`。
2. 解析可执行文件目录。没有 `--chart` 也没有 `--project` 时，`--smoke-test` 使用 `assets/projects/stage3_project`，其他启动使用 `assets/projects/stage1d_project`。
3. 读取偏好和音频 profile。`--smoke-test` 与 `--audio-smoke-test` 使用临时目录 `cuexis-player-smoke-config`，并删除已知的 `preferences.json` 和 `system-default` profile，避免读到本机配置。普通启动使用 `userConfigDirectory`。
4. `preparePlayerContent`。此时 Session 里还没有 commit 后的活动内容。
5. 仅当模式是 `CuexisAudio` 时打开音频设备和 clip。窗口尚未创建。
6. 创建 SDL runtime、窗口、`PlayerSurface` 和 OpenGL backend。可选 `--shader-cache-dir` 在 backend 创建后设置。
7. 记录一次 drawable 尺寸，并写出 effective window 日志。
8. `activatePreparedPlayback`：renderer `prepare`，Playback `commit`，renderer `activate`。
9. 音频 seat 存在时调用 `play`。
10. smoke 或 audio-smoke 时安装 `PlayerHooks`。`--frame-stats` 存在时创建 `FrameDiagnostics`。
11. `runPlayerFrameLoop`。
12. 写出最终音频状态和 frame-stats，然后关闭。

### 3.2 `player_options.cpp`

只解析命令行，不读取工程，也不包含 SDL。

| 选项 | 含义 |
| --- | --- |
| `--smoke-test` | 六帧后退出，并安装表现 smoke hook |
| `--audio-smoke-test` | 九十帧后退出，并安装音频 smoke hook |
| `--chart <path>` | Chart 文本。与 `--project` 互斥 |
| `--project <path>` | 文件系统工程。与 `--chart` 互斥 |
| `--shader-cache-dir <path>` | OpenGL shader cache 目录 |
| `--frame-stats <prefix>` | 帧诊断导出前缀 |

重复选项、缺少路径、未知参数、同时给出 chart 与 project、同时打开两种 smoke，都返回 `player.arguments.*` 错误。

### 3.3 `player_assembly.cpp`

平台工厂和两个窄接口的唯一实现。

- `playerExecutableBase` / `playerProjectDirectory` 使用 SDL 的可执行文件目录，不使用当前工作目录。
- `preparePlayerAudioClip` 从已准备的主音乐字节做 WAV 解码，并注册进 `AudioClipStore`。
- `openPlayerAudio` 在 `CuexisAudio` 以外的模式直接成功返回，不创建设备。`system-default` 走 `SdlAudioTransport::create`。具名 profile 先枚举，再按 driver 与名称唯一匹配，然后 `createForDevice`。随后 `applyGain` 和 `load`。
- `createPlayerRuntime` 初始化 SDL 并记录 video driver。
- `createPlayerWindow` 使用偏好里的宽、高和全屏，窗口可调整、高 DPI，并请求 OpenGL。
- `createPlayerBackend` 按偏好的 vsync 配置 GL context，创建 `OpenGlBackend`，并记录版本、厂商和 renderer 字符串。
- `logEffectiveWindow` 读取一次 drawable 尺寸。日志只报告实际宽高，以及音频设备是否打开。
- `makePlayerSurface` 返回 `SdlPlayerSurface`。`quitRequested` 每帧调用一次 `pollEvents`。
- `makePlayerAudioSeat` 返回 `SdlPlayerAudioSeat`，把设备复核、replacement、gain 和 unload 转到具体 transport。

### 3.4 `player_control.cpp`

内容准备和正式帧循环。这个翻译单元包含 Playback、presentation renderer、后端无关 audio 和 player_support。它不包含 SDL、`platform_sdl`、`render_opengl`、`audio_sdl` 或 GLAD。

`openConfiguredPlaybackSource` 在有工程路径时调用 `PlaybackSource::fromFilesystemProject`，否则按 16 MiB 上限读取 Chart 文本并调用 `fromChartText`。

`preparePlayerContent`：

1. 先按 `ChartClock` 冻结 `ResolvedSessionConfig`。输出校正只在 `CuexisAudio` 时进入这份身份。
2. `prepareLoad`。若失败码是 `playback.mode.content_mismatch`，改用 `CuexisAudio` 重新冻结配置并再准备一次。其他失败原样返回。
3. 记录 session 配置身份。
4. `--audio-smoke-test` 但结果不是 `CuexisAudio` 时返回 `player.audio_smoke_test.audio_required`。
5. 用准备结果里的 `timingOffsetMs` 创建 `RuntimeTimeline` 和 `ChartClock`。这一步不 commit。

`activatePreparedPlayback` 以 debug pass 打开来准备 renderer candidate。Playback `commit` 失败时 `discard` 该 candidate。成功后 `activate`。活动 Chart 的 object count 为 0 时返回 `player.chart.empty`。

`runPlayerFrameLoop` 每帧的顺序：

1. `PlayerSurface::quitRequested`。为真则离开循环。
2. 有音频 seat 时，先跑 `beforeAudioService`，再 `service`、`recheckBoundDevice`、读取 snapshot。配置里的输出校正非 0 时，只改送给 `RuntimeTimeline` 的位置副本，不改 transport snapshot。
3. 没有音频 seat 时，用 `ChartClock` 采样。`--smoke-test` 使用固定时刻 `0, 625, 1250, 1750, 1750, 625` 毫秒。普通播放使用从进程启动起的墙钟。
4. `timeline.advance`。失败立即返回。
5. `afterTimelineAdvance`。hook 可以替换本帧的 `RuntimeFrame`。
6. `session.update`，查询 drawable 尺寸，`renderer.resize`，`extractFrame`。
7. 清空 `RenderScene`，追加 snapshot 调试轴，并在需要时记录 frame diagnostics。
8. `beforeSubmit`。
9. `renderer.submit(snapshot, &scene)`。错误码 `presentation.renderer.surface.zero_size` 时跳过本帧，不增加帧号，也不 present。
10. `renderer.present`。然后 `notePresented`，再 `validatePresented`。
11. 第 0 帧写 snapshot 对象数和 draw 计数。
12. `--smoke-test` 满 6 帧或 `--audio-smoke-test` 满 90 帧后请求退出。提前退出且帧数不足时返回对应的 incomplete 错误。

正式提交使用 `IPresentationRenderer::submit` 和 `present`。控制循环不调用 `renderPresentationFrame`。

### 3.5 `player_smoke.cpp`

`PlayerSmokeBinding` 只在两种 smoke 之一打开时构造。正式播放的 `PlayerHooks` 为空。两种 smoke 不能同时打开。

表现 smoke 安装 `afterTimelineAdvance`、`beforeSubmit`、`notePresented` 和 `validatePresented`：

| 帧 | 行为 |
| --- | --- |
| 0 | 校验不透明中心像素，最小化再恢复，比较 digest 和像素。drawable 仍非 0 时不把这一步当作零尺寸提交 |
| 1 | 先用 `renderPresentationFrame` 比较省略场景、空场景和正式提交的 debug summary，再校验透明纹理像素和 digest |
| 2 | 校验不可见帧没有 presentation draw，中心像素保持清屏色 |
| 3 | 非法 Chart 的 `prepareReload` 必须失败并保留活动 GPU cache。随后拒绝不支持的 presentation 请求和 legacy candidate，且不 commit |
| 4 | 第二个 renderer candidate 必须因 outstanding candidate 被拒绝。第一个 candidate commit 并 activate，timeline 重置后从 chart time 0 再 advance |
| 5 | 再次校验透明帧 digest 和像素 |
| 每帧 | 用 OpenGL `lastPixelProbe` 做像素断言 |

音频 smoke 安装 `beforeAudioService` 和 `afterTimelineAdvance`：

| 帧 | 行为 |
| --- | --- |
| 15 | pause，等待 2 秒，时钟状态、帧号、位置和 discontinuity 必须保持，然后 play |
| 30 | 用 `reverseSeekSourcePositionUs` 把 chart time 500 ms 换回 source，再 `seekMs`。负 source 返回 `player.audio_profile.seek_outside` |
| 45 | `stop` |
| 46 | `play` |
| 55 | 非法替换 Chart 的 `prepareReload` 必须失败，活动内容和音频 snapshot 保持不变 |
| 60 | 准备替换内容、renderer candidate 和音频 replacement，激活音频后再 commit 和 activate。然后 reset timeline，替换 clip，再 advance 一次 |

像素探针和 `renderPresentationFrame` 留在这份文件里，因为它们是 `OpenGlBackend` 的探针，不是 `IPresentationRenderer` 的正式帧入口。

## 4. 窄接口

控制循环不接收 `SdlWindow`、`OpenGlBackend` 或 `SdlAudioTransport`。

`PlayerSurface` 提供两件事：本帧是否请求退出，以及 drawable 的宽高。尺寸为负时控制循环把它夹到 0 再交给 `resize`。

`PlayerAudioSeat` 暴露 `IAudioTransport`，另加四项不在该虚表上的操作：`recheckBoundDevice`、`prepareReplacement`、`activateReplacement`、`applyGain`，以及 `unload`。ChartClock 路径的 seat 指针为空。

`PlayerHooks` 的五个回调点按上面的帧顺序调用。回调返回错误时帧循环停止，并把该错误交给 `run`。

## 5. 关闭顺序

`runPlayerFrameLoop` 返回后，组合根依次：

1. 记录最终音频 snapshot 和 metrics。
2. 导出 frame diagnostics。
3. `OpenGlBackend::close`。这一步释放 GL 资源，不关闭窗口。
4. `PlayerAudioSeat::unload`。
5. 从 `AudioClipStore` 移除活动 clip。
6. `PlaybackSession::unload`。

函数返回后，局部对象按声明的相反顺序析构：帧循环和 smoke binding 先于 backend，backend 先于窗口和 `PlayerSurface`，窗口先于 SDL runtime，音频 seat 先于 transport 和 subsystem，clip store 先于准备内容和 Session。

## 6. 仍留在 S6-C3 的部分

当前没有应用级命令表，也没有把按键或命令行动词译成 load、play、pause、stop、seek、reload。smoke 剧本直接调用 Session、renderer 和音频 seat。`cuexis_player_support` 也还没有命令状态机。

S6-C3 要补的是这张命令表，以及 load、reload、Seek 的允许状态、失败注入和同一控制器上的用户入口。那一批再改命令路径。本文的源文件边界已经把正式帧循环和平台工厂分开，供那一批使用。

## 7. 架构检查

`cmake/VerifyArchitecture.cmake` 读取下列文件。任一文件出现 SDL、`platform_sdl`、`render_opengl`、`audio_sdl` 或 GLAD 头，配置期架构测试失败：

- `app/player/src/player_options.hpp`
- `app/player/src/player_options.cpp`
- `app/player/src/player_surface.hpp`
- `app/player/src/player_audio_seat.hpp`
- `app/player/src/player_control.hpp`
- `app/player/src/player_control.cpp`

`player_app.cpp`、`player_assembly.cpp` 和 `player_smoke.cpp` 可以包含这些平台头。`player_app.cpp` 只负责把具体对象交给窄接口和 smoke binding。

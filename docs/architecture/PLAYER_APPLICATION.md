# Player 应用结构

状态：现行应用结构

更新日期：2026-09-24

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

所有控制面（包括 smoke 剧本）都产生同一个 `PlayerCommand`，交给 `PlayerController::apply`。应用状态是 `Empty` / `Loaded` / `Playing` / `Paused` / `Stopped` / `Failed`，不扩展公共 `playback::SessionState`。命令表在 `cuexis_player_support`，控制器和固定事务顺序在 `player_control.cpp`。

## 1. 依赖

```text
cuexis_player_support
  -> cuexis_core / cuexis_json_support

cuexis_player 控制源文件
  -> cuexis_playback / cuexis_presentation_renderer / cuexis_audio / cuexis_player_support

cuexis_player 装配源文件与 smoke 源文件
  -> cuexis_platform_sdl / cuexis_render_opengl / cuexis_audio_sdl
```

`cuexis_player_support` 拥有偏好、音频 profile、配置目录、session 配置身份和应用命令表。它不进入安装列表。它的源文件不包含 Playback、SDL、OpenGL、AudioSDL 或 nlohmann JSON DOM。Playback 不反向依赖 Player 或 `cuexis_player_support`。

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

组合根。`run` 在这个翻译单元里声明 `AudioClipStore`、`PlayerController`、SDL runtime、窗口和 `OpenGlBackend`，所以这些对象的析构顺序由这里的声明顺序决定。它调用其他文件，不写帧循环正文。

启动顺序：

1. `parsePlayerOptions`。
2. 解析可执行文件目录。没有 `--chart` 也没有 `--project` 时，`--smoke-test` 使用 `assets/projects/stage3_project`，其他启动使用 `assets/projects/stage1d_project`。
3. 读取偏好和音频 profile。`--smoke-test` 与 `--audio-smoke-test` 使用临时目录 `cuexis-player-smoke-config`，并删除已知的 `preferences.json` 和 `system-default` profile，避免读到本机配置。普通启动使用 `userConfigDirectory`。
4. 创建 `AudioClipStore`、SDL runtime、窗口、`PlayerSurface` 和 OpenGL backend。可选 `--shader-cache-dir` 在 backend 创建后设置。
5. 构造 `PlayerControlPorts`：`makeSource` 是 `openConfiguredPlaybackSource`，`prepareClip` 是 `preparePlayerAudioClip`，`openAudio` 是 `makePlayerAudioOpener`。
6. 构造 `PlayerController`，然后 `apply(Load)`。`Load` 是唯一会创建第一个 bundle 的入口；此时 Session 里还没有 commit 后的活动内容。
7. 记录一次 drawable 尺寸，并写出 effective window 日志（音频设备是否打开来自 `controller.audio()`）。
8. `apply(Play)`。
9. smoke 或 audio-smoke 时安装 `PlayerHooks`。`--frame-stats` 存在时创建 `FrameDiagnostics`。
10. `runPlayerFrameLoop`。
11. 写出最终音频状态和 frame-stats，然后 `OpenGlBackend::close` 和 `controller.shutdown()`。

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

平台工厂和三个窄接口的唯一实现。

- `playerExecutableBase` / `playerProjectDirectory` 使用 SDL 的可执行文件目录，不使用当前工作目录。
- `preparePlayerAudioClip` 从已准备的主音乐字节做 WAV 解码，并注册进 `AudioClipStore`。它不激活任何内容。
- `makePlayerAudioOpener` 返回 `PlayerAudioOpener`：输入目标 clip、gain 和起始 source 位置，输出 `PlayerAudioSeat`。空 clip 表示目标没有音轨，返回空 seat，控制层据此丢掉上一个设备。
- `openSeatForProfile` 先建 `SdlAudioSubsystem` 和 `SdlAudioTransport`。`system-default` 走 `SdlAudioTransport::create`。具名 profile 先枚举，再按 driver 与名称唯一匹配，然后 `createForDevice`。随后 `applyGain`、`load`，并在起始位置大于 0 时 `seekMs`。新设备从 source 原点开始，这一步把保留的位置补上；它仍在允许失败的激活阶段内。
- `createPlayerRuntime` 初始化 SDL 并记录 video driver。
- `createPlayerWindow` 使用偏好里的宽、高和全屏，窗口可调整、高 DPI，并请求 OpenGL。
- `createPlayerBackend` 按偏好的 vsync 配置 GL context，创建 `OpenGlBackend`，并记录版本、厂商和 renderer 字符串。
- `logEffectiveWindow` 读取一次 drawable 尺寸。日志只报告实际宽高，以及音频设备是否打开。
- `makePlayerSurface` 返回 `SdlPlayerSurface`。`quitRequested` 每帧调用一次 `pollEvents`。
- `SdlPlayerAudioSeat` 拥有 SDL subsystem 和 transport，把 `recheckBoundDevice`、`prepareReplacement`、`activateReplacement`、`applyGain` 和 `unload` 转到具体 transport。

### 3.4 `cuexis_player_support` 命令表

`player_command.hpp` 只依赖 `cuexis_core`，因此应用状态机可以无 GPU、无窗口、无音频设备地测试。

| 类型 | 内容 |
| --- | --- |
| `PlayerAppState` | `Empty` / `Loaded` / `Playing` / `Paused` / `Stopped` / `Failed` |
| `PlayerCommandKind` | `Load` / `Play` / `Pause` / `Stop` / `Seek` / `Reload` / `Rebuild` |
| `PlayerReloadPolicy` | `KeepChartTime` / `RestartAtZero` |
| `PlayerTransportObservation` | `None` / `Playing` / `Paused` / `Stopped` / `Ended` / `Error` |

`evaluatePlayerCommand` 是纯转换表，返回要发布的状态或稳定拒绝：`player.command.transaction_in_progress`、`player.command.not_loaded`、`player.command.not_playing`、`player.command.failed`、`player.command.duplicate`、`player.command.unknown`。重复命令的上下文带 `duplicate=true`，调用方据此报告“已经处于该状态”而不是非法转换。事务进行中（load、reload、rebuild）时所有命令都被拒绝，因此不存在重入。

`observeTransportState` 把设备观察折进应用状态：`Ended` 只在应用正在播放时停住它，`Error` 进入 `Failed`，其他观察不改变状态，所以没有音轨的 ChartClock 会话不会因为“长度未知”被停掉。

`validateSeekTargetMs` 要求目标有限且非负；负时长表示可播放长度未知，只检查下界；目标不被夹取。

### 3.5 `player_control.cpp`

`PlayerController` 拥有活动 bundle、应用状态和固定事务顺序。这个翻译单元包含 Playback、presentation renderer、后端无关 audio 和 player_support。它不包含 SDL、`platform_sdl`、`render_opengl`、`audio_sdl` 或 GLAD。

`apply` 先查命令表，再按 kind 分派到 `runLoad` / `runReload` / `runRebuild` / `runPlay` / `runPause` / `runStop` / `runSeek`。命令按值接收，因为显式 `PlaybackSource` 只能移动一次。只有成功时才发布 `decision.state`；失败保留原状态，除非该阶段已经显式进入 `Failed`。

命令语义：

- `Load` 是显式替换。它建一个新的 `PlaybackSession`，按显式 mode 或 `ChartClock` 准备，遇到 `playback.mode.content_mismatch` 且来源是配置来源时重新读取并以 `CuexisAudio` 再准备一次。显式来源不能重读，必须自带 mode，否则返回 `player.command.mode_required`。这是切换有/无音轨内容的入口。
- `Reload` 保持活动 `PlaybackMode`，位置由 `PlayerReloadPolicy` 决定。准备阶段出现 `playback.mode.content_mismatch` 时返回 `player.command.mode_change_requires_load`。
- `Rebuild` 重建 renderer 和音频设备，再按 `KeepChartTime` 重载活动来源。它是 `Failed` 的唯一恢复入口：先 `renderer.rebuild()` 失效所有 token，再关闭现有 seat，让事务重新开设备。没有已发布 bundle 时，只有 `Failed` 会退化为一次 load 事务；其他状态返回 `player.command.not_loaded`。
- `Play` / `Pause` / `Stop` / `Seek` 在 CuexisAudio 下转给 transport，在 ChartClock 下改控制器持有的时间。ChartClock 的 `Pause` 冻结上一帧的 chart time，`Play` 从该时间恢复，`Stop` 回到 chart time 0，`Seek` 设置目标；每次都发布一个 discontinuity。暂停中的 `Seek` 合法并保持暂停状态。

`commitBundle` 是固定事务顺序，参数 `restartTimeline` 决定时间是否重来：

1. 校验 prepared 元数据、冻结 timing offset 与 `ResolvedSessionConfig`，并复制/重置 `RuntimeTimeline`。失败不接触任何活动对象。
2. `renderer.prepare` 取候选，然后 `prepareClip` 取新 clip；设备不变时再 `prepareReplacement` 完成同设备流准备。任何失败都 `discard` 候选和已注册 clip，旧内容、旧资源和旧播放不受影响。
3. 预检 `renderer.accepts`。owner、generation 或 token 过期时同样只丢弃候选。
4. 激活音频。设备变化时 `openAudio` 开新设备并把 seat 暂存；设备不变时 `activateReplacement`。这是最后一个允许失败的物理步骤；失败时应用进入 `Failed`，已发布的内容、identity、音频句柄和 GPU cache 都不改，但设备可能已经被切换，因此不声称可以回滚硬件。
5. 无失败交换：`PlaybackSession::commit`，然后 `renderer.activate`。commit 失败时同样 `discard` 候选并进入 `Failed`。
6. 发布 bundle：替换 `session_`、`activeMode_`、`sessionConfig_`、`timingOffsetMs_`，装好新的 `ChartClock`，换掉 seat 和活动 clip，移除上一个 clip，并置位 `consumeBundleReplaced`。ChartClock 会话把 `pendingChartSeekMs_` 设成 0（重来）或事务前的 chart time（保留）；音频会话交给 transport，新设备按同一位置开流。帧记录 `lastFrame_` 保持不变：它表示 session 真正收到的上一帧，既是后续 reload 的目标，也是下一帧 discontinuity 的基准。

提交之后的失败（`submit` 或 `present` 返回错误）由 `markFailed` 发布 `Failed`。`presentation.renderer.surface.zero_size` 例外：它只跳过该帧，不进入故障状态。

`runPlayerFrameLoop` 每帧的顺序：

1. `PlayerSurface::pollInput`。返回 `quitRequested` 和本帧的动作列表。退出请求直接离开循环，其余动作逐个经 `playerCommandForInput` 翻成 `PlayerCommand` 再走 `apply`：被拒绝的动作只记 `player.command` 警告，不让播放器停止，成功则记 `player.input` 信息。
2. 有音频 seat 时，先跑 `beforeAudioService`，再 `service`、`recheckBoundDevice`、读取 snapshot，并把观察折进应用状态。配置里的输出校正非 0 时，只改送给 `RuntimeTimeline` 的位置副本，不改 transport snapshot。
3. 没有音频 seat 时，用 `ChartClock` 采样。先消费待处理的 seek target 并 rebase；`--smoke-test` 使用固定时刻 `0, 625, 1250, 1750, 1750, 625` 毫秒；普通播放使用从进程启动起的墙钟；非播放状态保持上一时刻。
4. `timeline.advance`。失败立即返回。
5. `afterTimelineAdvance`。hook 可以替换本帧的 `RuntimeFrame`。
6. 事务（输入或 hook 里的）若替换了 bundle，`consumeBundleReplaced` 为真，本帧用新 bundle 再采样、再 advance 一次。这一帧才是 session 收到的帧，所以当它的 discontinuity id 与上一次真正投递的 id 不同时，delta 归零，满足「新 discontinuity 的第一帧 delta 必须为 0」的合同。
7. `session.update`，查询 drawable 尺寸，`renderer.resize`，`extractFrame`。
8. 清空 `RenderScene`，追加 snapshot 调试轴，并在需要时记录 frame diagnostics。
9. `beforeSubmit`。
10. `renderer.submit(snapshot, &scene)`。错误码 `presentation.renderer.surface.zero_size` 时跳过本帧，不增加帧号，也不 present。
11. `renderer.present`。然后 `notePresented`，再 `validatePresented`。失败时 `markFailed` 并返回该错误。
12. 第 0 帧写 snapshot 对象数和 draw 计数。
13. `--smoke-test` 满 6 帧或 `--audio-smoke-test` 满 90 帧后请求退出。提前退出且帧数不足时返回对应的 incomplete 错误。

键盘绑定把窗口按键翻成后端无关的 `PlayerInputAction`，再翻成命令。`playerCommandForInput` 是纯函数，因此这套翻译在无 GPU 的测试里被完整覆盖：

| 按键 | 动作 | 命令 | 说明 |
| --- | --- | --- | --- |
| 空格 | `PlayPause` | `Pause` / `Play` | 播放中暂停，否则播放 |
| ← | `SeekBackward` | `Seek` | 目标为 `max(0, 当前 chart time - 5000ms)` |
| → | `SeekForward` | `Seek` | 目标为 `当前 chart time + 5000ms` |
| S | `Stop` | `Stop` | ChartClock 回到 chart time 0 |
| R | `Reload` | `Reload` | `KeepChartTime` |
| B | `Rebuild` | `Rebuild` | 只有 `Failed` 或已加载内容时才有意义 |
| Esc | `Quit` | 无 | 请求退出循环，不经过命令表 |

暂停时按空格会恢复播放，播放时按空格暂停。空状态下按空格或 Seek 会被命令表拒绝，只记警告。SDL 只对持有键盘焦点的窗口上报按键，所以自动化验证需要先把窗口提到前台。

诊断内容和正式内容在同一次 `submit` 里：调试轴走 `snapshot_scene.cpp` 的后端无关 `RenderScene`，正式内容走同一 `FrameSnapshot`。正式提交使用 `IPresentationRenderer::submit` 和 `present`。控制循环不调用 `renderPresentationFrame`。

### 3.6 `player_smoke.cpp`

`PlayerSmokeBinding` 只在两种 smoke 之一打开时构造。正式播放的 `PlayerHooks` 为空。两种 smoke 不能同时打开。

smoke 剧本不再自己调用 Session、renderer 或音频 seat 来完成内容生命周期，而是构造 `PlayerCommand` 交给 `controller_.apply`。剩下的直接调用只是不提交内容的负例探针和像素/digest 断言。

表现 smoke 安装 `afterTimelineAdvance`、`beforeSubmit`、`notePresented` 和 `validatePresented`：

| 帧 | 行为 |
| --- | --- |
| 0 | 校验不透明中心像素；最小化再恢复，比较 digest 和中心像素，并在最小化后 drawable 为 0 时验证零尺寸 `submit` 被拒绝 |
| 1 | 先用 `renderPresentationFrame` 比较省略场景、空场景和正式提交的 debug summary，再校验透明纹理像素和 digest |
| 2 | 校验不可见帧没有 presentation draw，中心像素保持清屏色 |
| 3 | 非法来源的 `Reload` 必须失败并保留活动 GPU cache。随后拒绝不支持的 presentation 请求和 legacy candidate，且不 commit |
| 4 | 第二个 renderer candidate 必须因 outstanding candidate 被拒绝。然后 `Reload{RestartAtZero}` 提交并 activate，timeline 重置后从 chart time 0 再 advance |
| 5 | 再次校验透明帧 digest 和像素 |
| 每帧 | 用 OpenGL `lastPixelProbe` 做像素断言 |

音频 smoke 安装 `beforeAudioService` 和 `afterTimelineAdvance`：

| 帧 | 行为 |
| --- | --- |
| 15 | `Pause`，等待 2 秒，时钟状态、帧号、位置和 discontinuity 必须保持，然后 `Play` |
| 30 | `Seek{500}`。控制器用 `reverseSeekSourcePositionUs` 把 chart time 换回 source，再 `seekMs` |
| 45 | `Stop` |
| 46 | `Play` |
| 55 | 显式坏来源的 `Reload` 必须失败，活动内容和音频 snapshot 保持不变 |
| 60 | `Reload{KeepChartTime}`：准备替换内容、renderer candidate 和音频 replacement，激活音频后再 commit 和 activate。然后 reset timeline，替换 clip，再 advance 一次 |

像素探针和 `renderPresentationFrame` 留在这份文件里，因为它们是 `OpenGlBackend` 的探针，不是 `IPresentationRenderer` 的正式帧入口。

## 4. 窄接口

控制循环不接收 `SdlWindow`、`OpenGlBackend` 或 `SdlAudioTransport`。

`PlayerSurface` 提供三件事：本帧的 `PlayerInput`（退出请求和动作列表），以及 drawable 的宽高。尺寸为负时控制循环把它夹到 0 再交给 `resize`。装配层的 `SdlPlayerSurface` 把 `WindowEvents` 里按下的命名键映射成动作；`PlayerInputAction`、`PlayerInput` 和这个窄接口都不含 SDL 类型。

`PlayerControlPorts` 是控制层无法自己实现的三件事：`makeSource` 产出配置来源，`prepareClip` 把 prepared 主音乐变成已注册 clip，`openAudio` 开设备。装配层提供它们，控制层只按事务顺序调用。

`PlayerAudioSeat` 暴露 `IAudioTransport`，另加四项不在该虚表上的操作：`recheckBoundDevice`、`prepareReplacement`、`activateReplacement`、`applyGain`，以及 `unload`。ChartClock 路径的 seat 指针为空。

`PlayerHooks` 的五个回调点按上面的帧顺序调用。回调返回错误时帧循环停止，并把该错误交给 `run`。

## 5. 关闭顺序

`runPlayerFrameLoop` 返回后，组合根依次：

1. 记录最终音频 snapshot 和 metrics。
2. 导出 frame diagnostics。
3. `OpenGlBackend::close`。这一步释放 GL 资源，不关闭窗口。
4. `PlayerController::shutdown`：`PlayerAudioSeat::unload` 并释放 seat，从 `AudioClipStore` 移除活动 clip，`PlaybackSession::unload`，最后把应用状态置回 `Empty`，使后续命令不再报告已经不存在的内容。

函数返回后，局部对象按声明的相反顺序析构：帧循环和 smoke binding 先于 backend，backend 先于窗口和 `PlayerSurface`，窗口先于 SDL runtime，音频 seat 先于 transport 和 subsystem，clip store 先于 Session。

## 6. 应用状态、故障状态与恢复

- `Failed` 不是“内容消失”。它表示某次事务在不可回滚的步骤上失败，已发布的内容、identity、时间、GPU cache 和音频句柄保持不变，但设备可能已经变化。
- 只有 `Load` 和 `Rebuild` 能离开 `Failed`；`Play`、`Pause`、`Stop`、`Seek`、`Reload` 都返回 `player.command.failed`。
- 软件准备失败（来源、prepare、候选、precheck）不进入 `Failed`，只是拒绝本次命令并保留旧 bundle，旧播放允许自然前进。
- `Reload` 从不改变 `PlaybackMode`；切换有/无音轨内容必须用 `Load`。`Rebuild` 保持活动 mode，但会重建 renderer 和音频设备。
- 应用状态不强迫公共 `playback::SessionState` 扩张；设备观察经 `observeTransportState` 折进应用状态。

## 7. 架构检查

`cmake/VerifyArchitecture.cmake` 读取下列文件。任一文件出现 SDL、`platform_sdl`、`render_opengl`、`audio_sdl` 或 GLAD 头，配置期架构测试失败：

- `app/player/src/player_options.hpp`
- `app/player/src/player_options.cpp`
- `app/player/src/player_surface.hpp`
- `app/player/src/player_audio_seat.hpp`
- `app/player/src/player_control.hpp`
- `app/player/src/player_control.cpp`

`player_app.cpp`、`player_assembly.cpp` 和 `player_smoke.cpp` 可以包含这些平台头。`player_app.cpp` 只负责把具体对象交给窄接口和 smoke binding。

`cuexis_player_control_tests` 直接编译 `player_control.cpp`，用中立的 `TestPresentationRenderer` 和共享的 fake audio transport 驱动命令表和全部事务失败阶段，因此一般状态机测试不依赖 GPU、窗口或真实音频设备。

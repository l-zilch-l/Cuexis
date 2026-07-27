# Cuexis 阶段 1D 完成报告

状态：实现与自动化最终验收完成；GPU 和物理默认音频设备脚本门禁完成，主观听感仍需人工观察  
报告日期：2026-07-27  
完成版本：`26.07.18.18-1`（Debug：`26.07.18.18-1-dev`）  
SDK preview API：`0.2.0`  
阶段目标：完成主音乐内容、ChartClock/HostClock/CuexisAudio 三模式、Prepared Playback、
RuntimeTimeline、WAV 解码、SDL 音频 adapter 与可脚本诊断闭环。

## 1. 完成结论

阶段 1D 的实现范围和本地自动化门禁已经完成。Asset Index v2 和 Chart v2 提供 typed
主音乐引用；PlaybackSession 在 commit 前同步准备 Chart、AudioSource 和内容信息，并以
PreparedPlayback 保证 load/reload 的候选对象、Source Lease 和模式一致性。ChartClock、
HostClock 和 CuexisAudio 都通过后端无关的 RuntimeTimeline 生成 1C RuntimeFrame，继续复用
PlaybackSession、Behavior 绝对时间采样和 FrameSnapshot，不建立第二条私有 Runtime 路径。

`cuexis_audio` 提供不依赖 SDL 的配置、Clip Store、Transport/Clock contract；
`cuexis_audio_sdl` 提供有界内存 WAV 解码和 SDL3 默认设备实现。CuexisAudio 失败不会静默回退到
HostClock 或 ChartClock。Player 的音频路径在创建窗口、OpenGL 和设备前完成内容/WAV preflight，
reload 使用 prepare/activate/commit 顺序，确定性准备失败保留旧内容。

Debug、Headless Debug 和 Release 的 fresh configure、clean build 与完整 CTest 已通过；
adapter-disabled headless 构建不包含 SDL3、OpenGL、Player、platform SDL 或 AudioSDL target。
add_subdirectory/find_package 的基础和 AudioSDL component consumer 均在完整 Visual Studio
Developer 环境中通过。Debug/Release GPU smoke 与物理默认音频设备脚本 smoke 也已通过。

## 2. 已冻结并落地的契约

| 领域 | 落地结果 |
| --- | --- |
| 内容格式 | Asset Index v2 增加 `audio`；Chart v2 增加 `audio.mainMusic`；v1 语义保持不变 |
| 模式 | ChartClock、HostClock、CuexisAudio 显式且不可在活动 Session/reload 中切换 |
| 内容交付 | PreparedPlayback 内部持有 Source Lease，只暴露高层 MainMusicSourceView |
| 时钟 | SourceClockSample 使用 source-domain position、状态和 discontinuity；RuntimeTimeline 统一映射 RuntimeFrame |
| PCM | 整首 WAV 同步预解码为 interleaved F32，支持 1/2 channels |
| 预算 | encoded 64 MiB、decoded 256 MiB；queue/low-water 默认 200/100 ms |
| 状态机 | load/play/pause/resume/seek/stop/reload/error 均有稳定 Result 与状态语义 |
| 错误边界 | 已选模式不 silent fallback；异常不跨公共模块边界；实时回调不抛异常 |
| 诊断 | 固定容量双 CSV + metadata sidecar，默认关闭，离线写盘 |
| SDK package | 导出 `Cuexis::Audio`；`Cuexis::AudioSDL` 仅在请求组件时引入 SDL3 |

相关决策由 [ADR 0031](../adr/0031-main-music-content-format-v2.md) 和
[ADR 0032](../adr/0032-playback-clock-and-prepared-audio-transaction.md) 冻结；
[ADR 0012](../adr/0012-use-sdl3-audio-backend.md) 记录 SDL backend、状态机和设备限制。

## 3. 实际交付范围

| 模块 | 完成内容 |
| --- | --- |
| Project/Chart | Asset Index v2、Chart v2 Schema/Reader、typed mainMusic 与 v1/v2 严格路由 |
| Assets | AudioSource type、Handle/Lease、content revision 与 Required 加载 |
| Playback | Prepared load/reload、PlaybackContentInfo、MainMusicSourceView、模式校验、RuntimeTimeline |
| `cuexis_audio` | AudioConfig、AudioClip/Store/Handle/Lease、Transport、Clock、metrics、Fake/Chart Clock |
| `cuexis_audio_sdl` | 内存 WAV decoder、SDL subsystem/device/stream RAII、queue、clock、状态机与遥测 |
| Player | 1D 默认项目、ChartClock/CuexisAudio 驱动、audio smoke、frame/audio diagnostics |
| Packaging | Audio 与可选 AudioSDL component export、adapter-disabled preset、外部 consumer gate |
| 文档 | 构建、版本、依赖、Chart/Runtime/Timing、ADR、1C review closure 与 1D 计划/报告 |

阶段 fixture `assets/projects/stage1d_project` 使用 Chart v2、Asset Index v2 和一个 48 kHz 双声道
非静音 WAV。阶段 1A/1C 的无音频 Chart 继续作为 ChartClock 回归输入。

### 3.1 最终复核修复

2026-07-27 的逐项最终复核补齐了以下实现与证据：

- SDL underrun 时冻结最后有效的 source position，并重设 device-frame 基线；恢复供数后从冻结位置继续。
- PreparedPlayback 使用全局唯一 Session token 和 generation；成功 update 后旧候选失效，同地址重建的 Session 也不能接收旧候选。
- 测试专用 Fake transport 覆盖 underrun 冻结/恢复/限频、设备 Error、replacement 准备与激活失败；HostClock/CuexisAudio 对相同控制脚本逐帧比较 RuntimeFrame 和 FrameSnapshot。
- Audio smoke 执行真实两秒 Pause 冻结检查，并验证失败 reload 不改变活动内容或音频 Clock；成功 reload 仍走 prepare/activate/commit。
- AudioSDL 对 callback 使用的 32/64 位整数原子增加 `is_always_lock_free` 编译期约束，不支持的平台只拒绝构建可选 adapter。
- 诊断导出在截断时仍先写完双 CSV 与 sidecar，再返回稳定的 `player.frame_stats.truncated`；新增启用/关闭诊断不改变 RuntimeFrame/frameHash 轨迹的测试，并在 Player 退出前记录最终 state、queue、discontinuity 和 underrun。

## 4. 构建与自动化验证

构建环境为 Windows x64、MSVC `19.51.36248.0`、CMake、Ninja 和 vcpkg manifest mode。
external consumer 必须从 Visual Studio Developer 环境运行，使 `cl.exe`、`rc.exe`、`mt.exe` 和
Windows SDK 都在 PATH；仅传递 C++ 编译器路径不足以完成 CMake 的 MSVC manifest try-compile。

```powershell
cmake --preset debug --fresh
cmake --build --preset debug --clean-first
ctest --preset debug --no-tests=error
cmake --build out\build\debug --target cuexis_format_check

cmake --preset headless-debug --fresh
cmake --build --preset headless-debug --clean-first
ctest --preset headless-debug --no-tests=error

cmake --preset release --fresh
cmake --build --preset release --clean-first
ctest --preset release --no-tests=error
```

| 验证项 | 结果 |
| --- | --- |
| Debug fresh configure | 通过 |
| Debug clean build | `133/133` 步通过 |
| Debug CTest | 226 项，0 失败；1 项 symlink 测试按平台条件跳过 |
| Headless Debug fresh configure | 通过 |
| Headless Debug clean build | `108/108` 步通过 |
| Headless Debug CTest | 195 项，0 失败；1 项 symlink 测试按平台条件跳过 |
| Release fresh configure | 通过 |
| Release clean build | `133/133` 步通过 |
| Release CTest | 226 项，0 失败；1 项 symlink 测试按平台条件跳过 |
| Architecture/target allowlist | 三套完整 CTest 均通过 |
| clang-format dry-run | 通过，0 条格式诊断 |

最终复核时，受限 shell 首次重建 external consumer manifest 因无权访问用户 vcpkg registry 而失败；
提升到完整 Visual Studio Developer 环境后，三个 preset 的四种 consumer gate 均独立重跑通过。
这属于测试执行环境要求，不是 Cuexis configure、compile 或 link 失败。

## 5. Headless 与外部 Consumer

Headless Debug 只解析/安装 Catch2、EnTT、GLM、nlohmann JSON、JSON Schema Validator 和
tl-expected 相关依赖，不查找 SDL3、glad 或 spdlog。生成 target 集合不含 AudioSDL、platform
SDL、OpenGL 和 Player，证明 HostClock/ChartClock Playback 核心不依赖设备 adapter。

| Preset | add_subdirectory base | add_subdirectory AudioSDL | find_package base | find_package AudioSDL |
| --- | --- | --- | --- | --- |
| Debug | 通过 | 通过 | 通过 | 通过 |
| Headless Debug | 通过 | 通过 | 通过 | 通过 |
| Release | 通过 | 通过 | 通过 | 通过 |

基础 `find_package` consumer 显式禁止 SDL3 查找仍可构建；AudioSDL component 通过独立
`CuexisAudioSDLTargets.cmake` 引入 SDL3。短 external build 路径避免 Windows MSVC 对象路径上限，
顶层 Ninja 和 C++ compiler 被显式传递给子构建。

## 6. GPU 与物理音频设备门禁

```powershell
.\out\build\debug\bin\cuexis_player.exe --smoke-test
.\out\build\release\bin\cuexis_player.exe --smoke-test

.\out\build\debug\bin\cuexis_player.exe --audio-smoke-test `
  --frame-stats .\out\artifacts\stage1d-debug
.\out\build\release\bin\cuexis_player.exe --audio-smoke-test `
  --frame-stats .\out\artifacts\stage1d-release
```

| 构建 | GPU smoke | Audio smoke | 结果 |
| --- | --- | --- | --- |
| Debug | 3 frames | 90 frames，两秒 Pause、失败/成功 reload 完成 | 通过 |
| Release | 3 frames | 90 frames，两秒 Pause、失败/成功 reload 完成 | 通过 |

GPU 环境为 Windows SDL video driver、NVIDIA GeForce RTX 4060 Laptop GPU，OpenGL
`3.3.0 NVIDIA 596.36`。默认音频设备协商结果为 48,000 Hz、2 channels、480-frame buffer，
估算输出 latency 为 10 ms。两次 audio smoke 均打开真实默认设备，执行 scripted
load/play/两秒 pause/resume/seek/stop/失败 reload/成功 reload 流程并正常退出；Pause 期间位置、
状态、frame 和 discontinuity 保持冻结，失败 reload 未改变活动内容和 Clock。最终状态均为
Playing、discontinuity 为 4、underrun count 为 0；Debug/Release 最终 queue 分别为 5,280/4,800
frames。

脚本执行证明设备创建、stream、时钟、控制流、reload 和诊断导出闭环可工作，但自动化无法独立
证明用户实际听到声音或“无明显爆音”。非静音 fixture 已由测试保证；最终主观听感仍需人在设备前
观察，不能由本报告虚构为已验证。

## 7. 诊断产物

每次 audio smoke 产生固定命名的 `.frames.csv`、`.audio.csv` 和 `.meta.json`：

| 构建 | Frame rows | Audio rows | Dropped | Truncated | Mode |
| --- | ---: | ---: | ---: | --- | --- |
| Debug | 90 | 90 | 0 | false | `cuexis_audio` |
| Release | 90 | 90 | 0 | false | `cuexis_audio` |

metadata 使用 `cuexis.frame-stats` version 1，记录 build version 和 SDK API `0.2.0`。
确定性 CSV 固定列为 frame index、chart time、delta、discontinuity 和 frame hash；设备 CSV 固定列为
wall clock、source position、latency、queue、underrun 和 transport state。产物达到容量上限时停止
追加并累计 droppedRows；导出器仍写出可检查的稳定前缀与 sidecar，但自动门禁随后以
`player.frame_stats.truncated` 失败。本次两个构建均未丢行或截断。

## 8. 验收标准对照

| 阶段 1D 验收标准 | 证据 | 结论 |
| --- | --- | --- |
| v1/v2 内容严格路由且 v1 不接受 Audio | Schema/Reader/Runtime/fixture 测试 | 通过 |
| 三模式使用同一 RuntimeFrame 与 1C Playback 路径 | Clock、Timeline、Playback 和 mode parity 测试 | 通过 |
| Prepared load/reload 保证内容与模式事务 | Playback/Player reload 测试和 audio smoke | 通过 |
| WAV/PCM/Store/状态机有界且错误稳定 | Audio/AudioSDL 单元与 dummy backend 测试 | 通过 |
| Headless 核心无 SDL/OpenGL adapter | headless target/dependency 集合与 architecture scan | 通过 |
| 基础 package 不传递 SDL，AudioSDL 可选 | 12 次 external consumer gate | 通过 |
| Debug/Headless/Release、format、architecture | 本报告第 4 节 | 通过 |
| GPU 与物理默认设备脚本门禁 | 本报告第 6 节 | 通过 |
| 双 CSV + sidecar、无 drop/truncate | 本报告第 7 节 | 通过 |
| 主观听感 | 需要人在实际输出设备前确认 | 待人工观察 |

## 9. 已知限制与后续边界

当前没有已知 P0/P1 实现问题。保留以下边界：

| 优先级 | 事项 | 后续处理 |
| --- | --- | --- |
| P2 | SDL 对同格式默认 route 的透明迁移可能无法从公共 API 可靠检测 | 保留在 EffectiveAudioSettings/诊断限制中；设备管理阶段再评估平台能力 |
| P2 | 主观响度、爆音和实际声卡输出无法由当前脚本自动判定 | 在发布前硬件矩阵中补充人工听感记录 |
| P2 | 托管 CI 没有真实 GPU/音频设备 | CI 只声明 headless、dummy、build/package/format/architecture 结果 |
| P3 | 仅支持整首 WAV 预解码和单主音乐流 | OGG/MP3/FLAC、streaming、mixing、loop/gapless 留待后续阶段 |

阶段 1E 继续冻结完整 public component matrix、package compatibility policy 和 shared-library
范围；不得把 AudioSDL 变成 Playback 核心的传递依赖。UserPreferences、AudioDeviceProfile、设备
身份和持久化校准留给阶段 6；正式性能预算和 profiler 接入留给阶段 9A。

## 10. 相关文档

- [阶段 1D 实施计划](../stage_plans/stage_1d_implementation_plan.md)
- [阶段 1C 完成报告](stage_1c_completion_report.md)
- [260722 阶段 1C 全量审查](260722-1c-review.md)
- [SDK 转型方案](../stage_plans/cuexis_sdk_transition_plan.md)
- [阶段 1E 实施计划](../stage_plans/stage_1e_implementation_plan.md)
- [ADR 0027：Playback SDK 产品边界](../adr/0027-playback-sdk-product-boundary.md)
- [ADR 0030：Preview API、package version 与 Result](../adr/0030-playback-preview-api-version-and-result.md)
- [ADR 0031：主音乐内容格式 v2](../adr/0031-main-music-content-format-v2.md)
- [ADR 0032：Playback 时钟与 Prepared Audio 事务](../adr/0032-playback-clock-and-prepared-audio-transaction.md)
- [构建指南](../BUILDING.md)
- [Chart 格式](../CHART_FORMAT.md)
- [Timing Model](../TIMING_MODEL.md)

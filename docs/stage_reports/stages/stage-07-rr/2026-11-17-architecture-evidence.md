# 架构与实现探索证据记录（2026-11-17）

<!-- cuexis-meta
id: EVIDENCE-stages.stage-07-rr.2026-11-17-architecture-evidence
status: evidence
authority: none
date: 2026-11-17
-->

状态：evidence；一次性记录，不随代码演进维护

更新日期：2026-11-17

本文件保存 8 个无上下文研究子代理的结论与 owner 抽查结果，目的是**在上下文压缩后仍能复核**。
结论性摘要与减重方案见[架构与实现研究](2026-11-17-architecture-research.md)；
条目化登记见[发现登记册](../../../stage_plans/active/stage-07-rr/findings.md)。

> 证据状态标记：**[核]** = owner 亲自读过实现或做过统计；其余为子代理报告，未逐条独立复核。
> 复核方法见 §10；本文件不是合同，不构成对任何行为的保证。

## 1. 依赖与分层

规模：`engine` ≈ 86.6k 行，`engine+app+tools` ≈ 114.4k 行 **[核]**；26 个 engine 目录。

| 模块 | 文件/行 | 链接消费者 | 角色 |
| --- | --- | --- | --- |
| core | 29/2,813 | 25（全库） | God 模块：error/Result/diagnostic/sha256/uuid/path + `internal/` |
| judgement | 87/20,518 | 1（V5=OFF 时 0） | 三个域塞一库：graph/assembler 5,981、execution kernel 3,944、input/timebase/recovery 5,865 |
| chart | 98/17,864 | 6 | 5 条并行加载/解码线 |
| playback | 53/13,216 | 7 | SDK 门面，PRIVATE 私连 9 个内部库（含 runtime/world/render） |
| gameplay_packed | 20/4,821 | 1（仅 V5） | 离线 packed 物理适配器 |
| cxc | 16/3,280 | 4 | 容器 + 4 个 CLI 共用 |
| render_opengl | 12/3,219 | 1（player） | 唯一 GL 后端，PUBLIC 依赖 playback/presentation_renderer |
| runtime | 11/3,060 | 1（playback） | 旧 RuntimeSession 路径，仍被 SDK 私连 |
| assets | 9/2,416 | 3 | 资源数据库/管理器 |
| shader / shader_cache | 11/2,155 | 1 / 2 | shader-tools 默认 OFF |
| json_support | 14/2,114 | 6 | nlohmann 隔离层 |
| world | 12/1,908 | 6 | EnTT 世界 |
| project | 9/1,532 | 3 | 工程文档 |
| animation | 11/1,489 | 2 | CXT→动画程序 |
| audio_sdl / platform_sdl | 6/1,338、5/518 | 1、2 | SDL 适配器 |
| player_support | 12/1,215 | 1 | 玩家配置/CLI，**不是引擎能力** |
| presentation_renderer | 4/1,182 | 1 | draw-command 渲染器，依赖 playback |
| audio | 6/535 | 3 | 音频传输抽象 |
| filesystem | 3/443 | 6 | secure_file |
| content | 2/340 | 3 | 仅 IContentProvider 门面（已进安装树） |
| behavior | 4/331 | 1 | 行为系统 |
| render | 5/176 | 7 | 极小的 RenderBackend 契约 |
| debug | 2/70 | **0** | 仅自己的测试链接 |
| gameplay | 1/9 | 1 | 只有两个 tag struct，仍占被依赖位 |
| particles | 0/0 | 0 | INTERFACE stub，**从未 add_subdirectory** |

分层（实测 CMake 边）：

```text
L0 core
L1 audio / filesystem / json_support / world / content / shader_cache
L2 project / assets / chart / animation / behavior / judgement / shader / gameplay(I)
L3 cxc / render / runtime / gameplay_packed
L4 playback / debug
L5 presentation_renderer  (+ app/execution_host)
L6 platform_sdl / audio_sdl / render_opengl
   player（可执行）  tools/*（旁路）
```

结构性违规边：

| 编号 | 违规 | 证据 |
| --- | --- | --- |
| V1 | 可选渲染适配器 PUBLIC 依赖 SDK | `engine/render_opengl/CMakeLists.txt:27-31` |
| V2 | "后端中立渲染器"仍依赖 playback；实际 3 个 render target | `engine/presentation_renderer/CMakeLists.txt:12` |
| V3 | 工具反向消费 SDK：`chart_candidate → playback`；`gameplay_author` 直编 `engine/playback/src/gameplay_entry.cpp` | `tools/chart/chart_candidate/CMakeLists.txt:5`、`tools/gameplay/gameplay_assembler/CMakeLists.txt:9-13` |
| V4 | 消费方读被消费方私有源码：playback 加 `engine/runtime/internal`；gameplay_packed 加 `../judgement/src`、`../chart/src` | `engine/playback/CMakeLists.txt:53,101`、`engine/gameplay_packed/CMakeLists.txt:18-21` |
| V5 | 死重量占分层位：debug（0 消费者）、particles（未接入）、gameplay（9 行仍被 runtime 依赖） | `engine/debug/CMakeLists.txt:15`、`engine/runtime/CMakeLists.txt:41` |
| V6 | 默认 OFF 的候选分支全量编译：judgement 20.5k 在 OFF 时**零产品链接边** | `CMakeLists.txt:44-46`、`engine/CMakeLists.txt:24-25`、`engine/playback/CMakeLists.txt:47-54` |

隐式耦合：

- `cuexis_internal/` 两个互不相识的根：`engine/core/internal/cuexis_internal/` 与
  `engine/render_opengl/internal/cuexis_internal/`。
- core 的内部头承载 judgement 执行契约：`execution_diagnostics.hpp:9` 的 `Part` 枚举是被判定内核与
  playback 共同依赖的阶段名；`presentation_metrics.hpp` 有 9 个消费者跨 5 个模块。
- 假公共路径：`<cuexis/judgement/publication.hpp>` 实为 `engine/judgement/internal/...`，
  playback 通过私有 include 拿到（`engine/playback/CMakeLists.txt:50`）。
- 跨模块 src 头：`engine/gameplay_packed/src/gameplay_capsule_common_internal.hpp:7` include
  `engine/judgement/src/claim_key_internal.hpp`。
- **应用层越界（2026-11-17 复核）**：`app/` 直接 include `engine/playback/src/replay_identity.hpp`
  6 处（`app/execution_host/host_candidate.cpp:2` 与 `app/player/src/execution_host*.cpp:3`），
  直接 include `cuexis_internal/` 至少 11 处；`app/execution_host` 6,835 行、`app/player/src`
  7,794 行，自成一层并复制会话逻辑；`engine/player_support` 反向只被 app/player 链接。
  门禁只对 6 个 `player_neutral_sources` 禁止 adapter 头，不禁止 src/internal include。
- 时间表示 4 套并存：`chart::RationalBeat`、`judgement::RationalBeat`（互不引用）、
  `judgement::Tick`、`playback::ChartClock`/`RealtimeClock`、`audio::HostClock`。
- 门禁缺口：`VerifyArchitecture.cmake` 只禁第三方/上层头，**不**检查 `cuexis_internal/` 越界或跨模块
  `src/` include，上述耦合全部合法通过。

## 2. 双产品线

两条链（关键位置）：

- **A 中立链**：`main.cpp:44` → `player_app.cpp:102` → `player_options.cpp:21` → `player_control.cpp:59`
  解析源 → 时钟选择 `player_app.cpp:185-200` → `prepareLoad` `player_control.cpp:270-282` →
  `commitBundle` `player_control_transaction.cpp:338-516` → 主循环
  `player_control_frame_loop.cpp:69-433`（推进 `:83-129`、`update` `:295`、`extractFrame` `:315-319`、
  呈现 `:350-365`）→ 关闭 `player_app.cpp:463-468`。
- **B 候选链**：`player_app.cpp:177-180`（`CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE` +
  `--execution-test-only`）→ `player_execution.cpp:107` → 准入自检 `:109-122` →
  `ExecutionHost::create:357` → `execution_host_frame.cpp:21-94` load → 主循环
  `:420-612`（控制 `:456-533`、advance `:534-558`、取帧 `:566-573`、呈现 `:606-608`）→
  `host->close` `execution_host_close.cpp`（30 阶段）→ 排空 `execution_host.cpp:71-78`。

同语义双份（择要，完整 18 行对照在子代理报告中）：

| 职责 | A | B | 行为差异 |
| --- | --- | --- | --- |
| 提交门 | `playback_session_prepare.cpp:481`（六步） | `commitExecutionCandidate` `playback_session_execution.cpp:95`（fence 四步） | 同一 `PlaybackSession` 两个 commit 入口 |
| 会话所有权 | 单指针 `player_control.hpp:190` | `SessionWorker` 线程 + 命令/回复 | A 单线程；B 两个 owner 线程 |
| 时间基准 | `PlayerClock`（steady_clock→double ms）+ `ChartClock::sample` | `RealtimeClock` + `ClockObservation{validUntil,errorBoundUs}` | B 可表达 stale，A 不能 |
| 帧推进 | `timeline.advance` 必执行 | `host->advance` 可 `control_pending` 退让 | 语义不同 |
| 帧取回 | `extractFrame` 必得 | `trySample` + `DeliveryStatus` 可跳过 | 是否允许丢帧 |
| 呈现提交 | `renderer.submit/present` | `presentOwnedFrame` + `RendererConsumerGate` | B 有 receipt/retained frame |
| 诊断投影 | `FrameDiagnostics` | `execution_diagnostics` + rpa ledger | `--frame-stats` 在 B 被静默忽略 |
| 错误通道 | `core::Result` 逐层返回 | 异常 `Failure` + 顶层 try/catch | 见下 |
| 关闭/回滚 | `transaction:518-544` | 31 阶段 + 1s 预算 + 排空 | 失败路径 A 不清场（`player_app.cpp:437-439`） |
| 音频所有权 | `PlayerAudioSeat` | `AudioWorker` + `OwnedAudioDevice` | 两套设备抽象 |

行为差异（非风格）：丢帧允许性、失效时钟、暂停是否事务、seek 可发明性（B 前向 seek 不得超 horizon）、
失败清理、错误码空间（`player.execution.test_only` 单码 vs `player.*`/`playback.*`）、
同一 flag 两义（`--frame-stats` 在 B 无效、`--smoke-test` 在 B 被拒）。

规模：A 专属约 1,890 行（另 `player_smoke` 582、`frame_diagnostics` 174）；
B 专属 `app/player/src/execution_host*` ≈2,878 + `app/execution_host` **6,835 行 [核]**
（子代理给 6,748，差异是它含 10 行 CMakeLists 且用 `Measure-Object -Line`，本文件取 cpp/hpp 物理行）；
引擎候选执行面 ≈4,357；`tests/rpa`+`tests/pd` ≈19,814。
条件编译：`CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE` **65 处 [核]**、`CUEXIS_RPA_OBSERVATION_TEST_ONLY` 135、
`CUEXIS_PLAYBACK_TRACE_HARNESS` 30。只在 B 上存在的目标：`cuexis_execution_host_tests`、
`cuexis_host_protocol_tests`、`cuexis_pd_host_probe`、`cuexis_pd_baseline_probe`、`cuexis_execution_device_probe`。

合并选项：①只留 A 删 B（风险最高，作废 RPA/PD 可复现链）；②只留 B（会迫使公共面暴露
`playback::detail::*`，违反安装头约束）；③以 A 为唯一产品链、把 B 的帧观察/时间锚点/关闭事务
作为可注入件（折中）；④不合并只收敛开关面（低风险过渡）。

## 3. 格式线

| # | 格式 | Reader | Writer | Migrator | Schema 被代码引用 | 行数 | 默认入口消费 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | Chart v1/v2/v3 | ✓ | ✓ | ✓ | 否 | 5,162 | ✓ |
| 2 | Chart v4 | ✓ | ✓ | ✓ | 否 | 4,030 | ✓ |
| 3 | Chart v5 Core | ✓ | ✗ | ✗ | — | 765 | ✗（flag） |
| 4 | Packed CXPk5 rev1 | ✓ | ✓ | ✗ | — | 4,579 | ✗ |
| 5 | CXT v1 | ✓ | ✓ | ✗ | 否 | 282 | ✓ |
| 6 | CXT v2 | ✓ | **✗** | ✗ | 否 | 1,958 | ✗（Playback 零引用） |
| 7 | CXC v1 | ✓ | ✓ | ✗ | 否 | ~2,900 | ✓ |
| 8 | CXC candidate entry | ✓ | ✓（工具） | ✗ | — | ~380 | ✗ |
| 9 | Capsule v2 | ✓ | ✓ | ✗ | — | 2,833 | ✗（目标总建、零链接） |
| 10 | Capsule rev3/4 | ✓ | ✓ | ✗ | — | 501 | ✗ |
| 11 | Graph v1 | ✓ | ✓ | ✗ | 否 | 1,487 | ✗ |
| 12 | Project v1 | ✓ | ✓ | ✗ | 否 | ~900 | ✓ |
| 13 | Asset Index v1–v3 | ✓ | **✗** | ✗ | 否 | ~630 | ✓ |

关键事实：

- `cuexis_chart` 的 `packed_*`/`cxt_v2_*`/`candidate_*` 在**无 if() 的 `target_sources`** 里
  **[核]**（`engine/chart/CMakeLists.txt:6-60`）= 7,302/17,864 = **40.9% 默认不可达**。
- `cuexis_gameplay_packed` 4,821 行默认编译、零链接（`engine/playback/CMakeLists.txt:47-54` 是唯一入口且在 flag 内）。
- 7 个生产 Schema **没有任何 loader 调用** `validateAgainstSchema`
  （`engine/json_support/include/cuexis/json/schema.hpp:6` 自述 "No loader calls this adapter yet"）。
- 同一 CXPk5 信封两套独立写实现：`packed_chart_tables_writer.cpp:503-541` 与
  `gameplay_capsule_emit.cpp:122-174`；前者 CRC 只算前 92 字节 **[核]**。
- 等价入口 7 组：`PackedChartReader::decode` vs `packed::decode`；`packed::inspect` vs
  `gameplay_detail::inspect`；两套 CXPk5 writer；`packed::decode` vs `gameplay_packed::decoded`；
  `ChartV4Resolver::resolve` vs `detail::resolveV4Parsed`；legacy 三条加载路；CXT v1 vs CXT v2 两代模板语言。
- 文档冲突：`PACKED_CHART_FORMAT.md:3` 称"未实现"（实际 4,579 行 + 68 用例）；
  `docs/formats/README.md` 称 Capsule "revision 1 不变"（代码默认 `candidateRevision{2}`，支持 3/4）。

最小集：真正端到端被消费的闭包 = v1/v2/v3 + v4 + CXT v1 + CXC v1 + Project + Asset Index。
选项：A 只留 v4+CXT v1+CXC v1（违反兼容窗口承诺）；B 把 packed/v5/CXT v2/Capsule/Graph 移出默认目标、
主库保留 v1–v4（推荐）；C 零删除只做构建隔离 + 文档纠偏。

## 4. 时间

- **ChartClock 不是时钟**：`runtime_timeline.hpp:34-46` 只做 `chartTimeMs + offset → SourceClockSample`
  与自增 `discontinuityId_`；真实墙钟是 `PlayerClock`（`player_control_frame_loop.cpp:32-63`）。
- **HostClock**：`audio_transport.hpp:65-82`，产品代码无实例化，只有 `tests/audio/audio_tests.cpp`
  与 `tests/playback/mode_parity_tests.cpp` 构造 **[核]**；`PlaybackMode::HostClock` 只是模式枚举。
- **CuexisAudio**：`IAudioClock/IAudioTransport`，实现 `sdl_audio_transport.cpp` + `sdl_audio_impl.hpp`。
- **RealtimeClock**（RPA 第四套）：`realtime_clock.hpp:68-111`，owner 是
  `PlaybackSession::State::realtimeClock`（`playback_session_internal.hpp:243`）。
- **RuntimeTimeline** 是归一化器，不是时钟。

重复换算与风险：

1. 采样帧→us 两种舍入：`llround`（`sdl_audio_impl.hpp:170`）与 half-even（`realtime_clock.cpp:63-71`）。
2. `sourceMs−offset→chartTimeMs` 两份：`runtime_timeline.cpp:43`（在用）与
   `TimingMap::audioTimeMsToChartTimeMs`（产品无调用）。
3. 反向 seek 两份：`player_support/src/audio_device_profile.cpp:187-203` 与
   `RealtimeClock::seekPosition`（`realtime_clock.cpp:161-174`）。
4. **两条路径 offset 不等价**：非 RPA 扣"设备补偿 + 谱面 offset"；RPA 只扣 `A=outputCorrectionUs`，
   `TimingMap.offsetMs` 从不进 tick 域，仅在 `A == offsetMs == 0` 时重合，**无校验**。
5. `tickScale` 声明单位但 `engine/playback` 全模块不读；`tickScale≠1/1` 时 horizon 静默违例。

死字段与矛盾：

- `simulationDeltaTimeMs` 无求值消费方（只被校验、摘要哈希、CSV 使用），而 `RUNTIME_SESSION.md`
  声称状态型 System 消费它。
- `timeDiscontinuityId` 三处生产者、两种语义（自增计数 vs 时钟段 token）；文档的
  `onTimeDiscontinuity` 回调全仓不存在。
- `PlaybackState::Empty` 被 transport 发布，却被 `validateSourceClockSample` 拒绝。
- `Stopped` 强制 `positionMs==0`，"停在某位置"不可表达。
- `SourceClockSample` 允许负值 pre-roll，`ClockFramePosition` 拒绝 `frame<0` —— RPA 无法表达负 chart time。
- offset 在 6 处独立持有（TimingMap、PlaybackContentInfo、MainMusicSourceView、
  PresentationCandidate、RuntimeTimeline、ChartClock）。
- 单调性三处独立守卫；`ClockObservation.source/deviceMixed` 只校验不使用；
  `RuntimeFrame` 有两个同形类型。

选项：A 统一 `IClockSource` + 唯一 `Timeline`（破坏 ADR 0029/0032/0046）；B 表示层统一为
"整数 tick + 显式单位"（`RuntimeFrame` 变更 → FrameDigest v4）；C 只删重复语义（死转换、`ChartTick` 接线、
Empty 定义、tickScale 校验）；D 三种模式实现同一接口但保留两份数学（不推荐作终点）。

## 5. 渲染

- 链 A（`RenderBackend::renderFrame`）：全仓**唯一生产调用** `player_app.cpp:284` 启动静帧（空场景清屏），
  **零测试** **[核]**；`renderFrame` 定义 `open_gl_backend.cpp:569`。
- 链 B（`presentation_renderer` + OpenGL 适配器）：Player 正式帧、execution host、headless（17 个测试文件）。
- 半入口：`renderPresentationFrame`（`open_gl_presentation_frame.cpp:54-59`）绕过 `frameSubmitted_` 与
  token 校验，只被 `player_smoke.cpp:339,346` 使用 **[核]**。
- 同语义双份：`sdlError`、`requireMainThread`、shader 编译/链接、`DebugVertex`（两个同布局 struct）、
  调试行校验（三份）、清屏与颜色校验、GL 错误检查、`SwapWindow`（三处）。
- 规模：A 独占约 350 行；B 中立层 1,306 + 适配器呈现子集 2,371 + Player 适配 169。
- 功能对等：A 是 B 的真子集，唯一不可替代的是"没有 active presentation 时清一帧"。
- 选项：1 只留 B、把 A 降级为 `clearSurface`（约 -130 行）；2 删 B（不可行，回退 S6-D2）；
  3 两入口保留但共享 `gl_common`（约 -180~220 行）；4 另删 `renderPresentationFrame`（约 -40 行）。
  组合 1+4 推荐。

## 6. 会话层

- `RuntimeSession` 2,707 行/8 文件；无常量表，用 `world_` 是否为空隐式表达
  （`runtime_session.cpp:374-377`）；`RuntimeSessionKind::StudioPreview` 只有测试使用。
- `PlaybackSession` 5,742 行 + 公共头 488；对 RuntimeSession 只调 **9 个方法、12 处**
  （`prepare_stages.cpp:402,419,483`、`playback_session.cpp:155,315,361,367,556,615`、
  `gameplay.cpp:382`、`realtime.cpp:491`）；`reload/configureDebug/withSnapshot/withWorld/applyBaseProperty`
  在 playback 层调用数为 0。
- `execution_host` 6,835 行/25 文件；`HostState` 7 值 + 关闭阶段 30 值；对 RuntimeSession/World/entt
  **零引用**（唯一越界是 `host_candidate.cpp:2` 直 include `engine/playback/src/replay_identity.hpp`）。
- 重复职责（11 项）：prepared 构建、原子提交、active/candidate 切换、generation/staleness、
  诊断投影、所有权/线程检查、重入 RAII 守卫、`matchingStep`（`playback_session_execution.cpp:20-31`
  与 `host_candidate.cpp:7-18` 逐字相同）、token 铸造、presentation revision 时钟、错误码翻译表。
- "哪个 session 代还活着"被摊成 ≥12 个 u64 字段，散在 `ExecutionFence`/`StepTarget`/`HostContext`/`State`。
- `SessionState::Failed` 全仓**只被参考宿主读取、从无写入** **[核]**。
- 公开声明随 `CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE` 改变（47/90 方法），与 ADR 0042 S6-D01 冲突。
  2026-11-17 逐行复核修正：`playback_session.hpp` 共 488 行，宏块位于 24-28/279-381/465-475；
  OFF 侧 39 条方法声明，ON 侧 51 条（49 个唯一方法名），两侧零重名——即同一安装头给出两个类定义。
  原先的 47 是统计口径差异，取复核值 39/51（49 唯一）。
- 选项：A 合并为单一 `SessionKernel`（撞 4 个 cmake 门禁 + ADR 0030/0033）；B 保留门面、共享内核（推荐）；
  C 只统一事务与诊断语义（低风险但结构不变）。

## 7. 文档与注释

体量：721 文件 / 99,196 行 / 9.48 MB **[核]**（当前 722 文件，含本记录）。

| 目录 | 文件 | 行数 | 占比 |
| --- | --- | --- | --- |
| stage_reports | 475 | 48,094 | 48.5% |
| stage_plans | 72 | 15,436 | 15.6% |
| proposals | 47 | 11,514 | 11.6% |
| formats | 21 | 9,019 | 9.1% |
| archive | 19 | 6,240 | 6.3% |
| adr | 48 | 3,005 | 3.0% |
| api | 13 | 2,538 | 2.6% |
| guides | 8 | 1,482 | 1.5% |
| architecture | 8 | 1,069 | 1.1% |

合同类 17.3%，过程类 70.3%（比例 1:4.1）；`stage_reports/stages` 单目录 41.7%。

重复承载（字面量计数）：诊断码类别表 21 份；Packed 预算 63/102 份；SDK 版本 48 份；
FrameDigest 49 份；候选入口名 11 份；`iterationIndexPlusOne` 5 份；
"当前状态只以 CURRENT_STATUS 为准"50 份逐字相同；"权威计划"头部块 15 份。

可机械断言：403 条抽样（22 份合同文档），349 条可解析 = 86.6%；**5/22 份含已确认假陈述**：

1. `GAMEPLAY_V2_SPEC.md:2587`、`GAMEPLAY_V2_ABI.md:1208` → 引用 `tools/check_stage6_a2.py`，
   实际在 `tools/verification/` 下。
2. `CHART_FORMAT.md:427` → 引用不存在的 `docs/stage_plans/stage_2_implementation_plan.md`。
3. `MATERIAL_SHADER.md` → 诊断码 `playback.presentation.shader.stage_unsupported` 全仓 0 命中。
4. `realtime-host-boundary.md:34` → 9 个 API 名（acceptCapture/acceptPrefix/…）全仓 0 命中。

注释抽样：442 个 engine 文件、275 条行为性断言行，核对 12 条，**推翻 4 条、证实 8 条**。

推翻：

1. `world.hpp:3` "thread-safe wrapper" —— `engine/world` 内 mutex/atomic/lock_guard 命中 **0 [核]**，
   下一行自陈 ThreadChecker 仅 Debug 断言。
2. `audio_transport.hpp:63-64` "returns a complete sample without allocating" —— 8 次自旋失败后直接返回陈旧缓存。
3. `transform_system.cpp:438-463` "Atomic on validation failure" —— 先 remove 再 emplace，异常后不回滚 registry。
4. `execution_kernel_archive.cpp:329-331` 声称 O(1)/O(目录) —— 实际每条记录整份 `RuntimeState` 复制。

证实：`limits.hpp` 可收紧不可放宽、`chart_loader.hpp` 未知格式不猜、`shared_sequence` 页指针优先比较、
`gameplay_graph.cpp` 无递归、`execution_kernel_advance.cpp:436` 故障零分配、`asset_database` 不可变、
`playback_session.hpp:11-13` 跨线程 terminate、`gameplay_graph_reference.hpp:167` 引用已排序。

另有 5 个模块有 "owner thread" 断言但模块内 `ThreadChecker` 引用为 0（audio、audio_sdl、content、
player_support、shader）——"未强制"而非"已证伪"。

## 8. 公共 API 面

- 安装面：23 个手写安装头 / 2,105 行；`playback` 9 个/1,158 行（55%），candidate 构建 +3 个/+660 行。
- `PlaybackSession` 90 条 `[[nodiscard]] auto` 声明，47 条只在 candidate 构建存在；
  `materializePresentation` 无宏保护 **[核]**。
- 约 96 个公共类型（46%）无 app/external/reference_host 消费者。
- 完全死码：`ShaderStage`（**仅声明处 1 处命中 [核]**）、`RendererProfileKind`、`GameplayFactId`、
  `GameplayObservationTick`、`GameplayRawTimestamps`、`GameplayCapabilityRecord`。
- Host override 6 类型 + `hostPropertyBit` + `acquireHostOverride`：只在 5 个测试文件使用 **[核]**，
  `app/`、`examples/`、`tests/external/` 为 0。
- `SessionState::Failed` 死状态（同 §6）。
- 泄漏：`detail::*Storage` 作为公有类私有成员进入对象布局；`CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE`
  是 PUBLIC 编译定义而头文件内容随宏改变；`GuardedResult` 等内部工具在公共 `result.hpp` 里。
- 版本副本：`CuexisVersion.cmake`（权威）、`vcpkg.json`（受强校验）、生成头、CMake 包版本、
  包变量、`project()` 版本、reference_host 下限+上限、2 个门禁脚本硬编码、2 处测试断言；
  `0.9.1` 在 6+ 处手写，参考宿主硬拒绝 `>=0.10.0`。
- 选项：A candidate 类型移出安装树（低风险）；B 删死类型（需 0.9.x 内 ABI 清理）；
  C 常量下沉（零 ABI 成本）；D 按 capability 拆子库（门禁成本最高）；E `RuntimeTimeline` 降级
  （须同步 `cmake/VerifySharedExports.cmake:59`）。

## 9. owner 抽查记录

已核实（10 项）：当前版本 `26.11.16-1` 与 SDK `0.9.1`；`CURRENT_STATUS` 版本/分支/PR 过期；
`docs/formats/README.md:40` Packed "未实现"；`packed_chart_tables_writer.cpp:557` CRC 只算 92 字节；
`input.cpp:182-204` 两个诊断码互换；`playback_session.hpp` 的 `materializePresentation` 无宏；
`presentation_parse.cpp:404-414` 要求 profile 列入 `requiredHostExtensions`；
`renderFrame` 唯一调用点 `player_app.cpp:284`；`engine/chart/CMakeLists.txt:6-60` 无条件；
`world.hpp:3` 线程安全声明与 0 个 mutex；`ShaderStage` 仅声明处；`Failed` 无写入；
`audio::HostClock` 仅测试构造；候选宏 65 处；`app/execution_host` 6,835 行。

推翻的子代理结论（2 项）：

1. "`ChartTick` 全仓零构造"——**错**，`ChartTick` 在 `gameplay_graph_phase.hpp:56`、
   `gameplay_author.hpp:87,148` 等 34 处使用 **[核]**。
2. "`acquireHostOverride` 全仓 0 引用"——**不准确**，5 个测试文件在用；宿主消费者确为 0 **[核]**。

计数口径差异：`tests/pd/execution_memory_observer.cpp` 有 910/917 两个数字，
`Measure-Object -Line` 不计空行故给 910；本仓取 917（`Get-Content .Count`、Python、LF 一致）**[核]**。

## 10. 复核指引（上下文压缩后如何快速重验）

只读即可重算的关键数字：

```powershell
# 候选宏数量（应为 65）
(Select-String -Path (Get-ChildItem engine,app,tests -Recurse -File -Include *.cpp,*.hpp,*.h,*.txt) `
  -Pattern 'CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE' | Measure-Object).Count

# execution_host 规模（cpp/hpp 物理行，应为 6835）
(Get-ChildItem app\execution_host -Recurse -File -Include *.cpp,*.hpp |
  ForEach-Object { (Get-Content $_.FullName).Count } | Measure-Object -Sum).Sum

# legacy 渲染调用点（应只有 player_app.cpp:284）
Select-String -Path engine\*\src\*.cpp,app\*\src\*.cpp -Pattern 'renderFrame\s*\('

# chart 是否无条件编译候选源（应无 if()）
Select-String -Path engine\chart\CMakeLists.txt -Pattern 'packed_|cxt_v2_|candidate_'

# 文档事实与门禁
python -B tools/check_docs.py
```

判断口径提醒：

- "默认不可达"= 默认 preset 下没有从 Player/Playback 出发的调用边，不等于代码未被编译。
- 行数为物理行（含空行/注释），不是逻辑行；TEST_CASE 计数为正则近似。
- 统计必须排除 `out/sources/r4/*`（3–4 份历史源码快照），否则虚增数倍。
- 子代理结论未标 **[核]** 的，在据其删除/合并代码前必须先补证。

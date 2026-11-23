# 架构与实现研究（2026-11-17）

<!-- cuexis-meta
id: EVIDENCE-stages.stage-07-rr.2026-11-17-architecture-research
status: evidence
authority: none
date: 2026-11-17
-->

状态：active；研究结论，未包含任何重构实施

更新日期：2026-11-17

返回[Stage 7-RR 报告索引](../README.md)；计划见
[Stage 7-RR 计划](../../../../stage_plans/active/stage-07-rr/plan.md)，
问题登记见[发现登记册](../../../../stage_plans/active/stage-07-rr/findings.md)。
当前进度唯一以 [CURRENT_STATUS](../../../../CURRENT_STATUS.md) 为准。

本文件是结论性摘要与减重方案；逐模块的原始证据、量化表、owner 抽查与推翻记录、
以及上下文压缩后的复核命令，保存在[架构与实现探索证据记录](2026-11-17-architecture-evidence.md)。

## 0. 范围、方法与证据状态

- 基线：`aac83e5e`（分支 `stage-7.5`，2026-11-16），SDK API `0.9.1`，日期版本 `26.11.16-1`。
- 方法：8 个并行无上下文子代理分别研究依赖分层、双产品线、格式线、时间源、渲染路径、会话层次、
  文档与注释、公共 API 面；全部只读（不构建、不跑 CTest、不执行 git 写操作）。
- owner 抽查 10 处关键结论，另推翻 2 条子代理结论（见 §9）。
- 证据标注：标 **[核]** 的条目 owner 亲自读过实现或做过统计；其余为子代理报告，未逐条独立复核。
- 本报告只描述现状与选项，**不做任何减重决定**；决定与实施排期归计划批次 RR-3。

## 1. 一句话结论

臃肿不在"模块数量"，而在**一条默认关闭的候选支线被当成完整的第二产品线全量编译、测试与门禁挂接**，
以及**同一个会话/事务语义在三层各写一份**；格式线与时间源的真实数量比口头模型更多，
但它们的代码量远小于前两项。文档的体量问题主要是过程证据（70.3%），不是合同本身；合同的
可机械断言命中率 86.6%，但仍有 22.7% 的合同文档含至少一条被证实的错误断言。

## 2. 实测结构（不是文档声称的结构）

### 2.1 分层与依赖

实测分层（依据 CMake 链接边）：

```text
L0 core
L1 audio / filesystem / json_support / world / content / shader_cache
L2 project / assets / chart / animation / behavior / judgement / shader / gameplay(INTERFACE)
L3 cxc / render / runtime / gameplay_packed
L4 playback / debug
L5 presentation_renderer  (+ app/execution_host)
L6 platform_sdl / audio_sdl / render_opengl
   player（可执行）   tools/*（旁路）
```

规模（物理行）：`engine` ≈ 86.6k，`engine+app+tools` ≈ 114.4k **[核]**。

结构性违规边（全部来自 CMake，非风格问题）：

| 编号 | 违规 | 证据 |
| --- | --- | --- |
| V1 | 可选渲染适配器 PUBLIC 依赖 SDK：`render_opengl → playback + presentation_renderer`，适配器无法脱离 SDK 复用 | `engine/render_opengl/CMakeLists.txt:27-31` |
| V2 | "后端中立渲染器"仍依赖 `playback`，实际存在 3 个 render target 而非 2 条路径 | `engine/presentation_renderer/CMakeLists.txt:12` |
| V3 | 工具反向消费 SDK：`chart_candidate → playback`；`gameplay_author` 把 `engine/playback/src/gameplay_entry.cpp` 直接编进自己 | `tools/chart/chart_candidate/CMakeLists.txt:5`、`tools/gameplay/gameplay_assembler/CMakeLists.txt:9-13` |
| V4 | 消费方读被消费方私有源码：`playback` 把 `engine/runtime/internal` 加进 include；`gameplay_packed` 加 `../judgement/src`、`../chart/src` | `engine/playback/CMakeLists.txt:53,101`、`engine/gameplay_packed/CMakeLists.txt:18-21` |
| V5 | 死重量占分层位：`debug`（70 行，0 产品消费者）、`particles`（0 行，未接入）、`gameplay`（9 行 tags 仍被 `runtime` 依赖） | `engine/debug/CMakeLists.txt:15`、`engine/CMakeLists.txt`（无 particles）、`engine/runtime/CMakeLists.txt:41` |
| V6 | 默认 OFF 的候选分支仍全量编译：`judgement` 20.5k 行在 `CUEXIS_ENABLE_CHART_V5_CANDIDATE=OFF` 时**没有任何产品链接边**，但仍被无条件 `add_subdirectory` | `CMakeLists.txt:44-46`、`engine/CMakeLists.txt:24-25`、`engine/playback/CMakeLists.txt:47-54` |

`cuexis_internal/` 有两个互不相识的根（core 与 render_opengl），且 core 的内部头承载了 judgement 的执行契约
（`execution_diagnostics.hpp` 的 `Part` 枚举是被判定内核与 playback 共同依赖的阶段名），
形成"通过 core 内部头对齐测量契约"的隐式耦合。

### 2.2 四条轴的真实计数

| 轴 | 口头模型 | 实测 | 证据 |
| --- | --- | --- | --- |
| 产品线 | 2 | **2.5**：中立 Player 链 + 旗标化 execution host 链 + 未接入的 Studio | 见 §3.1 |
| 格式线 | 5 | **5 组 chart 加载器 + cxc + project/asset index**：v1–v3、v4、v5/Packed、CXT v1、CXT v2；另有 Capsule v2/3/4、Graph v1 | 见 §3.2 |
| 时间 | 3 | **3 种源模式 + 4 个时间实现**：PlayerClock、RealtimeClock、RuntimeTimeline、TimingMap/Timebase；`audio::HostClock` 只有测试实例化 | 见 §3.4 |
| 渲染 | 2 | **3 个 target**：`render`（契约）、`presentation_renderer`（中立）、`render_opengl`（GL）；实际上只有 1 条全功能链 + 1 条半链 | 见 §3.5 |

修正口头模型的意义：真正需要"减"的不是 5 条格式线全部，而是**默认不可达的那部分**；
时间问题的本质不是"三套时钟"，而是**同一换算写了两份、offset 语义在两条路径上不等价**。

## 3. 臃肿的真实来源（按可减重量排序）

### 3.1 V5 / candidate 支线：默认 OFF，却按第二产品线维护

- 开关：`CUEXIS_ENABLE_CHART_V5_CANDIDATE` 默认 OFF（`CMakeLists.txt:44-46`）。
- 但默认构建仍编译 `judgement`（20,518 行）、`gameplay_packed`（4,821 行，**零链接**，
  `engine/playback/CMakeLists.txt:47-54` 是唯一入口且在 flag 内）；`chart` 里的 packed/cxt_v2/candidate
  也照编译（§3.2）。
- execution host 侧：`app/execution_host` 6,835 行 **[核]**（25 文件）+ `app/player/src/execution_host*` 约 2.9k 行；
  只为 `--execution-test-only` 服务（`app/player/src/player_app.cpp:177-180`）。
- 依赖它的测试：`tests/rpa` 10,208 行、`tests/pd` 10,362 行、`tests/player` 中 3,042 行。
- 条件编译：`CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE` 65 处 **[核]**、`CUEXIS_RPA_OBSERVATION_TEST_ONLY` 92 处、
  `CUEXIS_PLAYBACK_TRACE_HARNESS` 30 处。
- 合计约 **27k 行**（engine 候选执行面 ≈4.4k + execution host ≈9.6k + 相关测试 ≈20k 中的大部分），
  且**公开类声明随宏改变**（`playback_session.hpp:279-381` 的 47/90 个方法仅在 candidate 构建存在），
  与 ADR 0042 S6-D01"开关只控制实现、不用条件编译改变公开类定义"直接冲突。

这是单点最大杠杆：它同时解释了"2 条产品线"、公开面 55% 的体量与 20k 行测试为何存在。

### 3.2 chart 单库塞进 5 条格式线，40.9% 默认不可达

`engine/chart/CMakeLists.txt:6-60` 把 `packed_*`（12 文件）、`cxt_v2_*`（5 文件）、`candidate_*`（3 文件）
放进**无条件的** `target_sources` **[核]**，即默认构建照编译。按文件名分桶：

| 线 | 行数 | TEST_CASE | 默认 Player 端到端消费 |
| --- | --- | --- | --- |
| Chart v1/v2/v3 | 5,162 | ~47 | 是（唯一默认路径） |
| Chart v4 | 4,030 | ~48 | 是 |
| Packed + v5 Core | 5,344 | ~75 | **否** |
| CXT v1 | 282 | 7 | 是 |
| CXT v2 | 1,958 | 17 | **否**（且**无 Writer**） |
| CXC v1 | ~2,900 | 66 | 是 |
| Capsule v2/3/4 + Graph v1 | 4,821 | 51 | **否**（目标总是构建、无人链接） |
| Project + Asset Index | ~1,530 | 37 | 是 |

半条线：CXT v2 无 Writer、Playback 零引用；Asset Index 只有 Reader；7 个生产 Schema **没有任何 loader
调用** `validateAgainstSchema`（`engine/json_support/include/cuexis/json/schema.hpp:6` 自述
"No loader calls this adapter yet"）；同一 CXPk5 信封有两套独立写实现
（`packed_chart_tables_writer.cpp:503-541` 与 `gameplay_capsule_emit.cpp:122-174`）**[核]**（前者 CRC 已确认只算 92 字节）。

### 3.3 会话层：一个内核 + 三份事务语义

- `RuntimeSession` 2,707 行，但 playback 对它只有 **9 个方法、12 处调用点**，直接管道 ≤40 行；
  `reload/configureDebug/withWorld/applyBaseProperty` 在 playback 层调用数为 0（只被 tests 消费）。
- `PlaybackSession` 5,742 行 + 公共头 488 行，主体是 prepare 管线、portable presentation（3,916 行）、
  gameplay/realtime 与 fence。**它不是 RuntimeSession 的门面**，而是并列的第二套内核。
- `execution_host` 6,845 行，状态机 `HostState`（7 值）+ 关闭阶段 **30 值**。
- 同语义重复（子代理逐一对照）：prepared 构建、原子提交、active/candidate 切换、generation/staleness 判定、
  诊断投影、所有权/线程检查、重入 RAII 守卫、`matchingStep` 谓词（`playback_session_execution.cpp:20-31`
  与 `host_candidate.cpp:7-18` 逐字相同）、会话 token 铸造、presentation revision 时钟、错误码翻译表。
- "哪个 session 代还活着"被标量化成 ≥12 个 u64 字段，散在 `ExecutionFence`、`StepTarget`、`HostContext`、
  `State` 四个结构里。
- `SessionState::Failed` 全仓**只有读取、从无写入** **[核]**（`examples/reference_host/src/host_runner.cpp:40,311`
  在读，`engine/` 内 0 写入）——宿主可分支却永不可达。

### 3.4 时间：4 个实现、3 种模式、重复换算

- 表示层：`playback::RuntimeFrame`（同形类型在 `runtime::RuntimeFrame` 还有一份）、`ClockFramePosition`、
  `ClockObservation`、`ClockTicks`、`judgement::Tick/TickSpan`、`chart::RationalBeat` 与
  `judgement::RationalBeat`（互不引用、各自 `Result` 构造）、`audio::SourceClockSample`、`FrameEnvelope.presentationTick`。
- 重复换算：采样帧→us 有 `llround`（`sdl_audio_impl.hpp:170`）与 half-even（`realtime_clock.cpp:63-71`）两份；
  `sourceMs−offset→chartTimeMs` 有两份实现（`runtime_timeline.cpp:43` 在用，
  `TimingMap::audioTimeMsToChartTimeMs` 产品无调用）；反向 seek 有两份。
- **两条路径的 offset 不等价**：非 RPA 路径扣"设备补偿 + 谱面 offset"，RPA 路径只扣 `A=outputCorrectionUs`，
  `TimingMap.offsetMs` 从未进入 tick 域；两者只在 `A == offsetMs == 0` 时重合，**无代码校验该等式**。
- `simulationDeltaTimeMs` 无求值消费方（只被校验、哈希与 CSV 使用），而 `RUNTIME_SESSION.md` 声称
  状态型 System 消费它。
- `timeDiscontinuityId` 有三处生产者、两种语义（自增计数 vs 时钟段 token）。
- `audio::HostClock` 只有测试实例化 **[核]**；`PlayableState::Empty` 被 transport 发布却又被
  `validateSourceClockSample` 拒绝，接口可返回自身校验器拒收的样本。

### 3.5 渲染：一条全功能链 + 一条只剩清屏的半链

- 链 A（legacy `RenderBackend::renderFrame`）：全仓**唯一生产调用**是 `app/player/src/player_app.cpp:284`
  的启动静帧（空场景清屏），**零测试** **[核]**。
- 链 B（`presentation_renderer` + OpenGL 适配器）：Player 正式帧、execution host、headless 测试
  （17 个测试文件）全部走它，功能是 A 的严格超集。
- 半入口：`renderPresentationFrame`（`open_gl_presentation_frame.cpp:54-59`）绕过 `frameSubmitted_` 与
  token 校验，只被 `player_smoke.cpp:339,346` 使用 **[核]**。
- 同语义双份：`sdlError`、`requireMainThread`、shader 编译/链接、`DebugVertex`（两个同布局 struct）、
  调试行校验（三份）、清屏与颜色校验、GL 错误检查、`SwapWindow`（三处）。
- 规模：链 A 独占约 350 行；链 B 中立层 1,306 + 适配器呈现子集 2,371 + Player 适配 169。

### 3.6 公共 API 面：一半没有外部消费者

- 安装面：23 个手写安装头 / 2,105 行；`playback` 独占 1,158 行（55%），candidate 构建再加 660 行。
- `PlaybackSession` 90 条 `[[nodiscard]] auto` 声明中 47 条只在 candidate 构建存在；`materializePresentation`
  无任何宏保护 **[核]**；`detail::*Storage` 作为公有类私有成员进入对象布局与 ABI。
- 约 96 个公共类型（46%）无 app/external/reference_host 消费者；完全死码包括 `ShaderStage`、
  `RendererProfileKind`、`GameplayFactId`、`GameplayObservationTick`、`GameplayRawTimestamps`、
  `GameplayCapabilityRecord`；`ShaderStage` 仅声明处 1 处命中 **[核]**。
- Host override 簇（6 类型 + `hostPropertyBit` + `acquireHostOverride`）只有测试在用 **[核]**。
- 版本权威分散在 10+ 处手写副本，只有 `vcpkg.json` 受 configure 期强校验；参考宿主硬拒绝 `>=0.10.0`，
  任何 minor 升级会同时打断参考宿主与 4 个 `find_package` 门禁。

### 3.7 死重量清单（可直接删或下沉）

| 对象 | 规模 | 证据 |
| --- | --- | --- |
| `engine/particles` | 0 行，未接入 | `engine/CMakeLists.txt` 无该子目录 |
| `engine/debug` | 70 行，0 产品消费者 | `engine/debug/CMakeLists.txt:15` |
| `engine/gameplay` | 9 行 tags，仍占一个被依赖位 | `engine/runtime/CMakeLists.txt:41` |
| `engine/content` | 90 行门面，已进安装树，职责与 `playback.content_provider` 重叠 | `engine/content/CMakeLists.txt:19` |
| `engine/player_support` | 1,215 行，只被 `app` 使用，不是引擎能力 | `engine/player_support/CMakeLists.txt:16` |
| `app/studio` | 0 源文件，死 CMakeLists | `app/studio/CMakeLists.txt:1-2` |
| `TimingMap::audioTimeMsToChartTimeMs/chartTimeMsToAudioTimeMs` | 死转换 | `timing_map.hpp:55-56` |
| `packed_chart_primitives` 的 GridDelta/descriptor codec | 无调用者 | `packed_chart_primitives.cpp:234-331` |

## 4. 一致性问题（文档与注释）

- 文档体量：721 个文件 / 99,196 行 / 9.48 MB；合同类 17.3%，**过程类 70.3%**（`stage_reports` 单目录 41.7%）。
  "臃肿"主要是历史证据堆积，不是合同膨胀。
- 重复承载：诊断码类别表 21 份、Packed 预算 63 份、SDK 版本 48 份、FrameDigest 49 份、
  导航样板"当前状态只以 CURRENT_STATUS 为准"50 份逐字相同。
- 可机械断言命中率 86.6%（403 条抽样，349 条可解析）；**22 份合同文档中 5 份含已确认的错误断言**，
  例如 `GAMEPLAY_V2_SPEC.md:2587` 与 `GAMEPLAY_V2_ABI.md:1208` 引用不存在的 `tools/check_stage6_a2.py` 路径、
  `CHART_FORMAT.md:427` 引用不存在的计划文件、`MATERIAL_SHADER.md` 的诊断码全仓 0 产出、
  `realtime-host-boundary.md:34` 把设计选择写成已存在的接口名（9 个 API 名全仓 0 命中）。
- 代码注释：12 条抽样中 4 条被推翻（33%）。最典型是 `world.hpp:3` "thread-safe wrapper"——
  `engine/world` 内 `std::mutex/std::atomic/lock_guard` 命中数 **0** **[核]**，下一行自己又写"ThreadChecker 仅 Debug 断言"。
  规律：文件头 banner 失真率最高（线程/并发/复杂度/原子性断言），函数体内不变量注释基本可信。
- 另有 5 个模块存在"owner thread"断言但模块内 `ThreadChecker` 引用为 0（audio、audio_sdl、content、
  player_support、shader），属"未强制"而非"已证伪"。

## 5. 门禁盲区

- `VerifyArchitecture.cmake` 不检查 `cuexis_internal/` 越界与跨模块 `src/` include，所以 V4 类耦合全部合法通过。
- 不检查安装集合本身（除了硬编码禁止 `presentation_renderer`/`player_support` 进入安装）。
- 不检查文档事实（版本、目标名、预算数值、诊断码是否存在），所以 22.7% 的失真文档照常通过。
- `cuexis_shader_cache` 在默认 GL 构建下没有任何测试目标。
- `CUEXIS_ALLOCATION_LIFETIME_OBSERVER` 的门禁在 `CUEXIS_LIBRARY_TYPE` 定义之前执行，打开即 FATAL_ERROR。
- 只盘点 `^cuexis_` 前缀 target，换前缀的新 target 可绕过 active-target 与 warnings 登记。

## 6. 减重选项（供 RR-3 决策，不在本报告选定）

| 选项 | 内容 | 减重 | 风险 |
| --- | --- | --- | --- |
| R1 构建隔离（最低风险） | 把 packed/v5/CXT v2/Capsule/Graph 与 execution host 全部移出默认目标，改成 flag 下的独立 target；默认测试不再跑候选 126 个用例 | 默认构建少约 12.1k 行编译、少 20k 行测试挂接 | 低；不删任何代码，兼容承诺不动 |
| R2 候选支线降级 | 在 R1 之上，把 candidate 公开声明改为不随宏改变的稳定面（或整体移出安装树），删除死类型与 host-override 簇 | 公共面 2,105 → 约 1,890 行、-13 类型 | 中；0.9.x 内一次性 ABI 清理 |
| R3 会话收敛 | 抽 `SessionKernel`，`PlaybackSession`/`SessionOwner` 变薄适配；三份 fence 合成一份 | 结构性，约 2–4k 行重复语义 | 中高；撞 4 个 cmake 门禁 + ADR 0030/0033 修订 |
| R4 时间收敛 | 统一"整数 tick + 显式单位"表示，合并重复换算与 offset 路径，`simulationDeltaTimeMs` 降为诊断值 | 约 500 行 + 消除路径不等价 | 中；`RuntimeFrame` 形状变更 → FrameDigest v4 |
| R5 渲染收敛 | 删 `RenderBackend::renderFrame`/`RenderFrame`，启动静帧改为 `clearSurface`；删 `renderPresentationFrame` | 约 170 行 + 一个公共抽象类 | 低；需 ADR 0041 D5 提前退出 |
| R6 文档减重 | 单一权威表 + 只引用；过程文档移出仓库只留索引；新增 `check_docs_facts.py` | 文档 -40% 以上 | 低，但会破现有 `check_docs` 的硬编码清单，需同步改 |

## 7. 建议的几周路线（可调整）

```text
第 1 周  R1 构建隔离 + 门禁补盲（cuexis_internal/跨 src include/安装集/文档事实）
第 2 周  R5 渲染收敛 + R2 公共面死码清理（两者风险低、立即减面）
第 3-4 周 R3 会话收敛（先定 ADR 修订，再动 fence/token）
第 5 周  R4 时间表示统一（牵动 FrameDigest，需独立版本决定）
第 6 周  R6 文档减重与过程文档外迁
```

R1 必须最先做：它把"候选代码是否参与默认图"从人肉记忆变成构建事实，
后续所有删除动作才有可信的回归面。

## 8. 需要 owner 决策的分叉

1. **candidate 支线是保留、冻结还是删除**：保留则必须回答"谁在生产中使用它"（当前唯一驱动者是 test-only 装配）；
   冻结则 R1 即可；删除则需处理 20k 行测试与 RPA/PD 证据链。
2. **v1–v3 兼容窗口是否继续**：删除它们能减约 5.2k 行，但直接违反
   `chart-format-update` 计划的"兼容窗口不缩短"承诺。
3. **公共 API 的 minor 升级**：删死类型/改 `RuntimeFrame` 形状都需要 `0.10` 级别的版本动作，
   且会同时打断参考宿主与 4 个 `find_package` 门禁。
4. **文档过程的归档位置**：移出仓库会破现有门禁清单，需要一次政策级决定。

## 9. 证据边界与已推翻的结论

- 未做：任何构建、CTest、hosted、GPU/设备实测；运行时行为（关闭预算、丢帧频率、真实设备时钟误差）未验证。
- 行数为物理行（含空行与注释），不是逻辑行；`tests` 的 TEST_CASE 用正则近似。
- 仓库内 `out/sources/r4/*` 有 3–4 份历史源码快照，所有统计已排除，否则会虚增数倍。
- 推翻的子代理结论：
  1. "`ChartTick` 全仓零构造"——错，`ChartTick` 在 `gameplay_graph_phase.hpp:56`、`gameplay_author.hpp:87,148`
     等 34 处被使用 **[核]**；
  2. "`acquireHostOverride` 全仓 0 引用"——不准确，它在 5 个测试文件中被使用，但 `app/`、`examples/`、
     `tests/external/` 确实为 0 **[核]**。
- 保留的近似：`judgement` 的"消费者=1"指链接消费者；V5=OFF 时该边不存在，这一点已单独说明。

# Cuexis 问题清单（Findings）

状态：evidence

更新日期：2026-11-17

<!-- cuexis-meta
id: REPORT-EXTERNAL-FINDINGS-2026-11-17
status: evidence
authority: none
-->

> 来源：外部只读审查报告（`C:\Users\Zilch\Downloads\finding.md`，2026-11-17），基线 `master@aac83e5e`，
> 共 59 条（S1 七条），未运行构建/CTest/GPU 测试。交叉核对见
> [Stage 7-RR findings §8](../../../stage_plans/active/stage-07-rr/findings.md)。
> 本文件是**证据**，不是权威；条目未经仓库复核前不得作为修复依据。

状态：active  
更新日期：2026-11-17  
审阅基线：`master` @ `aac83e5e`  
方法：只读代码与文档审阅。没有运行构建、CTest 或 GPU 测试；性能数字引用仓库内已有证据，已标注来源。  
严重度：**S1** 数据丢失、错误结果、崩溃或挂死；**S2** 契约错误、明显降级、会持续制造 bug；**S3** 维护性、误导、架构债；**S4** 局部风格或小问题。

## 摘要

| 类别 | 条数 | 说明 |
| --- | --- | --- |
| C 契约与假保证 | 7 | 代码或文档声称有保证，实际没有 |
| S 结构性缺陷 | 7 | 设计会持续制造 bug，修一处不够 |
| D 局部缺陷 | 16 | 位置明确，改函数或改几行 |
| DOC 文档漂移 | 9 | 文档与代码不一致，或结构上不可维护 |
| P 性能与内存 | 5 | 机制已确认，绝对值需测量 |
| T 测试与验证 | 5 | 覆盖缺口，导致问题被隐藏 |
| A 架构与流程 | 10 | 组合数、产能、门禁不对称 |
| **合计** | **59** | 其中 S1 七条 |

### 七条 S1

1. **C-01** GameplayOnly 被自己的 `valid()` 判无效，文档主路径不可用。
2. **C-03** `ChartWriter::write` 静默把 v4 投影成 v3，丢字段。
3. **S-01** `PlaybackSession` 三份手工状态清单，已经漂移过（D18）。
4. **D-03** Close 超预算后，`ExecutionHostLifetime` 析构永久自旋。
5. **D-13** `find_if` 未判 `end()` 就解引用，当前不可达，一旦资源表不同步就是 UB。
6. **D-18** `unload()` 漏清 5 个候选字段，跨 unload 保活。
7. **D-19** `PreparedPlayback::State::owner` 是裸指针，Session 先销毁会 UAF。

### 怎么用这份清单

- 先修 S1，再修“假保证”，最后才是重构。
- 每条都有位置；动手前先确认当前 HEAD 没变。
- 标“需测量”的条目，不要直接当结论对外说。
- 修复后把条目改成 resolved，不要删，保留决策记录。

---

## C. 契约与假保证

这一组最危险，因为它们同时骗过 review 和 CI：看起来有保证，实际没有。

### C-01 GameplayOnly 被自己的 `valid()` 判无效

**严重度**：S1  
**位置**：`engine/playback/src/playback_session_prepared.cpp:109-111`；`engine/playback/src/playback_session_prepare.cpp:337-342`

`valid()` 要求 `state_->runtimeSession != nullptr`。GameplayOnly 分支故意不建 RuntimeSession，于是候选被判无效，`contentInfo()`、`semanticIdentity()`、`presentationManifest()` 全部拒绝。`commit()` 不调用 `valid()`，测试也只测 commit，所以缺陷被掩盖。

**影响**：文档把 GameplayOnly 写成安装消费者的主路径，实际第一步查询就失败。  
**建议**：`valid()` 改为 `runtimeSession != nullptr || gameplayContent.has_value()`，补一条 GameplayOnly 的访问器测试。

### C-02 `SessionState::Failed` 不可达

**严重度**：S2  
**位置**：`engine/playback/src/*.cpp`（全模块无赋值点）

枚举和文档都有 `Failed`，但没有任何代码把 `sessionState` 设为它。错误全部走 `Result` 返回，状态机永远停在 Empty/Ready/Running。

**影响**：按文档等待 `Failed` 的宿主会永久等待。  
**建议**：要么实现（不可恢复错误时进入 Failed 并保持），要么从枚举和文档里删掉。

### C-03 `ChartWriter::write` 说谎

**严重度**：S1  
**位置**：`engine/chart/src/chart_writer.cpp:676-685`

`write()` 固定写 `version = 3`，并把 `templates`、`requiredExtensions` 写成空数组，`timing.tempoEvents`、`stops`、`sourceTemplate` 直接丢弃。头文件没有声明这是投影。

**影响**：外部调用者以为是无损往返，实际数据丢失。  
**建议**：改名 `writeV3Projection`，或加显式目标版本参数；补一条 v4 round-trip 测试，把丢字段变成显式错误。

### C-04 恒真的 bit-2 校验

**严重度**：S2  
**位置**：`engine/chart/src/packed_chart_tables_decoder_model.cpp:436`

`componentMask()` 从 `requirements.empty()` 反推 bit 2，而检查只在 requirements 非空时执行，所以条件永远成立。规范要求的“archetype mask ⇔ REQ0 行”双向检查实际不存在。

**影响**：非规范 artifact 会被接受，校验给人假安全感。  
**建议**：用 header 里的 mask 与 requirements 独立比对，补一个反例 fixture。

### C-05 只有测试调用的规范化实现

**严重度**：S2  
**位置**：`engine/playback/src/presentation_extraction.cpp:163`（397 行）；生产路径在 `engine/presentation_renderer/src/draw_command.cpp:24,477-517`

`normalizePresentationFrame` 全树只有测试调用。生产 renderer 另写了一套规范化，深度量化常量也是两份（4096）。

**影响**：presentation 测试验证的是死代码，真实路径没有等价覆盖。  
**建议**：把规范化下沉到共享函数，或删掉 playback 副本，测试改测 renderer 的真实入口。

### C-06 `ConstructionBudget` 每节点重建

**严重度**：S2  
**位置**：`engine/judgement/src/pattern_dfa_node.hpp:52`

`ConstructionBudget budget{measurement};` 写在节点循环内部，注释却说是“一次调用的共享计数器”。

**影响**：实际预算是每节点 2²⁴，整张图没有总量上界。  
**建议**：把 budget 提到循环外，按调用共享；补一个节点数 × 每节点预算的边界测试。

### C-07 `mapBeatToTick` 没有接线

**严重度**：S2  
**位置**：`engine/judgement/src/timebase.cpp:707`

函数实现、测试都有，但生产代码没有调用点。作者 timing 直接给整数 tick，`TimebaseProfile` 只参与校验、身份和 diff，tempo/stop/tickScale 不参与计算。

**影响**：Spec §3.7.2 的“prepare 期精确映射”尚未接线，时间基目前是声明而非机制。  
**建议**：要么接线，要么把 Spec 和头文件降级为“预留”，别让读者以为已经在用。

---

## S. 结构性缺陷

这一组不是某一行写错，而是设计会持续生产 bug。修一条不够，要改结构。

### S-01 `PlaybackSession` 的三份手工状态清单

**严重度**：S1  
**位置**：`commit()` 在 `playback_session_prepare.cpp:481`；`unload()` 在 `playback_session.cpp:320`；prepare 失败路径在 `playback_session_prepare.cpp:548`

同一份状态要在三个地方各清一遍：commit 手写 30 个字段的 move，unload 手写 22 个字段的 reset，prepare 失败再来一份。没有单一 `reset()`，也没有编译期检查。

**影响**：新增字段靠人记三处。D-18 已经证明它会漂移：`unload()` 漏了 `tailTransition`、`executionMapping`、`hostFence`、`acceptedHostControl`、`confirmedHostContext`。  
**建议**：把候选状态收进 `CandidateState`，提供 `reset()`，三处共用；commit 改成整块 move。

### S-02 公共 ABI 被一个宏切成两半

**严重度**：S2  
**位置**：`engine/playback/include/cuexis/playback/playback_session.hpp:279-381`；`engine/playback/CMakeLists.txt:40-47`

打开候选宏后，同一个 `PlaybackSession` 多出 47 个方法，链接闭包多出 judgement 和 gameplay_packed，安装 flavor 变成 experimental，`CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE` 还是 PUBLIC 定义。

**影响**：同一个头文件对应两套符号表；消费方必须整体重建；两套测试、两套文档。  
**建议**：候选方法移到独立 `GameplaySession`，`PlaybackSession` 只留一个访问器；或者干脆承认是两个包。

### S-03 应用层是一个没有纪律的第二运行时

**严重度**：S2  
**位置**：`app/execution_host/session_worker_dispatch.cpp`；`app/player/src/player_execution.cpp`

`session_worker_dispatch.cpp` 是 677 行单函数，41 个 `if constexpr`，39 种命令。`player_execution.cpp` 里同样是几百行的执行函数。两者都没有独立单测，回归靠 `tests/rpa` 集成和设备证据。`ExecutionHostLifetime` 的析构还会自旋。

**影响**：engine 层的边界、事务、校验到 app 层断了；主循环和线程问题只能靠手工。  
**建议**：dispatch 按“队列 / 命令 / 归因”拆三个函数；补假时钟 + headless 的 frame loop 测试；关闭路径加硬上限。

### S-04 三套时间模型，没有强类型隔离

**严重度**：S2  
**位置**：`engine/chart/include/cuexis/chart/timing_map.hpp`（double 毫秒）；`engine/judgement/include/cuexis/judgement/timebase.hpp`（int64 tick）；`engine/playback/include/cuexis/playback/realtime_clock.hpp`（segment + error bound）

表现层用 double 毫秒，判定层用 int64 tick，实时路径又有一套带 segment 的时钟。转换点分散，类型系统不阻止混用。

**影响**：边界错误难定位；C-07 的映射没接线，使“声明的时间基”和“实际用的时间”进一步分离。  
**建议**：chart 边界引入 `ChartTimeMs` 强类型；写一份转换点清单；让编译器拦住 double 和 tick 的直接运算。

### S-05 两套渲染入口

**严重度**：S3  
**位置**：`engine/render/`（233 行，实际只剩 DebugLine）；真正的路径在 `engine/presentation_renderer/`

旧模块还在树里，头文件不安装，但源码可读。新人或新会话容易从 `render::RenderScene` 进入，改错地方。

**影响**：认知负担，误改旧路径。  
**建议**：把 `render` 并进 `presentation_renderer` 或直接删；`debug` 只依赖新的绘制命令。

### S-06 Chart 有五种表示

**严重度**：S3  
**位置**：`ChartDocument`、`ChartRuntime`、`ChartV4SourceDocument`、`ChartV4ResolvedArtifact`、`CanonicalSemanticChart`

每一种都有理由：作者文档、编译结果、v4 源、v4 解析结果、规范化语义图。但每加一个字段，要决定它进哪几种表示、在哪一步转换。

**影响**：字段改动容易漏转；新版本会再增加一种表示。  
**建议**：在架构文档里固定一张“表示与转换点”表；新增表示必须说明替代了哪一种。

### S-07 只加不减

**严重度**：S3  
**位置**：全仓

`render` 模块、legacy wire、Packed 原型、旧版本 loader 都还在。Stage 7 结束时没有删除或合并任何模块。限制只写在注释和文档里，没有删除清单。

**影响**：树越来越大，读的人要自己判断哪些是活的。  
**建议**：每个阶段结束加一张“删除/合并”清单，和新增清单一样是交付物。

---

## D. 局部缺陷

这一组位置明确，改函数或改几行即可。原报告里的 D-11/12/15/16/17/21/23 已分别归入 C-07/C-06/C-01/C-02/C-05/C-04/C-03；D-04/D-05/D-22 归入 DOC-04/DOC-03/DOC-01。这里不重复。

| ID | 严重度 | 位置 | 事实 | 影响 |
| --- | --- | --- | --- | --- |
| D-01 | S2 | `app/player/src/player_app.cpp:171`、`player_execution.cpp:362` | 候选路径把 `openConfiguredPlaybackSource(options)` 调了两次，第一次结果丢弃；与 `player_app.cpp:164-169` 的“exactly once”注释冲突 | 大 CXC/工程二次读盘和校验 |
| D-02 | S3 | `player_control_frame_loop.cpp:371-386` | `renderMicroseconds` 先置 0 传给 hook，之后才赋值 | `first_frame_presented` 日志里的 `render_us` 恒为 0 |
| D-03 | S1 | `app/player/src/execution_host.cpp:71-78`、`execution_host_close.cpp:468-495` | 析构在 `state()!=closed` 时无限 `close()+waitFor(1ms)`；Close 的 1 s 预算首次调用后不再刷新，超预算后每次 `close()` 必抛 | 一次 Close 超预算，进程退出时永久自旋 |
| D-06 | S2 | `app/player/src/player_app.cpp:91` | 无 `#ifdef` 使用 `getenv_s`；Linux headless preset 全部关掉 Player | Player 实际只被 Windows 工具链编译过，跨平台回归没有覆盖 |
| D-07 | S2 | `app/execution_host/capture_route.cpp:467-470` | `pending()` 按值返回 `std::vector<GameplayCaptureDelivery>`，每帧 `flush()` 整份复制未 ack 的积压 | O(帧数 × 积压) 拷贝，是“输入累积”在 app 层的放大点 |
| D-08 | S2 | `app/execution_host/session_worker_dispatch.cpp` | 677 行单函数，41 个 `if constexpr`；`prepared` 的 reset 条件分散在 3 处 | 无法按命令测试，改动易漏 |
| D-09 | S2 | `engine/judgement/src/execution_kernel_restore.cpp:14-55` | `captureState()` 全量复制 projection/coverage/pending/matchers/ingress，并 `flattenLog` 整条日志 | 与文档记录的“118.64 MiB / 255 状态”方向一致（后者实测，此处机制确认） |
| D-10 | S3 | `cmake/VerifyArchitecture.cmake:535-540` | app 侧禁列表只覆盖 6 个文件，`app/execution_host/**` 不在其中 | “worker 不碰 SDL/GL”没有自动校验 |
| D-13 | S1 | `engine/judgement/src/execution_kernel_impl_phases.cpp:80-88` | `find_if` 结果未判 `end()` 即解引用 | 当前不可达；资源表不同步时是 UB |
| D-14 | S3 | `engine/judgement/src/gameplay_graph_render.cpp:292`、`execution_kernel_internal.hpp:35-39` | `sameTimebase` 有两份实现 | 改一处漏一处 |
| D-18 | S1 | `engine/playback/src/playback_session.cpp:332-344` | `unload()` 的候选清理漏了 `tailTransition`、`executionMapping`、`hostFence`、`acceptedHostControl`、`confirmedHostContext` | 跨 unload 保活；后续 commit 可能带旧上下文 |
| D-19 | S1 | `engine/playback/src/playback_session_prepared.cpp:30-42` | `PreparedPlayback::State::owner` 是裸指针，`materializePresentation` 会解引用 | Session 先销毁就是 UAF；文档未规定生命周期顺序 |
| D-20 | S3 | `engine/playback/src/playback_session_prepare.cpp:407,417,531,539` | `activeChartJson` 与 `State::parameters` 只写不读 | 长期持有整份 chart 文本的第二份拷贝 |
| D-24 | S2 | `engine/chart/src/canonical_chart_loader.cpp:216`、`playback_session_prepare.cpp:294` | load 末尾编译一次只为校验并丢弃，playback 再编一次；migrator 三遍 | 40k 实体预算下的重复计算 |
| D-25 | S2 | `engine/chart/src/cxt_v2_loader.cpp:216-249` | 父图解析对每个实体线性扫描全部实体 | O(n²)；40k 容量探针没有覆盖 CXT 展开形态 |
| D-26 | S3 | `engine/chart/src/behavior_candidate.cpp:124-128` | 用自定义字符串 hash 填 `CandidateRuntimeMetadata::semanticIdentity`（类型是 `PackedSemanticIdentity`） | 同名字段两处含义不同 |

---

## DOC. 文档漂移

### DOC-01 Packed Chart 文档写“未实现”

**严重度**：S3  
**位置**：`docs/formats/README.md:40`、`docs/formats/PACKED_CHART_FORMAT.md:3`

代码已经有完整的 candidate writer/reader/inspect、测试和工具，文档仍写“未实现”。  
**建议**：把状态改成 candidate，补一条 claim。

### DOC-02 Player 选项表只有 7 项

**严重度**：S3  
**位置**：`docs/architecture/PLAYER_APPLICATION.md:109-116`

解析器支持 19 个参数，文档表只列了 7 个，缺 `--mode`、`--cxc`、`--gameplay-*`、`--candidate-entry`、`--show-fps` 等 13 个。  
**建议**：补全表格；或从解析器生成表格。

### DOC-03 启动流程写错

**严重度**：S3  
**位置**：`docs/architecture/PLAYER_APPLICATION.md:98`；`player_control.cpp:270-307`

文档说启动走 `apply(Load)`，代码走 `preparePlayerLoadBundle` + `adoptStartupLoad`，后者绕过命令表。  
**建议**：按实际调用链重写这一段。

### DOC-04 R 键行为写错

**严重度**：S3  
**位置**：`docs/architecture/PLAYER_APPLICATION.md:201`；`player_control.cpp:151-154`

文档写 `KeepChartTime`，代码是 `RestartAtZero`。  
**建议**：改文档，或改代码，二选一。

### DOC-05 SDK 版本自相矛盾

**严重度**：S3  
**位置**：`docs/api/diagnostics-identity-and-compatibility.md:7,22,64`

页头写“适用版本 SDK API 0.9.1”，正文两处写“当前 SDK 0.8.0”。  
**建议**：统一到 0.9.1，并加一条版本门禁。

### DOC-06 `computeFrameDigest` 签名写错

**严重度**：S4  
**位置**：`docs/api/frames-digests-and-timelines.md:56`；`engine/playback/include/cuexis/playback/frame_digest.hpp:18`

文档说“以 RuntimeFrame、FrameSnapshot 和 digest version 为输入”，实际签名只有前两个参数。  
**建议**：删掉“digest version 为输入”的说法。

### DOC-07 `MODULE_BOUNDARIES.md` 停在 Stage 7 之前

**严重度**：S3  
**位置**：`docs/architecture/MODULE_BOUNDARIES.md:5`

更新日期 2026-10-28，全文没有 `execution_host`，也没有 Stage 7 的候选分层。  
**建议**：补 execution_host、playback 候选、judgement 内部边界；每阶段更新一次。

### DOC-08 文档比代码多，状态词过载

**严重度**：S3  
**位置**：`docs/` 全量

127,331 行文档、732 篇 Markdown、61,869 行阶段报告（474 篇）。状态词有 production、candidate、testOnly、inconclusive、not-observed、blocked、partial。  
**建议**：阶段报告全部转 historical；`CURRENT_STATUS` 控制在 100 行内；状态词压到 4 个。

### DOC-09 判定“只依赖 core”只写在阶段计划里

**严重度**：S3  
**位置**：`docs/stage_plans/active/stage-07/04-pd/plan.md:51`、`docs/architecture/MODULE_BOUNDARIES.md:18`

这条约束没有 ADR 级记录。阶段计划归档后，新会话只能看到 CMake，看不到原因。  
**建议**：在 ADR 0027 加一节，或补一条小 ADR。

---

## P. 性能与内存

绝对值以 `docs/CURRENT_STATUS.md:49-64` 的实测为准，这里只记录机制。

### P-01 输入累积

**严重度**：S2  
机制：app 层 `pending()` 每帧按值复制（D-07），加上判定侧 ingress 保活。高频输入或长活动下，复制量随积压线性增长。  
**建议**：环形缓冲 + 只读视图；`pending()` 返回 span。

### P-02 Replay 保活与 `captureState` 深复制

**严重度**：S2  
机制：Replay 要保活历史状态；`captureState()` 全量复制 projection/coverage/pending/matchers/ingress，并 flatten 日志（D-09）。文档记录 255 个状态约 118.64 MiB。  
**建议**：共享不可变前缀；只复制变化页。

### P-03 CXT 展开 O(n²)

**严重度**：S2  
机制：父图解析对每个实体线性扫描全部实体（D-25）。40k 容量探针没有覆盖 CXT 展开形态。  
**建议**：建索引；把 CXT 展开加入容量探针。

### P-04 同一份 chart 编译两次

**严重度**：S2  
机制：canonical loader 末尾编译一次只为校验并丢弃，playback 再编一次（D-24）；migrator 三遍。  
**建议**：把校验用的编译结果传下去，或让校验复用 playbook 的编译产物。

### P-05 Seek 冷路径 O(n²)

**严重度**：S2  
机制：冷路径要重放 receipts 并逐条比较，最坏 O(n²)。`CURRENT_STATUS` 的 fallback/cold 重复性 inconclusive 与此方向一致。  
**建议**：按 checkpoint 分块比较；补一个 10 分钟长度的 seek 基准。

---

## T. 测试与验证

### T-01 `execution_host` 没有独立单测

**严重度**：S2  
**位置**：`app/execution_host/`；覆盖在 `tests/rpa`

6.8k 行应用代码，0 个独立 case。线程、队列、关闭路径的回归只能靠集成和设备证据。  
**建议**：假时钟 + headless，覆盖 start/stop/close 三条路径。

### T-02 `tests/pd` 只在 Windows 跑

**严重度**：S2  
**位置**：`tests/pd`、`CMakeLists.txt` 的 PD observer 条件

性能/内存观测是 Windows STATIC 专用；Linux hosted 不跑同一组。  
**建议**：至少把分配计数做成跨平台的最小门禁。

### T-03 `player_execution` 无单测

**严重度**：S2  
**位置**：`app/player/src/player_execution.cpp`

几百行的执行函数，没有按命令拆分的测试；与 D-01、D-02 同源。  
**建议**：先拆函数，再按命令补 case。

### T-04 GameplayOnly 的缺陷被测试掩盖

**严重度**：S2  
**位置**：`tests/playback`、`playback_session_prepared.cpp:109`

测试只验证 `commit()`，不验证 `valid()` 和访问器，所以 C-01 一直没被发现。  
**建议**：补一条“prepareGameplayLoad 后读 contentInfo”的测试。

### T-05 没有文档与代码的一致性门禁

**严重度**：S3  
**位置**：`tools/check_docs.py`

`check_docs.py` 检查链接、H1、可达性、阶段目录结构，不检查“文档里的版本/参数/签名是否和代码一致”。DOC-01 到 DOC-06 都是这么漏过去的。  
**建议**：从 `player_options.cpp` 生成选项表；对版本、公共签名做锚点检查。

---

## A. 架构与流程

这一组不是 bug，是让 bug 更容易发生的环境。

### A-01 两条产品线

候选路径占 `PlaybackSession` 公共方法的 58%（47/81），有独立构建、安装 flavor、测试目录和文档。维护面翻倍，生产路径反而更少被走。  
**建议**：冻结候选的兼容承诺，或拆成独立包。

### A-02 五条格式线

Chart v1–v5、CXT v1/v2、Packed、CXC、Capsule/Graph。每加一版，实际增加 loader、writer、fixtures、golden、迁移、capability、预算、文档各一套。  
**建议**：v1 只保留 Chart v4、CXC v1、CXT v1；其余明确“不在 v1”。

### A-03 14 个超过 30 KB 的文件

最大 41 KB（dispatch），其次 37 KB（playback execution）、34.7 KB（timebase）。一个文件承担一个概念的全部变体。  
**建议**：先拆 execution、dispatch、timebase 三个。

### A-04 `PlaybackSession` 81 个方法 / 30 多个字段

唯一的“上帝对象”。事务化、owner-thread、不可移动让它可控，但每个新能力都往上加方法。  
**建议**：见 S-01、S-02；把候选面拆出去。

### A-05 文档比代码多，状态词过载

见 DOC-08。维护文档的时间已经超过写代码。  
**建议**：只维护“活的那十几篇”，其余转 historical。

### A-06 平台门禁不对称

PD observer 仅 Windows；24 个 preset；hosted 与本地结论不一定一致。  
**建议**：把关键门禁做成跨平台最小集，其余标注“平台专用”。

### A-07 版本日期驱动

`26.11.14-1`，文档改动也可能触发版本门，小步迭代有摩擦。  
**建议**：把“产品版本”和“构建标识”彻底分开，文档改动不升版本。

### A-08 阶段报告驱动，缺架构快照

`docs/architecture/` 只有 8 篇、1,423 行；状态靠 474 篇阶段报告和 `CURRENT_STATUS`。  
**建议**：加一份 2 页架构快照，每阶段更新；报告只做证据。

### A-09 约束只活在阶段计划里

“judgement 只链接 core”在 `stage-07/04-pd/plan.md:51`，计划归档后就只剩 CMake。  
**建议**：约束进 ADR 或 MODULE_BOUNDARIES 的“依赖原则”。

### A-10 单人产能配平台级验收

代码 10.8 万行，流程侧是 12.7 万行文档、24 个 preset、4 条 CI 工作流。每次改动要同时满足两套构建、格式矩阵、hosted 证据和文档同步。  
**建议**：减定义，不减质量：发布一条线，候选一条线，v1 一个范围。

---

## 修复顺序（建议 30 天）

| 顺序 | 条目 | 工作量 | 理由 |
| --- | --- | --- | --- |
| 1 | C-01、D-03、D-01 | 2–3 天 | 主路径可用、退出不挂死、不重复读盘 |
| 2 | S-01（`CandidateState` + `reset()`） | 1–2 天 | 消灭三份清单，D-18 不再复发 |
| 3 | C-05（删测试专用副本） | 1 天 | 让 presentation 测试测真实路径 |
| 4 | C-03（改名或加参数） | 半天 | 停止数据丢失陷阱 |
| 5 | C-02、C-04、C-06、C-07 | 2–3 天 | 清掉契约谎和校验剧场 |
| 6 | DOC-01 到 DOC-04 | 1 天 | 文档不再误导主路径 |
| 7 | S-03 拆分 dispatch | 1 周 | 应用层可测、可改 |
| 8 | P-01、P-02 | 2 周 | 输入累积和 Replay 保活是生产接受的硬阻塞 |

## 已排除，不算问题

- 表现与判定两套状态机：这是“判定独立”的代价，不是缺陷。
- judgement 22k 行：模块大，但边界、tick 类型、恢复校验有纪律。
- 四种安装库、12 个公共头：产品边界清晰，不是膨胀。
- 1,460 个测试：测试多不是问题，测试测错对象才是（C-05）。

---

*本清单基于只读审阅。修复前请用当前 HEAD 复核位置；行号会漂移，符号名和文件路径更可靠。*


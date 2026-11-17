# 假保证与假实现扫描（2026-11-17）

状态：completed（扫描与合并完成；修复未开始）

更新日期：2026-11-17

## 结论

7 个无上下文子代理按模块扫描了「注释/合同承诺」与「代码实际行为」的差距，合并去重后得到
**48 条候选，其中 25 条经 owner 亲自复核确认**。按危害排序，前三名不是性能问题，而是会
**破坏数据或让验收结论失真**：

1. **保存路径丢数据**：`ChartWriter::write` 把 timing.stops/tempoEvents、模板库、版本号硬编码成
   空值/3（`engine/chart/src/chart_writer.cpp:676-685`），载入→保存→重读会静默改变谱面时间映射。
2. **诊断字段恒为常量**：`render_us` 在首帧恒 0、`exactLossCount` 恒 false、
   `meshResourceLookupComparisons` 永不自增——而测试还断言这些假值（`CHECK(... == 0)`），
   等于测试为假实现背书。
3. **状态机有分支但永不进入**：`SessionState::Failed`、`FeedbackStatus::ConfirmedPresent`、
   `KernelSessionState::Created/Configured`、`StudioPreview`、`Ready` 但无 World——
   声明了完整状态空间，生产只走一条边。

一句话：**这些不是「没实现」，而是「实现了一条更弱的路，注释承诺的却是更强的那条」**。

## 方法与证据分级

- 7 个只读子代理，范围：world/core/runtime/content、playback、judgement/gameplay、chart/assets/animation、
  host/player、render/audio/shader、文档 vs 代码。
- 每个子代理被要求：`file:line`、原文引用、代码反证、判定等级、复核命令，并区分「读过确认」与「推断」。
- owner 对每个子代理的 3–4 条关键结论做了独立 grep/读码复核；**未复核的条目单独标注**，
  不与确认项混排。

## 确认清单（25 条，已亲自复核）

| ID | 位置 | 承诺 | 实际 | 判定 |
| --- | --- | --- | --- | --- |
| F401 | `engine/chart/src/chart_writer.cpp:676-685` | ChartDocument 面向保存/迁移 | `requiredExtensions`/`templates`/`timing.stops`/`tempoEvents` 恒空，`version` 恒 3 | 假实现（丢数据） |
| F402 | `engine/chart/src/chart_writer.cpp:693,699` | `writeV4/writeCanonical(limits)` 受预算约束 | `static_cast<void>(limits)` 死参数 | 半实现 |
| F403 | `engine/chart/include/cuexis/chart/limits.hpp:3-6` | 调用方不得放宽预算 | Chart 侧无 `min(requested, frozen)`，仅 Packed 有 | 假保证 |
| F404 | `engine/json_support/src/schema.cpp:50-51` | 校验失败应报错 | 诊断 append 后恒 `return {}` | 易误用（已自述） |
| F405 | `engine/playback/include/.../playback_session.hpp:250-254` | 开关不改变公开类定义（ADR 0042） | `materializePresentation` 在 `#if` 外声明并导出 `host.rpa.*` | 假保证 |
| F406 | `engine/playback/src/playback_session_execution.cpp:484` | 反馈有 created/confirmed/expired 状态 | 唯一写入恒 `created`，其余零写点 | 假实现 |
| F407 | `engine/playback/src/playback_session_execution.cpp:542-548` | 返回完整 `StepEffects` | 只填 4/9 字段，其余默认 | 半实现 |
| F408 | `engine/playback/include/.../presentation.hpp:203,208` | v2 Built-in 字段参与能力判定 | 零消费，仅声明 | 死字段 |
| F409 | `engine/judgement/src/execution_kernel_archive.cpp:332` | 未变记录 O(1)/O(目录) 比较 | 每条记录 `make_shared<RuntimeState>(*active)` 深拷贝全部容器 | 假保证 |
| F410 | `engine/judgement/include/.../kernel_types.hpp:202` | 四态会话状态 | `Created/Configured` 无任何写入/读取 | 死实现 |
| F411 | `engine/judgement/include/.../judgement_session.hpp:143-147` | `hasPreparedState()` 永远 false | `session.cpp:283/423/516` 写 `prepared=true` | 过期陈述 |
| F412 | `engine/playback/include/.../playback_session.hpp:170-175` | `Failed` 表示不可继续 | 全模块无写入点，仅 reference_host 读取 | 死实现 |
| F413 | `engine/world/include/cuexis/world/world.hpp:3` | thread-safe wrapper | world 目录 mutex/atomic 计数 0；断言在 NDEBUG 为空 | 假保证 |
| F414 | `engine/runtime/include/.../runtime_session.hpp:274` + `transform_system.cpp:145` | 回调前使缓存失效 | World 层从不维护 `transformCacheValid_`，注释说的 before 实为 after | 假保证（机制缺失） |
| F415 | `engine/core/internal/.../presentation_revision.hpp:15` | epoch 使借用失效 | 唯一读取在测试，生产不消费 | 半实现 |
| F416 | `engine/world/src/property_resolver.cpp:494,531` | `commit/rollback` 是事务入口 | 生产零调用，`rollback` 恒空转 | 死实现 |
| F417 | `app/player/src/execution_host.cpp:71-78` | 析构时排空 | `while (state != closed) { close(); waitFor(1ms); }` 无终止条件 | 假保证 |
| F418 | `app/player/src/player_app.cpp:177-180` | `--frame-stats` 产出诊断 | 执行线直接 return，`frameStatsPrefix` 零引用 | 半实现（静默） |
| F419 | `app/player/src/player_control_frame_loop.cpp:374,384` | 首帧 `render_us` 为渲染耗时 | 构造时 0，钩子在赋值前消费 | 假实现（恒 0） |
| F420 | `app/execution_host/frame_observation.cpp:240` | `exactLossCount` 表示计数是否精确 | 聚合初始化第 5 字段为字面量 `false` | 假实现（常量） |
| F421 | `engine/render_opengl/src/open_gl_presentation_frame.cpp:226-241` | `capabilities()` 返回真实能力 | 非主线程/未 MakeCurrent 时返回默认零值，无报错 | 假保证 |
| F422 | `engine/shader/src/shader_pipeline_cache.cpp:32-39` | 编译在 worker 线程 | `std::thread` 后立即 `join()`，仍在调用线程 | 假保证 |
| F423 | `docs/api/frames-digests-and-timelines.md:62` | `computeFrameDigest` 接受 digest version | 公开签名两参，版本号只是返回字段 | 假保证（已修） |
| F424 | `docs/api/diagnostics-identity-and-compatibility.md:28,70`、`GAMEPLAY_V2_ABI.md:75` | 当前 SDK 0.8.0 | 实际 0.9.1 | 过期陈述（已修） |
| F425 | `docs/formats/TIMING_MODEL.md:52` | `startRecording/loadReplay` 是现行入口 | 全仓 0 命中，已由 gameplay replay/seek 取代 | 过期陈述（已修） |
| F426 | `docs/guides/LIMITS.md`（生成器） | 单一可读来源 | 生成器只匹配 `std::size_t`，漏掉两个 `std::int64_t` Beat 上限 | 生成缺陷（已修） |

已修 = 本轮已改文档并重跑检查；其余为**只登记、未改代码**。

**补充复核（2026-11-17，owner 追问后补证）**：类级切分本身登记在 RR-F306，本轮从「未复核」升级为已核实——
`playback_session.hpp` 488 行，三个宏块；OFF 侧 39 条方法声明、ON 侧 51 条（49 个唯一方法名），
两侧零重名，即同一安装头在两种构建下给出**两个不同的 `PlaybackSession` 类定义**。
F405 是反方向（candidate 入口漏进 OFF 面），两者是同一决策缺陷的两面。

## 未复核清单（23 条，子代理声称已读码）

以下条目子代理标记「读过确认」，但 owner 未逐条复核，**不得据此删除代码或改合同**：

| 位置 | 摘要 |
| --- | --- |
| `presentation_renderer.cpp:521` | `detachCandidate` 无 owner 校验，可在任意线程改共享状态 |
| `presentation_renderer.cpp:464-495` / `open_gl_presentation_frame.cpp:417` | `rebuild()` 复活 `closed`；close 后 rebuild 假成功 |
| `open_gl_backend.cpp:663-670` | `release()` 非主线程静默 return，文件头承诺“泄漏日志” |
| `open_gl_presentation_units_internal.hpp:25-28` | `depthQuantization` 等常量零引用，注释把排序归因于适配器 |
| `audio_transport.hpp:62` / `audio_transport.cpp:70-88` | `snapshot()` 重试失败返回陈旧/全零样本 |
| `audio_config.hpp:12` | `AudioDeviceRequest` 单枚举值，校验分支不可达 |
| `engine/chart/src/chart_v4_loader.cpp:134-162` | 预算超限只记诊断不中断，与 legacy 路径语义相反 |
| `engine/chart/src/candidate_assembler.cpp:32-38` | 未解析冻结预算，可先插入后拒绝 |
| `engine/chart/src/chart_migrator.cpp:216` | `discardedFields` 恒空，模板丢弃无记录 |
| `engine/playback/src/frame_digest.cpp:8-14` | v1/v2 与 `version_unsupported` 仅测试可达 |
| `engine/playback/src/playback_session_prepare.cpp:422` | 无 World 会话仍置 `Ready`，逐帧操作全被拒 |
| `engine/judgement/internal/.../publication.hpp:45-47` | 借用指针无生命周期强制，mutation 不检查 read |
| `engine/judgement/src/gameplay_prepare.cpp:156` | `PreparedResource` 顺序不变量无验证，产品内无消费者 |
| `engine/runtime/src/runtime_session_overrides.cpp:243,375-390` | 5 个 const 观察函数只 Debug 断言 |
| `engine/core/include/cuexis/core/math.hpp:57` | `noexcept` + `Error` 字符串构造 → OOM 时 terminate |
| `app/player/src/player_control.hpp:93-95` | “owner thread”无强制 |
| `app/execution_host/owner_channel.hpp:17-18` | “take() 单消费者”无强制（加锁本身属实） |
| `engine/core/internal/.../presentation_revision.hpp:41-43` | `version()` 无调用者且 publish 后空指针 |
| `docs/formats/execution-recovery-wire-v3.md:73` | 引用不存在的 `rpa_b3_tests.cpp`（已修为 `rpa_b3_control_tests.cpp`） |
| `docs/api/gameplay-v2-execution-types.md:57` | `CoordinationCommit` 全仓不存在（已标注设计名） |
| `docs/architecture/STAGE6_PRODUCTIZATION_BOUNDARIES.md:14,117` | media import / ALLOW_EXPERIMENTAL 已实现（已修） |
| `docs/formats/README.md:58-59` | 入口与媒体导入“未交付”（已修） |
| `engine/gameplay_packed/src/gameplay_graph_writer_internal.hpp:298` | 组件按 `variant::index()` 排序，canonical 性待定 |

## 阴性对照（子代理确认属实，避免误改）

- `engine/assets` 的全部预算都有读点且真实执行；`ResourceScope` 移动只搬指针，事务回滚在 fallback 前。
- `OwnerChannel` 全部 API 确实持锁（29 处 `lock_guard`），`reportAllocationFailure` 确实不分配。
- `GameplayAssembler::assembleInto/prepareInto` 失败路径确实零发布（先构后 move）。
- 模式求值器确实无递归、按状态序填表；`CompiledPattern` 的 repeat 用不动点终止。
- `OpenGlBackend::present()` 确实拒绝未提交帧；`renderPresentationFrame` 已在头文件标注为 0.7.0 兼容入口。
- 执行线对 `--mode host` 是显式拒绝，`SessionWorker` 收件箱有界且溢出记账。

## 对 R1–R6 的影响

| 重构步骤 | 会被自然消除的条目 |
| --- | --- |
| R1 构建隔离 | F405（宏改变公开类）必须随构建隔离一起修，不能只改构建 |
| R2 候选面降级 | F406/F407/F412/F405 的 candidate 部分 |
| R3 会话收敛 | F417（析构无界重试）、F415、F411 |
| R4 时间收敛 | 无直接对应，`HostClock` 回退问题（未复核）归时间线的静默回退 |
| R5 渲染收敛 | F421/F422/F409 的 workspace |
| R6 文档减重 | F423–F426 已修；其余过期陈述随合同重写消失 |

**结论**：假保证不是独立于减重的工作，但**F401（保存丢数据）与 F420/F419（诊断常量）必须单独修**——
它们不会因删代码而消失，反而会在重构期间继续产生错误证据。

## 建议处置顺序

1. 先修 `ChartWriter`（F401/F402）：这是唯一会破坏用户数据的条目，且修法明确（按文档字段补写 + 加非空用例）。
2. 再修三个诊断常量（F419/F420 + `meshResourceLookupComparisons`）：同时删除为假值背书的测试断言。
3. 然后按 R 步骤批量处理：F405 归 R1、F406/F407/F412 归 R2、F417 归 R3。
4. 未复核 23 条在动代码前逐条复核，不复核不动。

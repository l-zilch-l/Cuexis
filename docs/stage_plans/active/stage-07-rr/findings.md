# Stage 7-RR 发现登记册（2026-11-17）

状态：active；只登记复核发现，不包含任何修复结论

更新日期：2026-11-21

<!-- cuexis-meta
id: PLAN-active.stage-07-rr.findings
status: active
authority: owns
-->

返回 [Stage 7-RR 计划](plan.md)；阶段报告入口见[Stage 7-RR 报告索引](../../../stage_reports/stages/stage-07-rr/README.md)；
当前进度唯一以 [CURRENT_STATUS](../../../CURRENT_STATUS.md) 为准。

## 0. 口径与证据状态

严重度只表达"对已接受合同的破坏程度"，不表达修复成本：

| 级别 | 含义 |
| --- | --- |
| P0 | 现行权威文档陈述与仓库事实直接冲突，会误导接手者或消费者做出错误判断 |
| P1 | 合同承诺与实现不一致，或互操作/接受集缺陷：按文档构造的输入被拒、按文档实现的外部工具不兼容、同一失败得到不同稳定码 |
| P2 | 未文档化的收紧/放宽、未登记的受限行为、死字段或死码、可观测性缺失 |
| P3 | 冗余、风格不一致、门禁过宽或过窄，不改变行为 |

证据状态只有两种：

- **已核实**：owner 在本次复核中亲自读取过实现或文档原文。
- **未复核**：来自 10 个并行子代理中对应模块的报告，本次未逐条独立确认。
  处置前必须先补证；未复核条目不因"看起来很严重"自动升格。

本册**不是**修复记录。任何条目在 RR 批次里被关闭前，都应先有对应的 Spec/ADR 裁定或标注决定。

## 1. P0（状态文档与仓库事实冲突）

| ID | 位置 | 问题 | 证据状态 |
| --- | --- | --- | --- |
| RR-F001 | `docs/CURRENT_STATUS.md:5`、`:80` | 自称日期版本 `26.11.14-1`（历史记录），实际 `cmake/CuexisVersion.cmake:1-4` 与 `vcpkg.json:3` 均为 `26.11.16-1`（2026-11-16 提升后未回写） | 已核实 |
| RR-F002 | `docs/CURRENT_STATUS.md:26` | 自称"执行分支 `codex/r4-closeout`；唯一集成 PR 为 #44，Ready for review"；实际分支 `stage-7.5`，PR #44（`564e0d90`，2026-11-14）与 #45（`aac83e5e`，2026-11-16）均已合并 | 已核实 |

两条都与同文件 `:22` 的"当前为 Stage 7.5"自相矛盾，属"唯一状态权威失效"，因此列 P0。

## 2. P1

### 2.1 状态与文档漂移

| ID | 位置 | 问题 | 证据状态 |
| --- | --- | --- | --- |
| RR-F101 | `AGENTS.md`（2026-11-17 前版本） | 已修：AGENTS 精简后原句与 `:261` 均不存在；`ROADMAP.md:48`、`GAMEPLAY_V2_SPEC.md:2595` 仍称“AGENTS 写 Stage 12”，本轮同日订正 | 已核实（已修） |
| RR-F102 | `AGENTS.md`（2026-11-17 前版本） | 已修：AGENTS 精简后“Stage 7A 是当前实施准备线”“Stage 4-12 都是 future/deferred”均已删除 | 已核实（已修） |
| RR-F103 | `docs/api/diagnostics-identity-and-compatibility.md:22`、`:64`；`docs/api/GAMEPLAY_V2_ABI.md:69` | 仍写"当前分支 SDK API 为 `0.8.0`"；实际 `cmake/CuexisVersion.cmake:8` 为 `0.9.1`，同目录其他页已写 0.9.1 | 已核实 |
| RR-F104 | `docs/formats/MATERIAL_SHADER.md:632` | 仍写“当前分支为 0.7.5”，实际 `CuexisVersion.cmake:8` 为 0.9.1（`CURRENT_STATUS.md:115` 同） | 已修（文档：改为 0.9.1） |
| RR-F105 | `docs/formats/README.md:40` | 权威矩阵把 Packed Chart v1 标为"未实现"；实际 Header/目录、字典/身份、ARCH/ENT0、TRN0/REN0/CAM0、REQ0/CNS0、语义 hash、Reader/Writer 与 40k 测试均已实现并接入 CMake | 已核实 |

### 2.2 格式互操作与接受集

| ID | 位置 | 问题 | 证据状态 |
| --- | --- | --- | --- |
| RR-F111 | [Packed §5.1](../../../formats/PACKED_CHART_FORMAT.md#51-fixed-header96-bytes)；`packed_chart_tables_writer.cpp` / `packed_chart_tables_inspect.cpp` | 原文规定完整 Header 置零计算，与 Reader/Writer 的前 92 字节算法不一致。owner 已接受按现行实现更正合同，Header 总长仍为 96 字节；兼容后果见 [D-25](architecture-design.md#d-25-兼容入口与格式合同收敛)。 | 合同更正已完成；实现未变；真实外部消费者及互操作情况未核实 |
| RR-F112 | `packed_chart_tables_sections.cpp:55-57,489-490`；`decoder_tables.cpp:195-197`；`decoder_model.cpp:340-344` | Writer 硬写 descriptor 0；Reader 把 TIME mode 1 判 `packed.time.invalid`、REQ0 判 `packed.requirements.codec`。`writeBeatDescriptor/readBeatDescriptor` 全仓 0 调用者；GridDelta codec 仅测试调用 | 已修（文档：删除 mode 1 与 Writer 选择段） |
| RR-F113 | `limits.hpp:58-59`；`canonical_chart_loader_primitives.cpp:325-330`；`CHART_FORMAT.md:159-167` | v1/v2/v3 生产 Reader 额外拒绝 `|分子| > 1e12`、`分母 > 1e9`（`chart.beat.limit_exceeded`）；`chart.beat.out_of_range` 来自 v4 reader 与 `rational_beat.cpp` 算术，不是这两个上限。两个上限已由 `guides/LIMITS.md:62-63` 生成登记，但未进格式合同 | 已核实（“未在任何文档声明”收窄为“未在格式合同声明”） |
| RR-F114 | [CXC](../../../formats/CXC_FORMAT.md)；[Chart Entry §7](../../../formats/CHART_ENTRY_V1_FORMAT.md#7-behavior-source-candidate-profile)；`cxc_candidate.cpp` | 原合同对 candidate playback encoding 的范围不一致。owner 已接受按具名 profile 对齐：静态 profile 必须为 `packed-chart`，Behavior profile 必须为 `source-json`；普通 CXC v1 默认入口继续沿用原路径。 | 两份合同已对齐并移除冲突提示；实现未变；本轮未重跑候选播放验收 |
| RR-F115 | `CXT_V2_FORMAT.md:317,323,326,690` | `cxtImports`/`cxtInstances` 全仓 0 命中；§6 写成合同未标未实现；§12:690 的“Chart invocation”同样过度声明。展开只由工具显式传参触发 | 已修（文档：§6 移入 proposals，§12 收窄） |
| RR-F116 | `CHART_ENTRY_V1_FORMAT.md:126-145`；`cxc_candidate.cpp` | 文档声明 16 码中 2 个零产出（`resource_closure_invalid` 只有异命名空间的 `playback.candidate.*`）；实现另发 **12 个**未登记码（原文写 5 个，计数订正）。文档写“minimum stable set”，额外码本身不违约 | 已修（文档：删除 2 个零产出码） |

### 2.3 SDK 与表现合同

| ID | 位置 | 问题 | 证据状态 |
| --- | --- | --- | --- |
| RR-F121 | `frames-digests-and-timelines.md:56,67`；`frame_digest.hpp:18-20`；`frame_digest_internal.hpp:9-12` | 文档主句已改为“公开 API 不接受版本入参”；残留：同文件 `:67` 仍要求“指定版本 digest”的 parity，安装 API 不可达；测试把 `src/frame_digest_version.cpp` 编进测试目标 | 已核实（主句已修，残留待处置） |
| RR-F122 | `include/cuexis/playback/playback_session.hpp:250-254` | `materializePresentation` 无 `#if CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE` 门；production 构建的安装头暴露 RPA candidate 入口，错误码命名空间 `host.rpa.*` 也随之出现在 OFF 构建 | 已核实 |
| RR-F123 | `docs/formats/MATERIAL_SHADER.md` §9；`engine/render_opengl/src/open_gl_presentation.cpp:105` | `builtInRendererProfileVersion` 只有写入点与测试读取，无任何生产读取点；`requiredRendererProfile` 与 adapter profile 不匹配时不构成失败，合同项静默通过 | 已核实 |
| RR-F124 | `docs/formats/MATERIAL_SHADER.md` §5；`engine/playback/src/presentation_parse.cpp:404-414` | 实现额外要求 host-extension profile 必须出现在 `requiredHostExtensions` 中（文档无此规则）；同时 profile ID 走 `isUserIdentifier`，禁止 `.` 与 `cuexis` 前缀，与 §9 带点 ID 风格、`profileByteCount [1,64]` 三处互相矛盾 | 已核实 |
| RR-F125 | `MATERIAL_SHADER.md` §5、§12；`shader_compiler_sources.cpp:113-138` | 子集预检只覆盖大小/BOM-CR/`#version 450`/`#include`；geometry/tess/compute 等子集项落到 shaderc → `shader.compile.failed`。但 `playback.presentation.shader.subset_invalid` **并非零产出**：`shader_compiler_sources.cpp:128,133`、`presentation_reader.cpp:549,563`、`presentation_parse.cpp:198,204` | 部分修（文档不再承诺“完整预检”；实现仍只做部分预检） |
| RR-F126 | `MATERIAL_SHADER.md` §9；`playback_session_prepared.cpp:322-347`；`shader_pipeline_cache.cpp:148-153` | `shaderHotReloadEnabled` 由四个 capability 标志（version==2 且三份源码）推导；`render_opengl` 只调 `prepareCandidate()`/`candidate()`，全目录 0 处 `activate()`/`active()`，candidate 永不换入 active | 已修（文档：删除 warning/effective false 承诺） |

### 2.4 运行时、判定与失败语义

| ID | 位置 | 问题 | 证据状态 |
| --- | --- | --- | --- |
| RR-F131 | `docs/formats/GAMEPLAY_V2_SPEC.md:467-473`；`engine/judgement/src/input.cpp:182-184,191-205` | 同一 canonical 身份在同一 Tick 再次入场，Spec 要求 `judgement.input.same_tick_collision` / `invalid_relation`，实现返回 `late.duplicate_queue_entry` / `late_policy_incomplete`；同 Tick 不同身份又被报成 `same_tick_collision`。两个稳定码被互换，且被 `tests/judgement/execution_profile_tests.cpp:276-285` 固化 | 已核实 |
| RR-F132 | `gameplay-v2-execution-types.md:77-85`；`execution_kernel_prepare.cpp:61,77,92-93` | 实现把 `;gameplay.fold.finite.v1` 追加到 `judgementSemanticRevision`、`;grade.signed-interval.v1` 追加到 `factSemanticRevision`；Spec 规定前者应进 ruleset/session identity，后者写作 `signed-interval/1`（拼写不同） | 已核实（补充第一条位置偏差） |
| RR-F133 | `CHART_FORMAT.md:483,496-497`；`behavior_system.cpp:174-177,210-238` | 旧 `chartTimeMs` 重载 `beat` 保持 0，含 v3 event/step track 的程序经该重载求值会按 Beat 0 采样。仅当全部事件 `startBeat>0` 时才是“基准值”；`startBeat≤0` 的事件会在 Beat 0 返回其值。头文件无限制声明、测试不覆盖 track | 已核实（“一律基准值”收窄） |
| RR-F134 | `execution-coordination.md:39-44`；`sdl_audio_transport.cpp:490-518` | `activateReplacement()` 只 close/open，不重置 `clockAnchorUs/presentedSampleUs/clockErrorUs`，也不推进 discontinuity；`presentedClockObservation()` 在 Playing 下直接返回旧样本，`min(*clockAnchorUs, observedAnchor)` 让旧锚持续胜出 | 已核实（补充未推进 discontinuity） |
| RR-F135 | `frame_observation.cpp:72,96,124,288,420,524` 对比 `:335,426,447` 对比 `:550,598,613` | 三种互斥处置并存；`bad_alloc` 被冒充为 `host.rpa.capacity_exceeded`（与预留故障槽同码，仅 message 不同），或被吞成成功 | 已核实（补 420/524；同码问题更严重） |
| RR-F136 | `app/player/src/player_app.cpp:177-179` | candidate 执行路径在 `PlayerController` 之前分流，完全绕过 `PlayerCommand` 命令表与固定事务顺序；`PLAYER_APPLICATION.md:30,213` 的"所有控制面产生同一个 PlayerCommand"只对中立路径成立 | 已核实 |

### 2.5 主机构建面与文档覆盖

| ID | 位置 | 问题 | 证据状态 |
| --- | --- | --- | --- |
| RR-F141 | `PLAYER_APPLICATION.md:83-89,115-122,124` | 源文件清单只列 3 个；实际另有 4 个基础 .cpp + 条件编译的 `player_execution.cpp` + 5 个 `execution_host*.cpp`。选项表 6 项 vs 19 个 flag。`:124` 的“重复都返回 `player.arguments.*`”不成立：4 个布尔选项无重复检查，`:53/:93/:125` 返回 `player.candidate.disabled` | 已修（文档：选项表 19 项、源文件 13 个） |
| RR-F142 | `player_execution.cpp:36-38` 及 `:110,115,120,139,608,630,632,642` | 该 `error()` 辅助的 9 类失败统一为 `player.execution.test_only`；文件另有 7 个 `player.execution.*` 码，`:632` 在 fault 自带码时透传 | 已核实（“统一”限定为这 9 类） |
| RR-F143 | `check_docs.py`、`check_pe_export_gate.py`、`gnu_asset_mirror.py`；`worker_process.cpp:22-31,49,76,83` | `check_docs.py` 在 docs 有 251 处引用，“零引用”不成立；真正零/极少引用的是 `check_pe_export_gate.py`（仅 findings）与 `gnu_asset_mirror.py`。worker 逐字节 `static_cast<wchar_t>`，任何非 ASCII narrow 路径都会 mojibake | 已核实（“零引用”收窄） |

## 3. P2（登记但不阻塞本阶段退出）

| ID | 位置 | 问题 | 证据状态 |
| --- | --- | --- | --- |
| RR-F201 | `docs/architecture/OVERVIEW.md:97-106` | 属性求值顺序漏掉 `GameplayOverride`（代码在 Animation 与 HostOverride 之间插入该层；`ANIMATION_MIXING.md` 正确） | 已核实 |
| RR-F202 | `RUNTIME_SESSION.md:89-94`；`runtime_frame.hpp:10`；`runtime_session_evaluation.cpp:666` | 文档 7 态机在代码中不存在；`RuntimeSession` 无 state 枚举，`onTimeDiscontinuity` 全仓 0 命中，`ResourceState::Reloading` 从未赋值。但 `timeDiscontinuityId` 存在并在 evaluation 比较；`PlaybackSession::SessionState` 只有 4 态且无 Preparing/Reloading/Closing 迁移 | 已修（文档：删除 7 态机与回调描述） |
| RR-F203 | `runtime_session_overrides.cpp:41-102`；`runtime_session.cpp:404-431` | `acquireOverride` 只校验线程/reentry/world/kind/空 writes/对象/mask，不校验 `PropertyValue` 值域；应用点在 `runtime_session.cpp:404-431`，失败即返回且 token 仍在 `evaluation_->hostOverrides`；只能靠 `releaseOverride` 恢复，错误不指出 token | 已核实（改为“无自动回滚、错误不定位 token”） |
| RR-F204 | `property.hpp:63-70`；`GAMEPLAY_V2_SPEC.md:31`；`property_helpers.hpp:31-33` | 层域优先级是 Initial<Behavior<Animation<Gameplay<Host<Preview，枚举值只有 Gameplay 错位，不是“相反”；`seenLayers` 仅作同层重复写检测与 layerValue，不参与排序；实际顺序靠 `runtime_session.cpp:372-431` 显式调用序 | 已核实（“相反”改为“Gameplay 枚举值与优先级位置不符”） |
| RR-F205 | `diagnostic.hpp:63,66`；`diagnostic.cpp:56-60,104-106` | `add/append` 返回 bool 但未标 `[[nodiscard]]`；engine+app 135 处调用中 88 处裸丢弃。但截断有兜底：达上限插入 limit diagnostic 且 `limitReached()` 公开（20 处调用）。CODE_POLICY 引用有误 | 已核实（删“无从得知”；修正引用） |
| RR-F206 | `content_provider.cpp:144-145,213,241`、`:197` | 三处 `shared_ptr<X>{new X{...}}`，同文件已有 `make_shared<Data>`；但标准保证控制块分配失败会 delete 指针，不是“非异常安全”；真实问题是多一次控制块分配，且构造函数私有使 make_shared 不能直接替换 | 已核实（改为“多一次分配”，删除异常安全断言） |
| RR-F207 | `resource_scope.cpp:79-472` | `request()` 394 行；五类 acquire 分支 `:218-307` 与五段 fallback `:332-465` 除 handle 类型外近乎逐字重复；小组件 `requestMesh` 等只是薄包装 | 已核实（补重复行数） |
| RR-F208 | `playback_session_prepare.cpp:270-279,292-297,352-358`；`playback_session_prepare_stages.cpp:443-467` | capability 预检先于 chart 预算（compile 内 `chart_runtime.cpp:546-552`）与 presentation 预算；presentation 解析后还有第二次 capability 检查，文档阶段顺序未分列 | 已核实（位置与分列说明） |
| RR-F209 | `playback_session.hpp:170-175`；`playback_session.cpp:165,347`；`docs/api/playback-session.md:55-62` | `sessionState` 只有 Running/Empty/Ready 写入点，无 Failed 赋值；文档赋予“不可继续”语义；参考宿主存在等待 Failed 的死分支 | 已修（文档：Failed 标为从不进入；宿主死分支待代码修） |
| RR-F210 | `presentation.hpp:41-44,208` | `ShaderStage`、`shaderGlslEs300` 全仓仅声明处命中；`playback.presentation.shader.stage_unsupported` 在 C++ 0 命中，仅 `MATERIAL_SHADER.md:606` 说明未产出 | 已核实 |
| RR-F211 | `sdl_audio_transport.cpp:371-399`、`:425-426`；`audio_transport.cpp:70-71` | `snapshot()` 的 `for(;;)` 无尝试上限，同类实现用 8 次；但 `publish()` 为 noexcept 且奇偶 sequence 间无可抛调用，正常路径不会永久停在奇序号，风险限于发布线程被永久挂起 | 已核实（收窄自旋条件） |
| RR-F212 | `audio_transport.cpp:29-31,59-89`；`sdl_audio_transport.cpp:291,297,392-393` | validator 拒绝 Empty，但 transport unload 会发布 Empty 且 snapshot 原样返回；公共头未写清二者关系。`HostClock::snapshot` 的 thread_local 缓存以 `this` 为键，地址复用且 8 次全失败时返回旧对象 | 已核实（收窄“接口声明不得 Empty”） |
| RR-F213 | `sdl_window.cpp:281-287`；`sdl_audio.cpp:78-82` | 两者非 owner 线程析构都 `std::terminate()`；但窗口公共头 `sdl_window.hpp:66-75,100-105` 已明确记录该约束，audio 子系统头没有 | 已核实（按类型拆分：platform 已记录、audio 缺说明） |
| RR-F214 | `tests/playback/CMakeLists.txt:25-26,57,117` | 测试把 `frame_digest_version.cpp`、`presentation_extraction.cpp` 编进测试目标又链 playback；两文件本就在库源中，当前无宏差异故行为未分叉，属测试卫生问题 | 已核实（补“当前无宏差异”） |
| RR-F215 | `tests/platform/sdl_window_tests.cpp:39-167`；`tests/platform/CMakeLists.txt:12-14`；`BUILDING.md:444` | 测试建窗并请求全屏，仅靠 `SDL_VIDEODRIVER=dummy` 缓解，无 CTest LABELS；规则出处是 `BUILDING.md:444`“算法单元测试不得依赖窗口”，平台适配器测试是否适用存疑 | 已核实（修正引用） |
| RR-F216 | `CMakeLists.txt:92-97,155`；`CuexisLibraryType.cmake:1` | observer 门禁在 `CUEXIS_LIBRARY_TYPE` 定义前执行，全新 configure 未显式传参时必然 FATAL；已有 build dir 因 CMakeCache 有值可通过；官方 profile 脚本显式传参规避 | 已核实（补“仅全新 configure 必败”） |
| RR-F217 | `engine/CMakeLists.txt:18-20`；`engine/shader/CMakeLists.txt:31-34`；`tests/CMakeLists.txt:30-31` | 默认构建下 `cuexis_shader_cache` 无条件存在，而 shader 测试只在 `if(TARGET cuexis_shader)` 且 SHADER_TOOLS 时编入 → 默认构建遗留零测试 target | 已核实 |
| RR-F218 | `app/studio/CMakeLists.txt:1-2`；`engine/particles/CMakeLists.txt:1-3`；`VerifyArchitecture.cmake:579-606` | 只有 custom target / INTERFACE，均未 `add_subdirectory`，也不在 active 清单；一旦接入会触发 target allowlist FATAL | 已核实 |
| RR-F219 | `VerifyArchitecture.cmake:117`；`MODULE_BOUNDARIES.md:19`；`sha256.cpp:11,13` | core 规则只匹配 `SDL|glad|GL/`，不覆盖 `<windows.h>/<intrin.h>/<cpuid.h>`；引用出处是 `MODULE_BOUNDARIES.md:19`（不是 AGENTS）；core 已有 intrin/cpuid 先例，当前无 windows.h | 已核实（修正引用与“可漏入”） |
| RR-F220 | `CMakeLists.txt:1035-1044,1486-1497` | 两处 allowlist 条目宽于直接 LINK_LIBRARIES（shader→core、playback_tests→assets），但都在传递闭包内；门禁仍拒绝任何未预置的新依赖，属许可超集而非漏洞 | 已核实（改“冗余但非漏洞”） |

## 4. 源码拆分预算与超长文件
### 4.1 预算事实

> **2026-11-17 owner 决定：取消文件拆分工作。** 本阶段转为架构减重重构（见
> [plan.md](plan.md)）；本节只作为事实记录保留，不再安排拆分批次。`.cpp ≤ 800`、`.hpp ≤ 500`
> 也不再作为门禁目标。

- 预算数值来自上一轮拆分（2026-11-03）的状态记录：`.cpp ≤ 800` 行、`.hpp ≤ 500` 行，当时"终检 0 个"。
  该表述现已不在任何现行文档中，只留在历史证据
  （`stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-05-pre-o1-evidence/raw/baseline/tracked.patch`
  与 `source-split-audit/index.md` 的引用里）。
- **没有任何自动门禁**检查该预算（`cmake/**`、`.github/workflows/**` 均无），也从未写进
  `docs/guides/CODE_POLICY.md`。规则实际只存在于提交说明。
- 现状终检（2026-11-17，`engine/`、`app/`、`tools/`、`tests/`）：
  `.hpp` 全部合规（最大 `engine/playback/include/cuexis/playback/playback_session.hpp`，488 行）；
  `.cpp` 唯一越限 `tests/pd/execution_memory_observer.cpp` = **917 行**。

### 4.2 唯一越限文件

| 文件 | 行数 | 现状 | 拆分方向 |
| --- | --- | --- | --- |
| `tests/pd/execution_memory_observer.cpp` | 917 | 单一 TU 同时承担：分配账本（`Header`/`Structure`/`Snapshot` 计数）、语义根与 census（`recordSemanticPhase`/`recordSemanticCensus`/`release`）、checkpoint 连接、以及全局 `operator new/delete` 替换。经根 `CMakeLists.txt:214` 同时链入 `cuexis_pd_host_probe` 与 `cuexis_execution_device_probe` | 抽内部头承载共享状态（`inline` 变量保持单一实例），拆为"分配账本 + 全局 operator 替换"与"语义根/ census"两个 TU；全局 `new/delete` 必须仍只定义一处 |

注意：这是测试专用 interposition 单元，拆分风险高于普通源文件（ODR、单一 `operator new` 定义、
匿名 namespace 状态）。RR-4 必须先给出等价性说明再动。

### 4.3 700+ 行与 600+ 行的候选深拆清单（非本阶段强制）

若 owner 决定把阈值下调到 600/700，以下为各模块报告给出的可分离单元（行数为当次统计）：

- `judgement`：`timebase.cpp` 799（有理/积分内核 vs 公共契约 API）、`execution_kernel_restore.cpp` 635（快照生成 vs 恢复校验）。
- `chart`：`chart_writer.cpp` 779、`chart_runtime.cpp` 717、`packed_semantic_identity.cpp` 685、
  `packed_chart_tables_sections.cpp` 657、`chart_migrator.cpp` 614、`cxt_v2_loader_parse_nodes.cpp` 677。
- `playback`：`playback_session_execution.cpp` 719、`playback_session.cpp` 630、`playback_session_gameplay.cpp` 623。
- `cxc`：`zip32_envelope_internal.cpp` 733、`cxc_package.cpp` 687、`cxc_candidate.cpp` 683。
- `runtime/world/assets`：`runtime_session_evaluation.cpp` 731、`runtime_session.cpp` 699、
  `asset_database.cpp` 694、`property_resolver.cpp` 679（`build()` 单函数 330 行）、`resource_scope.cpp` 601。
- `shader/render`：`shader_cache.cpp` 778、`open_gl_backend.cpp` 715。
- `audio/animation`：`animation_mixer.cpp` 687、`audio_import.cpp` 656。
- `app`：`session_worker_dispatch.cpp` 677（一个 `std::visit` + ~50 个 `if constexpr` 分支）、
  `player_execution.cpp` 645、`frame_observation.cpp` 616。
- `tools/tests`：`check_docs.py` 881、`gameplay_author.cpp` 695、以及 `tests/` 下 11 个 700 行附近的用例文件。

## 5. 复核方法与已知边界

- 方法：10 个并行无上下文子代理按模块只读审查 + owner 抽查 8 处关键结论（本册"已核实"项）。
- 未覆盖：Release/候选预设构建、完整 CTest 矩阵、跨平台与外部消费者复验、GPU/设备路径实测。
  本册任何"实现如此"的判断都只对当次工作树文本成立。
- 已知偏差：不同计数方式对 `execution_memory_observer.cpp` 给出 910/917 两个数字；
  本册采用 `917`（`Get-Content .Count`、Python 行数、LF 计数三者一致），
  910 来自 `Measure-Object -Line` 对空行的处理。
- 本册不重复 `docs/stage_reports/stages/stage-07/90-shared/source-split-audit/` 已记录的
  五项安全网缺口；那是拆分门禁本身的审计，与本册的文档-实现审查互补。

## 6. 架构层发现（2026-11-17 研究）

8 个无上下文研究子代理的结论已整理为
[架构与实现研究](../../../stage_reports/stages/stage-07-rr/2026-11-17-architecture-research.md)；
本节只登记条目，细节与证据见该报告对应小节。

| ID | 发现 | 证据状态 |
| --- | --- | --- |
| RR-F301 | 候选支线默认 OFF，却按第二产品线全量编译与挂接（约 27k 行：engine 候选面 4.4k + execution host 9.6k + 相关测试大头） | 已核实（规模与宏计数抽查） |
| RR-F302 | `cuexis_chart` 无条件编译 packed/cxt_v2/candidate，40.9% 默认不可达 | 已核实（`engine/chart/CMakeLists.txt:6-60`） |
| RR-F303 | `engine/gameplay_packed`；`engine/CMakeLists.txt:25`；`engine/playback/CMakeLists.txt:47-54` | 模块 4,821 非空行 / 5,009 物理行，无条件编译；产品目标 0 条链接边；playback 边只在 candidate 块；默认开发者工具 `gameplay_author` 有 1 条 PUBLIC 边 | 已核实（补注行数口径与“产品 0 边、默认工具 1 边”） |
| RR-F304 | `cxt_v2_loader.hpp:49-52`；`docs/MAP.md:126`；`docs/formats/README.md:51` | 只有 `CxtV2Loader::expand`、无 v2 Writer；playback 对 cxt_v2 0 引用；文档以 candidate 登记并自述 reader/expander、未接入默认 Playback——“冒充 active”不成立 | 已核实（改写“按合同登记”的暗示） |
| RR-F305 | `prepared/commit/generation/token/presentation revision` 对照表见报告 §3.3 | 三层各一份的只有 5 项（prepared、commit、active/candidate、诊断投影、线程守卫）；generation、matchingStep、token、presentation revision 仅两层重复（matchingStep 归一化后 IDENTICAL） | 已核实（拆为 5 项三层 / 4 项两层） |
| RR-F306 | 公共类被一个宏切成两半：`PlaybackSession` 在同一安装头里按 `CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE` 提供两套类定义（OFF 39 条方法声明 / ON 90 条，其中 49 个方法名仅 ON 存在，两侧零重名），与 ADR 0042 S6-D01「开关不改变公开类定义或布局」直接冲突 | 已核实（`playback_session.hpp` 488 行；宏块 24-28/279-381/465-475；逐行计数） |
| RR-F307 | `player_control_frame_loop.cpp:95-103`；`runtime_timeline.cpp:43`；`player_execution.cpp:116`；`realtime_clock.cpp:161-171` | 非 RPA 扣两次（outputCorrection + `TimingMap.offsetMs`），RPA 只扣 A 与 inputCompensation；`offsetMs` 与 A 无等式校验。重合条件是 `offsetMs + consumedCorrection == A`，不是全为 0 | 已核实（重合条件订正） |
| RR-F308 | `player_control_frame_loop.cpp:32`；`realtime_clock.hpp:68`；`runtime_timeline.hpp:14`；`timing_map.hpp:41`；`timebase.hpp:97` | 4 个时间实现；Timebase 单列则为 5 个；两种舍入（llround vs half-even）；两份反向 seek；`simulationDeltaTimeMs` 生产侧只校验/哈希/CSV/置 0 | 已核实（补“4 或 5 取决于是否单列 Timebase”） |
| RR-F309 | legacy 渲染链只剩启动清屏一个调用点、零测试；`renderPresentationFrame` 绕过帧校验 | 已核实（`player_app.cpp:284`、`player_smoke.cpp:339,346`） |
| RR-F310 | 公共面 47/90 方法仅 candidate 构建存在；约 96 个类型无宿主消费者；`ShaderStage` 仅声明处命中 | 已核实（抽查 3 项） |
| RR-F311 | `2026-11-17-architecture-evidence.md:261-273`；`2026-11-17-documentation-refactor.md:43-49` | 70.3% 可复现（当前同口径 70.8%）；基线时 5/22 合同含错，样例已在本轮订正；22 篇清单未留存，当前非计划非 ADR 合同为 43 篇，不得再按 5/22 描述当前状态 | 已核实（改为历史基线 + 已订正） |
| RR-F312 | 代码 banner 注释失真 33%（抽样 12 条推翻 4 条）；`world.hpp:3` "thread-safe" 与实现相反 | 已核实（`world` 内 mutex/atomic 命中 0） |
| RR-F313 | `cmake/VerifyArchitecture.cmake:109-563`；`check_docs_facts.py:99-144`；`check_docs.py:531-560` | 架构门禁不查 `cuexis_internal` 越界、通用 `src/` include、安装集合本身；文档事实已有生成页新鲜度与版本漂移（仅警告），合同正文引用仍不校验 | 已核实（第四条限定到具体门禁） |
| RR-F314 | `tools/chart/chart_candidate/CMakeLists.txt:4-6`；`tools/gameplay/gameplay_assembler/CMakeLists.txt:9-14` | chart_candidate 无条件链接 playback（用 PlaybackSession），但不直编 src；gameplay_author 的链接与直编 `engine/playback/src/gameplay_entry.cpp` 仅在 `CUEXIS_ENABLE_CHART_V5_CANDIDATE` 内 | 已核实（拆分两件事并加开关限定） |
| RR-F315 | 各模块 CMake 与消费者统计 | 死重量成立：particles（无源码、未接入）、debug（仅测试链接）、gameplay（9 行 tags，runtime 仍 PRIVATE 依赖）、app/studio（仅 custom target）。不成立：content 是 canonical 公共模块（进 `CUEXIS_PUBLIC_EXPORT_TARGETS`），不是门面；player_support（1,215 行）只 app+tests 用，属职责错位而非死码 | 已核实（删 content 表述；player_support 改为“应下沉 app”） |
| RR-F316 | **应用层对 engine 无纪律（双向）**：① `app/` 直接 include `engine/playback/src/replay_identity.hpp` 6 处、`cuexis_internal/` 至少 11 处；② `cuexis_execution_host_support` 作为 app/ 库 PUBLIC 依赖 playback 并复制会话逻辑（app/execution_host 6,835 行 + app/player/src 7,794 行）；③ engine/ 内反向存放应用职责 `player_support`（唯一消费者是 app/player 与其测试）；④ 架构门禁对 app 只做 nlohmann/GL 全局扫描与 6 个 player 文件的 adapter 禁止，**不禁止** src/internal include | 已核实（include 命中与 CMakeLists 链接关系逐条查证） |

已推翻的子代理结论（避免按错情报动手）：`ChartTick` **不是**死类型（34 处使用）；
`acquireHostOverride` 并非全仓 0 引用（5 个测试文件在用，但宿主消费者为 0）。

## 7. 假保证与假实现（2026-11-17 专班扫描）

7 个无上下文子代理按模块扫描「承诺 vs 实现」，合并去重后 48 条候选，其中 26 条越过 owner 复核。
完整证据、未复核清单与阴性对照见[扫描报告](../../../stage_reports/stages/stage-07-rr/2026-11-17-false-guarantees-scan.md)。

### 7.1 已复核（owner 亲自 grep/读码确认）

| ID | 位置 | 摘要 | 判定 |
| --- | --- | --- | --- |
| RR-F401 | `engine/chart/src/chart_writer.cpp:676-685` | 保存把 `templates`/`timing.stops`/`tempoEvents` 写成空、`version` 恒 3，载入再保存丢数据 | 假实现（丢数据） |
| RR-F402 | `engine/chart/src/chart_writer.cpp:693,699` | `writeV4/writeCanonical(limits)` 的 `limits` 是死参数 | 半实现 |
| RR-F403 | `engine/chart/include/cuexis/chart/limits.hpp:3-6` | Chart 侧无 `min(requested, frozen)` 钳制，只有 Packed 有 | 假保证 |
| RR-F404 | `engine/json_support/src/schema.cpp:50-51` | `validateAgainstSchema` 诊断后恒返回 Ok | 易误用（已自述） |
| RR-F405 | `engine/playback/include/.../playback_session.hpp:250-254` | `materializePresentation` 在 candidate 宏外声明并导出 `host.rpa.*`，与 ADR 0042 冲突（这是**反方向**泄漏：candidate 入口漏进 OFF 面；类级切分见 RR-F306） | 假保证 |
| RR-F406 | `engine/playback/src/playback_session_execution.cpp:484` | `FeedbackStatus` 唯一写入恒 `created` | 假实现 |
| RR-F407 | `engine/playback/src/playback_session_execution.cpp:542-548` | `executionStepEffects()` 只填 4/9 字段 | 半实现 |
| RR-F408 | `engine/playback/include/.../presentation.hpp:203,208` | `shaderGlslEs300`/`builtInRendererProfileVersion` 零消费 | 死字段 |
| RR-F409 | `engine/judgement/src/execution_kernel_archive.cpp:332` | 每条记录 `make_shared<RuntimeState>(*active)` 深拷贝，与 O(1) 注释相反 | 假保证 |
| RR-F410 | `engine/judgement/include/.../kernel_types.hpp:202` | `KernelSessionState::Created/Configured` 无写入/读取 | 死实现 |
| RR-F411 | `engine/judgement/include/.../judgement_session.hpp:143-147` | 注释“永远 false”，实际 `prepared=true` 已写 | 过期陈述 |
| RR-F412 | `engine/playback/include/.../playback_session.hpp:170-175` | `SessionState::Failed` 全模块无写入 | 死实现 |
| RR-F413 | `engine/world/include/cuexis/world/world.hpp:3` | “thread-safe”与 0 mutex/atomic 矛盾 | 假保证 |
| RR-F414 | `engine/runtime/include/.../runtime_session.hpp:274` | `transformCacheValid_` 由调用方维护，World 层机制缺失 | 假保证 |
| RR-F415 | `engine/core/internal/.../presentation_revision.hpp:15` | `mutationEpoch` 只写不读 | 半实现 |
| RR-F416 | `engine/world/src/property_resolver.cpp:494,531` | `commit/rollback` 生产零调用、`rollback` 空转 | 死实现 |
| RR-F417 | `app/player/src/execution_host.cpp:71-78` | 析构排空是无终止条件的重试循环 | 假保证 |
| RR-F418 | `app/player/src/player_app.cpp:177-180` | `--frame-stats` 在执行线被静默忽略 | 半实现 |
| RR-F419 | `app/player/src/player_control_frame_loop.cpp:374,384` | 首帧 `render_us` 恒 0（钩子早于赋值） | 假实现 |
| RR-F420 | `app/execution_host/frame_observation.cpp:240` | `exactLossCount` 是字面量 `false` | 假实现 |
| RR-F421 | `engine/render_opengl/src/open_gl_presentation_frame.cpp:226-241` | `capabilities()` 非主线程返回默认零能力集 | 假保证 |
| RR-F422 | `engine/shader/src/shader_pipeline_cache.cpp:32-39` | “worker 线程编译”实为 `thread` + 立即 `join()` | 假保证 |
| RR-F423 | `docs/api/frames-digests-and-timelines.md:62` | 文档写三参 digest，公开 API 两参 | 假保证（已修） |
| RR-F424 | `docs/api/diagnostics-identity-and-compatibility.md:28,70`、`GAMEPLAY_V2_ABI.md:75` | 写当前 SDK 0.8.0，实际 0.9.1 | 过期陈述（已修） |
| RR-F425 | `docs/formats/TIMING_MODEL.md:52` | `startRecording/loadReplay` 全仓 0 命中却被写成现行入口 | 过期陈述（已修） |
| RR-F426 | `docs/guides/LIMITS.md` 生成器 | 只匹配 `std::size_t`，漏两个 `std::int64_t` Beat 上限 | 生成缺陷（已修） |

### 7.2 未复核（子代理声称已读码，动代码前必须复核）

| ID | 位置 | 摘要 |
| --- | --- | --- |
| RR-F427 | `presentation_renderer.cpp:521-536` | `detachCandidate` 是唯一没有 owner 校验的实现；唯一入站路径是公开值类型 `PreparedPresentation` 的析构/move-assign，跨线程时静默改 `pending/outstanding`，而 `activate/discard` 同场景会 terminate | 已核实（潜在，非已发生） |
| RR-F428 | `presentation_renderer.cpp:464-480`；`open_gl_presentation_frame.cpp:417-454`；`open_gl_backend.cpp:680,706-711` | `rebuild()` 无 closed 守卫并置 `closed=false`；OpenGL 后端 close 后 context/window 已销毁，rebuild 仍返回成功，下一次 present 报 `render.opengl.backend_unavailable`；测试把该行为编码为预期 | 已核实（两处假成功） |
| RR-F429 | `open_gl_backend.cpp:663-670`；文件头 `:6` | 注释承诺“非主线程泄漏日志”，实现两处静默 return；真正带告警的是 `:691-695`；两个入口都先做线程检查，故静默分支当前不可达，实质后果是错误线程 close 只返错、GPU 资源泄漏且无告警 | 已核实（注释与实现不符） |
| RR-F430 | `audio_transport.cpp:59-89`；`audio_transport.hpp:62-64` | 8 次重试全失败时返回 thread-local 上次成功样本（陈旧）；首次调用返回 `Stopped/0` 的默认样本，不是 Empty；头文件只说“不分配不抛”，未声明 stale 回退 | 已核实 |
| RR-F431 | `chart_v4_loader.cpp:132-155` | 动画条数预算超限只记 `chart.animation.generated_limit` 诊断、继续解析全部 clip，直到 `:185-188` 才整体拒绝；预算不约束工作量。行号订正为 132-155 | 已核实（行号订正） |
| RR-F432 | `candidate_assembler.cpp:32-38`；`packed_limits_internal.hpp:23-34` | assembler 用未钳制的 `maxPackedEntities` 前检，先全量插入，再由 `encode` 的 `min(requested, frozen)` 在 `:60-63` 拒绝；`effectiveLimits` 已存在但未调用；风险是放宽上限时内存/工作量不受 40,000 约束 | 已核实 |
| RR-F433 | `chart_migrator.cpp:216`；`chart_migrator.hpp:43` | `discardedFields = {}` 是全仓唯一写点，从无 push_back/非空赋值；迁移报告恒为空数组，与 writer 丢 templates 互相印证 | 已核实 |
| RR-F434 | `frame_digest.cpp:8-15`；`frame_digest_version.cpp:77-82` | v1/v2 仅测试可达；`version_unsupported` 连测试都不覆盖、全仓无调用方可触发，是纯防御死分支 | 部分成立（比“仅测试可达”更弱） |
| RR-F435 | `playback_session_prepare.cpp:422`；`playback_session_gameplay.cpp:180-185,318-322` | `:422` 无条件置 Ready；只有 GameplayOnly 候选路径能造出“Ready 且无 World”；RuntimeFrame 管线的 update/extractFrame/chartInfo/presentationManifest 全被 `playback.session.empty` 拒绝，但 gameplay tick 仍可用并把状态推进到 Running | 部分成立（“逐帧操作全被拒”限定为 World 帧管线） |
| RR-F436 | `publication.hpp:45-47`；`publication.cpp:14-38` | read 本身有 `weak_ptr+epoch` 校验，但 `metadata()`/`fact()` 交出的借用（`PreparedIdentity&`、`core::Error*`、`RequirementIdentity*`）无任何类型/运行时强制，只能靠注释 | 已核实（限定：只保护 read 本身） |
| RR-F437 | `runtime_session_overrides.cpp:243,375,380,385,390`；`thread_checker.cpp:19-22` | 恰好 5 个 const 观察函数仅 `assertCurrent()`，Release 下 assert 被 NDEBUG 编译掉 → 跨线程静默 UB | 已核实 |
| RR-F438 | `math.hpp:57,65-66,70,71-72,75`；`math.cpp:111-135` | `noexcept` 函数内构造带堆存储的 `Error`，OOM 时 terminate；这是 math.hpp 全部 Result 入口的共同约定，不是单点 | 已核实（面扩大到全部 Result 入口） |

## 8. 外部审查交叉核对（2026-11-17）

来源：[外部审查报告（59 条）](../../../stage_reports/stages/stage-07-rr/2026-11-17-external-findings-review.md)，
基线 `aac83e5e`（与本阶段一致），只读审阅，未运行构建。下表只登记**本轮独立核对过证据**的条目；
外部报告里未核对的条目保留原文，不得据其直接改代码。

### 8.1 已有对应（外部 → 本登记册）

| 外部 | 本登记册 | 核对结果 |
| --- | --- | --- |
| C-02 | RR-F412、RR-F209 | 一致：`SessionState::Failed` 全模块无写入点 |
| C-03 | RR-F401 | 一致：`ChartWriter::write` 丢 timing/templates |
| D-02 | RR-F419 | 一致：`render_us` 恒 0 |
| D-03 | RR-F417 | 一致：close 超预算后析构自旋 |
| S-01 | RR-F305 | 部分：F305 记三层重复，外部补充三份手工字段清单的具体漏项（见 F508） |
| S-02 | RR-F306、RR-F405 | 一致 |
| S-03 | RR-F316 | 一致：应用层无纪律 |
| D-10 | RR-F313、RR-F316 | 一致：门禁 app 侧只覆盖 6 个文件 |
| DOC-01 | RR-F105 | 部分：`formats/README.md` 行已改，`PACKED_CHART_FORMAT.md:3` 仍写“未实现” |
| DOC-05 | RR-F103、RR-F424 | 已修 |
| DOC-06 | RR-F121、RR-F423 | 已修 |
| DOC-08 | 文档治理批次 | 部分处置（MAP/元数据/证据头） |
| DOC-09 / A-09 | **本轮已处置** | ADR 0027「依赖方向」已登记 Judgement 边界 |

### 8.2 本轮新核实并补登记

| ID | 位置 | 事实 | 结论 |
| --- | --- | --- | --- |
| RR-F501 | `playback_session_prepared.cpp:109-111`；`playback_session_prepare.cpp:337-342` | GameplayOnly 分支不建 RuntimeSession，`valid()` 恒 false；`commit()` 不校验，测试只测 commit | 已核实（S1） |
| RR-F502 | `packed_chart_tables_decoder_model.cpp:436`；`canonical_semantic_chart.hpp:156-157` | `componentMask()` 的 bit2 由 `requirements.empty()` 反推，检查条件恒假，双向校验不存在 | 已核实（S2） |
| RR-F503 | `presentation_extraction.cpp:163`；`presentation_renderer/src/draw_command.cpp:24,480` | `normalizePresentationFrame` 仅测试调用；生产 renderer 另写一套，量化常量两份 | 已核实（S2） |
| RR-F504 | `pattern_dfa_node.hpp:49-52` | `ConstructionBudget` 在节点循环内构造，预算是每节点而非整次调用 | 已核实（S2） |
| RR-F505 | `timebase.cpp:707` | `mapBeatToTick` 仅头文件与测试引用，生产零调用 | 已核实（S2） |
| RR-F506 | `player_app.cpp:171`、`:177-180` 对比 `:164-169` | candidate 分支在 `content` 被使用前返回，`openConfiguredPlaybackSource` 二次调用，首次结果丢弃 | 已核实（S1） |
| RR-F507 | `execution_kernel_impl_phases.cpp:80-86` | 两个 `find_if` 结果未判 `end()` 即解引用；当前不可达 | 已核实（S1） |
| RR-F508 | `playback_session.cpp:319-348`；`playback_session_internal.hpp:217,218,221,245,246` | `unload()` 未清 `hostFence`、`acceptedHostControl`、`confirmedHostContext`、`executionMapping`、`tailTransition` | 已核实（S1） |
| RR-F509 | `playback_session_internal.hpp:268`；`playback_session_prepared.cpp:36` | `PreparedPlayback::State::owner` 为裸指针并在 materialize 时解引用，Session 先销毁即 UAF | 已核实（S1） |
| RR-F510 | `docs/MAP.md` 路由表；ADR 0043、`api/GAMEPLAY_JUDGEMENT_ABI.md`、`formats/GAMEPLAY_JUDGEMENT_SPEC.md` | Judgement seek 有实现（`judgement_session.hpp:119-140`），但三份合同文档均为 `historical`，无 active 权威入口 | 已核实（路由盲区） |

### 8.3 外部条目核查覆盖

外部报告 59 条已全部核查：D/DOC 类 13 条见 8.4 的 D1，P/T/A 类 20 条见 8.4 的 D2；
其余 C/S 类与既有登记册的对应关系见 8.1/8.2。8.4 中的“部分成立”已按核查结果改写断言，
不得再按外部原文引用。

### 8.4 分批核查结果（2026-11-17）

#### D1 批次：外部报告 D/DOC 类（13 条）

| 外部 | 判定 | 校正与证据 |
| --- | --- | --- |
| D-06 | 成立 | `player_app.cpp:91` 无保护使用 `getenv_s`；engine 侧同款均带 `_MSC_VER` + `std::getenv` 回退；Player 只在 windows preset 编译 |
| D-07 | 成立 | `capture_route.cpp:467-470` 按值返回整份 `pending_`；`execution_host_frame.cpp:130` 每帧 `take()`；积压放大仅在 `take()` 抛错中断 flush 时成立 |
| D-08 | 部分成立 | 文件 677 行、单函数 667 行（`session_worker_dispatch.cpp:9-675`）；`if constexpr` 41 个正确；`prepared.reset()` 是 **4 处**（429/491/606/647），不是 3 处 |
| D-09 | 成立 | 函数实际 `execution_kernel_restore.cpp:14-60`；`flattenLog` 仅 rpa 分支；118.64 MiB 出处是 R4 交接 `:100-101`，不在 CURRENT_STATUS |
| D-14 | 成立 | 第二份在 `gameplay_graph_render.cpp:292-297`，7 个字段逐项一致 |
| D-20 | 成立 | `activeChartJson` 全仓仅声明/写/清三处、`parameters` 无读；candidate 来源时 `chartJson` 为空 |
| D-24 | 成立 | `canonical_chart_loader.cpp:216` 校验编译后丢弃；`playback_session_prepare.cpp:294` 再编；migrator 216/430/534 三次均为校验后丢弃 |
| D-25 | **部分成立（关键修正）** | O(n²) 成立，位置 `cxt_v2_loader.cpp:240-249`（另 220-237 有第二处线性扫描）；但“40k 探针未覆盖 CXT 展开”**不成立**：`chart_capacity_probe.cpp:426-454` 的 `high-reuse-v1` 已是 `cxt-v2-source` + `groups=10000`；真实缺口是 `expand()` 耗时未计入 `measureProfile`（268-329） |
| D-26 | 成立 | 行号 124-127；`canonicalBytesIdentity` 裸 sha256、无域分隔与长度框 |
| DOC-02 | 部分成立 | 选项表实际 **6 行**（`PLAYER_APPLICATION.md:115-122`），不是 7；解析器 19 个 flag、缺 13 个两个数字正确 |
| DOC-03 | 成立 | 文档 `:104`（另 36 行需同改）；实际链是 `preparePlayerLoadBundle` → `adoptStartupLoad` |
| DOC-04 | 成立 | 文档 `:207` 写 `KeepChartTime`，代码是 `RestartAtZero`（`player_control.cpp:151-154`） |
| DOC-07 | 成立 | `MODULE_BOUNDARIES.md:5` 日期 2026-10-28；全文无 `execution_host` |

统计：成立 10 / 部分成立 3 / 不成立 0 / 无法核实 0。D1 的净产出是三条校正：D-25 的探针结论、
D-08 的计数、DOC-02 的表格行数。

#### D2 批次：外部报告 P/T/A 类（20 条）

| 外部 | 判定 | 校正与证据 |
| --- | --- | --- |
| P-01 | 成立 | `capture_route.cpp:467-470` 按值返回整份 `pending_`；`execution_host_frame.cpp:130` 每帧 `flush()`；判定侧 `input.cpp:264-265` 追加、仅 `input_mapping.cpp:195-197` 清空，积压随会话保活 |
| P-02 | 成立 | `execution_kernel_restore.cpp:14-57` 全量复制并 `flattenLog`；`NativeRecoveryPoint` 保活；118.64 MiB 是对照实验值，不是生产实测 |
| P-03 | **部分成立（与 D1 冲突，按 D1）** | O(n²) 成立（`cxt_v2_loader.cpp:220-237,238-250`）；但“40k 探针不含 CXT 展开”**不成立**——D2 只搜了 `tests/pd` 与 `tools/verification/execution_acceptance`，漏了 `tests/chart/chart_capacity_probe.cpp:426-454`（`high-reuse-v1` = `cxt-v2-source` + `groups=10000`）。真实缺口同 D-25：`expand()` 未计入 `measureProfile` |
| P-04 | 成立 | v4 路径 `chart_v4_loader.cpp:255-258` → `canonical_chart_loader.cpp:216` 编译后丢弃、prepare 再编；v1–v3 migrator 三次（216/430/534）。需区分 v4 不重复 |
| P-05 | 成立 | 冷 seek `execution_kernel_archive.cpp:215-280` 逐 record 重放，`recovery.cpp:63-82` 全量比较 → O(records×projection) |
| T-01 | 部分成立 | “0 个独立 case”不实：`tests/pd` 直接 include execution_host 头、至少 37 个 Catch2 case；`rg session_worker_dispatch tests` 为 0，677 行 dispatch 与 close 仍只有 `tests/rpa` 集成覆盖 |
| T-02 | **部分成立** | 只有 allocation observer 与 ETW 是 Windows 专用；`tests/pd` 门只要求 candidate+STATIC，Linux CI 的 `candidate-headless-debug` 也构建 baseline pd。外部“只在 Windows 跑”不成立 |
| T-03 | 成立 | `player_execution.cpp` 643 行，`runExecutionPlayer` 约 536 行，tests 0 命中 |
| T-04 | 成立 | GameplayOnly 无访问器用例；但“测试只验证 commit()”措辞不精确，还有 advance/query |
| T-05 | 部分成立 | `check_docs.py` 有窄口径契约检查（如 SDK API 只比对 BUILDING 的 find_package），但没有通用的选项/签名/启动流程一致性门禁 |
| A-01 | 成立 | 候选 47 / 公共 81 = 58.0%；测试是共用 `tests/playback` 按 tag 而非独立目录 |
| A-02 | 成立 | 5 条格式线均在树（Chart v1–v4 + v5 candidate、CXT v1/v2、Packed、CXC v1、Capsule/Graph） |
| A-03 | 成立 | 五个核心模块 >30,000 B 恰 14 个；前三 42,092 / 38,261 / 35,561 B。全仓口径不同（22 个），报告未写口径 |
| A-04 | 部分成立 | 81 个公共方法正确；`PlaybackSession::State` 是 44 个字段（25 生产 + 19 候选），不是“30 多个” |
| A-05 | 部分成立 | 计数精确（717 篇 127,323 行；代码 108,626 行）；“维护文档时间超过写代码”无仓库证据，属判断 |
| A-06 | 成立 | observer/ETW 平台限制、24 个 preset、hosted/本地差异均有据 |
| A-07 | 部分成立 | 版本门禁对全部 PR 生效、无 paths 过滤；但引用版本已过期，当前为 `26.11.16-1`（历史记录） |
| A-08 | 部分成立 | `OVERVIEW.md`（136 行）已是现成总览，“缺架构快照”过强；缺的是按阶段刷新（更新日期 2026-10-28） |
| A-09 | 不成立（已处置） | `MODULE_BOUNDARIES.md:18` 已有约束；ADR 0027 本轮已补 Judgement 边界句；`judgement/CMakeLists.txt:63-66` 仅链 core |
| A-10 | 无法核实 | 数字均可核（10.8 万/12.7 万/24 preset/4 workflow）；“单人产能配平台级验收”属人力判断，仓库无数据 |

统计：成立 11 / 部分成立 7 / 不成立 1 / 无法核实 1。净产出是四条校正：T-02 的平台范围、T-01 的用例数、
P-03 的探针覆盖（按 D1 裁定）、A-04/A-07/A-08 的计数与现状。

## 9. 修复清单（2026-11-17 核查后）

排序依据：后果是否不可逆或静默 → 触发条件是否可达 → 修复成本 → 能否不构建验证。
“需构建”指必须跑 CTest/构建才能确认修复；其余可先做静态审查与单测设计。

### 9.1 第一批：数据正确性与挂死（P0，需构建）

| ID | 缺陷 | 触发 | 修复动作 | 验证 |
| --- | --- | --- | --- | --- |
| RR-F401 | `ChartWriter::write` 恒写 v3，丢 timing/templates | 任意保存 | 改名 `writeV3Projection` 或加目标版本参数；v4 显式报错 | v4 round-trip 测试 |
| RR-F501 | GameplayOnly 被 `valid()` 判无效 | `prepareGameplayLoad` 后读访问器 | `valid()` 增加 `gameplayContent` 条件 | GameplayOnly 访问器测试 |
| RR-F506 | `openConfiguredPlaybackSource` 调两次 | candidate 执行路径 | 复用第 171 行已解析的 `content` | 读盘计数测试 |
| RR-F508 | `unload()` 漏清 5 个候选字段 | unload → commit | 引入 `CandidateState::reset()`，三处共用 | 跨 unload 状态泄漏测试 |
| RR-F509 | `PreparedPlayback::State::owner` 裸指针 | Session 先于 Prepared 销毁 | 改 `weak_ptr`/generation 校验 | 生命周期测试 |
| RR-F507 | `find_if` 未判 `end()` | 资源表不同步（当前不可达） | 两处补 `end()` 检查 | 单元测试（构造缺失资源） |
| RR-F417 | Close 超预算后析构自旋 | Close 首次超预算 | 析构加上限与放弃策略 | 假时钟 close 测试 |

### 9.2 第二批：假保证与死分支（P1，部分需构建）

| ID | 缺陷 | 修复动作 | 验证 |
| --- | --- | --- | --- |
| RR-F428 | close 后 `rebuild()` 假成功 | `rebuild`/`resize` 统一拒绝 closed；OpenGL 后端在 context 已销毁时返回错误 | renderer 单测 + 假后端 |
| RR-F427 | `detachCandidate` 无 owner 校验 | 首行 `if (!owner.isCurrent()) return/terminate`，与 `activate/discard` 一致 | 跨线程析构测试（debug） |
| RR-F429 | 注释承诺泄漏日志，实现静默 | 在 close 的 not-main-thread 分支补 `logWarning`，或删注释 | 日志断言 |
| RR-F430 | `HostClock::snapshot` 返回陈旧样本 | 注释写明 stale 语义，或返回带版本/optional | 并发失败注入测试 |
| RR-F434 | `version_unsupported` 不可达 | 删除分支或补越界版本测试 | 单测 |
| RR-F435 | 无 World 会话仍置 `Ready` | `:422` 区分 gameplay-only 状态；文档说明 World 帧管线限制 | feasibility 单测 |
| RR-F432 | assembler 不用 `effectiveLimits` | `:32` 改用 `packed::limits_detail::effectiveLimits(limits)` | 放宽上限测试 |
| RR-F431 | 预算超限继续解析 | 超限立即返回或截断循环 | 生成 40k+ clip fixture |
| RR-F433 | `discardedFields` 恒空 | 至少记录 v3 `templates` 的丢弃 | migrator 报告测试 |
| RR-F436 | 借出指针无生命周期强制 | 改按值返回或带租约 token | 借用逃逸测试 |
| RR-F437 | 5 个 const 观察函数仅 Debug 断言 | Release 下返回错误/空值，或写入契约 | Release 构建测试 |
| RR-F438 | `noexcept` + 分配型 `Error` | 契约写明 OOM=terminate，或改不分配错误码 | 契约评审（无需构建） |

### 9.3 第三批：文档与契约（P2）

DOC-02、DOC-03、DOC-04、DOC-07 已于 2026-11-17 本轮修复（选项表补到 19 项、启动链改为
`preparePlayerLoadBundle` → `adoptStartupLoad`、R 键改为 `RestartAtZero`、模块边界补
`cuexis_execution_host` 行与依赖原则）。剩余 P2 项：RR-F104（MATERIAL_SHADER 已修）、
35 条版本字面量漂移、RR-F510（Judgement seek 无 active 权威入口，待契约决策）。

## 10. 工具链审计（2026-11-17）

覆盖 `tools/` 120 个脚本（5 个无上下文子代理）。核心结论：**不能按“目录搬迁”整理**。
R4 / PD-5 套件的相对路径已进入验收哈希账本（`binding.py:73`、六处 `prepare_*` 的 glob、
`etw_safety` 封包按文件哈希），搬目录会让历史验收记录永久失配。整理改为
“**原位冻结 + 修门禁 + 登记报告来源**”。

### 10.1 新发现（已核实）

| ID | 位置 | 事实 | 结论 |
| --- | --- | --- | --- |
| RR-F511 | `tests/pd/CMakeLists.txt:54-58`；`CMakeLists.txt:44-45`；`CMakePresets.json` | `execution_acceptance` 的 22 个测试注册为 CTest，但整块被默认 OFF 的 `CUEXIS_ENABLE_CHART_V5_CANDIDATE` 包住；CI 的 `debug`/`release` 不打开，预推送只跑 `debug`/`release` → 实际生效数为 **0**；而 `tools/README.md` 又把它标为“冻结证据工具，不进 CI” | 已修（owner 2026-11-17 决定移除注册；复现改用人工 `unittest discover`） |
| RR-F512 | `CMakePresets.json:527-555` | candidate preset 的测试过滤为 `[Cc]andidate\|reference_host`，`cuexis_gameplay_assembler_cli` 与 `cuexis_gameplay_installed_consumer` 的名字不匹配 → 候选构建里也不跑 | 已修（三个 candidate preset 的过滤加入 `assembler_cli\|installed_consumer`） |
| RR-F513 | `tools/CMakeLists.txt`；`CMakeLists.txt:173`；shader-tools presets；`BUILDING.md:139-140` | `cuexis_asset_importer_tool_tests` 需要 `DEVELOPER_TOOLS` + `SHADER_TOOLS`，但 hosted shader-tools preset 显式 `DEVELOPER_TOOLS=OFF` → 仅本地可达；BUILDING 漏写该前提 | 已修（顶层与 `tools/CMakeLists.txt` 改为 `SHADER_TOOLS` 单独可进；importer 测试随 preset 可达） |
| RR-F514 | `tools/gameplay/gameplay_assembler/generate_author_schema.py` | 生成 `schemas/candidate/*.schema.json` 合同产物，但无生成器新鲜度检查 | 已修（新增 `--check` 并接入预推送） |
| RR-F515 | `tools/README.md`；`BUILDING.md:52-59` | README 只登记 Python 门禁；C++ 门禁 target（cxc/chart/media/gameplay）未登记，`cuexis_gameplay_assemble` 也不在 BUILDING 的 target 清单 | 已修（README 重写并补 C++ target） |
| RR-F516 | `tools/verification/playback_performance/compare_read_candidates.py` | 无测试、无 `docs/` 引用、只服务已关闭的 C2 候选 → 唯一 archive 候选 | 已归档到 `tools/archive/verification/playback_performance/`（2026-11-17） |
| RR-F517 | R4 套件 14 个归档文件；`audit_layout.py`、`build.py` | 文件头无“复现哪份报告”；两处硬编码本机 VS/vcpkg 路径，违反 `tools/README.md` 自有规则 | 已修（14 个文件加复现映射头；`--vcvars/--vcpkg` 改为参数或环境变量） |
| RR-F518 | `linux-quality.yml:279-282`；`gen_docs_map.py:208` | CI 的 `documentation-contracts` 只跑 `check_docs.py`、`update_version.py --check`、`gnu_asset_mirror_tests.py`，**不跑** `check_docs_facts.py`；生成块却写“`--check` 会在 CI/预推送校验” | 已修（CI 加跑 `check_docs_facts.py`；生成文案改为“由 `check_docs_facts.py` 在预推送校验”） |
| RR-F519 | `check_docs_{status,target,section}_contract_tests.py` | 三个活门禁的回归测试没有任何 CI/CTest/预推送入口，只能人工运行；其中 section 测试已因 stage-07 迁移漂移出 11 个失败 | 已修（接入预推送 Quick；section 测试路径同步 `completed/stage-07`） |
| RR-F520 | `generate_author_schema.py:91`；`schemas/candidate/*.schema.json`；`execution-coordination.md:12` | 生成器把 `executionProfile` 写成单个 `const`，但已提交 schema 与合同允许 `gameplay.execution.t4-k4.rpa.v1`；直接重新生成会删掉该 enum（真实漂移，被新加的 `--check` 首次运行抓到） | 已修（生成器改为两项 enum；`--check` 进预推送） |

### 10.2 分类统计

| 套件 | keep-gate | keep-maintainer | archive | delete-candidate |
| --- | --- | --- | --- | --- |
| `execution_acceptance`（50） | 22 | 14 | 14 | 0 |
| `playback_performance`（37 py） | 19 | 17 | 1 | 0 |
| `cxc`/`chart`/`gameplay`/`shader`/`media`（54） | 51 | 3 | 0 | 0 |

“delete-candidate”为 0 的直接原因：每个脚本都至少被一份证据报告按路径引用；但引用不等于需要，
真正可退役的是 `compare_read_candidates.py`（RR-F516）。

## 11. S-06 / S-07 独立核查（2026-11-17）

两个无上下文子代理只读核查外部报告的两条架构断言。结论均为**部分成立**。

### 11.1 S-06 “Chart 有五种表示”

- 被点名的 5 个类型全部存在，定义集中在 `engine/chart/include/cuexis/chart/`；`ChartRuntime`
  是编译结果、`CanonicalSemanticChart` 是另一输入族的语义图，职责对得上。
- “五种”是低估：实际 **7 个内容型表示**（另有 `ResolvedChartDocument`、`CandidateRuntimeArtifact`）、
  **6 个载体**（`PrepareArtifact` 同时持有 ChartDocument/v4 源/v4 解析产物）、**3 类物理编码**。
- 五者不是一条流水线：**三条输入族**（v1–v3 JSON / v4 JSON / Packed-CXT-candidate）共用
  `ChartDocument` 与 `ChartRuntime` 两个节点。
- “容易漏转”有实证：v4→v3 投影逻辑写了两份（`chart_v4_loader_projection.cpp:197-201` 与
  `chart_v4_resolver_patching.cpp:256-260`）；`ChartWriter::write` 丢 timing/templates/behavior tracks
  （RR-F401）。
- 文档缺口：**没有**“表示与转换点”表；`CHART_V4_FORMAT.md:36-49` 用占位名
  `SourceChartDocument`；`PACKED_CHART_FORMAT.md:68-80` 列出的 `semanticVersion` /
  `behaviorDefinitions` 等字段在 `canonical_semantic_chart.hpp:180-188` 不存在。
- 处置：新增“Chart 表示与转换点”表（文档）；F401 仍按 §9 P0 修代码。

### 11.2 S-07 “只加不减”

- 四个对象都还在：`cuexis_render`、Chart v1/v2/v3 wire、Packed（CXPk5 rev1）、旧 loader。
- “Stage 7 没有删除任何模块”**成立**：模块级 0 删除/合并，反而新增 3 个
  （judgement、gameplay_packed、execution_host）。
- 但“只有加”不准确：代码路径删除 19 个文件（6 个真删、13 个拆分替换），另有 15 个文件跨模块
  搬到 `app/execution_host`。
- 把 `render` 当遗留物是误判：它是生产 ECS 数据面（runtime/playback/opengl/player 都链接）；
  真正零生产消费者的是 `engine/particles`（空壳）与 `cuexis_debug`（仅测试）。
- “没有删除清单”成立；但限制不是散注释，而是 **ADR 0041** 的明确决策（Chart v1–v3 保留、
  `RenderFrame` 标 legacy 暂不删）。
- 处置：阶段收尾把“删除/合并清单”列为交付物（文档政策）；空壳与测试专用模块登记为清理候选。

| ID | 位置 | 事实 | 结论 |
| --- | --- | --- | --- |
| RR-F521 | `engine/particles/`（仅 `CMakeLists.txt:1-3`） | 空 INTERFACE 库，`engine/CMakeLists.txt` 从不 `add_subdirectory` | 已修（2026-11-17 删除目录；无构建引用） |
| RR-F522 | `engine/debug/`（87 行） | 唯一链接方是 `cuexis_debug_tests`；`DebugDraw` 生产 0 引用 | 已处置（保留并标 test-only；删除留待 RR-3） |

## 12. 提案复核记录（2026-11-21）

本轮以 `ed1537fd` 为文档修订依据，核对本地 `Cuexis-Stage-7-RR-PR47-审查报告-2026-11-21.md` 与源码、台账和 owner 已接受共识。该外部报告是待验材料；P1–P6 是提案编号，与本册严重度编号无关。按 owner 本轮补设计授权，P1–P5 五项设计缺口均已有待审推荐；下表只记录核对判断，详细设计见链接。

| 项 | 结论 | 核对与文档处置 | 证据与详细设计 |
| --- | --- | --- | --- |
| P1 | 部分成立 | 作者类型、头链、target 分离与成品装载校验确有边界缺口；“必须至少四个 target”未获证明，仅链接依赖也不足以满足已接受目标。D-06/D-10 已订正为拆出的运行内核在头文件与链接上仅依赖 `core`；类型、构建与安装、装载、当次输出及旧 SDK 迁移已补待审方案。 | [当前头链](../../../../engine/judgement/include/cuexis/judgement/kernel_types.hpp)、[装载校验](../../../../engine/gameplay_packed/src/gameplay_capsule_read.cpp)；[D-21](architecture-design.md#d-21-谱面表现与判定共用拍位换算)、[P1](simplification-proposal.md#p1-数据与-sdk) |
| P2 | 部分成立 | 原 D-23 的约数示例未冻结整数取整；把约数与 P2 待审 floor 规则判为已接受合同的硬冲突不成立。具体矩形、帧布局与整数用例已补待审方案，不在台账预先选定。 | [D-23](architecture-design.md#d-23-谱面画面比例与宿主尺寸)、[P2](simplification-proposal.md#p2-渲染与兼容迁移) |
| P3 | 部分成立 | 报告混用 Chart 的定拍位停顿与 Judgement 的拍位区间冻结/终点跳变两种模型；不能直接用旧区间 fixture 推断新模型必然改变同一接受集。新 Chart 的量化退化、碰撞、负拍/零拍边界及 clamp/seek 已补待审方案；旧零拍相邻区间样例保留，单区间跨零沿旧规则。 | [Chart 表示](../../../../engine/chart/include/cuexis/chart/timing_map.hpp)、[Judgement 表示](../../../../engine/judgement/include/cuexis/judgement/timebase_profile.hpp)；[D-21](architecture-design.md#d-21-谱面表现与判定共用拍位换算)、[P3](simplification-proposal.md#p3-时间与会话时钟) |
| P4 | 部分成立 | 台账确有漂移；owner 已接受目标 `R=Retry`、Reload 另键，F5 仍为推荐。源码 `R=Reload` 是未迁移现状；“Retry 键未定”的判断已不适用。Failed 装载恢复、设备状态矩阵、旧 Rebuild 职责迁移及无音轨与启动交互已补待审方案；Empty 仍只走首次 Load。 | [当前按键接线](../../../../app/player/src/player_assembly.cpp)；[D-13](architecture-design.md)、[P4](simplification-proposal.md#p4-player-与旧控制权) |
| P5 | 部分成立 | 损失接口、首批目标版本、旧签名同批退出，以及输入到转换模型的损失记录与 Writer 损失合并，均已补待审方案；写入前报告具体已知损失、覆盖由 Studio／第三方决定的原则已获接受，不再列为未决归属。 | [当前 Writer](../../../../engine/chart/include/cuexis/chart/chart_writer.hpp)；[P5](simplification-proposal.md#p5-写回与诊断)、[D-19 保存职责](architecture-design.md#d-19-sdk-示例与宿主试用) |
| P6 | 部分成立 | plan 漏登 D-23–D-25、D-15 局间保留漏同步成立，本轮作摘要订正并标明历史替代。D-05 仅限定 RR-3；RR-4 独立前置和实施授权边界已明列。历史基线与当前文档依据须分别绑定，不能据相差提交数推断全部源码事实失效。 | [台账与实施状态](plan.md#4-已确认方向与决策台账)、[D-15](architecture-design.md)、[P6](simplification-proposal.md#p6-迁移与验收) |

历史研究继续绑定 `aac83e5e`。本轮实跑 `git diff --name-status aac83e5e..ed1537fd -- engine app tests CMakeLists.txt CMakePresets.json cmake/CuexisVersion.cmake vcpkg.json`，列出变化仅为根 CMake、Presets、版本、`engine/debug/CMakeLists.txt`、删除 `engine/particles/CMakeLists.txt`、`tests/pd/CMakeLists.txt` 与 `vcpkg.json`；该范围内 `engine/chart`、`engine/judgement`、`engine/playback` 及 `app/player` 实现源码无差异。这是有限差异核对，未重跑历史证据命令，不证明当前构建、安装面或运行验收通过。

本轮修正状态为文档订正与待审设计补齐，实现未验；详细提案由[附页](architecture-design.md)和[提案](simplification-proposal.md)维护，新增 P1–P6 技术推荐仍待审。本轮统一文档校验状态与结果以 [HANDOFF](../../../HANDOFF.md) 为准。

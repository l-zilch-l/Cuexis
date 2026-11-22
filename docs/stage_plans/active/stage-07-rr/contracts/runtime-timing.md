# P1 / P3：运行内核、谱面时间与成品合同

状态：candidate

更新日期：2026-11-22

实现状态：未实施。

<!-- cuexis-meta
id: PLAN-active.stage-07-rr.contract-runtime-timing
status: candidate
authority: references
-->

## 范围与授权

本页是 [P1/P3](../simplification-proposal.md) 的技术设计基线。D-05 的 RR-3 仍仅包含 R1/R5/R6；本页在 RR-3 内只排 R1。实施及发布分别依计划门禁授权。保持 D-20 的旧谱行为、D-21 的量化与反查、D-22 的等级目标和 D-24 的当次结算交付。新增 API、target、wire、诊断和命令未实施。源码基线 `9623efa3`。

2026-11-21 的 [D-22](../architecture-design.md#d-22-谱师自定义判定等级)计分、误差与方向共识在下文落为实施候选合同。技术闭合不改变 `NEXT` 状态；旧 delta Fold 仅沿 D-20 的旧分支读取和运行。

**显式发行 delta，待 owner 接受。** [Chart v5 计划 §3.8](../../chart-format-update-for-v5/plan.md#38-gameplay-v2-route-selected-for-stage-7a8)及 [Stage 8 已确认约束](../../../future/stage-08/plan.md#已确认约束)限定 Chart v5 的 Gameplay v2 路线及 v2 Graph/Packed 同语义。本页保留该交付目标。后继能力另获批准后，为同一个 Chart v5 增加显式 `gameplay.version=3` 分支；仅在这一分支取代上述计划的“v5 只接受 v2”和“只发行 v2 Graph”约束。Stage 7.5 的现有高级输入任务沿用原边界；推荐另立 D-21 基础与 D-22 分阶段任务，Stage 8 维持 v2 必交项，v3 独立试用、互操作验收和晋升。Chart v6 的模型、网格与 submesh 规划不变；遵循 D-17 的各层派发，不新增跨层版本号。

## 构建、头与安装

根 `CMakeLists.txt` 同步更新 `CUEXIS_ACTIVE_TARGETS`、`cuexis_verify_target_dependencies(... ALLOWED ...)`、static 实现归档列表及 export。下表列 target 职责和依赖，子目录顺序由后面的“配置拓扑”规定。依赖为完整直接 `ALLOWED` 集合，省略 `cuexis::`；未标 PUBLIC 的依赖为 PRIVATE。内部归档 `STATIC`、`cxx_std_20`、`BUILD_INTERFACE` include、无安装头；运行侧和编译侧分别拥有 include 根。

| target / alias 后缀 / export 后缀 | 头源分配 | ALLOWED |
| --- | --- | --- |
| `cuexis_timing` / `timing` / `InternalTiming` | `engine/timing/include/cuexis/timing/{rational_beat,timing_map}.hpp` 与对应 `.cpp`；P3 后随 Chart。 | `core` PUBLIC |
| `cuexis_judgement` / `judgement` / `InternalJudgement` | `engine/judgement` 运行头与源；下文列职责。R1 保留现有 static 安装归档；收口后默认不建，仅实验、具名内核测试或获准的正式组合。 | `core` PUBLIC |
| `cuexis_judgement_compile` / `judgement_compile` / `InternalJudgementCompile` | `engine/judgement_compile` 的旧作者/图/旧 timebase 源和新 `beat_us_compile.hpp/.cpp`；候选、作者工具与启用的编译测试；默认启测也需该目标。 | `judgement core timing` PUBLIC |
| `cuexis_gameplay_packed_read` / `gameplay_packed_read` / `InternalGameplayPackedRead` | `engine/gameplay_packed/include/cuexis/gameplay_packed/{compiled_capsule,detail/compiled_artifact_rows}.hpp`；`engine/gameplay_packed/src/{compiled_capsule_read,compiled_graph_read,compiled_artifact_finalize}.cpp`。Reader 与 Source 共用此目标的 `finalizeCompiledArtifact`。 | `core judgement timing chart` PUBLIC；`json_support`。 |
| `cuexis_gameplay_packed_source` / `gameplay_packed_source` / `InternalGameplayPackedSource` | `engine/gameplay_packed/source_include/cuexis/gameplay_packed/{compiled_source,compiled_writer}.hpp`；`engine/gameplay_packed/src/{compiled_source,compiled_writer}.cpp`。 | `gameplay_packed_read judgement_compile chart timing core` PUBLIC；`json_support`。无 Playback 边。 |
| `cuexis_cxc` / `cxc` / `InternalCxc` | `engine/cxc` 的通用包、清单、路径、完整性、资源闭包及写入。 | `core content project chart` PUBLIC；`filesystem json_support MINIZIP::minizip-ng`。 |
| `cuexis_cxc_candidate` / `cxc_candidate` / `InternalCxcCandidate` | `engine/cxc/include/cuexis/cxc/cxc_candidate.hpp` 与 `engine/cxc/src/cxc_candidate.cpp`；保留既有候选解码，后继条目只验包络、metadata、路径和 hash。 | `cxc chart core` PUBLIC；`json_support`。无玩法 Reader 或 Judgement 边。 |
| `cuexis_gameplay_packed` / `gameplay_packed` / `InternalGameplayPacked` | 原 `gameplay_capsule.hpp` 在 T-13 移至 `engine/gameplay_packed/legacy_include/cuexis/gameplay_packed/`，include 拼写不变；原 `gameplay_capsule*.cpp/.hpp`、Graph JSON 及语义哈希辅助头留原目标，保留 rev2–4、Graph1 重装配及 `semanticDiff`。 | `chart judgement_compile filesystem core` PUBLIC；`json_support` |
| `cuexis_chart` / `chart` / `InternalChart` | 旧 Reader、`behavior_candidate.cpp`、`packed_chart_*` 保留；新 `chart_v5_loader.cpp`、`chart_v5_document.hpp`、`compiled_chart_timing.cpp/.hpp`；旧 `chart::RationalBeat`/`TimingMap` 保留原接口和旧诊断。 | `core` PUBLIC；P3 后加 `timing` PUBLIC；`json_support` |
| `cuexis_playback` / `playback` / `Playback` | 普通与实验源分表，沿用单个 SDK 库。 | `audio content core` PUBLIC；`animation assets chart cxc filesystem project runtime render world`；实验加 `cxc_candidate json_support judgement gameplay_packed`；后继成品加 `gameplay_packed_read timing`；显式源 bridge 才加 `gameplay_packed_source`。 |
| `cuexis_gameplay_author` / `gameplay_author` 与 `cuexis_gameplay_author_cli` / `gameplay_author_cli` / 不安装 | `tools/gameplay/gameplay_assembler` 的源解析、编译与编码；`src/assembler_cli.cpp` 只编入 CLI 接线；旧 metadata helper 沿现有方式把 `engine/playback/src/gameplay_entry.cpp` 编入 CLI 私有源表，不新增 shared 导出。 | author: `core json_support chart judgement_compile gameplay_packed gameplay_packed_source` PUBLIC；CLI 接线: `gameplay_author core playback cxc cxc_candidate asset_publish`；`cuexis_gameplay_assemble` 只连 CLI 接线；`cuexis_chart_candidate` 在原普通工具边上追加 CLI 接线。 |

**配置拓扑。** `engine/CMakeLists.txt` 保留现有基础目录次序，T-12 在 `chart` 之前插入 `timing`，T-10 在 `judgement` 之后插入 `judgement_compile`，`gameplay_packed` 只 `add_subdirectory` 一次。涉及本合同的先后是 `core/json_support/content/project` → `timing`（T-12 起）→ `chart` → `cxc` → `judgement` → `judgement_compile` → `gameplay_packed` → 原 `render/debug/runtime` 等 Playback 前置 → `playback`；其它原目录和开关保留。`cxc/CMakeLists.txt` 先创建 `cuexis_cxc` 再创建 T-04 的 `cuexis_cxc_candidate`；`gameplay_packed/CMakeLists.txt` 先创建旧 `cuexis_gameplay_packed`，T-13 再依次创建 `cuexis_gameplay_packed_read`、`cuexis_gameplay_packed_source`。这些是真实 target 名，不增加同名新目录。根仍在 `engine` 全部完成后进入 `tools`；`tools` 内先 `common/asset_publish`，再 `gameplay/gameplay_assembler`，后 `chart/chart_candidate`。依赖检查在所有启用目标创建后运行；按开关跳过的目标及其边也一同跳过，不把后继 target 写进前卡的 `ALLOWED` 或安装列表。

**分阶段构建。** 上表是后继接线完成后的目标图；源表、链接和 `ALLOWED` 按任务同步生效。

- [T-10](../tasks/T-10.md)：`judgement_compile` 只编译迁入的 legacy 作者、图、旧 timebase 及适配源，`PUBLIC` 直接链接 `cuexis::judgement` 和 `cuexis::core`，`ALLOWED` 仅这两项。运行计分 records、局部验证与 Fold 同卡归 `judgement`，只用 Core。默认顶层启测（`BUILD_TESTING=ON`，`CUEXIS_BUILD_TESTS` 随之启用）也构建 `judgement_compile`；不以 candidate 或 developer tools 为前提。`tests/rpa/CMakeLists.txt` 给既有 `cuexis_rpa_tests` 追加 `PRIVATE cuexis::judgement_compile`，根给该测试追加同名 `ALLOWED`；测试夹具直接使用作者图和旧 prepare。该测试边不改变运行目标或默认安装闭包。旧 `gameplay_packed` 与 `gameplay_author` 从 `judgement_compile` 取得旧公开作者头（原作者验收的 judgement 测试亦加此边），同时更新根 `ALLOWED` 与编译/安装归档表；候选 Playback 的旧作者用法从旧 Packed 的 PUBLIC 编译边取得。仍引用搬迁私有头的旧 Packed 和测试只在 `PRIVATE` 增 `judgement_compile/src` 相应路径。`cmake/VerifyArchitecture.cmake` 按归属检查两个分区；运行 `include/internal/src` 逐文件禁止拉入迁出的作者头或 timing，编译侧存在性/声明检查改到新根。同卡无 `timing`、`beat_us_compile`、共享成品头或 `gameplay_packed_read`。实验 static SDK 的 `CUEXIS_STATIC_IMPLEMENTATION_TARGETS` 加 `cuexis_judgement_compile`（`EXPORT_NAME InternalJudgementCompile`，不安装头）；shared 安装不引出该归档。默认安装若不链接该归档不加入安装树，保留现行旧归档规则。
- T-10 同卡在 `tools/gameplay/gameplay_assembler/CMakeLists.txt` 首创 `add_library(cuexis_gameplay_author_cli STATIC ...)` 及 alias `cuexis::gameplay_author_cli`，仅在 developer tools 和 candidate 同时启用时创建。将现有 candidate 分支的 `src/assembler_cli.cpp`、`engine/playback/src/gameplay_entry.cpp`、私有 include 根和 Playback/CXC/asset_publish 接线从 `cuexis_gameplay_author` 移入该目标；CLI 的 BUILD_INTERFACE 暴露原 `include`，私有根仍为 `engine/core/internal`、`engine/playback/src`、`engine/judgement/internal`。author 保留 `src/gameplay_author.cpp` 与旧作者头，T-10 PUBLIC 边仅 `core json_support chart judgement_compile gameplay_packed`，无 Playback/CXC/发布边。CLI 直接 PRIVATE 边与上表一致；`cuexis_gameplay_assemble` 改链 `cuexis::gameplay_author_cli`，`cuexis_chart_candidate` 的 candidate 分支把 `cuexis::gameplay_author` 换为 `cuexis::gameplay_author_cli`，原普通工具边保留。根同步 active-target 列表与这四个目标的 `ALLOWED`；不安装两种 author 归档/头。T-13 才给 author 追加 `gameplay_packed_source` PUBLIC 并在已有 CLI 加 `beat-us` 分支，不重复创建目标。
- [T-12](../tasks/T-12.md)：创建 `timing`，加入 `engine/judgement_compile/include/cuexis/judgement/beat_us_compile.hpp` 与 `engine/judgement_compile/src/beat_us_compile.cpp`，链接与 `ALLOWED` 加 `timing` PUBLIC。首创 `engine/judgement/internal/cuexis/judgement/compiled_gameplay_rows.hpp`，放 `CanonicalGameplayRows`、`CompiledBeatKeys`、未终结 `BeatGameplayProduct`；同卡首创 `compiled_gameplay_context.hpp`，仅提取 `CompileCapabilityContext`、`ContentProfileLimits`，直接包含 T-10 的运行值头 `prepared_identity.hpp`，旧作者头转引。四个身份声明由 T-10 定义，本卡不移动或重定义。共享内部根以 `judgement_compile` 的 `BUILD_INTERFACE` 给编译消费者；运行头/TU 无须从该内部 context 取得身份定义。T-13 只消费这两头，定义与兼容转引按[成品合同](prepared-artifact.md#拥有关系与入口)。
- [T-13](../tasks/T-13.md)：才引入 `gameplay_packed_read`、`gameplay_packed_source` 与表列文件。Reader 自己以 `BUILD_INTERFACE` 声明 `engine/judgement/internal`，不链接编译目标，不取作者 include 根；Reader 仍不得引入旧重装配或源/写出目标。Source/Writer 和两者唯一入口见[成品拥有关系](prepared-artifact.md#拥有关系与入口)。归档按各实验 static 路径的实际链接闭包安装；离线独用的 Source/Writer 不随纯成品安装。

**架构检查归属。** T-10 逐文件检查运行 `include/internal/src`。T-12 在同一个 `VerifyArchitecture.cmake` 检查中，只从这份运行文件清单排除完整相对路径 `engine/judgement/internal/cuexis/judgement/compiled_gameplay_rows.hpp`：它归编译侧与成品 Reader 的共享产物，可包含 `cuexis/timing/`。`compiled_gameplay_context.hpp` 及其它运行 TU 和头继续接受原作者头与 timing 禁入检查。所有运行扫描项还须拒绝路径末段为 `compiled_gameplay_rows.hpp` 的 `#include`，含引号相对路径和尖括号路径；同一分区内通过中转头引入也会在该中转头处失败。该例外不增加 `cuexis_judgement` 链接边、源或运行头依赖，不按目录或名字通配。

### R1：八个 Playback TU

相对 [Playback CMake](../../../../../engine/playback/CMakeLists.txt)的 `src/`。R1 不改已装声明/布局；普通/候选私有 hook 由选中的源表提供唯一实现。

| 基线 TU | 基础保留 | `CUEXIS_ENABLE_CHART_V5_CANDIDATE=ON` 才编译 |
| --- | --- | --- |
| `playback_session.cpp` | 普通 load/update/extract/unload、观察、override 主体；默认 hooks 空操作或原样放行。 | `playback_session_candidate_hooks.cpp`：fence、候选 update/尾段 guard、卸载与 override 的候选校验/清理。 |
| `playback_session_execution.cpp` | 无。 | 全文件，内部协调、时钟、回放 cut、反馈、表现采样。 |
| `playback_session_gameplay.cpp` | 无；普通 guard 调第一行 hook。 | 全文件，attach/prepare、submit/advance/query 和回放。 |
| `playback_session_gameplay_seek.cpp` | 无。 | 全文件。 |
| `playback_session_prepare.cpp` | 普通 prepare/load/reload/commit 编排和发布；候选无效时保留原拒绝。 | `playback_session_prepare_gameplay.cpp`：意图与判定构造、绑定检查、commit 转移。 |
| `playback_session_prepared.cpp` | 正式资源树、元数据、`materializePresentation`；默认不返回 gameplay publication。 | `playback_session_prepared_gameplay.cpp`：私有 publication selection hook。 |
| `playback_session_realtime.cpp` | 无。 | 全文件，capture、advance、收据、pause/resume、tail 与 control。 |
| `playback_source_gameplay.cpp` | 无。 | 全文件，具名来源、Graph/Packed codec dispatch。 |

额外分支：`playback_session_prepare_capabilities.cpp` 保留普通校验及候选拒绝，候选能力填表到 `gameplay_candidate.cpp`；通用 entry 工厂保留，候选具体选择到 `playback_source_gameplay.cpp`。`playback_session_prepare.cpp` 的 `prepareCandidateStage` 调用保留：抽 `playback_session_prepare_stages.cpp` 中的候选 lowering 至实验 `playback_session_prepare_gameplay.cpp`，普通表用同签名拒绝实现返回 `playback.candidate.disabled`。`cxc_candidate.cpp` 从通用 `cuexis_cxc` 迁至 T-04 的 `cuexis_cxc_candidate`（独立私有归档），候选 Playback、离线工具及具名测试才依赖；通用 CXC 继续整包路径/CRC/清单哈希与资源闭包检查，可读候选条目字节但不解码。普通路径错误码及 validate/commit 顺序不变；源闭包及各配置构建前置见 [T-04](../tasks/T-04.md)。

### Judgement 运行与旧作者头链

运行侧保留 `timebase_tick.hpp`（`Tick`、`TickSpan` 与域包装）；实现移 `src/timebase_tick.cpp`。`timebase.cpp` 余下旧有理运算、profile validation 和 `mapBeatToTick`、`timebase_{rational,profile}.hpp`、`timebase.hpp` 移编译侧，保留旧 include 拼写。`input_boundary{,_amount,_mapping}.hpp` 和 `ruleset.hpp` 改直接依赖 tick、`phase_types.hpp` 和各自运行依赖；旧作者转换若接收有理时长，移至编译侧 `legacy_input_conversion.hpp/.cpp`。`Outcome`、`PhaseKind`、`FactCategory`、phase/requirement 稳定标识与对应文本 token 设施移入运行 `phase_types.hpp/.cpp`。

**T-10 首创的运行纯值头。** `engine/judgement/include/cuexis/judgement/prepared_identity.hpp` 原样定义 `EngineIdentityDeclaration`、`RulesetIdentityDeclaration`、`SessionIdentityDeclaration`、`PreparedIdentityDeclarations`；从现有 `gameplay_assembler_identity.hpp` 移出，保留命名空间、成员名/顺序、默认比较与布局。它仅需 `<optional>`、`<string>`、`<vector>`：Engine 为原六个 token（含可选 execution/late），Ruleset 为 interface token、moduleOrder、buildHash，Session 为原四个 token，Prepared 按值持三者；不含生成器、Graph 或编译上下文。`prepared_gameplay_records.hpp`、`prepared_gameplay.hpp` 与 `kernel_types.hpp` 直接包含此头，`CompiledGameplayView::identities` 可由独立运行消费者自行构造，prepare 拷贝进不可变 storage；`SessionConfiguration::identityDeclarations` 继续按值拥有，不为头隔离改成不透明指针。旧作者身份头转引此唯一完整定义。

同卡把 `gameplay_graph_reference.hpp::StableDeclarationId` 原样移入运行 `phase_types.hpp`：`std::string sourceDocumentId; std::uint32_t declarationOrdinal;`，保留默认 `operator==`、`operator<=>` 及其 `noexcept`；该头直接包含所需 `<string>/<cstdint>/<compare>`。`prepared_gameplay_records.hpp` 直接包含 `phase_types.hpp`，`PreparedRequirement::stableId` 因而是完整按值类型；旧 `gameplay_graph_reference.hpp` 转引它，`DeclarationRef`、`MergedNamespace` 和作者引用闭包继续留编译侧。T-12 的编译器、T-13 的 Reader 均消费这组定义，不复制 DTO。两运行值头经 `cuexis_judgement` 原 BUILD_INTERFACE 根可见；D-19 晋升时随直接运行头链安装，当前不提前新增 Judgement SDK 安装件。

`prepared_gameplay.hpp/.cpp` 由运行侧持有。在 `prepared_gameplay_records.hpp` 提取现有已编译 pattern/measure、运行窗口/计时器、资源槽/争用顺序、FactBinding、运行 profile token，来源为 `execution_profile.hpp`、`gameplay_assembler_compiled.hpp`、`gameplay_graph_{phase,pattern,record,reference}.hpp`。作者 Pattern/Measure、引用声明、`CanonicalGameplayGraph`/`AssembledGameplay`、编译计数/预算留编译侧。`gameplay_assembler_*`、`gameplay_graph{,_diff,_render}.cpp`、`gameplay_prepare.cpp` 归编译侧；旧准备结果用 `LegacyGameplayPublication` 同时拥有 `prepared` 与 `assembled`，后者借用的旧 profile/late 声明由同一不可变出版物持有。原 Packed `PreparedCapsule`、旧 metadata/debug 从该出版物观察，不改 wire/hash。`diagnostic.cpp`、`diagnostic_policy.hpp` 留运行侧。

原 `execution_kernel_*.cpp`、`execution_scheduler.cpp`、`input.cpp`、`input_mapping.cpp`、`kernel_types.cpp`、`session.cpp`、`ruleset.cpp`、`publication.cpp` 留运行侧：`graph()` 改读 `PreparedGameplay::records()`；phase offsets、timer、规则与晚到准入使用同一组 records。`recovery{,_codec}.cpp/.hpp`、`execution_kernel_internal.hpp`、`identity_factory.hpp`、`structural_key_codec_internal.hpp` 改用运行身份和结构键；运行 replay/snapshot DTO 不持有 `CanonicalGameplayGraph`、`TimebaseProfile`、作者闭包或 `assembled()`。旧完整 profile/Graph 的 archive 解码、比对及 preimage 放编译侧 `legacy_recovery_adapter.hpp/.cpp`；旧建局与恢复先做完整旧 profile 比较及原身份校验，输出已验证的 owning tick records 给运行 codec。保留旧 wire 与身份算法，拒绝只比 id 字符串或重编计算旧 tick。`LatePolicyParameters`、`MeasuredParameter` 留运行 `execution_profile.hpp` 的直接头链，不再经 `gameplay_assembler.hpp`。

### 安装与窗口

| 组合 | 消费契约 |
| --- | --- |
| R1 默认 static/shared | 普通 Playback 不连候选，static 保留当前 `Cuexis::InternalJudgement`，shared 导出及安装头保持 R1 当时的基线。 |
| R5（T-06）后默认/实验 shared | 两种 Playback 库均精确导出 `computeContentRect`、`computeFrameLayout`、`preparedFramings`、`extractLaidOutFrame`、`relayoutRetainedFrame` 与 `LaidOutFrame` 跨库所需的移动、析构及只读访问成员，具体签名及白名单依[帧布局合同](render-player-writer.md)。声明只维护在 `engine/playback/experimental_include/cuexis/playback/experimental/frame_layout.hpp`；默认源码构建仅向实际仓内消费者提供 PRIVATE include，不安装该头；实验 SDK 经 `FILE_SET HEADERS` 安装。默认不增加候选 Reader/Gameplay 依赖，既有公共类布局不变。 |
| ABI 收口后默认 | `Cuexis::Playback` 和原组件，static 随装所需内部归档（含 `InternalTiming`），无候选头和候选 archive；shared 只从公共库消费。 |
| 实验 static/shared | 独立前缀，`Cuexis_ALLOW_EXPERIMENTAL=ON`。消费者链接 `Cuexis::Playback`；static 私有链接归档随装，shared 保留旧符号并新增下文精确表列的实验 facade 与仓内符号。内部头不安装，内部符号无 SDK 承诺；CLI 仅源码树消费。 |
| D-19 通过后 | 候选组件 `Judgement`、导出 `Cuexis::Judgement`；安装 `judgement_session.hpp`、`timebase_tick.hpp`、`phase_types.hpp`、`prepared_identity.hpp`、`prepared_gameplay{,_records}.hpp`、`advance.hpp` 及直接运行依赖头与 `judgement_export.hpp`，不装作者头。 |

按 [D-19 分组迁移共识](../architecture-design.md#d-19-sdk-示例与宿主试用)逐组执行兼容新增和稳定后退出。兼容新增阶段提供 `cuexis/playback/experimental/{gameplay,gameplay_realtime,gameplay_replay}.hpp`，命名空间 `cuexis::playback::experimental`；保留对应旧成员与符号并公告弃用。每组至少经历一个实际发布的过渡版本，旧入口持续保留至替代入口稳定、适用迁移证据通过并获准退出。具体发布版本逐组裁定，不预排统一删除版本。

过渡期用同一宿主对照旧调用和新自由函数；替代入口须已实际接入参考应用与独立安装消费者，覆盖承诺场景和错误行为，无未解决的阻断迁移问题。删除阶段只针对本次获准组验证旧声明/符号退出和新消费者正常工作；其他旧组继续可用。共享 DTO、头和实现由仍受支持的入口决定保留范围，不随某一组提前清理。各组的替代发布、弃用、稳定性证据及删除裁定附在下表对应行和计划采用记录中，未满足条件的组顺延。

`PreparedPlayback::materializePresentation` 不在 RR-4 窄迁移范围，须单独满足替代稳定及批准条件。`execution_host_support` 消费 Playback 实验库的窄仓内协调接线；涉及已安装公共类的变化仍按源码和 ABI 门禁处理。Judgement 经 D-19 独立成熟度评审后再裁定晋升版本，格式晋升由发行矩阵批准。公共变更同期更新 ABI、安装和发行说明；安装头为 ASCII。

### RR-4 逐符号兼容迁移

每组兼容新增时，自由函数沿用表列成员的**原名、所有重载、参数类型/顺序/默认值、返回类型、nodiscard 与异常约束**，只对非静态成员在最前加一个 `PlaybackSession&`（原 const 成员加 `const PlaybackSession&`），并移除尾部成员 `const`。静态 `PlaybackSource` 工厂不加参数；已有自由函数例外见表。参照 [Session 声明](../../../../../engine/playback/include/cuexis/playback/playback_session.hpp)和 [Source 声明](../../../../../engine/playback/include/cuexis/playback/playback_source.hpp)。兼容新增直接转发同版已装入口，保留原 move/borrow，避免二次校验；除仓内执行协调入口使用下表专有 TU 外，与原入口同 TU。替代稳定并获准退出后，只移除批准组旧成员；公开实验组用无条件声明的 `detail::ExperimentalGameplayAccess` 访问原实现，仓内协调组按下表的 `detail::ExecutionHostAccess` 接线。共享候选宏按仍受支持成员保留或清理。`gameplay_candidate.hpp` 继续提供原 DTO，退出后保留实验类型转发，不复制 DTO。本表不依赖 T-10 至 T-14 或新 format。

| 旧头 / 成员 | 新入口（根为 `cuexis/playback`） |
| --- | --- |
| `playback_source.hpp`：`fromCxcFileGameplayEntry`、`fromCxcMemoryGameplayEntry`、`fromFilesystemGameplayEntry`、`fromGameplayEntry`、`fromGameplayPacked`、`fromGameplayGraph` | `experimental/gameplay.hpp`，`cuexis::playback::experimental` |
| `playback_session.hpp`：`prepareGameplayLoad`、`prepareGameplayReload`、`submitGameplay`、`advanceGameplay`、`gameplayState`、`queryGameplayCapability`、`queryGameplay`、`queryGameplayPresentation`、`gameplayFeedback`、`sampleGameplayPresentation`、`controlGameplay` | 同上；这条 `advanceGameplay` 保留旧 `Result<void>` 和旧语义。新输出见下文，为另一个重载。 |
| `playback_session.hpp`：`archiveGameplay`、`snapshotGameplay`、`evaluateGameplayReplay`、`restoreGameplay`、`seekGameplay`（两个重载）、`checkpointGameplay`、`gameplayReplayCut` | `experimental/gameplay_replay.hpp`，同命名空间 |
| `playback_session.hpp`：`invalidateGameplayClock`、`mapGameplayClock`、`beginRealtimeGameplay`、`beginRealtimeTail`、`deliverGameplayCapture`、`requestGameplayAdvance`、`queryGameplayAdvance`、`retireGameplayAdvance`、`gameplayAdvanceReceipt`、`acceptGameplayControl`、`pauseRealtimeGameplay`、`resumeRealtimeGameplay`、`gameplayExecutionLag` | `experimental/gameplay_realtime.hpp`，同命名空间 |
| `execution_control.hpp`：已装自由函数 `cuexis::playback::computeGameplayReconciliationIdentity(const GameplayReconciliation&) -> core::Result<PreparedSemanticIdentity>` | `experimental/gameplay_realtime.hpp` 添加 `[[nodiscard]] CUEXIS_PLAYBACK_API` 的 `cuexis::playback::experimental::computeGameplayReconciliationIdentity`，不加 session 参数。兼容新增阶段的新入口转发原 `playback_session_execution.cpp` 实现，保留旧声明和导出；该组替代稳定并获准退出后废止旧声明和符号，把同一算法入口移至 `experimental`，仓内 Resume/捕获路由调用同一实现。保留 `execution_coordination.hpp` 中 `GameplayReconciliation`、`GameplayPauseTransition` 与其签名所需 DTO，实验 realtime 头直接提供这条窄类型链，不引入私有 owner 控制头。 |
| `playback_session.hpp`：`enterExecutionFence`（两个重载）、`commitExecutionCandidate`、`releaseExecutionFence`、`validateExecutionControlBinding`、`validateExecutionControlProof`、`validateExecutionFrameContext`、`acceptExecutionControl`、`executionFence`、`completeExecutionClose`、`executionOwnerState`、`executionStepEffects`、`executionAdvanceRetention`、`retireExecutionStep`、`pauseExecutionStep`、`resumeExecutionStep`、`seekExecutionStep` | 保持逐函数原名、重载、参数与返回类型，新增首参 `PlaybackSession&` 或 `const PlaybackSession&`（随原 const），置于 `cuexis::playback::detail`。build-only `engine/playback/internal/cuexis/playback/execution_host_access.hpp` 声明各入口，`engine/playback/src/playback_session_execution_host_access.cpp` 定义并编入实验 `cuexis_playback`，同版 `execution_host_support` 与指定测试消费。安装例 owner/步骤探针移仓内测试。旧安装头中的 DTO 若仍被公开签名引用则保留窄声明。 |
| 候选 private `gameplayWriteGuard`、`attachGameplay`、`fromGameplayEncoded` | 不提供自由公开函数；随相关组兼容裁定从共享类移入原 TU 或相应 access 实现；private 名称不豁免已安装公共类的源码、布局及 ABI 门禁。 |
| `PreparedPlayback::materializePresentation` | RR-4 内不变；另行获准后迁同一 `execution_host_access.hpp`，按同一 ABI 窗口检查。 |

**shared 内接线与退出。** 上表所有自由 facade 及 `detail` 入口的声明与定义均使用 `CUEXIS_PLAYBACK_API`，`cmake/VerifySharedExports.cmake` 精确允许表列限定名及其合法重载，只放行实验库。`validateExecutionControlProof` 与 `executionAdvanceRetention` 同表迁移，`host_candidate.cpp` 的两处消费者改用仓内入口。实验 DLL 逐函数导出内部符号；头不安装，符号无 SDK 稳定承诺。`PlaybackSession`、`PreparedPlayback`、`ExecutionFence` 无条件 friend 同一前置声明的 `detail::ExecutionHostAccess`，不变更任何布局。过渡期自由函数转发旧成员，保留全部旧导出。另获退出批准后，把原函数体移入同一 Playback TU 的 `ExecutionHostAccess` 静态成员，按原顺序检查并访问三个友元的数据；自由函数只转入这些成员。旧声明和符号仅在这一步移除。`execution_host_support` 继续单独 STATIC，按同版重编；RR-4 以 R5 的具名帧布局增量为默认导出基线，不向默认库追加本表的 Gameplay 或仓内协调符号，实验安装沿既有候选配置。

T-08 安装消费者以现有 [Resume 对照](../../../../../tests/playback/playback_gameplay_execution_tests.cpp) 建两次独立 legacy 实验局：送 `GameplayEpochEnd{{21},1}` 并 Pause；以 `GameplayReconciliation{{21},0,1,0,{}}` 分别走旧、新身份函数和 Resume 入口，新 context `{1,2,2}`、segment `2`、首样本 `22` µs、`finalRouteApplied=true`，结果相同。篡改 `pauseEpoch=2` 后身份变化且提交拒绝，局保留 Paused。实验 shared Player/host 测试以实际库和 `execution_host_support` 驱动 fence、Pause、Resume、close；默认安装探针包含实验头及私有头分别失败，实验安装也不得包含私有头。退出验证重跑同一 shared Player/host 路径并核对只有该组批准移除的旧符号消失；保留组旧消费者仍通过。

## Prepared 与当次推进

`PreparedGameplay` 是可复制、不可默认构造的不可变句柄，持有 `shared_ptr<const detail::PreparedGameplayStorage>`。仅 friend 运行侧 `prepareCompiledGameplay(const CompiledGameplayView&, const PrepareGameplayLimits&) -> core::Result<PreparedGameplay>`；旧编译器与新 Reader 共用，不共享 storage 定义。成功后拥有全部数据，不借 wire 或临时 view；构建期可分配，失败原 publication 不变。

`PreparedTimebase { std::string identity; std::int64_t microsecondsNumerator, microsecondsDenominator; Tick origin; }` 携带已核验域和正的约分 tick 时长；新模式固定 1/1、原点 0。`JudgementSession::prepare(const PreparedGameplay&, RunKey) -> core::Result<void>` 同步绑定现有配置、prepared 和 run；失败原局不变。`RunKey` 复用现有 execution scope 的 `(sessionScope,runEpoch)`，两项都需非零、不纳入时钟 segment；沿现有 owner 世代规则。同局恢复保持原键并校验 prepared/ruleset/timebase/calibration/已封 frontier；Retry 取新 epoch。原 `advance(Tick,bool)` 及旧恢复保留原语义与 ABI 窗口，不混用新旧推进。

`cuexis/judgement/prepared_gameplay_records.hpp` 的运行输入如下。嵌套值沿用现有字段，定义位置见上节；`PreparedRequirement` 是入内核数组项目，legacy 另保存其现有编译测量值，不要求新 Reader 构造 `CompiledPattern` 的作者缓存。

```cpp
struct PreparedRequirement final {
    StableDeclarationId stableId;
    RequirementIdentity identity;
    std::vector<PhaseDeclaration> phases;
    RequirementTiming timing;             // Former RequirementRecord::Timing; not optional.
    Tick deadline;
    std::optional<std::uint64_t> expandedPatternNodes;
    std::vector<CompiledMeasureComponent> measure;
    std::vector<AtomBinding> atomBindings;
    std::vector<ResourceClaimDeclaration> resourceClaims;
    std::optional<ClaimPolicyDeclaration::CompetitionKey> independentCompetition;
    std::optional<PatternExecutionProgram> executionProgram;
};
struct PreparedResource final {
    std::string resourceId, slotToken;
    std::vector<std::uint32_t> requirementIndices;
    bool terminalAfterTermination;
};
struct CompiledGameplayView final {
    const PreparedTimebase& timebase;
    const LatePolicyParameters& latePolicy;
    const PreparedIdentityDeclarations& identities;
    std::string_view contentIdentity, preparedIdentity, executionProfile;
    const PreparedRuleset* ruleset;          // Legacy only.
    const PreparedNormalizedScore* normalizedScore; // Beat-us v3 only.
    std::span<const PreparedRequirement> requirements;
    std::span<const PreparedResource> resources;
    std::span<const PreparedTimerKey> timers;
};
struct PrepareGameplayLimits final {
    MeasuredParameter<std::uint64_t> maxRequirements, maxResources, maxMeasureComponents;
    MeasuredParameter<std::uint64_t> maxExpandedPatternNodes, maxAutomatonStates;
};
```

`RequirementTiming` 拆出原 `Timing/SuccessWindow/PhaseTarget`；`CompiledMeasureComponent` 保留 legacy `gradeTable`，v3 另有 `std::optional<std::uint32_t> gradeTableIndex`，指向下文 v3 表，两值互斥，category 继续由 phase 派生。同名 Measure 可被不同实例选不同表：v3 的 GPD0 声明 `gradeTokens` 恒空，各实例的运行等级仅由对应 JPR1 table index 派生。资源 rows 从各行已解析 claim/competition 生成；保留需求索引的原争用序、槽标识和原 GRC0 `terminalAfterTermination`，不得重新推断 Free/Terminal 或重跑 solver。运行 Fact Ledger 只需 phase/requirement 身份，呈现 `FactBinding` 归 Playback；Reader/SDK 校验存在性与目标引用。没有 `executionProgram` 的 legacy 数据只可供原静态观察。`SessionConfiguration` 的 `OwnedTimebaseProfile` 换为 `PreparedTimebase`；`OwnedInputMappingProfile`、身份、校准、执行/晚到/fact token 和 legacy ruleset 保留。v3 session 从 Prepared 取 normalizedScore；传 legacy ruleset 或宿主覆盖拒绝。旧局只走旧 Fold。

`PreparedGameplay::records() noexcept -> CompiledGameplayView` 借本句柄；`requirements()/resources()/timers() noexcept` 返回对应 `span<const T>`，`identity() noexcept -> std::string_view` 返回 prepared identity，另保留 `admitsSuccess`、`settlementCapacity() noexcept -> std::size_t`、`settlementDictionary() noexcept -> const SettlementDictionary&`。同步 `prepareCompiledGameplay` 在发布前校验身份、late policy、所有交叉索引、窗口/可达等级、计分覆盖和正负边界；编译源先过原 `ContentProfileLimits/PatternCompileBudget`（包含完整展开），Reader 先过 `PackedChartLimits` 或 Graph 限额。`PrepareGameplayLimits` 从上述既有实测 requirement/resource/measure 和 expansion/state 限制投影，pending 不代入数字；状态数实数及展开计数在装载校验时逐记录比较。P 的额外可表示性在同一 prepare 入口检查。

`cuexis/judgement/advance.hpp` 定义以下值。`advance`、`submitCanonical(std::vector<CanonicalInput>) -> core::Result<std::vector<InputReceiptPending>>` 均为 `JudgementSession` 成员，输入沿现行规范化、序号、RPA occurrence 与 late 校验；prepare 绑定唯一 Prepared，不接第二份 prepared。`Tick` 复用现有 64 位包装，`Outcome` 复用 `hit=0,miss=1`。

```cpp
struct RunKey final {
    std::uint64_t sessionScope, runEpoch;
    friend auto operator==(RunKey, RunKey) noexcept -> bool = default;
};
enum class AdvanceStatus : std::uint8_t { ongoing, completed, faulted };
enum class AdvanceFaultStage : std::uint8_t { none, kernel, fold, control };
enum class AdvanceFaultReason : std::uint8_t { none, resource, arithmetic, invariant };
struct FaultCode final { AdvanceFaultStage stage; AdvanceFaultReason reason; };
enum class TimingDirection : std::uint8_t { early, exact, late };
struct NormalizedScoreResult final {
    std::uint64_t j, jMax, c, cMax;
    std::uint32_t mj, mc, score, combo, maxCombo;
};
struct SettlementEvent final {
    Tick tick;
    std::uint32_t phaseIndex;
    std::uint32_t gradeIndex;  // UINT32_MAX: absent.
    Outcome outcome;
    std::optional<TickDelta> error;  // Actual input observation minus phase target.
    std::optional<TimingDirection> direction;
};
struct SettlementPhase final { RequirementIdentity requirement; PhaseKind phase; };
struct SettlementDictionary final {
    std::vector<SettlementPhase> phases;
    std::vector<std::string> grades;
};
struct AdvanceOutcome final {
    RunKey run;
    std::size_t written;
    std::optional<Tick> processedThrough;
    AdvanceStatus status;
    FaultCode fault;
    std::optional<NormalizedScoreResult> score;
};
[[nodiscard]] auto advance(Tick horizon, RunKey run, std::span<SettlementEvent> slots)
    -> core::Result<AdvanceOutcome>;
```

事件及成绩均按值、可平凡拷贝且不抛，`sizeof/alignof/offsetof` 由同一工具链按声明决定；不作 wire，不跨 API 类型强转。`tick` 为结算时刻，`error` 是 `observationTick−target` 的受检 `TickDelta`；证据存在且阶段有选表才独立算方向和 grade。无选表时保留真实误差，方向、等级为空；timer Miss、无证据 Hold 阶段或无输入的 Pattern 成功三项都空。真实 release Miss 可同时有档与方向。`observationTick` 取[Playback 会话换算](#playback-时钟与实时准入)后的观测值，已含设备校正、谱面 offset 与输入补偿，不含显示提前；显示提前只进 `presentationTick`。legacy 保留原 grade/误差，方向为空。字典 phases 按 canonical requirement 顺序及其 phase 声明序；grades 为不同等级标识的 UTF-8 字节序。字段在 seal 前具备，按 ledger 中新 fact 顺序交付。字典为 owning 值，宿主开局前复制并分配槽；卸载后仍能解释保留的结果。v3 `score` 仅在该次首次正常终止（自然完成或成功 Stop）有值；ongoing、faulted、重复已完成调用及旧局均为空。旧摘要保持原入口。

**帧观测缺失的报告。** 新会话的观察查询如实报告帧观测记录的缺失，并标明缺失计数是否精确：`lossEvents` 为已知缺失数，`exactLossCount` 仅在缺失计数精确时置 `true`，即所有缺失来源都被单独计数、没有无法计数的丢失；任一来源未知或计数饱和时为 `false`，缺口另由覆盖区间与 `attemptsEvicted`、`feedbackEvicted`、`receiptLoss`、`historyMisses` 说明。不得把不完整观测当作完整证据。判据细节随 T-14 验收收敛。

P 从已展开、去重并验证的 requirements 累加 phase 实例，和 [kernel `phaseOffsets`](../../../../../engine/judgement/src/execution_kernel_impl_setup.cpp) 同序；`(RequirementIdentity,PhaseKind)` 唯一。现有 [`outcome`](../../../../../engine/judgement/src/execution_kernel_impl_phases.cpp) 检查 Pending 后才写终态；每个封定 phase 只交付一次，恢复历史建状态不重发。一次 `advance` 最多 P 条，含计时 Miss，非阶段事实不占槽。检查累加、`P<=UINT32_MAX`、等级数 `<=UINT32_MAX` 和 `P<=SIZE_MAX/sizeof(SettlementEvent)`；SDK 投影单独检查其类型。P=0 合法。P 只界定事件容量，不给计分定分母；`phaseDeclarations` 只作交叉核对，requirement/实体/pending 限额不能代用。槽分配失败拒绝开局，不增预算框架。

用已有 session owner-thread 串行推进；slots 独占且调用期有效；不调用宿主、不重入。预检顺序：错误线程/重入、未准备/已经故障、run 不符/为零、horizon 逆行、槽少于 P。此前失败返回 error，不消费输入、不写槽、不修改状态。预检后按 tick 事务执行，先得到可拷贝记录再 seal。seal 成功即递进 frontier 并写槽；结果交付从此不分配、不抛。内核草稿及独立 Fold 原有分配/失败边界仍存在。首个或后续 kernel、Fold/control 失败返回承载 `faulted` 的 outcome，保留全部已封槽前缀，未封 tick 不占槽，冻结本局。`bad_alloc` 为 `resource`，受检算术失败为 `arithmetic`，其余为 `invariant`。详细诊断返回后观察。`processedThrough` 只报告实际封定的连续处理前缀（含已认证无事件的区间）；无则空。`completed` 需要实际封定应处理的尾界、全部 phase 终态及应处理输入完成，不替代 EOF；重复同 horizon 或已完成同局调用可零写入。检查 `written` 外的槽保持原值；Fold 失败不能交付成绩。

**Stop 截止（与 [Player D-15 接线](render-player-writer.md#所有权暂停与收尾) 共用）。** 增仓内 `detail::StopCut { RunKey run; std::optional<IngressSequence> acceptedThrough; Tick cutoff; }` 与 `detail::advanceStopCut(JudgementSession&, const StopCut&, std::span<SettlementEvent>) -> core::Result<AdvanceOutcome>`，放 `engine/judgement/internal/cuexis/judgement/stop_cut.hpp`，Playback 是唯一生产调用方。`acceptedThrough` 为现有 ingress 接收序号的含端上界（`nullopt` 表空，序号 0 有效），与 capture 消息投递序号分开；`cutoff` 为 Stop 实际时刻的含端逻辑 tick。会话 owner 先预检 owner/run、`cutoff>=sealedFrontier`、槽 P、输入边界可读取，并用 `acceptControl` 阻止等候的普通推进；随后关闭 capture 路由，沿现有确认规则收齐最后完整批片和 epoch end，在 kernel 接收仍开放时按原规范化、晚到策略和原排序调用 `receiveCanonicalBatch` 完成编号及记录。所有前缀成员接收完成后，在同一串行事务调用 `acceptedIngressThrough` 读取真实上界，再由 `advanceStopCut` 原子核对 run、截止及上界并关闭 kernel 接收、执行 seal；query 到关闭之间不处理其它输入。不能用投递序号代替输入前缀。关闭后只排空已入核且生效 tick `<=cutoff` 的 pending，不再提交末批或分配编号。已被原规则拒绝、合法延至 cutoff 之后或未在前缀接纳的输入不结算；不得改写 tick、绕过晚到策略或补造输入。

**接收游标的生产与读取。** T-10 在 `ExecutionKernel::Impl` 的同局接收状态中唯一持有 `optional<IngressSequence> acceptedThrough` 与关闭标记，初始为空/开放。在 `engine/judgement/internal/cuexis/judgement/canonical_ingress.hpp` 定义仓内 `detail::receiveCanonicalBatch(JudgementSession&, std::vector<CanonicalInput>&) -> core::Result<std::vector<InputReceiptPending>>`，由 `session.cpp` 的窄友元 access 进入 `execution_kernel_submit.cpp`。该 v3 入口完成 owner/run、整批 canonical 内容和重复检查、记录及 pending 所需预留后，以原 canonical 顺序分配连续序号并回填传入 batch；随后执行原 late/admission。接收记录与游标一起提交，late 拒绝仍保存带真实序号的 `ReplayRecord::batch/rejection`；canonical/重复/容量/预留失败无编号、无记录、游标不动。v3 原 `submitCanonical` 通过同一实现处理其局部 batch；legacy 保持原入口、调用方序号及接受集。不建第二个接收队列，恢复沿下文的既有 snapshot/replay 状态。

T-14 把 `ExecutionScheduler::deliver` 在完成全部批片、计数、source-local positions 和去重之后的 `session_.submitCanonical(members)` 接到此仓内入口；返回的同一 `members` 携真实编号供 `AdmissionDecision` 与归档核对，晚到拒绝也不丢编号。`deliverGameplayCapture` 的 `canonicalOccurrence` 只生产 key，不能把调用方提供的 sequence 或 `deliverySequence` 当作接收游标。`controlRpa` 首次合法 Resume 的 trace 在同一 kernel 使用同一编号分配器，控制 seal 同时提交 `reconciliationIngress` 和游标；seal 前拒绝不改游标，seal 后 Fold fault 保留已封编号。capture/request/trace 重投均不再分配。Play/EOF 换 segment 不更换此 owner，Retry/Reload 新 run 重新初始化。

同一 `stop_cut.hpp` 由 T-10 声明、`session.cpp` 实现 `detail::acceptedIngressThrough(const JudgementSession&, RunKey) -> core::Result<std::optional<IngressSequence>>`，直接读取上述 kernel 状态；校验 owner、run 和准备状态，`nullopt` 明确表示合法空前缀，错误不能折算为空。T-14 的 Playback Stop owner 按上段次序完成预检、`acceptControl` 与最后批片收齐；所有前缀成员完成上述接收后，在同一串行 owner 事务读取此函数，构造 `{run, *bound, cutoff}` 并调用 `advanceStopCut`。后者再核对传入上界与 kernel 当前值全等，才锁住接收并 seal；读取与锁定之间不处理其他输入命令。Paused Stop 复用已关闭的 capture 边界并读同一游标。host channel 不传另一个计数，`confirmedSequence()` 只证明 capture 消息连续，不能生产 `StopCut.acceptedThrough`。query 与接收入口都只在仓内头可见，不增加 Playback 安装 DTO 或 shared 导出。

复用 `advanceRecorded` 的 tick/事务循环，拆一个内部 `advanceToFrontier(frontier, slots)`。普通推进继续计算 `horizon−windowCloseThreshold`；Stop 在已锁接收前缀证明完整后直接传 `frontier=cutoff`。已暂停时仍可在同一冻结位置收尾，不调用 Resume；Stop 专用入口只放行该截止事务，不打开普通推进。现存 `controlDrain` 只记回放，不能覆盖 frontier；不伪造 `horizon=C+threshold`。timer、输入、signal 仅处理 `<=C`，不为未到期 phase 补 Miss；前缀为空也可 seal 截止。成功=`completed` 表示 Stop 前缀已结清；同次调用返回全部新增事件。锁后排空或执行故障返回 `faulted`，保留已封事实和槽前缀，不交付正常成绩。Playback 转交独立成绩后清局；重复 Stop/EOF 由现有 owner 终止凭据幂等。

Stop 的归一化成绩沿用编译时的整谱 `Jmax/Cmax`，只累计实际结算前缀的积分。

**Stop 内存回放。** 只在 Stop 成功时追加终止归档行。`RecoveryInputs` 为新 prepare 保留已绑定的 `optional<RunKey>`（旧局空）。在最后一条 `ReplayRecord` 保存 `optional<StopCut>`、实际 `processedThrough` 与终态 `result`；此行的 `horizon`、`control`、`rejection`、`batch`、`admission` 为空，`controlDrain=false`。关闭 capture 后的最后完整批片须在 kernel 接收仍开放时经上述 producer 编号，并写入 batch/admission（或 rejection）行，随后才 query 上界、由 `advanceStopCut` 核对并关闭 kernel 接收。终止行之前直接保留这些已经录得的行，包含合法但尚未调度或规则延至 C 之后的项；关闭时不补录、重投或重新编号，不记录无法规范化的原始数据。`acceptedThrough` 为整个已接收 canonical 前缀（含 trace）的最大 `IngressSequence`，无输入为空，晚到拒绝不会反改接收上界。

v3 会话 owner 在合法 canonical 输入首次被接收时、执行 late/admission 决定之前，由上述 kernel 唯一编号器分配 `IngressSequence`。RPA 在确认无重复且批片已完整、按原 canonical 排序后调用该 producer，使用回填的成员编号；只有接受真实 trace 的首次 Resume 在 RPA control 归档中按 trace 顺序保存同一编号器分配的各转换 ingress 编号（`ReplayRecord::reconciliationIngress`，按值 `vector<IngressSequence>`，不改已安装的 `GameplayReconciliation`）。批次、trace 或 request 重投不再分配，无法 canonicalize 的数据不分配。`0` 是首序号，维护 `optional<IngressSequence> acceptedThrough`；超过 `UINT64_MAX` 在接收前整批拒绝 `host.rpa.capacity_exceeded`，绝不回卷。跨 Pause/Resume/EOF segment 延续，Retry/Reload 新 `RunKey` 才重置。与 capture delivery 和 reconciliation 的序号各自独立。晚到拒绝仍将实际 `CanonicalInput::sequence` 和原错误写 `ReplayRecord::batch/rejection`；尚未调度的合法输入也携同一序号，不以数组位置、有效 dispatch 序或已计分事实替代。Stop 校验的是整个已锁接收前缀的上界（含这些拒绝和已控制处理的 trace）。

完整 `ExecutionKernel::evaluate` 在普通 horizon/control/batch 分派前识别 Stop 行，校验恰一行、最后一行、字段形状、`run` 与 `RecoveryInputs` 绑定、`acceptedThrough` 与重放的已接收 canonical 序号上界一致。前序 batch 按原方法重放并校验；Stop 直接进实时 Stop 共用的内部 cutoff 路径，禁用宿主 ingress、结果槽与记录再追加。要求 `completed`、实际 `processedThrough==cutoff`、终止状态（关闭该 run 接收/推进）以及 `sameKernelResult` 与原结果相同；`sameReplayHistory` 另比较 StopCut 三字段、processedThrough 与终态，不使用二进制摘要代替。无新的宿主事件或成绩回调。非末尾、重复、伪造边界或结果、故障终态均以 `judgement.execution.relation_invalid` 拒绝。实时 Stop 发生故障时仍交付已封前缀与 fault；该局不产生可回放的 Stop 行，`archive/snapshot` 按 history unavailable 的既有错误拒绝，不将截断前缀冒充完整归档。

`replayCutAt` 查到末尾 Stop 时，`horizon>=cutoff` 返回包含终止行的完整行计数，`partialHorizon=nullopt`；更早位置只对终止行之前的记录执行既有 cut。`seek/checkpoint` 校验拒绝 `horizon>=cutoff` 或任何显式 cut 消费终止行（含部分切入）；`restore/recover` 拒绝携带终止行或 Stop 终态的输入。v3 非终态 `SnapshotDTO` 保存同局 `RunKey`、`acceptedThrough`、全部已接收 canonical 序号（含 rejection 和 trace）及完整 normalized Fold `{remainingPerUnit, settledPerUnit, j, c, combo, maxCombo, factCursor}`；同时保存原 sealed cursor、Resume re-fold 的同 tick Fold 基线、pending 与历史。`sameKernelResult/sameReplayHistory` 及非终态恢复核对这些字段和实际录得的序号前缀；发现缺号、重号、上界不符或状态不一致即拒绝，不能重新编号或略过 Fold。成功恢复只恢复同局状态，不重发事件/成绩。v3 recovery 全程仅内存；所有 v3 archive/snapshot/checkpoint 的旧二进制编码、旧解码要求使用 v3 dependencies 或依赖旧编码的 `canonicalIdentity()` 一律拒绝 `codec.version_unsupported`（含无 Stop 的局）。旧 revision 2/3 与旧 `controlDrain` 维持原语义。

实验 SDK 在 `experimental/gameplay.hpp` 新增 `[[nodiscard]] CUEXIS_PLAYBACK_API` 的 `advanceGameplay(PlaybackSession&, GameplayTick, GameplayRunKey, GameplaySettlementBuffer&) -> core::Result<GameplayAdvanceOutcome>` 和 `prepareGameplaySettlementBuffer(const PlaybackSession&) -> core::Result<GameplaySettlementBuffer>`。Playback DTO 逐字段对应运行 DTO（前缀 `Gameplay`）；`tick/processedThrough` 用已有 `GameplayTick`，`outcome` 用已有 `GameplayOutcome::Hit/Miss`；`error` 为 `std::optional<std::int64_t>`（自身玩法的 tick delta，v3 为 µs）；`GameplayTimingDirection : std::uint8_t { Early, Exact, Late }` 从 0 连续，`score` 为 `std::optional<GameplayNormalizedScoreResult>`，同字段名/定宽类型。`GameplaySettlementDictionary` 按相同索引，phase 元素使用现有公开的 requirement/phase 标识。`prepare` 同步绑定当前 prepared/run、预分配 P 个 kernel 槽和 owning 字典，失败不影响本局。move-only `GameplaySettlementBuffer` 用私有实现；`at(std::size_t) const noexcept -> std::optional<GameplaySettlementEvent>` 按值读 `written` 范围，`dictionary() const noexcept -> const GameplaySettlementDictionary&` 借 buffer。seal 后只做固定大小转换，不重新分配、复制字典或布局强转。预检错误保留 buffer；内容卸载不废弃宿主持有的 buffer。同一个 buffer 再次成功用于推进会更新 `at()` 的范围；宿主按值取出的事件始终有效，字典也可另存或持有原 buffer，不设结果有效期。

T-10 逐函数在实验 shared 导出上述两个 facade，以及 `GameplaySettlementBuffer` 的 `noexcept` 析构、move 构造、move 赋值、`at`、`dictionary`（非 inline 的定义/声明均使用 `CUEXIS_PLAYBACK_API`；复制删除）。`VerifySharedExports.cmake` 只精确允许这些限定名及对应工具链的 ABI 析构变体；私有构造与实现类型不导出，默认库在 R5 基线上不新增该组 Gameplay 符号。实验 shared 独立消费者须实际构造、移动、访问和销毁，以验证所有链接路径；不以 static 通过替代。

### 实时调度的封定与单次回复

本节只用于绑定 v3 `RunKey` 的实时局。T-10 提供同步 seal/DTO 和可复用的 drain；T-14 接已有 owner 调度与回复。`advanceGameplay` 直接调用只用于非实时 owner；实时局拒绝绕开水位的直接调用。已安装 `GameplayAdvanceDecision`、旧函数签名、旧局执行和决定查询保持原合同。

**实际写点。** 在 `execution_scheduler.hpp` 增内部 `ScheduledSettlement { AdvanceRequest request; uint64_t decisionOrdinal; AdvanceOutcome outcome; }`。v3 内部 `request`、`deliver` 复用原参数和返回类型，各额外接 `std::span<SettlementEvent>` 与空的 `std::optional<ScheduledSettlement>&`；二者把同一输出传给 `tryStart`。owner 在输入变更前验证 buffer 的 run、prepared 身份与 P；等待记录只存现有 request/decision，不存 span、事件或成绩。连续 delivery、完整批片和真实 prefix 更新水位后，只有 `verifiedExclusive > H−W` 才可开始。占用原决定 ordinal 后，由 `tryStart` 调 v3 同步 seal，写本次槽并填原 `requestId/commandOrdinal/context` 对应的 `ScheduledSettlement`；成功、故障分别填原 `executed/failed` 决定。若 `request` 当场封定，同次请求带回；若先为 Waiting，由真正触发 seal 的 `SessionCaptureDelivery` 回复携带原推进请求身份及结果。空水位、相等水位、批片缺口、等待、未受理、取消和时钟撤权均无 settlement。`SessionQueryAdvance`、同 ID 同内容重投、`retire` 只读或回收原决定，不能再取事件或成绩；异内容沿原 conflict 路径。

**Pause drain。** `acceptControl` 先取消仍等待的普通请求；验证实际 `EndRunEpoch`、匹配 cut/pauseEpoch 与完整捕获闭合证明后，`ExecutionScheduler::pause` 用额外 `span<SettlementEvent>`、`optional<AdvanceOutcome>&` 接 v3 drain 输出。保留旧 `checked(cut+W)` 和 `controlDrain=true` 的回放记账，实际封定用同一 `advanceToFrontier(cut, slots)`。`pauseExecutionStep` → `pauseRealtimeGameplay` → scheduler 将输出透传至 `HostCoordinationAccess::applyPause`/`SessionPause` 原 `StepKey` 回复；只有 `ledger.start` 首次执行时可能写。若 seal 后 `controlRpa(pause)` 或 `adoptTailPause` 失败，将该 drain outcome 改为 `faulted`、`fault.stage=control`、`score=nullopt`，保留 `written/processedThrough`，暂停提交失败并冻结故障局。后续 renderer/音频/清理失败也必须保留已交付的 drain 事件；外围失败不造新的 gameplay outcome。Pause 合流、重投及步骤查询只返回原决定。

**Resume seal。** `SessionResume` → `HostCoordinationAccess::applyResume` → `resumeExecutionStep` → `resumeRealtimeGameplay` → `ExecutionScheduler::resume` 沿 Pause 同一槽/可选 outcome 参数链。`controlRpa(resume)` 在 cut 可能因真实暂停期间 release 中断已持有 Hold，写阶段 Miss；无误差证据则无档/方向。现行 `execution_kernel_advance.cpp::controlRpa` 在交接 `impl_->active` 时已 seal、随后独立做 Fold。v3 在该交接写新增阶段事件，Fold 重算从已有 tick-start checkpoint 同时恢复 normalized Fold，不能再发同 cut 既有事件。Fold 失败仍交付 Resume 已封前缀，`fault.stage=fold,score=nullopt`，不授权新路由；证明无效或旧重投无新 seal、无事件。Resume 不推进 cut，不因 Hold 中断把控制事务当正常终局；恢复或后续普通推进真正满足终局条件时才正常结清。Pause 也暂不消费正常终局成绩；成功 drain 达到自然尾界时，由随后同局普通完成或 Stop 确认终局一次。

**仓内载荷与所有权。** 在 `app/execution_host/session_worker.hpp` 定义 `RealtimeAdvanceOrigin { GameplayExecutionContext context; uint64_t requestId,commandOrdinal,decisionOrdinal; }` 和 `SessionSettlementReply`，字段为 `GameplaySettlementBuffer buffer`、`optional<GameplayAdvanceOutcome> outcome`、`variant<monostate,RealtimeAdvanceOrigin,StepKey> origin`、`core::Result<variant<monostate,GameplayAdvanceDecision,StepReceipt>> decision`。`monostate` 决定表示 capture 成功；未开始执行的回复无 outcome，origin 为 `monostate`。`StepKey` 绑定 Pause/Resume/Stop，须匹配原操作和完整旧/新路由目标；advance origin 绑定原请求和封定 segment。v3 `SessionRealtimeStart` 在开启 capture/出声前依同一 prepared/run 预配一个 owning buffer（P 槽及字典），经原启动回复移给控制者；预算不足拒绝开局。控制者用私有 `SessionSettlementCommand` 封套把 buffer 移入已有 `SessionCoherentAdvance`、`SessionCaptureDelivery`、`SessionPause`、`SessionResume` 或 Stop 会话命令，`SessionOwner::dispatch` 借给同一 owner 的 Playback。`SessionWorker::postSettlement(SessionSettlementCommand&) -> core::Result<uint64_t>` 在原 channel 锁内检查通过后才移动入槽，拒绝时原调用方仍握 buffer。v3 实时路始终最多一个物理 P 槽 buffer；它随当前 session job 来回，Waiting 归还空 buffer。控制步骤在取回上一 job 后执行；不按 retained-message capacity 乘 P。

**现有 terminal/ack。** `SessionReply` 新增 `GameplaySettlementBuffer`（启动）、`SessionSettlementReply` 及 `SessionSettlementStatus { bool taken,ok; optional<GameplayAdvanceDisposition> advance; optional<StepStatus> step; }`（重读）；旧 alternatives 保留。`SessionWorker::poll` 在 `OwnerChannel` 的原锁内用内部 `pollProjected(id, project)` 对已完成的可变 `Result<SessionReply>&` 投影：旧回复原样复制；首次读启动/结算回复以不抛移动取出拥有值，原 slot 留固定大小 `SessionSettlementStatus`（成功位和原 disposition/status）；再 poll 只读该状态，ACK 正常回收。无新消息、结果队列、ACK 或有效期。封定后同步状态、决定摘要或诊断发布失败仍移交 outcome，将错误放 `decision`；不得降为外层 error 吞掉已封前缀。错 owner、重入、错 run/prepared、槽不足或关局在输入/记录变更前拒绝，归还 buffer、无 outcome；事务内 kernel/Fold 失败返回已封前缀。所有结果投影只读 `written`，未执行回复对宿主暴露零条。错 run/segment/operation 的回复仅回收原槽，不交付或复用到新局。控制者先按值取事件及 `score`，再处理决定/故障，按原捕获证明 ACK delivery；未完成原 session job 时不得重投来补结果。取出的值和独立持有的字典不受 ACK、复用或卸载影响。

## 归一化计分与 Prepared 记录

**终局交付。** v3 著录的唯一成绩是首次正常结清或成功 Stop 当次交付的 `GameplayAdvanceOutcome::score`，带 `RunKey` 按值提供 `j,jMax,c,cMax,mj,mc,score,combo,maxCombo`。会话 owner 随当次结果获得这个值，若判定先于 EOF 结清则保存这一局的值，按 [Player 终止与交付](render-player-writer.md#所有权暂停与收尾) 在宿主收尾时移交。此动作不查询 `queryGameplay`、`SessionGameplayResult` 或旧 Fold，不重算、不重发；若已结清而尚未 EOF，又接受 Stop，仍使用首次终态值。`CompletedRun.result` 是该处定义的 `variant<GameplayResult,GameplayNormalizedScoreResult>`：旧路径交付前者，v3 只交付后者。普通播放、未实际开玩和 faulted 局无正常终局成绩；真实零分仍有完整载荷。

**独立观察。** v3 在私有 kernel projection 保存独立 normalized Fold（原积分、combo、maxCombo、phase Hit/Miss 计数、`factCursor`），不伪造 legacy `FoldProjection`。`queryGameplay()` 仍可返回 owning `GameplayResult` 来观察健康的已 Fold 前缀；其 `GameplayScore.score` 是同一舍入 helper 输出（`u32` 无损扩到 `i64`）；`combo/maxCombo` 取单元连击（无损扩到 `u64`）；`hits/misses` 统计已 Fold 的真实 phase Hit/Miss，含未计分和级联 phase，不按单元去重。`state()` 保留原状态投影，`factCount()` 取 sealed cursor，`publishableFactCount()` 取 v3 Fold cursor，`sameResult()` 比较原内核结果及完整 normalized Fold/基数。观察失败沿既有码，不能改变或补取终局交付。共享公开类无宏条件字段或新增成员。

`engine/judgement/include/cuexis/judgement/normalized_score.hpp` 由 T-10 提供。运行侧仅使用标准库、Core 和 Judgement 运行头。作者适配和成品 Reader 共用以下逻辑记录与验证；布局不作 wire。

```cpp
enum class ComboAction : std::uint8_t { keep, increment, reset };
struct ScoreRuleRow final {
    PhaseKind phase;
    Outcome outcome;
    std::optional<std::string> grade;
    bool qualifies;
    std::uint64_t judgePoints;
    ComboAction combo;
};
struct ComboRewardSegment final { std::uint32_t from; std::uint64_t rewardAtFrom; std::uint32_t slope; };
struct ScoreMode final {
    std::string id;
    std::uint32_t total, mj, mc;
    ComboAction onExhausted;
    std::vector<ComboRewardSegment> comboRewards;
    std::vector<ScoreRuleRow> rules;
};
struct PreparedGradeInterval final { std::int64_t start, end; std::string grade; };
struct PreparedGradeTable final {
    std::string id;
    std::int64_t exactStart, exactEnd;
    std::vector<PreparedGradeInterval> intervals;
};
struct ScoreUnit final { std::vector<std::uint32_t> phaseIndices; };
struct ScoreBasis final { std::uint64_t jMax, cMax; };
struct PreparedNormalizedScore final {
    ScoreMode mode;
    std::vector<PreparedGradeTable> gradeTables;
    std::vector<ScoreUnit> units;
    ScoreBasis basis;
};
```

表按 id 字节序；档区间按 start；规则按 `(phase 枚举值,outcome 枚举值,无 grade 优先/grade 字节序)`；`phaseIndices` 递增，每个 phase 最多出现一次；单元非空，按首 phase 索引递增。索引使用当次输出字典的展开顺序。表、规则、阶段和单元数组按 `u32` 计数并校验既有输入/解码上限与 `size_t`。成品拒绝非 canonical 顺序，源适配在验证无重复后排序。未使用的合法表和规则也保留供身份与导出。grade table 与计分模式只访问编译 tick，不收时间或作者类型。

**单元与资格。** mode 有本阶段的合格局部评分键时，普通 phase 默认形成单例；作者可显式停用或用同名 `unit` 把互斥替代的 phase 合为一个单元。同一 Hold 的 head/body/tail 若独立可累计则分别计分；同阶段的 Hit、release Miss、timer Miss 本就只能出现其一。同 Pattern 的多条可接受路径只结算一个原 phase，不按节点计分。声明为竞争替代的多 Requirement 按原竞争规则运行；共享计分只能由作者显式声明，不从同 lane、重叠窗口或共享资源猜分组。ObserveOnly 没有 phase outcome，不入计分单元，显式分组拒绝。未入单元的 phase 继续运行、发事件和更新原统计，不改 `J`、`C` 或 combo。

按已封 ledger 的 phase fact 顺序处理。按 phase、Outcome 和可选 grade 查精确规则；`qualifies=true` 时，尚未完成的单元立即且仅一次消费该 fact，先加 `judgePoints`，再按 `combo` 执行 keep、加一或清零。只有 `increment` 累加 `reward(更新后 combo)`；同时更新 `maxCombo`。`qualifies=false` 不立即改积分或 combo：每个成员第一次终态扣减该单元剩余数，最后一个成员终态仍未有合格结果时结算 0 积分，按 `onExhausted` 清零或保持。合格后剩余成员仍正常返回原事件，不再计分。该单元先遇到无证据 Miss 仍可由后续合格成员赢得积分；两个独立 Hold 单元会分别收到级联 Miss 并独立处理。提前 Stop 不强行耗尽未终态单元。v3 Fold 状态预分配每单元剩余数、已结算位及统计并保持 fact cursor；按事务发布，Fold 失败保留已封事件但不发布成绩。不把 score 或 combo 编进结算事件。

**局部键集合。** 按 phase 的无档计时/级联键及真实误差的局部整数区间生成。tap/head 有 null Miss；无表且有非空成功域时有 null Hit，有表时以成功窗和各档的整数交集生成 Hit 键；`accepting[start]` 的 tap 另有 null Hit。Body 有 null Hit/Miss，release 生成域内带档 Miss；Tail 有 null Miss，以尾成功域生成 Hit，release 可在整个有效释放域产生 Miss（dispatch 或 atom 也可令窗内观测判 Miss）。无表的 release Miss 合入 null Miss。窗口与域按[作者量化规则](#作者版本stop比例和最小多等级)裁剪。对已入单元的 phase 的每个局部键要求精确规则，禁止缺键隐式回退。跨谱模式可以保留本谱不存在的键，分母不计这些键。

**固定满分上界。** 每单元 `Uj=max(0, 所有成员局部合格行的 judgePoints)`；`K` 为至少有一条局部合格 `increment` 行的单元数。`Jmax=Σ Uj`，`Cmax=Σ reward(k), k=1..K`，空和为 0。奖励按 `from` 选择最后一段，计算 `rewardAtFrom+slope*(k−from)`；`from` 首项为 1、严格递增，段衔接不下降（比较前一段最后一点与新段首值）。斜率 0 可封顶；分段可改斜率及向上跃升。初始分、combo 均为零。每单元最多加一次，第 i 次加一后的 combo 至多为 i，故两式分别覆盖任何结算前缀；同一单元的多个互斥 phase 分别取 max 或计一次，不累加为 N。两个上界不承诺同一局可同时达到。物理输入、竞争、Pattern 和窗口交互可使谱面的实际上限更低；作者负责谱面配置与实测，无全局搜索、无作者输入见证、无 Reader 重放或相应准入。分母不会因运行实际结果降低。

T-10 在 build-only 运行边界提供 `deriveScoreBasis(std::span<const PreparedRequirement>, const LatePolicyParameters&, const PreparedNormalizedScore&) -> core::Result<ScoreBasis>`。输入不需最终身份。复算前拒绝空单元、越界/重复 phase、ObserveOnly、全组无局部合格键、非法 `onExhausted`（只收 keep/reset）；全组无资格报 `judgement.execution.relation_invalid`、`reason=score_unit_unqualified`，标出单元下标。所有不合格规则一律 0/keep，包含本谱未使用的行。源写计算值；`prepareCompiledGameplay` 用同一函数逐项核对声明基数，与纯 Reader 共用局部验证；错值报 `judgement.execution.relation_invalid`、`reason=basis_mismatch`，路径 `/scoreBasis/jMax` 或 `/scoreBasis/cMax`（Reader 路径按 wire 段）。完整 mode、表、绑定、单元与基数按[成品合同](prepared-artifact.md)进入身份，解码无需重编。

**整数边界与输出。** 检查 `mj+mc==total<=UINT32_MAX`（先 u64 加）；`Jmax/Cmax`、每次加与奖励输出均为 u64，combo/maxCombo/K 为 u32，任何加乘溢出在 prepare 拒绝，禁止饱和或低位环绕。`mj>0` 必须 `Jmax>0`，`mc>0` 必须 `Cmax>0`；未启用分量精确贡献为 0（即使分母也为 0），不重分配份额。两个分量皆未启用时合法输出 0，仍保留模式、原事件与精确累加器。对启用分量，保留原整数比例，不在 Fold 计算任何浮点数或逐条取整。输出时令 `dj=(mj==0?1:jMax),dc=(mc==0?1:cMax)`，构造 `A=mj*j*dc+mc*c*dj`（未启用项为 0）、`B=dj*dc`，整除得 `q,r` 后用 `r` 与 `B−r` 比较；大于半数加一，恰半仅在 q 为奇数时加一。`A <= total*B < 2^160`、`B < 2^128`，在 `normalized_score.cpp` 用私有五个 32-bit limb 的无符号栈值、u64 乘加进位和从最高位到最低位的移位减法除法（余数在每步归约，比较前可达 `<2B`）；所有缩窄和进位边界检查。禁止 `__int128`、依赖浮点模式、外部大整数或新增 Core 公共 helper。得到 `[0,total]` 的 `uint32_t`；越界是 invariant fault，不夹值。`NormalizedScoreResult` 保存四个原积分数与两份满分，可重建精确比例；Stop 仍用完整谱分母。

`Cmax` 对 k=1…K 逐项求受检 u64 `reward(k)` 并累加（K 不超过已验证 phase 数）；不构造等差和的双倍中间值。两个 `UINT64_MAX/2` 合法，和为 `UINT64_MAX−1`；若第三项仍同等奖励则溢出。段连续性检查也仅求前段最后一点。

## 谱面时间 API

`cuexis/timing/rational_beat.hpp` 提供 `timing::RationalBeat`：迁入现有 Chart 的 `create(int64,int64)`、`zero/one`、分子分母访问、`toDouble/toString`、全序与受检算术，正分母、约分、`0/1`。Chart v1–v4 继续用本身的入口和包装；旧 `judgement::RationalBeat/RationalDuration/TimebaseProfile` 只在编译适配，保留可翻号负分母、分子 `INT64_MIN` 拒绝、旧身份和旧舍入。禁止把名字相似当作可无条件互换。

`cuexis/timing/timing_map.hpp`，命名空间 `cuexis::timing`：

```cpp
struct TimeUs final { std::int64_t value; };
struct TempoEvent final {
    RationalBeat startBeat, durationBeats;
    double startBpm, endBpm, startSlope, endSlope;
};
struct Stop final { RationalBeat beat; double durationMs; };
struct TempoBoundary final { RationalBeat beat; TimeUs at; };
struct StopBoundary final { RationalBeat beat; TimeUs start, end; };
struct AnchoredBeat final { RationalBeat beat; TimeUs at; };
struct TimingSourceView final {
    double defaultBpm;
    std::span<const TempoEvent> tempos;
    std::span<const Stop> stops;
};
struct TimingBoundariesView final {
    std::span<const TempoBoundary> tempos;
    std::span<const StopBoundary> stops;
    std::span<const AnchoredBeat> anchors;
};
struct BeatSample final {
    double beat;                            // Continuous interpolation only.
    std::optional<RationalBeat> stopBeat;   // Exact at S <= t <= E.
    bool inStop;
    double stopProgress;                    // Zero outside [S,E).
};
class TimingMap final {
public:
    static auto compile(TimingSourceView, std::span<const RationalBeat> requiredAnchors)
        -> core::Result<TimingMap>;
    static auto fromCompiled(TimingSourceView, TimingBoundariesView,
                             std::span<const RationalBeat> requiredAnchors)
        -> core::Result<TimingMap>;
    auto boundaries() const noexcept -> TimingBoundariesView;
    auto matchesSource(TimingSourceView) const noexcept -> bool;
    auto beatToTime(RationalBeat) const -> core::Result<TimeUs>;
    auto timeToBeat(TimeUs) const -> core::Result<BeatSample>;
};
auto roundMicroseconds(double us) -> core::Result<TimeUs>;
```

`TimingMap` 无默认构造，以 `shared_ptr<const Storage>` 拥有 source、离散边界及积分缓存；输入 view 同步借用，`boundaries` 借 map，查询返回值。`compile/fromCompiled` 可分配，分配失败映射现有 budget 诊断；const 查询成功路不分配、不修改、不调用宿主，可并发，失败走 `core::Result`（非实时诊断可分配）。无设备 offset 或 `judgement::Tick`。Chart 编译解析 CXT/对象与动画时间声明，按[成品共享时间合同](prepared-artifact.md#共享时间与运行时边界)收集需要整数比较的拍位并精确去重；循环与动态混合保持运行求值。一次 compile 绑定同份源快照；Playback prepare 不混用其他谱的 map。`fromCompiled` 要求 `requiredAnchors` 与 tempo/Stop/Beat 0 的并集去重后恰好覆盖保存的 anchor 键；调用方从已验证的独立表现与玩法声明收集键，禁止从待验证的 `anchors` 取键。

`matchesSource` 比较 map 持有的规范化 `TimingSourceView` 值：default BPM、tempo/Stop 数量和声明序，以及各行的所有拍和 binary64 字段。输入按同一规则将浮点 −0 视为 +0；非有限、顺序/值不等即 false，不分配、不 hash、不比较 caller 字符串或离散锚点。候选源码 lowering 在调用 `compileBeatGameplay` 前从同一 Chart 时间声明构造 view，`TimingMap` 在构建时持有其规范化副本。

`fromCompiled` 合并 tempo、Stop、anchor 三表，按精确拍升序校验：同拍只可有一个离散 tick，Stop 拍的值为 S；不同拍不许倒退。对每个 Stop，所有更早拍位的 tick 必须 `<=S`，所有更晚拍位的 tick 必须 `>=E`；边界相等允许，Stop 之间不允许 `前 E==后 S`。这些关系逐项检查，不重积分。失败报 `judgement.execution.relation_invalid`（Stop 两条专有码仍按原优先级），指向相冲突的行；Reader 增加 TIM1 行路径。零拍 anchor 仍固定为 0。

`engine/chart/internal/cuexis/chart/compiled_chart_timing.hpp` 定义 build-only `chart::CompiledChartTiming { timing::TimingMap map; std::int64_t chartOffsetUs; }`。源 Chart 准备时量化一次 `chartOffsetUs=R(offsetMs*1000)`（缺省按原 Chart 的零值），与 map 一起拥有并绑定到同一 Prepared/commit；成品 Reader 返回同型值。offset 不进入 `TimingMap` 或 Judgement prepared records，也不参与离散锚点的积分。

积分沿 [Chart TimingMap](../../../../../engine/chart/src/timing_map.cpp) 的有界连续 BPM 与边缘外推。同 Beat 的 Tempo 阶跃先应用，再进入 Stop；比较和排序按精确有理拍，零 beat 固定为 0。令 `F(b)` 为无 Stop 的 tempo 累计减 `F(0)`，`L(b)=F(b)+Σ(stop.beat<b) duration−Σ(stop.beat<0) duration`（计算单位 us）。`R(x)` 最近整数、恰等距时取偶，与进程当前浮点舍入模式无关。检查有限、缩放、累计、取整后范围（`−2^63<=R(x)<2^63`），再转整数，绝不先 cast。Stop `S=R(L(b)), E=R(L(b)+duration)`，不算 `S+R(duration)`。同 beat Stop 拒绝；连续源顺序逐个验证，先检查算术、再 `E<=S`、再前 `E==S`，附原数组下标。不同拍 tempo 边可同 tick：该 tick 按升序精确拍批处理后取最终 tempo；不同音符可同 tick。

`beatToTime` 对保存的锚点返回原 tick（Stop 拍返回 S）；其它查询可积分量化，但结果不能替代已保存锚点作为事件比较。`timeToBeat` 先查 Stop：`[S,E)` 返回精确 Stop 拍和 `(tick−S)/(E−S)`，结果上界夹至小于 1 的最大可表示 `double`；`E` 返回同拍、`inStop=false,progress=0`。差值以无溢出的无符号距离计算。其它时刻按量化后的 tempo/Stop 边界选择区段；用该段已存 tick 端点把局部比例映回对应的连续积分区间，再用现有单调积分反查；端点外按对应末段 BPM 外推（只有一侧端点时用该端点对齐）。同 tick 批以最后 tempo 边为左锚；Stop 的 S/E 特判优先。反查只返回连续数值用于插值，从不比较它决定事件是否已发生，也不把它重构成作者精确拍。`fromCompiled` 只从 source 建连续插值系数；校验保存的关系、键集合、单调性和 Stop 两端，**不**用重积分结果重算、替换或逐位比对离散边界。极大拍位造成连续系数不可用时在构建阶段返回范围诊断，不传播 NaN。

## 作者版本、Stop、比例和最小多等级

保持已规划 v5 alpha、Behavior/Clip/CXT 与旧 `compileBehaviorCandidate` 接受集。可选多画幅、默认项、规约、取景范围及 prepared 类型只由 [作者画幅](render-player-writer.md#作者画幅) 定义；时间与判定模块不持画幅或设备键位。v1–v4 Reader 不增字段。输入映射按 [Player 输入](render-player-writer.md#命令与启动)。

`gameplay.version=3` 要求外层 `format="cuexis.chart",version=5` 与 `gameplay.authorProfile="gameplay.author.beat-us.v1"`。从 Chart 读取唯一 `offsetMs,defaultBpm,tempoEvents,stops`；[v4 timing 形状](../../../../../schemas/cuexis.chart.v4.schema.json)不变，Stop 只接受 `beat,durationMs`（有限正数，不接受 `endBeat`）。`offsetMs` 量化后独立保存，不参与积分。先按 version 派发，再核验 profile，不从字段推断或 fallback。无 Gameplay、旧 v5/v2 与当前纯表现 Reader 保留旧路径；默认不启用 v3。

`schemas/candidate/cuexis.gameplay-author.beat-us.v3.schema.json` 复用 [inline v2](../../../../../schemas/candidate/cuexis.gameplay-inline.t4-k4.v1.schema.json) 的非差量定义，v5 schema 用 `oneOf` 分派。新对象 `additionalProperties:false`，除 `?` 外各字段均必填。`id` 符合 `^[A-Za-z_][A-Za-z0-9_.-]{0,63}$`；`i64/u64/u32` 接受确切整数 token（小数、指数、越域拒绝）；毫秒为有限 binary64 JSON number；`Beat={numerator:i64,denominator:i64>0}`，源可未约分，编译值约分。

| 对象 / 路径 | 差量 |
| --- | --- |
| `Payload` | 保留 `sourceDocumentId,common,requirements,rankAssignment`；`version=3`、`authorProfile="gameplay.author.beat-us.v1"`。 |
| `Common` | `graphRevision="3"`；移除 `timebase` 及 `rulesetRef`；新增 `gradeTables:Table[]`、`noteTypes:NoteType[]`（各自 id 唯一，可空）、`scoreMode:ScoreMode`；可选 `presentationBindings`（缺省空数组，T-13 按[资源闭包与完整身份](prepared-artifact.md#资源闭包与完整身份)接入）。原 execution profile、协调、资源及 late 策略保留。GPH0 旧 ruleset 引用槽由编译器投影 `scoreMode.id`；Reader 核对 JPR1 `ScoreMode.id`，不接受独立值或宿主覆盖。 |
| `Timing` | `{endBeat:Beat,phaseTargets:[{phase:PhaseKind,beat:Beat}],successWindows:[{phase:Phase,startMs:number,endMs:number}],body?:{startBeat:Beat,endBeat:Beat}}`。`PhaseKind` 原 `tap/head/body/tail`，每个声明 phase 一 target。end/body 两拍为绝对拍；窗端点相对同阶段 target。保留原 hold/窗与 grace 关系；普通 Tap 的末窗右端等于最终 deadline，越过终点只结算一次 timer Miss，记录原终点。 |
| `Requirement` | `timing` 替换，增 `noteType?:id,gradeTables?:GradeSelection[],scoring?:ScoreSelection[]`。原 Measure 的 `gradeTokens` 仍必填但必须空，编译时从已选表派生。其它原字段保留。 |
| `NoteType` / `GradeSelection` | `{id,gradeTables:GradeSelection[]}` / `{phase:PhaseKind,gradeTable:id}`。每种 phase 单选；实例覆盖优先。引用须存在，实例覆盖须匹配声明 phase 和 Measure component。忽略类型上与实例无关的 phase。未选表的结果无 grade/方向。 |
| `ScoreSelection` | 互斥 `{phase:PhaseKind,unit:id}` 或 `{phase:PhaseKind,disabled:true}`，每个声明 phase 最多一条。同名 `unit` 在 Gameplay 文档展开后合为一组；缺选择按[默认单元](#归一化计分与-prepared-记录)。显式分组选入无合格键成员时，同组须有至少一合格键；ObserveOnly 不可分组。 |
| `Table` | `{id,exact:{startMs:number,endMs:number},intervals:[{grade:id,startMs:number,endMs:number},...]}`。档数组非空，可复用等级名；量化前后均 `start<end` 且相邻 `end==next.start`。Exact 必填，量化前后 `start<=0<end`，宽非零、允许非对称，端点独立于档区间。 |
| `ScoreMode` | `{id,total:u32,mj:u32,mc:u32,onExhausted:"keep"或"reset",comboRewards:[{from:u32,rewardAtFrom:u64,slope:u32},...],rules:ScoreRuleRow[]}`。奖励表非空；零奖励可用 `{from:1,rewardAtFrom:0,slope:0}`。模式可原值跨谱复用。 |
| `ScoreRuleRow` | `{phase:PhaseKind,outcome:"hit"或"miss",grade:id或null,qualifies:boolean,judgePoints:u64,combo:"keep"或"increment"或"reset"}`。键唯一；`qualifies=false` 强制零分且 keep。有实际误差的 Miss 可评分；未被本谱使用的模式键保留。 |

目标用 `R(t)`，绝对成功窗用 `R(t)+R(δ)`，等级和 Exact 相对端点用 `R(δ)`（先从毫秒乘 1000）。Tap/Head 的观测域取成功窗（head 上界再按 `body.start+1` 裁剪）。T-10 `normalized_score.cpp` 的共用局部域 helper 由 body/tail 的真实 release dispatch 区间 `[a,b)` 反推 observation：若区间非空，取 `[max(INT64_MIN,a−hop),b)`，`reject_late` 的 hop=0，`queue_next_tick` 的 hop=`maxQueueHop`，受检向下相减、下溢裁到 INT64_MIN。Body Miss 的 dispatch 为 `[最早可获取 head 的窗起点,body.end)`；Tail Miss 为 `[body.end,min(deadline,最后一个 tail 成功窗右端))`，不存在可用尾窗时真实 release 域为空。Tail Hit 还要求 observation 和 dispatch 同属各自有效窗集合；静态键用该观测域与尾成功窗交集。先处理同 tick timer，所以尾窗末端的 release 不生成带档 Miss，body timer 因同 tick 较早 observation 的 release 中断也是无档 Miss。所有 observation 域与 late 可接纳的闭范围 `[INT64_MIN+windowOpenThreshold,INT64_MAX−finalizationWatermark]` 相交，再以整数差核对区间、差值、`end−1`、表覆盖及 `target+端点`；编译、纯 Reader 和局部键一律调用同一运行 helper。这个保守静态包络不证明同时可达；实际误差始终取 observation，不用 dispatch。旧闭区间保持原载体。

CXT v2 不升版，由引用它的 Chart v5/gameplay v3 选择玩法子模式；`timing`、选表与计分单元只在该上下文解释。完整 Chart 场景、`objects`/`entities`、CXT 路径、解析和展开入口以[成品源输入](prepared-artifact.md#源输入与-lowering)为准，本节仅定义玩法差量。原展开顺序、实例身份、Pattern 与资源/争用保留，旧路径不猜新字段。

**旧准入与迁移。** legacy delta、时间身份、原窗口和 codec 沿原路径，不补 grade/Exact，不按新规则重算旧评分。v3 拒绝旧计分字段；旧 Reader 不猜新输入语义。旧源只有 tick 时不能反查补作者拍，也不从旧成绩猜 Exact 或固定满分模式；作者显式补齐后选择新分支。目标不能保真时依 [Writer](render-player-writer.md#写入规则)报告损失并失败，保留原文件和会话。

## 成品、Reader 与身份派发

[成品合同](prepared-artifact.md) 唯一规定 owning 产品、wire、版本派发、资源/身份及 CXC 闭包。Reader 和源编译共用 `prepareCompiledGameplay` 静态核验表、局部键、单元与分母。独立 Reader 不解析作者源、不生成 Pattern、不重编时间与玩法。

后继 CXC 在容器边界只检查条目包络、metadata、路径与 hash；Packed header 使用通用 Chart header 读取及具名 revision 检查。选定条目并通过编码准入后，Playback 才调用完整 Gameplay Reader。容器本身不链接该 Reader 或运行判定内核。

`engine/judgement_compile/include/cuexis/judgement/beat_us_compile.hpp` 提供 `compileBeatGameplay(const BeatGameplaySourceView&, const timing::TimingMap&, const GameplayCompileContext&) -> core::Result<BeatGameplayProduct>`。输入为同步借用：

```cpp
struct BeatWindow final { PhaseDeclaration phase; double startMs, endMs; };
struct BeatGradeSelection final { PhaseKind phase; std::string gradeTable; };
struct BeatScoreSelection final { PhaseKind phase; std::optional<std::string> unit; }; // null: disabled
struct BeatRequirementSource final {
    RequirementIdentity identity;
    CompiledBeatKeys beats;
    std::vector<BeatWindow> successWindows;
    std::optional<std::string> noteType;
    std::vector<BeatGradeSelection> gradeTables;
    std::vector<BeatScoreSelection> scoring;
};
struct BeatGradeInterval final { std::string grade; double startMs, endMs; };
struct BeatGradeTable final {
    std::string id;
    double exactStartMs, exactEndMs;
    std::vector<BeatGradeInterval> intervals;
};
struct BeatNoteType final { std::string id; std::vector<BeatGradeSelection> gradeTables; };
struct BeatMeasureDefinition final {
    std::optional<std::string> id;
    MeasureSpecDeclaration declaration;
};
struct BeatGameplaySourceView final {
    timing::TimingSourceView timing;
    std::span<const GameplaySource> sources;
    std::span<const PatternDeclaration> patternDefinitions;
    std::span<const BeatMeasureDefinition> measureDefinitions;
    std::span<const BeatRequirementSource> requirements;
    std::span<const BeatGradeTable> gradeTables;
    std::span<const BeatNoteType> noteTypes;
    const ScoreMode& scoreMode;
    const LatePolicyParameters& latePolicy;
    const DeclaredCapabilitySet& declaredCapabilities;
    const FeatureClosure& declaredFeatures;
    const ClosureContributions& closureContributions;
    std::string_view executionProfile, normalizationProfileToken, coordinatorPolicyToken;
};
struct GameplayCompileContext final {
    const CompileCapabilityContext& capabilities;
    const ContentProfileLimits& contentLimits;
    const PatternCompileBudget& patternBudget;
    const PrepareGameplayLimits& prepareLimits;
};
```

`sources` 借合并前的已有 v3 graph 声明和已展开实例；每个实例的完整 `RequirementIdentity` 唯一匹配一个 `requirements` 项，拍已按 invocation 平移；旧 `RequirementRecord::timing` 必须为空，禁止双定义。版本/profile 由此入口固定，GPH0 镜像由模式派生。先 `map.matchesSource(view.timing)`，失败用 `judgement.execution.relation_invalid`、`reason=timing_source_mismatch`、`fieldPath=/timing`，不生成 rows；每个 `BeatWindow.phase` 保留源 `PhaseDeclaration` 的 `kind` 与 `declarationOrdinal`；先在对应 requirement 的 `phases` 中按两字段全等校验，失败报 `judgement.execution.relation_invalid / invalid_relation`，`fieldPath` 指向原 `requirements[i].timing.successWindows[j].phase` 的 JSON Pointer，不产出 rows。通过才按 `phase.kind` 找该阶段 target。`phaseTargets` 继续使用 `PhaseKind`；不增加运行或 wire 字段。随后只查 `boundaries().anchors` 的已存锚，不走插值 fallback。新入口无身份输入、不调用旧图出版物/Hasher。产物用[成品拥有关系](prepared-artifact.md#拥有关系与入口)的 `BeatGameplayProduct`，只含 owning 未终结 rows、beat keys、运行数组、late 及 score；其分母由 T-10 纯函数导出。T-13 的[身份终结](prepared-artifact.md#身份终结)才求 digest、绑定内容和 prepared 身份、调用最终 `prepareCompiledGameplay`；T-12 独立目标不链接后继终结器，验收直接核对未终结结果。

两个定义 span 由 [Chart/CXT 源适配器](prepared-artifact.md#源输入与-lowering) 按既有 stable merge 交接，包含未被实例引用的定义体。编译侧沿 `gameplay_author.cpp` 的双向校验：每个 `patternDefinition` / `measureDefinition` 声明与同种、同名定义体恰好一对一；具名定义跨两种共用名称唯一约束，缺失、孤立或重复均按原定义关系错误拒绝。空 `patternId`、缺席 Measure `id` 不参与具名匹配，既有 inline 声明路径不变。`BeatMeasureDefinition` 属本编译头，复用现有 `MeasureSpecDeclaration` 类型，无 Packed 头依赖。编译以这些输入和 merged declarations 生成完整 canonical GPD0，保留未用定义；v3 Measure 声明行 `gradeTokens` 仍为空，实例选表独立处理。

## Playback 时钟与实时准入

Player 负责适配器观测和设备 profile 的取得；Playback 负责查询时刻的 extrapolation、校正、offset、输入和显示分支。基线 [Player correction](../../../../../engine/player_support/src/audio_device_profile.cpp) 已做 `max(0,raw−correction)`，基线 [`RealtimeClock`](../../../../../engine/playback/src/realtime_clock.cpp) 使用已有 presented 锚点并减 offset；本段只给后继路径，不改变两个旧入口。`engine/playback/src/compiled_clock_mapping.hpp/.cpp` 私有、仅后继构建；在 Playback prepare/启用实时事务中把同一 Prepared 内容的 `CompiledChartTiming::chartOffsetUs` 直接复制到 `PreparedClockMapping::chartOffsetUs`，禁止从源或 TIME 的 `offsetMs` 重新换算，并绑定 prepared/timebase/calibration 与 `RunKey`；提交后不可原地换设备校准。公开实验操作仍借用 Playback session。

```cpp
enum class PositionBasis : std::uint8_t { raw, alreadyCorrected };
struct PreparedClockMapping final {
    RunKey run;
    std::string preparedIdentity, timebaseIdentity, calibrationIdentity;
    std::int64_t chartOffsetUs, deviceCorrectionUs, inputCompensationUs, displayLeadUs;
    PositionBasis basis;
};
[[nodiscard]] auto sampleCompiledClock(const PreparedClockMapping&, RunKey,
    const ClockObservation&, std::int64_t queryMonotonicUs) -> core::Result<ClockTicks>;
```

`sampleCompiledClock` 是会话 owner 的纯采样辅助，先由现有 `RealtimeClock` 的观测验证（domain/segment、发布序、播放状态、观测预算与误差、有效期、rate/设备变化）确认可用。query 必须在该条观测有效区间；输入采样需要较早时刻则用对应区间的缓存观测，不能拿较新的观测反向猜。对 raw，`ClockObservation::presented` 表示未应用设备修正的音源等价帧，不用 deviceMixed；以其 rate 转有理微秒，播放时按 `1:1` 加 elapsed，paused/EOF 用冻结位置，尊重有限源尾。对 alreadyCorrected，进入 helper 的 `presented` 必须是**所求时刻**重新取的已校正位置（query 必等于 sample monotonic，或 paused/EOF 冻结）；禁止对 clamp 后的移动锚点外推。两种 basis 在同校准的非钳位区等价。成功路定宽受检算术、无分配、不触设备；`core::Result` 拒绝保持会话、输入与已有观测不变。设备切换、校准身份变化、过期、错 run/domain/segment 拒绝新实时采样；恢复须重验并沿原局 owner 路线。

先算 `p(query)`，raw 用 `c=max(0,p−deviceCorrectionUs)`，alreadyCorrected 直接取 `c=p`；令 `L=c−chartOffsetUs`。输出 `horizon=R(L)`，`observationTick=R(L−inputCompensationUs)`，`presentationTick=R(L+displayLeadUs)`，沿用已有 sequence/segment/errorBound。差、和、帧转换、结果缩窄检查溢出，round 原则同 D-21。暂停可观察冻结值，但禁止借它推进。新实时准入固定 `1 us|1:1`、origin=0、profile/校准可验证，显式请求不满足就拒绝请求，不悄悄降级；普通路径与 legacy replay 保留既有映射。音轨真实从 source 0 开始，逻辑起点可以为负，不能 seek 到 offset 凑 0；无音轨开始直接用逻辑 0，设备 correction/offset 不重复扣减。新 seek 延期；获准后在 seekable source 范围求完整映射反解，不可达报错，钳位平台取最早源位置。此条不更改旧普通 seek 或 D-15 的 EOF/Stop 范围。

**生产实时准入。** [既有调度器](../../../../../engine/judgement/src/execution_scheduler.cpp)仅在真实 `verifiedExclusive > H−W` 时提交普通推进。旧 `beginRealtimeTail` 用 `D+W` 检查终点可达性；现有 `beginRealtimeGameplay` 仅接受 `testOnly` 策略，不能据此宣称已有生产校准关系。v3 开局在创建/启用调度器、开启 capture 及开始出声前，用 [Player 具名发行行](render-player-writer.md#命令与启动)绑定非 `testOnly` 的已测 `SchedulingProfile`、作者声明的已校验 LatePolicy 与实际设备校准。令 `I=inputCompensationUs`、`W=windowCloseThreshold`。对整数 I，`R(L)−R(L−I)` 在非半 tick 等于 I，半 tick 最坏为 `I+1`（I 为奇数）或 I（I 为偶数）。因此要求 `W>I+(I%2!=0?1:0)`，以受检算术比较；这是 v3 任意合法采样相位的必要开局约束，不修改作者的 W、不调整 I、不把窗口或人工 watermark 延长。超范围、不符合已测有效期/误差/容量或该不等式失败报 `host.rpa.profile_incomplete`，`fieldPath=/clock/inputCompensationUs` 用于不等式失败，Playback 保留旧局/普通路径，不隐式降级。同一校准身份的 Resume 复验；换设备不能原地改映射。合法组合仍须等实际确认，算术可达性不能替代交付证明。无音轨的 chart clock 如走此调度也按自己的实际 I/W 行核验。

**EOF 尾段。** 最后真实 EOF 冻结 `p`；owner 同局用 `RealtimeClock::startTail/sampleTail` 的 scope、建锚、暂停和恢复流程，尾锚为已完成设备校正及 offset 的有理逻辑位置 `L0`。从该锚按 `1:1` 加活动单调时间，`L` 上限取 `max(L0,整数 H_end)`，两分支再扣 I/加 display lead，禁止重扣 correction、offset 或伪造 `ClockObservation`。`D` 为实际 prepared timer/deadline、必需 Fold signal 与允许最后 dispatch 的最大界；尾策略不得排除任何 prepared 义务，不扩张输入合法窗口。上限 `H_end=max(R(L0),checked(D+W))`，在开局能确定尾界及有限 source 上界时先验证可表示性；实际 EOF 用真实锚再次检查。上限处按真实 L 采样，必须 `R(L−I)>H_end−W`（开局不等式的守卫核对）；未确认实际 `verifiedExclusive > H_end−W` 时不得 seal 终点。按普通推进成功实际封过 D、处置应处理输入并完成阶段时才发布本局结束。暂停冻结累计，Resume 续同锚；越界、失效或执行故障沿真实故障路径保留前缀。EOF 与判定完成须各有真实事实。

**同局 Resume 的上下文。** v3 保留 `sessionScope/runEpoch`，严格增加 `clockSegment`；现有 `operationId/controlSequence`、`pauseEpoch`、RPA `controlOrdinal` 与确认后的 reconciliation trace 仍独立递进。T-14 在 `ExecutionHost::resume`（`app/player/src/execution_host_frame.cpp`）仅 legacy 递增 `runEpoch` 并检查其溢出；两支均检查 segment/控制序溢出。在 `host_protocol_helpers.hpp::validNextContext` 与 `host_protocol.cpp` 两个 preflight/accept 调用点，Resume 按已绑定模式检查 epoch，Seek 保持 legacy 原守卫。`capture_route.cpp::CaptureRouteOwner::resumeRoute`、`playback_session_realtime.cpp::resumeRealtimeGameplay` 和 `execution_scheduler.cpp::ExecutionScheduler::resume` 用相同判定：v3 `next.runEpoch == current.runEpoch`、legacy 严格大于；两者都要求同 scope 及新 segment 严格大于。HostCoordinator/CaptureRouteOwner 用构造时从会话 owner 已校验 prepared/known run 得到的仓内 `ResumeEpochPolicy { legacyNewRun, keepRun }`（旧构造默认 legacy），Playback/调度器由已绑定 session 取模式；命令、时钟样本或客户端 flag 不可选分支。

`applyResume`/`resumeExecutionStep` 仍校验原 `StepKey`、当前和目标 context、route receipt、cut、pauseEpoch、reconciliation ordinal、finalTraceCut/digest 与完整 trace；通过才交接新 clock/capture 路由和旧 Hold/Fold 状态。新 segment 重置的仅是原 delivery/local route 计数和水位，继续从真实新段 capture 证明取得水位；不重置同局 ingress 计数、`advanceId/commandOrdinal` 或已交付结果。现有 `deliver` 全 context 守卫拒绝旧 segment（即同 run 的 deliverySequence=0 也不可混）。旧操作的相同请求可由原 ledger 回读其终态，不再执行、不把旧结果应用到新 segment；错 proof 为原 `context_stale`/`message_conflict`，缺证明保持暂停，已接受但不可恢复的原 `resume_aborted` 仍生效。Retry/Reload 新局照旧换 `runEpoch`；legacy Resume、Seek、replay 仍用旧世代及记录语义。

## 稳定诊断与迁移

D-04 先接受 Spec/ADR 再改 schema/实现。两个新增 Stop 码及容量码由 [`cuexis.gameplay-diagnostics.v2.codes.json`](../../../../../schemas/cuexis.gameplay-diagnostics.v2.codes.json) 注册；对旧码保持类别和私有源码映射。源码表对接 `engine/judgement/src/{diagnostic,diagnostic_policy,source_codes}.*` 实际同名文件。新的错误返回 `core::Error` 带稳定 code、`category`、`severity=error`、`faulted=false` 和 `fieldPath`；新作者/成品 `fieldPath` 用源 JSON Pointer（`/timing/stops/i/durationMs` 等），二进制用 `/TIM1/stops/i` 或 `/JPR1/rows/i/...`。碰撞 `fieldPath` 指后 Stop，`relatedFieldPath` 指前 Stop；数组下标原始来源或 wire 行号，从零计。首错阶段依 header/schema、引用/关系、算术、合法性/边界、发布，Stop 组内按上节顺序。`bad_alloc` 在非实时准备复用 budget 类诊断，禁止回退生产状态。新注册行 `categoryBasis=documented_mapping_row, severity=error, faulted=session_unaffected, stableRejectCode=null, rejectionEntries=[]`；Stop 的 `ownerBatch` 为 `RR-FMT`、槽为 `RR-ADV`，`source` 引提升后的 Spec 小节，`publicConsumptionSurface` 为现有 `DiagnosticCode` 与上下文。

| 条件 | code / category |
| --- | --- |
| 越界累计/缩窄、绝对端点加法、帧与 us 转换 | `judgement.timebase.tick_overflow` / `budget_exceeded`；复用公开码，旧私有 `judgement.s7a2.timebase.tick_overflow` 保持原映射。 |
| 新 Stop `E<=S` | **新增** `judgement.timebase.stop_quantized_interval_invalid` / `invalid_relation`。 |
| 新 Stop 不同拍前 E 等于后 S | **新增** `judgement.timebase.stop_endpoint_collision` / `invalid_relation`。 |
| 非法字段/引用；有效 grade 域中 gap、overlap、空窗；缺 scoring 覆盖；错误 clock basis | `judgement.execution.relation_invalid` / `invalid_relation`；字段路径区分。 |
| 不支持 Gameplay version/profile | `format.gameplay_version_unsupported` / `ambiguous_migration`；Packed 物理 revision 拒绝保留 `packed.header.unsupported_revision`，在 Playback 映射为对应格式拒绝时保留 `sourceCode`。 |
| 准备身份、摘要或 CXC 闭包不符 | `playback.gameplay.invalid` / `identity_closure_incomplete`，保留原 metadata 上下文。 |
| Packed/Graph 限额；运存可表示范围/已接受上限 | 保留 Packed `packed.budget.*` 子码，Gameplay 边界 `capability.budget_insufficient` / `budget_exceeded`。 |
| 新推进 slots 小于 P | **新增** `judgement.advance.output_capacity_insufficient` / `budget_exceeded`；`fieldPath=/slots`，`required`/`provided` 元素数。 |
| 错 run、错误线程/重入、未准备或已经故障 | `session.mutation_rejected` / `invalid_relation`；分别 `fieldPath=/run,/session/owner,/session/state`。 |
| horizon 逆转 | `judgement.timebase.time_reversal` / `invalid_relation`。 |
| 预检后执行故障 | 当次只返回无分配的 `FaultCode`；Fold 的详细观察沿 `ruleset.transaction_failed` / `invalid_relation` + `session_faulted`；其它原内核故障诊断不改。 |

迁移顺序：先冻结 v1–v4/v5 candidate、v2 Graph1/rev2–4 的接受与拒绝 fixture、wire/hash/replay golden；再注册上表新增码并生成 `python -B tools/verification/gen_docs_codes.py`，跑诊断门禁；接 timing/作者 schema 与新 Reader 并保持旧 readiness。Stage 8 提升时在 Chart v5 计划 §3.8 与 Stage 8 发行矩阵明确 v3 是独立候选/正式组合，更新 `CHART_V5_FORMAT`、Gameplay v3 Spec/ABI、`GAMEPLAY_GRAPH_*_FORMAT`、`GAMEPLAY_CAPSULE_*_FORMAT`、`CHART_ENTRY_V1_FORMAT`、`PACKED_CHART_FORMAT`、`TIMING_MODEL`、新版 schema/诊断/兼容说明，记录 Stage 8 原 v2 默认不变。无 playback 路径自动转换旧输入；离线 Writer 只在字段、窗口、身份与能力可证明无损时生成新目标，失败保留原字节及当前会话。

## 验收向量与任务前置

以下是实施验收，当前未跑。tick 单位 us；`b=n/d` 为精确有理。使用同 fixture 验证源编译、Graph2/Packed5 往返、编译 Reader 闭包与准备/推进输出。

| 向量 | 输入与确定结果 |
| --- | --- |
| TT-01 | BPM 120，Stop `b=-1,d=10ms` 与 `b=0,d=20ms`。前 `[−510000,−500000)`、后 `[0,20000)`；对每组 S−1/S/S+1/E−1/E/E+1 反查。S 精确 beat 且 progress=0；E 精确同 beat、false、0；例如后一 S−1 beat `−0.000002`，E+1 `0.000002`（`1e−12` 容差）。`beatToTime(1)=520000`。 |
| TT-02 | BPM 120，Stop `b=1/2000000,d=0.0005ms` 得 S=0、E=1，接受（分别量化优于 `S+R(d)`）；同拍改 `d=0.0001ms` 得 `0,0`，首 Stop code。两 Stop `b=0,d=0.0011ms`、`b=1/2500000,d=0.003ms`，前 E=1、后 S=1，报碰撞且附 `/timing/stops/0` 与 `/timing/stops/1`。 |
| TT-03 | `R(0.5,1.5,2.5,−0.5,−1.5,−2.5)` 逐项为 `0,2,2,0,−2,−2`；`R(−2^63)` 合法，`R(2^63)` 与 `+inf` 拒绝；绝对 `INT64_MAX+1` 拒绝，不能饱和。给目标 tick 100/101，同一相对端点 −1.5µs、+2.5µs，结果 `[98,102)`、`[99,103)`。量化同 tick 的不同拍 tempo 按拍序最后一个生效，两条 phase target 同 tick 仍各有身份。 |
| TT-04 | 给 v2 `TimebaseProfile` 两个相邻 `[−1,0)`、`[0,1)` 且 owner 已允许的零拍连接；用现有 legacy fixture 的合法 scale 和 duration，映射与期望旧 digest 不变；单个 `[−1,1)` 继续依旧 profile 拒绝，不走新 Stop 码。 |
| TT-05 | BPM 120→240，`startBeat=0/1,durationBeats=4/1,startSlope=endSlope=1`，无 Stop/offset。在第 2 拍分别设离散 Step 和 judgement target；积分 `2000000*ln(1.5)≈810930.216 µs`，编译存同一 `810930`。`810929` 未跨边界，`810930` 两者同时跨界；后者即使 `timeToBeat(...).beat<2` 也不得推迟。核对目标、表现和判定边界来源只一次积分量化。表现 wire 闭合后，由同一源样例写出后继成品，解码保留同一 Step 程序，以同一求值路径复验两侧 tick 的表现与判定。 |
| TT-06 | BPM 120，在第 1 拍设同 BPM 的零时长 tempo 阶跃，拍 `3/2`、`3` 各有独立玩法锚；第 2 拍 Stop 20 ms。第 1 拍 Tempo/anchor 同为 `500000`，第 `3/2` 拍为 `750000`，第 2 拍 `[1000000,1020000)`，第 3 拍为 `1520000`。`TIME.offsetMs=20.001` 时，源/Packed5/Graph2 的 Prepared 都持 `chartOffsetUs=20001`，source 0 的无校正音轨逻辑起点 `−20001`，离散锚不变。逐个变异：只把第 1 拍 tempo 项改至 `500001`；把前锚改至 `1000001` 或后锚改至 `1019999`（对应玩法 tick 一起改，使其仍与 anchor 一致）；`TIM1.offsetUs` 改至 `20000`；GPH0 presence 改 `true` 并加合法 rational。每项在重新编码并重算适用 CRC/digest 后仍拒绝，原 active 不变。 |
| TT-07 | 分别用 BPM 120、60、121（无 tempo 变动、无 Stop）构造仅含 0 拍的 map，离散边界相同。把 120 map 分别配 60、121 view 调 `compileBeatGameplay`，在生成任何 row 前拒绝 `timing_source_mismatch`，路径 `/timing`；同源组合通过。`−0/+0` 只在合法浮点字段归一为同值。 |
| GP-01 | 用下文作者 fixture：目标 `1000000`、成功窗 `[950000,1050000)`，各次只交一次窗内 press。相对误差 `−50000,−10001,−10000,0,8000,9999,10000,14999,15000,49999` 对应 `Good/Early,Good/Early,Great/Exact,Great/Exact,Great/Exact,Great/Exact,Good/Exact,Good/Exact,Good/Late,Good/Late`，误差实值不变。Good：`(J,C)=(1,1)`、55 分；Great：`(2,1)`、100 分。空输入或只在 `1050000` press：`advance(1050002)` 无事件，`advance(1050003)` 恰一 `Miss@1050000`，grade/error/direction 皆空，终局 0；重复不重发。缺 Exact、量化后塌缩、并列键/错引用/档间隙拒绝。另用五档 `G1…G5` 区间 `[-50,-30),[-30,-10),[-10,10),[10,30),[30,50)` ms，按每档左端和端前一 tick 验证上一档/本档边界；模式给 `G1…G5` 分别 `1..5`、均 increment，预期 `28,46,64,82,100`。类型默认 main、指定实例覆盖 five，分别断言 `−30000` 得 Good/G2；旧未声明表样本无 grade/方向且旧分不变。 |
| GP-02 | Hold 用 head `[850,901)`、body `[900,1000)`、tail `[1000,1010)` 和 `[1015,1020)`、deadline `1040`；body/tail target `1000`，close/watermark/hop=`3/8/4`。表覆盖 `[-154,0):break,[0,20):release`，Exact `[-10,15)`，head 不选表；`−154` 来自最早 dispatch 850 减 hop 4，`20` 来自最后窗关闭。`mj=90,mc=10`，`reward(k)=k`、耗尽 reset。head/Body null Hit 各 +2、Tail Hit/release +6，均 increment；Body Miss/break、Tail Miss/break 或 release 各 +1 并 reset；其余局部键不合格。封前提交 press@900，各推进至 1103：① release@950 → Body Miss/break/Early/−50、尾级联无档无误差，`J=3,C=1,S=29`；② release@1011 → Body timer Hit 无档无误差，Tail Miss/release/Exact/+11，`J=5,C=3,S=50`；③ 无 release 或 release@1020 → Timer Miss@1020 三项空，`J=4,C=3,S=41`；④ release@1019 → Tail Hit/release/Late/+19，`J=10,C=6,S=100`。`Jmax=10,Cmax=6` 不变。② 的旧查询 `GameplayResult::score()` 为 `{50,0,2,2,1}`，终局实验结果原比例 `5/10,3/6`。另送 press@850 后 `advance(853)`，再送 observation 849 的 release，dispatch 851 → Body Miss/break/Early/−151。短 Hold 对照：Head target `850`，body 及其唯一成功窗均为 `[850,851)`、Body target `851`，Tail target `1000` 及原尾窗保留；送 press@850 后 `advance(854)`，再送 observation 849 的 release，dispatch 852 → Tail Miss/break/Early/−151。把表下界改为 `−150`、删必要键或篡改分母，源与 Reader 均拒；仅缩去 `+20` 及以后的非必要尾域不拒。旧 Hold/资源/CXT 对照保留结果与 hash。 |
| GP-03 | P=3（各事件类型字节数单独受检）；P−1 sentinel 槽拒绝不动槽/ledger/frontier，换 P 后返回原事件；双 session 同 epoch 键互拒；卸载后保留值可读，同局恢复零重发、Retry 新 epoch。空局 slots `{}`。分别于第一个和第二个 tick seal 前故障，输出零条及第一条；第二 tick seal 后 Fold 故障输出两条且 `faulted`、成绩空；注入预检分配失败不推进。 |
| GP-04 | 同一模式 `total=1000000,mj=900000,mc=100000`，`reward(k)=k`，规则 Tap Hit/`A`、Hit/`B`、Hit/`solo` 权重 2/5/3 且增连，各选仅有对应 grade 的独立表（有效误差与 Exact 均 `[0,1)`），另有 0/keep 的 null Miss。200 和 1000 个 A 单例分别得基数 `(400,20100)`、`(2000,500500)`；全中均 `1000000`。A/B 显式共一单元，另一个 solo：P=3，`Jmax=8,Cmax=3`。先 A 合格、再 B 合格、再 solo，B 只交事件，`J=5,C=3,S=662500`；B 先合格为 `1000000`；A 无输入 Miss 不提前 reset，B 后续仍可获单元积分。仅物理不可达的 B 不改分母。额外造全不合格单元、重复 phase、ObserveOnly 入组、`onExhausted=increment` 或本谱未用的 false 行非 0/keep，各自拒绝。`reward` 段 `{1,1,1},{2,2,0}` 的四单例 `Cmax=7`；段从 2 递减到 0 拒绝。 |
| GP-05 | `(mj,mc;J/Jmax,C/Cmax)=(1,1;1/2,1/2)` 输出 1；`(1,0;1/2,0/0)` 输出 0；`(3,0;1/2,0/0)` 输出 2。空谱 `0,0` 输出 0；任一启用份额且分母 0 拒绝。`m=UINT32_MAX,d=UINT64_MAX`，`mj=m−1,mc=1,J=d−1,C=d−2,Jmax=Cmax=d` 输出 m；两个单元上界分别 `UINT64_MAX`、1 或奖励 `UINT64_MAX+1` 的受检累加拒绝。MSVC 与 Clang/GCC 跑相同向量；近半边界比较用整数 oracle。 |
| CLK-01 | raw 0/2999/3000/3001/4000，correction=3000、offset=20000，L 为 −20000/−20000/−20000/−19999/−19000；raw anchor=2000 且 elapsed=1500 时 L=−19500，拒绝用 corrected anchor 外推。L=−19500、input=200、display=300 得 `observation=-19700,horizon=-19500,presentation=-19200`。offset=−20000 时 source 0 得 +20000。校正后值 500 在同查询时刻结果相同；不同 query/播放 basis 拒绝。paused/EOF 固定，不允许暂停推进；设备变化/过期拒绝。 |
| CLK-02 | Stop `C=100,acceptedThrough=7,close=15`。先接收序号 6、有效 tick 105 的合法输入，再接收序号 7、有效 tick 90 的输入，均尚未调度。Stop 排空并交付后者的命中；序号 6 的原输入及编号保留在内存 archive/replay，因 tick 大于 C 不结算。锁后到达的 tick 95 输入无编号、无归档、不结算。另一 phase 在 100 到期则 Miss@100，110 到期的及未到期尾不补。普通 `advance(100)`（含仅标记 `controlDrain=true`）只封到 85。成功 Stop 封到 100；完整内存 replay 得同一两个 fact、frontier、Fold 和终态，宿主零重发；测试空前缀与实际序号 0 不相混。篡改 `run`、上界 `7→8`、cutoff `100→101`、存储 frontier 或追加第二 Stop 均拒绝；终止前的合法 cut 保持可用，`seek/checkpoint` 以 `horizon=100` 或显式消费终止行的 cut 拒绝，终态 restore 拒绝。编码该 archive 或求 `canonicalIdentity` 返回 `codec.version_unsupported`，旧对照不变。槽不足或错 run 前缀不变；锁后 Fold 故障交付已封槽且零成绩，尝试获取该局 archive/snapshot 拒绝。 |
| CLK-03 | raw EOF=523000、correction=3000、offset=20000 → `L0=500000`；D=700000、W=15、I=5。活动 elapsed=199999 得 H=699999，不完；elapsed=200015 得 H=700015、observation=700010。`verifiedExclusive=700000` 不能封 D，实际更新至 `700001` 后封到 D 并完成。I=15 在开局即拒绝；W=15、I=14 可准入；W=14、I=13 在 `L=14.5` 的近半对照检查，改 `L=15.5` 得 `H=16, observation=2`、临界相等故此组合开局拒绝；W=15、I=13 准入。暂停不累加 elapsed，Resume 续。 |

GP-05 补测：两增连单元、`reward={1,UINT64_MAX/2,0}`，`Cmax=UINT64_MAX−1` 合法；加同奖励第三单元拒绝。独立受检加法对照 `(UINT64_MAX−1)+2` 拒绝，不依赖使用一个破坏单调性的奖励表。

CLK-02 补测：从真序号 0 开始，接受一个迟到后被拒绝的 canonical 输入，Pause/Resume 与 EOF 换段后再送两个合法输入和对应重投。核对 Snapshot/Replay 保存拒绝行真编号、同局编号不重置、重投不消耗编号；Resume trace 与 delivery 的本地序号更新不替代 ingress。篡改序号/上界、任一单元剩余量/settled 位或 sealed/Fold cursor 的恢复拒绝；无 Stop 的 v3 archive/checkpoint 走旧 codec 也拒绝，旧局仍通过。

CLK-02 分层构造：上表的 105/90 前缀由 T-10 用纯内核接收入口实际生成 0–7，验证未来 pending 也进入 Stop 上界。T-14/P4-05 的真实 capture 对照先送六条不匹配义务的合法 release（tick 10/20/30/40/50/60），再送 release@89 与匹配的 press@90；各原 tick 成闭合批次，证明 observed tick 分别晚一 tick，均不晚于 Stop 的 C=100。按真实接收顺序得到 0–7，仅 press@90 和到期 timer@100 产生目标事实。记录全部 batchId/deliverySequence/ingress，插入合法 prefix 使 delivery 上界与 7 不同；Stop 从生产 query 取得 7，不能手填。批片不完整时不编号、不成功 Stop；相同 delivery 重投不再编号。实际未来原 tick 105/观测证明 106 不能在 C=100 的实时钟下接纳；不放宽 `deliverGameplayCapture` 的未来证明拒绝或 scheduler 的 epoch-end 时序检查来迁就内核向量。锁后 tick 95、Paused Stop、空前缀和首个序号 0 的生产路径仍分别验证。

异步推进、Pause/Resume seal 和同局 Resume 的端到端输入/期望集中在 [P4-11～13](render-player-writer.md#所有权暂停与收尾)，以真实 worker 和 capture 证明验证，不用回放或同步 mock 替代。

GP-01 作者 fixture：从 `tests/fixtures/gameplay_author/inline_prototype.json` 取原资源、Pattern 与能力字段，置于[成品源输入](prepared-artifact.md#源输入与-lowering)规定的 v5/v3 外壳，使用 `objects` 和对应 full K4；删除旧 timebase/rulesetRef，清空作者 `gradeTokens`。BPM=120、无 tempo 变动/Stop、offset=0。`endBeat=21/10`、tap target `2/1`、相对窗 `[-50,50)` ms。`NoteType {id:"tap",gradeTables:[{phase:"tap",gradeTable:"main"}]}`，实例 `noteType="tap"`；`main` 的 Exact `[-10,15)` ms，档为 `Good [-50,-10),Great [-10,10),Good [10,50)` ms。模式 `id="standard",total=100,mj=90,mc=10,onExhausted="reset",comboRewards=[{from:1,rewardAtFrom:1,slope:1}]`；Tap Hit/Good、Hit/Great 分别 1 和 2、qualifies=true/increment；Tap Miss/null 0、qualifies=false/keep。late policy `queue_next_tick` 的 watermark/hop/close/open 分别 8/4/3/2 µs，测试限定。无需作者 `scoring` 字段即形成单例。

命令、写域和证据以任务卡为准；前置只在[调度表](../schedule.md)。本合同补任务级用例：

| 任务卡 | 合同验收 |
| --- | --- |
| [T-04](../tasks/T-04.md#任务级验收) | 八 TU/私有 hook 清单、默认候选阴性及 static/shared 安装闭包。 |
| [T-08](../tasks/T-08.md#任务级验收) | 逐符号转发及删除窗口；使用旧运行内核独立验证。 |
| [T-10](../tasks/T-10.md#任务级验收) | 纯 tick records 跑 GP-02…05、CLK-02、旧 restore；无 timing/新成品 target 的构建须能链接旧作者/旧成品消费者；默认与实验 headless 启测均定向构建 `cuexis_rpa_tests` 并运行既有 B3 用例。独立运行消费者只有 Core/Judgement，阴性引入作者 `timebase.hpp` 或 `timing_map.hpp` 失败。 |
| [T-11](../tasks/T-11.md#任务级验收) | 作者版本派发、比例、计分字段与诊断；源窗口的 phase 原样保留 kind 和 ordinal 交编译；结构上接受有档 Miss，拒绝非法 grade token 或重复键，必需键覆盖见 GP-02。 |
| [T-12](../tasks/T-12.md#任务级验收) | TT-01…05/07、GP-01 / 02 / 04 源编译；GP-01 从源及直接 typed 编译分别验声明 tap/1、窗口 tap/1 通过，窗口改 tap/2 报关系错、无 rows；独立消费者只连 `judgement_compile` 与 timing，验证未终结 rows，无 Chart/Reader。 |
| [T-13](../tasks/T-13.md#任务级验收) | 依[成品验收](prepared-artifact.md)，跨源/Packed/Graph 核对 TT-05/06、GP-01/02/04 与变异分母拒绝。独立成品消费者无作者头或源即可准备。 |
| [T-14](../tasks/T-14.md#任务级验收) | CLK-01…03、P4-07、P4-11～13。T-14 提供具名 Retry 生产命令及唯一 owner 消费，两个 Pause 与 Retry 各回一次终态；R/F5 键位接线归 T-15。准入拒绝不影响普通/旧映射；真实异步捕获、Pause/Resume/Stop seal 单次交付，Resume 同局换 segment，旧对照不变。 |

**实施与发行门禁。** v5 增字段、v3 发行分支、诊断、SDK 窗口仍按计划取得采用与发行裁定。生产 late/scheduling 参数须通过具名实测行与 D-12/D-15 守卫，试验值不晋升；新 seek 仍延期。实施时验证源与成品的同一会话结果、独立 Reader 闭包、旧 ABI 及旧回放兼容。

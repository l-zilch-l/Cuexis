# Stage 7-RR 架构精简方案（待审阅）

状态：candidate

更新日期：2026-11-21

<!-- cuexis-meta
id: PLAN-STAGE-07-RR-SIMPLIFICATION-PROPOSAL
status: candidate
authority: references
-->

**推荐：默认产品长期只维护一条 Player 执行路径。** 改暂停、换谱或结算时，只改一个会话控制者，用普通播放和显式判定两组用例守住差异；改某个候选格式时，主要验证它的适配器和共享边界，只有影响公共行为才扩展回归，不再让每种格式都配一套时钟和渲染流程。

按[计划 §4 D-05](plan.md#4-已确认方向与决策台账)，RR-3 仍限于 R1 构建隔离、R5 渲染收敛与 R6 文档减重，R2/R3/R4 继续延期。已定设计见[架构设计附页](architecture-design.md)；旧读取和迁移仍按[兼容退出政策](../../../adr/0041-legacy-format-exit-policy.md)。

## 先审哪六项

P1–P6 是本页的审阅编号，只审各项未定部分。接受一项不表示接受其他项，也不等于变更现行 API、批准实施或排期。

已接受的时间、比例、SDK 与兼容设计分别以 [D-21](architecture-design.md#d-21-谱面表现与判定共用拍位换算)、[D-23](architecture-design.md#d-23-谱面画面比例与宿主尺寸)、[D-24](architecture-design.md#d-24-sdk-判定接入与实验扩展)、[D-25](architecture-design.md#d-25-兼容入口与格式合同收敛) 为准。下文[整体推荐方案](#整体推荐方案待审)补齐余项；全部待审，不表示 owner 接受或授权实施、排期。

同日续议确认 [D-25](architecture-design.md#d-25-兼容入口与格式合同收敛)：旧渲染入口逐步替代并最终删除；Packed CRC 与 CXC playback encoding 采用现行实现的明确规则，两处格式合同已更正。兼容迁移的推荐见 P1、P2；正式发行的 profile、revision 与版本仍待裁定。

| 待审 | 要决定什么 | 推荐与主要代价 |
| --- | --- | --- |
| P1 格式与 SDK | [数据流、构建依赖、作者表示与安装入口](#p1-数据与-sdk)；已接受的职责边界继续有效，具体推荐待审。 | 拆开作者编译与成品装载；迁移已安装成员须经过版本裁定，发行组合仍由 Stage 8 决定。 |
| P2 渲染 | [矩形坐标、整数取整与旧入口迁移](#p2-渲染与兼容迁移)；D-23／D-25 已接受的呈现归属与退出原则继续有效。 | SDK 统一计算内容矩形；兼容期保留替代探针和适用的消费者门禁。 |
| P3 时间 | [Stop 量化碰撞、起播钳位与实时准入](#p3-时间与会话时钟)。Stop 半开区间与结束反查已接受，量化退化和碰撞处理仍为待审推荐。 | 保留旧成品与旧回放规则；新实时须验证完整映射，不能只比线性区间。 |
| P4 Player 与旧宿主 | [显式请求、重试装载、无音轨与控制接管](#p4-player-与旧控制权)。R=Retry 已定；Reload 具体键位推荐 F5。 | 复用同一 prepare 路径和唯一控制者；新交互与接管安排待审。 |
| P5 合同与诊断 | [目标版本、损失报告内容与失败诊断](#p5-写回与诊断)。保存前报告已知损失、覆盖策略归编辑器的原则已认可。 | 维持已更正的 Packed CRC 与 CXC 编码；只审具体表示、实现和必要的合同修订。 |
| P6 阶段交付 | [迁移顺序、分段验收与停止条件](#p6-迁移与验收)。D-05 范围已定，RR-4 独立跟踪。 | 分开核验 RR-3 的 R1/R5/R6 与 RR-4；RR-4 以前置 RR-2 和 R1 为门槛。 |

审阅可以只回复编号和「接受／修改／延期」，也可以只改一行，例如「P4：无音轨的普通播放改为允许内容指定终点」。没有回复的项继续待审；将来若牵涉已接受的合同或 D-05，须单独裁定。

### 为什么选择按消费者收敛

比较的是**长期如何减轻同一个维护者的负担**，不重新表决是否保留单一 Player。有些路线超出 D-05，只用于说明取舍。

| 路线 | 能省什么 | 会留下什么成本 |
| --- | --- | --- |
| 只收紧文档和运行时默认值 | 不迁调用方，最易撤回 | 默认构建仍可编译、链接或安装候选；下一次改公共头或工具链仍需兼顾它们。 |
| 只隔离构建，不整理 SDK 和调用关系 | 能先减少默认构建依赖，边界容易用预设检查 | 宏两侧的公开类定义和旧调用可能仍不同；外部消费者仍需知道两套行为。可作中间步骤。 |
| 把所有候选提升为正式能力 | 一份安装包即可试用所有路径，不再选构建 | 把未成熟格式、时钟和证据变成兼容承诺，维护面反而最大；还会提前替 Stage 8 作发行决定。 |
| 归档候选实现，只保留格式资料与历史输入 | 默认产品积累最少代码 | 仍在用的工具和可复现实验失去入口；以后恢复某种格式须重新验证甚至重写。只适合已经查明没有消费者的对象。 |
| **按实际消费者隔离，逐项替代再退役（推荐）** | 默认只维护一条产品路径；有用的候选与旧谱兼容各自有明确入口和退出条件 | 前期要查调用方、比对故障行为、维护少量隔离测试；长期每条重叠路径要么有具名消费者，要么可以删除。 |

## 整体推荐方案（待审）

以下是一套连贯的技术推荐，P1–P6 均待审；已接受边界仍以对应 D 条目为准。拟名不占用版本号，本节不构成 owner 共识、合同修订、实施授权或排期。

源谱编译、成品装载与会话推进各有边界，见 [P1](#p1-数据与-sdk)；分段授权与证据见 [P6](#p6-迁移与验收)。

推荐先保留旧样本、核清安装消费者，再做当前范围的隔离与渲染替代；后续获准实施新时间和玩法后，逐项由单会话接管旧职责，最后在适用兼容窗口退出旧入口。下表明确各段的技术选择和验收。

### P1 数据与 SDK

**类型归属与构建（待审推荐）。** 按 D-21，由 Chart 解析权威节奏，`timing` 持有新作者拍位和不可变映射；源谱直开只编译一次，同次生成表现与玩法的离散 tick。比例用宽、高正整数，玩法用精确拍位和带符号毫秒窗口。`aspectRatio`、`gradeTables`、`gradeTable` 暂作拟名；等级表具名、类型默认、单音符可覆盖，未知引用拒绝，旧无表谱不补等级。计分复用整数 `ScoreRule` 和连击规则。

| 归属 | 迁移与兼容 |
| --- | --- |
| `timing`：作者 `RationalBeat`、节奏映射与换算 | 沿用 [Chart 拍位语义](../../../../engine/chart/src/rational_beat.cpp)：正分母、归约、现有构造与全序。先迁 Chart 换算及新玩法作者拍位；Chart 对外类型名和已装符号按兼容窗口迁移。旧 Reader 的字段范围、诊断和接受集各自保留。 |
| 旧候选入口：Judgement `RationalBeat`、`RationalDuration` 与 `TimebaseProfile` 解码和编译声明 | [旧工厂](../../../../engine/judgement/src/timebase.cpp)允许可翻号的负分母，但拒绝 `INT64_MIN` 分子。先用旧入口规则校验和报告原诊断，只有需要且可精确表示时才转换；不统一两个旧接受集。旧 `RationalDuration` 不进入运行期通用时间类型。 |
| 判定运行侧：`Tick`、`TickSpan`、phase/outcome、`PreparedGameplay` | 运行头只见离散边界、已验证配置和不可变准备数据；旧 tick 的单位和语义不变。prepared 声明、访问器和内部构造留在运行 target；作者请求、旧 wire 图和装配编译入口留在上游。按头与源码职责分组，无需新增 `model` 库。 |

[当前 `TimebaseProfile`](../../../../engine/judgement/include/cuexis/judgement/timebase_profile.hpp)同时携带原点拍位、tempo/stop、单位与 profile 身份。编译或装载校验完整声明，节奏数据留在时间库或旧候选入口；准备边界输出已验证的 profile 身份、单位和必要的 tick 比例（旧比例可用已验证的整数分子分母表示）。旧入口仍做原有完整 profile 比对并用原身份算法，然后才提取运行记录。内核核对成品与会话的身份和单位，运行侧保留 `LatePolicyParameters` 准入；Playback 拥有校准和时间换算，内核接收校准标识与已换算输入，并核对运行或恢复所需身份。发布后的配置和 prepared 数据不借作者或解码临时对象。

| 构建边 | 拟保留或调整的职责 |
| --- | --- |
| `timing → core`；判定运行 target `→ core` | 两个独立 target，`core` 不承载时间库。内核的公开及内部传递 include 均不引入 timing、Chart 或作者编译头；`ruleset` 直接用窄运行类型，移除经 `gameplay_prepare.hpp` 引入编译声明的路径。 |
| `chart → timing`；`judgement_compile`（拟名）`→ timing` 与判定运行 target | Chart 原有其他依赖照旧。时间库边界传有符号 64 位整数微秒或时间库自己的时间值，`timing` 不引用 `judgement::Tick`。作者编译器完成量化与范围检查后构造运行 `Tick` 和 prepared。编译 target 承接从现有 Judgement 移出的作者装配和旧图重装配；由上游传入已解析的 Chart 节奏上下文，自身不链接 Chart、Packed、Playback 或工具。`animation → chart` 保留。 |
| `gameplay_packed → chart`、判定运行 target；旧 codec 另 `→ judgement_compile` | 保留 Packed 的 `core`、`filesystem` 和 JSON 支持依赖。新 Reader 调窄判定准备，旧 codec 暂从同一 Packed target 调编译侧旧装配入口；同库过渡期保留这条实际链接边。实验 Playback 依赖 Packed 和判定运行 target，源谱直开接线可直接调用编译 target；成品接线只调 Reader，恢复 codec 使用运行窄头。 |
| `gameplay_author → judgement_compile`、`gameplay_packed`、Chart/JSON/Core；具名 CLI `→ gameplay_author` 和所需现有工具依赖 | 现有 `gameplay_author` 负责源解析和成品组装，仍可经 Packed 编码。`assembler_cli.cpp` 移入具名作者工具的接线层；`cuexis_gameplay_assemble` 和 `cuexis_chart_candidate --gameplay` 共用这份非安装的接线实现，保留其 Playback、CXC、asset-publish 依赖。工具复用 Playback 的 entry 实现，不重复编入 `gameplay_entry.cpp`；无 Playback 或 Packed 回指工具的边。 |

**构建与安装组合（待审推荐）。** 当前 [engine CMake](../../../../engine/CMakeLists.txt)无条件纳入 Judgement/Packed，[根 CMake](../../../../CMakeLists.txt)将 Judgement 放入 static SDK 实现归档清单、只在实验组合追加 Packed。[Playback CMake](../../../../engine/playback/CMakeLists.txt)虽按开关加入候选源码、链接边和安装头，仍有多份候选翻译单元列在基础源码中。[作者工具](../../../../tools/gameplay/gameplay_assembler/CMakeLists.txt)在开发工具组合就定义 `gameplay_author`，其 CLI 与额外链接边受候选开关控制。以下为拟议组合，须按实际生成的目标、翻译单元和安装导出验收。

| 组合 | 拟构建 | 拟安装或消费 |
| --- | --- | --- |
| R1 过渡 | 先隔离普通 Player/Playback 的候选源码与链接，同时继续构建现有 static 安装所需的 Judgement 归档。 | 保留现有 `Cuexis::InternalJudgement` 导出归档和安装面，不把构建隔离当作安装退役许可。 |
| 收口后、能力正式化前的默认 Player / SDK | Chart/Animation 和原普通 Playback 的依赖。候选翻译单元及混合翻译单元中的候选部分独立编译，只在显式组合中启用判定运行 target、玩法编译器、Gameplay Packed 和候选作者适配。通用开发工具可单独启用。`timing` 获实施授权后随 Chart 构建。 | 普通 `Cuexis::Playback` 和现有通用依赖。static 导出所需实现归档（`timing` 落地后随装），shared 经公共库使用私有实现。保留通用显式 entry 工厂，候选请求仍拒绝。 |
| 能力正式化前的实验 Player / SDK | 候选开关开启后加入判定运行、编译侧、Packed 和候选 Playback，暂含旧 codec 兼容边。可关闭开发工具。用同一消费者验证两种构建的普通播放行为。 | 用独立前缀和 `Cuexis_ALLOW_EXPERIMENTAL` 显式选择。第三方链接 `Cuexis::Playback` 并包含窄实验头。static 随装链接所需的私有归档，shared 经现有共享库导出入口。这一阶段编译器、Packed 与判定内核仍是私有实现。 |
| 具名作者工具 | 同时显式启用候选和开发工具，构建 `cuexis_gameplay_assemble`、`cuexis_chart_candidate --gameplay` 及上述依赖，可关闭 Player。普通工具组合继续提供 Chart 校验、迁移及 CXC 工具。 | 工具和接线层按源码工作树消费。 |
| 判定运行内核独立消费者 | 用仓内独立的最小 CMake 工程，只配置 Core 与判定运行子目录。测试宿主用运行头和固定 tick 准备数据建局、推进、复查结果。该入口独立于会纳入其它模块的根工程开关。 | 从构建树消费。核对传递 include、`ALLOWED` 和实际链接，并用引入作者头或时间库的阴性用例验证门禁。 |
| D-19 验收通过并获准正式化后 | 同一默认 Player 按同一启动参数和能力规则支持普通播放与显式判定。经批准的判定、格式和必要编译或装载路径进入默认组合。实验组合只在共享路径上增加未成熟格式及其适配。 | 默认 SDK 安装成熟判定的窄公开头和消费组件，第三方可独立接入判定或经 Playback 使用。组件命名和版本目标随正式化裁定。 |

R1 先分离编译输入、链接与测试，保留当前安装成员与行为。默认 static 安装的 `InternalJudgement` 仅在确认不再被默认导出依赖、具名消费者已迁移且安装验收通过后，按下文窗口在下一获准的不兼容 minor 退出；证据或批准缺失就保留。纯运行测试跟独立内核配置，作者/codec 测试跟实验或工具组合。后续取得新时间/编译实施授权才落地 `timing` 和 `judgement_compile`；默认组合不提前启用新 wire。

**成品装载（待审推荐）。** 新 codec 直接采用保存的目标、窗口、等级和表现边界，不重算拍位或调用作者编译器。Reader 校验版本、CRC/内容身份、尺寸和预算，核对索引、引用、闭包及声明一致性；运行侧校验离散 tick 范围与算术、半开窗口与等级区间、阶段关系和会话配置匹配，再构造执行索引。只有校验成功才发布。旧 codec 暂保留[重装配、`semanticDiff` 与源闭包比对](../../../../engine/gameplay_packed/src/gameplay_capsule_read.cpp)，不重新量化或改写原 tick、wire 与 identity。旧装配只从 Packed 兼容入口调用，不暴露在运行头，也不允许新 Reader 依赖该入口。替换旧校验前，以固定成品和带有效完整性信息的语义损坏样本对照接受集、拒绝原因、边界、身份和回放；现阶段未验证等价。各格式可按实际变更独立演进，Stage 8 决定具名发行组合。

**单次推进输出（待审草图）。** 宿主在局准备时取得槽数和本局拥有型结果字典：`RunKey`、tick 单位/必要比例、阶段序号到 Requirement/阶段的映射及等级标识。`RunKey` 复用既有开局路径的 `sessionScope` 和 `runEpoch` 两个值，不新增 ID 生成或协议；字典优先复用现有拥有型元数据。宿主将槽与该局绑定，推进时传入 `RunKey`，返回同一个值；事件只持有值，字典和事件可独立保存，局清理、Retry 和卸载后仍能解释，不必保活整份 prepared。复用槽前，由宿主收存需要保留的事件。

```cpp
struct SettlementEvent {
    int64_t tick;
    uint32_t phaseIndex;    // Index in the host-owned run dictionary.
    uint32_t gradeIndex;    // UINT32_MAX means no grade.
    uint8_t outcome;        // Hit or Miss.
    uint8_t reserved[7]{};
};
struct AdvanceOutcome {
    RunKey run;
    size_t written;
    std::optional<Tick> processedThrough;
    AdvanceStatus status;   // ongoing, completed, faulted
    FaultCode fault;        // Fixed-size reason and stage; none on success.
};
// Events are in slots.first(outcome.written).
auto advance(Tick horizon, RunKey run, std::span<SettlementEvent> slots)
    -> core::Result<AdvanceOutcome>;
```

`P` 是本局完整展开后的实际可结算阶段实例数，对准备后的 Requirement 逐一累加声明阶段。可复用 [ContentProfileCounts::phaseDeclarations](../../../../engine/judgement/include/cuexis/judgement/gameplay_assembler_publication.hpp)，前提是统计同一展开集合且累加经过溢出检查。每个实例只在首次转为终态时提交一次 `Hit` 或 `Miss`，无输入到期也结算；进度、输入回执和其他事实不占槽。同局恢复保留终态，不得重发；Retry 属新局。

准备时检查求和、序号与等级索引的范围及 `P * sizeof(SettlementEvent)` 乘法溢出，再由宿主按现有资源策略分配或拒绝。草图槽布局拟为 24 字节，实施时以静态断言确认；例如实际含 `N` 个单阶段 Requirement 的谱需要 `24 × N` 字节槽空间，字典另计。40k 实体样本和 [Packed 的 Requirement 限额](../../../../engine/chart/include/cuexis/chart/limits.hpp)不足以推出全局 phase 上界；pending 限额不按零或固定上限处理。无需新增预算框架。

少于 `P` 槽、局不匹配或输出不可用时，预检以 `core::Result` 拒绝，不写槽、不改状态、不消费输入。预检通过后，按 tick 事务暂存结算，seal 成功才无分配写入槽；seal 前失败的 tick 不进入 `written`。`processedThrough` 只标实际封定的处理位置，尚无则为空，不用请求 horizon 伪造进度。若 seal 后 Fold 或后续 tick 失败，同次返回 `faulted`、固定大小的原因及已提交前缀，冻结本局；未完成 Fold 的成绩不作为正常交付。详细诊断可在返回后走既有路径，不让诊断分配掩盖已提交事件。不增队列、ACK 或结果有效期。验收覆盖 `P`/`P-1`（含输入不消费）、空局、到期 Miss、多路竞争、seal 前失败、seal 后 Fold 失败、同局恢复不重复及卸载后读取。

**共享 SDK（待审迁移）。** `PlaybackSession`、`PlaybackSource` 和 `PreparedPlayback` 在收口时统一默认与实验构建的类声明及布局。保留通用 source/entry 工厂、prepare/commit、更新、帧和资源接口，以及只读 `presentationSource`、`contentInfo`、`semanticIdentity`、`diagnostics` 等正式观察。实验自由函数借用现有会话或 prepared 对象，内部通过无条件声明的访问入口使用同一私有状态与 owner-thread 规则。下表覆盖候选块的成员名和宏外的候选呈现入口。分组表中的实验头名为拟名。

| 现有声明 | 目标去向 |
| --- | --- |
| `PlaybackSource` 的 `from{CxcFile,CxcMemory,Filesystem}GameplayEntry`、`fromGameplay{Entry,Packed,Graph}` | `experimental/gameplay.hpp` 的显式来源自由函数。工程、包、内存和 Graph/Packed 的校验与接入继续由现有 source/prepare 实现承担。通用非 Gameplay entry 工厂留在共享类。 |
| `prepareGameplayLoad`、`prepareGameplayReload`、`submitGameplay`、`advanceGameplay`、`gameplayState`、`queryGameplay{Capability,Presentation}`、`queryGameplay`、`gameplayFeedback`、`sampleGameplayPresentation`、`controlGameplay` | `experimental/gameplay.hpp` 的准备、输入、推进、控制和观察自由函数。`GameplayResult` 等拥有型观察值保留在实验侧。新推进复用上文当次输出；原推进语义在引入窗口由转发保留。 |
| `archiveGameplay`、`snapshotGameplay`、`evaluateGameplayReplay`、`restoreGameplay`、`seekGameplay`（两重载）、`checkpointGameplay`、`gameplayReplayCut` | `experimental/gameplay_replay.hpp` 的显式回放与恢复自由函数。第三方继续使用现有拥有型 archive/snapshot、已支持的 Graph/Packed 来源绑定和原回放语义。已带设备步骤证明的 `seekExecutionStep` 属于下行的内部协调。 |
| `invalidateGameplayClock`、`mapGameplayClock`、`beginRealtimeGameplay`、`beginRealtimeTail`、`deliverGameplayCapture`、`{request,query,retire}GameplayAdvance`、`gameplayAdvanceReceipt`、`acceptGameplayControl`、`{pause,resume}RealtimeGameplay`、`gameplayExecutionLag` | `experimental/gameplay_realtime.hpp` 的显式时钟、capture、推进与回执自由函数，承接现有安装实验宿主的实时输入路径。推进回执跟随这条路径，结算事件仍按上文当次返回。 |
| `enterExecutionFence`（两重载）、`commitExecutionCandidate`、`releaseExecutionFence`、`validateExecution{ControlBinding,FrameContext}`、`acceptExecutionControl`、`executionFence`、`completeExecutionClose`、`executionOwnerState`、`executionStepEffects`、`{retire,pause,resume,seek}ExecutionStep` | 归 Playback 内部协调入口，由具名 `execution_host_support` 和其测试消费，复用现有设备握手、世代检查及步骤提交。跨包宿主通过前述窄操作接入。安装实验样例中的 owner 状态与无绑定步骤探针迁到内部测试，安装样例保留可公开接入的正反行为测试。 |
| 私有 `validateExecutionControlProof`、`executionAdvanceRetention`、`gameplayWriteGuard`、`attachGameplay`、`PlaybackSource::fromGameplayEncoded` | 移到现有实现或内部访问代码，去掉共享头的候选私有声明。 |
| `PreparedPlayback::materializePresentation` | **后续公共面迁移，须单独获准范围。** 推荐由前述内部协调入口承担候选资源准备。获准后，按同一兼容窗口退出已安装成员，并随具名消费者迁出仅供内部协调的头与类型。正式 `presentationManifest`、`validatePresentation`、`acquirePresentationResource` 和只读来源观察留在共享面。D-19 已接受的 RR-4 窄迁移不含本行。 |

**兼容窗口（待审推荐）。** 引入期先在实验包新增自由函数，保留已有成员、符号、布局和默认行为。需要访问私有状态时复用已有的无条件 `friend` 访问声明；若需补声明，两种构建都加同一个 `friend`，不增加状态、虚函数或会话外壳，不改变布局或虚表。新旧入口共享一份实现，现有宏分叉只保留到收口。迁移 Player、具名工具和实验安装样例，分别验证旧源码用引入包重建、新源码按最低包版本重建。仅经验证为兼容的引入可按 patch 提案。若要加 `deprecated` 标记，先记录并批准迁移说明、消费者门禁和版本目标。规则见 [VERSIONING](../../../guides/VERSIONING.md#sdk-版本递增规范) 和 [ADR 0041](../../../adr/0041-legacy-format-exit-policy.md#兼容路径按版本退出)。

在下一获准的不兼容 minor（patch 归零）一并退出旧成员与符号、移除共享头的候选宏分叉、裁减仅供内部协调的安装头。收口前记录具名替代、安装与导出差异，以及外部消费者迁移证明。普通宿主从两种安装树以同一源码重建，实验宿主显式选择实验包并验证输入、推进、回放和恢复。static/shared 分前缀验证，并检查最低请求、过高 patch 和跨 minor 请求拒绝。0.x shared 消费者按同 minor、工具链与构建配置干净重建。证据不足时保留引入窗口，历史 candidate breaking patch 例外不适用本次迁移。

### P2 渲染与兼容迁移

| 事项 | 待审推荐 |
| --- | --- |
| Player 初始尺寸 | 短边设置和可用绘图区都按实际像素计算；初始目标按谱面比例放入可用区，只缩不放。平台建窗只接逻辑坐标时，由宿主换算请求并读回实际 drawable 像素，再把实际绘图区交给 SDK。例如 2× DPI、空间足够时，`720 px` 短边对应 360 逻辑单位的请求。 |
| SDK 内容矩形 | 宿主给定目标内左上原点的实际像素区域。先校验宿主矩形不越界，再用检查过的整数算术求最大居中比例矩形，受约束维取满，另一维向下取整，奇数余量给右／下。21:9 放入 `1280×720` 得 `1280×548`，相对宿主原点 `x=0, y=86`；`1280×721` 上留 86、下留 87。合法空目标、空宿主矩形或取整为空时返回空值，不提帧、不提交、不计呈现。新谱按作者比例投影，旧谱的内容矩形等于宿主矩形，投影随其宽高变化。 |
| 帧布局值与提取 | 保留现有 [`FrameViewport` / `FrameSnapshot`](../../../../engine/playback/include/cuexis/playback/playback_session.hpp)。独立实验头新增 `PixelRect{x,y,width,height: uint32_t}`、`FrameTarget{FrameViewport extent; PixelRect host}`、`FrameLayout{FrameTarget target; PixelRect content}`。`LaidOutFrame` 拥有快照、布局、帧来源及 Playback 的资源身份值，只读访问。`extractLaidOutFrame(const PlaybackSession&, FrameTarget, optional<uint64_t> exactResultVersion = {}) -> Result<optional<LaidOutFrame>>` 复用 `PresentationAccess::materialize` 的线程、重入、已提交状态及版本检查，从同一提交读取比例并生成布局、相机与物体快照。 |
| OpenGL 提交桥 | 独立实验适配头新增 `BorrowedGlFramebuffer{uint32_t name; FrameViewport extent}` 和 `submitLaidOut(OpenGlBackend&, const LaidOutFrame&, const render::RenderScene* debug = nullptr, const BorrowedGlFramebuffer* borrowed = nullptr) -> Result<DrawSummary>`。经内部访问桥与旧虚 `IPresentationRenderer::submit` 共用校验、命令生成及绘制；旧签名和行为保留。准备新比例内容时校验适配器能力，不支持则 prepare 拒绝。借用 FBO 仅限同一活动 GL context、当次调用有效；返回前归还状态，不交换窗口。坐标换算、清屏及绘制范围见 [D-23](architecture-design.md#d-23-谱面画面比例与宿主尺寸)。 |
| 帧交付与保留 | Player 沿用原循环，窗口提交成功后才 `present`；嵌入由宿主实际呈现后按原路径出回执。ExecutionHost 将布局与快照放进同一 `FramePayload`，计入现有存储预算，复用 `FrameEnvelope` 的帧归属、generation、资源及 attempt lease。保留帧按原拥有型来源与比例重算布局和投影。宿主可用自己的适配器及此前取得的匹配资源独立绘制旧帧，不回查当前会话、不发当前会话的呈现回执；内置桥不为此保留退役缓存。 |
| 旧入口 | 启动、像素和摘要探针取得替代证据后，按 D-25／ADR 0041 的实际发布承诺和消费者门禁退出；公共符号保留到批准窗口。 |

帧资源身份取自 Playback prepare 时的 `PreparedPlayback::presentationCandidateToken()`；`commit` 成功后保留值副本，提帧时随同一提交写入 `LaidOutFrame`。渲染器 prepare 产出的 `PreparedPresentation` 带独立的渲染器候选身份，供校验与 `activate` 使用，提交帧无需传这个句柄。正式提交继续校验帧归属和时效、实际目标，以及帧的 Playback 身份与已激活资源集是否匹配。

兼容新增按 P1；已装布局或虚表的变化须等获准的不兼容 minor。验收覆盖初始尺寸只缩不放、2× DPI 实际像素、横竖屏、奇数留边、嵌入偏移、空布局与越界、旧谱投影和不支持适配器的 prepare 拒绝。验证借用状态归还、区域外像素不变、正式呈现后才回执；保留旧帧重排时仅用匹配的原资源，不产生当前会话回执。正式提交遇目标或资源错配须拒绝。旧入口的替代证据见[渲染迁移](#渲染入口与兼容期)。

### P3 时间与会话时钟

| 事项 | 待审推荐 |
| --- | --- |
| Stop 模型 | 新映射沿 Chart 的单一作者 Beat 加正时长；Beat 0 归零只扣 `stop.beat < 0` 的 Stop 总长。旧 `TimebaseProfile` 的 Stop 按 `[startBeat, endBeat)` 工作，保留旧接受集及零拍相邻样例；单区间跨零仍按旧规则处理。 |
| Stop 端点 | 完整累计映射后分别量化绝对起点 S、终点 E，不把量化时长加到 S。只在新作者编译时拒绝量化后的 `E <= S` 和不同 Beat 的 `E == nextS`：按 D-21，前一个 Stop 的 E 返回该 Stop 的作者 Beat，且 `inStop=false`；同一 tick 若也是下一 Stop 的 S，则要求返回下一 Stop 的作者 Beat，且 `inStop=true`。不新增诊断类别。 |
| 同 tick | 不同音符可同 tick；不同 Beat Tempo 阶跃同 tick 按源 Beat 稳定顺序批处理取最终状态；同 Beat Tempo+Stop 沿既有语义。等级相对窗口仍按已接受的 `R(t)+R(δ)`，与 Stop 绝对端点分开。 |
| 查询时刻换算 | Playback 先估算实际所求时刻的源位置，对尚未校正的原始位置应用 `max(0, source - correction)`，再减谱面 offset；等价校正后观测不重复校正。必须在所求时刻 clamp，不能 clamp 锚点后线性外推。 |
| 起播与分支 | source=0、correction=3 ms、offset=20 ms 时逻辑时间为 -20 ms，直接扣 23 ms 错误。A 相加只在同观测基准的非 clamp 区间可作核对，不能替代完整映射或假定测试 A 自带 offset。输入补偿、显示提前各从共同逻辑时间分支应用。有音轨从源 0、无音轨从谱面时钟 0 起播，不 seek 到 offset 凑零点。 |
| 实时准入 | 新实时仅接固定 us／1:1 且身份、原点、校准可核对的映射，包含起播钳位边界；旧实时仅接具名验证适配。缺准入只拒绝请求的实时能力，保留普通播放及旧回放。旧普通路径和旧候选回放均保留原映射；设备切换／观测过期后重新核实。 |
| 后续 seek | 仅在新实时路径另获 seek 支持时，复用完整映射在实际可 seek 的源范围内求反解；目标不可达时拒绝，clamp 平台多解时取最早的可用源位置。source 0 起播和旧普通 seek 的既有范围、舍入与诊断都保持。 |

新作者编译在写入量化边界前完成算术与转换检查，再按源 Beat 顺序校验区间和碰撞。报告首错的源路径；碰撞附两个 Stop 的位置。失败不产出成品、不提交，也不将当前会话置为故障。旧 codec 不加这组检查，不公开旧 `profile_invalid` 私有码。

| 拒绝条件 | 诊断码与现有类别 |
| --- | --- |
| 累计或量化结果超 `int64` | 复用 `judgement.timebase.tick_overflow` → `budget_exceeded`。 |
| 量化后 `E <= S` | 拟新增 `judgement.timebase.stop_quantized_interval_invalid` → `invalid_relation`。 |
| 不同 Beat 的前一 `E == nextS` | 拟新增 `judgement.timebase.stop_endpoint_collision` → `invalid_relation`。 |

两个新码待审、未登记。验收：测 Stop S/E 及相邻 tick、累计／量化溢出、零宽、不同 Beat E/S 重合、同 tick 合法事件以及负拍、零拍 Stop；对照旧区间样例仍可接受且回放不变。另测 clamp 转折与跨锚点查询、正负 offset、暂停、输入和显示分支；将来接新实时 seek 时补不可达、平台多解和源范围边界。

### P4 Player 与旧控制权

| 事项 | 待审推荐 |
| --- | --- |
| 启动请求 | 官方 Player 一次启动固定来源／难度／用途／必要配置，更改时重启；第三方 SDK 宿主仍可同进程另 prepare。独立布尔选项拟名 `--judge`，显式校验失败报原因、不开局、不自动降级。 |
| Retry | **R=Retry 已定**，现行按键接线尚未迁移。只使用完好已加载内容，不重读来源；设备与会话安全就绪后从零开新局并直接播放。Stop 或自然结束后清理局状态，保留会话的不可变内容和健康已准备音频。 |
| Reload | F5 为待审键位。停止旧局的输出与推进，显式重读同一配置来源，复用 Load 的读取、校验与准备。B 须完成验证、加载、初始化及必要的设备接入才提交；B 从零暂停，等待显式 Play。旧操作不再引用 A 后才卸载 A。切换前失败只清理 B、保留已有的完好 A 并停播；切换后故障按 B 报告并停播，不伪称回滚。 |
| 纯画面 Rebuild | 从当前 A 的拥有型资源与设置重新准备并激活呈现，复用适配器的 `prepareOwned`、`accepts`、`activate`，保留本局位置和判定。无内容则拒绝；不重读、不调用 `prepareReload` 或提交内容，不处理音频设备。当前[组合 Rebuild](../../../../app/player/src/player_control_transaction.cpp)可能重新读谱并处理设备，须迁移调用方，不能作为 D-14 的实现证据。 |
| 无音轨与尾段 | 无音轨建议复用单调时钟、首版显式 Stop 结束，不从音频掉线自动切入。有音轨实机结束按 D-15 等 EOF 和未完成判定义务实际处理完毕，不增固定黑场时长或尾段字段。Retry／Reload／Stop 和局间寿命沿既有共识。 |
| 控制与故障 | 唯一控制者在 Retry／Reload 中先核实并复用可用的现有 seat，仅在必要时按原配置重开并确认实际接入；状态枚举不代替设备就绪，无需新恢复模式。Retry 的新局替代旧局暂停意图；设备回执只清自身原因，不清本次新到的 Pause。准备失败不挂待恢复后自动播放的意图。回执沿用 session／run／operation 归属；迟到回执不得影响新局。 |

**待审命令准入。** 完好 A 指不可变内容及健康的已准备音频数据；seat 或旧局仍可能故障。缺配置来源的 Reload 一律拒绝。对比[当前命令准入](../../../../engine/player_support/src/player_command.cpp)：目前 `Empty` 和 `Failed` 均拒绝 Reload；下表的首次失败恢复为新行为，须获准后实施。

| 状态 | Retry | Reload（有配置来源） | 纯画面 Rebuild |
| --- | --- | --- | --- |
| `Empty`，无 A | 拒绝 | 拒绝，首装走 Load | 拒绝 |
| `Empty`，却有 A | 诊断状态不一致、保留 A 并停播 | 同左 | 同左 |
| `Failed`，首次 Load 失败且无 A | 拒绝 | 复用首次 Load 准备；成功从零暂停，含设备接入在内的任一步失败仍无谱、`Failed`、停播 | 拒绝，不转 Load |
| `Failed`，有完好 A、设备可用 | 从零开 A 的新局并播放 | 保留 A 准备 B；成功切换，从零暂停 | 只恢复 A 的画面 |
| `Failed`，有完好 A、设备不可用 | 先接入；成功才从零开局播放，失败保 A 停播 | 准备 B 时确认必要设备接入，失败保 A 不切换 | 可恢复画面，不清设备故障 |

Rebuild 后按 D-14 恢复：普通播放只有在中断前正在播放、同一局确因画面中断而暂停且无其他暂停原因时自动续播；实机因画面中断暂停后先显示冻结画面，再显式 Resume。判定已 `Faulted` 的旧局不得借 Rebuild 续局，使用 Retry 或 Reload 开新局。D-12 的同局设备安全恢复条件不变。

验收覆盖矩阵各行及无来源、按键、无音轨长尾、独立暂停原因和迟到回执；注入 B 各准备阶段及切换后故障，分别核对停播、当前内容与退役时机。将磁盘来源改为 B 后 Rebuild，确认没有读取且仍显示 A；另测判定故障和重接设备期间新到的 Pause。

### P5 写回与诊断

| 事项 | 待审推荐 |
| --- | --- |
| 目标版本 | 首批只为 v3 源模型写 v3、v4 源模型写 v4；调用显式指定目标，未支持的组合报错。现有旧转换由 Migrator 按 ADR 0041 显式生成目标模型，再调用 Writer；本方案不增加 v4 → v3 支持或隐式升级。 |
| 写回结果 | 草图 `write(source, targetVersion) -> core::Result<ChartWriteResult>`；结果为 `{targetVersion, text, losses}`，损失项为 `{objectOrPath, field, kind, detail}`，用稳定对象标识或字段路径定位，说明写出后实际消失或改变的值/语义。`kind` 至少区分目标格式无法表达、已支持目标但 writer 尚未保真的字段。组合不受支持、不能生成合法目标文本或损失无法具体说明时返回错误，不给可写文本；空列表仅用于已核对无语义损失的保存。 |
| 保存职责 | 同版本可表示字段的完整往返是目标。[现有 `ChartWriter::write`](../../../../engine/chart/src/chart_writer.cpp)把非空 tempo/stop 写成空数组，这是 Writer 的保真缺口，不构成 v3 的格式限制。v4 以保真源树为底稿，将作者模型的编辑写入对应字段并保留未编辑的结构与不透明扩展；不能可靠写入且无法具体报告损失时拒绝。不能忽略编辑、仅写回旧 `canonicalSource`。底层写入工具只产出文本和损失报告；编辑器决定覆盖、另存、取消和自动保存。以后公开另走 [D-19](architecture-design.md#d-19-sdk-示例与宿主试用)。 |
| 格式边界 | 维持已定 Packed 前 92 字节 CRC 与 CXC profile/encoding；旧 Beat 上限推荐用对应 Reader 常量，只修受影响直接合同，不收紧其他版本。 |
| 诊断 | 保留底层原因，区分分配失败、容量拒绝和未知异常；未知／未测值标不可用，不冒充零或成功。新稳定码、映射和接受集合先经对应合同裁定；跨版本迁移沿 ADR 0041。 |

**非安装 Writer 同批迁移（待审推荐）。** `chart_writer.hpp` 在 [Chart 构建](../../../../engine/chart/CMakeLists.txt)中列为 PRIVATE 头，按 [ADR 0033](../../../adr/0033-cpp-shared-library-preview-boundary.md) 不构成安装 SDK 合同。[ADR 0041](../../../adr/0041-legacy-format-exit-policy.md) 保护既有读取和显式迁移，未保护这些内部 Writer 签名。按下表同批迁移后删除两个旧纯文本签名，不增加包装；源码消费者迁移说明列出旧签名到新签名的对应关系，并说明取得成功结果后必须检查 `losses` 才能按用途使用 `text`。CLI 仍经 Migrator 取得报告和文本后才提交输出。

| 现有调用 | 推荐去向 |
| --- | --- |
| `write(ChartDocument)`：`migrateToV3` | 改用 `write(source, 3)`；先完成显式转换并记录转换损失。 |
| `writeV4`：Migrator、`behavior_candidate`、Writer／C2 闭包／解析特征测试 | 改用 `write(source, 4)`；生成迁移或 identity 成品的调用仅在成功且无损时取 `text`。 |
| `writeCanonicalJson`：Migrator、Playback prepare、identity／解析测试；`writeAnimationTemplate`：v4 模板解析、Writer／解析测试 | 保留前者的全量 JSON 规范化及后者的 CXT 唯一写入实现；不改作 Chart 保存入口。规范化与 identity 单独验收，不借 Writer 迁移改变合同。 |

**先核对转换，再核对写入。** [现有 Migrator](../../../../engine/chart/src/chart_migrator.cpp)的 v3 路径会在调用 Writer 前清除部分 timing 和模板数据，另一路径将 `discardedFields` 初始化为空。按实际输入和目标模型，在转换阶段记录真实丢失的字段及原因，复用 `ChartMigrationReport::discardedFields`；与 Writer 损失合并后再判断能否交付成品。只有按既有显式迁移合同核对并确认无信息损失的规范化或等价展开，才不计为损失。其他结构变化仍逐项核对并记录实际损失；合法展开仍按既有迁移报告记录。转换或写入有真实损失时，迁移与 identity 路径用诊断交付字段和原因，拒绝交付成品；CLI 不写盘。编辑器直接调用底层 Writer 时可取得合法文本和逐项报告，自行决定是否保存。

验收：以非空 tempo/stop、动画、模板及 v4 编辑和不透明扩展写回，用对应 Reader 逐字段比较值、引用与有序语义；另验等价展开、转换先丢信息而 Writer 不报损失、Writer 单独丢失字段，核对路径、原因及无成品。缺目标支持、准备失败或编辑器取消时原字节不变。另用独立样本核对 canonical/identity、CRC、编码、文件／内存诊断及资源／未知异常。明细成本留在写入工具。

### P6 迁移与验收

| 范围 | 待审交付与停止条件 |
| --- | --- |
| RR-3：D-05 范围 | 仅 R1 构建隔离、R5 渲染收敛和 R6 文档减重。对照实际生产/实验构建目标与安装树，保留渲染替代和具名消费者证据；不借隔离删除已安装成员。 |
| RR-4：独立的公共面核查与窄迁移 | 前置为 RR-2 对相关候选入口及合同的裁定、R1 构建隔离完成。按 D-19，窄迁移只处理 `PlaybackSession`、`PlaybackSource` 的宏控候选声明；按 P1 核对具名消费者、替代入口、安装差异和版本窗口。`PreparedPlayback::materializePresentation` 等范围外迁移须单独获准；已安装成员/符号退出或布局变更仍须另经版本与合同裁定。 |
| 基线与差异 | 旧报告及样本保留原 SHA、配置和结论，只作对照。另记当前核验 SHA、较旧基线变化的路径/合同、应重跑的正反用例及已验证/未核实状态；新结果只归本次基线，不回填旧报告。 |
| 后续目标 | 新时间、新玩法和统一会话待相应实施授权，按本节顺序推进；R2/R3/R4 继续延期，不借本提案重启。 |
| 退出 | 每段检查相关行为、实际构建／安装及故障场景，不要求一次全矩阵；合同、行为证据或兼容窗口未齐时保留原唯一控制权和入口。长构建／hosted 按既有授权规则，不预定 SDK 版本或 Stage 8 格式组合。 |

## 产品、格式与安装边界

下表按源码和 CMake 记录现状。“默认 Player”指 `CUEXIS_ENABLE_CHART_V5_CANDIDATE=OFF` 时的普通 `--chart`、`--project`、`--cxc` 入口。

整包完整性检查与显式候选解码按 [D-18](architecture-design.md#d-18-按用途编译-player)／[D-25](architecture-design.md#d-25-兼容入口与格式合同收敛)；安装消费与可选判定按 [D-19](architecture-design.md#d-19-sdk-示例与宿主试用)／[D-24](architecture-design.md#d-24-sdk-判定接入与实验扩展)。其余数据流、目标划分和窄迁移推荐统一见 [P1](#p1-数据与-sdk)，以下保留消费者现状与核查入口。

Chart v1–v4 共用普通 Player 入口。Playback 准备谱面时，兼容读取器按 v1、v2、v3 各自的规则直接生成 `ChartDocument`，保留原版本。v4 用专用读取器解析源谱，并处理谱中声明的参数、动画和模板。两路随后都用 `ChartCompiler` 编译，进入同一播放运行时。播放器默认的 ChartClock 适用于无主音乐的谱，含主音乐的谱须选择 `--mode audio` 或 `--mode host`。[读取分流](../../../../engine/chart/src/chart_v4_loader.cpp#L233) · [编译与时钟条件](../../../../engine/playback/src/playback_session_prepare.cpp#L306) · [旧谱播放样例](../../../../tests/playback/playback_migration_equivalence_tests.cpp#L242)

Stage 7-RR 最初按加载器分组，把 `v5/Packed` 计作一组。下表分行列出可编辑的 v5 源谱和工具生成的 Packed 二进制文件，以说明谁会读写它们。[原研究的五组口径](../../../stage_reports/stages/stage-07-rr/2026-11-17-architecture-research.md#L77) · [Chart 表示与转换](../../../architecture/CHART_REPRESENTATIONS.md#1-三条输入族不是一条流水线)

| 条目 | 当前消费者与状态 | 默认 Player 可直接打开 | 建议边界 |
| --- | --- | --- | --- |
| Chart v1、v2、v3 | 三版共享兼容策略：[标准文本来源](../../../../engine/playback/src/playback_source.cpp)进入[兼容 Reader](../../../../engine/chart/src/canonical_chart_loader.cpp)；显式迁移到 v4 受 ADR 0041 保护。 | 是，`--chart` 或受支持的工程/CXC 声明的谱面。 | 每版保留可读样本及显式迁移验收；播放旧谱不要求用户先转档，也不覆盖原谱。 |
| Chart v4 | 当前默认生产基线；Player 普通文本、工程与 CXC 来源均走 Playback prepare。 | 是，`--chart`、`--project`、`--cxc` 的已支持入口。 | 继续作为默认格式线；不为候选入口改写 v4 合同。 |
| CXT v1 | 已接受的动画模板，由 Chart v4 引用并在准备时读取；见 [`animation_template_loader.cpp`](../../../../engine/chart/src/animation_template_loader.cpp)。 | 否，模板是被导入的依赖，不是独立可播放文件。 | 保持模板导入语义；不增加独立 Player 文件入口。 |
| CXC v1 | 已接受的 Project 容器；[`PlaybackSource::fromCxcFile`](../../../../engine/playback/src/playback_source.cpp) 经 [`CxcPackageLoader`](../../../../engine/cxc/src/cxc_package.cpp) 读取包及其声明入口。 | 是，`--cxc` 打开容器内默认声明的受支持内容。 | 分两层校验：先验 CXC 头、manifest、entry 路径与完整性，再由所选 payload 的 Chart/CXT Reader 校验语义；容器有效不等于 payload 有效。 |
| Chart v5 | 候选。工具可编译受支持的源谱，实验版 Playback 从工程或 CXC 读取显式选中的条目。[来源选择](../../../../engine/playback/src/playback_source.cpp) | 否。实验构建也会拒绝裸 `--chart v5.json`。在实验构建中，`--candidate-entry` 须与 `--project` 或 `--cxc` 搭配使用。 | 保留工具编译与实验条目入口，对外支持范围按现有 SDK 合同执行。 |
| Packed Chart v1 | v5 静态候选谱的二进制编码，`v1` 是这套编码的版本号。工具把支持的谱面数据展开、编译后写成 Packed，可将其作为条目放进 CXC 工程包。作者编辑的是源文件。[Packed 格式](../../../formats/PACKED_CHART_FORMAT.md#1-分层与设计选择) | 否。普通 Player 的 `--chart` 读源谱。实验构建可从工程或 CXC 显式选择受支持的 Packed 条目。 | 先校验二进制头和各段边界，再校验读出的谱面内容。保留显式开发入口。[候选条目检查](../../../../engine/cxc/src/cxc_candidate.cpp) |
| CXT v2 | candidate；[`CxtV2Loader`](../../../../engine/chart/src/cxt_v2_loader.cpp) 提供读取与展开，尚未成为默认 Playback 路径。 | 否；只作为显式候选格式处理，不作独立 Player 输入。 | 随 Chart v5 candidate 隔离；模板展开结果仍需通过对应 Chart/Gameplay 校验。 |
| Gameplay Capsule v2 | candidate 的类型化玩法内容；[gameplay_packed](../../../../engine/gameplay_packed/src/gameplay_capsule_read.cpp)提供读取，执行测试有 `--gameplay-packed` 专用输入。 | 否；测试参数不是普通 Player 的文件入口。 | 将 Packed 物理封装与 Capsule 的玩法数据分层校验，再验证静态 Chart、引用闭包和玩法语义；任一层通过不替代其他层。 |
| Compiled Gameplay Graph JSON v1 | candidate typed JSON 表示；候选 `GameplayContent::fromGraph` 路径见 [`gameplay_candidate.cpp`](../../../../engine/playback/src/gameplay_candidate.cpp)。 | 否；无默认独立文件入口。 | 校验 Graph envelope/revision 后，再由 typed reader 校验记录、身份与闭包；Schema 通过不能代替语义校验。 |
| 工具 | CXC pack/unpack/validate 属开发工具，见 [`cuexis_cxc_pack`](../../../../tools/cxc/cxc_pack/CMakeLists.txt)、[`cuexis_cxc_unpack`](../../../../tools/cxc/cxc_unpack/CMakeLists.txt)、[`cuexis_cxc_validate`](../../../../tools/cxc/cxc_validate/CMakeLists.txt)；Gameplay assembler 随候选构建提供，见 [`gameplay_assembler`](../../../../tools/gameplay/gameplay_assembler/CMakeLists.txt)。 | 否；工具消费不等于 Player 默认支持。 | 保留工具作为显式开发/验证消费者；不要据此扩大默认 Player 或 SDK 合同。 |

| 构建/安装面 | 当前状态 | 建议 |
| --- | --- | --- |
| 默认构建 | [根构建](../../../../CMakeLists.txt)的候选开关默认 OFF，但[Chart 目标](../../../../engine/chart/CMakeLists.txt)仍列入 Packed、CXT v2 等源码。运行时不接收，不等于编译图已隔离。 | 产品保留上表正式及兼容路径；R1 检查实际编译源、传递链接、注册测试及安装清单。失败时保留实现，绝不为缩小安装包而悄悄砍正式能力。 |
| 显式开发构建 | `candidate-*` preset 显式打开 `CUEXIS_ENABLE_CHART_V5_CANDIDATE` 并构建开发工具；构建 flavor 为 experimental。 | candidate 只在显式开发构建、具名工具或 entry 中保留；不把“能编译/能读”描述为 production。 |
| SDK 安装 | [Playback 构建](../../../../engine/playback/CMakeLists.txt)在候选开关下公开 `CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE`，加入候选头；experimental 安装需显式 `Cuexis_ALLOW_EXPERIMENTAL`。Player 应用本身不随 SDK 安装；官方 Player 是 SDK 参考应用。D-10 将 Player 与测试共用内部会话控制实现列为未来验收目标，Studio 仍为未来计划。 | 默认 SDK 长期维护；具体窄入口、消费者分组及安装验收采用 [P1 待审推荐](#p1-数据与-sdk)，成熟流程按 D-19。 |

**R1 先隔离源码和链接。** 用两套干净构建播放同一份旧谱，再打开同一个含正式和候选条目的合法 CXC。两版都应按默认入口播放正式谱、执行整包完整性检查，且不解码未选择的候选条目；显式选择受支持的候选条目时，实验版应能播放，默认版应给出拒绝原因。

[`PlaybackSession`](../../../../engine/playback/include/cuexis/playback/playback_session.hpp)与 [`PlaybackSource`](../../../../engine/playback/include/cuexis/playback/playback_source.hpp)目前随宏改变声明，与 [ADR 0042](../../../adr/0042-stage-6-productization-boundaries.md) 的目标不符。迁移按 [P1](#p1-数据与-sdk)，独立 RR-4 的前置与退出按 [P6](#p6-迁移与验收)。安装核验复用[外部消费者](../../../../cmake/VerifyExternalConsumer.cmake)和[参考宿主](../../../../cmake/VerifyReferenceHost.cmake)，并检查 `cuexis_internal/` 泄漏。

## 渲染入口与兼容期

比例与呈现归属按 [D-23](architecture-design.md#d-23-谱面画面比例与宿主尺寸)，坐标和取整采用 [P2 待审推荐](#p2-渲染与兼容迁移)。判定完成后的表现故障按 D-24，不重算判定或计分。下表保留需迁移的调用与探针。

| 范围 | 当前源码路径 | 建议目标 |
| --- | --- | --- |
| 启动静帧 | [Player 启动](../../../../app/player/src/player_app.cpp)在 `deviceFirst` 分支、准备内容前调用 `renderFrame(still)`。此时没有谱面快照。 | 用 Player 窗口外壳的启动占位绘制替换该调用，不创建谱面快照，不计作谱面帧已呈现。无来源时的「无资源」画面同属 D-16；明确指定来源失败仍按实际来源和原因诊断。用冷启动、准备前关闭和不可画的 surface 验证。 |
| 正式帧交付 | [Player 帧循环](../../../../app/player/src/player_control_frame_loop.cpp)填充 `RenderScene`，通过 [IPresentationRenderer](../../../../engine/presentation_renderer/include/cuexis/presentation_renderer/presentation_renderer.hpp) 的 `submit` / `present` 交付；`presentation.renderer.surface.zero_size` 时跳过，不 `present`。 | 保留 Render ECS、表现层和帧循环；比例布局按 P2 的提取与提交桥接入。有效帧至多提交一次，成功后才呈现；失败回执不报告呈现，重复通知不重复计分或重绘。 |
| OpenGL 兼容和 smoke | [smoke 测试](../../../../app/player/src/player_smoke.cpp)还调用 `renderPresentationFrame`，比较省略调试场景、空场景与填充场景的摘要，并有像素读回。它不是第二个产品帧循环。 | 对独立测试帧用正式 renderer 或测试专用观测比较摘要及像素，不能在同一产品帧多次呈现。替代不能验证原有差异时暂留探针；只迁调用不等于删除公开符号。 |

**退出规则已接受，迁移待实施。** 按 [D-25](architecture-design.md#d-25-兼容入口与格式合同收敛)，逐个核对旧入口的实际发布承诺及消费者；兼容包装只作过渡。[ADR 0041 D5](../../../adr/0041-legacy-format-exit-policy.md)保护的 `RenderFrame` / `renderFrame` 须取得版本与迁移裁定。若证实曾正式支持安装消费者，用代表性仓外宿主从安装树验证迁移；若证实仅有源码消费，记录安装门禁不适用，仍须取得保护入口的退出裁定。仍执行 ADR 0041 适用的 hosted consumer 等既有门禁，代表性安装宿主验收不替代这些条件。`renderPresentationFrame` 核对 OpenGL 头、适配器和消费者后，若证实仅供内部使用且替代用例通过，可单独删除。各入口须有适用的启动、零尺寸、像素或恢复场景的替代证据。

## 时间换算与判定 tick

已接受的基准、量化及窗口端点规则见 [D-21](architecture-design.md#d-21-谱面表现与判定共用拍位换算)。时间映射、Stop 绝对端点和实时会话时钟的待审推荐统一见[P3](#p3-时间与会话时钟)，本节保留现有候选基准和输入事实。

**已接受的编译精度：** 新内容量化到最近整数微秒，半数取偶；遇到非法时间值、溢出或量化后失效的窗口，编译拒绝。表现与判定使用同一次编译生成的离散 tick。[Chart 连续 BPM 换算](../../../../engine/chart/src/timing_map.cpp)使用浮点积分；不承诺跨平台位级一致。精度和接入细节以 [D-21](architecture-design.md#d-21-谱面表现与判定共用拍位换算) 与 [D-24](architecture-design.md#d-24-sdk-判定接入与实验扩展) 为准，实现未开始。

**窗口端点（2026-11-21 已接受）：** 目标时刻 `t` 与带符号相对偏移 `δ` 按上述规则分别量化，端点取 `R(t) + R(δ)`。同一等级表的相对窗口宽度不随目标 tick 改变，[D-22](architecture-design.md#d-22-谱师自定义判定等级) 的相邻档共用量化端点。`int64` 转换或端点加法溢出、非法或非有限值、量化后 `start >= end` 均拒绝，不扩窗。单就两次量化，绝对端点的误差至多 `1 μs`，不含浮点 BPM 积分误差；设备实际精度不因 tick 单位改变。

Playback 负责设备观测、音源位置和输入时间的会话换算与校正。旧成品保留原时间基准和规则，不统一重算，也不改写现有 wire 或 identity。作者精确拍位保留供 Studio 编辑。类型、旧入口迁移和新旧成品校验按 [P1 待审推荐](#p1-数据与-sdk)，新的格式版本仍须按 D-17 裁定。

**现有候选基准。** 候选 Capsule/Graph 携带玩法时间基准；[`TimebaseProfile`](../../../../engine/judgement/include/cuexis/judgement/timebase_profile.hpp)声明 `unitToken`、`tickScale`（一个 tick 有多大）与节拍映射所需数据。[现有实现](../../../../engine/judgement/src/timebase.cpp)能声明 `engine.tick.us.v1`、`us`、`1/1`，但不默认给出 tempo；`mapBeatToTick` 按 tempo/stop 积分，没有按任意 `tickScale` 缩放。对 scale 的校验为正，不证明每个正值都能对齐实时输入。输入侧的 `ObservationTick` 已由校准时钟给出，不再当作 beat 二次转换。

新实时映射的准入与拒绝边界按[P3](#p3-时间与会话时钟)待审推荐；旧谱、普通播放和既有候选回放保留原基准。此处不改 wire、FrameDigest、identity 字节或公共 ABI。

普通音频仍按 ADR 0042 的原公式和起播 clamp；候选时钟与 `TimebaseProfile` 现状只作为对照，不能据非 clamp 线性区间推定全程等价。新实时换算、输入／显示分支和起播反例见[P3](#p3-时间与会话时钟)；偏移核对见[登记册 RR-F307](findings.md)。

后续正反验收覆盖见[P3](#p3-时间与会话时钟)；普通 seek 的既有舍入和旧样本仍按原规则验证，候选 `steady.us` 不自动构成生产证据。

## Player 的产品行为

无来源启动和不自动开启判定沿 D-16 至 D-18；Retry、Reload 与结束交付沿 D-13／D-15。按键、待审的命令准入及故障验收见 [P4](#p4-player-与旧控制权)。

## 旧宿主的职责与退出条件

按 D-10，Player 与无窗口测试共用同一份**非安装的会话控制实现**，由唯一控制者决定会话状态。播放与可选判定在每会话的执行线程串行推进，设备及窗口仍按各自的线程要求工作。Playback 保留装载/提交与会话内换算责任，运行时判定内核 `judgement` 只依赖 `core`。独立谱面时间库与作者编译器的依赖边界按 [D-21](architecture-design.md#d-21-谱面表现与判定共用拍位换算)。现有协调部署见 [ADR 0046](../../../adr/0046-realtime-playback-coordination.md)；变更其约束须先修订对应合同。

| 现有消费者或职责 | 合并后由谁承接 | 撤掉旧控制权前须能证明 |
| --- | --- | --- |
| Player 控制器、候选 `ExecutionHost`：装载、prepare/commit、Retry/Reload、Pause/Resume/Stop | 现有 Playback 继续校验内容与提交；由唯一控制者决定状态并发布结果；Player 外壳只翻译 CLI/窗口意图。 | 同一组内存谱面不经 SDL 驱动真实控制代码。覆盖重复 Pause、切换前失败保留 A、先命中再 Stop、未开局与真实零分、同会话 Retry 无需再加载；普通路径不实例化判定。 |
| 捕获、时钟、执行调度与 replay/seek | 输入侧提供有序的本局输入；Playback 校验时钟并换算；判定执行路径确认实际执行边界，控制者决定是否推进。 | 用[时钟用例](../../../../tests/rpa/rpa_b3_clock_tests.cpp)和[回放/seek 用例](../../../../tests/rpa/rpa_b5_seek_host_tests.cpp)对照；过期观察、未交齐的输入不能被当作已执行，回放不虚构未来输入。 |
| 帧投递、时限、回执；故障收尾 | 渲染 owner 报告本局该帧是否实际提交和呈现，音频 owner 报告设备事实；控制者独占暂停/续局及结果交付决定。 | [renderer](../../../../tests/rpa/rpa_b4_renderer_tests.cpp)、[故障收尾](../../../../tests/rpa/rpa_b5_close_tests.cpp)：重复/超时/旧回执不能重复呈现、计分或结束下一局。 |
| `--execution-test-only`、[验收工具](../../../../tools/verification/execution_acceptance/player_evidence.py)与候选测试 | 工具改驱动上述同一实现；具名且不能同步迁移的调用方才允许临时转发。 | 列出最后的调用方、替代命令及对应证据；迁完去掉转发。历史脚本和已封存证据不挪不改，测试参数与旧控制逻辑分别退役。 |

切换以行为为单位。迁移前可在隔离用例上对比旧、新实现的同一条输入记录，不能在玩家同一局挂两个控制者。若正常和故障路径不能同时对齐，就让该行为继续由原实现承担，不删旧入口；撤销新接线也不得让已经执行的命令再执行一遍。

## 合同与诊断：接受什么，拒绝什么

维持 D-25 已更正的 Packed CRC 与 CXC profile/encoding 合同；整包完整性检查与候选语义解码按上文分工。其余条目保留各自待审状态。按 D-04，改变文件接受集合、判定身份或稳定码前，须先接受具体 ADR/Spec 修订。发现与处置状态以[登记册](findings.md)为准；Reader/Writer 互读仍不能代替独立互操作验证。

| 对象 | 建议采用的规则 | 验收或不动的条件 |
| --- | --- | --- |
| Packed Header CRC（RR-F111） | 已按现行[写入](../../../../engine/chart/src/packed_chart_tables_writer.cpp)和[读取](../../../../engine/chart/src/packed_chart_tables_inspect.cpp)更正[格式合同](../../../formats/PACKED_CHART_FORMAT.md)：计算 Header 前 92 字节，末尾 4 字节保存 CRC，总长仍为 96 字节。现有产物及其 artifact identity 无需改写。 | 后续用独立固定样本验证两种计算范围的差异、损坏拒绝和 Writer/Reader 互操作。真实外部消费者尚未核实；若需保留按旧文档算法生成的文件，另行明确显式迁移与哈希更新。Reader 不自动尝试两种 CRC。 |
| GridDelta、Beat、CXT v2（RR-F112/F113/F115） | GridDelta 不作为生产 Reader/Writer 的 mode；CXT v2 的谱内调用仍是 proposal，不把工具显式展开当作 Player 功能。这两处文档已收窄，不再立项。旧 v1–v3 Reader 的 Beat 分子/分母上限建议采纳[现有常量](../../../../engine/chart/include/cuexis/chart/limits.hpp)为对应格式合同边界，不借机收紧其他版本。 | 对各上限用刚好符合、刚好超界的有理数分别验证，区分 Reader 上限与算术越界。只有获批的生产读写与消费者出现，才重开 GridDelta 或谱内 CXT 调用。 |
| CXC playback entry（RR-F114） | [CXC](../../../formats/CXC_FORMAT.md) 与 [Chart Entry](../../../formats/CHART_ENTRY_V1_FORMAT.md) 已按[实现](../../../../engine/cxc/src/cxc_candidate.cpp)对齐：`candidate.static-tap-lanes4-v1` 必须使用 `packed-chart`，`candidate.behavior-v2-v1` 必须使用 `source-json`。保留显式候选选择和原默认路径。 | 后续各用合法 profile/encoding、错配、普通 source-only、未显式选择验证接受边界。正式化时按实际不兼容程度决定是否新增 entry revision，并明确兼容迁移；具体发行组合、版本和排期仍待裁定。 |
| 判定输入与诊断（RR-F131/F135/F142） | 同一 canonical 身份同 tick 再入按 [Gameplay Spec](../../../formats/GAMEPLAY_V2_SPEC.md) 的 `same_tick_collision` / `invalid_relation` 处理；只因 tick 相同不能把不同身份说成同一身份，真正的迟到队列重复另判。内存分配失败、达到容量上限、未知异常和操作成功分开；异常不跨模块。Player 把已有底层 fault 原样保留，在没有下层原因时才按失败阶段给原因，不让 `player.execution.test_only` 遮掉诊断。 | 对三类输入分别断言接纳/拒绝、code 和 category；注入资源耗尽、真实容量拒绝、未知异常，断言不假成功、不冒充容量。只改[散落失败](../../../../app/player/src/player_execution.cpp)中已核对的调用点，不重命名已有稳定码；涉及新码或映射先裁定。 |
| Packed 两个读取入口、JSON Schema 校验（含 RR-F404） | [文件读取](../../../../engine/chart/src/packed_chart_io.cpp)与内存解码保留共同的内容错误；打不开文件仍是 I/O 错误，不复活旧码别名。[Schema 校验](../../../../engine/json_support/src/schema.cpp)的 `Ok` 是“校验过程成功”，不是“实例合法”：需要合法内容的调用方还须检查本次违规诊断。 | 同一组损坏字节经文件和内存得到同一内容码；文件不存在可区分。分别测合法实例、违规实例、schema 无法编译，消费方不得仅因 `Ok` 放行违规实例。没有差异证据不另造解码器或错误层。 |
| 写回与观测（RR-F401/F419/F420） | 写回合同及验收见 [P5](#p5-写回与诊断)。首帧 `render_us` 未采到不当零时长；`exactLossCount=false` 不能读成“没有丢失”。 | 分别验证已知与不可得的测量；当前载体若不能区分则明确标不可用，由消费者忽略，不伪造数值或静默修改稳定 ABI。 |

这些用例是实施前需要准备的正反对照，本稿没有运行它们，也不要求一次清空所有登记项。P0/P1 按[计划 §9](plan.md#9-验收标准)逐条说明处置，P2 延期须注明接手阶段和重新评估条件。

## 文档减重、阶段交付与停止条件

| 归属 | 本方案建议的交付物 | 何时停止或交接 |
| --- | --- | --- |
| 现在：只读决策表 | 以产品、格式、时间、渲染为维度，为每个拟删/合并对象列出状态、实际消费者（产品/工具/测试/外部）、权威合同、替代者、迁移成本与阻塞。优先录入本稿各表中的对象，**仓内搜索不到生产调用不是删除许可**。 | 消费者未核清或受兼容保护，默认保留/隔离。`particles` 已处置、`debug` 已标 test-only，勿把旧发现重新列作“立即删除”；见[登记册](findings.md)。 |
| 交付与退出 | RR-3、RR-4 的边界、顺序和差异证据见 [P6](#p6-迁移与验收)。R6 将同一语义留在一份权威 ADR/Spec；计划记录范围，附页记录决策，代码注释保留约束与原因。 | 缺与对象相关的证据不宣布退出；RR-6 汇总并由 owner 接受。不删历史报告、归档或哈希绑定路径。 |
| Plan 拆分试点 | 维持[阶段计划 §6](plan.md#6-工作分解与依赖)和[交接要求](../../../HANDOFF.md)：只试点 RR-0 至 RR-6；`plan.md` 总览，`schedule.md` 只记 ID/波次/前置/并行或独占，任务卡只写任务级验收，`acceptance.md` 只写阶段验收。 | 以后用 `check_plan_schedule.py` 查覆盖、波次和独占，并作一次只交一张卡的真实无上下文委派；由主代理重跑、独立抽查含阴性用例。缺一步就不宣称试点完成，不向其他 active plan 推广。这里不创建这些文件。 |
| 后续阶段接口 | [Stage 7.5](../stage-07-5/plan.md) 只接收玩法能力和限制，[Stage 8](../../future/stage-08/plan.md)按发行矩阵决定格式提升，[Stage 10](../../future/stage-10/plan.md) 的 Studio 可复用会话；预览纯播放、实机和编辑后的新局遵循既定规则。 | 审阅接受后再提炼进相应阶段的范围、边界、待决与验收方向。future 仍是提纲，不提前建任务卡、定设备阈值、实现 autoplay 或定义 Studio 公共 API。 |

**延期不等于放弃，重启要有具体理由。** R2（候选公开面清理）等 RR-4 摸清外部消费者、差异、替代与版本窗口；R3（会话层收敛）等逐项行为和故障验证能证明只有一处控制权，避免为了少几个类重写所有事务；R4（时间表示统一）只在 P3 的适配仍不能满足具名消费者、且有 identity/FrameDigest 兼容方案时重评。三项均在本阶段退出后由 owner 另作决定；不能倒过来作为本轮开工条件。超出现有判定约束的玩法、输入域与计分规则交 Stage 7.5，不在结构迁移时顺带更改。

复核顺序是**先定接受什么，再验证谁在使用，最后才撤旧入口**。结构变更前保存与当前行为绑定的正反样例；候选开/关、普通/实机与故障场景分开取证。只回退本次接线或保留旧的唯一入口，不撤已交付的成绩、不静默改写旧文件，也不以测试专用参数或旧基线冒充生产证据。

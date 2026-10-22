# PD-5 操作级协作停止合同与 candidate Playback 接口方案

状态：paused candidate / not accepted / not implemented

更新日期：2026-10-23

本次暂缓：公开停止接口与 SDK 0.10.0 方向保持未接受候选，暂停推进；不作为现有合同内 factory、Player 生命周期和原 job 保活验证的必需前置。先处理有源码证据的最小缺陷，不恢复撤回候选，不宣称旧 40k 关闭阻塞已归因或解决。以下原方案作为候选保留。

本方案只定义后续接口与验收输入，不授权产品实现。审计基线是隔离分支
`codex/pd5-gameplay-40k-admission` 的干净 `115e4e64b066f87fc21274f3d64514ce138b69d9`。
保留 `be84f7a` 内核修复、现有 owner/thread/fence/Closing 规则和禁止私有导出规则。
不恢复已撤回的 `3df6046`、`9d51cd3`；不引入全局、进程注册表或 TLS 停止通道。
现行 API 和关闭行为在接受、实施及验证之前保持原样。
本轮生命周期/错误路径收敛从干净 `ea60b1fbe9d18f39f0d001126f775e76eec91578` 开始；
以下源码审计是静态证据，不表示失败注入或新关闭实现已经通过。

现行权威是 [ADR 0033](../../../adr/0033-cpp-shared-library-preview-boundary.md)、
[实时宿主边界 §2/§5/§7](../../../api/realtime-host-boundary.md)、
[Gameplay V2 ABI](../../../api/GAMEPLAY_V2_ABI.md) 和
[RPA 执行合同](../../../formats/rpa-execution-contract.md)。
既有修复证据见[内核诊断与关闭阻塞交回](../../../stage_reports/stages/stage-07/pd/2026-10-23-execution-core-diagnostic-and-repair.md)。
本文件是待核验方案的唯一正文；不把以下设计写成已支持能力。

## 1. 选择与适用范围

推荐通过**公开、受支持且只在 candidate SDK 安装的 Playback 类型**传递显式操作级 token。
停止源仅发布单调停止意图，Session owner 在协作检查点裁决实际结果；停止线程不操作 Session。
内部沿普通函数参数把借用上下文传至 Scheduler、Judgement、Kernel、恢复 codec 和准备链。
同一次嵌套调用复用该上下文，不查询 ambient 状态，不新增私有 DLL accessor。

这里的“操作”是一次顶层同步 SDK 调用，不是无限延续的会话或后台任务。
一个 worker job 可有多个顺序 SDK 子调用，用同一原 job ID 加不同 call ordinal 关联；
每次可能耗时的调用用新的 token，关闭意图在子调用之间阻止继续执行。
嵌套的 prepare/seek/decode 不创建新 token，也不重复认领同一操作。

`requestGameplayAdvance` 返回 Waiting 时，此次同步操作正常完成；其 token 随即终结，
不持有到未来 delivery。Waiting 的逻辑 request 继续由既有 Scheduler 账本和 K1 控制管理。
之后 `deliverGameplayCapture` 若启动该 request，使用 delivery 这次同步操作的 token，
同时保留被启动的**原 advance requestId**。旧 token 的迟到停止不能影响未来调用。
这避免把“一次同步停止”扩展成第二套异步请求取消协议。

| 方案 | 判定与维护成本 |
| --- | --- |
| Playback owning source/token，显式参数 | 推荐；共享状态在 Playback 二进制内创建/销毁，公开类型可按 ADR 0033 审计；需要一组小型 public types 和统一内部适配 |
| 直接公开 `std::stop_token` | 可利用现有 C++20/matching-toolchain 条件，但仍需额外关联身份、单次消费、结果输出和包兼容合同；不解决这些问题本身，本轮不并行提交第二套接口 |
| callback、裸 worker/atomic 指针 | 不选；引入跨模块回调、借用失效和异常/重入责任，破坏软件会话与宿主 owner 分离 |
| TLS、私有导出、全局表、调大 deadline | 排除；分别隐式传递/跨 DLL 分裂、违反导出合同、引入全局寿命，或掩盖问题 |

## 2. 拟议接口差异

以下为待实施的声明形状，**不是本轮新增的 C++ 头文件**。所有新增安装头保持 ASCII；
只对真实公共类使用 `CUEXIS_PLAYBACK_API`。`State`、内部 context、Kernel 和 worker 不导出。

```cpp
struct PlaybackOperationKey final {
    std::uint64_t clientScope, operationId, callOrdinal;
};

class CUEXIS_PLAYBACK_API PlaybackStopToken final {
public:
    PlaybackStopToken() noexcept; // NoStop
    // Copy/move/destruction are declared here and defined in Playback.
    [[nodiscard]] auto key() const noexcept -> std::optional<PlaybackOperationKey>;
private:
    struct State; // opaque; no exported State functions
    // Owning implementation handle, defined/managed by Playback.
};

class CUEXIS_PLAYBACK_API PlaybackStopSource final {
public:
    [[nodiscard]] static auto create(PlaybackOperationKey)
        -> core::Result<PlaybackStopSource>;
    [[nodiscard]] auto token() const noexcept -> PlaybackStopToken;
    void requestStop() const noexcept;
    // Copy/move/destruction are out-of-line and noexcept; no implicit stop/join.
private:
    // Shares only this operation's opaque stop state with its tokens.
};

struct PlaybackOperation final {
    PlaybackStopToken stop;
    PlaybackOperationReceipt receipt; // output only, owned/read by call owner
};

// New overloads retain each existing Result<T> return type.
auto requestGameplayAdvance(GameplayAdvanceRequest, const ClockObservation&,
                            std::int64_t, PlaybackOperation&)
    -> core::Result<GameplayAdvanceDecision>;
auto seekGameplay(const GameplayReplay&, GameplayTick, const GameplayReplayCut&,
                  std::span<const GameplayCheckpoint>, GameplayPresentationTick,
                  const RuntimeFrame&, PlaybackOperation&) -> core::Result<void>;
static auto GameplaySnapshot::fromBytes(std::span<const std::byte>,
    const GameplayContent&, GameplayCodecBudget, PlaybackOperation&)
    -> core::Result<GameplaySnapshot>;
```

`clientScope` 是调用方给出的关联命名空间，`operationId` 保留原 worker/channel job ID，
`callOrdinal` 区分该 job 的 SDK 子调用；无 worker 的消费者自行分配这三个值。
它们不是 HostContext/StepKey、不是 Gameplay requestId，也不授予控制权限；
既有 scope、generation、载荷绑定和重复 ID 拒绝仍单独验证。`clientScope`/`operationId` 不得为 0；
ordinal 从 0 开始且 checked 递增，不回绕。SDK 不创建全局 ID 分配器。

同一非空 token 的共享状态只允许一个顶层调用认领。第二次顶层使用，无论 key 是否相同，
均以拟议 `playback.operation.token_consumed` 拒绝；不重新执行旧请求。
空 NoStop token 对应旧行为，不引入一次性限制。外部不能通过复制 token 获得第二次消费权。
关联 key 不负责幂等性：重传请求先查询原账本，原终态不得因新 token 改写。
这里的重传规则适用于新的有效 token 或旧 NoStop 入口；已消费 token 仍拒绝再次使用。
cheap owner/context/身份检查、token 认领及既有终态查询均在第一次停止检查之前完成，
已知终态重传只读原结果，不为一个迟到 stop 改写为 StoppedBeforeExecution。

### 2.1 输出字段

`PlaybackOperationReceipt` 与 `PlaybackOperation` 拟放在 `gameplay_candidate.hpp` 中，
位于所需 Gameplay owning 值/receipt 定义之后；source/token/key 放新 candidate `operation_stop.hpp`。
工厂声明可前向声明 `PlaybackOperation`，Session/Source 的安装头按现有 candidate 条件引用。
不复制 `execution_control.hpp` 的 StepKey 定义，不制造头文件循环。

| 拟议字段 | 语义 |
| --- | --- |
| `key` | 可缺省的本次同步调用关联身份，复制自 token；NoStop 时 absent，不采信 receipt 的输入值 |
| `disposition` | `NotStarted` 初始值；终态为 `Completed`、`StoppedBeforeExecution`、`Stopped`、`Failed` |
| `stage` | 实际阶段：Preflight、Preparation、InputAdmission、ReplayRebuild、Kernel、Ledger、Fold、Reconciliation、Publication、Commit；成功为 Completed |
| `stopRequested` / `stopObserved` | 终态输出时的快照；有意图不等于已停止，正常完成可有 requested=true、observed=false；返回后的意图不追写 receipt |
| `gameplayRequestId` / `controlOperationId` | 本次实际关联的原逻辑请求/控制 ID，可缺省；不得用 operationId 替换 |
| `prefixTarget` | None / ActiveSession / Candidate，标明进展属于活动状态还是临时候选 |
| `prefix` | 可缺省的固定进展字段：requestedHorizon、processedFrontier、lastAdvanceHorizon、failedTick、foldedWorkTick、sealed/publishable fact cursors、实际 faultStage/kernelFaulted；未知值保持 absent |
| `sealedResult` | 可缺省的 owning `GameplayResult`，保留对应的实际前缀；不借用已销毁候选 |
| `effects` | 已发生效果位：InputAdmitted、KernelSealed、ControlSealed、RuntimePublished、ClockRouteApplied、ActiveReplaced；不能据缺位推断跨设备取消成功 |
| `confirmedDeliverySequence` | delivery 已承认的实际前缀；停止不撤销已承认输入 |
| `cause` | 停止过程中若已形成原 kernel/Fold 错误，移动保留原 error/context/cause；不重建或省略 fieldPath/category/severity/faulted/sourceCode |

receipt 是输出值，没有 SDK 权限或第二份可写会话状态。调用期间仅 owner 写；
请求停止的线程不得读取这个普通值。完成后通过现有 worker reply/ledger 的发布同步传递，
消费者按原 ID 查询；直接 SDK 消费者保留返回的 Result 和 receipt。
StopSource 只保留 intent/单次认领状态，不另建进度/结果缓存或新全局 query 服务。

`Completed` 表示这次 SDK 调用按原协议正常返回，不保证其返回的逻辑 decision 是 Executed：
Waiting、BusyNotAccepted、以及原协议以值返回的 Failed decision 均保留原值。
内层失败仍在 decision.error/prefix/cause 中保留真实阶段；不得把它误报为逻辑成功。
若本次确实观察并执行了协作停止，则返回拟议 stop 错误并填 Stopped receipt，
同时在原逻辑账本保留真实 decision。既成原生错误优先时，仍返回旧的 Result/decision 形状。

所有新 overload 都是“原参数 + 必选 `PlaybackOperation&`”，没有隐式默认全局 source。
旧 overload 的签名、Result 和拒绝行为保持，委托同一实现的 NoStop 上下文。
新路径不得调用一个丢失 token 的旧 public overload；内部委托显式借用同一上下文。
对带 class export 标注的类型，private member 也可能成为 DLL 符号，因此不能直接给
`PlaybackSource::fromGameplayEncoded`、`PlaybackSession::prepare/attachGameplay` 加私有 overload
或改成带 context 的导出签名。旧签名按需保留 NoStop 转发，带 context 的共用实现提取到
`src` 中未导出的 State/implementation helper；不得用公共类白名单放过新增私有 helper。
实际导出差异必须按声明的公共 API 清单逐项核对，不能只检查是否含 `detail` 字样。

## 3. 同步入核入口完整清单

本节按基线源代码的调用关系分类，而非按方法名猜测；“入核”包含 prepare、submit、query、
snapshot、archive、controlRpa 和隐式历史执行。矩阵的 E 编号同时是后续适配覆盖清单。

| 编号 / 公共入口族 | 基线调用链与行为 | 后续 token 接入 |
| --- | --- | --- |
| E1 `GameplayContent::fromPacked/fromGraph` | `makeGameplayContent → JudgementSession::prepare → Kernel::prepare`，前面还有正式 Reader/shared prepare；不 advance | 两个工厂新增 overload，准备/解码共用 context |
| E2 `PlaybackSource::fromGameplayPacked/fromGameplayGraph/fromGameplayEntry`，`fromCxcFileGameplayEntry/fromCxcMemoryGameplayEntry/fromFilesystemGameplayEntry` | 包/文件/metadata → 私有 `fromGameplayEncoded → E1`；入核准备前必须保留 R4 metadata/Header/semantic 校验顺序 | 六个公共工厂新增 overload；私有 helper 仅内部参数，不新增导出 |
| E3 `prepareLoad(PlaybackSource&&)`、`prepareReload(PlaybackSource&&)`，各含 prepare options overload；`load(PlaybackSource&&)`、`reload(PlaybackSource&&)` 及 options overload | Gameplay source → `prepare → JudgementSession::prepare`；load/reload 随后 commit | 这些 Source 形态在 candidate 条件下新增 overload；prepare 到 commit 同一个 context |
| E4 `prepareGameplayLoad/prepareGameplayReload` | E3 后 `attachGameplay → makeGameplaySession → prepare`；某些输入可能先后准备两处 | 两个 overload；两处传同一 context，不以更换来源跳过正式入口 |
| E5 `submitGameplay` | `JudgementSession::submit → submit/submitBatch`；无 advance，但会承认输入 | 新 overload，记录 admission 效果边界 |
| E6 `advanceGameplay` | `JudgementSession::advance → advanceRecorded → timer/ledger/Fold`，随后 Gameplay publication/runtime | 新 overload，前缀和 publication 结果分开 |
| E7 `requestGameplayAdvance` | Scheduler `request → tryStart → session.advance`；也可能 Waiting/Busy/原终态重传 | 新 overload；只控制本次同步执行，不延续 Waiting token |
| E8 `deliverGameplayCapture` | Scheduler `deliver → submitCanonical`；闭合输入后 `tryStart → advance` | 新 overload；必须带入间接执行，并记录原 advance requestId |
| E9 `pauseRealtimeGameplay/pauseExecutionStep` | Scheduler `pause → advance(controlDrain=true) → controlRpa(pause)` | 两个 overload；不能只覆盖普通 advance |
| E10 `resumeRealtimeGameplay/resumeExecutionStep` | canonical trace → Scheduler.resume → `controlRpa(resume) → ledger/Fold`；本身不调用 advanceRecorded | 两个 overload；保留 route 已 Applied 后的 A1/ResumeAborted 规则 |
| E11 `evaluateGameplayReplay` | `JudgementSession::evaluateReplay → Kernel::evaluate → prepare/submitCanonical/controlRpa/advance` | 新 overload，只作用于候选执行，活动会话不被替换 |
| E12 `restoreGameplay` | `recover → Kernel::restore`；RPA 按 records advance/controlRpa，generic 结构重建还可能调用 advanceRecorded | 新 overload，不能仅在外围 decode 传 token |
| E13 `checkpointGameplay`；两种 `seekGameplay`；`seekExecutionStep` | `seekCandidate` 的 prepare、checkpoint restore、逐记录 submit/control/advance 和尾部 advance；step 还先调用 Replay.toBytes 再启动 realtime | 各现有形态新增 overload；嵌套验证/执行不重新认领；seek step 不在停止后继续 beginRealtime |
| E14 `GameplayReplay::fromBytes` | `decodeReplay → initial(prepare) → records submit/controlRpa/advance` | 新 overload；中断不能被误当“记录中预期的 fault” |
| E15 `GameplaySnapshot::fromBytes` | `decodeSnapshot → initial(prepare) → Kernel::restore → E12` | 新 overload；旧 fault 等价检查不得吞掉 stop |
| E16 `GameplayReplay::toBytes` | `encodeReplay → initial → Kernel::prepare`；不是仅把已有 vector 写出 | 新 overload；也覆盖 E13 的 archive identity 计算前段 |
| E17 `controlGameplay(Reset/Stop)` | `makeGameplaySession → prepare`，随后替换活动 Gameplay；Pause/Resume 只切旧非 RPA 状态 | 新 overload，控制值校验和现有 lifecycle 拒绝先保持 |
| E18 `snapshotGameplay/archiveGameplay` | `Kernel::snapshot/captureState`、`canArchive/archive`；复制/取得 owning 快照/记录，不重放 | 新 overload，为规模循环和候选输出设检查点；不声称会 advance |
| E19 `beginRealtimeGameplay`、`queryGameplay`、`executionOwnerState`、`executionStepEffects`、`gameplayExecutionLag`、`controlGameplay(Pause/Resume)`、`deliver/request/resume` 的预查、`sampleGameplayPresentation` | 进入或间接读取 `Kernel::query`；Scheduler.create/状态同步亦只读；这些读取自身不执行 timer/advance | 单纯 query 保持原签名和 owner 规则；复合方法内部由外层 context 控制，不能在线程外 query Session |
| E20 `gameplayState/queryGameplayPresentation`、`beginRealtimeTail`、`acceptExecutionControl/acceptGameplayControl`、`invalidateGameplayClock/mapGameplayClock`、advance query/retire、fence/commit/release/close/retire、Replay cut/feedback/receipt | 已提交缓存、准备好的 timer/requirements 扫描、Scheduler K1/账本/时钟或控制证明；其中状态缓存不入核，关联状态检查可读取 E19，但没有隐藏 tryStart/advance | 不新增“跨线程取消 Session”重载；已有 K1 负责未开始的 Waiting request；Close 收尾不能被普通停止 token 再次取消 |

补充排除与边界：普通 Chart JSON/typed-project/CXC Chart 的 source 工厂及 JSON 的 prepare/load/reload，
在基线不构造 Gameplay kernel；`PlaybackSession::update` 只做 Runtime update，不隐式 advance Gameplay。
`GameplaySnapshot::toBytes`、configuration encode/decode、GameplayResult 的 accessor/sameResult、
Replay.cutAt 和能力查询不调用 Kernel 执行；其复制/扫描仍遵循现有预算，但不伪称已获得新停止保证。
test-only KernelTestAccess 不属于 SDK 入口。后续新增入核调用必须补 E 表及测试，不能默认 NoStop。

内部传递覆盖：`session.cpp` 的 prepare/submit/submitCanonical/controlRpa/advance/recover/checkpoint/seek，
`execution_scheduler.cpp` 的 deliver/request/tryStart/pause/resume，`execution_kernel.cpp` 的
prepare/restore/evaluate/seekCandidate/advanceRecorded/controlRpa/captureState，
`recovery_codec.cpp` 的 initial/encodeReplay/decodeReplay/decodeSnapshot 都接受显式上下文。
一个 context 贯通新建的候选 kernel；不可在 restore 的重验证或 codec 的 replay 中丢失。

## 4. STATIC/SHARED 所有权与并发

StopSource/Token 的共享状态只含关联 key、原子停止位和单次消费控制；不包含 Session、worker、
provider、ledger、renderer 或任意 host callback。状态由 Playback 创建，其最后释放通过 Playback
定义的 out-of-line 特殊成员完成；STATIC 与 SHARED 走同一代码，不借 EXE/DLL 各自的 inline TLS。
Source.create 的分配失败用 Result 返回，尚无产品副作用。

token 是 owning handle。Session 调用开始后在栈帧保留强引用，嵌套 helper 只借用此稳定对象，
不在操作结束后把裸 context 留进 Scheduler、Prepared、kernel 或 Replay；候选重建也遵守这一点。
Source 先销毁不撤销 token，不自动 requestStop；旧 token 不能带入下一操作。
有意长期保存的 owning receipt/result 只保留不可变前缀，不保留会话可写指针。

停止控制中，允许非 Session owner 线程调用的动词仅为独立 owning Source handle 的
`requestStop()`：幂等、noexcept、不分配、
不等待 owner、不调用用户代码，release 发布意图；owner acquire 检查。
不得并发移动/析构正在使用的同一个 handle 对象；线程各持自己的副本。
PlaybackOperation 及 Session 方法仍受 owner/reentry 约束，输出未发布前不能跨线程读取。
这些类型从不允许另一个线程析构 PlaybackSession/PreparedPlayback。
Source.create/token 及独立 handle 副本的特殊成员本身不绑定 Session owner；
它们不能被用来从别的线程调用任何 Session 方法。
handle 的 copy/move 依赖匹配工具链，均不得分配；moved-from Source 仅允许析构或赋值，
不能从无效 Source 静默取得 NoStop 来满足一次要求可停止的调用。默认空 Token 只用于显式 NoStop。

Windows SHARED 验证必须是 EXE 中 host/requestStop 经**实际公开导入**到 Playback DLL，
DLL 内 checkpoint 读取同一 owning state；不能用 EXE 内静态 judgement 代替证明。
保持 matching-toolchain、matching-runtime `/MD` 和现有安装/版本拒绝；任何 handle、调用、
prefix result 尚存活时不得卸载其 Playback DLL。candidate OFF 不安装这些类型和新入口。

## 5. 停止裁决、错误与查询

停止意图不等于停止确认，也不等同 B1 Apply/Cancel、K1 CanceledNotExecuted 或成功 Close。
普通 30 秒 replyDeadline 不调用 requestStop；超时仍用原 job/operation ID 查询，保持未知阶段。
只有明确的终止 worker/宿主关闭意图请求协作停止，不能借它绕过设备责任域。

| 真实位置/竞态 | Result 与 receipt 的拟议结局 | 实际状态/查询责任 |
| --- | --- | --- |
| 已接纳但尚未 dispatch 的 worker job，终止关闭先到 | 同原 job ID 返回 stop 诊断，StoppedBeforeExecution；不运行 SDK，不构造 prefix | 入口和对象状态不变；由 host 原 channel/ledger 保留可查询终态 |
| SDK cheap owner/身份/生命周期前置拒绝已确定 | 原错误与 Failed 优先，不为了 stop 替换既有已知错误 | 旧拒绝上下文不变；尚未运行的昂贵验证不为了证明“可能有错”先执行 |
| SDK 检查到 stop，尚未开始内核/语义操作，且无输入承认或其他副作用 | `playback.operation.stopped`，StoppedBeforeExecution，effects 空 | 活动 Session 不变；候选释放；无 fake failedTick=0 或 kernel Faulted |
| 活动 advance 已开始，首个或后续 tick/timer/ledger 尚未 seal | stop 诊断，Stopped；保留初始或最后 sealed prefix 和真实 kernel faultStage/failedTick | 丢弃未 seal 草稿；沿现有故障前缀收束，Scheduler 原 request 终态 Failed，绝不是 CanceledNotExecuted |
| 已 seal 当前 tick，Fold 被停止 | Stopped，prefix 为已 seal kernel + 既有 Fold 前缀，faultStage=Fold | 不把完整 kernel seal 回滚；原 request/error 与 owning prefix 保留 |
| 优化 idle hardDeadline tick | 每 tick 在 active/faultReserve 成对同步之前检查；两份同步是不可分的无失败短区间 | 观察 stop 后使用最后同步的镜像，不让旧 view 或失败前缀被修改 |
| delivery 已承认输入，之后触发原 Waiting request 时停止 | Stopped，effects/confirmed sequence 记录已承认输入，关联原 advance requestId | 已确认输入不撤销；未开始的 request 仍 Waiting，只有后续符合既有 K1 的 Close/控制才能记 CanceledNotExecuted；已进入 advance 则 Failed + prefix |
| Replay/restore/Seek 临时候选中停止 | Stopped，prefixTarget=Candidate，保留必要 owning 候选前缀；临时候选析构在 owner | 不把候选装成活动状态；活动会话/旧 view 不变；已有 Host fence/route 副作用另记，不假回滚 |
| Resume 的 final route 已 Applied，R2 seal 前停止 | Stopped，ClockRouteApplied 等效果保留；无 kernel fault 时按既有 A1 进入 ResumeAborted/Suspended | 不再直接 Resume，不声称 route 未生效；原 StepKey 控制结果按实际副作用保留 |
| 正常工作在最终不可回退提交区间前已观察 stop | Stopped + 实际效果，不继续新的语义/发布工作 | 若 kernel 已完成 H，prefix 如实含 H；原逻辑 advance 可已是 Executed，外层 publication/job 仍可 Stopped |
| 最后检查后进入预留好的无失败提交/返回区间，或已完成后才来 stop | 原成功结果、Completed；意图可为 requested=true/observed=false | 不倒改已提交结果；发送 requestStop 的时刻本身不承诺抢赢；相同请求重传只读原终态 |
| 已形成真实 kernel/Fold/codec/provider 错误，再出现 stop | 原 Error、Failed 优先；stop 意图只作观察信息 | 保留首错、真实 stage/context/cause，不将 allocation/identity failure 改写成停止成功 |

新 stop/token-consumed 错误仅属于 candidate Playback 操作边界，不新增 N3 类别或修改旧 sourceCode。
stop 的主诊断在操作启动前预留；若内核为收束前缀返回了原失败值，将该值完整移入 receipt.cause。
`fieldPath/category/severity/faulted/sourceCode` 和递归 cause 保持原值；不只锁 code/message。
内核的 fault mirror、Replay/Snapshot wire 和既有 fault 编码不以新 stop 诊断重新编码。
缺失前缀用 absent；不能为出错回执先深拷贝整个模型或整份 preimage。

内部可使用显式 tagged stop 返回，或在同一模块栈内抛受控标记后就地捕获；
任何方案都必须让 kernel seal/Fold reserve 先按真实位置收束，外层 codec 不能把中断解释为
`codec.record_invalid`、allocation failure 或“重放恰好命中档案预期 fault”。
公开边界捕获所有异常并返回 Result，销毁和实时路径仍不抛异常。

终态查询沿现有权威：worker 原 job ID 返回 Result+receipt；已接纳 advance 的原 requestId
由 `queryGameplayAdvance` 保留结果；控制保持原 operationId/StepKey 并记录实际已生效阶段。
owner 在更新这些记录后才发布 channel reply；未能可靠发布时保持 Pending/Closing，不能捏造确认。
无副作用的 const Replay/codec 调用只保存候选/返回值，不给活动 Session 注入 Faulted。
终态 receipt 完成后不可变；现有 ACK/retirement/retired-prefix 规则不缩短，也不为 query 自动重跑。
停止同步调用不凭空终结另一个 Waiting 逻辑请求。正常 Close 在 owner 上完成原 K1 裁决后，
宿主保留其 owning decision/前缀再回收软件会话；不能在会话销毁后重建 SDK query，
也不能用 job 已停止冒充该逻辑请求已被 K1 取消。责任域确认缺失时仍 Pending/Closing。

## 6. 协作检查点与资源成本

检查只观察停止意图，不执行 SHA、不改变预算预检顺序、不改字段遍历/摘要算法或 wire 身份。
检查点覆盖：

1. 顶层 preflight 后、第一次产品副作用前；准备/重建阶段转换和进入下一候选前。
2. prepare/index/phase、codec records、恢复验证、Replay/seek 的外层循环，以及每次嵌套 advance/control 前。
3. 每个 tick 开始；timer、input、ledger/fact、Fold、RPA trace/reconciliation 等规模循环按固定小块检查。
   初始候选间隔为最多 64 个迭代单元；内层仍有规模工作时也加检查，不能只算外层 64 次。
4. `be84f7a` idle deadline 快路径的每个 tick；成对更新 active/faultReserve 内不插入抛出/分配点。
5. seal 前与 kernel seal 后 Fold 前；Fold/公开发布/活动替换边界保留各自故障含义。
6. 候选完成、进入 no-fail swap/publication/返回区间之前；该短区间内不回调、不分配、不重新判 stop。

64 是待验证的轮询工作量设计，**不是时间 deadline 或生产性能门槛**。
单次 provider 调用、分配/释放、容器扩容、不可拆标准排序、系统调用和驱动调用不保证可中断；
前后有检查点也不能承诺固定墙钟收尾时间。不得为停止响应重写 comparator 以抛异常破坏容器不变量。
可拆的自有循环才加有界检查；任何后续算法重构另作保语义 diff/验证。

每操作一份固定 stop state/认领状态与预留错误/结果包装；内置宿主的数量随既有有限 channel/
请求保留数受控，不声称 SDK 能限制任意外部调用方自行保存的 handle 数量。
公开 receipt 的 owning prefix 优先复用既有不可变投影，必要的小包装在执行前预留；
停止/错误路径不能新增全模型复制。Source/Token 清理无 join、无 callback。
hash 入口新增适配缓冲仍纳入既定同一 scratch 账本，不能改名为 operation 存储绕过预算；
七项物理预算和四维 accepted ceilings 均不提高。实现时需列真实分配及边界失败清理证据。

## 7. 生命周期、错误收尾与析构前置条件

### 7.1 线程寿命与 Host 状态分开

下表是拟议的 worker 寿命合同，不新增 Playback 执行状态，也不把 HostState::closed 当作线程证明。
成功创建线程的事实须单独保留；不能只凭 exited=false 判定它仍在运行。

| worker 状态 | 确认依据与允许的清理 |
| --- | --- |
| NotStarted（未启动） | 从未成功取得线程；可能尚未构造 delivery，也可能 std::thread 创建失败。只清理已构造成员，不访问空 delivery、不等 exited、不 join；可正常销毁 |
| Running（运行中） | 已成功启动，尚无 owner 退出确认；即使空闲、SDK 返回或装配尚未完成，也存在活动 owner，不能销毁包装 |
| Closing | 已发出显式关闭意图，关闭普通 admission，但保留必要 control/query/cleanup；worker 可能仍执行或阻塞，不能销毁包装 |
| Exited（已退出） | owner 对象与资源已经在正确线程清理，终态已保存，再 release 发布退出、由宿主 acquire 确认；线程句柄仍可能 joinable，不等于已 join |
| Joined（已 join） | 已确认 Exited 并成功 join，句柄不再 joinable；可释放线程包装，Host 还须满足独立责任域收尾条件 |

普通路径为 NotStarted → Running → Closing → Exited → Joined；启动后的 owner 初始化失败也可
Running → Exited，但须记录实际启动失败、清理部分构造成员并发布终态。线程可能在工厂交出 Host
前已退出，不能用“工厂返回成功”代替 Running/Exited 的真实观察。构造函数未完成时，只有已构造
成员展开，没有完整对象析构；尚未启动的失败无须伪造 Exited/Joined。

拟议宿主顺序为：明确 beginShutdown → 关闭普通 admission 并保持 Closing → 请求当前 SDK source 停止 →
owner 取得实际结局并终结/回复原 job → 沿预留的 control/query/cleanup 通道完成原 Close/K1 及责任域确认 →
保留 owning 终态回执 → 请求最终 worker 退出 → owner 清理 Playback/Prepared/ledger/provider → 发布 exited →
main 确认 exited 后 joinExited → 释放 worker/host 包装。音频/renderer/capture 的责任域确认仍独立。
beginShutdown 不能先关闭 Close 所需的控制/查询入口；最终退出才关闭剩余 admission。

当前 source 发布与关闭需短锁串行化：worker 在锁内检查 Closing 并登记新的 owning source；
requestShutdown 在锁内标 Closing 并复制当前 source，在锁外 requestStop。
清除当前 source 与开始下一子调用也遵守该锁；先 Closing 后登记不能漏掉停止。
内置 worker 的 E1–E18 调用必须登记有效的非空 source；创建/预留失败在产品副作用前终结，
不能降级成 NoStop 继续该作业。
锁内不执行 SDK/provider、等待、join 或用户回调；它是实例内控制交接，不是全局停止通道。
排队未开始 job 在原 ID 下终结，已执行 job 不伪报未执行；owner 仍负责其全部对象析构。

### 7.2 现行返回/展开路径审计与后续责任

以下位置以本轮固定基线为准，指出待实施缺口；本轮不修产品代码。

| 调用方/位置 | 当前可达路径与缺口 | 接受后必须承担收尾的 owner |
| --- | --- | --- |
| [SessionWorker::create / 析构 / requestShutdown](../../../../app/execution_host/session_worker.cpp) | create 在 worker 分配后才创建 delivery，再启动 thread。delivery 创建失败会返回错误、展开 worker；现析构无条件 requestShutdown，而该函数无条件访问 delivery_。线程创建抛异常时则 delivery 已存在、thread 未启动；构造中途抛异常只展开已构造成员。不能以同一个 exited 前置条件覆盖三者 | 工厂内 construction owner 区分 NotStarted；未启动资源必须可局部、空值安全地清理，不调用活动 owner 的关闭协议。FrameDelivery::create 的零 consumers 等返回错误可静态到达上述缺口，尚未做运行复现 |
| [ExecutionHost::create / 构造](../../../../app/player/src/execution_host.cpp) | 先 SessionWorker::create 启动线程，之后 new Host/成员装配仍可能失败；catch 返回裸错误，局部/成员 unique_ptr 展开会落入现有 worker 析构 join。尚未交出 Host 不代表未启动 | 工厂持有 construction owner；推荐先装配 Host 外壳、ports、renderer 借用和全部可失败资源，最后启动 worker，并用 noexcept 所有权交接完成发布。后启动异常不得随错误返回丢失活动 owner，见下文 |
| [Host load](../../../../app/player/src/execution_host.cpp)、[AudioWorker::start](../../../../app/execution_host/audio_worker.cpp)、[Player audio factory](../../../../app/player/src/player_execution.cpp) | load 才启动可选 audio owner；线程创建失败、设备/provider/decoder 的 Result 错误或捕获异常会沿 audio reply/load 返回，先前 session worker 仍可能存活。AudioWorker 未启动失败也不能套用 exited=true 条件 | 已交出的 Host 仍由 Player/test 持有，保留各 worker 的独立寿命和已生效 load 阶段；音频设备/transport 的部分构造在其实际 owner 上清理，设备失败不隐式终止别的 owner |
| [Player runExecutionPlayer](../../../../app/player/src/player_execution.cpp) | load 中 Host 创建后，内容读取/load、advance、sample/present、reload 的 close 或最终 close 均可经 take 抛 Failure。函数级 catch 在体内局部变量展开后才接住；不能再持有 Host 驱动收尾。host 早于 actions 等泵所借用变量声明，故展开中的 Host 析构也不能假设这些捕获仍存活 | Player 的外层寿命持有者保留 Host、ports/pump 的全部捕获、backend/window/runtime；操作置于其内部 try/catch。保存首错后转入关闭驱动，不在此处直接返回 Result<void> 或重建 backend |
| [PD capacity](../../../../tests/pd/pd5_capacity_probe.hpp)、[Trace](../../../../tests/pd/pd5_capacity_r4_support.hpp) | Trace::take 把错误记录后抛异常；Trace::run 记录失败再重抛，REQUIRE 也可能展开。显式 host-close/diagnostic-close 仅在已到达该步骤时执行；当前无覆盖失败路径的外层持有者 | test case 的外层寿命持有者保存 fixture/renderer/Host、原始错误或 exception_ptr 与实际阶段；内层执行测试，退出内层后先驱动关闭/查询/最终 join，再重抛原断言或报告失败 |
| [PD host probe](../../../../tests/pd/pd5_host_probe.cpp)、[host delivery probe](../../../../tests/pd/host_delivery_probe.cpp) | pd5Take/take 或 REQUIRE 可跳过尾部 close；host probe reset 分支也是 close 成功后才 reset，失败仍展开。各自局部 Host 的应急析构不能替代显式收尾 | 同一测试 owner 在失败后继续持有 Host 及 renderer/设备上下文；重置/重载前必须完成旧 Host 关闭和 join。无设备 probe 可无平台泵，但仍需驱动/查询控制与退出 |
| [PD matrix probe](../../../../tests/pd/pd5_matrix_probe.cpp) | 主 matrix case 直接持有 owner-thread PlaybackSession/renderer/delivery，没有 SessionWorker；尾部 renderer.close 同样会被异常跳过。非 device 目标另 include capacity case，后者按上行处理 | 主 matrix 的 Session 在创建线程清理，renderer 生命周期/显式 close 仍受测试外层持有者保护；不把它误记为异步 Host worker，也不为了本方案更改共享 fixture |

工厂推荐采取 **start-last**：construction owner、泵上下文和失败记录先就位，线程启动成功之后
不再做可失败的主线程装配/分配，仅执行已证明 noexcept 的交接；worker 自身初始化失败由 worker
记录、在 owner 上展开后发布 Exited。若仍存在启动后装配/发布失败，必须由工厂仍然持有的 construction
owner 在内部捕获、驱动 Closing、泵主线程并保存终态，直至安全 join 后才返回错误；其清理不能靠
guard 析构临时启动，也不能将活动 unique_ptr 藏在已返回的 unexpected 中或交给全局回收器。
尚无已发布的软件/设备会话时只做对应的局部关闭，不虚构 K1/设备确认。
工厂若要提前返回，必须另有已明确交给调用方的 owning Closing 对象；当前裸 Result<unique_ptr<Host>>
错误分支没有这种能力，本方案推荐不增加第二种工厂返回协议。

Player/测试的关闭驱动在原 owner 上运行：关闭普通操作接纳，登记首个业务失败与原 job/request/StepKey，
显式请求协作停止，推进原 Close/K1，继续泵必要的主线程/设备事件、查询原 ID 的真实终态并保留 owning
前缀，最后确认退出和 join。关闭期泵不再排入 reload/advance/普通输入；pump 的错误作为次级收尾诊断
保存，不能抛出越过寿命持有者或覆盖首错。不能通过新 ID 重做原失败操作来“查询”结果。
close 返回 Pending 或其他尚未确认关闭的错误后，继续保留同一持有者和上下文，推进/查询原关闭操作；
不 reset Host、不卸载 DLL、不提前释放 renderer/ports，也不在下一轮无条件创建新的 Close/K1。
SDK 会话销毁前取得终态副本，销毁后只查询宿主已保留记录，不能调用已销毁的 Session。

当前 Player 的 Result<void> 和测试的普通返回都不能承载活动 Host；因此关闭尚未确认时不能用该返回
退出持有者作用域。保持 Closing、继续有节制地等待/泵/查询；provider/allocator/系统调用阻塞可能使
它长期不能返回，协作停止不提供固定退出时限。测试框架可展开的断言/C++ 异常须延后重抛；清理阶段
避免新的致命断言跳过 join。abort、不可恢复硬断言、OS 强杀不承诺执行 C++ 收尾，外部测试观察只能
记录未确认退出，不能据进程消失宣称 close/join 成功。

### 7.3 合法销毁与 fail-fast 的严格适用域

**以下为待 owner 接受的前置条件，须先完成上面的调用方迁移才能启用违例处理：**

- NotStarted 的完整包装可正常销毁；部分构造成员按 C++ 规则清理。线程创建失败和资源装配失败
  本身是普通错误，不要求 exited=true，也不触发 fail-fast。
- 已启动的 SessionWorker 包装须先确认 Exited；可由显式 joinExited 转为 Joined，析构最多只 join
  已确认 Exited 的句柄。Exited 与 Joined 仍分别记录，不把析构开始当作退出证明。
- Host 析构前还须保存终态、完成适用的软件/设备责任域确认，并确认全部 worker Exited 或 Joined。
  推荐显式完成 join；析构至多补 join 已确认 Exited 的 worker，包装释放前全部转为 Joined，不等待 Running/Closing。
  owner-bound 对象仍在创建 owner 释放；无会话的启动失败按实际构造范围清理，不强求不存在的 Close 证据。
- 只有调用方违反上述已明确前置条件、强行销毁仍活动的 owner，才属于 noexcept fail-fast 的候选违约场景；
  不 detach、不释放线程仍访问的内存。实现前还须验证所有内置错误路径符合前置条件。

普通 Result 错误、advance 失败、超时、close Pending、OOM、可展开的断言/异常、provider 阻塞均不是
fail-fast 触发条件。不得把“处理错误时栈展开至析构”当作正常收尾策略，再把该错误自动升级为违约终止。
超时本身不请求取消；上述显式关闭意图才请求停止。未确认 owner 退出、终态或责任域确认时保持 Closing，
即使超时反复发生也不自动 fail-fast。它不改变 PlaybackSession/PreparedPlayback 在创建 owner 上析构的旧规则。

## 8. 后续验证矩阵（本轮不执行）

所有 V 项在固定新源码 SHA、独立 Release 目录、实际测试发现和原始日志下执行。
STATIC 和 Windows MSVC SHARED 都为必需；candidate OFF 做安装/符号负向检查。
只用 1/64 plumbing 和有界 small/medium/人工 barrier，禁止据此开启 40k 或阶段 C。
barrier/计数器仅测试构建，需被显式启用且正常产品构建不存在。

| 编号 | 验证对象 | 必须观察到的结果 |
| --- | --- | --- |
| V1 | 真实 STATIC 与 SHARED worker→Playback→Kernel；分别源/操作/检查点位置 | SHARED 的请求在线程 EXE、检查点在实际 DLL；同一 owning state，禁止私有导出/TLS/global；记录模块/源码/二进制绑定 |
| V2 | E1–E18 每族；特别 capture 唤醒 Waiting、Pause drain、Resume controlRpa/Fold、Replay/Snapshot decode/encode、generic/RPA restore、checkpoint/Seek fallback | 每条链都到达指定 checkpoint；没有某条间接 advance 丢 token；查询 E19/E20 不误触发执行 |
| V3 | queued/认领前/首 tick/首批 timer，及第一 tick 尚未 seal | StoppedBeforeExecution 或真实首次草稿中止；无虚构前缀/错误 Tick；未执行 job 保留原 ID |
| V4 | 后 tick、同 tick timer/ledger 循环、seal 后 Fold、控制 reconciliation | 同原故障参考路径比较完整 DTO、原 cause 全上下文及 sealed/Fold 前缀；不漏已承认输入 |
| V5 | `be84f7a` 优化 idle deadline、快路径前后及末 tick | 逐 tick 检查不绕过 token；active 与 fault mirror 始终成对；逻辑 timer/cursor 保留；不恢复全量复制 |
| V6 | stop 与最后 checkpoint/no-fail commit/正常返回/原生错误 的 barrier 竞态 | 两种合法顺序分别确定；完成不被迟到 stop 改写；既成首错优先；记录 actual effects，不能仅比较耗时 |
| V7 | retain old query、Snapshot、publication view；活动执行与候选 restore/evaluate/Seek 分别停止 | 旧 owning 值不变；候选未替换 active；必要前缀保活，候选析构后仍可读；Live/ Candidate 标识正确 |
| V8 | Waiting 返回后的旧 source、delivery 新 source、重复 token、原 ID 重传/ACK/retire | 旧 token 不控制未来调用；新 token 停止间接执行；尚未开始的逻辑 request 保持 Waiting，原 Close/K1 后才有取消终态；已执行的 Failed 前缀可查；重复不重跑，retired prefix 拒绝旧 ID |
| V9 | route Applied 后 Resume 中断；Seek 已 fence 后中断；仅输入已承认 | A1/ResumeAborted、Suspended/Closing 和原 StepKey 不丢；不谎报 CanceledNotApplied 或全局回滚 |
| V10 | 两阶段 shutdown 的 publish-source/close-admission 竞态、队列剩余 job、所有子调用间隙 | 无遗漏当前 source、无新作业开始；每个已接纳 ID 有真实终态或明确 pending，无复活 |
| V11 | Source/Token 不同顺序销毁、不同线程不同副本、候选/Prepared/Session 析构和最终 join | 无 UAF/跨 owner 析构；worker 内全部析构先于 exited；只 join 已确认退出线程；只有违反 §7.3 前置条件的强行析构才单独子进程验证 fail-fast |
| V12 | 可控 provider/分配器/system-call barrier（有限释放，不模拟可任意打断） | 停止不假装中断阻塞；未释放时 Closing、无假 exited/Closed；释放后 checkpoint/清理/正常 join 可观测；不靠测试超时证明成功 |
| V13 | source.create/适配器预留/发布/前缀包装 OOM，重复请求与错误清理 | 无泄漏、无 stop-path 深拷贝；原错误/context/cause 保留；无可靠终态时不确认 Close；完整分配/预算清单可审阅 |
| V14 | 无 token 的旧 golden/拒绝/Replay-Snapshot bytes、R4 metadata 顺序/单次 SHA、预算边界 | 字节/身份/预算/旧拒绝不变；停止时丢弃不完整 SHA，不产生第二编码器；R4 没有被旁路 |
| V15 | 新 candidate 安装消费者、SHARED exports/imports/不兼容包拒绝、架构/格式/文档、相关 Release 回归 | 只导出真实 public stop classes/重载；candidate OFF 不暴露；内部 Judgement/Runtime/worker 不泄漏；版本决定先接受再实施 |
| V16 | 启动前参数拒绝、成员部分构造/OOM、FrameDelivery 失败、std::thread 创建失败 | NotStarted 可正常销毁；只清理已构造成员，空 delivery 不访问，无 join/等待 exited/stop/fail-fast；保留原始启动错误 |
| V17 | Host 启动后装配/发布失败、worker owner 初始化失败 | 证明 start-last 的分配顺序；对残余后启动故障注入，construction owner 保活并泵/查询，原错误保留，owner 清理→Exited→Joined 后才能返回错误，无提前展开活动 owner |
| V18 | load/首次及后续 advance 的 Result 错误与 Player/test take 抛出 | 首错、实际阶段、原 ID 与 sealed prefix 保留；外层持有者不丢 Host；显式关闭前不因失败自行取消，关闭后只终结适用的原请求；无 fail-fast |
| V19 | close 返回 Pending、pump 返回错误、provider barrier 导致关闭不能完成 | 未释放 barrier 时仍 Closing，Host/回调捕获/renderer/DLL 保活；次级错误不覆盖首错；继续原操作终态查询，无新 Close ID/普通任务；释放后真实确认退出并 join |
| V20 | Player 与三类 Host probe 的可展开 REQUIRE/异常退出、reload close 失败 | 关闭驱动运行时所有泵捕获仍有效；先保存断言/exception_ptr，后收尾，再重抛/返回错误；清理无新致命断言绕过 join；硬 abort 只记录非正常退出，不伪造成功 |
| V21 | 正常关闭后析构、未发布会话的启动失败、已 Exited 尚未 join 的销毁 | 分别观察 NotStarted 或 Running/Closing/Exited/Joined；worker 内析构、exited 发布、join、包装/设备释放有序且各一次；无会话不造 K1；未启动与普通错误路径不进入 fail-fast |


终态验证同时断言 code/message/fieldPath/category/severity/faulted/sourceCode/cause、原 ID、
实际 failedTick/faultStage、prefixTarget/effects、query 结果和退出状态，不能只以进程返回 0 判定。
受影响 codec 还要证明 stop 不被 fault-replay 等价分支吞掉，旧有效/无效档案行为不变。

## 9. 后续合同修改位置与维护成本

| 待修改位置 | 接受后才实施的内容 |
| --- | --- |
| `docs/api/realtime-host-boundary.md` §2/§5/§7 | 停止意图与 K1/B1/timeout 分离；两阶段 Closing/析构前置条件；公开 candidate token 跨 DLL |
| `docs/api/GAMEPLAY_V2_ABI.md` | 新类型/overload、owner 并发、Result+receipt、兼容/安装；不暴露内部 kernel 类型 |
| `docs/formats/rpa-execution-contract.md` | 原 request/StepKey 终态、控制 drain/Resume/间接 advance 的停止效果；不改 wire 字段或算法 |
| `docs/adr/0033-cpp-shared-library-preview-boundary.md` | 若需补例，只说明新增的是实际 public 类型；禁止私有导出正文不放宽 |
| `engine/playback/include/cuexis/playback/operation_stop.hpp`（拟新增），`gameplay_candidate.hpp`、`playback_source.hpp`、`playback_session.hpp`；`engine/playback/CMakeLists.txt` 与 `cmake/VerifySharedExports.cmake` | 上述两类 owning handles、output record、E 表 overload；只 candidate 安装；真实公开符号的精确清单 |
| Playback/内部 Scheduler、session/kernel、recovery codec | 一份显式 borrowed context 和统一操作边界；结果/停止适配不复制语义编码；内部函数不加 DLL export |
| `app/execution_host`、Player Host/Player、PD probes | 实例内 active source 交接、原 job 账本、五态寿命/start-last、外层持有者与显式关闭驱动；先迁移全部失败路径再启用析构违例检查 |
| 包版本与消费门禁 | 推荐下述 SDK API 0.10.0 一次性裁决；接受后统一更新 SDK 元数据/安装消费者/导出与兼容拒绝门禁，本轮不改 SDK/日期版本 |

主要成本是 E 表的参数贯通、live/candidate/codec 的失败适配和两种链接模式矩阵。
通过一个内部 context、一个 public receipt 定义、既有 ID 账本和同一 semantic codec 控制维护面；
不追加全局缓存、回调系统、每会话跨线程 mutation API、第二份调度器或新异步 query 服务。
没有 token 的路径须维持快速 NoStop 分支；检查开销只做受控观察，不产生正式性能接受结论。

### 9.1 推荐版本与包兼容处置（待一次性裁决）

推荐 **candidate SDK API 0.10.0**，不在 0.9.x 内追加这组类型/overload 与更严格的宿主销毁前置条件。
依据 [ADR 0030](../../../adr/0030-playback-preview-api-version-and-result.md) 和
[ADR 0033](../../../adr/0033-cpp-shared-library-preview-boundary.md)：这是新增公开 preview 合同，
当前仍为 matching-toolchain C++ preview；现有 `CMakeLists.txt` 使用 SameMinorVersion，
`cmake/CuexisLibraryType.cmake` 的 SHARED 名称/SOVERSION 使用 SDK major.minor。

| 组合 | 推荐处置 |
| --- | --- |
| 原 0.9 consumer + 原 0.9 包 | 保持原合同；不回填 fail-fast 或悄然改变关闭行为 |
| 请求 0.10 的 consumer + 0.9 包；请求 0.9 的 consumer + 0.10 包 | SameMinorVersion 双向拒绝跨 minor 的带版本请求；分别做负向 configure 检查，不以手工链接侥幸成功作为兼容 |
| 0.9 源码迁移到 0.10 candidate | 原无 token overload 保留签名/NoStop 与既有 wire 行为；需要全量重新编译，Host 错误收尾按 §7 迁移，不承诺免改宿主集成或二进制替换兼容 |
| 0.10 candidate STATIC / Windows SHARED | 同一 SDK minor、同一 package build、matching toolchain/runtime/config；独立干净 build/install/consumer。SHARED 以 `-candidate-0.10` 身份区分，不混用旧头/库/DLL |
| candidate OFF 或未声明接受 experimental 的 consumer | 不安装/导出 stop surface；沿现有 build-flavor/experimental 同意门禁拒绝候选包误消费，不伪装为稳定 SDK |

未来验证消费者使用 `find_package(Cuexis 0.10.0 EXACT CONFIG REQUIRED ...)` 固定本次候选，
并检查 package build/flavor、头/导入库/实际 DLL 身份；EXACT 版本自身不证明这些身份。
保留仓库 SameMinorVersion 规则，不引入只针对 stop API 的第二套包版本或独立 stop DLL。
SDK API 值、生成头、包版本/库名、安装 consumer 和负向兼容测试须在接受后同一轮实施/验证；
日期构建身份按独立版本政策处置，不随本建议自动改变。Capsule revision、Gameplay.2、Replay/Snapshot
wire、golden、七项物理预算、四维 ceilings 和 1.10/15% 门禁均不变化。

## 10. 本次交回与待接受项

本轮提交范围仅本方案与 CURRENT_STATUS；已有索引保留。没有修改 C++、CMake、SDK/版本、golden、冻结证据。
本轮实际只做静态调用审计、文档检查和 git diff 检查；V1–V21 全部为后续计划，未执行。
未运行 40k，未进入阶段 C，未创建正式 preparation/registration，旧窗口 3 保持停止。

请核验以下一组方案决定后再授权实现：

1. 同步操作级 public owning token；Waiting 后用后续同步调用的新 token，保留原逻辑 ID。
2. 同 Result 签名加显式 output operation record、首错/前缀/实际效果及 completion-versus-stop 规则。
3. §7 五态寿命、start-last 与错误收尾持有者；NotStarted 正常销毁，活动 owner 保持 Closing；仅明确的销毁前置条件违例采用 fail-fast。
4. 一并裁决 §9.1 的 candidate SDK API 0.10.0/包兼容建议，以 V1–V21 为后续实施入口，保持禁止私有导出及所有冻结预算。

本次提交不代表这些决定已被接受，也不授权恢复候选或继续阶段 C。

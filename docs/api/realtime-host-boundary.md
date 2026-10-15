# 实时宿主边界合同

状态：candidate contract；B3在SDK 0.8.0提供同owner执行首用；跨owner/设备宿主协议仍待B4/B5

更新日期：2026-10-11

决策归 [ADR 0046](../adr/0046-realtime-playback-coordination.md)，执行/时间/控制语义归
[RPA 补充 Spec](../formats/rpa-execution-contract.md)，批次和门禁归
[RPA 计划](../stage_plans/active/stage-07/plan-rpa.md)。本页拥有载体、接口、所有权和宿主状态。
旧候选 TB-01–07 表由补充 Spec 的已选合同替代；不改变当前头文件或宣称已通过设备验证。

## 1. 表示与接口

以下是实现设计，字段不得靠 sentinel 表示缺失。Tick/Span/Delta 使用现有 i64 strong types；
ID/cursor/代数为 checked u64，耗尽拒绝而不 wrap；text 是 owning UTF-8 string；list 是 owning vector。
跨域只传不可变 owning 值。运行 scope 使用不透明 owner 生成 token，不得把地址/hash当判定身份。

| 载体 | 必需字段 | 生产者与消费者 |
| --- | --- | --- |
| MessageContext | sessionScope、clockSegment、runEpoch、controlSequence；资源消息另带 contentIdentity/resourceGeneration/rendererGeneration | 每个 owner 检查自身预期上下文；不同代数不合并 |
| ClockObservation | 连贯源/设备/presented 位置、各自 rate、sampleMonotonicUs、clockDomain、publicationSequence、state/quality、validUntil、errorBound、segment | audio/host adapter 发布；D1/D2 读同一 owning snapshot；字段语义归 Spec §2 |
| ClosedCaptureBatch / CapturePrefix / EndRunEpoch | Spec §1/§3 的完整成员、计数、连续序号、正常 exclusive B 或控制 inclusive q/cut | D1→D2，整批校验后准入；不能只传最后序号 |
| AdvanceRequest | context、requestId、commandOrdinal、原 H、所用 observation ID、profile identity | D6/显式驱动→D2；等待不重写 H |
| AdvanceOutcome | requestId、commandOrdinal、D2 decisionOrdinal、Waiting/BusyNotAccepted/CanceledNotExecuted/ClockInvalidNotExecuted/Retired/Executed/Failed；实际 receipt、失败阶段和前缀只在相应分支存在 | D2→调用方；不把宿主状态加入 kernel enum |
| ControlRequest | context、operationId、commandOrdinal、kind、expected generations、显式目标；Seek 必含 archive identity/cut/H | D6 排序，D2/D3/D4/D5 在各自 owner 准备或应用 |
| ControlReceipt | operationId、commandOrdinal、D2接入 decisionOrdinal、controlSequence、Prepared/Rejected/SoftwareApplied/DeviceApplied/Complete/Suspended/Failed、各参与域的实际位置和 error | 每域独立确认；排队或发送不算生效 |
| OwnerStepRequest / OwnerStepReceipt | context、operationId、controlSequence、participant、phase、expected generation、Apply/Cancel/query；回执含 Pending/InProgress/RejectedNotApplied/CanceledNotApplied/Applied/Failed/Unknown、实际副作用和 error | 每个 owner 串行裁决一个步骤；步骤终态不是整个组合控制完成 |
| PauseTrace / Reconciliation | pauseEpoch、连续 capture 序列、来源/动作/provenance、末尾 cut；软件记录 controlOrdinal、applied cursor 和 deferred occurrence 引用 | D1→D2；不走 Paused 禁止的普通 submit |
| FrameEnvelope | owning FrameSnapshot、frameId、T、RuntimeFrame、Fold cursor/workTick、projectionScope、controlSequence、content/resource/renderer generations、feedback membership | D3→D5；完整帧可 latest-value 合并 |
| PresentReceipt | frameId、scope、controlSequence、renderer/resource generations、Succeeded/Failed、实际调用时刻与 error | D5→D3 的内部观察通道；不进 Gameplay identity |

内部 API 选择：D2 `acceptCapture/acceptPrefix/requestAdvance/requestControl/queryOutcome`；
D3 `consumeCommittedPrefix/samplePresentation`；D4 `prepareAudio/applyAudioControl/readObservation`；
D5 `readCapabilities/prepareCandidate/activateCandidate/present/retire`。
这些是职责入口，SDK 不隐式创建线程，不安装内部执行图；新公共批次/控制 facade 由 B3/B4
按相同合同提供 owning opaque 值，旧同步方法保留显式 legacy 行为，版本策略见 Spec §6。
公共时钟扩展采用单独可选连贯观察接口，不给旧 IAudioClock 虚函数增加纯虚义务；
旧 HostClock/第三方实现没有连贯观察时仍能用于旧路径，RPA 实时启动明确拒绝能力不足。

operationId/requestId 由各会话调用方单调生成；同 ID 同载荷是查询/重传，异载荷拒绝。
至多一个非终态控制操作；新控制在入口明确 ControlBusyNotAccepted，Close/紧急故障可进入清理路径。
被拒绝的新控制未被 D2 接入，不触发 K1。已开始的控制不能被新命令覆盖或靠超时推断取消。
结果保留到调用方确认，随后以 retired 连续前缀保证旧 ID 不再执行；跨 scope 重用旧 ID 拒绝。

D6 发出的 commandOrdinal 不赋予请求成功或取消效力；D2 的 decisionOrdinal 与 Spec §1.3
是唯一推进 Started/K1 接入顺序。D6 必须保留未获 D2 接入回执的请求供同 ID 查询/重传，
不能因本地超时把它当作未接纳并换 ID 重试。跨域回执携带原 ID、scope、ordinal 和阶段，
重复回执幂等；同 ID 异载荷及同 ordinal 异请求都拒绝并停止使用该通道的旧许可。
调用方明确确认终态后才可退休结果；阶段回执、传输确认和最终结果确认是三种不同事件。
旧 ID 的退休记录至少保留到相应 scope 结束，不能因结果对象释放而允许重执行。

## 2. 状态、错误与有界通道

宿主状态为 Running、ControlPreparing、Paused、ResumeStarting、Suspended、Closing、Closed。
Gameplay 的 Prepared/Running/Paused/Faulted 保持独立；Suspended 是禁止推进的宿主状态，
不冒充 kernel fault。实际 kernel/Fold fault 按原生命周期限制操作。

| 条件 | 设计码/typed 结局 | 状态与恢复 |
| --- | --- | --- |
| 普通推进占槽 | BusyNotAccepted | 不入 FIFO；不改旧请求/输入 |
| 有效控制串行接入 | CanceledNotExecuted（K1） | 只取消未开始推进；控制失败不复活 |
| 旧 scope/segment/代数 | `host.rpa.context_stale` | 拒绝；旧资源候选仅清理 |
| 同消息或操作 ID 异载荷 | `host.rpa.message_conflict` | 撤销许可，Suspended；不得选择任一载荷继续 |
| 连续交付尚有缺口 | Waiting，reason=InputPrefix | 保留输入、精确重传；不借超时跳过 |
| 时钟无效/超期/误差参数缺失 | `host.rpa.clock_invalid` | 撤销许可；显式新段恢复，不静默 fallback |
| 配置预算未测量 | `host.rpa.profile_incomplete` | 生产启动拒绝；testOnly 明示 |
| 可靠通道不能接纳 | `host.rpa.capacity_exceeded` | 未发送请求明确拒绝；已捕获无法保留则记录 gap 并 Suspended |
| PauseTrace 已确认不可补齐 | `host.rpa.capture_gap` | 拒绝直接续跑；保留结果，显式 Stop/reset/完整恢复 |
| 已接纳操作迟迟无设备回执 | `host.rpa.device_pending` | 报实际阶段；超时不是回滚或 Closed |
| renderer/audio 失败 | `host.rpa.device_failed`，cause 保留原码 | 按实际副作用收束；未生效步骤依B1取消；切段或软件提交后保留实际状态，不假回滚 |
| 内核/Playback 失败 | 沿原 error/sourceCode/first-fault | 保留实际 sealed/Fold 前缀；不包装为 Busy |
| 控制切段结局未知 | `host.rpa.control_pending` | 按 operationId 查询 D1；未确认前 Suspended，不当作未切段重试 |
| Resume最终切段后、R2 seal前准备确定失败 | `host.rpa.resume_aborted`，cause保留原码 | A1：Suspended并终止本次直接续跑资格；同/新ID的Resume均拒绝，允许显式退出/新会话恢复 |

host.rpa 码只属于宿主协议，不投影成 judgement.*，不增加 N3 九类别或 19 个拒绝面。
每个错误回执含 operation/request ID、context、stage、retryability 和原 cause；
前置校验顺序为载体完整性→context/ID→生命周期→数值/预算→语义准备→提交→设备回执。
分配失败必须能利用启动时保留的故障槽报告，不能再分配一份日志才表示失败。

输入/控制/Fact 是可靠通道；frame 是完整值合并通道；性能 trace 可按声明丢样。
容量在 SchedulingProfile 明示。输入通道满时先保留 D1 未确认记录，不能阻塞 callback/事件泵；
保留空间也耗尽则用预留故障槽标记捕获缺口并停推进，不声称无损捕获。
控制/故障保留独立通道，不能与输入共享最后一个槽而无法 Pause/Close。
观察回执丢失记 Unknown；不得反向阻塞 Gameplay。量产容量、等待上限在 B1/B3/F3 测量接受。

控制消息和故障回执各有预留容量。D1 的切段步骤也遵循下面 B1，Applied 的 payload 为
CutApplied(cut,q,pauseEpoch)；当前未执行为 Pending，不能用裸 NotApplied 充当取消成功。
超时/查询不到记录只表示未知，保持实际宿主状态和禁止推进要求，不授权撤销或重发新操作。

### 2.1 B1：owner 串行裁决 Apply 与 Cancel

步骤键为 `(sessionScope,operationId,participant,phase)`；controlSequence、expected generation
及原 Apply 载荷在首次有效请求时绑定，后续异值按 message_conflict 拒绝。Apply、Cancel 和
query 使用同一步骤键；Cancel 携带原 Apply 的不可变目标描述，即使先于 Apply 到达也可绑定
目标。Cancel 是针对该步骤的动词，不按“同 ID 异载荷”误拒绝。
query 只读；Cancel 必须在所属 owner 与 Apply 开始点串行裁决，D6 发送取消不产生终态效力。
音频启动、补偿停音频及 D1 切段是不同 phase；取消其中一个不能被解释为整个控制已回滚。

| owner步骤状态 | Cancel结果 | 原Apply/重传随后到达 |
| --- | --- | --- |
| 未见Apply或Pending，尚未开始 | 先在该scope的操作记录中保存CanceledNotApplied，再确认；禁止该步骤之后开始 | 返回原取消终态，不执行，即使这是第一次到达的Apply |
| InProgress，已开始且结果未定 | 返回InProgress与实际阶段，不能承诺取消成功 | 不再次启动；查询直到实际结局明确 |
| Applied | 返回Applied及副作用/代数；补偿是另一个显式步骤 | 返回原结果，不重复应用 |
| RejectedNotApplied或CanceledNotApplied | 重发原终态确认 | 拒绝执行，原终态不变 |
| Failed或Unknown，副作用尚不能确认 | 保留实际阶段并查询/清理；不转换为未生效终态 | 不重新启动或假报取消 |

RejectedNotApplied 表示 owner 已终结该步骤且未发生副作用，之后同样不会执行；Pending
仅表示尚未开始，不能代替终态拒绝。表中的记录是会话内拥有的记录，不要求新增磁盘持久化。
取消记录及终态回执使用预留控制/故障容量；不能保存终态时不得确认取消成功。
终态记录可按已有确认协议压缩为退休前缀，但该前缀必须拒绝旧步骤再次执行，不能只删除记录。

只有 owner 确认终态未生效，且相关在途引用/设备使用已结束或由独立 owning 值保活，才能
以取消为由回收对应候选；已Applied对象沿正常退休/补偿清理路径释放。一个参与域的取消回执
不授权回收其它域仍使用的对象。Apply已开始但回执丢失时，已知开始则保持InProgress，
不能确认则记Unknown并查询实际阶段，不改报Pending或以超时撤销。已软件提交的操作可按原
合同重试剩余设备阶段；A1终止的软件恢复不得据此继续提交。查询不可得时保持 Suspended/
Closing 及缺失责任域。Close和A1清理也必须遵守这些终态和寿命要求。
已确定终结且合同允许重试的设备失败使用同 operationId/controlOrdinal 下的新 phase 尝试，
phase 含 checked u64 尝试序号（初次为0）；旧步骤终态不变。InProgress/Unknown 不能由新尝试
覆盖；这项设备重试规则不适用于 A1 已终止的软件恢复、R2 seal 或已应用的 D1 cut。

## 3. Prepared 与资源提交

采用 P3-a：PreparedPlayback 仍仅在 Session owner 创建、使用和析构。
P-B 中 Session owner 是 Playback worker，GL renderer owner 是主线程。

1. 主线程读取 owning capabilities（含 renderer generation），交给 Playback worker。
2. Playback worker 调用 Prepared 的 validate/acquire，深拷贝 manifest，取得 owning portable resources，
   物化内部 PresentationCandidateInput：sessionScope、mutationGeneration、candidateId、content identity、
   renderer generation、capabilities identity、manifest、校验结果及资源集合。没有借用 span、provider 回调或 Prepared 指针。
3. 主线程消费该值构造 GPU candidate。创建/激活/销毁 GL 对象全部留在主线程；
   回执必须绑定 candidateId 及全部代数，不能只报告成功 bool。
4. D6 收齐 D2/D4/D5 prepared 回执后请求 commit fence；各 owner 确认停在可提交边界。
   D2 fence 期间禁止 submit/advance/update/override 等 mutation；新捕获暂存，不被丢弃。
   进入 fence 时重验 generation；此前新 mutation 使 candidate stale 则取消并重建，不阻塞正常活动去掩盖 stale。
5. D2 先 reserve 所有软件提交空间，发布新组合后给出 SoftwareApplied；其它域据同一 controlSequence 激活。
   GPU/audio 物理失败可能不可逆，进入 Suspended 并保留实际提交回执；不声称跨设备原子回滚。
   仅全域确认后解除 fence、切换输入上下文并发布新帧；旧帧不能越过此控制序号。
6. 提交前且未发生其它控制副作用时，按 B1 取得各 owner 终态未生效确认后取消候选，旧组合保持；
   未确认前不解除 fence 或提前回收，已切段的 Resume 失败按 Spec §3.5。旧资源活到最后一个 frame 读者和设备使用结束。
   CPU owning 值析构不能调用 GL/SDL；需要设备析构的资源由所属 owner 排队回收。

P3-a 不新增公共可共享 Prepared 类型，也不意味所有 RPA 公共 API 无变化。
CPU owning 候选由公开的 `PreparedPlayback::materializePresentation` 产生，renderer adapter
仅消费安装的 `PresentationCandidateInput` 值；不再包含 Playback 私有宿主头文件。保持 target
allowlist、无 backend 反向依赖。此同步物化入口不改变 Prepared 的 owner 或 GPU 线程约束。
现有 renderer.prepare(Prepared&) 仍作为同 owner 兼容入口；跨 owner 仅传 owning 候选，
不能把物化入口理解为 Prepared 可跨线程使用。

音频准备也必须闭合寿命：在 Prepared owner 内从 MainMusicSourceView 取得/复制 owning 编码数据或解码产物，
然后把不含借用的音频候选交给 audio owner 创建/管理 ClipStore 与设备句柄。
不能只因当前主线程立即 decode 的路径可用，就推断迁到 audio worker 无寿命工作。
候选、活动 clip、callback 引用和 store 的释放顺序须以同一 content/control generation 审核。

## 4. 表现、反馈与日志

D2 发布完整成功 Fold 前缀，D3 自己维护 FactBinding cursor、dedup、future queue、aggregation、tokens。
表现失败不回滚 Gameplay，也不能重新 submit 已准入输入。T 按声明时钟独立采样，
不把 watermark/最后 Fact 当 T；同段 T 回退须显式恢复，不普通 clamp。
帧仅在 scope、已应用控制序号和资源代数全部匹配时交给 renderer。

采用 F1 + F3 可观测，内部回执通道不新增“保证用户看见”的公共义务。
feedbackId=`(projectionScope,FactId,targetId,bindingId,activationOrdinal)`；聚合结果记录实际成员集合，
frame 保存实际参与提交的 feedback membership。Succeeded 只表示 present 调用成功。
每个反馈记录 created/expiry、ConfirmedPresent、ExpiredUnconfirmed、CanceledByControl 或 UnknownEvidence；
到期时的状态与后来到达的确认分别记录，不把迟到回执倒填成“到期时已确认”。
旧 scope 的迟到回执只更新对应历史诊断，不复活效果或污染新 scope 计数；重复 frame receipt 幂等。
若记录预算耗尽/回执丢失则计 Unknown 和缺失数，不能把它计为确定未呈现。
RemainingFrames 仍按成功 Runtime 更新消耗，不按 present 次数、不因等待回执续命。

执行日志采用分块追加、immutable prefix 共享，当前可变状态仍由唯一 owner 维护。
owning query/result 独立于 Session/World/设备；旧 handle 可以跨 Close 保留。
checkpoint 键含 archive/cut/H/profile identity；活跃尾块复制/释放与历史增长分别测量，
不能只把全量复制搬到另一个线程。实时循环不做每帧完整 ReplayEvaluation；
停止后或显式验证使用独立 owning archive 运行完整 evaluator，不在活动 owner 上并发调用。

## 5. 控制退出与关闭

Pause/Resume 的 drain、R2 及错误路径由补充 Spec §3 唯一定义。
A1失败后暂停音频属于清理，不恢复直接续跑资格；失败回执与清理完成分别可查，
清理未结束前仅允许query及串行接入的显式退出/清理命令，不把普通控制槽空闲当作可再次Resume。
Seek/Stop/reset/reload 不借 R2 更改既有目标语义：Stop/reset 明确放弃活动游戏，归档保留，
future pending 只随明确替换清除；Seek 明确选择 archive/cut/H；reload 使用新内容身份。
只换表现重建绑定/资源与 projection scope，不换判定四分量。旧回执不可解除新控制的 fence。

Close 的有向顺序：停止新请求并应用 K1→固定可取得归档/取消候选→停音频并确认 callback 隔离
→各 owner 清理活动对象→主线程完成 GPU/在途资源退休→join 已无主线程依赖的 worker
→释放协调器并报告 Closed。主线程在 join 前持续处理需要它执行的清理任务。
任一步未确认则保留 Closing 与缺失责任域；超时不 detach、不释放仍被访问内存、不假报 Closed。
外部 CPU owning 结果不需要被全部销毁才可 Close；其最终析构不得依赖已关闭的 worker。

## 6. 验证接口

B1 fake clock/可控交付 fixture 走同一协议；时间 oracle 用独立有理数模型，
T2/R2 oracle 用扫描式 reducer，不调用生产调度/advance 充当预期。
同 canonical/admission/control 轨迹比较完整 kernel/Fold/Replay，刻意改封窗轨迹则验证不同结果或稳定拒绝。
真实 DFJK、听音、focus、设备切换和主线程阻塞仍须 B5 实测，文档与 fixture 不能代替。


B4 已增加非安装的 `host_protocol`、`host_candidate`、`capture_route`、`audio_owner`、
`renderer_owner` 和 `frame_observation` 实施入口；同线程显式驱动，不创建线程。
生产源码身份、实际调用测试和验证边界见
[B4 实施记录](../stage_reports/stages/stage-07/rpa/2026-10-12-b4-host-and-resources.md)。
这里的接口实施不替代 B5 的设备、callback 隔离和线程部署验收。

B5 应用装配位于 Player 的 `execution_host` / `player_execution`，AudioWorker 与
SessionWorker 为非安装的 owner 实施入口；SDK facade 保持显式单 owner，不自动创建线程。
主机以原 job ID 查询在途回执，控制等待期间泵主线程事件。D1 为关闭批和 EpochEnd
另留两个真实消息槽；故障 Close 导出未确认输入、tentative Tick 成员和实际 fault，
不制造 ACK 或 canonical admission。
显式 EOF tail 在同一 kernel/run 内换 clock segment。暂停/恢复动作使用同域单调时间锚点，
worker 采用 source 动作时间，不采用回执到达时间；新 run 的 terminal H 至少覆盖实际
Pause control drain，原输入截止 Tick 保持不变。这是 synthetic tail 的控制状态，
不是修改物理音频位置。配置不能在终点证明 `F(H)<B` 时拒绝启用尾段。
Player 的候选入口为 `--execution-test-only`；EOF 要求显式
`--execution-tail-input-deadline`。当前 A 来自 app profile output correction，I/V=0、rate=1，
calibration 不匹配则拒绝。主音乐使用显式 Audio clock，Playback 内部由 HostClock
接收 owner 供给的 RuntimeFrame。全部容量/误差/有效期为 testOnly，不能据此接受生产参数。
实施、测量和自动化边界见 [B5 记录](../stage_reports/stages/stage-07/rpa/2026-10-13-worker-deployment.md)。

## 7. DLL 与内置宿主实施边界（2026-10-14 修复）

按 owner 选择的方案 A，`app/execution_host/` 为非安装的静态宿主支持模块，拥有 worker、
消息队列、capture/audio/renderer owner、步骤 ledger、表现回执 ledger 与组合控制协调器。
这些类不使用 Playback 导出宏，SDK 不安装它们，也不负责创建宿主线程。

Playback 保留软件会话的唯一 accepted control、generation、fence 和执行状态。
`PlaybackSession` 的 candidate 同步能力分为：owner 状态/软件副作用查询、控制接受与绑定验证、
fence/软件提交与完成确认、受控退休/Pause/Resume/Seek、时钟映射/撤销、Replay cut、
表现采样与反馈成员读取。它们不接收宿主 ledger、worker 或协调器指针；调用、对象析构
仍在 Session owner。宿主步骤的终态重传、Apply/Cancel 串行裁决、容量预留和回收由
宿主 ledger 负责，调用原始 SDK mutation 不提供跨设备组合控制的 exactly-once 保证。

[`execution_control.hpp`](../../engine/playback/include/cuexis/playback/execution_control.hpp)
只在 candidate SDK 安装，声明 owning 请求/回执值、可复制但不可由调用方构造有效值的
`ExecutionFence`、owner 状态副本及纯 proof 检查。副本不是第二份可变会话状态。
Resume 传最终 route 回执并校验同一 trace identity；Seek 校验 owning archive identity 与 cut；
软件清理仍需 capture cut、audio stop/isolation 证据。scope、代数和操作绑定不得省略。

通用 [`presentation_candidate.hpp`](../../engine/playback/include/cuexis/playback/presentation_candidate.hpp)
也用于旧同步 renderer prepare，拥有 manifest、portable resources 和编码音频副本，不拥有
Prepared、provider 回调或设备句柄。其物化使用原有 acquire 路径，没有新增深拷贝资源策略。

candidate OFF 不构建宿主支持模块、不安装 execution control，不保存 RPA 控制/fence 状态。
ADR 0033 禁止私有导出的规则保留；共享库白名单仅登记实际公共类型/函数，不能放行 detail。
上述接口仍属 SDK 0.8.0 preview；修复不接受生产 profile、设备验收或 Stage 7 关闭。

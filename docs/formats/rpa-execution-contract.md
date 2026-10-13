# RPA 执行与控制补充合同

状态：candidate contract；SDK 0.8.0的B3执行协调实施中，未发行

更新日期：2026-10-11

本文补充 [Gameplay V2 Spec](GAMEPLAY_V2_SPEC.md) 和 [execution profile](gameplay-v2-execution-profile.md)。
只在下述明确列出的 RPA profile 中替代旧条款；SDK 0.7.5 的实现行为仍见
[CURRENT_STATUS](../CURRENT_STATUS.md)。选择理由归 [ADR 0046](../adr/0046-realtime-playback-coordination.md)，
载体/所有权归 [实时宿主边界](../api/realtime-host-boundary.md)，实施门禁归
[RPA 计划](../stage_plans/active/stage-07/plan-rpa.md)。术语见 [Glossary](../api/GLOSSARY.md)。

授权来源：owner 直接选择 C1、T2、O1、I1、T2-C1、T2-L1、G1、Q1、K1、TB-05 和 R2；
2026-10-11 随后授权其余事项由代理合理裁定，仅极难决定者再询问。
本文的控制切点、表示、版本和失败策略是该授权下的设计决定，不冒充更早的 owner 逐项选择。
生产预算、设备证据及最终关闭仍按原门禁，不由纸面裁定替代。
后续 owner 明确选择本轮审查修补 A1（恢复切段后准备失败终止直接续跑资格）和 B1（owner
串行裁决 Apply/Cancel）；这里的 A1/B1 是修补方案编号，不是计划批次 B1 或尾段 A1-b。

## 1. 前缀证据与请求

### 1.1 有限证据载体

一次正常运行捕获区间称 run epoch。D1 是该区间唯一捕获聚合者；所有来源必须先汇入 D1。
区间内 deliverySequence:u64 从 0 连续递增，覆盖所有成员、关批、空区间和控制 cut 记录；
计数耗尽显式失败，不回绕。它只证明交付覆盖，不进入 I1 或判定身份。

`ClosedCaptureBatch` 包含 sessionScope、clockSegment、runEpoch、originalTick、
每来源的完整有序成员及成员数、首末 deliverySequence、关批证据。
I1 位置由完整 originalTick/source 序列派生，局部位置是 u64；向 D2 转交 owning 原值。
正常关批证据为同段有效校准观察 `observedTick > originalTick`。
分片传输必须先重组、核对每来源数量及位置连续性，才可整体准入；同片重复可确认而不重交 kernel。
相同传输序号而内容不同是协议故障，不把冲突成员中的任一个当作有效版本。

`CapturePrefix` 包含同一上下文、截止 deliverySequence、exclusiveTick B 和对应有效观察。
D1 声明该 epoch 中原始 t<B 的全部捕获均已在该序列前缀完整列出；空区间用同一声明覆盖。
D2 只有在序列从起点连续、所有相关批已完整重组、每批 admission 或稳定拒绝已有结局时才确认 B。
越序到达只能暂存；缺口不能由最高序号、队列为空、digest 相等或超时填补。
成员已稳定拒绝也计入“已处理”，但不得把未知丢失当作拒绝；协议破损不推进 B。

确认回执含连续已处理序号与已核验 B。D1 在确认前保留原记录以便精确重传；确认后可释放传输副本。
D2 以累计确认序号拒绝旧重传重新执行，不需永久保留每份传输副本；消息通道须保证字节完整性，
已确认旧序号的重传只返回累计确认，不作为新内容接受。应用新请求即使内容相同仍走 I1 重复检查。
session/segment/epoch 更换后旧证明不能授权新推进。控制期间可能保留旧段已准入的 future pending，
它们的原身份和 admission 记录不因证据失效被删除。

### 1.2 G1、Q1、K1

普通推进只在 owner 串行边界执行：`F(H)=checked(H-C)` 且 `F(H)<B`。
H、C 和 B 使用同一 Tick 域。等待保持原 H；仅一个等待槽，新请求返回 BusyNotAccepted。
不同 requestId 即使 H 相等也不是同一请求；同 requestId 同载荷重传返回原状态，内容不同拒绝。
请求结果确认后旧 ID 只返回 Retired，不重执行；调用方丢失原结果时必须 query，不能按成功推断。

**K1：** 有效 Pause/Seek/Stop/reload/Close 在 D2 串行接入处理时，先将尚未开始的普通推进
终结为 CanceledNotExecuted，再准备控制。基本字段/作用域校验失败的控制不取消它。
控制准备失败也不复活已取消请求；若仍允许 Running，调用方重新提交新 ID。
已开始的推进完成或按原 fault/partial-prefix 规则结束后才能处理控制，不能撤销它的已 seal 结果。
输入与证明继续得到处理机会。取消不取消输入、不代表 Pause 成功、不伪造 kernel advance。

### 1.3 跨域裁决顺序

D6 为同一 session 的普通推进与显式控制分配连续的 commandOrdinal；D2 以接收后的单一
decisionOrdinal 记录每次请求接入、推进开始、控制接入、前缀确认和终态。两种 ordinal 都是
checked u64，分别表示发送顺序与 D2 实际裁决顺序；D6 已排队、传输已发送或收到 D6 回执
都不等于 D2 接入。D2 不能因等待 G1 前缀占住串行处理循环，仍须处理输入证据和控制。

推进的执行线性化点是 D2 在同一裁决中重验 context、clock、`F(H)<B` 并标记 Started；
K1 的线性化点是 D2 接入有效控制并先终结尚未 Started 的等待请求。两者只有一个先发生。
前缀确认与控制并发到达时按 D2 实际 decisionOrdinal 裁决：先 Started 则保留该推进的实际
seal/partial 结果，再处理控制；先接入控制则取消等待推进，之后到达的 B 不能复活它。
不得用 D6 发送时刻、队列入队时刻或事后设备回执重排历史。操作日志保存两种 ordinal、
接入时的 B/clock 身份、线性化结果与实际 kernel 调用；Replay 只记录真正执行的部分。
同 ID 重传或 query 只返回原阶段/终态，不分配新 ordinal 或重复线性化。

## 2. 时间映射、offset 与音频观察

### 2.1 算术与字段

RPA 使用 `engine.tick.us.v1`。先在 checked 有理数域完成换算和 offset 相加，输出整数 Tick 时
仅作一次 round-half-to-even；负值对称、负 Tick 合法。中间与结果溢出均原子失败，不截断或饱和。
Beat→Tick 继续现有 TimingMap 数学；本节只定义宿主时钟→会话 Tick，不从 RuntimeFrame double 反推。

设 P 为有效 presented 源位置换算成微秒后的有理数，A 为音轨 timingOffset，
I 为输入延迟补偿，V 为显示提前量。字段与正号定义如下：

| 字段 | 单位/域与正号 | 唯一应用处 | 负值与检查 |
| --- | --- | --- | --- |
| sourceFrame/sourceRate | 源采样帧/源帧每秒，rate>0 | 源位置→微秒；绝不用设备 rate 除源帧 | 帧位置按 source 有效范围验证，乘除 checked |
| deviceMixedFrame/deviceRate | 设备混音帧/设备帧每秒 | 仅用于估计 presented，不直接作 H | 单独域与范围验证 |
| outputLatency | 非负微秒估计 | 仅 adapter 构造 presented 时扣一次 | 缺失或无界误差标 invalid，不填 0 |
| timingOffset A | 有符号微秒；正值使谱面比 source 时间晚开始 | `S=P-A`；与现行 `chartTimeMs=positionMs-timingOffsetMs` 同号 | 负值允许，checked |
| inputCompensation I | 有符号微秒；正值把捕获的输入提前 | `observationTick=RHE(S-I)`；B 与 cut q 也用此域 | 固定 session 配置，负值允许，checked |
| displayLead V | 有符号微秒；正值让表现时间向前 | `T=RHE(S+V)`，只进表现 | 不进入输入、H 或 Fact；负值允许，checked |
| H | 会话目标 Tick | `RHE(S)`；G1 因 I 与交付前缀差异等待是合法结果 | 不按帧数递增，不等同 F(H) |

A 的内容来源保留现有声明；I 是基本固定补偿，不新增 S7C-1 自适应/历史重建能力。
A/I 的实际值和映射 revision 进入既有 session 校准身份；V 属表现配置。
host monotonic 模式用显式锚点构造同一个 S；暂停 rebase 时剔除暂停 wall time。
播放速率只支持当前声明的固定速率，不新增动态变速。浮点旧 API 仅在兼容路径保留，
RPA 时钟桥的配置以明确单位的有理数值输入；不得从每帧 double 累加恢复精度。
若 presented 已扣除 outputLatency，桥不再次扣减；若源端只能给 consumed，则必须显式给估计模型，
不能把 consumed 换名为 presented。441/44100 秒与设备 480/48000 秒均为 10ms，但帧号不可互换。

### 2.2 连贯观察与失效

新增设计 `ClockObservation` 为 owning 值：clockDomain、segment、publicationSequence:u64、
sampleMonotonicUs:i64、source/presented/device 各自帧率与位置、播放状态、quality、
validUntilMonotonicUs、errorBoundUs，以及同一发布点的 underrun/device/rate 状态。
可选帧字段用 optional，不能伪造精确游标。误差上界须非负且包含估计/采样偏差。
有限源可额外提供 `maximumPresented` 真实末帧和同一帧率。Playing 时间外推在这个源端点
停止，EOF 仍由设备状态明确报告，再按既定合同进入 tail；缺省时外推合同保持不变。
此端点不是 Judgement horizon 上限、Hold 截断或任意旧 H 的 clamp。
所有字段必须同一次连贯发布；adapter 在实际获取样本时打时间戳。
旧 seqlock 缓存仍保留原时间戳和序号；接收时刻只作运输诊断，不能刷新其有效期。

同一 clockDomain 的单调时间才可比较；跨域必须提供经过验证的有界转换，否则拒绝用于实时推进。
在声明有效区间内按锚点和固定速率推算 S，误差上界须覆盖整个区间；超时、未知质量、
欠载导致源停滞、设备/rate 变化立即撤销许可，取消尚未执行请求为 ClockInvalidNotExecuted。
已执行 H 不回退，不把静音设备帧算作歌曲进度，不自动切到 HostClock。
进入可观察的 Suspended 状态，保持可取得的结果；恢复须显式控制、新 segment 和重新取得的有效观察。
focus/输入历史缺口按 §3.4 处理。正常 EOF 仅能走 §7 的显式尾段路径。

有效期和允许误差是 SchedulingProfile 的 MeasuredParameter；没有可用参数不得启动生产实时模式。
testOnly fixture 可显式供值，不能据此声称测量通过。计划选用 additive 的连贯观察接口，
保留旧 AudioClockSnapshot 的含义与同步兼容路径；未来公共头差异与 minor 版本门禁见 §6。

## 3. Pause、R2 与恢复

### 3.1 显式控制 cut

正常 T2-C1 仍以严格后续观察关批。Pause 为不能自然关掉最后一批的问题增加**唯一显式控制例外**：
D1 在同一个捕获串行边界结束当前 run epoch，记录最后有效校准 Tick q、已捕获序号 cut、
完整末批和 `EndRunEpoch(q,cut)`。它承诺该 epoch 不再产生普通输入。
q 必须不小于该 epoch 已捕获最大 Tick；q 不是 wall time、音频停点或最后 Fact Tick。
后续物理转换进入有序 PauseTrace，包含来源/动作/捕获序号、pauseEpoch 和真实观测 provenance。
它们没有普通输入的命中资格。末批不因队列暂空而关闭，也没有虚构 q+1 的观察。

Pause 顺序为 D2 接入有效控制并应用 K1 → D6 请求 D1 切段 → D1 在捕获边界一次性
封存末批/cut 并切到 PauseTrace 路由 → D2 校验并 drain → audio owner 确认停点。
D1 回执必须区分 Pending（尚未执行）、InProgress、终态 RejectedNotApplied/
CanceledNotApplied、CutApplied(cut,q,pauseEpoch) 与 Unknown；只有终态未生效确认
且其他域未有不可逆效果时，控制准备拒绝才允许保留原 Running 组合。终态按宿主 B1
拒绝该切段步骤的迟到 Apply；当前尚未执行或查询不到记录都不能授权返回 Running。CutApplied 后即使
传输/校验/音频失败也不能撤销切段，进入 Suspended 或原 fault 状态，保持实际日志和可查询
的 cut；Unknown 不得猜测未切段，应先按 operationId 查询 D1，无法确认则停在 Suspended。
新捕获在路由切换后都入 PauseTrace，不能因 D2 尚未收到 cut 回执落入旧普通批。

D2 收齐至 cut 的所有记录并确定 admission/拒绝结局，核对终止证书后得到 **inclusive q** 覆盖。
这不是普通 G1 的 B，二者必须使用不同 tagged 类型。控制以 `Hcut=checked(q+C)` 处理到 q，
使用原 exact-H 内核规则；先核对单调性/可表示性和生命周期。若本 epoch 曾执行普通推进，
其最后一次许可使用的已核验 exclusive B 必须由同段、同 epoch 的真实观察支持，终止 q 不小于该 B；
于是已有 processed frontier `<B<=q`，固定 C 下 Hcut 严格大于该次成功 horizon。
若观察倒退、B 的上下文不符、实际已 seal 前沿已超过 q，或 Hcut 小于已有成功 horizon，
控制准备失败并保留实际前缀；不能降低 H、重开 Tick 或用较大的虚构 q 补证据。
K1 已取消的旧请求不复活，
Hcut 是归档中明确的 ControlDrain，不是替换/夹紧旧 H 的普通推进。
恢复后若真实时钟目标 H 尚小于已记录 Hcut，调度器等待真实时钟追到可单调请求的位置；
不把 Hcut 当作新的源时钟锚点，也不因此重复执行已 seal 的 Tick。
封存到 q 的普通 timer/input 顺序仍为 T2；future dispatch>q 保留。
若原始 t<=q 因 late-policy 已转发至 q 之后，它仍是 future pending；不得提前执行或重编号。

当 q+C 溢出、时钟无效、交付缺口或 control drain 实际失败时，不伪造成功 Pause；
保留可取得的 sealed prefix，进入 Suspended/Faulted 并报告具体阶段。不能用加一、饱和或超时绕过。
尚未产生新 seal 且恢复 Running 的全部条件仍成立时可拒绝准备、保持旧组合；
一旦 drain 发布了新前缀，不再声称回到了接入前状态。其新前缀是实际历史。

音频 Pause 在其 owner 执行，回执给出实际音频停点/segment；软件 q 与物理停点分别保存。
组合 Paused 只有软件 cut/drain、输入路由切换和音频停住三者已确认才成立；无音频模式显式标 NotApplicable。
主线程持续泵事件/回执，worker 不同步等主线程。暂停后保持最后成功画面，普通 submit/advance/update 禁止。

### 3.2 Resume 的 R2 事务

Resume 首先固定一个连续 PauseTrace 前缀和唯一 controlOrdinal:u64（从 0 连续、checked）。
以 cut 后执行状态及 trace 建立候选，按来源及来源内真实顺序处理；绝不只比较最终键态。
重复传输不重复应用。候选和发布所需日志/Fact/Fold 空间先 reserve；只有最终捕获路由
尚未切换、其切换步骤已终态拒绝/取消且音频已确认停住（或 NotApplicable）时，准备拒绝才
可保持 Paused。音频已进入 ResumeStarting 时须先确认重新停住；未知阶段不能冒充 Paused。
切换后准备失败采用 §3.5 的 A1，不回到旧 Paused，也不重试完成该恢复的软件提交。

对已建立 head contact 的 Hold，只要 trace 中存在该 contact 的真实 release，就标 interrupted。
松开后按回不恢复旧 contact；一直按住且 trace 完整则保持。暂停 press 只更新物理对齐状态，
不能创建 gameplay contact/head/tap。恢复时仍按住但无 gameplay contact 的键进入 quarantine，
直到真实 release 后才允许下一次 press 建立新 contact；重复 keydown 不冒充新 press。

对 cut 前已准入但 dispatch>q 的 future input，保留其原值和 admission。
R2按cut前完整来源序列确定暂停开始时的物理contact，关联其实际start press occurrence，
不能把该来源所有历史/未来press都标成中断。记录该occurrence的暂停中断关联：若它日后建立的contact在暂停trace中已release，
则在该 occurrence 的输入 reducer 后立即执行依赖的 R2 中断 reducer，禁止该 contact 活到后续 body。
该动作是同一 TickDraft 的 control consequence，不是插入一次 timer；原输入本身的合法 Hit 不回滚。
这避免 future press 在恢复后生成永久 held，同时不抹去真实的 cut 前输入。

| cut/待执行状态与暂停轨迹 | 唯一恢复结果 |
| --- | --- |
| body Pending，真实 release | body Miss；声明的 tail 若 Pending 也 Miss；结束 contact/lease |
| body 已 Hit、tail Pending，真实 release | body 保留 Hit，tail Miss；不因暂停点在 tail window 内得到 Hit |
| phase 已 Hit/Miss | 保留终态，不再发该 phase 的 Fact |
| 无显式 tail 的 Hold，真实 release | 只处理 Pending body，不创建 tail |
| release 后 press 回原键态 | 旧 Hold 仍被中断；新按住进入 quarantine |
| 暂停中新键 press/release | 只更新恢复物理状态，不发命中或 stray/consumeEmpty Fact |
| 完整 trace 无相关转换 | contact/lease/phase 不变 |
| 连续两次成功 Pause/Resume、重复 Resume | 每次新 cut/ordinal；重复查询/传输不再发布；A1失败后禁止直接续跑 |

### 3.3 R2 Fact 和顺序

运行期普通 timer/input 已完整处理到 q 后，Resume 在 q 追加一个独立、原子的 **control commit**。
不重开 q 的普通 Tick，不重新执行 q 的 timer/input。只有 R2 允许追加此控制后缀；
新增 originKind `reconciliation` rank=4（旧 correction=3 仍禁止）。
originScope=`(q,controlOrdinal)`；相同 origin 内按既有 phase/factKind/E 顺序生成 localOrdinal。
commitTick=q，origin evidence 为完整 PauseTrace cut 及触发 release 的来源位置；普通 observation evidence absent。
依赖 future occurrence 的控制后果则在它的 dispatch TickDraft 中生成 rank=4 origin，
scope=`(dispatchTick,controlOrdinal,occurrenceIdentity)`，无 immediate 与 deferred 二义性。
同一 Tick 的 reconciliation origins 按 controlOrdinal、tag（immediate=0,deferred=1）、occurrence 结构序排列。

失败 Fact 的 outcome=Miss，category 从 phase 派生，error=absent、grade=absent；
暂停没有合法 phase observation，不能写 error=0 或伪造观测。Ruleset 按既有 Miss/absent-grade 分支 Fold。
状态/Fact/control-applied cursor 原子 seal；随后 Fold 沿既有独立失败边界。
Fold 失败保留 sealed Fact 并 fault，不再次提交恢复；失败前缀与最新回执可查询。
control commit 必须在 q 的普通 Tick seal/Fold 完成后、任何 `>q` 普通 Tick 开始前发布；
这是同一逻辑 Tick 的可寻址后缀，不改变 processed frontier、成功 advance horizon 或普通 Tick 的
timer/input 已执行标记。rank=4 及追加时序保证 Ledger 顺序不回退；重复 controlOrdinal 不再追加。
Fold 从旧 factCursor 仅消费新增控制 Fact，更新计分、combo 和统计，workTick 仍为 q；
这是 RPA profile 对旧「每工作 Tick 一次最终写」的受限续提交例外；不得重跑 q 的普通 Fact、
due/bonus 消费或其它 Tick 一次性 reducer。Hook 以保存的 q 起点 committed Fold view
评价新增贡献，并与 q 已有贡献规范合成；同一 `(q,target)` 仍只保留一个 q+1 due 输出，
不能因为后缀追加第二个输出或读取第一次 q Fold 后的 view 改变结果。
Fold 成功必须同时推进 Fact/control cursor；无新 Fact 的控制后缀也须推进 control cursor，
不得按“同 Tick 无新工作”跳过。Fold 失败时旧 Fold、旧 due 和 cursor 原样保留；
活动会话不能再次 seal 同一 control commit；显式归档 Replay/恢复按 controlOrdinal 恰好重建一次。
成功后更新 mutation generation；保留 Ledger scope，但发布的新控制序号使旧画面不能冒充恢复后帧。

新 epoch 普通捕获只在实际有效观察严格大于 q 后开放；在此之前的真实转换仍归 PauseTrace。
恢复握手以最终 trace cut 封存候选并原子切换捕获路由，切换后转换是新 epoch 普通输入。
音频可先进入 ResumeStarting 以取得新观察，但未收到音频/软件完成回执不报告 Running；
最终 trace 前缀未齐时继续等待，不能用 q+1 伪造首个输入。
初始候选只是预备值；最终路由切换 cut 确定后须增量纳入其余 trace 并重新校验/reserve，
只发布该最终候选一次。切换后普通输入可暂存，但在恢复提交前不执行。
上述切换后校验/reserve 的确定失败执行 §3.5；前缀尚未交齐或回执未知按原操作等待/查询，
不得把运输延迟当作失败，也不得为重新准备而建立另一个最终 cut。
达到 Tick 极值无法取得更大观察时，直接续跑拒绝，保留归档并允许明确 Stop/reset/新会话。

### 3.4 不完整轨迹与设备失败

focus 丢失、捕获溢出、trace 序号缺口均不能证明“没有 release”。
暂时未交付可等待/精确重传；已确认无法恢复的缺口不允许直接 Resume，不猜测所有键松开或全保持。
保留暂停结果，可显式 Stop/reset 或用完整 archive 恢复到新的合法会话；不自动清空成绩。
软件恢复提交后音频失败，回执标明 SoftwareApplied/AudioFailed，进入 Suspended；
已发 R2 Fact 保留。重试设备阶段绑定同 controlOrdinal，不再发一次 R2 Miss。
设备换段不能把旧 B、旧观察、旧回执当作新段许可。

### 3.5 A1：恢复切段后准备失败

本节覆盖最终 trace cut/新 epoch 路由已生效，而 R2 尚未 seal 时的确定校验、增量 reserve
或软件提交前准备失败。软件已经 seal 的 Fold fault、软件成功后的设备失败仍归 §3.3/3.4，
不能按本节重做 R2。切换是否生效未知时先按原操作查询；未查明前禁止普通执行和新的 Resume。

1. 进入宿主 Suspended，终结本次恢复的软件续执行资格；内核保持实际尚未恢复状态，
   不产生假的 control commit/成功回执。原 operationId/controlOrdinal 与失败阶段可查询；
   重传原请求只返回已有结局，新 ID 的 Resume 同样拒绝，不能重切 epoch 或补做软件提交。
2. 保留最终 cut、完整已取得 trace、新 epoch 暂存输入的原 Tick/身份、实际 archive/Fold 前缀。
   新 epoch 的输入不重新解释为 PauseTrace，也不在失败会话准入/执行。D1 撤销普通捕获许可，
   保留已捕获未交付数据；后续转换仅保留可用 provenance 供诊断，不保证当前局可连续重演。
   不再具有有效时钟时不生成 canonical Tick；容量不足沿预留故障槽登记缺口，不无限积压。
3. 请求 audio owner 停音频并确认实际停点，同时按宿主 B1 终结尚未开始的相关启动/切换步骤。
   已开始步骤按实际结局收束，不能取消一个已生效的 cut。停音频失败或无回执时保持
   Suspended 和待清理责任域；即使最终停住，也不把本次失败恢复改报为 Paused/Running。
4. 允许显式 Stop/reset、按完整有效 archive 恢复到新会话或 Close；须先完成相关 owner 的
   终态隔离和资源清理，不能借退出命令覆盖仍在途的 Apply。faulted 时沿原生命周期限制，
   Stop 不成为绕过 fault 的入口。未准入输入留作操作记录，不伪造其为 archive 成功历史；
   新会话按独立合法边界建立，不能用旧操作或旧段许可恢复这次半完成事务。

失败标记归宿主操作记录，记录实际 cut、软件未提交事实与清理阶段；捕获到输入不等于已准入。
Replay 仅消费归档中实际已准入/已提交历史，不重演设备清理或把 A1 失败当作已完成 R2。
该策略接受极少数准备故障后无法原地继续当前局的代价，首版不提供半完成恢复的向前重试。

## 4. T2 Fact identity、Replay 与 Seek

T2 observation/coordination origin 把旧 CanonicalObservationKey 引用替换为完整
`OccurrenceIdentity=(originalTick,sourceKey,sourceLocalPosition,normalizedContent)`。
slot 冲突比较不含 content 的三个定位分量（sourceKey 是一个结构分量）；同 slot 同值是重复，
同 slot 异值是冲突。contact startPress、candidate origin、receipt、snapshot pending 和 Replay 全部保留该身份。
处理顺序继续 O1/T2-L1；Fact origin 的结构比较按上述元组，不能用处理数组下标或 hash 代替。
Fact 唯一总序仍是 `(commitTick,originKindPriority,canonicalOrdinal)`；加入 §3.3 控制后缀。
普通 origin 的 originOrdinal=0，其余 localOrdinal/commitId/factId 规则沿 execution profile §7.2。

Replay 记录实际 admission（含拒绝）、普通 advance 实际调用及 partial result、ControlDrain、
Pause/Resume cut、连续 trace、R2 immediate/deferred 应用、controlOrdinal 和结果。
等待/Busy/取消普通请求仅进宿主操作日志，不伪造 kernel advance。
设备/present 回执不参与判定重演；其影响先显式转为归档中的控制决定。
完整 Replay 以存档 canonical 输入与控制轨迹为输入，不要求接上设备或用主机当前时钟。

Snapshot 增加 run/pause 的逻辑状态、R2 applied cursor、deferred occurrence associations、
quarantine 和必要 trace 前缀；运行线程/指针/原设备句柄不入 payload。
Seek identity 必须同时指定 archive identity、log cut 与 H；同 H、不同 control cut 是不同状态。
恢复前后同 q 的快照不能按 H 命中同一个缓存。旧 archive 不原地改写，回退程序保留原件。

## 5. 新错误与状态的边界

宿主调度结果是 typed variant，不新增 kernel SessionState：
Waiting、BusyNotAccepted、CanceledNotExecuted、ClockInvalidNotExecuted、Retired、
Executed(receipt)、Failed(stage,error,actualPrefix)。前五项及 Retired 不伪造 Gameplay fault。
协议等待本身不是错误；错误码与阶段见 [宿主边界](../api/realtime-host-boundary.md)。

内核新增 `judgement.input.occurrence_conflict`（invalid_relation、error、faulted=false）用于 I1 同槽异值；
同发生重复沿 `judgement.late.duplicate_queue_entry`（late_policy_incomplete）；
新控制声明非法为 `judgement.control.reconciliation_invalid`（invalid_relation、error、faulted=false）。
已开始 seal/Fold 的 invariant/分配/算术失败沿既有实际 fault stage 与 first-fault 策略，
不能复用前置拒绝码掩盖 partial prefix。公开投影继续按既有类别映射，sourceCode 保留具体原因；
九类别和 19 个拒绝面不因调度状态新增而增加。具体映射新增项必须纳入 N3 增量复验。
旧 same_tick_collision/dispatch_collision 只服务旧 profile，不在 T2 合法同 Tick 批上产生。

## 6. 版本与兼容门禁

| 轴 | RPA 设计选择 | 兼容策略 |
| --- | --- | --- |
| execution token | `gameplay.execution.t4-k4.rpa.v1` | 显式选择；旧 token 不偷换行为 |
| late token | `late.window.batch.v1` | 表示 I1/T2-L1，实际 late 参数仍显式并进 session identity |
| fact semantic | `fact.semantic.rpa.v1` | 新 occurrence origin 与 reconciliation rank 进 engine identity |
| state schema / event codec | schema `2` / `canonical.discrete.rpa.le.v1` | 当前 schema1 / canonical.discrete.le.v1 不承载新 occurrence/control；旧 snapshot 不冒充新状态 |
| Recovery envelope | revision 3 | RPA reader/writer 只接纳 rev3；rev1/2 稳定 version_unsupported，原件保留，无隐式升级 |
| SDK API | SDK 0.8.0（下一 minor） | 新连贯时钟/批次输入及控制能力的公共差异需要 minor；B3公共增量已按此门禁更新版本 |

RPA profile 与旧 engine identity 不相等，即使某条轨迹得到相同分数也不能混合 archive。
未来实现保留显式 legacy 同步路径；新 Player 用显式 RPA 配置，不能让旧入口默认启用新语义。
不改 Chart/CXC/CXT 内容字段；现有 profile token 引用位置承载新 token，Reader/Writer registry 与 identity 定向回归。
rev3 在现有 envelope/section 编码规则下增加 typed occurrence/control 内容；B3 首个 codec 提交必须
同时交付按字段顺序的 wire 表、边界/golden 与版本拒绝测试，不能先写消费者再补格式说明。
这是实现产物的明确先后门禁，不允许继续用 rev2 写入新增字段。

## 7. 需要测量的后续门禁

尾段选择 A1-b：正常 EOF 后通过显式 TailClockTransition 延续校准会话时钟；设备失效不等于 EOF。
设计固定为连续时间、不在 EOF 一次性把 pending 判 Miss、不跳过合法输入窗口；暂停期间不累积。
TailProfile 必须声明 EOF 连贯锚点/误差处理、固定速度、输入截止条件、由最晚 timer/deadline
和 late 参数推导的终止 horizon，且记录实际转换与最终 H；缺任何一项拒绝启用生产尾段。
B1 给 EOF/尾部内容分布，B3 给有效时钟/误差和完整结果，B5 前冻结具体 profile；
不得以“测量未做”阻塞参数化实现，也不得把 fixture 值带入生产。

SchedulingProfile 同样持有观察有效期/误差、通道容量、积压与拆分判据；没有 value-or-default。
初始 D2+D3 同 worker。执行 lag 分开报告 `Hclock-lastSuccessfulH` 和
`checked(Hclock-C)-processedFrontier`；Fold lag、表现 T 和 Fact cursor 另报，不能任选其中一个当 drift。
最老未消费 Fact 的年龄用同域 Tick 差或同一单调域发布/接收时刻差，cursor 差只是数量。
在 B3 有真实时钟后，以预先声明的内容/配置、分位数、持续窗口和界评价 D3 成本；
只有证实表现阻塞是超界原因才开启拆 D3 工作包，不能用拆线程掩盖输入缺口或无效时钟。
具体数值及生产预算保留 F3 的接受门禁；不需要再讨论本轮已选的普通设计方向。

字段顺序与golden的实现落点见[Execution Recovery rev3 wire](execution-recovery-wire-v3.md)。

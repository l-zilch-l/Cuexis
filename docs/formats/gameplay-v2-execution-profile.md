# Gameplay V2 S7A-3 / S7A-4 execution profile

状态：candidate；本轮选定的完整实施方案，不是运行验证或生产发布

更新日期：2026-10-06

实现状态：分批实现与受限功能验收只由 [CURRENT_STATUS](../CURRENT_STATUS.md) 汇总；
本候选合同的状态不等同于Stage 7A关闭或容量证明。

本文件是 [Gameplay V2 Spec](GAMEPLAY_V2_SPEC.md) 的补充 Spec。选择理由由
[ADR 0045](../adr/0045-gameplay-v2-execution-profile.md) 拥有；字段所有权与接口由
[typed 补充](../api/gameplay-v2-execution-types.md) 拥有；实施顺序由
[Stage 7计划](../stage_plans/active/stage-07/plan.md) 拥有；原3/4执行卡见
[历史交接](../archive/stage-07-planning/s7a-3-4-implementation-handoff.md)。
本文件补齐首次运行消费的空白，不重开 T4/K4、19 条拒绝面或已有第 1–7 轮裁定。
引用“待首次消费冻结”的历史文字时，本文件在明确覆盖的字段上给出本轮选择；不冒充旧裁定。

## 1. 唯一执行子集与版本

选择组合 B：L1 参数合同 + L2 独立模型、H2 结构身份、F1 逻辑元组 + F3 rank registry、
R1 三态资源与 immutable commit、S2 prepared timers + 独立 S1 扫描 evaluator。

执行 profile token 为 `gameplay.execution.t4-k4.v1`，late 算法 token 为
`late.window.logical.v1`，Fact semantic token 为 `fact.semantic.phase-local.v1`。
以上是显式算法声明，不能当缺省值，也不是新 capability 或脚本入口。
engine identity 包括以上执行算法及 registry；静态匹配声明和 phase targets 属 chart 判定投影；
实际 mapping/calibration 与 late 参数的 session 投影沿主 Spec。不得只保存 token 而遗漏实际参数。

可执行 phase 形状恰为 `{tap}`、`{head,body}`、`{head,body,tail}`。
无 resource claim 的 Tap/Hold 是独立判定；consume/claim 使用一个 capacity=1 resource；
observe 只输出内部 observation 记录，不产生 phase outcome 或 owner。
consume 在 Tap 成功的同一次 commit 内获得并结束瞬时 lease；claim 的 Tap 同理。
Hold 的 consume/claim 均保留 lease 到 T4 终止，intent 始终保留，不能归一化成同一个 token。
observe 必须为 `{tap}` 且无 body/tail/grace override，其他形状稳定拒绝。

不实现 Score/grade、Ruleset fold、Snapshot/Replay wire、Playback/Player entry、SDK 版本升级。
不接受 gap/handoff、抢占、capacity>1、sameContact、连续输入、跨资源 fanout、correction、
运行脚本、运行时 solver、动态 Requirement。future 类型不以成功空实现代替。

## 2. 执行前的完整静态输入

### 2.1 Phase target 与 timing

每个可执行 Requirement 的 timing 新增 `phaseTargets`：每个声明 phase 恰好一个
`(phase, chartTick:i64)`。禁止用窗口中点、end、数组位置或零猜测 target。
Tap/head/tail 的 target 必须在该 phase 某个成功窗口中；body target 必须等于 b1。
hard deadline D 沿既有 T4：有显式 tail 时 checked(end+preparedGrace)，无 tail 时 D=end；
无 tail 的非零 preparedGrace 不延长任何成功机会。bodyEnd=b1 的即时终止保持。
body 成功依赖完整覆盖，不依赖 body window 中某次输入；body window 是声明的测量范围，
不得据它生成提前成功。窗口/body/end/deadline 的其他规则保持主 Spec §3.8.11。
phase declarationOrdinal 必须在本 Requirement 内唯一，但不决定语义 rank。
Hold head 的 chartTick 还必须不晚于 b0；body 的 successWindows 恰为一个 [b0,b1)，
它是覆盖测量区间，关闭时先做 bodyEnd 再结算，不能在 b1 前由别的 body window 超时。
非观察实例 activationTick 为 tap/head 成功窗口 start 的最小值；observe 用 tap 的最小 start。
activation 前所有 phase Dormant，activation 后 pending；body/tail 仅 head/body 完成后可成功。
只有 `{head,body,tail}` 的 requiresReleaseTailSemantics=true；另两种形状必须 false。
本 profile 验证时间/字段合法性，不承诺 Pattern、predicate、contact 与竞争的联合 Hit 可达性；
合法但从未匹配的实例允许 prepare，并在最后机会关闭/D 得到 Miss，不引入未定义的 liveness solver。
Hold head 的可执行输入只允许 `t<=b0`；窗口内晚于 b0 的部分不回填覆盖。
prepare 证明至少一个合法 head Tick 存在，不能把“至少存在”误写成每次输入都成功。

### 2.2 Atom binding

每个 Requirement 新增 `atomBindings`，每个 Pattern atomRef 恰好一条，无未知 atom。
一行字段为 `atomRef`、`domainToken`、`sourceClass`、`channelToken`、`action`、
`amountRange:optional(minimum:i64,maximum:i64)`；五个 token/action 必须显式声明。
domain 必须等于 Requirement 的 domainBinding，domain/sourceClass/amount 范围须与
session mapping 兼容。没有 amountRange 表示不按 amount 匹配，不能解释成 amount=0；
有 range 则 observation 必须有 amount，按闭区间检查，且范围在 AmountSpec 内。
channel/sourceClass 使用精确 token 匹配，没有 wildcard、大小写折叠或隐式 action alias。
同一观测可匹配若干 atom，形成按 atomRef bytes 排序的集合，禁止选择“第一条 binding”。
几何 domain 的坐标声明不自行产生空间采样；本 profile 输入是离散 channel/amount。
需要多轴位置、轨迹或动态 frame 的匹配不在此 profile，prepare 拒绝，不能忽略字段。
静态 domain axes 继续做既有范围/引用验证并进入 identity；本 profile 没有空间采样谓词，
不能从 channelToken 的拼写推导轴坐标或声称实现了空间命中。声明空间谓词时明确拒绝。
所有 binding sourceClass 必须与会话唯一 mapping sourceClass 相同；多 sourceClass 的 contact
隔离可作为纯 reducer fixture 检查，不能宣称 session 已有多映射/多设备入口。

head Pattern 的每一条成功路径都必须以 press 结束；tail 匹配必须为 release
结束。Tap 可用显式 press/release/update。body 只用覆盖 timer，不另运行输入 Pattern。
本 profile 的 Pattern 用于 tap 或 head；tail 的 release 匹配使用本 Requirement 中 action=release
的 bindings，并要求该 release 属于持有 contact。存在 tail 时至少一条这样的 binding。
Pattern 中不引用的 tail-only release binding 允许，但须显式 `tailOnly=true`；其他多余 binding 拒绝。
`tailOnly` 默认为不存在而不是 false，作者必须声明布尔值；非 tailOnly bindings 与 atomRefs 恰好相等。
prepare 检查 press 终结性质，不能先运行再发现 Hold 成功没有 press/contact。

### 2.3 物理承载

缺失字段不能被当作“未来 S7A-5 再补”。可执行源必须提供上述 fields；Packed 使用
[Capsule §12](GAMEPLAY_CAPSULE_V2_FORMAT.md) 的 candidate revision 3。
revision 2 保留已有合同/golden，仅用于其既有静态 round-trip；执行 gate 拒绝缺失 execution
profile 的旧图。禁止在 revision 2 下偷偷增加字段或改变 semantic hash。
作者表达、affine lowering 与两路适配见 [author profile](gameplay-v2-author-profile.md)。

## 3. Late window：参数、边界与转发

### 3.1 单位与关系

所有数值均为同一 Timebase 的 TickSpan，`maxQueueHop` 是最大转发**时间跨度**，不是次数。
缩写 O=windowOpenThreshold，C=windowCloseThreshold，F=finalizationWatermark，H=maxQueueHop。
完整准入关系：`0<=O`、`0<=C<F`；rejectLate 要求 `H=0`；queueNextTick 要求
`1<=H<=F-C`。各值必须 declared+measured，checked subtraction 先于比较。
F 不表示绝对 Tick；同样的 span 相对每个窗口平移，负 observation Tick 合法。
不设置生产默认值、研究限额或 INT64_MAX 业务上限；S7A-9 仍接受实测阈值。
非负/严格序/关系是算法合同，不是生产数值选择。

对 logical Tick t，观测 cell 为 `[t,t+1)`，整数观测恰属于 cell t；
cell 的 open=t-O、close=t+C、final=t+F。open 是提前准备/pending 分类边界，
不扩大成功窗口；close 是逻辑提交可处理边界；final 是禁止迟到转发边界。
所有所需加减法 checked；不能在 i64 最大值处加一并 wrap。
不可表示的事件/cell 拒绝；不可表示的 prepare timer 边界拒绝整个 prepare。
cell 不逐微秒分配对象，用公式计算；O 只控制提前准备，不制造逐 Tick timer。

### 3.2 两个时钟游标

`advance(a)` 的 a 是调用者声明的校准会话推进 Tick，单调不减；不是 wallclock。
最后成功 advance 的 a 记 A，未 advance 时 A=absent，不填零。
已处理 frontier P=`checked(A-C)`；在一次 advance 中按 logical Tick 顺序处理到 P。
Fact 的 commitTick 是被处理的 logical Tick t，不是调用的批量 horizon a。
render frame 只是何时请求 advance；不参与观测、竞争或 Fact identity。
调用一次 advance(1000) 与分段 advance 到相同终点，在**相同 submit/admission 轨迹**下应等价。
不同提交轨迹跨过封窗边界可能有不同迟到处置，不能宣称无条件帧率独立。

advance 不必枚举空 Tick：处理 activation/timer/input/signal 的下一个有事 Tick，最终推进 P。
空 Tick 无 Fact，但仍视为 sealed，不能因跳过空 Tick 而重新收旧输入。
窗口开始/结束 cell 恰按整数单位解释；C 是提交缓冲跨度，不改变 T4 使用的 logical Tick。

### 3.3 admission 表

先在只读 journal 完成 S7A-2 normalization 与重复校验，再执行以下表，最后原子发布 ingress state
和 pending entry。拒绝不能消耗 observationId、ingressSequence 或留下去重条目。
每条 entry 拥有 copied canonical subject，并携带 `dispatchTick`、`wasForwarded`、
`admissionFrontier:optional(P)`、`admissionHorizon:optional(A)`；分别记录接收时已封存
逻辑边界与最后成功调用 horizon，不能把 A 当 P；它们是执行轨迹，不是 content identity。

| 条件（原始 observationTick=t） | rejectLate | queueNextTick |
| --- | --- | --- |
| A absent 或 A<t+C | 接收，dispatchTick=t | 同左 |
| A>=t+F | 拒绝 finalized | 拒绝 finalized |
| t+C<=A<t+F | 拒绝 late | 仅当未转发且 q=P+1 可表示、`1<=q-t<=H` 时接收，dispatchTick=q |
| canonical subject 已在 pending 或已接收集合 | 重复拒绝 | 重复拒绝 |

未来未 open 的 entry 可 pending；到 open 后变成 ready，但直到其 dispatchTick 被处理才求值。
队列最多转发**一次**，次数规则是固定算法条件，不把 H 改成 u32/count。
forward 后原 observationTick 不改；调度/activation/lease 生命周期使用 dispatchTick，
成功测量仍使用原 observationTick，并且相应 phase 在 dispatchTick 必须仍 pending/eligible。
tap/head/tail 的 observation candidate 同时要求原 t 与 dispatchTick 均在该 phase 的窗口并集，
不要求同一段 window；窗口间空隙不可推进 matcher。owner release 的失败控制路径除外。
已完成、已 miss、过 D 的 Requirement 不复活；head dispatchTick>b0 不得建立覆盖。
tail release 若原 t<b1 而 dispatchTick>=b1，按原释放时刻判 body 覆盖失败，不能算满覆盖。
timer 只在原 logical Tick 处理，不进入迟到输入转发队列。
事件原 t 与 dispatchTick 分别参与 origin subject 和 commitTick，不混同。

同 Tick 的第二个**不同** canonical subject 仍按 S7A-2 `same_tick_collision` 拒绝；
相同 subject 是重复输入/重复排队。转发导致 dispatchTick 碰撞时也拒绝第二个 distinct subject，
按 canonical subjects 的整体集合验证；批量 submit 的碰撞使整批失败，不能按数组顺序选一个。
现有单条 API 对已接收状态的拒绝是声明的 admission 行为；无权把它改为多输入仲裁。
一个 Tick 可有多个 timer 和至多一个 admitted input；timer 释放后该输入可使用 free slot。

L2 独立模型枚举边界与 admission，不调用 L1 helper。测试值 O=2,C=3,F=8,H=4
仅 fixture：t=100，A=102 native100；A=103 forward101；A=106 forward104；
A=107 因 hop=5 拒绝；A=108 finalized。reset 恢复 A/P absent。

## 4. 结构身份、contact 与资源

### 4.1 比较与生成

text 按 UTF-8 unsigned bytes 比较，无 Unicode normalization/区域排序；比较结构字段，
不按 delimiter 拼接字符串、pointer、hash 或运行期 counter 比较。
CanonicalObservationKey=`(observationTick,domainToken,sourceClass,channelToken,actionRank,
amountPresence,amountInteger)`；action rank press=0,release=1,update=2。
absent amount 的 integer 固定规范化为 0，但 amountPresence=false 与 present0 不等。
现有 ObservationId/IngressSequence 只诊断与 admission，不能替代该结构 key。

CandidateKey=`(Requirement E,phase,originKey,resourceId optional,intent optional)`。
独立组的 resourceId/intent 均 absent；真实 claim 保留原 intent，不能将独立实例伪装成 consume。
optional 比较 absent 在前，intent 逻辑 rank observe=0,consume=1,claim=2；不是 wire tags。
SlotIdentity 保持 `(resourceId,slotToken,0)`。
ContactHandle=`(domainToken,sourceClass,channelToken,startPressKey)`。
LeaseIdentity=`(SlotIdentity,Requirement E,CandidateKey)`，只有真正获胜的 commit 产生。
不分配 losing lease，不把内部 dense table index 写入逻辑身份。Reset 新 session 的
结构值可相同；配置的 JudgementIdentity 相同并不代表同一运行实例。
内部 view 以 self-owned runScope anchor 区分每次 prepare/reset 运行，anchor 只用于有效性检查，
不进入逻辑 key、hash、排序或 Replay。跨 scope 的 ID 不能用于 session mutation；
相同输入运行的 canonical 值对照忽略 runScope，仍应相等。

### 4.2 Contact reducer

contact 通道 key=`(domainToken,sourceClass,channelToken)`，每个 key 至多一个 active contact。
无 active + press：开 contact；有 active + release：关闭该 contact；
有 active + update：保持；无 active + release/update：合法 orphan observation，不开 contact；
有 active + press：重复按压 observation，不换 startPressKey，不建立第二个 contact。
orphan/repeat 并非设备 discontinuity，不自动 fault；它们可供 Tap 的显式 binding 匹配。
Hold head 必须是实际开 contact 的 press；repeat press 不能复用已开 contact 获取新 Hold lease。
contact 的开始/结束与资源 occupancy 分离：Tap 结束 lease 不伪造物理 release；
无 tail Hold bodyEnd 结束 lease 后，contact 可继续 active 到实际 release。
一个 contact 在本 profile 至多支持一个 active Hold；无资源 Hold 也做该检查。
release 路由所有匹配观察者及其实际 owner，不按“当前 resource 最小 pair”改 owner。
update 更新物理 contact 状态并可匹配 Tap/observer；head 已 Hit 后 matcher 停止，
body 只由覆盖 timer 结算，update 不自动作为 Hold owner control 消费，不阻止合法新 Tap 竞争。

### 4.3 Resource 三态

Free 不带 owner/lease/contact；Held 恰有一个 owner/lease/contact；Terminal 三者 absent。
Held 是持久 Hold 状态；Tap/ε Tap 的瞬时 lease 只存在 draft 内，最终 Free→Free/Terminal，
不为没有 physical contact 的 Tap 伪造 Held/contact。
Terminal 永久不回 Free，reset/重新 prepare 才重建声明初态 Free。
Held 只允许 owner 的合法继续/终止；竞争者被 considered 后排除，无抢占/回填。
Free 才执行 occupying eligible 子集的 K4 唯一最小 pair。observe 不参加排序，独立只读执行。
同一观测最多推进一个 occupying/独立非观察 Requirement；独立实例也须显式 competition pair
并使用逻辑 arbitration resource `@independent`，该命名空间为引擎保留，作者 resource 不能同名。
prepare 对此组进行相同 pair 唯一性检查，但不产生持久 Slot/Lease。
合法 owner release 控制先路由 owner，再考虑新竞争；owner consumed 时不再选新 winner。
owner 控制不被更早 resourceId 的 free contender 抢走。没有 owner 控制消费时，
跨多个真实 resource 与独立组先按 resourceId bytes 找有 eligible 的组，再按 K4 选一个；
fanout=1 是整个 observation 的消费上限，不能每个 resource 各消费一次。

## 5. Pattern 推进、considered 与消费

每个 tap/head activation 初始化 compiled Pattern 状态；不可变程序由 prepare 产生。
一次 observation 给出 atom 集合，对每个可能匹配 atom 分别求 transition，并在本实例内
取状态集合/ε closure 的 canonical 并集；不以 binding 或 NFA 容器顺序截断分支。
成功 arm 沿现有 leftmost-first 的显式 Pattern operands 顺序选；choice operands 是语义列表，
不可为“重排确定性”排序。有限 repeat 先做既有 canonical 完全展开；符号化实现应逐字段等价。
候选携带 matcher 的 staged next state，赢家才发布 occupying/独立 state；loser 不推进 matcher。
成功前的合法 Pattern progress 也算一次消费，Hold 尚未成功时不取得 lease。
Hold prefix 可包含显式 release/update/press；最终 accepting transition 必须是本次真正新开 contact
的 press。若该事件仅为 repeat press，整个 accepting candidate 不 eligible、不发布该 matcher
patch、不消费，保留原 matcher；不把它降为不成功的 progress 或复用旧 contact。
observe 可发布自身 matcher progress，但无消费、lease 或 phase outcome。
Tap/head 成功后不再启动第二次匹配；不采用滑动 suffix、隐式 retry 或自动反复激活。
仅与本 Requirement binding 匹配的事件进入其 Pattern；不匹配事件不追加失败 token。
有匹配 binding 但 transition 无可达状态，该事件 considered、未消费，当前 matcher 保持不变。
negation 的字母表是本实例显式非 tailOnly atomRefs，不能把任意未知输入塞入 complement。
ε 接受只在 activation/window-open 的 timer 路径求值一次；Tap 可产生 timer Hit，
Hold head ε 成功因没有 press/contact 在 prepare 拒绝。observe ε 只生成 observation 记录一次。

### 5.1 Matcher 与 observer 的补充状态

matcher 初态仅初始化一次，跨同 phase 的窗口空隙保留状态；新 window-open 不重置。
一次 observation 只消耗一个语言步，atom 集合的各个候选并行分支，不能连续当作多个输入。
next state 先剔除不能到达 accepting state 的死分支；存在有效分支才算 progress，
包括有效自环。所有有效分支保留，当前有 accepting 分支就立即完成，不能等将来更长的 arm。
多个当前 accepting arm 按现有 leftmost-first 选；相同 arm 的 atom witness 按 atomRef 字节序选。
无有效 transition 时保持 matcher，输入不加入本地已消费 trace；这不是重启 matcher。
εAttempted 在首次合法 activation/window-open 置 true，即使资源阻塞也不对后续窗口重试 ε；
若仍有非空匹配路径，后续输入仍可推进它。所有初态 ε 的检查和 flag 更新仅在 Tick draft 内。

observe 使用独立 ObservationMatchState=Pending/Observed/Expired，不走 Hit/Miss phase reducer：
合法 transition 发布观察者自身 progress；accept 时 Observed 并结束；最后 tap window close 时
未 accept 则 Expired。它不获得 lease、不发 phase Miss，也不继续考虑已经 Observed 的实例。
每个 considered 的 observe 在成功 seal 后产生一条内部 ObservationRecord，包含 E、originKey、
commitTick、optional observationKey、progressed/accepted bool；多 atom 仍只一条。
ε 记录 observationKey absent。记录随 query owner 保活，仅 inspection，不进 Fact Ledger。

`considered` 在过滤后、Pattern 成功/资源检查前确定：实例 active、phase pending、原 t 位于
该 phase window、dispatch 时尚未终止、domain/source/channel/action/amount binding 匹配。
head 额外满足 dispatch<=b0；tail 只有 body Hit 后才能 considered；
body Pending 不运行输入 Pattern，owner 控制另算。count 按 distinct Requirement E 计，
一个事件匹配多个 atom/branch 不能把 consideredCount 重复累加。实际 owner release 是控制路径，body/tail Pending 时即算 considered，
即使不在成功窗口或没有成功 release binding；它必须能失败并结束 lease。owner update 没有 body/tail matcher，本 profile 不按 owner 身份 considered/consumed；
仍可进入其他实例的显式 Tap/observer binding，不凭 update 自动占据 fanout。
observe 也计 considered。资源 held/terminal、竞争落败、
Pattern 无 transition 不取消 considered；窗外或无 binding 匹配不计。
`consumed` 只在获胜 CoordinationCommit 成功 seal 后计 true；只有 progress 也可 consumed，
失败草案不算消费。owner release 用来结束 Hold 同样 consumed，即使产生 Miss。

每个 admitted observation 形成 `(consideredCount,consumedBy optional E)` receipt。
count=0 产生一次 stray；count>0 且无 consumedBy 产生一次 consumeEmpty；二者互斥。
observe-only 产生 consumeEmpty，并附内部 observation 记录；不产生 owner/Hit/Miss。
这些分类是 coordination Fact，不能凭 raw event 直接 emit Hit。无需 phase 的分类 error=absent。
Release 导致 owner body/tail Miss 时 receipt 仍为 consumed，不再生成 consumeEmpty。

## 6. Phase reducer 与完整失败传播

状态集合 Dormant、Pending、Hit、Miss；Hit/Miss 为终态，非 observe 实例的每个声明 phase Fact 恰好一次。
Requirement 状态 Dormant、Active、Complete；不存在依赖失败后又重开的路径。
Tap 必有 tap；Hold 必有 head/body，tail 仅显式存在时处理。一次失败传播包含所有尚未
完成的依赖 phase，不回滚已经 Hit 的 head/body。下面表中的下游 phase 若 absent 不创建。

| 触发 | phase 变化 | resource/contact 处理 |
| --- | --- | --- |
| activation | 声明 phase Dormant→Pending；仅 tap/head 可开始 matcher | 无 lease |
| Tap 成功 | tap→Hit，Complete | 瞬时 lease 同 commit 结束；contact 按输入 reducer |
| head 成功、contact 新开且 dispatch<=b0 | head→Hit，body 等待覆盖 | Hold 原子获得 lease/contact ownership |
| head 最后可成功 Tick 之后，仍 Pending | head/body/声明 tail→Miss，Complete | 不制造 lease；未 owned contact 不改 |
| owner release 原 t<b1 | body/尚 Pending tail→Miss，Complete | 结束 lease；实际 contact 关闭 |
| b1 timer 且 head Hit、contact 从不晚于 b0 持续到 b1 | body→Hit | 无 tail 则 Complete 并结束 lease；有 tail 继续 Held |
| b1 timer 且覆盖不成立 | body/尚 Pending head/tail→Miss，Complete | 结束自己的 lease |
| tap 最后窗口关闭且 Pending | tap→Miss，Complete | 不生成瞬时 lease |
| body Hit 后原 t 在 tail window，dispatch<D，contact 相同 | tail→Hit，Complete | 结束 lease/contact |
| body Hit 后 owner release 不在 tail window | tail→Miss，Complete | 立即结束 lease/contact，无 gap |
| tail 最后窗口关闭 | 尚 Pending tail→Miss，Complete | 立即结束 lease；物理 contact 可仍 active |
| D timer | 所有仍 Pending phase→Miss，Complete | 结束自己的 lease；已 Complete 无操作 |

head 最后机会由 head windows 中 `min(hi,b0+1)` 的最大值决定，checked b0+1；
是离散包含 b0 的上界，不把 body 或成功窗口统一加一。head 没有合法机会的图 prepare 已拒。
timer 同 Tick 稳定执行：activation/window-open → bodyEnd → phase-close → hard-deadline。
先完成全部到期 timer 再应用输入；例如 b1=1000、tail[1000,1020)，release1000 成功，
release1020 在 close timer 后只能 orphan/其他实例匹配，不能挽救 tail。
同 Requirement、同 Tick 的多个 timer 合并一个 TimerBatch draft，禁止重复 phase Fact。
bodyEnd coverage 检查已 admitted、dispatchTick=b1 且原 release<b1 的 pending owner release，
把它作为本批已知的覆盖中断 evidence；此时先产生 timer Miss，再处理 release/contact reducer。
不前看尚未提交的未来输入，也不提前执行该输入的 Pattern/消费；输入 receipt 按 timer 后状态计算。
无真实 resource 的 Hold 也记录 headContact/acquiredTick，但 lease absent，不能制造虚拟 Slot/Lease。
完成的 Hold 保留 headContact/覆盖证据至 reset，供 late admission 判断已 seal body 冲突。
body 覆盖由 contact 的已接受实际 release 时刻判断；late release 原 t<b1 必须在该 bodyEnd
seal 前可知，否则 bodyEnd 已提交不得撤回；若 submit 时发现原 release 会反写已 seal body，
整条 admission 拒绝，ingress/contact 不变，返回 late diagnostic，不产生 admitted receipt。
不能产出第二次 body Miss；原 lease 仍按原来的 tail close/D 终止，不创建新 lease。
该规则是“已提交事实不重开”；Replay 必须复现相同 admission 轨迹，不能事后改用设备时间纠正。

owner release 的 tail Hit 还必须匹配至少一个 release binding（包括 amountRange）；
不匹配也必须 consumed，并产生 tail Miss/立即结束 lease，不能让别的 contender 吞掉 release。

TimingError 为 optional<TickDelta>：观察驱动的 phase outcome 用原 observationTick-chartTick，
checked signed i64；timer、依赖传播、stray/consumeEmpty 无真实 phase 观测时 absent。
timer body Hit 可引用覆盖 evidence，但不能把 b1 冒充 observationTick 或 error=0。
未先由 timer finalize 的早释放引起 body/tail Miss：body 有 release 观测的 error；
tail 是依赖失败，error absent。上述 late-release-at-b1 的 timer Miss 保持 timer error absent。
head 超时导致下游 Miss 同理全部 absent。grade 本批 absent，category 从 phase 唯一派生。
error 溢出使整个未 seal Tick 失败，不饱和、不部分发布。

## 7. Timer、origin、Fact 总序与提交

### 7.1 Prepared timers

TimerIdentity=`(Requirement E,phase,kind,logicalTick,windowStart optional,windowEnd optional)`。
kind registry 为 activation=0、windowOpen=1、bodyEnd=2、phaseClose=3、hardDeadline=4；
rank 是内部逻辑比较，不是 public/wire enum。相同结构去重，不以插入顺序编号。
timer 表按 `(logicalTick, executionKindRank, E, phaseRank, windowStart, windowEnd)` 排序；
所有 timer identity 放进只读表，seek/reference 重建不得生成新逻辑 ID。
先激活全部到期实例，再按全局 timerKind/E/phase 次序逐个 reduce；TimerBatch 只是合并
同 origin 的 patches/Facts，不得改成按 E 一次执行它的所有 kinds，否则会改变跨资源顺序。
prepared T4 timers 的数量单独计数；maxDeadlineElements 沿既有 Pattern containment 口径，
不把静态 window/body timers 偷偷加到 Pattern deadline 容量或借零容量禁用 timers。
构造规则固定如下：activation 一条，phase=tap 或 head，tick 为该根 phase 最早 start；
windowOpen 按每个声明 successWindow 的 start 生成，windowStart/windowEnd 保存原窗口值；
phaseClose 对 tap/tail 按各 window end 生成，对 head 按每个非空有效片段
`[lo,min(hi,b0+1))` 的 end 生成（key 仍保存原 window 值），空有效片段不生成；
body 恰一条 bodyEnd 和一条 phaseClose 在 b1，phase=body。hardDeadline 恰一条，
phase 为该 Requirement 的根 tap/head，tick=D，windowStart/windowEnd absent。
activation/bodyEnd 的两窗口字段亦 absent；windowOpen/phaseClose 有原窗口字段。
同结构 timer 去重。window-open 的 ε 检查只作用根 tap/head 的 Pending matcher，
body/tail window-open 只更新 eligibility；εAttempted 阻止 activation/windowOpen 重复尝试。
中间 window close 只更新 eligibility；最后机会关闭才 finalize。无 tail 不派生 tail timers。
timer identity order 与 Fact total order 不同，不能把 callback 次序直接当 Fact ordinal。
ε Tap timer 是针对本实例的事件，eligible 集合不加入其他实例的 ε timer；不同 timer 按上述
执行顺序处理。K4 证明的是每个事件的唯一选择，不是同 Tick 全局最优化。
S2 对 sorted table 维护 cursor+active phase index；S1 每次扫描全部声明自行计算到期事项。
S1 不读 S2 cursor、heap、排序结果或缓存，实现独立 oracle。

### 7.2 Logical origin 与 rank

originKindPriority 保持 observation=0,timer=1,coordination=2,correction=3；correction 拒绝。
phasePriority：none=0,tap=1,head=2,body=3,tail=4。
factKindPriority：phaseOutcome=0,stray=1,consumeEmpty=2；Outcome hit=0,miss=1 仅作 payload。
unknown phase/kind prepare 或 seal 拒绝；新增 rank 必须改变 FactSemanticRevision。

Observation originScope 为 `(dispatchTick,CanonicalObservationKey)`，originOrdinal=0，
该事件所有 phase draft 合并一个 CoordinationCommit；实际“coordination commit”不意味着
其 originKind 自动变 coordination。Timer originScope 为 `(logicalTick,E)`，originOrdinal=0，
该 Requirement 同 Tick TimerBatch 合并一个 commit。coordination receipt scope 为
`(dispatchTick,CanonicalObservationKey)`，ordinal=0。三个 scope 用 tagged union，不拼字符串。
每个 commit 的 Fact descriptors 按 `(phasePriority,factKindPriority,Requirement E)` 排序，
无 phase 的 receipt E=absent，得到 contiguous localOrdinal:u64 从 0 开始。
一个 phase 在同一 commit 不得有两个 outcome；重复是 invariant error，不选一个。

canonicalOrdinal **逻辑表示**为
`(originScope,originOrdinal,localOrdinal,phasePriority,factKindPriority)`，结构比较；
旧 `CanonicalOrdinal:uint64` helper 仍服务 S7A-2，不能塞一个 hash 当运行 Fact 全序。
无需为 runtime 构造有损 dense ordinal，也不消费未冻结 wire。
所有 Tick drafts 完成后，Fact 唯一总序仍为
`(commitTick,originKindPriority,canonicalOrdinal)`；即使 timer reducer 先执行，
observation Fact 可以在 Ledger 的同 Tick 顺序中排在 timer Fact 前。
消费者读已封存全 Tick，不依赖“读取下一条就能重放资源 mutation”。资源 replay 应用 commit
patch/规范 evaluator；Ruleset fold 才按 Fact 顺序处理。

commitId 为 session 内 u64，起始 0，按上述 origin 完整排序给**非空 Fact commit**赋值，
同 commit Facts 连续，同一 commitId；空 progress patch 没有 Ledger commitId，仍原子发布 state。
factId=`(commitId,localOrdinal)`；originId 为完整 tagged originKey，不用 observationId。
ID allocator 显式持有 nextCommitId:u64 与 exhausted:bool；初态 0/false。
UINT64_MAX 可作为最后一个合法 ID，分配后置 exhausted=true，不 wrap；申请超过剩余容量时
整个未 seal Tick 失败。localOrdinal 从 0 开始独立检查，empty commit 不消耗 ID。
seek/replay 不重编号已有 Ledger，reset 清空 session。
engine revision/profile/ranks 与 session identity 分别比较，不能把 runtime ID 写进 prepare digest。

### 7.3 Atomic seal 与后续批次

读取已提交 state → 在临时 TickDraft 顺序计算所有 timer/input patches → 完整校验资源、phase、
contact、计数和 Fact identity → reserve 全部发布存储 → seal 全 Tick state+ledger+receipts。
draft 对外不可见；allocation/arithmetic/invariant 失败不发布该失败 Tick 的任何 patch/Fact，
不越过它推进 P。一次 advance 内每个 logical Tick 独立 seal；失败以前已 seal 的 prefix 保留。
资源终止与该 phase outcome 同一次 seal；timer 中间 state 只给后续同 Tick reducer 的草案看。
已成功 submit 的 pending 输入仍可保留供诊断；seal 失败会话进入 faulted，不能重试猜测状态。
一次 advance(100) 在 t40 成功、t50 失败：保留 t40 结果和 [41,50) 的已处理空 prefix，
P=49；t50 patches/Fact/receipt/counters 不发布。若失败 Tick 为 INT64_MIN，P 可 absent。
A 只在整个 advance 成功时更新为请求 horizon；Faulted 的 P 可与旧 A-C 不同，
诊断另保存 failedTick/requestedHorizon，不在 Faulted 执行 late admission。正常成功返回 P=a-C。
prepare/submit 的校验或分配失败是 session_unaffected；advance 开始前 horizon 反转/
无法表示 a-C 同样 unaffected。处理已接收 Tick 时失败才 kernel.transaction_failed/faulted。

第 6 步 signal 标记 visibleFrom=`checked(commitTick+1)`，同 Tick不可触发第二轮候选。
本批用内部 signal fixture 验证可见性，不假装已实现 Ruleset/Hook。时间加一溢出在 seal 前失败。
S7A-5 加 Fold 时按主 Spec：Ledger 已 seal 后 Fold 失败，保留 Ledger 和旧 fold state，
不发布 effects，session faulted。**R1 draft rollback 不能回滚已 seal Ledger**。

## 8. 失败优先级、诊断与关闭界限

submit([]) 在 Prepared 为成功空操作；不得推动 clock/ID/queue。batch 中 ingressSequence
重复先报既有 sequence duplicate；同 subject 重复其次（duplicate_queue_entry）；
同原 Tick distinct 再报 same_tick_collision；
只有这些全部通过才判断 dispatch collision。单条既有 normalization 的错误优先级保持。
S7A-2 校验全部在 journal 中进行；任一 capturedTick 小于 live lastObservedTick 仍报
既有 time_reversal，queueNextTick 不豁免校准时钟倒退。批内比较顺序按 capturedTick 和 canonical subject，
不得使用 ingressSequence 决定成功候选。拒绝整批，pending receipt 不返回半批。
InputReceiptPending 只有 observationKey/dispatchTick/wasForwarded/admissionFrontier/admissionHorizon，
没有 considered/consumed 或 FactId；最终 receipt 仅由 advance seal 发布。
query 的 observer/receipt 集合只能查询已 seal prefix，不会显示 admitted 未执行输入。

验证顺序：载体 bounds/revision → 完整字段/引用 → 数值可表示性 → phase/timing/atom 关系 →
late 声明/关系 → capability → K4 唯一性 → pattern containment → 接受预算。
同类多个错误按 canonical field path 排序返回首项，detail 可另列所有错误，不以 source array 决定。
具体内部码与 category 在 typed 补充登记；必须先同步集中码表和诊断测试再消费，不能借临时代码
返回成功。现有 R 编号仍 19、category 仍九类，新增详细码不是新 capability。

arm/deadline 包含性 missing/pending 是 gate_incomplete，prepare 原子失败；它不等于可选
编译状态数测量 missing。后者保留 measured/unknown/证得下界状态，不当作超限，也不写通过。
若实际测得值或下界超过显式 acceptedThreshold，budget_exceeded；没有接受阈值不编造一个。
状态/稳态性能数值整体证明仍归 S7A-9，S7A-3/4 仅允许按
[交接计划](../archive/stage-07-planning/s7a-3-4-implementation-handoff.md) 的受限功能验收关闭。
这不解除 Stage 7A 整阶段关闭门禁，也不为 future stage 创建默认可执行承诺。

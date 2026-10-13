# Execution coordination preview

适用版本：SDK API `0.8.0`；更新日期：2026-10-12。

本入口属于显式 candidate SDK，复用 PlaybackSession 内的唯一 JudgementSession。
选择 `gameplay.execution.t4-k4.rpa.v1`、`late.window.batch.v1`、`fact.semantic.rpa.v1`
及 `cuexis.gameplay.rpa.v1`，并使用 matching compiled content。
旧 profile 保持单 Tick 单输入和 Recovery revision2。

## 首用与推进

B3 安装消费者使用 GameplayOnly。加载后，调用 `beginRealtimeGameplay`，传入显式
SchedulingProfile、A/I/V offset、固定 rate、monotonic domain、scope/segment/runEpoch。
本轮参数仅允许 testOnly；生产参数的数值、容量和误差接受归 B5/F3。
运行时传输仍在当前 Playback owner 串行处理；线程、音频和呈现协调归 B4/B5。

`GameplayOccurrence` 保留捕获时 ObservationTick 和 sourceLocalPosition。`GameplayClosedBatch`
具有观察到的 closure 或显式 epoch termination 证据；分片完整后一次准入。
`deliverGameplayCapture` 传入当前真实 monotonic time 与 coherent observation。
连续 capture prefix B 完成后，只有 F(H)=H-C < B 才执行原来的 H；B 等号继续等待。
分片 gap 不推进 cumulative acknowledgement，消息重传不再准入。

`requestGameplayAdvance` 返回 Waiting、BusyNotAccepted 或 terminal decision。
等待不占用输入/控制循环。request ID 的精确重传返回原结果；内容冲突失败。
`acceptGameplayControl` 在有效 owner reception 取消尚未执行的等待请求；执行与取消互斥。
`queryGameplayAdvance`、`retireGameplayAdvance` 保留同一 operation identity。
退休记录拒绝重新开始；有限 retention 用尽明确报 capacity_exceeded。

## Clock 与 offset

`RealtimeClock` 也可单独由无头宿主消费。ClockObservation 同时携带 source、presented、
deviceMixed position、publicationSequence、sample/validUntil、quality、errorBound 和状态。
有限音频源可提供 `maximumPresented`（与 presented 同帧率的真实源末帧），使 Playing
外推在源末端停止，随后仍须显式 EOF/tail 转换。缺省时保持原有无界外推；不能用它截断普通谱面
或回退已处理的 H。SDL adapter 的 owner-only `presentedClockObservation()` 提供由实际
postmix 回调锚定的名义设备速率映射、缓存样本时间、真实 clip 末帧及观测误差。
尚无回调样本时属于 warm-up；轮询不重新标记缓存帧的时间。
H = P-A，ObservationTick = P-A-I，PresentationTick = P-A+V。
计算保持 checked rational，到最终 Tick 只做一次 round-half-even。
presented 已含 adapter 的输出延迟估计，桥不重复扣除输出延迟。
prepare 前用 `RealtimeClock::calibrationIdentity(mapping)` 设置 GameplayConfiguration.calibration。
该函数将约分后的实际 A/I、固定 rate 和 mapping revision 绑定到既有 session 校准身份，
`beginRealtimeGameplay` 校验该身份与实际 mapping 一致；不一致拒绝启动，旧 profile 不受影响。
V 只影响显示，不进入该身份；等价有理数得到同一身份。

缓存观察保留原 sample timestamp。同 sequence 内容改变、过期、unknown quality、欠载、
设备或速率变化、算术溢出均撤销执行许可。失效段不能用下一条同段观察自动续跑；
显式新段还需执行协调的恢复/替换协议。等待请求在输入补齐时再次复验时钟。

正常 EOF 用 `RealtimeClock::startTail` 和显式 TailProfile 创建 TailClockTransition。
输入截止点与 terminal H = max(lastTimer,inputDeadline)+C 分开。
`sampleTail` 从真实 monotonic elapsed 推导 H；`pauseTail`/`resumeTail` 排除暂停墙钟时间，
resume 使用新 segment。轮询跨过终点时冻结在声明的最终 H，使任意固定 rate 都能到达
最后 timer；这些 owning 结果仍交给连续 prefix 门禁。设备失效不会进入 EOF tail。本轮提供时间桥和 owning transition；
Player/audio owner 的尾段部署与输入截止执行归 B5。

## Pause / Resume 与恢复

先接受控制、终止 capture epoch，交付包括 q 的最后完整批次与 EndRunEpoch。
`pauseRealtimeGameplay` 用单独标记的 ControlDrain 到 q+C，再固定 inclusive q；
软件 Paused 不代表物理音频已经停住。
`resumeRealtimeGameplay` 传入连续 final trace prefix、pause epoch、control ordinal、新 context，
以及实际 final-route-applied 结局。未知或缺失 trace 返回 control_pending。
context 与 paused control identity 在修改时钟之前校验；被拒绝的旧 scope/epoch 不占用新 segment。
恢复后的真实 H 小于已记录 ControlDrain horizon 时返回 control_pending，暂不接纳请求，
不消耗 request ID/command ordinal；等待真实时钟追赶后再提交真实 H，既不夹紧也不回退。

R2 在 q 追加独立 control commit；真实 release 只使 Pending body/tail Miss，不产生
观测 error/evidence/grade，也不重跑 timer。future-dispatch press 绑定 deferred interruption；
pause-created held contact quarantine 到真实 release。
同 q Fold 从 committed q-start view 重算，Hook 在 q+1 只输出一次。

已切换且准备失败进入 ResumeAborted，禁止直接续跑；实际封口后 Fold 失败保留
sealed facts 和旧 folded cursor，进入 Faulted。查询/归档保留实际提交历史，
A1 宿主失败不会伪造完成的 kernel Resume。故障会话不能通过公共入口新建 Snapshot。
Reset、Seek 或 restore 创建替换 scope，再显式 begin；旧上下文失效。

完整 Replay、Snapshot、checkpoint/cut/H 与 owning 生命周期见
[Recovery wire revision3](../formats/execution-recovery-wire-v3.md)。
`gameplayExecutionLag` 分别返回 clock H - lastAdvanceHorizon 与 F(clock H) - processedFrontier，
并独立报告 folded work tick、sealed/folded cursor；不得将最终 score 或一个 lag 当全部证据。

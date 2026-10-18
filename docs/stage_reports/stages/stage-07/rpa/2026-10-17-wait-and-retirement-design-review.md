# 回复等待与请求退役回收方案复审

状态：design review / candidate refinement；本报告提出改进，未实施下述新增方案。
日期：2026-10-17（Asia/Shanghai）。当前分支 `stage-7`，HEAD `ef7fa30d5b578896ccc165622fe256c553a5641c` 加工作区修改。

本次按用户要求重新比较两个缺陷的方案，优先维护性，再评估性能、扩展与计划冲突。
保留此前候选补丁及证据，暂停继续实施和性能重测。当前任务由
[CURRENT_STATUS](../../../../CURRENT_STATUS.md)维护。

## 结论

- 回复等待：保留“目标回复的持久状态 + 原 mutex + 独立回复 condition_variable”。
  调整接口职责、deadline 与故障验证；目前没有必要引入 future、无锁队列或新 owner。
- 身份回收：保留已接受的连续前缀语义，但替换“宿主逐条查询完整结果判断是否已回收”的实现。
  调度器提供有作用域的轻量回收进度，宿主只清理原始 coherent 请求载荷，不裁决第二套回收边界。
- 两者分别解决跨线程等待与单 owner 内存寿命。使用现有 owner，不建立通用任务/历史管理框架。
- 查询/重传推进请求与重试退役确认应分别定义；建议后者幂等，不能把回收后的查询错误扩大成 ACK 重试失败。
  此项是新建议，现候选代码仍对再次 retire 返回错误，尚未调整。

## 1. 现候选与证据范围

[此前等待修复报告](2026-10-17-retention-progress-and-wait-boundaries.md)保存原失败、红绿和停点。
之后已实现候选前缀回收，并按已接受的新约定调整 B3 与 Playback 的旧 Retired 返回断言。

当前源码中：

- [OwnerChannel](../../../../../app/execution_host/owner_channel.hpp)的 `waitForReply()`
  在 mutex 下检查指定 slot 的 terminal 状态；结果由 poll/ACK 取得和释放。
- [ExecutionScheduler](../../../../../engine/judgement/src/execution_scheduler.cpp)持有
  `lastRequestId_` 与 `retiredThrough_`；删除已接纳顺序的连续 Retired 前缀。
- [SessionWorker](../../../../../app/execution_host/session_worker.cpp)退役成功后遍历 coherent 表，
  调用 `queryGameplayAdvance()`，按 `host.rpa.request_retired` 清理条目。
- [PlaybackSession](../../../../../engine/playback/src/playback_session.cpp)对已回收 ID 的直接推进重传，
  在采样时钟前返回已退役错误。

已运行证据：容量 2 的调度器旧实现在第三个请求失败；新候选的 Release 普通 PD baseline、PD host、
host protocol、execution host、RPA、Judgement 六个程序完整普通用例通过。
其中 EOF tail 宿主回归已通过。随后 Playback 程序因一条旧 Retired 断言失败；
该断言已修改，Debug/Release 后续构建成功，但尚未重新运行完整矩阵。
这里不宣称整个修复退出，也没有新的性能收益或 PD-0 基线结论。

原始记录位于 `out/pd-20261017/retirement-red.log`、
`out/pd-20261017/retirement-regression-20261017T034027Z/` 和
`out/pd-20261017/retirement-build-{release,debug}.log`。

## 2. 回复等待方案比较

| 方案 | 维护与生命周期 | 性能取舍 | 计划关系与结论 |
| --- | --- | --- | --- |
| 目标持久状态、原 mutex、两类 CV | 原 slot 保有结果至 ACK；无第二套结果所有权；两类等待职责清楚 | 无每请求新分配；仍有锁和槽查找 | 推荐；兼容现 owner 部署和可靠请求 |
| 一个 CV、两类 predicate、统一 notify_all | 可正确实现，但每个事件都可能唤醒另一类等待者；后续容易误改回 notify_one | 同步对象少，交叉唤醒更多；不能仅凭对象少断言更快 | 可行替代，维护收益不足以替换当前方案 |
| 每请求 promise/future 或独立完成对象 | 增加共享状态、异常、ACK 后对象存活及取消职责 | 是否分配取决实现；多一套生命周期须衡量 | 当前单 host 串行等待无足够收益；不采用 |
| 原子等待或平台事件 | 还需处理 deadline、slot 复用、旧 ID 与关闭；仅改唤醒原语不替代持久结果 | 需真实负载验证；自旋消耗 CPU，平台事件增加适配成本 | 暂不采用；不以 Windows 单机现象冻结平台 API |
| 缩短 sleep、忙轮询、绑核或提高优先级 | 保留原完成时序问题，增加环境假设 | 可能缩短某些延迟，也会增加 CPU/调度干扰 | 只能作为诊断变量，不能作为缺陷修复 |

关键不是保留一次通知，而是同一锁下发布并检查可持久查询的完成状态。
[C++ condition_variable 规范](https://eel.is/c++draft/thread.condition.condvar)规定带 predicate 等待会反复检查条件；
初始条件已满足时不阻塞。通知本身不代替结果保留和 ACK。
[原子等待规范](https://eel.is/c++draft/atomics.wait)也不提供业务请求身份、结果和回收语义。

建议将 worker 取命令的等待与 host 等回复的等待明确命名；旧 `waitFor()` 同时承载活动等待，
不能未经调用点核对就删除其完成通知。`ExecutionHost::pump()` 仍需服务主线程输入/窗口，
不能改为一次长时间阻塞至回复结束。

等待预算建议由 host 保有绝对 deadline，每次等待取剩余时间与主线程泵预算的较小值。
现 `awaitSession()` 已有总体 deadline；改进是避免最后一次固定 1 ms 切片越过剩余预算，
不是假定 `wait_until` 消除了系统调度延迟，也不是把当前 1 ms 冻结为生产预算。

待验证的故障边界是可终止通道故障、owner 退出、ACK 与等待并发时，等待返回和 poll 错误一致。
现 fault 写入不都通知 replyCv，这属于需要定向核对的覆盖缺项，尚未定向复现新的故障。
关闭 admission 本身不能当作 pending 请求已有终端回复；正常关闭仍须 owner 排空或显式终结。

## 3. 身份回收方案比较

| 方案 | 状态与维护成本 | 性能及失败代价 | 结论 |
| --- | --- | --- | --- |
| 当前前缀 + 逐条完整 query 清宿主缓存 | 两层边界依赖查询错误码同步；内部寿命判断耦合公开结果转换 | 扫描 C 个槽；查询保留结果可复制 KernelProjection 与构造 GameplayResult | 作为可运行候选保留，不建议作为最终实现 |
| 调度器前缀 + 带作用域的轻量进度 | 调度器唯一裁定，宿主只持原始 clock 载荷；成功结果和回收元数据分离 | 一次小元数据读取/返回，按前缀释放；无需查询完整历史结果 | 推荐本次采用 |
| 在 Playback 内统一 coherent 入口和记录寿命 | 减少 app/Playback 的同步；需迁移原始 observation 校验和多个 SDK 入口 | 长期可减少重复查找；改动面及回归面较大 | 可作为后续独立重构，不在 PD-0 前置中整体迁移 |
| 每条退役立即释放，另存区间/稀疏 tombstone | 乱序回收更灵活，但新增长期区间合并、预算及历史裁决 | 单个早期空洞较少阻塞；记录可能碎片化 | 当前串行接入不需要；未来放宽乱序接纳才重审 |
| 增容、TTL 淘汰或落盘永久完整历史 | 增容只是延迟耗尽；TTL 易删除未确认身份；落盘引入 I/O 与恢复协议 | 增加驻留内存、I/O 或故障处理成本 | 不采用 |

### 3.1 当前候选的具体改进点

**完整 query 清理有确定的额外工作。** 调度器 `poll()` 按值返回 AdvanceDecision；其中
KernelProjection 含 phases、resources、contacts、receipts、facts 等 vector。
Playback 的 `publicDecision()` 还会转换 owning GameplayResult。
如果有多个未确认终端结果，每次清理可能复制与回收无关的结果；其成本取决于保留记录数及结果大小。
正常串行、每次立即确认时通常只剩当前条目，不能把最坏情形当作已有实测耗时。

**建议的窄接口是回收进度，而不是历史结果。** 内部元数据至少绑定所属身份作用域和 reclaimedThrough；
新接纳高水位与回收边界由同一调度器维护。宿主获得进度后按范围释放 coherent 载荷，
不根据 query 的本地化消息或错误码猜测寿命，也不独立计算第二条权威前缀。
可经现有非安装 `HostCoordinationAccess` 桥返回，公共 `retireGameplayAdvance()` 的签名无需因此改变。
返回值形状、作用域和重置点仍须在实施前落实，示意不是已冻结 API。

**不能把两层载荷简单合并进 Judgement。** coherent 表比较原 ClockObservation，调度器比较映射后的
AdvanceRequest；不同 observation 可能映射相同 H，仅比较 H 会漏掉原载荷冲突。
Judgement 也不能因此依赖 Playback、音频或宿主类型。首版保留两类载荷，但只有一个回收裁决来源。

**区分重复推进和重复退役确认。** 已释放请求的 query/推进重传继续明确返回 request_retired。
建议对已回收前缀内的重复 retire 幂等确认，返回相同进度或 AlreadyRetired；
这只证明无需再保留和不能再执行，不证明原推进执行成功。Waiting 或未终态的首次 retire 继续拒绝。
反例：退役已生效但确认回复丢失，重试退役不应让正常会话进入故障。
这项建议对现候选“再次 retire 返回错误”的语义作收窄，需要同步合同、调用方和回归，不能悄然更改。

### 3.2 生命周期与扩展边界

| 情形 | 必须成立的关系 |
| --- | --- |
| 单调 ID 跳号 | 前缀按已接纳顺序推进；不等待从未接纳的数字；不能仅按 ID 取模覆盖槽 |
| Waiting / 结果尚未确认 | 完整身份仍在；拒绝退役，不因后续结果确认或超时清除 |
| 后一个请求先退役 | 先移除其大结果；保留必要身份至早期空洞闭合；满容量时如实背压 |
| Resume / 时钟切段 | 保持原请求去重和身份边界；不能让旧请求成为新推进 |
| Seek / reload / Close | 明确调度器替换、coherent 缓存清理与 scope/epoch 的同 owner 提交点；失败时不能只重置一侧 |
| 未来多个并发调用方 | 现合同要求新接纳 ID 增加；并发生成不保证按序抵达，放宽时需重审接纳/空洞策略 |

源码显示 Seek 会重建执行调度器，而宿主 coherent 缓存不在该分支直接清理。
这是跨对象寿命需要核对的事实；尚未证明现请求号与 context 防护下会发生旧 ID 复执行。
现新增定向测试也尚未覆盖“保留未确认结果跨 Seek 后继续超过容量”的组合，不能据普通 Seek 通过宣布此边界关闭。

目前保留 map 容器较容易审计前缀与空洞，无需同时换成无锁环。
如后续测量证实节点分配构成问题，可独立改为预分配槽/索引，保持同一合同和回归。
容器替换不应成为本次增加 allocator、索引代数和取消状态的理由。

## 4. 与现行和后续计划的关系

| 边界 | 影响与处理 |
| --- | --- |
| RPA / SDK 0.8.0 preview | 前缀后不再返回完整 Retired 已获授权；仍须记录 SDK 行为差异、新错误码和 installed consumer 回归。重复 retire 幂等是本报告新增建议 |
| PD-0 | 两个缺陷修复仍是单独前置；补丁稳定后重新绑定测量源码和 binary，不沿用旧噪声或性能结论 |
| PD-1 / PD-2 | 不改 Fact/Fold 读取、绑定索引、canonical 时间或推进顺序；回收结果查询元数据不应捕获新完整判定结果 |
| PD-3 | 保持同 owner 及明确提交点；不能把退役成功但宿主缓存释放失败伪装成整个操作未发生 |
| PD-4 | 计划明确观察性 SessionPresentReceipt 无等待；Gameplay advance/query/control 继续可靠确认。请求去重、帧 payload lease、journal、GPU fence 分属不同寿命 |
| PD-5 | 没有新增 owner 或另建调度框架的证据；不能借本缺陷提前触发线程拆分 |
| Stage 12 / 跨平台 | 采用标准 mutex/CV；不暴露平台句柄或通用任务系统至 SDK，不冻结 Windows timer/affinity 为公共要求 |

依据为 [ADR 0046](../../../../adr/0046-realtime-playback-coordination.md)、
[RPA 合同](../../../../formats/rpa-execution-contract.md)、
[宿主边界](../../../../api/realtime-host-boundary.md)、
[PD 计划](../../../../stage_plans/active/stage-07/plan-presentation-decoupling.md)和
[PD 细则](../../../../stage_plans/active/stage-07/presentation-decoupling-design.md)。
这里不建议把 OwnerChannel ACK、advance retire、present receipt、CPU 最后引用或设备 fence 合并成一种确认。

## 5. 后续实施与验证建议

优先完成轻量回收进度、明确重复 retire 行为和 scope 转换；保留当前两类 CV 等待。
增加的验证应集中在结果确认丢回复后的 retire 重试、空洞闭合、过期时钟旧 ID、
跨 Seek/Resume/时钟段的缓存寿命、owner 异常退出及未 ACK 回复保留。
现有两对象/小请求用例只能证明局部行为，不能证明高载荷开销。

行为矩阵通过后，单独比较稀疏确认、长结果与正常串行负载下的分配数、回收耗时及端到端延迟；
对比源码/binary 保持冻结，性能窗口不与构建并发。没有实际 A/B 数据前，只主张省去了结果查询/复制路径，
不宣称具体加速、噪声门禁通过或生产性能接受。

本轮仅新增复审报告并更新索引与当前状态，未据本报告继续修改生产代码或追加性能测试。

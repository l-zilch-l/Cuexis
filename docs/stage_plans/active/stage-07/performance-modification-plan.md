# Stage 7 RPA Performance Modification Plan

状态：proposal；2026-10-14；只记录实施方案，尚未修改产品代码、冻结生产预算或关闭 F3/B5。

本方案承接 [Stage 7-RPA 计划](plan-rpa.md)、[ADR 0046](../../../adr/0046-realtime-playback-coordination.md)、[实时宿主边界](../../../api/realtime-host-boundary.md) 和
[RPA 执行合同](../../../formats/rpa-execution-contract.md)。它记录当前实现审查后形成的修改顺序、保持不变的语义、验证矩阵和回退边界。实现状态仍以 [CURRENT_STATUS](../../../CURRENT_STATUS.md) 为准。

## 1. 现状与问题边界

当前性能证据位于 [2026-10-14 性能定位报告](../../../../out/perf-investigation-20261014/findings.md)；该报告使用 Debug、Windows、RTX 4060 Laptop、同一 CXC 和受控输入，不能替代 Release、跨平台、实体设备或生产预算验收。

当前主要问题按风险排序如下：

1. `ExecutionScheduler::admissions_` 持续追加完整 batch、成员和 receipt，缺少 active-tail 与 archive 的退休边界。
2. `FrameObservationLedger`、Gameplay feedback membership 和过期处理存在历史结构全量线性扫描。
3. `ExecutionHost` 和 Player 外层循环固定休眠 1 ms，造成可测得的每帧等待下限；不能直接删除并改成忙轮询。
4. `OwnerChannel` 的 post/take/complete/poll/ack 在同一 mutex 下扫描全部 slot，`poll()` 可能在锁内复制大型 terminal reply。
5. Runtime 与 Judgement 为保持事务回滚、FaultPrefix 和 Replay 语义，在热路径复制完整状态。
6. Scheduler、Gameplay owning result 和音频 publication 存在额外值复制。
7. Stop/Reset 当前与 Pause 共同映射到 `HostState::paused`，生命周期语义不清晰。
8. 音频 worker 回调和时间函数的线程、阻塞、所有权、重入契约记录不完整。

## 2. 保持不变的约束

以下语义在性能修改中不得改变：

- capture-time、Tick/Horizon、控制 cut、owner/fence、request ID、command ordinal 和 scope/generation 校验。
- `Waiting`、`BusyNotAccepted`、`CanceledNotExecuted`、`Retired`、`Failed` 等推进结果的区别。
- terminal reply 查询、重传、显式 acknowledge、迟到 completion 和 fault reserve。
- sealed prefix、FaultPrefix、Replay/Seek、Snapshot 生命周期和分配失败时的旧状态保留。
- Present receipt 的 `Unknown`、迟到 receipt、feedback membership 和 projection scope 隔离。
- 主线程负责输入、窗口事件、GL/设备提交和协调；Playback worker 与 audio owner 的责任边界。
- 不为性能优化引入隐式线程、不改变公共 SDK 线程模型、不重写 Chart/CXC/CXT 格式。

## 3. 实施顺序

### P0：建立可重复基线

先增加只读诊断，不改变行为：

- 每帧请求数、worker 往返数、等待时间和 worker 实际执行时间。
- OwnerChannel slot 扫描次数、锁持有时间和 terminal reply 复制字节数。
- Scheduler active、archive、retired 和 admission 峰值。
- Frame Ledger active frame、feedback、Unknown、迟到 receipt 和退休数量。
- Runtime/Judgement 状态复制字节数、次数和耗时。

固定同一版本身份、同一 CXC、同一输入和同一音频时钟，保存 Debug 与 Release 的原始 CSV、完整 Replay、score、Hit/Miss 和关闭/EOF 结果。

### P1：替换固定轮询等待

引入 completion generation：

```text
worker 完成请求 -> 更新 completion generation -> notify
主线程 poll(requestId) -> 泵 SDL/GL -> 有限等待 -> 再次 poll
```

要求：

- 主线程只能有限等待，不能无限阻塞窗口事件。
- 等待超时不代表取消、失败或未接纳。
- 保留 request ID、terminal reply、ack 和重传语义。
- worker 使用条件等待或平台等待原语；主线程使用带事件泵的有限等待。
- 等待时间片先作为内部参数测量，不在本方案中冻结生产阈值。

退出条件：无丢唤醒、无重复执行、窗口/输入/关闭保持响应，且 `await` 中的空等显著下降。

### P2：重构 OwnerChannel 存储

采用“有界命令 ring + 预分配 slot 表 + request-id 索引完成表”的组合，而不是直接替换成普通无锁队列。

- ring 负责命令运输；索引表负责 poll、重传、ack 和 terminal retention。
- control reserve、fault reserve、同 ID 异载荷冲突和迟到 completion 必须保留。
- `take()`、`poll()`、`ack()` 不再扫描全部 slot。
- 先保留 slot 内的 terminal reply；确认复制成本后，再评估不可变 reply handle。
- 不在实时路径引入不可控的动态分配。

必须覆盖容量耗尽、重复 poll/ack、迟到 completion、控制优先、分配失败和关闭排空。

### P3：拆分 Scheduler active tail 与 archive

将 `admissions_` 改为有界 active tail 加分块不可变 archive：

- active tail 保存仍可能被查询、重放或等待确认的记录。
- archive 按不可变块追加，避免整个历史 vector 反复扩容复制。
- 以连续 cursor 退休已确认前缀；request ID 的冲突防护保留到 scope 结束。
- 对外已有 admission 查询语义不变；若改为 cursor/view，先补齐内部接口和测试。

退出条件：长时运行内存不随 admission 总数线性增长，Replay/Seek/FaultPrefix 与基线一致。

### P4：为 FrameObservationLedger 建立索引

维护以下内部索引：

- `FrameId -> frame slot`。
- `FeedbackId -> feedback slot`。
- `FeedbackId -> containing frame set`。
- active feedback set 和必要的 Unknown/迟到 receipt tombstone。

`observe()` 直接定位 frame；`expire()` 只处理到期 feedback；已不可能收到合法 receipt 的记录才允许退休。scope、generation、projection identity 和 Unknown 语义必须保留。

### P5：修正 Stop/Reset 生命周期

Stop/Reset 不能继续与 Pause 共享可恢复语义。建议保留现有公共状态集合，通过明确的 suspended reason 表达放弃活动局：

```text
Pause  -> Paused，可 Resume
Stop   -> Suspended(Abandoned)，不可直接 Resume
Reset  -> 新 Gameplay session，进入 Prepared
```

如果实现需要新增 `Stopped` 状态，必须先同步宿主合同、状态转移表和版本差异记录。无论采用哪种表示，都必须保证：

- Stop 保留归档和结果，但放弃当前活动局。
- Reset 创建新的 Gameplay session。
- 旧 request、receipt、scope 不能在新会话中重执行。
- Stop 不能绕过 Faulted 的恢复限制。

### P6：拆分音频实时快照与诊断状态

将 `AudioPublication` 分为：

- 固定大小的 `AudioClockSnapshot`：位置、采样率、segment、sequence、采样时刻、状态、有效期和误差。
- 低频 `AudioServiceStatus`：错误文本、设备信息、服务诊断和慢速状态。

实时读取只处理固定大小快照；错误和设备诊断走非实时查询。为 `DeviceFactory`、`DecodeClip`、`monotonicNow` 补充调用线程、阻塞性、所有权和重入规则。

### P7：最后评估 Runtime/Judgement 复制

在 P1–P6 有数据证明后，才评估事务存储重构：

```text
immutable prepared/sealed prefix
    + mutable active tail
    + per-tick delta
    + preallocated fault reserve
```

Runtime 只复制可变 delta；Judgement 以不可变 Fact/Replay chunk 保存 sealed prefix。任何改变都必须重新证明 rollback、FaultPrefix、Replay、Snapshot 和分配失败语义。不得仅因为计时变快就删除完整事务副本。

## 4. 提交与回退

建议每个阶段独立提交，语义变化与机械性能重构分开：

1. `perf: add coordination and retention counters`
2. `perf: replace fixed polling sleep with completion wait`
3. `perf: index owner channel completion slots`
4. `perf: separate scheduler active tail from archive`
5. `perf: index frame observations and feedback`
6. `fix: distinguish stop/reset lifecycle semantics`
7. `perf: reduce runtime and judgement transaction copies`
8. `perf: split realtime audio snapshot from diagnostics`

每个提交都必须能够单独回退；如果某阶段改变公共合同、错误码、状态枚举或 SDK 版本，先更新对应 ADR/Spec/ABI 和版本差异记录，不把实现提交当成合同接受。

## 5. 验证矩阵

每阶段至少验证：

- OwnerChannel：容量、reserve、重复 ID、异载荷冲突、迟到 completion、ack、关闭排空、分配失败。
- Scheduler：active/archive 退休、长历史、Replay/Seek、FaultPrefix、request retention。
- Frame Ledger：乱序 receipt、重复 receipt、Unknown 后成功、迟到 receipt、scope 切换、过期和容量耗尽。
- Host：Pause/Resume、Stop/Reset、Faulted、Close、EOF tail、控制抢占和窗口响应。
- Runtime/Judgement：afterTimers、afterInput、beforeLedger、beforeFold 等故障点，以及旧前缀保留。
- Audio：快照一致性、segment 切换、暖机、EOF、设备故障和服务关闭。
- Player：物理输入路径、窗口事件、GL 提交、关闭、同一 CXC 完整 Replay。

性能验收必须同时记录：Debug/Release、源码和包 hash、输入方案、FPS、CPU 分段、等待/worker 比例、内存峰值、复制字节数、Hit/Miss/score、Replay digest，以及未执行的设备或 hosted 检查。

## 6. 退出条件

本方案完成不等于 Stage 7 或 B5 完成。只有在以下证据齐全后，才可更新 F3/B5 状态：

- 等待空转、历史扫描、slot 扫描和完整状态复制均有前后对比数据。
- 生产预算由实测和 owner 接受的目标共同形成，不能用单次 Debug FPS 自动成为阈值。
- Replay、FaultPrefix、Snapshot、receipt、Stop/Reset 和关闭语义零差异或有已接受的合同变更。
- Release、目标平台和实体设备证据齐全；仅本地 Debug 或测试注入不能关闭门禁。


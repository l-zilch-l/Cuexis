# Execution Recovery revision 3 wire

状态：candidate implementation contract；仅显式 RPA execution profile 使用

更新日期：2026-10-12

语义权威为 [执行控制合同](rpa-execution-contract.md)。本表定义实现字段顺序，
不改变 Chart/CXC/CXT 格式；内部 Kernel/Recovery 头不安装。旧 profile 保持 revision2。

## Envelope 与基础编码

依次写入：8 字节 ASCII `CXGREC01`、revision:u32=3、kind:u8（Replay=1、Snapshot=2）、
payloadBytes:u64、payloadFNV1a64:u64、payload。所有整数 little endian；有符号整数为
two's complement；FNV 使用 offset basis 14695981039346656037、prime 1099511628211。
截断、额外字节、非法 tag、UTF-8、预算或 digest 错误均拒绝；digest 不代替完整 Replay 比较。
新 profile reader 在读正文前拒绝 revision1/2；旧 profile reader 拒绝 revision3。

section 为 id:u8、bytes:u64、正文，固定顺序 1 header、2 body。
header 依次为 identity:bytes、effectiveFactSemantic:string、stateSchema:u32=2、
eventCount:u64、factCount:u64、eventCodec:string=`canonical.discrete.rpa.le.v1`。
eventCount 包含归档中的成功及稳定拒绝输入成员；调度 Waiting/Busy/取消不写成 advance。

bool、optional presence、enum、variant tag 为 u8；bool/presence 仅0或1。
vector/string/bytes 先写长度u64；string 内容为 UTF-8。optional 随 presence 写值。
Tick/ObservationTick 为 i64、IngressSequence 为 u64。core::Error 按 code、message、
contextCount、顺序 context 的 key/value、causePresence 递归编码。
不存在 hash-only identity 或借用指针字段。

## 新增载体与字段顺序

下表顺序就是 `recovery_wire.hpp` 的调用顺序。旧字段仍按 revision2 实现的字段表编码，
新增字段只在 revision3 出现；零长度和 absent 均有不同的明确字节。

| 载体 | 按写入顺序的字段 |
| --- | --- |
| CanonicalObservationKey / occurrence | observationTick:i64、domainToken:string、sourceClass:string、channelToken:string、action:u8、amount:optional i64、sourceLocalPosition:optional u64（新 profile 必须 present） |
| PauseTransition | sequence:u64、observation:occurrence |
| RpaControlRecord | kind:u8（pause=0、resume=1）、cutTick:i64、controlOrdinal:u64、pauseEpoch:u64、finalTraceCut:u64、trace:vector PauseTransition |
| DeferredInterruption | startPress:occurrence、controlOrdinal:u64、release:PauseTransition |
| ReconciliationOrigin | tick:i64、controlOrdinal:u64、deferredOccurrence:optional occurrence、traceCut:u64、release:optional PauseTransition |
| RpaLogicalState | paused:bool、cutTick:optional Tick、nextControlOrdinal:u64、pauseEpoch:u64、accepted:vector occurrence、quarantine:vector occurrence、deferred:vector DeferredInterruption、controls:vector RpaControlRecord、foldAtTickStart:optional FoldProjection、foldBaseTick:optional Tick、foldedControlCount:u64 |
| KernelProjection 的 rev3 后缀 | rpa:optional RpaLogicalState |
| ReplayRecord | batch:vector CanonicalInput、admission:vector InputReceiptPending、horizon:optional Tick、result:optional KernelProjection、control:optional RpaControlRecord、rejection:optional Error、controlDrain:bool |
| SnapshotDTO 的 rev3 后缀 | rpaHistory:vector ReplayRecord；用于完整重建并逐字段验证 logical/private 状态 |

OriginId variant tag：observation=0、timer=1、coordination=2、reconciliation=3。
tag 是 wire discriminant；reconciliation 的语义优先级为4，correction rank3仍禁止。
immediate scope 的 deferredOccurrence absent；deferred scope 为完整 occurrence。
immediate 的具体 release 证据由 control trace 保留，全部 immediate Facts 属同一 origin；
deferred 同时保留其实际 release。localOrdinal 不使用 deliverySequence 或数组下标替代。

## Body 与恢复校验

Replay body 是 vector ReplayRecord。输入记录和 advance/control 记录互斥；稳定拒绝记录保留
原 batch 和完整 error，不修改 accepted/pending。reader 验证连续 controlOrdinal、完整 trace、
occurrence admission 和实际调用形状；完整 evaluator 比较所有 Kernel/Fold/Fact/故障字段。
无设备、无当前墙钟、无 World 依赖。

Snapshot body 顺序为 kernel、matchers、coverageHistory、activeRequirements、timerCursor、
nextCommitId、commitIdsExhausted、ingress、pending、rpaHistory。
新 profile 按拥有的完整历史重建候选，核对全部 SnapshotDTO；失败不替换当前会话。
checkpoint 同时绑定 archive identity、log cut、H 和 profile identity，同 H 不同 cut 不复用状态。
owning 结果可以越过 Session Close/析构；最后释放不要求 Session/worker 存活。

## Golden 与边界

`tests/rpa/rpa_b3_tests.cpp` 固定 occurrence golden：Tick=-1、domain=d、source=s、channel=c、
press、amount absent、sourceLocalPosition=UINT64_MAX。
完整 hex 为
`ffffffffffffffff010000000000000064010000000000000073010000000000000063000001ffffffffffffffff`。
control 与 ControlDrain record 另有固定 golden，覆盖 kind、ordinal、epoch、trace count、
optional discriminant 和末尾 controlDrain bool。其余测试覆盖完整 Replay/Snapshot round-trip、每个截断位置、rev1/2拒绝、同 H 不同 cut、
重传、失败前缀及自足恢复。生产数值预算仍按 B5/F3 接受，不由 golden 自动接受。

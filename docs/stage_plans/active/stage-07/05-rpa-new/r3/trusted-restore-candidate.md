# R3b explicit trusted restore candidate

状态：implementation candidate，2026-10-27。功能/兼容通过，当前固定诊断成本不满足预算；见 [R3b 裁定包](../../../../../stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-27-r3b-certified-prefix-and-cost.md)，未接受 R3 退出或性能。
返回 [R3 实施登记](plan.md)。本候选消费[总计划 §8.2–8.4](../plan.md)，不改变既有 wire 或公开 SDK。

## 1. 条件和范围

先完成 R3a 当前修复的功能、架构及消费者门；另以固定 64/256/2048 requirement、33 条 advance
记录的普通构建诊断判断重复前缀成本。已有 64 requirement recover 对照不是该 Seek 诊断。
检查点固定 H=1119（第 17 条记录），目标 H=1135，先声明再运行，不预制目标专用检查点。

首版复用既有显式 checkpoint 工厂，不新增自动缓存、定时生成、线程或公开入口。
总计划允许不能落实两级自然绑定时先在最终 archive 上生成/认证；本候选采用该限定，
工厂的原点执行、全量 Snapshot、canonical 编码/hash、首次恢复与最后释放全部计入。
它不代表自然检查点首次使用已交付，也不接受 40k 或任意 Seek 的 20 ms 合同。

## 2. 来源与私有凭据

只给内核独占构造并封存的准确 source owner 授信。公开构造/Reader/替换字段不自动获得
内部 provenance；现有 `internallySealed_` 只证明 owner 不可变，仍需本次工厂完成：

- 整份 source 的既有 shape、eventCount、identity、diagnostic dialect 检查；
- 指定 completeRecords/partialH/H 的原点执行与完整 Snapshot 捕获；
- 原 canonical `encodeReplay(source)` 的 SHA-256，保留原字节语义；
- 私有证书绑定 source owner、canonical identity、cut/H、不可变 state owner、状态/索引版本。

canonical SHA 是生成时附着于准确 owner 的证据，不声称 hot Seek 重新编码/hash。
封存依赖内部独占构造、不暴露可变 builder alias 和 const-correct 只读使用；不把移除 const、
任意内存改写等进程内破坏纳入只读 API 合同。外部合法 mutable alias 的普通构造仍不授信。
首版继续整份 source 的 shape/count/dialect 扫描，只减少前缀执行，不宣称消除全部 O(history)。

私有状态证书的可选分配失败应丢弃优化，保留原工厂结果和原点回退，不新增 kernel fault。
公开替换 archive/cut/H/state 均不能保留快恢复资格；相同 hash 或地址单独不足以授信。

## 3. 全状态闭包和恢复

| 当前状态 | 恢复规则 |
| --- | --- |
| configuration/prepared、phaseOffsets/timerRequirements | 仍用当前身份门和一次正常 prepare；静态数据不借外部可变别名 |
| KernelProjection（含完整 Fold/RPA） | 从已认证、不可变 DTO 复制；不省略任一字段 |
| matchers/coverageHistory/activeRequirements/timerCursor/nextCommitId/exhaustion | 从证书指向的 DTO 对应字段重建独占状态 |
| pending | DTO 自持有值复制，保持顺序、dispatch 与 key |
| RPA ingress | RPA 使用 projection.rpa.accepted；独立 legacy ingress 在正常 RPA 执行中未被修改，须以源码审计/反例证明此不变量 |
| signalVisibility | 只有内部 checking signalTicks 注入写入；普通工厂原点执行无该注入。非空或 test controls 状态不得生成快恢复证书 |
| fault reserve | 从恢复态重新构造，完成所有 fallible 工作后才提交候选；不删除任一 fault 边界 |
| runScope/publication/scheduler/context/clock | 新建，不复活旧运行；目标恢复仍由原 Playback worker 完成 |
| recording/logTail/seekSource | 见下一节；完整源与应用前缀分别保存，Snapshot/archive 仍显式物化 |

构造失败时原候选仍为未执行的准备态，可安全走原点路径；外部 Snapshot 恢复继续使用原有完整验证。
普通产物全字段、wire、未来输入/同 Tick control、quarantine/deferred、故障、ID、旧 owning 生命周期
对照全部通过前，不把这张映射表当作实现证明。

## 4. 前缀日志候选（依赖审查后的修订）

在现有 ExecutionLogChunk 上增加只读 base，持有工厂封存的 SnapshotDTO owner 与准确 completeRecords。
不让 base 直接持有整个 source，以免新输入后 source suffix 仍随新运行长驻。
base 引用 DTO 中已存在的 applied history，不再复制前缀；工厂保存其 eventCount，hot Seek 不重算前缀计数。
计数包括确定性拒绝记录的 batch，不能采用完整 source.eventCount。
普通新追加仍用原 capacity=32 的独占尾块；flatten 只发生在原 archive/Snapshot 出口。

现工厂将 SnapshotStorage::dto 再深复制到 ReplayCheckpoint::state；候选用 aliasing shared_ptr
直接持有该原 DTO，省去这份全量副本。base 在后续写入后仍会延长整个旧 DTO 的存活期，
该 alias 还持有 SnapshotStorage 的 RecoveryInputs/configuration/prepared owner；
这些 active/retired 内存及最后释放单独记账，不能把省去副本当作零成本证明。

首个快恢复证书仅在准确完整 horizon record 边界生成。partial checkpoint 保持原点回退；完整边界
checkpoint 恢复后仍可执行最终 partial advance。需覆盖同 H 不同控制 cut、partial 目标、后续完整记录
及 base 后 0/31/32/33 追加边界。

## 5. 有界选择、内存和未决门

候选建议仅优化单个显式 hint；多个 hint 保持原点回退，原合法 Seek 不新增容量拒绝。
不新增内部缓存列表、索引或驻留数量。所有外部已持有的 Checkpoint/Snapshot/source 仍计入 live/retired。
依赖审查确认单 hint 不需要自动间隔或内部索引上限，但外部 owning 值和 prefix 保留仍受 M-CAP。
不能把它解释成生产 caps 已接受。

重新逐条核对后，总计划 §8.2 已允许首版在最终 archive 封存时生成/认证 checkpoint，并明确承担首次成本。
本候选采用该既有回退，只认证正常完整 record 边界，不新增公开 API，不专门命中目标 H，
不将其记作自然生成已完成。前次将这条入口误判为必须额外审批，现已撤回该确认请求；用户继续 R3 的授权有效。

新增私有证书、base、header/control block 的实际 sizeof/分配与保留、恢复动态态、全量公开 DTO、
尾部和最后释放分别登记；固定规模的参考数据不自动升级为生产上限。未落实数值和来源时标缺证据，
不使用 0、无限或未经证明的检查点间隔占位。

如果依赖审查认为必须先接受生产 profile 或正常状态仍不能闭合，停止相应实现并交具体裁定，
R3a 独立验证继续；不通过增加公开 DTO/wire 字段或降低拒绝门来绕过。

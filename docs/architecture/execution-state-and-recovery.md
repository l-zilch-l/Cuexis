# 执行状态、恢复与准备路径

状态：candidate；当前分支的内部实现参考，不是稳定 SDK 或生产验收声明

更新日期：2026-10-29

本文说明 R3 重构后保留的所有权、复用和缓存设计。产品边界仍由
[ADR 0027](../adr/0027-playback-sdk-product-boundary.md) 定义，实时职责与线程部署由
[ADR 0046](../adr/0046-realtime-playback-coordination.md) 定义；当前工作状态及未验收事项只见
[CURRENT_STATUS](../CURRENT_STATUS.md)。字段与拒绝语义见
[RPA 执行合同](../formats/rpa-execution-contract.md)和
[实时宿主边界](../api/realtime-host-boundary.md)，本文不另定义协议。

## 状态与所有权

执行仍只有一个 kernel / Fold。Playback 提供宿主门面，Player 的协调层组合各 owner；
存储共享没有增加第二条执行路径，也没有赋予同一会话并发写入能力。

| 数据 | 当前表示与生命周期 | 维护约束 |
| --- | --- | --- |
| phase / fact / matcher 序列 | `SharedSequence` 以共享目录和共享页保存值，默认每页 64 项 | 读取只返回 const 值；写入先分离目录及受影响页，不提供连续内存承诺 |
| canonical identity 字节 | 拥有所有权的共享不可变字节 | 共享存储不改变精确字节比较，也不能用地址替代内容身份 |
| `ExecutionPreparation` | 保存不可变初始状态、RecoveryInputs、phase offset 和 timer requirement 派生表 | 不保存可变运行状态；只复用已验证的精确 owner |
| 活动执行实例 | 各自持有 run scope、matcher 状态与 fault reserve | 新运行拥有独立 scope；写时分离防止回写旧状态 |
| 执行历史 | 不可变前缀与有界日志块；每块最多 32 条记录 | 追加不得修改已经发布的历史 |
| 自然恢复锚点 | 保留不可变状态、日志前缀、recording 和完整记录数的 owning 引用 | 是可选加速缓存，不是外部可提交的可信凭证 |

`SharedSequence::update()` 先在局部值上运行修改回调，再分离并发布；回调抛异常不发布该修改。
相等比较仍比较精确值，只在共享同一不可变目录或页时跳过重复遍历。
encoder 的 `storageOwner()` 会保活目录，后续写入因此必须分离。
需要平铺数据时显式物化，调用方不得把分页状态当成 `std::vector` 的连续存储。

共享页和历史引用可能延长内存存活期。锚点数量有界不等于总内存峰值已验收，
也不意味着归档销毁后所有页立即释放；必须检查其他快照、执行实例及编码缓存是否仍持有 owner。

## prepare 复用与恢复路径

内部封存的 Replay 持有与之绑定的准备结果。Seek 只有在 archive 已内部封存、准备结果存在，
且其 RecoveryInputs 是同一 owning 对象时才复用准备；其余来源重新经过 prepare。
内容摘要相同不能建立这一内部信任关系。复用的是不可变派生数据，运行 scope、
matcher 的可变视图和故障预留仍属于新执行实例。

自然锚点只在符合条件的已提交 RPA 记录边界捕获：无测试注入、pending 为空、状态 Prepared、
已有推进 horizon 且没有待处理 signal visibility，并满足内部 eligibility。
首次符合条件的记录及之后已有日志块边界可留锚，不新增执行记录或改变推进节奏。
当前最多保留 64 个，满时保留最早锚点、移除其后最老锚点；分配失败跳过缓存，
不会把已提交操作追溯为失败。这些是实现常量，不是生产内存预算。

没有显式 checkpoint 或测试控制时，内部封存来源可从不超过目标 cut / horizon 的最新自然锚点续算。
没有可用锚点则从原点恢复。显式 certified checkpoint 另校验 archive、state、cut、horizon
及私有 certificate 的精确绑定；可变外部对象不能通过替换公共字段取得凭证。
外部 Replay、Snapshot 和文件仍通过原有解码、关系和版本校验，不因缓存路径而获豁免。

实现入口为 `execution_kernel.cpp` 中的 `ExecutionPreparation`、`publishRecording()` 和 Seek。
协议与 wire 仍分别归 [RPA 合同 §4](../formats/rpa-execution-contract.md#4-t2-fact-identityreplay-与-seek)
及 [Recovery rev3 wire](../formats/execution-recovery-wire-v3.md)。

## 内容身份与来源身份

| 身份 | 用途 | 不可替代的边界 |
| --- | --- | --- |
| canonical Replay SHA-256 | 完整 canonical 内容身份，可用于外部宿主绑定 | 需要编码及哈希；不能据此取得内部准备或锚点凭证 |
| `ReplaySourceIdentity` | 同一进程内同一 owning Replay storage 的请求绑定 | 副本共享，独立 capture / decode 不共享；不进入 wire，不是内容校验 |
| run scope / context / fence | 绑定执行实例和控制许可 | 来源相同仍须检查 context、代数、cut、H、proof 与操作身份 |

内置 Host 默认选择 owning source，避免每次 Seek 为同一内存来源重新编码和哈希完整 Replay。
来源 token 只保活独立的小对象，不保活历史；它也不是 native recovery 凭证。
Seek 的 canonical 与 source 两种绑定必须恰好选择一种，详细拒绝及重传规则由 Spec 拥有。
非安装 `replay_identity.hpp` 桥负责内部访问，不能被描述为新增的安装版宿主 API。

这次 candidate 改动改变了 `StepTarget` 及嵌套 request / proof / receipt 的 C++ 布局。
消费者必须整体重新编译，不能声称旧二进制兼容。当前分支 SDK API 为 `0.9.1` preview，
不因此成为已发行接口；稳定 C ABI 仍属后续阶段。

## Runtime Transform 缓存

Runtime 的事务式属性求值通过 `TransformUpdate::prepare(..., false, true)` 使用可信缓存路径。
缓存有效时，从实际变化的 local transform 沿 children 标记后代，按缓存的拓扑 `orderIndex`
求 world matrix，并在事务提交时更新受影响缓存。entity ID 顺序不能替代父先于子的拓扑顺序。
该路径仍有输入、有限矩阵和层级检查及 scratch 管理，不承诺整帧成本只与脏节点数有关。

可变 `RuntimeSession::withWorld()` 回调结束后使缓存失效；它包围的 Registry 访问仍受 owner thread、
禁止重入及禁止保留引用的约束。不能笼统地说任意 `World::withRegistry()` 自身都会清除缓存：
独立 World 的默认 `updateWorldTransforms()` 走完整验证路径，Runtime 的可变入口由外层负责失效。
冷缓存、默认不可信路径和 legacy 路径保留相应完整扫描/重建行为。
新增 Runtime 内部可变入口时，必须维持这些信任前提或显式使缓存失效。
生命周期与事务边界见 [RuntimeSession](RUNTIME_SESSION.md)。

外观求值在一次查找中读取四个属性，借用指针在当前求值中立即消费，不跨帧保存。
resolver 的 touched 标记代替重复成员扫描；有序 entry 插入时同步平移 touched / committed /
本次提交索引，reset 清除标记，不能让优化改变先前已记录 entry 的指向。

## Packed 准备与 closure

已冻结输入通过私有只读视图同步进入共享 assembler，不为 grace lowering 复制整份输入；
作者输入仍保留可变副本。assembler 构造拥有型图，prepare 在成功后立即将图的 timebase / latePolicy
指针重绑到 prepared storage 的自有配置；输入视图不得逃逸，也不能借此信任外部对象。

Packed 解码仍重建 typed 内容，并走共享 gameplay prepare 及语义比较。
调用期索引替代 graph 匹配的重复扫描，已有 canonical 声明顺序支持二分查找；
这些索引不会成为跨调用全局缓存，查找加速不能改变输入次序、重复判定或首错选择。

content-only completeness 通过私有 `gameplay_closure.hpp` collector 显式传入空 contributions，
不再为删除 contributions 而复制整张 `CanonicalGameplayGraph`。
公开 full closure 入口仍包含原 contributions，两者共享同一内容遍历与精确 revision 比较、排序和去重。
assembler 保持全 unknown 检查、全 disabled 检查、capability completeness、feature completeness 的顺序。
继续扩展 closure 时应修改共享 collector，不能另建一套规则或依赖图的临时可变副本。

## Fact 投影与空索引

没有 presentation source binding 时，逐 Fact PublicationRead 和 kind / phase / outcome 校验仍执行，
校验通过后才跳过 owning source key 构造和匹配。不能在函数入口按空索引直接返回，
游标、scope、已有 future / active 状态、group、target 与 lastTick 仍沿共同路径处理。
非空索引继续拥有完整身份 key，查询与匹配排序保持原算法；不缓存 Fact 借用视图，
也不减少 owner / 线程 / epoch / ordinal 检查。后续优化必须维持首错和失败不修改输入状态的保证。

## 编码、缓冲与哈希

Recovery 以单输出缓冲编码，回填 section 长度；测量和编码共享字段访问逻辑，支持精确预留，
增量写入仍检查长度溢出并按有界算术增长容量。
每次 Writer 自有共享序列缓存，保存 storage owner、编码 revision 与对应字节/测量长度，
当前至多 128 个序列、8 MiB 编码量，满时新序列走普通编码。
这不是进程级缓存，也不限制输出总长或证明总分配/峰值内存有界于 8 MiB。

FNV-1a 与 SHA-256 算法及 wire 不变。FNV buffer 实现固定上游版本；SHA 的 x86 加速只在
CPU 能力满足时调用，其余走 portable fallback。第三方来源、适配和许可归
[依赖政策](../guides/DEPENDENCY_POLICY.md)及 [Third-party notices](../../THIRD_PARTY_NOTICES.md)。
身份 writer 的范围插入试验已撤销，不属于当前实现。

## 维护与验证入口

| 修改区域 | 必须保持的约束与回归方向 |
| --- | --- |
| 共享页、快照、日志块 | 拷贝后独立写、旧值不变、异常不发布、owner 释放；不能将共享等同并发安全 |
| prepare / 恢复锚点 | 精确 provenance、scope 隔离、cache miss / 分配失败回退、fault 与前缀一致 |
| source binding | 副本与独立解码的区别、恰选一种绑定、proof / 重传 / context 拒绝规则 |
| Transform | 可变回调后重建、层级增删、父子传播、事务失败不发布半帧、默认路径完整校验 |
| Packed / closure | canonical 顺序、重复与首错、revision、输入不变及旧身份字节一致 |
| 编码 / 哈希 | 已知向量、精确字节、版本拒绝、portable fallback、STATIC/SHARED 消费者 |

本地测量、回退试验和测试数量保留在
[2026-10-28 验证报告](../stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-28-r3-redesign-verification-and-optimization.md)，
不作为本文的长期架构约束。修改以上实现时同步维护本页；字段改变改 Spec，公共载体改变改 API，
测量追加带日期报告，当前接受状态只更新状态页。

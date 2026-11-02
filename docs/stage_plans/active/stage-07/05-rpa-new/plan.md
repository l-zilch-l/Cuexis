# Cuexis Stage 7-RPA 性能整改方案

状态：总体仍为 candidate；F1–F5 裁定保留，R1、R2 已退出，owner 于 2026-10-26 明确接受 R2 按原计划退出；受控局部门通过，旧不确定结果与证据范围限制保留，R4 正式接受未登记。当前承接见 §7.11；原判据保持。2026-11-02 owner 就四项 R3 阶段门、40K 240 FPS 与内存上限作出裁定，见 §9.3、§9.4、§12.2。

更新日期：2026-11-02。

当前 R3 实施中，最新核验见 [R3 实施登记](r3/plan.md)；依赖与范围见 §4、§5、§8。历史 R1 执行入口：[R1 实施与验证计划](r1/plan.md)。R1 进展见 [R1 继续实施记录](../../../../stage_reports/stages/stage-07/05-rpa-new/r1/2026-10-25-r1-continuation.md)，前次 ON/OFF 红色证据见 [R1 停审报告](../../../../stage_reports/stages/stage-07/05-rpa-new/r1/2026-10-25-r1-stop-review.md)，历史记录不回填。下列“设计基线”“裁定核查基线”和编写请求保留 2026-10-24 的历史语境，不替代本轮授权或现场身份。

设计基线：准入树 `codex/pd5-gameplay-40k-admission`，HEAD `3e5837f63f76397429c9f7603fd7450d3ce9855d`，包含本设计开始前已有的未提交插桩、工具和报告；主树 `stage-7`，HEAD `eed36648a692333e47f798aa45be36ca850d08de`，工作区干净。以下源码位置均相对于准入树，行号是 2026-10-24 静态核查定位，实施前应按函数重新定位。

权威归属：[ADR 0046](../../../../adr/0046-realtime-playback-coordination.md)（RPA 决定）、[RPA 补充 Spec](../../../../formats/rpa-execution-contract.md)（语义）、[实时宿主边界](../../../../api/realtime-host-boundary.md)（表示与所有权）、[RPA 分册](../03-rpa-legacy/plan.md)（范围、批次、门禁与验收）、[性能修改方案](../03-rpa-legacy/performance-modification-plan.md)、[PD-5 测量方案](../04-pd/pd-5/pd5-measurement-proposal.md)。本文不改变这些来源的条款。

审阅记录：[2026-10-24 候选设计审阅](../../../../stage_reports/stages/stage-07/05-rpa-new/design/2026-10-24-performance-remediation-design-review.md)。该记录保留送审版发现；本轮 F1–F5 裁定见本文 §1.1、§5.2、§6.2、§7.1 及审阅记录 §8，不把设计裁定记作实施退出。

裁定核查基线（2026-10-24）：主树 `stage-7` 的 `f0c7e3230e3abd479fda18e009a7688df89c7672`，开始时干净；准入树 HEAD 仍为 `3e5837f63f76397429c9f7603fd7450d3ce9855d`，当前 54 个修改/未跟踪文件，含新增的 session_worker.cpp 清理许可差异及 rpa_b5_session_tests.cpp 用例。它们未合入主树；原设计基线与历史证据保持独立。

本文最初的编写请求是“阅读文档，对当前设计进行性能整改，反复思考后给出方案”。2026-10-24 编写过程只做只读核查和设计文档，不修改产品、不登记测量、不提交、不启动后台任务。2026-10-25 的 R1 授权及先补文档约束单列于上方执行入口；用户随后另行授权下一对话规划并实施 R2，该次文档整理未启动产品修改；当时由主代理规划并实施首版候选，该首版未获性能接受；当前第三候选的 owner 退出记录见 §7.11。不扩展到 R3/R4 或正式测量。

源码定位约定：execution_host.cpp、player_execution.cpp 在 app/player/src/；session_worker.cpp、host_protocol.cpp、host_candidate.cpp、audio_worker.cpp 在 app/execution_host/；playback_session.cpp 与 presentation_materializer.* 在 engine/playback/src/；execution_kernel.cpp、session.cpp 与恢复实现位于 engine/judgement/src/。避免将同名“宿主”代码误定位到 Playback 模块。

## 1. 推荐结论

建议沿用已经接受的 RPA 六域和部署合同，先做 R0 归因与可控反例，再修 Close 的清理许可和超时续行、验证读取物化候选；Seek 则根据实际成本选择重复准备去重、动态事务复制整改或可信前缀恢复。维护性优先：每个改动只负责一个可证明的成本来源；复用现有布局、共享 prepared、分块日志、owner ledger 和恢复载体；不给局部性能问题增加服务线程、通用调度框架或第二份动态状态权威。

本次复核收回“PreparedGameplay 尚需改成共享存储”的前提，也不再把“不可变共享 + checkpoint”预先定为 Seek 的完整解法：前者已有，后者尚不能约束单条长 advance、恢复时的大动态副本和后缀重放中的事务复制。当前可以提出候选机制与选择门，不能从总耗时给各机制排出收益顺序。

| 主线 | 采用机制 | 可支持的效果判断 | 置信度与依赖 |
| --- | --- | --- | --- |
| Close | 正常 fence 与受限清理许可分开；清理按 accepted binding 和 owner ledger 复核；以原 job/operation 续行未完成的 Close，保留首因、cut、隔离及终态证据 | 可针对已知 fence 矛盾和超时后的阶段丢失分别设计修复；不能推断历史失败消除率或毫秒改善区间 | 清理许可已有局部候选；续行存在静态风险与测试缺口，未动态复现。先闭合相关 R0，再判断 R1 退出 |
| 读取 | 保留窗口，增加有界中性借用块；直接写最终 owning DTO，省去 ObjectPresentation 值记录中的矩阵中转和未使用的 Renderable 检索；复用现有布局 | 有明确可删除的工作；136 B 是历史反汇编口径、128 B 是字段推算，当前布局与 scratch 依 §7.1 实测；实际比值区间未知，目标仍为 95% 区间上界 ≤1.10 | 语义可保真：中高；性能收益：待测。需当前 F1 重锚和 R2 实测 |
| Seek 准备与存储 | 保护现有 PreparedGameplay 共享和分块日志；只去除实测重复 prepare、派生表构建或 canonical 编码/hash | 有静态重复调用落点；PreparedGameplay handle 复制本身不是深复制证据；尾块仍有 batch/admission 值复制 | 落点已核实；占比及收益待 R0。进入 R3a 的对应子项 |
| Seek 动态事务 | 分别计量 draft、fault reserve、Fold 和 finalization 的 RuntimeState 复制，并区分 full/idle Tick | checkpoint 不能自动消除后缀中的副本；是否需要调整表示、能否保留回滚，均未定 | 静态复制点已核实，主导成本未证实；R3a 内单独决策门，不预授权通用容器重构 |
| Seek 恢复 | 重放/验证若经 R0 证明确有必要，再采用私有、来源受控 checkpoint；认证与恢复分离，恢复独占动态状态 | 可去掉可信前缀的重复执行/比较；恢复副本、单记录尾部、首次认证及发布仍需入账 | 条件候选；需状态闭包、来源证明和成本证据。R3a → R3b |
| 长 advance 尾部 | 仅在证明必要后，另立内部 seal continuation 设计门 | 记录级 checkpoint 不能自动约束单条长 advance 的内部工作；目前无效果区间 | 最高风险，R3c 条件启动；不能直接开工 |

**不建议现在承诺“40k Seek 能从约 6.3 秒降到 20 ms”。** 当前证据不足以知道序列化、准备、内部执行、完整比较、投影、发布、时钟和呈现各占多少。方案给出改变复杂度的机制、能证伪的实验和失败出口，不填未经测量的倍数或预测区间。

PD-5 维持“未通过但定案”。后续证据归 Stage 7-RPA，不修改历史判定。完成本文中的性能整改也不等于 RPA、生产、F4 或整个 Stage 7 关闭。

### 1.1 F1–F5 本轮裁定与实施边界

本节 F1–F5 是审阅发现编号，不是 F1 冻结候选或既有 F1–F4 验收阶段。采用以下设计选择；不据此追认准入树局部实现通过，也不把本轮文档修改扩展为产品或实验执行授权。

| 发现 | 裁定 | 退出约束 |
| --- | --- | --- |
| F1：清理 fence 与实际机制不同 | 选择“受限清理许可”，不新增清理 fence；正常控制 fence 不放宽。许可来自完整 accepted binding、owner ledger、cut 和隔离，不由 status 自授权 | 无 fence 分支的 fence 对象/代数为不适用，不记通过；现有“无 fence 拒绝位图”是观测证据缺口。绑定/decision/generation 与旧 fence 兼容性仍须闭合；见 §5.2、§6.2 |
| F2：清理许可与续行混淆 | R1 保留一个批次，分别跟踪“清理许可局部候选”与“§6.3 四类续行未实施” | 审阅所载 542 assertions / 4 test cases 仅是清理许可单元证据，本轮未重跑；不是全部许可反例通过，更不是 R1 完成。两部分及 R0 观测、生命周期门均齐备后才可申请退出 |
| F3：136 B / 128 B 口径 | 保留历史口径与当前推算的来源，不择一硬编码为现行布局；R2 首个提交现场登记 sizeof/alignof 并设置对应布局/预算的 static_assert | ObjectPresentation、Result 包装、借用行与完整 scratch 分别登记；没有新构建实证，不接受 scratch 预算。K=32 仍是预定候选，不扫描到过线 |
| F4：failed 谓词层间差异 | 登记 worker 无 fence 分支与 retireSoftware/retireExecutionStep 的不同许可范围，保持本轮源码不变 | worker 的分支选择不等于所有入口已拒绝 failed Close；已有 fence 及直接调用反例待证。新增调用方或扩大路径前单独审查统一/下沉谓词，不能顺手放宽 |
| F5：字段缺少观测载体 | R0 增加编译期可关闭、owner-local、只读导出的内部诊断通道，并先完成字段映射 | Close 阶段、首次 flush 失败阶段、实际 fence 拒绝及清理许可校验结果不得从通用错误码推测；缺字段记证据不足，不新增公开 API/ABI |

## 2. 设计前必须纠正的事实

此前的交接描述是有效输入，但以下说法不能原样进入实现或验收。

| 此前交接的描述 | 本次核查结论 | 对方案的影响 |
| --- | --- | --- |
| 读取以“区间下界 ≤1.10”通过 | 已接受测量方案 §4 和 measurement.py::paired_interval 都规定：上界 ≤1.10 才通过；下界 >1.10 为超界；跨线为 inconclusive | 沿用原统计门，不改变判据，不把“尚未证明超界”写成通过 |
| H=100000，可按十万记录估算 | pd5_capacity_probe.hpp 的 tick=100000、clock rate=1000000，表示 100 ms；max_records=100000 是 Reader 预算。该 fixture 显式调用一次 advance，实际归档条数尚未单列 | 按实际 records、inputs、timer、Tick/Fold 和嵌套结果元素计量；不用 6.37s/100000 推导每记录成本 |
| 6.37 秒是 Seek 内核时间 | 冻结口径覆盖 host.seek → sample → present；当前 dirty 探针多了 seek_confirmed_us，也不能反推旧冻结运行已有可靠相位数据 | 保持外层计时，另建相位账本；嵌套时间不能重复相加 |
| materials 是 unordered_map，查找是 hash find | SnapshotEntity::materials 实际为 std::map，比较器 std::less<> | 优化讨论针对真实 map 和字符串成本，不使用错误的 hash 成本模型 |
| 静态布局尚待建立 | buildSnapshotLayout 已绑定 ID、runtimeIndex、mesh、材料闭包和字符串容量上界 | 复用已有布局，禁止平行建立第二套映射或缓存 |
| PreparedGameplay handle 复制会复制全部 prepared 数据 | 已通过 shared_ptr<const PreparedGameplayStorage> 共享不可变存储；Impl 仍会构建 phaseOffsets/timerRequirements 等派生表，prepare 也仍做校验、身份和动态态准备 | 区分 handle、配置值复制、重复 prepare、派生表及动态态成本；不重做已有共享 |
| 分块日志追加只复制 32 个轻量 handle，或仍复制全部历史 | capacity=32 限制的是当前尾块的 ReplayRecord 值；其中 batch/admission 是 owning vector，result 才共享；完整历史块已共享，flatten 属于显式出口 | 按嵌套 payload 计量尾块，而不是只计 handle 或误报每次全历史复制 |
| 每个 Tick 都执行相同数量的完整事务副本 | 完整事务存在多处 RuntimeState 深复制；deadline-only idle 路径已有跳过机制与 reference 对照测试源码 | 计量实际 full/idle Tick 和各复制点，不按时间刻度数乘固定副本数；已有测试未在本轮执行 |
| 布局不变即可不写静态 DTO 字段 | FrameSnapshot 是公开可变 owning aggregate，调用方能修改对象数量、ID、mesh、material 及字符串 | 每次提帧必须恢复全部可观察字段；地址、size、layout token 都不能证明输出仍正确 |
| 新增 rev3/schema2 即可解决恢复 | 当前 recovery_wire/recovery_codec 已有 rev3 与 RPA 字段；问题包含来源可信性、私有闭包和重放策略 | 首选内部存储与认证机制，不“再次升级”已有版本；新增字段另过版本门 |
| 90,823,328 B 可当作直接恢复量 | 这是编码 Snapshot 长度，不是可直接 memcpy 的运行态；既有内存报告中 SnapshotDTO 与 decodedSnapshot 的 retained 量还不同 | 不按 90.8 MB/带宽估算恢复耗时；分别测独占状态、共享前缀、索引与构造/释放 |
| close 4 次中 1 次失败可估算正式全组成功概率 | 四次不是同分布独立重复，且混合不同运行条件 | 不使用 0.75 的九次方等概率推论；以可控反例和预声明运行判定 |
| RPA B1–B5 产品实施“未开始”意味着源码没有这些机制 | 计划批次未退出，与源码已有 RPA、rev3、分块日志、worker 等机制是不同事实 | 建立“合同要求—代码已有—尚缺证据”差异表，避免重做现有机制或宣布批次完成 |
| 内存是不达标的产品性能项 | PD-5 定案是内存“未测量”，观察器量程拒绝，不能等价为产品内存超限 | 新分域证据单列；不修改旧结论，不用旧 requested 峰值直接接受新 caps |

核查依据：engine/runtime/src/presentation_read.cpp；engine/playback/src/presentation_materializer.hpp、engine/playback/src/presentation_materializer.cpp；engine/playback/src/playback_session.cpp；engine/judgement/src/execution_kernel.cpp、session.cpp、recovery_codec.cpp；tests/pd/pd5_capacity_probe.hpp；tools/verification/playback_performance/measurement.py；原测量方案与 P8/B0 报告。

## 3. 全批次不变量与维护成本

1. 六域、D2+D3 同 Playback worker、D1/D5/D6 主线程及 D4 audio owner 均保持。只有满足原 M-SPLIT 的持续有效窗口、归因和 owner 另行接受，才讨论拆分。
2. capture-time、canonical/admission/control 顺序、完整 Fact/Fold/ReplayEvaluation、cut/H、故障前缀和未来 pending 语义保持。digest 是补充核对，不代替完整结果。
3. 活跃可变状态始终唯一 owner；旧结果、checkpoint、archive 的成功前缀只读且自足 owning。恢复不能把 checkpoint 原对象转成可变活态。
4. Presentation 读取保留 owner、完整 stamp、serial、epoch、窗口过期、重入和 mutation 约束；不泄漏 World、registry、entity 或组件。
5. Close 未确认任何必需责任域时保持 Closing；不丢未确认输入、不伪造 ACK、不跳过 audio callback 隔离、不假报 Closed、不 detach。
6. 1.10、20 ms、15%、p99 预算和全部既有物理/hash 上限保持。适用范围也不能由实现擅自缩小。
7. B0 冻结上限来源、observer.py::memory() 的语义、旧冻结证据保持。新的工具与产品身份独立绑定。
8. 外部数据的完整验证、首次使用成本和合法慢回退都必须可见；缓存命中不能冒充首次或全能力通过。
9. 新公开合同、字段、格式、ABI、profile 或诊断语义需先落实 ADR/Spec/版本决定。内部整改不自动要求公开新接口；确需公开时先停在相应设计门。

| 设计对象 | 协调与状态同步成本 | 所有权/故障成本 | 本方案的控制方式 |
| --- | --- | --- | --- |
| 中性读取块 | 无新线程；仅现有窗口内一次同步调用 | 临时借用可能逃逸；跨行错误可能乱序 | 私有 typed callback、固定小块、每行状态、原序物化；不保存借用 |
| 私有可信 checkpoint | 增加一个受控凭据类型，不增加异步服务 | 来源冒充、mutable alias、闭包遗漏 | 受控 seal 后 mint；精确绑定；不可信入口默认无凭据；恢复独占状态 |
| 不可变前缀 | 增加块寿命计费；不增加可变状态副本 | 最后释放尾延迟、旧 handle 长持有 | 复用现有分块模式；active/retired/final 三段验证 |
| 定向 Close 清理 | 只扩充现有 accepted control/owner ledger 的清理解释 | 状态被误当自授权、迟到副作用、join 互等 | 正常 fence 与 cleanup 权限分开；完整代数核验；可控调度反例 |
| Close 阶段续行 | 增加少量 owner-local 进度与原在途回复类型，不增加第二个协议协调器 | 重启 Close 会重复切输入/退休；通用 cleanup 可能丢掉原 typed reply | 本地只记录已走到哪里；授权与完成仍由现有 coordinator/owner ledger 决定 |
| 动态事务复制 | 改表示会触及 rollback、FaultPrefix、Fold 与 owning 出口 | 减少副本可能丢掉失败前缀，或把物化成本移到每次发布 | R0 独立归因；没有具体写集、回滚和物化证明就不进入该子项 |
| 内部 continuation | 需要保存“记录尚未收尾”的执行位置，证明成本最高 | 同 Tick Fold、partialH、finalization 容易错 | 单独 R3c；没有闭包和等价性证明就不进入实施 |

## 4. 批次、依赖与既有计划的关系

本文的 R0–R4 是性能整改批次；“RPA B1–B5”是既有产品计划；“内存 B0/B1/B2/B3”是 PD 内存取证工作包。三个命名空间分别使用，不互相代验。

| 性能批次 | 所属 RPA 工作 | 依赖与可并行范围 | 退出后能说明什么 |
| --- | --- | --- | --- |
| R0 事实闭合 | B1 观测/基线 | 先静态落点、实际 full/idle/复制计数与工具拒绝门；实验另行批准 | 得到可复核归因，不证明产品性能通过 |
| R1 定向关闭与续行 | B4 的最小垂直修复，生命周期由 B5 补强 | R0 close 字段、四阶段超时及许可反例；不等待整个 Seek 重构 | 对声明故障矩阵证明原 Close 可续行并清理，完成获批 pilot；不等于 B4/B5 全部退出 |
| R2 读取物化 | B1 归因、现有 PD 边界；资源代数与 B4 对齐 | R0 Q4 重锚；可与 R3a 的独立源码部分并行，集成仍串行验证 | 生产读取候选的语义/局部收益证据 |
| R3a 存储与准备 | B2 | R0 Seek 相位；保留已有共享，逐项决定 prepare/派生表、尾块、动态事务、身份。与 R2 共享文件时不得并发写同一函数 | 各获准子项的等价与成本；未证明动态事务表示时不得把它记为已解决 |
| R3b 可信恢复 | B2 绑定与 B3.3 recovery | R3a、R0 支持重放/验证整改、闭包与来源审计、版本差异核查；先验证早期 cut 恢复反例 | 受控 checkpoint 的正确恢复及首次/热路径证据；不自动覆盖长记录或动态副本 |
| R3c 内部长推进续点 | B3.2/B3.3 独立条件设计门 | 仅 R0/R3b 证明单记录尾工作仍阻断预算时讨论；先语义接受再实施 | 合法内部续点的闭包；不能替代任意 Seek 验收 |
| R4 集成与正式证据 | B3 → B4 → B5，最后接 F1–F4 | R1/R2/R3 的适用子批完成、全部 profile/工具门齐备；独立运行授权 | 新行为的正式性能结论，仍需独立设备/生产/owner 接受 |

部署不提前切线程。先在现有 owner 内验证存储和可控协议，再按 B5 接入实际线程。最终正式样本必须来自集成后的同一行为身份，不能拼接“读取用 R2、Seek 用 R3、关闭用旧树”的成绩。

对 performance-modification-plan.md 采用逐项归属，不整份升级为已接受计划：

| 既有 proposal 项 | 本方案处理 |
| --- | --- |
| P0 观测 | 并入 R0 / RPA B1 |
| admissions、FrameObservationLedger、日志/索引和大结果复制 | 对实测关联路径纳入 B2/B3；P7 全态复制与 checkpoint 同根，不能一律排到最后 |
| 固定等待、OwnerChannel 扫描、大回复复制 | 由 B4/B5 按 R0 证据处理；不在本轮另建通道框架 |
| Stop/Reset 生命周期 | 保留 B3/B4 的语义门与定向反例，不借性能整改改变协议 |
| audio owner | 归 B4/B5；真实设备证据仍独立 |

M-TIME、M-TAIL、M-CAP、M-FINAL 均保留。testOnly 参数能支持证明机制，不能补作生产默认容量或生产调度 profile。

### 4.1 R0 残留的批次承接

R0 已按“带残余”口径关闭，历史 partial、`inconclusive` 和未知量保持原样，不随后续批次回填。本表只规定承接归属、被阻塞的结论和销项证据；逐项证据口径以 R0 报告 §8.1 为准，销项写入承接批次自己的报告。

| R0 残留 | 承接批次 | 阻塞什么结论 | 销项证据 |
| --- | --- | --- | --- |
| 旧 fence/cleanup permit 只有固定槽/局部证据；cleanup 与 Close 路径的软件退休共用 `Fence` 槽；当前捕获点在 Close 步骤之前，只导出软件退休记录 | R1 | R1 退出；不得声明旧 fence 跨代、暂停清理、错误 binding/decision 或 Close 步骤本身已被真实 owner 路径覆盖 | cleanup、Close 路径软件退休与 `SessionSoftwareClose` 步骤分开观测；Close 步骤后单独捕获；Close 未执行记 `notExecuted`，已发出未取得记 `executing/resultPending`，只有查询也失败记 `observationMissing`，已执行被拒记 `rejected` 并保留 `closeAttempted=true`、`closeApplied=false`、身份和首错；Close 步骤入口先置 `closeAttempted=true`、`closeApplied=false`，只有 receipt.status 为 applied（含明确合法的幂等 applied 回执）才置 true，`Result` 有值不等于 applied，`StepStatus::failed` 回执不得置 true，拒绝不得沿用旧值。`path` 只作软件退休/cleanup 路由证据，不作 Close 步骤分支证据；Close 步骤用 `closeAttempted/closeApplied` 五态判定。真实路径覆盖正常/错误 binding、decision、generation、旧 fence/新 Close、正常退休增代、合法重传、迟到/重复回执。`permitDirected=true` 不作分支证据；不新增 fence，不扩大 failed Close，failed Close 必须有/无旧 fence 分别拒绝。 |
| `pending_` 队列容量已有反例，但 `exportCloseEvidence()` 的复制没有独立显式上限，现有证据只有注入 allocation/flush 失败 | R1，优先与 Close 失败留证/续行一并验证 | 不得声明导出容量边界通过；若 R1 的失败留证或续行依赖它，则阻塞 R1 退出 | 队列未满/恰好满/下一次拒绝已有真实反例，不重复；导出侧核查是否存在独立上限合同，且只归入 `noIndependentContract`/`contractUnverified`/`verificationFailed`/`verificationPassed` 之一，只有 `verificationPassed` 算通过。合法填满后导出并核对复制量、拒绝码、首因、状态保留；不制造上限或越界写。 |
| 注入 fault 不等于现场设备/宿主根因 | RPA B5 及后续真实设备/宿主验证 | 不自动阻塞 R1 功能验证；阻塞“设备/宿主根因已定位” | 首因、操作身份、owner 相位与设备侧证据的因果链；注入仅作可控回归。 |
| `configValues=0`、S0 sealed identity 未知、无独立公开 DTO 字节复制计数 | R2/R3，仅当对应决策依赖时 | 阻塞“配置复制成本已测得”“产品无身份校验”“DTO 复制成本已量化”等依赖结论 | 决策前补测字段/字节/分配；未知量保持不可得；R3b 独立证明来源与绑定。 |
| Q4 `inconclusive`，历史 B 探针缺失且不可重哈希 | 历史限制保留；R2 建新对照，R4 正式验收 | Q4 已复现、旧窗口已通过、正式 read gate 已过均不得声明 | R2 重新登记当前 A/B/Q4 的树、构建、二进制、窗口；新 B 不套旧身份；R4 按既有 95% 配对门槛重做正式矩阵。 |
| R0 patch 只覆盖 12 路径 | R0 限定关闭；R1 起各批自保 | 不得声明整树/PD-5 其余脏路径可一键回滚 | 每批保存起始字节、既有脏差异、新文件清单，生成独立 patch，验证限定反向恢复；保留 R0 patch/快照。 |

按上表，R1 进入时直接承接第 1、2 行；第 3–6 行不得被 R1 顺手吸收或改写。R0 第 4、5 项仍为 partial，R1 不得用既有 542 条断言替代其许可或容量证据。R1 的执行顺序固定为 **保全起点 → 补观测与可控反例 → 做最小修复 → 验证完整续行和生命周期**。

## 5. R0：先闭合事实，不从总耗时猜根因

| 字段 | 设计 |
| --- | --- |
| ID / 名称 | R0-observation-and-anchor |
| 目标 | 为 Close 给出首因/拒绝谓词证据，为 Seek 给出实际工作量和相位账本，为读取得到当前 F1 上的 Q4 重锚对照 |
| 机制 | 复用既有 Trace/PartScope 的相位身份与相关计数点；新增字段采用编译期可关闭的定长 owner-local 聚合与受控导出。当前同步日志模式另列为有扰动诊断，不冒充无锁实现；普通 allocator 的无插桩对照独立构建 |
| 改动点 | engine/core/internal/cuexis_internal/execution_diagnostics.hpp；execution_host.cpp::seek/cleanup/close（约 850/930/1020）；host_protocol.cpp::suspend/finish（约 705/664）；session_worker.cpp::fence/SessionSoftwareRetire（约 249/787）；playback_session.cpp::makeGameplaySession/seekGameplay/GameplayPublicationAccess::publish（约 3463/4059/3701）；execution_kernel.cpp::prepare/advanceRecorded/restore/seekCandidate；tests/pd/pd5_capacity_probe.hpp；独立诊断分析工具 |
| 不变量 | 不改控制决策、顺序、timeout、输入、fixture、cut 或判据；新增热路径采集不格式化、不动态分配、不同步打印、不引入日志锁；不跨 owner 无同步读取内部状态。既有同步 logger 若仍启用，独立标记其扰动 |
| 等价性证明 | 宏 ON/OFF 使用相同声明轨迹，比较完整 canonical/admission/control、ReplayEvaluation、Snapshot 与错误；编译输出/依赖审计证明宏 OFF 无观察实现进入产品路径 |
| 测试 | 字段完整/重复/缺失拒绝；相位嵌套关系；失败首因只写一次；计数溢出与缓冲满可见；wrong identity 与插桩误作正式样本须被拒绝 |
| 证据计划 | 新目录、新 binding；普通构建作性能对照，插桩只做归因。执行前登记各样本、规模、采集终点、扰动评估和停止条件；任何 40k/ETW/pilot/正式运行均不在本轮启动 |
| 回滚 | 独立 observation patch 可整批撤回；保留已生成诊断产物及源码身份；不删除或改写旧冻结证据 |
| 风险 | 打戳、计数和缓存效应会扰动墙钟；宏 OFF 审计不足会污染正式构建；跨线程偷看会引入数据竞争 |
| 预期效果 | 本批没有性能改善目标。观测开销范围待配对测定，不宣称“零时序影响”；无插桩正式预算不能由插桩绝对时间判定 |
| 依赖 | 静态实施与实验分别按后续授权进入；使用独立 N3 基线并核对差异。R1/R3 只等待各自相关归因，不要求先跑完所有规模 |

### 5.1 Seek 相位与工作量账本

采用固定槽聚合计时，不逐对象、逐 record 打日志；计时器本身的成本和缺样必须记录。以下阶段可以嵌套，应同时保存包络和子相位，报告排他时间时扣除规则固定，不能把它们相加制造总耗时。

**已有观测不是零扰动。** execution_diagnostics.hpp 已有 copy/timers/validate/ledger/reserve/foldCopy/fold/idleDeadline 的 PartScope 聚合，但 Trace 的 begin/end 与按二次幂触发的 progress 会持有 mutex，执行 fprintf/fflush。固定 owner-local 聚合、延后导出是本方案要求的新行为，不能当作当前 logger 已具备。先复用已有计数身份与测试观察量，仅补缺失点；正式计时编译宏 OFF，不能只把运行时开关关掉便声称已消除实现。

| 相位 | 采集边界 | 附带工作量 |
| --- | --- | --- |
| S0 宿主身份 | archive.toBytes 与 SHA-256 各自起止 | 编码字节数、是否已有内部封存身份、分配/复制量 |
| S1 宿主协议 | capture/software/renderer/audio 的 prepare、fence、activate、确认与等待分别起止 | operation、generation、参与域、队列与等待原因 |
| S2 候选准备 | makeGameplaySession 与内核 prepare，区分外层和内层；单列 phaseOffsets/timerRequirements 构建 | entities、requirements、matcher、timer/索引元素数；prepare 调用、有效候选和被丢弃候选各多少次；PreparedGameplay handle 与配置值复制分别记 |
| S3 校验与恢复 | archive shape、history/certificate/identity、restore、完整比较分别起止 | actual records、batch inputs、control/horizon 数；sameKernelResult/sameSnapshot 的调用和遍历元素；嵌套 result 的元素和字节，不能仅计 records |
| S4 内部执行 | submitCanonical/controlRpa/advance；draft、fault reserve、Fold clone、finalization 分开聚合 | 实际 full/idle Tick、timer、matcher 步、Fold、facts；各点 RuntimeState 复制次数、元素/嵌套 payload 字节与容量峰值；单条 record 的内部工作峰值 |
| S5 发布与替换 | query、projection rebuild、Runtime 发布、swap、旧状态释放分别起止 | 对象/Fact 数、复制/分配/释放量，retained 旧 owner |
| S6 时钟与画面 | 新 context/clock 绑定、sample、Snapshot、present-return | clock 有效性、新 generation、目标画面身份、端到端总时长 |

复制计量区分 RuntimeState、ReplayRecord 尾块、RecoveryInputs/配置、公开 DTO 和 canonical bytes；聚合自身不遍历大状态来估算大小，优先在相应容器实际构造/复制点记录已有 size/capacity，并明确字符串等嵌套 payload 的覆盖。full/idle 使用现有 work observation 的定义。静态发现多个 clone 不等于每 Tick 固定复制若干次，也不等于它们已被证明是 6.37 s 的主因。

先用较小但结构相同的可控 fixture 验证探针与计数语义，不能据其外推 40k 是否通过。40k 相位采集仍需独立授权、完整规模和新的诊断构建；不打开或重跑冻结二进制来补字段。

### 5.2 Close 最小可观测集

裁定 F5：采用内部只读诊断通道。以下是 R0 待实施的观测要求，不是现有 ControlReceipt 或公开 Playback API 已提供的字段。预分配 owner-local 失败快照，保存：

- 首个 flush/delivery 错误及其最初阶段；之后 fence 错误另记，不能覆盖首因。
- operationId、decisionOrdinal、controlSequence、kind、status、完整 context/scope/generation/candidate。
- 路径标识、是否实际调用正常 fence、已有 fence 是否存在；实际 fence 调用的拒绝位图记录 operationActive、已有 fence、无 accepted control、请求不符、status 不符、类型不符、cut 缺失。清理许可另记 accepted binding/cut/isolation/ledger 的校验结果，不合并成一个“fence 通过”；已有 fence 的完整身份单列。
- cut、连续 delivery 的已确认末端和未 ACK 范围、close evidence 的计数与 owning 标识。
- audio stop/isolate/callbackIsolated，各参与 owner 的 retire/close 终态、worker exited/joined。
- coordinator 内部状态与门面对外状态，尤其 pump 期间所见 Closing/Closed。
- 本地 Close 续行阶段、原在途 job 的命令/回复类型和归属；coordinator 是否已 finish、confirmation/shutdown/exited/join 各是否确认。
- 外部首个 Close 请求时间、统一截止、各阶段实际等待及首次超预算；重入续行不得重置这条记录。

字段映射在 R0 编码前固定；新诊断只记录既有控制流，不担任 §6.3 的续行状态机，不得用于授权清理或推进业务：

| 字段组 | 权威来源与采集点 | 诊断载体与读取方式 | 当前缺口 |
| --- | --- | --- | --- |
| Close 请求时间、阶段、原 job/回复类型、finish/confirm/shutdown/exited/join | ExecutionHost::close/cleanup 与原 session/audio 等待、确认处；在前置 cleanup 前记录首次请求 | 主线程 owner 的定长快照；只读复制已记录值 | 无独立 Close 阶段字段；不能用已有 worker 测试推定四类续行已实现 |
| 首个 flush/delivery 错误及发生阶段 | ExecutionHost::flush 和 Close 首次捕获错误处；后续错误另列 | 首因固定槽 + 有界次因字段；只记录稳定码/枚举及原 ID，不在热路径格式化日志 | closeDeliveryError_ 不构成完整的首次发生阶段快照 |
| 正常 fence 拒绝、已有 fence、清理许可路径 | SessionSoftwareRetire 路径选择、实际 enterExecutionFence 拒绝处、retireSoftware/retireExecutionStep 校验处 | 各 owner 本地位图/适用性/有效性字段，通过内部有界回复导出 | 无独立拒绝位图；跳过 fence 不能伪造空位图或 fence 通过 |
| accepted request、scope、decision、controlSequence、expected/current generation | Playback acceptedHostControl/acceptedHostDecision/current generation；原请求和退休回执 | D2 owner 串行采集只读副本，含来源身份；不从 proof.status 反推 | 请求等式不等于额外 decision/当前代数比较已经执行；必须分项标记 |
| cut、未 ACK 范围、隔离和步骤终态 | Capture/ControlReceipt/各 owner step ledger 与原 typed reply | 复用已有权威值；每份快照携带 operation/step/scope/generation 关联 | 不新增第二份可变 ledger；字段可读不等于跨 owner 同时快照 |

所有新增记录均为编译期可关闭的定长 owner-local 数据；仅 owner 自己更新，外部只读导出，不触发新控制、正常 fence、ACK、计时重置或状态迁移。不引入全局观察器、跨线程裸读、公开 API/ABI 或新的 SDK 诊断语义。需要跨 worker 取值时只走有界内部通道，明确 reserve、reply 归属、释放与停止后的可用性；优先复用已确认足够的预留故障容量，未核实容量不能宣称失败时必可导出。

裁定 F1 的证据规则：只有记录证明本次走“无 fence 清理许可”分支时，fence 对象/代数及本次未执行的 fence 检查才标记不适用；既有 fence 的身份仍须记录和核验。历史运行或当前构建缺少路径/拒绝字段时标记缺证据，不能事后填不适用或 0。清理许可检查失败另记原因；通道不可用、溢出或字段缺失均为归因证据不足，不能解释为没有 flush/fence 失败。

R0 退出该观测项前，验证字段到源码分支的映射、首因不覆盖、无 fence 的不适用与缺证据区分、有/无旧 fence、导出容量失败和宏 OFF 的构建差异；诊断不改变原返回、消息次序、预算或清理权限。普通 allocator、诊断宏 OFF 的独立构建才可承担正式测时，不能用 runtime toggle 冒充插桩不存在。

### 5.3 Q4 当前 F1 重锚

必须先重锚，旧阶梯的 1.1404 只能作历史线索。这里“F1”指当前修复源码锚点，不是重新运行 F1 collector 工具包 r4。

1. 独立诊断 worktree 和新构建目录；精确记录当前正常 B 的 HEAD、dirty diff、构建选项、编译器和二进制。Q4 只加入原物化直读诊断差异；若不能无歧义迁移，先报告冲突，不把新增差异包装成“原样 Q4”。
2. 使用 tests/pd/measurement_method_tests.cpp 的公开 reusable extractFrame/read_long 路径。N=64 为主，保留原 viewport、资源、输入和 64-step fresh-session 预热；30 秒机器热身、每进程 8 块、每块 ≥100 ms；普通 allocator、无其他归因插桩，固定原核心策略。
3. 预声明一次独立诊断窗口，3 轮 A/B/Q4 进程组，次序为 A-B-Q4、B-Q4-A、Q4-A-B；A 是原 pre-PD 对照，新建可复核构建，B 是当前 F1 正常语义。每轮产生 B/A、Q4/A 与 Q4/B 的诊断对照；不增加轮次到通过。若 A 无法合法重建，只能报告 Q4/B，不能宣称重现 Q4/A=1.14。
4. 只把点估计和有限配对区间作为诊断，不算正式 3 窗口证据。若约 1.14，仍不能推论中性生产实现会达到它；若 ≥1.19，停止沿用旧阶梯的收益分解，先复核构建/成本结构。1.19 是此前交接约定的调查触发点，不是新验收线。
5. 历史 R0-b 的 owning record 批量化已经证明不足（136 B 是当时报告口径，非当前 sizeof 登记），不把同一机制换名再做。Q3 丢 owner/serial/expiry，继续排除。

从 1.1404 到 1.10 需再减少 0.0404×A，约 Q4 成本的 3.54%；这是差额计算，不是可获得收益的证据。

### 5.4 归因后的选择规则

| R0 证据 | 进入的候选 | 必须同时排除的误判 |
| --- | --- | --- |
| prepare/派生表重复次数与普通构建消融显示可删成本 | R3a 准备去重；只共享尚未共享且已证明静态的表 | PreparedGameplay handle 已共享；动态 matcher/fault reserve 不可混入 |
| 尾块 batch/admission 值复制有可归因成本 | R3a 尾块内部表示的最小子项 | capacity=32 不约束嵌套 payload；整块历史已有共享 |
| full Tick/control/finalization 动态副本使剩余预算不能满足 | R3a 动态事务表示设计门 | idle fast path 已有；删 fault reserve 或每 Tick flatten 不是合格修复 |
| 已认证前缀重复执行/比较占用预算，来源与恢复闭包可证 | R3b 可信恢复 | 仍要算动态恢复、后缀副本、外部首验、发布与首个目标画面 |
| 单条 advance 内部尾工作仍超剩余预算 | R3c 独立 continuation 设计门 | 记录级周期 checkpoint 不等于内部进度有界 |
| 固定身份/协议/发布/时钟/画面已不能满足预算，或所需表示无法保真 | §12.2 裁定包 | 不能继续堆 checkpoint、移出计时边界或据此放松合同 |

这些行可以同时成立；逐项有对照才能合并收益。先做小规模结构/计数验证，再按独立授权的完整规模确认；没有数据时保留“候选/未定”，不默认全部实施，也不按相位诊断的绝对时间直接验收。

## 6. R1：定向清理许可与同一次 Close 的阶段续行

实现位于准入树，主树未合入。裁定 F2 保持 R1 一个总批次，不新增 R1a/R1b。以下为实施过程历史说明，当前 R1 退出结论见本地记录 §11。原裁定时“清理许可局部实现/单元证据已有”“§6.3 四类中断续行未实施”是历史状态；2026-10-25 两部分均已有新实现和绑定当前源码的 Release 回归，见上方本地记录，不使用旧 542 assertions / 4 cases 代替。§6.3、§6.4、相关 R0 字段和适用门禁按新记录逐项核对，pilot 与 owner 退出仍独立，不重命名剩余工作来提前记完成。

R1 进入后按 **保全起点 → 补观测与可控反例 → 做最小修复 → 验证完整续行和生命周期** 推进，不跳阶段。§6.3 的四类中断必须分别实现、分别留证；任何一类通过都不抵消其余类别失败，542 条断言也不能替代其中任何一类。R0 第 4、5 项仍为 partial：第 4 项的许可/观测由 R1 直接承接，第 5 项若被 R1 的失败留证或续行依赖，同样阻塞 R1 退出。

| 字段 | 设计 |
| --- | --- |
| ID / 名称 | R1-directed-close-cleanup |
| 目标 | 消除已有 Close/suspend/retire 授权矛盾；覆盖 Close 接受前、接受后、finish 后和 shutdown 后的续行；所有必需终态前保留 Closing |
| 机制 | 从 D2 accepted binding、decision 和 owner step ledger 推导有限 cleanup 许可；增加 owner-local 单调进度，按原 job/operation 消费回复并继续。正常 fence 保留 preparing 条件；Closed 在必需退出/join 后可见 |
| 改动点 | playback_session.cpp::enterExecutionFence（约 1730）及 retireExecutionStep（约 1961）；session_worker.cpp::fence、SessionSoftwareRetire、SessionConfirmControl（约 283/867/484）；host_candidate.cpp::retireSoftware（约 217）；execution_host.cpp::cleanup/close/failed/state（约 1027/1165/354/1506）；host_protocol.cpp::suspend/finish（约 705/658） |
| 不变量 | 保留首次 delivery 错误与未准入输入；核对完整绑定和 cut；音频隔离前不退休软件；无清理许可的普通 suspended/failed 不得继续执行；不伪造 ACK 或回滚已经发生的副作用 |
| 等价性证明 | 同可控轨迹比较完整 owner 历史和 Close evidence；许可与四阶段中断分别做反例；原 job/operation/step 一致、已应用副作用不重复、generation 单调核验；完整有向路径持续到退出/join |
| 测试 | preparing 正常关闭；flush 失败有/无 job；cut/隔离缺失；错 operation/seq/scope/gen；旧 fence/旧非 Close 失败；failed Close 无/有旧 fence；inspection/archive 原 typed reply；迟到/重复 receipt；合法重传幂等与退役 generation 只增一次；四阶段 timeout；pump query/reentry；旧 handle；共享总预算与超时后保活/exit/join |
| 证据计划 | 先同线程可控调度与 headless 小规模；后获批一次有限 pilot，明确功能与 ≤1000 ms Close 边界。40k 及真实 audio/GPU 另列，不能由小规模成功替代 |
| 回滚 | 先保存 64 条 dirty 字节、预计触碰的干净文件起始字节和新路径不存在记录；R1 独立 patch/差异清单；在一次性副本走“起始哈希 → 正向 check/apply → 结束哈希 → 反向 check/apply → 起始哈希恢复”；仅撤本批，不重置整个 dirty 工作树。已运行失败仍保留；回滚后新实验身份不能冒用原 binding |
| 风险 | status 自授权；旧 fence 跨代；重复 Close 或 SessionInspect 发往已关闭 owner；通用 cleanup 丢掉原回复/漏记增代；suspended 掩盖 Closing；提前 Closed；析构捕获失效引用 |
| 预期效果 | 针对已证 fence 矛盾和未闭合续行路径；后者仍需动态反例确认。故障概率和延迟区间未知；一次 pilot 成功只支持该次声明样本 |
| 依赖 | R0 close 字段、可控反例、RPA B4 清理合同；实际线程/设备由 B5 补证。若修复需要新公开取消/清理 API，停止扩张，先提交最小接口与版本决定 |

### 6.1 已证矛盾与未证历史根因

原设计基线中，close 的 flush 失败分支保存 closeDeliveryError_ 并调用 HostCoordinator::suspend。后者把 receipt.status 设为 suspended，Close 的协议状态仍是 closing。SessionSoftwareRetire 原先先调用 fence；PlaybackSession::enterExecutionFence 却只接受 preparing。后续 retireSoftware/retireExecutionStep 原本允许 Close 的定向清理并核对 capture cut/audio 隔离。因此“前置 fence 比清理动作本身更窄”是该基线的静态矛盾。当前准入树的 worker 局部差异已对 suspended、以及非 Close 的 failed 分支跳过正常 fence；本轮将其准确命名为“受限清理许可候选”，不是新增或放宽了 fence。

它还不是历史 40k 失败的根因定论：旧日志没有首个 flush 错误、proof 和拒绝位图。R0 应能证实或否定这条链；不通过改一条 if 就追认旧失败已解决。

另有独立的静态风险：close() 每次从 cleanup() 重新进入，再发起新的 Close；当前没有保存本次 Close 进度。generic completeCleanup 明确拒绝旧 kind=Close；Close 已在 coordinator finish 后，再次 accept 也被拒绝。因此，不能仅凭“旧 Tail 超时后再次 close 能成功”的测试源码，推断 Close 自己在任意阶段超时都能恢复。这条链尚未在本轮运行复现，不与历史 control_pending 直接画等号。

观测捕获点还有一层必须写进 R1 设计：当前 `rpaCaptureFence()` 在 `execution_host.cpp:1330` 的 `SessionSoftwareRetire` 之后、`:1386` 的 `SessionSoftwareClose` 之前调用。因此 R0 的 `rpaClose_.fence` 是 Close 路径上的**软件退休**记录，不是 `SessionSoftwareClose` 步骤记录；`:925/:949` 写入的 `closeAttempted/closeApplied` 没有被之后导出。R0 的 `path=normalControl` 活体断言只能证明软件退休走了正常 fence，不能证明 Close 步骤路径。R1 必须在 Close 步骤之后单独捕获，并把该缺口计入第 4 项残余；不得用当前记录补写 Close 步骤证据。

### 6.2 许可矩阵与最小改动范围

| 情况 | 推荐规则 |
| --- | --- |
| 正常 Pause/Resume/Seek 等提交 | 保持 preparing、当前 owner、完整 accepted binding、generation 与既有阶段前置 |
| 本 owner 已接受的 Close，preparing | 走既有关闭规则 |
| 本 owner 已接受的 Close，suspended，处于明确 cleanup/retire 路径 | 取得受限清理许可，不建立正常或新增清理 fence；沿 retireSoftware → retireExecutionStep 复核 accepted request/owner binding、capture cut、audio stop/isolate 与 owner ledger。scope、decision、sequence、generation 分项列证据，不能因 request 等式成立就将其全记通过 |
| 任意非 Close 的 suspended/failed | 不因这个状态自动获得正常 fence；只在既有清理合同证明“旧已接受操作仅退休/卸载”后考虑内部清理路径 |
| Close 的 failed 状态 | 首版不顺带放行；先给出来源、副作用和故障反例，并分别覆盖无旧 fence、有旧 fence 两条真实 worker 路径，同时覆盖 Close 步骤入口本身（软件已退休/旧 fence 已满足时 `SessionSoftwareClose` 仍拒绝 failed proof）；`fence()` 返回 existing 不能当作许可。当前已知主链是 suspended |
| 已有 fence | 不能直接 return existing。普通提交要求完整同一绑定；Close 退休旧未解 fence 单列兼容规则，不把旧 fence 当本次普通提交凭据 |

最小实现先在现有函数内区分 Close 的 cleanup 资格，复用已存在的 request/owner 状态，不新增公开签名。可信依据必须是 D2 保存的 acceptedHostControl/decision 与本 owner ledger；proof 是证据载体，传入的 status 不能自授权。cut/isolation 的现有校验继续存在，拒绝时保留原错误和原因。

裁定 F1 选择清理许可路线的原因：当前退休/卸载已经有 accepted binding、owner 步骤幂等与隔离检查，新增 fence 会增加生成、复用、解除和超时保活责任，尚无证据证明必要。该选择只取消“必须创建一个清理 fence”的机制要求，不删除正常控制 fence 或 scope/代数/操作绑定合同；见实时宿主边界 §7。没有 fence 位图的现状按 §5.2 记观测缺口，不能靠改名记为验收通过。

绑定检查须沿完整调用链记账：host_candidate.cpp::retireSoftware 先比较 acceptedControl 与 proof.request、target、operation、参与方和阶段；PlaybackSession::retireExecutionStep 再调用 validateExecutionControlBinding，后者显式核对 owner、sessionScope、operationId 与 accepted target。因此不能笼统记“整个退休路径没有 scope 比较”。但已有 request/target 等式也不证明 proof.decisionOrdinal 与 D2 acceptedHostDecision、或退休前当前 generation 与 expected generation 的独立一致性检查已完成；这些仍需来源映射与错值反例，缺项时保留 R1 退出阻断。

旧 fence 的控制序列不能一刀切改成“必须等于新 Close”：当前 completeExecutionClose 已允许新 Close 退休较旧 fence，并要求 Close sequence 更大；同序列 Close 还走已有 leave/confirm 路径。实施必须分别测试这两条路径和代数，不扩大正常提交权限。

generation 按阶段核验：取得清理许可前，检查已接受请求的 expected software generation 与原 owner 步骤状态；不能用“没有新 fence”免除当前代数校验。退休 unload 会合法增代，旧 fence 保留的是退休前 generation。终态核对实际退休回执的新 generation，不能要求退休后的 current generation 等于旧 fence generation；重传已终态步骤须按原 ledger 结果处理，不能以退休后的代数误拒自己的重传。增加“正常退休增代仍可关闭”与“旧控制回执不能释放新 Close fence”测试。

裁定 F4 为记录性限制：worker 只将 suspended 或 failed 且非 Close 导向“跳过 fence”分支；retireSoftware 的 directedCleanup 及下游 retireExecutionStep 仍把 failed 作为可选清理状态，但还要求 accepted binding/cut/isolation/ledger。当前非测试调用方是 worker，tests/rpa/rpa_b4_tests.cpp 还有直接调用；不能称底层合同已统一收窄。worker 不跳过 fence 也不等于显式拒绝 failed Close：其 fence() 可先返回已有 fence，故有/无旧 fence 的 failed Close 仍要反例验证。首版“不放行 failed Close”是待闭合要求，不是已获证的全路径性质。本轮不修改这些谓词；新增调用方、扩大清理场景或关闭此缺口前，单独审查将资格检查统一或下沉的最小变更，不能顺手扩大。

特别前置：close() 一开始调用 cleanup()。若旧非 Close 控制在进入 fence 前失败，cleanup() 会 suspend 它，然后同样发送 SoftwareRetire。只修 Close/suspended 不能自动修好这条路径。R1 必须先覆盖这个反例；若失败，仅对“旧已接受操作的取消后退休/卸载”设计单独内部清理许可，保留不可提交条件，再评审；未解决时 R1 不得记退出。不能因此一次放宽所有 suspended/failed。

### 6.3 四类中断必须续行原操作

以下四类的统一 Close 续行机制尚未实现；已有清理许可测试不提供此退出证据。继续实施时先补对应受控停顿反例，不把清理许可 green 当作跳过本节的前置。

当前 `tests/rpa/rpa_b5_host_tests.cpp` 的 `B5 deadline retains original tail job until pumped Close completes`（`[b5][host][fault][deadline]`）阻塞的是旧 Tail：第一次 Close 等待旧 job 超时，随后释放它，再次 Close 成功。它覆盖下表第 1 类；本轮没有运行该测试，也未发现它覆盖第 2–4 类。行号会随 R1 改动漂移，定位以测试名和 tag 为准。现有 Busy guard 已拒绝重入，保持它即可；问题是返回后的下一次 owner 调用如何续行。

| 中断位置 | 下次 owner 调用的合法动作 | 需要拦住的反例 |
| --- | --- | --- |
| 1. Close 接受前，旧非 Close、归档或 inspection job 在途 | 先按原 job 查询并消费原类型回复；旧非 Close 按既有合同清理；完成前置准备后只接受一次 Close | 丢掉 inspection/archive 回复；重复准备覆盖原 control；把旧清理回执当成新 Close 回执 |
| 2. D2 Close acceptance 在途，或 Close 已接受但 owner 步骤未完成 | 保留 acceptance job 与 request；确认原 decision 一次；之后按原 operation/step 与 owner ledger 续 cut、delivery、隔离、退休、关闭 | generic completeCleanup(Close)；第二次 begin Close；重复增 pauseEpoch、重复切输入/导出证据或重做已应用副作用 |
| 3. coordinator 已 finish，SessionConfirmControl 尚未确认 | 保存 completed proof/finalized context；先收原 confirmation 回复；尚未 post 时才 post 一次；接着 shutdown | 再走归档、SessionInspect、退休或 HostCoordinator::accept；把协议终态冒充线程退出 |
| 4. shutdown 已请求，等待 exited/join | 只泵尚需主线程的事件、检查退出并 joinExited；全部完成后设置门面 Closed | 向已关闭/退出 worker 发新业务命令；重建 Close；超时后析构仍活跃依赖 |

最小实现是在 ExecutionHost 内保存一个单调 Close 进度和固定的在途回复归属。至少能区分前置清理/准备、接受与 owner 步骤、协议完成待确认、shutdown 待退出、已 join；owner 步骤的授权和幂等终态仍取自原 coordinator/owner ledger。本地进度只选择下一步，不能自行证明 accepted、isolation 或 retired；不另建平行控制器，也不新增公开 cancellation API。

session()/audio() 超时保留原 job 的现有行为继续使用。迟到回复必须按原命令类型处理：inspection 的 generation、归档、acceptance decision、StepReceipt 和 confirmation 不能都交给只识别部分 variant 的 generic cleanup 后丢弃。结果消费、ACK 与阶段更新保持原有通道语义；不能为了续行提前 ACK 尚未正确归属的结果。对退休回执同步核对 software/audio generation：现有 observed() 会更新本地 generation，而 cleanup 有直接 host_->observe 的路径，需可控晚回复测试确认二者一致，不能预先宣称已复现增代缺陷。

记录已发命令、原 job/operation/step 与其终态后，重试只查询原 ID；若需重传，保持同一 ID 与载荷。archive/cut/evidence 只在其原阶段取得和固定；已经取得的 owning 证据保留。错误阶段留下原错误及未完成责任域，不能为了进入下一阶段假造成功。

四类测试之外，第 2 类还需分别在四个停顿点受控停住：acceptance/request 已准备但尚未 post；已 post 但未取得 decision；已取得/接受 decision 但原 typed reply 尚未消费；已接受但 cut/delivery/隔离/退休/关闭未完成。验证原 ID、实际副作用次数、generation、未确认输入、状态可见性及最终 join。第 1 类通过不抵消其余类别失败，2d 也不能代替 2a–2c。

### 6.4 Closing、Closed、共享预算与超时保活

本次未发现 close 成功返回早于 join：现代码先等待 exited，再 joinExited，最后 closed_=true。但 HostCoordinator::finish 先设置内部 Closed，而外层还在等线程退出；ExecutionHost::failed/state 又可能用 suspended_ 掩盖内部 Closing。应在 pump 回调内查询做反例，然后把对外状态终结点与所有必需 owner 退出/join 对齐，或以内部门面状态明确屏蔽提前的 Closed；不只修最终 return 值。

保持宿主合同的有向顺序：K1/停止新请求 → 固定可取得归档与未准入输入、取消候选 → 音频停止并确认 callback 隔离 → 各 owner 退休/释放 → 线程退出与 join → 报告 Closed。缺少哪域必须可查。

若超时，保留 Closing 和仍被访问的全部依赖，继续泵清理/故障回复；不增加 timeout、不 detach、不强行释放。SessionWorker/AudioWorker 析构当前直接 join，不能把“扔进 quarantine vector”当完整修复：配置 callback 若捕获栈变量、renderer 或控制器引用，移动 unique_ptr 仍会悬空。只有能证明现有会话 owner 连同 callback captures 的生命周期闭包都可保活，才能继续这个实现；否则交回最小生命周期裁定，不临时引入全局回收服务。

Close 的 ≤1000 ms 从外部首个关闭请求起算，时间戳和续行记录必须在前置 cleanup 之前建立，覆盖 renderer 撤销/归档、协议等待、退出和 join。保留原 transport replyDeadline；各次等待还要受 Close 总剩余时间约束，不能逐阶段起一个新总预算。现有 awaitSession/audio/cleanup/exit 各自起算的 deadline 是本次必须核对的落点。

超过总预算时记录该次延迟失败，返回未完成的 Closing。原截止时间不能让后续清理永远无法推进：之后的 owner 调用仍可按同一阶段、同一 ID 非阻塞查询和推进，必要事件继续由原 owner 泵送；等待片段保持有界。最终完成可报告资源已关闭，同时保留首个请求超预算的事实，不能以重置计时的“第二次成功”替换它。此设计区分时间接受与最终安全回收，不需要延长合同 timeout。

超时后的保活责任落在原调用方/宿主 owner，须覆盖整个 callback captures 闭包。当前 player_execution.cpp 的 HostLifetime 析构会 host.reset，随后 worker 析构可能阻塞 join；因此不能在 close 返回错误后立即走该路径，再声称已保持可泵 Closing。R1 需要在现有调用方作用域内证明继续保活和泵清理，若做不到就停在最小生命周期裁定门。保留 unique_ptr 本身不是完整证明。

pilot 可采用此前交接中 owner 已选的“关闭失败记 invalid-process、整窗最多重跑一次”规则，但只适用于明确登记的 pilot、实际执行仍需授权。原失败与重跑原因同时保留；不得扩展为正式慢样本、性能超限、身份错误或语义错误的替换规则。

### 6.5 R1 执行清单与停审

按 [R1 实施与验证计划](r1/plan.md) 执行：先检查文档，再保全起点、只拆观测、补旧行为反例，随后才做逐项最小修复。C6 第一优先；C6 失败，或 C7/C7a 不能证明 failed Close 在无/有旧 fence 时被拒绝，均先留证并提交具体最小变更审查，不能把候选修复表当授权。failed Close 的 Close 步骤入口也必须拒绝，不能只修 retire 层。

该执行计划集中登记三类观测与 Close 五态、C0–C10、四类续行（含 2a–2d）、显式 started/固定 1000 ms 总预算、导出容量四类事实、ON/OFF 等价、R1-only 正反向回滚和 G0–G11。G0–G11 只支持“实现及本地验证完成，提交 R1 退出评审”；有限 pilot 另需具体计划及明确授权，适用回归、架构/消费者门和 owner 退出评审未完成前不得记 R1 退出。

## 7. R2：有界借用行，原序写入最终 Snapshot

2026-10-25 实施进度：用户本次授权主代理设计、规划并实施 R2；首版候选、五构建回归、适用消费者和独立代码补丁验证已完成，唯一局部诊断已完成：单对象退化、N64 收益区间跨 1，当前候选未接受，R2 收益门未完成。实现与证据见 [R2 本地记录](../../../../stage_reports/stages/stage-07/05-rpa-new/r2/2026-10-25-r2-borrowed-presentation.md)。以下设计与门禁继续适用；不将本地实现写成 R4 正式通过。

| 字段 | 设计 |
| --- | --- |
| ID / 名称 | R2-borrowed-presentation-rows |
| 目标 | 删除每对象值记录中的矩阵中转、Snapshot 路径未使用的 Renderable 检索及可证明冗余的物化工作，保留全部读取保证 |
| 机制 | Runtime 在现有同步窗口内产生固定小块的 typed borrowed rows；Playback 按原序直接写最终 owning FrameSnapshot；复用现有 SnapshotLayout |
| 改动点 | engine/runtime/internal/cuexis/runtime/presentation_read.hpp；engine/runtime/src/presentation_read.cpp；engine/playback/src/presentation_materializer.cpp::fillObjects（约 53）；presentation_materializer.hpp；playback_session.cpp::buildSnapshotLayout（约 917）。新增内部 visitSnapshotObjects/ObjectVisualRead；原 readObject 保留 |
| 不变量 | owner/完整 stamp/serial/epoch/expiry/reentry/mutation 检查保持；外层与行内错误顺序保持；公开 DTO 每次恢复全字段；引用不逃逸；材料以当前 Appearance 为准 |
| 等价性证明 | 新旧路径对同一只读状态比较全部字段、默认值、resource identity、对象数、错误码/上下文/先后次序和部分写入行为；FrameDigest 仅作补充 |
| 测试 | 现有 Presentation/Playback 测试扩展借用边界、跨行错误、调用方污染 DTO、资源 override、相机、跨会话/重载/Close；消费者、架构与公开头 ASCII 门保持 |
| 证据计划 | 先 R0 Q4 重锚，再普通 allocator 的新旧生产候选配对诊断；固定 N=64 主点及原已登记规模，成功后才进 R4 原正式矩阵。Q4 本身不作为产品候选 |
| 回滚 | Runtime 内部接口和 Playback 物化调用作为一批撤回，恢复原 readObject 路径；不动公开头和已冻结实验 |
| 风险 | 临时矩阵地址悬空；块级预检抢先报告后行错误；借用被缓存；过大的 scratch 增加栈/缓存成本；单材料快路径忽略动态 override |
| 预期效果 | 收益区间未知；已有代码能证明删除哪些工作，不能证明节省多少 ns。保持 95% 区间上界 ≤1.10 目标；局部收益不足时不扩张成缓存框架 |
| 依赖 | R0 当前 F1 重锚；现有中性同步借用合同；若需借用跨窗口或公开 DTO 布局变化，停止本批另过合同门 |

R2 的 Q4 重锚必须建立新的证据身份：对当前生产候选重新登记树、构建、二进制和窗口，不得复用 R0 的 A/B/Q4 身份，也不得把历史缺失的 B 探针补成旧窗口。R2 只产出候选对照与局部收益证据，正式读取验收仍在 R4；Q4 本身不作为产品候选。新 B 无法建立时，保持 R0 的 `inconclusive`，不能用旧 A 或新构建拼出新结论。

### 7.1 内部数据流与边界

裁定 F3：首版固定 K=32，但它不是已测最优参数，也不是已接受的字节预算。136 B 保留为 F1 冻结反汇编报告的历史口径；128 B 仅为当前头文件及特定布局假设的推算。两者都不能直接作为当前 ObjectPresentation、Result 返回包装或借用块 scratch 的 sizeof；也不据此反推旧证据错误。

R2 首个提交必须随同构建登记源 SHA/dirty diff、编译器与标准库、目标架构、Debug/Release 和编译选项，现场取得 sizeof/alignof(ObjectPresentation)、sizeof/alignof(Result<ObjectPresentation>)、sizeof/alignof(ObjectVisualRead) 及完整 scratch aggregate。对已登记的目标布局设置匹配的 static_assert，并对 scratch 总预算设置编译期上界断言；不把某工具链的 128/136 硬编码成所有 ABI 的承诺。登记须来自当前实际类型，不能另造替身结构算尺寸。

完整 scratch 包含 K 个索引、K 个借用行、固定辅助状态和真实对齐/padding；用实际 aggregate 的 sizeof 或逐项保守汇总且明确未覆盖的栈开销，不以 K×旧值记录大小代算。布局断言是变更报警，不是性能收益或整函数栈峰值证明。缺少现场值/预算断言的 R2 首个提交不得通过入口。实验不现场扫描 K 直到过线；若后续确需改变，作为有机制解释的新候选重新绑定。

内部接口形态如下，仅表达职责，不是新增 SDK API：

~~~cpp
auto visitSnapshotObjects(
    std::span<const std::size_t> indices,
    void* context,
    core::Result<void> (*consume)(void*, std::span<const ObjectVisualRead>))
    const -> core::Result<void>;
~~~

indices 的来源必须明确：SnapshotLayout 存的是 vector<SnapshotEntity>，runtimeIndex 是结构体中的字段，不能直接当成连续 span。由 Playback 以现有布局顺序分块，将本块至多 K 个 runtimeIndex 收集到栈上 std::array<std::size_t, K>；Runtime 拒绝超过 K 的调用，并以固定容量行数组完成本次同步 callback。禁止新增持久化索引副本或每帧 heap vector。总 scratch 同时登记索引数组、行数组及固定辅助状态的 sizeof；空集、尾块和溢出必须有定义。

ObjectVisualRead 只包含轻量行状态、ID/material 的 string_view、指向真实 WorldTransform 中 core::Mat4 的 const 指针、presence 和标量。没有 entity、registry、World 或设备对象。矩阵指针不能指向临时 ObjectPresentation、局部 Mat4 或反复覆写的 scratch 矩阵。

每次入口执行现有 available() 的 owner/window/serial/active 检查；外层 PresentationAccess::read 的完整 stamp、CallbackScope、RAII End 以及 const World 约束保持。Runtime gather 只作边界安全检查、registry.valid/try_get 与中性字段装载，必须只读、无分配、无异常。越界先不碰 registry，写该行 status；不在 gather 做 ID 匹配、材料长度/闭包检查、错误字符串构造或相机计算。

Playback 逐行消费，行内保持现有顺序。前行若发生 resource mismatch，必须先返回它，不能被同块后行的越界或缺失错误覆盖。callback 首次失败立即停止，不开始下一块。读取、callback 和物化同步完成；借用不能进入 FrameSnapshot、SnapshotLayout、缓存、消息、旧结果或异步任务。若实施中 gather 无法满足无分配/无异常/无跨行错误聚合，就退回逐对象 typed visitor，不能改用裸 Registry 接口。

整体顺序保持：state/stamp 检查 → 清 camera/clear/viewport → 必要 resize → 逐行对象物化 → 最终对象数恢复 → readCamera/相机计算。callback 抛异常仍由现有模块边界转换和 RAII 结束窗口；新路径不另设吞异常后继续机制。

### 7.2 具体删减与保留

| 项目 | 决定与原因 |
| --- | --- |
| ObjectPresentation 中的 matrix 值（布局按 §7.1 现场登记） | Snapshot 新路径直接从组件复制到最终 DTO 一次；不再先复制进值记录 |
| hasRenderable 检索 | 仅 Snapshot 专用 gather 省去。GameplayPublicationAccess::publish 的目标存在性判断仍使用 readObject/hasRenderable，不能全局删除 |
| 布局 | 使用现有 ID/runtimeIndex/mesh/材料闭包和容量上界；不增加第二套实体或资源索引 |
| 单材料闭包 | size()==1 且唯一 key 与当前材料完全相等时直取引用；否则保留原 find/失败语义。多材料仍使用现有 std::map |
| 字符串/资源引用 | 现代码已有条件赋值，不能宣称消除每帧全部字符串拷贝。保留容量上界、需要时赋值、ID 对照与完整 ref 恢复；不依据“上帧写过”跳过 |
| 每对象 Result | gather 改轻量行状态，Playback 只在对应行生成原错误；不是删除失败分支或弱化错误顺序 |
| Q4 的 const void* Registry 通路 | 只用于归因，不进入产品，不成为中性边界的永久逃生口 |
| last-key memo、跨帧属性缓存、FramePayload 池 | 首版不采用；增加失效、末读者和 Close 成本，且不能直接解释公开 reusable DTO 的 read_long 收益 |

现有 worker 每次构造 owning FramePayload 是另一个调用路径，不能用内部池命中改善来替代正式公开 extractFrame 的验收。

### 7.3 必须覆盖的反例

测试覆盖 wrong owner、escaped view、旧 serial 不能在新窗口复活、重入和 mutation；空集、K 边界、尾块、越界和 size 算术溢出；callback 返回失败/抛异常；预热后的成功路径分配。

调用方将一个可复用 DTO 的所有公开字段污染，包括对象数、ID、mesh/material ref 的 type/identity/assetId、矩阵、visible/opacity/tint、camera/viewport/clear，再次提帧应与新 DTO 的成功结果完全一致。两个 DTO 交错、跨 session/reload、缺 transform/appearance/mesh、Behavior/override 换材、非法/超长材质及旧 owning 值跨 unload/Close 均需覆盖。失败路径也比较与旧实现相同的首错及已写字段。

### 7.4 第二候选：同一窗口内的逐对象 typed visitor

2026-10-25 用户继续授权 R2 后，由主代理决定改用同步单行 visitor，子代理仅实施、主代理亲自核验。首版批量候选因局部退化未接受，旧 K=32 源码与窗口冻结保留；本次不是扫描 K，而是直接删除索引/行数组与两阶段批量搬运。Runtime 在同一读取窗口内借用一个真实组件行，Playback 原序立即写最终 owning DTO；owner/stamp/window/serial、mutation/reentry、首错/部分写入、旧 owning API 和公共 SDK 合同全部保留。

§7.1 的固定块说明保留为首版设计记录，不要求当前候选继续预构造 32 行。当前单行实际对象与消费上下文须分别按真实类型取值、保守汇总并设上界，明确未计入的栈开销。完整回归、包边界、原字段/错误等价与新身份局部收益门继续适用；不得通过缓存、池或 raw registry 扩张。本次实际进度与限定见 [单行续做报告](../../../../stage_reports/stages/stage-07/05-rpa-new/r2/2026-10-25-r2-single-row-continuation.md)，不将子代理实施结论当作父代理核验。

### 7.5 第三候选停止与统一实施入口

第三候选 direct borrowed Result 于 2026-10-25 23:03:20 +08:00 被要求停止（停止快照于 23:03:54 生成）；源码随本次两树整合原样迁入 `D:/Cuexis-worktree`。本次迁移构建与正确性回归完成，消费者沿用证据边界见整合报告，无第三候选性能诊断或收益结论；前两候选及其窗口保持各自历史身份，不能借本次构建结果改判。后续实施只在主树，`D:/Cuexis-pd5-40k-admission` 为只读历史证据树。整合记录见[2026-10-26 报告](../../../../stage_reports/stages/stage-07/90-shared/worktree-consolidation/2026-10-26-worktree-consolidation.md)。本次本地整合/构建测试授权不包含新性能诊断、ETW、40k、pilot、正式窗口或新候选；R2/R4 未退出。

### 7.6 第三候选续接后的停止位置

2026-10-26 用户明确要求继续实施 RPA-R2，覆盖第三候选剩余核验及一次新身份 B/E 固定局部诊断，
取代 §7.5 所述整合批结束时的等待继续状态。技术机制不变，没有新增第四机制。
精确回滚、当前布局、Release 对象码、四规模语义身份和五配置适用回归完成；
N1/N8/N64/N256 仍超过原 15% 噪声门，R2 整体收益门未完成，R2/R4 未退出。
实际结果和全部证据边界见[第三候选续接报告](../../../../stage_reports/stages/stage-07/05-rpa-new/r2/2026-10-26-r2-direct-result-continuation.md)。

第三候选为本批最后机制，唯一三对六进程窗口已完成，旧窗口与全部样本保留。
下一步先处置该结果；不自动追加样本、换样本、改阈值、产生第四机制或进入 R3/R4。
不得由新的 E/B 局部对照拼接旧 A/Q4 窗口得到正式相对 A 接受。

### 7.7 B 基线状态归因的当前承接

用户再次要求继续 R2，并显式要求重新运行 B-only 诊断后，本轮完成一次独立环境诊断和一次
显式重跑。读取变慢与系统报告处理器状态下降同时出现；重跑读取区间报告频率稳定，但
具体触发原因与跨进程重复性尚未闭合。结果见[B 基线状态诊断](../../../../stage_reports/stages/stage-07/05-rpa-new/r2/2026-10-26-r2-baseline-state-diagnostics.md)。
两次都不进入原六进程区间，不新增第四产品机制。下一步针对状态切换的可控条件继续归因，
保留原判据与未接受状态；R2/R4 未退出，R3 尚未启动。

### 7.8 HighQoS 局部收益与公开 N1 剩余项

用户补充旧测量期间运行 IDEA 服务，并明确要求再次 B-only 确认后继续 R2。本轮确认再次出现
默认策略下的处理器报告状态变化，继而以只影响新 native 进程的显式 HighQoS 请求进行
预登记单变量诊断，并为同一第三候选登记一次新的 B/E 三对六进程对照。旧窗口不替换或合并。

四规模均通过原噪声门，N8/N64/N256 局部收益确认，主点 N64 改善约 20.19%。公开 N1 的
95% 配对区间仍跨过 1.10。同进程物化器消融在固定 fixture 中约降低 4.84% 成本，
只缩小疑点，不替代公开提帧验收。当前 R2/R4 仍未退出；详见[本轮报告](../../../../stage_reports/stages/stage-07/05-rpa-new/r2/2026-10-26-r2-high-qos-local-benefit.md)。

下一步限定在公开 N1 的进程间差异与外层读取成本归因，保留已确认的主点收益与第三候选。
不将 HighQoS 请求当作固定硬件频率、默认产品行为或第四产品机制；不由该局部证据接受 R4，
不追加同一窗口的样本，原 15%、1.10/95% 与适用规模保持。

### 7.9 公开 N1 与共用未变目标文件对照

用户继续授权后的限定归因完成：同一公开 N1 会话旧/新物化消融在三进程均改善约 9.0%–9.3%；
outer-only 仅为合成分层，不能替代公开接受。原短块组按门停止、保留，增加 outer-only 固定工作量后另登记。
原 B/E PlaybackSession 外层目标码只在编译单元匿名标记不同，归一化后 12870 个函数一致；
该事实不证明运行时频率、cache、ASLR 或具体后台服务根因。

另登记一次共用驱动及全部未改对象/库、仅替换原 B/E 两个实现对象的四规模三对六进程窗口，
本机 HighQoS / 共用未变目标文件配置的局部收益及不退化门通过，提交 R2 退出评审核对。原独立构建窗口的 N1 仍保持 inconclusive；不拼接、替换或追认。
完整结果/身份/限定见[本地报告](../../../../stage_reports/stages/stage-07/05-rpa-new/r2/2026-10-26-r2-public-n1-and-common-objects.md)。
本机构建配置的局部证据与 R2 owner 退出、R4 原正式矩阵仍分开，不放宽旧阈值或扩展产品机制。

### 7.10 R2 退出报告与证据整理（形成时记录，状态分类由 §7.11 更正）

用户要求编写退出报告、整理证据及文档后，已形成[退出评审报告](../../../../stage_reports/stages/stage-07/05-rpa-new/r2/2026-10-26-r2-exit-report.md)，
附逐文件 SHA-256 清单和轻量证据包。它汇总已测/复用/缺证据、旧失败与停止、原始路径及回滚范围，
不重定义本节与 §9 的门槛，也不记录 owner 已接受。

当前受控局部结果支持保留第三候选；原独立构建 N1 inconclusive、R0/Q4 重锚历史缺口和默认配置推广仍需明确技术处置。
完整退出不能写成只差确认，也不由文档整理自动豁免条件、转移给 R4 或授权补测。
R2 退出裁定与 R4 原正式矩阵保持独立；本次未执行新测量或改变产品机制。

### 7.11 R2 技术收尾与 owner 退出接受

用户要求继续完成 R2 后，已完成[技术收尾复核](../../../../stage_reports/stages/stage-07/05-rpa-new/r2/2026-10-26-r2-technical-closure-review.md)。
新 A/B/Q4 对照及 1.19 触发调查已在 R2 首版按本计划实施，并非尚未完成；本轮核对归档身份、
独立复算 72 个重锚块和 192 个当前局部对照块，新执行五配置共 46 项适用回归，均通过。

§7.10 对“完整退出缺口”的分类过宽：旧 N1/Q4 的不确定结果仍保持原结论，
默认 QoS/其他构建布局的推广仍未接受，但不新增“所有历史窗口必须改判通过”或“所有调度策略必须通过”的 R2 门。
2026-10-26，owner 明确表示“好，那我现在接受R2退出，你可以开始修改文档了”。R2 已按原计划完成并获接受退出；
接受记录见[退出接受 JSON](../../../../stage_reports/stages/stage-07/05-rpa-new/r2/2026-10-26-r2-owner-exit-acceptance.json)。
该决定登记 R2 原定阶段的退出，不缩小原要求，不改变性能证据适用条件或正式验收门。
R4 原共同 A、三窗口九对、15%/1.10/95% 门及设备/生产接受均不由局部成绩代替。
本轮无新产品机制、新构建或性能窗口，旧原始证据/501 文件归档保持不变。

## 8. R3：按归因选择准备、动态复制与可信恢复子项

不选择“前缀 digest 代替完整语义验证”，也不选择“只加密集周期 checkpoint”。R3a 先保留现有共享并分项归因；只有 R0 支持且状态闭包成立时才进入 R3b；R3c 是单记录内部工作仍超预算时的独立设计门。原点重放路径保留为不可信输入验证路径与对照 oracle，但不能以它慢为由缩小适用 Seek。

R3 进入前须保留 R0 的三个未知量，不得把它们当作零成本：`configValues=0` 只表示该字段未导出，不表示配置复制成本为零；S0 `sealedIdentityKnown=0` 不证明产品没有身份校验；当前也没有独立的公开 DTO 字节复制计数，`copyAllocations`/`objects` 是次数或个数，不是字节数。R3a 若以删除 config 复制或身份/哈希工作作为收益来源，必须先补测该项成本；R3b 若以 S0 缺失推断无身份校验，必须先独立证明来源与绑定。未知量不为填表扩大观测范围。

### 8.1 R3a：不可变存储、准备与身份成本

| 字段 | 设计 |
| --- | --- |
| ID / 名称 | R3a-immutable-storage-and-prepare |
| 目标 | 在既有 prepared 共享和分块日志上，去掉已归因的重复 prepare/派生构造/编码；单独判断动态事务与尾块 payload 是否需要整改 |
| 机制 | PreparedGameplay 现成 handle 继续共享；只对重复派生表考虑封存共用；历史整块沿用现有共享、尾部独占。动态事务复制另过表示/回滚门；封存 owner 可保留精确原 canonical 身份 |
| 改动点 | engine/judgement/src/execution_log.hpp（现有 capacity=32 分块模式）；execution_kernel.cpp::prepare/advanceRecorded/controlRpa/query/captureState/seekCandidate；session.cpp::prepare/seek；playback_session.cpp::makeGameplaySession/seekGameplay（约 3463/4059）；app/player/src/execution_host.cpp::seek（约 850） |
| 不变量 | cut 只暴露成功前缀；同 Tick Fold/故障/rollback 保真；旧 owning 结果自足；当前会话配置必须允许 archive；archiveIdentity 仍是原 canonical toBytes 的 SHA-256 |
| 等价性证明 | 各写点/出口列清单；块边界、同 Tick 控制、fault、分配失败前后比较完整结果；新旧当前配置拒绝面相同；所有编码字节和 hash 完全相同 |
| 测试 | 现有 prepared 共享、会话动态态隔离；0/31/32/33 尾块及大 batch/admission；full/idle/reference、Fold/finalization/fault；旧结果跨 Close；配置拒绝；mutable alias；首次编码/失效/最后释放 |
| 证据计划 | R0 分别报告 prepare/派生表次数、各 RuntimeState 副本、尾块嵌套 payload、编码/hash、query/发布/DTO 物化与释放；保留既有 idle 优化的工作计数与完整结果对照；普通构建测时、分域构建测内存 |
| 回滚 | prepare/派生表、尾块 payload、动态事务、身份复用分别有最小可撤差异；未获准子项不混入。保留原路径；不用 git reset/clean 清除既有改动 |
| 风险 | 把可变 vector 当 append-only；共享导致旧大结果长驻；适配层 flatten 抵消收益；绕开当前配置 identity 门；把首次成本藏入 load |
| 预期效果 | 区间未知。目标是删除实测重复成本；不声称消除了所有 O(history)，不承诺 Seek ≤20 ms |
| 依赖 | R0 相位；现有 B2 生命周期/容量合同；每种容器先审计写点，公开布局变化另过 ADR/Spec/版本门 |

#### 8.1.1 准备、尾块与身份：只改剩余重复工作

PreparedGameplay 在 gameplay_prepare.hpp/.cpp 已持有 shared_ptr<const PreparedGameplayStorage>，返回 const 引用/span，string_view 的存储由该 owner 持有。复制这个 handle 不复制整份 prepared。ExecutionKernel::Impl 仍构建 phaseOffsets、timerRequirements 和临时 requirement map，RecoveryInputs 仍复制配置值并复制 prepared handle；prepare 仍有校验、identity、projection、matcher 和 fault reserve。它们不能笼统算成“prepared 深复制”。

已核实的重复调用点：Playback 的 makeGameplaySession 已 configure+prepare，seekCandidate 再 prepare；RPA restore 则在 execution_kernel.cpp:1983 和 1990 各 prepare，一份 initial candidate 没有参与 RPA 分支重放。首选小改是按相位账本去重这些候选构造；派生表确有重复成本时，再决定是否与同一 immutable prepared 绑定共享。matcher、runScope、ingress、active 和 fault reserve 仍为每个运行新建，不能借“静态共享”混入动态态。

去重必须保留 JudgementSession::seek 在 session.cpp:468 的当前配置 identity 拒绝门，及原有参数、版本、内容和错误转换；不能直接信任 archive.dependencies() 绕开当前内容边界。分配次数减少后不要求“第 N 次 allocation 仍失败”，应在仍存在的语义失败点注入，核对 first-fault、FaultPrefix、回滚和错误行为；去掉一次分配不等于删除必要的 fault reserve。

execution_log.hpp 已共享完整不可变前块，append 至多复制一个 capacity=32 的尾块；但复制的是 ReplayRecord 值，其 batch/admission vector 的嵌套数据也会复制，result projection 才是 shared_ptr。需要记录尾块大小、嵌套元素/字节及 append 次数，既不误报“32 个小指针”，也不误报“每次整个历史”。只有实测仍有该项成本，才讨论尾块内部表示；不能重新实现已有分块日志。

同 Tick Fold、quarantine、deferred、active requirement、matcher 和临时 control 状态可能覆写，必须逐个列出写点。复用分块思想不代表 ExecutionLogChunk 可直接承载所有类型；只有已封存数据可共享，可变尾部必须独占。实施顺序是写点/物化出口清单 → 分项归因 → 最小候选消融 → 等价/成本门；不是把所有 vector 换成一种通用持久容器。

host archiveIdentity 的语义不换成 prepared identity、prefix digest 或分块 hash 根。只有内部封存 owner 对精确 canonical bytes/header/count 的不变性成立，才能保存相同 SHA-256；生成、bytes 保留、失效和最后释放都入账。外部对象存在可变别名时仍重新验证/编码，指针相同不是缓存命中资格。

**公开值物化是独立成本。** KernelProjection/SnapshotDTO 目前含完整 vector 与历史。新内部共享不能自动让完整 DTO 的构造、编码或旧值维持免费。现有 ExecutionKernel::query 返回共享 owning storage，GameplayPublicationAccess 已通过 PublicationAccess::readSince 读取，不能误报成“query 每次必然 flatten”；应保护这两条现成路径。逐出口登记是否物化、大小和 owner，防止重构后反而为了公开适配每次 flatten。凡合同仍要求完整 owning 值的边界，成本照实计入，不悄悄改变公开布局。

#### 8.1.2 动态事务复制：checkpoint 不能代验的决策门

RuntimeState 包含 owning KernelProjection、matchers、coverageHistory、activeRequirements、signalVisibility 和游标；KernelProjection/Fold/RPA 又含多组动态 vector。以下是已读到的复制点，不是本轮测得的主耗时：

| 路径 | 当前复制落点 | 应保留的语义 |
| --- | --- | --- |
| full Tick | execution_kernel.cpp:1795 draft；1856 reserveFault；1869 Fold clone；1884 Fold 后 reserve | Tick 未 seal 前失败前缀、seal 后 Fold 失败前缀、旧 owning 结果 |
| advance finalization | 1903 final clone、1906 reserve | frontier/horizon 提交及失败状态 |
| RPA control | 1575 draft、1647 reserve、1653 Fold clone、1667 reserve | 同 Tick 控制、pause/resume、quarantine/deferred 与 Fold 重算 |
| idle deadline Tick | 1775–1788 的既有快路径 | 不能把 idle 当 full 估算，也不能在重构中丢掉此优化 |

现有 fold_recovery_tests.cpp:1662–1693 在 legacy/RPA 的 64 fixture 上比较完整 Snapshot、Snapshot wire 与 Replay wire，并检查 timers=256、full transactions≤4、idle ticks≥60。相关 fault/Fold/finalization 覆盖也已存在；本轮只读测试源码，没有执行，不可外推 40k 或新表示已通过。

若 R0 和普通构建消融显示动态复制需要整改，本子项必须先提交每组容器的写集、可封存前缀、独占可变部分、故障 reserve、seal/Fold 提交顺序与 owning 出口的具体映射。候选限于现有 owner 内的“不可变成功前缀 + 独占可变工作集”，旧完整事务路径保留为对照；必须证明回滚与两个 fault 阶段、旧结果和未来行为一致，且不会为了兼容公开 vector 在每 Tick 重物化全量投影。此处尚未完成该逐成员表示决定，不能直接进入实现。

如果这项主导成本已被确认，但无法给出上述映射及成本证据，就阻断受影响的 R3 子项并交具体设计裁定；不能把新增 checkpoint、删除 reserve 或全局持久容器框架当成默认答案。即使 R3b 能跳过一段历史，剩余后缀执行、checkpoint 生成及动态恢复复制仍分别计量。

### 8.2 R3b：私有可信 checkpoint，而非结构合法即可信

| 字段 | 设计 |
| --- | --- |
| ID / 名称 | R3b-certified-rpa-restore |
| 目标 | 对来源与完整状态均已证明的前缀，避免每次 Seek 重新执行和比较已认证工作；保持外部不可信数据的拒绝行为 |
| 机制 | 私有 opaque ValidatedCheckpoint/受控 certificate；复用 SnapshotStorage/CheckpointCertificate 的内部载体；恢复新的独占动态状态、scope、fault reserve 与派生索引，再验证未认证后缀 |
| 改动点 | engine/judgement/src/execution_kernel.cpp::captureState/restore/seekCandidate（约 1939/1981/2641）；session.cpp::checkpoint/seek（约 434/468）；私有恢复存储/证书定义；engine/judgement/include/cuexis/judgement/recovery.hpp 的既有公开入口只做兼容核对，不先扩 API |
| 不变量 | 完整 source 身份与 applied prefix/cut 分开；whole-archive 结构认证与 prefix 执行认证分开；H/partialH/control/Fold/first-fault 保真；旧 handle 不变；每次新执行状态独占 |
| 等价性证明 | 原点路径完整 Snapshot+ReplayEvaluation 及独立预期 fixture；恢复后继续相同与对抗输入，比较未来输出、错误、fault 和控制轨迹；不采用抽样/digest 替代 |
| 测试 | 早期 cut/partialH 后 snapshot→recover；同 H 不同 cut；pause/future/timer/Fold；quarantine/deferred；不可信/畸形后缀；mutable alias/字段替换；跨配置/版本；分配失败和 ID 耗尽；旧结果跨 Close |
| 证据计划 | 本会话自然 checkpoint 首次恢复、重复热 Seek、外部首次完整认证、冷进程和无 checkpoint 回退分别登记；每项仍测请求→目标画面可用 |
| 回滚 | 新凭据和快恢复路径独立撤回，回到原点重放；新派生缓存可失效并释放，旧 archive/Snapshot wire 不重写 |
| 风险 | 认证来源可伪造；结构认证越权代替执行认证；遗漏私有状态；source 后缀混入当前 Snapshot；恢复仍复制大动态闭包；缓存失败改变原成功语义 |
| 预期效果 | 对已可信前缀可去掉重复执行/比较；端到端改善区间未知，外部首次不保证改善。合法回退仍超过 20 ms 就如实失败 |
| 依赖 | R3a；R0 支持减少重复重放/验证且首次/恢复成本仍需验算；状态/来源闭包、早期 cut 反例和旧拒绝面审计；公开语义或 wire 新字段先决策，不因已有 rev3/schema2 而免兼容门 |

私有凭据只允许两种来源：内核独占构造后封存、从未暴露可变别名的状态；或外部数据完成原有结构及执行可达性验证后，复制到内部封存 owner。shared_ptr<const> 只能约束这一个指针，不能证明别处没有 mutable alias。ReplayArchive 可由外部共享 data 构造，ReplayCheckpoint 的公开字段也可替换，所以 Reader、测试 DTO、复制、分支归档和字段替换入口默认没有新凭据。复用公开 handle 只可附带内部不可伪造 provenance；不能依对象地址、hash 相等或“Reader 成功”授信。

凭据至少绑定：当前 prepared/content/semantic revision；完整 source archive 的精确封存 owner 和 canonical 身份；整个 archive 的结构/预算/方言已认证范围；实际已应用 completeRecords、partialH、H、控制 cut；对应动态状态和索引版本。归档追加、header/count 改动、分支或内容变化必须形成新 owner/新认证范围；不能把一个 prefix 的执行认证扩成整个 archive 的认证。

自然生成与最终归档采用两级绑定，避免“运行时 checkpoint 的 owner 与导出 archive 不同，全部失效”。运行中的 checkpoint 首先绑定私有不可变 applied-prefix owner、精确 cut/H 和动态状态；B2 封存完整 archive 时，独立完成该 whole archive 的结构认证，再根据内核受控 append-only 链与完整块成员/边界关系，证明此 prefix 是该 source 的精确前缀，随后 mint 绑定最终 source owner 的 Seek 凭据。尾块只能以已封存版本或有界独占复制参与证明，不允许后来原位追加改变旧 prefix。

这种包含证明来自内部受控构造过程，不是“两个地址/hash/prepared identity 相同”；也不授予后缀执行认证。追加/分支产生的新 source 不自动继承旧 source 证书；只有上述关系重新成立才可绑定，外部/非同源/无法证明的候选仍走原完整验证。封存、完整身份、结构认证、关系索引与重绑定的时间和内存全部计入，不能在 Seek 前暗做后只报快恢复。若首版不能落实两级绑定，就只能在最终 archive 封存时生成/认证 checkpoint，并明确承担其首次准备成本，不得声称自然 checkpoint 已覆盖首次 Seek。

现有 Seek 即使只到早期 cut，也检查整份 archive 的结构、eventCount 和 diagnostic dialect。新路径必须保留这种拒绝行为：只有整份精确封存 archive 已获结构认证时，才能免重复扫描；前缀证书本身不允许接受畸形后缀。未认证后缀仍按原 admission/control/horizon/result 顺序逐条验证。

选择 checkpoint 时先核来源和身份，再按完整 cut/H 的有界索引定位；不保留“每候选 sameReplayHistory 全量深比”又大量增加 checkpoint，避免形成 checkpoint 数×history 的新成本。候选数、索引 bytes、比较工作和外部恶意候选上限均属于既有 M-CAP，生产上限未登记时不能拍脑袋设置无界默认值。

### 8.3 状态闭包与早期 cut 阻断门

当前代码在 RPA 下跳过 checkpoint，restore 仍从 rpaHistory 原点执行并比较完整 Snapshot。这是当前保守行为，不足以证明“RPA 状态不可恢复”，也不足以授权删除验证。需按以下表逐项证明。

| 状态 | 当前载体/关系 | 恢复要求 |
| --- | --- | --- |
| 语义静态数据 | 当前 configuration、已有共享 prepared、matcher 定义/timing 与可证明的派生索引 | 身份与版本核对后复用；matcher 运行状态另归动态态；不依赖外部可变别名 |
| 内核动态状态 | SnapshotDTO.kernel、matchers、coverageHistory、activeRequirements、timerCursor、nextCommitId/exhaustion | 全成员恢复并保持未来行为；不得只恢复可见投影 |
| 输入与 pending | ingress accepted、lastObservedTick、nextId/exhaustion、pending | 顺序、身份、拒绝面和未来处理一致 |
| 投影与故障前缀 | phase/resource/contact/ownership、observer/receipt、facts/frontier/H、fault、Fold/RPA | 成功前缀与 fault reserve 自足；不能用 digest 省略成员 |
| RPA 私有执行关系 | paused、cut/ordinal/epoch、accepted/quarantine/deferred/controls、foldAtTickStart/foldBaseTick/foldedControlCount | 同 Tick、多控制、暂停/未来输入和部分 Horizon 均可继续执行 |
| 归档关系 | source archive、applied prefix/cut、rpaHistory | 区分完整源身份与当前已应用状态，不以完整源历史冒充当前闭包 |
| 派生/测试状态 | RuntimeState::signalVisibility 等 DTO 外状态 | 逐项定义可确定重建、需保存或不允许的入口；sameSnapshot 不自动证明 DTO 外闭包 |
| 新运行绑定 | runScope、scheduler、设备/clock、execution context、generation | 按现有合同新建；旧 handles 不复活；保留 request ID 高水位等退役约束 |

**新增阻断反例：** seekCandidate 尾部保存完整 source archive 到 seekSource，archive() 返回它，captureState() 又将全部 records 放入 rpaHistory；而 RPA restore 重放全部 rpaHistory 后比较当前状态。因此“源有后续记录 → Seek 早期完整 cut/partialH → Snapshot → recover”可能不自洽。此为静态风险，未运行反例，不能先宣布已有缺陷已复现。R3b 首先以小规模可控测试验证，再决定内部应用前缀载体；不可简单截断 source 破坏原 archive 保留语义。

这个反例的终点还要继续输入与控制，比较恢复当下的全状态和未来行为。现有早期 cut/partialH 测试没有串起这条恢复链，终点状态的 RPA recovery 测试、legacy partial checkpoint 测试也不能替代它。即使本轮不采用 checkpoint，这仍是需要确认的恢复合同风险；设计门不依赖某个性能候选是否过线。

首选扩展私有存储/受控凭据，保留现有公开 Snapshot/ReplayCheckpoint 形态。若当前公开 SnapshotDTO 无法表达本来就应支持的早期 cut 恢复，则先形成具体差异和 ADR/Spec/兼容决定，才新增公开字段；不先创建一套公开 RpaCheckpoint，更不能让“仅热内部路径成功”掩盖公开恢复不自洽。

等价性既比较恢复当下，也继续运行。至少覆盖同 H 不同 cut、partialH、pause/resume、未来输入、同 Tick Fold、quarantine/deferred、fault 与 allocation failure、ordinal/request ID 耗尽、旧 scope/handle 拒绝。对照包括原点执行和独立固定预期，避免新旧共同依赖同一个有问题的恢复函数。

可选 checkpoint 的生成失败不应反向抹掉已完成的真实成功前缀。缓存是可选优化时，丢弃候选、记录 miss、按原合法路径继续；不能为缓存分配失败新增内核 fault，也不能在成功 seal 后假装回滚。若设计把生成纳入事务 reserve，须单独证明 first-fault、rollback 与旧行为一致。

### 8.4 checkpoint 生成、间隔与内存

首版只在正常执行完成的 record 边界生成内部 checkpoint，不加公开显式 API，不预知或专门命中测量目标 H。生成在现有 D2 owner 上；D2+D3 同 worker，没有新增“免费后台线程”。

间隔不能按未经核实的“每 N 条记录”直接填写：一次 advance 可能只产生一条 Horizon record，却处理大量 timer、Tick/Fold 和累计结果。R0 先登记每个 record 的内部工作与尾成本，随后以可证明的最坏尾工作和端到端剩余预算确定间隔，并冻结 checkpoint count/bytes 上限。没有数据之前，间隔和容量是实施前必须填入 profile 的值，不以 0、无限或临时倍增占位。

以同一新候选实测：

~~~text
T_seek = 身份/协议 + checkpoint选择/恢复 + 未认证后缀执行/验证
       + 投影/发布/替换释放 + clock + sample/present

可用尾预算 = 20ms - 同一请求实测的其余必要工作

M_live = 共享不可变基座 + 各checkpoint独占动态量
       + active/candidate/tail/索引 + 外部旧handles延长的存活量
~~~

公式是记账关系，不是把各相位 p99 相加，也不是保证尾预算必为正。Snapshot 的 90,823,328 B 编码长度不能代替独占运行态大小；既有报告中 SnapshotDTO retained 约 104,912,812 B、decodedSnapshot 约 207,829,482 B 也只说明各载体不同，不是新方案预测。

每次生成的最大暂停、D2 p99、capture 到处理的队列 age、requested/storage 峰值、索引数和最后释放均计入。即使生成移到 load，仍承担首次加载 ≤2000 ms 与首次成本披露；只报告热 Seek 不够。基座共享后每个动态 checkpoint 仍可能很大；如果预算装不下足够密度，方案在该 profile 不成立，应交裁定包。

淘汰只移除缓存索引对旧 checkpoint 的持有；外部合法 owning 值继续计入 retired live，不阻塞 Close 去等待它们。最后一个 owning handle 的析构不能回调已关闭 worker，其耗时也不能藏到不计量的路径。

### 8.5 R3c：单记录长推进仍过大时的条件设计

| 字段 | 设计 |
| --- | --- |
| ID / 名称 | R3c-bounded-internal-continuation |
| 目标 | 仅当自然 record 边界不足以限制尾工作时，判断能否在不改公开 Replay 语义下提供合法内部续点 |
| 机制 | 另立私有 sealed continuation，表示 record 内尚未完成的执行位置；与最终 Snapshot/checkpoint 明确区分 |
| 改动点 | execution_kernel.cpp 的 advance/controlRpa/seekCandidate 内部循环及相关私有 timer/matcher/Fold 游标；先提交落点和状态差异，不预授权修改公开 recovery wire |
| 不变量 | 不增造 advance/control，不改 archive/canonical 顺序；不将中间状态公开成已完成 Fold；partialH、lastAdvanceHorizon 与 finalization 一致 |
| 等价性证明 | 每种合法暂停位置恢复后与不中断执行逐成员、逐事实、逐错误一致；同 Tick 与 finalization 边界需独立 oracle |
| 测试 | 暂停在 timer/matcher/Fold/commit 前后；空工作/最后一步；分配失败/故障；相同 H 不同内部位置；中间值被误当最终 Snapshot 须拒绝 |
| 证据计划 | 先闭包/合同评审，再有限 headless 验证，再普通构建测尾预算、生成开销与内存；不能直接进入 40k 正式实验 |
| 回滚 | 独立于 R3b；移除 continuation 和索引后仍保留合法原点/record checkpoint 路径，过慢照实报告 |
| 风险 | 未完成 record 的控制与 Fold 关系最易错；新增第二种恢复状态，维护成本最高；可能触发公开恢复语义决定 |
| 预期效果 | 未知；只能针对已测得的长 record 尾工作，不保证固定发布成本或外部首验满足 20 ms |
| 依赖 | R3b 正确且相位证明必要；独立语义接受、闭包和版本决定在前。本文件不把它列为可立即实施批次 |

续点至少需要保存正在执行的 record 身份、已消费内部工作位置、pending/timer/matcher/Fold 游标、partialH、lastAdvanceHorizon 关系和剩余 finalization。不能把普通最终 Snapshot 改名后当续点，也不能靠插入虚假小 advance 来制造密度。若这套状态无法在合理维护成本内证明，停止此方向，保留原判据与未通过结论。

## 9. R4：集成后取证，区分时间、内存与设备

| 字段 | 设计 |
| --- | --- |
| ID / 名称 | R4-integrated-evidence |
| 目标 | 对同一最终产品行为取得可复核的语义、读取、Seek、Close、分域内存与生产矩阵证据 |
| 机制 | 先 B3/B4 可控机制验证，再 B5 实际部署，最后 F1–F4 对应门；时间与内存采用独立构建/进程、共同源码差异身份 |
| 改动点 | 新 profile/registration/binding 与只读裁定工具；既有测试与 consumer/architecture 门。不得覆盖冻结构建、B0 两文件、旧 invocation 或 F1 r4 |
| 不变量 | 原判据、适用性、样本、失败保留与设备边界保持；不得拼接不同产品版本的成绩 |
| 等价性证明 | 原固定轨迹的完整结果/字段/错误与版本兼容；headless、实际线程、真实设备各自取证，不相互代验 |
| 测试 | debug/release、格式/文档/版本、architecture、STATIC/SHARED clean consumers、完整适用 CTest；工具对身份/缺字段/非法统计/失败替样的拒绝测试 |
| 证据计划 | §9.1–9.4；所有实验/正式运行另行授权。正式 source 变更产生新 binding，不沿用旧 seal/claim |
| 回滚 | 工具与产品候选分别有差异/文件清单；撤回候选不删除失败证据，不追改历史判定 |
| 风险 | 插桩污染正常时间；错误自由度；把 cold/first/hot 混合；profile/caps 未接受；把诊断成功写成生产关闭 |
| 预期效果 | 本批不新增性能机制；结果只能是通过、超界、inconclusive 或未测量/缺证据，不以完成运行替代通过 |
| 依赖 | R1/R2/R3 适用子批退出；B3/B4/B5 集成；profile/工具/环境与运行授权；生产/设备/owner 接受保持独立 |

### 9.1 构建与冻结身份

普通性能候选采用原已接受 Release、STATIC、candidate ON、普通 allocator、metrics/trace/归因插桩 OFF；Debug 单独报告，不套 Release 预算。SHARED 与外部消费者是兼容门，不把它们误作已接受性能 profile。

每个新 binding 记录 HEAD、全部 dirty 产品差异/文件 hash、N3 基线差异、fixture/Replay/输入、工具/控制器、构建选项/工具链、二进制、设备/驱动/端点、显示刷新率、电源/亲和性、样本数/顺序、环境与回执。现有 dirty 观察器不能无审计混入普通候选，不能仅用 HEAD 3e5837f 标记源码。

沿用原 pre-PD A 作为正式共同指标对照，分别构建匹配 harness；当前 F1 正常 B 与 Q4 只用于归因和重锚，不能把 Q4 或优化中间版本改成正式 A。所有新构建使用新目录，不重建两个冻结目录，也不重跑 F1 collector r4。

### 9.2 正式统计与失败规则

| 单元/指标 | 协议 |
| --- | --- |
| 可比较 A/B 单元 | 每单元 3 个预声明窗口×每窗 3 对独立进程；顺序 AB/BA/AB、BA/AB/BA、AB/BA/AB；窗间从上窗全部完成后至少 600 s |
| 热身与 P0 | 每进程 30 s 机器热身、固定 fresh-session 64 steps；P0 8 块，read_long 每块至少 100 ms；其他稳态/控制样本严格消费原方案登记值 |
| 相对比值 | 以进程对的中位成本比取 log，再用 Student-t 双侧 95% 区间；逐窗 n=3/df=2，全组 n=9/df=8；帧数不是自由度 |
| 读取裁定 | 全组上界 ≤1.10 为通过、下界 >1.10 为超界、跨线 inconclusive；逐窗区间和原始每对比值披露，持续超界按原 M-SPLIT 裁定 |
| 重复性 | 原 (max-min)/median ≤15%；逐窗三进程、前六合并、全九分别检查；保留慢值，不取消第三窗或追加到过线 |
| 分位数 | 每进程报告 median、p95/p99/max/n；p95 需 n≥200，p99 需 n≥1000。少量控制用原 median/max，不跨进程凑 p99 |
| B-only | 按原方案每窗 3 个 B 进程；无共同指标记 N/A，不给 A 填 0。GPU/内存探针另进程 |
| P8 40k | 保持 owner 已定的 low/high 两种 reuse 各一次的完整规模单侧口径；未来新身份采集须另行授权，不把旧单侧补成正式九对或用 low/high 算重复性 |

“9 对”是每个可比较单元的进程对，不是整个 P0–P8 矩阵总共 18 个进程。不同内容、首次/热路径和设备单元分别登记，不能混成更漂亮的总体比值。逐窗持续门、全组接受门与每进程预算不互相替代。

身份/执行/语义错误按原门立即停止；性能超界保留慢样本，按固定组完成后裁定；环境噪声为 inconclusive，不以追加样本救结果。invalid-process 必须事前具名登记，工具缺失字段与身份错误不能事后包装成可重跑；性能超限本身不是 invalid。

Close 的“整窗最多重跑一次”仅是已选但未执行的 pilot 规则，不自动进入正式规则。正式若尚无已接受的对应 invalid/retry 处置，就保留失败并停止该次接受，不擅自替换。pilot 不进入正式九对，也不证明概率可靠性。

### 9.3 延迟边界与首次成本

| 指标 | 保留预算及边界 |
| --- | --- |
| D2 / D3 / Snapshot / 排队 | 每进程 p99 分别 ≤1/2/2/1 ms；边界依原方案，不重复求和 |
| request→ready / GPU render | 每进程 p99 各 ≤4.167 ms；前者实测端到端，后者同 GPU 命令范围；present/VSync 另报 |
| Pause/Resume/resize | 每次 ≤50 ms，包含所需状态与画面确认 |
| Reset / Close | 每次 ≤1000 ms；Close 含外部请求后的前置清理、必需退出和 join，外部合法 lease 占用另报 |
| 初次 load | 每次 ≤2000 ms，进程启动→首有效画面；记录文件缓存未控制，不称 disk-cold |
| 适用 Seek | ≤20 ms，请求→必要重建/clock 绑定且目标画面可用，保留 sample/present 外层口径 |

CPU owning lease 可跨 Close 保留且不阻塞关闭；GPU、设备或 worker 在途使用所需退休与等待仍计入 Close 的端到端 ≤1000 ms，不能因其名为 lease 而扣除。合法保留量和最终释放延迟另外报告。

若缺少可比较的无插桩子阶段计时边界，对应预算记未判定；归因探针绝对时间不能直接接受它。各阶段 p99 不相加，CPU submit/present-return/receipt/ACK/GPU complete 也不互换。

**2026-11-02 owner 裁定（本节的唯一例外）**：D2 / D3 / Snapshot / 排队 四项接受“可开关阶段计时 + 归因 + 开销披露”作为登记方式。四条须同时成立才可登记：计时设施由编译期开关控制、关闭时不进入产品路径；只登归因读数并逐进程报 p99，不改判据与范围；必须用同负载对照披露插桩开销；不与任何无插桩预算读数混用。本次登记读数（judgement / projection / runtime / snapshot / queue 的每进程 p99 分别为 45.8–122.2 / 17.9–19.2 / 6.0–6.4 / 11.0–18.4 / 26.2–88.4 µs）与约 +12 % 开销对照见[四项门裁定](../../../../stage_reports/stages/stage-07/05-rpa-new/r3/2026-11-02-r3-owner-gate-rulings.md)。该例外只覆盖这四项；其余预算仍按“缺少无插桩边界记未判定”执行。

Seek 预声明五类：本会话自然 checkpoint 首次使用、同档重复热使用、外部归档首次认证、冷进程、无合法 checkpoint 回退。全部保留真实必要成本，目标 H 不专门预制 checkpoint。首次认证即使移到 load，也要在 load/capture/p99/内存里完整入账；合同未改变前不能仅凭热路径接受该能力。

### 9.4 内存与物理门

B0 目前只有 14 个参考 profile，规模 1/200/201；64/2048/40k 尚无正式登记 caps。本方案推荐沿 B1 建议另建 derived_scale 工具，import 冻结常量并标记 formal_registration:false，供诊断与设计比较；这不是本轮对新 caps 的接受，也不修改 memory_caps.py、memory_caps.v1.json。

**2026-11-02 owner 裁定**：内存预算超过 128 MiB 已被接受，128 MiB 不再作为 R3 的适用上限判据。40K prepare 的实测私有峰值（最小 254.7 MiB）与续点预算计费（244,028,048 B；同一构建实际多留 323,303 B，计费高估 755×）按各自口径登记，不判超界；本裁定不设新的数字上限，也不替代本节要求的正式 caps/profile 注册。见[四项门裁定](../../../../stage_reports/stages/stage-07/05-rpa-new/r3/2026-11-02-r3-owner-gate-rulings.md)。

参考规模正式预算有缺口时提交 owner 接受具体 profile/caps，不能以“分域零泄漏”代替上限接受，不能把派生结果自动升级为正式注册。World/current/candidate/scratch/index、payload/attempt/journal/receipt/command/delivery 的标签覆盖与峰值/age 均需核对；缺标签为 N/A/缺证据，不填 0。

分域构建保留 observer.py::memory() 的语义；观察器锁、记账、块头和快照成本独立披露。既有约 477 MB 块头与约 7.5% 墙钟扰动是历史测量口径，不能按固定比例从新时间中扣掉“校正”出正式结果。

分别观察 active、Close 后仍保留旧合法 handle、释放最后 lease 且设备 fence/owner 回收完成三个阶段。after-close 非零不能直接当泄漏；最终要求相应 payload/attempt/GPU live 归零，允许保留 World/cache/journal 回到已声明有界稳态，不要求进程 RSS 归零。requested、storage、working set/commit 与 GPU 实占分别报告。

物理/hash 沿冻结判据 ID 与原表：Packed/decoded physical/max section 各 ≤16,777,216 B；entities=requirements=40,000；strings/references 各 ≤100,000；shaBytes ≤134,217,728；其余 hash ceilings 1,000,000/4,096/8,388,608 的适用性保持。原 P8 只独立用到对应 shaBytes 门，不能宣称四项均在本轮重验，也不以“九项”的口头计数重新定义集合。

## 10. 接口、格式与实施边界清单

| 对象 | 首选变化 | 需要先决定的例外 |
| --- | --- | --- |
| Runtime Presentation 内部头 | 新增 typed borrowed rows/callback；保持 readObject 和现有窗口合同 | 借用跨窗口、泄漏 World/registry 或改公共语义均超出 R2 |
| FrameSnapshot / SDK API | 保持当前 owning 可变布局与全字段恢复 | 若必须改变公开布局/能力，先 ADR/Spec/API/包兼容决定，不能事后补 |
| prepared/log/checkpoint 内部存储 | 保留已有 prepared/整块历史共享；按证据减少重复准备、增加受控凭据/有界索引 | 不重做已有共享；动态事务表示先过写集/回滚/物化门；不可伪造 provenance |
| SnapshotDTO/Recovery wire | 先审计现有 rev3/schema2 与私有闭包，优先内部实现 | 新公开字段、序列化语义或版本兼容变化先定映射/版本；不把现有版本当新升级 |
| Close/fence | 现有签名内推导 cleanup 资格；owner-local 阶段续行、原回复归属、统一预算与外部终态 | 不复制协议权威；需新公开取消/清理 API 或全局回收服务时停止最小批次，提交具体决定 |
| archive identity | 原 canonical bytes 的同一 SHA-256 | 采用 prefix digest/树根改变身份，属于协议决定，首版不选择 |
| profile/工具 | 新文件、新 binding、明确诊断/正式身份 | derived_scale 不能自封正式 caps；不改冻结来源或 observer 语义 |
| ABI/架构 | 保持依赖 allowlist、无隐式线程、公开头 ASCII、STATIC/SHARED clean consumers | 任一需要破界时先决策，不以“性能需要”豁免架构门 |

每个获准实施批次先保存本批起点与文件级 patch/新增文件清单，再修改。回滚只撤本批确切差异和新增文件；先对照身份，禁止对整个准入树 git reset --hard、git clean 或覆盖原有 dirty 文件。保存的 patch/新增清单就是执行 agent 的一条撤回脚本输入；成稿尚未实施，本文不伪造一个现在可运行的逆向 patch。

## 11. 对此前交接十问的显式答复

| 问题 | 答复与定位 |
| --- | --- |
| 1. 先相位归因还是直接改 | 先 R0。复用既有 PartScope/工作计数，但当前 Trace 会锁内打印；新增固定槽与宏 OFF 正常对照，分别测 full/idle、prepare、动态副本、尾块、比较及发布。不据 6.37 s 猜占比。见 §5 |
| 2. Seek 选哪种机制 | 先保留现有 prepared/日志共享，按 §5.4 选择准备去重、动态事务表示或可信恢复。checkpoint 是重复前缀执行/比较的条件候选，不能解决所有复制或长 record；拒绝 digest 替代验证；必要时另评 R3c/裁定。见 §8 |
| 3. 状态闭包/为何 !rpa | 现有 !rpa 是保守跳过，RPA restore 仍原点重放；DTO 有多类 RPA 字段，但早期 cut/history 与 DTO 外派生状态未获充分证明。首选私有载体，不新增公开 RpaCheckpoint；完整结果与未来行为验证。见 §8.2–8.3 |
| 4. 间隔/内存/线程 | 不凭 Reader 的 100000 预算造间隔；先测内部工作、剩余预算及每份独占动态量，再冻结 count/bytes。D2 原 owner 生成，成本纳入 p99/capture/load/最终释放。见 §8.4–8.5 |
| 5. 读取组合与公开头 | Playback 栈上收集≤32 个索引，Runtime 同步借用行；现有布局、单材料精确快路径、原序物化，矩阵写最终 DTO。字符串原已有条件赋值，仍保留全部字段恢复/错误检查。只改内部头/src。见 §7 |
| 6. Q4 重锚如何做 | 必须先做当前 F1 重锚；read_long/N64，独立诊断窗口 3 轮 A/B/Q4，8 块≥100ms，固定热身；有合法 A 才报告 Q4/A。它不进入正式九对。见 §5.3 |
| 7. suspend/状态机/pilot | suspended 与 Closing 分轴；补 cleanup 授权和四阶段续行，原 ID/typed reply/generation 一致。原请求总预算不重置，超预算后仍要安全完成清理。先可控反例，观测本身不证明修复；pilot 保留失败且最多整窗重跑一次。见 §5.2/§6 |
| 8. 顺序与授权 | R0 → R1 许可/续行反例；R2 与获支持的 R3a 子项可独立开展；R3b 先过闭包和来源门；集成后 R4。R1、R2 已退出，R2 owner 接受记录见 §7.11；R3/R4 按各自授权及依赖进入。不将外部输入转成执行指令，也不把条件子项统称已可开工。见 §4/§12 |
| 9. 正式窗口与冻结 | 每可比单元九对、新产品/工具/fixture/设备绑定；pilot 不混入，invalid 事前定义；P8 仍单侧独立。两个冻结目录/F1r4/B0 不动。见 §9 |
| 10. 失败出口 | 固定成本、首次认证、尾工作或内存约束使预算无法满足时，提交原始数据、闭包/反例、三选裁定包；保持未通过，不自动改门或拆线程。见 §12.2 |

## 12. 开工条件、停止条件与裁定包

### 12.1 下一批具体动作

R1、R2 已退出，R0 保持历史残余。owner 于 2026-10-26 接受 R2 按原计划退出，记录见 §7.11；用户随后授权接手完成 R3，当前 R3a 修复/去重已通过本地矩阵，R3b 首个可信恢复候选已核验但预算未通过，按 §12.2 提交裁定包，见 [R3 实施登记](r3/plan.md)。R2 新重锚/触发调查已经完成，旧不确定结果保留，当前四规模受控局部结果通过。旧窗口不重跑或补样，不将内部消融替代公开提帧接受。R4、R3、正式窗口、40k、ETW 和设备接受仍按各自边界判断。下列原设计顺序保留为历史来源，不作为当前未完成的 R1 清单：

1. 重读 CURRENT_STATUS、RPA B0/B1–B5、PD-5 定案和本文，重新核验两树 HEAD/status/diff；保护既有 dirty 文件，形成独立可审查起点。
2. 先查清 Close accepted binding、旧 fence/new Close、generation 与状态暴露；加入 flush 失败、旧非 Close 清理和 §6.3 四阶段中断反例，验证原 typed reply、ID、增代与 pump query，随后再选最小修复。
3. 完成 R0 固定字段/缺失拒绝/宏 OFF 审计；Seek 登记实际 record、full/idle、prepare、动态副本、尾块 payload、完整比较和发布。沿用已有 prepared/日志与诊断落点，不从零重建。静态实施不附带 ETW、40k、设备或 pilot 运行许可。
4. R1 最小产品差异、适用测试及 docs/architecture/consumer 门完成后，给出具体 pilot 计划和构建身份；已有 owner 选中的 pilot 处置口径不重复询问。
5. R2/R3a 只在各自 R0 结果支持后进入；动态事务表示未定时停住该子项。R3b 先确认早期 cut→Snapshot→recover→继续运行反例并退出状态/来源闭包门。每批完成必要检查及适用回归后再扩大；不把后一批当救场。

执行侧日常修改按 AGENTS 的 Debug configure/build/CTest 与格式门验证；合并/版本前按 Release fresh/clean-first 和完整适用检查。文档仅改时用 check_docs.py/git diff --check。实际选择的 toolchain/preset、STATIC/SHARED 和未跑平台必须逐项写清，历史绿色不是新候选证据。

### 12.2 何时停止继续堆优化

满足任一条件即提交裁定包，不自动放松阈值：

- 正确的公开读取实现经正式协议仍超界；剩余收益只能通过删 owner/expiry 或跳过 DTO 恢复获得。
- 固定必要的身份/协议/发布/clock/sample/present 已消耗 20 ms，或外部首次完整认证/合法无 checkpoint 回退仍超界。
- 单条 advance 的不可细分尾工作超预算，而合法 continuation 的闭包/维护成本不可接受。
- 足够密度的 checkpoint 导致已接受 caps、D2 p99、capture 队列 age、load 或最后释放预算失败。
- 动态事务复制被确认阻断预算，但尚无可保留 rollback、FaultPrefix、旧 owning 结果与有界物化的具体表示；新增 checkpoint 不能代替此决定。
- Close 最小许可修复仍无法在原宿主 owner 内证明超时保活和可泵终态，需要重新决定生命周期合同。

**2026-11-02 已裁定的一项**：40K @ 240 FPS 的“结构性不可能”判定已由 owner 接受，按选项 2 处理——明确改变适用能力范围，40K 不再作为 240 FPS 的适用规模；240 FPS 边界仍以冻结容量值（1080p 仅渲染 1800、静态 1300）与实测前缘括区表述，不发布新容量值。判定文本、读数与全部累计证据保留；本裁定不关闭 R4、设备或生产接受。见[四项门裁定](../../../../stage_reports/stages/stage-07/05-rpa-new/r3/2026-11-02-r3-owner-gate-rulings.md)。

裁定包应包含：源/工具/构建/fixture 身份；所有失败和原始数据；端到端边界与实际工作量；冷/首次/热/回退分层；逐窗 CI/15%/observer 扰动；相位包络与排他量；每份状态大小/生成成本/active-retired-final 内存；闭包与安全反例；已测/未测清单；可回滚最小差异。没有数学或实验下界时只能写“当前候选达不到”，不能断言所有设计都不可能。

| 选项 | 后果与推荐 |
| --- | --- |
| 1. 保持判据和未通过状态，继续证据支持的限定整改 | 默认推荐。只继续有可删工作和可验证收益的子批；不无限叠缓存/线程 |
| 2. owner 明确改变适用合同/能力范围或预算 | 需独立产品决定、ADR/Spec/profile 与新接受记录；技术方案不能自行接受，也不追改 PD-5 |
| 3. 暂停相应规模/能力的交付 | 明确未交付能力、恢复条件与用户影响；保留其余已验证能力，不宣称整体通过 |

三种选择都要列出对 F1 产品矩阵、F4 接受清单、40k/Packed/Seek 物理门、真实 audio/GPU/输入设备和生产预算的影响。机制正确、本机 reference 通过、设备通过和 owner 接受是不同门；本文与后续局部整改均不宣布 RPA 或 Stage 7 关闭。

### 12.3 本方案的非目标

- 重开六域、D2/D3 部署、capture-time 或 Judgement/Gameplay 语义。
- 将公开恢复改成 digest/抽样验证，或用序列化、局部 Seek、目标专用预热冒充端到端 Seek。
- 引入公开取消 API、通用持久化框架、新服务线程、全局 quarantine/reaper、跨帧属性缓存或未经归因的对象池。
- 翻案 PD-5、覆盖冻结产物、改 B0 上限来源、改变 observer.py::memory()、提交/推送/合并。
- 以 Debug/插桩/单次 pilot/旧 CI 代替新 Release 正式、完整规模、设备或生产接受。

## 13. 核查来源与交付状态

本方案以此前输入、两树实时身份及以下准入树源码/合同为依据；未改写、重建或重跑冻结构建与证据。其中 2026-10-23/24 的 PD 报告与 B0/B1 工具记录目前位于准入树工作区，尚未进入主树，本文只按文件名引用。

- docs/CURRENT_STATUS.md；docs/api/realtime-host-boundary.md；docs/stage_plans/active/stage-07/plan-rpa.md、performance-modification-plan.md、presentation-decoupling-design.md、pd5-measurement-proposal.md。
- docs/stage_reports/stages/stage-07/pd/2026-10-24-pd5-verdict-and-stage-7-pd-exit.md、2026-10-24-p8-40k-single-sided-complete-scale-ruling.md、2026-10-24-close-path-control-pending-static-qualification.md、2026-10-24-b0-memory-cap-ledger.md、2026-10-24-b1-domain-attribution-tooling.md，以及 2026-10-23 两份 read-regression attribution 和 memory-envelope attribution 报告。
- app/player/src/execution_host.cpp、player_execution.cpp；app/execution_host/session_worker.cpp、host_protocol.cpp、host_candidate.cpp、audio_worker.cpp；engine/playback/src/playback_session.cpp、presentation_materializer.hpp/.cpp。
- engine/runtime/internal/cuexis/runtime/presentation_read.hpp、engine/runtime/src/presentation_read.cpp；engine/judgement/src/execution_kernel.cpp、session.cpp、execution_log.hpp、recovery.cpp、recovery_codec.cpp；engine/judgement/include/cuexis/judgement/recovery.hpp。
- tests/pd/pd5_capacity_probe.hpp、measurement_method_tests.cpp；tools/verification/playback_performance/measurement.py 及冻结 B0 工具/数据的只读源码。
- engine/judgement/include/cuexis/judgement/gameplay_prepare.hpp、kernel_types.hpp；engine/judgement/src/gameplay_prepare.cpp；engine/core/internal/cuexis_internal/execution_diagnostics.hpp；tests/rpa/rpa_b5_host_tests.cpp；tests/judgement/fold_recovery_tests.cpp。

### 13.1 证据强度、现有覆盖与待证反例

“源码确认”只说明本轮静态核查；“已有测试”只说明读到了相应测试逻辑。下表没有任何一项表示本轮已运行测试、复现故障或取得新性能数字。

| 判断 | 源码定位/现有覆盖 | 当前证据强度 | 下一步如何证伪或闭合 |
| --- | --- | --- | --- |
| PreparedGameplay 已共享不可变 storage | gameplay_prepare.hpp:54–72；gameplay_prepare.cpp:140–157 | 源码确认 | 分别计数 handle、配置复制、prepare/派生表和动态态构造；不能再以“handle 深复制”为前提 |
| RPA restore 有两次 prepare，Seek 也有重复准备落点 | execution_kernel.cpp:1983/1990/2689；Playback makeGameplaySession | 调用链确认，成本未测 | 消融保留当前配置 identity 和语义失败点；收益不足则不增加共享表 |
| 日志已有分块，但尾块复制嵌套 payload | execution_log.hpp::reserveLogAppend；recovery.hpp:23–30 | 源码确认 | 0/31/32/33 及大 batch/admission，分别计次数、字节、旧 handle 与释放 |
| 完整事务动态复制与 idle 快路径并存 | execution_kernel.cpp 的 full/control/finalization clone；fold_recovery_tests.cpp:1662–1693 | 源码与现有测试逻辑确认，均未运行 | 对真实 fixture 计 full/idle/各副本；保留 reference 全状态/wire/fault 对照 |
| 原设计基线的 Close/suspended 前置 fence 比退休许可更窄；准入树已有无 fence 清理许可差异 | enterExecutionFence、SessionSoftwareRetire、retireSoftware/retireExecutionStep | 原矛盾与当前分支差异为源码事实；542/4 仅引用清理许可单元记录，本轮未重跑 | 按 §6.2 补绑定/decision/generation、failed Close 有无旧 fence 与 flush 反例；按 §5.2 区分无 fence 不适用和字段缺证据；不追认历史根因或 R1 完成 |
| Close 自身各阶段超时的续行缺口 | close/cleanup；HostCoordinator::finish/completeCleanup/accept；rpa_b5_host_tests.cpp:422–504 只覆盖旧 Tail | 静态风险与覆盖缺口 | 四阶段受控停顿；原 job/typed reply/operation/step 不变，最终退出/join且首次预算失败可见 |
| 门面对外状态可能与退出/join 不同步 | host_protocol::finish 先 Closed；execution_host::state/failed；close 成功在 join 后 | 静态时序风险；未发现成功返回早于 join | 在 pump 内 query，并在 finish/confirm/shutdown/exit 阶段分别中断 |
| 早期 Seek 后 Snapshot→recover 可能重放过多历史 | seekSource→archive→captureState::rpaHistory→restore；现有早期 cut 和终点恢复测试未串联该链 | 静态风险，未复现 | 完整源含未来记录，分别 Seek 早期 complete cut/partialH，再恢复并比较全状态与未来行为 |
| 读取有矩阵中转和 Snapshot 未用 Renderable 查询 | presentation_read/materializer；已有 SnapshotLayout 与字符串条件赋值 | 可删工作确认，收益未测 | 当前 F1 Q4 重锚；有界 typed rows 与旧路径比较首错/部分写入/被污染 DTO，再测普通构建 |
| 当前诊断 Trace 有 mutex/stdio/flush | execution_diagnostics.hpp:60–99；PartScope:112–125 | 源码确认 | 延后导出候选与宏 OFF 审计；扰动独立量化，诊断时间不得直接接受预算 |

### 13.2 本轮交付与保全

本文于 2026-10-24 首次迁入仓库时，正文技术结论未改写；迁入只统一状态行、路径表述和链接，不保留外部备份路径、备份 hash 或外部交付描述。同日后续 F1–F5 裁定更新 §1.1、§5.2、§6.2/§6.3、§7.1 与本节，属于明确的设计修订，不能继续称为仅迁入排版。

设计成稿时的历史保全核验：准入树 HEAD/status 与当时设计起点一致，原有 52 个修改/未跟踪文件的 SHA-256 全部一致；当时主树为 eed36648a692333e47f798aa45be36ca850d08de / stage-7，工作区干净。这不是本次裁定时的实时状态；本次开始于主树 f0c7e3230e3abd479fda18e009a7688df89c7672，准入树已有 54 个修改/未跟踪文件，见 §1.1 和审阅记录 §8。文档核查不替代产品测试。

未经 owner 接受的部分仍是建议。本轮未修改产品、未构建或运行 CTest、未登记或执行新测量；静态调用链与测试覆盖审查没有证明历史失败根因、运行正确性或性能达标。

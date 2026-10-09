# Gameplay V2 execution typed boundary supplement

状态：candidate；S7A-3 / S7A-4 首次消费的选定表示

更新日期：2026-10-05

本文件补充 [Gameplay V2 ABI](GAMEPLAY_V2_ABI.md)，只拥有表示、所有权和接口。
运行语义以[执行 Spec](../formats/gameplay-v2-execution-profile.md) 为准。
所有类型属于内部 `cuexis::judgement`，不安装 judgement headers、不增加 SDK API、
不定义 wire/struct padding/稳定 C ABI。ABI 的 9 域 / 126 角色计数不因实现 helper 增加。

## 1. 表示与字段

以下 spelling 为本轮选择；类型宽度不是实现时任选，optional 不能用 sentinel 替代。
text 使用 owning `std::string`；list 是 owning vector，查询 view 由 immutable storage 保活。
Tick/Span/Delta 继续用现有 strong types；phase/action 继续用现有 enum，新的 rank 不改 enum wire 值。

| 结构 / 新增字段 | C++ 内容与所有权 |
| --- | --- |
| `PhaseTarget` | `PhaseKind phase; Tick chartTick;`，存入 `RequirementTiming::phaseTargets` |
| `AmountMatchRange` | `int64_t minimum,maximum`，闭区间；不使用 float |
| `AtomBinding` | owning atomRef/domainToken/sourceClass/channelToken，`InputAction action`，`optional<AmountMatchRange> amountRange`，`bool tailOnly` |
| `RequirementRecord::atomBindings` | vector<AtomBinding>；准备后 immutable，关联既有 pattern |
| `RequirementRecord::independentCompetition` | optional<CompetitionKey>；仅无真实 claim 的非 observe 实例 |
| `CanonicalGameplayGraph::executionProfile` | owning token；不会从 graphRevision 推断 |
| `AssemblyRequest` / `CanonicalGameplayGraph` 新增约束 | owning normalizationProfileToken/coordinatorPolicy/executionProfile；legacy 静态路径允许新增 string 字段空值，execution profile 必须显式完整，沿 CapsuleProfiles 对账 |
| `CanonicalObservationKey` | ObservationTick、owned domain/source/channel、action、optional<int64_t> amount；通过结构导出 comparison key |
| `AdmittedObservation` | owning normalized fields + canonical key，Tick dispatchTick、bool wasForwarded、optional<Tick> admissionFrontier/admissionHorizon；raw timestamps 另放 diagnostic |
| `CandidateKey` | E、phase、tagged OriginKey、optional<string> resourceId、optional<ResourceClaimIntent> intent；值类型，不暴露内部 index |
| `ContactHandle` | domain/source/channel + CanonicalObservationKey startPressKey；只引擎生成 |
| `HeadContactOwnership` | E、ContactHandle、Tick acquiredTick、optional<LeaseIdentity>；独立 Hold 无 lease |
| `SlotIdentity` | owning resourceId/slotToken + uint64_t ordinal=0；实际声明必须显式，0 是唯一 slot 的规范值 |
| `LeaseIdentity` | SlotIdentity、E、CandidateKey；只在获胜 commit 建立 |
| `ResourceState` | variant<Free,Held,Terminal>；Held 持有 E/LeaseIdentity/ContactHandle，另两分支没有这些字段 |
| `PreparedTimerKey` | E、phase、timerKind、Tick、optional<Tick> windowStart/windowEnd；结构比较 |
| `LogicalCanonicalOrdinal` | tagged OriginScope、u64 originOrdinal/localOrdinal、phase/factKind rank；不复用旧 scalar helper 或 hash |
| `OriginId` | tagged union ObservationOrigin/TimerOrigin/CoordinationOrigin；字段沿 execution Spec，不接受外部 ID |
| `CommitId` / `FactId` | u64 / pair<CommitId,u64 localOrdinal>；会话域、overflow checked；仅逻辑 typed 表示 |
| `PhaseOutcomeFact` | E、phase、Outcome、派生 FactCategory、optional<TickDelta> error、optional<CanonicalObservationKey> evidence、originId/commitId/factId/commitTick |
| `ReceiptFact` | kind stray/consumeEmpty、observationKey 与身份字段；没有 phase/error/grade |
| `InputReceiptPending` | owning observationKey、Tick dispatchTick、bool wasForwarded、optional<Tick> admissionFrontier/admissionHorizon；只 admission 确认 |
| `ObservationRecord` | E、originKey、CommitTick、optional<CanonicalObservationKey>、bool progressed/accepted；observer inspection，非 Ledger |
| `InputReceipt` | u64 consideredCount、optional<E> consumedBy、dispatchTick；无输入 receipt 不能假造原 observation |
| `CoordinationCommit` | const-owned origin、phase/resource/contact/matcher patches 与 Fact descriptors；创建后只读 |
| `KernelProjection` | owning immutable phase/resource/contact/observer/receipt/Fact prefix、optional<Tick> processedFrontier/lastAdvanceHorizon、KernelSessionState、optional fault diagnostic、optional failedTick/requestedHorizon、opaque self-owned runScope anchor；generation 不进入 identity |

grade 字段不在 S7A-4 outcome 中编造值，S7A-5 首次消费再增加真实计算结果。
evidence 的 absence 不写一个空 ObservationId。所有 immutable 产品允许 shared_ptr<const Storage>
保活；borrowed view 不能逃出 owner，不能引用 submit 的 string_view 或已销毁 Reader buffer。
H1 dense index 仅 table addressing，必须显式恢复 H2 才能作逻辑比较、trace 或 identity。

## 2. Session 接口与生命周期

保留 create；configure/prepare/submit/advance 的旧无参数 skeleton overload 仍拒绝。
query/reset 实现原有签名，snapshot/seek 继续原有拒绝；不以返回类型区分 C++ overload。
configure/prepare/submit/advance 的有参 overload 实现真实路径，不用无参数伪成功。

`SessionConfiguration` 的精确字段为 inputMapping、timebase、latePolicy、identityDeclarations、
calibrationIdentityToken、executionProfileToken、lateAlgorithmToken、factSemanticRevision。
前三项是 self-owned typed profile storage，identityDeclarations 沿已有 PreparedIdentityDeclarations；
三个算法 token 必须匹配 execution Spec §1，不用调用者任意 token 替换内置算法。
calibrationIdentityToken 是 opaque 已校准入口 provenance，必须非空；本批不新增 offset/slope/
设备校准函数，ClockedIngress 仍是已捕获的 ObservationTick。token 与 mapping 全部实际字段
进入 session identity；prepared graph 与 configuration 的完整 timebase/late 参数逐字段相等。
不要只比较 profileId，也不要把 late policy 的 source/semantic 投影删除。
新增 executionProfile、phaseTargets、atomBindings、independentCompetition 也必须进入既有
makeChartIdentity/makeContentIdentity/makePreparedIdentity 的对应投影；不能只更新 Capsule hash。
EngineIdentityDeclaration 增加 optional owning executionProfileToken/lateAlgorithmToken；旧静态路径
均 absent，运行路径必须与 configuration 的对应显式 token 相等，并进入 engine identity。
本实现 profile 的已有四个 engine 字段固定注册为：judgementSemanticRevision=`judgement.t4-k4.v1`、
factSemanticRevision=`fact.semantic.phase-local.v1`、fixedPointTableId=`fixed-point.none.v1`、
coordinationPhaseOrderToken=`coordination.six-eight.t4-k4.v1`。none 是本 kernel 不消费固定点表的
算法声明，不是生产参数默认值；未实施 grade 不假冒引用 grading table。未知 engine token 拒绝执行。
PreparedGameplay 增加 const identityDeclarations() accessor，storage 自持有全部声明；
prepare 要求 configuration 与 prepared 的全部 engine/ruleset/session 声明逐字段相等。
运行 JudgementIdentity 保持该 engine/ruleset/chart 三分量，session 分量包含原 session 声明、
完整规范化 InputMapping、完整 late parameters 和 calibrationIdentityToken，不以 opaque
judgementConfigToken 替代实际数值。query 持有这个运行身份；它不回写 chart 或 Capsule hash。
配置 mapping 变化时原 PreparedIdentity 的静态比较值可保持，但运行 JudgementIdentity 必须改变。
KernelSessionState 是内部 uint8_t 枚举 Created/Configured/Prepared/Faulted，序 0/1/2/3，
不暴露新宿主 lifecycle；新 runScope anchor 每次成功 prepare 建立，reset 后旧 projection 仍可读。
KernelProjection 是内部只读数据；现有 JudgementProjection 增加 kernelView() const accessor
返回 const KernelProjection&，由该 JudgementProjection 保活。query() 的已有签名保持。
不提供业务数值默认 constructor；optional/pending 保持可诊断，configure/prepare 不能接受执行缺口。
reference fixtures 显式构造测量值，不能把 fixture 值注册为生产 default。

| 接口 | 入参 → 结果 | 行为 |
| --- | --- | --- |
| `configure(configuration)` | owning configuration → Result<void> | Created→Configured；完整字段校验；再次配置拒绝 |
| `prepare(preparedGameplay)` | self-owning immutable PreparedGameplay → Result<void> | Configured→Prepared；复核 execution profile、mapping、late gate，失败保持 Configured/旧值 |
| `submit(batch)` | vector<ClockedIngress>，每项显式 ObservationTick + IngressDeclaration → Result<vector<InputReceiptPending>> | Prepared；临时 ingress transaction，全批成功才接收；pending receipt 只确认 admission，不声称 consumed |
| `advance(horizon)` | Tick → Result<JudgementProjection> | Prepared；逐 Tick seal 至 horizon-C，失败保留已 seal prefix；成功查询值同 query；重复 horizon 幂等 |
| `query() const` | → Result<JudgementProjection> | Prepared/Faulted；只读 self-owned projection，无状态推进 |
| `reset()` | → Result<void> | 任意有效 owner-thread session→Created；清 configuration/prepared/ledger/cursors/contacts/ingress，不发 Fact |
| `snapshot()/seek()` | 保留 skeleton Result 拒绝 | S7A-6 实施；本批没有临时 codec、伪 payload 或成功 seek |

`ClockedIngress` 是 internal adapter 输入，ObservationTick 已由校准会话时钟捕获；
不能由 rawTimestamps/hostArrival 重建。batch admission 按 observationTick/key 验证，而非
按源数组顺序 resolve collision；多项同 Tick distinct 原子拒绝。
维护 SessionIngressState 的 transactional admission API，不能假装现有 noncopyable state 可复制。
选定 private journal：`prepareIngressBatch` 只读 state，返回 owning normalized entries 与待写记录；
late/dispatch 校验通过、全部 queue/去重存储 reserve 完成后，`commitIngressBatch` noexcept 发布。
现有单条 normalize 复用同一个内部 prepare-one/commit 路径，保留原有拒绝语义。
选定 owned subject 节点存入 vector<unique_ptr<const OwnedSubject>>；节点和全部字符串在
journal 内分配，live owner vector、subject/sequence vectors、pending queue 都先 reserve。
commit 仅 noexcept move unique_ptr/owning entries 和写 scalar，live view 指向稳定 heap node；
不能以 unordered_set.reserve 当作节点插入无分配的证明，也不对 deque 编造 reserve。
SessionIngressState 的去重记录必须拥有字符串；其 private owned subject nodes 地址稳定，
不能长期保存 caller 的 string_view，不能靠 vector<string> 重分配后悬空的 SSO bytes。
NormalizedObservation 的旧 borrowed 返回约定可保留，但去重/key/queue 的状态存储必须 owning。
late 拒绝丢弃 journal，完全不改 live state；不通过“先 normalize live 再尽量撤回”补救。
submit/advance/query/reset 都服从已有 owner-thread check；query storage 可跨线程只读，
从 projection 取 view 时其 owner 必须活着。session 可销毁，旧 projection 仍可读。
Faulted 允许 query/reset，其余变更一律拒绝；不从 faulted 调用 advance 重试 partial Tick。
prepare 不包含 author compilation；编译在离线 assembler，Session 只消费自持有 prepared 产品。

## 3. 诊断登记

保留九类和 19 个 R 项。下面是**新增详细内部码的选定登记**，实施先写
`schemas/cuexis.gameplay-diagnostics.v2.codes.json` 和 code tests，再消费。
所有诊断 severity=error；除最后一行外 session_unaffected，失败返回 Result 不抛跨模块异常。

| detailed code | category | 条件 |
| --- | --- | --- |
| `judgement.s7a4.execution.profile_incomplete` | identity_closure_incomplete | profile/phase target/atom binding/独立 pair 缺失，旧 graph 不可执行 |
| `judgement.s7a4.execution.profile_unsupported` | capability_disabled | 明确声明未知或本 profile 不支持的执行表示 |
| `judgement.s7a4.execution.relation_invalid` | invalid_relation | target/window、atom/action、phase/contact 或 source/rank 结构关系不合法 |
| `judgement.s7a4.late.parameters_invalid` | late_policy_incomplete | measured 参数负值或 O/C/F/H 关系不合法 |
| `judgement.s7a4.late.rejected` | late_policy_incomplete | rejectLate、finalized、hop 超界、二次转发或会反写已 seal phase 的输入 |
| `judgement.s7a4.late.dispatch_collision` | invalid_relation | 两个 distinct subjects 调度到同 dispatchTick |
| `judgement.s7a4.session.lifecycle_order` | invalid_relation | 当前状态不允许该有参接口，或旧无参 skeleton 不可消费 |
| `judgement.s7a4.kernel.transaction_failed` | invalid_relation | 已入场的 Tick 计算遇到不可恢复 allocation/arithmetic/invariant 失败；session_faulted |

既有同 Tick 原 observation collision、time reversal、amount、duplicate sequence/subject、
unsupported capability、non_unique_solution、containment gate_incomplete 保持既有码，不复制同义码。
纯 prepare/submit 可表示性失败沿既有 tick/amount overflow 类 budget_exceeded；
只有运行中已接收 Tick 的 seal 失败用 transaction_failed，detail 指具体失败字段/预算维度。
集中码表中的 faulted 允许集合应按此次 profile 补充 kernel.transaction_failed，
原先“只有 ruleset.transaction_failed”是 S7A-5 的已有限定，不适用于新增 kernel 路径。
budget 生产阈值未接受时只计数；若未来 accepted steady bound 触发，使用该批已登记 budget 码
并按主 Spec faulted 语义执行，本轮不假造该限额。

## 4. 模块落点

既有 `cuexis_judgement`：timebase/input/prepare/kernel，依赖 core；
既有 `cuexis_gameplay_packed`：Capsule revision3，依赖按当前 target allowlist，不把 parser 放 judgement。
离线 author adapter 选 `tools/gameplay_assembler/` 内的私有 library
`cuexis_gameplay_author`，在 CUEXIS_BUILD_DEVELOPER_TOOLS=ON 时启用，测试另要求 BUILD_TESTING=ON；允许 core/json_support/chart/
judgement/gameplay_packed，按实际 CMake target 名登记全部直接依赖。
它是 tools 私有 target，不安装，不导出，不转成 Playback 依赖；DOM 到 DTO 的部分放 json_support。
测试 target 选 `cuexis_gameplay_author_tests`，位于 BUILD_TESTING；架构测试新增该边界。
新增 cuexis_* target 必须同步 ACTIVE_TARGETS、allowlist、BUILDING target index 和 checker tests。
不新增第三方依赖，不改 vcpkg baseline/license；不改 engine/chart 的禁止 judgement/platform 边。
三个 Catch2 test target 必须登记 judgement/gameplay_packed/gameplay_author CTest labels，
使用 labels/真实 test 名称验收，不把 executable target 名当作所有 Catch2 test 名。
configure/prepare 的 Runtime storage 必须拥有字符串，旧 borrowed profile view 只作短期适配。

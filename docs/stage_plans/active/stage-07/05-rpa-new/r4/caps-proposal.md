# R4 O1 预算与生产参数裁定包

状态：owner 已接受 reference 设计预算；生产 profile 仍未接受或启用

更新日期：2026-11-05

返回 [R4 实施方案](plan.md)。本文件对应 O1；预算来源为声明容量和当前类型尺寸，
不使用实测高水位反填。既有[B0 账本](../../../../../stage_reports/stages/stage-07/04-pd/pd-5/memory-attribution/2026-10-24-b0-memory-cap-ledger.md)
保持原字节；新增机器可读表位于仓库 tools/verification/execution_acceptance/budget-proposal.v1.json。

## 1. 请求裁定的结论

**推荐选 B：接受本文件完整数值表作为 R4 本机 reference 的设计预算，先验证，再决定生产接受。**
新数值适用 64、2048、40000 的具名内容；40K 只做原完整规模单侧 low/high，
不要求达到 240 FPS，也不改变既有 1800/1300 容量裁定。

本次把两类证据分开标明：固定数组/槽位可由真实类型直接算出；decoded、prepare、历史保留
和设备等新增字节值属于**拟接受的设计预算**，尚不是从完整数据结构证明出的最坏堆上界。
超过预算仍应报告超界；接受预算不保证运行通过，不表示产品已实现这些新总额的硬性拒绝。

| 方案 | 接受内容与优点 | 代价与限制 |
| --- | --- | --- |
| A：只用既有已接受 profile | 新数值决定少，可继续已有规模的工具/正确性工作 | 64/2048/40K 新分域正式门继续阻断，无法完成本轮完整 R4 取证 |
| **B：接受完整 reference 设计预算（推荐）** | 明确各域 count/bytes/生命周期、别名及总量，允许在工具准入后验证新规模；保留失败出口 | 40K 请求量包络约 9.08 GiB，另有 fixture/观察器；不能据此声称 SDK 对任意输入有生产内存保证 |
| C：先给较紧生产目标再重新设计 | 例如把 40K CPU 总目标定为 2 GiB，可以直接约束未来产品部署 | 该数只是备选产品目标，不受当前静态审计支持；历史结果/外部 owning 值可能要求改变存储或合同，需独立设计，不能靠调表完成 |

选择 B 的接受范围为 §3–§6 的 reference 表、storage/进程/设备分口径及失败规则；
§7 的生产目标只作后续校准目标，**生产启用须补证据和 profile 身份，不移除 testOnly**。
owner 可对个别数值提出修改，代理提交表差异与影响，不重新询问已选整体路线。

## 2. 当前静态审计发现

MSVC 19.51.36256、C++20、Release /MD、candidate ON、metrics/归因 OFF，
本轮只运行 sizeof 探针，没有运行 40K 采样。真实私有类型通过内部 constexpr 方法提供尺寸，
不使用手写“相似结构”冒充语言标准保证的精确布局。

| 当前类型/槽位 | B | 影响 |
| --- | ---: | --- |
| SessionCommand / SessionReply | 38,712 / 904 | 命令本体不是队列完整储存成本 |
| session 完整 optional 槽位 | 39,664 | 包括 command、可选 Result reply、ID、状态和对齐 |
| audio 完整槽位 | 1,688 | 命令与回复同时保留 |
| OwnerStepLedger 槽位 | 912 | 含 StepReceipt、确认/回收状态和 optional |
| host ControlReceipt 槽位 | 36,920 | HostCoordinator 真实 optional 布局 |
| history / consumer 槽位 | 208 / 1,192 | history 每消费者独立，不能漏乘消费者数 |
| observation frame / feedback 槽位 | 456 / 136 | 定容槽位与 journal 动态字节必须分别计费 |
| FramePayload / FrameEnvelope | 304 / 736 | owning 数组和字符串另计 |
| FrameSnapshot::ObjectSnapshot / FeedbackId | 320 / 96 | 用于原 frameByteBound 算式 |
| SnapshotDTO / ReplayRecord / KernelFact | 1,152 / 272 / 1,152 | 都含动态或共享成员，sizeof 不是完整堆大小 |
| RequirementRecord / PreparedRequirement | 1,096 / 192 | 编译后图、字符串、自动机和索引另计 |
| NativeRecoveryPoint / maxAnchors | 56 / 64 个 | 64 是索引数量上限，所持状态和历史不是 64×56 B |

本次确认两项生产边界：SessionWorker、AudioWorker、ExecutionHost 和 Player 当前都要求
testOnly；仅有预算数字不能启动真实 production 配置。execution log 会保留历史结果，
合法外部 Snapshot/Replay/结果可延长生命周期；**现有代码没有任意会话时长、任意外部
handle 数量下的固定总堆上限**。不能把 native anchor 64 个误当全部历史状态的上限。

历史 frozen 元素表仍记录当时类型。新尺寸、源文件/hash、编译参数在
tools/verification/execution_acceptance/element-sizes-msvc-release.v1.json 单独登记，
其他 ABI/工具链使用前须重审计，不能把 MSVC 布局套到 MinGW。

## 3. reference 域容量与预算

N 为具名内容的 entities/requirements 上限；下表公式使用字节。MiB=1048576 B，
GiB=1073741824 B。除具名共享别名外各行相加；没有启用的音频/设备域记录实际 0 与不适用原因，
不能以未采到读数代替 0。两份 codec buffer、四份 owning 结果是测量夹具的同时保留声明，
不限制合法 SDK 用户的持有权。

| 域 | count / requested bytes 建议 | 来源与边界 |
| --- | --- | --- |
| World | 4096N + 65536 | 沿 B0 声明系数，新增规模待接受 |
| current | 2048N + 65536 | 同上；不能与 owning 结果重算同一共享块 |
| candidate | 4096N + 65536 | 同上；包括事务候选的域预算 |
| scratch | 2048N + 65536 | 同上；本轮工作临时量 |
| index | 1024N + 65536 | 同上；目标/绑定/分组索引 |
| decoded content | 32768N + 8 MiB | 新设计预算：完整图、字符串/表与解码拥有量；32 KiB/N 尚非最坏情况证明 |
| prepare-only transient | 65536N + 16 MiB | 新设计预算：同时准备、验证、编码中转；不把它藏在 fixture |
| execution retained | 32768N + 16 MiB；native anchors ≤64 | 新设计预算：所有日志结果、索引、旧代和共享状态的唯一存活量；不限于 anchor 对象本体 |
| CPU owning results | 4×(8192N + 1 MiB) | 新设计预算；4 个非别名拥有者；别名共享只计一次，编码 buffer 单列 |
| codec buffers/scratch | 2×256 MiB + 16 MiB | 两份有界编码 buffer；不拿 encoded 长度代 decoded heap |
| payload | 2×F(N) | P8 现有声明：2 个 payload；F(N) 见下方，其他 P0–P7 原 profile 不能偷换成此配置 |
| attempt | 64×(2×736+24+1+512) = 128576 | 严格沿实际 attemptBytes：两份 envelope、lease、dispatch token 与 512 B 元数据 |
| ready | ≤8 槽；bytes 别名到 payload/attempt | 不重复加 payload 字节；不得将 alias 写成缺测的 0 |
| receipt | 128×232 + 128×4096 = 553984 | 真实槽位加每个 4 KiB 有界结构化错误预算 |
| request history/consumer | 8×32×208 + 8×1192 = 62784 | 定长 history 与消费者元数据 |
| session channel | 128×39664 + 16 MiB = 21854208 | 32 个控制预留已包含在 128 总槽内；16 MiB 单列嵌套拥有载荷 |
| audio channel | 128×1688 + 16 MiB = 16993280 | 32 个预留已在 128 内；encoded 命令聚合独立预算 |
| host controls | 256×36920 + 1 MiB = 10500096 | 宿主记录与嵌套诊断字符串 |
| capture | 4096×(128+2×192+200)+8 MiB = 11304960 | pending、batch、tickMembers、trace 及嵌套载荷；128 控制预留不额外加槽 |
| steps | (4096×3+128)×912 + 4 MiB = 15517696 | software/renderer/capture 三个 ledger 加 audio；诊断动态量独立预算 |
| journal | 10832000 | 8 MiB 动态量 + 4096×(456+136) +128×(136+8)+8×16；不将字节上限当整张 ledger 大小 |
| static/bookkeeping | 8 MiB | 新设计预算：固定控制结构、缓存及未分配到其他域的产品元数据 |
| audio PCM/transport | 512 MiB + 16 MiB | ≤4 注册 clips、每 clip ≤256 MiB、注册总 PCM ≤512 MiB；旧 lease 的存活 PCM 也计入此 reference 聚合预算；**PCM 属 CPU 堆** |
| GPU backend allocation | ≤2 个 active/candidate/retired 代，聚合 ≤64 MiB | 新设备预算；不同 owner/generation 仍按实际退休寿命记录，不能代指驱动实占 |

原函数 frameByteBound(N,2,N) 在当前 Release 布局下：

~~~text
F(N) = (304+736+1024) + 2×2×16
     + N×[2×320 + 4×(512+32)]
     + N×[2×96 + 2×(512+32)]
     = 2128 + 4096N
~~~

2 个 payload 的数值只用于该 P8/scale profile；P1–P7 已接受的 8 payload、1/2/4 captures、
每帧 1 MiB、8/64 MiB 域等配置继续逐项展开，不套此表替换原工作负载。
统一表可给更宽的外围检查，但不能覆盖原 profile 中更严格的槽数或字节门。

**这些表是验收预算，不是峰值预测。** 容器容量、嵌套字符串、共享页目录、历史结果、
ControlReceipt 中的错误/输入 trace 等均可能令本候选超界；需按真实拥有关系取证，
不能把所有分配塞入 static 或以总额未超代替分域通过。若某个域无法证明映射，
该域保持缺证据并阻断正式准入，不将建议余量视为未知内存的通行证。

## 4. 总包络、storage 与机器保护

CPU requested 合计包含 audio PCM，排除 GPU 设备分配、夹具和观察器；CPU 域合计再加
声明余量 1/8，余量不改变各域自身的上限。表中的 5/4 storage 比例是**拟接受的独立
storage 验收预算**，不是宣称 CRT 必然满足该比例，也不用于从 requested 推算实测 storage。

| N | CPU 各域合计 / B | 加 1/8 的 CPU requested / B | requested / MiB | CPU storage / B | fixture requested / B |
| --- | ---: | ---: | ---: | ---: | ---: |
| 64 | 1261763744 | 1419484212 | 1353.73 | 1774355265 | 20971520 |
| 2048 | 1629486240 | 1833172020 | 1748.25 | 2291465025 | 150994944 |
| 40000 | 8663661728 | 9746619444 | 9295.10 | 12183274305 | 2638217216 |

| 独立口径 | 建议值 | 用途 |
| --- | --- | --- |
| fixture requested | 65536N + 16 MiB | 与产品分账，包括夹具生成/副本；不是 decoded 产品 heap 的排除理由 |
| observer requested | 2 GiB；最多 8388608 条分配记录 | 运行前校验表容量；量程不足停止并保留，不能判为产品内存超界 |
| process private commit | 16 GiB | 独立机器保护和结果披露，含产品/夹具/观察器/运行库；不可与 requested 做减法证明通过 |
| process working set | 16 GiB | 独立驻留检查；不要求 Close 后 RSS 回零 |
| GPU backend allocations | 64 MiB | 明确拥有的 buffer/texture/query 等 active/candidate/retired 聚合 |
| GPU driver dedicated/shared delta | 256 MiB | 相同进程空载基准和独立探针；驱动噪声/不具备归属时缺证据，不拿它代 backend requested |
| 全机剩余 RAM / 磁盘 | 启动前至少 8 GiB / 12 GiB | 机器保护；运行期间测资源趋势，不因压力追加或替换正式样本 |

16 GiB 是约 31.3 GiB 可见内存机器上的运行保护线，不是 SDK 承诺。40K proposed CPU storage
约 11.35 GiB，若再加最大 fixture 和观察器已接近该线；这是刻意保留的可证伪边界，
不能用预算相加推断必能完成。单次所需资源不成立则不启动该单元，提交范围/预算处置。

## 5. age、代数、别名与释放

| 对象 | count/age/lifecycle 规则 |
| --- | --- |
| delivery/request/receipt | 原 receipt/request deadline 5000000 µs；超期记录真实状态；外部合法 owning handle 不因该 deadline 自动失效 |
| observation journal | retention 1000000 µs 的 scale/tight 单元及 Player 60000000 µs 单元分别冻结；receipt deadline 均 5000000 µs，不混用 |
| history | 每消费者 32 条环形覆盖，8 consumers；按 count 有界，不给合法常驻环编造总年龄期限 |
| commands/steps | 总体 reply deadline 5000 ms；它是内部等待保护，不能替代 Pause 50 ms/Close 1000 ms 接受门；终态直到显式确认仍计槽 |
| native index/history | native 64 个，日志按真实记录数/共享块计费；native count 不替代 retained bytes；归档/结果 prolong lifetime 继续计入原拥有域 |
| source/current/candidate | 本有限 workload 同时 current+candidate，旧代数逐次记录；历史结果/handles 的总存活受对应设计预算检查，不能归零处理 |
| CPU owning leases | active、Close 后保留旧合法 handle、最终 release 三阶段；不为通过而提前丢弃原轨迹规定的 owning 值 |
| GPU/audio | fence/回调隔离与 owner 回收完成后检查；必要等待仍计入 Close，不以“lease”名义扣掉 |

最后释放后，payload/attempt/GPU 对应 live 应为 0；声明允许的 World/cache/journal
回到各自有界稳态。encoder 临时量、prepare-only 仅在各自阶段出现；保留 handle 的域不能
把 Close 后非零直接判泄漏。计费按照唯一 allocation/共享块，域视图与 owner 引用数单列，
不能靠每个引用乘全量或把共享对象全部忽略来改变结果。

## 6. 正式采样前仍必须关闭的技术准入

O1 只接受数值；代理继续完成以下 R4-4 条件后才能正式运行：

1. 新 collector 对 World、prepare-only、decoded semantic heap、execution retained/owning、
   command 嵌套数据及设备域有完整标签/分区核对；旧 memory_domains.py 的 excluded/unrated
   分类不能直接证明这些新域合格。保留 observer.py::memory() 原语义，新映射版本化。
2. 对 caller/controller/source/tool/fixture/二进制、实际配置、设备/端点、电源和容量建立新
   binding、一次性 seal；候选 C 中无未登记产品差异。未绑定或预算未接受的 profile 一律拒绝。
3. 正常计时、阶段例外归因、内存/设备探针分开进程；观察器自用开销和量程先校验。
4. 原 P0–P8 负载/消费者/控制数不变；新外围预算不改旧严格门，P8 low/high 各一次；
   采样执行/语义失败按既定规则停止，性能超界保留固定组，不事后升高本表。

## 7. Scheduling/Tail 与生产参数建议

| 参数 | 当前可复核 reference | 推荐后续生产目标 / 处理 |
| --- | --- | --- |
| 时钟 | engine.tick.us.v1；固定速率 1/1 | 保持；A 来自内容校准，I/V 具名冻结，不能以帧 double 累加 |
| observationValidityUs | Player 500000；scale probe 1000000 µs | 10000 µs 作为校准目标；必须测发布间隔/调度压力并绑定 measurementIdentity，不能直接缩短旧 reference |
| maximumErrorUs | Player 20000；scale synthetic 10 µs | 2000 µs 作为目标；真实设备若仍声明 20000 则不能冒充达到 2 ms |
| retainedMessageCapacity | Player 4096；scale probe 65536 | 4096 条作为初始候选，按实际负载/最长阻塞审计 count/bytes/age，不接受任意输入率 |
| audio serviceInterval | 2 ms | 保持候选；实测调度与 clock/cache age，未测不能生成 production measurementIdentity |
| Tail fixedRate / anchor error | 1/1；误差不得小于 EOF 真实 errorBound | 1/1、maximumAnchorErrorUs 2000 目标；reference 可按真实 20000 预算，分别标记 |
| Tail inputDeadline / terminal | 内容有效输入期限；terminal=max(lastTimer,inputDeadline)+closeThreshold | 沿语义按内容计算 exact Tick 并在注册表展开，不填全局 0 或给所有谱面同一常量 |
| Player session/step/capture | 128/32；4096/128；4096/128（总数/控制预留） | 原值作为容量候选；满时明确拒绝/暂停，不新增线程作为默认处置 |
| Player audio/host | 128/32 audio channel；host 256 | 原值作为候选；等待/回收和嵌套载荷计费已纳表 |
| codec | snapshot buffer ≤256 MiB，records≤100000，elements≤67108864 | 仅现有 testOnly workload 声明，不能把数值换入 productionCodecBudget；其他 Reader 保留原独立物理/hash 门 |
| 240 FPS 能力 | 已登记仅渲染 1800、静态 1300 | 保持；40K 是功能/完整规模取证，所有新目标仍逐设备/场景接受 |

productionCodecBudget、实际生产启动路径和 measurementIdentity 的生成/验证仍需后续独立
收敛。**推荐本次接受 reference 数值和生产校准目标，保留生产启用门。** 这是提交可实施的
具体目标，不把未知硬件能力预先写成已测参数。

## 8. O1 接受记录

owner 于 **2026-11-05** 裁定：“裁定使用方案B”。该裁定接受本包 §3–§6 的 24 域预算、三个规模、
storage/进程/设备独立口径、生命周期/别名与超限处置，适用于具名的 64、2048、40000
reference workloads；40K 仍为完整规模单侧 low/high，不适用 240 FPS。§7 的生产参数只获准
作为校准目标，不构成已测生产值、生产 profile 或移除任何 `testOnly` 门的授权。

当次裁定的文档及机器表 SHA-256、当前登记版本与运行授权边界见
[O1 接受记录](../../../../../stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-05-o1-budget-acceptance.json)。
机器预算提案仍保持 `accepted: null` 与 `formal_registration: false`，避免把数值接受误写为正式注册或运行许可。

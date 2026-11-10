# PD-5 40k Gameplay-host B0 预算裁决交回

状态：completed

更新日期：2026-10-22

快照日期：2026-10-22。B0 分析与交回完成；**下列 envelope 是提交 owner 接受的建议，尚未接受**。
本报告不授予 B/C 开工、revision 4 产品准入或正式测量资格。当前状态由
[CURRENT_STATUS](../../../../../../../CURRENT_STATUS.md) 汇总。

## 1. 输入、边界与核查结果

从 `codex/pd5-gameplay-40k-admission` 的干净提交
`3b550ad3f3d733d165951408eb6a1858aeb82856` 开始。已读取
[阶段 A 报告](../stage-a/2026-10-21-gameplay-admission-stage-a.md)、
[会计草案](../../../../../../../proposals/implementation-input/stage-07/pd5-capsule-r4-accounting.md)
和桌面 `Cuexis-Stage7-PD5-40k-Gameplay-host-准入修复实施方案-2026-10-21.md`。
桌面方案是实施输入；本轮用户的 **B0 后停止** 要求限定执行范围，方案中的 B/C 不是本轮授权。
桌面文件 SHA256 仍为 `160371d9ee02f96f044fb7b65b2dcdbaca94a0cdd511381fffe7c0713a57dd6a`。

裁决依据为 [Capsule §§4–12](../../../../../../../formats/GAMEPLAY_CAPSULE_V2_FORMAT.md)、
[Packed §3.3](../../../../../../../formats/PACKED_CHART_FORMAT.md)、
[Gameplay Spec §§3.8/5/8.5](../../../../../../../formats/GAMEPLAY_V2_SPEC.md)、
[Chart Entry 的 S7A-7/8 节](../../../../../../../formats/CHART_ENTRY_V1_FORMAT.md)，以及
[CM-V07 裁定](../../../../01-a/s7a-7-8/2026-10-03-s7a-7-wrapup-rulings.md)。
报告给出待接受的合同差异；现行生产 Spec、旧 revision 与公共 API 本轮均未改写。

本轮只读复核结果如下；不是重跑阶段 A 的原生观察或测试：

| 对象 | 核对结果 |
| --- | --- |
| 工作树 | admission 分支起点精确为 3b550ad，起始无 tracked/untracked 变更；主树仍为干净 F1 eed36648a692333e47f798aa45be36ca850d08de |
| A 证据包 | 6,161 项库存、ZIP 内容和 receipt 校验通过；ZIP SHA256=`590cbaf5f3bc37d9b6a1d521d693210deb03d73cf4339e04d99c7ae5c91a09fe` |
| A 源码/构建 | 实测 source HEAD 仍是 083275d，不冒充交接提交 3b550ad；插桩库存 fe6f884d7f45df00b2ed5a788deb5009def0a0948decbf1fab1aa26a7889fedb、plan、工具、fixture、23 项构建绑定均吻合 |
| A 计划/绑定 | plan SHA256=96341e0bca6f1766b5e99f179ecedc3e3d346d4073df0977883ec3e4cc2ed338；binding SHA256=e9f74bb42bdf032c4b70ada4600ed290b8c6aae792c7449cc0ebb2c9d0a39370 |
| 历史冻结证据 | 原三个 protected 根共 14,202 文件，文件集合和逐文件 SHA 均一致；两份 pause 快照 341/442 项及历史 301 项构建绑定通过 |
| C2/停止状态 | C2 源码/构建/工具绑定通过，独立工作树仍为 747ef18；旧窗口 3 claim 仍不存在，继续 STOPPED |

A 的 Release STATIC、candidate ON、metrics/trace/lifetime observer OFF 身份未变；本轮不配置、
重建或启动其 exe。原七项物理门禁继续为：file/decoded/section 各 16,777,216 bytes，
entities/requirements 各 40,000，strings/references 各 100,000。1.10/15%、F1 比较基线、
生命周期与原子发布规则不变；没有读性能候选、正式 preparation 或 registration。

额外比较当前 checkout 与 A 原始 Git archive 时，初始 raw-byte 核验只在一份 Python 测试的
CRLF/LF 差异处停止；修正核验口径后，编译输入逐 byte 相同，该测试去 CRLF 后也与归档一致。
失败日志/脚本另存不覆盖；这不是 A 构建输入变化，A 插桩源码和全部绑定仍按原始 bytes 校验。

## 2. 建议接受的四维 envelope

建议内部 profile ID 为 `capsule4.hash-work.v1`，只适用显式物理 revision 4。
四项是同时满足的独立硬上限，恰等于允许、超出拒绝；按 `min(requested, accepted)` 求有效值，
0 始终是字面上限。下表的政策倍数是本次提请 owner 裁决的资源选择，**不是阶段 A 测出的许可值**。
物理文件可容纳，不保证落在新 hash profile 内；本 profile 也不承诺整个 prepare 的堆峰值或耗时。

| 字段 | 建议 accepted 值 | 单位/资源政策 | 原样 40k 的依据 |
| --- | ---: | --- | --- |
| maxSemanticPreimageBytes | **134,217,728（128 MiB）** | 单次完整语义 preimage 的 SHA 输入 bytes，绝对流量封顶；选原 16 MiB 物理包络的 8 倍作为独立展开工作级别 | 99,588,531；余量 34,629,197 bytes，使用约 74.20% |
| maxExpandedReferenceVisits | **1,000,000** | 完全展开的语义 Ix 访问次数；选原 100,000 物理引用行上限的 10 倍作为独立操作级别，不是把 REF0 上限放大 | 完整口径为 440,001；余量 559,999，使用约 44.00% |
| maxProjectionKeyBytes | **4,096（4 KiB）** | 任意一个完整 cxgp2: UTF-8 文本的逻辑 bytes，含前缀，不含字符串 NUL 或外层 text framing | 最大 274；下文给出可覆盖的完整身份结构示例，不以 274 直接取上限 |
| maxHashScratchBytes | **8,388,608（8 MiB）** | 单次 hash/count 调用同时保留的专用 workspace 请求容量，含全部容量/固定工作集；选物理包络的一半，约束流式实现不能回到大 preimage/字典 | 下文拟定布局的保守计划为 5,669,984；这是设计账本，尚无实现高水位验证 |

128 MiB 与一百万次只是明确的有限资源等级；没有新的时间样本证明某阈值“恰好最快”或满足
读取性能门禁。接受它们表示接受该工作量级别，不表示旧 16 MiB hash 检查被证明错误。
比值 8 和 10 也不构成所有输入的放大率承诺；小文件仍可因任一绝对上限被拒绝。

### 2.1 preimage 与 Ix 的计算口径

preimage 仍按原 semantic codec 的所有字节计费：domain/NUL/version、static prefix、global、
requirements、definitions、coordination；每次重复展开都算。阶段 A 分拆为
`2,920,218 + 894 + 75,111,124 + 1,909,192 + 19,647,103 = 99,588,531`。
嵌套 expansions.bytes 不能再相加；只用互斥阶段分拆求总数。流式 SHA 只减少存储，不减计费。

**A 的计数口径补充，原始证据不覆盖：**A 的 `400,001 = 10*N+1` 是 `index()` 钩子调用和，
不含 `requirementRow` 对 ENT0 的直接 lookup。GPR0 的 entityOrdinal 是 Ix，hash 中按完整
canonical entity identity 替换，按本次完整定义必须再计 N 次；因此为 **`11*N+1`**。
N=1/64/40000 的新口径分别为 12/705/440001。此为读取原日志与原 visitor 后的静态复算，
不是新的原生样本，不修改 A 日志、汇总或报告。

完整定义是：每次沿一个 wire Ix 语义展开边取目标，计 1；嵌套、重复均计入，包括 ENT0、
Claim→Requirement 的 E-only 访问和 Claim/Relation→Resource。只写稳定 ID 文本的 R 引用、
列表自身、Pattern 内联 tree edge、虚拟 projection 文本访问不另冒充 Ix；它们的 bytes 仍计入
preimage，遍历工作集计入 scratch。memo 命中仍按逻辑展开次数计费，不能按 cache miss 计费。
owner backlink 的结构校验不再作为第二次语义展开收费，也不能把 E-only 回指递归成完整 Requirement。

计数 visitor 先按唯一 typed record 得到 `(bytes, expandedVisits, maxKey)` cost，再按合法引用
和重复次数 checked 加乘。共享 Pattern/Measure/Domain/Solver/Binding 的 cost 可复用，
语义字节与 visits 必须按使用次数累加；禁止先完整展开恶意输入才发现超过预算。
Pattern 原语 tree 是内联声明，不按 repeat 的语言基数展开运行轨迹。

### 2.2 单 key 的结构计算与政策依据

令 `Ebytes = 24 + 五个文本的 bytes 和 + Σ(nodeId bytes + 12)`；24 为六个 u32 framing。
Emission/identity projection 的完整文本为 `6 + 2*(1+Ebytes)`，Claim 为
`6 + 2*(1+4+namespaceBytes+Ebytes)`；原始 UTF-8 bytes 经 lowercase hex 加倍，不能按字符数计费。
此上限覆盖所有 cxgp2: 子域及虚拟/实际 key，不只检查被省略的三类。外层 R kind/text 长度前缀
进入 preimage，不进入 key 本身的 4 KiB；如临时拥有 NUL，NUL 必须进入 scratch。

4 KiB 可容纳一个明确的结构级别：E 的五个文本各 256 bytes，namespace 256 bytes，
16 个 path node 各有 18-byte ID，得到
`6+2*(1+4+256+24+5*256+16*(18+12)) = 4096`。
这是资源政策示例，不把 ChartLimits 的 identifier 字段偷换为 E 的新逐字段规范；其他长度分配
只要完整 key 仍 ≤4096 也允许。更长合法结构可以在物理预算内但被本 hash profile 拒绝，
不得截断、hash-shortening、折叠 path、删 namespace 或重排身份以求通过。

### 2.3 scratch 的可执行分配账本要求

A 已知的 `5,120,806 = prefix capacity + 两 key buffer` 只是局部下界，**不用于证明 8 MiB 足够**。
下面选择一种可实现、可被 B 的分配观测推翻的设计约束：借用已验证的 owning typed 输入，
通过 sink 写 prefix/key/定义；不复制 Model/graph，不建长 key 集合，不物化完整 prefix/preimage。
任何仅为 hash 新建的副本、索引、排序空间都必须记账，不能改名为“输入”逃费。

令 Q 为实体、Requirement、GPD0 和 GRC0 全部实际行以及一条 global 的总数；
E/R 为实际实体/Requirement 数，H 为显式非递归遍历同时活跃 frame 数。拟采用：

| 同时保留的 workspace | 请求容量约束 |
| --- | --- |
| 每个 typed row 的 cost 与辅助槽 | `32*Q`：3×u64 cost 加 2×u32 辅助槽；不假定 STL node/map 的 sizeof |
| Requirement owner/backlink/claim 校验槽 | `8*R`；不能存第二份完整 E/claim key |
| Entity owner lookup | `4*E`，只定位，不进入身份与排名 |
| 固定工作集 | 65,536 bytes：含 SHA 状态/固定栈开销、primitive framing、hex/比较 chunk、ledger/control 和有界诊断；B 以布局断言核对 |
| 非递归遍历 stack | `4096*ceil(H/255)`；每页 16-byte 页头、255×16-byte frame，不使用递归 C++ 调用栈 |

因此计划峰值为 `32*Q + 8*R + 4*E + 65536 + 4096*ceil(H/255)`。
额外的 cost role、canonical sorting 临时区、第三方分配、对齐 padding 或新 buffer 若不能放入上述
已计容量，均须加收；不能凭此公式豁免。辅助槽若布局不足必须回报设计，不能暗增每行 sizeof。
声明记录的 E-only 与 full-row cost 必须区分，避免把合法 Claim→Requirement E-only 回指当成递归环。

原样输入 Q=`40000+40000+40006+40004+1=160011`。其 Pattern hash row 最大 31 bytes、
E-only 最大 116 bytes，分别至多容纳 6 个最小 Pattern node、7 个最小 path step；
配合不超过 16 层固定 schema frame 的拟定遍历，H≤255 可由一页覆盖。
代入得到 **5,669,984 bytes**，余量 2,718,624。这是对拟实现布局的条件上界，
不是当前产品、A 工具或全进程实际 heap 峰值。若 B 实现不能兑现布局/深度条件，本报告不授权
提高 8 MiB，须交回设计；不把“尚未实现”改写成“已测足够”。

所有容量在 reserve/allocate 之前 checked 计费，计实际 capacity 而非 size；新旧 buffer 迁移
同时存活时两份都算。页式增长不复制旧 stack；池/arena 已保留容量在真实释放前持续记账。
计数、校验、hash 和 metadata 调用内的共享 workspace 不得各自重置 ledger 叠加逃限。
输入业务对象的固有内存与原物理解码存储另受已有合同控制，但 hash 生成的任何物化数据不属豁免。
物化 semanticPreimage 的导出 buffer 也须计入同一 scratch；大输入只提供有界 digest，
不能把 128 MiB 的工作量上限当成允许分配 128 MiB vector。

## 3. 攻击性输入、拒绝顺序与诊断

| 攻击/反例 | 必须观察的门禁与拒绝路径 |
| --- | --- |
| 很大的共享 Pattern 被 40k Requirement 引用 | 唯一定义计数后乘引用次数；超过 preimage 先拒绝后 hash。1 MiB×40k≈39.06 GiB 是放大模型，不声称满足全部现行 Pattern/物理条件 |
| 大量很短的合法 Ix 重复列表/深层展开 | 即便文本短、preimage 尚可，expandedVisits >1,000,000 也拒绝；memo 不减少逻辑 visits |
| 长 E、path、namespace、多字节 UTF-8 或合法未使用 cxgp2: 引用 | checked key 公式/实际物理 key 长度 >4096 拒绝；未引用不构成豁免；验证 framing/hex 合法性，不创建第二份长文本 |
| 巨量小定义、深 Pattern、cache/stack/排序工作区、先复制大 prefix | 在触发分配前按所需 capacity 拒绝 scratch；B 必须证明没有未计分配和无界递归 |
| 新旧字段混写、未使用的 R11/R12/R13:1、坏 owner/backlink、重复 E/K4 | 结构/closure 拒绝，不用“预算足够”使其合法；超宽 wire 字段和算术溢出不能截断 |
| caller 请求放宽、文件自报更大 envelope、0 被当无限、metadata 走旧字典 | 固定 profile/min/0 与统一入口测试拦截；revision 4 digest 不得以旧 staticTables 成功为前提 |

新 revision 的拒绝顺序固定如下；旧 revision 的顺序不变：

1. file/metadata 外层物理限制、Header magic/version/revision、flags/directory/CRC/section 和原七项预算；
   未知 revision 早拒绝，不猜布局。metadata 先核 path/encoding/exact artifact hash。
2. 有界 typed decode，核实际 counts、排序、字段宽度/UTF-8、REF0 合法子集、owner/backlink、
   claim intent 字段形状、E/K4 唯一性与 typed closure。任何工作区均先受对应安全约束。
3. hash 预检：先检查固定 workspace/所需 cache 容量；对唯一结构计数，过程中每次 stack/cache
   增长先检查 scratch。已完整计数且无更早安全失败时，按 **key → visits → preimage** 判断。
   不能为维护错误码优先级继续超限分配；scratch/结构失败可先于尚未求出的其他预算失败。
4. 仅计数通过后增量 SHA；复用同一 semantic atom visitor/分帧，actual bytes/visits/maxKey 必须
   与 count 对账，sink 在每次写入前守剩余额度。对账差异作为内部一致性错误，不能静默采用某个摘要。
5. 比较 Header/metadata digest；完成同一 typed prepare、执行/资源/能力/proof 门禁和完整 prepared
   identity 检查后，才原子发布。前置唯一性检查不代替最终 shared prepare 的 K4/containment 复核。

每次调用计算一次该摘要；同次 Writer/metadata 内用已验证的局部结果，不持久化全局 cache。
独立调用各自受限，此 envelope 不承诺宿主整个生命周期的累计 hash bytes。

四种新预算失败统一复用**已注册** `capability.budget_insufficient`，category=`budget_exceeded`、
severity=`error`、faulted=`false`；不用不存在的 packed.* 公共 token，也不复用 decoded_bytes
假装是物理尺寸失败。底层 Result 与 Playback/metadata 入口保留相同分类，具体区分由以下上下文给出：

| budgetId / fieldPath | originPath 示例 |
| --- | --- |
| maxSemanticPreimageBytes / semanticHash.preimageBytes | GPR0.requirements[i].pattern，或 semanticHash.total |
| maxExpandedReferenceVisits / semanticHash.expandedReferenceVisits | GPR0.requirements[i].entityOrdinal、claims 或 bindings |
| maxProjectionKeyBytes / semanticHash.projectionKeyBytes | GPR0.requirements[i].identity 或 GRC0.claims[i].namespace |
| maxHashScratchBytes / semanticHash.scratchBytes | semanticHash.cache、stack、sort 或 exportBuffer |

另外携带 profile、phase（count/hash/export）、limit 和 required，未求出完整需求时用
requiredAtLeast，明确 exact=false；不把下界写成精确需求。全部值用整数，不经 f64。
ordinal 仅用于定位。诊断不复制攻击者的巨大 key/token，字段名/数值有界，计入固定工作集。
checked u64 加乘只在已证明至少超过有效上限时可报告预算失败；允许饱和到 limit+1，禁止回绕。
非法 wire 宽度/循环/坏索引仍用既有结构诊断；预算内实际 OOM 用既有 OOM 错误，不伪称预算不足。
失败保留旧 active、旧有效文件和既有 view/token/close/reload 生命周期保证，不发布部分结果。

## 4. 类型与入口归属裁决建议

**建议用内部 accepted profile，四字段放在内部 HashWorkLimits 值类型中。**
profile 常量/checked ledger 属 gameplay_packed 私有实现；uint64_t 明确计量单位，转 size_t/线性
内存分配前另做 checked narrowing。可供内部测试/可信调用收紧，所有入口最后再 min；不存在外部
自报 profile 或放宽能力。profile 不进入 wire、metadata、semantic preimage 或 prepared identity，
同一内容在不同收紧限制下只有“可接受/拒绝”不同，不生成不同语义身份。

不往 installed `chart::PackedChartLimits` 加字段，不修改 Playback 公共配置、SDK API/日期版本、
Graph limits 或旧 EncodeRequest/DecodeContext 默认 revision。现有 gameplay_packed CMake 为
STATIC offline adapter，include 仅 BUILD_INTERFACE，明确无 installed headers；跨 Playback
需要的 digest 结果接口可放其**构建期内部接口**，不导出为 installed SDK 合同。
公开调用仍返回现有 Result/Diagnostic；四项成为被明确文档化的 revision 4 接受 profile，
不以内部常量位置为理由隐瞒门禁，也不凭 B0 新建公共预算 API。

Writer、Reader、Header 验证、file/memory 桥、显式 capsule.4 metadata/CXC Gameplay factory
统一经过 count+digest。metadata 生成不得先走 revision 3 的 STR0/REF0 或完整 preimage vector；
entry 不能用自己宣称的 compiled hash 代替重新计算。旧 graph.1/capsule.3 入口继续原合同，
不因共享 helper 获得新预算或隐式升级；非 Gameplay factory 继续不自动发现该 extension。

## 5. revision 4 的最终拟议差异与 semantic codec

本节冻结本次交回的**一个**拟议布局，待 owner 接受后由 B 同步到 owning Specs；本轮未实施。

| 位置 | revision 4 相对 revision 3 的差异 |
| --- | --- |
| Header/目录 | candidateRevision=4；packedVersion=1、chartVersion=5、gameplayVersion=2、graphRevision=2；96-byte Header、32-byte directory、既有 flags/CRC/framing 不变 |
| Gameplay section framing | schemaRevision=1、rowCodec=1、flags=0、16-byte framing 不变 |
| GPR0 第 1 组 | 精确为 entityOrdinal:Ix(ENT0), stableId:D, identity:E；删除 emissionRef:R12 和 identityProjection:R13:1 两个 wire 字段 |
| GPR0 其余 | 原 §§6/12 的第 2–8 组与 phaseTargets、atomBindings、independentCompetition 全保留，次序不变；完整 E/path/D 不变 |
| GRC0 Claim 完整行 | requirement:Ix(GPR0), resource:Ix(resources), intent:u8, policyToken:S, explicitNamespace:O(S), competition:O(priority:Z,tieRank:V), slot:O(U), graceOverride:u8=0 |
| Claim 删除 | claimKey:O(R11) 的 presence byte 和 payload 全删除；仅 wire 删除。observe 的 namespace/competition/slot 全 absent；占用型全 present、namespace 非空、slot=0；consume/claim 不合并 |
| REF0 | kind 1–10 原合法子集保留；11/12 全拒，13 只允许 0/2–8，13:1 全拒；即使未使用也拒冗余行；0/14–255、未知子域拒绝 |
| counts/STR0 | Header/GPH0 的 referenceCount 仍是真实 REF0 行数；重新收集实际保留字段需要的 STR0/REF0。不得删除兼有真实引用用途的 token 或规范要求保留的 unused definition |
| 其他 | entities、资源、定义、声明、关系及有序成员、要求动作、namespace/policy/competition、presentation 及 execution 字段全保留 |

逻辑三角色仍独立，checked 算 `2*requirements+occupyingClaims <= 3*requirements`；
40k 为 120,000，不受错误的“物理 refs≤100000 ⇒ 逻辑角色≤100000”推导限制。
这不是新增一个无界表；须按需要由 E/namespace 派生，不能生成第二份 120k 长字符串字典。

| 物理 revision | 显式 semantic codec | Header/compiled semantic identity |
| --- | --- | --- |
| 1 | 原 Foundation codec | 原路径/domain/golden 完全不变 |
| 2 | Gameplay.1 | cuexis.chart.semantic.v5.gameplay.1 + NUL + 原 version/framing |
| 3 | Gameplay.2 | cuexis.chart.semantic.v5.gameplay.2 + NUL + 原 version/framing |
| 4 | Gameplay.2 | 与 3 的完整 preimage 逐 byte 相同；仅新 count/sink/profile 路径 |
| 其他 | 无 | Header 早拒绝，不能 `>=3` 或 else 回退 Gameplay.1 |

hash visitor 在原位置**补回完整语义表示**：E 后的 R12/R13:1 kind/text/framing、Claim namespace
后的原 claim-key presence 与 R11 kind/full text，以及原 codec 的所有重复 Ix 展开。
不能直接 hash 精简后的 wire rows，也不能省略已删的 option byte、hex 或用摘要代替结构。
execution 字段依据 semantic codec=Gameplay.2 分派，不能仍以 physicalRevision==3 选择 domain/字段。
Header.semanticIdentity 与 metadata.compiledSemanticIdentity 用同一有界结果；artifactIdentity
仍对 exact wire bytes 求 SHA。CM-V07 不允许把物理 revision 放进 compiledSemanticIdentity。
Judgement 的 ChartIdentity、ContentIdentity、PreparedIdentity（engine/ruleset/chart/session 四分量）
及 canonical Replay bytes 必须另作逐项比对，
Header 摘要相同不替代这些门禁。

## 6. 兼容矩阵与必须完成的合同更新

| Reader/入口 | revision 1 | revision 2 | revision 3 | revision 4/未知 |
| --- | --- | --- | --- | --- |
| 原冻结 Reader | 原行为 | 原行为 | 原行为 | 4/未知在 Header 不支持处拒绝；不落入旧布局 |
| B 后显式 dispatcher | 原 Foundation 分派 | 原离线/legacy 路径 | 原 execution 路径 | 4 独立布局+Gameplay.2+本 profile；未知拒绝 |
| execution publication | 原不适用 | 仍拒绝 profile_incomplete，不补字段 | 原完整 shared prepare | 4 同样完整 prepare/closure/lifetime 门禁；不是 digest 通过即成功 |
| 旧 Writer/default | 原行为 | 默认/显式选择不变 | 显式 3 不变 | 不自动改写成 4；不能向 2/3 丢弃新增/不兼容字段 |
| 新 Writer 显式 4 | 不串用 | 不升级/降级旧 artifact | 可对等价内容作 oracle | 仅新 layout；拒混写或旧冗余行；七项物理预算仍逐项检查 |
| metadata | 原 Foundation entry | 不新增 Gameplay alias | packed-chart/capsule.3、gameplay-graph/graph.1 原义 | packed-chart/capsule.4 明确新增；与 Header 不匹配即拒绝，即使 digest 相同 |

旧 revision 2/3 的字节、golden、预算值、拒绝路径/顺序均不重解释，不把新 profile 反套过去。
graph.1 不改 wire/schema/hash/原限制；没有自动“读不进就转换 4”的 fallback。
未支持 capsule.4 的旧 metadata parser 拒未知 encoding；旧 CXC/非 Gameplay factory 不自动识别新入口。

owner 接受后 B 必须一次对齐 Capsule §revision4、Packed 的 explicit dispatcher/工作预算归属、
Gameplay Spec 的物理与工作成本分层、Chart Entry 的 encoding 闭集/semantic codec 映射及 CXC
Gameplay extension 消费合同。metadata version 不借机升级，新增 encoding 必须有严格的匹配表；
尚无公共 API/SDK 变更授权。任何发现需要改旧版语义/身份/API 的情况先交回。

## 7. B 的验证清单与本轮验证范围

以下均是**待实施/待运行**的清单，不是 B0 通过声明：

| 组 | 必需检查 |
| --- | --- |
| 四维边界 | 每维 cost 恰等于/超一单位、requested=0/收紧/放宽；正常 case 用实际 cost 与 cost-1 收紧复现。key 文本恒偶长：实际 4096 接受、4098 拒绝，另以 limit=4095 验证超一 byte；计费器覆盖整数 4097 |
| checked 算术 | u64 add/multiply/narrowing 溢出、limit+1 饱和、u32 wire 上界；未得到完整 cost 的 requiredAtLeast/exact=false，不能包回或冒充精确需求 |
| visitor 对账 | N=1/64 完整 old3 oracle preimage 与 new4 导出逐 byte/SHA 相同；ENT0 必计使 visits 为 12/705，重复/嵌套 Ix、E-only backlink 和已删物理 option 仍进 hash |
| 抗放大 | 大共享合法定义、短引用密集、长 UTF-8/namespace/E/path、深 Pattern、巨大 unused projections/definitions；满足其余条件且确由目标预算拦截，不靠别的非法字段“通过负例” |
| scratch | 分配 tracer/受控 allocator 对账完整 count→hash→metadata 的容量/重叠高水位；所有排序/aux/export/异常路径、cache/stack 耗尽在分配前拒；32-byte 槽/16-byte frame/64 KiB 固定集断言；预算内 OOM 独立分类 |
| wire 严格性 | 全字段 typed round-trip；R11/R12/R13:1 冗余 unused 行、旧字段混写、未知 revision/subdomain、Header/GPH/实际 counts 不符、未知/重复/缺字段；负例重算 CRC，确保命中目标检查 |
| 身份/行为 | 完整 E/D、owner、K4、observe/consume/claim、独立 competition、ordered relation/资源、execution/presentation/闭包逐项等价与 mutation 拒绝；Chart/Content/Prepared 分量和 Replay canonical bytes 对比，不只比较 Header SHA |
| 入口一致性 | Writer/encode、Reader/decode、read/writeAtomic、decodeInto、Header 校验、metadata 生成/读取、显式 Gameplay file/memory/CXC factories 均不能绕过四维；坏 exact hash、encoding/Header 错配、closure/hash 伪造稳定拒绝 |
| 旧版回归 | revision 1/2/3 的 golden、七预算 min/0/顺序与 Graph v1 原行为全部保留；旧 Reader 拒 4，新 Reader 显式旧路径，default Writer 不变，低版拒不兼容输入 |
| 生命周期 | 失败时旧 active/文件/snapshot/token 有效性沿原合同；reload/close/publish 原子性、shared prepare/proof 与资源/能力验证不省略；不得加入 RT 分配、全局 cache 或后台任务 |
| 适用矩阵 | B 的 Debug/Release、架构、格式、文档、相关 native/Python、static/shared/外部消费者及支持环境 sanitizer/跨编译器 golden；Windows 不冒用不适用的 Linux preset |

待 B 通过后 C 才验证原样 40k low/high 的 Host→Snapshot→Replay→Seek→present→close；
不能把 A 的 proposed sizes 或 B 的预算单测当作已经到达 Host 的成功证据。正式性能测量另建
全新 Release/preparation/registration，不能复用本报告、A 或当前有限诊断登记。

本轮实际运行仅为只读完整性核验、原日志静态复算、文档检查和 diff 检查。
没有新 C++ 构建/CTest、分配高水位、Reader/Writer 或 40k Host 结果，阶段 A 原测试结果不重标日期。

## 8. 交付与停止点

新证据根 `out/pd5-b0-envelope-20261021-3b550ad`（开始于 21 日，跨午夜后交回）。
`stage-a-packet.log`、`integrity.json/.log` 保存初始核对；`arithmetic.json/.log` 与
`derive_envelope.py` 保存六日志引用/SHA、ENT0 补充口径、key 与 scratch 模型；
`stage-a-tree-before.json` / `final-integrity.json` 比对 A 全树及冻结目录。
`docs-check.log`、`diff-check.log`、`review.diff` 与 `handoff.json` 保存本轮验证、提交和差异；
新包的 index/receipt 提供完整性复核，不覆盖 A 包或历史目录。

本轮 tracked diff 只含本报告、报告索引与 CURRENT_STATUS；A 报告/草案、权威 Spec、产品源码、
测试工具、构建输入及旧 golden 均不改。报告提交后停止。owner 需先接受
**134217728 / 1000000 / 4096 / 8388608** 四项 envelope（及上述口径、内部 profile 与拒绝路径），
再决定是否进入 B。若不接受，只修订 B0 设计，不自动试第二套阈值或启动产品实现。

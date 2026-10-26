# PD-5 Capsule revision 4 与 hash 工作量会计草案

状态：candidate

更新日期：2026-10-21

本文件是[准入阶段 A 计划](../../../stage_plans/active/stage-07/04-pd/pd-5/pd5-gameplay-admission.md)的交回输入。
尚未接受新格式或预算，没有 Reader/Writer revision 4 实现。现行合同仍归
[Capsule](../../../formats/GAMEPLAY_CAPSULE_V2_FORMAT.md)、
[Packed](../../../formats/PACKED_CHART_FORMAT.md)和 [Gameplay Spec](../../../formats/GAMEPLAY_V2_SPEC.md)。
需求实测与未知事项另见[阶段 A 报告](../../../stage_reports/stages/stage-07/04-pd/pd-5/admission/stage-a/2026-10-21-gameplay-admission-stage-a.md)。

## 提议的物理差异

Header 的 candidateRevision 显式为 4；96-byte Header、32-byte directory、各 Gameplay section
16-byte schemaRevision=1/rowCodec=1/flags=0 framing 和原七项物理预算不变。
GPH0 仍写真实物理 referenceCount，必须与 REF0、Header 和实际解码计数相等。

| 表/位置 | revision 4 提议字段顺序或变化 |
| --- | --- |
| GPR0 第 1 组 | entityOrdinal:Ix(ENT0), stableId:D, identity:E；整段不写 emissionRef:R12、identityProjection:R13 |
| GPR0 其余 | 原 §6 第 2–8 组原顺序，然后原 revision 3 phaseTargets、atomBindings、independentCompetition；全部字段保留 |
| GRC0 Claim | requirement:Ix(GPR0), resource:Ix(resources), intent:u8, policyToken:S, explicitNamespace:O(S), competition:O(Z,V), slot:O(U), graceOverride:u8=0 |
| Claim 差异 | 完整移除 claimKey:O(R11)，包括 presence byte；observe 的 namespace/competition/slot 均 absent，占用型均 present，namespace 非空、slot=0 |
| 其他表 | 所有资源、定义、关系成员、有序列表、声明、实体、E/D 与执行字段原样保留 |

物理 REF0 的 kind 1–10 保持既有合法子集；11、12 不允许存在。13 仅允许子域 0、2–8；13/1
不允许存在。0、14–255、未知/错误子域继续拒绝。即使不被引用，冗余 11/12/13:1 行也拒绝，
不能混写新旧布局。未使用但现行规范要求保留的合法定义/其他引用不删除。

逻辑角色仍为 emission、identityProjection、claimKey 三种独立角色，全部由完整结构真源派生，
不建立第二张 120k 长字符串隐藏表：前两者从完整 E，后者从 namespace 与对应 Requirement E。
先 checked 计算 `2*requirements + occupyingClaims <= 3*requirements`，验证最多一条 claim、
唯一 owner/backlink、typed closure、重复 E 和 K4 唯一性，再按需要访问。Ix 是定位，不进入身份或排名。

## wire revision、semantic codec 与入口

采用显式映射，不能用 `>=3` 猜测未来版本：

| 支持的物理 revision | 执行布局 | semantic domain |
| --- | --- | --- |
| 1 | 原 Foundation 分派 | 原 Foundation 合同 |
| 2 | 原 Capsule 2 | cuexis.chart.semantic.v5.gameplay.1 |
| 3 | 原 execution 布局 | cuexis.chart.semantic.v5.gameplay.2 |
| 4（待接受） | 上述物理省略，execution 字段与 3 相同 | cuexis.chart.semantic.v5.gameplay.2 |
| 其他 | Header 早期拒绝 | 无回退/补零 |

revision 4 的 hash 仍写 revision 3 的全部完整结构投影文本、域、大小写 hex、framing 和重复展开；
不把物理省略变成 hash 省略。Header 与 metadata compiledSemanticIdentity 使用同一有界 digest，
artifactIdentity 随 wire bytes 改变。Judgement 四分量及 Replay canonical bytes 另行比对。
符合 CM-V07 的“物理 revision 不进入 compiledSemanticIdentity”，不改变语义身份强度。

新入口须明确 capsule.4；capsule.3/graph.1 原义保留，旧 Reader 读新 revision 早拒绝。
[Chart Entry](../../../formats/CHART_ENTRY_V1_FORMAT.md) 的 Gameplay metadata 闭集、CXC extension
和解析/比对链必须在阶段 B 同步明文更新。metadata 不能先构造旧 revision 3 字典来计算 digest。
默认 Writer、Graph JSON v1、SDK API/发布版本不自动升级；公共消费面变化需独立列明。

## 会计草案：有界计数先于 hash

物理 decoded bytes 只计算各 section codec 后的和，不再承担新 revision 的 hash work 计费。
旧 revision 仍走原路径、原拒绝与 golden；新会计不能作为旧 Writer 放宽的后门。

建议由一次接受的工作量 envelope 声明以下四个独立上限；本轮**没有接受值或默认值**：

| 提议字段 | 单位与精确定义 | 需要防住的输入 |
| --- | --- | --- |
| maxSemanticPreimageBytes | 按原语义 framing 完全展开后送入 SHA 的总 bytes，含 prefix 和每次重复引用 | 大共享定义反复展开、长 token 被重复引用 |
| maxExpandedReferenceVisits | 每次 Ix 语义解引用一次计 1，嵌套 Ix 逐次计入；不同类型分项可复算 | 大量很短引用、深链/重复展开；不把只存一份定义当只执行一次 |
| maxProjectionKeyBytes | 单个完整 cxgp2: 文本的逻辑 UTF-8 bytes，即使不物化也计费 | 长 E、长 path、长 namespace，hex 加倍 |
| maxHashScratchBytes | hash 专用同时存活 scratch 的请求 bytes：count cache、遍历工作集、分帧/hex chunk；不含 caller 拥有的输入，也不冒充全进程 heap | 无界 prefix/key/vector、过多 memo entries、恶意深度 |

接受后各入口统一使用 `min(requested, accepted)`，0 为字面上限；不能由文件自报一个更大值。
本轮实测只能给这些字段的必要需求，不能将需求自动当成许可上限。是否按明确复杂度 profile
接受工作量 envelope、硬上限取值与诊断码/path，均须先核验决定。

原样 40k 两种数值输入的实测必要需求为 99,588,531 preimage bytes、400,001 Ix 展开访问、
最大 274-byte projection key。完整 scratch 上界尚未证明；已测 prefix capacity 及局部 key
payload 只构成部分会计。所有四维允许值仍为未接受，不按这些最低需求直接赋默认值。
现有入口的 PackedChartLimits 只有七项物理限额；新增会计使用独立类型还是内部 accepted profile
载体、是否影响公共接口，须在 B 前列明决定，不能把新限额隐塞回旧 decoded 字段。

计数步骤建议：先在唯一 typed record 上复用语义 atom visitor，checked 计算每个定义的 cost
摘要；无环的声明引用用已验证拓扑传播 `(bytes, referenceVisits, maxKey, scratch)`。
每次引用按 cost 累加，重复列表用 checked 乘法；不在计数阶段先把恶意共享大定义展开一万次。
计数本身的缓存和遍历必须先受物理输入/accepted scratch 控制；遇环、溢出或预算不足稳定拒绝。
不能用先完整展开再计数的实现承担不可信输入的防放大门禁。

计数通过后才按完全相同语义 visitor 向现有增量 SHA 输出。prefix、projection、定义展开也需流式
sink，不能只换最外层：hex 按固定 bounded chunk 输出，完整 key 按长度计费而不构造大字符串。
实现内部 chunk 大小不改变 framing 或哈希。小规模 semanticPreimage 字节导出继续受限并保留 golden。
Reader、Writer、Header 验证和 metadata 共用一套 count + digest；任意入口都不能绕过会计。

## 放大模型与必须拒绝的负例

完整 E 原始结构 bytes：`4*6 + sum(E 的五个文本 bytes) + sum(path node bytes + 12)`。
其中六个 u32 分帧包括四个前缀文本、path count、localId；单 E projection 为
`6 + 2*(1 + Ebytes)`，claim projection 为 `6 + 2*(1 + 4 + namespaceBytes + Ebytes)`。
长 path/token 既增加唯一字典 bytes，又增加每次展开的 projection/hash 成本，不能只检查 STR0 行数。

例如共享的 1 MiB 合法 Pattern 被 40k 个 Requirement 引用，单独该定义的 hash 展开就约 39.06 GiB；
这是放大模型，不是本轮新采样，不证明该组合满足所有字段/物理限制。通用计数器应从合法引用
和已计定义成本推导其需求，在 digest 前因已接受预算拒绝，不能按 wire 去重后约 1 MiB 计费。

阶段 B 必须补：每维恰等于界/超一单位、0/收紧/请求放宽、checked u64 加乘溢出、长 namespace/
E/path、共享定义重复展开、scratch cache 耗尽、错误/循环链接、混写冗余物理引用、孤儿/多 owner
claim、observe 携带占用字段、K4 冲突与所有入口一致拒绝；失败保留旧 active 和有效文件。

## 交回与恢复

阶段 A 交回后先接受会计 envelope 和字段差异，再进入 B 的权威合同/codec/入口实现。
如果完整提议 file 超原 16 MiB、身份/行为不等或需要其他语义/API 改动，立即交回设计，不换轻
fixture、不扩大预算、不顺手优化 readObject。之后 C 才有完整原样 40k 功能验收；正式性能登记另行处理。

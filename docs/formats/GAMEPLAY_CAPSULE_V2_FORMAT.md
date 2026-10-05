# Gameplay Capsule v2 candidate format

状态：candidate；2026-10-04 按 owner 的文档设计授权闭合 S7A-3 首次消费合同；
不是生产 wire、Reader/Writer 实现证据或 S7A-3 整批关闭证明。

更新日期：2026-10-05

依据：[Gameplay V2 Spec](GAMEPLAY_V2_SPEC.md) §3.8–§3.12、§5、§8，
[Foundation Packed](PACKED_CHART_FORMAT.md) 与
[决策登记](../stage_reports/stages/stage-07/2026-10-03-s7a-3-decision-register.md)。
本文唯一拥有 Capsule 的物理字段；时间消费与仲裁语义仍由 Gameplay V2 Spec 拥有。

2026-10-05 新增 §12 的 execution candidate revision 3；§§1–11 的 revision 2 合同与
已有 golden 不被改写。当前实现状态只见 [CURRENT_STATUS](../CURRENT_STATUS.md)。


## 1. Profile 与兼容

采用 **G2-S2 + G2-T4 + R2-K4 + P1-W1 + E1**。Capsule 是 Chart v5 Packed envelope
内的 typed Gameplay graph，不是 CXT AST、JSON blob、运行时模板或 Snapshot。

| 项 | revision 2 合同 |
| --- | --- |
| Header | Foundation 96 bytes、magic `CXPK5`、`packedVersion=1`、`flags=1`、`candidateRevision=2` |
| Directory | Foundation 32 bytes；使用 `offset/encodedBytes/decodedBytes/recordCount` 原字段名 |
| 必需基础 sections | `META/TIME/STR0/REF0/IDN0/ARCH/ENT0/REQ0/CNS0` |
| 必需 Gameplay sections | `GPH0/GPR0/GPD0/GRC0`，加共享的 `REF0`，整体不可缺席 |
| 静态 streams | `TRN0/REN0/CAM0` 按 Foundation component presence 规则出现 |
| Legacy 要求 | `REQ0` 为两个 Rational descriptor `00 00`、目录 count=0；`CNS0` 为零 bytes、count=0 |
| Header requirementCount | 等于 `GPR0` requirement 行数；不得再与空 `REQ0` 比较 |
| ENT0 bit 2 | 当且仅当该实体拥有至少一条 `GPR0`；不是 Foundation tap profile |
| eventCount | 此 profile 为 0；`BEH0/BHD0/ANM0/FXS0` 仍不接受 |
| semantic flags/codec | 目录 flags=0、codec=0；不能用 inspection 标志隐藏 Gameplay |
| inspection | `DBG0 flags=1` 可省略；未知 inspection 经 bounds/CRC 后可忽略，绝不参与 prepare |

其余 META/TIME、实体 identity、parent、Archetype、静态组件布局保持 Foundation。
REQ0/CNS0 不承载 V2，也不得混存两代要求；本节变化仅适用于显式 revision 2 路径。
revision 1 Reader 必须早期拒绝 revision 2；V2 Reader 不把 revision 1 猜作 V2。
本设计不切换默认 Writer、SDK 版本或 Playback/CXC 入口，集成仍属 S7A-7。

Writer 将目录按四个 ASCII code bytes 升序输出；Reader 可接受其他物理 section 次序，
但 sections 必须按目录连续、无 gap/overlap/trailing/重复。物理次序不决定解码依赖次序。
CRC 使用 Foundation CRC-32/ISO-HDLC；七项既有硬预算完整复用，不新增或放宽限额。
Header/directory 预算预检先于 payload 访问、reserve 与 CRC，具体错误优先级沿 Foundation §9。
Decoded bytes 是 section bytes 的 checked sum，不冒充 graph 堆占用。

## 2. Atom 与规范顺序

本文字段表从左到右为实际 wire 顺序；括号内为严格依次串接的 nested fields。
不使用 struct padding、任意字段 tag、JSON、opaque operand 或未登记 extension。

| Atom | 编码与验证 |
| --- | --- |
| `U` / `V` | 最短 unsigned LEB128，分别 u32/u64；拒绝超宽、非最短、截断 |
| `Z` | signed i64 ZigZag + 最短 LEB128，含 Tick/TickSpan/priority/geometry |
| `B` | u8，只有 0/1 |
| `S` | STR0 的 0-based `U` 索引；空字符串是真实字典项，不是 absent |
| `Rk` | REF0 的 0-based `U` 索引，要求 kind=k |
| `Ix(T)` | 表 T 的 0-based `U` 索引；不能跨表使用 |
| `O(X)` | presence:B；0 无后续 bytes，1 紧随 X；不使用 numeric sentinel |
| `L(X)` | count:U，紧随 count 个 X；每项不额外加 length |
| `Q` | numerator:Z、denominator:V；正且 ≤ INT64_MAX、约分，零必须 0/1 |
| `M(X)` | tag:u8；0=absent、1=pending、2=measured + X；不是零值替代缺席 |
| `D` | 稳定声明 ID `(sourceDocumentId:S, declarationOrdinal:U)` |
| `E` | `(chartEntryId:S, invocationId:S, moduleId:S, exportId:S, emissionPath:L(nodeId:S, repeatIndex:V), requirementLocalId:S)` |
| `C` | `(capabilityRef:R13, revision:O(S))`；仅引用 §4 的 capability 子域 |
| `N` | `(features:L(R7), capabilities:L(C))` |
| `J` | Phase tag:u8：1=tap、2=head、3=body、4=tail；不是 PhasePriority |
| `A` | `(phase:J, declarationOrdinal:U)`；精确指向本 Requirement 声明的 phase |
| `I` | `(start:Z, end:Z)`，半开且 start<end |

所有列表 count 必须受剩余 bytes 和相应 accepted limit 约束，先 checked arithmetic 再分配。
Measured(0)、INT64_MAX、UINT64_MAX 都是普通测量值。合法发布物的执行参数必须 measured；
Reader 能识别 absent/pending 是为了准确拒绝，而不是允许绕过准入门禁。

集合按其解析后的语义 key 严格升序、禁止重复；ordered list 不排序。下列为 ordered list：
E 的 emissionPath、Pattern 的 operands、gradeTokens、solver objective/tieBreak。
stable declaration 按 `(sourceDocumentId UTF-8 bytes, declarationOrdinal)`；E 按各字符串
UTF-8 bytes、路径各 nodeId/repeatIndex、路径长度和 localId 作字典序比较（不是字典索引序）。
所有比较用 unsigned UTF-8 bytes、数学有符号或无符号数值，不用 locale。
表中未单独规定的 token 集合按解析后的 token bytes 排序。

## 3. Gameplay section framing

**只有 `GPH0/GPR0/GPD0/GRC0`** 使用以下 16-byte little-endian header：

```text
schemaRevision:u16 = 1
rowCodec:u8 = 1
flags:u8 = 0
rowCount:u32
payloadBytes:u64
```

payloadBytes 必须恰等于 directory decodedBytes−16；rowCount 必须等于 directory
recordCount。任何未知 revision/codec/flags、计数差异或 trailing bytes 都拒绝。
REF0 保持 Foundation raw rows，**没有这个表头**。

| Section | payload 次序 | rowCount |
| --- | --- | --- |
| GPH0 | 一行 global record（§5） | 1，即使 requirementCount=0 |
| GPR0 | Requirement rows（§6），由 rowCount 定界，无重复 count | requirementCount |
| GPD0 | patterns:L(P)、measures:L(W)、domains:L(H)、mergedDeclarations:L(K)（§7） | 四表 count 的 checked sum |
| GRC0 | resources:L(T)、relations:L(F)、solverProfiles:L(Y)、factBindings:L(S)、claims:L(X)（§8） | 五表 count 的 checked sum |

各子表固定分组次序，无跨组任意 tag。GPH0 counts 与实际子表、目录/Header 全部交叉核对。
共享定义去重只能合并**全部字段相等**的定义，不能仅比较匹配语言或 token 名称。
内联匿名 Pattern/Measure 没有虚构的用户 ID；其 dictionary 行按下文语义结构序排列。
子表 ordinal 只是压缩引用，不是 declaration identity、slot identity 或 competition rank。

## 4. REF0 注册与投影引用

REF0 每行仍是 `kind:u8 + stringIndex:U`，count 来自 Header/directory。
按 `(kind, resolved string bytes)` 严格升序且唯一；Foundation kind 1–7 原义不变。

| Kind | 名称 | 本 profile 的用途 |
| ---: | --- | --- |
| 1–7 | Foundation 原类别 | assets、behavior、animation、domain、action、effect、extension/capability |
| 8 | pattern | 命名 Pattern ID |
| 9 | measure | 命名 Measure ID |
| 10 | resource | graph 内 resource ID |
| 11 | claim-key | 显式 claim namespace 与展开实例路径的结构 key |
| 12 | emission | E 六元组及 emissionPath 的结构 key |
| 13 | gameplay-capability | Gameplay capability 与判定投影的固定子域引用，见下表 |

14–255 拒绝，不把内部 `ReferenceKind` enum 数值复制成 wire kind。
kind 13 的历史名称不意味着其所有子域都是“能力声明”；它是此 candidate 的
**typed Gameplay reference namespace**，投影记录不能误送入 capability enabled/unknown 检查。
这样保留已选 8–13 编号，又不滥用 Foundation kind 7 为任意 profile 字符串。

kind 11–13 的 string 是 `cxgp2:` + **小写 hex**，hex bytes 为以下结构编码：
`subdomain:u8 + payload`。字符串、列表、数值分别用 §9 的 fixed-width hash atom 编码，
不得 delimiter joining、hash 截短或临时 row ordinal。Reader 解析、核对子域并重算此文本。

| Kind / subdomain | payload | 对应逻辑引用 |
| --- | --- | --- |
| 11 / 0 | explicitNamespace:text + E | claimKey |
| 12 / 0 | E | requirement / emission / requirement identity 六元组 |
| 13 / 0 | capabilityId:text | CapabilityRef；revision 在 C 中携带 |
| 13 / 1 | E | requirementIdentityProjection，定位 typed Requirement，不携带自指 hash |
| 13 / 2 | profileId:text | timebaseProjection，定位 GPH0 timebase |
| 13 / 3 | timebaseProfileId:text | latePolicyProjection，定位同 GPH0 late policy |
| 13 / 4 | normalizationProfileToken:text | normalizationProfileProjection，声明上下文 |
| 13 / 5 | solverId:text + revision:text | solverProfileProjection |
| 13 / 6 | coordinatorPolicyToken:text | coordinationPolicyProjection |
| 13 / 7 | factBindingId:text | factBindingExistence |
| 13 / 8 | rulesetRef:text | ruleset 判定引用 |

kind 8/9/10 和 Foundation 4/5 使用原稳定 ID 文本。匿名 Pattern/Measure 的引用归属
由 requirement projection 指向 dictionary 结构，不捏造命名定义。判定所需的 timebase、
late policy、normalization、solver/coordinator、FactBinding 存在性必须各有上述 REF0 引用，
同时拥有对应完整 typed fields；只存引用 key 不能替代实体记录。
compiledSemanticIdentity 是这些 typed 投影的 digest，见 §9；不在 REF0 内保存自引用摘要。

Gameplay 判定 closure 按 Spec §3.8.6 派生、与声明集合逐项比对。Foundation 静态
Presentation 所需 asset 等仍可存在共享 REF0 中，但**不属于 Gameplay judgement subset**；
纯表现 Gameplay 引用仍只进 presentation closure。不要通过删除基础 asset rows 实现隔离。
同一 Gameplay 逻辑引用不得跨归属登记，sourceMap 不进入 REF0。
unused 但规范要求保留的定义/引用保留，不按“本次没有命中”裁剪。

## 5. GPH0：global record

下表各行依次串接，global counts 用 `U`。GPH0 恢复的数据必须由 decoded owner 持有；
不能让 CanonicalGameplayGraph 的 borrowed timebase/latePolicy 指向已释放的临时 buffer。

| 次序 | 字段与类型 |
| ---: | --- |
| 1 | gameplayVersion:U=2, graphRevision:V, rulesetRef:S, rulesetProjection:R13, normalizationProfileToken:S, normalizationProjection:R13, coordinatorPolicy:S, coordinatorProjection:R13 |
| 2 | graceResolverRevision:S, effectiveGracePolicy:S；分别声明解析算法与 T4 消费算法，不是来源枚举 |
| 3 | timebaseProjection:R13, timebase:(profileId:S, unitToken:S, tickScale:Q, originBeat:Q, initialTempo:O(Q), tempoSections:L(startBeat:Q,durationPerBeat:Q), stopSections:L(startBeat:Q,endBeat:Q,duration:Q)) |
| 4 | latePolicyProjection:R13, latePolicy:(mode:O(u8), finalizationWatermark:M(Z), maxQueueHop:M(Z), windowCloseThreshold:M(Z), windowOpenThreshold:M(Z)) |
| 5 | declaredFeatures:L(R7), declaredCapabilities:L(C), closureContributions:N, derivedFeatures:L(R7), derivedCapabilities:L(C), resourceClosure:L(R10) |
| 6 | sourceClosure:(sourceDocumentIds:L(S), compilerProfileToken:S)；content-only，不是诊断文件路径 |
| 7 | counts: requirement, pattern, measure, domain, mergedDeclaration, resource, relation, solverProfile, factBinding, claim, reference（每项 U） |

late mode 仅 1=rejectLate、2=queueNextTick；absence 可解码但执行 gate 拒绝。Timebase 及
late-policy 数值、声明次序、stop 函数和一致性沿 Spec §3.7，不靠 Foundation TIME 的 f64
重新生成 judgement Tick。TIME 是基础 chart timing，GPH0 是精确判定时基；二者不能暗中互代。
本 profile 显式接受 `graceResolverRevision=grace.resolver.s2.v1` 与
`effectiveGracePolicy=local.close.t4.v1`；每条 GPR0 的 localClosePolicyToken 必须等于后者。
这是已选解析/消费算法的版本 token，不是 capability、默认值或可执行脚本；未知 token 拒绝。
META requiredFeatures 与 derivedFeatures 必须一致；Capability revision absence 与空 token
不混同。Derived closure 不能被当作可信缓存，Reader 从 records 重新派生。
normalization token 是 chart 编译的匹配约束，实际映射/calibration 仍由 session 声明和 identity
提供，文件不能注入宿主 mapping 或补全 session identity。

## 6. GPR0：Requirement record

按 E 严格升序，重复 E 或映射到缺失实体拒绝。entityOrdinal 使用 `U` 绝对索引，
不使用旧稿的模糊 localId/delta 代理六元组。

| 次序 | 字段与类型 |
| ---: | --- |
| 1 | entityOrdinal:Ix(ENT0), stableId:D, identity:E, emissionRef:R12, identityProjection:R13 |
| 2 | requiredActions:L(R5), domainBinding:S, judgementDomain:Ix(domains) |
| 3 | phases:L(A), requiresReleaseTailSemantics:B |
| 4 | pattern:Ix(patterns), maxArmElements:M(V), maxDeadlineElements:M(V), patternArmRefs:L(S) |
| 5 | measure:Ix(measures), resourceClaims:L(Ix(claims)) |
| 6 | preparedGrace:Z, localClosePolicyToken:S, solverProfile:Ix(solverProfiles), factBindingRefs:L(Ix(factBindings)), requiredRefs:N |
| 7 | timing:O(end:Z, successWindows:L(phase:A,interval:I), body:O(I)) |
| 8 | graceProvenance:(sourceKind:u8, inheritedFromDeclarationId:O(S), allowChartGrace:B) |

sourceKind 为 1=explicit、2=single-hop inherited、3=frozen default；只有 2 有继承 ID。
第 8 行恢复 content/diagnostic provenance，不进入 judgement projection。原精确作者 duration、
路径和位置可保留于 Source Project/DBG0，不是 executable graph 所需输入；Packed 不重跑 resolver。
preparedGrace 已非负量化、冻结；Reader 按声明 policy/range/identity 规则复核，不二次舍入。
其中 range 只指 canonical signed i64 非负域及 T4 timing 一致性；原始 duration 与作者
minimumCanonical/maximumCanonical 已在 source resolver 检查，不在本文 wire 中，Reader
不能声称重新验证这些原始来源条件。执行上下文若声明更紧的 accepted bound，仍须单独检查。
同最终 grace、同有效消费 policy 而不同来源时，judgement projection 相同。

Phase 列表按 `(phase tag,declarationOrdinal)` 排序且唯一；成功窗口按 phase、start、end 排序，
同 phase 不重叠，必须引用已声明 phase。body/tail/end 合法性沿 Spec §3.8.11。
无 timing 的图可以是离线声明图，但不能发布 T4 可执行 Requirement，不能由 Reader 添加零 end。
本 profile 中每个 Requirement 最多一个 resource claim；claim 行必须回指同一 Requirement。
unsupportedForms 不作为合法 wire 字段携带：上游必须稳定拒绝，Writer 不编码禁用表示。
测量缺口、缺席与 pending 不会被编码成“已证在状态预算内”；包含性 gate 仍独立执行。

## 7. GPD0：typed definitions

### 7.1 Pattern P

```text
patternId:O(R8)
matchPolicy:u8 = 1                     # leftmost-first
requiredRefs:N
nodes:L(opcode:u8, childCount:U, opcodeOperands)
```

匿名 ID 的 absence 对应 typed patternId 的空值；有名定义必须是非空 ID。
nodes 为完整 rooted tree 的 preorder：每个节点紧随其 childCount 个子树，无共享 parent、
回边或游离节点。迭代 decoder 用 checked pending-child 计数验证恰好一棵树，
不按恶意深度递归调用。root 包括在 count，非空 Pattern 不能缺 root。

| Opcode | 原语 | childCount | opcodeOperands（固定次序） |
| ---: | --- | --- | --- |
| 1 | atom | 0 | atomRef:S |
| 2 | sequence | ≥1 | 无；保持 child 次序 |
| 3 | choice | ≥1 | 无；保持 child 次序 |
| 4 | boundedRepeat | 1 | minimum:V, maximum:V，minimum≤maximum |
| 5 | skip | 0 | 无 |
| 6 | instant | 0 | 无 |
| 7 | complement | 1 | 无 |

这些号只是 Capsule opcode，不改变公共 enum 值。原语语言、空轨迹、份数序、并集与不动点
沿 Spec §3.8.4/§3.8.8。wire 保存有限 declarative Pattern tree，不保存运行期计数器；
Reader 交给同一 prepare 编译器求有限程序、包含性与测量，不能只信 Writer 的结论。
repeat 极大值本身不新增拒绝；测量缺口和可证明超 accepted limit 沿 Spec §8.2。
skip/instant 语言相同仍不同字段身份；choice operand 顺序有意义。
空 operand **列表**拒绝；能够匹配 ε 的合法 operand（如 skip）不是空列表，不得因此拒绝 repeat。
Pattern 行按 `(optional ID, 完整结构字段)` 字典序：absent 在前，重复结构且 ID 相同则去重；
同名但结构不同拒绝。

### 7.2 Measure W

```text
measureId:O(R9)
components:L(phase:J, categoryToken:S, declaredGradeTokens:L(S))
requiredRefs:N
```

components 按 `(phase,categoryToken bytes)` 升序且唯一。空 category 与显式派生 category
不互相替代；grade token ordered list 原样携带、无默认值、不求值、不聚合。
Measure 行按 `(optional ID,完整字段)` 排序；同名不同定义拒绝。
measureId 只来自已解析 merged-declaration 命名空间；匿名定义为 absent。它不要求给现有
`MeasureSpecDeclaration` 增加公共 ID 字段，也不把匿名 dictionary ordinal 当成作者 ID。

### 7.3 Domain H

```text
domainId:R4
coordinateSystemToken:S
axes:L(axisToken:S, minimum:Z, maximum:Z)
frame:u8 = 1                          # static
dynamicProviderToken:S               # static 时必须为空
requiredRefs:N
```

domains 按 domain ID bytes，axes 按 axisToken bytes；axis 唯一且 min≤max。
动态 frame、宿主视口/Presentation transform 推导不在 accepted subset，稳定拒绝。
整数 geometry 不转成 f64，不依赖屏幕像素。

### 7.4 Merged declaration K

```text
stableId:D
kind:u8                              # 1=requirement,2=pattern,3=measure,4=resource,5=domain,6=solver
name:S
resolvedReferences:L(D)
requiredRefs:N
```

按 D 升序，references 是 D 集合；name 冲突拒绝而非覆盖。同 sourceDocumentId 与 ordinal
须有唯一 record，引用须定位对应 typed definition/Requirement，不能靠行号合并。
跨 invocation 的显式引用已经解析完毕，Packed 不再进行名称搜索。

## 8. GRC0：resources 与 immutable plan

| 表 | 每行字段（依次编码） | 行排序 |
| --- | --- | --- |
| Resource T | resourceId:R10, declaredCapacity:V=1, slotToken:S, decisionPolicyRef:S, terminalAfterTermination:B, declaredGapGrace:Z=0, requiredRefs:N | resourceId bytes |
| Relation F | kind:u8=1（exclusive）, resource:Ix(resources), members:L(D), policy:S, declaredCapacity:V=1, requiredRefs:N | resource ID, kind, members, policy；重复完整行拒绝 |
| Solver Y | solverId:S, revision:S, algorithmToken:S, objective:L(S), tieBreak:L(S), rejectIfNonUnique:B, requiredRefs:N, projectionRef:R13 | solverId bytes,revision bytes |
| FactBinding | factBindingId:S, existenceRef:R13 | factBinding ID bytes |
| Claim X | requirement:Ix(GPR0), resource:Ix(resources), intent:u8, policyToken:S, explicitNamespace:O(S), claimKey:O(R11), competition:O(priority:Z,tieRank:V), slot:O(U), graceOverride:u8=0 | resource ID, Requirement E |

intent 明确为 1=observe、2=consume、3=claim，**不折叠后两者**。observe 的 namespace、
claimKey、competition、slot 全部 absent，不是假零；它不拥有 lease。consume/claim 全部 present，
namespace 非空，slot 必须为 0，claimKey 必须等于 namespace+E 的结构 key。
capacity=1 的所有候选引用同一 `(resourceId,slotToken,0)`；slot 不是第几个竞争者。
slotToken 必须显式非空；owner/lease/contact 不写入文件，由 S7A-4 引擎按规范阶段分配。
graceOverride 只有 none=0；sticky/observe override 和 unsupported forms 稳定拒绝，不做 ignore。

claim 的 K4 顺序为同资源 `(priority:i64,tieRank:u64)` 升序、唯一，不能按 GRC0 物理行排序选胜者。
observe 不参加这个唯一性集合。关系成员必须存在且与资源关联一致，7A 禁用的 relation 不编码。
FactBinding 只有判定侧存在性，target、效果、材质、绑定详情均不在此表。
Solver 字段是显式声明，不补默认 objective/tieBreak；支持与证明规则见 Spec §3.8.12。
proof/certificate 不持久化，不为它杜撰 codec；Reader prepare 重新证明。

## 9. Semantic identity：结构 framing，不是 wire dump

revision 2 Header 的 semanticIdentity 是**完整 compiled chart 的结构 digest**，不是
PreparedJudgementIdentity 或 internal CanonicalIdentityBytes。算法 revision 为
`cuexis.chart.semantic.v5.gameplay.1`，不包含 candidateRevision；以后仅改物理压缩/布局而
语义不变时可复用。首次使用新语义 preimage 与 revision 1 明确 domain-separated。

Hash atom：u8 原字节；U→u32 LE、V→u64 LE、Z→i64 二进制补码 LE；
text→u32 LE UTF-8 byte length+bytes；list→u32 LE count+items；
option/measured/enum 按本表 tag + 内容；Q→i64 numerator+u64 denominator；
STR0 索引替换为 text、REF0 索引替换为 `(kind:u8,text)`；
table index 替换为所指 record 的**完整结构字段**，不是 ordinal。
匿名 definition 按结构展平；所有展开字节数量先做 sizing/budget 检查，不在 hash 中嵌自身 digest。

预映像依次串接：

1. ASCII `cuexis.chart.semantic.v5.gameplay.1` + NUL + u16 LE(5)。
2. Foundation §10.1 全局步骤 2–9（META、TIME、基础资源闭包），原字段次序/浮点 bits不变。
3. entityCount:u32；按 Foundation identity-byte 序写实体步骤 1–3，bit 2 presence 保留；
   不写 Foundation Requirement 步骤 4–7。基础 definition counts 为三个 u32(0)。
4. GPH0 完整字段（§5 次序）中去掉 graceResolverRevision、sourceClosure、counts；
   graphRevision 保留，沿 Spec §5.2 属判定语义修订。
   projection references 写结构定位 key，不是 digest；declared/derived closure 均保留并先验证相等关系。
5. GPR0 count:u32；按 E 序写每行 §6 步骤 1–7；entityOrdinal 换成实体 identity bytes 的
   u32 length+bytes，stableId/identity 保留；不写 graceProvenance。
6. GPD0 四表及 GRC0 五表，按固定组次序写 count+完整字段；indices 按上文 dereference。
   claim requirement 索引只展开 E（不是递归展开 Requirement）；resource 索引只展开 resourceId；
   relation member 为 D；这些指定 key 引用断开 Requirement↔Claim 环。

上文所有其余 Ix 引用无环，若出现环则拒绝。语义 digest 包含未使用但须保留的定义、
capacities、phase ordinals、timing、intent、competition keys、域几何、solver 有序列表与终止策略。
不包含字典/row ordinal、inspection、物理 section order、CRC、source paths、grace source kind，
也不散列宿主指针或 struct bytes。

sourceClosure、graceProvenance、graceResolverRevision 保留在 wire 和 content-artifact
identity（exact artifact SHA-256）中；其是否进入更高层 content projection 按 Spec §5。
resolver revision 表示上游来源解析，不影响已经冻结的执行值；effectiveGracePolicy 则必须进入语义摘要。
在 owner 授权的 E1 实现验收前没有本 revision 的 verified hash golden，本文不编造摘要。
Header digest 校验成功仍不能替代逐字段 semantic diff、closure validation 和 prepare proof。
完整 prepared identity 还必须绑定 engine、ruleset、session，包括实际 normalization/calibration；
不能拿此 Header digest 宣称 replay interchangeable。

## 10. Reader → 同一 prepare → 原子发布

物理 decode 与 execution publication 是两层结果。Reader 拥有 decoded records，可返回离线
candidate；只有完成下列全部执行门禁才能返回 prepared publication：

```text
Foundation size/header/revision/budget/directory/CRC checks
  -> STR0/REF0/IDN0 and Foundation static typed chart
  -> GPH0/GPD0/GPR0/GRC0 bounded decode and cross-table validation
  -> actual counts / ordering / graph closure / subset validation
  -> recompute structural semanticIdentity
  -> shared typed prepare: Pattern compile + containment + T4 + K4 + local proof
  -> measured budget evidence recorded separately from incomplete dimensions
  -> recompute prepared identity using explicit execution context
  -> one atomic prepared publication (no partial session / no active replacement on failure)
```

未知/disabled capability、无效 refs、缺 section、counts 错、illegal opcodes、overflow、
closure/hash 不符和语义矛盾都不能降为默认或警告成功。
资源 plan、derived closure、containment/proof 不因来自 Packed 而可信。
Writer 只接受完成同一语义验证的 canonical graph；先 sizing、再 envelope、后输出，
file bridge 使用临时产物+原子替换，memory bridge 返回 owning bytes。

拒绝诊断沿 Foundation `packed.*` 与 V2 Spec §9 既有族/类别；记录
`section/table/record/field` 路径。本轮**不新增公共诊断码串或类别**。
需要新具名公共码时，实现批次先逐条登记集中码表并通过校验，不能以本文的描述短语代替登记。
state-budget 测量缺口仍是独立 **INCOMPLETE GATE**；没有 accepted threshold 时不杜撰值，
缺口本身不是内容超限，也不是预算通过。Containment gateIncomplete 则原子 prepare 拒绝。

## 11. E1 必交证据

以下是验收要求，**不是本轮实测结果**：

| 证据组 | 最小覆盖 |
| --- | --- |
| typed round-trip | 空 requirement 图、完整 timebase/late、Tap、Hold head/body/tail、匿名/命名定义、多 action、domain axes、phase ordinals、Measure category/grade、merged refs、solver、closure、provenance |
| S2 | 三种合法来源、单层引用、来源不同最终值同 identity、半整数 ties-even、负/越界/溢出、missing/default/chain/override/conflicting source |
| T4 | bodyEnd/end/D 的前一 tick/相等/后一 tick、grace=0、窗口不裁切、早释放失败、tail 完成/错过、资源即时 free/terminal、已发 Fact 不重复 |
| K4 | 显式实例 rank、affine mixed-radix 展开、不同 priority/相同 priority、key/rank collision、算术溢出、未分配 rank、observe absence、consume/claim 保真、slot=0、held 不抢占 |
| negative Reader | revision/section/kind/subdomain/opcode/flags/codec/count、非法 varint/UTF-8/CRC、截断/trailing、O/M tag、kind错配、循环/dangling、同名冲突、capacity/gap/handoff、假 derived closure/hash/proof、containment incomplete |
| determinism | input array/parameter/reference permutation；inline/CXT field-by-field diff；不改 choice/grade/objective ordered list；skip 与 instant 不合并 |
| file/memory + golden | Writer canonical bytes、Reader↔Writer、GCC/clang/MSVC 同输入 byte golden 与 hash preimage；revision 1 Reader 对 revision 2 早拒绝；失败保持旧 artifact/active |
| publication | 所有 negative 在 publish 前失败；actual count 不信 Header；资源/identity/closed-world proof 重建；状态预算未测项如实报告 |

S7A-3 的 E1 不缩减；S7A-7 承接 Playback/Player/CXC 的入口、manifest 与生命周期集成。
Snapshot/Replay/FactId/CommitId codec、runtime resource state encoding、S7A-9 新数值上限、
7B+ capabilities 均不由此文件冻结。

## 12. Execution candidate revision 3

本节为 2026-10-05 的选定补充，运行含义由
[execution profile](gameplay-v2-execution-profile.md) 拥有，不表示实现已存在。
candidateRevision=3、graphRevision 显式为数值 `2`（既有 V/u64，不是字符串 token）；
chartVersion=5、gameplayVersion=2、packedVersion=1 保持。revision3 仅完整 execution profile，
不接受 legacy requirements、runtime state/proof/AST 或缺字段的静态图。
所有基础 section、flags、directory、varint、CRC、bounds 和七项物理限额沿 §§1–11。

增量字段恰为以下内容；按行指定的位置串接，使用本文已有 S/Z/V/B/L/O/J atoms。

| 位置 | 新增字段与物理顺序 |
| --- | --- |
| GPH0，§5 第 7 行 counts 之后 | `executionProfile:S`，精确 token `gameplay.execution.t4-k4.v1` |
| GPR0，每条 §6 第 8 行 graceProvenance 之后 | `phaseTargets:L(phase:J,chartTick:Z)` |
| 紧接 phaseTargets | `atomBindings:L(atomRef:S,domainToken:S,sourceClass:S,channelToken:S,action:u8,amountRange:O(minimum:Z,maximum:Z),tailOnly:B)` |
| 紧接 atomBindings | `independentCompetition:O(priority:Z,tieRank:V)` |

phaseTargets 按 J 升序且每个声明 phase 恰一行；J 是已有 phase kind tag，不追加 declarationOrdinal。
atomBindings 按 atomRef bytes 升序、同一 atomRef 不重复；action 明确 1=press、2=release、3=update，
未知拒绝。amountRange 非空闭区间；tailOnly 为显式 B，不是 hint。新增字符串全部进入 STR0，
不借 REF0 原有 kind 给 source/channel 编造新资源引用；atomRef 必须定位同实例 Pattern atom
或已声明 tailOnly binding。不存在新公共 capability 自动补全。
independentCompetition 与真实 resourceClaims 互斥；无 claim 的非观察实例必须 present，
读取后放入 prepare 的虚拟独立仲裁组，不能在文件添加名为 `@independent` 的 Resource row。
资源引用的存在性、target 合法性、mapping 兼容与执行 gate 仍由 Reader→prepare 完整复核。

结构 preimage domain 改为 `cuexis.chart.semantic.v5.gameplay.2`，后跟 NUL+u16 LE(5)。
§9 全部步骤沿用，但 GPH0 添加 executionProfile，GPR0 每行添加本节三个完整结构字段，
位置与 wire 增量相同；S 展开为 text，不 hash dictionary indices。每个 phase target 用 u8 J+i64 LE；
binding 用 text×4+u8 action+option tag+i64 range×2+B；独立 pair 用 option+ i64+u64。
所有新增字段进入 chart/compiled judgement 投影，sourceMap 和物理顺序仍不进入。
旧 semanticIdentity/golden 不重解释，新 revision3 golden 单独固定长度、bytes 与 preimage digest。

revision1/2 Reader 对 3 在 header gate 早拒绝；revision3 dispatcher 可以显式分派旧 Reader
用于离线读取，但不能把旧图补零后交 execution gate。执行旧图拒绝 profile_incomplete；
不自动迁移 phase targets、bindings 或 pair。Writer 显式选 revision3，不切换已有默认模式。
向低版本 Writer 交新增 profile 数据必须拒绝，不能丢字段写 revision2。

E1 增加 target/binding/pair typed round-trip、非法/缺失/重复 binding、unknown action、
旧/新 revision 互拒、target/hash mutation 与全新 byte golden。
原 revision2 E1/golden 保留；不能删除旧测试让 revision3 通过。


### 12.1 Revision 3 增量的严格验证

不允许重复 phase kind，即使 declarationOrdinal 不同；phaseTargets 引用 phase kind 而非 ordinal，
且 phase declarationOrdinal 在该 Requirement 内唯一。完整 phase shape/Release flag/body 单窗口
由 execution Spec §2.1 校验。新增字段必须在 Writer/Reader/所有 canonical identity 投影保真。
phase/body/tail validation 不使用 admitsSuccess 的无 phase union helper 来代替 runtime gate。
旧合成 E/path fixtures 不作 CXT 真实身份的证据；实际 CXT marker 映射归 author profile §3。
normalization/coordinator 约束在 revision3 的 owning graph/AssemblyRequest 必须保留，
不能只存在 CapsuleProfiles 中而被 identity generator 丢弃；其既有 GPH0 wire 位置不改。
Reader 的 DecodeContext/coordinator/identity 声明与保存的约束逐字段对账。

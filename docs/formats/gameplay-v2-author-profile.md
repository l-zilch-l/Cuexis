# Gameplay V2 candidate author profile

状态：candidate；S7A-3 两路 authoring 的选定输入合同

更新日期：2026-10-05

实现状态：工具私有 owning author adapter、payload/inline candidate Schema 与 revision 3 已实现，聚焦验证中；整批验收未完成，不是生产 Schema

本文件仅拥有源表达与 lowering，运行语义见
[执行 Spec](gameplay-v2-execution-profile.md)，Packed fields 见
[Capsule](GAMEPLAY_CAPSULE_V2_FORMAT.md)。不把研究稿或 Foundation lanes4 requirement
自动解释成 Gameplay v2。所有 authoring adapter 都是离线入口，不是 Playback entry。

## 1. 显式源入口

选定 extension ID=`cuexis.gameplay.v2`，author profile=`gameplay.author.t4-k4.v1`。
仅显式 candidate author 工具入口接受；默认生产 Chart/CXT Reader 继续拒绝此 required extension。
候选组合 Reader **先**验证原始 root/version/requiredExtensions，再分派基础字段与 Gameplay payload。
现有 CxtV2Loader 要求 requiredExtensions/扩展内容为空，不能原样调用后期望它接受新 extension。
json_support 内的组合 Reader 只消费本 profile 的已注册字段，产出 owning Gameplay DTO 与
Foundation root view；基础 Reader/expander 复用该 view。未知 required extension/core key 仍拒绝，
不能先删任意未知字段再交基础 Reader。Source identity/bytes budget 使用原始 bytes，
不把重写后的基础 view 当成原始 source；未修改的 v1/v2 Reader 和默认 Playback 仍不接受。
JSON DOM 保持在 json_support，typed adapter 不向 judgement 传 DOM；不新增 chart→judgement 依赖。

| 路径 | Payload 的精确位置 | lower 职责 |
| --- | --- | --- |
| Chart v5 inline | root `gameplay` object，显式 `gameplay.version=2`；requiredExtensions 列 cuexis.gameplay.v2 | owning 全实例 records 与显式 E/rank |
| CXT v2 emission | module `extensions["cuexis.gameplay.v2"]`，payload version=2；requiredExtensions 列该 ID | 真实 prototype/pattern export 经 Foundation expansion 生成 identity，再挂接全实例 records |

两条路径复用同一 payload schema；inline 同名 extensions payload 禁止，不能并存两个权威。
root 的 Chart version=5，CXT module version=2；GameplaySourceDocument.chartVersion 从被调用的
Chart context 显式 5 得到，gameplayVersion 从 payload.version=2 得到，不能把 CXT.version 当 Chart 版本。
CXT moduleKind 支持 prototype/pattern；animation 拒绝。CXT 与 inline 的 Foundation legacy
requirements 必须为空；与 v2 混存在最早 source gate 拒绝，不能到 Writer 丢弃 legacy rows。
仅显式 candidate author 入口接受，不能开启 Playback/CXC 默认 entry。
后续 source schema 固定位置 `schemas/candidate/cuexis.gameplay-author.t4-k4.v1.schema.json`，
它是工具私有 payload schema，不是生产 Chart/CXT root Schema。

### 1.1 工具私有 inline 基础图合同

实现检查发现 Foundation 只有 owning CanonicalSemanticChart 与 CXT expander，尚无 Chart v5
inline JSON Reader。最小反例是只有 format/version/requiredExtensions/gameplay 的 root：
它无法确定 chartId、ENT0、Timing 和 Camera，因此不能用调用者的 typed 图冒充源解析结果。
本补充选定工具私有 root Schema `schemas/candidate/cuexis.gameplay-inline.t4-k4.v1.schema.json`；
它不接受生产 Playback，也不宣称 Stage 8 的正式 v5 source 格式已冻结。

root 必含 format=`cuexis.chart`、version=5、chartId、features、timing、defaultCamera、
resourceClosure、entities、requiredExtensions 和 gameplay；mainMusic 可省略。其他键拒绝。
features 每行 {id,version:u32}。timing={offsetMs:number,defaultBpm:number,tempoEvents,stops}；
tempo row={startBeat:Q,durationBeats:Q,startBpm,endBpm,startSlope,endSlope}；
stop row={beat:Q,durationMs:number}。Q 沿 §2 的规范字符串。
defaultCamera={type,fovY,nearPlane,farPlane,pitch,yaw,roll,defaultTransform?}；
Transform={position:[number,number,number],rotation:[number,number,number,number],scale:[number,number,number]}，
rotation 顺序为 x/y/z/w。所有 number 须有限；Transform 数值须能以 finite float32 表示。
resourceClosure 每行 {assetId,use:"mainMusic|renderableMesh|renderableMaterial"}。
entity={identity,parent?,components,requirements:[]}；identity 是
{kind:"explicit",objectId} 或 {kind:"generated",chartId,bindingId,moduleId,exportId,path}；
path 每步 {nodeId,iterationIndexPlusOne:u32}，不改 marker。
component 是 {kind:"transform",position,rotation,scale}、
{kind:"renderable",mesh,material,alpha:u32(0..255)} 或
{kind:"camera",type,fovY,nearPlane,farPlane}。没有隐含默认值或 opaque 字段。
组合 Reader 在 json_support 验证 root Schema 并产出 owning DTO；typed adapter 只做类型转换，
再由现有 Capsule canonical validation 校验基础图。inline root.chartId 必须等于 Chart context.chartId。
inline 全部 ENT0 来自源 bytes，不从 context.entities 补齐。inline 的 rankAssignment 必须 explicit；
affine 的真实 Repeat cardinality 只由 CXT emission 路径证明。

## 2. Payload 与字段映射

Payload 必含 `version:2`、`authorProfile`、`sourceDocumentId`、`common`、`requirements`、`rankAssignment`。
不允许新增 emissions/rootNodeIds 第二套 AST；真实 emission/Repeat 只来自 CXT 标准 Pattern。
token 非空、UTF-8、精确匹配；下述匿名 ID/空 category/none override 的显式空字符串例外有明确含义。
schema 不允许额外 core key。可选字段省略表示 absent，禁止 null/空字符串代替 absent；
布尔、列表、零数值必须显式；i64/u64 用规范十进制**字符串**避免 JSON 浮点窄化，
u32 用精确 JSON integer；有理数用 `{numerator:i64-string,denominator:u64-string}`，正分母、约分。
十进制字符串不允许加号、空白、前导零或负零；唯一零为 `"0"`，负号仅用于非零 i64。

`common` 是 owning GameplaySource 的公共声明加 Assembly context：
`chartEntryId`、`graphRevision`、`rulesetRef`、`normalizationProfileToken`、
`coordinatorPolicy`、`executionProfile`、`timebase`、`latePolicy`、`graceInputs`、
`declarations`、`resources`、`relations`、`solverProfiles`、`factBindings`、
`judgementDomains`、`declaredFeatures`、`declaredCapabilities`、`namedGraceDurations`、
`patternDefinitions`、`measureDefinitions`。
全部必有；允许合法空列表，不推断默认 profile。
source 结构镜像下面 §2.1 的 typed **declaration**，不镜像 Capsule 的物理 indices/tag/framing。
REF0 projection/claimKey/derived closure/counts/compiled identity/preparedGrace/DFA/proof/sourceMap
禁止作者输入，assembler/Writer 从实体定位关系派生，不能当可信缓存。
common.graceInputs 恰一行对应一条 requirement stableId，行形状 `{requirement:StableId,inputs:GraceInputs}`；
namedGraceDurations 是 `{declarationId,duration:Q}` 列表。源 duration/default 都必须是显式冻结值。
timebase 与 latePolicy 的完整数值进入 source/semantic/judgement 投影；session 执行还必须逐字段相等。

`AuthorCompileContext` 由显式工具调用提供 owning CxtV2Invocation/Chart context、
compilerProfileToken、CompileCapabilityContext、ContentProfileLimits、PatternCompileBudget、
ClosureContributions 和 PreparedIdentityDeclarations；不放在作者 payload 中。
它们沿既有 AssemblyRequest/GameplayPrepareRequest 字段，不补业务默认值；fixture 显式构造。
参数声明/default/Binding 的冻结沿 Foundation CXT，Source nominal Tick 不二次加 startBeat。

每个 requirement object 的核心键如下；除明确 optional 的字段外全部必有。
rank/namespace/claimKey 等派生值不是作者键，不能只交 DSL 字符串或让 Reader 推断。

| 字段 | author 表示 / lowering |
| --- | --- |
| `stableId` | sourceDocumentId + 显式 declarationOrdinal；不从数组位置生成 |
| `identity` | E 六元组全部字段，emissionPath 是有序 `{nodeId,repeatIndex:u64-string}` 列表 |
| `requiredActions`,`domainBinding`,`judgementDomainId` | typed refs 的精确 token；action/domain 不互相推导 |
| `phases`,`requiresReleaseTailSemantics` | phase 名 tap/head/body/tail + 显式 declarationOrdinal；不自动补 tail |
| `pattern`,`patternArmRefs` | 完整 PatternDeclaration；patternArmRefs 是 Requirement 的独立列表，不嵌在 pattern；operand 顺序有语义 |
| `maxArmElements`,`maxDeadlineElements` | `{state:"measured",value:u64-string}`；pending/absent 可诊断但不能通过执行 prepare |
| `measure` | Measure components 与 ordered grade tokens；本阶段只保存，不求 grade |
| `resourceClaims` | 至多一行 `{resourceId,intent,policyToken,graceOverride:{mode:"none",overrideToken:""}}`；observe/consume/claim 分开，namespace/pair/claimKey 从 rankAssignment 派生 |
| `independentCompetition`（禁止 source 键） | 无真实 claim 的实例由 rankAssignment 派生；作者不能在此重复给 pair |
| `grace` | G2-S2 GraceDeclaration，包括显式/单跳/default 来源；不把 end+grace 写回作者 end |
| `timing` | 显式 end、phase-keyed successWindows、body optional、phaseTargets，每项 Tick 用 i64-string |
| `atomBindings` | execution Spec §2.2 的全部字段，含显式 tailOnly；unknown/wildcard 拒绝 |
| `solverProfileRef`,`localClosePolicyToken`,`factBindingRefs`,`requiredRefs` | owning typed references，存在性和 closure 在 prepare 校验 |

### 2.1 Leaf grammar 与确定的名称映射

Q=`{numerator:i64-string,denominator:u64-string}`，denominator 还须 <=INT64_MAX，与已有 rational type
一致，约分/零0/1；Phase=`{kind:"tap|head|body|tail",declarationOrdinal:u32}`。
StableId=`{sourceDocumentId,declarationOrdinal:u32}`；E 精确使用 RequirementIdentity 六个成员名；
emissionPath 每步 `{nodeId,repeatIndex:u64-string}`，其 marker 语义见 §3。
RequiredRefs=`{features:[text],capabilities:[{capabilityId,revision?:text}]}`；revision absent 不写空字符串。
source `requiredRefs` 映射 typed `.required`，`domainBinding`/requiredActions/resourceId 等 token
映射相应强 ref 的 fromToken；不接受 Capsule Rk/Ix/hex 作为作者缩写。

| source leaf | 精确 keys / 映射 |
| --- | --- |
| LocalDeclaration | stableId,kind,localName,references,requiredRefs；reference={scope:"sameDocument|explicitCrossDocument",sourceDocumentId?:text,declarationOrdinal:u32} |
| declaration kind | requirement/patternDefinition/measureDefinition/resourceRecord/judgementDomain/solverProfile，逐名映射现有 enum |
| PatternDeclaration | patternId?:text,matchPolicy:"leftmost-first",root:Node,requiredRefs；匿名 ID 省略，adapter 转为 typed patternId 空值 |
| Node | primitive:"atom|sequence|choice|boundedRepeat|skip|instant|complement",operands:[Node]；atom 才有 atomRef，boundedRepeat 才有 repeatBounds:{minimum:u64-string,maximum:u64-string}；其余 opcode 特有字段禁止 |
| Measure | components:[{phase:phase-kind,categoryToken:text,gradeTokens:[text]}],requiredRefs；空 categoryToken 为显式派生请求，映射 declaredGradeTokens；不默认 grade |
| Resource | resourceId,declaredCapacity:u64-string,slotToken,decisionPolicyRef,terminalAfterTermination:bool,declaredGapGrace:i64-string,requiredRefs |
| Relation | kind:"exclusive",resourceId,members:[StableId],policyToken,declaredCapacity:u64-string,requiredRefs；它不是 per-instance ClaimPolicy，不给竞争 pair |
| Solver | solverId,revision,algorithmToken,objective:[text],tieBreak:[text],rejectIfNonUnique:bool,requiredRefs |
| FactBinding | text ID 列表，只声明存在性，不带 target/effects/material |
| Domain | domainId,coordinateSystemToken,axes:[{axisToken,minimum:i64-string,maximum:i64-string}],frame:"static",requiredRefs；映射 frameResolution=FrameResolution::staticDeclaration，dynamic provider typed 空值 |
| GraceDeclaration | policy:"explicit|inherited|default",allowChartGrace:bool,inheritedFromDeclarationId?:text；只 inherited 有该字段，另外两种映射 typed 空 ID |
| GraceInputs | unitInTicks:Q,minimumCanonical:i64-string,maximumCanonical:i64-string,chartDuration?:Q,inheritedDuration?:Q,defaultDuration?:Q；兼容 candidate canonical shortcut 禁止，统一走 duration resolver |
| Timebase | profileId,unitToken,tickScale:Q,originBeat:Q,initialTempo:Q,tempoSections:[{startBeat:Q,durationPerBeat:Q}],stopSections:[{startBeat:Q,endBeat:Q,duration:Q}]；tempo/stop 的 ordered 契约沿既有 validator |
| LatePolicy | mode:"reject_late|queue_next_tick",finalizationWatermark/maxQueueHop/windowCloseThreshold/windowOpenThreshold 每项={state:"measured",value:i64-string}；mode 映射已有 typed policy |
| Timing | end:i64-string,successWindows:[{phase:Phase,start:i64-string,end:i64-string}],body?:{start:i64-string,end:i64-string},phaseTargets:[{phase:phase-kind,chartTick:i64-string}] |
| AtomBinding | atomRef,domainToken,sourceClass,channelToken,action:"press|release|update",amountRange?:{minimum:i64-string,maximum:i64-string},tailOnly:bool |

common.coordinatorPolicy 必须显式 greedy token；common.graphRevision=`"2"` 是规范 u64 十进制字符串，
source→typed 后为数值2，不是版本 token；source declaredFeatures=[text]、declaredCapabilities 使用
RequiredRefs 的 capability row。未列的 core key 拒绝。unsupportedForms 不作为合法 author field。
Relation.policy 映射 policyToken，claimKeyToken 显式空、competition absent；关系不是 claim，
不能丢失非空 typed relation key：外部 typed 输入含非空 key/pair 时，本 profile 稳定拒绝。
patternDefinitions 是 PatternDeclaration 列表；measureDefinitions 是 `{id?:text,declaration:Measure}`
列表，沿 EncodeRequest additionalPatterns/additionalMeasures 保存未引用定义，不能因未命中而裁剪。
它们与对应 LocalDeclaration 的 patternId/id/localName 做存在性与重名校验；没有定义的声明拒绝。
common.chartEntryId 必须等于显式 Chart context.entryId 与每个 E.chartEntryId；
GeneratedEntityIdentity.chartId 则等于该 context 的基础 ChartId。二者是独立声明，
不把 CXC entry ID 或作者 entry token 强制解释为 UUID。
common.normalizationProfileToken 必须等于 context.identityDeclarations.session.normalizationProfileToken；
common.coordinatorPolicy 与所有使用的 Solver.algorithmToken/Claim.policyToken 必须是
`coordinator.policy.greedy_v1`；Solver objective=`["first-eligible"]`、
tieBreak=`["priority.asc","tieRank.asc"]`、rejectIfNonUnique=true，沿主 Spec §3.8.12，
沿 profiles 和新增 graph 约束保存，不允许读取后丢弃或替调用者声明另一个 policy。
工具输出 self-owned AuthorPreparedArtifact，保有 Foundation chart、PreparedGameplay、
RequirementOwner 表、CapsuleProfiles、additional definitions；timebase/late/context 引用均由 owner 保活。
不只返回一个借用临时 AssemblyRequest 的裸 graph。

## 3. Emission 与 rankAssignment

CXT 只使用 [CXT v2 §§5–7](CXT_V2_FORMAT.md) 的真正 Node/Repeat/Emit 与 CxtV2Loader expansion：
prototype export 是合成 `__direct__`；pattern export 的 repeat count/emit bindings 按既有规则冻结。
不得建立与 Foundation tree 分离的第二套 AST，也不得把其按 nodeId canonical 遍历改成源数组顺序。

生成 E 的确定映射：chartEntryId=显式 common.chartEntryId，invocationId=bindingId，
moduleId/exportId 原值，emissionPath **完整复制** GeneratedEntityIdentity.path 的每个 nodeId/marker，
再附显式 requirementLocalId。Foundation marker 为 emit0/repeat(index+1)，C++ legacy member
repeatIndex 存该 marker，不存 rank 的零基 iteration；u32 marker checked 扩为 u64。
直接 prototype 的 E path 恰为 `[("__direct__",0)]`，不自行生成空 path。
inline 必须声明完全相同的 generated E 和 ENT0 identity；不拿显式 UUID 冒充 generated identity。

requirements 是全实例表；真实 expansion 的每个 emitted entity 按 E 的前五项定位任意多个
显式 requirementLocalId，挂接对应实例字段/stableId。同一 E 恰一行，必须有真实 emitted entity；
零实例不产生 requirement。execution profile 的 LocalDeclaration.localName 是全实例唯一声明名，
通过 stableId 定位 requirement；它不是模板的 requirementLocalId。最小反例：同一 localId
经 2×3 Repeat 得到六个不同 E，强制六个声明同名会与 merged namespace 唯一名称要求冲突。
执行 profile 允许六个独立 localName；历史无 execution profile 的 typed 输入仍保留旧的
localName=requirementLocalId 校验。emitted entity 可无 Gameplay requirement，不把 entityCount 当
requirementCount；显式 stableId 全局唯一，不给所有 repeat instance 复制同一个 ordinal。
原 Field Record 中 Tick 已是 global judgement Tick，不再次加 invocation.startBeat；
实体/表现的 Foundation Beat lowering 仍照原规则做一次 shift，两条 author 路径的基础图也须相等。

遍历前按已接受 Foundation limits 做 finite expansion/counts，非零路径 cardinality checked；
先化简 zero-repeat 子树，不遍历巨大计数才发现实体数超限。不创设新的业务展开限额。
参数/slot/index 仅既有 ValueSource；host 绑定被冻结，未知引用即使在零子树也校验。
root 路径复用实际 module/export/Invocation，不从 source 文件位置或数组行号生成 identity。

rankAssignment 为判别 union：`mode="explicit"` + `rows`，或 `mode="affine"` + `blocks`。
explicit row 恰为 `{identity:E,priority:i64-string,tieRank:u64-string,namespace:text}`；
独立实例 namespace=`@independent` 为 profile 的明确作者值，不能漏填。
affine block 恰为 `{invocationId,moduleId,exportId,requirementLocalId,priority,base,stride,
namespace,nodeOrder,radices}`；priority i64，其余数 u64-string，nodeOrder 是有序稳定 nodeId，
radices 同长且与真实已冻结 repeat count 相等，stride>0。
nodeOrder 只列 outer-to-inner repeat ancestors，不列 emit node。同名 repeat 在一个 family 路径中
无法唯一定位时 affine 拒绝，作者须 explicit mode；不能偷偷补源数组 ordinal。
每个 occupying/独立实例恰好属于一个 row/block；observe 不分配。
rank 中 iteration=`E marker-1`（只对真实 repeat step），emit marker0 不参与。
按 outer-to-inner `index=index*radix+iteration`，tieRank=base+stride*index，全部 checked。
node/path 不完全覆盖、未知 E、重叠 block、缺行、pair collision、claimKey collision 拒绝。
即使不同 resource，block 本身的覆盖歧义也拒绝；不同资源 pair 相同本身不是 collision。
同一 block 若覆盖多个 emit node 而算得同组相同 pair，按 collision 拒绝并使用 explicit mode；
不能在公式之外加 emission 数组偏移。observe 的真实 resource 存在性仍检查，
其 Claim namespace/key/competition/slot 沿 Capsule 全 absent，rank rows 不可包含 observe。
claim namespace+E 的 framing 沿 Capsule §4；独立组不生成真实 claim/slot。
affine 最终只产生显式 pair；AST/block/radix 不写进 Packed runtime records。

## 4. 等价性与参考样本

inline 与 CXT 使用同一 sourceDocumentId、stableId、E、common 和最终数值，
即使原文件路径/排版不同也可 canonical graph 全字段相等。
若作者显式 sourceDocumentId/E 不同，预期不等价；不得把 identity 差异用 sourceMap 过滤掉。
semantic diff 只忽略 sourceMap/物理容器位置，不忽略 sourceClosure、ordered lists 或 identity fields。
content-artifact identity 可因文件/来源 AST 不同变化，judgement 执行投影在等价图下相同。
双路输出都经过 assemble → prepare → revision3 Writer → Reader → reprepare；
必须保留 .chart.json/.cxt/Bindings、explicit-rank 对照和 normalized graph diff 工件。

affine golden：外层 nodeA radix2、内层 nodeB radix3，base10,stride2，priority=-1，
rank iterations(0,0),(0,1),(0,2),(1,0),(1,1),(1,2)，相应 E 的 repeat markers
为(1,1),(1,2),(1,3),(2,1),(2,2),(2,3)，另含完整 emit step0；tieRank 恰为 10,12,14,16,18,20。
作者表/声明/资源集合重排不改变输出；交换 nodeOrder 或 choice operand 顺序是语义变化。
zero-repeat、u64 溢出、重复 stableId、跨 invocation 隐式引用、同名 relation、amount absent0、
missing phase target、tailOnly 非 release 均有负例；错误必须在发布 prepared output 前产生。

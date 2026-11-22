# P3：后继准备内容、成品与 CXC 源包

状态：candidate

更新日期：2026-11-22

实现状态：未实施。[T-13](../tasks/T-13.md) 仍属 `NEXT`；设计、采用和发行分开登记，[唯一派单授权入口](../plan.md#唯一派单授权入口)与 D-05 范围不变。

<!-- cuexis-meta
id: PLAN-active.stage-07-rr.contract-prepared-artifact
status: candidate
authority: owns
-->

## 拥有关系与入口

按 [D-25](../architecture-design.md#后继成品与-cxc-源包)采用 move-only、建成后只读的 build-only `gameplay_packed::CompiledCapsule`。它拥有下表 typed 数据及派生索引。Writer 和 Reader 使用该数据；Playback 移入同一 `PreparedPlayback` candidate，继续使用同一世界实例化、Property Resolver、Behavior 求值、AnimationSampler/Mixer、玩法推进和渲染路径。

| 拥有数据 | 构建及消费 |
| --- | --- |
| `chart::CanonicalSemanticChart chart`、`PresentationRows presentation` | 基线、full E、父子树及静态组件来自 `chart`；`presentation` 是下文 `SCN1/BHV1/ANI1` 的 owning rows。实例化时由 ordinal 建本局 entity handle；基线从实例化后的组件捕获，wire 无第二份。 |
| `chart::CompiledChartTiming timing` | 拥有 [TimingMap 和 offset](runtime-timing.md#谱面时间-api)。所有表现及玩法使用这一 map；播放时只应用一次 offset。 |
| `judgement::PreparedGameplay gameplay`、canonical graph rows、beat keys、`vector<RequirementOwner> owners` | 保存运行表、完整 canonical graph rows（含未用的合法 Pattern/Measure）、独立精确拍位及完整 Requirement→E owner，保留 profile 和 source document IDs。 |
| `ResourceRows resources`、`Digest compiledDigest` | 含 `RES1` 的用途及内容闭包。`Digest = array<uint8_t,32>`；Reader 和 source 终结用同一计数和散列 visitor。Provider、设备、GPU 按原候选生命周期管理。 |

共享 `engine/judgement/internal/cuexis/judgement/compiled_gameplay_rows.hpp` 供 T-12/T-13。`BeatGameplayProduct` 是**未终结**的拥有型结果：`CanonicalGameplayRows rows; vector<CompiledBeatKeys> beatKeys; LatePolicyParameters latePolicy; vector<PreparedRequirement> requirements; vector<PreparedResource> resources; vector<PreparedTimerKey> timers; PreparedNormalizedScore normalizedScore;`。`CompiledBeatKeys { timing::RationalBeat end; optional<pair<RationalBeat,RationalBeat>> body; vector<pair<PhaseKind,RationalBeat>> phaseTargets; }`，phase 声明序。`CanonicalGameplayRows` 含 GPH0 单 row、GPR0 rows、GPD0 的 `patterns/measures/judgementDomains/mergedDeclarations` 和 GRC0 的 `resources/relations/solverProfiles/factBindings/claims` rows；row 为 `vector<GraphAtom>`，`GraphAtom=variant<bool,int64_t,uint64_t,string,GraphReference>`，`GraphReference { uint8_t kind; string token; }`。Atom 类型由对应旧 typed row 的 traversal 固定，原 IEEE bit pattern 字段仍以原原子表示，token 拥有其 bytes。该 DTO 不持 `PreparedGameplay`、外部身份、host mapping、calibration 或 session。Writer 从终结结果的同一 canonical rows 和不可变 `gameplay.records()` 取数据。

内部 `engine/gameplay_packed/include/cuexis/gameplay_packed/compiled_capsule.hpp` 在 `cuexis::gameplay_packed_read` 声明以下同步接口；实现落 `src/compiled_capsule_read.cpp`、`src/compiled_graph_read.cpp`。不安装，返回值不借源、字典、archive/DOM 或 provider。

```cpp
auto decodeCompiledCapsule(std::span<const std::byte>, const CompiledDecodeContext&,
                           CompiledArtifactLimits) -> core::Result<CompiledCapsule>;
auto decodeCompiledGraph(std::string_view, const CompiledDecodeContext&, GraphReaderLimits,
                         CompiledArtifactLimits) -> core::Result<CompiledCapsule>;
```

候选 `cuexis::gameplay_packed_source` 提供下面两个分开的 build-only 头和实现，不进入 read 闭包。`engine/gameplay_packed/source_include/cuexis/gameplay_packed/compiled_writer.hpp` / `src/compiled_writer.cpp` 声明和实现两个 encode；同根 `compiled_source.hpp` / `src/compiled_source.cpp` 声明和实现 compile。该目标显式依赖 `gameplay_packed_read`、`judgement_compile`、`chart`、`timing`、`core`，JSON source adapter 私有 `json_support`；无 `playback` 边。Gameplay assembler 及被启用的 source bridge 链这一目标，只有成品读取的宿主保留纯 read 边。

```cpp
auto encodeCompiledCapsule(const CompiledCapsule&, CompiledArtifactLimits)
    -> core::Result<std::vector<std::byte>>;
auto encodeCompiledGraph(const CompiledCapsule&, GraphWriterLimits, CompiledArtifactLimits)
    -> core::Result<std::string>;
auto compilePreparedArtifact(std::string_view chartJson, const CompiledSourceContext&,
                             CompiledArtifactLimits) -> core::Result<CompiledCapsule>;
```

`CompiledArtifactLimits` 拥有 `chart::PackedChartLimits packed; chart::ChartLimits chart; CompiledResourceLimits resources; size_t maxPropertyWritesPerFrame;`，后两项来源见[限额](#限额与稳定拒绝)。`CompiledDecodeContext` 按值含 `judgement::CompileCapabilityContext capabilities`、`judgement::ContentProfileLimits contentLimits`、`judgement::PreparedIdentityDeclarations identities`、`judgement::PrepareGameplayLimits prepareLimits`、`optional<Digest> expectedCompiledDigest`；CXC 成品必传 expected，裸读及 CXC 源可缺。无作者 request、旧 Timebase、Score/Exact 覆盖。`CompiledSourceContext { const CompiledDecodeContext& admission; span<const chart::ProjectDocument> documents; span<const CompiledAssetInput> assets; judgement::PatternCompileBudget patternBudget; }` 同步借输入，documents 是 portable path 到 CXT 文本，每个 `CompiledAssetInput` 即 §资源闭包的 `Asset` 行，调用方从已验证 provider 算 byteCount/digest。只以包或工程快照解析，不在 Reader 或编译器内寻宿主文件。

裸 Chart、工程、CXC 源与离线工具在显式选定 Chart/资产索引后调同一入口，按[源输入与 lowering](#源输入与-lowering)运行；它们与 Reader 共用[身份终结顺序](#身份终结)。

Reader 只做有界解码、typed closure 检查、`TimingMap::fromCompiled`、索引、dispatch/timer 与 reverse reachability；安全运行表 prepare 可用，禁止调用 `ChartV4Resolver::resolve`、`ChartCompiler::compile`、`AnimationCompiler::compile`、`compilePattern`、grace resolution、`assembleGameplay`、`prepareResolvedGameplay` 或 `semanticDiff`。现有 `AnimationCompiler` 的计数与 index 填充可抽为 resolved 行的普通检查函数。成品不存源 JSON、CXT 文本、EnTT 对象、宿主覆写、输入、结算、历史或逐帧轨迹。失败不替换既有 active。

`cuexis::gameplay_packed_read` 的允许边为 `cuexis::core`、`cuexis::judgement`、`cuexis::timing`、`cuexis::chart`，另 `PRIVATE cuexis::json_support` 用于 Graph SAX。沿现有 Chart 目标及共享 build-only 产物头，不拆 Chart。Reader 不能调用 Chart/Animation 作者 resolver。Writer 共用 Reader 的有界 visitors；源 adapter 与旧 codec 各走独立路径。

**头文件与链接隔离。** T-10 先按[运行纯值头](runtime-timing.md#judgement-运行与旧作者头链)把四个 `*IdentityDeclaration[s]`（Engine、Ruleset、Session、Prepared）原样定义在 `engine/judgement/include/cuexis/judgement/prepared_identity.hpp`，把 `StableDeclarationId` 原样定义在运行 `phase_types.hpp`。旧作者头仅转引；`prepared_gameplay_records.hpp` 直接包含两头，纯 Core/Judgement 消费者可构造完整 `CompiledGameplayView`，不需要内部根或作者头。`SessionConfiguration` 继续按值拥有身份声明，prepare 复制到不可变 storage；当前不提前安装 Judgement，D-19 晋升时这两头随运行闭包安装。

T-12 独占首创 `engine/judgement/internal/cuexis/judgement/compiled_gameplay_context.hpp`：从旧身份头仅提取 `CompileCapabilityContext`，从 `gameplay_assembler_publication.hpp` 提取 `ContentProfileLimits`，保留原命名空间、成员与布局，直接包含 `prepared_identity.hpp` 和运行 `MeasuredParameter` 定义头；不再定义四个身份类型。T-12 同卡完成旧头转引，首创共享 `compiled_gameplay_rows.hpp`；T-13 只消费这两个已交付头，不重新提取。`PatternCompileBudget` 留 `judgement_compile` 的 `gameplay_assembler_compiled.hpp`。共享 `engine/judgement/internal` 以 `$<BUILD_INTERFACE:…>` 给 `judgement_compile`，T-13 Reader 自己声明同一 build-only 根；运行公共头不包含该 context/rows 或未安装 include。`CompiledChartTiming` 从 `engine/chart/internal` 同样取得。T-13 才把原 `GraphReaderLimits/GraphWriterLimits` 原样提取到 `engine/gameplay_packed/include/cuexis/gameplay_packed/compiled_graph_limits.hpp`，两新接口和旧 `gameplay_capsule.hpp` 转引。各目标、首创卡和合法子目录顺序按[构建、头与安装](runtime-timing.md#构建头与安装)。

Reader build-interface 只导出 Packed 的 `include` 根；将旧 `gameplay_capsule.hpp` 移至 `engine/gameplay_packed/legacy_include/cuexis/gameplay_packed/`，原 include 拼写保持，由 legacy 目标导出 `legacy_include` 与提供 Graph limits 的 `include` 根。Source 目标额外导出 `source_include`，并经 `judgement_compile` 才能看到作者根；legacy/source 均不倒流给 Reader。经 include 根测试裸 `#include`；另用手写等签名声明强制实际调用，独立验作者、旧重装配和 Playback 符号在纯 Reader 消费者不能链接。只有合法运行和独立成品读符号随 Reader 可达。

## 源输入与 lowering

候选 `cuexis.chart` v5/玩法 v3 的根外壳只由本节定义；内部玩法字段、窗口、档和计分由[作者玩法](runtime-timing.md#作者版本stop比例和最小多等级)定义。`engine/chart/src/chart_v5_loader.cpp` 提供 build-only 解析和表现 projection；`compiled_source.cpp` 读取 Gameplay DTO、展平玩法引用、连接时间和终结。两者同步处理同一个有界源快照；Chart 不依赖 Judgement。根 `additionalProperties:false`：

| 根字段 | 规则 |
| --- | --- |
| `format,version,chartId,metadata,timing,parameters,templates,behaviors,animationTemplateImports,animationClips,objects,requiredExtensions,extensions` | 全部必填；采用 [Chart v4 schema](../../../../../schemas/cuexis.chart.v4.schema.json) 同名字段（`version` 固定 5；下述差量）。数组可空；不猜版本。唯一 timing 来自这里；省略或另写为 Gameplay timebase 均非法。 |
| `camera,audio,framings` | 可选；audio 用 v4 `audio.version=1,mainMusic:{domain:"asset",id}`；camera 差量见 SCN1，framings 只按[作者画幅](render-player-writer.md#作者画幅)。 |
| `gameplay` | 必填，根路径 `/gameplay`，必须 `version=3,authorProfile="gameplay.author.beat-us.v1"`；编译器 profile 取该常量。`requiredExtensions` 必须且只含 `{"id":"cuexis.gameplay.v3","version":3}`；禁止以字符串写 token。`extensions` 不可再声明 `cuexis.gameplay.v2` 或 `cuexis.gameplay.v3`；未注册的非必需 opaque extensions 视为仅作者 metadata，不进入运行语义。 |
| `cxtImports,cxtInstances` | 可选，缺省 `[]`；独立于 v4 的动画导入。形状和绑定见下。 |
| `entities,features,defaultCamera,resourceClosure,mainMusic` | 旧 inline 外壳专用，v3 路径拒绝；用 `objects`、玩法能力声明、`camera`、从实际 uses 查资产、`audio.mainMusic`。不会调用旧 `readAuthorSource` 的 inline root reader。 |

**表现 lowering。** `objects` 的 direct `components` 与 `template+overrides` 互斥，沿 v4 `object/template/patch`，先完成继承、对象覆盖及 typed 参数绑定，再按 resolved 对象生成 `ExplicitEntityIdentity{ChartObjectId}`、父引用、Transform/Renderable/Camera、SCN1 flags、Behavior 绑定和 Animator。用 `ChartV4Loader::loadV4Value` / `ChartV4Resolver::resolve` 的同名 typed 解析/参数/掩码/引用检查；抽出 typed helper 接收 v5 扩展字段，无须序列化重读 v4 JSON。Renderable v4 shape 增 `alpha` 精确整数 `[0,255]`、缺省 255，允许原整组件 patch 及 `/components/cuexis.renderable/alpha` 的 add/remove/replace；v5 lowering 保留最终值，不能先按对象 ID 记值再解析继承。Behavior 接受 v4 `behavior.event` v1 和原 v5 `behavior.event` v2：两者用 v4 `events/stepEvents` shape；v2 用 `render.alpha` 整数端值、拒 `material.opacity`，两者保留原其余 typed predicates。为保留 legacy keyframe，允许原 `behavior.transform.keyframe` v1 的 typed author shape（`canonical_chart_loader_behaviors.cpp::parseBehaviors` 的 `tracks` 路径）；仅可绑定实际支持属性。Behavior 组件 `cuexis.behavior`、Note/Element、Camera、Animator 均走原组件/引用规则。动画 Clip、混合、动画 CXT v1 和参数沿 v4 shape，v4 Clip 的 `material.opacity` 仍合法；不可在同一编译 pass 先执行旧 `compileBehaviorCandidate` 的「无模板/Clip」门禁。成品中的 Static/BHV1/ANI1 是 resolved 语义，不保存上述源形状。未能表示的已发布表现以 `unsupported_feature` 定位具体源字段。

**CXT v2。** `cxtImports[] = {id,source}`；`source` 为 Chart 所在工程根下的 portable lowercase `.cxt` 路径，使用包/工程 document 表精确查找；`id`、`source` 分别唯一且 `id==moduleId`。`cxtInstances[] = {bindingId,module,export:{kind,id},startBeat,parent,parameters,slotBindings}`，全部必填，无其它字段。`bindingId` 全谱唯一；`module` 引用 import id；`export.kind` 只准 `prototype|pattern` 且匹配该 module 的 `exports`；`parent` 为 v4 `objectReference` 或 null，只指显式对象。`parameters[] = {id,value}`，`slotBindings[] = {slot,value}`，value 为精确 i64 整数或 Beat literal，并按 CXT 声明类型检查；各表键唯一，未绑定用模块默认；pattern 的 `slotBindings` 为空。`startBeat` 是 Beat。分别投影至 `CxtV2Invocation.chartId/bindingId/moduleId/exportId/startBeat/parent/parameters/slotBindings`。v4 `animationTemplateImports` 只接 CXT v1 动画，不能使用同一条 import/instance 同时提供两种实体/动画解释。

v2 文档根 `format="cuexis.animation-template",version=2`，Core 字段和展开限额按 `CxtV2Loader::expand`，所有 `prototypes[].requirements` 为空。玩法模块只在 `/extensions/cuexis.gameplay.v3` 携 v3 payload（禁止根 `gameplay`、旧 `extensions["cuexis.gameplay.v2"]` 与双声明），同时采用 CXT v2 的**字符串** `requiredExtensions:["cuexis.gameplay.v3"]`；无玩法时此标识和对应扩展都缺席。先验证包装并取出该已识别的 token 和 payload，从 typed Core 投影移除它们再调用原 expander。模块 payload 用 Chart `/gameplay` 的同一 `version,authorProfile,sourceDocumentId,common,requirements,rankAssignment`；`common` 按玩法章的 typed fields，`chartEntryId,graphRevision,normalizationProfileToken,coordinatorPolicy,executionProfile,latePolicy,gradeTables,noteTypes,scoreMode` 必须与 Chart 的 canonical 值相同（按 typed 顺序/键归一后比对），只由 Chart 决定最终值。`presentationBindings` 只准 Chart `common`；其它声明、资源、Pattern/Measure、capability/feature refs、grace 输入和 ranks 按原 stable merge，重复语义键拒绝；各玩法文档 `sourceDocumentId` 唯一，每个被引用模块读取和加入一次。未使用 import 的玩法声明保留，任何实际 requirement 必须对应已有 invocation。`requirements` 包含该文档已声明的完整 K4 源实例行；按 `identity.invocationId` 选择实例，要求 `moduleId/exportId` 和完整 emissionPath 精确匹配实际 Core 展开物。Requirement 的 `endBeat/body/phaseTargets` 在模块中为 invocation-local，由实例 `startBeat` 平移一次；相对窗口、等级区间和 Exact 不平移。仅玩法部分用 `gameplay_author.cpp::frozenCount/affineRank/ranks` 的 checked repeat 参数/顺序逻辑，`explicit` 和现有 CXT affine 规则沿原 typed 定义；不能按字符串猜 generated ID。CXT JSON 不入成品。

**统一实体与 owner。** 显式对象玩法行只在 Chart `/gameplay/requirements`；K4 的 `chartEntryId` 等于 Chart `common.chartEntryId`，`invocationId` 等于该对象的 UUID，`moduleId="__chart__",exportId="__object__",emissionPath=[{nodeId:"__direct__",repeatIndex:"0"}]`（原 K4 整数字符串）。`requirementLocalId` 保留作者值。CXT 保留生成 full E `{chartId,bindingId,moduleId,exportId,path}`，图上 `invocationId=bindingId`；`(nodeId,repeatIndex)` 与 E 的 `(nodeId,iterationIndexPlusOne)` 逐项相等，非索引步骤为 0，indexed 步骤为 index+1，与 `gameplay_author.cpp::sameEmission` 同规则；CXT moduleId 禁用 `__chart__`。生成组件/父子与显式对象合入同一场景，legacy Foundation requirements 禁止。每个 GPR0 row 恰有一个匹配的 `RequirementOwner`，全 E 比对、允许同一 E 拥有多条不同 local ID 的 requirement；Writer/Reader 校验同样两种 owner 映射，不用运行哈希或对象 ordinal 代替 K4。 无匹配或未列入源闭包的 `sourceDocumentId` 报 `dangling_reference`，多匹配或来源冲突报 `binding_conflict`，定位相应 identity；不创建占位对象。

**逻辑谱目 ID 的校验边界。** 源编译逐条将完整 K4 的 `chartEntryId` 与 Chart 的 `common.chartEntryId` 比，异值报 `dangling_reference` 于实际 `requirements[i].identity.chartEntryId`。独立 Capsule/Graph Reader 没有作者 common：先验每行 K4 的合法 token，非空 GPR0 以首条的逻辑 ID 为内部参照，后续 K4 必须相同；首个异值报 `binding_conflict`（关系阶段，定位对应 GPR0 行与 identity 字段）。零条不推导 ID。两 Reader 另验完整 owner、来源闭包、各镜像与载体自摘要，再比 `expectedCompiledDigest`（若有）。CXC 成品必须传该行 expected；与同包 metadata 一致只证明包内一致。外部独立保存的 expected 可绑定指定内容。任何成品自述都不能证明未提供的原作者值；不从 Chart UUID、物理 path 或文档 ID 推导，不改 wire 或重复保存逻辑 ID。

**源管线。** 完成 schema/版本/引用/闭包预检后准备上述 owning 基线及动态行、合并玩法与已平移拍位，收集一次[共享锚键](#共享时间与运行时边界)。将同一规范 timing source 交 `TimingMap::compile` 和 `compileBeatGameplay` 做精确 source binding；不按成品 digest 反推作者输入。源格式/扩展拒绝先于 resolver，符号、资源、拍算术错误先于 encoder/宿主能力。版本/字段形状错分别用 `revision_mismatch`/`source_shape`，源模式重复用 `source_authority_conflict`（指第二个 authority），源玩法可表示性和未启用入口走各自的 `unsupported_feature` 路径。

`compiled_source.cpp` 从 Chart 和同一源闭包内每个 CXT 玩法 payload 的 `common.patternDefinitions`、`common.measureDefinitions` 取所有完整定义体，包括匿名和未被实例引用的定义。同步调用 `compileBeatGameplay` 时，`BeatGameplaySourceView.sources` 传原有声明、稳定引用和 requirement，另按 [T-12 的 `BeatGameplaySourceView`](runtime-timing.md#作者版本stop比例和最小多等级) 将全部 `PatternDeclaration`、`BeatMeasureDefinition` 分别传入 `patternDefinitions`、`measureDefinitions` span，由 adapter 持有至调用返回。按既有 stable merge 交接；仅具名定义与同种、同名的 `patternDefinition` / `measureDefinition` 声明校验双向一一对应，并沿原规则拒绝缺失、孤立或重复的具名项。空 `patternId` 或缺席 Measure `id` 的定义照常传入，跳过具名匹配，沿既有 inline 路径处理。T-12 以这些定义和 merged declarations 生成完整 GPD0 owning rows。不能只从 `LocalDeclaration`、已展开实例或运行程序回推定义，不能调用旧 Packed Model 补表。

## 版本、目录与原语

`capsule.5` 复用 [Packed §5.1–5.2](../../../../formats/PACKED_CHART_FORMAT.md#5-文件布局) 的 magic、96-byte header、32-byte 目录、CRC32 与字段偏移；`packedVersion=1,flags=1,candidateRevision@88=5,eventCount@72=0`（表现计数由各表检查），`semanticIdentity@32` 为本页 digest。目录按 tag 字节升序，`flags=codec=reserved=0`，storedBytes=decodedBytes，紧密连续直到 EOF。复用 [Capsule §3、§12–13](../../../../formats/GAMEPLAY_CAPSULE_V2_FORMAT.md#13-capsule-物理-revision-4-与-accepted-hash-work) 的 frame 和必需静态集合及 `GPH0/GPR0/GPD0/GRC0`；新增必需表 `SCN1/BHV1/ANI1/TIM1/JPR1/RES1`。禁止 DBG0、其它未知表、重复表和替代压缩。`REQ0` 为 `00 00`、records=0，`CNS0` 为空、records=0；实际玩法 requirement 只在 GPR0。`STR0/REF0` 只含旧字段使用的实际项，新表不引用旧字典。

`META/TIME/IDN0` 沿 [Packed §6.3–6.5](../../../../formats/PACKED_CHART_FORMAT.md#63-meta)，`ARCH/ENT0/TRN0/REN0/CAM0` 沿 §7.1–7.3 和 rev4 typed decoder，不运行 Foundation §7.6 profile。`STR0,REF0,META,TIME,IDN0,ARCH,ENT0,REQ0,CNS0` 必须存在；TRN0/REN0/CAM0 仅在对应组件实际存在时出现。默认相机 static projection 见 SCN1。`GPH0` 的 `gameplayVersion=3,graphRevision=3`；旧 timebase frame 唯一值为 id=`engine.tick.us.v1`、unit=`us`、scale=`1/1`、origin=`0/1`、initialTempo presence=false、tempo/stop 均空（Graph 同位 false 不跟 rational）。连续时间以 TIME 为唯一来源。旧 ruleset 引用槽类型/位置不变，值必须等于 JPR1 `ScoreMode.id`，没有独立作者字段或宿主覆盖。其它字段按 [Capsule §4–8、§12–13](../../../../formats/GAMEPLAY_CAPSULE_V2_FORMAT.md#4-ref0-注册与投影引用) 的完整字段与 rev4 差异表、源码 `gameplay_capsule_visit_internal.hpp::registry/visitFields` 和 `gameplay_capsule_emit.cpp::physicalTables` 的 traversal。GPD0 的每一 Measure component（包括未用定义）`gradeTokens` 计数固定 0，Reader 拒非空；不同实例可共享同一具名 Measure 而选择不同表。运行等级仅从该 requirement 的 JPR1 `gradeTableIndex` 按表求值，不回写声明或对多个实例取并集。旧载体语义不变。

`graph.2`：`format="cuexis.gameplay-graph",graphFormatRevision=2,capsuleRevision=5`；根键顺序严格为 `format,graphFormatRevision,capsuleRevision,semanticIdentity,staticChart,GPH0,GPR0,GPD0,GRC0,scenePayload,behaviorPayload,animationPayload,timingPayload,preparedPayload,resourcePayload`。前九项沿 `schemas/cuexis.gameplay-graph.v1.schema.json` 的 `staticChart` 与 typed row schema/SAX，改上列常量及 GPH0 字段值。后六项依次为 SCN1/BHV1/ANI1/TIM1/JPR1/RES1 的完整 canonical payload。JSON string 解码后须为偶数个 `[0-9a-f]` ASCII 字节；不接受前缀、空白或大写。JSON 合法转义若解成同样字符可接受，例如 `\u0030` 与 `0` 的 semantic digest 相同，原文件 SHA 不同；Writer 只输出直接小写 hex。SAX 前以 `GraphReaderLimits.maxBytes` 守原始全文；收到已解码 string 后，检查 `maxStringBytes` 与 `length/2 <= maxPackedSectionBytes` 再装载，不做 raw token 扫描。新表 records 由同一 payload 定义得出，逻辑预算同 Capsule。未知、重复、缺键和乱序失败，两个版本均不得 fallback。

新表不使用 GPH0 frame。`u8/u16/u32/u64` 无符号，`i64` 二补码，`f32/f64` IEEE binary32/64，全部 LE。浮点有限；源 -0 规范化到 +0，Reader 拒绝 -0，不重归一化 quat。`Bool=u8` 0/1，`Opt<T>=Bool` 后仅在 1 时紧跟 T，`Vec<T>=u32 count` 加 count 项，`Str=Vec<u8>` 且有效 UTF-8、无 NUL，ID 再过消费字段的原 token validator。`Beat=i64 num,i64 den`，正分母、最简、零 `0/1`；`Q=4*f32` 的 x/y/z/w；`V=3*f32` 的 x/y/z。`EIx=u32` 的 IDN0 ordinal（Graph 为 canonical `staticChart.entities` ordinal）；`BIx/CIx=u32` 的 behavior/clip ordinal。无 padding、隐式扩展位、索引 sentinel 或尾字节。除 Opt/空向量外无 wire 缺省；源缺省均在 lowering 展开。

## 场景与动态表现

### SCN1 场景补量

records=实体数；`Opt<{Q rotation,V scale}> defaultTransform, FramingPayload framing, Vec<EntityFlags> entities`。`EntityFlags = Bool element,Opt<Beat> noteBeat`，和 IDN0 同序等长。无 defaultTransform 时 META position 须为零；有则合并 META position 与本表 rotation/scale。值域用 `CameraData/TransformData` 及 `ChartWorldInstantiator::validate`；非默认相机仍用 CAM0。Note/Element 对应 `RuntimeObject.components.note/element`，独立于 GPR0 owner。`FramingPayload=Vec<{u32 width,u32 height,f64 verticalScale}>`，一行投影一个 [FrameFraming](render-player-writer.md#作者画幅)，保持作者次序，首项默认，空表为未声明。Reader 校验原合同的项数、已约分非零 pair、无重复和 scale 有界；此处无第二份比例或投影参数。

**默认相机。** v5/v3 `camera` 沿 v4 值域；`defaultTransform` 只接受旧 `{position}`，或完整 `{position,rotation,scale}`（literal vec3/quat/vec3）。完整形状禁同时写 `pitch/yaw/roll`，即使其值为零，报 `source_authority_conflict`。旧形状/未给 transform 时，lowering 只在构建阶段按 `chart_world_instantiator.cpp` 的现有 Euler→Quat 公式得到旋转，scale 为 `(1,1,1)`；缺 camera 取原默认镜头及 identity transform。无 transform 字段且 Euler 皆为零时 SCN1 写 absent；有 transform 或非零 Euler 时写 present。canonical `META` 的 pitch/yaw/roll 为零，position 如上；Graph `staticChart.defaultCamera` 的同三字段为零，`defaultTransform` 仅作静态投影：absent 编 `null`，present 编 position 加 identity rotation、unit scale。两 Reader 先验证投影再合并 SCN1，Quaternion 不重新从 Euler 推导。正有限 scale 各分量须可表示为非零 f32，Quat 沿 TRN0 的归一性校验；还须保证现有 TRS 组合及其 `core::inverse` 有限且可用，失败报 `scalar_invalid`（源原字段或 `/SCN1/defaultTransform`）。

NEXT 在 `chart_world_instantiator.cpp` 隐式 camera 创建位置增加候选分支，直接用上述 position/rotation/scale 写 `world::TransformComponent`，`CameraComponent` Euler 槽一律 0；`presentation_materializer.cpp` 继续从该世界矩阵取逆构造实际 view，各画幅复用。旧入口仍用原 position/Euler/unit scale 分支。隐式 camera 没有 full E 或 EIx，不可以被 BHV1 或 ANI1 绑定；`camera.fovY` 行只绑定有 Camera 组件的实体。

### BHV1 行与行为绑定

records=定义数；`Vec<Definition>,Vec<{EIx owner,BIx definition}>`。Definition 为 `Str id,u8 kind,Vec<KeyTrack>,Vec<EventTrack>,Vec<StepTrack>`；kind=0/1/2 为 `behavior.transform.keyframe` v1、`behavior.event` v1、`behavior.event` v2 的已准备记录，保留原输入 profile 对这些 kind 的允许规则。v2 alpha 已除 255 变成 opacity。Keyframe 只有 key tracks；event 只有 events/steps 且总行数非零。定义按 BehaviorId 字节升序唯一（保留未绑定定义）；绑定按 EIx 升序唯一，可复用定义。各表按 property 号升序唯一。

| 行 | 字节顺序 |
| --- | --- |
| `KeyTrack` | `u8 property,Vec<{Beat beat,Value value,u8 easing}>`；easing=0/1/2/3 对应 Linear/InCubic/OutCubic/InOutCubic；首键 canonical=0，其余是右键的 easing。 |
| `EventTrack` | `u8 property,Vec<{Beat start,Beat end,Value startValue,Value endValue,f64 startSlope,f64 endSlope}>`；end>=start，相等表示 instantaneous，此时端值相同、两个 slope=0。 |
| `StepTrack` | `u8 property,Vec<{Beat beat,StepValue value}>`。 |

属性 0/1/2=`transform.position.x/y/z`（f64），3=`transform.rotation`（Q），4=`transform.scale`（V），5=`camera.fovY`（f64），6=`material.opacity`（f64），7=`material.tint`（V），8=`render.visible`（Bool），9=`render.material`（Str AssetId）；0–7 为 `Value`，8–9 为 `StepValue`，无额外 variant tag。值域沿 `chart_runtime.cpp::valueMatchesProperty/compileBehaviorTracks/compileEventBehavior` 与 `ChartWorldInstantiator::validate` 的 typed predicates，移出校验不调用编译。Slope `[0,3]`；Event 按精确 start 严格递增、相邻不重叠；key/step 按精确拍严格递增；存在的 track 非空。源仍验证 `groupId`，lower 后同拍共有 tick 即保持边界，同组标签不入 wire。绑定必须指向实际具有对应 property baseline 的实体；不得自动补缺组件。所有 Material Step 目标纳入 RES1。

### ANI1 Clip、绑定和混合

records=clips 数；`Vec<Clip>,Vec<ObjectProgram>`。按 `animation_program_input.hpp` 的 `AnimationProgramClip/ObjectAnimationProgram/ResolvedAnimationLayer/ResolvedBlendGroup/ResolvedClipInstance` 和 `chart_v4_document.hpp` 的 `AnimationSegment/AnimationStep` 投影。`AnimationClip::id` 用显式 id 或生成 templateId 恢复；保留 full tuple，禁止从 source 重新展开 CXT。诊断 fieldPath 由解码行位置构建。

| 行 | 字节顺序 |
| --- | --- |
| `AId` | `u8 kind`：0 后 `Str id`；1 后 `EIx owner,Str bindingId,Str templateId,u8 recordKind`，其中 0/1/2/3=clip/layer/group/instance。 |
| `Clip` | `AId identity,Beat duration,Vec<CurveTrack>,Vec<ClipStepTrack>`；duration>0。 |
| `CurveTrack` | `u8 property,Vec<{Beat start,Beat end,Value startValue,Value endValue,f64 startSlope,f64 endSlope}>`；本地拍 `0<=start<end<=duration`。 |
| `ClipStepTrack` | `u8 property,Vec<{Beat beat,StepValue value}>`；`0<=beat<=duration`。 |
| `ObjectProgram` | `EIx owner,Vec<Layer>`。 |
| `Layer` | `AId identity,i64 priority,f64 weight,u16 mask,Vec<Group>`。 |
| `Group` | `AId identity,u8 mode,f64 weight,Vec<Instance>`；mode=0/1 对应 Override/Additive。 |
| `Instance` | `AId identity,CIx clip,Beat start,Beat scale,u16 iterations,u8 fill,f64 weight,u16 mask`；scale>0；iterations=0 为 infinite，正数为 v4 `AnimationIterations` 有限计数；fill=0/1 对应 None/Hold。 |

属性用 BHV1 的 0–4、6–9；mask bit 依该属性编号，其余必须为 0。源 `PropertyMask` 两表均空时写全部允许位，否则按 `AnimationMixer::matchesMask` 的 exact name 或 prefix 并集展开；没有匹配写 0。层与实例取交；weight 均 `[0,1]`。属性行升序唯一；segment/step 按精确 start/beat 严格递增，segment 不重叠。空向量、Quat、Additive 合法属性、离散 override unit weights/冲突与生成对象可写性沿 `chart_v4_animation_reader_values_internal.hpp` 的值读取校验及 `chart_v4_resolver_program.cpp` 的 typed mask/binding 校验；Reader 直接对 resolved 行、实际组件与已展开 mask 检查。无权重 contribution 就不产生写；不剪裁实际只读 definition 或已声明但不贡献的实例。

Objects 按 EIx 升序；Clips 按 AId；层按 `(priority,AId)`；组、实例按 AId，均严格升序。AId 先 kind 再显式 ID 的 UTF-8 或生成 tuple `(owner full E canonical bytes,bindingId,templateId,recordKind)`，比较与 owner EIx 升序等价。Clips、instances 的 ID 各全表唯一；层在对象内、组在层内唯一。生成绑定须恰有同 tuple 的 clip/layer/group/instance，后三项嵌套，instance 引用该 clip；位置与 recordKind 匹配，不跨对象。保留未引用 explicit clip，其材料仍查闭包。连续采样、逐属性 blend、priority 及同级权重 tie-break 沿 AnimationSampler/Mixer；同一 owner 的 generated AId 比较不退化为 runtime handle 或作者数组顺序。

## 共享时间与运行时边界

`TIM1` records=1；`i64 offsetUs,Vec<{Beat beat,i64 at}> tempos,Vec<{Beat beat,i64 start,i64 end}> stops,Vec<{Beat beat,i64 at}> anchors`。各列按精确拍严格升序；没有比例字段。offset 须等于 `R(TIME.offsetMs*1000)`；tempo 键恰为 TIME 中所有 tempo 起/止拍去重，Stop 键恰为 TIME Stop 拍。`anchors` 键是以下独立键精确去重的全集：

1. 零拍、tempo 与 Stop 键；SCN1 中所有 noteBeat。
2. BHV1 的全部 key/step 拍和每个 event 的 start/end（包括未绑定定义）。
3. 每条 ANI1 instance 的 start、首轮每个 segment start/end、step 拍及首轮结束；有限 instance 再加总结束。不剔除被 mask、零权重隐藏的已声明边界。不绑定到实例的 clip 本地拍不入全局键集。
4. JPR1 的 end、body 两端、各 phase target（与 GPR0 同一行）。

第 3 项用 `B(k,l)=start+scale*(k*duration+l)` 精确运算，首轮 `k=0`、有限结束 `B(iterations,0)`；溢出或不能构成规范 Beat 在源终结/解码拒绝，不从 TIM1 倒推出声明。不重复收集后续轮；重叠的图键、音符和表现键可以同拍，也可以不同拍同 tick。`TimingMap::fromCompiled` 验证键全集、同拍同值、Beat 0=0、tick 不倒退、Stop `[S,E)` 正宽、互不接触、前拍 `<=S` 后拍 `>=E`。保存的 tick 是离散权威，不重新积分作认证。源生成按 [谱面时间 API](runtime-timing.md#谱面时间-api) 的连续积分、Stop 和 ties-to-even；同一次编译中表现与玩法使用同一次取整。

**动态换算。** 每个循环候选边界用 Timing 的受检 `RationalBeat` 算术求 `B(k,l)`，将该拍传同一个 `TimingMap::beatToTime`。精确锚命中直接取保存值，Stop 拍取 S；缺锚用原连续积分与 R 一次量化。然后在 TIM1 anchor 中按精确拍取相邻的下、上界：左为 Stop 用 E，右为 Stop 用 S，其余用保存 tick，缺一侧则该方向不夹。量化结果夹在闭区间内；两界同 tick 可直接返回。任何受检拍或时间计算失败走 `time_range`，不猜轮次。禁止副本积分器或缓存逐轮状态。存储边界跨平台使用相同 tick；未存的动态拍若接近半 tick，连续积分的跨平台差异可能移动离散转换一 tick。同一运行环境的源和两种解码共享同一实现、数据和整数比较；不承诺跨平台连续几何位等价。

**求值。** 有限 Behavior 通过已存 tick 二分，每次采样无论是否连续经过都得到当前值。相同 tick 多拍按拍升序视为同时完成跨界，后拍生效；同拍相邻 event 从新 event 取值。Step 此前用基线；instantaneous 到 tick 取端值；event 连续曲线在 start/end 间用 `timeToBeat` 供原 Hermite/SLERP，进度夹至 `[0,1]`，分支仍按整数边界。Keyframe 用整数 tick 间的精确差的比例和右键 easing；同 tick 键折到这一批最后一个，避免除零。

Clip 先以起止 tick 判定：未开始无写；有限结束 tick 起 Hold 采样 `duration`，None 无写。活动时找最大的 `k` 满足 `tick(B(k,0))<=sampleTick`。先用 `timeToBeat` 的连续拍估算轮号 `floor((beat−start)/(scale*duration))`，夹到非负和有限次数范围；估算只能用于选择探测起点。从起点向所需方向按 1、2、4… 指数探测，再在整数轮号区间二分，最多 63 次探测和 63 次二分，使用与原 `AnimationSampler` 相同的有符号 i64 轮号范围；每个探测都比较量化后 tick，不能用反查的浮点拍落选/入选。有限上界使用保存的结束边界（该边界不活动）；无限下界为 start。无可表示的上界或相邻验证不满足 `tick(k)<=t<tick(k+1)` 时报告 `time_range`。轮号确定后，每条 track 对内用同一动态换算二分；同 tick 选择最大本地拍。wrap 时下一轮优先，缩到同 tick 的整轮不产生额外写。连续段用 `timeToBeat` 和该轮 start/scale 插值，夹到整数判定选中的 segment；**不得**因恰等普通量化锚 tick 就把连续进度强制改为锚的精确拍。反查只按 TimingMap 的 tempo/Stop 连续规则；空档按 `AnimationSampler::sampleTrack` 的 previous end 规则，Step 未命中无写。Stop `[S,E)` 内保持共享拍进度，恰等 Stop 的切换在 S 生效；E 不重放原切换，后拍若量化到 E 可生效。

这些程序是当前状态的纯求值，不是向宿主逐轮发送触发队列。Gameplay/宿主在 T-10 既有 property 层对本次状态施加交互覆写；成品仅保存所需 FactBinding 和可写目的属性，不记已发生的输入。相同准备内容在同一 tick 和同一输入前缀返回同一分支和离散值；失败保持上一帧。源与成品连续几何对照用相同 sampler，不允许以误差容差掩盖分支错误。

## JPR1 玩法准备行

records=GPR0 行数；整个 payload 是 `ScorePayload,Vec<RequirementPrepared>`，后表与 GPR0 同序等长。枚举 phase=0/1/2/3 为 tap/head/body/tail，outcome=0/1 为 hit/miss；这些是新表的枚举，旧表的 registry 不变。

`RequirementPrepared = Beat end,Opt<{Beat start,Beat end}> body,Vec<{u8 phase,Beat target}> phaseTargets,i64 deadline,Vec<{u8 phase,Opt<u32> gradeTableIndex}> measureTables,u64 expandedPatternNodes,Program program`。拍位是 `CompiledBeatKeys` 的声明，phase targets 及 measure tables 按 phase 序，分别严格匹配该 GPR0 row 的 targets/measure components；后者一行即一个 `CompiledMeasureComponent` 的 v3 table index（`absent` 不补 grade/Exact）。禁止额外 phase 或不引用本表的 grade override；GPD0 Measure row 保留 phase/category，`gradeTokens` 恒空。两 Reader 用本地 index 派生各实例可见 grade key 并验证完整局部计分覆盖。同拍可跨 phase；锚点 tick 对照 GPR0 的 end/body/target，各值须等于 TIM1。无 tail 时 deadline=end；有 tail 时按已解析 grace checked 加到 end，不查源或再做 grace resolution。阶段窗口、资源槽及原规则由 GPR0/GRC0 的运行字段投影；timer keys 从其边界及完整 requirement identity 构建。每条 `PreparedResource` 一对一取同一 GRC0 `ResourceRecord` 的 `resourceId,slotToken,terminalAfterTermination`，索引依据已解析 claim 归组且保留原 GRC0 rank/候选顺序；布尔值原样复制，不从 claim 的存在或竞争键推断。`expandedPatternNodes` 是已有 `CompiledPattern::fullyExpandedCount()` 的值，和实际状态量另计，作为分配前门禁；没有可表示合法计数的源不可写此版本。

`Program = Vec<Str> atomRefs,u32 start,Vec<Bool> accepting,Vec<Bool> live,Vec<u32> transitions`。atomRefs 按 `PatternExecutionProgram` alphabet column 顺序，token 唯一且消费该 Requirement 已绑定 atom；两个 flag 向量等长、states>0；start<states；转移为 state-major，列数=atomRefs.size()，长度严格为 `checked(states*cols)`，`UINT32_MAX` 表示无边，其余 `<states`。建 reverse reachability 核对 live 恰达接受状态（接受状态经零条边可达）。Reader 不以定义重建自动机；合法未用 Pattern/Measure 仍保留在 `GPD0`。

`ScorePayload` 按 [归一化计分与 Prepared 记录](runtime-timing.md#归一化计分与-prepared-记录)的 `PreparedNormalizedScore` 投影：`Mode mode,Vec<GradeTable> gradeTables,Vec<Unit> units,u64 jMax,u64 cMax`。各行明确定义如下；只验证结构、局部资格及静态上界，不包含输入见证、可达性证明或回放。

| Typed 行 | Wire 顺序 |
| --- | --- |
| `ScoreMode`（Mode） | `Str id,u32 total,u32 mj,u32 mc,u8 onExhausted,Vec<ComboRewardSegment> comboRewards,Vec<ScoreRuleRow> rules`；`onExhausted` 只准 `ComboAction` 0=keep、2=reset。 |
| `ComboRewardSegment` | `u32 from,u64 rewardAtFrom,u32 slope`。 |
| `ScoreRuleRow` | `u8 phase,u8 outcome,Opt<Str> grade,Bool qualifies,u64 judgePoints,u8 combo`；`ComboAction` 0=keep、1=increment、2=reset。 |
| `PreparedGradeTable`（GradeTable） | `Str id,i64 exactStart,i64 exactEnd,Vec<PreparedGradeInterval> intervals`。 |
| `PreparedGradeInterval` | `i64 start,i64 end,Str grade`；相对 tick 半开。 |
| `ScoreUnit`（Unit） | `Vec<u32> phaseIndices`。 |
| `ScoreBasis` | `u64 jMax,u64 cMax`，紧随 units，各一次。 |

表按 id、interval 按 start；rules、units 及 phase index 展开严格用上述合同的 canonical 顺序。完整保存合法未用计分表/规则；选表 index 指该次同一 `ScorePayload.gradeTables`，有表的非空 Exact 用已保存 `[exactStart,exactEnd)`，没有就 absent，不补 0。基数用同一 `deriveScoreBasis(requirements,latePolicy,normalizedScore)` 从未终结 records 计算；源填写、Reader 核对。每 unit 至少有一条局部合格键，全不合格的虚假 unit 即使不改 `jMax/cMax` 也以 `score_unit_unqualified` 定位 `/JPR1/score/units/i` 拒绝；单独校验资格，禁止暗清 combo。`scoreDigest=SHA-256(ScorePayload 完整字节)`，不含 JPR1 requirement 表；选表绑定同样纳入 compiled digest。

## 资源闭包与完整身份

`RES1` records=资产数，顺序 `Vec<Use> uses,Vec<Asset> assets,Vec<Binding> bindings`：

| 行 | Wire 顺序 |
| --- | --- |
| `Use` | `Str assetId,u8 use`；0=mainMusic、1=mesh、2=静态 material、3=Behavior 材料 Step、4=Clip 材料 Step。按 `(assetId,use)` 严格升序。 |
| `Asset` | `Str id,u8 type,Str rootId,Str logicalSource,Vec<Str> dependencies,u64 byteCount,Digest sha256`；type 0/1/2/3/4=Mesh/Material/Texture/Audio/Shader（`project::AssetType`）。path 为 root-relative portable path，不能绕出 root；digest raw 32 bytes。 |
| `Binding` | `Str id,u32 requirement,u8 phase,u8 outcome,u8 timing,EIx target,Bool visible,i64 start,Opt<i64> end,u8 aggregation,Vec<Str> groupMembers`；requirement 为 GPR0 ordinal；timing=0/1/2/3 为 Any/Early/Exact/Late；aggregation=0/1/2 为 Any/All/GroupCommit。 |

从 META mainMusic、全部实体 Renderable、所有 BHV1/ANI1 `render.material` 值（含未绑定定义）独立求 `Use`，必须与本表相等；资产从该集合递归追踪依赖得到恰好可达集合，按 id UTF-8 严格升序；dependencies 按 id 升序唯一，禁止断边、环、重复 logical locator、角色/类型不符或以其它类型自动降级。跨根依赖仍按 Asset Index 唯一 id；使用 [CXC §5](../../../../formats/CXC_FORMAT.md#5-project-与资源闭包) 的路径/Asset Index 合同。资源文件收在包/外部 provider；图、payload 和源内不藏资源副本。纯内存 Reader 验证记录闭包但不声称 provider bytes 已验证；Playback 接每条 descriptor 对照 provider 并在交付候选前核对 byteCount/sha256；CXC 使用已验证 manifest 对照，无需第二次解压。资源不足、错误 asset 类型或内容交换不能变成 fallback 资源。旧 `chart.resourceClosure` 只含静态 use 0–2 的旧 projection；禁止把新的 3/4 塞进旧 enum。

`engine/gameplay_packed/include/cuexis/gameplay_packed/prepared_binding_rows.hpp` 定义 owning DTO，read/source 共用，无 Playback include 或链接边：

```cpp
struct PreparedBindingRow final {
    std::string id;
    std::uint32_t requirement;
    std::uint8_t phase, outcome, timing;
    std::uint32_t target;
    bool visible;
    std::int64_t start;
    std::optional<std::int64_t> end;
    std::uint8_t aggregation;
    std::vector<std::string> groupMembers;
};
```

v3 源 `gameplay.common.presentationBindings` 可省略，缺省空；每个对象完整采用 `engine/playback/src/gameplay_configuration.cpp::fields(GameplayFactBinding&)` / `enumNames` 的 `bindingId,source,phase,outcome,timing,target,visible,start,end,aggregation,groupMembers` 现有 JSON 形状和 token（end absent=`null`），拒绝未知/缺项。Source TU 独立降为 primitive DTO；`source` 必须解析到唯一 full GPR0 RequirementIdentity，`target` 到唯一场景稳定标识（显式 `ChartObjectId` 或原 full-E runtime projection，禁止 EnTT ID）。候选只接谱面声明的 binding。

把 `gameplay_candidate.cpp` 的组团、alias、区间及潜在冲突纯检查移植为 `engine/gameplay_packed/src/compiled_binding_validation.cpp::validatePreparedBindings(std::span<const PreparedBindingRow>, std::span<const PreparedBindingSource>, std::span<const std::uint8_t>) -> core::Result<void>`，编入 Reader。`PreparedBindingSource` 是同一头中的 build-only view `{uint8_t phaseMask,gradedPhaseMask; span<const string> bindingRefs;}`，和 GPR0 同序；最后一个 span 按 EIx，0/1 表示 Renderable present。函数从 `bindingRefs` 反查所需 binding IDs 的并集，调用方另验证它等于 GRC0 FactBinding IDs；索引和对应 phase 必须有效、被引用 requirement 声明该 id、目标须有 Renderable。非 Any timing 要求对应 `gradedPhaseMask` 位（该 phase 选了含 Exact 的表）；无需模仿旧 config 默认。相同 id 的 aliases 须一致于组、aggregation，分组时还须一致于 requirement/phase/outcome/timing；成员必须包含自身，各成员的组回指完整，空成员仅准 Any aggregation。异值同 target、区间重叠且 predicates 可同时成立的行冲突；同源同 phase 不同 outcome，或两个不等且非 Any 的 timing 互斥。`end` 存在须 `start<end`，absence 表 UntilReset。允许不同 alias；完全重复非法。绑定按 wire 字段顺序用数值、UTF-8、`absent < present` 和向量字典序逐项比较，groupMembers 排序去重，Reader 严格验序。Legacy `gameplay_candidate.cpp` 仍走原 guard/类型及接受集。

Playback 只从已验证 DTO 映射到现有 `GameplayFactBinding`（full RequirementIdentity、full E 稳定目标和原 presentation tick 值），资源/源 metadata 投影也取这些行。后继 `presentation_projection.cpp` 用同次 `SettlementEvent.direction` 映射 Early/Exact/Late；既含 Any 映射也含实际 direction，方向缺省仅匹配 Any。该方向由作者选表的半开 `[exactStart,exactEnd)` 决定，包括带误差的 Miss，禁止把 `error==0` 或是否命中 grade 当作 Exact。旧投影继续按源代码中的 `<0/==0/>0` 路径。Property 层的冲突/优先级和组团提交沿原运行政策，无逐帧快照。

`compiledDigest=SHA-256(ASCII "cuexis.compiled-gameplay.v3\n" + StaticGraph + TaggedPayloads)`；prefix 末尾 LF、无 NUL。`StaticGraph` 是源码 `gameplay_capsule_emit.cpp::preimage` 的 `gp::staticPreimage(chartProjection,owners,4)`，紧接 `gameplay_capsule_operations_internal.hpp::semanticRows` 的 hash-mode traversal；第一个旧 prefix 保留在该子流内，GPH0 和 grade tokens 用本版值，复用 [Capsule §13](../../../../formats/GAMEPLAY_CAPSULE_V2_FORMAT.md#13-capsule-物理-revision-4-与-accepted-hash-work) 的扩展 Ix、full E/K4/claims 编码，不调用旧 Model 派生。`TaggedPayloads` 严格依 `ANI1,BHV1,JPR1,RES1,SCN1,TIM1` 各写四个 ASCII tag、`u64 payloadByteCount`、完整 canonical payload，之后追加同 framing 的 `SRC1`，其 payload 为 `Str compilerProfile,Vec<Str> sourceDocumentIds`（直接取 GPH0，IDs 字节升序唯一），弥补旧 hash-mode visitor 省略 provenance；`SRC1` 仅为 digest frame，不是目录 section 或 Graph 键。GPH0 的 resolver 槽仍为旧常量 `grace.resolver.s2.v1`，只说明已解析数据所属合同，不驱动 Reader。派生 counts 必须等于实际行数，但不单独入 preimage。不 hash padding、物理字典索引、目录/CRC、JSON 转义或外部 caller 配置。动态 ordinal 与 canonical E/GPR/definition 行闭合。先有界计数再增量 hash。两 codec 的 digest 和 CXC `compiledSemanticIdentity` 相同，`artifactIdentity` 是各文件 exact bytes 的 SHA-256；文件内 digest 和调用者提供的 expected digest 分别校验。

### 身份终结

`engine/gameplay_packed/src/compiled_artifact_finalize.cpp` 编入 Reader，内部 `finalizeCompiledArtifact(CompiledArtifactRows&&, const CompiledDecodeContext&, CompiledArtifactLimits, optional<Digest> carrierDigest) -> core::Result<CompiledCapsule>`。`CompiledArtifactRows` 在 `engine/gameplay_packed/include/cuexis/gameplay_packed/detail/compiled_artifact_rows.hpp`，拥有 `chart::CanonicalSemanticChart chart; PresentationRows presentation; chart::CompiledChartTiming timing; judgement::BeatGameplayProduct gameplay; vector<RequirementOwner> owners; ResourceRows resources;`。Reader/Source/Writer 共享同一个 canonical visitor；两个 Reader 解码得到同一组未终结 owning rows，Source 调 T-12 得 `BeatGameplayProduct` 再汇集其它行，`carrierDigest` 只由 Reader 提供。所有失败统一丢弃临时数据。

1. Source 将规范 `timing::TimingSourceView` 借入 `BeatGameplaySourceView.timing`；同一 view 值构造 map。`compileBeatGameplay` 首先以 `map.matchesSource(view.timing)` 对照 default BPM、tempo/stop 的拍和数值、数量及次序（沿 [Timing API](runtime-timing.md#谱面时间-api) 的 −0 规则）。不能只比 anchors 或由宿主提供字符串；同一组 anchor 键即使 tick 恰也相同，BPM 不同仍以 `timing_source_mismatch` 拒绝。玩法边界只查独立声明在 map 已存的锚。作者 `GameplayCompileContext` 从 `admission` 投影 `capabilities/contentLimits/prepareLimits`，另收 `patternBudget`；不需预先提供 content/prepared identity、host mapping 或 calibration。
2. finalize 先验资源、表、边界、交叉关系和 admitted capability/limits，并用上面的 `deriveScoreBasis` 验证声明基数。用[本节 preimage](#资源闭包与完整身份)有界地散列**未终结** rows，JPR1 直接从 `BeatGameplayProduct` 取；先比 `carrierDigest` 再比 `expectedCompiledDigest`（若存在）。`contentIdentity=lowerHex(compiledDigest)`，`PreparedTimebase={"engine.tick.us.v1",1,1,Tick{0}}`。只从已验证 typed 内容派生，不反查源、宿主或 `PreparedGameplay`。
3. `PreparedIdentityDeclarations` 是 host 提供的 engine/rules interface/session 声明；沿已有运行准入检查，凡有内容镜像的 normalization/execution/late tokens 必须匹配 GPH0，不能改写任何 payload。static prepared 身份由 Judgement 的单一纯值 builder 生成：沿 `gameplay_assembler_assemble.cpp::IdentityByteWriter/writePreparedPrefix/writePreparedSession` 的编码和字段顺序，签名改 `cuexis.judgement.identity.prepared.beat-us.v1`，从 prefix 的 `chart` component 之后用 `writeText(contentIdentity)` 取代旧 graph projection，接 session component。`preparedIdentity=lowerHex(SHA-256(该字节流))`。两路只调用相同 builder；旧 identity 生成保持原 prefix/projection。`makeRuntimePreparedIdentity` 的 v3 分支沿同一 builder 以 `writeExecutionSession` 顺序追加 mapping、late admission、calibration，再求该局运行身份；这三个 host 值不进成品、content/score digest 或离线 static prepared。Reader 只需 static 声明，可在无设备/Playback 下准备。
4. 以最终 identity、固定 timebase、无 legacy ruleset 和同一未终结运行 rows 构造 `CompiledGameplayView`，调用一次 `prepareCompiledGameplay` 建拥有型运行表。此处做该运行入口的最终校验（包括静态基数防线）；成功才构造 `CompiledCapsule`，失败无任何可发布值。Writer 重用同一个 visitor 核对持有身份并输出，不从 session/Fold、回放或作者文件派生 rows。identity 声明或硬件变更不能导致同一 artifact 字节改变。

## CXC 条目、源包及派发

保留 ZIP32、manifest v1 `entries[{path,byteCount,sha256}]`、project、Asset Index 与路径规则。新增候选 project extension `extensions["cuexis.gameplay-entry.v2"]={"entries":[...]}`；对应 manifest `requiredExtensions` 须恰有一条 `{"id":"cuexis.gameplay-entry.v2","version":2}`，二者有无须一致。它只在具名后继 CXC 入口启用，默认入口不从未知 extension 自动采用 v3；既有 `cuexis.chart-entry.v1` 和 `cuexis.gameplay-entry.v1` 原形状、接受集不改。v2 `entries` 非空、至多 manifest `maxEntries`，按 exact portable path 字节序严格递增；同路径不能再被另一 entry 扩展声明，大小写冲突沿 manifest 路径规则。所有下表 JSON 对象为闭集，缺项、重复键、未知键拒绝。

| Carrier | `entryKind` / `encoding` | Required entry fields |
| --- | --- | --- |
| 源 | `chart` / `source-json` | `format:"cuexis.gameplay-entry"`, `version:2`, `path`, `playback:true`, `entryKind`, `encoding`, `compilerProfile:"gameplay.author.beat-us.v1"`, `artifactIdentity`。不带 compiled digest、计数、旁置 ruleset、成品路径或其它字段。 |
| Packed | `packed-chart` / `capsule.5` | 上述公共字段加 `expandedEntityCount,expandedRequirementCount,compiledSemanticIdentity,rulesetBinding,capabilityClosure,resourcePresentationClosure,sourceOf`，定义如下。 |
| Graph | `gameplay-graph` / `graph.2` | 同 Packed，必须使用 Graph2 的双 revision 和有界 decoder。 |

每行 `path` 所指文件必须在同一 archive 与 manifest 恰好出现一次，`artifactIdentity` 是本行**实际文件**（源行为 JSON bytes）的 lowercase SHA-256 且等于 manifest entry `sha256`。`project.entry.chart` 仍为必须存在的 bootstrap locator：若其解析路径命中本扩展条目，候选容器按该行类型检查（成品 bootstrap 不假当 JSON 读取）；若未命中则按现行 Chart/CXC bootstrap 合同。后继 v2 候选始终必须显式给 exact portable path（Player 用 `--entry PATH`），即使仅一行、路径等于 bootstrap、或请求普通模式也不得自动选取；普通自动默认仅适用于原合同认可的普通入口。独立选择的多张谱及多载体可以共存，各行不引用其他 carrier；source-only 可无成品文件/行；compiled-only 可以用成品作 bootstrap，无需伪造源。没有合法 bootstrap 或实际引用缺失是容器错误。

**编译条目的新增字段。** `expandedEntityCount/expandedRequirementCount` 是 owning 数据中实际 ENT0/GPR0 行数（无符号整数，不超 Packed 上限）；`compiledSemanticIdentity` 必须匹配已验 digest。`rulesetBinding={modeId,scoreDigest}`，前者等于 JPR1 `ScoreMode.id` 并核对 GPH0 镜像槽，后者是 JPR1 的 `scoreDigest` lower hex；两者都不得由宿主补齐。`capabilityClosure={declared,derived,requiredFeatures}` 使用现有 `gameplay_entry.cpp::gameplayEntryMetadata` 的 `[capabilityId,revision]` arrays 及 feature IDs，从 GPH0/graph derived fields 取值按对应 typed order。`resourcePresentationClosure={gameplayResources,chartResources,assets,bindings}`；`gameplayResources` 为 GRC0 resource IDs，`chartResources` 为 RES1 uses 的 `[assetId,use]`，`assets` 从 RES1 逐项投影 `{id,type,rootId,logicalSource,dependencies,byteCount,sha256}`（SHA 小写 hex），`bindings` 按 RES1 顺序投影原 `GameplayFactBinding` JSON 字段（同 §资源闭包）。`sourceOf={kind:"not-packaged",documentIds}`；`documentIds` 精确等于 GPH0 的 source closure IDs，表示**本条无源文件依赖**，即使包里另有源条目也不能从逻辑 IDs 推断路径。四个 closure 数组不得删、重排或改值；CXC 元数据平行校验同一 owning records，不能替代 RES1。

**检查与能力分离。** 候选 CXC 容器复用现有 ZIP/CRC/hash/限额、资产索引、portable path 和 reachability 检查。在 `cxc_package.cpp` 校验 manifest/project 后、现有 bootstrap `ChartLoader::load` 之前识别并登记 v2 entry directory，核对 manifest required extension 和 project 声明；若 bootstrap 命中已登记 v2 `path`，按该条的 v2 encoding 检查，绝不先把它交给旧 Chart JSON bootstrap parser。只有 bootstrap 未命中时才走旧 bootstrap，仍检查 v2 目录所有条目。此预分派也用于 CXC Writer 的发布前读回。

对所有 v2 条目做结构、实际 entry/hash、source documents 与资源闭包校验：源条目严格读取 Chart v5/gameplay v3 的 header/schema，遍历 `animationTemplateImports`、`cxtImports` 的实际 CXT v1/v2 文档及 Asset Index。使用 Chart 的有界 v5 解析及 typed source-side 对象/参数检查、schema 形状及资源清单，不构造 TimingMap、执行表或运行局；不从 Payload 猜另一代 schema。Bootstrap 以外的每个源条目也须验证；资源路径和 CXT import 都加入 reachable。Compiled 条目在此步校验 metadata 的资产路径、type、依赖、字节数及 SHA 与 manifest/Asset Index 相符，并确认 envelope 派发字段匹配；CXC 不链接 Gameplay Reader 或 source target，metadata 与实际成品语义的相等性由选定后的 Reader 证明。必需存在的文件仍受 manifest unlisted/unreachable 检查；上下游只认可验证过的包快照。

候选 Playback 显式选择后，先匹配 entry 和 artifact hash，再核对声明与 header（`capsule.5` rev5、`graph.2` 两版本、`source-json` Chart v5/gameplay v3）。合法包但所选源编码未启用，SDK 以 capability 拒绝，参考 Player 按[启动合同](render-player-writer.md#命令与启动)投影为 `player.source.encoding_unsupported`；不报告包损坏，不尝试其它路径或 codec。启用的源调用 `compilePreparedArtifact`；所选成品调用对应 `decodeCompiled*`，传入 expected digest，核对计数及模式、能力、资源和溯源闭包。内容/身份校验通过才查 Player 模式/设备发行支持行。坏头、引用缺失或 hash 冲突始终先于编码拒绝。容器校验不代替装载时源编译诊断。旧 rev1–4/Graph1、普通 Chart/CXC、旧 Behavior source candidate 保持原派发和旧静态编码；向不能承载新语义的旧载体输出以 `unsupported_feature` 拒绝。

### Schema、工具与接线

工具目标由 T-10 先交付：`cuexis_gameplay_author_cli`（alias `cuexis::gameplay_author_cli`）拥有现有 `src/assembler_cli.cpp` 与其旧 metadata helper 接线，`cuexis_gameplay_assemble` 和 candidate 分支的 `cuexis_chart_candidate` 已改链该目标。T-13 消费此拆分，在 `cuexis_gameplay_author` 增加 `PUBLIC cuexis::gameplay_packed_source`、对应根 `ALLOWED`，并在已有 CLI 追加下述分支。`gameplay_packed_read/source` 同卡在原 `engine/gameplay_packed/CMakeLists.txt` 创建，工具目录仍在 engine 之后，源库无 Playback 边。

T-13 新增 `schemas/candidate/cuexis.gameplay-entry.v2.schema.json`（本节三种互斥条目及 directory、`additionalProperties:false`）与 `schemas/candidate/cuexis.gameplay-graph.v2.schema.json`（前九键复用 Graph1 typed shape，调整常量、后六个 hex 必填）。SAX 另验顺序、解码字符、计数和跨表关系。Manifest v1 shape 不改；新增 requiredExtension 的 ID/version 注册和候选 package loader 派发。T-11 首创候选 v5 作者基础壳 `schemas/candidate/cuexis.chart.beat-us.v5.schema.json`，引用同卡创建的 `schemas/candidate/cuexis.gameplay-author.beat-us.v3.schema.json`；T-13 扩展 `presentationBindings` 并接壳到成品 source adapter。CXT v2 的候选 v3 包络由 T-13 创建 `schemas/candidate/cuexis.animation-template.beat-us.v2.schema.json` 引用同一玩法 payload，旧 CXT v2 schema 接受集不改。T-12 仍拥有玩法/时间 typed 编译入口。v5 Chart schema 与 loader 只由显式后继路径选用；普通自动默认不选后继目录中的条目。

`tools/gameplay/gameplay_assembler/src/assembler_cli.cpp` 给现有 CMake 可执行目标 `cuexis_gameplay_assemble` 加显式 `beat-us` 首 token 分支，拼写：`beat-us --base BASE.cxc --entry PATH --output OUTPUT --publish cxc|filesystem --encoding capsule.5|graph.2|both --test-only true [--graph-budget B]`。每项一次且有值，拒绝未知/重复/旧 `--configuration` 等选项。BASE 必须通过候选容器校验、指定 v2 `source-json` 条目；PATH 无自动选择。`--graph-budget` 用现有七项严格整数口径，只在 `graph.2|both` 必填，其余拒绝。编译 context 用显式 test fixture 的原批准能力和预算输入，不读旧计分配置；缺适用门禁即拒绝。新分支调用 `compilePreparedArtifact` 一次，按 `--encoding` 写 `<selected-path>.capsule.5` 和/或 `<selected-path>.graph.2`；尾部固定编码即使原路径带扩展名也不替换。输出布局：filesystem 把这些相对文件名和 `cuexis.gameplay-entry.v2.json`（directory 对象，仅所写编码、按 path 升序）交现有 multi-artifact 发布；CXC 复制验证过的 BASE，追加新文件和对应项目目录 rows，重算项目/manifest entries hash，保留原 bootstrap 和源条目，重新通过容器校验再原子发布。路径碰撞、超路径、策略失败均拒绝；compiled-only 测试用新目录/manifest Writer 接线，以已选成品为 bootstrap，不要求这个 CLI 删源。单独 `--help` 按 [P5-06](render-player-writer.md#工具外部行为) 直接输出；除此以外，无 `beat-us` 的命令仍走旧解析和输出。T-13 同卡接 `tools/chart/chart_candidate/main.cpp`、`tools/cxc/cxc_pack/main.cpp` 的单独帮助入口；三具名工具的原成功/错误分类保持。

## 限额与稳定拒绝

使用请求与冻结值逐字段取小值（0 是实际限额）。Packed 沿 `PackedChartLimits` 的 file/decoded/section/strings/references/entities/requirements；Graph 沿显式 `GraphReaderLimits/GraphWriterLimits`，仍只接受 `testOnly` 预算，新增 hex 独立检查每段 decoded+总 decoded，不能靠 Graph 放宽逻辑计数。对新表 `Str` 单项用对应 `ChartLimits.maxIdentifierBytes/maxStringBytes` 或 CXC path/Asset Index 限额，累计解码字节按 section/aggregate 限额。旧 STR0/REF0 计数不伪报新增字段；所有新索引/引用另外逐项有界。

`CompiledResourceLimits` 在 `compiled_capsule.hpp` 为纯值 `{uint64_t maxEntryBytes,maxListedBytes; size_t maxDependencyDepth,maxAssets,maxDependenciesPerAsset,maxAssetIdBytes,maxPortablePathBytes;}`，由调用者填有效有限值：CXC `maxEntryBytes=min(CxcPackageLimits.maxEntryBytes,manifest.maxEntryBytes)`，`maxListedBytes=min(CxcPackageLimits.maxPackageBytes,manifest.maxListedBytes)`，`maxDependencyDepth=CxcPackageLimits.maxDependencyDepth`；Asset Index 的三个 assets/dependencies/id 限额取 `CxcPackageLimits.assetIndex` 同名字段，`maxPortablePathBytes=min(CxcPackageLimits.maxPathBytes,assetIndex.maxPortablePathBytes)`。裸读/写或工程用同一组现有默认作为 ceiling 后可逐项收紧；CXC 宿主用实参及同一 ceiling；总 bytes 按唯一资产行求和。Reader 不链接 CXC/Project；绑定值不进 wire，调用者不能靠放宽入参扩容已冻结运行配置。

`maxPropertyWritesPerFrame` 显式由调用方从 `world::maxPropertyWritesPerFrame` 与宿主实际 PropertyWriteBuffer 容量（可再有请求 tightening）取最小值；工具和独立消费者取同一 world ceiling 或更小的显式预算。Reader 不含 World 头：对非空 ANI1 用 `checked(programCount * propertyCount)`（现 `world::requiredAnimationWrites` 的 10 种属性）比较该字段；随运行时原保守准备门禁，行为 track 写数另按 `ChartWorldInstantiator::validate` 的受检加总与同一 cap 比较。测试固定 cap−1/cap 的拒绝/接受，runtime 仍额外检查合成当帧实际写入。

| 表 | 字段计数及附加门禁 |
| --- | --- |
| SCN1 | entities 与 Packed 上限及 `ChartLimits.maxObjects` 同取小，画幅按 [作者画幅](render-player-writer.md#作者画幅)。 |
| BHV1 | `maxBehaviors`、bindings `maxObjects`；key tracks 用 `maxTracksPerBehavior/maxKeysPerTrack/maxTotalBehaviorKeys`；event+step 合计用 `maxEventsPerBehavior/maxTotalBehaviorEvents`。 |
| ANI1 | `maxAnimationClips/maxObjects/maxAnimationTracksPerClip/maxAnimationSegmentsOrStepsPerTrack/maxAnimationLayersPerAnimator/maxBlendGroupsPerLayer/maxClipInstancesPerBlendGroup/maxAnimationTracks/maxAnimationSegmentsAndSteps/maxGeneratedAnimationRecords` 与 `AnimationCompiler::validateCompiledBudgets` 的展开计数和 `world::animationWriteBudgetFits`。不按循环次数展开。 |
| TIM1 | tempo/stop 的来源各过 `maxTempoEvents/maxStops`；tempo keys `<=2*tempoEvents`；anchor count 上限为 §共享时间列出的独立候选数 checked sum 与实际 section 剩余字节之 min；Reader 键验证使用按拍索引或排序去重，不乘每帧次数。 |
| JPR1 | 行数与 GPR0 严格一致，表与单元按已冻结源 `ContentProfileLimits` 和 `PrepareGameplayLimits`；phase 项每 requirement 最多 4，unit index 总数不超展开 phase 数。Vec states/expanded-count 在分配前执行已认证 profile 限额。 |
| RES1 | uses/绑定引用用 Packed `maxPackedReferences`（含别名及组成员引用分别累计）；资产用 Asset Index `maxAssets/maxDependenciesPerAsset`、CXC `maxEntryBytes/maxListedBytes/maxDependencyDepth`。源与两 codec 同口径。 |

所有 `count*minRowBytes`、偏移/长度/聚合和 Graph 扩展倍数先受检，能由现有剩余 bytes/atoms 证明超限即拒绝；验证索引和有界 scratch 后才 reserve/copy。Hash 以 [Capsule §13](../../../../formats/GAMEPLAY_CAPSULE_V2_FORMAT.md#13-capsule-物理-revision-4-与-accepted-hash-work) 的四维上限对完整新流计数，新增 payload 长度也进入 preimage work；每个 ordinal/lookup 和控制可达验证的索引访问进入相应检查；哈希前禁止任意规模重建。这些上限不构成发布实时准入。runtime 候选保留现有 per-frame Property Write 容量门禁；超限回滚当帧。

新 codec 返回有界 `core::Error`，`severity=error,faulted=false`；保留原稳定顶层 code，并附 `reason`、ASCII `fieldPath`、`encoding`。Reader 从零编号，Graph hex 的数据错误仍用同一段名路径（Graph root 错误用实际 `/scenePayload` 等）。容器原 ZIP/path/CRC 错误保留原 code。优先级按下表；同阶段按目录/根序和行号，缺锚按拍升序，原 Stop 优先级仍由 TimingMap 给出。任何安全上限可在必须分配前提前拒绝；预算内 OOM 的 reason 单列。

| 次序 | code / category | reason 与定位 |
| --- | --- | --- |
| 1 | `packed.header.unsupported_revision` / `graph.header.unsupported_revision`；类型不符外包 `playback.gameplay.invalid` | `revision_mismatch`，`/header/candidateRevision`、`/graphFormatRevision`、`/capsuleRevision` 或 `/entry/encoding`。原 envelope magic/CRC 码优先。 |
| 2 | 二进制 `packed.requirements.invalid`、Graph `graph.structure.invalid` / `invalid_schema` | `section_missing`、`section_unknown`、`section_duplicate`、`length_invalid`、`row_order`、`invalid_enum`、`scalar_invalid`、`framing_invalid`；`/directory/<TAG>`、`/<TAG>/<table>/<i>/<field>`，重复画幅指后项。Graph duplicate key 沿 `graph.structure.duplicate_key`。 |
| 2a | `capability.budget_insufficient` / `budget_exceeded` | `limit_exceeded` 或 `allocation_failed`，指最早所需空间；另附 budgetId/limit/requiredAtLeast，分配失败不伪造 required。 |
| 2b | `chart.v5.behavior_candidate.invalid` 或 JSON 原语法码（后继源） / `invalid_schema` | `revision_mismatch`、`source_shape`、`source_authority_conflict`；指实际根、CXT 路径或首个冲突字段。 |
| 3 | `judgement.execution.relation_invalid` / `invalid_relation` | `dangling_reference`、`binding_conflict`、`asset_type`、`asset_closure`、`timing_source_mismatch`、`anchor_missing`、`anchor_unexpected`、`anchor_order`、`phase_timing_mismatch`、`program_invalid`、`basis_mismatch`、`score_unit_unqualified`，例如 `/timing`（错配 map）、`/TIM1/anchors`（缺锚再附 `beat`、`relatedFieldPath` 指独立声明）、`/JPR1/rows/i/program/transitions/j`。Stop 碰撞/范围用原专码。运行边界不可表示给 `time_range`，定位到实际定义/实例并拒绝该帧。 |
| 4 | `packed.identity.mismatch` / `graph.identity.mismatch` | `compiled_digest_mismatch`，`/header/semanticIdentity`、`/semanticIdentity` 或 `/entry/compiledSemanticIdentity`。错误 digest 不能覆盖结构失败。 |
| 容器 | `cxc.project.invalid`、`cxc.entry.missing`、`cxc.entry.hash_mismatch`、`cxc.entry.unlisted` | `entry_shape` 指项目 extension row；`missing_entry` 指缺文件的 path/asset/import；`artifact_digest_mismatch` 指实际 hash 字段；`unreachable_entry` 指 manifest row。先整包物理，再目录 shape/path，再文件闭包，最后 Player 选择。 |
| 选择 | `playback.gameplay.invalid` 或 `capability.permanently_unsupported` / `capability_disabled` | `entry_required`（后继路径缺省）、`entry_not_declared`（显式 `/entry/path`）、`closure_mismatch`（metadata 的首个不符字段）、`unsupported_feature`（`/entry/encoding` 或未获目标承载能力的源字段）。对照 Player 分别投影 `player.source.entry_required`、`player.source.entry_not_found` 及 `player.source.encoding_unsupported`（仅未启用的源编码）；包/内容损坏保留原诊断。 |

源/成品任何阶段失败都不更改已发布内容、时钟、会话、GPU 资源 token 或磁盘已有输出。离线 writer 验证完整 owning 内容后才按旧 atomic 文件发布计划写出；两种 codec 或同包任一产物/metadata 失败均保留原包。

## 验收向量

### 完整作者夹具

以下构造均从空根或明确的已检入 JSON 子树开始。`B(n,d)` 表示 JSON `{ "numerator":n,"denominator":d }`，`asset(id)` 表示 `{ "domain":"asset","id":id }`；这两个只是 fixture 生成器记法，成品源不包含宏。按下列赋值后直接把该根序列化，不拼旧外壳。

**GP-01（也是 PA-02 的底稿）。** 唯一根如下；最后把 `gameplay:null` 替换为下面生成的对象：

```json
{
  "format":"cuexis.chart", "version":5,
  "chartId":"019a0000-0000-7000-8000-000000000001", "metadata":{"title":"GP-01"},
  "timing":{"offsetMs":0,"defaultBpm":120,"tempoEvents":[],"stops":[]},
  "parameters":[], "templates":[], "behaviors":[], "animationTemplateImports":[],
  "animationClips":[],
  "objects":[{
    "id":"019a0000-0000-7000-8000-000000000002", "parent":null,
    "components":{"cuexis.transform":{"version":1,"position":[0,0,0],"rotation":[0,0,0,1],"scale":[1,1,1]}},
    "extensions":{}
  }],
  "requiredExtensions":[{"id":"cuexis.gameplay.v3","version":3}], "extensions":{},
  "gameplay":null
}
```

复制 [inline_prototype.json](../../../../../tests/fixtures/gameplay_author/inline_prototype.json) 的**仅 `/gameplay`**：改 `version=3,authorProfile="gameplay.author.beat-us.v1",common.graphRevision="3"`，移除 `common.timebase/common.rulesetRef`；清空 `common.factBindings` 和唯一 requirement 的 `factBindingRefs`，用于无资源/无绑定版。对 `requirements[0].identity` 与 `rankAssignment.rows[0].identity` 同时设置 `invocationId` 为上面对象 UUID、`moduleId="__chart__",exportId="__object__"`，保留 `chartEntryId/emissionPath/requirementLocalId`。替换 `requirements[0].timing` 为 `{endBeat:B(21,10),phaseTargets:[{phase:"tap",beat:B(2,1)}],successWindows:[{phase:{kind:"tap",declarationOrdinal:1},startMs:-50,endMs:50}]}`；`noteType="tap"`。保留 source declarations、Pattern、Measure（`gradeTokens:[]`）、grace、资源和其它非差量字段。赋值：

```json
{
  "gradeTables":[{"id":"main","exact":{"startMs":-10,"endMs":15},"intervals":[
    {"grade":"Good","startMs":-50,"endMs":-10},
    {"grade":"Great","startMs":-10,"endMs":10},
    {"grade":"Good","startMs":10,"endMs":50}]}],
  "noteTypes":[{"id":"tap","gradeTables":[{"phase":"tap","gradeTable":"main"}]}],
  "scoreMode":{"id":"standard","total":100,"mj":90,"mc":10,"onExhausted":"reset",
    "comboRewards":[{"from":1,"rewardAtFrom":1,"slope":1}],"rules":[
      {"phase":"tap","outcome":"hit","grade":"Good","qualifies":true,"judgePoints":1,"combo":"increment"},
      {"phase":"tap","outcome":"hit","grade":"Great","qualifies":true,"judgePoints":2,"combo":"increment"},
      {"phase":"tap","outcome":"miss","grade":null,"qualifies":false,"judgePoints":0,"combo":"keep"}]}
}
```

上段三字段插入 `gameplay.common`，其它字段保留。用显式无音频离线 fixture 与原 test-only 能力、测量值，核对 [GP-01](runtime-timing.md#验收向量与任务前置) 的窗口、计分、Exact 两端及 miss。

**双 Tap 共用具名 Measure。** 从 GP-01 复制第一个对象、requirement、rank、grace 和 stable declaration 创建第二例：对象 UUID 尾部改 `0003` 并同步其 K4 `invocationId`，`requirementLocalId="requirement.two"`，stable ID 为 `doc.alpha/5`，同步 grace 引用，声明 `localName="requirement.1"`，rank `tieRank="11"` 并同步完整 K4。将两条 `kind:"requirement"` 声明（`doc.alpha/4` 和 `doc.alpha/5`）各自的 `references` 设为 `[{"scope":"sameDocument","declarationOrdinal":101}]`，都解析到 `doc.alpha/101` 的 `unused.measure`。两个实例的 `measure` 均复制该具名定义的 `declaration`，保留空 `gradeTokens`。加 GP-01 的 `five` 表及其模式规则，`five.exact` 复制 `main.exact`，只有第二实例设置 `gradeTables:[{phase:"tap",gradeTable:"five"}]`，第一实例仍从 `noteType` 选 `main`。

第一 Tap 保持目标 `1000000`、窗口 `[950000,1050000)`；第二 Tap 用 `phaseTargets[0].beat=B(3,1)`、`endBeat=B(31,10)`，相对窗不变，得到目标 `1500000`、窗口 `[1450000,1550000)`。每条路径在同一 run 的同一输入通道按序送 `press@970000,release@1100000,press@1470000`，在封定前入队，推进至 `1550003`；两次独立 press 都产生 `Hit/Early`、误差 `−30000`，按各自 K4 分别归档为 `Good`、`G2`，中间 release 不结算。检查源及两个 Reader 的两个 GPR0 `measure` 引用指向**同一条** GPD0 `unused.measure` row，该具名行恰有一条且 `gradeTokens` 空；JPR1 的两个 `gradeTableIndex` 分别指 `main` 和 `five`，必须不同。非空 GPD0 token 变异即使和一侧吻合也拒绝。

**未用具名 Pattern 正对照。** 对 GP-01 原样和仅把 `gameplay.common.patternDefinitions[0]`（`patternId="unused.pattern"`，`doc.alpha/100`）的 `root.primitive` 从 `instant` 改为 `skip` 的版本各编译一次，再各自经 Capsule5 和 Graph2 读回。两版本均合法，声明和引用不变；T-12 owning GPD0 及两成品均保留该定义，primitive 分别为 `instant`、`skip`，两个 compiled digest 不同、同一版本三路 digest 相同。每路重放同一 GP-01 输入（`press@970000`，推进至 `1050003`），结算与计分轨迹相同；按事件字段比较，排除随内容改变的身份。

**PA-02（完整根构造）。** 从上述 GP-01 最终根改 `metadata.title="PA-02"`；`timing.stops=[{beat:B(5,4),durationMs:100}]`；为保留 Tap 的已知 1050000 deadline，把 `endBeat=B(19,10),phaseTargets[0].beat=B(9,5)`。写 `camera={type:"perspective",fovY:60,near:0.1,far:1000,defaultTransform:{position:[2,4,8],rotation:[0,0,1,0],scale:[2,4,1]}}`，`framings=[{width:16,height:9},{width:4,height:3}]`。原对象 `cuexis.transform.position=[0,0,4]`，使实际 draw 使用下文相机测试世界点；再加 `cuexis.renderable={version:1,mesh:asset("mesh.note"),material:asset("material.note"),alpha:128}`。从 [chart_v4_animation.json](../../../../../tests/fixtures/chart_format_update/valid/chart_v4_animation.json) 取 `/objects/0/components/cuexis.animator`，保持层/组/实例形状，实例 `startBeat=B(0,1),iterations="infinite"`，两处 `propertyMask={properties:["transform.position.x","render.material"],prefixes:[]}`。复制该文件 `/animationClips/0` 并替换 `durationBeats=B(1,1),tracks=[{property:"transform.position.x",segments:[{startBeat:B(0,1),durationBeats:B(1,1),startValue:0,endValue:2,startSlope:1,endSlope:1}]}],stepTracks=[{property:"render.material",steps:[{beat:B(1,4),value:asset("material.step")}]}]`，保持 clip id/version，作为唯一 `animationClips` 项。

写两个 `behaviors`：`{id:"hide.at-stop",type:"behavior.event",version:2,events:[],stepEvents:[{property:"render.visible",beat:B(5,4),value:false}]}`，另一项同形、`id="hide.only",beat=B(1,8)`。另建两个对象，UUID 与底稿对象相同前缀、尾部依次 `0003/0004`，`parent={domain:"object",id:底稿对象UUID}`，`extensions:{}`；各复制底稿的 transform/renderable（子对象的 local position 分别设 `[1,0,0]`、`[2,0,0]`），无 Animator，无 Note，各写 `cuexis.behavior={version:1,behavior:{domain:"behavior",id:对应 hide id}}`。用已有有效测试 mesh/material 字节提供 `mesh.note`、`material.note=M0`、`material.step=M1`，从实际 provider 填 RES1 byteCount/hash；若材质声明外部依赖，提供其 Asset Index 记录和字节。无其它资源引用。

写 `cxtImports=[{id:"module.one",source:"templates/prototype.cxt"}]` 与 `cxtInstances=[{bindingId:"scene.prototype",module:"module.one",export:{kind:"prototype",id:"export.one"},startBeat:B(0,1),parent:null,parameters:[],slotBindings:[]}]`。文档从 [prototype.cxt](../../../../../tests/fixtures/gameplay_author/prototype.cxt) 复制，改 `requiredExtensions:[],extensions:{}`，生成一个无玩法的实体；不得用作者 `entities[]` 注入 full E。补一个玩法 CXT 子用例：将底稿的全部 `gameplay`（包括唯一 requirement、rank、开发 fixture 的 `common` 声明）移入该模块的新 `/extensions/cuexis.gameplay.v3` 并设该模块的 required token。模块 K4 和 rank 的 `invocationId/moduleId/exportId` 改 `scene.prototype/module.one/export.one`，Chart gameplay 保留相同 graph-wide 字段及空 `declarations/resources/relations/solverProfiles/factBindings/judgementDomains/graceInputs/namedGraceDurations/patternDefinitions/measureDefinitions/requirements/rankAssignment.rows`，其 `sourceDocumentId="doc.chart"`；共同 mode 不重复生成。设 invocation `startBeat=B(1,4)`，模块的绝对目标、end 拍分别减 `1/4` 作为 local 值。最终拍和分数与原 PA-02 一致，唯玩法 owner 为实际生成 full E；错绑到显式对象拒绝。验证 CXT v1 animation binding 作为独立增量对照，以 [v4 CXT fixture](../../../../../tests/fixtures/chart_format_update/valid/chart_v4_cxt_template_binding.json) 的 `animationTemplateImports`、`cuexis.animator` 与对应文件为输入，在 PA-02 再加一个显式对象（复制该 fixture 的 `objects[2]`，`parent=null`，ID 不变）；复制根 `parameters` 和 `animationTemplateImports`，将 `motion.duration-scale.default=B(2,1)` 及 `constraints.maximum=B(2,1)`，weight 保持 1，不覆盖 PA-02 自己的 Clip。

### 载体验收

**PA-01，字节金样。** 独立构造至少下列 canonical payload，直接测试 encode/decode；不得拿 Writer 的输出作为唯一预期。空 `SCN1` 是 9 个 `00`（absent transform、0 framings、0 entities）；空 `ANI1` 是 8 个 `00`；空 `RES1` 是 12 个 `00`。零 offset、无 tempo/Stop 且仅零拍时 `TIM1` 共 44 字节：`i64(0),u32(0),u32(0),u32(1),i64(0),i64(1),i64(0)`。有一 entity、无 transform、framings=`[(16,9,1),(4,3,2)]`、无 element/note 时 `SCN1` 依次为 `00`, `02000000`, `1000000009000000000000000000f03f`, `04000000030000000000000000000040`, `01000000`, `0000`。一条 id=`b` 的 v2 behavior 仅在第 `1/4` 拍关闭 `render.visible`，绑定 entity 0，`BHV1` 恰为 56 字节，空格只分字段：

```text
01000000 0100000062 02 00000000 00000000 01000000 08 01000000
0100000000000000 0400000000000000 00 01000000 00000000 00000000
```

**PA-02，动态等价。** 用[完整作者夹具](#完整作者夹具)编译并写 Capsule5/Graph2；三路同一 AId、entity、程序字典与 digest，重开后按下列 tick 驱动同一 runtime。CXT v1 子用例的 binding 设 scale=`2/1`、weight=1，播放时长为被导入 Clip 两倍。用两个作者比例对照 [R5 画幅向量](render-player-writer.md#兼容目标及向量)。作者源使用 `camera`；Graph 正例只含 `staticChart.defaultCamera`。分别将合法 Graph 的该键换为 `camera`、及在保留合法键时另加 `camera`，均在封闭 staticChart 结构校验拒绝，不进入 SCN1 合并。正例仍在 tick 0 的实际 frame snapshot，view 矩阵行表达为 `diag(-1/2,-1/4,1,1)`，平移列前三项为 `(1,1,-8)`；将世界点 `(0,0,4,1)` 映到 `(1,1,-4,1)`。两个画幅的 view 相同，投影各自应用对应比例；各路径核对真正提交的矩阵与由它得到的 draw 投影，不能只比 SCN1 解码字段。用旧入口的 Euler/单位 scale 结果及交换 Quat/scale 的结果作反例。Clip infinite 时预期如下（表的 Behavior 列指 `hide.at-stop`）：

| `sampleTick` | Clip 期望 | 独立 Behavior 期望 |
| --- | --- | --- |
| `124999`、`125000` | 材质依次 `M0,M1` | true |
| `499999`、`500000` | 第一轮末前 `M1`；回绕到 x=0、M0 | true |
| `624999`、`625000`、`724999`、`725000` | `M0,M1,M1,M1`；后三者 x 均为 0.5 | true,false,false,false |
| `1099999`、`1100000` | `M1,M0`，第二项进入第三轮且 x=0 | false |
| `1224999`、`1225000` | `M0,M1`；`9/4` 拍未在 TIM1 保存，经动态查询生效 | false |

`hide.only` 的 `1/8` 拍仅在 BHV1 声明：TIM1 恰有 `(1/8,62500)`，其对象 `62499` 可见，`62500` 隐藏；其它对象不受影响。另复制 Clip 给两个不同对象做有限 iterations=2 的 Hold/None，`1100000` 时 Hold 写 x=2、M1，None 不写且回实际基线。取一个 separate blend fixture：X baseline=2，override clip=6 且 layer/group weight=1，下一高 priority additive delta=4、group weight=0.5、layer weight=1，最终 X=8；同组增加 `transform.scale` 段但只选 X mask，scale 保持基线。另选 v4/v5 fixture 中的 FOV behavior、Quat 曲线和 `transform.scale`、visibility Step、重叠权重 Additive/Override 及 CXT 参数实例，断言源与两 Reader 的相同 frame 意图（离散字段精确、连续分量以同路径 golden 容差）。一轮依 tick 连续取样，一轮直接跳到 `1225000`；离散期望相同。

短轮另用无 Stop、120 BPM，起拍 0，`duration=3/2000000`、scale=1、infinite、单线性 X segment 从 0 到 3（两 1 斜率、无 Material Step）；首三个轮边界实值 `0,0.75,1.50` µs， ties-to-even tick `0,1,2`。tick 1 已是第二轮 `k=1`，连续进度 `(1−0.75)/0.75=1/3`、x=1；不能因 `tick==首轮结束保存锚` 将进度置 0。加一对不同拍同 tick 的 Step 判例仍按最大拍生效，无除零或逐轮扫帧。

**PA-03，玩法与交互。** 用 [运行向量](runtime-timing.md#验收向量与任务前置) 的 v3 Grade/Exact/NormalizedScore 和同一 v2 对照，保留一个未用合法 Pattern、Measure、GradeTable。以 Exact `[-10000,15000)` µs 的独立可命中 phase，装配一个当前可见目标的 `Exact` binding 改为 false（另用另一个目标的 `Any`）。四个独立 run 在 `-10001,-10000,14999,15000` 的误差分别匹配 `Early,Exact,Exact,Late`；故仅中间两次第一目标隐藏，Any 四次均运行。无误差的 timer Miss 只匹配 Any 的 Miss binding。对 Hold 的真实误差 Miss、grade 与 direction 独立、phase 比较和合格 phase 单元计分做同一 source/Packed/Graph 对照。无须提供作者输入见证。每路的 prepared records、full owner、状态转移表、完整 score payload 与最后统计都核对相等。

**PA-04，关系、身份和长度阴性。** 每项独立、从有效产物变异；需要进入关系校验的项重算 CRC、所有适用目录长度/counters 和 header/metadata digest。删 `9/4` 动态拍不会改变 TIM1（本来不在锚表）；加入该拍或删一个只在 BHV1 的拍分别应是 `anchor_unexpected/anchor_missing`。将 TIM1 与 TIME 的 Stop 键或同拍 tempo tick 拆开均拒绝；把后续拍 tick 改到 E 前且 GPR0 同改仍按关系拒绝。将 Clip mode=2、非法 mask bit、循环 scale=0、重复 AId、错误 generated owner、CIx 越界、重叠段、负零 float、无效 UTF-8、错误 Behavior binding、重复/未约分画幅或 payload 多一字节分别拒绝。删除一个 live flag、替换转移为超域状态、删除必要 grade rule、选表越界、令 Exact 不含 0、修改 `jMax/cMax` 均拒绝；新段计数使用 `UINT32_MAX` 或超过 effective budget 前一项，分别验不先 reserve 及精确边界允许。只更新 CRC 后修改有效场景 rotation、BHV slope、Animation weight、RES1 material digest 或保存 Stop E 的值，应先过合法关系，再报 compiled digest mismatch。两 codec 各测一项同时包含关系错误与不匹配 digest，以证明优先级。

**PA-04 的逻辑 ID 对照。** 源 common=A、K4=B 时源编译拒绝。由双 Tap 有效成品构造两条 K4 A/B，保持各 row 镜像、计数、CRC 和自摘要自洽，Capsule5/Graph2 均先报第二种 ID 的关系错误。另将同一有效 A 成品的所有 K4 及相关镜像一致改为 B，按需重排、重算 CRC 和摘要，保留独立的旧 expected A：两种裸读均应接受（不传 expected），同 bytes 配旧 expected 则在摘要阶段报 `compiled_digest_mismatch`；不能用变体自己的摘要作独立 expected。分别用其余合法的零 requirement 样本验证两 Reader 不要求逻辑 ID。每个拒绝保持原 active。

明确删 `(1/8,62500)` 并保留 `hide.only`，`relatedFieldPath` 指该 BHV1 Step。独立 T-12 用同一组 Beat keys（只含 0 时连 anchor ticks 也相同）将 120 BPM 的 map 绑到 121 BPM 的 view，拒绝 `timing_source_mismatch`。对已有合法 score unit 的样本再加入一个只含「选表无合格结果」的**非 ObserveOnly** phase（安排为未参加任何 unit，保留全套不合格 0/keep 规则），变异只将该 phase 作为新单独 unit，保留旧分母值；预期 `score_unit_unqualified`，不能仅验分母。对 GRC0 两个有效独立样本各设 `terminalAfterTermination=false/true`，用同一 claim 与相同稳定排序；两 Reader 投影保持原值并验证释放后可复用/终止的区别。Graph 分别用 `\u0030` 替换 payload 的一个 `0`（接受，同 digest）、`\u0041`（解为大写 `A`，拒绝）、奇长（拒绝），以及原全文超 `maxBytes`（SAX 前拒绝）。default camera 增 Euler/Quat 双声明、补量非法 scale、Graph 静态投影非 identity quat 均拒绝。

**PA-05，包矩阵。** 使用同一项目分别构造 source-only（bootstrap 指源）、compiled-only（bootstrap 指任一成品）、同包多难度/混合载体（显式选择非首项）。容器均接受；source 入口启用时与裸谱/工程使用同一 compile，未启用时 SDK 回 `unsupported_feature`，参考 Player 回 `player.source.encoding_unsupported`。单个候选且 bootstrap 指向它，无 `--entry` 仍报 `player.source.entry_required`。删除已列 source/CXT/成品/所用动态材质/其依赖各一次，均按实际引用报 `missing_entry`；替换文件且不改 manifest 报 hash，不跳去测试能力。只保留空 entry directory、缺必需扩展、跨条目重复路径、错 artifactIdentity、交换 Capsule/Graph encoding 字段、改 selected metadata 的只读 mode/count/asset/binding 字段，均拒绝且不尝试其他条目。添加未引用的成品文件即 `unreachable_entry`；合法 source-only 仍接受。旧默认 CXC 保持可接受原样，显式 v1 candidate 的 source/encoding 对照不变。

**PA-06，闭包与兼容。** 独立消费者只链接 `cuexis::gameplay_packed_read`，不提供源文件、作者上下文或 Playback；测试可解码两种载体、查询准备数据、释放输入 bytes 后仍读取。独立阴性编译单元 include `compiled_source.hpp`、`compiled_writer.hpp`、任一作者头、`gameplay_capsule.hpp` 均须找不到声明头；另外用手工窄声明在有效 TU 引用 `compilePreparedArtifact`、旧装配/重装配、Playback binding 投影符号，须编译过而链接失败。记录 link map、include roots 和 allowed dependencies。对同一有效载体改变一个可接受且没有内容镜像的 host identity token，使 static prepared identity 改而 compiled digest/scoreDigest 和输出字节不改；明示 token 如 `session.loadoutToken`。修改场景/动画/触发属性使其超出新 profile 或请求旧静态输出时，断言无输出替换。保留原 Packed1、旧 Capsule/Graph、旧 Behavior source 的黄金 bytes、接受集与回放对照。

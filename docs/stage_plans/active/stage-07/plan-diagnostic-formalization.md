# Diagnostic Formalization Prerequisite Plan: N0–N3

状态：active；所属Stage7未关闭；独立诊断前置N0–N3已退出，SDK0.7.5已在本地分支提交；交付状态归CURRENT_STATUS

更新日期：2026-10-11

独立退出证据：[N3真实验证与冻结工作树](../../../stage_reports/stages/stage-07/diagnostic-formalization/2026-10-10-n3-verification-and-exit.md)。
本次任务到N3退出为止，不自动启动RPA，不授权提交/推送/PR/合并/发行。

## 阶段目标

将活动诊断中的开发阶段/临时状态命名改为稳定业务原因，修正已证实的原因混用，
保持公共投影、错误分类、故障状态、完整结果及恢复校验边界，并形成独立退出证据。
本工作包是Stage 7既有成果后的独立前置，不是新增编号阶段，也不属于RPA内部工作线。
原`RPA-N0–N3`保留作历史编号与检查清单标识；本文使用N0–N3，不另造阶段号。

```text
Stage 7A既有内核与S7A-7/8受限集成基线
  -> 诊断码正式化独立前置：N0 -> N1 -> N2 -> N3
  -> RPA新架构原型及A4–A6 -> A7/F1–F4完整验收与接受
  -> Stage 7B+实施、验收与接受 -> Stage 8
```

RPA A1–A3纸面设计、既有行为的只读基线盘点及测量设计可与本工作包并行。
新架构原型、A4–A6产品实施以N3退出为硬前置，另须满足RPA相应首用合同/选型及任务授权。
N3不代替RPA A7/F线的最终行为SHA矩阵，也不构成RPA或Stage 7A关闭。
依赖是诊断N3退出后RPA才能进入相应实施，不是反向等待RPA。RPA架构选型、生产预算接受、
实体设备及同步验收均不属于诊断N1–N3的退出条件，不得据此阻塞诊断正式化。

## 决策状态与本轮授权

2026-10-10 owner接受“定向原因整改、显式公共投影、新Recovery版本明确拒绝旧版本”的推荐组合，
并授权编写/细化规划，随后要求将诊断正式化从RPA剥离为独立前置。
这些方向已选定，不因旧文档中的“RPA内部命名线”或“下一轮泛化架构审核”重新征求整项同意。

本文持有范围与门禁；正式字段/兼容合同归所属Spec/ABI及码表。
[N1退出记录](../../../stage_reports/stages/stage-07/diagnostic-formalization/2026-10-10-n1-contract-and-entry.md)
已形成；N2已迁移正式名称/投影/revision2/0.7.5，N3定向检查及完整本地硬矩阵均通过，独立退出证据已形成。
较早授权是文档规划；owner随后要求下一次对话开始剩余阶段，从N1进入N2/N3，现已按该指令实施。
该任务授权不自动授权提交、推送、PR、对外评论、保护变更、合并或发行。
owner最新明确指定本工作包目标SDK API为`0.7.5`，版本数字已裁定，不再作为N1等待owner的未决项；
实际版本文件及消费者已在N2.4同步更新，开工源码基线为`0.7.1`，当前分支为`0.7.5`。
旧Recovery版本明确拒绝方向保持。公开差异、版本选择依据及必要合同已由N1/N3记录闭合，
不是仅凭版本数字接受判定通过；后续超出该证据范围的公开差异仍按具体影响处理。

## 前置条件与权威输入

- 每次执行先核对实际branch/HEAD/status/diff，保留全部已有未提交改动。
- N0基线为`stage-7`的`2ddbcafe22bd3facc63b4917cf99eeccc3d81cd1`及报告记录的工作树。
  后续若定义/调用/码表变动，刷新相应清单，不能复用旧计数冒作新基线。
- [N0报告](../../../stage_reports/stages/stage-07/rpa/2026-10-10-n0-diagnostic-inventory-review.md)
  及其ZIP持有逐码引用、动态构造、公开性与载体影响，不在本计划复制全部审计表。
- [Gameplay V2 Spec](../../../formats/GAMEPLAY_V2_SPEC.md)拥有诊断分类、故障、恢复及线格式合同；
  [ABI](../../../api/GAMEPLAY_V2_ABI.md)消费公共边界；
  [execution profile](../../../formats/gameplay-v2-execution-profile.md)、
  [author profile](../../../formats/gameplay-v2-author-profile.md)拥有各自声明/执行约束。
- `schemas/cuexis.gameplay-diagnostics.v2.codes.json`拥有集中登记；
  `cmake/VerifyGameplayDiagnosticsCodes.cmake`是现有表门禁，不证明生产者或投影覆盖。
- [VERSIONING](../../../guides/VERSIONING.md)拥有SDK/日期构建选择与更新流程；
  [文档政策](../../../DOCUMENTATION_POLICY.md)拥有文档角色和追溯边界。
- [RPA分册](plan-rpa.md)只消费本工作包的N3退出记录，管理架构、设备和综合收尾。
  [CURRENT_STATUS](../../../CURRENT_STATUS.md)仍是唯一当前状态摘要。

## 范围与不变量

覆盖原67项阶段诊断（66迁移候选、1占位）、2个活动provisional码、已证实的grace原因混用、
相应生产者/消费者、公共投影、恢复载体、码表/合同/测试及必要版本门禁。
N0发现63项有生产路径、3项仅声明、1个无调用占位；这些口径不等于集中表69个公共码。
细分原因可能改变最终码数，验收按已收敛映射/分支计算，不冻结历史数量作为产品目标。

- 保持九类别、severity/faulted、既有R拒绝编号、首错顺序及原会话状态变化。
- 名称不决定公开性；已登记公共、明确src-only、未登记内部分别记录。
- 新活动路径不输出阶段/provisional旧名或旧名别名；旧值只保留在已有owning结果、历史证据、
  明确旧格式输入/负例中，逐项说明用途。
- `sameError`、完整Kernel/ReplayEvaluation/Snapshot/Checkpoint比较不忽略code/message/
  ordered context/cause，不通过归一化或仅比较Score/Facts制造等价。
- `sourceCode`同样是可观察诊断值，需要同步正式化；不将它登记为新增公共原因码。
- 不改变Tick/Resume/输入碰撞支持、判定规则、Score、Fact顺序、预算或线程拓扑。
- 不重命名算法/profile identity token、fixture ID、普通步骤stage、测试标签或历史报告/ZIP。

## 已选定的N1收敛目标

### 命名与触发条件映射

N1形成逐触发条件映射，列出旧code、旧生产分支、目标code、类别/severity/faulted、公开性、
投影策略、原首错顺序、允许变化字段、owning/持久化影响与验证入口。一旧码可按真实原因拆分；
不得通过一个全局替换字典把所有同串调用映为同一个新码。
一般阶段码按N0的模块/子域/原因映射处理，以下目标优先：

| 项目 | 已选择的规划目标 | 收敛要求 |
| --- | --- | --- |
| 原`s7a4.session.lifecycle_order` | `judgement.session.lifecycle_order` | 延续原公共登记与现有检查条件 |
| 原`provisional.session.lifecycle_order` | `judgement.session.operation_state_invalid` | 保留独立来源/错误语义；不因原类别相同便合并或提升公开性 |
| 原`provisional.semantics.unfrozen`的legacy无参动词分支 | `judgement.session.legacy_entry_unsupported` | 保持原拒绝；`query()`有kernel时仍成功，不笼统命名整个无参API |
| 同一旧码的`query()` prepared但无kernel兜底 | `judgement.session.execution_state_unavailable` | 保留原已有分支，不新增状态检查/修复；可达性与支持范围分别记录 |
| `chartDuration`和`candidate`同时存在 | `judgement.grace.value_source_conflict` | `invalid_relation/error/faulted=false`；保持该检查先于禁止chart供给 |
| chart供给且`allowChartGrace=false` | `judgement.grace.chart_supply_not_allowed` | `budget_exceeded/error/faulted=false`，不借改名纠正类别 |
| 原`content.unsupported_form` | `judgement.content.unsupported_form`，本批不拆分 | 保留boundedRelationInstance的non_terminating_source与switch兜底的capability_disabled；不假设兜底已证明运行可达 |
| 三个仅声明常量 | 保留并正式命名，标记无活动生产者 | queue_hop_exceeded不猜category；另两项的注释意图不冒作运行元数据；不补新能力 |
| `secondHalfError`及专属token | 删除 | 开始实施前再查引用；若出现新消费者只暂停此删除批次 |

新目标符号/字符串需与当前全量码表及内部定义检查碰撞。本表是已接受方案的命名目标，
正式归属、所有调用覆盖及合同文字仍是N1待完成项；发现冲突时记录具体替代，不机械抢占旧公共码。
原provisional内部Result是否能经安装SDK到达宿主，必须追踪到实际公共入口。
直接返回不等于公共登记；不得用本次正式化默许泄漏或无声改变既有可观察结果。
若实际首用要求新增公共承载，单列合同/版本差异，不能混进“保持公开性”的实现批次。

两个原semantics.unfrozen分支共用“所需语义在本批次未冻结”的summary；仅改code仍会误述
legacy入口拒绝或执行状态缺失。N1为这两类分支收敛准确的summary/message及必要说明，
将其列为明确允许的字段差异，并追踪实际公开影响。其余message保持；grace两分支现有消息已准确，
不全量重写。旧owning错误保持原消息，完整比较仍包含message，不以忽略字段实现新旧相等。

### 显式投影策略

使用有限内部集合/策略表替代s7a2/s7a3前缀判断，不引入公共DiagnosticId或通用注册框架。
至少分别记录原5个公开输入/时基码透传、原S7A2/3内部码按现有九类别映射、原S7A4公共码直返，
以及provisional等其他来源的原处理路径。分支细分后仍按实际生产category投影，
特别是grace两条路径的公共code原本就可能不同，不因内部名拆分统一公共code。

公共登记状态、内部投影选择与生产category是三个不同属性，不强制code到category一对一。
当前graph族补充元数据、未知category的原fallback及错误context顺序分别保留并验证；
cause是否保留也按原路径表征，不借本批偷偷修复或删除错误链。
内部新增原因只进入内部集合，不自动新增集中公共登记。

投影输入表必须分别覆盖已登记公共、已知需内部投影、已知其他内部路径、已知内部码但category
缺失/未知、未知非阶段code及人工未定义旧前缀code。已知内部生产者的缺失/未知category保持原
默认类别选择和unknown-category fallback；未知非阶段code按原实际路径表征。
私有helper曾对任意旧阶段前缀投影，不证明这些未定义输入有公共生产者或兼容合同。
N1列明人工未定义旧前缀输入的基线差异和支持范围，不为它保留旧前缀alias，
也不把未知code提升为公共能力；新增内部生产者必须显式登记其投影策略。
原内部码的message、ordered context、sourceCode附加位置及cause处理逐项对照；
仅改映射中已收敛的字段（含上述两类消息差异），不能重排context、去重同名context或顺手补cause。
预生成的`failureResult`、`foldFailureResult`、`faultReserve`及恢复后的reserve副本也要追踪，
不能只改即时返回错误的调用点。公共返回Result、GameplayAdvanceReceipt.error与owning结果分别检查，
确保同一次失败的多份回执使用对应正式值，不遗漏缓存副本。

### Recovery格式分界与旧值保留

已选择新格式明确拒绝旧格式，默认目标为Recovery envelope revision `2`：

| 载体/操作 | 规划行为 | 必须证明的边界 |
| --- | --- | --- |
| 新Replay/Snapshot Writer | 写revision2与正式诊断值 | 不把旧owning值默默替换字符串后封成新格式 |
| 旧revision1 Replay/Snapshot（健康或含fault） | 当前Reader明确拒绝 | 使用有效依赖及结构合法的旧样本证明稳定版本拒绝；健康旧样本同样拒绝 |
| 未知revision | 明确拒绝 | 不能按大小关系当兼容版本；没有Reader降级或alias |
| revision2含旧诊断拼写且digest正确 | 明确拒绝 | 仅改envelope字段不能使含旧诊断的载体合法；按完整错误结构校验，拒绝原因在所属Spec/码表落定 |
| 新revision2健康/fault载体 | 完整验证及适用的恢复/评估 | digest、闭包、identity、counts、预算、fault结构与完整结果比较不放宽 |
| 已有owning Error/Result/Replay/Snapshot/Checkpoint | 原值保持 | 读操作不改名；不承诺旧完整结果与新完整结果相等；不兼容操作显式拒绝 |

版本拒绝优先复用既有`codec.version_unsupported`，但不改变当前依赖prepare、budget、
结构、digest等检查的既有首错顺序；多重损坏输入必须有独立首错golden。
旧值Writer拒绝及revision2旧码污染拒绝需要N1具体确定错误归属，不默认均为版本错误。
仅检查实际diagnostic code和相关sourceCode结构，不扫描普通message/context任意文本后擅拒绝历史引用。

不新增离线迁移工具、旧格式长期执行层或并行诊断dialect。不修改/重打包原归档。
外部已有归档需要升级的代价在迁移说明中明确；该代价已包含健康旧归档拒绝。
Checkpoint目前是owning组合而非独立codec，不为它虚构新文件revision；追踪其Archive/Snapshot及certificate。

envelope revision、Snapshot stateSchema、eventCodec、FactSemanticRevision、PreparedIdentity、
Graph/Capsule revision和SDK版本逐项形成“变更/保持及理由”矩阵。
默认只提升envelope，不因诊断改名自动改其他identity/profile token；只有实际表示/语义差异证明需要才修订。
保持body布局时，健康body与其payload digest是否逐字节不变须实测；完整envelope因版本变化不会字节相同。
magic与header/body现有字段不作顺手重设计；原码表schemaVersion也不自动跟随envelope递增。
若健康body完全相同，将旧文件版本字段改为2后可能已满足新格式的全部字节规则，
Reader无法仅凭这些字节判断制作来源。不能承诺检测这种来源；验证的是声明revision2的完整合法性。
这不提供受支持的迁移工具或转换证明，也不改变原revision1文件被拒绝的合同。

### 绕过codec的owning操作与检查范围

envelope revision只存在于bytes，不自动成为内存Error/Replay/Snapshot/Checkpoint的年代标签。
不得声称对所有旧owning对象都能判定来源；特别是健康对象若值和依赖与当前合同一致，
可以按既有操作规则使用或编码为revision2，这不是Reader接受revision1，也不是承诺旧归档迁移。
公开句柄来自匹配工具链/包边界，测试中的旧对象值用独立旧样本或明确构造的旧值表征，
不通过跨版本DLL传递内部布局来制造兼容证据。

| 非codec入口 | 必须覆盖的情况 | 规划边界 |
| --- | --- | --- |
| encodeReplay/encodeSnapshot及公开toBytes | 已知旧code藏于任一record.result、faultDiagnostic或cause/sourceCode | 写入前发现已知不兼容旧值显式拒绝，不改原对象；只检查末条record不充分 |
| recover/restore及公开restoreGameplay | 直接给旧值Snapshot、不调用fromBytes | 沿现有严格fault/依赖/状态校验拒绝不兼容值；失败不替换旧会话组合 |
| evaluateReplay及公开evaluateGameplayReplay | 旧错误证据与当前完整执行结果不同 | 不归一化sameError；沿原契约区分调用失败和evidenceValid=false，不强制两者变成一种错误 |
| checkpoint/seek及公开checkpointGameplay/seekGameplay | 旧Archive、旧Snapshot、混合Checkpoint/certificate | 按现有操作合同核验依赖及完整结果，保留Checkpoint筛选/复验/fallback；候选失败不变当前session，不能只验证Archive头或重新签发certificate冒作相容 |
| Result查询/AdvanceReceipt读取/同结果比较 | 持有旧code的owning副本 | 原值保持；完整比较可不等；句柄空值/移动后拒绝及寿命行为保持 |

不兼容拼写检查按已收敛旧码/前缀构造集合识别，并遍历每个实际Error的整个cause链及
各层sourceCode字段；当前未登记的其他任意code不因此被全局白名单拒绝。
新增检查复用既有结构，按现有wire方式迭代遍历cause，不为扫描复制整链或递归展开。
保持maxBytes/maxElements等现有预算及首错规则，覆盖深链和重复sourceCode负例；
不为本次命名检查新增生产限额，也不将其扩大成通用预算整改。
不扫描普通message、field.section、remediation等自由文本寻找阶段字样；
这些字段的当前说明修订/历史引用分别按映射和例外登记，不假装都是诊断code。
检查位置、错误归属以及与digest/结构/预算/执行校验的首错关系必须在N1列明，
新污染检查不能把原结构Reader升级成“全部Replay执行证据已经有效”的验证器。

Seek中的不适用或history不匹配Checkpoint按原规则忽略；certificate失效时先从起点复验，
复验或restore不能采用时保留从起点重放的fallback。不能因混合缓存或未采用Checkpoint中
有旧码，就一律把原可成功的Seek改成拒绝；实际选中并使用的证据校验位置及拒绝归属在N1落定。
fallback不重写旧对象、不伪造相容证书，最终重放失败仍沿原错误和候选原子性合同返回。
当前evaluate完整不匹配返回Result失败、成功返回evidenceValid=true；
不为覆盖表格另造evidenceValid=false分支，只有所属合同实际修订后才有相应新验证。

### 版本与合同落点

目标SDK API已由owner指定为`0.7.5`；本次指定及其与常规选择规则的关系归
[VERSIONING的诊断正式化版本裁定](../../../guides/VERSIONING.md#2026-10-10诊断正式化版本裁定)。
必须覆盖新公共code/sourceCode、旧载体接受范围、candidate ON/OFF、安装消费者和包请求行为。
candidate隔离是否保持受支持生产边界兼容需实际证据，不因指定patch数字或签名未改便声称兼容。
N1将实际不兼容项及本次限定版本裁定明确记录；不反向改变已接受的旧载体拒绝来制造patch兼容。
日期构建由`tools/update_version.py`更新；SDK API版本按VERSIONING的独立来源更新，
不能声称updater同时更新SDK API版本。

| 权威工件 | N1需落地的内容 |
| --- | --- |
| 适用ADR与V2 Spec | 正式原因、已选旧载体拒绝、格式分界及不兼容影响；如触及已接受ADR，修订该决策 |
| V2 ABI / Playback API引用 | 公共诊断及owning结果、Reader拒绝、sourceCode可观察行为 |
| 集中码表及现有校验规则 | 改名公共项/src-only项，fault码特判、family元数据；不把内部新增原因注册为公共 |
| execution/author profile相关章节 | 与首用诊断/恢复revision的必要引用；算法语义未变不重写profile |
| 版本差异说明 | 实际base/head/tree/工作树范围、SDK选择依据、最低consumer与迁移验收；不复用旧审批 |

N1退出产物为逐分支映射、投影策略、逐载体兼容/revision矩阵、版本差异说明及所属合同同步。
仅本计划完成不算N1退出；不要把历史N0报告改写为已实施证据。
必要说明整改包括source_codes、session、diagnostic及session测试的“码表尚未建立”陈旧说明，
以及judgement_session头中legacy无参动词拒绝的适用范围；typed重载不可被笼统描述为未支持。
安装公共头的说明仍须保持ASCII，不将中文规划文字复制到安装头。

### 可重复的清单与残留门禁

N1为每个映射条目记录production_status及可达性：活动路径、只有声明、已有但未证明可达的兜底、
无调用待删除辅助函数分别处理。不可达项用调用/enum/状态不变量证据解释，
不为达到覆盖数字构造原契约禁止的生产状态，也不把人工负例当真实故障发生证据。

N2交付可重复运行的清单/残留检查，报告定义、动态构造、完整目标、调用分支与例外。
扫描engine/app/tools、schemas/cmake、正向fixture/测试以及现行Spec/ABI/profile中的活动码，
覆盖常量引用和拆开拼接的前后缀；检查码表code/family/source与srcOnlyPolicyNote的当前说明，
对rulingProvenance等历史原文按字段列例外，不把整个码表排除。
门禁检查阶段/provisional活动码残留、目标碰撞、公共登记/投影覆盖、故障码特判与实际类别差异，
尤其保留`content.unsupported_form`的多类别和仅声明项的“无实际赋值”。

例外登记采用具体路径/字段/符号、旧值或匹配规则、用途和依据，不整目录豁免tests或src。
至少包括Spec冻结的`presentation-target-missing`/`partial-group`、既有裸公共投影码、
两段业务族、算法/profile identity、prepare操作步骤stage、fixture、历史ZIP/报告、
旧revision负例与保留owning值。正向测试/现行生产说明不得沿用旧码。
对门禁本身验证“新增旧常量”“拆开阶段前缀拼接”“公共投影漏项”“目标重合”均能失败，
以及明确历史负例/非诊断token不会误报；这项证明补足现有码表门禁只检查表内部一致性的限制。

N1确定该门禁的持久执行入口、命令、candidate ON/OFF适用范围、CTest登记/labels及失败传播；
可扩展现有码表gate或增加script-mode检查，不要求另造通用检查框架。
N2将适用检查接入现有日常自动入口，N3核对实际发现列表、运行日志和隔离负例的非零退出，
证明自动入口能阻断遗漏；只手工跑一次扫描不满足交付。需要工作流变更时单列影响，
不因本条规划自动修改或发布CI工作流。

## N0–N3执行批次与依赖

| 批次 | 工作与可审查产物 | 退出门禁 |
| --- | --- | --- |
| N0 当前审计 | 已有逐码JSON/CSV、动态构造、额外问题、例外、载体影响及历史ZIP核对；开工前刷新差异 | 当前基线可追溯，目标/生产状态口径清楚；没有把静态审计当运行证据 |
| N1.1 映射收敛 | 对每个调用分支应用上述目标，保留实际元数据/公开性，验证名称碰撞、legacy可达性和首错 | 所有迁移/保留/删除/细分项都有落点，无未解释的新增公共能力 |
| N1.2 投影与恢复合同 | 显式策略、旧值操作矩阵、revision2格式分界、版本/污染拒绝码、所属合同修订 | 类别/故障/完整结果规则闭合；旧格式拒绝影响已写明 |
| N1.3 版本与实施准入 | actual public diff、已指定SDK 0.7.5的差异/版本说明及显示版本处理，命令/配置矩阵、旧格式fixture固化、回退包 | 消费已裁定版本，必要合同与N2输入闭合；不再等待版本数字选择，变更先于消费者落地 |
| N2.1 常量与生产者 | source_codes、profile/kernel动态构造、session分支细分、grace拆码、占位删除/注释修订 | 活动定义/生产路径无旧标签；分支顺序和元数据保持；新名目标唯一 |
| N2.2 公共投影与码表 | 有限显式策略、集中码表/family/校验器同步，残留门禁及自动入口登记，工具/fixture期望及公共合同引用 | 原公开/内部边界逐项对应；含sourceCode的完整诊断符合映射；适用检查被发现且失败向上传播 |
| N2.3 恢复与消费者 | 新Writer/Reader版本与旧值拒绝、fault严格校验、owning/Replay测试及安装消费者同步 | 所有写入/读取/restore/evaluate/Seek路径一致，旧值不被重写 |
| N2.4 版本集成 | 按N1决策更新实际版本/consumer最低请求及相关拒绝门禁 | fresh configure、clean build和安装包/生成头一致；不沿用旧审批 |
| N3.1 定向语义/兼容 | 下节全部错误、首错、旧值、载体及完整结果检查 | 所有适用硬项通过；失败保留复现，不放宽检查 |
| N3.2 配置/消费者/平台 | 受影响Debug/Release、ON/OFF、headless、static/shared与适用平台门禁 | 命令、配置、源码指纹/最终行为SHA、失败及未执行项完整 |
| N3.3 独立退出与交接 | 残留/例外、验收报告、原始日志、版本/回退及RPA输入基线 | 硬门禁无缺口，N3退出记录成立；RPA据此准入原型/产品实施 |

表中N2是依赖顺序及审查分组，不承诺每个中间批次都可独立发布。
生产者、投影、恢复及对应合同/测试必要时组成一个完整可构建变更集，避免中间态先绕过投影。
先运行每组定向验证再扩大矩阵；只有新变化、失败或未解问题需要时才重复已通过的检查。

## 验收标准

### 定向功能与兼容矩阵

| ID | 验证对象 | 必须核验的结果 |
| --- | --- | --- |
| D01 | same_tick_collision、amount、timebase负例 | 正式code，类别/severity/faulted/context与首错行为保持 |
| D02 | 原S7A2/3内部投影与grace两个分支 | 原公共映射保持，sourceCode为对应正式内部码，无公共能力扩张 |
| D03 | 生命周期、legacy拒绝、query兜底 | 两生命周期来源分别验证；prepared且无kernel兜底当前按状态不变量提供不可达证据，不造新生产状态；若合法可达性改变则刷新映射和用例；old/new首错对照 |
| D04 | 同时竞争grace值且禁止chart供给 | value_source_conflict仍先返回invalid_relation，禁止供给单独返回budget_exceeded |
| D05 | kernel/Fold故障注入 | kernel新码、Fold原码、faultStage/faulted与已提交前缀/画面许可保持 |
| D06 | owning旧值与Checkpoint | 原错误链/原件不变；新Writer遇不兼容旧值显式拒绝；新旧完整结果不伪造相等 |
| D07 | 完整ReplayEvaluation/Seek/restore | 健康与适用fault路径、evidenceValid、完整Kernel/Fold/error及检查顺序；decode通过不代替evaluation有效 |
| D08 | 旧revision1 Replay/Snapshot | 合法健康/故障样本均稳定拒绝；原文件hash保持；Checkpoint组成载体另查 |
| D09 | 新revision2及旧诊断污染 | 正常读写/适用恢复；有效digest但旧code混入显式拒绝；仅修改版本字段不消除旧code；健康同body不作来源鉴别承诺 |
| D10 | 损坏/未知/超限输入 | version/tag/length/section/digest/identity/count/record/budget拒绝与适用多重故障首错golden |
| D11 | identity与字节比较 | 用无诊断代表样本比较body/digest；envelope版本区别单列；诊断改名不自动改变判定identity，必要revision有独立证据 |
| D12 | 常量、动态构造、投影、工具/码表残留 | 无未登记阶段/provisional活动code；仅声明项元数据不过度断言；所有例外逐项有依据 |
| D13 | 嵌套错误与非末条Replay旧值 | 当前code但cause/sourceCode含已知旧码仍被发现；普通message历史字样不误拒绝；遍历早期record与预生成fault副本；深cause/重复sourceCode按迭代与既有预算首错验证 |
| D14 | 非codec旧值/混合Checkpoint | 直接restore/evaluate/checkpoint/Seek的旧诊断、健康同值及混合certificate分别核验；保留不适用Checkpoint忽略、失效凭证复验与从起点fallback；不依赖fromBytes拒绝代验 |
| D15 | 公共失败原子性与多份回执 | prepare/load/reload/restore/Seek候选失败按现有合同保持旧组合及generation；submit/advance区分预检拒绝、已准入执行及发布失败，保留sealed前缀；逐字段核对Kernel、公开Result及AdvanceReceipt对应诊断，不要求不同失败阶段的错误相同 |
| D16 | 残留门禁的正负验证 | 旧常量/动态片段/漏投影/碰撞能被发现，具体合法例外通过，无整目录忽略；日常自动入口发现并执行适用检查，隔离负例使入口非零退出 |

旧样本从真实旧基线生成/固化，不靠把新产物字符串替回旧名冒作历史格式；
损坏/污染样本则明确标为人工负例并保持可复现。faulted Replay是否能按现有语义重放，
遵守原evidenceValid合同，不以故障注入改变生产轨迹或伪造重放成功。
跨新旧版本逐项比较完整字段/载体：仅映射和revision矩阵明确允许的code/sourceCode、
两类summary/message及相应说明/版本差异可变化，其余字段按原合同保持，不忽略未变字段。
同一版本内仍保持完整结果比较；跨版本差异核对不替代sameError/sameResult的严格语义。
faulted Snapshot的旧样本来源须区分公开snapshot是否允许生成与内部DTO/test-only构造。
如当前公共入口拒绝faulted snapshot，不借生成兼容fixture开放该入口；
合法内部格式样本、人工污染样本和可重放故障证据各自标记，不能互相替代。

### 构建与公共消费者矩阵

核对现有preset、编译器、SDK/candidate配置及CTest发现结果，优先按实际labels选择用例。
Debug/Release、candidate ON/OFF、headless、static/shared安装消费按变更影响覆盖，
不能只用包含全部内部头的单元测试证明公共SDK兼容。
码表、architecture/allowlist、公共头泄露/ASCII、package/最低与过高/跨minor版本请求、
SDK/日期版本和文档门禁分别留证。版本变更必须fresh/clean-first，使用clean staged消费者。
MSVC、MinGW、Linux按适用现有环境执行；Linux sanitizer/coverage/clang-tidy不得由MSVC检查替代。
hosted需要新行为SHA的适用证据及独立工具授权，旧SHA通过不能代验本批。

N1必须交付命令级矩阵，而非只列“Debug/Release/ON/OFF”。每行写preset或等价配置、
独立build/staging目录、CUEXIS_ENABLE_CHART_V5_CANDIDATE显式值、library type、compiler/
triplet、适用case集合、必须执行/不适用及理由、所需源码身份。用隔离目录保留既有构建/evidence，
不复用含相反candidate缓存的目录；CTest发现后核对实际case数，0例不能记通过。
`cuexis_judgement_tests`有judgement label，但Playback Catch2 cases未统一标为judgement，
不能以该label通过代替候选公共测试；记录发现列表并选中实际Gameplay cases。
基础headless presets关闭developer tools，但candidate-headless-debug显式开启；按实际配置
选择可覆盖工具/Host命令的候选headless或单独配置，必要时使用匹配工具产物，
缺工具不能把相关case的SKIP当通过。

公共消费者至少包括clean staged static/shared安装测试中的候选fromBytes/toBytes、
evaluateGameplayReplay/restoreGameplay/checkpointGameplay/seekGameplay，以及错误Result/AdvanceReceipt。
补足旧revision拒绝、新负例code/sourceCode和候选失败保持旧session的检查；仅健康读写样本不充分。
工具/CLI与Reference Host相关错误输出用现有可控fixture验证，区分内部原始码与SDK投影码，
不因命名回归要求重新进行本工作包未触及的真实音频/GPU/键盘设备验收。
candidate OFF验证包隔离及无新生产能力，不能尝试调用其不存在的候选API。

文档门禁包含`check_docs.py`、适用status/section/target契约测试和`git diff --check`；
版本门禁按实际差异执行updater检查及version gate脚本/测试，审批tuple与发布验证保持独立。
MSVC/MinGW/Linux/hosted各行在N1明确是否属于本工作包硬门禁及理由，不能实施后才把失败项改成不适用；
RPA-F2的最终同SHA矩阵仍由RPA负责，不机械提前复制整个设备/预算关闭矩阵。

硬门禁未执行时N3保持未退出，注明具体缺口；不因没有推送权限擅自发布，也不把平台未运行标通过。
RPA F1/F2最终矩阵仍在架构及预算稳定后绑定其最终行为SHA，允许映射明确的同SHA同配置证据，
不能用本工作包N3代替后续架构/设备验收。

## 证据、回退与交接

每次运行记录日期、环境、命令、配置、编译器、退出码、原始stdout/stderr和fixture/hash。
未提交验证记录HEAD加实际diff/新增文件及内容指纹，不将HEAD单独冒作工作树源码身份；
获准形成提交后再将退出证据绑定实际行为SHA，文件版本变化另记。
N3.3记录每项要求的pass/fail/not_run/not_applicable及日志入口，独立区分本地候选验证完成、
适用硬门禁全部通过与退出源码身份冻结。N1同时明确退出所需身份：仅本地硬矩阵允许完整冻结、
可复现的工作树快照（含base HEAD、diff、新增文件、配置及内容哈希）作为身份；
需要hosted或精确SHA审批的硬项则必须绑定实际提交SHA及相应证据，不用快照冒作该SHA。
未获准提交本身不额外阻断只要求快照身份的矩阵，也不构成自动提交授权。
硬门禁缺环境、所需身份尚未冻结或相应审批缺失时，可交付工作树候选证据，
但不得写成N3已退出或据此启动RPA产品实施。

已有N0报告/ZIP留在`docs/stage_reports/stages/stage-07/rpa/`原路径，作为迁移前的dated证据保留，
路径名不决定后续任务归属。本工作包新合同差异/验收报告建议放
`docs/stage_reports/stages/stage-07/diagnostic-formalization/`，建立目录README并从Stage7证据索引可达。
只在实际形成报告时创建目录/索引，不建立空的“已验收”入口。

交接包包含映射/投影/载体/revision/SDK决策、实际实现与检查证据、残留例外、旧归档拒绝说明、
回退源码基线/版本/工具链与旧载体原件入口，以及RPA应复验的受影响项。
回退是恢复整套匹配的旧源码/合同/包及原件，不降级重写新revision2产物，不回退已发行版本号。
文档、SDK、码表、生产者与消费者不能只回退其中一层。

## 排除项与停止条件

不实施RPA架构/线程/音频调度、同Tick/Resume规则变化、预算接受、F线关闭、7B+或Stage8发行。
不引入稳定诊断数值ID、公开通用registry、旧格式永久执行层、离线迁移工具或新算法版本。
N0对`playback.override.mask`仅提出可选弱命名建议，本包不迁移该码；若另行处理需独立范围与公共影响说明。
需要改变原类别、公开承载、判定语义、body布局或既有不变量时，登记反例、被替换条款及影响；
只暂停依赖该未决项的消费者，继续可独立完成的审计/规划/验证准备。
名称碰撞、删除项新消费者、公开首用泄漏、实际版本差异不符前提均按具体问题收敛，
不重复请求是否开始已接受的整项工作。任务授权、审批、合并/发行门禁仍分别执行。

## 归档来源与追溯

归档来源：[历史阶段诊断审计](../../../stage_reports/stages/stage-07/rpa/2026-10-09-diagnostic-phase-name-audit.md)、
[2026-10-10 N0复核](../../../stage_reports/stages/stage-07/rpa/2026-10-10-n0-diagnostic-inventory-review.md)
及其各自原始ZIP；[plan-a旧命名入口](plan-a.md#3419-诊断码去阶段命名规划2026-10-09)
保留DN78到RPA-N的历史映射。最新独立前置归属以本文和Stage7总计划为准，
不改写历史报告的审计日期、计数、当时授权或验收结论。

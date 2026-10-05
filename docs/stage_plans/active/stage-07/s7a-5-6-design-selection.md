# S7A-5 / S7A-6 Implementation Options and Selection

状态：design recommendation；已选下一轮批次组合与推荐方案，尚未实施或冻结生产字段/线格式

日期：2026-10-06

## 1. 本次要解决的问题

owner要求选出多个批次作为下一次实施目标，并对阻碍实施的未定决策提供4–5套方案、选优。
本文件取代854efa3中“下一轮只做5、把profile选型留给下一轮”的排期结论。
选定 **S7A-5.1–5.5 + S7A-6.1–6.5**，同一实施会话内按依赖先5后6，最后联合验收。

权威语义仍是 [V2 Spec](../../../formats/GAMEPLAY_V2_SPEC.md) §3.14–3.22、§5、§8和
[V2 ABI](../../../api/GAMEPLAY_V2_ABI.md) 域5–7/9及未决项。
下述P56编号只用于本规划，不增加CONTRACT_MATRIX条目、R编号、诊断类别或新的裁定轮次。
推荐方案是下一轮的明确设计输入；消费前须落到相应Spec/ABI/profile和逐字段typed合同，
不能将本规划当作已完成的实现、owner接受或生产线格式。

### 1.1 五套批次组合

| 方案 | 下一轮目标 | 收益 | 风险与退出条件 | 选择 |
| --- | --- | --- | --- | --- |
| A | 仅5 | 集中完成Fold；修改面较小 | 5.5的真实Seek/Replay一致性仍跨会话悬挂；未满足本次“多个批次”目标 | 不选 |
| B | 5 + 6 | 同轮闭合结果状态、恢复、统计重建；状态字段只交接一次 | 工作量高；必须先锁定5的状态，再消费6的codec，保留中间门禁 | **选定** |
| C | 5 + 6 + 7 | 进一步取得Playback/Player/consumer入口 | 同时改变状态、wire和SDK桥接；7的失败责任较难归属，独立consumer矩阵扩大 | 后一轮候选 |
| D | 5 + 6 + 7 + 8 | 完成产品接线和Stage6四项交接 | 还需D-9 master落库、具名复核与SDK条件化放行；owner前置尚不齐全 | 不纳入下一轮 |
| E | 5–9 | 全Stage7A关闭包 | 容量阈值须测量后分别接受，最终平台/设备/owner证据无法预先替代 | 不纳入下一轮 |

排序先检查硬约束：既有语义、确定性、所有权、原子失败与跨工具链可复现；
再比较字段可审计性、实现复杂度、跨批返工和今后可扩展性。
没有本轮性能实测，以下“成本”是结构评估，不是耗时预测。
联合实施仍保留5→6依赖；5的内部检查通过后才开始6消费，不把两个事务混成一个事务。

## 2. 真正阻碍5/6的决策清单

| 本地编号 | 未定决策 / 现场依据 | 首次消费 | 选定推荐 |
| --- | --- | --- | --- |
| P56-01 | Ruleset Interface/manifest/StateDelta的具体内存表示；ABI域6仍是角色追踪 | 5.1/5.2 | R1：owning typed声明 + 内置有限操作注册表 |
| P56-02 | declaredGradeTokens尚无可执行grade scale/tolerance；当前PhaseOutcomeFact不携带grade结果 | 5.3、5/6对照 | G2：显式phase-local有符号整数区间表，seal前一次求值 |
| P56-03 | Score/Combo/Statistics宽度、初始化、溢出/饱和和合法monoid carrier | 5.2–5.5 | N1：i64 Score/u64计数 + 明示策略 + 白名单monoid |
| P56-04 | seal后Fold事务、分配失败发布保障、Fold cursor与Ledger cursor关系 | 5.2/5.4 | T2：独立候选Fold状态、预留fault载体、单次发布 |
| P56-05 | 完整可恢复状态DTO、私有ingress借用、恢复校验与owning快照 | 6.3 | S2：Kernel/Ingress/Fold分族owning DTO |
| P56-06 | 两套header、Event/Fact/ID/resource等首次wire与格式版本轴 | 6.2/6.3 | W2：确定section顺序 + LE定宽record + 完整性摘要 |
| P56-07 | Replay怎样保留late admission、advance分段并复用真实输入路径 | 6.2/6.5 | E2：规范事件 + admission/control journal + Fact校验前缀 |
| P56-08 | 任意Seek目标、同horizon多次admission和最近可用快照定位 | 6.4 | K2：显式目标cut + 按(horizon,cut)排序的全量快照索引 |
| P56-09 | 未接受预算下descriptor、读入预检和负例怎样可执行 | 5.2/6.2–6.4 | B2：测量/接受状态显式分离，生产数值留9 |

三阶段顺序、Fact总序、三类Register、t+1 Hook、faulted矩阵、Life/package/correction拒绝、
四分量identity归属、全量Snapshot十四条与不得保存清单已经裁定，不重新列为待选语义。
D-9与SDK0.7.1归8；真正业务预算与maxSeekLatency数值归9，均不在本轮代选。

## 3. P56-01：Ruleset内存与有限执行模型

| 方案 | 表示与执行 | 优点 | 成本 / 风险 |
| --- | --- | --- | --- |
| R1 | owning typed声明；prepare生成只读manifest；运行时只派发已链接的有限操作 | 字段、owner、身份投影可直接追溯；无需解释器或依赖 | 需要逐项实现、验证已支持操作；扩展须新增受控实现 |
| R2 | 静态模块类与虚接口，每个模块手工管理私有状态 | 模块封装清晰；现有C++实现方式熟悉 | 状态闭包、执行边界、snapshot DTO易分散，额外审查每个模块 |
| R3 | 完整预编译决策表，按phase/outcome/grade索引产出delta | 每次消费简单、易人工golden | 组合爆炸；跨Fact combo和ledger_derived仍需独立状态逻辑 |
| R4 | 扁平typed arena，句柄连接module/register/operator records | 分配集中、所有权稳定，便于计数 | 索引错配、无效句柄和schema迁移检查更多，可读性较低 |
| R5 | 命名record/variant字段目录，显式tag驱动内置处理 | 增加字段方便，检查规则可集中 | 容易发展成通用解释器；字段完整性与类型配对需额外验证 |

**推荐R1。** 不是任意宿主回调、脚本或bytecode：操作集合由已编译实现穷尽，禁止IO/随机/动态插件/
动态Requirement。static registry只查显式module ID/revision/build identity，不执行外部package。

下一轮字段清单至少包含：

| 组 | 必须具名的字段和规则 |
| --- | --- |
| Interface | ID/revision、支持phase/outcome/grade token、capability、arbitration、programPolicy、outcomeScope |
| manifest | 有序module ID/revision/build identity列表、有序fold entry；顺序是语义，不排序消除 |
| RegisterDeclaration | ID、kind、owner/contributor集合、value carrier、operator ID/revision、identity element |
| StateDelta | register ID、writer/contribution identity、typed值；exclusive每Tick只提交一次最终写 |
| 评分配置 | 每category/outcome/optional grade的显式规则、Score/Combo初值、break规则、溢出策略 |
| Hook | 单owner、输入/输出值类型、t+1生效值、pending signal的owning状态 |
| session | Loadout/JudgementConfig的显式有效值；沿Spec§5.2归session，不混入ruleset分量 |

缺字段、未知module/operator/grade、重复owner/contribution、非法policy/scope在prepare原子拒绝。
禁止借用调用方字符串；Source数组的无语义排列规范化，manifest/fold有语义顺序保留。
首版RegisterValue选i64/u64/bool三个明确tag，operator声明与值tag严格配对；
grade token作为owning评分输入，统计按具名token计数，不扩展成任意userdata。
贡献键采用register ID + module ID + commitTick + optional FactId + localOrdinal，
全部来自规范执行上下文；同Tick重复键拒绝，不使用arrival sequence或线程完成序。

## 4. P56-02：grade从声明到不可变Fact

| 方案 | 做法 | 优点 | 成本 / 风险 |
| --- | --- | --- | --- |
| G1 | 显式整数步长与等级数，prepare生成等宽分箱 | 小声明、查找简单 | 不适合early/late非对称窗口；生成端点必须受检 |
| G2 | 按phase声明有符号tick error的互不重叠区间与grade token | 支持非对称、非等宽；人工正负例清楚 | 必须证明可达域覆盖、端点及Miss策略完整 |
| G3 | 显式绝对error半径表，error本身仍保持有符号 | 对称时间窗自然、表较小 | 不能表达非对称；INT64_MIN绝对值须宽化受检 |
| G4 | 每个可达error整数值的完整查表 | 一次lookup、含义直接 | 大窗口表体积大；prepare和identity字节成本高 |
| G5 | 已链接的有限整数公式profile + 显式参数 | 常见曲线少字段，仍可纯确定执行 | profile逐个证明；边界更难人工审计，不能允许任意表达式 |

**推荐G2。** 区间使用显式闭端点i64；检测重叠、空区间、重复token、未知token与声明可达error域的缺口。
不能用相邻端点加一造成溢出。Hit、Miss、没有error时的grade presence都须由表显式声明；
没有grade表仍是absent，无默认grade；新增可执行grading profile下，有token声明却无可执行table不能接受。
旧3/4 profile允许opaque token携带而不求值的历史路径继续按原合同支持，不被上述新门禁误拒绝。

prepare将源Measure声明与本次显式typed table绑定；不在5/6改Chart/CXT/Capsule发行格式。
phase-local grading在kernel构造Fact、seal之前执行一次，新的profile/FactSemanticRevision显式登记；
Fold只读最终optional grade，不追溯改写已seal Fact，不从error重新猜grade。
旧3/4 profile和golden继续支持原先absent行为；有grade的新profile采用独立golden。
表的Measure语义进chart投影，解释算法/revision进engine，grade→score映射进ruleset，
按既有Spec§5.2投影归属逐项登记，不另造第二张identity权威表。

## 5. P56-03：数值与三类Register

| 方案 | Score与计数 / 合成 | 优点 | 成本 / 风险 |
| --- | --- | --- | --- |
| N1 | i64 Score、u64 Combo/计数；显式checked/clamp政策；已证明carrier的operator白名单 | 与现有64位typed/受检算术相衔接，wire简单 | Score顺序累加与monoid必须分开验证 |
| N2 | 双u64 limb表示有符号128位Score，u64计数 | 表示域大、延迟表示溢出 | Windows/GCC一致运算及signed wire需要额外实现，不可依赖原生int128 |
| N3 | module显式固定精度decimal，整数mantissa + scale | 固定精度计分直观 | rescale、舍入与组合carrier更复杂；不允许浮点替代 |
| N4 | 显式模2^64 carrier；按模运算产生状态 | 合成闭合、无UB，算子容易证明 | 回绕后的分数/计数业务解释不自然，必须显式声明且改变identity |
| N5 | 有限状态值集合 + prepare验证的combine表 | 任意小carrier可穷举验证交换/结合/单位元 | 大分数域不可行；operator表与Score投影复杂 |

**推荐N1。**

- Score值/增量为i64，Combo/current/max及逐category/outcome/grade计数为u64。
  初始值、每类增量、absent grade评分和combo break全部显式提供，不以零默认补字段。
- Score以规范Fact顺序在单owner局部累加；整个Tick对exclusive目标只发布一次写。
  支持明示checked-reject与明示clamp-to-declared-range；范围是评分语义，不能冒充容量预算。
  clamp与range进入ruleset identity，溢出分支必须有独立人工golden。
- Combo/统计计数受检；不可表示时Fold失败，不回绕，也不把表示失败改名为已证明预算超限。
- commutative_monoid只接已声明、已经证明的operator/carrier：例如u64非负饱和sum、max、
  bitwise union。每个operator都有明确单位元、carrier边界、revision、贡献类型。
  非负饱和sum在固定carrier上可结合；checked有符号sum及有符号饱和sum不能作为该sum operator。
- ledger_derived无直接写，按对应已提交Fold前缀重建投影；原始Fact前缀另可查询。
  Score/Combo/Statistics候选值与Ruleset状态同一阶段发布，不在kernel中另建评分路径。

owning Statistics至少具名：Fold Fact cursor、总phaseOutcome/Hit/Miss数、每category的Hit/Miss、
每个已声明grade与absent计数、receipt的stray/consumeEmpty计数，以及current/max combo与Score投影。
空组、输出顺序和reset值明确；不把phasePriority、source arrival排列、表现资源加入评分。

## 6. P56-04：seal后事务与失败查询

| 方案 | 发布机制 | 优点 | 成本 / 风险 |
| --- | --- | --- | --- |
| T1 | 整个kernel + Fold候选深拷贝，分别发布Fact与Fold | 单次实现概念简单 | 复制完整Ledger/历史成本大；容易误回滚第一阶段 |
| T2 | 保留kernel Fact seal；Fold候选状态/写集合单独构造，校验后单次发布 | 直接对应既有三阶段，失败责任清楚 | 必须预留fault发布/输出容量，禁止提交后再做可失败分配 |
| T3 | 就地更新Fold + 有界undo journal | 拷贝少 | undo必须无失败；回滚本身分配/抛异常会破坏承诺 |
| T4 | persistent/COW寄存器树与不可变版本root | 旧query存活自然，更新量可小 | 节点分配、snapshot展开与性能计数复杂 |
| T5 | prepare建立双buffer，Tick末切换active | 发布无分配、状态小则直接 | 动态grade/register/统计规模增长需要预先说明容量来源，不能填隐藏cap |

**推荐T2。** 在每Tick seal消费点接入Fold，不能等整个advance结束再评分/发布signal。
预构造失败诊断、fault状态载体和发布所需容量；所有可能失败的分配在阶段2发布前完成。
阶段2发布只切换owning状态和已准备值，不调用任意宿主代码。

显式保留两个游标：sealedLedgerCursor与foldCommittedCursor。
kernel未seal失败回滚整个该Tick的kernel候选；seal后Fold失败保留新Ledger和旧Fold cursor/state。
普通Statistics查询返回旧已提交Fold值；显式Ledger查询可见新Fact。重建统计从Fold cursor起算，
不能用原始Ledger末端覆盖旧统计，也不能隐藏失败Tick的已seal Fact。
faulted查询报告failedTick、两个cursor与稳定原因；新signal/RuleEffect/表现发布均为零。
旧owning query、reset及替换新session的生命周期按既有合同保留。

## 7. P56-05：完整owning恢复状态

| 方案 | DTO组织 | 优点 | 成本 / 风险 |
| --- | --- | --- | --- |
| S1 | 一个全量显式SemanticSessionState record | 状态检查集中、字段完整性直观 | record很大，kernel/ingress/Fold职责容易混杂 |
| S2 | KernelState、IngressState、FoldState三个owning家族 + 共用header/cursor | ownership和首次消费对齐，分别校验后统一恢复 | 需要一张跨家族引用/一致性表 |
| S3 | 全量稠密index arrays，graph identity校验后恢复index | 体积小、访问直接 | index有效域及图顺序必须稳定，诊断和wire可审计性较弱 |
| S4 | 以结构ID为键的全量有序record集合 | 接近语义、错误定位直接 | 字符串/结构ID重复和查找成本较高 |
| S5 | full snapshot持有不可变chunks，序列化时展开完整DTO | 不依赖原session，快照共享可节省复制 | sharing与schema是两层，需证明别名不引入可变借用 |

**推荐S2，引用优先用已有结构ID，局部index仅在重新取得prepared graph后校验/映射。**
禁止序列化C++内存、指针、size_t布局、runScope地址和test controls。

| 状态族 | 要覆盖的实际状态与校验 |
| --- | --- |
| Ledger/结果 | 完整Fact前缀/IDs/cursor、phase状态、resource/contact/lease/ownership、observer/receipt查询记录 |
| Matcher/活动 | DFA状态与epsilonAttempted、active Requirements、coverageHistory、Release/tail推进 |
| 时间/ID | processedFrontier、lastAdvanceHorizon、timerCursor、nextCommitId及exhausted、failedTick/requestedHorizon |
| Ingress | owned规范subject值、sequence/重复检测集合、lastObservedTick、next ingress ID及exhausted、pending输入与admission context |
| Fold/Hook | 寄存器/Score/Combo/Statistics、foldCommittedCursor、Hook committed值、t+1 pending signal值与可见时刻 |
| session | lifecycle、完整稳定fault诊断、journal cut；所有引用须闭合 |
| 准备值 | 记录实际preparedGrace；由identity重新取得prepared graph/FactBinding并复核一致，不保存其图副本 |

Snapshot十四条逐项映射到实际成员；KernelProjection只是查询值，不能以它代替完整私有状态。
特别是coverageHistory、timerCursor、ID exhaustion、ingress ownedSubjects/admittedSequences、pending admission
都会影响后续release/去重/late admission，必须保存或证明能无损重建。

pending signal保存的是将来输入所需的owning语义值/时刻与Hook committed状态，
不是RuleEffectEvent、Effect Graph或表现事件cache；与Spec§3.25禁止序列化投影事件保持一致。
恢复先全量验证到候选存储，再一次替换；失败保留旧active、旧query和旧snapshot。
fault字段schema可验证，live faulted仍禁止新建snapshot、seek、就地restore；
旧快照仅可恢复到显式新session，不能补齐失败Tick delta。

## 8. P56-06：首次wire与独立校验

| 方案 | 编码 | 优点 | 成本 / 风险 |
| --- | --- | --- | --- |
| W1 | 单块LE定宽record，动态字段长度前缀，固定字段顺序 | 最易独立编码/人工golden | section定位与字段家族诊断较弱，schema新增改整块 |
| W2 | 固定有序section，section内LE定宽record/长度前缀 | 可分家族检验；无varint最短编码歧义；容易计数 | 比压缩编码大，必须严格约束section唯一性和顺序 |
| W3 | canonical varint/ZigZag + 有序section | 数值小的流更紧凑 | overlong/noncanonical编码、极值及长度预检更复杂 |
| W4 | BE定宽record + 有序section | 可移植，部分整数便于字节审计 | 与现有主要LE候选工具方向不同，仍不能按bytes取代语义排序 |
| W5 | typed columnar records + 全量字符串字典 | 重复ID较多时体积可小 | dictionary完整性、列长关联、解码成本及golden复杂度高 |

**推荐W2。** 先选表示并冻结完整表，再写Reader/Writer；不提前发行，不引入压缩/增量snapshot或跨minor迁移。
编码草案：formatVersion/stateSchemaRevision与section tag为u32，计数/长度/CommitId/localOrdinal为u64，
Tick/error/Score为显式i64二进制补码，bool/presence/已登记小tag为u8；无padding、无native layout。
FactSemanticRevision和已有Interface/module/profile revision沿注册token，使用显式长度前缀，
不把既有语义token擅自窄化成上述codec数字；四分量identity保持既有不透明值表示。
FactId编码为两个LE u64；资源语义tag显式登记free/held/terminal，其他tag拒绝；
各phase/outcome/OriginKind/tag独立登记，不复制C++ enum underlying或variant index。

Replay和Snapshot分别使用独立candidate formatVersion/eventCodecId/stateSchemaRevision表。
两套语义header字段仍沿Spec§3.18；envelope/framing和完整性摘要属于物理层，不冒充新judgement字段。
section清单/顺序/是否必需、record字段顺序/宽度/presence、字符串现有校验规则须在首卡枚举完整。
未知/缺失/重复/错序必需section或tag、尾随bytes、长度溢出、截断与不闭合引用整体拒绝。

先核对实际buffer长度与受检count×recordBytes，禁止按不可信count直接reserve。
使用现有core内部SHA256校验envelope字节完整性，再独立校验identity和状态引用；
摘要不进judgement identity、不代替Fact/Score结构对照。不得新加JSON DOM或第三方序列化依赖。
snapshot独立校验不需要原session；恢复仍必须取得同identity的prepared graph和静态Ruleset。
版本不兼容稳定拒绝，不把byte layout revision误登记为Fact语义变化。

## 9. P56-07：Replay输入、late admission和control journal

| 方案 | 权威输入与检验 | 优点 | 成本 / 风险 |
| --- | --- | --- | --- |
| E1 | 规范事件 + 每次submit/advance边界，末尾完整Fact Ledger对照 | 实现直接，能复现arrival上下文 | 若仅末尾校验，定位长流错误较慢 |
| E2 | 规范事件 + admission/control journal + 分commit Fact前缀校验 | 复现queue_next_tick与分段；定位首个偏差；可供Seek复用 | journal物理字段和一致性检查需首先固定 |
| E3 | 每Tick完整输入frame与Fact集合，含空Tick边界 | trace易人工对照 | 大量空Tick存储；不得枚举巨大空区间造成无法有界执行 |
| E4 | 全量checkpoint + 规范事件/control suffix + Ledger校验 | 缩短恢复、具备独立分段检验 | 文件携带更多全量状态，与snapshot codec耦合 |
| E5 | canonical事件块 + 块级control manifest + Ledger块 | 易分块Reader与峰值计数 | 首版块索引、跨块sequence和拒绝原子性复杂 |

**推荐E2。** 记录accepted规范输入、source/sequence去重所需metadata、submit batch boundary、
admissionFrontier/admissionHorizon、原始与dispatch tick/wasForwarded，以及advance horizon/control cut。
这些复现上下文不成为Fact规范排序键；原始设备私有对象、表现/Effect事件不进文件。

live与Replay都进入同一normalize/admission/submit/advance核心：Reader重建owning输入，
复算并对照dispatch/forwarded结果，不能凭文件提供的dispatch tick绕过late policy。
Replay保存已提交Fact Ledger供逐commit结构检验，不能把文件Fact安装到kernel假装完成判定。
旧journal无与结果相关的信息时稳定拒绝；不能只按observationTick排序后全量预提交。

文件格式拒绝在变更旧active前完成；执行/校验使用新候选session，全部成功才接受替换。
格式坏、identity错与实际执行的deterministic Ruleset fault分别登记，
故障等价性使用相同故障注入条件，不承诺重放随机分配失败或墙钟性能差异。
负例分别比较shared-input层的同code/category/faulted与wire层格式拒绝；
不能把一份损坏文件的transport错误当作“同一正常事件流”去要求live报相同格式错误。

## 10. P56-08：Seek cut、全量快照和恢复策略

| 方案 | checkpoint选择与推进 | 优点 | 成本 / 风险 |
| --- | --- | --- | --- |
| K1 | 全量checkpoint线性扫描，选满足target cut的最近值 | 最简单，适合作独立oracle | 快照多时线性查找成本大 |
| K2 | 全量checkpoint按(horizon,journalCut)排序，二分定位并校验cut | 确定、便于人工验证；避免同horizon晚到admission污染 | 需明确target cut，维护有序索引 |
| K3 | 平衡树索引全量checkpoint | 插入/最近查找统一 | 节点分配、重复horizon规则和序列化索引更复杂 |
| K4 | 显式调用方保留的全量checkpoint集合，查找最近可用值 | ownership/保留成本透明 | caller选择影响可用快照数；无最近值时仍需起点fallback |
| K5 | 按事件量/语义边界建立全量checkpoint，保留有序索引 | 稀疏输入可以减少空时间checkpoint | checkpoint调度更多；不允许变为增量snapshot |

**推荐K2，独立reference用K1和从起点运行。**
目标Tick解释为advance horizon，不混成observationTick、dispatchTick或processedFrontier。
首先定义目标cut：原journal中第一次advance horizon达到/超过目标的control前缀；
若该advance跨过目标，用同一已admit输入将它截到目标；若目标超过已录制horizon，
使用完整已录制输入/control前缀再空推进到目标。Tick表示与目标域错误仍原子拒绝。
这一定义是首次恢复消费所需的推荐补充，须先以late-admission最小反例写入恢复profile并验证，
不得改变现有L1/L2 admission或将source arrival顺序变成Fact排序键。

同horizon的多次submit产生不同journalCut；选snapshot时同时要求horizon≤目标且cut≤targetCut，
不能用在同一horizon更晚录入输入的snapshot冒充更早目标状态。没有快照时从起点fallback。
尚未advance的horizon保持absent，作为独立起点checkpoint处理，不能用隐式0覆盖负Tick。
空日志目标以初始状态推进；目标早于首个已录制advance时，保留该步骤之前已admit的pending输入。
这些cut边界必须成为人工golden，不能由生产Seek函数自己生成参考答案。
从选定snapshot建立新候选状态，重放control前缀，重新构造t+1语义signal并校验Ledger/统计；
成功后一次替换，失败保持旧active。输入archive/快照自持有，不借用原session。

采用全量快照，不设隐式默认快照数量/业务频率或未测量延迟承诺。
fixture显式提供多种checkpoint间隔；间隔/索引/cut不进judgement identity。
同horizon晚到、queue_next_tick、snapshot有pending输入、负Tick、两端表示值与seek返回旧query
必须逐项比对；不能只比较最终分数或hash。

## 11. P56-09：未接受预算与可执行负例

| 方案 | 门禁处理 | 优点 | 成本 / 风险 |
| --- | --- | --- | --- |
| B1 | descriptor仅携带measurement-profile引用，全部统计在sidecar | header简单，不预选阈值 | 需额外明确哪些required计数在header，sidecar不能代替它们 |
| B2 | descriptor明确pending/accepted状态、profile引用；observed counts独立字段 | 首次表示可闭合，未接受不会冒充无限值 | accepted数值分支的生产启用留9，需测试隔离 |
| B3 | descriptor引用具名registry，所有测量状态由registry查询 | 策略集中 | owning与跨进程必须携带可验证registry版本，不能借宿主可变全局 |
| B4 | 文档期测量descriptor与后续接受descriptor分型 | 类型上防止误填限额 | 9需要额外schema迁移，wire版本多一轴 |
| B5 | 首版仅记录事实性counts/bytes/time，budget请求全部显式拒绝 | 行为明确、无隐藏cap | 与既有header的typed descriptor角色还需独立映射 |

**推荐B2。** S7A-5/6生产只允许pending_measurement + 具名measurement-profile引用；
header必需的event/Fact/byte counts是事实性值，不是上限。
对新Replay/Snapshot预算或maxSeekLatency填具体承诺值的输入稳定拒绝，
不写0、UINT64_MAX或“无限制”占位，不复制研究上限。

实际buffer边界、字段可表示性、语义引用和identity检查始终执行，与业务预算接受分开。
真实运行只累计计数和耗时；缺阈值维度保留INCOMPLETE GATE。
budget超限分支通过显式test-only accepted fixture/injection验证，不向生产registry或公共头写测试阈值。
现有已接受Packed limits继续原样消费；本轮不把生产Replay解码预算或Seek承诺宣称完成。

## 12. 整体推荐与退出

组合B + R1/G2/N1/T2/S2/W2/E2/K2/B2构成下一轮推荐。
核心做法是先确定可序列化的真实Fold状态，再实现两个独立提交边界，最后以同一输入日志完成恢复闭包。
完整实施步骤、小目标对账、验证矩阵和新对话指令见
[联合实施接手](s7a-5-6-implementation-handoff.md)；剩余7–9归属见
[五批交付规划](s7a-5-9-delivery-plan.md)。

第一张实施卡必须把九项推荐转成完整字段/owner/revision/诊断与正负例表；
本轮不会预填缺失的业务阈值，不会把选优文档升级为实施/容量证明。
遇到反例先指出具体哪项推荐无法满足既有合同并修订该项，其他独立卡继续推进；
普通表示细节可在已选方案内补齐，不能无证据改回“所有字段留到以后”。

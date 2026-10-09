# S7A-7/8 Review Options and Recommendations

状态：candidate；方案比较与推荐组合，待对应合同首用落定

更新日期：2026-10-07

## 范围与选优依据

本文件消费[10组重审问题](../../../stage_reports/stages/stage-07/readiness/2026-10-07-s7a-7-8-plan-review.md)，
推荐与首用门禁登记在[plan-a §3.4](../../../stage_plans/active/stage-07/plan-a.md)。
实际基线为stage-7、55fc8e6920e7ddb4b34a56bd0b9c27d07d60b833，保留未提交规划/拆分/审查改动。
R78-09拆为发布/版本，R78-10拆为文档引用/公共状态，共12项选择、每项5方案，总计60个方向。
后缀a/b只是这两组的子问题，不新增capability、码表类别、ABI角色或原R78组数。

比较优先顺序：既有语义与拒绝规则一致；失败后结果可观察、可恢复；实时/Replay同路径；默认SDK兼容；
范围与实现复杂度可控。没有成本实测，不给虚构分数；最优指当前S7A-7/8边界下的推荐。
标明需修合同、需改范围的备选不能按普通表示授权直接实施。
本次不实施产品代码、不变更SDK/build版本、不接受新数值预算、不代owner审批、不关闭阶段。

## R78-01：组合推进的部分提交与失败报告

| 方案 | 不同方向 | 优点 | 代价与约束 |
| --- | --- | --- | --- |
| A | 前置预检 + 逐阶段提交报告 | 沿用kernel/Fold封存；宿主知道哪里已提交 | 须明确Result与seal后报告/查询的关系，预留报告容量 |
| B | 先准备Runtime基础帧候选，再推进Gameplay与发布帧 | 尽早发现可预计算的帧错误 | Gameplay投影依赖仍在后半段，不能宣称整Tick回滚 |
| C | Playback内分成显式准入/推进/帧发布协议 | 单步边界清晰，适合调试 | 宿主须遵守更多顺序，需修P78-03组合入口合同 |
| D | Gameplay与帧各发布immutable版本，以配对凭证关联 | 可以查询精确版本和落后帧 | 版本配对、迟滞与资源寿命更复杂，不改封存规则 |
| E | 请求ID及结果回执缓存，重复请求返回原回执 | 防止失败后的不确定重试 | 新增请求生命周期与缓存成本；仍须报告实际提交阶段 |

**推荐A。** 校验时间、discontinuity、batch与可预留容量在变更前；报告分清accepted、sealed/成功Fold前缀、
实际H/F(H)、faultStage与帧/投影是否更新。seal后故障不能返回“全旧状态未变”的含义，宿主不得原样重投已接受batch。
Gameplay fault沿J0保留前缀；表现失败独立诊断，不改Gameplay fault语义。
报告的所有权、错误时取得方式和下一合法操作先写Playback lifecycle/ABI；不另建第二套码表。
验收：前置失败、kernel/Fold故障、投影失败、重试与旧帧保持的完整query/archive/FrameSnapshot对照。

## R78-02：可发布表现前缀

| 方案 | 不同方向 | 优点 | 代价与约束 |
| --- | --- | --- | --- |
| A | 从成功Fold的committed cursor/workTick派生发布边界 | 复用现有状态，无新增持久化投影日志 | 无Fold模式须明示适用边界，不能只取Ledger.size |
| B | 每个成功Tick生成内部发布许可回执 | adapter不必推测阶段是否成功 | 许可生命周期/预分配/恢复重建需证明 |
| C | 第二阶段成功后才生成transient投影任务 | 发布资格与任务构造放在同一边界 | 任务生成失败不能回滚Fold；恢复仍从Ledger重建 |
| D | 仅消费typed成功Tick提交描述，不直接扫描Ledger | 发布协议明确 | 提交描述不得变为宿主callback或新的持久化Fact |
| E | 每次从最近成功提交的owning结果全量构造投影 | 逻辑简单，适合小内容和参考对照 | 每帧扫描/分配成本高；faulted不可生成新Snapshot |

**推荐A。** 实时可发布前缀与显式只读重建分开；Fold失败保留Fact，但失败Tick不激活token。
依据成功Fold状态而非从“最后一条Fact”猜发布边界。无Fold配置先定义允许的投影来源，否则拒绝未定义组合。
不把cursor变成新Fact身份或新Snapshot wire字段；已有Fold状态及完整Ledger足以作为重建输入。
验收：成功前缀保持，失败Tick Fact可查询、Score旧值保持、零新token，查询faulted不改画面。

## R78-03：Prepared失效规则

| 方案 | 不同方向 | 优点 | 代价与约束 |
| --- | --- | --- | --- |
| A | 统一组合mutation generation | stale校验简单，与现有Prepared一致 | 每种操作须有精确变更/无操作表，溢出前预检 |
| B | Playback与Gameplay两套generation，候选捕获二元组 | 保留两模块独立更新 | 每个入口须正确捕获/检查两域，漏一域即失效漏洞 |
| C | 用内容身份、archive/cut/H、accepted frontier结构凭证比对 | 能精确说明状态差异 | 比较与维护成本高，不能漏future accepted/控制记录 |
| D | 候选持有写租约，存在期间阻止新增变更 | 不产生过期候选 | 改变原prepare→update语义，需修合同并保留legacy行为 |
| E | immutable live版本句柄，commit只接受原句柄 | 天然区分旧状态 | 引入状态版本存储/COW，增加内存与析构复杂度 |

**推荐A。** 成功accepted submit、持久advance/控制、内容替换及既有成功update纳入失效表；
accepted future或零Fact也算变更。空submit、纯query、预检拒绝按现行J0不伪造变更；same-H记录是否持久决定是否变化。
generation不进判定四分量；不得回绕复用旧凭证。borrowed view按实际owner与存储失效规则另列，不改owning结果寿命。
验收：prepare→每种变更→commit stale；no-op/拒绝、owner、move/discard和重复commit矩阵。

## R78-04：Seek/分支投影重建

| 方案 | 不同方向 | 优点 | 代价与约束 |
| --- | --- | --- | --- |
| A | 暂存整套transient投影状态，验证后一次替换 | 失败保持旧投影，易证明无旧队列残留 | control重建需要一次临时状态空间 |
| B | 每帧纯函数从当前Ledger/时间全量求投影 | 无持久游标/去重残留 | 大Ledger的扫描成本高，需改P78-06增量选择 |
| C | immutable前缀共享，只重建被截断/更换的后缀 | 长局控制成本可能更低 | 前缀匹配必须校验archive/cut/身份，证明复杂 |
| D | 新projection scope令旧队列/token整体失效，延迟清理 | 控制路径不必立即回收全部旧对象 | resolver须过滤旧scope；旧资源寿命与回收更复杂 |
| E | 缓存经验证的投影描述检查点，恢复时重新取token | 重复Seek可复用描述 | 新缓存及验证成本；不得持久化Host token或偷用未验证cache |

**推荐A。** 新状态包含scope、cursor、去重集、future队列、aggregation与token描述及当前目标映射；
成功激活只做不可失败的owner-thread move/swap，失败保留旧active投影。
去重仍为scope内(factId,targetId)，不追加bindingId，不改Fact identity。
projection scope只在重建/替换/分支时变化，与R78-03每次mutation的generation分开，避免每次submit都重建。
验收：same-H、多次Seek、同长度不同Ledger、分支复用FactId、旧未来队列、失败swap前资源错误及owning旧query。

## R78-05：换表现后的恢复

| 方案 | 不同方向 | 优点 | 代价与约束 |
| --- | --- | --- | --- |
| A | 判定身份兼容时显式重取当前表现依赖并重建 | 允许只换表现而复用判定记录，保持身份分区 | 要验证当前binding/资源/目标映射与控制意图 |
| B | Presentation identity不同即拒绝，另发独立rebind操作 | 控制分工保守清晰 | 宿主多一步，组合rebind/recovery失败仍须事务 |
| C | 此类控制要求完整artifact/content身份相同 | 最严格、最易拒绝旧内容 | 有效的只改表现恢复也拒绝；不能收紧底层Snapshot接受面 |
| D | 静态注册投影profile的显式兼容映射 | 支持多代表现合同 | 注册/迁移表增加范围，不能动态加载代码 |
| E | SDK给owning投影描述，由宿主准备并显式激活 | 宿主可自由翻译表现 | 宿主要维护更多资源/事务约束，不替代公共SDK合同 |

**推荐A。** 必须是typed control明确的重绑定意图，不能silent fallback；新Prepared先完整校验，
只改纯表现不改四分量，当前表现身份及资源决定投影内容。换判定身份仍按原恢复拒绝或创建新会话。
复用R78-04整体swap、R78-02发布资格；坏必需资源不能因headless降级。
验收：目标A→B、资源替换/缺失、presentation-only变化、有/无表现的完整ReplayEvaluation与不同投影结果。

## R78-06：覆盖层实现

| 方案 | 不同方向 | 优点 | 代价与约束 |
| --- | --- | --- | --- |
| A | PropertyResolver显式区分layer与layer内priority | 层顺序不受宿主整数影响，保留原Host调用 | 须写清新层关系、同层冲突和OFF兼容 |
| B | Gameplay先经独立typed resolver，再接既有Host/Studio求值 | 既有Host resolver改动较小 | 多个求值步骤的错误与值来源需统一，不直接写Component |
| C | 预注册独立Gameplay写通道，以通道所有权决定覆盖 | 目标/属性冲突可以提前验证 | 通道mask/所有权/释放协议增加表示复杂度 |
| D | 为Gameplay保留Host priority区间 | 可复用旧token载体 | 旧Host允许任意i64；需改变合同并验证迁移，不属于普通表示 |
| E | 新显式Presentation policy选择Host/Gameplay冲突处理 | 可支持多种宿主策略 | 新policy及身份/兼容维护扩大范围，首版不宜引入 |

**推荐A。** 建议顺序为Initial < Behavior < Animation < GameplayOverride/FactBinding < HostOverride < StudioPreviewOverride。
保留已冻结的Gameplay相对Animation/Studio顺序，并保留Host显式覆盖基础求值的用途；Host priority只在Host层内比较。
这是待写回Spec §3.23与Animation/Playback合同的推荐，不宣称原合同已定义Host相对Gameplay位置。
同层相同priority冲突沿明确拒绝规则；绑定同键冲突仍按P78-12处理，不依container顺序选值。
验收：Host优先级i64极值、显式false、Host/Gameplay同属性、Studio覆盖、default-OFF旧帧与公开布局保持。

## R78-07：Gameplay lifetime

| 方案 | 不同方向 | 优点 | 代价与约束 |
| --- | --- | --- | --- |
| A | typed PresentationTick绝对区间，调度时判当前有效性 | 端点/Seek/帧率可统一，沿未来队列 | 区间和无期限形状须首用冻结，checked算术 |
| B | SDK给typed到期请求，宿主负责按Tick释放 | 宿主可定制显示 | 宿主必须补处理跨区间跳跃，证明路径更多 |
| C | 每帧对已提交Fact的lifetime区间纯函数采样 | 无创建/释放时序残留 | 全量采样成本较高，需改增量adapter策略 |
| D | 显式duration锚定确定的commit/effective Tick，再派生区间 | 适合相对时长反馈 | 锚点改变行为，early/late不能按首个渲染帧起算 |
| E | 编译为独立有限表现timeline，按绝对Tick采样 | 生命周期与已有timeline求值接近 | 增加投影编译产物/identity/资源闭包，首版复杂 |

**推荐A。** 有限区间建议[start,end)，start<end；无期限使用显式UntilReset形状，不把INT64_MAX当隐式无穷。
T上先排除end<=T，再计算start<=T<end的当前值；跨过整段不激活、不补发瞬间画面。
同Tick的不同写仍走resolver冲突规则，不用队列插入顺序定赢家。Pause保持或重建策略按C78-04明示；
Seek/reload从目标T重建。PresentationTick与判定Tick的显式关系归C78-03，不默认同单位或从double ms倒推。
不改旧Host RemainingFrames/UntilChartTimeMs合同。验收：exact端点、早晚、零/负区间、负Tick/极值、不同帧率、大步跳跃。

## R78-08：Graph JSON的解析路径

| 方案 | 不同方向 | 优点 | 代价与约束 |
| --- | --- | --- | --- |
| A | json_support内SAX/事件解析，直接构造受检typed Graph | 可在DOM/大容器前拒绝深度/重复键/count，准确保留整数 | Reader状态机与字典/引用顺序检查较复杂 |
| B | 对同份owning bytes做结构/预算预检，再用现有DOM解析 | typed lowering复用现有DOM代码 | 多一次字节扫描；预检与DOM必须同规则，不能二次读Provider |
| C | Schema生成专用Reader/Writer和校验器 | 字段类型和表示容易保持一致 | 引入生成链与Schema变更审查，不新增第三方依赖 |
| D | 新compiled JSON规范采用扁平表/固定record形状 | grammar嵌套上界可以由结构导出 | 格式设计限制更强，仍须入口字节/count/重复键预检 |
| E | 运行只接受Capsule，Graph JSON仅作离线审查材料 | 完整复用现有binary界 | 不满足S7A-7的gameplay-graph entry验收，须先改阶段范围，当前排除 |

**推荐A。** 仍用P78-04的显式版本JSON Graph，在json_support内解析，不泄漏DOM到Playback/Judgement。
grammar可导出的结构上界先登记，复用已有物理界的维度须逐项证明可比，不能把Packed数字机械复制为新Graph阈值。
不得新增默认生产限额；确需新阈值时先登记首用门禁，未接受时不启用受影响消费。
整数直接用i64/u64受检载体，整数位不接受f64中转或舍入；重复键在解析时拒绝，未知必需字段与revision早拒绝。
Writer按同一规范产生唯一字节表示；读侧容许哪些非canonical拼写另列，不能凭DOM默认行为接受。
Provider读取/closure校验归现有source owner，后续只消费同份owning bytes/DTO；不展开author-source、不调用tools。
验收：深嵌套/大count/短数据、重复键、2^53与i64/u64边界、错revision、人工JSON/Packed同义和失败顺序。

## R78-09a：多产物发布

| 方案 | 不同方向 | 优点 | 代价与约束 |
| --- | --- | --- | --- |
| A | 默认仅输出单CXC，验证后原子替换 | 复用现有单文件发布，边界最小 | filesystem多文件输出需另定，不能暗称已覆盖 |
| B | immutable generation目录，既有project manifest是唯一commit点 | 同时支持多文件project与旧active保留 | 需规定内容派生目录名、并发及旧generation寿命 |
| C | 多文件事务标记，Reader只接受已committed集合 | 可继续原路径布局 | Reader/工具都要理解事务标记，须改格式合同 |
| D | 工具持久发布journal，重启按journal完成或恢复 | 故障恢复过程可审计 | 跨平台journal/同步/锁协议维护重，不能回滚已消费内容 |
| E | CLI输出完整immutable bundle，宿主负责发布入口 | 发布责任适合嵌入工具宿主 | 生产CLI本身不自动完成所有发布；须另有具名宿主验收 |

**推荐B用于filesystem多产物，CXC单文件保持现有A实现。** 先暂存Graph/Packed/closure/资源并通过真实prepare，
发布immutable最终generation后，只替换既有project manifest；该manifest是唯一可见入口，不引入第二种逻辑包格式。
generation名由内容闭包派生，随机staging名不进入最终字节/逻辑路径/identity；相同输入跨工具链产物保持确定。
并发相同generation先验证实际字节再复用，不覆盖其他构建；不同generation以单入口发布序列控制。
失败入口仍指完整旧内容。旧generation不得在仍可能被active/provider引用时删除；本轮不自动猜测外部reader寿命来GC。
原子可见性与断电持久性分开写，不能仅凭rename承诺跨平台断电零丢失。
验收：各产物失败、manifest替换失败、并发发布/Reader观察、确定性与临时目录残留，不以逐文件rename当整体原子。

## R78-09b：SDK版本策略

| 方案 | 不同方向 | 优点 | 代价与约束 |
| --- | --- | --- | --- |
| A | 实际公共diff决定版本：证明兼容可保留未发行patch，否则minor | 与现行VERSIONING一致，不提前虚构兼容性 | 必须完整API/consumer审查后才能确定最终号码 |
| B | Gameplay公共合同统一选下一minor | 发行边界清楚，容纳签名/布局调整 | 即使可兼容也增加迁移/发行范围，仍须owner接受 |
| C | Entry patch与Gameplay minor分两次发行 | 两个合同包各自验收 | 增加分支/发行/审批步骤，需改当前联合交付安排 |
| D | 新optional component建立独立功能版本，保持基础接口 | consumer可明确选择新组件 | 安装/版本矩阵增加；组件版本不能绕过SDK变更政策 |
| E | 本轮保持internal-only，公共Gameplay API延后 | 避免本轮新公共差异 | 不满足当前S7A-7安装consumer目标，需改范围，当前排除 |

**推荐A。** 0.7.1是现有Entry候选，不自动覆盖未来Gameplay变化；先盘点ON/OFF的声明/符号/DTO、布局、
签名、默认行为、重载、聚合初始化、继承实现与最低consumer版本，能证明兼容才保留patch候选。
无法证明或已有不兼容即提minor与迁移，按实际最新基线选择，不预设具体号，不回退/重用已发行号。
同一次未发行合同可分批实现；最终owner审批绑定最终base/head/tree/version/date，先前tuple不能沿用。
新Gameplay公共DTO独立，不alias/install内部Kernel/Recovery执行图；shared仍为匹配工具链C++ preview。
验收：0.7.0重建consumer、ON/OFF/static/shared、错误flavor/版本拒绝及可信owner gate；本文件不发布审批。

## R78-10a：章节引用与维护

| 方案 | 不同方向 | 优点 | 代价与约束 |
| --- | --- | --- | --- |
| A | 人工逐项复核并修链接 | 改动最小 | 再拆分仍易出现“文件存在、章节不归属”的错误 |
| B | 扩展现有docs checker，验证高风险引用的章节归属和锚点 | 沿现有门禁，针对本次真实错误 | 规则须区分live引用与带日期历史快照，控制误报 |
| C | 从稳定章节ID生成导航链接 | 重组可以自动更新 | 新ID/生成流程需维护，避免复制权威内容 |
| D | 模板生成Spec/ABI的共同引用段 | 一处更新多处导航 | 生成边界与人工正文混排更复杂 |
| E | 全部引用固定revision/SHA，发布新修订时更新 | 历史证据可精确回看 | live合同频繁变化不适合全冻结，维护开销高 |

**推荐B。** 复用check_docs.py，用当前索引中的canonical路径验证关键章节和锚点；先覆盖本次§1.1/§5.3/§10错位。
只维护导航归属规则，不生成第二份字段合同；带日期历史报告/旧兼容入口按文档政策处理，不机械重写历史行号。
规则与有意义的错链样例在后续工具实施时落地；当前只是方案，不声称checker已能发现所有语义错链。
验收：错文件但文件存在、锚点缺失、章节移位负例与合法historical引用；保留一H1、可达性与状态合同检查。

## R78-10b：公共状态与fault映射

| 方案 | 不同方向 | 优点 | 代价与约束 |
| --- | --- | --- | --- |
| A | 保留Playback原状态，加candidate owning Gameplay状态/故障来源查询 | 旧API保持，两个状态域不混淆 | 需完整公共映射与操作许可矩阵 |
| B | 新统一公开状态机，枚举全部组合 | 单一状态入口直观 | 组合数增加，可能改变旧state语义/签名，需版本迁移 |
| C | 状态不扩展，所有Gameplay故障只经typed诊断查询 | 公开类型较少 | caller需组合诊断判断许可；不能靠message文本推断状态 |
| D | 以机器可读状态/操作表生成query映射与拒绝逻辑 | 文档与实现能一致审查 | 生成表需成为明确合同输入，初版工具成本较高 |
| E | 每个操作返回owning状态回执，最后回执作为恢复依据 | 调用结果直接携带故障边界 | 回执存储/替换与空query规则更复杂，仍须保留真实fault原因 |

**推荐A。** 旧Playback SessionState含义保持，candidate查询单独说明inactive/configured/prepared/running/faulted等公共语义，
具体闭合集在ABI首用冻结；不直接安装内部enum，不按值强转。错误保留集中code/category/severity及实际faultStage。
Gameplay fault与Playback Failed分别映射；kernel/Fold/control来源以现行execution/J0为准，不能套旧“只有Ruleset fault”。
对submit/advance/update/query/prepare/commit/Seek/restore/Replay/reset/unload和显式替换列success/reject与状态保持，
区分已有sealed前缀、旧Fold与旧Frame；表现失败不产生Gameplay faulted。owner-thread/reentry/Result规则沿现有合同。
验收：状态笛卡尔组合中可达/不可达项、三种fault来源、纯表现错误、faulted查询/reset/新会话替换、默认纯播放兼容。

## 推荐组合、写回顺序与停止点

推荐向量：R78-01 A、02 A、03 A、04 A、05 A、06 A、07 A、08 A、09a B、09b A、10a B、10b A。
09a的B只扩展filesystem多文件发布，现有单CXC的A发布路径继续复用。
它与原P78方向共同使用；R78-01细化P78-02/03，R78-02/04细化P78-06，R78-06/07细化表现桥，
R78-08沿P78-04 JSON选择，R78-09b/10b控制公共表面与版本。不得从不同备选中暗拼语义。

先落公共操作/故障/状态与失效合同（01/02/03/10b）；内容格式/解析界与工具发布（08/09a）；
再落control/投影重建与层级/lifetime（04/05/06/07）。公共API差异形成后执行09b，
文档check加强10a可独立实施；仍按I78-0–5依赖安排验证，I78-6只累计S7A-9准备材料。

每项写回对应C78所列Spec/ABI/Playback/格式/表现/版本合同，明确字段/owner/reset/identity/诊断与正负例。
选择推荐不使R78退出；合同落定、实现与证据分别登记。新阈值、跨层语义、公共版本及owner门禁不得被普通表示授权绕过。
保留F(H)、future pending/lastObservedTick、archive/cut/H checkpoint与完整ReplayEvaluation，不重开既有5/6语义。
不进入7B+/S7C、Stage8发行、生产预算接受或Stage7A关闭。

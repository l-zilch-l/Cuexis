# S7A-5 / S7A-6 Joint Implementation Handoff

状态：historical planning snapshot；原S7A-5/6联合接手正文，当前执行安排已归入主计划

日期：2026-10-06

> 归档日期：2026-10-06；来源为b5594cf保存的规划正文，链接随目录迁移修正。
> 本文保留当时的九项方案、执行卡和指令；当前十项残余细节与实施安排以
> [主计划§3.2](../../stage_plans/active/stage-07/plan.md#32-s7a-5--s7a-6-联合实施目标决策和准入) 为准。
> 当前接手文档导出到owner桌面；下文的旧接手指令不再作为独立维护入口。

## 1. 目标、基线与阅读顺序

在D:/Cuexis-worktree、现有stage-7分支完成两个批次：

1. S7A-5：owning PreparedRuleset、有限内置Fold、三类Register、grade/Score/Combo/Statistics、t+1 Hook。
2. S7A-6：四分量identity、canonical Replay、全量owning Snapshot、原子restore/Seek与真实双路径等价。

同一会话联合验收，解决5.5与6.3–6.5的跨批恢复要求。
受限功能验收与容量整体分别报告；S7A-9 INCOMPLETE GATE不由本轮关闭。
S7A-7/8/9、Life、package、correction、连续能力和SDK0.7.1不进入本轮产品实现。

代码行为基线a0c8b7e4f783995bc19e626f3654ab11f845a0a7；854efa3是先前单批规划提交，
owner随后澄清为“多个批次 + 未定决策4–5套方案并选优”，以本文件和
[方案比较与选择](../../proposals/implementation-input/stage-07/s7a-5-6-design-selection.md) 为当前接手输入。
开工先核对HEAD/branch/status，基线须为祖先；保留后续文档、用户改动和现有缓存。
SDK仍0.7.0。阶段状态只由 [CURRENT_STATUS](../../CURRENT_STATUS.md) 维护。

阅读顺序：

1. AGENTS、CURRENT_STATUS、[总计划](../../stage_plans/active/stage-07/plan.md) 的5/6与小目标台账、
   [五批交付评估](s7a-5-9-delivery-plan.md)。
2. [方案选择](../../proposals/implementation-input/stage-07/s7a-5-6-design-selection.md) 的组合B与九项推荐；不要重做已裁定语义选择。
3. [V2 Spec](../../formats/GAMEPLAY_V2_SPEC.md) §3.14–3.22/§3.25/§5/§8，
   [V2 ABI](../../api/GAMEPLAY_V2_ABI.md) 域5–7/9、未决项及5/6限定冻结节。
4. [第5轮](../../stage_reports/stages/stage-07/2026-10-03-s7a-5-6-gate-rulings.md)、
   [execution profile](../../formats/gameplay-v2-execution-profile.md)、
   [execution types](../../api/gameplay-v2-execution-types.md)。
5. kernel_types、judgement_session、execution_kernel、input_boundary、ingress_transaction与
   execution_reference及其fixture；本文件中的模块拆分是实施建议，不是已有实现。

当前只有typed configure/prepare/submit/advance/query/reset具备3/4执行行为；
SnapshotPayload是owning角色载体，snapshot/seek仍拒绝。
旧无参数动词及旧注释不能当作typed成功或恢复证据。
模块仍internal/noninstalled，本轮不改默认SDK导出或Playback产品入口。

## 2. 九项明确推荐和实施前门禁

| 决策 | 选定方向 | 首卡必须补齐的合同与证据 |
| --- | --- | --- |
| P56-01 / R1 | owning typed manifest/有限内置操作 | 所有字段/owner/值类型、programPolicy/outcomeScope、缺失与重复负例、identity投影 |
| P56-02 / G2 | 有符号tick区间grade table，seal前求值 | 区间端点/覆盖/Miss/error absence政策；旧absent profile与新grade profile/revision |
| P56-03 / N1 | i64 Score、u64计数、明示checked/clamp、白名单monoid | 初值/增量/break/范围/operator单位元、contribution identity、溢出与分组golden |
| P56-04 / T2 | 独立Fold候选 + 单次发布 + fault reserve | 两游标、perTick消费点、无分配发布、kernel与Fold失败的两种前缀 |
| P56-05 / S2 | Kernel/Ingress/Fold全量owning DTO | Spec十四项→实际成员逐项表、借用转own、引用闭合和恢复原子性 |
| P56-06 / W2 | 有序section、LE定宽record、SHA256完整性 | 两套完整byte表、tag表、版本轴、负例优先级、跨工具链人工golden |
| P56-07 / E2 | accepted事件/control journal + Ledger校验 | batch/sequence/admission/advance/cut字段、shared normalize路径、转发复算、错误责任 |
| P56-08 / K2 | 明确target cut、全量snapshot有序索引 | horizon与frontier区别、同horizon晚到最小反例、从起点oracle与nearest snapshot条件 |
| P56-09 / B2 | measurement状态与业务接受分离 | descriptor必需字段、pending生产路径、测试门禁隔离、事实counts与上限区分 |

上述推荐足以开始选定方案内的字段补充；不能再把整个profile/codec选择原样推迟。
九项推荐尚不是生产冻结，首卡先将消费字段写入相应Spec/ABI/profile。
正常字段命名/布局选择可在本会话内完成；若推荐与既有裁定实质冲突，给出最小反例、
影响范围和替代处置，先修该项，再消费。不会因此重新开放Life/package/连续能力。
不替owner执行D-9 master落库、版本放行或阶段接受。

## 3. 顺序实施的八张卡

### J0：统一字段、版本和最小反例准入

输出九项字段/owner/读取时点/初始化/reset/identity/codec/诊断表。
设计文档持有对比方案；Spec/ABI补充持有最终消费字段；dated报告持有实际验证，三者不要互相代替。
为grade、两个commit cursor、pending signal、Replay target cut和预算状态各写人工正反例。
列明新/旧profile允许组合、FactSemanticRevision与stateSchemaRevision各自变化原因。
为Replay/Snapshot确定完整schema与envelope字节表后才写首个Reader/Writer。

退出：没有需要猜测的必需字段、隐式初值/grade/预算；所有实际消费字段具名且可校验。
J0完成不代表S7A-5或6已实现。

### J1：PreparedRuleset、grade与configure/prepare

建议内部文件：ruleset_types.hpp、ruleset_prepare.hpp及对应src；按现有目录/ASCII/Result规则实施。
prepare冻结ordered manifest、内置module注册、Interface、register声明、有限operator与明确评分配置。
缺module/outcome/grade、未知operator、重复owner/contribution、Life/package/correction、
policy/scope非法和配置identity错配均原子拒绝，旧prepared/configuration保持、不fault。
package拒绝在读取其内部hash/manifest前发生。

G2表与Measure声明绑定，验证各phase覆盖、端点与Miss/error absence。
在Fact seal前产生一次optional grade；Fold不重判/改写Fact。
保留旧3/4 grade absent路径与golden，新行为独立profile登记。
Ruleset/Measure/engine/session变化按Spec§5.2分别投影，非语义排列不污染identity。

退出：PreparedRuleset/grade字段长期自持有；两个profile正负例和identity对照明确。

### J2：perTick Fold、Register、结果状态和t+1 Hook

在execution_kernel的每Tick seal后调用已链接的有限Fold实现，先计算候选状态再整体发布。
一个Tick可处理多个Fact，但exclusive只发布一次最终write；monoid贡献identity去重与分组不变性明确。
贡献键按P56-01具名，来自register/module/Tick/optional FactId/localOrdinal，不使用到达序列。
未知operator、第二写、重复贡献、ledger_derived直接写、数值/分配/测试预算失败逐项稳定拒绝。
按N1实现Score/Combo/逐category/outcome/grade统计，包含absent、负值、Miss break、长流和明示饱和。

保留sealedLedgerCursor与foldCommittedCursor。kernel未seal失败保持旧kernel/Fact前缀；
seal后Fold失败保留新Fact/旧Fold，不追加fault Fact、不发布本Tick RuleEffect/PresentationEvent。
预留失败载体，失败后查询仍可成功取到旧state/新Fact/稳定原因；发布不产生额外可失败分配。
成功Hook下一Tick可见，同Tick看不到；snapshot保存未来所需信号值，不保存RuleEffect事件cache。
reset清空新run、不产生Fact；旧owning query继续有效。

建立独立Fold reference，只读Fact和明确Ruleset声明，不调用生产combine/reducer/排序/事务helper。
先以人工golden验证reference，再与生产逐字段对照；5的已实现行先回填，
5.5真实seek/replay行在J5–J7完成前保持IU，不能提前写整批完成。

退出：5.1–5.4的正常/失败路径、5.5只读/reset与Ledger重建基线成立，状态模型可交给6消费。

### J3：四分量identity、codec与Replay journal

建议内部replay/codec模块，复用core现有内部SHA256，不引入第三方序列化或JSON DOM依赖。
W2按已冻结byte表编码，EventCodecId与eventCodecId拼写一致；
FactId/CommitId、资源/phase/outcome/origin tag均显式映射，不memcpy native结构。
先完整预检长度/count/section/identity/版本，再allocate到候选owning records。

记录accepted输入、batch/sequence、admissionFrontier/Horizon、observation/dispatch/forwarded、
advance horizon与journal cut；保存已提交Fact Ledger供逐commit校验。
live/Replay从同一normalize/admission/submit/advance路径消费，不从文件Fact或dispatch直接造成功结果。
header metadata须与复算结果一致，重复/乱序/截断/篡改/未知tag或版本/不闭合引用整体拒绝。
control journal是复现上下文，不成为Fact排序键或另一个ingress规范顺序。

退出：6.1、6.2与6.5的Replay独立golden/实时对照基线成立，错误保持旧active。

### J4：全量Snapshot与restore

建议snapshot_internal/state_capture模块；按S2逐成员capture/validate/restore，不把KernelProjection当全状态。
包括private matcher、coverageHistory、timer/ID exhaustion、ingress owners/去重/latestTick、
pending admission、two cursors、Fold与未来signal值、session/fault字段及journal cut。
保存Fact前缀与实际preparedGrace；重新取得prepared graph/FactBinding，不存其图副本。
所有跨record引用、sequence/时间关系、timer cursor、DFA状态范围和Ledger/统计一致性逐项校验。

snapshot自持有，在原session销毁后仍可独立检验，并恢复到新session；
runScope/地址/owner thread不序列化，新session建立自己的owner与scope。
Reader/restore全量构造后单次替换，失败保持旧active/query/snapshot。
faulted live session禁止新snapshot/seek/就地恢复；schema包含fault字段不构成绕过。
不保存Effect/Presentation cache、Animation临时值、HostOverride、未提交delta或连续采样相位。

退出：Spec十四项对账、跨工具链bytes与恢复后同尾部输入逐字段相等；6.3原子性完整。

### J5：target cut、最近快照Seek与重建

先用J0选定target cut最小反例验证目标是advance horizon、不是sealed frontier。
快照索引按(horizon,cut)有序；两个约束同时满足才选为最近可用checkpoint。
同horizon更晚admit输入不能进入更早target cut；无快照从起点fallback。
未advance的horizon保持absent；负Tick目标不能与隐式0起点混用。
source control的跨目标advance截到目标，不能先把所有输入预提交再seek。

用新候选session恢复、复放control、从已提交Ledger重建Fold/统计/只读表现投影，
验证活动实例、资源/Release/timer、sequence、grade/IDs、pending信号和未提交窗口。
表现token/Playback adapter实现归7，本轮只检验内部恢复输入与已提交FactBinding的投影DTO。
完整成功后替换；任一步失败保持旧active。Seek不会重分配已有timer/Fact IDs。

退出：负Tick/空输入/多间隔/同horizon晚到/queue_next_tick/有pending输入/多次前后seek
与独立起点运行oracle逐字段一致；6.4与5.5真实恢复门禁可回填IV。

### J6：联合失效矩阵、fixture与回归

区分格式预检失败（旧session unaffected）、真实kernel未seal失败、
seal后Fold fault、恢复候选失败和faulted操作拒绝。
成功前缀之后失败、失败后的旧query/snapshot存活、reset与新session替换必须各有断言。
分配失败注入覆盖每个可能分配点；不能在错误返回时再次分配以致异常跨公开边界。
grade变化与Score变化分别定位identity分量；stateSchemaRevision仅改载体，不改judgement identity。

保留全部T4/K4、L1/L2、S1/S2、inline/CXT author、affine、Capsule2/3及旧fixture。
静态内置Signal fixture不能替代真实Ruleset Hook验收。
real-time/Replay/Snapshot restore/Seek四路径比较Fact内容/总序/IDs、完整状态/统计和错误，
不能只比较score/hash或把两个共用helper的结果当独立证据。

### J7：完整验证、计数与退出报告

为Ruleset/Replay/Snapshot/Seek登记独立label，先枚举非零清单，再执行；
Catch case不是executable regex。运行Debug/Release完整矩阵、headless/shared/MinGW、
Linux GCC/Clang及既有hosted质量组合；sanitizer/coverage只在支持平台按仓库方法执行。
旧Playback安装static/shared consumer作为回归，新增Judgement产品导出仍归7。
检查architecture、allowlist、ASCII、Result/异常、format、docs/status/target、版本与git diff --check。

生产只计数、不预选预算：记录register/module/贡献、Fact/状态/delta/signal峰值、
Replay/event/byte/decode、snapshot/restore/Seek分布；budget分支以test-only accepted fixture验证。
未接受生产阈值保持INCOMPLETE GATE；具体maxSeekLatency请求仍拒绝。
统计/计数表示溢出与预算超限分别报告；现有Packed limits不改变。

逐行回填下表，形成新的dated报告与S7A-7接手包。
涉及行为更改的证据绑定新行为SHA；旧a0c8b7e hosted只作基线。
如日期build需升级，经update_version同步并fresh/clean-first；SDK0.7.0保持。
提交/推送按会话授权，不重复申请已经授予的授权；按owner要求决定是否等待CI。

## 4. 十个小目标的最终联合核验

| 行 | 主要交付卡 | 必须具名的退出证据 |
| --- | --- | --- |
| 5.1 | J0/J1 | owning manifest/Interface、profile字段、非法owner/policy/outcome/package原子拒绝 |
| 5.2 | J2/J6 | 人工多Fact总序、两个cursor、两类失败边界、无半提交/发布/fault Fact |
| 5.3 | J1/J2 | phase-local grade/absence、显式分值/初始化、Miss break、负值/长流/饱和、旧profile保持 |
| 5.4 | J2/J6 | 三类Register、单位元/交换/结合/贡献去重、exclusive第二写、t/t+1与失败不发布 |
| 5.5 | J2/J4/J5/J6 | owning统计、reset、真实Replay/restore/Seek与Ledger重建逐字段一致，不再将这行整体留6 |
| 6.1 | J1/J3/J6 | 四分量正反例、prepared value/source差异、FactSemantic/stateSchema独立轴 |
| 6.2 | J3/J6 | 两套header对应字段、EventCodecId、完整byte golden、乱序/重复/截断/身份/结构/测试预算负例 |
| 6.3 | J4/J6 | 十四项→实际成员、原session销毁后校验、同尾部输入一致、faulted限制与旧active保持 |
| 6.4 | J5/J6 | 明确target cut、最近可用全量snapshot、任意点/间隔/late/pending输入、原子失败与起点oracle |
| 6.5 | J3/J5/J6 | live/Replay真实同输入路径、Fact/grade/IDs/Score/Combo/Statistics/错误全量比较 |

行级IV/IU/M/LB保留准确语义；已实施功能与9的生产容量/承诺门禁分开。
未退出功能行不能写整批完成；预算整体未接受也不能写Stage7A完成。
下一轮止于5/6受限功能验收与7的交接，不自主扩展7–9。

## 5. 接手证据、清理与来源

3/4 hosted和已归档临时证据见
[hosted/归档报告](../../stage_reports/stages/stage-07/2026-10-05-s7a-3-4-hosted-and-handoff.md)。
其“下一轮仅5”是854efa3当时计划，后续排期以本文件为准；CI与归档事实仍有效。
out/build缓存、用户备份和早期非本次证据保留；不要git clean -fdx。
本轮没有新产品代码或C++运行证据。

## 6. 新对话可复制指令

> 在D:/Cuexis-worktree、现有stage-7分支，按s7a-5-6-implementation-handoff.md联合实施
> S7A-5.1–5.5与S7A-6.1–6.5。先读CURRENT_STATUS/AGENTS/Spec/ABI与方案选优文档，
> 采用组合B和R1/G2/N1/T2/S2/W2/E2/K2/B2。按J0–J7先闭合并登记字段/owner/revision/byte表，
> 再依次PreparedRuleset/grade、perTick Fold、codec/Replay、全量Snapshot、Seek与联合验收。
> 普通表示细节在本会话补齐，不把已选设计原样推迟；真正合同冲突给最小反例与明确处置。
> 复用已seal Fact，分开kernel回滚与Fold失败；保持两个cursor、旧owning查询与t+1信号。
> 恢复私有ingress/matcher等全量状态，Replay复算late admission，Seek按明确target cut选快照。
> 使用独立Fold/起点恢复oracle、人工bytes golden、故障注入和跨工具链矩阵逐行退出十个小目标。
> 本指令授权相应实施与验证；不实施7–9/Life/package/连续能力/SDK0.7.1，预算整体留9。
> 保留用户改动；提交/推送依会话授权，已授予的不重复申请；完成5/6后形成7的接手包并停止扩展。

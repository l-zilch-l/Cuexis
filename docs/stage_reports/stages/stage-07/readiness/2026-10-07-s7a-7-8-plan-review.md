# S7A-7/8 Plan and Contract Review

状态：reviewed planning snapshot；引用已修正，消费者合同待首用冻结

审查日期：2026-10-07

## 范围与证据口径

审查 [Stage 7A计划](../../../../stage_plans/active/stage-07/plan-a.md) 的S7A-7/8、§3.1/§3.4，
总计划、V2 Spec/ABI与execution补充、Playback/Presentation/Animation合同、Entry/Capsule格式、
版本政策、四项交接报告及桌面接手。实际分支stage-7，HEAD为
`55fc8e6920e7ddb4b34a56bd0b9c27d07d60b833`；保留未提交的规划、分册与索引修改。

以下反例是合同推导，未运行新产品代码复现，不是已证实的运行时缺陷。没有重新查询PR/CI，
不将原38项成功记录作为本次新证据。R78是本次审查追踪号，不新增capability、码表类别、稳定R编号或ABI角色。

阶段范围和四层边界可保留；15项P78及12行C78仍有价值，但不证明消费者合同已经闭合。
本次登记10组问题：R78-01–08在受影响消费者前闭合，R78-09为交付与版本核验，R78-10为已修导航及现行口径提醒。
P1表示实施前阻塞，P2表示交付/验证/文档待补，不作为已运行产品的故障评级。

## 发现、反例与解决方向

### R78-01（P1）：组合advance的部分提交边界

P78-02/C78-02定义load/Seek/Reload组合事务，C78-03列出双时间域，但未给组合推进的逐阶段失败表。
反例：Gameplay已封存至H，RuntimeFrame校验或表现更新随后失败，宿主把普通失败理解成旧状态未变而重试同一batch。
建议先预检时间/discontinuity/batch及可预留容量，再沿J0封存规则推进；Playback lifecycle/ABI必须说明
每阶段失败是否已准入/已封存、可查询的H/frontier、允许的下一操作及独立表现诊断。
不能把Prepared全量回滚套到已封存运行Tick。验证前置、kernel、Fold和推进后表现故障，完整比较query/archive/帧。
归C78-02/03/04/09，I78-2/3；依据[Spec §3.14与J0-6](../../../../formats/GAMEPLAY_V2_SPEC.md)。

### R78-02（P1）：sealed Ledger前缀不是可发布表现前缀

[Spec §3.14](../../../../formats/GAMEPLAY_V2_SPEC.md)规定Fold失败保留Fact，但不执行第三阶段；查询/恢复可只读重建。
P78-06的sealed prefix游标缺少该限制。反例：Hit已封存而Score失败，adapter按Ledger.size扫描并生成visible token。
建议实时发布以成功Fold提交边界为限，无Fold模式另行明示提交边界；只读重建与Runtime激活必须分开，
faulted查询不得修改画面。不新增Fact/持久化token，不读未提交delta。验证失败Tick Fact存在、旧Score保持、零新token。
归C78-07/08/09，I78-2/3。

### R78-03（P1）：Gameplay变更的Prepared失效表

[Portable Presentation §9](../../../../formats/PORTABLE_PRESENTATION.md)已有update使旧Prepared过期；
C78-02提组合generation，未逐操作说明新增submit/advance/control的影响。
反例：prepareReload后submit接受future batch，尚未更新帧，commit旧候选丢掉已接受输入。
建议列mutate/generation/候选与borrowed view失效表；已接受零Fact batch也须检查，查询与失败预检不伪造变更。
generation不进入JudgementIdentity。覆盖prepare→新增操作→commit stale、owner/move/discard/重复commit。
归C78-02/04/08，I78-2。

### R78-04（P1）：Seek/分支的去重作用域与整体重建

既定键(factId,targetId)保留，但不可跨Ledger全局复用。反例：Seek回退后旧future队列仍在，新分支复用FactId，
旧去重集抑制新投影或旧队列写到新对象。C78-04/07尚未列cursor/去重集/队列/聚合状态/目标绑定的整体替换。
建议按组合实例/当前Ledger隔离临时状态，域内仍用原二元键，不增加隐藏bindingId或修改Fact身份；
候选重建完成后一次替换，失败保持旧投影。验证same-H、同长度不同Ledger、多次回退/前进、reset/unload/destroy。
复用archive/cut/H和惰性分支，不只比较Ledger长度。归C78-04/07/08/09，I78-2/3。

### R78-05（P1）：同判定身份、不同表现绑定的恢复

[Spec §5.2](../../../../formats/GAMEPLAY_V2_SPEC.md)隔离纯表现身份；Snapshot不存FactBinding/token。
反例：只将目标A改为B，判定四分量不变；恢复旧Snapshot却沿用旧resolver，把Fact继续投影到A。
建议分别定义换表现、同内容重载、换判定内容的控制策略，重新取得当前Prepared绑定/表现身份/资源并重建投影。
操作若要求更严格内容身份，可明示拒绝，但不能把纯表现字段偷偷放入判定四分量。
验证只改表现时完整ReplayEvaluation相同、投影按当前绑定；坏资源仍原子失败。
归C78-04/05/07/08/09，I78-1/2/3。

### R78-06（P1）：GameplayOverride层不是任意Host priority

[Animation Mixing](../../../../formats/ANIMATION_MIXING.md)有Host整数priority与冲突规则，
[Spec §3.23](../../../../formats/GAMEPLAY_V2_SPEC.md)有独立Gameplay命名层。
反例：adapter选大整数，宿主同值或更高priority，使层顺序/冲突受宿主偶然输入影响。
建议typed resolver明确层域、层内排序及Host相对Gameplay/Studio关系；保留旧Host合同，不能用无保留规则的magic priority。
不借条件编译改变默认公开布局。验证极值priority、同层冲突、false及Animation/Host/Gameplay/Studio竞争。
归C78-07/12，I78-3/4；先修表现合同。

### R78-07（P1）：lifetime的Tick端点和跨区间跳跃

旧Host lifetime有RemainingFrames及double UntilChartTimeMs，而C78-07要求不同帧率等价，具体映射未定。
反例：30/144 fps的相同frame lifetime不同；一次跨过整个有效期，先创建再释放产生错误单帧反馈。
建议Gameplay队列/到期用明确typed PresentationTick，先判有效区间再投影，不以帧数或浮点ms隐式换判定时间。
首用合同选定端点、同Tick顺序、零长度/非法lifetime、暂停/Seek及跳过区间行为，不改旧Host API含义。
验证early/exact/late、负时间/极值、不同帧率与大步跳跃。归C78-03/07/11，I78-2/3/4。

### R78-08（P1）：Graph JSON的前置资源界和整数保真

P78-04推荐JSON Graph，C78-05提整数/未知字段，未把分配前校验与Packed既有物理界的关系列为退出项。
反例：深嵌套JSON在语义校验前耗栈；数组先分配再被closure拒绝；2^53以上Tick经double失真；重复键被DOM覆盖。
建议Graph格式先定义版本分派、raw bytes/解析深度/count/text/decoded边界及适用已有上限；
不能以预算归S7A-9忽略安全界或默认接受新数值。必要新阈值未接受时单列首用门禁。
Reader在正确层拒绝重复键/非法整数/未知必需字段/错revision，i64/u64不经f64；正负例与错误顺序都有人工golden。
归C78-05/06/10，I78-1；[Spec §8.1](../../../../formats/GAMEPLAY_V2_SPEC.md)和[Capsule](../../../../formats/GAMEPLAY_CAPSULE_V2_FORMAT.md)既有界不放宽。

### R78-09（P2，版本不兼容时P1）：多产物发布和SDK版本重新核验

不推翻Foundation CLI/0.7.1现有候选证据。若输出project目录中Graph/Packed/manifest，逐文件rename并非整体原子。
C78-06须选单CXC发布或版本目录/manifest唯一commit点，明确崩溃、并发、旧输出和临时文件清理；
验证Graph成功而Packed/资源/manifest失败后旧入口仍完整，pack/unpack不隐式compile。

0.7.1目前证明的是Entry兼容新增，不能自动认证未来Gameplay公共API。
C78-12按[VERSIONING](../../../../guides/VERSIONING.md)盘点方法、ON/OFF声明、DTO及符号；
布局、默认行为、重载、聚合初始化和继承实现均证明兼容；不兼容先提minor与迁移，由owner接受，不预设新号。
新公共头不直接alias/install内部执行图。旧consumer和ON/OFF static/shared矩阵按实际API验证；
审批绑定最终base/head/tree/date，旧tuple不沿用，可信bootstrap/保护仍由owner完成。
归C78-06/12，I78-1/4/5。

### R78-10（P2）：拆分导航和现行适用范围

已修正：ABI的plan-a §10、Spec的plan-a §5.3和两合同的plan-a §1.1分别改指总计划、plan-b和总计划。
plan-a“本文S7A-7限定冻结”改指真正拥有该章节的Spec/ABI。仅验证目标文件存在不足以发现章节错位。

旧Spec §9.8“只有Ruleset产生faulted”是早期范围；execution已有kernel失败，J0-6已有FaultStage。
本次给旧段落加现行来源提示，保留日期/裁定/计数；Playback SessionState::Failed不能未经矩阵映射等同Gameplay faulted。
新增公共状态/错误映射仍须C78-04/08/10在I78-2冻结。

## 首用退出与停止边界

R78-01–08不能靠P78已推荐、C78已列字段或台账open=0退出。消费者开工前修对应Spec/ABI/格式/Playback/表现合同，
附最小正负例；逐行绑定C78/I78和小目标，记录合同版本、SHA、fixture、命令、原始输出和未执行项。
普通表示沿已有授权推进，真冲突先修合同。本次未冻结新wire/Graph预算/版本/跨层语义，未实施产品代码。
不进入S7A-9预算接受、Stage8发行、7B+/S7C或Stage7A关闭。

## 文档验证

后续应owner要求形成[各五方向方案与选优](../../../../proposals/implementation-input/stage-07/s7a-7-8-review-options.md)：
R78-09/10的两类子问题分别比较，共12项/60方案；当前推荐归plan-a §3.4.7。推荐不使本报告所列首用门禁退出。

check_docs.py通过：382份Markdown、20份candidate JSON/CXT；状态合同检查4/4通过；git diff --check通过。
桌面附录逐字对照当前plan-a §3.4（仅相对文件链接转绝对路径），15项P78、12行C78、7张I78与10组R78齐全，全部本地文件链接目标存在。
本次检查仅证明文档完整性及快照一致性，不证明消费者行为、产品矩阵或生产容量已通过。

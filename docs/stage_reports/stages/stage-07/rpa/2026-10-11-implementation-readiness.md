# RPA 实施规划就绪审查

状态：design readiness record；B0设计闭合，未实施、未完成RPA验收

日期：2026-10-11（Asia/Shanghai）

## 1. 范围、授权和结论

owner先选择K1，随后明确剩余事项除极难决策者由代理合理给出方案。本轮据此完成合同和计划，
不继续逐项等待普通选项点选。历史直接选择与委托决定分别记录于
[RPA补充Spec](../../../../formats/rpa-execution-contract.md)和
[ADR0046](../../../../adr/0046-realtime-playback-coordination.md)。

规划已具备明确实现路径：B1基线/oracle→B2存储→B3时间/执行/版本→B4宿主/资源→B5线程/设备，
具体小批次、输入、退出和回退归[RPA计划](../../../../stage_plans/active/stage-07/plan-rpa.md)。
剩余参数有M-TIME/TAIL/SPLIT/CAP的测量与拒绝启用规则，不作为空白语义留给实现者补默认值。
本结论是**实施规划就绪**，不是产品、生产预算、实体设备或阶段关闭结论。

工作身份：`stage-7`，HEAD `7ca1fc6cf2dd4c7072aab3ae260ee5931f5df2d3`，
SDK `0.7.5` / 日期构建 `26.10.10-1`；工作树包含此前未提交源码/测试与文档。
本轮只修改文档，不以HEAD单独冒充整个dirty工作树的运行验证身份。

## 2. 决定和维护成本

| 决定 | 采用理由与保留代价 |
| --- | --- |
| B职责边界、P-B部署、D2/D3首版共置 | 唯一kernel/Fold与单owner；隔离GL/音频等待，先减少控制参与域；主线程捕获延迟仍是明确限制 |
| K1与单等待槽 | 不增加挂起/复活状态；控制失败后宿主须重新请求推进 |
| D6/D2双序号与分阶段确认 | 可复核并发时实际裁决点和丢回执重试；增加两套序号、查询及退休前缀的维护成本，B3/B4按R28–R34验证 |
| 显式epoch cut/drain | Pause不依赖不存在的后续Tick观察；带来独立控制记录/partial-prefix，不能假称全事务回滚 |
| R2控制后缀、deferred关联 | 能处理暂停真实中断与cut前future press；代价是新Fact语义和Recovery载体 |
| P3-a内部候选 | Prepared单owner不变，GL继续主线程；需明确generation/fence及设备部分失败 |
| 连贯观察、一次offset | 缓存样本年龄可辨、避免重复延迟扣减；生产有效期/误差必须测量 |
| F1+F3可观测 | 不承诺首见，提供present证据/未知计数；RemainingFrames保持原语义 |
| 分块追加/owning结果 | 控制历史复制和保留成本；仍需测量活跃尾块、旧handle和最后释放 |
| 审查修补A1：切段后准备失败终止直接续跑 | 减少半完成恢复的状态和重试；代价是故障后仅能显式退出或恢复到新会话 |
| 审查修补B1：owner串行Apply/Cancel | 终态未生效保证迟到Apply不执行；增加按步骤终态记录及回收确认，复用既有单owner |

没有采用全流水线、通用任务图或整体不可变表现重写；理由与重开条件见ADR。

## 3. 纸面反例与确定预期

下面是后续必须实现/运行的验收场景，不是已通过测试的清单。C=3等数字仅为testOnly示例。

| ID | 场景 | 预期与落点 |
| --- | --- | --- |
| R01 | 同Tick同来源press/release/press；两来源和弦 | O1来源连续、I1区分发生；timer阶段不交错；完整结果/Fact总序可复算 |
| R02 | 转发900 press与准时901 release均dispatch901 | L1合批，原始Tick/I1不变；仅共享dispatch不再拒绝 |
| R03 | 最后分片缺失，后续Prefix先到 | B不前进；收到完整片并确定准入/拒绝后才确认 |
| R04 | 同传输ID重传；同I1槽异值 | 传输重传不重执行；同槽异值原子拒绝，旧pending保持 |
| R05 | H105/C3，B102后到B103 | 等号不许可；最终只执行原105，不偷偷advance103 |
| R06 | H105占槽，再请求106/107/另一个105 | 新请求BusyNotAccepted；不自动重放，不伪造成功 |
| R07 | 有效Pause接入且105尚未执行 | K1取消105；准备失败不复活；若105已开始则完成实际结果后处理控制 |
| R08 | Pause时最后一个原始Tick批无后续观察 | EndRunEpoch收齐cut，inclusive q；ControlDrain到q；不合成q+1 |
| R09 | q+C溢出、Tick极值无法恢复 | 拒绝相应drain/续跑；保留结果与显式退出路径，不饱和不假成功 |
| R10 | body Pending暂停release；body Hit/tail Pending暂停release | 分别body/显式tail Miss或仅tail Miss；旧Hit不改，error/grade absent |
| R11 | release再press回同最终键态；暂停新键 | 旧Hold仍中断；新物理按住quarantine，不获暂停Hit；真实释放后再press才能新开contact |
| R12 | 没有tail；完整trace没有转换；连续两次恢复 | 不凭空tail、不重复phase Fact；无转换保留；每次cut/ordinal清楚 |
| R13 | cut前future press，暂停中实际release | 保留原input/admission；dispatch时附带R2 consequence，不能永久held或提前丢输入 |
| R14 | focus/捕获缺口；暂时乱序可补交 | 可补交继续等；确认不可恢复则拒绝直接Resume，不猜最终键态 |
| R15 | R2已seal而Fold/音频失败；重复Resume | 保留实际新前缀并fault/suspend；重复不再次结算Miss；设备阶段可关联重试 |
| R16 | 同H=q的恢复前后Seek | archive cut区分控制后缀；完整Replay不接设备且逐字段一致 |
| R17 | seqlock返回缓存；rate变更或静音欠载 | 旧采样stamp不变；新段显式，失效撤销许可；device mixed不充当源进度 |
| R18 | 44.1k源/48k设备、offset正负、负Tick、ties/溢出 | 分域/各一次/half-even；不从RuntimeFrame反推，不二次扣输出延迟 |
| R19 | GPU candidate准备期间Session mutation；迟到旧资源回执 | generation重验拒绝旧候选；fence期间mutating操作暂停；GPU仅主线程 |
| R20 | SoftwareApplied后设备激活失败 | Suspended并报告实际阶段；不宣称恢复旧软件组合 |
| R21 | 完整帧合并、反馈过期、丢回执、迟到旧scope成功 | Feedback membership准确；过期/取消/Unknown分开；不保证用户看见，不续命RemainingFrames |
| R22 | 可靠通道及D1保留空间耗尽 | 预留故障槽记录gap并停推进；不丢键后继续声称完整前缀 |
| R23 | Close时主线程仍有GPU退休任务，外部旧CPU结果仍存活 | 先泵清理再join；不detach；外部结果独立保活不阻塞Close |
| R24 | EOF后仍有合法timer/窗口 | 显式TailProfile推进并取得完整结果；缺生产参数拒绝启用，不立即漏判或批量Miss |
| R25 | RPA rev3与旧rev1/2、旧execution token混用 | 稳定拒绝，原件保留；不能同revision写新字段或只比score判断兼容 |
| R26 | 已Fold到q后R2同q追加Miss，q已有Hook贡献/待消费bonus | 只消费新Fact；Hook读q起点view并合成同一q+1 due；Fact/control cursor推进，workTick保持q，不重复due或旧Fact |
| R27 | 正常已处理前沿P、旧许可B与Pause末批q跨段或倒退 | 只接受同段同epoch且P<B<=q的终止证据；否则控制准备失败，不以回退H或虚构q修补 |
| R28 | H105等待时B103与有效Pause并发抵达D2 | 若D2先标记Started则执行105的实际结果后处理Pause；若先接入Pause则K1取消105，后来的B不能复活；两条decisionOrdinal及Replay实际调用可核对 |
| R29 | D6已排队/传输控制但D2未接入，105已获得许可 | D6回执不算K1；D2先Started则105不回滚；请求、接入和开始的ordinal不得用设备回执重排 |
| R30 | D2接入Pause并取消105，D1终态未生效或CutApplied后drain失败 | RejectedNotApplied/CanceledNotApplied且无其它不可逆效果可保持Running，但105不复活；Pending/Unknown不能证明未切段；CutApplied保留cut/trace并Suspended |
| R31 | D1实际CutApplied但回执丢失为Unknown；同operationId重试 | 先查D1实际阶段；确认前Suspended，不再切一次epoch，切段后输入全部进入PauseTrace |
| R32 | D4/D5应用控制后回执丢失，D6等候超时 | 按原operationId/代数查询，确认Applied只补剩余阶段；不能重新软件提交、切段或把Unknown当NotApplied |
| R33 | 同requestId/operationId重复、异载荷、终态确认后旧ID重现 | 同值只查询原阶段，异值拒绝并撤销旧许可；退休前缀返回Retired，任何分支都不重执行 |
| R34 | 新scope的迟到控制/资源回执与Close清理同时到达 | 旧回执只留历史；不能解除新fence或报告Closed；主线程完成仍待其执行的退休任务后再join |
| R35 | Resume最终切换前准备失败；音频尚停住或已ResumeStarting；切换请求仍在途 | 切换步骤取得终态未生效确认且音频确认停住才可报告Paused；否则不解除限制，迟到切换Apply受B1拒绝 |
| R36 | Resume已切新epoch并暂存真实release，最终校验/reserve失败；停音频成功、失败或回执丢失 | A1：Suspended，保留原cut/ID/输入，未seal则不发R2 Fact；不重分类release、不准入执行；同/新ID Resume均拒绝，停音频不恢复资格，退出先隔离在途步骤 |
| R37 | 最终trace暂缺/回执未知；对照已确定失败、R2已seal后Fold故障及软件成功后设备故障 | 暂缺/未知按原操作等候查询，不误终止；确定准备失败走A1；已seal故障保留实际前缀，设备重试不重复R2 |
| R38 | Cancel先于第一次Apply，或query返回Pending后Cancel先被owner处理，原Apply随后到达 | owner先保存CanceledNotApplied再确认；后续首次/重复Apply均不执行；相同scope/步骤终态可查询 |
| R39 | Apply先于Cancel并处于InProgress，或已Applied；query/Cancel/回执乱序；对照已终结设备失败的合法重试 | 未结束不报取消成功；Applied保留副作用，补偿使用另一phase；合法设备重试新phase尝试不改旧终态，Unknown不许覆盖，不能回收仍在用资源 |
| R40 | 取消确认丢失、重复Cancel/query、终态确认后压缩退休记录、迟到Apply；预留记录槽耗尽 | 重发原终态/Retired而不执行；未能记录终态不得确认取消；不靠超时释放候选 |
| R41 | D1或D5已取消，而另一owner仍InProgress；A1清理与Close并发、旧设备启动迟到 | 各phase分别确认；单域取消不授权全域释放；拒绝已取消的启动，已开始者按实际结局隔离；清理未完成不报Closed或恢复Running |

覆盖归属：R01–R18/R24–R27主在B3，R19–R23主在B4，跨线程与真实设备版本在B5复验；
B1先提供fixture/oracle，B2提供完整结果与生命周期比较基线。运行失败应登记，不调整fixture制造通过。
R28–R31主在B3的可控裁决/切段测试，R32–R34主在B4的跨owner回执/重试/关闭测试；
B3.3/B4.4共同覆盖R35–R37的A1边界，B4.1/B4.3覆盖R38–R41的B1取消和回收。
B5按实际线程、设备和容量复验。所有R01–R41仍是未运行的纸面预期。

## 4. 替代关系和剩余门禁

被替换原文与落点见[B0记录§10](2026-10-11-b0-contract-revision.md#10-k1落约与委托裁定收口)。
Prepared说明中原“主线程校验/worker构造GPU”与P-B冲突，已改为worker校验/acquire、主线程GPU；
音频侧补齐owning候选/ClipStore/callback寿命。AD02说明的cursor差已更正为数量，年龄另用时间差。
具体wire字段顺序/golden、公共C++签名和消费者是B3.1的成对实现交付物，不允许消费者先跑后补合同。

当前剩余工作均为实施或测量门禁：生产TailProfile、观察误差/有效期、容量/峰值、拆分判据、
最终平台/设备矩阵、F3预算接受、F4 owner关闭接受。无需再点选普通架构方向。

## 5. 本轮验证

`python -B tools/check_docs.py`通过：424份Markdown、20份candidate JSON/CXT；
status/section/target三组定向文档合同测试分别4/5/2项通过；`update_version.py --check`
确认`26.10.10-1`；`git diff --check`通过（仅出现LF/CRLF提示）。
新增就绪记录到B0 §10的片段链接与目标标题一致；仓库检查器只验证路径，不自动校验片段。
七个既有源码/测试改动编辑前后diff摘要相同：`ba316274afa9ead2812292bbc6b78d404dd791df`。
未运行C++构建、CTest、性能/尾段测量、实体设备、听音或hosted；未提交、推送、建PR或变更分支。

## 6. 同日后续细化

本次续修针对三域交接的可判定性：Spec §1.3 固定 D2 接入/Started 的裁决顺序，
Spec §3.1 固定 D1 切段的三态及不可逆失败，宿主边界固定跨域 ID/ordinal、回执确认和
Applied/Unknown 查询重试规则；计划增加 B1–B5 的逐批测试清单与退出记录。
R28–R34 是新增验收输入，不是已执行测试；B0 仍只表示实施规划就绪。
本次续修的文档检查通过（424份Markdown、20份candidate JSON/CXT），status/section/target
定向文档测试分别4/5/2项通过，版本一致性检查通过；七处既有源码/测试diff摘要仍为
`ba316274afa9ead2812292bbc6b78d404dd791df`。未运行C++/CTest或设备测试。

## 7. 子代理复审后的A1+B1修补

同日三个子代理只读复审，主审交叉核对后确认两项设计缺口：P1最终Resume切换后仍可能
准备失败，却未定义实际路由/音频的收束；P2查询NotApplied不能阻止原Apply迟到首次执行。
owner明确选择修补A1+B1；方案取舍归ADR0046，A1权威规则归Spec §3.5，B1归宿主 §2.1。
该编号不等于实施批次B1、历史RPA-A1或尾段A1-b。

| 原条款 | 本次修补 |
| --- | --- |
| Spec §3.2“准备拒绝保留旧Paused” | 限于最终路由未切且切换步骤已终态未生效、音频确认停住；切后确定准备失败走A1 |
| Spec §3.3切换后重新校验/reserve | 保留正常流程；新增失败终止直接续跑、实际输入保留及显式退出，不提供半完成软件重试 |
| 宿主 §2“确认NotApplied才能撤销候选” | query为Pending/Unknown不授权撤销；owner串行Cancel形成终态，拒绝未来同一步骤Apply |
| 宿主 §3提交前失败取消并保留旧组合 | 限于无其它副作用且相关owner终态/寿命已确认；切段后的Resume按A1，不假回滚 |

计划已对应更新B3.3、B4.1/B4.3/B4.4的退出要求；R30修正终态含义，新增R35–R41。
两项审查发现完成文档层修补；产品正确性仍须上述故障注入通过，不能用本记录代替实施证据。

本次文档检查通过：424份Markdown、20份candidate JSON/CXT；status/section/target定向文档
测试分别4/5/2项通过，版本检查仍为`26.10.10-1`，`git diff --check`通过。既有七处源码/测试
diff摘要保持`ba316274afa9ead2812292bbc6b78d404dd791df`。未运行C++构建、CTest、性能或设备测试。

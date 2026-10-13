# Cuexis Current Status

状态：current

更新日期：2026-10-13

本文是 Cuexis 当前产品和阶段状态的唯一摘要。ADR 定义决策，Spec 定义字段和语义，阶段计划定义未来
范围，阶段报告保存带日期的实施证据；它们不得绕过本文重新定义当前状态。

## 产品边界

Cuexis 由可嵌入的 Playback SDK、独立参考 Player 和独立 Studio 构成。宿主通过
`PlaybackSession`、`PlaybackSource`、`FrameSnapshot`、ContentProvider 和后续 Judgement/Replay
合同接入；宿主不访问 RuntimeSession、World、EnTT、SDL、OpenGL 或 JSON DOM。

权威决策：[ADR 0027](adr/0027-playback-sdk-product-boundary.md)。公共使用说明见
[API reference](api/README.md)。

## 阶段状态

| 阶段 | 当前状态 | 权威入口 |
| --- | --- | --- |
| Stage 0 | completed | [report](stage_reports/stages/stage-00/completion.md) |
| Stage 1A-1E | completed | [reports](stage_reports/stages/stage-01/README.md) |
| Stage 2 | completed | [plan](stage_plans/completed/stage-02/plan.md) |
| Stage 3 | completed | [plan](stage_plans/completed/stage-03/plan.md) |
| Stage Chart Format Update | completed | [plan](stage_plans/completed/chart-format-update/plan.md) |
| Stage 4 | completed | [plan](stage_plans/completed/stage-04/plan.md) |
| Stage 5 | completed | [plan](stage_plans/completed/stage-05/plan.md)、[completion](stage_reports/stages/stage-05/completion.md) |
| 260830-followup（维护计划） | completed | [plan](stage_plans/completed/260830-followup/plan.md)、[关闭报告](stage_reports/reviews/260830-followup/2026-09-01-final.md) |
| Chart Format Foundation | completed | [plan](stage_plans/completed/chart-format-foundation/plan.md)、[关闭记录](stage_reports/stages/chart-format-foundation/2026-09-16-closure-and-handoff.md) |
| Chart Format Foundation Hardening（Foundation 交接加固） | completed | [plan](stage_plans/completed/chart-format-foundation-hardening/plan.md)、[R5 报告](stage_reports/stages/chart-format-foundation/2026-09-17-r5-regression-and-handoff.md) |
| Stage 6 | completed | [plan](stage_plans/completed/stage-06/plan.md)、[关闭报告](stage_reports/stages/stage-06/completion.md) |
| Stage 7A | active；S7A-7/8受限批次完成，8.4于2026-10-10退出；原S7A-9剩余收尾已转交RPA，未完成阶段关闭；诊断正式化归独立前置，架构及实体设备/实时同步实施与验收移交RPA | [总计划](stage_plans/active/stage-07/plan.md)、[Stage 7A分册](stage_plans/active/stage-07/plan-a.md)、[7/8收口证据](stage_reports/stages/stage-07/verification/2026-10-10-s7a-8-4-closure.md)、[7/8实现](stage_reports/stages/stage-07/implementation/2026-10-08-s7a-7-8-entry-assembler-progress.md)、[5/6证据](stage_reports/stages/stage-07/verification/2026-10-06-s7a-5-6-functional-acceptance.md)、[3/4证据](stage_reports/stages/stage-07/verification/2026-10-05-s7a-3-4-functional-acceptance.md) |
| Stage 7-RPA | active；B0设计闭合，B1.1–B1.4观测身份、证据 manifest、可控交付/fake clock、独立 oracle、MinGW Release 原路径基线、owner 核对和初始分位数/峰值已实施并记录；B2 分块追加、不可变成功前缀、自足 ReplayEvaluation/fault prefix、checkpoint/archive/cut/H 身份、失败注入、旧 handle 与最后释放已在隔离 test-only 设施完成并验证；B3.1–B3.5 真实内核/公共首用/Recovery rev3/clock 时间桥已完成本地实施与适用验证（testOnly）；B4.1–B4.5 同线程宿主/owning资源/控制fence/设备回执/呈现与有向Close已完成本地实施与适用验证（testOnly）；B5 非实体设备部分本地实施与自动验证完成（testOnly）；实体设备和生产部署仍待验收；原S7A-9矩阵/预算/交接统一纳入F1–F4 | [RPA分册](stage_plans/active/stage-07/plan-rpa.md)、[B1.1/B1.2记录](stage_reports/stages/stage-07/rpa/2026-10-11-b1-observation-and-oracle.md)、[B1.3/B1.4记录](stage_reports/stages/stage-07/rpa/2026-10-11-b1-baseline-and-measurements.md)、[B2记录](stage_reports/stages/stage-07/rpa/2026-10-11-b2-storage-and-lifetime.md)、[B3记录](stage_reports/stages/stage-07/rpa/2026-10-12-b3-execution-and-clock.md)、[B4记录](stage_reports/stages/stage-07/rpa/2026-10-12-b4-host-and-resources.md)、[诊断独立前置](stage_plans/active/stage-07/plan-diagnostic-formalization.md) |
| Stage 7B+ | future；RPA完整退出后实施S7B-0–4、S7C-1/2，验收并接受后进入Stage 8 | [plan-b](stage_plans/active/stage-07/plan-b.md) |
| Stage 8 | future；Stage 7B+完成并接受后，进行Chart v5 / CXT v2 / Packed Chart正式发行与语义收敛 | [plan](stage_plans/future/stage-08/plan.md) |
| Stage 9 | future；Presentation Foundation 与 Chart v6 / Model v1 | [plan](stage_plans/future/stage-09/plan.md) |
| Stage 10 | future；Chart v5 Studio 和发行工作流 | [plan](stage_plans/future/stage-10/plan.md) |
| Stage 11 | future；Chart v7 单轴几何表现与 shader.json 声明接口 | [plan](stage_plans/future/stage-11/plan.md) |
| Stage 12 | future；规模化、桌面性能、Android 与 Vulkan | [plan](stage_plans/future/stage-12/plan.md) |
| Stage 13 | future；Chart v8、高级表现与确定性粒子 | [plan](stage_plans/future/stage-13/plan.md) |
| Stage 14 | future；稳定 ABI 与 Playback SDK v1 | [plan](stage_plans/future/stage-14/plan.md) |

2026-10-13核对：当前工作树 SDK API 为 `0.8.0`，日期构建为 `26.10.12-2`；
源码起点为 stage-7 / `8f18394`，B3/B4/B5 实施尚未提交。不能将本地 HEAD 单独当作工作树的
验证身份；本轮未查询远端或发行状态，不据本地变更宣称发行。
RPA的B0合同设计已闭合；B1.1–B1.4已完成测试/证据基础设施与原路径基线，**B3 内核与时间桥、B4 同线程宿主与资源协议本地实施完成；B5 非实体设备实施与本地自动验证完成（testOnly），实体设备及生产部署接受尚未完成**。
owner直接选择capture-time、T2/O1/I1/C1/L1、G1/Q1/K1、TB-05与R2，随后授权代理收敛其余普通决策。
本轮按该授权采用B职责边界+P-B部署（主线程捕获/GL/协调，Playback worker共置执行/表现，独立audio owner）、
P3-a内部owning候选、连贯时钟观察、F1+F3呈现回执观测、分块追加存储及显式尾段时钟。
[RPA补充Spec](formats/rpa-execution-contract.md)与[宿主边界](api/realtime-host-boundary.md)已规定
完整前缀、控制cut/末批、R2的Fact/Replay/Seek、错误、资源fence和关闭顺序；
[ADR0046](adr/0046-realtime-playback-coordination.md)记录选择，[RPA分册](stage_plans/active/stage-07/plan-rpa.md)
给出B1–B5小批次、检查和M-TIME/TAIL/SPLIT/CAP测量门禁。
显式新 profile、Recovery rev3/schema2 和公共 SDK minor `0.8.0` 已实施；旧 profile 保持 revision2。
B4 子代理复核及本地 Debug/Release、MSVC、shared、candidate OFF 验证范围见
[B4记录](stage_reports/stages/stage-07/rpa/2026-10-12-b4-host-and-resources.md)；真实线程、设备和生产容量未据此验收。
B3 子代理复核修正后33项定向、Debug/Release 完整受影响 executable 各409项、static/shared
安装消费者与 candidate OFF 通过；未过滤963项套件、复跑、Skip和最终972项未重跑的边界见 B3 记录。
TailProfile具体参数、观察误差/有效期、生产容量及D3拆分阈值仍需B1/B3/B5数据与F3接受，
并非未选择方向；缺参数时生产路径拒绝启用，testOnly值不转正。
[本轮就绪记录](stage_reports/stages/stage-07/rpa/2026-10-11-implementation-readiness.md)只证明文档与纸面合同，
不增加C++/设备/性能/hosted证据；当前用户另已明确授权完成 B3、B4 及 B5 非实体设备部分实施。
B1/B2 隔离设施已经验证；B3/B4 本地 testOnly 实施和适用检查完成；B5 非实体设备线程部署与自动验证已完成（testOnly），实体设备验收待执行，见 [B5 记录](stage_reports/stages/stage-07/rpa/2026-10-13-worker-deployment.md)；F1-F4 尚未退出，Stage 7 未关闭。

2026-10-13追加：owner 授权的 A 批真实设备基础已完成（testOnly）。实际 SDL 音频
callback、独立服务、隔离/恢复/EOF/关闭，以及 RTX 4060 OpenGL Advance、Pause/Resume、
绘制提交/像素读取和有向 Close，修复前缀 Tick 缓存导致的 Close 拒绝后连续三次通过；
完整 MSVC B5 24 项回归通过。源码及失败/修复/复验身份见 B5 记录新增 A 批章节。
实体键盘、Player 人工操作、听音、设备失效/压力和生产参数接受仍待，B5 未退出。

2026-10-13 B 批谱面尝试已按 owner 指令撤回：轨道/文字使用 Player 临时引导而未定义为
谱面实体，不符合此次谱面要求。新增 Player 引导接入、私有呈现桥参数、工具显式选项和
生成脚本已回退；95 个源码文件逐项 SHA256 与 A 批完成快照一致，B3/B4/B5 和 A 批
关闭修复保留。新桌面包启动入口禁用，保留产物/日志作历史证据；旧 2026-10-09 目录未修改。
实体谱面重做和 B 批交互仍未完成；此前新包播放时钟/控制失败证据保留，不能宣称已解决。

2026-10-13 实体呈现修复（历史快照，当前谱面见 10-14 追加）：此前 v5 static candidate 把所有音符静态排布，不能验收运动；
旧包及脚本当时归档。当时桌面包明确选择 compiled/chart.v5.json，注册 source-json profile
candidate.behavior-v2-v1；chart/CXC/Playback 连接 Object 与 Behavior event v2 的有限子集，
40实体、21 Behavior、14程序生成资源。新路径校验整数 alpha、独立语义身份、资源闭包及能力，
factory 编译一次并持有 typed runtime，prepare/update 不解析 JSON。默认关闭调试坐标轴，
Player 不持有谱面布局或动画代码。实际 SDL/OpenGL 窗口已确认短音符下降、Hold伸缩与离场隐藏。
旧静态 Packed profile 的 Behavior 拒绝门禁保留；Template、参数、Clip/Animator、CXT import
仍不在新 profile 范围。现行 [v5 指南](guides/CHART_V5_AUTHORING.md)已同步。
此轮源码已超出 A 批快照，旧95文件一致声明仅归属于修复前历史证据。
当时仍为零 Gameplay Requirement；B5判定与实体交互、设备听音/同步和EOF tail待完成。
35秒PCM最后3秒为静音，不是EOF tail；本次不关闭V5-F、Stage 8或B5。
证据见 [B5记录](stage_reports/stages/stage-07/rpa/2026-10-13-worker-deployment.md)。

2026-10-13 Player R重播修正：输入入口此前误用KeepChartTime，现改为RestartAtZero；
暂停时回零并保留暂停，Space继续。34项Player控制测试与实际窗口复验通过，owner确认
“新版 R 已正常”。重启后首帧阶段日志保留用于诊断；未捕获旧版无响应栈，不声称已定位死锁。
此为局部Player操作修复，不关闭B5判定或实体设备的剩余验收。

2026-10-14 按 owner 要求清理 Player 及旧谱面，并接入 B5 判定：移除私有四轨 guide 绘制、
解析器、旧参数和专用启动器；旧包/旧预览按清单清理，保留 settings/runs、构建及验证证据，
2026-10-09 桌面目录不变。当前 v5 包为 104 Object、22 Behavior、37 资源；独立 Gameplay V2
载荷为 19 Requirement / 21 阶段 / 42 FactBinding，显式使用 rpa.v1 profile，按通用参数附加到
动态呈现源。Player 不含课程布局、UUID、DFJK 键位或音符时刻，Hit/Miss 徽章由谱面实体表达。
全命中、无输入、Hold 提前松开的离线结果和完整 Replay 一致；实际 SDL/OpenGL 的窗口输入注入
获得 46 分 / 21 Hit / 0 Miss，EOF tail 完成。暂停、暂停后 R、恢复、播放中 R 和关闭复验通过。
修复了输入泵更新协调器观察游标、缓存音频帧轮询重标时间、EOF scope 切换及新载入暂停无 Runtime
帧的问题；时钟映射由 SDL 回调锚定，明确报告估计误差，并用真实 clip 末帧限制 EOF 前外推。
六个相关测试目标共 493 项、架构与文档检查通过。现行作者指南与桌面启动说明已同步。
这些是 testOnly 接入和自动验证，不能替代实体键盘手感、听音/同步、设备失效/压力和生产参数接受；
B5、V5-F、Stage 8 与整体 Stage 7 仍未退出。详细证据见上述 B5 工作记录新增 10-14 章节。

2026-10-10诊断工作：owner接受定向原因整改、显式投影、新Recovery版本拒绝旧格式等方向，
并要求诊断正式化从RPA剥离为[独立前置工作包](stage_plans/active/stage-07/plan-diagnostic-formalization.md)。
N0差异刷新无新增/删除；[N1合同与准入](stage_reports/stages/stage-07/diagnostic-formalization/2026-10-10-n1-contract-and-entry.md)
已退出，N2正式生产者、有限投影、Recovery revision2及SDK 0.7.5已落地。N3定向诊断和安装消费者通过，
Debug/Release、ON/OFF、static/shared及MSVC/MinGW/Linux ASan/UBSan本地硬矩阵均通过；
[N3独立退出与冻结证据](stage_reports/stages/stage-07/diagnostic-formalization/2026-10-10-n3-verification-and-exit.md)成立；报告中的未提交工作树为当时证据身份，后续本地提交见上文。
诊断前置不新增阶段编号；N3退出先于RPA新架构原型和A4–A6，RPA纸面设计/既有行为只读盘点可并行。
owner指定诊断正式化目标SDK为`0.7.5`并授权的N1–N3已完成；
公开差异和拒绝合同已落定，本地硬矩阵按N3逐项证据通过（M1为初跑123项加修复后单项复跑，非修复后全量124项重跑），退出证据按当时工作树快照冻结，
RPA选型、生产预算和实体设备验收不构成诊断N1–N3的退出条件。

2026-10-10较早规划修订（历史状态，已被上方2026-10-11设计决定取代）：当时RPA按维护性、性能、扩展性的优先级开展多方案比较、有限验证和分批实施，
当时尚未选定单一架构。owner要求活动诊断码采用正式名称、不保留阶段名；N1–N3须在新架构原型及A4–A6实施前退出，
该规划窗口当时尚未执行迁移与新验证。已有[逐码审计及证据入口](stage_reports/stages/stage-07/rpa/2026-10-09-diagnostic-phase-name-audit.md)
继续作为先行工作输入；根目录初稿已[归档](archive/stage-07-planning/2026-10-10-rpa-target-architecture-draft.md)，
诊断码审计及证据ZIP留在原rpa报告路径作为历史证据，任务归属按上述独立前置更新；
原S7A-9.1–9.4全部移交RPA-F1–F4，不再独立排期。
后续主线为诊断独立前置N3退出 → RPA完整退出 → Stage 7B+实施与验收 → Stage 8。
实施依赖由[Stage 7总计划](stage_plans/active/stage-07/plan.md)及对应分册维护；本次只改文档，不产生产品/API/版本变化。

2026-10-09范围调整（2026-10-10补充收尾归属及顺序）：按owner要求，实时架构重构和正式命名迁移从S7A-7/8拆出，
较早由Stage 7内的[Stage 7-RPA子阶段](stage_plans/active/stage-07/plan-rpa.md)承接；随后诊断正式化独立，架构仍由RPA承接。当时指令已将
其从future独立阶段归入active/stage-07分册，原plan-a §3.4.18/19
保留兼容入口。两项只有审查/规划，未改产品实现；真实Player失速、音频与判定同步仍未修复。
S7A-7/8原有实现/受限验收保留，S7A-8.4已按下述最终证据退出。owner进一步明确实体设备和实时同步
的实施及最终验收直接归RPA，旧失败随交接保留。原S7A-9剩余收尾也全部转入RPA-F1–F4。
RPA完成并接受后实施Stage 7B+，7B+当前计划范围完成并接受后再进入Stage 8。
诊断独立前置已退出；该轮接手任务为RPA设计讨论与文档修正；其后规划就绪进展以上方2026-10-11记录为准。
较早的诊断实施授权不自动授权RPA原型或产品；本轮只核对诊断退出基线及后续差异。
本轮不实施RPA或7B+，不接受生产预算或关闭整个Stage7A。

2026-10-10收口核对：验收基线为PR32与本地HEAD
`1ebcbdc92d92fa9d45e4e1f835e5a1f338e7fc2d`，tree为
`90f930a97ee2e2980fec12de3334b687ac529064`，base为
`3076948ce403ea0da6fbe389835b6a4c9defbcd3`。
该SHA的PR及push三平台、PR Version Gate共7个run全部success；39个job成功，
另2个Version Gate job因不适用当前事件跳过。真实pre-merge日志确认可信base checker、
`context=live`、UTC2026-10-09与精确owner审批评论6084640280。
owner随后授权配置保护，master已启用active ruleset24799746：必须PR、strict App15368版本检查、
禁止强推/删除，无bypass；既有classic protection保留。个人仓库的workflows来源规则请求被
GitHub以422拒绝。2026-10-10 owner改为接受VERSIONING规定的单人维护来源例外，
不再强制指定workflow来源；App/context保护、真实base checker和最终审批/日志要求保留，queue禁用。
owner本轮明确要求关闭8.4；最终基线的审批、真实日志、hosted和保护证据现已收齐，
S7A-8.4退出，S7A-7/8受限批次完成。详见[关闭记录](stage_reports/stages/stage-07/verification/2026-10-10-s7a-8-4-closure.md)。
本次关闭文档是上述已验证基线的证据回填；后续新提交审批/CI仍独立绑定，不把本记录冒作新SHA成功。
九项目标、保护现状及原始输出见[本次余项核对](stage_reports/stages/stage-07/verification/2026-10-09-s7a-7-8-current-gate-audit.md)。
本轮配置及平台限制见[保护与交付记录](stage_reports/stages/stage-07/verification/2026-10-09-s7a-7-8-protection-and-delivery.md)。
来源要求后续裁定见[单人接受记录](stage_reports/stages/stage-07/decisions/2026-10-10-single-maintainer-workflow-source-exception.md)。
旧审批失败复现继续保留在[历史路线订正](stage_reports/stages/stage-07/verification/2026-10-09-version-gate-and-rpa-routing.md)。
以下历史轨迹只证明各自SHA/fixture，不把审批材料或本地测试当作线上门禁通过。

**Stage 6 未完成项的归属变更（2026-09-29）**：关闭后对 [ADR 0042](adr/0042-stage-6-productization-boundaries.md)
逐条核查发现四项只部分实现或未实现，且**此前不被任何阶段计划列入范围**（`completion.md` 的 Stage 7A
与 Stage 8 交接清单均未登记，Stage 8 计划正文对四项零命中）。它们是：显式 candidate 与实验隔离
（`Cuexis_ALLOW_EXPERIMENTAL`、candidate flavor、Player `--candidate-entry`、没有任何 preset 或 CI
开启 candidate）、离线 typed assembler 与 feature 派生、具名宿主六动词命令循环、SDK API `0.7.1`。
经项目所有者指定，四项自 2026-09-29 起由 **Stage 7A 承接，并作为其关闭前置条件**，登记在
[Stage 7 计划](stage_plans/active/stage-07/plan.md) 的 §1.2（并在 §3 的 S7A-8 工作内容与 §3.1 台账的
S7A-8.1–S7A-8.4 展开；2026-10-02 订正，原文误写为 §2 与 §6）。**这四项已于2026-10-10完成本批受限收口**；实施进度与最终证据见上述关闭记录。本段只记录
归属与前置条件，不构成任何实现或发布声明。逐条证据见
[复核交付报告 §10](stage_reports/reviews/stage-06-review-2026-09/2026-09-28-delivery-report.md)；
2026-10-02 的当前状态逐项复核（含"具名宿主命令循环已由 PR #31 合入 `master`、动词集为
`open/play/pause/tick/seek/reload/quit`"这一取代性事实）见
[S7A-0 四项交接收口台账](proposals/gameplay-v2-acceptance/STAGE6_HANDOVER_LEDGER.md)。

2026-10-07 四项实施已授权提交现有 PR #32，由 owner 本人审批。candidate flavor/许可/安装隔离、
Foundation typed assembler CLI 原子包与真实 prepare、安装后宿主 candidate entry 和 SDK `0.7.1`
候选及精确审批门禁均已实施；本地 ON/OFF static/shared、Linux GCC、MinGW 矩阵已回填，
MSVC OFF Release 全量 1006 项无失败（1 项 Windows 符号链接测试跳过）。
当前代码交付不等于门禁整体退出：可信 master bootstrap已于2026-10-09安装，平台保护与 owner 精确审批以及同 SHA
hosted 证据仍待回填。尤其 merge_group 的候选 workflow 不能仅靠复制 base checker 证明可信；
启用 merge queue 前必须采用受保护的 required workflow/外部可信检查源，否则保持禁用。
历史复核见[四项交接复核](stage_reports/stages/stage-07/verification/2026-10-06-stage6-handover-audit.md)，
本轮实施与证据见[四项实施报告](stage_reports/stages/stage-07/verification/2026-10-07-stage6-handover-implementation.md)。

本轮 S7A-7.1–7.5 / S7A-8.1–8.4 联合实施及受限本地功能验收完成，实现提交 `83935f1375be61ba0f3ea16205fccc6a70094c8e`，已推送现有 PR #32。
typed Gameplay source→Graph/Packed→完整 Entry/CXC/捕获 filesystem generation→实际 prepare、
生产 assembler 完整闭包与确定性原子发布、显式 Gameplay 生命周期/组合事务、typed H/T bridge、
同实际 kernel/Fold 的完整 Replay/Snapshot/exact Seek/control、optional FactBinding 的 resolver/group/
typed lifetime/owning sourceMap 已实现。Headless、Player 实际 SDL scancode 接线、Reference Host
六动词+tick、candidate ON/OFF 与 installed static/shared consumer 已执行本地验证。
人工 golden、独立 Tap/Hold oracle、故障注入及完整结果比较通过；Debug/Release、headless、
MinGW、Linux GCC/Clang sanitizer、架构/package/ASCII/诊断码表/docs/version 的本轮输出和
首次失败修复边界见[10-08受限验收记录](stage_reports/stages/stage-07/implementation/2026-10-08-s7a-7-8-entry-assembler-progress.md)。
SDK 实际 production API 差异证明支持 preview patch 0.7.1，当前日期构建为 26.10.09-2，完成日期更新后
fresh/clean-first build 与消费者验证，不构成发行许可。
实际 GPU/OpenGL 纯播放 smoke 完成6帧；显式 Gameplay 零输入到期产生 Miss 并完整 Replay 一致。
真实键盘/音频设备、实时校准和 Gameplay 正反馈 GPU 像素专项未执行。新 SHA hosted 待回填。
S7A-8.4 **未退出**：owner approval、保护/required workflow 与同 SHA
hosted 门禁仍未满足；trusted master bootstrap已于2026-10-09完成；未代审批、执行 bootstrap、改保护、合并或发行。S7A-9 仅累计计数/测量输入/
handoff 草稿，不接受生产阈值，不进行最终关闭；Stage7A保持active，Stage8正式发行未开始。
Version advancement pre-merge 触发断层已修复：保留 target、恢复 pull_request，两事件只执行
trusted base 工具，本地回归通过。首次推送 aaa9ca3 的 pre-merge job 已实际启动，因可信 base 缺 .github/sdk-api-owners.json 报
version.bootstrap.required。b6081c3 的 MSVC hosted 成功，Linux/MinGW 严格编译失败已定位并修复（代码提交 `6d34e7adc482a833eaea0c01e5be106539fe7940`）；
增量 fresh/clean-first 本地验证及新修复 SHA hosted 状态见
[10-09 CI 修复记录](stage_reports/stages/stage-07/verification/2026-10-09-s7a-7-8-ci-repair.md)。新 SHA hosted 继续待回填。可信 base 缺前置条件时
历史缺基线情形的 bootstrap_required 保留阻断，不执行 PR head 脚本、不绕过 owner 门禁。
2026-10-09 owner 授权验证后推送 bootstrap，旧 SHA 六个 CI 已确认取消；直接 master 推送被 GH006 required pre-merge check 拒绝，未修改保护。
兼容候选 `3076948ce403ea0da6fbe389835b6a4c9defbcd3` 已推送 `codex/version-gate-bootstrap`，保留旧 workflow/tests、SDK0.7.0；旧可信 checker 与 Linux19项兼容测试通过。
代理误建的bootstrap PR #33已关闭，未合并；该PR及bootstrap分支已无运行中/排队CI。核对已有真实GitHub Actions pre-merge success绑定3076948和要求的App15368后，已在不改保护、不伪造状态、不新建PR的条件下，将bootstrap提交3076948ce403ea0da6fbe389835b6a4c9defbcd3直接推入master。stage-7已同步新base，日期经updater升至26.10.09-2；SDK仍为0.7.1。bootstrap缺文件问题已解除；精确owner审批、保护来源与新SHA hosted门禁仍未退出。
证据见[bootstrap报告](stage_reports/stages/stage-07/verification/2026-10-09-version-gate-bootstrap.md)；S7A-8.4保持未退出。

2026-10-09 后续核对：`c110c500e1e7987fe19a47ba7141d0787e5ce4b8` 的 hosted 三平台及
Version Gate 已通过，实际 owner LF 评论 `6075108949` 绑定该 HEAD；此前审批/hosted 待回填
叙述不再适用于此 SHA。CRLF 筛选/解析代码修复与 D/F/J/K 自生成设备谱面已提交实现 `33df051dc0ac3c0252f731daa32e30bca15e9537`，
该提交 SDK 为 0.7.1，日期构建曾误更新为 26.10.09-3（下文已订正）；新 HEAD 必须另绑定 approval/hosted。
具体证据见[修复与谱面报告](stage_reports/stages/stage-07/verification/2026-10-09-crlf-and-device-fixture.md)，
操作见[设备验收示例](examples/s7a78-device-acceptance.md)。master 旧 checker 不因 PR 修复自动更新；
保护来源、真实设备/校准门禁仍独立保留，S7A-8.4 与 Stage7A 均未退出。

随后真实 DFJK keyboard/audio 运行均在首个按键触发 `input.continuous_unsupported`；用户确认
音乐与六方块可见，但按键判定未通过。首次 Controller Load 后 adapter 错把新离散事件标作
跨采样 gap，现已在 `afdcfcd8890cb8c5a3ae6aa8c8273d4fdc15b5ff` 按所属 Playback 合同修复并通过
Debug/Release 控制回归；修复后两次真实设备复测均有一次 Hit，但随后同 Tick 碰撞退出1，整体未通过。
SDK 显式 discontinuity 拒绝保持。1afe382 的审批
6078336026 已核对有效，Version Gate 实际因候选 build3 而拒绝；日期候选通过 updater 订正为
26.10.09-2（相对 trusted master build1），不会改 gate 规则。证据归
[设备失败与修复报告](stage_reports/stages/stage-07/verification/2026-10-09-player-discrete-device-repair.md)。
修复后真实按键/控制、GPU正反馈、设备校准以及新HEAD approval/hosted仍待回填。
18:10/18:11 的 keyboard/audio 运行绑定 `1458ec5`，最后成功 H185/H277、score2/hits1，
完整 Replay 比较均 same；`same_tick_collision` 的物理转换序列未在日志中逐条保留，根因仍待最小反例。
原始日志与复核见[真实设备复测](stage_reports/stages/stage-07/verification/2026-10-09-device-retest-same-tick-collision.md)。
随后最小 poll press/release、多键与连续点按复现旧桥失败；Player testOnly 采样合同与适配器
已在 `90b8115b547b93fc80d66afb688145859ab54e40` 改为每个映射转换一个工作 Tick、无输入帧一个
Tick，T仍每帧一次；kernel 一个 Tick 至多一个
input 与稳定诊断码保持。Debug/Release PlayerControl各32/713、Gameplay各25/1467及安装门禁各3/3通过。
证据归[多转换桥修复](stage_reports/stages/stage-07/verification/2026-10-09-player-multi-transition-tick-repair.md)，
修复后真实设备及新SHA approval/hosted仍待回填，S7A-8.4不退出。

2026-10-09 设备练习追加可选四轨guide：Ready等Space、DFJK下落短音符、K长条与判定线，
反馈取实际FactBinding snapshot，分数取query，不改H/T采样或kernel/Fold。新五requirement/
七phase人工golden18/7/7/0；独立source/binding核对、四Host golden与定向SDL/GPU窗口通过。
Debug/Release ON控制各34/739、OFF各28/483，Gameplay25/1467与安装消费者保持通过。
说明与原始证据见[可读练习报告](stage_reports/stages/stage-07/verification/2026-10-09-player-readable-practice.md)。
定向窗口输入不替代实体键盘/听音验收，音频节拍仍未校准；新SHA hosted/owner与S7A-8.4独立保留。

## 当前格式与 SDK 合同

### Gameplay v2 收敛状态（2026-10-04）

Gameplay V2 已成为 Stage 7A 的实施基线：Gameplay I 的 ADR/Spec/ABI 已标注 `superseded`，只保留为历史
候选基线与差异对照（取舍见 [ADR 0043](adr/0043-gameplay-judgement-ruleset-convergence.md)）。现行权威为
[ADR 0044](adr/0044-gameplay-v2-semantic-kernel.md)、[Gameplay V2 Spec](formats/GAMEPLAY_V2_SPEC.md) 与
[Gameplay V2 ABI](api/GAMEPLAY_V2_ABI.md)（三份顶层均为 `candidate`；部分实现不等于整批验收）。按 2026-10-02 的第 1 轮
补充（S1-01…S1-05，见 [裁决清单](proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) §7A），冻结采用
**分批冻结**口径：只授权**受限** S7A-1 骨架（类型角色与边界、模块与安装边界、所有权与异常承诺），
该受限骨架授权本身**不授权**字段表示、整数宽度、序列化与线格式、枚举集或其余批次语义；
后续各批次已经闭合的时间、输入与 Capsule candidate 表示仅按其明确合同消费
（第 1–7 轮**已全部裁定、无待裁定轮次**，语义由各自小节授权；第 7 轮"收尾澄清与缺陷"见
[第 7 轮收尾裁定与缺陷处置记录](stage_reports/stages/stage-07/decisions/2026-10-03-s7a-7-wrapup-rulings.md)）。
**裁定完成不等于实现完成**：受限 S7A-1 骨架、S7A-2 两个半批与 S7A-3 第二半 part 1 已落地并有带日期记录；
**S7A-3/4 受限功能验收完成，容量整体证明未完成**；包含性 gateIncomplete 与真实容量装配消费点已由当前 Spec §3.8.8
记录落地，不再作为“尚未接线”缺口。2026-10-05 的只读核对补齐既有实现与当前 SHA hosted 证据，
见 [剩余证据核对](stage_reports/stages/stage-07/verification/2026-10-05-s7a-3-remaining-evidence.md)；该报告属于此前只读核对。随后本工作区实施了 runtime T4/K4 与真实 author adapter，聚焦 C++ 验证见
[受限功能验收证据](stage_reports/stages/stage-07/verification/2026-10-05-s7a-3-4-functional-acceptance.md)；E1 功能行与四工具链完整矩阵已补齐；state-budget 保持 INCOMPLETE GATE，Stage7A 不关闭。
提交a0c8b7e的hosted与临时证据归档见 [新SHA交接报告](stage_reports/stages/stage-07/handoffs/2026-10-05-s7a-3-4-hosted-and-handoff.md)；
本轮由owner授权实施S7A-5.1–5.5 + S7A-6.1–6.5；目标、九项已选方向、十三项首用合同和
J0–J7执行卡归 [Stage 7A分册§3.2](stage_plans/active/stage-07/plan-a.md#32-s7a-5--s7a-6-联合实施目标决策和准入)。
[方案比较与选优](proposals/implementation-input/stage-07/s7a-5-6-design-selection.md) 保存每项五套备选及反例；
[U01–U13重审](stage_reports/stages/stage-07/verification/2026-10-06-s7a-5-6-u01-u13-rereview.md) 修正六项边界，其他七项保留方向并补条件，消费合同已落入Spec/ABI/profile，实施与验证见本轮报告。
active/stage-07只保留主计划与 [旧路径映射](stage_plans/active/stage-07/legacy-paths.md)，
历史接手和交付评估已归档；当前接手导出至owner桌面，决策仍写回plan/对应合同。
5/6受限功能验收完成，Debug/Release/headless/shared/MinGW/GCC/Clang本地证据已回填，Replay/Snapshot六套字节一致；新提交hosted待回填。尚未实施7–9、接受生产预算或关闭阶段；既有
[排期修订记录](stage_reports/stages/stage-07/handoffs/2026-10-06-s7a-5-6-planning-selection.md) 保留当时证据，
本次目录/桌面交接见 [整理记录](stage_reports/stages/stage-07/handoffs/2026-10-06-s7a-5-6-plan-and-desktop-handoff.md)。设计来源见
[Gameplay V2 redesign research](proposals/research/gameplay-v2/README.md)。Hold 的 prepared grace、作者
默认值与单条覆盖、严格边界、identity 分区和预算/快照原则已统一；Slider continuity/grace
仍属于 Stage 7B+ capability，不是 Stage 7A 默认语义。
**稳定 C ABI 的唯一归属阶段是 Stage 14**（不是 Stage 12）：Stage 7A/7B/8 只提供 C++ typed preview，
稳定 C ABI 由 [Stage 14 计划](stage_plans/future/stage-14/plan.md) 承接；`AGENTS.md` 中把稳定 C ABI 写作
Stage 12 的表述是**孤例**（第 21、260 行），由 owner 择时订正，本轮不改 `AGENTS.md`（缺陷 `D-11`）。
在当前路线修订中，Stage 7A/Stage 8 的 Gameplay semantic 基线固定为 Chart v5 下的
`gameplay.version = 2`。Canonical Gameplay Graph 可作为 CXC v1 独立 `gameplay-graph`
Playback entry，与 `packed-chart` entry 共用 typed prepare、Judgement、Ruleset、Snapshot、
Replay 和 Presentation bridge。Stage 7A 的目标合同包括 Tap/Hold/Release-tail、单一
`capacity=1` exclusive resource、TimebaseProfile、Ruleset fault transaction，以及
early/exact/late/Miss 的 early/late Presentation bridge；TimebaseProfile 已随 S7A-2 落地，T4/K4 与真实 author adapter
已有受限功能验收。Ruleset Fold、Score/Combo/Statistics、Snapshot/Replay、exact Seek 和惰性分支已随
S7A-5/6 落地；Playback/Presentation bridge 产品集成仍归 S7A-7，四项 Stage 6 交接收口归 S7A-8，
生产预算、最终矩阵与原7A阶段关闭现归RPA-F1–F4（原S7A-9）。局部功能验收不替代这些后续门禁。

V2 三份顶层文档仍是工作稿、待 owner acceptance；C10–C14 与代表性切片仍是研究性证据，不能直接作为生产 ABI 限额。

- Chart v1/v2/v3 Reader、迁移和 Playback 路径继续保留；Chart v4 的静态、参数化和合法非空动画已由
  默认 Playback Session 求值。
- FrameDigest v1-v3、canonical bytes/order、合法输入 identity 与默认 capability 维持兼容。
- CXC v1、CXT v1、Chart v4 的格式语义以 [formats index](formats/README.md) 为准；内部 CXC 不是独立
  公共 package SDK。
- Chart v5 的发行路径仍是**显式 candidate**：正式默认 Writer、40,000 语义实体和 16 MiB Packed entry
  的发行门禁关闭前，Chart v5 不作为默认发行格式。Stage 7A 冻结最小 Judgement / Input / Replay 合同；
  [Stage 8](stage_plans/future/stage-08/plan.md) 完成 Chart v5 / Gameplay v2 / Canonical Gameplay
  Graph / CXT v2 / Packed Chart 与两种 Playback entry 的正式发行收敛；Stage 7B+在RPA后完成当前计划范围，
  验收并接受后进入Stage 8；能力以版本化capability明确选入发行。详细格式工作见
  [Chart v5 format plan](stage_plans/active/chart-format-update-for-v5/plan.md)（其 Foundation 交付已归档）。
- CXT v2 候选合同见 [CXT_V2_FORMAT.md](formats/CXT_V2_FORMAT.md)：模板、Prototype/Instance、
  Slot/Binding/ValueSource、Pattern、有限确定性展开和动画扩展；不包含任意脚本。
- Packed 候选物理合同见 [PACKED_CHART_FORMAT.md](formats/PACKED_CHART_FORMAT.md)：
  身份/字典、Archetype/实体差异流、无损 Beat 和容量 profile。它不是已实现的生产
  Reader；40k/16 MiB 仍需实测验收，更改被冻结的 CXT 参数需要显式重新编译。
- Gameplay revision 2 的完整 Packed 字段由 [Capsule format](formats/GAMEPLAY_CAPSULE_V2_FORMAT.md)
  唯一拥有；来源 / 有效 policy、T4 时间端点、K4 实例顺序与局部 proof 已设计闭合。
  来源身份投影修正、Reader/Writer、file/memory 与三工具链 focused 已有实施证据，
  当前 SHA hosted 包含既有 Capsule golden；完整 E1 与 authoring/affine 余项见上述核对报告。
  历史 3519/110 与 focused 测试数字不代表本轮复跑或整批新合同验收。
- CXC v1 仍为容器版本，v5 candidate playback path 与 v4 entry 并存。Stage 7A / Stage 8 负责收敛
  `gameplay-graph` 与 `packed-chart` 两种 Playback entry；Stage 8 只有在二者恢复同一 Canonical Gameplay
  Graph、prepared identity 和 Judgement kernel 后，才可正式发行。
- Chart v6 / Model v1、Chart v7 和 Chart v8 仍不可加载，分别作为 Stage 9/11/13 的表现设计输入；
  不再作为 Judgement 的隐性前置。
- 当前工作树 SDK API 为 `0.8.0`（B3/B4 尚未提交）；历史0.7.1审批不覆盖当前变更，本地提交不等于发行。既有发行基线记录为 `0.7.0`，本轮未复核远端发行。安装后的 Playback headers 不泄露 EnTT、SDL、OpenGL/GLAD、JSON DOM、
  RuntimeSession 或 World。
- Playback 的 default `allCapabilities()` 包含 shader asset 和 parameterized material capability；
  显式裁剪 Session 仍可稳定拒绝它们。Playback 热路径不调用 shader compiler。

## 延期和禁止边界

运行时脚本和逐帧 script callback 无限期延后；不为它们预留 Chart/CXT/CXC 字段、extension、
capability、bytecode、ABI 或 Playback hook。离线 authoring generator 只能作为未来独立工具讨论。

RT-29、T1 World/Animation 大规模优化与 T2 大包解析降本仍需另外的触发证据和明确授权。
[Chart v5 format plan](stage_plans/active/chart-format-update-for-v5/plan.md) 现作为 Stage 8 的详细格式
工作包；其前置 [Chart Format Foundation](stage_plans/completed/chart-format-foundation/plan.md) 与
[交接加固 R0–R5](stage_plans/completed/chart-format-foundation-hardening/plan.md) 已于 2026-09-17 完成并归档。
2026-09-01 阶段核验的三个问题（版本门禁、后端中立表现渲染边界、常用媒体支持）已随 Stage 6 关闭处置为
`closed`，历史现象不改写；删除旧 Reader 仍按 ADR 0041，不因 v5 弃用窗口实施。Stage 7B+在RPA后实施并验收，随后进入Stage 8；Stage 8 关闭后交出经批准的 SDK 发行版本 / Chart v5 / CXT v2 接手基线。

## 更新规则

任何产品阶段状态变化必须同时更新本文与相应的计划或新报告。历史报告不得被改写为新的验证结果；
新的关闭、复核或纠正必须形成新的带日期报告。

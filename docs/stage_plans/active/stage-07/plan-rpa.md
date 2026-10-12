# Stage 7-RPA Plan: 实时播放架构与接口规范化

状态：active；B0设计闭合，实施规划已就绪；尚未开始产品实施

更新日期：2026-10-11

归档来源：[旧版 PROJECT_GUIDE](../../../archive/PROJECT_GUIDE_LEGACY_2026-08-10.md)、
[RPA目标架构v0.5历史初稿](../../../archive/stage-07-planning/2026-10-10-rpa-target-architecture-draft.md)、
[原RT78迁移记录](../../../stage_reports/stages/stage-07/handoffs/2026-10-09-realtime-stage-separation.md)。

## 阶段目标

在已实现的Gameplay内核与Playback集成上，建立可靠的输入/音频/判定/表现时间关系、
明确的所有权和资源/快照发布协议，解决实际Player实时成本与调度问题，
并承接原S7A-9的最终矩阵、预算、Stage 8交接及关闭包。

RPA是Stage 7内的子阶段代号，与7A、7B+分册共同受Stage 7总计划约束；
不占用既有S7B/S7C编号，不重排Stage8–14。RPA保留原A0–A7追溯编号（见「原A0–A7兼容映射」）
及F1–F4收尾工作线，原S7A-9.1–9.4映射为RPA-F1–F4。
诊断码正式化N0–N3已剥离为[独立前置工作包](plan-diagnostic-formalization.md)，不计入RPA实施进度。

本分册是RPA**范围、批次、门禁与验收的唯一来源**；决定归
[ADR0046](../../../adr/0046-realtime-playback-coordination.md)，语义归
[RPA补充Spec](../../../formats/rpa-execution-contract.md)，表示/所有权归
[宿主边界](../../../api/realtime-host-boundary.md)。报告只记录选择依据与dated证据。选择顺序为维护性优先，其次性能、扩展性；
低耦合、局部可替换和故障可解释性是比较依据，不以线程数、组件数或消息数单独判断优劣。

## 前置条件与执行位置

Stage7A保留既有内核、集成与受限验收证据；剩余S7A-9工作及计数/测量/handoff输入全部由本分册接管。
诊断独立前置基于7/8已有基线完成N0–N3；RPA消费其退出基线实施实时架构及综合收尾，
交接给7B+并准备Stage8输入，不以7A另行关闭作为开工前置。

**独立诊断前置N3退出是B5及所有新架构原型的硬前置**；B0–B4的纸面设计、既有行为只读盘点、
存储整改与观测建设可并行推进，N3退出不自动授权实验代码或RPA架构实施。

- 输入基线为现有typed source→Graph/Packed→Entry→prepare、实际kernel/Fold、Replay/Seek、
  optional FactBinding和安装消费者；原有证据边界按对应SHA保留。
- 使用现有V2 Spec/ABI、execution/author profile及Playback/Audio/Animation/Presentation合同。
  B0已选语义由RPA补充Spec明确替代；B3按新profile实施首用消费者，保留未替代的判定规则。
- 接收N3记录时核对其源码身份、映射/投影/载体合同与当前基线的差异；后续修改这些边界时，
  在对应增量复验受影响项并保留原记录，不把旧N3证据当作新行为已通过，也不重跑无关整套迁移。
- 实体DFJK/focus/control、真实听音、设备失效/恢复、卡顿/刷新率/输入延迟与实时同步，
  包括实施和最终验收，整体归RPA；与F线的内核/消费者验收在同一子阶段统一调度，证据分别归类。
  旧失败作为待办基线，不改记通过或转回7A。
- 原S7A-8.4已按[10-10记录](../../../stage_reports/stages/stage-07/verification/2026-10-10-s7a-8-4-closure.md)收口；
  RPA后续API变化仍需独立版本差异、审批与门禁证据。
- RPA-F4接受完整交接包（包含7A既有成果及原9收尾）后进入Stage 7B+；
  7B+当前计划范围实施、验收并经owner接受后，再进入Stage 8。

## 范围与责任域

六项责任为输入捕获D1、确定性执行D2、表现采样D3、音频服务D4、设备提交D5和协调D6；
**责任数不等于线程数**。首版D2/D3共置Playback worker，D1/D5/D6在主线程，D4为独立audio owner；
实施须验证Prepared/renderer、输入交付与组合事务的跨域边界。
RPA使用独立诊断前置交付的正式码/公共投影/恢复合同，不另立命名或兼容合同。

责任到可变状态域的划分、部署候选与淘汰条件、时间桥与推进许可的具体设计、存储寿命规则、
失败/恢复/关闭顺序，以及各决策项的推荐与代价，全部归上述Spec与宿主合同；[生产架构方案](../../../stage_reports/stages/stage-07/rpa/2026-10-11-production-architecture-design.md)保留选择依据。
本文只持有批次、门禁与验收。

### 与既有合同和集成面的关联（不改变任务归属）

| 关联原集成面 | RPA承担的工作 | 关联首用行/既有边界 |
| --- | --- | --- |
| S7A-7.1 | 校准会话时钟、输入准入与推进顺序、明确H/T/RuntimeFrame；控制、推进/表现采样和恢复边界 | C78-01–04/08；R78-01–05，保留sealed前缀、faulted只读、generation/scope区分 |
| S7A-7.3 | 可控时钟和延迟交付fixture、独立时间oracle、同轨迹完整Replay/Seek验证 | C78-03/08/09；不能用生产推进函数充当oracle |
| S7A-7.4 | Player实际输入、音频服务/渲染调度解耦、热路径Replay整改、快照与资源代数一致 | C78-03/04/07/11；R78-06/07，resolver层域和typed lifetime保持 |
| S7A-7.5、8.1 | 任何新公共边界的ON/OFF、owner、ASCII及static/shared安装消费 | C78-08/10/12；无内部Kernel/Recovery安装，无默认生产新能力 |
| S7A-8.3 | Reference Host同协议六动词加tick，typed clock供给与拒绝矩阵 | C78-04/11；fixture步进保留显式用途，不伪称实时设备模式 |
| S7A-8.4 | 按RPA最终公共差异重审patch/minor与owner/保护/hosted材料 | C78-12；7/8历史受限基线为0.7.1，诊断退出基线为0.7.5；均不预先约束RPA未来API |
| S7A-7.2、8.2 | 保留已完成source/assembler闭包；新时间profile若影响identity则定向回归 | C78-05/06，R78-08/09沿既有Graph/发布合同；不趁机重写格式 |
| S7A-6.1–6.5关联回归 | 执行状态、journal和owning结果存储整改的完整语义回归 | 完整ReplayEvaluation、archive/cut/H、future pending、lastObservedTick、惰性分支不变 |

RA-02/03/04是真实语义或产品支持范围问题，不作为「普通表示」交给代码自行决定。若选择原始事件
时刻重建、改变碰撞支持或新的推进fence，必须先明确Spec/ABI/profile/identity/稳定诊断修订，列出
被替换文字和影响测试；不因大范围重构建议而默认重开全部S7A-2–6规则。

## 决策台账与规划就绪判定

2026-10-11 owner 已授权：剩余事项除极难决策者，由代理提出合理方案并落约。
下表区分直接选择和委托裁定；测量缺口列为实施 gate，不反复要求重新选择已经确定的方向。
**可实施**指职责、协议、失败、版本、批次、验证及未测量参数的处理方法已确定；
不表示产品已实现、预算已接受、原型已授权或阶段已关闭。

| 项 | 采用方案与决定来源 | 权威落点/实施批次 |
| --- | --- | --- |
| D-TB01 | owner：capture-time；阻塞期间按键以恢复后的实际捕获时刻记 Tick | V2 Spec §3.7.3；B3 |
| D-TB03 | owner：T2/O1/I1/T2-C1/T2-L1；快速点按与和弦，不做多 timer/input 交错 | execution profile §3.4–3.6/4.1.1；B3 |
| D-TB05 | owner：各层 offset 各一次；委托补齐正号/单位/checked 舍入 | RPA Spec §2；B3 |
| D-TB07 | owner：R2；委托补齐 control cut、Fact、future pending 与 Replay | RPA Spec §3–4；B3内核/B4宿主 |
| D-AD06 | owner：G1/Q1/K1；委托补齐连续前缀、有限载体、请求结果、正常/控制覆盖分离 | RPA Spec §1、宿主 §1–2；B3/B4 |
| D-AD13 | 委托：带实际采样时刻、segment/质量/有效期/误差的连贯观察；旧缓存不刷新 | RPA Spec §2、宿主 §1；B3 |
| D-TB06 | 委托采用 A1-b 显式尾段时钟；具体 TailProfile 依 B1/B3 数据冻结 | RPA Spec §7；参数化实现B3，生产profile在B5前 |
| D-AD10 | 委托采用 F1+F3 可观测；内部 present 回执，不保证首见，RemainingFrames不改 | 宿主 §4；B4/B5 |
| D-AD02 | 委托采用 D2+D3 同 worker；B3后测量且确认表现导致超界才重开拆分 | RPA Spec §7；B5准入/F3 |
| D-PREP | 委托采用 P3-a：内部 owning 候选；Playback worker校验/acquire，主线程GPU | 宿主 §3；B4 |
| D-ARCH | 委托采用 B 职责边界 + P-B 初始部署；分块追加/immutable prefix/self-owned result | ADR0046、宿主 §3–5；B2/B4/B5 |
| D-VERSION | 委托选新 execution/late/Fact token、Recovery rev3、state schema2、新SDK minor | RPA Spec §6；B3/B4及F1 |
| D-RESUME-FAIL | owner选审查修补A1：最终切段后、R2 seal前准备失败终止直接续跑资格，保留现场并显式退出 | RPA Spec §3.5；B3.3/B4.4；R35–R37 |
| D-CANCEL | owner选审查修补B1：owner按步骤串行裁决Apply/Cancel，终态拒绝迟到执行后才允许回收 | 宿主 §2.1；B4.1/B4.3；R38–R41 |

采用方案不需再等待一次方向选择。仍需生产测量/接受的项目见下面 M 表；如实现遇到与已定合同
冲突的新反例，先修所属合同，不自行放宽为静默降级。
修补方案A1/B1只指上述两项，不替换实施批次B1或已选尾段A1-b。

## 批次表

本轮只交付设计与文档；以下“进入”是技术依赖，**不自动授予产品代码、原型或设备运行权限**。
后续实施获授权后按下面依赖执行，先核对 branch/HEAD/dirty 状态和 N3 基线。
每批保存变更身份、命令、失败/未执行项和回退边界；语义变更与机械重构分开。

| 批次 | 依赖 | 产物与退出要点 |
| --- | --- | --- |
| B0 合同闭合 | 本轮设计授权 | 已选Spec/ABI/ADR、替换表、最小反例、批次和测量gate；仅文档验证 |
| B1 观测与基线 | 后续实施授权，N3差异核对 | 固定轨迹、fake clock、独立oracle、源码身份manifest、Release基线与测量协议 |
| B2 存储与寿命 | B1基线可比较 | 分块追加、不可变前缀、自足owning结果、完整Replay与长历史保留/回收曲线 |
| B3 时间与执行协议 | B0、B1 oracle；触及存储的部分基于B2 | T2/G1/Q1/K1、R2内核与codec、连贯时钟/offset、参数化尾段；headless确定性闭包 |
| B4 宿主与资源分离 | B3 | 单owner协议首用、P3-a、fence/R2握手、表现回执、错误/关闭矩阵 |
| B5 部署与设备 | B4、有效N3、所需M参数与平台证据 | P-B线程部署、平台输入/音频、实体设备/实时同步和关闭；F1–F4最终收尾 |

B1/B2的独立准备可并行，B3的语义提交必须与存储版本对齐，不能把旧codec硬接新key。
B1提供初始数据；真实时钟下的drift必须B3后补测；跨worker性能/设备指标在B5迭代测量。
不能要求B1提前证明尚不存在的B5拓扑性能。

### B0 合同闭合

**产物与证据：** [RPA补充Spec](../../../formats/rpa-execution-contract.md)、
[宿主边界](../../../api/realtime-host-boundary.md)、[ADR0046](../../../adr/0046-realtime-playback-coordination.md)、
[B0修订记录](../../../stage_reports/stages/stage-07/rpa/2026-10-11-b0-contract-revision.md)、
[规划就绪审查](../../../stage_reports/stages/stage-07/rpa/2026-10-11-implementation-readiness.md)。
直接裁定的O1/I1/C1/L1/G1/Q1/K1与委托补充均有authority落点，旧规则替代表、版本与最小反例可定位。
文档检查通过后可登记B0设计退出；不登记运行证据或产品完成。

### B1 观测与基线

**实现顺序：** B1.1定义事件/ID与manifest → B1.2可控交付/时钟fixture与独立有理数/扫描oracle
→ B1.3原路径Release基线、平台所有权只读核对/授权取证 → B1.4输出测量设计与初始数据。
manifest绑定源码/dirty差异、配置、语义revision、内容、硬件/驱动、采样缺失和观测开销。
埋点覆盖 capture/delivery/admission/seal/Fold/publish/consume/present、control阶段、队列与资源保留。

**退出：** 同canonical/admission/control轨迹可复现并完整比较；缺失与观测开销可见；
平台捕获能力和实际停点有记录；EOF尾部timer分布、表现耗时p50/p95/p99/max、保留峰值提供初始输入。
不把当前合成hStep的结果称为真实clock drift；不以共享hosted runner毫秒数当硬预算。

### B2 存储与寿命

**实现顺序：** 日志分块追加 → immutable成功前缀 → 自足owning结果 → checkpoint/archive/cut/H绑定
→ 活跃状态/旧handle/最后释放验证。整体状态仍唯一owner，不用结构共享绕过可变状态所有权。
**退出：** 完整ReplayEvaluation和故障前缀相等；长历史推进/发布/读取/恢复/释放分别测量；
预留与分配失败、旧handle跨Close、最后释放尾延迟、无Session/World/设备反向依赖得到验证。
回退仅回代码，不删除新格式归档，不用score/hash摘要替代完整结果。

### B3 时间与执行协议

| 小批次 | 交付物 | 必须先通过的定向检查 |
| --- | --- | --- |
| B3.1 typed/版本 | occurrence/closed batch/control carriers、registry、identity投影、rev3字段顺序wire表/golden、schema2、错误/公开投影 | rev1/2拒绝、旧profile不偷换、同slot重复/异值冲突、完整字段边界、ON/OFF public差异 |
| B3.2 T2与推进 | O1/I1/C1/L1 reducer、连续前缀、G1/Q1/K1；先同线程驱动 | 同Tick press/release/press、和弦、转发合批、B等号、缺片/乱序、控制取消与执行互斥 |
| B3.3 R2/recovery | inclusive cut/drain、immediate/deferred控制提交、trace/quarantine、完整Replay/Seek、A1失败出口 | body/tail边界、future press暂停释放、无tail、同H不同cut、重复恢复、分配/Fold失败、Tick极值；R35–R37按最终切段/seal边界区分拒绝、终止续跑与实际fault |
| B3.4 时间桥 | 连贯观察、offset/一次舍入、clock invalid状态、参数化tail clock、废除生产hStep | 44.1/48k、负Tick、ties/溢出、旧缓存/欠载/设备换段、EOF余timer与pause rebase |
| B3.5 集成 | 新profile显式首用、参数化Scheduling/TailProfile、安装消费者、drift可测量性 | 独立oracle、完整结果与回执、旧路径兼容；testOnly参数不得作为生产profile |

B3.1公共修改按Spec §6下一minor策略；同一提交更新API/wire文档、诊断映射和消费者。
不改Chart/CXC/CXT字段，不从设备旧时间重建canonical，不在同Tick插多timer阶段。
B3退出给出真实时钟导出的H及F(H)/processedFrontier分项测量，不等同生产预算已接受。

### B4 宿主与资源分离

**实现顺序：** B4.1 narrow owner接口、按步骤Apply/Cancel及终态记录 → B4.2 P3-a资源/音频owning候选
→ B4.3 prepare/commit fence与部分失败 → B4.4 R2设备握手/回执 → B4.5 frame/present观测与有向Close。
先同线程可控调度验证所有消息顺序，再交B5迁移线程。保留SDK显式单owner facade。

**退出：** 发布失败不重复准入；软件/设备回执分别可查；Prepared generation失效可重建；
GPU仅主线程创建/激活/销毁；旧帧/scope/资源不可越界；trace缺口不直接续跑；
RemainingFrames不改；回执丢失Unknown、迟到只改历史诊断；关闭无主线程互等；
参数化容量/预留故障槽有满队列与分配失败证据。
A1清理保留实际cut与未准入输入，停止音频的确认不恢复直接续跑资格；B1取消须验证先于首次
Apply、执行中竞争、回执丢失、退休和多参与域回收。R35–R41退出前不得将这两条协议记为已闭合实施。

### B5 部署接入

**实现顺序：** audio owner完整创建/服务/隔离callback/析构 → Playback worker D2+D3
→ 主线程D1/D5/D6协议接入 → 代表与最坏内容测量/配置收敛 → 设备与最终矩阵。
主线程不join仍需主线程清理的worker；无设备模式/headless/Reference Host使用同协议显式驱动。

**退出：** DFJK/Hold、focus、pause/resume、Seek/reload、真实听音、欠载/丢设备、
present阻塞/刷新率、queue满、失效时钟、软件提交后设备失败、关闭均有分项证据。
对已选capture-time限制按合同验收，不承诺主线程阻塞期物理按键零延迟。
同SHA自动化、设备、预算与owner接受分别归F1–F4，不用Replay相等代替听音/键盘证据。

### 批次测试清单与退出记录

每项记录 fixture/故障注入点、独立预期、实际完整结果、源码/配置身份和未执行原因；
同一测试可覆盖多项，但不能仅凭最终 score、digest 或单一回执推断中间阶段正确。
定向测试先在可控同线程调度执行；B5 再以实际 owner/线程和设备重跑适用项。

| 批次 | 必须可判定的测试项 | 退出证据 |
| --- | --- | --- |
| B1 | 分别延迟捕获、交付、前缀、D2 裁决和 present；同输入轨迹重放至少两种合法消息交错；oracle 独立给出 Tick、批关闭、执行顺序与完整 Fact/Fold | 事件 ID/缺样/埋点开销、基线源码身份和平台约束；不以测量值自动生成通过阈值 |
| B2 | 活跃尾块跨发布、旧结果跨 Close、checkpoint/archive/cut/H 区分、分配失败与最后释放；逐字段比较完整 ReplayEvaluation 和 fault prefix | 峰值、复制量、释放尾延迟、失败位置及重试/回退可重复记录 |
| B3 | R01–R18、R24–R31及R35–R37；控制与前缀竞争、切段阶段、同ID重传/异值、Fold后缀失败、A1确定失败与等待/已seal的区分 | decisionOrdinal、原H/B/clock、实际调用、cut、续跑资格和完整结果；wire/golden、旧版本拒绝与旧profile比较 |
| B4 | R19–R23、R32–R34及R35–R41；在实际参与阶段注入失败/回执丢失，验证A1清理及B1取消竞争、终态重传、退休和回收 | 各域Pending/InProgress/终态未生效/Applied/Unknown、generation、资源释放顺序、实际宿主状态；无双提交、迟到执行或假回滚 |
| B5 | 同轨迹改变绘制阻塞/刷新率/交付节奏；真实 DFJK/focus/Hold/听音、EOF、欠载、设备失效、主线程清理与 Close；失效时确保仍能泵控制/故障 | 自动化与实体设备证据分栏，观察误差/容量/积压/同步和未执行项实录，移交 F1–F4 |

跨批次失败不得以缩小 fixture、调大 timeout 或丢掉未确认消息制造通过。若 B3/B4
新增状态只能靠跨线程时序复现，应先把该状态写成可控调度反例，再用 B5 线程证据补强。

### 测量与裁定门禁

| ID | 需取得的数据/明确产物 | 最晚门禁与缺失行为 |
| --- | --- | --- |
| M-TIME | 观察误差/有效期、source/device/presented差异、各offset来源；SchedulingProfile的测量记录 | B3可testOnly；B5生产实时启动前必须具备，否则profile_incomplete |
| M-TAIL | EOF连贯锚点、尾部timer/window分布；TailProfile起点/速度/输入期限/终止H | B3参数化；B5生产尾段启用前冻结；不得EOF直接漏判或批量Miss |
| M-SPLIT | B3后H lag/frontier lag、Fold lag、表现成本、最老未消费年龄、预声明分位数/持续窗口/阈值 | B5初始同worker；若证明D3导致超界，新增受限拆分实现/验收包；未知不能报告部署验收通过 |
| M-CAP | 输入/trace/控制/回执/资源/旧handle峰值、观测开销、停点和满队列行为 | B5生产通道容量、F3限额；缺失保留testOnly，不补默认容量 |
| M-FINAL | 五类预算与最终平台/设备矩阵 | F1–F4；最终owner接受仍独立 |

阈值由实测和预先声明的目标共同形成，并非“实测值自动就是合格线”；F3生产接受权限不变。
这些缺口已有负责批次、数据、失败行为和退出条件，故不阻塞本轮**实施规划**就绪。

## 综合收尾工作线（原S7A-9）

本节是原S7A-9剩余工作的唯一实施计划，plan-a保留旧编号和兼容导航。下列编号表达来源映射，
不是要求先F1/F2再F3：测量贯穿实施，F3最终裁定及相关实现先稳定，再运行F1/F2/A7，最后F4关闭。

| 新ID | 原ID | 工作与交付物 | 退出标准 |
| --- | --- | --- | --- |
| RPA-F1 | S7A-9.1 | Debug/Release、candidate ON/OFF、static/shared、headless、MinGW本地最终矩阵；architecture、target allowlist、公共头泄露/ASCII、package/external consumer、version/docs/diff及许可证检查 | 绑定最终行为SHA和配置；覆盖原内核/Fold、输入/公共消费者、完整Replay/Seek及RPA变化，逐项保留fixture、命令、输出、失败与未执行项 |
| RPA-F2 | S7A-9.2 | Linux Quality、Windows MSVC、Windows MinGW同SHA hosted证据；记录本地/hosted差异 | 所需job实际通过；sanitize/coverage/clang-tidy仅使用支持的平台，GPU/设备另列；旧SHA日志不冒作新证据 |
| RPA-F3 | S7A-9.3 | 内容、稳态、快照/Seek、Replay、Packed五类预算与回归报告；代表/最坏内容的requirement、activity、Pattern状态、Fact、prepare峰值和运行时间等机器可读测量 | 计数口径、环境、实测、拟议阈值及硬/软属性分开；含maxSeekLatency等原待测项；owner接受后才落实限额，超限行为回归，不能缩小fixture或扩大既有限额制造通过 |
| RPA-F4 | S7A-9.4 | 面向7B+的完整交接及Stage8 typed handoff输入、v4/v5输入映射、requirement/Replay identity、golden、capability/拒绝表、预算profile、迁移与回退说明、未选7B+清单、dated completion与问题处置 | 独立诊断前置N3退出记录有效，B0–B5及F1–F3适用门禁全部满足；owner明确接受RPA和移交的7A收尾；合并/发行另按既有权限 |

原S7A-9的执行责任、预算接受工作和Stage 8 handoff均随迁移转移，验收强度不减少。
已有I78-6计数/测量/handoff草稿、S7A-0.3五类预算输入及旧报告按原SHA保留；
它们是准备材料，不是F1–F4通过证据。历史合同中的“S7A-9”现在指向本节对应F项，不另生7A待办。
F3不能把故障注入100/500ms等值当生产阈值，也不替代Stage8的40k/16 MiB发行门禁。
F1/F2/A7维护一份最终覆盖矩阵，允许同SHA同配置证据映射多个要求；行为或限额改变须重跑受影响门禁。
F4完成时一并记录原7A关闭要求的接受结论；本次转移不提前宣布Stage7A或RPA完成。

## 原A0–A7兼容映射

A0–A7是2026-10-09迁移时的追溯编号，保留以便历史报告可对照；后续实施与验收按B0–B5执行。
本表不改变任何已完成工作的证据边界。

| 原ID | 原内容 | 现在的位置 |
| --- | --- | --- |
| RPA-A0 架构审查 | 实际代码/合同/平台约束、RA反例、复制清单、证据限制 | 已完成，作为B0与B1的设计输入；证据见[10-09审查](../../../stage_reports/stages/stage-07/readiness/2026-10-09-realtime-architecture-review.md) |
| RPA-A1 边界设计与typed review | 完成TB-01–07，逐字段补owner、单位、捕获/准入时点、profile/identity、控制矩阵与失败码 | **B0**；分析输入见[10-11审查](../../../stage_reports/stages/stage-07/rpa/2026-10-11-design-and-entry-review.md)与[生产架构方案](../../../stage_reports/stages/stage-07/rpa/2026-10-11-production-architecture-design.md) |
| RPA-A2 调度与存储验证设计 | 可比较轨迹、成本/复制/保留/积压与观测开销口径、fake clock、独立oracle、平台取证 | **B1**（观测与平台取证）+ **B2**（存储成本口径） |
| RPA-A3 架构与线程模型选择 | 比较五种状态组织方向及ADR0046部署候选，纸面筛选后以关键实验裁定 | **B0**（D-AD02裁定）+ **B1**（实验证据）；五方向比较输入见10-11审查 |
| RPA-A4 后续热路径整改 | 移出每帧完整Replay，处理journal/RuntimeState/公开result复制与retention | **B2** |
| RPA-A5 后续时钟与发布实施 | 实施已冻结typed bridge；解开判定推进与表现采样频率；资源/快照/控制消息有版本和owner | **B3**（时钟与捕获）+ **B4**（发布与接口分离） |
| RPA-A6 后续owner调度实施 | 按已选择拓扑创建/析构对象，音频不等GPU；有界可靠输入/控制及可合并frame通道 | **B5** |
| RPA-A7 后续消费者验收 | Headless/Player/Host/安装树、真实设备与同步；回填RPA验收报告和原C78关联复验行 | **B5**，与F1/F2共用最终行为SHA和运行矩阵，各自覆盖项明确，不互相代验 |

原A表RPA-A1的「未闭合项目继续标open」口径由B0的决策台账承接，不再另立状态词。

## 正式命名工作线

本标题保留为旧链接兼容入口。2026-10-10 owner最新归属调整已将N0–N3移出RPA，
完整目标、已选方案、合同收敛、生产者/消费者迁移、版本/载体兼容、验收与回退统一归
[诊断码正式化独立前置计划](plan-diagnostic-formalization.md)。本分册不维护第二份N0–N3批次表。

[N0历史复核](../../../stage_reports/stages/stage-07/rpa/2026-10-10-n0-diagnostic-inventory-review.md)
只记录当时静态清单；后续合同同步、迁移和运行验证由
[N3独立退出报告](../../../stage_reports/stages/stage-07/diagnostic-formalization/2026-10-10-n3-verification-and-exit.md)
提供证据，最新状态归CURRENT_STATUS。历史报告/ZIP仍留原路径，目录名不表示继续属于RPA内部工作线。

RPA准入须取得独立N3退出记录、实际源码/配置证据、公开投影与旧载体拒绝说明、
SDK版本结论及回退入口，再建立新架构比较基线。RPA新变化仍须在B5/F线回归相应合同，
但不重做N线或借自身矩阵代替独立N3。

## 实体设备与实时同步验收归属

| RPA验收项 | 证据要求 |
| --- | --- |
| 实体DFJK与Hold、焦点/暂停/恢复 | 人工操作记录与实际输入/控制/完整结果，区别定向Win32投递测试 |
| 真实音频、设备丢失与恢复 | 实际听音/设备日志、欠载与时钟有效性/分段、恢复资源重取 |
| 卡顿、刷新率、输入捕获与交付 | 分开改变绘制、交付、捕获；按已定语义验证，不承诺所有延迟结果相同 |
| 音频/判定/表现同步 | 共同时间合同、观测来源与误差证据；不能用Replay=same代替 |
| 平台设备缺项 | 逐项标未执行或失败；RPA退出前与F线统一收口，不移回Stage7A |

7B+提供的高级动作和校准能力另有各自验收；上述基础设备验收不吞并7B+计划。

## 验收标准

1. 决策台账逐项有裁定并落入所属合同，最小反例、owner/identity/错误与状态转换可追溯；
   正式线程选择有平台与测量证据，无隐式raw时间回溯、Tick递增规避碰撞或生产阈值。
2. 音频服务不依赖GPU等待；同admission/control轨迹跨渲染节奏完整结果等价；
   延迟捕获和延迟交付分别按已定合同验收，完整ReplayEvaluation保留。
3. 资源/帧/控制跨代、暂停释放、恢复、欠载、队列满、fault、关闭和分配失败有独立反例与故障注入。
   真实设备、音频和窗口证据单列，自动投递键盘及Debug探针不能替代。
4. 退出时必须取得有效的独立诊断前置N3记录，RPA采用其正式码、显式投影、Recovery版本/兼容及SDK基线；
   架构变化对这些合同的影响在本阶段定向回归，历史日志和算法身份不批量改写。
5. 受影响Debug/Release、candidate ON/OFF、headless、static/shared、MinGW/Linux及
   架构/package/ASCII/诊断码/docs/version门禁逐项有最终行为SHA证据。按Spec §6的新增公共能力采用下一minor，
   实施时核对实际基线并完成差异审批；版本改变需updater、fresh configure、clean-first与消费者。
6. F1–F4完整承接原S7A-9；预算裁定、最终矩阵、typed handoff和原7A关闭包在RPA退出前一并完成。
   交付RPA阶段报告及Stage7B+/Stage8交接，owner接受与发布权限分开；未执行项显式保留且不据此宣称关闭。
7. B0–B5每批有定向验收与回退边界；回退代码不保证旧版本可读取已产生的新archive，
   应保留原件与版本规则；语义修订与机械重构分开提交。

## 排除项与停止条件

不增加第二判定路径，不安装内部Kernel/Recovery执行图，不隐式改变既有S7A-2–6语义。
不引入通用任务图/actor框架、运行时脚本或逐帧回调；不建Studio、独立GUI或在线监控服务。
不实施S7B+/S7C高级能力、Stage8发行或Stage12规模化优化；生产预算与原S7A-9关闭工作已纳入F线。
不代审批、不改保护、不新建PR或改变现有分支。

新增真实合同冲突先登记最小反例和修订影响；存在未定语义时停止对应消费者实施，继续独立工作。
未选语义不得由消费者实现补默认值。新原型须具备对应B0裁定、B1测量设计、纸面准入、
N3有效基线和独立授权；产品实施还需后续授权。

## 交接与追溯来源

- 设计已就绪；后续获得实施授权后从B1开始，按上表依赖推进B2–B5；核对N3退出及后续差异，
  不重复诊断迁移。当前授权仍仅设计与文档，不自行启动原型/代码/设备运行。
  先核验实际branch/HEAD/status/diff，每次区分裁定、实施与证据。
- [生产架构方案](../../../stage_reports/stages/stage-07/rpa/2026-10-11-production-architecture-design.md)：
  六责任域、部署候选、时间桥与推进许可的选择依据、存储寿命、失败与关闭顺序、AD-01–15历史比较。
- [A1–A3设计与准入审查](../../../stage_reports/stages/stage-07/rpa/2026-10-11-design-and-entry-review.md)：
  TB-01–07反例、DR-01–05、A2测量表与五方向纸面比较输入。
- [根目录v0.5初稿归档](../../../archive/stage-07-planning/2026-10-10-rpa-target-architecture-draft.md)：
  保留原设计与反例；架构筛选/实施顺序以本分册为准，命名迁移归独立前置计划。
- [S7A-7/8受限收口与8.4关闭证据](../../../stage_reports/stages/stage-07/verification/2026-10-10-s7a-8-4-closure.md)。
- [架构审查](../../../stage_reports/stages/stage-07/readiness/2026-10-09-realtime-architecture-review.md)及
  [正式命名审计](../../../stage_reports/stages/stage-07/rpa/2026-10-09-diagnostic-phase-name-audit.md)。
- [归入Stage 7子阶段的修订记录](../../../stage_reports/stages/stage-07/handoffs/2026-10-09-rpa-stage7-substage.md)
  与[独立阶段拆分记录](../../../stage_reports/stages/stage-07/handoffs/2026-10-09-realtime-stage-separation.md)：
  保存原plan-a增补文本和迁移映射；历史RT78-0–7对应RPA-A0–A7，DN78-0–3对应RPA-N0–N3。
- [Stage7总计划](plan.md)与[plan-a兼容入口](plan-a.md#3418-实时架构重审与规划2026-10-09)。
- [ADR0046](../../../adr/0046-realtime-playback-coordination.md)和[实时宿主边界草案](../../../api/realtime-host-boundary.md)。

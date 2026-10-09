# Stage 7-RPA Plan: 实时播放架构与接口规范化

状态：active；Stage 7 子阶段规划，尚未开始产品实施

更新日期：2026-10-10

## 阶段目标

在已实现的Gameplay内核与Playback集成上，建立可靠的输入/音频/判定/表现时间关系、
明确的所有权和资源/快照发布协议，解决实际Player实时成本与调度问题；同步完成阶段诊断码
正式命名及公共/持久化兼容治理。RPA是Stage 7内的子阶段代号，与7A、7B+分册共同受Stage 7总计划约束；
不占用既有S7B/S7C编号，不重排Stage8–14。RPA-A0–A7及RPA-N0–N3工作编号保持。

本分册按2026-10-09 owner最新归属指令纳入Stage 7，承接原RT78和DN78工作。工作范围、实施顺序和验收以此处为唯一来源；
Stage7 plan-a §3.4.18/19仅保留交接索引。当前授权仍是规划，不授权本轮产品代码实施。

## 前置条件与执行位置

Stage7A完成原定内核/集成/预算验收，Stage7B+保留独立高级能力线；RPA基于7/8已有基线
负责实时系统、正式命名以及实体设备与实时同步验收，三者成果按依赖交给Stage8。
RPA设计无需等待Stage7A关闭，S7A-9原范围验收也不以RPA完成为前置；设备验收不再回挂7A。
Stage8消费7A与RPA及已选入的7B+能力，不要求整个7B+先结束。
RPA设计不等待8.4 owner门禁退出；合并/发行仍服从相应版本、审批和保护门禁。

- 输入基线为现有typed source→Graph/Packed→Entry→prepare、实际kernel/Fold、Replay/Seek、
  optional FactBinding和安装消费者；原有证据边界按对应SHA保留。
- 使用现有V2 Spec/ABI、execution/author profile及Playback/Audio/Animation/Presentation合同。
  TB-01–07未定项先裁定，再实施首用消费者；保留已完成判定规则。
- 架构重构和命名迁移各自形成可审查提交及测试证据；交叉影响通过共享兼容矩阵验证。
- 实体DFJK/focus/control、真实听音、设备失效/恢复、卡顿/刷新率/输入延迟与实时同步，
  包括实施和最终验收，整体归RPA。旧失败作为待办基线，不改记通过或回挂S7A-9。
  原S7A-8.4已按[10-10记录](../../../stage_reports/stages/stage-07/verification/2026-10-10-s7a-8-4-closure.md)收口；
  RPA后续API变化仍需独立版本差异、审批与门禁证据。

## 范围与工作线

五项责任为输入捕获、音频服务、确定性执行、表现采样和设备提交；责任数不等于线程数。
音频独立owner是优先验证方向；Playback worker须先闭合Prepared/renderer、输入交付与组合事务。
阶段诊断码遵循模块/子域/具体原因命名；公共性、恢复载荷兼容和判定身份分别验证。

### 与既有合同和集成面的关联（不改变任务归属）

| 关联原集成面 | RPA承担的工作 | 关联首用行/既有边界 |
| --- | --- | --- |
| S7A-7.1 | 校准会话时钟、输入准入与推进顺序、明确H/T/RuntimeFrame；控制、推进/表现采样和恢复边界 | C78-01–04/08；R78-01–05，保留sealed前缀、faulted只读、generation/scope区分 |
| S7A-7.3 | 可控时钟和延迟交付fixture、独立时间oracle、同轨迹完整Replay/Seek验证 | C78-03/08/09；不能用生产推进函数充当oracle |
| S7A-7.4 | Player实际输入、音频服务/渲染调度解耦、热路径Replay整改、快照与资源代数一致 | C78-03/04/07/11；R78-06/07，resolver层域和typed lifetime保持 |
| S7A-7.5、8.1 | 任何新公共边界的ON/OFF、owner、ASCII及static/shared安装消费 | C78-08/10/12；无内部Kernel/Recovery安装，无默认生产新能力 |
| S7A-8.3 | Reference Host同协议六动词加tick，typed clock供给与拒绝矩阵 | C78-04/11；fixture步进保留显式用途，不伪称实时设备模式 |
| S7A-8.4 | 按RPA最终公共差异重审patch/minor与owner/保护/hosted材料 | C78-12；7/8既有门禁已收口，当前0.7.1不预先约束RPA未来API |
| S7A-7.2、8.2 | 保留已完成source/assembler闭包；新时间profile若影响identity则定向回归 | C78-05/06，R78-08/09沿既有Graph/发布合同；不趁机重写格式 |
| S7A-6.1–6.5关联回归 | 执行状态、journal和owning结果存储整改的完整语义回归 | 完整ReplayEvaluation、archive/cut/H、future pending、lastObservedTick、惰性分支不变 |

RA-02/03/04是真实语义或产品支持范围问题，不作为“普通表示”交给代码自行决定。若选择原始事件
时刻重建、改变碰撞支持或新的推进fence，必须先明确Spec/ABI/profile/identity/稳定诊断修订，列出
被替换文字和影响测试；不因大范围重构建议而默认重开全部S7A-2–6规则。

### 架构工作线

| 步骤 | 必需产物及完成条件 | 后续依赖 |
| --- | --- | --- |
| RPA-A0 架构审查 | 实际代码/合同/平台约束、RA反例、复制清单、证据限制；已形成审查输入 | 是设计输入，不是产品修复 |
| RPA-A1 边界设计与typed review | 完成TB-01–07，逐字段补owner、单位、捕获/准入时点、profile/identity、控制矩阵与失败码；现有语义和候选修订明确分开 | 时间消费者实施前置；未闭合项目继续标open |
| RPA-A2 调度与存储验证设计 | 定义可比较的输入/admission/control轨迹，准备计时/复制/保留内存/队列积压的测量方案；设计fake clock、独立oracle和设备试验 | 不预设线程数、无锁或生产阈值；现有证据只含基线测量 |
| RPA-A3 线程模型选择 | 比较ADR0046候选；音频独立服务为优先方向，Playback worker须先通过Prepared/renderer跨域、提交fence与输入采集可行性审查 | 选择需成本及平台证据；当前不冻结三线程为正式实现 |
| RPA-A4 后续热路径整改 | 移出每帧完整Replay，处理journal/RuntimeState/公开result复制与retention；完整恢复和故障注入回归 | 不能只把同样的增长成本搬到worker；需另轮实施 |
| RPA-A5 后续时钟与发布实施 | 实施已冻结typed bridge；解开判定推进与表现采样频率；资源/快照/控制消息有版本和owner | RPA独立实施；每个提交可独立验证，保留旧显式测试桥兼容 |
| RPA-A6 后续owner调度实施 | 按已选择拓扑创建/析构对象，音频不等GPU；有界可靠输入/控制及可合并frame通道 | 不跨线程使用Prepared，不因队列满丢键，不把新容量默认为生产接受 |
| RPA-A7 后续消费者验收 | Headless/Player/Host/安装树、同SHA平台矩阵、真实设备；回填RPA验收报告和原C78关联复验行 | RPA独立验收；设备/同步在RPA独立退出，不以S7A-9复验替代 |

RPA-A0沿用已形成审查证据；A1–A3已有设计输入，字段冻结和线程选择仍需后续完成。
本计划独立管理RPA工作，不将其计入I78-0–5进度；现有审计不构成阶段验收。

### 验证设计与后续阶段分工

验收区分三类：只改变渲染节奏；已捕获输入延迟交付；输入本身延迟捕获。第一类在同admission/control
前提下比较完整结果；后两类按明确捕获与late-policy合同验收，不能无条件要求与无延迟相同。
100ms/500ms等停顿只作故障注入输入，不能成为生产容忍值。额外覆盖音轨末尾尾判、44.1k/48k转换、
offset重复补偿、Pause/Seek、跨代旧帧、设备欠载、队列满、关闭与分配失败、长历史和owning对象寿命。
GPU/window/audio/实体键盘证据单列，不能用Replay=same代替。

S7A-9按原内核/公共消费者范围收敛矩阵、预算和关闭；RPA独立承担实时设备验收，
两者不互相代验。RPA仅累计后续成本输入，不默认接受新生产阈值。
Stage7B+含S7C-1，扩展高级CalibrationProfile与设备策略；基础实时同步归RPA。
Stage8消费已验证合同并做发行兼容；Stage12做规模/平台优化，不承担首次修复基础播放失速。
当前状态只回填CURRENT_STATUS，dated报告存证，桌面handoff同步摘要。

### 正式命名工作线

| 顺序 | 首用修订/实施要求 | 验收边界 |
| --- | --- | --- |
| RPA-N0 审计 | 保留逐码映射、定义/消费者/测试/合同，排除历史、算法版本和fixture ID | 本轮已完成审计，未实施 |
| RPA-N1 合同 | Spec/ABI/集中码表与execution/Recovery/Replay兼容、VERSIONING逐项证明 | 正式名称不提升公开性，不扩九类别/R编号，不默认接受旧恢复bytes |
| RPA-N2 消费者 | source_codes、s7a4动态构造、Playback显式投影、严格恢复验证及工具预期同步 | 不靠阶段前缀决定公开性，保留sourceCode、故障边界和完整结果 |
| RPA-N3 验证 | 新码负例、投影、旧owning结果、故障注入和新旧恢复字节/digest、消费者及平台矩阵 | 新SHA证据；历史日志保留原码；门禁缺项准确登记 |

## 实体设备与实时同步验收归属

| RPA验收项 | 证据要求 |
| --- | --- |
| 实体DFJK与Hold、焦点/暂停/恢复 | 人工操作记录与实际输入/控制/完整结果，区别定向Win32投递测试 |
| 真实音频、设备丢失与恢复 | 实际听音/设备日志、欠载与时钟有效性/分段、恢复资源重取 |
| 卡顿、刷新率、输入捕获与交付 | 分开改变绘制、交付、捕获；按已定语义验证，不承诺所有延迟结果相同 |
| 音频/判定/表现同步 | 共同时间合同、观测来源与误差证据；不能用Replay=same代替 |
| 平台设备缺项 | 逐项标未执行或失败；RPA退出时自行收口，不移回S7A-7/8或S7A-9 |

7B+提供的高级动作和校准能力另有各自验收；上述基础设备验收不吞并7B+计划。

## 验收标准

1. TB-01–07逐项落入所属合同，最小反例、owner/identity/错误与状态转换可追溯；
   正式线程选择有平台与测量证据，无隐式raw时间回溯、Tick递增规避碰撞或生产阈值。
2. 音频服务不依赖GPU等待；同admission/control轨迹跨渲染节奏完整结果等价；
   延迟捕获和延迟交付分别按已定合同验收，完整ReplayEvaluation保留。
3. 资源/帧/控制跨代、暂停释放、恢复、欠载、队列满、fault、关闭和分配失败有独立反例与故障注入。
   真实设备、音频和窗口证据单列，自动投递键盘及Debug探针不能替代。
4. 66个正式命名与占位处置完成；显式公共投影、旧owning值、旧新Recovery/Replay字节/digest
   与兼容revision验证；历史日志和算法身份不批量改写。
5. 受影响Debug/Release、candidate ON/OFF、headless、static/shared、MinGW/Linux及
   架构/package/ASCII/诊断码/docs/version门禁逐项有最终行为SHA证据。公共差异决定patch/minor，
   不预设版本；版本改变需updater、fresh configure、clean-first与消费者。
6. 交付RPA阶段报告及Stage7B+/Stage8交接，owner接受与发布权限分开；未执行项显式保留。

## 排除项与停止条件

不增加第二判定路径，不安装内部Kernel/Recovery执行图，不隐式改变既有S7A-2–6语义。
不实施S7B+/S7C高级能力、Stage8发行或Stage12规模化优化；不接受生产预算或执行S7A-9最终关闭。
新增真实合同冲突先登记最小反例和修订影响；存在未定语义时停止对应消费者实施，继续独立工作。
不代审批、不改保护、不新建PR或改变现有分支；执行策略须依据后续明确任务授权。

## 交接与追溯来源

- 下一次对话以本子阶段审核为目标，先核验实际branch/HEAD/status/diff，再复验RA反例、
  TB-01–07、A1–A3与N1的首用合同缺口、兼容/identity/失败边界及独立验证设计。
  输出dated审核报告、分册决策修订和实施准入清单；现有规划不自动授权产品实施。
- [S7A-7/8受限收口与8.4关闭证据](../../../stage_reports/stages/stage-07/verification/2026-10-10-s7a-8-4-closure.md)。
- [归入Stage 7子阶段的修订记录](../../../stage_reports/stages/stage-07/handoffs/2026-10-09-rpa-stage7-substage.md)。
- [Stage7总计划](plan.md)与[plan-a兼容入口](plan-a.md#3418-实时架构重审与规划2026-10-09)。
- [ADR0046](../../../adr/0046-realtime-playback-coordination.md)和[实时宿主边界草案](../../../api/realtime-host-boundary.md)。
- [架构审查](../../../stage_reports/stages/stage-07/readiness/2026-10-09-realtime-architecture-review.md)及
  [正式命名审计](../../../stage_reports/stages/stage-07/readiness/2026-10-09-diagnostic-phase-name-audit.md)。
- [独立阶段拆分记录](../../../stage_reports/stages/stage-07/handoffs/2026-10-09-realtime-stage-separation.md)
  保存原plan-a增补文本和迁移映射；历史RT78-0–7对应RPA-A0–A7，DN78-0–3对应RPA-N0–N3。

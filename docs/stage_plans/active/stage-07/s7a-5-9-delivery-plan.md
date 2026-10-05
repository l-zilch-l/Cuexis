# S7A-5–9 Delivery Plan and Readiness Assessment

状态：active planning；下一次联合实施S7A-5与S7A-6，5–9本轮均未开始产品实施

更新日期：2026-10-06；初版日期：2026-10-05

## 1. 基线、权威和评估方法

当前状态只由 [CURRENT_STATUS](../../../CURRENT_STATUS.md) 维护；本文件负责剩余批次的顺序、范围与门禁，
不替代 [Stage 7 总计划](plan.md)、[V2 Spec](../../../formats/GAMEPLAY_V2_SPEC.md) 或
[V2 ABI](../../../api/GAMEPLAY_V2_ABI.md)。代码行为基线为
`a0c8b7e4f783995bc19e626f3654ab11f845a0a7`，SDK0.7.0、日期build26.10.05-1。
S7A-3/4 已达到“受限功能验收完成，容量整体证明未完成”；同源 head SHA 的7个 hosted runs 全绿，
详见 [新 SHA hosted 与交接记录](../../../stage_reports/stages/stage-07/2026-10-05-s7a-3-4-hosted-and-handoff.md)。
CI 通过不替代 S7A-9 容量阈值接受、真实设备验证或 owner 阶段接受。

本次评估读取既有裁定、公开角色/内部接口和生命周期实现的相关声明，不进行新产品实现。
工作量按待交付闭包、首用合同与失败边界给出相对评级，不编造工时或以测试数量估算开发速度。

## 2. 五个批次的评估

| 批次 / 小目标 | 当前可复用基础 | 首次消费与准入缺口 | 相对工作量 / 风险 | 本轮选择 |
| --- | --- | --- | --- | --- |
| S7A-5，5.1–5.5 | sealed Fact Ledger、T4/K4 phase outcomes、canonical order、t+1信号、owning query/reset | 内存profile/grade/数值/Register/事务表示由九项方案选优给出明确推荐；首卡补齐并登记消费字段 | 高；失败边界最集中，影响后续状态闭包 | 下一次联合目标的前半，先锁定状态并验收 |
| S7A-6，6.1–6.5 | 四分量identity骨架、规范输入、prepared graph与kernel状态 | 消费5的提交状态；full DTO、wire、admission journal与Seek cut按推荐补齐；当前产品仍拒绝 | 很高；恢复正确性与线格式首次闭合 | 下一次联合目标的后半，同会话完成5/6恢复验收 |
| S7A-7，7.1–7.5 | Playback SDK、Chart candidate、CXC、Presentation、旧consumer门禁 | 需要5/6结果及恢复入口；Graph/Packed共同prepare、manifest closure、只读表现桥、headless/Player/安装包入口未接线 | 很高；跨模块兼容和candidate隔离 | 5/6退出后实施 |
| S7A-8，8.1–8.4 | 默认candidate开关、离线author工具、Reference Host六动词基线、现行版本门禁 | 需要7的真实入口/安装；四项Stage6交接不能沿用旧证据；D-9 master落库及SDK0.7.1显式放行是owner门禁 | 高；实现与owner决策必须分开 | 7退出后收口；owner事项可提前安排 |
| S7A-9，9.1–9.4 | 3/4的golden、跨编译器证据、局部计数/进程观察值 | 需要5–8完整对象模型；五类预算真实/最坏fixture、阈值分别接受、最终行为SHA矩阵、Stage8 handoff和owner acceptance | 很高；容量证据、平台差异和阶段关闭风险 | 最后收口；测量记录从5开始积累 |

当前内部 `JudgementSession` 的typed configure/prepare/submit/advance/query/reset已具备3/4行为；
`SnapshotPayload` 载体和无参数旧动词不代表6已实现，snapshot/seek仍稳定拒绝。
未找到已实现的PreparedRuleset、ScoreState、ComboState、StatisticsSnapshot或StateDelta产品闭包；
ABI中的角色追踪也不等于字段表示已经冻结。下一轮首先核对这一差异。

### 2.1 S7A-5：Fold与提交状态

交付PreparedRuleset、显式静态模块manifest/Interface、三类Register、原子StateDelta、
Score/Combo/Statistics及owning查询，并把其语义投影纳入ruleset identity。
首卡按 [九项方案选优](s7a-5-6-design-selection.md) 的明确推荐补齐字段、整数表示、
初始化/负值/溢出/饱和策略、允许operator及缺grade处理；
选择须写入Spec/ABI对应补充，未选定字段不得以临时typedef或默认表绕过。

最重要的两个边界：kernel未seal失败保留旧Fact前缀；Fact已经seal后的Fold失败保留新Fact，
只回滚本Tick的Fold状态/RuleEffect/发布事件。二者必须有不同的失败注入golden。
`commutative_monoid` 必须在声明的carrier上闭合、可交换、可结合；不能仅凭整数加法的数学名称证明。
例如有符号checked sum的MAX、1、-1会因中间溢出受分组影响，饱和有符号加法也不能自动视为合法monoid。

5.5的“Statistics snapshot”指只读owning统计投影；真正session Snapshot/Seek/Replay归6。
先在5验证已提交Ledger重建Fold/统计的等价性，再在同会话6实现真实seek/replay恢复；
联合退出时回填5.5与6.3–6.5，不能把5.5整行延期或以内部fixture冒充实际seek。
实施卡与退出矩阵见 [S7A-5/6联合接手](s7a-5-6-implementation-handoff.md)。

### 2.2 S7A-6：完整状态与首次wire

按Spec§3.18逐项核对十四条SnapshotPayload闭包；在5的commit状态、Hook和统计确定后，
冻结内存字段与codec版本、字节布局、EventCodecId、FactId/CommitId编码、预算descriptor及跨工具链golden。
先identity/预算preflight和原子Reader，再录制/Replay，再Snapshot，再任意Seek/恢复对照。

退出要求包括逐字段Fact/Score/Combo/Statistics一致、旧active保持、截断/篡改/乱序/重复/未知版本拒绝，
以及faulted不能新建snapshot。预算descriptor不是已接受生产限额，maxSeekLatency仍不得填猜测数值。
表现缓存、未提交delta、连续采样状态和snapshot interval不得进入payload/identity。

### 2.3 S7A-7：单一产品入口与兼容

先完成Playback/Prepared Playback owner-thread入口与无输入休眠，再接Graph/Packed共同prepare及CXC entry；
接着只从已提交FactBinding构造表现投影，验证early/exact/late/Miss的render.visible生命周期。
最后验证headless、Player与static/shared外部消费者都经过同一路径，保留v4回退和默认production隔离。

必须逐项消费第6轮已接受的entry/manifest/identity/capability/closure合同及诊断码表CTests，
不能把已接受条目重新写成open，也不能把“语义已裁定”写成接线已完成。

### 2.4 S7A-8：四项交接与owner门禁

8.1验证default-OFF/explicit-ON/错误entry及至少一个实际消费candidate的preset或CI；
8.2接入真实离线assembler并移除测试feature注入；8.3在同一新SHA运行
open/play/pause/seek/reload/quit矩阵，tick仅是运行时步骤；8.4独立完成SDK版本条件化放行和consumer兼容。

本次实时核对发现：D-9 shell视图探针/System32 bash拒绝已在stage-7提交a198da6存在，
`origin/master=5472c463640cf3b66a03dd86fe87bd87239b2659` 的测试脚本没有该守卫。
因此历史“工作区未提交补丁”不可沿用为当前状态，但owner具名复核/master落库仍未取得退出证据。
当前0.7.0 Version Gate成功不等于0.7.1可发行；实现方不得自行merge master、修改保护规则或放行SDK。
D-12历史研究稿修改仍需原登记的owner确认，不阻塞5，不在本轮改写研究稿。

### 2.5 S7A-9：五类容量和最终交接

内容、运行稳态、Snapshot/Seek、Replay/解码、Packed wire分别计数与接受阈值，遵守“谁产生谁计数”。
从5起保留实际module/register/贡献/Fact/状态峰值和耗时分布，6增加事件/字节/快照/Seek指标，
7/8增加实际内容与生命周期fixture；各批采样只作证据积累，不关闭9。

最后以最终行为SHA执行本地与hosted矩阵，列明GPU/真实设备/音频证据限制，形成预算profile、
capability拒绝表、未纳入7B+清单、回滚路线、Stage8交接包和owner接受记录。
任何硬门禁缺失都保留INCOMPLETE GATE；不能以研究上限或UINT64_MAX作为新业务限额。

## 3. 下一次对话的选择

| 方案 | 可得到的结果 | 代价 / 风险 | 结论 |
| --- | --- | --- | --- |
| 只做5 | 得到Fold基线 | 不符合owner澄清的多个批次目标，真实统计恢复仍跨会话悬挂 | 不选 |
| 联合5+6 | 同会话闭合Score/Replay/Snapshot/Seek与5.5 | 高工作量；九项推荐先落字段表，J0–J7串行门禁拆开失败责任 | **选定** |
| 联合5+6+7 | 再取得Playback/Player产品接线 | 同时扩大状态/wire/SDK桥/consumer检查面 | 下一轮之后再评估 |
| 联合5–8 | 再完成四项Stage6交接 | 叠加D-9 master/具名复核与SDK放行owner前置 | 当前前置不齐全 |
| 联合5–9 | 关闭全Stage7A | 阈值接受、最终SHA矩阵、设备与owner接受均须独立证据 | 当前不能预先承诺 |

执行顺序仍为5→6→7→8→9；下一轮将5/6放在同一会话，保留上游状态与下游codec的内部消费门禁。
计划取代854efa3的单批排期。批次组合有五套，九项未定实现决策各有五套方案和推荐，
见 [选优记录](s7a-5-6-design-selection.md)。下一轮目标为5.1–5.5与6.1–6.5的联合受限功能验收；
预算整体留9，不授权提前实施7–9。

## 4. 交接、停止条件与来源

使用 [下一轮联合接手指令](s7a-5-6-implementation-handoff.md)。首先读取当前状态、Spec§3.14–3.22、
ABI域6及第5轮裁定，再消费当前execution profile；出现真正合同矛盾须提供最小反例并修订权威合同。
普通实现错误在原合同内修复；当前未决字段不能用伪成功/默认值绕过。

本轮不改生产代码、CMake、CI、SDK或版本；仅规划和清理。新的实施、commit/push及owner-only行为
以对应会话明确授权为准，已经授予的授权不重复请求。

历史输入：[第5轮](../../../stage_reports/stages/stage-07/2026-10-03-s7a-5-6-gate-rulings.md)、
[第6轮](../../../stage_reports/stages/stage-07/2026-10-03-s7a-6-gate-rulings.md)、
[第7轮](../../../stage_reports/stages/stage-07/2026-10-03-s7a-7-wrapup-rulings.md)、
[旧S7A-3/4实施交接](s7a-3-4-implementation-handoff.md)。历史记录的当时状态保留，不覆盖实时评估。

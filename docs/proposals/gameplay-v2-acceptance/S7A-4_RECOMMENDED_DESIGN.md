# S7A-4 推荐方案登记

状态：recorded recommendation；未冻结新增字段、公共编码或数值限额

2026-10-05 后续：本文件保留方案比较与当时的未决清单；当前完整选定方案已落入
[ADR 0045](../../adr/0045-gameplay-v2-execution-profile.md)、
[execution Spec](../../formats/gameplay-v2-execution-profile.md) 和
[实施交接](../../archive/stage-07-planning/s7a-3-4-implementation-handoff.md)。
本文件中“仍需细化”的历史文字不再作为新增语义阻塞；运行验证/容量证据仍按交接计划补齐。

日期：2026-10-05（Asia/Shanghai）

上级文档：[Stage 7 计划](../../stage_plans/active/stage-07/plan.md) §S7A-4 ·
[Gameplay V2 acceptance package](README.md)

## 1. 授权、基线与文档角色

owner 在本次对话要求把上一轮推荐方案记录到文档，并询问 S7A-3 尚缺哪些部分。
本轮只记录设计建议、核对既有证据并维护导航；不实施产品代码、不提交或推送。
核对基线为 `stage-7`、`6b11d102f6dcac4d9782cd03d0bae15add54af28`，编辑前工作区干净。

推荐采用 **组合 B：参考模型与实用内核并行**。本文件保存候选比较、推荐理由、
待细化字段和验收入口；记录推荐不等于逐字段合同已经接受或冻结。
尤其 late-policy 的关系校验门禁不会因选定设计方向而自动关闭。

权威分工保持不变：ADR 拥有决策，Spec 拥有运行语义，ABI 拥有 typed 边界，
计划拥有范围，报告拥有带日期证据。当前实施状态只由
[CURRENT_STATUS](../../CURRENT_STATUS.md) 拥有。本登记不新增 CONTRACT_MATRIX 项、
裁定轮次、公共诊断码或 ABI 类型，也不改既有追踪计数。

## 2. 保留的已接受合同

以下不是本次重新选择的事项：

- [Spec §3.9](../../formats/GAMEPLAY_V2_SPEC.md)：六步 Tick / 八阶段唯一映射，
  timer 在同 Tick 输入前处理，Hook/signal 下一 Tick 可见。
- Spec §3.8.11：T4 的 body 覆盖、显式 tail 成功窗口、checked deadline、
  提前失败和立即终止；preparedGrace 不开启 gap/handoff 或连续接触恢复。
- Spec §3.8.12：K4 的 signed i64 priority / unsigned u64 tieRank 升序，
  同资源 occupying pair 唯一，claimKey/requirementId/hash 不补平局。
  局部 first-eligible 唯一性证明算法已接受；公共 proof codec 未冻结。
- Spec §3.10：capacity=1、唯一 slot、free/held/terminal，observe 不占资源，held 不被抢占。
- Spec §3.9 / §3.17：既定 Fact 总序、引擎派生 origin/commit/fact identity，
  timer identity 重建不重新编号。

## 3. L：late-policy 参数关系校验

**归属：明确阻塞 S7A-4 的遗留合同。** Spec §3.7.4 / §3.7.7 与
[BATCH_GATES](BATCH_GATES.md) 已登记：当前 validatePrepare 只检查参数已声明且已测量，
S7A-4 消费 late window 前还须补齐参数序关系、上界与一致性校验。

| 方案 | 内容 | 取舍 |
| --- | --- | --- |
| L1 | 按 reject_late / queue_next_tick 建立显式约束表，prepare 校验 | 易审查、诊断明确；需要完整边界表 |
| L2 | 有限窗口状态参考模型，验证声明参数不会产生不合法转移 | 边界和组合错误覆盖较强；设计成本较高 |
| L3 | 只接受登记并验证过的实测 profile | 准入简单；配置自由度及 profile 维护成本较差 |

**推荐 L1，配合 L2 作为独立验证依据。** L3 可作为后续发行准入层，不代替基本合同。
关系合同细化时必须明确：

1. 各参数的量纲与参照点；queue hop 若为次数，不能直接与 Tick 阈值比较。
2. open/close/finalization 的关系及边界等号；不得在本登记猜测阈值相对于哪一时间点。
3. queue_next_tick 的 eligible window 条件、重复排队判据和已终止 phase 的处理。
   不能复活已终止 Requirement、撤销 timer Fact 或重开已提交窗口。
4. 缺失、未测量、关系矛盾和算术溢出的失败路径；沿既有码表登记规则处理。
5. 关系验证与具体实测数值分开；默认值、窗口半宽及生产限额仍归 S7A-9 / 对应 profile 批次。

验收入口：两种策略、合法/矛盾参数、边界前一 Tick/相等/后一 Tick、重复排队、
目标 phase 已终止、checked arithmetic、失败保持旧 active 状态。
**本登记未给出完整关系公式，门禁保持待闭合。**

## 4. H：Candidate、lease 与最小 contact handle

**归属：首次消费时的表示细化。** ABI 域 2 / 域 4 已接受职责和生命周期，
ContactRef / ResourceLease / CandidateId 的具体生成表示仍须按消费范围闭合。

| 方案 | 内容 | 取舍 |
| --- | --- | --- |
| H1 | 引擎按规范事件与阶段顺序分配单调编号 | 紧凑；恢复和分批推进须保证分配一致 |
| H2 | 规范 origin、Requirement 实例、局部路径等组成结构化语义身份 | 易追溯、易重建；表示较大 |
| H3 | 从结构键派生摘要，并规定碰撞处理 | 存储紧凑；增加编码、版本及碰撞责任 |

**推荐 H2 作为语义基础，H1 只作内部索引；H3 暂缓。** 最终结构键字段、
命名、宽度和失效规则仍须形成范围明确的 typed 合同，不能直接用本表替代。

必须保留：同一逻辑 Candidate 重复生成身份相同；contact end 后旧 handle 失效，
槽位复用建立新身份；宿主不提供 lease/contact identity；身份不作 K4 排序 fallback。
细化时明确静态身份/生成规则和运行期实例值的分界，不把运行期分配值反写 prepared hash。
此方向不授权新的公共 identity/runtime 字节编码，也不启用 sameContact/handoff。

验收入口：源数组置换、同 Tick 多事件、重复生成、contact end 后旧 handle、
资源 free 后复用，以及规范输入前缀重执行的一致性。

## 5. F：Fact 排序注册表与 timer 定位

**归属：既定排序公式的具体派生表示。** 不重新选择总序。
originKindPriority 的 observation 0 / timer 1 / coordination 2 / correction 3 已在
Spec §5.2 接受，correction 在 7A 仍拒绝；timer 执行在前不意味着 timer Fact 排序在前。

| 方案 | 内容 | 取舍 |
| --- | --- | --- |
| F1 | 直接保留并比较既定逻辑元组，timer 使用 prepared logical identity | 透明、适合参考模型；记录与比较成本较高 |
| F2 | prepare 可确定的静态键按规范顺序映射为紧凑 ordinal，动态 origin 按既定规则派生 | 紧凑；须证明与逻辑比较等价 |
| F3 | 集中管理稳定 phase / fact-kind 语义 rank 与 FactSemanticRevision | 扩展治理明确；增加注册表维护 |

**推荐 F1 作为语义和参考依据，配合 F3；F2 等测量后评估。**
phase/fact-kind 表、localOrdinal、originScope/originOrdinal 和 timer logical path 的
完整派生关系仍需在首次消费前具体化；不能使用容器顺序、线程完成顺序或 ingressSequence。
改变生成或 rank 语义须改变 FactSemanticRevision；FactId/CommitId 公共编码仍归 S7A-6。

验收入口：同 Tick 混合 observation/timer Fact、多 phase、相同 commit 多 Fact、
实例/timer 枚举置换和重执行；按既定逻辑键核对身份、顺序及 commit 归属。

## 6. R：内部资源状态与提交组织

**归属：内部实现选择，不新增公共资源状态或 wire 编码。**

| 方案 | 内容 | 取舍 |
| --- | --- | --- |
| R1 | free/held/terminal 三态分支类型，先形成提交草案再提交 | 难以构造非法字段组合，便于原子性审查 |
| R2 | 统一记录加状态标签与可选 owner/lease，集中校验组合 | 查询简单；依赖完整不变量校验 |
| R3 | 状态、owner、lease 分离数组存储 | 可能改善局部性；一致性维护复杂且收益未测 |

**推荐 R1。** 先确定内部合法状态和草案接口，不由此冻结 public enum 数值或布局。
phase finalization 和 owned resource 终止必须属于同一 CoordinationCommit；
对外可见性遵守 Spec 的提交/Fact/Ruleset 边界，不提前发布草案。
同 Tick 后续 canonical event 可使用已 free 的 slot，terminal 永不再分配。

CM-C12 的 V2 重申需分别记录“有实例考虑事件”和“实际消费事件”，
明确 consideration 所在阶段，使资源冲突不能被误报为根本没有候选。
noCandidate 与 consumeEmpty 保留既定区别；具体 trace 字段和 consideration 判据仍需细化。

验收入口：held owner update、竞争失败、早释放、成功/失败即时终止、同 Tick 槽位复用、
observe 不改变 owner，以及两种 stray 政策差分。

## 7. S：scheduler 与生命周期重建证据

**归属：内部调度选择，不改变规范 Tick 顺序。**

| 方案 | 内容 | 取舍 |
| --- | --- | --- |
| S1 | 在必须处理的逻辑边界完整扫描活动实例、窗口和 timer | 适合独立参考模型；规模成本较高 |
| S2 | prepared timer 规范排序表加活动实例索引 | 性能与复杂度均衡，行为易核对 |
| S3 | 输入、timer、后续 signal 等统一事件堆 | 灵活；同 Tick 次序、取消与恢复较复杂 |

**推荐生产路径 S2，独立 S1 做差分对照。** 所有路径执行相同必要逻辑边界，
不得以渲染帧划分替代 Tick，也不得跳过 Hook/signal 首次可见的 Tick。

S7A-4 可先以内部规范输入前缀重执行建立生命周期重建证据；这不是公开 Seek、Snapshot
或 Replay 已交付。公共 codec 归 S7A-6，Playback/Player 集成归 S7A-7；不以空操作或
固定成功返回值绕过尚未实现的接口。验收核对活动实例、phase、timer、资源、Fact 和身份，
并比较同一规范输入流不同 advance 分段的结果。

## 8. 组合选择与后续边界

| 组合 | 选择 | 适用目标 |
| --- | --- | --- |
| A | L1 + L2 验证、H2、F1 + F3、R1、S1 | 优先参考正确性，随后优化 |
| B（推荐） | 与 A 相同合同方向；生产 S2、独立 S1 对照 | 兼顾确定性验证与实现复杂度 |
| C | L1 + L2、H2 加紧凑索引、F2 + F3、R3、S3 | 已有规模压力与测量依据时评估 |

进入实施前，L/H/F 及 CM-C12 首次消费所需字段必须在各自权威正文按限定范围闭合。
R/S 是内部设计方向，任何公共表示仍遵守 ABI 首次消费登记规则。
本登记不关闭 S7A-3 或 S7A-4，不扩大已有实施授权。

继续暂缓：公共 proof/runtime 编码、terminal serialization、Snapshot/Replay 与
FactId/CommitId codec、SolverProfile 默认列表、具体窗口半宽和 max* 限额、
gap/handoff/capacity>1/sameContact/连续轨迹。归属按 Spec §S7A-4 / ABI §未决项执行。

S7A-3 的既有实现与剩余证据见
[2026-10-05 核对报告](../../stage_reports/stages/stage-07/2026-10-05-s7a-3-remaining-evidence.md)。

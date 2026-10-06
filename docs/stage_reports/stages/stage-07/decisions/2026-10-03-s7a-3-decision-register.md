# S7A-3 后续合同讨论与决策登记

日期：2026-10-03  
收口更新：2026-10-04  
状态：设计裁决记录；G2 + R2 + P1 及其细化组合已按 owner 授权完成裁决；实现与验收尚未完成。  
范围：prepared grace 来源与消费、资源仲裁、Packed 表示及验收组织。

## 记录目的与授权边界

owner 已明确选择 **G2 + R2 + P1**，并要求先写入文档以防上下文压缩丢失。
已接受方向与原停止条件以
[合同组合裁定](2026-10-03-s7a-3-contract-selection-g2-r2-p1.md) 为准。

之后的讨论提出来源、消费、仲裁和编码等细化方案；最近一次推荐由
`G2-T1 + R2-K2` 调整为 **G2-T4 + R2-K4**。
本轮补充确认记录 **G2-S2 + P1-W1 + E1**；owner 已授权直接裁决 **G2-T4 + R2-K4**，并冻结 P1-W1 的 Gameplay Capsule v2 物理合同。

本文保存带日期的讨论、裁决及理由，不是规范正文或实现完成证据。2026-10-04 按 owner 的
自主裁决授权，把结果写入 Spec §3.8.9–§3.8.12 与独立 Capsule format；以下未采用的
备选仅为历史，不能覆盖现行规范。本轮仅文档，不进行 runtime 实现。
当前状态仍由 [CURRENT_STATUS.md](../../../../CURRENT_STATUS.md) 维护。

## 决策总表

| 事项 | 已接受方向 | 最终细化选择 | 状态 |
| --- | --- | --- | --- |
| Grace 解析 | G2：完整解析、精确量化、校验并冻结最终值 | G2-S2：显式值 / 单层命名引用 / 显式冻结默认值 | **G2-S2 已记录为本轮选择** |
| Grace 消费 | 不自动开启 gap、handoff 或连续恢复 | G2-T4：独立窗口与截止一致性验证 | **G2-T4 已裁决** |
| 资源准备 | R2：不可变 prepared resource plan，运行时分配实际 owner / lease / contact | R2-K4：业务优先级与显式稳定顺序 | **R2-K4 已裁决** |
| Packed 隔离 | P1：新 candidate revision 与新必需 semantic section 组 | P1-W1：显式字段与 typed 索引的行表 | **P1-W1 已裁决为 Gameplay Capsule v2：revision 2、GPH0/GPR0/GPD0/GRC0/REF0、REF0 kind 8--13 与字段布局已冻结** |
| 验收组织 | 不缩减 S7A-3 原有门禁 | E1：维持现有批次范围，提交完整正反证据 | **E1 已记录为本轮实施组织选择；不表示门禁关闭** |

已记录并裁决的完整组合为 **G2-S2 + G2-T4 + R2-K4 + P1-W1 + E1**。
其中 E1 仍表示完整验收范围，不表示门禁已关闭；实现、golden、跨工具链与负例证据尚未完成。
本文中的 T / K 编号只是本轮选项标签，不是 capability、registry token 或 wire 编号。

## Grace 来源备选

| 选项 | 方案 | 取舍 |
| --- | --- | --- |
| G2-S1 | 上游完成来源解析，resolver 只接受已解析候选并量化、校验 | 消费接口较小，但上游仍需闭合缺失、引用和来源追溯合同 |
| G2-S2 | 显式值、单层命名值引用或显式冻结默认值；命名值不继续继承 | **本轮已记录选择**；保留复用，避免为 7A 引入递归继承系统 |
| G2-S3 | 有限无环继承图 | 表达能力更强，但增加环检测、深度 / 工作量约束及诊断责任 |

来源枚举、解析算法 revision 与有效消费 policy 分开。S2 已选择，具体合法来源、冲突 / 单跳
限制、负值先验与一次 half-even 量化由 Spec §3.8.10 规定；显式引用缺失不得退回默认。
最终值 prepare 后只读，选项标签不是 wire enum。

## Grace 消费备选与推荐修正

### 原 T1 / T2 / T3

| 选项 | 原讨论方向 | 本轮发现的限制 |
| --- | --- | --- |
| G2-T1 | 显式 Hold tail 的晚释放宽限；不恢复接触 | 把成功窗口和 hard deadline 混在一起；零 grace 与既有 tail 窗口不能靠 `[end,D)` 自动解释 |
| G2-T2 | 不扩大成功窗口，只延后失败 finalization | 若已不可能成功却继续占用资源，会影响其他 Requirement；资源终止与 Fact 可见性需另行闭合 |
| G2-T3 | 有限、版本化的 per-phase policy | 能容纳多种行为，但尚不能替代每个 policy 的明确时间与资源语义 |

原推荐 T1 不再作为可直接实施的消费合同。原讨论中“无 tail 必须拒绝非零 grace”的建议也没有
成为裁定；必须先明确适用 phase，不能凭该建议给所有 Requirement 增加拒绝条件。

### G2-T4：独立窗口与截止一致性验证，已采用

分别显式表达并冻结 tail 成功窗口、body 覆盖要求、hard deadline 和资源终止规则。
沿 [CONTRACT_MATRIX](../../../../proposals/gameplay-v2-acceptance/CONTRACT_MATRIX.md) 的 CM-R09
方向，要求 tail 语义时 Hold 的截止为 `D = end + preparedGrace`；消费细节已收口到
Spec §3.8.11 的 prepare 一致性与 phase/资源终止表。

- Grace 决定截止，不自动扩大成功窗口，不延长 body，也不允许断开后恢复。
- prepare 验证窗口、覆盖要求、截止与终止规则能同时成立；冲突或算术不可表示稳定拒绝。
- 有效成功观测必须发生在截止之前；窗口端点规则需显式定义，不做隐式裁切。
- 资源按已确认的终止规则转换为 `free` / `terminal`，不因 grace 自动保留到 D，也不抢占尚未终止的 owner。
- 最终 grace / policy 沿 G2 归属进入判定身份；窗口和终止语义须对齐既有身份归属，不以来源信息替代它们。

单独的时间边界示例：`end=1000、grace=40、D=1040`，tail 成功窗口可以为 `[980,1020)`，
而不必扩大为 `[980,1040)`。这仅说明窗口与截止的区别，不是已经通过 body 覆盖验证的完整 Hold fixture。

T4 的价值是修改 grace 不会自动修改成功容差；若新截止与原窗口不一致则明确拒绝。
代价是要求作者提供一致配置，不能通过默认值或钳制补齐。

### G2-T5：显式窗口裁切策略

作者提供原始 tail 窗口，并显式选择按截止裁切的 close policy；prepare 计算最终有效窗口。
只有该策略被明确选择才允许裁切；裁切后没有合法观测 tick 则拒绝。
原始 / 有效窗口应可诊断，最终窗口与 policy 进入相应身份投影。

该方向更便于窗口模板复用，但 grace 会影响可成功范围；它不是 G2 禁止的数值钳制，也不能
被用于悄悄修复非法 grace。当前不推荐替代 T4；若选择，必须新增明确的窗口语义裁定。

### G2-T6：命名 Close Profile

通过有限、版本化的命名 profile 提供窗口、截止和资源终止规则；prepare 将其展开为 T4 或 T5
的显式合同，运行时不解释模板，不执行 callback 或脚本。

T6 是声明 / 复用方式，不是第三种独立时间语义，可后续叠加在 T4 上。
它增加引用、版本、迁移与展开验证成本，不作为完成 S7A-3 的默认前置，也没有指定后续阶段归属。

### 已闭合的时间边界

[GAMEPLAY_V2_SPEC](../../../../formats/GAMEPLAY_V2_SPEC.md) §3.9 的规范 Tick 顺序先处理
到期 timer，再处理同 Tick 输入。因此 `t=D` 的 release 不能挽救已截止的 Requirement。
`preparedGrace=0` 时 `D=end`，不得同时默认为“在 end 释放仍可成功”。

这些问题现由 Spec §3.8.11 明确：半开窗口、bodyEnd timer 先结算覆盖、早释放立即失败、
tail 合法 release 成功、最后可成功窗口关闭可提前终止、终止立即 free/terminal，Fact 不重复。
无 tail 使用 D=end，非零 preparedGrace 不产生虚构 tail。
若实现要求改变截止公式或 Tick 顺序，仍须修订设计；不得加一 tick、交换 timer/input、
隐式补 tail 或引入 resource gap grace。

## 资源仲裁备选与推荐修正

### 原 K1 / K2 / K3

| 选项 | 原讨论方向 | 本轮发现的限制 |
| --- | --- | --- |
| R2-K1 | 每个资源使用唯一显式整数 rank；重复拒绝 | 易验证，但业务优先级与平局顺序合成一个编号，分组维护不便 |
| R2-K2 | 显式 rank 后使用 canonical Requirement identity 打破平局 | 若 identity 为完整语义投影或其 hash，修改非仲裁字段也可能改变胜者 |
| R2-K3 | 显式有序排序字段列表及比较方向 | 可配置，但每个字段与组合都增加验证、身份、版本和 proof 责任 |

K2 的问题是完整语义身份不宜兼任业务竞争顺序，不是所有稳定声明 ID 都不能参与身份。
本轮不采用未经确认的 identity fallback。

### R2-K4：业务优先级与显式稳定顺序，已采用

使用专门声明的 `(priority, tieRank)` 作为竞争键，明确固定比较方向。
同资源 occupying 实例重复完整键拒绝，不以未被选中的 K6 互斥证明放宽。

- 先验证候选资格及资源状态，再对合法竞争者应用 prepared 顺序。
- 已有、未终止的 owner 不参加抢占；`observe` 不占 slot，也不要求为占用竞争制造 rank。
- `tieRank` 不是 hash、容器位置或完整 Requirement identity；业务顺序进入判定身份，其他字段不能暗中替代它。
- 有效竞争者不得依赖 RequirementId fallback 选出胜者；与既有 resource / claimKey / requirementId 顺序的关系须在消费点明确。
- prepare 输出确定的排序依据，运行时执行 `coordinator.policy.greedy_v1`；不增加运行期全局搜索。

类型已裁决为 signed i64 priority / unsigned u64 tieRank，固定升序、同资源唯一。
ClaimKey 是 `(explicitNamespace,E)` 的结构寻址 key，不作排序 fallback；完整规范见 Spec §3.8.12。

### R2-K5：显式优先关系图

作者声明“A 优先于 B”等关系，prepare 检查环并编译为确定顺序。
可能竞争但没有明确先后关系的候选拒绝，不按输入顺序或随意的拓扑排序补齐。
运行时仍执行 prepared 顺序，不搜索关系图。

优点是适合复杂局部关系；代价是关系维护、展开与验证工作量。当前不作为 7A 默认，也不因此
放开跨 Requirement relation 的既有限制；若需要引入新的关系表示，必须单独核对范围并裁定。

### R2-K6：仅拒绝可能竞争的平局

允许不同 Requirement 使用相同键，但 prepare 必须证明它们不可能参与同一次竞争。
发现歧义或无法证明时拒绝；不能只看时间区间不相交就宣称完成证明。

优点是减少无意义的编号；代价是有界竞争分析和 proof 合同。
它可以后续用于放宽 K4 的保守重复检查，当前不以它降低准入标准，不预先冻结任何预算数值。

### 必须保留的证明与展开问题

确定的总序只能说明配置顺序下的选择确定，不能冒充其他 objective 下的最优或唯一解证明。
仍须按 Spec §3.12 与 CM-C06 / CM-C07 验证声明的 objective / tieBreak 和唯一性要求。

CXT invocation 与有限展开实例使用显式 E→pair 表，或按明确 node 顺序/radix 的有限 affine
rank block；checked mixed-radix 后生成 pair，再查全局 collision。局部 proof 的明确 objective
为 `[first-eligible]`、tieBreak 为 `[priority.asc,tieRank.asc]`，rejectIfNonUnique=true；
它们是显式准入值，不是自动补全的默认列表。不设置公共 proof codec。
不得用源数组位置、哈希或临时 Requirement identity 自动补 rank；声明不足则 prepare 拒绝。
一个 Requirement 最多一个 claim、capacity=1、fanout=1 与无抢占 / handoff / gap 的既有边界保持不变。

## Packed 与验收组织备选

| 事项 | 选项 | 讨论结论 |
| --- | --- | --- |
| Packed | P1-W1：显式字段 / typed 索引行表 | **已采用并闭合 candidate 物理设计**；唯一完整字段见 [Capsule](../../../../formats/GAMEPLAY_CAPSULE_V2_FORMAT.md) |
| Packed | P1-W2：列式表 | 便于批量读取，但增加表间对应与缺项验证责任 |
| Packed | P1-W3：带 tag / length 的记录 | 扩展边界清楚，但增加必需项、重复 tag 和嵌套长度验证责任 |
| 验收 | E1：维持现有完整范围 | **本轮已记录选择**；真实装配、grace / resource / identity、首次 Packed Reader / Writer、inline / CXT 等价、file / memory 与跨工具链证据；Playback / Player / CXC 集成留 S7A-7 |
| 验收 | E2：拆分子批次 | 可以分批交付，但子批通过不代表 S7A-3 完成，Packed 等剩余门禁仍必须关闭 |
| 验收 | E3：提前扩到完整 authoring / CXC / Playback | 需要修改范围与依赖，不作为当前默认 |

P1 不改变 Foundation `REQ0` / `CNS0` 或 REF0 的既有 kind 意义；Gameplay 判定闭包与
presentation closure 的分离不授权删除原有表现资源记录。W1 的具体物理设计已于 2026-10-04 闭合。

## 2026-10-04 首次消费合同收口

本轮按 owner 自主裁决授权完成文档设计，不再等待 T/K 或七项物理值选择：

| 裁决 | 结果 / 唯一正文 |
| --- | --- |
| S2 与 identity | 来源、resolver revision、effective policy 分离；相同最终 grace/policy 同判定身份；Spec §3.8.10 |
| T4 | 覆盖 / 窗口 / 截止独立，timer-first、早终止与零 grace 有明确表；Spec §3.8.11 |
| K4 | 全实例表 / affine 展开、pair 类型与升序、冲突拒绝、无 fallback、有限局部唯一性证明；Spec §3.8.12 |
| Capsule | revision 2，四个 Gameplay sections 加 raw REF0，固定组与完整 typed atoms；Capsule §1–§8 |
| REF0 kind 13 细化 | 固定 typed 投影子域，补齐 §3.8.6 所需完整逻辑引用；仅子域 0 是 capability；不改变 Foundation 1–7 或新增 ABI 类型 |
| Hash | graphRevision 进入结构语义；来源不进 judgement；索引解引用、明确断开 claim 环；Capsule §9 |
| E1 | 全量正反、逐字段 diff、file/memory、跨工具链/golden/publication；Capsule §11 |

字段权威从旧 P1 草案移交到 [Capsule format](../../../../formats/GAMEPLAY_CAPSULE_V2_FORMAT.md)；
提案页只保存三种组织方案与选择理由。ABI 仍为 **9 域 / 126 = 96 + 30**，集中诊断码与九类不增。
设计检查与文档验证见 [收口报告](../implementation/2026-10-04-s7a-3-design-closure.md)。

## 下一步与停止条件

1. 本轮止于文档。后续获得实现授权后修正现有来源枚举进入 judgement identity 的投影，再补来源等价测试。
2. 实现 Capsule typed Reader/Writer、同一 prepare 接线、旧 Reader 早拒绝与全量 E1；不能将内部 identity bytes 当作 wire。
3. 实现若无法满足闭合字段或要求引入默认值、未登记公共码、隐藏 rank/slot 或扩大 7A 子集，先修订并记录合同，不能静默变义。
4. 实测门禁通过后才可登记 S7A-3 实现关闭；包含性接线已有记录仍须整批复验，状态预算缺口保持独立。

本轮修改了设计 Spec / ABI 文本，不实现 resolver / coordinator / Reader / Writer；
不关闭状态预算 `INCOMPLETE GATE` 或 S7A-3 实现门禁，不暂存、提交或授权合并。

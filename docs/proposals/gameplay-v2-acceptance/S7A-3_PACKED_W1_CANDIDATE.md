# S7A-3 P1-W1 Packed 设计选择记录

状态：candidate 的设计选择已裁决；仅记录选择历史，不拥有物理字段，不表示实现验收。

更新日期：2026-10-04

完整组合：**G2-S2 + G2-T4 + R2-K4 + P1-W1 + E1**。
owner 已授权自主设计裁决，并选择本轮只完成文档。

## 1. 权威与边界

- [Gameplay V2 Spec](../../formats/GAMEPLAY_V2_SPEC.md) §3.8.10–§3.8.12 拥有 S2/T4/K4 的消费语义。
- [Gameplay Capsule v2 format](../../formats/GAMEPLAY_CAPSULE_V2_FORMAT.md) 唯一拥有首次 Packed 消费的完整字段、编码、hash 与拒绝规则。
- [Foundation Packed](../../formats/PACKED_CHART_FORMAT.md) 继续拥有 revision 1；revision 2 仅按 Capsule 的显式差异扩展。
- [决策登记](../../stage_reports/stages/stage-07/2026-10-03-s7a-3-decision-register.md) 保存授权与备选；[设计收口报告](../../stage_reports/stages/stage-07/2026-10-04-s7a-3-design-closure.md) 保存本轮证据与未完成实现门禁。

此页不再提供第二套 row layout。旧草稿的重复表、zero-as-absent、observe/occupy 合并、
claim 行号分配 slot 和缺失 graph 字段已撤销，不能继续作为实现依据。
原草稿的备选价值保留于下表，不保留互相矛盾的字段作为另一份规范。

## 2. 三种组织方案

| 方案 | 结构 | 优点 | 代价 / 处置 |
| --- | --- | --- | --- |
| A：多 section | global、Requirement、resource 分层，加共享 REF0 | 语义边界清晰，可分别检查闭包与计数 | 跨表引用多；选用增强版，另设完整 definition dictionary |
| B：单 section | GPV0 内含 typed 行表 | 外层目录少，定位集中 | 内部 tag/version/framing 增加复杂度；未采用 |
| C：基础表加最小语义表 | JDG0/RPL0，与 Foundation 表并列 | 可复用基础 tooling | 旧工具忽略新表风险较高，legacy 与 V2 互斥规则复杂；未采用 |

上述 B/C 名称仅为历史备选，未注册为可接受 section。

## 3. 最终选择：增强 A

采用 **Gameplay Capsule v2**：显式 `candidateRevision=2`，必需
`GPH0/GPR0/GPD0/GRC0/REF0`，各组职责分别为 global、Requirement、definitions、resource plan、
共享 references。四个 Gameplay section 使用 schemaRevision=1 / rowCodec=1 / flags=0；
**REF0 沿 Foundation raw rows，没有 Gameplay 表头**。

采用 typed atoms、显式 presence、固定分组、完整 E 六元组与 phase ordinal。
定义可以结构去重，但 source AST、诊断路径和运行期 owner/lease/contact 不进入执行行表。
REF0 8–13 固定分配；kind 13 的固定子域补齐 timebase/late/normalization/solver/coordinator/
FactBinding/requirement 投影，只有 capability 子域参与能力 enabled 检查。

Foundation `REQ0/CNS0` 在 revision 2 为空且不承载 V2；Header requirementCount 和 ENT0 bit 2
明确由 GPR0 解释。revision 1 Reader 必须早拒绝 revision 2，不能忽略新表后继续成功。
空图仍有一行 GPH0，不把“无 Requirement”误作“缺 Gameplay 合同”。

## 4. 配套语义

S2 来源解析只有一次精确 half-even 量化；来源与有效执行 policy 分离。
T4 独立 body 覆盖、成功窗口和 deadline，冲突拒绝而非裁切；零 grace 与 exact-end 沿 timer-first。
K4 采用显式 `(priority:i64,tieRank:u64)` 升序，同资源唯一，不用 claimKey/identity/hash fallback；
实例表或有限 affine rank block 在 prepare 展开为显式 pair。
observe 不占 slot，consume/claim intent 保真；capacity=1 的所有候选共用 slot 0。

结构 semantic hash 与 wire dump、internal identity bytes、完整 prepared identity 分开；
graphRevision 是语义，grace 来源是 provenance。Reader 从完整 typed graph 重做 closure、
包含性、T4/K4 与局部唯一性证明，全部通过后原子发布。

## 5. 实现准入与退出

E1 仍要求完整 typed round-trip、正反边界、inline/CXT 逐字段 diff、file/memory、
canonical byte/hash golden、跨工具链与失败保持旧 publication 的证据。
具体 fixture 只有 [Capsule §11](../../formats/GAMEPLAY_CAPSULE_V2_FORMAT.md) 一份权威清单。

本轮只闭合设计；Reader/Writer、来源无关 identity 修正、prepare 接线与 E1 实测仍待实现。
状态预算 **INCOMPLETE GATE** 不关闭，不新增 S7A-9 数值上限；
Snapshot/Replay/FactId/CommitId codec 与 runtime resource state encoding 不在本次冻结。
Playback/Player/CXC 集成仍属 S7A-7。

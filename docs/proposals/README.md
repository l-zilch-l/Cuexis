# Proposal Index

状态：现行提案索引

更新日期：2026-10-06

提案文档描述候选合同、已接受但未实施的方向或延期研究输入。它们不等同于生产 API；
生产与候选边界以 [格式索引](../formats/README.md) 和 [当前状态](../CURRENT_STATUS.md) 为准。

Gameplay I 工作稿是**已被取代**的历史研究基线：取舍见 [ADR 0043](../adr/0043-gameplay-judgement-ruleset-convergence.md)
（`superseded`），字段和运行语义见 [Gameplay Judgement Spec](../formats/GAMEPLAY_JUDGEMENT_SPEC.md)
（`superseded`），typed preview 边界见 [Gameplay Judgement ABI](../api/GAMEPLAY_JUDGEMENT_ABI.md)
（`superseded`）。[Gameplay V2 redesign research](research/gameplay-v2/README.md) 已成为 Chart v5 / Stage 7A 的
现行方向：决策见 [ADR 0044](../adr/0044-gameplay-v2-semantic-kernel.md)、字段与运行语义见
[Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md)、typed 边界见 [Gameplay V2 ABI](../api/GAMEPLAY_V2_ABI.md)；
三份 V2 文档的字段与冻结状态按各自声明维护；分批实现进度只由 [CURRENT_STATUS](../CURRENT_STATUS.md)
汇总，不从候选文档状态推断实现完成。

## 目录分区

本目录按生命周期分为三组。Gameplay 研究稿、分阶段实施输入和延期输入分别进入独立子目录；
历史路径统一记录在本目录的迁移说明中，不再把不同生命周期的正文平铺在根目录。

| 分区 | 文件 | 角色 |
| --- | --- | --- |
| Stage 6 implementation input | `implementation-input/stage-06/STAGE6_API_AND_INSTALL_DRAFT.md` | 仍被工具、阶段报告和实施门禁引用的候选输入 |
| Stage 7 implementation input | `implementation-input/stage-07/s7a-5-6-design-selection.md` | 联合5/6的方案比较；决策台账归主计划，消费字段归Spec/ABI/profile |
| Gameplay research inputs | `research/gameplay/GAMEPLAY_*.md` | I 收敛前的推导、缺陷、压测和设计历史 |
| Gameplay v2 redesign | `research/gameplay-v2/` | Chart v5/Stage 7A 的候选重构、压测和交接合同 |
| Gameplay V2 acceptance package | `gameplay-v2-acceptance/` | 由 S7A-0 产出的准入包：合同台账、支持/拒绝矩阵、预算计划、批次门禁、交接台账与 owner 决策请求 |
| Deferred inputs | `deferred/` | 尚未进入当前实施阶段的专题设计 |

Gameplay 文件不是生产 API、已接受 Spec 或当前实现声明；旧研究文件以本索引顶部列出的 I 工作稿为准，
Gameplay v2 目录则记录新的候选路线及其尚未闭合的实施合同。

- [Gameplay research index](research/README.md)
- [Gameplay V2 redesign research](research/gameplay-v2/README.md)
- [Gameplay V2 acceptance package](gameplay-v2-acceptance/README.md)：S7A-0 产出的准入包（合同台账、
  支持/拒绝矩阵、Chart v5/CXC entry 与 identity 矩阵、target/依赖图、预算与证据计划、批次门禁、
  四项 Stage 6 交接台账、未决问题与 owner 决策请求）。它仍是 candidate：owner 接受前不修改
  ADR/Spec/ABI 的权威状态，也不构成实施授权。
- [Implementation-input index](implementation-input/README.md)

## Stage 7 实施输入

- [S7A-5/6方案比较和选优](implementation-input/stage-07/s7a-5-6-design-selection.md)：45套方向方案、残余细节备选及推荐理由；当前目标和准入见 [Stage 7主计划§3.2](../stage_plans/active/stage-07/plan.md#32-s7a-5--s7a-6-联合实施目标决策和准入)。

## 当前候选格式提案

- [ADR 0038](../adr/0038-cxc-v1-and-chart-v4-boundary.md)：CXC v1、Chart v4 和 CXT v1 边界。
- [Chart v4 candidate](../formats/CHART_V4_FORMAT.md)：Chart v4 字段和 lowering。
- [CXC v1 candidate](../formats/CXC_FORMAT.md)：容器、manifest 和闭包。
- [CXT v1 candidate](../formats/CXT_FORMAT.md)：声明式模板 JSON 文件。

## Stage 6 实施输入

- [Stage 6 candidate API and install draft](implementation-input/stage-06/STAGE6_API_AND_INSTALL_DRAFT.md)：显式 candidate entry、experimental 安装和 Reference Host 的候选实施合同；仍由 Stage 6 工具和报告引用。

## Gameplay 研究输入

- [Gameplay research index](research/gameplay/README.md)：Input / Judgement 研究推导、折叠演算、身份、
  Hook、预算与压测记录。
- [Gameplay Ruleset 设计讨论记录](research/gameplay/GAMEPLAY_RULESET_DISCUSSION.md)：Input / Judgement / 脚本系统
 重新设计的共识与待讨论项，保留为讨论历史。
- [Gameplay Program IR 原语草案](research/gameplay/GAMEPLAY_PROGRAM_IR_DRAFT.md)：判定程序执行模型、原语、仲裁、
  静态验证与案例压测；部分语义已被 Fold Calculus 和 I 收敛 Spec 取代。
- [Ruleset Fold Language 草案](research/gameplay/GAMEPLAY_RULESET_FOLD_DRAFT.md)：L3 Ruleset 折叠语言、打包、
  静态验证与打包研究；字段权威已转移到 I 收敛 Spec。
- [Gameplay Identity 分层草案](research/gameplay/GAMEPLAY_IDENTITY_DRAFT.md)：字段分区、两个摘要、JudgementIdentity
  组成与失配策略；身份裁决已由 ADR 0043 和 Spec 承接。
- [Skill Hooks 与模块扩展性草案](research/gameplay/GAMEPLAY_SKILL_HOOKS_DRAFT.md)：程序可见 Hook 与模块本地状态
  的区分、触发词汇表、Interface 投影与禁止清单；保留为扩展性研究。
- [Gameplay 设计压测与缺陷登记](research/gameplay/GAMEPLAY_STRESS_TEST_DRAFT.md)：34 项案例压测、12 条设计缺陷、
  7 条局限与收敛趋势观察；保留为证据登记。
- [Bounded Fold Calculus 提案](research/gameplay/GAMEPLAY_FOLD_CALCULUS_DRAFT.md)：把核心概念从约 15 个归约到 3
  个（Fold / Pattern / Measure）的替换性重设计，消除 10 条缺陷中的 6 条（D1–D10 计数），
  已被 I 收敛工作稿吸收，未独立成为生产合同。
- [Fold Calculus 封闭性验证](research/gameplay/GAMEPLAY_CALCULUS_CLOSURE_CHECK.md)：34 项案例只许用算子组合的
  重写结果、V1–V7 缺口和后续修补证据；保留为验证历史。
- [Gameplay 预算与规模上界草案](research/gameplay/GAMEPLAY_BUDGET_AND_SCALE_DRAFT.md)：容量 / 稳态 / Seek 三类
  成本、活动度与每事件代价、补的规模裁决、快照间隔可行区间与 identity 分区清单；
  数值仍待真实内容测量，不构成生产 ABI 限额。
- [引擎冻结合同草案](research/gameplay/GAMEPLAY_ENGINE_FROZEN_CONTRACTS_DRAFT.md)：定点表登记点、缓动曲线的准入
  判据与起始集合、判定语义版本清单与门禁三来源原则；保留为引擎实现输入。

## 延期设计输入

- [Deferred proposals](deferred/README.md)：粒子和 Android/移动端方向。Shader 字段合同已由
  [MATERIAL_SHADER.md](../formats/MATERIAL_SHADER.md) 取代。

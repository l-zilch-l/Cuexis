# Proposal Index

状态：现行提案索引

更新日期：2026-08-28

提案文档描述候选合同、已接受但未实施的方向或延期研究输入。它们不等同于生产 API；
生产与候选边界以 [格式索引](../formats/README.md) 和 [当前状态](../CURRENT_STATUS.md) 为准。

Gameplay I 收敛已经建立工作稿权威：取舍见 [ADR 0043](../adr/0043-gameplay-judgement-ruleset-convergence.md)，
字段和运行语义见 [Gameplay Judgement Spec](../formats/GAMEPLAY_JUDGEMENT_SPEC.md)，typed preview
边界见 [Gameplay Judgement ABI](../api/GAMEPLAY_JUDGEMENT_ABI.md)。下列 Gameplay 提案继续保留
为研究推导、案例和压测证据；发生冲突时，以三份收敛工作稿为准，直到 owner acceptance。

## 当前候选格式提案

- [ADR 0038](../adr/0038-cxc-v1-and-chart-v4-boundary.md)：CXC v1、Chart v4 和 CXT v1 边界。
- [Chart v4 candidate](../formats/CHART_V4_FORMAT.md)：Chart v4 字段和 lowering。
- [CXC v1 candidate](../formats/CXC_FORMAT.md)：容器、manifest 和闭包。
- [CXT v1 candidate](../formats/CXT_FORMAT.md)：声明式模板 JSON 文件。
- [Stage 6 candidate API and install draft](STAGE6_API_AND_INSTALL_DRAFT.md)：显式 candidate entry、experimental 安装和 Reference Host 的未实现草案。

## 设计讨论记录

- [Gameplay Ruleset 设计讨论记录](GAMEPLAY_RULESET_DISCUSSION.md)：Input / Judgement / 脚本系统
  重新设计的共识与待讨论项，未接受、未实施。
- [Gameplay Program IR 原语草案](GAMEPLAY_PROGRAM_IR_DRAFT.md)：判定程序执行模型、原语、仲裁、
  静态验证与七个案例压测，未接受、未实施。
- [Ruleset Fold Language 草案](GAMEPLAY_RULESET_FOLD_DRAFT.md)：L3 Ruleset 折叠语言、打包、
  静态验证与待决项，未接受、未实施。
- [Gameplay Identity 分层草案](GAMEPLAY_IDENTITY_DRAFT.md)：字段分区、两个摘要、JudgementIdentity
  组成与失配策略，未接受、未实施。
- [Skill Hooks 与模块扩展性草案](GAMEPLAY_SKILL_HOOKS_DRAFT.md)：程序可见 Hook 与模块本地状态
  的区分、触发词汇表、Interface 投影与禁止清单，未接受、未实施。
- [Gameplay 设计压测与缺陷登记](GAMEPLAY_STRESS_TEST_DRAFT.md)：34 项案例压测、12 条设计缺陷、
  7 条局限与收敛趋势观察，未接受、未实施。
- [Bounded Fold Calculus 提案](GAMEPLAY_FOLD_CALCULUS_DRAFT.md)：把核心概念从约 15 个归约到 3
  个（Fold / Pattern / Measure）的替换性重设计，消除 10 条缺陷中的 6 条（D1–D10 计数），
  未接受、未实施。
- [Fold Calculus 封闭性验证](GAMEPLAY_CALCULUS_CLOSURE_CHECK.md)：34 项案例只许用算子组合的
  重写结果，确认无新核心概念，但发现 7 处声明缺失（V1–V7），未接受、未实施。
- [Gameplay 预算与规模上界草案](GAMEPLAY_BUDGET_AND_SCALE_DRAFT.md)：容量 / 稳态 / Seek 三类
  成本、活动度与每事件代价、补的规模裁决、快照间隔可行区间与 identity 分区清单，
  未接受、未实施。
- [引擎冻结合同草案](GAMEPLAY_ENGINE_FROZEN_CONTRACTS_DRAFT.md)：定点表登记点、缓动曲线的准入
  判据与起始集合、判定语义版本清单与门禁三来源原则，未接受、未实施。

## 延期设计输入

- [Deferred proposals](deferred/README.md)：粒子和 Android/移动端方向。Shader 字段合同已由
  [MATERIAL_SHADER.md](../formats/MATERIAL_SHADER.md) 取代。

# Stage 7 Evidence Index

状态：current index

更新日期：2026-10-09

本索引按报告用途分类，不复制裁定、字段合同或当前阶段结论。
[CURRENT_STATUS](../../../CURRENT_STATUS.md) 是唯一当前实现状态摘要；
[Stage 7总计划](../../../stage_plans/active/stage-07/plan.md)、[7A分册](../../../stage_plans/active/stage-07/plan-a.md) 和 [7B+分册](../../../stage_plans/active/stage-07/plan-b.md) 持有范围、批次与实施决策。
每份报告只代表其证据日期，历史“下一步”不能覆盖主计划。

## 分类导航

| 分类 | 数量 | 主要用途 |
| --- | --- | --- |
| 准入与基线 (readiness) | 6 | owner准入记录、工具链基线、typed合同与计划审视、D-9候选证据 |
| 裁定记录 (decisions) | 13 | 分轮准入/实施裁定、合同选择、落地位置与诊断分类；现行字段仍归Spec/ABI |
| 设计与实施记录 (implementation) | 6 | 带日期的设计收口和实施推进；设计交付与运行验证分别按报告声明解释 |
| 验证与审计 (verification) | 18 | 独立复验、功能验收、边界审计、CI优化和文档恢复证据 |
| 交接与规划修订 (handoffs) | 3 | hosted交接、排期修订和桌面接手整理；历史下一步不覆盖主计划 |

## 常用入口

- [3/4受限功能验收](verification/2026-10-05-s7a-3-4-functional-acceptance.md)
- [同源hosted与证据归档](handoffs/2026-10-05-s7a-3-4-hosted-and-handoff.md)
- [5/6规划与桌面交接记录](handoffs/2026-10-06-s7a-5-6-plan-and-desktop-handoff.md)
- [当前Stage 7A计划和决策](../../../stage_plans/active/stage-07/plan-a.md)
- [S7A-7/8计划与合同重审](readiness/2026-10-07-s7a-7-8-plan-review.md)

## 准入与基线

| 报告 | 证据日期 |
| --- | --- |
| [S7A-7/8计划与合同重审](readiness/2026-10-07-s7a-7-8-plan-review.md) | 2026-10-07 |
| [D-9 候选补丁与验证证据：version-gate 的 POSIX shell 判据](readiness/2026-10-02-d9-shell-guard-candidate.md) | 2026-10-02 |
| [Gameplay V2 acceptance package：owner 接受记录](readiness/2026-10-02-s7a-0-acceptance.md) | 2026-10-02 |
| [S7A-0 执行基线与合同表征](readiness/2026-10-02-s7a-0-baseline.md) | 2026-10-02 |
| [Stage 7A S7A-0：Release 基线证据](readiness/2026-10-02-s7a-0-release-baseline.md) | 2026-10-02 |
| [Stage 7A typed contract review（S7A-1 冻结顺序第 4 步）](readiness/2026-10-02-s7a-1-typed-contract-review.md) | 2026-10-02 |

## 裁定记录

| 报告 | 证据日期 |
| --- | --- |
| [S7A-2 准入门禁：第 2 轮（时间域与迟到策略）裁定落地记录](decisions/2026-10-03-s7a-2-gate-rulings.md) | 2026-10-03 |
| [S7A-2 时基实现裁定落地记录（第 2 轮实现复核修订）](decisions/2026-10-03-s7a-2-implementation-rulings.md) | 2026-10-03 |
| [S7A-2 输入规范化半批实现裁定落地记录（第 2 轮输入半批复核）](decisions/2026-10-03-s7a-2-input-implementation-rulings.md) | 2026-10-03 |
| [S7A-3 未闭合合同组合裁定：G2 + R2 + P1](decisions/2026-10-03-s7a-3-contract-selection-g2-r2-p1.md) | 2026-10-03 |
| [S7A-3 后续合同讨论与决策登记](decisions/2026-10-03-s7a-3-decision-register.md) | 2026-10-03 |
| [S7A-3 准入门禁：第 3 轮（prepare、装配与 entry）裁定落地记录](decisions/2026-10-03-s7a-3-gate-rulings.md) | 2026-10-03 |
| [S7A-3 实施裁定记录（第二半 part 1 第一轮实现复核修订 + 第 2、3 轮复审改正）](decisions/2026-10-03-s7a-3-implementation-rulings.md) | 2026-10-03 |
| [S7A-3 落地位置裁定（卡 4 / 卡 5）](decisions/2026-10-03-s7a-3-landing-location-rulings.md) | 2026-10-03 |
| [S7A-4 准入门禁：第 4 轮（仲裁、资源与事实序）裁定落地记录](decisions/2026-10-03-s7a-4-gate-rulings.md) | 2026-10-03 |
| [S7A-5 / S7A-6 准入门禁：第 5 轮（Ruleset、事实序、Score 与 Snapshot）裁定落地记录](decisions/2026-10-03-s7a-5-6-gate-rulings.md) | 2026-10-03 |
| [S7A-6 / S7A-7 准入门禁：第 6 轮（发布粒度、表现桥接与诊断）裁定落地记录](decisions/2026-10-03-s7a-6-gate-rulings.md) | 2026-10-03 |
| [第 7 轮（收尾澄清与缺陷）裁定与收尾落地记录](decisions/2026-10-03-s7a-7-wrapup-rulings.md) | 2026-10-03 |
| [第 6 轮 `CM-D04` / `P1-09` 首次消费的诊断分类学后续补齐记录](decisions/2026-10-03-s7a-diagnostics-taxonomy-rulings.md) | 2026-10-03 |

## 设计与实施记录

| 报告 | 证据日期 |
| --- | --- |
| [S7A-7/8 Graph、Entry 与生产 assembler 证据](implementation/2026-10-08-s7a-7-8-entry-assembler-progress.md) | 2026-10-08 |
| [S7A-7/8 集成推进记录](implementation/2026-10-07-s7a-7-8-integration-progress.md) | 2026-10-07 |
| [S7A-3 实现推进记录：G2-S2 / G2-T4 / R2-K4 / P1-W1 裁决后状态](implementation/2026-10-03-s7a-3-implementation-progress.md) | 2026-10-03 |
| [S7A-3 设计收口与实施交接报告](implementation/2026-10-04-s7a-3-design-closure.md) | 2026-10-04 |
| [S7A-3 / S7A-4 complete execution design handoff](implementation/2026-10-05-s7a-3-4-execution-design.md) | 2026-10-05 |
| [S7A-3/4 implementation progress and pause point](implementation/2026-10-05-s7a-3-4-implementation-progress.md) | 2026-10-05 |

## 验证与审计

| 报告 | 证据日期 |
| --- | --- |
| [Version Gate bootstrap 验证与保护阻断](verification/2026-10-09-version-gate-bootstrap.md) | 2026-10-09 |
| [S7A-7/8 CI 严格编译修复](verification/2026-10-09-s7a-7-8-ci-repair.md) | 2026-10-09 |
| [Stage 6 四项交接实施与验收](verification/2026-10-07-stage6-handover-implementation.md) | 2026-10-07 |
| [`docs/api/GAMEPLAY_V2_ABI.md` 事故性截断与重建的证据报告](verification/2026-10-03-abi-document-restoration.md) | 2026-10-03 |
| [S7A-3 第二半 part 1：主控独立复验的带日期证据](verification/2026-10-03-s7a-3-independent-verification.md) | 2026-10-03 |
| [Stage 7 CI runtime optimization](verification/2026-10-05-ci-runtime-optimization.md) | 2026-10-05 |
| [S7A-3 / S7A-4 execution design ambiguity audit](verification/2026-10-05-s7a-3-4-ambiguity-audit.md) | 2026-10-05 |
| [S7A-3 E1 and S7A-4 execution acceptance audit](verification/2026-10-05-s7a-3-4-e1-audit.md) | 2026-10-05 |
| [S7A-3/4 limited functional acceptance](verification/2026-10-05-s7a-3-4-functional-acceptance.md) | 2026-10-05 |
| [S7A-3 剩余实施与验收证据核对](verification/2026-10-05-s7a-3-remaining-evidence.md) | 2026-10-05 |
| [S7A-5/6 实施阻塞复核](verification/2026-10-06-s7a-5-6-implementation-blocker-audit.md) | 2026-10-06 |
| [S7A-5/6 U01–U13全量重审](verification/2026-10-06-s7a-5-6-u01-u13-rereview.md) | 2026-10-06 |
| [S7A-5/6 limited functional acceptance](verification/2026-10-06-s7a-5-6-functional-acceptance.md) | 2026-10-06 |
| [Stage 6 四项交接现状复核与解决方案](verification/2026-10-06-stage6-handover-audit.md) | 2026-10-06 |

## 交接与规划修订

- [Player 可读四轨设备练习](verification/2026-10-09-player-readable-practice.md)（2026-10-09）：
  下落音符、判定线、Hold与实际结果；受限定向SDL/GPU验证通过，实体设备/校准待回填。
- [Player 多转换 Tick 桥修复](verification/2026-10-09-player-multi-transition-tick-repair.md)（2026-10-09）：
  保留内核一个 Tick 至多一个输入，显式逐转换工作 Tick；CPU/完整Replay通过，设备重测待回填。
- [真实设备复测：同 Tick 碰撞](verification/2026-10-09-device-retest-same-tick-collision.md)（2026-10-09）：
  修复后两次真实输入均取得一次 Hit 与完整 Replay=same，随后 same_tick_collision 退出1；整体未通过。
- [Player 真实离散按键失败与修复](verification/2026-10-09-player-discrete-device-repair.md)（2026-10-09）：
  实际设备日志、Controller 首次输入反例、退出码与候选 build 修正。

- [CRLF 修复与真实设备验收谱面](verification/2026-10-09-crlf-and-device-fixture.md)（2026-10-09）：
  审批换行回归、可重复自生成素材、生产装配与设备操作材料。

| 报告 | 证据日期 |
| --- | --- |
| [S7A-3/4 Hosted Evidence and S7A-5 Handoff](handoffs/2026-10-05-s7a-3-4-hosted-and-handoff.md) | 2026-10-05 |
| [S7A-5 / S7A-6 Plan Consolidation and Desktop Handoff](handoffs/2026-10-06-s7a-5-6-plan-and-desktop-handoff.md) | 2026-10-06 |
| [S7A-5 / S7A-6 Planning Selection and Handoff Revision](handoffs/2026-10-06-s7a-5-6-planning-selection.md) | 2026-10-06 |

## 路径迁移

历史报告按用途迁入上述叶目录，历史正文和证据日期保留，链接已重定位；新增报告在对应分类登记。
旧逻辑路径见 [legacy-paths](legacy-paths.md)，不建立逐文件stub。
叶目录由本索引直接列出全部正文，不再为每个分类复制README。

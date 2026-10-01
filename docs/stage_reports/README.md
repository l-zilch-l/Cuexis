# Stage Report Index

状态：current index

更新日期：2026-09-28

报告保存带日期的实施、审查和验证证据。报告中的“下一步”只代表其快照日期，不能重新定义
[CURRENT_STATUS.md](../CURRENT_STATUS.md)。

`reviews/<topic>/` 保存跨阶段专题复核。一篇复核可以包含多份正文（例如按审查轴拆分的
Standards / Spec 记录），由该专题自己的 `README.md` 承担导航；轴与轴之间保持独立，
不合并成一张表，也不跨轴重排。

## 按阶段归档

- Stage 0：[completion](stages/stage-00/completion.md)
- [Stage 1](stages/stage-01/README.md)
- Stage 2：[completion](stages/stage-02/completion.md)、[review](stages/stage-02/2026-08-06-review.md)
- Stage 3：[completion](stages/stage-03/completion.md)、[review](stages/stage-03/2026-08-08-review.md)
- [Stage Chart Format Update](chart-format-update/README.md)
- [Stage 4](stages/stage-04/README.md)
- [Stage 5](stages/stage-05/README.md)
- [Chart Format Foundation](stages/chart-format-foundation/README.md)
- [Stage 6](stages/stage-06/README.md)：[关闭报告](stages/stage-06/completion.md)（2026-09-27 关闭并归档）

## 跨阶段专题

- [CFU and Stage 4 review](reviews/cfu-stage-04/2026-08-28-review.md)
- Documentation reorganization：[initial report](reviews/documentation-reorganization-2026-08/2026-08-30-reorganization.md)、
  [root consolidation](reviews/documentation-reorganization-2026-08/2026-08-30-root-consolidation.md)、
  [README consolidation](reviews/documentation-reorganization-2026-08/2026-08-30-readme-consolidation.md)
- 260830-followup：[最终关闭报告](reviews/260830-followup/2026-09-01-final.md)、[任务 2 Chart/CXC parse-once 完成报告](reviews/260830-followup/2026-08-31-task-2-chart-cxc-parse-once.md)、
  [任务 3 hosted 完成报告](reviews/260830-followup/2026-08-31-task-3-hosted-verification.md) 和
  [任务 3 本地快照](reviews/260830-followup/2026-08-31-task-3-critical-branch-coverage.md)
- [Full Review 2026-08](reviews/full-review-2026-08/README.md)
- [Full Review final closure](reviews/full-review-2026-08/2026-08-30-final.md)
- [SDK transition verification](sdk-transition/verification.md)
- [Stage Verification 2026-09 核验记录](reviews/stage-verification-2026-09/2026-09-01-findings.md)（active）
- Stage 6 双轴复核（`13dab93..eaaf375`，事后复核）：[汇总](reviews/stage-06-review-2026-09/2026-09-28-summary.md)、
  [Standards 轴](reviews/stage-06-review-2026-09/2026-09-28-standards.md)、
  [Spec 轴](reviews/stage-06-review-2026-09/2026-09-28-spec.md)、
  [切片附录](reviews/stage-06-review-2026-09/2026-09-28-slice-appendix.md)、
  [二轮细化](reviews/stage-06-review-2026-09/2026-09-28-refinement.md)（active）
- Stage 6 复核修正（批次报告）：[R1 文档与证据链修正](reviews/stage-06-review-2026-09/2026-09-28-r1-document-and-evidence.md)、
  [W1 诊断码与版本门禁](reviews/stage-06-review-2026-09/2026-09-28-w1-diagnostics-and-version-gate.md)、
  [W2 媒体导入修正](reviews/stage-06-review-2026-09/2026-09-28-w2-media-import.md)、
  [W3 发布事务修正](reviews/stage-06-review-2026-09/2026-09-28-w3-publication-transaction.md)、
  [W4 渲染收敛](reviews/stage-06-review-2026-09/2026-09-28-w4-render-convergence.md)、
  [W5 宿主与分发门禁](reviews/stage-06-review-2026-09/2026-09-28-w5-host-and-distribution-gates.md)、
  [W6 代码健康度](reviews/stage-06-review-2026-09/2026-09-28-w6-code-health.md)、
  [交付报告](reviews/stage-06-review-2026-09/2026-09-28-delivery-report.md)
- Gameplay Ruleset 设计（candidate 提案的研究性实测，非实施）：
  [Fold Spike 预算实测与确定性检查](reviews/gameplay-ruleset-2026-10/2026-10-01-fold-spike.md)（2026-10-01）、
  [I 收敛前置审查](reviews/gameplay-ruleset-2026-10/2026-10-01-i-entry-readiness.md)（2026-10-01）
- Stage 6 复核修正批次 R9（另行开启；最后行为 SHA `71de8b1`，owner 接受待定）：
  [R9 参考宿主命令循环与 play/pause](stages/stage-06/2026-09-28-r9-reference-host-command-loop.md)

## 历史路径

- [Legacy stage-report paths](legacy-paths.md)

历史报告不可被改写为新的验证结果。新的关闭或复核必须创建新的报告，并从当前状态页或相应索引链接。

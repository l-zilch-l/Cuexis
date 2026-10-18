# Stage 7 发布门禁的历史阻塞与关闭依据

状态：historical

证据日期：2026-10-09 至 2026-10-10；整理日期：2026-10-17

本页合并已被最终关闭报告覆盖的四份 bootstrap、来源保护和交付停点记录。
最终接受见[S7A-8.4 关闭](../s7a/verification/2026-10-10-s7a-8-4-closure.md)，
现行流程归[VERSIONING](../../../../guides/VERSIONING.md)，不从历史失败恢复旧任务清单。

## 历史问题

1. 可信 base 尚未消费版本检查器时，candidate 的普通 CI 成功不能替代 trusted bootstrap。
2. 审批必须绑定实际 repo/PR/base/head/tree/from/to/UTC；审批换行、版本差异及真实执行日志曾需单独修复核验。
3. required context 与 GitHub App 绑定不等于可信 workflow 来源已经强制固定。
   个人单人仓库的平台能力限制后来通过[有范围的 owner 例外](../s7a/decisions/2026-10-10-single-maintainer-workflow-source-exception.md)处理，
   不是把原来源门禁默认为通过。
4. 实体设备与生产预算接受归 RPA，不因 Version Gate 或 7A 的受限功能验收通过而关闭。

## 后续关闭与原始材料

10 月 10 日 owner 明确关闭 S7A-8.4。最终报告绑定 `1ebcbdc92d92fa9d45e4e1f835e5a1f338e7fc2d`，
同 SHA 七个 hosted run 成功，审批评论 6084640280、可信 base 3076948 及来源例外逐项核对。
这不证明报告所在后续提交已自动通过，也不表示整个 Stage 7A/RPA 或发布已完成。

仍具独立取证价值的历史原始包保留，不改内部路径、日志或散列：

- [bootstrap 与保护阻塞原始证据](2026-10-09-version-gate-bootstrap-evidence.zip)
- [审批失败及 RPA 归属核对原始证据](2026-10-09-version-gate-and-rpa-routing-evidence.zip)
- [来源保护及交付原始证据](2026-10-09-s7a-7-8-protection-and-delivery-evidence.zip)

删除的是已经过时的待办/停点正文，不将历史失败改记为成功，不删除当前仍依赖的接受或唯一决策记录。

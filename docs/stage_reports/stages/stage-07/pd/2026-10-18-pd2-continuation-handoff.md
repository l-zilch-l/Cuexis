# PD-2 Continuation Handoff

日期：2026-10-18。状态：PD-1 已提交；PD-2 尚未开始。

## 接手基线

- 仓库：`D:/Cuexis-worktree`，分支：`stage-7`。
- HEAD：`66fc310376ce0f014c8dd0601e038bd0a9a50635`，提交 `refactor(pd): add owner-bound Judgement publication bridge`。
- PD-1 回退对照点：`324d874f56eb466f1c8533550e80794bf106a02b`。没有 push、PR、合并或发行。
- 当前状态摘要：[CURRENT_STATUS](../../../../CURRENT_STATUS.md)。PD-1 实施与验证证据：[PD-1 报告](2026-10-18-pd1-publication-bridge.md)。

接手时工作区仅有既存 Python `__pycache__` 未跟踪文件；继续前重新检查 Git 状态，不清理或提交这些缓存。

## 下一批：PD-2

用户已授权继续实施 PD。完成只读状态核对后，从 PD-2 开始。权威范围见 [PD 计划](../../../../stage_plans/active/stage-07/plan-presentation-decoupling.md) 的 PD-2 条目；字段与顺序规则见[设计细则 §3](../../../../stage_plans/active/stage-07/presentation-decoupling-design.md#3-projection-的确定性处理pd-2)。

实施重点是把绑定索引移至 prepare 阶段，并建立 Projection Adapter 候选状态：`projectionCursor`、active effects、group state、future queue 和 rejected group state。按 Fact ordinal 处理并用稳定 identity 去重；只有 Runtime 成功后才安装候选并推进 cursor。继续使用现有 golden，覆盖 any/all/groupCommit、partial-group、aliases/去重、目标缺失、future/到期、无新增 Fact 更新、scope 重建和 Runtime 失败重试；不得重生成 golden 掩盖差异，也不得改变 Judgement Kernel 或 Gameplay 语义。

PD-1 报告指出，未知必需枚举的拒绝分支没有损坏 KernelProjection 注入测试证明失败后 cursor、scope 与 Runtime 状态保持不变。这是尚缺的运行时证据，不是已确认缺陷；如 PD-2 实施触及该路径或测试设施，可补最小 test-only 验证并如实记录。

## 边界

PD-2 完成时更新 `docs/CURRENT_STATUS.md`、PD 计划清单和带日期实施报告，并保留 PD-3-a 至 PD-5 未开始状态。不要把 PD 进度写成 Stage 7A、RPA 或整体 Stage 7 已关闭；不要 push、创建 PR、合并或发布。PD-0 冻结证据与失败样本继续保留。

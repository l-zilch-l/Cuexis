# New RPA Reports and Evidence

状态：current index

更新日期：2026-10-27

性能整改工作线，设计与 R0、R1、R2 分组；停止、失败和后续报告保留在同批目录。

当前状态只以 [CURRENT_STATUS](../../../../CURRENT_STATUS.md) 为准；本页只负责定位，不新增实施、测量或阶段接受结论。

先按下表定位批次；同批报告与附件放在同一目录。完整原始路径直接查 [证据清单](evidence-catalog.md)，无须逐篇翻阅。

返回 [Stage 7 阶段总表](../README.md)。计划入口：[对应阶段计划](../../../../stage_plans/active/stage-07/05-rpa-new/README.md)。

工作树整合与本次文档整理记录见 [共享材料](../90-shared/README.md)。

## 批次定位（实施推进顺序）

| 批次目录 | 报告 / 附件 | 直接入口 |
| --- | --- | --- |
| `design` | 1 / 0 | [2026-10-24 · Stage 7-RPA 性能整改候选设计审阅（2026-10-24）](design/2026-10-24-performance-remediation-design-review.md) |
| `r0` | 1 / 0 | [2026-10-24 · R0 observation and anchor](r0/2026-10-24-r0-observation-and-anchor.md) |
| `r1` | 2 / 0 | [New RPA / r1](r1/README.md) |
| `r2` | 8 / 5 | [New RPA / r2](r2/README.md) |
| `r3` | 10 / 0 | [R3a 恢复闭包与准备去重](r3/2026-10-27-r3a-recovery-and-prepare.md)、[R3b 可信前缀与成本核验](r3/2026-10-27-r3b-certified-prefix-and-cost.md)、[空推进 reserve 限定尝试](r3/2026-10-27-r3-empty-reserve-attempt.md)、[高 requirements 耗时归因](r3/2026-10-27-r3-high-requirements-attribution.md)、[分页与自然锚点重构](r3/2026-10-27-r3-redesign.md)、[首次身份与体积归因](r3/2026-10-27-r3-cold-identity-attribution.md)、[哈希与编码器恢复点](r3/2026-10-27-r3-hash-and-wire-checkpoint.md)、[来源绑定实验](r3/2026-10-27-r3-source-binding-experiment.md)、[完整 Packed 准备归因](r3/2026-10-27-r3-packed-prepare-attribution.md)、[完整 Packed 索引优化](r3/2026-10-27-r3-packed-index-optimization.md) |

## 原始材料与历史位置

- [按批次列出的原始证据路径](evidence-catalog.md)
- [重组前路径映射](../legacy-paths.md)

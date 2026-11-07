# Stage 7 Reports and Evidence

状态：current index

更新日期：2026-11-04

当前状态只以 [CURRENT_STATUS](../../../CURRENT_STATUS.md) 为准；本页只负责定位，不新增实施、测量或阶段接受结论。


实施脉络：A → 独立诊断前置 → 旧 RPA → PD → 新 RPA；B+ 尚未实施，作为后续阶段置后。共享材料列于末尾。数字是目录排序号，不是项目阶段编号，也不表示阶段已验收关闭。A、诊断与旧 RPA 的设计/设备材料存在日期交叉，旧 RPA 与 PD 也有重叠；按实际实施主线和前置关系定位，不将交叉材料解释为严格串行。

## 阶段与批次总表

同一批次的报告、审查、交接与 ZIP/JSON/LOG 附件集中在同一英文目录。每阶段索引列出全部批次；证据清单直接列出历史原始路径，避免逐篇翻找。

| 阶段 | 批次结构 | 报告入口 | 原始证据路径 |
| --- | --- | --- | --- |
| Stage 7A | `s7a-0 / s7a-1 / s7a-2 / s7a-3 / s7a-3-4 / s7a-4 / s7a-5-6 / s7a-6-7 / s7a-7-8 / s7a-8-4 / cross-batch / stage-06-handover` | [批次总表](01-a/README.md) | [证据清单](01-a/evidence-catalog.md) |
| Diagnostics | `audit / n0 / n1 / n3` | [批次总表](02-diagnostics/README.md) | [证据清单](02-diagnostics/evidence-catalog.md) |
| Legacy RPA | `design / b0–b4 / deployment / f1-f2 / f3 / f4 / performance / device` | [批次总表](03-rpa-legacy/README.md) | [证据清单](03-rpa-legacy/evidence-catalog.md) |
| Presentation Decoupling | `design / pd-0 / pd-1 / pd-2 / pd-3/a–c / pd-4 / pd-5` | [批次总表](04-pd/README.md) | [证据清单](04-pd/evidence-catalog.md) |
| New RPA | `design / r0 / r1 / r2 / r3 / r4` | [批次总表](05-rpa-new/README.md) | [证据清单](05-rpa-new/evidence-catalog.md) |
| Stage 7B+ | `现有材料中无独立报告` | [批次总表](06-b/README.md) | [证据清单](06-b/evidence-catalog.md) |
| Shared Evidence | `ci / release-gates / document-restoration / worktree-consolidation / documentation` | [批次总表](90-shared/README.md) | [证据清单](90-shared/evidence-catalog.md) |

## 计划与回溯

- [五阶段计划与配套文档总表](../../../stage_plans/active/stage-07/README.md)；[Stage 7 全局计划](../../../stage_plans/active/stage-07/plan.md)。
- [旧路径到当前位置映射](legacy-paths.md)：保留至少一个整理周期。
- 原始 out、ETL、claim/seal、登记组、失败窗口与封包内部路径保持原位；本次只移动 docs 中附件，原始字节不变。
- 历史报告的日期、结论、绝对路径与源码身份保留；历史“下一步”不覆盖当前状态和授权。

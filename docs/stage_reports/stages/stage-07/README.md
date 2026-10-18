# Stage 7 Evidence Index

状态：current index

更新日期：2026-10-17

本索引只负责导航，不复制合同、裁定或当前实现状态。当前状态以 [CURRENT_STATUS](../../../CURRENT_STATUS.md) 为准；范围与验收门禁以 [Stage 7 总计划](../../../stage_plans/active/stage-07/plan.md)、[Stage 7A](../../../stage_plans/active/stage-07/plan-a.md)、[RPA](../../../stage_plans/active/stage-07/plan-rpa.md) 和 [PD](../../../stage_plans/active/stage-07/plan-presentation-decoupling.md) 为准。

## 分类入口

- [Stage 7A](s7a/README.md)：准入、裁定、实施、验证和交接。
- [RPA](rpa/README.md)：实时播放、宿主、设备、性能和生产验收。
- [PD](pd/README.md)：Presentation Decoupling 设计与 PD-0 测量。
- [诊断正式化](diagnostic-formalization/README.md)：N0-N3 独立诊断前置证据。
- [共享证据](shared/README.md)：跨子阶段门禁、文档恢复和发布历史。
- [旧路径映射](legacy-paths.md)：整理前路径到 canonical 文档的映射。

## 当前工作线

PD 当前仍处于 PD-0。无插桩基线、补充复测和测量方法演进都集中在 [PD 证据索引](pd/README.md)；历史原始测量包按 owner 要求清理后，不再将旧临时路径当作可复核入口。

RPA 与 Stage 7A 的历史报告继续保留原日期和证据范围。报告中的历史“下一步”不覆盖当前计划；新的实现状态只写入 [CURRENT_STATUS](../../../CURRENT_STATUS.md)。

## 归档原则

- 报告按证据用途归类，正文中的日期、源码身份和验收范围保持不变。
- 已被摘要替代的临时测量记录不再单独挂在索引中；摘要保留其限制和结论边界。
- ZIP、JSON 和日志等原始证据跟随所属报告保存，不在本索引复制内容。

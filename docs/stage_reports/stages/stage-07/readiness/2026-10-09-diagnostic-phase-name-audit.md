# 阶段诊断码审计与正式命名迁移规划

状态：dated audit；仅规划，重命名未实施

证据日期：2026-10-09

归属更新（同日后续owner指令）：本报告的审计发现及测量有效范围保留；原S7A-7/8下RT78/DN78
工作归属由[独立Stage RPA](../../../../stage_plans/future/realtime-playback-foundation/plan.md)取代。
原plan-a链接现为交接入口；不将迁移工作计为7/8新增缺口，不将原有设备失败改记通过。
迁移记录与原规划文本归[拆分报告](../handoffs/2026-10-09-realtime-stage-separation.md)。

## 1. 范围与审计结果

代码基线：stage-7，`d33c64b15575a945e50948ad70acb88f05117dce`，现有PR32。
用户要求派出子代理检查类似`judgement.s7a2.input.same_tick_collision`的阶段诊断码，
并采用`judgement.input.same_tick_collision`等正式名称。本轮沿用只规划、不实施范围；
保留此前未提交实时架构规划。主代理和子代理均未修改产品代码、现行码表、版本或历史日志。

审计覆盖git跟踪的engine/app/schemas/cmake/tools/tests及docs文本，逐项核查定义和使用；
除了完整字符串，还检查execution_profile和execution_kernel的动态前缀/后缀构造。
有效阶段码定义共67个：s7a2为18、s7a3为41、s7a4为8。集中表已登记公共码13、
明确src-only码13、其余未登记内部码41；公开性不能由名字推断。
其中1个未调用占位建议删除，其余66个迁移至无阶段名的名称；现有目标未发现字符串碰撞。

[完整机器清单与原始检查](2026-10-09-diagnostic-phase-name-audit-evidence.zip)逐码记录旧名、
建议新名、符号、类别与消费者/测试/合同/历史引用。本报告行号只定位此次基线，不构成长期合同锚点。

## 2. 命名规则与完整清单

推荐`模块.子域.具体原因`，阶段/执行卡信息保留在provenance或开发文档。
去前缀不改变category/severity/faulted、R编号、拒绝行为、输入规则或公共承载面。
内部码重命名后仍为内部码；不能因为名称正式就登记为新的公共能力。

| 旧码 | 推荐正式码或处置 | 当前承载 |
| --- | --- | --- |
| `judgement.s7a2.input.amount_narrowed` | `judgement.input.amount_narrowed` | 集中表公共 |
| `judgement.s7a2.input.amount_out_of_range` | `judgement.input.amount_out_of_range` | 集中表公共 |
| `judgement.s7a2.input.ingress_sequence_duplicate` | `judgement.input.ingress_sequence_duplicate` | 明确src-only |
| `judgement.s7a2.input.mapping_declaration_invalid` | `judgement.input.mapping_declaration_invalid` | 明确src-only |
| `judgement.s7a2.input.runtime_mapping_change` | `judgement.input.runtime_mapping_change` | 明确src-only |
| `judgement.s7a2.input.same_tick_collision` | `judgement.input.same_tick_collision` | 集中表公共 |
| `judgement.s7a2.late.duplicate_queue_entry` | `judgement.late.duplicate_queue_entry` | 明确src-only |
| `judgement.s7a2.late.parameter_pending` | `judgement.late.parameter_pending` | 明确src-only |
| `judgement.s7a2.late.policy_undeclared` | `judgement.late.policy_undeclared` | 明确src-only |
| `judgement.s7a2.late.queue_hop_exceeded` | `judgement.late.queue_hop_exceeded` | 明确src-only |
| `judgement.s7a2.timebase.commit_not_at_commit_tick` | `judgement.timebase.commit_not_at_commit_tick` | 明确src-only |
| `judgement.s7a2.timebase.commit_window_invalid` | `judgement.timebase.commit_window_invalid` | 明确src-only |
| `judgement.s7a2.timebase.interval_reversed` | `judgement.timebase.interval_reversed` | 明确src-only |
| `judgement.s7a2.timebase.profile_invalid` | `judgement.timebase.profile_invalid` | 明确src-only |
| `judgement.s7a2.timebase.profile_value_out_of_range` | `judgement.timebase.profile_value_out_of_range` | 明确src-only |
| `judgement.s7a2.timebase.rational_invalid` | `judgement.timebase.rational_invalid` | 明确src-only |
| `judgement.s7a2.timebase.tick_overflow` | `judgement.timebase.tick_overflow` | 集中表公共 |
| `judgement.s7a2.timebase.time_reversal` | `judgement.timebase.time_reversal` | 集中表公共 |
| `judgement.s7a3.budget.content_profile_exceeded` | `judgement.budget.content_profile_exceeded` | 未登记内部 |
| `judgement.s7a3.closure.capability_disabled` | `judgement.closure.capability_disabled` | 未登记内部 |
| `judgement.s7a3.closure.declared_incomplete` | `judgement.closure.declared_incomplete` | 未登记内部 |
| `judgement.s7a3.content.unsupported_form` | `judgement.content.unsupported_form` | 未登记内部 |
| `judgement.s7a3.declaration.duplicate_entry` | `judgement.declaration.duplicate_entry` | 未登记内部 |
| `judgement.s7a3.declaration.structurally_incomplete` | `judgement.declaration.structurally_incomplete` | 未登记内部 |
| `judgement.s7a3.domain.dynamic_frame_unsupported` | `judgement.domain.dynamic_frame_unsupported` | 未登记内部 |
| `judgement.s7a3.domain.geometry_overflow` | `judgement.domain.geometry_overflow` | 未登记内部 |
| `judgement.s7a3.domain.narrowing_rejected` | `judgement.domain.narrowing_rejected` | 未登记内部 |
| `judgement.s7a3.domain.range_reversed` | `judgement.domain.range_reversed` | 未登记内部 |
| `judgement.s7a3.domain.range_undeclared` | `judgement.domain.range_undeclared` | 未登记内部 |
| `judgement.s7a3.entry.chart_version_unsupported` | `judgement.entry.chart_version_unsupported` | 未登记内部 |
| `judgement.s7a3.entry.playback_entry_mismatch` | `judgement.entry.playback_entry_mismatch` | 未登记内部 |
| `judgement.s7a3.entry.source_document_duplicate` | `judgement.entry.source_document_duplicate` | 未登记内部 |
| `judgement.s7a3.entry.source_set_empty` | `judgement.entry.source_set_empty` | 未登记内部 |
| `judgement.s7a3.entry.timebase_missing` | `judgement.entry.timebase_missing` | 未登记内部 |
| `judgement.s7a3.grace.chart_supply_not_allowed` | `judgement.grace.chart_supply_not_allowed` | 未登记内部 |
| `judgement.s7a3.grace.inheritance_undeclared` | `judgement.grace.inheritance_undeclared` | 未登记内部 |
| `judgement.s7a3.grace.inheritance_unexpected` | `judgement.grace.inheritance_unexpected` | 未登记内部 |
| `judgement.s7a3.grace.not_representable` | `judgement.grace.not_representable` | 未登记内部 |
| `judgement.s7a3.grace.out_of_range` | `judgement.grace.out_of_range` | 未登记内部 |
| `judgement.s7a3.grace.quantization_mismatch` | `judgement.grace.quantization_mismatch` | 未登记内部 |
| `judgement.s7a3.grace.range_reversed` | `judgement.grace.range_reversed` | 未登记内部 |
| `judgement.s7a3.grace.unit_not_positive` | `judgement.grace.unit_not_positive` | 未登记内部 |
| `judgement.s7a3.grace.value_missing` | `judgement.grace.value_missing` | 未登记内部 |
| `judgement.s7a3.identity.collision` | `judgement.identity.collision` | 未登记内部 |
| `judgement.s7a3.measure.category_not_derived_from_phase` | `judgement.measure.category_not_derived_from_phase` | 未登记内部 |
| `judgement.s7a3.merge.cross_document_reference_implicit` | `judgement.merge.cross_document_reference_implicit` | 未登记内部 |
| `judgement.s7a3.merge.declaration_name_duplicate` | `judgement.merge.declaration_name_duplicate` | 未登记内部 |
| `judgement.s7a3.merge.declaration_ordinal_unassigned` | `judgement.merge.declaration_ordinal_unassigned` | 未登记内部 |
| `judgement.s7a3.merge.reference_dangling` | `judgement.merge.reference_dangling` | 未登记内部 |
| `judgement.s7a3.merge.stable_id_duplicate` | `judgement.merge.stable_id_duplicate` | 未登记内部 |
| `judgement.s7a3.pattern.arm_bound_exceeded` | `judgement.pattern.arm_bound_exceeded` | 未登记内部 |
| `judgement.s7a3.pattern.atom_outside_declared_arms` | `judgement.pattern.atom_outside_declared_arms` | 未登记内部 |
| `judgement.s7a3.pattern.budget_exceeded` | `judgement.pattern.budget_exceeded` | 未登记内部 |
| `judgement.s7a3.requirement.release_tail_implicit` | `judgement.requirement.release_tail_implicit` | 未登记内部 |
| `judgement.s7a3.requirement.solver_profile_missing` | `judgement.requirement.solver_profile_missing` | 未登记内部 |
| `judgement.s7a3.resource.claim_conflict` | `judgement.resource.claim_conflict` | 未登记内部 |
| `judgement.s7a3.resource.reference_dangling` | `judgement.resource.reference_dangling` | 未登记内部 |
| `judgement.s7a3.resource.state_transition_invalid` | `judgement.resource.state_transition_invalid` | 未登记内部 |
| `judgement.s7a3.second_half.not_implemented` | 删除无调用占位及辅助函数（候选） | 未登记内部 |
| `judgement.s7a4.execution.profile_incomplete` | `judgement.execution.profile_incomplete` | 集中表公共 |
| `judgement.s7a4.execution.profile_unsupported` | `judgement.execution.profile_unsupported` | 集中表公共 |
| `judgement.s7a4.execution.relation_invalid` | `judgement.execution.relation_invalid` | 集中表公共 |
| `judgement.s7a4.kernel.transaction_failed` | `judgement.kernel.transaction_failed` | 集中表公共 |
| `judgement.s7a4.late.dispatch_collision` | `judgement.late.dispatch_collision` | 集中表公共 |
| `judgement.s7a4.late.parameters_invalid` | `judgement.late.parameters_invalid` | 集中表公共 |
| `judgement.s7a4.late.rejected` | `judgement.late.rejected` | 集中表公共 |
| `judgement.s7a4.session.lifecycle_order` | `judgement.session.lifecycle_order` | 集中表公共 |

## 3. 需要保留的兼容和投影边界

`engine/playback/src/gameplay_internal.hpp`的projectGameplayError按s7a2/s7a3前缀识别内部
来源，并对5个公共输入/时基码透传。迁移应将来源判定改为显式码集合/描述元数据，
保持原公共投影和sourceCode上下文；不得去掉分流条件后让全部内部码直接穿过公共API。
类别映射中的s7a4.late.parameters_invalid也须同时更新。集中表与CMake诊断校验器、
Spec/ABI/profile首用条目须按同一映射修订；src-only不因去前缀变为pending公共登记。

`engine/judgement/src/execution_kernel.cpp`对kernel.transaction_failed的恢复校验按字面码串比较；
`recovery.cpp`的sameError也比较码。`recovery_wire.hpp`序列化faultDiagnostic，
`recovery_codec.cpp`对payload做digest。因此改名可改变完整结果、恢复载荷字节和摘要，
不能按纯日志显示调整交付。旧owning结果保持旧值，不原地改写；新旧完整结果不能声称自动相等。

建议先在Recovery/Replay合同定义兼容边界：新写入采用新码并声明对应兼容revision；
旧revision的拒绝或显式迁移按受影响载体逐项决定。若提供迁移，必须先验证旧bytes/digest、
保留原件，再产出新载体和新digest；不能由Reader静默替换字符串。正常归档未包含诊断时是否
字节不变须实测；Replay失败评价的码变化须覆盖完整结果比较。具体revision及SDK patch/minor
由实际公共差异与VERSIONING证明决定，本审计不接受版本号或旧数据默认兼容。

## 4. 排除项和残留处置

- 已删除的`judgement.s7a3.pattern.expansion_not_representable`及
  `judgement.s7a3.pattern.state_count_not_measured`仅作历史说明，保留原名。
- `judgement.s7a3.second_half.not_implemented`只见kSecondHalfPendingCode定义及
  maybe_unused secondHalfError，无调用。建议单独删除占位函数和其remediation等残留，
  不正式化second_half这个实现阶段域；仍须验证没有测试/工具依赖。
- `grace.resolver.s2.v1`是Capsule已选S2解析算法的版本token，`local.close.t4.v1`同理，
  不属于Stage编号。它们的wire/identity合同保持，不能随诊断改名修改。
- material.stage2、mesh.stage2等是测试资产ID；runtime.stage2也属fixture标识。
  测试标签、目录、普通阶段编号、历史报告/ZIP/设备日志不作批量替换。

## 5. 后续实施与验收设计

实施归[plan-a §3.4.19](../../../../stage_plans/active/stage-07/plan-a.md#3419-诊断码去阶段命名规划2026-10-09)，
与实时架构整改分开提交和证明，避免把命名变更与判定行为修改混在同一验收中。

1. 合同与码表：定稿66条映射、公开性、fault恢复兼容、版本差异；保持九类别和拒绝语义。
2. 生产者与消费者：更新常量/动态构造、显式投影集合、严格恢复验证、工具与fixture预期。
3. 门禁：除历史/排除项外，活动诊断码不得包含阶段段；验证目标唯一性、每码类别与原公开性对应。
4. 功能证明：same_tick_collision等负例新码，内部错误投影/sourceCode，kernel故障注入、
   owning旧值、完整ReplayEvaluation、Snapshot/Recovery旧新载体/损坏摘要/未知revision拒绝。
5. 受影响Debug/Release、candidate ON/OFF、headless、static/shared消费者与MinGW/Linux，
   诊断码表/架构/package/ASCII/docs/version检查；平台未执行项逐项记录。

本轮仅完成只读审计和迁移规划，未证明新码的任何运行验收，S7A-8.4未退出。

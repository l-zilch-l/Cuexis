# 诊断码与类别（可读视图）

<!-- cuexis-meta
id: GUIDE-DIAGNOSTIC-CODES
status: current
authority: references
-->

状态：current（由 `tools/verification/gen_docs_codes.py` 从 `schemas/cuexis.gameplay-diagnostics.v2.codes.json` 生成，勿手改）

表 `cuexis.gameplay-diagnostics.v2.codes`，schemaVersion `2`，注册状态 `registered-pending-owner-review`。

**权威边界**：本页只是注册表的可读投影。九个类别的语义权威仍是 `docs/formats/GAMEPLAY_V2_SPEC.md` §9.2；码的注册与首次消费以 JSON 为准。本页不新增裁定，也不改变任何码的含义。

## 类别

| 类别 | 来源 |
| --- | --- |
| `unknown_capability` | GAMEPLAY_V2_SPEC.md#9.2 |
| `capability_disabled` | GAMEPLAY_V2_SPEC.md#9.2 |
| `budget_exceeded` | GAMEPLAY_V2_SPEC.md#9.2 |
| `ambiguous_migration` | GAMEPLAY_V2_SPEC.md#9.2 |
| `non_terminating_source` | GAMEPLAY_V2_SPEC.md#9.2 |
| `non_unique_solution` | GAMEPLAY_V2_SPEC.md#9.2 |
| `invalid_relation` | GAMEPLAY_V2_SPEC.md#9.2 |
| `late_policy_incomplete` | GAMEPLAY_V2_SPEC.md#9.2 |
| `identity_closure_incomplete` | GAMEPLAY_V2_SPEC.md#9.2 |

## 已注册码（73 条）

| 码 | 类别 | 严重度 | 会话影响 | 首个消费批次 |
| --- | --- | --- | --- | --- |
| `format.gameplay_version_unsupported` | ambiguous_migration | error | session_unaffected | S7A-3 |
| `migration.ambiguous` | ambiguous_migration | error | session_unaffected | S7A-3 |
| `capability.budget_insufficient` | budget_exceeded | error | session_unaffected | S7A-3 |
| `graph.budget.exceeded` | budget_exceeded | error | session_unaffected | S7A-7 |
| `judgement.input.amount_narrowed` | budget_exceeded | error | session_unaffected | S7A-2 |
| `judgement.input.amount_out_of_range` | budget_exceeded | error | session_unaffected | S7A-2 |
| `judgement.timebase.tick_overflow` | budget_exceeded | error | session_unaffected | S7A-2 |
| `capability.disabled` | capability_disabled | error | session_unaffected | S7A-2 |
| `capability.future_development` | capability_disabled | error | session_unaffected | S7A-3 |
| `capability.permanently_unsupported` | capability_disabled | error | session_unaffected | S7A-3 |
| `coordination.relation_unsupported` | capability_disabled | error | session_unaffected | S7A-4 |
| `fact.correction_unsupported` | capability_disabled | error | session_unaffected | S7A-5 |
| `input.continuous_unsupported` | capability_disabled | error | session_unaffected | S7A-2 |
| `input.direction_unsupported` | capability_disabled | error | session_unaffected | S7A-3 |
| `judgement.execution.profile_unsupported` | capability_disabled | error | session_unaffected | S7A-4 |
| `late.reopen_unsupported` | capability_disabled | error | session_unaffected | S7A-2 |
| `resource.capacity_unsupported` | capability_disabled | error | session_unaffected | S7A-4 |
| `resource.handoff_unsupported` | capability_disabled | error | session_unaffected | S7A-4 |
| `ruleset.package_unsupported` | capability_disabled | error | session_unaffected | S7A-5 |
| `solver.profile_unsupported` | capability_disabled | error | session_unaffected | S7A-4 |
| `graph.closure.invalid` | identity_closure_incomplete | error | session_unaffected | S7A-7 |
| `graph.identity.mismatch` | identity_closure_incomplete | error | session_unaffected | S7A-7 |
| `identity_closure_incomplete` | identity_closure_incomplete | error | session_unaffected | S7A-7 |
| `judgement.execution.profile_incomplete` | identity_closure_incomplete | error | session_unaffected | S7A-4 |
| `codec.allocation_failed` | invalid_relation | error | session_unaffected | S7A-7 |
| `codec.budget_exceeded` | invalid_relation | error | session_unaffected | S7A-7 |
| `codec.budget_unaccepted` | invalid_relation | error | session_unaffected | S7A-7 |
| `codec.count_mismatch` | invalid_relation | error | session_unaffected | S7A-7 |
| `codec.digest_mismatch` | invalid_relation | error | session_unaffected | S7A-7 |
| `codec.identity_mismatch` | invalid_relation | error | session_unaffected | S7A-7 |
| `codec.length_invalid` | invalid_relation | error | session_unaffected | S7A-7 |
| `codec.magic_invalid` | invalid_relation | error | session_unaffected | S7A-7 |
| `codec.record_invalid` | invalid_relation | error | session_unaffected | S7A-7 |
| `codec.section_invalid` | invalid_relation | error | session_unaffected | S7A-7 |
| `codec.tag_invalid` | invalid_relation | error | session_unaffected | S7A-7 |
| `codec.trailing_bytes` | invalid_relation | error | session_unaffected | S7A-7 |
| `codec.truncated` | invalid_relation | error | session_unaffected | S7A-7 |
| `codec.utf8_invalid` | invalid_relation | error | session_unaffected | S7A-7 |
| `codec.version_unsupported` | invalid_relation | error | session_unaffected | S7A-7 |
| `geometry.inference_rejected` | invalid_relation | error | session_unaffected | S7A-7 |
| `graph.header.unsupported_format` | invalid_relation | error | session_unaffected | S7A-7 |
| `graph.integer.out_of_range` | invalid_relation | error | session_unaffected | S7A-7 |
| `graph.structure.duplicate_key` | invalid_relation | error | session_unaffected | S7A-7 |
| `graph.structure.invalid` | invalid_relation | error | session_unaffected | S7A-7 |
| `judgement.control.reconciliation_invalid` | invalid_relation | error | session_unaffected | Stage 7-RPA B3 |
| `judgement.execution.relation_invalid` | invalid_relation | error | session_unaffected | S7A-4 |
| `judgement.input.occurrence_conflict` | invalid_relation | error | session_unaffected | Stage 7-RPA B3 |
| `judgement.input.same_tick_collision` | invalid_relation | error | session_unaffected | S7A-2 |
| `judgement.kernel.transaction_failed` | invalid_relation | error | session_faulted | S7A-4 |
| `judgement.late.dispatch_collision` | invalid_relation | error | session_unaffected | S7A-4 |
| `judgement.session.lifecycle_order` | invalid_relation | error | session_unaffected | S7A-4 |
| `judgement.timebase.stop_endpoint_collision` | invalid_relation | error | session_unaffected | RR-FMT |
| `judgement.timebase.stop_quantized_interval_invalid` | invalid_relation | error | session_unaffected | RR-FMT |
| `judgement.timebase.time_reversal` | invalid_relation | error | session_unaffected | S7A-2 |
| `partial-group` | invalid_relation | error | session_unaffected | S7A-7 |
| `pattern.relation_unsupported` | invalid_relation | error | session_unaffected | S7A-3 |
| `playback.gameplay.invalid` | invalid_relation | error | session_unaffected | S7A-7 |
| `presentation-target-missing` | invalid_relation | error | session_unaffected | S7A-7 |
| `replay.identity_mismatch` | invalid_relation | error | session_unaffected | S7A-7 |
| `replay.state_invalid` | invalid_relation | error | session_unaffected | S7A-7 |
| `ruleset.configuration_invalid` | invalid_relation | error | session_unaffected | S7A-7 |
| `ruleset.hook_invalid` | invalid_relation | error | session_unaffected | S7A-7 |
| `ruleset.registry_invalid` | invalid_relation | error | session_unaffected | S7A-7 |
| `ruleset.transaction_failed` | invalid_relation | error | session_faulted | S7A-5 |
| `session.mutation_rejected` | invalid_relation | error | session_unaffected | S7A-1 |
| `snapshot.state_invalid` | invalid_relation | error | session_unaffected | S7A-7 |
| `judgement.late.parameters_invalid` | late_policy_incomplete | error | session_unaffected | S7A-4 |
| `judgement.late.rejected` | late_policy_incomplete | error | session_unaffected | S7A-4 |
| `playback.gameplay.non_terminating_source` | non_terminating_source | error | session_unaffected | S7A-7 |
| `playback.gameplay.non_unique_solution` | non_unique_solution | error | session_unaffected | S7A-7 |
| `capability.revision_mismatch` | unknown_capability | error | session_unaffected | S7A-3 |
| `capability.unknown` | unknown_capability | error | session_unaffected | S7A-7 |
| `graph.header.unsupported_revision` | unknown_capability | error | session_unaffected | S7A-7 |

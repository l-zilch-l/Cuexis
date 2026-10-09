# Compiled Gameplay Graph JSON candidate v1

Status: candidate first-use contract, 2026-10-07 revision 1. This document does not certify
a Reader, Writer, Entry consumer, production budget or Stage 7A exit. Current implementation
status belongs to [CURRENT_STATUS](../CURRENT_STATUS.md).

This is the explicit compiled JSON representation selected by P78-04 A and R78-08 A.
It contains owning typed semantic records, not author source, a Capsule byte string, CXT,
an executable program or a Recovery execution graph. Its semantic rules remain those of
[Gameplay V2](GAMEPLAY_V2_SPEC.md), the [execution profile](gameplay-v2-execution-profile.md)
and [Capsule revision 3](GAMEPLAY_CAPSULE_V2_FORMAT.md).

The candidate [JSON Schema](../../schemas/cuexis.gameplay-graph.v1.schema.json) owns structural
shape only. It cannot enforce physical member order, duplicate keys, integer parser-token
categories, typed row field positions/counts, finite conversion, closures or identities.
Those checks remain mandatory in the typed SAX Reader; schema acceptance is insufficient.

## Envelope and ownership

The required root fields, in Writer order, are:

| Field | Physical type and meaning |
| --- | --- |
| format | Exact UTF-8 string `cuexis.gameplay-graph` |
| graphFormatRevision | Integer 1; this revision does not change Chart or Gameplay versions |
| capsuleRevision | Integer 3; selects the corresponding semantic record contract explicitly |
| semanticIdentity | 64 lowercase ASCII hex characters: SHA-256 of the existing Capsule structural semantic preimage |
| staticChart | Complete static chart record described below |
| GPH0 | One typed global record stream, using the revision 3 field order |
| GPR0 | Array of typed Requirement record streams; each row includes its owner and reference indices |
| GPD0 | Object with exactly patterns, measures, judgementDomains and mergedDeclarations; each is an array of typed row streams |
| GRC0 | Object with exactly resources, relations, solverProfiles, factBindings and claims; each is an array of typed row streams |

All fields are mandatory. Unknown fields, duplicate keys and unsupported revisions reject.
The four envelope header members must precede every payload member; their order within that
prefix may vary. Payload member order may vary. A payload before a complete validated header
rejects instead of guessing a revision. Dependencies and references are linked only after all required
typed records are present; physical member order never chooses a semantic interpretation.
The source owner reads bytes once, verifies the Entry artifact identity, and hands the same
owning bytes to the Reader. The owning decoded result outlives Provider reads. Playback never
compiles author source or invokes an offline tool.

## Typed record streams

The existing Capsule field tables uniquely define the field order, variant tags, optional
presence and counts. JSON record streams represent those fields directly:

| Capsule atom | JSON representation |
| --- | --- |
| U / V / Ix(T) / enum tag | JSON integer in the exact u32/u64/u8 range owned by that field |
| Z | JSON integer in i64 range; a nonnegative unsigned parser token is accepted only after checked i64 range validation |
| B | JSON boolean |
| S | Literal UTF-8 string, rather than a dictionary index |
| Rk | Object with exactly `kind` (integer k) and `token` (UTF-8 string); kind must match the consuming field |
| O(X) | Boolean presence followed by X only when true; absence consumes no X fields |
| counted list/table | Exact u32 count followed by the declared fields/rows |

Each record stream is an array. GPH0 is one record; GPR0 and every definitions/coordination
table separate rows into arrays. Their table counts come from GPH0; per-row counted lists retain
the Capsule count fields. This permits SAX to validate and append one typed row at a time.
No record permits trailing atoms. A count must fit the
remaining typed record input and the explicitly supplied candidate resource bounds before
reserve or expansion. References remain typed: an owner index cannot become a Requirement,
Pattern, resource or string index. No integer is converted through f64. JSON number tokens
with a fractional part or exponent reject in integer fields, even if mathematically integral.
REF0 is reconstructed from literal typed references; its actual count must equal GPH0's count.
The existing unused definitions, closure declarations and execution fields remain present.

## Complete static chart

staticChart has exactly `chartId`, `mainMusic`, `features`, `timing`, `defaultCamera`,
`resourceClosure` and `entities`. All are mandatory; optional values use JSON null.

| Record | Exact fields |
| --- | --- |
| feature | id:string, version:u32 |
| timing | offsetMs:f64, defaultBpm:f64, tempoEvents:array, stops:array |
| tempo event | startBeat:rational pair, durationBeats:rational pair, startBpm:f64, endBpm:f64, startSlope:f64, endSlope:f64 |
| stop | beat:rational pair, durationMs:f64 |
| rational pair | `[numerator:i64, denominator:i64]`, canonical positive denominator; existing exact RationalBeat validation applies |
| defaultCamera | type:string, fovY:f64, nearPlane:f64, farPlane:f64, pitch:f64, yaw:f64, roll:f64, defaultTransform:transform or null |
| transform | position:3 f32, rotation:4 f32 in existing quaternion component order, scale:3 f32 |
| resource use | assetId:string, use:u8 using the existing MainMusic/Mesh/Material tags |
| entity | identity:entity identity, parent:entity identity or null, components:array |
| explicit identity | kind:integer 0, objectId:string |
| generated identity | kind:integer 1, chartId:string, bindingId:string, moduleId:string, exportId:string, path:array |
| generated identity path step | nodeId:string, iterationIndexPlusOne:u32; distinct from Requirement emissionPath repeatIndex:u64 |
| transform component | kind:integer 0, value:transform |
| renderable component | kind:integer 1, mesh:string, material:string, alpha:u8 |
| camera component | kind:integer 2, type:string, fovY:f64, nearPlane:f64, farPlane:f64 |

Unknown record fields reject. Float fields require finite values and the existing static chart
validation; f32 conversion must round trip the declared f32 value. Float fields are confined to
existing presentation/static timing data; they cannot supply Gameplay Tick or Requirement time.
There is no legacy Requirement array in an entity: ownership comes exclusively from GPR0.
Entity/component/closure ordering and uniqueness remain the existing canonical typed rules.

## Identity, validation and failure

Writer consumes the same complete typed EncodeRequest as Capsule. It validates the existing
canonical graph, static chart, owners, references and closures before emitting deterministic
UTF-8 JSON. Writer member order follows this document; strings use standard JSON escaping,
integers use decimal without exponent, floats use locale-independent round-trip decimal.
Float Writer tokens always contain a decimal point or exponent, including `-0.0` for negative
zero, so the JSON parser cannot erase a floating sign by classifying the token as integer zero.
Reader accepts whitespace and the member reordering permitted above, not guessed profiles or alternate tags.

Reader validates format/revision and field shape through json_support SAX events and directly
constructs checked typed records. It does not first construct a JSON DOM. Duplicate/unknown
keys and structural/count violations reject before the affected owning container grows.
After typed closure validation, Reader derives the existing structural preimage and verifies
semanticIdentity, then invokes the same canonical prepare path as Capsule. Graph/Packed
artifact bytes and hashes differ; their complete typed semantic result and Judgement identity
must agree. Entry's seven metadata fields and sourceOf belong to
[ChartEntry](CHART_ENTRY_V1_FORMAT.md) and [CXC](CXC_FORMAT.md), not this envelope.

Writer 首用的 `GraphWriterLimits` 明确携带 `maxBytes`（最终 UTF-8 文本字节）、
`maxStringBytes`（任一 decoded string 字节）、`maxRowAtoms`（任一 typed row atom 个数）
和 `testOnly`。三项数值均须为正，testOnly 必须 true；没有默认值或生产接受。
任一追加之前检查最终文本界，任一 atom 之前检查行 atom 界；字符串先验证 UTF-8
和 decoded 字节，再进行 escaping。Writer 使用既有 canonical/Packed 限额验证其输入，
这些既有限额不当成新的 Graph 文本默认值。
Reader 首用 `GraphReaderLimits` 显式携带 maxBytes/maxDepth/maxStringBytes/maxValues/
maxContainerElements（json_support 结构准入）、maxRowAtoms、maxRowDecodedStringBytes
（单行临时原子与字符串累计界）及 testOnly；所有数值为正且 testOnly=true，无生产默认。
资源/Requirement 引用先保存 typed index/claim-key，完整表就绪后按原 Capsule 链接与
结构 claim-key 规则验证，不缓存整篇原始 row，不通过物理表顺序推断所有权。

Candidate text/record/string/element bounds are explicit caller inputs with testOnly admission.
Temporary row input has an explicit scalar/decoded-string bound and is discarded immediately
after checked typed row construction. It is not a whole-document DOM; neither a claimed count
nor an unclosed row may reserve an unbounded typed table. The Reader verifies count versus the
bounded remaining row input before constructing counted typed subcontainers.
They are not defaults or accepted production thresholds. Packed byte numbers cannot be copied
into Graph limits without a dimension/comparability proof. Grammar bounds, new measured inputs
and production INCOMPLETE GATE are recorded separately; incomplete production budgets reject.
The Graph-specific stable diagnostics are:

| Code | Existing V2 category |
| --- | --- |
| graph.header.unsupported_format | invalid_relation |
| graph.header.unsupported_revision | unknown_capability |
| graph.structure.invalid | invalid_relation |
| graph.structure.duplicate_key | invalid_relation |
| graph.integer.out_of_range | invalid_relation |
| graph.budget.exceeded | budget_exceeded |
| graph.identity.mismatch | identity_closure_incomplete |
| graph.closure.invalid | identity_closure_incomplete |

All are error/session_unaffected. An incomplete production budget uses the existing
capability.budget_insufficient. Raw input admission precedes SAX; SAX reports the first rejected
token/field in physical order. The header prefix validates before payload allocation. Typed
closure/count/link/identity and canonical prepare run in that fixed order after structural
decode. Existing canonical prepare failures retain their centralized public mappings.
These first-use codes must be registered in the central table before the first consumer.
Failed decode/prepare leaves the old combined session
and output artifacts unchanged. No package reader silently compiles or repairs the payload.

Acceptance requires manual Graph/Packed golden, an independent oracle, complete typed/result
comparison, source permutation determinism, integer extremes and 2^53 boundaries, duplicate
keys, count/short-input/unknown-field negatives, closure/hash tampering, failure ordering and
the installed ON/OFF/static/shared/platform matrix. Contract registration alone exits none of
C78-05/06 or R78-08.

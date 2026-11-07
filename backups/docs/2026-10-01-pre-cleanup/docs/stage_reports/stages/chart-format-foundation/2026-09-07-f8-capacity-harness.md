# Chart Format Foundation F8: Capacity and Security Harness

日期：2026-09-07  
状态：harness added; raw cross-platform measurements pending

## Harness

`tests/chart/chart_foundation_capacity_tests.cpp` now creates the declared low-reuse profile:
40,000 explicit entities, 40,000 independent explicit identities, one point Tap requirement per
entity, four-lane constraints, and distinct Beat values. The test uses the candidate Packed sizing
API and records `packedBytes`, `decodedBytes`, entity/requirement counts, and IDN0 section records
on standard output. It asserts the frozen 16 MiB Packed entry limit.

The companion negative test creates 40,001 entities and requires the encoder to fail with the
stable `packed.budget.entities` diagnostic before any candidate bytes are published. This keeps
the entity-count gate independent from the still-unfrozen decoded/section/prepare budgets.

Per owner decision on 2026-09-07, high-reuse and bounded-mixed generators are non-binding design
references rather than separate Foundation closure tests. Their expected lower encoded repetition
cost does not replace the measured low-reuse upper-bound profile.

## Security matrix covered by the harness

The test entry point is intended to run with the existing Packed negative matrix for truncated
headers/varints, CRC and directory failures, unknown sections, identity mismatch, and checked
entity/requirement budgets. F8 must retain the failure diagnostics and must not lower the 40,000
entity or 16 MiB limits to make a fixture pass.

## Pending evidence

This report intentionally does not claim Foundation closure. Raw section contributors, source
bytes, prepare peak, compile timing, Debug/Release/headless parity, and hosted MSVC/MinGW/Linux
evidence remain required before F8/F9 acceptance. High-reuse/mixed measurements are optional
follow-up observations and are not required for this Foundation gate.

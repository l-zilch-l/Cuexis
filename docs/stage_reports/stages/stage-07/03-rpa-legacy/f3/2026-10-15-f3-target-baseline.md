# RPA F3 Target Baseline

Date: 2026-10-15

## Current RPA measurement target

The current RPA F3 measurement uses exactly 40,000 expanded semantic entities
and 40,000 requirements, matching the current design maximum. The Packed Chart
entry target is 16 MiB (16,777,216 bytes).

The predeclared latency target for this baseline is less than or equal to
20 ms for the applicable measured latency, including the Seek latency item
where the measurement path supports it.

These values are current RPA implementation test targets, not final
production limits. The planned PD work will restructure the judgement system
and is expected to change the performance profile. After PD implementation,
the affected content, steady-state, snapshot/Seek, Replay, and Packed
measurements must be rerun and independently reviewed.

## Acceptance boundary

Current measurements may establish a baseline, expose regressions, and
identify whether the current implementation exceeds the predeclared target.
They must not be reported as the final post-PD production conclusion or used
to freeze limits before the PD rerun and owner acceptance.

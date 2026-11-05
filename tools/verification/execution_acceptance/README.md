# Execution acceptance tools

These tools prepare budgets, validate admission and seal fixed execution registrations.
The O1 reference budget was accepted externally on 2026-11-05; the frozen proposal retains
accepted=null. Admission verifies the separate owner record and exact accepted artifact hash.
Historical PD-5 collectors and their records remain separate.

Run from the repository root:

~~~powershell
python -B -m unittest discover -s tools/verification/execution_acceptance -p "test_*.py" -v
python -B tools/verification/execution_acceptance/audit_layout.py --build out/build/r4/pre-o1/msvc-on-debug --output out/evidence/r4/layout-new
python -B -m tools.verification.execution_acceptance.budgets --layout tools/verification/execution_acceptance/element-sizes-msvc-release.v1.json --output out/evidence/r4/budget-new.json
~~~

The layout probe compiles a sizeof-only executable with MSVC Release definitions. Its generated
headers come from the explicit configured build. Output directories must be new. The recorded
layout hashes project headers and the source of the frame/attempt accounting formulas; a different
ABI needs a new audit.

Budget coefficients are accepted reference design ceilings, not measured peaks or proven whole-heap bounds.
Numerical acceptance alone cannot authorize a run with missing technical admission. GPU allocations, CPU PCM,
fixture/observer memory, requested bytes, usable storage, and process commit/working set have
separate meanings.

The acceptance matrix runner now supports --build-root. For example, add
--build-root out/build/r4/preflight to keep each invocation in a fresh directory. --output alone
still selects only the evidence directory. Existing build trees and paths outside out/build are
rejected in isolated mode.

`binding.py` binds clean Git sources and file bytes. `admission.py` validates evidence and writes
one-time claims/seals. Attempt limits use canonical workload keys in the execution authorization's
fixed ledger, including the one-invocation P8 limit. A partial failed claim stays visible.
`runner.py` currently implements normal owner/common-host fixed groups only. It refuses other
adapters before claiming a run, preserves complete process statistics and absolute gates, and never
replaces failed samples. `results.py` reuses the frozen statistical functions.

`build.py` creates fresh explicit clean-source MSVC builds with English compiler output and binds
the whole target's Ninja object/header dependency inventory. `prepare_cpu.py` predeclares a single
four-process owner pilot group, fixes read operations at 1048576, and emits preparation only.
It never claims a formal attempt or adapts read counts after observing results.
`runtime_monitor.py` samples processes, foreground, AC and power scheme once per second while
each child runs; it records gaps and parent CPU overhead. It may miss sub-interval activity and
does not certify GPU/audio conditions.

`memory_domains.py` validates extended observer rows and simultaneous allocation aggregates.
Its domain mapping comes from the same internal `.def` table as the C++ observer. Current allocation
origin tags do not yet close semantic ownership/lifetime, native/GPU and reference overhead gates;
its result remains partial/unrated and cannot admit formal memory or P8 runs.

The extended collector requires CUEXIS_ALLOCATION_LIFETIME_OBSERVER=ON and
CUEXIS_EXECUTION_ALLOCATION_OBSERVER=ON in a separate Windows STATIC test build.
It leaves the original observer and log prefixes unchanged in historical mode.

`semantic_memory.py` validates a deliberately incomplete World lower bound. The extended
collector v2 adds an allocation ID (64-byte header versus v1's 56), a fixed 256-root table,
and bounded, allocation-free census storage. Narrow scopes capture only registry storage
that cannot migrate into runtime candidates. Appearance/Camera nested strings, transform
scratch, World object/member initialization and unvisited containers remain uncharged unknowns.
Actual Appearance/Camera/WorldTransform page spans are matched synchronously inside live
backing new allocations at prepared, runtime-installed, and Playback-active instants. MSVC
can return an interior aligned address for a standard allocator payload. Only the interval
union of observed payload bytes contributes to this lower bound; private STL alignment bytes
are excluded. Full backing requested/storage bytes and page offsets are reported separately.
No shared handles
are copied. Root generations and allocation IDs prevent address reuse and duplicate charges;
subset retirement waits for real frees. A released root certifies only the tracked subset.
Scope peaks are origin observations, never persistent-World verdicts. Only an active instant's
unique lower bound above the unchanged ceiling proves failure; all other outcomes remain
unrated. Whole-log validity, exact origin books and final C++ product release are prerequisites.
These proofs neither admit formal memory/P8 nor establish native/GPU or full semantic coverage.

Scope, numbers, owner decisions and formal admission requirements are maintained in
[the R4 plan](../../../docs/stage_plans/active/stage-07/05-rpa-new/r4/plan.md),
[O1 proposal](../../../docs/stage_plans/active/stage-07/05-rpa-new/r4/caps-proposal.md) and
[registration draft](../../../docs/stage_plans/active/stage-07/05-rpa-new/r4/formal-registration.md).

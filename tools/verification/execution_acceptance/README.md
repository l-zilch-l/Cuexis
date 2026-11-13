# Execution acceptance tools

These tools prepare budgets, validate admission and seal fixed execution registrations.
The O1 reference budget was accepted externally on 2026-11-05; the frozen proposal retains
accepted=null. Admission verifies the separate owner record and exact accepted artifact hash.
Historical PD-5 collectors and their records remain separate.

Run from the repository root:

~~~powershell
python -B -m unittest discover -s tools/verification/execution_acceptance -t . -p "test_*.py" -v
python -B tools/verification/execution_acceptance/audit_layout.py --build <explicit-configured-build> --output out/evidence/r4/layout-new
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
--build-root out/build/r4/fresh-invocation to keep each invocation in a fresh directory. --output
alone still selects only the evidence directory. Existing build trees and paths outside out/build
are rejected in isolated mode.

`binding.py` binds clean Git sources and file bytes. `admission.py` validates evidence;
`formalize.py` claims and seals an admitted preparation without launching a workload. Preparations
declare `formal_registration: false`; formal execution requires the separate claim/seal step.
Claims use the execution authorization's fixed ledger. Default allowances are two attempts per canonical unit and one for
P8; a bound owner waiver may widen only its named keys. Every further claim needs a disposition
bound to the previous claim and a verified remedy. Failed or partial claims remain consumed.

Backend identity is separate from build instrumentation. `backend: cpu` (also the default for old
records) preserves historical keys such as `p4-capture-1-release`; `backend: device` uses
`p4-capture-1-device-release`. CPU and device attempts are independent, and changing a source SHA,
binary or display name does not replenish either allowance. Unknown backends are rejected.

`runner.py` executes one sealed normal fixed group using the owner, common-host, matrix probe,
controls, stream or Seek adapter. It rejects unsupported adapters and instrumented modes before
launch, retains the three-window process statistics and applicable absolute gates, and never
replaces failed samples. Capacity qualification uses its separate single-invocation collector. `results.py`
reuses the frozen statistical functions; a valid log or successful pilot is not a passing gate.

`probe_units.py` states the original PD-5 presentation matrix once: 14400 real-time frames at
240 Hz after a 2400-frame warmup, the per-mode consumer count, the 8:1 update ratio and the 256-frame
topology reload interval. `prepare_probe.py` runs one bounded pilot process per selected unit,
verifies the pre-declared counters, identity, environment and runtime policy, and emits preparation
and proofs only. The probe units are B-only: the pre-PD tree has no matching harness, so A stays
N/A instead of being filled with zero. `prepare_probe.py --device` registers the explicitly selected
`p4-capture-1`, `p4-capture-2` or `p4-capture-4` units against
`cuexis_execution_device_probe.exe`. Their identity is `renderer-matrix`, with GPU enabled, one
renderer in addition to the declared capture consumers, and separate renderer-delivery,
CPU-submit and present-call counts. CPU-only modes are rejected on this executable. A device P4
unit is not the standalone P3 renderer workload.

`prepare_stream.py` supplies P3's independent `stream` adapter and
`p3-renderer-device-release` identity. One bounded pilot uses the Release normal device executable
and the fixed one-entity `gpu-stream-v1` fixture: 14400 frames at 240 Hz after 2400 warmup frames,
with GPU timers off. It binds the exact digest, all counters, request-to-ready,
request-to-present-return, CPU-submit, present-call and lifecycle endpoints. There are 57602 native
samples plus the parser's process-start-to-first-frame `load` sample. The resulting preparation
still needs claim/seal and the normal three-window group; it admits neither GPU time nor memory,
audio, P7 or a larger capacity profile. `prepare_controls.py` similarly prepares the separate P5
scripted-control workload, including its 73 samples per process; controls do not borrow matrix
counts or a pre-PD A side.

`environment_stability.py` holds `SetThreadExecutionState` display/system requests for a bounded
interval and samples foreground, AC, power scheme and conflicting processes once per second. It is
remedy evidence, not a workload: it does not simulate input, so a secure screen saver or a manual
lock still fails the record. `prepare_probe.py --retry-remedy <artifact>` then writes one
`rpa.execution.retry-disposition.v1` per unit that binds the previous claim and the verified remedy;
without a verified remedy a second registration is refused.

`build.py` creates fresh explicit clean-source MSVC STATIC candidate builds with English compiler
output and binds the whole target's Ninja object/header dependency inventory. Select the build
mode explicitly; the Release collectors also verify the executable name, configuration, source,
compiler and dependency baseline.

| `--mode` | Instrumentation selected by `build.py` | Evidence use |
| --- | --- | --- |
| `normal` | All bound instrumentation options OFF | Normal timing and CPU/device pilots or qualification controls |
| `memory` | `CUEXIS_ALLOCATION_LIFETIME_OBSERVER` and `CUEXIS_EXECUTION_ALLOCATION_OBSERVER` ON | Separate allocation/lifecycle evidence |
| `stage-timing` | Only `CUEXIS_PD_METRICS_TEST_ONLY` ON | Dedicated stage observer processes |
| `player-trace` | Only `CUEXIS_PLAYBACK_TRACE_HARNESS` ON | Windows scripted Player functional evidence |

`build_binding.py` checks the bound cache, compiler commands and dependency closure. Normal
evidence must not borrow stage, memory or Player-trace binaries. Runtime GPU-query enablement
also distinguishes observer processes from normal timing even when they share a normal binary.
`prepare_cpu.py` predeclares one four-process owner pilot group, fixes read operations at 1048576,
and emits preparation only. It never claims a formal attempt or adapts counts after observing
results.
`runtime_monitor.py` samples processes, foreground, AC and power scheme once per second while
each child runs; it records gaps and parent CPU overhead. It may miss sub-interval activity and
does not certify GPU/audio conditions.

`stream_observers.py` runs three fixed independent sets in normal/GPU/stage/memory order: twelve
stream sample processes, each preceded by machine warmup. It compares workload, digest, common
counts and result after removing declared observer fields and the extra GPU-query count.
GPU and stage admission require at most 15% overhead in
each set's request-to-ready, present-return and CPU-submit medians. GPU queries must cover all
14400 measured frames and the final drain; stage records cover judgement, projection, snapshot and
queue. Failed overhead leaves those budget gates `observer_unvalidated`. Memory cost is disclosed
separately. This is observer qualification (`formal_sampling: false`, `formal_acceptance: false`),
not the normal three-window group; observer cost is never subtracted from normal timing.

`memory_domains.py` validates extended observer rows and simultaneous allocation aggregates. Its
domain mapping uses the C++ observer's internal `.def` table. `memory_attribution.py` adds the
semantic partition comparisons. Origin-only ratings remain partial/unrated: exact C++ allocation
totals alone do not establish semantic lifetime, GPU/driver coverage or full budget acceptance.
The separate memory build leaves historical observer log prefixes unchanged in historical mode.

`capacity_evidence.py --purpose observer-qualification` collects one declared n64 or n2048
low/high process in normal or memory mode, on the CPU or `--device` backend. `prepare_capacity.py`
requires three normal/memory qualification pairs in six independent invocation directories, at
one matching small scale/reuse/backend, with no phase holds. It rechecks native process anchors,
source/build/toolchain/environment bindings, exact packed/Snapshot hashes and semantic results,
lifecycle checkpoint counters and finite observer range, and discloses the wall-cost ratios.

Current memory/P8 admission must fail closed: the complete ownership/reference/lifetime graph
is still missing. `prepare_capacity.py` must write qualification results and a blocked disposition
only, without generating a formal preparation or automatically asserting a passed semantic proof.
Its output cannot be used with `formalize.py`; no new memory/P8 claim or seal is admissible at this
stage. Equal packed/Snapshot output, exact allocation-origin books, final C++ release counters and
a World lower bound do not replace the missing graph. The collector's `sealed-memory-evidence`
entrypoint cannot bypass this prerequisite.

Qualification remains limited to n64/n2048. P8 retains the accepted `(40000, 4096)` content identity,
but cannot be run as an unclaimed qualification workload. The low variant decorrelates requirement
end ticks; it is not a general low-reuse or cold-cache upper bound. Existing evidence and attempt
records retain their original identities and do not establish current admission.

Capacity evidence compares content prepare, host-load prepare, active, close and final fixture
release checkpoints. It records requested C++ bytes, usable storage, process guards, concurrent
prepare peaks and observer header/static/checkpoint storage separately. Device evidence also
requires logical GPU allocation checkpoints through device release and bounded 500..5000 ms phase
holds for independently sampled process GPU residency. Logical GL accounting covers buffers and
RGBA8 textures; process Dedicated+Shared residency uses the same process's initialized empty
renderer baseline. These are separate views and are not added together. Missing counters or
incomplete sampled coverage remain unavailable, never measured zero. Summaries retain
`formal_acceptance: false`; over-cap results and missing domains must be reported explicitly.

`semantic_memory.py` validates a deliberately incomplete World lower bound. The extended
collector v2 adds an allocation ID (64-byte header versus v1's 56), a fixed 256-root table,
and bounded, allocation-free census storage. Narrow scopes capture only registry storage
that cannot migrate into runtime candidates. The typed census also observes actual current heap strings, all seven nonempty component
payload types, packed entity size ranges, World object, transform cache/children/order capacities.
Inline strings are excluded, and moving a heap string changes its instantaneous owner rather
than permanently attaching its allocation to the old root. Private EnTT backing, transform
scratch/transactions and unvisited containers remain unknown. Observed spans are matched synchronously inside live
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
This lower-bound proof cannot establish native/GPU/full semantic coverage. Combining it with
matching output hashes or qualification checkpoints still cannot admit formal memory/P8 while
the ownership/reference/lifetime graph is missing; capacity qualification remains blocked.

`player_evidence.py --kind music|dfjk` collects and validates one scripted Release Player-trace
process with its bound bundle, settings, script, inventory and actual opened audio endpoint. The
current inventory check requires the active Senary system-default endpoint. Music runs to its
explicit 130-second Close and must observe the complete source and audio EOF. DFJK preserves the
bound 38 input transitions, terminal execution and score/hit/miss oracle before its explicit Close.
The parser checks native startup anchors, frame counts, audio observations and replay evidence,
and separates active trajectory duration from the terminal wait; EOF frozen redraws cannot extend
steady execution evidence. Native Player focus checks accompany sampled AC/power/conflict checks.

This P7 collector is available, but its output is single-process functional evidence:
`formal_registration: false`, `acceptance: false`, `formal_attempts_consumed: 0`, with no automatic
retry. It does not create a formal timing registration, certify 600-second retention, listening,
physical input, calibration or production acceptance. Failed invocations retain `stopped.json` and
their original logs. Tool availability, a preparation, or a valid functional record must not be
reported as completed R4/device/production acceptance; current delivery status remains in
[CURRENT_STATUS](../../../docs/CURRENT_STATUS.md).

Scope, numbers, owner decisions and formal admission requirements are maintained in
[the R4 plan](../../../docs/stage_plans/active/stage-07/05-rpa-new/r4/plan.md),
[O1 proposal](../../../docs/stage_plans/active/stage-07/05-rpa-new/r4/caps-proposal.md) and
[registration draft](../../../docs/stage_plans/active/stage-07/05-rpa-new/r4/formal-registration.md).

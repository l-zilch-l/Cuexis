# Stage 7-PD tests and measurement probes

PD-specific fixtures, tests and measurement probes belong in this directory. They use independent
CMake targets and the `pd` test label; they are not RPA acceptance tests.

The static candidate target `cuexis_pd_baseline_probe` contains the preserved legacy
`PD deterministic presentation baseline` trajectory and separate fixed legacy/RPA workloads.
It imports the existing RPA test content builder as a dependency; that reuse does not make the
legacy trajectory an RPA workload.
The original saved executables and logs remain historical evidence at their original paths.

`formal_baseline.py` is the separate Windows uninstrumented baseline entrypoint. It requires
Debug/Release STATIC candidate builds in `out/build/pd0-formal-{debug,release}` with
`CUEXIS_PD_METRICS_TEST_ONLY=OFF`, checks actual compiler commands, and uses the normal allocator.
It also requires nonempty Ninja header-dependency records for both measurement objects, including
`measurement_environment.hpp`, and rejects an object older than its source/header inputs or an
executable older than its measurement object. A frozen executable hash alone does not establish
that the current measurement sources were built.
Its hidden fixed trajectory includes per-step checks and FrameDigest; it is not pure Playback cost.
The finite pilot compares two topology-verified physical cores, then freezes counts and affinity.
Formal sampling uses six independent processes per configuration/profile across two windows,
three per window, with at least ten minutes between windows. Both windows and their combined
six-process range must satisfy the unchanged 15% limit. Machine CPU warmup and fresh-session
warmup are distinct. No recording A/B or ETW is used. Host defaults to the inherited CPU set,
uses independent below-capacity trajectories and separately reports mean repetition and tails.
Unknown tail budgets, OEM conditions, thread-query failures and missing quantiles are explicit.
See `docs/stage_reports/stages/stage-07/pd/2026-10-17-pd0-uninstrumented-baseline.md`.

On the recorded Chinese MSVC installation, a console input code page of 936 and output code page
of 65001 produced a misdecoded `msvc_deps_prefix` and zero recorded header dependencies. Matching
both console code pages to UTF-8 during fresh configure and clean build restored dependency capture.
The local evidence recipe is `out/pd0-controlled-environment-20261018/fresh_build_consistent.py`;
it restores the console settings afterward. `VSLANG=1033` alone did not help because only Chinese
compiler language resources were installed. This is a build-binding issue; the cause of historical
timing variation remains unproven.

The completed controlled-environment registration, same-source characterization, frozen inputs
and PD-0 exit evidence are recorded in
`docs/stage_reports/stages/stage-07/pd/2026-10-18-pd0-controlled-environment-and-build-binding.md`.
Historical failed groups remain unchanged; PD-1 through PD-5 and production gates remain open.

```powershell
python -B tests/pd/formal_baseline_tests.py
python -B tests/pd/formal_baseline.py pilot
python -B tests/pd/formal_baseline.py register --pilot <pilot-directory>/manifest.json
python -B tests/pd/formal_baseline.py window --registration <registration.json>
# Run the same window command once more after the registered ten-minute minimum gap.
```

Registration consumption is tracked; execution or identity errors stop sampling, and failed
performance groups retain all samples rather than accepting replacement runs. Historical
instrumented tools below retain their original purpose and evidence.

`diagnose_core_contention.py` is a separate Windows diagnostic, with no formal acceptance.
It runs eight fixed Debug/RPA processes in the registered order of one logical CPU and both
logical CPUs on that same physical core. Affinity permits placement; it does not reserve a core.
English PDH counters collect all logical CPUs once per second, recording busy time, reported
performance/frequency, DPC and interrupt time. Its observer overhead is uncalibrated; results
cannot replace a formal window or identify a historical cause that did not reproduce.
The current implementation supports a single processor group and requires an SMT sibling.

```powershell
python -B tests/pd/diagnose_core_contention.py check
python -B tests/pd/diagnose_core_contention.py run --registration <formal-registration.json>
```

The fixed-core formal result and bounded diagnostic are recorded in
`docs/stage_reports/stages/stage-07/pd/2026-10-18-pd0-core-policy-and-contention.md`.

`capture_formal_runtime.py` prepares one CPU/Power ETW diagnostic against the frozen formal
probe binaries, independent of the historical instrumented collector's deleted calibration.
Preparation exports and hashes the WPR profile and checks its scheduling/power keywords.
Capture requires an administrator terminal, uses unique named sessions and an exclusive claim,
and saves an owned recording even when the workload fails. Seven ownership/cleanup tests are
available in `capture_formal_runtime_tests.py`. Actual privileged capture remains unverified.

```powershell
python -B tests/pd/capture_formal_runtime_tests.py
python -B tests/pd/capture_formal_runtime.py prepare --formal-registration <formal-registration.json>
# In an administrator terminal, using the diagnostic registration emitted above:
python -B tests/pd/capture_formal_runtime.py capture --registration <diagnostic-registration.json>
```

The three fixed diagnostic processes are Debug/RPA, Release/RPA and Debug/legacy, each with
eight blocks per metric and the frozen affinity/read counts. ETW overhead is uncalibrated.
Collection alone proves neither a historical cause nor formal repeatability; inspect event loss,
QPC coverage and whether slow blocks actually reproduced before making an attribution.

The owner-requested three-round supplement uses the same frozen binaries and counts, with
three processes per cell per round and at least five minutes between completed rounds.
Its registration, results and archive inventory are documented in
`docs/stage_reports/stages/stage-07/pd/2026-10-17-pd0-supplemental-retests-and-archive.md`.
The one-off runner is preserved with that evidence, rather than added as another permanent tool.
Historical instrumented collectors below remain reproducibility/diagnostic entrypoints;
their recording comparisons do not replace the uninstrumented performance baseline.

`cuexis_pd_allocation_probe` uses `pd_allocation_probe.hpp` only in its own static executable;
the timing baseline retains the normal allocator. Requested C++ bytes/counts and live high-water
observations exclude malloc/CRT/driver/GPU storage. Allocation-probe timings are not performance
evidence. `collect_storage.py` registers independent repetitions and coverage checks.

`delivery_baseline_tests.cpp` characterizes the old ledger capacity fault and the portable CPU
test renderer. The fault is an expected defect baseline, not a successful bounded-lifetime gate.

`tools/verification/measure_pd0_method.py` separately registers calibration and acceptance. It uses fixed fresh
session trajectories, outer block timing, read-only Snapshot blocks, controlled process affinity
and active/dormant runtime recording pairs. Single-call timings remain diagnostic. Both workloads
validate complete Replay/Snapshot bytes and FrameDigest exactly. See
`docs/stage_reports/stages/stage-07/pd/2026-10-17-measurement-method-history.md` for evidence.

`measure_scheduling.py` retains the historical instrumented Windows calibration and validation
(current output uses the v3 QPC schema). It keeps
fresh fixed trajectories, compares recording in balanced ABBA/BAAB quartets, and freezes a
long read-only batch from pilot data. The original 4096-read blocks remain a separate diagnostic
profile. Wall time is the primary metric; thread CPU time and raw cycle counts are diagnostics.
CPU accounting units are not its observed resolution, and cycles must not be converted to time.

```powershell
python -B tests/pd/measurement_statistics_tests.py
python -B tests/pd/measure_scheduling.py calibrate
python -B tests/pd/measure_scheduling.py validate --calibration <calibration-directory>/manifest.json
```

The runner requires the existing Debug/Release static candidate probes with PD metrics enabled.
Each invocation prints its evidence directory. Validation requires unchanged calibration inputs;
it retains all samples, keeps the 15% absolute wall-noise gate, and does not retry failed groups.
See `docs/stage_reports/stages/stage-07/pd/2026-10-17-measurement-method-history.md`
for results, alternatives and limits. Statistics checks cover drift, known effects, incomplete
quartets and retention of a slow sample.

`cuexis_pd_host_probe` builds the existing application ExecutionHost and measures its actual
sample -> CPU renderer -> worker receipt acknowledgement route. It compares every returned
frame against a direct PlaybackSession reference. This separate profile has no Gameplay,
audio or GPU; its 32 warm-up and 512 measured frames remain below the declared old ledger
capacity of 1024. It does not claim bounded steady-state behavior.

```powershell
python -B tests/pd/collect_host_delivery.py calibrate
python -B tests/pd/collect_host_delivery.py accept --calibration <calibration-directory>/manifest.json
```

The collector preserves individual latencies and gates three independent process means,
retaining all long delays. Its first acceptance group was inconclusive; do not retry it until
passing or substitute a median for its preregistered mean. The source and proposed follow-up
are recorded in
`docs/stage_reports/stages/stage-07/pd/2026-10-17-measurement-method-history.md`.

`compare_host_affinity.py` preregisters one 24-process diagnostic contrasting the inherited
allowed CPU mask with its lowest single bit. It rotates configuration/order, keeps all samples,
and reuses the unchanged host binary. Neither profile passed the first complete comparison;
the diagnostic does not automatically accept a baseline. `collect_host_delivery.py` also accepts
`--affinity default` for a future separately calibrated/registered group; the compatibility
default remains `single`. Calibration and acceptance must use the same affinity.

`owner_wait_probe.cpp` characterizes a specific existing channel/caller mismatch: a reply
completed after poll but before wait does not satisfy the command-oriented wait predicate.
`python -B tests/pd/reproduce_owner_wait.py` preregisters 12 processes across Debug/Release
and default/single affinity. Latches and join force the interleaving without sleeps. It checks
0/1/20/100 ms waits, pending-command and in-flight controls, reply preservation and ACK.
The original v1 hidden diagnostic expected the old behavior. The current v2 uses target reply
waits for in-flight/completed cases and checks that a completed reply returns promptly;
its old evidence remains historical. See
`docs/stage_reports/stages/stage-07/rpa/2026-10-17-retention-progress-and-wait-boundaries.md`.

`owner_channel_wait_tests.cpp` adds ordinary `pd` CTest cases for reply readiness, queued/unrelated
jobs, worker command waiting, retained replies and shutdown errors. Reply publication requires nothrow-movable reply types.
`advance_retirement_tests.cpp` and `coherent_retirement_tests.cpp` cover bounded request retirement,
retained holes, duplicate ACK, old-ID rejection before clock sampling and SDK Seek identity transfer.
The worker reads scoped retention metadata; it does not query complete results to reclaim identities.
These are independent defect regressions preceding the new PD-0 baseline. Current evidence belongs to
`docs/CURRENT_STATUS.md`; the dated design comparison is
`docs/stage_reports/stages/stage-07/rpa/2026-10-17-wait-and-retirement-design-review.md`.

These measurement cases are hidden Catch cases and run by explicit name through the runner;
they are not automatically registered as CTest `pd` cases. Method validation alone does not close
PD-0. The ordinary ledger/renderer cases are discoverable. The earlier B5 Pause failure remains
unexplained; subsequent unchanged-code retests passed, as recorded in
`docs/stage_reports/stages/stage-07/pd/2026-10-17-measurement-method-history.md`.

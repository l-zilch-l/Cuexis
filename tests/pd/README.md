# Stage 7-PD tests and measurement probes

PD-specific fixtures, tests and measurement probes belong in this directory. Measurement probes
use independent CMake targets. Publication/projection tests compile in `cuexis_playback_tests`
for both static and shared candidate builds and use the `[pd]` Catch2 tag. They are not RPA
acceptance tests.

PD-3-b `presentation_read_tests.hpp` runs in the Runtime transaction translation unit so it
uses the same allocator probe. It covers true const overloads, bounded owner read windows,
stale versions/layouts, reentry, conservative publication after mutable callback failures,
all Runtime writer revisions, checked exhaustion and zero-allocation warmed neutral reads.
`presentation_state_tests.cpp` covers initial/owning/reusable snapshots, lifetime consumption,
camera materialization failure, exact current versions, real source absence, GameplayOnly,
Fold metadata, Reset and pure-control/query behavior. Private Playback descriptor tests run in
static builds; shared builds exercise the public materializer without exporting a test bridge.
`post_install_extraction_tests.hpp` also asserts the public allocation-error code and operation
context through the new materializer. Shared allocator-injection cases explicitly skip because
the executable allocator does not intercept allocations inside the Playback DLL.

`world_transaction_tests.cpp` and `runtime_transaction_tests.cpp` compile into the existing
World and Runtime test executables, including candidate OFF builds. They cover prepared
component storage/topology, actual EnTT group rejection, derived matrix overflow, unchanged
committed caches/debug/frame/lifetimes on failure, first update, reload candidate sampling,
and warmed standalone allocation reuse. `transaction_state.hpp` observes private cache state;
the noninstalled Runtime test bridge samples the same candidate evaluator as reload.
The allocation injector excludes the MSVC Debug iterator-proxy size class because the STL
allocates it in noexcept constructors; Release sweeps all allocations in these operations.
The candidate-sampling case remains a focused boundary test. The additional public
resource-backed reload case sweeps preparation allocations through the real reload entry,
checks old state/entity mapping/resource counts, and retries normally. Populated ResourceScope
move construction/assignment must allocate nothing; its container storage is prepared before
noexcept ownership transfer.

`software_transaction_integration_tests.cpp` tests a real sealed/Fold prefix whose finite
parent/child local scales overflow the derived world matrix. It checks old Snapshot/projection
state and same-prefix retry against a clean control and Replay. `post_install_extraction_tests.hpp`
reuses the Playback allocation executable: delivery failures preserve the installed digest and
RemainingFrames lifetime. Release static builds inject all reusable extraction allocation points;
Debug uses a deterministic viewport failure and shared builds explicitly skip executable-only
allocator injection. These tests do not establish PD-3-b revisions or a new materializer.

```powershell
.\out\build\pd3a-debug\bin\cuexis_world_tests.exe "[pd][transaction]"
.\out\build\pd3a-debug\bin\cuexis_runtime_tests.exe "[pd][transaction]"
.\out\build\pd3a-debug\bin\cuexis_playback_tests.exe "[pd][transaction]"
.\out\build\pd3a-debug\bin\cuexis_playback_allocation_tests.exe "[pd][transaction]"
```

`projection_adapter_tests.cpp` exercises the private adapter with an explicit publication reader:
full identity and timing lookup, ordinal consumption, future/expiry without new Facts, group memory,
partial/all-missing targets, stable rejection, stale prefixes/scopes and unknown required records.
`projection_integration_tests.cpp` uses the actual Playback/Runtime path for failure/retry,
source-map alias lifetimes, scope reconstruction and existing prepare conflict rejection.
The private adapter implementation is also compiled into the test executable, so these unit tests
do not require new shared-library exports. Public integration tests still exercise Playback.

```powershell
.\out\build\pd2-debug\bin\cuexis_playback_tests.exe "[pd][projection]"
```

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
See `docs/stage_reports/stages/stage-07/04-pd/pd-0/2026-10-17-pd0-uninstrumented-baseline.md`.

On the recorded Chinese MSVC installation, a console input code page of 936 and output code page
of 65001 produced a misdecoded `msvc_deps_prefix` and zero recorded header dependencies. Matching
both console code pages to UTF-8 during fresh configure and clean build restored dependency capture.
The local evidence recipe is `out/pd0-controlled-environment-20261018/fresh_build_consistent.py`;
it restores the console settings afterward. `VSLANG=1033` alone did not help because only Chinese
compiler language resources were installed. This is a build-binding issue; the cause of historical
timing variation remains unproven.

The completed controlled-environment registration, same-source characterization, frozen inputs
and PD-0 exit evidence are recorded in
`docs/stage_reports/stages/stage-07/04-pd/pd-0/2026-10-18-pd0-controlled-environment-and-build-binding.md`.
Historical failed groups remain unchanged. Current batch status is owned by
`docs/CURRENT_STATUS.md`; later implementation checks do not replace this frozen PD-0 evidence.

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
`docs/stage_reports/stages/stage-07/04-pd/pd-0/2026-10-18-pd0-core-policy-and-contention.md`.

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
`docs/stage_reports/stages/stage-07/04-pd/pd-0/2026-10-17-pd0-supplemental-retests-and-archive.md`.
The one-off runner is preserved with that evidence, rather than added as another permanent tool.
Historical instrumented collectors below remain reproducibility/diagnostic entrypoints;
their recording comparisons do not replace the uninstrumented performance baseline.

`cuexis_pd_allocation_probe` uses `pd_allocation_probe.hpp` only in its own static executable;
the timing baseline retains the normal allocator. Requested C++ bytes/counts and live high-water
observations exclude malloc/CRT/driver/GPU storage. Allocation-probe timings are not performance
evidence. `collect_storage.py` registers independent repetitions and coverage checks.

`delivery_baseline_tests.cpp` characterizes the old ledger capacity fault and the portable CPU
test renderer. The fault is an expected defect baseline, not a successful bounded-lifetime gate.

`tools/verification/playback_performance/baseline_method.py` separately registers calibration and acceptance. It uses fixed fresh
session trajectories, outer block timing, read-only Snapshot blocks, controlled process affinity
and active/dormant runtime recording pairs. Single-call timings remain diagnostic. Both workloads
validate complete Replay/Snapshot bytes and FrameDigest exactly. See
`docs/stage_reports/stages/stage-07/04-pd/pd-0/2026-10-17-measurement-method-history.md` for evidence.

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
See `docs/stage_reports/stages/stage-07/04-pd/pd-0/2026-10-17-measurement-method-history.md`
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
`docs/stage_reports/stages/stage-07/04-pd/pd-0/2026-10-17-measurement-method-history.md`.

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
`docs/stage_reports/stages/stage-07/03-rpa-legacy/performance/2026-10-17-retention-progress-and-wait-boundaries.md`.

`owner_channel_wait_tests.cpp` adds ordinary `pd` CTest cases for reply readiness, queued/unrelated
jobs, worker command waiting, retained replies and shutdown errors. Reply publication requires nothrow-movable reply types.
`advance_retirement_tests.cpp` and `coherent_retirement_tests.cpp` cover bounded request retirement,
retained holes, duplicate ACK, old-ID rejection before clock sampling and SDK Seek identity transfer.
The worker reads scoped retention metadata; it does not query complete results to reclaim identities.
These are independent defect regressions preceding the new PD-0 baseline. Current evidence belongs to
`docs/CURRENT_STATUS.md`; the dated design comparison is
`docs/stage_reports/stages/stage-07/03-rpa-legacy/performance/2026-10-17-wait-and-retirement-design-review.md`.

These measurement cases are hidden Catch cases and run by explicit name through the runner;
they are not automatically registered as CTest `pd` cases. Method validation alone does not close
PD-0. The ordinary ledger/renderer cases are discoverable. The earlier B5 Pause failure remains
unexplained; subsequent unchanged-code retests passed, as recorded in
`docs/stage_reports/stages/stage-07/04-pd/pd-0/2026-10-17-measurement-method-history.md`.


PD-3-c source/control regressions live in `control_binding_tests.cpp` (existing protocol
executable), `retained_control_tests.cpp` (actual ExecutionHost executable), and the public
source query case in `presentation_state_tests.cpp`. They cover complete confirmed rebinding,
late receipts, source-time frontier preservation, older successful snapshots, resize from retained
values, once-only attempts, no retained source, Resume/EOF, Seek revocation and renderer failure.
They do not register consumers, reclaim the frame ledger or certify physical devices/PD-5 timing.
The public query was introduced in SDK 0.8.1; the current package requires SDK 0.9.0 after the PD-4 expiry migration. Original PD-0 evidence remains historical.

## PD-4 delivery and retirement

`delivery_retirement_tests.cpp` covers CPU and attempt leases, activation/revocation frontiers,
Ready/take/cancellation/deadline arbitration, independent capture consumers, compact history,
optional expiry, bounded coverage and late receipt handling. `retained_control_tests.cpp` also
runs 5,000 actual host updates/presents with a four-attempt journal, at least 35 seconds of logical
source and Replay equivalence; the numbers are testOnly limits, not production budgets.
The allocation-only probe verifies owning error-tree bytes with exact C++ requested allocations.

The frozen pre-PD4 ledger lives only in `legacy_frame_observation.*`; its 4097 failure expectation
remains a historical baseline. Actual Player uses the new registered delivery path.
The existing host probe selector is retained, but PD4 emits `pd4.present-return.v1` and asynchronous
receipt mode. Its intervals end at present return / receipt enqueue, not Worker acknowledgement.
Historical PD0 collectors explicitly reject that schema; old binary evidence and the 15% gate
remain unchanged. PD4 results do not authorize PD5 scheduling or formal performance acceptance.

## PD-5 tool preparation

Measurement implementations and profiles live under
[tools/verification/playback_performance](../../tools/verification/playback_performance/README.md). Python tests and C++ probes remain
here. Old Python paths are thin compatibility entries; new imports and commands use the tool
directory. Historical archives retain their original paths and inputs.

The owner accepted the local reference scope, thresholds and three-window protocol on
2026-10-19. This does not register formal samples. [measurement.py](../../tools/verification/playback_performance/measurement.py) parses the preserved
owner, old ACK, current asynchronous host and scripted PD5 schemas without renaming completion
endpoints. It provides independent-process pair intervals, the unchanged 15% noise checks,
sample-count guards and explicit unvalidated-observer dispositions.

`pd5_host_probe.cpp` runs a fixed logical source clock with actual ExecutionHost controls,
checks executed horizons, hashes every stream frame outside timing intervals and checks Replay
equivalence and zero CPU payload occupancy after Close. The explicit device executable compiles
the same harness via `pd5_device_probe.cpp`. Its noninstalled GPU timer implementation stays
under `engine/render_opengl/`; query objects are released before context Close/rebuild.
GPU timestamps surround submit, excluding present/VSync. No new SDK export or normal Player
instrumentation is introduced. Hidden timing cases are explicit invocations, not unit/device
acceptance gates.

[profiles.json](../../tools/verification/playback_performance/profiles.json) lists finite CPU/GPU stream/control tool profiles and explicitly lists the
remaining P0-P8 coverage gaps. The native fixture has no audio and is not the real music/DFJK
course. Host-local Load is named `load_host`; it does not establish process-start latency.
The full stream profile declares 10 seconds warmup plus 60 seconds/14,400 measured updates;
the `probe` command intentionally overrides this with a hidden-window short plumbing check.

[prepare_profiles.py](../../tools/verification/playback_performance/prepare_profiles.py) checks all transitive object dependency inventories and source/header/build
metadata freshness, linked object/library hashes, compiler configuration and the complete
repository input inventory. Current B must use a dedicated build tree, never a historical
pd0/pd4 directory. Preparation manifests and run results use unique paths; failed runs remain.
The command refuses drift and never issues a formal window claim or acceptance result.

```powershell
python -B tests/pd/pd5_measurement_tests.py
python -B tests/pd/pd5_prepare_tests.py
python -B tools/verification/playback_performance/prepare_profiles.py prepare --profile cpu-controls-v1 --build out/build/pd5-preparation-release
python -B tools/verification/playback_performance/prepare_profiles.py verify --manifest <preparation-manifest.json>
python -B tools/verification/playback_performance/prepare_profiles.py probe --manifest <preparation-manifest.json> --full-controls
python -B tools/verification/playback_performance/prepare_profiles.py collect --log <preserved.log> --kind host --exit-code 0 --output <new-observation.json>
```

Replace the profile with `cpu-stream-v1`, `gpu-controls-v1` or `gpu-stream-v1` as needed.
`probe` is tool validation only, with eight stream samples or one control cycle; `--full-controls`
validates all ten declared control cycles. Neither mode has a machine warmup/formal window.
GPU observer calibration, full per-domain memory accounting, combined renderer/captures,
audio/DFJK and full runtime RPA-F3 rebinding remain separate readiness items.

`pd5_matrix_probe.cpp` adds seven standalone SDK + FrameDelivery tool profiles for no consumer,
8:1 updates/deliveries, independent 1/2/4 CPU captures and sparse/topology reloads. They have no
Gameplay/Replay, renderer or audio. `matrix_defaults` explicitly overrides inherited host metadata.
Topology reload retires consumers and delivery, increments resource/control generations and
registers the new scope. Independent captures of a revision must have identical FrameDigest.
The separate metrics ON build checks actual Snapshot counts; its times never accept normal budgets.

[owner_runner.py](../../tools/verification/playback_performance/owner_runner.py) provides the P0 owner/common-short-host subset only: separate prepare,
single finite pilot, single registration and three claimed windows. The registration freezes
identities, read counts, affinity and power policy. Before advancing, [window_protocol.py](../../tools/verification/playback_performance/window_protocol.py) rehashes
claims/results/records/raw logs and reparses statistics; it rejects evidence drift, replacement,
incomplete groups or a gap below 600 seconds on either UTC or monotonic time. The three-window
summary also checks the host mean noise and never declares full PD5 completion.

The new [process_control.py](../../tools/verification/playback_performance/process_control.py) starts the native child only after a Windows kill-on-close Job handshake.
Timeout/exception closes the whole tree, and all failure paths retain original output and hashes.
[device_inventory.py](../../tools/verification/playback_performance/device_inventory.py) only reads stable Core Audio default endpoint IDs, current primary
display mode, GPU drivers and existing independent music/DFJK content hashes. It never opens audio
or declares a device profile ready; actual SDL endpoint binding remains pending.

```powershell
python -B -m unittest discover -s tests/pd -p 'pd5*_tests.py' -v
python -B tools/verification/playback_performance/prepare_profiles.py prepare --profile cpu-capture-4-v1 --build out/build/pd5-preparation-release
python -B tools/verification/playback_performance/owner_runner.py prepare --output out/pd5-owner
python -B tools/verification/playback_performance/device_inventory.py --output <new-inventory.json>
```

Prepare validates tooling and identities only. Do not invoke pilot/register/window with a stale
preparation after source/profile/document changes. Finite pilot, formal windows and actual audio
still require the readiness conditions in the accepted proposal. Current evidence and remaining
scope are recorded in `docs/stage_reports/stages/stage-07/04-pd/pd-5/preparation/2026-10-19-pd5-window-and-matrix-preparation.md`.

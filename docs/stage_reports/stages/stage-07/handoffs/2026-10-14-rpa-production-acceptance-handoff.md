# Stage 7-RPA production acceptance and handoff draft

Date: 2026-10-14. Status: preparation; B5/F1-F4 remain open.

## Source and authorization

Repository D:/Cuexis-worktree, stage-7, entry HEAD d744fa78fa626053c31f68f2ada20819632235f6.
Implementation checkpoint d7a20d9; wait optimization 74cee7e. Historical reports describing
these changes as uncommitted are dated snapshots. Current SDK 0.8.0, build 26.10.14-1.
The owner requested RPA continuation and then deferred physical participation for this run.
PD implementation is outside this run. No production profile, public field, wire revision,
ruleset, Replay or clock semantics is changed by this batch.

Canonical entry is [CURRENT_STATUS](../../../../CURRENT_STATUS.md), followed by the
[RPA plan](../../../../stage_plans/active/stage-07/plan-rpa.md),
[execution contract](../../../../formats/rpa-execution-contract.md),
[host boundary](../../../../api/realtime-host-boundary.md),
[ADR 0046](../../../../adr/0046-realtime-playback-coordination.md),
[N3 exit](../diagnostic-formalization/2026-10-10-n3-verification-and-exit.md),
the B3/B4/B5 reports indexed in the Stage 7 report index, and
[this run's evidence](../rpa/2026-10-14-final-matrix-and-budget-progress.md).

## Remaining acceptance gates

| Gate | Concrete missing input | Next action |
| --- | --- | --- |
| B5 physical input | DFJK/Hold/head/body/tail, early release, focus, Pause/Resume, restart/reset, Close | Human operation with actual logs; injected input is separately classified |
| B5 audio | Listening/synchronization, underrun, device switch/loss/recovery | Actual output observations and clock/device segments |
| M-TIME | Observation validity/error and source/device/presented offsets | Measure on physical output before freezing SchedulingProfile |
| M-TAIL | EOF anchor error, final timers/windows, deadline and terminal H | Measure real EOF; silent PCM tail is not EOF |
| M-SPLIT | H/frontier/Fold lag, projection blocking, oldest unconsumed age, queue peaks | Release measurements with the chosen shared Playback worker |
| M-CAP | Input/trace/control/receipt/resource/old-handle peaks and overflow behavior | Long runs, delayed consumers and retirement tests |
| F1 | Final behavior commit and complete row disposition | Match all fresh local rows to that exact source/configuration; preserve failures and skips |
| F2 | Same-SHA Linux Quality/MSVC/MinGW | Prepare reviewable commit and run hosted verification after remote publication is authorized |
| F3 | Actual representative/worst production content, prepare heap peak, host/device Seek latency, accepted targets | Complete five-budget measurement and owner review; do not turn test-only values into defaults |
| F4 | B5/F1-F3 closure and explicit owner acceptance | Review complete handoff only after applicable prerequisite gates |

## Five budget domains

| Domain | This run's available inputs | Still unmeasured or unaccepted |
| --- | --- | --- |
| Content | 1/256 Hold fixtures, phase and Fact counts, preparation time; 1/256 Tap owning host fixture | Production representative/worst contents, activity/Pattern state distributions, prepare heap peak |
| Steady state | 32 empty advances after timer burst, p50/p95/max; host requests, H/frontier/Fold cursors and Close time | Long-history slope, input bursts, real presentation/queue/retention pressure, accepted sustained targets |
| Snapshot/Seek | rev3/schema2 Snapshot bytes, encode/decode/recover, kernel exact Seek and full-result equality | Host/device Seek and checkpoint timings, production maxSeekLatency, simultaneous old handles and heap peaks |
| Replay | rev3 archive bytes, encode/decode/evaluate and full-result equality | Long input/control archives, late/fault prefixes at production scale, accepted record/element/byte limits |
| Packed | Existing host fixture Packed bytes and construction time | Production content profile, separate writer/reader peaks, Stage 8 40k/16 MiB profile evidence |

Thresholds are intentionally unspecified until measurement plus declared targets and owner
acceptance exist. Current production routes refuse unaccepted profiles; existing capacity
failures, non-test codec rejection and reserve/Close behavior remain regression requirements.
The 256-Requirement fixture is a declared synthetic boundary, not a production maximum.

## Typed handoff inputs

- Preserve the 7A results and their original source/configuration identities, golden expectations,
  full Replay/Snapshot/Seek comparison functions and first-fault/sourceCode behavior.
- Preserve v4 gameplay entry mappings and the v5 author -> typed graph -> Packed identity chain.
  The current desktop practice selects compiled/chart.v5.json plus explicit gameplay.packed;
  its empty v4 fallback provides old CXC project closure and is not equivalent playable content.
- Carry Requirement identity/emission path, Ruleset/engine/calibration identity, exact artifact
  SHA and semantic identity independently; Replay must not be migrated by rewriting digests.
- Carry enabled/rejected capabilities and installed static/shared flavor/consumer evidence.
  Gameplay candidate remains explicit; default OFF and unsupported features must reject.
- Carry source/configuration/content hashes, source scope/generation, renderer/resource generation,
  committed Fact prefix, software/device receipt distinctions, Unknown/late/expiry and control cut.
- Retain the deployed ownership boundary: main input/window/GL/coordinator, Playback execution and
  presentation worker, independent audio owner. PD does not authorize a topology replacement.
- Retain unselected 7B+ capability decisions and Stage 8 prerequisites. Neither F3 measurements
  nor an RPA handoff closes Stage 8 or certifies the 40k/16 MiB release profile.

## Migration and rollback

Entry rollback checkpoint is d744fa7. Do not reset hard, clean the worktree, remove out evidence,
or include tools/__pycache__ in a commit. Roll back individual new batches using normal Git
reverts only when needed; keep generated headers tied to fresh configure/clean-first builds.
RPA profile uses Recovery rev3/schema2; the legacy profile remains rev2. Do not silently accept
or rewrite old payloads. SDK 0.8.0 remains a matching-toolchain C++ preview, not stable C ABI.

## Closure disposition

All missing physical observations remain unexecuted. Historical failed device runs stay failed.
No owner acceptance, remote publication, merge, release or Stage 7 closure is implied here.

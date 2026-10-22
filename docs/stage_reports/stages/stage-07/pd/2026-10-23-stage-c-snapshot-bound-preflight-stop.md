# PD-5 Snapshot-bound Stage C preflight stop

Status: stopped before native discovery / low and high not run / Stage C not passed.

Date: 2026-10-23, Asia/Shanghai. Worktree: `D:/Cuexis-pd5-40k-admission`.

## Reviewed source commit

Source commit **`54e8c2a1d770a90e1d7a2728f00510b1b19aabaf`** has parent
`57b604d995f41563652724523d895b36dedf7059`. The committed patch contains exactly
the five reviewed files: the capacity probe, Fold recovery tests, CURRENT_STATUS,
the PD index and the [Snapshot probe repair report](2026-10-23-snapshot-probe-budget-repair.md).
The original review diff and receipt matched; 2,051 source files, 75 receipt records
and 72 repair-protected files were checked before the explicit-path source commit.
Fresh docs and diff checks passed. The already recorded selected regressions were
not rerun, and no full-regression claim is added.

The R4 **TEST ONLY maxElements declaration increased from 20,000,000 to 67,108,864**.
It retains maxBytes=268435456, maxRecords=100000 and testOnly=true; revision 3 keeps
its original declaration. This is a workload declaration increase, not unchanged
budgets or production acceptance. The prior diagnostic and saved wire were preserved
and were not decoded again.

## Fresh build and binding

New build: `out/build/pd5-stage-c-snapshot-release-20261023-54e8c2a/`.
MSVC/Ninja Release STATIC, Chart-v5 candidate ON, lifetime observer ON, metrics,
Playback trace harness and execution diagnostics OFF, with the actual source SHA.
Configure and the 139-step probe build exited zero. Hidden native build processes
used BelowNormal priority and maximum parallelism four. No old build was rebuilt.
The existing environment script and copied showIncludes override remained unchanged.

Before any probe invocation, `source.zip`, `source-inventory.json`, `build-payload.zip`
and `input-binding.json` bound source, libraries, executable, cache, Ninja files,
compile commands and runner inputs. Probe SHA256:
`6722c6e86a0908d6375ad0022b0a503d7b9a99b534b07e60e84f4cfe87b1cbfd`.
Fixture-construction text and both fixed historical R4 artifact hashes matched.

A pre-run setup interruption is retained: the new binder initially read CP936 Ninja
metadata as UTF-8. Its local metadata reader was corrected before an input binding
existed or any 40k process started. No source, build artifact, environment script,
budget or deadline changed, and the build was not repeated. The original helper,
failed build-runner output and `setup-interruption.json` are preserved.

## First gate failure and outcomes

The bounded runner then passed source/binary/archive binding, but its first protected
audit failed with:

```text
AssertionError: out/pd5-stage-c-snapshot-rerun-20261023/agent-events.jsonl
```

This orchestration event log had been included in the broad protected inventory.
Its baseline SHA256 was `ecf77475111c747b7bfedb0e5237a95e68acc9e0bcf5f10f7166cedf94f41caa`.
The after-stop audit at 2026-10-22 23:36:09 UTC recorded
`3a267e1eed8e3084d6433e691e657f576b78448e3cf38a9a2b521ac81d1db335`.
The writer was not independently established. The gate was not waived, the manifest
was not rebased and no native attempt followed the failure.

Across 44 protected roots, that audit found 41,092 of 41,093 files unchanged, one
changed event log, no missing files and no added files. Snapshot diagnostics, old
builds and physical artifacts matched. Source/binary/archive binding passed again.
The main worktree remained at F1 `eed36648a692333e47f798aa45be36ca850d08de`;
its environment script matched the captured hash.

| Profile or requirement | Actual outcome |
| --- | --- |
| Hidden-case discovery | Not reached; protected audit precedes discovery |
| Original 40000 low | Not run; no native process or stage records |
| Original 40000 high | Not run; complete low prerequisite unsatisfied |
| Ordered 24 stages / Snapshot declaration / equality / close | Not exercised |
| Native retries or external probe termination | None; zero owned 40k probes |
| Runner exit | 1; acceptance-state contains an empty process list |

The runner includes the existing `gameplay_admission_evidence.validate()` and the
additional single Snapshot-decode declaration/order/size check, but neither was
exercised by a native run. No low/high success or new codec/lifecycle failure is claimed.

## Evidence and next action

Evidence: `out/pd5-stage-c-snapshot-20261023-54e8c2a/`, including plan, archives,
binding, original setup/build/runner logs, protected inventory, after-stop audit,
acceptance-state, source receipt, final review diff and next-action receipt.
The result is recorded in a separate documentation commit; its hash and final
binding verification are stored in that directory's report-commit receipt.

Parent must review the protected-log drift and distinguish active orchestration
outputs from immutable historical inputs before authorizing a new invocation.
Do not restart this runner or reuse its acceptance directory. No automatic repair,
extra native diagnostics, reduced fixture or saved-wire decode was performed.

Production defaults, Recovery wire, public API/SDK, Gameplay semantics, the seven
Packed physical limits, four hash ceilings 134217728/1000000/4096/8388608,
replyDeadline=30s and 1.10/15% gates remain unchanged. Public stop-interface work
remains suspended, unaccepted and unimplemented. Old window 3, formal sampling and
registration, merge and push remain stopped. The historical 25-minute close stall
remains unexplained; this attempt provides no explicit-close evidence. PD-5, RPA,
production acceptance and Stage 7 closure remain incomplete.

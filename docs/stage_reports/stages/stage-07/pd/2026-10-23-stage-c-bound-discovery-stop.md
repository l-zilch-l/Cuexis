# PD-5 bound Stage C discovery stop

Status: binding and new immutable inventory passed / discovery CLI failed /
native test not started / low and high not run / Stage C not passed.

Date: 2026-10-23, Asia/Shanghai. Worktree: `D:/Cuexis-pd5-40k-admission`.

## Exact source and report binding

The native source remains **`54e8c2a1d770a90e1d7a2728f00510b1b19aabaf`**.
The invocation started from clean report commit
`f30b2c6434f405f9e3cd2f4d2b9e04c65c7e74a6`, whose parent is that source commit.
The difference is exactly CURRENT_STATUS, the PD evidence index and the
[prior preflight-stop report](2026-10-23-stage-c-snapshot-bound-preflight-stop.md).
The binder verifies each of those three exact Git blobs and checkout hashes;
all other tracked source bytes match the original native-source archive.
There is no general report-file exemption.

The existing Release STATIC build was reused without rebuilding:
`out/build/pd5-stage-c-snapshot-release-20261023-54e8c2a/`.
Its original libraries, executable, cache, Ninja metadata and compile commands
match the original binding. The executable contains the source identity, and its
SHA256 is `6722c6e86a0908d6375ad0022b0a503d7b9a99b534b07e60e84f4cfe87b1cbfd`.
Fixture construction and both fixed low/high R4 artifacts match.

The R4 **TEST ONLY maxElements increased from 20,000,000 to 67,108,864** in the
already committed probe repair. maxBytes=268435456, maxRecords=100000,
testOnly=true and the revision 3 declaration remain unchanged. This invocation
does not dynamically validate that declaration because the test never started.

## Historical evidence and preparation attempts

New independent evidence directory:
`out/pd5-stage-c-bound-20261023-f30b2c6/`.
All historical directories, inventories, failures and packages were read-only.

1. The first new prepare stopped before binding on a trailing empty item from
   `git ls-files -z`. The original binder, transcribed tool traceback and timed
   setup receipt were retained. Parent authorized only filtering that empty item.
2. The second prepare stopped before binding because the retired scheduler had
   generated two exit products after the earlier historical audit. Parent
   authorized their exact paths, hashes and sizes, without changing the original
   inventory or accepting any other addition. The failed prepare stderr remains.
3. The third prepare passed exact source/report, build/archive and new immutable
   inventory checks. Parent independently reran `bind.py check` and `bind.py frozen`
   before authorizing the one runner invocation. The acceptance runner was copied
   byte-for-byte from the prior invocation and bound before execution.

The original **41,093-file gate remains FAILED**: 41,092 original files still match,
and only `out/pd5-stage-c-snapshot-rerun-20261023/agent-events.jsonl` differs.
Its original hash is `ecf77475111c747b7bfedb0e5237a95e68acc9e0bcf5f10f7166cedf94f41caa`;
its retired final hash is `db02ee0cb938c0bc0f2949d4da50ad0bb1ffe538e21da04e303bdd24495ab7e2`.
No original file is missing. The old audit's zero additions describe its own
earlier instant; the following precise post-audit additions are separately disclosed:

| Historical scheduler addition | Bytes | SHA256 |
| --- | ---: | --- |
| `out/pd5-stage-c-snapshot-rerun-20261023/agent-exit.txt` | 3 | `13bf7b3039c63bf5a50491fa3cfd8eb4e699d1ba1436315aef9cbe5711530354` |
| `out/pd5-stage-c-snapshot-rerun-20261023/agent-final.txt` | 1136 | `042427424fe1b4f52a7342b2e00fe509a6f119978ce433b1a600dfcd2e2035cc` |

Their discovery time, file mtimes, exact bytes and explanation are in
`post-audit-additions-discovery.json`. Original package payload hashes match.
The new immutable inventory includes both retired outputs, the entire previous
invocation and all current historical PD/build roots: **46 roots / 42,689 files**.
The active new invocation is separately input-bound; it is not an immutable
historical root. No active external scheduler log was frozen as historical input.

## Discovery failure and actual outcomes

The runner passed `binding-before-native` and `protected-before-native`, then ran:

```text
cuexis_pd_host_probe.exe "PD5 full content Snapshot Seek Replay boundary" --list-test-names-only
```

The existing Catch CLI rejected the discovery parameter:

```text
Error(s) in input:
  Unrecognised token: --list-test-names-only
```

Discovery stdout is empty. Runner exit is 1 and its state records
`AssertionError('hidden test discovery failed')` with `processes=[]`.
The discovery subprocess's numeric exit code was not persisted by the original
runner; it was not reconstructed by rerunning discovery. A native executable
process performed CLI parsing, but **no native test / 40k low or high started**.
Low and high are both **NOT RUN**; the low native-test authorization was not
consumed. No alternate CLI, extra discovery, retry, reduced fixture or saved-wire
decode followed. No owned probe remained after the stop.

The 24-stage validator, Snapshot declaration, Replay/Seek equality, explicit close
and after-close checkpoint were not exercised. The 90-minute external ceiling,
five-minute error-exit grace, hidden/BelowNormal launch policy and replyDeadline=30s
were retained in the bound runner; no external termination was needed.

## Verification and receipts

Fresh post-stop `bind.py check` and `bind.py frozen` both exited zero before any
report edit. They confirm source/report/build/binary/archive/runner identity,
all 42,689 new frozen files, clean main F1
`eed36648a692333e47f798aa45be36ca850d08de` and unchanged environment script.
The original binding is retained and will not be loosened for this separate
report commit. The final report receipt verifies the precise new documentation
overlay and all other original source bytes separately.

| Preserved record | SHA256 |
| --- | --- |
| `input-binding.json` | `fced6857558b216ae6a14907a136e6e192537e3360e48fcfb5bed7ffdfd8de68` |
| `frozen-before.json` | `2f1caa5cfcfa60055a9f74db759335a1466e3ed251d49921e67868f6334771c0` |
| `source.zip` | `552748328cb51530023cef297800b596877989aea513cc698138e3a6dca112d6` |
| `build-payload.zip` | `282f3ed5b81b9bf32f420e0eb0efd5419a60573881cb80dd5a2a2434c3f88394` |
| `probe-tests.stdout.log` | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |
| `probe-tests.stderr.log` | `74dc4be48017091bdf3808d60adcdc9560e89131afacbeabac1165b0d22280d5` |
| `acceptance-runner.stdout.log` | `aa4ed30ddc78de3b42444dddf533985020a81833e08fb72a28393ae896f13e44` |
| `acceptance-runner.stderr.log` | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |
| `acceptance-state.json` | `261dfafc5757460db687ed813b0a7dede881ac20967043385e31f467bda3ca77` |

`final-stop-audit.json` also records byte lengths, original commands and all setup
attempts. The source and report commits remain separate; the eventual report SHA
is stored in `report-commit-receipt.json`. Documentation and diff checks are the
applicable checks for this report-only update; no C++ regression or test rerun is
claimed. Any further runner correction and invocation require parent coordination.

Production defaults, Recovery wire, public API/SDK, Gameplay/Snapshot semantics,
seven Packed physical budgets, four hash ceilings 134217728/1000000/4096/8388608,
deadline and 1.10/15% gates were not changed. Public stop-interface work remains
suspended, unaccepted and unimplemented. Old window 3, formal sampling/registration,
merge and push remain stopped. The historical close stall remains unexplained.
There is no performance, general lifecycle, PD-5, RPA, production or Stage 7 closure.

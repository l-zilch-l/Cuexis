# Stage 6 Four-Item Handover Implementation

状态：implemented candidate；local functional and Release verification passed；owner / hosted acceptance pending

证据日期：2026-10-07（UTC build date 2026-10-06）

## Scope and authority

The owner requested completion of the four Stage 6 residuals already assigned to S7A-8,
implementation on the existing `stage-7`, and submission to existing PR #32 for their own
approval. Baseline was `4d8024e1c62d83a7a1a2859fc146107d8bcc5096`; existing documentation
edits were preserved. No history reset, new PR, merge or stage-close operation was performed.
The prior [audit](2026-10-06-stage6-handover-audit.md) remains dated source evidence.

## Implementation disposition

| Item | Implemented behavior | Acceptance boundary |
| --- | --- | --- |
| S7A-8.1 | ON package metadata, distinct candidate public library filenames, explicit import permission, install flavor stamp, Player project/CXC selector, candidate presets and a Linux Quality ON row | OFF/ON and installed consumer matrix; hosted row pending |
| S7A-8.2 | Real typed Foundation CXT expansion and assembly CLI, Packed encoding, project and CXC entry metadata, embedded closure report, actual Playback prepare/commit before atomic package publication | Foundation registered profile; Gameplay v2 retains strict author contract; S7A-7 Judgement bridge excluded |
| S7A-8.3 | Existing installed Reference Host seven-verb regression plus explicit candidate entry and package flavor observation | Commands and frame outputs verified locally; hosted same-SHA evidence pending |
| S7A-8.4 | SDK 0.7.1 patch candidate, legacy consumer rebuilds, trusted-base registry/CODEOWNERS, metadata-only PR gate, exact API owner approval and merge-tree audit | Owner record, trusted-master bootstrap and platform protection still pending; no release approval implied |

The Foundation assembler derives its registered lanes4 feature and actual resource uses from
typed content, rather than caller-provided feature injection. Source metadata assertions are
validated before appending expansion. The canonical Writer/Reader owns profile, identity,
parent and budget rejection. Gameplay v2 authored declarations are not repaired implicitly.
The current strict Capsule revision 3 author adapter remains independent. This repair does not
implement or certify full Judgement/Playback integration, budgets, S7A-9 or stage closure.

The CLI atomically embeds `cuexis.candidate-closure.v1` in the manifest. CXC prohibits unlisted
archive files, so a separate unregistered report file was rejected during real integration and
replaced by this manifest representation. Project extensions carry the same entry descriptor,
allowing filesystem and CXC consumption. Resource preflight failure preserves the old package.
The final report includes compiled/artifact/prepared identities and actual Playback capabilities.

## Local evidence matrix

These logs live under the ignored `out/` directory. Product implementation is committed at
`fcc2af350392b5c2b61709d168a00fbed96980ae`; parallel fixture isolation is committed at
`6f4592b3b4718bc302a83bf398c624a021e48a96`. Initial matrices are working-tree evidence;
post-commit rows below explicitly identify the later verification, without relabeling old runs.

| Row | Invocation / evidence | Result |
| --- | --- | --- |
| MSVC OFF Debug fresh/clean | `out/s7a8-debug-configure.log`, `out/s7a8-debug-build.log` | 1006-case initial matrix exposed 7 SDK-literal/CLI failures; corrected 41-case regression and 3 package/distribution checks passed; one Windows symlink case skipped |
| MSVC ON Debug fresh/clean | `out/s7a8-candidate-configure.log`, `out/s7a8-candidate-build.log` | 38/38 passed with real CLI, missing-asset atomic failure, mixed-prefix and installed Host identity checks |
| MSVC ON shared Debug fresh/clean | `out/s7a8-candidate-shared-configure.log`, build/incremental/tests logs | 38/38 passed; final changed CLI/Host subset 7/7 passed; private access remains static-only, public shared results checked |
| Linux GCC 15.2 headless ON | `out/s7a8-linux-configure.log`, build/test logs | build passed; 17/17 candidate tests passed |
| MinGW headless ON | `out/s7a8-mingw-configure.log`, build/test logs | build passed; 28/28 candidate tests passed |
| MSVC OFF Release fresh/clean | `out/s7a8-release-configure.log`, build/tests logs | build passed; 1006 tests, zero failures, one Windows symlink case skipped; 641.51 seconds |
| Post-commit MSVC ON static/shared | `out/s7a8-postcommit-msvc-static.log`, `out/s7a8-postcommit-msvc-shared.log` | parallel runs passed 7/7 each after the fixture isolation commit |
| Post-commit Linux GCC / MinGW ON | `out/s7a8-postcommit-linux.log`, `out/s7a8-postcommit-mingw.log` | rebuilt changed test target; 17/17 and 28/28 passed respectively |
| SDK authorization | `python -B tools/check_version_gate_tests.py`, `out/s7a8-version-tests.log` | 24 tests: 22 passed, 2 environment-dependent shell cases skipped on Windows |
| Docs / target / API metadata | `python -B tools/check_docs.py` | 379 Markdown/20 JSON-CXT passed; status contract 4/4 and target contract 2/2 passed; format target passed |

MSVC static/shared, Linux GCC and MinGW produce identical final package bytes: SHA256
`11109688cd9b26cdc625e5b52c66b193eb6735e9bf4d7eb28628d21b0432dc0e`.
The actual prepared identity is
`2360b547c60ee506482808c7daa85f4e8a8c4051f1cce2428b5440ff7d6a3b86`.

The first parallel static/shared rerun exposed a test fixture collision: both processes used
`cuexis-candidate-1` and could clean up the other's input. The test now atomically reserves a
process-specific directory without deleting an existing directory. Both parallel rows passed
after rebuilding this fix; product package bytes and public contracts did not change.

The 16-tap stair golden compiled semantic identity is
`c08e0e8a1236001058ae338cffac0cb609b8ee95fb23ceb5d7b0dbc3629febcc`;
artifact identity is `b74e768657a076ebfdea222d5c19351ca7937ff05c59c7c552c71fbf4d94f7df`.
The real CLI array-reordered input must produce identical package/report bytes. Negative rows
include lane 4, duplicate identity, foreign chart, entity budget, missing source, missing actual
asset, OFF factory/no-read, wrong entry/profile/revision and unpermitted installed package.
Installed Host drives open/play/tick/pause/seek/reload/quit with 16 objects, suppressed paused
Ticks and discontinuity checks. Legacy v4 command/failure fixtures remain part of the same gate.

SDK authorization negatives cover changed head/base/tree/version/date, wrong actor, bot actor,
edited approval, wrong field types and CLI boolean bypass. A real temporary Git repository
also covers squash, source branch deletion and a fresh clone lacking the approved head. The
checker fetches only the validated SHA from the trusted repository, never executing that code.
The independent reviewer reproduced this absence before the fix.

## Owner and hosted evidence to fill

1. Owner reviews PR #32 and supplies the exact unedited owner record defined by
   [VERSIONING](../../../../guides/VERSIONING.md#s7a-84-owner-approval-record). The agent does not
   post that approval. The PR description carries a draft tuple for the current head/base/tree.
2. Owner establishes the checker/workflow/registry on a trusted master baseline. Current master
   lacks this new registry, so the proposed gate deliberately fails bootstrap rather than reading
   candidate authorization. No owner bootstrap or branch protection update is claimed here.
3. Required-check context and CODEOWNERS alone do not prove a trusted workflow source. In
   particular merge_group YAML comes from the queue candidate; copying the base checker inside
   that YAML cannot prevent a replaced job. Before enabling queue release, configure a protected
   required workflow/external trusted checker, or keep merge queue disabled. This is an acceptance
   blocker, not a bypass accepted by this implementation.
4. Fill same-SHA hosted Linux Quality / Windows MSVC / MinGW rows after push. Per owner request,
   submission does not wait for CI. GPU/window and media-device tests are not inferred from
   headless command tests.

S7A-8.1–8.3 local implementation evidence and S7A-8.4 candidate delivery remain separate from
owner release acceptance. Stage 6 historical closure is unchanged; Stage 7 remains active.

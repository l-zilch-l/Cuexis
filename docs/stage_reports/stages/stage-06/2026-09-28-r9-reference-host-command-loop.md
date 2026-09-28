# R9 Reference Host Command Loop and Play/Pause

Date: 2026-09-28
Package: Stage 6 review remediation, R9
Normative source: [R9-reference-host-command-loop.md](../../../stage_plans/reviews/stage-06-review-remediation/R9-reference-host-command-loop.md)
Status: implemented and locally verified; hosted same-SHA validation and owner acceptance are not claimed here.

This report owns dated evidence only. The contract, the state matrix and the case
definitions stay in the R9 document; nothing here restates them as a second
authority.

## 1. What was approved and what was built

The approved design gives the reference host a command program: a text file of
`open` / `play` / `pause` / `tick` / `seek` / `reload` / `quit` commands, run
against one live session, with a machine-readable run record. The point is that
the host is the time authority, so a pause, a resume, a seek and an in-place
reload all have to be measurable rather than assumed.

Delivered:

| Area | Where |
|---|---|
| Command file parser, limits, 14 stable diagnostic suffixes | `examples/reference_host/src/host_commands.{hpp,cpp}` |
| Host clock, transport, counters | `examples/reference_host/src/host_clock.{hpp,cpp}` |
| Command dispatch, `HostContext`, shared SDK actions | `examples/reference_host/src/host_runner.cpp` |
| Invocation parsing, the run record is opened before any rejection | `examples/reference_host/src/main.cpp` |
| `host.diagnostic` event | `examples/reference_host/src/host_report.{hpp,cpp}` |
| Case runner, wiring, A2 increments | `cmake/VerifyReferenceHostCommands.cmake`, `cmake/VerifyReferenceHost.cmake`, `tools/check_stage6_a2.py` |

The legacy path is unchanged. Its run record is byte for byte what it was and
the frozen golden assertions in `cmake/VerifyReferenceHost.cmake` were not
touched; the two fields section 3.6 adds are appended only for a user program.

## 2. Machine acceptance

| Check | Command | Result |
|---|---|---|
| Command cases | `ctest --preset debug -j1 -R cuexis_reference_host_staging -V` | `expected 41, completed 41, passed 41` |
| Legacy regression | same test | golden frame, identity and package assertions unchanged and passing |
| A2 characterization | `ctest --preset debug -j1 -R cuexis_contract_s6_a2` | passed |
| Architecture | `ctest --preset debug -j1 -R cuexis_architecture_tests` | passed |
| Documentation | `python -B tools/check_docs.py` | passed |
| Full local suite | `ctest --preset debug -j1` | see section 6 |

The absolute anchor in section 6.1 was re-derived by running the built host
directly rather than read off the gate, so the claim does not depend on the
gate agreeing with itself. `open; seek 625; quit` yields identity
`6d01494c126f3ae8fc9420259dc92873233022dec9dd6bf9caf04b217f100cc5`, a frame of
625 with discontinuity id 1, two objects, algorithm 3 and digest
`11596562486377158370`, at frame index 0, which is what section 6.1 permits when
the legacy initial sample is omitted. That turns section 6.1's execution
precondition from an assumption into a measurement.

The case runner's own checker was verified before it was trusted, because in the
red state no report exists and the runner never reaches its parsing, so a broken
checker would have been used to judge the implementation without ever having been
shown to fail. Driving it with a fixed fake report found two defects in it: a
required literal compared one token at a time, so a multi-token `require` could
never fail, and a frame comparison that passed its condition to `if()` as text,
which re-tokenizes on whitespace and turns the embedded quotes into literals, so
a deliberately wrong frame silently matched. Both are fixed.

## 3. A parser defect found by review

The command file parser was reviewed line by line only after the case suite and
the mutation evidence were both green, because both of those exercise the
behaviour the contract describes and neither can see a path the fixtures never
take.

Building a filesystem path from the decoded `open` argument is the one step in
the parser that can fail: on a platform whose native path encoding cannot
represent those bytes, the conversion throws. The decoded bytes are arbitrary
and unchecked, so a command file can reach it. The throw left the parser, left
`main`, and in a debug build with no console handler ended in a blocking abort
rather than an exit.

This is not hypothetical. A command file holding `open "<0xFF 0xFE>"` made the
host hang with a zero-byte report on Windows with code page 936, which is the
code page of the machine this work was done on; the same invocation with an ASCII
path exits normally with a diagnostic. The host is a diagnostic tool, and hanging
on malformed input with no diagnostic is the one outcome it must never produce.

The conversion is now guarded: a path the system cannot represent is reported as
`host.command.syntax` with `step=parse` and the source line, and the host exits
non-zero. The invalid-byte file now yields
`line 2: 'open' has a path argument that this system cannot represent.`, and all
41 cases still pass.

Two things follow. First, this cannot become a committed fixture, because the
fixture set is pure ASCII by policy, so a regression here would not be caught by
the suite; it is recorded here and the reproduction is a one-line command file
holding those two bytes. Second, the reason originally given for constructing the
path from a narrow string rather than `u8string` was that the narrow conversion
cannot throw. That is wrong: the narrow conversion is not exception-free either,
so the choice between the overloads does not remove this failure mode at all. A
general backstop that turns any escaped exception into a diagnostic and a
non-zero exit would be more robust than a guard at the one currently reachable
site, but it needs a diagnostic code and section 3.5's list is closed, so that is
an owner decision rather than something to settle here.

## 4. Section 6.2 digest relations

Section 6.2 asks for two relations that a per-frame triple comparison cannot
express, because the `frame` directive deliberately carries no digest value. Both
were missing. Only three cases used `digest-anchor` and all three sample a single
frame, while the five cases that sample more than once checked no digest value at
all, so a host that printed one fixed digest for every frame would have passed the
whole suite. Section 5's item-wise digest equivalence across a reload was
unverified for the same reason.

The runner now enforces both across every case, without adding a directive to the
frozen expectation grammar: a case that samples two different frames must not
report one digest for all of them, and a frame sampled in two places must carry
the same digest in both. The correct host satisfies both, which is what makes the
second a real constraint rather than a tautology.

Checking these exposed a defect in the checker itself. The failure helper is
declared as `function(cuexis_command_record_failure case_id message)`, so a call
written as two adjacent quoted strings binds only the first and silently discards
the rest. Four call sites did that, and one dropped the half that says a crash or
timeout is not negative evidence. A truncated message is worse than a short one:
it still contains the substring that probe looks for, so the check reads as live
while reporting less than it claims. All four now pass a single message, and the
A2 characterization check rejects the split form outright.

## 5. Mutation evidence

Sixteen mutations, each applied inside an isolated git worktree that the main
working tree never sees. The whole set was run against `0e14232` and re-run
against it after the parser fix in section 3, because evidence about a revision
stops being evidence as soon as the revision moves. Every run had the unmutated
host green first, every mutation was restored byte for byte afterwards (the
worktree was verified clean in all sixteen cases), and no mutation was allowed to
count if the mutant failed to build, crashed, or timed out. The target column is
the assertion the mutation was expected to break.

| Mutation | Target | Result | Other cases that also failed |
|---|---|---|---|
| `pause` is a no-op | c02 | caught | c04 |
| a paused tick still advances the clock | c07 | caught | c02, c04, c08 |
| resume compensates for the wait | c02 | caught | c04, c07 |
| `seek` does not bump the discontinuity id | c01 | caught | c07, c11a, c11b, n05f |
| `seek` auto-plays | c07 | caught | none |
| `reload` clears the transport | c05 | caught | c04 |
| `reload` resets the clock | c05 | caught | c04 |
| `reload` keeps a stale non-zero delta | c04 | caught | none |
| constant digest | c02, c05 | caught | c03, c04, c07 |
| a second `open` unloads the first | n03 | caught | none |
| bypass one tick budget guard | none, recorded | survived, documented | none |
| bypass both tick budget guards | n05d | caught | none |
| bypass the termination check | n04f | caught | none |
| bypass the command-count limit | n05b | caught | none |
| bypass the seek limit | n05e | caught | none |
| delete the gate call | A2 characterization | caught, exit 1 | not applicable |

Two entries carry more weight than the rest.

The constant-digest mutation is the one that shows section 6.2 was a real gap and
not a formality: before the relations existed it survived every case, and it is
caught now exactly because every frame directive still matches and only the
relation can see it.

The reload-stale-delta mutation is what section 5 and H4 item 1 asked for: it
makes the second of two consecutive paused reloads pass the stale non-zero delta,
and c04 fails on `require-count targetSimulationDeltaTimeMs=0 1`. That is the
assertion catching the actual SDK call argument, not a proxy.

### 5.1 The one recorded survivor

Removing the per-command tick budget guard alone leaves the suite green, and that
is a property of the contract rather than a hole. A single tick above the budget
also pushes the cumulative total above it, both guards report
`host.command.limit_exceeded` at the same source line, and `detail` wording is not
asserted anywhere, so neither guard alone is observably load-bearing. Removing
both is caught by n05d, which is what shows the budget as a whole is pinned.

This is recorded rather than hidden because the alternative readings are both
wrong: calling it a coverage gap would be false, and quietly dropping the entry
would leave the next reader unable to tell a redundant guard from an unverified
one. Whether the two guards should be collapsed into one is a contract question
for the owner, not something to be settled by deleting a check.

## 6. Local environment deviations

The full local suite reports two failures, and both are pre-existing and
unrelated to R9:

- `cuexis_player_distribution` fails at `VerifyPlayerDistribution.cmake:217`. It
  packages and runs the Player, which needs a player device.
- `cuexis_contract_version_gate` fails its bootstrap-shell case. The identical
  test fails with the identical diagnostic at the merge base `670cca8`, which was
  checked by running it there rather than inferred.

The version date is not the cause: UTC was still 2026-09-28 and
`26.09.28-2` matched the trusted date when this was written.

## 7. Boundaries held

The frozen text of ADR 0042 was not modified. No SDK public API, enum or golden
was changed. No existing gate was removed or weakened, and `RESOURCE_LOCK` and the
timeout budget were not adjusted to buy green. Historical report phenomena and
evidence were not rewritten. Stage 7A / Stage 8 / Stage 9-12 deliverables and
Proposal 1 were not started. SPEC-27 remains open; closing it needs owner
acceptance and is not part of R9.

## 8. Not claimed

- Hosted same-SHA Linux Quality, Windows MSVC and Windows MinGW validation on the
  final R9 commit. The Version Gate passes on the current commit; the rest are
  recorded when the branch stops moving.
- Owner acceptance of the R9 contract, including the redundant tick budget guard
  noted in section 5.1.
- The mutation harness and the checker self-test live under `out/`, which is not
  committed. The harness is reproducible from this report's method, and the
  defect class it fixed is now guarded permanently by the A2 check, but the
  records themselves are working-tree evidence rather than repository contents.

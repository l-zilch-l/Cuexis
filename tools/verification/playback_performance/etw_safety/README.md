# PD-5 ETW tool acceptance (lifecycle canary and F1 collector)

This private Windows x64 tool validates cleanup mechanics only. It has no Cuexis
player, 40k, performance-window, pilot or registration entry. The tool itself
never changes the power plan, the system affinity or DiagTrack; the one affinity
value it reports is the fixed mask 4 that the sealed F1 workload applies to its
own processes, exactly as `plan.json` declares. The normal-allocator formal
profile remains **no ETW**; this optional diagnostic tool is not a predecessor to
that profile.

The sealed package carries two fixed roles. Role `canary` runs two empty
lifecycle cases. Role `f1` runs three F1 collector cases against one bounded
diagnostic WPR capture, which is currently the only path to attribute the F1
read-ratio excess. Neither role performs formal measurement.

## Delivery and limits

`prepare_etw_tools.py` copies every source module, this README, both `.wprp`
files and `plan.json` into a new directory and binds each Python source, both
profiles, `plan.json`, the frozen inputs, the exact interpreter and the WPR
executable. The generated `run_tool_canary.ps1` and `run_f1_scale.ps1` pin the
binding and bootstrap hashes. Bootstrap checks the full Python source set, the
artifact set including `plan.json`, and the profile bindings before any local
import. All roles run with `-B -I -S`. No automatic UAC.

From administrator PowerShell, invoke the sealed package's `run_tool_canary.ps1`
once. Its fixed cases are:

1. Minimal `PD5ToolLifecycle` file-mode trace, hold 2 seconds, normal named WPR stop. Require
   worker Job zero, both exact-name queries and elevated QueryAllTraces absence,
   nonempty stable merged trace, no remaining temporary ETL, guardian release,
   guardian exit, and an independent harness absence proof.
2. A fresh empty trace, hold 2 seconds, start one bounded harmless Python sleeper,
   and intentionally exit the exact controller before requesting stop. Guardian
   proves controller death, drains its shared
   Job, stops only the two sealed names, and proves absence. Raw temporary ETL is
   retained. This tests death during an active trace; it does **not** validate a
   real WPR-stop timeout or Windows service-side merge wedge.

Stop on the first failed case; no retry or replacement. A failed case is still
failed when recovery succeeds. These are tool limits, unrelated to PD budgets:
120 seconds per controller/guardian, takeover at 95 seconds; active logger
emergency threshold 20 seconds from case start; disk threshold 128 MiB;
WPR start 15 seconds and normal
stop 60 seconds. A logger already absent may leave a normal merge running until
its own deadline. Harness case supervision is capped at 135 seconds, with at
most 3+3+15 seconds reserved for failed-case recovery. A disk threshold is checked
periodically, so it is not a hard allocation/storage ceiling.
The active threshold is also a periodic trigger, not a hard promise of absence
at exactly 20 seconds: an in-flight query can use its 8-second wait budget, plus
up to a two-second polling interval, before takeover starts. Remaining recovery
must still fit the stated controller/guardian deadline.

### F1 tool acceptance cases

Role `f1` runs exactly three cases, each allowed once, with no retry and no
sample replacement:

1. `f1-scale`: start one sealed capture, then let the sealed collector run six
   native F1 launches across three processes (mask 4, 30 second warmup). Require
   collector exit 0, a complete `collector-result.json` with all six launch
   records, Job zero after the drain, the normal named WPR stop, both exact-name
   queries and elevated QueryAllTraces absence, and guardian release.
2. `f1-scale-timeout`: kill the collector 20 seconds into the 30 second warmup,
   while a real native F1 child is provably alive. Require controller exit 74,
   the recorded active-process count at expiry, a drained Job, every observed
   native PID proved dead by process identity, and the same orderly WPR stop and
   absence proof. A surviving Job member or native PID is `FAILED/UNRESOLVED`.
3. `f1-scale-death`: exit the exact controller at the first proof that a native
   F1 child is alive, without requesting stop. Require controller exit 73, the
   recorded native PID and creation time, guardian takeover of the shared Job,
   and a per-PID proof that the native child died after both controller and
   guardian were gone.

Each case's controller exit code (0, 74 and 73 respectively) is one of the
conditions the harness checks; it is recorded in that case's `result.json`. The
launcher itself returns the harness exit code: `0` when all fixed cases passed
and `1` for `FAILED/UNRESOLVED`. A `74` or `73` launcher exit never means pass.

The collector spawns the native F1 executable inside its own inner Job
(`process_control.py`, `KILL_ON_JOB_CLOSE`), so that child is not a member of the
outer capture Job. Proof is therefore both `job.active() == 0` for the gate and
collector, and a per-PID liveness/death record taken from the `PD5_PROCESS_START`
anchors in the collector's own native logs. Exiting the controller through
`os._exit(73)` deliberately skips the case `finally`, so the guardian's own Job
handle keeps the Job alive long enough to drain it.

F1 tool limits are separate from both the canary and the PD budgets: whole
capture 420 seconds, guardian takeover 380 seconds, disk threshold 2 GiB,
collector budget 250 seconds, supervisor workload budget 300 seconds (collector
budget plus a 50 second grace), WPR start 20 seconds, normal stop 110 seconds,
harness supervision 480 seconds. A healthy workload is bounded by the collector;
only a hung collector reaches the supervisor budget. None of these values is a
product budget, and none may be reused as one.

## Ownership and cleanup

Each case uses a sealed fresh random nonce, two exact lifecycle-profile logger names,
a fresh named Job, and immutable create-new receipts. Nonempty ETW logfile paths
must resolve into that case's trace-temp directory. This is directory
corroboration, not an exact-basename identity proof; WPR chooses temporary names.
Blank log paths establish no ownership. Exact sealed logger names and the fresh
claim establish the limited ownership key. Case variants/unknown suffixes are
unresolved and never silently treated as absence. Only exact expected names can
be stopped. Unknown/changed WPR sessions and old PD5 sessions block preflight.
The four already inventoried DiagTrack name/empty-path tuples are recorded
background, not proof of a quiet machine or permission to stop them.

Normal WPR commands run inside the shared KILL_ON_CLOSE Job. The guardian and
harness retain handles outside it. A trusted, stdlib-only gate waits on its sole
stdin writer and launches nothing before assignment and membership verification.
If the parent dies before GO, it exits inertly on EOF; that pre-GO wrapper may
exit asynchronously outside the Job. Job-zero proves no ETW-capable Job command
remains, not synchronous disappearance of every inert bootstrap.

The controller never performs raw ETW STOP. Guardian takeover first terminates
and waits for the exact controller process object (PID plus creation time), then
terminates/drains Job workers. Only then can a bounded API helper issue exact
STOP. The helper independently checks controller dead and Job zero. If guardian
crashes, harness recovery additionally proves that guardian dead before taking
over. Stable process handles prevent PID reuse. STOP return code alone is never
absence proof. Access denial, incomplete inventory, exceptions and timeout mean
unknown/failure, with preserved partial evidence.

API helpers have 8-second waits and are killed/reaped on timeout. Windows/kernel
failure can still prevent a positive cleanup proof. In that case report
`FAILED/UNRESOLVED`, preserve files and do not rerun; no user-session/global
cancel or broad logger shutdown exists. Simultaneous loss of controller,
guardian and harness is outside the kernel-session recovery guarantee: closing
all Job handles kills processes, not kernel ETW loggers.

Before WPR start, the case waits for an exact harness acknowledgment of retained
controller/guardian identity and shared Job handles. Raw-stop authority is
create-new and one-shot per case. A crashed caller or unreaped helper never
causes a second STOP helper: subsequent attempts only query until the deadline.
Completion needs the caller's positive helper-exit receipt as well as absence.

The fixed canary is inspected before the F1 cases are used. Only the sealed
`run_f1_scale.ps1` connects the collector, and it remains a bounded diagnostic:
the accepted formal measurement stays ETW-free. A canary or F1 tool pass is not
performance acceptance, not full fault coverage, not 40k memory proof and not
PD-5 completion.

## Profile boundary after the first administrator failure

The preserved Power canary `canary-20261023T105430Z-8d7faec2f9024d418f229d8c90643f53`
failed normal completion: its guardian hit the tool-only 128 MiB disk threshold
while normal stop was in progress, then proved emergency cleanup. Raw ETL ended
at 142 + 69 MiB. The second case was not run. Never reinterpret that result as
a pass or reuse that package/run as a successful lifecycle test.

The new profile is a different, fixed tool-verification configuration: only
ProcessThread and the WPR Control provider keyword 0x40000, two 64 KiB/16-buffer
collectors, no CPU sampling/CSwitch/DPC/stack tracing. The logger suffixes and
cleanup rules are unchanged. Bootstrap checks its hash before local imports;
both preflight and start use the same XML/name. There is no built-in Power
fallback. The configured event scope is smaller; actual volume and successful
normal completion still require the two-case administrator check. No volume or
latency pass is inferred from profile parsing.

This follows the provider/collector structure documented in
[Microsoft WPR custom profiles](https://devblogs.microsoft.com/performance-diagnostics/authoring-custom-profiles-part-2/)
and [provider definitions](https://learn.microsoft.com/en-us/windows-hardware/test/wpt/2-system-and-event-provider-definitions).
Its purpose is lifecycle verification only. It cannot certify the later Power
performance diagnostic, which remains separately scoped and not a predecessor
for normal no-ETW formal measurement. All tool limits remain unchanged; no
automatic run, retry, sample replacement or product budget change is introduced.

## F1 profile boundary

`f1_scale.wprp` is a second, independent fixed tool profile, `PD5F1Scale`, sealed
alongside the lifecycle profile. It uses one 128 KiB/64-buffer system collector
restricted to CSwitch, DPC, IdleStates, Interrupt, Power and ReadyThread, one
128 KiB/32-buffer event collector, and the
`Microsoft-Windows-Kernel-Processor-Power` provider at keyword mask `0x2ff`.
The explicit `0x2ff` matters: the original `0xffffffffffffffff` mask was reported
as `0xff` by `wpr -profiledetails` and was corrected before validation. Parsing
succeeded with exit 0 and `PD5F1Scale.Verbose.File`; bootstrap binds the file
hash, and both preflight and start use the same XML and profile name.

The F1 profile exists to attribute the F1 read-ratio excess under one bounded
diagnostic capture. It is not a Power performance profile, not a formal
measurement profile, and not a volume or latency acceptance. Its keywords widen
the event scope substantially compared with the lifecycle canary, so the two
profiles are never interchanged, and the sealed binding refuses a package whose
`f1-scale` profile resolves to the lifecycle name. The blocklist and absence
rules above are unchanged: only the two exact sealed logger names may be stopped,
and a pass still requires Job zero plus the elevated absence proof.

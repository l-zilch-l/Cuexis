# RPA F4 Handoff Package

Date: 2026-10-15

## Handoff status

This is the current typed handoff package assembled from RPA evidence. It is
ready for PD consumption as a baseline, but F4 is not owner-accepted or
stage-closing because F3 production thresholds and physical-device evidence
remain open.

## Inputs for PD

- Current behavior baseline: commits debb206 and c82e500.
- Automated F1/F2 evidence:
  rpa/2026-10-15-f1-f2-final-matrix.md.
- F3 baseline:
  rpa/2026-10-15-f3-final-baseline.md.
- Capacity limits used by this baseline: 40,000 expanded semantic entities,
  40,000 requirements, and 16 MiB Packed entry.
- Current latency target: 20 ms applicable Seek target.
- Required PD action: rerun content, steady-state, snapshot/Seek, Replay, and
  Packed measurements after the judgement refactor.

## Boundary and rollback

The 40k/16 MiB values are candidate capacity gates from the Packed Chart
contract; they are not a claim that every arbitrarily complex 40k chart fits.
The 20 ms value is a predeclared current-RPA measurement target. PD must
produce a new implementation identity and new measurements before any final
production limit is frozen.

Rollback means retaining the current RPA evidence and reverting only the
implementation change under review. Existing v4/v5 identity and rejection
evidence remain separately versioned; this handoff does not alter them.

## Open acceptance items

- Physical DFJK/Hold, focus, pause/resume, listening synchronization, and
  device-loss recovery.
- End-to-end Seek latency and prepare-only memory accounting.
- Production profile and threshold owner acceptance.
- FrameObservationLedger reclamation defect documented on the desktop.
- Final owner acceptance of RPA and the Stage 7A close package.

## PD handoff rule

PD may consume this package as a measured RPA baseline. It must not treat the
current F3 values as final performance certification, and it must rerun the
affected F3 dimensions after the judgement-system redesign.

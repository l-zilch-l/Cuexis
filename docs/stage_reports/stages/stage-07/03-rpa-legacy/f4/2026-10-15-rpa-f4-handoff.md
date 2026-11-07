# RPA F4 Handoff Package

Date: 2026-10-15

## Handoff status

This is the current typed handoff package assembled from RPA evidence. It is
ready for PD consumption as a baseline, but F4 is not owner-accepted or
stage-closing because F3 production thresholds and physical-device evidence
remain open.

## Inputs for PD

- Current behavior baseline: commits debb206 and c82e500.
- RPA execution and recovery contract:
  ../../../formats/rpa-execution-contract.md.
- Runtime host boundary and control/resource ownership:
  ../../../api/realtime-host-boundary.md.
- Candidate capacity and Packed entry budgets:
  ../../../formats/PACKED_CHART_FORMAT.md.
- Existing production acceptance inventory and source identity ledger:
  2026-10-14-rpa-production-acceptance-handoff.md.
- Automated F1/F2 evidence:
  rpa/2026-10-15-f1-f2-final-matrix.md.
- F3 baseline:
  rpa/2026-10-15-f3-final-baseline.md.
- Capacity limits used by this baseline: 40,000 expanded semantic entities,
  40,000 requirements, and 16 MiB Packed entry.
- Current latency target: 20 ms applicable Seek target.
- Required PD action: rerun content, steady-state, snapshot/Seek, Replay, and
  Packed measurements after the judgement refactor.

## Typed handoff inventory

- Input mapping: v4 remains the accepted Playback input path; v5 gameplay
  graph and Packed Chart remain explicit candidate capabilities.
- Requirement and Replay identity: preserve the existing canonical identity,
  complete ReplayEvaluation, fault prefix, cut, checkpoint, archive, and
  recovery revisions recorded by B2/B3.
- Golden and rejection evidence: consume the existing RPA and Chart capacity
  fixtures; retain the 40,000 and 16 MiB over-limit rejection cases.
- Capability and rejection table: preserve candidate-only v5/Packed entry
  selection and refuse unsupported revisions, over-budget entries, and
  incompatible profiles.
- Migration and rollback: PD changes receive a new implementation identity;
  the current RPA reports remain the rollback comparison point.

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

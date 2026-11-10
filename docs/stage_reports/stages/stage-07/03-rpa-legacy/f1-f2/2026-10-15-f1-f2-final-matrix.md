# RPA F1/F2 Matrix Evidence

Date: 2026-10-15

## Scope

This report records the automated F1/F2 continuation on commit debb206
(parent acceptance fix: 3fab43e). Physical-device acceptance, production
calibration, owner acceptance, and frame-ledger reclamation remain open.

## Results

- Hosted same-SHA F2 workflows passed: Linux Quality 38674366454, Windows
  MSVC 38674366503, and Windows MinGW 38674366544.
- MSVC OFF Debug, OFF Release, OFF Headless Release, OFF Shared Debug, and
  ON Shared Debug completed with exit code 0.
- MinGW ON Debug and ON Shared Debug completed with exit code 0.
- MinGW ON Release initially failed because GCC 16 emitted
  Warray-bounds for enum indexing in controlFullyApplied; the fix maps
  each Participant explicitly to its array slot. The full row was then
  rerun and passed configure, build, 1041 tests, and all budget shards.
- The earlier SDL callback pause test failed once in the first OFF Debug run.
  Three targeted repetitions passed. The original failure is retained as an
  intermittent environment candidate, not erased.

## Evidence

Evidence root: out/rpa-final-20261015.

MinGW Release retest:
out/rpa-final-20261015/mingw-release-retest-20261015-135757/20261015-135757-b77bc2aa.

## Open gates

Physical keyboard/listening/device-loss recovery, production parameter
calibration, budget owner sign-off, frame-ledger reclamation, and owner
acceptance are not complete. The CXC unpack transient Access Denied diagnostic
also remains unresolved and is not converted into a production retry.

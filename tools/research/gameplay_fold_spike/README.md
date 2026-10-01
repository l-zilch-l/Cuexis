# Gameplay fold spike

Research spike for the candidate gameplay ruleset design in `docs/proposals/`. It is **not** part of
the product build: it is not added from `tools/CMakeLists.txt`, has no CMake target, links no
Cuexis library and is not covered by the architecture or allowlist tests. Nothing here is an
implementation of Stage 7A, and none of it may be copied into `engine/` without going through the
normal ADR, specification and stage gates.

It exists to put numbers on the candidate budget table in
`docs/proposals/GAMEPLAY_BUDGET_AND_SCALE_DRAFT.md` and to test three determinism claims. The
findings are recorded in
`docs/stage_reports/reviews/gameplay-ruleset-2026-10/2026-10-01-fold-spike.md`.

## What it contains

- `pattern.*`: Pattern -> Thompson NFA -> subset construction -> Moore minimization, with
  complement on a complete DFA and a state budget that names the failing subexpression.
- `runtime.*`: a tick-driven fold for Tap / Hold / Bomb / Roll with the four-phase tick order,
  arbitration by `claimKey`, `every(period)` sampling anchored at `arm`, a derived window-scale
  hook visible from `t+1`, an L3 fold with one time-bounded module, and canonical snapshot / seek.
- `continuity.*`: an isolated minimum model for handoff Hold / contact-following Slider continuity
  (`Gap` + `holdGrace`), strict grace boundaries, preserved Slider progress, and snapshot restore.
- `main.cpp` / `continuity_report.cpp`: synthetic charts and players, and the report.

## Build and run

No dependencies beyond a C++20 compiler.

```sh
g++ -std=c++20 -O2 pattern.cpp runtime.cpp contacts.cpp scenarios.cpp differential.cpp \
  differential_report.cpp geometry.cpp geometry_report.cpp continuity.cpp continuity_report.cpp \
  main.cpp -o fold_spike
./fold_spike > report.md
```

The run exits non-zero if any check fails: static sweep below observed activity, enumeration order
changing a result, a lossy seek, or the targeted sweep-signature case not behaving as predicted.
Section E prints per-profile result digests; build with a second compiler and compare them.

# Stage 7-PD tests and measurement probes

PD-specific fixtures, tests and measurement probes belong in this directory. They use independent
CMake targets and the `pd` test label; they are not RPA acceptance tests.

The static candidate target `cuexis_pd_baseline_probe` contains the preserved legacy
`PD deterministic presentation baseline` trajectory and separate fixed legacy/RPA workloads.
It imports the existing RPA test content builder as a dependency; that reuse does not make the
legacy trajectory an RPA workload.
The original saved executables and logs remain historical evidence at their original paths.

`cuexis_pd_allocation_probe` uses `pd_allocation_probe.hpp` only in its own static executable;
the timing baseline retains the normal allocator. Requested C++ bytes/counts and live high-water
observations exclude malloc/CRT/driver/GPU storage. Allocation-probe timings are not performance
evidence. `collect_storage.py` registers independent repetitions and coverage checks.

`delivery_baseline_tests.cpp` characterizes the old ledger capacity fault and the portable CPU
test renderer. The fault is an expected defect baseline, not a successful bounded-lifetime gate.

`tools/measure_pd0_method.py` separately registers calibration and acceptance. It uses fixed fresh
session trajectories, outer block timing, read-only Snapshot blocks, controlled process affinity
and active/dormant runtime recording pairs. Single-call timings remain diagnostic. Both workloads
validate complete Replay/Snapshot bytes and FrameDigest exactly. See
`docs/stage_reports/stages/stage-07/verification/2026-10-16-pd0-measurement-method.md` for evidence.

These measurement cases are hidden Catch cases and run by explicit name through the runner;
they are not automatically registered as CTest `pd` cases. They do not close PD-0.
The ordinary ledger/renderer cases are discoverable. A Release B5 Pause regression currently
stops further work; see `docs/stage_reports/stages/stage-07/verification/2026-10-16-pd0-storage-and-stop.md`.

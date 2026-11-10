# Chart Format Foundation F9: Handoff Readiness

日期：2026-09-07  
状态：pending owner acceptance and hosted verification

The F7 typed CXC candidate mapping and F8 low-reuse capacity harness are present in the working
tree. The Stage 6 handoff is not yet closed. Closure requires one candidate revision and commit
SHA to pass the same build, round-trip, capacity, and rejection matrix on hosted MSVC, MinGW, and
Linux, followed by owner acceptance.

## Allowed handoff surface

- candidate revision 1 and flags 1;
- Foundation static Tap/point/lane/press profile;
- STR0, REF0, IDN0, ARCH, ENT0, TRN0, REN0, CNS0, and REQ0 once F4-F6 reports are complete;
- CXT v2 integer/beat expansion and the measured 40,000-entity low-reuse capacity profile;
- CXC v1 `cuexis.chart-entry.v1` extension mapping with a required Packed playback entry.

## Explicitly not handed off

Formal v5 default Writer, formal CXC playback release, Hold/Release, Slide/Flick, multi-touch,
undefined BEH0/BHD0/ANM0/FXS0, runtime CXT expansion, and any unfrozen decoded/prepare budget.

The dated completion report must replace this readiness note only after hosted evidence and owner
acceptance are attached.

# Chart Format Foundation F4-F6: Packed Tables and IO

日期：2026-09-07  
状态：implemented candidate; hosted closure pending

## Implementation

F4 adds the candidate revision 1 Packed tables and streams: `STR0`, `REF0`, `META`, `TIME`,
`IDN0`, `ARCH`, `ENT0`, `TRN0`, `REN0`, `CAM0`, `CNS0`, and `REQ0`. The encoder emits the
96-byte header and 32-byte directory entries with CRC32, deterministic ordering, checked ranges,
and the Foundation static Tap/point/lane/press profile. The decoder reconstructs a typed
`CanonicalSemanticChart`; it never executes CXT.

F5 adds exact sizing and atomic output through `PackedChartWriter::size` and
`PackedChartWriter::writeAtomic`. Temporary files are exclusively created and flushed before
replacement; encoding, budget, or replacement failures do not publish a partial candidate.

F6 adds bounded file and byte readers. Header, directory, section CRC, known-section, count,
identity, parent graph, component stream, requirement profile, and 16 MiB gates run before the
semantic model is returned.

## Evidence

The MinGW headless chart executable passed 162 test cases and 1,136 assertions on 2026-09-07,
including Packed round trips, truncation/file-budget rejection, static Tap restoration, and the
40,000 low-reuse capacity test. The low-reuse measurement was 40,000 entities and requirements,
1,232,408 Packed bytes, and 1,232,024 decoded section bytes.

Invocation:

```powershell
cmake --preset mingw-headless-debug --fresh
cmake --build --preset mingw-headless-debug --target cuexis_chart_tests
.\out\build\mingw-headless-debug\bin\cuexis_chart_tests.exe "[packed]" --reporter compact
```

## Compatibility and gaps

Chart v4, CXT v1, CXC v1, and the default Playback path are unchanged. Unsupported sections and
future requirement kinds are rejected rather than treated as opaque payloads. Hosted MSVC,
MinGW, and Linux same-revision verification, high-reuse/bounded-mixed raw measurements, and
owner acceptance remain required before Foundation closure.

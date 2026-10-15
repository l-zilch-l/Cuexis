# RPA F3 Baseline Measurement

Date: 2026-10-15

## Declared current baseline

- Content target: 40,000 expanded semantic entities and 40,000 requirements.
- Packed target: one Packed Chart entry at or below 16 MiB
  (16,777,216 bytes).
- Current latency target: measured applicable Seek latency at or below 20 ms
  (20,000,000 ns).

These are the current RPA measurement targets requested for this run. They
are not final post-PD production limits. The planned PD judgement refactor
changes the implementation under test, so affected measurements must be
repeated after PD before final limits are accepted.

## Capacity evidence

The Release capacity probe was run on both MSVC and MinGW. Both reported
status: ok for the 40,000-entity low-reuse and high-reuse profiles.

Observed Packed sizes were:

| Profile | Entities | Requirements | Packed bytes | Result |
| --- | ---: | ---: | ---: | --- |
| low-reuse-v1 | 40,000 | 40,000 | 1,232,408 | within 16 MiB |
| high-reuse-v1 | 40,000 | 40,000 | 1,247,446 | within 16 MiB |

The largest observed high-reuse operation on MSVC Release was 542,007 us for
sizing, 538,146 us for encoding, 359,402 us for decoding, and 478,656 us for
re-encoding. These are capacity/serialization timings, not Seek latency.

Machine-readable evidence:

- out/rpa-final-20261015/f3-chart-capacity-msvc-release.json
- out/rpa-final-20261015/f3-chart-capacity-mingw-release.json

## Runtime budget evidence

The serial budget runner executed 30 workloads, three repetitions each, across
candidate Debug, candidate Release, MinGW Release, representative content, and
history pressure. Every process exited with code 0 and the source SHA was
unchanged.

Machine-readable evidence:

out/rpa-final-20261015/serial-budgets-053d3848/measurements.json

The process working-set and commit values are observation-only process peaks.
They are not prepare-only heap budgets. The runner does not establish a valid
end-to-end Seek latency measurement.

## F3 disposition

The current RPA baseline measurement is complete for the available automated
paths. The following remain unaccepted or unmeasured:

- end-to-end maxSeekLatency against the 20 ms target;
- prepare-only peak heap;
- decoded semantic heap;
- real-device timing and listening synchronization;
- final production threshold acceptance.

No value in this report is a final post-PD conclusion.

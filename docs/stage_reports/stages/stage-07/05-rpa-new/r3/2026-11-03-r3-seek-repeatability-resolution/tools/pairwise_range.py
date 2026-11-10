"""Distribution of the two-process relative range over the registered resolution group.

The archived v4 Seek group compared two processes per arm, so its (max-min)/median used two
samples. That is not the plan 9.2 statistic, which is defined per window of three processes. This
script answers the question the mismatch raises: over the nine registered processes of one
unchanged binary, how often would a two-process draw itself exceed 15 percent?

Every value comes from `results.json`; no new process is run and no reading is dropped.

    python -B pairwise_range.py
"""

from __future__ import annotations

import itertools
import json
import statistics as stats
from pathlib import Path

HERE = Path(__file__).resolve().parent
V4_RANGE = 22.4329  # the 1K H902 candidate figure that stopped the archived group


def main() -> int:
    document = json.loads((HERE / "results.json").read_text(encoding="utf-8"))
    print("| records | 量 | 两两组合 | 最小 | 中位 | 最大 | >15% 的组合 | >22.43% 的组合 |")
    print("| ---: | --- | ---: | ---: | ---: | ---: | ---: | ---: |")
    for scale in sorted(document["verdicts"]):
        for name, quantity in document["verdicts"][scale]["quantities"].items():
            values = quantity["values_us"]
            ranges = [abs(a - b) / stats.median([a, b]) * 100.0
                      for a, b in itertools.combinations(values, 2)]
            ranges.sort()
            print("| {} | {} | {} | {:.2f}% | {:.2f}% | {:.2f}% | {} | {} |".format(
                scale, name, len(ranges), ranges[0], stats.median(ranges), ranges[-1],
                sum(1 for value in ranges if value > 15.0),
                sum(1 for value in ranges if value > V4_RANGE)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

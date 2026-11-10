"""Apply the corrected R3 repeatability reading to the archived legacy parse-once decision.

The archived group was not selected because its repeatability screen failed. Every arm preserved
its samples, so the same readings can be summarised by the statistic the plan actually uses for an
A/B decision: the paired log ratio with a Student-t interval. This script only recomputes; it does
not select the prototype, accept a load budget, or reopen any closed gate.

    python -B disclose_parse_once.py
"""

from __future__ import annotations

import json
import math
import statistics as stats
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = next(parent for parent in HERE.parents if (parent / ".git").exists())
SOURCE = (ROOT / "docs/stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-31-startup-evidence"
          / "legacy-parse-once-measure-v1" / "decision.json")
T975 = {1: 12.706204736174705, 2: 4.302652729911275, 3: 3.182446305284263,
        4: 2.7764451051977987, 5: 2.570581835636314}


def main() -> int:
    decision = json.loads(SOURCE.read_text(encoding="utf-8"))
    print("| 规模 | baseline 中位 ms | baseline 极差 | candidate 中位 ms | candidate 极差 | 变化 | 配对比值 [95% 区间] |")
    print("| ---: | ---: | ---: | ---: | ---: | ---: | --- |")
    document = {"source": str(SOURCE.relative_to(ROOT)).replace("\\", "/"), "scales": []}
    for entry in decision["summary"]:
        arms = entry["values"]
        base = arms["baseline"]
        cand = arms["candidate"]
        logs = [math.log(c / b) for b, c in zip(base["samples_us"], cand["samples_us"])]
        mean = stats.mean(logs)
        half = T975[len(logs) - 1] * stats.stdev(logs) / math.sqrt(len(logs))
        ratio = math.exp(mean)
        lower, upper = math.exp(mean - half), math.exp(mean + half)
        document["scales"].append({
            "n": entry["n"],
            "baseline_median_us": base["median_us"],
            "baseline_relative_range_pct": round(base["relative_range_percent"], 4),
            "candidate_median_us": cand["median_us"],
            "candidate_relative_range_pct": round(cand["relative_range_percent"], 4),
            "change_pct": round(entry["change_percent"], 4),
            "paired_ratio": round(ratio, 4),
            "interval": [round(lower, 4), round(upper, 4)],
            "df": len(logs) - 1,
            "upper_within_1_10": upper <= 1.10,
        })
        print("| {} | {:.1f} | {:.2f}% | {:.1f} | {:.2f}% | {:+.2f}% | {:.4f} [{:.4f}, {:.4f}] "
              "df={} |".format(
                  entry["n"], base["median_us"] / 1000.0, base["relative_range_percent"],
                  cand["median_us"] / 1000.0, cand["relative_range_percent"],
                  entry["change_percent"], ratio, lower, upper, len(logs) - 1))
    document["numeric_gate"] = decision.get("numeric_gate")
    document["repeatability_screen"] = decision.get("repeatability_screen")
    document["selected"] = decision.get("selected")
    (HERE / "parse-once-disclosure.json").write_text(
        json.dumps(document, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

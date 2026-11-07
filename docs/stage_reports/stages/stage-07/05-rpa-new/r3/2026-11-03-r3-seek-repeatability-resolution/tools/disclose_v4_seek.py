"""Recompute the archived R3c v4 Seek readings under the plan 9.2 statistics.

Reads the committed `v4-seek-results.json` and prints, for every scenario and arm, the
per-process medians, the relative range (max-min)/median, the same two numbers with the first
attempt removed, the paired log ratio with its Student-t interval, and the absolute gap. Nothing
here decides a gate: the group is two processes per arm, so the interval is reported only to show
how wide it is at n=2.

    python -B disclose_v4_seek.py

Writes `disclosure.json` next to the evidence directory and prints the Markdown table.
"""

from __future__ import annotations

import json
import math
import statistics as stats
from pathlib import Path

HERE = Path(__file__).resolve().parent
EVIDENCE = HERE.parent
SOURCE = EVIDENCE.parent / "2026-10-30-resume-evidence" / "v4-seek-results.json"
OUTPUT = HERE / "disclosure.json"

# Two-sided 95 percent Student-t quantiles. The group has two process pairs, so df=1.
T975 = {1: 12.706204736174705, 2: 4.302652729911275, 3: 3.182446305284263,
        4: 2.7764451051977987, 5: 2.570581835636314, 6: 2.446911848791681,
        7: 2.3646242510102993, 8: 2.306004135204166}


def relative_range(values: list[float]) -> float:
    return (max(values) - min(values)) / stats.median(values)


def t_interval(log_ratios: list[float]) -> dict:
    count = len(log_ratios)
    mean = stats.mean(log_ratios)
    if count < 2:
        return {"n": count, "log_mean": mean, "lower": None, "upper": None, "df": 0}
    deviation = stats.stdev(log_ratios)
    half = T975[count - 1] * deviation / math.sqrt(count)
    return {
        "n": count,
        "df": count - 1,
        "log_mean": mean,
        "ratio": math.exp(mean),
        "lower": math.exp(mean - half),
        "upper": math.exp(mean + half),
    }


def summarise(processes: list[dict], field: str) -> dict | None:
    values = [process[field] for process in processes if process[field] is not None]
    if len(values) < 2:
        return None
    return {
        "n": len(values),
        "values_us": [round(value, 3) for value in values],
        "min_us": round(min(values), 3),
        "median_us": round(stats.median(values), 3),
        "max_us": round(max(values), 3),
        "relative_range_pct": round(relative_range(values) * 100.0, 4),
    }


def main() -> int:
    records = json.loads(SOURCE.read_text(encoding="utf-8"))
    scenarios: dict[tuple[int, str], dict[str, list[dict]]] = {}
    for record in records:
        key = (record["n"], str(record["horizon"]))
        arm = scenarios.setdefault(key, {}).setdefault(record["variant"], [])
        events = sorted(record["events"], key=lambda event: event["attempt"])
        all_values = [event["request_to_present_us"] for event in events]
        repeats = all_values[1:]
        arm.append({
            "pair": record["pair"],
            "valid": record.get("valid"),
            "attempts_us": all_values,
            "first_us": all_values[0],
            "median_all_us": stats.median(all_values),
            "median_repeat_us": stats.median(repeats) if repeats else None,
            "max_all_us": max(all_values),
        })

    table: list[dict] = []
    for (scale, horizon), arms in sorted(scenarios.items()):
        entry = {"n": scale, "horizon": horizon, "arms": {}}
        for variant in ("baseline", "candidate"):
            processes = sorted(arms.get(variant, []), key=lambda item: item["pair"])
            entry["arms"][variant] = {
                "processes": processes,
                "all": summarise(processes, "median_all_us"),
                "repeat": summarise(processes, "median_repeat_us"),
            }
        pairs = len(entry["arms"]["baseline"]["processes"])
        if pairs and entry["arms"]["candidate"]["processes"]:
            logs = []
            for base, cand in zip(entry["arms"]["baseline"]["processes"],
                                  entry["arms"]["candidate"]["processes"]):
                if base["median_all_us"] > 0 and cand["median_all_us"] > 0:
                    logs.append(math.log(cand["median_all_us"] / base["median_all_us"]))
            entry["paired_ratio"] = t_interval(logs)
            base_median = stats.median([item["median_all_us"]
                                        for item in entry["arms"]["baseline"]["processes"]])
            cand_median = stats.median([item["median_all_us"]
                                        for item in entry["arms"]["candidate"]["processes"]])
            entry["absolute_gap_us"] = round(cand_median - base_median, 3)
            entry["absolute_gap_pct"] = round(
                (cand_median - base_median) / base_median * 100.0, 4)
        table.append(entry)

    document = {
        "source": str(SOURCE.relative_to(EVIDENCE.parents[5])).replace("\\", "/"),
        "statistic": "(max-min)/median over the per-process medians; plan 9.2 defines it per "
                     "formal window of three processes and over the merged nine",
        "caveat": "Each arm has two processes, below the plan's three per window. A range over two "
                  "samples is not the plan's statistic and the paired interval at df=1 is too wide "
                  "to decide anything; both are reported as disclosure only.",
        "t_quantile_975": T975[1],
        "scenarios": table,
    }
    OUTPUT.write_text(json.dumps(document, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    print("| 规模 | horizon | 臂 | 每进程中位 µs（全部） | 相对极差（全部） | 每进程中位 µs（仅重复） | 相对极差（仅重复） |")
    print("| ---: | ---: | --- | --- | ---: | --- | ---: |")
    for entry in table:
        for variant in ("baseline", "candidate"):
            arm = entry["arms"][variant]
            if not arm["all"]:
                continue
            all_values = " / ".join(str(round(value, 2)) for value in arm["all"]["values_us"])
            repeat = arm["repeat"]
            repeat_values = (" / ".join(str(round(value, 2)) for value in repeat["values_us"])
                             if repeat else "-")
            repeat_range = f'{repeat["relative_range_pct"]:.2f}%' if repeat else "-"
            print("| {} | {} | {} | {} | {:.2f}% | {} | {} |".format(
                entry["n"], entry["horizon"], variant, all_values,
                arm["all"]["relative_range_pct"], repeat_values, repeat_range))
    print()
    for entry in table:
        ratio = entry.get("paired_ratio")
        if not ratio or ratio["upper"] is None:
            continue
        print("| {} / {} | {:.4f} | {:.4f} – {:.4f} | {} | {:+.3f} µs |".format(
            entry["n"], entry["horizon"], ratio["ratio"], ratio["lower"], ratio["upper"],
            "df=1", entry["absolute_gap_us"]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

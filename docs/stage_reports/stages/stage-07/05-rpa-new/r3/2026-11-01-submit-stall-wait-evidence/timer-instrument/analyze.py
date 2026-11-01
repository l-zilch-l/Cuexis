"""Analysis of the GPU-timer diagnostic: is the >6 ms frame waiting on the GPU?

Reads the three run directories produced by run_stall_diagnosis.py at 1800, 2400 and 2800
entities and answers the pre-registered questions in PLAN.md:

  P1  does the per-frame GPU interval rise with N, and to what fraction of the CPU frame?
  P2  do slow frames carry a larger GPU interval than fast frames in the same process?
  P3  does 2400 static reproduce the 11-12% slow-frame rate with the timer attached?
  P4  does 1800 static stay at the rare-event level?

Only already-recorded samples are read; nothing is re-measured here.
"""

import glob
import json
import os
import statistics

BASE = r"out/verification-20261101/r3-stall-mechanism-v1"
SLOW_US = 6000.0
ENTITY_POINTS = (1800, 2400, 2800)


def load(path):
    samples = []
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            if line.startswith("GPU_THROUGHPUT_SAMPLE "):
                samples.append(json.loads(line[len("GPU_THROUGHPUT_SAMPLE ") :]))
    return samples


def quantile(values, fraction):
    ordered = sorted(values)
    return ordered[min(len(ordered) - 1, max(0, int(round(fraction * (len(ordered) - 1)))))]


def mean_rank(values):
    order = sorted(range(len(values)), key=lambda index: values[index])
    ranks = [0.0] * len(values)
    index = 0
    while index < len(order):
        end = index
        while end + 1 < len(order) and values[order[end + 1]] == values[order[index]]:
            end += 1
        rank = (index + end) / 2.0
        for position in range(index, end + 1):
            ranks[order[position]] = rank
        index = end + 1
    return ranks


def spearman(pairs):
    if len(pairs) < 3:
        return None
    left = mean_rank([pair[0] for pair in pairs])
    right = mean_rank([pair[1] for pair in pairs])
    left_mean = statistics.mean(left)
    right_mean = statistics.mean(right)
    numerator = sum((a - left_mean) * (b - right_mean) for a, b in zip(left, right))
    denominator = (
        sum((a - left_mean) ** 2 for a in left) * sum((b - right_mean) ** 2 for b in right)
    ) ** 0.5
    return None if denominator == 0 else numerator / denominator


def describe(path):
    samples = load(path)
    if not samples:
        return None
    cycles = [s["cycle_us"] for s in samples]
    submits = [s["submit_us"] for s in samples]
    gpus = [s.get("gpu_interval_us") for s in samples]
    if any(value is None for value in gpus):
        return {"log": os.path.basename(path), "error": "sample without gpu_interval_us"}
    slow = [s for s in samples if s["cycle_us"] > SLOW_US]
    fast = [s for s in samples if s["cycle_us"] <= SLOW_US]
    record = {
        "log": os.path.basename(path),
        "frames": len(samples),
        "cycle_us": {
            "p50": quantile(cycles, 0.50),
            "p99": quantile(cycles, 0.99),
            "max": max(cycles),
        },
        "submit_us": {"p50": quantile(submits, 0.50), "p99": quantile(submits, 0.99)},
        "gpu_us": {
            "p50": quantile(gpus, 0.50),
            "p90": quantile(gpus, 0.90),
            "p99": quantile(gpus, 0.99),
            "max": max(gpus),
        },
        "gpu_share_of_cycle_p50": quantile(gpus, 0.50) / quantile(cycles, 0.50),
        "gpu_vs_cycle_spearman": round(spearman(list(zip(gpus, cycles))) or 0.0, 4),
        "slow_frames": len(slow),
        "slow_rate": round(len(slow) / len(samples), 5),
    }
    if slow:
        slow_gpu = statistics.median([s["gpu_interval_us"] for s in slow])
        fast_gpu = statistics.median([s["gpu_interval_us"] for s in fast]) if fast else None
        record["slow_vs_fast"] = {
            "slow_gpu_median": round(slow_gpu, 1),
            "fast_gpu_median": round(fast_gpu, 1) if fast_gpu is not None else None,
            "slow_gpu_ratio": round(slow_gpu / fast_gpu, 3) if fast_gpu else None,
            "slow_submit_median": round(statistics.median([s["submit_us"] for s in slow]), 1),
            "fast_submit_median": round(statistics.median([s["submit_us"] for s in fast]), 1)
            if fast
            else None,
            "slow_cycle_median": round(statistics.median([s["cycle_us"] for s in slow]), 1),
            "fast_cycle_median": round(statistics.median([s["cycle_us"] for s in fast]), 1)
            if fast
            else None,
            "slow_present_median": round(statistics.median([s["present_us"] for s in slow]), 1),
            "fast_present_median": round(statistics.median([s["present_us"] for s in fast]), 1)
            if fast
            else None,
        }
    return record


def main():
    results = {}
    for entities in ENTITY_POINTS:
        directory = os.path.join(BASE, "n{}".format(entities))
        if not os.path.isdir(directory):
            continue
        for mode in ("render-only", "static"):
            paths = sorted(glob.glob(os.path.join(directory, "*-{}-{}.log.txt".format(mode, entities))))
            records = [describe(path) for path in paths]
            records = [record for record in records if record]
            if not records:
                continue
            results["{}|{}".format(entities, mode)] = records
            print("== entities {} / {} ({} processes)".format(entities, mode, len(records)))
            for record in records:
                if "error" in record:
                    print("   {}: {}".format(record["log"], record["error"]))
                    continue
                print(
                    "   {}  n={}  cycle p50 {:.0f} p99 {:.0f}  submit p50 {:.0f}  "
                    "gpu p50 {:.0f} p90 {:.0f} p99 {:.0f}  gpu/cycle {:.2f}  rho {:.2f}  "
                    "slow {}/{} {:.2%}".format(
                        record["log"],
                        record["frames"],
                        record["cycle_us"]["p50"],
                        record["cycle_us"]["p99"],
                        record["submit_us"]["p50"],
                        record["gpu_us"]["p50"],
                        record["gpu_us"]["p90"],
                        record["gpu_us"]["p99"],
                        record["gpu_share_of_cycle_p50"],
                        record["gpu_vs_cycle_spearman"],
                        record["slow_frames"],
                        record["frames"],
                        record["slow_rate"],
                    )
                )
                if "slow_vs_fast" in record:
                    detail = record["slow_vs_fast"]
                    print(
                        "        slow gpu {} vs fast {} (x{})   slow cycle {} vs fast {}   "
                        "slow submit {} vs fast {}".format(
                            detail["slow_gpu_median"],
                            detail["fast_gpu_median"],
                            detail["slow_gpu_ratio"],
                            detail["slow_cycle_median"],
                            detail["fast_cycle_median"],
                            detail["slow_submit_median"],
                            detail["fast_submit_median"],
                        )
                    )
            usable = [record for record in records if "error" not in record]
            if usable:
                summary = {
                    "cycle_p50": statistics.median([r["cycle_us"]["p50"] for r in usable]),
                    "gpu_p50": statistics.median([r["gpu_us"]["p50"] for r in usable]),
                    "gpu_p90": statistics.median([r["gpu_us"]["p90"] for r in usable]),
                    "gpu_share_p50": statistics.median(
                        [r["gpu_share_of_cycle_p50"] for r in usable]),
                    "slow_rate": statistics.median([r["slow_rate"] for r in usable]),
                }
                results["{}|{}|summary".format(entities, mode)] = summary
                print("   median of processes: cycle {:.0f}  gpu {:.0f} (p90 {:.0f})  "
                      "share {:.2f}  slow {:.2%}".format(
                          summary["cycle_p50"], summary["gpu_p50"], summary["gpu_p90"],
                          summary["gpu_share_p50"], summary["slow_rate"]))
            print()
    with open(os.path.join(BASE, "analysis.json"), "w", encoding="utf-8") as stream:
        json.dump(results, stream, indent=2, ensure_ascii=False)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

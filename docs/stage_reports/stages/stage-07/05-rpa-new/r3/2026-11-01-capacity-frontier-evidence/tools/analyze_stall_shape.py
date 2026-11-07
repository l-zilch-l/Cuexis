"""Describe the shape of the p99 crossing from the per-frame samples a run already recorded.

The ladder answers "does this entity count pass?" per point. This script answers the next question
without measuring anything new: *how* does the point fail? It reads only the `GPU_THROUGHPUT_SAMPLE`
lines of an existing run directory and reports, per process and per mode,

  * the cycle-time distribution (p50/p90/p95/p99 and the share of frames over the 4.166667 ms
    budget and over a `--threshold-us` "slow frame" line),
  * how the slow frames are grouped in time (burst lengths, gaps between burst starts),
  * the per-phase medians (`update/extract/submit/present/total/cycle`, plus `gpu_interval_us` when
    the run carried GPU timestamps) for the fast frames and for the slow frames separately.

The last item is the point of the script: it attributes the slow frames to a phase instead of
leaving "p99 is high" as an unlabelled number. It never re-samples, never drops a process, and
never re-judges pass/fail — the verdict stays the ladder's, and this file only describes it.

    python -B analyze_stall_shape.py --run frontier-bracket-4/entities-2300 --out shapes/2300.json
"""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import statistics
from pathlib import Path

BUDGET_US = 1000000.0 / 240.0
SAMPLE_TAG = "GPU_THROUGHPUT_SAMPLE "
PHASES = ("update_us", "extract_us", "submit_us", "present_us", "total_us", "cycle_us")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as stream:
        for chunk in iter(lambda: stream.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def percentile(values, fraction):
    ordered = sorted(values)
    return ordered[int(fraction * (len(ordered) - 1))]


def median(values):
    return statistics.median(values) if values else None


def load(path: Path):
    samples = []
    with open(path, encoding="utf-8", errors="replace") as stream:
        for line in stream:
            index = line.find(SAMPLE_TAG)
            if index < 0:
                continue
            samples.append(json.loads(line[index + len(SAMPLE_TAG):]))
    return samples


def bursts(flags):
    """Maximal runs of consecutive true values: their lengths and their start ordinals."""
    lengths = []
    starts = []
    run = 0
    for position, flag in enumerate(flags):
        if flag:
            if run == 0:
                starts.append(position)
            run += 1
        elif run:
            lengths.append(run)
            run = 0
    if run:
        lengths.append(run)
    return lengths, starts


def describe(samples, threshold):
    cycle = [sample["cycle_us"] for sample in samples]
    slow_flags = [value > threshold for value in cycle]
    over_flags = [value > BUDGET_US for value in cycle]
    slow = [sample for sample, flag in zip(samples, slow_flags) if flag]
    fast = [sample for sample, flag in zip(samples, slow_flags) if not flag]
    lengths, starts = bursts(slow_flags)
    over_lengths, over_starts = bursts(over_flags)
    gaps = [later - earlier for earlier, later in zip(starts, starts[1:])]
    over_gaps = [later - earlier for earlier, later in zip(over_starts, over_starts[1:])]
    phases = {phase.split("_")[0]: median([sample[phase] for sample in samples])
              for phase in PHASES}
    detail = {
        "frames": len(samples),
        "start_utc_s": samples[0].get("end_us", 0.0) / 1e6,
        "end_s": samples[-1].get("end_us", 0.0) / 1e6,
        "cycle_us": {
            "min": min(cycle),
            "p50": percentile(cycle, 0.50),
            "p90": percentile(cycle, 0.90),
            "p95": percentile(cycle, 0.95),
            "p99": percentile(cycle, 0.99),
            "max": max(cycle),
        },
        "over_budget": {
            "count": sum(over_flags),
            "share": sum(over_flags) / len(samples),
            "bursts": len(over_lengths),
            "burst_length_median": median(over_lengths),
            "burst_length_max": max(over_lengths) if over_lengths else 0,
            "gap_frames_median": median(over_gaps),
            "first_ordinal": over_starts[0] if over_starts else None,
            "first_utc_s": (samples[over_starts[0]]["end_us"] / 1e6
                            if over_starts else None),
        },
        "slow": {
            "threshold_us": threshold,
            "count": len(slow),
            "share": len(slow) / len(samples),
            "bursts": len(lengths),
            "burst_length_median": median(lengths),
            "burst_length_max": max(lengths) if lengths else 0,
            "gap_frames_median": median(gaps),
            "gap_frames_p90": percentile(gaps, 0.90) if gaps else None,
            "first_ordinal": starts[0] if starts else None,
            "first_utc_s": samples[starts[0]]["end_us"] / 1e6 if starts else None,
        },
        "phases_us": phases,
        "fast_phases_us": {phase.split("_")[0]: median([sample[phase] for sample in fast])
                           for phase in PHASES} if fast else None,
        "slow_phases_us": {phase.split("_")[0]: median([sample[phase] for sample in slow])
                           for phase in PHASES} if slow else None,
    }
    if samples and "gpu_interval_us" in samples[0]:
        detail["gpu_interval_us"] = {
            "p50": percentile([sample["gpu_interval_us"] for sample in samples], 0.50),
            "p99": percentile([sample["gpu_interval_us"] for sample in samples], 0.99),
            "fast_p50": (median([sample["gpu_interval_us"] for sample in fast])
                         if fast else None),
            "slow_p50": (median([sample["gpu_interval_us"] for sample in slow])
                         if slow else None),
            "max": max(sample["gpu_interval_us"] for sample in samples),
        }
    return detail


def mode_of(name: str) -> str:
    if "render-only" in name:
        return "render-only"
    if "-static-" in name:
        return "static"
    return "unknown"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--run", required=True, help="run directory holding *.log.txt")
    parser.add_argument("--out", required=True, help="JSON file to write")
    parser.add_argument("--threshold-us", type=float, default=6000.0)
    args = parser.parse_args()

    run = Path(args.run).resolve()
    out = Path(args.out).resolve()
    if not run.is_dir():
        raise SystemExit("run directory not found: {}".format(run))
    logs = sorted(path for path in run.glob("*.log.txt"))
    if not logs:
        raise SystemExit("no *.log.txt under {}".format(run))

    processes = []
    for log in logs:
        samples = load(log)
        if not samples:
            raise SystemExit("no samples in {}".format(log.name))
        processes.append({
            "log": log.name,
            "log_sha256": sha256(log),
            "mode": mode_of(log.name),
            "ordinal_continuous": [sample["ordinal"] for sample in samples]
            == list(range(len(samples))),
            "detail": describe(samples, args.threshold_us),
        })

    groups = {}
    for mode in ("render-only", "static"):
        subset = [process for process in processes if process["mode"] == mode]
        if not subset:
            continue
        groups[mode] = {
            "processes": len(subset),
            "frames": [process["detail"]["frames"] for process in subset],
            "cycle_p50_us": [process["detail"]["cycle_us"]["p50"] for process in subset],
            "cycle_p99_us": [process["detail"]["cycle_us"]["p99"] for process in subset],
            "over_budget_share": [process["detail"]["over_budget"]["share"]
                                  for process in subset],
            "slow_share": [process["detail"]["slow"]["share"] for process in subset],
            "slow_frames": [process["detail"]["slow"]["count"] for process in subset],
            "slow_bursts": [process["detail"]["slow"]["bursts"] for process in subset],
            "slow_gap_frames_median": [process["detail"]["slow"]["gap_frames_median"]
                                       for process in subset],
            "first_slow_utc_s": [process["detail"]["slow"]["first_utc_s"] for process in subset],
            "submit_us_median": [process["detail"]["phases_us"]["submit"] for process in subset],
            "slow_submit_us_median": [
                (process["detail"]["slow_phases_us"] or {}).get("submit") for process in subset],
            "fast_submit_us_median": [
                (process["detail"]["fast_phases_us"] or {}).get("submit") for process in subset],
            "all_processes_continuous": all(process["ordinal_continuous"] for process in subset),
        }
        if "gpu_interval_us" in subset[0]["detail"]:
            groups[mode]["gpu_interval_p50_us"] = [process["detail"]["gpu_interval_us"]["p50"]
                                                   for process in subset]
            groups[mode]["gpu_interval_slow_p50_us"] = [
                process["detail"]["gpu_interval_us"]["slow_p50"] for process in subset]
            groups[mode]["gpu_interval_fast_p50_us"] = [
                process["detail"]["gpu_interval_us"]["fast_p50"] for process in subset]

    payload = {
        "scope": ("shape of the p99 crossing, read from the recorded per-frame samples of a run "
                  "that was already judged; descriptive only, never a verdict"),
        "created_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "run": run.name,
        "budget_us": BUDGET_US,
        "threshold_us": args.threshold_us,
        "sample_tag": SAMPLE_TAG.strip(),
        "script_sha256": sha256(Path(__file__).resolve()),
        "processes": processes,
        "groups": groups,
    }
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    for mode, group in groups.items():
        print("{}: p50 {} p99 {} slow {} first_slow {} submit fast->slow {}".format(
            mode,
            group["cycle_p50_us"],
            group["cycle_p99_us"],
            [round(100.0 * share, 2) for share in group["slow_share"]],
            [None if value is None else round(value, 1) for value in group["first_slow_utc_s"]],
            [None if fast is None or slow is None else round(slow - fast, 1)
             for fast, slow in zip(group["fast_submit_us_median"],
                                   group["slow_submit_us_median"])]))
        if "gpu_interval_p50_us" in group:
            print("  gpu_interval p50 {} fast {} slow {}".format(
                group["gpu_interval_p50_us"],
                group["gpu_interval_fast_p50_us"],
                group["gpu_interval_slow_p50_us"]))
    print(str(out))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

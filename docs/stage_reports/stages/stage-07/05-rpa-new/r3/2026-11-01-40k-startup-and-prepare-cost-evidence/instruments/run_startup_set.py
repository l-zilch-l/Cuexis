"""Run a pre-registered startup-observability set and summarise it.

Each sample is one fresh probe process. Nothing is re-sampled or excluded: every process is
recorded, and the summary reports the per-arm median together with the measured spread so a
reader can judge repeatability without trusting a single number.

Usage (from D:\\Cuexis-redesign):
    python -B run_startup_set.py --stamp 20261101-a
"""

import argparse
import json
import os
import pathlib
import statistics
import subprocess
import sys
import time

EXE = pathlib.Path("out/build/rpa-candidate-release/bin/cuexis_execution_device_probe.exe")
TEST_NAME = "GPU visible entity throughput probe"
ROOT = pathlib.Path("out/verification-20261101/r3-startup-observability-v1")

STARTUP_STAGES = [
    "fixture_source",
    "playback_prepare",
    "sdl_runtime",
    "gl_context",
    "window_create",
    "backend_create",
    "pixel_readback",
    "renderer_prepare",
    "session_commit",
    "backend_activate",
    "first_update",
    "first_extract",
    "first_submit",
    "first_present",
]

# arm label -> (entities, traced, mode, frames, warmup)
ARMS = {
    "A-untraced-40000": (40000, False, "static", 3, 2),
    "B-traced-40000": (40000, True, "static", 3, 2),
    "C-traced-1800": (1800, True, "static", 3, 2),
    "D-traced-2400": (2400, True, "static", 3, 2),
    "E-traced-2800": (2800, True, "static", 3, 2),
    "F-traced-10000": (10000, True, "static", 3, 2),
}

PLAN = {
    "20261101-a": [("A-untraced-40000", 6), ("B-traced-40000", 6)],
    "20261101-a2": [("A-untraced-40000", 8), ("B-traced-40000", 8)],
    "20261101-b": [("C-traced-1800", 3), ("D-traced-2400", 3), ("E-traced-2800", 3),
                   ("F-traced-10000", 3)],
}

# Stamps whose arms alternate sample by sample (A,B,A,B,...). Grouping one arm's processes together
# leaves the first arm exposed to cold-cache effects, which is exactly the confound this stamp exists
# to remove.
INTERLEAVED = {"20261101-a2"}


def parse_startup(text: str) -> tuple[dict, dict]:
    """Return (stage -> microseconds, extra GPU_STARTUP_* payloads)."""
    stages: dict[str, float] = {}
    extra: dict[str, object] = {}
    for line in text.splitlines():
        if line.startswith("GPU_STARTUP_END "):
            parts = line.split()
            stages[parts[1]] = float(parts[2])
        elif line.startswith("GPU_STARTUP_WINDOW "):
            extra["window"] = json.loads(line[len("GPU_STARTUP_WINDOW "):])
        elif line.startswith("GPU_STARTUP_SHOWN "):
            extra["shown_us"] = float(line.split()[1])
        elif line.startswith("GPU_STARTUP_TOTAL "):
            extra["total"] = json.loads(line[len("GPU_STARTUP_TOTAL "):])
        elif line.startswith("GPU_STARTUP_HEARTBEAT "):
            parts = line.split()
            extra.setdefault("heartbeat", []).append(
                {"stage": parts[1], "stage_elapsed_us": float(parts[2]), "abs_us": float(parts[3])})
    return stages, extra


def parse_trace(text: str) -> tuple[list[dict], float]:
    """Return (events, total covered microseconds from first BEGIN to last END)."""
    events: list[dict] = []
    pending: dict[str, float] = {}
    first = None
    last = None
    for line in text.splitlines():
        parts = line.split()
        if len(parts) >= 4 and parts[0] == "PREPARE_TRACE_BEGIN":
            stage, abs_us = parts[1], float(parts[2])
            pending[stage] = abs_us
            events.append({"event": "begin", "stage": stage, "abs_us": abs_us})
            first = abs_us if first is None else first
        elif len(parts) >= 4 and parts[0] == "PREPARE_TRACE_END":
            stage, elapsed, abs_us = parts[1], float(parts[2]), float(parts[3])
            begin = pending.pop(stage, None)
            events.append({"event": "end", "stage": stage, "elapsed_us": elapsed,
                           "abs_us": abs_us, "begin_us": begin})
            last = abs_us
    span = 0.0 if first is None or last is None else last - first
    return events, span


def top_level(events: list[dict]) -> dict[str, float]:
    """Stage durations in call order, excluding nested repeats (nested scopes are reported too)."""
    result: dict[str, float] = {}
    for event in events:
        if event["event"] != "end":
            continue
        stage = event["stage"]
        result[stage] = result.get(stage, 0.0) + event["elapsed_us"]
    return result


def run_once(arm: str, index: int, stamp_dir: pathlib.Path) -> dict:
    entities, traced, mode, frames, warmup = ARMS[arm]
    env = dict(os.environ)
    env["CUEXIS_GPU_ENTITIES"] = str(entities)
    env["CUEXIS_GPU_MODE"] = mode
    env["CUEXIS_GPU_FRAMES"] = str(frames)
    env["CUEXIS_GPU_WARMUP"] = str(warmup)
    env["CUEXIS_GPU_TIMER"] = "0"
    env.pop("CUEXIS_PREPARE_TRACE", None)
    if traced:
        env["CUEXIS_PREPARE_TRACE"] = "1"
    started = time.perf_counter()
    completed = subprocess.run([str(EXE), TEST_NAME], env=env, capture_output=True, text=True,
                               timeout=600)
    wall = time.perf_counter() - started
    sample_dir = stamp_dir / "raw"
    sample_dir.mkdir(parents=True, exist_ok=True)
    stem = f"{arm}-{index:02d}"
    (sample_dir / f"{stem}.out.txt").write_bytes(completed.stdout.encode("utf-8"))
    (sample_dir / f"{stem}.err.txt").write_bytes(completed.stderr.encode("utf-8"))
    stages, extra = parse_startup(completed.stdout)
    events, span = parse_trace(completed.stderr)
    return {
        "arm": arm,
        "index": index,
        "entities": entities,
        "mode": mode,
        "traced": traced,
        "returncode": completed.returncode,
        "wall_s": round(wall, 3),
        "stages_us": stages,
        "extra": extra,
        "trace_events": events,
        "trace_stages_us": top_level(events),
        "trace_span_us": span,
    }


def summarise(samples: list[dict]) -> dict:
    arms: dict[str, dict] = {}
    for sample in samples:
        arm = sample["arm"]
        entry = arms.setdefault(arm, {"samples": [], "stages": {}, "trace": {}})
        entry["samples"].append({
            "index": sample["index"],
            "returncode": sample["returncode"],
            "wall_s": sample["wall_s"],
            "first_present_us": sample["extra"].get("total", {}).get("first_present_us"),
            "shown_us": sample["extra"].get("shown_us"),
        })
        for stage, value in sample["stages_us"].items():
            entry["stages"].setdefault(stage, []).append(value)
        for stage, value in sample["trace_stages_us"].items():
            entry["trace"].setdefault(stage, []).append(value)
    summary: dict[str, dict] = {}
    for arm, entry in arms.items():
        stages = {}
        for stage, values in entry["stages"].items():
            median = statistics.median(values)
            stages[stage] = {"median": round(median, 1), "min": round(min(values), 1),
                             "max": round(max(values), 1),
                             "spread_pct": round(100.0 * (max(values) - min(values)) / median, 2)
                             if median else None, "n": len(values)}
        trace = {}
        for stage, values in entry["trace"].items():
            median = statistics.median(values)
            trace[stage] = {"median": round(median, 1), "min": round(min(values), 1),
                            "max": round(max(values), 1),
                            "spread_pct": round(100.0 * (max(values) - min(values)) / median, 2)
                            if median else None, "n": len(values)}
        summary[arm] = {"samples": entry["samples"], "stages": stages, "trace_stages": trace}
    return summary


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--stamp", required=True)
    args = parser.parse_args()
    plan = PLAN[args.stamp]
    stamp_dir = ROOT / "runs" / args.stamp
    stamp_dir.mkdir(parents=True, exist_ok=True)
    samples: list[dict] = []
    if args.stamp in INTERLEAVED:
        arm_cycles = [(arm, index) for index in range(max(count for _, count in plan))
                      for arm, count in plan if index < count]
    else:
        arm_cycles = [(arm, index) for arm, count in plan for index in range(count)]
    for arm, index in arm_cycles:
        sample = run_once(arm, index, stamp_dir)
        samples.append(sample)
        total = sample["extra"].get("total", {}).get("first_present_us")
        print(f"{arm} #{index}: rc={sample['returncode']} "
              f"prepare={sample['stages_us'].get('playback_prepare')} total={total} "
              f"wall={sample['wall_s']}", flush=True)
    summary = {"stamp": args.stamp, "exe": str(EXE),
               "exe_sha256": __import__("hashlib").sha256(EXE.read_bytes()).hexdigest(),
               "arms": {arm: list(spec) for arm, spec in ARMS.items()},
               "plan": {arm: count for arm, count in plan},
               "summary": summarise(samples), "samples": samples}
    (stamp_dir / "startup-set.json").write_text(json.dumps(summary, indent=1), encoding="utf-8")
    print(json.dumps({arm: data["stages"].get("playback_prepare") for arm, data in
                      summary["summary"].items()}, indent=1))
    return 0


if __name__ == "__main__":
    sys.exit(main())

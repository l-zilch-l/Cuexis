"""Run the second R3 gate batch: PD5 controls and stream on the device probe.

Instrument: cuexis_execution_device_probe.exe, which compiles tests/pd/pd5_device_probe.cpp with
CUEXIS_PD5_DEVICE and therefore drives the real OpenGL backend and the GPU timer query.

    python -B run_pd5_gates.py

Every process is fresh. Nothing is retried; a process that fails a validity check is recorded and
leaves its group short.
"""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import math
from pathlib import Path
import re
import statistics as stats
import subprocess
import sys
import time

ROOT = Path(r"D:\Cuexis-redesign")
EXE = ROOT / "out/build/rpa-candidate-release/bin/cuexis_execution_device_probe.exe"
HERE = Path(__file__).resolve().parent
TEST = "PD5 scripted host timing probe"
SAMPLE = re.compile(r"^PD5_SAMPLE (\{.*\})$")
ID = re.compile(r"^PD5_ID (\{.*\})$")
ENV = re.compile(r"^PD_ENV (\{.*\})$")
CONTROL_METRICS = ("pause", "resume", "reset_paused", "reset_running", "resize", "close")
STREAM_METRICS = ("request_to_ready", "gpu_render", "cpu_submit", "present_call",
                  "request_to_present_return", "load_host", "close")
BUDGET_MS = {"pause": 50, "resume": 50, "resize": 50, "reset_paused": 1000, "reset_running": 1000,
             "close": 1000, "request_to_ready": 4.166666666666667, "gpu_render": 4.166666666666667}


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def percentile(values, fraction):
    ordered = sorted(values)
    rank = max(1, math.ceil(fraction * len(ordered)))
    return ordered[rank - 1]


def statistics(values):
    if not values:
        return None
    return {
        "n": len(values),
        "min_us": round(min(values), 3),
        "median_us": round(stats.median(values), 3),
        "p95_us": round(percentile(values, 0.95), 3),
        "p99_us": round(percentile(values, 0.99), 3),
        "max_us": round(max(values), 3),
    }


def run_once(group: str, index: int, environment: dict, timeout: float) -> dict:
    log = HERE / group / f"run-{index:02d}.log.txt"
    log.parent.mkdir(parents=True, exist_ok=True)
    merged = {key: value for key, value in __import__("os").environ.items()
              if not key.startswith("CUEXIS_PD5_")}
    merged.update(environment)
    started = time.monotonic()
    with log.open("wb") as stream:
        completed = subprocess.run([str(EXE), TEST], cwd=str(EXE.parent), stdout=stream,
                                   stderr=subprocess.STDOUT, env=merged, timeout=timeout)
    wall = time.monotonic() - started
    text = log.read_bytes().decode("utf-8", "replace")
    samples, ids, envs = [], [], []
    for line in text.splitlines():
        stripped = line.strip()
        for pattern, sink in ((SAMPLE, samples), (ID, ids), (ENV, envs)):
            match = pattern.match(stripped)
            if match:
                sink.append(json.loads(match.group(1)))
    metrics = {}
    for sample in samples:
        metrics.setdefault(sample["metric"], []).append(sample["elapsed_us"])
    record = {
        "run": index,
        "group": group,
        "exit_code": completed.returncode,
        "wall_seconds": round(wall, 3),
        "metrics": {name: statistics(values) for name, values in sorted(metrics.items())},
        "metric_counts": {name: len(values) for name, values in sorted(metrics.items())},
        "id": ids[0] if ids else None,
        "environment": envs[0] if envs else None,
        "log": str(log.relative_to(ROOT)),
    }
    if group == "controls":
        # Each cycle runs pause and resume twice (before and after the paused reset) and the
        # other controls once; close happens once per process.
        expected = {"pause": 40, "resume": 40, "reset_paused": 20, "reset_running": 20,
                    "resize": 20, "close": 1}
        record["valid"] = bool(completed.returncode == 0 and ids
                               and all(record["metric_counts"].get(name) == count
                                       for name, count in expected.items()))
    else:
        record["valid"] = bool(completed.returncode == 0 and ids and ids[0].get("gpu_timer") is True
                               and all(record["metric_counts"].get(name) == 1200
                                       for name in ("request_to_ready", "gpu_render", "cpu_submit",
                                                    "present_call", "request_to_present_return")))
    return record


def verdicts(group, runs):
    if group == "controls":
        result = {}
        for name in CONTROL_METRICS:
            values = [run["metrics"][name]["max_us"] for run in runs
                      if run["valid"] and name in run["metrics"]]
            if not values:
                result[name] = {"decision": "undecided", "reason": "no valid reading"}
                continue
            worst = max(values)
            result[name] = {"budget_ms": BUDGET_MS[name], "max_ms": round(worst / 1000.0, 4),
                            "decision": "pass" if worst / 1000.0 <= BUDGET_MS[name] else "over"}
        return result
    result = {}
    for name in ("request_to_ready", "gpu_render"):
        per_process = [run["metrics"][name]["p99_us"] for run in runs
                       if run["valid"] and name in run["metrics"]]
        if not per_process:
            result[name] = {"decision": "undecided", "reason": "no valid reading"}
            continue
        worst = max(per_process)
        result[name] = {
            "budget_ms": BUDGET_MS[name],
            "p99_ms_per_process": [round(value / 1000.0, 4) for value in per_process],
            "worst_p99_ms": round(worst / 1000.0, 4),
            "decision": "pass" if worst / 1000.0 <= BUDGET_MS[name] else "over",
        }
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--runs", type=int, default=3)
    parser.add_argument("--timeout", type=float, default=1200.0)
    arguments = parser.parse_args()

    record = {
        "scope": "Second R3 gate batch: PD5 controls and stream on the device probe.",
        "instrument": {"exe": str(EXE), "sha256": sha256_file(EXE), "test": TEST},
        "head": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT).decode().strip(),
        "started_local": dt.datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        "groups": {},
    }
    plans = (
        ("controls", {"CUEXIS_PD5_MODE": "controls", "CUEXIS_PD5_CYCLES": "20",
                      "CUEXIS_PD5_SMOKE": "1"}),
        ("stream", {"CUEXIS_PD5_MODE": "stream", "CUEXIS_PD5_FRAMES": "1200",
                    "CUEXIS_PD5_WARMUP": "64", "CUEXIS_PD5_RATE": "240",
                    "CUEXIS_PD5_GPU_TIMER": "1", "CUEXIS_PD5_SMOKE": "1"}),
    )
    for group, environment in plans:
        runs = []
        for index in range(arguments.runs):
            attempt = run_once(group, index, environment, arguments.timeout)
            runs.append(attempt)
            print("{} run {:02d}: exit={} valid={} wall={}s".format(
                group, index, attempt["exit_code"], attempt["valid"], attempt["wall_seconds"]),
                flush=True)
            record["groups"][group] = {"environment": environment, "runs": runs,
                                       "verdicts": verdicts(group, runs)}
            with (HERE / "results.json").open("w", encoding="utf-8", newline="\n") as stream:
                json.dump(record, stream, ensure_ascii=False, indent=2)
                stream.write("\n")
        print(json.dumps(record["groups"][group]["verdicts"], ensure_ascii=False), flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())

"""Measure the repeatability resolution of the product Seek probe on one unchanged binary.

This is not an A/B comparison and not a gate reading. It answers one question: with a single
binary and a single scenario, how much does the per-process reading move across nine independent
processes? If one variant alone cannot stay inside (max-min)/median <= 15 percent, then no
candidate can be judged by that statistic at that magnitude, and the plan 9.2 repeatability row
cannot bind below it.

The registration is written before the first process starts, from this same script, so the
declared quantities and decision rule cannot follow the readings.

    python -B run_seek_resolution.py [--processes 9]

Writes registration.json, results.json and logs/ next to itself. Nothing is retried; a process
that fails a validity check is recorded as invalid and its group stays short.
"""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import math
import os
from pathlib import Path
import re
import statistics as stats
import subprocess
import sys
import time

HERE = Path(__file__).resolve().parent
EVIDENCE = HERE.parent
ROOT = next(parent for parent in EVIDENCE.parents if (parent / ".git").exists())
EXE = ROOT / "out/build/rpa-candidate-release/bin/cuexis_execution_device_probe.exe"
TEST = "PD5 scripted seek timing probe"
SEEK = re.compile(r"^PD5_SEEK (\{.*\})$")
IDENTITY = re.compile(r"^PD5_SEEK_ID (\{.*\})$")
ENVIRONMENT = re.compile(r"^PD_ENV (\{.*\})$")
SCALES = (129, 1024)
SEEKS_PER_PROCESS = 16
PRIMARY_SAMPLE = 16   # per-process median over all sixteen seeks
SECONDARY_SAMPLE = 3  # the archived v4 shape: three requests per process


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def relative_range(values: list[float]) -> float:
    return (max(values) - min(values)) / stats.median(values) * 100.0


def describe(values: list[float]) -> dict:
    return {
        "n": len(values),
        "values_us": [round(value, 3) for value in values],
        "min_us": round(min(values), 3),
        "median_us": round(stats.median(values), 3),
        "max_us": round(max(values), 3),
        "relative_range_pct": round(relative_range(values), 4),
        "inside_15_percent": relative_range(values) <= 15.0,
    }


def registration(processes: int) -> dict:
    return {
        "schema": "r3.seek.resolution.v1",
        "formal": False,
        "scope": "Repeatability resolution only: one unchanged product binary, no A/B, no candidate. "
                 "Measures how far a single variant's per-process reading moves across nine "
                 "independent processes so that the plan 9.2 (max-min)/median <= 15 percent row can "
                 "be judged for feasibility at sub-millisecond magnitudes.",
        "instrument": {
            "exe": str(EXE).replace("\\", "/"),
            "sha256": sha256_file(EXE),
            "test": TEST,
            "suite": "cuexis_execution_device_probe",
            "profile": "Release, STATIC, candidate ON, normal allocator, metrics/trace OFF",
            "source": {"head": subprocess.check_output(
                ["git", "rev-parse", "HEAD"], cwd=ROOT).decode().strip(),
                "dirty": subprocess.check_output(
                    ["git", "status", "--porcelain"], cwd=ROOT).decode().strip()},
        },
        "design": {
            "processes_per_scale": processes,
            "scales_records": list(SCALES),
            "seeks_per_process": SEEKS_PER_PROCESS,
            "order": "interleaved: one process per scale in turn, so both scales spread over the "
                     "same wall-clock window instead of one scale running first",
            "environment": {f"CUEXIS_PD5_SEEK_RECORDS": "scale", "CUEXIS_PD5_SEEK_COUNT":
                            str(SEEKS_PER_PROCESS)},
            "affinity": "not pinned; matches the 2026-11-01 seek probe runs that produced the "
                        "941/1012 us ledger readings",
            "no_parallel_build": True,
        },
        "quantities": {
            "primary": "per-process median of request_to_present_us over all 16 seeks, then "
                       "(max-min)/median across the nine process medians",
            "secondary": "same but the per-process median uses only the first 3 seeks, matching the "
                         "archived v4 group shape",
            "tertiary": "per-process maximum of request_to_present_us (the quantity the Seek gate "
                        "judges), then the same across-process relative range",
        },
        "decision_rule": "Declared before measurement. For each scale and quantity: a relative "
                         "range above 15 percent means the statistic cannot be satisfied by any "
                         "implementation at that magnitude, and the 9.2 row is recorded as "
                         "unattainable there rather than as a candidate defect. At or below 15 "
                         "percent the criterion stays usable at that magnitude. No threshold is "
                         "changed by this run and no candidate is accepted or rejected by it.",
        "validity": "exit code 0, one PD5_SEEK_ID line, exactly 16 PD5_SEEK lines, records equal to "
                    "the requested scale. Invalid processes are kept and never replaced.",
        "not_claimed": ["no A/B or candidate comparison", "no formal window or nine pairs",
                        "no change to the 20 ms Seek gate or its two covered classes",
                        "no device, production or stage acceptance"],
    }


def run_once(scale: int, index: int, timeout: float) -> dict:
    log = HERE / "logs" / f"records-{scale}" / f"run-{index:02d}.log.txt"
    log.parent.mkdir(parents=True, exist_ok=True)
    environment = {key: value for key, value in os.environ.items()
                   if not key.startswith("CUEXIS_PD5_")}
    environment["CUEXIS_PD5_SEEK_RECORDS"] = str(scale)
    environment["CUEXIS_PD5_SEEK_COUNT"] = str(SEEKS_PER_PROCESS)
    started = time.monotonic()
    with log.open("wb") as stream:
        completed = subprocess.run([str(EXE), TEST], cwd=str(EXE.parent), stdout=stream,
                                   stderr=subprocess.STDOUT, env=environment, timeout=timeout)
    wall = time.monotonic() - started
    text = log.read_bytes().decode("utf-8", "replace")
    seeks, identity, environments = [], [], []
    for line in text.splitlines():
        stripped = line.strip()
        for pattern, sink in ((SEEK, seeks), (IDENTITY, identity), (ENVIRONMENT, environments)):
            match = pattern.match(stripped)
            if match:
                sink.append(json.loads(match.group(1)))
    present = [entry["request_to_present_us"] for entry in seeks]
    ready = [entry["request_to_ready_us"] for entry in seeks]
    record = {
        "scale": scale,
        "run": index,
        "exit_code": completed.returncode,
        "wall_seconds": round(wall, 3),
        "seek_count": len(present),
        "identity": identity[0] if identity else None,
        "environment": environments[0] if environments else None,
        "request_to_present_us": present,
        "request_to_ready_us": ready,
        "log": str(log.relative_to(EVIDENCE)).replace("\\", "/"),
    }
    record["valid"] = bool(completed.returncode == 0 and identity
                           and identity[0].get("records") == scale
                           and len(present) == SEEKS_PER_PROCESS)
    if record["valid"]:
        record["median_all_us"] = round(stats.median(present), 3)
        record["median_first3_us"] = round(
            stats.median(present[:SECONDARY_SAMPLE]), 3)
        record["max_us"] = round(max(present), 3)
        record["median_ready_all_us"] = round(stats.median(ready), 3)
    return record


def verdicts(results: dict[int, list[dict]]) -> dict:
    outcome = {}
    for scale, runs in results.items():
        valid = [run for run in runs if run["valid"]]
        scale_result = {"valid_processes": len(valid), "quantities": {}}
        for name, field in (("primary_median_all", "median_all_us"),
                            ("secondary_median_first3", "median_first3_us"),
                            ("tertiary_max", "max_us"),
                            ("request_to_ready_median", "median_ready_all_us")):
            values = [run[field] for run in valid if field in run]
            if len(values) < 2:
                scale_result["quantities"][name] = {"decision": "undecided",
                                                    "reason": "fewer than two valid processes"}
                continue
            described = describe(values)
            described["decision"] = ("unattainable_at_this_magnitude"
                                     if not described["inside_15_percent"]
                                     else "criterion_usable")
            scale_result["quantities"][name] = described
        outcome[str(scale)] = scale_result
    return outcome


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--processes", type=int, default=9)
    parser.add_argument("--timeout", type=float, default=600.0)
    arguments = parser.parse_args()
    if not EXE.is_file():
        print("missing binary: {}".format(EXE), file=sys.stderr)
        return 2

    plan = registration(arguments.processes)
    plan["started_local"] = dt.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    (HERE / "registration.json").write_text(
        json.dumps(plan, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("registered; head={}".format(plan["instrument"]["source"]["head"]), flush=True)

    record = {"registration_sha256": sha256_file(HERE / "registration.json"),
              "started_local": plan["started_local"], "groups": {}}
    results: dict[int, list[dict]] = {scale: [] for scale in SCALES}
    for index in range(arguments.processes):
        for scale in SCALES:
            attempt = run_once(scale, index, arguments.timeout)
            results[scale].append(attempt)
            print("records {:5d} run {:02d}: exit={} valid={} screen={} wall={}s".format(
                scale, index, attempt["exit_code"], attempt["valid"],
                attempt.get("median_all_us"), attempt["wall_seconds"]), flush=True)
            record["groups"] = {str(key): items for key, items in results.items()}
            record["verdicts"] = verdicts(results)
            (HERE / "results.json").write_text(
                json.dumps(record, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    print(json.dumps(record["verdicts"], ensure_ascii=False, indent=2), flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

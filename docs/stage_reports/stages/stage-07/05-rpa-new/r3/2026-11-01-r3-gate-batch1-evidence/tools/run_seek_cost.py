"""Collect the seek-matrix cost readings (request -> ready and request -> present).

This group is registered as COST EVIDENCE for the functional seek matrix, not as the applicable-Seek
gate: the matrix runs the CPU test renderer on 33/129-record content and stamps `formal:false`, and
the plan forbids shrinking the applicable scope to whatever the cheapest instrument covers.

    python -B run_seek_cost.py --runs 3
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import time

ROOT = Path(r"D:\Cuexis-redesign")
EXE = ROOT / "out/build/rpa-candidate-release/bin/cuexis_execution_host_tests.exe"
HERE = Path(__file__).resolve().parent
EVENT = re.compile(r'^\{"event":"r3\.seek\.matrix".*\}$')


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def statistics(values):
    if not values:
        return None
    ordered = sorted(values)
    index = max(0, min(len(ordered) - 1, int(round(0.95 * (len(ordered) - 1)))))
    return {
        "n": len(ordered),
        "min_us": ordered[0],
        "median_us": ordered[len(ordered) // 2] if len(ordered) % 2
        else (ordered[len(ordered) // 2 - 1] + ordered[len(ordered) // 2]) / 2,
        "p95_us": ordered[index],
        "max_us": ordered[-1],
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--runs", type=int, default=3)
    parser.add_argument("--timeout", type=float, default=1800.0)
    arguments = parser.parse_args()

    target = HERE / "seek"
    if (target / "results.json").exists():
        raise SystemExit("results already exist, refusing to re-sample: {}".format(target))
    target.mkdir(parents=True, exist_ok=True)

    record = {
        "scope": "Seek-matrix cost readings (request->ready, request->present). Cost evidence, not "
                 "the applicable-Seek gate.",
        "exe": str(EXE),
        "exe_sha256": sha256_file(EXE),
        "filter": "[seek-matrix]",
        "head": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT).decode().strip(),
        "runs": [],
    }
    for index in range(arguments.runs):
        log = target / "run-{:02d}.log".format(index)
        started = time.monotonic()
        with log.open("wb") as stream:
            completed = subprocess.run([str(EXE), "[seek-matrix]"], cwd=str(EXE.parent),
                                       stdout=stream, stderr=subprocess.STDOUT,
                                       timeout=arguments.timeout)
        wall = time.monotonic() - started
        text = log.read_bytes().decode("utf-8", "replace")
        events = [json.loads(line) for line in text.splitlines() if EVENT.match(line.strip())]
        ready = [event["request_to_ready_us"] for event in events]
        present = [event["request_to_present_us"] for event in events]
        attempt = {
            "run": index,
            "exit_code": completed.returncode,
            "wall_seconds": round(wall, 3),
            "events": len(events),
            "distinct_cases": sorted({(event["records"], event["cut_kind"], event["external"],
                                       event["hint_kind"]) for event in events}),
            "ready": statistics(ready),
            "present": statistics(present),
            "present_us": present,
            "ready_us": ready,
            "log": str(log.relative_to(ROOT)),
        }
        attempt["valid"] = completed.returncode == 0 and len(events) == 60
        record["runs"].append(attempt)
        print("run {:02d}: events={} exit={} present median={} max={}".format(
            index, len(events), completed.returncode,
            attempt["present"]["median_us"] if attempt["present"] else None,
            attempt["present"]["max_us"] if attempt["present"] else None), flush=True)
        with (target / "results.json").open("w", encoding="utf-8", newline="\n") as stream:
            json.dump(record, stream, ensure_ascii=False, indent=2)
            stream.write("\n")

    record["summary"] = {
        "valid_runs": sum(1 for run in record["runs"] if run["valid"]),
        "invalid_runs": sum(1 for run in record["runs"] if not run["valid"]),
        "pooled_present": statistics([value for run in record["runs"] for value in run["present_us"]]),
        "pooled_ready": statistics([value for run in record["runs"] for value in run["ready_us"]]),
        "per_run_present_medians_us": [run["present"]["median_us"] for run in record["runs"]],
        "per_run_present_maxima_us": [run["present"]["max_us"] for run in record["runs"]],
        "per_run_ready_maxima_us": [run["ready"]["max_us"] for run in record["runs"]],
    }
    with (target / "results.json").open("w", encoding="utf-8", newline="\n") as stream:
        json.dump(record, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    print(json.dumps(record["summary"], ensure_ascii=False), flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())

"""Run the three uncovered pre-declared Seek classes on the PD5 device probe.

    python -B run_r3_seek_classes.py [--dry-run]

Every process is fresh and nothing is retried. A process that fails a validity check is kept in
place with its log and leaves its group short; the run is never repeated to obtain a pass.

The instrument is cuexis_execution_device_probe.exe, which compiles tests/pd/pd5_device_probe.cpp
with CUEXIS_PD5_DEVICE and therefore runs app/player ExecutionHost on OpenGlBackend, rather than
the CPU test renderer, so its readings can stand for the 20 ms applicable-Seek budget.
"""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import re
import statistics as stats
import subprocess
import sys
import time

ROOT = Path(r"D:\Cuexis-r3seek-verify")
EXE = Path(r"D:\Cuexis-redesign\out\_verify-seek-classes\bin\cuexis_execution_device_probe.exe")
HERE = Path(r"D:\Cuexis-redesign\out\_verify-seek-classes\replay\tools")
EVIDENCE = HERE.parent
TEST = "PD5 scripted seek timing probe"
SEEK = re.compile(r"^PD5_SEEK (\{.*\})$")
IDENTITY = re.compile(r"^PD5_SEEK_ID (\{.*\})$")
VERIFY = re.compile(r"^PD5_SEEK_VERIFY (\{.*\})$")
ENV = re.compile(r"^PD_ENV (\{.*\})$")
BUDGET_US = 20000
CLASSES = ["natural", "external", "fallback"]
SCALES = [129, 1024]
PROCESSES = 6
SEEKS = 16
HORIZONS = [1103 + index * 7000 for index in range(SEEKS)]


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def git(*args: str) -> str:
    return subprocess.run(
        ["git", *args], cwd=ROOT, capture_output=True, text=True, check=True
    ).stdout.strip()


def run_once(class_id: str, records: int, index: int) -> dict:
    group = f"runs/{class_id}/{records}"
    log = EVIDENCE / group / f"run-{index:02d}.log.txt"
    log.parent.mkdir(parents=True, exist_ok=True)
    environment = {
        key: value
        for key, value in os.environ.items()
        if not key.startswith("CUEXIS_PD5_")
    }
    environment.update(
        {
            "CUEXIS_PD5_SEEK_RECORDS": str(records),
            "CUEXIS_PD5_SEEK_COUNT": str(SEEKS),
            "CUEXIS_PD5_SEEK_CLASS": class_id,
        }
    )
    started = time.monotonic()
    with log.open("wb") as stream:
        completed = subprocess.run(
            [str(EXE), TEST],
            cwd=str(EXE.parent),
            stdout=stream,
            stderr=subprocess.STDOUT,
            env=environment,
            timeout=600,
        )
    wall = time.monotonic() - started
    text = log.read_bytes().decode("utf-8", "replace")
    seeks, identity, verify, envs = [], None, None, []
    for line in text.splitlines():
        if match := SEEK.match(line):
            seeks.append(json.loads(match.group(1)))
        elif match := IDENTITY.match(line):
            identity = json.loads(match.group(1))
        elif match := VERIFY.match(line):
            verify = json.loads(match.group(1))
        elif match := ENV.match(line):
            envs.append(json.loads(match.group(1)))
    problems = []
    if completed.returncode != 0:
        problems.append(f"exit code {completed.returncode}")
    if identity is None:
        problems.append("missing PD5_SEEK_ID")
    else:
        if identity.get("records") != records:
            problems.append(f"records {identity.get('records')} != {records}")
        if identity.get("seeks") != SEEKS:
            problems.append(f"seeks {identity.get('seeks')} != {SEEKS}")
        if identity.get("seek_class") != class_id:
            problems.append(f"seek_class {identity.get('seek_class')} != {class_id}")
    if len(seeks) != SEEKS:
        problems.append(f"{len(seeks)} seek readings != {SEEKS}")
    else:
        horizons = [row["horizon"] for row in seeks]
        if horizons != HORIZONS:
            problems.append("unexpected horizon sequence")
        mismatched = [row["index"] for row in seeks if not row["state_matches_oracle"]]
        if mismatched:
            problems.append(f"oracle mismatch at {mismatched}")
    if verify is None:
        problems.append("missing PD5_SEEK_VERIFY")
    elif verify.get("mismatched") or verify.get("verified") != SEEKS:
        problems.append("verify summary not clean")
    if not envs:
        problems.append("missing PD_ENV")
    to_present = [row["request_to_present_us"] for row in seeks]
    to_ready = [row["request_to_ready_us"] for row in seeks]
    return {
        "class": class_id,
        "records": records,
        "run": index,
        "log": str(log.relative_to(EVIDENCE)).replace("\\", "/"),
        "wall_seconds": round(wall, 3),
        "valid": not problems,
        "problems": problems,
        "identity": identity,
        "environment": envs[0] if envs else None,
        "readings_to_present_us": to_present,
        "readings_to_ready_us": to_ready,
        "max_to_present_us": max(to_present) if to_present else None,
        "max_to_ready_us": max(to_ready) if to_ready else None,
        "median_to_present_us": round(stats.median(to_present), 3) if to_present else None,
        "passes": bool(to_present) and max(to_present) <= BUDGET_US and not problems,
    }


def summarise(group: dict) -> dict:
    runs = group["runs"]
    valid = [run for run in runs if run["valid"]]
    maxima = [run["max_to_present_us"] for run in valid if run["max_to_present_us"]]
    all_readings = [
        value for run in valid for value in run["readings_to_present_us"]
    ]
    return {
        **{key: group[key] for key in ("class", "records")},
        "processes_requested": PROCESSES,
        "processes_valid": len(valid),
        "seeks": len(all_readings),
        "per_process_max_us": maxima,
        "worst_to_present_us": max(maxima) if maxima else None,
        "median_to_present_us": round(stats.median(all_readings), 3) if all_readings else None,
        "p99_to_present_us": (
            sorted(all_readings)[max(1, -(-99 * len(all_readings) // 100)) - 1]
            if all_readings
            else None
        ),
        "status": (
            "pass"
            if len(valid) == PROCESSES and maxima and max(maxima) <= BUDGET_US
            else ("invalid" if not maxima else "over")
        ),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--out", default=str(HERE / "results.json"))
    args = parser.parse_args()

    diff = subprocess.run(
        ["git", "diff", "HEAD"], cwd=ROOT, capture_output=True, check=True
    ).stdout
    document = {
        "scope": (
            "Fixed diagnostic on one source identity. Test-only probe; no formal CI, no device "
            "or production acceptance, no R3 exit."
        ),
        "started_local": dt.datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        "source": {
            "head": git("rev-parse", "HEAD"),
            "dirty_diff_sha256": hashlib.sha256(diff).hexdigest(),
            "dirty_files": git("diff", "--name-only", "HEAD").splitlines(),
            "instrument": str(EXE),
            "instrument_sha256": sha256_file(EXE),
            "instrument_size": EXE.stat().st_size,
        },
        "criterion": {"metric": "request_to_present_us", "budget_us": BUDGET_US},
        "protocol": {
            "classes": CLASSES,
            "scales_records": SCALES,
            "processes_per_class_scale": PROCESSES,
            "seeks_per_process": SEEKS,
        },
        "groups": [],
    }
    if args.dry_run:
        print(json.dumps(document, indent=2))
        return 0

    for class_id in CLASSES:
        for records in SCALES:
            group = {"class": class_id, "records": records, "runs": []}
            for index in range(PROCESSES):
                run = run_once(class_id, records, index)
                group["runs"].append(run)
                marker = "ok" if run["passes"] else "FAIL"
                print(
                    f"{class_id:9s} {records:5d} run {index:02d} {marker} "
                    f"max_to_present={run['max_to_present_us']}us"
                )
            group["summary"] = summarise(group)
            document["groups"].append(group)

    document["finished_local"] = dt.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    Path(args.out).write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {args.out}")
    failures = [group for group in document["groups"] if group["summary"]["status"] != "pass"]
    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())

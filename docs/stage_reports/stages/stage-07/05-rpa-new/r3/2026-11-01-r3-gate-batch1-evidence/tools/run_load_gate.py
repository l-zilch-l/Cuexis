"""Measure the initial-load gate: process creation to first valid frame present-return.

The boundary needs two clocks that are comparable across processes: the child's creation time
(from GetProcessTimes, a FILETIME) and the wall-clock timestamp the player itself embeds in its
`first_frame_presented` log line. Both are absolute UTC, so the difference is the gate reading
without any pipe or scheduling guesswork on the parent side.

    python -B run_load_gate.py --runs 9

Every run is a fresh process. Nothing is retried: a run that fails its validity checks stays in the
results as an invalid record and leaves the group short.
"""

from __future__ import annotations

import argparse
import ctypes
import ctypes.wintypes as wintypes
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time

ROOT = Path(r"D:\Cuexis-redesign")
EXE = ROOT / "out/build/rpa-candidate-release/bin/cuexis_player.exe"
HERE = Path(__file__).resolve().parent
GATE_MS = 2000.0
GOLDEN = "Frame 5 summary=18316288860163381829"
STAMP = re.compile(r"^(\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d\.\d{3}[+-]\d\d:\d\d)")
FIRST_FRAME = re.compile(r"\[player\.startup_phase\] first_frame_presented (.+)$")

kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
kernel32.OpenProcess.restype = wintypes.HANDLE
kernel32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
kernel32.GetProcessTimes.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.FILETIME),
                                     ctypes.POINTER(wintypes.FILETIME),
                                     ctypes.POINTER(wintypes.FILETIME),
                                     ctypes.POINTER(wintypes.FILETIME)]
kernel32.CloseHandle.argtypes = [wintypes.HANDLE]

PROCESS_QUERY_LIMITED_INFORMATION = 0x1000


def creation_utc(pid: int) -> dt.datetime:
    handle = kernel32.OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, False, pid)
    if not handle:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        created, exited, kernel, user = (wintypes.FILETIME() for _ in range(4))
        if not kernel32.GetProcessTimes(handle, ctypes.byref(created), ctypes.byref(exited),
                                        ctypes.byref(kernel), ctypes.byref(user)):
            raise ctypes.WinError(ctypes.get_last_error())
        ticks = (created.dwHighDateTime << 32) | created.dwLowDateTime
    finally:
        kernel32.CloseHandle(handle)
    return dt.datetime(1601, 1, 1, tzinfo=dt.timezone.utc) + dt.timedelta(microseconds=ticks / 10)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def run_once(index: int, timeout: float) -> dict:
    out_path = HERE / "load" / f"run-{index:02d}.out.log"
    err_path = HERE / "load" / f"run-{index:02d}.err.log"
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("wb") as out, err_path.open("wb") as err:
        started = time.monotonic()
        child = subprocess.Popen([str(EXE), "--smoke-test"], cwd=str(EXE.parent),
                                 stdout=out, stderr=err)
        try:
            created = creation_utc(child.pid)
        except OSError:
            created = None
        try:
            code = child.wait(timeout=timeout)
            timed_out = False
        except subprocess.TimeoutExpired:
            child.kill()
            child.wait()
            code = None
            timed_out = True
        wall = time.monotonic() - started

    text = out_path.read_bytes().decode("utf-8", "replace")
    lines = text.splitlines()
    stamps = []
    first_frame = None
    for line in lines:
        match = STAMP.match(line)
        if match:
            stamps.append(dt.datetime.fromisoformat(match.group(1)))
        frame = FIRST_FRAME.search(line)
        if frame and first_frame is None:
            marker = match.group(1) if match else None
            first_frame = {"stamp": marker, "detail": frame.group(1)}

    record = {
        "run": index,
        "exit_code": code,
        "timed_out": timed_out,
        "wall_seconds": round(wall, 3),
        "created_utc": created.isoformat() if created else None,
        "first_log_utc": stamps[0].isoformat() if stamps else None,
        "first_frame": first_frame,
        "golden_present": GOLDEN in text,
        "first_frame_lines": sum(1 for line in lines if FIRST_FRAME.search(line)),
        "log_lines": len(lines),
        "out_log": str(out_path.relative_to(ROOT)),
        "err_log": str(err_path.relative_to(ROOT)),
    }
    if created and first_frame and first_frame["stamp"]:
        present = dt.datetime.fromisoformat(first_frame["stamp"])
        record["load_ms"] = round((present - created).total_seconds() * 1000.0, 3)
        if stamps:
            record["logger_start_ms"] = round((stamps[0] - created).total_seconds() * 1000.0, 3)
    record["valid"] = bool(
        code == 0 and not timed_out and record["golden_present"]
        and record["first_frame_lines"] == 1 and record.get("load_ms") is not None)
    return record


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--runs", type=int, default=9)
    parser.add_argument("--timeout", type=float, default=180.0)
    arguments = parser.parse_args()

    if not EXE.is_file():
        raise SystemExit("player is missing: {}".format(EXE))
    target = HERE / "load"
    if (target / "results.json").exists():
        raise SystemExit("results already exist, refusing to re-sample: {}".format(target))

    record = {
        "scope": "Initial-load gate: process creation to first valid frame present-return.",
        "gate_ms": GATE_MS,
        "exe": str(EXE),
        "exe_sha256": sha256_file(EXE),
        "workload": "--smoke-test",
        "head": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT).decode().strip(),
        "runs": [],
    }
    for index in range(arguments.runs):
        attempt = run_once(index, arguments.timeout)
        record["runs"].append(attempt)
        print("run {:02d}: load_ms={} valid={} golden={} exit={}".format(
            index, attempt.get("load_ms"), attempt["valid"], attempt["golden_present"],
            attempt["exit_code"]), flush=True)
        with (target / "results.json").open("w", encoding="utf-8", newline="\n") as stream:
            json.dump(record, stream, ensure_ascii=False, indent=2)
            stream.write("\n")

    valid = [run["load_ms"] for run in record["runs"] if run["valid"]]
    if valid:
        ordered = sorted(valid)
        median = ordered[len(ordered) // 2] if len(ordered) % 2 else \
            (ordered[len(ordered) // 2 - 1] + ordered[len(ordered) // 2]) / 2
        summary = {
            "valid_runs": len(valid),
            "invalid_runs": len(record["runs"]) - len(valid),
            "min_ms": min(valid),
            "median_ms": round(median, 3),
            "max_ms": max(valid),
            "relative_range": round((max(valid) - min(valid)) / median, 6),
            "gate_pass_per_run": all(value <= GATE_MS for value in valid),
            "repeatability_pass": (max(valid) - min(valid)) / median <= 0.15,
        }
        record["summary"] = summary
        print(json.dumps(summary, ensure_ascii=False), flush=True)
    with (target / "results.json").open("w", encoding="utf-8", newline="\n") as stream:
        json.dump(record, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())

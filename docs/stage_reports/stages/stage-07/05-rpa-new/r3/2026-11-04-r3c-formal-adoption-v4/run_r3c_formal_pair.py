#!/usr/bin/env python3
"""Pre-registered R3c baseline/candidate Seek unit.

The same test source is built in two worktrees.  The baseline tree has the common product
changes without R3c; the candidate tree additionally has the archived R3c patch.  Each
process runs the 30-second machine warmup, then the hidden CPU Host Seek measurement with
16 requests.  Three windows of three independent pairs are retained without replacement.
"""

from __future__ import annotations

import ctypes
import datetime as dt
import hashlib
import json
import math
import os
from pathlib import Path
import statistics as stats
import subprocess
import time

HERE = Path(__file__).resolve().parent
EVIDENCE = HERE
BASELINE = Path(r"D:\Cuexis-r3formal-base")
CANDIDATE = Path(r"D:\Cuexis-r3c-v4")
BASELINE_EXE = BASELINE / "out/build/formal-release/bin/cuexis_execution_host_tests.exe"
CANDIDATE_EXE = CANDIDATE / "out/build/formal-release/bin/cuexis_execution_host_tests.exe"
BASELINE_WARM = BASELINE / "out/build/formal-release/bin/cuexis_pd_baseline_probe.exe"
CANDIDATE_WARM = CANDIDATE / "out/build/formal-release/bin/cuexis_pd_baseline_probe.exe"
TEST = "R3 CPU host Seek measures first and repeated requests through target present"
WARM = "PD machine CPU warmup"
WINDOWS = 3
PAIRS_PER_WINDOW = 3
SEEKS = 16
WARMUP_SECONDS = 30
AFFINITY_MASK = 4
T_CRITICAL_95 = {2: 4.302652729911275, 5: 2.570581835636315, 8: 2.306004135204166}
CASES = [
    {"id": "1k-h902", "requirements": 1000, "horizon": 902},
    {"id": "40k-h5", "requirements": 40000, "horizon": 5},
    {"id": "40k-h902", "requirements": 40000, "horizon": 902},
]
ORDER = [("A", "B"), ("B", "A"), ("A", "B")]
BINARY = {"A": BASELINE_EXE, "B": CANDIDATE_EXE}
WARM_BINARY = {"A": BASELINE_WARM, "B": CANDIDATE_WARM}
ROOTS = {"A": BASELINE, "B": CANDIDATE}
POWER = 4
CREATE_NO_WINDOW = 0x08000000
BELOW_NORMAL_PRIORITY_CLASS = 0x00004000


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def git(root: Path, *args: str) -> str:
    return subprocess.run(["git", *args], cwd=root, capture_output=True, text=True,
                          check=True).stdout.strip()


class PowerState(ctypes.Structure):
    _fields_ = [("version", ctypes.c_ulong), ("control_mask", ctypes.c_ulong),
                ("state_mask", ctypes.c_ulong)]


def apply_controls(child: subprocess.Popen) -> dict:
    if os.name != "nt":
        return {"platform": "non-windows"}
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.OpenProcess.argtypes = [ctypes.c_ulong, ctypes.c_bool, ctypes.c_ulong]
    kernel.OpenProcess.restype = ctypes.c_void_p
    kernel.SetProcessAffinityMask.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
    kernel.GetProcessAffinityMask.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_size_t),
                                              ctypes.POINTER(ctypes.c_size_t)]
    kernel.SetPriorityClass.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
    kernel.GetPriorityClass.argtypes = [ctypes.c_void_p]
    kernel.GetPriorityClass.restype = ctypes.c_ulong
    kernel.SetProcessInformation.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p,
                                             ctypes.c_ulong]
    kernel.GetProcessInformation.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p,
                                             ctypes.c_ulong]
    kernel.CloseHandle.argtypes = [ctypes.c_void_p]
    handle = kernel.OpenProcess(0x0200 | 0x1000, False, child.pid)
    if not handle:
        raise RuntimeError("OpenProcess failed")
    try:
        state = PowerState(1, 1, 0)
        if not kernel.SetProcessAffinityMask(handle, AFFINITY_MASK):
            raise RuntimeError("SetProcessAffinityMask failed")
        if not kernel.SetPriorityClass(handle, BELOW_NORMAL_PRIORITY_CLASS):
            raise RuntimeError("SetPriorityClass failed")
        if not kernel.SetProcessInformation(handle, POWER, ctypes.byref(state), ctypes.sizeof(state)):
            raise RuntimeError("SetProcessInformation failed")
        mask = ctypes.c_size_t()
        system = ctypes.c_size_t()
        if not kernel.GetProcessAffinityMask(handle, ctypes.byref(mask), ctypes.byref(system)):
            raise RuntimeError("GetProcessAffinityMask failed")
        result = {
            "affinity": mask.value,
            "system_affinity": system.value,
            "priority": kernel.GetPriorityClass(handle),
            "power_control_requested": state.control_mask,
            "power_state_requested": state.state_mask,
        }
        if result != {"affinity": AFFINITY_MASK, "system_affinity": 65535,
                      "priority": BELOW_NORMAL_PRIORITY_CLASS,
                      "power_control_requested": 1, "power_state_requested": 0}:
            raise RuntimeError(f"unexpected process controls: {result}")
        return result
    finally:
        kernel.CloseHandle(handle)


def clear_env() -> dict[str, str]:
    return {k: v for k, v in os.environ.items()
            if not k.startswith(("CUEXIS_", "R3_"))}


def check_no_build() -> None:
    table = subprocess.check_output(["tasklist", "/FO", "CSV", "/NH"], text=True,
                                    encoding="utf-8", errors="replace").lower()
    forbidden = ("cl.exe", "link.exe", "ninja.exe", "gcc.exe", "g++.exe", "cc1.exe", "cc1plus.exe")
    if any(name in table for name in forbidden):
        raise RuntimeError("concurrent build detected")


def source_identity(root: Path) -> dict:
    return {
        "root": str(root),
        "head": git(root, "rev-parse", "HEAD"),
        "status": git(root, "status", "--short"),
        "diff_sha256": hashlib.sha256(subprocess.check_output(["git", "diff", "--binary"],
                                                               cwd=root)).hexdigest(),
        "files": {name: sha256(root / name) for name in (
            "tests/rpa/rpa_b5_seek_host_tests.cpp",
            "tests/rpa/rpa_b3_r3c_tests.cpp") if (root / name).is_file()},
    }


def registration() -> dict:
    plan = []
    for case in CASES:
        for window in range(WINDOWS):
            for pair in range(PAIRS_PER_WINDOW):
                variants = ORDER[window]
                for variant in variants:
                    plan.append({"case": case["id"], "requirements": case["requirements"],
                                 "horizon": case["horizon"], "window": window,
                                 "pair": pair, "variant": variant})
    return {
        "schema": "r3c.formal-pair.v1",
        "formal": True,
        "scope": "R3c baseline versus candidate on identical source except the archived R3c patch. "
                 "CPU Host request through target present only; not physical GPU, load, memory-cap, "
                 "R4, production or R3-exit acceptance.",
        "started_local": dt.datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        "cases": CASES,
        "design": {
            "windows": WINDOWS,
            "pairs_per_window": PAIRS_PER_WINDOW,
            "seeks_per_process": SEEKS,
            "order_per_unit": ["AB/BA/AB", "BA/AB/BA", "AB/BA/AB"],
            "machine_warmup_seconds_per_process": WARMUP_SECONDS,
            "warmup_test": WARM,
            "affinity_mask": AFFINITY_MASK,
            "priority": "BELOW_NORMAL",
            "power_throttling": "execution throttling OFF",
            "no_parallel_build": True,
            "no_retry_or_replacement": True,
        },
        "decision_rule": "For each case, form the candidate/baseline ratio from each process's median "
                         "request_to_present_us over 16 requests. Use Student-t 95% intervals on the "
                         "log ratios for each window and all nine pairs. A case passes when the all-nine "
                         "upper bound is <= 1.10. Every candidate request must also be <= 20000 us. "
                         "The 15% (max-min)/median screen is reported for the formal unit and does not "
                         "replace the ratio decision or discard slow values.",
        "source": {"baseline": source_identity(BASELINE), "candidate": source_identity(CANDIDATE)},
        "binaries": {
            "baseline": {"path": str(BASELINE_EXE), "sha256": sha256(BASELINE_EXE),
                         "size": BASELINE_EXE.stat().st_size},
            "candidate": {"path": str(CANDIDATE_EXE), "sha256": sha256(CANDIDATE_EXE),
                          "size": CANDIDATE_EXE.stat().st_size},
            "baseline_warmup": {"path": str(BASELINE_WARM), "sha256": sha256(BASELINE_WARM)},
            "candidate_warmup": {"path": str(CANDIDATE_WARM), "sha256": sha256(CANDIDATE_WARM)},
        },
        "runner_sha256": sha256(Path(__file__)),
        "plan": plan,
    }


def run_warmup(variant: str, label: str) -> dict:
    path = EVIDENCE / "logs" / f"{label}-warmup.log.txt"
    path.parent.mkdir(parents=True, exist_ok=True)
    started = time.monotonic()
    with path.open("wb") as stream:
        completed = subprocess.run([str(WARM_BINARY[variant]), WARM, "--rng-seed", "1"],
                                   cwd=str(ROOTS[variant]), env=clear_env(), stdout=stream,
                                   stderr=subprocess.STDOUT, timeout=120,
                                   creationflags=CREATE_NO_WINDOW)
    text = path.read_text(encoding="utf-8", errors="replace")
    return {"exit": completed.returncode, "seconds": round(time.monotonic() - started, 3),
            "complete": completed.returncode == 0 and "PD_MACHINE_WARMUP" in text}


def run_measurement(item: dict, ordinal: int) -> dict:
    check_no_build()
    variant = item["variant"]
    label = f"{ordinal:02d}-{item['case']}-w{item['window']}-p{item['pair']}-{variant}"
    warm = run_warmup(variant, label)
    if not warm["complete"]:
        return {**item, "valid": False, "reason": "warmup incomplete", "warmup": warm}
    path = EVIDENCE / "logs" / f"{label}.log.txt"
    env = clear_env()
    env.update({
        "CUEXIS_SEEK_RECORDS": "129",
        "CUEXIS_SEEK_REQUIREMENTS": str(item["requirements"]),
        "CUEXIS_SEEK_CONTENT": "packed",
        "CUEXIS_SEEK_IDENTITY": "source",
        "CUEXIS_SEEK_ATTEMPTS": str(SEEKS),
        "CUEXIS_SEEK_HORIZON": str(item["horizon"]),
    })
    started = time.monotonic()
    with path.open("wb") as stream:
        child = subprocess.Popen([str(BINARY[variant]), TEST, "--rng-seed", "1"],
                                 cwd=str(ROOTS[variant]), env=env, stdout=stream,
                                 stderr=subprocess.STDOUT, creationflags=CREATE_NO_WINDOW |
                                 BELOW_NORMAL_PRIORITY_CLASS)
        try:
            controls = apply_controls(child)
        except Exception:
            child.kill()
            child.wait()
            raise
        try:
            code = child.wait(timeout=900)
        except subprocess.TimeoutExpired:
            child.kill()
            child.wait()
            code = "timeout"
    text = path.read_text(encoding="utf-8", errors="replace")
    events = []
    for line in text.splitlines():
        if line.startswith('{"event":"r3.redesign.host"'):
            events.append(json.loads(line))
    attempts = [e.get("attempt") for e in events]
    expected = list(range(SEEKS))
    reading = [e.get("request_to_present_us") for e in events]
    valid = (code == 0 and "All tests passed" in text and attempts == expected and
             all(isinstance(v, int) and v > 0 for v in reading) and
             all(e.get("horizon") == item["horizon"] for e in events))
    result = {**item, "ordinal": ordinal, "valid": valid, "exit": code,
              "seconds": round(time.monotonic() - started, 3), "warmup": warm,
              "controls": controls, "log": str(path.relative_to(EVIDENCE)),
              "events": events, "request_to_present_us": reading}
    if valid:
        result["median_us"] = stats.median(reading)
        result["max_us"] = max(reading)
    return result


def interval(values: list[float]) -> dict:
    logs = [math.log(v) for v in values]
    mean = stats.mean(logs)
    if len(logs) < 2:
        return {"n": len(logs), "ratio": math.exp(mean), "low": None, "high": None}
    half = T_CRITICAL_95[len(logs) - 1] * stats.stdev(logs) / math.sqrt(len(logs))
    return {"n": len(logs), "ratio": math.exp(mean), "low": math.exp(mean - half),
            "high": math.exp(mean + half)}


def range_pct(values: list[float]) -> float | None:
    return None if len(values) < 2 else (max(values) - min(values)) / stats.median(values) * 100.0


def verdicts(rows: list[dict]) -> dict:
    result = {}
    for case in CASES:
        case_rows = [row for row in rows if row["case"] == case["id"]]
        pairs = {}
        for window in range(WINDOWS):
            for pair in range(PAIRS_PER_WINDOW):
                selected = [row for row in case_rows if row["window"] == window and row["pair"] == pair]
                if len(selected) != 2 or not all(row["valid"] for row in selected):
                    continue
                a = next(row for row in selected if row["variant"] == "A")
                b = next(row for row in selected if row["variant"] == "B")
                pairs[f"{window}-{pair}"] = b["median_us"] / a["median_us"]
        intervals = {}
        for count in (3, 6, 9):
            chosen = list(pairs.values())[:count]
            intervals[str(count)] = interval(chosen) if len(chosen) == count else {
                "n": len(chosen), "ratio": None, "low": None, "high": None}
        all_interval = intervals["9"]
        valid_processes = sum(1 for row in case_rows if row["valid"])
        candidate_values = [row["max_us"] for row in case_rows if row["valid"] and row["variant"] == "B"]
        repeatability = {}
        for variant in ("A", "B"):
            values = [row["median_us"] for row in case_rows if row["valid"] and row["variant"] == variant]
            repeatability[variant] = {str(count): range_pct(values[:count]) for count in (3, 6, 9)}
        decision = "inconclusive"
        if all_interval["high"] is not None:
            if all_interval["high"] <= 1.10:
                decision = "pass"
            elif all_interval["low"] > 1.10:
                decision = "regression"
        within_20ms = bool(candidate_values) and max(candidate_values) <= 20000
        result[case["id"]] = {
            "valid_processes": valid_processes,
            "pair_ratios": pairs,
            "intervals": intervals,
            "repeatability_pct": repeatability,
            "candidate_max_us": max(candidate_values) if candidate_values else None,
            "candidate_within_20ms": within_20ms,
            "decision": decision,
        }
    return result


def main() -> int:
    if (HERE / "registration.json").exists():
        raise SystemExit("registration already exists; refusing a second run in this directory")
    for path in (BASELINE_EXE, CANDIDATE_EXE, BASELINE_WARM, CANDIDATE_WARM):
        if not path.is_file():
            raise SystemExit(f"missing binary: {path}")
    registration_doc = registration()
    (HERE / "registration.json").write_text(json.dumps(registration_doc, indent=2) + "\n",
                                             encoding="utf-8")
    print(f"registered {len(registration_doc['plan'])} processes", flush=True)
    rows = []
    for ordinal, item in enumerate(registration_doc["plan"]):
        row = run_measurement(item, ordinal)
        rows.append(row)
        (HERE / "results.json").write_text(json.dumps({"registration":
                                                       sha256(HERE / "registration.json"),
                                                       "rows": rows,
                                                       "verdicts": verdicts(rows)}, indent=2) + "\n",
                                            encoding="utf-8")
        print(f"{ordinal + 1:02d}/{len(registration_doc['plan'])} {item['case']} "
              f"w{item['window']}p{item['pair']} {item['variant']} "
              f"valid={row['valid']} median={row.get('median_us')} max={row.get('max_us')}",
              flush=True)
        if not row["valid"]:
            print("invalid process retained; stopping without replacement", flush=True)
            return 1
    final = verdicts(rows)
    (HERE / "results.json").write_text(json.dumps({"registration": sha256(HERE / "registration.json"),
                                                   "rows": rows, "verdicts": final},
                                                  indent=2) + "\n", encoding="utf-8")
    print(json.dumps(final, indent=2), flush=True)
    return 0 if all(v["decision"] == "pass" and v["candidate_within_20ms"]
                    for v in final.values()) else 1


if __name__ == "__main__":
    raise SystemExit(main())

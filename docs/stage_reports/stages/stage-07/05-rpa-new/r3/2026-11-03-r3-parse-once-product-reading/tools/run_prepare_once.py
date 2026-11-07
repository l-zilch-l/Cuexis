"""Measure the shipped legacy parse-once path against the behaviour before it.

The product owns the parsed Chart tree once since 466ff76a: the dispatcher returns the canonical
identity bytes and loadDocumentStage consumes them instead of re-parsing the source text. This
runner compares that shipped behaviour (arm `reuse`) with a build that ignores the returned bytes
(arm `noreuse`, the six-line A-side edit recorded in `a-side.patch`), on one unchanged probe.

Three pairs per scale, AB / BA / AB, every process fresh and controlled before it starts timing.
One window only: this is a change-value reading under the corrected ratio statistic, not the plan
9.2 nine-pair formal unit, and it promotes nothing.

    python -B run_prepare_once.py

Writes registration.json, results.json and logs/ beside itself. Nothing is retried.
"""

from __future__ import annotations

import argparse
import ctypes
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

HERE = Path(__file__).resolve().parent
EVIDENCE = HERE.parent
ROOT = next(parent for parent in EVIDENCE.parents if (parent / ".git").exists())
BINARIES = ROOT / "out/_parseonce"
ARMS = {"baseline": "prepare-twoparse.exe", "candidate": "prepare-once.exe"}
TEST = "CPU legacy preparation fixed diagnostic"
RESULT = re.compile(r"^PREPARE_RESULT (\{.*\})$")
SCALES = (1000, 10000, 40000)
PAIRS = 3
T975 = {1: 12.706204736174705, 2: 4.302652729911275, 3: 3.182446305284263}
AFFINITY = 4
BELOW_NORMAL = 0x4000
CREATE_NO_WINDOW = 0x08000000


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def registration() -> dict:
    return {
        "schema": "r3.prepare.parse_once.v1",
        "formal": False,
        "scope": "Shipped legacy parse-once versus the same source with the reuse disabled. One "
                 "unchanged probe, one timed synchronous prepareLoad per fresh process, three "
                 "pairs per scale AB/BA/AB, one window. Not the plan 9.2 nine-pair formal unit and "
                 "not a load or stage acceptance.",
        "head": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT).decode().strip(),
        "dirty": subprocess.check_output(["git", "status", "--porcelain"], cwd=ROOT).decode().strip(),
        "arms": {
            "baseline": {"role": "the dispatcher produces no identity bytes, so loadDocumentStage "
                                 "calls the canonical writer on the text and parses it a second "
                                 "time: the shape before 466ff76a",
                         "exe": str(BINARIES / ARMS["baseline"]),
                         "sha256": sha256_file(BINARIES / ARMS["baseline"])},
            "candidate": {"role": "shipped product behaviour: the canonical bytes come from the same "
                                  "parsed tree and the stage does not parse again",
                          "exe": str(BINARIES / ARMS["candidate"]),
                          "sha256": sha256_file(BINARIES / ARMS["candidate"])},
        },
        "a_side": "The baseline build differs from HEAD only by the edit in a-side.patch: the legacy "
                  "dispatch stops producing the canonical bytes. The source was restored before this "
                  "registration was written.",
        "probe": {"source_sha256": sha256_file(ROOT / "tests/pd/json_prepare_probe.cpp"),
                  "test": TEST,
                  "fixture": "synthetic legacy Chart v1: entities objects on a square grid, one "
                             "transform plus one renderable each, two assets from "
                             "assets/projects/stage3_project"},
        "design": {"scales": list(SCALES), "pairs_per_scale": PAIRS,
                   "order": "pair 0 AB, pair 1 BA, pair 2 AB",
                   "controls": {"affinity": AFFINITY, "priority_class": BELOW_NORMAL,
                                "creation": "CREATE_NO_WINDOW",
                                "execution_throttling": "not controlled in this group"},
                   "timing": "one timed prepareLoad per process; the fixture is built before the "
                             "timer and commit/update/extract/digest run after it"},
        "decision_rule": "Declared before measurement. The A/B quantity is the paired log ratio of "
                         "the prepare median with a Student-t interval over the three pairs. The "
                         "product arm must not regress: an upper bound above 1.10 is recorded as a "
                         "regression, not as noise. Identity and frame digest must match across all "
                         "processes of a scale. No threshold is changed and no candidate is selected "
                         "by this run.",
        "validity": "exit code 0, 'All tests passed' in the log, exactly one PREPARE_RESULT line "
                    "with entities equal to the scale. Invalid processes are kept and never replaced.",
    }


def run_once(scale: int, pair: int, arm: str, timeout: float) -> dict:
    exe = BINARIES / ARMS[arm]
    log = HERE / "logs" / f"n{scale}" / f"pair{pair}-{arm}.log.txt"
    log.parent.mkdir(parents=True, exist_ok=True)
    environment = {key: value for key, value in os.environ.items()
                   if not key.startswith("CUEXIS_PREPARE_")}
    environment["CUEXIS_PREPARE_ENTITIES"] = str(scale)
    started = time.monotonic()
    with log.open("wb") as stream:
        child = subprocess.Popen([str(exe), TEST, "--rng-seed", "1"], cwd=str(ROOT),
                                 stdin=subprocess.PIPE, stdout=stream,
                                 stderr=subprocess.STDOUT, env=environment,
                                 creationflags=CREATE_NO_WINDOW | BELOW_NORMAL)
        controls = {}
        handle = ctypes.windll.kernel32.OpenProcess(0x0200 | 0x1000, False, child.pid)
        try:
            if handle:
                ctypes.windll.kernel32.SetProcessAffinityMask(handle, AFFINITY)
                mask = ctypes.c_size_t()
                system = ctypes.c_size_t()
                ctypes.windll.kernel32.GetProcessAffinityMask(handle, ctypes.byref(mask),
                                                              ctypes.byref(system))
                controls = {"affinity": mask.value,
                            "priority": ctypes.windll.kernel32.GetPriorityClass(handle)}
            child.stdin.write(b"go\n")
            child.stdin.flush()
            child.stdin.close()
            code = child.wait(timeout=timeout)
        finally:
            if handle:
                ctypes.windll.kernel32.CloseHandle(handle)
            if child.poll() is None:
                child.kill()
                child.wait()
    wall = time.monotonic() - started
    text = log.read_bytes().decode("utf-8", "replace")
    points = []
    for line in text.splitlines():
        match = RESULT.match(line.strip())
        if match:
            points.append(json.loads(match.group(1)))
    record = {"scale": scale, "pair": pair, "arm": arm, "exit_code": code,
              "wall_seconds": round(wall, 3), "controls": controls, "log": str(log.relative_to(EVIDENCE)),
              "result": points[0] if len(points) == 1 else None, "points": len(points)}
    record["valid"] = bool(code == 0 and "All tests passed" in text and len(points) == 1
                           and points[0]["entities"] == scale)
    return record


def analyse(rows: list[dict]) -> dict:
    outcome = {"scales": {}, "equivalence": {}}
    for scale in SCALES:
        selected = [row for row in rows if row["scale"] == scale and row["valid"]]
        arms = {arm: sorted([row for row in selected if row["arm"] == arm],
                            key=lambda row: row["pair"])
                for arm in ARMS}
        entry = {"pairs": {}, "arms": {}}
        for arm in ARMS:
            values = [row["result"]["prepare_us"] for row in arms[arm]]
            entry["arms"][arm] = {
                "n": len(values),
                "values_us": [round(value, 3) for value in values],
                "median_us": round(stats.median(values), 3) if values else None,
                "relative_range_pct": round((max(values) - min(values)) / stats.median(values) * 100.0, 4)
                if len(values) > 1 else None,
            }
        logs = []
        for index in range(min(len(arms["baseline"]), len(arms["candidate"]))):
            base = arms["baseline"][index]["result"]["prepare_us"]
            cand = arms["candidate"][index]["result"]["prepare_us"]
            logs.append(stats_math_log(cand / base))
            entry["pairs"][str(index)] = {"baseline_us": round(base, 3),
                                          "candidate_us": round(cand, 3),
                                          "ratio": round(cand / base, 4)}
        if len(logs) >= 2:
            import math
            mean = stats.mean(logs)
            half = T975[len(logs) - 1] * stats.stdev(logs) / math.sqrt(len(logs))
            entry["ratio"] = {"n": len(logs), "df": len(logs) - 1,
                              "point": round(math.exp(mean), 4),
                              "lower": round(math.exp(mean - half), 4),
                              "upper": round(math.exp(mean + half), 4)}
            entry["change_percent"] = round((math.exp(mean) - 1) * 100.0, 3)
            entry["no_regression"] = entry["ratio"]["upper"] <= 1.10
        identities = {row["result"]["identity"] for row in selected}
        digests = {row["result"]["frame_digest"] for row in selected}
        outcome["equivalence"][str(scale)] = {
            "identities": sorted(identities), "digests": sorted(digests),
            "identity_equal": len(identities) == 1, "digest_equal": len(digests) == 1,
            "processes": len(selected),
        }
        outcome["scales"][str(scale)] = entry
    return outcome


def stats_math_log(value: float) -> float:
    import math
    return math.log(value)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--timeout", type=float, default=300.0)
    arguments = parser.parse_args()
    for arm, name in ARMS.items():
        if not (BINARIES / name).is_file():
            print("missing binary for {}: {}".format(arm, BINARIES / name), file=sys.stderr)
            return 2

    plan = registration()
    plan["started_local"] = dt.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    (HERE / "registration.json").write_text(
        json.dumps(plan, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("registered; head={}".format(plan["head"][:8]), flush=True)

    rows: list[dict] = []
    record = {"registration_sha256": sha256_file(HERE / "registration.json"),
              "started_local": plan["started_local"], "rows": rows}
    for scale in SCALES:
        for pair in range(PAIRS):
            order = ("baseline", "candidate") if pair % 2 == 0 else ("candidate", "baseline")
            for arm in order:
                row = run_once(scale, pair, arm, arguments.timeout)
                rows.append(row)
                print("n={:5d} pair={} {:9s} exit={} valid={} prepare={}us".format(
                    scale, pair, arm, row["exit_code"], row["valid"],
                    row["result"]["prepare_us"] if row["valid"] else None), flush=True)
                record["analysis"] = analyse(rows)
                (HERE / "results.json").write_text(
                    json.dumps(record, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
                if not row["valid"]:
                    print("invalid process; group stops here and keeps the reading", file=sys.stderr)
                    return 3

    print(json.dumps(record["analysis"], ensure_ascii=False, indent=2), flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

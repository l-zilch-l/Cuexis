"""Paired A/B read-cost measurement: pre-PD d344088 (A) against current HEAD (B).

Protocol, registered before the first pair:
  * 3 windows, 3 A/B process pairs per window, at least 600 s between windows
  * within a window the pair order alternates (A->B, B->A, A->B) and the next window starts with
    the opposite arm, so a monotone machine drift cannot masquerade as a difference
  * per process: median of the three `read` block elapsed_us values (blocks are >=100 ms, the
    long-block calibration the PD protocol requires)
  * per pair: ratio = B_median / A_median; windows report their own geometric mean and 95% interval
    (t, df=2), and the whole group reports nine ratios (t, df=8)
  * decision: whole-group upper bound <= 1.10 passes, lower bound > 1.10 is over, otherwise
    inconclusive; no extra pair is ever added to reach a verdict

    python -B run_read_pair.py

Nothing is retried. A process that fails a validity check is recorded as invalid and the pair is left
short, which leaves the window with fewer than three complete ratios.
"""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import math
from pathlib import Path
import re
import statistics
import subprocess
import sys
import time

ROOT = Path(r"D:\Cuexis-redesign")
HERE = Path(__file__).resolve().parent
ARM_A_EXE = ROOT / "out/_armA-d344088/out/build/armA-release/bin/cuexis_pd_baseline_probe.exe"
ARM_B_EXE = ROOT / "out/build/rpa-candidate-release/bin/cuexis_pd_baseline_probe.exe"
TEST_NAME = "PD finite read and update scaling"
ROW = re.compile(r"^PD_FOLLOW_ROW (\{.*\})$")
CONTENT = re.compile(r"^PD_FOLLOW_CONTENT (\{.*\})$")
MIN_BLOCK_US = 100_000.0
T975 = {2: 4.302652729911275, 8: 2.306004135204166}
GATE = 1.10


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def run_arm(arm: str, exe: Path, window: int, pair: int, order: int) -> dict:
    log = HERE / "read-remeasure" / f"w{window}-p{pair}-{order}-{arm}.log"
    log.parent.mkdir(parents=True, exist_ok=True)
    environment = {"CUEXIS_FOLLOW_COUNTS": "1800 2000 2000"}
    started = time.monotonic()
    with log.open("wb") as stream:
        completed = subprocess.run([str(exe), TEST_NAME], cwd=str(exe.parent),
                                   stdout=stream, stderr=subprocess.STDOUT, env=environment)
    wall = time.monotonic() - started
    text = log.read_bytes().decode("utf-8", "replace")
    rows = [json.loads(match.group(1)) for match in (ROW.match(line.strip()) for line in text.splitlines()) if match]
    content = [json.loads(match.group(1)) for match in (CONTENT.match(line.strip()) for line in text.splitlines()) if match]
    reads = [row for row in rows if row["mode"] == "read"]
    updated = [row for row in rows if row["mode"] == "updated"]
    record = {
        "arm": arm,
        "window": window,
        "pair": pair,
        "order": order,
        "exit_code": completed.returncode,
        "wall_seconds": round(wall, 3),
        "chart_sha256": content[0]["sha256"] if content else None,
        "read_blocks_us": [row["elapsed_us"] for row in reads],
        "updated_blocks_us": [row["elapsed_us"] for row in updated],
        "read_digests": sorted({row["digest"] for row in reads}),
        "log": str(log.relative_to(ROOT)),
    }
    record["read_median_us"] = round(statistics.median(record["read_blocks_us"]), 3) if reads else None
    record["valid"] = bool(
        completed.returncode == 0 and len(reads) == 3 and len(updated) == 3
        and record["chart_sha256"] is not None
        and all(value >= MIN_BLOCK_US for value in record["read_blocks_us"])
        and record["read_median_us"] is not None)
    return record


def interval(ratios):
    logs = [math.log(value) for value in ratios]
    mean = sum(logs) / len(logs)
    variance = sum((value - mean) ** 2 for value in logs) / (len(logs) - 1)
    half = T975[len(logs) - 1] * math.sqrt(variance / len(logs))
    return {
        "n": len(logs),
        "ratio": round(math.exp(mean), 4),
        "lower": round(math.exp(mean - half), 4),
        "upper": round(math.exp(mean + half), 4),
        "ratios": [round(value, 4) for value in ratios],
    }


def verdict(whole):
    if whole["upper"] <= GATE:
        return "pass"
    if whole["lower"] > GATE:
        return "over"
    return "inconclusive"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--windows", type=int, default=3)
    parser.add_argument("--pairs", type=int, default=3)
    parser.add_argument("--gap", type=float, default=600.0)
    parser.add_argument("--pair-pause", type=float, default=3.0)
    parser.add_argument("--resume", action="store_true",
                        help="keep the windows already in results.json and continue after them")
    arguments = parser.parse_args()

    target = HERE / "read-remeasure"
    previous = None
    if (target / "results.json").exists():
        if not arguments.resume:
            raise SystemExit("results already exist, refusing to re-sample")
        previous = json.loads((target / "results.json").read_text(encoding="utf-8"))
    for exe in (ARM_A_EXE, ARM_B_EXE):
        if not exe.is_file():
            raise SystemExit("probe is missing: {}".format(exe))

    record = previous if previous else {
        "scope": "Re-measurement of the formal relative-read gate after the materializer trims: A = pre-PD d344088, B = HEAD plus the trims.",
        "gate": {"upper_bound_passes_below": GATE, "min_block_us": MIN_BLOCK_US,
                 "windows": arguments.windows, "pairs_per_window": arguments.pairs},
        "arm_a": {"exe": str(ARM_A_EXE), "sha256": sha256_file(ARM_A_EXE)},
        "arm_b": {"exe": str(ARM_B_EXE), "sha256": sha256_file(ARM_B_EXE)},
        "head": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT).decode().strip(),
        "started_local": dt.datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        "windows": [],
    }

    done = len(record["windows"])
    if done and not previous:
        raise SystemExit("resume state is inconsistent")
    if done >= arguments.windows:
        raise SystemExit("all registered windows are already collected")
    for window in range(done + 1, arguments.windows + 1):
        if window > 1:
            print("waiting {} s before window {}".format(arguments.gap, window), flush=True)
            time.sleep(arguments.gap)
        window_record = {"window": window, "pairs": [], "started_local":
                         dt.datetime.now().strftime("%Y-%m-%d %H:%M:%S")}
        for pair in range(1, arguments.pairs + 1):
            first_is_a = (pair + window) % 2 == 1
            order = [("A", ARM_A_EXE), ("B", ARM_B_EXE)] if first_is_a else [("B", ARM_B_EXE), ("A", ARM_A_EXE)]
            runs = {}
            for index, (arm, exe) in enumerate(order):
                runs[arm] = run_arm(arm, exe, window, pair, index)
                print("w{} p{} {} median={} us valid={}".format(
                    window, pair, arm, runs[arm]["read_median_us"], runs[arm]["valid"]), flush=True)
            a, b = runs["A"], runs["B"]
            pair_record = {"pair": pair, "first": order[0][0], "a": a, "b": b}
            if a["valid"] and b["valid"] and a["chart_sha256"] == b["chart_sha256"] \
                    and a["read_digests"] == b["read_digests"]:
                pair_record["ratio"] = round(b["read_median_us"] / a["read_median_us"], 6)
            else:
                pair_record["ratio"] = None
                pair_record["problem"] = "invalid run, different content or different read digest"
            window_record["pairs"].append(pair_record)
            if pair < arguments.pairs:
                time.sleep(arguments.pair_pause)
        ratios = [pair["ratio"] for pair in window_record["pairs"] if pair["ratio"]]
        window_record["ratios"] = ratios
        window_record["interval"] = interval(ratios) if len(ratios) >= 2 else None
        window_record["finished_local"] = dt.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        record["windows"].append(window_record)
        with (target / "results.json").open("w", encoding="utf-8", newline="\n") as stream:
            json.dump(record, stream, ensure_ascii=False, indent=2)
            stream.write("\n")

    all_ratios = [pair["ratio"] for window in record["windows"] for pair in window["pairs"] if pair["ratio"]]
    if all_ratios:
        whole = interval(all_ratios)
        record["whole_group"] = dict(whole, decision=verdict(whole))
    else:
        record["whole_group"] = {"decision": "undecided", "reason": "no complete pair"}
    record["finished_local"] = dt.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    with (target / "results.json").open("w", encoding="utf-8", newline="\n") as stream:
        json.dump(record, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    print(json.dumps(record["whole_group"], ensure_ascii=False), flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())

"""Drive the pre-registered 1080p p99 budget-crossing ladder.

Reads frontier-bracket-preregistration.json, runs run_40k_refresh.py once per registered entity
count in the registered order, and refuses to continue if the probe binary changes between runs
(which would mean the build drifted mid-ladder). It never retries an invalid run and never edits a
completed run directory.

    python -B run_frontier_bracket.py --exe out/build/rpa-candidate-release/bin/cuexis_execution_device_probe.exe
"""

from __future__ import annotations

import argparse
import datetime as dt
import json
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
PREREGISTRATION = HERE / "frontier-bracket-preregistration.json"
RUNNER = HERE / "run_40k_refresh.py"
SUMMARY = HERE / "frontier-bracket-summary.json"


def say(message: str) -> None:
    stamp = dt.datetime.now().strftime("%H:%M:%S")
    line = "[{}] {}".format(stamp, message)
    print(line, flush=True)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", required=True)
    parser.add_argument("--work", default=str(HERE / "frontier"))
    parser.add_argument("--processes", type=int, default=3)
    parser.add_argument("--seconds", type=int, default=20)
    parser.add_argument("--width", type=int, default=1920)
    parser.add_argument("--height", type=int, default=1080)
    parser.add_argument("--timeout", type=int, default=900)
    parser.add_argument("--only", type=int, default=None, help="run a single entity count")
    args = parser.parse_args()

    plan = json.loads(PREREGISTRATION.read_text(encoding="utf-8"))
    order = plan["ladder"]["order"]
    if args.only is not None:
        if args.only not in order:
            raise SystemExit("entity count {} is not in the registered ladder".format(args.only))
        order = [args.only]

    work = Path(args.work).resolve()
    work.mkdir(parents=True, exist_ok=True)
    exe = Path(args.exe).resolve()

    records = []
    binary = None
    for count in order:
        out = work / "entities-{}".format(count)
        if out.exists():
            raise SystemExit("run directory already exists, refusing to re-sample: {}".format(out))
        command = [
            sys.executable,
            "-B",
            str(RUNNER),
            "--exe",
            str(exe),
            "--out",
            str(out),
            "--entities",
            str(count),
            "--seconds",
            str(args.seconds),
            "--processes",
            str(args.processes),
            "--width",
            str(args.width),
            "--height",
            str(args.height),
            "--timeout",
            str(args.timeout),
        ]
        say("start entities={} -> {}".format(count, out.name))
        completed = subprocess.run(command, cwd=str(HERE))
        if completed.returncode != 0:
            raise SystemExit("run for entities={} exited {}".format(count, completed.returncode))
        registration = json.loads((out / "registration.json").read_text(encoding="utf-8"))
        digest = registration["binary_sha256"]
        if binary is None:
            binary = digest
            say("probe binary {}".format(digest))
        elif digest != binary:
            raise SystemExit(
                "probe binary changed between ladder runs ({} -> {}); the ladder is not one build"
                .format(binary, digest))
        results = json.loads((out / "results.json").read_text(encoding="utf-8"))
        row = {
            "entities": count,
            "run": out.name,
            "head": registration["head"],
            "binary_sha256": digest,
            "created_utc": registration["created_utc"],
            "modes": {
                mode: {
                    "frames": group["frames"],
                    "min_second_fps": group["min_second_fps"],
                    "wall_fps": group["wall_fps"],
                    "wall_fps_median": group["wall_fps_median"],
                    "p99_us": group["p99_us"],
                    "p99_us_median": group["p99_us_median"],
                    "p99_relative_range": group["p99_relative_range"],
                    "fps_relative_range": group["fps_relative_range"],
                    "all_passed_budget": group["all_passed_budget"],
                    "p99_vs_budget": group["p99_vs_budget"],
                    "over_budget": [run["statistics"]["cycle_us"]["over_budget"]
                                    for run in results["runs"] if run["mode"] == mode],
                }
                for mode, group in results["groups"].items()
            },
        }
        records.append(row)
        for mode, group in row["modes"].items():
            say("  entities={} {:<11} fps {:.3f} p99 {:.4f} ms window {} pass {}".format(
                count, mode, group["wall_fps_median"], group["p99_us_median"] / 1000.0,
                min(group["min_second_fps"]), group["all_passed_budget"]))
        SUMMARY.write_text(json.dumps({
            "scope": plan["scope"],
            "head": records[0]["head"],
            "binary_sha256": binary,
            "preregistration": PREREGISTRATION.name,
            "registered_order": plan["ladder"]["order"],
            "completed": [record["entities"] for record in records],
            "pending": [count for count in plan["ladder"]["order"] if count not in
                        [record["entities"] for record in records]],
            "records": records,
        }, indent=2, ensure_ascii=False), encoding="utf-8")

    say("ladder complete: {} points, binary {}".format(len(records), binary))
    return 0


if __name__ == "__main__":
    sys.exit(main())

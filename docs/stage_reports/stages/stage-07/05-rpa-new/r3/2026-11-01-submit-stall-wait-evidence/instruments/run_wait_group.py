"""Drive the registered wait-attribution groups and their per-process analysis.

The groups are the ones frozen in PLAN-WAIT.md section 3: two registered groups of three fresh
processes (the stall point and the stall-free control) plus one unregistered handle-identity process.
Nothing here chooses a parameter: every value comes from the plan.
"""

import argparse
import json
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
PROBE = Path(r"D:\Cuexis-redesign\out\build\rpa-candidate-release\bin\cuexis_execution_device_probe.exe")

# name, entities, processes, identify handles
GROUPS = [
    ("wait-2800-static", 2800, 3, False),
    ("wait-1800-static", 1800, 3, False),
    ("wait-2800-static-handles", 2800, 1, True),
]

SECONDS = "20"


def run(cmd):
    print(" ".join(str(part) for part in cmd), flush=True)
    code = subprocess.call([str(part) for part in cmd], cwd=str(HERE))
    print("exit {}".format(code), flush=True)
    return code


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--groups", nargs="*", default=None,
                        help="restrict to these group names")
    args = parser.parse_args(argv)

    summary = {}
    for name, entities, processes, handles in GROUPS:
        if args.groups and name not in args.groups:
            continue
        out = HERE / name
        for index in range(1, processes + 1):
            command = [sys.executable, "-B", HERE / "run_sample.py",
                       "--exe", PROBE, "--out", out,
                       "--index", str(index),
                       "--entities", str(entities),
                       "--mode", "static",
                       "--seconds", SECONDS,
                       "--processes", str(processes),
                       "--sampler", "wait",
                       "--stack-bytes", "0"]
            if handles:
                command.append("--handles")
            if run(command) != 0:
                raise SystemExit("process {} of {} failed".format(index, name))
        analysed = []
        for samples in sorted(out.glob("*.samples.txt")):
            stem = samples.name[: -len(".samples.txt")]
            target = out / (stem + ".wait.json")
            command = [sys.executable, "-B", HERE / "analyze_wait.py",
                       "--samples", samples,
                       "--modules", out / (stem + ".modules.txt"),
                       "--out", target]
            handle_table = out / (stem + ".handles.txt")
            if handle_table.exists():
                command += ["--handles", handle_table]
            if run(command) != 0:
                raise SystemExit("analysis failed for " + stem)
            analysed.append(target.name)
        summary[name] = dict(entities=entities, processes=processes, handles=handles,
                             analyses=analysed)
    (HERE / "run_wait_group.summary.json").write_text(
        json.dumps(summary, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps(summary, indent=2), flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())

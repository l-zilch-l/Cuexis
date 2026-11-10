"""Re-derive the per-frame wait episodes for every sampled run in one directory tree.

The episode numbers quoted by the report (episodes per frame, median episode, wait microseconds per
frame, wait time share) come from re-reading the archived sample streams, not from the run itself.
This driver exists so that re-derivation is one command instead of a hand-written call per stream:
it walks every ``<group>/results-*.json`` registration, rebuilds the matching ``.runs.json`` in
place, and reports whether the result is byte-identical to the file that was archived.

Usage:
    python -B derive_all.py            # rewrite every .runs.json and compare
    python -B derive_all.py --check    # compare only, write nothing
"""

import argparse
import json
import pathlib
import sys

import derive_wait_runs

HERE = pathlib.Path(__file__).resolve().parent
NAME = "NtWaitForSingleObject"


def run_one(results_path, write):
    group = results_path.parent
    registration = json.loads(results_path.read_text(encoding="utf-8"))
    stem = results_path.name[len("results-"):-len(".json")]
    samples = group / registration["samples"]
    wait = group / (stem + ".wait.json")
    modules = group / (registration["modules"] if registration.get("modules")
                       else stem + ".modules.txt")
    target = group / (stem + ".runs.json")
    if not wait.is_file():
        return "skipped"
    previous = target.read_bytes() if target.is_file() else None
    argv = ["--samples", str(samples.relative_to(HERE)),
            "--wait", str(wait.relative_to(HERE)),
            "--modules", str(modules.relative_to(HERE)),
            "--frames", str(registration["frames"]), "--fps", repr(registration["wall_fps"]),
            "--gap-us", "0"]
    probe = target if write else target.with_suffix(".check.json")
    argv += ["--out", str(probe.relative_to(HERE))]
    saved = sys.argv
    sys.argv = ["derive_wait_runs.py"] + argv
    try:
        derive_wait_runs.main()
    finally:
        sys.argv = saved
    if not write:
        current = probe.read_bytes()
        probe.unlink()
        return previous is not None and previous == current
    current = target.read_bytes()
    return previous is not None and previous == current


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true", help="do not rewrite .runs.json")
    args = parser.parse_args()

    results = sorted(HERE.glob("*/results-*.json"))
    if not results:
        raise SystemExit("no results-*.json under " + str(HERE))
    identical = 0
    skipped = []
    differing = []
    for path in results:
        same = run_one(path, write=not args.check)
        if same is True:
            identical += 1
        elif same is False:
            differing.append(path.parent.name + "/" + path.name)
        else:
            skipped.append(path.parent.name + "/" + path.name)
    print()
    print("streams: {}  byte-identical: {}  changed: {}  skipped: {}".format(
        len(results), identical, len(differing), len(skipped)))
    for name in skipped:
        print("  skipped (no wait analysis):", name)
    for name in differing:
        print("  changed:", name)
    return 0 if not differing else 1


if __name__ == "__main__":
    sys.exit(main())

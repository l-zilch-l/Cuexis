"""Compare one CTest case across two acceptance-matrix runs of the same source.

The 2026-10-31 22:06 run and the 2026-11-01 00:04 run bind to the same HEAD and, as this script
recomputes, to the same per-file hash map. The case in question passed in the first and failed in the
second, so the difference cannot come from the source: it is intermittent behaviour under the same
build. Every number here is recomputed from the two `manifest.json` files and the two CTest logs.

    python -B compare_runs.py --prior out/rpa-acceptance/20261031-220604-9e3ab6ac \
        --current out/rpa-acceptance/20261101-000420-1926fec6 \
        --case "Audio publication preserves device observation time across service polls" \
        --out out/verification-20261031/r3-matrix-flake-904/cross-run-comparison.json
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path


def identity_digest(files: dict) -> str:
    return hashlib.sha256(json.dumps(files, sort_keys=True).encode()).hexdigest()


def locate(log: Path, case: str) -> dict:
    """Find the case's result line and its CTest index in one ctest log."""
    pattern = re.compile(
        r"^\s*\d+/\d+ Test\s+#(\d+): (.+?) \.+\s*(\*\*\*)?(Passed|Failed|Skipped|Not Run)\s")
    matches = []
    for line in log.read_text(encoding="utf-8", errors="replace").splitlines():
        found = pattern.match(line)
        if found and found.group(2).strip() == case:
            matches.append({"index": int(found.group(1)), "status": found.group(4),
                            "line": line.rstrip()})
    return {"log": str(log), "log_sha256": hashlib.sha256(log.read_bytes()).hexdigest(),
            "matches": matches}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--prior", required=True)
    parser.add_argument("--current", required=True)
    parser.add_argument("--case", required=True)
    parser.add_argument("--row", default="msvc-on-release")
    parser.add_argument("--out", required=True)
    args = parser.parse_args()

    output = Path(args.out).resolve()
    records = {}
    for label, directory in (("prior", args.prior), ("current", args.current)):
        run = Path(directory).resolve()
        manifest = json.loads((run / "manifest.json").read_text(encoding="utf-8"))
        source = manifest["source"]
        row = next(entry for entry in manifest["rows"] if entry["name"] == args.row)
        log = run / "{}-ctest.log".format(args.row)
        located = locate(log, args.case)
        records[label] = {
            "run": str(run),
            "head": source["head"],
            "recorded_worktree_sha256": source["worktree_sha256"],
            "recomputed_worktree_sha256": identity_digest(source["files"]),
            "implementation_sha256": source["implementation_sha256"],
            "file_count": len(source["files"]),
            "row": args.row,
            "row_status": row.get("status"),
            "row_test_counts": row.get("test_counts"),
            "cache_sha256": row.get("cache_sha256"),
            "row_source_unchanged": row.get("source_unchanged"),
            "case": located,
        }

    prior, current = records["prior"], records["current"]
    same_head = prior["head"] == current["head"]
    same_files = prior["recomputed_worktree_sha256"] == current["recomputed_worktree_sha256"]
    same_recorded = prior["recorded_worktree_sha256"] == current["recorded_worktree_sha256"]
    same_cache = prior["cache_sha256"] == current["cache_sha256"]
    prior_status = prior["case"]["matches"][0]["status"] if prior["case"]["matches"] else None
    current_status = current["case"]["matches"][0]["status"] if current["case"]["matches"] else None

    report = {
        "scope": (
            "Cross-run comparison of one CTest case between two acceptance-matrix runs. It shows "
            "whether the two runs bind to the same source, and what each run recorded for the case, "
            "so an intermittent failure cannot be read as a source difference."
        ),
        "case": args.case,
        "same_head": same_head,
        "same_source_files": same_files,
        "recorded_worktree_hashes_agree": same_recorded,
        "same_configure_cache_sha256": same_cache,
        "runs": records,
        "outcome": {
            "prior_status": prior_status,
            "current_status": current_status,
            "prior_test_index": prior["case"]["matches"][0]["index"] if prior["case"]["matches"] else None,
            "current_test_index": current["case"]["matches"][0]["index"] if current["case"]["matches"] else None,
        },
        "reading": (
            "Same HEAD, same per-file hash map and same configure cache: the case passed in the "
            "earlier run and failed in the later one under the same build configuration, so the "
            "failure is not attributable to a source or configuration change. The two CTest "
            "indices differ, which also shows why a case must be tracked by name rather than by "
            "number."
            if same_head and same_files and same_cache and prior_status == "Passed"
            and current_status == "Failed"
            else "The two runs differ in source, configuration or outcome; see the fields above and "
                 "do not read this comparison as an intermittent-failure finding."
        ),
        "command": "python -B compare_runs.py --prior {} --current {} --case \"{}\" --row {} --out {}"
                   .format(prior["run"], current["run"], args.case, args.row, output),
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("prior {} / current {}".format(prior_status, current_status))
    print("same head {} same source {}".format(same_head, same_files))
    print("wrote", output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

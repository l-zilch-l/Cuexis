"""Extract the terminal state of the aborted 2026-10-31 acceptance-matrix run.

The run that preceded the current one stopped on a full disk after seven rows. Its directory is not
part of any archive, so this script records the facts a report needs to cite: per-row status, test
counts, phase exit codes, the source identity the run bound to, and the exception that ended it.

    python -B summarize_aborted_run.py --run out/rpa-acceptance/20261031-220604-9e3ab6ac \
        --out out/verification-20261031/r3-matrix-flake-904/prior-run-status.json
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--run", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--log", required=True, help="stdout of the aborted run")
    args = parser.parse_args()

    run = Path(args.run).resolve()
    manifest_path = run / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))

    rows = []
    for record in manifest.get("rows", []):
        phases = record.get("phases") or []
        if isinstance(phases, dict):
            phases = [{"phase": name, **(value or {})} for name, value in phases.items()]
        rows.append({
            "name": record.get("name"),
            "status": record.get("status"),
            "test_counts": record.get("test_counts"),
            "skipped_tests": record.get("skipped_tests", []),
            "phases": [{"phase": entry.get("phase"), "exit_code": entry.get("exit_code"),
                        "elapsed_seconds": entry.get("elapsed_seconds"),
                        "log": entry.get("log"), "log_sha256": entry.get("log_sha256")}
                       for entry in phases],
            "measurement_count": len(record.get("measurements") or []),
            "source_unchanged": record.get("source_unchanged"),
            "build_directory": record.get("build_directory"),
        })

    log_path = Path(args.log).resolve()
    tail = []
    if log_path.is_file():
        lines = log_path.read_text(encoding="utf-8", errors="replace").splitlines()
        tail = lines[-25:]

    finished = [row for row in rows if row["status"] is not None]
    passed = [row for row in finished if row["status"] == "local_pass"]
    payload = {
        "scope": (
            "Terminal state of the acceptance-matrix run that was aborted by disk exhaustion, as "
            "recorded by its own manifest and stdout. It documents why the current run is the first "
            "complete execution of the same eleven rows, and it is not itself a matrix result."
        ),
        "run": str(run),
        "manifest": {"path": str(manifest_path), "sha256": digest(manifest_path),
                     "bytes": manifest_path.stat().st_size},
        "log": {"path": str(log_path), "sha256": digest(log_path) if log_path.is_file() else None,
                "bytes": log_path.stat().st_size if log_path.is_file() else None,
                "tail": tail},
        "source": {key: manifest["source"][key]
                   for key in ("head", "worktree_sha256", "implementation_sha256")},
        "manifest_source_unchanged": manifest.get("source_unchanged"),
        "rows": rows,
        "row_census": {"rows_in_manifest": len(manifest.get("rows", [])),
                       "rows_with_status": len(finished),
                       "rows_local_pass": len(passed),
                       "rows_without_status": [row["name"] for row in rows if row["status"] is None],
                       "note": ("The run died while running the eighth row, so the manifest holds "
                                "eight row records; the three mingw rows were never created and have "
                                "no record in this manifest.")},
        "reading": (
            "Seven rows reached local_pass, the eighth has no status, and the run never wrote a "
            "manifest-level source_unchanged value, so it cannot stand as a completed matrix."
            if len(finished) == 7 and len(passed) == 7 and manifest.get("source_unchanged") is None
            else "The recorded row census differs from the expected abort shape; read rows verbatim."
        ),
        "command": "python -B summarize_aborted_run.py --run {} --log {} --out {}"
                   .format(run, log_path, Path(args.out).resolve()),
    }

    output = Path(args.out).resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("rows", len(rows), "with status", len(finished), "local_pass", len(passed))
    print("wrote", output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

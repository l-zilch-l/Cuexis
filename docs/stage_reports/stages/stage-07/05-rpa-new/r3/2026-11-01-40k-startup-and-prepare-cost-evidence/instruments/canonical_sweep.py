#!/usr/bin/env python3
"""Record the SHA-256 of the canonical bytes that `cuexis_chart_migrator` writes.

The chart canonical text is the byte identity of a chart
(`chart::canonicalBytesIdentity` hashes it), so any change to the canonical
writer must leave these hashes untouched. This script fixes one corpus, runs the
migrator once per input in a fresh process, and records the digest of the output
file. Re-running the same corpus with a different binary and comparing the two
records is the byte-identity check for a writer refactor.

Usage:
    python -B canonical_sweep.py --migrator <exe> --work <dir> --out <json>
    python -B canonical_sweep.py --migrator <exe> --work <dir> --out <json> \
        --baseline <earlier-json>

With `--baseline`, the script also writes a `compare` block and exits non-zero
when any recorded digest differs.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import subprocess
import sys

# Pre-registered corpus. Relative to the repository root, resolved by the
# caller. The last entry is the 40K startup fixture, which lives under the
# gitignored `out/` tree and is archived separately as evidence.
CORPUS = [
    "assets/charts/stage1a_example.cuexis.chart.json",
    "assets/charts/stage2_example.cuexis.chart.json",
    "assets/projects/demo20s/assets/charts/demo20s.cuexis.chart.json",
    "assets/projects/stage1b_project/assets/charts/stage1b_example.cuexis.chart.json",
    "assets/projects/stage1c_project/assets/charts/stage1c_example.cuexis.chart.json",
    "assets/projects/stage1d_project/assets/charts/stage1d_example.cuexis.chart.json",
    "assets/projects/stage3_project/assets/charts/stage3_example.cuexis.chart.json",
    "tests/fixtures/chart_format_update/cfu_f_reference_project/assets/charts/main.cuexis.chart.json",
    "tests/fixtures/chart_format_update/source_project/assets/charts/main.cuexis.chart.json",
    "tests/fixtures/chart_format_update/static_project/assets/charts/main.cuexis.chart.json",
    "tests/fixtures/chart_format_update/parameterized_project/assets/charts/main.cuexis.chart.json",
    "tests/fixtures/chart_format_update/golden/chart_v1_to_v4.cuexis.chart.json",
    "tests/fixtures/chart_format_update/valid/chart_v3_static_migration.json",
    "tests/fixtures/chart_format_update/valid/chart_v4_animation.json",
    "tests/fixtures/chart_format_update/valid/chart_v4_cxt_template_binding.json",
    "tests/fixtures/chart_format_update/valid/chart_v4_parameterized_rational.json",
    "tests/fixtures/chart_format_update/valid/chart_v4_parameterized_transform.json",
    "tests/fixtures/chart_format_update/valid/chart_v4_template_animator.json",
    "tests/fixtures/chart_format_update/valid/chart_v4_static_migration.json",
    "tests/fixtures/stage1b_project/assets/charts/stage1b_example.cuexis.chart.json",
    "tests/fixtures/stage2_migration_v1.cuexis.chart.json",
    "out/verification-20261101/r3-startup-observability-v1/player-40k-project/assets/charts/stress_40k.cuexis.chart.json",
]

# Tried in order; the first target that the migrator accepts is recorded.
TARGETS = ["3", "4"]


def digest(path: pathlib.Path) -> tuple[str, int]:
    data = path.read_bytes()
    return hashlib.sha256(data).hexdigest(), len(data)


def run_one(migrator: pathlib.Path, root: pathlib.Path, work: pathlib.Path, rel: str) -> dict:
    source = root / rel
    if not source.is_file():
        return {"input": rel, "status": "missing"}
    source_sha, source_bytes = digest(source)
    stem = hashlib.sha256(rel.encode("utf-8")).hexdigest()[:16]
    record: dict = {
        "input": rel,
        "source_sha256": source_sha,
        "source_bytes": source_bytes,
    }
    attempts = []
    for target in TARGETS:
        out_file = work / f"{stem}-t{target}.json"
        report_file = work / f"{stem}-t{target}.report.json"
        for stale in (out_file, report_file):
            if stale.exists():
                stale.unlink()
        completed = subprocess.run(
            [
                str(migrator),
                "--input",
                str(source),
                "--output",
                str(out_file),
                "--report",
                str(report_file),
                "--target",
                target,
            ],
            cwd=str(root),
            capture_output=True,
            timeout=900,
        )
        attempt = {
            "target": target,
            "return_code": completed.returncode,
            "stdout": completed.stdout.decode("utf-8", "replace").strip()[:400],
            "stderr": completed.stderr.decode("utf-8", "replace").strip()[:400],
            "wrote_output": out_file.is_file(),
        }
        if out_file.is_file():
            attempt["output_sha256"], attempt["output_bytes"] = digest(out_file)
        attempts.append(attempt)
        if completed.returncode == 0 and out_file.is_file():
            record["status"] = "canonicalized"
            record["target"] = target
            record["output_sha256"] = attempt["output_sha256"]
            record["output_bytes"] = attempt["output_bytes"]
            record["attempts"] = attempts
            # Keep the canonical bytes for offline inspection of the 40K case only.
            if source_bytes > 8 * 1024 * 1024:
                record["output_path"] = str(out_file)
            else:
                out_file.unlink(missing_ok=True)
            return record
        out_file.unlink(missing_ok=True)
    record["status"] = "unsupported"
    record["attempts"] = attempts
    return record


def compare(recorded: dict, baseline: dict) -> dict:
    base = {row["input"]: row for row in baseline.get("cases", [])}
    differences = []
    for row in recorded.get("cases", []):
        other = base.get(row["input"])
        if other is None:
            differences.append({"input": row["input"], "reason": "new-case"})
            continue
        for key in ("status", "target", "source_sha256", "output_sha256", "output_bytes"):
            if row.get(key) != other.get(key):
                differences.append(
                    {
                        "input": row["input"],
                        "field": key,
                        "baseline": other.get(key),
                        "recorded": row.get(key),
                    }
                )
    for name in base:
        if not any(row["input"] == name for row in recorded.get("cases", [])):
            differences.append({"input": name, "reason": "missing-case"})
    return {"differences": differences, "identical": not differences}


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--migrator", required=True)
    parser.add_argument("--work", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--baseline")
    parser.add_argument("--root", default=".")
    args = parser.parse_args(argv)

    root = pathlib.Path(args.root).resolve()
    migrator = pathlib.Path(args.migrator).resolve()
    work = pathlib.Path(args.work).resolve()
    work.mkdir(parents=True, exist_ok=True)

    tool = {
        "migrator": str(migrator),
        "migrator_sha256": digest(migrator)[0],
        "migrator_bytes": digest(migrator)[1],
    }
    cases = []
    for rel in CORPUS:
        record = run_one(migrator, root, work, rel)
        cases.append(record)
        print(
            f"{record['status']:<14} {record.get('target', '-'):<2} "
            f"{record.get('output_bytes', 0):>9} {record.get('output_sha256', '-')[:16]:<16} {rel}",
            flush=True,
        )

    payload = {"tool": tool, "corpus_size": len(CORPUS), "cases": cases}
    if args.baseline:
        baseline = json.loads(pathlib.Path(args.baseline).read_bytes().decode("utf-8"))
        payload["baseline_tool"] = baseline.get("tool")
        payload["compare"] = compare(payload, baseline)

    out_path = pathlib.Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(
        (json.dumps(payload, indent=2, ensure_ascii=False) + "\n").encode("utf-8")
    )
    print(f"wrote {out_path}", flush=True)
    if args.baseline:
        result = payload["compare"]
        print(
            f"compare: identical={result['identical']} differences={len(result['differences'])}",
            flush=True,
        )
        for item in result["differences"]:
            print(f"  DIFF {item}", flush=True)
        return 0 if result["identical"] else 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

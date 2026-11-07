#!/usr/bin/env python3
"""Verify the recorded final RPA acceptance matrix without editing it."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

EXPECTED = {
    "msvc-on-debug": (1418, 20, 13),
    "msvc-on-release": (1418, 20, 13),
    "msvc-on-shared-debug": (1401, 20, 13),
    "msvc-off-debug": (1211, 0, 3),
    "msvc-off-headless-debug": (1066, 0, 3),
    "msvc-off-release": (1211, 0, 3),
    "msvc-off-headless-release": (1066, 0, 3),
    "msvc-off-shared-debug": (1064, 0, 3),
    "mingw-on-debug": (1266, 20, 13),
    "mingw-on-shared-debug": (1248, 20, 13),
    "mingw-on-release": (1266, 20, 13),
}


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def normalized(data: bytes) -> bytes:
    return data.replace(b"\r\n", b"\n").replace(b"\r", b"\n")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("manifest", type=Path)
    parser.add_argument("--source-root", type=Path)
    parser.add_argument("--fresh-checkout", type=Path)
    parser.add_argument("--content-manifest", type=Path)
    args = parser.parse_args()

    run = args.manifest.resolve().parent
    manifest_bytes = args.manifest.read_bytes()
    manifest = json.loads(manifest_bytes)
    errors: list[str] = []
    rows = manifest.get("rows", [])

    if len(rows) != len(EXPECTED):
        errors.append(f"row count {len(rows)} != {len(EXPECTED)}")
    if manifest.get("source_unchanged") is not True:
        errors.append("manifest source_unchanged is not true")
    if manifest.get("source") != manifest.get("source_after"):
        errors.append("source before/after differ")
    if manifest.get("final_acceptance") is not False:
        errors.append("final_acceptance must remain false")
    if manifest.get("production_thresholds_accepted") is not False:
        errors.append("production_thresholds_accepted must remain false")

    seen: set[str] = set()
    total_tests = 0
    total_failures = 0
    total_measurements = 0
    for row in rows:
        name = row.get("name", "")
        seen.add(name)
        if name not in EXPECTED:
            errors.append(f"unexpected row {name!r}")
            continue
        tests, measurements, phases = EXPECTED[name]
        counts = row.get("test_counts") or []
        if len(counts) != 3:
            errors.append(f"{name}: malformed test_counts")
            continue
        percent, failed, total = counts
        if failed != 0 or percent != 100 or total != tests:
            errors.append(f"{name}: test_counts {counts} != expected 100 percent / 0 failures / {tests}")
        if row.get("status") != "local_pass":
            errors.append(f"{name}: status {row.get('status')!r}")
        if row.get("source_unchanged") is not True:
            errors.append(f"{name}: source_unchanged is not true")
        if len(row.get("measurements", [])) != measurements:
            errors.append(f"{name}: measurement count mismatch")
        if len(row.get("phases", [])) != phases:
            errors.append(f"{name}: phase count mismatch")
        for phase in row.get("phases", []):
            if phase.get("exit_code") != 0:
                errors.append(f"{name}: phase {phase.get('phase')} exit != 0")
            log = run / str(phase.get("log", ""))
            if not log.is_file():
                errors.append(f"{name}: missing phase log {log.name}")
            elif sha256(log.read_bytes()) != phase.get("log_sha256"):
                errors.append(f"{name}: phase log hash mismatch {log.name}")
        total_tests += total
        total_failures += failed
        total_measurements += len(row.get("measurements", []))

    if seen != set(EXPECTED):
        errors.append(f"row names differ: missing={sorted(set(EXPECTED) - seen)}")

    source_files_checked = 0
    raw_recorded_matches = 0
    content_manifest_bytes = b""
    if args.source_root or args.fresh_checkout:
        if not (args.source_root and args.fresh_checkout):
            parser.error("--source-root and --fresh-checkout must be supplied together")
        content_path = args.content_manifest or (Path(__file__).resolve().parent /
                                                 "source-content-sha256.json")
        content_manifest_bytes = content_path.read_bytes()
        content_manifest = json.loads(content_manifest_bytes)
        content_files = content_manifest.get("files", {})
        if content_manifest.get("source_head") != manifest["source"]["head"]:
            errors.append("source content manifest head mismatch")
        files = manifest["source"]["files"]
        for relative, recorded_sha in files.items():
            source = args.source_root / relative
            fresh = args.fresh_checkout / relative
            if not source.is_file() or not fresh.is_file():
                errors.append(f"identity file missing: {relative}")
                continue
            source_bytes = source.read_bytes()
            if sha256(source_bytes) == recorded_sha:
                raw_recorded_matches += 1
            expected_content_sha = content_files.get(relative)
            if expected_content_sha is None:
                errors.append(f"source content manifest missing: {relative}")
            elif sha256(normalized(source_bytes)) != expected_content_sha:
                errors.append(f"source content mismatch: {relative}")
            elif sha256(normalized(fresh.read_bytes())) != expected_content_sha:
                errors.append(f"fresh checkout content mismatch: {relative}")
            source_files_checked += 1

    report = {
        "schema": "cuexis.rpa-acceptance-verification.v1",
        "manifest": str(args.manifest.resolve()),
        "manifest_sha256": sha256(manifest_bytes),
        "rows": len(rows),
        "tests": total_tests,
        "failures": total_failures,
        "measurements": total_measurements,
        "source_files_checked": source_files_checked,
        "raw_recorded_worktree_hash_matches": raw_recorded_matches,
        "source_content_manifest_sha256": sha256(content_manifest_bytes) if content_manifest_bytes else None,
        "identity_mode": "archived line-ending-normalized source content" if source_files_checked else "manifest-only",
        "errors": errors,
        "status": "pass" if not errors else "fail",
    }
    print(json.dumps(report, indent=2))
    return 0 if not errors else 1


if __name__ == "__main__":
    raise SystemExit(main())

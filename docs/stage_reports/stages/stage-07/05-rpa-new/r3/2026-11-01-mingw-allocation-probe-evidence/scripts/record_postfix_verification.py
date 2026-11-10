"""Re-run the suites that the probe defect broke and record the result as evidence.

The archive claims that the MinGW rows failed because of the test instrument and that the instrument
now passes when the offset-payload replacement is compiled out. That claim is only worth the
transcript behind it, so this script runs each affected suite as a fresh process and records the
Catch2 summary line, the exit code and the executable's sha256 and byte count.

Usage:
    python -B record_postfix_verification.py [--work <dir>]

Every entry is asserted to exit 0 *and* to keep its recorded assertion and case counts, because the
counts are what tells a passing suite apart from one that silently registered fewer tests. A rebuild
that changes them makes this script fail instead of quietly relaxing the record.
"""

import argparse
import hashlib
import json
import platform
import os
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(r"D:\Cuexis-redesign")
HERE = Path(__file__).resolve().parent
SUMMARY = re.compile(r"All tests passed \((\d+) assertions? in (\d+) test cases?\)")

TARGETS = [
    {
        "name": "mingw-gameplay-packed",
        "build": "mingw-headless-debug",
        "binary": "cuexis_gameplay_packed_tests.exe",
        "assertions": 5643,
        "cases": 51,
        "why": "the suite whose 51 cases aborted with 0xc0000374 in run 2 row 9",
    },
    {
        "name": "mingw-allocation-probe",
        "build": "mingw-headless-debug",
        "binary": "cuexis_pd_allocation_probe.exe",
        "assertions": 16414,
        "cases": 2,
        "why": "the probe's own suite with the replacement compiled out (resource-only topology)",
    },
    {
        "name": "mingw-baseline-probe",
        "build": "mingw-headless-debug",
        "binary": "cuexis_pd_baseline_probe.exe",
        "assertions": 16844,
        "cases": 6,
        "why": "the baseline probe shares the header and must stay unaffected",
    },
    {
        "name": "msvc-debug-gameplay-packed",
        "build": "debug",
        "binary": "cuexis_gameplay_packed_tests.exe",
        "assertions": 5641,
        "cases": 51,
        "why": "the same suite on the topology that always ran the replacement, as a control",
    },
    {
        "name": "msvc-candidate-allocation-probe",
        "build": "candidate-debug",
        "binary": "cuexis_pd_allocation_probe.exe",
        "assertions": 16428,
        "cases": 4,
        "why": "the probe's own suite on MSVC, where the observers add two cases",
    },
    {
        "name": "msvc-candidate-gameplay-packed",
        "build": "candidate-debug",
        "binary": "cuexis_gameplay_packed_tests.exe",
        "assertions": 5641,
        "cases": 51,
        "why": "the same suite in the candidate configuration of run 2 row 1",
    },
]


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--work", type=Path, default=HERE / "post-fix")
    arguments = parser.parse_args()
    work = arguments.work.resolve()
    work.mkdir(parents=True, exist_ok=True)
    machine = platform.platform()
    records = []
    failures = 0
    for target in TARGETS:
        executable = REPO / "out" / "build" / target["build"] / "bin" / target["binary"]
        if not executable.is_file():
            raise SystemExit("missing binary: {}".format(executable))
        ran = subprocess.run([str(executable)], cwd=str(executable.parent), capture_output=True,
                             timeout=1800)
        output = (ran.stdout + ran.stderr).decode("utf-8", "replace")
        log = work / (target["name"] + ".log")
        with open(log, "w", encoding="utf-8", newline="\r\n") as stream:
            stream.write(output)
        match = None
        for line in output.splitlines():
            found = SUMMARY.search(line)
            if found:
                match = found
        assertions = int(match.group(1)) if match else None
        cases = int(match.group(2)) if match else None
        matched = (ran.returncode == 0 and assertions == target["assertions"] and
                   cases == target["cases"])
        if not matched:
            failures += 1
        records.append({
            "name": target["name"],
            "build": target["build"],
            "binary": target["binary"],
            "binary_bytes": executable.stat().st_size,
            "binary_sha256": sha256_file(executable),
            "exit": ran.returncode,
            "assertions": assertions,
            "cases": cases,
            "expected_assertions": target["assertions"],
            "expected_cases": target["cases"],
            "why": target["why"],
            "log": log.name,
            "matches_expectation": matched,
        })
        print("{}: exit={} assertions={} cases={}{}".format(
            target["name"], ran.returncode, assertions, cases,
            "" if matched else "  MISMATCH"))
    payload = {"machine": machine, "working_directory": str(REPO), "runs": records,
               "mismatches": failures}
    summary = work / "summary.json"
    with open(summary, "w", encoding="utf-8", newline="\r\n") as stream:
        json.dump(payload, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    print("wrote {} ({} mismatches)".format(summary, failures))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())

"""Isolate what actually breaks the heap, without the test framework in the way.

The framework reproduction shows a difference between the two C++ runtimes, but it cannot say whether
the difference is the *payload's shape* (a pointer sixteen bytes into a block) or the *release path*
that receives it. This control removes the framework and answers both halves directly:

  roundtrip   new[]/delete[], a heap `std::string`, and an aligned `new []`/`delete []` - every
              release goes back to the replacement `operator delete`.
  mismatch    the same, plus one `::operator new(65)` released with `std::free`, which is the shape
              of a release path that bypasses the replacement.

`archive-src/isolation-no-framework.cpp` is built twice, against the same as-run header bytes and
with the same compiler, once with the DLL C++ runtime and once with it linked statically. Four runs:

  dynamic * roundtrip   the payload survives a matched round trip
  dynamic * mismatch    the payload reaches the CRT `free`, and the heap is corrupted
  static  * roundtrip   the same matched round trip survives without the DLL
  static  * mismatch    the same mismatched release is just as fatal without the DLL

The point is the cross: the payload's shape is harmless on every matched path in both runtimes, and
fatal on the mismatched path in both runtimes. So "the offset payload is inherently unsafe" is false,
"the DLL runtime is inherently unsafe" is false, and what remains is the one thing the framework
reproduction proved by trace: a release path inside the DLL receives a payload. The transcripts under
`gdb/` name that path; this matrix removes the alternative explanations.

Usage:
    python -B record_isolation_matrix.py

It exits non-zero when a run's exit status or its printed markers stop matching the recorded
expectation, so a stale matrix cannot keep asserting an old result.
"""

import argparse
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(r"D:\Cuexis-redesign")
CXX = Path(r"C:\msys64\ucrt64\bin\c++.exe")
HERE = Path(__file__).resolve().parent
SOURCE = HERE / "archive-src" / "isolation-no-framework.cpp"
TRACE = HERE / "trace"
OUT = HERE / "isolation"
AS_RUN_HEAD = "3fbc31f68430ce2d0ad576094ff617cc09772f10"
AS_RUN_PATH = "tests/pd/pd_allocation_probe.hpp"
AS_RUN_RUN = REPO / "out" / "rpa-acceptance" / "20261101-021741-4daf2bb2"
AS_RUN_COPY = HERE / "repro" / "pd_allocation_probe.as-run-2.hpp"

BASE = ["-std=c++20", "-O0", "-g"]
# No probe define: the replacement operators must be installed, which is what `#if
# !defined(CUEXIS_PD_ALLOCATION_PROBE_RESOURCE_ONLY)` in the as-run header compiles.
DEFINES = []

BUILDS = [
    {"name": "dynamic-runtime", "extra": []},
    {"name": "static-runtime", "extra": ["-static-libstdc++", "-static-libgcc"]},
]

MODES = [
    {
        "name": "roundtrip",
        "expect_exit": "0x00000000",
        "expect_done": True,
        "expect_mismatch_block": False,
        "why": "every release returns to the replacement delete, so the offset payload is never seen "
               "by the CRT as a block header",
    },
    {
        "name": "mismatch",
        "expect_exit": "0xc0000374",
        "expect_done": False,
        "expect_mismatch_block": True,
        "why": "a payload of the replacement is handed to std::free, which is the corruption the "
               "framework reproduction triggers through the DLL's own delete",
    },
]


def sha256_file(path):
    with open(path, "rb") as stream:
        return hashlib.sha256(stream.read()).hexdigest()


def text(value):
    return value.decode("utf-8", "replace")


def stage_as_run_header():
    blob = subprocess.run(["git", "-C", str(REPO), "show", "{}:{}".format(AS_RUN_HEAD, AS_RUN_PATH)],
                          capture_output=True)
    if blob.returncode != 0:
        raise SystemExit("git show failed: " + text(blob.stderr))
    digest = hashlib.sha256(blob.stdout).hexdigest()
    recorded = json.loads((AS_RUN_RUN / "manifest.json").read_text(encoding="utf-8"))["source"]["files"][AS_RUN_PATH]
    if recorded != digest:
        raise SystemExit("as-run header does not reproduce run 2's recorded hash: {} != {}".format(digest, recorded))
    include = TRACE / "as-run-include"
    include.mkdir(parents=True, exist_ok=True)
    (include / "pd_allocation_probe.hpp").write_bytes(blob.stdout)
    if sha256_file(AS_RUN_COPY) != digest:
        raise SystemExit("staged reproduction copy diverged from the as-run header")
    return {"source": "git show {}:{}".format(AS_RUN_HEAD[:8], AS_RUN_PATH),
            "sha256": digest, "bytes": len(blob.stdout),
            "manifest_run": str(AS_RUN_RUN.relative_to(REPO)), "manifest_sha256": recorded}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.parse_args()
    if not CXX.is_file():
        raise SystemExit("missing compiler: {}".format(CXX))
    if not SOURCE.is_file():
        raise SystemExit("missing isolation source: {}".format(SOURCE))
    OUT.mkdir(parents=True, exist_ok=True)
    header = stage_as_run_header()
    version = subprocess.run([str(CXX), "--version"], capture_output=True, text=True).stdout.splitlines()[0]

    runs = []
    failures = []
    for build in BUILDS:
        binary = TRACE / ("isolation-" + build["name"] + ".exe")
        command = ([str(CXX)] + BASE + DEFINES
                   + ["-I" + str(TRACE / "as-run-include"), "-I" + str(REPO / "engine" / "core" / "internal")]
                   + build["extra"] + ["-o", str(binary), str(SOURCE)])
        compiled = subprocess.run(command, cwd=str(HERE), capture_output=True)
        if compiled.returncode != 0:
            raise SystemExit("{} failed to compile:\n{}".format(build["name"], text(compiled.stderr)[-4000:]))
        for mode in MODES:
            started = subprocess.run([str(binary), mode["name"]], cwd=str(HERE), capture_output=True, timeout=900)
            code = "0x{:08x}".format(started.returncode & 0xFFFFFFFF)
            transcript = OUT / "{}-{}.txt".format(build["name"], mode["name"])
            with open(transcript, "w", encoding="utf-8", newline="\n") as stream:
                stream.write(text(started.stdout) + text(started.stderr))
            stdout = text(started.stdout)
            checks = {
                "exit_status": code == mode["expect_exit"],
                "printed_done": ("DONE" in stdout) == mode["expect_done"],
                "mismatch_block_printed": ("MISMATCH block=" in stdout) == mode["expect_mismatch_block"],
                "probe_block_published": bool(re.search(r"^PROBE_BLOCK ptr=", stdout, re.MULTILINE)),
            }
            probe = re.search(r"^PROBE_BLOCK ptr=0x[0-9a-f]+ w_minus_16=(0x[0-9a-f]+)", stdout, re.MULTILINE)
            record = {
                "build": build["name"],
                "mode": mode["name"],
                "why": mode["why"],
                "expected_exit": mode["expect_exit"],
                "exit_status": code,
                "extra_flags": build["extra"],
                "compile_command": [str(part) for part in command],
                "compile_exit": compiled.returncode,
                "binary": str(binary.relative_to(HERE)),
                "binary_sha256": sha256_file(binary),
                "transcript": str(transcript.relative_to(HERE)),
                "transcript_sha256": sha256_file(transcript),
                "published_block_offset": 16 if probe else None,
                "checks": checks,
                "matches_expectation": all(checks.values()),
            }
            if not record["matches_expectation"]:
                failures.append({"run": "{}-{}".format(build["name"], mode["name"]),
                                 "failed": sorted(k for k, v in checks.items() if not v)})
            runs.append(record)
            print("{:16} {:9} exit={} ok={}".format(build["name"], mode["name"], code,
                                                    record["matches_expectation"]))

    payload = {
        "compiler": str(CXX),
        "compiler_version": version,
        "flags": BASE,
        "defines": DEFINES,
        "source": str(SOURCE.relative_to(HERE)),
        "source_sha256": sha256_file(SOURCE),
        "header": header,
        "runs": runs,
        "conclusion": "the offset payload is harmless on every matched release path in both runtimes and "
                      "fatal on a release path that bypasses the replacement in both runtimes, so the "
                      "framework reproduction's runtime dependence is a difference in which release paths "
                      "exist, not a difference in the payload",
        "mismatches": failures,
    }
    record_path = OUT / "isolation-matrix.json"
    with open(record_path, "w", encoding="utf-8", newline="\r\n") as stream:
        json.dump(payload, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    print("wrote {} ({} runs, {} mismatches)".format(record_path, len(runs), len(failures)))
    for failure in failures:
        print("  MISMATCH {}: {}".format(failure["run"], ", ".join(failure["failed"])))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Reproduce the R3c head-port byte accounting readings.

Runs the hidden Catch2 diagnostic `R3c accounting diagnostic` for a fixed set of
(requirement count, mind-set wave) fixtures and turns its JSON lines into one
result document with the source and binary identity attached.

The diagnostic is environment driven on purpose: it must stay out of the daily
gate set, and the same binary has to produce every row so the rows remain
comparable.

Usage:
    python -B run_r3c_accounting.py --worktree D:/Cuexis-r3c-v4 \
        --binary D:/Cuexis-r3c-v4/out/build/r3c-debug/bin/cuexis_rpa_tests.exe \
        --out accounting-results.json
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import re
import subprocess
import sys

SOURCES = [
    "engine/judgement/include/cuexis/judgement/shared_sequence.hpp",
    "engine/judgement/src/execution_kernel.hpp",
    "engine/judgement/src/execution_kernel_advance.cpp",
    "engine/judgement/src/execution_kernel_archive.cpp",
    "engine/judgement/src/execution_kernel_impl_setup.cpp",
    "engine/judgement/src/execution_kernel_internal.hpp",
    "engine/judgement/src/execution_kernel_query.cpp",
    "engine/judgement/src/execution_kernel_restore.cpp",
    "engine/judgement/src/recovery_codec.cpp",
    "tests/rpa/CMakeLists.txt",
    "tests/rpa/rpa_b3_r3c_tests.cpp",
]

# (requirements, mind-set wave, admission limit in MiB, capture arm).
# Wave 0 keeps every requirement in one window, so the fixture only closes one window and
# stages a single point; wave 256 spreads them across six windows so the slot rotation and
# the cross-generation retention both appear. The capture=0 rows run the same fixture and the
# same journal growth through advanceForReplay, so the difference between the two arms at the
# same horizon isolates what the optional payload keeps alive. The 40K quota rows exist because
# the open owner question is which limit buys which Seek coverage, not whether the cache is
# bounded.
CASES = [
    (1000, 0, 0, 1),
    (1536, 256, 0, 1),
    (1536, 256, 0, 0),
    (40000, 0, 0, 1),
    (40000, 256, 0, 1),
    (40000, 256, 0, 0),
    (40000, 256, 256, 1),
    (40000, 256, 512, 1),
]

FILTER = "[.][r3c-accounting]"


def sha256_file(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def git(worktree: pathlib.Path, *args: str) -> str:
    result = subprocess.run(
        ["git", *args], cwd=worktree, capture_output=True, text=True, check=True
    )
    return result.stdout.strip()


class ProcessSampler:
    """Process-wide private bytes / working set of one child process.

    Observation only: the product test never contains measurement code. On a platform without
    the Windows kernel call the sampler degrades to no readings instead of failing the run.
    """

    _COUNTERS_SIZE = 80  # sizeof(PROCESS_MEMORY_COUNTERS_EX) on x64

    def __init__(self) -> None:
        self._k32 = None
        if sys.platform == "win32":
            import ctypes
            import ctypes.wintypes as wintypes

            self._ctypes = ctypes
            k32 = ctypes.WinDLL("kernel32", use_last_error=True)
            k32.K32GetProcessMemoryInfo.argtypes = [
                wintypes.HANDLE,
                ctypes.c_void_p,
                wintypes.DWORD,
            ]
            k32.K32GetProcessMemoryInfo.restype = wintypes.BOOL
            self._k32 = k32
            self._wintypes = wintypes

    @property
    def available(self) -> bool:
        return self._k32 is not None

    def sample(self, process: subprocess.Popen) -> dict | None:
        if not self.available:
            return None
        handle = self._ctypes.c_void_p(int(process._handle))  # noqa: SLF001 - documented CPython field
        buffer = self._ctypes.create_string_buffer(self._COUNTERS_SIZE)
        ok = self._k32.K32GetProcessMemoryInfo(handle, buffer, self._COUNTERS_SIZE)
        if not ok:
            return None
        import struct

        raw = buffer.raw
        peak_working_set = struct.unpack_from("<Q", raw, 8)[0]
        working_set = struct.unpack_from("<Q", raw, 16)[0]
        pagefile = struct.unpack_from("<Q", raw, 56)[0]
        private = struct.unpack_from("<Q", raw, 72)[0]
        return {
            "private_bytes": private,
            "working_set_bytes": working_set,
            "peak_working_set_bytes": peak_working_set,
            "pagefile_bytes": pagefile,
        }


def run_case(
    binary: pathlib.Path,
    worktree: pathlib.Path,
    n: int,
    wave: int,
    limit_mib: int,
    capture: int,
    sampler: ProcessSampler | None,
) -> dict:
    env = dict(os.environ)
    env["R3C_ACCOUNTING_N"] = str(n)
    env["R3C_ACCOUNTING_WAVE"] = str(wave)
    env["R3C_ACCOUNTING_CAPTURE"] = str(capture)
    if limit_mib:
        env["R3C_ACCOUNTING_LIMIT_MIB"] = str(limit_mib)
    else:
        env.pop("R3C_ACCOUNTING_LIMIT_MIB", None)
    # Barriers keep the product-side change to stdin reads: one before the fixture exists and,
    # when a sampler is available, one after every printed line so the reading belongs to that
    # exact point instead of racing the whole run.
    sampled = bool(sampler and sampler.available)
    env["R3C_ACCOUNTING_HOLD"] = "1" if sampled else "0"
    env["R3C_ACCOUNTING_STEP"] = "1" if sampled else "0"

    process = subprocess.Popen(
        [str(binary), FILTER, "--reporter", "compact"],
        cwd=worktree,
        env=env,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1,
    )
    readings = {"baseline": None, "peak": None, "released": None, "horizons": []}
    events: list[dict] = []
    stdout_tail: list[str] = []
    hold = sampled

    def poll() -> dict | None:
        if not sampler or not sampler.available:
            return None
        current = sampler.sample(process)
        if current is None:
            return None
        if readings["peak"] is None or current["private_bytes"] > readings["peak"]["private_bytes"]:
            readings["peak"] = dict(current)
        return current

    readings["baseline"] = sampler.sample(process) if sampled else None
    process.stdin.write("run\n")
    process.stdin.flush()
    released_sampled = False
    while True:
        line = process.stdout.readline()
        if not line:
            break
        stdout_tail.append(line.rstrip("\n"))
        stripped = line.strip()
        if hold and '"event":"r3c.accounting_final"' in stripped:
            released_sampled = True
            readings["released"] = sampler.sample(process)
            process.stdin.write("release\n")
            process.stdin.flush()
            continue
        if stripped.startswith('{"event":"r3c.accounting'):
            event = json.loads(stripped)
            events.append(event)
            sample = poll()
            if sample is not None and "horizon" in event:
                readings["horizons"].append({"horizon": event["horizon"], **sample})
            if hold:
                process.stdin.write("step\n")
                process.stdin.flush()
    process.wait()
    stderr = process.stderr.read()
    stdout = "\n".join(stdout_tail)
    passed = "All tests passed" in stdout
    return {
        "requirements": n,
        "wave": wave,
        "capture": bool(capture),
        "limit_mib_requested": limit_mib,
        "exit_code": process.returncode,
        "diagnostic_passed": passed,
        "events": events,
        "process_memory": readings,
        "stderr_tail": stderr.strip().splitlines()[-3:],
    }


def attribute_cache(document: dict) -> None:
    """Subtract the replay-only arm from the capture arm at the same horizon."""
    by_key: dict[tuple[int, int, int], dict[bool, dict]] = {}
    for case in document["cases"]:
        key = (case["requirements"], case["wave"], case["limit_mib_requested"])
        by_key.setdefault(key, {})[case["capture"]] = case
    for case in document["cases"]:
        key = (case["requirements"], case["wave"], case["limit_mib_requested"])
        pair = by_key[key]
        if len(pair) != 2:
            continue
        reference = pair[not case["capture"]]["process_memory"]
        mine = case["process_memory"]
        if not reference["horizons"] or not mine["horizons"]:
            continue
        by_horizon = {row["horizon"]: row for row in reference["horizons"]}
        rows = []
        for row in mine["horizons"]:
            other = by_horizon.get(row["horizon"])
            if other is None:
                continue
            rows.append(
                {
                    "horizon": row["horizon"],
                    "private_bytes": row["private_bytes"],
                    "replay_only_private_bytes": other["private_bytes"],
                    "cache_attributed_bytes": row["private_bytes"] - other["private_bytes"],
                }
            )
        case["cache_attribution"] = rows


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--worktree", required=True, type=pathlib.Path)
    parser.add_argument("--binary", required=True, type=pathlib.Path)
    parser.add_argument("--out", required=True, type=pathlib.Path)
    args = parser.parse_args()

    worktree = args.worktree.resolve()
    binary = args.binary.resolve()
    patch = subprocess.run(
        ["git", "diff", "HEAD"], cwd=worktree, capture_output=True, check=True
    ).stdout

    sampler = ProcessSampler()
    document = {
        "scope": (
            "Fixed diagnostic only; test-only Catch2 case behind a hidden tag. "
            "No formal CI, no product quota acceptance, no stage exit. Process memory is "
            "observation only, sampled from outside the test process."
        ),
        "process_memory_method": (
            "K32GetProcessMemoryInfo private bytes / working set, sampled from the harness"
            if sampler.available
            else "unavailable on this platform; payload charge only"
        ),
        "source": {
            "head": git(worktree, "rev-parse", "HEAD"),
            "diff_sha256": hashlib.sha256(patch).hexdigest(),
            "files": {name: sha256_file(worktree / name) for name in SOURCES},
        },
        "binary": {
            "path": str(binary),
            "sha256": sha256_file(binary),
            "size": binary.stat().st_size,
        },
        "filter": FILTER,
        "cases": [],
    }
    for n, wave, limit_mib, capture in CASES:
        case = run_case(binary, worktree, n, wave, limit_mib, capture, sampler)
        document["cases"].append(case)
        if case["exit_code"] != 0 or not case["diagnostic_passed"]:
            print(f"case n={n} wave={wave} limit={limit_mib} failed", file=sys.stderr)
            print("\n".join(case["stderr_tail"]), file=sys.stderr)
            return 1
        memory = case["process_memory"]
        delta = ""
        if memory["baseline"] and memory["peak"]:
            delta = (
                f" private_delta={(memory['peak']['private_bytes'] - memory['baseline']['private_bytes']) / (1 << 20):.2f}MiB"
            )
        print(
            f"n={n} wave={wave} limit={limit_mib}MiB capture={capture}: "
            f"{len(case['events'])} events{delta}"
        )

    attribute_cache(document)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

"""Characterize one intermittent CTest failure recorded by the 2026-11-01 matrix.

The same-source matrix row `msvc-on-release` failed exactly one test:

    904: Audio publication preserves device observation time across service polls

The test (tests/rpa/rpa_b5_audio_tests.cpp:52) starts an AudioWorker with a fake device, sleeps
20 ms, samples ``publication()`` twice, and expects a strictly increasing sequence with the
device's observation fields (sampleMonotonicUs 123, maximumPresentedFrame 48000, errorBoundUs 7).
The recorded failure shows every field at its zero value, i.e. the worker had not published yet.

This script does not re-run the matrix. It answers one question only: is that failure
deterministic on this source and binary, or intermittent? It reuses the exact executable that
failed, in three regimes, and never rebuilds anything.

Usage:
    python -B diagnose_audio_publication_flake.py --exe <cuexis_execution_host_tests.exe>
"""

import argparse
import concurrent.futures
import hashlib
import json
import os
import subprocess
import time

CASE = "Audio publication preserves device observation time across service polls"


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as stream:
        for chunk in iter(lambda: stream.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def environment(exe):
    env = dict(os.environ)
    env["PATH"] = os.path.dirname(exe) + os.pathsep + env.get("PATH", "")
    return env


def run_once(exe, env):
    started = time.perf_counter()
    completed = subprocess.run([exe, CASE], capture_output=True, text=True, env=env,
                               cwd=os.path.dirname(exe))
    elapsed = time.perf_counter() - started
    combined = (completed.stdout or "") + (completed.stderr or "")
    return {
        "exit_code": completed.returncode,
        "elapsed_seconds": round(elapsed, 3),
        "passed": completed.returncode == 0,
        "assertions_failed": combined.count("FAILED:"),
        "tail": [line for line in combined.splitlines() if line.startswith("D:") or "FAILED" in line][:6],
    }


def summarize(label, records):
    passes = sum(1 for record in records if record["passed"])
    return {
        "regime": label,
        "runs": len(records),
        "passed": passes,
        "failed": len(records) - passes,
        "elapsed_seconds_min": min(record["elapsed_seconds"] for record in records),
        "elapsed_seconds_max": max(record["elapsed_seconds"] for record in records),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", required=True)
    parser.add_argument("--sequential", type=int, default=20)
    parser.add_argument("--parallel", type=int, default=8)
    parser.add_argument("--rounds", type=int, default=3)
    parser.add_argument("--stress-exe", default=None,
                        help="second test executable run concurrently to load the machine")
    parser.add_argument("--stress-count", type=int, default=4)
    parser.add_argument("--out", default=os.path.dirname(os.path.abspath(__file__)))
    args = parser.parse_args()

    exe = os.path.abspath(args.exe)
    env = environment(exe)
    digest = sha256_file(exe)
    print("exe  ", exe)
    print("sha256", digest)

    sequential = [run_once(exe, env) for _ in range(args.sequential)]
    print("sequential: {}/{} passed".format(sum(1 for r in sequential if r["passed"]), len(sequential)))

    parallel_rounds = []
    for index in range(args.rounds):
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.parallel) as pool:
            records = list(pool.map(lambda _ignored: run_once(exe, env), range(args.parallel)))
        passed = sum(1 for record in records if record["passed"])
        print("parallel round {}: {}/{} passed".format(index + 1, passed, len(records)))
        parallel_rounds.append({"round": index + 1, "records": records})

    stress = None
    if args.stress_exe:
        stress_exe = os.path.abspath(args.stress_exe)
        background = [
            subprocess.Popen([stress_exe], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                             env=environment(stress_exe), cwd=os.path.dirname(stress_exe))
            for _ in range(args.stress_count)
        ]
        try:
            records = [run_once(exe, env) for _ in range(args.parallel)]
        finally:
            for process in background:
                process.terminate()
            for process in background:
                try:
                    process.wait(timeout=30)
                except subprocess.TimeoutExpired:
                    process.kill()
        passed = sum(1 for record in records if record["passed"])
        print("under background load: {}/{} passed".format(passed, len(records)))
        stress = {
            "background_exe": stress_exe,
            "background_exe_sha256": sha256_file(stress_exe),
            "background_processes": args.stress_count,
            "records": records,
            "summary": summarize("loaded", records),
        }

    payload = {
        "scope": (
            "Diagnosis of the single intermittent CTest failure recorded by the 2026-11-01 "
            "same-source matrix (row msvc-on-release). Not a matrix re-run, not a re-sample of a "
            "registered group, and not evidence that the test is sound."
        ),
        "case": CASE,
        "exe": exe,
        "exe_sha256": digest,
        "exe_bytes": os.path.getsize(exe),
        "exe_mtime_local": time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(os.path.getmtime(exe))),
        "logical_processors": os.cpu_count(),
        "sequential": {"summary": summarize("sequential-idle", sequential), "records": sequential},
        "parallel": {
            "workers_per_round": args.parallel,
            "rounds": parallel_rounds,
            "summary": [summarize("parallel-{}".format(entry["round"]), entry["records"])
                        for entry in parallel_rounds],
        },
        "stress": stress,
        "verdict_rule": (
            "Deterministic failure means every run fails; intermittent failure means at least one "
            "run passes and at least one fails. A pass under a lighter regime than CTest -j 4 does "
            "not clear the test; it only shows the outcome depends on timing or load."
        ),
    }
    os.makedirs(args.out, exist_ok=True)
    target = os.path.join(args.out, "diagnosis.json")
    with open(target, "w", encoding="utf-8", newline="\n") as stream:
        json.dump(payload, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    print("wrote", target)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

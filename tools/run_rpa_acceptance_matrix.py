"""Run fresh, unfiltered local RPA acceptance rows and preserve every attempt.

This runner never promotes test-only parameters or declares F1/F3 acceptance.
Use from a VS developer shell for MSVC, or pass --vcvars on Windows.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import subprocess
import time
import uuid

ROOT = Path(__file__).resolve().parents[1]
ROWS = {
    "msvc-on-debug": ("candidate-debug", "candidate-debug", []),
    "msvc-on-release": ("rpa-candidate-release", "release", ["-DCUEXIS_ENABLE_CHART_V5_CANDIDATE=ON"]),
    "msvc-on-shared-debug": ("rpa-candidate-shared-debug", "candidate-shared-debug", []),
    "msvc-off-debug": ("debug", "debug", []),
    "msvc-off-headless-debug": ("b3-default-off", "headless-debug", []),
    "msvc-off-release": ("release", "release", []),
    "msvc-off-headless-release": ("headless-release", "headless-release", []),
    "msvc-off-shared-debug": ("headless-shared-debug", "headless-shared-debug", []),
    "mingw-on-debug": ("mingw-headless-debug", "mingw-headless-debug", ["-DCUEXIS_ENABLE_CHART_V5_CANDIDATE=ON"]),
    "mingw-on-shared-debug": ("rpa-boundary-mingw-shared", "mingw-headless-debug", ["-DCUEXIS_ENABLE_CHART_V5_CANDIDATE=ON", "-DCUEXIS_LIBRARY_TYPE=SHARED"]),
    "mingw-on-release": ("rpa-mingw-release", "mingw-headless-debug", ["-DCUEXIS_ENABLE_CHART_V5_CANDIDATE=ON", "-DCMAKE_BUILD_TYPE=Release", "-DCUEXIS_VERSION_SUFFIX=", "-DCUEXIS_WARNINGS_AS_ERRORS=ON"]),
}


def identity():
    tracked = subprocess.check_output(["git", "ls-files", "-z"], cwd=ROOT).split(b"\0")
    paths = {p.decode("utf-8") for p in tracked if p}
    # Include new implementation files, but never unrelated caches or out evidence.
    untracked = subprocess.check_output(["git", "ls-files", "--others", "--exclude-standard", "-z"], cwd=ROOT).split(b"\0")
    paths.update(p.decode("utf-8") for p in untracked if p and
                 p.decode("utf-8").startswith(("app/", "tests/", "tools/", "engine/", "cmake/", "schemas/", "docs/"))
                 and Path(p.decode("utf-8")).suffix != ".pyc")
    files = {p: hashlib.sha256((ROOT / p).read_bytes()).hexdigest()
             for p in sorted(paths) if (ROOT / p).is_file()}
    implementation = {p: sha for p, sha in files.items()
                      if not p.startswith("docs/") and not p.endswith(".md")}
    return {"head": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT).decode().strip(),
            "worktree_sha256": hashlib.sha256(json.dumps(files, sort_keys=True).encode()).hexdigest(),
            "implementation_sha256": hashlib.sha256(json.dumps(implementation, sort_keys=True).encode()).hexdigest(),
            "files": files}


def save(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rows", nargs="+", choices=ROWS, default=list(ROWS))
    parser.add_argument("--output", type=Path, default=ROOT / "out/rpa-acceptance")
    parser.add_argument("--vcvars", type=Path)
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--repeat-measurements", type=int, default=5)
    args = parser.parse_args()
    if args.jobs < 1 or args.repeat_measurements < 1:
        parser.error("jobs and repeat-measurements must be positive")
    if args.vcvars and not args.vcvars.is_file():
        parser.error("vcvars must name an existing developer shell script")
    run = args.output.resolve() / (time.strftime("%Y%m%d-%H%M%S") + "-" + uuid.uuid4().hex[:8])
    run.mkdir(parents=True)
    manifest = {"source": identity(), "os": platform.platform(), "machine": platform.machine(),
                "logical_processors": os.cpu_count(), "test_only": True,
                "final_acceptance": False, "production_thresholds_accepted": False,
                "rows": [], "unexecuted": ["physical_keyboard", "listening", "device_loss_recovery",
                                           "production_calibration", "hosted_same_sha", "owner_acceptance"]}
    save(run / "manifest.json", manifest)
    print(f"Evidence: {run}", flush=True)

    def execute(row, phase, command, environment):
        log = run / f"{row}-{phase}.log"
        invocation = command
        if row.startswith("msvc") and args.vcvars:
            # Paths/arguments come from this runner; quote with Windows CRT rules.
            batch = run / f"{row}-{phase}.cmd"
            batch.write_text('@echo off\ncall "' + str(args.vcvars.resolve()) +
                             '" >nul\nif errorlevel 1 exit /b %errorlevel%\n' +
                             subprocess.list2cmdline(command) + '\nexit /b %errorlevel%\n',
                             encoding="utf-8")
            invocation = ["cmd.exe", "/d", "/c", str(batch)]
        start = time.monotonic()
        with log.open("wb") as stream:
            stream.write((json.dumps(command) + "\n").encode())
            stream.flush()
            result = subprocess.run(invocation, cwd=ROOT, env=environment, stdout=stream,
                                    stderr=subprocess.STDOUT)
        record = {"phase": phase, "command": command, "exit_code": result.returncode,
                  "elapsed_seconds": time.monotonic() - start, "log": log.name,
                  "log_sha256": hashlib.sha256(log.read_bytes()).hexdigest()}
        print(f"{row} {phase}: exit={result.returncode}", flush=True)
        return record

    for row in args.rows:
        directory, preset, options = ROWS[row]
        build = ROOT / "out/build" / directory
        environment = os.environ.copy()
        if row.startswith("mingw"):
            environment["PATH"] = "C:/msys64/ucrt64/bin;" + environment["PATH"]
        record = {"name": row, "build_directory": str(build), "phases": [], "measurements": [],
                  "source_before": identity()}
        manifest["rows"].append(record)
        commands = [
            ("configure", ["cmake", "--preset", preset, "--fresh", "-B", str(build), *options]),
            ("build", ["cmake", "--build", str(build), "--clean-first", "--parallel", str(args.jobs)]),
            ("ctest", ["ctest", "--test-dir", str(build), "--no-tests=error", "--output-on-failure", "-j", str(args.jobs)]),
        ]
        for phase, command in commands:
            result = execute(row, phase, command, environment)
            record["phases"].append(result)
            if phase == "ctest":
                text = (run / result["log"]).read_text(encoding="utf-8", errors="replace")
                counts = re.search(r"(\d+)% tests passed, (\d+) tests failed out of (\d+)", text)
                record["test_counts"] = list(map(int, counts.groups())) if counts else None
                record["skipped_tests"] = [line.strip() for line in text.splitlines()
                                           if "***Skipped" in line]
                temporary = build / "Testing/Temporary"
                for name in ("LastTest.log", "LastTestsFailed.log"):
                    artifact = temporary / name
                    if artifact.exists():
                        (run / f"{row}-{name}").write_bytes(artifact.read_bytes())
            save(run / "manifest.json", manifest)
            if result["exit_code"]:
                record["status"] = f"failed_{phase}"
                break
        else:
            record["status"] = "local_pass"
            if not record["test_counts"]:
                record["status"] = "test_summary_missing"
            if "-DCUEXIS_ENABLE_CHART_V5_CANDIDATE=ON" in options or "candidate" in preset:
                for executable in ("cuexis_execution_host_tests", "cuexis_rpa_tests"):
                    probe = build / "bin" / (executable + ".exe")
                    for index in range(args.repeat_measurements):
                        phase = f"{executable}-budget-{index + 1}"
                        measurement = execute(row, phase, [str(probe), "[measurement]"], environment)
                        record["phases"].append(measurement)
                        for line in (run / measurement["log"]).read_text(encoding="utf-8", errors="replace").splitlines():
                            if line.startswith('{"event":'):
                                record["measurements"].append({"attempt": index + 1, **json.loads(line)})
                        if measurement["exit_code"]:
                            record["status"] = "failed_measurement"
                        save(run / "manifest.json", manifest)
        cache = build / "CMakeCache.txt"
        if cache.exists():
            record["cache_sha256"] = hashlib.sha256(cache.read_bytes()).hexdigest()
            (run / f"{row}-CMakeCache.txt").write_bytes(cache.read_bytes())
        record["binary_sha256"] = {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                                   for p in (build / "bin").glob("*.exe")}
        record["source_after"] = identity()
        record["source_unchanged"] = record["source_before"] == record["source_after"]
        save(run / "manifest.json", manifest)
    manifest["source_after"] = identity()
    manifest["source_unchanged"] = manifest["source"] == manifest["source_after"]
    save(run / "manifest.json", manifest)
    return 0 if manifest["source_unchanged"] and all(r["status"] == "local_pass" for r in manifest["rows"]) else 1


if __name__ == "__main__":
    raise SystemExit(main())

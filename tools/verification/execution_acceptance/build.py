"""Build an explicit clean source in a fresh tree; preserve configure/build and input identities."""
import argparse
import ctypes
import os
from pathlib import Path
import subprocess
import time

from .binding import clean_source, sha256, write_once
from .build_binding import INSTRUMENTATION_OPTIONS, bind


def run(args):
    source, build, output = args.source.resolve(), args.build.resolve(), args.output.resolve()
    identity = clean_source(source)
    if build.exists() or output.exists():
        raise ValueError("fresh build and evidence directories required")
    output.mkdir(parents=True)
    write_once(output / "source.json", identity)
    setup = output / "developer-environment.cmd"
    setup.write_text('@echo off\ncall "' + str(args.vcvars) + '" >nul\n'
                     'if errorlevel 1 exit /b %errorlevel%\nset\n', encoding="utf-8")
    environment = os.environ.copy()
    for line in subprocess.check_output(["cmd.exe", "/d", "/c", str(setup)]).decode("utf-8", errors="replace").splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            if key:
                environment[key] = value
    # ASCII /showIncludes prefix avoids locale/codepage-dependent Ninja header loss.
    environment.update(VSLANG="1033", VCPKG_ROOT=str(args.vcpkg.resolve()))
    options = {key: "OFF" for key in INSTRUMENTATION_OPTIONS}
    if args.mode == "memory":
        options.update(CUEXIS_ALLOCATION_LIFETIME_OBSERVER="ON", CUEXIS_EXECUTION_ALLOCATION_OBSERVER="ON")
    commands = [
        ("configure", ["cmake", "--preset", args.config.lower(), "-B", str(build), "--fresh",
                       "-DCUEXIS_LIBRARY_TYPE=STATIC", "-DCUEXIS_ENABLE_CHART_V5_CANDIDATE=ON",
                       "-DCUEXIS_WARNINGS_AS_ERRORS=ON", *[f"-D{key}={value}" for key, value in options.items()]]),
        ("build", ["cmake", "--build", str(build), "--clean-first", "--parallel", str(args.jobs),
                   "--target", *args.targets])]
    kernel = ctypes.windll.kernel32
    original_cp = kernel.GetConsoleCP(), kernel.GetConsoleOutputCP()
    try:
        kernel.SetConsoleCP(65001)
        kernel.SetConsoleOutputCP(65001)
        for phase, command in commands:
            log = output / (phase + ".log")
            started = time.monotonic()
            with log.open("xb") as stream:
                result = subprocess.run(command, cwd=source, env=environment, stdout=stream,
                                        stderr=subprocess.STDOUT)
            write_once(output / (phase + ".json"), {"command": command, "exit_code": result.returncode,
                       "elapsed_seconds": time.monotonic() - started, "log_sha256": sha256(log),
                       "VSLANG": environment["VSLANG"], "console_codepage": 65001})
            print(phase, result.returncode, flush=True)
            result.check_returncode()
        for target in args.targets:
            metadata = bind(build, target + ".exe", source, output / "build.log")
            write_once(output / (target + "-build-binding.json"), metadata)
        if clean_source(source) != identity:
            raise ValueError("source changed while building")
    finally:
        kernel.SetConsoleCP(original_cp[0])
        kernel.SetConsoleOutputCP(original_cp[1])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for field in ("source", "build", "output"):
        parser.add_argument("--" + field, type=Path, required=True)
    parser.add_argument("--config", choices=("Release", "Debug"), required=True)
    parser.add_argument("--mode", choices=("normal", "memory"), default="normal")
    parser.add_argument("--targets", nargs="+", required=True)
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--vcvars", type=Path, default=Path("D:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat"))
    parser.add_argument("--vcpkg", type=Path, default=Path("D:/vcpkg"))
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("positive build concurrency required")
    run(args)


if __name__ == "__main__":
    main()

"""Run the throughput probe once and walk the stack of every thread stopped in a kernel stub.

This is a diagnostic companion to `run_sample.py`.  It spawns the *same* probe with the
*same* environment, waits for the barrier, sends `go`, and then uses `walk_once.py` to
unwind the full call chain of each thread found inside an ntdll syscall stub.  No sampling,
no suspend loop: a handful of walks, so the probe's own timings are not used for anything.

Usage:
    python -B walk_probe.py --want 20 --ssn 4 [--entities 2800] [--mode static]
"""

import argparse
import datetime as dt
import json
import os
import subprocess
import sys
from pathlib import Path

import walk_once

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import run_sample  # noqa: E402  (needs HERE on sys.path)

PROBE = Path(r"D:\Cuexis-redesign\out\build\rpa-candidate-release\bin"
             r"\cuexis_execution_device_probe.exe")


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--attempts", type=int, default=40)
    parser.add_argument("--want", type=int, default=10)
    parser.add_argument("--gap-ms", type=float, default=50.0)
    parser.add_argument("--entities", type=int, default=2800)
    parser.add_argument("--mode", default="static")
    parser.add_argument("--seconds", type=int, default=20)
    parser.add_argument("--warmup", type=int, default=8)
    parser.add_argument("--warmup-ms", type=int, default=2000)
    parser.add_argument("--width", type=int, default=1920)
    parser.add_argument("--height", type=int, default=1080)
    parser.add_argument("--mask", type=int, default=65535)
    parser.add_argument("--max-depth", type=int, default=24)
    parser.add_argument("--readback", type=int, default=1, choices=(0, 1))
    parser.add_argument("--out", default=str(HERE / "walk-probe"))
    args = parser.parse_args(argv)

    out = Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    log = out / "probe.log.txt"
    env = {key: value for key, value in os.environ.items()
           if not key.startswith(("CUEXIS_PD", "CUEXIS_GPU_"))}
    env.update(
        CUEXIS_GPU_BARRIER="1",
        CUEXIS_GPU_WIDTH=str(args.width),
        CUEXIS_GPU_HEIGHT=str(args.height),
        CUEXIS_GPU_ENTITIES=str(args.entities),
        CUEXIS_GPU_MODE=args.mode,
        CUEXIS_GPU_FRAMES="120",
        CUEXIS_GPU_WARMUP=str(args.warmup),
        CUEXIS_GPU_WARMUP_MS=str(args.warmup_ms),
        CUEXIS_GPU_TIMER="0",
        CUEXIS_GPU_PIXEL_READBACK=str(args.readback),
        CUEXIS_GPU_SECONDS=str(args.seconds),
    )
    started = dt.datetime.now(dt.timezone.utc).isoformat()
    with open(log, "x", encoding="utf-8") as stream:
        child = subprocess.Popen(
            [str(PROBE), "GPU visible entity throughput probe", "--rng-seed", "1"],
            cwd=str(PROBE.parent), env=env, stdin=subprocess.PIPE,
            stdout=stream, stderr=subprocess.STDOUT,
            creationflags=subprocess.CREATE_NO_WINDOW | subprocess.BELOW_NORMAL_PRIORITY_CLASS,
        )
        try:
            if not run_sample.wait_for_marker(log, b"GPU_THROUGHPUT_READY", 120.0):
                raise SystemExit("probe never reached the barrier")
            controls = run_sample.runner.configure_child(child, args.mask)
            child.stdin.write(b"go\n")
            child.stdin.flush()
            child.stdin.close()
            walk_argv = ["--pid", str(child.pid), "--attempts", str(args.attempts),
                         "--want", str(args.want), "--max-depth", str(args.max_depth),
                         "--gap-ms", str(args.gap_ms),
                         "--out", str(out / "walk.txt")]
            code = walk_once.main(walk_argv)
            exit_code = child.wait(timeout=300)
        finally:
            if child.poll() is None:
                child.kill()
                child.wait()
    text = log.read_text(encoding="utf-8", errors="replace")
    verdict = [line for line in text.splitlines()
               if line.startswith("GPU_THROUGHPUT_RESULT") or line.startswith("GPU_THROUGHPUT_ID")]
    print("walk exit", code, "probe exit", exit_code, "controls", controls)
    for line in verdict:
        print(line[:400])
    (out / "walk.json").write_text(json.dumps({
        "started_utc": started,
        "probe": str(PROBE),
        "attempts": args.attempts,
        "want": args.want,
        "entities": args.entities,
        "mode": args.mode,
        "seconds": args.seconds,
        "readback": args.readback,
        "probe_exit": exit_code,
    }, indent=2) + "\n", encoding="utf-8", newline="\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())

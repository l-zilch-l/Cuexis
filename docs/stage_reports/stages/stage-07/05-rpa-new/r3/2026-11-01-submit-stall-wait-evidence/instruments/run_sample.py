"""Run one measured probe process while sampling its instruction pointer.

This is the driver for the instruction pointer diagnostic registered in `PLAN-RIP.md`. It is not a
capacity runner: it re-uses the frozen ladder runner's child controls, environment and validity
rules so that the probe's own slow-frame rate can be used as the control that the phenomenon
persisted, but its timings are not comparable with any other instrument.

Protocol:
  1. start the probe with `CUEXIS_GPU_BARRIER=1`, so it stops at the barrier after the GPU context
     is up and all modules are loaded;
  2. wait for `GPU_THROUGHPUT_READY` in the log, then apply affinity / priority / execution QoS;
  3. start the sampler, restricted to one logical CPU and BelowNormal, and wait for its ready file;
  4. release the barrier, wait for the probe, then collect the sampler;
  5. score the probe's own frames with the frozen ladder rules, and analyse the sample stream.

Usage:
    python -B run_sample.py --exe <probe.exe> --out <dir> --index 1 \
        --entities 2800 --mode static --seconds 20
"""

import argparse
import ctypes as ct
import datetime as dt
import importlib.util
import json
import os
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
RUNNER_PATH = Path(r"D:\Cuexis-redesign\out\verification-20261031"
                    r"\r3-40k-throughput-refresh-v1\run_stall_diagnosis.py")


def load_runner():
    spec = importlib.util.spec_from_file_location("frozen_runner", RUNNER_PATH)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


runner = load_runner()
kernel = runner.kernel
BUDGET_US = runner.BUDGET_US
REPO_ROOT = runner.REPO_ROOT
sha256 = runner.sha256


def wait_for_marker(path, marker, timeout):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if path.exists():
            with open(path, "rb") as stream:
                if marker in stream.read():
                    return True
        time.sleep(0.05)
    return False


def restrict_affinity(pid, mask):
    handle = kernel.OpenProcess(0x0200 | 0x1000, False, pid)
    if not handle:
        raise RuntimeError("OpenProcess failed for the sampler")
    try:
        if not kernel.SetProcessAffinityMask(handle, mask):
            raise RuntimeError("SetProcessAffinityMask failed for the sampler")
        observed = ct.c_size_t()
        system = ct.c_size_t()
        if not kernel.GetProcessAffinityMask(handle, ct.byref(observed), ct.byref(system)):
            raise RuntimeError("GetProcessAffinityMask failed for the sampler")
        return observed.value
    finally:
        kernel.CloseHandle(handle)


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--index", type=int, default=1)
    parser.add_argument("--entities", type=int, default=2800)
    parser.add_argument("--mode", default="static", choices=("render-only", "static"))
    parser.add_argument("--seconds", type=float, default=20.0)
    parser.add_argument("--frames", type=int, default=120)
    parser.add_argument("--warmup", type=int, default=8)
    parser.add_argument("--warmup-ms", type=int, default=2000)
    parser.add_argument("--width", type=int, default=1920)
    parser.add_argument("--height", type=int, default=1080)
    parser.add_argument("--mask", type=int, default=65535)
    parser.add_argument("--sampler-mask", type=int, default=32768)
    parser.add_argument("--sample-ms", type=int, default=24000)
    parser.add_argument("--ready-timeout", type=float, default=120.0)
    parser.add_argument("--timeout", type=float, default=300.0)
    parser.add_argument("--processes", type=int, default=1)
    parser.add_argument("--sampler", choices=("rip", "wait"), default="rip")
    parser.add_argument("--stack-bytes", type=int, default=0,
                        help="scan this many stack bytes for callers (wait sampler only)")
    parser.add_argument("--handles", action="store_true",
                        help="identify every wait handle the sampler sees (wait sampler only)")
    args = parser.parse_args(argv)
    if args.handles and args.sampler != "wait":
        parser.error("--handles needs --sampler wait")
    if args.stack_bytes and args.sampler != "wait":
        parser.error("--stack-bytes needs --sampler wait")
    sampler_name = "sample_wait.py" if args.sampler == "wait" else "sample_rip.py"

    exe = Path(args.exe).resolve()
    out = Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    stem = "{:02d}-{}-{}".format(args.index, args.mode, args.entities)
    log = out / "{}.log.txt".format(stem)
    samples = out / "{}.samples.txt".format(stem)
    modules = out / "{}.modules.txt".format(stem)
    ready = out / "{}.ready".format(stem)
    sampler_log = out / "{}.sampler.txt".format(stem)
    handle_table = out / "{}.handles.txt".format(stem)
    for path in (log, samples, modules, ready, sampler_log, handle_table):
        if path.exists():
            path.unlink()

    registration_path = out / "registration.json"
    plan = {
        "groups": [args.mode],
        "processes_per_group": args.processes,
        "entities": args.entities,
        "seconds_per_process": args.seconds,
        "frames_per_process": args.frames,
        "viewport": [args.width, args.height],
        "affinity": args.mask,
        "sampler_affinity": args.sampler_mask,
        "priority": "BelowNormal",
        "qos": "execution throttling OFF",
        "timer": False,
        "pixel_readback": True,
        "budget_us": BUDGET_US,
        "pass_rule": "wall FPS >= 240 and cycle p99 <= 4.166667 ms and every complete one-second "
                     "window >= 240 frames",
        "stop": "an invalid process stops the run; budget failures are retained",
        "instrument": "{} instruction pointer sampling, roughly 13 kHz per thread".format(
            sampler_name),
        "sampler": sampler_name,
        "identify_handles": bool(args.handles),
        "identify_stack_bytes": int(args.stack_bytes),
    }
    if registration_path.exists():
        registration = json.loads(registration_path.read_text(encoding="utf-8"))
        for key, value in plan.items():
            if registration["plan"][key] != value:
                raise SystemExit("plan mismatch on {}".format(key))
    else:
        registration = {
            "scope": "instruction pointer sampling of the submit-path stall at 1080p; separate "
                     "instrument, diagnostic only, not the pre-registered ladder and not acceptance",
            "created_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
            "head": subprocess.run(["git", "rev-parse", "HEAD"], cwd=str(REPO_ROOT),
                                   capture_output=True, text=True).stdout.strip(),
            "dirty_files": subprocess.run(["git", "status", "--porcelain"], cwd=str(REPO_ROOT),
                                          capture_output=True, text=True).stdout.split(),
            "plan": plan,
            "binary": str(exe),
            "binary_sha256": sha256(exe),
            "probe_source_sha256": sha256(REPO_ROOT / "tests/player/gpu_throughput_probe.cpp"),
            "sampler": str(HERE / sampler_name),
            "sampler_sha256": sha256(HERE / sampler_name),
            "analyzer": str(HERE / ("analyze_wait.py" if args.sampler == "wait"
                                    else "analyze_rip.py")),
            "analyzer_sha256": sha256(HERE / ("analyze_wait.py" if args.sampler == "wait"
                                              else "analyze_rip.py")),
            "driver": str(Path(__file__).resolve()),
            "driver_sha256": sha256(Path(__file__).resolve()),
            "runner_sha256": sha256(RUNNER_PATH),
        }
        registration_path.write_text(json.dumps(registration, indent=2) + "\n",
                                     encoding="utf-8", newline="\n")

    assert sha256(exe) == registration["binary_sha256"], "binary changed"
    assert sha256(REPO_ROOT / "tests/player/gpu_throughput_probe.cpp") == \
        registration["probe_source_sha256"], "probe source changed"
    assert sha256(HERE / sampler_name) == registration["sampler_sha256"], "sampler changed"

    env = {key: value for key, value in os.environ.items()
           if not key.startswith(("CUEXIS_PD", "CUEXIS_GPU_"))}
    env.update(
        CUEXIS_GPU_BARRIER="1",
        CUEXIS_GPU_WIDTH=str(args.width),
        CUEXIS_GPU_HEIGHT=str(args.height),
        CUEXIS_GPU_ENTITIES=str(args.entities),
        CUEXIS_GPU_MODE=args.mode,
        CUEXIS_GPU_FRAMES=str(args.frames),
        CUEXIS_GPU_WARMUP=str(args.warmup),
        CUEXIS_GPU_WARMUP_MS=str(args.warmup_ms),
        CUEXIS_GPU_TIMER="0",
        CUEXIS_GPU_PIXEL_READBACK="1",
    )
    if args.seconds:
        env["CUEXIS_GPU_SECONDS"] = str(int(args.seconds))

    started = dt.datetime.now(dt.timezone.utc).isoformat()
    controls = None
    sampler_affinity = None
    sampler_note = ""
    with open(log, "x", encoding="utf-8") as stream:
        child = subprocess.Popen(
            [str(exe), "GPU visible entity throughput probe", "--rng-seed", "1"],
            cwd=str(REPO_ROOT),
            env=env,
            stdin=subprocess.PIPE,
            stdout=stream,
            stderr=subprocess.STDOUT,
            creationflags=subprocess.CREATE_NO_WINDOW | subprocess.BELOW_NORMAL_PRIORITY_CLASS,
        )
        sampler = None
        try:
            if not wait_for_marker(log, b"GPU_THROUGHPUT_READY", args.ready_timeout):
                raise SystemExit("probe never reached the barrier")
            controls = runner.configure_child(child, args.mask)
            sampler_command = [sys.executable, "-B", str(HERE / sampler_name),
                               "--pid", str(child.pid),
                               "--out", str(samples),
                               "--modules", str(modules),
                               "--ready", str(ready),
                               "--ms", str(args.sample_ms)]
            if args.handles:
                sampler_command += ["--handles-out", str(handle_table)]
            if args.stack_bytes:
                sampler_command += ["--stack-bytes", str(args.stack_bytes)]
            sampler = subprocess.Popen(
                sampler_command,
                cwd=str(HERE),
                stdout=open(sampler_log, "w", encoding="utf-8"),
                stderr=subprocess.STDOUT,
                creationflags=subprocess.CREATE_NO_WINDOW | subprocess.BELOW_NORMAL_PRIORITY_CLASS,
            )
            sampler_affinity = restrict_affinity(sampler.pid, args.sampler_mask)
            deadline = time.monotonic() + args.ready_timeout
            while time.monotonic() < deadline and not ready.exists():
                if sampler.poll() is not None:
                    raise SystemExit("sampler exited before sampling")
                time.sleep(0.05)
            if not ready.exists():
                raise SystemExit("sampler never signalled ready")
            child.stdin.write(b"go\n")
            child.stdin.flush()
            child.stdin.close()
            try:
                code = child.wait(timeout=args.timeout)
            except subprocess.TimeoutExpired:
                child.kill()
                child.wait()
                code = "timeout"
        finally:
            if child.poll() is None:
                child.kill()
                child.wait()
            if sampler is not None:
                try:
                    sampler.wait(timeout=60.0)
                except subprocess.TimeoutExpired:
                    sampler.kill()
                    sampler.wait()
                    sampler_note = "sampler killed"

    text = log.read_text(encoding="utf-8", errors="replace")
    identity_lines = [line for line in text.splitlines()
                      if line.startswith("GPU_THROUGHPUT_ID ")]
    records = [json.loads(line.split(" ", 1)[1]) for line in text.splitlines()
               if line.startswith("GPU_THROUGHPUT_SAMPLE ")]
    identity = json.loads(identity_lines[0].split(" ", 1)[1]) if len(identity_lines) == 1 else None
    problems = []
    if code != 0:
        problems.append("exit {}".format(code))
    if "All tests passed" not in text:
        problems.append("test not passed")
    if identity is None:
        problems.append("identity lines {}".format(len(identity_lines)))
    if not records:
        problems.append("no samples")
    if identity is not None:
        expected = {
            "mode": args.mode,
            "entities": args.entities,
            "draws_per_frame": args.entities,
            "in_view_centers": args.entities,
            "fixture_revision": 3,
            "timer": False,
            "moved_entities": 0,
            "width": args.width,
            "height": args.height,
            "vsync": 0,
            "warmup_ms": args.warmup_ms,
            "pixel_readback": True,
            "outside_timing_pixel_proof_frames": 2,
            "duration_seconds": args.seconds,
        }
        for key, value in expected.items():
            if identity[key] != value:
                problems.append("{}={} expected {}".format(key, identity[key], value))
        if identity["pixel_rgba"] != [255, 204, 51, 255]:
            problems.append("pixel {}".format(identity["pixel_rgba"]))
        if "NVIDIA" not in identity["vendor"]:
            problems.append("vendor {}".format(identity["vendor"]))
        if identity["measured_wall_us"] < args.seconds * 1000000.0:
            problems.append("measured wall below the duration")
        if len(records) != identity["frames"]:
            problems.append("samples {} != identity frames {}".format(
                len(records), identity["frames"]))
        if [item["ordinal"] for item in records] != list(range(len(records))):
            problems.append("ordinal discontinuity")
        if any("gpu_interval_us" in item for item in records):
            problems.append("gpu_interval_us present although the timer is off")
        for item in records:
            parts = sum(item[key] for key in
                        ("update_us", "extract_us", "submit_us", "present_us"))
            if abs(parts - item["total_us"]) >= 0.001:
                problems.append("total mismatch at {}".format(item["ordinal"]))
                break
        if abs(sum(item["cycle_us"] for item in records)
               - identity["measured_wall_us"]) >= 0.01:
            problems.append("cycle sum != measured_wall_us")

    summary = {}
    wall_fps = None
    windows = []
    if records:
        summary = {metric: runner.stats([item[metric] for item in records])
                   for metric in records[0] if metric not in ("ordinal", "end_us")}
    if identity and records:
        wall_fps = len(records) * 1000000.0 / identity["measured_wall_us"]
        windows = runner.second_windows(records, int(args.seconds))
    slow = [item for item in records if item["cycle_us"] > 6000.0]
    slow_submit = [item["submit_us"] for item in slow]
    fast_submit = [item["submit_us"] for item in records if item["cycle_us"] <= 6000.0]
    sampler_text = sampler_log.read_text(encoding="utf-8", errors="replace") \
        if sampler_log.exists() else ""
    result = {
        "mode": args.mode,
        "index": args.index,
        "entities": args.entities,
        "seconds": args.seconds,
        "log": log.name,
        "log_sha256": sha256(log),
        "samples": samples.name,
        "samples_sha256": sha256(samples) if samples.exists() else None,
        "modules": modules.name,
        "modules_sha256": sha256(modules) if modules.exists() else None,
        "sampler_log": sampler_log.name,
        "sampler_sha256": sha256(sampler_log) if sampler_log.exists() else None,
        "handles": handle_table.name,
        "handles_sha256": sha256(handle_table) if handle_table.exists() else None,
        "sampler_output": sampler_text.strip().splitlines()[-8:],
        "sampler_affinity": sampler_affinity,
        "sampler_note": sampler_note,
        "started_utc": started,
        "exit": code,
        "problems": problems,
        "controls": controls,
        "identity": identity,
        "frames": len(records),
        "wall_fps": wall_fps,
        "second_windows": windows,
        "min_window": min(windows) if windows else None,
        "summary": summary,
        "slow_frames": {
            "threshold_us": 6000.0,
            "count": len(slow),
            "share": round(len(slow) / len(records), 6) if records else None,
            "submit_us": min(slow_submit) if slow_submit else None,
            "fast_submit_us": min(fast_submit) if fast_submit else None,
        },
    }
    verdict = {
        "budget_pass": bool(
            not problems and wall_fps is not None and wall_fps >= 240.0
            and summary["cycle_us"]["p99_us"] <= BUDGET_US
            and (not windows or min(windows) >= 240)),
        "control_ok": bool(not records or (len(slow) / len(records)) >= 0.01),
    }
    result.update(verdict)
    (out / "results-{}.json".format(stem)).write_text(
        json.dumps(result, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps({"index": args.index, "exit": code, "problems": problems,
                      "frames": len(records), "wall_fps": wall_fps,
                      "p99_us": summary.get("cycle_us", {}).get("p99_us"),
                      "slow_share": result["slow_frames"]["share"],
                      "control_ok": verdict["control_ok"],
                      "budget_pass": verdict["budget_pass"],
                      "sampler": result["sampler_output"]}, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())

"""Timer-enabled diagnosis of the submit-path stall the frontier ladder exposed.

This file is `run_40k_refresh.py` (`16a3f57a100613f85b823eb97300d9227fe8466a3692777cd82d7e2ab2a2648b`)
with exactly four differences, and nothing else:

  1. `CUEXIS_GPU_TIMER="1"`, so every sample carries `gpu_interval_us` (the probe's own GPU
     timestamps) next to the CPU-side phase timings;
  2. the identity check expects `timer: true`;
  3. a sample without `gpu_interval_us` is a problem, so a silent fallback to timerless samples
     cannot pass as a diagnosis;
  4. the registration records `timer: true` and its own scope string.

It is a **separate instrument**, not part of the pre-registered ladder: its runner hash differs, GPU
timer queries perturb the measurement, and its outputs may not be merged into the ladder's records.
The ladder keeps the timer off because one timer query per frame is an extra synchronisation point.
Everything else stays identical on purpose: three fresh processes per group, BelowNormal, execution
throttling off, the default 16-logical-CPU set, a clean worktree, and hashes re-checked before
every process.

Re-runs the frozen 2026-10-29 `coarse` 40000 conditions (40000 visible entities, 1920x1080,
120 measured frames, 8-frame and 2000 ms warmup, GPU timestamps off, synchronous pixel
readback, default 16 logical CPU set, BelowNormal, execution throttling off) so the 40K@240
verdict can cite a measurement of the committed source instead of only the frozen scan.

The validity rules are the frozen runner's rules, not relaxed versions of them: one identity
line per process, exactly `--frames` samples, ordinal continuity, `draws_per_frame`,
`in_view_centers`, viewport, VSync, warmup, pixel `[255,204,51,255]`, NVIDIA vendor,
`update+extract+submit+present == total` per frame, `sum(cycle) == measured_wall_us`,
`moved_entities == 0`, and the binary/source hashes unchanged before every process.

Pre-registered plan:
  groups   : render-only (submit/present only), static (update/extract/submit/present)
  samples  : three fresh processes per group, 120 measured frames each
  controls : default 16 logical CPU set, BelowNormal, execution throttling OFF, verified
  stop rule: an invalid process stops the whole run; budget failures are retained
  budget   : 1000000/240 us per complete cycle (p99) plus wall FPS >= 240

`--seconds N` switches a group from frame-counted to duration runs for the sustained capacity
points: the probe then measures until the last frame ends past N seconds, and the budget rule
additionally requires every complete one-second window to contain at least 240 finished frames.
Frame-counted runs keep the frozen 120-frame rule.

Usage:
    python -B run_40k_refresh.py --exe <probe.exe> --out <dir> [--entities 40000]
    python -B run_40k_refresh.py --exe <probe.exe> --out <dir> --entities 1800 --seconds 20
"""

import argparse
import ctypes as ct
import datetime as dt
import hashlib
import json
import math
import os
import statistics
import subprocess
import sys
from ctypes import wintypes as w
from pathlib import Path

BUDGET_US = 1000000.0 / 240.0
MODES = ("render-only", "static")
ENTITY_LIMIT = 40000


def repository_root():
    for candidate in Path(__file__).resolve().parents:
        if (candidate / "CMakePresets.json").is_file():
            return candidate
    raise SystemExit("repository root with CMakePresets.json not found")


REPO_ROOT = repository_root()

kernel = ct.WinDLL("kernel32", use_last_error=True)
kernel.OpenProcess.argtypes = [w.DWORD, w.BOOL, w.DWORD]
kernel.OpenProcess.restype = w.HANDLE
kernel.CloseHandle.argtypes = [w.HANDLE]
kernel.SetProcessAffinityMask.argtypes = [w.HANDLE, ct.c_size_t]
kernel.GetProcessAffinityMask.argtypes = [w.HANDLE, ct.POINTER(ct.c_size_t), ct.POINTER(ct.c_size_t)]
kernel.SetProcessInformation.argtypes = [w.HANDLE, ct.c_int, ct.c_void_p, w.DWORD]
kernel.GetProcessInformation.argtypes = [w.HANDLE, ct.c_int, ct.c_void_p, w.DWORD]
kernel.GetPriorityClass.argtypes = [w.HANDLE]
kernel.GetPriorityClass.restype = w.DWORD
kernel.GetCurrentProcess.restype = w.HANDLE
kernel.GetSystemPowerStatus.argtypes = [ct.c_void_p]


class Power(ct.Structure):
    _fields_ = [("Version", w.DWORD), ("ControlMask", w.DWORD), ("StateMask", w.DWORD)]


class Battery(ct.Structure):
    _fields_ = [
        ("ACLineStatus", w.BYTE),
        ("BatteryFlag", w.BYTE),
        ("BatteryLifePercent", w.BYTE),
        ("SystemStatusFlag", w.BYTE),
        ("BatteryLifeTime", w.DWORD),
        ("BatteryFullLifeTime", w.DWORD),
    ]


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as stream:
        for chunk in iter(lambda: stream.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def second_windows(samples, seconds):
    """Completed one-second windows of measured time, counted by frame end timestamps."""
    windows = [0] * seconds
    for sample in samples:
        index = int(sample["end_us"] // 1000000.0)
        if 0 <= index < seconds:
            windows[index] += 1
    return windows


def stats(values):
    ordered = sorted(values)
    count = len(ordered)
    return {
        "n": count,
        "mean_us": statistics.mean(ordered),
        "p50_us": ordered[math.ceil(0.50 * count) - 1],
        "p95_us": ordered[math.ceil(0.95 * count) - 1],
        "p99_us": ordered[math.ceil(0.99 * count) - 1],
        "max_us": ordered[-1],
        "over_budget": sum(1 for value in ordered if value > BUDGET_US),
    }


def configure_child(process, expected_mask):
    handle = kernel.OpenProcess(0x0200 | 0x1000, False, process.pid)
    if not handle:
        raise RuntimeError("OpenProcess failed")
    try:
        power = Power(1, 1, 0)
        if not kernel.SetProcessAffinityMask(handle, expected_mask):
            raise RuntimeError("SetProcessAffinityMask failed")
        if not kernel.SetProcessInformation(handle, 4, ct.byref(power), ct.sizeof(power)):
            raise RuntimeError("SetProcessInformation failed")
        observed = Power(1, 0, 0)
        mask = ct.c_size_t()
        system = ct.c_size_t()
        if not kernel.GetProcessAffinityMask(handle, ct.byref(mask), ct.byref(system)):
            raise RuntimeError("GetProcessAffinityMask failed")
        if not kernel.GetProcessInformation(handle, 4, ct.byref(observed), ct.sizeof(observed)):
            raise RuntimeError("GetProcessInformation failed")
        controls = {
            "affinity": mask.value,
            "priority": kernel.GetPriorityClass(handle),
            "qos_control": observed.ControlMask,
            "qos_state": observed.StateMask,
        }
        expected = {
            "affinity": expected_mask,
            "priority": 16384,
            "qos_control": 1,
            "qos_state": 0,
        }
        if controls != expected:
            raise RuntimeError("child controls {} != {}".format(controls, expected))
        return controls
    finally:
        kernel.CloseHandle(handle)


def run_one(exe, out, mode, index, args, mask, registration):
    assert sha256(exe) == registration["binary_sha256"], "binary changed"
    assert sha256(REPO_ROOT / "tests/player/gpu_throughput_probe.cpp") == \
        registration["probe_source_sha256"], "probe source changed"
    log = out / "{:02d}-{}-{}.log.txt".format(index, mode, args.entities)
    env = {key: value for key, value in os.environ.items()
           if not key.startswith(("CUEXIS_PD", "CUEXIS_GPU_"))}
    env.update(
        CUEXIS_GPU_BARRIER="1",
        CUEXIS_GPU_WIDTH=str(args.width),
        CUEXIS_GPU_HEIGHT=str(args.height),
        CUEXIS_GPU_ENTITIES=str(args.entities),
        CUEXIS_GPU_MODE=mode,
        CUEXIS_GPU_FRAMES=str(args.frames),
        CUEXIS_GPU_WARMUP=str(args.warmup),
        CUEXIS_GPU_WARMUP_MS=str(args.warmup_ms),
        CUEXIS_GPU_TIMER="1",
        CUEXIS_GPU_PIXEL_READBACK="1",
    )
    if args.seconds:
        env["CUEXIS_GPU_SECONDS"] = str(args.seconds)
    battery = Battery()
    if not kernel.GetSystemPowerStatus(ct.byref(battery)):
        raise RuntimeError("GetSystemPowerStatus failed")
    started = dt.datetime.now(dt.timezone.utc).isoformat()
    with log.open("x", encoding="utf-8") as stream:
        child = subprocess.Popen(
            [str(exe), "GPU visible entity throughput probe", "--rng-seed", "1"],
            cwd=str(REPO_ROOT),
            env=env,
            stdin=subprocess.PIPE,
            stdout=stream,
            stderr=subprocess.STDOUT,
            creationflags=subprocess.CREATE_NO_WINDOW | subprocess.BELOW_NORMAL_PRIORITY_CLASS,
        )
        try:
            controls = configure_child(child, mask)
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
    text = log.read_text(encoding="utf-8", errors="replace")
    identity_lines = [line for line in text.splitlines()
                      if line.startswith("GPU_THROUGHPUT_ID ")]
    samples = [json.loads(line.split(" ", 1)[1]) for line in text.splitlines()
               if line.startswith("GPU_THROUGHPUT_SAMPLE ")]
    identity = json.loads(identity_lines[0].split(" ", 1)[1]) if len(identity_lines) == 1 else None
    problems = []
    if code != 0:
        problems.append("exit {}".format(code))
    if "All tests passed" not in text:
        problems.append("test not passed")
    if identity is None:
        problems.append("identity lines {}".format(len(identity_lines)))
    if not samples:
        problems.append("no samples")
    if not problems:
        expected = {
            "mode": mode,
            "entities": args.entities,
            "draws_per_frame": args.entities,
            "in_view_centers": args.entities,
            "fixture_revision": 3,
            "timer": True,
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
        if len(samples) != identity["frames"]:
            problems.append("samples {} != identity frames {}".format(
                len(samples), identity["frames"]))
        if args.seconds:
            # A process that finishes the full duration but stays below the registered rate is a
            # *budget failure*, not an invalid sample: `budget_pass` and the per-second windows below
            # already record it. Treating it as invalid made a capacity ladder abort at its first
            # failing point instead of bracketing it, and it contradicted this runner's own
            # registered stop rule ("an invalid process stops the run; budget failures are
            # retained"). Truncation is still invalid, so a probe that dies early cannot hide here.
            if identity["measured_wall_us"] < args.seconds * 1000000.0:
                problems.append("measured wall below the duration")
        elif len(samples) != args.frames:
            problems.append("samples {} != requested {}".format(len(samples), args.frames))
        if identity["pixel_rgba"] != [255, 204, 51, 255]:
            problems.append("pixel {}".format(identity["pixel_rgba"]))
        if "NVIDIA" not in identity["vendor"]:
            problems.append("vendor {}".format(identity["vendor"]))
        if [s["ordinal"] for s in samples] != list(range(len(samples))):
            problems.append("ordinal discontinuity")
        if any("gpu_interval_us" not in sample for sample in samples):
            problems.append("sample without gpu_interval_us")
        for sample in samples:
            parts = sum(sample[key] for key in
                        ("update_us", "extract_us", "submit_us", "present_us"))
            if abs(parts - sample["total_us"]) >= 0.001:
                problems.append("total mismatch at {}".format(sample["ordinal"]))
                break
        if abs(sum(s["cycle_us"] for s in samples) - identity["measured_wall_us"]) >= 0.01:
            problems.append("cycle sum != measured_wall_us")
    summary = {}
    wall_fps = None
    if samples:
        summary = {metric: stats([s[metric] for s in samples])
                   for metric in samples[0] if metric not in ("ordinal", "end_us")}
    if identity:
        wall_fps = len(samples) * 1000000.0 / identity["measured_wall_us"]
    windows = second_windows(samples, args.seconds) if args.seconds else []
    min_window = min(windows) if windows else None
    passed = bool(
        not problems and wall_fps is not None and wall_fps >= 240.0
        and summary["cycle_us"]["p99_us"] <= BUDGET_US
        and (min_window is None or min_window >= 240)
    )
    return {
        "mode": mode,
        "index": index,
        "entities": args.entities,
        "frames": args.frames,
        "seconds": args.seconds,
        "log": log.name,
        "log_sha256": sha256(log),
        "started_utc": started,
        "exit": code,
        "controls": controls,
        "ac_line": battery.ACLineStatus,
        "identity": identity,
        "statistics": summary,
        "second_windows": windows,
        "min_second_fps": min_window,
        "wall_fps": wall_fps,
        "valid": not problems,
        "problems": problems,
        "budget_pass": passed,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--entities", type=int, default=ENTITY_LIMIT)
    parser.add_argument("--frames", type=int, default=120)
    parser.add_argument("--warmup", type=int, default=8)
    parser.add_argument("--warmup-ms", type=int, default=2000)
    parser.add_argument("--width", type=int, default=1920)
    parser.add_argument("--height", type=int, default=1080)
    parser.add_argument("--processes", type=int, default=3)
    parser.add_argument("--seconds", type=int, default=0,
                        help="duration run per process; 0 keeps the 120-frame rule")
    parser.add_argument("--timeout", type=int, default=600)
    parser.add_argument("--affinity", type=lambda text: int(text, 0), default=None,
                        help="default: the process default set (16 logical CPUs)")
    parser.add_argument("--allow-dirty", action="store_true")
    args = parser.parse_args()

    if args.entities > ENTITY_LIMIT:
        raise SystemExit("entities above the registered limit")
    out = Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=False)
    exe = Path(args.exe).resolve()

    allowed = ct.c_size_t()
    system_allowed = ct.c_size_t()
    if not kernel.GetProcessAffinityMask(kernel.GetCurrentProcess(), ct.byref(allowed),
                                        ct.byref(system_allowed)):
        raise SystemExit("GetProcessAffinityMask failed")
    mask = args.affinity if args.affinity is not None else allowed.value
    if mask != allowed.value:
        raise SystemExit("affinity {} does not match the default set {}".format(
            mask, allowed.value))

    dirty = subprocess.check_output(
        ["git", "status", "--porcelain", "--untracked-files=no"],
        cwd=str(REPO_ROOT)).decode("utf-8", "replace").splitlines()
    if dirty and not args.allow_dirty:
        raise SystemExit("worktree not clean: {}".format(dirty[:5]))

    registration = {
        "scope": ("GPU-timer diagnosis of the submit-path stall at 1080p; separate instrument, "
                  "diagnostic only, not the pre-registered ladder and not acceptance"),
        "created_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "head": subprocess.check_output(["git", "rev-parse", "HEAD"],
                                        cwd=str(REPO_ROOT)).decode().strip(),
        "dirty_files": dirty,
        "plan": {
            "groups": list(MODES),
            "processes_per_group": args.processes,
            "entities": args.entities,
            "frames_per_process": None if args.seconds else args.frames,
            "seconds_per_process": args.seconds or None,
            "warmup": "at least {} frames and {} ms".format(args.warmup, args.warmup_ms),
            "viewport": [args.width, args.height],
            "affinity": mask,
            "priority": "BelowNormal",
            "qos": "execution throttling OFF",
            "timer": True,
            "pixel_readback": True,
            "budget_us": BUDGET_US,
            "pass_rule": ("wall FPS >= 240 and cycle p99 <= 4.166667 ms and every complete "
                          "one-second window >= 240 frames" if args.seconds else
                          "wall FPS >= 240 and cycle p99 <= 4.166667 ms"),
            "stop": "an invalid process stops the run; budget failures are retained",
        },
        "binary": str(exe),
        "binary_sha256": sha256(exe),
        "probe_source_sha256": sha256(REPO_ROOT / "tests/player/gpu_throughput_probe.cpp"),
        "runner_sha256": sha256(Path(__file__)),
    }
    (out / "registration.json").write_text(json.dumps(registration, indent=2), encoding="utf-8")
    print(json.dumps({"head": registration["head"],
                      "binary_sha256": registration["binary_sha256"],
                      "affinity": mask}), flush=True)

    runs = []
    for mode in MODES:
        for index in range(1, args.processes + 1):
            record = run_one(exe, out, mode, index, args, mask, registration)
            runs.append(record)
            print(json.dumps({
                "mode": mode,
                "index": index,
                "valid": record["valid"],
                "wall_fps": round(record["wall_fps"], 2) if record["wall_fps"] else None,
                "p99_ms": round(record["statistics"].get("cycle_us", {}).get("p99_us", 0) / 1000, 3),
                "max_ms": round(record["statistics"].get("cycle_us", {}).get("max_us", 0) / 1000, 3),
                "over_budget": record["statistics"].get("cycle_us", {}).get("over_budget"),
                "min_second_fps": record["min_second_fps"],
                "budget_pass": record["budget_pass"],
                "problems": record["problems"],
            }, ensure_ascii=False), flush=True)
            if not record["valid"]:
                (out / "results.json").write_text(
                    json.dumps({"registration": registration, "runs": runs, "groups": {}},
                               indent=2, ensure_ascii=False), encoding="utf-8")
                raise SystemExit("invalid process: run stopped, no retry")

    groups = {}
    for mode in MODES:
        rows = [record for record in runs if record["mode"] == mode]
        fps = [record["wall_fps"] for record in rows]
        p99 = [record["statistics"]["cycle_us"]["p99_us"] for record in rows]
        frames = [record["statistics"]["cycle_us"]["n"] for record in rows]
        groups[mode] = {
            "processes": len(rows),
            "frames": frames,
            "seconds": args.seconds or None,
            "min_second_fps": [record["min_second_fps"] for record in rows] if args.seconds else None,
            "wall_fps": fps,
            "wall_fps_median": statistics.median(fps),
            "p99_us": p99,
            "p99_us_median": statistics.median(p99),
            "p99_relative_range": (max(p99) - min(p99)) / statistics.median(p99),
            "fps_relative_range": (max(fps) - min(fps)) / statistics.median(fps),
            "all_passed_budget": all(record["budget_pass"] for record in rows),
            "p99_vs_budget": statistics.median(p99) / BUDGET_US,
            "required_speedup": 240.0 / statistics.median(fps),
            "per_entity_mean_us": statistics.median(
                [record["statistics"]["cycle_us"]["mean_us"] for record in rows]) / args.entities,
            "per_entity_p99_us": statistics.median(p99) / args.entities,
            "per_entity_budget_us": BUDGET_US / args.entities,
        }
    payload = {"registration": registration, "runs": runs, "groups": groups}
    (out / "results.json").write_text(json.dumps(payload, indent=2, ensure_ascii=False),
                                      encoding="utf-8")
    print(json.dumps(groups, ensure_ascii=False, indent=1), flush=True)


if __name__ == "__main__":
    sys.exit(main())

"""Run a pre-registered external-observation set for the 40K startup question.

Registered before the first run of the stamp (this file is the registration):

  stamp 20261101-w1, arms interleaved sample by sample in the listed order:
    player-40k-untraced x3   cuexis_player.exe --project player-40k-project, close at 8 s
    player-40k-traced   x3   same, with CUEXIS_PREPARE_TRACE=1
    probe-40k-static-untraced x1  probe, 40000 static, 240 frames, warmup 30
    probe-40k-static-traced   x1  same, with CUEXIS_PREPARE_TRACE=1

  stamp 20261101-w2, the device-first start-up order against the default order:
    player-40k-untraced      x3  default order, close at 8 s
    player-40k-window-first  x3  CUEXIS_PLAYER_WINDOW_FIRST=1, close at 8 s
    player-40k-window-first-close1 x1  same, but the close is posted at 1.0 s, i.e. while the
                                        content is still preparing
    player-40k-untraced-close1     x1  default order, close posted at 1.0 s, no close possible
                                        before the window exists, so it runs into the 40 s wall
  The last two arms answer one question: can a user who started the wrong file close it? They are
  behavioural arms, not throughput arms, and are reported as such.

Every sample is one fresh process observed by watch_window.py; nothing is re-sampled or excluded.
Each arm's report is the median of the per-process values, plus the spread, so repeatability is
visible without trusting one number. The Player closes itself on a posted WM_CLOSE, which is also
the measured close latency.

Usage (from D:\\Cuexis-redesign):
    python -B run_watch_set.py --stamp 20261101-w1
"""

import argparse
import json
import os
import pathlib
import statistics
import sys

import watch_window

ROOT = pathlib.Path("out/verification-20261101/r3-startup-observability-v1")
PLAYER = pathlib.Path("out/build/rpa-candidate-release/bin/cuexis_player.exe")
PROBE = pathlib.Path("out/build/rpa-candidate-release/bin/cuexis_execution_device_probe.exe")
PROJECT = ROOT / "player-40k-project"
TEST_NAME = "GPU visible entity throughput probe"

PLAN = {
    "20261101-w1": [
        ("player-40k-untraced", 3),
        ("player-40k-traced", 3),
        ("probe-40k-static-untraced", 1),
        ("probe-40k-static-traced", 1),
    ],
    "20261101-w2": [
        ("player-40k-untraced", 3),
        ("player-40k-window-first", 3),
        ("player-40k-window-first-close1", 1),
        ("player-40k-untraced-close1", 1),
    ],
}

PROBE_STAGES = [
    "fixture_source", "playback_prepare", "sdl_runtime", "gl_context", "window_create",
    "backend_create", "pixel_readback", "renderer_prepare", "session_commit", "backend_activate",
    "first_update", "first_extract", "first_submit", "first_present",
]


def arm_arguments(arm: str) -> tuple[list[str], list[str], float, float]:
    """Return (command, env overrides, close_after, timeout)."""
    traced = arm.endswith("-traced")
    env = ["CUEXIS_PREPARE_TRACE=1"] if traced else []
    if arm.startswith("player"):
        if "window-first" in arm:
            env = ["CUEXIS_PLAYER_WINDOW_FIRST=1"] + env
        close_after = 1.0 if arm.endswith("-close1") else 8.0
        return [str(PLAYER), "--project", str(PROJECT)], env, close_after, 40.0
    probe_env = [
        "CUEXIS_GPU_ENTITIES=40000",
        "CUEXIS_GPU_MODE=static",
        "CUEXIS_GPU_FRAMES=240",
        "CUEXIS_GPU_WARMUP=30",
        "CUEXIS_GPU_TIMER=0",
    ]
    return [str(PROBE), TEST_NAME], probe_env + env, 0.0, 120.0


def parse_player_phases(text: str) -> dict:
    phases = {}
    for line in text.splitlines():
        if "[player.startup_phase]" not in line:
            continue
        parts = line.split("[player.startup_phase]", 1)[1].split()
        if len(parts) >= 5 and parts[1] == "elapsed_us" and parts[3] == "total_us":
            phases[parts[0]] = {"elapsed_us": float(parts[2]), "total_us": float(parts[4])}
    return phases


def parse_probe_stages(text: str) -> dict:
    stages = {}
    for line in text.splitlines():
        if line.startswith("GPU_STARTUP_END "):
            parts = line.split()
            stages[parts[1]] = float(parts[2])
    return stages


def parse_probe_frames(text: str) -> dict:
    cycles, totals = [], []
    for line in text.splitlines():
        if not line.startswith("GPU_THROUGHPUT_SAMPLE "):
            continue
        payload = json.loads(line[len("GPU_THROUGHPUT_SAMPLE "):])
        cycles.append(payload["cycle_us"])
        totals.append(payload["total_us"])
    frames = {}
    if cycles:
        frames = {
            "frames": len(cycles),
            "median_cycle_us": round(statistics.median(cycles), 3),
            "median_frame_fps": round(1e6 / statistics.median(cycles), 3),
            "median_total_us": round(statistics.median(totals), 3),
        }
    return frames


def median_of(values):
    clean = [value for value in values if value is not None]
    return round(statistics.median(clean), 4) if clean else None


def spread_of(values):
    clean = [value for value in values if value is not None]
    if len(clean) < 2 or statistics.median(clean) == 0:
        return None
    return round((max(clean) - min(clean)) / statistics.median(clean) * 100.0, 2)


def summarise(arm: str, samples: list[dict]) -> dict:
    keys = [
        "window_appeared_t_s", "primary_appeared_t_s", "primary_visible_t_s", "exit_t_s",
        "wm_close_to_exit_s", "probes_after_primary", "probes_timed_out",
        "probe_latency_over_100ms",
    ]
    medians = {key: median_of([s["summary"].get(key) for s in samples]) for key in keys}
    spreads = {key: spread_of([s["summary"].get(key) for s in samples]) for key in keys}
    latency = {
        key: median_of([s["summary"]["probe_latency_ms"].get(key) for s in samples])
        for key in ("p50", "p90", "p99", "max")
    }
    cpu = {}
    for key in samples[0]["summary"]["cpu_core_ratio"]:
        cpu[key] = median_of([s["summary"]["cpu_core_ratio"].get(key) for s in samples])
    phases = {}
    for sample in samples:
        for name, value in sample["phases"].items():
            phases.setdefault(name, {"elapsed_us": [], "total_us": []})
            phases[name]["elapsed_us"].append(value["elapsed_us"])
            phases[name]["total_us"].append(value["total_us"])
    phase_medians = {
        name: {"elapsed_us": median_of(values["elapsed_us"]),
               "total_us": median_of(values["total_us"])}
        for name, values in phases.items()
    }
    stages = {}
    for sample in samples:
        for name, value in sample["stages"].items():
            stages.setdefault(name, []).append(value)
    stage_medians = {name: median_of(values) for name, values in stages.items()}
    frames = [sample["frames"] for sample in samples if sample["frames"]]
    return {
        "arm": arm,
        "n": len(samples),
        "return_codes": [s["summary"]["return_code"] for s in samples],
        "killed": [s["summary"]["killed"] for s in samples],
        "medians": medians,
        "spread_pct": {k: v for k, v in spreads.items() if v is not None},
        "probe_latency_ms": latency,
        "cpu_core_ratio": cpu,
        "player_phases_us": phase_medians,
        "probe_stages_us": {name: stage_medians.get(name) for name in PROBE_STAGES},
        "probe_frames": frames,
        "windows": [s["summary"].get("windows") for s in samples],
        "per_process": [
            {
                "index": s["index"],
                "summary": {
                    key: s["summary"].get(key)
                    for key in (
                        "window_appeared_t_s", "primary_appeared_t_s", "primary_visible_t_s",
                        "primary_final_client", "probe_latency_ms", "cpu_core_ratio", "exit_t_s",
                        "wm_close_to_exit_s", "probes_after_primary",
                    )
                },
            }
            for s in samples
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--stamp", required=True)
    parser.add_argument("--only-arm", default=None)
    args = parser.parse_args()
    plan = PLAN[args.stamp]
    if args.only_arm:
        plan = [(arm, count) for arm, count in plan if arm == args.only_arm]
    stamp_dir = ROOT / "runs" / args.stamp
    stamp_dir.mkdir(parents=True, exist_ok=True)
    order = []
    for cycle in range(max(count for _, count in plan)):
        for arm, count in plan:
            if cycle < count:
                order.append(arm)
    samples = []
    for position, arm in enumerate(order):
        index = sum(1 for previous in samples if previous["arm"] == arm)
        command, env, close_after, timeout = arm_arguments(arm)
        label = f"{arm}-{index:02d}"
        namespace = argparse.Namespace(
            label=label,
            out=str(stamp_dir / "raw"),
            cwd=None,
            env=env,
            timeout=timeout,
            close_after=close_after,
            poll_ms=2.0,
            enum_interval_ms=5.0,
            probe_interval_ms=25.0,
            probe_timeout_ms=400.0,
            cpu_interval_ms=100.0,
            json=str(stamp_dir / "raw" / f"{label}.json"),
            command=command,
        )
        result = watch_window.run(namespace)
        out_text = (stamp_dir / "raw" / f"{label}.out.txt").read_text(encoding="utf-8", errors="replace")
        err_text = (stamp_dir / "raw" / f"{label}.err.txt").read_text(encoding="utf-8", errors="replace")
        sample = {
            "arm": arm,
            "index": index,
            "position": position,
            "summary": result["summary"],
            "phases": parse_player_phases(out_text),
            "stages": parse_probe_stages(out_text),
            "frames": parse_probe_frames(out_text),
            "probes": result["probes"],
            "events": result["events"],
            "cpu_samples": result["cpu_samples"],
            "trace_events": [
                line for line in err_text.splitlines() if line.startswith("PREPARE_TRACE_")
            ],
        }
        samples.append(sample)
        print(
            f"{label}: rc={sample['summary']['return_code']} "
            f"visible={sample['summary']['primary_visible_t_s']} "
            f"close={sample['summary']['wm_close_to_exit_s']} "
            f"p50={sample['summary']['probe_latency_ms']['p50']}",
            flush=True,
        )
    payload = {
        "stamp": args.stamp,
        "plan": {arm: count for arm, count in plan},
        "order": order,
        "summary": {
            arm: summarise(arm, [s for s in samples if s["arm"] == arm]) for arm, _ in plan
        },
        "samples": samples,
    }
    path = stamp_dir / "watch-set.json"
    path.write_text(json.dumps(payload, indent=1, ensure_ascii=True) + "\n", encoding="utf-8")
    print(f"wrote {path}", flush=True)
    for arm in payload["summary"]:
        entry = payload["summary"][arm]
        print(
            f"{arm}: n={entry['n']} visible={entry['medians']['primary_visible_t_s']} "
            f"p50={entry['probe_latency_ms']['p50']} max={entry['probe_latency_ms']['max']} "
            f"close={entry['medians']['wm_close_to_exit_s']}",
            flush=True,
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())

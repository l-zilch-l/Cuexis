"""Windows static PD-0 method calibration and separately registered acceptance.

No historical sample is changed. Calibration is excluded from acceptance; a failed
fixed run stops the runner. No adaptive reruns or post-hoc threshold changes.
"""
import argparse
import ctypes as ct
from ctypes import wintypes as wt
import datetime as dt
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import statistics as st
import subprocess as sp
import sys
import time
import uuid

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[3]))

from tools.verification.run_rpa_acceptance_matrix import identity as repository_identity

ROOT = Path(__file__).resolve().parents[3]


def identity():
    result = repository_identity()
    result["repository_implementation_sha256"] = result["implementation_sha256"]
    # Other chats may reorganize unrelated tools. Freeze all implementation inputs
    # except those tools; preserve the full repository inventory for auditing.
    implementation = {p: digest for p, digest in result["files"].items()
                      if not p.startswith("docs/") and not p.endswith(".md")
                      and (not p.startswith("tools/") or p in (
                          "tools/verification/playback_performance/baseline_method.py",
                          "tools/verification/run_rpa_acceptance_matrix.py"))}
    result["implementation_sha256"] = hashlib.sha256(
        json.dumps(implementation, sort_keys=True).encode()).hexdigest()
    result["implementation_scope"] = "repository implementation excluding unrelated tools; full inventory retained"
    return result
K = ct.WinDLL("kernel32", use_last_error=True)
K.GetCurrentProcess.restype = wt.HANDLE
K.GetProcessAffinityMask.argtypes = [wt.HANDLE, ct.POINTER(ct.c_size_t), ct.POINTER(ct.c_size_t)]
K.SetProcessAffinityMask.argtypes = [wt.HANDLE, ct.c_size_t]


def checked(ok):
    if not ok:
        raise ct.WinError(ct.get_last_error())


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def save(path, data):
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def stats(values):
    values = sorted(values)
    return {"n": len(values), "median_us": st.median(values), "mean_us": st.mean(values),
            "p95_us": values[math.ceil(.95 * len(values))-1] if len(values) >= 200 else None,
            "p99_us": values[math.ceil(.99 * len(values))-1] if len(values) >= 1000 else None,
            "max_us": values[-1], "min_us": values[0]}


def noise(values):
    return (max(values) - min(values)) / st.median(values)


def machine():
    class Power(ct.Structure):
        _fields_ = [("ac", wt.BYTE), ("flag", wt.BYTE), ("percent", wt.BYTE),
                    ("reserved", wt.BYTE), ("life", wt.DWORD), ("full_life", wt.DWORD)]
    power = Power()
    checked(K.GetSystemPowerStatus(ct.byref(power)))
    process_mask, system_mask = ct.c_size_t(), ct.c_size_t()
    checked(K.GetProcessAffinityMask(K.GetCurrentProcess(), ct.byref(process_mask), ct.byref(system_mask)))
    scheme = sp.check_output(["powercfg", "/getactivescheme"])
    return {"os": platform.platform(), "processor": os.environ.get("PROCESSOR_IDENTIFIER"),
            "logical_processors": os.cpu_count(), "parent_affinity": process_mask.value,
            "selected_affinity": process_mask.value & -process_mask.value,
            "selected_core_type": "unknown", "ac_line_status": power.ac,
            "power_scheme_raw_sha256": hashlib.sha256(scheme).hexdigest(),
            "power_scheme": scheme.decode("utf-8", errors="replace").strip(),
            "priority_change": "none", "gpu_audio_vsync": "not used; headless CPU only"}


def binary_info(path):
    path = path.resolve()
    cache = path.parent.parent / "CMakeCache.txt"
    content = cache.read_text(encoding="utf-8")
    for expected in ("CUEXIS_LIBRARY_TYPE:STRING=STATIC", "CUEXIS_ENABLE_CHART_V5_CANDIDATE:BOOL=ON",
                     "CUEXIS_PD_METRICS_TEST_ONLY:BOOL=ON"):
        if expected not in content:
            raise ValueError(f"Required cache value missing: {expected} in {cache}")
    compiler_files = list((path.parent.parent / "CMakeFiles").glob("*/CMakeCXXCompiler.cmake"))
    compiler = {str(p.relative_to(ROOT)): {"sha256": sha(p), "contents": p.read_text()}
                for p in compiler_files}
    return {"path": str(path), "sha256": sha(path), "cache_sha256": sha(cache), "compiler": compiler}


def invoke(output, stem, binary, mask, profile=None, active=True, calibration=False):
    env = os.environ.copy()
    env.pop("CUEXIS_PD_METRICS", None)
    if active:
        env["CUEXIS_PD_METRICS"] = str(output / stem)
    if profile:
        env["CUEXIS_PD_PROFILE"] = profile
    test = "PD clock and observer calibration" if calibration else "PD fixed trajectory measurement"
    command = [sys.executable, "-B", str(Path(__file__).resolve()), "--child", str(mask),
               str(binary), test, "--rng-seed", "1"]
    start = time.perf_counter()
    with (output / (stem + ".log")).open("wb") as log:
        result = sp.run(command, env=env, cwd=ROOT, stdout=log, stderr=sp.STDOUT, timeout=120)
    text = (output / (stem + ".log")).read_text(encoding="utf-8", errors="replace")
    prefix = "PD_CLOCK " if calibration else "PD_METHOD "
    lines = [line[len(prefix):] for line in text.splitlines() if line.startswith(prefix)]
    record = {"name": stem, "command": command, "exit_code": result.returncode,
              "wall_seconds": time.perf_counter() - start, "log_sha256": sha(output / (stem + ".log")),
              "data": json.loads(lines[0]) if len(lines) == 1 else None,
              "diagnostics": [], "active": active, "profile": profile}
    for path in output.glob(stem + ".*.jsonl"):
        for line in path.read_text().splitlines():
            item = json.loads(line)
            record["diagnostics"].append({"stage": item["stage"], "samples": stats(item["exclusive_us"]),
                                           "items": item["items"], "omitted": item["omitted"],
                                           "allocation_count": item.get("allocation_count"),
                                           "requested_allocation_bytes": item.get("requested_allocation_bytes"),
                                           "logical_copy_bytes": item.get("logical_copy_bytes"),
                                           "use": "single-call diagnostic; not a performance gate"})
    if active and not calibration:
        actual = {item["stage"]: item for item in record["diagnostics"]}
        for stage in ("judgement", "projection", "runtime", "snapshot"):
            if stage not in actual or actual[stage]["samples"]["n"] != 8192 or actual[stage]["omitted"]:
                record["diagnostic_window_error"] = f"Expected 8192 untruncated {stage} samples"
    save(output / (stem + ".json"), record)
    print(f"{stem}: exit={result.returncode}", flush=True)
    if result.returncode or not record["data"] or record.get("diagnostic_window_error"):
        raise RuntimeError(f"Measurement failed; preserved {output / (stem + '.log')}")
    return record


def run(args):
    output = args.output.resolve() / (args.phase + "-" + dt.datetime.now(dt.timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "-" + uuid.uuid4().hex[:6])
    output.mkdir(parents=True)
    env = machine()
    binaries = {"release": binary_info(args.release), "debug": binary_info(args.debug)}
    source = identity()
    manifest = {"revision": "pd0-fixed-trace-method-v1", "phase": args.phase,
                "started_utc": dt.datetime.now(dt.timezone.utc).isoformat(), "source": source,
                "runner_sha256": sha(__file__), "machine": env, "binaries": binaries,
                "registration": {"noise_limit": .15, "noise_formula": "(max-min)/median of process medians",
                    "warmup": "one separate fresh complete trajectory plus 64 fixed steps per measured session",
                    "trajectories_per_process": 8, "steps_per_trajectory": 1024,
                    "read_blocks": 128, "read_operations_per_block": 4096,
                    "primary": ["fixed trajectory block duration", "read-only Snapshot block duration"],
                    "microtimings": "diagnostic only; never gated by relative single-call noise",
                    "minimum_block_clock_steps": 100, "maximum_empty_read_fraction": .05,
                    "max_observer_relative_overhead": .15,
                    "quantile_support": "p95 requires 200 samples; p99 requires 1000; otherwise null",
                    "acceptance_pair_order": [["dormant", "active"], ["active", "dormant"], ["dormant", "active"]],
                    "between_process_seconds": 1, "historical_disposition": "unchanged inconclusive",
                    "no_rerun_or_outlier_removal": True},
                "runs": [], "pd0_exit": False, "production_thresholds_accepted": False}
    if args.phase == "accept":
        if not args.calibration:
            raise ValueError("Acceptance requires a completed calibration manifest")
        calibration = json.loads(args.calibration.read_text(encoding="utf-8"))
        if calibration.get("status") != "calibration_applicable":
            raise ValueError("Calibration did not establish applicability")
        for key in ("implementation_sha256",):
            if source[key] != calibration["source"][key]:
                raise ValueError("Implementation changed since calibration")
        if binaries != calibration["binaries"] or env != calibration["machine"]:
            raise ValueError("Binary or controlled machine conditions changed since calibration")
        manifest["calibration"] = {"path": str(args.calibration.resolve()), "sha256": sha(args.calibration)}
    save(output / "manifest.json", manifest)  # Freeze method and identity before observations.
    print("EVIDENCE " + str(output), flush=True)
    try:
        if args.phase == "calibrate":
            for config, binary in binaries.items():
                for i in range(3):
                    manifest["runs"].append(invoke(output, f"{config}-clock-{i+1}", binary["path"], env["selected_affinity"], calibration=True))
                    save(output / "manifest.json", manifest)
        for config, binary in binaries.items():
            for profile in ("legacy", "rpa"):
                orders = [["dormant", "active"]] if args.phase == "calibrate" else manifest["registration"]["acceptance_pair_order"]
                for pair, order in enumerate(orders, 1):
                    for mode in order:
                        record = invoke(output, f"{config}-{profile}-{pair}-{mode}", binary["path"], env["selected_affinity"], profile, mode == "active")
                        record.update(config=config, pair=pair)
                        manifest["runs"].append(record)
                        save(output / "manifest.json", manifest)
                        time.sleep(1)
        results = []
        clock_step_us = max(r["data"]["minimum_positive_ns"] for r in manifest["runs"] if "minimum_positive_ns" in r["data"]) / 1000 if args.phase == "calibrate" else calibration["clock_step_us"]
        manifest["clock_step_us"] = clock_step_us
        for config in binaries:
            for profile in ("legacy", "rpa"):
                rows = [r for r in manifest["runs"] if r.get("config") == config and r["profile"] == profile]
                golden_keys = ("digest", "replay_sha256", "snapshot_sha256", "packed_sha256", "configuration_sha256")
                same = all(all(r["data"][k] == rows[0]["data"][k] for k in golden_keys) for r in rows)
                for mode in (False, True):
                    subset = [r for r in rows if r["active"] == mode]
                    for metric in ("trace_us", "read_block_us"):
                        medians = [st.median(r["data"][metric]) for r in subset]
                        resolution_ok = min(min(r["data"][metric]) for r in subset) >= 100 * clock_step_us
                        empty_ok = all(st.median(r["data"]["empty_read_block_us"]) / st.median(r["data"]["read_block_us"]) <= .05 for r in subset)
                        item = {"config": config, "profile": profile, "active": mode, "metric": metric,
                                "per_process": [stats(r["data"][metric]) for r in subset],
                                "repeat_medians_us": medians, "noise": noise(medians) if len(medians) >= 3 else None,
                                "resolution_ok": resolution_ok, "empty_read_ok": empty_ok, "exact_equivalence": same}
                        item["pass"] = resolution_ok and empty_ok and same and (args.phase == "calibrate" or item["noise"] <= .15)
                        results.append(item)
                dormant = st.median(st.median(r["data"]["trace_us"]) for r in rows if not r["active"])
                active = st.median(st.median(r["data"]["trace_us"]) for r in rows if r["active"])
                overhead = (active - dormant) / dormant
                manifest.setdefault("observer_overhead", []).append({"config": config, "profile": profile,
                    "relative_overhead": overhead, "pass": overhead <= .15})
        manifest["results"] = results
        manifest["source_after"] = identity()
        manifest["source_unchanged"] = source["implementation_sha256"] == manifest["source_after"]["implementation_sha256"]
        manifest["binaries_unchanged"] = all(sha(b["path"]) == b["sha256"] for b in binaries.values())
        good = all(r["pass"] for r in results) and all(r["pass"] for r in manifest["observer_overhead"]) and manifest["source_unchanged"] and manifest["binaries_unchanged"]
        manifest["status"] = ("calibration_applicable" if args.phase == "calibrate" else "method_validated") if good else "inconclusive"
        save(output / "manifest.json", manifest)
        print("STATUS " + manifest["status"], flush=True)
        return 0 if good else 1
    except Exception as error:
        manifest["status"] = "stopped_error"
        manifest["error"] = str(error)
        save(output / "manifest.json", manifest)
        raise


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--child":
        checked(K.SetProcessAffinityMask(K.GetCurrentProcess(), int(sys.argv[2])))
        raise SystemExit(sp.call(sys.argv[3:], cwd=ROOT))
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("phase", choices=("calibrate", "accept"))
    parser.add_argument("--release", type=Path, default=ROOT / "out/build/rpa-candidate-release/bin/cuexis_pd_baseline_probe.exe")
    parser.add_argument("--debug", type=Path, default=ROOT / "out/build/candidate-debug/bin/cuexis_pd_baseline_probe.exe")
    parser.add_argument("--output", type=Path, default=ROOT / "out/pd0-method-v1")
    parser.add_argument("--calibration", type=Path)
    raise SystemExit(run(parser.parse_args()))

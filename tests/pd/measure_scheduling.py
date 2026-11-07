"""Separate PD calibration/validation with paired traces and QPC block telemetry."""
import argparse
import datetime as dt
import json
import math
import os
from pathlib import Path
import statistics as st
import subprocess as sp
import sys
import uuid

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.verification.playback_performance import baseline_method as base
from measurement_statistics import paired_effect, per_operation


def invoke(output, name, binary, machine, profile, phase, reads, seed):
    env = dict(os.environ, CUEXIS_PD_PROFILE=profile, CUEXIS_PD_V2_PHASE=phase,
               CUEXIS_PD_V2_READS=str(reads), CUEXIS_PD_V2_SEED=str(seed),
               CUEXIS_PD_METRICS=str(output / name))
    cmd = [sys.executable, "-B", str(ROOT / "tools/verification/playback_performance/baseline_method.py"),
           "--child", str(machine["selected_affinity"]), str(binary),
           "PD interleaved scheduling measurement", "--rng-seed", "1"]
    with (output / (name + ".log")).open("wb") as log:
        result = sp.run(cmd, cwd=ROOT, env=env, stdout=log, stderr=sp.STDOUT, timeout=180)
    lines = (output / (name + ".log")).read_text(encoding="utf-8", errors="replace").splitlines()
    parse = lambda prefix: [json.loads(x[len(prefix):]) for x in lines if x.startswith(prefix)]
    clocks, identities, samples = parse("PD_V2_CLOCK "), parse("PD_V2_ID "), parse("PD_V2 ")
    if result.returncode or len(clocks) != 1 or len(identities) != 1:
        raise RuntimeError(f"Execution failed: {name}; exit {result.returncode}")
    trace = [x for x in samples if x["kind"] in ("trace", "trace_sham")]
    if len(trace) != (8 if phase == "pilot" else 16):
        raise RuntimeError("Incomplete fixed trace observations")
    quantum = max(clocks[0]["cpu_accounting_steps_us"])
    for row in samples:
        row["cpu_span_in_observed_steps"] = row["sample"]["cpu_us"] / quantum
        # Keep negative gaps too: coarse CPU accounting can straddle a window.
        row["wall_minus_cpu_us"] = row["sample"]["wall_us"] - row["sample"]["cpu_us"]
    diagnostics = [json.loads(line) for p in output.glob(name + ".*.jsonl")
                   for line in p.read_text().splitlines()]
    if any(d["omitted"] for d in diagnostics):
        raise RuntimeError("Internal diagnostic samples were omitted")
    if phase == "run":
        counts = {d["stage"]: len(d["exclusive_us"]) for d in diagnostics}
        for stage in ("judgement", "projection", "runtime", "snapshot", "runtime_state_copy",
                      "runtime_prepare", "runtime_commit"):
            if counts.get(stage) != 8192:
                raise RuntimeError(f"Unexpected {stage} count: {counts}")
    record = {"name": name, "command": cmd, "exit_code": result.returncode,
              "identity": identities[0], "clocks": clocks[0], "samples": samples,
              "paired_wall_effect": paired_effect(trace),
              "paired_cycle_effect": paired_effect(trace, "cycles"),
              "diagnostic_counts": {d["stage"]: len(d["exclusive_us"]) for d in diagnostics}}
    base.save(output / (name + ".json"), record)
    print(name + ": exit=0", flush=True)
    return record


def run(args):
    output = args.output.resolve() / (args.phase + "-" + dt.datetime.now(dt.timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "-" + uuid.uuid4().hex[:6])
    output.mkdir(parents=True)
    machine, source = base.machine(), base.identity()
    binaries = {"release": base.binary_info(args.release), "debug": base.binary_info(args.debug)}
    manifest = {"revision": "pd0-interleaved-clocks-v3-qpc", "phase": args.phase, "source": source,
                "machine": machine, "binaries": binaries, "registration": {
                    "absolute_wall_noise_limit": .15, "observer_relative_limit": .15,
                    "wall_noise_formula": "(max-min)/median of independent process medians",
                    "processes_per_config_profile": 1 if args.phase == "calibrate" else 3,
                    "quartets_per_process": 2 if args.phase == "calibrate" else 4,
                    "quartets": "balanced ABBA/BAAB, seeded shuffle; all fresh fixed 64+1024 trajectories",
                    "pilot_sham": "both nominal modes have internal recording disabled",
                    "read_pairs_per_process": 3 if args.phase == "calibrate" else 8,
                    "read_batch_selection": "pilot-only next power of two to >=max(250ms,20*observed max CPU accounting step); cap 4194304",
                    "minimum_validation_long_block_us": 100000,
                    "cpu_time_and_cycles": "diagnostic, no cycles-to-time conversion; CPU units are not resolution",
                    "no_filter_or_rerun": True, "power_priority_changes": "none",
                    "absolute_and_relative": "paired improvement cannot substitute for an unstable absolute wall baseline"},
                "runs": [], "read_counts": {}, "status": "registered", "pd0_exit": False}
    calibration = None
    if args.phase == "validate":
        if not args.calibration:
            raise ValueError("Validation requires a separate completed calibration")
        calibration = json.loads(args.calibration.read_text(encoding="utf-8"))
        if calibration["status"] != "calibration_applicable":
            raise ValueError("Calibration was not applicable")
        if calibration["source"]["implementation_sha256"] != source["implementation_sha256"] or calibration["binaries"] != binaries or calibration["machine"] != machine:
            raise ValueError("Controlled implementation/binary/machine identity changed")
        manifest["calibration"] = {"path": str(args.calibration.resolve()), "sha256": base.sha(args.calibration)}
        manifest["read_counts"] = calibration["read_counts"]
    base.save(output / "manifest.json", manifest)
    print("EVIDENCE " + str(output), flush=True)
    try:
        # Rotate configuration/profile order across repeats instead of keeping all
        # samples of one configuration together for many minutes.
        combinations = [(c, p) for c in binaries for p in ("legacy", "rpa")]
        for repeat in range(1 if args.phase == "calibrate" else 3):
            ordered = combinations[repeat:] + combinations[:repeat]
            for config, profile in ordered:
                key = config + "/" + profile
                record = invoke(output, f"{config}-{profile}-{repeat+1}", binaries[config]["path"],
                                machine, profile, "pilot" if calibration is None else "run",
                                4096 if calibration is None else manifest["read_counts"][key], repeat)
                record.update(config=config, profile=profile, repeat=repeat + 1)
                manifest["runs"].append(record)
                if calibration is None:
                    pilot = st.median(x["sample"]["wall_us"] for x in record["samples"] if x["kind"] == "read_short")
                    quantum = max(record["clocks"]["cpu_accounting_steps_us"])
                    target = max(250000, 20 * quantum)
                    required = max(4096, math.ceil(4096 * target / pilot))
                    if required > 4194304:
                        raise RuntimeError("Read batch target exceeds registered operation cap")
                    manifest["read_counts"][key] = min(4194304, 1 << (required - 1).bit_length())
                base.save(output / "manifest.json", manifest)
        results = []
        for config, profile in combinations:
            records = [r for r in manifest["runs"] if r["config"] == config and r["profile"] == profile]
            for label, kind, mode in (("trace_dormant", "trace", False), ("trace_active", "trace", True),
                                      ("read_short", "read_short", None), ("read_long", "read_long", None)):
                if calibration is None and kind == "trace":
                    kind = "trace_sham"
                medians = [st.median(per_operation(x) for x in r["samples"] if x["kind"] == kind and (mode is None or x["active"] == mode)) for r in records]
                noise = base.noise(medians) if len(medians) >= 3 else None
                results.append({"config": config, "profile": profile, "metric": label,
                                "process_medians_us_per_operation": medians, "noise": noise,
                                "primary": label != "read_short", "pass": noise is None or noise <= .15})
            effects = [r["paired_wall_effect"] for r in records]
            manifest.setdefault("paired_effects", []).append({"config": config, "profile": profile,
                "per_process": effects, "effect": math.expm1(st.mean(math.log1p(x) for x in effects)),
                "pass": all(abs(x) <= .15 for x in effects) if calibration is None else math.expm1(st.mean(math.log1p(x) for x in effects)) <= .15})
            identities = [{k: r["identity"][k] for k in ("digest", "replay_sha256", "snapshot_sha256", "packed_sha256")} for r in records]
            if any(x != identities[0] for x in identities):
                raise RuntimeError("Exact semantic identities differ")
        manifest["results"] = results
        manifest["source_after"] = base.identity()
        manifest["source_unchanged"] = manifest["source_after"]["implementation_sha256"] == source["implementation_sha256"]
        manifest["binaries_unchanged"] = all(base.sha(b["path"]) == b["sha256"] for b in binaries.values())
        manifest["long_block_duration_ok"] = calibration is None or all(x["sample"]["wall_us"] >= 100000 for r in manifest["runs"] for x in r["samples"] if x["kind"] == "read_long")
        good = all(r["pass"] for r in results if r["primary"]) and all(r["pass"] for r in manifest["paired_effects"]) and manifest["source_unchanged"] and manifest["binaries_unchanged"] and manifest["long_block_duration_ok"]
        manifest["status"] = ("calibration_applicable" if calibration is None else "method_validated") if good else "inconclusive"
        base.save(output / "manifest.json", manifest)
        base.save(output / "evidence-hashes.json", {p.name: base.sha(p) for p in output.iterdir() if p.is_file()})
        print("STATUS " + manifest["status"], flush=True)
        return 0 if good else 1
    except Exception as error:
        manifest.update(status="stopped_error", error=str(error))
        base.save(output / "manifest.json", manifest)
        raise


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("phase", choices=("calibrate", "validate"))
    parser.add_argument("--output", type=Path, default=ROOT / "out/pd0-scheduling-v2")
    parser.add_argument("--release", type=Path, default=ROOT / "out/build/rpa-candidate-release/bin/cuexis_pd_baseline_probe.exe")
    parser.add_argument("--debug", type=Path, default=ROOT / "out/build/candidate-debug/bin/cuexis_pd_baseline_probe.exe")
    parser.add_argument("--calibration", type=Path)
    raise SystemExit(run(parser.parse_args()))

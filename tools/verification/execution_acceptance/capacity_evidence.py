"""One bound capacity process and independent CPU, logical GPU and residency evidence.

This collector never widens an allowance or grants acceptance. A formal invocation must be
launched from an admitted, sealed capacity preparation; qualification runs are explicitly marked.
"""
import argparse
import json
from pathlib import Path
import re
import time

from tools.verification.playback_performance import measurement, process_control
from . import build_binding, memory_attribution, memory_domains, semantic_memory
from .admission import read_sealed
from .binding import artifact, clean_source, read, verify_artifact, write_once
from .environment import capture_cpu, process_environment, verify_cpu
from .runtime_monitor import RuntimeMonitor

TEST_CASE = "PD5 full content Snapshot Seek Replay boundary"
SEMANTIC_FIELDS = ("reuse", "entities", "requirements", "packed_bytes", "snapshot_bytes",
                   "packed_sha256", "snapshot_sha256", "snapshot_valid",
                   "decoded_snapshot_valid", "advance_executed", "replay_evidence_valid",
                   "frame_byte_bound", "global_payloads", "global_payload_bytes",
                   "replay_equal", "closed", "gpu")
PHASES = ("before-device", "empty-device", "active", "after-close", "after-fixture-release")


def capacity(text, entities, rows, reuse, device):
    values = measurement.records(text, "PD5_CAPACITY ")
    if len(values) != 1 or values[0].get("schema") != "pd5.capacity.v1":
        raise ValueError("one complete capacity identity required")
    value = values[0]
    expected = {"entities": entities, "requirements": rows, "reuse": reuse, "gpu": device,
                **{key: True for key in ("snapshot_valid", "decoded_snapshot_valid",
                                         "advance_executed", "replay_evidence_valid",
                                         "replay_equal", "closed")}}
    if any(type(value.get(key)) is not type(item) or value[key] != item
           for key, item in expected.items()):
        raise ValueError("capacity workload or semantic result differs from declaration")
    for key in ("packed_sha256", "snapshot_sha256"):
        if not re.fullmatch(r"[a-f0-9]{64}", value.get(key, "")):
            raise ValueError("capacity output hash missing")
    for key in ("packed_bytes", "snapshot_bytes", "frame_byte_bound"):
        if type(value.get(key)) is not int or value[key] <= 0:
            raise ValueError("invalid capacity size")
    return value


def gpu_storage(text, ceiling):
    values = measurement.records(text, "PD5_GPU_MEMORY ")
    if not values or values[-1].get("label") != "after-device-release":
        raise ValueError("logical GPU release evidence missing")
    for value in values:
        if value.get("exceeded") is not False or value.get("gpu_drained") is not True:
            raise ValueError("GPU observer range or completion failed")
        for live, peak, limit in (("live_count", "peak_count", "limit_count"),
                                  ("logical_storage_bytes", "peak_logical_storage_bytes",
                                   "limit_logical_storage_bytes")):
            if any(type(value.get(key)) is not int or value[key] < 0
                   for key in (live, peak, limit)) or not value[live] <= value[peak] <= value[limit]:
                raise ValueError("invalid logical GPU storage accounting")
    final = values[-1]
    if final["live_count"] or final["logical_storage_bytes"]:
        raise ValueError("logical GPU storage retained after close")
    peak = max(value["peak_logical_storage_bytes"] for value in values)
    return {"checkpoints": values, "peak_bytes": peak, "accepted_reference_ceiling": ceiling,
            "status": "within_reference_ceiling" if peak <= ceiling else "over_reference_ceiling",
            "coverage": "GL buffers and RGBA8 textures; excludes shader, driver and framebuffer"}


def residency(text, sampled, ceiling):
    intervals = measurement.records(text, "PD5_CAPACITY_PHASE ")
    if [row.get("label") for row in intervals] != list(PHASES):
        raise ValueError("memory residency phase intervals missing or reordered")
    phases = {}
    for interval in intervals:
        begin, end, frequency = (interval.get(key) for key in
                                 ("begin_qpc", "end_qpc", "frequency"))
        if (any(type(value) is not int or value <= 0 for value in (begin, end, frequency))
                or end <= begin):
            raise ValueError("invalid memory phase QPC interval")
        selected = [row for row in sampled
                    if row.get("pid") == interval["pid"] and row.get("qpc_frequency") == frequency
                    and begin <= row.get("qpc", -1) <= end]
        totals = []
        for row in selected:
            values = [row.get("bytes", {}).get(key) for key in ("Dedicated Usage", "Shared Usage")]
            if all(type(value) is int and value >= 0 for value in values):
                totals.append(sum(values))
        phases[interval["label"]] = {"interval": interval, "sample_count": len(selected),
                                      "valid_count": len(totals), "bytes": totals}
    # A same-process empty renderer is the baseline; report context/driver startup separately.
    # Missing process counter instances remain unknown, never a zero-memory observation.
    baseline = phases["empty-device"]["bytes"]
    active = phases["active"]["bytes"]
    delta = max(0, max(active) - min(baseline)) if baseline and active else None
    lower = phases["empty-device"]["interval"]["end_qpc"]
    upper = phases["after-close"]["interval"]["begin_qpc"]
    pid = intervals[0]["pid"]
    sampled_window = [row for row in sampled if row.get("pid") == pid
                      and row.get("qpc_frequency") == intervals[0]["frequency"]
                      and lower <= row.get("qpc", -1) <= upper]
    window_bytes = []
    for row in sampled_window:
        values = [row.get("bytes", {}).get(key) for key in ("Dedicated Usage", "Shared Usage")]
        if all(type(value) is int and value >= 0 for value in values):
            window_bytes.append(sum(values))
    peak_delta = max(0, max(window_bytes) - min(baseline)) if baseline and window_bytes else None
    times = [lower, *(row["qpc"] for row in sampled_window), upper]
    maximum_gap = max((right - left for left, right in zip(times, times[1:])), default=0)
    covered = (bool(window_bytes) and len(window_bytes) == len(sampled_window)
               and maximum_gap <= intervals[0]["frequency"])
    return {"phases": phases, "active_delta_bytes": delta, "sampled_peak_delta_bytes": peak_delta,
            "load_through_close_samples": len(sampled_window), "maximum_gap_qpc": maximum_gap,
            "sampled_peak_coverage_complete": covered, "reference_ceiling": ceiling,
            "baseline": "same process, initialized empty renderer before host load",
            "status": "over_reference_ceiling" if peak_delta is not None and peak_delta > ceiling else
                      "unavailable" if peak_delta is None or delta is None or not covered else
                      "within_sampled_reference_ceiling",
            "coverage": "sampled process Dedicated+Shared GPU usage; includes driver residency, "
                        "not a sum with logical GL bytes; unknown samples are retained"}


def collect(text, profile, *, device=False, gpu_rows=(), residency_required=True):
    document = memory_domains.collect(text)
    checkpoints = {row["label"]: row for row in document["checkpoints"]}
    required = ("before-load", "after-content-prepare", "after-host-load-prepare", "active",
                "after-close", "after-fixture-release")
    if not set(required).issubset(checkpoints):
        raise ValueError("capacity prepare/active/close/release lifecycle incomplete")
    # Necessary release/subset rejection only; this is not a complete ownership graph.
    semantic = semantic_memory.analyze_workflow(text, profile)
    comparisons = {label: memory_attribution.compare(memory_attribution.attribute(text, label),
                                                      profile) for label in required}
    prepared = checkpoints["after-host-load-prepare"]
    active_product = checkpoints["active"]["product"]
    peaks = {row["domain"]: row["peak_requested_cpp_bytes"] for row in prepared["aggregates"]}
    return {"schema": "rpa.execution.capacity-memory-evidence.v1", "profile": profile["id"],
            "comparisons": comparisons, "origin_peak_rating": memory_domains.rate(document, profile),
            "semantic_workflow": semantic,
            "prepare": {"origin_peaks_bytes": peaks,
                         "product_concurrent_peak_bytes": prepared["product"]["peak_requested_cpp_bytes"],
                         "interpretation": "fresh-process cumulative simultaneous peaks through load; "
                                           "no subtraction of sticky peaks or summing per-domain peaks"},
            "observer": {"header_bytes_per_live_allocation": active_product["observer_header_size"],
                         "active_header_bytes": active_product["observer_header_bytes"],
                         "static_bytes": active_product["observer_static_bytes"],
                         "checkpoint_stack_bytes": active_product["observer_checkpoint_stack_bytes"],
                         "cost_not_subtracted_from_timings": True},
            "gpu_logical": gpu_storage(text, profile["gpu_backend_allocation_ceiling"])
                           if device else {"status": "not_applicable"},
            "gpu_residency": residency(text, gpu_rows, profile["gpu_driver_delta_ceiling"])
                             if device and residency_required else
                             {"status": "not_collected" if device else "not_applicable"},
            "formal_acceptance": False}


def run(args):
    root, output = args.source.resolve(), args.output.resolve()
    source = clean_source(root)
    metadata = read(args.build_binding)
    build_binding.verify(metadata, source, args.mode)
    probe = verify_artifact(metadata["binary"])
    expected = "cuexis_execution_device_probe.exe" if args.device else "cuexis_pd_host_probe.exe"
    if probe.name != expected or metadata["config"] != "Release":
        raise ValueError("capacity backend or Release configuration mismatch")
    rows = args.entities if args.rows is None else args.rows
    if not 1 <= rows <= args.entities or (args.entities == 40000 and rows != 4096):
        raise ValueError("capacity content must match the declared entity/row profile")
    if args.hold_ms and (args.mode != "memory" or not 500 <= args.hold_ms <= 5000):
        raise ValueError("phase holds are limited to 500..5000 ms in separate memory builds")
    formal = args.purpose == "sealed-memory-evidence"
    if not formal and args.entities == 40000:
        raise ValueError("P8 cannot be used as an unclaimed observer qualification workload")
    if formal:
        if not args.seal or args.mode != "memory":
            raise ValueError("formal memory evidence requires its admitted seal and memory build")
        preparation = read_sealed(args.seal)
        if len(preparation["units"]) != 1 or preparation["source_binding"] != source:
            raise ValueError("capacity seal must bind one unit and the exact current source")
        unit = preparation["units"][0]
        if (unit.get("workload") != ("p8" if args.entities == 40000 else "memory") or
                unit.get("entities") != args.entities or unit.get("rows") != rows or
                unit.get("reuse") != args.reuse or
                unit.get("backend", "cpu") != ("device" if args.device else "cpu") or
                unit.get("measurement_mode") != "memory" or
                preparation["binary_bindings"][unit["candidate_binary"]] != metadata["binary"]):
            raise ValueError("capacity invocation differs from the sealed workload")
    output.mkdir(parents=True, exist_ok=False)
    environment = process_environment()
    environment.update(CUEXIS_PD5_ENTITIES=str(args.entities), CUEXIS_PD5_REQUIREMENTS=str(rows),
                       CUEXIS_PD5_REUSE=args.reuse, CUEXIS_PD5_CAPSULE_REVISION="3",
                       CUEXIS_PD5_SMOKE="1", SDL_WINDOW_ACTIVATE_WHEN_SHOWN="0",
                       SDL_WINDOW_ACTIVATE_WHEN_RAISED="0")
    if args.hold_ms:
        environment["CUEXIS_PD5_MEMORY_HOLD_MS"] = str(args.hold_ms)
    if args.device and args.mode == "memory":
        if formal and not args.hold_ms:
            raise ValueError("device memory evidence requires bounded phase holds")
        if args.hold_ms:
            environment.update(CUEXIS_PROCESS_GPU_MEMORY_LOG=str(output / "gpu-residency.jsonl"),
                               CUEXIS_ALLOCATION_OBSERVER_PROCESS="1")
    declaration = {"schema": "rpa.execution.capacity-invocation.v1", "formal_sampling": formal,
                   "purpose": args.purpose, "source_sha": source["sha"],
                   "build_binding": artifact(args.build_binding), "device": args.device,
                   "entities": args.entities, "rows": rows, "reuse": args.reuse, "mode": args.mode,
                   "phase_hold_ms": args.hold_ms, "processes": 1, "timeout_seconds": 300,
                   "stop": "one invocation only; nonzero, identity or observer failure is retained"}
    write_once(output / "declaration.json", declaration)
    if formal:
        write_once(args.seal.with_suffix(".running.json"), {
            "schema": "rpa.execution.capacity-running.v1", "output": str(output),
            "seal": artifact(args.seal), "declaration": artifact(output / "declaration.json")})
    command = [str(probe), TEST_CASE, "--reporter", "compact", "--rng-seed", "1"]
    started = time.monotonic()
    try:
        if formal:
            proof = read(verify_artifact(unit["environment_binding"]))
            before = proof["cpu_environment"]
            verify_cpu(before)
        else:
            before = capture_cpu()
            before["foreground_policy"] = "fixed-existing-foreground"
        write_once(output / "environment-before.json", before)
        monitor = RuntimeMonitor(before["foreground_pid"], timeout=305)
        with (output / "stdout.log").open("xb") as log, monitor:
            operation_started = time.monotonic()
            result = process_control.run(command, process_control.affinity(), cwd=root,
                                         env=environment, output=log, timeout=300)
            elapsed = time.monotonic() - operation_started
        monitored = monitor.result()
        write_once(output / "runtime-environment.json", monitored)
        after = verify_cpu(before)
        after["foreground_policy"] = before["foreground_policy"]
        write_once(output / "environment-after.json", after)
        write_once(output / "process.json", {"exit_code": result.returncode, "elapsed_seconds": elapsed,
                                              "command": command, "log": artifact(output / "stdout.log")})
        if result.returncode:
            raise ValueError("capacity process failed; no replacement")
        if not monitored["passed"]:
            raise ValueError("capacity environment changed; no replacement")
        text = (output / "stdout.log").read_text(encoding="utf-8", errors="strict")
        identity = capacity(text, args.entities, rows, args.reuse, args.device)
        summary = {"identity": identity, "elapsed_seconds": elapsed,
                   "semantic_identity": {key: identity[key] for key in SEMANTIC_FIELDS}}
        if args.device:
            device = measurement.records(text, "PD5_CAPACITY_DEVICE ")
            if len(device) != 1 or not all(device[0].get(key) for key in ("vendor", "renderer", "version")):
                raise ValueError("actual device context identity missing")
            summary["device"] = device[0]
        if args.mode == "memory":
            budget = read(root / "tools/verification/execution_acceptance/budget-proposal.v1.json")
            profile = next(row for row in budget["profiles"]
                           if row["id"] == f"r4-reference-n{args.entities}-v1")
            gpu_path = output / "gpu-residency.jsonl"
            gpu_rows = [json.loads(line) for line in gpu_path.read_text().splitlines()] if gpu_path.exists() else []
            summary["memory"] = collect(text, profile, device=args.device, gpu_rows=gpu_rows,
                                        residency_required=bool(args.hold_ms))
        if clean_source(root) != source:
            raise ValueError("source changed during capacity invocation")
        write_once(output / "summary.json", summary)
        print("CAPACITY " + str(output / "summary.json"), flush=True)
    except Exception as error:
        write_once(output / "stopped.json", {"error": str(error), "formal_sampling": formal,
                                             "elapsed_seconds": time.monotonic() - started})
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for field in ("source", "build-binding", "output"):
        parser.add_argument("--" + field, type=Path, required=True)
    parser.add_argument("--mode", choices=("normal", "memory"), required=True)
    parser.add_argument("--entities", type=int, choices=(64, 2048, 40000), required=True)
    parser.add_argument("--rows", type=int)
    parser.add_argument("--reuse", choices=("low", "high"), required=True)
    parser.add_argument("--device", action="store_true")
    parser.add_argument("--hold-ms", type=int, default=0)
    parser.add_argument("--purpose", choices=("observer-qualification", "sealed-memory-evidence"),
                        default="observer-qualification")
    parser.add_argument("--seal", type=Path)
    run(parser.parse_args())


if __name__ == "__main__":
    main()

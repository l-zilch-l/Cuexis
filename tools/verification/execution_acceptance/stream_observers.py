"""Fixed independent P3 observer checks; never substitutes instrumented time for normal time."""
import argparse
import copy
from pathlib import Path
import time

from tools.verification.playback_performance import measurement, observer, process_control
from . import build_binding, prepare_stream
from .binding import artifact, clean_source, read, verify_artifact, write_once
from .environment import capture_cpu, process_environment, verify_cpu, verify_native, verify_warmup
from .runtime_monitor import RuntimeMonitor

KINDS = ("normal", "gpu", "stage", "memory")
STAGES = {"judgement": "d2", "projection": "d3", "snapshot": "materialize", "queue": "queue"}


def signature(parsed):
    identity = copy.deepcopy(parsed["identity"])
    for key in ("allocator", "metrics_compile_enabled", "gpu_timer", "payload_peak_count",
                "payload_peak_bytes", "journal_peak_bytes"):
        identity.pop(key, None)
    identity["counts"].pop("gpu_render", None)
    return identity


def summarize(rows):
    if len(rows) != 12 or [(row["pair"], row["kind"]) for row in rows] != [
            (pair, kind) for pair in range(3) for kind in KINDS]:
        raise ValueError("all twelve fixed observer processes are required")
    expected = signature(rows[0]["parsed"])
    if any(signature(row["parsed"]) != expected for row in rows):
        raise ValueError("observer changed the workload, digest, counts or result")
    overhead = {}
    for kind in KINDS[1:]:
        overhead[kind] = []
        for pair in range(3):
            normal = next(row for row in rows if row["pair"] == pair and row["kind"] == "normal")
            observed = next(row for row in rows if row["pair"] == pair and row["kind"] == kind)
            # The sleep-paced wall duration is disclosed separately. Endpoint costs measure
            # perturbation without hiding it under the fixed 240 Hz pacing interval.
            ratios = {metric: observed["parsed"]["statistics"][metric]["median_us"] /
                              normal["parsed"]["statistics"][metric]["median_us"]
                      for metric in ("request_to_ready", "request_to_present_return", "cpu_submit")}
            overhead[kind].append({"pair": pair, "endpoint_median_ratios": ratios,
                                   "process_wall_ratio": observed["elapsed_seconds"] / normal["elapsed_seconds"]})
    # The accepted four-stage exception requires a compile-time switch, identical workload,
    # coverage and disclosed overhead. The 15% repeatability limit is not an observer limit.
    # GPU time has no corresponding exception or accepted numerical perturbation rule here.
    admitted = {"stage": True, "gpu": False}
    gates = []
    for row in rows:
        if row["kind"] == "gpu":
            samples = [sample["elapsed_us"] for sample in row["parsed"]["samples"]
                       if sample["metric"] == "gpu_render"]
            if len(samples) != prepare_stream.FRAMES:
                raise ValueError("GPU query coverage or final drain incomplete")
            gates.append({"pair": row["pair"], "metric": "gpu_render",
                          **measurement.budget("gpu_render", samples,
                                               observer_validated=admitted["gpu"])})
        if row["kind"] == "stage":
            for stage, metric in STAGES.items():
                values = row["stages"][stage]["inclusive_us"]
                gates.append({"pair": row["pair"], "metric": metric,
                              **measurement.budget(metric, values,
                                                   observer_validated=admitted["stage"])})
    return {"schema": "rpa.execution.stream-observers.v1", "processes": len(rows),
            "exact_output_equal": True, "overhead": overhead, "observer_admitted": admitted,
            "observer_policy": "stage attribution uses the accepted 2026-11-02 switchable "
                               "four-stage exception with exact outputs, coverage and overhead disclosure; "
                               "GPU remains observer_unvalidated; no invented perturbation threshold "
                               "or conversion into normal timing",
            "stage_and_gpu_gates": gates, "formal_acceptance": False,
            "scope": "fixed observer qualification processes; not the normal three-window group"}


def run(args):
    root, output = args.source.resolve(), args.output.resolve()
    source = clean_source(root)
    modes = {"normal": "normal", "gpu": "normal", "stage": "stage-timing", "memory": "memory"}
    paths = {"normal": args.normal_binding, "gpu": args.normal_binding,
             "stage": args.stage_binding, "memory": args.memory_binding}
    metadata = {kind: read(path) for kind, path in paths.items()}
    warm = read(args.warmup_binding)
    build_binding.verify(warm, source, "normal")
    warmup = verify_artifact(warm["binary"])
    for kind in KINDS:
        build_binding.verify(metadata[kind], source, modes[kind])
        binary = verify_artifact(metadata[kind]["binary"])
        if binary.name != prepare_stream.PROBE_EXECUTABLE or metadata[kind]["config"] != "Release":
            raise ValueError("observer requires the fixed Release device stream binary")
        if metadata[kind]["toolchain_signature"] != warm["toolchain_signature"]:
            raise ValueError("observer toolchains differ")
    output.mkdir(parents=True, exist_ok=False)
    write_once(output / "declaration.json", {
        "schema": "rpa.execution.stream-observer-declaration.v1", "formal_sampling": False,
        "source_sha": source["sha"], "pairs": 3, "order": list(KINDS),
        "frames": prepare_stream.FRAMES, "warmup_frames": prepare_stream.WARMUP,
        "builds": {kind: artifact(path) for kind, path in paths.items()},
        "warmup": artifact(args.warmup_binding), "maximum_processes": 12,
        "stop": "first failed process/identity/environment/coverage check; no replacement"})
    before = capture_cpu()
    before["foreground_policy"] = "fixed-existing-foreground"
    rows, native = [], None
    try:
        for pair in range(3):
            for kind in KINDS:
                verify_cpu(before)
                folder = output / f"pair-{pair + 1}-{kind}"
                folder.mkdir()
                env = process_environment()
                env.update(prepare_stream.environment())
                env["CUEXIS_PD5_GPU_TIMER"] = "1" if kind == "gpu" else "0"
                if kind == "stage":
                    env["CUEXIS_PD_METRICS"] = str(folder / "stages")
                commands = (("warmup", [str(warmup), "PD machine CPU warmup", "--rng-seed", "1"]),
                            ("sample", [str(verify_artifact(metadata[kind]["binary"])),
                                        prepare_stream.PROBE_TEST, "--reporter", "compact", "--rng-seed", "1"]))
                elapsed = 0
                for phase, command in commands:
                    monitor = RuntimeMonitor(before["foreground_pid"], timeout=605)
                    with (folder / (phase + ".log")).open("xb") as log, monitor:
                        started = time.monotonic()
                        result = process_control.run(command, before["machine"]["parent_affinity"],
                                                     cwd=root, env=env, output=log, timeout=600)
                        elapsed = time.monotonic() - started
                    monitored = monitor.result()
                    write_once(folder / (phase + "-environment.json"), monitored)
                    write_once(folder / (phase + ".json"), {"exit_code": result.returncode,
                               "elapsed_seconds": elapsed, "command": command,
                               "log": artifact(folder / (phase + ".log"))})
                    if result.returncode or not monitored["passed"]:
                        raise ValueError("observer process or environment failed")
                    if phase == "warmup":
                        verify_warmup((folder / "warmup.log").read_text(encoding="utf-8"))
                text = (folder / "sample.log").read_text(encoding="utf-8")
                native = verify_native(text, before["machine"]["parent_affinity"], native)
                parsed = measurement.parse_probe(text, instrumented=kind == "stage",
                                                  memory_observer=kind == "memory")
                row = {"pair": pair, "kind": kind, "parsed": parsed, "elapsed_seconds": elapsed,
                       "log": artifact(folder / "sample.log")}
                if kind == "stage":
                    row["stages"] = observer.stages(sorted(folder.glob("stages.*.jsonl")),
                                                    {"judgement": prepare_stream.FRAMES,
                                                     "projection": prepare_stream.FRAMES,
                                                     "snapshot": prepare_stream.FRAMES, "queue": -1})
                write_once(folder / "parsed.json", row)
                rows.append(row)
                print(pair + 1, kind, "complete", flush=True)
        verify_cpu(before)
        if clean_source(root) != source:
            raise ValueError("source changed during observer qualification")
        write_once(output / "summary.json", summarize(rows))
    except Exception as error:
        write_once(output / "stopped.json", {"error": str(error),
                                             "completed": [(row["pair"], row["kind"]) for row in rows]})
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for field in ("source", "normal-binding", "stage-binding", "memory-binding", "warmup-binding", "output"):
        parser.add_argument("--" + field, type=Path, required=True)
    run(parser.parse_args())


if __name__ == "__main__":
    main()

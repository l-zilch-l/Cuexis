"""Bounded pilot, proofs and preparation for the P5 scripted control unit.

The control workload has no pre-PD counterpart and no presentation-matrix counts, so it declares its
own identity instead of borrowing the matrix one. Every count is derived here from the declared
cycle count; the pilot only proves that the registered control workload, identity and environment
are executable. No formal attempt is claimed.
"""
import argparse
from pathlib import Path

from tools.verification.playback_performance import measurement

from . import admission, build_binding
from .binding import artifact, clean_source, read, verify_artifact, write_once
from .environment import capture_cpu, verify_cpu, verify_native, verify_warmup
from .prepare_probe import TIMEOUT, WARMUP_TEST, child
from .runtime_monitor import POLICY

GATE = "p5-controls"
STABLE = GATE + "-release"
PROBE_TEST = "PD5 scripted host timing probe"
ENDPOINT = "first-present-after-control"
FIXTURE = "pd5-host-probe"
PROFILE = "controls-10-cycles-v1"
FRAMES = 512
WARMUP = 32
CYCLES = 10
RATE = 240
CONTROL_METRICS = ("pause", "resume", "reset_paused", "reset_running", "resize")
METRICS = [*CONTROL_METRICS, "load_host", "close"]


def counts(cycles=CYCLES):
    """The declared sample counts; the loop issues two pauses and two resumes per cycle."""
    return {"pause": 2 * cycles, "resume": 2 * cycles, "reset_paused": cycles,
            "reset_running": cycles, "resize": cycles, "load_host": 1, "close": 1}


def launch(probe, warmup, root, affinity):
    return {"command": [str(probe), PROBE_TEST],
            "warmup_command": [str(warmup), WARMUP_TEST, "--rng-seed", "1"],
            "cwd": str(root), "affinity": affinity, "warmup_seconds": 30,
            "timeout_seconds": TIMEOUT,
            "environment": {"CUEXIS_PD5_MODE": "controls", "CUEXIS_PD5_CYCLES": str(CYCLES),
                            "CUEXIS_PD5_RATE": str(RATE), "CUEXIS_PD5_FRAMES": str(FRAMES),
                            "CUEXIS_PD5_WARMUP": str(WARMUP), "CUEXIS_PD5_SMOKE": "0"}}


def identity(parsed):
    """The exact identity a formal process must reproduce; the counts are pre-declared."""
    value = parsed["identity"]
    if (value.get("mode") != "controls" or value.get("cycles") != CYCLES or
            value.get("rate") != RATE or value.get("frames") != FRAMES or
            value.get("warmup_frames") != WARMUP or value.get("audio") is not False or
            value.get("gpu") is not False or value.get("closed") is not True or
            value.get("replay_equal") is not True):
        raise ValueError("pilot identity differs from the registered control workload")
    if value.get("counts") != counts():
        raise ValueError("pilot counts differ from the pre-declared control counts")
    for metric in METRICS:
        measurement.positive(parsed["statistics"][metric]["median_us"])
    return {key: value[key] for key in
            ("mode", "frames", "warmup_frames", "cycles", "rate", "source_step_ticks",
             "source_tick_rate", "audio", "closed", "replay_equal", "counts")}


def run(args):
    root, output = args.source.resolve(), args.output.resolve()
    source = clean_source(root)
    candidate, warmup_metadata = read(args.build_binding), read(args.warmup_build_binding)
    for metadata in (candidate, warmup_metadata):
        build_binding.verify(metadata, source, "normal")
        if metadata["config"] != "Release":
            raise ValueError("control registration requires the reference Release configuration")
    if candidate["toolchain_signature"] != warmup_metadata["toolchain_signature"]:
        raise ValueError("probe and machine-warmup binaries use different toolchains")
    probe = Path(verify_artifact(candidate["binary"]))
    warmup = Path(verify_artifact(warmup_metadata["binary"]))
    if probe.name != "cuexis_pd_host_probe.exe" or warmup.name != "cuexis_pd_baseline_probe.exe":
        raise ValueError("control registration requires the host and machine-warmup executables")
    environment = capture_cpu()
    environment["foreground_policy"] = "fixed-existing-foreground"
    affinity = environment["machine"]["parent_affinity"]
    output.mkdir(parents=True, exist_ok=False)
    write_once(output / "pilot-declaration.json", {
        "schema": "rpa.execution.controls-pilot-declaration.v1", "formal_sampling": False,
        "unit": GATE, "sides": ["B"], "processes": 1, "cycles": CYCLES, "rate": RATE,
        "declared_counts": counts(), "machine_warmup_seconds_per_process": 30,
        "timeout_seconds": TIMEOUT, "runtime_monitor_policy": POLICY, "source_sha": source["sha"],
        "affinity": affinity, "cpu_environment": environment,
        "stop": "Any build/environment/identity/count failure; no replacement and no rerun"})
    state = {}
    try:
        spec = launch(probe, warmup, root, affinity)
        warm_log, warm_raw = child({"id": GATE}, "warmup", spec, environment, output)
        verify_warmup(warm_log.read_text(encoding="utf-8", errors="strict"))
        log, raw = child({"id": GATE}, "sample", spec, environment, output)
        text = log.read_text(encoding="utf-8", errors="strict")
        state["native"] = verify_native(text, affinity, state.get("native"))
        verify_cpu(environment)
        parsed = measurement.parse_probe(text, exit_code=0)
        expected = identity(parsed)
        parsed_path = output / "pilot-parsed.json"
        write_once(parsed_path, parsed)
        raw.extend((artifact(parsed_path), *warm_raw))
        proofs = {}
        for kind in ("semantic", "pilot", "environment"):
            path = output / (kind + "-proof.json")
            write_once(path, {
                "schema": "rpa.execution." + kind + ".v1", "passed": True, "unit": GATE,
                "source_sha": source["sha"], "endpoint": ENDPOINT,
                "candidate_binary_sha256": candidate["binary"]["sha256"],
                "observed_medians_us": {metric: parsed["statistics"][metric]["median_us"]
                                        for metric in METRICS},
                "raw_bindings": raw, "cpu_environment": environment,
                "runtime_monitor_policy": POLICY,
                "scope": "scripted control latency only; Reset is a teardown and reload, Resize "
                         "reaches the renderer only, and no GPU timer, audio device, Seek or "
                         "A-side comparison is admitted by this proof"})
            proofs[kind + "_binding"] = artifact(path)
        if clean_source(root) != source:
            raise ValueError("source changed during the control pilot")
        record = {
            "id": GATE, "stable_protocol_id": STABLE, "workload": "steady", "gate": GATE,
            "config": "Release", "comparison": "B-only", "candidate_binary": "B",
            "cold_process": False, "requests_per_process": counts()["pause"], "fixture": FIXTURE,
            "profile": PROFILE, "completion_endpoint": ENDPOINT, "required_metrics": METRICS,
            "adapter": "controls", "measurement_mode": "normal",
            "runtime_monitor_policy": POLICY, "expected_identity": expected,
            "launch": {"B": spec}, **proofs}
        preparation = {
            "schema": "rpa.execution.preparation.v1", "formal_registration": False,
            "source_binding": source,
            "acceptance_binding": artifact(args.acceptance),
            "authorization_binding": artifact(args.authorization),
            "budget_binding": artifact(root / "tools/verification/execution_acceptance/"
                                              "budget-proposal.v1.json"),
            "tool_bindings": {str(path.relative_to(root)): artifact(path) for path in
                              [*root.joinpath("tools/verification/execution_acceptance").glob("*.py"),
                               *root.joinpath("tools/verification/playback_performance").glob("*.py"),
                               root / "tests/pd/formal_baseline.py",
                               root / "tests/pd/system_runtime_support.py"]},
            "fixture_bindings": {
                FIXTURE: artifact(root / "tests/pd/pd5_host_probe.cpp"),
                "probe-environment": artifact(root / "tests/pd/measurement_environment.hpp")},
            "binary_bindings": {"B": candidate["binary"], "W": warmup_metadata["binary"]},
            "binary_build_bindings": {"B": artifact(args.build_binding),
                                      "W": artifact(args.warmup_build_binding)},
            "windows": 3, "processes_per_window": 3, "gap_seconds": 600,
            "orders": admission.ORDERS, "regression_limit": 1.10, "noise_limit": .15,
            "units": [record],
            "invalid_process_rules": ["No replacement: timeout/nonzero/missing/identity/"
                                      "environment failure stops this registration; raw files remain."],
            "attempt_ledger": read(args.authorization)["attempt_ledger"]}
        admission.validate_preparation(preparation)
        write_once(output / "preparation.json", preparation)
        print("PREPARATION " + str(output / "preparation.json"), flush=True)
    except Exception as error:
        write_once(output / "stopped.json", {"error": str(error), "formal_sampling": False,
                                             "formal_attempts_consumed": 0})
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for field in ("source", "build-binding", "warmup-build-binding", "output"):
        parser.add_argument("--" + field, type=Path, required=True)
    r4 = Path("docs/stage_reports/stages/stage-07/05-rpa-new/r4")
    parser.add_argument("--acceptance", type=Path, default=r4 / "2026-11-05-o1-budget-acceptance.json")
    parser.add_argument("--authorization", type=Path,
                        default=r4 / "2026-11-05-execution-authorization.json")
    args = parser.parse_args()
    args.acceptance = args.acceptance if args.acceptance.is_absolute() else args.source / args.acceptance
    args.authorization = (args.authorization if args.authorization.is_absolute()
                          else args.source / args.authorization)
    run(args)


if __name__ == "__main__":
    main()

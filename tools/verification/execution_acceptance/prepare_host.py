"""Bounded pilot, proofs and preparation for the B-only common-host delivery unit.

The pre-PD counterpart measures a receipt-ACK boundary and reports a different frame digest, so
this unit is B-only: A stays N/A instead of being filled with zero. The pilot fixes the
deterministic output digest and the environment; the formal processes must reproduce both.
"""
import argparse
from pathlib import Path

from tools.verification.playback_performance import measurement

from . import admission, build_binding
from .binding import artifact, clean_source, read, verify_artifact, write_once
from .environment import capture_cpu, verify_cpu, verify_native, verify_warmup
from .prepare_probe import TIMEOUT, child
from .runtime_monitor import POLICY

GATE = "p0-host-common"
STABLE = GATE + "-release"
FRAMES = 1024
NATIVE_WARMUP = 32
ENDPOINT = "present-return"
FIXTURE = "host-delivery"
PROFILE = "common-host-1024-v1"
PROBE_TEST = "PD host delivery and receipt acknowledgement baseline"
WARMUP_TEST = "PD machine CPU warmup"
SOURCE = "assets/projects/stage3_project"
VIEWPORT = [1280, 720]
OBJECTS = 2
METRICS = ["request_to_ready_us", "ready_to_present_return_us", "request_to_present_return_us"]


def launch(probe, warmup, root, affinity):
    # 1024 samples are the harness's own finite extended batch: the 512 default cannot reach the
    # registered p99 (n >= 1000) and would leave the accepted budget unjudged.
    return {"command": [str(probe), PROBE_TEST],
            "warmup_command": [str(warmup), WARMUP_TEST, "--rng-seed", "1"],
            "cwd": str(root), "affinity": affinity, "warmup_seconds": 30,
            "timeout_seconds": 120,
            "environment": {"CUEXIS_PD_HOST_DIAGNOSTIC_1024": "1"}}


def identity(parsed):
    """The exact output identity a formal process must reproduce; the digest is deterministic."""
    value = parsed["identity"]
    if (value.get("source") != SOURCE or value.get("viewport") != VIEWPORT or
            value.get("objects") != OBJECTS or value.get("warmup") != NATIVE_WARMUP or
            value.get("measured") != FRAMES or value.get("audio") is not False or
            value.get("gpu") is not False or value.get("closed") is not True):
        raise ValueError("pilot identity differs from the registered common-host workload")
    if len(parsed["samples"]) != FRAMES:
        raise ValueError("pilot sample count differs from the registered batch")
    for metric in METRICS:
        measurement.positive(parsed["statistics"][metric]["median_us"])
    return {key: value[key] for key in
            ("digest", "source", "viewport", "objects", "warmup", "measured", "audio", "gpu")}


def run(args):
    root, output = args.source.resolve(), args.output.resolve()
    source = clean_source(root)
    candidate, warmup_metadata = read(args.build_binding), read(args.warmup_build_binding)
    for metadata in (candidate, warmup_metadata):
        build_binding.verify(metadata, source, "normal")
        if metadata["config"] != "Release":
            raise ValueError("common-host registration requires the reference Release configuration")
    if candidate["toolchain_signature"] != warmup_metadata["toolchain_signature"]:
        raise ValueError("probe and machine-warmup binaries use different toolchains")
    probe = Path(verify_artifact(candidate["binary"]))
    warmup = Path(verify_artifact(warmup_metadata["binary"]))
    if probe.name != "cuexis_pd_host_probe.exe" or warmup.name != "cuexis_pd_baseline_probe.exe":
        raise ValueError("common-host registration requires the host and machine-warmup executables")
    environment = capture_cpu()
    environment["foreground_policy"] = "fixed-existing-foreground"
    affinity = environment["machine"]["parent_affinity"]
    output.mkdir(parents=True, exist_ok=False)
    write_once(output / "pilot-declaration.json", {
        "schema": "rpa.execution.host-pilot-declaration.v1", "formal_sampling": False,
        "unit": GATE, "sides": ["B"], "processes": 1, "measured_frames": FRAMES,
        "native_warmup_frames": NATIVE_WARMUP, "machine_warmup_seconds_per_process": 30,
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
        parsed = measurement.parse_host(text, exit_code=0)
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
                "scope": "common host only; no GPU, audio, device, Seek or A-side comparison is "
                         "admitted by this proof"})
            proofs[kind + "_binding"] = artifact(path)
        if clean_source(root) != source:
            raise ValueError("source changed during the host pilot")
        record = {
            "id": GATE, "stable_protocol_id": STABLE, "workload": "steady", "gate": GATE,
            "config": "Release", "comparison": "B-only", "candidate_binary": "B",
            "cold_process": False, "requests_per_process": FRAMES, "fixture": FIXTURE,
            "profile": PROFILE, "completion_endpoint": ENDPOINT, "required_metrics": METRICS,
            "adapter": "host-common", "measurement_mode": "normal",
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
                FIXTURE: artifact(root / "tests/pd/host_delivery_probe.cpp"),
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

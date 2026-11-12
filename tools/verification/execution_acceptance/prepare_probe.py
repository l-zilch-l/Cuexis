"""Bounded pilot, proofs and preparation for admitted presentation-matrix probe units.

No formal attempt is claimed here. Every count comes from `probe_units`; the pilot only proves
that the registered workload, identity, environment and observer envelope are executable.
"""
import argparse
import os
from pathlib import Path
import time

from tools.verification.playback_performance import measurement, process_control

from . import admission, build_binding, probe_units
from .binding import artifact, clean_source, read, sha256, verify_artifact, write_once
from .environment import capture_cpu, verify_cpu, verify_native, verify_warmup
from .runtime_monitor import POLICY, RuntimeMonitor

WARMUP_TEST = "PD machine CPU warmup"
TIMEOUT = 600
BASELINE_EXECUTABLE = "cuexis_pd_baseline_probe.exe"
CPU_EXECUTABLE = "cuexis_pd_host_probe.exe"
DEVICE_EXECUTABLE = "cuexis_execution_device_probe.exe"
CPU_TEST = probe_units.PROBE_PREFIX
DEVICE_TEST = "PD5 renderer and capture workload matrix"
CPU_SCOPE = ("CPU presentation matrix only; no GPU, audio, device, Seek or absolute observer "
             "gate is admitted by this proof")
DEVICE_SCOPE = ("device renderer/capture matrix only; no audio, Seek or absolute observer gate "
                "is admitted by this proof")


def required_test_case(executable, device):
    """The matrix test the selected executable compiles; the two names are not interchangeable."""
    expected = DEVICE_EXECUTABLE if device else CPU_EXECUTABLE
    if Path(str(executable)).name != expected:
        raise ValueError("probe backend requires " + expected)
    return DEVICE_TEST if device else CPU_TEST


def expected_identity(unit, device=False):
    """The pre-declared identity for the selected backend, including the device GPU flag."""
    return probe_units.expected_fields(unit, device=device)


def verify_identity(unit, parsed, device=False):
    """Reject a pilot identity the selected backend could not have produced.

    The device expectation declares ``gpu`` true, so a probe that reports false fails here before
    any proof is written, even though the pre-declaration and the observation share a key set.
    """
    expected = expected_identity(unit, device=device)
    identity = parsed.get("identity") or {}
    if identity.get("counts") != expected["counts"]:
        raise ValueError("pilot counters differ from the pre-declared unit counts")
    if any(identity.get(key) != value for key, value in expected.items() if key != "counts"):
        raise ValueError("pilot identity differs from the pre-declared unit identity")
    return expected


def launch(probe, warmup, root, affinity, unit, test_case=CPU_TEST):
    return {"command": [str(probe), test_case],
            "warmup_command": [str(warmup), WARMUP_TEST, "--rng-seed", "1"],
            "cwd": str(root), "affinity": affinity, "warmup_seconds": 30,
            "timeout_seconds": TIMEOUT, "environment": probe_units.environment(unit)}


def child(unit, phase, spec, environment, output):
    """One monitored pilot process; the returned record binds every raw byte it produced."""
    verify_cpu(environment)
    command = spec["command"] if phase == "sample" else spec["warmup_command"]
    env = {key: value for key, value in os.environ.items()
           if not key.startswith(("CUEXIS_PD_", "CUEXIS_PD5_", "CUEXIS_REDESIGN_"))}
    env.update(spec["environment"])
    log = output / (unit["id"] + "-" + phase + ".log")
    timeout = spec.get("timeout_seconds", TIMEOUT)
    monitor = RuntimeMonitor(environment["foreground_pid"], timeout=timeout + 5)
    started = time.monotonic()
    with log.open("xb") as stream, monitor:
        result = process_control.run(command, spec["affinity"], cwd=spec["cwd"], env=env,
                                     output=stream, timeout=timeout)
    monitored = monitor.result()
    environment_path = output / (unit["id"] + "-" + phase + "-environment.json")
    write_once(environment_path, monitored)
    record = {"command": command, "exit_code": result.returncode, "phase": phase,
              "elapsed_seconds": time.monotonic() - started, "log": artifact(log)}
    record_path = output / (unit["id"] + "-" + phase + ".json")
    write_once(record_path, record)
    raw = [artifact(environment_path), artifact(record_path), artifact(log)]
    if result.returncode or not monitored["passed"]:
        raise ValueError("pilot process or environment failed; no replacement")
    return log, raw


def pilot_unit(unit, probe, warmup, root, affinity, environment, output, state,
               *, test_case=CPU_TEST, device=False):
    spec = launch(probe, warmup, root, affinity, unit, test_case)
    warm_log, warm_raw = child(unit, "warmup", spec, environment, output)
    verify_warmup(warm_log.read_text(encoding="utf-8", errors="strict"))
    log, raw = child(unit, "sample", spec, environment, output)
    text = log.read_text(encoding="utf-8", errors="strict")
    state["native"] = verify_native(text, affinity, state.get("native"))
    verify_cpu(environment)
    parsed = measurement.parse_probe(text, exit_code=0)
    verify_identity(unit, parsed, device=device)
    for metric in unit["metrics"]:
        measurement.positive(parsed["statistics"][metric]["median_us"])
    parsed_path = output / (unit["id"] + "-parsed.json")
    write_once(parsed_path, parsed)
    raw.extend((artifact(parsed_path), *warm_raw))
    print(unit["id"], "pilot complete", flush=True)
    return parsed, raw


def unit_record(unit, probe, warmup, root, affinity, proofs, *, test_case=CPU_TEST,
                device=False):
    stable = unit["gate"] + "-release"
    return {"id": unit["id"], "stable_protocol_id": stable, "workload": "steady",
            "gate": unit["gate"], "config": "Release", "comparison": "B-only",
            "candidate_binary": "B", "cold_process": False,
            "requests_per_process": probe_units.FRAMES, "fixture": probe_units.HARNESS,
            "profile": unit["profile"], "completion_endpoint": probe_units.ENDPOINT,
            "required_metrics": list(unit["metrics"]), "adapter": "probe",
            "measurement_mode": "normal", "runtime_monitor_policy": POLICY,
            "expected_identity": expected_identity(unit, device=device),
            "absolute_gate_metrics": list(unit.get("absolute_gate_metrics", [])),
            "launch": {"B": launch(probe, warmup, root, affinity, unit, test_case)},
            **proofs}


def retry_bindings(units, ledger, remedy, output):
    """One disposition per unit when a verified remedy exists; never invent one."""
    bindings = {}
    for unit in units:
        stable = unit["gate"] + "-release"
        record = admission.retry_disposition(ledger, stable, remedy)
        path = output / (unit["id"] + "-retry-disposition.json")
        write_once(path, record)
        bindings[unit["id"]] = artifact(path)
    return bindings


def run(args):
    root, output = args.source.resolve(), args.output.resolve()
    device = args.device
    acceptance = args.acceptance if args.acceptance.is_absolute() else root / args.acceptance
    authorization = args.authorization if args.authorization.is_absolute() else root / args.authorization
    source = clean_source(root)
    candidate = read(args.build_binding)
    warmup_metadata = read(args.warmup_build_binding)
    for metadata in (candidate, warmup_metadata):
        build_binding.verify(metadata, source, "normal")
        if metadata["config"] != "Release":
            raise ValueError("probe registration requires the reference Release configuration")
    if candidate["toolchain_signature"] != warmup_metadata["toolchain_signature"]:
        raise ValueError("probe and machine-warmup binaries use different toolchains")
    units = [probe_units.validate(unit) for unit in probe_units.select(args.units)]
    for unit in units:
        if unit["id"] != unit["gate"]:
            raise ValueError("probe unit and gate identity differ")
        probe_units.require_mode(unit, device)
    probe = Path(verify_artifact(candidate["binary"]))
    warmup = Path(verify_artifact(warmup_metadata["binary"]))
    expected_probe = DEVICE_EXECUTABLE if device else CPU_EXECUTABLE
    if probe.name != expected_probe or warmup.name != BASELINE_EXECUTABLE:
        if device:
            raise ValueError("device probe registration requires the device matrix and "
                             "machine-warmup executables")
        raise ValueError("probe registration requires the matrix and machine-warmup executables")
    test_case = required_test_case(probe, device)
    environment = capture_cpu()
    environment["foreground_policy"] = "fixed-existing-foreground"
    affinity = environment["machine"]["parent_affinity"]
    output.mkdir(parents=True, exist_ok=False)
    declaration = {
        "schema": "rpa.execution.probe-pilot-declaration.v1", "formal_sampling": False,
        "units": [unit["id"] for unit in units], "processes_per_unit": 1, "sides": ["B"],
        "frames_per_process": probe_units.FRAMES,
        "warmup_frames_per_process": probe_units.WARMUP_FRAMES,
        "machine_warmup_seconds_per_process": 30, "timeout_seconds": TIMEOUT,
        "runtime_monitor_policy": POLICY, "source_sha": source["sha"],
        "affinity": affinity, "cpu_environment": environment,
        "stop": "Any build/environment/semantic/count failure; no replacement, recount or rerun"}
    if device:
        declaration["backend"] = "device"
    write_once(output / "pilot-declaration.json", declaration)
    records, state = [], {}
    try:
        for unit in units:
            parsed, raw = pilot_unit(unit, probe, warmup, root, affinity, environment, output,
                                     state, test_case=test_case, device=device)
            proofs = {}
            for kind in ("semantic", "pilot", "environment"):
                path = output / (unit["id"] + "-" + kind + "-proof.json")
                write_once(path, {
                    "schema": "rpa.execution." + kind + ".v1", "passed": True,
                    "unit": unit["id"], "source_sha": source["sha"],
                    "endpoint": probe_units.ENDPOINT,
                    "candidate_binary_sha256": candidate["binary"]["sha256"],
                    "observed_medians_us": {metric: parsed["statistics"][metric]["median_us"]
                                            for metric in unit["metrics"]},
                    "raw_bindings": raw, "cpu_environment": environment,
                    "runtime_monitor_policy": POLICY,
                    "scope": DEVICE_SCOPE if device else CPU_SCOPE})
                proofs[kind + "_binding"] = artifact(path)
            records.append(unit_record(unit, probe, warmup, root, affinity, proofs,
                                       test_case=test_case, device=device))
        if args.retry_remedy:
            remedy = artifact(args.retry_remedy)
            ledger = read(authorization)["attempt_ledger"]
            bindings = retry_bindings(units, ledger, remedy, output)
            for record in records:
                record["retry_disposition_binding"] = bindings[record["id"]]
        if clean_source(root) != source:
            raise ValueError("source changed during the probe pilot")
        fixtures = {
            probe_units.HARNESS: artifact(root / "tests/pd/pd5_matrix_probe.cpp"),
            "rpa-execution-fixture": artifact(root / "tests/rpa/execution_fixture.hpp"),
            "probe-environment": artifact(root / "tests/pd/measurement_environment.hpp")}
        if device:
            fixtures.update({
                "execution-device-probe": artifact(root / "tests/pd/pd5_device_probe.cpp"),
                "device-execution-fixture": artifact(
                    root / "tests/player/execution_device_probe.cpp")})
        preparation = {
            "schema": "rpa.execution.preparation.v1", "formal_registration": False,
            "source_binding": source,
            "acceptance_binding": artifact(acceptance),
            "authorization_binding": artifact(authorization),
            "budget_binding": artifact(root / "tools/verification/execution_acceptance/"
                                              "budget-proposal.v1.json"),
            "tool_bindings": {str(path.relative_to(root)): artifact(path) for path in
                              [*root.joinpath("tools/verification/execution_acceptance").glob("*.py"),
                               *root.joinpath("tools/verification/playback_performance").glob("*.py"),
                               root / "tests/pd/formal_baseline.py",
                               root / "tests/pd/system_runtime_support.py"]},
            "fixture_bindings": fixtures,
            "binary_bindings": {"B": candidate["binary"], "W": warmup_metadata["binary"]},
            "binary_build_bindings": {"B": artifact(args.build_binding),
                                      "W": artifact(args.warmup_build_binding)},
            "windows": 3, "processes_per_window": 3, "gap_seconds": 600,
            "orders": admission.ORDERS, "regression_limit": 1.10, "noise_limit": .15,
            "units": records,
            "invalid_process_rules": ["No replacement: timeout/nonzero/missing/identity/"
                                      "environment failure stops this registration; raw files remain."]}
        if device:
            # validate_preparation forwards the document to registration.validate_draft, which
            # reads known fields only, so this backend marker is admitted and sealed with it.
            preparation["backend"] = "device"
        preparation["attempt_ledger"] = read(
            verify_artifact(preparation["authorization_binding"]))["attempt_ledger"]
        admission.validate_preparation(preparation)
        write_once(output / "preparation.json", preparation)
        print("PREPARATION " + str(output / "preparation.json"), flush=True)
    except Exception as error:
        write_once(output / "stopped.json", {"error": str(error), "formal_sampling": False,
                                             "formal_attempts_consumed": 0,
                                             "completed_units": [row["id"] for row in records]})
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for field in ("source", "build-binding", "warmup-build-binding", "output"):
        parser.add_argument("--" + field, type=Path, required=True)
    r4 = Path("docs/stage_reports/stages/stage-07/05-rpa-new/r4")
    parser.add_argument("--acceptance", type=Path, default=r4 / "2026-11-05-o1-budget-acceptance.json")
    parser.add_argument("--authorization", type=Path,
                        default=r4 / "2026-11-05-execution-authorization.json")
    parser.add_argument("--units", nargs="+", default=["all"])
    parser.add_argument("--device", action="store_true",
                        help="register the capture units against the GPU/OpenGL device probe")
    parser.add_argument("--retry-remedy", type=Path,
                        help="verified remedy artifact; enables one second registration per unit")
    run(parser.parse_args())


if __name__ == "__main__":
    main()

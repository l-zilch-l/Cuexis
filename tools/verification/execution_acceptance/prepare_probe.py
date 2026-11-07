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


def launch(probe, warmup, root, affinity, unit):
    return {"command": [str(probe), probe_units.PROBE_PREFIX],
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
    monitor = RuntimeMonitor(environment["foreground_pid"], timeout=TIMEOUT + 5)
    started = time.monotonic()
    with log.open("xb") as stream, monitor:
        result = process_control.run(command, spec["affinity"], cwd=spec["cwd"], env=env,
                                     output=stream, timeout=TIMEOUT)
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


def pilot_unit(unit, probe, warmup, root, affinity, environment, output, state):
    spec = launch(probe, warmup, root, affinity, unit)
    warm_log, warm_raw = child(unit, "warmup", spec, environment, output)
    verify_warmup(warm_log.read_text(encoding="utf-8", errors="strict"))
    log, raw = child(unit, "sample", spec, environment, output)
    text = log.read_text(encoding="utf-8", errors="strict")
    state["native"] = verify_native(text, affinity, state.get("native"))
    verify_cpu(environment)
    parsed = measurement.parse_probe(text, exit_code=0)
    expected = probe_units.expected_fields(unit)
    if parsed["identity"].get("counts") != expected["counts"]:
        raise ValueError("pilot counters differ from the pre-declared unit counts")
    if any(parsed["identity"].get(key) != value for key, value in expected.items() if key != "counts"):
        raise ValueError("pilot identity differs from the pre-declared unit identity")
    for metric in unit["metrics"]:
        measurement.positive(parsed["statistics"][metric]["median_us"])
    parsed_path = output / (unit["id"] + "-parsed.json")
    write_once(parsed_path, parsed)
    raw.extend((artifact(parsed_path), *warm_raw))
    print(unit["id"], "pilot complete", flush=True)
    return parsed, raw


def unit_record(unit, probe, warmup, root, affinity, proofs):
    stable = unit["gate"] + "-release"
    return {"id": unit["id"], "stable_protocol_id": stable, "workload": "steady",
            "gate": unit["gate"], "config": "Release", "comparison": "B-only",
            "candidate_binary": "B", "cold_process": False,
            "requests_per_process": probe_units.FRAMES, "fixture": probe_units.HARNESS,
            "profile": unit["profile"], "completion_endpoint": probe_units.ENDPOINT,
            "required_metrics": list(unit["metrics"]), "adapter": "probe",
            "measurement_mode": "normal", "runtime_monitor_policy": POLICY,
            "expected_identity": probe_units.expected_fields(unit),
            "absolute_gate_metrics": list(unit.get("absolute_gate_metrics", [])),
            "launch": {"B": launch(probe, warmup, root, affinity, unit)},
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
    probe = Path(verify_artifact(candidate["binary"]))
    warmup = Path(verify_artifact(warmup_metadata["binary"]))
    if probe.name != "cuexis_pd_host_probe.exe" or warmup.name != "cuexis_pd_baseline_probe.exe":
        raise ValueError("probe registration requires the matrix and machine-warmup executables")
    environment = capture_cpu()
    environment["foreground_policy"] = "fixed-existing-foreground"
    affinity = environment["machine"]["parent_affinity"]
    output.mkdir(parents=True, exist_ok=False)
    write_once(output / "pilot-declaration.json", {
        "schema": "rpa.execution.probe-pilot-declaration.v1", "formal_sampling": False,
        "units": [unit["id"] for unit in units], "processes_per_unit": 1, "sides": ["B"],
        "frames_per_process": probe_units.FRAMES,
        "warmup_frames_per_process": probe_units.WARMUP_FRAMES,
        "machine_warmup_seconds_per_process": 30, "timeout_seconds": TIMEOUT,
        "runtime_monitor_policy": POLICY, "source_sha": source["sha"],
        "affinity": affinity, "cpu_environment": environment,
        "stop": "Any build/environment/semantic/count failure; no replacement, recount or rerun"})
    records, state = [], {}
    try:
        for unit in units:
            parsed, raw = pilot_unit(unit, probe, warmup, root, affinity, environment, output, state)
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
                    "scope": "CPU presentation matrix only; no GPU, audio, device, Seek or "
                             "absolute observer gate is admitted by this proof"})
                proofs[kind + "_binding"] = artifact(path)
            records.append(unit_record(unit, probe, warmup, root, affinity, proofs))
        if args.retry_remedy:
            remedy = artifact(args.retry_remedy)
            ledger = read(authorization)["attempt_ledger"]
            bindings = retry_bindings(units, ledger, remedy, output)
            for record in records:
                record["retry_disposition_binding"] = bindings[record["id"]]
        if clean_source(root) != source:
            raise ValueError("source changed during the probe pilot")
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
            "fixture_bindings": {
                probe_units.HARNESS: artifact(root / "tests/pd/pd5_matrix_probe.cpp"),
                "rpa-execution-fixture": artifact(root / "tests/rpa/execution_fixture.hpp"),
                "probe-environment": artifact(root / "tests/pd/measurement_environment.hpp")},
            "binary_bindings": {"B": candidate["binary"], "W": warmup_metadata["binary"]},
            "binary_build_bindings": {"B": artifact(args.build_binding),
                                      "W": artifact(args.warmup_build_binding)},
            "windows": 3, "processes_per_window": 3, "gap_seconds": 600,
            "orders": admission.ORDERS, "regression_limit": 1.10, "noise_limit": .15,
            "units": records,
            "invalid_process_rules": ["No replacement: timeout/nonzero/missing/identity/"
                                      "environment failure stops this registration; raw files remain."]}
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
    parser.add_argument("--retry-remedy", type=Path,
                        help="verified remedy artifact; enables one second registration per unit")
    run(parser.parse_args())


if __name__ == "__main__":
    main()

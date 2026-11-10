"""One bounded owner A/B pilot group, followed by preparation only; never claims formal attempts."""
import argparse
from pathlib import Path
import os
import time

from tools.verification.playback_performance import measurement, process_control
from . import admission, build_binding
from .binding import artifact, clean_source, read, sha256, verify_artifact, write_once
from .environment import capture_cpu, verify_cpu, verify_native, verify_warmup
from .runtime_monitor import POLICY, RuntimeMonitor


READS = 1048576
TIMEOUT = 180
TEST = "PD uninstrumented validated fixed workload"
WARMUP = "PD machine CPU warmup"


def launch(binary, root, affinity, profile, phase):
    return {"command": [str(binary), TEST, "--rng-seed", "1"],
            "warmup_command": [str(binary), WARMUP, "--rng-seed", "1"],
            "cwd": str(root), "affinity": affinity, "warmup_seconds": 30,
            "timeout_seconds": TIMEOUT,
            "environment": {"CUEXIS_PD_PROFILE": profile, "CUEXIS_PD_FORMAL_PHASE": phase,
                            "CUEXIS_PD_V2_READS": str(READS)}}


def run(args):
    root, output = args.source.resolve(), args.output.resolve()
    candidate = clean_source(root)
    baseline = clean_source(args.baseline_source)
    if baseline["sha"] != admission.PRE_PD:
        raise ValueError("original pre-PD source required")
    builds = {"A": read(args.baseline_build_binding), "B": read(args.candidate_build_binding)}
    sources = {"A": baseline, "B": candidate}
    for side in builds:
        build_binding.verify(builds[side], sources[side], "normal")
        if builds[side]["config"] != args.config:
            raise ValueError("pilot configuration mismatch")
    if builds["A"]["toolchain_signature"] != builds["B"]["toolchain_signature"]:
        raise ValueError("A/B toolchain mismatch")
    environment = capture_cpu()
    environment["foreground_policy"] = "fixed-existing-foreground"
    affinity = environment["topology"]["candidate_masks"][0]
    output.mkdir(parents=True, exist_ok=False)
    write_once(output / "pilot-declaration.json", {
        "schema": "rpa.execution.cpu-pilot-declaration.v1", "formal_sampling": False,
        "profiles": ["legacy", "rpa"], "sides": ["A", "B"], "maximum_processes": 4,
        "machine_warmup_seconds_per_process": 30, "native_phase": "pilot", "blocks": 3,
        "read_operations_per_block": READS, "timeout_seconds": TIMEOUT,
        "runtime_monitor_policy": POLICY, "source_sha": candidate["sha"],
        "baseline_sha": baseline["sha"], "cpu_environment": environment,
        "stop": "Any build/environment/semantic/count failure; no replacement or count adaptation"})
    units = []
    native_settings = None
    try:
        for profile in ("legacy", "rpa"):
            unit_id = "p0-" + profile + "-" + args.config.lower()
            results, raw = {}, []
            for side in ("A", "B"):
                verify_cpu(environment)
                binary = verify_artifact(builds[side]["binary"])
                spec = launch(binary, root, affinity, profile, "pilot")
                env = {key: value for key, value in os.environ.items()
                       if not key.startswith(("CUEXIS_PD_", "CUEXIS_PD5_", "CUEXIS_REDESIGN_"))}
                env.update(spec["environment"])
                for phase, command in (("warmup", spec["warmup_command"]), ("sample", spec["command"])):
                    log = output / f"{profile}-{side}-{phase}.log"
                    monitor = RuntimeMonitor(environment["foreground_pid"], timeout=TIMEOUT + 5)
                    started = time.monotonic()
                    try:
                        with log.open("xb") as stream, monitor:
                            result = process_control.run(command, affinity, cwd=root, env=env,
                                                         output=stream, timeout=TIMEOUT)
                    finally:
                        monitored = monitor.result()
                        monitor_path = output / f"{profile}-{side}-{phase}-environment.json"
                        write_once(monitor_path, monitored)
                        raw.append(artifact(monitor_path))
                    record = {"command": command, "exit_code": result.returncode,
                              "elapsed_seconds": time.monotonic() - started, "log": artifact(log)}
                    path = output / f"{profile}-{side}-{phase}.json"
                    write_once(path, record)
                    raw.extend((artifact(path), artifact(log)))
                    if result.returncode or not monitored["passed"]:
                        raise ValueError("pilot failed process/environment; no replacement")
                    text = log.read_text(encoding="utf-8", errors="strict")
                    if phase == "warmup":
                        verify_warmup(text)
                    else:
                        native_settings = verify_native(text, affinity, native_settings)
                        results[side] = measurement.parse_owner(text, phase="pilot")
                        reads = [row for row in results[side]["samples"] if row["kind"] == "read_long"]
                        if any(row["operations"] != READS or row["sample"]["wall_us"] < 100000 for row in reads):
                            raise ValueError("fixed pilot read block below minimum; no automatic count adaptation")
                verify_cpu(environment)
                print(profile, side, "pilot complete", flush=True)
            for metric in ("validated_trace", "playback_only", "read_long"):
                measurement.require_comparable(results["A"], results["B"], metric)
            parsed = output / (profile + "-parsed.json")
            write_once(parsed, results)
            raw.append(artifact(parsed))
            unit = {"id": unit_id, "stable_protocol_id": unit_id, "workload": "steady",
                    "gate": "p0-" + profile, "config": args.config, "comparison": "A/B",
                    "baseline": "pre-PD", "baseline_binary": "A", "candidate_binary": "B",
                    "cold_process": False, "requests_per_process": 8, "fixture": "fixed-trace",
                    "profile": "original-fixed-" + profile, "completion_endpoint": "fixed-owner-block",
                    "required_metrics": ["validated_trace", "playback_only", "read_long"],
                    "measurement_mode": "normal", "adapter": "owner", "runtime_monitor_policy": POLICY,
                    "expected_identity": results["B"]["identity"],
                    "launch": {side: launch(Path(builds[side]["binary"]["path"]), root, affinity, profile, "formal")
                               for side in ("A", "B")}}
            for kind in ("semantic", "pilot", "environment"):
                path = output / (profile + "-" + kind + "-proof.json")
                write_once(path, {"schema": "rpa.execution." + kind + ".v1", "passed": True,
                    "unit": unit_id, "source_sha": candidate["sha"], "endpoint": unit["completion_endpoint"],
                    "candidate_binary_sha256": builds["B"]["binary"]["sha256"], "raw_bindings": raw,
                    "cpu_environment": environment, "runtime_monitor_policy": POLICY,
                    "scope": "CPU owner fixed trace; no GPU/audio/Seek/memory admission"})
                unit[kind + "_binding"] = artifact(path)
            units.append(unit)
        if clean_source(root) != candidate or clean_source(args.baseline_source) != baseline:
            raise ValueError("source changed during pilot")
        authorization = root / "docs/stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-05-execution-authorization.json"
        tools = [*root.joinpath("tools/verification/execution_acceptance").glob("*.py"),
                 *root.joinpath("tools/verification/playback_performance").glob("*.py"),
                 root / "tests/pd/formal_baseline.py", root / "tests/pd/system_runtime_support.py"]
        preparation = {"schema": "rpa.execution.preparation.v1", "formal_registration": False,
            "source_binding": candidate, "baseline_source_binding": baseline, "baseline_sha": baseline["sha"],
            "acceptance_binding": artifact(authorization.with_name("2026-11-05-o1-budget-acceptance.json")),
            "authorization_binding": artifact(authorization), "attempt_ledger": read(authorization)["attempt_ledger"],
            "budget_binding": artifact(root / "tools/verification/execution_acceptance/budget-proposal.v1.json"),
            "tool_bindings": {str(path.relative_to(root)): artifact(path) for path in tools},
            "fixture_bindings": {"fixed-trace": artifact(root / "tests/rpa/execution_fixture.hpp")},
            "binary_bindings": {side: builds[side]["binary"] for side in ("A", "B")},
            "binary_build_bindings": {"A": artifact(args.baseline_build_binding), "B": artifact(args.candidate_build_binding)},
            "windows": 3, "processes_per_window": 3, "gap_seconds": 600, "orders": admission.ORDERS,
            "regression_limit": 1.10, "noise_limit": .15, "units": units,
            "invalid_process_rules": ["No replacement: timeout/nonzero/missing/identity/environment failure stops this registration; raw files remain."]}
        admission.validate_preparation(preparation)
        write_once(output / "preparation.json", preparation)
    except Exception as error:
        write_once(output / "stopped.json", {"error": str(error), "formal_sampling": False,
                                            "formal_attempts_consumed": 0})
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for field in ("source", "baseline-source", "baseline-build-binding", "candidate-build-binding", "output"):
        parser.add_argument("--" + field, type=Path, required=True)
    parser.add_argument("--config", choices=("Release", "Debug"), required=True)
    run(parser.parse_args())


if __name__ == "__main__":
    main()

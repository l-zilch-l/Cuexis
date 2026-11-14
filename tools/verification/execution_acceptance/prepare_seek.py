"""Preparation for one or more of the five pre-declared Seek classes.

Every class is its own unit with its own gate and request count, because the classes differ by the
fixture and by the path the request takes, not by a multiplier. The device probe is the only build
that compiles those fixtures, so the preparation binds the OpenGL executable and records the same
identity, integrity and environment proofs the other adapters use. This module never claims an
attempt: admission.claim does, and the runner only executes a sealed preparation.
"""
import argparse
from pathlib import Path

from tools.verification.playback_performance import measurement

from . import admission, build_binding
from .binding import artifact, clean_source, read, verify_artifact, write_once
from .environment import capture_cpu, verify_cpu, verify_native, verify_warmup
from .prepare_probe import TIMEOUT, WARMUP_TEST, child
from .runtime_monitor import POLICY

PROBE_TEST = "PD5 scripted seek timing probe"
ENDPOINT = "seek-to-target-present"
FIXTURE = "execution-device-probe"
CLASSES = ("natural", "hot", "external", "fallback", "cold")
RECORDS = 129
SEEKS = 16
COLD_SEEKS = 1
# The probe names the class it builds; the cold class is the first seek of a fresh process, which the
# probe expresses as one hot request.
PROBE_CLASS = {"natural": "natural", "hot": "hot", "external": "external",
               "fallback": "fallback", "cold": "hot"}
METRICS = ("request_to_ready_us", "request_to_present_us")


def gate(name):
    return "p5-seek-" + name


def stable_id(name):
    """The canonical Seek identity: class and declared record scale, never an invented entity count."""
    return f"seek-{name}-{RECORDS}-release"


def requests(name):
    return COLD_SEEKS if name == "cold" else SEEKS


def launch(probe, warmup, root, affinity, name):
    return {"command": [str(probe), PROBE_TEST, "--rng-seed", "1"],
            "warmup_command": [str(warmup), WARMUP_TEST, "--rng-seed", "1"],
            "cwd": str(root), "affinity": affinity, "warmup_seconds": 30,
            "timeout_seconds": TIMEOUT,
            "environment": {"CUEXIS_PD5_SEEK_CLASS": PROBE_CLASS[name],
                            "CUEXIS_PD5_SEEK_RECORDS": str(RECORDS),
                            "CUEXIS_PD5_SEEK_COUNT": str(requests(name))}}


def identity(name, parsed):
    """The identity a formal process must reproduce, plus the class precondition."""
    value = parsed["identity"]
    if value.get("schema") != "pd5.seek.v1":
        raise ValueError("pilot is not a seek probe record")
    if value.get("seek_class") != PROBE_CLASS[name]:
        raise ValueError("pilot class differs from the registered class")
    if value.get("records") != RECORDS or value.get("seeks") != requests(name):
        raise ValueError("pilot records or seek count differ from the registration")
    certified = value.get("certified_records")
    if name == "natural":
        if not isinstance(certified, int) or certified <= 0:
            raise ValueError("natural class must consume a session certificate")
    elif certified != 0:
        raise ValueError("class must not consume a certificate")
    for metric in METRICS:
        measurement.positive(parsed["statistics"][metric]["median_us"])
    return {key: value[key] for key in ("schema", "records", "seeks", "seek_class")}


def run(args):
    root, output = args.source.resolve(), args.output.resolve()
    names = [name.strip() for name in args.classes.split(",") if name.strip()]
    unknown = [name for name in names if name not in CLASSES]
    if not names or unknown:
        raise ValueError("declare one or more of the five Seek classes: " + ", ".join(CLASSES))
    source = clean_source(root)
    candidate, warmup_metadata = read(args.build_binding), read(args.warmup_build_binding)
    for metadata in (candidate, warmup_metadata):
        build_binding.verify(metadata, source, "normal")
        if metadata["config"] != "Release":
            raise ValueError("seek registration requires the reference Release configuration")
    if candidate["toolchain_signature"] != warmup_metadata["toolchain_signature"]:
        raise ValueError("probe and machine-warmup binaries use different toolchains")
    probe = Path(verify_artifact(candidate["binary"]))
    warmup = Path(verify_artifact(warmup_metadata["binary"]))
    if probe.name != "cuexis_execution_device_probe.exe":
        raise ValueError("seek registration requires the device probe executable")
    if warmup.name != "cuexis_pd_baseline_probe.exe":
        raise ValueError("seek registration requires the machine-warmup executable")
    environment = capture_cpu()
    environment["foreground_policy"] = "fixed-existing-foreground"
    affinity = environment["machine"]["parent_affinity"]
    output.mkdir(parents=True, exist_ok=False)
    write_once(output / "pilot-declaration.json", {
        "schema": "rpa.execution.seek-pilot-declaration.v1", "formal_sampling": False,
        "classes": names, "records": RECORDS, "seeks": {name: requests(name) for name in names},
        "machine_warmup_seconds_per_process": 30, "timeout_seconds": TIMEOUT,
        "runtime_monitor_policy": POLICY, "source_sha": source["sha"], "affinity": affinity,
        "cpu_environment": environment,
        "stop": "Any build/environment/identity/oracle failure; no replacement and no rerun"})
    units = []
    for name in names:
        spec = launch(probe, warmup, root, affinity, name)
        warm_log, warm_raw = child({"id": gate(name)}, "warmup", spec, environment, output)
        verify_warmup(warm_log.read_text(encoding="utf-8", errors="strict"))
        log, raw = child({"id": gate(name)}, "sample", spec, environment, output)
        text = log.read_text(encoding="utf-8", errors="strict")
        verify_native(text, affinity, None)
        verify_cpu(environment)
        parsed = measurement.parse_seek(text, exit_code=0)
        expected = identity(name, parsed)
        parsed_path = output / (name + "-parsed.json")
        write_once(parsed_path, parsed)
        raw.extend((artifact(parsed_path), *warm_raw))
        proofs = {}
        for kind in ("semantic", "pilot", "environment"):
            path = output / f"{name}-{kind}-proof.json"
            write_once(path, {
                "schema": "rpa.execution." + kind + ".v1", "passed": True, "unit": gate(name),
                "source_sha": source["sha"], "endpoint": ENDPOINT,
                "candidate_binary_sha256": candidate["binary"]["sha256"],
                "observed_medians_us": {metric: parsed["statistics"][metric]["median_us"]
                                        for metric in METRICS},
                "oracle": parsed["verify"], "raw_bindings": raw, "cpu_environment": environment,
                "runtime_monitor_policy": POLICY,
                "scope": "one registered Seek class on the OpenGL backend; no other class, no "
                         "presentation-matrix counts and no A-side comparison is admitted here"})
            proofs[kind + "_binding"] = artifact(path)
        if clean_source(root) != source:
            raise ValueError("source changed during the Seek pilot")
        units.append({
            "id": gate(name), "stable_protocol_id": stable_id(name), "workload": "seek",
            "seek_class": name, "seek_records": RECORDS,
            "gate": gate(name), "config": "Release", "comparison": "B-only",
            "candidate_binary": "B", "cold_process": name == "cold",
            "requests_per_process": requests(name), "fixture": FIXTURE,
            "profile": "seek-" + name + "-" + str(RECORDS) + "-records-v1",
            "completion_endpoint": ENDPOINT, "required_metrics": list(METRICS),
            "adapter": "seek", "measurement_mode": "normal", "runtime_monitor_policy": POLICY,
            "expected_identity": expected, "launch": {"B": spec}, **proofs})
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
        "fixture_bindings": {FIXTURE: artifact(root / "tests/pd/pd5_device_probe.cpp"),
                             "seek-fixture": artifact(root / "tests/pd/pd5_host_probe.cpp"),
                             "probe-environment": artifact(root / "tests/pd/measurement_environment.hpp")},
        "binary_bindings": {"B": candidate["binary"], "W": warmup_metadata["binary"]},
        "binary_build_bindings": {"B": artifact(args.build_binding),
                                  "W": artifact(args.warmup_build_binding)},
        "windows": 3, "processes_per_window": 3, "gap_seconds": 600,
        "orders": admission.ORDERS, "regression_limit": 1.10, "noise_limit": .15,
        "units": units,
        "invalid_process_rules": ["No replacement: timeout/nonzero/missing/oracle/identity/"
                                  "environment failure stops this registration; raw files remain."],
        "attempt_ledger": read(args.authorization)["attempt_ledger"]}
    admission.validate_preparation(preparation)
    write_once(output / "preparation.json", preparation)
    print("PREPARATION " + str(output / "preparation.json"), flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for field in ("source", "build-binding", "warmup-build-binding", "output"):
        parser.add_argument("--" + field, type=Path, required=True)
    parser.add_argument("--classes", default=",".join(CLASSES))
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

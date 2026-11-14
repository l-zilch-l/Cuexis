"""Verify finite observer qualification pairs and record blocked memory/P8 admission."""
import argparse
from pathlib import Path

from . import build_binding, capacity_evidence
from .admission import CAPACITY_ADMISSION_BLOCKER
from .binding import artifact, clean_source, read, verify_artifact, write_once
from .environment import original_support
from .environment_stability import summary as environment_summary
from .runtime_monitor import POLICY


def qualification(normal, memory, source, device):
    if len(normal) != 3 or len(memory) != 3:
        raise ValueError("three fixed normal/memory qualification pairs required")
    if len({path.parent.resolve() for path in (*normal, *memory)}) != 6:
        raise ValueError("six independent qualification invocation directories required")
    ratios, bindings, scale = [], [], None
    expected_environment, toolchain = None, None
    process_starts, verified_builds = set(), set()
    for left, right in zip(normal, memory):
        for path, mode in ((left, "normal"), (right, "memory")):
            declaration = read(path.parent / "declaration.json")
            if (declaration.get("source_sha") != source["sha"] or
                    declaration.get("purpose") != "observer-qualification" or
                    declaration.get("formal_sampling") is not False or
                    declaration.get("device") is not device or declaration.get("mode") != mode or
                    declaration.get("entities") not in (64, 2048) or declaration.get("phase_hold_ms")):
                raise ValueError("qualification identity, backend or phase waits differ")
            current = (declaration["entities"], declaration["rows"], declaration["reuse"])
            if scale is not None and scale != current:
                raise ValueError("qualification workload differs between independent pairs")
            scale = current
            process = read(path.parent / "process.json")
            bound_path = verify_artifact(declaration["build_binding"])
            metadata = read(bound_path)
            build_key = (declaration["build_binding"]["sha256"], mode)
            if build_key not in verified_builds:
                build_binding.verify(metadata, source, mode)
                verified_builds.add(build_key)
            executable = verify_artifact(metadata["binary"])
            expected_binary = "cuexis_execution_device_probe.exe" if device else "cuexis_pd_host_probe.exe"
            if (metadata["config"] != "Release" or executable.name != expected_binary or
                    process.get("command") != [str(executable), capacity_evidence.TEST_CASE,
                                                "--reporter", "compact", "--rng-seed", "1"]):
                raise ValueError("qualification command does not use its exact bound capacity binary")
            if toolchain is not None and metadata["toolchain_signature"] != toolchain:
                raise ValueError("qualification toolchains differ")
            toolchain = metadata["toolchain_signature"]
            environment = read(path.parent / "runtime-environment.json")
            if process.get("exit_code") != 0 or environment.get("passed") is not True:
                raise ValueError("qualification process failed")
            before = read(path.parent / "environment-before.json")
            after = read(path.parent / "environment-after.json")
            for current in (before, after):
                if expected_environment is None:
                    expected_environment = current
                original_support().same_environment(expected_environment, current)
                if (current.get("foreground_pid") != expected_environment.get("foreground_pid") or
                        current.get("topology") != expected_environment.get("topology") or
                        current.get("foreground_policy") != "fixed-existing-foreground"):
                    raise ValueError("qualification foreground or topology changed")
            replayed = environment_summary(environment["samples"])
            if (environment.get("policy") != POLICY or not replayed["passed"] or
                    replayed["distinct_foreground_pids"] != [before["foreground_pid"]] or
                    environment.get("errors") or
                    environment.get("maximum_gap_seconds") != replayed["maximum_gap_seconds"]):
                raise ValueError("qualification environment proof differs from its actual samples")
            raw = verify_artifact(process["log"]).read_text(encoding="utf-8", errors="strict")
            starts = capacity_evidence.measurement.records(raw, "PD5_PROCESS_START ")
            if len(starts) != 1 or starts[0].get("boundary") != "before-native-CreateProcess":
                raise ValueError("qualification native process identity missing")
            identity = tuple(starts[0].get(key) for key in ("pid", "qpc", "frequency"))
            if any(type(value) is not int or value <= 0 for value in identity) or identity in process_starts:
                raise ValueError("qualification processes are not independent invocations")
            process_starts.add(identity)
            native = capacity_evidence.capacity(raw, declaration["entities"], declaration["rows"],
                                                declaration["reuse"], device)
            summary = read(path)
            if (summary.get("semantic_identity") != {key: native[key] for key in capacity_evidence.SEMANTIC_FIELDS}
                    or summary.get("elapsed_seconds") != process.get("elapsed_seconds")):
                raise ValueError("qualification summary differs from its native process record")
            if mode == "memory":
                budget_path = source["files"]["tools/verification/execution_acceptance/budget-proposal.v1.json"]["path"]
                profile = next(row for row in read(budget_path)["profiles"]
                               if row["id"] == f"r4-reference-n{declaration['entities']}-v1")
                observed = capacity_evidence.collect(raw, profile, device=device, residency_required=False)
                if summary.get("memory") != observed:
                    raise ValueError("qualification memory summary differs from original counters")
            bindings.extend((artifact(path), artifact(path.parent / "declaration.json"),
                             artifact(path.parent / "process.json"), process["log"],
                             artifact(path.parent / "runtime-environment.json"),
                             artifact(path.parent / "environment-before.json"),
                             artifact(path.parent / "environment-after.json"),
                             declaration["build_binding"], metadata["binary"]))
        a, b = read(left), read(right)
        if a["semantic_identity"] != b["semantic_identity"]:
            raise ValueError("observer altered exact packed/Snapshot output or workload")
        if (not b.get("memory", {}).get("comparisons") or
                b["memory"]["origin_peak_rating"].get("formal_acceptance") is not False):
            raise ValueError("qualification must expose complete memory evidence, not self-accept")
        for duration in (a["elapsed_seconds"], b["elapsed_seconds"]):
            if not isinstance(duration, (int, float)) or not 0 < duration <= 300:
                raise ValueError("invalid qualification cost")
        ratios.append(b["elapsed_seconds"] / a["elapsed_seconds"])
    return {"raw_bindings": bindings, "wall_cost_ratios": ratios, "scale": scale,
            "cpu_environment": expected_environment, "toolchain_signature": toolchain,
            "formal_sampling": False, "formal_acceptance": False,
            "coverage": "exact allocation partitions, prepare/active/close/release checkpoints, "
                        "final product new-book release, tracked World subset, independent "
                        "normal/memory output hashes and finite observer range",
            "scope": "non-formal observer qualification only; no complete semantic graph, "
                     "residency/phase-hold qualification or target-scale extrapolation",
            "overhead_policy": "memory observer cost disclosed; never used to correct normal timing"}


def run(args):
    root, output = args.source.resolve(), args.output.resolve()
    source = clean_source(root)
    metadata = read(args.build_binding)
    build_binding.verify(metadata, source, "memory")
    binary = verify_artifact(metadata["binary"])
    expected = "cuexis_execution_device_probe.exe" if args.device else "cuexis_pd_host_probe.exe"
    if binary.name != expected or metadata["config"] != "Release":
        raise ValueError("capacity memory binary/backend differs")
    observed = qualification(args.qualification_normal, args.qualification_memory, source, args.device)
    if metadata["toolchain_signature"] != observed["toolchain_signature"]:
        raise ValueError("target capacity build differs from qualification toolchain")
    rows = args.entities if args.rows is None else args.rows
    if (args.entities == 40000 and rows != 4096) or not 1 <= rows <= args.entities:
        raise ValueError("unknown capacity workload")
    workload = "p8" if args.entities == 40000 else "memory"
    identifier = f"{workload}-{args.entities}-{args.reuse}"
    backend = "device" if args.device else "cpu"
    target = {"id": identifier, "workload": workload, "backend": backend,
              "entities": args.entities, "rows": rows, "reuse": args.reuse}
    entities, observed_rows, reuse = observed["scale"]
    output.mkdir(parents=True, exist_ok=False)
    qualification_path = output / "qualification.json"
    record = {**observed, "schema": "rpa.execution.capacity-qualification.v1",
              "formal_sampling": False, "formal_acceptance": False,
              "source_sha": source["sha"], "target": target,
              "candidate_build_binding": artifact(args.build_binding),
              "candidate_binary": metadata["binary"],
              "applicability": {
                  "observed_workload": {"backend": backend, "entities": entities,
                                        "rows": observed_rows, "reuse": reuse},
                  "target_workload_matches": observed["scale"] == (args.entities, rows, args.reuse),
                  "phase_hold_ms": 0, "gpu_residency_qualified": False,
                  "scope": "exact observed backend/scale/rows/reuse without phase holds only; "
                           "a matching target still has no formal semantic admission"}}
    write_once(qualification_path, record)
    blocked_path = output / "blocked.json"
    write_once(blocked_path, {"schema": "rpa.execution.capacity-blocked.v1", "status": "blocked",
                              "formal_registration": False, "formal_acceptance": False,
                              "target": target, "reason": CAPACITY_ADMISSION_BLOCKER,
                              "qualification_binding": artifact(qualification_path)})
    print("BLOCKED " + str(blocked_path), flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for field in ("source", "build-binding", "output"):
        parser.add_argument("--" + field, type=Path, required=True)
    parser.add_argument("--qualification-normal", type=Path, nargs=3, required=True)
    parser.add_argument("--qualification-memory", type=Path, nargs=3, required=True)
    parser.add_argument("--entities", type=int, choices=(64, 2048, 40000), required=True)
    parser.add_argument("--rows", type=int)
    parser.add_argument("--reuse", choices=("low", "high"), required=True)
    parser.add_argument("--device", action="store_true")
    run(parser.parse_args())


if __name__ == "__main__":
    main()

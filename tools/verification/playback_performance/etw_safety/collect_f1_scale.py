#!/usr/bin/env python3
"""Collect three bounded F1-only scale processes for a diagnostic ETW trace."""
from __future__ import annotations

import hashlib
import importlib.util
import json
import os
import argparse
import ctypes
import re
import subprocess
from pathlib import Path
import statistics
import sys
import time
import traceback

HERE = Path(__file__).resolve().parent
PLAN_PATH = HERE / "plan.json"
BINDING_PATH = HERE / "binding.json"
METRICS = ("elapsed_us", "cpu_us", "cycles")
OBJECTS = (1, 8, 64, 256)
MODES = ("read", "updated")
BLOCKS = (0, 1, 2)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def write_new_json(path: Path, value) -> None:
    payload = json.dumps(value, indent=2, sort_keys=True, ensure_ascii=False) + "\n"
    with path.open("x", encoding="utf-8", newline="\n") as stream:
        stream.write(payload)
        stream.flush()
        os.fsync(stream.fileno())


def validate_workload_shape(plan) -> None:
    """Refuse a plan whose fixed workload no longer matches this collector.

    The row and content contracts are derived from the fixed OBJECTS/MODES/
    BLOCKS sequence, so a plan that describes a different shape must stop the
    run instead of being parsed into a misleading failure.
    """
    workload = plan["workload"]
    if list(workload["objects"]) != list(OBJECTS) or list(workload["modes"]) != list(MODES):
        raise RuntimeError("fixed objects/modes workload was changed")
    if workload["blocks_per_metric"] != len(BLOCKS):
        raise RuntimeError("fixed blocks-per-metric workload was changed")
    counts = workload["counts"].split()
    if len(counts) != len(OBJECTS) * 3:
        raise RuntimeError("workload counts do not describe one read/update pair per objects count")
    if [int(counts[index]) for index in range(0, len(counts), 3)] != list(OBJECTS):
        raise RuntimeError("workload counts disagree with the fixed objects sequence")


def verify_bindings(plan, binding) -> None:
    for relative, expected in binding["artifact_sha256"].items():
        actual = sha256(HERE / relative)
        if actual.lower() != expected.lower():
            raise RuntimeError(f"prepared artifact hash changed: {relative}")
    for key, item in binding["input_sha256"].items():
        actual = sha256(Path(item["path"]))
        if actual.lower() != item["sha256"].lower():
            raise RuntimeError(f"pinned input changed: {key}: {item['path']}")

    anchor = plan["input_binding"]
    source_plan = load_json(Path(anchor["plan_path"]))
    f1 = source_plan["builds"]["A"]["bindings"]["baseline"]
    if str(Path(f1["path"])) != str(Path(anchor["baseline_executable"])):
        raise RuntimeError("F1 executable path does not match the input-plan A binding")
    if f1["sha256"].lower() != anchor["baseline_executable_sha256"].lower():
        raise RuntimeError("F1 executable hash does not match the input-plan A binding")
    if source_plan["builds"]["A"]["source_identity"]["sha256"].lower() != \
            anchor["build_source_identity_sha256"].lower():
        raise RuntimeError("F1 source identity does not match the input-plan A binding")
    if plan["workload"]["mask"] != 4 or plan["workload"]["processes"] != 3:
        raise RuntimeError("fixed three-process mask-4 workload was changed")
    if plan["workload"]["warmup_seconds_before_each_measured_process"] != 30:
        raise RuntimeError("fixed 30-second warmup was changed")
    validate_workload_shape(plan)


def marker_records(text: str, marker: str):
    result = []
    prefix = marker + " "
    for line in text.splitlines():
        if line.startswith(prefix):
            try:
                result.append(json.loads(line[len(prefix):]))
            except json.JSONDecodeError as error:
                raise ValueError(f"malformed {marker} JSON: {error}") from error
    return result


def validate_content_identity(contents) -> None:
    """Check the content-identity pairs printed around the read blocks.

    The frozen F1 executable prints two PD_FOLLOW_CONTENT records per fixed
    objects count: one before and one after the three read blocks. Both must
    name the same sha256, because the read blocks are non-mutating; the
    observed identity is preserved as evidence instead of being inferred.
    """
    expected_pairs = len(OBJECTS) * 2
    if len(contents) != expected_pairs:
        raise ValueError(
            f"expected {expected_pairs} PD_FOLLOW_CONTENT records "
            f"(one identity pair per fixed objects count), got {len(contents)}")
    order = [row.get("objects") for row in contents]
    wanted = [objects for objects in OBJECTS for _ in range(2)]
    if order != wanted:
        raise ValueError(
            f"PD_FOLLOW_CONTENT order is not one pair per fixed objects count: {order}")
    for objects, (before, after) in zip(OBJECTS, zip(contents[0::2], contents[1::2])):
        for row in (before, after):
            digest = row.get("sha256")
            if not isinstance(digest, str) or len(digest) != 64:
                raise ValueError(f"content record for {objects} objects has no sha256")
        if before["sha256"].lower() != after["sha256"].lower():
            raise ValueError(
                f"content changed across the read blocks for {objects} objects: "
                f"{before['sha256']} -> {after['sha256']}")


def validate_output(rows, contents, plan) -> None:
    counts = [int(part) for part in plan["workload"]["counts"].split()]
    expected = {
        counts[index]: (counts[index + 1], counts[index + 2])
        for index in range(0, len(counts), 3)
    }
    expected_rows = len(OBJECTS) * len(MODES) * len(BLOCKS)
    if len(rows) != expected_rows or len(contents) != len(OBJECTS) * 2:
        raise ValueError(
            f"expected {expected_rows} PD_FOLLOW_ROW and {len(OBJECTS) * 2} "
            f"PD_FOLLOW_CONTENT records, got {len(rows)} and {len(contents)}")
    if {row.get("objects") for row in rows} != set(OBJECTS):
        raise ValueError("PD_FOLLOW_ROW object counts differ from the fixed plan")
    validate_content_identity(contents)
    for objects in OBJECTS:
        for mode_index, mode in enumerate(MODES):
            selected = [row for row in rows
                        if row.get("objects") == objects and row.get("mode") == mode]
            if [row.get("block") for row in selected] != list(BLOCKS):
                raise ValueError(f"missing/duplicate block for {objects}-{mode}")
            if any(row.get("operations") != expected[objects][mode_index] for row in selected):
                raise ValueError(f"operation count mismatch for {objects}-{mode}")
            if any(not isinstance(row.get(key), (int, float))
                   for row in selected for key in METRICS):
                raise ValueError(f"missing timing metrics for {objects}-{mode}")
            if mode == "read" and any(row["elapsed_us"] < 100000 for row in selected):
                raise ValueError(f"read block below 100000 us for {objects}")


def validate_anchors(starts, env_records, measured_pid: int) -> None:
    if len(starts) != 2 or sum(row.get("pid") == measured_pid for row in starts) != 1:
        raise ValueError("expected one warmup and one measured process-start record")
    measured_start = next(row for row in starts if row.get("pid") == measured_pid)
    if not measured_start.get("qpc") or not measured_start.get("frequency"):
        raise ValueError("measured native PID QPC/frequency anchor is missing")
    measured_env = [item["record"] for item in env_records
                    if item["record"].get("pid") == measured_pid]
    if len(measured_env) != 2 or {row.get("label") for row in measured_env} != {"begin", "end"}:
        raise ValueError("measured PID must have PD_ENV begin/end records")
    for row in measured_env:
        if row.get("process_affinity_error") != 0 or row.get("process_affinity") != 4:
            raise ValueError("PD_ENV process affinity is not mask 4")
        if row.get("processor_group_error") != 0:
            raise ValueError("PD_ENV processor-group query failed")


def summarize(processes):
    summary = {}
    for objects in OBJECTS:
        for mode in MODES:
            rows_by_process = [
                [row for row in process["rows"]
                 if row["objects"] == objects and row["mode"] == mode]
                for process in processes
            ]
            for metric in METRICS:
                per_process = [
                    {
                        "process_index": process["index"],
                        "pid": process["measured_pid"],
                        "block_values": [row[metric] for row in rows],
                        "process_median": statistics.median(row[metric] for row in rows),
                    }
                    for process, rows in zip(processes, rows_by_process)
                ]
                centers = [item["process_median"] for item in per_process]
                center = statistics.median(centers)
                summary[f"{objects}-{mode}-{metric}"] = {
                    "unit": "us" if metric.endswith("_us") else "cycles",
                    "process_medians": per_process,
                    "median_of_three_process_medians": center,
                    "minimum_process_median": min(centers),
                    "maximum_process_median": max(centers),
                    "relative_range_percent": (max(centers) - min(centers)) / center * 100
                    if center else None,
                }
    return summary


def verify_capture_authorization(args, plan, binding, output):
    if os.name != "nt" or ctypes.windll.shell32.IsUserAnAdmin() == 0:
        raise RuntimeError("collector requires an administrator token")
    privilege_text = subprocess.run(
        ["whoami.exe", "/priv"], check=True, capture_output=True, text=True, timeout=10
    ).stdout
    privilege_line = next(
        (line for line in privilege_text.splitlines()
         if re.match(r"^\s*SeSystemProfilePrivilege\s+", line)),
        None,
    )
    if privilege_line is None:
        raise RuntimeError("collector token lacks SeSystemProfilePrivilege")
    privilege_state = (
        "Enabled" if re.search(r"\bEnabled\b", privilege_line)
        else "Disabled" if re.search(r"\bDisabled\b", privilege_line)
        else "Unknown"
    )

    receipt_path = args.capture_receipt.resolve()
    if receipt_path.parent != output.parent:
        raise RuntimeError("capture receipt must be beside the dedicated native output directory")
    receipt = load_json(receipt_path)
    plan_hash = sha256(PLAN_PATH)
    binding_hash = sha256(BINDING_PATH)
    if receipt.get("schema") != "pd5.f1-controller-capture.v1":
        raise RuntimeError("missing sealed-controller owned-capture receipt")
    if receipt.get("status") != "OWNED WPR CAPTURE ACTIVE" or receipt.get("wpr_start_succeeded") is not True:
        raise RuntimeError("owned-capture receipt does not confirm a successful WPR start")
    if receipt.get("wpr_instance") != args.wpr_instance:
        raise RuntimeError("owned-capture receipt instance does not match CLI instance")
    if receipt.get("plan_sha256") != plan_hash or receipt.get("binding_sha256") != binding_hash:
        raise RuntimeError("owned-capture receipt plan/binding hashes do not match")
    if receipt.get("controller_claim_sha256", "").lower() != args.controller_claim_sha256.lower():
        raise RuntimeError("owned-capture receipt claim does not match the supervising controller")
    # The controller that wrote the receipt must be the source hash sealed beside this collector.
    expected_controller_hash = binding["controller_source_sha256"]
    if receipt.get("controller_source_sha256", "").lower() != expected_controller_hash.lower():
        raise RuntimeError("owned-capture receipt controller hash does not match binding")

    status = subprocess.run(
        [str(args.wpr), "-status", "-instancename", args.wpr_instance],
        capture_output=True, text=True, timeout=15,
    )
    status_text = (status.stdout or "") + (status.stderr or "")
    status_record = {
        "command": [str(args.wpr), "-status", "-instancename", args.wpr_instance],
        "exit_code": status.returncode,
        "wpr_instance": args.wpr_instance,
        "stdout_stderr": status_text,
        "queried_utc_unix": time.time(),
    }
    write_new_json(output / "wpr-named-status.json", status_record)
    if status.returncode != 0 or re.search(r"\bnot recording\b", status_text, re.IGNORECASE):
        raise RuntimeError("the receipt's named WPR instance is not active")
    if not re.search(r"\brecording\b", status_text, re.IGNORECASE):
        raise RuntimeError("named WPR status did not confirm an active recording")
    return {
        "receipt_path": str(receipt_path),
        "wpr_instance": args.wpr_instance,
        "plan_sha256": plan_hash,
        "binding_sha256": binding_hash,
        "controller_claim_sha256": args.controller_claim_sha256,
        "controller_source_sha256": expected_controller_hash,
        "receipt_sha256": sha256(receipt_path),
        "administrator_token": True,
        "SeSystemProfilePrivilege_state": privilege_state,
        "named_wpr_status_exit_code": status.returncode,
        "named_wpr_active": True,
    }


def main(args) -> int:
    output = args.output.resolve()
    if not output.is_dir() or any(output.iterdir()):
        raise RuntimeError("output must be a new, empty native subdirectory")
    plan = load_json(PLAN_PATH)
    binding = load_json(BINDING_PATH)
    verify_bindings(plan, binding)
    capture = verify_capture_authorization(args, plan, binding, output)
    write_new_json(output / "capture-authorization.json", capture)

    anchor = plan["input_binding"]
    executable = Path(anchor["baseline_executable"])
    helper_path = Path(plan["support"]["frozen_process_control"])
    spec = importlib.util.spec_from_file_location("frozen_f1_process_control", helper_path)
    if spec is None or spec.loader is None:
        raise RuntimeError("cannot import pinned F1 process-control helper")
    control = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(control)

    counts = plan["workload"]["counts"]
    env = {key: value for key, value in os.environ.items()
           if not key.startswith(("CUEXIS_PD", "CUEXIS_FOLLOW"))}
    env.update(CUEXIS_FOLLOW_COUNTS=counts, CUEXIS_PD_HOST_DIAGNOSTIC_1024="1")
    run_id = output.name
    deadline = time.monotonic() + int(plan["wpr"]["collector_budget_seconds"])
    write_new_json(output / "run-claim.json", {
        "status": "CAPTURED DIAGNOSTIC RUN",
        "run_id": run_id,
        "plan_sha256": sha256(PLAN_PATH),
        "binding_sha256": sha256(BINDING_PATH),
        "binary_sha256": sha256(executable),
        "expected_binary_sha256": anchor["baseline_executable_sha256"],
        "mask": 4,
        "measured_process_count": 3,
        "warmup_process_count": 3,
        "formal_acceptance": False,
        "performance_acceptance": False,
        "candidate_comparison": False,
        "capture_authorization": capture,
        "controller_claim_sha256": args.controller_claim_sha256,
        "started_unix": time.time(),
    })

    processes = []
    written = []
    failures = []
    for index in range(3):
        name = f"f1-scale-{index}"
        log_path = output / f"{name}.log"
        record = {
            "name": name,
            "index": index,
            "side": "A",
            "kind": "scale",
            "measurement_filter": plan["workload"]["measurement_filter"],
            "warmup_filter": plan["workload"]["warmup_filter"],
            "mask": 4,
            "binary_path": str(executable),
            "binary_sha256": anchor["baseline_executable_sha256"],
            "start_unix": time.time(),
            "log": str(log_path),
            "warmup_timeout_seconds": plan["workload"]["timeout_seconds"]["one_warmup_process"],
            "measurement_timeout_seconds": plan["workload"]["timeout_seconds"]["one_measurement_process"],
            "launch_environment": {
                "CUEXIS_FOLLOW_COUNTS": counts,
                "CUEXIS_PD_HOST_DIAGNOSTIC_1024": "1",
                "inherited_CUEXIS_PD_and_CUEXIS_FOLLOW_variables": "removed",
            },
            "qpc_anchor_semantics": "PD5_PROCESS_START qpc is sampled before native CreateProcess",
            "per_block_qpc_anchors_available": False,
        }
        try:
            with log_path.open("x", encoding="utf-8", newline="\n") as log:
                warmup_timeout = min(record["warmup_timeout_seconds"],
                                     int(deadline - time.monotonic()))
                if warmup_timeout <= 0:
                    raise TimeoutError("whole-capture budget expired before warmup")
                warmup = control.run(
                    [str(executable), plan["workload"]["warmup_filter"], "--reporter", "compact"],
                    4, cwd=anchor["repository_root"], env=env, output=log, timeout=warmup_timeout)
                record["warmup_exit_code"] = warmup.returncode
                warmup.check_returncode()

                measured_timeout = min(record["measurement_timeout_seconds"],
                                       int(deadline - time.monotonic()))
                if measured_timeout <= 0:
                    raise TimeoutError("whole-capture budget expired before measurement")
                measured = control.run(
                    [str(executable), plan["workload"]["measurement_filter"],
                     "--rng-seed", "1", "--reporter", "compact"],
                    4, cwd=anchor["repository_root"], env=env, output=log, timeout=measured_timeout)
                record["exit_code"] = measured.returncode
                measured.check_returncode()
                log.flush()
                os.fsync(log.fileno())

            text = log_path.read_text(encoding="utf-8-sig")
            starts = marker_records(text, "PD5_PROCESS_START")
            rows = marker_records(text, "PD_FOLLOW_ROW")
            contents = marker_records(text, "PD_FOLLOW_CONTENT")
            env_records = [
                {"record": json.loads(line[len("PD_ENV "):]), "raw": line}
                for line in text.splitlines() if line.startswith("PD_ENV ")
            ]
            measured_start = next(row for row in reversed(starts)
                                  if row.get("boundary") == "before-native-CreateProcess")
            measured_pid = int(measured_start["pid"])
            validate_anchors(starts, env_records, measured_pid)
            validate_output(rows, contents, plan)
            record.update(
                measured_pid=measured_pid,
                process_start_records=starts,
                pd_env_records=env_records,
                native_launch_count=len(starts),
                rows=rows,
                contents=contents,
                log_sha256=sha256(log_path),
                end_unix=time.time(),
                output_validated=True,
            )
            write_new_json(output / f"{name}.process.json", record)
            written.append(record)
            processes.append(record)
        except Exception as error:
            record.update(end_unix=time.time(), output_validated=False,
                          error=f"{type(error).__name__}: {error}",
                          traceback=traceback.format_exc())
            if log_path.exists():
                record["log_sha256"] = sha256(log_path)
                try:
                    text = log_path.read_text(encoding="utf-8-sig")
                    record["process_start_records"] = marker_records(text, "PD5_PROCESS_START")
                    record["pd_env_lines"] = [
                        line for line in text.splitlines() if line.startswith("PD_ENV ")
                    ]
                    record["native_launch_count"] = len(record["process_start_records"])
                except Exception:
                    pass
            write_new_json(output / f"{name}.process.json", record)
            written.append(record)
            failures.append({"process": name, "error": record["error"]})
            break

    result = {
        "status": "FAILED" if failures else "COMPLETE",
        "run_id": run_id,
        "plan_sha256": sha256(PLAN_PATH),
        "process_count_expected": 3,
        "process_count_completed": len(processes),
        "native_start_records": sum(row.get("native_launch_count", 0) for row in written),
        "failures": failures,
        "formal_acceptance": False,
        "performance_acceptance": False,
        "candidate_comparison": False,
        "per_block_qpc_anchors_available": False,
        "quantiles_computed": False,
        "raw_samples_preserved": True,
        "ended_unix": time.time(),
    }
    if not failures and len(processes) == 3:
        result["descriptive_process_summaries"] = summarize(processes)
        result["process_identities"] = [
            {
                "index": row["index"],
                "pid": row["measured_pid"],
                "qpc": next(item["qpc"] for item in row["process_start_records"]
                            if item.get("pid") == row["measured_pid"]),
                "qpc_frequency": next(item["frequency"] for item in row["process_start_records"]
                                      if item.get("pid") == row["measured_pid"]),
                "log_sha256": row["log_sha256"],
            }
            for row in processes
        ]
    write_new_json(output / "collector-result.json", result)
    return 1 if failures or len(processes) != 3 else 0


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--capture-receipt", type=Path, required=True)
    parser.add_argument("--wpr", type=Path, required=True)
    parser.add_argument("--wpr-instance", required=True)
    parser.add_argument("--controller-claim-sha256", required=True)
    args = parser.parse_args()
    try:
        raise SystemExit(main(args))
    except Exception as error:
        output = args.output.resolve()
        if output.is_dir() and not (output / "collector-fatal.json").exists():
            try:
                write_new_json(output / "collector-fatal.json", {
                    "status": "FAILED",
                    "error": f"{type(error).__name__}: {error}",
                    "traceback": traceback.format_exc(),
                    "formal_acceptance": False,
                })
            except Exception:
                pass
        raise

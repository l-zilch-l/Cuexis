"""Run a sealed fixed group once. No retries, sample replacement or automatic budget changes."""
import argparse
import datetime as dt
from pathlib import Path
import time

from tools.verification.playback_performance import measurement, process_control

from . import prepare_stream, probe_units
from .admission import read_sealed
from .binding import read, sha256, verify_artifact, write_once
from .results import verdict
from .environment import process_environment, verify_cpu, verify_native, verify_warmup
from .runtime_monitor import POLICY, RuntimeMonitor

PROBE_GATES = probe_units.GATES
PROBE_IDENTITY = ("mode", "harness", "frames", "warmup_frames", "updates", "materializations",
                  "capture_consumers", "renderer_consumers", "base_entities", "max_entities",
                  "payload_charge_per_frame", "gameplay", "audio", "gpu", "closed")


def utc():
    return dt.datetime.now(dt.timezone.utc).isoformat()


def process_order(comparison, window):
    if comparison == "B-only":
        return [(index, "B") for index in range(3)]
    if comparison == "A/B":
        orientations = ("AB", "BA", "AB") if window != 1 else ("BA", "AB", "BA")
        return [(index, side) for index, order in enumerate(orientations) for side in order]
    raise ValueError("P8 requires its separate single-invocation runner")


def parse_result(unit, text, exit_code):
    if unit["adapter"] == "owner":
        if unit.get("gate") not in ("p0-legacy", "p0-rpa"):
            raise ValueError("owner adapter requires its canonical legacy/RPA gate")
        parsed = measurement.parse_owner(text, exit_code=exit_code, phase="formal")
    elif unit["adapter"] == "host-common":
        parsed = measurement.parse_host(text, exit_code=exit_code)
    elif unit["adapter"] == "probe":
        if unit.get("gate") not in PROBE_GATES:
            raise ValueError("probe adapter requires its canonical presentation-matrix gate")
        parsed = measurement.parse_probe(text, exit_code=exit_code)
    elif unit["adapter"] == "controls":
        if unit.get("gate") != "p5-controls":
            raise ValueError("controls adapter requires the canonical scripted-control gate")
        parsed = measurement.parse_probe(text, exit_code=exit_code)
    elif unit["adapter"] == "stream":
        prepare_stream.validate_registration(unit)
        parsed = measurement.parse_probe(text, exit_code=exit_code)
        prepare_stream.identity(parsed)
    elif unit["adapter"] == "seek":
        if unit.get("gate") not in SEEK_GATES:
            raise ValueError("seek adapter requires a pre-declared Seek gate")
        parsed = measurement.parse_seek(text, exit_code=exit_code)
    else:
        raise ValueError("unimplemented result adapter cannot launch a formal process")
    expected = unit["expected_identity"]
    required = ({"digest", "replay_sha256", "snapshot_sha256", "packed_sha256", "profile"}
                if unit["adapter"] == "owner" else
                set(PROBE_IDENTITY) if unit["adapter"] == "probe" else
                {"mode", "cycles", "rate", "counts"} if unit["adapter"] == "controls" else
                set(prepare_stream.IDENTITY_FIELDS) if unit["adapter"] == "stream" else
                set(SEEK_IDENTITY) if unit["adapter"] == "seek" else
                {"digest", "source", "viewport", "objects", "warmup", "measured", "audio", "gpu"})
    if not required.issubset(expected):
        raise ValueError("incomplete preregistered semantic identity")
    if unit["adapter"] == "owner" and expected["profile"] != unit["gate"].removeprefix("p0-"):
        raise ValueError("owner gate/profile identity differs")
    if any(parsed["identity"].get(key) != value for key, value in expected.items()):
        raise ValueError("native result differs from preregistered workload semantics")
    count = (parsed["statistics"]["validated_trace"]["n"] if unit["adapter"] == "owner"
             else parsed["identity"]["frames"] if unit["adapter"] == "probe"
             else len(parsed["samples"]))
    if count != unit["requests_per_process"]:
        raise ValueError("native request/block count differs from registration")
    values = {}
    for metric in unit["required_metrics"]:
        values[metric] = parsed["statistics"][metric]["median_us"]
        measurement.positive(values[metric])
    absolute = {}
    if unit["adapter"] == "host-common":
        samples = [row["request_to_ready_us"] for row in parsed["samples"]]
        absolute["request_to_ready"] = measurement.budget("request_to_ready", samples,
                                                         config=unit["config"])
    if unit["adapter"] == "seek":
        samples = parsed["samples"]
        absolute["seek"] = measurement.budget(
            "seek", [row["request_to_present_us"] for row in samples], config=unit["config"])
        absolute["request_to_ready"] = measurement.budget(
            "request_to_ready", [row["request_to_ready_us"] for row in samples],
            config=unit["config"])
    if unit["adapter"] == "probe":
        # Only a metric with an accepted absolute budget may carry one; a missing budget stays
        # unjudged instead of borrowing the relative threshold.
        for metric in unit.get("absolute_gate_metrics", []):
            samples = [row["elapsed_us"] for row in parsed["samples"] if row["metric"] == metric]
            absolute[metric] = measurement.budget(metric, samples, config=unit["config"])
    if unit["adapter"] in ("controls", "stream"):
        metrics = (("pause", "resume", "resize", "reset_paused", "reset_running", "close", "load")
                   if unit["adapter"] == "controls" else ("request_to_ready", "close", "load"))
        for metric in metrics:
            samples = [row["elapsed_us"] for row in parsed["samples"] if row["metric"] == metric]
            if not samples:
                raise ValueError("missing registered absolute endpoint: " + metric)
            # load_host intentionally has no budget: load uses the native process-start anchor.
            absolute[metric] = measurement.budget(metric, samples, config=unit["config"])
    return {"medians": values, "parsed": parsed, "absolute_gates": absolute}


SEEK_GATES = ("p5-seek-natural", "p5-seek-hot", "p5-seek-external", "p5-seek-fallback",
              "p5-seek-cold")
SEEK_IDENTITY = ("schema", "records", "seeks", "seek_class")
CONTROLS_IDENTITY = ("mode", "frames", "warmup_frames", "cycles", "rate", "audio",
                     "closed", "replay_equal", "counts")


def require_same_workload(unit, reference, current):
    """Every process in a group must have measured the same workload, not merely the same name.

    The control and Seek probes declare their own identities, so comparing them through the
    presentation-matrix key set would reject correct logs; everything else keeps that comparison.
    """
    keys = (CONTROLS_IDENTITY if unit["adapter"] == "controls"
            else prepare_stream.IDENTITY_FIELDS if unit["adapter"] == "stream"
            else SEEK_IDENTITY if unit["adapter"] == "seek" else None)
    if keys is None:
        for metric in unit["required_metrics"]:
            measurement.require_comparable(reference, current, metric)
        return
    for key in keys:
        if reference.get("identity", {}).get(key) != current.get("identity", {}).get(key):
            raise ValueError("workload identity differs from the registered semantics: " + key)


def run(seal_path, output):
    seal_path, output = Path(seal_path).resolve(), Path(output).resolve()
    preparation = read_sealed(seal_path)
    adapters = ("owner", "host-common", "probe", "controls", "stream", "seek")
    if any(unit.get("adapter") not in adapters
           for unit in preparation["units"]):
        raise ValueError("result adapter not admitted; no process started")
    for unit in preparation["units"]:
        if unit.get("runtime_monitor_policy") != POLICY:
            raise ValueError("runtime monitoring policy must be frozen before execution")
        if unit["workload"] == "p8" or unit["comparison"] == "P8-single-sided":
            raise ValueError("P8 cannot use the timing group runner")
        if ((unit["workload"] not in ("steady", "seek")) or
                (unit["adapter"] == "controls" and unit.get("gate") != "p5-controls") or
                (unit["adapter"] == "stream" and unit.get("gate") != prepare_stream.GATE) or
                (unit["adapter"] == "seek" and unit.get("gate") not in SEEK_GATES) or
                (unit["adapter"] == "host-common" and unit.get("gate") != "p0-host-common") or
                (unit["adapter"] == "owner" and
                 unit.get("gate") not in ("p0-legacy", "p0-rpa")) or
                (unit["adapter"] == "probe" and unit.get("gate") not in PROBE_GATES)):
            raise ValueError("adapter does not implement the registered workload/gate")
        if unit["measurement_mode"] != "normal":
            raise ValueError("instrumented runs require their dedicated adapter")
        if unit["adapter"] == "probe" and unit.get("comparison") != "B-only":
            raise ValueError("presentation-matrix probes are a B-only structural/timing group")
        if unit["adapter"] == "stream":
            prepare_stream.validate_registration(unit)
        for side in (("A", "B") if unit["comparison"] == "A/B" else ("B",)):
            launch = unit["launch"][side]
            key = unit["baseline_binary"] if side == "A" else unit["candidate_binary"]
            binary = verify_artifact(preparation["binary_bindings"][key])
            if Path(launch["command"][0]).resolve() != binary:
                raise ValueError("launch command does not use the exact bound executable")
            if (launch.get("warmup_seconds") != 30 or type(launch.get("timeout_seconds")) is not int or
                    not 1 <= launch["timeout_seconds"] <= 3600 or
                    type(launch.get("affinity")) is not int or launch["affinity"] <= 0):
                raise ValueError("warmup, timeout or affinity not frozen")
            if not launch.get("warmup_command"):
                raise ValueError("missing machine warmup command")
            warmup = Path(launch["warmup_command"][0]).resolve()
            if warmup not in {Path(row["path"]).resolve() for row in preparation["binary_bindings"].values()}:
                raise ValueError("warmup executable has not been bound")
            environment = read(verify_artifact(unit["environment_binding"]))["cpu_environment"]
            expected_affinity = (environment["topology"]["candidate_masks"][0]
                                 if unit["adapter"] == "owner"
                                 else environment["machine"]["parent_affinity"])
            if launch["affinity"] != expected_affinity:
                raise ValueError("affinity violates the original topology selection policy")
    # The marker is next to the immutable seal. A crash consumes this registration.
    write_once(seal_path.with_suffix(".running.json"),
               {"schema": "rpa.execution.running.v1", "started": utc(), "output": str(output),
                "seal_sha256": sha256(seal_path)})
    output.mkdir(parents=True, exist_ok=False)
    data = {unit["id"]: [[{} for _ in range(3)] for _ in range(3)]
            for unit in preparation["units"]}
    previous_window_end = None
    native_settings = None
    semantic_references = {}
    absolute_results = {unit["id"]: [] for unit in preparation["units"]}
    try:
        for window in range(3):
            if previous_window_end is not None:
                deadline = previous_window_end + preparation["gap_seconds"]
                while time.monotonic() < deadline:
                    time.sleep(min(30, deadline - time.monotonic()))
            write_once(output / f"window-{window + 1}-start.json", {"utc": utc()})
            for unit in preparation["units"]:
                for pair, side in process_order(unit["comparison"], window):
                    # Recheck immutable artifacts before every process; environmental runtime
                    # anchors inside the native log remain required by its parser/admission.
                    read_sealed(seal_path)
                    launch = unit["launch"][side]
                    expected_environment = read(verify_artifact(unit["environment_binding"]))["cpu_environment"]
                    before_environment = verify_cpu(expected_environment)
                    directory = output / unit["id"] / f"window-{window + 1}" / f"pair-{pair + 1}-{side}"
                    directory.mkdir(parents=True, exist_ok=False)
                    write_once(directory / "environment-before.json", before_environment)
                    env = process_environment()
                    env.update(launch["environment"])
                    for phase, command in (("warmup", launch["warmup_command"]),
                                           ("sample", launch["command"])):
                        log = directory / (phase + ".log")
                        started = utc()
                        monitor = RuntimeMonitor(expected_environment["foreground_pid"],
                                                 timeout=launch["timeout_seconds"] + 5)
                        try:
                            with log.open("xb") as stream, monitor:
                                result = process_control.run(command, launch["affinity"],
                                    cwd=launch["cwd"], env=env, output=stream,
                                    timeout=launch["timeout_seconds"])
                        except Exception as error:
                            write_once(directory / (phase + "-runtime-environment.json"), monitor.result())
                            write_once(directory / (phase + "-error.json"),
                                       {"started": started, "ended": utc(), "error": str(error),
                                        "log_sha256": sha256(log) if log.exists() else None})
                            raise
                        monitored = monitor.result()
                        write_once(directory / (phase + "-runtime-environment.json"), monitored)
                        write_once(directory / (phase + ".json"),
                                   {"started": started, "ended": utc(), "command": command,
                                    "exit_code": result.returncode, "log_sha256": sha256(log)})
                        if result.returncode:
                            raise RuntimeError("native process failed; no sample replacement")
                        if not monitored["passed"]:
                            raise RuntimeError("runtime environment changed or monitoring was incomplete; no replacement")
                        if phase == "warmup":
                            verify_warmup(log.read_text(encoding="utf-8"))
                    write_once(directory / "environment-after.json", verify_cpu(expected_environment))
                    native_settings = verify_native(log.read_text(encoding="utf-8"),
                                                    launch["affinity"], native_settings)
                    parsed = parse_result(unit, log.read_text(encoding="utf-8", errors="strict"), 0)
                    if unit["id"] in semantic_references:
                        require_same_workload(unit, semantic_references[unit["id"]],
                                              parsed["parsed"])
                    else:
                        semantic_references[unit["id"]] = parsed["parsed"]
                    data[unit["id"]][window][pair][side] = parsed["medians"]
                    absolute_results[unit["id"]].append({"window": window + 1, "pair": pair + 1,
                                                         "side": side,
                                                         "gates": parsed["absolute_gates"]})
                    write_once(directory / "parsed.json", parsed)
                    print(utc(), unit["id"], window + 1, pair + 1, side, flush=True)
            previous_window_end = time.monotonic()
            write_once(output / f"window-{window + 1}-end.json", {"utc": utc()})
        result = {unit["id"]: verdict(data[unit["id"]], unit["comparison"])
                  for unit in preparation["units"]}
        for key, value in result.items():
            value["scope"] = "relative_cost_and_repeatability_only"
            value["overall_acceptance"] = "requires_all_registered_absolute_and_other_gates"
            value["per_process_absolute_gates"] = absolute_results[key]
        write_once(output / "result.json", result)
        return result
    except Exception as error:
        # A stopped group must keep the absolute gates it already computed; otherwise a failed
        # window silently discards judged per-process evidence that the raw logs still support.
        write_once(output / "stopped.json", {"utc": utc(), "error": str(error),
                                            "partial_results": data,
                                            "partial_absolute_gates": absolute_results,
                                            "status": "execution_or_semantic_failure"})
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seal", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.seal, args.output)


if __name__ == "__main__":
    main()

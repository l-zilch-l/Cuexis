"""Reduce the 40K start-up evidence to one table, re-deriving every number from the raw files.

The table exists so that the report can cite numbers that are computed rather than remembered. It
reads only files that are archived beside it, recomputes the aggregates that the run drivers
already printed, and records any disagreement instead of silently preferring one side:

  probe groups (runs/20261101-a, -a2, -b of r3-startup-observability-v1)
      Per-arm medians of every GPU_STARTUP_END stage and every PREPARE_TRACE stage, the arm's
      first_present total, and the run order. The order is derived from samples[], not from the
      plan, because the difference between -a (armed in blocks) and -a2 (armed sample by sample)
      is exactly what those two stamps exist to separate.

  trace perturbation
      The untraced/traced pair per index, on the same binary in the same project, for the probe
      groups and for the player groups. A paired difference is the only form in which the trace
      cost is separable from the run-to-run variation.

  entity scaling
      playback_prepare against entity count for the four traced probe arms of -b plus the 40000
      arm of -a2, so prepare cost per entity can be read off directly.

  watcher groups (runs/20261101-w1, -w2)
      Window appearance, first frame, probe latency and CPU share per arm. The player's own
      [player.startup_phase] lines are re-parsed from the archived .out.txt of every process and
      compared with the aggregate the driver recorded; a mismatch is reported, not smoothed.

  serializer before/after (r3-prepare-cost-v1)
      The stage split of both stamps, their self times, per-process first-frame totals and the
      distance to the 2000 ms first-frame gate.

  bindings
      Every executable digest that a run recorded about itself, with the file and the field that
      recorded it, and the digest of the binaries still on disk. Nothing is inferred: every arm
      either wrote a digest per process or its group wrote one digest for the whole group.

Usage (from D:\\Cuexis-redesign):
    python -B tabulate_startup.py
"""

import datetime
import hashlib
import json
import pathlib
import statistics
import sys

S1 = pathlib.Path("out/verification-20261101/r3-startup-observability-v1")
S2 = pathlib.Path("out/verification-20261101/r3-prepare-cost-v1")
OUT = pathlib.Path("out/verification-20261101/evidence-staging/r3-40k-startup/derived")

PROBE = pathlib.Path("out/build/rpa-candidate-release/bin/cuexis_execution_device_probe.exe")
PLAYER = pathlib.Path("out/build/rpa-candidate-release/bin/cuexis_player.exe")

REPEATABILITY_LIMIT_PCT = 15.0
REPEATABILITY_MIN_US = 1000.0
CLAIM_QUANTITIES = ("first_present_us", "stage:playback_prepare")
FIRST_FRAME_GATE_US = 2_000_000

PROBE_STAMPS = ["20261101-a", "20261101-a2", "20261101-b"]
WATCH_STAMPS = ["20261101-w1", "20261101-w2"]
SUPERSEDED_PROBE_GROUPS = {
    "20261101-a": (
        "armed in blocks and measured first, so the cold file cache and a machine state that had "
        "not yet settled are inside every number; 20261101-a2 asks the same question with the arms "
        "alternating sample by sample"
    ),
}
FOCUS_STAGES = [
    "json_dom_parse",
    "chart_typed_read",
    "chart_canonical_normalize",
    "chart_canonical_dump",
    "chart_canonical_write",
    "load_canonical",
    "load_chart",
    "load_document",
    "chart_compile",
    "runtime",
    "snapshot_layout",
    "presentation",
    "assemble",
]
PROBE_STAGE_ORDER = [
    "fixture_source",
    "playback_prepare",
    "sdl_runtime",
    "gl_context",
    "window_create",
    "backend_create",
    "pixel_readback",
    "renderer_prepare",
    "session_commit",
    "backend_activate",
    "first_update",
    "first_extract",
    "first_submit",
    "first_present",
]
PLAYER_PHASES = ["content_prepared", "content_committed", "first_frame_presented"]


def sha256_of(path):
    with open(path, "rb") as stream:
        return hashlib.sha256(stream.read()).hexdigest()


def read_json(path):
    with open(path, "rb") as stream:
        return json.loads(stream.read().decode("utf-8"))


def median_of(values):
    numbers = [value for value in values if value is not None]
    return round(statistics.median(numbers), 1) if numbers else None


def spread_pct(values):
    numbers = [value for value in values if value is not None]
    if len(numbers) < 2:
        return None
    median = statistics.median(numbers)
    if not median:
        return None
    return round(100.0 * (max(numbers) - min(numbers)) / median, 2)


def describe(values):
    numbers = [value for value in values if value is not None]
    return {
        "n": len(numbers),
        "median": median_of(numbers),
        "min": min(numbers) if numbers else None,
        "max": max(numbers) if numbers else None,
        "spread_pct": spread_pct(numbers),
        "values": [round(value, 1) for value in numbers],
    }


def parse_startup_phases(text):
    """Parse the player's own [player.startup_phase] lines: name -> {elapsed_us, total_us}."""
    phases = {}
    for line in text.splitlines():
        if "[player.startup_phase]" not in line:
            continue
        parts = line.split("[player.startup_phase]", 1)[1].split()
        if len(parts) >= 5 and parts[1] == "elapsed_us" and parts[3] == "total_us":
            phases[parts[0]] = {"elapsed_us": float(parts[2]), "total_us": float(parts[4])}
    return phases


def trace_scope_names(events):
    """Union of prepare-trace scope names, from either capture shape.

    The start-up probe records the engine's prepare trace as a JSON array of {event, stage,
    elapsed_us}; the watcher captures the same trace from stderr as PREPARE_TRACE_END lines. The
    names are the instrument's vocabulary, so a rebuild that keeps them keeps the numbers
    comparable in form.
    """
    names = set()
    for event in events or []:
        if isinstance(event, dict):
            if event.get("event") in (None, "end") and event.get("stage"):
                names.add(str(event["stage"]))
            continue
        parts = str(event).split()
        if len(parts) >= 2 and parts[0] == "PREPARE_TRACE_END":
            names.add(parts[1])
    return sorted(names)


def sample_files(directory):
    """Group <arm>-<NN>.out.txt by arm label, in index order."""
    arms = {}
    for path in sorted(pathlib.Path(directory).glob("*.out.txt")):
        stem = path.name[: -len(".out.txt")]
        label, _sep, index = stem.rpartition("-")
        if not label:
            continue
        arms.setdefault(label, []).append((index, path))
    return {label: [path for _index, path in sorted(pairs)] for label, pairs in arms.items()}


# --------------------------------------------------------------------------------------------------
# probe groups
# --------------------------------------------------------------------------------------------------


def probe_stamp(stamp):
    payload = read_json(S1 / "runs" / stamp / "startup-set.json")
    samples = payload["samples"]
    order = [sample["arm"] for sample in samples]
    switches = sum(1 for left, right in zip(order, order[1:]) if left != right)
    arms = {}
    for arm, spec in payload["arms"].items():
        entities, traced, mode = spec[0], spec[1], spec[2]
        recorded = payload["summary"].get(arm)
        if recorded is None:
            continue
        mine = [sample for sample in samples if sample["arm"] == arm]
        first_present = [sample["extra"].get("total", {}).get("first_present_us") for sample in mine]
        shown = [sample["extra"].get("shown_us") for sample in mine]
        arms[arm] = {
            "entities": entities,
            "mode": mode,
            "traced": traced,
            "return_codes": sorted({sample["returncode"] for sample in mine}),
            "n": len(mine),
            "wall_s": describe([sample["wall_s"] for sample in mine]),
            "first_present_us": describe(first_present),
            "shown_us": describe(shown),
            "stages_us": {
                stage: {
                    "median": value["median"],
                    "min": value["min"],
                    "max": value["max"],
                    "spread_pct": value["spread_pct"],
                    "n": value["n"],
                }
                for stage, value in sorted(
                    recorded["stages"].items(),
                    key=lambda pair: PROBE_STAGE_ORDER.index(pair[0])
                    if pair[0] in PROBE_STAGE_ORDER
                    else 99,
                )
            },
            "trace_stages_us": {
                stage: {
                    "median": value["median"],
                    "min": value["min"],
                    "max": value["max"],
                    "spread_pct": value["spread_pct"],
                    "n": value["n"],
                }
                for stage, value in sorted(recorded["trace_stages"].items())
            },
        }
    return {
        "stamp": stamp,
        "exe": payload["exe"],
        "exe_sha256": payload["exe_sha256"],
        "plan": payload["plan"],
        "samples": len(samples),
        "order": order,
        "arm_switches": switches,
        "interleaved": switches == len(samples) - 1 and len(set(order)) > 1,
        "trace_scopes": trace_scope_names(
            [
                event
                for sample in samples
                if sample.get("traced")
                for event in (sample.get("trace_events") or [])
            ]
        ),
        "arms": arms,
    }


def paired_delta(samples, arm_a, arm_b, path):
    """Pair samples of two arms by their run index and return the paired differences."""
    def value(sample):
        node = sample
        for key in path:
            if node is None:
                return None
            node = node.get(key) if isinstance(node, dict) else None
        return node

    left = {sample["index"]: value(sample) for sample in samples if sample["arm"] == arm_a}
    right = {sample["index"]: value(sample) for sample in samples if sample["arm"] == arm_b}
    pairs = []
    for index in sorted(set(left) & set(right)):
        if left[index] is None or right[index] is None:
            continue
        pairs.append(
            {
                "index": index,
                arm_a: round(left[index], 1),
                arm_b: round(right[index], 1),
                "delta": round(right[index] - left[index], 1),
                "delta_pct": round(100.0 * (right[index] - left[index]) / left[index], 3),
            }
        )
    deltas = [pair["delta"] for pair in pairs]
    return {
        "arm_a": arm_a,
        "arm_b": arm_b,
        "metric": ".".join(path),
        "pairs": pairs,
        "median_delta": median_of(deltas),
        "median_delta_pct": round(
            100.0 * statistics.median(deltas) / statistics.median([pair[arm_a] for pair in pairs]), 3
        )
        if pairs and statistics.median([pair[arm_a] for pair in pairs])
        else None,
        "max_abs_delta": max((abs(delta) for delta in deltas), default=None),
    }


def probe_perturbation(stamp):
    payload = read_json(S1 / "runs" / stamp / "startup-set.json")
    samples = payload["samples"]
    untraced = next((arm for arm, spec in payload["arms"].items() if not spec[1]), None)
    traced = next((arm for arm, spec in payload["arms"].items() if spec[1] and spec[0] == 40000), None)
    if untraced is None or traced is None:
        return {"stamp": stamp, "available": False}
    prepare = paired_delta(samples, untraced, traced, ["stages_us", "playback_prepare"])
    total = paired_delta(samples, untraced, traced, ["extra", "total", "first_present_us"])
    return {
        "stamp": stamp,
        "available": True,
        "interleaved": sum(
            1 for left, right in zip([s["arm"] for s in samples], [s["arm"] for s in samples][1:])
            if left != right
        )
        == len(samples) - 1,
        "n_per_arm": min(len(prepare["pairs"]), len(total["pairs"])),
        "playback_prepare": prepare,
        "first_present_us": total,
    }


def entity_scaling(blocks):
    points = []
    for block in blocks:
        for arm, data in block["arms"].items():
            if not data["traced"]:
                continue
            prepare = data["stages_us"].get("playback_prepare", {}).get("median")
            if prepare is None:
                continue
            points.append(
                {
                    "entities": data["entities"],
                    "arm": arm,
                    "stamp": block["stamp"],
                    "playback_prepare_median_us": prepare,
                    "us_per_entity": round(prepare / data["entities"], 4),
                    "first_present_us": data["first_present_us"]["median"],
                }
            )
    points.sort(key=lambda point: point["entities"])
    if len(points) >= 2:
        smallest, largest = points[0], points[-1]
        ratio_per_entity = (
            round(largest["us_per_entity"] / smallest["us_per_entity"], 3)
            if smallest["us_per_entity"]
            else None
        )
    else:
        ratio_per_entity = None
    return {"points": points, "us_per_entity_largest_over_smallest": ratio_per_entity}


# --------------------------------------------------------------------------------------------------
# watcher groups
# --------------------------------------------------------------------------------------------------


def watch_stamp(stamp):
    """Re-derive the watcher aggregates from the per-process samples, not from the aggregates.

    watch-set.json keeps every process twice: once as the driver's own summary and once, in
    samples[], as the parsed player phases, probe stages, probe frames and trace events. The
    aggregate player_phases_us is a median over those samples, so recomputing it exercises the
    same path the driver used and catches a mis-transcribed median.

    The child stdout and stderr are persisted per process under raw/<arm>-<NN>.out.txt, so the
    parsed phases of every sample are also checked against the player's own log lines. That is the
    independent path: the summary JSON against the bytes the process wrote.
    """
    directory = S1 / "runs" / stamp
    payload = read_json(directory / "watch-set.json")
    samples = payload["samples"]
    persisted = sorted((directory / "raw").glob("*.out.txt"))
    arms = {}
    for arm, recorded in payload["summary"].items():
        mine = [sample for sample in samples if sample["arm"] == arm]
        phase_totals = {}
        for name in PLAYER_PHASES + ["window_created"]:
            phase_totals[name] = describe(
                [sample["phases"].get(name, {}).get("total_us") for sample in mine]
            )
        phase_elapsed = {
            name: describe([sample["phases"].get(name, {}).get("elapsed_us") for sample in mine])
            for name in PLAYER_PHASES + ["window_created"]
        }
        recorded_phases = recorded.get("player_phases_us") or {}
        matches = {}
        for name in recorded_phases:
            recorded_total = recorded_phases[name].get("total_us")
            derived_total = median_of(
                [sample["phases"].get(name, {}).get("total_us") for sample in mine]
            )
            matches[name] = (
                recorded_total is not None
                and derived_total is not None
                and abs(derived_total - recorded_total) < 1.0
            )
        probe_stages = {}
        for name in (recorded.get("probe_stages_us") or {}):
            probe_stages[name] = {
                "recorded_median": recorded["probe_stages_us"][name],
                "rederived": describe([sample["stages"].get(name) for sample in mine]),
            }
        log_check = []
        for sample in mine:
            path = directory / "raw" / "{}-{:02d}.out.txt".format(sample["arm"], sample["index"])
            entry = {
                "index": sample["index"],
                "log": str(path.name),
                "exists": path.is_file(),
                "phases_in_sample": len(sample["phases"]),
                "phases_in_log": 0,
                "compared": [],
                "disagreements": [],
            }
            if entry["exists"]:
                from_log = parse_startup_phases(path.read_bytes().decode("utf-8", "replace"))
                entry["phases_in_log"] = len(from_log)
                for name, values in from_log.items():
                    if name not in sample["phases"]:
                        continue
                    entry["compared"].append(name)
                    mine_values = sample["phases"][name]
                    for field_name in ("elapsed_us", "total_us"):
                        if abs(values[field_name] - mine_values[field_name]) >= 1.0:
                            entry["disagreements"].append(
                                (name, field_name, values[field_name], mine_values[field_name])
                            )
            log_check.append(entry)
        # The watcher posts WM_CLOSE to whatever it currently believes is the primary window and
        # records wm_close_to_exit_s whether or not a window existed to post to. When the primary
        # window has not appeared yet the post is a no-op, so the field is the remainder of the
        # watcher's timeout rather than a close latency. Derive that per process instead of
        # quoting the field.
        close_events = []
        for sample in mine:
            summary = sample["summary"]
            summary_windows = summary.get("windows") or []
            primary_hwnd = summary.get("primary_hwnd")
            first_seen = None
            for info in summary_windows:
                if info["hwnd"] == primary_hwnd:
                    first_seen = info["first_seen_t_s"]
                    break
            posted_at = summary.get("wm_close_posted_t_s")
            close_events.append(
                {
                    "index": sample["index"],
                    "posted_t_s": posted_at,
                    "to_exit_s": summary.get("wm_close_to_exit_s"),
                    "primary_set_t_s": summary.get("primary_set_t_s"),
                    "primary_first_seen_t_s": first_seen,
                    "return_code": summary.get("return_code"),
                    "killed": summary.get("killed"),
                    "target_existed_at_post": (
                        None
                        if posted_at is None
                        else bool(first_seen is not None and first_seen <= posted_at)
                    ),
                }
            )
        arms[arm] = {
            "n": recorded["n"],
            "return_codes": recorded["return_codes"],
            "killed": recorded["killed"],
            "medians": recorded["medians"],
            "spread_pct": recorded["spread_pct"],
            "probe_latency_ms": recorded["probe_latency_ms"],
            "cpu_core_ratio": recorded["cpu_core_ratio"],
            "recorded_player_phases_us": recorded_phases,
            "phase_totals_us": phase_totals,
            "phase_elapsed_us": phase_elapsed,
            "phase_medians_match_recorded": matches,
            "probe_stages_us": probe_stages,
            "per_process_window_visible_s": [
                sample["summary"].get("primary_visible_t_s") for sample in mine
            ],
            "per_process_exit_s": [sample["summary"].get("exit_t_s") for sample in mine],
            "per_process_close_s": [sample["summary"].get("wm_close_to_exit_s") for sample in mine],
            "per_process_probes_after_primary": [
                sample["summary"].get("probes_after_primary") for sample in mine
            ],
            "per_process_probe_p99_ms": [
                (sample["summary"].get("probe_latency_ms") or {}).get("p99") for sample in mine
            ],
            "per_process_probe_max_ms": [
                (sample["summary"].get("probe_latency_ms") or {}).get("max") for sample in mine
            ],
            "per_process_cpu_1s": [
                (sample["summary"].get("cpu_core_ratio") or {}).get("1s") for sample in mine
            ],
            "frames": [sample.get("frames") for sample in mine],
            "trace_event_counts": [len(sample.get("trace_events") or []) for sample in mine],
            "executables": [
                {"sha256": digest, "bytes": size}
                for digest, size in sorted(
                    {
                        (sample["summary"].get("exe_sha256"), sample["summary"].get("exe_bytes"))
                        for sample in mine
                    },
                    key=lambda pair: (pair[0] or "", pair[1] or 0),
                )
            ],
            "commands": [
                list(item)
                for item in sorted({tuple(sample["summary"].get("command") or []) for sample in mine})
            ],
            "env_overrides": [
                list(item)
                for item in sorted(
                    {tuple(sample["summary"].get("env_overrides") or []) for sample in mine}
                )
            ],
            "started_wall": [sample["summary"].get("started_wall") for sample in mine],
            "stdout_bytes": [sample["summary"].get("stdout_bytes") for sample in mine],
            "stderr_bytes": [sample["summary"].get("stderr_bytes") for sample in mine],
            "log_check": log_check,
            "close_events": close_events,
            "raw_text_persisted": bool(persisted),
        }
    return {
        "stamp": stamp,
        "plan": payload["plan"],
        "order": payload["order"],
        "samples": len(samples),
        "trace_scopes": trace_scope_names(
            [event for sample in samples for event in (sample.get("trace_events") or [])]
        ),
        "arms": arms,
    }


def pilot_block():
    """The single-sample pilots that preceded the registered watcher groups."""
    entries = []
    for path in sorted((S1 / "runs").glob("watch/*.json")) + sorted(
        (S1 / "runs").glob("smoke-w2/*.json")
    ):
        summary = read_json(path)["summary"]
        entries.append(
            {
                "sample": path.relative_to(S1).as_posix(),
                "label": summary.get("label"),
                "exe_sha256": summary.get("exe_sha256"),
                "exe_bytes": summary.get("exe_bytes"),
                "started_wall": summary.get("started_wall"),
                "return_code": summary.get("return_code"),
                "killed": summary.get("killed"),
                "exit_t_s": summary.get("exit_t_s"),
                "env_overrides": summary.get("env_overrides"),
                "window_appeared_t_s": summary.get("window_appeared_t_s"),
                "primary_visible_t_s": summary.get("primary_visible_t_s"),
                "probe_latency_ms": summary.get("probe_latency_ms"),
            }
        )
    return {"samples": entries}


# --------------------------------------------------------------------------------------------------
# serializer before/after
# --------------------------------------------------------------------------------------------------


def stamp_block(path, label):
    payload = read_json(path)
    processes = payload["processes"]
    first_frame = [
        process["phase_lines"].get("first_frame_presented", {}).get("total_us")
        for process in processes
    ]
    content_prepared_phase = [
        process["phase_lines"].get("content_prepared", {}).get("total_us") for process in processes
    ]
    return {
        "label": label,
        "file": str(path),
        "file_sha256": sha256_of(path),
        "stamp": payload.get("stamp"),
        "summarised_at": payload.get("summarised_at"),
        "processes": len(processes),
        "process_labels": [process["label"] for process in processes],
        "return_codes_derived_from": "logs only; the stamp keeps no exit code",
        "content_prepared_us": payload["summary"]["content_prepared_us"],
        "outside_us": payload["summary"]["outside_us"],
        "stages_us": {
            stage: {
                "median_us": value["median_us"],
                "min_us": value["min_us"],
                "max_us": value["max_us"],
                "spread_pct": value["spread_pct"],
                "self_median_us": value["self_median_us"],
                "per_process_us": value["per_process_us"],
            }
            for stage, value in payload["summary"]["stages"].items()
            if stage in FOCUS_STAGES
        },
        "content_prepared_phase_us": describe(content_prepared_phase),
        "first_frame_presented_total_us": describe(first_frame),
        "environment": payload.get("environment", {}),
        "binding_recovery": payload.get("binding_recovery", {}),
    }


def serializer_block():
    before = stamp_block(S2 / "baseline" / "prepare-split.json", "before: nlohmann DOM dump")
    after = stamp_block(S2 / "runs" / "serializer-direct-v1" / "prepare-split.json", "after: direct writer")
    compare = read_json(S2 / "after" / "prepare-split-compare.json")
    median_after = after["first_frame_presented_total_us"]["median"]
    median_before = before["first_frame_presented_total_us"]["median"]
    return {
        "before": before,
        "after": after,
        "compare": compare,
        "first_frame": {
            "before_median_us": median_before,
            "after_median_us": median_after,
            "delta_us": round(median_after - median_before, 1),
            "delta_pct": round(100.0 * (median_after - median_before) / median_before, 1),
            "gate_us": FIRST_FRAME_GATE_US,
            "gap_to_gate_ms": round((median_after - FIRST_FRAME_GATE_US) / 1000.0, 1),
            "gap_to_gate_pct": round(100.0 * (median_after - FIRST_FRAME_GATE_US) / FIRST_FRAME_GATE_US, 1),
            "gate_note": (
                "the 2000 ms gate is the R3 clamp on a first load (process start to first valid "
                "frame) from the stage plan; this arm is a cache-uncontrolled measurement, not a "
                "disk-cold one"
            ),
        },
    }


# --------------------------------------------------------------------------------------------------
# bindings
# --------------------------------------------------------------------------------------------------


def iso_from_mtime(path):
    return datetime.datetime.fromtimestamp(pathlib.Path(path).stat().st_mtime).strftime(
        "%Y-%m-%dT%H:%M:%S"
    )


def build_lineage(probe_blocks, watch_blocks, pilots):
    """Every executable digest visible in the evidence, oldest observation first.

    A group-level stamp records its digest once for all of its processes, so its observation time
    is the moment the driver wrote the summary, while a per-process sample records the moment that
    process started. Both are local wall clock; the kind is kept in when_kind.
    """
    lineage = []

    def add(digest, size, when, when_kind, source, method, note):
        for entry in lineage:
            if entry["sha256"] == digest:
                entry["observed"].append(
                    {"when": when, "when_kind": when_kind, "source": source, "method": method}
                )
                return
        lineage.append(
            {
                "sha256": digest,
                "bytes": size,
                "observed": [
                    {"when": when, "when_kind": when_kind, "source": source, "method": method}
                ],
                "note": note,
            }
        )

    for block in probe_blocks:
        source = "runs/{}/startup-set.json".format(block["stamp"])
        add(
            block["exe_sha256"],
            None,
            iso_from_mtime(S1 / "runs" / block["stamp"] / "startup-set.json"),
            "file_mtime",
            source,
            "recorded",
            "probe binary, one digest for every process of {}".format(block["stamp"]),
        )
    for entry in pilots["samples"]:
        when = datetime.datetime.fromtimestamp(float(entry["started_wall"])).strftime(
            "%Y-%m-%dT%H:%M:%S"
        )
        add(
            entry["exe_sha256"],
            entry["exe_bytes"],
            when,
            "process_start",
            entry["sample"],
            "recorded",
            "player binary, single pilot process ({}), exit {}".format(
                entry["env_overrides"], entry["return_code"]
            ),
        )
    for block in watch_blocks:
        for arm, data in block["arms"].items():
            starts = [value for value in data["started_wall"] if value is not None]
            when = (
                datetime.datetime.fromtimestamp(min(starts)).strftime("%Y-%m-%dT%H:%M:%S")
                if starts
                else iso_from_mtime(S1 / "runs" / block["stamp"] / "watch-set.json")
            )
            for item in data["executables"]:
                if not item["sha256"]:
                    continue
                add(
                    item["sha256"],
                    item["bytes"],
                    when,
                    "process_start" if starts else "file_mtime",
                    "runs/{}/watch-set.json samples[{}]".format(block["stamp"], arm),
                    "recorded",
                    "one digest for all {} process(es) of arm {}".format(data["n"], arm),
                )
    for path in sorted((S1 / "runs" / "20261101-c" / "raw").glob("*.json")):
        summary = read_json(path)["summary"]
        when = datetime.datetime.fromtimestamp(float(summary["started_wall"])).strftime(
            "%Y-%m-%dT%H:%M:%S"
        )
        add(
            summary["exe_sha256"],
            summary["exe_bytes"],
            when,
            "process_start",
            str(path).replace("\\", "/"),
            "recorded",
            "player binary, writer-split (before) arm",
        )
    after_stamp = S2 / "runs" / "serializer-direct-v1" / "prepare-split.json"
    after = read_json(after_stamp)
    after_logs = sorted((S2 / "runs" / "serializer-direct-v1").glob("player-40k-traced-*.err.txt"))
    add(
        after["environment"]["player_sha256"],
        after["environment"]["player_bytes"],
        iso_from_mtime(after_logs[0]) if after_logs else iso_from_mtime(after_stamp),
        "run_file_mtime" if after_logs else "file_mtime",
        "r3-prepare-cost-v1/runs/serializer-direct-v1/player-40k-traced-00.err.txt environment",
        "recorded",
        "player binary, direct-writer (after) arm; hashed in-run, bracketed by an identity guard. "
        "The stamp itself was written at {}, after the last process exited ({}), so the first "
        "launch log is the earlier observation".format(
            iso_from_mtime(after_stamp), iso_from_mtime(after_logs[-1]) if after_logs else "unknown"),
    )
    return {
        "entries": sorted(lineage, key=lambda entry: entry["observed"][0]["when"]),
        "disk_now": {
            "probe": {"path": str(PROBE), "sha256": sha256_of(PROBE), "bytes": PROBE.stat().st_size},
            "player": {
                "path": str(PLAYER),
                "sha256": sha256_of(PLAYER),
                "bytes": PLAYER.stat().st_size,
            },
        },
        "note": (
            "Every entry is a digest a run wrote about itself. The watcher groups record one digest "
            "per process in samples[].summary.exe_sha256, so w1 and w2 are bound by what their own "
            "processes reported rather than by inference from a neighbouring group. No binary is "
            "archived: a rebuild replaces the file in place and MSVC links are not byte-"
            "reproducible, so the reproducible artifact is the instrumented source in git plus "
            "these digests."
        ),
        "watch_binding": {
            block["stamp"]: {
                "method": (
                    "recorded"
                    if all(
                        len(data["executables"]) == 1 and data["executables"][0]["sha256"]
                        for data in block["arms"].values()
                    )
                    else "incomplete"
                ),
                "processes": block["samples"],
                "arms": {
                    arm: {
                        "n": data["n"],
                        "executables": data["executables"],
                        "command": (
                            data["commands"][0]
                            if len(data["commands"]) == 1
                            else data["commands"]
                        ),
                        "env_overrides": data["env_overrides"],
                        "first_started_iso": (
                            datetime.datetime.fromtimestamp(
                                min(value for value in data["started_wall"] if value is not None)
                            ).strftime("%Y-%m-%dT%H:%M:%S")
                            if any(value is not None for value in data["started_wall"])
                            else None
                        ),
                        "stdout_bytes": data["stdout_bytes"],
                        "stderr_bytes": data["stderr_bytes"],
                    }
                    for arm, data in block["arms"].items()
                },
                "basis": (
                    "watch-set.json samples[].summary.exe_sha256 / exe_bytes / started_wall / "
                    "command / env_overrides, written by watch_window.py for every process"
                ),
            }
            for block in watch_blocks
        },
    }


# --------------------------------------------------------------------------------------------------
# checks
# --------------------------------------------------------------------------------------------------


def check(name, expected, actual, ok, note=""):
    return {"name": name, "expected": expected, "actual": actual, "ok": bool(ok), "note": note}


def finding(what, subject, detail, note, kind):
    """A recorded fact that is not a protocol violation.

    Checks must hold for the evidence to be usable. Findings are the divergences, confounds and
    instrument limits that the report has to keep saying out loud: they are not failures of the
    tabulation, and turning them into passing checks would hide them.
    """
    return {"what": what, "subject": subject, "detail": detail, "note": note, "kind": kind}


def build_checks(probe_blocks, perturbations, watch_blocks, scaling, serializer, pilots):
    checks = []
    findings = []
    digests = {block["stamp"]: block["exe_sha256"] for block in probe_blocks}
    checks.append(
        check(
            "probe digest identical across a, a2, b",
            sorted(set(digests.values()))[0] if digests else None,
            digests,
            len(set(digests.values())) == 1,
        )
    )
    disk = sha256_of(PROBE)
    if digests and digests.get("20261101-a2") != disk:
        w1_block = next(
            (block for block in watch_blocks if block["stamp"] == "20261101-w1"), None
        )
        w1_probe = [
            entry
            for arm, data in (w1_block["arms"].items() if w1_block else [])
            for entry in data["executables"]
            if arm.startswith("probe-")
        ]
        measured_scopes = next(
            (block["trace_scopes"] for block in probe_blocks if block["stamp"] == "20261101-a2"),
            [],
        )
        w1_scopes = next(
            (block["trace_scopes"] for block in watch_blocks if block["stamp"] == "20261101-w1"),
            [],
        )
        findings.append(
            finding(
                "the probe on disk is not the probe the groups measured",
                "cuexis_execution_device_probe.exe",
                {
                    "measured": digests.get("20261101-a2"),
                    "watcher_group_w1": w1_probe[0] if w1_probe else None,
                    "on_disk": disk,
                    "on_disk_bytes": PROBE.stat().st_size,
                    "trace_scopes_identical": bool(measured_scopes)
                    and measured_scopes == w1_scopes,
                    "trace_scope_count": len(measured_scopes),
                },
                "the probe links cuexis_json_support and the engine, so the direct writer and the "
                "chart trace scopes relinked it without a probe source change; the w1 probe is a "
                "third digest again, and its prepare-trace vocabulary is identical to the group "
                "probe's, so the watcher numbers stay comparable in form while every number in "
                "this table still belongs to the digest that produced it. Attribute nothing to "
                "the binary now on disk without a fresh group",
                kind="binding_divergence",
            )
        )
    for block in probe_blocks:
        codes = {
            arm: data["return_codes"] for arm, data in block["arms"].items()
        }
        checks.append(
            check(
                "{} return codes".format(block["stamp"]),
                [0],
                codes,
                all(codes[arm] == [0] for arm in codes),
            )
        )
    a = next(block for block in probe_blocks if block["stamp"] == "20261101-a")
    a2 = next(block for block in probe_blocks if block["stamp"] == "20261101-a2")
    b = next(block for block in probe_blocks if block["stamp"] == "20261101-b")
    checks.append(
        check(
            "20261101-a is armed in blocks",
            "arm_switches == 1",
            a["arm_switches"],
            a["arm_switches"] == 1,
            "batched order is the confound a2 removes",
        )
    )
    checks.append(
        check(
            "20261101-a2 alternates every sample",
            "arm_switches == samples - 1",
            a2["arm_switches"],
            a2["arm_switches"] == a2["samples"] - 1,
        )
    )
    checks.append(
        check(
            "20261101-b is armed in blocks",
            "arm_switches == 3",
            b["arm_switches"],
            b["arm_switches"] == 3,
        )
    )
    for perturbation in perturbations:
        if not perturbation.get("available"):
            continue
        prepare = perturbation["playback_prepare"]
        shares = [
            abs(pair["delta_pct"]) for pair in prepare["pairs"] if pair["delta_pct"] is not None
        ]
        detail = {
            "median_delta_us": prepare["median_delta"],
            "median_delta_pct": prepare["median_delta_pct"],
            "max_abs_delta_us": prepare["max_abs_delta"],
            "max_abs_pair_pct": round(max(shares), 3) if shares else None,
            "n_pairs": len(prepare["pairs"]),
            "interleaved": perturbation["interleaved"],
        }
        if perturbation["stamp"] in SUPERSEDED_PROBE_GROUPS:
            findings.append(
                finding(
                    "trace perturbation measured on the superseded group",
                    perturbation["stamp"],
                    detail,
                    "armed in blocks, so the order effect is inside this number; it is the reason "
                    "the authoritative group alternates arms",
                    kind="superseded_group",
                )
            )
            continue
        checks.append(
            check(
                "{} trace perturbation is inside run-to-run noise".format(perturbation["stamp"]),
                "|median paired delta| < 1 % of playback_prepare",
                detail,
                abs(prepare["median_delta_pct"]) < 1.0,
                "the median is the statistic: a single pair moves by the process-to-process "
                "variation, which is what the paired design is for",
            )
        )
    authoritative = [
        block for block in probe_blocks if block["stamp"] not in SUPERSEDED_PROBE_GROUPS
    ]
    superseded_spreads = []
    claimed = []
    contextual = []
    sub_millisecond = []
    for block in probe_blocks:
        for arm, data in block["arms"].items():
            measured = {"first_present_us": data["first_present_us"]}
            for stage, value in data["stages_us"].items():
                measured["stage:" + stage] = value
            for stage, value in (data["trace_stages_us"] or {}).items():
                measured["trace:" + stage] = value
            for name, value in measured.items():
                if value["n"] < 3 or value["spread_pct"] is None:
                    continue
                entry = {
                    "stamp": block["stamp"],
                    "arm": arm,
                    "stage": name,
                    "median_us": round(value["median"], 1),
                    "abs_spread_us": round(value["max"] - value["min"], 1),
                    "spread_pct": value["spread_pct"],
                }
                if block["stamp"] in SUPERSEDED_PROBE_GROUPS:
                    if value["spread_pct"] > REPEATABILITY_LIMIT_PCT:
                        superseded_spreads.append(entry)
                elif name in CLAIM_QUANTITIES:
                    claimed.append(entry)
                elif value["median"] < REPEATABILITY_MIN_US:
                    sub_millisecond.append(entry)
                else:
                    contextual.append(entry)
    worst = max(claimed, key=lambda entry: entry["spread_pct"]) if claimed else None
    over = [entry for entry in claimed if entry["spread_pct"] > REPEATABILITY_LIMIT_PCT]
    checks.append(
        check(
            "every probe quantity the claim rests on is repeatable at 15 %",
            "all spread_pct <= 15 over first frame and playback_prepare",
            {
                "groups": [block["stamp"] for block in authoritative],
                "quantities_checked": len(claimed),
                "over_limit": over,
                "worst": worst,
            },
            bool(claimed) and not over,
            "these are the numbers this table compares; the device-handshake stages and the traced "
            "split of these groups are recorded separately, and the split is re-measured in its own "
            "group before any attribution is made",
        )
    )
    contextual_over = [
        entry for entry in contextual if entry["spread_pct"] > REPEATABILITY_LIMIT_PCT
    ]
    handshake_over = [entry for entry in contextual_over if not entry["stage"].startswith("trace:")]
    split_over = [entry for entry in contextual_over if entry["stage"].startswith("trace:")]
    if handshake_over:
        findings.append(
            finding(
                "device-handshake probe stages are wider than the 15 % limit",
                "{} millisecond-scale stages that no claim uses".format(len(handshake_over)),
                {
                    "over_limit": handshake_over,
                    "quantities_checked": len(contextual),
                    "max_abs_spread_us": max(
                        (entry["abs_spread_us"] for entry in handshake_over), default=None),
                },
                "backend_create, first_extract, first_submit and the neighbouring handshake "
                "stages move by milliseconds between fresh processes while their median is tens of "
                "milliseconds. They are reported as medians only; the comparative numbers in this "
                "table are the first frame total and playback_prepare, which are inside the limit.",
                kind="instrument_limit",
            )
        )
    if split_over:
        findings.append(
            finding(
                "the traced prepare split inside the startup probe groups is too noisy to compare",
                "{} trace stages over the limit".format(len(split_over)),
                {
                    "over_limit": split_over,
                    "note": (
                        "these groups exist to show the shape of the split and to bound the trace "
                        "perturbation; the split numbers quoted in the report come from the "
                        "dedicated serializer group, which is gated separately and passes"
                    ),
                },
                "a startup probe process also creates the device and reads pixels, so a 20 % "
                "excursion on chart_typed_read is possible there. Rather than widen the limit, the "
                "split comparison was re-run as its own N=3 group over the canonical sweep, where "
                "every quoted stage is inside 15 %.",
                kind="instrument_limit",
            )
        )
    if sub_millisecond:
        over_limit = [entry for entry in sub_millisecond if entry["spread_pct"] > REPEATABILITY_LIMIT_PCT]
        findings.append(
            finding(
                "sub-millisecond probe stages cannot meet a 15 % relative limit",
                "{} quantities whose median is under 1000 us".format(len(sub_millisecond)),
                {
                    "over_limit": over_limit,
                    "max_abs_spread_us": max(
                        (entry["abs_spread_us"] for entry in over_limit), default=None),
                },
                "gl_context, pixel_readback, backend_activate and the other device-handshake "
                "stages are microseconds wide, so 15 % of them is far below the clock and driver "
                "jitter floor; they are reported with their medians and never used as a claim.",
                kind="instrument_limit",
            )
        )
    if superseded_spreads:
        findings.append(
            finding(
                "the superseded group violates the repeatability limit",
                "spread_pct > 15 on measured stages",
                superseded_spreads,
                "this is the confound the interleaved group removes, recorded rather than hidden",
                kind="superseded_group",
            )
        )
    for block in watch_blocks:
        for arm, data in block["arms"].items():
            if data["n"] < 3:
                continue
            durations = {
                key: value
                for key, value in (data["spread_pct"] or {}).items()
                if key in ("primary_visible_t_s", "exit_t_s", "wm_close_to_exit_s")
            }
            phase_spreads = {
                name: data["phase_totals_us"][name]["spread_pct"]
                for name in data["phase_totals_us"]
                if data["phase_totals_us"][name]["spread_pct"] is not None
            }
            over_durations = {
                key: value
                for key, value in {**durations, **phase_spreads}.items()
                if value > REPEATABILITY_LIMIT_PCT
            }
            checks.append(
                check(
                    "{} {} start-up and shutdown durations are repeatable".format(
                        block["stamp"], arm
                    ),
                    "every duration spread_pct <= 15",
                    {"durations": durations, "phases": phase_spreads, "over_limit": over_durations},
                    not over_durations,
                )
            )
            counts = {
                key: value
                for key, value in (data["spread_pct"] or {}).items()
                if key in ("probes_after_primary", "probe_latency_over_100ms")
            }
            over_counts = {
                key: value for key, value in counts.items() if value > REPEATABILITY_LIMIT_PCT
            }
            if over_counts:
                findings.append(
                    finding(
                        "{} {} probe count spread exceeds 15 %".format(block["stamp"], arm),
                        "counts, not durations",
                        {
                            "spread_pct": over_counts,
                            "per_process_probes_after_primary": data[
                                "per_process_probes_after_primary"
                            ],
                        },
                        "a count varies with how many probes fit after the window appears; the "
                        "latency distribution is the responsiveness measure, not the count",
                        kind="count_metric",
                    )
                )
            matches = data["phase_medians_match_recorded"]
            checks.append(
                check(
                    "{} {} re-derived phase medians match the recorded aggregate".format(
                        block["stamp"], arm
                    ),
                    "every recorded phase median",
                    matches,
                    bool(matches) and all(matches.values()),
                    "recomputed from samples[] of the same file, one entry per process",
                )
            )
            logs = data["log_check"]
            with_log = [entry for entry in logs if entry["exists"] and entry["compared"]]
            bad_logs = [entry for entry in with_log if entry["disagreements"]]
            if logs and any(entry["exists"] for entry in logs):
                checks.append(
                    check(
                        "{} {} parse of the archived log reproduces samples[]".format(
                            block["stamp"], arm
                        ),
                        "every phase present in both the log and samples[] agrees to 1 us",
                        {
                            "logs": len(logs),
                            "with_phases": len(with_log),
                            "disagreements": bad_logs,
                            "phases_compared": {
                                entry["log"]: entry["compared"] for entry in with_log
                            },
                        },
                        not bad_logs,
                        "the log is the byte stream the process wrote; samples[] is the driver's "
                        "parse of it, so this checks the summary against the raw bytes",
                    )
                )
    for block in watch_blocks:
        for arm, data in block["arms"].items():
            absent = [
                event
                for event in data["close_events"]
                if event["target_existed_at_post"] is False
            ]
            if not absent:
                continue
            findings.append(
                finding(
                    "{} {} records a close latency for a close that had no window to land on"
                    .format(block["stamp"], arm),
                    "{} process(es)".format(len(absent)),
                    {
                        "events": absent,
                        "note": (
                            "watch_window.py posts WM_CLOSE to the window it currently believes is "
                            "primary and stamps wm_close_to_exit_s regardless of whether the post "
                            "happened, so for this arm that field is the remainder of the watcher's "
                            "timeout, not a close latency"
                        ),
                    },
                    "the close deadline falls before the primary window exists, so PostMessageW is "
                    "never called; the number is correct as an artefact and is excluded from every "
                    "shutdown claim. The device-first arm is the valid counterpart, because there a "
                    "window exists at the same 1 s deadline.",
                    kind="close_without_target",
                )
            )
    for block in watch_blocks:
        recorded = {arm: data["executables"] for arm, data in block["arms"].items()}
        checks.append(
            check(
                "{} binds every arm to one recorded executable digest".format(block["stamp"]),
                "one non-empty exe_sha256 per arm, identical for every process",
                recorded,
                all(
                    len(items) == 1 and items[0]["sha256"]
                    for items in recorded.values()
                ),
                "read from samples[].summary.exe_sha256, which watch_window.py writes for every "
                "process it launches",
            )
        )
    by_stamp = {block["stamp"]: block for block in watch_blocks}
    if "20261101-w1" in by_stamp and pilots["samples"]:
        w1_starts = [
            value
            for data in by_stamp["20261101-w1"]["arms"].values()
            for value in data["started_wall"]
            if value is not None
        ]
        w1_first = min(w1_starts) if w1_starts else None
        earlier = [
            entry
            for entry in pilots["samples"]
            if entry["started_wall"] is not None
            and w1_first is not None
            and float(entry["started_wall"]) < float(w1_first)
        ]
        latest = max(earlier, key=lambda entry: float(entry["started_wall"])) if earlier else None
        expected = latest["exe_sha256"] if latest else None
        player_arms = {
            arm: [item["sha256"] for item in data["executables"]]
            for arm, data in by_stamp["20261101-w1"]["arms"].items()
            if arm.startswith("player-")
        }
        checks.append(
            check(
                "w1 ran the player binary of the last pilot before it",
                expected,
                {
                    "arms": player_arms,
                    "expected_from": latest["sample"] if latest else None,
                    "w1_first_start": w1_first,
                    "pilots": {
                        entry["sample"]: {
                            "sha256": entry["exe_sha256"],
                            "started_wall": entry["started_wall"],
                        }
                        for entry in pilots["samples"]
                    },
                },
                bool(expected)
                and len(player_arms) == 2
                and all(value == [expected] for value in player_arms.values()),
                "the pilots walked three builds (56a04af4, 6938a31a after the window-first patch, "
                "9da8939c after the thread fix); w1 followed the first of them, the only one that "
                "started before it, and its traced and untraced arms differ only by the "
                "CUEXIS_PREPARE_TRACE environment variable",
            )
        )
    if "20261101-w2" in by_stamp:
        w2_arms = by_stamp["20261101-w2"]["arms"]
        default_digest = [
            item["sha256"] for item in w2_arms["player-40k-untraced"]["executables"]
        ]
        first_digest = [
            item["sha256"] for item in w2_arms["player-40k-window-first"]["executables"]
        ]
        checks.append(
            check(
                "w2 default order and window-first order are one binary",
                default_digest,
                {
                    "default": default_digest,
                    "window_first": first_digest,
                    "default_env": w2_arms["player-40k-untraced"]["env_overrides"],
                    "window_first_env": w2_arms["player-40k-window-first"]["env_overrides"],
                },
                bool(default_digest) and default_digest == first_digest,
                "the order is selected by CUEXIS_PLAYER_WINDOW_FIRST at run time, so the pair is "
                "same-binary and env-only",
            )
        )
    player_now = {"sha256": sha256_of(PLAYER), "bytes": PLAYER.stat().st_size}
    after_environment = serializer["after"]["environment"]
    checks.append(
        check(
            "the direct-writer arm is the player now in the release bin",
            {"sha256": after_environment.get("player_sha256")},
            {"sha256": player_now["sha256"], "bytes": player_now["bytes"]},
            after_environment.get("player_sha256") == player_now["sha256"],
            "the after arm hashed the executable it launched; no later rebuild replaced it, so the "
            "number and the tree's binary are one artifact",
        )
    )
    newer_sources = [
        str(path).replace("\\", "/")
        for root in (pathlib.Path("engine"), pathlib.Path("app"))
        for path in root.rglob("*")
        if path.is_file()
        and path.suffix.lower() in (".cpp", ".hpp", ".h", ".inl", ".in")
        and path.stat().st_mtime > PLAYER.stat().st_mtime
    ]
    checks.append(
        check(
            "no engine or app source is newer than the release player",
            "no C++ source newer than {}".format(iso_from_mtime(PLAYER)),
            {
                "player_mtime": iso_from_mtime(PLAYER),
                "newer_sources": newer_sources[:20],
                "newer_count": len(newer_sources),
            },
            not newer_sources,
            "a source newer than the binary would mean the measured player predates the tree",
        )
    )
    if "20261101-w2" in by_stamp:
        w2 = by_stamp["20261101-w2"]["arms"]
        default_order = w2.get("player-40k-untraced")
        window_first = w2.get("player-40k-window-first")
        if default_order and window_first:
            created_default = default_order["phase_totals_us"]["window_created"]["median"]
            created_first = window_first["phase_totals_us"]["window_created"]["median"]
            visible_default = default_order["medians"]["primary_visible_t_s"]
            visible_first = window_first["medians"]["primary_visible_t_s"]
            frame_default = default_order["phase_totals_us"]["first_frame_presented"]["median"]
            frame_first = window_first["phase_totals_us"]["first_frame_presented"]["median"]
            ratio_created = (
                round(created_first / created_default, 4) if created_default else None
            )
            ratio_visible = (
                round(visible_first / visible_default, 4) if visible_default else None
            )
            penalty = (
                round(frame_first - frame_default, 1)
                if frame_first is not None and frame_default is not None
                else None
            )
            checks.append(
                check(
                    "the device-first order creates the window before prepare finishes",
                    "window_created total <= 50 % of the default order",
                    {
                        "default_us": created_default,
                        "window_first_us": created_first,
                        "ratio": ratio_created,
                        "content_prepared_default_us": default_order["phase_totals_us"][
                            "content_prepared"
                        ]["median"],
                        "content_prepared_window_first_us": window_first["phase_totals_us"][
                            "content_prepared"
                        ]["median"],
                    },
                    ratio_created is not None and ratio_created <= 0.5,
                )
            )
            checks.append(
                check(
                    "the visible window appears earlier than the first frame",
                    "primary_visible_t_s <= 50 % of the default order",
                    {
                        "default_s": visible_default,
                        "window_first_s": visible_first,
                        "ratio": ratio_visible,
                    },
                    ratio_visible is not None and ratio_visible <= 0.5,
                )
            )
            checks.append(
                check(
                    "the early window costs a bounded first-frame penalty",
                    "0 < penalty <= 200000 us",
                    {"default_us": frame_default, "window_first_us": frame_first, "penalty_us": penalty},
                    penalty is not None and 0 <= penalty <= 200_000,
                    "the context and the surface are created before prepare instead of after it, "
                    "so the whole start-up pays for the same work in a different order",
                )
            )
        close_first = w2.get("player-40k-window-first-close1")
        close_default = w2.get("player-40k-untraced-close1")
        if close_first and close_default:
            checks.append(
                check(
                    "a close posted at 1.0 s reaches the device-first window and not the default one",
                    "window-first exits on its own, default order has to be killed at the wall",
                    {
                        "window_first_return_codes": close_first["return_codes"],
                        "window_first_killed": close_first["killed"],
                        "window_first_close_s": close_first["medians"]["wm_close_to_exit_s"],
                        "window_first_exit_s": close_first["medians"]["exit_t_s"],
                        "default_return_codes": close_default["return_codes"],
                        "default_killed": close_default["killed"],
                        "default_close_s": close_default["medians"]["wm_close_to_exit_s"],
                        "default_exit_s": close_default["medians"]["exit_t_s"],
                    },
                    close_first["killed"] == [False] and close_default["killed"] == [True],
                )
            )
            checks.append(
                check(
                    "the early close is honoured after prepare finishes, not during it",
                    "wm_close_to_exit_s stays above the 1000 ms Reset/Close gate",
                    {
                        "window_first_close_s": close_first["medians"]["wm_close_to_exit_s"],
                        "gate_ms": 1000,
                    },
                    (close_first["medians"]["wm_close_to_exit_s"] or 0) * 1000.0 > 1000.0,
                    "recorded as a limitation of this order, not as a passing close latency",
                )
            )
        if default_order and window_first:
            timeouts_default = default_order["medians"].get("probes_timed_out")
            timeouts_first = window_first["medians"].get("probes_timed_out")
            if (timeouts_first or 0) > (timeouts_default or 0):
                findings.append(
                    finding(
                        "the device-first window is visible but does not pump messages while preparing",
                        "probe timeouts inside the prepare window",
                        {
                            "probes_timed_out_default": timeouts_default,
                            "probes_timed_out_window_first": timeouts_first,
                            "probe_latency_ms_window_first": window_first["probe_latency_ms"],
                            "probe_latency_ms_default": default_order["probe_latency_ms"],
                        },
                        "the user can see the window and can post WM_CLOSE, but the close is only "
                        "serviced once prepare returns; this is why the order is a perceived "
                        "latency improvement and not a responsiveness fix",
                        kind="behavioural_limit",
                    )
                )
    if scaling["us_per_entity_largest_over_smallest"] is not None:
        checks.append(
            check(
                "prepare cost per entity does not collapse with entity count",
                "<= 1.5x between the smallest and largest traced arm",
                scaling["us_per_entity_largest_over_smallest"],
                scaling["us_per_entity_largest_over_smallest"] <= 1.5,
                "a flat cost per entity is what a per-entity parse and canonicalise would give",
            )
        )
    for side in ("before", "after"):
        block = serializer[side]
        frames = block["first_frame_presented_total_us"]
        checks.append(
            check(
                "serializer {} first frame spread <= 15 %".format(side),
                "spread_pct <= 15",
                frames["spread_pct"],
                frames["spread_pct"] is not None and frames["spread_pct"] <= REPEATABILITY_LIMIT_PCT,
            )
        )
        over = {
            stage: value["spread_pct"]
            for stage, value in block["stages_us"].items()
            if value["spread_pct"] is not None and value["spread_pct"] > REPEATABILITY_LIMIT_PCT
        }
        checks.append(
            check(
                "serializer {} stages repeatable at 15 %".format(side),
                "no stage over the limit",
                over,
                not over,
            )
        )
    before_environment = serializer["before"]["environment"]
    after_environment = serializer["after"]["environment"]
    checks.append(
        check(
            "the two serializer arms ran different binaries",
            "different digests",
            {
                "before": before_environment.get("player_sha256"),
                "after": after_environment.get("player_sha256"),
                "before_binding": before_environment.get("player_binding"),
                "after_binding": after_environment.get("player_binding"),
            },
            before_environment.get("player_sha256") is not None
            and after_environment.get("player_sha256") is not None
            and before_environment["player_sha256"] != after_environment["player_sha256"],
        )
    )
    same_project = (
        before_environment.get("project_chart_sha256") == after_environment.get("project_chart_sha256")
    )
    checks.append(
        check(
            "both serializer arms measured the same chart bytes",
            before_environment.get("project_chart_sha256"),
            after_environment.get("project_chart_sha256"),
            same_project,
        )
    )
    return checks, findings


# --------------------------------------------------------------------------------------------------
# rendering
# --------------------------------------------------------------------------------------------------


def us(value):
    return "{:,.0f}".format(value) if isinstance(value, (int, float)) else "-"


def opt(value):
    if value is None:
        return "-"
    if isinstance(value, float):
        return "{:.4f}".format(value)
    return str(value)


def render_markdown(table):
    lines = []
    lines.append("# 40K start-up and prepare cost, derived table")
    lines.append("")
    lines.append("Generated by `tabulate_startup.py` at {} from the JSON files named per section.".format(
        table["generated_at"]))
    lines.append("Every number below is recomputed from those files; the script fails a check rather")
    lines.append("than reporting a value it could not re-derive. Units are microseconds unless a column")
    lines.append("says otherwise.")
    lines.append("")

    lines.append("## 1. Probe groups")
    lines.append("")
    for block in table["probe"]["stamps"]:
        lines.append("### {}".format(block["stamp"]))
        lines.append("")
        lines.append(
            "`{}` samples={} switches={} interleaved={} probe `{}`".format(
                pathlib.Path(block["exe"]).name,
                block["samples"],
                block["arm_switches"],
                block["interleaved"],
                block["exe_sha256"],
            )
        )
        lines.append("")
        lines.append("| arm | entities | traced | n | rc | first frame median | spread % |")
        lines.append("|---|---|---|---|---|---|---|")
        for arm, data in block["arms"].items():
            lines.append(
                "| {} | {} | {} | {} | {} | {} | {} |".format(
                    arm,
                    data["entities"],
                    data["traced"],
                    data["n"],
                    ",".join(str(code) for code in data["return_codes"]),
                    us(data["first_present_us"]["median"]),
                    data["first_present_us"]["spread_pct"],
                )
            )
        lines.append("")
        for arm, data in block["arms"].items():
            if not data["trace_stages_us"]:
                continue
            lines.append("#### {} stage split".format(arm))
            lines.append("")
            lines.append("| stage | median | min | max | spread % |")
            lines.append("|---|---|---|---|---|")
            for stage, value in data["trace_stages_us"].items():
                lines.append(
                    "| {} | {} | {} | {} | {} |".format(
                        stage, us(value["median"]), us(value["min"]), us(value["max"]),
                        value["spread_pct"])
                )
            lines.append("")

    lines.append("## 2. Trace perturbation")
    lines.append("")
    lines.append("| group | metric | pairs | median delta | median delta % | max abs delta |")
    lines.append("|---|---|---|---|---|---|")
    for perturbation in table["trace_perturbation"]["probe"]:
        if not perturbation.get("available"):
            continue
        for metric in ("playback_prepare", "first_present_us"):
            data = perturbation[metric]
            lines.append(
                "| {} (probe, {}) | {} | {} | {} | {} | {} |".format(
                    perturbation["stamp"],
                    "interleaved" if perturbation["interleaved"] else "blocked",
                    metric,
                    len(data["pairs"]),
                    us(data["median_delta"]),
                    data["median_delta_pct"],
                    us(data["max_abs_delta"]),
                )
            )
    lines.append("")

    lines.append("## 3. Prepare cost against entity count")
    lines.append("")
    lines.append("| entities | arm | stamp | playback_prepare median | us per entity | first frame |")
    lines.append("|---|---|---|---|---|---|")
    for point in table["entity_scaling"]["points"]:
        lines.append(
            "| {} | {} | {} | {} | {} | {} |".format(
                point["entities"], point["arm"], point["stamp"],
                us(point["playback_prepare_median_us"]), point["us_per_entity"],
                us(point["first_present_us"]))
        )
    lines.append("")

    lines.append("## 4. Watcher groups")
    lines.append("")
    for block in table["watch"]["stamps"]:
        lines.append("### {}".format(block["stamp"]))
        lines.append("")
        lines.append("order: {}".format(", ".join(block["order"])))
        lines.append("")
        lines.append("| arm | n | rc | visible s | exit s | close s | first frame | content prepared "
                     "| window created | probe p50 | probe p99 | probe max | cpu 1s |")
        lines.append("|---|---|---|---|---|---|---|---|---|---|---|---|---|")
        for arm, data in block["arms"].items():
            medians = data["medians"]
            latency = data["probe_latency_ms"]
            cpu = data["cpu_core_ratio"]
            totals = data["phase_totals_us"]
            frame = totals.get("first_frame_presented", {}).get("median")
            prepared = totals.get("content_prepared", {}).get("median")
            created = totals.get("window_created", {}).get("median")
            # A star marks an arm whose recorded close latency is the watcher timeout remainder
            # because no window existed when the close was due; section 4.1 explains each one.
            close_cell = opt(medians.get("wm_close_to_exit_s"))
            if any(
                event["target_existed_at_post"] is False for event in data["close_events"]
            ):
                close_cell += " *"
            lines.append(
                "| {} | {} | {} | {} | {} | {} | {} | {} | {} | {} | {} | {} | {} |".format(
                    arm, data["n"], ",".join(str(code) for code in data["return_codes"]),
                    opt(medians.get("primary_visible_t_s")), opt(medians.get("exit_t_s")),
                    close_cell, us(frame), us(prepared), us(created),
                    opt(latency.get("p50")), opt(latency.get("p99")), opt(latency.get("max")),
                    opt(cpu.get("1s")))
            )
        lines.append("")
        if any(
            event["posted_t_s"] is not None
            for data in block["arms"].values()
            for event in data["close_events"]
        ):
            lines.append("#### Close posting, one row per process")
            lines.append("")
            lines.append("| arm | index | posted at s | primary first seen s | target existed "
                         "| to exit s | rc | killed |")
            lines.append("|---|---|---|---|---|---|---|---|")
            for arm, data in block["arms"].items():
                for event in data["close_events"]:
                    if event["posted_t_s"] is None:
                        continue
                    lines.append(
                        "| {} | {} | {} | {} | {} | {} | {} | {} |".format(
                            arm, event["index"], opt(event["posted_t_s"]),
                            opt(event["primary_first_seen_t_s"]),
                            event["target_existed_at_post"], opt(event["to_exit_s"]),
                            event["return_code"], event["killed"])
                    )
            lines.append("")
            lines.append("`watch_window.py` posts `WM_CLOSE` to the window it currently believes is "
                         "primary and records `wm_close_to_exit_s` whether or not a window existed "
                         "to receive it. Where the target is `False` the post never happened, so "
                         "that row's `to exit s` measures the watcher's own timeout, not a close "
                         "latency, and no shutdown claim uses it.")
            lines.append("")
        for arm, data in block["arms"].items():
            if not data["phase_totals_us"]["content_prepared"]["n"]:
                continue
            lines.append("#### {} per-process phases".format(arm))
            lines.append("")
            lines.append("| phase | n | median | min | max | spread % |")
            lines.append("|---|---|---|---|---|---|")
            for name, value in data["phase_totals_us"].items():
                if not value["n"]:
                    continue
                lines.append(
                    "| {} | {} | {} | {} | {} | {} |".format(
                        name, value["n"], us(value["median"]), us(value["min"]), us(value["max"]),
                        opt(value["spread_pct"]))
                )
            lines.append("")

    lines.append("## 5. Serializer before and after")
    lines.append("")
    lines.append("| stage | before median | after median | delta | delta % | after self |")
    lines.append("|---|---|---|---|---|---|")
    for row in table["serializer"]["compare"]["rows"]:
        self_us = table["serializer"]["after"]["stages_us"].get(row["stage"], {}).get(
            "self_median_us")
        lines.append(
            "| {} | {} | {} | {} | {} | {} |".format(
                row["stage"], us(row["before_us"]), us(row["after_us"]), us(row["delta_us"]),
                row["delta_pct"], us(self_us))
        )
    frames = table["serializer"]["first_frame"]
    lines.append("")
    lines.append(
        "first frame: before {} us, after {} us, delta {} us ({} %), gate {} us, gap {} ms ({} %)".format(
            us(frames["before_median_us"]), us(frames["after_median_us"]), us(frames["delta_us"]),
            frames["delta_pct"], us(frames["gate_us"]), frames["gap_to_gate_ms"],
            frames["gap_to_gate_pct"]))
    lines.append("")

    lines.append("## 6. Checks")
    lines.append("")
    lines.append("| check | expected | actual | ok |")
    lines.append("|---|---|---|---|")
    for entry in table["checks"]:
        actual = json.dumps(entry["actual"], ensure_ascii=False)
        if len(actual) > 180:
            actual = actual[:177] + "..."
        lines.append(
            "| {} | {} | `{}` | {} |".format(
                entry["name"],
                str(entry["expected"]).replace("|", "/"),
                actual.replace("|", "/"),
                entry["ok"],
            )
        )
    lines.append("")
    lines.append("## 7. Findings")
    lines.append("")
    lines.append("Recorded divergences that the report has to keep stating. They are not protocol")
    lines.append("violations, so they are not checks; a check that always passes would hide them.")
    lines.append("")
    for entry in table.get("findings", []):
        lines.append("### {}".format(entry["what"]))
        lines.append("")
        lines.append("- subject: `{}`".format(entry["subject"]))
        lines.append("- kind: {}".format(entry["kind"]))
        lines.append("- why: {}".format(entry["note"]))
        lines.append("")
        detail = json.dumps(entry["detail"], ensure_ascii=False, indent=1)
        if len(detail) > 900:
            lines.append("```json")
            lines.extend(detail.splitlines())
            lines.append("```")
        else:
            lines.append("- detail: `{}`".format(detail.replace("\n", " ").replace("|", "/")))
        lines.append("")
    lines.append("## 8. Bindings")
    lines.append("")
    lines.append("| first observed | how the time was obtained | sha256 | bytes | evidence |")
    lines.append("|---|---|---|---|---|")
    for entry in table["binding"]["entries"]:
        observed = entry["observed"][0]
        lines.append(
            "| {} | {} | `{}` | {} | {} |".format(
                observed["when"], observed.get("when_kind", "-"), entry["sha256"],
                entry["bytes"] if entry["bytes"] else "-", observed["source"])
        )
    lines.append("")
    disk = table["binding"]["disk_now"]
    lines.append("On disk now: probe `{}` ({} bytes), player `{}` ({} bytes).".format(
        disk["probe"]["sha256"], disk["probe"]["bytes"], disk["player"]["sha256"],
        disk["player"]["bytes"]))
    lines.append("")
    lines.append("Watcher process binding, one row per arm (the digest each process reported about "
                 "itself):")
    lines.append("")
    lines.append("| group | arm | n | sha256 | bytes | env overrides | first start |")
    lines.append("|---|---|---|---|---|---|---|")
    for stamp, entry in table["binding"]["watch_binding"].items():
        for arm, data in entry["arms"].items():
            if len(data["executables"]) == 1:
                digest = data["executables"][0]["sha256"]
                size = data["executables"][0]["bytes"]
            else:
                digest = " / ".join(item["sha256"] or "none" for item in data["executables"])
                size = None
            env = "; ".join(",".join(item) for item in data["env_overrides"]) or "-"
            lines.append(
                "| {} | {} | {} | `{}` | {} | {} | {} |".format(
                    stamp, arm, data["n"], digest.replace("|", "/"),
                    size if size else "-", env.replace("|", "/"), data["first_started_iso"] or "-")
            )
    lines.append("")
    return "\n".join(lines) + "\n"


def main():
    if not S1.is_dir() or not S2.is_dir():
        raise SystemExit("run this from the repository root: {} or {} is missing".format(S1, S2))

    probe_blocks = [probe_stamp(stamp) for stamp in PROBE_STAMPS]
    perturbations = [probe_perturbation(stamp) for stamp in ("20261101-a", "20261101-a2")]
    scaling = entity_scaling(probe_blocks)
    watch_blocks = [watch_stamp(stamp) for stamp in WATCH_STAMPS]
    pilots = pilot_block()
    serializer = serializer_block()
    binding = build_lineage(probe_blocks, watch_blocks, pilots)
    checks, findings = build_checks(
        probe_blocks, perturbations, watch_blocks, scaling, serializer, pilots
    )

    table = {
        "scope": (
            "Derived table for the 40K start-up observability, the window-first start-up order, "
            "and the direct-writer prepare-cost comparison. Diagnostic evidence: no capacity "
            "point, no acceptance row, and no replacement for the pre-registered matrix."
        ),
        "generated_by": "tabulate_startup.py",
        "generated_at": datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds"),
        "inputs": {
            "probe_groups": ["runs/{}/startup-set.json".format(stamp) for stamp in PROBE_STAMPS],
            "watch_groups": ["runs/{}/watch-set.json".format(stamp) for stamp in WATCH_STAMPS],
            "pilots": [entry["sample"] for entry in pilots["samples"]],
            "before_stamp": "r3-prepare-cost-v1/baseline/prepare-split.json",
            "after_stamp": "r3-prepare-cost-v1/runs/serializer-direct-v1/prepare-split.json",
            "compare": "r3-prepare-cost-v1/after/prepare-split-compare.json",
        },
        "repeatability_limit_pct": REPEATABILITY_LIMIT_PCT,
        "first_frame_gate_us": FIRST_FRAME_GATE_US,
        "probe": {"stamps": probe_blocks},
        "trace_perturbation": {"probe": perturbations},
        "entity_scaling": scaling,
        "watch": {"stamps": watch_blocks, "pilots": pilots},
        "serializer": serializer,
        "binding": binding,
        "checks": checks,
        "findings": findings,
        "notes": [
            (
                "runs/20261101-w1 and runs/20261101-w2 persisted every child's stdout and stderr "
                "under raw/ and every process's own summary inside watch-set.json samples[], so "
                "the executable digest, the command line, the environment overrides and the start "
                "time are recorded per process without needing --json on watch_window.py. What the "
                "aggregate file does not keep is a standalone per-process JSON document; the next "
                "watcher group should still pass --json so that each process has a file of its own "
                "whose digest can be checked independently."
            ),
            (
                "20261101-a and 20261101-a2 measured the same question with different orders. a2 is "
                "the authoritative perturbation group; a is kept because it is what a batched order "
                "produces, and dropping it would hide that ordering effect."
            ),
            (
                "runs/smoke-w2/smoke-wf-01 exited 0xC0000409 (3221226505): the first window-first "
                "patch moved a device call off the owner thread and the process died. smoke-wf2-01 "
                "is the fix. Both are kept as the record of why the shipped order is thread-free."
            ),
            (
                "The 40000 entity arms are a stress case chosen to expose the prepare path, not a "
                "quality gate; the 2000 ms first-frame clamp is cited only to state the distance."
            ),
        ],
    }

    OUT.mkdir(parents=True, exist_ok=True)
    json_path = OUT / "startup-cost-table.json"
    md_path = OUT / "startup-cost-table.md"
    json_text = json.dumps(table, indent=1, ensure_ascii=False) + "\n"
    json_path.write_bytes(json_text.encode("utf-8"))
    md_path.write_bytes(render_markdown(table).encode("utf-8"))

    failed = [entry for entry in checks if not entry["ok"]]
    print("probe groups: {}".format(", ".join(
        "{} ({} samples, {} switches)".format(block["stamp"], block["samples"], block["arm_switches"])
        for block in probe_blocks)))
    print("watch groups: {}".format(", ".join(
        "{} ({} arms)".format(block["stamp"], len(block["arms"])) for block in watch_blocks)))
    print("first frame: before {} after {} delta {} ({} %)".format(
        us(serializer["first_frame"]["before_median_us"]),
        us(serializer["first_frame"]["after_median_us"]),
        us(serializer["first_frame"]["delta_us"]),
        serializer["first_frame"]["delta_pct"]))
    print("gap to gate: {} ms ({} %)".format(
        serializer["first_frame"]["gap_to_gate_ms"], serializer["first_frame"]["gap_to_gate_pct"]))
    print("checks: {} passed, {} failed".format(len(checks) - len(failed), len(failed)))
    for entry in failed:
        print("  FAILED {}: expected {}, got {}".format(
            entry["name"], entry["expected"], json.dumps(entry["actual"], ensure_ascii=False)[:400]))
    print("findings: {}".format(len(findings)))
    for entry in findings:
        print("  {} [{}]: {}".format(entry["what"], entry["kind"], json.dumps(
            entry["detail"], ensure_ascii=False)[:200]))
    print("wrote {} ({} bytes, sha256 {})".format(json_path, json_path.stat().st_size,
                                                  sha256_of(json_path)))
    print("wrote {} ({} bytes, sha256 {})".format(md_path, md_path.stat().st_size,
                                                  sha256_of(md_path)))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())

"""Measure the intra-prepare stage split of the 40K player startup, and diff two stamps.

Registered before the first run of the stamp (this file is the registration):

  Measured quantity: the elapsed time of every PREPARE_TRACE scope inside one player prepare,
  plus the player's own content_prepared phase, for the 40K project on the release candidate
  build. This is a diagnostic of stage composition, not a capacity or acceptance measurement.

  Protocol, identical for every stamp:
    N = 3 fresh processes of
      out/build/rpa-candidate-release/bin/cuexis_player.exe --project player-40k-project
    with CUEXIS_PREPARE_TRACE=1, the player's default start-up order, a WM_CLOSE posted at 8.0 s,
    and a 40 s wall. Observed and measured by watch_window.py with its defaults (poll 2 ms,
    window enumeration 5 ms, probe every 25 ms with a 400 ms cap).
    Nothing is re-sampled, excluded or reordered. A stage's report is the median over the three
    processes; the spread (max - min) / median is printed with it, and a spread above 15 % marks
    that stage as not repeatable at N = 3.

  Derived quantities, computed per process before any median:
    self_us   = elapsed - sum(immediate children elapsed)     (the work the scope itself owns)
    traced_us = sum of top-level scope elapsed
    outside   = content_prepared - traced_us                   (prepare work with no scope)
  The self_us of chart_canonical_write is the part of the canonical write that is neither
  normalization nor dumping; it is where the Value tree is destroyed.

Usage (from D:\\Cuexis-redesign):
    python -B run_prepare_split_set.py run --stamp serializer-direct-v1
    python -B run_prepare_split_set.py summarise --dir runs/20261101-c --prefix writer-split- \
        --out baseline/prepare-split.json --label "before: nlohmann DOM dump"
    python -B run_prepare_split_set.py compare --before baseline/prepare-split.json \
        --after runs/serializer-direct-v1/prepare-split.json

Addendum 2026-11-01, written after both stamps had been measured (the text above is kept
verbatim because it is the registration):

  A stamp's environment.player_sha256 is only meaningful when the digest was taken from the
  binary that actually produced the samples. `run` hashes the player before the first launch and
  after the last one and refuses to write a stamp if it moved. `summarise` re-reads logs of
  processes that are gone, so the digest visible when it runs proves nothing about them. The
  first version of this file wrote that digest into environment.player_sha256 anyway, which was
  wrong for the writer-split (before) stamp: it was summarised at 11:41, after the 11:34 rebuild,
  so it carried the *after* binary's digest beside the *before* arm's numbers. The original,
  mis-bound file is kept beside the corrected one as baseline/prepare-split.original.json.

  `summarise` now recovers the binding instead of guessing it. watch_window.py takes a digest of
  the executable before every launch and stores it in <dir>/raw/<label>.json, so a stamp that was
  observed by the watcher can name its own binary: recover_binding() reads those summaries, and
  when every measured log has a matching raw sample and all of them agree, environment.
  player_sha256 is that digest with player_binding = "recorded by watch_window.py before each
  launch". When the raw samples are missing or disagree, player_sha256 is null and
  player_binding is "unrecorded"; the digest visible at summarise time stays under
  player_at_summarise and is labelled as such. The per-sample recovery is kept in the stamp under
  binding_recovery so a reader can audit the claim. The `run` path is untouched: it binds the
  digest it hashed itself, and the samples of that stamp carry their own copy in samples[].
"""

import argparse
import datetime
import hashlib
import json
import pathlib
import statistics
import sys

ROOT = pathlib.Path("out/verification-20261101/r3-startup-observability-v1")
HERE = pathlib.Path("out/verification-20261101/r3-prepare-cost-v1")
PLAYER = pathlib.Path("out/build/rpa-candidate-release/bin/cuexis_player.exe")
PROJECT = ROOT / "player-40k-project"

N_SAMPLES = 3
CLOSE_AFTER = 8.0
TIMEOUT = 40.0
REPEATABILITY_LIMIT_PCT = 15.0

FOCUS = [
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


def sha256_of(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def parse_trace(text: str) -> list[dict]:
    """Turn the flat BEGIN/END lines into frames with depth, parent and self time."""
    stack = []
    frames = []
    for line in text.splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[0] == "PREPARE_TRACE_BEGIN":
            stack.append({"name": parts[1], "child_us": 0, "elapsed_us": None})
            continue
        if len(parts) == 4 and parts[0] == "PREPARE_TRACE_END":
            name = parts[1]
            elapsed = int(parts[2])
            if not stack or stack[-1]["name"] != name:
                raise SystemExit(f"unbalanced prepare trace at {line!r}")
            frame = stack.pop()
            frame["elapsed_us"] = elapsed
            frame["depth"] = len(stack)
            frame["parent"] = stack[-1]["name"] if stack else None
            frame["self_us"] = elapsed - frame.pop("child_us")
            if stack:
                stack[-1]["child_us"] += elapsed
            frames.append(frame)
    if stack:
        raise SystemExit(f"unterminated prepare trace scopes: {[f['name'] for f in stack]}")
    return frames


def parse_phases(text: str) -> dict:
    phases = {}
    for line in text.splitlines():
        marker = "[player.startup_phase]"
        if marker not in line:
            continue
        tokens = line.split(marker, 1)[1].split()
        name = tokens[0]
        fields = {}
        for index in range(1, len(tokens) - 1):
            if tokens[index + 1].isdigit():
                fields[tokens[index]] = int(tokens[index + 1])
        phases[name] = fields
    return phases


def summarise_process(frames: list[dict], phases: dict) -> dict:
    elapsed = {}
    self_time = {}
    for frame in frames:
        elapsed.setdefault(frame["name"], []).append(frame["elapsed_us"])
        self_time.setdefault(frame["name"], []).append(frame["self_us"])
    stages = {name: sum(values) for name, values in elapsed.items()}
    own = {name: sum(values) for name, values in self_time.items()}
    traced = sum(frame["elapsed_us"] for frame in frames if frame["depth"] == 0)
    prepared = phases.get("content_prepared", {}).get("elapsed_us")
    return {
        "stages_us": stages,
        "self_us": own,
        "traced_us": traced,
        "content_prepared_us": prepared,
        "outside_us": None if prepared is None else prepared - traced,
        "frames": frames,
    }


def median_of(values):
    clean = [value for value in values if value is not None]
    return round(statistics.median(clean), 1) if clean else None


def spread_of(values):
    clean = [value for value in values if value is not None]
    if len(clean) < 2 or statistics.median(clean) == 0:
        return None
    return round((max(clean) - min(clean)) / statistics.median(clean) * 100.0, 2)


def aggregate(processes: list[dict]) -> dict:
    names = []
    for process in processes:
        for name in process["stages_us"]:
            if name not in names:
                names.append(name)
    stages = {}
    for name in names:
        elapsed = [process["stages_us"].get(name) for process in processes]
        own = [process["self_us"].get(name) for process in processes]
        stages[name] = {
            "median_us": median_of(elapsed),
            "min_us": min(value for value in elapsed if value is not None),
            "max_us": max(value for value in elapsed if value is not None),
            "spread_pct": spread_of(elapsed),
            "self_median_us": median_of(own),
            "per_process_us": elapsed,
        }
    prepared = [process["content_prepared_us"] for process in processes]
    outside = [process["outside_us"] for process in processes]
    return {
        "n": len(processes),
        "content_prepared_us": {
            "median_us": median_of(prepared),
            "per_process_us": prepared,
            "spread_pct": spread_of(prepared),
        },
        "outside_us": {
            "median_us": median_of(outside),
            "per_process_us": outside,
            "spread_pct": spread_of(outside),
        },
        "stages": stages,
    }


def load_stamp(directory: pathlib.Path, prefix: str, label: str) -> dict:
    processes = []
    for path in sorted(directory.glob(f"{prefix}*.err.txt")):
        stem = path.name[: -len(".err.txt")]
        out_path = directory / f"{stem}.out.txt"
        err_text = path.read_bytes().decode("utf-8", errors="replace")
        out_text = out_path.read_bytes().decode("utf-8", errors="replace")
        frames = parse_trace(err_text)
        phases = parse_phases(out_text)
        process = summarise_process(frames, phases)
        process["label"] = stem
        process["err_sha256"] = sha256_of(path)
        process["err_bytes"] = path.stat().st_size
        process["phase_lines"] = {
            name: value
            for name, value in phases.items()
            if name in ("content_prepared", "content_committed", "first_frame_presented")
        }
        processes.append(process)
    if not processes:
        raise SystemExit(f"no {prefix}*.err.txt under {directory}")
    return {
        "label": label,
        "source_dir": str(directory),
        "prefix": prefix,
        "processes": processes,
        "summary": aggregate(processes),
    }


def run_samples(stamp: str, count: int) -> dict:
    sys.path.insert(0, str(ROOT))
    import watch_window  # noqa: PLC0415

    stamp_dir = HERE / "runs" / stamp
    stamp_dir.mkdir(parents=True, exist_ok=True)
    if any(stamp_dir.glob("*.err.txt")):
        raise SystemExit(f"{stamp_dir} already holds samples; a stamp is measured once")
    identity_before = (sha256_of(PLAYER), PLAYER.stat().st_size)
    samples = []
    for index in range(count):
        label = f"player-40k-traced-{index:02d}"
        namespace = argparse.Namespace(
            label=label,
            out=str(stamp_dir),
            cwd=None,
            env=["CUEXIS_PREPARE_TRACE=1"],
            timeout=TIMEOUT,
            close_after=CLOSE_AFTER,
            poll_ms=2.0,
            enum_interval_ms=5.0,
            probe_interval_ms=25.0,
            probe_timeout_ms=400.0,
            cpu_interval_ms=100.0,
            json=str(stamp_dir / "raw" / f"{label}.json"),
            command=[str(PLAYER), "--project", str(PROJECT)],
        )
        (stamp_dir / "raw").mkdir(parents=True, exist_ok=True)
        result = watch_window.run(namespace)
        samples.append(
            {
                "label": label,
                "return_code": result["summary"]["return_code"],
                "killed": result["summary"]["killed"],
                "primary_visible_t_s": result["summary"]["primary_visible_t_s"],
            }
        )
        print(f"{label}: rc={samples[-1]['return_code']} killed={samples[-1]['killed']}", flush=True)
    identity_after = (sha256_of(PLAYER), PLAYER.stat().st_size)
    if identity_after != identity_before:
        raise SystemExit(
            f"the player changed while the stamp ran: {identity_before} -> {identity_after}"
        )
    return {"stamp": stamp, "count": count, "samples": samples}


def environment() -> dict:
    return {
        "player": str(PLAYER),
        "player_sha256": sha256_of(PLAYER),
        "player_bytes": PLAYER.stat().st_size,
        "project": str(PROJECT),
        "project_chart_sha256": sha256_of(
            PROJECT / "assets" / "charts" / "stress_40k.cuexis.chart.json"
        ),
        "close_after_s": CLOSE_AFTER,
        "timeout_s": TIMEOUT,
        "samples": N_SAMPLES,
        "repeatability_limit_pct": REPEATABILITY_LIMIT_PCT,
        "trace_gate": "CUEXIS_PREPARE_TRACE=1",
        "watch_window_poll_ms": 2.0,
        "probe_interval_ms": 25.0,
    }


def recover_binding(directory: pathlib.Path, prefix: str) -> dict:
    """Recover the digest of the player that produced a stamp from the run's own raw samples.

    watch_window.py writes <directory>/raw/<label>.json per process and records the digest it took
    of the executable before launching it. When those summaries exist and agree, the stamp's
    binding is a recorded measurement rather than an observation of whatever is on disk when the
    logs are summarised. Anything incomplete degrades to "unrecorded" instead of a guess.
    """
    samples = []
    for path in sorted((directory / "raw").glob(f"{prefix}*.json")):
        summary = json.loads(path.read_bytes().decode("utf-8")).get("summary", {})
        samples.append(
            {
                "label": summary.get("label"),
                "raw": str(path),
                "raw_sha256": sha256_of(path),
                "exe_sha256": summary.get("exe_sha256"),
                "exe_bytes": summary.get("exe_bytes"),
                "started_wall": summary.get("started_wall"),
                "command": summary.get("command"),
                "env_overrides": summary.get("env_overrides"),
            }
        )
    digests = sorted(
        {(sample["exe_sha256"], sample["exe_bytes"]) for sample in samples if sample["exe_sha256"]}
    )
    measured = sorted(path.name[: -len(".err.txt")] for path in directory.glob(f"{prefix}*.err.txt"))
    matched = sorted({sample["label"] for sample in samples if sample["label"]})
    recorded = bool(samples) and len(digests) == 1 and matched == measured
    return {
        "method": "recorded" if recorded else "unrecorded",
        "measured_labels": measured,
        "matched_labels": matched,
        "distinct_player": [{"sha256": digest[0], "bytes": digest[1]} for digest in digests],
        "samples": samples,
    }


def environment_at_summarise(recovery: dict) -> dict:
    """Environment for a stamp reconstructed from logs.

    The measured processes are gone, so the digest this process can see is not evidence about
    them. Use the digest the watcher recorded when it exists; otherwise keep the shape of
    environment(), null the binding, and name the observed digest for what it is.
    """
    environment_now = environment()
    observed = {
        "path": environment_now["player"],
        "sha256": environment_now["player_sha256"],
        "bytes": environment_now["player_bytes"],
    }
    if recovery["method"] == "recorded":
        player = recovery["distinct_player"][0]
        environment_now["player_sha256"] = player["sha256"]
        environment_now["player_bytes"] = player["bytes"]
        environment_now["player_binding"] = "recorded by watch_window.py before each launch"
        environment_now["player_binding_note"] = (
            "taken from the exe_sha256 that watch_window.py wrote into the run's own "
            "raw/<label>.json for every measured process, not from the file on disk now; "
            "see binding_recovery"
        )
    else:
        environment_now["player_sha256"] = None
        environment_now["player_bytes"] = None
        environment_now["player_binding"] = "unrecorded"
        environment_now["player_binding_note"] = (
            "summarised from logs after the runs, with no digest recorded at run time; no "
            "raw/<label>.json bound every measured process to one executable"
        )
    environment_now["player_at_summarise"] = observed
    return environment_now


def stamp_time() -> str:
    return datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds")


def print_table(summary: dict) -> None:
    print(f"{'stage':32s}{'median_us':>12s}{'min':>10s}{'max':>10s}{'spread%':>9s}{'self':>12s}")
    for name in FOCUS:
        entry = summary["stages"].get(name)
        if not entry:
            continue
        print(
            f"{name:32s}{entry['median_us']:12.1f}{entry['min_us']:10d}{entry['max_us']:10d}"
            f"{str(entry['spread_pct']):>9s}{entry['self_median_us']:12.1f}"
        )
    print(
        f"{'content_prepared':32s}{summary['content_prepared_us']['median_us']:12.1f}"
        f"{'':10s}{'':10s}{str(summary['content_prepared_us']['spread_pct']):>9s}"
    )
    print(
        f"{'outside (no scope)':32s}{summary['outside_us']['median_us']:12.1f}"
        f"{'':10s}{'':10s}{str(summary['outside_us']['spread_pct']):>9s}"
    )


def compare(before: dict, after: dict) -> dict:
    rows = []
    for name in FOCUS:
        old = before["summary"]["stages"].get(name, {}).get("median_us")
        new = after["summary"]["stages"].get(name, {}).get("median_us")
        if old is None or new is None:
            continue
        rows.append(
            {
                "stage": name,
                "before_us": old,
                "after_us": new,
                "delta_us": round(new - old, 1),
                "delta_pct": round((new - old) / old * 100.0, 2) if old else None,
            }
        )
    old_prepared = before["summary"]["content_prepared_us"]["median_us"]
    new_prepared = after["summary"]["content_prepared_us"]["median_us"]
    return {
        "before": before.get("label", before.get("stamp", "before")),
        "after": after.get("label", after.get("stamp", "after")),
        "rows": rows,
        "content_prepared_us": {"before": old_prepared, "after": new_prepared},
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="command", required=True)
    run = sub.add_parser("run", help="measure a fresh stamp")
    run.add_argument("--stamp", required=True)
    run.add_argument("--count", type=int, default=N_SAMPLES)
    run.add_argument("--out", default=None)
    summarise = sub.add_parser("summarise", help="analyse an existing stamp directory")
    summarise.add_argument("--dir", required=True)
    summarise.add_argument("--prefix", required=True)
    summarise.add_argument("--out", required=True)
    summarise.add_argument("--label", required=True)
    diff = sub.add_parser("compare", help="diff two summarised stamps")
    diff.add_argument("--before", required=True)
    diff.add_argument("--after", required=True)
    diff.add_argument("--out", default=None)
    args = parser.parse_args()

    if args.command == "run":
        payload = run_samples(args.stamp, args.count)
        stamp_dir = HERE / "runs" / args.stamp
        stamp = load_stamp(stamp_dir, "player-40k-traced-", f"after: {args.stamp}")
        payload.update({"label": f"after: {args.stamp}", "environment": environment(),
                        "summary": stamp["summary"], "processes": stamp["processes"]})
        target = pathlib.Path(args.out) if args.out else stamp_dir / "prepare-split.json"
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(json.dumps(payload, indent=1, ensure_ascii=True) + "\n", encoding="utf-8")
        print(f"wrote {target}", flush=True)
        print_table(stamp["summary"])
        return 0
    if args.command == "summarise":
        stamp = load_stamp(pathlib.Path(args.dir), args.prefix, args.label)
        recovery = recover_binding(pathlib.Path(args.dir), args.prefix)
        stamp["environment"] = environment_at_summarise(recovery)
        stamp["binding_recovery"] = recovery
        stamp["summarised_at"] = stamp_time()
        target = pathlib.Path(args.out)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(json.dumps(stamp, indent=1, ensure_ascii=True) + "\n", encoding="utf-8")
        print(f"wrote {target}", flush=True)
        print_table(stamp["summary"])
        return 0
    before = json.loads(pathlib.Path(args.before).read_text(encoding="utf-8"))
    after = json.loads(pathlib.Path(args.after).read_text(encoding="utf-8"))
    result = compare(before, after)
    print(f"{'stage':32s}{'before_us':>12s}{'after_us':>12s}{'delta_us':>12s}{'delta%':>9s}")
    for row in result["rows"]:
        print(
            f"{row['stage']:32s}{row['before_us']:12.1f}{row['after_us']:12.1f}"
            f"{row['delta_us']:12.1f}{str(row['delta_pct']):>9s}"
        )
    prepared = result["content_prepared_us"]
    print(f"{'content_prepared':32s}{prepared['before']:12.1f}{prepared['after']:12.1f}")
    if args.out:
        pathlib.Path(args.out).write_text(
            json.dumps(result, indent=1, ensure_ascii=True) + "\n", encoding="utf-8"
        )
        print(f"wrote {args.out}", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())

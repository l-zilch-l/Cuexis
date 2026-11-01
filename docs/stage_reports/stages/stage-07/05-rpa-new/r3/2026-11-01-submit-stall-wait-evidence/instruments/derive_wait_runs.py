"""Count wait episodes on the render thread from a recorded sample stream.

`analyze_wait.py` reports the *share of time* a thread spends inside a syscall. A share of time is
not a count of calls: one 2 ms wait and fifty 40 us waits are indistinguishable. This script
re-reads the same archived stream and reports the episodes instead, so the per-frame cost of the
wait can be compared across two configurations that run at different frame rates.

Definitions, so the numbers can be argued with:

  * An *episode* is a maximal run of consecutive render-thread samples that all resolve to the same
    syscall name, allowing non-wait samples to appear between them only if the gap is at most
    `--gap-us` (default 0, so a strictly contiguous run).
  * A sample's cost is the time to the next render-thread sample, so an episode that one sample wide
    is still charged one sampling interval. Episodes are therefore a *lower bound* on the number of
    driver calls, and their durations are quantised by the sampling rate (about 156 us per thread at
    the registered settings). Both biases are stated in the report; neither affects the comparison of
    two configurations measured with the same instrument.
  * Everything here is derived from bytes that are already stored next to it: no new measurement.

Usage:
  python -B derive_wait_runs.py --samples <dir>/*.samples.txt --wait <same>.wait.json \
      --modules <same>.modules.txt --frames <probe frames> --out <same>.runs.json
"""

import argparse
import json
import sys
from pathlib import Path

import analyze_wait as aw
import pe_bytes

WAIT_NAME = "NtWaitForSingleObject"


def episodes(main_rows, gap_us):
    """Group contiguous same-syscall samples; yields (name, rows) for runs of at least one sample."""
    out = []
    current_name = None
    current = []
    for row in main_rows:
        name = row["call"]["name"] if row["call"] else None
        if current and name != current_name:
            gap = row["us"] - current[-1]["us"]
            if name is not None and gap <= gap_us:
                current.append(row)
                continue
            out.append((current_name, current))
            current = []
        if name is None:
            if current:
                out.append((current_name, current))
                current = []
            continue
        current_name = name
        current.append(row)
    if current:
        out.append((current_name, current))
    return out


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--samples", required=True)
    parser.add_argument("--wait", required=True, help="analyze_wait.py output for the same stream")
    parser.add_argument("--modules", required=True)
    parser.add_argument("--ntdll", default=r"C:\Windows\System32\ntdll.dll")
    parser.add_argument("--frames", type=int, required=True)
    parser.add_argument("--fps", type=float, default=None)
    parser.add_argument("--gap-us", type=int, default=0)
    parser.add_argument("--out", default=None)
    args = parser.parse_args(argv)

    wait = json.loads(Path(args.wait).read_text(encoding="utf-8"))
    main_tid = wait["main_thread"]
    modules = aw.Modules(aw.load_modules(args.modules))
    stubs = pe_bytes.stub_table(args.ntdll)

    main_rows = []
    with open(args.samples, "r", encoding="utf-8") as stream:
        for line in stream:
            if line.startswith("#") or not line.strip():
                continue
            parts = line.strip().split(",")
            if len(parts) < 6 or int(parts[1]) != main_tid:
                continue
            rip = int(parts[2], 16)
            module, rva = modules.locate(rip)
            call = None
            if module.lower() in aw.NTDLL_NAMES:
                found = pe_bytes.resolve(stubs, rva)
                if found:
                    call = {"name": found[0], "ssn": found[1], "offset": found[2]}
            main_rows.append({"us": int(parts[0]), "module": module, "rva": rva, "call": call})
    if not main_rows:
        raise SystemExit("no samples for the render thread")

    total_us = main_rows[-1]["us"] - main_rows[0]["us"]
    # Cost of each sample: time until the next render-thread sample.
    for index, row in enumerate(main_rows[:-1]):
        row["cost_us"] = main_rows[index + 1]["us"] - row["us"]
    main_rows[-1]["cost_us"] = 0

    runs = episodes(main_rows, args.gap_us)
    by_name = {}
    for name, group in runs:
        if name is None:
            continue
        entry = by_name.setdefault(name, {"episodes": 0, "sampled_us": 0, "widest_us": 0,
                                          "narrowest_us": None, "spans": []})
        span = sum(row["cost_us"] for row in group)
        entry["episodes"] += 1
        entry["sampled_us"] += span
        entry["widest_us"] = max(entry["widest_us"], span)
        narrow = group[-1]["us"] - group[0]["us"]
        if entry["narrowest_us"] is None or narrow < entry["narrowest_us"]:
            entry["narrowest_us"] = narrow
        entry["spans"].append(span)

    duration_s = total_us / 1e6
    fps = args.fps if args.fps else args.frames / duration_s
    summary = {}
    for name, entry in sorted(by_name.items(), key=lambda item: -item[1]["episodes"]):
        spans = sorted(entry.pop("spans"))
        entry["time_share"] = entry["sampled_us"] / total_us if total_us else 0.0
        entry["episodes_per_s"] = entry["episodes"] / duration_s if duration_s else 0.0
        entry["episodes_per_frame"] = entry["episodes"] / args.frames if args.frames else 0.0
        entry["time_us_per_frame"] = entry["sampled_us"] / args.frames if args.frames else 0.0
        entry["median_us"] = spans[len(spans) // 2]
        entry["p99_us"] = spans[min(len(spans) - 1, int(0.99 * len(spans)))]
        summary[name] = entry

    report = {
        "scope": "episode count and cost on the render thread, re-read from the archived sample "
                 "stream; derived analysis, not a new measurement",
        "samples": args.samples,
        "wait_analysis": args.wait,
        "main_thread": main_tid,
        "main_samples": len(main_rows),
        "duration_us": total_us,
        "frames": args.frames,
        "fps": fps,
        "gap_us": args.gap_us,
        "calls": summary,
    }
    text = json.dumps(report, indent=2) + "\n"
    if args.out:
        Path(args.out).write_text(text, encoding="utf-8", newline="\n")
    wait_entry = summary.get(WAIT_NAME, {})
    print("{} tid={} frames={} fps={:.3f} duration={:.3f}s".format(
        Path(args.samples).name, main_tid, args.frames, fps, duration_s), flush=True)
    print("  {:<28} {:>8} {:>10} {:>12} {:>10} {:>10}".format(
        "call", "episodes", "per frame", "time us/frame", "median us", "p99 us"), flush=True)
    for name, entry in list(summary.items())[:8]:
        print("  {:<28} {:>8} {:>10.2f} {:>12.1f} {:>10} {:>10}".format(
            name, entry["episodes"], entry["episodes_per_frame"], entry["time_us_per_frame"],
            entry["median_us"], entry["p99_us"]), flush=True)
    if not wait_entry:
        print("  no {} episode".format(WAIT_NAME), flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())

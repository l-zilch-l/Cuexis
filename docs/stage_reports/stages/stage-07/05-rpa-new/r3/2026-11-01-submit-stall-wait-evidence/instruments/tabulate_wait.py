"""Tabulate the registered wait groups and apply the PLAN-WAIT.md decision rule.

Inputs are the per-process probe results (`results-*.json`), the per-process wait analysis
(`*.wait.json`), the per-process episode derivation (`*.runs.json`) and, when it exists, the handle
table (`*.handles.txt`). Everything is read from the recorded bytes; nothing is re-measured.

The decision rule is the one registered in `PLAN-WAIT.md` section 2:
  * B/A >= 0.75 on wait episodes per frame -> H5 (background per-frame driver wait)
  * B/A <= 0.50                          -> H6 (stall-only wait)
  * otherwise -> undecided, and the repeatability caveat is reported instead.
"""

import argparse
import json
import pathlib
import statistics
import sys

import pe_bytes
import analyze_wait as aw

WAIT_NAME = "NtWaitForSingleObject"


def spread(values):
    """Repeatability in the project's own units: (max - min) / median, as a fraction."""
    if not values:
        return None
    low, high = min(values), max(values)
    middle = statistics.median(values)
    return (high - low) / middle if middle else None


def wait_targets(group, stem, main_tid):
    """Which kernel object the render thread blocks on, from the handle table of the same run."""
    table_path = group / (stem + ".handles.txt")
    samples_path = group / (stem + ".samples.txt")
    if not table_path.exists():
        return None
    table = aw.load_handles(table_path)
    modules = aw.Modules(aw.load_modules(group / (stem + ".modules.txt")))
    stubs = pe_bytes.stub_table(r"C:\Windows\System32\ntdll.dll")
    counts = {}
    total = 0
    with open(samples_path, "r", encoding="utf-8") as stream:
        for line in stream:
            if line.startswith("#") or not line.strip():
                continue
            parts = line.strip().split(",")
            if len(parts) < 6 or int(parts[1]) != main_tid:
                continue
            module, rva = modules.locate(int(parts[2], 16))
            if module.lower() not in aw.NTDLL_NAMES:
                continue
            found = pe_bytes.resolve(stubs, rva)
            if not found or found[0] != WAIT_NAME or found[2] < 19:
                continue
            value = int(parts[5], 16)
            key = "{:#06x} {}".format(value, aw.label(table, value))
            counts[key] = counts.get(key, 0) + 1
            total += 1
    if not total:
        return {"samples": 0}
    ranked = sorted(counts.items(), key=lambda item: -item[1])
    return {"samples": total,
            "distinct": len(counts),
            "targets": [{"target": key, "samples": count, "share": count / total}
                        for key, count in ranked]}


def collect(label, group, want_frames):
    rows = []
    for results_path in sorted(group.glob("results-*.json")):
        stem = results_path.name[len("results-"):-len(".json")]
        results = json.loads(results_path.read_text(encoding="utf-8"))
        wait = json.loads((group / (stem + ".wait.json")).read_text(encoding="utf-8"))
        runs = json.loads((group / (stem + ".runs.json")).read_text(encoding="utf-8"))
        wait_entry = runs["calls"].get(WAIT_NAME, {})
        exit_stem = stem.rsplit("-", 1)[0]
        rows.append({
            "group": label,
            "index": results["index"],
            "entities": results["entities"],
            "mode": results["mode"],
            "frames": results["frames"],
            "wall_fps": results["wall_fps"],
            "cycle_p50_us": results["summary"]["cycle_us"]["p50_us"],
            "cycle_p99_us": results["summary"]["cycle_us"]["p99_us"],
            "submit_p50_us": results["summary"].get("submit_us", {}).get("p50_us"),
            "present_p50_us": results["summary"].get("present_us", {}).get("p50_us"),
            "slow_frames": results["slow_frames"]["count"],
            "slow_share": results["slow_frames"]["share"],
            "min_window": results["min_window"],
            "budget_pass": results["budget_pass"],
            "main_thread": wait["main_thread"],
            "main_samples": wait["main_thread_samples"],
            "main_module_share": wait["main_module_share"],
            "main_kernel_call_share": wait["main_kernel_call_share"],
            "main_calls": wait["main_calls"],
            "main_waits_pooled": wait["main_waits"],
            "wait_episodes": wait_entry.get("episodes"),
            "wait_episodes_per_frame": wait_entry.get("episodes_per_frame"),
            "wait_episodes_per_s": wait_entry.get("episodes_per_s"),
            "wait_us_per_frame": wait_entry.get("time_us_per_frame"),
            "wait_time_share": wait_entry.get("time_share"),
            "wait_median_episode_us": wait_entry.get("median_us"),
            "wait_p99_episode_us": wait_entry.get("p99_us"),
            "wait_targets": wait_targets(group, stem, wait["main_thread"]),
            "results_sha256": results["identity"].get("results_sha256"),
            "samples_sha256": results["samples_sha256"],
            "log_sha256": results["log_sha256"],
            "started_utc": results["started_utc"],
            "exit": results["exit"],
            "problems": results["problems"],
        })
    return rows


def summarise(rows):
    def column(key):
        return [row[key] for row in rows if row[key] is not None]

    return {
        "processes": len(rows),
        "frames": sum(row["frames"] for row in rows),
        "wall_fps": {"median": statistics.median(column("wall_fps")),
                     "spread": spread(column("wall_fps")), "values": column("wall_fps")},
        "cycle_p99_us": {"median": statistics.median(column("cycle_p99_us")),
                         "spread": spread(column("cycle_p99_us")),
                         "values": column("cycle_p99_us")},
        "slow_share": {"median": statistics.median(column("slow_share")),
                       "spread": spread(column("slow_share")), "values": column("slow_share")},
        "wait_episodes_per_frame": {
            "median": statistics.median(column("wait_episodes_per_frame")),
            "spread": spread(column("wait_episodes_per_frame")),
            "values": column("wait_episodes_per_frame")},
        "wait_us_per_frame": {
            "median": statistics.median(column("wait_us_per_frame")),
            "spread": spread(column("wait_us_per_frame")),
            "values": column("wait_us_per_frame")},
        "wait_time_share": {
            "median": statistics.median(column("wait_time_share")),
            "spread": spread(column("wait_time_share")),
            "values": column("wait_time_share")},
        "wait_median_episode_us": {
            "median": statistics.median(column("wait_median_episode_us")),
            "values": column("wait_median_episode_us")},
        "main_kernel_call_share": {
            "median": statistics.median(column("main_kernel_call_share")),
            "values": column("main_kernel_call_share")},
    }


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".")
    parser.add_argument("--stall", default="wait-2800-static")
    parser.add_argument("--control", default="wait-1800-static")
    parser.add_argument("--identity", default="wait-2800-static-handles")
    parser.add_argument("--out", default="wait-groups.tabulated.json")
    args = parser.parse_args(argv)

    root = pathlib.Path(args.root)
    groups = {
        "A": collect("A", root / args.stall, 2800),
        "B": collect("B", root / args.control, 1800),
        "identity": collect("identity", root / args.identity, 2800),
    }
    for label, rows in groups.items():
        if not rows:
            raise SystemExit("no results for group {}".format(label))

    summary = {label: summarise(rows) for label, rows in groups.items()}
    a = summary["A"]["wait_episodes_per_frame"]["median"]
    b = summary["B"]["wait_episodes_per_frame"]["median"]
    ratio = b / a if a else None
    a_us = summary["A"]["wait_us_per_frame"]["median"]
    b_us = summary["B"]["wait_us_per_frame"]["median"]
    verdict = None
    if ratio is not None:
        if ratio >= 0.75:
            verdict = "H5"
        elif ratio <= 0.50:
            verdict = "H6"
        else:
            verdict = "undecided"

    repeatable = {
        "A": all(summary["A"][key]["spread"] is not None
                 and summary["A"][key]["spread"] <= 0.15
                 for key in ("wait_episodes_per_frame", "wait_us_per_frame", "wall_fps")),
        "B": all(summary["B"][key]["spread"] is not None
                 and summary["B"][key]["spread"] <= 0.15
                 for key in ("wait_episodes_per_frame", "wait_us_per_frame", "wall_fps")),
    }

    report = {
        "scope": "derived tabulation of the registered wait groups; reads recorded bytes only",
        "plan": "PLAN-WAIT.md",
        "decision_rule": {"h5_ratio_at_least": 0.75, "h6_ratio_at_most": 0.50},
        "comparison": {
            "episodes_per_frame_A": a,
            "episodes_per_frame_B": b,
            "ratio_B_over_A": ratio,
            "verdict": verdict,
            "us_per_frame_A": a_us,
            "us_per_frame_B": b_us,
            "ratio_us_per_frame": (b_us / a_us) if a_us else None,
            "median_episode_us_A": summary["A"]["wait_median_episode_us"]["median"],
            "median_episode_us_B": summary["B"]["wait_median_episode_us"]["median"],
            "entities_A": 2800,
            "entities_B": 1800,
            "marginal_wait_us_per_entity": ((a_us - b_us) / 1000.0)
            if a_us and b_us else None,
        },
        "repeatability_within_15_percent": repeatable,
        "summary": summary,
        "processes": groups,
    }
    text = json.dumps(report, indent=2, sort_keys=False) + "\n"
    (root / args.out).write_text(text, encoding="utf-8", newline="\n")

    def line(row):
        return ("{g:>3} {i:>2} {e:>5} {f:>5} {fps:>8.2f} {p50:>8.1f} {p99:>8.1f} {sf:>5} "
                "{ss:>7.4f} {ep:>6.2f} {us:>9.1f} {sh:>7.4f} {med:>7.0f} {kp:>6.4f}").format(
            g=row["group"], i=row["index"], e=row["entities"], f=row["frames"],
            fps=row["wall_fps"], p50=row["cycle_p50_us"], p99=row["cycle_p99_us"],
            sf=row["slow_frames"], ss=row["slow_share"], ep=row["wait_episodes_per_frame"],
            us=row["wait_us_per_frame"], sh=row["wait_time_share"],
            med=row["wait_median_episode_us"], kp=row["main_kernel_call_share"])

    header = ("{g:>3} {i:>2} {e:>5} {f:>5} {fps:>8} {p50:>8} {p99:>8} {sf:>5} {ss:>7} "
              "{ep:>6} {us:>9} {sh:>7} {med:>7} {kp:>6}".format(
                  g="grp", i="ix", e="ent", f="frms", fps="fps", p50="p50", p99="p99",
                  sf="slow", ss="share", ep="ep/fr", us="wait us", sh="wshare",
                  med="med ep", kp="kern"))
    print(header)
    for label in ("A", "B", "identity"):
        for row in groups[label]:
            print(line(row))
    print()
    print("wait episodes per frame: A {} vs B {} -> B/A {} -> {}".format(
        a, b, None if ratio is None else round(ratio, 4), verdict))
    print("wait us per frame: A {:.1f} vs B {:.1f} (median episode {:.0f} vs {:.0f} us)".format(
        a_us, b_us, summary["A"]["wait_median_episode_us"]["median"],
        summary["B"]["wait_median_episode_us"]["median"]))
    print("marginal wait cost per extra entity: {:.3f} us".format(
        report["comparison"]["marginal_wait_us_per_entity"]))
    print("repeatable within 15%: {}".format(repeatable))
    identity = groups["identity"][0]["wait_targets"]
    if identity:
        print("wait target (identity process, {} samples):".format(identity["samples"]))
        for target in identity["targets"][:5]:
            print("   {:<28} {:>7} {:6.3f}%".format(
                target["target"], target["samples"], 100.0 * target["share"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())

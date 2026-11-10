"""Turn the frontier ladder into a bracket verdict, applying the pre-registered decision rule.

    python -B summarize_frontier.py

Reads frontier-bracket-summary.json (written by run_frontier_bracket.py) and writes
frontier-bracket-verdict.json plus a Markdown table on stdout. It never interpolates, never averages
across entity counts, and never retries a point.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
BUDGET_US = 1000000.0 / 240.0
REFRESH_ANCHOR = {"render-only": 3210.5, "static": 3725.1}
REFRESH_BINARY = "91ed0d303c09ef92e6abab9e30b63c87da77322d1dc7132c6b45204c220578b9"
MODES = ("render-only", "static")


def load_summary(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def mode_points(summary: dict, mode: str) -> list:
    points = []
    for record in summary["records"]:
        group = record["modes"].get(mode)
        if not group:
            continue
        points.append({
            "entities": record["entities"],
            "passed": bool(group["all_passed_budget"]),
            "p99_us": group["p99_us_median"],
            "p99_all_us": group["p99_us"],
            "fps": group["wall_fps_median"],
            "fps_all": group["wall_fps"],
            "min_second_fps": group["min_second_fps"],
            "p99_range": group["p99_relative_range"],
            "fps_range": group["fps_relative_range"],
            "over_budget": group["over_budget"],
        })
    return sorted(points, key=lambda point: point["entities"])


def bracket(points: list) -> dict:
    prefix = []
    for point in points:
        if not point["passed"]:
            break
        prefix.append(point)
    lower = prefix[-1]["entities"] if prefix else None
    upper = None
    for point in points:
        if lower is not None and point["entities"] > lower and not point["passed"]:
            upper = point["entities"]
            break
    failures_low = [point["entities"] for point in points
                    if not point["passed"] and lower is not None and point["entities"] <= lower]
    passes_high = [point["entities"] for point in points
                   if point["passed"] and upper is not None and point["entities"] > upper]
    if lower is None and points:
        state = "no_passing_point"
    elif upper is None:
        state = "unbracketed_open_above"
    elif failures_low or passes_high:
        state = "non_monotonic"
    else:
        state = "bracketed"
    return {
        "state": state,
        "passing_below_or_at": lower,
        "first_failing_above": upper,
        "failed_counts": [point["entities"] for point in points if not point["passed"]],
        "passing_counts": [point["entities"] for point in points if point["passed"]],
    }


def drift(points: list, mode: str) -> dict:
    anchor = [point for point in points if point["entities"] == 1800]
    if not anchor:
        return {"checked": False, "reason": "no 1800 point in the ladder"}
    measured = anchor[0]["p99_us"]
    reference = REFRESH_ANCHOR[mode]
    relative = (measured - reference) / reference
    return {
        "checked": True,
        "ladder_1800_p99_us": measured,
        "refresh_1800_p99_us": reference,
        "relative_difference": relative,
        "band": "inside 10% of the refresh median" if abs(relative) <= 0.10 else "outside 10%",
    }


def markdown(summary: dict, verdict: dict) -> str:
    lines = [
        "| 实体数 | 仅渲染 wall FPS (中位) | 仅渲染 p99 (ms) | 仅渲染 最小秒窗 | 仅渲染 判定 "
        "| 静态 wall FPS (中位) | 静态 p99 (ms) | 静态 最小秒窗 | 静态 判定 |",
        "| ---: | ---: | ---: | ---: | :---: | ---: | ---: | ---: | :---: |",
    ]
    by_count = {}
    for mode in MODES:
        for point in mode_points(summary, mode):
            by_count.setdefault(point["entities"], {})[mode] = point
    for count in sorted(by_count):
        cells = ["{}".format(count)]
        for mode in MODES:
            point = by_count[count].get(mode)
            if not point:
                cells.extend(["", "", "", ""])
                continue
            cells.extend([
                "{:.3f}".format(point["fps"]),
                "{:.4f}".format(point["p99_us"] / 1000.0),
                "{}".format(min(value for value in point["min_second_fps"])),
                "通过" if point["passed"] else "未通过",
            ])
        lines.append("| " + " | ".join(cells) + " |")
    lines.append("")
    for mode in MODES:
        entry = verdict["modes"][mode]
        lines.append("- `{}`：{}；通过点 {}；失败点 {}；区间 {}。".format(
            mode,
            entry["bracket"]["state"],
            entry["bracket"]["passing_counts"] or "无",
            entry["bracket"]["failed_counts"] or "无",
            "[{}, {})".format(entry["bracket"]["passing_below_or_at"],
                              entry["bracket"]["first_failing_above"])
            if entry["bracket"]["state"] == "bracketed" else "未闭合区间"))
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary", default=str(HERE / "frontier-bracket-summary.json"))
    parser.add_argument("--out", default=str(HERE / "frontier-bracket-verdict.json"))
    args = parser.parse_args()
    summary_path = Path(args.summary)
    summary = load_summary(summary_path)
    verdict = {
        "scope": summary["scope"],
        "head": summary["head"],
        "binary_sha256": summary["binary_sha256"],
        "refresh_binary_sha256": REFRESH_BINARY,
        "binary_matches_refresh": summary["binary_sha256"] == REFRESH_BINARY,
        "completed": summary["completed"],
        "pending": summary["pending"],
        "budget_us": BUDGET_US,
        "modes": {},
    }
    for mode in MODES:
        points = mode_points(summary, mode)
        verdict["modes"][mode] = {
            "points": [
                {key: point[key] for key in ("entities", "passed", "p99_us", "fps",
                                             "min_second_fps", "p99_range", "fps_range",
                                             "over_budget")}
                for point in points
            ],
            "bracket": bracket(points),
            "drift_vs_refresh_1800": drift(points, mode),
        }
    table = markdown(summary, verdict)
    verdict["markdown_table"] = table
    verdict["notes"] = [
        "Diagnostic only. This ladder does not replace the frozen 2026-10-29 confirmed capacity "
        "points and does not constitute R3/R4 or production acceptance.",
        "No point was re-sampled; no value is interpolated between entity counts.",
        "A group whose p99 or FPS relative range exceeds 15% is used only for pass/fail and is "
        "never quoted as a speed reading.",
        "If binary_matches_refresh is false, every published ladder number belongs to its own "
        "build and must not be differenced against the refresh sustain numbers.",
    ]
    Path(args.out).write_text(json.dumps(verdict, indent=2, ensure_ascii=False), encoding="utf-8")
    print(table)
    print()
    print(json.dumps({"completed": verdict["completed"], "pending": verdict["pending"],
                      "binary_matches_refresh": verdict["binary_matches_refresh"]},
                     ensure_ascii=False))
    return 0


if __name__ == "__main__":
    sys.exit(main())

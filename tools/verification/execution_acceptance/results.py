"""Pure fixed-group verdicts; preserve per-process samples, noise and window intervals."""
from tools.verification.playback_performance import measurement


def verdict(windows, comparison):
    if comparison not in ("A/B", "B-only"):
        raise ValueError("P8 is a single-sided full-scale result, not a repeated timing group")
    if len(windows) != 3 or any(len(window) != 3 for window in windows):
        raise ValueError("incomplete fixed three-window group")
    sides = ("A", "B") if comparison == "A/B" else ("B",)
    metrics = set(windows[0][0][sides[0]])
    if not metrics:
        raise ValueError("missing registered primary metrics")
    for window in windows:
        for pair in window:
            if set(pair) != set(sides):
                raise ValueError("missing or substituted side")
            for side in sides:
                if set(pair[side]) != metrics:
                    raise ValueError("missing or extra primary metric")
                for value in pair[side].values():
                    measurement.positive(value)
    output = {}
    for metric in sorted(metrics):
        result = {"repeatability": {
            side: measurement.repeatability([[pair[side][metric] for pair in window]
                                              for window in windows]) for side in sides}}
        if comparison == "A/B":
            groups = [[(pair["A"][metric], pair["B"][metric]) for pair in window]
                      for window in windows]
            result["window_intervals"] = [measurement.paired_interval(group) for group in groups]
            result["full_interval"] = measurement.paired_interval(sum(groups, []))
            result["status"] = result["full_interval"]["status"]
        else:
            result["status"] = "timing_group_complete_absolute_gates_separate"
        if any(row["status"] != "pass" for row in result["repeatability"].values()):
            result["status"] = "inconclusive"
        output[metric] = result
    return {"schema": "rpa.execution.fixed-group-result.v1", "metrics": output}

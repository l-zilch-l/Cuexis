"""Statistics for complete, symmetric PD measurement quartets."""
import math
import statistics as st


def quartet_effect(rows, field="wall_us"):
    if len(rows) != 4 or sum(r["active"] for r in rows) != 2:
        raise ValueError("Expected two active and two dormant samples")
    if [r["active"] for r in rows] not in ([False, True, True, False], [True, False, False, True]):
        raise ValueError("Expected ABBA or BAAB")
    if any(r["sample"][field] <= 0 for r in rows):
        raise ValueError("Ratios require positive observations")
    active = [math.log(r["sample"][field]) for r in rows if r["active"]]
    dormant = [math.log(r["sample"][field]) for r in rows if not r["active"]]
    return st.mean(active) - st.mean(dormant)


def paired_effect(rows, field="wall_us"):
    groups = sorted({r["group"] for r in rows})
    logs = [quartet_effect([r for r in rows if r["group"] == g], field) for g in groups]
    if not logs:
        raise ValueError("No complete quartets")
    return math.expm1(st.mean(logs))


def per_operation(row, field="wall_us"):
    if row["operations"] <= 0:
        raise ValueError("Operation count must be positive")
    return row["sample"][field] / row["operations"]

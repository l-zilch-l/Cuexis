"""Validate an R4 registration draft without launching, accepting, or sealing a run."""
import math


def validate_draft(document):
    if document.get("schema") != "rpa.r4.registration-draft.v1":
        raise ValueError("unexpected draft schema")
    if document.get("formal_registration") is not False:
        raise ValueError("draft cannot register itself")
    if document.get("windows") != 3 or document.get("processes_per_window") != 3:
        raise ValueError("fixed three-by-three protocol required")
    if document.get("gap_seconds") != 600:
        raise ValueError("600 second window gap required")
    if document.get("orders") != ["AB/BA/AB", "BA/AB/BA", "AB/BA/AB"]:
        raise ValueError("fixed pair order changed")
    if document.get("regression_limit") != 1.10 or document.get("noise_limit") != .15:
        raise ValueError("accepted thresholds changed")
    units = document.get("units")
    if (not units or any(not isinstance(u.get("id"), str) or not u["id"] for u in units) or
            len({u["id"] for u in units}) != len(units)):
        raise ValueError("missing or duplicate unit")
    for unit in units:
        if unit.get("workload") not in ("seek", "steady", "control", "memory", "p8"):
            raise ValueError("explicit workload kind required")
        if unit.get("comparison") not in ("A/B", "B-only", "P8-single-sided"):
            raise ValueError("explicit comparison required")
        if (unit["workload"] == "p8") != (unit["comparison"] == "P8-single-sided"):
            raise ValueError("P8 workload requires its exclusive single-sided protocol")
        count = unit.get("requests_per_process")
        if type(count) is not int or count < 1:
            raise ValueError("finite request count required")
        if unit.get("cold_process") and count != 1:
            raise ValueError("cold process cannot include subsequent hot requests")
        if unit["workload"] == "seek":
            seek_class = unit.get("seek_class")
            if seek_class not in ("natural", "hot", "external", "fallback", "cold"):
                raise ValueError("explicit Seek class required")
            if unit.get("cold_process") is not (seek_class == "cold"):
                raise ValueError("Seek class and process temperature disagree")
            if count != (1 if seek_class == "cold" else 16):
                raise ValueError("Seek class has a fixed per-process request count")
        if unit.get("comparison") == "A/B" and unit.get("baseline") != "pre-PD":
            raise ValueError("A/B requires original pre-PD baseline")
        if not unit.get("completion_endpoint") or not unit.get("profile"):
            raise ValueError("profile and endpoint required")
        if unit.get("comparison") == "P8-single-sided" and unit.get("invocations") != 1:
            raise ValueError("P8 low/high require one invocation each")
    return document


def validate_samples(values, required):
    if len(values) != required:
        raise ValueError("missing or substituted process sample")
    if any(type(v) not in (int, float) or not math.isfinite(v) or v <= 0 for v in values):
        raise ValueError("non-positive or non-finite sample")


def formal_blockers(document):
    validate_draft(document)
    required = ("accepted_caps_record", "candidate_sha", "baseline_binding", "fixture_binding",
                "tool_binding", "binary_binding", "pilot_binding", "environment_binding",
                "invalid_process_rules")
    return [key for key in required if not document.get(key)]

"""Fixed case table for the sealed PD-5 ETW tool package.

Every case is one named, bounded envelope. The controller re-derives the whole
envelope from this table, so a claim that disagrees with the table is rejected
before any WPR start.

The two canary cases launch no workload and keep the numbers of the accepted
tool-lifecycle canary. The three f1-scale cases supervise the sealed F1
collector inside the same worker Job. The F1 numbers here are a copy of the
sealed plan.json envelope, cross-checked at preparation time and by the tests;
plan.json stays the authority for the F1 workload and the table never silently
diverges from it.

A case is tooling, not a product budget: none of these numbers is a PD
performance budget, and no case performs formal measurement.
"""

PROFILES = ("canary", "f1-scale")

# Profile markers that a read-only wpr -profiledetails run must report before a
# capture is allowed. The canary marker is the exact minimal lifecycle keyword;
# the F1 markers are the sealed plan's required profile details.
PROFILE_PREFLIGHT_TOKENS = {
    "canary": ("ProcessThread",),
    "f1-scale": (
        "CSwitch",
        "ReadyThread",
        "DPC",
        "Interrupt",
        "Microsoft-Windows-Kernel-Processor-Power",
    ),
}

# Copy of plan.json workload.native_launches: 2 process-start anchors for each of
# the 3 measured processes. Cross-checked at preparation time and by the tests.
F1_NATIVE_LAUNCHES = 6

# The supervisor budget must outlive the collector's own budget, so a healthy
# run is bounded by the collector and only a hung collector is bounded here.
F1_SUPERVISOR_GRACE_SECONDS = 50

CANARY = {
    "profile": "canary",
    "workload": "none",
    "native_launches": 0,
    "deadline_seconds": 120,
    "takeover_seconds": 95,
    "disk_cap_bytes": 134217728,
    "active_logger_cap_seconds": 20,
    "start_cap_seconds": 15,
    "stop_cap_seconds": 60,
    "supervision_seconds": 135,
    "workload_budget_seconds": None,
    "exit_codes": (0,),
}

F1_SCALE = {
    "profile": "f1-scale",
    "workload": "sealed-f1-collector",
    "native_launches": 6,
    "deadline_seconds": 420,
    "takeover_seconds": 380,
    "disk_cap_bytes": 2147483648,
    "active_logger_cap_seconds": None,
    "start_cap_seconds": 20,
    "stop_cap_seconds": 110,
    "supervision_seconds": 480,
    "workload_budget_seconds": 250 + F1_SUPERVISOR_GRACE_SECONDS,
    "exit_codes": (0,),
}


def _case(base, **override):
    entry = dict(base)
    entry.update(override)
    entry["formal_acceptance"] = False
    entry["performance_acceptance"] = False
    return entry


# The timeout case fires 20 s into the first 30 s warmup, so a real native F1
# child is provably alive when the supervisor budget expires.
F1_TIMEOUT_BUDGET_SECONDS = 20

CASES = {
    "normal": _case(CANARY),
    "controller-death": _case(CANARY, exit_codes=(73,)),
    "f1-scale": _case(F1_SCALE),
    "f1-scale-timeout": _case(
        F1_SCALE,
        workload_budget_seconds=F1_TIMEOUT_BUDGET_SECONDS,
        exit_codes=(74,),
    ),
    "f1-scale-death": _case(F1_SCALE, exit_codes=(73,)),
}

CANARY_CASES = tuple(name for name, spec in CASES.items() if spec["workload"] == "none")
F1_CASES = tuple(name for name, spec in CASES.items() if spec["workload"] != "none")


def spec(case) -> dict:
    """Return the fixed envelope for one case name; unknown names fail closed."""
    try:
        return CASES[case]
    except (KeyError, TypeError):
        raise RuntimeError("unknown fixed tool case: " + repr(case)) from None


def cases_for(profile) -> tuple:
    """Return the fixed case names that belong to one profile, in table order."""
    if profile not in PROFILES:
        raise RuntimeError("unknown fixed tool profile: " + repr(profile))
    return tuple(name for name, entry in CASES.items() if entry["profile"] == profile)


def envelope(case) -> dict:
    """Return a serialisable copy of one envelope, without tuple members."""
    entry = dict(spec(case))
    entry["exit_codes"] = list(entry["exit_codes"])
    return entry

"""Pre-declared presentation-matrix probe units; counters are derived, never observed.

The original PD-5 matrix harness fixes one real-time frame per 1/240 s, a 10 s warmup and a
60 s measured window. This module states that workload once so a registration, its pilot and
its runner verdict cannot silently change the counts after a slow reading was seen.
"""

FRAMES = 14400
WARMUP_FRAMES = 2400
RELOAD_INTERVAL = 256
SMOKE_RELOAD_INTERVAL = 4
BASE_ENTITIES = 1
SPARSE_ENTITIES = 200
TOPOLOGY_ENTITIES = 201
ENDPOINT = "delivery-ready"
HARNESS = "presentation-matrix"
PROBE_PREFIX = "PD5 presentation workload matrix"

# mode -> (capture consumers, updates per frame, sparse, reloads at the fixed interval)
MODES = {
    "no-consumer": (0, 1, False, False),
    "low-delivery": (1, 8, False, False),
    "capture-1": (1, 1, False, False),
    "capture-2": (2, 1, False, False),
    "capture-4": (4, 1, False, False),
    "sparse": (1, 1, True, False),
    "topology": (1, 1, True, True),
}

# Registered R4 units. P3, P7 and the device capture variants need the GPU/audio adapter and
# are deliberately absent: an unavailable endpoint is never filled with this CPU harness.
UNITS = (
    {"id": "p1-no-consumer", "gate": "p1-no-consumer", "mode": "no-consumer",
     "profile": "presentation-no-consumer-v1", "metrics": ["sdk_update"]},
    {"id": "p2-low-delivery", "gate": "p2-low-delivery", "mode": "low-delivery",
     "profile": "presentation-low-delivery-v1", "metrics": ["sdk_update", "capture_delivery"]},
    {"id": "p4-capture-1", "gate": "p4-capture-1", "mode": "capture-1",
     "profile": "presentation-capture-1-v1", "metrics": ["capture_delivery"]},
    {"id": "p4-capture-2", "gate": "p4-capture-2", "mode": "capture-2",
     "profile": "presentation-capture-2-v1", "metrics": ["capture_delivery"]},
    {"id": "p4-capture-4", "gate": "p4-capture-4", "mode": "capture-4",
     "profile": "presentation-capture-4-v1", "metrics": ["capture_delivery"]},
    {"id": "p6-sparse", "gate": "p6-sparse", "mode": "sparse",
     "profile": "presentation-sparse-200-v1", "metrics": ["capture_delivery"]},
    {"id": "p6-topology", "gate": "p6-topology", "mode": "topology",
     "profile": "presentation-topology-200-201-v1",
     "metrics": ["capture_delivery", "topology_reload"]},
)


GATES = frozenset(unit["gate"] for unit in UNITS)


def registered(unit_id):
    for unit in UNITS:
        if unit["id"] == unit_id:
            return dict(unit)
    raise ValueError("unknown probe unit: " + str(unit_id))


def reload_count(warmup, frames, smoke=False):
    """The harness reloads before any measured frame that is a multiple of the interval."""
    interval = SMOKE_RELOAD_INTERVAL if smoke else RELOAD_INTERVAL
    return sum(1 for index in range(1, warmup + frames) if index % interval == 0)


def measured_reloads(warmup, frames):
    return sum(1 for index in range(warmup, warmup + frames) if index % RELOAD_INTERVAL == 0)


def counts(unit, frames=FRAMES, warmup=WARMUP_FRAMES, smoke=False):
    consumers, ratio, _, reloading = MODES[unit["mode"]]
    values = {"sdk_update": frames * ratio, "materialize": frames * consumers,
              "capture_delivery": frames * consumers}
    if reloading:
        values["topology_reload"] = 0 if smoke else measured_reloads(warmup, frames)
    return {name: value for name, value in values.items() if value}


def identity(unit, frames=FRAMES, warmup=WARMUP_FRAMES, smoke=False):
    """The exact identity a registered process must reproduce; no field is measured first."""
    consumers, ratio, sparse, reloading = MODES[unit["mode"]]
    maximum = TOPOLOGY_ENTITIES if reloading else SPARSE_ENTITIES if sparse else BASE_ENTITIES
    base = SPARSE_ENTITIES if sparse else BASE_ENTITIES
    return {"schema": "pd5.probe.v1", "mode": unit["mode"], "harness": HARNESS,
            "metrics_compile_enabled": False, "allocator": "normal", "smoke": smoke,
            "frames": frames, "warmup_frames": warmup, "updates": frames * ratio,
            "materializations": frames * consumers, "capture_consumers": consumers,
            "renderer_consumers": 0, "source_step_ticks": 7000, "source_tick_rate": 1000000,
            "base_entities": base, "max_entities": maximum,
            "reloads": reload_count(warmup, frames, smoke) if reloading else 0,
            "delivery_generations": (reload_count(warmup, frames, smoke) if reloading else 0) + 1,
            "payload_charge_per_frame": 1048576, "payload_live_after_close": 0,
            "attempt_live_after_close": 0, "gameplay": False, "replay_equal": None,
            "audio": False, "gpu": False, "closed": True, "counts": counts(unit, frames, warmup, smoke)}


def expected_fields(unit, frames=FRAMES, warmup=WARMUP_FRAMES, smoke=False):
    """Identity fields a runner compares. Digest and peak counters are observed, not asserted."""
    value = identity(unit, frames, warmup, smoke)
    for key in ("schema", "smoke", "payload_peak_count", "payload_peak_bytes", "digest"):
        value.pop(key, None)
    return value


def environment(unit, frames=FRAMES, warmup=WARMUP_FRAMES, smoke=False):
    return {"CUEXIS_PD5_MODE": unit["mode"], "CUEXIS_PD5_FRAMES": str(frames),
            "CUEXIS_PD5_WARMUP": str(warmup), "CUEXIS_PD5_SMOKE": "1" if smoke else "0"}


def select(names):
    """Explicit, ordered selection; an unknown or repeated name is a registration error."""
    if not names or names == ["all"]:
        return [registered(unit["id"]) for unit in UNITS]
    chosen = [registered(name) for name in names]
    if len({unit["id"] for unit in chosen}) != len(chosen):
        raise ValueError("duplicate probe unit selection")
    return chosen


def validate(unit):
    if unit.get("mode") not in MODES:
        raise ValueError("unknown probe mode")
    if unit.get("gate") not in GATES:
        raise ValueError("unknown probe gate")
    if not unit.get("metrics"):
        raise ValueError("explicit probe metric list required")
    return unit


REGISTRATION_IDENTITY = ("mode", "harness", "frames", "warmup_frames", "updates",
                         "materializations", "capture_consumers", "renderer_consumers",
                         "base_entities", "max_entities", "payload_charge_per_frame", "gameplay",
                         "audio", "gpu", "closed", "counts")


def validate_registration(unit):
    """Shape rules for an emitted registration record, shared by admission and the runner."""
    if unit.get("workload") != "steady" or unit.get("gate") not in GATES:
        raise ValueError("probe adapter requires a registered presentation-matrix gate")
    if unit.get("comparison") != "B-only":
        raise ValueError("probe units are B-only; A is N/A rather than zero")
    identity = unit.get("expected_identity") or {}
    if identity.get("frames") != unit.get("requests_per_process"):
        raise ValueError("probe request count differs from the registered frame count")
    missing = [key for key in REGISTRATION_IDENTITY if key not in identity]
    if missing:
        raise ValueError("missing pre-declared probe identity: " + ", ".join(missing))
    return unit

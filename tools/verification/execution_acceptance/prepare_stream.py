"""One bounded P3 device-host stream pilot; never claims a formal attempt.

This is the original gpu-stream-v1 host workload, not the renderer/capture matrix. Its fixed
one-entity fixture advances 14,400 frames at 240 Hz after 2,400 warmup frames. GPU timers stay off;
the normal build measures CPU host endpoints and does not admit GPU-time or memory budgets.
"""
from pathlib import Path
import sys

from tools.verification.playback_performance import measurement

from . import prepare_controls
from .binding import artifact

GATE = "p3-renderer"
BACKEND = "device"
STABLE = GATE + "-device-release"
ADAPTER = "stream"
PROBE_EXECUTABLE = "cuexis_execution_device_probe.exe"
PROBE_TEST = prepare_controls.PROBE_TEST
ENDPOINT = "request-to-present-return"
FIXTURE = "pd5-device-stream"
PROFILE = "gpu-stream-v1"
FRAMES = 14400
WARMUP = 2400
CYCLES = 10
RATE = 240
METRICS = ("request_to_ready", "request_to_present_return", "cpu_submit", "present_call",
           "load_host", "close")
PILOT_SCHEMA = "rpa.execution.stream-pilot-declaration.v1"
PILOT_FIELDS = {"backend": BACKEND, "entities": 1, "frames": FRAMES,
                "warmup_frames": WARMUP, "gpu_timer": False}
SCOPE = ("one-entity gpu-stream-v1 host workload; CPU request-to-ready, present-return, submit, "
         "present-call and lifecycle endpoints only; no GPU timer, audio, capture matrix, Seek, "
         "memory or A-side comparison is admitted by this proof")


def counts():
    """Native samples only: two lifecycle samples and four samples per measured frame."""
    return {"load_host": 1, "close": 1, "request_to_ready": FRAMES,
            "request_to_present_return": FRAMES, "cpu_submit": FRAMES, "present_call": FRAMES}


def sample_count():
    """The parser adds one process-start-to-first-frame load sample to the 57,602 native rows."""
    return sum(counts().values()) + 1


def environment():
    return {"CUEXIS_PD5_MODE": "stream", "CUEXIS_PD5_CYCLES": str(CYCLES),
            "CUEXIS_PD5_RATE": str(RATE), "CUEXIS_PD5_FRAMES": str(FRAMES),
            "CUEXIS_PD5_WARMUP": str(WARMUP), "CUEXIS_PD5_SMOKE": "0",
            "CUEXIS_PD5_GPU_TIMER": "0", "SDL_WINDOW_ACTIVATE_WHEN_SHOWN": "0",
            "SDL_WINDOW_ACTIVATE_WHEN_RAISED": "0"}


def launch(probe, warmup, root, affinity):
    spec = prepare_controls.launch(probe, warmup, root, affinity)
    spec["environment"] = environment()
    return spec


def expected_fields(digest):
    """All registered inputs plus the pilot's exact output digest; peaks remain observations."""
    return {"schema": "pd5.probe.v1", "mode": "stream", "metrics_compile_enabled": False,
            "allocator": "normal", "smoke": False, "gpu_timer": False, "gpu": True,
            "frames": FRAMES, "warmup_frames": WARMUP, "cycles": CYCLES, "rate": RATE,
            "source_step_ticks": 7000, "source_tick_rate": 1000000, "audio": False,
            "payload_after_close": 0, "closed": True, "replay_equal": True,
            "digest": digest, "counts": counts()}


IDENTITY_FIELDS = tuple(expected_fields(0))


def validate_identity(value):
    if not isinstance(value, dict):
        raise ValueError("stream requires a complete identity record")
    digest = value.get("digest")
    if type(digest) is not int or not 0 <= digest < 2**64:
        raise ValueError("stream pilot requires an exact unsigned FrameDigest")
    expected = expected_fields(digest)
    if any(type(value.get(key)) is not type(wanted) or value[key] != wanted
           for key, wanted in expected.items()):
        raise ValueError("stream identity differs from the fixed gpu-stream-v1 workload")
    if any(type(number) is not int for number in value["counts"].values()):
        raise ValueError("stream sample counters must be integers")
    return expected


def identity(parsed):
    expected = validate_identity(parsed["identity"])
    if len(parsed.get("samples", [])) != sample_count():
        raise ValueError("stream pilot requires all native samples and the process-start load")
    for metric in (*METRICS, "load"):
        if metric not in parsed["statistics"]:
            raise ValueError("stream pilot is missing the endpoint: " + metric)
        measurement.positive(parsed["statistics"][metric]["median_us"])
    return expected


def validate_registration(unit):
    required = {"id": GATE, "gate": GATE, "adapter": ADAPTER, "backend": BACKEND,
                "stable_protocol_id": STABLE, "workload": "steady", "config": "Release",
                "comparison": "B-only", "profile": PROFILE, "fixture": FIXTURE,
                "completion_endpoint": ENDPOINT, "measurement_mode": "normal",
                "requests_per_process": sample_count()}
    if any(unit.get(key) != value for key, value in required.items()):
        raise ValueError("stream registration requires the canonical B-only device host unit")
    if unit.get("required_metrics") != list(METRICS):
        raise ValueError("stream registration must retain every declared metric")
    expected = unit.get("expected_identity") or {}
    if expected != validate_identity(expected):
        raise ValueError("stream registration contains an unregistered identity field")
    spec = unit.get("launch", {}).get("B", {})
    command = spec.get("command", [])
    if (len(command) != 2 or Path(command[0]).name != PROBE_EXECUTABLE or
            command[1] != PROBE_TEST or spec.get("environment") != environment()):
        raise ValueError("stream registration requires its exact device executable and launch")
    return unit


def fixture_bindings(root):
    return {FIXTURE: artifact(root / "tests/pd/pd5_host_probe.cpp"),
            "device-host-probe": artifact(root / "tests/pd/pd5_device_probe.cpp"),
            "host-content-fixture": artifact(root / "tests/rpa/execution_fixture.hpp"),
            "probe-environment": artifact(root / "tests/pd/measurement_environment.hpp")}


def run(args):
    return prepare_controls.run(args, profile=sys.modules[__name__])


def main():
    prepare_controls.main(profile=sys.modules[__name__])


if __name__ == "__main__":
    main()

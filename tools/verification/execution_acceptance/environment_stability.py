"""Hold the workstation awake, keep the input idle timer reset, and sample the formal environment.

This produces remedy evidence only: it never launches a workload, never changes a threshold and
never claims an attempt. It asks Windows to keep the display and system available, sends one
one-pixel relative mouse move and back on a bounded cadence so an idle screen saver or lock cannot
start, and samples the same fields `runtime_monitor` samples. It cannot unlock a workstation that is
already locked, so such a record fails instead of being reinterpreted as a clean interval.
"""
import argparse
import ctypes as ct
from ctypes import wintypes as w
import datetime as dt
import json
from pathlib import Path
import time

from .runtime_monitor import POLICY, WindowsSnapshot

ES_CONTINUOUS, ES_SYSTEM_REQUIRED, ES_DISPLAY_REQUIRED = 0x80000000, 0x1, 0x2
MOUSEEVENTF_MOVE = 0x0001


class MOUSEINPUT(ct.Structure):
    _fields_ = [("dx", w.LONG), ("dy", w.LONG), ("mouseData", w.DWORD), ("flags", w.DWORD),
                ("time", w.DWORD), ("extra", ct.c_size_t)]


class INPUT(ct.Structure):
    _fields_ = [("type", w.DWORD), ("mouse", MOUSEINPUT)]


def utc():
    return dt.datetime.now(dt.timezone.utc).isoformat()


def hold(on):
    kernel = ct.WinDLL("kernel32", use_last_error=True)
    kernel.SetThreadExecutionState.argtypes = [ct.c_uint]
    kernel.SetThreadExecutionState.restype = ct.c_uint
    flags = (ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED) if on else ES_CONTINUOUS
    return {"requested": flags, "previous_state": kernel.SetThreadExecutionState(flags) or 0}


def nudge():
    """One pixel right and back: resets the idle timer without clicking or switching a window."""
    user = ct.WinDLL("user32", use_last_error=True)
    user.SendInput.argtypes = [w.UINT, ct.POINTER(INPUT), ct.c_int]
    events = (INPUT * 2)(INPUT(0, MOUSEINPUT(1, 0, 0, MOUSEEVENTF_MOVE, 0, 0)),
                         INPUT(0, MOUSEINPUT(-1, 0, 0, MOUSEEVENTF_MOVE, 0, 0)))
    return user.SendInput(2, events, ct.sizeof(INPUT))


def nudge_due(elapsed, interval, sent):
    return interval > 0 and elapsed >= (sent + 1) * interval


def summary(samples, policy=POLICY):
    """Judge one sampling interval. Zero foreground is a failed remedy, not a clean workstation."""
    if not samples:
        raise ValueError("no environment samples")
    errors = set()
    if any(row["foreground_pid"] == 0 for row in samples):
        errors.add("no foreground window")
    if any(row["ac_line_status"] != 1 for row in samples):
        errors.add("AC line left")
    if any(row["conflicts"] for row in samples):
        errors.add("conflicting process present")
    if len({row["power_scheme_bytes"] for row in samples}) > 1:
        errors.add("active power scheme changed")
    if len({row["foreground_pid"] for row in samples}) > 1:
        errors.add("foreground process changed")
    maximum_gap = max((b["monotonic"] - a["monotonic"]
                       for a, b in zip(samples, samples[1:])), default=0.0)
    if len(samples) < 2 or maximum_gap > policy["maximum_gap_seconds"]:
        errors.add("incomplete environment sampling interval")
    return {"distinct_foreground_pids": sorted({row["foreground_pid"] for row in samples}),
            "distinct_power_schemes": sorted({row["power_scheme_bytes"] for row in samples}),
            "maximum_gap_seconds": maximum_gap, "errors": sorted(errors),
            "passed": not errors}


def verify(seconds, interval, output, nudge_seconds=240):
    if not 60 <= seconds <= 21600:
        raise ValueError("remedy verification is bounded to 60..21600 seconds")
    if nudge_seconds < 0:
        raise ValueError("nudge cadence cannot be negative")
    if output.exists():
        raise ValueError("fresh verification output required")
    snapshot = WindowsSnapshot()
    output.parent.mkdir(parents=True, exist_ok=True)
    samples, nudges = [], []
    started = time.monotonic()
    record = {"schema": "rpa.execution.environment-stability.v1", "policy": POLICY,
              "requested_seconds": seconds, "interval_seconds": interval,
              "nudge_seconds": nudge_seconds, "started_utc": utc(), "samples": samples,
              "nudges": nudges,
              "limitations": ["one pixel relative move and back on the declared cadence only; no "
                              "click, key or window switch is sent",
                              "a manual lock or an already locked workstation is not unlocked and "
                              "fails this record",
                              "sampling is one probe per interval and can miss sub-interval changes"]}
    record["execution_state"] = hold(True)
    try:
        while time.monotonic() - started < seconds:
            elapsed = time.monotonic() - started
            if nudge_due(elapsed, nudge_seconds, len(nudges)):
                sent = nudge()
                samples_before = len(samples)
                nudges.append({"elapsed_seconds": elapsed, "send_input_events": sent,
                               "samples_before": samples_before})
            samples.append(snapshot())
            time.sleep(interval)
    finally:
        record["released"] = hold(False)
        record["ended_utc"] = utc()
        record["duration_seconds"] = time.monotonic() - started
        record.update(summary(samples))
        record["passed"] = record["passed"] and all(n["send_input_events"] == 2 for n in nudges)
        with output.open("x", encoding="utf-8", newline="\n") as stream:
            stream.write(json.dumps(record, ensure_ascii=False, indent=2) + "\n")
    print(("PASSED " if record["passed"] else "FAILED ") + str(output.resolve()), flush=True)
    return record["passed"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seconds", type=int, default=900)
    parser.add_argument("--interval", type=float, default=1.0)
    parser.add_argument("--nudge-seconds", type=int, default=240)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.interval <= 0:
        parser.error("positive sampling interval required")
    raise SystemExit(0 if verify(args.seconds, args.interval, args.output,
                                 args.nudge_seconds) else 1)


if __name__ == "__main__":
    main()

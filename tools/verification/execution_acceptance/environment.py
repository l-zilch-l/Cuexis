"""Read-only runtime environment admission shared with the original CPU topology policy."""
from pathlib import Path
import subprocess
import sys

from tools.verification.playback_performance import measurement


def original_support():
    directory = str(Path(__file__).resolve().parents[3] / "tests/pd")
    if directory not in sys.path:
        sys.path.insert(0, directory)
    import formal_baseline
    return formal_baseline


def capture_cpu():
    support = original_support()
    # Builds and tests cannot share a formal sampling interval. Scan before asking for the tracing
    # state: `no_trace()` starts `wpr -status`, and a still-exiting child would flag itself.
    command = ["powershell", "-NoProfile", "-Command",
               "Get-Process -ErrorAction SilentlyContinue | "
               "Where-Object { $_.ProcessName -in @('cl','ninja','ctest','clang','clang-tidy','wpr') } | "
               "Select-Object -ExpandProperty ProcessName"]
    found = subprocess.check_output(command, timeout=30).decode("utf-8", errors="replace").strip()
    if found:
        raise ValueError("conflicting build, test or tracing process: " + found)
    value = support.environment()
    value["topology"] = support.topology(value["machine"])
    value["no_trace"] = support.no_trace()
    return value


def verify_cpu(expected):
    support = original_support()
    actual = capture_cpu()
    support.same_environment(expected, actual)
    if expected["topology"] != actual["topology"]:
        raise ValueError("physical-core topology changed")
    if expected.get("foreground_policy") != "fixed-existing-foreground":
        raise ValueError("CPU/headless foreground policy must be explicit")
    if expected.get("foreground_pid") != actual["foreground_pid"]:
        raise ValueError("foreground process changed")
    return actual


def verify_native(text, affinity, expected=None):
    rows = original_support().native_environment(text.splitlines())
    actual = rows[0]
    if actual.get("process_affinity") != affinity:
        raise ValueError("native process affinity differs from registration")
    fields = ("process_priority", "thread_priority", "process_power_error",
              "process_power_control", "process_power_state", "thread_power_error",
              "thread_power_control", "thread_power_state", "process_affinity_error",
              "processor_group_error")
    controlled = {field: actual[field] for field in fields}
    if expected is not None and controlled != expected:
        raise ValueError("native scheduling/power settings changed across processes")
    return controlled


def verify_warmup(text):
    rows = measurement.records(text, "PD_MACHINE_WARMUP ")
    if len(rows) != 1 or rows[0].get("seconds") != 30:
        raise ValueError("missing completed 30 second machine warmup")

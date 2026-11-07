"""External observer for a windowed Cuexis process.

The registered startup path creates its window only after a synchronous prepare, so everything that
happens before the window exists is invisible from inside the process and unmeasurable from a log
flushed at exit. This watcher observes the process from the outside instead:

  * every top level window the process owns, and when each one appears, becomes visible, is reported
    as hung by Windows, or disappears,
  * the `primary` window (largest visible one) and its title, class, size and DWM cloak state,
  * how long `WM_NULL` round trips take against the primary window (message pump latency),
  * cumulative kernel and user CPU time, sampled while the process runs,
  * how long a `WM_CLOSE` takes to terminate the process.

`WM_NULL` is the standard responsiveness probe: it reaches the window procedure only while the
target pumps its message queue, so a piggybacked `SendMessageTimeout` measures how long an input
message would have waited to be looked at. Before the primary window exists there is nothing to
deliver to, which is itself the honest answer for the pre-window phase.

All timestamps are `perf_counter` seconds relative to the moment just before `subprocess.Popen`
returns; that call itself is a few milliseconds of process wall clock and is not part of any
reported interval.
"""

from __future__ import annotations

import argparse
import ctypes
import ctypes.wintypes as wintypes
import hashlib
import json
import os
import pathlib
import statistics
import subprocess
import sys
import threading
import time

# --- Win32 plumbing -------------------------------------------------------------------------------

user32 = ctypes.WinDLL("user32", use_last_error=True)
kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
dwmapi = ctypes.WinDLL("dwmapi", use_last_error=True)

EnumWindowsProc = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)

user32.EnumWindows.argtypes = [EnumWindowsProc, wintypes.LPARAM]
user32.EnumWindows.restype = wintypes.BOOL
user32.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
user32.GetWindowThreadProcessId.restype = wintypes.DWORD
user32.IsWindowVisible.argtypes = [wintypes.HWND]
user32.IsWindowVisible.restype = wintypes.BOOL
user32.IsWindow.argtypes = [wintypes.HWND]
user32.IsWindow.restype = wintypes.BOOL
user32.IsHungAppWindow.argtypes = [wintypes.HWND]
user32.IsHungAppWindow.restype = wintypes.BOOL
user32.GetWindowTextW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
user32.GetWindowTextW.restype = ctypes.c_int
user32.GetClassNameW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
user32.GetClassNameW.restype = ctypes.c_int
user32.GetWindowRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
user32.GetWindowRect.restype = wintypes.BOOL
user32.GetClientRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
user32.GetClientRect.restype = wintypes.BOOL
user32.SendMessageTimeoutW.argtypes = [
    wintypes.HWND,
    wintypes.UINT,
    wintypes.WPARAM,
    wintypes.LPARAM,
    wintypes.UINT,
    wintypes.UINT,
    ctypes.POINTER(ctypes.c_size_t),
]
user32.SendMessageTimeoutW.restype = wintypes.LPARAM
user32.PostMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
user32.PostMessageW.restype = wintypes.BOOL

kernel32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
kernel32.OpenProcess.restype = wintypes.HANDLE
kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
kernel32.GetProcessTimes.argtypes = [
    wintypes.HANDLE,
    ctypes.POINTER(wintypes.FILETIME),
    ctypes.POINTER(wintypes.FILETIME),
    ctypes.POINTER(wintypes.FILETIME),
    ctypes.POINTER(wintypes.FILETIME),
]
kernel32.GetProcessTimes.restype = wintypes.BOOL
# Windows reports virtualized client rectangles to a DPI-unaware observer; a 1280x720 window on a
# 125% display would otherwise be reported as 1024x576. Per monitor awareness makes the recorded
# size the real pixel size.
try:
    user32.SetProcessDpiAwarenessContext(ctypes.c_void_p(-4))
except (AttributeError, OSError):  # pragma: no cover - older Windows
    pass

WM_NULL = 0x0000
WM_CLOSE = 0x0010
SMTO_BLOCK = 0x0001
SMTO_ABORTIFHUNG = 0x0002
PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
DWMWA_CLOAKED = 14


def filetime_seconds(value) -> float:
    return (value.dwHighDateTime << 32 | value.dwLowDateTime) / 1.0e7


def process_cpu(handle) -> tuple[float, float] | None:
    created, exited, kernel, user = (wintypes.FILETIME() for _ in range(4))
    if not kernel32.GetProcessTimes(handle, created, exited, kernel, user):
        return None
    return filetime_seconds(kernel), filetime_seconds(user)


def cloaked(hwnd) -> int | None:
    value = wintypes.DWORD(0)
    result = dwmapi.DwmGetWindowAttribute(
        wintypes.HWND(hwnd), ctypes.c_uint(DWMWA_CLOAKED), ctypes.byref(value), ctypes.sizeof(value)
    )
    return int(value.value) if result == 0 else None


def window_text(hwnd) -> str:
    buffer = ctypes.create_unicode_buffer(512)
    user32.GetWindowTextW(hwnd, buffer, 512)
    return buffer.value


def window_class(hwnd) -> str:
    buffer = ctypes.create_unicode_buffer(256)
    user32.GetClassNameW(hwnd, buffer, 256)
    return buffer.value


def window_rect(hwnd) -> list[int] | None:
    rect = wintypes.RECT()
    if not user32.GetWindowRect(hwnd, ctypes.byref(rect)):
        return None
    return [rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top]


def client_size(hwnd) -> list[int] | None:
    rect = wintypes.RECT()
    if not user32.GetClientRect(hwnd, ctypes.byref(rect)):
        return None
    return [rect.right - rect.left, rect.bottom - rect.top]


def windows_of(pid: int) -> list[int]:
    found: list[int] = []

    def callback(hwnd, _lparam):
        owner = wintypes.DWORD(0)
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
        if owner.value == pid:
            found.append(int(hwnd))
        return True

    user32.EnumWindows(EnumWindowsProc(callback), 0)
    return found


def describe(hwnd) -> dict:
    rect = window_rect(hwnd)
    client = client_size(hwnd)
    return {
        "hwnd": hwnd,
        "title": window_text(hwnd),
        "class": window_class(hwnd),
        "visible": bool(user32.IsWindowVisible(hwnd)),
        "hung": bool(user32.IsHungAppWindow(hwnd)),
        "rect": rect,
        "client": client,
        "area": (client[0] * client[1]) if client else 0,
        "cloaked": cloaked(hwnd),
    }


# --- run ------------------------------------------------------------------------------------------


def run(args) -> dict:
    exe = pathlib.Path(args.command[0]).resolve()
    events: list[dict] = []
    probes: list[dict] = []
    cpu_samples: list[dict] = []
    lock = threading.Lock()

    env = dict(os.environ)
    for item in args.env:
        key, _, value = item.partition("=")
        env[key] = value

    out_dir = pathlib.Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)
    out_path = out_dir / f"{args.label}.out.txt"
    err_path = out_dir / f"{args.label}.err.txt"

    with open(out_path, "wb") as out_file, open(err_path, "wb") as err_file:
        started_wall = time.time()
        t0 = time.perf_counter()
        process = subprocess.Popen(
            [str(exe), *args.command[1:]],
            stdout=out_file,
            stderr=err_file,
            stdin=subprocess.DEVNULL,
            cwd=args.cwd,
            env=env,
        )
        pid = process.pid
        handle = kernel32.OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, False, pid)

        state: dict = {"primary": None, "stop": False}
        events.append({"t": time.perf_counter() - t0, "kind": "spawn", "pid": pid})

        def probe_loop():
            while True:
                with lock:
                    if state["stop"]:
                        return
                    hwnd = state["primary"]
                if not hwnd or not user32.IsWindow(hwnd):
                    time.sleep(0.002)
                    continue
                sent = time.perf_counter()
                result = ctypes.c_size_t(0)
                ok = user32.SendMessageTimeoutW(
                    hwnd,
                    WM_NULL,
                    0,
                    0,
                    SMTO_BLOCK | SMTO_ABORTIFHUNG,
                    int(args.probe_timeout_ms),
                    ctypes.byref(result),
                )
                elapsed = time.perf_counter() - sent
                with lock:
                    probes.append(
                        {
                            "t": elapsed + sent - t0,
                            "latency_ms": round(elapsed * 1000.0, 3),
                            "ok": bool(ok),
                            "hwnd": hwnd,
                        }
                    )
                time.sleep(args.probe_interval_ms / 1000.0)

        probe_thread = threading.Thread(target=probe_loop, daemon=True)
        probe_thread.start()

        known: dict[int, dict] = {}
        visible_since: dict[int, float] = {}
        primary: int | None = None
        next_cpu = 0.0
        next_enum = 0.0
        close_posted_at: float | None = None
        exit_time: float | None = None
        killed = False
        final_client: list[int] | None = None
        while True:
            now = time.perf_counter() - t0
            alive = process.poll() is None
            if alive and now >= next_enum:
                seen = windows_of(pid)
                for hwnd in seen:
                    info = describe(hwnd)
                    previous = known.get(hwnd)
                    if previous is None:
                        known[hwnd] = info
                        events.append({"t": now, "kind": "window_appeared", **info})
                        if info["visible"]:
                            visible_since[hwnd] = now
                            events.append({"t": now, "kind": "window_visible", **info})
                        continue
                    if info["visible"] and not previous["visible"]:
                        visible_since[hwnd] = now
                        events.append({"t": now, "kind": "window_visible", **info})
                    if info["hung"] and not previous["hung"]:
                        events.append({"t": now, "kind": "window_hung", **info})
                    if info["client"] != previous["client"]:
                        events.append(
                            {
                                "t": now,
                                "kind": "window_resized",
                                "hwnd": hwnd,
                                "from": previous["client"],
                                "to": info["client"],
                            }
                        )
                    known[hwnd] = info
                for hwnd in list(known):
                    if hwnd not in seen:
                        events.append({"t": now, "kind": "window_gone", "hwnd": hwnd})
                        known.pop(hwnd, None)
                        visible_since.pop(hwnd, None)
                # Only a visible window with a real client area can represent the application
                # window; SDL also owns small hidden helper windows, and during shutdown every
                # window loses its client rectangle. Keeping the previous primary in that case
                # avoids retargeting the summary and the probes at a dying handle.
                candidates = [i for i in known.values() if i["visible"] and i["area"] > 0]
                best = max(candidates, key=lambda info: info["area"])["hwnd"] if candidates else primary
                if best != primary:
                    primary = best
                    with lock:
                        state["primary"] = best
                    if best is not None:
                        events.append({"t": now, "kind": "primary_set", **known[best]})
                next_enum = now + args.enum_interval_ms / 1000.0
            if primary is not None and primary in known:
                final_client = known[primary]["client"]
            if now >= next_cpu and handle:
                times = process_cpu(handle)
                if times:
                    cpu_samples.append(
                        {
                            "t": round(now, 4),
                            "kernel_s": round(times[0], 4),
                            "user_s": round(times[1], 4),
                            "primary": primary,
                        }
                    )
                next_cpu = now + args.cpu_interval_ms / 1000.0
            if args.close_after > 0 and now >= args.close_after and close_posted_at is None:
                target = primary
                posted = bool(user32.PostMessageW(target, WM_CLOSE, 0, 0)) if target else False
                close_posted_at = now
                events.append(
                    {
                        "t": now,
                        "kind": "wm_close_posted",
                        "hwnd": target,
                        "posted": posted,
                        "primary_existed": target is not None,
                    }
                )
            if not alive:
                exit_time = now
                break
            if now >= args.timeout:
                killed = True
                process.kill()
                exit_time = time.perf_counter() - t0
                events.append({"t": exit_time, "kind": "killed_by_watcher"})
                break
            time.sleep(args.poll_ms / 1000.0)

        with lock:
            state["stop"] = True
        probe_thread.join(timeout=2.0)
        return_code = process.wait(timeout=30)
        if handle:
            kernel32.CloseHandle(handle)

    def first(kind, hwnd=None):
        return next(
            (e for e in events if e["kind"] == kind and (hwnd is None or e["hwnd"] == hwnd)), None
        )

    primary_events = [e for e in events if e["kind"] == "primary_set"]
    primary_info = primary_events[0] if primary_events else None
    target_hwnd = primary_info["hwnd"] if primary_info else None
    appeared = first("window_appeared", target_hwnd)
    shown = first("window_visible", target_hwnd)
    latch = primary_info["t"] if primary_info else 0.0
    after = [p for p in probes if p["t"] >= latch]
    latencies = [p["latency_ms"] for p in after]
    windows = []
    for hwnd in sorted({e["hwnd"] for e in events if e["kind"] == "window_appeared"}):
        first_seen = next(e for e in events if e["kind"] == "window_appeared" and e["hwnd"] == hwnd)
        first_visible = first("window_visible", hwnd)
        last = next(
            (e for e in reversed(events) if e.get("hwnd") == hwnd and "client" in e), first_seen
        )
        windows.append(
            {
                "hwnd": hwnd,
                "title": first_seen["title"],
                "class": first_seen["class"],
                "first_seen_t_s": round(first_seen["t"], 4),
                "first_visible_t_s": round(first_visible["t"], 4) if first_visible else None,
                "first_client": first_seen["client"],
                "last_client": last.get("client"),
            }
        )
    cpu_at = {}
    for target in (0.5, 1.0, 2.0, 2.4, 3.0):
        sample = next((s for s in cpu_samples if s["t"] >= target), None)
        if sample:
            cpu_at[f"{target:g}s"] = round((sample["kernel_s"] + sample["user_s"]) / sample["t"], 4)
    resizes = [e for e in events if e["kind"] == "window_resized" and e["hwnd"] == target_hwnd]
    summary = {
        "label": args.label,
        "return_code": return_code,
        "killed": killed,
        "exit_t_s": round(exit_time, 4) if exit_time is not None else None,
        "windows_seen": sorted({e["hwnd"] for e in events if e["kind"] == "window_appeared"}),
        "windows": windows,
        "first_any_window_t_s": round(
            next((e["t"] for e in events if e["kind"] == "window_appeared"), 0.0), 4
        ),
        "primary_hwnd": target_hwnd,
        "primary_set_t_s": round(primary_info["t"], 4) if primary_info else None,
        "primary_title": primary_info["title"] if primary_info else None,
        "primary_class": primary_info["class"] if primary_info else None,
        "primary_first_appeared_t_s": round(appeared["t"], 4) if appeared else None,
        "primary_visible_t_s": round(shown["t"], 4) if shown else None,
        "primary_final_client": final_client,
        "primary_client_timeline": [{"t": round(e["t"], 4), "to": e["to"]} for e in resizes],
        "primary_cloaked": primary_info["cloaked"] if primary_info else None,
        "hung_events": [
            {"t": round(e["t"], 4), "hwnd": e["hwnd"]} for e in events if e["kind"] == "window_hung"
        ],
        "probes_total": len(probes),
        "probes_after_primary": len(after),
        "probes_timed_out": sum(1 for p in after if not p["ok"]),
        "probe_timeout_budget_ms": args.probe_timeout_ms,
        "probe_latency_ms": {
            "p50": round(statistics.median(latencies), 3) if latencies else None,
            "p90": round(sorted(latencies)[int(0.9 * (len(latencies) - 1))], 3) if latencies else None,
            "p99": round(sorted(latencies)[int(0.99 * (len(latencies) - 1))], 3) if latencies else None,
            "max": round(max(latencies), 3) if latencies else None,
        },
        "probe_latency_over_100ms": sum(1 for value in latencies if value > 100.0),
        "probe_latency_over_100ms_times": [round(p["t"], 3) for p in after if p["latency_ms"] > 100.0],
        "probe_latency_window": [round(after[0]["t"], 3), round(after[-1]["t"], 3)] if after else None,
        "cpu_core_ratio": cpu_at,
        "wm_close_posted_t_s": round(close_posted_at, 4) if close_posted_at is not None else None,
        "wm_close_to_exit_s": round(exit_time - close_posted_at, 4)
        if (exit_time is not None and close_posted_at is not None)
        else None,
        "command": [str(exe), *args.command[1:]],
        "exe_sha256": hashlib.sha256(exe.read_bytes()).hexdigest(),
        "exe_bytes": exe.stat().st_size,
        "started_wall": started_wall,
        "cwd": str(pathlib.Path(args.cwd).resolve()) if args.cwd else os.getcwd(),
        "env_overrides": sorted(args.env),
        "stdout_bytes": out_path.stat().st_size,
        "stderr_bytes": err_path.stat().st_size,
    }
    return {
        "summary": summary,
        "events": events,
        "cpu_samples": cpu_samples,
        "probes": probes,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--label", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--cwd", default=None)
    parser.add_argument("--env", action="append", default=[])
    parser.add_argument("--timeout", type=float, default=30.0)
    parser.add_argument("--close-after", type=float, default=0.0)
    parser.add_argument("--poll-ms", type=float, default=2.0)
    parser.add_argument("--enum-interval-ms", type=float, default=5.0)
    parser.add_argument("--probe-interval-ms", type=float, default=25.0)
    parser.add_argument("--probe-timeout-ms", type=float, default=400.0)
    parser.add_argument("--cpu-interval-ms", type=float, default=100.0)
    parser.add_argument("--json", default=None)
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if args.command and args.command[0] == "--":
        args.command = args.command[1:]
    if not args.command:
        parser.error("missing command")
    result = run(args)
    text = json.dumps(result, indent=1, ensure_ascii=True)
    if args.json:
        path = pathlib.Path(args.json)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text + "\n", encoding="utf-8")
    print(json.dumps(result["summary"], indent=1, ensure_ascii=True))
    return 0


if __name__ == "__main__":
    sys.exit(main())

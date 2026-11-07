"""Bounded Windows polling during a child interval; no tracing or per-frame instrumentation."""
import ctypes as ct
from ctypes import wintypes as w
import os
import threading
import time


POLICY = {"id": "sampled-windows-environment-v1", "interval_seconds": 1.0,
          "maximum_gap_seconds": 2.0, "coverage": "sampled; sub-interval activity may be missed"}
CONFLICTS = {"cl.exe", "link.exe", "lib.exe", "ninja.exe", "cmake.exe", "ctest.exe",
             "clang.exe", "clang++.exe", "clang-tidy.exe", "gcc.exe", "g++.exe", "cc1plus.exe",
             "make.exe", "mingw32-make.exe", "wpr.exe", "perfview.exe"}


class WindowsSnapshot:
    def __init__(self):
        if os.name != "nt":
            raise RuntimeError("runtime environment monitor requires Windows")

        class ProcessEntry(ct.Structure):
            _fields_ = [("size", w.DWORD), ("usage", w.DWORD), ("pid", w.DWORD),
                        ("heap", ct.c_size_t), ("module", w.DWORD), ("threads", w.DWORD),
                        ("parent", w.DWORD), ("priority", w.LONG), ("flags", w.DWORD),
                        ("name", w.WCHAR * 260)]

        class PowerStatus(ct.Structure):
            _fields_ = [("ac", w.BYTE), ("battery", w.BYTE), ("percent", w.BYTE),
                        ("flag", w.BYTE), ("remaining", w.DWORD), ("full", w.DWORD)]

        self.ProcessEntry, self.PowerStatus = ProcessEntry, PowerStatus
        self.kernel = ct.WinDLL("kernel32", use_last_error=True)
        self.user = ct.WinDLL("user32", use_last_error=True)
        self.power = ct.WinDLL("powrprof", use_last_error=True)
        self.kernel.CreateToolhelp32Snapshot.argtypes = [w.DWORD, w.DWORD]
        self.kernel.CreateToolhelp32Snapshot.restype = w.HANDLE
        self.kernel.Process32FirstW.argtypes = [w.HANDLE, ct.POINTER(ProcessEntry)]
        self.kernel.Process32NextW.argtypes = [w.HANDLE, ct.POINTER(ProcessEntry)]
        self.kernel.CloseHandle.argtypes = [w.HANDLE]
        self.kernel.LocalFree.argtypes = [ct.c_void_p]
        self.kernel.GetSystemPowerStatus.argtypes = [ct.POINTER(PowerStatus)]
        self.user.GetForegroundWindow.restype = w.HWND
        self.user.GetWindowThreadProcessId.argtypes = [w.HWND, ct.POINTER(w.DWORD)]
        self.power.PowerGetActiveScheme.argtypes = [w.HANDLE, ct.POINTER(ct.c_void_p)]

    def __call__(self):
        started = time.monotonic()
        snapshot = self.kernel.CreateToolhelp32Snapshot(2, 0)
        if snapshot == ct.c_void_p(-1).value:
            raise ct.WinError(ct.get_last_error())
        conflicts = []
        entry = self.ProcessEntry()
        entry.size = ct.sizeof(entry)
        try:
            if not self.kernel.Process32FirstW(snapshot, ct.byref(entry)):
                raise ct.WinError(ct.get_last_error())
            while True:
                name = entry.name.lower()
                if name in CONFLICTS or (name.startswith("cuexis_") and name.endswith("_tests.exe")):
                    conflicts.append({"pid": entry.pid, "name": name})
                if not self.kernel.Process32NextW(snapshot, ct.byref(entry)):
                    if ct.get_last_error() != 18:  # ERROR_NO_MORE_FILES
                        raise ct.WinError(ct.get_last_error())
                    break
        finally:
            self.kernel.CloseHandle(snapshot)
        foreground, status, guid = w.DWORD(), self.PowerStatus(), ct.c_void_p()
        self.user.GetWindowThreadProcessId(self.user.GetForegroundWindow(), ct.byref(foreground))
        if not self.kernel.GetSystemPowerStatus(ct.byref(status)):
            raise ct.WinError(ct.get_last_error())
        error = self.power.PowerGetActiveScheme(None, ct.byref(guid))
        if error:
            raise OSError(error, "PowerGetActiveScheme failed")
        try:
            plan = ct.string_at(guid, 16).hex()
        finally:
            self.kernel.LocalFree(guid)
        return {"monotonic": started, "duration_seconds": time.monotonic() - started,
                "conflicts": conflicts, "foreground_pid": foreground.value,
                "ac_line_status": status.ac, "power_scheme_bytes": plan}


class RuntimeMonitor:
    def __init__(self, foreground_pid, *, timeout, snapshot=None):
        self.foreground_pid, self.timeout = foreground_pid, timeout
        self.snapshot = snapshot or WindowsSnapshot()
        self.samples, self.errors = [], []
        self.cpu_seconds = 0
        self.stop = threading.Event()

    def sample(self):
        try:
            row = self.snapshot()
            self.samples.append(row)
            if row["conflicts"] or row["ac_line_status"] != 1 or row["foreground_pid"] != self.foreground_pid:
                self.errors.append("conflicting process, AC or foreground change")
            if row["power_scheme_bytes"] != self.samples[0]["power_scheme_bytes"]:
                self.errors.append("active power scheme changed")
        except Exception as error:
            self.errors.append(str(error))

    def __enter__(self):
        self.started = time.monotonic()
        self.cpu_started = time.process_time()
        self.sample()
        def poll():
            while not self.stop.wait(POLICY["interval_seconds"]):
                self.sample()
                if time.monotonic() - self.started > self.timeout:
                    self.errors.append("monitor deadline exceeded")
                    return
        self.thread = threading.Thread(target=poll, daemon=True)
        self.thread.start()
        return self

    def __exit__(self, *_):
        self.stop.set()
        self.thread.join(timeout=5)
        if self.thread.is_alive():
            self.errors.append("monitor did not stop")
        else:
            self.sample()
        self.cpu_seconds = time.process_time() - self.cpu_started

    def result(self):
        times = [row["monotonic"] for row in self.samples]
        maximum_gap = max((end - start for start, end in zip(times, times[1:])), default=0)
        errors = list(self.errors)
        if len(times) < 2 or maximum_gap > POLICY["maximum_gap_seconds"]:
            errors.append("incomplete environment sampling interval")
        return {"schema": "rpa.execution.runtime-environment.v1", "policy": POLICY,
                "passed": not errors, "errors": errors, "samples": self.samples,
                "maximum_gap_seconds": maximum_gap, "parent_process_cpu_seconds": self.cpu_seconds,
                "monitor_scope": "process enumeration, foreground, AC, power scheme; no GPU/audio claim"}

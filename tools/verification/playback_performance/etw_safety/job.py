"""Owned Windows Job and process identity helpers for private PD-5 tools.

The worker gate is started outside the Job's containment guarantee, but it blocks on
stdin and cannot spawn a command until Job.spawn has assigned it and verified membership.
"""
from __future__ import annotations

import ctypes as ct
from ctypes import wintypes as w
import json
import math
import os
from pathlib import Path
import subprocess
import sys
import time
from collections.abc import Sequence
from typing import BinaryIO


ERROR_ALREADY_EXISTS = 183
ERROR_ACCESS_DENIED = 5
ERROR_FILE_NOT_FOUND = 2
ERROR_INVALID_PARAMETER = 87
ERROR_NOT_FOUND = 1168
WAIT_OBJECT_0 = 0
WAIT_TIMEOUT = 0x00000102
WAIT_FAILED = 0xFFFFFFFF

PROCESS_TERMINATE = 0x0001
PROCESS_SET_QUOTA = 0x0100
PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
SYNCHRONIZE = 0x00100000

JOB_OBJECT_ASSIGN_PROCESS = 0x0001
JOB_OBJECT_QUERY = 0x0004
JOB_OBJECT_TERMINATE = 0x0008
JOB_OBJECT_SET_ATTRIBUTES = 0x0010
JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE = 0x00002000
JOB_OBJECT_EXTENDED_LIMIT_INFORMATION = 9
JOB_OBJECT_BASIC_ACCOUNTING_INFORMATION = 1

_MAX_WAIT_MS = 0xFFFFFFFE
_WAIT_SLICE_SECONDS = 0.025
_KERNEL = None
# OpenProcess reports ERROR_INVALID_PARAMETER for a PID that no longer exists, while
# other lookups report ERROR_FILE_NOT_FOUND or ERROR_NOT_FOUND. These three mean "this
# PID does not name a process now"; anything else (for example ERROR_ACCESS_DENIED for a
# protected process) means the identity exists but was refused and must not read as death.
GONE_WINERRORS = frozenset({ERROR_FILE_NOT_FOUND, ERROR_INVALID_PARAMETER, ERROR_NOT_FOUND})


class _BasicLimitInformation(ct.Structure):
    _fields_ = [
        ("PerProcessUserTimeLimit", ct.c_int64),
        ("PerJobUserTimeLimit", ct.c_int64),
        ("LimitFlags", w.DWORD),
        ("MinimumWorkingSetSize", ct.c_size_t),
        ("MaximumWorkingSetSize", ct.c_size_t),
        ("ActiveProcessLimit", w.DWORD),
        ("Affinity", ct.c_size_t),
        ("PriorityClass", w.DWORD),
        ("SchedulingClass", w.DWORD),
    ]


class _IoCounters(ct.Structure):
    _fields_ = [
        ("ReadOperationCount", ct.c_uint64),
        ("WriteOperationCount", ct.c_uint64),
        ("OtherOperationCount", ct.c_uint64),
        ("ReadTransferCount", ct.c_uint64),
        ("WriteTransferCount", ct.c_uint64),
        ("OtherTransferCount", ct.c_uint64),
    ]


class _ExtendedLimitInformation(ct.Structure):
    _fields_ = [
        ("BasicLimitInformation", _BasicLimitInformation),
        ("IoInfo", _IoCounters),
        ("ProcessMemoryLimit", ct.c_size_t),
        ("JobMemoryLimit", ct.c_size_t),
        ("PeakProcessMemoryUsed", ct.c_size_t),
        ("PeakJobMemoryUsed", ct.c_size_t),
    ]


class _BasicAccountingInformation(ct.Structure):
    _fields_ = [
        ("TotalUserTime", ct.c_int64),
        ("TotalKernelTime", ct.c_int64),
        ("ThisPeriodTotalUserTime", ct.c_int64),
        ("ThisPeriodTotalKernelTime", ct.c_int64),
        ("TotalPageFaultCount", w.DWORD),
        ("TotalProcesses", w.DWORD),
        ("ActiveProcesses", w.DWORD),
        ("TotalTerminatedProcesses", w.DWORD),
    ]


class _FileTime(ct.Structure):
    _fields_ = [("dwLowDateTime", w.DWORD), ("dwHighDateTime", w.DWORD)]


def _require_windows_x64() -> None:
    if os.name != "nt" or ct.sizeof(ct.c_void_p) != 8:
        raise OSError("ETW safety process primitives require 64-bit Windows")


def _kernel():
    global _KERNEL
    _require_windows_x64()
    if _KERNEL is None:
        dll = ct.WinDLL("kernel32", use_last_error=True)
        dll.CloseHandle.argtypes = [w.HANDLE]
        dll.CloseHandle.restype = w.BOOL
        dll.OpenProcess.argtypes = [w.DWORD, w.BOOL, w.DWORD]
        dll.OpenProcess.restype = w.HANDLE
        dll.GetProcessTimes.argtypes = [
            w.HANDLE,
            ct.POINTER(_FileTime),
            ct.POINTER(_FileTime),
            ct.POINTER(_FileTime),
            ct.POINTER(_FileTime),
        ]
        dll.GetProcessTimes.restype = w.BOOL
        dll.GetExitCodeProcess.argtypes = [w.HANDLE, ct.POINTER(w.DWORD)]
        dll.GetExitCodeProcess.restype = w.BOOL
        dll.WaitForSingleObject.argtypes = [w.HANDLE, w.DWORD]
        dll.WaitForSingleObject.restype = w.DWORD
        dll.TerminateProcess.argtypes = [w.HANDLE, w.UINT]
        dll.TerminateProcess.restype = w.BOOL
        dll.CreateJobObjectW.argtypes = [ct.c_void_p, w.LPCWSTR]
        dll.CreateJobObjectW.restype = w.HANDLE
        dll.OpenJobObjectW.argtypes = [w.DWORD, w.BOOL, w.LPCWSTR]
        dll.OpenJobObjectW.restype = w.HANDLE
        dll.SetInformationJobObject.argtypes = [w.HANDLE, ct.c_int, ct.c_void_p, w.DWORD]
        dll.SetInformationJobObject.restype = w.BOOL
        dll.QueryInformationJobObject.argtypes = [
            w.HANDLE, ct.c_int, ct.c_void_p, w.DWORD, ct.POINTER(w.DWORD)
        ]
        dll.QueryInformationJobObject.restype = w.BOOL
        dll.TerminateJobObject.argtypes = [w.HANDLE, w.UINT]
        dll.TerminateJobObject.restype = w.BOOL
        dll.AssignProcessToJobObject.argtypes = [w.HANDLE, w.HANDLE]
        dll.AssignProcessToJobObject.restype = w.BOOL
        dll.IsProcessInJob.argtypes = [w.HANDLE, w.HANDLE, ct.POINTER(w.BOOL)]
        dll.IsProcessInJob.restype = w.BOOL
        _KERNEL = dll
    return _KERNEL


def _raise_last_error(operation: str) -> None:
    error = ct.get_last_error()
    if error:
        raise ct.WinError(error, operation)
    raise OSError(f"{operation} failed without a Win32 error code")


def _close_handle(handle) -> None:
    if handle and not _kernel().CloseHandle(handle):
        _raise_last_error("CloseHandle")


def _filetime_value(value: _FileTime) -> int:
    return (int(value.dwHighDateTime) << 32) | int(value.dwLowDateTime)


def _creation_time(handle) -> int:
    created, exited, kernel_time, user_time = _FileTime(), _FileTime(), _FileTime(), _FileTime()
    if not _kernel().GetProcessTimes(
        handle, ct.byref(created), ct.byref(exited), ct.byref(kernel_time), ct.byref(user_time)
    ):
        _raise_last_error("GetProcessTimes")
    return _filetime_value(created)


def _open_process(pid: int, access: int):
    if not isinstance(pid, int) or isinstance(pid, bool) or pid <= 0 or pid > 0xFFFFFFFF:
        raise ValueError("pid must be a positive DWORD")
    handle = _kernel().OpenProcess(access, False, pid)
    if not handle:
        _raise_last_error("OpenProcess")
    return handle


def _deadline_wait(handle, deadline: float) -> None:
    if not isinstance(deadline, (int, float)) or not math.isfinite(deadline):
        raise ValueError("deadline must be a finite monotonic timestamp")
    kernel = _kernel()
    while True:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            result = kernel.WaitForSingleObject(handle, 0)
            if result == WAIT_OBJECT_0:
                return
            if result == WAIT_FAILED:
                _raise_last_error("WaitForSingleObject")
            raise TimeoutError("process did not exit before the deadline")
        timeout_ms = min(_MAX_WAIT_MS, max(1, math.ceil(remaining * 1000)))
        result = kernel.WaitForSingleObject(handle, timeout_ms)
        if result == WAIT_OBJECT_0:
            return
        if result == WAIT_FAILED:
            _raise_last_error("WaitForSingleObject")
        if result != WAIT_TIMEOUT:
            raise OSError(f"unexpected WaitForSingleObject result: {result}")


class ProcessRef:
    """A PID sealed to its creation time and held through a stable process handle."""

    def __init__(self, pid: int, creation_time: int):
        if not isinstance(creation_time, int) or isinstance(creation_time, bool) or not 0 <= creation_time <= 0xFFFFFFFFFFFFFFFF:
            raise ValueError("creation_time must be an unsigned FILETIME value")
        handle = _open_process(
            pid, PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE | SYNCHRONIZE
        )
        try:
            actual = _creation_time(handle)
            if actual != creation_time:
                raise ProcessLookupError(
                    f"PID {pid} creation identity changed: expected {creation_time}, got {actual}"
                )
        except BaseException:
            _close_handle(handle)
            raise
        self.pid = pid
        self.creation_time = int(creation_time)
        self._handle = handle

    @classmethod
    def from_pid(cls, pid: int) -> ProcessRef:
        """Open a PID now and seal its current creation identity."""
        handle = _open_process(
            pid, PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE | SYNCHRONIZE
        )
        try:
            created = _creation_time(handle)
        except BaseException:
            _close_handle(handle)
            raise
        return cls._sealed(pid, created, handle)

    @classmethod
    def acquire(cls, pid: int, creation_time: int) -> ProcessRef | None:
        """Return the exact sealed identity only while that process is still running.

        A PID that no longer names a process, or that now names a different process,
        returns None instead of raising: for a process identity, absence is the proof
        of death rather than a tool error. A refused handle (for example
        ERROR_ACCESS_DENIED on a protected process) still raises, so an identity that
        could not be inspected never reads as dead.
        """
        if (not isinstance(creation_time, int) or isinstance(creation_time, bool)
                or not 0 <= creation_time <= 0xFFFFFFFFFFFFFFFF):
            raise ValueError("creation_time must be an unsigned FILETIME value")
        try:
            handle = _open_process(
                pid, PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE | SYNCHRONIZE
            )
        except OSError as error:
            if getattr(error, 'winerror', None) in GONE_WINERRORS:
                return None
            raise
        try:
            created = _creation_time(handle)
        except BaseException:
            _close_handle(handle)
            raise
        if created != creation_time:
            _close_handle(handle)
            return None
        return cls._sealed(pid, creation_time, handle)

    @classmethod
    def _sealed(cls, pid: int, creation_time: int, handle) -> ProcessRef:
        instance = cls.__new__(cls)
        instance.pid = pid
        instance.creation_time = int(creation_time)
        instance._handle = handle
        return instance

    @property
    def identity(self) -> tuple[int, int]:
        return self.pid, self.creation_time

    def alive(self) -> bool:
        if self._handle is None:
            raise RuntimeError("ProcessRef is closed")
        result = _kernel().WaitForSingleObject(self._handle, 0)
        if result == WAIT_OBJECT_0:
            return False
        if result == WAIT_TIMEOUT:
            return True
        if result == WAIT_FAILED:
            _raise_last_error("WaitForSingleObject")
        raise OSError(f"unexpected WaitForSingleObject result: {result}")

    def terminate_and_wait(self, deadline: float, exit_code: int = 1) -> None:
        """Terminate this exact process object and wait until its handle is signaled."""
        if self._handle is None:
            raise RuntimeError("ProcessRef is closed")
        if not isinstance(exit_code, int) or isinstance(exit_code, bool) or not 0 <= exit_code <= 0xFFFFFFFF:
            raise ValueError("exit_code must be a DWORD")
        if self.alive() and not _kernel().TerminateProcess(self._handle, exit_code):
            error = ct.get_last_error()
            # Access denied is harmless only if the same process handle is already signaled.
            if self.alive():
                raise ct.WinError(error, "TerminateProcess")
        _deadline_wait(self._handle, deadline)

    def exit_code(self) -> int:
        if self.alive():
            raise RuntimeError("exit code requested before process exit")
        code = w.DWORD()
        if not _kernel().GetExitCodeProcess(self._handle, ct.byref(code)):
            _raise_last_error("GetExitCodeProcess")
        return int(code.value)

    def close(self) -> None:
        handle, self._handle = self._handle, None
        if handle is not None:
            _close_handle(handle)

    def __enter__(self) -> ProcessRef:
        if self._handle is None:
            raise RuntimeError("ProcessRef is closed")
        return self

    def __exit__(self, exc_type, exc, traceback) -> None:
        self.close()

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass


class Job:
    """A named Job handle. Create is controller-only; open is query/terminate-only."""

    def __init__(self, name: str, handle, *, can_assign: bool):
        self.name = name
        self._handle = handle
        self._can_assign = can_assign

    @classmethod
    def create(cls, name: str) -> Job:
        """Create a fresh named Job with KILL_ON_JOB_CLOSE; reject an existing name."""
        _validate_job_name(name)
        kernel = _kernel()
        ct.set_last_error(0)
        handle = kernel.CreateJobObjectW(None, name)
        if not handle:
            _raise_last_error("CreateJobObjectW")
        error = ct.get_last_error()
        if error == ERROR_ALREADY_EXISTS:
            _close_handle(handle)
            raise FileExistsError(ERROR_ALREADY_EXISTS, "named Job already exists", name)
        try:
            limits = _ExtendedLimitInformation()
            limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
            if not kernel.SetInformationJobObject(
                handle,
                JOB_OBJECT_EXTENDED_LIMIT_INFORMATION,
                ct.byref(limits),
                ct.sizeof(limits),
            ):
                _raise_last_error("SetInformationJobObject")
            return cls(name, handle, can_assign=True)
        except BaseException:
            _close_handle(handle)
            raise

    @classmethod
    def open(cls, name: str) -> Job:
        """Open a guardian handle with only query and terminate access."""
        _validate_job_name(name)
        access = JOB_OBJECT_QUERY | JOB_OBJECT_TERMINATE
        handle = _kernel().OpenJobObjectW(access, False, name)
        if not handle:
            _raise_last_error("OpenJobObjectW")
        return cls(name, handle, can_assign=False)

    def active(self) -> int:
        if self._handle is None:
            raise RuntimeError("Job is closed")
        accounting = _BasicAccountingInformation()
        if not _kernel().QueryInformationJobObject(
            self._handle,
            JOB_OBJECT_BASIC_ACCOUNTING_INFORMATION,
            ct.byref(accounting),
            ct.sizeof(accounting),
            None,
        ):
            _raise_last_error("QueryInformationJobObject")
        return int(accounting.ActiveProcesses)

    def contains(self, process: ProcessRef) -> bool:
        if self._handle is None or process._handle is None:
            raise RuntimeError("Job/process handle is closed")
        member = w.BOOL()
        if not _kernel().IsProcessInJob(process._handle, self._handle, ct.byref(member)):
            _raise_last_error("IsProcessInJob")
        return bool(member.value)

    def terminate(self, exit_code: int = 1) -> None:
        if self._handle is None:
            raise RuntimeError("Job is closed")
        if not isinstance(exit_code, int) or isinstance(exit_code, bool) or not 0 <= exit_code <= 0xFFFFFFFF:
            raise ValueError("exit_code must be a DWORD")
        if not _kernel().TerminateJobObject(self._handle, exit_code):
            _raise_last_error("TerminateJobObject")

    def terminate_and_drain(self, deadline: float, exit_code: int = 1) -> int:
        """Terminate members and return only after active count is positively zero."""
        if self._handle is None:
            raise RuntimeError("Job is closed")
        if not isinstance(deadline, (int, float)) or not math.isfinite(deadline):
            raise ValueError("deadline must be a finite monotonic timestamp")
        if self.active():
            try:
                self.terminate(exit_code)
            except OSError:
                # A concurrent exit may make termination fail; only a fresh zero query resolves it.
                if self.active():
                    raise
        while True:
            active = self.active()
            if active == 0:
                return 0
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError(f"Job {self.name!r} still has {active} active process(es)")
            time.sleep(min(_WAIT_SLICE_SECONDS, remaining))

    def spawn(
        self,
        command: Sequence[str],
        output: str | os.PathLike[str] | BinaryIO | int | None,
    ) -> subprocess.Popen[bytes]:
        """Launch a blocked Python gate, assign and verify it, then send its JSON go message.

        The gate process is not guaranteed to be in the Job before assignment succeeds.
        It does not spawn a command before receiving go; parent-pipe EOF makes it exit.
        """
        if self._handle is None:
            raise RuntimeError("Job is closed")
        if not self._can_assign:
            raise PermissionError("this Job handle has no assignment authority")
        argv = _validated_command(command)
        gate = Path(__file__).with_name("worker_gate.py").resolve()
        if not gate.is_file():
            raise FileNotFoundError(gate)
        gate_command = [sys.executable, "-B", "-I", "-S", str(gate)]
        output_handle = None
        close_output = False
        if isinstance(output, (str, os.PathLike)):
            output_handle = open(output, "ab", buffering=0)
            close_output = True
        else:
            output_handle = output
        process = None
        assigned = False
        go_may_have_been_sent = False
        try:
            process = subprocess.Popen(
                gate_command,
                stdin=subprocess.PIPE,
                stdout=output_handle,
                stderr=subprocess.STDOUT,
                close_fds=True,
                creationflags=subprocess.CREATE_NO_WINDOW,
            )
            if process.stdin is None:
                raise RuntimeError("worker gate stdin pipe was not created")
            handle = _open_process(
                process.pid,
                PROCESS_SET_QUOTA
                | PROCESS_TERMINATE
                | PROCESS_QUERY_LIMITED_INFORMATION
                | SYNCHRONIZE,
            )
            try:
                if not _kernel().AssignProcessToJobObject(self._handle, handle):
                    _raise_last_error("AssignProcessToJobObject")
                assigned = True
                in_job = w.BOOL()
                if not _kernel().IsProcessInJob(handle, self._handle, ct.byref(in_job)):
                    _raise_last_error("IsProcessInJob")
                if not in_job.value:
                    raise RuntimeError("worker gate assignment was not confirmed")
            finally:
                _close_handle(handle)
            payload = json.dumps(
                {"go": True, "command": argv},
                ensure_ascii=True,
                separators=(",", ":"),
            ).encode("utf-8") + b"\n"
            go_may_have_been_sent = True
            process.stdin.write(payload)
            process.stdin.flush()
            process.stdin.close()
            return process
        except BaseException:
            if process is not None:
                if process.stdin is not None and not process.stdin.closed:
                    try:
                        process.stdin.close()  # EOF prevents an unstarted gate from spawning.
                    except OSError:
                        pass
                if assigned and go_may_have_been_sent:
                    # A complete line may have reached the gate before a pipe error.
                    try:
                        self.terminate_and_drain(time.monotonic() + 5.0)
                    except Exception:
                        pass
                if process.poll() is None:
                    try:
                        process.kill()  # Popen retains the exact process handle.
                    except OSError:
                        pass
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    raise RuntimeError("worker gate could not be reaped after spawn failure")
            raise
        finally:
            if close_output and output_handle is not None:
                output_handle.close()

    def close(self) -> None:
        handle, self._handle = self._handle, None
        if handle is not None:
            _close_handle(handle)

    def __enter__(self) -> Job:
        if self._handle is None:
            raise RuntimeError("Job is closed")
        return self

    def __exit__(self, exc_type, exc, traceback) -> None:
        self.close()

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass


def _validate_job_name(name: str) -> None:
    if not isinstance(name, str) or not name or "\x00" in name:
        raise ValueError("Job name must be a nonempty string without NUL")


def _validated_command(command: Sequence[str]) -> list[str]:
    if isinstance(command, (str, bytes)) or not isinstance(command, Sequence):
        raise TypeError("command must be a nonempty sequence of strings")
    argv = list(command)
    if not argv or any(not isinstance(part, str) or "\x00" in part for part in argv):
        raise ValueError("command must contain nonempty NUL-free strings")
    if not argv[0]:
        raise ValueError("command executable must not be empty")
    return argv

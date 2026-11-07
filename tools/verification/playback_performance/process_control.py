"""Windows measurement process tree, held until assignment to a kill-on-close Job."""
import ctypes as ct
import os
import json
from pathlib import Path
import subprocess
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[3]))


def affinity():
    from ctypes import wintypes as w
    kernel = ct.WinDLL('kernel32', use_last_error=True)
    kernel.GetCurrentProcess.restype = w.HANDLE
    kernel.GetProcessAffinityMask.argtypes = [w.HANDLE, ct.POINTER(ct.c_size_t), ct.POINTER(ct.c_size_t)]
    process, system = ct.c_size_t(), ct.c_size_t()
    if not kernel.GetProcessAffinityMask(kernel.GetCurrentProcess(), ct.byref(process), ct.byref(system)):
        raise ct.WinError(ct.get_last_error())
    return process.value


def run(command, mask, *, cwd, env, output, timeout):
    if os.name != 'nt':
        raise RuntimeError('PD5 process policy requires Windows')
    from ctypes import wintypes as w

    class Basic(ct.Structure):
        _fields_ = [('process_time', ct.c_int64), ('job_time', ct.c_int64),
                    ('flags', w.DWORD), ('minimum_working_set', ct.c_size_t),
                    ('maximum_working_set', ct.c_size_t), ('active_processes', w.DWORD),
                    ('affinity', ct.c_size_t), ('priority', w.DWORD), ('scheduling', w.DWORD)]

    class Extended(ct.Structure):
        _fields_ = [('basic', Basic), ('io', ct.c_uint64 * 6),
                    ('process_memory', ct.c_size_t), ('job_memory', ct.c_size_t),
                    ('peak_process_memory', ct.c_size_t), ('peak_job_memory', ct.c_size_t)]

    kernel = ct.WinDLL('kernel32', use_last_error=True)
    kernel.CreateJobObjectW.argtypes = [ct.c_void_p, w.LPCWSTR]
    kernel.CreateJobObjectW.restype = w.HANDLE
    kernel.SetInformationJobObject.argtypes = [w.HANDLE, ct.c_int, ct.c_void_p, w.DWORD]
    kernel.AssignProcessToJobObject.argtypes = [w.HANDLE, w.HANDLE]
    kernel.OpenProcess.argtypes = [w.DWORD, w.BOOL, w.DWORD]
    kernel.OpenProcess.restype = w.HANDLE
    kernel.CloseHandle.argtypes = [w.HANDLE]
    job = kernel.CreateJobObjectW(None, None)
    if not job:
        raise ct.WinError(ct.get_last_error())
    process = None
    try:
        limits = Extended()
        limits.basic.flags = 0x2000  # JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
        if not kernel.SetInformationJobObject(job, 9, ct.byref(limits), ct.sizeof(limits)):
            raise ct.WinError(ct.get_last_error())
        wrapper = [sys.executable, '-B', str(Path(__file__).resolve()), '--child', str(mask), *command]
        # Child cannot launch the native process until the parent sends the handshake.
        process = subprocess.Popen(wrapper, cwd=cwd, env=env, stdin=subprocess.PIPE,
            stdout=output, stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW)
        handle = kernel.OpenProcess(0x0101, False, process.pid)  # SET_QUOTA | TERMINATE
        if not handle:
            raise ct.WinError(ct.get_last_error())
        try:
            if not kernel.AssignProcessToJobObject(job, handle):
                raise ct.WinError(ct.get_last_error())
        finally:
            kernel.CloseHandle(handle)
        process.stdin.write(b'go\n')
        process.stdin.flush()
        process.stdin.close()
        return subprocess.CompletedProcess(command, process.wait(timeout=timeout))
    finally:
        # Kills wrapper and every descendant on timeout, exceptions and parent shutdown.
        kernel.CloseHandle(job)
        if process is not None:
            if process.poll() is None:
                # Assignment might have failed, leaving the wrapper waiting at stdin.
                process.kill()
            process.wait(timeout=10)
            if process.stdin and not process.stdin.closed:
                process.stdin.close()


def child():
    if sys.stdin.buffer.readline() != b'go\n':
        return 2
    from ctypes import wintypes as w
    kernel = ct.WinDLL('kernel32', use_last_error=True)
    kernel.GetCurrentProcess.restype = w.HANDLE
    kernel.SetProcessAffinityMask.argtypes = [w.HANDLE, ct.c_size_t]
    if not kernel.SetProcessAffinityMask(kernel.GetCurrentProcess(), int(sys.argv[2])):
        raise ct.WinError(ct.get_last_error())
    frequency, started = ct.c_int64(), ct.c_int64()
    kernel.QueryPerformanceFrequency(ct.byref(frequency))
    kernel.QueryPerformanceCounter(ct.byref(started))
    native = subprocess.Popen(sys.argv[3:])
    print('PD5_PROCESS_START ' + json.dumps({'pid': native.pid,
        'qpc': started.value, 'frequency': frequency.value,
        'boundary': 'before-native-CreateProcess'}), flush=True)
    if os.environ.get('CUEXIS_PROCESS_GPU_MEMORY_LOG'):
        if os.environ.get('CUEXIS_ALLOCATION_OBSERVER_PROCESS') != '1':
            native.kill()
            native.wait()
            raise ValueError('GPU residency sampling is restricted to separate observer processes')
        from tools.verification.playback_performance.gpu_residency import monitor
        monitor(native,os.environ['CUEXIS_PROCESS_GPU_MEMORY_LOG'])
    return native.wait()


if __name__ == '__main__':
    if len(sys.argv) < 4 or sys.argv[1] != '--child':
        raise SystemExit('Internal child only')
    raise SystemExit(child())

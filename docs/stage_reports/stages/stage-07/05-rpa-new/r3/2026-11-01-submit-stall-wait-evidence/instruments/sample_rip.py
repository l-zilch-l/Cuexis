"""Out-of-process instruction pointer sampler (64-bit host required).

Instrument only. Every sample costs the target a suspend/resume round trip, so a run under the
sampler is not comparable with an unsampled run. The question it answers is *where* a thread
spends a multi-millisecond block, not how long the block lasts. The target's own log is the
control: if its slow-frame rate collapses, the sampling rate is too high and the run must be
repeated at a lower rate.

Sampling shape: short back-to-back bursts (fine enough to land inside a 3 ms block several
times), followed by a 1 ms sleep so the sampler never holds a core at full tilt.
"""

import argparse
import ctypes
import struct
import sys
import time
from ctypes import wintypes

kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

THREAD_SUSPEND_RESUME = 0x0002
THREAD_GET_CONTEXT = 0x0008
THREAD_QUERY_INFORMATION = 0x0040
TH32CS_SNAPTHREAD = 0x00000004
INVALID_HANDLE_VALUE = ctypes.c_void_p(-1).value
CONTEXT_AMD64_FULL = 0x0010000B
CONTEXT_FLAGS_OFFSET = 0x30
# AMD64 CONTEXT: six debug registers, then sixteen general purpose registers, so Rip sits at 0x0F8.
RIP_OFFSET = 0x0F8
CONTEXT_BYTES = 1232
PROCESS_QUERY_INFORMATION = 0x0400
PROCESS_VM_READ = 0x0010
STILL_ACTIVE = 259

kernel32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
kernel32.OpenProcess.restype = wintypes.HANDLE
kernel32.OpenThread.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
kernel32.OpenThread.restype = wintypes.HANDLE
kernel32.SuspendThread.argtypes = [wintypes.HANDLE]
kernel32.SuspendThread.restype = wintypes.DWORD
kernel32.ResumeThread.argtypes = [wintypes.HANDLE]
kernel32.ResumeThread.restype = wintypes.DWORD
kernel32.GetThreadContext.argtypes = [wintypes.HANDLE, ctypes.c_void_p]
kernel32.GetThreadContext.restype = wintypes.BOOL
kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
kernel32.CloseHandle.restype = wintypes.BOOL
kernel32.CreateToolhelp32Snapshot.argtypes = [wintypes.DWORD, wintypes.DWORD]
kernel32.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
kernel32.Thread32First.argtypes = [wintypes.HANDLE, ctypes.c_void_p]
kernel32.Thread32First.restype = wintypes.BOOL
kernel32.Thread32Next.argtypes = [wintypes.HANDLE, ctypes.c_void_p]
kernel32.Thread32Next.restype = wintypes.BOOL
kernel32.Module32FirstW.argtypes = [wintypes.HANDLE, ctypes.c_void_p]
kernel32.Module32FirstW.restype = wintypes.BOOL
kernel32.Module32NextW.argtypes = [wintypes.HANDLE, ctypes.c_void_p]
kernel32.Module32NextW.restype = wintypes.BOOL
kernel32.GetExitCodeProcess.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
kernel32.GetExitCodeProcess.restype = wintypes.BOOL
kernel32.K32GetProcessMemoryInfo.argtypes = [wintypes.HANDLE, ctypes.c_void_p, wintypes.DWORD]
kernel32.K32GetProcessMemoryInfo.restype = wintypes.BOOL


class ThreadEntry(ctypes.Structure):
    _fields_ = [
        ("dwSize", wintypes.DWORD),
        ("cntUsage", wintypes.DWORD),
        ("th32ThreadID", wintypes.DWORD),
        ("th32OwnerProcessID", wintypes.DWORD),
        ("tpBasePri", wintypes.LONG),
        ("tpDeltaPri", wintypes.LONG),
        ("dwFlags", wintypes.DWORD),
    ]


def thread_ids(pid):
    snapshot = kernel32.CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0)
    if snapshot == INVALID_HANDLE_VALUE:
        raise OSError("CreateToolhelp32Snapshot failed")
    try:
        entry = ThreadEntry()
        entry.dwSize = ctypes.sizeof(ThreadEntry)
        found = []
        if not kernel32.Thread32First(snapshot, ctypes.byref(entry)):
            return found
        while True:
            if entry.th32OwnerProcessID == pid:
                found.append(entry.th32ThreadID)
            if not kernel32.Thread32Next(snapshot, ctypes.byref(entry)):
                break
        return found
    finally:
        kernel32.CloseHandle(snapshot)


TH32CS_SNAPMODULE = 0x00000008
TH32CS_SNAPMODULE32 = 0x00000010


class ModuleEntry(ctypes.Structure):
    _fields_ = [
        ("dwSize", wintypes.DWORD),
        ("th32ModuleID", wintypes.DWORD),
        ("th32ProcessID", wintypes.DWORD),
        ("GlblcntUsage", wintypes.DWORD),
        ("ProccntUsage", wintypes.DWORD),
        ("modBaseAddr", ctypes.c_void_p),
        ("modBaseSize", wintypes.DWORD),
        ("hModule", wintypes.HMODULE),
        ("szModule", wintypes.WCHAR * 256),
        ("szExePath", wintypes.WCHAR * 260),
    ]


def module_map(pid, attempts=6):
    """Module base/size snapshot taken while sampling, so RIPs resolve against the loaded image.

    `CreateToolhelp32Snapshot(TH32CS_SNAPMODULE)` transiently fails with ERROR_BAD_LENGTH while the
    target is still mapping images, so a short retry loop is part of the interface contract.
    """
    for attempt in range(attempts):
        snapshot = kernel32.CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid)
        if snapshot != INVALID_HANDLE_VALUE:
            break
        error = ctypes.get_last_error()
        if attempt + 1 == attempts or error not in (24, 31, 6):
            raise OSError("CreateToolhelp32Snapshot(modules) failed with {}".format(error))
        time.sleep(0.01 * (attempt + 1))
    try:
        entry = ModuleEntry()
        entry.dwSize = ctypes.sizeof(ModuleEntry)
        found = []
        if not kernel32.Module32FirstW(snapshot, ctypes.byref(entry)):
            return found
        while True:
            found.append((entry.modBaseAddr or 0, entry.modBaseSize, entry.szModule,
                          entry.szExePath))
            if not kernel32.Module32NextW(snapshot, ctypes.byref(entry)):
                break
        return found
    finally:
        kernel32.CloseHandle(snapshot)


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--pid", type=int, required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--modules", default=None)
    parser.add_argument("--ready", default=None)
    parser.add_argument("--ms", type=int, default=20000)
    parser.add_argument("--burst", type=int, default=20)
    parser.add_argument("--pause", type=float, default=0.001)
    args = parser.parse_args(argv)

    process = kernel32.OpenProcess(
        PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, False, args.pid)
    if not process:
        print("OpenProcess failed {}".format(ctypes.get_last_error()), file=sys.stderr)
        return 3

    ids = thread_ids(args.pid)
    handles = []
    for tid in ids:
        handle = kernel32.OpenThread(
            THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, False, tid)
        if handle:
            handles.append((tid, handle))
    print("pid={} threads={} opened={}".format(args.pid, len(ids), len(handles)), flush=True)
    if not handles:
        return 4

    raw = ctypes.create_string_buffer(CONTEXT_BYTES + 32)
    aligned = (ctypes.addressof(raw) + 15) & ~15
    counters = ctypes.create_string_buffer(72)
    # PROCESS_MEMORY_COUNTERS: cb at 0x00, PageFaultCount at 0x04, WorkingSetSize at 0x10.
    ctypes.c_uint32.from_address(ctypes.addressof(counters)).value = 72
    flags = ctypes.c_uint32.from_address(aligned + CONTEXT_FLAGS_OFFSET)
    rip = ctypes.c_uint64.from_address(aligned + RIP_OFFSET)

    samples = {tid: 0 for tid, _ in handles}
    failures = {tid: 0 for tid, _ in handles}
    lines = []
    faults = 0
    working_set = 0
    started = time.perf_counter()
    deadline = started + args.ms / 1000.0
    module_stream = None
    module_seen = set()

    def refresh_modules(phase, strict=False):
        if module_stream is None:
            return
        try:
            found = module_map(args.pid)
        except OSError:
            if strict:
                raise
            return
        if strict and not found:
            raise OSError("empty module snapshot")
        for base, size, name, path in found:
            key = (base, name)
            if key in module_seen:
                continue
            module_seen.add(key)
            module_stream.write("{:016x} {} {} {}\n".format(base, size, name, path))
        module_stream.write("# phase={} t_us={}\n".format(
            phase, int((time.perf_counter() - started) * 1000000.0)))
        module_stream.flush()

    if args.modules:
        module_stream = open(args.modules, "w", encoding="utf-8", newline="\n")
        module_stream.write("# module snapshot; modules appear as they are loaded\n")
        refresh_modules("before", strict=True)
    if args.ready:
        with open(args.ready, "w", encoding="utf-8") as marker:
            marker.write("ready\n")
    try:
        with open(args.out, "w", encoding="utf-8", newline="\n") as stream:
            stream.write("# qpc_us,tid,rip,faults,working_set\n")
            burst = 0
            next_module = started + 0.25
            while True:
                now = time.perf_counter()
                if now >= deadline:
                    break
                for tid, handle in handles:
                    if kernel32.SuspendThread(handle) == 0xFFFFFFFF:
                        failures[tid] += 1
                        continue
                    flags.value = CONTEXT_AMD64_FULL
                    ok = kernel32.GetThreadContext(handle, ctypes.c_void_p(aligned))
                    kernel32.ResumeThread(handle)
                    if not ok:
                        failures[tid] += 1
                        continue
                    if rip.value == 0:
                        # A live thread never has a null instruction pointer; treat it as a
                        # capture failure rather than as a real sample.
                        failures[tid] += 1
                        continue
                    samples[tid] += 1
                    lines.append("{},{},{:016x},{},{}\n".format(
                        int((time.perf_counter() - started) * 1000000.0), tid, rip.value,
                        faults, working_set))
                burst += 1
                if burst >= args.burst:
                    burst = 0
                    if kernel32.K32GetProcessMemoryInfo(
                            process, ctypes.byref(counters), ctypes.sizeof(counters)):
                        faults = struct.unpack_from("<I", counters.raw, 4)[0]
                        working_set = struct.unpack_from("<Q", counters.raw, 16)[0]
                    if len(lines) >= 2000:
                        stream.writelines(lines)
                        lines = []
                    if now >= next_module:
                        next_module = now + 0.25
                        refresh_modules("mid")
                    time.sleep(args.pause)
            stream.writelines(lines)
    finally:
        for _, handle in handles:
            kernel32.CloseHandle(handle)
        kernel32.CloseHandle(process)

    elapsed = time.perf_counter() - started
    total = sum(samples.values())
    if module_stream is not None:
        refresh_modules("after")
        module_stream.close()
    print("elapsed_s={:.3f} samples={} rate={:.0f}/s failures={}".format(
        elapsed, total, total / elapsed if elapsed else 0.0, sum(failures.values())), flush=True)
    for tid, _ in handles:
        print("  tid={} samples={} failures={}".format(tid, samples[tid], failures[tid]), flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

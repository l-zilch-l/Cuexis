"""Out-of-process sampler that records the wait handle of a blocking system call.

`sample_rip.py` proved that the stalled interval is spent inside ntdll system call stubs, so the
instruction pointer alone cannot separate "waiting for the GPU", "waiting for the compositor" and
"waiting on a driver-internal object". On x64 the `Nt*` stub starts with `mov r10, rcx`, so the
first syscall argument (the object handle) survives in R10 for the whole call and is present in the
user context the kernel reports for a suspended thread. Recording R10 next to the instruction
pointer turns each sample into "which object was this thread waiting on", and identifying that
object (type and name) answers the hypothesis directly.

Instrument only, exactly like `sample_rip.py`: every sample costs the target a suspend/resume round
trip, so a sampled run is not comparable with an unsampled one. The target's own log is the control.
Handle identification duplicates the target handle into this process and queries its type before its
name; `NtQueryObject(ObjectNameInformation)` is skipped for every object type that can block, which
is the documented way to avoid the well known naming deadlock.
"""

import argparse
import bisect
import ctypes
import struct
import sys
import time
from ctypes import wintypes

kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
ntdll = ctypes.WinDLL("ntdll")

THREAD_SUSPEND_RESUME = 0x0002
THREAD_GET_CONTEXT = 0x0008
THREAD_QUERY_INFORMATION = 0x0040
TH32CS_SNAPTHREAD = 0x00000004
TH32CS_SNAPMODULE = 0x00000008
TH32CS_SNAPMODULE32 = 0x00000010
INVALID_HANDLE_VALUE = ctypes.c_void_p(-1).value
CONTEXT_AMD64_FULL = 0x0010000B
CONTEXT_FLAGS_OFFSET = 0x30
# AMD64 CONTEXT: six debug registers, sixteen general purpose registers, so Rip sits at 0x0F8.
RIP_OFFSET = 0x0F8
# General purpose registers start at 0x078 with Rax; R10 is the tenth, so 0x078 + 10 * 8.
R10_OFFSET = 0x0C8
# Rsp is the fourth general purpose register, so 0x078 + 4 * 8.
RSP_OFFSET = 0x098
CONTEXT_BYTES = 1232
PROCESS_QUERY_INFORMATION = 0x0400
PROCESS_VM_READ = 0x0010
PROCESS_DUP_HANDLE = 0x0040
SYNCHRONIZE = 0x00100000
DUPLICATE_SAME_ACCESS = 0x0002
OBJECT_NAME_INFORMATION = 1
OBJECT_TYPE_INFORMATION = 2
# Naming an object of one of these types can block on outstanding I/O, so only the type is queried.
UNSAFE_NAME_TYPES = {"File", "Directory", "SymbolicLink", "Pipe", "Device"}

kernel32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
kernel32.OpenProcess.restype = wintypes.HANDLE
kernel32.OpenThread.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
kernel32.OpenThread.restype = wintypes.HANDLE
kernel32.GetCurrentProcess.argtypes = []
kernel32.GetCurrentProcess.restype = wintypes.HANDLE
kernel32.DuplicateHandle.argtypes = [wintypes.HANDLE, wintypes.HANDLE, wintypes.HANDLE,
                                     ctypes.POINTER(wintypes.HANDLE), wintypes.DWORD,
                                     wintypes.BOOL, wintypes.DWORD]
kernel32.DuplicateHandle.restype = wintypes.BOOL
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
kernel32.K32GetProcessMemoryInfo.argtypes = [wintypes.HANDLE, ctypes.c_void_p, wintypes.DWORD]
kernel32.K32GetProcessMemoryInfo.restype = wintypes.BOOL
kernel32.ReadProcessMemory.argtypes = [wintypes.HANDLE, ctypes.c_void_p, ctypes.c_void_p,
                                       ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
kernel32.ReadProcessMemory.restype = wintypes.BOOL
kernel32.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
kernel32.WaitForSingleObject.restype = wintypes.DWORD
kernel32.GetProcessId.argtypes = [wintypes.HANDLE]
kernel32.GetProcessId.restype = wintypes.DWORD
kernel32.GetThreadId.argtypes = [wintypes.HANDLE]
kernel32.GetThreadId.restype = wintypes.DWORD
kernel32.GetProcessIdOfThread.argtypes = [wintypes.HANDLE]
kernel32.GetProcessIdOfThread.restype = wintypes.DWORD
kernel32.QueryFullProcessImageNameW.argtypes = [wintypes.HANDLE, wintypes.DWORD,
                                                ctypes.c_wchar_p, ctypes.POINTER(wintypes.DWORD)]
kernel32.QueryFullProcessImageNameW.restype = wintypes.BOOL
ntdll.NtQueryObject.argtypes = [wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p, wintypes.ULONG,
                                ctypes.POINTER(wintypes.ULONG)]
ntdll.NtQueryObject.restype = ctypes.c_long


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


def module_map(pid, attempts=6):
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


def unicode_string(buffer, offset):
    # UNICODE_STRING on x64 is two 16-bit lengths, four bytes of padding, then an 8-byte pointer.
    length, _maximum, pointer = struct.unpack_from("<HH4xQ", buffer, offset)
    text = ""
    if length:
        base = ctypes.addressof(buffer)
        inside = base <= pointer < base + len(buffer)
        if inside and pointer + length <= base + len(buffer):
            text = ctypes.string_at(pointer, length).decode("utf-16-le", "replace")
        elif length <= len(buffer) - offset - 16:
            # The kernel is not required to relocate the pointer into the caller buffer.
            text = ctypes.string_at(base + offset + 16, length).decode("utf-16-le", "replace")
    return length, text


class HandleLookup:
    """Type and name of a target handle, queried at most once per handle value."""

    def __init__(self, process):
        self.process = process
        self.cache = {}
        self.errors = 0
        self.buffer = ctypes.create_string_buffer(0x1000)
        self.duplicated = ctypes.c_void_p()

    def describe(self, value):
        if value in self.cache:
            return self.cache[value]
        if len(self.cache) >= 4096:
            return None
        duplicate = wintypes.HANDLE()
        if not kernel32.DuplicateHandle(self.process, ctypes.c_void_p(value),
                                        kernel32.GetCurrentProcess(), ctypes.byref(duplicate),
                                        0, False, DUPLICATE_SAME_ACCESS):
            self.errors += 1
            self.cache[value] = {"error": "duplicate {}".format(ctypes.get_last_error())}
            return self.cache[value]
        result = {"value": value}
        try:
            status = ntdll.NtQueryObject(duplicate, OBJECT_TYPE_INFORMATION, self.buffer,
                                         ctypes.sizeof(self.buffer), None)
            if status != 0:
                result["error"] = "type status {:#x}".format(status & 0xFFFFFFFF)
                self.errors += 1
                self.cache[value] = result
                return result
            _length, kind = unicode_string(self.buffer, 0)
            result["type"] = kind
            if kind == "Process":
                identifier = kernel32.GetProcessId(duplicate)
                result["id"] = identifier
                image = ctypes.create_unicode_buffer(512)
                size = wintypes.DWORD(512)
                if kernel32.QueryFullProcessImageNameW(duplicate, 0, image,
                                                       ctypes.byref(size)):
                    result["image"] = image.value
            elif kind == "Thread":
                identifier = kernel32.GetThreadId(duplicate)
                result["id"] = identifier
                result["pid"] = kernel32.GetProcessIdOfThread(duplicate)
            if kind not in UNSAFE_NAME_TYPES:
                status = ntdll.NtQueryObject(duplicate, OBJECT_NAME_INFORMATION, self.buffer,
                                             ctypes.sizeof(self.buffer), None)
                if status == 0:
                    _length, name = unicode_string(self.buffer, 0)
                    result["name"] = name
                else:
                    result["name_status"] = "{:#x}".format(status & 0xFFFFFFFF)
            result["type_offset"] = 0
        finally:
            kernel32.CloseHandle(duplicate)
        self.cache[value] = result
        return result


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--pid", type=int, required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--modules", default=None)
    parser.add_argument("--handles-out", default=None)
    parser.add_argument("--ready", default=None)
    parser.add_argument("--ms", type=int, default=20000)
    parser.add_argument("--burst", type=int, default=20)
    parser.add_argument("--pause", type=float, default=0.001)
    parser.add_argument("--handle-every", type=int, default=1,
                        help="identify one new handle value every Nth burst to bound perturbation")
    parser.add_argument("--stack-bytes", type=int, default=0,
                        help="scan this many stack bytes for call-preceded return addresses")
    parser.add_argument("--stack-candidates", type=int, default=4)
    args = parser.parse_args(argv)

    process = kernel32.OpenProcess(
        PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_DUP_HANDLE | SYNCHRONIZE, False,
        args.pid)
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
    ctypes.c_uint32.from_address(ctypes.addressof(counters)).value = 72
    flags = ctypes.c_uint32.from_address(aligned + CONTEXT_FLAGS_OFFSET)
    rip = ctypes.c_uint64.from_address(aligned + RIP_OFFSET)
    r10 = ctypes.c_uint64.from_address(aligned + R10_OFFSET)
    rsp = ctypes.c_uint64.from_address(aligned + RSP_OFFSET)
    frame = ctypes.create_string_buffer(16)
    transferred = ctypes.c_size_t()
    lookup = HandleLookup(process) if args.handles_out else None

    samples = {tid: 0 for tid, _ in handles}
    failures = {tid: 0 for tid, _ in handles}
    failure_codes = {}
    failure_seconds = {}
    retries = [0, 0]

    def suspend(handle):
        """SuspendThread reports access denied for short transient windows; retry before counting."""
        for attempt in range(4):
            if kernel32.SuspendThread(handle) != 0xFFFFFFFF:
                if attempt:
                    retries[0] += 1
                    retries[1] += attempt
                return True
        return False
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
    ntdll_range = (0, 0)
    for base, size, name, _path in module_map(args.pid):
        if name.lower() == "ntdll.dll":
            ntdll_range = (base, base + size)
    if lookup is not None and ntdll_range == (0, 0):
        print("ntdll not in the module list", file=sys.stderr)
        return 5
    range_map = None
    stack = ctypes.create_string_buffer(max(8, args.stack_bytes))
    instruction = ctypes.create_string_buffer(1)

    def scan_stack(stack_pointer):
        """Return addresses above the immediate caller, keeping only call-preceded words."""
        if not kernel32.ReadProcessMemory(process, ctypes.c_void_p(stack_pointer),
                                          ctypes.byref(stack), args.stack_bytes,
                                          ctypes.byref(transferred)):
            return []
        found = []
        count = min(args.stack_bytes, transferred.value or 0) // 8
        for index in range(1, count):
            value = struct.unpack_from("<Q", stack.raw, index * 8)[0]
            if code_range(value) is None:
                continue
            if not kernel32.ReadProcessMemory(process, ctypes.c_void_p(value - 5),
                                              ctypes.byref(instruction), 1,
                                              ctypes.byref(transferred)):
                continue
            if instruction.raw[0] != 0xE8:
                continue
            found.append(value)
            if len(found) >= args.stack_candidates:
                break
        return found

    def code_range(value):
        nonlocal range_map
        if range_map is None:
            range_map = []
            for base, size, name, _path in module_map(args.pid):
                range_map.append((base, base + size, name))
            range_map.sort()
        index = bisect.bisect_right([entry[0] for entry in range_map], value) - 1
        if index < 0:
            return None
        low, high, name = range_map[index]
        return (name, value - low) if value < high else None
    if args.ready:
        with open(args.ready, "w", encoding="utf-8") as marker:
            marker.write("ready\n")
    try:
        with open(args.out, "w", encoding="utf-8", newline="\n") as stream:
            stream.write("# qpc_us,tid,rip,faults,working_set,r10,rsp,caller[,candidates]\n")
            burst = 0
            pending = []
            pending_values = set()
            next_module = started + 0.25
            reason = "deadline"
            while True:
                now = time.perf_counter()
                if now >= deadline:
                    break
                if kernel32.WaitForSingleObject(process, 0) == 0:
                    # Threads of a terminated process refuse to suspend, so stop with the process.
                    reason = "process_exit"
                    break
                for tid, handle in handles:
                    if not suspend(handle):
                        failures[tid] += 1
                        second = int(now - started)
                        failure_seconds[second] = failure_seconds.get(second, 0) + 1
                        code = "suspend {}".format(ctypes.get_last_error())
                        failure_codes[code] = failure_codes.get(code, 0) + 1
                        continue
                    flags.value = CONTEXT_AMD64_FULL
                    ctypes.set_last_error(0)
                    ok = kernel32.GetThreadContext(handle, ctypes.c_void_p(aligned))
                    kernel32.ResumeThread(handle)
                    if not ok:
                        failures[tid] += 1
                        code = "context {}".format(ctypes.get_last_error())
                        failure_codes[code] = failure_codes.get(code, 0) + 1
                        continue
                    if rip.value == 0:
                        failures[tid] += 1
                        continue
                    samples[tid] += 1
                    caller = 0
                    if kernel32.ReadProcessMemory(process, ctypes.c_void_p(rsp.value),
                                                  ctypes.byref(frame), 16,
                                                  ctypes.byref(transferred)):
                        caller = struct.unpack_from("<Q", frame.raw, 0)[0]
                    fields = [str(int((time.perf_counter() - started) * 1000000.0)), str(tid),
                              "{:016x}".format(rip.value), str(faults), str(working_set),
                              "{:016x}".format(r10.value), "{:016x}".format(rsp.value),
                              "{:016x}".format(caller)]
                    if args.stack_bytes:
                        for value in scan_stack(rsp.value):
                            fields.append("{:016x}".format(value))
                    lines.append(",".join(fields) + "\n")
                    if (lookup is not None and r10.value
                            and len(pending) < args.handle_every
                            and ntdll_range[0] <= rip.value < ntdll_range[1]):
                        value = r10.value
                        if value not in lookup.cache and value not in pending_values:
                            pending.append((tid, value))
                            pending_values.add(value)
                burst += 1
                if burst >= args.burst:
                    burst = 0
                    if lookup is not None:
                        # One new handle per burst bounds the perturbation and still labels every
                        # distinct wait target within the first seconds of a run.
                        for _tid, value in pending:
                            lookup.describe(value)
                        pending = []
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

    elapsed = time.perf_counter() - started
    total = sum(samples.values())
    if args.handles_out and lookup is not None:
        with open(args.handles_out, "w", encoding="utf-8", newline="\n") as stream:
            stream.write("# value type name\n")
            for value in sorted(lookup.cache):
                entry = lookup.cache[value]
                detail = ""
                if "id" in entry:
                    detail = "id={} pid={} image={}".format(
                        entry["id"], entry.get("pid", "?"), entry.get("image", "-"))
                stream.write("0x{:x} {} {} {} {}\n".format(
                    value, entry.get("type", "?"), entry.get("name", "") or "-",
                    entry.get("error", "") or entry.get("name_status", ""), detail))
    if module_stream is not None:
        refresh_modules("after")
        module_stream.close()
    kernel32.CloseHandle(process)
    print("elapsed_s={:.3f} samples={} rate={:.0f}/s failures={} handles={} reason={}".format(
        elapsed, total, total / elapsed if elapsed else 0.0, sum(failures.values()),
        len(lookup.cache) if lookup else 0, reason), flush=True)
    for tid, _ in handles:
        print("  tid={} samples={} failures={}".format(tid, samples[tid], failures[tid]),
              flush=True)
    if failure_codes:
        print("  failure_codes {}".format(sorted(failure_codes.items())), flush=True)
    if retries[0]:
        print("  suspend_retries rescued={} attempts={}".format(retries[0], retries[1]), flush=True)
    if failure_seconds:
        spread = sorted(failure_seconds.items())
        print("  failure_seconds {} buckets, first {} last {}".format(
            len(spread), spread[:3], spread[-3:]), flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

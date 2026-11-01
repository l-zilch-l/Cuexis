"""Walk the full x64 call chain of every thread stopped inside a kernel syscall stub.

The 8-byte stack scan used by `sample_wait.py --stack-bytes` cannot tell a live frame
from a stale word left behind by an earlier call at the same stack depth.  This tool
unwinds properly: it suspends each target thread, records where RIP is, and - when RIP
sits at the `syscall` of an ntdll stub - runs dbghelp `StackWalk64`, which walks the
`.pdata` unwind records of every module in the target.  No PDB is needed: without
symbols a frame is reported as `module+rva`, which is all we need, because the question
is which module owns the caller, not what the C++ name was.

The render thread is identified by its own data rather than guessed: it is the thread
whose sampled RIP lands in the probe executable most often.

Usage:
    python -B walk_once.py --pid 1234 [--attempts 60] [--want 12]
"""

import argparse
import bisect
import collections
import ctypes
import sys
import time
from ctypes import wintypes
from pathlib import Path

import pe_bytes
import sample_wait

kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

IMAGE_FILE_MACHINE_AMD64 = 0x8664
PROCESS_SUSPEND_RESUME = 0x0800
ADDR_MODE_FLAT = 3
STILL_ACTIVE = 259
STUB_OFFSET = 19          # recorded samples land at stub+20 (after `syscall`), see the notes
RIP_AT = sample_wait.RIP_OFFSET
RSP_AT = sample_wait.RSP_OFFSET
RBP_AT = 0x0A0

# --- dbghelp ----------------------------------------------------------------


class ADDRESS64(ctypes.Structure):
    _fields_ = [("Offset", ctypes.c_uint64), ("Segment", wintypes.WORD),
                ("Mode", ctypes.c_int)]


class KDHELP64(ctypes.Structure):
    _fields_ = [("Thread", ctypes.c_uint64),
                ("ThCallbackStack", wintypes.DWORD),
                ("ThCallbackBStore", wintypes.DWORD),
                ("NextCallback", wintypes.DWORD),
                ("FramePointer", wintypes.DWORD),
                ("KiCallUserMode", ctypes.c_uint64),
                ("KeUserCallbackDispatcher", ctypes.c_uint64),
                ("SystemRangeStart", ctypes.c_uint64),
                ("KiUserExceptionDispatcher", ctypes.c_uint64),
                ("StackBase", ctypes.c_uint64),
                ("StackLimit", ctypes.c_uint64),
                ("BuildVersion", ctypes.c_uint64),
                ("RetpolineStubFunctionTable", ctypes.c_uint64),
                ("RetpolineStubFunctionTableSize", wintypes.DWORD),
                ("Reserved1", wintypes.DWORD),
                ("Reserved2", ctypes.c_uint64 * 3)]


class STACKFRAME64(ctypes.Structure):
    _fields_ = [("AddrPC", ADDRESS64),
                ("AddrReturn", ADDRESS64),
                ("AddrFrame", ADDRESS64),
                ("AddrStack", ADDRESS64),
                ("AddrBStore", ADDRESS64),
                ("FuncTableEntry", ctypes.c_void_p),
                ("Params", ctypes.c_uint64 * 4),
                ("Far", wintypes.BOOL),
                ("Virtual", wintypes.BOOL),
                ("Reserved", ctypes.c_uint64 * 3),
                ("KdHelp", KDHELP64)]


STACKFRAME_SLACK = 0x200
CONTEXT_FLAGS = 0x0010000B

dbghelp = ctypes.WinDLL("dbghelp", use_last_error=True)
dbghelp.SymSetOptions.argtypes = [wintypes.DWORD]
dbghelp.SymSetOptions.restype = wintypes.DWORD
dbghelp.SymInitializeW.argtypes = [wintypes.HANDLE, ctypes.c_wchar_p, wintypes.BOOL]
dbghelp.SymInitializeW.restype = wintypes.BOOL
dbghelp.SymCleanup.argtypes = [wintypes.HANDLE]
dbghelp.SymCleanup.restype = wintypes.BOOL
dbghelp.StackWalk64.argtypes = [wintypes.DWORD, wintypes.HANDLE, wintypes.HANDLE,
                                ctypes.POINTER(STACKFRAME64), ctypes.c_void_p,
                                ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p,
                                ctypes.c_void_p]
dbghelp.StackWalk64.restype = wintypes.BOOL

kernel32.OpenThread.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
kernel32.OpenThread.restype = wintypes.HANDLE
kernel32.GetExitCodeThread.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
kernel32.GetExitCodeThread.restype = wintypes.BOOL
sample_wait.kernel32.GetThreadContext.argtypes = [wintypes.HANDLE, ctypes.c_void_p]
sample_wait.kernel32.GetThreadContext.restype = wintypes.BOOL

SYMOPT_UNDNAME = 0x00000002
SYMOPT_DEFERRED_LOADS = 0x00000004
SYMOPT_LOAD_LINES = 0x00000010
SYMOPT_OMAP_FIND_NEAREST = 0x00000020


class MEMORY_BASIC_INFORMATION(ctypes.Structure):
    _fields_ = [("BaseAddress", ctypes.c_void_p),
                ("AllocationBase", ctypes.c_void_p),
                ("AllocationProtect", wintypes.DWORD),
                ("PartitionId", wintypes.WORD),
                ("RegionSize", ctypes.c_size_t),
                ("State", wintypes.DWORD),
                ("Protect", wintypes.DWORD),
                ("Type", wintypes.DWORD)]


MEM_IMAGE = 0x01000000

kernel32.VirtualQueryEx.argtypes = [wintypes.HANDLE, ctypes.c_void_p,
                                    ctypes.POINTER(MEMORY_BASIC_INFORMATION),
                                    ctypes.c_size_t]
kernel32.VirtualQueryEx.restype = ctypes.c_size_t
try:
    _mapped_file = kernel32.K32GetMappedFileNameW
except AttributeError:
    _mapped_file = ctypes.WinDLL("psapi", use_last_error=True).GetMappedFileNameW
_mapped_file.argtypes = [wintypes.HANDLE, ctypes.c_void_p, ctypes.c_wchar_p,
                         wintypes.DWORD]
_mapped_file.restype = wintypes.DWORD


class Modules:
    """module+rva naming for the target's address space.

    ToolHelp only lists modules created through the loader.  Image views mapped later by a
    driver, and any view whose module record we snapshotted before it appeared, are resolved
    a second way: `VirtualQueryEx` says whether the address lives in an image, and
    `GetMappedFileNameW` names the backing file of that image's allocation base.
    """

    def __init__(self, pid, process=None):
        self.process = process
        self.rows = sorted((base, size, name)
                           for base, size, name, _path in sample_wait.module_map(pid))
        self.bases = [row[0] for row in self.rows]
        self.mapped = {}
        self.fallbacks = collections.Counter()

    def _image_name(self, allocation_base):
        if self.process is None or not allocation_base:
            return None
        if allocation_base in self.mapped:
            return self.mapped[allocation_base]
        buffer = ctypes.create_unicode_buffer(512)
        length = _mapped_file(self.process, ctypes.c_void_p(allocation_base),
                              buffer, 512)
        name = Path(buffer.value).name.split("\\")[-1] if length else None
        self.mapped[allocation_base] = name
        return name

    def locate(self, address):
        index = bisect.bisect_right(self.bases, address) - 1
        if index >= 0:
            base, size, name = self.rows[index]
            if address < base + size:
                return name, address - base
        information = MEMORY_BASIC_INFORMATION()
        if self.process and kernel32.VirtualQueryEx(
                self.process, ctypes.c_void_p(address), ctypes.byref(information),
                ctypes.sizeof(information)) == ctypes.sizeof(information):
            if information.Type == MEM_IMAGE:
                base = information.AllocationBase or 0
                name = self._image_name(base)
                if name:
                    self.fallbacks[name] += 1
                    return name, address - base
        return "unmapped", address

    def label(self, address):
        name, offset = self.locate(address)
        return "{} +{:#x}".format(name, offset)


def walk(process, thread, context, depth):
    frame = STACKFRAME64.from_buffer(ctypes.create_string_buffer(STACKFRAME_SLACK))
    frame.AddrPC.Offset = int.from_bytes(context[RIP_AT: RIP_AT + 8], "little")
    frame.AddrPC.Mode = ADDR_MODE_FLAT
    frame.AddrStack.Offset = int.from_bytes(context[RSP_AT: RSP_AT + 8], "little")
    frame.AddrStack.Mode = ADDR_MODE_FLAT
    frame.AddrFrame.Offset = int.from_bytes(context[RBP_AT: RBP_AT + 8], "little")
    frame.AddrFrame.Mode = ADDR_MODE_FLAT
    chain = []
    for _ in range(depth):
        if not dbghelp.StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, thread,
                                   ctypes.byref(frame), ctypes.byref(context),
                                   None, None, None, None):
            break
        address = frame.AddrPC.Offset
        if address == 0:
            break
        chain.append(address)
    return chain


class Walks:
    def __init__(self, pid, stubs, depth):
        self.pid = pid
        self.stubs = stubs
        self.depth = depth
        self.process = sample_wait.kernel32.OpenProcess(
            sample_wait.PROCESS_QUERY_INFORMATION | sample_wait.PROCESS_VM_READ |
            sample_wait.PROCESS_DUP_HANDLE | sample_wait.SYNCHRONIZE |
            PROCESS_SUSPEND_RESUME, False, pid)
        if not self.process:
            raise SystemExit("cannot open pid {}: error {}".format(
                pid, ctypes.get_last_error()))
        self.modules = Modules(pid, self.process)
        if not dbghelp.SymInitializeW(self.process, None, True):
            raise SystemExit("SymInitialize failed: {}".format(ctypes.get_last_error()))
        dbghelp.SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES |
                              SYMOPT_OMAP_FIND_NEAREST)
        self.rip_module = collections.defaultdict(collections.Counter)
        self.rip_site = collections.defaultdict(collections.Counter)
        self.chains = collections.defaultdict(list)
        self.attempts = 0
        self.misses = collections.Counter()

    def close(self):
        dbghelp.SymCleanup(self.process)
        kernel32.CloseHandle(self.process)

    def attempt(self):
        self.attempts += 1
        for tid in sample_wait.thread_ids(self.pid):
            thread = kernel32.OpenThread(
                sample_wait.THREAD_SUSPEND_RESUME | sample_wait.THREAD_GET_CONTEXT |
                sample_wait.THREAD_QUERY_INFORMATION, False, tid)
            if not thread:
                self.misses["open {}".format(ctypes.get_last_error())] += 1
                continue
            try:
                exit_code = wintypes.DWORD()
                if not kernel32.GetExitCodeThread(thread, ctypes.byref(exit_code)) or \
                        exit_code.value != STILL_ACTIVE:
                    continue
                if kernel32.SuspendThread(thread) == 0xFFFFFFFF:
                    self.misses["suspend {}".format(ctypes.get_last_error())] += 1
                    continue
                try:
                    context = ctypes.create_string_buffer(sample_wait.CONTEXT_BYTES)
                    context[0x30:0x34] = CONTEXT_FLAGS.to_bytes(4, "little")
                    if not sample_wait.kernel32.GetThreadContext(thread, context):
                        self.misses["context {}".format(ctypes.get_last_error())] += 1
                        continue
                    rip = int.from_bytes(context[RIP_AT: RIP_AT + 8], "little")
                    name, rva = self.modules.locate(rip)
                    self.rip_module[tid][name] += 1
                    self.rip_site[tid][(name, rva)] += 1
                    if name.lower() != "ntdll.dll":
                        continue
                    resolved = pe_bytes.resolve(self.stubs, rva)
                    if not resolved or resolved[2] < STUB_OFFSET:
                        continue
                    chain = walk(self.process, thread, context, self.depth)
                    self.chains[tid].append((resolved[1], resolved[0], rip, rva,
                                             [self.modules.label(a) for a in chain]))
                finally:
                    kernel32.ResumeThread(thread)
            finally:
                kernel32.CloseHandle(thread)


def summarize(walks, want, out):
    exe_names = [name for name in {n for counter in walks.rip_module.values()
                                   for n in counter}
                 if name.lower().startswith("cuexis_")]
    render = None
    for tid, counter in walks.rip_module.items():
        share = sum(counter[name] for name in exe_names) / max(1, sum(counter.values()))
        if share > 0 and (render is None or share > render[1]):
            render = (tid, share)
    lines = []
    lines.append("attempts {} threads {}".format(walks.attempts, len(walks.rip_module)))
    for tid in sorted(walks.rip_module):
        counter = walks.rip_module[tid]
        total = sum(counter.values())
        tag = " <- render thread" if render and tid == render[0] else ""
        lines.append("tid {:6d} samples {:4d}{}".format(tid, total, tag))
        for name, count in counter.most_common(5):
            lines.append("        rip {:<34s} {:5d} {:6.1%}".format(name, count,
                                                                    count / total))
        syscalls = collections.Counter((c[0], c[1]) for c in walks.chains.get(tid, [])
                                       if c[0] is not None)
        for (ssn, syscall), count in syscalls.most_common(6):
            lines.append("        sys {:>3d} {:<32s} {:5d}".format(ssn, syscall, count))
        signatures = collections.Counter()
        for ssn, syscall, _rip, _rva, chain in walks.chains.get(tid, []):
            signatures[(syscall, " <- ".join(chain[2:6]))] += 1
        for (syscall, signature), count in signatures.most_common(6):
            lines.append("        chain {} x{}  [{}]".format(syscall, count, signature))
    if render:
        tid = render[0]
        lines.append("")
        lines.append("render thread tid={} exe-rip share {:.1%}, {} walks".format(
            tid, render[1], len(walks.chains.get(tid, []))))
        for index, (ssn, syscall, _rip, _rva, chain) in enumerate(
                walks.chains.get(tid, [])[:want]):
            lines.append("  [{}] {} ({} frames)".format(index + 1, syscall, len(chain)))
            for depth, frame in enumerate(chain):
                lines.append("        {:2d} {}".format(depth, frame))
    for reason, count in walks.misses.most_common(8):
        lines.append("  miss {:5d} {}".format(count, reason))
    for name, count in walks.modules.fallbacks.most_common(8):
        lines.append("  named-by-mapped-file {:5d} {}".format(count, name))
    text = "\n".join(lines)
    print(text)
    if out:
        with open(out, "w", encoding="utf-8", newline="\n") as stream:
            stream.write(text + "\n")
    return render


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--pid", type=int, required=True)
    parser.add_argument("--attempts", type=int, default=40)
    parser.add_argument("--want", type=int, default=10, help="chains to print per thread")
    parser.add_argument("--max-depth", type=int, default=20)
    parser.add_argument("--gap-ms", type=float, default=0.0)
    parser.add_argument("--ntdll", default=r"C:\Windows\System32\ntdll.dll")
    parser.add_argument("--out", default=None)
    args = parser.parse_args(argv)
    stubs = pe_bytes.stub_table(args.ntdll)
    walks = Walks(args.pid, stubs, args.max_depth)
    try:
        for _ in range(args.attempts):
            walks.attempt()
            if args.gap_ms:
                time.sleep(args.gap_ms / 1000.0)
    finally:
        walks.close()
    summarize(walks, args.want, args.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())

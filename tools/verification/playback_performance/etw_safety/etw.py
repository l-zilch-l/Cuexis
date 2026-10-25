"""Private diagnostic tool: elevated, exact-name ETW inventory and emergency stop."""
import ctypes as ct
from ctypes import wintypes as w
import os
from pathlib import Path
import re


def require_admin():
    if os.name != 'nt' or ct.sizeof(ct.c_void_p) != 8 or not ct.windll.shell32.IsUserAnAdmin():
        raise PermissionError('64-bit Python with an elevated administrator token is required')


class Wnode(ct.Structure):
    _fields_ = [('BufferSize', w.ULONG), ('ProviderId', w.ULONG),
                ('HistoricalContext', ct.c_uint64), ('TimeStamp', ct.c_int64),
                ('Guid', ct.c_byte * 16), ('ClientContext', w.ULONG), ('Flags', w.ULONG)]


class Properties(ct.Structure):
    _fields_ = [('Wnode', Wnode)] + [(name, w.ULONG) for name in (
        'BufferSize', 'MinimumBuffers', 'MaximumBuffers', 'MaximumFileSize',
        'LogFileMode', 'FlushTimer', 'EnableFlags', 'AgeLimit', 'NumberOfBuffers',
        'FreeBuffers', 'EventsLost', 'BuffersWritten', 'LogBuffersLost', 'RealTimeBuffersLost')]
    _fields_ += [('LoggerThreadId', w.HANDLE), ('LogFileNameOffset', w.ULONG),
                 ('LoggerNameOffset', w.ULONG)]


def make_buffer():
    if ct.sizeof(Properties) != 120 or Properties.LoggerNameOffset.offset != 116:
        raise RuntimeError('unexpected x64 EVENT_TRACE_PROPERTIES ABI')
    buf = ct.create_string_buffer(120 + 2 * 2048 * 2)
    props = ct.cast(buf, ct.POINTER(Properties))
    props.contents.Wnode.BufferSize = len(buf)
    props.contents.LoggerNameOffset = 120
    props.contents.LogFileNameOffset = 120 + 4096
    return buf, props


def string_at(buf, offset):
    if offset == 0:
        return ''
    if offset < 120 or offset >= len(buf) or offset % 2:
        raise RuntimeError('invalid ETW output string offset')
    raw = buf.raw[offset:]
    return raw.decode('utf-16-le').split('\0', 1)[0]


class Backend:
    def __init__(self):
        require_admin()
        self.dll = ct.WinDLL('advapi32', use_last_error=True)
        self.dll.QueryAllTracesW.argtypes = [ct.POINTER(ct.POINTER(Properties)), w.ULONG,
                                            ct.POINTER(w.ULONG)]
        self.dll.QueryAllTracesW.restype = w.ULONG
        self.dll.ControlTraceW.argtypes = [ct.c_uint64, w.LPCWSTR,
                                           ct.POINTER(Properties), w.ULONG]
        self.dll.ControlTraceW.restype = w.ULONG

    def sessions(self):
        capacity = 64
        while capacity <= 1024:
            buffers = [make_buffer() for _ in range(capacity)]
            pointers = (ct.POINTER(Properties) * capacity)(*(p for _, p in buffers))
            count = w.ULONG()
            code = self.dll.QueryAllTracesW(pointers, capacity, ct.byref(count))
            if code == 234:
                capacity = max(capacity * 2, count.value)
                continue
            if code or count.value > capacity:
                raise OSError(code, 'ETW inventory unavailable/incomplete; no idle inference')
            return [dict(name=string_at(buf, ptr.contents.LoggerNameOffset),
                         log_file=string_at(buf, ptr.contents.LogFileNameOffset),
                         guid=bytes(ptr.contents.Wnode.Guid).hex(),
                         events_lost=ptr.contents.EventsLost,
                         buffers_written=ptr.contents.BuffersWritten)
                    for buf, ptr in buffers[:count.value]]
        raise RuntimeError('ETW inventory exceeds bounded capacity')

    def stop_exact(self, name):
        _, props = make_buffer()
        # Name selects the session; zero Guid avoids selecting a global kernel logger.
        return int(self.dll.ControlTraceW(0, name, props, 1))

    def query_exact(self, name):
        buf, props = make_buffer()
        code = int(self.dll.ControlTraceW(0, name, props, 0))
        if code == 4201:  # ERROR_WMI_INSTANCE_NOT_FOUND, not Access Denied.
            return None
        if code:
            raise OSError(code, 'exact ETW query failed; absence is not proved')
        return dict(name=name, log_file=string_at(buf, props.contents.LogFileNameOffset))

    def prove_absent(self, ownership):
        inventory = self.sessions()
        selected = ownership.select(inventory)
        exact = [row for name in sorted(ownership.names)
                 if (row := self.query_exact(name)) is not None]
        ownership.select(exact)  # Validate any nonempty path too.
        return {'sessions': inventory, 'owned': selected, 'exact': exact,
                'sessions_absent': not selected and not exact}


class Ownership:
    def __init__(self, instance, trace_dir):
        if not re.fullmatch(r'PD5F1Noise_[0-9a-f]{32}', instance):
            raise ValueError('invalid private run instance')
        self.instance = instance
        self.trace_dir = Path(trace_dir).resolve()
        self.names = {f'WPR_initiated_{instance}_{suffix}' for suffix in
                      ('WPR Event Collector', 'WPR System Collector')}

    def select(self, sessions):
        owned = []
        for row in sessions:
            if row['name'].casefold() in {name.casefold() for name in self.names} and row['name'] not in self.names:
                raise RuntimeError('owned-name case mismatch; exact identity is unresolved')
            if row['name'] not in self.names:
                continue
            # A nonempty logfile must corroborate the exact name, never widen ownership.
            if row.get('log_file') and Path(row['log_file']).resolve().parent != self.trace_dir:
                raise RuntimeError('exact run name has an unexpected logfile; manual recovery required')
            owned.append(row)
        return owned



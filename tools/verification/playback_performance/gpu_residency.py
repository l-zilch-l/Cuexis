"""Windows per-process GPU residency observation via English PDH counters."""
import ctypes as ct
import json
from pathlib import Path
import time


class Residency:
    def __init__(self, pid):
        from ctypes import wintypes as w
        self.pid, self.query = pid, ct.c_void_p()
        self.pdh = ct.WinDLL('pdh')
        self.pdh.PdhOpenQueryW.argtypes=[w.LPCWSTR,ct.c_size_t,ct.POINTER(ct.c_void_p)]
        self.pdh.PdhAddEnglishCounterW.argtypes=[ct.c_void_p,w.LPCWSTR,ct.c_size_t,ct.POINTER(ct.c_void_p)]
        self.pdh.PdhCollectQueryData.argtypes=[ct.c_void_p]
        self.pdh.PdhGetFormattedCounterArrayW.argtypes=[ct.c_void_p,w.DWORD,ct.POINTER(w.DWORD),ct.POINTER(w.DWORD),ct.c_void_p]
        self.pdh.PdhCloseQuery.argtypes=[ct.c_void_p]
        self.check(self.pdh.PdhOpenQueryW(None,0,ct.byref(self.query)))
        self.counters={}
        try:
            for name in ('Dedicated Usage','Shared Usage'):
                handle=ct.c_void_p()
                self.check(self.pdh.PdhAddEnglishCounterW(self.query,
                    '\\GPU Process Memory(*)\\'+name,0,ct.byref(handle)))
                self.counters[name]=handle
        except BaseException:
            self.close()
            raise

    @staticmethod
    def check(code):
        if code != 0: raise OSError('GPU PDH status 0x%08x'%(code & 0xffffffff))

    def sample(self):
        from ctypes import wintypes as w
        class Value(ct.Structure):
            _fields_=[('status',w.DWORD),('value',ct.c_int64)]
        class Item(ct.Structure):
            _fields_=[('name',w.LPWSTR),('value',Value)]
        self.check(self.pdh.PdhCollectQueryData(self.query))
        qpc, frequency = ct.c_int64(), ct.c_int64()
        kernel = ct.WinDLL('kernel32', use_last_error=True)
        if not kernel.QueryPerformanceCounter(ct.byref(qpc)) or not kernel.QueryPerformanceFrequency(ct.byref(frequency)):
            raise ct.WinError(ct.get_last_error())
        result={'pid':self.pid,'monotonic_ns':time.monotonic_ns(),
                'qpc':qpc.value,'qpc_frequency':frequency.value,'instances':{},'bytes':{}}
        for name,handle in self.counters.items():
            size,count=w.DWORD(),w.DWORD()
            code=self.pdh.PdhGetFormattedCounterArrayW(handle,0x400,ct.byref(size),ct.byref(count),None)
            if code & 0xffffffff != 0x800007d2: self.check(code)
            if not size.value:
                raise ValueError('GPU PDH returned no process instances')
            buffer=ct.create_string_buffer(size.value)
            self.check(self.pdh.PdhGetFormattedCounterArrayW(handle,0x400,ct.byref(size),ct.byref(count),buffer))
            items=ct.cast(buffer,ct.POINTER(Item))
            selected=[]
            for i in range(count.value):
                item=items[i]
                if item.name.startswith('pid_'+str(self.pid)+'_'):
                    if item.value.status not in (0,1) or item.value.value < 0:
                        raise ValueError('Invalid GPU process memory counter value')
                    selected.append({'instance':item.name,'bytes':item.value.value})
            result['instances'][name]=selected
            result['bytes'][name]=sum(x['bytes'] for x in selected) if selected else None
        return result

    def close(self):
        if self.query:
            self.pdh.PdhCloseQuery(self.query)
            self.query=ct.c_void_p()


def monitor(native, path):
    """Called only for a separate memory observer process, never normal timing."""
    path=Path(path)
    with path.open('x',encoding='utf-8') as stream:
        observer=None
        try:
            observer=Residency(native.pid)
            while native.poll() is None:
                try:
                    row=observer.sample()
                except Exception as error:
                    # The native process may not have a GPU instance before creating its
                    # context. Preserve that unavailable sample and keep observing later phases.
                    qpc, frequency = ct.c_int64(), ct.c_int64()
                    kernel = ct.WinDLL('kernel32')
                    kernel.QueryPerformanceCounter(ct.byref(qpc))
                    kernel.QueryPerformanceFrequency(ct.byref(frequency))
                    row={'pid':native.pid,'monotonic_ns':time.monotonic_ns(),
                         'qpc':qpc.value,'qpc_frequency':frequency.value,
                         'status':'unavailable','error':str(error)}
                stream.write(json.dumps({'schema':'playback.gpu-residency.v1',**row})+'\n')
                stream.flush()
                try: native.wait(timeout=.25)
                except Exception as error:
                    import subprocess
                    if not isinstance(error,subprocess.TimeoutExpired): raise
        except Exception as error:
            stream.write(json.dumps({'schema':'playback.gpu-residency.v1','status':'unavailable','error':str(error)})+'\n')
            # Unavailability is retained; a collector must not report zero residency.
            native.wait()
        finally:
            if observer:observer.close()

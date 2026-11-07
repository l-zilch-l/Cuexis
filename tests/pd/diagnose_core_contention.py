"""Bounded CPU-affinity diagnostic; never substitutes for a formal baseline."""
import argparse
import ctypes as ct
from ctypes import wintypes as wt
import json
from pathlib import Path
import statistics as st
import sys
import threading
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tests/pd'))
import formal_baseline as formal


class CounterValue(ct.Structure):
    _fields_ = [('status', wt.DWORD), ('value', ct.c_double)]


def qpc():
    value = ct.c_longlong()
    formal.base.checked(formal.base.K.QueryPerformanceCounter(ct.byref(value)))
    return value.value


class ProcessorCounters:
    """English PDH paths on localized Windows, one sample per second."""
    def __init__(self, count):
        self.api = ct.WinDLL('pdh')
        self.api.PdhOpenQueryW.argtypes = [wt.LPCWSTR, ct.c_size_t, ct.POINTER(wt.HANDLE)]
        self.api.PdhAddEnglishCounterW.argtypes = [wt.HANDLE, wt.LPCWSTR, ct.c_size_t,
                                                  ct.POINTER(wt.HANDLE)]
        self.api.PdhCollectQueryData.argtypes = [wt.HANDLE]
        self.api.PdhGetFormattedCounterValue.argtypes = [wt.HANDLE, wt.DWORD,
                                                        ct.POINTER(wt.DWORD),
                                                        ct.POINTER(CounterValue)]
        self.api.PdhCloseQuery.argtypes = [wt.HANDLE]
        self.query = wt.HANDLE()
        self.handles = {}
        self.checked(self.api.PdhOpenQueryW(None, 0, ct.byref(self.query)))
        try:
            for cpu in range(count):
                for name in ('% Processor Time', '% Processor Performance',
                             'Processor Frequency', '% DPC Time', '% Interrupt Time'):
                    path = f'\\Processor Information(0,{cpu})\\{name}'
                    handle = wt.HANDLE()
                    self.checked(self.api.PdhAddEnglishCounterW(self.query, path, 0,
                                                                ct.byref(handle)))
                    self.handles[path] = handle
            self.checked(self.api.PdhCollectQueryData(self.query))
            self.previous = qpc()
        except BaseException:
            self.close()
            raise

    @staticmethod
    def checked(status):
        if status:
            raise OSError(f'PDH error 0x{status & 0xffffffff:08x}')

    def collect(self):
        before = qpc()
        self.checked(self.api.PdhCollectQueryData(self.query))
        after = qpc()
        values = {}
        errors = {}
        for path, handle in self.handles.items():
            value = CounterValue()
            # NOCAP100 is necessary for boost/performance values above 100%.
            status = self.api.PdhGetFormattedCounterValue(handle, 0x200 | 0x8000,
                                                          None, ct.byref(value))
            if status or value.status not in (0, 1):
                errors[path] = {'api_status': status & 0xffffffff, 'data_status': value.status}
            else:
                values[path] = value.value
        result = {'utc': formal.utc(), 'previous_collect_end_qpc': self.previous,
                  'collect_start_qpc': before, 'collect_end_qpc': after,
                  'values': values, 'errors': errors}
        self.previous = after
        return result

    def close(self):
        if self.query:
            self.checked(self.api.PdhCloseQuery(self.query))
            self.query = wt.HANDLE()


def check():
    counters = ProcessorCounters(formal.os.cpu_count())
    try:
        time.sleep(1)
        row = counters.collect()
        print(json.dumps({'counter_count': len(row['values']), 'errors': row['errors'],
                          'core_2_3': {p: v for p, v in row['values'].items()
                                       if '(0,2)' in p or '(0,3)' in p}}, indent=2))
        if row['errors']:
            raise RuntimeError('PDH counter validation failed')
    finally:
        counters.close()


def run(args):
    source = formal.base.identity()
    binary = formal.binaries()
    env = formal.environment()
    topo = formal.topology(env['machine'])
    single = formal.select_registered_mask(topo)
    physical = next(mask for mask in topo['physical_core_masks'] if mask & single)
    if physical == single:
        raise ValueError('Distinct SMT sibling not available')
    if env['machine']['logical_processors'] > 64:
        raise ValueError('Diagnostic supports a single processor group only')
    calibration = formal.load(args.registration)
    if binary != calibration['binaries']:
        raise ValueError('Historical workload binary changed')
    order = [single, physical, physical, single, physical, single, single, physical]
    output = formal.evidence(args.output, 'core-diagnostic')
    manifest = {'revision': 'pd0-core-contention-diagnostic-v1', 'status': 'registered',
                'source': source, 'binaries': binary, 'environment_before': env, 'topology': topo,
                'runner_sha256': formal.base.sha(__file__), 'runs': [], 'pd0_exit': False,
                'registration': {'processes': 8, 'order': order, 'config': 'debug', 'profile': 'rpa',
                    'phase': 'formal', 'blocks_per_metric': 8, 'machine_warmup_seconds': 30,
                    'reads': calibration['read_counts']['debug/rpa'], 'etw': False,
                    'pdh_interval_seconds': 1, 'no_filter_or_retry': True,
                    'question': 'Compare one logical CPU with its full physical-core allowed mask; observe sibling activity and reported performance.',
                    'limitation': 'Affinity does not reserve CPUs; PDH is coarse system telemetry with uncalibrated overhead. No baseline acceptance or GHz inference from thread cycles.'}}
    formal.save(output / 'manifest.json', manifest)
    counters = ProcessorCounters(env['machine']['logical_processors'])
    stop = threading.Event()
    failures = []
    def monitor():
        try:
            with (output / 'processor-counters.jsonl').open('x', encoding='utf-8') as stream:
                while not stop.wait(1):
                    row = counters.collect()
                    stream.write(json.dumps(row) + '\n')
                    stream.flush()
                    if row['errors']:
                        raise RuntimeError('PDH counter data invalid; see retained telemetry')
        except BaseException as error:
            failures.append(str(error))
        finally:
            counters.close()
    worker = threading.Thread(target=monitor)
    worker.start()
    try:
        formal.no_trace()
        formal.child(output, 'machine-warmup', binary['release']['binaries']['baseline']['path'],
                     single, warm=True)
        for ordinal, mask in enumerate(order, 1):
            if failures:
                raise RuntimeError(failures[0])
            formal.same_environment(env, formal.environment())
            formal.no_trace()
            data = formal.child(output, f'debug-rpa-{ordinal}-mask-{mask}',
                                binary['debug']['binaries']['baseline']['path'], mask, 'rpa',
                                reads=manifest['registration']['reads'])
            manifest['runs'].append({'kind': 'single_owner', 'window': 0, 'mask': mask,
                                      'config': 'debug', 'profile': 'rpa', 'data': data})
            formal.same_environment(env, formal.environment())
            formal.save(output / 'manifest.json', manifest)
        formal.validate_runs(manifest['runs'])
        results = []
        for mask in (single, physical):
            for metric in formal.OWNER_METRICS:
                rows = [r for r in manifest['runs'] if r['mask'] == mask]
                values = [st.median(x['sample']['wall_us'] / x['operations']
                                    for x in r['data']['samples'] if x['kind'] == metric) for r in rows]
                results.append({'mask': mask, 'metric': metric, 'process_medians_us': values,
                                'noise': formal.noise(values)})
        manifest.update(status='diagnostic_complete', results=results)
    except BaseException as error:
        manifest.update(status='stopped_error', error=str(error))
        raise
    finally:
        stop.set()
        worker.join()
        manifest.update(source_after=formal.base.identity(), environment_after=formal.environment(),
                        finished_utc=formal.utc(), telemetry_errors=failures)
        if failures or binary != formal.binaries() or source != manifest['source_after']:
            manifest.update(status='stopped_error', validation_error='Telemetry or input identity changed')
        try:
            formal.same_environment(env, manifest['environment_after'])
        except ValueError as error:
            manifest.update(status='stopped_error', validation_error=str(error))
        formal.save(output / 'manifest.json', manifest)
        formal.save(output / 'evidence-hashes.json', {p.name: formal.base.sha(p)
                    for p in output.iterdir() if p.is_file() and p.name != 'evidence-hashes.json'})
    print('STATUS ' + manifest['status'], flush=True)
    return int(manifest['status'] != 'diagnostic_complete')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['check', 'run'])
    parser.add_argument('--registration', type=Path)
    parser.add_argument('--output', type=Path, default=ROOT / 'out/pd0-core-diagnostic')
    args = parser.parse_args()
    if args.action == 'run' and not args.registration:
        parser.error('run requires --registration')
    if args.action == 'check':
        check()
    else:
        raise SystemExit(run(args))


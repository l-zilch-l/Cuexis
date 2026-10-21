"""Run a frozen, finite diagnostic plan; never registers or resumes formal windows."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import statistics as st
import subprocess
import sys
import time

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[3]))
from tools.verification.playback_performance import diagnostic as d, measurement as m
from tools.verification.playback_performance import process_control as control
from tools.verification.playback_performance.historical_integrity import sha

ROOT = Path(__file__).resolve().parents[3]
UNIT = 'PD finite read and update scaling'
HOST = 'PD host delivery and receipt acknowledgement baseline'
CAPACITY = 'PD5 full content Snapshot Seek Replay boundary'
COUNTS = '1 2097152 262144 8 1048576 131072 64 262144 32768 256 65536 8192'


def save(path, data):
    with path.open('x', encoding='utf-8') as stream:
        json.dump(data, stream, indent=2)
        stream.write('\n')


def source_identity(root):
    # Product/build/harness bytes, including the new harness. Excludes docs and output artifacts.
    files = {}
    for base in ('engine', 'app', 'cmake', 'tests', 'schemas', 'assets'):
        for path in sorted((root / base).rglob('*')):
            if path.is_file() and path.suffix not in ('.pyc',):
                files[path.relative_to(root).as_posix()] = sha(path)
    for path in sorted(root.glob('*')):
        if path.is_file() and (path.name.startswith(('CMake', 'vcpkg')) or path.suffix == '.cmake'):
            files[path.name] = sha(path)
    return {'files': files, 'sha256': hashlib.sha256(json.dumps(files, sort_keys=True).encode()).hexdigest()}


def freeze(args):
    folder = args.output.resolve()
    folder.mkdir(exist_ok=True, parents=True)
    builds = {}
    for side, source, build in (('A', args.baseline_source.resolve(), args.baseline_build.resolve()),
                                ('B', ROOT, args.current_build.resolve())):
        bindings = {}
        for kind in ('baseline', 'host'):
            exe = build / 'bin' / ('cuexis_pd_' + kind + '_probe.exe')
            bindings[kind] = {'path': str(exe), 'sha256': sha(exe)}
        metadata = {str(p): sha(p) for p in (build/'CMakeCache.txt', build/'compile_commands.json', build/'build.ninja')}
        metadata.update({str(p): sha(p) for p in sorted((build/'lib').glob('*.lib'))})
        builds[side] = {'source': str(source), 'source_identity': source_identity(source),
                        'bindings': bindings, 'build_files': metadata}
    tools = {str(p): sha(p) for p in Path(__file__).parent.glob('*.py')}
    plan = {'schema': 'pd5.finite-followup-plan.v1', 'formal': False, 'created_unix': time.time(),
            'head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
            'method': d.METHOD, 'protocol': d.PROTOCOL, 'builds': builds, 'tools': tools,
            'counts': COUNTS, 'blocks': 3, 'pairs': 3, 'order': [['A', 'B'], ['B', 'A'], ['A', 'B']],
            'scale_mask': 4, 'host_mask': control.affinity(), 'warmup_seconds': 30,
            'host': {'warmup': 32, 'measured': 1024, 'frame_capacity': 2048},
            'production': {'entities': 40000, 'requirements': 40000, 'reuse': ['low', 'high'],
                           'processes_per_reuse': 3, 'side': 'B', 'acceptance': False},
            'stop_rule': 'Execution/identity/output/short-block error stops that unit; no replacement. Noise retains all fixed samples.',
            'quantiles': 'Only per-operation host samples; read/update blocks are averages.',
            'limits': {'regression': m.REGRESSION_LIMIT, 'noise': m.NOISE_LIMIT}}
    save(folder / 'plan.json', plan)
    print('Frozen plan:', folder/'plan.json', sha(folder/'plan.json'))


def verify(plan):
    for path, expected in plan['tools'].items():
        if sha(Path(path)) != expected:
            raise ValueError('Diagnostic tool changed: ' + path)
    for build in plan['builds'].values():
        if source_identity(Path(build['source'])) != build['source_identity']:
            raise ValueError('Diagnostic source changed')
        for path, expected in build['build_files'].items():
            if sha(Path(path)) != expected:
                raise ValueError('Diagnostic build input changed: ' + path)
        for binding in build['bindings'].values():
            if sha(Path(binding['path'])) != binding['sha256']:
                raise ValueError('Diagnostic binary changed')


def launch(folder, plan, side, kind, index, extra=None):
    name = f'{kind}-{index}-{side}'
    path = folder / (name + '.log')
    binding = plan['builds'][side]['bindings']
    env = {k: v for k, v in os.environ.items() if not k.startswith(('CUEXIS_PD', 'CUEXIS_FOLLOW'))}
    env.update(CUEXIS_FOLLOW_COUNTS=plan['counts'], CUEXIS_PD_HOST_DIAGNOSTIC_1024='1')
    if extra:
        env.update(extra)
    mask = plan['scale_mask'] if kind == 'scale' else plan['host_mask']
    unit = {'scale': UNIT, 'host': HOST, 'capacity-low': CAPACITY, 'capacity-high': CAPACITY}[kind]
    exe = binding['baseline' if kind == 'scale' else 'host']['path']
    record = {'name': name, 'side': side, 'kind': kind, 'index': index,
              'start_unix': time.time(), 'log': str(path), 'mask': mask,
              'binary_sha256': sha(Path(exe)), 'unit': unit, 'exit_code': None}
    with path.open('x', encoding='utf-8') as output:
        try:
            result = control.run([binding['baseline']['path'], 'PD machine CPU warmup', '--reporter', 'compact'],
                                 mask, cwd=str(ROOT), env=env, output=output, timeout=120)
            result.check_returncode()
            result = control.run([exe, unit, '--rng-seed', '1', '--reporter', 'compact'], mask,
                                 cwd=str(ROOT), env=env, output=output, timeout=900)
            record['exit_code'] = result.returncode
            result.check_returncode()
        finally:
            record['end_unix'] = time.time()
            output.flush()
            record['log_sha256'] = sha(path)
            save(folder/(name+'.process.json'), record)
    print('completed', name, flush=True)
    return record


def scale_records(record, plan):
    if record['exit_code'] != 0 or sha(Path(record['log'])) != record['log_sha256']:
        raise ValueError('Failed or modified scale process')
    text = Path(record['log']).read_text(encoding='utf-8')
    rows = m.records(text, 'PD_FOLLOW_ROW ')
    contents = {r['objects']: r['sha256'] for r in m.records(text, 'PD_FOLLOW_CONTENT ')}
    if len(rows) != 24 or len(contents) != 4:
        raise ValueError('Missing scale configuration/block')
    pid = m.records(text, 'PD5_PROCESS_START ')[-1]['pid']
    result = {}
    for n in (1, 8, 64, 256):
        for mode in ('read', 'updated'):
            selected = [r for r in rows if r['objects'] == n and r['mode'] == mode]
            if [r['block'] for r in selected] != [0, 1, 2]:
                raise ValueError('Missing/duplicate block')
            operations = selected[0]['operations']
            counts = [int(x) for x in plan['counts'].split()]
            expected = {counts[i]: counts[i+1 if mode == 'read' else i+2]
                        for i in range(0, len(counts), 3)}
            if operations != expected[n]:
                raise ValueError('Frozen operation count mismatch')
            if any(r['operations'] != operations for r in selected):
                raise ValueError('Operation count changed')
            total = operations * 3
            sequence = dict(mode=mode, objects=n, operations=operations, blocks=3, warmup=64,
                            time='10 + ordinal*0.01 ms', delta_ms=.01)
            process = dict(schema='playback.diagnostic-process.v1', method=d.METHOD, protocol=d.PROTOCOL,
                source_sha256=plan['builds'][record['side']]['source_identity']['sha256'],
                binary_sha256=record['binary_sha256'], run_id=record['name'], pid=pid,
                output_validated=record['exit_code'] == 0,
                workload=dict(objects=n, cameras=1, registry_entities=n+1, U=64+(total if mode=='updated' else 0),
                              F=64+total, R=0, content_sha256=contents[n],
                              sequence_sha256=hashlib.sha256(json.dumps(sequence,sort_keys=True).encode()).hexdigest(),
                              viewport=[1280,720], output_digest=[str(r['digest']) for r in selected]),
                metric=dict(name=mode, start='update-start' if mode=='updated' else 'extract-start',
                            end='snapshot-ready', unit='us', sample_kind='block_average'), samples=selected)
            d.validate(process, {k: process[k] for k in ('source_sha256', 'binary_sha256')})
            result[f'{n}-{mode}'] = process
    return result


def analyze(folder, plan):
    result = {'acceptance': False, 'plan_sha256': sha(folder/'plan.json'), 'scale': {}, 'host': {}}
    scale = {side: [scale_records(json.loads((folder/f'scale-{i}-{side}.process.json').read_text(encoding='utf-8-sig')), plan)
                    for i in range(3)] for side in ('A','B')}
    ids = {side: dict(source_sha256=plan['builds'][side]['source_identity']['sha256'],
                     binary_sha256=plan['builds'][side]['bindings']['baseline']['sha256']) for side in ('A','B')}
    for key in scale['A'][0]:
        data = {'identities': ids, 'pairs': [[scale['A'][i][key], scale['B'][i][key]] for i in range(3)]}
        save(folder/(key+'.input.json'),data)
        result['scale'][key] = d.compare(data['pairs'],ids)
    hosts = {side: [host_record(json.loads((folder/f'host-{i}-{side}.process.json').read_text(encoding='utf-8-sig')),plan)
                    for i in range(3)] for side in ('A','B')}
    pairs = []
    for i in range(3):
        m.require_comparable(hosts['A'][i],hosts['B'][i],'request_to_ready_us')
        pairs.append(tuple(hosts[s][i]['statistics']['request_to_ready_us']['median_us'] for s in ('A','B')))
    spreads = {s: m.noise([p[j] for p in pairs]) for j,s in enumerate(('A','B'))}
    effect = m.paired_interval(pairs)
    effect['status'] = 'inconclusive' if max(spreads.values()) > m.NOISE_LIMIT else 'diagnostic_'+effect['status']
    result['host'] = {'effect': effect, 'noise': spreads, 'process_pairs': pairs,
                      'statistics_by_endpoint': {s: [h['statistics'] for h in hosts[s]] for s in hosts},
                      'completion_comparable': False,
                      'unavailable': ['receipt-enqueue', 'receipt-drain', 'GPU-complete']}
    save(folder/'analysis.json',result)
    return result


def host_record(record, plan):
    if record['exit_code'] != 0 or sha(Path(record['log'])) != record['log_sha256']:
        raise ValueError('Failed or modified host process')
    parsed = m.parse_host(Path(record['log']).read_text(encoding='utf-8-sig'))
    identity = parsed['identity']
    expected = dict(plan['host'], objects=2, frames_published=1056, successful_receipts=1056)
    if any(identity.get(k) != v for k,v in expected.items()):
        raise ValueError('Host run does not match frozen counts/capacity')
    return parsed


def run(args):
    folder = args.output.resolve()
    plan = json.loads((folder/'plan.json').read_text(encoding='utf-8-sig'))
    verify(plan)
    save(folder/'run.claim.json', {'plan_sha256':sha(folder/'plan.json'),'started_unix':time.time()})
    failures = []
    for kind in ('scale','host'):
        try:
            for index, order in enumerate(plan['order']):
                pair = {}
                for side in order:
                    record = launch(folder, plan, side, kind, index)
                    if kind == 'scale': pair[side] = scale_records(record,plan)
                    else: pair[side] = host_record(record,plan)
                if kind == 'scale':
                    if any(pair['A'][key]['workload'] != pair['B'][key]['workload'] for key in pair['A']):
                        raise ValueError('A/B output or logical work mismatch')
                else:
                    m.require_comparable(pair['A'],pair['B'],'request_to_ready_us')
        except Exception as error:
            failures.append({'unit':kind,'error':str(error)})
            print('STOP',kind,str(error),flush=True)
    for reuse in ('low','high'):
        try:
            for index in range(3):
                record = launch(folder,plan,'B','capacity-'+reuse,index,
                                {'CUEXIS_PD5_ENTITIES':'40000','CUEXIS_PD5_REUSE':reuse})
                rows = m.records(Path(record['log']).read_text(encoding='utf-8-sig'), 'PD5_CAPACITY ')
                if len(rows) != 1 or any(rows[0].get(k) != v for k,v in
                    dict(entities=40000, requirements=40000, reuse=reuse, replay_equal=True,
                         snapshot_valid=True, closed=True, formal_acceptance=False).items()):
                    raise ValueError('Incomplete production correctness record')
        except Exception as error:
            failures.append({'unit':'capacity-'+reuse,'error':str(error)})
            print('STOP capacity',reuse,str(error),flush=True)
    if not any(f['unit'] in ('scale','host') for f in failures):
        analyze(folder,plan)
    save(folder/'run-result.json', {'failures':failures,'acceptance':False,'ended_unix':time.time()})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=['freeze','run'])
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--baseline-source',type=Path)
    parser.add_argument('--baseline-build',type=Path)
    parser.add_argument('--current-build',type=Path)
    args = parser.parse_args()
    {'freeze':freeze,'run':run}[args.command](args)


if __name__ == '__main__':
    main()

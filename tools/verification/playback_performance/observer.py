"""Separate stage/allocation observer checks; never converts instrumented timings into acceptance."""
import argparse
import json
import os
from pathlib import Path
import sys
import time
import uuid

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[3]))
from tools.verification.playback_performance import measurement as measure
from tools.verification.playback_performance import prepare_profiles as prepare
from tools.verification.playback_performance import process_control as process
from tools.verification.playback_performance import profile_runner as runner


def stages(paths, required):
    result = {}
    for path in paths:
        for line in Path(path).read_text(encoding='utf-8-sig').splitlines():
            row = json.loads(line)
            if row.get('omitted') != 0:
                raise ValueError('Stage observer omitted observations')
            inclusive, exclusive, correlations = (row.get(k, []) for k in
                ('inclusive_us','exclusive_us','correlations'))
            if not inclusive or len(inclusive) != len(exclusive) or len(inclusive) != len(correlations):
                raise ValueError('Incomplete observer arrays')
            for total, own in zip(inclusive,exclusive):
                measure.positive(total,True); measure.positive(own,True)
                if own > total + .001:
                    raise ValueError('Exclusive stage cost exceeds inclusive boundary')
            bucket = result.setdefault(row['stage'], {'inclusive_us':[], 'exclusive_us':[],
                'items':0,'logical_copy_bytes':0,'files':[]})
            bucket['inclusive_us'].extend(inclusive)
            bucket['exclusive_us'].extend(exclusive)
            bucket['items'] += row['items']
            bucket['logical_copy_bytes'] += row['logical_copy_bytes']
            bucket['files'].append({'path':str(path),'sha256':prepare.sha(path)})
    for name, count in required.items():
        actual = len(result.get(name,{}).get('inclusive_us',[]))
        if actual != count if count >= 0 else actual == 0:
            raise ValueError('Observer coverage mismatch: '+name+'='+str(actual))
    return {name:{**row,'inclusive':measure.statistics(row['inclusive_us']),
        'exclusive':measure.statistics(row['exclusive_us'])} for name,row in result.items()}


def memory(text, *, gpu=False, age_limit_us=900000000, structure_limit=1048576):
    rows = measure.records(text,'PD5_MEMORY ')
    labels = [x.get('label') for x in rows]
    if labels.count('before-load') != 1 or labels.count('after-close') != 1:
        raise ValueError('Missing allocation lifecycle checkpoints')
    previous = [0,0,0]
    for row in rows:
        if row.get('schema') != 'pd5.memory.v1' or row.get('exceeded') is not False or row.get('exact_partition') is not False or row.get('bound_kind') != 'global_cpp_new_envelope':
            raise ValueError('Allocation envelope incomplete or exceeded')
        for value, peak, limit in (
            ('live_count','peak_live_count','limit_count'),
            ('requested_cpp_bytes','peak_requested_cpp_bytes','limit_requested_cpp_bytes'),
            ('crt_usable_storage_bytes','peak_crt_usable_storage_bytes','limit_crt_storage_bytes')):
            if not 0 <= row[value] <= row[peak] <= row[limit] or row[limit] <= 0:
                raise ValueError('Allocation count/bytes exceed declared finite bounds')
        peaks = [row[k] for k in ('peak_live_count','peak_requested_cpp_bytes','peak_crt_usable_storage_bytes')]
        if any(a < b for a,b in zip(peaks,previous)):
            raise ValueError('Allocation peak lost a transient high-water mark')
        previous = peaks
        if row['oldest_live_age_us'] > age_limit_us:
            raise ValueError('Allocation lifetime exceeds process envelope')
    structures = measure.records(text,'PD5_STRUCTURE ')
    if not structures:
        raise ValueError('No structural storage observations')
    for row in structures:
        if not 0 <= row['peak_count'] <= row['peak_capacity'] <= structure_limit:
            raise ValueError('Structural count/capacity exceeds finite profile')
    gpu_rows = measure.records(text,'PD5_GPU_MEMORY ')
    if gpu:
        if not gpu_rows or gpu_rows[-1].get('label') != 'after-device-release' or not gpu_rows[-1].get('gpu_drained'):
            raise ValueError('Missing GPU completion and device release boundary')
        for row in gpu_rows:
            if row.get('exceeded') is not False or not 0 <= row['live_count'] <= row['peak_count'] <= row['limit_count'] or not 0 <= row['logical_storage_bytes'] <= row['peak_logical_storage_bytes'] <= row['limit_logical_storage_bytes']:
                raise ValueError('GPU logical storage envelope exceeded')
        if gpu_rows[-1]['live_count'] or gpu_rows[-1]['logical_storage_bytes']:
            raise ValueError('GPU storage still live after drain/release')
    return {'schema':'playback.memory-observation.v1','checkpoints':rows,'structures':structures,
        'gpu_logical':gpu_rows,'age_limit_us':age_limit_us,'structure_limit':structure_limit,
        'domains_share_cpp_envelope':['World','Runtime current/candidate','scratch','projection/Snapshot indexes'],
        'exact_partition':False,'overlap_group':'process_cpp_new','do_not_sum_domains':True,
        'uncovered':['direct malloc','SDL/OS heaps','shader/driver/framebuffer GPU storage'],
        'acceptance':False,'status':'observer_plumbing_verified'}


def identity(data):
    return {k:v for k,v in data['identity'].items() if k not in
        ('allocator','metrics_compile_enabled','payload_peak_count','payload_peak_bytes','journal_peak_bytes')}


def check(args):
    suite = prepare.load(prepare.SUITE)
    profile = prepare.expanded_profile(suite,next(p for p in suite['profiles'] if p['id']==args.profile))
    if profile['gpu']:
        raise ValueError('GPU observer requires explicit foreground device run; use collect for device logs')
    target = 'cuexis_pd_host_probe.exe'
    matrix = profile.get('harness') == 'presentation-matrix'
    source = 'tests/pd/pd5_matrix_probe.cpp' if matrix else 'tests/pd/pd5_host_probe.cpp'
    bindings = {kind:prepare.build_binding(build,target,[source],observer=kind) for kind,build in
        [('none',args.normal_build),('stages',args.stage_build),('memory',args.memory_build)]}
    folder=args.output/('observer-check-'+uuid.uuid4().hex[:12]); folder.mkdir(parents=True)
    prepare.save_new(folder/'inputs.json',{'source':prepare.inventory(),'profile':profile,'bindings':bindings,'acceptance':False})
    runs=[]
    reference=None
    # Fixed three calibration pairs. These are short instrumented tool checks, not pilots.
    for repetition in range(3):
        for kind in ('none','stages','memory'):
            name=kind+'-'+str(repetition)
            env=runner.probe_environment(profile,smoke=True)
            env.update(CUEXIS_PD5_FRAMES='8',CUEXIS_PD5_WARMUP='4',CUEXIS_PD5_CYCLES='1')
            if kind=='stages': env['CUEXIS_PD_METRICS']=str(folder/name)
            path=folder/(name+'.log')
            command=runner.command({'profile':profile,'build':bindings[kind]})
            started=time.perf_counter_ns()
            try:
                with path.open('xb') as stream:
                    result=process.run(command,process.affinity(),cwd=prepare.ROOT,env=env,output=stream,timeout=120)
            except Exception as error:
                prepare.save_new(folder/(name+'-process.json'),{'command':command,'error':str(error),'log':str(path),'acceptance':False})
                raise
            elapsed=(time.perf_counter_ns()-started)/1000
            text=path.read_text(encoding='utf-8-sig')
            data=measure.parse_probe(text,result.returncode,instrumented=kind=='stages',memory_observer=kind=='memory')
            if data['identity']['counts'] != prepare.expected_counts(profile,smoke=True):
                raise ValueError('Observer trajectory/count mismatch')
            if reference is None: reference=identity(data)
            if identity(data)!=reference: raise ValueError('Observer changes exact workload/output')
            row={'kind':kind,'pair':repetition,'wall_us':elapsed,'raw_log':str(path),'raw_sha256':prepare.sha(path),
                'data':data,'acceptance':False}
            if kind=='stages':
                required={'snapshot':8*profile['capture_consumers']} if matrix else {'judgement':8,'projection':8,'snapshot':8,'queue':-1}
                row['stage_observation']=stages(sorted(folder.glob(name+'.*.jsonl')),required)
            if kind=='memory': row['memory_observation']=memory(text)
            prepare.save_new(folder/(name+'.json'),row); runs.append(row)
    costs={kind:[x['wall_us'] for x in runs if x['kind']==kind] for kind in bindings}
    ratios={kind:[b/a for a,b in zip(costs['none'],costs[kind])] for kind in ('stages','memory')}
    prepare.save_new(folder/'summary.json',{'schema':'playback.observer-check.v1','runs':[
        {'path':str(folder/(x['kind']+'-'+str(x['pair'])+'.json'))} for x in runs],
        'process_wall_us':costs,'process_overhead_ratios':ratios,
        'coverage_verified':True,'exact_output_equal':True,'acceptance':False,
        'budget_observer_validated':False,
        'limitation':'Short startup-dominated calibration reports overhead, not a correction bound for uninstrumented stage budgets'})
    print(folder/'summary.json')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    sub=parser.add_subparsers(dest='action',required=True)
    p=sub.add_parser('check')
    p.add_argument('--profile',default='cpu-stream-v1')
    p.add_argument('--normal-build',type=Path,default=prepare.ROOT/'out/build/pd5-ready-release')
    p.add_argument('--stage-build',type=Path,default=prepare.ROOT/'out/build/pd5-ready-observer')
    p.add_argument('--memory-build',type=Path,default=prepare.ROOT/'out/build/pd5-ready-memory')
    p.add_argument('--output',type=Path,required=True)
    p=sub.add_parser('collect-memory');p.add_argument('--log',type=Path,required=True)
    p.add_argument('--gpu',action='store_true');p.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    if args.action=='check':check(args)
    else:prepare.save_new(args.output,memory(args.log.read_text(encoding='utf-8-sig'),gpu=args.gpu))


if __name__=='__main__':main()

"""Bounded uninstrumented pilot and two-window formal baseline; no ETW capture."""
import argparse
import ctypes as ct
import datetime as dt
import json
import math
import os
from pathlib import Path
import re
import statistics as st
import subprocess as sp
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.verification.playback_performance import baseline_method as base
from system_runtime_support import power_policy, require_same_power, validate_alignment
from collect_host_delivery import invoke as invoke_host

COMBINATIONS = [('release','legacy'),('debug','rpa'),('release','rpa'),('debug','legacy')]
OWNER_METRICS = ('validated_trace', 'playback_only', 'read_long')
FIELDS = ['request_to_ready_us','ready_to_receipt_ack_us','request_to_receipt_ack_us','present_call_to_ack_us']
ACTIVE_OUTPUT = None

def utc(): return dt.datetime.now(dt.timezone.utc).isoformat()
def load(path): return json.loads(Path(path).read_text(encoding='utf-8'))
def save(path,obj): base.save(Path(path),obj)
def noise(values): return (max(values)-min(values))/st.median(values)

def measurement_identity_matches(registered, current):
    """Keep the registered measurement inputs stable while allowing runner fixes."""
    excluded = {'tests/pd/formal_baseline.py'}
    if registered.get('head') != current.get('head'):
        return False
    registered_files = {p: digest for p, digest in registered.get('files', {}).items()
                        if p not in excluded}
    current_files = {p: digest for p, digest in current.get('files', {}).items()
                     if p not in excluded}
    return registered_files == current_files
def evidence(output,label):
    global ACTIVE_OUTPUT
    p=output/(label+'-'+dt.datetime.now(dt.timezone.utc).strftime('%Y%m%dT%H%M%SZ')+'-'+uuid.uuid4().hex[:6])
    p.mkdir(parents=True);ACTIVE_OUTPUT=p;print('EVIDENCE '+str(p),flush=True);return p

def validate_dependency_inventory(text, object_path, source_path, build):
    counts = re.findall(r'#deps\s+(\d+)', text)
    if len(counts) != 1 or int(counts[0]) == 0:
        raise ValueError('Missing Ninja header dependency inventory; clean reconfigure/build required')
    dependencies = [Path(line.strip()) for line in text.splitlines()[1:] if line.strip()]
    if len(dependencies) != int(counts[0]):
        raise ValueError('Incomplete Ninja dependency inventory')
    dependencies = [p if p.is_absolute() else build / p for p in dependencies]
    required = (ROOT / 'tests/pd/measurement_environment.hpp').resolve()
    if required not in [p.resolve() for p in dependencies]:
        raise ValueError('Probe does not track measurement_environment.hpp')
    newest = max(p.stat().st_mtime_ns for p in [source_path, *dependencies])
    compiled = object_path.stat().st_mtime_ns
    if newest > compiled:
        raise ValueError('Probe object is older than a registered source/header input')
    return {'dependencies': int(counts[0]), 'inventory_sha256': base.hashlib.sha256(text.encode()).hexdigest(),
            'object_mtime_ns': compiled, 'newest_input_mtime_ns': newest}

def binaries():
    result={}
    for config in ('release','debug'):
        build=ROOT/'out/build'/('pd0-formal-'+config)
        cache=build/'CMakeCache.txt'; text=cache.read_text(encoding='utf-8')
        for required in ['CUEXIS_PD_METRICS_TEST_ONLY:BOOL=OFF','CUEXIS_ENABLE_CHART_V5_CANDIDATE:BOOL=ON',
                         'CUEXIS_LIBRARY_TYPE:STRING=STATIC','CMAKE_BUILD_TYPE:STRING='+config.title()]:
            if required not in text: raise ValueError('Required build setting missing: '+required)
        commands=load(build/'compile_commands.json')
        if any('CUEXIS_PD_METRICS_TEST_ONLY' in r.get('command','') for r in commands):
            raise ValueError('Stage instrumentation remains in compiler commands')
        ninja = re.search(r'^CMAKE_MAKE_PROGRAM:FILEPATH=(.+)$', text, re.MULTILINE)
        if not ninja:
            raise ValueError('Ninja build metadata required')
        dependencies = {}
        for name, source in (('baseline', 'measurement_method_tests.cpp'), ('host', 'host_delivery_probe.cpp')):
            object_file = Path('tests/pd/CMakeFiles') / ('cuexis_pd_' + name + '_probe.dir') / (source + '.obj')
            queried = sp.run([ninja[1], '-C', str(build), '-t', 'deps', object_file.as_posix()],
                             stdout=sp.PIPE, stderr=sp.STDOUT, check=True, timeout=30)
            inventory = queried.stdout.decode('utf-8', errors='strict')
            dependencies[name] = validate_dependency_inventory(inventory, build / object_file,
                                                                ROOT / 'tests/pd' / source, build)
            if (build / 'bin' / ('cuexis_pd_' + name + '_probe.exe')).stat().st_mtime_ns < (build / object_file).stat().st_mtime_ns:
                raise ValueError('Probe executable predates its measurement object')
        result[config]={'cache_sha256':base.sha(cache),'compile_commands_sha256':base.sha(build/'compile_commands.json'),
                        'dependency_validation': dependencies,
                        'binaries':{name:{'path':str(build/'bin'/('cuexis_pd_'+name+'_probe.exe')),
                            'sha256':base.sha(build/'bin'/('cuexis_pd_'+name+'_probe.exe'))} for name in ('baseline','host')}}
    return result

def environment():
    machine=base.machine()
    user=ct.WinDLL('user32',use_last_error=True)
    user.GetForegroundWindow.restype=ct.c_void_p
    user.GetWindowThreadProcessId.argtypes=[ct.c_void_p,ct.POINTER(ct.c_ulong)]
    foreground=ct.c_ulong()
    user.GetWindowThreadProcessId(user.GetForegroundWindow(),ct.byref(foreground))
    return {'machine':machine,'power':power_policy(machine['ac_line_status']),
            'foreground_pid':foreground.value,'parent_pid':os.getpid()}

def same_environment(expected,actual):
    require_same_power(expected['power'],actual['power'])
    for key in ['os','logical_processors','parent_affinity','ac_line_status']:
        if expected['machine'][key]!=actual['machine'][key]:raise ValueError('Environment changed: '+key)
    if actual['machine']['ac_line_status']!=1:raise ValueError('AC line required')

def no_trace():
    p=sp.run(['wpr','-status'],stdout=sp.PIPE,stderr=sp.STDOUT,timeout=15)
    if b'WPR is not recording' not in p.stdout: raise ValueError('WPR active or unknown; formal sampling refused')
    return {'checked_utc':utc(),'output':p.stdout.decode('utf-8',errors='replace')}

def topology(machine):
    size=ct.c_ulong(); k=ct.WinDLL('kernel32',use_last_error=True)
    k.GetLogicalProcessorInformation(None,ct.byref(size)); buffer=ct.create_string_buffer(size.value)
    if not k.GetLogicalProcessorInformation(buffer,ct.byref(size)):raise ct.WinError(ct.get_last_error())
    if ct.sizeof(ct.c_void_p)!=8 or size.value%32: raise ValueError('Unsupported topology ABI')
    cores=[int.from_bytes(buffer.raw[i:i+8],'little') & machine['parent_affinity'] for i in range(0,size.value,32)
           if int.from_bytes(buffer.raw[i+8:i+12],'little')==0]
    cores=[c for c in cores if c]
    first=machine['parent_affinity'] & -machine['parent_affinity']
    other=next((c & -c for c in cores if not c & first), first)
    # CPU 0 remains a diagnostic control, but is a poor default benchmark
    # core on this host because system interrupt activity is concentrated there.
    # The selected core is fixed from topology before formal samples are read;
    # pilot noise cannot reorder this list or choose a favorable result.
    candidates=[other]
    if first not in candidates:
        candidates.append(first)
    return {'physical_core_masks':cores,'candidate_masks':candidates}

def native_environment(lines):
    records=[json.loads(s[len('PD_ENV '):]) for s in lines if s.startswith('PD_ENV ')]
    # Two anchors must bracket the measured region. A probe may prefix them to mark its own region
    # ("seek-begin"/"seek-end"); the prefix has to match on both sides.
    labels=[r.get('label','') for r in records]
    bracketed=(len(records)==2 and labels[0].endswith('begin') and labels[1].endswith('end')
               and labels[0][:-len('begin')]==labels[1][:-len('end')])
    if not bracketed:raise ValueError('Missing native environment')
    # Processor placement is an observation, not a frozen condition. Process
    # affinity and scheduling/power controls remain part of the invariant.
    keys=[k for k in records[0] if k not in ('label','pid','tid','processor_group','processor_number')]
    if any(records[0][k]!=records[1][k] for k in keys):raise ValueError('Priority or explicit throttling changed')
    if not records[0]['process_priority']:raise ValueError('Could not query process priority')
    return records

def validate_runs(runs):
    references={}
    native_common=None
    native_by_affinity={}
    scheduling_keys = ('process_priority', 'thread_priority', 'process_power_error',
                       'process_power_control', 'process_power_state', 'thread_power_error',
                       'thread_power_control', 'thread_power_state')
    for run in runs:
        data=run.get('data',run)
        identity=data['identity']
        profile=identity.get('profile','host')
        keys=['digest','replay_sha256','snapshot_sha256','packed_sha256'] if profile!='host' else [
            'digest','warmup','measured','frame_capacity','frames_published','successful_receipts','source','viewport','objects','audio','gpu','closed']
        signature={key:identity[key] for key in keys}
        if profile in references and signature!=references[profile]:raise ValueError('Cross-process exact identity mismatch')
        references[profile]=signature
        record=data['environment'][0]
        controlled={key:record.get(key) for key in scheduling_keys}
        common=controlled
        if native_common is not None and common!=native_common:
            raise ValueError('Cross-process explicit scheduling state changed')
        native_common=common
        # Pilot intentionally compares separately pinned candidate cores. The
        # affinity itself must be stable within each candidate group, while
        # priority, power and throttling state remain global invariants.
        affinity=record.get('process_affinity')
        affinity_group=run.get('mask', affinity if affinity is not None else run.get('kind'))
        if affinity is not None and affinity_group in native_by_affinity and affinity != native_by_affinity[affinity_group]:
            raise ValueError('Cross-process affinity state changed within group')
        if affinity is not None:
            native_by_affinity[affinity_group]=affinity

def child(output,name,binary,mask,profile=None,phase='formal',reads=4096,warm=False):
    env=os.environ.copy()
    for key in ('CUEXIS_PD_METRICS','CUEXIS_PD_PROFILE','CUEXIS_PD_FORMAL_PHASE','CUEXIS_PD_V2_READS'):env.pop(key,None)
    env.update(CUEXIS_PD_PROFILE=profile or 'legacy',CUEXIS_PD_FORMAL_PHASE=phase,CUEXIS_PD_V2_READS=str(reads))
    command=[sys.executable,'-B',str(ROOT/'tools/verification/playback_performance/baseline_method.py'),'--child',str(mask),str(binary),
             'PD machine CPU warmup' if warm else 'PD uninstrumented validated fixed workload','--rng-seed','1']
    with (output/(name+'.log')).open('wb') as log:
        p=sp.run(command,cwd=ROOT,env=env,stdout=log,stderr=sp.STDOUT,timeout=180)
    if p.returncode:raise RuntimeError(name+' failed, exit='+str(p.returncode))
    lines=(output/(name+'.log')).read_text(encoding='utf-8',errors='replace').splitlines()
    if warm:
        if not any(s.startswith('PD_MACHINE_WARMUP ') for s in lines):raise ValueError('Incomplete machine warmup')
        return {'command':command,'exit_code':0,'log_sha256':base.sha(output/(name+'.log'))}
    parse=lambda prefix:[json.loads(s[len(prefix):]) for s in lines if s.startswith(prefix)]
    ids,samples,anchors=parse('PD_FORMAL_ID '),parse('PD_V2 '),parse('PD_QPC_ANCHOR ')
    if len(ids)!=1 or ids[0]['metrics_compile_enabled'] or ids[0]['allocator']!='normal':raise ValueError('Not uninstrumented normal allocator')
    count=3 if phase=='pilot' else 8
    if len(samples)!=len(OWNER_METRICS)*count or any(
            sum(r['kind']==kind for r in samples)!=count for kind in OWNER_METRICS):
        raise ValueError('Incomplete fixed blocks')
    alignment=validate_alignment([r['sample'] for r in samples],anchors)
    if list(output.glob(name+'.*.jsonl')):raise ValueError('Unexpected instrumentation output')
    result={'command':command,'exit_code':0,'identity':ids[0],'samples':samples,
            'environment':native_environment(lines),'alignment':alignment,'log_sha256':base.sha(output/(name+'.log'))}
    save(output/(name+'.json'),result);print(name+': exit=0',flush=True);return result

def summaries(runs):
    results=[]
    for config,profile in COMBINATIONS:
        rows=[r for r in runs if r.get('config')==config and r.get('profile')==profile and r['kind']=='single_owner']
        for metric in OWNER_METRICS:
            values=[st.median(x['sample']['wall_us']/x['operations'] for x in r['data']['samples'] if x['kind']==metric) for r in rows]
            windows=[[v for r,v in zip(rows,values) if r['window']==w] for w in (1,2)]
            raw=[x['sample']['wall_us']/x['operations'] for r in rows for x in r['data']['samples'] if x['kind']==metric]
            results.append({'config':config,'profile':profile,'metric':metric,'process_medians':values,
                'window_noise':[noise(v) if len(v)==3 else None for v in windows],
                'combined_noise':noise(values) if len(values)==6 else None,'raw_blocks':base.stats(raw)})
    hosts=[]
    for config in ('release','debug'):
        rows=[r for r in runs if r['kind']=='host' and r['config']==config]
        if not rows:continue
        values=[r['data']['statistics']['request_to_receipt_ack_us']['mean_us'] for r in rows]
        windows=[[v for r,v in zip(rows,values) if r['window']==w] for w in (1,2)]
        hosts.append({'config':config,'process_means_us':values,'window_noise':[noise(v) if len(v)==3 else None for v in windows],
            'combined_noise':noise(values) if len(values)==6 else None,'tail_budget':None,'over_budget_count':None,
            'pooled':{field:base.stats([x[field] for r in rows for x in r['data']['samples']]) for field in FIELDS},
            'diagnostic_reference_counts_not_budgets':{field:{str(limit):sum(x[field]>limit for r in rows for x in r['data']['samples'])
                for limit in (1000,5000,16667)} for field in FIELDS}})
    return results,hosts

def repeatability_valid(rows,expected_count):
    return len(rows)==expected_count and all(r['combined_noise'] is not None and r['combined_noise']<=.15 and
        all(value is not None and value<=.15 for value in r['window_noise']) for r in rows)

def select_registered_mask(topology):
    masks=topology.get('candidate_masks',[])
    if not masks:
        raise ValueError('Topology did not provide a candidate physical-core mask')
    # The first candidate is the first non-CPU-0 physical core from topology.
    # Pilot noise remains diagnostic and cannot select a favorable result.
    return masks[0]

def main(args):
    if args.action=='recover':
        registration=load(args.registration)
        state_path=args.registration.with_suffix('.state.json')
        state=load(state_path)
        if state.get('status')!='stopped_error' or len(state.get('windows',[]))!=1:
            raise ValueError('Only a stopped first window can be recovered')
        folder=Path(state['windows'][0])
        manifest=load(folder/'manifest.json')
        if len(manifest.get('runs',[])) != 18:
            raise ValueError('Incomplete window cannot be recovered')
        validate_runs(manifest['runs'])
        manifest['results'],manifest['host_results']=summaries(manifest['runs'])
        long_ok=all(x['sample']['wall_us']>=100000 for r in manifest['runs']
                    if r['kind']=='single_owner' for x in r['data']['samples'] if x['kind']=='read_long')
        manifest.update(status='collected',revalidated_utc=utc(),long_block_duration_ok=long_ok,
                        recovery='runner validation only; no measurement process rerun')
        save(folder/'manifest.json',manifest)
        state.update(status='window1_complete',window1_finished_epoch=time.time(),
                     window1_finished_monotonic=time.monotonic(),recovered_utc=utc())
        save(state_path,state)
        print('STATUS window1_recovered',flush=True)
        return
    if args.action=='pilot':
        output=evidence(args.output,'pilot'); env=environment(); topo=topology(env['machine']); binary=binaries();source=base.identity()
        manifest={'status':'registered','source':source,'binaries':binary,'environment_before':env,'topology':topo,'runs':[],
            'registration':{'masks':topo['candidate_masks'],'processes':16,'repeats':2,'order':COMBINATIONS,
                            'machine_warmup_seconds':30,'phase':'pilot','reads':4096,'retry':False,'etw':False}}
        save(output/'manifest.json',manifest)
        for mask in topo['candidate_masks']:
            no_trace();same_environment(env,environment())
            child(output,f'warm-mask-{mask}',binary['release']['binaries']['baseline']['path'],mask,warm=True)
            for repeat in range(2):
                ordered=COMBINATIONS[repeat:]+COMBINATIONS[:repeat]
                for config,profile in ordered:
                    same_environment(env,environment());no_trace()
                    data=child(output,f'mask-{mask}-{config}-{profile}-{repeat+1}',binary[config]['binaries']['baseline']['path'],mask,profile,'pilot')
                    manifest['runs'].append({'mask':mask,'config':config,'profile':profile,'data':data})
                    same_environment(env,environment());save(output/'manifest.json',manifest)
        scores={}
        for mask in topo['candidate_masks']:
            scores[mask]=max(noise([st.median(x['sample']['wall_us']/x['operations'] for x in r['data']['samples'] if x['kind']==metric)
                for r in manifest['runs'] if r['mask']==mask and r['config']==config and r['profile']==profile])
                for config,profile in COMBINATIONS for metric in OWNER_METRICS)
        chosen=select_registered_mask(topo)
        reads={}
        for config,profile in COMBINATIONS:
            costs=[x['sample']['wall_us']/x['operations'] for r in manifest['runs'] if r['mask']==chosen and r['config']==config and r['profile']==profile
                   for x in r['data']['samples'] if x['kind']=='read_long']
            required=max(4096,math.ceil(312500/min(costs)))
            batch=1<<(required-1).bit_length()
            if batch>4194304:raise ValueError('Pilot read count exceeds fixed cap')
            reads[config+'/'+profile]=batch
        validate_runs(manifest['runs'])
        manifest.update(status='pilot_complete',diagnostic_scores=scores,
                        core_selection_policy='pre-registered first physical-core mask; pilot noise is diagnostic only',
                        selected_mask=chosen,read_counts=reads,
                        environment_after=environment(),source_after=base.identity())
        same_environment(env,manifest['environment_after'])
        if source['implementation_sha256']!=manifest['source_after']['implementation_sha256'] or binaries()!=binary:raise ValueError('Pilot identity changed')
        save(output/'manifest.json',manifest);print('STATUS pilot_complete',flush=True);return

    if args.action=='register':
        pilot=load(args.pilot)
        if pilot['status']!='pilot_complete':raise ValueError('Completed pilot required')
        source=base.identity();binary=binaries();env=environment();same_environment(pilot['environment_before'],env)
        if source['implementation_sha256']!=pilot['source']['implementation_sha256'] or binary!=pilot['binaries']:raise ValueError('Pilot binding changed')
        path=args.output/('registration-'+dt.datetime.now(dt.timezone.utc).strftime('%Y%m%dT%H%M%SZ')+'.json')
        path.parent.mkdir(parents=True,exist_ok=True)
        if path.exists():raise ValueError('Registration already exists')
        registration={'revision':'pd0-uninstrumented-two-window-v1','registered_utc':utc(),'source':source,'binaries':binary,
            'environment':env,'selected_mask':pilot['selected_mask'],'topology':pilot['topology'],'read_counts':pilot['read_counts'],
            'pilot':{'path':str(args.pilot.resolve()),'sha256':base.sha(args.pilot)},'oem_declaration':args.oem,
            'windows':2,'processes_per_cell_per_window':3,'minimum_gap_seconds':600,'noise_limit':.15,
            'machine_warmup_seconds':30,'etw':False,'stage_instrumentation':False,'allocator':'normal',
            'metrics':list(OWNER_METRICS),'counts_per_process':8,'minimum_long_block_us':100000,
            'host_processes_per_config_per_window':3,'host_affinity':'inherited allowed CPUs',
            'primary_boundary':'fixed trajectory includes correctness checks and FrameDigest',
            'no_filters_or_retry':True,'comparison_effect_or_CI':None,'tail_budget':None}
        save(path,registration);print('REGISTRATION '+str(path),flush=True);return

    registration=load(args.registration);reg_hash=base.sha(args.registration)
    state_path=args.registration.with_suffix('.state.json')
    state=load(state_path) if state_path.exists() else {'status':'registered','registration_sha256':reg_hash,'windows':[]}
    if state['registration_sha256']!=reg_hash or state['status'] not in ('registered','window1_complete'):raise ValueError('Registration consumed or stopped')
    index=len(state['windows'])+1
    if index==2 and time.monotonic()-state['window1_finished_monotonic']<600:raise ValueError('Second window not yet eligible; no sampling started')
    env=environment();same_environment(registration['environment'],env);binary=binaries();source=base.identity()
    if not measurement_identity_matches(registration['source'], source) or binary!=registration['binaries']:
        raise ValueError('Registered measurement identity changed')
    no_trace()
    claim=args.registration.with_suffix(f'.window-{index}.claim.json')
    with claim.open('x',encoding='utf-8') as stream:json.dump({'utc':utc(),'pid':os.getpid()},stream)
    output=evidence(args.output,f'window-{index}')
    state['status']='running';state['windows'].append(str(output));save(state_path,state)
    manifest={'status':'registered','window':index,'source':source,'binaries':binary,'environment_before':env,
              'registration':{'path':str(args.registration.resolve()),'sha256':reg_hash},'runs':[],'pd0_exit':False}
    save(output/'manifest.json',manifest)
    try:
        child(output,'machine-warmup',binary['release']['binaries']['baseline']['path'],registration['selected_mask'],warm=True)
        for repeat in range(3):
            offset=(repeat+index-1)%4;ordered=COMBINATIONS[offset:]+COMBINATIONS[:offset]
            for config,profile in ordered:
                same_environment(env,environment());no_trace()
                data=child(output,f'{config}-{profile}-{repeat+1}',binary[config]['binaries']['baseline']['path'],
                    registration['selected_mask'],profile,'formal',registration['read_counts'][config+'/'+profile])
                manifest['runs'].append({'kind':'single_owner','window':index,'config':config,'profile':profile,'data':data})
                same_environment(env,environment());save(output/'manifest.json',manifest)
            for config in (('release','debug') if repeat%2==0 else ('debug','release')):
                same_environment(env,environment());no_trace();machine=dict(env['machine'],selected_affinity=env['machine']['parent_affinity'])
                data=invoke_host(output,f'{config}-host-{repeat+1}',binary[config]['binaries']['host']['path'],machine,False)
                lines=(output/f'{config}-host-{repeat+1}.log').read_text(encoding='utf-8',errors='replace').splitlines()
                data['environment']=native_environment(lines)
                manifest['runs'].append({'kind':'host','window':index,'config':config,'data':data})
                same_environment(env,environment());save(output/'manifest.json',manifest)
        validate_runs(manifest['runs'])
        manifest['results'],manifest['host_results']=summaries(manifest['runs'])
        manifest.update(status='collected',source_after=base.identity(),environment_after=environment(),finished_utc=utc())
        same_environment(env,manifest['environment_after'])
        if not measurement_identity_matches(source, manifest['source_after']) or binaries()!=binary:
            raise ValueError('Final measurement identity changed')
        save(output/'manifest.json',manifest)
        if index==1:
            state.update(status='window1_complete',window1_finished_epoch=time.time(),window1_finished_monotonic=time.monotonic())
        else:
            runs=[r for folder in state['windows'] for r in load(Path(folder)/'manifest.json')['runs']]
            validate_runs(runs)
            results,hosts=summaries(runs)
            long_ok=all(x['sample']['wall_us']>=100000 for r in runs if r['kind']=='single_owner' for x in r['data']['samples'] if x['kind']=='read_long')
            state.update(status='baseline_validated' if repeatability_valid(results,len(COMBINATIONS)*len(OWNER_METRICS)) and long_ok else 'inconclusive',results=results,
                host_status='repeatability_validated' if repeatability_valid(hosts,2) else 'inconclusive',host_results=hosts,long_block_duration_ok=long_ok,pd0_exit=False)
        save(state_path,state)
    except Exception as error:
        manifest.update(status='stopped_error',error=str(error));save(output/'manifest.json',manifest)
        state.update(status='stopped_error',error=str(error));save(state_path,state);raise
    print('STATUS '+state['status'],flush=True)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('action',choices=['pilot','register','window','recover'])
    p.add_argument('--output',type=Path,default=ROOT/'out/pd0-uninstrumented');p.add_argument('--pilot',type=Path)
    p.add_argument('--registration',type=Path);p.add_argument('--oem',default='unknown; not confirmed by host')
    args=p.parse_args()
    if args.action=='register' and args.pilot is None:p.error('register requires --pilot')
    if args.action in ('window','recover') and args.registration is None:p.error(args.action+' requires --registration')
    try:
        main(args)
    except Exception as error:
        if args.action=='pilot' and ACTIVE_OUTPUT is not None and (ACTIVE_OUTPUT/'manifest.json').exists():
            failed=load(ACTIVE_OUTPUT/'manifest.json');failed.update(status='stopped_error',error=str(error))
            save(ACTIVE_OUTPUT/'manifest.json',failed)
        raise

"""Prepare immutable PD-5 tool-validation profiles. Never registers a formal window.

Artifacts under pd0/pd4 are read-only inputs. A separate registration is required
after capability, observer, semantic and environment gates are satisfied.
"""
import argparse
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time
import uuid

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[3]))

from tools.verification.playback_performance.measurement import collect, LIMITS_US
from tools.verification.playback_performance import process_control

ROOT = Path(__file__).resolve().parents[3]
SUITE = Path(__file__).with_name('profiles.json')


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for block in iter(lambda: f.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def load(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def save_new(path, data):
    with Path(path).open('x', encoding='utf-8') as f:
        f.write(json.dumps(data, ensure_ascii=False, indent=2) + '\n')


def validate_profile(p):
    if p.get('schema') != 'pd5.profile.v1' or not p.get('id') or p.get('config') not in ('Release','Debug'):
        raise ValueError('Invalid profile identity')
    if p.get('allocator') != 'normal' or p.get('metrics_compile_enabled') is not False or p.get('etw') is not False:
        raise ValueError('Formal timing configuration must exclude observers')
    matrix=p.get('harness') in ('presentation-matrix', 'renderer-matrix')
    modes = ('no-consumer','low-delivery','capture-1','capture-2','capture-4','sparse','topology') if matrix else ('stream','controls')
    if p.get('mode') not in modes:
        raise ValueError('Unknown probe mode')
    if matrix:
        expected=0 if p['mode']=='no-consumer' else 4 if p['mode']=='capture-4' else 2 if p['mode']=='capture-2' else 1
        if p.get('capture_consumers')!=expected or p.get('update_ratio')!=(8 if p['mode']=='low-delivery' else 1):
            raise ValueError('Matrix U/F/consumer declaration mismatch')
        if p.get('gpu') is not (p.get('harness') == 'renderer-matrix') or p.get('frame_byte_bound')!=1048576:
            raise ValueError('Matrix CPU/frame bound mismatch')
        if p.get('harness') == 'renderer-matrix' and p['mode'] not in ('capture-1','capture-2','capture-4'):
            raise ValueError('Renderer matrix supports only fixed capture modes')
    for key in ('frames','warmup_frames','rate','cycles'):
        value = p.get(key)
        if type(value) is not int or value <= 0:
            raise ValueError('Missing finite trajectory field: ' + key)
    limits = {'frames':1000000, 'warmup_frames':100000, 'rate':4000, 'cycles':100}
    if any(p[k] > v for k,v in limits.items()):
        raise ValueError('Trajectory exceeds native probe limits')
    caps = p.get('capacity',{})
    for key in ('session_commands','control_reserve','steps','step_reserve','journal_attempts',
                'journal_feedback','journal_bytes','tombstones','coverage_intervals','retention_us',
                'receipt_deadline_us','receipt_capacity','consumers','global_payloads','global_bytes',
                'request_history','global_attempts','max_frame_members','host_commands',
                'capture_commands','capture_reserve','reply_deadline_ms','renderer_payloads','renderer_attempts'):
        if type(caps.get(key)) is not int or caps[key] <= 0:
            raise ValueError('Missing finite capacity: ' + key)
    if caps['control_reserve'] >= caps['session_commands'] or caps['step_reserve'] >= caps['steps']:
        raise ValueError('Reliable queue reserve consumes ordinary capacity')
    if p.get('gpu') is not bool(p.get('gpu')) or p.get('audio') is not False:
        raise ValueError('This implemented probe has no physical audio path')
    if p.get('viewport')!=[1280,720] or p['rate']!=240 or p['cycles']!=10:
        raise ValueError('Reference viewport/rate/control trajectory differs from accepted suite')
    if p['gpu'] and (p.get('vsync') is not True or p.get('viewport') != [1280,720]):
        raise ValueError('GPU reference surface mismatch')
    if p['frames'] < 10000 or p['frames'] / p['rate'] < 60 or p['warmup_frames'] / p['rate'] < 10:
        raise ValueError('Steady trajectory/warmup too short')
    if p['frames'] / p['rate'] < 10 * caps['retention_us'] / 1000000 or p['frames'] < 10*caps['global_payloads']:
        raise ValueError('Trajectory does not cover retention/capacity')
    if not isinstance(p.get('formal_blockers'),list):
        raise ValueError('Explicit capability disposition required')
    return p


def require_target_clean(build, executable):
    import re
    cache=(build/'CMakeCache.txt').read_text(encoding='utf-8-sig')
    ninja=re.search(r'^CMAKE_MAKE_PROGRAM:FILEPATH=(.+)$',cache,re.M)[1]
    # -n cannot prove cleanliness with CMake CONFIGURE_DEPENDS: its always-dirty
    # glob check schedules a fictitious CMake rerun and hides later work. Query
    # the actual transitive commands/dependency database without executing them.
    raw=subprocess.check_output([ninja,'-C',str(build),'-t','commands',executable.replace('.exe','')],timeout=60)
    commands=raw.decode('utf-8',errors='strict').replace('\\','/')
    deps_raw=subprocess.check_output([ninja,'-C',str(build),'-t','deps'],timeout=60)
    inventories={}
    current=None
    for line in deps_raw.decode('utf-8',errors='strict').splitlines():
        head=re.match(r'^(.*\.obj): #deps (\d+)',line)
        if head:
            current=head[1].replace('\\','/')
            inventories[current]={'count':int(head[2]),'paths':[]}
        elif current and line.strip():
            inventories[current]['paths'].append(line.strip())
    selected=[]
    metadata=[build/'CMakeCache.txt', build/'compile_commands.json', build/'build.ninja',
              build/'CMakeFiles/rules.ninja', *build.glob('CMakeFiles/*/CMakeCXXCompiler.cmake')]
    newest_metadata=max(path.stat().st_mtime_ns for path in metadata)
    for row in load(build/'compile_commands.json'):
        match=re.search(r'/Fo"?([^" ]+\.obj)',row['command'])
        if not match or match[1].replace('\\','/') not in commands:
            continue
        obj=Path(row['directory'])/match[1]
        key=obj.relative_to(build).as_posix()
        inv=inventories.get(key)
        if not inv or inv['count']<=0 or len(inv['paths'])!=inv['count']:
            raise ValueError('Incomplete transitive object dependency inventory: '+key)
        paths=[Path(x) if Path(x).is_absolute() else build/x for x in inv['paths']]
        newest=max(p.stat().st_mtime_ns for p in [Path(row['file']),*paths])
        if newest>obj.stat().st_mtime_ns:
            raise ValueError('Transitive implementation object is stale: '+key)
        if newest_metadata>obj.stat().st_mtime_ns:
            raise ValueError('Build command/configuration newer than transitive object: '+key)
        selected.append(obj)
    if not selected:
        raise ValueError('Empty transitive compile closure')
    exe=build/'bin'/executable
    cmake_inputs=[ROOT/p for p in
        subprocess.check_output(['rg','--files','-g','CMakeLists.txt','-g','*.cmake','-g','*.in','cmake','engine','app','tests'],cwd=ROOT,text=True).splitlines()]
    cmake_inputs.extend([ROOT/'CMakeLists.txt',ROOT/'CMakePresets.json',ROOT/'vcpkg.json',ROOT/'vcpkg-configuration.json'])
    internal_libs=[p for p in (build/'lib').glob('*.lib') if p.relative_to(build).as_posix() in commands]
    if not internal_libs:
        raise ValueError('Missing linked-library closure')
    if max(p.stat().st_mtime_ns for p in [*selected,*internal_libs,*cmake_inputs])>exe.stat().st_mtime_ns:
        raise ValueError('Executable predates transitive objects/libraries/build inputs')
    return {'objects_checked':len(selected),'libraries_checked':len(internal_libs),
            'cmake_inputs_checked':len(cmake_inputs),'commands_sha256':hashlib.sha256(raw).hexdigest(),
            'metadata_hashes':{str(p.relative_to(build)):sha(p) for p in metadata},
            'object_hashes':{str(p.relative_to(build)):sha(p) for p in selected},
            'library_hashes':{str(p.relative_to(build)):sha(p) for p in internal_libs}}


def build_binding(build, executable, sources, *, observer='none'):
    build = Path(build).resolve()
    if ROOT / 'out/build' not in build.parents:
        raise ValueError('Build must be a dedicated local tree')
    if build.name.startswith(('pd0-', 'pd4-')):
        raise ValueError('PD5 preparation must not consume historical build directories as current B')
    cache = (build/'CMakeCache.txt').read_text(encoding='utf-8-sig')
    if observer not in ('none','stages','memory'):
        raise ValueError('Unknown observer kind')
    required = ('CUEXIS_LIBRARY_TYPE:STRING=STATIC','CUEXIS_ENABLE_CHART_V5_CANDIDATE:BOOL=ON',
                'CUEXIS_PD_METRICS_TEST_ONLY:BOOL='+('ON' if observer=='stages' else 'OFF'))
    if any(x not in cache for x in required):
        raise ValueError('Wrong measurement build configuration')
    commands = load(build/'compile_commands.json')
    if observer != 'stages' and any('CUEXIS_PD_METRICS_TEST_ONLY' in r.get('command','') for r in commands):
        raise ValueError('Instrumentation in actual compiler command')
    memory_enabled = 'CUEXIS_ALLOCATION_LIFETIME_OBSERVER:BOOL=ON' in cache
    if memory_enabled != (observer=='memory'):
        raise ValueError('Allocation observer differs from requested build boundary')
    if observer != 'memory' and any('CUEXIS_ALLOCATION_LIFETIME_OBSERVER' in r.get('command','') for r in commands):
        raise ValueError('Allocation observer in normal timing build')
    import re
    ninja = re.search(r'^CMAKE_MAKE_PROGRAM:FILEPATH=(.+)$',cache,re.M)[1]
    objects = []
    for source in sources:
        source = (ROOT/source).resolve()
        rows = [r for r in commands if Path(r['file']).resolve() == source and
                executable.replace('.exe','')+'.dir' in r['command']]
        if len(rows) != 1:
            raise ValueError('Missing/ambiguous measurement compile entry: '+str(source))
        row=rows[0]
        obj_match=re.search(r'(?:/Fo|/Fo\s+)"?([^" ]+\.obj)',row['command'])
        if not obj_match:
            raise ValueError('Missing MSVC object output')
        obj=Path(row.get('directory',str(build)))/obj_match[1]
        raw=subprocess.check_output([ninja,'-C',str(build),'-t','deps',str(obj.relative_to(build))])
        text=raw.decode('utf-8',errors='strict')
        counts=re.findall(r'#deps\s+(\d+)',text)
        deps=[Path(x.strip()) for x in text.splitlines()[1:] if x.strip()]
        deps=[x if x.is_absolute() else build/x for x in deps]
        if len(counts)!=1 or int(counts[0])<=0 or len(deps)!=int(counts[0]):
            raise ValueError('Incomplete Ninja dependency inventory')
        if max(x.stat().st_mtime_ns for x in [source,*deps])>obj.stat().st_mtime_ns:
            raise ValueError('Measurement object predates source/header')
        exe=build/'bin'/executable
        if exe.stat().st_mtime_ns<obj.stat().st_mtime_ns:
            raise ValueError('Executable predates measurement object')
        objects.append({'source':str(source.relative_to(ROOT)), 'source_sha256':sha(source),
                        'object':str(obj),'object_sha256':sha(obj),'dependencies':len(deps),
                        'dependency_inventory_sha256':hashlib.sha256(raw).hexdigest()})
    transitive=require_target_clean(build,executable)
    return {'executable':str(build/'bin'/executable),'executable_sha256':sha(build/'bin'/executable),
            'cache_sha256':sha(build/'CMakeCache.txt'),'commands_sha256':sha(build/'compile_commands.json'),
            'transitive_inputs':transitive,'objects':objects}


def inventory():
    raw=subprocess.check_output(['git','ls-files','--cached','--others','--exclude-standard','-z'],cwd=ROOT)
    files={}
    for p in sorted(set(raw.decode('utf-8').split('\0'))):
        path=ROOT/p
        if p and path.is_file() and not p.startswith(('out/','.git/')):
            files[p]=sha(path)
    return {'head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
            'files':files,'fingerprint':hashlib.sha256(json.dumps(files,sort_keys=True).encode()).hexdigest()}


def expanded_profile(suite, item):
    overrides=suite.get('matrix_defaults',{}) if item.get('harness') in ('presentation-matrix', 'renderer-matrix') else {}
    profile={**suite['defaults'], **overrides, **item}
    profile['capacity']={**suite['defaults']['capacity'],**item.get('capacity',{})}
    return validate_profile(profile)


def prepare(args):
    suite=load(SUITE)
    item=next(p for p in suite['profiles'] if p['id']==args.profile)
    profile=expanded_profile(suite,item)
    folder=args.output/('prepare-'+dt.datetime.now(dt.timezone.utc).strftime('%Y%m%dT%H%M%SZ')+'-'+uuid.uuid4().hex[:8])
    folder.mkdir(parents=True)
    gpu=profile['gpu']
    target='cuexis_execution_device_probe.exe' if gpu else 'cuexis_pd_host_probe.exe'
    sources=['tests/pd/pd5_device_probe.cpp','engine/render_opengl/src/gpu_timer.cpp'] if gpu else [
        'tests/pd/pd5_matrix_probe.cpp' if profile.get('harness')=='presentation-matrix' else 'tests/pd/pd5_host_probe.cpp']
    manifest={'schema':'pd5.preparation.v1','profile':profile,'profile_file_sha256':sha(SUITE),
              'status':'tool_validation_prepared','formal_registration':False,
              'source':inventory(),'build':build_binding(args.build,target,sources),
              'limits_us':LIMITS_US,'owner_acceptance':'2026-10-19 user explicitly accepted scope/all thresholds/protocol',
              'open_blockers':profile['formal_blockers']}
    cache=(args.build/'CMakeCache.txt').read_text(encoding='utf-8-sig')
    if 'CMAKE_BUILD_TYPE:STRING='+profile['config'] not in cache:
        raise ValueError('Profile configuration differs from actual build')
    save_new(folder/'manifest.json',manifest)
    print(folder/'manifest.json')


def verify(path):
    manifest=load(path)
    if manifest.get('schema')!='pd5.preparation.v1' or manifest.get('formal_registration') is not False:
        raise ValueError('Not a tool preparation manifest')
    validate_profile(manifest['profile'])
    if sha(SUITE)!=manifest['profile_file_sha256'] or inventory()!=manifest['source']:
        raise ValueError('Preparation source/profile identity changed')
    b=manifest['build']
    build=Path(b['executable']).parent.parent
    if sha(build/'CMakeCache.txt')!=b['cache_sha256'] or sha(build/'compile_commands.json')!=b['commands_sha256']:
        raise ValueError('Prepared build configuration changed')
    if require_target_clean(build,Path(b['executable']).name)!=b['transitive_inputs']:
        raise ValueError('Transitive build closure changed')
    if sha(b['executable'])!=b['executable_sha256']:
        raise ValueError('Prepared executable changed')
    for item in b['objects']:
        if sha(item['object'])!=item['object_sha256'] or sha(ROOT/item['source'])!=item['source_sha256']:
            raise ValueError('Prepared measurement object changed')
    return manifest


def expected_counts(profile, *, smoke=False, full_controls=False):
    frames = 8 if smoke else profile['frames']
    cycles = profile['cycles'] if not smoke or full_controls else 1
    matrix = profile.get('harness') in ('presentation-matrix', 'renderer-matrix')
    combined = profile.get('harness') == 'renderer-matrix'
    if matrix:
        total = profile['capture_consumers'] + int(combined)
        result = {'sdk_update': frames * profile['update_ratio']}
        if total: result['materialize'] = frames * total
        if profile['capture_consumers']: result['capture_delivery'] = frames * profile['capture_consumers']
        if combined: result['renderer_delivery'] = frames
        if profile['mode'] == 'topology':
            interval, warmup = (4, 4) if smoke else (256, profile['warmup_frames'])
            result['topology_reload'] = (warmup+frames-1)//interval - (warmup-1)//interval
    else:
        result = {'load_host': 1, 'close': 1}
        if profile['mode'] == 'stream':
            result.update(request_to_ready=frames, request_to_present_return=frames)
        else:
            result.update(pause=2*cycles, resume=2*cycles, reset_paused=cycles,
                          reset_running=cycles, resize=cycles)
    if profile['gpu'] and (matrix or profile['mode'] == 'stream'):
        result.update(cpu_submit=frames, present_call=frames)
        if smoke: result['gpu_render'] = frames
    return result


def run_probe(args):
    m=verify(args.manifest)
    p=m['profile']
    folder=args.manifest.parent/('tool-check-'+uuid.uuid4().hex[:8])
    folder.mkdir()
    # This command is only a short plumbing check. It can never consume a formal claim.
    env=os.environ.copy()
    for key in list(env):
        if key.startswith('CUEXIS_PD_') or key.startswith('CUEXIS_PD5_'):
            env.pop(key)
    env.update(CUEXIS_PD5_MODE=p['mode'], CUEXIS_PD5_FRAMES='8',CUEXIS_PD5_WARMUP='4',
               CUEXIS_PD5_CYCLES=str(p['cycles']) if args.full_controls else '1',
               CUEXIS_PD5_RATE=str(p['rate']),CUEXIS_PD5_SMOKE='1',
               CUEXIS_PD5_GPU_TIMER='1' if p['gpu'] and (p['mode']=='stream' or p.get('harness')=='renderer-matrix') else '0')
    matrix=p.get('harness') in ('presentation-matrix', 'renderer-matrix')
    combined=p.get('harness')=='renderer-matrix'
    command=[m['build']['executable'],'PD5 renderer and capture workload matrix' if combined else 'PD5 presentation workload matrix' if matrix else
             'PD5 scripted host timing probe','--rng-seed','1','--reporter','compact']
    start=time.perf_counter_ns()
    try:
        with (folder/'stdout.log').open('wb') as out, (folder/'stderr.log').open('wb') as err:
            r=process_control.run(command,process_control.affinity(),cwd=ROOT,env=env,output=out,timeout=120)
    except (subprocess.TimeoutExpired,OSError) as error:
        save_new(folder/'process.json',{'phase':'tool_validation_only','command':command,
            'exit_code':None,'error':type(error).__name__+': '+str(error),
            'launch_perf_counter_ns':start,'end_perf_counter_ns':time.perf_counter_ns(),
            'formal_acceptance':False,'manifest_sha256':sha(args.manifest)})
        raise
    raw={'phase':'tool_validation_only','command':command,'exit_code':r.returncode,
         'launch_perf_counter_ns':start,'end_perf_counter_ns':time.perf_counter_ns(),
         'formal_acceptance':False,'manifest_sha256':sha(args.manifest)}
    save_new(folder/'process.json',raw)
    parsed=collect(folder/'stdout.log','probe',r.returncode)
    if parsed['identity'].get('smoke') is not True:
        raise ValueError('Tool-validation mode not confirmed by child')
    identity=parsed['identity']
    expected_cycles=p['cycles'] if args.full_controls else 1
    expected_identity={'mode':p['mode'],'frames':8,'warmup_frames':4,'audio':False,
                       'source_step_ticks':7000,'source_tick_rate':1000000}
    if not matrix:expected_identity.update(cycles=expected_cycles,rate=p['rate'])
    if any(identity.get(k)!=v for k,v in expected_identity.items()):
        raise ValueError('Child trajectory differs from requested tool profile')
    declared_counts = expected_counts(p, smoke=True, full_controls=args.full_controls)
    if matrix:
        total=p['capture_consumers'] + int(combined)
        if identity.get('updates')!=8*p['update_ratio'] or identity.get('materializations')!=8*total:
            raise ValueError('Matrix did not execute registered U/F counts')
    if identity['counts']!=declared_counts:
        raise ValueError('Child did not execute the complete requested tool workload')
    save_new(folder/'collected.json',parsed)
    verify(args.manifest)
    print(json.dumps({'folder':str(folder),'exit_code':r.returncode,
                      'metrics':{k:v['n'] for k,v in parsed['statistics'].items()},'formal_acceptance':False}))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    sub=parser.add_subparsers(dest='action',required=True)
    p=sub.add_parser('prepare');p.add_argument('--profile',required=True)
    p.add_argument('--build',type=Path,default=ROOT/'out/build/pd5-preparation-release')
    p.add_argument('--output',type=Path,default=ROOT/'out/pd5-preparations')
    p=sub.add_parser('verify');p.add_argument('--manifest',type=Path,required=True)
    p=sub.add_parser('probe');p.add_argument('--manifest',type=Path,required=True)
    p.add_argument('--full-controls',action='store_true',help='Validate all declared control counts; still not acceptance')
    p=sub.add_parser('collect');p.add_argument('--log',type=Path,required=True)
    p.add_argument('--kind',choices=['owner','host','probe'],required=True)
    p.add_argument('--exit-code',type=int,required=True)
    p.add_argument('--phase',choices=['pilot','formal'],default='formal')
    p.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    if args.action=='prepare':prepare(args)
    elif args.action=='verify':verify(args.manifest);print('Preparation identity verified')
    elif args.action=='probe':run_probe(args)
    else:
        result=collect(args.log,args.kind,args.exit_code,args.phase)
        result.update(imported_observation_only=True,acceptance=False,source_log_sha256=sha(args.log))
        save_new(args.output,result)
        print('Imported observation only; no formal acceptance')


if __name__=='__main__':
    main()

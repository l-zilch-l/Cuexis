"""Bind and run the sole F1/C2 finite candidate; always hand back before retention."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

if __package__ in (None, ''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[3]))
from tools.verification.playback_performance import diagnostic as d, finite_followup as f
from tools.verification.playback_performance import host_diagnostics as h, measurement as m
from tools.verification.playback_performance import process_control as control

F1_INVENTORY_SHA256='1bfebe4b6e49dcc291136d3d2dec13e64e296341493e3e3d28023afcdecd6da3'


def compiled_source_identity(source):
    # Git archives use LF; the original Windows checkout can use CRLF.
    files=f.source_identity(source)['files']
    return {name:hashlib.sha256((source/name).read_bytes().replace(b'\r\n',b'\n')).hexdigest()
            for name in files if not (name.startswith('tests/') and name.endswith('.py'))}


def verify_f1_source(source):
    original=f.ROOT/'out/pd5-followup-20261021-62ca993'
    frozen=json.loads((original/'diagnostic/plan.json').read_text(encoding='utf-8'))['builds']['B']['source_identity']
    digest=hashlib.sha256(json.dumps(frozen['files'],sort_keys=True).encode()).hexdigest()
    if digest!=F1_INVENTORY_SHA256 or frozen['sha256']!=digest:
        raise ValueError('Fixed F1 inventory identity changed')
    archive=original/'f1-source'
    if f.source_identity(archive)!=frozen:
        raise ValueError('Original F1 source bytes changed')
    expected=compiled_source_identity(archive)
    if compiled_source_identity(source)!=expected:
        raise ValueError('Comparison source is not the fixed F1 product/build/harness')
    return dict(original_inventory_sha256=digest,
                compiled_inputs_sha256=hashlib.sha256(json.dumps(expected,sort_keys=True).encode()).hexdigest(),
                normalization='CRLF to LF; excludes only tests/**/*.py, which are not compiled probe inputs')


def verify_build_source(source,cache):
    entries=dict(line.split('=',1) for line in cache.splitlines() if line.startswith('CMAKE_HOME_DIRECTORY:INTERNAL='))
    if Path(entries.get('CMAKE_HOME_DIRECTORY:INTERNAL','')).resolve()!=source.resolve():
        raise ValueError('Build cache belongs to a different source directory')


def build_identity(source,build):
    bindings={kind:dict(path=str(build/'bin'/name),sha256=f.sha(build/'bin'/name))
              for kind,name in (('baseline','cuexis_pd_baseline_probe.exe'),('host','cuexis_pd_host_probe.exe'))}
    paths=[build/name for name in ('CMakeCache.txt','compile_commands.json','build.ninja')]
    paths+=sorted((build/'lib').glob('*.lib'))
    cache=(build/'CMakeCache.txt').read_text(encoding='utf-8-sig')
    verify_build_source(source,cache)
    for entry in ('CMAKE_BUILD_TYPE:STRING=Release','CUEXIS_LIBRARY_TYPE:STRING=STATIC',
                  'CUEXIS_PD_METRICS_TEST_ONLY:BOOL=OFF','CUEXIS_PLAYBACK_TRACE_HARNESS:BOOL=OFF',
                  'CUEXIS_ALLOCATION_LIFETIME_OBSERVER:BOOL=OFF','CUEXIS_ENABLE_CHART_V5_CANDIDATE:BOOL=ON'):
        if entry not in cache:raise ValueError('Wrong normal build configuration: '+entry)
    return dict(source=str(source),source_identity=f.source_identity(source),bindings=bindings,
                build_files={str(p):f.sha(p) for p in paths})


def environment():
    power=subprocess.check_output(['powercfg','/getactivescheme']).decode('utf-8',errors='replace').strip()
    command="""$ErrorActionPreference='Stop'; [pscustomobject]@{
      cpu=@(Get-CimInstance Win32_Processor | Select-Object Name,NumberOfCores,NumberOfLogicalProcessors);
      os=Get-CimInstance Win32_OperatingSystem | Select-Object Version,BuildNumber,OSArchitecture;
      processes=@(Get-Process | Select-Object Name,Id,CPU)
    } | ConvertTo-Json -Depth 4 -Compress"""
    machine=json.loads(subprocess.check_output(['powershell.exe','-NoProfile','-Command',command],
        encoding='utf-8',errors='replace'))
    return dict(power_scheme=power,machine=machine,allowed_affinity=control.affinity(),
                limitation='Point snapshots are not continuous thermal/frequency or contention traces')


def bind(args):
    folder=args.output.resolve()
    f1_anchor=verify_f1_source(args.f1_source.resolve())
    if args.f1_build.resolve()==args.candidate_build.resolve():
        raise ValueError('F1 and candidate require distinct build directories')
    validation=json.loads(args.validation.read_text(encoding='utf-8'))
    if validation['release_ctest_exit'] != 0 or validation['codegen_supported'] is not True:
        raise ValueError('Semantic/codegen validation required before timing')
    if validation['candidate_source_sha256'] != f.source_identity(args.candidate_source.resolve())['sha256']:
        raise ValueError('Validation belongs to a different candidate source')
    registration=json.loads((folder/'preregistration.json').read_text(encoding='utf-8'))
    if f.sha(f.ROOT/registration['plan']) != registration['plan_sha256']:
        raise ValueError('Predeclared conditions changed')
    plan=dict(schema='pd5.read-result-candidate.v1',formal=False,pending_review=True,
        method=d.METHOD,protocol=d.PROTOCOL,counts=f.COUNTS,blocks=3,pairs=3,
        order=registration['order'],scale_mask=4,host_mask=control.affinity(),warmup_seconds=30,
        host=dict(warmup=32,measured=1024,frame_capacity=2048),
        preregistration_sha256=f.sha(folder/'preregistration.json'),
        fixed_f1_anchor=f1_anchor,
        validation_sha256=f.sha(args.validation),validation_path=str(args.validation.resolve()),
        builds={'A':build_identity(args.f1_source.resolve(),args.f1_build.resolve()),
                'B':build_identity(args.candidate_source.resolve(),args.candidate_build.resolve())},
        tools={str(p):f.sha(p) for p in Path(__file__).parent.glob('*.py')},
        criteria=dict(read64_upper=.95,all_metric_upper=1.10,noise=.15),
        labels={'A':'F1','B':'C2'},stop='Execution/identity/output/short-block error stops remaining units; no replacement',
        quantiles='Host operations only; read/update blocks never operation quantiles')
    f.save(folder/'environment-before.json',environment())
    f.save(folder/'plan.json',plan)
    print(f.sha(folder/'plan.json'))


def decision(scale,host):
    rows=[*scale.values(),host]
    return dict(primary_read64=scale['64-read']['effect']['ci95'][1]<=.95,
        all_metric_intervals=all(r['effect']['ci95'][1]<=1.10 for r in rows),
        repeatability=all(max(r['noise'].values())<=.15 for r in rows))


def check_native_environment(record):
    raw=Path(record['log']).read_text(encoding='utf-8-sig')
    started=m.records(raw,'PD5_PROCESS_START ')[-1]
    snapshots=m.records(raw,'PD_ENV ')
    if [r['label'] for r in snapshots] != ['begin','end']:
        raise ValueError('Missing native environment snapshots')
    if any(r['pid']!=started['pid'] or r['process_affinity']!=record['mask'] for r in snapshots):
        raise ValueError('Native environment differs from the frozen process')


def run(args):
    folder=args.output.resolve();plan=json.loads((folder/'plan.json').read_text(encoding='utf-8'))
    f.verify(plan)
    if (f.sha(folder/'preregistration.json')!=plan['preregistration_sha256'] or
            f.sha(Path(plan['validation_path']))!=plan['validation_sha256']):
        raise ValueError('Preregistration or validation identity changed')
    f.save(folder/'run.claim.json',dict(plan_sha256=f.sha(folder/'plan.json'),formal=False))
    result=dict(formal=False,acceptance=False,pending_review=True,scale={},host={},errors=[])
    collected={s:[] for s in 'AB'}
    try:
        for kind in ('scale','host'):
            for index,order in enumerate(plan['order']):
                host_pair={}
                for side in order:
                    record=f.launch(folder,plan,side,kind,index)
                    check_native_environment(record)
                    if kind=='scale':collected[side].append(f.scale_records(record,plan))
                    else:host_pair[side]=h.host_record(record,plan)
                if kind=='scale':
                    if any(collected['A'][index][key]['workload']!=collected['B'][index][key]['workload']
                           for key in collected['A'][index]):
                        raise ValueError('Candidate changed logical work or output')
                else:m.require_comparable(host_pair['A'],host_pair['B'],'request_to_ready_us')
        ids={s:dict(source_sha256=plan['builds'][s]['source_identity']['sha256'],
                    binary_sha256=plan['builds'][s]['bindings']['baseline']['sha256']) for s in 'AB'}
        for key in collected['A'][0]:
            data=dict(identities=ids,pairs=[[collected['A'][i][key],collected['B'][i][key]] for i in range(3)])
            f.save(folder/(key+'.input.json'),data)
            result['scale'][key]=d.compare(data['pairs'],ids)
        data=h.analyze(folder,plan)
        f.save(folder/'host.input.json',data)
        result['host']=d.compare(data['pairs'],data['identities'])
        endpoints={s:[h.host_record(json.loads((folder/f'host-{i}-{s}.process.json').read_text(encoding='utf-8-sig')),plan)
                      for i in range(3)] for s in 'AB'}
        for i in range(3):m.require_comparable(endpoints['A'][i],endpoints['B'][i],'request_to_ready_us')
        f.save(folder/'host-endpoints.json',{s:[r['statistics'] for r in endpoints[s]] for s in 'AB'})
        after=environment();f.save(folder/'environment-after.json',after)
        before=json.loads((folder/'environment-before.json').read_text(encoding='utf-8'))
        if after['power_scheme']!=before['power_scheme'] or after['allowed_affinity']!=before['allowed_affinity']:
            raise ValueError('Power scheme or allowed CPU set changed')
        f.verify(plan)
        result['criteria']=decision(result['scale'],result['host'])
        result['self_check_met']=all(result['criteria'].values())
    except Exception as error:
        result['errors'].append(str(error));result['self_check_met']=False
        raise
    finally:
        f.save(folder/'analysis.json',result)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command',choices=['bind','run'])
    parser.add_argument('--output',type=Path,required=True)
    for name in ('validation','f1-source','f1-build','candidate-source','candidate-build'):
        parser.add_argument('--'+name,type=Path)
    args=parser.parse_args()
    {'bind':bind,'run':run}[args.command](args)

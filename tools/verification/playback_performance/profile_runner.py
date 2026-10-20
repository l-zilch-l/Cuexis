"""Finite B-only PD5 pilot and immutable three-window runner.

Normal timing, stage observers and memory observers remain separate processes.
Unresolved preparation blockers prohibit registration; no automatic retries.
"""
import argparse
import datetime as dt
import os
from pathlib import Path
import sys
import time
import uuid

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[3]))
from tools.verification.playback_performance import measurement as measure
from tools.verification.playback_performance import owner_runner as owner
from tools.verification.playback_performance import prepare_profiles as prepare
from tools.verification.playback_performance import process_control as process
from tools.verification.playback_performance import window_protocol as protocol


def utc():
    return dt.datetime.now(dt.timezone.utc).isoformat()


def command(manifest):
    harness = manifest['profile'].get('harness')
    name = {'presentation-matrix':'PD5 presentation workload matrix',
            'renderer-matrix':'PD5 renderer and capture workload matrix'}.get(harness, 'PD5 scripted host timing probe')
    return [manifest['build']['executable'], name, '--rng-seed', '1', '--reporter', 'compact']


def probe_environment(profile, *, smoke=False, gpu_timer=False):
    env = {k:v for k,v in os.environ.items() if not k.startswith(('CUEXIS_PD_', 'CUEXIS_PD5_'))}
    env.update(CUEXIS_PD5_MODE=profile['mode'], CUEXIS_PD5_FRAMES=str(profile['frames']),
               CUEXIS_PD5_WARMUP=str(profile['warmup_frames']), CUEXIS_PD5_CYCLES=str(profile['cycles']),
               CUEXIS_PD5_RATE=str(profile['rate']), CUEXIS_PD5_SMOKE='1' if smoke else '0',
               CUEXIS_PD5_GPU_TIMER='1' if gpu_timer else '0')
    return env


def validate_identity(manifest, data, *, smoke=False):
    p, identity = manifest['profile'], data['identity']
    expected = {k:p[k] for k in ('mode','frames','warmup_frames')}
    expected.update(smoke=smoke, audio=False, source_step_ticks=7000, source_tick_rate=1000000)
    matrix = p.get('harness') in ('presentation-matrix','renderer-matrix')
    combined = p.get('harness') == 'renderer-matrix'
    if not matrix:
        expected.update(cycles=p['cycles'], rate=p['rate'])
    else:
        expected.update(harness=p['harness'], updates=p['frames']*p['update_ratio'],
                        materializations=p['frames']*(p['capture_consumers']+int(combined)),
                        capture_consumers=p['capture_consumers'], renderer_consumers=int(combined))
    if any(identity.get(k) != v for k,v in expected.items()):
        raise ValueError('Actual trajectory differs from prepared profile')
    if identity.get('counts') != prepare.expected_counts(p, smoke=smoke):
        raise ValueError('Actual sample counts differ from prepared trajectory')
    if p.get('measurement_metrics') and set(p['measurement_metrics']) != set(data['statistics']):
        raise ValueError('Missing or undeclared measurement boundary')
    return data


def signature(data):
    # Frame counts, work counts, exact outputs and devices are all frozen by the pilot.
    return {key:value for key,value in data['identity'].items() if key not in
            ('payload_peak_count','payload_peak_bytes','journal_peak_bytes')}


def registered_units(prepared):
    units = []
    for item in prepared['manifests']:
        profile = prepare.verify(item['path'])['profile']
        if profile['formal_blockers']:
            raise ValueError('Unresolved profile blockers: '+ '; '.join(profile['formal_blockers']))
        units.append({'id':item['profile'],'kind':'probe','comparison':'B-only','config':profile['config'].lower(),
            'metrics':profile['primary_metrics'], 'mean_noise_metrics':profile.get('mean_noise_metrics',[]),
            'budget_metrics':profile.get('budget_metrics',[])})
    return units


def launch(folder, manifest, warm_binary, name):
    env = probe_environment(manifest['profile'])
    commands = [('warmup', [warm_binary, 'PD machine CPU warmup', '--rng-seed','1','--reporter','compact']),
                ('measure', command(manifest))]
    artifacts = {}
    for label, cmd in commands:
        path = folder / (name + '-' + label + '.log')
        try:
            with path.open('xb') as out:
                result = process.run(cmd, process.affinity(), cwd=prepare.ROOT, env=env,
                                     output=out, timeout=900)
            record = {'path':str(path), 'sha256':prepare.sha(path), 'command':cmd, 'exit_code':result.returncode}
        except Exception as error:
            record = {'path':str(path), 'sha256':prepare.sha(path), 'command':cmd,
                      'exit_code':None, 'error':str(error)}
            protocol.write_once(folder/(name+'-'+label+'-process.json'),record)
            raise
        protocol.write_once(folder/(name+'-'+label+'-process.json'),record)
        artifacts[label] = record
        if result.returncode:
            raise RuntimeError('Failed child; original output retained: ' + str(path))
        if label == 'warmup':
            rows = measure.records(path.read_text(encoding='utf-8-sig'), 'PD_MACHINE_WARMUP ')
            if len(rows) != 1 or rows[0].get('seconds') != 30:
                raise ValueError('Incomplete machine warmup')
    data = validate_identity(manifest, measure.collect(artifacts['measure']['path'], 'probe'))
    data['environment'] = owner.native().native_environment(Path(artifacts['measure']['path']).read_text(encoding='utf-8-sig').splitlines())
    if any(row['process_affinity'] != process.affinity() for row in data['environment']):
        raise ValueError('Child scheduling differs from default allowed CPU set')
    record = {'commands':artifacts, 'data':data, 'acceptance':False, 'completed_utc':utc()}
    path = folder/(name+'.json')
    protocol.write_once(path,record)
    return record,path


def verify_preparation(folder):
    data = protocol.read(folder/'preparation.json')
    if data.get('schema') != 'pd5.profile-preparation.v1':
        raise ValueError('Unknown preparation')
    for item in data['manifests']:
        if prepare.sha(item['path']) != item['sha256']:
            raise ValueError('Profile preparation changed')
        prepare.verify(item['path'])
    binding = data['warmup_binding']
    current = prepare.build_binding(Path(binding['executable']).parent.parent,
        Path(binding['executable']).name, [x['source'] for x in binding['objects']])
    if current != binding:
        raise ValueError('Warmup build changed')
    return data


def make_preparation(args):
    manifests = []
    for path in args.manifest:
        path = path.resolve()
        m = prepare.verify(path)
        manifests.append({'path':str(path),'sha256':prepare.sha(path),'profile':m['profile']['id']})
    if not manifests or len({x['profile'] for x in manifests}) != len(manifests):
        raise ValueError('Missing or duplicate profile')
    folder = args.output/('profiles-prepare-'+uuid.uuid4().hex[:12])
    folder.mkdir(parents=True)
    warmup = prepare.build_binding(args.build, 'cuexis_pd_baseline_probe.exe', ['tests/pd/measurement_method_tests.cpp'])
    protocol.write_once(folder/'preparation.json', {'schema':'pd5.profile-preparation.v1',
        'manifests':manifests, 'warmup_binding':warmup, 'created_utc':utc(), 'formal_registration':False})
    print(folder)


def pilot(args):
    prepared = verify_preparation(args.preparation)
    environment = owner.environment()
    folder = args.preparation/('pilot-'+uuid.uuid4().hex[:12])
    folder.mkdir()
    protocol.write_once(args.preparation/'pilot.claim.json', {'folder':str(folder),'created_utc':utc()})
    runs = []
    try:
        for item in prepared['manifests']:
            owner.same_environment(environment,owner.environment())
            verify_preparation(args.preparation)
            manifest = prepare.verify(item['path'])
            record,path = launch(folder,manifest,prepared['warmup_binding']['executable'],item['profile'])
            runs.append({'profile':item['profile'],'path':str(path),'sha256':prepare.sha(path),
                         'identity':signature(record['data']), 'scheduling':owner.scheduling(record['data'])})
        owner.same_environment(environment,owner.environment())
        verify_preparation(args.preparation)
    except Exception as error:
        protocol.write_once(folder/'pilot.json', {'status':'stopped_error','error':str(error),'runs':runs,'acceptance':False})
        raise
    protocol.write_once(folder/'pilot.json', {'schema':'pd5.profile-pilot.v1','status':'pilot_complete',
        'preparation_sha256':prepare.sha(args.preparation/'preparation.json'), 'environment':environment,
        'runs':runs,'acceptance':False})
    print(folder/'pilot.json')


def verify_pilot(path, folder):
    prepared = verify_preparation(folder)
    pilot_data = protocol.read(path)
    if pilot_data.get('schema') != 'pd5.profile-pilot.v1' or pilot_data.get('status') != 'pilot_complete' or pilot_data.get('preparation_sha256') != prepare.sha(folder/'preparation.json'):
        raise ValueError('A complete matching one-time pilot is required')
    if len(pilot_data['runs']) != len(prepared['manifests']):
        raise ValueError('Incomplete pilot')
    for run,item in zip(pilot_data['runs'],prepared['manifests']):
        if run['profile'] != item['profile'] or prepare.sha(run['path']) != run['sha256']:
            raise ValueError('Pilot order or original process record changed')
        record = protocol.read(run['path'])
        for artifact in record['commands'].values():
            if artifact['exit_code'] != 0 or prepare.sha(artifact['path']) != artifact['sha256']:
                raise ValueError('Pilot raw evidence changed')
        warm = measure.records(Path(record['commands']['warmup']['path']).read_text(encoding='utf-8-sig'),'PD_MACHINE_WARMUP ')
        if len(warm) != 1 or warm[0].get('seconds') != 30:
            raise ValueError('Pilot warmup incomplete')
        parsed = measure.collect(record['commands']['measure']['path'],'probe')
        validate_identity(prepare.verify(item['path']),parsed)
        if any(record['data'].get(k) != v for k,v in parsed.items()) or signature(parsed) != run['identity']:
            raise ValueError('Pilot parsed evidence differs from original')
        parsed['environment'] = owner.native().native_environment(Path(record['commands']['measure']['path']).read_text(encoding='utf-8-sig').splitlines())
        if owner.scheduling(parsed) != run['scheduling']:
            raise ValueError('Pilot scheduling differs from original')
    return prepared,pilot_data


def register(args):
    prepared,pilot_data = verify_pilot(args.pilot,args.preparation)
    if not args.environment_declaration.strip():
        raise ValueError('Fresh owner environment declaration required')
    owner.same_environment(pilot_data['environment'],owner.environment())
    units = registered_units(prepared)
    folder = args.preparation/('registration-'+uuid.uuid4().hex[:12])
    folder.mkdir()
    protocol.write_once(args.preparation/'registration.claim.json', {'folder':str(folder),'pilot':str(args.pilot),'pilot_sha256':prepare.sha(args.pilot)})
    reg = {'schema':'pd5.windows.v1','accepted_profile':True,'windows':3,'pairs_per_window':3,'gap_seconds':600,
        'noise_limit':.15,'regression_limit':1.10,'units':units,'created_utc':utc(),
        'preparation':str(args.preparation),'preparation_sha256':prepare.sha(args.preparation/'preparation.json'),
        'pilot':str(args.pilot),'pilot_sha256':prepare.sha(args.pilot),'environment':pilot_data['environment'],
        'environment_declaration':args.environment_declaration,
        'expected_identities':{x['profile']:x['identity'] for x in pilot_data['runs']},
        'expected_scheduling':{x['profile']:x['scheduling'] for x in pilot_data['runs']}}
    protocol.validate_registration(reg)
    protocol.write_once(folder/'registration.json',reg)
    protocol.seal_registration(folder)
    print(folder)


def window(args):
    folder = args.registration
    reg = protocol.read_registration(folder)
    prepared,pilot_data = verify_pilot(Path(reg['pilot']),Path(reg['preparation']))
    if reg['units'] != registered_units(prepared):
        raise ValueError('Registered workload or metric set differs from frozen preparation')
    if prepare.sha(reg['pilot']) != reg['pilot_sha256'] or prepare.sha(Path(reg['preparation'])/'preparation.json') != reg['preparation_sha256']:
        raise ValueError('Registered inputs changed')
    owner.same_environment(reg['environment'],owner.environment())
    claim = protocol.claim(folder,args.number,utc(),time.monotonic())
    work = folder/('window-'+str(args.number))
    work.mkdir()
    rows = []
    try:
        for target in claim['schedule']:
            owner.same_environment(reg['environment'],owner.environment())
            verify_preparation(Path(reg['preparation']))
            item = next(x for x in prepared['manifests'] if x['profile'] == target['unit'])
            unit = next(x for x in reg['units'] if x['id'] == target['unit'])
            record,path = launch(work,prepare.verify(item['path']),prepared['warmup_binding']['executable'],target['unit']+'-'+str(target['pair']))
            data = record['data']
            if signature(data) != reg['expected_identities'][target['unit']] or owner.scheduling(data) != reg['expected_scheduling'][target['unit']]:
                raise ValueError('Formal trajectory/output/scheduling differs from finite pilot')
            rows.append({**target,'exit_code':0,'execution_valid':True,'record':str(path),
                'record_sha256':prepare.sha(path),'raw_log_sha256':record['commands']['measure']['sha256'],
                'values':{m:data['statistics'][m]['median_us'] for m in unit['metrics']},
                'mean_values':{m:data['statistics'][m]['mean_us'] for m in unit.get('mean_noise_metrics',[])}})
        result = {'status':'collected','window':args.number,'registration_sha256':prepare.sha(folder/'registration.json'),
            'claim_sha256':prepare.sha(folder/f'window-{args.number}.claim.json'),'runs':rows,
            'ended_utc':utc(),'ended_monotonic':time.monotonic()}
        owner.same_environment(reg['environment'],owner.environment())
        verify_preparation(Path(reg['preparation']))
        protocol.validate_window(reg,claim,result)
        protocol.write_once(folder/f'window-{args.number}.result.json',result)
    except Exception as error:
        protocol.write_once(work/'partial-result.json', {'runs':rows,'error':str(error),'acceptance':False})
        protocol.write_once(folder/'stopped.json', {'window':args.number,'error':str(error),'utc':utc()})
        raise
    if args.number == 3:
        protocol.write_once(folder/'summary.json', protocol.summarize(reg,protocol.load_windows(folder)))
    print('Window collected: '+str(args.number))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='action',required=True)
    p = sub.add_parser('prepare')
    p.add_argument('--manifest',type=Path,action='append',required=True)
    p.add_argument('--build',type=Path,default=prepare.ROOT/'out/build/pd5-ready-release')
    p.add_argument('--output',type=Path,default=prepare.ROOT/'out/pd5-profiles')
    p = sub.add_parser('pilot'); p.add_argument('--preparation',type=Path,required=True)
    p = sub.add_parser('register'); p.add_argument('--preparation',type=Path,required=True)
    p.add_argument('--pilot',type=Path,required=True); p.add_argument('--environment-declaration',required=True)
    p = sub.add_parser('window'); p.add_argument('--registration',type=Path,required=True)
    p.add_argument('--number',type=int,choices=(1,2,3),required=True)
    args = parser.parse_args()
    {'prepare':make_preparation,'pilot':pilot,'register':register,'window':window}[args.action](args)


if __name__ == '__main__':
    main()

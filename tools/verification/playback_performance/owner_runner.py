"""Prepare/pilot/register/run the fixed PD5 owner/common-host comparison.

This is the finite P0 subset. Passing it never closes the complete PD5 matrix.
Historical A artifacts and all claims/results are immutable.
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
import zipfile

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[3]))

from tools.verification.playback_performance import measurement as measure
from tools.verification.playback_performance import prepare_profiles as prepare
from tools.verification.playback_performance import window_protocol as protocol
from tools.verification.playback_performance import process_control

ROOT=Path(__file__).resolve().parents[3]
OLD=ROOT/'out/pd0-fresh-bound-baseline-20261018'


def utc():return dt.datetime.now(dt.timezone.utc).isoformat()


def native():
    # Reuse the frozen PD0 support; its imports are still local to tests/pd.
    legacy_support = str(ROOT / 'tests/pd')
    if legacy_support not in sys.path:
        sys.path.insert(0, legacy_support)
    import formal_baseline
    return formal_baseline


def same_environment(expected,actual):
    native().same_environment(expected,actual)
    if native().topology(actual['machine'])!=expected['topology']:
        raise ValueError('Physical-core topology changed')


def environment():
    f=native();result=f.environment()
    result['topology']=f.topology(result['machine'])
    result['no_trace']=f.no_trace()
    return result


def old_binding(config,kind):
    manifest=prepare.load(OLD/'frozen-evidence-manifest.json')
    name='cuexis_pd_'+kind+'_probe.exe'
    key='pd0-formal-'+config+'\\bin\\'+name
    item=manifest['binary_and_build_metadata_files'][key]
    path=OLD/'frozen-binaries'/key.replace('\\','/')
    if prepare.sha(path)!=item['sha256']:
        raise ValueError('Frozen old executable changed')
    archive=Path(manifest['implementation_archive']['path'])
    if prepare.sha(archive)!=manifest['implementation_archive']['sha256']:
        raise ValueError('Frozen old implementation archive changed')
    # Both binaries read these assets from their compiled source directory.
    # Verify against A's archived bytes, not just the live B inventory.
    assets={}
    with zipfile.ZipFile(archive) as z:
        for path_name in z.namelist():
            if path_name.replace('\\','/').startswith('assets/projects/stage3_project/') and not path_name.endswith('/'):
                digest=hashlib.sha256(z.read(path_name)).hexdigest()
                current=ROOT/path_name.replace('\\','/')
                if prepare.sha(current)!=digest:raise ValueError('Historical runtime asset changed: '+path_name)
                assets[path_name]=digest
    if not assets:raise ValueError('Historical runtime asset inventory is empty')
    return {'executable':str(path),'executable_sha256':item['sha256'],
            'implementation_archive':str(archive),'implementation_archive_sha256':prepare.sha(archive),
            'implementation_fingerprint':manifest['implementation_sha256'],'runtime_assets':assets}


def verify(folder):
    manifest=prepare.load(Path(folder)/'preparation.json')
    if manifest.get('schema')!='pd5.owner-preparation.v1' or prepare.inventory()!=manifest['source']:
        raise ValueError('Preparation source identity changed')
    for config in ('release','debug'):
        for kind in ('baseline','host'):
            saved=manifest['builds'][config][kind]
            if old_binding(config,kind)!=saved['A']:raise ValueError('Historical binding changed')
            current=prepare.build_binding(Path(saved['B']['executable']).parent.parent,
                Path(saved['B']['executable']).name,[saved['B']['objects'][0]['source']])
            if current!=saved['B']:raise ValueError('Current transitive build identity changed')
    return manifest


def make_preparation(args):
    folder=args.output/('owner-prepare-'+uuid.uuid4().hex[:12]);folder.mkdir(parents=True)
    builds={}
    for config,build in [('release',args.release),('debug',args.debug)]:
        builds[config]={}
        for kind,source in [('baseline','measurement_method_tests.cpp'),('host','host_delivery_probe.cpp')]:
            b=prepare.build_binding(build,'cuexis_pd_'+kind+'_probe.exe',['tests/pd/'+source])
            cache=(build/'CMakeCache.txt').read_text(encoding='utf-8-sig')
            if 'CMAKE_BUILD_TYPE:STRING='+config.title() not in cache:
                raise ValueError('Debug/Release configuration mismatch')
            builds[config][kind]={'A':old_binding(config,kind),'B':b}
    data={'schema':'pd5.owner-preparation.v1','scope':'P0 fixed legacy/RPA + common short host',
          'source':prepare.inventory(),'builds':builds,'formal_registration':False,
          'initial_reads':{'release':2097152,'debug':32768},'created_utc':utc()}
    protocol.write_once(folder/'preparation.json',data)
    print(folder,flush=True)


def units():
    owner=[{'id':config+'-'+profile,'config':config,'profile':profile,'kind':'owner',
            'metrics':['validated_trace','playback_only','read_long']} for config,profile in
           [('release','legacy'),('debug','rpa'),('release','rpa'),('debug','legacy')]]
    host=[{'id':config+'-host-common','config':config,'kind':'host',
           'metrics':['request_to_ready_us'],'mean_noise_metrics':['request_to_ready_us']}
          for config in ('release','debug')]
    return owner+host


def launch(folder,manifest,unit,implementation,phase,reads,affinity,name):
    f=native();config=unit['config'];kind='baseline' if unit['kind']=='owner' else 'host'
    binary=manifest['builds'][config][kind][implementation]['executable']
    warm_binary=manifest['builds'][config]['baseline'][implementation]['executable']
    env=os.environ.copy()
    for key in list(env):
        if key.startswith(('CUEXIS_PD_','CUEXIS_PD5_')):env.pop(key)
    mask=affinity if unit['kind']=='owner' else f.base.machine()['parent_affinity']
    commands=[('warmup',[warm_binary,'PD machine CPU warmup','--rng-seed','1','--reporter','compact']),
              ('measure',[binary,'PD uninstrumented validated fixed workload' if unit['kind']=='owner' else
                         'PD host delivery and receipt acknowledgement baseline','--rng-seed','1','--reporter','compact'])]
    env.update(CUEXIS_PD_PROFILE=unit.get('profile','legacy'),CUEXIS_PD_FORMAL_PHASE=phase,CUEXIS_PD_V2_READS=str(reads))
    outputs={}
    for label,command in commands:
        log=folder/(name+'-'+label+'.log')
        try:
            with log.open('xb') as stream:
                completed=process_control.run(command,mask,cwd=ROOT,env=env,output=stream,timeout=180)
        except Exception as error:
            protocol.write_once(folder/(name+'-'+label+'-failed.json'),{'command':command,'mask':mask,
                'path':str(log),'sha256':prepare.sha(log),'error':str(error),'acceptance':False})
            raise
        outputs[label]={'command':command,'exit_code':completed.returncode,'sha256':prepare.sha(log),'path':str(log)}
        protocol.write_once(folder/(name+'-'+label+'-process.json'),
            {**outputs[label],'mask':mask,'launch_policy':'pd5-process-job-handshake.v1','acceptance':False})
        if completed.returncode:raise RuntimeError('Child failed; original output preserved: '+str(log))
        if label=='warmup':
            records=measure.records(log.read_text(encoding='utf-8',errors='strict'),'PD_MACHINE_WARMUP ')
            if len(records)!=1 or records[0].get('seconds')!=30:raise ValueError('Incomplete machine warmup')
    log=Path(outputs['measure']['path'])
    data=measure.collect(log,unit['kind'],0,phase)
    lines=log.read_text(encoding='utf-8',errors='strict').splitlines()
    data['environment']=f.native_environment(lines)
    for record in data['environment']:
        if record.get('process_affinity')!=mask:raise ValueError('Unexpected child CPU affinity')
    if unit['kind']=='owner':
        from system_runtime_support import validate_alignment
        data['alignment']=validate_alignment([x['sample'] for x in data['samples']],measure.records('\n'.join(lines),'PD_QPC_ANCHOR '))
    result={'commands':outputs,'data':data,'completed_utc':utc(),'acceptance':False}
    path=folder/(name+'.json');protocol.write_once(path,result)
    return result,path


def signature(data):
    keys=('digest','replay_sha256','snapshot_sha256','packed_sha256','profile') if data['schema']=='pd5.owner-fixed.v1' else (
          'digest','source','viewport','objects','warmup','measured','audio','gpu')
    return {key:data['identity'][key] for key in keys}


def scheduling(data):
    keys=('process_priority','thread_priority','process_power_error','process_power_control','process_power_state',
          'thread_power_error','thread_power_control','thread_power_state')
    return {key:data['environment'][0][key] for key in keys}


def pilot(args):
    preparation=verify(args.preparation);env=environment()
    folder=args.preparation/('pilot-'+uuid.uuid4().hex[:12]);folder.mkdir()
    # Claim once before any samples. Failure cannot be replaced under the same preparation.
    protocol.write_once(args.preparation/'pilot.claim.json',{'folder':str(folder),'created_utc':utc()})
    core=native().select_registered_mask(env['topology']);runs=[];read_counts={}
    manifest={'schema':'pd5.owner-pilot.v1','status':'running','preparation_sha256':prepare.sha(args.preparation/'preparation.json'),
              'environment':env,'selected_mask':core,'runs':runs,'registered':False}
    expected_identities={};expected_scheduling={}
    try:
        for unit in units():
            pair=[]
            for implementation in ('A','B'):
                same_environment(env,environment());verify(args.preparation)
                result,path=launch(folder,preparation,unit,implementation,'pilot',
                    preparation['initial_reads'][unit['config']],core,unit['id']+'-'+implementation)
                runs.append({'unit':unit['id'],'implementation':implementation,'path':str(path),'sha256':prepare.sha(path)})
                pair.append(result['data'])
            for metric in unit['metrics']:measure.require_comparable(*pair,metric)
            if scheduling(pair[0])!=scheduling(pair[1]):raise ValueError('Pilot A/B scheduling mismatch')
            expected_identities[unit['id']]=signature(pair[0]);expected_scheduling[unit['id']]=scheduling(pair[0])
            if unit['kind']=='owner':
                import math
                costs=[row['sample']['wall_us']/row['operations'] for data in pair for row in data['samples'] if row['kind']=='read_long']
                count=1<<(max(4096,math.ceil(312500/min(costs)))-1).bit_length()
                if count>4194304:raise ValueError('Required read batch exceeds finite cap')
                read_counts[unit['id']]=count
        same_environment(env,environment());verify(args.preparation)
        manifest.update(status='pilot_complete',read_counts=read_counts,completed_utc=utc(),
                        expected_identities=expected_identities,expected_scheduling=expected_scheduling)
    except Exception as error:
        manifest.update(status='stopped_error',error=str(error),completed_utc=utc())
        protocol.write_once(folder/'pilot.json',manifest);raise
    protocol.write_once(folder/'pilot.json',manifest);print(folder/'pilot.json',flush=True)


def verify_pilot(path, preparation):
    pilot_data=prepare.load(path)
    if pilot_data.get('status')!='pilot_complete' or pilot_data.get('preparation_sha256')!=prepare.sha(preparation/'preparation.json'):
        raise ValueError('A complete, matching finite pilot is required')
    targets=[(unit,implementation) for unit in units() for implementation in ('A','B')]
    if len(pilot_data.get('runs',[]))!=len(targets):raise ValueError('Incomplete finite pilot')
    for run,(unit,implementation) in zip(pilot_data['runs'],targets):
        if run['unit']!=unit['id'] or run['implementation']!=implementation or prepare.sha(run['path'])!=run['sha256']:
            raise ValueError('Pilot process order/record changed')
        record=prepare.load(run['path'])
        for command in record['commands'].values():
            if command['exit_code']!=0 or prepare.sha(command['path'])!=command['sha256']:
                raise ValueError('Pilot original output changed or failed')
        data=measure.collect(record['commands']['measure']['path'],unit['kind'],0,'pilot')
        if any(record['data'].get(key)!=value for key,value in data.items()):
            raise ValueError('Pilot parsed record differs from raw output')
        if signature(data)!=pilot_data['expected_identities'][unit['id']]:
            raise ValueError('Pilot exact identity changed')
        data['environment']=native().native_environment(Path(record['commands']['measure']['path']).read_text(encoding='utf-8-sig').splitlines())
        if scheduling(data)!=pilot_data['expected_scheduling'][unit['id']]:raise ValueError('Pilot scheduling changed')
    return pilot_data


def register(args):
    verify(args.preparation);pilot_data=verify_pilot(args.pilot,args.preparation)
    if pilot_data.get('status')!='pilot_complete' or pilot_data.get('preparation_sha256')!=prepare.sha(args.preparation/'preparation.json'):
        raise ValueError('A complete, matching finite pilot is required')
    same_environment(pilot_data['environment'],environment())
    if not args.environment_declaration.strip():raise ValueError('Current owner environment declaration required')
    folder=args.preparation/('registration-'+uuid.uuid4().hex[:12]);folder.mkdir()
    protocol.write_once(args.preparation/'registration.claim.json',
        {'folder':str(folder),'pilot':str(args.pilot),'pilot_sha256':prepare.sha(args.pilot),'created_utc':utc()})
    reg={'schema':'pd5.windows.v1','accepted_profile':True,'windows':3,'pairs_per_window':3,'gap_seconds':600,
         'noise_limit':.15,'regression_limit':1.10,'units':units(),'created_utc':utc(),
         'preparation':str(args.preparation),'preparation_sha256':prepare.sha(args.preparation/'preparation.json'),
         'pilot':str(args.pilot),'pilot_sha256':prepare.sha(args.pilot),'selected_mask':pilot_data['selected_mask'],
         'read_counts':pilot_data['read_counts'],'environment':pilot_data['environment'],
         'expected_identities':pilot_data['expected_identities'],'expected_scheduling':pilot_data['expected_scheduling'],
         'environment_declaration':args.environment_declaration,'scope':'P0 subset only; full PD5 not complete'}
    protocol.validate_registration(reg);protocol.write_once(folder/'registration.json',reg)
    protocol.seal_registration(folder)
    print(folder,flush=True)


def window(args):
    folder=args.registration;reg=protocol.read_registration(folder)
    if reg['units'] != units():
        raise ValueError('Registered owner workload or metric set changed')
    manifest=verify(Path(reg['preparation']))
    if prepare.sha(Path(reg['preparation'])/'preparation.json')!=reg['preparation_sha256'] or prepare.sha(reg['pilot'])!=reg['pilot_sha256']:
        raise ValueError('Registration inputs changed')
    same_environment(reg['environment'],environment())
    verify_pilot(Path(reg['pilot']),Path(reg['preparation']))
    claim=protocol.claim(folder,args.number,utc(),time.monotonic())
    work=folder/('window-'+str(args.number));work.mkdir()
    rows=[]
    try:
        for target in claim['schedule']:
            unit=next(x for x in reg['units'] if x['id']==target['unit'])
            same_environment(reg['environment'],environment());verify(Path(reg['preparation']))
            name=unit['id']+'-'+str(target['pair'])+'-'+target['implementation']
            data,path=launch(work,manifest,unit,target['implementation'],'formal',reg['read_counts'].get(unit['id'],4096),reg['selected_mask'],name)
            if signature(data['data'])!=reg['expected_identities'][unit['id']] or scheduling(data['data'])!=reg['expected_scheduling'][unit['id']]:
                raise ValueError('Formal output/scheduling identity differs from pilot')
            values={metric:data['data']['statistics'][metric]['median_us'] for metric in unit['metrics']}
            rows.append({**target,'exit_code':0,'execution_valid':True,'record':str(path),
                'record_sha256':prepare.sha(path),'raw_log_sha256':data['commands']['measure']['sha256'],'values':values,
                'mean_values':{metric:data['data']['statistics'][metric]['mean_us'] for metric in unit.get('mean_noise_metrics',[])}})
            # Identity failure stops immediately; performance is judged only after the fixed group.
            if len(rows)>=2 and rows[-2]['unit']==target['unit'] and rows[-2]['pair']==target['pair']:
                prior=prepare.load(rows[-2]['record'])['data']
                for metric in unit['metrics']:measure.require_comparable(prior,data['data'],metric)
        result={'status':'collected','window':args.number,'registration_sha256':prepare.sha(folder/'registration.json'),
                'claim_sha256':prepare.sha(folder/f'window-{args.number}.claim.json'),
                'runs':rows,'ended_utc':utc(),'ended_monotonic':time.monotonic()}
        same_environment(reg['environment'],environment());verify(Path(reg['preparation']))
        protocol.validate_window(reg,claim,result)
        protocol.write_once(folder/f'window-{args.number}.result.json',result)
    except Exception as error:
        protocol.write_once(work/'partial-result.json',{'runs':rows,'error':str(error),'status':'stopped_error'})
        protocol.write_once(folder/'stopped.json',{'window':args.number,'error':str(error),'utc':utc()});raise
    if args.number==3:
        results=protocol.load_windows(folder)
        summary=protocol.summarize(reg,results);protocol.write_once(folder/'summary.json',summary)
    print('Window collected without replacement samples: '+str(args.number),flush=True)


def main():
    p=argparse.ArgumentParser(description=__doc__);sub=p.add_subparsers(dest='action',required=True)
    a=sub.add_parser('prepare');a.add_argument('--release',type=Path,default=ROOT/'out/build/pd5-preparation-release')
    a.add_argument('--debug',type=Path,default=ROOT/'out/build/pd5-preparation-debug')
    a.add_argument('--output',type=Path,default=ROOT/'out/pd5-owner')
    a=sub.add_parser('pilot');a.add_argument('--preparation',type=Path,required=True)
    a=sub.add_parser('register');a.add_argument('--preparation',type=Path,required=True);a.add_argument('--pilot',type=Path,required=True)
    a.add_argument('--environment-declaration',required=True)
    a=sub.add_parser('window');a.add_argument('--registration',type=Path,required=True);a.add_argument('--number',type=int,choices=[1,2,3],required=True)
    args=p.parse_args();{'prepare':make_preparation,'pilot':pilot,'register':register,'window':window}[args.action](args)


if __name__=='__main__':main()

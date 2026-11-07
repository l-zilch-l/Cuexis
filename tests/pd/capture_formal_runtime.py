"""Register one bounded CPU/Power diagnosis of the frozen uninstrumented probes."""
import argparse
import ctypes
import json
from pathlib import Path
import shutil
import subprocess
import uuid
import xml.etree.ElementTree as ET

import formal_baseline as formal

ORDER = [('debug', 'rpa'), ('release', 'rpa'), ('debug', 'legacy')]


def capture_transaction(command, work, trace, instance, profile=None):
    """Only stop the uniquely named recording after a confirmed successful start."""
    before = command('wpr-before', ['wpr', '-status'])
    if before.returncode or b'WPR is not recording' not in before.stdout:
        raise RuntimeError('WPR active or unknown; existing recordings left untouched')
    profiles = ['-start', profile] if profile else ['-start', 'CPU', '-start', 'Power']
    started = command('wpr-start', ['wpr', *profiles, '-filemode', '-instancename', instance])
    if started.returncode:
        raise RuntimeError('WPR start failed; no automatic retry or cancel; inspect named session')
    try:
        return work()
    finally:
        stopped = command('wpr-stop', ['wpr', '-stop', str(trace), '-skipPdbGen',
                                      '-compress', '-instancename', instance])
        if stopped.returncode:
            raise RuntimeError('WPR stop failed; retain logs and inspect named session: ' + instance)


def require_binding(registration):
    source, binaries = formal.base.identity(), formal.binaries()
    expected = registration['source']
    if (source['head'], source['implementation_sha256']) != (
            expected['head'], expected['implementation_sha256']) or binaries != registration['binaries']:
        raise ValueError('Registered implementation or binary changed; no sampling started')
    for artifact in (registration['wpr_profile'], registration['wpr_binary']):
        if formal.base.sha(artifact['path']) != artifact['sha256']:
            raise ValueError('Registered WPR tool/profile changed; no sampling started')
    formal.same_environment(registration['environment'], formal.environment())


def prepare(args):
    reference = formal.load(args.formal_registration)
    binaries = formal.binaries()
    if binaries != reference['binaries']:
        raise ValueError('Frozen formal binary changed')
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    profile = output / ('CPU-Power-' + uuid.uuid4().hex + '.wprp')
    subprocess.run(['wpr', '-exportprofile', 'CPU+Power', str(profile), '-filemode'],
                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=True, timeout=30)
    xml = ET.parse(profile)
    keywords = {node.attrib['Value'] for node in xml.findall('.//SystemProvider/Keywords/Keyword')}
    if not {'CSwitch', 'ReadyThread', 'SampledProfile', 'Power', 'DPC', 'Interrupt'} <= keywords:
        raise ValueError('WPR export lacks required scheduling/power coverage')
    profiles = xml.findall('.//Profile')
    if len(profiles) != 1:
        raise ValueError('Expected one combined CPU/Power profile')
    wpr = shutil.which('wpr')
    if not wpr:
        raise ValueError('WPR executable not found')
    registration = {'revision': 'pd0-uninstrumented-cpu-power-v1', 'registered_utc': formal.utc(),
        'source': formal.base.identity(), 'binaries': binaries, 'environment': formal.environment(),
        'reference': {'path': str(args.formal_registration.resolve()),
                      'sha256': formal.base.sha(args.formal_registration)},
        'wpr_profile': {'path': str(profile), 'sha256': formal.base.sha(profile),
                        'name': profiles[0].attrib['Name'], 'system_keywords': sorted(keywords)},
        'wpr_binary': {'path': wpr, 'sha256': formal.base.sha(wpr)},
        'order': ORDER, 'selected_mask': reference['selected_mask'],
        'read_counts': reference['read_counts'], 'profiles': ['CPU', 'Power'],
        'machine_warmup_seconds': 30, 'counts_per_metric_per_process': 8,
        'phase': 'formal', 'processes': len(ORDER), 'retry': False, 'pd0_exit': False,
        'question': 'Correlate within-process slow blocks with scheduling, sibling execution, CPU stacks and power events.',
        'capture_overhead': 'uncalibrated; diagnostic collection, never formal acceptance',
        'trace_analysis_status': 'not_collected', 'allocator': 'normal', 'stage_instrumentation': False}
    path = output / ('registration-' + uuid.uuid4().hex + '.json')
    with path.open('x', encoding='utf-8') as stream:
        json.dump(registration, stream, indent=2)
        stream.write('\n')
    print('REGISTRATION ' + str(path), flush=True)
    return 0


def capture(args):
    # Deny before creating a claim, starting warmup, or touching any trace session.
    if not ctypes.windll.shell32.IsUserAnAdmin():
        raise PermissionError('Administrator Windows terminal required; no claim or recording started')
    registration = formal.load(args.registration)
    require_binding(registration)
    if registration.get('revision') != 'pd0-uninstrumented-cpu-power-v1' or registration['order'] != [list(x) for x in ORDER]:
        raise ValueError('Unsupported diagnostic registration')
    formal.no_trace()
    claim = args.registration.with_suffix('.claim.json')
    with claim.open('x', encoding='utf-8') as stream:
        json.dump({'utc': formal.utc(), 'registration_sha256': formal.base.sha(args.registration)}, stream)
    output = formal.evidence(args.output.resolve(), 'runtime-capture')
    manifest = {'status': 'registered', 'registration': str(args.registration.resolve()),
                'registration_sha256': formal.base.sha(args.registration),
                'source': registration['source'], 'binaries': registration['binaries'],
                'runs': [], 'commands': [], 'pd0_exit': False, 'trace_analysis_status': 'not_analyzed'}
    def save():
        formal.save(output / 'manifest.json', manifest)
    def command(prefix, label, arguments):
        completed = subprocess.run(arguments, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                   cwd=formal.ROOT, timeout=180)
        path = output / (prefix + '-' + label + '.log')
        path.write_bytes(completed.stdout)
        manifest['commands'].append({'command': arguments, 'exit_code': completed.returncode,
                                      'log': path.name, 'sha256': formal.base.sha(path)})
        save()
        return completed
    save()
    try:
        profile = output / 'CPU-Power.wprp'
        shutil.copyfile(registration['wpr_profile']['path'], profile)
        manifest['warmup'] = formal.child(output, 'machine-warmup',
            registration['binaries']['release']['binaries']['baseline']['path'],
            registration['selected_mask'], warm=True)
        for index, (config, profile) in enumerate(registration['order'], 1):
            require_binding(registration)
            name = f'{index}-{config}-{profile}'
            instance = 'CuexisPD0_' + uuid.uuid4().hex
            trace = output / (name + '.etl')
            run = {'kind': 'single_owner', 'config': config, 'profile': profile,
                   'instance': instance, 'trace': trace.name, 'status': 'registered'}
            manifest['runs'].append(run)
            save()
            def work():
                data = formal.child(output, name,
                    registration['binaries'][config]['binaries']['baseline']['path'],
                    registration['selected_mask'], profile, reads=registration['read_counts'][config + '/' + profile])
                run['data'] = data
                save()
                details = command(name, 'wpr-collectors',
                                  ['wpr', '-status', 'collectors', '-details', '-instancename', instance])
                if details.returncode:
                    raise RuntimeError('Could not retain collector/loss status')
                return data
            capture_transaction(lambda label, argv: command(name, label, argv), work, trace, instance,
                                str(profile) + '!' + registration['wpr_profile']['name'])
            if not trace.is_file() or trace.stat().st_size == 0:
                raise RuntimeError('WPR reported success without a nonempty ETL')
            run.update(status='collected', trace_sha256=formal.base.sha(trace), trace_bytes=trace.stat().st_size)
            save()
        formal.validate_runs(manifest['runs'])
        require_binding(registration)
        manifest.update(status='diagnostic_collected', finished_utc=formal.utc())
    except BaseException as error:
        manifest.update(status='stopped_error', error=str(error), finished_utc=formal.utc())
        raise
    finally:
        save()
        formal.save(output / 'evidence-hashes.json', {p.name: formal.base.sha(p)
                    for p in output.iterdir() if p.is_file() and p.name != 'evidence-hashes.json'})
    print('STATUS diagnostic_collected; ETL loss, time coverage and slow-state analysis still required', flush=True)
    return 0


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['prepare', 'capture'])
    parser.add_argument('--formal-registration', type=Path)
    parser.add_argument('--registration', type=Path)
    parser.add_argument('--output', type=Path, default=formal.ROOT / 'out/pd0-uninstrumented-runtime')
    args = parser.parse_args()
    if args.action == 'prepare' and not args.formal_registration:
        parser.error('prepare requires --formal-registration')
    if args.action == 'capture' and not args.registration:
        parser.error('capture requires --registration')
    raise SystemExit(prepare(args) if args.action == 'prepare' else capture(args))

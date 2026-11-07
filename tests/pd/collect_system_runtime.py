"""Prepare and preregister bounded QPC-aligned CPU/Power ETW diagnostics."""
import argparse
import ctypes as ct
import datetime as dt
import json
from pathlib import Path
import subprocess as sp
import time
import uuid

import measure_scheduling as measurement
from system_runtime_support import power_policy, require_same_power, validate_alignment

base = measurement.base
ROOT = measurement.ROOT
CALIBRATION = ROOT / 'out/pd0-scheduling-v2/calibrate-20261017T043818Z-453e2a/manifest.json'
CONFIGURATIONS = [('debug', 'rpa'), ('release', 'legacy'), ('debug', 'legacy'), ('release', 'rpa')]
SEED = 49001


def utc():
    return dt.datetime.now(dt.timezone.utc).isoformat()


def load(path):
    return json.loads(Path(path).read_text(encoding='utf-8'))


def binaries():
    return {c: base.binary_info(Path(b['path'])) for c, b in load(CALIBRATION)['binaries'].items()}


def environment():
    machine = base.machine()
    return {'machine': machine, 'power_policy': power_policy(machine['ac_line_status'])}


def require_environment(expected, actual):
    require_same_power(expected['power_policy'], actual['power_policy'])
    keys = ['os', 'logical_processors', 'parent_affinity', 'selected_affinity', 'ac_line_status']
    if any(expected['machine'][k] != actual['machine'][k] for k in keys):
        raise ValueError('OS/affinity/AC identity changed')


def verify_record(output, name, record):
    log = (output / (name + '.log')).read_text(encoding='utf-8', errors='replace')
    anchors = [json.loads(line[len('PD_QPC_ANCHOR '):]) for line in log.splitlines() if line.startswith('PD_QPC_ANCHOR ')]
    samples = [record['clocks']['deliberate_wait']] + [r['sample'] for r in record['samples']]
    alignment = validate_alignment(samples, anchors)
    expected = [r['identity'] for r in load(CALIBRATION)['runs'] if r['identity']['profile'] == record['identity']['profile']]
    keys = ['digest', 'replay_sha256', 'snapshot_sha256', 'packed_sha256']
    if not expected or any(any(r[k] != record['identity'][k] for k in keys) for r in expected):
        raise ValueError('Semantic identity differs from the historical reference; stop diagnosis')
    alignment['anchors'] = anchors
    alignment['blocks'] = [{'kind': 'positive_sleep_control', 'group': None, 'active': False, 'operations': 1}] + [
        {k: r[k] for k in ['kind', 'group', 'active', 'operations']} for r in record['samples']]
    if len(alignment['intervals']) != len(alignment['blocks']):
        raise ValueError('Incomplete block alignment')
    base.save(output / (name + '-alignment.json'), alignment)
    return {'pid': alignment['pid'], 'tid': alignment['tid'], 'block_count': len(samples),
            'anchor_widths_us': alignment['anchor_widths_us'],
            'endpoint_mapping_drift_us': alignment['endpoint_mapping_drift_us']}


def main(args):
    if args.action == 'register':
        preparation = load(args.preparation)
        if preparation['status'] != 'alignment_verified':
            raise ValueError('Completed preparation required')
        source = base.identity()
        binary = binaries()
        if source['implementation_sha256'] != preparation['source']['implementation_sha256'] or binary != preparation['binaries']:
            raise ValueError('Prepared source/binaries changed')
        env = environment()
        require_environment(preparation['environment_before'], env)
        rounds = [CONFIGURATIONS[i:] + CONFIGURATIONS[:i] for i in range(3)]
        registration = {'revision': 'pd0-system-runtime-qpc-v1', 'status': 'registered', 'registered_utc': utc(),
            'source': source, 'binaries': binary, 'environment': env,
            'preparation': {'path': str(args.preparation.resolve()), 'sha256': base.sha(args.preparation)},
            'rounds': rounds, 'read_counts': load(CALIBRATION)['read_counts'], 'seed': SEED,
            'profiles': ['CPU', 'Power'], 'phase': 'run', 'processes': 12,
            'prefix_suffix_seconds': 5, 'retry': False, 'stop_on_error': True,
            'capture_overhead': 'uncalibrated; diagnosis only, no formal performance acceptance',
            'anchor_width_and_endpoint_drift_limit_us': 1000, 'pd0_exit': False}
        path = args.output / ('registration-' + dt.datetime.now(dt.timezone.utc).strftime('%Y%m%dT%H%M%SZ') + '.json')
        path.parent.mkdir(parents=True, exist_ok=True)
        if path.exists():
            raise ValueError('Registration already exists')
        base.save(path, registration)
        print('REGISTRATION ' + str(path.resolve()), flush=True)
        return 0

    registration = load(args.registration) if args.action == 'run' else None
    if registration:
        if not ct.windll.shell32.IsUserAnAdmin():
            raise RuntimeError('Administrator terminal required for WPR; no capture attempted')
        if (args.registration.parent / (args.registration.stem + '.used.json')).exists():
            raise RuntimeError('Registration already consumed; no automatic retry')
    source, binary, env = base.identity(), binaries(), environment()
    if registration:
        if source['implementation_sha256'] != registration['source']['implementation_sha256'] or binary != registration['binaries']:
            raise ValueError('Registered source/binaries changed; no capture started')
        require_environment(registration['environment'], env)
    output = args.output / (args.action + '-' + dt.datetime.now(dt.timezone.utc).strftime('%Y%m%dT%H%M%SZ') + '-' + uuid.uuid4().hex[:6])
    output.mkdir(parents=True)
    manifest = {'revision': 'pd0-system-runtime-qpc-v1', 'status': 'registered', 'source': source,
        'binaries': binary, 'environment_before': env, 'runs': [], 'rounds': [], 'pd0_exit': False,
        'registration': {'path': str(args.registration.resolve()), 'sha256': base.sha(args.registration)} if registration else None,
        'preparation_plan': None if registration else {'phase': 'pilot', 'order': CONFIGURATIONS,
                                                      'reads': 4096, 'seed': SEED, 'retry': False}}
    def save():
        base.save(output / 'manifest.json', manifest)
    def command(name, cmd):
        result = sp.run(cmd, cwd=ROOT, stdout=sp.PIPE, stderr=sp.STDOUT, timeout=180)
        (output / (name + '.log')).write_bytes(result.stdout)
        return result
    save()
    print('EVIDENCE ' + str(output.resolve()), flush=True)
    if registration:
        used = args.registration.parent / (args.registration.stem + '.used.json')
        # Exclusive creation prevents two invocations from consuming the same registration.
        with used.open('x', encoding='utf-8') as stream:
            json.dump({'evidence': str(output.resolve()), 'utc': utc()}, stream)
    try:
        rounds = registration['rounds'] if registration else [CONFIGURATIONS]
        for i, order in enumerate(rounds):
            owned_trace = False
            round_record = {'index': i + 1, 'status': 'registered'}
            manifest['rounds'].append(round_record)
            save()
            try:
                if registration:
                    status = command(f'round-{i+1}-wpr-before', ['wpr', '-status'])
                    if b'WPR is not recording' not in status.stdout:
                        raise RuntimeError('WPR active or status unknown; existing recording left untouched')
                    result = command(f'round-{i+1}-wpr-start', ['wpr', '-start', 'CPU', '-start', 'Power', '-filemode'])
                    round_record['wpr_start_exit_code'] = result.returncode
                    if result.returncode:
                        raise RuntimeError('WPR start failed; no automatic retry/cancel')
                    owned_trace = True
                    time.sleep(5)
                for config, profile in order:
                    before = environment()
                    require_environment(env, before)
                    name = f'round-{i+1}-{config}-{profile}'
                    record = {'name': name, 'config': config, 'profile': profile, 'round': i + 1,
                              'environment_before': before, 'start_utc': utc()}
                    manifest['runs'].append(record)
                    save()
                    measured = measurement.invoke(output, name, Path(binary[config]['path']), env['machine'], profile,
                        'run' if registration else 'pilot', registration['read_counts'][config+'/'+profile] if registration else 4096, SEED)
                    record['measurement'] = measured
                    record['alignment'] = verify_record(output, name, measured)
                    record.update(end_utc=utc(), environment_after=environment())
                    save()
                    require_environment(before, record['environment_after'])
                if registration:
                    time.sleep(5)
                round_record['status'] = 'workloads_collected'
            finally:
                if owned_trace:
                    result = command(f'round-{i+1}-wpr-stop', ['wpr', '-stop', str(output.resolve() / f'round-{i+1}.etl')])
                    round_record['wpr_stop_exit_code'] = result.returncode
                    save()
                    if result.returncode:
                        raise RuntimeError('WPR stop failed; inspect session status; no automatic cancel')
        manifest['status'] = 'diagnostic_workloads_collected' if registration else 'alignment_verified'
    except Exception as error:
        manifest.update(status='stopped_error', error=str(error))
        print(str(error), flush=True)
    finally:
        manifest.update(environment_after=environment(), source_after=base.identity(), finished_utc=utc())
        try:
            require_environment(env, manifest['environment_after'])
            if manifest['source_after']['implementation_sha256'] != source['implementation_sha256'] or binaries() != binary:
                raise ValueError('Source or binary identity changed')
        except Exception as error:
            manifest.update(status='stopped_error', error=str(error))
        save()
        base.save(output / 'evidence-hashes.json', {p.name: base.sha(p) for p in output.iterdir() if p.is_file()})
    print('STATUS ' + manifest['status'], flush=True)
    return int(manifest['status'] == 'stopped_error')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['prepare', 'register', 'run'])
    parser.add_argument('--preparation', type=Path)
    parser.add_argument('--registration', type=Path)
    parser.add_argument('--output', type=Path, default=ROOT / 'out/pd0-system-runtime-qpc')
    args = parser.parse_args()
    if args.action == 'run' and args.registration is None:
        parser.error('run requires --registration')
    if args.action == 'register' and args.preparation is None:
        parser.error('register requires --preparation')
    raise SystemExit(main(args))

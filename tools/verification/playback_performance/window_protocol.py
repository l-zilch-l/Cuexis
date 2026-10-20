"""Immutable PD5 window protocol, independent of platform and process launching."""
import datetime as dt
import hashlib
import json
from pathlib import Path

from tools.verification.playback_performance.measurement import budget, collect, paired_interval, records, repeatability, require_comparable


def file_hash(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def write_once(path, value):
    with Path(path).open('x', encoding='utf-8') as stream:
        stream.write(json.dumps(value, ensure_ascii=False, indent=2) + '\n')


def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def seal_registration(folder):
    """Bind the completed registration outside its own mutable document."""
    folder = Path(folder).resolve()
    path = folder / 'registration.json'
    reg = validate_registration(read(path))
    preparation = Path(reg['preparation']).resolve()
    claim = read(preparation / 'registration.claim.json')
    if Path(claim['folder']).resolve() != folder or claim['pilot_sha256'] != reg['pilot_sha256']:
        raise ValueError('Registration does not match its one-time preparation claim')
    write_once(preparation / 'registration.seal.json', {
        'schema': 'pd5.registration-seal.v1', 'folder': str(folder),
        'registration_sha256': file_hash(path),
        'claim_sha256': file_hash(preparation / 'registration.claim.json')})


def read_registration(folder):
    folder = Path(folder).resolve()
    path = folder / 'registration.json'
    reg = validate_registration(read(path))
    preparation = Path(reg['preparation']).resolve()
    seal = read(preparation / 'registration.seal.json')
    claim_path = preparation / 'registration.claim.json'
    claim = read(claim_path)
    if (seal.get('schema') != 'pd5.registration-seal.v1' or
            Path(seal['folder']).resolve() != folder or Path(claim['folder']).resolve() != folder or
            seal['registration_sha256'] != file_hash(path) or
            seal['claim_sha256'] != file_hash(claim_path) or
            claim['pilot_sha256'] != reg['pilot_sha256'] or
            Path(claim['pilot']).resolve() != Path(reg['pilot']).resolve()):
        raise ValueError('Registration or its one-time claim changed after sealing')
    if (file_hash(preparation / 'preparation.json') != reg['preparation_sha256'] or
            file_hash(reg['pilot']) != reg['pilot_sha256']):
        raise ValueError('Registered preparation or pilot changed')
    return reg


def evidence_paths(row):
    record_path = Path(row['record'])
    record = read(record_path)
    return [record_path, *(Path(a['path']) for a in record['commands'].values())]


def unique_evidence(rows, artifacts=None):
    artifacts = set() if artifacts is None else artifacts
    for row in rows:
        for item in evidence_paths(row):
            # Native file identity catches hard links as well as path aliases on this machine.
            path = item.resolve()
            info = path.stat()
            keys = [('path', str(path).casefold()), ('file', info.st_dev, info.st_ino)]
            if any(key in artifacts for key in keys):
                raise ValueError('Process record or raw output reused as independent evidence')
            artifacts.update(keys)
    return artifacts


def pilot_evidence(reg):
    artifacts = set()
    if reg.get('pilot'):
        path = Path(reg['pilot'])
        if file_hash(path) != reg['pilot_sha256']:
            raise ValueError('Registered pilot changed')
        unique_evidence([{'record': run['path']} for run in read(path).get('runs', [])], artifacts)
    return artifacts


def validate_registration(reg):
    if reg.get('schema') != 'pd5.windows.v1' or reg.get('accepted_profile') is not True:
        raise ValueError('An accepted explicit profile is required')
    if reg.get('windows') != 3 or reg.get('pairs_per_window') != 3 or reg.get('gap_seconds') != 600:
        raise ValueError('Registered three-window protocol changed')
    if reg.get('noise_limit') != .15 or reg.get('regression_limit') != 1.10:
        raise ValueError('Accepted thresholds changed')
    units = reg.get('units', [])
    if not units or len({x['id'] for x in units}) != len(units):
        raise ValueError('Missing or duplicate workload unit')
    for unit in units:
        if unit.get('kind') not in ('owner', 'host', 'probe') or not isinstance(unit.get('metrics'),list) or not unit['metrics']:
            raise ValueError('Unsupported window unit')
        if unit.get('kind') == 'probe' and unit.get('comparison') != 'B-only':
            raise ValueError('Probe units require an explicit B-only disposition')
        if len(set(unit['metrics'])) != len(unit['metrics']):
            raise ValueError('Duplicate primary metric')
        if unit.get('config') not in ('release', 'debug'):
            raise ValueError('Unknown build configuration')
    return reg


def schedule(reg, window):
    validate_registration(reg)
    if window not in (1, 2, 3):
        raise ValueError('Invalid window')
    result = []
    for unit in reg['units']:
        for pair in range(3):
            order = ('B',) if unit.get('comparison') == 'B-only' else (('A', 'B') if (pair + window) % 2 else ('B', 'A'))
            for implementation in order:
                result.append({'window': window, 'unit': unit['id'], 'pair': pair,
                               'implementation': implementation})
    return result


def claim(folder, window, utc_now, monotonic_now):
    folder = Path(folder)
    reg_path = folder / 'registration.json'
    reg = read_registration(folder)
    if (folder / 'stopped.json').exists():
        raise ValueError('Registration stopped; replacement/retry prohibited')
    previous_windows = load_windows(folder, window - 1)
    if window > 1:
        previous = read(folder / f'window-{window-1}.result.json')
        elapsed_utc = (dt.datetime.fromisoformat(utc_now) - dt.datetime.fromisoformat(previous['ended_utc'])).total_seconds()
        elapsed_monotonic = monotonic_now - previous['ended_monotonic']
        if min(elapsed_utc, elapsed_monotonic) < reg['gap_seconds']:
            raise ValueError('Window gap shorter than 600 seconds, or clock/boot changed')
    value = {'schema': 'pd5.window-claim.v1', 'window': window,
             'registration_sha256': file_hash(reg_path), 'started_utc': utc_now,
             'started_monotonic': monotonic_now, 'schedule': schedule(reg, window),
             'previous_windows': window_hashes(folder, len(previous_windows))}
    write_once(folder / f'window-{window}.claim.json', value)
    return value


def validate_window(reg, claim_data, result):
    expected = schedule(reg, claim_data['window'])
    rows = result.get('runs', [])
    if len(rows) != len(expected):
        raise ValueError('Incomplete window; cannot use replacement runs')
    parsed, artifacts = [], set()
    for row, target in zip(rows, expected):
        if any(row.get(k) != v for k, v in target.items()):
            raise ValueError('Order, process pair or identity changed')
        if row.get('exit_code') != 0 or row.get('execution_valid') is not True:
            raise ValueError('Failed/unvalidated process')
        unit = next(x for x in reg['units'] if x['id'] == row['unit'])
        record_path = Path(row['record'])
        if file_hash(record_path) != row['record_sha256']:
            raise ValueError('Original process record changed')
        record = read(record_path)
        for command in ('warmup', 'measure'):
            artifact = record['commands'][command]
            path = str(Path(artifact['path']).resolve()).casefold()
            if path in artifacts:
                raise ValueError('Process output reused as another independent sample')
            artifacts.add(path)
            if artifact['exit_code'] != 0 or file_hash(artifact['path']) != artifact['sha256']:
                raise ValueError('Original process output changed or failed')
        warmups = records(Path(record['commands']['warmup']['path']).read_text(encoding='utf-8-sig'), 'PD_MACHINE_WARMUP ')
        if len(warmups) != 1 or warmups[0].get('seconds') != 30:
            raise ValueError('Missing complete machine warmup')
        artifact = record['commands']['measure']
        if artifact['sha256'] != row['raw_log_sha256']:
            raise ValueError('Raw log identity differs from process record')
        data = collect(artifact['path'], unit['kind'], artifact['exit_code'], 'formal')
        if any(data[key] != record['data'].get(key) for key in data):
            raise ValueError('Parsed record differs from original log')
        if reg.get('expected_identities'):
            expected_identity = reg['expected_identities'][unit['id']]
            if any(data['identity'].get(k) != v for k, v in expected_identity.items()):
                raise ValueError('Original workload identity differs from registered pilot')
            native = records(Path(artifact['path']).read_text(encoding='utf-8-sig'), 'PD_ENV ')
            expected_scheduling = reg['expected_scheduling'][unit['id']]
            if len(native) != 2 or [x.get('label') for x in native] != ['begin', 'end']:
                raise ValueError('Missing original scheduling evidence')
            if any(x.get(k) != v for x in native for k, v in expected_scheduling.items()):
                raise ValueError('Original scheduling differs from registered pilot')
        for metrics, statistic, field in ((unit['metrics'], 'median_us', 'values'),
                (unit.get('mean_noise_metrics', []), 'mean_us', 'mean_values')):
            if row.get(field, {}) != {key: data['statistics'][key][statistic] for key in metrics}:
                raise ValueError('Copied window values differ from raw observations')
        parsed.append(data)
        if (unit.get('comparison') != 'B-only' and len(parsed) >= 2 and
                target['unit'] == expected[len(parsed)-2]['unit'] and
                target['pair'] == expected[len(parsed)-2]['pair']):
            for metric in unit['metrics']:
                require_comparable(parsed[-2], parsed[-1], metric)


def window_hashes(folder, count):
    return [{'window': n, 'claim_sha256': file_hash(folder / f'window-{n}.claim.json'),
             'result_sha256': file_hash(folder / f'window-{n}.result.json')}
            for n in range(1, count + 1)]


def load_windows(folder, count=3):
    """Verify the saved chain and every original process artifact before advancing."""
    folder = Path(folder)
    reg = read_registration(folder)
    results, artifacts = [], pilot_evidence(reg)
    for n in range(1, count + 1):
        claim_path = folder / f'window-{n}.claim.json'
        result_path = folder / f'window-{n}.result.json'
        if not claim_path.is_file() or not result_path.is_file():
            raise ValueError('Earlier window is not complete')
        c, r = read(claim_path), read(result_path)
        if (c.get('window') != n or r.get('window') != n or r.get('status') != 'collected' or
                c.get('registration_sha256') != file_hash(folder / 'registration.json') or
                r.get('registration_sha256') != c['registration_sha256'] or
                r.get('claim_sha256') != file_hash(claim_path) or
                c.get('schedule') != schedule(reg, n) or
                c.get('previous_windows') != window_hashes(folder, n - 1)):
            raise ValueError('Saved window claim/result chain changed')
        elapsed_utc = (dt.datetime.fromisoformat(r['ended_utc']) - dt.datetime.fromisoformat(c['started_utc'])).total_seconds()
        if elapsed_utc < 0 or r['ended_monotonic'] < c['started_monotonic']:
            raise ValueError('Window completion precedes claim')
        validate_window(reg, c, r)
        unique_evidence(r['runs'], artifacts)
        if results:
            prior = results[-1]
            gap_utc = (dt.datetime.fromisoformat(c['started_utc']) - dt.datetime.fromisoformat(prior['ended_utc'])).total_seconds()
            if min(gap_utc, c['started_monotonic'] - prior['ended_monotonic']) < reg['gap_seconds']:
                raise ValueError('Saved window gap is invalid')
        results.append(r)
    return results


def summarize(reg, windows):
    validate_registration(reg)
    if len(windows) != 3 or [x.get('window') for x in windows] != [1, 2, 3]:
        raise ValueError('All three registered windows are required')
    result = []
    artifacts = pilot_evidence(reg)
    for window in windows:
        validate_window(reg, {'window': window['window']}, window)
        unique_evidence(window['runs'], artifacts)
    for unit in reg['units']:
        if unit.get('comparison') == 'B-only':
            result.extend(summarize_probe(unit, windows))
            continue
        for metric in unit['metrics']:
            values = {'A': [], 'B': []}
            for implementation in values:
                for window in windows:
                    rows = [x for x in window['runs'] if x['unit'] == unit['id'] and x['implementation'] == implementation]
                    values[implementation].append([x['values'][metric] for x in rows])
            repeated = {name: repeatability(groups) for name, groups in values.items()}
            mean_repeated = {}
            if metric in unit.get('mean_noise_metrics', []):
                for implementation in ('A','B'):
                    means=[[x['mean_values'][metric] for x in w['runs'] if x['unit']==unit['id'] and
                            x['implementation']==implementation] for w in windows]
                    mean_repeated[implementation]=repeatability(means)
            per_window = [paired_interval(list(zip(values['A'][i], values['B'][i]))) for i in range(3)]
            combined = paired_interval(list(zip(sum(values['A'], []), sum(values['B'], []))))
            noise_ok = all(x['status'] == 'pass' for x in [*repeated.values(),*mean_repeated.values()])
            result.append({'unit': unit['id'], 'config': unit['config'], 'metric': metric,
                'process_values': values, 'repeatability': repeated, 'mean_repeatability':mean_repeated,
                'window_effects': per_window,
                'combined_effect': combined,
                'status': combined['status'] if noise_ok else 'inconclusive',
                'split_design_trigger': noise_ok and all(x['status'] == 'over_budget' for x in per_window)})
    return {'schema': 'pd5.window-summary.v1', 'results': result,
            'status': 'pass' if all(x['status'] == 'pass' for x in result) else
                      'over_budget' if any(x['status'] == 'over_budget' for x in result) else 'inconclusive',
            'pd5_complete': False, 'production_acceptance': False}


def summarize_probe(unit, windows):
    """Retain every process, gate noise independently from absolute budgets."""
    result = []
    for metric in unit['metrics']:
        groups, mean_groups, budgets = [], [], []
        for window in windows:
            rows = [row for row in window['runs'] if row['unit'] == unit['id']]
            groups.append([row['values'][metric] for row in rows])
            if metric in unit.get('mean_noise_metrics', []):
                mean_groups.append([row['mean_values'][metric] for row in rows])
            observations = []
            for row in rows:
                process = read(row['record'])
                values = [s['elapsed_us'] for s in process['data']['samples'] if s['metric'] == metric]
                if metric in unit.get('budget_metrics', []):
                    observations.append(budget(metric, values, unit['config'].title(), observer_validated=False))
            budgets.append(observations)
        repeated = repeatability(groups)
        means = repeatability(mean_groups) if mean_groups else None
        noise_ok = repeated['status'] == 'pass' and (means is None or means['status'] == 'pass')
        all_budgets = sum(budgets, [])
        status = 'inconclusive' if not noise_ok else (
            'over_budget' if any(x['status'] == 'over_budget' for x in all_budgets) else
            'pass' if all_budgets and all(x['status'] == 'pass' for x in all_budgets) else 'inconclusive')
        # Per-process failures remain failures even if the window median passes.
        # The separate trigger uses precisely the accepted window statistic.
        window_values = []
        for observations in budgets:
            if len(observations) != 3 or any(x.get('statistic') is None or x['statistics'][x['statistic']] is None for x in observations):
                window_values.append(None)
            else:
                import statistics
                statistic = observations[0]['statistic']
                values = [x['statistics'][statistic] for x in observations]
                window_values.append(statistics.median(values) if statistic == 'p99_us' else max(values))
        from tools.verification.playback_performance.measurement import LIMITS_US
        result.append({'unit': unit['id'], 'config': unit['config'], 'comparison': 'B-only',
            'metric': metric, 'process_values': {'B': groups}, 'repeatability': {'B': repeated},
            'mean_repeatability': {'B': means} if means else {}, 'process_budgets': budgets,
            'window_budget_values_us': window_values, 'status': status,
            'split_design_trigger': noise_ok and all_budgets and
                all(x['status'] in ('pass', 'over_budget') for x in all_budgets) and
                all(x is not None and x > LIMITS_US[metric] for x in window_values)})
    return result

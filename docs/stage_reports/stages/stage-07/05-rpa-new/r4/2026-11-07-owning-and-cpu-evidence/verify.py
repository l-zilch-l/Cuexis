"""Verify archived raw bytes and recompute scoped memory/CPU results; no native launches."""
import datetime as dt
import hashlib
import json
from pathlib import Path
import zipfile


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def records(text, prefix):
    return [json.loads(line[len(prefix):]) for line in text.splitlines() if line.startswith(prefix)]


def memory(text):
    globals_ = records(text, 'RPA_MEMORY ')
    partitions = records(text, 'RPA_MEMORY_PARTITION ')
    domains = records(text, 'RPA_MEMORY_DOMAIN ')
    require(len(globals_) == len(partitions) == 19, 'checkpoint set differs')
    for global_, partition in zip(globals_, partitions):
        require(not global_['exceeded'] and partition['exact'] and
                partition['untagged_live_count'] == 0 and global_['label'] == partition['label'],
                'global partition invalid')
        selected = [row for row in domains if row['label'] == global_['label']]
        for key in ('live_count', 'requested_cpp_bytes', 'crt_usable_storage_bytes'):
            require(sum(row[key] for row in selected) == global_[key], 'domain sum differs')
    proofs = records(text, 'RPA_MEMORY_WORLD_PROOF ')
    allocations = records(text, 'RPA_MEMORY_WORLD_ALLOCATION ')
    spans = records(text, 'RPA_MEMORY_WORLD_SPAN ')
    require([row['phase'] for row in proofs] == ['prepared', 'runtime-installed', 'active'],
            'true active transition absent')
    totals = []
    for proof in proofs:
        require(proof['valid'] and not proof['complete_world_graph'] and
                proof['bound_kind'] == 'instantaneous_unique_lower_bound', 'invalid proof')
        selected = [row for row in allocations if row['census_id'] == proof['census_id']]
        require(len(selected) == len({row['allocation_id'] for row in selected}) == proof['live_count'],
                'duplicate allocation identity')
        total, stable, transient = 0, 0, 0
        for allocation in selected:
            intervals = []
            for span in spans:
                if (span['census_id'], span['allocation_id']) == (proof['census_id'], allocation['allocation_id']):
                    start, size = span['offset'], span['bytes']
                    require(0 <= start and 0 < size <= allocation['requested_cpp_bytes'] - start,
                            'span exceeds backing')
                    intervals.append((start, start + size))
            end, union = 0, 0
            for start, stop in sorted(intervals):
                union += max(0, stop - max(start, end))
                end = max(end, stop)
            require(allocation['membership'] in ('stable_registry_subset', 'actual_owned_span'),
                    'unexpected membership')
            charge = allocation['requested_cpp_bytes'] if allocation['membership'] == 'stable_registry_subset' else union
            require(charge == allocation['charged_cpp_bytes'], 'charge differs from union')
            total += charge
            if allocation['membership'] == 'stable_registry_subset':
                stable += charge
            else:
                transient += charge
        require(total == proof['lower_bound_requested_cpp_bytes'], 'lower bound sum differs')
        for field, key in (('backing_requested_cpp_bytes', 'requested_cpp_bytes'),
                           ('backing_crt_usable_storage_bytes', 'crt_usable_storage_bytes')):
            require(proof[field] == sum(row[key] for row in selected), 'backing sum differs')
        totals.append({'total': total, 'stable': stable, 'instantaneous': transient})
    roots = [row for row in records(text, 'RPA_MEMORY_SEMANTIC_ROOT ') if row['label'] == 'after-close']
    require(roots and all(row['phase'] == 'released' and row['live_count'] == 0 and
            row['requested_cpp_bytes'] == row['crt_usable_storage_bytes'] == 0 for row in roots),
            'Close release differs')
    product = records(text, 'RPA_MEMORY_PRODUCT ')[-1]
    require(product['label'] == 'after-fixture-release' and
            product['requested_cpp_bytes'] == product['crt_usable_storage_bytes'] == 0,
            'final product book not zero')
    require('All tests passed (21 assertions in 1 test case)' in text, 'native assertions differ')
    return totals[-1]


def main():
    directory = Path(__file__).resolve().parent
    manifest = json.loads((directory / 'manifest.json').read_text(encoding='utf-8'))
    require(digest((directory / 'raw.zip').read_bytes()) == manifest['archive_sha256'], 'archive differs')
    with zipfile.ZipFile(directory / 'raw.zip') as bundle:
        require(len(bundle.namelist()) == len(set(bundle.namelist())) and
                set(bundle.namelist()) == set(manifest['files']), 'archive member set differs')
        for name, item in manifest['files'].items():
            raw = bundle.read(name)
            require(len(raw) == item['bytes'] and digest(raw) == item['sha256'], 'member differs: ' + name)
        def read(name):
            return json.loads(bundle.read(name))
        memory_results = {}
        for reuse in ('high', 'low'):
            prefix = 'owning/64-' + reuse + '/'
            registration = read(prefix + 'registration.json')
            process = read(prefix + 'process-result.json')
            require(registration['source_sha'] == manifest['source_sha'] and
                    registration['maximum_processes'] == 1 and registration['timeout_seconds'] == 120 and
                    not registration['formal_sampling'] and not registration['p8'] and
                    not registration['entities_40000'], 'diagnostic scope differs')
            require(process['exit_code'] == 0 and not process['timed_out'], 'diagnostic process failed')
            for stream in ('stdout', 'stderr'):
                require(process[stream]['sha256'] == digest(bundle.read(prefix + stream + '.log')),
                        'diagnostic stream binding differs')
            memory_results[reuse] = memory(bundle.read(prefix + 'stdout.log').decode('utf-8'))
            require(memory_results[reuse] == {'total': 119534, 'stable': 77114, 'instantaneous': 42420} and
                    read(prefix + 'analysis.json')['rating']['status'] == 'unrated', 'memory result differs')
        stats = {'__name__': 'archived_measurement'}
        exec(compile(bundle.read('tools/measurement.py'), 'archived_measurement.py', 'exec'), stats)
        fixed = {'__name__': 'archived_results', 'measurement': type('Measurement', (), stats)}
        source = bundle.read('tools/results.py').decode('utf-8').replace(
            'from tools.verification.playback_performance import measurement', '')
        exec(compile(source, 'archived_results.py', 'exec'), fixed)
        cpu_results = read('owning/cpu-formal-run1/result.json')
        process_count = 0
        for window in range(1, 4):
            start = dt.datetime.fromisoformat(read(f'owning/cpu-formal-run1/window-{window}-start.json')['utc'])
            end = dt.datetime.fromisoformat(read(f'owning/cpu-formal-run1/window-{window}-end.json')['utc'])
            require(end > start, 'window chronology differs')
            if window > 1:
                require((start - previous_end).total_seconds() >= 600, 'window gap too short')
            previous_end = end
        for unit in ('p0-legacy-release', 'p0-rpa-release'):
            windows = []
            for window in range(1, 4):
                pairs = []
                for pair in range(1, 4):
                    sides = {}
                    for side in ('A', 'B'):
                        prefix = f'owning/cpu-formal-run1/{unit}/window-{window}/pair-{pair}-{side}/'
                        sample = read(prefix + 'sample.json')
                        raw = bundle.read(prefix + 'sample.log')
                        require(sample['exit_code'] == 0 and sample['log_sha256'] == digest(raw), 'CPU process differs')
                        parsed = stats['parse_owner'](raw.decode('utf-8'), exit_code=0, phase='formal')
                        saved = read(prefix + 'parsed.json')
                        require(parsed == saved['parsed'], 'CPU parse differs')
                        for phase in ('warmup', 'sample'):
                            require(read(prefix + phase + '-runtime-environment.json')['passed'], 'environment failed')
                        sides[side] = {key: parsed['statistics'][key]['median_us'] for key in saved['medians']}
                        require(sides[side] == saved['medians'], 'CPU medians differ')
                        process_count += 1
                    pairs.append(sides)
                windows.append(pairs)
            recomputed = fixed['verdict'](windows, 'A/B')
            require(recomputed['metrics'] == cpu_results[unit]['metrics'], 'CPU verdict differs')
        matrix = read('matrix/manifest.json')
        require(matrix['source']['head'] == manifest['source_sha'] and matrix['source_unchanged'] and
                len(matrix['rows']) == 11 and all(row['status'] == 'local_pass' for row in matrix['rows']) and
                all(row['test_counts'] and row['test_counts'][1] == 0 for row in matrix['rows']),
                'eleven-row local matrix differs')
        diagnostic_plan = read('owning/read-cost-diagnostic-plan.json')
        require(diagnostic_plan['formal_sampling'] is False and
                diagnostic_plan['maximum_rounds'] == 2 and diagnostic_plan['maximum_measured_processes_per_round'] == 4 and
                diagnostic_plan['timeout_seconds_each_process'] == 120,
                'read diagnostic scope differs')
        first_attempt = read('read-cost/diagnostic-v1/stopped.json')
        require(first_attempt['error'] == 'CPU/headless foreground policy must be explicit' and
                first_attempt['partial_completed_rounds'] == [] and first_attempt['no_replacement'],
                'pre-process controller stop differs')
        diagnostic = read('read-cost/diagnostic-v2/disposition.json')
        require(diagnostic['rounds_consumed'] == 1 and not diagnostic['bounded_effect_qualified'] and
                diagnostic['formal_claims_consumed'] == 0 and not diagnostic['formal_success'],
                'read diagnostic disposition differs')
        round_one = read('read-cost/diagnostic-v2/round-1-result.json')
        require(round_one['results']['legacy']['prototype_control_ratio'] > 1.0 and
                round_one['results']['rpa']['prototype_control_ratio'] > 1.05 and
                not round_one['fixed_effect_and_noise_conditions_met'], 'read diagnostic effect differs')
        hosted = [read('owning/hosted-' + str(run) + '.json') for run in
                  (42338703031, 42338703012, 42338703030, 42338703190, 42338703227, 42338703142,
                   42338711734, 42338711746, 42338711678, 42338711727)]
        require(all(row['headSha'] == manifest['source_sha'] for row in hosted), 'hosted SHA differs')
        print(json.dumps({'verified_files': len(manifest['files']), 'memory': memory_results,
              'cpu_processes_reparsed': process_count, 'matrix_rows': len(matrix['rows']),
              'diagnostic_rounds_consumed': diagnostic['rounds_consumed'],
              'diagnostic_formal_claims_consumed': diagnostic['formal_claims_consumed'],
              'hosted_runs': len(hosted), 'cpu_metrics': {key: {metric: value['status']
              for metric, value in result['metrics'].items()} for key, result in cpu_results.items()},
              'memory_formal_admitted': False, 'native_processes_launched': 0}, indent=2))


if __name__ == '__main__':
    main()

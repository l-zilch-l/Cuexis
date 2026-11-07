"""Finite diagnostic contract. Block observations never become independent processes."""
import argparse
import json
from pathlib import Path
import re
import statistics as st

if __package__ in (None, ''):
    import sys
    sys.path.insert(0, str(Path(__file__).resolve().parents[3]))
from tools.verification.playback_performance import measurement as m

METHOD = 'pd5.followup.v2'
PROTOCOL = 'local-reference-v1-diagnostic'
WORK = ('objects', 'cameras', 'registry_entities', 'U', 'F', 'R', 'content_sha256',
        'sequence_sha256', 'viewport', 'output_digest')


def digest(value):
    if not isinstance(value, str) or re.fullmatch('[0-9a-f]{64}', value) is None:
        raise ValueError('Missing or invalid SHA256')


def validate(process, expected):
    if process['schema'] != 'playback.diagnostic-process.v1' or process['method'] != METHOD or process['protocol'] != PROTOCOL:
        raise ValueError('Unknown diagnostic method/protocol')
    for key in ('source_sha256', 'binary_sha256'):
        digest(process[key])
        if process[key] != expected[key]:
            raise ValueError('Source/binary identity mismatch')
    if not process['run_id'] or type(process['pid']) is not int or process['pid'] <= 0:
        raise ValueError('Missing independent process identity')
    if type(process['native_start_qpc']) is not int or process['native_start_qpc'] <= 0:
        raise ValueError('Missing native process start identity')
    digest(process['log_sha256'])
    if process['output_validated'] is not True:
        raise ValueError('Output semantics not validated')
    work = process['workload']
    for key in WORK:
        if key not in work or work[key] is None:
            raise ValueError('Missing workload field: ' + key)
    for key in ('objects', 'cameras', 'registry_entities', 'U', 'F', 'R'):
        if type(work[key]) is not int or work[key] < 0:
            raise ValueError('Invalid workload count')
    for key in ('content_sha256', 'sequence_sha256'):
        digest(work[key])
    metric = process['metric']
    if any(not isinstance(metric[k], str) or not metric[k] for k in ('name', 'start', 'end')):
        raise ValueError('Missing metric endpoints')
    if metric['unit'] != 'us' or metric['sample_kind'] not in ('block_average', 'operation'):
        raise ValueError('Unknown sample semantics')
    values = []
    for row in process['samples']:
        operations = row['operations']
        if type(operations) is not int or operations <= 0:
            raise ValueError('Invalid operation count')
        elapsed = m.positive(row['elapsed_us'])
        if metric['sample_kind'] == 'operation' and operations != 1:
            raise ValueError('Block averages cannot stand for operation quantiles')
        if metric['sample_kind'] == 'block_average' and elapsed < 100000:
            raise ValueError('Diagnostic read block below 100 ms')
        values.append(elapsed / operations)
    if not values:
        raise ValueError('Missing samples')
    stats = m.statistics(values)
    if metric['sample_kind'] == 'block_average':
        stats['p95_us'] = stats['p99_us'] = None
    return stats


def compare(pairs, identities):
    if len(pairs) != 3:
        raise ValueError('Finite diagnostic requires exactly 3 independent process pairs')
    seen = set()
    native_starts = set()
    raw_logs = set()
    observations = []
    summaries = []
    common = None
    for a, b in pairs:
        stats = []
        for side, process in (('A', a), ('B', b)):
            native = (process['pid'], process['native_start_qpc'])
            if process['run_id'] in seen or native in native_starts or process['log_sha256'] in raw_logs:
                raise ValueError('Blocks/reused records cannot count as independent processes')
            seen.add(process['run_id'])
            native_starts.add(native)
            raw_logs.add(process['log_sha256'])
            stats.append(validate(process, identities[side]))
            contract = (process['method'], process['protocol'], process['workload'], process['metric'])
            if common is None:
                common = contract
            elif contract != common:
                raise ValueError('Logical work, output or metric endpoints differ')
        observations.append(tuple(s['median_us'] for s in stats))
        summaries.append(stats)
    interval = m.paired_interval(observations)
    spreads = {side: m.noise([p[index] for p in observations]) for index, side in enumerate(('A', 'B'))}
    interval['status'] = ('inconclusive' if max(spreads.values()) > m.NOISE_LIMIT else
                          'diagnostic_' + interval['status'])
    return {'method': METHOD, 'protocol': PROTOCOL, 'acceptance': False,
            'process_statistics': summaries, 'process_pairs': observations,
            'noise': spreads, 'effect': interval, 'metric': common[3],
            'workload': common[2]}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path, help='JSON containing identities and exactly three pairs')
    args = parser.parse_args()
    data = json.loads(args.input.read_text(encoding='utf-8-sig'))
    print(json.dumps(compare(data['pairs'], data['identities']), indent=2))

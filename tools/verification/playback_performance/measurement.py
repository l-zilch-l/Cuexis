"""PD-5 data contract and statistics. Does not launch or retry measurements."""
import json
import math
from pathlib import Path
import statistics as st

NOISE_LIMIT = .15
REGRESSION_LIMIT = 1.10
HOST_FIELDS = {
    'pd0.receipt-ack.v1': ('request_to_ready_us', 'ready_to_receipt_ack_us',
                         'request_to_receipt_ack_us', 'present_call_to_ack_us'),
    'pd4.present-return.v1': ('request_to_ready_us', 'ready_to_present_return_us',
                            'request_to_present_return_us', 'present_call_to_return_us'),
}
T95 = {3: 4.302653, 9: 2.306004}
# Identity a repeatable presentation-matrix probe process must reproduce; peaks and digest are
# observations, not registered inputs, so they are deliberately absent.
PROBE_IDENTITY = ('mode', 'harness', 'frames', 'warmup_frames', 'updates', 'materializations',
                  'capture_consumers', 'renderer_consumers', 'base_entities', 'max_entities',
                  'payload_charge_per_frame', 'gameplay', 'audio', 'gpu', 'closed', 'counts')
LIMITS_US = {'d2': 1000, 'd3': 2000, 'materialize': 2000, 'queue': 1000,
             'request_to_ready': 1000000 / 240, 'gpu_render': 1000000 / 240,
             'pause': 50000, 'resume': 50000, 'resize': 50000,
             'reset_paused': 1000000, 'reset_running': 1000000,
             'load': 2000000, 'close': 1000000, 'seek': 20000}


def positive(value, allow_zero=False):
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
        raise ValueError('Non-finite/non-numeric observation')
    if value < 0 or (value == 0 and not allow_zero):
        raise ValueError('Observation requires positive value')
    return value


def statistics(values):
    values = sorted(positive(x, True) for x in values)
    if not values:
        raise ValueError('Empty observations')
    return {'n': len(values), 'median_us': st.median(values), 'mean_us': st.mean(values),
            'p95_us': values[math.ceil(.95 * len(values)) - 1] if len(values) >= 200 else None,
            'p99_us': values[math.ceil(.99 * len(values)) - 1] if len(values) >= 1000 else None,
            'min_us': values[0], 'max_us': values[-1]}


def noise(values):
    values = [positive(x) for x in values]
    return (max(values) - min(values)) / st.median(values)


def paired_interval(pairs):
    if len(pairs) not in T95:
        raise ValueError('Only the registered 3 or 9 independent process pairs are supported')
    logs = [math.log(positive(b) / positive(a)) for a, b in pairs]
    center = st.mean(logs)
    half = T95[len(pairs)] * st.stdev(logs) / math.sqrt(len(logs))
    low, high = math.exp(center - half), math.exp(center + half)
    return {'n_pairs': len(pairs), 'ratios': [b / a for a, b in pairs],
            'geometric_ratio': math.exp(center), 'ci95': [low, high],
            'status': 'pass' if high <= REGRESSION_LIMIT else
                      'over_budget' if low > REGRESSION_LIMIT else 'inconclusive'}


def repeatability(windows):
    if len(windows) != 3 or any(len(w) != 3 for w in windows):
        raise ValueError('Three windows of three independent processes required')
    result = {'window_noise': [noise(w) for w in windows],
              'first_six_noise': noise(windows[0] + windows[1]),
              'all_nine_noise': noise(sum(windows, []))}
    result['status'] = 'pass' if all(x <= NOISE_LIMIT for x in
        [*result['window_noise'], result['first_six_noise'], result['all_nine_noise']]) else 'inconclusive'
    return result


def budget(metric, values, config='Release', observer_validated=False):
    stats = statistics(values)
    if config != 'Release':
        return {'statistics': stats, 'status': 'debug_observation_only'}
    if metric not in LIMITS_US:
        raise ValueError('Unknown metric; no implicit budget')
    if metric in ('d2', 'd3', 'materialize', 'queue', 'gpu_render') and not observer_validated:
        return {'statistics': stats, 'status': 'observer_unvalidated'}
    statistic = 'p99_us' if metric in ('d2', 'd3', 'materialize', 'queue', 'request_to_ready', 'gpu_render') else 'max_us'
    value = stats[statistic]
    return {'statistics': stats, 'statistic': statistic, 'limit_us': LIMITS_US[metric],
            'status': 'insufficient_samples' if value is None else
                      'pass' if value <= LIMITS_US[metric] else 'over_budget'}


def records(text, prefix):
    return [json.loads(line[len(prefix):]) for line in text.splitlines() if line.startswith(prefix)]


def parse_host(text, exit_code=0):
    ids, rows = records(text, 'PD_HOST_ID '), records(text, 'PD_HOST_SAMPLE ')
    if type(exit_code) is not int or exit_code != 0 or len(ids) != 1:
        raise ValueError('Failed or incomplete host process')
    identity = ids[0]
    schema = identity.get('measurement_schema', 'pd0.receipt-ack.v1')
    if schema not in HOST_FIELDS:
        raise ValueError('Unknown host schema')
    if (schema == 'pd4.present-return.v1') != (identity.get('receipt_mode') == 'asynchronous'):
        raise ValueError('Receipt mode does not match measurement schema')
    if len(rows) != identity.get('measured') or [r.get('ordinal') for r in rows] != list(range(len(rows))):
        raise ValueError('Missing, duplicate or reordered host observations')
    if not rows or identity.get('recording') is not False or identity.get('closed') is not True:
        raise ValueError('Not a complete uninstrumented normal run')
    for key in ('digest', 'source', 'viewport', 'objects', 'warmup', 'measured', 'audio', 'gpu'):
        if key not in identity:
            raise ValueError('Missing host workload identity: ' + key)
    fields = HOST_FIELDS[schema]
    stats = {field: statistics([r[field] for r in rows]) for field in fields}
    return {'schema': schema, 'identity': identity, 'samples': rows, 'statistics': stats,
            'receipt_endpoint': 'present_return' if schema == 'pd4.present-return.v1' else 'worker_ack'}


def parse_owner(text, exit_code=0, phase='formal'):
    ids, rows = records(text, 'PD_FORMAL_ID '), records(text, 'PD_V2 ')
    if type(exit_code) is not int or exit_code != 0 or len(ids) != 1 or ids[0].get('metrics_compile_enabled') is not False or ids[0].get('allocator') != 'normal':
        raise ValueError('Incomplete or instrumented owner process')
    metrics = ('validated_trace', 'playback_only', 'read_long')
    blocks = {'formal': 8, 'pilot': 3}[phase]
    if len(rows) != blocks * len(metrics):
        raise ValueError('Unexpected owner block count')
    result = {}
    for metric in metrics:
        selected = [r for r in rows if r.get('kind') == metric]
        if len(selected) != blocks or [r.get('group') for r in selected] != list(range(blocks)):
            raise ValueError('Incomplete or duplicate owner blocks')
        if any(r.get('active') for r in selected):
            raise ValueError('Recording enabled in formal owner block')
        if metric == 'read_long' and phase == 'formal' and any(positive(r['sample']['wall_us']) < 100000 for r in selected):
            raise ValueError('Read block below registered 100 ms minimum')
        result[metric] = statistics([positive(r['sample']['wall_us']) / positive(r['operations']) for r in selected])
    for key in ('digest', 'replay_sha256', 'snapshot_sha256', 'packed_sha256', 'profile'):
        if key not in ids[0]:
            raise ValueError('Missing exact owner identity: ' + key)
    return {'schema': 'pd5.owner-fixed.v1', 'identity': ids[0], 'samples': rows, 'statistics': result}


def require_comparable(a, b, metric):
    """Only explicitly common endpoints. ACK is never renamed to present return."""
    if metric not in a['statistics'] or metric not in b['statistics']:
        raise ValueError('Metric endpoint not available on both implementations')
    owner = a['schema'] == b['schema'] == 'pd5.owner-fixed.v1'
    probe = a['schema'] == b['schema'] == 'pd5.probe.v1'
    keys = ('digest', 'replay_sha256', 'snapshot_sha256', 'packed_sha256', 'profile') if owner else (
        PROBE_IDENTITY if probe else
        ('digest', 'source', 'viewport', 'objects', 'warmup', 'measured', 'audio', 'gpu'))
    if not owner and not probe and a['schema'] != b['schema'] and metric != 'request_to_ready_us':
        raise ValueError('Different host completion endpoints')
    if any(k not in a['identity'] or k not in b['identity'] for k in keys):
        raise ValueError('Missing exact workload/output identity')
    if any(a['identity'][k] != b['identity'][k] for k in keys):
        raise ValueError('Exact workload/output identity mismatch')


def parse_probe(text, exit_code=0, *, instrumented=False, memory_observer=False):
    ids, rows = records(text, 'PD5_ID '), records(text, 'PD5_SAMPLE ')
    if type(exit_code) is not int or exit_code != 0 or len(ids) != 1 or ids[0].get('schema') != 'pd5.probe.v1':
        raise ValueError('Failed/incomplete PD5 probe')
    identity = ids[0]
    if identity.get('metrics_compile_enabled') is not instrumented or identity.get('allocator') != ('lifetime-observer' if memory_observer else 'normal'):
        raise ValueError('Instrumented PD5 build')
    expected = identity.get('counts', {})
    if not isinstance(expected, dict) or not expected or any(type(n) is not int or n <= 0 for n in expected.values()):
        raise ValueError('Invalid PD5 declared counts')
    if sum(expected.values()) != len(rows):
        raise ValueError('Incomplete PD5 sample count')
    result = {}
    for metric, count in expected.items():
        selected = [r for r in rows if r.get('metric') == metric]
        if type(count) is not int or count <= 0 or len(selected) != count:
            raise ValueError('Invalid metric count')
        if [r.get('ordinal') for r in selected] != list(range(count)):
            raise ValueError('Missing/duplicate metric ordinal')
        for row in selected:
            if row.get('complete') is not True:
                raise ValueError('Operation did not complete')
            if metric in ('pause','resume','reset_paused','reset_running','resize') and 'start_us' not in row:
                raise ValueError('Missing control event endpoints')
            if 'start_us' in row:
                start, confirmed, end = (positive(row[k], True) for k in ('start_us', 'confirmed_us', 'first_present_us'))
                if not start <= confirmed <= end or abs(row['elapsed_us'] - (end - start)) > 1:
                    raise ValueError('Invalid command/confirmation/first-frame timing')
        result[metric] = statistics([r['elapsed_us'] for r in selected])
    starts, frames = records(text, 'PD5_PROCESS_START '), records(text, 'PD5_FIRST_FRAME ')
    if frames:
        if len(starts) != 1 or len(frames) != 1 or starts[0]['frequency'] != frames[0]['frequency']:
            raise ValueError('Native process startup/first-frame clock binding missing')
        if starts[0].get('boundary') != 'before-native-CreateProcess' or starts[0].get('pid', 0) <= 0:
            raise ValueError('Unknown startup boundary')
        frequency = positive(frames[0]['frequency'])
        duration = positive(frames[0]['qpc'] - starts[0]['qpc'], True) * 1000000 / frequency
        rows.append({'metric':'load', 'ordinal':0, 'elapsed_us':duration, 'complete':True})
        result['load'] = statistics([duration])
    no_gameplay = identity.get('gameplay') is False and 'replay_equal' in identity and identity['replay_equal'] is None
    if identity.get('closed') is not True or (identity.get('replay_equal') is not True and not no_gameplay):
        raise ValueError('Probe did not close')
    return {'schema': 'pd5.probe.v1', 'identity': identity, 'samples': rows, 'statistics': result,
            'acceptance': False}


def collect(path, kind, exit_code=0, phase='formal'):
    text = Path(path).read_text(encoding='utf-8-sig', errors='strict')
    if kind == 'owner':
        return parse_owner(text, exit_code, phase)
    return {'host': parse_host, 'probe': parse_probe}[kind](text, exit_code)

"""Power-policy identity and bounded QPC/UTC alignment for diagnostic probes."""
import base64
from fractions import Fraction
import hashlib
import json
import math
import re
import subprocess


def power_policy(ac_line_status):
    records = {}
    for label, args in [('active_scheme', ['/getactivescheme']),
                        ('all_settings_including_hidden', ['/qh', 'SCHEME_CURRENT'])]:
        result = subprocess.run(['powercfg', *args], stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, timeout=30, check=True)
        records[label] = {'arguments': args, 'sha256': hashlib.sha256(result.stdout).hexdigest(),
                          'raw_base64': base64.b64encode(result.stdout).decode('ascii')}
        tokens = re.findall(rb'[0-9a-fA-F]{8}-(?:[0-9a-fA-F]{4}-){3}[0-9a-fA-F]{12}|0x[0-9a-fA-F]+', result.stdout)
        if not tokens:
            raise ValueError('Power query did not expose GUID/value tokens')
        records[label]['guid_value_tokens'] = [t.decode('ascii').lower() for t in tokens]
        records[label]['guid_value_sha256'] = hashlib.sha256(json.dumps(records[label]['guid_value_tokens']).encode()).hexdigest()
    identity = {'ac_line_status': ac_line_status,
                'queries': {k: v['guid_value_sha256'] for k, v in records.items()}}
    return {'revision': 'pd0-power-policy-v1', 'identity': identity,
            'fingerprint': hashlib.sha256(json.dumps(identity, sort_keys=True).encode()).hexdigest(),
            'queries': records}


def require_same_power(expected, actual):
    if expected['identity'] != actual['identity']:
        raise ValueError('Power policy or AC line status changed; diagnostic stops')


def validate_alignment(samples, anchors, frequency=None):
    if len(anchors) != 2 or [a['label'] for a in anchors] != ['begin', 'end']:
        raise ValueError('Exactly two ordered clock anchors required')
    first, last = anchors
    hz = first['qpc_frequency']
    if hz <= 0 or last['qpc_frequency'] != hz or (frequency is not None and frequency != hz):
        raise ValueError('QPC frequency mismatch')
    if (first['pid'], first['tid']) != (last['pid'], last['tid']):
        raise ValueError('Anchor process/thread identity mismatch')
    widths = [Fraction(a['qpc_after'] - a['qpc_before'], hz) * 1000000 for a in anchors]
    if any(w < 0 or w > 1000 for w in widths):
        raise ValueError('UTC anchor uncertainty exceeds registered 1ms limit')
    midpoints = [Fraction(a['qpc_after'] + a['qpc_before'], 2) for a in anchors]
    if midpoints[1] <= midpoints[0]:
        raise ValueError('Clock anchors did not advance')
    drift = Fraction(last['utc_filetime_100ns'] - first['utc_filetime_100ns'], 10) - (midpoints[1] - midpoints[0]) * 1000000 / hz
    if abs(drift) > 1000:
        raise ValueError('UTC/QPC mapping drift exceeds registered 1ms limit')
    # End-point consistency cannot exclude an unobserved intermediate UTC step.
    padding_100ns = abs(drift) * 10
    intervals = []
    previous_end = first['qpc_after']
    for sample in samples:
        begin, end = sample['qpc_start'], sample['qpc_end']
        if not previous_end <= begin < end <= last['qpc_before']:
            raise ValueError('QPC block is reversed, overlapping, or outside its anchors')
        expected_us = (end - begin) * 1000000 / hz
        if not math.isclose(sample['wall_us'], expected_us, rel_tol=1e-12, abs_tol=1e-6):
            raise ValueError('Wall interval does not match its QPC boundaries')
        def bounds(tick):
            lower = first['utc_filetime_100ns'] + Fraction((tick - first['qpc_after']) * 10000000, hz) - padding_100ns
            upper = first['utc_filetime_100ns'] + Fraction((tick - first['qpc_before']) * 10000000, hz) + padding_100ns
            return [math.floor(lower), math.ceil(upper)]
        intervals.append({'qpc_start': begin, 'qpc_end': end,
                          'utc_filetime_start_bounds': bounds(begin),
                          'utc_filetime_end_bounds': bounds(end)})
        previous_end = end
    return {'qpc_frequency': hz, 'pid': first['pid'], 'tid': first['tid'],
            'anchor_widths_us': list(map(float, widths)), 'endpoint_mapping_drift_us': float(drift),
            'mapping_assumption': 'no unobserved UTC step between end-point anchors',
            'intervals': intervals}

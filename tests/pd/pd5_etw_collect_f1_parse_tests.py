"""F1 collector parse-contract tests. No real ETW, WPR, administrator or Cuexis call.

These pin the shape the frozen F1 executable really prints, because an earlier
sealed collector assumed one PD_FOLLOW_CONTENT record per objects count instead
of the observed identity pair. The real log from that failure is preserved and
is validated here when the evidence directory is still present.
"""
import json
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
PACKAGE = ROOT / 'tools/verification/playback_performance'
SEALED = PACKAGE / 'etw_safety'
sys.path.insert(0, str(SEALED))
import collect_f1_scale as collector

ARCHIVED_FAILURE = (ROOT / 'out/pd5-etw-f1-tools-20261023-3e5837f-r2'
                    / 'f1-20261023T122949Z-8ca88299bbd24d30a8c40ba75ec191b5'
                    / 'f1-scale/f1-native/f1-scale-0.log')


def plan():
    return json.loads((SEALED / 'plan.json').read_text(encoding='utf-8'))


def digest(objects):
    return format(objects, '04x') * 16


def row(objects, mode, block, operations, elapsed):
    return {'objects': objects, 'mode': mode, 'block': block,
            'operations': operations, 'elapsed_us': elapsed, 'cpu_us': elapsed - 1000,
            'cycles': int(elapsed * 1000), 'digest': 1, 'ordinal': 64}


def content(objects, sha):
    return {'objects': objects, 'sha256': digest(objects) if sha is None else sha}


def rows_and_contents(spec):
    counts = [int(part) for part in spec['workload']['counts'].split()]
    pairs = {counts[index]: counts[index + 1:index + 3] for index in range(0, len(counts), 3)}
    rows, contents = [], []
    for objects in spec['workload']['objects']:
        read_ops, update_ops = pairs[objects]
        contents.append(content(objects, None))
        rows.extend(row(objects, 'read', block, read_ops, 500000.0)
                    for block in range(spec['workload']['blocks_per_metric']))
        contents.append(content(objects, None))
        rows.extend(row(objects, 'updated', block, update_ops, 380000.0)
                    for block in range(spec['workload']['blocks_per_metric']))
    return rows, contents


def log_text(spec, warmup_pid=2508, measured_pid=29952, drop_row=None,
             read_elapsed=500000.0):
    rows, contents = rows_and_contents(spec)
    lines = ['"CDash" style output starts here']
    lines.append('PD5_PROCESS_START ' + json.dumps(
        {'pid': warmup_pid, 'qpc': 919406213446, 'frequency': 10000000,
         'boundary': 'before-native-CreateProcess'}))
    lines.append('Filters: "' + spec['workload']['warmup_filter'] + '"')
    lines.append('PD_MACHINE_WARMUP {"seconds":30,"checksum":10774485851396243457}')
    lines.append('test cases: 1 | 1 passed')
    lines.append('')
    lines.append('PD5_PROCESS_START ' + json.dumps(
        {'pid': measured_pid, 'qpc': 919707542723, 'frequency': 10000000,
         'boundary': 'before-native-CreateProcess'}))
    lines.append('Filters: "' + spec['workload']['measurement_filter'] + '"')
    lines.append('PD_ENV ' + json.dumps(env(measured_pid, 'begin')))
    blocks = spec['workload']['blocks_per_metric']
    next_row = 0

    def emit(count):
        nonlocal next_row
        for _ in range(count):
            item = dict(rows[next_row])
            next_row += 1
            if drop_row == (item['objects'], item['mode'], item['block']):
                continue
            if item['mode'] == 'read':
                item['elapsed_us'] = read_elapsed
            lines.append('PD_FOLLOW_ROW ' + json.dumps(item))

    for pair in range(len(spec['workload']['objects'])):
        # one identity pair per objects count: before and after the read blocks
        lines.append('PD_FOLLOW_CONTENT ' + json.dumps(contents[2 * pair]))
        emit(blocks)
        lines.append('PD_FOLLOW_CONTENT ' + json.dumps(contents[2 * pair + 1]))
        emit(blocks)
    lines.append('PD_ENV ' + json.dumps(env(measured_pid, 'end')))
    lines.append('All tests passed (109 assertions in 1 test case)')
    return '\n'.join(lines) + '\n'


def env(pid, label):
    return {'label': label, 'pid': pid, 'tid': 25620, 'process_affinity_error': 0,
            'process_affinity': 4, 'system_affinity': 65535, 'processor_group_error': 0,
            'processor_group': 0, 'processor_number': 2, 'process_priority': 32,
            'thread_priority': 0, 'process_power_error': 0, 'process_power_control': 0,
            'process_power_state': 0, 'thread_power_error': 0, 'thread_power_control': 0,
            'thread_power_state': 0}


class RealShapeTests(unittest.TestCase):
    def setUp(self):
        self.spec = plan()

    def parse(self, text):
        return (collector.marker_records(text, 'PD5_PROCESS_START'),
                collector.marker_records(text, 'PD_FOLLOW_ROW'),
                collector.marker_records(text, 'PD_FOLLOW_CONTENT'))

    def test_frozen_shape_has_one_identity_pair_per_objects_count(self):
        starts, rows, contents = self.parse(log_text(self.spec))
        self.assertEqual(len(starts), 2)
        self.assertEqual(len(rows), 24)
        self.assertEqual(len(contents), 8)
        collector.validate_output(rows, contents, self.spec)
        collector.validate_anchors(
            starts, [{'record': env(29952, 'begin')}, {'record': env(29952, 'end')}], 29952)

    def test_one_content_per_objects_count_is_refused(self):
        _, rows, contents = self.parse(log_text(self.spec))
        with self.assertRaises(ValueError) as caught:
            collector.validate_output(rows, contents[0::2], self.spec)
        self.assertIn('PD_FOLLOW_CONTENT', str(caught.exception))

    def test_content_order_must_follow_the_fixed_objects_sequence(self):
        _, rows, contents = self.parse(log_text(self.spec))
        swapped = [contents[2], contents[3], contents[0], contents[1]] + contents[4:]
        with self.assertRaises(ValueError) as caught:
            collector.validate_output(rows, swapped, self.spec)
        self.assertIn('order', str(caught.exception))

    def test_content_change_across_the_read_blocks_is_refused(self):
        _, rows, contents = self.parse(log_text(self.spec))
        broken = [dict(row) for row in contents]
        broken[1]['sha256'] = 'f' * 64
        with self.assertRaises(ValueError) as caught:
            collector.validate_output(rows, broken, self.spec)
        self.assertIn('content changed', str(caught.exception))

    def test_content_record_without_a_digest_is_refused(self):
        _, rows, contents = self.parse(log_text(self.spec))
        broken = [dict(row) for row in contents]
        broken[0]['sha256'] = 'not-a-digest'
        with self.assertRaises(ValueError) as caught:
            collector.validate_output(rows, broken, self.spec)
        self.assertIn('sha256', str(caught.exception))

    def test_missing_row_is_still_a_failure(self):
        _, rows, contents = self.parse(log_text(self.spec, drop_row=(8, 'read', 1)))
        self.assertEqual(len(rows), 23)
        with self.assertRaises(ValueError) as caught:
            collector.validate_output(rows, contents, self.spec)
        self.assertIn('PD_FOLLOW_ROW', str(caught.exception))

    def test_operation_count_drift_is_refused(self):
        _, rows, contents = self.parse(log_text(self.spec))
        broken = [dict(row) for row in rows]
        broken[0]['operations'] = broken[0]['operations'] + 1
        with self.assertRaises(ValueError) as caught:
            collector.validate_output(broken, contents, self.spec)
        self.assertIn('operation count mismatch', str(caught.exception))

    def test_short_read_block_is_refused(self):
        _, rows, contents = self.parse(log_text(self.spec, read_elapsed=99999.0))
        with self.assertRaises(ValueError) as caught:
            collector.validate_output(rows, contents, self.spec)
        self.assertIn('read block below', str(caught.exception))

    def test_duplicate_block_is_refused(self):
        _, rows, contents = self.parse(log_text(self.spec))
        broken = [dict(row) for row in rows]
        broken[1]['block'] = 0
        with self.assertRaises(ValueError) as caught:
            collector.validate_output(broken, contents, self.spec)
        self.assertIn('missing/duplicate block', str(caught.exception))

    def test_anchor_sequence_needs_one_warmup_and_one_measured_start(self):
        starts, _, _ = self.parse(log_text(self.spec))
        envs = [{'record': env(29952, 'begin')}, {'record': env(29952, 'end')}]
        collector.validate_anchors(starts, envs, 29952)
        with self.assertRaises(ValueError):
            collector.validate_anchors(starts[:1], envs, 29952)
        with self.assertRaises(ValueError):
            collector.validate_anchors(starts + starts[1:], envs, 29952)

    def test_measured_env_must_report_mask_four(self):
        starts, _, _ = self.parse(log_text(self.spec))
        broken = env(29952, 'begin')
        broken['process_affinity'] = 3
        envs = [{'record': broken}, {'record': env(29952, 'end')}]
        with self.assertRaises(ValueError) as caught:
            collector.validate_anchors(starts, envs, 29952)
        self.assertIn('affinity', str(caught.exception))

    def test_malformed_marker_json_is_reported(self):
        with self.assertRaises(ValueError) as caught:
            collector.marker_records('PD_FOLLOW_ROW {not-json}\n', 'PD_FOLLOW_ROW')
        self.assertIn('malformed PD_FOLLOW_ROW JSON', str(caught.exception))

    def test_empty_log_yields_no_records(self):
        self.assertEqual(collector.marker_records('', 'PD_FOLLOW_ROW'), [])
        _, rows, contents = self.parse('nothing here\n')
        with self.assertRaises(ValueError):
            collector.validate_output(rows, contents, self.spec)


class WorkloadShapeTests(unittest.TestCase):
    def setUp(self):
        self.spec = plan()

    def broken(self, mutate):
        value = json.loads(json.dumps(self.spec))
        mutate(value['workload'])
        return value

    def test_sealed_plan_matches_the_collector_shape(self):
        collector.validate_workload_shape(self.spec)

    def test_other_shapes_are_refused(self):
        for mutate in (lambda w: w.__setitem__('objects', [1, 8, 64]),
                       lambda w: w.__setitem__('objects', [1, 8, 256, 64]),
                       lambda w: w.__setitem__('modes', ['read']),
                       lambda w: w.__setitem__('blocks_per_metric', 4),
                       lambda w: w.__setitem__('counts', '1 2097152 262144 8 1048576 131072'),
                       lambda w: w.__setitem__(
                           'counts', '8 1048576 131072 1 2097152 262144 '
                                     '64 262144 32768 256 65536 8192')):
            with self.assertRaises(RuntimeError, msg=str(mutate)):
                collector.validate_workload_shape(self.broken(mutate))


@unittest.skipUnless(ARCHIVED_FAILURE.exists(),
                     'archived failing F1 log is not on this machine')
class ArchivedFailureLogTests(unittest.TestCase):
    def test_preserved_failing_log_matches_the_fixed_contract(self):
        text = ARCHIVED_FAILURE.read_text(encoding='utf-8-sig')
        rows = collector.marker_records(text, 'PD_FOLLOW_ROW')
        contents = collector.marker_records(text, 'PD_FOLLOW_CONTENT')
        starts = collector.marker_records(text, 'PD5_PROCESS_START')
        self.assertEqual((len(starts), len(rows), len(contents)), (2, 24, 8))
        collector.validate_output(rows, contents, plan())


if __name__ == '__main__':
    unittest.main()

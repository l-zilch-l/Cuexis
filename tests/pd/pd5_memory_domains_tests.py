import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.verification.playback_performance import memory_domains as domains


def cell(owner, domain, live=0, peak=None, requested=0, peak_requested=None, storage=None,
         peak_storage=None, age=0):
    peak = live if peak is None else peak
    peak_requested = requested if peak_requested is None else peak_requested
    storage = peak_requested if storage is None else storage
    peak_storage = max(peak_requested, storage) if peak_storage is None else peak_storage
    return {'owner': owner, 'domain': domain, 'live_count': live,
            'requested_cpp_bytes': requested, 'crt_usable_storage_bytes': storage,
            'peak_live_count': peak, 'peak_requested_cpp_bytes': peak_requested,
            'peak_crt_usable_storage_bytes': peak_storage, 'oldest_live_age_us': age}


def text(checkpoints, memory=True, totals=None, exceeded=False):
    lines = []
    for label, cells in checkpoints:
        sums = {key: sum(c[key] for c in cells) for key in
                ('live_count', 'requested_cpp_bytes', 'crt_usable_storage_bytes')}
        if memory:
            lines.append('PD5_MEMORY ' + json.dumps(
                {'schema': 'pd5.memory.v1', 'label': label, 'exceeded': exceeded,
                 'live_count': sums['live_count'],
                 'requested_cpp_bytes': sums['requested_cpp_bytes'],
                 'crt_usable_storage_bytes': sums['crt_usable_storage_bytes'],
                 'peak_live_count': sums['live_count'],
                 'peak_requested_cpp_bytes': sums['requested_cpp_bytes'],
                 'peak_crt_usable_storage_bytes': sums['crt_usable_storage_bytes'],
                 'oldest_live_age_us': max((c['oldest_live_age_us'] for c in cells), default=0)}))
        for c in cells:
            if c['live_count'] or c['peak_live_count']:
                lines.append('PD5_MEMORY_DOMAIN ' + json.dumps(
                    {'schema': 'pd5.memory.domain.v1', 'label': label,
                     'owner': c['owner'], 'domain': c['domain'],
                     'cell': c['owner'] + '.' + c['domain'], **c}))
        untagged = sum(c['live_count'] for c in cells if c['owner'] == 'unknown')
        row = {'schema': 'pd5.memory.partition.v1', 'label': label,
               'exact': True, 'cells': len(cells), 'untagged_live_count': untagged,
               'sum_live_count': sums['live_count'],
               'global_live_count': sums['live_count'],
               'sum_requested_cpp_bytes': sums['requested_cpp_bytes'],
               'global_requested_cpp_bytes': sums['requested_cpp_bytes'],
               'sum_crt_usable_storage_bytes': sums['crt_usable_storage_bytes'],
               'global_crt_usable_storage_bytes': sums['crt_usable_storage_bytes'],
               'exceeded': False}
        if totals:
            row.update(totals)
        lines.append('PD5_MEMORY_PARTITION ' + json.dumps(row))
    return '\n'.join(lines) + '\n'


def caps(profile, cells, entities=1):
    return {'schema': 'pd5.memory.caps.v1', 'contents': [
        {'profile': profile, 'content_entities': entities, 'cells': cells}]}


def cap(name, kind='bytes', count=None, requested=None, coverage='covered'):
    return {'cell': name, 'cap_kind': kind, 'cap_count': count,
            'cap_requested_bytes': requested, 'coverage': coverage}


def collect(checkpoints, **kwargs):
    return domains.collect(text(checkpoints, **kwargs))


class CollectTests(unittest.TestCase):
    def test_classification_is_total_and_static(self):
        for owner in domains.OWNERS:
            for name in domains.DOMAINS:
                kind, cap_cell = domains.classify(owner, name)
                self.assertIn(kind, ('mapped', 'excluded', 'unrated', 'harness', 'untagged'))
                if kind == 'mapped':
                    self.assertEqual(cap_cell, domains.PRODUCT_MAPPED[name])
                else:
                    self.assertIsNone(cap_cell)

    def test_unknown_axis_values_fail_closed(self):
        with self.assertRaisesRegex(domains.DomainError, 'Unknown owner'):
            domains.classify('gpu', 'current')
        with self.assertRaisesRegex(domains.DomainError, 'Unknown domain'):
            domains.classify('main', 'gpu-memory')

    def test_collect_classifies_cells_and_partitions(self):
        document = collect([('before-load', [cell('probe', 'fixture', 1, requested=64)]),
                            ('after-load', [cell('main', 'candidate', 2, requested=256),
                                            cell('unknown', 'other', 1, requested=32)])])
        self.assertEqual(document['schema'], 'pd5.memory.domains.v1')
        self.assertEqual(document['labels'], ['before-load', 'after-load'])
        kinds = {(c['label'], c['cell']): c['classification'] for c in document['cells']}
        self.assertEqual(kinds[('before-load', 'probe.fixture')], 'harness')
        self.assertEqual(kinds[('after-load', 'main.candidate')], 'mapped')
        self.assertEqual(kinds[('after-load', 'unknown.other')], 'untagged')
        self.assertEqual(document['totals']['untagged_peak_requested_cpp_bytes'], 32)
        partition = document['partitions'][1]
        self.assertEqual(partition['observed_cells'], 2)
        self.assertEqual(partition['global_memory']['requested_cpp_bytes'], 288)

    def test_partition_sum_must_match_its_cells(self):
        text_value = text([('a', [cell('main', 'current', 1, requested=8)])])
        text_value = text_value.replace('"sum_live_count": 1', '"sum_live_count": 2')
        with self.assertRaisesRegex(domains.DomainError, 'Partition sum differs'):
            domains.check(domains.collect(text_value))

    def test_partition_must_be_exact(self):
        text_value = text([('a', [])]).replace('"exact": true', '"exact": false')
        with self.assertRaisesRegex(domains.DomainError, 'not exact'):
            domains.check(domains.collect(text_value))

    def test_duplicate_label_fails(self):
        text_value = text([('a', []), ('a', [])])
        with self.assertRaisesRegex(domains.DomainError, 'Duplicate partition label'):
            domains.collect(text_value)

    def test_domain_without_partition_fails(self):
        text_value = text([('a', [])])
        text_value += 'PD5_MEMORY_DOMAIN ' + json.dumps(
            {'schema': 'pd5.memory.domain.v1', 'label': 'b', 'owner': 'main',
             'domain': 'current', 'cell': 'main.current', **cell('main', 'current', 1)}) + '\n'
        with self.assertRaisesRegex(domains.DomainError, 'without a partition'):
            domains.collect(text_value)

    def test_missing_partition_fails(self):
        with self.assertRaisesRegex(domains.DomainError, 'No PD5_MEMORY_PARTITION'):
            domains.collect(text([('a', [])]).split('PD5_MEMORY_PARTITION')[0])

    def test_empty_cell_is_not_evidence(self):
        text_value = text([('a', [])])
        row = {'schema': 'pd5.memory.domain.v1', 'label': 'a', 'owner': 'main',
               'domain': 'current', 'cell': 'main.current', **cell('main', 'current')}
        text_value = text_value.replace('PD5_MEMORY_PARTITION',
                                        'PD5_MEMORY_DOMAIN ' + json.dumps(row) +
                                        '\nPD5_MEMORY_PARTITION')
        with self.assertRaisesRegex(domains.DomainError, 'Empty cell'):
            domains.collect(text_value)

    def test_requested_bytes_cannot_exceed_storage(self):
        bad = cell('main', 'current', 1, requested=64)
        bad['crt_usable_storage_bytes'] = 32
        with self.assertRaisesRegex(domains.DomainError, 'exceed CRT usable storage'):
            domains.collect(text([('a', [bad])]))

    def test_cell_high_water_must_not_regress(self):
        text_value = text([('a', [cell('main', 'current', 1, 5, requested=80, peak_requested=80)]),
                           ('b', [cell('main', 'current', 0, 4, requested=0, peak_requested=64)])])
        with self.assertRaisesRegex(domains.DomainError, 'lost a transient'):
            domains.check(domains.collect(text_value))

    def test_domain_books_must_match_memory_books(self):
        text_value = text([('a', [cell('main', 'current', 1, requested=8)])])
        text_value = text_value.replace(
            '"label": "a", "exceeded": false, "live_count": 1, "requested_cpp_bytes": 8',
            '"label": "a", "exceeded": false, "live_count": 1, "requested_cpp_bytes": 9')
        with self.assertRaisesRegex(domains.DomainError, 'differ from PD5_MEMORY'):
            domains.check(domains.collect(text_value))

    def test_untagged_bytes_must_match_unknown_other(self):
        text_value = text([('a', [cell('unknown', 'other', 1, requested=8)])])
        text_value = text_value.replace('"untagged_live_count": 1',
                                        '"untagged_live_count": 2')
        with self.assertRaisesRegex(domains.DomainError, 'Untagged live count differs'):
            domains.check(domains.collect(text_value))

    def test_source_identity_is_recorded(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'log.txt'
            path.write_text(text([('a', [])]), encoding='utf-8')
            document = domains.collect(path.read_text(encoding='utf-8'), source=path)
            self.assertEqual(len(document['source']['sha256']), 64)
            self.assertEqual(document['source']['bytes'], path.stat().st_size)

    def test_no_checkpoint_is_not_evidence(self):
        with self.assertRaisesRegex(domains.DomainError, 'No PD5_MEMORY_PARTITION'):
            domains.collect('host output without observer records\n')


class CheckTests(unittest.TestCase):
    def test_untagged_allowance(self):
        document = collect([('a', [cell('unknown', 'other', 1, requested=64)])])
        domains.check(document, max_untagged_bytes=64)
        self.assertEqual(domains.check(document)['untagged']['worst_cell'], 'unknown.other')
        with self.assertRaisesRegex(domains.DomainError, 'Untagged peak exceeds'):
            domains.check(document, max_untagged_bytes=63)

    def test_age_cap_applies_to_live_cells(self):
        document = collect([('a', [cell('main', 'current', 1, requested=8, age=10)])])
        domains.check(document, max_age_us=10)
        with self.assertRaisesRegex(domains.DomainError, 'Allocation age exceeds'):
            domains.check(document, max_age_us=9)

    def test_required_checkpoints_are_enforced(self):
        document = collect([('before-load', []), ('after-close', [])])
        domains.check(document, require_labels=('before-load', 'after-close'))
        with self.assertRaisesRegex(domains.DomainError, 'Missing required checkpoint'):
            domains.check(document, require_labels=('after-all',))

    def test_final_zero_is_checked_at_the_named_checkpoint(self):
        released = [('before-load', [cell('main', 'candidate', 1, requested=8)]),
                    ('after-all', [])]
        domains.check(collect(released), expect_zero=('after-all',))
        with self.assertRaisesRegex(domains.DomainError, 'not zero after release'):
            domains.check(collect(released), expect_zero=('before-load',))
        with self.assertRaisesRegex(domains.DomainError, 'is not present'):
            domains.check(collect(released), expect_zero=('after-close',))

    def test_envelope_overflow_is_attributed_not_dropped(self):
        document = collect([('a', [cell('main', 'candidate', 1, requested=8)])], exceeded=True)
        verdict = domains.check(document)
        self.assertEqual(verdict['envelope_exceeded_labels'], ['a'])
        with self.assertRaisesRegex(domains.DomainError, 'envelope exceeded'):
            domains.check(document, strict=True)

    def test_caps_compare_peak_not_live(self):
        document = collect([('a', [cell('main', 'candidate', 1, 3, requested=8, peak_requested=96)])])
        declared = caps('p', [cap('state.candidate', requested=96)])
        domains.check(document, declared, 'p', 1)
        declared = caps('p', [cap('state.candidate', requested=95)])
        with self.assertRaisesRegex(domains.DomainError, 'Observed bytes exceed cap'):
            domains.check(document, declared, 'p', 1)

    def test_caps_count_kind(self):
        document = collect([('a', [cell('main', 'receipt', 2, 5, requested=16)])])
        declared = caps('p', [cap('delivery.receipt', 'both', count=5, requested=64)])
        domains.check(document, declared, 'p', 1)
        declared = caps('p', [cap('delivery.receipt', 'count', count=4, requested=None)])
        with self.assertRaisesRegex(domains.DomainError, 'Observed count exceeds cap'):
            domains.check(document, declared, 'p', 1)

    def test_unallocated_cell_must_stay_empty(self):
        document = collect([('a', [cell('main', 'gpu', 1, requested=8)])])
        declared = caps('p', [cap('gpu.inflight', 'both', coverage='NA:no GPU')])
        with self.assertRaisesRegex(domains.DomainError, 'unallocated cap cell'):
            domains.check(document, declared, 'p', 1)

    def test_uncaptured_covered_cell_blocks_strict(self):
        document = collect([('a', [cell('main', 'candidate', 1, requested=8)])])
        declared = caps('p', [cap('state.candidate', requested=64),
                              cap('state.world', requested=64)])
        verdict = domains.check(document, declared, 'p', 1)
        self.assertEqual(verdict['caps']['blocking_uncaptured_cells'], ['state.world'])
        with self.assertRaisesRegex(domains.DomainError, 'Strict attribution failed'):
            domains.check(document, declared, 'p', 1, strict=True)
        domains.check(document, declared, 'p', 1, uncaptured_ok=['state.world'],
                      strict=True)

    def test_count_only_cap_cells_are_not_uncaptured(self):
        document = collect([('a', [cell('main', 'candidate', 1, requested=8)])])
        declared = caps('p', [cap('state.candidate', requested=64),
                              cap('command.session', 'count', count=80),
                              cap('consumer.slots', 'count', count=8)])
        verdict = domains.check(document, declared, 'p', 1, strict=True)
        self.assertEqual(verdict['caps']['uncaptured_cells'], [])

    def test_caps_entity_count_must_match(self):
        document = collect([('a', [cell('main', 'candidate', 1, requested=8)])])
        declared = caps('p', [cap('state.candidate', requested=64)], entities=40000)
        with self.assertRaisesRegex(domains.DomainError, 'declares 40000 entities'):
            domains.check(document, declared, 'p', 1)

    def test_caps_without_profile_is_rejected(self):
        declared = caps('p', [cap('state.candidate', requested=64)])
        with self.assertRaisesRegex(domains.DomainError, 'requires a profile'):
            domains.check(collect([('a', [])]), declared)

    def test_unknown_caps_schema_is_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'caps.json'
            path.write_text(json.dumps({'schema': 'nope'}), encoding='utf-8')
            with self.assertRaisesRegex(domains.DomainError, 'Unexpected caps schema'):
                domains._load_caps(path)


class CommandTests(unittest.TestCase):
    def test_cli_round_trip(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            log = root / 'observer.log'
            log.write_text(text([('before-load', []),
                                 ('after-close', [cell('main', 'candidate', 1, requested=64)])]),
                           encoding='utf-8')
            out = root / 'domains.json'
            self.assertEqual(domains.main(['collect-domains', str(log), '--json', str(out)]), 0)
            document = json.loads(out.read_text(encoding='utf-8'))
            self.assertEqual(document['schema'], 'pd5.memory.domains.v1')
            caps_path = root / 'caps.json'
            caps_path.write_text(json.dumps(caps('p', [cap('state.candidate', requested=64)])),
                                 encoding='utf-8')
            verdict_path = root / 'verdict.json'
            self.assertEqual(domains.main(
                ['check-domains', str(log), '--caps', str(caps_path), '--profile', 'p',
                 '--entities', '1', '--json', str(verdict_path)]), 0)
            verdict = json.loads(verdict_path.read_text(encoding='utf-8'))
            self.assertEqual(verdict['schema'], 'pd5.memory.domains.check.v1')
            self.assertTrue(verdict['passed'])

    def test_cli_failure_is_a_clean_exit(self):
        with tempfile.TemporaryDirectory() as folder:
            log = Path(folder) / 'observer.log'
            log.write_text('not a log\n', encoding='utf-8')
            self.assertEqual(domains.main(['collect-domains', str(log)]), 1)


if __name__ == '__main__':
    unittest.main()

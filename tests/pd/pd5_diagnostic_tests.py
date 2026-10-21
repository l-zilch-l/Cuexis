import copy
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.verification.playback_performance import diagnostic as d
from tools.verification.playback_performance import build_profiles as builds


def inputs():
    identities = {side: dict(source_sha256=letter * 64, binary_sha256=letter * 64)
                  for side, letter in (('A', 'a'), ('B', 'b'))}
    pairs = []
    for i in range(3):
        pair = []
        for side in ('A', 'B'):
            pair.append(dict(schema='playback.diagnostic-process.v1', method=d.METHOD,
                             protocol=d.PROTOCOL, **identities[side], run_id=f'{side}-{i}', pid=i+1,
                             output_validated=True,
                             workload=dict(objects=1, cameras=1, registry_entities=2, U=1, F=1000,
                                           R=0, content_sha256='c'*64, sequence_sha256='d'*64,
                                           viewport=[1280, 720], output_digest='42'),
                             metric=dict(name='ready', start='request', end='ready', unit='us',
                                         sample_kind='operation'),
                             samples=[dict(elapsed_us=100, operations=1)]*1000))
        pairs.append(pair)
    return pairs, identities


class DiagnosticTests(unittest.TestCase):
    def test_common_ready_is_diagnostic_only(self):
        pairs, ids = inputs()
        result = d.compare(pairs, ids)
        self.assertFalse(result['acceptance'])
        self.assertEqual(result['effect']['n_pairs'], 3)
        self.assertEqual(result['effect']['status'], 'diagnostic_pass')

    def test_completion_endpoint_mismatch_rejected(self):
        pairs, ids = inputs()
        pairs[0][1]['metric']['end'] = 'present-return'
        with self.assertRaises(ValueError): d.compare(pairs, ids)

    def test_identity_work_missing_and_nonpositive_rejected(self):
        for change in (lambda p: p.update(binary_sha256='e'*64),
                       lambda p: p['workload'].pop('cameras'),
                       lambda p: p['workload'].update(U=2),
                       lambda p: p.update(output_validated=False),
                       lambda p: p['samples'][0].update(elapsed_us=0)):
            pairs, ids = inputs()
            pairs = copy.deepcopy(pairs)
            change(pairs[0][1])
            with self.assertRaises((ValueError, KeyError)): d.compare(pairs, ids)

    def test_blocks_cannot_inflate_pairs_or_quantiles(self):
        pairs, ids = inputs()
        with self.assertRaises(ValueError): d.compare([pairs[0]]*3, ids)
        with self.assertRaises(ValueError): d.compare(pairs[:2], ids)
        p = pairs[0][0]
        p['metric']['sample_kind'] = 'block_average'
        p['samples'] = [dict(elapsed_us=100000, operations=1000)]*1000
        self.assertIsNone(d.validate(p, ids['A'])['p99_us'])
        p['samples'][0] = dict(elapsed_us=99999, operations=1000)
        with self.assertRaises(ValueError): d.validate(p, ids['A'])

    def test_noise_prevents_diagnostic_pass(self):
        pairs, ids = inputs()
        pairs[2][1]['samples'] = [dict(elapsed_us=200, operations=1)]*1000
        self.assertEqual(d.compare(pairs, ids)['effect']['status'], 'inconclusive')

    def test_build_override_does_not_resolve_to_historical_tree(self):
        root = Path.cwd()
        fresh = root / 'out/build/followup-test'
        self.assertEqual(builds.build_directory(root, 'release', fresh), fresh.resolve())
        self.assertEqual(builds.build_directory(root, 'debug'), (root/'out/build/pd5-ready-debug').resolve())


if __name__ == '__main__':
    unittest.main()

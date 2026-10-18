import unittest
import os
from pathlib import Path
import tempfile
from unittest.mock import patch
from formal_baseline import (COMBINATIONS, OWNER_METRICS, repeatability_valid,
                             select_registered_mask, summaries, validate_runs,
                             validate_dependency_inventory)


def runs(values):
    result=[]
    for config,profile in COMBINATIONS:
        for i,value in enumerate(values):
            result.append({'kind':'single_owner','config':config,'profile':profile,'window':1 if i<3 else 2,
                'data':{'samples':[{'kind':metric,'operations':1,'sample':{'wall_us':value}}
                                   for metric in OWNER_METRICS]}})
    return result


class FormalBaselineTests(unittest.TestCase):
    def test_zero_header_dependencies_are_rejected(self):
        with self.assertRaisesRegex(ValueError, 'Missing Ninja'):
            validate_dependency_inventory('probe.obj: #deps 0, deps mtime 1 (VALID)\n',
                                          Path('absent.obj'), Path('absent.cpp'), Path('.'))

    def test_stale_header_is_rejected_even_with_a_valid_dependency_record(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            header = root / 'tests/pd/measurement_environment.hpp'
            header.parent.mkdir(parents=True)
            obj, source = root / 'probe.obj', root / 'probe.cpp'
            for path, stamp in ((obj, 2000000000), (source, 1000000000), (header, 3000000000)):
                path.write_text('test')
                os.utime(path, ns=(stamp, stamp))
            text = 'probe.obj: #deps 1, deps mtime 1 (VALID)\n    ' + str(header) + '\n'
            with patch('formal_baseline.ROOT', root):
                with self.assertRaisesRegex(ValueError, 'older than'):
                    validate_dependency_inventory(text, obj, source, root)
                os.utime(obj, ns=(4000000000, 4000000000))
                self.assertEqual(validate_dependency_inventory(text, obj, source, root)['dependencies'], 1)

    def test_missing_measurement_header_is_not_a_complete_probe_binding(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            text = 'probe.obj: #deps 1, deps mtime 1 (VALID)\n    other.hpp\n'
            with patch('formal_baseline.ROOT', root):
                with self.assertRaisesRegex(ValueError, 'does not track'):
                    validate_dependency_inventory(text, root / 'probe.obj', root / 'probe.cpp', root)

    def test_candidate_affinity_groups_are_compared_independently(self):
        def run(mask, priority=32):
            return {'mask': mask, 'config': 'release', 'profile': 'legacy',
                    'data': {'identity': {'profile': 'legacy', 'digest': 1,
                                          'replay_sha256': 'replay',
                                          'snapshot_sha256': 'snapshot',
                                          'packed_sha256': 'packed'},
                             'environment': [
                                 {'label': 'begin', 'pid': 1, 'tid': 1,
                                  'processor_group': 0, 'processor_number': 0,
                                  'process_affinity': mask, 'system_affinity': 15,
                                  'process_priority': priority},
                                 {'label': 'end', 'pid': 2, 'tid': 2,
                                  'processor_group': 0, 'processor_number': 0,
                                  'process_affinity': mask, 'system_affinity': 15,
                                  'process_priority': priority}]}}
        validate_runs([run(1), run(4)])
        with self.assertRaises(ValueError):
            validate_runs([run(1), run(1, priority=64)])

    def test_cross_window_shift_fails_even_when_each_window_is_flat(self):
        result,_=summaries(runs([100,100,100,120,120,120]))
        self.assertEqual(result[0]['window_noise'],[0,0])
        self.assertGreater(result[0]['combined_noise'],.15)
        self.assertFalse(repeatability_valid(result,len(COMBINATIONS)*len(OWNER_METRICS)))

    def test_fifteen_percent_limit_and_slow_sample_are_retained(self):
        result,_=summaries(runs([100,100,115,100,100,115]))
        self.assertTrue(repeatability_valid(result,len(COMBINATIONS)*len(OWNER_METRICS)))
        bad,_=summaries(runs([100,100,115,100,100,130]))
        self.assertIn(130,bad[0]['process_medians'])
        self.assertFalse(repeatability_valid(bad,len(COMBINATIONS)*len(OWNER_METRICS)))

    def test_incomplete_group_is_not_a_pass(self):
        result,_=summaries(runs([100,100,100]))
        self.assertFalse(repeatability_valid(result,len(COMBINATIONS)*len(OWNER_METRICS)))
        self.assertFalse(repeatability_valid([],2))

    def test_all_owner_metrics_are_required(self):
        result,_=summaries(runs([100,100,100,100,100,100]))
        incomplete=[row for row in result if row['metric'] != 'playback_only']
        self.assertFalse(repeatability_valid(incomplete,len(COMBINATIONS)*len(OWNER_METRICS)))

    def test_core_selection_is_registered_before_noise_is_observed(self):
        topology={'candidate_masks':[8,32]}
        self.assertEqual(select_registered_mask(topology),8)


if __name__=='__main__':
    unittest.main()

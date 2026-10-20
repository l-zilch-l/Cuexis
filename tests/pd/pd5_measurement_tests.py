import sys
from pathlib import Path
import copy
import json
import math
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.verification.playback_performance import measurement as m


def host(schema='pd4.present-return.v1'):
    identity = dict(measurement_schema=schema, digest=42, source='fixture', viewport=[1280,720],
                    objects=2, warmup=32, measured=512, audio=False, gpu=False, closed=True, recording=False)
    if schema.startswith('pd4'):
        identity['receipt_mode'] = 'asynchronous'
    rows = [dict(ordinal=i, **{f: 10. for f in m.HOST_FIELDS[schema]}) for i in range(512)]
    return 'PD_HOST_ID ' + json.dumps(identity) + '\n' + '\n'.join('PD_HOST_SAMPLE ' + json.dumps(r) for r in rows)


class Pd5MeasurementTests(unittest.TestCase):
    def test_ack_and_async_are_distinct(self):
        a, b = m.parse_host(host('pd0.receipt-ack.v1')), m.parse_host(host())
        m.require_comparable(a, b, 'request_to_ready_us')
        with self.assertRaises(ValueError):
            m.require_comparable(a, b, 'request_to_receipt_ack_us')

    def test_missing_reordered_nan_and_wrong_mode_rejected(self):
        for text in [host().rsplit('\n',1)[0], host().replace('"ordinal": 1,','"ordinal": 0,'),
                     host().replace('10.0','NaN',1), host().replace('"asynchronous"','"ack"')]:
            with self.subTest(text=text[:30]), self.assertRaises((ValueError, KeyError)):
                m.parse_host(text)

    def test_failed_run_and_recording_rejected(self):
        with self.assertRaises(ValueError): m.parse_host(host(), 1)
        with self.assertRaises(ValueError): m.parse_host(host().replace('"recording": false','"recording": true'))
        with self.assertRaises(ValueError): m.parse_host(host(), None)
        with self.assertRaises(ValueError): m.parse_host(host(), False)

    def test_workload_identity_must_match(self):
        a, b = m.parse_host(host()), m.parse_host(host().replace('"digest": 42','"digest": 43'))
        with self.assertRaises(ValueError): m.require_comparable(a,b,'request_to_ready_us')

    def test_noise_slow_sample_and_cross_window_drift_retained(self):
        windows = [[100]*3 for _ in range(3)]
        self.assertEqual(m.repeatability(windows)['status'],'pass')
        windows[2] = [120]*3
        self.assertEqual(m.repeatability(windows)['status'],'inconclusive')
        windows[2] = [100,100,200]
        self.assertGreater(m.repeatability(windows)['window_noise'][2], .15)

    def test_exact_noise_boundary_and_missing_window(self):
        self.assertAlmostEqual(m.noise([100,100,115]), .15)
        self.assertEqual(m.repeatability([[100,100,115]]*3)['status'],'pass')
        with self.assertRaises(ValueError): m.repeatability([[100]*3]*2)

    def test_pair_ci_pass_fail_and_inconclusive(self):
        self.assertEqual(m.paired_interval([(100,105)]*9)['status'],'pass')
        self.assertEqual(m.paired_interval([(100,120)]*9)['status'],'over_budget')
        self.assertEqual(m.paired_interval([(100,100),(100,115),(100,125)])['status'],'inconclusive')

    def test_pair_ci_uses_independent_pairs(self):
        result=m.paired_interval([(100,100),(100,100),(100,400)])
        self.assertAlmostEqual(result['geometric_ratio'],4**(1/3))
        self.assertEqual(len(result['ratios']),3)
        for pairs in [[(100,100)]*8,[(0,100)]*9,[(100,math.inf)]*9]:
            with self.assertRaises(ValueError): m.paired_interval(pairs)

    def test_quantile_sample_floor_and_debug(self):
        self.assertIsNone(m.statistics([1]*199)['p95_us'])
        self.assertIsNone(m.statistics([1]*999)['p99_us'])
        self.assertEqual(m.budget('request_to_ready',[1]*999)['status'],'insufficient_samples')
        self.assertEqual(m.budget('request_to_ready',[1]*1000)['status'],'pass')
        self.assertEqual(m.budget('reset_running',[9999999],config='Debug')['status'],'debug_observation_only')

    def test_unvalidated_gpu_and_stage_observer_not_acceptance(self):
        for metric in ('d2','d3','materialize','queue','gpu_render'):
            self.assertEqual(m.budget(metric,[1]*1000)['status'],'observer_unvalidated')

    def test_control_includes_entire_gap(self):
        row=dict(metric='reset_paused',ordinal=0,complete=True,start_us=100,confirmed_us=900,
                 first_present_us=1100,elapsed_us=1000)
        identity=dict(schema='pd5.probe.v1',metrics_compile_enabled=False,allocator='normal',
                      counts={'reset_paused':1},closed=True,replay_equal=True)
        text='PD5_ID '+json.dumps(identity)+'\nPD5_SAMPLE '+json.dumps(row)
        self.assertEqual(m.parse_probe(text)['statistics']['reset_paused']['max_us'],1000)
        bad=copy.deepcopy(row);bad['first_present_us']=800
        with self.assertRaises(ValueError):
            m.parse_probe('PD5_ID '+json.dumps(identity)+'\nPD5_SAMPLE '+json.dumps(bad))

    def test_control_max_and_unknown_budget(self):
        self.assertEqual(m.budget('pause',[1,50001])['status'],'over_budget')
        with self.assertRaises(ValueError):m.budget('fps',[240])


if __name__ == '__main__':
    unittest.main()

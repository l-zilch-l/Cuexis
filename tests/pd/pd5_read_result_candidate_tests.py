import copy
from pathlib import Path
import sys
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from tools.verification.playback_performance import read_result_candidate as candidate


class CandidateDecisionTests(unittest.TestCase):
    def test_noise_or_crossing_interval_cannot_pass_on_fast_point_estimate(self):
        good=dict(effect=dict(geometric_ratio=.90,ci95=[.85,.94]),noise={'A':.02,'B':.03})
        scale={'64-read':copy.deepcopy(good),'1-read':copy.deepcopy(good)}
        self.assertTrue(all(candidate.decision(scale,good).values()))
        scale['64-read']['effect']['ci95'][1]=.951
        self.assertFalse(candidate.decision(scale,good)['primary_read64'])
        scale['64-read']=copy.deepcopy(good)
        scale['1-read']['noise']['A']=.151
        self.assertFalse(candidate.decision(scale,good)['repeatability'])
        host=copy.deepcopy(good);host['effect']['ci95'][1]=1.101
        self.assertFalse(candidate.decision(scale,host)['all_metric_intervals'])


if __name__=='__main__':
    unittest.main()

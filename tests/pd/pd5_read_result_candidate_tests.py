import copy
from pathlib import Path
import sys
import unittest
import tempfile
import json
from unittest.mock import patch

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from tools.verification.playback_performance import read_result_candidate as candidate


class CandidateDecisionTests(unittest.TestCase):
    def test_candidate_cannot_be_registered_as_f1(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)
            original=root/'out/pd5-followup-20261021-62ca993'
            archive=original/'f1-source'
            (archive/'engine').mkdir(parents=True)
            (archive/'engine/read.cpp').write_bytes(b'F1')
            frozen=candidate.f.source_identity(archive)
            (original/'diagnostic').mkdir()
            (original/'diagnostic/plan.json').write_text(json.dumps({'builds':{'B':{'source_identity':frozen}}}))
            source=root/'candidate';source.mkdir()
            (source/'engine').mkdir()
            (source/'engine/read.cpp').write_bytes(b'candidate')
            with patch.object(candidate.f,'ROOT',root),patch.object(candidate,'F1_INVENTORY_SHA256',frozen['sha256']):
                candidate.verify_f1_source(archive)
                with self.assertRaisesRegex(ValueError,'not the fixed F1'):
                    candidate.verify_f1_source(source)
                (archive/'engine/read.cpp').write_bytes(b'changed archive')
                with self.assertRaisesRegex(ValueError,'Original F1 source bytes changed'):
                    candidate.verify_f1_source(source)

    def test_build_must_belong_to_the_declared_source(self):
        with tempfile.TemporaryDirectory() as folder:
            source=Path(folder)
            candidate.verify_build_source(source,'CMAKE_HOME_DIRECTORY:INTERNAL='+source.as_posix())
            with self.assertRaisesRegex(ValueError,'different source'):
                candidate.verify_build_source(source,'CMAKE_HOME_DIRECTORY:INTERNAL='+source.as_posix()+'/other')

    def test_compiled_identity_normalizes_only_line_endings_and_excludes_python(self):
        with tempfile.TemporaryDirectory() as folder:
            source=Path(folder); (source/'engine').mkdir(); (source/'tests').mkdir()
            code=source/'engine/read.cpp';code.write_bytes(b'F1\r\n')
            python=source/'tests/check.py';python.write_bytes(b'version1')
            initial=candidate.compiled_source_identity(source)
            code.write_bytes(b'F1\n');python.write_bytes(b'version2')
            self.assertEqual(initial,candidate.compiled_source_identity(source))
            code.write_bytes(b'C2\n')
            self.assertNotEqual(initial,candidate.compiled_source_identity(source))

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

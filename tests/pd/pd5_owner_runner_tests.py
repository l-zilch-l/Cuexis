import sys
import json
from pathlib import Path
import subprocess
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.verification.playback_performance import owner_runner as r


class RunnerTests(unittest.TestCase):
    def failure(self, error=None):
        with tempfile.TemporaryDirectory() as tmp:
            folder=Path(tmp)
            manifest={'builds':{'release':{'baseline':{'A':{'executable':'fake.exe'}}}}}
            unit={'id':'release-legacy','config':'release','kind':'owner','profile':'legacy'}
            with patch.object(r,'native'),patch.object(r.process_control,'run',
                    side_effect=error,return_value=SimpleNamespace(returncode=7)):
                with self.assertRaises((RuntimeError,subprocess.TimeoutExpired)):
                    r.launch(folder,manifest,unit,'A','pilot',4096,4,'failed-run')
            records=list(folder.glob('*.json'))
            self.assertEqual(len(records),1)
            data=json.loads(records[0].read_text(encoding='utf-8-sig'))
            self.assertEqual(data['sha256'],r.prepare.sha(data['path']))
            self.assertFalse(data['acceptance'])
            if error:self.assertIn('timed out',data['error'])
            else:self.assertEqual(data['exit_code'],7)

    def test_nonzero_exit_has_persisted_original_evidence(self):
        self.failure()

    def test_timeout_has_persisted_original_evidence(self):
        self.failure(subprocess.TimeoutExpired('fake.exe',180))


if __name__=='__main__':unittest.main()

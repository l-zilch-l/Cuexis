import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.verification.playback_performance import finite_followup as f


class FollowupTests(unittest.TestCase):
    def test_raw_counts_and_short_blocks_are_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder)/'raw.log'
            rows = []
            counts = [int(x) for x in f.COUNTS.split()]
            for i in range(0,len(counts),3):
                n = counts[i]
                rows.append('PD_FOLLOW_CONTENT '+json.dumps(dict(objects=n,sha256='c'*64)))
                for mode, count in (('read',counts[i+1]),('updated',counts[i+2])):
                    for block in range(3):
                        rows.append('PD_FOLLOW_ROW '+json.dumps(dict(objects=n,mode=mode,block=block,
                            operations=count,elapsed_us=200000,digest=42,ordinal=64)))
            rows.append('PD5_PROCESS_START {"pid":1}')
            path.write_text('\n'.join(rows),encoding='utf-8')
            record = dict(log=str(path),exit_code=0,log_sha256=f.sha(path),side='A',
                          name='scale-0-A',binary_sha256='a'*64)
            plan = dict(counts=f.COUNTS,builds={'A':{'source_identity':{'sha256':'b'*64}}})
            self.assertEqual(len(f.scale_records(record,plan)),8)
            original = path.read_text()
            for bad in (original.replace('2097152','1024'), original.replace('200000','99999'),
                        original.replace('"block": 2','"block": 1')):
                path.write_text(bad,encoding='utf-8')
                record['log_sha256'] = f.sha(path)
                with self.assertRaises(ValueError): f.scale_records(record,plan)
            record['log_sha256'] = '0'*64
            with self.assertRaises(ValueError): f.scale_records(record,plan)


if __name__ == '__main__':
    unittest.main()

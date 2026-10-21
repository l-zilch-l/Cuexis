import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.verification.playback_performance import finite_followup as f, measurement as m


class FollowupTests(unittest.TestCase):
    def test_host_runner_rejects_relabelled_native_processes(self):
        # Six distinct names must not turn two native logs into three independent pairs.
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            chart_path = 'assets/projects/stage3_project/assets/charts/stage3_example.cuexis.chart.json'
            chart = folder/chart_path
            chart.parent.mkdir(parents=True)
            chart.write_text('{"objects":[{},{}],"camera":{"id":"camera"}}',encoding='utf-8')
            plan = dict(host=dict(warmup=32,measured=1024,frame_capacity=2048),builds={})
            for side in 'AB':
                plan['builds'][side] = dict(source=str(folder),
                    source_identity=dict(sha256=side.lower()*64,files={chart_path:f.sha(chart)}),
                    bindings={kind:dict(sha256=side.lower()*64) for kind in ('host','baseline')})
                identity = dict(measurement_schema='pd0.receipt-ack.v1',digest=42,source='fixture',
                    viewport=[1280,720],objects=2,warmup=32,measured=1024,frame_capacity=2048,
                    frames_published=1056,successful_receipts=1056,audio=False,gpu=False,
                    closed=True,recording=False)
                rows = ['PD5_PROCESS_START '+json.dumps(dict(pid=1 if side=='A' else 2,qpc=100)),
                        'PD_HOST_ID '+json.dumps(identity)]
                rows += ['PD_HOST_SAMPLE '+json.dumps(dict(ordinal=i,**{
                    key:10. for key in m.HOST_FIELDS['pd0.receipt-ack.v1']})) for i in range(1024)]
                log = folder/(side+'.log')
                log.write_text('\n'.join(rows),encoding='utf-8')
                for i in range(3):
                    record = dict(log=str(log),exit_code=0,log_sha256=f.sha(log),side=side,
                                  name=f'host-{i}-{side}',binary_sha256=side.lower()*64)
                    (folder/f'host-{i}-{side}.process.json').write_text(json.dumps(record),encoding='utf-8')
                    (folder/f'scale-{i}-{side}.process.json').write_text('{}',encoding='utf-8')
            (folder/'plan.json').write_text(json.dumps(plan),encoding='utf-8')
            with patch.object(f,'scale_records',return_value={}):
                with self.assertRaisesRegex(ValueError,'independent processes'):
                    f.analyze(folder,plan)
            self.assertFalse((folder/'analysis.json').exists())

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
            rows.append('PD5_PROCESS_START {"pid":1,"qpc":10}')
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
